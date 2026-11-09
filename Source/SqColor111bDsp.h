#pragma once
// Squelchmoth 1.11 Color & Space, second wave, part 1 (super prompt C8, C9, C12, C13, C14). Standard C++ only, measured
// in isolation by tests/color111b_test.cpp.
//
//   Vellum   vintage broadcast EQ: low shelf-cut / very-low-bell boost, high gentle-low-pass cut / very-high-bell boost,
//            six-step mid with gain-dependent Q, a second (sweepable) mid, Stepped detents, Stereo / Mid / Side / L / R,
//            oversampled Drive with auto gain, and an analyser ring for the face
//   Tremor   tremolo / gate to full depth: LFO or 16-step Rhythm with per-step depth, nine shapes, Feel, Groove, Accent,
//            Env / Gate / Inverse dynamics with an external key, audio-rate Rate Mod, stereo phase, harmonic (split-band)
//            mode, Analog Style drive and Mix
//   Orbit    auto-pan: LFO, Rhythm Step, Rhythm Shape, Ping-Pong, Random, Random Step and Circle (depth + Haas cues),
//            offset +-105 deg, width to 210 deg, audio trigger with divider 1-16 and detector filters, binaural mode,
//            four pan laws and Mix
//   Mirror   micro-pitch widener: five styles, detune and delay in %, Focus crossover, ducked width, mono-compatibility
//            meter (correlation and mono-sum loss in dB)
//   Shutter  1978-style digital delay: Multiply 1/2/4/8x halves the line's clock each step (bandwidth 12 / 6 / 3 / 1.5 kHz),
//            8-bit / 12-bit companding converter colour, two lines to 10 s, six algorithms, LFO 0.1-256 Hz, Freeze and a
//            Smooth or Glitch time change
//
// Real-time safe after prepare(): no allocation, no locks on the audio thread.

#include "SqEngine111Dsp.h"

//==============================================================================================================
namespace Vellum
{
    using namespace Eng;
    inline const float* lowFreqs()  { static const float f[6] = { 35, 60, 100, 150, 220, 330 }; return f; }
    inline const float* midFreqs()  { static const float f[6] = { 700, 1000, 1500, 2300, 3500, 5600 }; return f; }
    inline const float* highFreqs() { static const float f[6] = { 3300, 5600, 8200, 10000, 12000, 15000 }; return f; }
    enum Target { stereo, mid, side, left, right, numTargets };
    inline const char* targetName (int t) { static const char* n[] = { "Stereo", "Mid", "Side", "Left", "Right" }; return n[clamp (t, 0, 4)]; }

    struct Params
    {
        int lowIdx = 2, midIdx = 1, highIdx = 3;
        float lowDb = 0, midDb = 0, highDb = 0;
        bool mid2On = false; float mid2Hz = 3000, mid2Db = 0;
        bool stepped = false;
        float drive = 0;            // 0..1 (0..24 dB into the amp)
        int target = stereo, oversampleLog2 = 2;
        float outputDb = 0;
    };

    /** The gains the EQ really uses (Stepped rounds to the hardware detents: 3 dB low / high, 2 dB mids). */
    inline float detent (float db, float step, bool stepped) { return stepped ? step * std::round (db / step) : db; }
    /** The mid Q law: broad at small gains, narrowing as the gain rises (proportional Q). */
    inline double midQ (float db) { return 0.45 + 0.19 * std::min (8.0f, std::abs (db)); }
    /** The high-cut corner: three octaves above the marked frequency at a touch of cut, on it at full cut. */
    inline double highCutCorner (float f, float cutDb, double sr) { const double a = clamp (-cutDb / 15.0f, 0.0f, 1.0f); return std::min (sr * 0.45, f * std::pow (2.0, 3.0 * (1.0 - a))); }

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            os.prepare (block, p.oversampleLog2, sr); osLog2 = p.oversampleLog2;
            Nonlinear::warm (Nonlinear::console, 0.35f);
            for (auto& s : stage) { s.model = Nonlinear::console; s.character = 0.35f; s.bias = 0.05f; s.prepare (sr * os.factor()); s.rebuild(); }
            for (auto& b : tmp) b.assign ((size_t) block, 0.0f);
            dirty = true; reset();
        }
        void reset()
        {
            for (auto& c : f) for (auto& b : c) b.reset();
            for (auto& s : stage) s.reset(); os.reset(); eIn = eOut = 0; ag = 1;
            std::fill (std::begin (ring), std::end (ring), 0.0f); ringPos = 0;
        }
        double latencySamples() const { return os.latencySamples(); }
        double rate() const { return sr; }

        /** The response the EQ section has now, in dB (faces / tests). */
        double magnitudeDb (double hz) const
        {
            double m = 0; for (int i = 0; i < 4; ++i) if (active[i]) m += f[0][i].magnitudeDb (hz, sr);
            return m;
        }
        float lowDbUsed() const { return detent (p.lowDb, 3, p.stepped); }
        float midDbUsed() const { return detent (p.midDb, 2, p.stepped); }
        float mid2DbUsed() const { return detent (p.mid2Db, 2, p.stepped); }
        float highDbUsed() const { return detent (p.highDb, 3, p.stepped); }

        void process (float* const* ch, int nch, int n)
        {
            nch = std::min (nch, 2);
            if (clamp (p.oversampleLog2, 0, 3) != osLog2) { osLog2 = clamp (p.oversampleLog2, 0, 3); os.setFactorLog2 (osLog2, Oversampler::minimumPhase); for (auto& s : stage) { s.prepare (sr * os.factor()); s.rebuild(); s.reset(); } }
            update();
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* x[2] = { ch[0] + off, ch[nch > 1 ? 1 : 0] + off };
                run (x, nch, m);
            }
        }
        // analyser ring (mono output), read by the face
        float ring[2048] {}; std::atomic<int> ringPos { 0 };

    private:
        double sr = 48000; int block = 256, osLog2 = -1; bool dirty = true;
        Biquad f[2][4]; bool active[4] {};
        Oversampler os; Nonlinear::Stage stage[2]; std::vector<float> tmp[2];
        float eIn = 0, eOut = 0, ag = 1;
        Params last;

        void update()
        {
            const bool same = ! dirty && last.lowIdx == p.lowIdx && last.midIdx == p.midIdx && last.highIdx == p.highIdx && last.lowDb == p.lowDb && last.midDb == p.midDb
                              && last.highDb == p.highDb && last.mid2On == p.mid2On && last.mid2Hz == p.mid2Hz && last.mid2Db == p.mid2Db && last.stepped == p.stepped;
            if (same) return;
            dirty = false; last = p;
            const float lo = lowDbUsed(), md = midDbUsed(), m2 = mid2DbUsed(), hi = highDbUsed();
            const float lf = lowFreqs()[clamp (p.lowIdx, 0, 5)], mf = midFreqs()[clamp (p.midIdx, 0, 5)], hf = highFreqs()[clamp (p.highIdx, 0, 5)];
            for (int c = 0; c < 2; ++c)
            {
                if (lo < 0) f[c][0].set (Biquad::lowShelf, sr, lf, 0.6, lo); else f[c][0].set (Biquad::peak, sr, lf, 0.9, lo);
                f[c][1].set (Biquad::peak, sr, mf, midQ (md), md);
                f[c][2].set (Biquad::peak, sr, clamp (p.mid2Hz, 120.0f, 12000.0f), midQ (m2), m2);
                if (hi < 0) f[c][3].set (Biquad::lowpass, sr, highCutCorner (hf, hi, sr), 0.55); else f[c][3].set (Biquad::peak, sr, hf, 0.8, hi);
            }
            active[0] = std::abs (lo) > 0.01f; active[1] = std::abs (md) > 0.01f; active[2] = p.mid2On && std::abs (m2) > 0.01f; active[3] = std::abs (hi) > 0.01f;
        }
        inline float eq (int c, float x) { for (int i = 0; i < 4; ++i) if (active[i]) x = f[c][i].process (x); return x; }

        void run (float** x, int nch, int n)
        {
            float* a = tmp[0].data(); float* b = tmp[1].data();
            // split into the two signals the EQ works on
            for (int i = 0; i < n; ++i)
            {
                const float l = x[0][i], r = nch > 1 ? x[1][i] : l;
                if (p.target == mid || p.target == side) { a[i] = 0.5f * (l + r); b[i] = 0.5f * (l - r); } else { a[i] = l; b[i] = r; }
            }
            const bool doA = p.target == stereo || p.target == mid || p.target == left, doB = p.target == stereo || p.target == side || p.target == right;
            for (int i = 0; i < n; ++i) { if (doA) a[i] = eq (0, a[i]); if (doB) b[i] = eq (1, b[i]); }
            for (int i = 0; i < n; ++i)
            {
                float l = a[i], r = b[i];
                if (p.target == mid || p.target == side) { l = a[i] + b[i]; r = a[i] - b[i]; }
                a[i] = l; b[i] = r;
            }
            // the amp: always through the oversampler (constant latency), the curve only when Drive is up
            const float drv = dbToGain (24.0f * clamp (p.drive, 0.0f, 1.0f)), agA = (float) (1.0 - std::exp (-1.0 / (0.5 * sr)));
            const bool driven = p.drive > 0.001f;
            if (driven) for (int i = 0; i < n; ++i) eIn += agA * (0.5f * (a[i] * a[i] + b[i] * b[i]) - eIn);
            float* chs[2] = { a, b };
            for (int c = 0; c < 2; ++c)
            {
                float* o = os.up (c, chs[c], n); const int m = n * os.factor();
                if (driven) { auto& st = stage[c]; st.drive = drv; for (int i = 0; i < m; ++i) o[i] = st.process (o[i]); }
                os.down (c, o, n, chs[c]);
            }
            const float out = dbToGain (p.outputDb);
            int rp = ringPos.load();
            for (int i = 0; i < n; ++i)
            {
                float g = 1.0f;
                if (driven)
                {
                    eOut += agA * (0.5f * (a[i] * a[i] + b[i] * b[i]) - eOut);
                    const float t = (eOut > 1e-12f && eIn > 1e-12f) ? clamp (std::sqrt (eIn / eOut), 0.05f, 4.0f) : 1.0f;
                    ag += agA * 4.0f * (t - ag); g = ag;
                }
                else ag = 1;
                x[0][i] = a[i] * g * out; if (nch > 1) x[1][i] = b[i] * g * out;
                ring[rp] = 0.5f * (x[0][i] + x[nch > 1 ? 1 : 0][i]); rp = (rp + 1) & 2047;
            }
            ringPos.store (rp);
        }
    };
}

