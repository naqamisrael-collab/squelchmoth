// Foil Plate measurements. g++ -O2 -std=c++17 -I../Source foilplate_test.cpp -o /tmp/fp && /tmp/fp
#include "SqFoilPlateDsp.h"
#include <chrono>
#include <cstdio>
#include <string>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const std::string& w) { (ok ? passes : fails)++; std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", w.c_str()); }
static std::string f2 (double v, int d = 2) { char b[64]; std::snprintf (b, sizeof b, "%.*f", d, v); return b; }
/** RT60 of a band from an impulse response (Schroeder backward integration, -5 .. -25 dB fit -> T20 x 3). */
static double rt60 (const std::vector<float>& ir, double sr, double hz)
{
    Biquad b1, b2; b1.set (Biquad::bandpass, sr, hz, 1.4); b2.set (Biquad::bandpass, sr, hz, 1.4);
    std::vector<double> e (ir.size()); for (size_t i = 0; i < ir.size(); ++i) { const double v = b2.process (b1.process (ir[i])); e[i] = v * v; }
    for (size_t i = e.size() - 1; i-- > 0;) e[i] += e[i + 1];
    const double e0 = e[0]; int t5 = -1, t25 = -1;
    for (size_t i = 0; i < e.size(); ++i) { const double db = 10 * std::log10 (e[i] / e0 + 1e-30); if (t5 < 0 && db < -5) t5 = (int) i; if (t25 < 0 && db < -25) { t25 = (int) i; break; } }
    return t5 > 0 && t25 > t5 ? 3.0 * (t25 - t5) / sr : -1;
}
static std::vector<float> impulse (FoilPlate::Processor& p, double sr, double secs)
{
    std::vector<float> l ((size_t) (sr * secs), 0.0f), r = l; l[0] = r[0] = 0.5f;
    for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, 256); }
    return l;
}
int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    const double sr = 48000;
    std::printf ("== Foil Plate ==\n");
    // decay accuracy at 500 Hz for every archetype at 1.5 s and 4 s; highs decay faster than mids
    for (int a = 0; a < FoilPlate::numArchetypes(); ++a)
    {
        for (float d : { 1.5f, 4.0f })
        {
            FoilPlate::Processor p; p.p.archetype = a; p.p.decayS = d; p.p.mix = 1; p.p.input = FoilPlate::clean; p.p.mod = 0; p.p.preDelayMs = 0; p.p.lowCut = 20; p.p.highCut = 20000; p.prepare (sr, 256);
            auto ir = impulse (p, sr, d * 1.6 + 1.0);
            const double m = rt60 (ir, sr, 500), h = rt60 (ir, sr, 6000);
            float lo, mid, hi; p.bandDecays (lo, mid, hi);
            check (std::abs (m - d) / d < 0.15 && h < m, std::string (FoilPlate::archetype (a).name) + " " + f2 (d, 1) + " s: RT60 at 500 Hz " + f2 (m) + " s, at 6 kHz " + f2 (h) + " s (set " + f2 (hi) + ")");
        }
    }
    // true stereo: a hard-left input reaches both sides at default width
    {
        FoilPlate::Processor p; p.p.mix = 1; p.prepare (sr, 256);
        std::vector<float> l (sr * 2, 0.0f), r (sr * 2, 0.0f); l[0] = 0.5f;
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, 256); }
        double el = 0, er = 0, x = 0; for (size_t i = 0; i < l.size(); ++i) { el += l[i] * l[i]; er += r[i] * r[i]; x += l[i] * r[i]; }
        const double bal = 10 * std::log10 (er / el), corr = x / std::sqrt (el * er);
        check (std::abs (bal) < 3.0 && corr < 0.5, "hard-left input: right side " + f2 (bal, 1) + " dB vs left, L/R correlation " + f2 (corr, 2) + " (a true stereo spread)");
    }
    // auto-decay lengthens and shortens
    {
        auto tailAfter = [&] (float ad)
        {
            FoilPlate::Processor p; p.p.mix = 1; p.p.decayS = 2.0f; p.p.autoDecay = ad; p.p.autoThresholdDb = -40; p.p.input = FoilPlate::clean; p.prepare (sr, 256);
            std::vector<float> l, r; TestSignals::pink (l, r, sr, 2.0, -18); l.resize (sr * 4, 0.0f); r.resize (sr * 4, 0.0f);
            float mulDuring = 1; size_t i = 0;
            for (; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, 256); if (i >= (size_t) sr && i < (size_t) sr + 256) mulDuring = p.autoDecayMul(); }
            return mulDuring;
        };
        const float shorter = tailAfter (-1.0f), longer = tailAfter (1.0f);
        check (shorter < 0.3f && longer > 2.5f, "auto-decay: while playing the decay is x" + f2 (shorter) + " (shorten) or x" + f2 (longer) + " (lengthen)");
    }
    // freeze holds; darkening loses highs over time
    {
        FoilPlate::Processor p; p.p.mix = 1; p.p.decayS = 3; p.p.input = FoilPlate::clean; p.prepare (sr, 256);
        std::vector<float> l, r; TestSignals::pink (l, r, sr, 1.0, -18); l.resize (sr * 8, 0.0f); r.resize (sr * 8, 0.0f);
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { if (i > sr * 0.9) p.p.freeze = true; float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, 256); }
        double e1 = 0, e2 = 0; for (int i = sr * 3; i < sr * 4; ++i) e1 += l[i] * l[i]; for (int i = sr * 7; i < sr * 8; ++i) e2 += l[i] * l[i];
        bool fin = true; for (float x : l) fin = fin && std::isfinite (x);
        check (fin && e1 > 0 && std::abs (10 * std::log10 (e2 / e1)) < 1.0, "freeze holds the tail (change over 4 s " + f2 (10 * std::log10 (e2 / e1), 2) + " dB)");
    }
    // input stages: audible, measured
    for (int s = 0; s < FoilPlate::numInputs; ++s)
    {
        FoilPlate::Processor p; p.p.input = s; p.p.inputDrive = 0.5f; p.prepare (sr, 256);
        const double thd = p.inputThd();
        check (s == FoilPlate::clean ? thd == 0.0 : (thd > 0.5 && thd < 8.0), std::string ("input ") + FoilPlate::inputName (s) + ": THD " + f2 (thd, 2) + " % at -18 dBFS");
    }
    // CPU
    {
        FoilPlate::Processor p; p.p.mix = 0.3f; p.prepare (sr, 128);
        std::vector<float> l, r; TestSignals::drums (l, r, sr, 10.0);
        auto t0 = std::chrono::steady_clock::now(); for (size_t i = 0; i + 128 <= l.size(); i += 128) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, 128); } auto t1 = std::chrono::steady_clock::now();
        const double pct = std::chrono::duration<double> (t1 - t0).count() / 10.0 * 100;
        check (pct < 4.0, "CPU at 48 kHz / 128: " + f2 (pct, 2) + " % of one core (budget 4 %)");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passes, fails);
    return fails ? 1 : 0;
}
