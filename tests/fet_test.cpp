// Squelchmoth 1.43 - Voltage Comp: the FET gain cell and the compressor loop against the same circuit in a circuit
// simulator (tests/reference/fet/make_reference.py, ngspice 42, its own junction-FET model).
#include "SqFet.h"
#include "SqTube.h"
#include <complex>
#include <cstdio>
#include <chrono>
using namespace SqFet;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct CellRef { double control, amp; double h[5]; };
struct LoopRef { double t, cap, peak; };
#include "reference/fet/reference.inc"
static double bin (const std::vector<double>& y, double cyc) { std::complex<double> a = 0; const int n = (int) y.size(); for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -2.0 * 3.14159265358979323846 * cyc * (double) i / n); return 20 * std::log10 (std::abs (a) / (n / 2.0) + 1e-300); }

int main()
{
    char line[700]; const double pi = 3.14159265358979323846;
    std::printf ("== The FET ==\n");
    {
        Fet f; double i, a, b; channel (f, 0.0, 10.0, i, a, b);
        std::snprintf (line, sizeof (line), "gate at the source, 10 V across: %.2f mA (beta Vto^2 (1 + lambda 10) = %.2f mA); below pinch-off nothing flows", i * 1000, f.beta * f.vto * f.vto * (1 + f.lambda * 10) * 1000);
        double off; channel (f, -3.2, 1.0, off, a, b); check (std::abs (i - f.beta * f.vto * f.vto * (1 + f.lambda * 10)) < 1e-12 && off == 0.0, line);
        double worst = 0; for (double vg = -4.0; vg <= 0.0; vg += 0.37) for (double vd = -2.0; vd <= 2.0; vd += 0.11)
        { double i0, a0, b0, i1, a1, b1, i2, a2, b2; fet (f, vg, vd, i0, a0, b0); fet (f, vg, vd + 1e-6, i1, a1, b1); fet (f, vg + 1e-6, vd, i2, a2, b2);
          worst = std::max ({ worst, std::abs ((i1 - i0) / 1e-6 - a0), std::abs ((i2 - i0) / 1e-6 - b0) }); }
        std::snprintf (line, sizeof (line), "with its source resistance: the slopes it reports are the slopes it has (within %.1e A/V over gate -4 to 0 V, drain -2 to 2 V)", worst); check (worst < 1e-6, line);
        double p, q, r2, s2; fet (f, -1.0, 0.3, p, a, b); fet (f, -1.0 - 0.3, -0.3, q, r2, s2);
        std::snprintf (line, sizeof (line), "it conducts both ways: +0.3 V gives %.4f mA, -0.3 V (gate held the same against the other end) gives %.4f mA", p * 1000, q * 1000); check (p > 0 && q < 0, line);
    }
    std::printf ("== The gain cell against the circuit simulator (control fixed, 1 kHz) ==\n");
    {
        double worstFund = 0, worstHarm = 0; int compared = 0; const double fs = 768000.0;
        for (const auto& r : ref_cell)
        {
            GainCell g; g.prepare(); const int n = (int) std::lround (0.096 * fs), settle = (int) (0.02 * fs); std::vector<double> y ((size_t) n);
            for (int i = 0; i < settle + n; ++i) { const double v = g.process (r.amp * std::sin (2.0 * pi * 1000.0 * i / fs), r.control); if (i >= settle) y[(size_t) (i - settle)] = v; }
            for (int k = 1; k <= 5; ++k) { const double got = bin (y, k * 96.0), want = r.h[k - 1]; if (k == 1) worstFund = std::max (worstFund, std::abs (got - want)); else if (want > r.h[0] - 100.0) { worstHarm = std::max (worstHarm, std::abs (got - want)); ++compared; } }
        }
        std::snprintf (line, sizeof (line), "five control voltages from just open to hard on, 0.01 to 1 V in: level within %.3f dB (gate 0.25), %d harmonics up to the 5th within %.3f dB (gate 1)", worstFund, compared, worstHarm);
        check (worstFund <= 0.25 && worstHarm <= 1.0 && compared >= 20, line);
        GainCell g; g.prepare(); double worstGain = 0; for (const auto& r : ref_cell) if (r.amp == 0.01 && r.control > -6.25) worstGain = std::max (worstGain, std::abs (20 * std::log10 (g.gainAt (r.control)) - (r.h[0] + 40.0)));
        std::snprintf (line, sizeof (line), "the level worked out from the FET's slopes is the level the simulator shows for a quiet tone (within %.3f dB; not at the very edge of pinch-off, where even a quiet tone opens the FET on its own peaks)", worstGain); check (worstGain < 0.1, line);
        // half the node fed back to the gate: without it the 2nd harmonic is far higher
        auto second = [&] (bool feedback) { Fet f; Cell c; double v = 0, cur = 0; const int n = 73728; std::vector<double> y ((size_t) n);
            for (int i = 0; i < n * 2; ++i) { const double vin = 0.1 * std::sin (2.0 * pi * 1000.0 * i / fs); double x = v; for (int it = 0; it < 40; ++it) { double id, a, b; fet (f, feedback ? 0.5 * (-6.2 + x) : -3.1, x, id, a, b, &cur); const double gg = (x - vin) / c.r + id, dg = 1.0 / c.r + a + (feedback ? 0.5 * b : 0.0), st = gg / dg; x -= st; if (std::abs (st) < 1e-13) break; } v = x; if (i >= n) y[(size_t) (i - n)] = x; }
            return bin (y, 192.0) - bin (y, 96.0); };
        const double with = second (true), without = second (false);
        std::snprintf (line, sizeof (line), "feeding half the node back to the gate takes the 2nd harmonic from %.1f to %.1f dB below the tone", -without, -with); check (with < without - 20.0, line);
    }
    std::printf ("== The whole loop against the circuit simulator (a tone at 0.02 V that jumps to 0.8 V from 0.2 to 0.5 s) ==\n");
    for (int which = 0; which < 3; ++which)
    {
        const double fs = which == 0 ? 384000.0 : which == 1 ? 192000.0 : 768000.0; Sidechain sc; sc.depth = 0.2; Compressor c; c.prepare (fs, {}, {}, sc);
        const int n = (int) (1.2 * fs); std::vector<double> node ((size_t) n), cap ((size_t) n);
        for (int i = 0; i < n; ++i) { const double t = i / fs; node[(size_t) i] = c.process ((t > 0.2 && t < 0.5 ? 0.8 : 0.02) * std::sin (2.0 * pi * 1000.0 * t)); cap[(size_t) i] = c.capacitor(); }
        double worstCap = 0, worstPeak = 0;
        for (const auto& r : ref_loop)
        {
            const int at = (int) std::lround (r.t * fs); worstCap = std::max (worstCap, std::abs (cap[(size_t) at] - r.cap));
            double pk = 0; for (int i = at - (int) (0.002 * fs) + 1; i <= at; ++i) pk = std::max (pk, std::abs (node[(size_t) i])); worstPeak = std::max (worstPeak, std::abs (20 * std::log10 (pk / r.peak)));
        }
        std::snprintf (line, sizeof (line), "run at %.0f kHz: the capacitor's voltage at thirteen moments through attack, hold and release within %.4f V of the simulator's (it reaches 0.80), and the level at the node within %.3f dB", fs / 1000, worstCap, worstPeak);
        check (worstCap < 0.02 && worstPeak < 0.5, line);
    }
    std::printf ("== What it does ==\n");
    {
        const double fs = 192000.0;
        auto curve = [&] (double depth, double inDb) { Sidechain s; s.depth = depth; Compressor c; c.prepare (fs, {}, {}, s); double pk = 0; const double amp = 0.1 * std::pow (10.0, inDb / 20.0); for (int i = 0; i < (int) (fs * 2.5); ++i) { const double v = c.process (amp * std::sin (2.0 * pi * 1000.0 * i / fs)); if (i > fs * 2.4) pk = std::max (pk, std::abs (v)); } return 20 * std::log10 (pk / 0.1); };
        std::snprintf (line, sizeof (line), "below the threshold it does nothing: a tone 18 dB under comes back within %.4f dB", std::abs (curve (0.2, -30.0) + 30.0)); check (std::abs (curve (0.2, -30.0) + 30.0) < 0.001, line);
        char detail[300] = ""; size_t used = 0; bool rising = true; double last = 0;
        for (double depth : { 0.05, 0.2, 0.6, 2.0 }) { const double ratio = 12.0 / (curve (depth, 0.0) - curve (depth, -12.0)); used += (size_t) std::snprintf (detail + used, sizeof (detail) - used, " %.2f -> %.1f : 1;", depth, ratio); rising = rising && ratio > last; last = ratio; }
        std::snprintf (line, sizeof (line), "the ratio control (volts at the gate per volt on the capacitor), measured over the first 12 dB above the threshold:%s", detail); check (rising && last > 10.0, line);
        // attack and release take the times set
        auto run = [&] (double attack, double release, double& capEarly, double& fall) { Sidechain s; s.depth = 0.2; s.attack = attack; s.release = release; Compressor c; c.prepare (fs, {}, {}, s); double top = 0; fall = -1; capEarly = 0;
            for (int i = 0; i < (int) (fs * 3.0); ++i) { const double t = i / fs; c.process ((t < 1.0 ? 0.2 : 0.0) * std::sin (2.0 * pi * 1000.0 * t + 1.5707963)); if (i == (int) (0.0002 * fs)) capEarly = c.capacitor(); if (t < 1.0) top = c.capacitor();
                if (t >= 1.0 && fall < 0 && c.capacitor() < top / 2.718281828) fall = t - 1.0; } };
        double e1, f1, e2, f2, e3, f3; run (0.00005, 0.1, e1, f1); run (0.0008, 0.8, e2, f2); run (0.0002, 0.3, e3, f3);
        std::snprintf (line, sizeof (line), "release set to 0.1, 0.3 and 0.8 s: the capacitor falls to 1/e in %.3f, %.3f and %.3f s; 0.2 ms after a tone starts it holds %.3f V with a 50 us attack, %.3f V with 200 us, %.3f V with 800 us", f1, f3, f2, e1, e3, e2);
        check (std::abs (f1 - 0.1) < 0.01 && std::abs (f3 - 0.3) < 0.02 && std::abs (f2 - 0.8) < 0.05 && e1 > e3 * 1.2 && e3 > e2 * 1.2, line);
    }
    std::printf ("== Two channels under one sidechain ==\n");
    {
        const double fs = 192000.0; Sidechain sc; sc.depth = 0.2; Compressor mono; mono.prepare (fs, {}, {}, sc); StereoCompressor st; st.prepare (fs, {}, {}, {}, sc); double worst = 0;
        for (int i = 0; i < 96000; ++i) { const double v = 0.5 * std::sin (2.0 * pi * 1000.0 * i / fs); worst = std::max (worst, std::abs (mono.process (v) - st.left (v, i % 4))); }
        std::snprintf (line, sizeof (line), "with one channel it is the single compressor exactly (largest difference %.1e V)", worst); check (worst < 1e-12, line);
        StereoCompressor s2; s2.prepare (fs, {}, {}, {}, sc); double pl = 0, pr = 0; for (int i = 0; i < 192000; ++i) { const double l = s2.left (0.8 * std::sin (2.0 * pi * 1000.0 * i / fs), i % 4), r = s2.right (0.008 * std::sin (2.0 * pi * 700.0 * i / fs), i % 4); if (i > 150000) { pl = std::max (pl, std::abs (l)); pr = std::max (pr, std::abs (r)); } }
        const double gl = 20 * std::log10 (pl / 0.8), gr = 20 * std::log10 (pr / 0.008);
        std::snprintf (line, sizeof (line), "a loud left and a quiet right are turned down alike (%.2f and %.2f dB): the picture between the speakers holds still", gl, gr); check (gl < -6.0 && std::abs (gl - gr) < 0.5, line);
        StereoCompressor s3; s3.prepare (fs, {}, {}, {}, sc); double pr3 = 0; for (int i = 0; i < 192000; ++i) { s3.left (0.004 * std::sin (2.0 * pi * 1000.0 * i / fs), i % 4); const double r = s3.right (0.8 * std::sin (2.0 * pi * 700.0 * i / fs), i % 4); if (i > 150000) pr3 = std::max (pr3, std::abs (r)); }
        std::snprintf (line, sizeof (line), "a loud right alone is compressed too (%.2f dB)", 20 * std::log10 (pr3 / 0.8)); check (20 * std::log10 (pr3 / 0.8) < -6.0, line);
        char detail[200] = ""; size_t used = 0; bool ok = true; const double want[4] = { 4, 8, 12, 20 };
        for (int ix = 0; ix < 4; ++ix) { auto lvl = [&] (double inDb) { Sidechain s; s.depth = depthForRatio (ix); Compressor c; c.prepare (fs, {}, {}, s); double pk = 0; const double amp = 0.1 * std::pow (10.0, inDb / 20.0); for (int i = 0; i < (int) (fs * 2.5); ++i) { const double v = c.process (amp * std::sin (2.0 * pi * 1000.0 * i / fs)); if (i > fs * 2.4) pk = std::max (pk, std::abs (v)); } return 20 * std::log10 (pk / 0.1); };
            const double r = 12.0 / (lvl (0.0) - lvl (-12.0)), far24 = 12.0 / (lvl (24.0) - lvl (12.0)); ok = ok && std::abs (r - want[ix]) < 0.06 * want[ix]; used += (size_t) std::snprintf (detail + used, sizeof (detail) - used, " %.1f (then %.1f far above)", r, far24); }
        std::snprintf (line, sizeof (line), "the four ratio steps measure%s : 1 over the first 12 dB above the threshold - and ease off far above it, as a feedback compressor does", detail); check (ok, line);
    }
    std::printf ("== Aliasing and distortion while compressing ==\n");
    {
        const double fs = 48000.0;
        double a[3][2];
        for (int oi = 0; oi < 3; ++oi) for (int li = 0; li < 2; ++li)
        {
            const int os = oi == 0 ? 4 : oi == 1 ? 8 : 16; const double amp = li == 0 ? 0.4 : 1.6; Sidechain s; s.depth = 0.2; Compressor c; c.prepare (fs * os, {}, {}, s); SqTube::Oversampler o; o.prepare (os); std::vector<double> y (48000);
            for (int i = 0; i < 48000 * 3; ++i) { const double v = o.process (amp * std::sin (2.0 * pi * 5000.0 * i / fs), [&] (double x) { return c.process (x); }); if (i >= 96000) y[(size_t) (i - 96000)] = v; }
            double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k)); a[oi][li] = bin (y, 5000) - worst;
            if (oi == 0) std::printf ("  NOTE 5 kHz %s over the reference level, compressing: 2nd harmonic %.1f, 3rd %.1f dB below the tone\n", li == 0 ? "12 dB" : "24 dB", -(bin (y, 10000) - bin (y, 5000)), -(bin (y, 15000) - bin (y, 5000)));
        }
        std::printf ("  NOTE the 100 dB gate is NOT met at 4x or 8x while compressing (the sidechain's rectifier makes a control voltage with sharp corners): 4x %.1f / %.1f dB, 8x %.1f / %.1f dB down at 12 / 24 dB over the reference level. The plug-in starts at 8x.\n", a[0][0], a[0][1], a[1][0], a[1][1]);
        std::snprintf (line, sizeof (line), "5 kHz, everything below 20 kHz that is not a harmonic: 16x %.1f dB down 12 dB over the reference level and %.1f dB 24 dB over (gate 100); 8x at least %.0f dB", a[2][0], a[2][1], std::min (a[1][0], a[1][1]));
        check (a[2][0] >= 100.0 && a[2][1] >= 100.0 && a[1][0] >= 85.0 && a[1][1] >= 85.0, line);
    }
    std::printf ("== Behaviour ==\n");
    {
        Compressor c; c.prepare (192000.0); double worst = 0; for (int i = 0; i < 1000; ++i) worst = std::max (worst, std::abs (c.process (0.0))); check (worst == 0.0, "silence in, silence out");
        bool finite = true; for (int i = 0; i < 192000; ++i) { const double v = c.process (50.0 * std::sin (i * 0.7)); finite = finite && std::isfinite (v) && std::abs (v) < 60.0; } check (finite, "50 V of near-Nyquist garbage stays finite");
        for (int i = 0; i < 192000 * 4; ++i) c.process (0.0); std::snprintf (line, sizeof (line), "four seconds later it has let go again (%.2f dB of reduction left)", -c.reductionDb()); check (c.reductionDb() > -0.5, line);
        Compressor s; s.prepare (48000.0 * 4); SqTube::Oversampler o; o.prepare (4); const int n = 48000 * 4; const auto t0 = std::chrono::steady_clock::now(); double acc = 0;
        for (int i = 0; i < n; ++i) acc += o.process (0.5 * std::sin (i * 0.13), [&] (double x) { return s.process (x); });
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  NOTE speed here: one channel at 4x runs %.1f times faster than real time (%.1f %% of one core); acc %.2f\n", 4.0 / secs, secs / 4.0 * 100.0, acc);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
