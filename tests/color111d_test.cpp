// Measures Larva Filter 2 and Twelve Phase 2 (Source/SqColor111dDsp.h).
// g++ -O2 -std=c++17 -ISource tests/color111d_test.cpp -o /tmp/c111d && /tmp/c111d
#include "SqColor111dDsp.h"
#include <cstdio>
#include <set>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const char* what, double v = 0) { std::printf ("%s  %-80s %10.3f\n", ok ? "PASS" : "FAIL", what, v); ok ? ++passes : ++fails; }
static const double SR = 48000;
static double rms (const std::vector<float>& x, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i]; return std::sqrt (e / (double) std::max<size_t> (1, b - a)); }
static double db (double g) { return 20 * std::log10 (std::max (1e-12, g)); }
static std::vector<float> sine (double hz, double secs, float amp = 0.1f) { std::vector<float> v ((size_t) (SR * secs)); for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2 * pi * hz * i / SR); return v; }
template <typename P> static void run (P& p, std::vector<float>& l, std::vector<float>& r, const std::vector<float>* key = nullptr)
{
    for (size_t i = 0; i + 256 <= l.size(); i += 256)
    {
        float* ch[2] = { l.data() + i, r.data() + i }; const float* k[2] = { key ? key->data() + i : nullptr, key ? key->data() + i : nullptr };
        p.process (ch, 2, key ? k : nullptr, 256);
    }
}
static Larva2::Params staticLarva (int model, int resp, float cut, float res)
{
    Larva2::Params p; p.stages = 1; p.model[0] = model; p.response[0] = resp; p.cutoffHz = cut; p.resonance = res; p.freqModOct = 0; p.resMod = 0; p.mix = 1; p.mod.mode = WingMod::lfo; p.mod.rateHz = 0.01;
    return p;
}
static double gainAt (const Larva2::Params& pp, double hz, int ch = 0)
{
    Larva2::Processor f; f.p = pp; f.prepare (SR, 256);
    auto l = sine (hz, 0.512), r = l; run (f, l, r);
    return db (rms (ch ? r : l, 12000, 24576) / (0.1 / std::sqrt (2.0)));
}

