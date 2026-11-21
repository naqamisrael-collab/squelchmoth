// Squelchmoth 1.41 - Voltage Console: the line transformer against the same circuit in a circuit simulator
// (tests/reference/console/make_reference.py, ngspice 42), and the bus crosstalk against its definition.
#include "SqConsole.h"
#include <cstdio>
#include <chrono>
using namespace SqTube;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct LineRef { double hz, amp; double h[7]; double flux; };
#include "reference/console/reference.inc"
static double bin (const std::vector<double>& y, double cyc) { std::complex<double> a = 0; const int n = (int) y.size(); for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -2.0 * 3.14159265358979323846 * cyc * (double) i / n); return 20 * std::log10 (std::abs (a) / (n / 2.0) + 1e-300); }

// the tone runs from the first sample; measured from 1.0 s, as in the simulator
static std::vector<double> run (int os, double rate, double amp, double hz, int n, const Core& iron = lineCore(), const LineCircuit& c = {}, double* peakFlux = nullptr)
{
    LineTransformer t; t.prepare (rate * os, iron, c); Oversampler o; o.prepare (os); const int start = (int) std::lround (rate), lag = o.latency(); std::vector<double> y ((size_t) n); double pk = 0;
    for (int i = 0; i < start + n + lag; ++i)
    {
        const double v = o.process (amp * std::sin (2.0 * 3.14159265358979323846 * hz * i / rate), [&] (double x) { return t.process (x); });
        if (i >= start + lag) { y[(size_t) (i - start - lag)] = v; pk = std::max (pk, std::abs (t.flux())); }
    }
    if (peakFlux != nullptr) *peakFlux = pk; return y;
}

