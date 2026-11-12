// Torque Comp DSP core: measured. Build: scripts/build_torque_test.sh
#include "SqTorqueDsp.h"
#include <cstdio>
#include <chrono>

using namespace SqTorque;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }

/** Steady-state output level (dB) for a sine at 'inDb' dBFS peak. */
static double steady (Engine& e, double fs, double inDb, double hz = 1000, double seconds = 2.0)
{
    e.reset();
    const int N = (int) (fs * seconds), B = 512; const double a = std::pow (10.0, inDb / 20.0);
    std::vector<float> l ((size_t) N), r ((size_t) N);
    for (int i = 0; i < N; ++i) l[(size_t) i] = r[(size_t) i] = (float) (a * std::sin (2 * pi * hz * i / fs));
    for (int p = 0; p < N; p += B) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (B, N - p)); }
    double pk = 0; for (int i = N - (int) (fs * 0.2); i < N; ++i) pk = std::max (pk, (double) std::abs (l[(size_t) i]));
    return db (pk);
}

int main()
{
    const double fs = 48000;
    std::printf ("-- the static curve (gain computer) --\n");
    {
        // hard knee 4:1 at -20: +12 dB over -> 3 dB over
        check (std::abs (staticGainDb (-8, -20, 4, 0, 60, false, 0) + 9.0) < 1e-9, "4:1 hard knee: 12 dB over the threshold comes out 3 dB over (-9 dB of gain)");
        check (staticGainDb (-21, -20, 4, 0, 60, false, 0) == 0.0, "below the threshold nothing happens");
        // soft knee: continuous and smooth at both edges
        double worstJump = 0; for (double x = -40; x < 0; x += 0.01) worstJump = std::max (worstJump, std::abs (staticGainDb (x + 0.01, -20, 4, 12, 60, false, 0) - staticGainDb (x, -20, 4, 12, 60, false, 0)));
        check (worstJump < 0.01, "12 dB soft knee: no step anywhere (largest change per 0.01 dB = " + juce::String (worstJump, 5) + " dB)");
        check (std::abs (staticGainDb (-20, -20, 4, 72, 60, false, 0) - (0.25 - 1) * 36 * 36 / 144.0) < 1e-9, "a 72 dB knee is available and centred on the threshold");
        check (std::abs (staticGainDb (0, -40, 100, 0, 12, false, 0) + 12) < 1e-9, "Range caps the gain reduction (12 dB)");
        check (std::abs (staticGainDb (-40, -20, 2, 0, 60, true, 0) - 10) < 1e-9, "upward 2:1: 20 dB under the threshold is brought up 10 dB");
        // variable mu: the effective ratio grows with the push
        const double g6 = staticGainDb (-14, -20, 8, 0, 60, false, 18), g18 = staticGainDb (-2, -20, 8, 0, 60, false, 18);
        check (-g6 / 6 < -g18 / 18 && g18 < -14, "Variable Mu: 6 dB over squeezes gently (" + juce::String (-g6, 2) + " dB), 18 dB over at the full 8:1 (" + juce::String (-g18, 2) + " dB)");
    }

    std::printf ("-- the engine: measured levels --\n");
    for (int style : { 0, 6, 9 })
    {
        Engine e; e.prepare (fs, 512);
        e.p.style = style; e.p.threshDb = -24; e.p.ratio = 4; e.p.kneeDb = 0; e.p.attackMs = 5; e.p.releaseMs = 100;
        const double out = steady (e, fs, -6, 1000, style == 9 ? 4.0 : 2.0);
        // a sine at -6 dBFS peak: the RMS-style detectors see the RMS ( x 1.414 = peak), so expect -24 + 18/4 = -19.5 (+ the style's own knee)
        const double kneeExtra = styles()[(size_t) style].kneeExtra, want = -6 + staticGainDb (-6, -24, 4, kneeExtra, 60, false, 0);
        check (std::abs (out - want) < 0.6, juce::String (styles()[(size_t) style].name) + ": -6 dBFS sine, threshold -24, 4:1 -> " + juce::String (out, 2) + " dBFS (the curve says " + juce::String (want, 2) + ")");
    }
    {
        // attack: time to reach 63 % of the final gain reduction from a step
        Engine e; e.prepare (fs, 512); e.p.style = 0; e.p.threshDb = -30; e.p.ratio = 20; e.p.kneeDb = 0; e.p.attackMs = 10; e.p.releaseMs = 200;
        const int N = (int) (fs * 0.5); std::vector<float> l ((size_t) N, 0.0f), r;
        for (int i = (int) (fs * 0.1); i < N; ++i) l[(size_t) i] = 0.5f;             // a DC step at -6 dBFS
        r = l;
        std::vector<float> gr; gr.reserve ((size_t) N);
        for (int p = 0; p + 64 <= N; p += 64) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, 64); gr.push_back (e.meterGrDb); }
        const double final = gr.back(); int at = -1;
        for (size_t i = 0; i < gr.size(); ++i) if (gr[i] <= 0.632 * final) { at = (int) i; break; }
        const double ms = (at * 64 - fs * 0.1) / fs * 1000.0;
        check (std::abs (ms - 10) < 3.0 && final < -20, "attack 10 ms: 63 % of the gain reduction after " + juce::String (ms, 1) + " ms (" + juce::String (final, 1) + " dB in all)");
    }
    {
        // lookahead: the gain is already down when the transient arrives (output never overshoots the threshold much)
        Engine a; a.prepare (fs, 512); a.p.style = 0; a.p.threshDb = -20; a.p.ratio = 50; a.p.kneeDb = 0; a.p.attackMs = 1; a.p.releaseMs = 100; a.p.lookaheadMs = 0;
        Engine b = Engine(); b.prepare (fs, 512); b.p = a.p; b.p.lookaheadMs = 5;
        auto hit = [&] (Engine& e)
        {
            e.reset(); const int N = (int) (fs * 0.3); std::vector<float> l ((size_t) N, 0.0f), r;
            for (int i = (int) (fs * 0.1); i < (int) (fs * 0.12); ++i) l[(size_t) i] = 0.9f;
            r = l; for (int p = 0; p < N; p += 256) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (256, N - p)); }
            double pk = 0; for (auto v : l) pk = std::max (pk, (double) std::abs (v)); return db (pk);
        };
        const double without = hit (a), with = hit (b);
        check (with < without - 6 && b.latency() == (int) std::lround (0.005 * fs), "5 ms lookahead: a hit peaks at " + juce::String (with, 1) + " dBFS instead of " + juce::String (without, 1) + "; latency " + juce::String (b.latency()) + " samples reported");
    }
    {
        // mix 0 % = dry, 100 % = compressed, 200 % = past it; and the dry is time-aligned (no comb with lookahead)
        Engine e; e.prepare (fs, 512); e.p.style = 0; e.p.threshDb = -30; e.p.ratio = 4; e.p.kneeDb = 0; e.p.lookaheadMs = 4;
        e.p.mix = 0.0; const double d0 = steady (e, fs, -6, 3000);
        e.p.mix = 1.0; const double d1 = steady (e, fs, -6, 3000);
        e.p.mix = 2.0; const double d2 = steady (e, fs, -6, 3000);
        e.p.mix = 0.5; const double dh = steady (e, fs, -6, 3000);
        const double gr1 = d1 + 6, gr2 = d2 + 6;
        check (std::abs (d0 + 6) < 0.05 && d1 < -15 && std::abs (gr2 - 2 * gr1) < 0.3 && dh > d1 && dh < d0,
               "mix: 0 % leaves the sine at " + juce::String (d0, 2) + " dB (the dry is time-aligned with the 4 ms lookahead), 50 % " + juce::String (dh, 1) + ", 100 % " + juce::String (d1, 1)
               + " (" + juce::String (gr1, 1) + " dB of gain change), 200 % " + juce::String (d2, 1) + " (" + juce::String (gr2, 1) + " dB, twice as deep)");
    }
    {
        // upward compression raises quiet material
        Engine e; e.prepare (fs, 512); e.p.style = 10; e.p.threshDb = -20; e.p.ratio = 2; e.p.kneeDb = 0; e.p.rangeDb = 12;
        const double q = steady (e, fs, -40, 1000, 2.0);
        check (q > -40 + 6, "Upward 2:1 (range 12): a -40 dBFS sine comes out at " + juce::String (q, 1) + " dBFS");
    }
    {
        // Mid only: a pure side signal is untouched, a pure mid signal is compressed
        Engine e; e.prepare (fs, 512); e.p.style = 0; e.p.threshDb = -30; e.p.ratio = 8; e.p.kneeDb = 0; e.p.routing = Routing::midOnly;
        auto run = [&] (bool side)
        {
            e.reset(); const int N = (int) fs; std::vector<float> l ((size_t) N), r ((size_t) N);
            for (int i = 0; i < N; ++i) { l[(size_t) i] = (float) (0.5 * std::sin (2 * pi * 1000 * i / fs)); r[(size_t) i] = side ? -l[(size_t) i] : l[(size_t) i]; }
            for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
            double pk = 0; for (int i = N - 4800; i < N; ++i) pk = std::max (pk, (double) std::abs (l[(size_t) i])); return db (pk);
        };
        const double mid = run (false), side = run (true);
        check (std::abs (side + 6.02) < 0.1 && mid < -15, "Mid Only: a side signal passes untouched (" + juce::String (side, 2) + " dB), a mid signal is compressed (" + juce::String (mid, 1) + " dB)");
    }
    {
        // key EQ: a high-pass in the key stops a loud 60 Hz tone from triggering
        Engine e; e.prepare (fs, 512); e.p.style = 0; e.p.threshDb = -30; e.p.ratio = 8; e.p.kneeDb = 0;
        const double open = steady (e, fs, -6, 60);
        SqSpectra::Band hp; hp.enabled = true; hp.shape = SqSpectra::Shape::lowCut; hp.freq = 250; hp.slope = 24; hp.q = 0.707; e.p.keyEq[0] = hp;
        const double filtered = steady (e, fs, -6, 60);
        check (open < -15 && filtered > -8, "key EQ: a 250 Hz low cut in the key stops a 60 Hz tone triggering (" + juce::String (open, 1) + " -> " + juce::String (filtered, 1) + " dBFS)");
    }
    {
        // character: 4x oversampling cuts the aliasing of a 9 kHz tone driven hard
        auto alias = [&] (int osLog2)
        {
            Engine e; e.prepare (fs, 512); e.p.style = 0; e.p.threshDb = 0; e.p.ratio = 1; e.p.characterMode = 3; e.p.driveDb = 18; e.p.oversampleLog2 = osLog2;
            const int N = 32768; std::vector<float> l ((size_t) N), r ((size_t) N);
            for (int i = 0; i < N; ++i) l[(size_t) i] = r[(size_t) i] = (float) (0.7 * std::sin (2 * pi * 9000.0 * i / fs));
            for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
            // energy at the folded 3rd harmonic (27 kHz -> 21 kHz) vs the fundamental, by correlation over the last 16k samples
            auto amp = [&] (double hz) { double si = 0, co = 0; for (int i = N - 16384; i < N; ++i) { si += l[(size_t) i] * std::sin (2 * pi * hz * i / fs); co += l[(size_t) i] * std::cos (2 * pi * hz * i / fs); } return 2 * std::sqrt (si * si + co * co) / 16384; };
            return db (amp (48000 - 27000) / amp (9000));
        };
        const double a1 = alias (0), a4 = alias (2), a32 = alias (5);
        check (a4 < a1 - 30 && a32 < -60, "character, 9 kHz driven 18 dB: the folded 3rd harmonic is " + juce::String (a1, 1) + " dB at 1x, " + juce::String (a4, 1) + " dB at 4x, " + juce::String (a32, 1) + " dB at 32x");
    }
    {
        // every style, every key, every routing: finite, bounded output on noise
        bool ok = true; juce::String bad;
        juce::Random rnd (3);
        for (int st = 0; st < (int) styles().size(); ++st)
            for (int rt = 0; rt < 5; ++rt)
            {
                Engine e; e.prepare (fs, 512); e.p.style = st; e.p.routing = (Routing) rt; e.p.threshDb = -30; e.p.ratio = 10; e.p.characterMode = 1; e.p.driveDb = 6; e.p.autoGain = true; e.p.autoRelease = st % 2 == 0;
                std::vector<float> l (512), r (512);
                for (int k = 0; k < 400; ++k)
                {
                    for (int i = 0; i < 512; ++i) { l[(size_t) i] = 0.8f * (rnd.nextFloat() * 2 - 1); r[(size_t) i] = 0.8f * (rnd.nextFloat() * 2 - 1); }
                    float* c[2] = { l.data(), r.data() }; e.process (c, 2, 512);
                    for (auto v : l) if (! std::isfinite (v) || std::abs (v) > 8.0f) { ok = false; bad = juce::String (styles()[(size_t) st].name) + " / " + routingNames()[rt]; }
                }
            }
        check (ok, "all 14 styles x 5 routings, auto gain, character on: finite and bounded on loud noise" + (ok ? juce::String() : " - " + bad));
    }
    {
        Engine e; e.prepare (fs, 512); e.p.style = 4; e.p.threshDb = -30; e.p.characterMode = 1; e.p.driveDb = 6; e.p.oversampleLog2 = 2; e.p.lookaheadMs = 5;
        SqSpectra::Band hp; hp.enabled = true; hp.shape = SqSpectra::Shape::lowCut; hp.freq = 100; hp.slope = 12; e.p.keyEq[0] = hp;
        std::vector<float> l (512), r (512); juce::Random rnd (1);
        auto t0 = std::chrono::steady_clock::now(); const int blocks = (int) (fs * 10 / 512);
        for (int k = 0; k < blocks; ++k) { for (int i = 0; i < 512; ++i) { l[(size_t) i] = rnd.nextFloat() - 0.5f; r[(size_t) i] = rnd.nextFloat() - 0.5f; } float* c[2] = { l.data(), r.data() }; e.process (c, 2, 512); }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (sec < 1.5, "Variable Mu + key EQ + 4x character + lookahead: 10 s of stereo in " + juce::String (sec * 1000, 0) + " ms (" + juce::String (sec * 10, 2) + "% of one core)");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
