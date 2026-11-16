#pragma once
// Radiant / Radiant Pre (super prompt 1.11, C10) and Maw / Maw Lite (C11). Standard C++ only
// (measured in tests/radiantmaw_test.cpp).
//
// Radiant: a two-stage tube mixer. Input -> tube stage 1 -> Baxandall bass / treble (+-12 dB) -> Output -> tube stage 2
// -> Mix, with Mic / Line (Mic is 10 dB hotter), a switchable noise floor (about -68 dBu), a heat VU, 1x-8x oversampling
// and Mid/Side. Three archetypes: 60s Tube Mixer, 50s Broadcast Pre, Tube Line Amp. Radiant Pre is the preamp
// archetype with a Bias switch that starves the tube into choppy breakup.
//
// Maw: a crushing podium-limiter archetype. Crush drives the input into ~20:1 gain reduction, attack 1.3 ms, release
// continuously variable 0.05-30 s (1.7 s by default) that stretches as the circuit saturates (Stretch amount, up to
// 13x), Crunch for make-up and oversampled distortion, Darkness as a labelled low-pass, Mix, and an external key with
// its own high-pass. A GR history is kept for the meter.

#include "SqEngine111Dsp.h"

namespace Radiant
{
    using namespace Eng;
    enum Arch { mixer60s, broadcast50s, lineAmp, preamp, numArch };
    inline const char* archName (int a) { static const char* n[] = { "60s Tube Mixer", "50s Broadcast Pre", "Tube Line Amp", "Tube Mic Pre" }; return n[clamp (a, 0, 3)]; }
    inline const char* archDescription (int a)
    {
        static const char* d[] = { "1960s tube mixer: two warm stages, a fat Baxandall tone stack, the classic colour box",
                                   "1950s broadcast preamp: transformer-coupled, darker, rich second harmonic, soft top",
                                   "tube line amplifier: cleaner and faster, more odd-order bite when pushed",
                                   "1960s tube mic preamp: a single hot stage; Bias starves it into choppy breakup" };
        return d[clamp (a, 0, 3)];
    }
    struct ArchDef { int nl1; float c1, b1; int nl2; float c2, b2; float bandwidthHz; float lowColourDb; };
    inline const ArchDef& arch (int a)
    {
        using namespace Nonlinear;
        static const ArchDef d[] = { { glow, 0.5f, 0.35f, glow, 0.4f, 0.25f, 18000, 0.5f },
                                     { transformer, 0.6f, 0.2f, glow, 0.6f, 0.45f, 12000, 1.0f },
                                     { pentode, 0.4f, 0.1f, pentode, 0.5f, 0.05f, 22000, 0.0f },
                                     { glow, 0.6f, 0.5f, glow, 0.0f, 0.0f, 16000, 0.5f } };
        return d[clamp (a, 0, 3)];
    }
    struct Params
    {
        int arch = mixer60s, oversampleLog2 = 2, ms = 0;       // ms: 0 stereo, 1 M/S, 2 mid only, 3 side only
        float inputDb = 0, bassDb = 0, trebleDb = 0, outputDb = 0, mix = 1, trimDb = 0;
        bool mic = false, noisy = false, bias = false;
        bool singleStage = false;                               // Radiant Pre
    };

