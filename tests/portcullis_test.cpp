// Portcullis Gate DSP core: measured. Build: scripts/build_portcullis_test.sh
#include "SqPortcullisDsp.h"
#include <cstdio>
#include <chrono>
#include <functional>

using namespace SqPortcullis;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }
static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }

static void run (Engine& e, std::vector<float>& l, std::vector<float>& r, const std::vector<float>* key = nullptr, std::function<void (Engine&, int)> per = {})
{
    for (int s = 0; s < (int) l.size(); s += 256)
    {
        const int n = std::min (256, (int) l.size() - s);
        if (per) per (e, s);
        float* ch[2] = { l.data() + s, r.data() + s };
        if (key) { const float* k[1] = { key->data() + s }; e.process (ch, 2, k, 1, n); } else e.process (ch, 2, nullptr, 0, n);
    }
}
static std::vector<float> tone (double fs, double sec, double f, double dbPeak, double from = 0, double to = 1e9)
{
    std::vector<float> v ((size_t) (fs * sec)); const double a = dbToGain (dbPeak);
    for (size_t i = 0; i < v.size(); ++i) { const double t = i / fs; v[i] = (t >= from && t < to) ? (float) (a * std::sin (2 * pi * f * t)) : 0.0f; }
    return v;
}

int main()
{
    juce::ScopedNoDenormals nd;
    const double fs = 48000;
    std::printf ("Portcullis Gate DSP\n");

    // 1. Open: the signal passes bit-exactly; closed: exactly the range down
    {
        auto loud = tone (fs, 1.0, 440, -6), lr = loud; const auto in = loud;
        Params p; p.thresholdDb = -30; Engine e; e.p = p; e.prepare (fs, 256); run (e, loud, lr);
        bool same = true; for (int i = (int) (0.1 * fs); i < (int) loud.size(); ++i) if (loud[(size_t) i] != in[(size_t) i]) same = false;
        check (same, "Above the threshold the gate is open and bit-exact");
        auto quiet = tone (fs, 1.0, 440, -50), qr = quiet; const auto qin = quiet;
        p.rangeDb = 40; Engine e2; e2.p = p; e2.prepare (fs, 256); run (e2, quiet, qr);
        const double d = db (rms (quiet, (int) (0.5 * fs), (int) fs) / rms (qin, (int) (0.5 * fs), (int) fs));
        check (std::abs (d + 40.0) < 0.05, "Below the threshold it closes by exactly the range: " + juce::String (d, 2) + " dB (range 40)");
    }
    // 2. Hysteresis: a note decaying slowly through the threshold closes once, never chatters
    {
        std::vector<float> l ((size_t) (fs * 3)); for (size_t i = 0; i < l.size(); ++i) { const double t = i / fs; l[i] = (float) (dbToGain (-20 - 12 * t) * std::sin (2 * pi * 300 * t) * (1 + 0.15 * std::sin (2 * pi * 7 * t))); }
        auto r = l;
        int changes[2];
        for (int k = 0; k < 2; ++k)
        {
            auto a = l, b = r; Params p; p.thresholdDb = -38; p.hysteresisDb = k ? 6 : 0; p.holdMs = 0; p.attackMs = 0.1; p.releaseMs = 1;
            Engine e; e.p = p; e.prepare (fs, 256); bool was = false; changes[k] = 0;
            run (e, a, b, nullptr, [&] (Engine& en, int) { const bool now = en.meterOpen > 0.5f; if (was && ! now) ++changes[k]; was = now; });   // count closings
        }
        check (changes[1] <= 1 && changes[0] > changes[1], "Hysteresis 6 dB: a wobbling decay closes " + juce::String (changes[1]) + " time(s); without it it chatters " + juce::String (changes[0]) + " times");
    }
    // 3. Attack, hold, release timings
    {
        auto l = tone (fs, 1.0, 1000, -10, 0.2, 0.4), r = l;
        Params p; p.thresholdDb = -30; p.rangeDb = 80; p.attackMs = 5; p.holdMs = 50; p.releaseMs = 100; p.curve = linear;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> open, keyDb; run (e, l, r, nullptr, [&] (Engine& en, int) { open.push_back (en.meterOpen); keyDb.push_back (en.meterKeyDb); });
        // hold counts from when the key falls below the close threshold (the detector's own release comes first)
        int startClose = -1, shut = -1, blk0 = (int) (0.4 * fs / 256);
        while (blk0 < (int) keyDb.size() && keyDb[(size_t) blk0] > p.thresholdDb - p.hysteresisDb) ++blk0;
        for (int b = blk0; b < (int) open.size(); ++b) { if (startClose < 0 && open[(size_t) b] < 0.999f) startClose = b; if (shut < 0 && open[(size_t) b] <= 0.0f) { shut = b; break; } }
        const double holdMeas = (startClose - blk0) * 256 / fs * 1000, relMeas = (shut - startClose) * 256 / fs * 1000;
        check (std::abs (holdMeas - 50) < 16 && std::abs (relMeas - 100) < 16, "Hold " + juce::String (holdMeas, 0) + " ms (50), release " + juce::String (relMeas, 0) + " ms (100), measured to the block");
    }
    // 4. Lookahead: the transient is not clipped by the opening gate
    {
        double first[2];
        for (int k = 0; k < 2; ++k)
        {
            auto l = tone (fs, 0.5, 2000, -6, 0.2, 0.3), r = l; Params p; p.thresholdDb = -30; p.rangeDb = 80; p.attackMs = 2; p.lookaheadMs = k ? 3 : 0;
            Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
            const int at = (int) (0.2 * fs) + e.latency(); double pk = 0; for (int i = at; i < at + (int) (0.0015 * fs); ++i) pk = std::max (pk, (double) std::abs (l[(size_t) i])); first[k] = db (pk);
        }
        check (first[1] > -7.0 && first[0] < first[1] - 3, "Lookahead 3 ms: the first 1.5 ms of a hit come through at " + juce::String (first[1], 1) + " dB (without: " + juce::String (first[0], 1) + " dB)");
    }
    // 5. Key filter: a bass bleed below the threshold's band does not open a high-passed key
    {
        auto l = tone (fs, 1.0, 60, -10), r = l; const auto in = l;
        Params p; p.thresholdDb = -30; p.rangeDb = 40; p.keyHpHz = 500; Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
        const double d = db (rms (l, (int) (0.5 * fs), (int) fs) / rms (in, (int) (0.5 * fs), (int) fs));
        check (d < -38, "Key high-pass at 500 Hz: a loud 60 Hz bleed keeps the gate shut (" + juce::String (d, 1) + " dB)");
    }
    // 6. Expand mode follows its ratio
    {
        auto l = tone (fs, 1.0, 440, -40), r = l; const auto in = l;
        Params p; p.mode = expand; p.thresholdDb = -30; p.ratio = 2; p.rangeDb = 60; Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
        const double d = db (rms (l, (int) (0.5 * fs), (int) fs) / rms (in, (int) (0.5 * fs), (int) fs));
        check (std::abs (d + 10.0) < 0.6, "Expand 1:2, 10 dB below the threshold: " + juce::String (d, 2) + " dB (curve -10)");
    }
    // 7. Duck: the side-chain pulls the signal down
    {
        auto l = tone (fs, 1.0, 440, -12), r = l; const auto in = l; auto key = tone (fs, 1.0, 80, -6, 0.3, 0.7);
        Params p; p.mode = duck; p.key = sidechain; p.thresholdDb = -30; p.rangeDb = 12; p.attackMs = 1; p.releaseMs = 50;
        Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r, &key);
        const double during = db (rms (l, (int) (0.45 * fs), (int) (0.65 * fs)) / rms (in, (int) (0.45 * fs), (int) (0.65 * fs))), before = db (rms (l, (int) (0.1 * fs), (int) (0.25 * fs)) / rms (in, (int) (0.1 * fs), (int) (0.25 * fs)));
        check (std::abs (during + 12) < 0.3 && std::abs (before) < 0.01, "Duck 12 dB under a side-chain hit: " + juce::String (during, 2) + " dB during, " + juce::String (before, 2) + " dB before");
    }
    // 8. MIDI key with velocity
    {
        auto l = tone (fs, 1.0, 440, -12), r = l; const auto in = l;
        Params p; p.key = midi; p.velocity = true; p.rangeDb = 80; p.attackMs = 0.5; p.releaseMs = 5; p.holdMs = 0;
        Engine e; e.p = p; e.prepare (fs, 256);
        run (e, l, r, nullptr, [&] (Engine& en, int s) { if (s == (int) (0.2 * fs) / 256 * 256) en.noteOn (0, 0.5f); if (s == (int) (0.6 * fs) / 256 * 256) en.noteOff (0); });
        const double on = db (rms (l, (int) (0.3 * fs), (int) (0.5 * fs)) / rms (in, (int) (0.3 * fs), (int) (0.5 * fs))), off = db (rms (l, (int) (0.7 * fs), (int) (0.9 * fs)) / rms (in, (int) (0.7 * fs), (int) (0.9 * fs)));
        check (std::abs (on - db (0.5)) < 0.2 && off < -70, "MIDI key: a velocity-64 note opens it to " + juce::String (on, 2) + " dB, note-off shuts it (" + juce::String (off, 1) + " dB)");
    }
    // 9. Flip: gated + flipped = input
    {
        auto src = tone (fs, 1.0, 440, -20, 0.3, 0.6); for (size_t i = 0; i < src.size(); ++i) src[i] += (float) (0.003 * std::sin (2 * pi * 3000 * i / fs));
        auto a = src, a2 = src, b = src, b2 = src;
        Params p; p.thresholdDb = -30; p.rangeDb = 30; Engine e1; e1.p = p; e1.prepare (fs, 256); run (e1, a, a2);
        p.flip = true; Engine e2; e2.p = p; e2.prepare (fs, 256); run (e2, b, b2);
        double err = 0, ref = 0; for (size_t i = 0; i < src.size(); ++i) { const double d = (double) a[i] + b[i] - src[i]; err += d * d; ref += (double) src[i] * src[i]; }
        check (10 * std::log10 (err / ref + 1e-30) < -120, "Flip: what the gate keeps + what it removes = the input (null " + juce::String (10 * std::log10 (err / ref + 1e-30), 1) + " dB)");
    }
    // 10. CPU and stability
    {
        std::vector<float> l ((size_t) fs * 20), r; juce::Random rnd (2); for (auto& v : l) v = (float) (rnd.nextDouble() * 2 - 1); r = l;
        Params p; p.lookaheadMs = 10; p.keyHpHz = 200; p.keyLpHz = 8000; p.attackMs = 0.01; p.releaseMs = 1;
        Engine e; e.p = p; e.prepare (fs, 256);
        const auto t0 = std::chrono::steady_clock::now(); run (e, l, r);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool fin = true; for (auto v : l) if (! std::isfinite (v)) fin = false;
        check (fin && sec / 20 < 0.01, "CPU (key filters + 10 ms lookahead): " + juce::String (sec / 20 * 100, 2) + " % of one core, finite");
    }
    std::printf ("== %d passed, %d failed ==\n", passed, failed);
    return failed ? 1 : 0;
}
