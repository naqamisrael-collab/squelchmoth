// Known-answer test for SqPlate (the thin plate as the sum of its own modes). No dependencies.
// Reference: the same equation stepped directly in time on a grid (tests/reference/plate), which knows nothing
// about modes. The rest is checked against the equation's own arithmetic.
#include <cstdio>
#include "SqPlate.h"
using namespace SqPlate;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
#include "reference/plate/reference.inc"
static const double pi = 3.14159265358979323846;
static char line[800];

static double against (double fs, double topHz, int& modes)
{
    const int n = (int) (sizeof (ref_out) / sizeof (ref_out[0])), os = (int) (fs / 192000.0); Plate p; p.lx = ref_lx; p.ly = ref_ly; p.thickness = ref_h; p.youngs = ref_e; p.density = ref_rho; p.poisson = ref_nu;
    Points at; at.driveX = ref_dx; at.driveY = ref_dy; at.leftX = ref_px; at.leftY = ref_py; Loss loss; loss.seconds = 6.907755278982137 / ref_sigma; loss.halfAtHz = 1e12; Cast c; modes = c.design (p, at, loss, fs, 0.0, topHz); std::vector<float> l, r; c.tap (l, r, 120.0, 0.03);
    const int flen = 400 * os; std::vector<double> f ((size_t) flen + 1); for (int i = 0; i <= flen; ++i) { const double x = (i / fs - ref_t0) / ref_width; f[(size_t) i] = -x * std::exp (-0.5 * x * x); }
    double peak = 0, worst = 0; const double scale = 4.0 / (ref_lx * ref_ly) / fs;
    for (int i = 0; i < n; ++i) { double y = 0; for (int j = 0; j <= std::min (i * os, flen); ++j) if (i * os - j < (int) l.size()) y += f[(size_t) j] * l[(size_t) (i * os - j)]; y *= scale; peak = std::max (peak, std::abs (ref_out[i])); worst = std::max (worst, std::abs (y - ref_out[i])); }
    return 20 * std::log10 (worst / peak);
}

