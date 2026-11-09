#pragma once
// Squelchmoth 1.11 Color & Space, second wave, part 3 (super prompt C6, C7). Standard C++ only, measured by
// tests/color111d_test.cpp. (Hive Rack, C16, hosts other plug-ins and lives in SqColor111d.h.)
//
//   Larva2  Larva Filter 2: up to eight filter stages in series, parallel or 4+4, each with its own response (LP / HP /
//           BP / notch) and model (transistor ladder, diode ladder, Sallen-Key scream, state-variable), driven by the
//           Wing Mod engine (LFO to 100 Hz with custom shapes and five smoothings, Rhythm with groove, Envelope, Random
//           S/H, Step, ADSR) with Freq Mod +-10 oct, Res Mod and an L/R offset to 2 oct, an external key for the
//           envelope, and the seven Analog Style drives before or after
//   Phase2  Twelve Phase 2: 2-24 allpass stages, 72 styles built from twelve phaser archetypes, Classic / Modern colour,
//           feedback around the chain or per pair with a resonance safety limiter, polarity, through-zero mode, the
//           Wing Mod engine with a key input, stereo phase, and Analog Style before or after
//
// Real-time safe after prepare().

#include "SqColor111cDsp.h"

namespace FilterModels
{
    using namespace Eng;
    enum Model { transistor, diode, sallenKey, svf, numModels };
    enum Response { lp, hp, bp, notch, numResponses };
    inline const char* modelName (int m) { static const char* n[] = { "Transistor Ladder", "Diode Ladder", "Sallen-Key Scream", "State Variable" }; return n[clamp (m, 0, 3)]; }
    inline const char* responseName (int r) { static const char* n[] = { "Low-Pass", "High-Pass", "Band-Pass", "Notch" }; return n[clamp (r, 0, 3)]; }
    inline float fastTanh (float x) { x = clamp (x, -3.0f, 3.0f); const float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2); }

    /** One filter stage, one channel. Zero-delay-feedback (TPT) structures; the nonlinear models put a saturator in the
        feedback path the way their circuits do. res 0..1 (self-oscillation near 1 on the ladders). */
    struct Stage
    {
        float s[4] {}; float lastY = 0;
        void reset() { for (auto& v : s) v = 0; lastY = 0; }
        float process (float x, int model, int resp, float g, float res, float drive)
        {
            // g = tan (pi fc / sr), already clamped
            const float G = g / (1.0f + g);
            switch (model)
            {
                case transistor:
                case diode:
                {
                    // four one-pole TPT stages with global feedback (the ladder); the diode ladder couples its poles
                    // (each stage sees half of the next one's state), which is what gives it the squelchier, less
                    // even response; both saturate the input of the ladder
                    const float k = 4.2f * res * (model == diode ? 1.15f : 1.0f) * (1.0f - 0.75f * G);   // the unit delay in the loop adds resonance up high: take it back out
                    const float u = fastTanh ((x - k * lastY) * drive) / drive;
                    float v = u, y[4];
                    for (int i = 0; i < 4; ++i)
                    {
                        float in = v;
                        if (model == diode && i < 3) in -= 0.5f * s[i + 1] * std::min (G, 0.6f);
                        const float t = (in - s[i]) * G; y[i] = t + s[i]; s[i] = undenorm (y[i] + t); v = y[i];
                    }
                    lastY = y[3];
                    if (! (std::abs (lastY) < 1.0e3f)) { reset(); return 0.0f; }   // a state that ran away (never on music; guards automation extremes)
                    switch (resp)
                    {
                        case hp:    return u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3];
                        case bp:    return 4.0f * (y[1] - 2.0f * y[2] + y[3]);
                        case notch: return u - 2.0f * y[1] + 2.0f * y[3] - 2.0f * (y[1] - 2.0f * y[2] + y[3]);
                        default:    return y[3] * (1.0f + 0.5f * res);       // a little make-up so resonance does not hollow the level
                    }
                }
                case sallenKey:
                {
                    // 2-pole Sallen-Key with a clipping feedback path: screams instead of ringing cleanly
                    const float k = 1.12f * res;                     // self-oscillation (the scream) only at the top of the range
                    const float t1 = (x - s[0]) * G; const float y1 = t1 + s[0]; s[0] = undenorm (y1 + t1);
                    const float fbk = fastTanh (k * lastY * 1.5f) / 1.5f;
                    const float in2 = y1 + fbk; const float t2 = (in2 - s[1]) * G; const float y2 = t2 + s[1]; s[1] = undenorm (y2 + t2);
                    lastY = y2;
                    if (! (std::abs (lastY) < 1.0e3f)) { reset(); return 0.0f; }
                    switch (resp)
                    {
                        case hp:    return x - y2;
                        case bp:    return 2.0f * (y1 - y2);
                        case notch: return x - 2.0f * (y1 - y2);
                        default:    return y2 / (1.0f + 0.3f * k);
                    }
                }
                default:
                {
                    // TPT state-variable (clean)
                    const float R = 1.0f - 0.98f * res, h = 1.0f / (1.0f + 2.0f * R * g + g * g);
                    const float yh = (x - (2.0f * R + g) * s[0] - s[1]) * h;
                    const float yb = g * yh + s[0]; s[0] = undenorm (g * yh + yb);
                    const float yl = g * yb + s[1]; s[1] = undenorm (g * yb + yl);
                    switch (resp)
                    {
                        case hp:    return yh;
                        case bp:    return 2.0f * R * yb;
                        case notch: return x - 2.0f * R * yb;
                        default:    return yl;
                    }
                }
            }
        }
    };
}

