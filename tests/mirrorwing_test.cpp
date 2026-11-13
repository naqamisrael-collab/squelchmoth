// Mirrorwing DSP core: measured. Build: scripts/build_mirrorwing_test.sh
#include "SqMirrorwingDsp.h"
#include <cstdio>
#include <chrono>

using namespace SqMirror;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }
static const double fs = 48000;

/** One sine on osc 1, filters open, amp env instant and full. */
static Settings clean()
{
    Settings s;
    for (int k = 0; k < numOsc; ++k) s.osc[(size_t) k].on = false;
    s.osc[0].on = true; s.osc[0].shape = 0; s.osc[0].level = 1;
    for (auto& f : s.filter) { f.on = false; f.cutoffHz = 20000; f.res = 0; f.keyTrack = 0; f.slope = 12; }
    s.env[0] = { 0, 0.0005, 0, 0.01, 1.0, 0.05 }; s.ampVel = 0; s.gain = 1;
    return s;
}
struct Out { std::vector<float> l, r; };
static Out render (const Settings& s, double hz, double secs, double releaseAt = -1, int note = 69, Voice* keep = nullptr)
{
    Voice local; Voice& v = keep ? *keep : local; v.prepare (fs); v.start (s, 0.5, true);
    const int N = (int) (secs * fs); Out o { std::vector<float> ((size_t) N), std::vector<float> ((size_t) N) };
    for (int i = 0; i < N; i += 32)
    {
        if (releaseAt >= 0 && i >= (int) (releaseAt * fs) && i < (int) (releaseAt * fs) + 32) v.release();
        v.render (o.l.data() + i, o.r.data() + i, std::min (32, N - i), hz, s, 1.0, note);
    }
    return o;
}
static double freqOf (const std::vector<float>& x, int a, int b)
{
    double fa = 0, fb = 0; int c = 0;
    for (int i = a + 1; i < b; ++i) if (x[(size_t) i - 1] < 0 && x[(size_t) i] >= 0) { const double t = i - 1 + x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]); if (c == 0) fa = t; fb = t; ++c; }
    return c > 1 ? (c - 1) / ((fb - fa) / fs) : 0;
}
static double rms (const std::vector<float>& x, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }
/** Level (dB, relative to the fundamental) of harmonic h of f0. */
static double harmonicDb (const std::vector<float>& x, double f0, int h)
{
    const int order = 15, N = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N; ++i) buf[(size_t) i] = x[(size_t) (4800 + i)] * (float) (0.5 - 0.5 * std::cos (2 * pi * i / (N - 1)));
    fft.performFrequencyOnlyForwardTransform (buf.data());
    auto at = [&] (double f) { double m = 0; const int k = (int) std::lround (f / (fs / N)); for (int j = k - 3; j <= k + 3; ++j) m = std::max (m, (double) buf[(size_t) j]); return m; };
    return db (at (f0 * h) / at (f0));
}

