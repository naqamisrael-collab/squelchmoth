// Conformance test for SqLoudness against the published cases: EBU Tech 3341 v4 Table 1 (cases 1-6 and 9-23, made
// here from their printed descriptions at 48 kHz; cases 7 and 8 are recordings this test does not have) and EBU Tech
// 3342 v4 (cases 1-4), and the K-weighting table of ITU-R BS.1770-5. No dependencies.
#include <cstdio>
#include <functional>
#include <vector>
#include "SqLoudness.h"
using namespace SqLoudness;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static char line[1200]; static const double fs = 48000.0, pi = 3.14159265358979323846;
struct Seg { double seconds, dbfs; };   // a 1 kHz sine in phase on both channels; dbfs below -200 = silence
static std::vector<std::vector<float>> tones (const std::vector<Seg>& segs, int channels = 2, const std::vector<double>& chDb = {})
{
    std::vector<std::vector<float>> x ((size_t) channels); long long t = 0;
    for (auto& s : segs) { const long long n = std::llround (s.seconds * fs); for (long long i = 0; i < n; ++i, ++t) for (int c = 0; c < channels; ++c) { const double db = chDb.empty() ? s.dbfs : chDb[(size_t) c]; x[(size_t) c].push_back (db < -200 ? 0.0f : (float) (std::pow (10.0, db / 20.0) * std::sin (2 * pi * 1000.0 * (double) t / fs))); } }
    return x;
}
static Meter run (const std::vector<std::vector<float>>& x)
{
    Meter m; m.prepare (fs, (int) x.size()); std::vector<const float*> p; for (auto& c : x) p.push_back (c.data()); m.pushPlanar (p.data(), (int) x[0].size()); return m;
}
int main()
{
    std::printf ("== K-weighting at 48 kHz against BS.1770-5 Tables 1 and 2 ==\n");
    {
        Biquad s, h; kWeighting (48000.0, s, h); const double t1[5] = { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 }, t2[5] = { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        const double got1[5] = { s.b0, s.b1, s.b2, s.a1, s.a2 }, got2[5] = { h.b0, h.b1, h.b2, h.a1, h.a2 }; double w = 0; for (int i = 0; i < 5; ++i) w = std::max (w, std::max (std::abs (got1[i] - t1[i]), std::abs (got2[i] - t2[i])));
        std::snprintf (line, sizeof (line), "all ten coefficients within %.1e of the printed table", w); check (w < 1.0e-8, line);
    }
    std::printf ("== EBU Tech 3341 Table 1 ==\n");
    {
        const double tol = 0.1; bool ok; double v;
        auto m1 = run (tones ({ { 20, -23 } })); ok = std::abs (m1.momentary() + 23) <= tol && std::abs (m1.shortTerm() + 23) <= tol && std::abs (m1.integrated() + 23) <= tol;
        std::snprintf (line, sizeof (line), "case 1: -23 dBFS stereo sine reads M %.3f, S %.3f, I %.3f LUFS (-23.0 +-0.1)", m1.momentary(), m1.shortTerm(), m1.integrated()); check (ok, line);
        auto m2 = run (tones ({ { 20, -33 } })); ok = std::abs (m2.momentary() + 33) <= tol && std::abs (m2.shortTerm() + 33) <= tol && std::abs (m2.integrated() + 33) <= tol;
        std::snprintf (line, sizeof (line), "case 2: -33 dBFS reads M %.3f, S %.3f, I %.3f LUFS (-33.0 +-0.1)", m2.momentary(), m2.shortTerm(), m2.integrated()); check (ok, line);
        v = run (tones ({ { 10, -36 }, { 60, -23 }, { 10, -36 } })).integrated(); std::snprintf (line, sizeof (line), "case 3: -36 / -23 / -36 dBFS reads I %.3f LUFS (-23.0 +-0.1)", v); check (std::abs (v + 23) <= tol, line);
        v = run (tones ({ { 10, -72 }, { 10, -36 }, { 60, -23 }, { 10, -36 }, { 10, -72 } })).integrated(); std::snprintf (line, sizeof (line), "case 4: -72 / -36 / -23 / -36 / -72 dBFS reads I %.3f LUFS (-23.0 +-0.1)", v); check (std::abs (v + 23) <= tol, line);
        v = run (tones ({ { 20, -26 }, { 20.1, -20 }, { 20, -26 } })).integrated(); std::snprintf (line, sizeof (line), "case 5: -26 / -20 / -26 dBFS reads I %.3f LUFS (-23.0 +-0.1)", v); check (std::abs (v + 23) <= tol, line);
        v = run (tones ({ { 20, 0 } }, 5, { -28, -28, -24, -30, -30 })).integrated(); std::snprintf (line, sizeof (line), "case 6: 5.0 channels at -28 / -28 / -24 / -30 / -30 dBFS read I %.3f LUFS (-23.0 +-0.1)", v); check (std::abs (v + 23) <= tol, line);
        std::printf ("  NOTE cases 7 and 8 are recorded programme from the EBU's test set, which could not be downloaded here: not run\n");
        {   // case 9: (1.34 s at -20, 1.66 s at -30) x 5: S stays at -23.0 after 3 s
            std::vector<Seg> s; for (int k = 0; k < 5; ++k) { s.push_back ({ 1.34, -20 }); s.push_back ({ 1.66, -30 }); } auto x = tones (s); Meter m; m.prepare (fs, 2); double lo = 1e9, hi = -1e9; float f[2];
            for (size_t i = 0; i < x[0].size(); ++i) { f[0] = x[0][i]; f[1] = x[1][i]; m.push (f); if (m.seconds() >= 3.0) { lo = std::min (lo, m.shortTerm()); hi = std::max (hi, m.shortTerm()); } }
            std::snprintf (line, sizeof (line), "case 9: S after 3 s stays between %.3f and %.3f LUFS (-23.0 +-0.1)", lo, hi); check (lo >= -23.1 && hi <= -22.9, line);
        }
        {   // case 10 (file-based): 20 files, i x 0.15 s silence, 3 s at -23, 1 s silence: max S of each = -23.0
            double worst = 0; for (int i = 0; i < 20; ++i) { auto m = run (tones ({ { i * 0.15, -300 }, { 3, -23 }, { 1, -300 } })); worst = std::max (worst, std::abs (m.maxShortTerm() + 23)); }
            std::snprintf (line, sizeof (line), "case 10: the highest S of each of 20 files is within %.3f LU of -23.0 (+-0.1)", worst); check (worst <= tol, line);
        }
        {   // case 11 (live): one file, 20 segments: i x 0.15 s silence, 3 s at -38 + i, (3 - i x 0.15) s silence: max S per segment -38 ... -19
            Meter m; m.prepare (fs, 2); double worst = 0; float f[2];
            for (int i = 0; i < 20; ++i) { auto x = tones ({ { i * 0.15, -300 }, { 3, -38.0 + i }, { 3 - i * 0.15, -300 } }); m.clearMaxima(); for (size_t k = 0; k < x[0].size(); ++k) { f[0] = x[0][k]; f[1] = x[1][k]; m.push (f); } worst = std::max (worst, std::abs (m.maxShortTerm() - (-38.0 + i))); }
            std::snprintf (line, sizeof (line), "case 11: in one running meter the highest S of each of 20 segments is within %.3f LU of -38, -37 ... -19 (+-0.1)", worst); check (worst <= tol, line);
        }
        {   // case 12: (0.18 s at -20, 0.22 s at -30) x 25: M stays at -23.0 after 1 s
            std::vector<Seg> s; for (int k = 0; k < 25; ++k) { s.push_back ({ 0.18, -20 }); s.push_back ({ 0.22, -30 }); } auto x = tones (s); Meter m; m.prepare (fs, 2); double lo = 1e9, hi = -1e9; float f[2];
            for (size_t i = 0; i < x[0].size(); ++i) { f[0] = x[0][i]; f[1] = x[1][i]; m.push (f); if (m.seconds() >= 1.0) { lo = std::min (lo, m.momentary()); hi = std::max (hi, m.momentary()); } }
            std::snprintf (line, sizeof (line), "case 12: M after 1 s stays between %.3f and %.3f LUFS (-23.0 +-0.1)", lo, hi); check (lo >= -23.1 && hi <= -22.9, line);
        }
        {   double worst = 0; for (int i = 0; i < 20; ++i) { auto m = run (tones ({ { i * 0.02, -300 }, { 0.4, -23 }, { 1, -300 } })); worst = std::max (worst, std::abs (m.maxMomentary() + 23)); }
            std::snprintf (line, sizeof (line), "case 13: the highest M of each of 20 files is within %.3f LU of -23.0 (+-0.1)", worst); check (worst <= tol, line); }
        {   Meter m; m.prepare (fs, 2); double worst = 0; float f[2];
            for (int i = 0; i < 20; ++i) { auto x = tones ({ { i * 0.02, -300 }, { 0.4, -38.0 + i }, { 0.4 - i * 0.02, -300 } }); m.clearMaxima(); for (size_t k = 0; k < x[0].size(); ++k) { f[0] = x[0][k]; f[1] = x[1][k]; m.push (f); } worst = std::max (worst, std::abs (m.maxMomentary() - (-38.0 + i))); }
            std::snprintf (line, sizeof (line), "case 14: in one running meter the highest M of each of 20 segments is within %.3f LU of -38 ... -19 (+-0.1)", worst); check (worst <= tol, line); }
        // true peak, cases 15-19: stereo sines at fs/4, fs/6, fs/8 with phase offsets, 10 ms fades
        auto tp = [&] (double f, double amp, double phaseDeg) { const int n = (int) fs, fade = (int) (0.01 * fs); std::vector<std::vector<float>> x (2, std::vector<float> ((size_t) n)); for (int i = 0; i < n; ++i) { double g = 1; if (i < fade) g = (double) i / fade; if (i > n - fade) g = (double) (n - i) / fade; x[0][(size_t) i] = x[1][(size_t) i] = (float) (amp * g * std::sin (2 * pi * f * i / fs + phaseDeg * pi / 180.0)); } return run (x).truePeakDb(); };
        struct TP { int c; double f, amp, ph, want; } cases[] = { { 15, fs / 4, 0.5, 0.0, -6.0 }, { 16, fs / 4, 0.5, 45.0, -6.0 }, { 17, fs / 6, 0.5, 60.0, -6.0 }, { 18, fs / 8, 0.5, 67.5, -6.0 }, { 19, fs / 4, 1.41, 45.0, 3.0 } };
        for (auto& c : cases) { const double got = tp (c.f, c.amp, c.ph); std::snprintf (line, sizeof (line), "case %d: sine at %.0f Hz, %.2f of full scale, phase %.1f deg: true peak %.3f dBTP (%.1f, +0.2 / -0.4)", c.c, c.f, c.amp, c.ph, got, c.want); check (got - c.want <= 0.2 && got - c.want >= -0.4, line); }
        // cases 20-23: fs/6 at 0.5 with one period of fs/4 at 1.0 spliced in phase-continuously, made at 4 x fs, low-passed, taken down with offsets 0 to 3
        for (int off = 0; off < 4; ++off)
        {
            const double F = 4 * fs; const int N = (int) (2 * F); std::vector<double> hi ((size_t) N); double ph = 0; const int s0 = N / 2, len = (int) std::lround (F / (fs / 4));
            for (int i = 0; i < N; ++i) { const bool burst = i >= s0 && i < s0 + len; const double f = burst ? fs / 4 : fs / 6, a = burst ? 1.0 : 0.5; hi[(size_t) i] = a * std::sin (ph); ph += 2 * pi * f / F; }
            const int taps = 511, h2 = taps / 2; std::vector<double> lp ((size_t) taps); double sum = 0; for (int k = 0; k < taps; ++k) { const double m = k - h2, w = 0.42 - 0.5 * std::cos (2 * pi * k / (taps - 1)) + 0.08 * std::cos (4 * pi * k / (taps - 1)); lp[(size_t) k] = (m == 0 ? 2 * 0.115 : std::sin (2 * pi * 0.115 * m) / (pi * m)) * w; sum += lp[(size_t) k]; } for (auto& v : lp) v /= sum;
            std::vector<std::vector<float>> x (2); for (int i = h2 + off; i + h2 < N; i += 4) { double acc = 0; for (int k = 0; k < taps; ++k) acc += lp[(size_t) k] * hi[(size_t) (i - h2 + k)]; x[0].push_back ((float) acc); x[1].push_back ((float) acc); }
            const double got = run (x).truePeakDb(); std::snprintf (line, sizeof (line), "case %d: the spliced burst taken down at offset %d: true peak %.3f dBTP (0.0, +0.2 / -0.4)", 20 + off, off, got); check (got <= 0.2 && got >= -0.4, line);
        }
    }
    std::printf ("== EBU Tech 3342 (loudness range) ==\n");
    {
        struct L { int c; std::vector<Seg> s; double want; } cases[] = { { 1, { { 20, -20 }, { 20, -30 } }, 10 }, { 2, { { 20, -20 }, { 20, -15 } }, 5 }, { 3, { { 20, -40 }, { 20, -20 } }, 20 }, { 4, { { 20, -50 }, { 20, -35 }, { 20, -20 }, { 20, -35 }, { 20, -50 } }, 15 } };
        for (auto& c : cases) { auto s = c.s; s.push_back ({ 1.5, -300 }); const double got = run (tones (s)).range(); std::snprintf (line, sizeof (line), "case %d: LRA %.3f LU (%.0f +-1)", c.c, got, c.want); check (std::abs (got - c.want) <= 1.0, line); }
        std::printf ("  NOTE cases 5 and 6 are recorded programme from the EBU's test set: not run\n");
    }
    std::printf ("== Other rates and a long run ==\n");
    {
        for (double r : { 44100.0, 96000.0 }) { const int n = (int) (20 * r); std::vector<std::vector<float>> x (2, std::vector<float> ((size_t) n)); for (int i = 0; i < n; ++i) x[0][(size_t) i] = x[1][(size_t) i] = (float) (std::pow (10.0, -23.0 / 20.0) * std::sin (2 * pi * 1000.0 * i / r)); Meter m; m.prepare (r, 2); const float* p[2] = { x[0].data(), x[1].data() }; m.pushPlanar (p, n);
            std::snprintf (line, sizeof (line), "at %.1f kHz the -23 dBFS sine reads I %.3f, M %.3f LUFS", r / 1000, m.integrated(), m.momentary()); check (std::abs (m.integrated() + 23) <= 0.1 && std::abs (m.momentary() + 23) <= 0.1, line); }
        Meter m; m.prepare (fs, 2); float f[2]; const double a = std::pow (10.0, -23.0 / 20.0); for (long long i = 0; i < (long long) (3600 * fs); ++i) { f[0] = f[1] = (float) (a * std::sin (2 * pi * (double) (i % 48) / 48.0)); m.push (f); }
        std::snprintf (line, sizeof (line), "an hour of the -23 dBFS sine: M %.4f, S %.4f, I %.4f LUFS (the running sums do not drift)", m.momentary(), m.shortTerm(), m.integrated()); check (std::abs (m.momentary() + 23) < 0.02 && std::abs (m.shortTerm() + 23) < 0.02 && std::abs (m.integrated() + 23) < 0.02, line);
        Meter mono; mono.prepare (fs, 1); std::vector<float> one ((size_t) (20 * fs)); for (size_t i = 0; i < one.size(); ++i) one[i] = (float) (a * std::sin (2 * pi * 1000.0 * (double) i / fs)); const float* p1[1] = { one.data() }; mono.pushPlanar (p1, (int) one.size());
        std::snprintf (line, sizeof (line), "a mono file (one channel) of the same sine reads %.3f LUFS - 3 dB under the stereo pair, as BS.1770 sums channels", mono.integrated()); check (std::abs (mono.integrated() + 26.01) < 0.1, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
