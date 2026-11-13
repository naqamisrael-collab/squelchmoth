// Sibyl De-Ess DSP core: measured. Build: scripts/build_sibyl_test.sh
#include "SqSibylDsp.h"
#include <cstdio>
#include <chrono>
#include <functional>

using namespace SqSibyl;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }
static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }

static void run (Engine& e, std::vector<float>& l, std::vector<float>& r, const std::vector<float>* key = nullptr)
{
    for (int s = 0; s < (int) l.size(); s += 256)
    {
        const int n = std::min (256, (int) l.size() - s);
        float* ch[2] = { l.data() + s, r.data() + s };
        if (key) { const float* k[1] = { key->data() + s }; e.process (ch, 2, k, 1, n); } else e.process (ch, 2, nullptr, 0, n);
    }
}

/** A voice stand-in: a 200 Hz tone (the body) plus band-limited noise centred on fc (the "s"), each at its own level. */
static void voice (std::vector<float>& l, double fs, double sec, double bodyDb, double essDb, double fc, int seed = 1)
{
    juce::Random rnd (seed); Svf a, b; a.set (fs, fc, 3); b.set (fs, fc, 3);
    const int N = (int) (fs * sec); l.assign ((size_t) N, 0);
    std::vector<double> ess ((size_t) N); double e2 = 0;
    for (int i = 0; i < N; ++i) { ess[(size_t) i] = b.bandpass (a.bandpass (rnd.nextDouble() * 2 - 1)); e2 += ess[(size_t) i] * ess[(size_t) i]; }
    const double essScale = dbToGain (essDb) / std::sqrt (e2 / N);
    for (int i = 0; i < N; ++i) l[(size_t) i] = (float) (dbToGain (bodyDb) * std::sqrt (2.0) * std::sin (2 * pi * 200 * i / fs) + essScale * ess[(size_t) i]);
}

/** Level (dB RMS) of v in a band (a 4th-order band-pass at fc). */
static double bandLevel (const std::vector<float>& v, double fs, double fc, int from)
{
    Svf a, b; a.set (fs, fc, 2); b.set (fs, fc, 2); double s = 0; int n = 0;
    for (int i = 0; i < (int) v.size(); ++i) { const double y = b.bandpass (a.bandpass (v[(size_t) i])); if (i >= from) { s += y * y; ++n; } }
    return db (std::sqrt (s / std::max (1, n)));
}

