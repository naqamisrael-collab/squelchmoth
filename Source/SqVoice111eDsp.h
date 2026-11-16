#pragma once
// Squelchmoth 1.11 Voice, second wave, part 1 (super prompt V8, V11, V12, V13). Measured by tests/voice111e_test.cpp.
//
//   SirenVerb   vocal reverb: seven algorithms (Hall, Plate, Room, Chamber, Spring, Ambience, Shimmer) on an 8-line
//               feedback delay network (the Plate is Foil Plate's engine), pre-verb FX (Pitch, Throat, Tube, De-Ess,
//               Reverse), post-verb FX (a pitch-tracked Auto-EQ that cuts the singer's fundamental and 2nd harmonic out
//               of the tail, Tone, Compressor, Gate, Width), mono / ping-pong echo, ducking, Assist and Dice
//   MicCloset   microphone modelling: 44 response archetypes (low contour, proximity, presence, dips, air, top roll-off,
//               colour, self-noise), source -> target conversion, distance, polar pattern, low cut, pad
//   Larynx      vocal-tract modelling in the spectral domain: the voice's own envelope (cepstrum) and excitation are
//               separated; the glottal source (hard / soft, pulse width, breathiness and its band) and the tract (length
//               and an 8-point Kelly-Lochbaum shape) are re-modelled; stereo, level matched, guarded against whistling
//   Exhale      breathiness: Moth Tune's harmonic / noise split, breath from off (-inf) to +12 dB, its tone, and
//               consonant protection, without touching the harmonic tone
//
// Real-time safe after prepare(). Uses the shared voice core (Moth Tune's tracker) - JUCE core only.

#include "SqVoiceCore.h"
#include "SqFoilPlateDsp.h"
#include "SqColor111cDsp.h"
#include <complex>
#include <random>

namespace VoiceFx
{
    using namespace Eng;

    /** Two-grain delay-line pitch shifter (mono). Ratio 0.25..4; 40 ms grains crossfaded. */
    struct GrainShift
    {
        std::vector<float> buf; int mask = 0, w = 0; double ph = 0, sr = 48000; float grain = 1920;
        void prepare (double rate) { sr = rate; int n = 1; while (n < (int) (0.2 * sr)) n <<= 1; buf.assign ((size_t) n, 0.0f); mask = n - 1; grain = (float) (0.040 * sr); reset(); }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; ph = 0; }
        float read (double d) const
        {
            const double p = (double) w - d; const int i = (int) std::floor (p); const float f = (float) (p - i);
            const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)]; return a + (b - a) * f;
        }
        float process (float x, float ratio)
        {
            buf[(size_t) (w & mask)] = x;
            // delay grows (ratio < 1) or shrinks (ratio > 1) across a grain; two taps half a grain apart
            ph += (1.0 - ratio) / grain; ph -= std::floor (ph);
            const double p2 = ph + 0.5 - std::floor (ph + 0.5);
            const float g1 = (float) std::sin (pi * ph), g2 = (float) std::sin (pi * p2);
            const float y = read (2.0 + ph * grain) * g1 + read (2.0 + p2 * grain) * g2;
            ++w; return y;
        }
    };
}

//==============================================================================================================
namespace SirenVerb
{
    using namespace Eng;
    enum Algo { hall, plate, room, chamber, spring, ambience, shimmer, numAlgos };
    inline const char* algoName (int a) { static const char* n[] = { "Hall", "Plate", "Room", "Chamber", "Spring", "Ambience", "Shimmer" }; return n[clamp (a, 0, (int) numAlgos - 1)]; }
    struct AlgoShape { float minMs, maxMs; float diffusion, earlyLevel, earlySpreadMs, modScale, hiDampScale, loRatio; };
    inline const AlgoShape& shape (int a)
    {
        static const AlgoShape s[] = {
            { 38, 160, 0.70f, 0.45f, 90, 1.0f, 1.00f, 1.10f },    // Hall: long lines, sparse wide early field
            { 20,  60, 0.62f, 0.00f,  0, 1.0f, 1.00f, 1.00f },    // Plate (Foil Plate engine; values unused)
            { 11,  42, 0.60f, 0.85f, 28, 0.6f, 1.25f, 0.95f },    // Room: short lines, dense early reflections
            { 21,  75, 0.78f, 0.60f, 45, 0.7f, 1.10f, 1.05f },    // Chamber: very diffuse, even
            {  9,  33, 0.40f, 0.10f, 10, 0.3f, 1.40f, 0.80f },    // Spring: small tank + dispersive drip
            {  6,  24, 0.65f, 0.95f, 18, 0.5f, 1.15f, 0.90f },    // Ambience: mostly early energy, short tail
            { 40, 165, 0.72f, 0.35f, 80, 1.2f, 0.85f, 1.00f },    // Shimmer: hall with an octave-up loop
        };
        return s[clamp (a, 0, (int) numAlgos - 1)];
    }
    enum DelayMode { delayOff, delayMono, delayPingPong };

    struct Params
    {
        int algo = hall;
        float size = 0.5f, decayS = 2.0f, preDelayMs = 20, damping = 0.5f, early = 0.5f, modRate = 0.6f, modDepth = 0.3f, width = 1.0f;
        float lowCutHz = 120, highCutHz = 12000, mix = 0.25f, outputDb = 0;
        // pre-verb FX
        float prePitchSt = 0, preThroat = 0, preTube = 0, preDeEss = 0, reverseMs = 400; bool reverse = false;
        // post-verb FX
        bool autoEq = false; float autoEqDb = 9, tone = 0, comp = 0, gate = 0, duck = 0, shimmerAmt = 0.35f, shimmerSt = 12;
        // echo
        int delayMode = delayOff; float delayMs = 375, delayFb = 0.3f, delayLevel = 0.3f;
    };

