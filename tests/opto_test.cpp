// Known-answer test for SqOpto (the optical gain cell and its levelling compressor). No dependencies.
// The loop is compared with ngspice 42 solving the same equations as a circuit (tests/reference/opto).
#include <cstdio>
#include <complex>
#include <vector>
#include "SqOpto.h"
using namespace SqOpto;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct LoopRef { double t, fast, slow, peak; };
#include "reference/opto/reference.inc"
static const double pi = 3.14159265358979323846;
static char line[700];

struct Trace { std::vector<double> gr; double fs; };       // gain reduction in dB once a millisecond
static Trace burst (double fs, double drive, double bias, double amp, double onS, double totalS, double hz = 1000.0)
{
    Compressor c; c.prepare (fs, {}, {}); c.set (drive, bias); Trace t; t.fs = fs; const int every = (int) (fs / 1000.0);
    for (int i = 0; i < (int) (totalS * fs); ++i) { double v = (i < onS * fs ? amp : 0.0) * std::sin (2 * pi * hz * i / fs); c.process (v); if (i % every == 0) t.gr.push_back (c.reductionDb()); }
    return t;
}
static double steady (double fs, double drive, double bias, double db, double seconds = 6.0)
{
    Compressor c; c.prepare (fs, {}, {}); c.set (drive, bias); const double amp = std::pow (10.0, db / 20.0); double pk = 0; const int n = (int) (seconds * fs);
    for (int i = 0; i < n; ++i) { double v = c.process (amp * std::sin (2 * pi * 1000.0 * i / fs)); if (i > n - (int) (fs / 10)) pk = std::max (pk, std::abs (v)); }
    return 20 * std::log10 (pk);
}

