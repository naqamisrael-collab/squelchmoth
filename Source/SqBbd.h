#pragma once
// SqBbd: a bucket-brigade delay line with the analog filters before and after it. Pure C++ (no JUCE).
//
// A bucket-brigade chip is a row of N tiny capacitors. On each clock edge every bucket passes its charge to the
// next, so the chip takes a sample of its input once per clock period and hands it out N / 2 clock periods later:
// delay = N / (2 x clock). It is a sampler running at its own, variable rate - slow the clock for a longer delay and
// the sound gets darker and starts to fold over; wobble the clock and the pitch wobbles.
//
// The method follows the published one for such circuits: the chip is a fixed-length queue clocked at its own rate,
// and the two analog filters do the conversion between the audio rate and the clock rate, each written as a sum of
// one-pole sections whose state can be read (or pushed) at any instant between two audio samples. Here the input
// between two audio samples is taken as a straight line (the published form takes it as an impulse), and the buckets
// bend the signal by the amount the chips' data sheets give.
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace SqBbd
{
    using cd = std::complex<double>;

    /** The buckets' distortion: y = x - a x^3 - b x^5 in volts, with a and b set so a tone measures the data sheets' two
        figures (0.8 % at 0.25 V rms, 2.5 % at 0.36 V rms). Past +/- 0.62 V it holds. */
    struct Buckets { double a = 0.1461, b = 0.6650, limit = 0.62; };
    inline double bend (const Buckets& k, double x) { x = std::max (-k.limit, std::min (k.limit, x)); const double x2 = x * x; return x * (1.0 - k.a * x2 - k.b * x2 * x2); }

    /** A Butterworth low-pass of odd order as a sum of one-pole sections: the real pole and the upper pole of each pair. */
    struct Filter
    {
        void design (int order, double cutoffHz, double sampleRate)
        {
            const double pi = 3.14159265358979323846, wc = 2.0 * pi * cutoffHz; ts = 1.0 / sampleRate; cd all[5]; order = std::min (order, 5);      // no allocation: this runs on the audio thread when Tone moves
            for (int k = 0; k < order; ++k) all[k] = wc * std::polar (1.0, pi * (2.0 * k + order + 1.0) / (2.0 * order));
            n = 0; double gain = 1.0; for (int k = 0; k < order; ++k) gain *= wc;
            for (int k = 0; k < order; ++k)
            {
                if (all[k].imag() < -1.0e-9 * wc) continue;                        // the lower pole of a pair rides with its partner
                cd r = gain; for (int j = 0; j < order; ++j) if (j != k) r /= (all[k] - all[j]);
                const bool pair = all[k].imag() > 1.0e-9 * wc; p[n] = all[k]; res[n] = r; weight[n] = pair ? 2.0 : 1.0;
                step[n] = std::exp (p[n] * ts); held[n] = (step[n] - 1.0) / p[n]; ramp[n] = (step[n] - 1.0) / (p[n] * p[n]) - ts / p[n]; ++n;
            }
        }
        int n = 0; double ts = 1.0 / 48000.0; cd p[3], res[3], step[3], held[3], ramp[3]; double weight[3] = { 1, 1, 1 };
    };

    /** One line: input filter (3rd order), the buckets, output filter (5th order). Call process() once per audio sample
        with the clock's rate at that moment. */
    struct Line
    {
        void prepare (double sampleRate, int stages, double cutoffHz) { fs = sampleRate; setStages (stages); setCutoff (cutoffHz); reset(); }
        void setStages (int stages) { stagesNow = std::max (4, stages & ~1); if ((int) ring.size() < maxStages / 2 + 4) ring.assign ((size_t) (maxStages / 2 + 4), 0.0); }
        void setCutoff (double hz) { if (hz != cutoffNow) { cutoffNow = hz; in.design (3, hz, fs); out.design (5, hz, fs); } }
        void reset() { std::fill (ring.begin(), ring.end(), 0.0); for (auto& v : xin) v = 0.0; for (auto& v : xout) v = 0.0; u0 = 0.0; held = 0.0; next = 1.0; even = true; w = 0; }      // the first clock edge falls on the first audio sample
        double delaySeconds (double clockHz) const { return stagesNow / (2.0 * clockHz); }

        double process (double u1, double clockHz)
        {
            const double half = fs / (2.0 * clockHz), slope = (u1 - u0) * fs;
            while (next <= 1.0)
            {
                const double d = next, t = d * in.ts;
                if (even)
                {
                    // what the input filter holds at this instant: its state carried forward, plus the straight piece of input since the last audio sample
                    double v = 0.0;
                    for (int m = 0; m < in.n; ++m) { const cd e = std::exp (in.p[m] * t), x = e * xin[m] + u0 * (e - 1.0) / in.p[m] + slope * ((e - 1.0) / (in.p[m] * in.p[m]) - t / in.p[m]); v += in.weight[m] * (in.res[m] * x).real(); }
                    ring[(size_t) w] = bend (buckets, v); if (++w >= (int) ring.size()) w = 0;
                }
                else
                {
                    // the chip hands out the sample it took (stages - 1) clock edges ago; the output filter is pushed by the size of the step
                    int r = w - 1 - (stagesNow / 2 - 1); const int size = (int) ring.size(); while (r < 0) r += size;
                    const double fresh = ring[(size_t) r], jump = fresh - held; held = fresh;
                    if (jump != 0.0) for (int m = 0; m < out.n; ++m) xout[m] += out.res[m] / out.p[m] * std::exp (out.p[m] * ((1.0 - d) * out.ts)) * jump;
                }
                even = ! even; next += half;
            }
            next -= 1.0;
            for (int m = 0; m < in.n; ++m) xin[m] = in.step[m] * xin[m] + u0 * in.held[m] + slope * in.ramp[m];
            u0 = u1;
            double y = held; for (int m = 0; m < out.n; ++m) { y += out.weight[m] * xout[m].real(); xout[m] *= out.step[m]; }
            return y;
        }

        static constexpr int maxStages = 8192;
        Buckets buckets; Filter in, out; std::vector<double> ring; cd xin[3], xout[3];
        double fs = 48000.0, cutoffNow = -1.0, u0 = 0.0, held = 0.0, next = 1.0; int stagesNow = 4096, w = 0; bool even = true;
    };
}
