// Squelchmoth 1.38 - Voltage Power, the output stage: compared with the same circuit in a circuit simulator.
// The reference numbers come from tests/reference/pentode/make_reference.py (ngspice 42, the published beam / pentode
// equations, 768 kHz steps). Harmonics are in dB re 1 V across the transformer primary for a 1 kHz sine of the given peak volts.
#include "SqTube.h"
#include <complex>
#include <cstdio>
#include <chrono>
using namespace SqTube;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct Ref { double amp; double h[7]; };
struct Gain { double hz, db; };
#include "reference/pentode/reference.inc"

static std::vector<double> run (int os, double amp, double f, double fs, int n, int settle, const Pentode& tube, const PowerCircuit& c = {})
{
    PowerStage s; s.prepare (fs * os, tube, c); Oversampler o; o.prepare (os);
    std::vector<double> y ((size_t) n);
    for (int i = 0; i < settle + n; ++i) { const double v = o.process (amp * std::sin (2.0 * 3.14159265358979323846 * f * i / fs), [&] (double x) { return s.process (x); }); if (i >= settle) y[(size_t) (i - settle)] = v; }
    return y;
}
static double bin (const std::vector<double>& y, int k) { std::complex<double> a = 0; const int n = (int) y.size(); for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -2.0 * 3.14159265358979323846 * k * (double) i / n); return 20 * std::log10 (std::abs (a) / (n / 2.0) + 1e-300); }