//==============================================================================================================
// A tiny shared helper: the LFO shapes every modulator here uses, bipolar, phase 0 = the top (open / centre-right).
namespace ModShapes
{
    using namespace Eng;
    inline float shape (int s, float p, float pw, const float* custom, int np, float holdV)
    {
        p -= std::floor (p);
        switch (s)
        {
            case WingMod::sine:     return std::cos (2.0f * (float) pi * p);
            case WingMod::triangle: return 1.0f - 4.0f * (p < 0.5f ? p : 1.0f - p);
            case WingMod::rampUp:   return 2.0f * p - 1.0f;
            case WingMod::rampDown: return 1.0f - 2.0f * p;
            case WingMod::square:   return p < 0.5f ? 1.0f : -1.0f;
            case WingMod::pulse:    return p < clamp (pw, 0.02f, 0.98f) ? 1.0f : -1.0f;
            case WingMod::sampleHold:
            case WingMod::smoothRandom: return holdV;
            case WingMod::custom:
            {
                np = clamp (np, 2, 16);
                const float xx = p * (float) np; const int i0 = (int) xx % np, i1 = (i0 + 1) % np; const float fr = xx - std::floor (xx);
                return custom[i0] + (custom[i1] - custom[i0]) * fr;
            }
            default: return 0;
        }
    }
    struct Rand { uint32_t s = 0x9E3779B9u; float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 8388607.5f - 1.0f; } };
    /** Swing over pairs of cycles: the first of each pair is longer by groove/2. Returns the local phase, the cycle index. */
    inline float swing (double pos, float groove, long long& idx)
    {
        const double pair = std::floor (pos * 0.5), p2 = pos - pair * 2.0, b = 1.0 + 0.5 * clamp (groove, 0.0f, 1.0f);
        const bool second = p2 >= b;
        idx = (long long) pair * 2 + (second ? 1 : 0);
        return (float) (second ? (p2 - b) / (2.0 - b) : p2 / b);
    }
    /** Feel: moves the half-way point of each cycle earlier (-) or later (+): the shape pushes or lays back. */
    inline float feel (float p, float fe)
    {
        const float k = 0.5f + 0.35f * clamp (fe, -1.0f, 1.0f);
        return p < k ? 0.5f * p / k : 0.5f + 0.5f * (p - k) / (1.0f - k);
    }
}

//==============================================================================================================
namespace Tremor
{
    using namespace Eng;
    enum Mode { lfo, rhythm, numModes };
    enum Dyn { dynOff, dynEnv, dynGate, dynInverse, numDyn };
    inline const char* modeName (int m) { static const char* n[] = { "LFO", "Rhythm" }; return n[clamp (m, 0, 1)]; }
    inline const char* dynName (int d)  { static const char* n[] = { "Off", "Env", "Gate", "Inverse" }; return n[clamp (d, 0, 3)]; }

