// Kiln DSP core: measured. Build: scripts/build_kiln_test.sh
#include "SqKilnDsp.h"
#include <cstdio>
#include <chrono>
#include <complex>
#include <functional>

using namespace SqKiln;
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
static std::vector<float> tone (double fs, double sec, std::initializer_list<std::pair<double, double>> parts)
{
    std::vector<float> v ((size_t) (fs * sec));
    for (size_t i = 0; i < v.size(); ++i) { double s = 0; for (auto [f, d] : parts) s += dbToGain (d) * std::sin (2 * pi * f * i / fs); v[i] = (float) s; }
    return v;
}
/** Amplitude (dB) of frequency f in v[from..from+N) by single-bin DFT with a Hann window. */
static double binDb (const std::vector<float>& v, double fs, double f, int from, int N)
{
    std::complex<double> acc = 0; double wsum = 0;
    for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * pi * i / (N - 1)); acc += w * (double) v[(size_t) (from + i)] * std::polar (1.0, -2 * pi * f * i / fs); wsum += w; }
    return db (2.0 * std::abs (acc) / wsum);
}
static Params clean (int bands)
{
    Params p; p.numBands = bands; p.autoGain = false;
    for (auto& b : p.band) { b.driveDb = -12; b.mix = 0; }
    return p;
}

