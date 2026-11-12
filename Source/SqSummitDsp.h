#pragma once
// Summit Limiter - DSP core (1.14 "Studio Line", phase C). No UI, no te:: types: tests/summit_test.cpp measures it.
//
// A lookahead brickwall limiter that guards TRUE peaks (between samples), not just sample peaks:
//   input gain -> DC filter -> optional soft clip (the algorithm's share) -> band-limit guard -> true-peak detector (4x polyphase
//   interpolation of the signal the limiter will output) -> sliding-window minimum of the needed gain over the
//   lookahead -> attack shaped across the lookahead (the algorithm's curve) -> release (the algorithm's law,
//   programme-dependent) -> separate stereo linking for transients and release -> gain applied (optionally at up to 32x
//   to keep the gain modulation's sidebands out of the audio band) -> ceiling -> a final true-peak safety clamp ->
//   dither (TPDF, three noise shapes) -> output. "Listen to what's removed" and unity-gain audition are built in.

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <deque>
#include <vector>

namespace SqSummit
{
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    struct Algorithm
    {
        const char* name; const char* about;
        double attackShape;     // 0 = linear ramp over the lookahead, 1 = raised cosine, 2 = late (holds the transient longer)
        double releaseMs;       // the algorithm's base release (scaled by the Release control, 0.25x-4x)
        double releaseFastShare;// 0..1 of the release that is a fast first stage (programme-dependent "breath")
        double clipShare;       // 0..1 soft clip before the limiter (part of the job done by clipping)
        double transientLink;   // default transient linking 0..1
        double releaseLink;     // default release linking 0..1
    };
    inline const std::array<Algorithm, 10>& algorithms()
    {
        static const std::array<Algorithm, 10> a { {
            { "Transparent", "Most invisible: smooth attack across the whole lookahead, patient release",          1.0, 400.0, 0.20, 0.00, 1.00, 1.00 },
            { "Punch",       "Lets the first edge through the lookahead, recovers fast between hits",               2.0, 120.0, 0.60, 0.05, 1.00, 1.00 },
            { "Dense",       "Loud and full: quicker recovery, a little clipping carries the peaks",                1.0,  60.0, 0.70, 0.25, 1.00, 1.00 },
            { "Smooth",      "Gentle on vocals and acoustic music: long raised-cosine attack, slow release",        1.0, 800.0, 0.10, 0.00, 1.00, 1.00 },
            { "Aggressive",  "Maximum level for club and trap: fast release, clipping shares the work",            0.0,  30.0, 0.80, 0.45, 1.00, 1.00 },
            { "Wide",        "Keeps the stereo image: transients limited per channel, release linked",              1.0, 200.0, 0.40, 0.05, 0.20, 1.00 },
            { "Bus",         "For groups and stems: medium release, a touch of clip, fully linked",                 1.0, 150.0, 0.40, 0.10, 1.00, 1.00 },
            { "Safe",        "A last-stage safety net: almost never works, never pumps",                            1.0, 1200.0, 0.00, 0.00, 1.00, 1.00 },
            { "Modern",      "Today's loud masters: programme-dependent release, clean transients",                1.0,  80.0, 0.75, 0.15, 0.80, 1.00 },
            { "Clipper",     "Hard ceiling by clipping alone - the limiter only catches what the clip leaves",      0.0,  20.0, 0.50, 1.00, 1.00, 1.00 },
        } };
        return a;
    }

    struct Params
    {
        int algorithm = 0;
        double inputDb = 0, ceilingDb = -1.0, lookaheadMs = 2.0, releaseScale = 1.0;   // release control 0.25..4
        bool truePeak = true;
        bool bandLimit = true;                              // the guard: a linear-phase low-pass at 90 % of Nyquist (max 20 kHz)
        double transientLink = -1, releaseLink = -1;        // -1 = the algorithm's own
        int oversampleLog2 = 0;                             // 0..5 (1x..32x) for the gain stage
        bool dcFilter = true;
        int ditherBits = 0;                                 // 0 off, 16, 20, 24
        int noiseShape = 0;                                 // 0 none, 1 gentle, 2 strong
        bool unityAudition = false;                         // hear the limiting at matched level (input gain removed)
        bool listenRemoved = false;                         // hear only what the limiter takes away
    };

