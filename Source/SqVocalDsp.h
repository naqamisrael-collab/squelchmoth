#pragma once
// Squelchmoth vocal DSP core (original code) - shared by Moth Tune, Moth Tune FX and Molt.
//
//  PitchTracker  YIN (cumulative-mean-normalised difference) on a decimated copy of the input, with parabolic
//                interpolation, octave-continuity protection, a 3-frame median, an energy gate and a sibilance
//                detector (energy above 4 kHz), so "s", "sh", "t" and breaths are never pitch-corrected.
//  Splice        the Live engine: a period-synchronous reader that repeats or drops whole cycles of the voice
//                (the technique in the public US 5,973,252 patent family) with a short cross-fade - well under 1 ms.
//  Psola         the Studio engine: pitch-synchronous overlap-add with peak-aligned pitch marks. Grains keep the
//                voice's spectral envelope, so formants are preserved, and resampling the grains moves the formants
//                independently (Formant and Throat). Several synthesis streams share one analysis (the harmonies).
//  LpcFormant    formant correction for the Live engine: whitening with the shifted signal's LPC envelope and
//                re-colouring with the (optionally warped) original envelope, via stable lattice filters.

#include <JuceHeader.h>
#include <complex>

namespace SqVocal
{
    using int64 = juce::int64;

    struct VoiceType { const char* name; float lo, hi; };
    inline const std::array<VoiceType, 6>& voiceTypes()
    {
        static const std::array<VoiceType, 6> v {{ { "Soprano", 130.0f, 1400.0f }, { "Alto/Tenor", 85.0f, 1000.0f }, { "Low Male", 58.0f, 620.0f },
                                                   { "Instrument", 45.0f, 1600.0f }, { "Bass Instrument", 28.0f, 480.0f }, { "Auto (wide)", 55.0f, 1400.0f } }};
        return v;
    }

    inline float hzToNote (float hz, float ref = 440.0f)   { return 69.0f + 12.0f * std::log2 (juce::jmax (1.0f, hz) / ref); }
    inline float noteToHz (float n, float ref = 440.0f)    { return ref * std::pow (2.0f, (n - 69.0f) / 12.0f); }

    /** Power-of-two ring of floats addressed by an absolute 64-bit sample index. */
    struct Ring
    {
        std::vector<float> d; int64 mask = 0;
        void prepare (int minSize) { int n = 1; while (n < minSize) n <<= 1; d.assign ((size_t) n, 0.0f); mask = n - 1; }
        void clear() { std::fill (d.begin(), d.end(), 0.0f); }
        float& at (int64 i)              { return d[(size_t) (i & mask)]; }
        float at (int64 i) const         { return d[(size_t) (i & mask)]; }
        float cubic (double pos) const
        {
            const int64 i = (int64) std::floor (pos);
            const float f = (float) (pos - (double) i);
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
    };

    /** Small TPT state-variable filter used for anti-aliasing and band detection. */
    struct Svf
    {
        float ic1 = 0, ic2 = 0, g = 0, k = 1.414f, a1 = 0, a2 = 0, a3 = 0;
        void set (double sr, double hz, float q = 0.707f)
        {
            g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (5.0, sr * 0.49, hz) / sr);
            k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        // returns low, band, high
        inline void tick (float v0, float& lo, float& bp, float& hp)
        {
            const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            lo = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
        inline float lp (float v0) { float l, b, h; tick (v0, l, b, h); return l; }
        inline float hpf (float v0) { float l, b, h; tick (v0, l, b, h); return h; }
        inline float bandpass (float v0) { float l, b, h; tick (v0, l, b, h); return b; }
        inline float bandpassNorm (float v0) { float l, b, h; tick (v0, l, b, h); return b * k; }   // unity gain at the centre
        void reset() { ic1 = ic2 = 0; }
    };

    //==========================================================================
    class PitchTracker
    {
    public:
        struct Frame { int64 pos = 0; float hz = 0, conf = 0, rms = 0; bool voiced = false, sibilant = false; };

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            D = juce::jmax (1, juce::roundToInt (sr / 11025.0));
            fsd = sr / D;
            ring.prepare (8192); full.prepare (8192 * D + 64);
            aa1.set (sr, juce::jmin (3800.0, fsd * 0.42)); aa2.set (sr, juce::jmin (3800.0, fsd * 0.42));
            hi1.set (sr, 4000.0); hi2.set (sr, 4000.0);
            lin.assign (4096, 0.0f); dbuf.assign (2048, 0.0f);
            reset();
            setRange (80.0f, 1000.0f);
        }
        void reset()
        {
            ring.clear(); full.clear(); aa1.reset(); aa2.reset(); hi1.reset(); hi2.reset();
            decW = 0; decCount = 0; hopCount = 0; absPos = 0; eHi = eAll = 0;
            last = {}; prevHz[0] = prevHz[1] = prevHz[2] = 0; lastVoicedHz = 0; voicedRun = 0; unvoicedRun = 10;
        }
        void setRange (float loHz, float hiHz)
        {
            minLag = juce::jmax (2, (int) std::floor (fsd / hiHz));
            maxLag = juce::jmin (900, (int) std::ceil (fsd / loHz) + 1);
            win = juce::jmax (maxLag, (int) (fsd * 0.012));
        }
        /** 0..100 like a 'Tracking' knob: higher = more tolerant of breathy / noisy voices. */
        void setTracking (float t)   { threshold = juce::jmap (juce::jlimit (0.0f, 100.0f, t), 0.0f, 100.0f, 0.07f, 0.32f); }
        void setGateDb (float db)     { gate = juce::Decibels::decibelsToGain (db); }

        /** Push one input sample; returns true when a new analysis frame is ready (see latest()). */
        bool push (float x)
        {
            ++absPos;
            const float h = hi2.hpf (hi1.hpf (x));
            eHi += h * h; eAll += x * x;
            const float y = aa2.lp (aa1.lp (x));
            full.at (absPos) = y;
            if (++decCount < D) return false;
            decCount = 0;
            ring.at (decW++) = y;
            if (++hopCount < hop) return false;
            hopCount = 0;
            analyse();
            return true;
        }
        const Frame& latest() const   { return last; }
        int64 position() const        { return absPos; }
        double frameRate() const      { return fsd / hop; }
        /** How far (in input samples) a frame's centre lags the newest input. */
        int lagSamples() const        { return (win / 2 + maxLag / 4) * D; }

    private:
        double sr = 44100, fsd = 11025;
        int D = 4, hop = 24, minLag = 10, maxLag = 200, win = 200;
        float threshold = 0.18f, gate = 0.0025f;
        Ring ring, full; int64 decW = 0; int decCount = 0, hopCount = 0; int64 absPos = 0;
        Svf aa1, aa2, hi1, hi2;
        double eHi = 0, eAll = 0;
        std::vector<float> lin, dbuf;
        Frame last;
        float prevHz[3] {}; float lastVoicedHz = 0; int voicedRun = 0, unvoicedRun = 10;

        /** Re-measures the period at full sample rate around the decimated estimate: removes the decimation bias. */
        float refine (double lagEstimate, int w)
        {
            const int c = (int) std::llround (lagEstimate);
            const int r = D + 1;
            double best = 1e300; int bi = c; double vals[64]; const int span = juce::jmin (31, r);
            for (int k = -span; k <= span; ++k)
            {
                const int tau = c + k; if (tau < 2) { vals[k + span] = 1e300; continue; }
                double sum = 0;
                for (int j = 0; j < w; j += 1) { const int64 i = absPos - 1 - j; const double dd = full.at (i) - full.at (i - tau); sum += dd * dd; }
                vals[k + span] = sum;
                if (sum < best) { best = sum; bi = tau; }
            }
            double t = bi;
            const int ii = bi - c + span;
            if (ii > 0 && ii < 2 * span)
            {
                const double a = vals[ii - 1], b = vals[ii], cc = vals[ii + 1], den = a + cc - 2 * b;
                if (std::abs (den) > 1e-20) t = bi + 0.5 * (a - cc) / den;
            }
            return (float) (sr / t);
        }