    class Processor
    {
    public:
        static constexpr int N = 8;
        Params p;

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            for (int i = 0; i < N; ++i) { int len = 1; while (len < (int) (0.40 * sr) + 64) len <<= 1; line[i].assign ((size_t) len, 0.0f); lmask[i] = len - 1; }
            int pd = 1; while (pd < (int) (0.52 * sr) + 64) pd <<= 1; for (auto& b : pre) b.assign ((size_t) pd, 0.0f); pmask = pd - 1;
            int ed = 1; while (ed < (int) (2.1 * sr) + 64) ed <<= 1; for (auto& b : echo) b.assign ((size_t) ed, 0.0f); emask = ed - 1;
            int rv = 1; while (rv < (int) (2.2 * sr) + 64) rv <<= 1; revBuf.assign ((size_t) rv, 0.0f); rmask = rv - 1;
            for (int c = 0; c < 2; ++c) for (int k = 0; k < numAp; ++k) ap[c][k].assign ((size_t) (0.03 * sr) + 8, 0.0f);
            for (int c = 0; c < 2; ++c) for (int k = 0; k < numDisp; ++k) disp[c][k] = 0;
            plateProc.prepare (sr, block);
            shift.prepare (sr); for (auto& s : preShift) s.prepare (sr);
            core.prepare (sr, block);
            mono.assign ((size_t) block, 0.0f); dryL.assign ((size_t) block, 0.0f); dryR.assign ((size_t) block, 0.0f); wetL.assign ((size_t) block, 0.0f); wetR.assign ((size_t) block, 0.0f); plateL.assign ((size_t) block, 0.0f); plateR.assign ((size_t) block, 0.0f);
            configure (true); reset();
        }
        void reset()
        {
            for (int i = 0; i < N; ++i) { std::fill (line[i].begin(), line[i].end(), 0.0f); lp[i] = 0; ls[i] = 0; }
            for (auto& b : pre) std::fill (b.begin(), b.end(), 0.0f);
            for (auto& b : echo) std::fill (b.begin(), b.end(), 0.0f);
            std::fill (revBuf.begin(), revBuf.end(), 0.0f);
            for (int c = 0; c < 2; ++c) for (int k = 0; k < numAp; ++k) { std::fill (ap[c][k].begin(), ap[c][k].end(), 0.0f); apW[c][k] = 0; }
            for (int c = 0; c < 2; ++c) { for (auto* b : { &lc[c], &hc[c], &tLo[c], &tHi[c], &thr1[c], &thr2[c], &deEssF[c], &eq1[c], &eq2[c], &echoHc[c] }) b->reset(); dsEnv[c] = 0; }
            lw = pw = ew = rw = 0; modPh = 0; compEnv = 0; gateEnv = 0; gateG = 1; dryEnv = 0; f0 = 0; revPh = 0;
            plateProc.reset(); shift.reset(); for (auto& s : preShift) s.reset(); core.reset();
            echoFb[0] = echoFb[1] = 0;
        }
        void configure (bool force = false)
        {
            const auto& a = shape (p.algo);
            if (force || p.algo != ap0.algo || p.size != ap0.size)
            {
                static const float frac[N] = { 0.0f, 0.137f, 0.271f, 0.398f, 0.523f, 0.659f, 0.811f, 1.0f };
                const float maxMs = a.minMs + (a.maxMs - a.minMs) * (0.35f + 0.65f * clamp (p.size, 0.0f, 1.0f));
                const float minMs = a.minMs * (0.6f + 0.4f * clamp (p.size, 0.0f, 1.0f));
                for (int i = 0; i < N; ++i) len[i] = std::max (24.0, (minMs + (maxMs - minMs) * frac[i]) * 0.001 * sr * (1.0 + 0.0173 * ((i * 7) % 5)));
                static const float apMs[numAp] = { 1.31f, 2.27f, 3.71f, 5.53f, 7.97f, 11.3f };
                for (int c = 0; c < 2; ++c) for (int k = 0; k < numAp; ++k) apLen[c][k] = std::max (2, (int) (apMs[k] * (0.5f + clamp (p.size, 0.0f, 1.0f)) * (1.0f + 0.11f * c) * 0.001 * sr));
                static const float eTap[numEarly] = { 0.07f, 0.13f, 0.21f, 0.29f, 0.38f, 0.49f, 0.61f, 0.74f, 0.86f, 1.0f };
                for (int k = 0; k < numEarly; ++k) { earlyD[k] = std::max (1.0, eTap[k] * a.earlySpreadMs * (0.5 + p.size) * 0.001 * sr); earlyG[k] = (1.0f - 0.6f * eTap[k]) * ((k % 3 == 1) ? -1.0f : 1.0f); earlyPan[k] = (k % 2 == 0) ? 0.85f : 0.15f; }
            }
            if (force || p.lowCutHz != ap0.lowCutHz || p.highCutHz != ap0.highCutHz)
                for (int c = 0; c < 2; ++c) { lc[c].set (Biquad::highpass, sr, clamp (p.lowCutHz, 20.0f, 2000.0f), 0.7071); hc[c].set (Biquad::lowpass, sr, clamp (p.highCutHz, 1000.0f, 20000.0f), 0.7071); }
            if (force || p.tone != ap0.tone)
                for (int c = 0; c < 2; ++c) { tLo[c].set (Biquad::lowShelf, sr, 400, 0.7, -6.0 * p.tone); tHi[c].set (Biquad::highShelf, sr, 3500, 0.7, 6.0 * p.tone); }
            if (force || p.preThroat != ap0.preThroat)
                for (int c = 0; c < 2; ++c) { thr1[c].set (Biquad::peak, sr, 320, 0.9, 6.0 * p.preThroat); thr2[c].set (Biquad::peak, sr, 2600, 1.0, -6.0 * p.preThroat); }
            if (force) for (int c = 0; c < 2; ++c) { deEssF[c].set (Biquad::highpass, sr, 5000, 0.7); echoHc[c].set (Biquad::lowpass, sr, 5500, 0.7); }
            // decay: per-line gain for RT60, one-pole high damping
            const float T = clamp (p.decayS, 0.1f, 30.0f);
            const float hiT = T * clamp (1.05f - 0.85f * p.damping * a.hiDampScale, 0.08f, 1.0f);
            for (int i = 0; i < N; ++i)
            {
                const double d = len[i] / sr;
                g[i] = (float) std::pow (10.0, -3.0 * d / (T * a.loRatio));
                const double gh = std::pow (10.0, -3.0 * d / hiT);
                // one-pole lowpass whose response at 4 kHz is gh / g (high band decays at hiT)
                const double ratio = clamp (gh / std::max (1.0e-6, (double) g[i]), 0.02, 1.0);
                const double wc = 2.0 * pi * 4000.0 / sr;
                // |H(4k)| = ratio for H = (1-a) / (1 - a z^-1): solve for a
                const double c = std::cos (wc), r2 = ratio * ratio;
                const double A = r2 - 1.0, B = 2.0 * (1.0 - r2 * c), C = r2 - 1.0;   // A a^2 + B a + C = 0 (from (1-a)^2 = r2 (1 - 2 a c + a^2))
                double aa = 0;
                if (std::abs (A) > 1e-12) { const double disc = std::max (0.0, B * B - 4 * A * C); aa = (-B + std::sqrt (disc)) / (2 * A); if (aa < 0 || aa > 0.999) aa = (-B - std::sqrt (disc)) / (2 * A); }
                damp[i] = (float) clamp (aa, 0.0, 0.995);
            }
            auto& fp = plateProc.p;
            fp.archetype = 0; fp.decayS = clamp (T, 0.3f, 60.0f); fp.preDelayMs = 0; fp.mix = 1.0f; fp.highCut = 20000; fp.lowCut = 20;
            fp.mod = clamp (p.modDepth, 0.0f, 1.0f); fp.modRateHz = clamp (p.modRate, 0.05f, 5.0f); fp.width = 1; fp.input = FoilPlate::clean; fp.outputDb = 0;
            fp.eqHighDb = -8.0f * p.damping + 2.0f;
            ap0 = p;
        }
        int latencySamples() const { return 0; }
        float liveHz() const { return f0; }
        float compGrDb() const { return compGr; }

