// Squelchmoth 1.42 - Voltage Clip: the diode clipper against the same circuit in a circuit simulator
// (tests/reference/clip/make_reference.py, ngspice 42; 1 kHz at 0.1 to 40 V).
#include "SqClip.h"
#include "SqTube.h"
#include <complex>
#include <cstdio>
#include <chrono>
using namespace SqClip;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct ClipRef { int diode, lopsided; double amp; double h[7]; double top, bottom; };
#include "reference/clip/reference.inc"
static double bin (const std::vector<double>& y, double cyc) { std::complex<double> a = 0; const int n = (int) y.size(); for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -2.0 * 3.14159265358979323846 * cyc * (double) i / n); return 20 * std::log10 (std::abs (a) / (n / 2.0) + 1e-300); }
static Diode kind (int i) { return i == 1 ? schottky() : i == 2 ? sharp() : silicon(); }
static std::vector<double> run (int os, double fs, double amp, double hz, int n, int settle, const Diode& d, const Circuit& c, double* top = nullptr, double* bottom = nullptr, int* most = nullptr)
{
    Clipper k; k.prepare (fs * os, d, c); SqTube::Oversampler o; o.prepare (os); std::vector<double> y ((size_t) n); double hi = -1e9, lo = 1e9; int its = 0;
    for (int i = 0; i < settle + n; ++i)
    {
        const double v = o.process (amp * std::sin (2.0 * 3.14159265358979323846 * hz * i / fs), [&] (double x) { const double r = k.process (x); its = std::max (its, k.lastIterations()); if (i >= settle) { hi = std::max (hi, r); lo = std::min (lo, r); } return r; });
        if (i >= settle) y[(size_t) (i - settle)] = v;
    }
    if (top != nullptr) *top = hi; if (bottom != nullptr) *bottom = lo; if (most != nullptr) *most = its; return y;
}