        void analyse()
        {
            Frame f;
            f.pos = absPos - (int64) lagSamples();
            const double sib = eAll > 1e-12 ? eHi / eAll : 0.0;
            f.sibilant = sib > 0.35;
            eHi = eAll = 0;
            const int N = win + maxLag + 2;
            if (decW < N) { last = f; return; }
            for (int i = 0; i < N; ++i) lin[(size_t) i] = ring.at (decW - N + i);
            const float* x = lin.data() + (N - win);   // newest 'win' samples; lag looks back
            double e = 0; for (int j = 0; j < win; ++j) e += (double) x[j] * x[j];
            f.rms = (float) std::sqrt (e / win);
            // difference function
            dbuf[0] = 0;
            for (int tau = 1; tau <= maxLag; ++tau)
            {
                const float* xs = x - tau;
                float s = 0;
                for (int j = 0; j < win; ++j) { const float dd = x[j] - xs[j]; s += dd * dd; }
                dbuf[(size_t) tau] = s;
            }
            // cumulative mean normalised difference
            double run = 0;
            dbuf[0] = 1.0f;
            for (int tau = 1; tau <= maxLag; ++tau)
            {
                run += dbuf[(size_t) tau];
                dbuf[(size_t) tau] = run > 0 ? (float) (dbuf[(size_t) tau] * tau / run) : 1.0f;
            }
            int best = -1;
            for (int tau = minLag; tau < maxLag; ++tau)
                if (dbuf[(size_t) tau] < threshold)
                {
                    while (tau + 1 < maxLag && dbuf[(size_t) tau + 1] < dbuf[(size_t) tau]) ++tau;
                    best = tau; break;
                }
            bool confident = best > 0;
            if (best < 0)
            {
                float m = 2.0f;
                for (int tau = minLag; tau < maxLag; ++tau) if (dbuf[(size_t) tau] < m) { m = dbuf[(size_t) tau]; best = tau; }
            }
            if (best <= 0) { last = f; return; }
            // octave continuity: prefer the candidate that continues the previous pitch when it is nearly as good
            if (lastVoicedHz > 0 && unvoicedRun < 6)
            {
                const double prevLag = fsd / lastVoicedHz;
                for (double mult : { 2.0, 0.5, 3.0 })
                {
                    const int alt = juce::roundToInt (best * mult);
                    if (alt < minLag || alt >= maxLag) continue;
                    int a = alt;   // settle on the local minimum near alt
                    for (int k = juce::jmax (minLag, alt - 3); k <= juce::jmin (maxLag - 1, alt + 3); ++k) if (dbuf[(size_t) k] < dbuf[(size_t) a]) a = k;
                    if (std::abs (a - prevLag) < prevLag * 0.12 && std::abs (best - prevLag) > prevLag * 0.3 && dbuf[(size_t) a] < dbuf[(size_t) best] * 1.6f + 0.03f)
                        { best = a; break; }
                }
            }
            // parabolic interpolation
            double tauF = best;
            if (best > 1 && best < maxLag - 1)
            {
                const double s0 = dbuf[(size_t) best - 1], s1 = dbuf[(size_t) best], s2 = dbuf[(size_t) best + 1];
                const double den = s0 + s2 - 2 * s1;
                if (std::abs (den) > 1e-12) tauF = best + 0.5 * (s0 - s2) / den;
            }
            const float cm = dbuf[(size_t) best];
            f.conf = juce::jlimit (0.0f, 1.0f, 1.0f - cm);
            float hz = (float) (fsd / tauF);
            if (D > 1) hz = refine (tauF * D, win * D);
            const bool candidate = (confident || cm < threshold * 1.4f) && f.rms > gate && ! f.sibilant;
            if (candidate) { ++voicedRun; unvoicedRun = 0; } else { voicedRun = 0; ++unvoicedRun; }
            f.voiced = candidate && voicedRun >= 2;
            if (candidate)
            {
                prevHz[2] = prevHz[1]; prevHz[1] = prevHz[0]; prevHz[0] = hz;
                if (prevHz[2] > 0)   // 3-point median kills single-frame outliers
                {
                    float a = prevHz[0], b = prevHz[1], c = prevHz[2];
                    hz = juce::jmax (juce::jmin (a, b), juce::jmin (juce::jmax (a, b), c));
                }
                lastVoicedHz = hz;
            }
            else prevHz[0] = prevHz[1] = prevHz[2] = 0;
            f.hz = hz;
            last = f;
        }
    };

    //==========================================================================
    /** Live engine reader: plays the delayed input at 'rate' and keeps itself inside a small window by jumping
        exactly one detected cycle back (repeat) or forward (drop), cross-fading over a fraction of a cycle. */
    struct Splice
    {
        double rp = 0, rpOld = 0; int fade = 0, fadeLen = 1;
        void reset (int64 newest, int dmin) { rp = (double) (newest - dmin); fade = 0; }
        /** Advance; newest = index of the newest written input sample. Returns false if nothing to read. */
        inline void advance (int64 newest, double rate, double period, int dmin, double sr)
        {
            rp += rate; if (fade > 0) rpOld += rate;
            const double d = (double) newest - rp;
            const int fl = (int) juce::jlimit (8.0, 0.0012 * sr, period * 0.25);
            const double th = dmin + juce::jmax (0.0, rate - 1.0) * fl;
            if (fade == 0)
            {
                if (d < th)
                {
                    const double jumps = std::ceil ((th - d) / period);
                    rpOld = rp; rp -= jumps * period; fade = fadeLen = fl;
                }
                else if (d > th + period + 2.0)
                {
                    const double jumps = std::floor ((d - th) / period);
                    rpOld = rp; rp += jumps * period; fade = fadeLen = fl;
                }
            }
            // never read ahead of the input, whatever happens
            const double lim = (double) newest - 3.0;
            if (rp > lim) rp = lim;
            if (rpOld > lim) rpOld = lim;
        }
        /** In silence or consonants (rate 1), drift back to the nominal delay with an inaudible cross-fade. */
        inline void recentre (int64 newest, int dmin, double sr)
        {
            if (fade > 0) return;
            const double d = (double) newest - rp;
            if (std::abs (d - dmin) > 0.5) { rpOld = rp; rp = (double) (newest - dmin); fade = fadeLen = (int) (0.004 * sr); }
        }
        inline float read (const Ring& in) const
        {
            if (fade <= 0) return in.cubic (rp);
            const float t = 1.0f - (float) fade / (float) fadeLen;
            return in.cubic (rpOld) * (1.0f - t) + in.cubic (rp) * t;
        }
        inline void endSample() { if (fade > 0) --fade; }
    };

