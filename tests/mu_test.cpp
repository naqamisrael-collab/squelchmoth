// Known-answer test for SqMu (the variable-mu pair and its compressor). No dependencies.
// The pair and the loop are compared with ngspice 42 solving the same circuit (tests/reference/mu).
#include <cstdio>
#include <complex>
#include <vector>
#include "SqMu.h"
using namespace SqMu;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct StaticRef { double control, amp, h[3]; };
struct LoopRef { double t, cap, peak; };
#include "reference/mu/reference.inc"
static const double pi = 3.14159265358979323846;
static char line[800];
static const double depthCompress = 1.9103, depthLimit = 16.0162;

static Compressor make (double fs, const Sidechain& sc, double mismatch = 0.0)
{
    Remote a = remote(), b = remote(); b.a.kg1 *= 1.0 + mismatch; b.b.kg1 *= 1.0 + mismatch;
    Compressor c; c.prepare (fs, a, b, a, b, {}, 1.0); c.set (sc); return c;
}
static double steady (double fs, Sidechain sc, double db)
{
    auto c = make (fs, sc); const double amp = std::pow (10.0, db / 20.0); double pk = 0; const int n = (int) ((1.5 + 6 * sc.release) * fs);
    for (int i = 0; i < n; ++i) { const double v = c.process (amp * std::sin (2 * pi * 1000.0 * i / fs)); if (i > n - (int) (fs / 10)) pk = std::max (pk, std::abs (v)); }
    return 20 * std::log10 (pk);
}