//==============================================================================================================
namespace Larva2
{
    using namespace Eng;
    enum Routing { series, parallel, split44 };
    inline const char* routingName (int r) { static const char* n[] = { "Series", "Parallel", "4 + 4" }; return n[clamp (r, 0, 2)]; }
    constexpr int maxStages = 8;
    struct Params
    {
        int stages = 2, routing = series;
        int response[maxStages] { 0, 0, 2, 1, 0, 2, 3, 0 }, model[maxStages] { 0, 1, 2, 3, 0, 1, 2, 3 };
        float stageOct[maxStages] {};                     // each stage's offset from Cutoff, octaves
        float cutoffHz = 800, resonance = 0.4f, freqModOct = 2, resMod = 0, lrOffsetOct = 0;
        float mix = 1, outputDb = 0;
        int analog = 0; float analogAmount = 0.5f; bool analogPost = false;
        WingMod::Settings mod;                            // the Wing Mod engine (mode, shape, rate / sync, smoothing, steps, envelope, ADSR)
        bool keyOn = false;
        float inputDb = 0;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            wm.prepare (sr); analog.prepare (sr, block, 1);
            for (auto& v : modL) v.assign ((size_t) block, 0.0f); for (auto& v : modR) v.assign ((size_t) block, 0.0f);
            det.assign ((size_t) block, 0.0f);
            reset();
        }
        void reset() { wm.reset(); analog.reset(); for (auto& c : st) for (auto& s : c) s.reset(); analogOn = -1; cutNow = (float) p.cutoffHz; }
        void setTransport (double bpm, double ppq, bool playing) { wm.setTransport (bpm, ppq, playing); }
        void trigger() { wm.trigger(); }
        void release() { wm.release(); }
        double latencySamples() const { return p.analog > 0 ? analog.latencySamples() : 0.0; }
        float modulatedCutoff() const { return cutNow; }
        float modValue() const { return modNow; }
        int currentStep() const { return wm.currentStep(); }
        float envelopeLevel() const { return wm.envelopeLevel(); }