    //==========================================================================
    /** Linear-prediction envelope (reflection coefficients) of a frame. */
    struct Lpc
    {
        static constexpr int maxOrder = 32;
        static inline double whitenLagHz = 40.0, targetLagHz = 40.0; static inline int whitenOrderExtra = 0;
        static bool analyse (const float* x, int n, int order, double sr, float* k, float& err, std::vector<float>& tmp, double lagHz = 60.0)
        {
            tmp.resize ((size_t) n);
            float prev = 0;
            for (int i = 0; i < n; ++i)
            {
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1));
                const float pe = x[i] - 0.9f * prev; prev = x[i];   // pre-emphasis for a better envelope fit
                tmp[(size_t) i] = pe * w;
            }
            double r[maxOrder + 1];
            for (int lag = 0; lag <= order; ++lag)
            {
                double s = 0;
                for (int i = lag; i < n; ++i) s += (double) tmp[(size_t) i] * tmp[(size_t) (i - lag)];
                const double bw = 2.0 * juce::MathConstants<double>::pi * lagHz * lag / sr;   // lag window: smooths away individual harmonics
                r[lag] = s * std::exp (-0.5 * bw * bw);
            }
            r[0] *= 1.0001; r[0] += 1e-9;
            if (r[0] < 1e-8) { std::fill (k, k + order, 0.0f); err = 0; return false; }
            double a[maxOrder + 1] {}, tmpA[maxOrder + 1] {};
            a[0] = 1; double e = r[0];
            for (int m = 1; m <= order; ++m)
            {
                double acc = r[m];
                for (int i = 1; i < m; ++i) acc += a[i] * r[m - i];
                double km = -acc / e;
                km = juce::jlimit (-0.995, 0.995, km);
                for (int i = 0; i <= m; ++i) tmpA[i] = a[i];
                for (int i = 1; i < m; ++i) a[i] = tmpA[i] + km * tmpA[m - i];
                a[m] = km;
                e *= (1.0 - km * km);
                k[m - 1] = (float) km;
            }
            err = (float) e;
            return true;
        }
    };

    /** Lattice filters for LPC (stable with interpolated reflection coefficients). */
    struct Lattice
    {
        float b[Lpc::maxOrder + 1] {};
        void reset() { std::fill (std::begin (b), std::end (b), 0.0f); }
        // all-zero whitening:  e = A(z) x
        inline float analysis (float x, const float* k, int p)
        {
            float f = x, bPrev = x;
            for (int m = 0; m < p; ++m)
            {
                const float bm1 = b[m];
                const float fn = f + k[m] * bm1;
                const float bn = k[m] * f + bm1;
                b[m] = bPrev; bPrev = bn; f = fn;
            }
            return f;
        }
        // all-pole colouring:  y = e / A(z)
        inline float synthesis (float e, const float* k, int p)
        {
            float f = e;
            for (int m = p - 1; m >= 0; --m)
            {
                f = f - k[m] * b[m];
                b[m + 1] = k[m] * f + b[m];
            }
            b[0] = f;
            return juce::jlimit (-8.0f, 8.0f, f);
        }
    };

    /** Moves the spectral envelope of 'y' (a shifted copy of 'x') back to the envelope of x warped by 'warp'. */
    class LpcFormant
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            order = juce::jlimit (14, 24, juce::roundToInt (sr / 2400.0) + 4);
            orderOut = juce::jmin (Lpc::maxOrder, order + Lpc::whitenOrderExtra);
            frame = juce::jmax (512, juce::roundToInt (sr * 0.02));
            hopLen = juce::jmax (64, juce::roundToInt (sr * 0.004));
            eA = (float) std::exp (-1.0 / (0.005 * sr)); gK = (float) (1.0 - std::exp (-1.0 / (0.002 * sr))); pkR = (float) std::exp (-1.0 / (0.040 * sr));
            yHist.prepare (frame * 2 + 8);
            fx.assign ((size_t) frame, 0.0f); fy.assign ((size_t) frame, 0.0f);
            reset();
        }
        void reset()
        {
            yHist.clear(); an.reset(); sy.reset(); count = 0; pos = 0;
            std::fill (std::begin (kIn), std::end (kIn), 0.0f); std::fill (std::begin (kOut), std::end (kOut), 0.0f);
            std::fill (std::begin (kInCur), std::end (kInCur), 0.0f); std::fill (std::begin (kOutCur), std::end (kOutCur), 0.0f);
            gain = 1; gainTarget = 1; yE = zE = 0; yPk = 0;
        }
        /** x: the original input ring (newest index 'newest'); y: the shifted sample to correct. */
        float process (const Ring& xRing, int64 newest, float y, float warp)
        {
            yHist.at (pos++) = y;
            if (++count >= hopLen)
            {
                count = 0;
                // original's envelope, read through the warp (resampling the frame moves its formants by 'warp')
                const double span = frame * warp;
                for (int i = 0; i < frame; ++i) fx[(size_t) i] = xRing.cubic ((double) newest - span + i * warp);
                for (int i = 0; i < frame; ++i) fy[(size_t) i] = yHist.at (pos - frame + i);
                float eIn = 0, eOut = 0;
                const bool okIn = Lpc::analyse (fx.data(), frame, order, sr, kIn, eIn, tmp, Lpc::targetLagHz);
                const bool okOut = Lpc::analyse (fy.data(), frame, orderOut, sr, kOut, eOut, tmp, Lpc::whitenLagHz);
                if (! okIn || ! okOut) { std::fill (std::begin (kIn), std::end (kIn), 0.0f); std::fill (std::begin (kOut), std::end (kOut), 0.0f); }
                for (int m = 0; m < order; ++m) dIn[m] = (kIn[m] - kInCur[m]) / (float) hopLen;
                for (int m = 0; m < orderOut; ++m) dOut[m] = (kOut[m] - kOutCur[m]) / (float) hopLen;
            }
            for (int m = 0; m < order; ++m) kInCur[m] += dIn[m];
            for (int m = 0; m < orderOut; ++m) kOutCur[m] += dOut[m];
            const float e = an.analysis (y, kOutCur, orderOut);
            float z = sy.synthesis (e, kInCur, order);
            // keep the loudness of y (over ~5 ms), and never let the re-coloured signal peak far above y's own peaks
            yE = yE * eA + y * y * (1.0f - eA); zE = zE * eA + z * z * (1.0f - eA);
            gainTarget = zE > 1e-12f ? juce::jlimit (0.25f, 2.0f, std::sqrt (yE / zE)) : 1.0f;
            gain += (gainTarget - gain) * gK;
            const float ay = std::abs (y); yPk = ay > yPk ? ay : yPk * pkR;
            const float lim = 2.0f * yPk + 1.0e-4f;
            return juce::jlimit (-lim, lim, z * gain);
        }
    private:
        double sr = 44100; int order = 20, orderOut = 28, frame = 1024, hopLen = 64, count = 0; int64 pos = 0;
        Ring yHist; std::vector<float> fx, fy, tmp;
        float kIn[Lpc::maxOrder] {}, kOut[Lpc::maxOrder] {}, kInCur[Lpc::maxOrder] {}, kOutCur[Lpc::maxOrder] {}, dIn[Lpc::maxOrder] {}, dOut[Lpc::maxOrder] {};
        Lattice an, sy;
        float gain = 1, gainTarget = 1, yE = 0, zE = 0, yPk = 0, eA = 0.995f, gK = 0.01f, pkR = 0.9995f;
    };

    //==========================================================================
    /** Studio engine: pitch-synchronous overlap-add with shared analysis and several synthesis streams. */
    class Psola
    {
    public:
        struct Mark { int64 pos = 0; float period = 0; bool voiced = false; };
        struct Grain { float ratio = 1, formant = 1, gain = 1; bool silent = false, monoSource = false; };
        static constexpr int maxStreams = 5, maxCh = 2;

        void prepare (double sampleRate, int numChannels, int latencySamples, float maxPeriodSamples)
        {
            sr = sampleRate; nch = juce::jlimit (1, maxCh, numChannels);
            L = latencySamples; Tmax = maxPeriodSamples;
            const int ringSize = juce::jmax (8192, (int) (L + 4 * Tmax + 4096));
            for (int c = 0; c < maxCh; ++c) in[c].prepare (ringSize);
            mono.prepare (ringSize);
            for (auto& s : streams) for (int c = 0; c < maxCh; ++c) s.acc[c].prepare (ringSize);
            marks.assign (2048, {});
            Tu = (float) juce::roundToInt (sr * 0.0045);
            reset();
        }
        void reset()
        {
            for (int c = 0; c < maxCh; ++c) in[c].clear();
            mono.clear();
            for (auto& s : streams) { for (int c = 0; c < maxCh; ++c) s.acc[c].clear(); s.ts = 0; s.started = false; s.unity = 0; }
            for (auto& r : readPos) r = 0;
            markW = 0; lastMarkPos = 0; inPos = 0; lp1 = lp2 = 0; lpState.clear();
        }
        int latency() const { return L; }
        const Ring& input (int ch) const { return in[juce::jmin (ch, nch - 1)]; }
        const Ring& monoInput() const    { return mono; }
        int64 newest() const             { return inPos - 1; }

        /** Write one input frame. 'period' and 'voiced' describe the pitch near the newest input. */
        void pushInput (const float* frame, int ch, float period, bool voiced, float f0)
        {
            float m = 0;
            for (int c = 0; c < nch; ++c) { const float v = frame[juce::jmin (c, ch - 1)]; in[c].at (inPos) = v; m += v; }
            m /= (float) nch;
            mono.at (inPos) = m;
            // low-passed copy for peak-aligned pitch marks
            const float a = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * juce::jlimit (60.0, 900.0, f0 > 0 ? f0 * 1.4 : 400.0) / sr));
            lp1 += a * (m - lp1); lp2 += a * (lp1 - lp2);
            lpState.push_back (lp2);
            ++inPos;
            placeMarks (period, voiced);
        }

        /** Make sure every grain overlapping output index 'n' has been added, then read stream outputs. */
        template <typename GrainFn>
        void synthesiseUpTo (int64 n, GrainFn&& grainFor, int numStreams)
        {
            for (int s = 0; s < numStreams; ++s)
            {
                auto& st = streams[(size_t) s];
                if (! st.started) { st.ts = (double) (n - 1); st.started = true; }
                int guard = 0;
                while (st.ts <= (double) n + 1.3 * Tmax && guard++ < 64)
                {
                    const double ta = st.ts - L;
                    const int64 mi = nearestMark (ta);
                    if (mi < 0) { st.ts += Tu; continue; }
                    const auto& mk = markAt (mi);
                    const auto spacing = localSpacing (mi);
                    Grain gr = grainFor (s, mk);
                    if (! mk.voiced) gr.ratio = 1.0f;
                    // with no pitch change, lock the synthesis mark to the analysis mark so the output stays
                    // sample-aligned with the input (clean dry/wet mixing, untouched consonants)
                    // sample-aligned with the input (clean dry/wet mixing, untouched consonants). Only after the ratio has
                    // stayed at 1 for a few periods, and by gliding (3 % of a period per grain): a correction that merely
                    // passes through 1 - every vibrato cycle when vibrato is cut or hard-tuned - must never make the
                    // synthesis time jump, which would leave a gap in the output.
                    if (std::abs (gr.ratio - 1.0f) < 0.0015f && std::abs (gr.formant - 1.0f) < 0.0015f)
                    {
                        if (++st.unity >= 6)
                        {
                            const double want = juce::jmax ((double) readPos[s], (double) mk.pos + L), d = want - st.ts;
                            st.ts += std::abs (d) < 1.0 || st.unity == 6 && std::abs (d) < 0.03 * spacing ? d : juce::jlimit (-0.03 * spacing, 0.03 * spacing, d);
                        }
                    }
                    else st.unity = 0;
                    const float hop = mk.voiced ? spacing / juce::jlimit (0.25f, 4.0f, gr.ratio) : spacing;
                    if (! gr.silent) addGrain (st, mk, spacing, gr, readPos[s], hop);
                    st.ts += hop;
                }
            }
        }
        float readAndClear (int stream, int ch, int64 n)
        {
            readPos[stream] = n + 1;
            auto& r = streams[(size_t) stream].acc[ch];
            const float v = r.at (n); r.at (n) = 0.0f; return v;
        }
        void restartStream (int s) { streams[(size_t) s].started = false; }

    private:
        struct Stream { double ts = 0; bool started = false; int unity = 0; Ring acc[maxCh]; };
        double sr = 44100; int nch = 1, L = 1024; float Tmax = 800, Tu = 200;
        Ring in[maxCh], mono;
        std::array<Stream, maxStreams> streams;
        std::vector<Mark> marks; int64 markW = 0; int64 lastMarkPos = 0; int64 inPos = 0;
        int64 readPos[maxStreams] {};
        float lp1 = 0, lp2 = 0;
        struct LpRing { std::vector<float> d; int64 w = 0; void clear() { d.assign (1 << 15, 0.0f); w = 0; } void push_back (float v) { if (d.empty()) clear(); d[(size_t) (w++ & ((int64) d.size() - 1))] = v; } float at (int64 i) const { return d[(size_t) (i & ((int64) d.size() - 1))]; } size_t size() const { return (size_t) w; } } lpState;

        const Mark& markAt (int64 i) const { return marks[(size_t) (i & (int64) (marks.size() - 1))]; }

        void placeMarks (float period, bool voiced)
        {
            for (int guard = 0; guard < 4; ++guard)
            {
                const float T = voiced ? juce::jlimit (8.0f, Tmax, period) : Tu;
                const double cand = (double) lastMarkPos + T;
                const int64 need = (int64) (cand + (voiced ? T * 0.3 : 0.0)) + 2;
                if (need >= inPos) return;
                int64 pos = (int64) std::llround (cand);
                if (voiced)
                {
                    const int64 lo = (int64) std::llround (cand - T * 0.25), hi = (int64) std::llround (cand + T * 0.25);
                    float best = -1e9f;
                    for (int64 i = juce::jmax (lastMarkPos + (int64) (T * 0.6f), lo); i <= hi; ++i)
                        if (lpState.at (i) > best) { best = lpState.at (i); pos = i; }
                }
                marks[(size_t) (markW & (int64) (marks.size() - 1))] = { pos, T, voiced };
                ++markW;
                lastMarkPos = pos;
            }
        }
        int64 nearestMark (double ta) const
        {
            if (markW == 0) return -1;
            const int64 first = juce::jmax ((int64) 0, markW - (int64) marks.size() + 2);
            int64 best = -1; double bd = 1e18;
            for (int64 i = markW - 1; i >= first; --i)
            {
                const auto& m = markAt (i);
                const double d = std::abs ((double) m.pos - ta);
                if (d < bd) { bd = d; best = i; }
                if ((double) m.pos < ta - 2 * Tmax) break;
            }
            // the grain must be fully readable
            while (best >= first && (double) markAt (best).pos + 1.3 * markAt (best).period + 3 >= (double) inPos) --best;
            return best < first ? -1 : best;
        }
        float localSpacing (int64 i) const
        {
            const auto& m = markAt (i);
            if (i + 1 < markW && i > 0)
            {
                const auto& a = markAt (i - 1); const auto& b = markAt (i + 1);
                if (b.pos > m.pos && m.pos > a.pos) return juce::jlimit (4.0f, Tmax * 1.5f, 0.5f * (float) (b.pos - a.pos));
            }
            return m.period;
        }
        /** Power of a train of Hann grains (half-length 1) spaced x apart, relative to x = 1 (where they sum to exactly 1). */
        static float olaPower (float x)
        {
            static const std::array<float, 129> table = []
            {
                std::array<float, 129> t {};
                for (int i = 0; i <= 128; ++i)
                {
                    const double H = 1.0 + 3.0 * i / 128.0; double acc = 0; const int n = 400;
                    for (int j = 0; j < n; ++j)
                    {
                        const double tt = H * j / n; double sum = 0;
                        for (int k = -3; k <= 3; ++k) { const double d = tt - k * H; if (std::abs (d) < 1.0) sum += 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * d); }
                        acc += sum * sum;
                    }
                    t[(size_t) i] = (float) (acc / n);
                }
                return t;
            }();
            const float f = juce::jlimit (0.0f, 128.0f, (x - 1.0f) / 3.0f * 128.0f); const int i = (int) f; const float fr = f - (float) i;
            return i >= 128 ? table[128] : table[(size_t) i] + fr * (table[(size_t) i + 1] - table[(size_t) i]);
        }
        void addGrain (Stream& st, const Mark& mk, float T, const Grain& gr, int64 firstWritable, float hop)
        {
            const float g = juce::jlimit (0.4f, 2.5f, gr.formant);
            const double Lo = juce::jlimit ((double) T * 0.5, (double) T * 1.25, (double) T / g);
            // lowering the pitch spreads the grains further apart than they are long: the gaps between them would lose
            // up to 5 dB at an octave down - restore the power the overlap-add loses
            const float gainOla = hop > Lo * 1.001 ? 1.0f / std::sqrt (juce::jmax (0.05f, olaPower ((float) (hop / Lo)))) : 1.0f;
            const int64 k0 = juce::jmax (firstWritable, (int64) std::ceil (st.ts - Lo)), k1 = (int64) std::floor (st.ts + Lo);
            const double inv = 1.0 / Lo;
            for (int64 k = k0; k <= k1; ++k)
            {
                const double u = ((double) k - st.ts) * inv;
                const float w = 0.5f + 0.5f * (float) std::cos (juce::MathConstants<double>::pi * u);
                const double ip = (double) mk.pos + ((double) k - st.ts) * g;
                if (gr.monoSource) st.acc[0].at (k) += w * gr.gain * gainOla * mono.cubic (ip);
                else for (int c = 0; c < nch; ++c) st.acc[c].at (k) += w * gr.gain * gainOla * in[c].cubic (ip);
            }
        }
    };
}

