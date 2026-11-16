// Apex Multiband 2 DSP core: measured. Build: scripts/build_mb2_test.sh
#include "SqApexMB2Dsp.h"
#include <cstdio>
#include <chrono>
#include <functional>

using namespace SqMB2;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }
static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }

static Params neutral (int bands, int phase)
{
    Params p; p.numBands = bands; p.phase = phase;
    for (auto& b : p.band) { b.ratio = 1; b.rangeDb = 0; b.thrDb = 0; }
    return p;
}

static void run (Engine& e, std::vector<float>& l, std::vector<float>& r, const std::vector<float>* kl = nullptr, std::function<void (Engine&, int)> per = {})
{
    for (int s = 0; s < (int) l.size(); s += 256)
    {
        const int n = std::min (256, (int) l.size() - s);
        if (per) per (e, s);
        float* ch[2] = { l.data() + s, r.data() + s };
        if (kl != nullptr) { const float* k[1] = { kl->data() + s }; e.process (ch, 2, k, 1, n); }
        else e.process (ch, 2, nullptr, 0, n);
    }
}

static std::vector<float> sine (double fs, double f, double dbfs, double sec)
{
    std::vector<float> v ((size_t) (fs * sec)); const double a = dbToGain (dbfs);   // dBFS peak (the detectors are sine-peak referenced)
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (a * std::sin (2 * pi * f * i / fs));
    return v;
}