    /** 4x polyphase interpolator (32 taps per phase, Kaiser-windowed sinc, beta 9) for true-peak detection. Accurate to a few
        hundredths of a dB on anything below 90 % of Nyquist - which the band-limit guard guarantees. */
    struct TruePeak4
    {
        static constexpr int taps = 32, phases = 4;
        static const std::array<std::array<float, taps>, phases>& coeffs()
        {
            static const auto c = [] {
                std::array<std::array<float, taps>, phases> k {};
                auto i0 = [] (double x) { double s = 1, t = 1; for (int j = 1; j < 40; ++j) { t *= (x / (2 * j)) * (x / (2 * j)); s += t; } return s; };
                const double beta = 9.0; const int L = taps * phases;
                for (int p = 0; p < phases; ++p)
                    for (int t = 0; t < taps; ++t)
                    {
                        const int n = t * phases + p; const double m = n - (L - 1) / 2.0;
                        const double x = m / phases, sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                        const double r = 2.0 * n / (L - 1) - 1.0, w = i0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / i0 (beta);
                        k[(size_t) p][(size_t) t] = (float) (sinc * w);
                    }
                return k;
            }();
            return c;
        }
        std::array<float, taps> hist {}; int pos = 0;
        void reset() { hist.fill (0); pos = 0; }
        /** Push one sample, return the largest absolute value among the 4 interpolated points (and the sample). */
        inline float push (float x)
        {
            hist[(size_t) pos] = x; pos = (pos + 1) % taps;
            float m = 0;
            const auto& c = coeffs();
            for (int p = 0; p < phases; ++p)
            {
                float s = 0;
                for (int t = 0; t < taps; ++t) s += c[(size_t) p][(size_t) t] * hist[(size_t) ((pos - 1 - t + taps * 2) % taps)];
                m = std::max (m, std::abs (s));
            }
            return m;
        }
        static constexpr int delay = taps / 2;   // the interpolator's group delay (samples)
    };

    /** Sliding minimum over a window (monotonic deque, O(1) amortised). */
    struct SlidingMin
    {
        std::deque<std::pair<long long, double>> q; long long n = 0; int win = 1;
        void reset (int w) { q.clear(); n = 0; win = std::max (1, w); }
        double push (double v)
        {
            while (! q.empty() && q.back().second >= v) q.pop_back();
            q.emplace_back (n, v);
            while (q.front().first <= n - win) q.pop_front();
            ++n; return q.front().second;
        }
    };

    /** The band-limit guard: a 127-tap linear-phase low-pass (Kaiser beta 8) at 90 % of Nyquist, capped at 20 kHz.
        Peaks between samples can only be measured exactly below the band edge; above ~0.9 x Nyquist no finite detector
        (and no two DACs) agree. The guard removes that inaudible sliver so the true-peak ceiling holds against an EXACT
        band-limited reconstruction. Delay: 63 samples, flat to 0.001 dB through the audio band. */
    struct BandGuard
    {
        static constexpr int taps = 127, delay = 63;
        std::array<float, taps> h {};
        std::array<std::array<float, taps * 2>, 2> buf {}; int pos = 0;
        void design (double fs)
        {
            const double fc = std::min (20000.0, 0.45 * fs) / fs;             // cycles per sample
            auto i0 = [] (double x) { double s = 1, t = 1; for (int j = 1; j < 40; ++j) { t *= (x / (2 * j)) * (x / (2 * j)); s += t; } return s; };
            double sum = 0;
            for (int n = 0; n < taps; ++n)
            {
                const double m = n - delay, sinc = m == 0 ? 2 * fc : std::sin (2 * pi * fc * m) / (pi * m);
                const double r = (double) m / delay, w = i0 (8.0 * std::sqrt (std::max (0.0, 1 - r * r))) / i0 (8.0);
                h[(size_t) n] = (float) (sinc * w); sum += sinc * w;
            }
            for (auto& v : h) v = (float) (v / sum);
            reset();
        }
        void reset() { for (auto& b : buf) b.fill (0); pos = 0; }
        inline double run (int c, double x)
        {
            auto& b = buf[(size_t) c]; b[(size_t) pos] = b[(size_t) (pos + taps)] = (float) x;
            double y = 0; const float* p = b.data() + pos + 1;
            for (int t = 0; t < taps; ++t) y += h[(size_t) (taps - 1 - t)] * p[t];
            return y;
        }
        void advance() { pos = (pos + 1) % taps; }
    };