    struct Pattern { const char* name; float v[16]; };
    /** The factory rhythm patterns (per-step depth 0..1); the "steps" text in the state overrides the chosen one. */
    inline const std::vector<Pattern>& patterns()
    {
        static const std::vector<Pattern> p {
            { "Even 16ths",      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
            { "Pulse Accents",   { 1, .4f, .7f, .4f, 1, .4f, .7f, .4f, 1, .4f, .7f, .4f, 1, .4f, .7f, .4f } },
            { "Offbeat Chop",    { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 } },
            { "Gallop",          { 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1 } },
            { "Tresillo",        { 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0 } },
            { "Clave 3-2",       { 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0 } },
            { "Clave 2-3",       { 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 } },
            { "Trance Gate",     { 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1 } },
            { "Stutter End",     { 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 } },
            { "Swell Build",     { .1f, .2f, .3f, .4f, .5f, .6f, .7f, .8f, .9f, 1, 1, 1, 1, 1, 1, 1 } },
            { "Fade Out",        { 1, 1, 1, .9f, .8f, .7f, .6f, .5f, .4f, .35f, .3f, .25f, .2f, .15f, .1f, .05f } },
            { "Half Time",       { 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 } },
            { "Quarter Pump",    { 1, .2f, .2f, .2f, 1, .2f, .2f, .2f, 1, .2f, .2f, .2f, 1, .2f, .2f, .2f } },
            { "Dotted Eighths",  { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 } },
            { "Skip Beat",       { 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0 } },
            { "Bounce",          { 1, .3f, .8f, .3f, .6f, .3f, .9f, .3f, 1, .3f, .8f, .3f, .6f, .3f, .9f, .3f } },
            { "Morse",           { 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0 } },
            { "Ghost Notes",     { 1, .25f, .25f, .6f, .25f, .25f, 1, .25f, .25f, .6f, .25f, .25f, 1, .25f, .6f, .25f } },
            { "Shuffle",         { 1, 0, .7f, 1, 0, .7f, 1, 0, .7f, 1, 0, .7f, 1, 0, .7f, 0 } },
            { "Broken",          { 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1 } },
            { "Sparse",          { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0 } },
            { "Rolling",         { 1, .8f, .6f, .4f, 1, .8f, .6f, .4f, 1, .8f, .6f, .4f, 1, .8f, .6f, .4f } },
            { "Afro Bell",       { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0 } },
            { "Amen Chop",       { 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1 } } };
        return p;
    }

    struct Params
    {
        int mode = lfo, shape = WingMod::sine;
        bool sync = true; int syncIndex = 9; float rateHz = 4;          // syncIndex into WingMod::syncBeats (9 = 1/8)
        float depth = 0.6f, feel = 0, groove = 0, accent = 0, smoothMs = 2, pulseWidth = 0.5f;
        float custom[16] { 1, 0.9f, 0.6f, 0.2f, -0.2f, -0.6f, -0.9f, -1, -1, -0.9f, -0.6f, -0.2f, 0.2f, 0.6f, 0.9f, 1 };
        int customPoints = 16;
        float steps[16] { 1, 0.4f, 0.7f, 0.4f, 1, 0.4f, 0.7f, 0.4f, 1, 0.4f, 0.7f, 0.4f, 1, 0.4f, 0.7f, 0.4f };   // per-step depth
        int numSteps = 16, stepDivision = 6;                                                                       // 1/16
        int dyn = dynOff; float thresholdDb = -30, attackMs = 10, releaseMs = 200;
        float rateModAmt = 0, rateModHz = 5;
        int analog = 0; float analogAmount = 0.5f;                       // 0 off, 1.. = AnalogStyle voicing + 1
        float mix = 1, stereoDeg = 0, outputDb = 0;
        bool harmonic = false; float xoverHz = 800;
        bool keyOn = false, retrigger = false;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, std::min (maxBlock, 512));
            analog.prepare (sr, block, 1); analogOn = -1;
            for (auto& b : tmp) b.assign ((size_t) block, 0.0f);
            dryDelay.assign (64, 0.0f);
            reset();
        }
        void reset()
        {
            ph = 0; fmPh = 0; env = 0; gateOpen = false; gSm[0] = gSm[1] = gSm2[0] = gSm2[1] = 1; lastCycle = -1; held = 0; heldPrev = 0; rnd = {};
            for (auto& c : xo) for (auto& b : c) b.reset(); analog.reset(); std::fill (dryDelay.begin(), dryDelay.end(), 0.0f); dw = 0;
        }
        void setTransport (double bpm, double ppqAtStart, bool isPlaying) { tBpm = bpm > 1 ? bpm : 120; tPpq = ppqAtStart; playing = isPlaying; }
        double latencySamples() const { return p.analog > 0 ? analog.latencySamples() : 0.0; }
        double cycleBeats() const { return p.mode == rhythm ? WingMod::syncBeats (p.stepDivision) : WingMod::syncBeats (p.syncIndex); }
        double cycleHz() const { return (p.mode == rhythm || p.sync) ? tBpm / 60.0 / cycleBeats() : (double) p.rateHz; }
        float currentGain() const { return gNow; }
        int currentStep() const { return stepNow; }
        float envelopeDb() const { return gainToDb (env); }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = std::min (nch, 2);
            if ((p.analog > 0) != (analogOn > 0) || (p.analog > 0 && p.analog != analogOn)) { analogOn = p.analog; if (p.analog > 0) { analog.setVoicing (p.analog - 1); analog.reset(); } }
            for (int c = 0; c < 2; ++c) { xo[c][0].set (Biquad::lowpass, sr, p.xoverHz, 0.7071); xo[c][1].set (Biquad::lowpass, sr, p.xoverHz, 0.7071);
                                          xo[c][2].set (Biquad::highpass, sr, p.xoverHz, 0.7071); xo[c][3].set (Biquad::highpass, sr, p.xoverHz, 0.7071); }
            for (int off = 0; off < n; off += block)
            {
                const int m = std::min (block, n - off);
                float* x[2] = { ch[0] + off, ch[nch > 1 ? 1 : 0] + off };
                const float* k[2] = { key != nullptr ? key[0] + off : nullptr, key != nullptr ? key[1] + off : nullptr };
                chunkOff = off;
                run (x, nch, (p.keyOn && key != nullptr) ? k : nullptr, m);
            }
        }

    private:
        double sr = 48000, tBpm = 120, tPpq = 0, ph = 0, fmPh = 0; bool playing = false; int chunkOff = 0; int block = 256;
        float env = 0, gSm[2] {}, gSm2[2] {}, gNow = 1, held = 0, heldPrev = 0; bool gateOpen = false; long long lastCycle = -1; int stepNow = 0;
        ModShapes::Rand rnd; Biquad xo[2][4]; AnalogStyle analog; int analogOn = -1; std::vector<float> tmp[2]; std::vector<float> dryDelay; int dw = 0;

        void run (float** x, int nch, const float* const* key, int n)
        {
            const float ca = (float) (1.0 - std::exp (-1.0 / (std::max (0.05f, p.attackMs) * 0.001 * sr)));
            const float cr = (float) (1.0 - std::exp (-1.0 / (std::max (1.0f, p.releaseMs) * 0.001 * sr)));
            const float sm = (float) (1.0 - std::exp (-1.0 / (std::max (0.05f, p.smoothMs) * 0.001 * sr)));
            const float thr = dbToGain (p.thresholdDb), thrOff = dbToGain (p.thresholdDb - 3.0f);
            const double beatsPerSample = tBpm / (60.0 * sr);
            const double cb = cycleBeats();
            const float stereo = p.stereoDeg / 360.0f;
            const int ns = clamp (p.numSteps, 1, 16);
            const double cyclesPerBar = std::max (1.0, std::round (4.0 / cb));
            float* wl = tmp[0].data(); float* wr = tmp[1].data();
            for (int i = 0; i < n; ++i)
            {
                const float in0 = x[0][i], in1 = nch > 1 ? x[1][i] : in0;
                // dynamics detector: the key when it is on, else the input
                const float det = key != nullptr ? std::max (std::abs (key[0][i]), std::abs (key[1][i])) : std::max (std::abs (in0), std::abs (in1));
                env += (det > env ? ca : cr) * (det - env); env = undenorm (env);
                bool onset = false;
                if (! gateOpen && env > thr) { gateOpen = true; onset = true; } else if (gateOpen && env < thrOff) gateOpen = false;
                // the phase: locked to the bar while the transport plays and the rate is synced, free otherwise
                double pos;
                const bool locked = playing && (p.sync || p.mode == rhythm);
                if (locked) pos = (tPpq + (double) (chunkOff + i) * beatsPerSample) / cb;
                else
                {
                    if (onset && (p.retrigger || (p.keyOn && key != nullptr))) ph = 0;
                    ph += cycleHz() / sr; if (ph > 1.0e6) ph -= std::floor (ph);
                    pos = ph;
                }
                if (p.rateModAmt > 0.0f)
                {
                    fmPh += p.rateModHz / sr; fmPh -= std::floor (fmPh);
                    pos += 0.25 * p.rateModAmt * std::sin (2.0 * pi * fmPh);
                }
                float g[2];
                for (int c = 0; c < 2; ++c)
                {
                    long long idx = 0;
                    const float local = ModShapes::swing (pos + (c == 1 ? stereo : 0.0f), p.groove, idx);
                    const float lp = ModShapes::feel (local, p.feel);
                    if (c == 0 && idx != lastCycle) { lastCycle = idx; heldPrev = held; held = rnd.next(); }
                    float hv = held;
                    if (p.shape == WingMod::smoothRandom) { const float w = 0.5f - 0.5f * std::cos ((float) pi * lp); hv = heldPrev + (held - heldPrev) * w; }
                    const float m = 0.5f * (1.0f + ModShapes::shape (p.shape, lp, p.pulseWidth, p.custom, p.customPoints, hv));   // 1 = open
                    float d = p.depth;
                    if (p.mode == rhythm) { const int si = (int) (((idx % ns) + ns) % ns); d *= clamp (p.steps[si], 0.0f, 1.0f); if (c == 0) stepNow = si; if (si % 4 == 0) d *= 1.0f - p.accent; }
                    else if (std::fmod ((double) idx, cyclesPerBar) == 0.0) d *= 1.0f - p.accent;
                    switch (p.dyn)
                    {
                        case dynEnv:     d *= clamp ((gainToDb (env) - p.thresholdDb) / 18.0f, 0.0f, 1.0f); break;
                        case dynGate:    d *= gateOpen ? 1.0f : 0.0f; break;
                        case dynInverse: d *= 1.0f - clamp ((gainToDb (env) - p.thresholdDb) / 18.0f, 0.0f, 1.0f); break;
                        default: break;
                    }
                    d = clamp (d, 0.0f, 1.0f);
                    const float tgt = 1.0f - d * (1.0f - m), tgt2 = 1.0f - d * m;
                    gSm[c] += sm * (tgt - gSm[c]); gSm2[c] += sm * (tgt2 - gSm2[c]);
                    g[c] = gSm[c];
                }
                gNow = g[0];
                float yl, yr;
                if (p.harmonic)
                {
                    const float lo0 = xo[0][1].process (xo[0][0].process (in0)), hi0 = xo[0][3].process (xo[0][2].process (in0));
                    const float lo1 = xo[1][1].process (xo[1][0].process (in1)), hi1 = xo[1][3].process (xo[1][2].process (in1));
                    yl = lo0 * g[0] + hi0 * gSm2[0]; yr = lo1 * g[1] + hi1 * gSm2[1];
                }
                else { yl = in0 * g[0]; yr = in1 * g[1]; }
                wl[i] = yl; wr[i] = yr;
            }
            if (p.analog > 0) { analog.setAmount (p.analogAmount); float* a[2] = { wl, wr }; analog.process (a, 2, n); }
            const int lat = p.analog > 0 ? clamp ((int) std::lround (analog.latencySamples()), 0, 31) : 0;
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
            for (int i = 0; i < n; ++i)
            {
                // the dry path is delayed by the drive's latency so Mix never combs
                const float d0 = x[0][i], d1 = nch > 1 ? x[1][i] : d0;
                dryDelay[(size_t) dw] = d0; dryDelay[(size_t) ((dw + 32) & 63)] = d1;
                const float r0 = dryDelay[(size_t) ((dw - lat) & 31)], r1 = dryDelay[(size_t) (((dw - lat) & 31) + 32)];
                dw = (dw + 1) & 31;
                x[0][i] = (r0 + mx * (wl[i] - r0)) * out;
                if (nch > 1) x[1][i] = (r1 + mx * (wr[i] - r1)) * out;
            }
        }
    };
}

