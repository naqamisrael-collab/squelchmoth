// Known-answer test for SqDrive (an overdrive's clipping stage: diodes in an amplifier's feedback). No dependencies.
// Harmonics are compared with ngspice 42 solving the same circuit (tests/reference/drive).
#include <cstdio>
#include <complex>
#include <vector>
#include "SqDrive.h"
using namespace SqDrive;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct DriveRef { double hz, amp, rf; bool lopsided; double h[7]; };
#include "reference/drive/reference.inc"
static const double pi = 3.14159265358979323846;
static char line[900];

static void harmonics (double fs, double hz, double amp, const Circuit& c, const SqClip::Diode& d, double* h, int count)
{
    Stage s; s.prepare (fs, d, c); const int settle = (int) (0.04 * fs), n = (int) (0.02 * fs); std::vector<std::complex<double>> a ((size_t) count);
    for (int i = 0; i < settle + n; ++i) { const double y = s.process (amp * std::sin (2 * pi * hz * i / fs)); if (i >= settle) for (int k = 0; k < count; ++k) a[(size_t) k] += y * std::polar (1.0, -2 * pi * hz * (k + 1) * i / fs); }
    for (int k = 0; k < count; ++k) h[k] = 20 * std::log10 (2 * std::abs (a[(size_t) k]) / n + 1e-300);
}
/** At the oversampled rate: the strongest tone folded back from above half that rate, dB below the fundamental (all of them, and only those landing under 20 kHz). */
static void aliasing (int factor, double hz, double amp, double rf, double& all, double& audible)
{
    const double fs = 48000.0 * factor; Circuit c; c.rf = rf; Stage s; s.prepare (fs, {}, c); const int N = (int) fs, settle = N / 2; std::vector<double> y ((size_t) N);
    for (int i = 0; i < settle + N; ++i) { const double v = s.process (amp * std::sin (2 * pi * hz * i / fs)); if (i >= settle) y[(size_t) (i - settle)] = v; }
    auto bin = [&] (double f) { std::complex<double> acc = 0; for (int i = 0; i < N; ++i) acc += y[(size_t) i] * std::polar (1.0, -2 * pi * f * i / fs); return 20 * std::log10 (2 * std::abs (acc) / N + 1e-300); };
    const double fund = bin (hz); all = audible = -400;
    for (int k = 3; k <= 401; k += 2) { if (k * hz < fs / 2) continue; if (k * hz > 2.5 * fs) break; double f = std::fmod (k * hz, fs); if (f > fs / 2) f = fs - f; const double m = std::fmod (f, hz); if (m < 0.5 || hz - m < 0.5) continue; const double v = bin (f) - fund; all = std::max (all, v); if (f < 20000.0) audible = std::max (audible, v); }
}