int main()
{
    std::printf ("== A small plate against the equation stepped directly in time (30 x 20 cm, half a millimetre of steel) ==\n");
    {
        int m1 = 0, m2 = 0; const double e1 = against (192000.0, 94000.0, m1), e2 = against (768000.0, 376000.0, m2);
        std::snprintf (line, sizeof (line), "a push at one point, the motion at another over 25 ms: the sum of the %d modes below 94 kHz follows the direct solution to %.0f dB below the peak, sample for sample", m1, -e1); check (e1 < -44.0, line);
        std::snprintf (line, sizeof (line), "and closer still with more modes - %d of them, to 376 kHz: %.0f dB. What is left is the faint give of modes far above hearing, which the plug-in leaves out", m2, -e2); check (e2 < e1 - 4.0 && e2 < -52.0, line);
    }
    std::printf ("== The classic plate's arithmetic (2 m x 1 m x 0.5 mm steel) ==\n");
    const Plate p = classic(); Loss loss; loss.seconds = 2.0; loss.halfAtHz = 1e12; Cast c; const int n = c.design (p, {}, loss, 48000.0);
    {
        const double predicted = p.modesPerHz() * (20000.0 - 20.0);
        std::snprintf (line, sizeof (line), "%d modes between 20 Hz and 20 kHz; area / (2 kappa) says %.3f in every hertz, %.0f in all (the count runs a little under, as it does for any plate with held edges); the lowest sits at %.2f Hz", n, p.modesPerHz(), predicted, p.hz (1, 1));
        check (n > 0.985 * predicted && n < predicted && std::abs (p.hz (1, 1) - 0.5 * pi * p.kappa() * 1.25) < 1e-9, line);
        int lowBand = 0, highBand = 0; for (int i = 0; i < n; ++i) { if (c.hzs[(size_t) i] >= 1000 && c.hzs[(size_t) i] < 2000) ++lowBand; if (c.hzs[(size_t) i] >= 15000 && c.hzs[(size_t) i] < 16000) ++highBand; }
        std::snprintf (line, sizeof (line), "they are spread evenly: %d between 1 and 2 kHz, %d between 15 and 16 kHz - unlike a spring, no pitch is kept waiting, so there is no chirp", lowBand, highBand); check (std::abs (lowBand - highBand) < 0.03 * lowBand && lowBand > 1250, line);
    }
    std::printf ("== The tap ==\n");
    {
        std::vector<float> l, r; c.tap (l, r, 90.0);
        auto rms = [&] (const std::vector<float>& y, double a, double b) { double s = 0; int k = 0; for (int i = (int) (a * 48000); i < (int) (b * 48000) && i < (int) y.size(); ++i) { s += (double) y[(size_t) i] * y[(size_t) i]; ++k; } return 10 * std::log10 (s / std::max (1, k)); };
        const double fall = rms (l, 0.2, 0.3) - rms (l, 1.2, 1.3);
        std::snprintf (line, sizeof (line), "with every mode given a 2 s decay the tap falls %.2f dB in a second (30 is 60 dB in 2 s), and it is %.2f s long before the last mode is 90 dB down", fall, l.size() / 48000.0); check (std::abs (fall - 30.0) < 0.5 && std::abs (l.size() / 48000.0 - 3.0) < 0.01, line);
        double el = 0, er = 0, cross = 0; for (size_t i = 0; i < l.size(); ++i) { el += (double) l[i] * l[i]; er += (double) r[i] * r[i]; cross += (double) l[i] * r[i]; }
        std::snprintf (line, sizeof (line), "its strength is what the modes' sum predicts (%.0f against %.0f left, %.0f against %.0f right): the plug-in sets its level from this, so a longer decay is longer, not louder", std::sqrt (el), c.level (false), std::sqrt (er), c.level (true));
        check (std::abs (std::sqrt (el) / c.level (false) - 1.0) < 0.03 && std::abs (std::sqrt (er) / c.level (true) - 1.0) < 0.03, line);
        std::snprintf (line, sizeof (line), "the two pickups hear different sums of the same modes: their taps are alike by only %.3f (1 would be the same sound, 0 unrelated) - that is the stereo", cross / std::sqrt (el * er)); check (std::abs (cross / std::sqrt (el * er)) < 0.05, line);
        std::vector<float> l2, r2; c.tap (l2, r2, 140.0); double worst = 0, peak = 0; for (size_t i = 0; i < l.size(); ++i) { worst = std::max (worst, (double) std::abs (l[i] - l2[i])); peak = std::max (peak, (double) std::abs (l2[i])); }
        std::snprintf (line, sizeof (line), "dropping each mode once it is 90 dB down changes the tap by %.0f dB below its peak at most", -20 * std::log10 (worst / peak)); check (worst < 1.0e-3 * peak, line);
        Loss shaped; shaped.seconds = 2.0; shaped.halfAtHz = 4000.0; Cast c2; c2.design (p, {}, shaped, 48000.0); std::vector<float> a, b; c2.tap (a, b, 90.0);
        // high modes die sooner: the tap's zero crossings thin out as it rings down
        auto crossings = [&] (double from, double to) { int k = 0; for (int i = (int) (from * 48000); i < (int) (to * 48000); ++i) if ((a[(size_t) i] < 0) != (a[(size_t) i + 1] < 0)) ++k; return k / (to - from) / 2.0; };
        const double early = crossings (0.02, 0.12), late = crossings (1.5, 1.6);
        std::snprintf (line, sizeof (line), "with the decay halving by 4 kHz the tap darkens as it rings: its mean pitch (from zero crossings) is %.0f Hz just after the tap and %.0f Hz a second and a half later", early, late); check (late < 0.6 * early && early > 3000.0, line);
        // bending waves: high notes travel faster than low ones (the opposite of a spring), at twice sqrt(2 pi f kappa)
        {
            // (on a plate 6 m by 5 m, so that the edges' echoes come late enough to tell the first arrival apart)
            Plate big; big.lx = 6.0; big.ly = 5.0; Points mid; mid.driveX = 0.5; mid.driveY = 0.5; mid.leftX = 0.5 - 0.84 / 6.0; mid.leftY = 0.5;
            const double fs = 48000.0, dist = 0.84; char d[200] = ""; size_t used = 0; bool ok = true; double prev = 1e9; Loss flat; flat.seconds = 2.0; flat.halfAtHz = 1e12; Cast cf; const int nBig = cf.design (big, mid, flat, fs, 20.0, 12000.0); std::vector<float> tl, tr; cf.tap (tl, tr, 60.0, 0.03);
            for (double f : { 500.0, 2000.0, 8000.0 })
            {
                const double want = dist / (2.0 * std::sqrt (2 * pi * f * big.kappa())), wdt = 0.0008, t0 = 0.003, aa = std::exp (-1.0 / (0.0004 * fs)); double er = 0, ei = 0, best = 0, at = 0;
                for (int i = 0; i < (int) (0.03 * fs); ++i) { double y = 0; for (int j = std::max (0, i - 600); j <= i; ++j) { const double t = j / fs, x = (t - t0) / wdt; if (std::abs (x) < 5 && i - j < (int) tl.size()) y += std::exp (-0.5 * x * x) * std::sin (2 * pi * f * t) * tl[(size_t) (i - j)]; }
                    const double t = i / fs; er = aa * er + (1 - aa) * y * std::cos (2 * pi * f * t); ei = aa * ei + (1 - aa) * y * std::sin (2 * pi * f * t); const double env = er * er + ei * ei, arr = t - t0 - 0.0004; if (arr > 0.4 * want && arr < 1.3 * want + 0.002 && env > best) { best = env; at = arr; } }
                ok = ok && std::abs (at - want) < 0.15 * want && at < prev; prev = at; used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.0f Hz %.2f ms (%.2f)", f, at * 1000, want * 1000);
            }
            std::snprintf (line, sizeof (line), "a burst crosses %.0f cm of plate sooner the higher its pitch:%s - in brackets, the distance over the speed bending waves have at that pitch (%d modes of a 6 m plate summed)", dist * 100, d, nBig); check (ok, line);
        }
        Cast c44, c96; const int n44 = c44.design (p, {}, shaped, 44100.0), n96 = c96.design (p, {}, shaped, 96000.0);
        std::snprintf (line, sizeof (line), "the same %d modes at 44.1 and 96 kHz: nothing is tuned to the sample rate", n44); check (n44 == n96 && n44 == n, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
