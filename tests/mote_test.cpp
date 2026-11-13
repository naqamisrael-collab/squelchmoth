// Mote DSP core: measured. Build: scripts/build_mote_test.sh
#include "SqMoteDsp.h"
#include <cstdio>
#include <chrono>

using namespace SqMote;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }
static const double fs = 48000;

/** Renders one voice for `secs` seconds. */
static std::vector<float> renderVoice (Settings s, double hz, double secs, double releaseAt = -1, int note = 60, Mods m = {}, double vel = 1.0)
{
    Voice v; v.prepare (fs); v.start (true);
    const int N = (int) (secs * fs); std::vector<float> l ((size_t) N, 0.0f), r ((size_t) N, 0.0f);
    for (int i = 0; i < N; i += 32)
    {
        if (releaseAt >= 0 && i >= (int) (releaseAt * fs) && i < (int) (releaseAt * fs) + 32) v.release();
        v.render (l.data() + i, r.data() + i, std::min (32, N - i), hz, s, m, 0.0, vel, note, 1.0, 1.0);
    }
    return l;
}
static Settings openFilter()
{
    Settings s; s.cutoffHz = 20000; s.res = 0; s.envOct = 0; s.keyTrack = 0; s.slope = 12; s.filterType = 0;
    s.aEnv[0] = 0.0005; s.aEnv[1] = 0.01; s.aEnv[2] = 1.0; s.aEnv[3] = 0.05; s.ampVel = 0; s.gain = 1;
    return s;
}
/** Magnitude spectrum (Blackman-Harris) of x[a .. a + 2^order). */
static std::vector<double> spectrum (const std::vector<float>& x, int a, int order)
{
    const int N = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const double w = 0.35875 - 0.48829 * std::cos (2 * pi * i / (N - 1)) + 0.14128 * std::cos (4 * pi * i / (N - 1)) - 0.01168 * std::cos (6 * pi * i / (N - 1));
        buf[(size_t) i] = (float) (x[(size_t) (a + i)] * w);
    }
    fft.performFrequencyOnlyForwardTransform (buf.data());
    return std::vector<double> (buf.begin(), buf.begin() + N / 2);
}
/** Worst alias (dB relative to the fundamental): the biggest bin that is not near a harmonic of f0. */
static double worstAlias (const std::vector<float>& x, double f0)
{
    const int order = 15, N = 1 << order; auto sp = spectrum (x, (int) (0.2 * fs), order);
    const double binHz = fs / N; double fund = 0, worst = 0;
    for (int k = 2; k < N / 2; ++k)
    {
        const double f = k * binHz, h = f / f0, dist = std::abs (h - std::round (h)) * f0;
        if (std::abs (f - f0) < 6 * binHz) fund = std::max (fund, sp[(size_t) k]);
        else if (dist > 8 * binHz && f < 20000) worst = std::max (worst, sp[(size_t) k]);
    }
    return db (worst / fund);
}
/** Naive (no band-limiting) reference waves, for comparison. */
static std::vector<float> naive (int wave, double f0, double secs)
{
    std::vector<float> x ((size_t) (secs * fs)); double ph = 0;
    for (auto& v : x) { v = (float) (wave == 2 ? 2 * ph - 1 : 4 * std::abs (ph - 0.5) - 1); ph += f0 / fs; if (ph >= 1) ph -= 1; }
    return x;
}
static double freqByCrossings (const std::vector<float>& x, int a, int b)
{
    int first = -1, last = -1, count = 0; double fa = 0, fb = 0;
    for (int i = a + 1; i < b; ++i)
        if (x[(size_t) i - 1] < 0 && x[(size_t) i] >= 0)
        {
            const double frac = x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]);
            const double t = i - 1 + frac;
            if (first < 0) { first = i; fa = t; } last = i; fb = t; ++count;
        }
    return (count - 1) / ((fb - fa) / fs);
}