int main()
{
    std::printf ("== The pair against the circuit simulator ==\n");
    {
        double worstF = 0, worst3 = 0, worst5 = 0; int n3 = 0;
        for (const auto& r : ref_static)
        {
            Stage s; s.prepare (remote(), remote(), {}); const int N = 7680; std::complex<double> a[3] = {};
            for (int i = 0; i < N; ++i) { const double y = s.process (r.amp * std::sin (2 * pi * 10.0 * i / N), r.control); for (int k = 0; k < 3; ++k) a[k] += y * std::polar (1.0, -2 * pi * 10.0 * (2 * k + 1) * i / N); }
            double h[3]; for (int k = 0; k < 3; ++k) h[k] = 20 * std::log10 (2 * std::abs (a[k]) / N + 1e-300);
            worstF = std::max (worstF, std::abs (h[0] - r.h[0])); if (r.h[1] > -110) { worst3 = std::max (worst3, std::abs (h[1] - r.h[1])); ++n3; } if (r.h[2] > -110) worst5 = std::max (worst5, std::abs (h[2] - r.h[2]));
        }
        std::snprintf (line, sizeof (line), "fifteen cases (control 0 to -12 V; 0.01, 1 and 2 V between the grids): level within %.4f dB, 3rd harmonic within %.3f dB (%d cases above the simulator's floor), 5th within %.3f dB", worstF, worst3, n3, worst5);
        check (worstF < 0.005 && worst3 < 0.1 && worst5 < 0.5 && n3 >= 8, line);
        Stage s; s.prepare (remote(), remote(), {}); const double g0 = s.gainAt (0.0); char d[200] = ""; size_t used = 0; bool falling = true; double prev = 1;
        for (double c : { -2.0, -4.0, -8.0, -12.0 }) { const double g = 20 * std::log10 (s.gainAt (c) / g0); falling = falling && g < prev; prev = g; used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.1f", g); }
        std::snprintf (line, sizeof (line), "pulling the grids negative turns the gain down smoothly: %s dB at -2, -4, -8 and -12 V (gain %.1f at rest)", d, g0); check (falling && prev < -20 && prev > -27, line);
    }
    std::printf ("== Push-pull ==\n");
    {
        auto h2of = [&] (double mismatch) { Remote a = remote(), b = remote(); b.a.kg1 *= 1.0 + mismatch; b.b.kg1 *= 1.0 + mismatch; Stage s; s.prepare (a, b, {}); const int N = 4800; std::complex<double> a1 = 0, a2 = 0; for (int i = 0; i < N; ++i) { const double y = s.process (1.0 * std::sin (2 * pi * 10.0 * i / N), -2.0); a1 += y * std::polar (1.0, -2 * pi * 10.0 * i / N); a2 += y * std::polar (1.0, -2 * pi * 20.0 * i / N); } return 20 * std::log10 (std::abs (a2) / std::abs (a1) + 1e-300); };
        const double matched = h2of (0.0), off = h2of (0.02);
        std::snprintf (line, sizeof (line), "a matched pair cancels the 2nd harmonic (%.0f dB); tubes 2 %% apart leave %.1f dB", matched, off); check (matched < -200 && off > -110 && off < -60, line);
        auto thump = [&] (double mismatch) { Remote a = remote(), b = remote(); b.a.kg1 *= 1.0 + mismatch; b.b.kg1 *= 1.0 + mismatch; Stage s; s.prepare (a, b, {}); const double rest = s.process (0.0, 0.0), deep = s.process (0.0, -8.0); return std::abs (deep - rest) / s.gainAt (0.0); };
        std::snprintf (line, sizeof (line), "the control voltage itself does not come through a matched pair (%.1e V); 2 %% apart, a move to -8 V leaks the equal of %.1f dB below full scale - the thump such compressors are trimmed for", thump (0.0), 20 * std::log10 (thump (0.02)));
        check (thump (0.0) < 1e-9 && thump (0.02) > 1e-4 && thump (0.02) < 0.1, line);
    }
    std::printf ("== The loop against the circuit simulator ==\n");
    for (int mode = 0; mode < 2; ++mode)
    {
        const LoopRef* ref = mode == 0 ? ref_compress : ref_limit; const int nref = 12;
        for (double fs : { 192000.0, 48000.0 })
        {
            Sidechain sc; sc.depth = mode == 0 ? depthCompress : depthLimit; sc.attack = 0.05; sc.release = 0.6; auto c = make (fs, sc); const double g0 = c.st[0].gainAt (0.0); sc.threshold = 1.5 / g0; c.set (sc);
            double worstC = 0, worstP = 0; int k = 0; std::vector<double> y;
            for (int i = 0; i < (int) (2.5 * fs) && k < nref; ++i)
            {
                const double t = i / fs;
                if (t >= ref[k].t) { double pk = 0; for (int j = (int) y.size() - (int) (0.002 * fs); j < (int) y.size(); ++j) pk = std::max (pk, std::abs (y[(size_t) j])); worstC = std::max (worstC, std::abs (c.capacitor() - ref[k].cap) / std::max (0.02, ref[k].cap)); worstP = std::max (worstP, std::abs (20 * std::log10 (pk * g0 / ref[k].peak))); ++k; }
                y.push_back (c.process ((t > 0.2 && t < 1.0 ? 1.0 : 0.05) * std::sin (2 * pi * 1000.0 * t)));
            }
            std::snprintf (line, sizeof (line), "%s at %.0f kHz: a tone jumping 26 dB up and back - the control capacitor within %.2f %%, the output level within %.3f dB at twelve moments", mode == 0 ? "Compress" : "Limit", fs / 1000.0, worstC * 100, worstP);
            check (worstC < (fs > 100000 ? 0.01 : 0.03) && worstP < (fs > 100000 ? 0.03 : 0.1), line);
        }
    }
    std::printf ("== Ratios, measured (published for such a unit: about 1.5 : 1 soft, 4 : 1 firmer, rising when pushed) ==\n");
    {
        const double fs = 48000.0; Sidechain sc; sc.threshold = 0.1; sc.attack = 0.05; sc.release = 0.6;
        auto ratio = [&] (Sidechain s, double from, double to) { return (to - from) / (steady (fs, s, -20.0 + to) - steady (fs, s, -20.0 + from)); };
        sc.depth = depthCompress; const double c1 = ratio (sc, 0, 12), c2 = ratio (sc, 12, 20); sc.depth = depthLimit; const double l1 = ratio (sc, 0, 12), l2 = ratio (sc, 12, 20);
        std::snprintf (line, sizeof (line), "Compress %.2f : 1 over the first 12 dB above the threshold and %.2f beyond; Limit %.2f and %.2f", c1, c2, l1, l2); check (std::abs (c1 - 1.5) < 0.05 && std::abs (l1 - 4.0) < 0.15 && c2 > c1 && l2 > l1, line);
        Sidechain fast = sc, slow = sc; fast.release = 0.2; slow.release = 8.0; const double rf = ratio (fast, 0, 12), rs = ratio (slow, 0, 12);
        std::snprintf (line, sizeof (line), "the ratio leans on the recovery time, because the capacitor empties between peaks: Limit measures %.2f : 1 at 0.2 s and %.2f at 8 s", rf, rs); check (rf < l1 && rs > l1 && rf > 2.5 && rs < 6.0, line);
        const double below = steady (fs, sc, -30.0) + 30.0; std::snprintf (line, sizeof (line), "under the threshold nothing moves (%.4f dB)", below); check (std::abs (below) < 0.005, line);
        const double far = steady (fs, sc, 22.0) - steady (fs, sc, 16.0);
        std::snprintf (line, sizeof (line), "the grids can be pulled no further than -12 V (about 23 dB of reduction): past that it stops turning down (%.1f dB out for 6 dB in)", far); check (far > 5.0, line);
    }
    std::printf ("== Attack and recovery (the capacitor's two time constants) ==\n");
    {
        const double fs = 48000.0;
        auto times = [&] (double attack, double release, double& att, double& rec)
        {
            Sidechain sc; sc.threshold = 0.1; sc.depth = depthLimit; sc.attack = attack; sc.release = release; auto c = make (fs, sc); const int on = (int) ((1.0 + 3 * release) * fs); std::vector<double> gr;
            for (int i = 0; i < on + (int) (3 * release * fs); ++i) { c.process (i < on ? std::sin (2 * pi * 1000.0 * i / fs) : 0.0); if (i % 48 == 0) gr.push_back (c.reductionDb()); }
            const int i0 = on / 48; const double end = gr[(size_t) i0 - 1]; int a = 0; while (gr[(size_t) a] > 0.632 * end) ++a; int r = i0; while (r < (int) gr.size() - 1 && gr[(size_t) r] < end * 0.3679) ++r; att = a; rec = (r - i0) / 1000.0;
        };
        double a25, r02, a70, r8; times (0.025, 0.2, a25, r02); times (0.07, 8.0, a70, r8);
        std::snprintf (line, sizeof (line), "attack 25 ms / recovery 0.2 s: 63 %% of the reduction in %.0f ms, and it falls to 37 %% in %.2f s; attack 70 ms / recovery 8 s: %.0f ms and %.1f s", a25, r02, a70, r8);
        check (a25 < a70 && a25 > 2 && a70 < 70 && r02 > 0.15 && r02 < 0.3 && r8 > 6.0 && r8 < 10.5, line);
    }
    std::printf ("== Two channels, one sidechain; any sample rate ==\n");
    {
        Sidechain sc; sc.threshold = 0.1; sc.depth = depthLimit; auto c = make (48000.0, sc, 0.01); double pl = 0, pr = 0;
        std::complex<double> al = 0, ar = 0; const int M = 9600;
        for (int i = 0; i < 4 * 48000; ++i) { double l = std::sin (2 * pi * 1000.0 * i / 48000.0), r = 0.01 * std::sin (2 * pi * 700.0 * i / 48000.0); c.process (l, r); if (i >= 4 * 48000 - M) { al += l * std::polar (1.0, -2 * pi * 1000.0 * i / 48000.0); ar += r * std::polar (1.0, -2 * pi * 700.0 * i / 48000.0); } }
        pl = 2 * std::abs (al) / M; pr = 2 * std::abs (ar) / M; const double gl = 20 * std::log10 (pl), gr = 20 * std::log10 (pr / 0.01);
        std::snprintf (line, sizeof (line), "a loud left takes a quiet right down with it (%.2f and %.2f dB; tubes 1 %% apart)", gl, gr); check (gl < -10 && std::abs (gl - gr) < 0.3, line);
        auto a1 = make (192000.0, sc, 0.01), a2 = make (192000.0, sc, 0.01); double wl = 0, wr = 0;
        for (int i = 0; i < 96000; ++i) { double l = std::sin (2 * pi * 1000.0 * i / 192000.0), r = 0.3 * std::sin (2 * pi * 700.0 * i / 192000.0); const double l2 = a2.left (l, i % 4), r2 = a2.right (r, i % 4); a1.process (l, r); wl = std::max (wl, std::abs (l - l2)); wr = std::max (wr, std::abs (r - r2)); }
        std::snprintf (line, sizeof (line), "taken a channel at a time (the right heard a few microseconds late) it is the same compressor to within %.1e left, %.1e right", wl, wr); check (wl < 2e-3 && wr < 2e-3, line);
        const double r44 = steady (44100.0, sc, 0.0), r192 = steady (192000.0, sc, 0.0); std::snprintf (line, sizeof (line), "the same compression at 44.1 and 192 kHz (%.3f, %.3f dB)", r44, r192); check (std::abs (r44 - r192) < 0.05, line);
    }
    std::printf ("== Distortion and aliasing ==\n");
    {
        auto spectrum = [&] (double fs, double hz, double depth, double amp, double& fund, double& h2, double& h3, double& worstOther)
        {
            Sidechain sc; sc.threshold = 0.1; sc.depth = depth; auto c = make (fs, sc); const int N = (int) fs; std::vector<double> y ((size_t) N); for (int i = 0; i < (int) (5 * fs); ++i) c.process (amp * std::sin (2 * pi * hz * i / fs));
            for (int i = 0; i < N; ++i) y[(size_t) i] = c.process (amp * std::sin (2 * pi * hz * (i + (int) (5 * fs)) / fs));
            auto bin = [&] (double f) { std::complex<double> acc = 0; for (int i = 0; i < N; ++i) acc += y[(size_t) i] * std::polar (1.0, -2 * pi * f * i / fs); return 20 * std::log10 (2 * std::abs (acc) / N + 1e-300); };
            fund = bin (hz); h2 = bin (2 * hz) - fund; h3 = bin (3 * hz) - fund; worstOther = -400;
            for (int k = 2; k <= 40; ++k) { double f = std::fmod (k * hz, fs); if (f > fs / 2) f = fs - f; if (k * hz > fs / 2 && std::abs (f - hz) > 1.0) worstOther = std::max (worstOther, bin (f) - fund); }
        };
        double f, h2, h3, al; spectrum (48000.0, 1000.0, 0.0, 1.0, f, h2, h3, al); const double rest3 = h3; spectrum (48000.0, 1000.0, depthLimit, 1.0, f, h2, h3, al);
        std::snprintf (line, sizeof (line), "a full-scale 1 kHz tone: 3rd harmonic %.1f dB down with no compression and %.1f dB down at %.1f dB of reduction (under a tenth of a per cent, as published for such units)", -rest3, -h3, -f);
        check (rest3 < -60 && h3 < -60 && h3 > -110, line);
        double f50, a50, b50, c50; spectrum (48000.0, 50.0, depthLimit, 1.0, f50, a50, b50, c50);
        std::snprintf (line, sizeof (line), "50 Hz: 3rd harmonic %.1f dB down - the capacitor ripples with a slow wave", -b50); check (b50 > h3 && b50 < -25, line);
        double w1 = -400, w4 = -400; for (double hz : { 5000.0, 9000.0, 15000.0 }) for (double depth : { depthCompress, depthLimit }) for (double amp : { 1.0, 3.98 }) { double ff, x2, x3, o; spectrum (48000.0, hz, depth, amp, ff, x2, x3, o); w1 = std::max (w1, o); spectrum (192000.0, hz, depth, amp, ff, x2, x3, o); w4 = std::max (w4, o); }
        std::snprintf (line, sizeof (line), "aliasing, 5 / 9 / 15 kHz tones at full scale and 12 dB over, both modes: the strongest folded-back tone is %.0f dB down at 4x (%.0f dB with no oversampling)", -w4, -w1); check (w4 < -100.0, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