    class Processor
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; mb = clamp (maxBlock, 16, 4096);
            os.prepare (mb, p.oversampleLog2, sr); osDry.prepare (mb, p.oversampleLog2, sr); alA.prepare (mb, p.oversampleLog2, sr); alB.prepare (mb, p.oversampleLog2, sr);
            for (int a = 0; a < numArch; ++a) { Nonlinear::warm (arch (a).nl1, arch (a).c1); Nonlinear::warm (arch (a).nl2, arch (a).c2); }
            Nonlinear::warm (Nonlinear::rectify, 0.85f);
            for (int c = 0; c < 2; ++c) { work[c].assign ((size_t) mb, 0.0f); dry[c].assign ((size_t) mb, 0.0f); }
            applied = {}; applied.arch = -1; configure (true); reset();
        }
        void reset() { os.reset(); osDry.reset(); alA.reset(); alB.reset(); for (int c = 0; c < 2; ++c) { s1[c].reset(); s2[c].reset(); bass[c].reset(); treb[c].reset(); bw[c].reset(); lowC[c].reset(); } heat = 0; rng = 1234567u; }
        Params p;
        void configure (bool force = false)
        {
            const int f = clamp (p.oversampleLog2, 0, 3);
            if (f != os.factorLog2()) { os.setFactorLog2 (f, 0); osDry.setFactorLog2 (f, 0); alA.setFactorLog2 (f, 0); alB.setFactorLog2 (f, 0); force = true; }
            const auto& d = arch (p.arch);
            if (force || p.arch != applied.arch || p.bias != applied.bias)
                for (int c = 0; c < 2; ++c)
                {
                    const bool starve = p.singleStage && p.bias;
                    s1[c].model = starve ? Nonlinear::rectify : d.nl1; s1[c].character = starve ? 0.85f : d.c1; s1[c].bias = starve ? 0.0f : d.b1; s1[c].prepare (sr * os.factor());
                    s2[c].model = d.nl2; s2[c].character = d.c2; s2[c].bias = d.b2; s2[c].prepare (sr * os.factor());
                    bw[c].set (sr, std::min ((double) d.bandwidthHz, sr * 0.45)); lowC[c].set (Biquad::lowShelf, sr, 90, 0.7, d.lowColourDb);
                }
            if (force || p.bassDb != applied.bassDb || p.trebleDb != applied.trebleDb)
                for (int c = 0; c < 2; ++c) { bass[c].set (Biquad::lowShelf, sr, 100, 0.45, p.bassDb); treb[c].set (Biquad::highShelf, sr, 8000, 0.45, p.trebleDb); }
            applied = p;
        }
        double latencySamples() const { return os.latencySamples() * (p.singleStage ? 1 : 2); }
        float heatDb() const { return gainToDb (heat + 1.0e-9f); }

        void process (float* const* ch, int nch, int n)
        {
            if (n > mb) { for (int o = 0; o < n; o += mb) { float* sub[2] = { ch[0] + o, ch[nch > 1 ? 1 : 0] + o }; process (sub, nch, std::min (mb, n - o)); } return; }
            configure();
            float* L = ch[0]; float* R = nch > 1 ? ch[1] : ch[0];
            const int chans = nch > 1 ? 2 : 1;
            const float g1 = dbToGain (p.inputDb + (p.mic ? 10.0f : 0.0f) + 6.0f), g2 = dbToGain (p.outputDb + 3.0f), trim = dbToGain (p.trimDb);
            const float noiseG = p.noisy ? dbToGain (-86.0f) : 0.0f;
            const float heatK = (float) (1.0 - std::exp (-(double) n / (0.3 * sr)));
            float blockHeat = 0;
            for (int i = 0; i < n; ++i)
            {
                float a = L[i], b = R[i];
                dry[0][(size_t) i] = a; dry[1][(size_t) i] = b;
                if (p.ms != 0) { const float m = 0.5f * (a + b), s = 0.5f * (a - b); a = m; b = s; }
                work[0][(size_t) i] = a; work[1][(size_t) i] = b;
            }
            for (int c = 0; c < chans; ++c)
            {
                const bool on = ! (p.ms == 2 && c == 1) && ! (p.ms == 3 && c == 0);
                float* w = work[c].data();
                float* u = os.up (c, w, n);
                const int m = n * os.factor();
                // stage 1 (oversampled): the input knob is drive and gain at once, like the console
                for (int i = 0; i < m; ++i) { const float x = u[i] * g1; blockHeat = std::max (blockHeat, std::abs (x)); u[i] = on ? s1[c].process (x) : x; }
                os.down (c, u, n, w);
                // the tone stack at the base rate, then stage 2 (oversampled again)
                for (int i = 0; i < n; ++i) w[i] = treb[c].process (bass[c].process (w[i] / g1)) * g2;
                if (! p.singleStage)
                {
                    float* u2 = osDry.up (c, w, n);
                    for (int i = 0; i < m; ++i) u2[i] = on ? s2[c].process (u2[i]) : u2[i];
                    osDry.down (c, u2, n, w);
                }
                for (int i = 0; i < n; ++i) w[i] = bw[c].lp (lowC[c].process (w[i])) / g2 * dbToGain (p.outputDb) + noiseG * noise();
            }
            heat += heatK * (blockHeat - heat);
            // the dry signal goes through the same round trips so Mix never combs
            for (int c = 0; c < chans; ++c)
            {
                float* d = dry[c].data();
                float* u = alA.up (c, d, n); alA.down (c, u, n, d);
                if (! p.singleStage) { float* u2 = alB.up (c, d, n); alB.down (c, u2, n, d); }
            }
            for (int i = 0; i < n; ++i)
            {
                float a = work[0][(size_t) i], b = chans > 1 ? work[1][(size_t) i] : a;
                if (p.ms != 0) { const float l = a + b, r = a - b; a = l; b = r; }
                L[i] = (dry[0][(size_t) i] * (1.0f - p.mix) + a * p.mix) * trim;
                if (nch > 1) R[i] = (dry[1][(size_t) i] * (1.0f - p.mix) + b * p.mix) * trim;
            }
        }
        /** THD (%) of the whole chain for a 1 kHz sine at -18 dBFS (not real-time). */
        double thd() const
        {
            Processor q; q.p = p; q.p.mix = 1; q.p.noisy = false; q.p.ms = 0; q.prepare (48000.0, 480);
            std::vector<float> l (480), r (480), acc;
            const double w = 2.0 * pi * 1000.0 / 48000.0;
            for (int bl = 0; bl < 60; ++bl)
            {
                for (int i = 0; i < 480; ++i) l[(size_t) i] = r[(size_t) i] = 0.177f * (float) std::sin (w * (bl * 480 + i));
                float* ch[2] = { l.data(), r.data() }; q.process (ch, 2, 480);
                if (bl >= 40) acc.insert (acc.end(), l.begin(), l.end());
            }
            double f = 0, h = 0;
            for (int k = 1; k <= 10; ++k)
            {
                double S = 0, C = 0; for (size_t i = 0; i < acc.size(); ++i) { const double t = 40.0 * 480 + (double) i; S += acc[i] * std::sin (w * k * t); C += acc[i] * std::cos (w * k * t); }
                const double a = std::sqrt (S * S + C * C); if (k == 1) f = a; else h += a * a;
            }
            return 100.0 * std::sqrt (h) / std::max (1.0e-12, f);
        }
    private:
        double sr = 48000; int mb = 512;
        Oversampler os, osDry, alA, alB;
        Nonlinear::Stage s1[2], s2[2]; Biquad bass[2], treb[2], lowC[2]; OnePole bw[2];
        std::vector<float> work[2], dry[2];
        float heat = 0; uint32_t rng = 1234567u;
        Params applied;
        inline float noise() { rng = rng * 1664525u + 1013904223u; return (float) ((rng >> 8) & 0xffff) / 32767.5f - 1.0f; }
    };
}

