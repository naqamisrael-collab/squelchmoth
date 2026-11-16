#pragma once
// Apex Series DSP (original code): the building blocks behind Apex Dynamics, Apex Limiter, Apex Multiband and Apex Sub.
//
//  * Biquad / SVF filters, Linkwitz-Riley crossovers with matching all-passes (minimum phase)
//  * TruePeak: 4x polyphase windowed-sinc interpolator (BS.1770 Annex 2 style true-peak)
//  * Loudness: ITU-R BS.1770-4 / EBU R128 meter that never allocates on the audio thread (histogram gating
//    for Integrated and Loudness Range, momentary 400 ms, short-term 3 s)
//  * GainComputer: compressor / limiter / expander / gate / duck / upward curves with soft knee and negative ratios

#include <JuceHeader.h>

namespace Apex
{
    inline float dbToGain (float db)   { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)    { return 20.0f * std::log10 (juce::jmax (1.0e-9f, g)); }

    //==========================================================================
    /** TPT state variable filter with every response. */
    struct Svf
    {
        double ic1 = 0, ic2 = 0; double g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0;
        void set (double sr, double hz, double q)
        {
            g = std::tan (juce::MathConstants<double>::pi * juce::jlimit (5.0, sr * 0.49, hz) / sr);
            k = 1.0 / juce::jmax (0.05, q); a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        inline void tick (double v0, double& lp, double& bp, double& hp)
        {
            const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
        inline double lowpass (double x)  { double l, b, h; tick (x, l, b, h); return l; }
        inline double highpass (double x) { double l, b, h; tick (x, l, b, h); return h; }
        inline double bandpass (double x) { double l, b, h; tick (x, l, b, h); return b * k; }      // unity peak
        inline double notch (double x)    { double l, b, h; tick (x, l, b, h); return l + h; }
        inline double allpass (double x)  { double l, b, h; tick (x, l, b, h); return l + h - k * b; }
        void reset() { ic1 = ic2 = 0; }
    };

    /** RBJ biquad (Direct Form I) for shelves and peaks. */
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void reset() { x1 = x2 = y1 = y2 = 0; }
        inline double process (double x) { const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
        void norm (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
        void peak (double sr, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.49, f) / sr, al = std::sin (w) / (2 * juce::jmax (0.05, q)), c = std::cos (w);
            norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void lowShelf (double sr, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.49, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * juce::jmax (0.05, q)), s = 2 * std::sqrt (A) * al;
            norm (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s), (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
        }
        /** First-order (6 dB/oct) low shelf: 'db' at DC, unity at the top, geometric midpoint at f. */
        void lowShelf1 (double sr, double f, double db)
        {
            const double G = std::pow (10.0, db / 20.0), g = std::sqrt (G), wc = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.45, f);
            const double K = wc / std::tan (wc / (2.0 * sr));
            norm (K + wc * g, -K + wc * g, 0.0, K + wc / g, -K + wc / g, 0.0);
        }
        void highShelf (double sr, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.49, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * juce::jmax (0.05, q)), s = 2 * std::sqrt (A) * al;
            norm (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s), (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
        }
        /** Magnitude (dB) of the current coefficients at hz. */
        double dbAt (double sr, double hz) const
        {
            const std::complex<double> z = std::polar (1.0, juce::MathConstants<double>::twoPi * hz / sr);
            const auto num = b0 + b1 / z + b2 / (z * z), den = 1.0 + a1 / z + a2 / (z * z);
            return 20.0 * std::log10 (juce::jmax (1e-12, std::abs (num / den)));
        }
    };

    /** 4th-order Linkwitz-Riley split (two cascaded Butterworth SVF stages). */
    struct LR4
    {
        Svf lp1, lp2, hp1, hp2, ap;
        void set (double sr, double f) { for (auto* s : { &lp1, &lp2, &hp1, &hp2 }) s->set (sr, f, 0.70710678); ap.set (sr, f, 0.70710678); }
        inline void split (double x, double& lo, double& hi) { lo = lp2.lowpass (lp1.lowpass (x)); hi = hp2.highpass (hp1.highpass (x)); }
        /** The all-pass that matches lo + hi (keeps the other bands in phase). */
        inline double allpass (double x) { return ap.allpass (x); }
        void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); ap.reset(); }
    };