int main()
{
    const double fs = 48000.0; char line[600];
    std::printf ("== The diode law ==\n");
    {
        double i, di; diode (silicon(), 0.6, i, di); const double back = i * 0.6458 + 1.906 * thermalVolts * std::log1p (i / 4.352e-9);
        std::snprintf (line, sizeof (line), "silicon at 0.6 V passes %.3f mA; putting that current back through junction and resistance gives %.9f V", i * 1000, back); check (std::abs (back - 0.6) < 1e-10 && i > 5e-4 && i < 5e-3, line);
        double worst = 0; for (int kd = 0; kd < 3; ++kd) for (double v = -1.0; v < 1.5; v += 0.013) { double a, da, b, db, m, dm; diode (kind (kd), v - 1e-6, a, da); diode (kind (kd), v + 1e-6, b, db); diode (kind (kd), v, m, dm); if (std::abs (v) > 1e-5) worst = std::max (worst, std::abs ((b - a) / 2e-6 - dm) / (std::abs (dm) + 1e-12)); }
        std::snprintf (line, sizeof (line), "the slope it reports is the slope it has (within %.1e, three diode types, -1 to 1.5 V)", worst); check (worst < 1e-3, line);
        std::snprintf (line, sizeof (line), "knee (100 uA): silicon %.3f V, low-threshold soft %.3f V, low-threshold sharp %.3f V", kneeVolts (silicon()), kneeVolts (schottky()), kneeVolts (sharp()));
        check (kneeVolts (silicon()) > kneeVolts (schottky()) && kneeVolts (silicon()) > kneeVolts (sharp()) && kneeVolts (silicon()) > 0.4 && kneeVolts (silicon()) < 0.6, line);
    }
    std::printf ("== The clipper against the circuit simulator ==\n");
    for (int os : { 16, 8, 4 })
    {
        double worstFund = 0, worstHarm = 0, worstPeak = 0; int compared = 0;
        for (const auto& r : ref_clip)
        {
            Circuit c; c.lopsided = r.lopsided != 0; double top = 0, bottom = 0; const auto y = run (os, fs, r.amp, 1000.0, 4800, 4800, kind (r.diode), c, &top, &bottom);
            worstPeak = std::max ({ worstPeak, std::abs (top - r.top), std::abs (bottom - r.bottom) });
            for (int k = 1; k <= 7; ++k)
            {
                const double got = bin (y, k * 100), want = r.h[k - 1];
                if (k == 1) worstFund = std::max (worstFund, std::abs (got - want));
                else if (want > r.h[0] - 100.0) { worstHarm = std::max (worstHarm, std::abs (got - want)); ++compared; }
            }
        }
        std::snprintf (line, sizeof (line), "%dx: three diode types and the lopsided pair, 0.1 to 40 V - fundamental within %.3f dB (gate 0.25), %d harmonics up to the 7th within %.3f dB (gate 1), clipping levels within %.1f mV", os, worstFund, compared, worstHarm, worstPeak * 1000);
        check (worstFund <= 0.25 && worstHarm <= 1.0 && compared >= 60 && worstPeak < 0.01, line);
    }
    std::printf ("== What it does ==\n");
    {
        Circuit even, lop; lop.lopsided = true;
        const auto a = run (8, fs, 2.0, 1000.0, 4800, 4800, silicon(), even), b = run (8, fs, 2.0, 1000.0, 4800, 4800, silicon(), lop);
        std::snprintf (line, sizeof (line), "an even pair makes odd harmonics only (2nd %.0f dB below the tone); the lopsided pair adds even ones (2nd %.1f dB below)", -(bin (a, 200) - bin (a, 100)), -(bin (b, 200) - bin (b, 100)));
        check (bin (a, 200) - bin (a, 100) < -100.0 && bin (b, 200) - bin (b, 100) > -30.0, line);
        // quiet signals pass through level, with the top rolled off where resistor and capacitor put it
        double worst = 0; for (double hz : { 100.0, 1000.0, 7234.0, 15000.0 }) { const auto y = run (8, fs, 0.001, hz, 4800, 4800, silicon(), even); const double want = -10 * std::log10 (1.0 + (hz / Clipper::corner (even)) * (hz / Clipper::corner (even))); worst = std::max (worst, std::abs (bin (y, hz / 10.0) - 20 * std::log10 (0.001) - want)); }
        std::snprintf (line, sizeof (line), "quiet signals: level, with the top 3 dB down at %.0f Hz as resistor and capacitor say (within %.3f dB from 100 Hz to 15 kHz)", Clipper::corner (even), worst); check (worst <= 0.25, line);
        Circuit bright; bright.c = 2.2e-9; double t1 = 0, b1 = 0; run (8, fs, 40.0, 1000.0, 4800, 4800, silicon(), bright, &t1, &b1);
        std::snprintf (line, sizeof (line), "40 V into the silicon pair comes out at +%.3f / %.3f V: it limits (the knee is %.3f V)", t1, b1, kneeVolts (silicon())); check (t1 < 0.9 && b1 > -0.9, line);
    }
    std::printf ("== Aliasing (5 kHz sine; the largest part below 20 kHz that is not a harmonic, against the fundamental) ==\n");
    {
        const double amps[4] = { 0.5, 2.0, 10.0, 40.0 }; double a[3][4];
        for (int oi = 0; oi < 3; ++oi) for (int ai = 0; ai < 4; ++ai)
        {
            const int os = oi == 0 ? 4 : oi == 1 ? 8 : 16; const auto y = run (os, fs, amps[ai], 5000.0, 48000, 48000, silicon(), {});
            double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k)); a[oi][ai] = bin (y, 5000) - worst;
        }
        for (int oi = 0; oi < 3; ++oi) std::printf ("  NOTE %2dx: %.1f dB down at 0.5 V (just clipping), %.1f at 2 V, %.1f at 10 V, %.1f at 40 V\n", oi == 0 ? 4 : oi == 1 ? 8 : 16, a[oi][0], a[oi][1], a[oi][2], a[oi][3]);
        std::snprintf (line, sizeof (line), "just clipping: %.1f dB down at 4x (gate 100)", a[0][0]); check (a[0][0] >= 100.0, line);
        std::snprintf (line, sizeof (line), "hard clipping (2 V and 10 V): %.1f and %.1f dB down at 16x", a[2][1], a[2][2]); check (a[2][1] >= 80.0 && a[2][2] >= 80.0, line);
    }
    std::printf ("== Behaviour ==\n");
    {
        Clipper k; k.prepare (192000.0); double worst = 0; for (int i = 0; i < 1000; ++i) worst = std::max (worst, std::abs (k.process (0.0))); check (worst == 0.0, "silence in, silence out");
        bool finite = true; int most = 0; for (int i = 0; i < 192000; ++i) { const double v = k.process (1000.0 * std::sin (i * 0.9)); finite = finite && std::isfinite (v) && std::abs (v) < 3.0; most = std::max (most, k.lastIterations()); }
        std::snprintf (line, sizeof (line), "1000 V of near-Nyquist garbage is solved in at most %d steps a sample and stays inside 3 V", most); check (finite && most < 60, line);
        int its = 0; run (4, fs, 40.0, 10000.0, 4800, 4800, sharp(), {}, nullptr, nullptr, &its); std::snprintf (line, sizeof (line), "40 V at 10 kHz into the sharpest diode: at most %d steps a sample", its); check (its < 60, line);
        Clipper s; s.prepare (fs * 8); SqTube::Oversampler o; o.prepare (8); const int n = 48000 * 4; const auto t0 = std::chrono::steady_clock::now(); double acc = 0;
        for (int i = 0; i < n; ++i) acc += o.process (2.0 * std::sin (i * 0.13), [&] (double x) { return s.process (x); });
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  NOTE speed here: one channel at 8x runs %.1f times faster than real time (%.1f %% of one core); acc %.1f\n", 4.0 / secs, secs / 4.0 * 100.0, acc);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