int main()
{
    const double fs = 48000.0; char line[600];
    std::printf ("== The line transformer against the circuit simulator ==\n");
    for (int os : { 1, 0, 4 })       // 1: the simulator's own rate; 0: 48 kHz with no oversampling, as the plug-in runs; 4: 4x of 48 kHz
    {
        const double rate = os == 1 ? 384000.0 : fs; double worstFund = 0, worstOdd = 0, worstEven = 0, worstFlux = 0; int odd = 0, even = 0;
        for (const auto& r : ref_line)
        {
            const int cyc = r.hz < 30 ? 8 : r.hz < 100 ? 16 : 96, n = (int) std::lround (rate * cyc / r.hz); double fl = 0; const auto y = run (std::max (1, os), rate, r.amp, r.hz, n, lineCore(), {}, &fl);
            worstFlux = std::max (worstFlux, std::abs (fl - r.flux) / r.flux);
            for (int k = 1; k <= 7; ++k)
            {
                const double got = bin (y, k * cyc), want = r.h[k - 1];
                if (k == 1) worstFund = std::max (worstFund, std::abs (got - want));
                else if (want > r.h[0] - 100.0) { if (k % 2 == 1) { worstOdd = std::max (worstOdd, std::abs (got - want)); ++odd; } else { worstEven = std::max (worstEven, std::abs (got - want)); ++even; } }
            }
        }
        if (os == 1) std::snprintf (line, sizeof (line), "at the simulator's own rate: 20, 50 and 1000 Hz at 0.1 to 15 V - fundamental within %.3f dB (gate 0.25), %d odd harmonics within %.3f dB and %d even ones within %.3f dB (gate 1), peak flux within %.2f %%", worstFund, odd, worstOdd, even, worstEven, worstFlux * 100);
        else std::snprintf (line, sizeof (line), "%s at 48 kHz: fundamental within %.3f dB, %d odd harmonics within %.3f dB, %d even ones within %.3f dB, peak flux within %.2f %%", os == 0 ? "no oversampling (as the plug-in runs)" : "4x", worstFund, odd, worstOdd, even, worstEven, worstFlux * 100);
        check (worstFund <= 0.25 && worstOdd <= 1.0 && worstEven <= 1.0 && odd >= 20 && worstFlux < 0.02, line);
    }
    std::printf ("== What the transformer does ==\n");
    {
        const auto a = run (4, fs, 5.0, 20.0, 19200), b = run (4, fs, 5.0, 1000.0, 4800);
        std::snprintf (line, sizeof (line), "distortion is a bass matter: 3rd harmonic %.1f dB below the tone at 20 Hz, %.1f dB at 1 kHz, same level", -(bin (a, 24) - bin (a, 8)), -(bin (b, 300) - bin (b, 100)));
        check (bin (a, 24) - bin (a, 8) > bin (b, 300) - bin (b, 100) + 30.0, line);
        const auto small = run (4, fs, 5.0, 50.0, 15360, SqConsole::ironFor (0.0)), big = run (4, fs, 5.0, 50.0, 15360, SqConsole::ironFor (100.0));
        std::snprintf (line, sizeof (line), "size: at 50 Hz a small transformer's 3rd harmonic is %.1f dB down, a large one's %.1f dB", -(bin (small, 48) - bin (small, 16)), -(bin (big, 48) - bin (big, 16)));
        check (bin (small, 48) - bin (small, 16) > bin (big, 48) - bin (big, 16) + 10.0, line);
        // small signals: the pass band sits at the divider's level and the bass corner where the small-signal inductance puts it
        const LineCircuit c; const double pass = 20 * std::log10 (LineTransformer::passGain (c)), corner = LineTransformer::bassCorner (lineCore(), c);
        const auto mid = run (4, fs, 0.001, 1000.0, 4800); 
        LineTransformer t; t.prepare (192000.0); const int n = 192000 * 4; std::vector<double> y ((size_t) n); for (int i = 0; i < n * 2; ++i) { const double v = t.process (1.0e-5 * std::sin (2.0 * 3.14159265358979323846 * corner * i / 192000.0)); if (i >= n) y[(size_t) (i - n)] = v; }
        const double atCorner = bin (y, corner * 4.0) - 20 * std::log10 (1.0e-5);
        std::snprintf (line, sizeof (line), "quiet signals: pass band %.3f dB (the two resistances give %.3f); the bass is %.2f dB down at %.2f Hz, where the iron's small-signal inductance (%.1f H) puts the corner", bin (mid, 100) - 20 * std::log10 (0.001), pass, pass - atCorner, corner, LineTransformer::smallInductance (lineCore()));
        check (std::abs (bin (mid, 100) - 20 * std::log10 (0.001) - pass) < 0.05 && std::abs (pass - atCorner - 3.01) < 0.3, line);
        std::printf ("  NOTE bass corner by size: small %.2f Hz, reference %.2f Hz, large %.2f Hz (loud bass sees more inductance and reaches lower)\n", LineTransformer::bassCorner (SqConsole::ironFor (0.0), c), corner, LineTransformer::bassCorner (SqConsole::ironFor (100.0), c));
    }
    std::printf ("== Aliasing (5 kHz sine; the largest part below 20 kHz that is not a harmonic, against the fundamental) ==\n");
    {
        double a[3]; int ix = 0;
        for (int os : { 1, 2, 4 }) { LineTransformer t; t.prepare (fs * os); Oversampler o; o.prepare (os); std::vector<double> y (48000); for (int i = 0; i < 96000; ++i) { const double v = o.process (40.0 * std::sin (2.0 * 3.14159265358979323846 * 5000.0 * i / fs), [&] (double x) { return t.process (x); }); if (i >= 48000) y[(size_t) (i - 48000)] = v; }
          double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k)); a[ix++] = bin (y, 5000) - worst; }
        std::snprintf (line, sizeof (line), "5 kHz at 40 V (the most the plug-in can send): %.1f dB down with no oversampling, %.1f at 2x, %.1f at 4x (gate 100) - so the plug-in runs without, and adds no delay", a[0], a[1], a[2]); check (a[0] >= 100.0, line);
        { LineTransformer t; t.prepare (fs); std::vector<double> y (48000); for (int i = 0; i < 96000; ++i) { const double v = t.process (40.0 * std::sin (2.0 * 3.14159265358979323846 * 9000.0 * i / fs)); if (i >= 48000) y[(size_t) (i - 48000)] = v; }
          double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 9000 != 0) worst = std::max (worst, bin (y, k)); std::snprintf (line, sizeof (line), "9 kHz at 40 V, no oversampling: %.1f dB down", bin (y, 9000) - worst); check (bin (y, 9000) - worst >= 100.0, line); }
    }
    std::printf ("== Bus crosstalk ==\n");
    {
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            SqConsole::Crosstalk x; x.prepare (rate); x.set (-80.0); double got[2]; int ix = 0;
            for (double hz : { 1000.0, 10000.0 })
            {
                const int n = (int) rate; std::vector<double> r ((size_t) n); x.reset();
                for (int i = 0; i < n * 2; ++i) { double l = std::sin (2.0 * 3.14159265358979323846 * hz * i / rate), rr = 0.0; x.process (l, rr); if (i >= n) r[(size_t) (i - n)] = rr; }
                got[ix++] = bin (r, hz);
            }
            std::snprintf (line, sizeof (line), "%.0f Hz rate: left alone, set to -80 dB: the right carries %.2f dB at 1 kHz and %.2f dB at 10 kHz (stray capacitance: rising with frequency)", rate, got[0], got[1]);
            check (std::abs (got[0] + 80.0) < 0.05 && got[1] > got[0] + 16.0 && got[1] < got[0] + 22.0, line);
        }
        SqConsole::Crosstalk x; x.prepare (48000.0); x.set (-120.0); double l = 1.0, r = 0.0; x.process (l, r); check (l == 1.0 && r == 0.0, "off is off: not one bit crosses");
        SqConsole::Crosstalk s; s.prepare (48000.0); s.set (-70.0); double worst = 0; for (int i = 0; i < 48000; ++i) { double a = std::sin (i * 0.1), b = a; s.process (a, b); worst = std::max (worst, std::abs (a - b)); }
        check (worst < 1e-15, "a centred (mono) signal stays centred");
    }
    std::printf ("== Behaviour ==\n");
    {
        LineTransformer t; t.prepare (192000.0); double worst = 0; for (int i = 0; i < 19200; ++i) worst = std::max (worst, std::abs (t.process (0.0))); check (worst == 0.0, "silence in, silence out");
        bool finite = true; int most = 0; for (int i = 0; i < 192000; ++i) { const double v = t.process (200.0 * std::sin (2.0 * 3.14159265358979323846 * 20.0 * i / 192000.0)); finite = finite && std::isfinite (v) && std::abs (v) < 400.0; most = std::max (most, t.lastIterations()); }
        std::snprintf (line, sizeof (line), "200 V at 20 Hz (the iron far into saturation) is solved in at most %d steps a sample and stays finite", most); check (finite && most < 40, line);
        for (int i = 0; i < 192000 * 2; ++i) t.process (0.0); double tail = 0; for (int i = 0; i < 1000; ++i) tail = std::max (tail, std::abs (t.process (0.0)));
        int idleAt = -1; for (int i = 0; i < 192000 * 60 && idleAt < 0; ++i) { t.process (0.0); if (t.lastIterations() == 0) idleAt = i; }
        std::snprintf (line, sizeof (line), "after that abuse the output dies away slowly (%.1e V two seconds later, far below hearing and below 20 Hz), the iron keeps some flux (%.4f Vs), and after %.1f s more it is at rest and costs nothing", tail, t.flux(), idleAt / 192000.0);
        check (tail < 1.0e-3 && idleAt >= 0 && t.process (0.0) == 0.0, line);
        LineTransformer q; q.prepare (192000.0); for (int i = 0; i < 9600; ++i) q.process (5.0 * std::sin (i * 0.01)); q.setParts (SqConsole::ironFor (20.0), {}); finite = true; for (int i = 0; i < 96000; ++i) finite = finite && std::isfinite (q.process (5.0 * std::sin (i * 0.01)));
        check (finite, "changing the iron's size while running stays finite");
        LineTransformer s; s.prepare (fs); const int n = 48000 * 4; const auto t0 = std::chrono::steady_clock::now(); double acc = 0;
        for (int i = 0; i < n; ++i) acc += s.process (5.0 * std::sin (i * 0.013));
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  NOTE speed here: one channel (no oversampling) runs %.1f times faster than real time (%.1f %% of one core); acc %.1f\n", 4.0 / secs, secs / 4.0 * 100.0, acc);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