//==============================================================================================================
namespace Orbit
{
    using namespace Eng;
    enum Mode { lfo, rhythmStep, rhythmShape, pingPong, random, randomStep, circle, numModes };
    enum Law { law0dB, law3dB, law4p5dB, law6dB, numLaws };
    inline const char* modeName (int m) { static const char* n[] = { "LFO", "Rhythm Step", "Rhythm Shape", "Ping-Pong", "Random", "Random Step", "Circle" }; return n[clamp (m, 0, 6)]; }
    inline const char* lawName (int l)  { static const char* n[] = { "0 dB", "-3 dB", "-4.5 dB", "-6 dB" }; return n[clamp (l, 0, 3)]; }

    /** Gains for a position -1 (left) .. 1 (right). Hard pan is unity; the centre sits at the law's dB. */
    inline void lawGains (int law, float p, float& gl, float& gr)
    {
        p = clamp (p, -1.0f, 1.0f);
        const float th = (p + 1.0f) * 0.25f * (float) pi;
        switch (law)
        {
            case law0dB:   gl = std::min (1.0f, 1.0f - p); gr = std::min (1.0f, 1.0f + p); break;
            case law4p5dB: gl = std::pow (std::cos (th), 1.5f); gr = std::pow (std::sin (th), 1.5f); break;
            case law6dB:   gl = 0.5f * (1.0f - p); gr = 0.5f * (1.0f + p); break;
            default:       gl = std::cos (th); gr = std::sin (th); break;
        }
    }

    struct Pattern { const char* name; float v[16]; };
    /** Factory step positions (-1 left .. 1 right); the "steps" text in the state overrides the chosen one. */
    inline const std::vector<Pattern>& patterns()
    {
        static const std::vector<Pattern> p {
            { "Hard L / R",      { -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1 } },
            { "Walk Across",     { -1, -.5f, 0, .5f, 1, .5f, 0, -.5f, -1, -.5f, 0, .5f, 1, .5f, 0, -.5f } },
            { "Narrow Flick",    { -.4f, .4f, -.4f, .4f, -.2f, .2f, -.2f, .2f, -.4f, .4f, -.4f, .4f, -.2f, .2f, -.2f, .2f } },
            { "Three Point",     { -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, 0 } },
            { "Scatter",         { -.8f, .3f, .9f, -.2f, .6f, -1, .1f, .7f, -.5f, 1, -.3f, .4f, -.9f, .2f, .8f, -.6f } },
            { "Centre Hits",     { 0, -1, 0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1, 0, 1 } },
            { "Left Lean",       { -1, -.6f, -1, -.3f, -1, -.6f, -1, .2f, -1, -.6f, -1, -.3f, -1, -.6f, -1, .4f } },
            { "Right Lean",      { 1, .6f, 1, .3f, 1, .6f, 1, -.2f, 1, .6f, 1, .3f, 1, .6f, 1, -.4f } },
            { "Spiral Out",      { 0, .15f, -.3f, .45f, -.6f, .75f, -.9f, 1, -1, .9f, -.75f, .6f, -.45f, .3f, -.15f, 0 } },
            { "Pairs",           { -1, -1, 1, 1, -1, -1, 1, 1, -.5f, -.5f, .5f, .5f, -.5f, -.5f, .5f, .5f } },
            { "Wide Triplets",   { -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 0 } },
            { "Drift",           { -.2f, -.1f, 0, .1f, .2f, .3f, .2f, .1f, 0, -.1f, -.2f, -.3f, -.2f, -.1f, 0, .1f } } };
        return p;
    }

