// Known-answer test for SqBridge (the diode ring and its compressor). No dependencies.
// The ring and the loop are compared with ngspice 42 solving the same circuit (tests/reference/bridge); the ratios
// are compared with the figures published for such compressors.
#include <cstdio>
#include <complex>
#include <vector>
#include "SqBridge.h"
using namespace SqBridge;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct StaticRef { double mult, amp, h[3]; };
struct LoopRef { double t, fast, slow, peak; };
#include "reference/bridge/reference.inc"
static const double pi = 3.14159265358979323846;
static char line[900];
static const double bridgeVolts = 0.03;      // the plug-in's own figure: volts across the ring's inputs for a full-scale sample

static Compressor make (double fs, const Sidechain& sc, double volts = bridgeVolts, double mismatch = 0.0)
{
    SqClip::Diode ring[8]; for (auto& d : ring) d = SqClip::silicon(); ring[1].is *= 1.0 + mismatch; ring[5].is *= 1.0 + mismatch;
    Compressor c; c.prepare (fs, ring, {}, volts); c.set (sc); return c;
}
/** Peak level out in dB for a steady 1 kHz tone `db` above full scale. */
static double steady (double fs, Sidechain sc, double db, double hz = 1000.0)
{
    auto c = make (fs, sc); const double amp = std::pow (10.0, db / 20.0); double pk = 0; const int n = (int) ((1.0 + 8 * sc.release) * fs);
    for (int i = 0; i < n; ++i) { const double v = c.process (amp * std::sin (2 * pi * hz * i / fs)); if (i > n - (int) (fs / 10)) pk = std::max (pk, std::abs (v)); }
    return 20 * std::log10 (pk);
}

