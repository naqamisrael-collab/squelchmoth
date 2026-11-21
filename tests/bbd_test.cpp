// Known-answer test for SqBbd (the bucket-brigade line with its filters). No dependencies.
// Two references: ngspice 42 running the chain as a continuous-time circuit, and a brute-force simulation at 7.68 MHz
// (tests/reference/bbd). The rest are checked against what the arithmetic of a sampler says they must be.
#include <cstdio>
#include <vector>
#include "SqBbd.h"
using namespace SqBbd;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
#include "reference/bbd/reference.inc"
#include "reference/bbd/bruteforce.inc"
static const double pi = 3.14159265358979323846;
static char line[800];
static double butter (int order, double f, double fc) { return 1.0 / std::sqrt (1.0 + std::pow (f / fc, 2.0 * order)); }
static double sinc (double x) { return std::abs (x) < 1e-12 ? 1.0 : std::sin (pi * x) / (pi * x); }
// level of one frequency in y (dB), over a whole number of cycles at the end
static double level (const std::vector<double>& y, double fs, double f, int n) { cd acc = 0; const int from = (int) y.size() - n; for (int i = 0; i < n; ++i) acc += y[(size_t) (from + i)] * std::polar (1.0, -2 * pi * f * (from + i) / fs); return 20 * std::log10 (2 * std::abs (acc) / n + 1e-300); }