    struct Params
    {
        int mode = lfo, shape = WingMod::sine;
        bool sync = false; int syncIndex = 12; float rateHz = 0.5f;
        float offsetDeg = 0, widthDeg = 180, smoothMs = 20, mix = 1, outputDb = 0, sourceWidth = 1;
        float steps[16] { -1, 1, -0.5f, 0.5f, -1, 1, 0, 0, -1, 1, -0.5f, 0.5f, -0.25f, 0.25f, -0.75f, 0.75f };
        int numSteps = 8, stepDivision = 9;
        bool audioTrigger = false; int divider = 1; float trigThreshDb = -24, detHpHz = 20, detLpHz = 20000;
        bool keyOn = false, binaural = false; int law = law3dB;
        float depth = 0.7f, haasMs = 0.6f, pulseWidth = 0.5f;
        float custom[16] { 0, 0.38f, 0.71f, 0.92f, 1, 0.92f, 0.71f, 0.38f, 0, -0.38f, -0.71f, -0.92f, -1, -0.92f, -0.71f, -0.38f };
        int customPoints = 16;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int)
        {
            sr = sampleRate;
            dl[0].assign (512, 0.0f); dl[1].assign (512, 0.0f);
            reset();
        }
        void reset()
        {
            ph = 0; posSm = 0; trig = false; trigEnv = 0; hold = 0; onsetCount = 0; stepIdx = 0; lastCycle = -1; cur = nxt = prv = 0; pp = 1; dw = 0; stepCounter = 0; rnd = {}; lastCut = -1; lastShadow = 1;
            for (auto& v : dl) std::fill (v.begin(), v.end(), 0.0f);
            for (auto& b : det) b.reset(); for (auto& b : shade) b.reset(); for (auto& b : shadow) b.reset();
        }
        void setTransport (double bpm, double ppqAtStart, bool isPlaying) { tBpm = bpm > 1 ? bpm : 120; tPpq = ppqAtStart; playing = isPlaying; }
        float position() const { return posNow; }           // -1..1 (and beyond, to +-105/90)
        float backness() const { return backNow; }          // circle: 0 front .. 1 behind
        int currentStep() const { return stepIdx; }
        int onsets() const { return onsetCount; }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = std::min (nch, 2);
            det[0].set (Biquad::highpass, sr, p.detHpHz, 0.7071); det[1].set (Biquad::lowpass, sr, p.detLpHz, 0.7071);
            const double cb = p.mode == rhythmStep || p.mode == rhythmShape || p.mode == pingPong ? WingMod::syncBeats (p.stepDivision) : WingMod::syncBeats (p.syncIndex);
            const double hz = (p.sync || p.mode == rhythmStep || p.mode == rhythmShape || p.mode == pingPong) ? tBpm / 60.0 / cb : (double) clamp (p.rateHz, 0.01f, 20.0f);
            const float sm = (float) (1.0 - std::exp (-1.0 / (std::max (0.1f, p.smoothMs) * 0.001 * sr)));
            const float on = dbToGain (p.trigThreshDb), offT = dbToGain (p.trigThreshDb - 6.0f), dec = (float) std::exp (-1.0 / (0.005 * sr));
            const int ns = clamp (p.numSteps, 1, 16), div = clamp (p.divider, 1, 16);
            const float half = 0.5f * clamp (p.widthDeg, 0.0f, 210.0f), offs = clamp (p.offsetDeg, -105.0f, 105.0f);
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
            const bool useKey = p.keyOn && key != nullptr;
            for (int i = 0; i < n; ++i)
            {
                const float in0 = ch[0][i], in1 = nch > 1 ? ch[1][i] : in0;
                // trigger detector (filtered), every Nth onset advances
                bool fire = false;
                if (p.audioTrigger)
                {
                    const float dsig = useKey ? 0.5f * (key[0][i] + key[1][i]) : 0.5f * (in0 + in1);
                    const float a = std::abs (det[1].process (det[0].process (dsig)));
                    trigEnv = std::max (a, trigEnv * dec);
                    if (! trig && trigEnv > on && hold <= 0) { trig = true; hold = (int) (0.03 * sr); ++onsetCount; if (onsetCount % div == 0) fire = true; }
                    else if (trig && trigEnv < offT) trig = false;
                    if (hold > 0) --hold;
                }
                // the phase / cycle counter
                double pos;
                if (p.audioTrigger && p.mode != lfo && p.mode != random && p.mode != circle) pos = (double) stepCounter + (fire ? 1.0 : 0.0);
                else if (playing && (p.sync || p.mode == rhythmStep || p.mode == rhythmShape || p.mode == pingPong)) pos = (tPpq + (double) i * tBpm / (60.0 * sr)) / cb;
                else { if (fire) ph = 0; ph += hz / sr; pos = ph; }
                if (fire) stepCounter++;
                const long long cyc = (long long) std::floor (pos);
                const float local = (float) (pos - (double) cyc);
                if (cyc != lastCycle) { lastCycle = cyc; prv = nxt; nxt = rnd.next(); cur = nxt; pp = -pp; stepIdx = (int) (((cyc % ns) + ns) % ns); }
                float v = 0;      // -1..1 across the width
                switch (p.mode)
                {
                    case lfo:         v = ModShapes::shape (p.shape, local - 0.25f, p.pulseWidth, p.custom, p.customPoints, cur); break;   // starts in the centre, moving right
                    case rhythmStep:  v = p.steps[stepIdx]; break;
                    case rhythmShape: v = p.steps[stepIdx] * ModShapes::shape (p.shape, local - 0.25f, p.pulseWidth, p.custom, p.customPoints, cur); break;
                    case pingPong:    v = (float) pp; break;
                    case random:      { const float w = 0.5f - 0.5f * std::cos ((float) pi * local); v = prv + (nxt - prv) * w; break; }
                    case randomStep:  v = cur; break;
                    case circle:      v = std::sin (2.0f * (float) pi * local); break;
                    default: break;
                }
                const float tgt = (offs + half * v) / 90.0f;
                posSm += sm * (tgt - posSm);
                posNow = posSm;
                const float back = p.mode == circle ? clamp (p.depth, 0.0f, 1.0f) * 0.5f * (1.0f - std::cos (2.0f * (float) pi * local)) : 0.0f;
                backNow = back;
                // source: mono part is panned, the side keeps what the position leaves of it
                const float m = 0.5f * (in0 + in1), s = 0.5f * (in0 - in1);
                const float core = clamp (posSm, -1.0f, 1.0f), beyond = std::max (0.0f, std::abs (posSm) - 1.0f) * 6.0f * 0.35f;
                float yl, yr;
                if (p.binaural)
                {
                    // ITD (Woodworth, 8.75 cm head) and a head-shadow shelf on the far ear
                    const float az = clamp (posSm, -1.0f, 1.0f) * 0.5f * (float) pi;
                    const float itd = (float) (0.0875 / 343.0 * (std::abs (az) + std::sin (std::abs (az))));
                    const float dly = itd * (float) sr;
                    const float shadowDb = -10.0f * std::abs (core);
                    if (std::abs (shadowDb - lastShadow) > 0.05f) { lastShadow = shadowDb; shadow[0].set (Biquad::highShelf, sr, 1800, 0.6, shadowDb); }
                    write (m);
                    const float nearS = m * (1.0f + 0.18f * std::abs (core)), farS = shadow[0].process (read (dly));
                    yl = core >= 0 ? farS : nearS; yr = core >= 0 ? nearS : farS;
                    yl += s * p.sourceWidth * (1.0f - std::abs (core)); yr -= s * p.sourceWidth * (1.0f - std::abs (core));
                }
                else
                {
                    // each input channel is a source of its own: both slide with the position, the image narrows towards
                    // the edges (Source Width), so a stereo input keeps its energy and a mono one pans by the law
                    const float w = p.sourceWidth * (1.0f - std::abs (core));
                    float a, b, c, d; lawGains (p.law, clamp (core - w, -1.0f, 1.0f), a, b); lawGains (p.law, clamp (core + w, -1.0f, 1.0f), c, d);
                    yl = a * in0 + c * in1; yr = b * in0 + d * in1;
                    if (p.mode == circle)
                    {
                        // depth: behind is darker and a little quieter; the far side gets a Haas delay
                        const float cutoff = 18000.0f * std::pow (0.16f, back);
                        if (std::abs (cutoff - lastCut) > 30.0f) { lastCut = cutoff; shade[0].set (Biquad::lowpass, sr, cutoff, 0.6); shade[1].set (Biquad::lowpass, sr, cutoff, 0.6); }
                        yl = shade[0].process (yl) * (1.0f - 0.25f * back); yr = shade[1].process (yr) * (1.0f - 0.25f * back);
                        dl[0][(size_t) dw] = yl; dl[1][(size_t) dw] = yr;
                        const float hd = p.haasMs * 0.001f * (float) sr * std::abs (core);
                        if (core > 0) yl = readCh (0, hd); else if (core < 0) yr = readCh (1, hd);
                    }
                }
                // beyond the speakers (+-90 .. +-105 deg): an anti-phase hint on the far side
                if (posSm > 1.0f) yl -= beyond * yr; else if (posSm < -1.0f) yr -= beyond * yl;
                ch[0][i] = (in0 + mx * (yl - in0)) * out;
                if (nch > 1) ch[1][i] = (in1 + mx * (yr - in1)) * out;
                dw = (dw + 1) & 511;
            }
        }

    private:
        double sr = 48000, tBpm = 120, tPpq = 0, ph = 0; bool playing = false;
        float posSm = 0, posNow = 0, backNow = 0, trigEnv = 0, cur = 0, nxt = 0, prv = 0, lastShadow = 1, lastCut = -1; int pp = 1;
        bool trig = false; int hold = 0, onsetCount = 0, stepIdx = 0; long long lastCycle = -1, stepCounter = 0;
        ModShapes::Rand rnd; Biquad det[2], shade[2], shadow[1];
        std::vector<float> dl[2]; int dw = 0;
        void write (float v) { dl[0][(size_t) dw] = v; }
        float read (float d) const { return readCh (0, d); }
        float readCh (int c, float d) const
        {
            d = clamp (d, 0.0f, 500.0f);
            const float rp = (float) dw - d; const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
            const float a = dl[c][(size_t) (i0 & 511)], b = dl[c][(size_t) ((i0 + 1) & 511)];
            return a + (b - a) * fr;
        }
    };
}