int main()
{
    const double unit = SqClip::silicon().n * SqClip::thermalVolts / 4700.0;
    std::printf ("== The ring against the circuit simulator ==\n");
    {
        double worstF = 0, worst3 = 0, worst5 = 0; int n3 = 0, n5 = 0;
        for (const auto& r : ref_static)
        {
            Bridge b; b.prepare (SqClip::silicon(), SqClip::silicon(), SqClip::silicon(), SqClip::silicon(), {}); const int N = 7680; std::complex<double> a[3] = {};
            for (int i = 0; i < N; ++i) { const double y = b.process (r.amp * std::sin (2 * pi * 10.0 * i / N), r.mult * unit); for (int k = 0; k < 3; ++k) a[k] += y * std::polar (1.0, -2 * pi * 10.0 * (2 * k + 1) * i / N); }
            double h[3]; for (int k = 0; k < 3; ++k) h[k] = 20 * std::log10 (2 * std::abs (a[k]) / N + 1e-300);
            worstF = std::max (worstF, std::abs (h[0] - r.h[0])); if (r.h[1] > -130) { worst3 = std::max (worst3, std::abs (h[1] - r.h[1])); ++n3; } if (r.h[2] > -130) { worst5 = std::max (worst5, std::abs (h[2] - r.h[2])); ++n5; }
        }
        std::snprintf (line, sizeof (line), "eighteen cases (control current 0 to 30 units; 1, 40 and 100 mV in): level within %.4f dB, 3rd harmonic within %.3f dB (%d cases above the simulator's floor), 5th within %.3f dB (%d cases)", worstF, worst3, n3, worst5, n5);
        check (worstF < 0.005 && worst3 < 0.2 && worst5 < 0.5 && n3 >= 9 && n5 >= 3, line);
        Bridge b; b.prepare (SqClip::silicon(), SqClip::silicon(), SqClip::silicon(), SqClip::silicon(), {}); const double g0 = b.gainAt (0.0); double worst = 0; char d[200] = ""; size_t used = 0;
        for (double m : { 1.0, 3.0, 9.0, 30.0, 100.0 }) { const double g = 20 * std::log10 (b.gainAt (m * unit) / g0), ideal = -20 * std::log10 (1.0 + m); worst = std::max (worst, std::abs (g - ideal)); used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.2f", g); }
        std::snprintf (line, sizeof (line), "the gain follows 1 / (1 + current): %s dB at 1, 3, 9, 30 and 100 units (within %.3f dB of that law; one unit is %.2f microamps)", d, worst, unit * 1e6); check (worst < 0.1 && std::abs (g0 - 1.0) < 0.002, line);
    }
    std::printf ("== Balance ==\n");
    {
        auto h2of = [&] (double mismatch) { SqClip::Diode a = SqClip::silicon(), m = a; m.is *= 1.0 + mismatch; Bridge b; b.prepare (a, m, a, a, {}); const int N = 4800; std::complex<double> a1 = 0, a2 = 0;
            for (int i = 0; i < N; ++i) { const double y = b.process (0.05 * std::sin (2 * pi * 10.0 * i / N), unit); a1 += y * std::polar (1.0, -2 * pi * 10.0 * i / N); a2 += y * std::polar (1.0, -2 * pi * 20.0 * i / N); } return 20 * std::log10 (std::abs (a2) / std::abs (a1) + 1e-300); };
        const double matched = h2of (0.0), off = h2of (0.05);
        std::snprintf (line, sizeof (line), "four matched diodes make no 2nd harmonic (%.0f dB); one diode 5 %% out leaves %.1f dB", matched, off); check (matched < -200 && off > -110 && off < -50, line);
        auto thump = [&] (double mismatch) { SqClip::Diode a = SqClip::silicon(), m = a; m.is *= 1.0 + mismatch; Bridge b; b.prepare (a, m, a, a, {}); const double rest = b.process (0.0, 0.0), deep = b.process (0.0, 9.0 * unit); return std::abs (deep - rest); };
        std::snprintf (line, sizeof (line), "the control current itself does not come through a matched ring (%.1e V); with one diode 5 %% out a move to 20 dB of reduction leaks %.2f mV", thump (0.0), thump (0.05) * 1000.0);
        check (thump (0.0) < 1e-9 && thump (0.05) > 1e-5 && thump (0.05) < 0.01, line);
    }
    std::printf ("== The loop against the circuit simulator ==\n");
    for (int mode = 0; mode < 2; ++mode)
    {
        const LoopRef* ref = mode == 0 ? ref_plain : ref_auto; const int nref = 12;
        for (double fs : { 192000.0, 48000.0 })
        {
            Sidechain sc; if (mode == 0) { sc.ratio = 4.0; sc.attack = 0.003; sc.release = 0.4; } else { sc.ratio = 2.0; sc.attack = 0.006; sc.release = 0.1; sc.slowRelease = 2.0; sc.slowCharge = 0.5; }
            auto c = make (fs, sc, 1.0); sc.threshold = 0.01 * c.open[0]; c.set (sc); double worstF = 0, worstS = 0, worstP = 0; int at = 0; double pk = 0; std::vector<double> ys;
            const int n = (int) (1.5 * fs);
            for (int i = 0; i < n && at < nref; ++i)
            {
                const double t = i / fs, amp = (t > 0.1 && t < 0.6) ? 0.04 : 0.005; const double y = c.process (amp * std::sin (2 * pi * 1000.0 * t)) / c.open[0]; ys.push_back (y);
                if (t + 0.5 / fs >= ref[at].t)
                {
                    pk = 0; for (int k = (int) ys.size() - 1; k >= 0 && k > (int) ys.size() - 1 - (int) (0.002 * fs); --k) pk = std::max (pk, std::abs (ys[(size_t) k]));
                    if (ref[at].fast > 0.02) worstF = std::max (worstF, std::abs (c.fast / ref[at].fast - 1.0)); if (ref[at].slow > 0.02) worstS = std::max (worstS, std::abs (c.slow / ref[at].slow - 1.0)); worstP = std::max (worstP, std::abs (pk / ref[at].peak - 1.0)); ++at;
                }
            }
            std::snprintf (line, sizeof (line), "%s, a 1 kHz tone jumping 18 dB up and back, at %.0f kHz: the capacitor within %.2f %%%s and the output's peaks within %.2f %% at twelve moments", mode == 0 ? "4:1 with plain recovery" : "2:1 with automatic recovery", fs / 1000.0, worstF * 100, mode == 1 ? (std::snprintf (line + 700, 100, ", the slow capacitor within %.2f %%", worstS * 100), line + 700) : "", worstP * 100);
            const double tol = fs > 100000.0 ? 0.02 : 0.05; check (worstF < tol && worstS < tol && worstP < tol && at == nref, line);
        }
    }
    std::printf ("== Ratios, against the published test figures ==\n");
    {
        const double ratios[5] = { 1.5, 2.0, 3.0, 4.0, 5.0 }, want[5] = { 6.5, 5.0, 3.5, 2.5, 1.5 }, tol[5] = { 1.0, 1.0, 1.0, 0.5, 0.5 }; bool ok = true; char d[300] = ""; size_t used = 0;
        for (int i = 0; i < 5; ++i) { Sidechain sc; sc.threshold = 0.01; sc.ratio = ratios[i]; sc.release = 0.1; const double got = steady (48000.0, sc, -10.0) - steady (48000.0, sc, -20.0); ok = ok && std::abs (got - want[i]) <= tol[i]; used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.2f (%.1f+-%.1f)", got, want[i], tol[i]); }
        std::snprintf (line, sizeof (line), "output change for 10 dB more input, 20 to 30 dB over the threshold, at 1.5, 2, 3, 4 and 5 to 1:%s dB", d); check (ok, line);
        Sidechain sc; sc.threshold = 0.01; sc.ratio = 6.0; sc.release = 0.1; const double six = steady (48000.0, sc, -10.0) - steady (48000.0, sc, -20.0);
        std::snprintf (line, sizeof (line), "at 6 to 1 the same 10 dB gives %.2f dB", six); check (six > 0.8 && six < 1.7, line);
        sc.ratio = 4.0; const double low = steady (48000.0, sc, -37.0) - steady (48000.0, sc, -39.0), high = steady (48000.0, sc, -28.0) - steady (48000.0, sc, -30.0), under = steady (48000.0, sc, -46.0) + 46.0;
        std::snprintf (line, sizeof (line), "the knee is soft: at 4 to 1, 2 dB more input gives %.2f dB just over the threshold and %.2f dB from 10 dB over (the full ratio comes in only some way above the threshold, as published); below it nothing happens (%.3f dB)", low, high, under);
        check (low > 1.2 && high < 0.8 && low > high && std::abs (under) < 0.01, line);
    }
    std::printf ("== Attack and recovery ==\n");
    {
        auto attackMs = [&] (double attack, double hpHz) { Sidechain sc; sc.threshold = 0.01; sc.ratio = 3.0; sc.attack = attack; sc.release = 0.8; sc.highPassHz = hpHz; auto c = make (192000.0, sc); double fin = 0;
            { auto f = make (192000.0, sc); for (int i = 0; i < 192000; ++i) f.process (0.1 * std::sin (2 * pi * 1000.0 * i / 192000.0)); fin = f.fast; }
            for (int i = 0; i < 192000; ++i) { c.process (0.1 * std::sin (2 * pi * 1000.0 * i / 192000.0)); if (c.fast > 0.632 * fin) return i / 192.0; } return 1e9; };
        const double fastMs = attackMs (0.003, 0.0), slowMs = attackMs (0.006, 100.0);
        std::snprintf (line, sizeof (line), "a tone arriving 20 dB over: the capacitor is 63 %% of the way in %.2f ms on Fast and %.2f ms on Slow", fastMs, slowMs); check (fastMs > 0.3 && fastMs < 4.0 && slowMs > 1.6 * fastMs && slowMs < 3.0 * fastMs, line);
        Sidechain sc; sc.threshold = 0.01; sc.ratio = 3.0; sc.release = 0.4; const double f40 = steady (48000.0, sc, -20.0, 40.0), f1k = steady (48000.0, sc, -20.0); sc.attack = 0.006; sc.highPassHz = 100.0; const double s40 = steady (48000.0, sc, -20.0, 40.0), s1k = steady (48000.0, sc, -20.0);
        std::snprintf (line, sizeof (line), "on Slow the sidechain hears less below 100 Hz: a 40 Hz tone 20 dB over comes out at %.2f dB (Fast %.2f); a 1 kHz tone at %.2f (Fast %.2f)", s40, f40, s1k, f1k); check (s40 > f40 + 1.0 && std::abs (s1k - f1k) < 0.3, line);
        char d[300] = ""; size_t used = 0; bool ok = true;
        for (double rel : { 0.1, 0.4, 0.8, 1.5 })
        {
            Sidechain s2; s2.threshold = 0.01; s2.ratio = 3.0; s2.release = rel; auto c = make (48000.0, s2); for (int i = 0; i < 48000; ++i) c.process (0.1 * std::sin (2 * pi * 1000.0 * i / 48000.0));
            const double start = c.fast; double took = -1; for (int i = 0; i < 48000 * 8; ++i) { c.process (0.0); if (c.fast < start / 2.718281828459045) { took = (i + 1) / 48000.0; break; } }
            ok = ok && std::abs (took / rel - 1.0) < 0.01; used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.0f", took * 1000.0);
        }
        std::snprintf (line, sizeof (line), "recovery: the capacitor falls to 1/e in%s ms at the 100, 400, 800 and 1500 ms settings", d); check (ok, line);
        auto halfBack = [&] (double burst, double fastRel, double slowRel) { Sidechain s2; s2.threshold = 0.01; s2.ratio = 3.0; s2.release = fastRel; s2.slowRelease = slowRel; auto c = make (48000.0, s2); const int nb = (int) (burst * 48000.0); for (int i = 0; i < nb; ++i) c.process (0.1 * std::sin (2 * pi * 1000.0 * i / 48000.0));
            const double start = c.held(); for (int i = 0; i < 48000 * 30; ++i) { c.process (0.0); if (c.held() < 0.25 * start) return (i + 1) / 48000.0; } return 1e9; };
        const double a1s = halfBack (0.03, 0.1, 2.0), a1l = halfBack (5.0, 0.1, 2.0), a2s = halfBack (0.03, 0.05, 5.0), a2l = halfBack (5.0, 0.05, 5.0);
        std::snprintf (line, sizeof (line), "automatic recovery: back to a quarter in %.2f s after a 30 ms peak but %.2f s after 5 s of loud (Auto 1, 100 ms / 2 s); %.2f s and %.2f s on Auto 2 (50 ms / 5 s)", a1s, a1l, a2s, a2l);
        check (a1s < 0.2 && a1l > 1.0 && a1l < 3.0 && a2s < 0.1 && a2l > 2.5 && a2l < 7.5, line);
    }
    std::printf ("== Two channels, one sidechain; any sample rate ==\n");
    {
        Sidechain sc; sc.threshold = 0.1; sc.ratio = 4.0; auto c = make (48000.0, sc, bridgeVolts, 0.02);
        std::complex<double> al = 0, ar = 0; const int M = 9600;
        for (int i = 0; i < 4 * 48000; ++i) { double l = std::sin (2 * pi * 1000.0 * i / 48000.0), r = 0.01 * std::sin (2 * pi * 700.0 * i / 48000.0); c.process (l, r); if (i >= 4 * 48000 - M) { al += l * std::polar (1.0, -2 * pi * 1000.0 * i / 48000.0); ar += r * std::polar (1.0, -2 * pi * 700.0 * i / 48000.0); } }
        const double gl = 20 * std::log10 (2 * std::abs (al) / M), gr = 20 * std::log10 (2 * std::abs (ar) / M / 0.01);
        std::snprintf (line, sizeof (line), "a loud left takes a quiet right down with it (%.2f and %.2f dB)", gl, gr); check (gl < -8 && std::abs (gl - gr) < 0.3, line);
        auto a1 = make (192000.0, sc), a2 = make (192000.0, sc); double wl = 0, wr = 0;
        for (int i = 0; i < 96000; ++i) { double l = std::sin (2 * pi * 1000.0 * i / 192000.0), r = 0.3 * std::sin (2 * pi * 700.0 * i / 192000.0); const double l2 = a2.left (l, i % 4), r2 = a2.right (r, i % 4); a1.process (l, r); wl = std::max (wl, std::abs (l - l2)); wr = std::max (wr, std::abs (r - r2)); }
        std::snprintf (line, sizeof (line), "taken a channel at a time (the right heard a few microseconds late) it is the same compressor to within %.1e left, %.1e right", wl, wr); check (wl < 2e-3 && wr < 2e-3, line);
        const double r44 = steady (44100.0, sc, 0.0), r192 = steady (192000.0, sc, 0.0); std::snprintf (line, sizeof (line), "the same compression at 44.1 and 192 kHz (%.3f, %.3f dB)", r44, r192); check (std::abs (r44 - r192) < 0.05, line);
    }
    std::printf ("== Distortion and aliasing ==\n");
    {
        auto spectrum = [&] (double fs, double hz, double ratio, double thr, double amp, double& fund, double& h2, double& h3, double& worstOther)
        {
            Sidechain sc; sc.threshold = thr; sc.ratio = ratio; auto c = make (fs, sc); const int N = (int) fs; std::vector<double> y ((size_t) N); for (int i = 0; i < (int) (5 * fs); ++i) c.process (amp * std::sin (2 * pi * hz * i / fs));
            for (int i = 0; i < N; ++i) y[(size_t) i] = c.process (amp * std::sin (2 * pi * hz * (i + (int) (5 * fs)) / fs));
            auto bin = [&] (double f) { std::complex<double> acc = 0; for (int i = 0; i < N; ++i) acc += y[(size_t) i] * std::polar (1.0, -2 * pi * f * i / fs); return 20 * std::log10 (2 * std::abs (acc) / N + 1e-300); };
            fund = bin (hz); h2 = bin (2 * hz) - fund; h3 = bin (3 * hz) - fund; worstOther = -400;
            for (int k = 2; k <= 40; ++k) { double f = std::fmod (k * hz, fs); if (f > fs / 2) f = fs - f; if (k * hz > fs / 2 && f < 24000.0 && std::abs (f - hz) > 1.0) worstOther = std::max (worstOther, bin (f) - fund); }
        };
        double f, h2, h3, al; spectrum (48000.0, 1000.0, 6.0, 100.0, 1.0, f, h2, h3, al); const double rest3 = h3; double worst3 = -400; char d[200] = ""; size_t used = 0;
        for (double thr : { 0.5, 0.25, 0.1, 0.03 }) { spectrum (48000.0, 1000.0, 6.0, thr, 1.0, f, h2, h3, al); worst3 = std::max (worst3, h3); used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.3f %% at %.1f dB", 100.0 * std::pow (10.0, h3 / 20.0), -f); }
        std::snprintf (line, sizeof (line), "a full-scale 1 kHz tone at 6 to 1: 3rd harmonic %.4f %% with no compression;%s of reduction (published: under 0.075 %% idle, under 0.2 %% compressing)", 100.0 * std::pow (10.0, rest3 / 20.0), d);
        check (rest3 < -62.5 && worst3 < -54.0 && worst3 > -90.0, line);
        double f50, a50, b50, c50; spectrum (48000.0, 50.0, 6.0, 0.1, 1.0, f50, a50, b50, c50); spectrum (48000.0, 1000.0, 6.0, 0.1, 1.0, f, h2, h3, al);
        std::snprintf (line, sizeof (line), "50 Hz: 3rd harmonic %.1f dB down (%.1f at 1 kHz) - the capacitor ripples with a slow wave", -b50, -h3); check (b50 > h3 && b50 < -20, line);
        double w1 = -400, w4 = -400; for (double hz : { 5000.0, 9000.0, 15000.0 }) for (double ratio : { 2.0, 6.0 }) for (double amp : { 1.0, 3.98 }) { double ff, x2, x3, o; spectrum (48000.0, hz, ratio, 0.1, amp, ff, x2, x3, o); w1 = std::max (w1, o); spectrum (192000.0, hz, ratio, 0.1, amp, ff, x2, x3, o); w4 = std::max (w4, o); }
        std::snprintf (line, sizeof (line), "aliasing, 5 / 9 / 15 kHz tones at full scale and 12 dB over, 2 and 6 to 1: the strongest tone folded back below 24 kHz is %.0f dB down at 4x (%.0f dB with no oversampling)", -w4, -w1); check (w4 < -100.0, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
