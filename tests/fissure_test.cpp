// Fissure Filter DSP core: measured. Build: scripts/build_fissure_test.sh
#include "SqFissureDsp.h"
#include <cstdio>
#include <chrono>
#include <functional>

using namespace SqFissure;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }
static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }

static void run (Engine& e, std::vector<float>& l, std::vector<float>& r, std::function<void (Engine&, int)> per = {})
{
    for (int s = 0; s < (int) l.size(); s += 256)
    {
        const int n = std::min (256, (int) l.size() - s);
        if (per) per (e, s);
        float* ch[2] = { l.data() + s, r.data() + s }; e.process (ch, 2, n);
    }
}
static Params one (int type, int slope, double fc, double res = 0)
{
    Params p; for (auto& s : p.slot) s.on = false;
    p.slot[0].on = true; p.slot[0].type = type; p.slot[0].slope = slope; p.slot[0].cutoffHz = fc; p.slot[0].resonance = res;
    return p;
}
/** Gain (dB) of a steady sine through the engine. */
static double gainAt (const Params& p, double f, double fs = 48000)
{
    Engine e; e.p = p; e.prepare (fs, 256);
    std::vector<float> l ((size_t) (fs * 0.6)); for (size_t i = 0; i < l.size(); ++i) l[i] = (float) (0.25 * std::sin (2 * pi * f * i / fs)); auto r = l; const auto in = l;
    run (e, l, r);
    return db (rms (l, (int) (0.4 * fs), (int) (0.6 * fs)) / rms (in, (int) (0.4 * fs), (int) (0.6 * fs)));
}