        /** The response of the stages as set (no modulation, linear models): dB at hz, for the face and the tests. */
        static double stageCutoff (const Params& p, int i, float cutoff) { return clamp ((double) cutoff * std::pow (2.0, (double) p.stageOct[clamp (i, 0, maxStages - 1)]), 16.0, 22000.0); }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = std::min (nch, 2);
            if (p.analog != analogOn) { analogOn = p.analog; if (analogOn > 0) { analog.setVoicing (analogOn - 1); analog.reset(); } }
            wm.s = p.mod;
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* x[2] = { ch[0] + off, ch[nch > 1 ? 1 : 0] + off };
                const float* k[2] = { key != nullptr ? key[0] + off : nullptr, key != nullptr ? key[1] + off : nullptr };
                run (x, nch, (p.keyOn && key != nullptr) ? k : nullptr, m);
            }
        }

    private:
        double sr = 48000; int block = 256, analogOn = -1; float cutNow = 800, modNow = 0;
        WingMod wm; AnalogStyle analog; FilterModels::Stage st[2][maxStages];
        std::vector<float> modL[1], modR[1], det;
        void run (float** x, int nch, const float* const* key, int n)
        {
            const float inG = dbToGain (p.inputDb);
            for (int i = 0; i < n; ++i) det[(size_t) i] = key != nullptr ? 0.5f * (key[0][i] + key[1][i]) : 0.5f * (x[0][i] + x[nch > 1 ? 1 : 0][i]) * inG;
            wm.process (det.data(), modL[0].data(), modR[0].data(), n);
            float dry[2][512];
            for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) { x[c][i] *= inG; dry[c][i] = x[c][i]; }
            if (p.analog > 0 && ! p.analogPost) { analog.setAmount (p.analogAmount); analog.process (x, nch, n); }
            const int ns = clamp (p.stages, 1, maxStages);
            const float nyq = (float) (sr * 0.45), ladderTop = (float) (sr * 0.36);
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < nch; ++c)
                {
                    const float mv = c == 0 ? modL[0][(size_t) i] : modR[0][(size_t) i];
                    const float cut = p.cutoffHz * std::exp2 (p.freqModOct * mv + (c == 1 ? p.lrOffsetOct : 0.0f));
                    const float res = clamp (p.resonance + p.resMod * mv, 0.0f, 0.995f);
                    if (c == 0) { cutNow = cut; modNow = mv; }
                    const float drive = 1.0f + 2.0f * res;
                    float in = x[c][i];
                    float out = 0;
                    auto stage = [&] (int s, float v)
                    {
                        const float fc = clamp (cut * std::exp2 (p.stageOct[s]), 16.0f, p.model[s] <= FilterModels::diode ? ladderTop : nyq);   // the ladders stop at 0.36 sr
                        const float g = std::tan ((float) pi * fc / (float) sr);
                        return st[c][s].process (v, p.model[s], p.response[s], g, res, drive);
                    };
                    if (p.routing == parallel)
                    {
                        for (int s = 0; s < ns; ++s) out += stage (s, in);
                        out /= std::sqrt ((float) ns);
                    }
                    else if (p.routing == split44)
                    {
                        const int a = (ns + 1) / 2; float y1 = in, y2 = in;
                        for (int s = 0; s < a; ++s) y1 = stage (s, y1);
                        for (int s = a; s < ns; ++s) y2 = stage (s, y2);
                        out = ns > a ? 0.7071f * (y1 + y2) : y1;
                    }
                    else { out = in; for (int s = 0; s < ns; ++s) out = stage (s, out); }
                    x[c][i] = std::isfinite (out) ? out : 0.0f;
                }
            }
            if (p.analog > 0 && p.analogPost) { analog.setAmount (p.analogAmount); analog.process (x, nch, n); }
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
            for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) x[c][i] = (dry[c][i] + mx * (x[c][i] - dry[c][i])) * out;
        }
    };
}