        void process (float* const* ch, int nch, int n)
        {
            configure();
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* x[2] = { ch[0] + off, ch[nch > 1 ? 1 : 0] + off };
                run (x, nch, m);
            }
        }

    private:
        static constexpr int numAp = 6, numEarly = 10, numDisp = 12;
        double sr = 48000; int block = 256;
        std::vector<float> line[N]; int lmask[N] {}; double len[N] {}; float g[N] {}, damp[N] {}, lp[N] {}, ls[N] {}; int lw = 0;
        std::vector<float> pre[2]; int pmask = 0, pw = 0;
        std::vector<float> echo[2]; int emask = 0, ew = 0; float echoFb[2] {};
        std::vector<float> revBuf; int rmask = 0, rw = 0; double revPh = 0;
        std::vector<float> ap[2][numAp]; int apLen[2][numAp] {}, apW[2][numAp] {};
        float disp[2][numDisp] {};
        double earlyD[numEarly] {}; float earlyG[numEarly] {}, earlyPan[numEarly] {};
        Biquad lc[2], hc[2], tLo[2], tHi[2], thr1[2], thr2[2], deEssF[2], eq1[2], eq2[2], echoHc[2];
        float dsEnv[2] {};
        double modPh = 0; float compEnv = 0, compGr = 0, gateEnv = 0, gateG = 1, dryEnv = 0, f0 = 0;
        FoilPlate::Processor plateProc; VoiceFx::GrainShift shift, preShift[2];
        SqVoiceCore::Core core;
        std::vector<float> mono, dryL, dryR, wetL, wetR, plateL, plateR;
        Params ap0;

        float readLine (int i, double d) const
        {
            const double pos = (double) lw - d; const int k = (int) std::floor (pos); const float f = (float) (pos - k);
            const float a = line[i][(size_t) (k & lmask[i])], b = line[i][(size_t) ((k + 1) & lmask[i])]; return a + (b - a) * f;
        }
        float readPre (int c, double d) const
        {
            const double pos = (double) pw - d; const int k = (int) std::floor (pos); const float f = (float) (pos - k);
            const float a = pre[c][(size_t) (k & pmask)], b = pre[c][(size_t) ((k + 1) & pmask)]; return a + (b - a) * f;
        }
        float diffuse (int c, float v, float gain)
        {
            for (int k = 0; k < numAp; ++k)
            {
                auto& b = ap[c][k]; const int L = apLen[c][k]; int& w = apW[c][k];
                const float d = b[(size_t) w];
                const float u = v + gain * d; b[(size_t) w] = undenorm (u); v = d - gain * u;
                if (++w >= L) w = 0;
            }
            return v;
        }

        void run (float** x, int nch, int n)
        {
            const auto& a = shape (p.algo);
            // the singer's pitch for the Auto-EQ (mono analysis of the dry input)
            for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (x[0][i] + x[nch > 1 ? 1 : 0][i]);
            core.analyse (mono.data(), n);
            const auto& sn = core.latest();
            if (sn.voiced && sn.hz > 50) f0 = f0 <= 0 ? sn.hz : f0 + 0.3f * (sn.hz - f0);
            if (p.autoEq && f0 > 0)
                for (int c = 0; c < 2; ++c) { eq1[c].set (Biquad::peak, sr, f0, 5.0, -p.autoEqDb); eq2[c].set (Biquad::peak, sr, 2.0 * f0, 4.0, -0.66 * p.autoEqDb); }
            const float shiftRatio = std::pow (2.0f, clamp (p.prePitchSt, -24.0f, 24.0f) / 12.0f), shimRatio = std::pow (2.0f, p.shimmerSt / 12.0f);
            const float tubeG = 1.0f + 6.0f * p.preTube;
            const float dsThr = dbToGain (-30.0f + 22.0f * (1.0f - p.preDeEss));
            const double preD = clamp (p.preDelayMs, 0.0f, 500.0f) * 0.001 * sr;
            const float modInc = (float) (2.0 * pi * clamp (p.modRate, 0.02f, 8.0f) / sr), modAmt = (float) (clamp (p.modDepth, 0.0f, 1.0f) * a.modScale * 0.0011 * sr);
            const float diff = a.diffusion;
            const float eAmt = clamp (p.early, 0.0f, 1.0f) * a.earlyLevel;
            const double echoD = clamp (p.delayMs, 1.0f, 2000.0f) * 0.001 * sr;
            const float envA = (float) (1.0 - std::exp (-1.0 / (0.005 * sr))), envR = (float) (1.0 - std::exp (-1.0 / (0.15 * sr)));
            const float gRel = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
            const double revLen = clamp (p.reverseMs, 60.0f, 1000.0f) * 0.001 * sr;
            for (int i = 0; i < n; ++i)
            {
                float in[2] = { x[0][i], x[nch > 1 ? 1 : 0][i] };
                dryL[(size_t) i] = in[0]; dryR[(size_t) i] = in[1];
                const float dmag = 0.5f * (std::abs (in[0]) + std::abs (in[1]));
                dryEnv += (dmag > dryEnv ? envA : envR) * (dmag - dryEnv);
                // ---- pre-verb FX
                for (int c = 0; c < 2; ++c)
                {
                    float v = in[c];
                    if (p.preThroat != 0) v = thr2[c].process (thr1[c].process (v));
                    if (p.preTube > 0.001f) v = FilterTanh (v * tubeG) / std::sqrt (tubeG);
                    if (p.preDeEss > 0.001f)
                    {
                        const float hp = deEssF[c].process (v); dsEnv[c] += (std::abs (hp) > dsEnv[c] ? envA : envR) * (std::abs (hp) - dsEnv[c]);
                        const float red = dsEnv[c] > dsThr ? dsThr / dsEnv[c] : 1.0f; v = (v - hp) + hp * red;
                    }
                    if (std::abs (p.prePitchSt) > 0.01f) v = preShift[c].process (v, shiftRatio);
                    in[c] = v;
                }
                if (p.reverse)
                {
                    // the send plays backwards in overlapping chunks: each note swells in instead of starting dry
                    revBuf[(size_t) (rw & rmask)] = 0.5f * (in[0] + in[1]);
                    revPh += 1.0 / revLen; if (revPh >= 1.0) revPh -= 1.0;
                    const double p2 = revPh + 0.5 - std::floor (revPh + 0.5);
                    auto rd = [&] (double ph) { const double back = 2.0 * ph * revLen; const int k = (int) ((double) rw - back); return revBuf[(size_t) (k & rmask)]; };   // read head runs backwards at normal speed
                    const float r = rd (revPh) * (float) std::sin (pi * revPh) + rd (p2) * (float) std::sin (pi * p2);
                    in[0] = in[1] = r; ++rw;
                }
                // ---- echo (into the reverb and the wet)
                float echoOut[2] = { 0, 0 };
                if (p.delayMode != delayOff)
                {
                    const auto rdE = [&] (int c) { const double pos = (double) ew - echoD; const int k = (int) std::floor (pos); const float f = (float) (pos - k);
                                                   return echo[c][(size_t) (k & emask)] + (echo[c][(size_t) ((k + 1) & emask)] - echo[c][(size_t) (k & emask)]) * f; };
                    const float e0 = rdE (0), e1 = rdE (1); echoOut[0] = e0; echoOut[1] = e1;
                    const float fb = clamp (p.delayFb, 0.0f, 0.95f);
                    if (p.delayMode == delayPingPong)
                    {
                        echo[0][(size_t) (ew & emask)] = undenorm (echoHc[0].process (0.5f * (in[0] + in[1]) + fb * e1));
                        echo[1][(size_t) (ew & emask)] = undenorm (echoHc[1].process (fb * e0));
                    }
                    else for (int c = 0; c < 2; ++c) echo[c][(size_t) (ew & emask)] = undenorm (echoHc[c].process (in[c] + fb * (c == 0 ? e0 : e1)));
                    ++ew;
                    for (int c = 0; c < 2; ++c) in[c] += 0.5f * p.delayLevel * echoOut[c];
                }
                // ---- pre-delay and the early field
                for (int c = 0; c < 2; ++c) pre[c][(size_t) (pw & pmask)] = in[c];
                ++pw;
                float send[2] = { readPre (0, preD + 1), readPre (1, preD + 1) };
                float er[2] = { 0, 0 };
                if (eAmt > 0.001f)
                    for (int k = 0; k < numEarly; ++k) { const float v = 0.5f * (readPre (0, preD + 1 + earlyD[k]) + readPre (1, preD + 1 + earlyD[k])) * earlyG[k]; er[0] += v * earlyPan[k]; er[1] += v * (1.0f - earlyPan[k]); }
                float wet[2] = { 0, 0 };
                if (p.algo == plate) { plateL[(size_t) i] = send[0]; plateR[(size_t) i] = send[1]; }   // the plate runs once per block, below
                else
                {
                    // ---- the FDN: input diffusion, 8 lines, Householder mix, per-line damping and decay, modulated reads
                    float d0 = diffuse (0, send[0], diff), d1 = diffuse (1, send[1], diff);
                    if (p.algo == spring)
                        for (int k = 0; k < numDisp; ++k) { const float gg = 0.62f; const float u0 = d0 + gg * disp[0][k]; const float o0 = disp[0][k] - gg * u0; disp[0][k] = undenorm (u0); d0 = o0;
                                                            const float u1 = d1 + gg * disp[1][k]; const float o1 = disp[1][k] - gg * u1; disp[1][k] = undenorm (u1); d1 = o1; }
                    modPh += modInc; if (modPh > 2.0 * pi) modPh -= 2.0 * pi;
                    float o[N]; float sum = 0;
                    for (int k = 0; k < N; ++k)
                    {
                        const double md = modAmt * std::sin (modPh + k * 0.785398);
                        float v = readLine (k, len[k] + md + 2.0);
                        lp[k] = v + damp[k] * (lp[k] - v);           // one-pole low-pass: highs decay faster
                        v = lp[k] * g[k];
                        o[k] = v; sum += v;
                    }
                    const float hh = 2.0f / N * sum;
                    float shim = 0;
                    if (p.algo == shimmer) shim = shift.process (0.5f * (o[0] + o[3]), shimRatio) * clamp (p.shimmerAmt, 0.0f, 0.9f);
                    for (int k = 0; k < N; ++k)
                    {
                        float fbv = o[k] - hh;
                        fbv += (k % 2 == 0 ? d0 : d1) * 0.35f;
                        if (p.algo == shimmer && (k == 1 || k == 6)) fbv += shim;
                        line[k][(size_t) (lw & lmask[k])] = undenorm (fbv);
                    }
                    ++lw;
                    for (int k = 0; k < N; ++k) { wet[0] += (k % 2 == 0 ? o[k] : -0.6f * o[k]); wet[1] += (k % 2 == 1 ? o[k] : -0.6f * o[k]); }
                    wet[0] *= 0.42f; wet[1] *= 0.42f;
                }
                wet[0] += eAmt * er[0] * 0.6f; wet[1] += eAmt * er[1] * 0.6f;
                wet[0] += p.delayLevel * echoOut[0]; wet[1] += p.delayLevel * echoOut[1];
                wetL[(size_t) i] = wet[0]; wetR[(size_t) i] = wet[1];
            }
            if (p.algo == plate)
            {
                float* pc[2] = { plateL.data(), plateR.data() };
                plateProc.process (pc, 2, nullptr, n);
                for (int i = 0; i < n; ++i) { wetL[(size_t) i] += plateL[(size_t) i]; wetR[(size_t) i] += plateR[(size_t) i]; }
            }
            // ---- post-verb FX (per sample, on the wet)
            const float cThr = dbToGain (-12.0f - 24.0f * p.comp);
            const float gThr = dbToGain (-70.0f + 50.0f * p.gate);
            const float duckRange = 24.0f * p.duck;
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
            const float dryG = mx < 0.5f ? 1.0f : 2.0f * (1.0f - mx), wetG = mx < 0.5f ? 2.0f * mx : 1.0f;
            for (int i = 0; i < n; ++i)
            {
                float l = wetL[(size_t) i], r = wetR[(size_t) i];
                l = hc[0].process (lc[0].process (l)); r = hc[1].process (lc[1].process (r));
                if (p.autoEq && f0 > 0) { l = eq2[0].process (eq1[0].process (l)); r = eq2[1].process (eq1[1].process (r)); }
                if (p.tone != 0) { l = tHi[0].process (tLo[0].process (l)); r = tHi[1].process (tLo[1].process (r)); }
                const float mag = std::max (std::abs (l), std::abs (r));
                if (p.comp > 0.001f)
                {
                    compEnv += (mag > compEnv ? envA * 2 : envR) * (mag - compEnv);
                    const float gr = compEnv > cThr ? std::pow (cThr / compEnv, 0.66f) : 1.0f;
                    const float mk = 1.0f + 0.5f * p.comp;
                    l *= gr * mk; r *= gr * mk; compGr = gainToDb (gr);
                }
                if (p.gate > 0.001f)
                {
                    gateEnv += (mag > gateEnv ? envA : envR) * (mag - gateEnv);
                    const float want = gateEnv > gThr ? 1.0f : 0.0f;
                    gateG += (want > gateG ? 0.02f : gRel) * (want - gateG);
                    l *= gateG; r *= gateG;
                }
                if (p.duck > 0.001f)
                {
                    const float lev = clamp ((gainToDb (dryEnv) + 40.0f) / 30.0f, 0.0f, 1.0f);
                    const float dg = dbToGain (-duckRange * lev); l *= dg; r *= dg;
                }
                // width on the wet
                const float m = 0.5f * (l + r), s = 0.5f * (l - r) * clamp (p.width, 0.0f, 2.0f);
                l = m + s; r = m - s;
                x[0][i] = (dryL[(size_t) i] * dryG + l * wetG) * out;
                if (nch > 1) x[1][i] = (dryR[(size_t) i] * dryG + r * wetG) * out;
            }
        }
        static float FilterTanh (float v) { v = clamp (v, -3.0f, 3.0f); const float v2 = v * v; return v * (27.0f + v2) / (27.0f + 9.0f * v2); }
    };
}