int main()
{
    juce::ScopedNoDenormals nd;
    const double fs = 48000;
    std::printf ("Apex Multiband 2 DSP\n");

    // 1. Transparency: unprocessed bands sum back to the input
    {
        std::vector<float> l ((size_t) fs * 2), r; juce::Random rnd (4); for (auto& v : l) v = (float) (rnd.nextDouble() * 2 - 1) * 0.5f; r = l;
        for (int nb : { 2, 4, 6 })
        {
            Engine e; e.p = neutral (nb, 1); e.prepare (fs, 256);
            auto ol = l, orr = r; run (e, ol, orr);
            const int D = e.latency(); double err = 0, ref = 0;
            for (int i = fs / 2; i < (int) l.size(); ++i) { const double d = ol[(size_t) i] - l[(size_t) (i - D)]; err += d * d; ref += (double) l[(size_t) (i - D)] * l[(size_t) (i - D)]; }
            check (10 * std::log10 (err / ref) < -90, "Linear phase, " + juce::String (nb) + " bands: the sum nulls against the delayed input at " + juce::String (10 * std::log10 (err / ref), 1) + " dB (latency " + juce::String (D / fs * 1000, 1) + " ms)");
        }
        // minimum phase: flat magnitude (all-pass) - measured on sines through 6 bands
        double worst = 0;
        for (double f : { 30.0, 100.0, 120.0, 300.0, 500.0, 1000.0, 2000.0, 4000.0, 6000.0, 9000.0, 12000.0, 18000.0 })
        {
            Engine e; e.p = neutral (6, 0); e.prepare (fs, 256);
            auto a = sine (fs, f, -12, 1.0), b = a; auto in = a; run (e, a, b);
            worst = std::max (worst, std::abs (db (rms (a, (int) (0.5 * fs), (int) fs) / rms (in, (int) (0.5 * fs), (int) fs))));
        }
        check (worst < 0.01, "Minimum phase, 6 bands: flat to " + juce::String (worst, 4) + " dB from 30 Hz to 18 kHz, zero latency");
    }
    // 2. Downward compression lands on the curve
    {
        Params p = neutral (4, 0); auto& b = p.band[2]; b.mode = compressDown; b.thrDb = -30; b.ratio = 4; b.rangeDb = 40; b.kneeDb = 0; b.attackMs = 1; b.releaseMs = 50;
        p.detector = Apex::Detector::rms; p.xover = { 100, 200, 5000, 9000, 12000 };   // a wide band 3, so the tone sits fully inside it
        Engine e; e.p = p; e.prepare (fs, 256);
        auto l = sine (fs, 1000, -10, 1.5), r = l; auto in = l; run (e, l, r);
        const double gr = db (rms (l, (int) fs, (int) (1.5 * fs)) / rms (in, (int) fs, (int) (1.5 * fs)));
        check (std::abs (gr - (-15.0)) < 0.6, "Compress 4:1 at -30 dB, -10 dB sine in band 3: " + juce::String (gr, 2) + " dB (curve: -15.0)");
        e.p.band[2].rangeDb = 6; auto l2 = sine (fs, 1000, -10, 1.5), r2 = l2; run (e, l2, r2);
        const double gr2 = db (rms (l2, (int) fs, (int) (1.5 * fs)) / rms (in, (int) fs, (int) (1.5 * fs)));
        check (std::abs (gr2 - (-6.0)) < 0.3, "Range 6 dB caps it at " + juce::String (gr2, 2) + " dB");
        // band isolation: a 1 kHz tone does nothing to band 4's gain
        check (std::abs (e.meterGrDb[3].load()) < 0.01 && std::abs (e.meterGrDb[0].load()) < 0.01, "Bands 1 and 4 stay untouched by a band-3 tone");
    }
    // 3. Upward compression lifts the quiet, capped by the range
    {
        Params p = neutral (3, 0); auto& b = p.band[1]; b.mode = compressUp; b.thrDb = -20; b.ratio = 2; b.rangeDb = 8; b.kneeDb = 0; p.detector = Apex::Detector::rms;
        p.xover = { 300, 3000, 6000, 9000, 12000 };
        Engine e; e.p = p; e.prepare (fs, 256);
        auto l = sine (fs, 1000, -30, 1.5), r = l; auto in = l; run (e, l, r);
        const double g = db (rms (l, (int) fs, (int) (1.5 * fs)) / rms (in, (int) fs, (int) (1.5 * fs)));
        check (std::abs (g - 5.0) < 0.5, "Compress up 2:1 at -20 dB lifts a -30 dB tone by " + juce::String (g, 2) + " dB (curve: +5.0)");
        auto l2 = sine (fs, 1000, -60, 1.5), r2 = l2; auto in2 = l2; run (e, l2, r2);
        const double g2 = db (rms (l2, (int) fs, (int) (1.5 * fs)) / rms (in2, (int) fs, (int) (1.5 * fs)));
        check (std::abs (g2 - 8.0) < 0.5, "...and a -60 dB tone by " + juce::String (g2, 2) + " dB (range 8)");
    }
    // 4. Expansion
    {
        Params p = neutral (3, 0); auto& b = p.band[1]; b.mode = expandDown; b.thrDb = -30; b.ratio = 2; b.rangeDb = 30; b.kneeDb = 0; p.detector = Apex::Detector::rms;
        p.xover = { 300, 3000, 6000, 9000, 12000 };
        Engine e; e.p = p; e.prepare (fs, 256);
        auto l = sine (fs, 1000, -40, 1.5), r = l; auto in = l; run (e, l, r);
        const double g = db (rms (l, (int) fs, (int) (1.5 * fs)) / rms (in, (int) fs, (int) (1.5 * fs)));
        check (std::abs (g + 10.0) < 0.6, "Expand 1:2 below -30 dB: a -40 dB tone drops " + juce::String (g, 2) + " dB (curve: -10.0)");
    }
    // 5. Mid / side per band
    {
        Params p = neutral (3, 0); auto& b = p.band[1]; b.mode = compressDown; b.thrDb = -40; b.ratio = 10; b.rangeDb = 12; b.kneeDb = 0; b.place = side;
        p.xover = { 150, 7000, 9000, 10000, 12000 };
        Engine e; e.p = p; e.prepare (fs, 256);
        auto m = sine (fs, 1000, -12, 1.5), s = sine (fs, 1100, -12, 1.5);
        std::vector<float> l (m.size()), r (m.size()); for (size_t i = 0; i < m.size(); ++i) { l[i] = m[i] + s[i]; r[i] = m[i] - s[i]; }
        run (e, l, r);
        std::vector<float> om (m.size()), os (m.size()); for (size_t i = 0; i < m.size(); ++i) { om[i] = 0.5f * (l[i] + r[i]); os[i] = 0.5f * (l[i] - r[i]); }
        const double dm = db (rms (om, (int) fs, (int) (1.5 * fs)) / rms (m, (int) fs, (int) (1.5 * fs))), ds = db (rms (os, (int) fs, (int) (1.5 * fs)) / rms (s, (int) fs, (int) (1.5 * fs)));
        check (std::abs (dm) < 0.05 && std::abs (ds + 12.0) < 0.3, "Side-only band: mid " + juce::String (dm, 3) + " dB, side " + juce::String (ds, 2) + " dB");
    }
    // 6. External key per band
    {
        Params p = neutral (3, 0); auto& b = p.band[0]; b.mode = compressDown; b.thrDb = -30; b.ratio = 20; b.rangeDb = 10; b.kneeDb = 0; b.key = 1; b.attackMs = 1;
        p.xover = { 300, 3000, 6000, 9000, 12000 };
        Engine e; e.p = p; e.prepare (fs, 256);
        auto l = sine (fs, 80, -40, 1.5), r = l; auto in = l; auto key = sine (fs, 60, -6, 1.5);
        run (e, l, r, &key);
        const double g = db (rms (l, (int) fs, (int) (1.5 * fs)) / rms (in, (int) fs, (int) (1.5 * fs)));
        check (std::abs (g + 10.0) < 0.5, "Band 1 keyed by an external kick: a quiet bass ducks " + juce::String (g, 2) + " dB (range 10)");
    }
    // 7. Lookahead: latency and transient catch
    {
        Params p = neutral (2, 0); auto& b = p.band[1]; b.mode = compressDown; b.thrDb = -30; b.ratio = 20; b.rangeDb = 24; b.kneeDb = 0; b.attackMs = 1; b.releaseMs = 100;
        p.detector = Apex::Detector::peak; p.xover = { 200, 3000, 6000, 9000, 12000 };
        auto burst = [&] { std::vector<float> v ((size_t) fs, 0.0f); for (int i = (int) (0.5 * fs); i < (int) (0.6 * fs); ++i) v[(size_t) i] = (float) (0.9 * std::sin (2 * pi * 2000 * i / fs)); return v; };
        double over[2];
        for (int la = 0; la < 2; ++la)
        {
            p.lookaheadMs = la ? 5 : 0; Engine e; e.p = p; e.prepare (fs, 256);
            auto l = burst(), r = l; run (e, l, r);
            double pk = 0; for (auto v : l) pk = std::max (pk, (double) std::abs (v)); over[la] = db (pk);
            if (la) check (e.latency() == (int) std::lround (0.005 * fs), "Lookahead 5 ms reports " + juce::String (e.latency()) + " samples");
        }
        check (over[1] < over[0] - 6, "Lookahead catches the onset: peak " + juce::String (over[0], 1) + " dB without, " + juce::String (over[1], 1) + " dB with");
    }
    // 8. Mix 0 = dry; bypassed band = transparent; solo
    {
        std::vector<float> l ((size_t) fs), r; juce::Random rnd (9); for (auto& v : l) v = (float) (rnd.nextDouble() * 2 - 1) * 0.5f; r = l;
        Params p = neutral (4, 0); p.mix = 0; for (auto& b : p.band) { b.ratio = 8; b.rangeDb = 20; b.thrDb = -40; }
        Engine e; e.p = p; e.prepare (fs, 256); auto ol = l, orr = r; run (e, ol, orr);
        bool same = true; for (size_t i = 0; i < l.size(); ++i) if (ol[i] != l[i]) same = false;
        check (same, "Mix 0 % (minimum phase, no lookahead) is the input, bit-exact");
        Params q = neutral (4, 0); q.band[2].solo = true; Engine e2; e2.p = q; e2.prepare (fs, 256);
        auto a = sine (fs, 1000, -12, 1.0), a2 = a; auto c = sine (fs, 100, -12, 1.0), c2 = c; run (e2, a, a2);
        Engine e3; e3.p = q; e3.prepare (fs, 256); run (e3, c, c2);
        check (db (rms (c, fs / 2, (int) fs)) < -50 && db (rms (a, fs / 2, (int) fs)) > -17, "Solo band 3 (500 Hz-2 kHz): 1 kHz passes " + juce::String (db (rms (a, fs / 2, (int) fs)), 1) + " dB, 100 Hz " + juce::String (db (rms (c, fs / 2, (int) fs)), 1) + " dB");
    }
    // 9. Crossovers moving in linear phase: the new filters cross-fade in, no burst
    {
        std::vector<float> l ((size_t) fs * 3), r; juce::Random rnd (2); for (auto& v : l) v = (float) (rnd.nextDouble() * 2 - 1) * 0.25f; r = l;
        Params p = neutral (4, 1); Engine e; e.p = p; e.prepare (fs, 256);
        double pk = 0;
        run (e, l, r, nullptr, [&] (Engine& en, int s) { if (s == (int) fs) { en.p.xover[1] = 900; en.buildLinearIfNeeded(); } });
        for (auto v : l) pk = std::max (pk, (double) std::abs (v));
        check (pk < 0.3, "Linear phase: moving a crossover mid-stream stays transparent (peak " + juce::String (pk, 3) + ", input 0.25)");
    }
    // 10. Stability and CPU
    {
        std::vector<float> l ((size_t) fs * 10), r; juce::Random rnd (3); for (auto& v : l) v = (float) (rnd.nextDouble() * 2 - 1); r = l;
        Params p; p.numBands = 6; p.lookaheadMs = 3;
        for (int k = 0; k < 6; ++k) { auto& b = p.band[(size_t) k]; b.mode = k % 4; b.thrDb = -30; b.ratio = 8; b.rangeDb = 24; b.place = k % 5; b.attackMs = 0.1; b.releaseMs = 20; b.gainDb = 6; }
        Engine e; e.p = p; e.prepare (fs, 256);
        const auto t0 = std::chrono::steady_clock::now(); run (e, l, r);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool fin = true; for (auto v : l) if (! std::isfinite (v)) fin = false;
        check (fin, "Six bands, every mode and place, extreme settings: finite");
        check (sec / 10 < 0.06, "CPU, six bands minimum phase + lookahead: " + juce::String (sec / 10 * 100, 2) + " % of one core");
        std::vector<float> l2 ((size_t) fs * 10), r2; for (auto& v : l2) v = (float) (rnd.nextDouble() * 2 - 1) * 0.5f; r2 = l2;
        p.phase = 1; Engine e2; e2.p = p; e2.prepare (fs, 256);
        const auto t1 = std::chrono::steady_clock::now(); run (e2, l2, r2);
        const double sec2 = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count();
        check (sec2 / 10 < 0.10, "CPU, six bands linear phase: " + juce::String (sec2 / 10 * 100, 2) + " % of one core");
    }
    std::printf ("== %d passed, %d failed ==\n", passed, failed);
    return failed ? 1 : 0;
}