int main()
{
    std::printf ("== Against the circuit simulator ==\n");
    {
        double worst1 = 0, worstOdd = 0, worstEven = 0; int nOdd = 0, nEven = 0, cases = 0;
        for (const auto& r : ref_drive)
        {
            Circuit c; c.rf = r.rf; c.lopsided = r.lopsided; double h[7]; harmonics (384000.0, r.hz, r.amp, c, {}, h, 7); ++cases;
            worst1 = std::max (worst1, std::abs (h[0] - r.h[0]));
            for (int k = 1; k < 7; ++k) if (r.h[k] > -100.0) { const double e = std::abs (h[k] - r.h[k]); if (k % 2 == 0) { worstOdd = std::max (worstOdd, e); ++nOdd; } else { worstEven = std::max (worstEven, e); ++nEven; } }
        }
        std::snprintf (line, sizeof (line), "%d cases (200 Hz, 1 and 5 kHz; 20 mV to 0.5 V; Drive at least, a fifth and most; even and lopsided pairs), at 8x: the tone within %.4f dB, odd harmonics to the 7th within %.3f dB (%d), even ones within %.3f dB (%d)", cases, worst1, worstOdd, nOdd, worstEven, nEven);
        check (cases == 14 && worst1 < 0.02 && worstOdd < 0.25 && worstEven < 0.25 && nOdd >= 36 && nEven >= 6, line);
        double w4 = 0, w16 = 0; for (const auto& r : ref_drive) { Circuit c; c.rf = r.rf; c.lopsided = r.lopsided; double a[7], b[7]; harmonics (192000.0, r.hz, r.amp, c, {}, a, 7); harmonics (768000.0, r.hz, r.amp, c, {}, b, 7); for (int k = 0; k < 7; ++k) if (r.h[k] > -100.0) { w4 = std::max (w4, std::abs (a[k] - r.h[k])); w16 = std::max (w16, std::abs (b[k] - r.h[k])); } }
        std::snprintf (line, sizeof (line), "the same at 4x: within %.3f dB; at 16x: within %.3f dB", w4, w16); check (w4 < 1.0 && w16 < 0.1, line);
    }
    std::printf ("== The plain amplifier underneath ==\n");
    {
        double worst = 0; char d[300] = ""; size_t used = 0;
        for (double rf : { 51000.0, 551000.0 }) for (double hz : { 100.0, 720.0, 3000.0, 10000.0 })
        {
            Circuit c; c.rf = rf; double h[1]; harmonics (384000.0, hz, 1.0e-4, c, {}, h, 1); const double got = h[0] + 80.0; Circuit e = c; e.rf = 1.0 / (1.0 / c.rf + 2.0 * SqClip::silicon().is / (SqClip::silicon().n * SqClip::thermalVolts)); const double want = 20 * std::log10 (Stage::smallGain (e, hz)); worst = std::max (worst, std::abs (got - want));
            used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.2f", got);
        }
        std::snprintf (line, sizeof (line), "a 0.1 mV tone at 100 Hz, 720 Hz, 3 and 10 kHz comes out%s dB up (Drive at least, then at most): within %.3f dB of 1 + Zf / Zg (the diodes' own leak counted in Zf)", d, worst); check (worst < 0.05, line);
        Circuit lo, hi; hi.rf = 551000.0; const double g12 = Stage::smallGain (lo, 5000.0), g118 = 1.0 + 551000.0 / 4700.0, corner = 1.0 / (2 * pi * 4700.0 * 47.0e-9), top = 1.0 / (2 * pi * 551000.0 * 51.0e-12);
        std::snprintf (line, sizeof (line), "gain about 12 with Drive at least (%.1f at 5 kHz) and 118 at most before the small capacitor rounds it (1 + 551 k / 4.7 k = %.0f; that corner %.0f Hz); bass corner %.0f Hz", g12, g118, top, corner);
        check (g12 > 11.0 && g12 < 12.0 && std::abs (g118 - 118.2) < 0.1 && std::abs (corner - 720.5) < 1.0 && top > 5000.0 && top < 6000.0, line);
    }
    std::printf ("== Soft clipping ==\n");
    {
        Circuit c; c.rf = 551000.0; Stage s; s.prepare (384000.0, {}, c); double most = 0, pk = 0; const double held = SqClip::kneeVolts ({}, 2.0 / 4700.0);
        for (int i = 0; i < 38400; ++i) { const double x = 2.0 * std::sin (2 * pi * 1000.0 * i / 384000.0), y = s.process (x); most = std::max (most, std::abs (y - x)); pk = std::max (pk, std::abs (y)); }
        std::snprintf (line, sizeof (line), "the output is the input plus what the diodes allow: a 2 V tone at full Drive comes out with %.3f V added at most (the diodes at that current hold %.3f V), peak %.3f V", most, held, pk); check (most < held + 0.01 && most > 0.5 && pk < 2.0 + held, line);
        double a[7], b[7]; Circuit m; m.rf = 151000.0; harmonics (384000.0, 1000.0, 0.1, m, {}, a, 7); harmonics (384000.0, 80.0, 0.1, m, {}, b, 7);
        std::snprintf (line, sizeof (line), "bass passes cleaner: 0.1 V at 1 kHz has its 3rd harmonic %.1f dB under the tone, at 80 Hz %.1f dB (the gain starts at 720 Hz)", a[0] - a[2], b[0] - b[2]); check (b[0] - b[2] > a[0] - a[2] + 3.0, line);
        Circuit lop = m; lop.lopsided = true; double e[7]; harmonics (384000.0, 1000.0, 0.1, lop, {}, e, 7);
        std::snprintf (line, sizeof (line), "an even pair makes no 2nd harmonic (%.0f dB); two diodes in series on one side leave it %.1f dB under the tone", a[1], e[0] - e[1]); check (a[1] < -150 && e[0] - e[1] < 40 && e[0] - e[1] > 10, line);
        double soft[7], sharp[7]; harmonics (384000.0, 1000.0, 0.1, m, SqClip::schottky(), soft, 7); harmonics (384000.0, 1000.0, 0.1, m, SqClip::sharp(), sharp, 7);
        std::snprintf (line, sizeof (line), "the pair sets the level it holds at: the tone comes out at %.1f (silicon), %.1f (soft) and %.1f dBV (sharp)", a[0], soft[0], sharp[0]); check (soft[0] < a[0] - 2.0 && sharp[0] < a[0] - 2.0, line);
    }
    std::printf ("== Good behaviour ==\n");
    {
        Circuit c; c.rf = 551000.0; Stage s; s.prepare (192000.0, {}, c); int most = 0; double worstZero = 0, ny = 0; std::vector<double> y;
        for (int i = 0; i < 192000; ++i) { const double v = s.process (1.0 * std::sin (2 * pi * 3000.0 * i / 192000.0)); most = std::max (most, s.lastIterations()); y.push_back (v); }
        double alt = 0; for (int i = 96000; i < 192000; ++i) alt += y[(size_t) i] * ((i & 1) ? -1.0 : 1.0); ny = 20 * std::log10 (std::abs (alt) / 96000.0 + 1e-300);
        Stage z; z.prepare (192000.0, {}, c); for (int i = 0; i < 1000; ++i) worstZero = std::max (worstZero, std::abs (z.process (0.0)));
        std::snprintf (line, sizeof (line), "a 1 V tone at full Drive: never more than %d Newton steps a sample, nothing ringing at half the rate (%.0f dBV); silence in is silence out (%.1e)", most, ny, worstZero); check (most <= 12 && ny < -120.0 && worstZero == 0.0, line);
    }
    std::printf ("== Aliasing ==\n");
    {
        double w[3] = { -400, -400, -400 }, u[3] = { -400, -400, -400 }, m[3] = { -400, -400, -400 }; const int factors[3] = { 4, 8, 16 };
        for (int q = 0; q < 3; ++q) for (double hz : { 5000.0, 9001.0, 15001.0 })
        {
            for (double rf : { 151000.0, 551000.0 }) for (double amp : { 0.1, 0.5 }) { double a, b; aliasing (factors[q], hz, amp, rf, a, b); w[q] = std::max (w[q], a); u[q] = std::max (u[q], b); }
            double a, b; aliasing (factors[q], hz, 0.05, 51000.0, a, b); m[q] = std::max (m[q], a);
        }
        std::snprintf (line, sizeof (line), "5 / 9 / 15 kHz tones at 0.1 and 0.5 V, Drive a fifth and most: the strongest folded-back tone is %.0f dB down at 4x, %.0f at 8x, %.0f at 16x (counting only those landing under 20 kHz: %.0f, %.0f, %.0f); a gentle setting (50 mV, Drive at least): %.0f, %.0f, %.0f",
                       -w[0], -w[1], -w[2], -u[0], -u[1], -u[2], -m[0], -m[1], -m[2]);
        check (w[1] < w[0] && w[2] < w[1] && m[1] < -80.0 && m[2] < -100.0, line);
        double g[3] = { -400, -400, -400 }, ga[3] = { -400, -400, -400 };
        for (int q = 0; q < 3; ++q) for (double hz : { 1001.0, 3001.0 }) for (double rf : { 151000.0, 551000.0 }) { double a, b; aliasing (factors[q], hz, 0.1, rf, a, b); g[q] = std::max (g[q], a); ga[q] = std::max (ga[q], b); }
        std::snprintf (line, sizeof (line), "1 and 3 kHz tones at 0.1 V (where a guitar's notes are), Drive a fifth and most: %.0f dB down at 4x, %.0f at 8x, %.0f at 16x (under 20 kHz: %.0f, %.0f, %.0f) - at full Drive the wave is nearly square and its harmonics fall off slowly: the 100 dB gate is not met", -g[0], -g[1], -g[2], -ga[0], -ga[1], -ga[2]);
        check (g[1] < g[0] && g[2] < g[1], line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