int main()
{
    juce::ScopedNoDenormals nd;
    const double fs = 48000;
    std::printf ("Fissure Filter DSP\n");

    // 1. Butterworth slopes: -3 dB at the cutoff, and the right fall per octave
    for (int slope : { 12, 24, 48 })
    {
        const double atFc = gainAt (one (lowPass, slope, 1000), 1000), at4 = gainAt (one (lowPass, slope, 1000), 4000);
        const double wr = std::tan (pi * 4000 / fs) / std::tan (pi * 1000 / fs);
        const double expect4 = -10 * std::log10 (1 + std::pow (wr, 2.0 * slope / 6.0));   // digital Butterworth: |H|^2 = 1 / (1 + (tan(pi f/fs) / tan(pi fc/fs))^(2n))
        check (std::abs (atFc + 3.01) < 0.15 && std::abs (at4 - expect4) < 0.6, "Low-pass " + juce::String (slope) + " dB/oct at 1 kHz: " + juce::String (atFc, 2) + " dB at the cutoff, " + juce::String (at4, 1) + " dB two octaves up (Butterworth " + juce::String (expect4, 1) + ")");
    }
    {
        const double atFc = gainAt (one (highPass, 24, 1000), 1000), at = gainAt (one (highPass, 24, 1000), 250);
        const double wr = std::tan (pi * 1000 / fs) / std::tan (pi * 250 / fs);
        check (std::abs (atFc + 3.01) < 0.15 && std::abs (at - (-10 * std::log10 (1 + std::pow (wr, 8.0)))) < 0.6, "High-pass 24: " + juce::String (atFc, 2) + " dB at the cutoff, " + juce::String (at, 1) + " dB two octaves down");
    }
    // 2. Resonance lifts the cutoff, the face's curve matches the sound
    {
        auto p = one (lowPass, 24, 1000, 0.6);
        const double meas = gainAt (p, 1000), drawn = Engine::responseDb (p.slot[0], fs, 1000);
        check (meas > 6 && std::abs (meas - drawn) < 0.3, "Resonance 60 %: +" + juce::String (meas, 1) + " dB at the cutoff (the face draws " + juce::String (drawn, 1) + " dB)");
    }
    // 3. Peak: exactly its gain at the centre; band-pass unity at the centre; notch deep
    {
        auto pk = one (peak, 12, 2000, 0.2); pk.slot[0].gainDb = 9;
        const double g = gainAt (pk, 2000), bp = gainAt (one (bandPass, 12, 2000, 0.3), 2000), nt = gainAt (one (notch, 12, 2000, 0.3), 2000);
        check (std::abs (g - 9) < 0.15 && std::abs (bp) < 0.1 && nt < -40, "Peak +9 dB -> " + juce::String (g, 2) + " dB; band-pass centre " + juce::String (bp, 2) + " dB; notch " + juce::String (nt, 1) + " dB");
    }
    // 4. Comb: peaks at the harmonics of the cutoff, dips between
    {
        auto p = one (comb, 12, 500, 0.8);
        const double on = gainAt (p, 1000), off = gainAt (p, 750);
        check (on > off + 15, "Comb at 500 Hz: harmonic 1 kHz " + juce::String (on, 1) + " dB, between at 750 Hz " + juce::String (off, 1) + " dB");
    }
    // 5. Routing: parallel low-pass 300 + high-pass 3k lets both ends through and scoops the middle; serial passes nothing
    {
        Params p; for (auto& s : p.slot) s.on = false;
        p.slot[0] = { true, lowPass, 24, 300, 0, 0, 0, 0, 1 }; p.slot[1] = { true, highPass, 24, 3000, 0, 0, 0, 0, 1 };
        p.routing = parallel; const double lo = gainAt (p, 100), mid = gainAt (p, 950), hi = gainAt (p, 10000);
        p.routing = serial; const double ser = gainAt (p, 950);
        check (lo > -7 && hi > -7 && mid < lo - 15 && ser < -40, "Parallel LP 300 + HP 3k: 100 Hz " + juce::String (lo, 1) + ", 950 Hz " + juce::String (mid, 1) + ", 10 kHz " + juce::String (hi, 1) + " dB; serial at 950 Hz " + juce::String (ser, 1) + " dB");
    }
    // 6. The LFO sweeps the cutoff by its octaves
    {
        auto p = one (lowPass, 24, 1000); p.lfoShape[0] = square; p.lfoHz[0] = 2; p.mod[0] = { srcLfo1, 1, 2.0 };
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) fs), r = l; float lo = 1e9f, hi = 0;
        run (e, l, r, [&] (Engine& en, int s) { if (s > (int) (0.05 * fs)) { lo = std::min (lo, en.meterCutoff[0]); hi = std::max (hi, en.meterCutoff[0]); } });
        check (std::abs (lo - 250) < 3 && std::abs (hi - 4000) < 40, "LFO (square, +-2 octaves) sweeps the cutoff " + juce::String (lo, 0) + " Hz .. " + juce::String (hi, 0) + " Hz");
    }
    // 7. The envelope opens the filter on loud notes
    {
        auto p = one (lowPass, 24, 300); p.mod[0] = { srcEnv, 1, 4.0 }; p.envAttackMs = 2; p.envReleaseMs = 50;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) fs); for (size_t i = 0; i < l.size(); ++i) l[i] = (float) ((i > fs / 2 ? 0.5 : 0.005) * std::sin (2 * pi * 200 * i / fs)); auto r = l;
        std::vector<float> cut; run (e, l, r, [&] (Engine& en, int) { cut.push_back (en.meterCutoff[0]); });
        const float quiet = cut[(size_t) (0.4 * fs / 256)], loud = cut[(size_t) (0.9 * fs / 256)];
        check (loud > quiet * 3, "Envelope -> cutoff: " + juce::String (quiet, 0) + " Hz when quiet, " + juce::String (loud, 0) + " Hz when loud");
    }
    // 8. Mid / side: the mid's filter leaves the side alone
    {
        Params p; for (auto& s : p.slot) s.on = false; p.routing = midSide; p.slot[0] = { true, lowPass, 48, 200, 0, 0, 0, 0, 1 };
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) fs), r = l;
        for (size_t i = 0; i < l.size(); ++i) { const double m = 0.25 * std::sin (2 * pi * 3000 * i / fs), sd = 0.25 * std::sin (2 * pi * 3300 * i / fs); l[i] = (float) (m + sd); r[i] = (float) (m - sd); }
        run (e, l, r);
        std::vector<float> sd (l.size()); for (size_t i = 0; i < l.size(); ++i) sd[i] = 0.5f * (l[i] - r[i]);
        const double side = db (rms (sd, (int) (0.5 * fs), (int) fs) / (0.25 / std::sqrt (2.0)));
        check (std::abs (side) < 0.05, "Mid / Side: a mid low-pass leaves the side at " + juce::String (side, 3) + " dB");
    }
    // 9. Mix 0 = dry; self-oscillating resonance under fast modulation stays finite and bounded
    {
        auto p = one (lowPass, 24, 1000); p.mix = 0; Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) fs); juce::Random rnd (2); for (auto& v : l) v = (float) (rnd.nextDouble() - 0.5); auto r = l; const auto in = l; run (e, l, r);
        bool same = true; for (size_t i = 0; i < l.size(); ++i) if (l[i] != in[i]) same = false;
        check (same, "Mix 0 % = dry, bit-exact");
        Params q; q.routing = serial;
        for (int k = 0; k < 4; ++k) q.slot[(size_t) k] = { true, k == 2 ? comb : lowPass, 48, 800.0 * (k + 1), 1.0, 12.0, 0, 0, 1 };
        q.lfoShape = { square, randomSteps }; q.lfoHz = { 20, 15 }; q.mod[0] = { srcLfo1, 1, 6 }; q.mod[1] = { srcLfo2, 2, -6 }; q.mod[2] = { srcLfo1, 5, -1 };
        Engine e2; e2.p = q; e2.prepare (fs, 256);
        std::vector<float> a ((size_t) fs * 10); for (auto& v : a) v = (float) (rnd.nextDouble() - 0.5); auto b = a;
        const auto t0 = std::chrono::steady_clock::now(); run (e2, a, b);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        double pk = 0; bool fin = true; for (auto v : a) { if (! std::isfinite (v)) fin = false; pk = std::max (pk, (double) std::abs (v)); }
        check (fin && pk < 50, "Four filters at full resonance and drive, swept +-6 octaves at 20 Hz: finite (peak " + juce::String (pk, 1) + ")");
        check (sec / 10 < 0.05, "CPU, four 48 dB/oct slots + modulation: " + juce::String (sec / 10 * 100, 2) + " % of one core");
    }
    std::printf ("== %d passed, %d failed ==\n", passed, failed);
    return failed ? 1 : 0;
}