int main()
{
    juce::ScopedNoDenormals nd;
    const double fs = 48000;
    std::printf ("Kiln DSP\n");

    // 1. Nothing driven: flat across the band
    {
        double worst = 0;
        for (double f : { 60.0, 200.0, 800.0, 2500.0, 5000.0, 9000.0, 15000.0 })
        {
            Engine e; e.p = clean (6); e.p.xover = { 120, 500, 2000, 6000, 12000 }; e.prepare (fs, 256);
            auto l = tone (fs, 1.0, { { f, -12 } }), r = l; const auto in = l; run (e, l, r);
            worst = std::max (worst, std::abs (db (rms (l, (int) (0.5 * fs), (int) fs) / rms (in, (int) (0.5 * fs), (int) fs))));
        }
        check (worst < 0.02, "Six bands, nothing driven: flat to " + juce::String (worst, 4) + " dB from 60 Hz to 15 kHz");
    }
    // 2. Band isolation: drive only the low band; a 3 kHz tone in band 2 stays clean
    {
        Params p = clean (3); p.xover = { 300, 6000, 9000, 10000, 12000 }; p.band[0].driveDb = 30; p.band[0].mix = 1; p.band[0].model = Cinder::diodeFuzz; p.oversampleLog2 = 2;
        Engine e; e.p = p; e.prepare (fs, 256);
        auto l = tone (fs, 1.5, { { 100, -10 }, { 3050, -20 } }), r = l; run (e, l, r);   // 3050: off the bass's own harmonic grid
        const int from = (int) (0.75 * fs), N = 16384;
        const double h3_100 = binDb (l, fs, 300, from, N), f3k = binDb (l, fs, 3050, from, N), side = std::max (binDb (l, fs, 3150, from, N), binDb (l, fs, 2950, from, N));
        check (h3_100 > -45, "Low band driven: the 100 Hz bass grows a 3rd harmonic at " + juce::String (h3_100, 1) + " dB");
        check (std::abs (f3k - (-20)) < 0.5 && side < f3k - 50, "...while the 3.05 kHz tone in band 2 stays at " + juce::String (f3k, 2) + " dB with no intermodulation (sidebands " + juce::String (side - f3k, 1) + " dB)");
    }
    // 3. Each band has its own model: triode (even) vs pentode (odd)
    {
        auto harm = [&] (int model)
        {
            Params p = clean (1); p.band[0].model = model; p.band[0].driveDb = 18; p.band[0].mix = 1;
            Engine e; e.p = p; e.prepare (fs, 256); auto l = tone (fs, 1.0, { { 500, -12 } }), r = l; run (e, l, r);
            return std::pair<double, double> { binDb (l, fs, 1000, (int) (0.5 * fs), 16384), binDb (l, fs, 1500, (int) (0.5 * fs), 16384) };
        };
        const auto tri = harm (Cinder::triodeBox), pen = harm (Cinder::pentodeBox);
        check (tri.first > tri.second && pen.second > pen.first, "Triode band: H2 " + juce::String (tri.first, 1) + " > H3 " + juce::String (tri.second, 1) + "; pentode band: H3 " + juce::String (pen.second, 1) + " > H2 " + juce::String (pen.first, 1));
    }
    // 4. The LFO moves a band's drive
    {
        Params p = clean (1); p.band[0].driveDb = 6; p.band[0].mix = 1; p.band[0].model = Cinder::pentodeBox; p.lfoHz = 2; p.lfoShape = square;
        p.slot[0] = { srcLfo, 1, 18.0f };
        Engine e; e.p = p; e.prepare (fs, 256);
        auto l = tone (fs, 2.0, { { 500, -12 } }), r = l;
        std::vector<float> drives; run (e, l, r, [&] (Engine& en, int) { drives.push_back (en.meterDriveDb[0]); });
        float lo = 1e9f, hi = -1e9f; for (auto d : drives) { lo = std::min (lo, d); hi = std::max (hi, d); }
        const double h3a = binDb (l, fs, 1500, (int) (0.02 * fs), 8192), h3b = binDb (l, fs, 1500, (int) (0.27 * fs), 8192);
        check (std::abs (hi - 24) < 0.01 && std::abs (lo - (-12)) < 0.01, "LFO (square, +-18 dB) swings band 1's drive between " + juce::String (lo, 1) + " and " + juce::String (hi, 1) + " dB");
        check (std::abs (h3a - h3b) > 10, "...and the harmonics follow (3rd harmonic " + juce::String (h3a, 1) + " dB vs " + juce::String (h3b, 1) + " dB)");
    }
    // 5. The envelope follower opens the mix on loud passages
    {
        Params p = clean (1); p.band[0].driveDb = 20; p.band[0].mix = 0; p.band[0].model = Cinder::diodeFuzz; p.slot[0] = { srcEnv, 7, 1.0f }; p.envAttackMs = 5; p.envReleaseMs = 50;
        Engine e; e.p = p; e.prepare (fs, 256);
        auto l = tone (fs, 2.0, { { 400, -50 } }); for (int i = (int) fs; i < (int) l.size(); ++i) l[(size_t) i] *= (float) dbToGain (44); auto r = l;
        run (e, l, r);
        const double quietH3 = binDb (l, fs, 1200, (int) (0.4 * fs), 16384) - binDb (l, fs, 400, (int) (0.4 * fs), 16384), loudH3 = binDb (l, fs, 1200, (int) (1.4 * fs), 16384) - binDb (l, fs, 400, (int) (1.4 * fs), 16384);
        check (loudH3 > quietH3 + 20, "Envelope -> band mix: quiet passage H3 " + juce::String (quietH3, 1) + " dBc, loud passage " + juce::String (loudH3, 1) + " dBc");
    }
    // 6. Oversampling keeps aliases down
    {
        auto alias = [&] (int os)
        {
            Params p = clean (1); p.band[0].driveDb = 30; p.band[0].mix = 1; p.band[0].model = Cinder::opAmpRail; p.oversampleLog2 = os;
            Engine e; e.p = p; e.prepare (fs, 256); auto l = tone (fs, 1.0, { { 7919, -6 } }), r = l; run (e, l, r);
            // an alias of the 5th harmonic (39595 Hz) folds to 48000 - 39595 = 8405 Hz
            return binDb (l, fs, 8405, (int) (0.5 * fs), 16384) - binDb (l, fs, 7919, (int) (0.5 * fs), 16384);
        };
        const double a1 = alias (0), a8 = alias (3);
        check (a8 < -60 && a8 < a1 - 20, "Hard op-amp clip on 7.9 kHz: the folded 5th harmonic is " + juce::String (a1, 1) + " dBc at 1x, " + juce::String (a8, 1) + " dBc at 8x");
    }
    // 7. Latency: the sum lines up with the reported latency, every oversampling setting
    {
        bool ok = true; juce::String msg;
        for (int os : { 0, 2, 4 })
        {
            Params p = clean (4); p.oversampleLog2 = os; p.linearPhaseOs = true; p.mix = 0.5f;
            Engine e; e.p = p; e.prepare (fs, 256);
            std::vector<float> l ((size_t) fs / 2, 0.0f); l[1000] = 1; auto r = l; run (e, l, r);
            int pk = 0; for (int i = 0; i < (int) l.size(); ++i) if (std::abs (l[(size_t) i]) > std::abs (l[(size_t) pk])) pk = i;
            const int D = pk - 1000; if (std::abs (D - e.latency()) > 2) ok = false;
            msg << juce::String (1 << os) << "x: " << D << " / " << e.latency() << "; ";
        }
        check (ok, "Latency matches the report, linear-phase oversampling, wet/dry 50 % (" + msg.trimEnd() + ")");
    }
    // 8. Stability and CPU
    {
        std::vector<float> l ((size_t) fs * 10), r; juce::Random rnd (2); for (auto& v : l) v = (float) (rnd.nextDouble() * 2 - 1); r = l;
        Params p; p.numBands = 6; p.oversampleLog2 = 2; for (int k = 0; k < 6; ++k) { p.band[(size_t) k].model = k; p.band[(size_t) k].driveDb = 36; p.band[(size_t) k].bias = 0.8f; }
        p.slot[0] = { srcLfo, 3, 20 }; p.slot[1] = { srcEnv, 13, -0.5f };
        Engine e; e.p = p; e.prepare (fs, 256);
        const auto t0 = std::chrono::steady_clock::now(); run (e, l, r);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool fin = true; double pk = 0; for (auto v : l) { if (! std::isfinite (v)) fin = false; pk = std::max (pk, (double) std::abs (v)); }
        check (fin && pk < 8, "Six bands at full drive and bias on full-scale noise: finite (peak " + juce::String (pk, 2) + ")");
        check (sec / 10 < 0.25, "CPU, six bands at 4x: " + juce::String (sec / 10 * 100, 1) + " % of one core");
    }
    std::printf ("== %d passed, %d failed ==\n", passed, failed);
    return failed ? 1 : 0;
}
