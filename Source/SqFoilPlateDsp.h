#pragma once
// Foil Plate (super prompt 1.11, C4): plate reverb. Standard C++ only (measured in tests/foilplate_test.cpp).
//
// A 16-line feedback delay network with a Hadamard mixing matrix, each line carrying its own absorption filter so the
// decay is set per band (highs die faster, as on a real plate), driven through a dispersive allpass chain (the plate's
// chirpy onset) from a transducer position and read at two pickup positions. Seven archetypes set the plate size,
// thickness (dispersion), band damping, pickups and transducer. Four input stages (Tube, Solid State with built-in
// compression, Clean, Tape) with audible, measured colour. Decay is RT60 at 500 Hz, 0.3 s .. 60 s or Freeze.
// Auto-Decay shortens or lengthens the tail while the input (or an external key) is active; Width, Balance, a
// 2-band post EQ, pre-delay, pre-filters at 6 / 12 / 24 dB/oct, modulation and an "infinite keeps darkening" freeze.

#include "SqEngine111Dsp.h"

namespace FoilPlate
{
    using namespace Eng;

    struct Archetype
    {
        const char* name; const char* description;
        float sizeMs;                 // longest line (the plate's size)
        float dispersion;             // allpass gain in the input chain (thickness / stiffness)
        float lowRatio, highRatio;    // RT60 at 120 Hz and 6 kHz relative to 500 Hz
        float highCornerHz;           // where the high-band damping starts
        float pickupSpread;           // 0..1 how different the two pickups are (stereo image)
        float brightness;             // output high shelf (dB) - the transducer's tilt
        float lowBloom;               // output low shelf (dB)
    };
    inline int numArchetypes() { return 7; }
    inline const Archetype& archetype (int i)
    {
        static const Archetype a[] = {
            { "Steel Classic",     "the classic 1950s-60s steel studio plate: bright, dense, a smooth splashy bloom",              62, 0.62f, 1.10f, 0.45f, 3500, 0.85f,  1.0f,  0.0f },
            { "Gold Foil",         "small gold-foil plate in a cabinet: shimmering, airy, shorter low end",                       41, 0.55f, 0.70f, 0.62f, 6000, 0.95f,  2.0f, -1.0f },
            { "Studio Steel",      "American studio steel plate: darker, thicker, more body in the low mids",                      70, 0.68f, 1.25f, 0.35f, 2800, 0.75f, -0.5f,  1.0f },
            { "Light Plate",       "lightweight modern plate: fast, clean, transparent",                                          38, 0.50f, 0.85f, 0.55f, 5000, 0.90f,  0.5f, -0.5f },
            { "Solid-State Plate", "1980s solid-state plate emulation: crisp, slightly grainy, wide",                             52, 0.58f, 0.90f, 0.50f, 4500, 1.00f,  1.5f,  0.0f },
            { "Silver Mist",       "Squelchmoth original: an impossible silver sheet - long glistening highs, soft lows",          57, 0.66f, 0.65f, 0.90f, 8000, 1.00f,  2.5f, -1.5f },
            { "Iron Mammoth",      "Squelchmoth original: a huge cast-iron plate - booming, slow, dark and enormous",              95, 0.72f, 1.55f, 0.28f, 2200, 0.80f, -1.5f,  2.5f },
        };
        return a[clamp (i, 0, numArchetypes() - 1)];
    }
    enum InputStage { tube, solidState, clean, tapeIn, numInputs };
    inline const char* inputName (int i) { static const char* n[] = { "Tube", "Solid State", "Clean", "Tape" }; return n[clamp (i, 0, 3)]; }

    struct Params
    {
        int archetype = 0, input = tube, lowSlope = 1, highSlope = 1;   // slopes: 0 = 6, 1 = 12, 2 = 24 dB/oct
        float decayS = 2.0f, preDelayMs = 10, lowCut = 80, highCut = 12000, mod = 0.3f, modRateHz = 0.8f, mix = 0.25f;
        float width = 1, balance = 0, eqLowDb = 0, eqHighDb = 0, autoDecay = 0, autoThresholdDb = -30, inputDrive = 0.5f, outputDb = 0;
        bool freeze = false, darkening = false, monoIn = false, keyOn = false;
    };