int main()
{
    std::printf ("== The cell as a divider ==\n");
    {
        Cell c; const double rest = 20 * std::log10 (gainFor (c, 0.0)), half = gainFor (c, 1.0);
        std::snprintf (line, sizeof (line), "in the dark it costs %.3f dB (the dark resistance is 200 times the series resistor); at the reference light the signal is halved (%.4f)", rest, half);
        check (std::abs (rest + 0.0433) < 0.001 && std::abs (half - 1.0 / 2.005) < 1e-12, line);
        const double t1 = 1.0 / gainFor (c, 1.0) - 1.0 - c.dark, t10 = 1.0 / gainFor (c, 10.0) - 1.0 - c.dark;
        std::snprintf (line, sizeof (line), "ten times the light lowers the resistance by 10^0.9 (measured factor %.4f, 10^0.9 = %.4f)", t10 / t1, std::pow (10.0, 0.9));
        check (std::abs (t10 / t1 - std::pow (10.0, 0.9)) < 1e-9, line);
        Compressor off; off.prepare (48000, {}, {}); off.set (0.0, 0.0); double worst = 0; for (int i = 0; i < 48000; ++i) { const double x = std::sin (2 * pi * 1000.0 * i / 48000.0); worst = std::max (worst, std::abs (off.process (x) - x / 1.005)); }
        std::snprintf (line, sizeof (line), "with the sidechain off it is a plain resistor divider: no compression, no distortion (largest difference %.1e)", worst); check (worst < 1e-15, line);
    }
    std::printf ("== The loop against the circuit simulator ==\n");
    for (int mode = 0; mode < 2; ++mode)
    {
        const LoopRef* ref = mode == 0 ? ref_bias0 : ref_bias8; const int nref = 12; const double bias = mode == 0 ? 0.0 : limitBias();
        for (double fs : { 192000.0, 48000.0 })
        {
            Compressor c; c.prepare (fs, {}, {}); c.set (4.0, bias); double worstF = 0, worstM = 0, worstP = 0; int k = 0; std::vector<double> y; y.reserve ((size_t) (3.0 * fs));
            for (int i = 0; i < (int) (3.0 * fs) && k < nref; ++i)
            {
                const double t = i / fs; double v = (t > 0.2 && t < 1.2 ? 1.0 : 0.05) * std::sin (2 * pi * 1000.0 * t);
                if (t >= ref[k].t)
                {
                    double pk = 0; for (int j = (int) y.size() - (int) (0.002 * fs); j < (int) y.size(); ++j) pk = std::max (pk, std::abs (y[(size_t) j]));
                    worstF = std::max (worstF, std::abs (c.fastState() - ref[k].fast) / std::max (0.02, ref[k].fast)); worstM = std::max (worstM, std::abs (c.slowState() - ref[k].slow) / std::max (0.02, ref[k].slow));
                    worstP = std::max (worstP, std::abs (20 * std::log10 (pk / ref[k].peak))); ++k;
                }
                c.process (v, v); y.push_back (v);
            }
            std::snprintf (line, sizeof (line), "%s at %.0f kHz: a tone jumping 26 dB up for a second and back - the cell's fast response within %.2f %%, its memory within %.2f %%, the output level within %.3f dB at twelve moments over three seconds",
                           mode == 0 ? "Compress" : "Limit", fs / 1000.0, worstF * 100, worstM * 100, worstP);
            check (worstF < (fs > 100000 ? 0.01 : 0.05) && worstM < (fs > 100000 ? 0.01 : 0.03) && worstP < (fs > 100000 ? 0.03 : 0.6), line);
        }
    }
    std::printf ("== Attack and release (published for this kind of cell: about 10 ms; half the release in about 60 ms, the rest over 1 to 15 s) ==\n");
    {
        const double fs = 48000.0;
        auto measure = [&] (double drive, double on, double& gr, int& att, int& half, double& full)
        {
            auto r = burst (fs, drive, 0.0, 1.0, on, on + 25.0); const int i0 = (int) (on * 1000); gr = r.gr[(size_t) i0 - 1]; const double g100 = r.gr[100];
            att = 0; while (r.gr[(size_t) att] > 0.632 * g100) ++att; half = i0; while (r.gr[(size_t) half] < 0.5 * gr) ++half; half -= i0;
            int f = i0; while (f < (int) r.gr.size() - 1 && r.gr[(size_t) f] < -0.5) ++f; full = (f - i0) / 1000.0;
        };
        double gr; int att, half; double full;
        measure (4.0, 2.0, gr, att, half, full);
        std::snprintf (line, sizeof (line), "a full-scale tone held for 2 s at %.1f dB of reduction: attack %d ms (to 63 %% of the reduction at 100 ms), half of the release in %d ms, back within 0.5 dB after %.1f s", -gr, att, half, full);
        check (att >= 5 && att <= 15 && half >= 50 && half <= 70 && full > 1.0 && full < 15.0, line);
        double gS, fS, gL, fL; int a2, h2; measure (4.0, 0.1, gS, a2, h2, fS); const int hShort = h2; measure (4.0, 10.0, gL, a2, h2, fL);
        std::snprintf (line, sizeof (line), "it remembers how long it was lit: after a tenth of a second it is back in %.2f s, after ten seconds in %.1f s (first half in %d and %d ms)", fS, fL, hShort, h2);
        check (fS < 0.5 * fL && fL < 15.0, line);
        double gH, fH, gG, fG; int aH, hH, aG, hG; measure (10.0, 2.0, gH, aH, hH, fH); measure (2.0, 2.0, gG, aG, hG, fG);
        std::snprintf (line, sizeof (line), "and how hard: at %.1f dB of reduction attack %d ms and back in %.1f s; at %.1f dB attack %d ms and back in %.1f s", -gG, aG, fG, -gH, aH, fH);
        check (aH < aG && fH > fG, line);
        auto slowTone = burst (fs, 4.0, 0.0, 1.0, 12.0, 12.0);
        std::snprintf (line, sizeof (line), "the attack has two stages: %.2f dB after 100 ms, creeping on to %.2f dB after 12 s as the memory fills", -slowTone.gr[100], -slowTone.gr[11990]);
        check (slowTone.gr[11990] < slowTone.gr[100] - 0.1 && slowTone.gr[11990] > slowTone.gr[100] - 1.5, line);
    }
    std::printf ("== Ratios, measured ==\n");
    {
        const double fs = 48000.0; char detail[300] = ""; size_t used = 0; bool ok = true, mono = true; double cLo = 99, cHi = 0, lLo = 99, lHi = 0;
        for (int mode = 0; mode < 2; ++mode) for (double drive : { 4.0, 8.0, 16.0 })
        {
            double o[5]; for (int j = 0; j < 5; ++j) { o[j] = steady (fs, drive, mode ? limitBias() : 0.0, -12.0 + 3.0 * j); if (j > 0) mono = mono && o[j] > o[j - 1]; }
            const double ratio = 12.0 / (o[4] - o[0]); (mode ? lLo : cLo) = std::min (mode ? lLo : cLo, ratio); (mode ? lHi : cHi) = std::max (mode ? lHi : cHi, ratio);
            used += (size_t) std::snprintf (detail + used, sizeof (detail) - used, " %s %.1f : 1 (%.1f dB down)", mode ? "Limit" : "Compress", ratio, -o[4]);
        }
        ok = cLo > 2.0 && cHi < 5.0 && lLo > 6.0 && mono;
        std::snprintf (line, sizeof (line), "over the top 12 dB at three depths:%s - Compress is a soft squeeze heading for 5 : 1, Limit a firm ceiling, and the output never falls as the input rises", detail); check (ok, line);
        const double below = steady (fs, 4.0, limitBias(), -30.0) + 30.0, belowC = steady (fs, 4.0, 0.0, -30.0) + 30.0;
        std::snprintf (line, sizeof (line), "30 dB down, Limit leaves the tone alone (%.3f dB, the dark loss) and Compress has barely begun (%.3f dB)", below, belowC); check (std::abs (below + 0.0433) < 0.002 && belowC > -0.1, line);
    }
    std::printf ("== Two channels, one panel; any sample rate ==\n");
    {
        const double fs = 48000.0; Cell a, b; b.sense = 1.03; Compressor c; c.prepare (fs, a, b); c.set (4.0, 0.0); double pl = 0, pr = 0;
        for (int i = 0; i < (int) (3 * fs); ++i) { double l = std::sin (2 * pi * 1000.0 * i / fs), r = 0.01 * std::sin (2 * pi * 700.0 * i / fs); c.process (l, r); if (i > 2.8 * fs) { pl = std::max (pl, std::abs (l)); pr = std::max (pr, std::abs (r)); } }
        const double gl = 20 * std::log10 (pl), gr = 20 * std::log10 (pr / 0.01);
        std::snprintf (line, sizeof (line), "a loud left takes a quiet right down with it (%.2f and %.2f dB; the right cell is 3 %% more sensitive)", gl, gr); check (gl < -6 && std::abs (gl - gr) < 0.4 && std::abs (gl - gr) > 0.01, line);
        {
            Compressor a1, a2; a1.prepare (192000.0, a, b); a2.prepare (192000.0, a, b); a1.set (4.0, limitBias()); a2.set (4.0, limitBias()); double worstL = 0, worstR = 0;
            for (int i = 0; i < 192000; ++i) { double l = std::sin (2 * pi * 1000.0 * i / 192000.0), r = 0.3 * std::sin (2 * pi * 700.0 * i / 192000.0); const double l2 = a2.left (l, i % 4), r2 = a2.right (r, i % 4); a1.process (l, r); worstL = std::max (worstL, std::abs (l - l2)); worstR = std::max (worstR, std::abs (r - r2)); }
            std::snprintf (line, sizeof (line), "taken a channel at a time (as the plug-in's oversampler hands them over) it is the same compressor (largest difference %.1e left, %.1e right)", worstL, worstR); check (worstL < 1e-12 && worstR < 1e-12, line);
        }
        const double r44 = steady (44100.0, 4.0, 0.0, 0.0), r48 = steady (48000.0, 4.0, 0.0, 0.0), r192 = steady (192000.0, 4.0, 0.0, 0.0);
        std::snprintf (line, sizeof (line), "the same compression at 44.1, 48 and 192 kHz (%.3f, %.3f, %.3f dB)", r44, r48, r192); check (std::abs (r44 - r192) < 0.05 && std::abs (r48 - r192) < 0.05, line);
    }
    std::printf ("== Distortion and aliasing ==\n");
    {
        double fs = 48000.0; const int N = 48000;
        auto spectrum = [&] (double hz, double drive, double bias, double& fund, double& h2, double& h3, double& worstOther)
        {
            Compressor c; c.prepare (fs, {}, {}); c.set (drive, bias); std::vector<double> y ((size_t) N); for (int i = 0; i < (int) (8 * fs); ++i) c.process (std::sin (2 * pi * hz * i / fs));
            for (int i = 0; i < N; ++i) y[(size_t) i] = c.process (std::sin (2 * pi * hz * (i + (int) (8 * fs)) / fs));
            auto bin = [&] (double f) { std::complex<double> acc = 0; for (int i = 0; i < N; ++i) acc += y[(size_t) i] * std::polar (1.0, -2 * pi * f * i / fs); return 20 * std::log10 (2 * std::abs (acc) / N + 1e-300); };
            fund = bin (hz); h2 = bin (2 * hz) - fund; h3 = bin (3 * hz) - fund; worstOther = -400;
            for (int k = 2; k <= 40; ++k) { double f = std::fmod (k * hz, fs); if (f > fs / 2) f = fs - f; if (k * hz > fs / 2 && std::abs (f - hz) > 1.0) worstOther = std::max (worstOther, bin (f) - fund); }      // harmonics folded back from above half the rate
        };
        double f, h2, h3, al; spectrum (1000.0, 4.0, 0.0, f, h2, h3, al);
        std::snprintf (line, sizeof (line), "1 kHz at %.1f dB of reduction: 2nd harmonic %.0f dB, 3rd %.1f dB below the tone (the light flickers at twice the tone's rate, far too fast for the cell to follow)", -f, h2, h3); check (h3 < -60 && h2 < -120, line);
        double f50, a50, b50, c50; spectrum (50.0, 4.0, 0.0, f50, a50, b50, c50);
        std::snprintf (line, sizeof (line), "50 Hz: 3rd harmonic %.1f dB - the cell half-follows a slow wave, so bass is bent more (as in such a compressor)", b50); check (b50 > h3 + 10 && b50 < -30, line);
        double worst1 = -400, worst4 = -400;
        for (double hz : { 5000.0, 9000.0, 15000.0 }) for (int mode = 0; mode < 2; ++mode) { double ff, x2, x3, o; spectrum (hz, 16.0, mode ? limitBias() : 0.0, ff, x2, x3, o); worst1 = std::max (worst1, o); fs = 192000.0; spectrum (hz, 16.0, mode ? limitBias() : 0.0, ff, x2, x3, o); worst4 = std::max (worst4, o); fs = 48000.0; }
        std::snprintf (line, sizeof (line), "aliasing, 5 / 9 / 15 kHz tones compressed hard, both modes: the strongest folded-back tone is %.0f dB down at 4x (it would be %.0f dB with no oversampling)", -worst4, -worst1); check (worst4 < -100.0, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