//==============================================================================================================
namespace Phase2
{
    using namespace Eng;
    constexpr int maxStages = 24;
    struct Style { std::string name; int stages; bool classic; int resMode; float resonance, centreHz, sweepOct, rateHz, spreadDeg; int shape; bool tz; int polarity; };
    /** 72 styles: twelve phaser archetypes in six variations each. */
    inline const std::vector<Style>& styles()
    {
        static const std::vector<Style> v = []
        {
            struct Arch { const char* name; int stages; bool classic; int resMode; float res, centre, sweep, rate, spread; int shape; bool tz; int pol; };
            const Arch a[12] {
                { "One-Knob Amber 4",  4, true,  0, 0.25f, 700,  3.0f, 0.5f,  0, WingMod::sine,     false, 0 },
                { "Script Amber 4",     4, true,  0, 0.10f, 650,  2.8f, 0.4f,  0, WingMod::sine,     false, 0 },
                { "Optical Vibe 6",     6, true,  0, 0.05f, 900,  2.2f, 4.5f, 20, WingMod::triangle, false, 0 },
                { "Pebble 4",      4, true,  1, 0.55f, 800,  3.2f, 0.3f,  0, WingMod::triangle, false, 0 },
                { "Twin Phase Rack 6",    6, true,  0, 0.40f, 600,  3.5f, 0.2f, 90, WingMod::sine,     false, 0 },
                { "Synth 12",        12, false, 0, 0.60f, 1000, 4.0f, 0.15f, 0, WingMod::triangle, false, 0 },
                { "Dual Rack 8",        8, false, 1, 0.45f, 750,  3.0f, 0.25f, 180, WingMod::sine,   false, 0 },
                { "Barber Pole 10",   10, false, 0, 0.30f, 1200, 4.5f, 0.08f, 120, WingMod::rampUp, false, 0 },
                { "Through-Zero 8",     8, false, 0, 0.20f, 900,  3.0f, 0.2f,  0, WingMod::sine,     true,  1 },
                { "Deep 24",         24, false, 0, 0.50f, 900,  4.0f, 0.1f, 45, WingMod::sine,     false, 0 },
                { "Notch Sweep 2",      2, false, 0, 0.00f, 1000, 3.5f, 0.3f,  0, WingMod::sine,     false, 1 },
                { "Odd Seven",        7, true,  1, 0.35f, 850,  3.0f, 0.35f, 60, WingMod::triangle, false, 0 } };
            const char* var[6] = { "", " Slow", " Fast", " Deep", " Subtle", " Wide" };
            std::vector<Style> out;
            for (auto& x : a)
                for (int k = 0; k < 6; ++k)
                {
                    Style s { std::string (x.name) + var[k], x.stages, x.classic, x.resMode, x.res, x.centre, x.sweep, x.rate, x.spread, x.shape, x.tz, x.pol };
                    switch (k)
                    {
                        case 1: s.rateHz *= 0.4f; break;
                        case 2: s.rateHz *= 2.8f; break;
                        case 3: s.resonance = std::min (0.85f, s.resonance + 0.3f); s.sweepOct += 1.0f; break;
                        case 4: s.resonance *= 0.4f; s.sweepOct *= 0.5f; break;
                        case 5: s.spreadDeg = std::min (180.0f, s.spreadDeg + 90.0f); break;
                        default: break;
                    }
                    out.push_back (s);
                }
            return out;
        }();
        return v;
    }
    enum ResMode { aroundChain, perPair };
    struct Params
    {
        int stages = 12; bool classic = false; int resMode = aroundChain; int polarity = 0; bool throughZero = false;
        float centreHz = 900, sweepOct = 3, resonance = 0.5f, spreadDeg = 90, depth = 1, mix = 0.5f, outputDb = 0;
        int analog = 0; float analogAmount = 0.5f; bool analogPost = false;
        WingMod::Settings mod; bool keyOn = false;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            wm.prepare (sr); analog.prepare (sr, block, 1);
            for (auto& v : mod) v.assign ((size_t) block, 0.0f); det.assign ((size_t) block, 0.0f);
            reset();
        }
        void reset() { wm.reset(); analog.reset(); for (auto& c : z) for (auto& v : c) v = 0; for (auto& c : zr) for (auto& v : c) v = 0; fb[0] = fb[1] = 0; limG = 1; analogOn = -1; }
        void setTransport (double bpm, double ppq, bool playing) { wm.setTransport (bpm, ppq, playing); }
        double latencySamples() const { return p.analog > 0 ? analog.latencySamples() : 0.0; }
        float sweepNow() const { return sweepPos; }
        float limiterDb() const { return gainToDb (limG); }
        int currentStep() const { return wm.currentStep(); }
        /** First-order allpass coefficient for a break frequency. */
        float coef (float hz) const { const float t = std::tan ((float) pi * clamp (hz, 10.0f, (float) (sr * 0.45)) / (float) sr); return (t - 1.0f) / (t + 1.0f); }
        /** Stage break frequencies spread: Classic spaces them over a ratio (like matched-but-not-identical cells). */
        static float stageRatio (const Params& p, int i, int n) { if (! p.classic || n < 2) return 1.0f; return std::pow (2.0f, 1.2f * ((float) i / (float) (n - 1) - 0.5f)); }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = std::min (nch, 2);
            if (p.analog != analogOn) { analogOn = p.analog; if (analogOn > 0) { analog.setVoicing (analogOn - 1); analog.reset(); } }
            wm.s = p.mod; wm.s.stereoPhaseDeg = p.spreadDeg;
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* x[2] = { ch[0] + off, ch[nch > 1 ? 1 : 0] + off };
                const float* k[2] = { key != nullptr ? key[0] + off : nullptr, key != nullptr ? key[1] + off : nullptr };
                run (x, nch, (p.keyOn && key != nullptr) ? k : nullptr, m);
            }
        }

    private:
        double sr = 48000; int block = 256, analogOn = -1; float fb[2] {}, limG = 1, sweepPos = 0.5f;
        WingMod wm; AnalogStyle analog; float z[2][maxStages] {}, zr[2][maxStages] {};
        std::vector<float> mod[2], det;
        void run (float** x, int nch, const float* const* key, int n)
        {
            for (int i = 0; i < n; ++i) det[(size_t) i] = key != nullptr ? 0.5f * (key[0][i] + key[1][i]) : 0.5f * (x[0][i] + x[nch > 1 ? 1 : 0][i]);
            wm.process (det.data(), mod[0].data(), mod[1].data(), n);
            float dry[2][512];
            for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) dry[c][i] = x[c][i];
            if (p.analog > 0 && ! p.analogPost) { analog.setAmount (p.analogAmount); analog.process (x, nch, n); }
            const int ns = clamp (p.stages, 2, maxStages);
            // resonance safety: the feedback that is allowed shrinks with the number of stages, and a limiter on the loop
            const float resMax = 0.95f - 0.012f * (float) ns;
            const float res = clamp (p.resonance, 0.0f, 1.0f) * resMax * (p.polarity ? -1.0f : 1.0f);
            const float limA = (float) (1.0 - std::exp (-1.0 / (0.002 * sr))), limR = (float) (1.0 - std::exp (-1.0 / (0.15 * sr)));
            const float refA = coef (p.centreHz);
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < nch; ++c)
                {
                    const float mv = mod[c][(size_t) i];                                    // bipolar -1..1
                    if (c == 0) sweepPos = 0.5f + 0.5f * mv;
                    const float f0 = p.centreHz * std::exp2 (0.5f * p.sweepOct * mv);
                    float v = x[c][i];
                    float in = v + res * fb[c];
                    if (p.classic) in = FilterModels::fastTanh (in * 0.8f) * 1.25f;            // the OTA cells colour the loop
                    float y = in, mid = in;
                    for (int s = 0; s < ns; ++s)
                    {
                        const float a = coef (f0 * stageRatio (p, s, ns));
                        const float o = a * y + z[c][s]; z[c][s] = undenorm (y - a * o); y = o;
                        if (s == ns / 2 - 1) mid = y;
                    }
                    // Res Mode: feedback from the end of the chain, or from its middle (the resonance lands between notches)
                    fb[c] = p.resMode == perPair ? mid : y;
                    // limiter on the loop
                    const float pk = std::abs (fb[c]);
                    const float want = pk > 0.9f ? 0.9f / pk : 1.0f;
                    limG += (want < limG ? limA : limR) * (want - limG);
                    fb[c] *= limG;
                    float wet = y;
                    if (p.throughZero)
                    {
                        // through-zero: against a fixed chain at the centre, so the notches collapse when the sweep crosses it
                        float yr = v;
                        for (int s = 0; s < ns; ++s) { const float o = refA * yr + zr[c][s]; zr[c][s] = undenorm (yr - refA * o); yr = o; }
                        x[c][i] = 0.5f * (y + (p.polarity ? -1.0f : 1.0f) * yr);
                        continue;
                    }
                    x[c][i] = 0.5f * (v + (p.polarity ? -1.0f : 1.0f) * p.depth * wet) * 2.0f / (1.0f + p.depth);
                }
            }
            if (p.analog > 0 && p.analogPost) { analog.setAmount (p.analogAmount); analog.process (x, nch, n); }
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
            for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) x[c][i] = (dry[c][i] + mx * (x[c][i] - dry[c][i])) * out;
        }
    };
}