namespace Maw
{
    using namespace Eng;
    struct Params
    {
        float crush = 0.5f, crunch = 0.3f, darknessHz = 20000, mix = 1, releaseS = 1.7f, stretch = 1, keyHpfHz = 20, outputDb = 0;
        bool keyOn = false, fastRelease = false; int oversampleLog2 = 2;
    };
    class Processor
    {
    public:
        static constexpr int historyLen = 256;
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; mb = clamp (maxBlock, 16, 4096);
            os.prepare (mb, p.oversampleLog2, sr); osDry.prepare (mb, p.oversampleLog2, sr);
            Nonlinear::warm (Nonlinear::glow, 0.3f); Nonlinear::warm (Nonlinear::diode, 0.5f);
            for (int c = 0; c < 2; ++c) { work[c].assign ((size_t) mb, 0.0f); dry[c].assign ((size_t) mb, 0.0f); st[c].model = Nonlinear::glow; st[c].character = 0.3f; st[c].bias = 0.3f; st[c].prepare (sr * os.factor()); }
            applied = {}; applied.darknessHz = -1; configure (true); reset();
        }
        void reset() { os.reset(); osDry.reset(); grDb = 0; env = 0; for (int c = 0; c < 2; ++c) { st[c].reset(); dark[c].reset(); dark2[c].reset(); khp[c].reset(); } histW = 0; for (auto& h : history) h = 0; histAcc = 0; histCount = 0; }
        Params p;
        void configure (bool force = false)
        {
            const int f = clamp (p.oversampleLog2, 0, 3);
            if (f != os.factorLog2()) { os.setFactorLog2 (f, 0); osDry.setFactorLog2 (f, 0); for (auto& s : st) s.prepare (sr * os.factor()); }
            if (force || p.darknessHz != applied.darknessHz) for (int c = 0; c < 2; ++c) { dark[c].set (Biquad::lowpass, sr, p.darknessHz, 0.7071); dark2[c].set (Biquad::lowpass, sr, p.darknessHz, 0.7071); }
            if (force || p.keyHpfHz != applied.keyHpfHz) for (int c = 0; c < 2; ++c) khp[c].set (Biquad::highpass, sr, std::max (20.0f, p.keyHpfHz), 0.7071);
            applied = p;
        }
        double latencySamples() const { return os.latencySamples(); }
        float gainReductionDb() const { return grDb; }
        /** GR history for the meter (oldest first), dB <= 0. */
        void historyCopy (float* out) const { for (int i = 0; i < historyLen; ++i) out[i] = history[(histW + i) % historyLen]; }
        static constexpr float attackMs = 1.3f;

        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            if (n > mb) { for (int o = 0; o < n; o += mb) { float* sub[2] = { ch[0] + o, ch[nch > 1 ? 1 : 0] + o }; const float* ks[2] = { key ? key[0] + o : nullptr, key ? key[1] + o : nullptr }; process (sub, nch, key ? ks : nullptr, std::min (mb, n - o)); } return; }
            configure();
            float* L = ch[0]; float* R = nch > 1 ? ch[1] : ch[0];
            const int chans = nch > 1 ? 2 : 1;
            const float inG = dbToGain (p.crush * 40.0f);                            // Crush: 0..+40 dB into the limiter
            const float thr = dbToGain (-10.0f), ratioSlope = 1.0f - 1.0f / 20.0f;
            const float atk = (float) (1.0 - std::exp (-1.0 / (attackMs * 0.001 * sr)));
            const double relBase = p.fastRelease ? 0.08 : clamp ((double) p.releaseS, 0.05, 30.0);
            const float crunchG = dbToGain (p.crunch * 24.0f);                       // Crunch: make-up into the saturator
            const float makeup = dbToGain (std::min (30.0f, p.crush * 40.0f * 0.75f)); // most of the crush comes back as level
            const float outG = dbToGain (p.outputDb);
            for (int i = 0; i < n; ++i)
            {
                dry[0][(size_t) i] = L[i]; dry[1][(size_t) i] = R[i];
                const float a = L[i] * inG, b = R[i] * inG;
                float det = std::max (std::abs (a), std::abs (b));
                if (p.keyOn && key != nullptr) det = std::max (std::abs (khp[0].process (key[0][i])), std::abs (khp[1].process (key[1][i]))) * inG;
                const float over = det > thr ? gainToDb (det / thr) : 0.0f;
                const float target = over * ratioSlope;                               // dB of gain reduction wanted
                if (target > grDb) grDb += atk * (target - grDb);
                else
                {
                    // the release stretches as the circuit saturates: up to 13x (1.7 s -> 22 s) at 20 dB of GR
                    const double sat = clamp ((double) grDb / 20.0, 0.0, 1.0);
                    const double rel = relBase * (1.0 + p.stretch * 12.0 * sat * sat);
                    grDb += (float) (1.0 - std::exp (-1.0 / (rel * sr))) * (target - grDb);
                }
                const float g = dbToGain (-grDb);
                work[0][(size_t) i] = a * g * makeup / inG; work[1][(size_t) i] = b * g * makeup / inG;
                histAcc = std::max (histAcc, grDb);
                if (++histCount >= (int) (sr * 0.02)) { history[histW] = -histAcc; histW = (histW + 1) % historyLen; histAcc = 0; histCount = 0; }
            }
            for (int c = 0; c < chans; ++c)
            {
                float* w = work[c].data();
                if (p.crunch > 0.001f)
                {
                    float* u = os.up (c, w, n); const int m = n * os.factor();
                    st[c].drive = crunchG;
                    for (int i = 0; i < m; ++i) u[i] = st[c].process (u[i]) / std::sqrt (crunchG);
                    os.down (c, u, n, w);
                }
                else { float* u = os.up (c, w, n); os.down (c, u, n, w); }
                if (p.darknessHz < 19900.0f) for (int i = 0; i < n; ++i) w[i] = dark2[c].process (dark[c].process (w[i]));
                float* d = dry[c].data(); float* ud = osDry.up (c, d, n); osDry.down (c, ud, n, d);
            }
            for (int i = 0; i < n; ++i)
            {
                const float a = work[0][(size_t) i], b = chans > 1 ? work[1][(size_t) i] : a;
                L[i] = (dry[0][(size_t) i] * (1.0f - p.mix) + a * p.mix) * outG;
                if (nch > 1) R[i] = (dry[1][(size_t) i] * (1.0f - p.mix) + b * p.mix) * outG;
            }
        }
    private:
        double sr = 48000; int mb = 512;
        Oversampler os, osDry; Nonlinear::Stage st[2]; Biquad dark[2], dark2[2], khp[2];
        std::vector<float> work[2], dry[2];
        float grDb = 0, env = 0, history[historyLen] {}, histAcc = 0; int histW = 0, histCount = 0;
        Params applied;
    };
}