//==============================================================================================================
namespace MicCloset
{
    using namespace Eng;
    enum Colour { none, tube, transformer, ribbon, solid };
    struct Mic
    {
        const char* name; const char* family;
        float lowHz, lowDb;            // low contour shelf
        float proximityDb;             // max proximity boost at 1 cm on a cardioid (dB at 100 Hz)
        float presHz, presDb, presQ;   // presence peak
        float dipHz, dipDb, dipQ;      // a characteristic dip (or second peak)
        float airHz, airDb;            // air shelf
        float topHz;                   // top roll-off (low-pass, 20000 = none)
        int colour; float colourAmt;   // colour stage and how much it adds at Colour 100 %
        float noiseDb;                 // self-noise (dBFS at 0 dB gain)
        int defaultPattern;            // 0 cardioid, 1 omni, 2 figure-8, 3 supercardioid
    };
    inline const std::vector<Mic>& mics()
    {
        static const std::vector<Mic> m {
            { "Flat (measurement)",                 "Reference",  100,  0.0f, 0.0f, 3000,  0.0f, 1.0f, 1000,  0.0f, 1.0f, 10000,  0.0f, 20000, none,        0.0f, -110, 1 },
            { "Large-Diaphragm Tube Condenser",     "Condenser",  120,  1.0f, 7.0f, 5500,  3.5f, 0.9f,  800, -0.8f, 1.2f, 12000,  2.0f, 20000, tube,        0.6f,  -95, 0 },
            { "Large-Diaphragm Tube, Dark",         "Condenser",  150,  2.0f, 7.0f, 4200,  1.5f, 1.0f, 2500, -1.0f, 1.5f, 10000, -1.5f, 17000, tube,        0.7f,  -94, 0 },
            { "Large-Diaphragm FET Condenser",      "Condenser",  100,  0.5f, 6.5f, 6500,  4.0f, 0.8f,  350, -1.0f, 1.0f, 12000,  2.5f, 20000, transformer, 0.3f,  -98, 0 },
            { "Modern Bright Studio Condenser",     "Condenser",   80,  0.0f, 6.0f, 9000,  4.5f, 0.7f,  600, -0.5f, 1.0f, 14000,  3.5f, 20000, solid,       0.1f, -100, 0 },
            { "Vintage Bottle Condenser",           "Condenser",  130,  1.5f, 7.5f, 4800,  2.5f, 1.0f, 1500,  1.0f, 0.9f,  9000, -1.0f, 16000, tube,        0.8f,  -92, 0 },
            { "Small-Diaphragm Pencil",             "Condenser",   80, -1.0f, 4.0f, 10000, 2.0f, 0.8f,  200, -0.5f, 1.0f, 15000,  1.5f, 20000, solid,       0.1f, -100, 0 },
            { "Small-Diaphragm Omni Pencil",        "Condenser",   60,  0.5f, 0.0f, 9000,  1.0f, 0.8f, 1000,  0.0f, 1.0f, 14000,  1.0f, 20000, solid,       0.05f, -101, 1 },
            { "Multi-Pattern Studio Condenser",     "Condenser",  100,  0.8f, 6.5f, 5000,  2.5f, 0.9f, 3500, -1.2f, 2.0f, 11000,  1.5f, 20000, transformer, 0.3f,  -97, 0 },
            { "Budget Large Condenser",             "Condenser",  110,  0.0f, 6.0f, 7500,  5.5f, 1.2f,  300, -1.5f, 1.0f, 12000,  2.0f, 19000, solid,       0.2f,  -88, 0 },
            { "USB Desktop Condenser",              "Condenser",  130, -1.5f, 5.5f, 5000,  3.0f, 1.0f,  250, -2.0f, 1.0f, 10000,  0.5f, 16000, solid,       0.15f, -84, 0 },
            { "Dynamic Stage Handheld",             "Dynamic",    150, -3.0f, 9.0f, 5000,  5.0f, 0.9f, 9000, -3.0f, 1.5f, 10000, -4.0f, 14000, none,        0.0f, -120, 0 },
            { "Supercardioid Stage Dynamic",        "Dynamic",    170, -3.5f, 9.5f, 4000,  4.0f, 1.0f, 7500, -2.0f, 1.2f, 10000, -3.0f, 14500, none,        0.0f, -120, 3 },
            { "Broadcast End-Address Dynamic",      "Dynamic",    120,  0.0f, 3.5f, 3000,  1.0f, 0.6f, 9000, -2.0f, 1.0f, 10000, -2.0f, 16000, transformer, 0.2f, -120, 0 },
            { "Broadcast Large Dynamic",            "Dynamic",    110,  2.0f, 5.0f, 3500,  2.0f, 0.7f,  700, -1.5f, 1.0f,  9000, -4.5f, 13000, transformer, 0.2f, -120, 0 },
            { "Instrument Dynamic",                 "Dynamic",    200, -4.0f, 9.0f, 6000,  6.0f, 1.1f, 2000, -1.0f, 1.0f, 10000, -3.0f, 14500, none,        0.0f, -120, 0 },
            { "Kick Drum Dynamic",                  "Dynamic",     60,  4.0f, 6.0f, 3500,  5.5f, 1.2f,  400, -8.0f, 0.8f, 10000, -2.0f, 12000, none,        0.0f, -120, 0 },
            { "Vintage Bullet Dynamic",             "Dynamic",    300, -6.0f, 4.0f, 2500,  4.0f, 0.6f, 6000, -4.0f, 0.8f,  8000, -8.0f,  7000, transformer, 0.5f, -110, 1 },
            { "Vintage Pill Dynamic",               "Dynamic",    200, -2.0f, 6.0f, 4000,  3.0f, 0.8f, 1000,  1.5f, 0.8f,  9000, -4.0f, 11000, transformer, 0.4f, -115, 0 },
            { "1950s Ribbon",                       "Ribbon",     100,  1.5f, 10.0f, 2000, -1.0f, 0.6f, 5000, -2.0f, 0.8f,  8000, -6.0f, 12000, ribbon,     0.5f, -105, 2 },
            { "1930s Ribbon",                       "Ribbon",     120,  2.5f, 11.0f, 1800, -2.0f, 0.5f, 4000, -3.0f, 0.8f,  7000, -8.0f,  9000, ribbon,     0.6f, -103, 2 },
            { "Modern Active Ribbon",               "Ribbon",      80,  1.0f, 9.0f, 4000,  0.5f, 0.6f, 8000, -1.0f, 0.8f, 11000, -3.0f, 16000, ribbon,     0.2f, -110, 2 },
            { "Broadcast Ribbon",                   "Ribbon",     140,  2.0f, 8.0f, 2500,  1.0f, 0.7f, 6000, -3.0f, 1.0f,  9000, -5.0f, 13000, ribbon,     0.5f, -104, 0 },
            { "Lavalier",                           "Specialty",  150, -2.0f, 0.0f, 6500,  4.0f, 0.8f,  500, -1.0f, 1.0f, 11000,  2.0f, 18000, solid,       0.1f,  -90, 1 },
            { "Headset Boom",                       "Specialty",  200, -4.0f, 8.0f, 4000,  4.5f, 1.0f, 1200, -1.0f, 1.0f, 10000, -1.0f, 15000, solid,       0.1f,  -92, 0 },
            { "Shotgun",                            "Specialty",  120, -2.5f, 3.0f, 7000,  3.5f, 0.9f, 2000, -1.5f, 1.2f, 12000,  1.5f, 18000, solid,       0.1f,  -96, 3 },
            { "Boundary / Plate",                   "Specialty",   80,  1.0f, 0.0f, 3000,  2.0f, 0.6f,  800,  1.0f, 0.8f, 10000,  0.0f, 17000, solid,       0.1f,  -95, 1 },
            { "Smartphone Built-In",                "Device",     350, -9.0f, 2.0f, 2500,  5.0f, 1.1f, 6000, -3.0f, 1.0f,  8000, -6.0f,  9000, solid,       0.3f,  -80, 1 },
            { "Laptop Built-In",                    "Device",     400,-10.0f, 0.0f, 3000,  4.0f, 1.2f, 1500, -4.0f, 1.5f,  8000, -8.0f,  8000, solid,       0.3f,  -78, 1 },
            { "Webcam",                             "Device",     300, -8.0f, 1.0f, 3500,  4.5f, 1.2f, 1000, -3.0f, 1.2f,  8000, -7.0f,  9000, solid,       0.3f,  -79, 1 },
            { "Telephone Handset",                  "Device",     400,-14.0f, 3.0f, 1800,  6.0f, 0.9f, 1000,  2.0f, 0.7f,  3400,-14.0f,  3400, transformer, 0.7f,  -75, 0 },
            { "Two-Way Radio",                      "Device",     500,-16.0f, 2.0f, 2000,  7.0f, 1.2f, 1000,  3.0f, 0.7f,  3000,-16.0f,  3000, solid,       0.9f,  -70, 0 },
            { "Megaphone Horn",                     "Device",     600,-16.0f, 0.0f, 1600,  9.0f, 1.5f, 3000,  5.0f, 1.5f,  4000,-15.0f,  4500, solid,       1.0f,  -72, 0 },
            { "Vintage Carbon Mic",                 "Device",     350,-12.0f, 2.0f, 1500,  7.0f, 0.8f, 2500,  3.0f, 0.9f,  4000,-12.0f,  4000, solid,       1.0f,  -65, 1 },
            { "Tube Talkback Speaker",              "Device",     250, -7.0f, 3.0f, 2200,  5.0f, 1.0f,  600,  2.0f, 1.0f,  6000, -6.0f,  7000, tube,        0.8f,  -82, 0 },
            { "Crystal Lo-Fi Mic",                  "Device",     450,-10.0f, 1.0f, 2800,  8.0f, 1.5f, 1000, -2.0f, 1.0f,  5000,-10.0f,  6000, solid,       0.8f,  -74, 1 },
            { "Squelchmoth Velvet",                 "Original",   120,  2.0f, 7.0f, 5000,  2.0f, 0.8f, 2800, -2.0f, 1.5f, 13000,  3.5f, 20000, tube,        0.5f,  -98, 0 },
            { "Squelchmoth Glass",                  "Original",    90, -1.0f, 6.0f, 8000,  3.0f, 0.7f,  400, -2.0f, 0.9f, 15000,  5.0f, 20000, solid,       0.1f, -101, 0 },
            { "Squelchmoth Smoke",                  "Original",   160,  3.0f, 8.0f, 3000,  1.5f, 0.6f, 6000, -3.0f, 1.0f,  8000, -4.0f, 14000, ribbon,     0.7f, -100, 2 },
            { "Squelchmoth Gold",                   "Original",   110,  1.5f, 7.0f, 4200,  3.0f, 0.9f, 1200,  1.0f, 1.0f, 11000,  2.5f, 20000, transformer, 0.6f,  -97, 0 },
            { "Squelchmoth Iron",                   "Original",   140,  3.5f, 9.0f, 2800,  2.5f, 0.7f, 5000, -2.0f, 1.0f,  9000, -2.0f, 15000, transformer, 0.9f,  -96, 0 },
            { "Squelchmoth Halo",                   "Original",   100,  0.5f, 5.0f, 6000,  2.0f, 0.6f, 2000, -1.0f, 1.0f, 12000,  4.5f, 20000, tube,        0.3f,  -99, 1 },
            { "Squelchmoth Mud Hut",                "Original",   250,  4.0f, 6.0f, 1200,  3.0f, 0.7f, 4000, -5.0f, 0.8f,  6000, -9.0f,  9000, tube,        0.9f,  -90, 1 },
            { "Squelchmoth Night Radio",            "Original",   300, -6.0f, 5.0f, 2400,  6.0f, 1.0f,  900,  2.0f, 0.8f,  6000, -9.0f,  6500, tube,        0.9f,  -80, 0 },
            { "Squelchmoth Cathedral Air",          "Original",    70,  1.0f, 3.0f, 9000,  2.0f, 0.6f,  500, -1.0f, 0.9f, 16000,  6.0f, 20000, solid,       0.1f, -100, 1 },
        };
        return m;
    }
    inline int numMics() { return (int) mics().size(); }
    inline const char* patternName (int p) { static const char* n[] = { "Cardioid", "Omni", "Figure-8", "Supercardioid" }; return n[clamp (p, 0, 3)]; }
    inline float patternProximity (int p) { static const float k[] = { 1.0f, 0.0f, 1.45f, 1.12f }; return k[clamp (p, 0, 3)]; }
    /** Proximity boost (dB) at a distance (cm) for a pattern, on a mic whose 1 cm boost is maxDb. Zero beyond ~60 cm. */
    inline float proximityDb (float maxDb, int pattern, float cm)
    {
        const float d = clamp (cm, 1.0f, 300.0f);
        const float k = clamp (std::log (60.0f / d) / std::log (60.0f), 0.0f, 1.0f);
        return maxDb * patternProximity (pattern) * k;
    }
    /** Built-in proximity at the distance the response archetype was published for (15 cm). */
    constexpr float refCm = 15.0f;

