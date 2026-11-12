#pragma once
// Atrium Verb - DSP core (1.14 "Studio Line", phase D). No UI, no te:: types: tests/atrium_test.cpp measures it.
//
// An algorithmic reverb built as a 16-line feedback delay network (FDN):
//   input -> predelay (gliding, up to 500 ms) -> early reflections (a per-space tap pattern, scaled by Size)
//                                             -> input diffusion (4 lattice all-passes per side; Bloom adds 3 long ones)
//                                             -> 16 delay lines, lengths spread geometrically and rounded to primes,
//                                                each read with cubic Hermite interpolation and slowly modulated
//                                                (Character) so the tail never rings metallic
//                                             -> per-line decay filter: mid gain from the Decay time, first-order low
//                                                and high shelves from the low / high decay multipliers - so the decay
//                                                time is set per band, exactly, at DC / mid / Nyquist
//                                             -> orthonormal 16x16 Hadamard mixing (lossless) back into the lines
//   late output taps (two orthogonal sign patterns: decorrelated left and right) -> gate (Gate space) ->
//   early + late -> post EQ (24 dB/oct Butterworth low cut, high cut) -> width (M/S) -> ducking keyed by the dry input -> mix -> out.
//   Freeze: decay exactly lossless, input muted - the tank holds what it has, forever.

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace SqAtrium
{
    constexpr double pi = 3.14159265358979323846;
    constexpr int N = 16;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    struct Space
    {
        const char* name; const char* about;
        double scale;        // multiplies the line lengths and the early-reflection spread
        double diffusion;    // multiplies the Diffusion control
        int erPattern;       // 0 none (plate-like), 1 room, 2 hall, 3 church
        double modDepth;     // 0..1 of the maximum modulation (Character scales it)
        double modRate;      // Hz, the centre of the 16 line LFO rates
        double colourHz;     // > 0: a gentle low-pass inside the tank (vintage / dark spaces)
        bool bloom;          // three long all-passes before the tank: the sound swells
        double gateHoldMs;   // > 0: the tail is gated by the input (with this hold)
    };
    inline const std::array<Space, 12>& spaces()
    {
        static const std::array<Space, 12> s { {
            { "Concert Hall",    "A large, even hall: early reflections from distant walls, a smooth dense tail",           1.00, 0.90, 2, 0.35, 0.50,    0, false,   0 },
            { "Plate",           "A bright metal plate: no early reflections, instant density, silky top",                0.55, 1.00, 0, 0.50, 0.90,    0, false,   0 },
            { "Grand Hall",      "A very large space: slower build-up, long wide tail",                                    1.35, 0.90, 2, 0.30, 0.35,    0, false,   0 },
            { "Chamber",         "A live echo chamber: close dense reflections, warm and solid",                           0.70, 0.80, 1, 0.25, 0.60,    0, false,   0 },
            { "Studio Room",     "A treated live room: short, natural, puts things in a real space",                      0.40, 0.70, 1, 0.15, 0.80,    0, false,   0 },
            { "Booth",           "A small booth: tight early sound with a short tail",                                      0.22, 0.60, 1, 0.10, 1.00,    0, false,   0 },
            { "Cathedral",       "Stone and height: late distant reflections and a very long, enveloping tail",           1.70, 0.95, 3, 0.30, 0.25,    0, false,   0 },
            { "Ambience",        "Space without obvious reverb: early sound and a whisper of tail",                        0.30, 0.90, 1, 0.20, 0.70,    0, false,   0 },
            { "Vintage Digital", "An early digital hall: more chorus in the tail and a softened top",                     0.80, 0.75, 0, 0.60, 1.30, 9000, false,   0 },
            { "Bloom",           "The tail swells in after the note instead of starting at once",                         1.20, 1.00, 0, 0.40, 0.30,    0, true,    0 },
            { "Dark Space",      "A large soft room where the highs die early",                                              1.10, 0.90, 2, 0.30, 0.40, 4500, false,   0 },
            { "Gate",            "The classic gated room: a dense burst that stops dead when the sound stops",             0.50, 1.00, 1, 0.10, 0.50,    0, false, 280 },
        } };
        return s;
    }

    struct Params
    {
        int space = 0;
        double decaySec = 2.5;          // RT60 at mid frequencies, 0.1..30
        double size = 0.6;              // 0..1
        double predelayMs = 10;         // 0..500
        double diffusion = 0.85;        // 0..1
        double width = 1.0;             // 0..2
        double character = 0.4;         // 0..1 modulation
        double lowMult = 1.2, highMult = 0.6;   // decay multipliers below / above the crossovers
        double lowXHz = 250, highXHz = 4000;
        double early = 1.0, late = 1.0; // 0..2 levels
        bool freeze = false;
        double duckDb = 0, duckThreshDb = -30, duckReleaseMs = 250;
        double lowCutHz = 10, highCutHz = 20000;   // <= 10 off, >= 20000 off
        double mix = 0.3;               // 0..1
        double outputDb = 0;
    };

    /** Cubic Hermite (4-point) interpolation of a ring buffer, delay >= 2 samples. */
    struct Line
    {
        std::vector<float> b; int mask = 0, w = 0;
        void prepare (int maxSamples) { int sz = 1; while (sz < maxSamples + 8) sz <<= 1; b.assign ((size_t) sz, 0.0f); mask = sz - 1; w = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
        float read (double d) const
        {
            const double pos = (double) w - d; const double fl = std::floor (pos); const int i = (int) fl; const float f = (float) (pos - fl);
            const float y0 = b[(size_t) ((i - 1) & mask)], y1 = b[(size_t) (i & mask)], y2 = b[(size_t) ((i + 1) & mask)], y3 = b[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
        float readLin (double d) const
        {
            const double pos = (double) w - d; const double fl = std::floor (pos); const int i = (int) fl; const float f = (float) (pos - fl);
            return b[(size_t) (i & mask)] + f * (b[(size_t) ((i + 1) & mask)] - b[(size_t) (i & mask)]);
        }
        void write (float x) { b[(size_t) (w & mask)] = x; ++w; }
        void writeAt0 (float x) { b[(size_t) (w & mask)] = x; }
        void advance() { ++w; }
    };

    /** Lattice all-pass (z^-d + g) / (1 + g z^-d). */
    struct AllPass
    {
        std::vector<float> b; int len = 1, idx = 0;
        void prepare (int n) { len = std::max (1, n); b.assign ((size_t) len, 0.0f); idx = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); idx = 0; }
        float process (float x, float g)
        {
            const float z = b[(size_t) idx]; const float w = x - g * z; b[(size_t) idx] = w; if (++idx >= len) idx = 0;
            return z + g * w;
        }
    };

    struct OnePole1 { double b0 = 1, b1 = 0, a1 = 0, x1 = 0, y1 = 0;
        inline double process (double x) { const double y = b0 * x + b1 * x1 - a1 * y1; x1 = x; y1 = y; return y; }
        void clear() { x1 = y1 = 0; } };

    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void clear() { z1 = z2 = 0; }
        void lowpass (double fs, double f, double q) { const double w = 2 * pi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0; }
        void highpass (double fs, double f, double q) { const double w = 2 * pi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0; }
        void bandpass (double fs, double f, double q) { const double w = 2 * pi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0; } };

    /** In-place orthonormal fast Walsh-Hadamard transform of 16 values (lossless mixing). */
    inline void hadamard16 (float* x)
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float a = x[j], b = x[j + h]; x[j] = a + b; x[j + h] = a - b; }
        for (int i = 0; i < N; ++i) x[i] *= 0.25f;
    }

    inline bool isPrime (int n) { if (n < 2) return false; for (int d = 2; d * d <= n; ++d) if (n % d == 0) return false; return true; }
    inline int nearestPrime (int n) { for (int k = 0; k < 1000; ++k) { if (isPrime (n + k)) return n + k; if (n - k > 2 && isPrime (n - k)) return n - k; } return n; }

    struct Tap { double sec; float gL, gR; };

    /** The early-reflection pattern of a space, at scale 1 (Size and the space's scale stretch it). Deterministic. */
    inline std::vector<Tap> erPattern (int pattern)
    {
        std::vector<Tap> t; if (pattern <= 0) return t;
        const int n = pattern == 1 ? 18 : pattern == 2 ? 14 : 12;
        const double t0 = pattern == 1 ? 0.002 : pattern == 2 ? 0.008 : 0.015, t1 = pattern == 1 ? 0.040 : pattern == 2 ? 0.090 : 0.140;
        const double tau = pattern == 1 ? 0.015 : pattern == 2 ? 0.040 : 0.060;
        unsigned s = 0x9E3779B9u * (unsigned) pattern;
        auto rnd = [&s] { s = s * 1664525u + 1013904223u; return (double) (s >> 8) / 16777216.0; };
        double eL = 0, eR = 0;
        for (int k = 0; k < n; ++k)
        {
            const double u = (k + 0.15 + 0.7 * rnd()) / n, sec = t0 + (t1 - t0) * u * u * 0.6 + (t1 - t0) * u * 0.4;
            const double g = std::exp (-(sec - t0) / tau);
            const float gL = (float) (g * (0.6 + 0.4 * rnd()) * (rnd() < 0.5 ? -1 : 1)), gR = (float) (g * (0.6 + 0.4 * rnd()) * (rnd() < 0.5 ? -1 : 1));
            t.push_back ({ sec, gL, gR }); eL += gL * gL; eR += gR * gR;
        }
        for (auto& x : t) { x.gL /= (float) std::sqrt (eL); x.gR /= (float) std::sqrt (eR); }
        return t;
    }

    class Engine
    {
    public:
        Params p;
        // meters (written on the audio thread)
        float meterWetDb = -100, meterDuckDb = 0, meterInDb = -100;

        static double maxLineSec() { return 0.120 * 1.70 + 0.004; }

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            for (auto& l : lines) l.prepare ((int) std::ceil (maxLineSec() * fs) + 64);
            inL.prepare ((int) std::ceil ((0.5 + 0.140 * 1.70 + 0.01) * fs)); inR.prepare (inL.mask);
            const double apL[4] = { 1.531, 2.237, 3.413, 4.597 }, apR[4] = { 1.613, 2.111, 3.559, 4.433 };   // 11.8 ms in all: dense, no lag
            const double blL[3] = { 37.1, 61.7, 89.3 }, blR[3] = { 41.3, 57.9, 97.1 };
            for (int k = 0; k < 4; ++k) { difL[k].prepare ((int) std::lround (apL[k] * 0.001 * fs)); difR[k].prepare ((int) std::lround (apR[k] * 0.001 * fs)); }
            for (int k = 0; k < 3; ++k) { bloomL[k].prepare ((int) std::lround (blL[k] * 0.001 * fs)); bloomR[k].prepare ((int) std::lround (blR[k] * 0.001 * fs)); }
            lastSpace = -1; haveCoeffs = false;
            update (true);
            reset();
        }

        void reset()
        {
            for (auto& l : lines) l.clear();
            inL.clear(); inR.clear();
            for (auto& a : difL) a.clear(); for (auto& a : difR) a.clear(); for (auto& a : bloomL) a.clear(); for (auto& a : bloomR) a.clear();
            for (auto& f : lsh) f.clear(); for (auto& f : hsh) f.clear(); colour.fill (0);
            for (auto* b : { &hpL, &hpR, &lpL, &lpR, &hp2L, &hp2R, &lp2L, &lp2R }) b->clear();
            erLpL = erLpR = 0; duckEnv = 0; duckGain = 1; gateEnv = 0; gateGain = 0; gateHold = 0;
            for (int i = 0; i < N; ++i) { const double ph = i * 2.399963229728653; lfoS[i] = std::sin (ph); lfoC[i] = std::cos (ph); }
            for (int i = 0; i < N; ++i) lenSm[i] = lenT[i];
            predSm = predT; erScaleSm = erScaleT; freezeSm = p.freeze ? 1.0 : 0.0; bloomSm = spaces()[(size_t) clampSpace()].bloom ? 1.0 : 0.0;
        }

        /** Mid decay time at frequency f (seconds), from the actual per-line filters (average line). For the face. */
        double decayAtHz (double f) const
        {
            if (p.freeze) return 1e9;
            double sum = 0; const double w = 2 * pi * f / fs;
            const std::complex<double> z1 = std::polar (1.0, -w);
            for (int i = 0; i < N; ++i)
            {
                auto H = [&] (const OnePole1& q) { return (q.b0 + q.b1 * z1) / (1.0 + q.a1 * z1); };
                double mag = std::abs (H (lsh[(size_t) i]) * H (hsh[(size_t) i])) * gMid[(size_t) i];
                if (colourA > 0) { const auto c = colourA / (1.0 - (1.0 - colourA) * z1); mag *= std::abs (c); }
                mag = std::min (mag, 0.9999999);
                sum += -3.0 * lenT[(size_t) i] / (fs * std::log10 (mag));
            }
            return sum / N;
        }

        /** The early reflections as drawn on the face: seconds after the predelay, and |gain| per side (level applied). */
        std::vector<Tap> currentTaps() const { std::vector<Tap> t = taps; for (auto& x : t) { x.sec *= erScaleT; x.gL *= (float) p.early; x.gR *= (float) p.early; } return t; }
        double meanLineSec() const { double s = 0; for (auto l : lenT) s += l; return s / N / fs; }
        double lateGain() const { return lateNorm; }

        void process (float* const* ch, int nch, int n)
        {
            update (false);
            const auto& sp = spaces()[(size_t) clampSpace()];
            const bool stereo = nch > 1;
            const float mix = (float) juce::jlimit (0.0, 1.0, p.mix), dryG = 1.0f - mix, outG = (float) dbToGain (p.outputDb);
            const double aGlide = 1.0 / (0.08 * fs), aFreeze = 1.0 / (0.03 * fs), aBloom = 1.0 / (0.05 * fs);
            const double duckAtt = 1.0 - std::exp (-1.0 / (0.001 * fs)), duckRel = 1.0 - std::exp (-1.0 / (std::max (5.0, p.duckReleaseMs) * 0.001 * fs));
            const double gateAtt = 1.0 - std::exp (-1.0 / (0.0005 * fs)), gateRel = 1.0 - std::exp (-1.0 / (0.012 * fs));
            const int holdN = (int) (sp.gateHoldMs * 0.001 * fs);
            const double gateThr = dbToGain (-50.0);
            const float dg1 = (float) (0.75 * diffEff), dg2 = (float) (0.625 * diffEff);
            const double modA = modDepthSamples;
            const float wS = (float) p.width;   // mid stays, side scaled
            float peakWet = 0, peakIn = 0; double minDuck = 1;
            const double erLpA = 1.0 - std::exp (-2 * pi * 9000.0 / fs);
            const bool frozenTarget = p.freeze; const double bloomTarget = sp.bloom ? 1.0 : 0.0;

            for (int s = 0; s < n; ++s)
            {
                const float dl = ch[0][s], dr = stereo ? ch[1][s] : dl;
                peakIn = std::max (peakIn, std::max (std::abs (dl), std::abs (dr)));
                // input history + predelay
                inL.writeAt0 (dl); inR.writeAt0 (dr);
                predSm += (predT - predSm) * aGlide; erScaleSm += (erScaleT - erScaleSm) * aGlide;
                freezeSm += ((frozenTarget ? 1.0 : 0.0) - freezeSm) * aFreeze;
                bloomSm += (bloomTarget - bloomSm) * aBloom;
                const float inGain = (float) (1.0 - freezeSm);
                const float pl = inL.readLin (predSm) * inGain, pr = inR.readLin (predSm) * inGain;
                // early reflections
                float eL = 0, eR = 0;
                if (! taps.empty() && p.early > 0)
                {
                    for (const auto& t : taps)
                    {
                        const double d = predSm + t.sec * erScaleSm * fs;
                        eL += inL.readLin (d) * t.gL; eR += inR.readLin (d) * t.gR;
                    }
                    eL *= inGain; eR *= inGain;
                    erLpL += (eL - erLpL) * erLpA; erLpR += (eR - erLpR) * erLpA;
                }
                inL.advance(); inR.advance();
                // input diffusion (+ bloom)
                float xl = pl, xr = pr;
                for (int k = 0; k < 2; ++k) { xl = difL[k].process (xl, dg1); xr = difR[k].process (xr, dg1); }
                const float onL = xl, onR = xr;   // the onset tap (spaces with no early reflections start here, 3.8 ms in)
                for (int k = 2; k < 4; ++k) { xl = difL[k].process (xl, dg2); xr = difR[k].process (xr, dg2); }
                if (bloomSm > 1e-4 || sp.bloom)
                {
                    float bl = xl, br = xr;
                    for (int k = 0; k < 3; ++k) { bl = bloomL[k].process (bl, 0.6f); br = bloomR[k].process (br, 0.6f); }
                    xl += (float) bloomSm * (bl - xl); xr += (float) bloomSm * (br - xr);
                }
                // tank
                float o[N], f[N];
                for (int i = 0; i < N; ++i)
                {
                    // glide toward a new Size, the pitch excursion capped at 4 % so a moving Size never pumps energy into the tank
                    lenSm[i] += juce::jlimit (-0.04, 0.04, (lenT[i] - lenSm[i]) * aGlide);
                    const double ns = lfoS[i] * lfoCw[i] + lfoC[i] * lfoSw[i]; lfoC[i] = lfoC[i] * lfoCw[i] - lfoS[i] * lfoSw[i]; lfoS[i] = ns;
                    o[i] = lines[(size_t) i].read (lenSm[i] + modA * (1.0 - freezeSm) * (1.0 + lfoS[i]));   // frozen: no modulation, no interpolation loss
                }
                const double fz = freezeSm;
                for (int i = 0; i < N; ++i)
                {
                    double y = lsh[(size_t) i].process (o[i]);
                    y = hsh[(size_t) i].process (y) * gMid[(size_t) i];
                    if (colourA > 0) { colour[(size_t) i] += colourA * (y - colour[(size_t) i]); y = colour[(size_t) i]; }
                    y += fz * ((double) o[i] - y);                         // freeze: exactly lossless
                    if (std::abs (y) > 4.0) y = 4.0 * std::tanh (y / 4.0) ; // a safety net that never touches music
                    f[i] = (float) y;
                }
                hadamard16 (f);
                for (int i = 0; i < N; ++i)
                {
                    const float inj = ((i & 1) ? xr : xl) * injSign[i] * 0.5f;
                    lines[(size_t) i].write (f[i] + inj);
                }
                float lL = 0, lR = 0;
                for (int i = 0; i < N; ++i) { lL += o[i] * outSignL[i]; lR += o[i] * outSignR[i]; }
                lL *= (float) (0.25 * lateNorm * p.late); lR *= (float) (0.25 * lateNorm * p.late);
                // gate (Gate space)
                if (holdN > 0 && ! frozenTarget)
                {
                    const double e = std::max (std::abs (pl), std::abs (pr));
                    gateEnv = std::max (e, gateEnv * 0.9995);
                    if (e > gateThr) gateHold = holdN; else if (gateHold > 0) --gateHold;
                    const double tgt = gateHold > 0 ? 1.0 : 0.0;
                    gateGain += (tgt - gateGain) * (tgt > gateGain ? gateAtt : gateRel);
                    lL *= (float) gateGain; lR *= (float) gateGain;
                }
                double wl = (double) erLpL * p.early + lL, wr = (double) erLpR * p.early + lR;
                if (onsetG > 0) { wl += onsetG * onL; wr += onsetG * onR; }
                // post EQ
                if (lowCutOn) { wl = hp2L.process (hpL.process (wl)); wr = hp2R.process (hpR.process (wr)); }
                if (highCutOn) { wl = lp2L.process (lpL.process (wl)); wr = lp2R.process (lpR.process (wr)); }
                // width
                const double m = 0.5 * (wl + wr), sd = 0.5 * (wl - wr) * wS;
                wl = m + sd; wr = m - sd;
                // ducking (keyed by the dry input)
                if (p.duckDb > 0.01)
                {
                    const double e = std::max (std::abs (dl), std::abs (dr));
                    duckEnv += (e - duckEnv) * (e > duckEnv ? duckAtt : duckRel);
                    const double over = juce::jlimit (0.0, 1.0, (gainToDb (duckEnv) - p.duckThreshDb) / 12.0);   // full depth 12 dB over
                    duckGain = dbToGain (-p.duckDb * over);
                    wl *= duckGain; wr *= duckGain; minDuck = std::min (minDuck, duckGain);
                }
                else duckGain = 1;
                peakWet = std::max (peakWet, (float) std::max (std::abs (wl), std::abs (wr)));
                ch[0][s] = (dl * dryG + (float) wl * mix) * outG;
                if (stereo) ch[1][s] = (dr * dryG + (float) wr * mix) * outG;
            }
            for (int i = 0; i < N; ++i) { const double r = std::sqrt (lfoS[i] * lfoS[i] + lfoC[i] * lfoC[i]); lfoS[i] /= r; lfoC[i] /= r; }
            meterWetDb = (float) gainToDb (peakWet); meterInDb = (float) gainToDb (peakIn); meterDuckDb = (float) gainToDb (minDuck);
        }

        int latency() const { return 0; }
        void refresh() { update (false); }

    private:
        double fs = 48000;
        std::array<Line, N> lines; Line inL, inR;
        std::array<AllPass, 4> difL, difR; std::array<AllPass, 3> bloomL, bloomR;
        std::array<OnePole1, N> lsh, hsh; std::array<double, N> gMid {}, colour {};
        double lenT[N] {}, lenSm[N] {}, lfoS[N] {}, lfoC[N] {}, lfoSw[N] {}, lfoCw[N] {};
        float injSign[N] {}, outSignL[N] {}, outSignR[N] {};
        std::vector<Tap> taps;
        Biquad hpL, hpR, hp2L, hp2R, lpL, lpR, lp2L, lp2R; bool lowCutOn = false, highCutOn = false;
        double onsetG = 0;
        double predT = 0, predSm = 0, erScaleT = 1, erScaleSm = 1, freezeSm = 0, bloomSm = 0, lateNorm = 1, diffEff = 0.85, modDepthSamples = 0, colourA = 0;
        float erLpL = 0, erLpR = 0; double duckEnv = 0, duckGain = 1, gateEnv = 0, gateGain = 0; int gateHold = 0;
        Params last; int lastSpace = -1; bool haveCoeffs = false;

        int clampSpace() const { return juce::jlimit (0, (int) spaces().size() - 1, p.space); }

        static bool same (const Params& a, const Params& b)
        {
            return a.space == b.space && a.decaySec == b.decaySec && a.size == b.size && a.predelayMs == b.predelayMs && a.diffusion == b.diffusion
                && a.character == b.character && a.lowMult == b.lowMult && a.highMult == b.highMult && a.lowXHz == b.lowXHz && a.highXHz == b.highXHz
                && a.freeze == b.freeze && a.lowCutHz == b.lowCutHz && a.highCutHz == b.highCutHz && a.early == b.early && a.late == b.late;
        }

        void update (bool force)
        {
            if (! force && haveCoeffs && same (p, last)) return;
            last = p; haveCoeffs = true;
            const int si = clampSpace(); const auto& sp = spaces()[(size_t) si];
            if (si != lastSpace)
            {
                taps = erPattern (sp.erPattern); lastSpace = si;
                for (int i = 0; i < N; ++i)
                {
                    // input / output sign patterns: three different Hadamard rows (orthogonal), so L and R taps decorrelate
                    auto row = [] (int r, int c) { return (__builtin_popcount ((unsigned) (r & c)) & 1) ? -1.0f : 1.0f; };
                    injSign[i] = row (5, i); outSignL[i] = row (9, i); outSignR[i] = row (6, i);
                }
            }
            const double sz = juce::jlimit (0.0, 1.0, p.size);
            const double tMin = std::max (0.0012, (0.0025 + 0.0175 * sz) * sp.scale), tMax = std::max (0.007, (0.010 + 0.110 * sz) * sp.scale);
            for (int i = 0; i < N; ++i)
            {
                // interleave short and long lines so both sides get the whole spread
                static const int order[N] = { 0, 15, 2, 13, 4, 11, 6, 9, 8, 7, 10, 5, 12, 3, 14, 1 };
                const double u = (double) order[i] / (N - 1);
                double sec = tMin * std::pow (tMax / tMin, u) * (1.0 + 0.035 * std::sin (i * 7.31));
                sec = std::min (sec, maxLineSec() - 0.002);
                lenT[i] = (double) nearestPrime ((int) std::lround (sec * fs));
            }
            erScaleT = (0.35 + 0.65 * sz) * sp.scale;
            predT = juce::jlimit (0.0, 0.5, p.predelayMs * 0.001) * fs;
            diffEff = juce::jlimit (0.0, 1.0, p.diffusion * sp.diffusion);
            modDepthSamples = juce::jlimit (0.0, 1.0, p.character) * sp.modDepth * 12.0 * fs / 48000.0;
            colourA = sp.colourHz > 0 && ! p.freeze ? 1.0 - std::exp (-2 * pi * std::min (sp.colourHz, 0.45 * fs) / fs) : 0.0;
            // per-line decay filters
            const double T = juce::jlimit (0.1, 30.0, p.decaySec);
            const double Tl = juce::jlimit (0.05, 100.0, T * juce::jlimit (0.1, 4.0, p.lowMult)), Th = juce::jlimit (0.05, 100.0, T * juce::jlimit (0.1, 2.0, p.highMult));
            const double Kl = std::tan (pi * juce::jlimit (20.0, 0.45 * fs, p.lowXHz) / fs), Kh = std::tan (pi * juce::jlimit (200.0, 0.45 * fs, p.highXHz) / fs);
            for (int i = 0; i < N; ++i)
            {
                const double L = lenT[i];
                const double gm = std::pow (10.0, -3.0 * L / (fs * T)), gl = std::pow (10.0, -3.0 * L / (fs * Tl)), gh = std::pow (10.0, -3.0 * L / (fs * Th));
                gMid[(size_t) i] = gm;
                { // low shelf: DC gain gl/gm, unity above
                    const double G = gl / gm, A = std::sqrt (G), B = 1.0 / A, a0 = 1 + B * Kl;
                    auto& q = lsh[(size_t) i]; q.b0 = (1 + A * Kl) / a0; q.b1 = (A * Kl - 1) / a0; q.a1 = (B * Kl - 1) / a0;
                }
                { // high shelf: Nyquist gain gh/gm, unity below
                    const double G = gh / gm, A = std::sqrt (G), B = 1.0 / A, a0 = B + Kh;
                    auto& q = hsh[(size_t) i]; q.b0 = (A + Kh) / a0; q.b1 = (Kh - A) / a0; q.a1 = (Kh - B) / a0;
                }
            }
            // the late level: energy grows with decay / mean length; keep it as the square root of that growth
            double Lm = 0; for (auto l : lenT) Lm += l; Lm /= N;
            const double growth = (T / 2.0) / ((Lm / fs) / 0.06);
            lateNorm = 0.9 * std::pow (std::max (1e-3, growth), -0.25);
            onsetG = sp.erPattern == 0 ? 0.3 * lateNorm * p.late : 0.0;
            // LFOs
            for (int i = 0; i < N; ++i)
            {
                const double rate = sp.modRate * (0.55 + 0.9 * i / (N - 1.0)) * (i & 1 ? 1.07 : 0.93);
                const double w = 2 * pi * rate / fs; lfoSw[i] = std::sin (w); lfoCw[i] = std::cos (w);
            }
            // post EQ
            lowCutOn = p.lowCutHz > 10.5; highCutOn = p.highCutHz < 19999 && p.highCutHz < 0.45 * fs;
            // 4th-order Butterworth low cut / high cut (two biquads, Q 0.5412 and 1.3066); only the coefficients move
            if (lowCutOn) for (auto* b : { &hpL, &hpR }) b->highpass (fs, p.lowCutHz, 0.5412);
            if (lowCutOn) for (auto* b : { &hp2L, &hp2R }) b->highpass (fs, p.lowCutHz, 1.3066);
            if (highCutOn) for (auto* b : { &lpL, &lpR }) b->lowpass (fs, p.highCutHz, 0.5412);
            if (highCutOn) for (auto* b : { &lp2L, &lp2R }) b->lowpass (fs, p.highCutHz, 1.3066);
        }
    };
}