//==============================================================================================================
namespace Mirror
{
    using namespace Eng;
    struct Style { const char* name; float maxCents, maxDelayMs, windowMs, wander, modHz; int voices; float shelfDb; };
    inline int numStyles() { return 5; }
    inline const Style& style (int i)
    {
        static const Style s[] = {
            { "Micro A",     15, 18, 30, 0.00f, 0.0f, 1, 0 },     // the classic micro-pitch preset: fixed +- detune, two short delays
            { "Micro B",     24, 12, 24, 0.10f, 0.2f, 1, 0 },     // the second classic: wider detune, shorter delay, a slow drift
            { "Rack Wander", 20, 30, 40, 0.55f, 0.35f, 1, 0 },    // the rack pitch-delay: pitch and delay that wander
            { "Tape ADT",     0, 40, 40, 1.00f, 0.6f, 1, 0 },     // ours: a doubling tape head, wow and flutter instead of a fixed shift
            { "Prism",       30, 25, 28, 0.15f, 0.25f, 2, 2 } };  // ours: two voices a side, a brighter top
        return s[clamp (i, 0, numStyles() - 1)];
    }
    struct Params
    {
        int style = 0; float detune = 0.5f, delay = 0.5f;       // 0..1 (the % knobs)
        float focusHz = 20, mix = 1, outputDb = 0;
        float duckDb = 0, duckThreshDb = -30; bool keyOn = false;
    };