int main()
{
    //======================================================================== Larva Filter 2
    for (int m = 0; m < FilterModels::numModels; ++m)
    {
        const auto p = staticLarva (m, FilterModels::lp, 1000, 0.0f);
        const double lo = gainAt (p, 100), hi = gainAt (p, 8000), want = m <= FilterModels::diode ? -45 : -25;
        char buf[128]; std::snprintf (buf, sizeof buf, "Larva 2: %s low-pass at 1 kHz passes 100 Hz, stops 8 kHz (dB at 8 kHz)", FilterModels::modelName (m));
        check (std::abs (lo) < 2.5 && hi < want, buf, hi);
    }
    {
        const double h = gainAt (staticLarva (FilterModels::svf, FilterModels::hp, 1000, 0), 100), b1 = gainAt (staticLarva (FilterModels::svf, FilterModels::bp, 1000, 0.6f), 1000),
                     b2 = gainAt (staticLarva (FilterModels::svf, FilterModels::bp, 1000, 0.6f), 150), nt = gainAt (staticLarva (FilterModels::svf, FilterModels::notch, 1000, 0.3f), 1000);
        check (h < -30 && b1 > b2 + 10 && nt < -25, "Larva 2: state-variable high-pass, band-pass and notch shapes (notch depth dB)", nt);
        const double hpL = gainAt (staticLarva (FilterModels::transistor, FilterModels::hp, 1000, 0), 100);
        check (hpL < -40, "Larva 2: transistor ladder high-pass (dB at 100 Hz for a 1 kHz cutoff)", hpL);
    }
    {
        const double flat = gainAt (staticLarva (FilterModels::transistor, FilterModels::lp, 1000, 0.0f), 1000), peak = gainAt (staticLarva (FilterModels::transistor, FilterModels::lp, 1000, 0.9f), 1000);
        check (peak - flat > 8, "Larva 2: ladder resonance peaks at the cutoff (dB above Res 0)", peak - flat);
        const double sk = gainAt (staticLarva (FilterModels::sallenKey, FilterModels::lp, 1000, 0.95f), 1000);
        bool fin = std::isfinite (sk); check (fin, "Larva 2: Sallen-Key at full resonance stays finite (dB at cutoff)", sk);
    }
    {
        auto p = staticLarva (FilterModels::svf, FilterModels::lp, 1000, 0); p.lrOffsetOct = 1;
        const double l = gainAt (p, 2000, 0), r = gainAt (p, 2000, 1);
        check (r - l > 5, "Larva 2: L/R offset 1 oct opens the right channel (dB R vs L at 2 kHz)", r - l);
    }
    {
        auto p = staticLarva (FilterModels::svf, FilterModels::lp, 1000, 0.3f); p.freqModOct = 2; p.mod.mode = WingMod::lfo; p.mod.shape = WingMod::sine; p.mod.rateHz = 2;
        Larva2::Processor f; f.p = p; f.prepare (SR, 256); auto l = sine (300, 1.0), r = l; float mn = 1e9f, mx = 0;
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; f.process (ch, 2, nullptr, 256); mn = std::min (mn, f.modulatedCutoff()); mx = std::max (mx, f.modulatedCutoff()); }
        check (std::abs (std::log2 (mx / mn) - 4) < 0.2, "Larva 2: Freq Mod 2 oct with the LFO sweeps 4 octaves end to end", std::log2 (mx / mn));
    }
    {
        // the envelope from an external key opens the filter
        auto p = staticLarva (FilterModels::svf, FilterModels::lp, 300, 0.2f); p.freqModOct = 4; p.mod.mode = WingMod::envelope; p.mod.bipolar = false; p.keyOn = true;
        Larva2::Processor f; f.p = p; f.prepare (SR, 256);
        auto l = sine (200, 1.0), r = l; std::vector<float> key (l.size(), 0.0f); for (size_t i = 24000; i < key.size(); ++i) key[i] = 0.8f * (float) std::sin (i * 0.05);
        float before = 0, after = 0;
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; const float* k[2] = { key.data() + i, key.data() + i }; f.process (ch, 2, k, 256); if (i == 20224) before = f.modulatedCutoff(); after = f.modulatedCutoff(); }
        check (after > before * 8, "Larva 2: a key on the envelope opens the cutoff (x)", after / before);
    }
    {
        Larva2::Params p; p.stages = 8; p.mix = 1;
        for (int rt = 0; rt < 3; ++rt)
            for (int m = 0; m < 4; ++m)
            {
                p.routing = rt; for (int s = 0; s < 8; ++s) { p.model[s] = (s + m) % 4; p.response[s] = s % 4; p.stageOct[s] = (float) (s % 3) - 1.0f; }
                p.resonance = 0.9f; p.analog = 1 + (m + rt) % 7; p.analogPost = rt == 1; p.mod.mode = (m + rt) % WingMod::numModes;
                Larva2::Processor f; f.p = p; f.prepare (SR, 256);
                std::vector<float> l, r; TestSignals::drums (l, r, SR, 1.0); run (f, l, r);
                bool fin = true; float pk = 0; for (size_t i = 0; i < l.size(); ++i) { fin = fin && std::isfinite (l[i]); pk = std::max (pk, std::abs (l[i])); }
                if (! fin || pk > 4) { check (false, "Larva 2: 8 stages, every routing / model / drive / mod mode stays finite", pk); goto larvaDone; }
            }
        check (true, "Larva 2: 8 stages, every routing / model / drive / mod mode stays finite", 1);
        larvaDone:;
    }
    {
        Larva2::Params p = staticLarva (FilterModels::svf, FilterModels::lp, 500, 0.3f); p.freqModOct = 3; p.mod.mode = WingMod::rhythm; p.mod.stepDivision = 6;
        Larva2::Processor f; f.p = p; f.prepare (SR, 256); std::set<int> seen; auto l = sine (200, 2.0), r = l;
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; f.process (ch, 2, nullptr, 256); seen.insert (f.currentStep()); }
        check (seen.size() >= 15, "Larva 2: Rhythm mode walks the 16-step editor (steps seen)", (double) seen.size());
    }
    //======================================================================== Twelve Phase 2
    {
        const auto& st = Phase2::styles(); std::set<std::string> nm; for (auto& s : st) nm.insert (s.name);
        check (st.size() == 72 && nm.size() == 72, "Twelve Phase 2: 72 styles, all named differently", (double) nm.size());
    }
    auto staticPhase = [] (int stages)
    {
        Phase2::Params p; p.stages = stages; p.classic = false; p.resonance = 0; p.sweepOct = 0; p.mix = 1; p.depth = 1; p.centreHz = 1000; p.mod.rateHz = 0.01; p.spreadDeg = 0; return p;
    };
    auto phGain = [] (const Phase2::Params& pp, double hz)
    {
        Phase2::Processor f; f.p = pp; f.prepare (SR, 256); auto l = sine (hz, 0.512), r = l; run (f, l, r);
        return db (rms (l, 12000, 24576) / (0.1 / std::sqrt (2.0)));
    };
    {
        // N equal first-order allpasses at fc: notches where the phase reaches 180, 540...: f = fc tan (k pi / (2N)), k odd
        const auto p = staticPhase (4);
        const double n1 = phGain (p, 1000 * std::tan (pi / 8)), n2 = phGain (p, 1000 * std::tan (3 * pi / 8)), pk = phGain (p, 1000);
        check (n1 < -30 && n2 < -30 && pk > -1, "Twelve Phase 2: 4 stages put notches exactly at fc tan(22.5) and fc tan(67.5) (dB)", n1);
        int notches = 0; double prev = 0, prev2 = 0; const auto p12 = staticPhase (12);
        for (double f = 30; f < 20000; f *= 1.01) { const double g = phGain (p12, f); if (prev < prev2 && prev < g && prev < -15) ++notches; prev2 = prev; prev = g; if (f > 400 && notches == 0 && false) break; }
        check (notches == 6, "Twelve Phase 2: 12 stages = 6 notches", notches);
    }
    {
        auto p = staticPhase (24); p.resonance = 1; p.mod.rateHz = 0.3; p.sweepOct = 4; p.classic = true;
        Phase2::Processor f; f.p = p; f.prepare (SR, 256); std::vector<float> l, r; TestSignals::pink (l, r, SR, 2.0, -6); run (f, l, r);
        float pk = 0; bool fin = true; for (auto v : l) { fin = fin && std::isfinite (v); pk = std::max (pk, std::abs (v)); }
        check (fin && pk < 2.5f, "Twelve Phase 2: 24 stages at full resonance held by the safety limiter (peak)", pk);
    }
    {
        // through-zero, inverted: the output vanishes as the sweep crosses the centre
        auto p = staticPhase (8); p.throughZero = true; p.polarity = 1; p.sweepOct = 2; p.mod.mode = WingMod::lfo; p.mod.shape = WingMod::triangle; p.mod.rateHz = 0.5;
        Phase2::Processor f; f.p = p; f.prepare (SR, 256); std::vector<float> l, r; TestSignals::pink (l, r, SR, 4.0, -18);
        double mn = 1e9, mx = 0;
        for (size_t i = 0; i + 256 <= l.size(); i += 256)
        {
            float* ch[2] = { l.data() + i, r.data() + i }; f.process (ch, 2, nullptr, 256);
            if (i > 48000) { double e = 0; for (int k = 0; k < 256; ++k) e += l[i + k] * l[i + k]; e = std::sqrt (e / 256); mn = std::min (mn, e); mx = std::max (mx, e); }
        }
        check (db (mx / std::max (1e-9, mn)) > 25, "Twelve Phase 2: through-zero cancels as the sweep crosses (dB range of the output)", db (mx / mn));
    }
    {
        auto a = staticPhase (6); a.classic = true; auto b = a; b.classic = false;
        const double ga = phGain (a, 1000 * std::tan (pi / 12)), gb = phGain (b, 1000 * std::tan (pi / 12));
        check (gb < -30 && ga > gb + 6, "Twelve Phase 2: Classic spreads its cells (the Modern notch is not where Classic's is, dB)", ga - gb);
    }
    {
        auto p = staticPhase (12); p.mod.mode = WingMod::envelope; p.mod.bipolar = true; p.keyOn = true; p.sweepOct = 4;
        Phase2::Processor f; f.p = p; f.prepare (SR, 256); auto l = sine (500, 1.0), r = l; std::vector<float> key (l.size(), 0.0f); for (size_t i = 24000; i < key.size(); ++i) key[i] = 0.7f;
        float a = 0, b = 0;
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; const float* k[2] = { key.data() + i, key.data() + i }; f.process (ch, 2, k, 256); if (i == 20224) a = f.sweepNow(); b = f.sweepNow(); }
        check (b > a + 0.5f, "Twelve Phase 2: an external key drives the sweep through the envelope", b - a);
    }
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
