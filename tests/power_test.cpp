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
struct CoreRef { double hz, amp; double h[7]; double flux; };
#include "reference/pentode/reference_core.inc"

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
    std::printf ("== The transformer's iron (saturation and hysteresis) against the circuit simulator ==\n");
    {
        Core iron; iron.on = true;
        // switched on from nothing, the tone from 0.6 s, measured from 1.0 s - the same history as the simulator's run
        auto cold = [&] (int os, double rate, double amp, double f, int n, const Core& k, double* fluxAtStart = nullptr, const PowerCircuit& c = {})
        {
            PowerStage s; s.prepare (rate * os, classicBeam(), c, k); s.startCold(); Oversampler o; o.prepare (os);
            const int toneAt = (int) std::lround (0.6 * rate), start = (int) std::lround (1.0 * rate), lag = o.latency(); std::vector<double> y ((size_t) n);
            for (int i = 0; i < start + n + lag; ++i)
            {
                if (i == start && fluxAtStart != nullptr) *fluxAtStart = s.flux();
                const double v = o.process (i >= toneAt ? amp * std::sin (2.0 * 3.14159265358979323846 * f * (i - toneAt) / rate) : 0.0, [&] (double x) { return s.process (x); });
                if (i >= start + lag) y[(size_t) (i - start - lag)] = v;
            }
            return y;
        };
        for (int os : { 4, 8 })
        {
            double worstFund = 0, worstHarm = 0; int compared = 0;
            for (const auto& r : ref_core)
            {
                const int cyc = r.hz < 100 ? 8 : 96, n = (int) std::lround (fs * cyc / r.hz); const auto y = cold (os, fs, r.amp, r.hz, n, iron);
                for (int k = 1; k <= 7; ++k)
                {
                    const double got = bin (y, k * cyc), want = r.h[k - 1];
                    if (k == 1) worstFund = std::max (worstFund, std::abs (got - want));
                    else if (want > r.h[0] - 100.0) { worstHarm = std::max (worstHarm, std::abs (got - want)); ++compared; }
                }
            }
            std::snprintf (line, sizeof (line), "%dx: 50 Hz at 1, 5, 10, 20 V and 1 kHz at 2, 10, 20 V - fundamental within %.3f dB (gate 0.25), %d harmonics up to the 7th within %.3f dB (gate 1)", os, worstFund, compared, worstHarm);
            check (worstFund <= 0.25 && worstHarm <= 1.0 && compared >= 38, line);
        }
        {
            // the iron's own state, at the simulator's rate with no oversampler in the way
            double worst = 0; for (const auto& r : ref_core) { double fl = 0; cold (1, 384000.0, r.amp, r.hz, 16, iron, &fl); worst = std::max (worst, std::abs (fl - r.flux)); }
            std::snprintf (line, sizeof (line), "the flux in the iron one second after switch-on agrees with the simulator's in all seven runs (largest difference %.2e Vs of about 1.5)", worst); check (worst < 2.0e-3, line);
        }
        std::printf ("== What the iron does ==\n");
        {
            PowerStage s; s.prepare (192000.0, classicBeam(), {}, iron); double worst = 0; for (int i = 0; i < 19200; ++i) worst = std::max (worst, std::abs (s.process (0.0)));
            std::snprintf (line, sizeof (line), "with the iron, silence in still gives silence out from the first sample (largest %.2e V); it rests at %.0f %% of its saturation flux because of the tube's steady current", worst, 100.0 * s.flux() / s.fluxAtSaturation());
            check (worst < 1.0e-4 && s.flux() > 0.2 * s.fluxAtSaturation() && s.flux() < 0.8 * s.fluxAtSaturation(), line);
            // memory: after a burst of bass that ends, the iron settles at a different flux than before, and stays there
            const double before = s.flux(); for (int i = 0; i < 192000; ++i) s.process (30.0 * std::sin (2.0 * 3.14159265358979323846 * 30.0 * i / 192000.0) * (i < 96000 ? 1.0 : std::max (0.0, 1.0 - (i - 96000) / 3200.0)));
            for (int i = 0; i < 192000 * 2; ++i) s.process (0.0); const double after = s.flux(); for (int i = 0; i < 192000 * 2; ++i) s.process (0.0); const double later = s.flux();
            double quiet = 0; for (int i = 0; i < 1000; ++i) quiet = std::max (quiet, std::abs (s.process (0.0)));
            std::snprintf (line, sizeof (line), "memory: after a bass burst the iron rests at %.4f Vs instead of %.4f, is still there two seconds later (%.4f), and the output is quiet (%.1e V)", after, before, later, quiet);
            check (std::abs (after - before) > 1.0e-3 && std::abs (later - after) < 0.1 * std::abs (after - before) && quiet < 1.0e-3, line);
        }
        {
            // saturation is a bass matter: against the plain inductance of the same size, the iron adds upper harmonics at 50 Hz and next to nothing at 1 kHz
            PowerCircuit lin; lin.lp = 22.0; Core off;
            auto upper = [&] (const std::vector<double>& y, int cyc) { double e = 0; for (int k = 4; k <= 9; ++k) e += std::pow (10.0, bin (y, k * cyc) / 10.0); return 10 * std::log10 (e) - bin (y, cyc); };
            const auto a50 = cold (4, fs, 5.0, 50.0, 7680, iron), b50 = cold (4, fs, 5.0, 50.0, 7680, off, nullptr, lin), a1k = cold (4, fs, 10.0, 1000.0, 4608, iron), b1k = cold (4, fs, 10.0, 1000.0, 4608, off, nullptr, lin);
            double d1k = 0; for (int k = 1; k <= 5; ++k) d1k = std::max (d1k, std::abs (bin (a1k, k * 96) - bin (b1k, k * 96)));
            std::snprintf (line, sizeof (line), "50 Hz at 5 V: harmonics 4 to 9 are %.1f dB below the fundamental with the iron, %.1f dB with a plain inductance; at 1 kHz the first five harmonics differ by at most %.2f dB", -upper (a50, 8), -upper (b50, 8), d1k);
            check (upper (a50, 8) > upper (b50, 8) + 3.0 && d1k < 0.5, line);
            const auto small = cold (4, fs, 10.0, 50.0, 7680, Core::sized (0.3)), big = cold (4, fs, 10.0, 50.0, 7680, Core::sized (1.8));
            std::snprintf (line, sizeof (line), "less iron saturates sooner: 50 Hz at 10 V, harmonics 4 to 9 at %.1f dB below the fundamental with a third of the iron, %.1f dB with nearly twice", -upper (small, 8), -upper (big, 8));
            check (upper (small, 8) > upper (big, 8) + 6.0, line);
            // for the record: how much the bass gain depends on level
            const auto q = cold (4, fs, 0.05, 30.0, 12800, iron), l = cold (4, fs, 5.0, 30.0, 12800, iron);
            std::printf ("  NOTE 30 Hz: gain %.2f dB at 0.05 V and %.2f dB at 5 V (plain inductance: the same at both)\n", bin (q, 8) - 20 * std::log10 (0.05), bin (l, 8) - 20 * std::log10 (5.0));
        }
        {
            double a[2]; int ix = 0;
            for (int os : { 4, 8 })
            {
                PowerStage s; s.prepare (fs * os, classicBeam(), {}, iron); Oversampler o; o.prepare (os); std::vector<double> y (48000);
                for (int i = 0; i < 96000; ++i) { const double v = o.process (20.0 * std::sin (2.0 * 3.14159265358979323846 * 5000.0 * i / fs), [&] (double x) { return s.process (x); }); if (i >= 48000) y[(size_t) (i - 48000)] = v; }
                double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k)); a[ix++] = bin (y, 5000) - worst;
            }
            std::snprintf (line, sizeof (line), "aliasing with the iron, 5 kHz at 20 V: %.1f dB down at 4x, %.1f dB at 8x (gate 100)", a[0], a[1]); check (a[0] >= 100.0 && a[1] >= 100.0, line);
            PowerStage s; s.prepare (192000.0, classicBeam(), {}, iron); bool finite = true; int most = 0;
            for (int i = 0; i < 192000; ++i) { const double v = s.process (80.0 * std::sin (2.0 * 3.14159265358979323846 * 30.0 * i / 192000.0)); finite = finite && std::isfinite (v) && std::abs (v) < 5000.0; most = std::max (most, s.lastIterations()); }
            std::snprintf (line, sizeof (line), "80 V at 30 Hz (the iron driven far into saturation) is solved in at most %d steps a sample and stays finite", most); check (finite && most < 50, line);
            for (int i = 0; i < 192000; ++i) finite = finite && std::isfinite (s.process (1000.0 * std::sin (i * 0.01))); check (finite, "1000 V (absurd) stays finite with the iron");
            PowerStage s2; s2.prepare (192000.0, classicBeam(), {}, iron); for (int i = 0; i < 19200; ++i) s2.process (10.0 * std::sin (i * 0.002));
            s2.setParts (classicBeam(), {}, Core::sized (0.5)); finite = true; for (int i = 0; i < 192000 * 3; ++i) finite = finite && std::isfinite (s2.process (i < 96000 ? 10.0 * std::sin (i * 0.002) : 0.0));
            double tail = 0; for (int i = 0; i < 1000; ++i) tail = std::max (tail, std::abs (s2.process (0.0)));
            std::snprintf (line, sizeof (line), "changing the size of the iron while running keeps the flux and settles by itself (%.1e V left)", tail); check (finite && tail < 1.0e-3, line);
        }
        {
            PowerStage s; s.prepare (fs * 4, classicBeam(), {}, iron); Oversampler o; o.prepare (4); const int n = 48000 * 4; const auto t0 = std::chrono::steady_clock::now(); double acc = 0;
            for (int i = 0; i < n; ++i) acc += o.process (10.0 * std::sin (i * 0.013), [&] (double x) { return s.process (x); });
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  NOTE speed with the iron: one channel at 4x runs %.1f times faster than real time (%.0f %% of one core); acc %.1f\n", 4.0 / secs, secs / 4.0 * 100.0, acc);
        }
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