namespace SqVocal
{
    //==========================================================================
    struct GraphNote { double start = 0, end = 0; float orig = 60, pitch = 60, retuneMs = 20, vibScale = 1, formantSt = 0, drift = 1; };
    struct TimedNote { double start = 0, end = 0; int pitch = 60; };

    struct Harm { int mode = 0; float interval = 0, gain = 0, pan = 0, formantSt = 0; };
    struct TuneSettings
    {
        float retuneMs = 20, humanize = 0, flex = 0, vibKeep = 1, natVib = 0, strength = 1, transpose = 0, formantSt = 0, throat = 1, ref = 440;
        float predictSec = 0;   // Live engine: how far the detector lags the audio being played
        float motionSteps = 0;  // Moth Tune FX motion pattern: offset of the target (scale steps or semitones)
        bool motionDiatonic = true; float motionGlideMs = 0;
        float vibRate = 5.5f, vibDepth = 0, vibDelay = 0.3f, vibOnset = 0.4f, vibVar = 0.2f; int vibShape = 0;
        bool classic = false, conform = false, preserve = true, midiTargets = false, allOctaves = true, midiPassThrough = true;
        int allowedMask = 0xfff, bypassMask = 0, keyRoot = 0;
        std::array<Harm, 4> harm {};
        float harmDetune = 6, harmTimingMs = 12;
        // ---- Moth Tune 2: dynamic vibrato (0 off = Vibrato Keep as before, 1 Preserve, 2 Cut, 3 Boost)
        int dvMode = 0;
        float dvThresholdCents = 15, dvDelay = 0.1f, dvAttack = 0.15f, dvVariation = 0;   // detection and how it fades in
        float dvPitch = 1, dvAmp = 0, dvTimbre = 0;                                         // separate depths, 0..1 (Boost up to 2)
        bool vibFxStraightOnly = false;   // created vibrato only where the singer holds a straight note
        float harmGate = 1;               // harmony trigger (MIDI / CC / latch), smoothed in the processor
    };