int main()
{
    std::printf ("== The line against two references (two tones, one above half the clock rate; 64 stages at 20 kHz) ==\n");
    {
        const int n = (int) (sizeof (ref_out) / sizeof (ref_out[0])); double peak = 0; for (int i = 0; i < n; ++i) peak = std::max (peak, std::abs (ref_out[i]));
        double w[3] = {}, b[3] = {}; int k = 0;
        for (double fs : { 192000.0, 96000.0, 48000.0 })
        {
            Line l; l.prepare (fs, (int) ref_stages, ref_cutoff); const int step = (int) (192000 / fs);
            for (int i = 0; i * step < n; ++i) { const double t = i / fs, y = l.process (ref_a1 * std::sin (2 * pi * ref_f1 * t) + ref_a2 * std::sin (2 * pi * ref_f2 * t), ref_clock); if (t > 0.003) { w[k] = std::max (w[k], std::abs (y - ref_out[i * step])); b[k] = std::max (b[k], std::abs (y - brute_out[i * step])); } }
            ++k;
        }
        auto db = [&] (double e) { return 20 * std::log10 (e / peak); };
        std::snprintf (line, sizeof (line), "at 192 kHz the output follows the brute-force simulation to %.0f dB below the signal's peak and the circuit simulator to %.0f dB, sample for sample over 37 ms", -db (b[0]), -db (w[0])); check (db (b[0]) < -65 && db (w[0]) < -54, line);
        std::snprintf (line, sizeof (line), "at 96 kHz to %.0f dB and at 48 kHz to %.0f dB (brute force) - the 13 kHz tone is too coarsely drawn at 48 kHz, which is why the plug-in runs the line at four times the rate", -db (b[1]), -db (b[2])); check (db (b[1]) < -55 && db (b[2]) < -40, line);
    }
    std::printf ("== Delay = stages / (2 x clock) ==\n");
    {
        const double fs = 192000.0, clock = 40000.0; auto run = [&] (int stages) { Line l; l.prepare (fs, stages, 8000.0); l.buckets.a = l.buckets.b = 0.0; std::vector<double> y; for (int i = 0; i < 96000; ++i) { const double t = i / fs; y.push_back (l.process (0.2 * std::sin (2 * pi * 500.0 * t) * (0.5 - 0.5 * std::cos (2 * pi * 20.0 * t)), clock)); } return y; };
        const auto a = run (1024), b = run (4096); double best = -1e300; int at = 0; auto corr = [&] (int lag) { double s = 0; for (int i = 30000; i < 60000; ++i) s += a[(size_t) i] * b[(size_t) (i + lag)]; return s; };
        const int expect = (int) std::lround ((4096 - 1024) / (2.0 * clock) * fs); for (int lag = expect - 40; lag <= expect + 40; ++lag) { const double c = corr (lag); if (c > best) { best = c; at = lag; } }
        const double c0 = corr (at - 1), c1 = corr (at), c2 = corr (at + 1), frac = 0.5 * (c0 - c2) / (c0 - 2 * c1 + c2), got = (at + frac) / fs * 1000.0, want = (4096 - 1024) / (2.0 * clock) * 1000.0;
        std::snprintf (line, sizeof (line), "4096 stages against 1024 at a 40 kHz clock: %.4f ms more delay (stages / (2 x clock) says %.4f)", got, want); check (std::abs (got - want) < 0.002, line);
        Line l; l.prepare (fs, 4096, 8000.0); std::snprintf (line, sizeof (line), "4096 stages: %.2f ms at 100 kHz and %.1f ms at 10 kHz, the data sheet's range", l.delaySeconds (100000.0) * 1000, l.delaySeconds (10000.0) * 1000); check (std::abs (l.delaySeconds (100000.0) - 0.02048) < 1e-9 && std::abs (l.delaySeconds (10000.0) - 0.2048) < 1e-9, line);
    }
    std::printf ("== The buckets' distortion (data sheet: 0.8 %% at 0.25 V rms, 2.5 %% at 0.36 V rms) ==\n");
    {
        auto thd = [&] (double vrms) { Buckets k; const int N = 4800; cd a[8] = {}; for (int i = 0; i < N; ++i) { const double y = bend (k, vrms * std::sqrt (2.0) * std::sin (2 * pi * 10.0 * i / N)); for (int h = 1; h < 8; ++h) a[h] += y * std::polar (1.0, -2 * pi * 10.0 * h * i / N); } double s = 0; for (int h = 2; h < 8; ++h) s += std::norm (a[h]); return 100.0 * std::sqrt (s / std::norm (a[1])); };
        const double t1 = thd (0.25), t2 = thd (0.36); std::snprintf (line, sizeof (line), "a tone at 0.25 V rms measures %.2f %% and at 0.36 V rms %.2f %%", t1, t2); check (std::abs (t1 - 0.8) < 0.03 && std::abs (t2 - 2.5) < 0.08, line);
        Buckets k; bool rising = true; double prev = -10; for (double x = -1.0; x <= 1.0; x += 0.001) { const double y = bend (k, x); rising = rising && y >= prev; prev = y; }
        std::snprintf (line, sizeof (line), "pushed past its limit it holds at %.3f V and never folds back", bend (k, 5.0)); check (rising && bend (k, 5.0) > 0.4 && bend (k, 5.0) < 0.62, line);
    }
    std::printf ("== A sampler's arithmetic ==\n");
    {
        const double fs = 192000.0; auto tone = [&] (double f, double clock, double cutoff, double measure, int stages = 256) { Line l; l.prepare (fs, stages, cutoff); l.buckets.a = l.buckets.b = 0.0; std::vector<double> y; for (int i = 0; i < 192000; ++i) y.push_back (l.process (0.1 * std::sin (2 * pi * f * i / fs), clock)); return level (y, fs, measure, 96000) - 20 * std::log10 (0.1); };
        double worst = 0; char d[200] = ""; size_t used = 0;
        for (double clock : { 10000.0, 25000.0, 100000.0 }) { const double f = 3000.0, got = tone (f, clock, 9000.0, f), want = 20 * std::log10 (butter (3, f, 9000.0) * butter (5, f, 9000.0) * sinc (f / clock)); worst = std::max (worst, std::abs (got - want)); used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.2f (%.2f)", got, want); }
        std::snprintf (line, sizeof (line), "a slower clock is a darker sound - each sample is held for a whole clock period: 3 kHz comes back at%s dB at 10, 25 and 100 kHz (in brackets: the two filters times the hold's own roll-off)", d); check (worst < 0.05, line);
        const double clock = 20000.0, f = 14000.0, cutoff = 9000.0, got = tone (f, clock, cutoff, clock - f), want = 20 * std::log10 (butter (3, f, cutoff) * sinc ((clock - f) / clock) * butter (5, clock - f, cutoff));
        std::snprintf (line, sizeof (line), "a 14 kHz tone into a 20 kHz clock comes back as 6 kHz at %.2f dB (the arithmetic says %.2f; the shortfall is the straight line drawn between samples of so high a tone): the chip's own fold-over, part of its sound", got, want); check (std::abs (got - want) < 0.2, line);
        const double img = tone (3000.0, 20000.0, 9000.0, 17000.0), wantImg = 20 * std::log10 (butter (3, 3000.0, 9000.0) * sinc (17000.0 / 20000.0) * butter (5, 17000.0, 9000.0));
        std::snprintf (line, sizeof (line), "and a 3 kHz tone leaves its staircase image at 17 kHz at %.1f dB (%.1f)", img, wantImg); check (std::abs (img - wantImg) < 0.3, line);
    }
    std::printf ("== A wobbling clock is a wobbling pitch ==\n");
    {
        const double fs = 192000.0, f = 1000.0, clock = 50000.0, rate = 5.0, depth = 0.05; const int stages = 1024; Line l; l.prepare (fs, stages, 10000.0); l.buckets.a = l.buckets.b = 0.0; std::vector<double> y;
        for (int i = 0; i < 2 * 192000; ++i) y.push_back (l.process (0.2 * std::sin (2 * pi * f * i / fs), clock * (1.0 + depth * std::sin (2 * pi * rate * i / fs))));
        // frequency from the spacing of upward zero crossings
        std::vector<double> zc; for (size_t i = 96000; i + 1 < y.size(); ++i) if (y[i] <= 0 && y[i + 1] > 0) zc.push_back (i + y[i] / (y[i] - y[i + 1]));
        double lo = 1e9, hi = 0, mean = 0; for (size_t i = 8; i < zc.size(); ++i) { const double fr = 8.0 * fs / (zc[i] - zc[i - 8]); lo = std::min (lo, fr); hi = std::max (hi, fr); mean += fr; } mean /= (zc.size() - 8);
        const double tau = stages / (2.0 * clock), predicted = f * 2 * pi * rate * tau * depth * sinc (rate * tau);
        std::snprintf (line, sizeof (line), "a 1 kHz tone through 1024 stages with the clock wobbling 5 %% five times a second swings %.2f Hz down and %.2f Hz up (first-order prediction %.2f) round %.3f Hz", f - lo, hi - f, predicted, mean);
        check (std::abs ((hi - lo) / 2 - predicted) < 0.08 * predicted && std::abs (mean - f) < 0.05, line);
    }
    std::printf ("== What the host's own sampling adds ==\n");
    {
        auto extra = [&] (double fs) { Line l; l.prepare (fs, 256, 12000.0); const double f = 5000.0, clock = 100000.0; std::vector<double> y; const int N = (int) fs; for (int i = 0; i < 2 * N; ++i) y.push_back (l.process (0.25 * std::sin (2 * pi * f * i / fs), clock)); const double fund = level (y, fs, f, N); double worst = -400;
            for (int k = 1; k <= 4; ++k) for (int m = -5; m <= 5; ++m) { const double real = k * clock + m * f; if (real < fs / 2 || m == 0) continue; double fold = std::fmod (real, fs); if (fold > fs / 2) fold = fs - fold; if (std::fmod (fold, f) < 1.0 || f - std::fmod (fold, f) < 1.0) continue; worst = std::max (worst, level (y, fs, fold, N) - fund); } return worst; };
        const double e4 = extra (192000.0), e1 = extra (48000.0);
        std::snprintf (line, sizeof (line), "with the filters wide open (12 kHz) and the clock at 100 kHz, images folded by the host's sampling are %.0f dB down at 4x (%.0f dB at the plain rate)", -e4, -e1); check (e4 < -100.0, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