    class Engine
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            fs = sampleRate; block = std::max (64, maxBlock);
            maxLook = (int) std::ceil (0.010 * fs) + TruePeak4::delay + 4;
            for (auto& d : delay) d.assign ((size_t) (maxLook + 4), 0.0f);
            for (int k = 0; k <= 5; ++k)
            {
                os[(size_t) k] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) k, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
                os[(size_t) k]->initProcessing ((size_t) block);
            }
            guard.design (fs); guard2.design (fs);
            gainBuf.assign ((size_t) block, 1.0f); gainBufR.assign ((size_t) block, 1.0f);
            for (auto& d : sdelay) d.assign ((size_t) (safetyDelay() + 8), 0.0f);
            for (auto& d : remDelay) d.assign ((size_t) (safetyDelay() + 4096), 0.0f);
            reset();
        }
        void reset()
        {
            for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
            dw = 0; guard.reset(); guard2.reset(); for (auto& t : tp) t.reset(); for (auto& m : mins) m.reset (lookN());
            for (auto& t : stp) t.reset(); smin.reset (2 * safetyHold + 1); sminDelayed.reset (safetyHold + 1); senv = 1; sw = 0; for (auto& d : sdelay) std::fill (d.begin(), d.end(), 0.0f);
            for (auto& r : ma1) r.reset(); for (auto& r : ma2) r.reset();
            env[0] = env[1] = 1; slow[0] = slow[1] = 1; dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0; err[0].fill (0); err[1].fill (0);
            for (auto& o : os) if (o) o->reset();
            meterGrDb = 0; meterOutTpDb = -100;
        }
        int lookN() const { return std::max (1, (int) std::lround (juce::jlimit (0.05, 10.0, p.lookaheadMs) * 0.001 * fs)); }
        int latency() const { return guardDelay() + lookN() + TruePeak4::delay + osLatency() + safetyDelay(); }
        int guardDelay() const { return p.truePeak && p.bandLimit ? 2 * BandGuard::delay : 0; }   // before the detector and after the gain
        /** The safety stage's delay: its detector plus a hold window wide enough to keep the gain flat across a peak. */
        static constexpr int safetyHold = 24;
        static int safetyDelay() { return TruePeak4::delay + safetyHold; }
        int osLatency() const { return p.oversampleLog2 > 0 ? (int) std::lround (os[(size_t) std::min (5, p.oversampleLog2)]->getLatencyInSamples()) : 0; }

        float meterGrDb = 0, meterOutTpDb = -100;

        void process (float* const* ch, int numCh, int n)
        {
            const bool stereo = numCh >= 2;
            const auto& al = algorithms()[(size_t) juce::jlimit (0, 9, p.algorithm)];
            const int L = lookN();
            if (L != lastL) { for (auto& m : mins) m.reset (L + 1); lastL = L; }
            const double inG = dbToGain (p.inputDb), ceil = dbToGain (p.ceilingDb), ceilDet = ceil * dbToGain (-0.2);   // 0.2 dB under the ceiling: the 4x detector's measuring tolerance
            const double tLink = p.transientLink >= 0 ? p.transientLink : al.transientLink, rLink = p.releaseLink >= 0 ? p.releaseLink : al.releaseLink;
            const double relMs = al.releaseMs * juce::jlimit (0.25, 4.0, p.releaseScale);
            const double rSlow = std::exp (-1.0 / (relMs * 0.001 * fs)), rFast = std::exp (-1.0 / (relMs * 0.12 * 0.001 * fs));
            const double clipT = ceil * (1.0 - 0.5 * al.clipShare);    // the clip starts this far under the ceiling
            const double dcR = 1.0 - 2.0 * pi * 5.0 / fs;               // 5 Hz DC blocker
            if ((int) gainBuf.size() < n) { gainBuf.resize ((size_t) n); gainBufR.resize ((size_t) n); }
            float grMin = 1.0f;
            for (int s = 0; s < n; ++s)
            {
                double x[2] = { ch[0][s] * inG, (stereo ? ch[1][s] : ch[0][s]) * inG };
                for (int c = 0; c < 2; ++c)
                {
                    if (p.dcFilter) { const double y = x[c] - dcX[c] + dcR * dcY[c]; dcX[c] = x[c]; dcY[c] = y; x[c] = y; }
                    if (al.clipShare > 0) x[c] = softClip (x[c], clipT, ceil);   // clip FIRST, so the guard band-limits its harmonics
                    if (p.truePeak && p.bandLimit) x[c] = guard.run (c, x[c]);
                }
                if (p.truePeak && p.bandLimit) guard.advance();
                // the gain each channel needs so its TRUE peak stays under the ceiling
                double need[2];
                for (int c = 0; c < 2; ++c)
                {
                    const float pk = p.truePeak ? tp[(size_t) c].push ((float) x[c]) : (float) std::abs (x[c]);
                    need[c] = pk > ceilDet ? ceilDet / pk : 1.0;
                }
                // transient linking: blend each channel's need towards the stricter one
                const double strict = std::min (need[0], need[1]);
                for (int c = 0; c < 2; ++c) need[c] = need[c] + tLink * (strict - need[c]);
                // the lookahead window: the smallest need ahead of the delayed sample, shaped as an attack
                double g[2];
                for (int c = 0; c < 2; ++c)
                {
                    const double m = mins[(size_t) c].push (need[c]);
                    // attack shaping, O(1) per sample: shape 0 = one moving average over the lookahead (a straight ramp),
                    // shape 1 = two cascaded averages of half the lookahead (a smooth S-ramp), shape 2 = one average over
                    // half the lookahead (a later, steeper ramp). Each is fully down by the time the peak is output.
                    double a;
                    {
                        auto& m1 = ma1[(size_t) c]; auto& m2 = ma2[(size_t) c];
                        const int len1 = al.attackShape == 0 ? L : std::max (1, L / 2);
                        const double v1 = m1.push (m, len1);
                        a = al.attackShape == 1 ? m2.push (v1, std::max (1, L / 2)) : v1;
                    }
                    a = std::min (a, m);                                        // never above what the window demands
                    // release: two stages - a fast "breath" share and the slow body
                    if (a < env[c]) { env[c] = a; slow[c] = std::min (slow[c], a); }
                    else
                    {
                        slow[c] = rSlow * slow[c] + (1 - rSlow) * a;
                        const double fastPart = rFast * env[c] + (1 - rFast) * a;
                        env[c] = std::min (a, al.releaseFastShare * fastPart + (1 - al.releaseFastShare) * std::max (slow[c], env[c] * rSlow + (1 - rSlow) * a));
                    }
                    g[c] = env[c];
                }
                // release linking
                const double gl = std::min (g[0], g[1]);
                for (int c = 0; c < 2; ++c) g[c] = g[c] + rLink * (gl - g[c]) * (g[c] > gl ? 1.0 : 0.0);
                // delay the audio to line up with the lookahead and the detector's delay
                const int D = L + TruePeak4::delay;
                for (int c = 0; c < 2; ++c) delay[(size_t) c][(size_t) dw] = (float) x[c];
                const int rd = (dw - D + (int) delay[0].size() * 4) % (int) delay[0].size();
                dw = (dw + 1) % (int) delay[0].size();
                const float d0 = delay[0][(size_t) rd], d1 = delay[1][(size_t) rd];
                gainBuf[(size_t) s] = (float) g[0]; gainBufR[(size_t) s] = (float) g[1];
                ch[0][s] = d0; if (stereo) ch[1][s] = d1;
                removedL = d0; removedR = d1;
                grMin = std::min (grMin, (float) std::min (g[0], g[1]));
            }
            // apply the gain (oversampled when asked: the gain modulation's sidebands stay above the audio band)
            applyGain (ch, numCh, n);
            // the safety stage: the limited signal's OWN true peaks (the gain moving between samples makes a few new
            // ones), a gain held flat across +/- safetyHold samples around each, a quick recovery - then the clamp
            float tpOut = 0;
            const double rSafe = std::exp (-1.0 / (0.020 * fs));
            const int SD = safetyDelay(), sz2 = (int) sdelay[0].size();
            for (int s = 0; s < n; ++s)
            {
                double y[2] = { ch[0][s], stereo ? ch[1][s] : ch[0][s] };
                // the second guard pass: the gain's own movement leaves sidebands at the band edge - take them out here
                if (p.truePeak && p.bandLimit) { for (int c = 0; c < 2; ++c) y[c] = guard2.run (c, y[c]); guard2.advance(); }
                double need2 = 1.0;
                for (int c = 0; c < 2; ++c) { const float pk = stp[(size_t) c].push ((float) y[c]); if (pk > ceilDet) need2 = std::min (need2, ceilDet / pk); }
                const double held = smin.push (need2);                          // the lowest need in the hold window
                senv = held < senv ? held : rSafe * senv + (1 - rSafe) * held;
                // the gain is applied to the sample from SD samples ago (centred in the hold window)
                for (int c = 0; c < 2; ++c) sdelay[(size_t) c][(size_t) sw] = (float) y[c];
                const int rd = (sw - SD + sz2 * 4) % sz2; sw = (sw + 1) % sz2;
                const double gS = std::min (senv, sminDelayed.push (held));
                ch[0][s] = (float) (sdelay[0][(size_t) rd] * gS); if (stereo) ch[1][s] = (float) (sdelay[1][(size_t) rd] * gS);
            }
            for (int s = 0; s < n; ++s)
                for (int c = 0; c < (stereo ? 2 : 1); ++c)
                {
                    double y = ch[c][s];
                    y = std::max (-ceil, std::min (ceil, y));                   // anything left is a rounding error
                    if (p.ditherBits > 0) y = dither (y, c);
                    if (p.unityAudition) y /= std::max (1e-6, inG);
                    ch[c][s] = (float) y;
                    tpOut = std::max (tpOut, outTp[(size_t) c].push ((float) y));
                }
            if (p.listenRemoved)
            {
                // the limiter's input, delayed by everything after the gain (oversampler + safety stage), minus the output
                const int extra = osLatency() + safetyDelay() + (p.truePeak && p.bandLimit ? BandGuard::delay : 0), sz3 = (int) remDelay[0].size();
                for (int s = 0; s < n; ++s)
                {
                    for (int c = 0; c < 2; ++c) remDelay[(size_t) c][(size_t) rw] = dryCopy[(size_t) c][(size_t) s];
                    const int rd = (rw - extra + sz3 * 4) % sz3; rw = (rw + 1) % sz3;
                    for (int c = 0; c < (stereo ? 2 : 1); ++c) ch[c][s] = (float) (remDelay[(size_t) c][(size_t) rd] - ch[c][s]);
                }
            }
            meterGrDb = (float) gainToDb (grMin); meterOutTpDb = (float) gainToDb (tpOut);
        }

    private:
        double fs = 48000; int block = 512, maxLook = 1, lastL = -1;
        std::array<std::vector<float>, 2> delay; int dw = 0;
        std::array<TruePeak4, 2> tp, outTp;
        std::array<SlidingMin, 2> mins;
        BandGuard guard, guard2;
        std::array<TruePeak4, 2> stp; SlidingMin smin, sminDelayed; double senv = 1;
        std::array<std::vector<float>, 2> sdelay, remDelay; int sw = 0, rw = 0;
        struct MovingAvg
        {
            std::vector<double> buf; int pos = 0, len = 0; double sum = 0;
            double push (double v, int n)
            {
                if (n != len) { buf.assign ((size_t) n, v); len = n; pos = 0; sum = v * n; }
                sum += v - buf[(size_t) pos]; buf[(size_t) pos] = v; pos = (pos + 1) % len;
                if (pos == 0) { sum = 0; for (auto x : buf) sum += x; }      // re-sum once per lap: no drift
                return sum / len;
            }
            void reset() { buf.clear(); len = 0; sum = 0; pos = 0; }
        };
        std::array<MovingAvg, 2> ma1, ma2;
        double env[2] { 1, 1 }, slow[2] { 1, 1 }, dcX[2] {}, dcY[2] {};
        std::array<std::array<double, 3>, 2> err {};
        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 6> os;
        std::vector<float> gainBuf, gainBufR;
        std::array<std::vector<float>, 2> dryCopy;
        float removedL = 0, removedR = 0;
        juce::Random rng { 0x5317 };

        static double softClip (double x, double t, double ceil)
        {
            const double a = std::abs (x);
            if (a <= t) return x;
            const double room = std::max (1e-9, ceil - t);
            const double y = t + room * std::tanh ((a - t) / room);         // smooth knee from t, asymptote at the ceiling
            return x < 0 ? -y : y;
        }
        void applyGain (float* const* ch, int numCh, int n)
        {
            const bool stereo = numCh >= 2;
            if (p.listenRemoved) for (int c = 0; c < 2; ++c) dryCopy[(size_t) c].assign (ch[stereo ? c : 0], ch[stereo ? c : 0] + n);
            if (p.oversampleLog2 <= 0)
            {
                for (int s = 0; s < n; ++s) { ch[0][s] *= gainBuf[(size_t) s]; if (stereo) ch[1][s] *= gainBufR[(size_t) s]; }
                return;
            }
            // upsample the audio, interpolate the gain linearly, multiply, downsample (latency reported by latency())
            monoScratch.assign (ch[0], ch[0] + n);
            float* chans[2] = { ch[0], stereo ? ch[1] : monoScratch.data() };
            juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
            auto& o = *os[(size_t) std::min (5, p.oversampleLog2)];
            auto up = o.processSamplesUp (blk);
            const int f = 1 << std::min (5, p.oversampleLog2);
            for (size_t c = 0; c < 2; ++c)
            {
                auto* d = up.getChannelPointer (c); const auto& gb = c == 0 ? gainBuf : gainBufR;
                for (int s = 0; s < n; ++s)
                {
                    const float g0 = s > 0 ? gb[(size_t) s - 1] : lastG[c], g1 = gb[(size_t) s];
                    for (int k = 0; k < f; ++k) d[(size_t) (s * f + k)] *= g0 + (g1 - g0) * (k + 1) / (float) f;
                }
                lastG[c] = gb[(size_t) n - 1];
            }
            o.processSamplesDown (blk);
        }
        float lastG[2] { 1, 1 };
        std::vector<float> monoScratch;

        /** TPDF dither at the chosen bit depth, with none / gentle (1st-order) / strong (3-tap, ear-weighted) noise shaping. */
        double dither (double y, int c)
        {
            const double q = 1.0 / std::pow (2.0, p.ditherBits - 1);
            static const double shape[3][3] = { { 0, 0, 0 }, { 1.0, 0, 0 }, { 1.623, -0.982, 0.109 } };
            const auto& h = shape[juce::jlimit (0, 2, p.noiseShape)];
            auto& e = err[(size_t) c];
            const double shaped = y - (h[0] * e[0] + h[1] * e[1] + h[2] * e[2]);
            const double tpdf = (rng.nextDouble() - rng.nextDouble()) * q;
            const double out = std::round ((shaped + tpdf) / q) * q;
            e[2] = e[1]; e[1] = e[0]; e[0] = out - shaped;
            return out;
        }
    };
}