    struct Params
    {
        int source = 0, target = 1, pattern = -1;      // pattern -1: the target mic's own
        float distanceCm = refCm, lowCutHz = 20, padDb = 0, colour = 0.5f, noise = 0, mix = 1, outputDb = 0;
        bool sourceProx = true;                        // the source was recorded at Distance too (proximity is converted, not stacked)
    };

    /** The response chain of one mic (as filters) - shelves and peaks are invertible; the top roll-off is target-only. */
    struct Shape
    {
        Biquad lo, pres, dip, air, prox, top, top2;
        bool hasTop = false;
        void set (const Mic& m, double sr, float proxDb, float sign, bool withTop)
        {
            lo.set (Biquad::lowShelf, sr, m.lowHz, 0.7, sign * m.lowDb);
            pres.set (Biquad::peak, sr, m.presHz, m.presQ, sign * m.presDb);
            dip.set (Biquad::peak, sr, m.dipHz, m.dipQ, sign * m.dipDb);
            air.set (Biquad::highShelf, sr, m.airHz, 0.7, sign * m.airDb);
            prox.set (Biquad::lowShelf, sr, 260, 0.6, sign * proxDb);
            hasTop = withTop && m.topHz < 19500;
            if (hasTop) { top.set (Biquad::lowpass, sr, m.topHz, 0.6); top2.set (Biquad::lowpass, sr, m.topHz * 1.05, 0.54); }
        }
        void reset() { for (auto* b : { &lo, &pres, &dip, &air, &prox, &top, &top2 }) b->reset(); }
        float process (float x)
        {
            x = air.process (dip.process (pres.process (lo.process (prox.process (x)))));
            if (hasTop) x = top2.process (top.process (x));
            return x;
        }
        double magnitudeDb (double hz, double sr) const
        {
            double d = lo.magnitudeDb (hz, sr) + pres.magnitudeDb (hz, sr) + dip.magnitudeDb (hz, sr) + air.magnitudeDb (hz, sr) + prox.magnitudeDb (hz, sr);
            if (hasTop) d += top.magnitudeDb (hz, sr) + top2.magnitudeDb (hz, sr);
            return d;
        }
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; (void) maxBlock;
            Nonlinear::warm (Nonlinear::glow, 0.4f); Nonlinear::warm (Nonlinear::transformer, 0.4f); Nonlinear::warm (Nonlinear::transformer, 0.7f); Nonlinear::warm (Nonlinear::console, 0.4f);
            for (int c = 0; c < 2; ++c) stage[c].prepare (sr);
            applied = {}; applied.target = -1; configure (true); reset();
        }
        void reset() { for (int c = 0; c < 2; ++c) { src[c].reset(); tgt[c].reset(); lowCut[c].reset(); lowCut2[c].reset(); stage[c].reset(); } }
        int effectivePattern() const { return p.pattern >= 0 ? p.pattern : mics()[(size_t) clamp (p.target, 0, numMics() - 1)].defaultPattern; }
        void configure (bool force = false)
        {
            if (! force && p.source == applied.source && p.target == applied.target && p.pattern == applied.pattern && p.distanceCm == applied.distanceCm
                && p.lowCutHz == applied.lowCutHz && p.sourceProx == applied.sourceProx && p.colour == applied.colour && p.padDb == applied.padDb) return;
            const auto& s = mics()[(size_t) clamp (p.source, 0, numMics() - 1)];
            const auto& t = mics()[(size_t) clamp (p.target, 0, numMics() - 1)];
            const int pat = effectivePattern();
            // the published curves are at refCm; a distance closer (or further) adds (or removes) proximity on the target;
            // the source's own proximity at that distance is taken back out when the source was recorded there too
            const float tProx = proximityDb (t.proximityDb, pat, p.distanceCm) - proximityDb (t.proximityDb, pat, refCm);
            const float sProx = p.sourceProx ? proximityDb (s.proximityDb, s.defaultPattern, p.distanceCm) - proximityDb (s.proximityDb, s.defaultPattern, refCm) : 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                src[c].set (s, sr, sProx, -1.0f, false);
                tgt[c].set (t, sr, tProx, 1.0f, true);
                if (p.lowCutHz > 21) { lowCut[c].set (Biquad::highpass, sr, p.lowCutHz, 0.54); lowCut2[c].set (Biquad::highpass, sr, p.lowCutHz, 1.31); }
                stage[c].model = t.colour == tube ? Nonlinear::glow : t.colour == transformer ? Nonlinear::transformer : t.colour == ribbon ? Nonlinear::transformer : Nonlinear::console;
                stage[c].character = t.colour == ribbon ? 0.7f : 0.4f; stage[c].bias = t.colour == tube ? 0.3f : 0.1f; stage[c].rebuild();
            }
            // the colour stage: how hard the target's electronics are driven (Pad backs it off)
            colourDrive = dbToGain (18.0f * clamp (p.colour, 0.0f, 1.0f) * t.colourAmt - p.padDb);
            colourOn = t.colour != none && p.colour * t.colourAmt > 0.01f;
            noiseG = dbToGain (t.noiseDb + 6.0f);
            applied = p;
        }
        /** Response (dB) of the conversion at hz (no colour, no noise): for the face and the tests. */
        double responseDb (double hz) const
        {
            double d = src[0].magnitudeDb (hz, sr) + tgt[0].magnitudeDb (hz, sr);
            if (p.lowCutHz > 21) d += lowCut[0].magnitudeDb (hz, sr) + lowCut2[0].magnitudeDb (hz, sr);
            return d;
        }
        double targetOnlyDb (double hz) const { return tgt[0].magnitudeDb (hz, sr); }

        void process (float* const* ch, int nch, int n)
        {
            configure();
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f), nz = noiseG * clamp (p.noise, 0.0f, 1.0f);
            const float pad = dbToGain (-p.padDb);
            for (int c = 0; c < std::min (nch, 2); ++c)
            {
                float* x = ch[c];
                for (int i = 0; i < n; ++i)
                {
                    const float dry = x[i];
                    float v = tgt[c].process (src[c].process (dry));
                    if (p.lowCutHz > 21) v = lowCut2[c].process (lowCut[c].process (v));
                    if (colourOn)
                    {
                        stage[c].drive = colourDrive;
                        const float y = stage[c].process (v * pad) / std::max (1.0e-3f, colourDrive * pad);
                        v = v + clamp (p.colour, 0.0f, 1.0f) * (y - v);
                    }
                    if (nz > 0) { rng = rng * 1664525u + 1013904223u; v += nz * ((float) (rng >> 9) / 8388608.0f - 1.0f); }
                    x[i] = (dry + mx * (v - dry)) * out;
                }
            }
        }
    private:
        double sr = 48000; Params applied; Shape src[2], tgt[2]; Biquad lowCut[2], lowCut2[2];
        Nonlinear::Stage stage[2]; float colourDrive = 1, noiseG = 0; bool colourOn = false; uint32_t rng = 0x1234567u;
    };
}