    class Processor
    {
    public:
        static constexpr int N = 16;

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; (void) maxBlock;
            for (int i = 0; i < N; ++i) { int len = 1; while (len < (int) (0.25 * sr) + 8) len <<= 1; line[i].assign ((size_t) len, 0.0f); lmask[i] = len - 1; }
            int pd = 1; while (pd < (int) (0.26 * sr) + 8) pd <<= 1;
            for (int c = 0; c < 2; ++c) { pre[c].assign ((size_t) pd, 0.0f); for (int k = 0; k < numAp; ++k) { ap[c][k].assign ((size_t) (0.012 * sr) + 4, 0.0f); apW[c][k] = 0; } }
            pmask = pd - 1;
            for (int t = 0; t < numInputs; ++t) Nonlinear::warm (stageModel (t), stageChar (t));
            applied = {}; applied.archetype = -1; configure (true);
            reset();
        }
        void reset()
        {
            for (int i = 0; i < N; ++i) { std::fill (line[i].begin(), line[i].end(), 0.0f); lw[i] = 0; absLo[i].reset(); absHi[i].reset(); }
            for (int c = 0; c < 2; ++c)
            {
                std::fill (pre[c].begin(), pre[c].end(), 0.0f);
                for (int k = 0; k < numAp; ++k) std::fill (ap[c][k].begin(), ap[c][k].end(), 0.0f);
                for (auto& f : lc[c]) f.reset();
                for (auto& f : hc[c]) f.reset();
                lc1[c].reset(); hc1[c].reset(); eqLo[c].reset(); eqHi[c].reset(); outLo[c].reset(); outHi[c].reset(); stage[c].reset();
            }
            pw = 0; env = 0; compEnv = 0; decayMul = 1; modPh = 0; darkGain = 1;
        }
        Params p;

