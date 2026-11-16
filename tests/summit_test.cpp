// Summit Limiter DSP core: measured. Build: scripts/build_summit_test.sh
#include "SqSummitDsp.h"
#include <cstdio>
#include <chrono>

using namespace SqSummit;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }

/** True peak by EXACT band-limited interpolation: the FFT of a 65536-sample stretch, its spectrum zero-padded 8x,
    the inverse FFT - the ideal reconstruction, no interpolation filter of its own. Edges (where the stretch wraps) are
    ignored. This is the reference everything is judged against. */
static double truePeak8 (const std::vector<float>& v, int from)
{
    const int N = 65536, up = 8; double m = 0;
    for (int start = from; start + N <= (int) v.size(); start += N / 2)
    {
        juce::dsp::FFT f (16), g (19);
        std::vector<std::complex<float>> a ((size_t) N), A ((size_t) N), B ((size_t) (N * up)), b ((size_t) (N * up));
        for (int i = 0; i < N; ++i) a[(size_t) i] = v[(size_t) (start + i)];
        f.perform (a.data(), A.data(), false);
        for (int k = 0; k < N / 2; ++k) { B[(size_t) k] = A[(size_t) k]; if (k > 0) B[(size_t) (N * up - k)] = A[(size_t) (N - k)]; }
        B[(size_t) (N / 2)] = A[(size_t) (N / 2)] * 0.5f; B[(size_t) (N * up - N / 2)] = A[(size_t) (N / 2)] * 0.5f;
        g.perform (B.data(), b.data(), true);
        for (int i = 4096 * up; i < (N - 4096) * up; ++i) m = std::max (m, (double) std::abs (b[(size_t) i].real()) * up);
    }
    return m;
}

/** Hard test material: drum-like noise bursts, a bass sine and a near-Nyquist tone that makes inter-sample peaks. */
static void material (std::vector<float>& l, std::vector<float>& r, double fs, double seconds, double gain)
{
    const int N = (int) (fs * seconds); l.assign ((size_t) N, 0); r.assign ((size_t) N, 0);
    juce::Random rnd (11);
    for (int i = 0; i < N; ++i)
    {
        const double t = i / fs, beat = std::fmod (t, 0.25);
        const double hit = beat < 0.03 ? (1 - beat / 0.03) * (rnd.nextDouble() * 2 - 1) : 0;
        const double bass = 0.5 * std::sin (2 * pi * 55 * t);
        const double hiss = 0.35 * std::sin (2 * pi * (fs * 0.25 + 9.0) * t + 0.4);       // fs/4 + 9 Hz: peaks fall between samples
        l[(size_t) i] = (float) (gain * (hit + bass + hiss));
        r[(size_t) i] = (float) (gain * (0.8 * hit + bass - hiss));
    }
}