    /** One micro-pitch voice: two read taps on a delay line, sin^2 crossfaded, sweeping through a window. */
    struct Voice
    {
        double phi = 0;
        float process (const std::vector<float>& buf, int w, int mask, double sr, double cents, double baseMs, double windowMs)
        {
            const double r = std::pow (2.0, cents / 1200.0), W = std::max (4.0, windowMs * 0.001 * sr);
            phi += (1.0 - r) / W; phi -= std::floor (phi);
            float y = 0;
            for (int t = 0; t < 2; ++t)
            {
                double f = phi + 0.5 * t; f -= std::floor (f);
                const double d = baseMs * 0.001 * sr + f * W + 2.0;
                const double rp = (double) w - d; const long long i0 = (long long) std::floor (rp); const float fr = (float) (rp - (double) i0);
                const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
                const float s = (float) std::sin (pi * f);
                y += s * s * (a + (b - a) * fr);
            }
            return y;
        }
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int)
        {
            sr = sampleRate; int sz = 1; while (sz < (int) (sr * 0.2)) sz <<= 1; mask = sz - 1;
            for (auto& b : buf) b.assign ((size_t) sz, 0.0f);
            reset();
        }
        void reset()
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
            w = 0; for (auto& v : voice) v.phi = 0; for (auto& c : hp) for (auto& b : c) b.reset(); for (auto& b : shelf) b.reset();
            lfo = 0; wanderA = wanderB = 0; wTarget[0] = wTarget[1] = 0; duckEnv = 0; sLL = sRR = sLR = sM = sS = 0; rnd = {}; wCount = 0;
        }
        float correlation() const { return corr.load(); }
        float monoLossDb() const { return monoLoss.load(); }
        float duckGainDb() const { return duckNow.load(); }
        double centsFor (int voiceIdx) const
        {
            const auto& st = style (p.style);
            const double c = st.maxCents * clamp (p.detune, 0.0f, 1.0f);
            static const double sign[4] = { 1, -1, 0.5, -0.5 };
            return c * sign[clamp (voiceIdx, 0, 3)];
        }

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            nch = std::min (nch, 2);
            const auto& st = style (p.style);
            const bool focusOn = p.focusHz > 21.0f;
            for (int c = 0; c < 2; ++c) { hp[c][0].set (Biquad::highpass, sr, p.focusHz, 0.7071); hp[c][1].set (Biquad::highpass, sr, p.focusHz, 0.7071); shelf[c].set (Biquad::highShelf, sr, 5000, 0.7, st.shelfDb); }
            const double dMs = st.maxDelayMs * clamp (p.delay, 0.0f, 1.0f), cents = st.maxCents * clamp (p.detune, 0.0f, 1.0f);
            const float out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.5f);
            const float rel = (float) (1.0 - std::exp (-1.0 / (0.2 * sr))), att = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
            const float mtr = (float) (1.0 - std::exp (-1.0 / (0.3 * sr)));
            const bool useKey = p.keyOn && key != nullptr;
            float duckG = 1;
            for (int i = 0; i < n; ++i)
            {
                const float in0 = ch[0][i], in1 = nch > 1 ? ch[1][i] : in0;
                float f0 = in0, f1 = in1;
                if (focusOn) { f0 = hp[0][1].process (hp[0][0].process (in0)); f1 = hp[1][1].process (hp[1][0].process (in1)); }
                buf[0][(size_t) w] = f0; buf[1][(size_t) w] = f1;
                // slow wander (smooth random) and the tape wow / flutter
                if (--wCount <= 0) { wCount = (int) (sr * 0.25); wTarget[0] = rnd.next(); wTarget[1] = rnd.next(); }
                wanderA += 0.00002f * (float) (48000.0 / sr) * (wTarget[0] - wanderA); wanderB += 0.00002f * (float) (48000.0 / sr) * (wTarget[1] - wanderB);
                lfo += st.modHz / sr; lfo -= std::floor (lfo);
                float wl, wr;
                if (p.style == 3)
                {
                    // Tape ADT: a doubling head; Detune sets the wow / flutter depth (pitch by Doppler)
                    const double depth = 0.4 + 2.6 * clamp (p.detune, 0.0f, 1.0f);       // ms of excursion
                    const double fl = std::sin (2.0 * pi * lfo * 10.0) * 0.12;
                    const double dl = 8.0 + dMs + depth * (0.5 + 0.5 * std::sin (2.0 * pi * lfo) + fl + 0.3 * wanderA);
                    const double dr = 8.0 + dMs * 0.83 + depth * (0.5 + 0.5 * std::sin (2.0 * pi * (lfo + 0.31)) - fl + 0.3 * wanderB);
                    wl = tap (0, dl); wr = tap (1, dr);
                }
                else
                {
                    const double wa = 1.0 + st.wander * wanderA, wb = 1.0 + st.wander * wanderB;
                    const double drift = st.modHz > 0 ? 0.15 * std::sin (2.0 * pi * lfo) : 0.0;
                    wl = voice[0].process (buf[0], w, mask, sr, cents * (wa + drift), dMs * wa, st.windowMs);
                    wr = voice[1].process (buf[1], w, mask, sr, -cents * (wb - drift), dMs * 1.3 * wb, st.windowMs);
                    if (st.voices > 1)
                    {
                        wl += 0.6f * voice[2].process (buf[0], w, mask, sr, -0.5 * cents * wa, dMs * 0.55, st.windowMs * 0.8);
                        wr += 0.6f * voice[3].process (buf[1], w, mask, sr, 0.5 * cents * wb, dMs * 0.75, st.windowMs * 0.8);
                        wl *= 0.75f; wr *= 0.75f;
                    }
                    if (st.shelfDb != 0) { wl = shelf[0].process (wl); wr = shelf[1].process (wr); }
                }
                // ducked width: the key (or the input) pushes the wet voices down
                const float dsig = useKey ? std::max (std::abs (key[0][i]), std::abs (key[1][i])) : std::max (std::abs (in0), std::abs (in1));
                duckEnv += (dsig > duckEnv ? att : rel) * (dsig - duckEnv);
                if (p.duckDb > 0.01f) duckG = dbToGain (-p.duckDb * clamp ((gainToDb (duckEnv) - p.duckThreshDb) / 12.0f, 0.0f, 1.0f));
                else duckG = 1;
                const float yl = (in0 + mx * duckG * wl) * out, yr = (in1 + mx * duckG * wr) * out;
                ch[0][i] = yl; if (nch > 1) ch[1][i] = yr;
                sLL += mtr * (yl * yl - sLL); sRR += mtr * (yr * yr - sRR); sLR += mtr * (yl * yr - sLR);
                w = (w + 1) & mask;
            }
            duckNow = gainToDb (duckG);
            const float den = std::sqrt (std::max (1e-20f, sLL * sRR));
            corr = sLL + sRR > 1e-12f ? clamp (sLR / den, -1.0f, 1.0f) : 1.0f;
            // mono-sum loss: energy of (L+R)/2 against the stereo average; 0 dB for mono, -3 dB uncorrelated
            const float monoE = 0.25f * (sLL + sRR + 2.0f * sLR), stE = 0.5f * (sLL + sRR);
            monoLoss = stE > 1e-12f ? 10.0f * std::log10 (std::max (1e-9f, monoE / stE)) : 0.0f;
        }

    private:
        double sr = 48000, lfo = 0; int mask = 0, w = 0, wCount = 0;
        std::vector<float> buf[2]; Voice voice[4]; Biquad hp[2][2], shelf[2];
        float wanderA = 0, wanderB = 0, wTarget[2] {}, duckEnv = 0, sLL = 0, sRR = 0, sLR = 0, sM = 0, sS = 0;
        std::atomic<float> corr { 1 }, monoLoss { 0 }, duckNow { 0 };
        ModShapes::Rand rnd;
        float tap (int c, double ms) const
        {
            const double rp = (double) w - std::max (1.0, ms * 0.001 * sr); const long long i0 = (long long) std::floor (rp); const float fr = (float) (rp - (double) i0);
            const float a = buf[c][(size_t) (i0 & mask)], b = buf[c][(size_t) ((i0 + 1) & mask)];
            return a + (b - a) * fr;
        }
    };
}

//==============================================================================================================
namespace Shutter
{
    using namespace Eng;
    enum Algo { classic, parallel, reverb, series, crissCross, pingPong, numAlgos };
    inline const char* algoName (int a) { static const char* n[] = { "Classic", "Parallel", "Reverb", "Series", "Criss-Cross", "Ping-Pong" }; return n[clamp (a, 0, 5)]; }
    inline const char* bitsName (int b) { static const char* n[] = { "Clean", "12-bit", "8-bit" }; return n[clamp (b, 0, 2)]; }
    constexpr double baseClock = 32000.0;        // the line's clock at 1x
    constexpr double maxTimeMs = 1250.0;         // the time knob at 1x (x8 = 10 s)
    inline int multiplyFactor (int m) { return 1 << clamp (m, 0, 3); }
    /** The converter's bandwidth at a Multiply setting: 12 kHz at 1x, halving each step. */
    inline double bandwidthHz (int m) { return 12000.0 / multiplyFactor (m); }

    struct Params
    {
        float timeAMs = 250, timeBMs = 375; int multiply = 0; int syncA = -1, syncB = -1;
        float feedbackA = 0.35f, feedbackB = 0.35f; int algo = classic; float mix = 0.3f;
        float lfoHz = 0.5f, lfoDepth = 0; int bits = 1; bool freeze = false, smooth = true;
        float inputDb = 0, outputDb = 0;
    };

    /** Companding quantiser: mu-law curve, then 'levels' steps a polarity. Idempotent on its own output. */
    inline float quantise (float x, int bits)
    {
        if (bits <= 0) return x;
        const float mu = bits == 2 ? 255.0f : 15.0f, levels = bits == 2 ? 127.0f : 2047.0f;
        x = clamp (x, -1.0f, 1.0f);
        const float c = std::log1p (mu * std::abs (x)) / std::log1p (mu);
        const float q = std::round (c * levels) / levels;
        return (x < 0 ? -1.0f : 1.0f) * (std::expm1 (q * std::log1p (mu)) / mu);
    }