int main()
{
    juce::ScopedNoDenormals nd;
    std::printf ("Mote DSP\n");

    // 1. Tuning: A4 = 440 Hz on the nose
    {
        auto s = openFilter(); s.shape = 0;
        auto x = renderVoice (s, 440.0, 2.0);
        const double f = freqByCrossings (x, (int) (0.1 * fs), (int) (2.0 * fs) - 10), cents = 1200 * std::log2 (f / 440.0);
        check (std::abs (cents) < 0.05, "Tuning: A4 plays " + juce::String (f, 4) + " Hz (" + juce::String (cents, 3) + " cents)");
    }
    // 2. Band-limiting: a saw and a triangle at the top of the keyboard against naive waves
    {
        auto s = openFilter(); const double f0 = 4186.0;   // C8
        s.shape = 2; const double aSaw = worstAlias (renderVoice (s, f0, 1.0), f0), nSaw = worstAlias (naive (2, f0, 1.0), f0);
        s.shape = 1; const double aTri = worstAlias (renderVoice (s, 2093.0, 1.0), 2093.0), nTri = worstAlias (naive (1, 2093.0, 1.0), 2093.0);
        check (aSaw < nSaw - 12 && aSaw < -30, "Saw at C8 (4186 Hz): worst alias " + juce::String (aSaw, 1) + " dB (naive saw " + juce::String (nSaw, 1) + " dB)");
        check (aTri < nTri - 12 && aTri < -55, "Triangle at C7 (2093 Hz): worst alias " + juce::String (aTri, 1) + " dB (naive triangle " + juce::String (nTri, 1) + " dB)");
    }
    // 3. The shape sweeps sine -> triangle -> saw -> square without a step; a narrow pulse carries no DC
    {
        auto s = openFilter(); Voice v; v.prepare (fs); v.start (true);
        const int N = (int) fs; std::vector<float> l ((size_t) N, 0.0f), r ((size_t) N, 0.0f);
        for (int i = 0; i < N; i += 32) { s.shape = 3.0 * i / N; v.render (l.data() + i, r.data() + i, 32, 110.0, s, {}, 0, 1, 60, 1, 1); }
        // the biggest jump between samples in the sweep, against the same jump on a steady square (whose edges are the largest steps)
        double maxD = 0; for (int i = (int) (0.01 * fs); i < N - 1; ++i) { const double d = std::abs (l[(size_t) i + 1] - l[(size_t) i]); maxD = std::max (maxD, d); }
        s.shape = 3; auto sq = renderVoice (s, 110.0, 0.3); double sqD = 0; for (int i = 4800; i < (int) sq.size() - 1; ++i) sqD = std::max (sqD, (double) std::abs (sq[(size_t) i + 1] - sq[(size_t) i]));
        s.width = 0.1; auto pl = renderVoice (s, 220.0, 1.0); double mean = 0; for (int i = 4800; i < 4800 + 43200; ++i) mean += pl[(size_t) i]; mean /= 43200;
        check (maxD <= sqD * 1.02, "Shape sweep 0 -> 3 over one second: largest step " + juce::String (maxD, 3) + " (a square's own edge: " + juce::String (sqD, 3) + ")");
        check (std::abs (mean) < 1e-3, "Pulse at 10 % width: DC " + juce::String (mean, 5));
    }
    // 4. Envelope times are what they say
    {
        auto s = openFilter(); s.shape = 0; s.aEnv[0] = 0.1; s.aEnv[1] = 0.05; s.aEnv[2] = 1.0; s.aEnv[3] = 0.5;
        Voice v; v.prepare (fs); v.start (true); std::vector<float> l (64), r (64);
        int peakAt = -1, i = 0;
        for (; i < (int) fs && peakAt < 0; ++i) { l[0] = r[0] = 0; v.render (l.data(), r.data(), 1, 440, s, {}, 0, 1, 60, 1, 1); if (v.amp.level >= 1.0) peakAt = i + 1; }
        v.release(); int downAt = -1; const double from = v.amp.level;
        for (int k = 0; k < (int) (2 * fs) && downAt < 0; ++k) { v.render (l.data(), r.data(), 1, 440, s, {}, 0, 1, 60, 1, 1); if (v.amp.level <= from * 0.01) downAt = k + 1; }
        check (std::abs (peakAt - 4800) <= 1, "Attack 100 ms: full level after " + juce::String (peakAt / fs * 1000.0, 2) + " ms");
        check (std::abs (downAt - 24000) <= 2, "Release 500 ms: down 40 dB after " + juce::String (downAt / fs * 1000.0, 2) + " ms");
    }
    // 5. Filter: the cutoff is where it says (-3 dB, 12 dB/oct, no resonance), key tracking moves it an octave per octave
    {
        auto s = openFilter(); s.shape = 0; s.slope = 12; s.cutoffHz = 1000;
        auto os = openFilter(); os.shape = 0; auto open = renderVoice (os, 1000.0, 0.5); auto x = renderVoice (s, 1000.0, 0.5);
        auto rmsOf = [] (const std::vector<float>& v) { double a = 0; for (size_t i = 4800; i < v.size(); ++i) a += (double) v[i] * v[i]; return std::sqrt (a / (v.size() - 4800)); };
        const double g = db (rmsOf (x) / rmsOf (open));
        Voice v; v.prepare (fs); v.start (true); std::vector<float> l (32), r (32); s.keyTrack = 1.0;
        v.render (l.data(), r.data(), 32, 523.25, s, {}, 0, 1, 72, 1, 1);
        check (std::abs (g + 3.01) < 0.2 && std::abs (v.lastCut - 2000.0f) < 0.5f, "Filter 1 kHz: a 1 kHz sine comes out " + juce::String (g, 2) + " dB; key track 100 % one octave up -> cutoff " + juce::String (v.lastCut, 1) + " Hz");
    }
    // 6. The filter envelope sweeps the cutoff by its octaves, and settles at the sustain
    {
        auto s = openFilter(); s.cutoffHz = 250; s.envOct = 4; s.fEnv[0] = 0.0005; s.fEnv[1] = 0.2; s.fEnv[2] = 0.5;
        Voice v; v.prepare (fs); v.start (true); std::vector<float> l (32), r (32); float peak = 0;
        for (int i = 0; i < (int) fs; i += 32) { v.render (l.data(), r.data(), 32, 110, s, {}, 0, 1, 60, 1, 1); peak = std::max (peak, v.lastCut); }
        check (std::abs (peak - 4000.0f) < 60.0f && std::abs (v.lastCut - 1000.0f) < 5.0f, "Filter envelope +4 oct from 250 Hz: peaks at " + juce::String (juce::roundToInt (peak)) + " Hz, settles at " + juce::String (juce::roundToInt (v.lastCut)) + " Hz (sustain 50 % = +2 oct)");
    }
    // 7. Drive is clean for quiet signals, and a released voice goes fully silent
    {
        auto thd = [] (const std::vector<float>& x)
        {   // harmonics 2..9 of 440 Hz over the fundamental, from the spectrum
            auto sp = spectrum (x, 9600, 15); const double bin = fs / 32768.0; auto at = [&] (double f) { double m = 0; for (int k = (int) (f / bin) - 3; k <= (int) (f / bin) + 3; ++k) m = std::max (m, sp[(size_t) k]); return m; };
            double h = 0; for (int k = 2; k <= 9; ++k) h += std::pow (at (440.0 * k), 2); return std::sqrt (h) / at (440.0);
        };
        auto pk = [] (const std::vector<float>& x) { double m = 0; for (size_t i = 9600; i < x.size(); ++i) m = std::max (m, (double) std::abs (x[i])); return m; };
        auto s = openFilter(); s.shape = 0; auto clean = renderVoice (s, 440, 1.0); s.driveDb = 12; auto driven = renderVoice (s, 440, 1.0);
        auto t = renderVoice (openFilter(), 440, 1.0, 0.2); double tail = 0; for (size_t i = (size_t) (0.6 * fs); i < t.size(); ++i) tail = std::max (tail, (double) std::abs (t[i]));
        const double t0 = thd (clean) * 100, t1 = thd (driven) * 100, dp = db (pk (driven) / pk (clean));
        check (t0 < 0.01 && t1 > 5 && std::abs (dp) < 1.0, "Drive +12 dB on a sine: THD " + juce::String (t0, 4) + " % -> " + juce::String (t1, 1) + " %, peak level moves " + juce::String (dp, 2) + " dB (the level stays put)");
        check (tail == 0.0, "Release 50 ms: exact silence 350 ms later (peak " + juce::String (tail) + ")");
    }
    // 8. CPU: 16 voices, 48 dB/oct with resonance, sub and noise, at 48 kHz
    {
        auto s = openFilter(); s.slope = 48; s.res = 0.6; s.cutoffHz = 1500; s.subLevel = 0.5; s.noiseLevel = 0.2; s.driveDb = 6; s.shape = 2.5;
        std::vector<Voice> vs (16); for (int k = 0; k < 16; ++k) { vs[(size_t) k].prepare (fs); vs[(size_t) k].start (true); }
        std::vector<float> l (32), r (32); const int N = (int) (2 * fs);
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; i += 32) for (int k = 0; k < 16; ++k) vs[(size_t) k].render (l.data(), r.data(), 32, 110.0 * (1 + k * 0.25), s, {}, std::sin (i * 0.0001), 1, 48 + k, 1, 1);
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (secs / 2.0 < 0.25, "16 voices (48 dB/oct, resonance, sub, noise, drive): " + juce::String (secs / 2.0 * 100.0, 1) + " % of one core");
    }
    std::printf ("Mote: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