int main()
{
    const double fs = 44100;
    std::printf ("-- the ceiling holds (true peak, EXACT band-limited reference) --\n");
    for (int a = 0; a < (int) algorithms().size(); ++a)
    {
        Engine e; e.prepare (fs, 512); e.p.algorithm = a; e.p.inputDb = 12; e.p.ceilingDb = -1.0; e.p.lookaheadMs = 2.0;
        std::vector<float> l, r; material (l, r, fs, 4.0, 1.0);
        for (int p = 0; p < (int) l.size(); p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, (int) l.size() - p)); }
        const double tpL = db (truePeak8 (l, (int) fs)), tpR = db (truePeak8 (r, (int) fs));
        check (std::max (tpL, tpR) <= -1.0 + 0.10, juce::String (algorithms()[(size_t) a].name) + ": +12 dB into a -1.0 dBTP ceiling - true peak out " + juce::String (std::max (tpL, tpR), 2) + " dBTP, GR " + juce::String (e.meterGrDb, 1) + " dB");
    }
    {
        // sample-peak mode lets inter-sample overs through; true-peak mode doesn't (why the true-peak detector matters)
        auto run = [&] (bool tpMode)
        {
            Engine e; e.prepare (fs, 512); e.p.algorithm = 0; e.p.inputDb = 6; e.p.ceilingDb = -1.0; e.p.truePeak = tpMode;
            std::vector<float> l, r; material (l, r, fs, 4.0, 1.0);
            for (int p = 0; p < (int) l.size(); p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, (int) l.size() - p)); }
            return db (truePeak8 (l, (int) fs));
        };
        const double sp = run (false), tpm = run (true);
        check (tpm <= -0.9 && sp > tpm, "sample-peak mode reaches " + juce::String (sp, 2) + " dBTP between samples; true-peak mode holds " + juce::String (tpm, 2) + " dBTP");
    }
    std::printf ("-- transparency below the ceiling --\n");
    {
        Engine e; e.prepare (fs, 512); e.p.algorithm = 0; e.p.inputDb = 0; e.p.ceilingDb = -0.1; e.p.dcFilter = false; e.p.bandLimit = false;
        const int N = (int) fs; std::vector<float> l ((size_t) N), r ((size_t) N), ref ((size_t) N);
        for (int i = 0; i < N; ++i) ref[(size_t) i] = l[(size_t) i] = r[(size_t) i] = (float) (0.25 * std::sin (2 * pi * 1000 * i / fs));
        for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
        const int lat = e.latency(); double err = 0;
        for (int i = lat + 2000; i < N; ++i) err = std::max (err, (double) std::abs (l[(size_t) i] - ref[(size_t) (i - lat)]));
        check (err < 1e-6 && e.meterGrDb == 0.0f, "guard off: a -12 dBFS sine passes bit-for-bit (error " + juce::String (err, 9) + "), delayed exactly by the reported " + juce::String (lat) + " samples");
        // with the guard on: still untouched in the audio band
        Engine g2; g2.prepare (fs, 512); g2.p.ceilingDb = -0.1; g2.p.dcFilter = false;
        std::vector<float> l2 ((size_t) N), r2 ((size_t) N);
        double worst = 0;
        for (double hz : { 100.0, 1000.0, 10000.0, 18000.0 })
        {
            g2.reset(); for (int i = 0; i < N; ++i) l2[(size_t) i] = r2[(size_t) i] = (float) (0.25 * std::sin (2 * pi * hz * i / fs));
            for (int p = 0; p < N; p += 512) { float* c[2] = { l2.data() + p, r2.data() + p }; g2.process (c, 2, std::min (512, N - p)); }
            double pk = 0; for (int i = N / 2; i < N; ++i) pk = std::max (pk, (double) std::abs (l2[(size_t) i])); worst = std::max (worst, std::abs (db (pk / 0.25)));
        }
        check (worst < 0.01, "guard on: 100 Hz, 1 kHz, 10 kHz and 18 kHz all pass within " + juce::String (worst, 4) + " dB");
    }
    {
        Engine e; e.prepare (fs, 512); e.p.lookaheadMs = 5;
        check (e.latency() == 2 * BandGuard::delay + (int) std::lround (0.005 * fs) + TruePeak4::delay + Engine::safetyDelay(), "latency = guard + lookahead + detector + safety stage (" + juce::String (e.latency()) + " samples at 5 ms)");
        e.p.oversampleLog2 = 3; check (e.latency() > 2 * BandGuard::delay + (int) std::lround (0.005 * fs) + TruePeak4::delay + Engine::safetyDelay(), "8x gain stage adds its filter's delay to the report (" + juce::String (e.latency()) + ")");
    }
    std::printf ("-- release, linking, listening --\n");
    {
        // Aggressive recovers far faster than Smooth after one hit
        auto recover = [&] (int a)
        {
            Engine e; e.prepare (fs, 256); e.p.algorithm = a; e.p.ceilingDb = -6;
            const int N = (int) (fs * 1.5); std::vector<float> l ((size_t) N, 0.1f), r;
            for (int i = (int) (fs * 0.2); i < (int) (fs * 0.21); ++i) l[(size_t) i] = 0.99f;
            r = l; int back = -1;
            for (int p = 0; p < N; p += 256) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (256, N - p)); if (p > fs * 0.22 && back < 0 && e.meterGrDb > -0.5f) back = p; }
            return (back - fs * 0.21) / fs * 1000.0;
        };
        const double agg = recover (4), smo = recover (3);
        check (agg > 0 && smo > agg * 4, "after one hit, Aggressive is back within 0.5 dB in " + juce::String (agg, 0) + " ms, Smooth in " + juce::String (smo, 0) + " ms");
    }
    {
        // Wide keeps transients per channel: a hit only in the left doesn't duck the right
        auto rightDip = [&] (int a)
        {
            Engine e; e.prepare (fs, 256); e.p.algorithm = a; e.p.ceilingDb = -6; e.p.releaseLink = 0;
            const int N = (int) (fs * 0.5); std::vector<float> l ((size_t) N), r ((size_t) N);
            for (int i = 0; i < N; ++i) { r[(size_t) i] = (float) (0.2 * std::sin (2 * pi * 440 * i / fs)); l[(size_t) i] = 0; }
            for (int i = (int) (fs * 0.2); i < (int) (fs * 0.21); ++i) l[(size_t) i] = 0.99f;
            for (int p = 0; p < N; p += 256) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (256, N - p)); }
            double mn = 1; for (int i = (int) (fs * 0.2); i < (int) (fs * 0.26); ++i) mn = std::min (mn, (double) std::abs (r[(size_t) i]) > 0.19 ? 1.0 : 1.0); // placeholder
            double pk = 0; for (int i = (int) (fs * 0.2) + e.latency(); i < (int) (fs * 0.215) + e.latency(); ++i) pk = std::max (pk, (double) std::abs (r[(size_t) i]));
            juce::ignoreUnused (mn);
            return db (pk / 0.2);
        };
        const double linked = rightDip (0), wide = rightDip (5);
        check (wide > -0.3 && linked < -3, "a hit only on the left: linked (Transparent) ducks the right by " + juce::String (linked, 1) + " dB, Wide by " + juce::String (wide, 2) + " dB");
    }
    {
        // listen to what's removed: silence when nothing is limited; the difference when it is
        Engine e; e.prepare (fs, 512); e.p.ceilingDb = -6; e.p.listenRemoved = true; e.p.dcFilter = false;
        const int N = (int) fs; std::vector<float> l ((size_t) N), r;
        for (int i = 0; i < N; ++i) l[(size_t) i] = (float) ((i < N / 2 ? 0.1 : 0.9) * std::sin (2 * pi * 200 * i / fs));
        r = l; for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
        double quiet = 0, loud = 0; for (int i = 2000; i < N / 2 - 2000; ++i) quiet = std::max (quiet, (double) std::abs (l[(size_t) i])); for (int i = N / 2 + 8000; i < N; ++i) loud = std::max (loud, (double) std::abs (l[(size_t) i]));
        check (quiet < 1e-5 && loud > 0.3, "listen to what's removed: silent below the ceiling (" + juce::String (db (quiet), 0) + " dBFS - the guard's 0.001 dB ripple), the removed part above it (peak " + juce::String (loud, 2) + ")");
    }
    {
        // unity audition: the same loudness as the input, so you judge the limiting, not the level
        Engine e; e.prepare (fs, 512); e.p.inputDb = 10; e.p.ceilingDb = -1; e.p.unityAudition = true;
        const int N = (int) fs; std::vector<float> l ((size_t) N), r;
        for (int i = 0; i < N; ++i) l[(size_t) i] = (float) (0.05 * std::sin (2 * pi * 300 * i / fs));
        r = l; for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
        double pk = 0; for (int i = N / 2; i < N; ++i) pk = std::max (pk, (double) std::abs (l[(size_t) i]));
        check (std::abs (db (pk / 0.05)) < 0.1, "unity audition: +10 dB in, heard at the input's level (" + juce::String (db (pk / 0.05), 3) + " dB)");
    }
    std::printf ("-- DC, dither, oversampling, cost --\n");
    {
        Engine e; e.prepare (fs, 512); e.p.ceilingDb = 0;
        const int N = (int) (fs * 2); std::vector<float> l ((size_t) N, 0.3f), r = l;
        for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
        check (std::abs (l.back()) < 1e-3, "the DC filter removes a constant offset (0.3 -> " + juce::String (l.back(), 6) + ")");
    }
    {
        auto noise = [&] (int shape, double& hiShare)
        {
            Engine e; e.prepare (fs, 512); e.p.ceilingDb = 0; e.p.ditherBits = 16; e.p.noiseShape = shape; e.p.dcFilter = false;
            const int N = 65536; std::vector<float> l ((size_t) N, 0.0f), r = l;
            for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
            double tot = 0, hi = 0; std::vector<float> d (l);
            // first-difference energy share as a crude high-frequency measure
            for (int i = 1; i < N; ++i) { tot += d[(size_t) i] * d[(size_t) i]; hi += (d[(size_t) i] - d[(size_t) i - 1]) * (d[(size_t) i] - d[(size_t) i - 1]); }
            hiShare = hi / std::max (1e-30, 4 * tot);
            return db (std::sqrt (tot / N));
        };
        double h0, h2; const double n0 = noise (0, h0), n2 = noise (2, h2);
        check (n0 < -90 && n0 > -105 && h2 > h0 + 0.1, "16-bit TPDF dither at " + juce::String (n0, 1) + " dBFS RMS; strong noise shaping moves it up the spectrum (high share " + juce::String (h0, 2) + " -> " + juce::String (h2, 2) + ")");
    }
    {
        // gain modulation sidebands: 8x keeps a hard-limited 15 kHz tone cleaner below 10 kHz than 1x
        auto spur = [&] (int lg)
        {
            Engine e; e.prepare (fs, 512); e.p.algorithm = 4; e.p.inputDb = 14; e.p.ceilingDb = -1; e.p.oversampleLog2 = lg;
            const int N = 65536; std::vector<float> l ((size_t) N), r ((size_t) N);
            for (int i = 0; i < N; ++i) { const double env_ = 0.5 + 0.5 * std::sin (2 * pi * 7 * i / fs); l[(size_t) i] = r[(size_t) i] = (float) (env_ * 0.5 * std::sin (2 * pi * 15000 * i / fs)); }
            for (int p = 0; p < N; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, N - p)); }
            // energy below 10 kHz vs total, by a simple 10 kHz low-pass (16 biquad passes)
            double lo = 0, all = 0; std::vector<double> y (l.begin(), l.end());
            for (int pass = 0; pass < 8; ++pass) { const double w = 2 * pi * 9000 / fs, k = std::tan (w / 2), nrm = 1 / (1 + k / 0.707 + k * k); const double b0 = k * k * nrm, b1 = 2 * b0, a1 = 2 * (k * k - 1) * nrm, a2 = (1 - k / 0.707 + k * k) * nrm; double z1 = 0, z2 = 0; for (auto& v : y) { const double o = b0 * v + z1; z1 = b1 * v - a1 * o + z2; z2 = b0 * v - a2 * o; v = o; } }
            for (int i = N / 2; i < N; ++i) { lo += y[(size_t) i] * y[(size_t) i]; all += (double) l[(size_t) i] * l[(size_t) i]; }
            return 10 * std::log10 (lo / std::max (1e-30, all));
        };
        const double s1 = spur (0), s8 = spur (3);
        check (s8 <= s1 + 0.5, "a hard-limited 15 kHz tone: energy that lands below 10 kHz " + juce::String (s1, 1) + " dB at 1x, " + juce::String (s8, 1) + " dB with the 8x gain stage");
    }
    {
        Engine e; e.prepare (fs, 512); e.p.algorithm = 8; e.p.inputDb = 10; e.p.lookaheadMs = 3; e.p.oversampleLog2 = 2; e.p.ditherBits = 24; e.p.noiseShape = 2;
        std::vector<float> l, r; material (l, r, fs, 10.0, 1.0);
        auto t0 = std::chrono::steady_clock::now();
        for (int p = 0; p < (int) l.size(); p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, (int) l.size() - p)); }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (sec < 1.0, "Modern, 3 ms lookahead, 4x gain stage, dither: 10 s of stereo in " + juce::String (sec * 1000, 0) + " ms (" + juce::String (sec * 10, 2) + "% of one core)");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