    struct FrameResult
    {
        bool voiced = false, hOn[4] {};
        float inNote = -1, outNote = -1, target = -1, ratio = 1, formant = 1, corrCents = 0;
        float hRatio[4] { 1, 1, 1, 1 }, hFormant[4] { 1, 1, 1, 1 }, hNote[4] { -1, -1, -1, -1 };
        float gain = 1;                                        // lead amplitude (dynamic vibrato's amplitude depth)
        float vibRateHz = 0, vibDepthCents = 0, vibAmount = 0; // the vibrato visualiser: measured rate / depth, how much is being acted on
    };

    inline float shapeLfo (int shape, float ph)   // ph 0..1
    {
        switch (shape)
        {
            case 1:  return 1.0f - 4.0f * std::abs (ph - 0.5f);                              // triangle
            case 2:  return ph < 0.5f ? 1.0f : -1.0f;                                        // square
            case 3:  return 2.0f * ph - 1.0f;                                                // saw
            default: return std::sin (juce::MathConstants<float>::twoPi * ph);               // sine
        }
    }

    /** Pitch-class mask helpers. */
    inline bool pcIn (int mask, int note)   { return (mask >> (((note % 12) + 12) % 12)) & 1; }
    inline int nearestInMask (float p, int mask)
    {
        if ((mask & 0xfff) == 0) return juce::roundToInt (p);
        const int c = juce::roundToInt (p);
        int best = c; float bd = 1e9f;
        for (int d = -12; d <= 12; ++d) { const int n = c + d; if (pcIn (mask, n) && std::abs ((float) n - p) < bd) { bd = std::abs ((float) n - p); best = n; } }
        return best;
    }
    inline int stepInMask (int note, int steps, int mask)
    {
        if ((mask & 0xfff) == 0) return note + steps;
        int n = nearestInMask ((float) note, mask);
        const int dir = steps > 0 ? 1 : -1;
        for (int k = 0; k < std::abs (steps); ++k) { n += dir; int g = 0; while (! pcIn (mask, n) && g++ < 12) n += dir; }
        return n;
    }

    //==========================================================================
    /** The correction brain: turns a detected pitch frame into output ratios for the lead and the harmonies. */
    class TuneBrain
    {
    public:
        void prepare (double frameRate) { fr = frameRate; reset(); }
        void reset()
        {
            c = 0; cv = 0; haveCentre = false; centre1 = centre2 = 0; target = -1000; noteTime = 0; vibPhase = 0; vibAmp = 0;
            for (auto& d : drift) d = 0;
            for (auto& d : driftTarget) d = 0;
            rateNoise = depthNoise = 0;
            vibEnv = 0; vibOnT = 0; dvAmt = 0; rmsFast = rmsSlow1 = rmsSlow2 = 0; dvNoise = 0;
        }

        FrameResult process (const PitchTracker::Frame& f, const TuneSettings& s, const std::array<bool, 128>& held, const GraphNote* gn)
        {
            FrameResult r;
            const float dt = (float) (1.0 / fr);
            if (! f.voiced || f.hz <= 0)
            {
                c *= std::exp (-dt / 0.03f); cv = 0;
                haveCentre = false; target = -1000; noteTime = 0; vibAmp = 0;
                vibEnv = 0; vibOnT = 0; dvAmt *= 0.9f; rmsSlow1 = rmsSlow2 = 0;
                r.voiced = false; r.ratio = 1.0f; r.formant = formantFor (s, 1.0f);
                for (int i = 0; i < 4; ++i) { r.hOn[i] = s.harm[(size_t) i].mode != 0; r.hRatio[i] = 1.0f; r.hFormant[i] = 1.0f; }
                return r;
            }
            const float nRaw = hzToNote (f.hz, s.ref);
            // Live engine: the detector sees the voice a few ms late - extrapolate along the pitch slope
            if (haveCentre) { const float inst = (nRaw - prevIn) * (float) fr; slope += (juce::jlimit (-60.0f, 60.0f, inst) - slope) * 0.3f; }
            else slope = 0;
            prevIn = nRaw;
            const float nIn = nRaw + juce::jlimit (-0.7f, 0.7f, slope * s.predictSec);
            const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * centreHz * dt);
            if (! haveCentre) { centre1 = centre2 = nIn; haveCentre = true; vibZcT = 0; vibHz = 5.5f; lastVibSign = 0; }
            else { centre1 += a * (nIn - centre1); centre2 += a * (centre1 - centre2); }
            const float vibRaw = nIn - centre2;
            // measure the vibrato rate (zero crossings) and undo the centre filter's leakage so kept vibrato keeps its true width
            vibZcT += dt;
            const int sgn = vibRaw > 0.03f ? 1 : vibRaw < -0.03f ? -1 : 0;
            if (sgn != 0 && sgn != lastVibSign)
            {
                if (lastVibSign != 0 && vibZcT > 0.04f && vibZcT < 0.25f) vibHz += (juce::jlimit (2.5f, 10.0f, 0.5f / vibZcT) - vibHz) * 0.3f;
                lastVibSign = sgn; vibZcT = 0;
            }
            const float vib = vibRaw * vibCompensation (vibHz);
            // ---- dynamic vibrato: is the singer making vibrato right now, and how far into it are we?
            {
                const float av = std::abs (vib);
                vibEnv += (av > vibEnv ? 0.25f : dt / 0.35f) * (av - vibEnv);              // peak depth (semitones), ~0.35 s memory
                const bool present = vibEnv * 100.0f >= juce::jmax (1.0f, s.dvThresholdCents) && vibHz > 2.8f && vibHz < 9.5f;
                vibOnT = present ? vibOnT + dt : juce::jmax (0.0f, vibOnT - 2.0f * dt);
                const float want = juce::jlimit (0.0f, 1.0f, (vibOnT - s.dvDelay) / juce::jmax (0.005f, s.dvAttack));
                dvAmt += (want - dvAmt) * (want > dvAmt ? 1.0f - std::exp (-dt / juce::jmax (0.005f, s.dvAttack * 0.35f)) : 0.05f);
                // amplitude: the voice's own level wobble (frame rms against a 2 Hz average of it)
                const float rmsNow = juce::jmax (1.0e-6f, f.rms);
                const float ka = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2.0f * dt);
                if (rmsSlow2 <= 0) rmsSlow1 = rmsSlow2 = rmsNow;
                rmsSlow1 += ka * (rmsNow - rmsSlow1); rmsSlow2 += ka * (rmsSlow1 - rmsSlow2);
                rmsFast = rmsNow;
            }
            const float keepVib = s.dvMode > 0 ? 1.0f : s.vibKeep;   // dynamic vibrato decides on its own; the target never chases the wobble
            const float pt = nIn - keepVib * vib;