//==============================================================================================================
namespace Larynx
{
    using namespace Eng;
    constexpr int fftN = 1024, hop = 256, bins = fftN / 2 + 1, points = 8;
    struct Params
    {
        float length = 1.0f;            // tract length ratio 0.6..1.6 (shorter = formants up)
        float shape[points] {};         // -1..1 per point, glottis (0) to lips (7): narrower / wider than neutral
        float glottal = 0.0f;           // -1 soft .. +1 hard (source tilt)
        float pulseWidth = 0.5f;        // 0.1..0.9 open quotient: wider = rounder, more fundamental
        float breath = 0.0f;            // 0..1 aspiration added
        float breathHz = 3000;          // centre of the aspiration band
        float precision = 0.5f;         // 0..1 envelope detail (cepstral lifter)
        float mix = 1.0f, outputDb = 0;
        bool levelMatch = true;
    };

    /** Kelly-Lochbaum lossy tube: |U_lips / U_glottis| (dB) for a tract of 8 sections with areas A and total length L. */
    inline double tubeDb (const double* area, double L, double hz)
    {
        const double c = 35000.0, l = L / points;
        const std::complex<double> k (2.0 * pi * hz / c, -0.0006 * std::sqrt (std::max (1.0, hz)) * (1.0 + 2000.0 / std::max (50.0, hz)) * 0.25);
        std::complex<double> M00 = 1, M01 = 0, M10 = 0, M11 = 1;
        for (int s = 0; s < points; ++s)
        {
            const std::complex<double> cs = std::cos (k * l), sn = std::sin (k * l), j (0, 1);
            const double Z = 1.0 / area[s];
            const std::complex<double> a = cs, b = j * Z * sn, cc = j * sn / Z, d = cs;
            const std::complex<double> n00 = M00 * a + M01 * cc, n01 = M00 * b + M01 * d, n10 = M10 * a + M11 * cc, n11 = M10 * b + M11 * d;
            M00 = n00; M01 = n01; M10 = n10; M11 = n11;
        }
        return -20.0 * std::log10 (std::max (1.0e-9, std::abs (M11)));
    }