    class Line
    {
    public:
        void prepare (int samples) { buf.assign ((size_t) samples + 8, 0.0f); size = (int) buf.size(); w = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void write (float v) { buf[(size_t) w] = v; w = (w + 1) % size; }
        float read (double d) const
        {
            d = clamp (d, 1.0, (double) size - 4.0);
            double rp = (double) w - d; while (rp < 0) rp += size;
            const int i0 = (int) rp; const float fr = (float) (rp - i0);
            const float a = buf[(size_t) i0], b = buf[(size_t) ((i0 + 1) % size)];
            return a + (b - a) * fr;
        }
        int capacity() const { return size - 8; }
    private:
        std::vector<float> buf; int size = 1, w = 0;
    };

    /** A read head with the Smooth (crossfaded) or Glitch (jump) time change. */
    struct Head
    {
        double cur = 1000, old = 1000; float xf = 1; bool started = false;
        void target (double d, bool smooth, double fadeStep)
        {
            if (! started) { cur = old = d; started = true; xf = 1; return; }
            if (! smooth) { cur = old = d; xf = 1; return; }
            if (xf >= 1.0f && std::abs (d - cur) > 0.5) { old = cur; cur = d; xf = 0; }
            step = (float) fadeStep;
        }
        float read (const Line& l, double mod)
        {
            float y = l.read (cur + mod);
            if (xf < 1.0f) { const float o = l.read (old + mod); y = o + (y - o) * (0.5f - 0.5f * std::cos ((float) pi * xf)); xf = std::min (1.0f, xf + step); }
            return y;
        }
        float step = 0.001f;
    };

    class Processor
    {
    public:
        Params p;
        void prepare (double sampleRate, int)
        {
            sr = sampleRate;
            const int cap = (int) (baseClock * 10.0) + 64;      // 10 s at the 1x clock: the knob needs 1.25 s x 32 kHz at any Multiply, sync up to 10 s
            for (auto& l : line) l.prepare (cap);
            mult = -1; reset();
        }
        void reset()
        {
            for (auto& l : line) l.reset(); for (auto& c : aa) for (auto& b : c) b.reset(); for (auto& c : rc) for (auto& b : c) b.reset();
            tick = 0; heldL = heldR = 0; ya = yb = 0; lfoPh = 0; for (auto& h : head) h.started = false;
        }
        void setTempo (double bpm) { tBpm = bpm > 1 ? bpm : 120; }
        double lineRate() const { return baseClock / multiplyFactor (p.multiply); }
        /** Total echo time of a line in ms (Time x Multiply, or the synced length). */
        double totalMs (int ln) const
        {
            const int s = ln == 0 ? p.syncA : p.syncB;
            double ms = s >= 0 ? WingMod::syncBeats (s) * 60000.0 / tBpm : (double) clamp (ln == 0 ? p.timeAMs : p.timeBMs, 1.0f, (float) maxTimeMs) * multiplyFactor (p.multiply);
            return std::min (ms, (double) line[0].capacity() / lineRate() * 1000.0);
        }

        void process (float* const* ch, int nch, int n)
        {
            nch = std::min (nch, 2);
            if (p.multiply != mult)
            {
                mult = clamp (p.multiply, 0, 3);
                const double bw = bandwidthHz (mult);
                static const double q[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };      // 8th-order Butterworth
                for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) { aa[c][k].set (Biquad::lowpass, sr, bw, q[k]); rc[c][k].set (Biquad::lowpass, sr, bw, q[k]); }
            }
            const double R = lineRate(), fade = 1.0 / (0.04 * R);
            head[0].target (totalMs (0) * 0.001 * R, p.smooth, fade);
            head[1].target (totalMs (1) * 0.001 * R, p.smooth, fade);
            const float inG = dbToGain (p.inputDb), out = dbToGain (p.outputDb), mx = clamp (p.mix, 0.0f, 1.0f);
            const float fa = p.freeze ? 1.0f : clamp (p.feedbackA, 0.0f, 1.1f), fb = p.freeze ? 1.0f : clamp (p.feedbackB, 0.0f, 1.1f);
            const double lfoHz = clamp (p.lfoHz, 0.1f, 256.0f);
            // LFO excursion: at most ~6 % pitch deviation, never more than half the shortest delay
            const double amp = p.lfoDepth <= 0 ? 0.0 : std::min ({ p.lfoDepth * 0.06 * R / (2.0 * pi * lfoHz), 0.45 * std::min (head[0].cur, head[1].cur), 400.0 });
            for (int i = 0; i < n; ++i)
            {
                const float in0 = ch[0][i] * inG, in1 = (nch > 1 ? ch[1][i] : ch[0][i]) * inG;
                float x0 = in0, x1 = in1;
                for (int k = 0; k < 4; ++k) { x0 = aa[0][k].process (x0); x1 = aa[1][k].process (x1); }
                tick += R / sr;
                while (tick >= 1.0)
                {
                    tick -= 1.0;
                    lfoPh += lfoHz / R; lfoPh -= std::floor (lfoPh);
                    const double mod = amp * std::sin (2.0 * pi * lfoPh);
                    ya = head[0].read (line[0], mod); yb = head[1].read (line[1], -mod);
                    const float xl = p.freeze ? 0.0f : x0, xr = p.freeze ? 0.0f : x1, mono = 0.5f * (xl + xr);
                    float ia = 0, ib = 0, ol = 0, orr = 0;
                    switch (p.algo)
                    {
                        case parallel:   ia = xl + fa * ya; ib = xr + fb * yb; ol = ya; orr = yb; break;
                        case reverb:     ia = mono + fa * 0.7071f * (ya - yb); ib = mono + fb * 0.7071f * (ya + yb); ol = ya; orr = yb; break;
                        case series:     ia = mono + fb * yb; ib = ya; ol = 0.6f * ya + 0.8f * yb; orr = 0.8f * ya + 0.6f * yb; break;
                        case crissCross: ia = xl + fa * yb; ib = xr + fb * ya; ol = ya; orr = yb; break;
                        case pingPong:   ia = mono + fa * yb; ib = ya; ol = ya; orr = yb; break;
                        default:         ia = mono + fa * ya; ib = 0; ol = orr = ya; break;
                    }
                    if (! p.freeze) { ia = soft (ia); ib = soft (ib); }
                    line[0].write (p.freeze ? ia : quantise (ia, p.bits)); line[1].write (p.freeze ? ib : quantise (ib, p.bits));
                    heldL = ol; heldR = orr;
                }
                float wl = heldL, wr = heldR;           // zero-order hold at the line's clock, then the reconstruction filter
                for (int k = 0; k < 4; ++k) { wl = rc[0][k].process (wl); wr = rc[1][k].process (wr); }
                const float d0 = ch[0][i], d1 = nch > 1 ? ch[1][i] : d0;
                ch[0][i] = (d0 + mx * (wl - d0)) * out;
                if (nch > 1) ch[1][i] = (d1 + mx * (wr - d1)) * out;
            }
        }
        double currentDelayMs (int ln) const { return head[clamp (ln, 0, 1)].cur / lineRate() * 1000.0; }

    private:
        double sr = 48000, tBpm = 120, tick = 0, lfoPh = 0; int mult = -1;
        Line line[2]; Head head[2]; Biquad aa[2][4], rc[2][4];
        float heldL = 0, heldR = 0, ya = 0, yb = 0;
        static float soft (float x) { return std::abs (x) < 0.7f ? x : (x > 0 ? 1.0f : -1.0f) * (0.7f + 0.3f * std::tanh ((std::abs (x) - 0.7f) / 0.3f)); }
    };
}