int main()
{
    juce::ScopedNoDenormals nd;
    const double fs = 48000;
    std::printf ("Sibyl De-Ess DSP\n");

    // 1. Idle = bit-exact
    {
        std::vector<float> l, r; voice (l, fs, 1.0, -20, -50, 7000); r = l; const auto in = l;
        Params p; p.thresholdDb = 0; p.detect = absolute; p.lookaheadMs = 0; Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
        bool same = true; for (size_t i = 0; i < l.size(); ++i) if (l[i] != in[i]) same = false;
        check (same, "Below the threshold the output is the input, bit for bit");
    }
    // 2. Split: the s comes down by the range, the body is untouched
    {
        std::vector<float> l, r; voice (l, fs, 2.0, -14, -14, 7000); r = l; const auto in = l;
        Params p; p.detect = absolute; p.thresholdDb = -40; p.rangeDb = 10; p.kneeDb = 0; p.lookaheadMs = 0; p.lowHz = 5000; p.highHz = 10000;
        Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
        const double dEss = bandLevel (l, fs, 7000, (int) fs) - bandLevel (in, fs, 7000, (int) fs), dBody = bandLevel (l, fs, 200, (int) fs) - bandLevel (in, fs, 200, (int) fs);
        check (dEss < -8.5 && dEss > -11, "Split: the 7 kHz \"s\" drops " + juce::String (dEss, 2) + " dB (range 10)");
        check (std::abs (dBody) < 0.05, "Split: the 200 Hz body moves " + juce::String (dBody, 3) + " dB");
        p.shape = wide; Engine w; w.p = p; w.prepare (fs, 256); auto l2 = in, r2 = in; run (w, l2, r2);
        const double wBody = bandLevel (l2, fs, 200, (int) fs) - bandLevel (in, fs, 200, (int) fs);
        check (wBody < -8.5, "Wide: the whole voice comes down (body " + juce::String (wBody, 2) + " dB)");
    }
    // 3. Relative detection: the same voice at -40 dB and at -10 dB is de-essed the same
    {
        double gr[2];
        for (int k = 0; k < 2; ++k)
        {
            const double lift = k ? 0 : -30;
            std::vector<float> l, r; voice (l, fs, 2.0, -10 + lift, -14 + lift, 7000); r = l; const auto in = l;
            Params p; p.detect = relative; p.thresholdDb = -12; p.rangeDb = 20; p.kneeDb = 0; p.lookaheadMs = 0;
            Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
            gr[k] = bandLevel (l, fs, 7000, (int) fs) - bandLevel (in, fs, 7000, (int) fs);
        }
        check (std::abs (gr[0] - gr[1]) < 0.5 && gr[1] < -3, "Relative: -40 dB voice " + juce::String (gr[0], 2) + " dB, -10 dB voice " + juce::String (gr[1], 2) + " dB (level-independent)");
        double ab[2];
        for (int k = 0; k < 2; ++k)
        {
            const double lift = k ? 0 : -30;
            std::vector<float> l, r; voice (l, fs, 2.0, -10 + lift, -14 + lift, 7000); r = l; const auto in = l;
            Params p; p.detect = absolute; p.thresholdDb = -30; p.rangeDb = 20; p.kneeDb = 0; p.lookaheadMs = 0;
            Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
            ab[k] = bandLevel (l, fs, 7000, (int) fs) - bandLevel (in, fs, 7000, (int) fs);
        }
        check (std::abs (ab[0]) < 0.1 && ab[1] < -8, "Absolute, for contrast: quiet voice " + juce::String (ab[0], 2) + " dB, loud " + juce::String (ab[1], 2) + " dB");
    }
    // 4. Track: the band finds the s
    {
        bool ok = true; juce::String msg;
        for (double fc : { 5500.0, 7000.0, 8500.0 })
        {
            std::vector<float> l, r; voice (l, fs, 1.5, -20, -16, fc); r = l;
            Params p; p.track = true; p.lowHz = 4000; p.highHz = 11000; p.detect = absolute; p.thresholdDb = -30; p.lookaheadMs = 0;
            Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
            const double t = e.meterTrackHz; if (std::abs (t / fc - 1) > 0.10) ok = false;
            msg << juce::String ((int) fc) << " -> " << juce::String ((int) t) << " Hz; ";
        }
        check (ok, "Track follows the sibilance within 10 % (" + msg.trimEnd() + ")");
    }
    // 5. Lookahead catches the onset
    {
        auto burst = [&] { std::vector<float> v; voice (v, fs, 1.0, -60, -6, 7000); for (int i = 0; i < (int) v.size(); ++i) if (i < (int) (0.4 * fs) || i > (int) (0.5 * fs)) v[(size_t) i] = 0; return v; };
        double first[2];
        for (int k = 0; k < 2; ++k)
        {
            auto l = burst(), r = l; Params p; p.detect = absolute; p.thresholdDb = -40; p.rangeDb = 20; p.kneeDb = 0; p.lookaheadMs = k ? 3 : 0; p.shape = wide;
            Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
            const int L = e.latency(), at = (int) (0.4 * fs) + L; double pk = 0; for (int i = at; i < at + (int) (0.002 * fs); ++i) pk = std::max (pk, (double) std::abs (l[(size_t) i])); first[k] = db (pk);
        }
        check (first[1] < first[0] - 10, "Lookahead 3 ms: the first 2 ms of an \"s\" peak at " + juce::String (first[1], 1) + " dB instead of " + juce::String (first[0], 1) + " dB");
    }
    // 6. Listen modes: the removed part plus the output is the input
    {
        std::vector<float> l, r; voice (l, fs, 1.0, -14, -14, 7000); r = l; const auto in = l;
        Params p; p.detect = absolute; p.thresholdDb = -40; p.rangeDb = 12; p.lookaheadMs = 0;
        Engine a; a.p = p; a.prepare (fs, 256); auto o = in, o2 = in; run (a, o, o2);
        p.listen = removed; Engine b; b.p = p; b.prepare (fs, 256); auto rm = in, rm2 = in; run (b, rm, rm2);
        double err = 0, ref = 0; for (size_t i = 0; i < in.size(); ++i) { const double d = (double) o[i] + rm[i] - in[i]; err += d * d; ref += (double) in[i] * in[i]; }
        check (10 * std::log10 (err / ref + 1e-30) < -100, "Removed + output = input (null " + juce::String (10 * std::log10 (err / ref + 1e-30), 1) + " dB)");
        p.listen = detection; Engine c; c.p = p; c.prepare (fs, 256); auto dt = in, dt2 = in; run (c, dt, dt2);
        const double body = bandLevel (dt, fs, 200, (int) (0.5 * fs)) - bandLevel (in, fs, 200, (int) (0.5 * fs));
        check (body < -60, "Listen to detection: the body is " + juce::String (body, 1) + " dB down, only the band is heard");
    }
    // 7. Side only
    {
        std::vector<float> m, s; voice (m, fs, 2.0, -14, -14, 7000, 1); voice (s, fs, 2.0, -60, -14, 7000, 2);
        std::vector<float> l (m.size()), r (m.size()); for (size_t i = 0; i < m.size(); ++i) { l[i] = m[i] + s[i]; r[i] = m[i] - s[i]; }
        Params p; p.stereo = sideOnly; p.detect = absolute; p.thresholdDb = -40; p.rangeDb = 10; p.kneeDb = 0; p.lookaheadMs = 0;
        Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r);
        std::vector<float> om (m.size()), os (m.size()); for (size_t i = 0; i < m.size(); ++i) { om[i] = 0.5f * (l[i] + r[i]); os[i] = 0.5f * (l[i] - r[i]); }
        const double dm = bandLevel (om, fs, 7000, (int) fs) - bandLevel (m, fs, 7000, (int) fs), ds = bandLevel (os, fs, 7000, (int) fs) - bandLevel (s, fs, 7000, (int) fs);
        check (std::abs (dm) < 0.05 && ds < -8, "Side only: the centre's s moves " + juce::String (dm, 3) + " dB, the sides' s " + juce::String (ds, 2) + " dB");
    }
    // 8. External key
    {
        std::vector<float> l, r; voice (l, fs, 2.0, -14, -40, 7000); r = l; const auto in = l; std::vector<float> key; voice (key, fs, 2.0, -60, -6, 7000, 5);
        Params p; p.sidechain = true; p.detect = absolute; p.thresholdDb = -30; p.rangeDb = 10; p.kneeDb = 0; p.lookaheadMs = 0;
        Engine e; e.p = p; e.prepare (fs, 256); run (e, l, r, &key);
        const double d = bandLevel (l, fs, 7000, (int) fs) - bandLevel (in, fs, 7000, (int) fs);
        check (d < -8.5, "Side-chain key: a quiet track's s band follows the key (" + juce::String (d, 2) + " dB)");
    }
    // 9. Stability and CPU
    {
        std::vector<float> l ((size_t) fs * 20), r; juce::Random rnd (2); for (auto& v : l) v = (float) (rnd.nextDouble() * 2 - 1); r = l;
        Params p; p.track = true; p.lookaheadMs = 15; p.rangeDb = 40; p.thresholdDb = -60; p.kneeDb = 0; p.releaseMs = 5;
        Engine e; e.p = p; e.prepare (fs, 256);
        const auto t0 = std::chrono::steady_clock::now(); run (e, l, r);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool fin = true; for (auto v : l) if (! std::isfinite (v)) fin = false;
        check (fin, "Extreme settings on full-scale noise: finite");
        check (sec / 20 < 0.02, "CPU (track + 15 ms lookahead): " + juce::String (sec / 20 * 100, 2) + " % of one core");
    }
    std::printf ("== %d passed, %d failed ==\n", passed, failed);
    return failed ? 1 : 0;
}