            // ---- choose the target note
            float desired = 0; float tauMs = s.retuneMs; float vibScale = 1.0f + s.natVib; float extraFormant = 0;
            int heldCount = 0; std::array<int, 16> heldList {};
            for (int n = 0; n < 128 && heldCount < 16; ++n) if (held[(size_t) n]) heldList[(size_t) heldCount++] = n;
            float newTarget = target;
            if (gn != nullptr)
            {
                newTarget = gn->pitch;
                desired = (gn->pitch - gn->orig) - (1.0f - gn->drift) * (centre2 - gn->orig);
                tauMs = gn->retuneMs; vibScale = gn->vibScale; extraFormant = gn->formantSt;
            }
            else
            {
                bool passthrough = false;
                if (s.midiTargets && heldCount > 0)
                {
                    float bd = 1e9f; int best = heldList[0];
                    for (int i = 0; i < heldCount; ++i)
                    {
                        int n = heldList[(size_t) i];
                        if (s.allOctaves) { while (n - pt > 6.0f) n -= 12; while (pt - n > 6.0f) n += 12; }
                        if (std::abs ((float) n - pt) < bd) { bd = std::abs ((float) n - pt); best = n; }
                    }
                    newTarget = (float) best;
                }
                else if (s.midiTargets && s.midiPassThrough) passthrough = true;
                else
                {
                    const int cand = nearestInMask (pt, s.allowedMask);
                    const bool keep = target > -999 && pcIn (s.allowedMask, (int) target) && std::abs (pt - target) <= std::abs (pt - (float) cand) + 0.2f;
                    newTarget = keep ? target : (float) cand;
                    if (pcIn (s.bypassMask, juce::roundToInt (pt))) passthrough = true;
                }
                if (passthrough) newTarget = pt;
                float dev = pt - newTarget;
                float strengthHere = s.strength;
                if (s.flex > 0.0f)
                {
                    const float zoneOut = 0.5f * (1.0f - 0.9f * s.flex) + 0.02f, zoneIn = zoneOut * 0.45f;
                    const float ad = std::abs (dev);
                    strengthHere *= ad <= zoneIn ? 1.0f : ad >= zoneOut ? 0.0f : 1.0f - juce::jmap (ad, zoneIn, zoneOut, 0.0f, 1.0f);
                }
                desired = -dev * strengthHere;
            }
            if (std::abs (newTarget - target) > 0.01f) { noteTime = 0; }
            target = newTarget;
            noteTime += dt;

            // ---- retune: Classic = first-order (the hard-tune snap), Modern = critically-damped second order (smooth glides)
            if (s.humanize > 0 && gn == nullptr) tauMs *= 1.0f + 6.0f * s.humanize * juce::jlimit (0.0f, 1.0f, (noteTime - 0.12f) / 0.4f);
            if (tauMs < 0.6f) { c = desired; cv = 0; }
            else if (s.classic)
            {
                c += (desired - c) * (1.0f - std::exp (-dt / (tauMs * 0.001f)));
            }
            else
            {
                const float w = 1.0f / (tauMs * 0.001f * 0.6f);
                cv += ((desired - c) * w * w - 2.0f * w * cv) * dt;
                c += cv * dt;
                if (! std::isfinite (c)) { c = desired; cv = 0; }
            }
            // Preserve: while the singer holds a vibrato, apply the correction averaged over exactly one vibrato period,
            // so not a trace of the vibrato's own rate leaks into the correction (rate and depth pass untouched in both
            // engines). The average restarts on every new target note, so note changes still snap at Retune 0.
            float cUse = c;
            if (s.dvMode == 1 && gn == nullptr)
            {
                const int want = juce::jlimit (8, 255, juce::roundToInt ((float) fr / juce::jmax (3.0f, vibHz)));
                if (noteTime <= dt * 1.5f || std::abs (want - cLen) > 4) {   // (the measured rate wobbles by a frame or two: keep the length)
                     cLen = want; cSum = 0; for (int k = 0; k < cLen; ++k) cHist[k] = c; cSum = (double) c * cLen; cW = 0; }
                cSum += (double) c - cHist[cW]; cHist[cW] = c; cW = (cW + 1) % cLen;
                cUse = c + dvAmt * ((float) (cSum / cLen) - c);
            }
            float out = nIn + cUse + (vibScale - 1.0f) * vib;
            float dvGain = 1.0f;
            if (s.dvMode >= 2 && gn == nullptr)
            {
                const float sign = s.dvMode == 2 ? -1.0f : 1.0f;
                if (s.dvVariation > 0) dvNoise += ((juce::Random::getSystemRandom().nextFloat() * 2 - 1) * 0.06f - dvNoise * 0.02f);
                const float depth = s.dvPitch * dvAmt * (s.dvMode == 3 ? 1.0f + s.dvVariation * juce::jlimit (-0.5f, 0.5f, dvNoise) : 1.0f);
                out += sign * depth * vib;
                if (s.dvAmp > 0 && rmsSlow2 > 0)
                    dvGain = juce::jlimit (0.5f, 2.0f, std::pow (rmsFast / rmsSlow2, sign * s.dvAmp * dvAmt));
                if (s.dvTimbre > 0) extraFormant += sign * s.dvTimbre * dvAmt * vib;
            }

            // ---- transpose (optionally conformed to the scale)
            float shift = s.transpose;
            if (s.conform && std::abs (s.transpose) > 0.01f && target > -999)
                shift = (float) nearestInMask (target + s.transpose, s.allowedMask) - target;
            // ---- motion pattern (Moth Tune FX): move the target by scale steps, with its own glide
            {
                float mWant = 0;
                if (std::abs (s.motionSteps) > 0.01f && target > -999)
                {
                    const int base = juce::roundToInt (target + shift);
                    mWant = s.motionDiatonic ? (float) (stepInMask (base, juce::roundToInt (s.motionSteps), s.allowedMask) - base) : s.motionSteps;
                }
                if (s.motionGlideMs < 0.5f) motionSm = mWant;
                else motionSm += (mWant - motionSm) * (1.0f - std::exp (-dt / (s.motionGlideMs * 0.001f)));
                shift += motionSm;
            }
            out += shift;

            // ---- create vibrato
            if (s.vibDepth > 0.01f && ! (s.vibFxStraightOnly && dvAmt > 0.5f))
            {
                rateNoise += (juce::Random::getSystemRandom().nextFloat() * 2 - 1) * 0.08f - rateNoise * 0.01f;
                depthNoise += (juce::Random::getSystemRandom().nextFloat() * 2 - 1) * 0.08f - depthNoise * 0.01f;
                const float rate = s.vibRate * (1.0f + s.vibVar * 0.25f * juce::jlimit (-1.0f, 1.0f, rateNoise));
                vibPhase += rate * dt; vibPhase -= std::floor (vibPhase);
                const float onsetT = noteTime - s.vibDelay;
                const float ampTarget = onsetT <= 0 ? 0.0f : (s.vibOnset <= 0.001f ? 1.0f : juce::jlimit (0.0f, 1.0f, onsetT / s.vibOnset));
                vibAmp += (ampTarget - vibAmp) * 0.2f;
                out += s.vibDepth / 100.0f * vibAmp * (1.0f + s.vibVar * 0.3f * juce::jlimit (-1.0f, 1.0f, depthNoise)) * shapeLfo (s.vibShape, vibPhase);
            }

            r.voiced = true; r.inNote = nIn; r.outNote = out; r.target = target + shift;
            r.gain = dvGain; r.vibRateHz = vibHz; r.vibDepthCents = vibEnv * 100.0f; r.vibAmount = dvAmt;
            r.ratio = juce::jlimit (0.25f, 4.0f, std::pow (2.0f, (out - nIn) / 12.0f));
            r.corrCents = (out - nIn) * 100.0f;
            r.formant = formantFor (s, r.ratio, extraFormant);

