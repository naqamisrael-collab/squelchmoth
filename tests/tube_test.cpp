// Squelchmoth 1.37 - Voltage, the triode stage: compared with the same circuit in a circuit simulator.
// The reference numbers below were produced by tests/reference/triode/make_reference.py (ngspice 42, the published
// triode equations, 768 kHz steps). Harmonics are in dB re 1 V at the output for a 1 kHz sine of the given peak volts.
#include "SqTube.h"
#include <complex>
#include <cstdio>
#include <chrono>
using namespace SqTube;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct Ref { double amp; double h[7]; };
struct Gain { double hz, db; };
static const Ref refBypassed[] = { { 0.05, { 9.391, -41.929, -87.676, -145.779, -165.971, -172.328, -169.777 } }, { 0.2, { 21.425, -17.837, -51.539, -96.069, -122.156, -143.101, -144.940 } }, { 0.5, { 29.342, -1.866, -27.577, -64.145, -81.925, -117.288, -133.699 } }, { 1.0, { 35.206, 10.370, -9.245, -40.088, -53.411, -75.840, -81.451 } }, { 2.0, { 40.133, 23.545, 9.894, -9.157, -13.511, -10.269, -14.333 } }, { 4.0, { 40.378, 33.526, 12.639, 18.146, 11.781, -9.900, -3.500 } } };
static const Ref refUnbypassed[] = { { 0.05, { 3.442, -59.701, -107.783, -147.111, -175.679, -162.304, -173.054 } }, { 0.2, { 15.482, -35.612, -71.654, -109.400, -147.887, -154.530, -155.443 } }, { 0.5, { 23.435, -19.657, -47.719, -77.435, -103.669, -130.853, -138.276 } }, { 1.0, { 29.433, -7.478, -29.440, -53.021, -73.077, -94.619, -114.242 } }, { 2.0, { 35.357, 5.161, -10.472, -27.605, -41.712, -57.277, -73.359 } }, { 4.0, { 40.380, 21.639, 13.042, -1.492, -8.275, -10.886, -25.158 } } };
static const Gain gainBypassed[] = { { 20.0, 34.206 }, { 50.0, 35.203 }, { 100.0, 35.360 }, { 1000.0, 35.412 }, { 5000.0, 35.401 }, { 10000.0, 35.361 }, { 20000.0, 35.226 } };
static const Gain gainUnbypassed[] = { { 20.0, 28.909 }, { 50.0, 29.370 }, { 100.0, 29.440 }, { 1000.0, 29.463 }, { 5000.0, 29.459 }, { 10000.0, 29.440 }, { 20000.0, 29.375 } };

static std::vector<double> run (int os, double amp, double f, double fs, int n, int settle, bool bypass, const Tube& tube = {})
{
    Circuit c; if (! bypass) c.ck = 0; Stage s; s.prepare (fs * os, tube, c); Oversampler o; o.prepare (os);
    std::vector<double> y ((size_t) n);
    for (int i = 0; i < settle + n; ++i) { const double v = o.process (amp * std::sin (2.0 * 3.14159265358979323846 * f * i / fs), [&] (double x) { return s.process (x); }); if (i >= settle) y[(size_t) (i - settle)] = v; }
    return y;
}
static double bin (const std::vector<double>& y, int k) { std::complex<double> a = 0; const int n = (int) y.size(); for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -2.0 * 3.14159265358979323846 * k * (double) i / n); return 20 * std::log10 (std::abs (a) / (n / 2.0) + 1e-300); }