int main()
{
    juce::ScopedNoDenormals nd;
    std::printf ("Mirrorwing DSP\n");

    // 1. Tuning, and each oscillator's own pitch (osc 2 a fifth up, -12 ct fine)
    {
        auto s = clean(); auto o = render (s, 440, 1.5);
        const double f = freqOf (o.l, 4800, (int) o.l.size());
        s.osc[0].on = false; s.osc[1].on = true; s.osc[1].shape = 0; s.osc[1].semis = 7; s.osc[1].cents = -12;
        auto o2 = render (s, 440, 1.5); const double f2 = freqOf (o2.l, 4800, (int) o2.l.size()), want = 440 * std::pow (2.0, (7 - 0.12) / 12.0);
        check (std::abs (1200 * std::log2 (f / 440)) < 0.05 && std::abs (1200 * std::log2 (f2 / want)) < 0.05,
               "Tuning: osc 1 plays " + juce::String (f, 3) + " Hz; osc 2 at +7 st -12 ct plays " + juce::String (f2, 3) + " Hz (" + juce::String (want, 3) + ")");
    }
    // 2. Unison: 7 copies keep the level (within 1.5 dB) and spread into stereo
    {
        auto s = clean(); s.osc[0].shape = 2; auto one = render (s, 220, 1.0);
        s.osc[0].unison = 7; s.osc[0].uniDetune = 20; s.osc[0].uniSpread = 1.0; auto seven = render (s, 220, 1.0);
        const double d = db (std::hypot (rms (seven.l, 4800, 48000), rms (seven.r, 4800, 48000)) / std::hypot (rms (one.l, 4800, 48000), rms (one.r, 4800, 48000)));   // stereo power
        double c = 0, ll = 0, rr = 0; for (int i = 4800; i < 48000; ++i) { c += (double) seven.l[(size_t) i] * seven.r[(size_t) i]; ll += (double) seven.l[(size_t) i] * seven.l[(size_t) i]; rr += (double) seven.r[(size_t) i] * seven.r[(size_t) i]; }
        const double corr = c / std::sqrt (ll * rr);
        check (std::abs (d) < 1.5 && corr < 0.8, "Unison x7, 20 ct, full spread: level " + juce::String (d, 2) + " dB vs one voice, L/R correlation " + juce::String (corr, 2));
    }
    // 3. FM: osc 2 (sine, same pitch) modulates osc 1 (sine) - sidebands appear; without FM there are none
    {
        auto s = clean(); s.osc[1].on = true; s.osc[1].shape = 0; s.osc[1].level = 0.0001; s.osc[1].route = 1; s.filter[1].on = true; s.filter[1].type = SqFissure::lowPass; s.filter[1].cutoffHz = 20; s.filter[1].slope = 48;
        // osc 2 runs nearly silent (its output still drives osc 3 only) - so put the carrier on osc 2 and the modulator on osc 1
        auto f = clean(); f.osc[0].shape = 0; f.osc[0].level = 1; f.osc[0].route = 1;                       // modulator, routed into a closed filter 2
        f.filter[1].on = true; f.filter[1].cutoffHz = 20; f.filter[1].slope = 48; f.parallel = true;
        f.osc[1].on = true; f.osc[1].shape = 0; f.osc[1].route = 0;                                          // carrier, to filter 1 (open)
        f.osc[1].fm = 0; auto plain = render (f, 440, 1.0); f.osc[1].fm = 1.0; auto fm = render (f, 440, 1.0);
        const double h2p = harmonicDb (plain.l, 440, 2), h2f = harmonicDb (fm.l, 440, 2), h3f = harmonicDb (fm.l, 440, 3);
        check (h2p < -80 && h2f > -25 && h3f > -45, "FM 100 %: 2nd harmonic " + juce::String (h2f, 1) + " dB, 3rd " + juce::String (h3f, 1) + " dB (without FM: " + juce::String (h2p, 1) + " dB)");
        (void) s;
    }
    // 4. Hard sync: osc 2 at a ratio of 1.5 synced to osc 1 repeats at osc 1's pitch
    {
        auto f = clean(); f.osc[0].level = 0.0002; f.osc[0].route = 1; f.filter[1].on = true; f.filter[1].cutoffHz = 20; f.filter[1].slope = 48; f.parallel = true;
        f.osc[1].on = true; f.osc[1].shape = 2; f.osc[1].semis = 7.02; f.osc[1].sync = true;
        auto o = render (f, 200, 1.0);
        // period by autocorrelation: the best lag between 2 and 20 ms
        std::vector<double> cs (960, 0.0); double bestC = -1;
        for (int lag = 96; lag < 960; ++lag) { double c = 0; for (int i = 4800; i < 4800 + 9600; ++i) c += (double) o.l[(size_t) i] * o.l[(size_t) (i + lag)]; cs[(size_t) lag] = c; bestC = std::max (bestC, c); }
        int best = 0; for (int lag = 96; lag < 960 && best == 0; ++lag) if (cs[(size_t) lag] >= 0.98 * bestC && cs[(size_t) lag] >= cs[(size_t) lag - 1] && cs[(size_t) lag] >= cs[(size_t) lag + 1]) best = lag;   // the shortest lag that repeats
        check (std::abs (fs / best - 200.0) < 1.0, "Hard sync: osc 2 at +7.02 st synced to osc 1 repeats at " + juce::String (fs / best, 1) + " Hz (osc 1 = 200 Hz)");
    }
    // 5. DAHDSR timing: delay 50 ms silent, attack 20 ms, hold 30 ms, then decay
    {
        auto s = clean(); s.env[0] = { 0.05, 0.02, 0.03, 0.2, 0.5, 0.1 };
        Voice v; v.prepare (fs); v.start (s, 0.1, true); std::vector<float> l (1), r (1);
        int firstSound = -1, peakAt = -1, holdEnd = -1;
        for (int i = 0; i < (int) fs / 2; ++i)
        {
            v.render (l.data(), r.data(), 1, 440, s, 1, 69);
            const double lv = v.env[0].level;
            if (firstSound < 0 && lv > 0) firstSound = i;
            if (peakAt < 0 && lv >= 1.0) peakAt = i;
            if (peakAt >= 0 && holdEnd < 0 && lv < 1.0) holdEnd = i;
        }
        check (std::abs (firstSound - 2400) <= 2 && std::abs (peakAt - 3360) <= 2 && std::abs (holdEnd - 4800) <= 3,
               "DAHDSR: sound starts at " + juce::String (firstSound / 48.0, 2) + " ms, full at " + juce::String (peakAt / 48.0, 2) + " ms, decay from " + juce::String (holdEnd / 48.0, 2) + " ms (50 / 70 / 100)");
    }
    // 6. Matrix: LFO 1 -> filter 1 cutoff, 0.5 = +-3 octaves around 1 kHz; Env 2 -> pitch +12 st doubles the frequency
    {
        auto s = clean(); s.filter[0].on = true; s.filter[0].cutoffHz = 1000; s.lfo[0].hz = 4; s.slot[0] = { sLfo1, tFilBase + tCut, 0.5 };
        Voice v; v.prepare (fs); v.start (s, 0.2, true); std::vector<float> l (32), r (32); float lo = 1e9f, hi = 0;
        for (int i = 0; i < (int) fs; i += 32) { v.render (l.data(), r.data(), 32, 220, s, 1, 60); lo = std::min (lo, v.cutNow[0]); hi = std::max (hi, v.cutNow[0]); }
        check (std::abs (lo - 125.0f) < 4 && std::abs (hi - 8000.0f) < 250, "Matrix LFO 1 -> Filter 1 Cutoff at 0.50 (+-3 oct): " + juce::String (juce::roundToInt (lo)) + " .. " + juce::String (juce::roundToInt (hi)) + " Hz");
        auto p = clean(); p.env[1] = { 0, 0.0005, 0, 0.01, 1.0, 0.1 }; p.slot[0] = { sEnv2, tGlobalPitch, 0.5 };
        auto o = render (p, 440, 1.0); const double f = freqOf (o.l, 9600, 48000);
        check (std::abs (1200 * std::log2 (f / 880.0)) < 0.5, "Matrix Env 2 -> Pitch at 0.50 (+12 st): A4 plays " + juce::String (f, 2) + " Hz");
    }
    // 7. Filter routing: osc 1 to filter 1 (low-pass 500 Hz), osc 2 to filter 2 (high-pass 2 kHz). Serial: 1 kHz from osc 1 vanishes; parallel keeps both paths
    {
        auto s = clean(); s.filter[0] = { true, SqFissure::lowPass, 24, 500, 0, 0, 0, 0 }; s.filter[1] = { true, SqFissure::highPass, 24, 2000, 0, 0, 0, 0 };
        s.osc[0].route = 0; s.parallel = false; auto ser = render (s, 1000, 0.6);
        auto open = clean(); auto ref = render (open, 1000, 0.6);
        const double serDb = db (rms (ser.l, 9600, 28800) / rms (ref.l, 9600, 28800));
        s.osc[0].route = 1; auto toF2 = render (s, 4000, 0.6); auto ref4 = render (open, 4000, 0.6);   // routed past filter 1 into filter 2 only
        const double hpDb = db (rms (toF2.l, 9600, 28800) / rms (ref4.l, 9600, 28800));
        check (serDb < -30 && std::abs (hpDb) < 1.0, "Routing: osc 1 at 1 kHz through LP 500 -> HP 2k (serial) " + juce::String (serDb, 1) + " dB; osc routed to filter 2 only, at 4 kHz: " + juce::String (hpDb, 2) + " dB");
    }
    // 8. Wavefold adds harmonics, never exceeds full scale
    {
        auto s = clean(); s.osc[0].fold = 0.8; auto o = render (s, 220, 1.0);
        double pk = 0; for (size_t i = 4800; i < o.l.size(); ++i) pk = std::max (pk, (double) std::abs (o.l[i]));
        const double h3 = harmonicDb (o.l, 220, 3);
        check (h3 > -20 && pk <= 1.0001, "Wavefold 80 % on a sine: 3rd harmonic " + juce::String (h3, 1) + " dB, peak " + juce::String (db (pk), 2) + " dBFS");
    }
    // 9. Silence after release; CPU at a heavy patch
    {
        auto s = clean(); auto o = render (s, 440, 0.6, 0.1); double tail = 0; for (size_t i = (size_t) (0.4 * fs); i < o.l.size(); ++i) tail = std::max (tail, (double) std::abs (o.l[i]));
        check (tail == 0.0, "Release 50 ms: exact silence 300 ms later");
        Settings h; for (int k = 0; k < numOsc; ++k) { h.osc[(size_t) k].unison = 2; h.osc[(size_t) k].shape = 1.0 + k * 0.6; h.osc[(size_t) k].fm = k ? 0.3 : 0; h.osc[(size_t) k].route = k % 2; }
        h.filter[0].slope = 48; h.filter[0].res = 0.6; h.filter[1].type = SqFissure::bandPass; h.parallel = true; h.noise = 0.1;
        for (int q = 0; q < 8; ++q) h.slot[(size_t) q] = { sLfo1 + q % 4, tOscBase + q * 3 % 28, 0.2 };
        std::vector<Voice> vs (16); for (int k = 0; k < 16; ++k) { vs[(size_t) k].prepare (fs); vs[(size_t) k].start (h, k * 0.1, true); }
        std::vector<float> l (32), r (32); const int N = (int) fs;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; i += 32) for (int k = 0; k < 16; ++k) vs[(size_t) k].render (l.data(), r.data(), 32, 110.0 * (1 + k * 0.2), h, 1, 48 + k);
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (secs < 0.6, "16 voices x 4 oscillators x unison 2, two filters, 8 matrix slots: " + juce::String (secs * 100.0, 1) + " % of one core");
    }
    std::printf ("Mirrorwing: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