    class Channel
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            inBuf.assign (fftN, 0.0f); outBuf.assign (fftN * 2, 0.0f); spec.assign (fftN, {}); cep.assign (fftN, {});
            win.resize (fftN); for (int i = 0; i < fftN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) pi * i / fftN);
            env.assign (bins, 0.0f); envOut.assign (bins, 0.0f); ratioDb.assign (bins, 0.0f); dryHist.assign (fftN * 2, 0.0f);
            reset();
        }
        void reset()
        {
            std::fill (inBuf.begin(), inBuf.end(), 0.0f); std::fill (outBuf.begin(), outBuf.end(), 0.0f); std::fill (dryHist.begin(), dryHist.end(), 0.0f);
            inW = 0; outR = 0; hopCount = 0; dw = 0; lvl = 1; rng = 0x9e3779b9u;
        }
        /** Recomputes the tract-shape ratio (vs a neutral tube of the same length) - message thread or once per block. */
        void setShape (const Params& p)
        {
            double areaS[points], area0[points];
            for (int s = 0; s < points; ++s) { areaS[s] = std::exp (1.3 * clamp (p.shape[s], -1.0f, 1.0f)); area0[s] = 1.0; }
            const double L = 17.5;
            for (int b = 0; b < bins; ++b)
            {
                const double hz = std::max (20.0, (double) b * sr / fftN);
                const double r = tubeDb (areaS, L, hz) - tubeDb (area0, L, hz);
                ratioDb[(size_t) b] = (float) clamp (r, -18.0, 18.0);          // whistle guard: the shape never adds more than 18 dB anywhere
            }
            // smooth across 5 bins so no single sharp peak survives
            std::vector<float> t (ratioDb); for (int b = 2; b < bins - 2; ++b) ratioDb[(size_t) b] = 0.1f * t[(size_t) b - 2] + 0.2f * t[(size_t) b - 1] + 0.4f * t[(size_t) b] + 0.2f * t[(size_t) b + 1] + 0.1f * t[(size_t) b + 2];
        }
        float process (float x, const Params& p)
        {
            inBuf[(size_t) inW] = x; inW = (inW + 1) % fftN;
            dryHist[(size_t) dw] = x; dw = (dw + 1) % (int) dryHist.size();
            if (++hopCount >= hop) { hopCount = 0; frame (p); }
            const float y = outBuf[(size_t) outR]; outBuf[(size_t) outR] = 0; outR = (outR + 1) % (int) outBuf.size();
            const float dry = dryHist[(size_t) ((dw - 1 - latency() + (int) dryHist.size() * 2) % (int) dryHist.size())];
            return dry + clamp (p.mix, 0.0f, 1.0f) * (y - dry);
        }
        static int latency() { return fftN - 1; }   // the frame that ends on this sample starts playing on the next one
    private:
        double sr = 48000; int inW = 0, outR = 0, hopCount = 0, dw = 0; float lvl = 1; uint32_t rng = 1;
        std::vector<float> inBuf, outBuf, win, env, envOut, ratioDb, dryHist; std::vector<std::complex<float>> spec, cep;

        void frame (const Params& p)
        {
            double eIn = 0;
            for (int i = 0; i < fftN; ++i) { const float v = inBuf[(size_t) ((inW + i) % fftN)]; spec[(size_t) i] = v * win[(size_t) i]; eIn += (double) v * v * win[(size_t) i] * win[(size_t) i]; }
            Murm::fft (spec.data(), fftN, false);
            // cepstral envelope: log magnitude -> cepstrum -> lifter -> back
            for (int b = 0; b < fftN; ++b) { const float m = std::abs (spec[(size_t) b]); cep[(size_t) b] = std::log (m + 1.0e-7f); }
            Murm::fft (cep.data(), fftN, true);
            const int lifter = 14 + (int) (46.0f * clamp (p.precision, 0.0f, 1.0f));
            for (int q = lifter; q <= fftN - lifter; ++q) cep[(size_t) q] = 0;
            Murm::fft (cep.data(), fftN, false);
            for (int b = 0; b < bins; ++b) env[(size_t) b] = cep[(size_t) b].real();          // natural-log envelope
            // the new envelope: the tract stretched (length) and reshaped (8 points)
            const float L = clamp (p.length, 0.5f, 1.8f);
            for (int b = 0; b < bins; ++b)
            {
                const float src = (float) b * L;                                            // shorter tract (L < 1) reads lower bins: formants rise
                const int i0 = (int) src; const float f = src - (float) i0;
                const float e = i0 + 1 < bins ? env[(size_t) i0] + f * (env[(size_t) i0 + 1] - env[(size_t) i0]) : env[(size_t) bins - 1] - 0.02f * (src - (float) (bins - 1));
                envOut[(size_t) b] = e + ratioDb[(size_t) b] * 0.115129f;                  // dB -> ln
            }
            // excitation: the whitened spectrum, re-tilted by the glottal source (hard / soft, pulse width)
            const float tiltDbOct = 4.0f * clamp (p.glottal, -1.0f, 1.0f) - 3.0f * (clamp (p.pulseWidth, 0.1f, 0.9f) - 0.5f) * 2.0f;
            const float breath = clamp (p.breath, 0.0f, 1.0f), bHz = clamp (p.breathHz, 500.0f, 9000.0f);
            for (int b = 1; b < bins; ++b)
            {
                const float hz = (float) b * (float) sr / fftN;
                const float tilt = dbToGain (clamp (tiltDbOct * std::log2 (std::max (60.0f, hz) / 1000.0f), -9.0f, 9.0f));   // the source tilt, held within +-9 dB
                const float pwBoost = hz < 350.0f ? 1.0f + 0.8f * (clamp (p.pulseWidth, 0.1f, 0.9f) - 0.5f) : 1.0f;
                const float gain = std::exp (envOut[(size_t) b] - env[(size_t) b]) * tilt * pwBoost;
                auto v = spec[(size_t) b] * gain;
                if (breath > 0.001f)
                {
                    // aspiration: noise under the new envelope, banded around the breath frequency
                    const float band = std::exp (-0.5f * std::pow (std::log2 (hz / bHz) / 1.1f, 2.0f));
                    rng = rng * 1664525u + 1013904223u; const float ph = (float) (rng >> 8) / 16777216.0f * 6.2831853f;
                    const float mag = std::exp (envOut[(size_t) b]) * breath * band * 0.6f;
                    v += std::complex<float> (mag * std::cos (ph), mag * std::sin (ph));
                }
                spec[(size_t) b] = v;
                if (b < fftN / 2) spec[(size_t) (fftN - b)] = std::conj (v);
            }
            spec[0] = spec[0] * std::exp (envOut[1] - env[1]);
            Murm::fft (spec.data(), fftN, true);
            double eOut = 0; for (int i = 0; i < fftN; ++i) { const float v = spec[(size_t) i].real() * win[(size_t) i]; eOut += (double) v * v; }
            // level matching: the frame's energy back to the input's (smoothed, never above +12 dB of correction)
            float want = 1;
            if (p.levelMatch && eOut > 1.0e-12) want = (float) clamp (std::sqrt (eIn / eOut), 0.25, 4.0);
            lvl += 0.35f * (want - lvl);
            const float g = lvl * dbToGain (p.outputDb) / 1.5f;                         // Hann x Hann at 75 % overlap sums to 1.5
            for (int i = 0; i < fftN; ++i) outBuf[(size_t) ((outR + i) % (int) outBuf.size())] += spec[(size_t) i].real() * win[(size_t) i] * g;
        }
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int) { sr = sampleRate; for (auto& c : chan) { c.prepare (sr); c.setShape (p); } shapeApplied = p; }
        void reset() { for (auto& c : chan) c.reset(); }
        int latencySamples() const { return Channel::latency(); }
        void process (float* const* ch, int nch, int n)
        {
            bool changed = false; for (int s = 0; s < points; ++s) changed = changed || p.shape[s] != shapeApplied.shape[s];
            if (changed) { for (auto& c : chan) c.setShape (p); shapeApplied = p; }
            const int nc = std::min (nch, 2);
            for (int c = 0; c < nc; ++c) for (int i = 0; i < n; ++i) ch[c][i] = chan[c].process (ch[c][i], p);
            // a safety limiter at -0.5 dBFS: a hard glottis or a narrow tract can make spiky peaks from a calm level
            const float ceil = 0.944f, rel = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
            for (int i = 0; i < n; ++i)
            {
                float pk = 0; for (int c = 0; c < nc; ++c) pk = std::max (pk, std::abs (ch[c][i]));
                const float want = pk > ceil ? ceil / pk : 1.0f;
                limG = want < limG ? want : limG + rel * (want - limG);
                for (int c = 0; c < nc; ++c) ch[c][i] *= limG;
            }
        }
    private:
        double sr = 48000; Channel chan[2]; Params shapeApplied; float limG = 1;
    };
}