            // ---- harmonies
            const float base = target + shift, micro = out - base;
            int midiVoice = 0;
            for (int i = 0; i < 4; ++i)
            {
                const auto& h = s.harm[(size_t) i];
                r.hOn[i] = false;
                if (h.mode == 0) continue;
                float hn = -1;
                if (h.mode == 1) hn = out + h.interval;
                else if (h.mode == 2) hn = (float) stepInMask (juce::roundToInt (base), juce::roundToInt (h.interval), s.allowedMask) + micro;
                else if (h.mode == 3)
                {
                    // MIDI: voices take the held notes in order (lowest first), skipping the one the lead is on
                    while (midiVoice < heldCount && s.midiTargets && heldList[(size_t) midiVoice] == juce::roundToInt (base)) ++midiVoice;
                    if (midiVoice >= heldCount) continue;
                    hn = (float) heldList[(size_t) midiVoice++] + micro * 0.5f;
                }
                // slow humanising drift per voice
                if (s.harmDetune > 0)
                {
                    if (juce::Random::getSystemRandom().nextFloat() < dt * 1.5f) driftTarget[(size_t) i] = (juce::Random::getSystemRandom().nextFloat() * 2 - 1) * s.harmDetune;
                    drift[(size_t) i] += (driftTarget[(size_t) i] - drift[(size_t) i]) * dt * 2.0f;
                    hn += drift[(size_t) i] / 100.0f;
                }
                r.hOn[i] = true; r.hNote[i] = hn;
                r.hRatio[i] = juce::jlimit (0.25f, 4.0f, std::pow (2.0f, (hn - nIn) / 12.0f));
                r.hFormant[i] = (s.preserve ? 1.0f : r.hRatio[i]) * std::pow (2.0f, h.formantSt / 12.0f) / juce::jmax (0.5f, s.throat);
            }
            return r;
        }

    private:
        double fr = 459;
        static constexpr float centreHz = 3.0f;
        float prevIn = 0, slope = 0, vibZcT = 0, vibHz = 5.5f, motionSm = 0; int lastVibSign = 0;
        static float vibCompensation (float fv)
        {
            // |1 - H(f)| for the two-pole centre filter H(f) = 1 / (1 + j f/fc)^2
            const std::complex<float> h = 1.0f / ((1.0f + std::complex<float> (0, fv / centreHz)) * (1.0f + std::complex<float> (0, fv / centreHz)));
            return 1.0f / juce::jmax (0.5f, std::abs (1.0f - h));
        }
        float c = 0, cv = 0, centre1 = 0, centre2 = 0, target = -1000, noteTime = 0, vibPhase = 0, vibAmp = 0, rateNoise = 0, depthNoise = 0;
        float vibEnv = 0, vibOnT = 0, dvAmt = 0, rmsFast = 0, rmsSlow1 = 0, rmsSlow2 = 0, dvNoise = 0;
        float cHist[256] {}; int cW = 0, cLen = 0; double cSum = 0;
        bool haveCentre = false;
        std::array<float, 4> drift {}, driftTarget {};

        static float formantFor (const TuneSettings& s, float ratio, float extraSt = 0)
        {
            return (s.preserve ? 1.0f : ratio) * std::pow (2.0f, (s.formantSt + extraSt) / 12.0f) / juce::jmax (0.5f, s.throat);
        }
    };
}


namespace SqVocal
{
    //==========================================================================
    /** Everything Moth Tune does to a block of audio: detection, the brain, the Live or Studio engine, harmonies,
        dry/wet. Pure DSP (no engine types), so the same core drives Moth Tune, Moth Tune FX, Molt's monophonic
        mode and the measurement tests. */
    class TuneProcessor
    {
    public:
        struct Context
        {
            double editStart = 0;   // edit time (s) of the block's first input sample
            bool playing = false;
            const std::vector<GraphNote>* graph = nullptr;
            const std::vector<TimedNote>* trackNotes = nullptr;
            const std::array<bool, 128>* heldIn = nullptr;
            float mix = 1, outGain = 1, tracking = 50;
        };
        std::function<void (const FrameResult&, const std::array<bool, 128>&)> onFrame;

        static int latencyFor (int eng, int vt, double sr)
        {
            if (eng == 0) return juce::roundToInt (sr * 0.0006) + 4;
            const auto& v = voiceTypes()[(size_t) juce::jlimit (0, 5, vt)];
            return (int) std::ceil (3.0 * sr / v.lo) + 64;
        }
        int latency() const { return L; }
        int engineMode() const { return eng; }
        /** The newest analysis result (Moth Tune FX uses it to drive pitch-tracked effects). */
        const FrameResult& latestResult() const { return results[(size_t) ((resultW - 1) & 2047)].r; }
        float latestHz() const { return lastVoiced ? lastHz : 0.0f; }

        void prepare (double sampleRate, int engineMode, int voiceType)
        {
            sr = sampleRate; eng = engineMode; vt = juce::jlimit (0, 5, voiceType);
            L = latencyFor (eng, vt, sr);
            const auto& v = voiceTypes()[(size_t) vt];
            tracker.prepare (sr); tracker.setRange (v.lo, v.hi);
            brain.prepare (tracker.frameRate());
            psola.prepare (sr, 2, L, (float) (sr / v.lo));
            for (int c = 0; c < 2; ++c) { liveIn[c].prepare ((int) (sr * 0.25)); lpcLead[c].prepare (sr); }
            liveMono.prepare ((int) (sr * 0.25));
            for (auto& l : lpcHarm) l.prepare (sr);
            for (auto& d : harmDelay) d.prepare ((int) (sr * 0.06) + 8);
            results.assign (2048, {});
            gateK = (float) (1.0 - std::exp (-1.0 / (0.012 * sr)));
            lpcK = (float) (1.0 - std::exp (-1.0 / (0.006 * sr)));
            reset();
        }
        void reset()
        {
            tracker.reset(); brain.reset(); psola.reset();
            for (int c = 0; c < 2; ++c) { liveIn[c].clear(); lpcLead[c].reset(); }
            liveMono.clear();
            for (auto& l : lpcHarm) l.reset();
            for (auto& d : harmDelay) d.clear();
            livePos = 0; lead = {}; for (auto& h : harmSp) h = {};
            std::fill (results.begin(), results.end(), Stored {}); resultW = 0;
            rSm = 1; for (auto& h : hrSm) h = 1; gSm = 1; gateSm = 1; lpcMix = 0; for (auto& m : lpcHMix) m = 0;
            lastPeriod = (float) (sr / 200.0); lastVoiced = false; lastHz = 0; hdW = 0;
            for (int i = 0; i < 4; ++i) timingPhase[(size_t) i] = 0.25f * (float) i;
        }