int main()
{
    const double fs = 48000.0; char line[500];
    struct Kind { const char* name; Pentode t; const Ref* refs; const Gain* gains; double p, k, s; };
    const Kind kinds[] = { { "Classic", classicBeam(), ref_classic, gain_classic, 338.3576, 23.0108, 342.3906 }, { "Heavy", heavyBeam(), ref_heavy, gain_heavy, 335.2854, 28.2540, 343.4532 }, { "Bold", boldBeam(), ref_bold, gain_bold, 334.2701, 29.1864, 346.7683 } };
    std::printf ("== Output stage against the circuit simulator ==\n");
    for (const auto& kd : kinds)
    {
        { PowerStage s; s.prepare (192000.0, kd.t); std::snprintf (line, sizeof (line), "%s: resting point plate %.3f V, cathode %.4f V, screen %.3f V (simulator: %.3f, %.4f, %.3f); %.1f mA plate, %.1f mA screen", kd.name, s.restingPlate(), s.restingCathode(), s.restingScreen(), kd.p, kd.k, kd.s, s.restingPlateCurrent() * 1000, s.restingScreenCurrent() * 1000);
          check (std::abs (s.restingPlate() - kd.p) < 0.01 && std::abs (s.restingCathode() - kd.k) < 0.002 && std::abs (s.restingScreen() - kd.s) < 0.01, line); }
        for (int os : { 4, 8 })
        {
            double worstFund = 0, worstHarm = 0; int compared = 0;
            for (int r = 0; r < 6; ++r)
            {
                const auto y = run (os, kd.refs[r].amp, 1000.0, fs, 4800, 48000, kd.t);
                for (int k = 1; k <= 7; ++k)
                {
                    const double got = bin (y, k * 100), want = kd.refs[r].h[k - 1];
                    if (k == 1) worstFund = std::max (worstFund, std::abs (got - want));
                    else if (want > kd.refs[r].h[0] - 100.0) { worstHarm = std::max (worstHarm, std::abs (got - want)); ++compared; }
                }
            }
            std::snprintf (line, sizeof (line), "%s, %dx: six drives from 0.5 to 40 V - fundamental within %.3f dB (gate 0.25), %d harmonics up to the 7th within %.3f dB (gate 1)", kd.name, os, worstFund, compared, worstHarm);
            check (worstFund <= 0.25 && worstHarm <= 1.0 && compared >= 25, line);
        }
        double worst = 0;
        for (int g = 0; g < 7; ++g) { const auto y = run (4, 0.05, kd.gains[g].hz, fs, 48000, 48000, kd.t); worst = std::max (worst, std::abs (bin (y, (int) kd.gains[g].hz) - 20 * std::log10 (0.05) - kd.gains[g].db)); }
        std::snprintf (line, sizeof (line), "%s: small-signal response 20 Hz to 20 kHz within %.3f dB of the simulator (gate 0.25)", kd.name, worst); check (worst <= 0.25, line);
        const double g = 20 * std::log10 (PowerStage::smallSignalGain (kd.t, {}));
        std::snprintf (line, sizeof (line), "%s: the gain worked out from the equations (%.2f dB) is the simulator's mid-band gain (%.2f dB), and does not invert", kd.name, g, kd.gains[3].db);
        check (std::abs (g - kd.gains[3].db) < 0.1 && PowerStage::smallSignalGain (kd.t, {}) > 0, line);
    }
    std::printf ("== Aliasing (5 kHz sine; the largest part below 20 kHz that is not a harmonic, against the fundamental) ==\n");
    {
        const double amps[3] = { 10.0, 20.0, 40.0 }; double a[3][3];
        for (int oi = 0; oi < 3; ++oi) for (int ai = 0; ai < 3; ++ai)
        {
            const int os = oi == 0 ? 4 : oi == 1 ? 8 : 16; const auto y = run (os, amps[ai], 5000.0, fs, 48000, 48000, classicBeam());
            double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k));
            a[oi][ai] = bin (y, 5000) - worst;
        }
        for (int oi = 0; oi < 3; ++oi) std::printf ("  NOTE %2dx: %.1f dB down at 10 V, %.1f dB at 20 V (grid conducting), %.1f dB at 40 V (far past sensible drive)\n", oi == 0 ? 4 : oi == 1 ? 8 : 16, a[oi][0], a[oi][1], a[oi][2]);
        std::snprintf (line, sizeof (line), "4x: %.1f dB down at 10 V (full clean swing) - the gate of 100 dB", a[0][0]); check (a[0][0] >= 100.0, line);
        std::snprintf (line, sizeof (line), "8x: %.1f dB down at 20 V (grid conducting)", a[1][1]); check (a[1][1] >= 100.0, line);
    }
    std::printf ("== Behaviour ==\n");
    {
        PowerStage s; s.prepare (192000.0); double worst = 0; for (int i = 0; i < 19200; ++i) worst = std::max (worst, std::abs (s.process (0.0)));
        std::snprintf (line, sizeof (line), "silence in gives silence out from the first sample (largest %.2e V across the primary)", worst); check (worst < 1.0e-4, line);
        bool finite = true; int most = 0; for (int i = 0; i < 19200; ++i) { const double v = s.process (80.0 * std::sin (i * 0.327)); finite = finite && std::isfinite (v) && std::abs (v) < 3000.0; most = std::max (most, s.lastIterations()); }
        std::snprintf (line, sizeof (line), "80 V at 10 kHz (the most the plug-in can send) is solved in at most %d steps a sample", most); check (finite && most < 50, line);
        finite = true; for (int i = 0; i < 19200; ++i) { const double v = s.process (1000.0 * std::sin (i * 0.3)); finite = finite && std::isfinite (v); }
        check (finite, "1000 V (absurd) stays finite");
        for (int i = 0; i < 192000 * 2; ++i) s.process (0.0);
        double after = 0; for (int i = 0; i < 1000; ++i) after = std::max (after, std::abs (s.process (0.0)));
        std::snprintf (line, sizeof (line), "two seconds after that abuse the output has settled back (%.2e V)", after); check (after < 1.0e-3, line);
    }
    {
        // a transformer passes no steady voltage: the mean of the output under hard drive stays at zero
        const auto y = run (4, 20.0, 1000.0, fs, 4800, 96000, classicBeam()); double mean = 0; for (double v : y) mean += v; mean /= (double) y.size();
        std::snprintf (line, sizeof (line), "hard drive: the output has no steady part (mean %.2e V against a swing of hundreds)", mean); check (std::abs (mean) < 0.05, line);
        // less iron: the bass falls away earlier
        PowerCircuit small; small.lp = 5.0; const auto a = run (4, 0.05, 50.0, fs, 48000, 48000, classicBeam()), b = run (4, 0.05, 50.0, fs, 48000, 48000, classicBeam(), small);
        std::snprintf (line, sizeof (line), "a quarter of the iron loses %.1f dB more at 50 Hz", bin (a, 50) - bin (b, 50)); check (bin (a, 50) - bin (b, 50) > 3.0, line);
        // for the record: bass distortion with less iron (this transformer is linear, so little changes)
        const auto c1 = run (4, 10.0, 50.0, fs, 48000, 48000, classicBeam()), c2 = run (4, 10.0, 50.0, fs, 48000, 48000, classicBeam(), small);
        std::printf ("  NOTE 50 Hz at 10 V: 3rd harmonic %.1f dB below the fundamental with 20 H, %.1f dB with 5 H\n", bin (c1, 50) - bin (c1, 150), bin (c2, 50) - bin (c2, 150));
        PowerStage s; s.prepare (192000.0); for (int i = 0; i < 9600; ++i) s.process (5.0 * std::sin (i * 0.0327));
        PowerCircuit c3; c3.rk = 470.0; c3.ra = 6000.0; s.setParts (classicBeam(), c3); bool finite = true; for (int i = 0; i < 192000; ++i) finite = finite && std::isfinite (s.process (5.0 * std::sin (i * 0.0327)));
        double tail = 0; for (int i = 0; i < 192000 * 3; ++i) { const double v = s.process (0.0); if (i > 192000 * 3 - 100) tail = std::max (tail, std::abs (v)); }
        std::snprintf (line, sizeof (line), "changing bias and load while running does not reset the stage; it settles by itself (%.2e V left)", tail); check (finite && tail < 1.0e-3, line);
    }
    {
        PowerStage s; s.prepare (fs * 4); Oversampler o; o.prepare (4); const int n = 48000 * 4; const auto t0 = std::chrono::steady_clock::now(); double acc = 0;
        for (int i = 0; i < n; ++i) acc += o.process (10.0 * std::sin (i * 0.13), [&] (double x) { return s.process (x); });
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  NOTE speed here: one channel at 4x runs %.1f times faster than real time (%.0f %% of one core); acc %.1f\n", 4.0 / secs, secs / 4.0 * 100.0, acc);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