    //==========================================================================
    /** 4x oversampling true-peak detector: 96-tap Kaiser-windowed sinc, 24 taps per phase (within 0.06 dB to 19 kHz at 44.1k). */
    struct TruePeak
    {
        static constexpr int taps = 24, phases = 4;
        float h[phases][taps] {};
        float hist[taps] {}; int w = 0;
        TruePeak()
        {
            const int N = taps * phases; const double beta = 8.0;
            auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
            for (int p = 0; p < phases; ++p)
            {
                double sum = 0;
                for (int k = 0; k < taps; ++k)
                {
                    const int n = k * phases + p;                      // index in the 64-tap prototype
                    const double c = n - (N - 1) * 0.5;
                    const double x = c / phases;
                    const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
                    const double r = 2.0 * n / (N - 1) - 1.0;
                    const double win = bessel0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - r * r))) / bessel0 (beta);
                    h[p][k] = (float) (sinc * win); sum += h[p][k];
                }
                for (int k = 0; k < taps; ++k) h[p][k] = (float) (h[p][k] / sum);   // unity DC gain per phase
            }
        }
        void reset() { std::fill (std::begin (hist), std::end (hist), 0.0f); w = 0; }
        /** Feed one sample; returns the largest absolute value among the 4 interpolated points. */
        inline float process (float x)
        {
            hist[w] = x; w = (w + 1) % taps;
            float m = 0;
            for (int p = 0; p < phases; ++p)
            {
                float s = 0;
                for (int k = 0; k < taps; ++k) s += h[p][k] * hist[(w + k) % taps];
                m = juce::jmax (m, std::abs (s));
            }
            return m;
        }
        static int delay() { return (taps * phases - 1) / (2 * phases); }   // ~7.9 samples
    };

    //==========================================================================
    /** BS.1770-4 loudness without audio-thread allocation. Histogram (0.1 LU bins) gating like libebur128. */
    class Loudness
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            // K-weighting stage 1 (shelf) and stage 2 (high-pass), re-derived for any sample rate
            {
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
                const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
                for (auto& s : shelf) s.norm (Vh + Vb * K / Q + K * K, 2.0 * (K * K - Vh), Vh - Vb * K / Q + K * K, a0, 2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);
            }
            {
                const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (juce::MathConstants<double>::pi * f0 / sr), a0 = 1.0 + K / Q + K * K;
                for (auto& s : hp) s.norm (1.0, -2.0, 1.0, a0, 2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);
            }
            subLen = juce::jmax (1, (int) std::llround (sr * 0.1));
            reset();
        }
        void reset()
        {
            for (auto& s : shelf) s.reset();
            for (auto& s : hp) s.reset();
            acc = 0; accN = 0; subW = 0; subCount = 0;
            std::fill (std::begin (sub), std::end (sub), 0.0);
            std::fill (std::begin (blockHist), std::end (blockHist), 0u);
            std::fill (std::begin (stHist), std::end (stHist), 0u);
            momentary = shortTerm = integrated = -100.0f; lra = 0; stCounter = 0;
        }
        /** Feed one stereo sample frame. */
        inline void process (float l, float r)
        {
            const double kl = hp[0].process (shelf[0].process (l)), kr = hp[1].process (shelf[1].process (r));
            acc += kl * kl + kr * kr;
            if (++accN >= subLen) finishSub();
        }
        float getMomentary() const  { return momentary; }
        float getShortTerm() const  { return shortTerm; }
        float getIntegrated() const { return integrated; }
        float getRange() const      { return lra; }

    private:
        double sr = 48000; int subLen = 4800;
        Biquad shelf[2], hp[2];
        double acc = 0; int accN = 0;
        double sub[30] {}; int subW = 0, subCount = 0, stCounter = 0;
        static constexpr int bins = 1000;                       // -70 .. +30 LUFS at 0.1 LU
        uint32_t blockHist[bins] {}, stHist[bins] {};
        float momentary = -100, shortTerm = -100, integrated = -100, lra = 0;

        static float lufs (double ms) { return ms > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : -100.0f; }
        static int binOf (float l)    { return juce::jlimit (0, bins - 1, (int) std::floor ((l + 70.0f) * 10.0f)); }
        static float levelOf (int b)  { return -70.0f + (b + 0.5f) * 0.1f; }
        static double energyOf (int b) { return std::pow (10.0, (levelOf (b) + 0.691) / 10.0); }

        void finishSub()
        {
            sub[subW] = acc / accN; subW = (subW + 1) % 30; subCount = juce::jmin (30, subCount + 1);
            acc = 0; accN = 0;
            if (subCount >= 4)
            {
                double m = 0; for (int i = 1; i <= 4; ++i) m += sub[(subW - i + 30) % 30];
                momentary = lufs (m / 4.0);
                if (momentary > -70.0f) { ++blockHist[binOf (momentary)]; updateIntegrated(); }
            }
            if (subCount >= 30)
            {
                double s = 0; for (auto v : sub) s += v;
                shortTerm = lufs (s / 30.0);
                // LRA uses short-term values at 10 Hz (EBU Tech 3342)
                if (shortTerm > -70.0f) { ++stHist[binOf (shortTerm)]; if (++stCounter % 10 == 0) updateRange(); }
            }
        }
        void updateIntegrated()
        {
            double e = 0; uint64_t n = 0;
            for (int b = 0; b < bins; ++b) if (blockHist[b]) { e += blockHist[b] * energyOf (b); n += blockHist[b]; }
            if (n == 0) return;
            const float rel = lufs (e / (double) n) - 10.0f;
            double e2 = 0; uint64_t n2 = 0;
            for (int b = binOf (rel); b < bins; ++b) if (blockHist[b] && levelOf (b) > rel) { e2 += blockHist[b] * energyOf (b); n2 += blockHist[b]; }
            integrated = n2 > 0 ? lufs (e2 / (double) n2) : -100.0f;
        }
        void updateRange()
        {
            double e = 0; uint64_t n = 0;
            for (int b = 0; b < bins; ++b) if (stHist[b]) { e += stHist[b] * energyOf (b); n += stHist[b]; }
            if (n == 0) return;
            const float rel = lufs (e / (double) n) - 20.0f;
            uint64_t total = 0; for (int b = binOf (rel); b < bins; ++b) if (levelOf (b) > rel) total += stHist[b];
            if (total < 2) return;
            const uint64_t lo = (uint64_t) std::llround ((double) (total - 1) * 0.10), hi = (uint64_t) std::llround ((double) (total - 1) * 0.95);
            uint64_t c = 0; float vlo = 0, vhi = 0; bool gotLo = false;
            for (int b = binOf (rel); b < bins; ++b)
            {
                if (levelOf (b) <= rel) continue;
                c += stHist[b];
                if (! gotLo && c > lo) { vlo = levelOf (b); gotLo = true; }
                if (c > hi) { vhi = levelOf (b); break; }
            }
            lra = juce::jmax (0.0f, vhi - vlo);
        }
    };

    //==========================================================================
    /** Static curves. Ratio parameter r: 1..20 = 1:1..20:1, 20..21 = towards LMTR (slope 0), 21..41 = negative ratios. */
    struct Curve
    {
        enum Mode { compress = 0, expand, gate, duck, upward };
        static float slopeFromRatio (float r)
        {
            if (r <= 20.0f) return 1.0f / juce::jmax (1.0f, r);
            if (r <= 21.0f) return (21.0f - r) / 20.0f;
            return -(r - 21.0f) / 20.0f;
        }
        static juce::String ratioText (float r, int mode)
        {
            if (mode == expand || mode == gate) return r >= 20.5f ? juce::String ("GATE") : "1:" + juce::String (juce::jmax (1.0f, r), 1);
            if (r <= 20.0f) return juce::String (juce::jmax (1.0f, r), 1) + ":1";
            if (r <= 21.0f) return r >= 20.95f ? juce::String ("LMTR") : juce::String (juce::roundToInt (20.0f / juce::jmax (0.001f, 21.0f - r))) + ":1";
            return "-" + juce::String (20.0f / (r - 21.0f), 1) + ":1";
        }
        /** Gain change (dB) for a detector level x (dB). */
        static float gainDb (int mode, float x, float thr, float r, float knee, float depth)
        {
            const float W = juce::jmax (0.0f, knee);
            switch (mode)
            {
                case expand: case gate:
                {
                    const float E = (mode == gate || r >= 20.5f) ? 1000.0f : juce::jmax (1.0f, r);   // expansion ratio
                    float y;
                    if (x >= thr + W * 0.5f) y = x;
                    else if (W > 0 && x > thr - W * 0.5f) { const float d = x - thr - W * 0.5f; y = x + (E - 1.0f) * d * d / (2.0f * W) * -1.0f; y = juce::jmin (y, x); }
                    else y = thr + (x - thr) * E;
                    return juce::jmax (-std::abs (depth), y - x);
                }
                case duck:
                {
                    // the key above the threshold pulls the signal down by up to 'depth'
                    if (x <= thr - W * 0.5f) return 0.0f;
                    const float t = W > 0 ? juce::jlimit (0.0f, 1.0f, (x - thr + W * 0.5f) / W) : 1.0f;
                    const float over = juce::jmax (0.0f, x - thr) * (1.0f - slopeFromRatio (r));
                    return -juce::jmin (std::abs (depth), juce::jmax (over, std::abs (depth) * t * t));
                }
                case upward:
                {
                    // lifts material below the threshold towards it (slope 1/r), never more than 'depth'
                    const float s = slopeFromRatio (juce::jmin (20.0f, r));
                    if (x >= thr) return 0.0f;
                    return juce::jmin (std::abs (depth), (thr - x) * (1.0f - s));
                }
                default:
                {
                    const float s = slopeFromRatio (r);
                    float y;
                    if (2.0f * (x - thr) < -W) y = x;
                    else if (W > 0 && 2.0f * std::abs (x - thr) <= W) { const float d = x - thr + W * 0.5f; y = x + (s - 1.0f) * d * d / (2.0f * W); }
                    else y = thr + (x - thr) * s;
                    return juce::jmax (-std::abs (depth), y - x);
                }
            }
        }
    };

    /** Level detector: Peak, RMS, Average, Fast and Smart (blends RMS and peak by crest factor). */
    struct Detector
    {
        enum Type { smart = 0, rms, average, peak, fast };
        double sr = 48000; float msState = 0, avState = 0, pkState = 0, fastState = 0;
        float aRms = 0, aAvg = 0, pkRel = 0, fastRel = 0;
        void prepare (double s)
        {
            sr = s;
            aRms = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
            aAvg = (float) (1.0 - std::exp (-1.0 / (0.020 * sr)));
            pkRel = (float) std::exp (-1.0 / (0.050 * sr));
            fastRel = (float) std::exp (-1.0 / (0.005 * sr));
            reset();
        }
        void reset() { msState = avState = pkState = fastState = 0; }
        /** Returns the level in dB. */
        inline float process (float x, int type)
        {
            const float ax = std::abs (x);
            msState += aRms * (x * x - msState);
            avState += aAvg * (ax - avState);
            pkState = juce::jmax (ax, pkState * pkRel);
            fastState = juce::jmax (ax, fastState * fastRel);
            switch (type)
            {
                case rms:     return 10.0f * std::log10 (msState + 1e-20f) + 3.01f;   // sine-referenced
                case average: return gainToDb (avState * 1.5708f);
                case peak:    return gainToDb (pkState);
                case fast:    return gainToDb (fastState);
                default:
                {
                    const float r = 10.0f * std::log10 (msState + 1e-20f) + 3.01f, p = gainToDb (pkState);
                    const float crest = juce::jlimit (0.0f, 1.0f, (p - r - 3.0f) / 12.0f);   // transient material leans to peak
                    return r + (p - r) * (0.3f + 0.6f * crest);
                }
            }
        }
    };

    /** Ballistics in the gain domain (attack while the gain change grows, release with hold). */
    struct Ballistics
    {
        float state = 0, aAtt = 0, aRel = 0; int holdSamples = 0, holdCount = 0;
        void set (double sr, float attMs, float relMs, float holdMs)
        {
            aAtt = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.01f, attMs) * 0.001 * sr)));
            aRel = (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0f, relMs) * 0.001 * sr)));
            holdSamples = (int) (holdMs * 0.001 * sr);
        }
        /** target: gain change in dB (negative = reduction, positive = boost). */
        inline float process (float target)
        {
            const bool engaging = std::abs (target) > std::abs (state) + 1e-6f;
            if (engaging) { state += aAtt * (target - state); holdCount = holdSamples; }
            else if (holdCount > 0) --holdCount;
            else state += aRel * (target - state);
            return state;
        }
    };

    //==========================================================================
    /** Uniformly-partitioned FFT convolution: one input, several FIR filters sharing its spectra.
        Latency = blockSize samples (plus each filter's own delay). Allocation-free after prepare(). */
    class MultiFir
    {
    public:
        void prepare (int blockSize, const std::vector<std::vector<float>>& irs)
        {
            P = blockSize; N = 2 * P;
            int order = 0; while ((1 << order) < N) ++order;
            fft = std::make_unique<juce::dsp::FFT> (order);
            numFilters = (int) irs.size();
            int M = 1; for (auto& h : irs) M = juce::jmax (M, (int) h.size());
            K = (M + P - 1) / P;
            bins = N / 2 + 1;
            spec.assign ((size_t) numFilters, std::vector<float> ((size_t) K * bins * 2, 0.0f));
            std::vector<float> tmp ((size_t) N * 2, 0.0f);
            for (int f = 0; f < numFilters; ++f)
                for (int k = 0; k < K; ++k)
                {
                    std::fill (tmp.begin(), tmp.end(), 0.0f);
                    for (int i = 0; i < P; ++i) { const int idx = k * P + i; if (idx < (int) irs[(size_t) f].size()) tmp[(size_t) i] = irs[(size_t) f][(size_t) idx]; }
                    fft->performRealOnlyForwardTransform (tmp.data(), true);
                    std::copy (tmp.begin(), tmp.begin() + bins * 2, spec[(size_t) f].begin() + (size_t) k * bins * 2);
                }
            fdl.assign ((size_t) K * bins * 2, 0.0f);
            frame.assign ((size_t) N * 2, 0.0f); work.assign ((size_t) N * 2, 0.0f);
            inBlock.assign ((size_t) P, 0.0f); prevBlock.assign ((size_t) P, 0.0f);
            out.assign ((size_t) numFilters, std::vector<float> ((size_t) P, 0.0f));
            head = 0; pos = 0;
        }
        int latency() const { return P; }
        /** Push one sample; y[f] receives each filter's output (delayed by latency()). */
        inline void process (float x, float* y)
        {
            for (int f = 0; f < numFilters; ++f) y[f] = out[(size_t) f][(size_t) pos];
            inBlock[(size_t) pos] = x;
            if (++pos >= P) { pos = 0; runBlock(); }
        }
    private:
        int P = 256, N = 512, K = 1, bins = 257, numFilters = 0, head = 0, pos = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<std::vector<float>> spec, out;
        std::vector<float> fdl, frame, work, inBlock, prevBlock;
        void runBlock()
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            std::copy (prevBlock.begin(), prevBlock.end(), frame.begin());
            std::copy (inBlock.begin(), inBlock.end(), frame.begin() + P);
            std::copy (inBlock.begin(), inBlock.end(), prevBlock.begin());
            fft->performRealOnlyForwardTransform (frame.data(), true);
            head = (head + K - 1) % K;
            std::copy (frame.begin(), frame.begin() + bins * 2, fdl.begin() + (size_t) head * bins * 2);
            for (int f = 0; f < numFilters; ++f)
            {
                std::fill (work.begin(), work.end(), 0.0f);
                for (int k = 0; k < K; ++k)
                {
                    const float* X = fdl.data() + (size_t) ((head + k) % K) * bins * 2;
                    const float* H = spec[(size_t) f].data() + (size_t) k * bins * 2;
                    for (int b = 0; b < bins; ++b)
                    {
                        const float xr = X[2 * b], xi = X[2 * b + 1], hr = H[2 * b], hi = H[2 * b + 1];
                        work[(size_t) (2 * b)] += xr * hr - xi * hi;
                        work[(size_t) (2 * b + 1)] += xr * hi + xi * hr;
                    }
                }
                fft->performRealOnlyInverseTransform (work.data());
                std::copy (work.begin() + P, work.begin() + 2 * P, out[(size_t) f].begin());   // overlap-save: the valid half
            }
        }
    };
}