        void configure (bool force = false)
        {
            const auto& a = archetype (p.archetype);
            if (force || p.archetype != applied.archetype)
            {
                // line lengths: 16 mutually prime-ish lengths spread over the plate size
                static const float frac[N] = { 0.239f, 0.281f, 0.317f, 0.353f, 0.389f, 0.431f, 0.467f, 0.509f, 0.557f, 0.601f, 0.653f, 0.709f, 0.761f, 0.823f, 0.887f, 1.0f };
                for (int i = 0; i < N; ++i) len[i] = std::max (16.0, frac[i] * a.sizeMs * 0.001 * sr);
                // the input transducer and the two pickups: sign / weight patterns over the lines
                for (int i = 0; i < N; ++i)
                {
                    inVec[i] = ((i * 5) % 7 < 4 ? 1.0f : -1.0f) * (0.8f + 0.2f * (float) ((i * 3) % 5) / 4.0f);
                    const float l = ((i * 3 + 1) % 4 < 2 ? 1.0f : -1.0f), r = ((i * 7 + 2) % 4 < 2 ? 1.0f : -1.0f);
                    outL[i] = l; outR[i] = l * (1.0f - a.pickupSpread) + r * a.pickupSpread;
                }
                static const float apMs[numAp] = { 0.71f, 1.13f, 1.79f, 2.41f, 3.37f, 4.73f, 6.11f, 8.29f };
                for (int c = 0; c < 2; ++c) for (int k = 0; k < numAp; ++k) apLen[c][k] = std::max (2, (int) (apMs[k] * (1.0f + 0.13f * c) * 0.001 * sr));
                for (int c = 0; c < 2; ++c) { outLo[c].set (Biquad::lowShelf, sr, 180, 0.7, a.lowBloom); outHi[c].set (Biquad::highShelf, sr, 7000, 0.7, a.brightness); }
            }
            if (force || p.input != applied.input || p.inputDrive != applied.inputDrive)
                for (int c = 0; c < 2; ++c) { stage[c].model = stageModel (p.input); stage[c].character = stageChar (p.input); stage[c].bias = 0.15f; stage[c].rebuild(); stage[c].prepare (sr); }
            if (force || p.lowCut != applied.lowCut || p.lowSlope != applied.lowSlope || p.highCut != applied.highCut || p.highSlope != applied.highSlope)
                for (int c = 0; c < 2; ++c) setCuts (c);
            if (force || p.eqLowDb != applied.eqLowDb || p.eqHighDb != applied.eqHighDb)
                for (int c = 0; c < 2; ++c) { eqLo[c].set (Biquad::lowShelf, sr, 200, 0.7, p.eqLowDb); eqHi[c].set (Biquad::highShelf, sr, 5000, 0.7, p.eqHighDb); }
            applied = p;
            decayFilters (decayMul);
        }
        /** RT60 (s) the plate is set to in each band: 120 Hz, 500 Hz, 6 kHz. */
        void bandDecays (float& lo, float& mid, float& hi) const
        {
            const auto& a = archetype (p.archetype);
            mid = p.decayS * decayMul; lo = mid * a.lowRatio; hi = mid * a.highRatio;
        }
        /** Input-stage THD (%) at -18 dBFS, 1 kHz for the current input and drive (not real-time). */
        double inputThd() const
        {
            Nonlinear::Stage s; s.model = stageModel (p.input); s.character = stageChar (p.input); s.bias = 0.15f; s.prepare (48000.0); s.drive = dbToGain (driveDb());
            if (p.input == clean) return 0.0;
            const int n = 48000; std::vector<float> y ((size_t) n);
            for (int i = 0; i < n; ++i) y[(size_t) i] = s.process (0.177f * (float) std::sin (2 * pi * 1000.0 * i / 48000.0));
            double f = 0, h = 0;
            for (int k = 1; k <= 8; ++k)
            {
                double S = 0, C = 0; for (int i = n / 2; i < n; ++i) { S += y[(size_t) i] * std::sin (2 * pi * 1000.0 * k * i / 48000.0); C += y[(size_t) i] * std::cos (2 * pi * 1000.0 * k * i / 48000.0); }
                const double amp = std::sqrt (S * S + C * C);
                if (k == 1) f = amp; else h += amp * amp;
            }
            return 100.0 * std::sqrt (h) / std::max (1.0e-12, f);
        }
        float autoDecayMul() const { return decayMul; }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            configure();
            float* L = ch[0]; float* R = nch > 1 ? ch[1] : ch[0];
            const float preS = clamp (p.preDelayMs, 0.0f, 250.0f) * 0.001f * (float) sr;
            const float drive = dbToGain (driveDb()), outG = dbToGain (p.outputDb);
            const float dry = p.mix <= 0.7f ? 1.0f : (1.0f - p.mix) / 0.3f, wet = p.mix;
            const float modDepth = p.mod * (float) (0.0006 * sr), modInc = (float) (p.modRateHz / sr);
            const float envA = (float) (1.0 - std::exp (-1.0 / (0.01 * sr))), envR = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));
            const float compA = (float) (1.0 - std::exp (-1.0 / (0.003 * sr))), compR = (float) (1.0 - std::exp (-1.0 / (0.12 * sr)));
            const float thr = dbToGain (p.autoThresholdDb);
            const auto& a = archetype (p.archetype);
            for (int i = 0; i < n; ++i)
            {
                float inL = L[i], inR = R[i];
                if (p.monoIn) inL = inR = 0.5f * (inL + inR);
                // auto-decay detector: the input, or the external key
                float det = 0.5f * (std::abs (inL) + std::abs (inR));
                if (p.keyOn && key != nullptr) det = 0.5f * (std::abs (key[0][i]) + std::abs (key[1][i]));
                env += (det > env ? envA : envR) * (det - env);
                if ((i & 31) == 0 && p.autoDecay != 0)
                {
                    const float active = clamp ((gainToDb (env + 1.0e-9f) - p.autoThresholdDb) / 12.0f, 0.0f, 1.0f);
                    const float target = p.autoDecay < 0 ? 1.0f + p.autoDecay * 0.9f * active : 1.0f + p.autoDecay * 2.0f * active;   // -100 %: down to a tenth; +100 %: up to 3x
                    decayMul += 0.15f * (target - decayMul);
                    decayFilters (decayMul);
                }
                (void) thr;
                // input stage (per channel), pre-filters, pre-delay, dispersion chain
                float x[2] = { inL, inR };
                for (int c = 0; c < 2; ++c)
                {
                    float v = x[c];
                    if (p.input == solidState)
                    {
                        const float a2 = std::abs (v); compEnv += (a2 > compEnv ? compA : compR) * (a2 - compEnv);
                        const float over = compEnv > 0.1f ? gainToDb (compEnv / 0.1f) : 0.0f;
                        v *= dbToGain (-over * 0.66f);                                        // 3:1 above -20 dBFS
                    }
                    if (p.input != clean) { stage[c].drive = drive; v = stage[c].process (v) / drive; }
                    if (p.lowCut > 20.5f) { if (p.lowSlope <= 0) v = lc1[c].hp (v); else for (int k = 0; k < cutStages (p.lowSlope); ++k) v = lc[c][k].process (v); }
                    if (p.highCut < 19900.0f) { if (p.highSlope <= 0) v = hc1[c].lp (v); else for (int k = 0; k < cutStages (p.highSlope); ++k) v = hc[c][k].process (v); }
                    pre[c][(size_t) pw] = v;
                    v = readPre (c, preS);
                    for (int k = 0; k < numAp; ++k)
                    {
                        auto& b = ap[c][k]; const int m = apLen[c][k];
                        int& wi = apW[c][k]; const int ri = (wi - m + (int) b.size()) % (int) b.size();
                        const float d = b[(size_t) ri], g = a.dispersion;
                        const float u = v + g * d; b[(size_t) wi] = u; wi = (wi + 1) % (int) b.size();
                        v = d - g * u;
                    }
                    x[c] = v;
                }
                pw = (pw + 1) & pmask;
                // FDN
                modPh += modInc; if (modPh >= 1.0f) modPh -= 1.0f;
                float y[N];
                for (int k = 0; k < N; ++k)
                {
                    const float m = modDepth * (float) std::sin (2 * pi * (modPh + k * 0.0625f)) * (k % 3 == 0 ? 1.0f : 0.6f);
                    y[k] = readLine (k, len[k] + m);
                }
                float ol = 0, orr = 0;
                for (int k = 0; k < N; ++k) { ol += outL[k] * y[k]; orr += outR[k] * y[k]; }
                hadamard (y);
                const float inject = p.freeze ? 0.0f : 1.0f;
                const float inMono = 0.5f * (x[0] + x[1]), inSide = 0.5f * (x[0] - x[1]);
                for (int k = 0; k < N; ++k)
                {
                    float v = y[k] * 0.25f;                                                   // 1/sqrt(16): the matrix is orthonormal
                    if (p.freeze)
                    {
                        if (p.darkening) { v = absHi[k].process (v); }                         // keeps darkening while frozen
                    }
                    else v = absHi[k].process (absLo[k].process (v)) * gMid[k];
                    v += inject * inVec[k] * (inMono + ((k & 1) ? inSide : -inSide)) * 0.35f;
                    line[k][(size_t) lw[k]] = undenorm (v);
                    lw[k] = (lw[k] + 1) & lmask[k];
                }
                // output: pickups, width, balance, post EQ, archetype tilt
                ol *= 0.35f; orr *= 0.35f;
                const float mid = 0.5f * (ol + orr), side = 0.5f * (ol - orr) * p.width;
                float wl = (mid + side) * std::min (1.0f, 1.0f - p.balance), wr = (mid - side) * std::min (1.0f, 1.0f + p.balance);
                wl = eqHi[0].process (eqLo[0].process (outHi[0].process (outLo[0].process (wl))));
                wr = eqHi[1].process (eqLo[1].process (outHi[1].process (outLo[1].process (wr))));
                L[i] = (L[i] * dry + wl * wet) * outG;
                if (nch > 1) R[i] = (R[i] * dry + wr * wet) * outG;
            }
        }

    private:
        static constexpr int numAp = 8;
        double sr = 48000;
        std::vector<float> line[N]; int lmask[N] {}, lw[N] {}; double len[N] {};
        float inVec[N] {}, outL[N] {}, outR[N] {}, gMid[N] {};
        Biquad absLo[N], absHi[N];
        std::vector<float> pre[2]; int pmask = 0, pw = 0;
        std::vector<float> ap[2][numAp]; int apLen[2][numAp] {}, apW[2][numAp] {};
        Biquad lc[2][4], hc[2][4], eqLo[2], eqHi[2], outLo[2], outHi[2];
        Nonlinear::Stage stage[2];
        float env = 0, compEnv = 0, decayMul = 1, modPh = 0, darkGain = 1, lastDecayMul = -1, lastDecayS = -1;
        Params applied;

        static int stageModel (int t) { return t == tube ? Nonlinear::glow : t == solidState ? Nonlinear::console : t == tapeIn ? Nonlinear::tape : Nonlinear::tape; }
        static float stageChar (int t) { return t == tube ? 0.5f : t == solidState ? 0.95f : 0.6f; }
        float driveDb() const { return p.input == clean ? 0.0f : (p.input == solidState ? 5.0f : (p.input == tube ? 0.0f : 3.0f)) + p.inputDrive * 18.0f; }   // 50 % = audible (about 1-3 % THD at -18 dBFS)
        static int cutStages (int slope) { return slope >= 2 ? 2 : 1; }
        OnePole lc1[2], hc1[2];
        void setCuts (int c)
        {
            const double lo = clamp ((double) p.lowCut, 20.0, 1000.0), hi = clamp ((double) p.highCut, 1000.0, 20000.0);
            // 6 dB/oct: one-pole-equivalent via a low-Q biquad pair; 12: one Butterworth biquad; 24: two (Linkwitz-Riley Q)
            lc1[c].set (sr, lo); hc1[c].set (sr, hi);
            if (p.lowSlope == 1) lc[c][0].set (Biquad::highpass, sr, lo, 0.7071);
            else { lc[c][0].set (Biquad::highpass, sr, lo, 0.5412); lc[c][1].set (Biquad::highpass, sr, lo, 1.3066); }
            if (p.highSlope == 1) hc[c][0].set (Biquad::lowpass, sr, hi, 0.7071);
            else { hc[c][0].set (Biquad::lowpass, sr, hi, 0.5412); hc[c][1].set (Biquad::lowpass, sr, hi, 1.3066); }
        }
        /** Per-line gains: mid gain from the RT60, shelves so 120 Hz and 6 kHz decay at their own RT60. */
        void decayFilters (float mul)
        {
            if (std::abs (mul - lastDecayMul) < 1.0e-4f && p.decayS == lastDecayS && applied.archetype == p.archetype && ! dirtyArch()) return;
            lastDecayMul = mul; lastDecayS = p.decayS; archSeen = p.archetype;
            const auto& a = archetype (p.archetype);
            const double rtMid = clamp ((double) p.decayS * mul, 0.05, 600.0), rtLo = rtMid * a.lowRatio, rtHi = rtMid * a.highRatio;
            for (int k = 0; k < N; ++k)
            {
                const double d = len[k];
                auto gdb = [&] (double rt) { return -60.0 * d / (rt * sr); };
                gMid[k] = (float) std::pow (10.0, gdb (rtMid) / 20.0);
                absLo[k].set (Biquad::lowShelf, sr, 250.0, 0.6, gdb (rtLo) - gdb (rtMid));
                absHi[k].set (Biquad::highShelf, sr, a.highCornerHz, 0.6, p.freeze && p.darkening ? -0.15 : gdb (rtHi) - gdb (rtMid));
            }
        }
        int archSeen = -1; bool dirtyArch() const { return archSeen != p.archetype; }
        inline float readLine (int k, double t) const
        {
            const auto& b = line[k]; const int m = lmask[k];
            const double rp = (double) lw[k] - t; const int i = (int) std::floor (rp); const float f = (float) (rp - i);
            const float a0 = b[(size_t) (i & m)], a1 = b[(size_t) ((i + 1) & m)];
            return a0 + (a1 - a0) * f;
        }
        inline float readPre (int c, float t) const
        {
            if (t < 1.0f) return pre[c][(size_t) pw];
            const double rp = (double) pw - t; const int i = (int) std::floor (rp); const float f = (float) (rp - i);
            const float a0 = pre[c][(size_t) (i & pmask)], a1 = pre[c][(size_t) ((i + 1) & pmask)];
            return a0 + (a1 - a0) * f;
        }
        static void hadamard (float* v)
        {
            for (int h = 1; h < N; h <<= 1)
                for (int i = 0; i < N; i += h * 2)
                    for (int j = i; j < i + h; ++j) { const float a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
        }
    };
}