//==============================================================================================================
namespace Exhale
{
    using namespace Eng;
    struct Params
    {
        float breathDb = 0;             // -60 = off (-inf) .. +12
        float tone = 0;                 // -1 darker .. +1 brighter (on the breath only)
        float consonants = 0.7f;        // 0..1 how much unvoiced consonants are kept as they were
        float bodyDb = 0;               // the harmonic part, -6..+6 (normally left at 0)
        float mix = 1.0f, outputDb = 0;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 2048));
            for (auto& c : core) c.prepare (sr, block);
            for (auto& v : h) v.assign ((size_t) block, 0.0f); for (auto& v : nz) v.assign ((size_t) block, 0.0f);
            lat = core[0].splitLatency();
            for (auto& d : dly) d.assign ((size_t) (lat + block + 8), 0.0f);
            reset();
        }
        void reset() { for (auto& c : core) c.reset(); for (auto& d : dly) std::fill (d.begin(), d.end(), 0.0f); dwi = 0; g = 1; for (int c = 0; c < 2; ++c) { tlo[c].reset(); thi[c].reset(); } applied = -99; }
        int latencySamples() const { return lat; }
        float hnrDb() const { return core[0].latest().hnrDb; }
        bool voiced() const { return core[0].latest().voiced; }
        float breathGainDb() const { return gainToDb (g); }

        void process (float* const* ch, int nch, int n)
        {
            if (p.tone != applied) { for (int c = 0; c < 2; ++c) { tlo[c].set (Biquad::lowShelf, sr, 1500, 0.7, -5.0 * p.tone); thi[c].set (Biquad::highShelf, sr, 5000, 0.7, 6.0 * p.tone); } applied = p.tone; }
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                const int nc = std::min (nch, 2);
                for (int c = 0; c < nc; ++c) { core[c].analyse (ch[c] + off, m); core[c].split (ch[c] + off, h[c].data(), nz[c].data(), m); }
                // the breath gain: Breath when voiced; towards 0 dB (as recorded) on unvoiced consonants, by Consonants
                const bool v = core[0].latest().voiced;
                const float target = p.breathDb <= -59.5f ? 0.0f : dbToGain (clamp (p.breathDb, -60.0f, 12.0f));
                const float want = v ? target : target + clamp (p.consonants, 0.0f, 1.0f) * (1.0f - target);
                const float k = (float) (1.0 - std::exp (-(double) m / (0.012 * sr)));
                const float body = dbToGain (clamp (p.bodyDb, -12.0f, 12.0f)), out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
                for (int c = 0; c < nc; ++c)
                {
                    float* x = ch[c] + off; float gg = g;
                    const int L = (int) dly[c].size();
                    for (int i = 0; i < m; ++i)
                    {
                        gg += k / (float) m * (want - gg);
                        dly[c][(size_t) ((dwi + i) % L)] = x[i];
                        const float dry = dly[c][(size_t) ((dwi + i - lat + L * 4) % L)];
                        float br = nz[c][(size_t) i];
                        if (p.tone != 0) br = thi[c].process (tlo[c].process (br));
                        const float wet = h[c][(size_t) i] * body + br * gg;
                        x[i] = (dry + mx * (wet - dry)) * out;
                    }
                    if (c == nc - 1) { g = gg; dwi = (dwi + m) % L; }
                }
                if (nc == 1 && nch > 1) for (int i = 0; i < m; ++i) ch[1][off + i] = ch[0][off + i];
            }
        }
    private:
        double sr = 48000; int block = 512, lat = 800, dwi = 0; float g = 1, applied = -99;
        SqVoiceCore::Core core[2]; std::vector<float> h[2], nz[2], dly[2]; Biquad tlo[2], thi[2];
    };
}