        void process (float* const* io, int nch, int num, const TuneSettings& s0, const Context& ctx)
        {
            nch = juce::jlimit (1, 2, nch);
            tracker.setTracking (ctx.tracking);
            TuneSettings sl = s0;
            if (eng == 0) sl.predictSec = (float) ((tracker.lagSamples() - L) / sr) * 0.8f;
            const TuneSettings& s = sl;
            float hGain[4], hPanL[4], hPanR[4];
            bool anyHarm = false;
            for (int i = 0; i < 4; ++i)
            {
                hGain[i] = s.harm[(size_t) i].gain;
                const float p = juce::jlimit (-1.0f, 1.0f, s.harm[(size_t) i].pan);
                hPanL[i] = std::cos ((p + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
                hPanR[i] = std::sin ((p + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
                anyHarm = anyHarm || s.harm[(size_t) i].mode != 0;
            }
            const int64 blockAbs = tracker.position();
            for (int i = 0; i < num; ++i)
            {
                float frame[2];
                frame[0] = io[0][i]; frame[1] = io[nch - 1][i];
                const float mono = nch > 1 ? 0.5f * (frame[0] + frame[1]) : frame[0];

                if (tracker.push (mono))
                {
                    const auto& f = tracker.latest();
                    const double tEdit = ctx.editStart + (double) (f.pos - blockAbs) / sr;
                    const GraphNote* gn = (ctx.graph != nullptr && ctx.playing) ? findGraph (*ctx.graph, tEdit) : nullptr;
                    std::array<bool, 128> held {};
                    if (ctx.heldIn != nullptr) held = *ctx.heldIn;
                    if (ctx.trackNotes != nullptr && ctx.playing)
                        for (auto& tn : *ctx.trackNotes) { if (tn.start > tEdit) break; if (tEdit < tn.end) held[(size_t) juce::jlimit (0, 127, tn.pitch)] = true; }
                    const auto res = brain.process (f, s, held, gn);
                    results[(size_t) (resultW & 2047)] = { f.pos, res };
                    ++resultW;
                    if (f.voiced) { lastPeriod = (float) (sr / f.hz); lastHz = f.hz; }
                    lastVoiced = f.voiced;
                    if (onFrame) onFrame (res, held);
                }

                float outL = 0, outR = 0, dryL, dryR;
                gateSm += (s.harmGate - gateSm) * gateK;
                if (eng == 1)
                {
                    psola.pushInput (frame, 2, lastPeriod, lastVoiced, lastVoiced ? lastHz : 0.0f);
                    const int64 n = psola.newest();
                    psola.synthesiseUpTo (n, [this, &s] (int st, const Psola::Mark& mk)
                    {
                        Psola::Grain g;
                        const auto& r = resultAt (mk.pos);
                        if (st == 0) { g.ratio = r.voiced ? r.ratio : 1.0f; g.formant = r.voiced ? r.formant : 1.0f; g.gain = r.voiced ? r.gain : 1.0f; return g; }
                        const int h = st - 1;
                        g.monoSource = true;
                        if (s.harm[(size_t) h].mode == 0) { g.silent = true; return g; }
                        if (! r.voiced) { g.ratio = 1.0f; g.formant = 1.0f; g.gain = 0.35f; return g; }
                        if (! r.hOn[h]) { g.silent = true; return g; }
                        g.ratio = r.hRatio[h]; g.formant = r.hFormant[h];
                        return g;
                    }, anyHarm ? 5 : 1);
                    outL = psola.readAndClear (0, 0, n); outR = psola.readAndClear (0, 1, n);
                    if (anyHarm)
                        for (int h = 0; h < 4; ++h)
                        {
                            const float v = harmTime (h, psola.readAndClear (h + 1, 0, n), s.harmTimingMs) * gateSm;
                            outL += v * hGain[h] * hPanL[h]; outR += v * hGain[h] * hPanR[h];
                        }
                    dryL = psola.input (0).at (n - L); dryR = psola.input (1).at (n - L);
                }
                else
                {
                    for (int c = 0; c < 2; ++c) liveIn[c].at (livePos) = frame[c];
                    liveMono.at (livePos) = mono;
                    const auto& r = results[(size_t) ((resultW - 1) & 2047)].r;
                    const float tr = r.voiced ? r.ratio : 1.0f;
                    rSm += (tr - rSm) * 0.08f;
                    const double period = lastVoiced ? (double) lastPeriod : sr * 0.0045;
                    lead.advance (livePos, rSm, period, L, sr);
                    if (! lastVoiced && std::abs (rSm - 1.0f) < 0.001f) { rSm = 1.0f; lead.recentre (livePos, L, sr); }
                    const float warp = r.voiced ? r.formant : 1.0f;
                    // the formant stage runs all the time (its filters stay warm) and is crossfaded in by how far the
                    // formant has to move from the pitch shift: switching it on and off per sample - as a hard-tuned
                    // vibrato crosses ratio 1 - restarted the lattices from stale state and burst
                    const float lpcWant = juce::jlimit (0.0f, 1.0f, (std::abs (warp - rSm) - 0.002f) * 250.0f);
                    lpcMix += (lpcWant - lpcMix) * lpcK;
                    for (int c = 0; c < 2; ++c)
                    {
                        float y = lead.read (liveIn[c]);
                        const float z = lpcLead[c].process (liveIn[c], livePos, y, warp);
                        if (lpcMix > 1.0e-4f) y += lpcMix * (z - y);
                        (c == 0 ? outL : outR) = y;
                    }
                    lead.endSample();
                    gSm += ((r.voiced ? r.gain : 1.0f) - gSm) * 0.08f;
                    outL *= gSm; outR *= gSm;
                    if (anyHarm)
                        for (int h = 0; h < 4; ++h)
                        {
                            if (s.harm[(size_t) h].mode == 0) continue;
                            const bool on = r.voiced ? r.hOn[h] : true;
                            hrSm[h] += ((r.voiced ? r.hRatio[h] : 1.0f) - hrSm[h]) * 0.08f;
                            harmSp[(size_t) h].advance (livePos, hrSm[h], period, L, sr);
                            float v = harmSp[(size_t) h].read (liveMono);
                            harmSp[(size_t) h].endSample();
                            const float hw = r.voiced ? r.hFormant[h] : 1.0f;
                            {   // same as the lead: always running, crossfaded (unison doubles sit right at ratio 1)
                                const float z = lpcHarm[(size_t) h].process (liveMono, livePos, v, hw);
                                const float want = juce::jlimit (0.0f, 1.0f, (std::abs (hw - hrSm[h]) - 0.002f) * 250.0f);
                                lpcHMix[h] += (want - lpcHMix[h]) * lpcK;
                                if (lpcHMix[h] > 1.0e-4f) v += lpcHMix[h] * (z - v);
                            }
                            if (! on) v = 0; else if (! r.voiced) v *= 0.35f;
                            v = harmTime (h, v, s.harmTimingMs) * gateSm;
                            outL += v * hGain[h] * hPanL[h]; outR += v * hGain[h] * hPanR[h];
                        }
                    dryL = liveIn[0].at (livePos - L); dryR = liveIn[1].at (livePos - L);
                    ++livePos;
                }
                if (anyHarm) ++hdW;
                io[0][i] = (dryL * (1.0f - ctx.mix) + outL * ctx.mix) * ctx.outGain;
                if (nch > 1) io[1][i] = (dryR * (1.0f - ctx.mix) + outR * ctx.mix) * ctx.outGain;
            }
        }

        static const GraphNote* findGraph (const std::vector<GraphNote>& v, double t)
        {
            auto it = std::upper_bound (v.begin(), v.end(), t, [] (double x, const GraphNote& n) { return x < n.start; });
            if (it == v.begin()) return nullptr;
            --it;
            return t < it->end ? &*it : nullptr;
        }

    private:
        double sr = 44100; int eng = 1, vt = 1, L = 0;
        PitchTracker tracker; TuneBrain brain; Psola psola;
        Ring liveIn[2], liveMono;
        Splice lead; std::array<Splice, 4> harmSp;
        LpcFormant lpcLead[2]; std::array<LpcFormant, 4> lpcHarm;
        std::array<Ring, 4> harmDelay; int64 hdW = 0;
        std::array<float, 4> timingPhase {};
        int64 livePos = 0;
        float rSm = 1, hrSm[4] { 1, 1, 1, 1 }, gSm = 1, gateSm = 1, gateK = 0.002f, lpcMix = 0, lpcK = 0.004f, lpcHMix[4] {};
        float lastPeriod = 220, lastHz = 0; bool lastVoiced = false;
        struct Stored { int64 pos = 0; FrameResult r; };
        std::vector<Stored> results; int64 resultW = 0;

        const FrameResult& resultAt (int64 pos) const
        {
            static const FrameResult none;
            if (resultW == 0) return none;
            int64 best = resultW - 1;
            for (int64 k = resultW - 1; k >= juce::jmax ((int64) 0, resultW - 2000); --k)
            {
                const auto& st = results[(size_t) (k & 2047)];
                best = k;
                if (st.pos <= pos)
                {
                    if (k + 1 < resultW && std::abs (results[(size_t) ((k + 1) & 2047)].pos - pos) < pos - st.pos) best = k + 1;
                    break;
                }
            }
            return results[(size_t) (best & 2047)].r;
        }

        float harmTime (int h, float v, float maxMs)
        {
            auto& d = harmDelay[(size_t) h];
            const int64 w = hdW;   // all four lines share the write index (advanced once per sample)
            d.at (w) = v;
            float ms = 0;
            if (maxMs > 0.05f)
            {
                timingPhase[(size_t) h] += (float) ((0.13 + 0.07 * h) / sr);
                if (timingPhase[(size_t) h] > 1.0f) timingPhase[(size_t) h] -= 1.0f;
                ms = maxMs * (0.55f + 0.45f * std::sin (juce::MathConstants<float>::twoPi * timingPhase[(size_t) h]));
            }
            return d.cubic ((double) w - 2.0 - ms * 0.001 * sr);
        }
    };
}
