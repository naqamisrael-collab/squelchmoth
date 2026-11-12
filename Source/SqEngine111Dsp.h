#pragma once
// Squelchmoth 1.11 shared engines (super prompt §5). Standard C++ only, no JUCE, so every engine can be measured in
// isolation (tests/engine111_test.cpp) and then shared by the Color & Space and Voice families.
//
//   Eng::HalfBand / Eng::Oversampler   polyphase IIR half-band filters, 1x to 16x, designed at run time to a stop-band
//                                      target, with the measured group delay reported for latency compensation
//   Eng::Nonlinear                     the ten Cinder saturation models as stateful stages (run at the oversampled rate)
//   Eng::AnalogStyle                   seven drive voicings built from those stages: Clean, Fat, Squash, Dirt, Crunch,
//                                      Shred, Pump
//   Eng::WingMod                       the modulation engine: LFO, Rhythm, Envelope, Random S/H, Step and ADSR, each with
//                                      sidechain input, tempo sync locked to the transport and five smoothing modes
//   Eng::TestSignals                   pink noise, a drum loop and a sung vocal, generated, for the preset audit
//
// Everything is real-time safe after prepare(): no allocation, no locks, no exceptions on the audio thread.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <atomic>
#include <mutex>

namespace Eng
{
    constexpr double pi = 3.14159265358979323846;
    inline float dbToGain (float db)   { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)    { return g > 1.0e-9f ? 20.0f * std::log10 (g) : -180.0f; }
    template <typename T> inline T clamp (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
    /** Flush values that would become denormals in feedback paths. */
    inline float undenorm (float v)    { return std::abs (v) < 1.0e-20f ? 0.0f : v; }

    //==========================================================================
    // Basic filters
    //==========================================================================
    /** Leaky DC blocker (one zero at DC, one pole just inside). ~5 Hz corner at the given rate. */
    struct DcBlock
    {
        float r = 0.9995f, x1 = 0, y1 = 0;
        void prepare (double sr, double cornerHz = 5.0)   { r = (float) std::exp (-2.0 * pi * cornerHz / sr); reset(); }
        void reset()                                      { x1 = y1 = 0; }
        float process (float x)                           { const float y = x - x1 + r * y1; x1 = x; y1 = undenorm (y); return y; }
    };

    /** One-pole low-pass / high-pass (TPT). */
    struct OnePole
    {
        float g = 0, s = 0;
        void set (double sr, double hz)   { const double w = std::tan (pi * clamp (hz, 1.0, sr * 0.49) / sr); g = (float) (w / (1.0 + w)); }
        void reset()                      { s = 0; }
        float lp (float x)                { const float v = (x - s) * g; const float y = v + s; s = undenorm (y + v); return y; }
        float hp (float x)                { return x - lp (x); }
    };

    /** RBJ biquad in transposed direct form II (coefficients in double, state in double: clean at low frequencies). */
    struct Biquad
    {
        enum Type { lowpass, highpass, bandpass, peak, lowShelf, highShelf, notch, allpass };
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (Type t, double sr, double hz, double q, double gainDb = 0)
        {
            hz = clamp (hz, 5.0, sr * 0.49); q = std::max (0.05, q);
            const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * pi * hz / sr, cw = std::cos (w), sw = std::sin (w), al = sw / (2.0 * q);
            double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
            switch (t)
            {
                case lowpass:   B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
                case highpass:  B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
                case bandpass:  B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
                case notch:     B0 = 1; B1 = -2 * cw; B2 = 1; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
                case allpass:   B0 = 1 - al; B1 = -2 * cw; B2 = 1 + al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
                case peak:      B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
                case lowShelf:
                {
                    const double sa = 2 * std::sqrt (A) * al;
                    B0 = A * ((A + 1) - (A - 1) * cw + sa); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sa);
                    A0 = (A + 1) + (A - 1) * cw + sa; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sa; break;
                }
                case highShelf:
                {
                    const double sa = 2 * std::sqrt (A) * al;
                    B0 = A * ((A + 1) + (A - 1) * cw + sa); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sa);
                    A0 = (A + 1) - (A - 1) * cw + sa; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sa; break;
                }
            }
            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
        }
        void setFlat()   { b0 = 1; b1 = b2 = a1 = a2 = 0; }
        void reset()     { z1 = z2 = 0; }
        float process (float xf)
        {
            const double x = xf, y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
            if (! (std::abs (y) < 1.0e6)) { z1 = z2 = 0; return 0.0f; }   // a runaway or a NaN never leaves the filter
            if (std::abs (z1) < 1.0e-25) z1 = 0;
            if (std::abs (z2) < 1.0e-25) z2 = 0;
            return (float) y;
        }
        /** Magnitude in dB at a frequency (for the faces and the tests). */
        double magnitudeDb (double hz, double sr) const
        {
            const double w = 2.0 * pi * hz / sr;
            const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
            const double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i, dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
            return 10.0 * std::log10 ((nr * nr + ni * ni) / std::max (1.0e-30, dr * dr + di * di));
        }
    };

    //==========================================================================
    // Oversampling
    //==========================================================================
    /** Designs polyphase IIR half-band coefficients (two parallel chains of first-order allpasses in z^-2), after the
        elliptic-function method published by de Soras: given a transition bandwidth (fraction of the rate, 0..0.5) and
        a stop-band attenuation it returns the fewest coefficients that reach both. */
    struct HalfBandDesign
    {
        static void transitionParams (double transition, double& k, double& q)
        {
            k = std::tan ((1.0 - transition * 2.0) * pi / 4.0); k *= k;
            const double kksqrt = std::pow (1.0 - k * k, 0.25);
            const double e = 0.5 * (1.0 - kksqrt) / (1.0 + kksqrt), e2 = e * e, e4 = e2 * e2;
            q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
        }
        static int order (double attenuationDb, double q)
        {
            const double attnP2 = std::pow (10.0, -attenuationDb / 10.0), a = attnP2 / (1.0 - attnP2);
            int o = (int) std::ceil (std::log (a * a / 16.0) / std::log (q));
            if ((o & 1) == 0) ++o;
            if (o == 1) o = 3;
            return o;
        }
        static double accNum (double q, int order, int c)
        {
            double acc = 0, term = 0; int i = 0, j = 1;
            do { term = std::pow (q, (double) i * (i + 1)) * std::sin ((i * 2 + 1) * c * pi / order) * j; acc += term; j = -j; ++i; }
            while (std::abs (term) > 1.0e-100 && i < 200);
            return acc;
        }
        static double accDen (double q, int order, int c)
        {
            double acc = 0, term = 0; int i = 1, j = -1;
            do { term = std::pow (q, (double) i * i) * std::cos (i * 2 * c * pi / order) * j; acc += term; j = -j; ++i; }
            while (std::abs (term) > 1.0e-100 && i < 200);
            return acc;
        }
        static double coef (int index, double k, double q, int order)
        {
            const int c = index + 1;
            const double num = accNum (q, order, c) * std::pow (q, 0.25), den = accDen (q, order, c) + 0.5;
            const double ww = num / den, wwsq = ww * ww;
            const double x = std::sqrt ((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
            return (1.0 - x) / (1.0 + x);
        }
        static std::vector<double> design (double attenuationDb, double transition)
        {
            double k, q; transitionParams (transition, k, q);
            const int o = order (attenuationDb, q), n = (o - 1) / 2;
            std::vector<double> c ((size_t) n);
            for (int i = 0; i < n; ++i) c[(size_t) i] = coef (i, k, q, o);
            return c;
        }
    };

    /** One 2x stage, both directions, for one channel. Even coefficients form path 0, odd ones path 1. */
    struct HalfBand
    {
        static constexpr int maxCoefs = 16;
        int n = 0; double c[maxCoefs] {};
        double ux[maxCoefs] {}, uy[maxCoefs] {};             // upsampler allpass states (x[n-1], y[n-1]) per stage
        double dx[maxCoefs] {}, dy[maxCoefs] {};             // downsampler states
        void setCoefs (const std::vector<double>& co)   { n = std::min ((int) co.size(), maxCoefs); for (int i = 0; i < n; ++i) c[i] = co[(size_t) i]; reset(); }
        void reset()   { std::fill (std::begin (ux), std::end (ux), 0.0); std::fill (std::begin (uy), std::end (uy), 0.0); std::fill (std::begin (dx), std::end (dx), 0.0); std::fill (std::begin (dy), std::end (dy), 0.0); }

        static inline double ap (double x, double coef, double& xs, double& ys)   { const double y = coef * (x - ys) + xs; xs = x; ys = y; return y; }

        /** One input sample -> two output samples (gain 1 in the pass band). */
        inline void up (float in, float& o0, float& o1)
        {
            double p0 = in, p1 = in;
            for (int i = 0; i < n; i += 2) p0 = ap (p0, c[i], ux[i], uy[i]);
            for (int i = 1; i < n; i += 2) p1 = ap (p1, c[i], ux[i], uy[i]);
            o0 = (float) p0; o1 = (float) p1;
        }
        /** Two input samples -> one output sample. */
        inline float down (float i0, float i1)
        {
            double p0 = i1, p1 = i0;
            for (int i = 0; i < n; i += 2) p0 = ap (p0, c[i], dx[i], dy[i]);
            for (int i = 1; i < n; i += 2) p1 = ap (p1, c[i], dx[i], dy[i]);
            return (float) (0.5 * (p0 + p1));
        }
        void flushDenormals()
        {
            for (int i = 0; i < n; ++i)
            {
                auto z = [] (double& v) { if (std::abs (v) < 1.0e-25) v = 0; };
                z (ux[i]); z (uy[i]); z (dx[i]); z (dy[i]);
            }
        }
    };

    /** Linear-phase FIR half-band (Kaiser-windowed sinc), polyphase, zero taps skipped. One channel, both directions. */
    struct FirHalfBand
    {
        static constexpr int maxTaps = 256;
        // nonzero taps of each polyphase branch (interpolation gain 2 folded in)
        float up0[maxTaps / 2 + 2] {}, up1[maxTaps / 2 + 2] {}; int nUp0 = 0, nUp1 = 0, off0 = 0, off1 = 0;
        float h[maxTaps] {}; int len = 0;
        std::vector<float> xh, vh; int xw = 0, vw = 0, xn = 0, vn = 0;   // doubled rings: contiguous reads

        static std::vector<double> design (double attenuationDb, double transition)
        {
            const double beta = attenuationDb > 50 ? 0.1102 * (attenuationDb - 8.7) : 0.5842 * std::pow (attenuationDb - 21, 0.4) + 0.07886 * (attenuationDb - 21);
            int n = (int) std::ceil ((attenuationDb - 8.0) / (2.285 * 2.0 * pi * transition)) + 1;
            while ((n - 3) % 4 != 0) ++n;                       // 4k + 3: odd centre, symmetric zeros
            n = std::min (n, maxTaps - 1);
            while ((n - 3) % 4 != 0) --n;
            auto bessel0 = [] (double x) { double sum = 1, term = 1; for (int k = 1; k < 50; ++k) { term *= (x / (2 * k)) * (x / (2 * k)); sum += term; } return sum; };
            std::vector<double> t ((size_t) n); const double c = (n - 1) / 2.0, i0b = bessel0 (beta);
            for (int i = 0; i < n; ++i)
            {
                const double m = i - c, r = m / c;
                const double sinc = std::abs (m) < 1e-12 ? 0.5 : std::sin (pi * 0.5 * m) / (pi * m);
                t[(size_t) i] = sinc * bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            }
            return t;
        }
        void setTaps (const std::vector<double>& t)
        {
            len = std::min ((int) t.size(), maxTaps);
            for (int i = 0; i < len; ++i) h[i] = (float) t[(size_t) i];
            nUp0 = nUp1 = 0;
            for (int k = 0; k < len; ++k)
            {
                if (std::abs (h[k]) < 1.0e-9f) continue;
                if ((k & 1) == 0) { if (nUp0 == 0) off0 = k / 2; up0[nUp0++] = 2.0f * h[k]; }
                else              { if (nUp1 == 0) off1 = (k - 1) / 2; up1[nUp1++] = 2.0f * h[k]; }
            }
            xn = len / 2 + 4; vn = len + 4;
            xh.assign ((size_t) xn * 2, 0.0f); vh.assign ((size_t) vn * 2, 0.0f); reset();
        }
        void reset()   { std::fill (xh.begin(), xh.end(), 0.0f); std::fill (vh.begin(), vh.end(), 0.0f); xw = vw = 0; }
        inline void up (float in, float& o0, float& o1)
        {
            xh[(size_t) xw] = xh[(size_t) (xw + xn)] = in;
            const float* x = xh.data() + xw + xn;                  // x[0] newest, x[-j] older
            // branch 0 uses even taps h[2j] on x[n - j]; branch 1 odd taps h[2j+1] on x[n - j]
            float a = 0, b = 0;
            if (nUp0 == 1) a = up0[0] * x[-off0];
            else { int j = off0; for (int k = 0; k < nUp0; ++k, ++j) a += up0[k] * x[-j]; }
            if (nUp1 == 1) b = up1[0] * x[-off1];
            else { int j = off1; for (int k = 0; k < nUp1; ++k, ++j) b += up1[k] * x[-j]; }
            o0 = a; o1 = b;
            if (++xw >= xn) xw = 0;
        }
        inline float down (float i0, float i1)
        {
            vh[(size_t) vw] = vh[(size_t) (vw + vn)] = i0; if (++vw >= vn) vw = 0;
            vh[(size_t) vw] = vh[(size_t) (vw + vn)] = i1;
            const float* v = vh.data() + vw + vn;                  // v[0] = i1 (newest)
            float acc = 0;
            for (int k = 0; k < len; ++k) if (h[k] != 0.0f) acc += h[k] * v[-k];
            if (++vw >= vn) vw = 0;
            return acc;
        }
        void flushDenormals() {}
    };

    /** Up to 16x oversampling for up to 2 channels, minimum-phase (polyphase IIR, the default: ~5 samples round trip)
        or linear-phase (Kaiser FIR: no phase shift, more latency). Stage 0 sits at the base rate and has the steepest
        transition; later stages only reject images far above the audio band, so they are cheaper. Stop band 110 dB.
        The factor and phase can change on the audio thread (no allocation); latency for every combination is measured
        once in prepare(). */
    class Oversampler
    {
    public:
        static constexpr int maxStages = 4, maxChannels = 2;
        enum Phase { minimumPhase, linearPhase };

        void prepare (int maxBlockSize, int factorLog2, double baseRate, int phase = minimumPhase)
        {
            static const double tbw[maxStages] = { 0.0425, 0.24, 0.36, 0.42 };
            static const double tbwFir[maxStages] = { 0.0425, 0.20, 0.30, 0.36 };
            for (int s = 0; s < maxStages; ++s)
            {
                const auto co = HalfBandDesign::design (110.0, tbw[s]);
                const auto taps = FirHalfBand::design (110.0, tbwFir[s]);
                for (int ch = 0; ch < maxChannels; ++ch) { hb[ch][s].setCoefs (co); fir[ch][s].setTaps (taps); }
                coefCount[s] = (int) co.size(); tapCount[s] = (int) taps.size();
            }
            maxBlock = std::max (1, maxBlockSize);
            for (int ch = 0; ch < maxChannels; ++ch)
            {
                bufA[ch].assign ((size_t) (maxBlock << maxStages), 0.0f);
                bufB[ch].assign ((size_t) (maxBlock << maxStages), 0.0f);
            }
            rate = baseRate;
            for (int ph = 0; ph < 2; ++ph)
                for (int f = 0; f <= maxStages; ++f)
                {
                    stages = f; mode = ph; reset();
                    latencyTable[ph][f] = measureLatency (baseRate * (ph == linearPhase ? 17.0 : 341.0) / 16384.0);   // whole cycles in the window; FIR ~50 Hz (latency under one period), IIR ~1 kHz   // FIR: low enough that its latency is under one period; IIR: phase delay at 1 kHz
                }
            stages = clamp (factorLog2, 0, maxStages); mode = phase == linearPhase ? linearPhase : minimumPhase;
            reset();
        }
        /** Safe on the audio thread: switches the factor / phase and clears the filter states. */
        void setFactorLog2 (int f, int phase)
        {
            f = clamp (f, 0, maxStages); phase = phase == linearPhase ? linearPhase : minimumPhase;
            if (f == stages && phase == mode) return;
            stages = f; mode = phase; reset();
        }
        void reset()   { for (auto& c : hb) for (auto& s : c) s.reset(); for (auto& c : fir) for (auto& s : c) s.reset(); }
        int factor() const          { return 1 << stages; }
        int factorLog2() const      { return stages; }
        int phase() const           { return mode; }
        int maxBlockSize() const    { return maxBlock; }
        /** Round-trip (up + down) delay at 1 kHz in base-rate samples: what the host should compensate. */
        double latencySamples() const   { return latencyTable[mode][stages]; }
        double latencySamplesFor (int f, int ph) const { return latencyTable[ph == linearPhase ? 1 : 0][clamp (f, 0, maxStages)]; }
        int coefficients (int stage) const { return coefCount[clamp (stage, 0, maxStages - 1)]; }
        int firTaps (int stage) const      { return tapCount[clamp (stage, 0, maxStages - 1)]; }

        /** Upsample n base-rate samples of one channel; returns the oversampled buffer (n * factor samples). */
        float* up (int ch, const float* in, int n)
        {
            ch = clamp (ch, 0, maxChannels - 1); n = std::min (n, maxBlock);
            float* src = bufA[ch].data();
            if (src != in) std::memmove (src, in, sizeof (float) * (size_t) n);
            if (stages == 0) return src;
            float* dst = bufB[ch].data(); int len = n;
            for (int s = 0; s < stages; ++s)
            {
                if (mode == minimumPhase) { auto& f = hb[ch][s];  for (int i = 0; i < len; ++i) f.up (src[i], dst[2 * i], dst[2 * i + 1]); }
                else                      { auto& f = fir[ch][s]; for (int i = 0; i < len; ++i) f.up (src[i], dst[2 * i], dst[2 * i + 1]); }
                std::swap (src, dst); len *= 2;
            }
            return src;
        }
        /** Downsample n * factor samples (normally the buffer up() returned, processed in place) to n samples in out. */
        void down (int ch, float* osBuf, int n, float* out)
        {
            ch = clamp (ch, 0, maxChannels - 1); n = std::min (n, maxBlock);
            if (stages == 0) { if (out != osBuf) std::memmove (out, osBuf, sizeof (float) * (size_t) n); return; }
            float* src = osBuf;
            float* dst = (src == bufA[ch].data()) ? bufB[ch].data() : bufA[ch].data();
            int len = n << stages;
            for (int s = stages - 1; s >= 0; --s)
            {
                len /= 2;
                if (mode == minimumPhase) { auto& f = hb[ch][s];  for (int i = 0; i < len; ++i) dst[i] = f.down (src[2 * i], src[2 * i + 1]); }
                else                      { auto& f = fir[ch][s]; for (int i = 0; i < len; ++i) dst[i] = f.down (src[2 * i], src[2 * i + 1]); }
                std::swap (src, dst);
            }
            std::memmove (out, src, sizeof (float) * (size_t) n);
            if (mode == minimumPhase) for (int s = 0; s < stages; ++s) hb[ch][s].flushDenormals();
        }

    private:
        int stages = 0, mode = minimumPhase, maxBlock = 512, coefCount[maxStages] {}, tapCount[maxStages] {};
        double rate = 48000, latencyTable[2][maxStages + 1] {};
        HalfBand hb[maxChannels][maxStages];
        FirHalfBand fir[maxChannels][maxStages];
        std::vector<float> bufA[maxChannels], bufB[maxChannels];

        /** Round-trip delay at f Hz, measured: run a sine through up + down and read its phase from the quadrature
            projections (exact to a few thousandths of a sample). Uses channel 0 then clears it. Not real-time. */
        double measureLatency (double hz)
        {
            if (stages == 0) return 0.0;
            const int block = std::min (maxBlock, 256), total = 32768;
            std::vector<float> in ((size_t) block), out ((size_t) block);
            const double w = 2.0 * pi * hz / rate; double S = 0, C = 0;
            for (int p = 0; p < total; p += block)
            {
                for (int i = 0; i < block; ++i) in[(size_t) i] = (float) std::sin (w * (p + i));
                float* o = up (0, in.data(), block); down (0, o, block, out.data());
                if (p >= total / 2) for (int i = 0; i < block; ++i) { S += out[(size_t) i] * std::sin (w * (p + i)); C += out[(size_t) i] * std::cos (w * (p + i)); }
            }
            reset();
            double d = std::atan2 (-C, S) / w;
            while (d < 0) d += 2.0 * pi / w;
            return d;
        }
    };

    //==========================================================================
    // Nonlinear core: the ten Cinder models
    //==========================================================================
    /** Each model is a stateful stage running at the oversampled rate. Drive is linear gain into the stage; the stage
        is normalised so that its small-signal gain is 1 (drive changes harmonics, the caller handles loudness).
        Every memoryless curve runs through first-order antiderivative anti-aliasing (ADAA): the antiderivative is
        tabulated once (double precision, Hermite-interpolated with the curve itself as the slope), which removes most
        of the remaining fold-back on top of oversampling. Asymmetry comes from an operating-point offset that follows
        the signal level (grid-conduction / bias-shift behaviour), so even harmonics survive at any drive instead of
        collapsing into DC. */
    namespace Nonlinear
    {
        enum Model { glow, pentode, tape, transformer, console, diode, germanium, rectify, fold, brick, numModels };
        inline const char* modelName (int m)
        {
            static const char* n[] = { "Glow", "Pentode", "Tape", "Iron", "Console", "Diode", "Germ", "Rectify", "Fold", "Brick" };
            return n[clamp (m, 0, (int) numModels - 1)];
        }
        inline const char* modelDescription (int m)
        {
            static const char* d[] = {
                "Triode stage: asymmetric grid conduction, bias that shifts and sags with level - warm even harmonics that bloom on transients",
                "Pentode stage: flatter knee, more odd harmonics, a brighter, more forward growl",
                "Tape: soft symmetrical compression with level-dependent memory (hysteresis) and a gentle high-frequency rounding",
                "Iron: transformer core saturation - lows saturate first (flux grows as frequency falls), highs stay clean",
                "Console: op-amp / transistor line stage - clean until the rails, then a smooth knee",
                "Diode: silicon clipper, one diode one way and two the other - the classic pedal edge",
                "Germ: germanium transistor fuzz - low threshold, gated, sputtering asymmetry",
                "Rectify: half-wave rectifier blend - octave-up even harmonics and a hollow buzz",
                "Fold: wave folder - the signal folds back on itself instead of clipping, metallic overtones",
                "Brick: hard clip with a tiny knee - maximum loudness, maximum bite" };
            return d[clamp (m, 0, (int) numModels - 1)];
        }

        inline double softAlgD (double x)  { return x / std::sqrt (1.0 + x * x); }

        /** A memoryless curve with its antiderivative tabulated for ADAA. */
        struct Curve
        {
            static constexpr double range = 64.0, step = 1.0 / 32.0;
            std::vector<double> F, f; int n = 0; double fLo = 0, fHi = 0, FLo = 0, FHi = 0;
            template <typename Fn> void build (Fn fn)
            {
                n = (int) std::round (2 * range / step) + 1;
                F.assign ((size_t) n, 0.0); f.assign ((size_t) n, 0.0);
                for (int i = 0; i < n; ++i) f[(size_t) i] = fn (-range + i * step);
                // integrate from 0 outwards (Simpson on 8 sub-steps) so F(0) = 0 and the table is symmetric in error
                const int z = (int) std::round (range / step);
                auto seg = [&fn] (double x0, double x1)
                {
                    const int m = 8; const double h = (x1 - x0) / m; double s = fn (x0) + fn (x1);
                    for (int k = 1; k < m; ++k) s += fn (x0 + k * h) * (k % 2 ? 4.0 : 2.0);
                    return s * h / 3.0;
                };
                for (int i = z + 1; i < n; ++i) F[(size_t) i] = F[(size_t) (i - 1)] + seg (-range + (i - 1) * step, -range + i * step);
                for (int i = z - 1; i >= 0; --i) F[(size_t) i] = F[(size_t) (i + 1)] - seg (-range + i * step, -range + (i + 1) * step);
                fLo = f.front(); fHi = f.back(); FLo = F.front(); FHi = F.back();
            }
            inline double eval (double x) const      // f(x), linear between table points (used for tiny steps)
            {
                if (x <= -range) return fLo;
                if (x >= range) return fHi;
                const double p = (x + range) / step; const int i = std::min (n - 2, (int) p); const double t = p - i;
                return f[(size_t) i] + (f[(size_t) i + 1] - f[(size_t) i]) * t;
            }
            inline double anti (double x) const      // F(x), cubic Hermite with f as the slope
            {
                if (x <= -range) return FLo + fLo * (x + range);
                if (x >= range)  return FHi + fHi * (x - range);
                const double p = (x + range) / step; const int i = std::min (n - 2, (int) p); const double t = p - i;
                const double t2 = t * t, t3 = t2 * t;
                return (2 * t3 - 3 * t2 + 1) * F[(size_t) i] + (t3 - 2 * t2 + t) * step * f[(size_t) i] + (-2 * t3 + 3 * t2) * F[(size_t) i + 1] + (t3 - t2) * step * f[(size_t) i + 1];
            }
        };

        Curve buildCurve (int model, double character);

        /** Shared, immutable curves for every (model, character to 1/100): built once, then read lock-free. Call
            warm() from prepare() for every combination the audio thread may switch to. */
        inline const Curve& curveFor (int model, float character)
        {
            static std::atomic<Curve*> table[numModels][101] {};
            static std::mutex buildLock;
            const int m = clamp (model, 0, (int) numModels - 1), c = clamp ((int) std::lround (character * 100.0f), 0, 100);
            if (auto* p = table[m][c].load (std::memory_order_acquire)) return *p;
            std::lock_guard<std::mutex> lock (buildLock);
            if (auto* p = table[m][c].load (std::memory_order_acquire)) return *p;
            auto* fresh = new Curve (buildCurve (m, c / 100.0));       // lives for the whole run (shared by every instance)
            table[m][c].store (fresh, std::memory_order_release);
            return *fresh;
        }
        inline void warm (int model, float character)   { (void) curveFor (model, character); }

        /** ADAA state for one curve. */
        struct Adaa
        {
            double prevX = 0, prevF = 0;
            void reset (const Curve& c, double x0 = 0) { prevX = x0; prevF = c.anti (x0); }
            inline double process (const Curve& c, double x)
            {
                const double Fx = c.anti (x), dx = x - prevX;
                const double y = std::abs (dx) > 1.0e-6 ? (Fx - prevF) / dx : c.eval (0.5 * (x + prevX));
                prevX = x; prevF = Fx;
                return y;
            }
        };

        /** Second-order ADAA of the hard clip at +-1 (analytic antiderivatives): ~20 dB less fold-back than first order. */
        struct Adaa2Clip
        {
            double x1 = 0, x2 = 0;
            static double F1 (double x) { const double a = std::abs (x); return a <= 1 ? 0.5 * x * x : a - 0.5; }
            static double F2 (double x) { if (x > 1) return 0.5 * x * x - 0.5 * x + 1.0 / 6.0; if (x < -1) return -(0.5 * x * x) - 0.5 * x - 1.0 / 6.0; return x * x * x / 6.0; }
            static double clip (double x) { return x > 1 ? 1 : (x < -1 ? -1 : x); }
            static double D1 (double a, double b) { const double d = a - b; return std::abs (d) > 1.0e-5 ? (F2 (a) - F2 (b)) / d : F1 (0.5 * (a + b)); }
            void reset() { x1 = x2 = 0; }
            inline double process (double x0)
            {
                const double d = x0 - x2;
                const double y = std::abs (d) > 1.0e-5 ? 2.0 / d * (D1 (x0, x1) - D1 (x1, x2)) : clip (0.25 * x0 + 0.5 * x1 + 0.25 * x2);
                x2 = x1; x1 = x0;
                return y;
            }
        };

        struct Stage
        {
            int model = glow;
            float drive = 1, bias = 0, character = 0.5f;     // character: model-specific (sag, hysteresis, knee...)
            double rate = 192000;
            const Curve* curve = nullptr; const Curve* tanhCurve = nullptr; Adaa adaa, adaaT; Adaa2Clip clip2;
            float env = 0, sagAtk = 0, sagRel = 0, hyst = 0, gate = 0, gateCoef = 0, lpZ = 0, lpG = 0, ironG = 0;
            DcBlock dc;

            void prepare (double oversampledRate)
            {
                rate = oversampledRate;
                sagAtk = (float) (1.0 - std::exp (-1.0 / (0.004 * rate)));
                sagRel = (float) (1.0 - std::exp (-1.0 / (0.120 * rate)));
                gateCoef = (float) (1.0 - std::exp (-1.0 / (0.010 * rate)));
                const double w = std::tan (pi * std::min (16000.0, rate * 0.45) / rate); lpG = (float) (w / (1.0 + w));
                const double wi = std::tan (pi * 160.0 / rate); ironG = (float) (wi / (1.0 + wi));
                dc.prepare (rate, 4.0);
                rebuild();
                reset();
            }
            /** Picks up the shared curve for the current model / character (no allocation once warmed). */
            void rebuild()   { curve = &curveFor (model, character); tanhCurve = &curveFor (tape, 0.0f); }
            void reset()   { env = hyst = gate = lpZ = 0; dc.reset(); if (curve == nullptr) rebuild(); adaa.reset (*curve); adaaT.reset (*tanhCurve); clip2.reset(); }

            /** Operating-point offset that follows the level: asymmetric duty cycle at any drive (even harmonics). */
            inline float offset (float a)
            {
                env += (a > env ? sagAtk : sagRel) * (a - env);
                return bias * (0.04f + 0.45f * std::min (env, 8.0f));
            }
            inline float shaped (float v, float off)
            {
                return (float) (adaa.process (*curve, (double) v + off) - curve->eval (off));
            }
            inline float process (float in)
            {
                const float x = in * drive;
                float y = 0;
                switch (model)
                {
                    case glow:
                    {
                        const float off = offset (std::abs (x));
                        y = shaped (x, off) / (1.0f + character * 0.12f * std::min (env, 8.0f));   // sag: the stage gives a little under load
                        break;
                    }
                    case pentode:   y = shaped (x, offset (std::abs (x)) * 0.5f); break;
                    case tape:
                    {
                        // memory: the core follows the curve through a level-dependent lag, giving hysteresis loops
                        const float off = offset (std::abs (x)) * 0.3f;
                        const float target = shaped (x, off);
                        const float speed = 0.25f + 0.75f * (1.0f - character) / (1.0f + std::min (env, 8.0f) * 2.0f);   // follows the level, not the waveform (no added aliasing)
                        hyst += speed * (target - hyst);
                        y = 0.7f * target + 0.3f * hyst;
                        lpZ += lpG * (y - lpZ); y = lpZ;                                   // head-gap rounding
                        break;
                    }
                    case transformer:
                    {
                        // the core saturates on flux, and flux grows as frequency falls: split at a low corner, saturate
                        // the lows (curve + ADAA), the highs only through the gentle overall ceiling
                        lpZ += ironG * (x - lpZ);
                        const float lows = lpZ, highs = x - lows;
                        const float k = 1.0f + character * 2.0f;
                        const float satLows = shaped (lows * k, bias * 0.1f) / k;
                        y = 2.0f * (float) softAlgD ((satLows + highs) * 0.5);
                        break;
                    }
                    case diode:     y = shaped (x, offset (std::abs (x)) * 0.6f); break;
                    case germanium:
                    {
                        // low-threshold, gated: the starved bias makes quiet tails sputter
                        const float a = std::abs (in);
                        gate += gateCoef * (a - gate);
                        const float starve = character * character * 0.02f;
                        const float gr = gate / std::max (1.0e-7f, starve);
                        const float g = gr >= 1.0f ? 1.0f : gr * gr * gr;
                        y = shaped (x * g, offset (std::abs (x * g)) * 0.8f + bias * 0.05f);
                        break;
                    }
                    case rectify:   y = shaped (x, 0.0f); break;                                 // its asymmetry is the curve itself
                    case brick:
                    {
                        const float off = offset (std::abs (x)) * 0.5f;
                        const double v = (double) x + off;
                        y = (float) ((1.0 - 0.5 * character) * (clip2.process (v) - Adaa2Clip::clip (off))
                                     + 0.5 * character * (adaaT.process (*tanhCurve, v) - std::tanh ((double) off)));
                        break;
                    }
                    default:        y = shaped (x, offset (std::abs (x)) * 0.5f); break;      // console, fold
                }
                return dc.process (y);
            }
        };
        inline Curve buildCurve (int model, double ch)
        {
            Curve c;
            switch (model)
            {
                case glow:        c.build ([] (double u) { return u > 0 ? std::tanh (1.25 * u) / 1.25 : softAlgD (0.75 * u) / 0.75; }); break;
                case pentode:     { const double k = 1.0 + ch * 2.0; c.build ([k] (double u) { return u / std::pow (1.0 + std::pow (std::abs (u), 2.0 * k), 1.0 / (2.0 * k)); }); break; }
                case tape:
                case transformer: c.build ([] (double u) { return std::tanh (u); }); break;
                case console:     { const double hr = 1.0 + (1.0 - ch); c.build ([hr] (double u) { const double v = u / hr, a = std::abs (v);
                                      return (a < 0.6 ? v : (v > 0 ? 1.0 : -1.0) * (0.6 + 0.4 * std::tanh ((a - 0.6) / 0.4))) * hr; }); break; }
                case diode:       { const double vt = 0.35 + 0.4 * (1.0 - ch), negK = 1.0 + ch * 1.2;
                                    c.build ([vt, negK] (double u) { return u > 0 ? vt * std::asinh (u / vt) : vt * negK * std::asinh (u / (vt * negK)); }); break; }   // Shockley: logarithmic, no hard ceiling
                case germanium:   c.build ([] (double u) { return (u > 0 ? 1.0 - std::exp (-u * 1.5) : -(1.0 - std::exp (u * 1.2)) / 1.2 * 1.5) / 1.5; }); break;
                case rectify:     c.build ([ch] (double u) { const double r = u > 0 ? u : u * (1.0 - ch); return std::tanh (r) / (1.0 - 0.5 * ch); }); break;
                case fold:        { const double d = 1.0 + ch * 4.0; c.build ([d] (double u) { u = std::max (-60.0, std::min (60.0, u)); return std::sin (u * d * pi * 0.5) / (d * pi * 0.5); }); break; }
                case brick:       c.build ([ch] (double u) { return (1.0 - 0.5 * ch) * std::max (-1.0, std::min (1.0, u)) + 0.5 * ch * std::tanh (u); }); break;
                default:          c.build ([] (double u) { return u; }); break;
            }
            return c;
        }
    }

    //==========================================================================
    // Analog Style: seven drive voicings (shared by Wing Mod hosts, Larva Filter 2, Twelve Phase 2, Shard...)
    //==========================================================================
    class AnalogStyle
    {
    public:
        enum Voicing { clean, fat, squash, dirt, crunch, shred, pump, numVoicings };
        static const char* voicingName (int v)
        {
            static const char* n[] = { "Clean", "Fat", "Squash", "Dirt", "Crunch", "Shred", "Pump" };
            return n[clamp (v, 0, (int) numVoicings - 1)];
        }
        struct Def { int model; float driveDb, bias, character, lowDb, lowHz, presDb, presHz, compRatio, compThreshDb, attackMs, releaseMs; };
        static const Def& def (int v)
        {
            static const Def d[] = {
                // model                     drive  bias  char   lows        presence      ratio thresh  att   rel
                { Nonlinear::console,        0.0f,  0.0f, 0.2f,  0.0f, 100,  0.0f, 3000, 1.0f,   0.0f, 5.0f, 100.0f },  // Clean
                { Nonlinear::glow,           6.0f,  0.3f, 0.5f,  3.0f, 110, -1.0f, 3500, 1.0f,   0.0f, 5.0f, 100.0f },  // Fat
                { Nonlinear::tape,           4.0f,  0.0f, 0.6f,  1.0f, 90,   0.0f, 3000, 4.0f, -18.0f, 3.0f, 80.0f },   // Squash
                { Nonlinear::diode,         12.0f,  0.2f, 0.4f,  0.0f, 100,  2.0f, 2500, 1.0f,   0.0f, 5.0f, 100.0f },  // Dirt
                { Nonlinear::pentode,       16.0f,  0.1f, 0.6f, -1.0f, 120,  3.0f, 2200, 1.0f,   0.0f, 5.0f, 100.0f },  // Crunch
                { Nonlinear::germanium,     24.0f,  0.4f, 0.5f, -2.0f, 150,  4.0f, 1800, 1.0f,   0.0f, 5.0f, 100.0f },  // Shred
                { Nonlinear::transformer,    6.0f,  0.0f, 0.6f,  2.0f, 80,   0.0f, 3000, 8.0f, -24.0f, 1.0f, 180.0f },  // Pump
            };
            return d[clamp (v, 0, (int) numVoicings - 1)];
        }

        void prepare (double baseRate, int maxBlock, int oversampleLog2)
        {
            sr = baseRate;
            for (int v = 0; v < numVoicings; ++v) Nonlinear::warm (def (v).model, def (v).character);
            os.prepare (maxBlock, oversampleLog2, baseRate);
            for (auto& st : stage) st.prepare (baseRate * os.factor());
            setVoicing (voicing, true);
            reset();
        }
        void reset()   { os.reset(); for (auto& s : stage) s.reset(); for (auto& f : lowSh) f.reset(); for (auto& f : pres) f.reset(); for (auto& f : deEmph) f.reset(); compEnv = 0; eIn = eOut = 0; agGain = 1; }
        void setVoicing (int v, bool force = false)
        {
            if (v == voicing && ! force) return;
            voicing = clamp (v, 0, (int) numVoicings - 1);
            const auto& d = def (voicing);
            for (auto& s : stage) { s.model = d.model; s.bias = d.bias; s.character = d.character; s.rebuild(); s.reset(); }
            for (int c = 0; c < 2; ++c)
            {
                lowSh[c].set (Biquad::lowShelf, sr, d.lowHz, 0.7, d.lowDb);
                pres[c].set (Biquad::peak, sr, d.presHz, 0.8, d.presDb);
                deEmph[c].set (Biquad::highShelf, sr, 6000.0, 0.7, -0.5 * d.presDb);
            }
            compA = (float) (1.0 - std::exp (-1.0 / (d.attackMs * 0.001 * sr)));
            compR = (float) (1.0 - std::exp (-1.0 / (d.releaseMs * 0.001 * sr)));
        }
        int getVoicing() const   { return voicing; }
        /** amount 0..1 scales the voicing's drive (0 = clean through the same filters). */
        void setAmount (float a)  { amount = clamp (a, 0.0f, 1.0f); }
        double latencySamples() const   { return os.latencySamples(); }

        /** Process up to maxBlock samples of up to 2 channels in place. */
        void process (float* const* ch, int nch, int n)
        {
            nch = std::min (nch, 2);
            const auto& d = def (voicing);
            const float drv = dbToGain (d.driveDb * amount);
            const float agA = (float) (1.0 - std::exp (-1.0 / (0.8 * sr)));   // slow loudness match: level stays put, pumping stays
            if (autoGain)
                for (int i = 0; i < n; ++i) { float e = 0; for (int c = 0; c < nch; ++c) e += ch[c][i] * ch[c][i]; eIn += agA * (e - eIn); }
            // dynamics first (Squash / Pump), detector on the stereo sum
            if (d.compRatio > 1.0f)
            {
                const float thr = dbToGain (d.compThreshDb), slope = 1.0f - 1.0f / d.compRatio;
                for (int i = 0; i < n; ++i)
                {
                    float a = 0; for (int c = 0; c < nch; ++c) a = std::max (a, std::abs (ch[c][i]));
                    compEnv += (a > compEnv ? compA : compR) * (a - compEnv);
                    const float over = compEnv > thr ? gainToDb (compEnv / thr) : 0.0f;
                    const float g = dbToGain (-over * slope * amount);
                    for (int c = 0; c < nch; ++c) ch[c][i] *= g;
                }
            }
            for (int c = 0; c < nch; ++c)
            {
                for (int i = 0; i < n; ++i) ch[c][i] = pres[c].process (lowSh[c].process (ch[c][i]));
                float* o = os.up (c, ch[c], n);
                const int m = n * os.factor();
                auto& st = stage[c]; st.drive = drv;
                for (int i = 0; i < m; ++i) o[i] = st.process (o[i]);
                os.down (c, o, n, ch[c]);
                for (int i = 0; i < n; ++i) ch[c][i] = deEmph[c].process (ch[c][i]);
            }
            if (! autoGain) return;
            for (int i = 0; i < n; ++i)
            {
                float eo = 0; for (int c = 0; c < nch; ++c) eo += ch[c][i] * ch[c][i];
                eOut += agA * (eo - eOut);
                const float target = (eOut > 1.0e-12f && eIn > 1.0e-12f) ? clamp (std::sqrt (eIn / eOut), 0.05f, 4.0f) : 1.0f;
                agGain += agA * 4.0f * (target - agGain);
                for (int c = 0; c < nch; ++c) ch[c][i] *= agGain;
            }
        }
    private:
        double sr = 48000; int voicing = 0; float amount = 1, compEnv = 0, compA = 0, compR = 0, eIn = 0, eOut = 0, agGain = 1;
    public:
        bool autoGain = true;
    private:
        Oversampler os;
        Nonlinear::Stage stage[2];
        Biquad lowSh[2], pres[2], deEmph[2];
    };

    //==========================================================================
    // Wing Mod: the modulation engine
    //==========================================================================
    class WingMod
    {
    public:
        enum Mode { lfo, rhythm, envelope, random, step, adsr, numModes };
        enum Shape { sine, triangle, rampUp, rampDown, square, pulse, sampleHold, smoothRandom, custom, numShapes };
        enum Smoothing { off, light, medium, heavy, glide, numSmoothing };
        enum Trigger { audio, midi, manual };
        static const char* modeName (int m)  { static const char* n[] = { "LFO", "Rhythm", "Envelope", "Random", "Step", "ADSR" }; return n[clamp (m, 0, (int) numModes - 1)]; }
        static const char* shapeName (int s) { static const char* n[] = { "Sine", "Triangle", "Ramp Up", "Ramp Down", "Square", "Pulse", "S&H", "Smooth Random", "Custom" }; return n[clamp (s, 0, (int) numShapes - 1)]; }
        static const char* smoothingName (int s) { static const char* n[] = { "Off", "Light", "Medium", "Heavy", "Glide" }; return n[clamp (s, 0, (int) numSmoothing - 1)]; }
        /** Tempo-synced lengths in beats (quarter notes), straight / dotted / triplet, 1/64 .. 4 bars. */
        static int numSyncDivisions()   { return 27; }
        static double syncBeats (int idx)
        {
            static const double b[] = { 1.0 / 16, 1.0 / 16 * 1.5, 1.0 / 16 * 2.0 / 3,  1.0 / 8, 1.0 / 8 * 1.5, 1.0 / 8 * 2.0 / 3,
                                        1.0 / 4, 1.0 / 4 * 1.5, 1.0 / 4 * 2.0 / 3,     1.0 / 2, 1.0 / 2 * 1.5, 1.0 / 2 * 2.0 / 3,
                                        1.0, 1.5, 2.0 / 3,                             2.0, 3.0, 4.0 / 3,
                                        4.0, 6.0, 8.0 / 3,                             8.0, 12.0, 16.0 / 3,
                                        16.0, 24.0, 32.0 / 3 };
            return b[clamp (idx, 0, numSyncDivisions() - 1)];
        }
        static const char* syncName (int idx)
        {
            static const char* n[] = { "1/64", "1/64.", "1/64T", "1/32", "1/32.", "1/32T", "1/16", "1/16.", "1/16T", "1/8", "1/8.", "1/8T",
                                       "1/4", "1/4.", "1/4T", "1/2", "1/2.", "1/2T", "1 bar", "1 bar.", "1 bar T", "2 bars", "2 bars.", "2 bars T",
                                       "4 bars", "4 bars.", "4 bars T" };
            return n[clamp (idx, 0, numSyncDivisions() - 1)];
        }

        struct Settings
        {
            int mode = lfo, shape = sine, smoothing = off, trigger = audio;
            bool sync = false, bipolar = true, retrigger = false;
            double rateHz = 1.0; int syncIndex = 12;                  // 1/4
            float phaseDeg = 0, stereoPhaseDeg = 0, pulseWidth = 0.5f;
            float custom[16] { 0, 0.38f, 0.71f, 0.92f, 1, 0.92f, 0.71f, 0.38f, 0, -0.38f, -0.71f, -0.92f, -1, -0.92f, -0.71f, -0.38f };
            int customPoints = 16;
            // rhythm
            float steps[16] { 1, 0, 0.6f, 0, 1, 0, 0.6f, 0, 1, 0, 0.6f, 0, 1, 0, 0.6f, 0 };
            int numSteps = 16, stepDivision = 6;                      // index into syncBeats: 1/16
            float swing = 0, feel = 0, stepGate = 0.9f, stepRampMs = 3;
            // envelope follower
            float attackMs = 10, releaseMs = 150, sensitivityDb = 0, floorDb = -48;
            // trigger (step / ADSR / retrigger): audio threshold with hysteresis
            float thresholdDb = -24, hysteresisDb = 6, holdMs = 30;
            // ADSR
            float aMs = 5, dMs = 150, sLevel = 0.6f, rMs = 300;
        };

        void prepare (double sampleRate)  { sr = sampleRate; reset(); }
        void reset()
        {
            for (int c = 0; c < 2; ++c) { phase[c] = 0; held[c] = 0; prevHeld[c] = 0; nextHeld[c] = 0; smoothed[c] = 0; glideVel[c] = 0; }
            env = 0; envFast = 0; trigEnv = 0; triggered = false; holdCount = 0; stepIndex = -1; adsrStage = idle; adsrLevel = 0; rngState = 0x2545F491u;
            transportPpq = 0; transportBpm = 120; playing = false; sampleClock = 0;
        }
        Settings s;

        /** Call once per block with the host transport. ppq = position in quarter notes at the first sample of the block. */
        void setTransport (double bpm, double ppqAtBlockStart, bool isPlaying)
        {
            transportBpm = bpm > 1 ? bpm : 120; transportPpq = ppqAtBlockStart; playing = isPlaying; sampleClock = 0;
        }
        /** Fire a manual or MIDI trigger (Step advances, ADSR starts, retriggered LFOs restart). */
        void trigger()   { pendingTrigger = true; }
        void release()   { pendingRelease = true; }

        /** One sample: detector = the (sidechain or main) signal used by Envelope / triggers. Returns L and R values,
            bipolar -1..1 or unipolar 0..1 per Settings::bipolar. */
        inline void next (float detector, float& outL, float& outR)
        {
            const double ppq = transportPpq + (double) sampleClock * transportBpm / (60.0 * sr);
            ++sampleClock;
            // trigger detector (shared by Step / ADSR / retrigger)
            bool fire = false, rel = false;
            {
                const float a = std::abs (detector);
                trigEnv = std::max (a, trigEnv * trigDecay());
                const float on = dbToGain (s.thresholdDb), off = dbToGain (s.thresholdDb - s.hysteresisDb);
                if (s.trigger == audio)
                {
                    if (! triggered && trigEnv > on && holdCount <= 0) { triggered = true; fire = true; holdCount = (int) (s.holdMs * 0.001 * sr); }
                    else if (triggered && trigEnv < off) { triggered = false; rel = true; }
                    if (holdCount > 0) --holdCount;
                }
                if (pendingTrigger) { fire = true; pendingTrigger = false; }
                if (pendingRelease) { rel = true; pendingRelease = false; }
            }
            float v[2] {};
            switch (s.mode)
            {
                case lfo:      lfoTick (ppq, fire, v); break;
                case rhythm:   rhythmTick (ppq, v); break;
                case envelope: envTick (detector, v); break;
                case random:   randomTick (ppq, fire, v); break;
                case step:     stepTick (fire, v); break;
                case adsr:     adsrTick (fire, rel, v); break;
                default: break;
            }
            for (int c = 0; c < 2; ++c) v[c] = smooth (c, v[c]);
            // modes are produced bipolar (lfo / random / custom) or unipolar (rhythm / env / step / adsr); map to the request
            const bool producedBipolar = (s.mode == lfo || s.mode == random);
            for (int c = 0; c < 2; ++c)
            {
                float x = v[c];
                if (producedBipolar && ! s.bipolar) x = 0.5f * (x + 1.0f);
                else if (! producedBipolar && s.bipolar) x = 2.0f * x - 1.0f;
                v[c] = x;
            }
            outL = v[0]; outR = v[1];
        }
        /** Block helper: fills L (and R if given) with modulation values. */
        void process (const float* detector, float* outL, float* outR, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                float l, r; next (detector != nullptr ? detector[i] : 0.0f, l, r);
                outL[i] = l; if (outR != nullptr) outR[i] = r;
            }
        }
        double cycleHz() const
        {
            return s.sync ? transportBpm / 60.0 / syncBeats (s.syncIndex) : s.rateHz;
        }
        float envelopeLevel() const   { return env; }
        int currentStep() const       { return stepIndex; }

    private:
        double sr = 48000, transportPpq = 0, transportBpm = 120; bool playing = false; int64_t sampleClock = 0;
        double phase[2] {}; float held[2] {}, prevHeld[2] {}, nextHeld[2] {}, smoothed[2] {}, glideVel[2] {};
        float env = 0, envFast = 0, trigEnv = 0; bool triggered = false, pendingTrigger = false, pendingRelease = false; int holdCount = 0;
        int stepIndex = -1; enum { idle, att, dec, sus, relSt } adsrStage = idle; float adsrLevel = 0; uint32_t rngState = 0x2545F491u;

        float trigDecay() const   { return (float) std::exp (-1.0 / (0.005 * sr)); }
        float rnd()   { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return (float) (rngState & 0xffffff) / 8388607.5f - 1.0f; }

        static float shapeAt (const Settings& st, int shape, double ph, float holdV)
        {
            const float p = (float) (ph - std::floor (ph));
            switch (shape)
            {
                case sine:      return (float) std::sin (2.0 * pi * p);
                case triangle:  return p < 0.25f ? 4 * p : (p < 0.75f ? 2 - 4 * p : 4 * p - 4);
                case rampUp:    return 2 * p - 1;
                case rampDown:  return 1 - 2 * p;
                case square:    return p < 0.5f ? 1.0f : -1.0f;
                case pulse:     return p < st.pulseWidth ? 1.0f : -1.0f;
                case sampleHold:
                case smoothRandom: return holdV;
                case custom:
                {
                    const int np = clamp (st.customPoints, 2, 16);
                    const float x = p * (float) np; const int i0 = (int) x % np, i1 = (i0 + 1) % np; const float f = x - std::floor (x);
                    return st.custom[i0] + (st.custom[i1] - st.custom[i0]) * f;
                }
                default: return 0;
            }
        }
        void lfoTick (double ppq, bool fire, float* v)
        {
            const double base = s.phaseDeg / 360.0;
            for (int c = 0; c < 2; ++c)
            {
                const double off = base + (c == 1 ? s.stereoPhaseDeg / 360.0 : 0.0);
                double ph;
                if (s.sync && playing)
                    ph = ppq / syncBeats (s.syncIndex) + off;          // locked to the bar, survives loops and jumps
                else
                {
                    if (fire && s.retrigger) phase[c] = 0;
                    phase[c] += s.rateHz / sr * (s.sync ? (transportBpm / 60.0 / syncBeats (s.syncIndex)) / std::max (1.0e-9, s.rateHz) : 1.0);
                    if (phase[c] >= 1.0) phase[c] -= std::floor (phase[c]);
                    ph = phase[c] + off;
                }
                // random shapes: new value each cycle
                const double cyc = std::floor (ph);
                if (cyc != lastCycle[c])
                {
                    lastCycle[c] = cyc;
                    prevHeld[c] = nextHeld[c];
                    nextHeld[c] = (c == 1 && s.stereoPhaseDeg == 0) ? nextHeld[0] : rnd();
                    held[c] = nextHeld[c];
                }
                float hv = held[c];
                if (s.shape == smoothRandom)
                {
                    const float f = (float) (ph - cyc), w = 0.5f - 0.5f * (float) std::cos (pi * f);
                    hv = prevHeld[c] + (nextHeld[c] - prevHeld[c]) * w;
                }
                v[c] = shapeAt (s, s.shape, ph, hv);
            }
        }
        double lastCycle[2] { -1, -1 };

        void rhythmTick (double ppq, float* v)
        {
            const double stepBeats = syncBeats (s.stepDivision);
            double pos;
            if (playing) pos = ppq / stepBeats;
            else { phase[0] += transportBpm / 60.0 / stepBeats / sr; pos = phase[0]; }
            const int ns = clamp (s.numSteps, 1, 16);
            // swing delays every second step: within each pair the boundary moves from 1 to 1 + swing / 2 steps
            const double pair = std::floor (pos * 0.5), p2 = pos - pair * 2.0, b = 1.0 + 0.5 * clamp (s.swing, 0.0f, 1.0f);
            const bool second = p2 >= b;
            const long long idx = (long long) pair * 2 + (second ? 1 : 0);
            const double local = second ? (p2 - b) / (2.0 - b) : p2 / b;     // 0..1 through the step
            const int si = (int) (((idx % ns) + ns) % ns);
            stepIndex = si;
            const float level = s.steps[si];
            const bool accent = level > 0.85f;
            const float scale = accent ? 1.0f : 1.0f - s.feel * 0.25f;                       // feel: accents stand out
            const float gateLen = clamp (s.stepGate * (accent ? 1.0f : 1.0f - s.feel * 0.3f), 0.02f, 1.0f);
            const double stepSeconds = stepBeats * 60.0 / transportBpm * (second ? (2.0 - b) : b);
            const double rampFrac = std::max (1.0e-4, s.stepRampMs * 0.001 / stepSeconds);
            double g = 0;
            if (local < gateLen) g = std::min (1.0, local / rampFrac) * std::min (1.0, (gateLen - local) / rampFrac);
            v[0] = v[1] = clamp (level * scale * (float) g, 0.0f, 1.0f);
        }
        void envTick (float det, float* v)
        {
            const float a = std::abs (det) * dbToGain (s.sensitivityDb);
            const float ca = (float) (1.0 - std::exp (-1.0 / (std::max (0.05f, s.attackMs) * 0.001 * sr)));
            const float cr = (float) (1.0 - std::exp (-1.0 / (std::max (1.0f, s.releaseMs) * 0.001 * sr)));
            env += (a > env ? ca : cr) * (a - env);
            env = undenorm (env);
            // map in dB so the knob feels musical: floor .. 0 dBFS -> 0 .. 1
            const float db = gainToDb (env);
            const float out = clamp ((db - s.floorDb) / -s.floorDb, 0.0f, 1.0f);
            v[0] = v[1] = out;
        }
        void randomTick (double ppq, bool fire, float* v)
        {
            const int keep = s.shape; s.shape = (s.smoothing == off ? sampleHold : smoothRandom);
            lfoTick (ppq, fire, v);
            s.shape = keep;
        }
        void stepTick (bool fire, float* v)
        {
            const int ns = clamp (s.numSteps, 1, 16);
            if (fire) stepIndex = (stepIndex + 1) % ns;
            v[0] = v[1] = stepIndex < 0 ? 0.0f : s.steps[stepIndex];
        }
        void adsrTick (bool fire, bool rel, float* v)
        {
            auto coef = [this] (float ms) { return (float) (1.0 - std::exp (-1.0 / (std::max (0.1f, ms) * 0.001 * sr))); };
            if (fire) adsrStage = att;
            if (rel && adsrStage != idle) adsrStage = relSt;
            switch (adsrStage)
            {
                case att: adsrLevel += (1.25f - adsrLevel) * coef (s.aMs); if (adsrLevel >= 1.0f) { adsrLevel = 1.0f; adsrStage = dec; } break;
                case dec: adsrLevel += (s.sLevel - adsrLevel) * coef (s.dMs); if (std::abs (adsrLevel - s.sLevel) < 1.0e-4f) adsrStage = sus; break;
                case sus: adsrLevel = s.sLevel; break;
                case relSt: adsrLevel += (0.0f - adsrLevel) * coef (s.rMs); if (adsrLevel < 1.0e-5f) { adsrLevel = 0; adsrStage = idle; } break;
                default: break;
            }
            v[0] = v[1] = adsrLevel;
        }
        float smooth (int c, float x)
        {
            static const float ms[] = { 0.0f, 2.0f, 12.0f, 45.0f, 120.0f };
            const int m = clamp (s.smoothing, 0, (int) numSmoothing - 1);
            if (m == off) { smoothed[c] = x; return x; }
            const float k = (float) (1.0 - std::exp (-1.0 / (ms[m] * 0.001 * sr)));
            if (m == glide)
            {
                // critically damped second-order glide: no overshoot, smooth start and stop
                const float w = k * 4.0f;
                glideVel[c] += w * w * (x - smoothed[c]) - 2.0f * w * glideVel[c];
                smoothed[c] += glideVel[c];
                return smoothed[c];
            }
            smoothed[c] += k * (x - smoothed[c]);
            return smoothed[c];
        }
    };

    //==========================================================================
    // Test signals for the preset audit (all deterministic)
    //==========================================================================
    namespace TestSignals
    {
        struct Rng { uint32_t s = 22222u; float next() { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) & 0xffffff) / 8388607.5f - 1.0f; } };

        /** Pink noise (Kellet's refined filter) at the given RMS dBFS. */
        inline void pink (std::vector<float>& l, std::vector<float>& r, double sr, double seconds, float rmsDb = -18.0f)
        {
            const size_t n = (size_t) (sr * seconds); l.resize (n); r.resize (n);
            for (int ch = 0; ch < 2; ++ch)
            {
                Rng rng; rng.s = 9001u + (uint32_t) ch * 77u;
                float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
                auto& o = ch ? r : l; double e = 0;
                for (size_t i = 0; i < n; ++i)
                {
                    const float w = rng.next();
                    b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
                    b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
                    o[i] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f; b6 = w * 0.115926f;
                    e += (double) o[i] * o[i];
                }
                const float g = dbToGain (rmsDb) / (float) std::sqrt (e / (double) n + 1e-20);
                for (auto& x : o) x *= g;
            }
        }

        /** A two-bar 100 BPM drum loop: kick, snare, closed / open hats, peaks near -6 dBFS. */
        inline void drums (std::vector<float>& l, std::vector<float>& r, double sr, double seconds)
        {
            const size_t n = (size_t) (sr * seconds); l.assign (n, 0.0f); r.assign (n, 0.0f);
            const double beat = 60.0 / 100.0, sixteenth = beat / 4;
            Rng rng;
            auto addKick = [&] (double t0)
            {
                double ph = 0;
                for (size_t i = (size_t) (t0 * sr); i < n && i < (size_t) ((t0 + 0.45) * sr); ++i)
                {
                    const double t = i / sr - t0, f = 48.0 + 110.0 * std::exp (-t * 30.0);
                    ph += 2 * pi * f / sr;
                    const float v = (float) (std::sin (ph) * std::exp (-t * 7.0) * 0.9 + (t < 0.003 ? rng.next() * 0.3 * (1 - t / 0.003) : 0));
                    l[i] += v; r[i] += v;
                }
            };
            auto addSnare = [&] (double t0)
            {
                double ph = 0;
                for (size_t i = (size_t) (t0 * sr); i < n && i < (size_t) ((t0 + 0.3) * sr); ++i)
                {
                    const double t = i / sr - t0; ph += 2 * pi * 185.0 / sr;
                    const float body = (float) (std::sin (ph) * std::exp (-t * 25.0) * 0.45), nz = rng.next() * (float) std::exp (-t * 14.0) * 0.38f;
                    l[i] += body + nz; r[i] += body + nz * 0.92f;
                }
            };
            auto addHat = [&] (double t0, double len, float pan)
            {
                float hp1 = 0, prev = 0;
                for (size_t i = (size_t) (t0 * sr); i < n && i < (size_t) ((t0 + len) * sr); ++i)
                {
                    const double t = i / sr - t0;
                    const float w = rng.next(); hp1 = 0.9f * (hp1 + w - prev); prev = w;
                    const float v = hp1 * (float) std::exp (-t * (len < 0.1 ? 60.0 : 9.0)) * 0.22f;
                    l[i] += v * (1 - pan); r[i] += v * (1 + pan);
                }
            };
            for (int bar = 0; bar < 64; ++bar)
            {
                const double b0 = bar * 4 * beat; if (b0 > seconds) break;
                addKick (b0); addKick (b0 + 2.5 * beat); addSnare (b0 + beat); addSnare (b0 + 3 * beat);
                for (int s16 = 0; s16 < 16; ++s16) if (s16 % 2 == 0) addHat (b0 + s16 * sixteenth, s16 == 14 ? 0.35 : 0.06, s16 % 4 == 0 ? -0.2f : 0.2f);
            }
            float pk = 0; for (size_t i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i])));
            const float g = pk > 0 ? dbToGain (-6.0f) / pk : 1.0f;
            for (size_t i = 0; i < n; ++i) { l[i] *= g; r[i] *= g; }
        }

        /** A sung phrase: glottal source (Rosenberg pulse) with vibrato and jitter, five vowels through four formants,
            breath noise, an 's' consonant and a 't' plosive; mono centred, RMS about -20 dBFS. */
        inline void vocal (std::vector<float>& l, std::vector<float>& r, double sr, double seconds)
        {
            const size_t n = (size_t) (sr * seconds); l.assign (n, 0.0f); r.assign (n, 0.0f);
            struct Vowel { float f[4]; float bw[4]; float g[4]; };
            static const Vowel vowels[] = {
                { { 800, 1150, 2800, 3500 }, { 80, 90, 120, 130 }, { 1.0f, 0.5f, 0.10f, 0.06f } },   // a
                { { 400, 2100, 2800, 3500 }, { 60, 100, 120, 150 }, { 1.0f, 0.25f, 0.18f, 0.08f } },  // e
                { { 280, 2300, 3000, 3600 }, { 50, 90, 100, 120 }, { 1.0f, 0.12f, 0.12f, 0.06f } },   // i
                { { 450, 800, 2830, 3500 },  { 70, 80, 100, 130 }, { 1.0f, 0.60f, 0.05f, 0.03f } },   // o
                { { 325, 700, 2530, 3500 },  { 50, 60, 170, 180 }, { 1.0f, 0.28f, 0.03f, 0.02f } } }; // u
            static const float notes[] = { 57, 59, 60, 62, 64, 62, 60, 59 };   // A3 .. E4 and back
            // Klatt-style cascade of formant resonators (unity gain at DC), as in a real vocal tract
            struct Res { double a = 1, b = 0, c = 0, y1 = 0, y2 = 0;
                         void set (double fs, double f, double bw) { const double r = std::exp (-pi * bw / fs); c = -r * r; b = 2 * r * std::cos (2 * pi * f / fs); a = 1 - b - c; }
                         double process (double x) { const double y = a * x + b * y1 + c * y2; y2 = y1; y1 = y; return y; } };
            Res fl[4]; Biquad sh; Rng rng; double ph = 0, lipPrev = 0; float breathLp = 0;
            sh.set (Biquad::highpass, sr, 5500, 0.7);
            const double noteLen = 0.5;
            for (size_t i = 0; i < n; ++i)
            {
                const double t = i / sr;
                const int ni = (int) (t / noteLen) % 8;
                const double inNote = std::fmod (t, noteLen);
                const double f0 = 440.0 * std::pow (2.0, (notes[ni] - 69) / 12.0) * (1.0 + 0.012 * std::sin (2 * pi * 5.6 * t) * std::min (1.0, inNote / 0.25)) * (1.0 + 0.002 * rng.next());
                ph += f0 / sr; if (ph >= 1) ph -= 1;
                const float p = (float) ph, open = 0.6f;
                float glott = p < open * 0.66f ? 0.5f * (1 - std::cos ((float) pi * p / (open * 0.66f))) : (p < open ? std::cos ((float) pi * 0.5f * (p - open * 0.66f) / (open * 0.34f)) : 0.0f);
                glott = glott - 0.4f;
                if ((i % 64) == 0)
                {
                    const auto& v = vowels[ni % 5];
                    for (int k = 0; k < 4; ++k) fl[k].set (sr, v.f[k], v.bw[k]);
                }
                const float nz = rng.next(); breathLp += 0.3f * (nz - breathLp);
                double src = glott + 0.04f * breathLp;
                for (int k = 0; k < 4; ++k) src = fl[k].process (src);
                float y = (float) (src - lipPrev); lipPrev = src;                 // lip radiation (+6 dB/oct)
                // envelope per note: short attack, small dip between notes
                const float env = (float) (std::min (1.0, inNote / 0.04) * std::min (1.0, (noteLen - inNote) / 0.05));
                y *= env;
                // consonants: an 's' at the start of notes 2 and 6, a 't' at the start of note 4
                if ((ni == 2 || ni == 6) && inNote < 0.08) y += sh.process (nz) * 0.25f * (float) std::sin (pi * inNote / 0.08);
                if (ni == 4 && inNote < 0.012) y += nz * 0.5f * (float) std::exp (-inNote * 400.0);
                l[i] = y; r[i] = y;
            }
            double e = 0; for (size_t i = 0; i < n; ++i) e += (double) l[i] * l[i];
            const float g = dbToGain (-20.0f) / (float) std::sqrt (e / (double) n + 1e-20);
            for (size_t i = 0; i < n; ++i) { l[i] *= g; r[i] = l[i]; }
        }
    }
}