int main()
{
    const double fs = 48000.0; char line[400];
    std::printf ("== Triode stage against the circuit simulator ==\n");
    { Stage s; s.prepare (192000.0); std::snprintf (line, sizeof (line), "resting point: plate %.2f V, cathode %.4f V (simulator: 202.92 V, 1.4562 V)", s.restingPlate(), s.restingCathode());
      check (std::abs (s.restingPlate() - 202.92) < 0.1 && std::abs (s.restingCathode() - 1.4562) < 0.002, line); }
    for (int variant = 0; variant < 2; ++variant)
    {
        const bool bypass = variant == 0; const Ref* refs = bypass ? refBypassed : refUnbypassed;
        for (int os : { 4, 8 })
        {
            double worstFund = 0, worstHarm = 0; int compared = 0;
            for (int r = 0; r < 6; ++r)
            {
                const auto y = run (os, refs[r].amp, 1000.0, fs, 4800, 48000, bypass);
                for (int k = 1; k <= 7; ++k)
                {
                    const double got = bin (y, k * 100), want = refs[r].h[k - 1];
                    if (k == 1) worstFund = std::max (worstFund, std::abs (got - want));
                    else if (want > refs[r].h[0] - 100.0) { worstHarm = std::max (worstHarm, std::abs (got - want)); ++compared; }     // harmonics more than 100 dB down are below what either can resolve
                }
            }
            std::snprintf (line, sizeof (line), "%s, %dx: six drives from 0.05 to 4 V - fundamental within %.3f dB (gate 0.25), %d harmonics up to the 7th within %.3f dB (gate 1)", bypass ? "bypassed" : "no bypass capacitor", os, worstFund, compared, worstHarm);
            check (worstFund <= 0.25 && worstHarm <= 1.0 && compared >= 15, line);
        }
        const Gain* gains = bypass ? gainBypassed : gainUnbypassed; double worst = 0;
        for (int g = 0; g < 7; ++g) { const auto y = run (4, 0.01, gains[g].hz, fs, 48000, 48000, bypass); worst = std::max (worst, std::abs (bin (y, (int) gains[g].hz) + 40.0 - gains[g].db)); }
        std::snprintf (line, sizeof (line), "%s: small-signal response 20 Hz to 20 kHz within %.3f dB of the simulator (gate 0.25)", bypass ? "bypassed" : "no bypass capacitor", worst);
        check (worst <= 0.25, line);
        std::snprintf (line, sizeof (line), "%s: the gain worked out from the equations (%.2f dB) is the measured mid-band gain (%.2f dB)", bypass ? "bypassed" : "no bypass capacitor", 20 * std::log10 (std::abs (Stage::smallSignalGain ({}, [&] { Circuit c; if (! bypass) c.ck = 0; return c; }()))), gains[3].db);
        check (std::abs (20 * std::log10 (std::abs (Stage::smallSignalGain ({}, [&] { Circuit c; if (! bypass) c.ck = 0; return c; }()))) - gains[3].db) < 0.15 && Stage::smallSignalGain ({}, {}) < 0, line);
    }
    std::printf ("== Aliasing (5 kHz sine; the largest part below 20 kHz that is not a harmonic, against the fundamental) ==\n");
    {
        double a[2][2];
        for (int oi = 0; oi < 2; ++oi) for (int ai = 0; ai < 2; ++ai)
        {
            const int os = oi == 0 ? 4 : 8; const auto y = run (os, ai == 0 ? 2.0 : 4.0, 5000.0, fs, 48000, 48000, true);
            double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k));
            a[oi][ai] = bin (y, 5000) - worst;
        }
        std::snprintf (line, sizeof (line), "4x: %.1f dB down at 2 V (hard drive, grid conducting) - the gate of 100 dB is met there", a[0][0]); check (a[0][0] >= 100.0, line);
        std::snprintf (line, sizeof (line), "8x: %.1f dB down at 2 V and %.1f dB at 4 V (far past any sensible drive)", a[1][0], a[1][1]); check (a[1][0] >= 100.0 && a[1][1] >= 100.0, line);
        std::printf ("  NOTE 4x at 4 V is %.1f dB down: the 100 dB gate is NOT met at 4x for extreme overdrive; 8x meets it.\n", a[0][1]);
    }
    std::printf ("== Behaviour ==\n");
    {
        Oversampler o; o.prepare (4); int peakAt = -1; double peak = 0;
        for (int i = 0; i < 400; ++i) { const double v = o.process (i == 0 ? 1.0 : 0.0, [] (double x) { return x; }); if (std::abs (v) > peak) { peak = std::abs (v); peakAt = i; } }
        std::snprintf (line, sizeof (line), "oversampler: a click comes out %d samples later, as latency() says (%d), at full height (%.4f)", peakAt, o.latency(), peak);
        check (peakAt == o.latency() && o.latency() == 96 && std::abs (peak - 1.0) < 0.03, line);
        Oversampler o8; o8.prepare (8); Oversampler o16; o16.prepare (16); check (o8.latency() == 96 && o16.latency() == 96, "the delay is the same whole number of samples at 4x, 8x and 16x");
    }
    {
        Stage s; s.prepare (192000.0); double worst = 0; for (int i = 0; i < 19200; ++i) worst = std::max (worst, std::abs (s.process (0.0)));
        std::snprintf (line, sizeof (line), "silence in gives silence out from the first sample (largest %.2e V at the plate, against a gain of 59): the stage starts at its resting point", worst); check (worst < 1.0e-4, line);
        bool finite = true; int most = 0; for (int i = 0; i < 19200; ++i) { const double v = s.process (32.0 * std::sin (i * 0.327)); finite = finite && std::isfinite (v) && std::abs (v) < 400.0; most = std::max (most, s.lastIterations()); }
        std::snprintf (line, sizeof (line), "32 V at 10 kHz (the most the plug-in can send) is solved in at most %d steps a sample", most); check (finite && most < 50, line);
        finite = true; for (int i = 0; i < 19200; ++i) { const double v = s.process (500.0 * std::sin (i * 0.3)); finite = finite && std::isfinite (v) && std::abs (v) < 400.0; }
        check (finite, "500 V (absurd) stays finite and inside the supply");
        for (int i = 0; i < 192000 * 2; ++i) s.process (0.0);
        double after = 0; for (int i = 0; i < 1000; ++i) after = std::max (after, std::abs (s.process (0.0)));
        std::snprintf (line, sizeof (line), "two seconds after that abuse the output has settled back (%.2e V)", after); check (after < 1.0e-3, line);
    }
    {
        const double a = Stage::smallSignalGain (highGain(), {}), b = Stage::smallSignalGain (mediumGain(), {});
        std::snprintf (line, sizeof (line), "the medium-gain triode gives less gain than the high-gain one in the same circuit (%.1f against %.1f dB)", 20 * std::log10 (std::abs (b)), 20 * std::log10 (std::abs (a))); check (std::abs (b) < std::abs (a) * 0.5 && std::abs (b) > 3.0, line);
        Stage s; s.prepare (192000.0); for (int i = 0; i < 9600; ++i) s.process (0.2 * std::sin (i * 0.0327));
        Circuit c2; c2.rk = 2700.0; s.setParts (highGain(), c2); bool finite = true; for (int i = 0; i < 192000; ++i) finite = finite && std::isfinite (s.process (0.2 * std::sin (i * 0.0327)));
        double tail = 0; Stage fresh; fresh.prepare (192000.0, highGain(), c2); double fr = 0;
        for (int i = 0; i < 192000 * 3; ++i) { const double v = s.process (0.0), w = fresh.process (0.0); if (i > 192000 * 3 - 100) { tail = std::max (tail, std::abs (v)); fr = std::max (fr, std::abs (w)); } }
        std::snprintf (line, sizeof (line), "changing the cathode resistor while running does not reset the stage; it settles to the new resting point by itself (%.2e V left)", tail); check (finite && tail < 1.0e-3, line);
    }
    {
        Stage s; s.prepare (fs * 4); Oversampler o; o.prepare (4); const int n = 48000 * 4; const auto t0 = std::chrono::steady_clock::now(); double acc = 0;
        for (int i = 0; i < n; ++i) acc += o.process (1.0 * std::sin (i * 0.13), [&] (double x) { return s.process (x); });
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  NOTE speed here: one channel at 4x runs %.1f times faster than real time (%.0f %% of one core); acc %.1f\n", 4.0 / secs, secs / 4.0 * 100.0, acc);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
