// Known-answer test for SqSpring (the helical spring as a bank of its own modes). No dependencies.
// Reference: the same published equations stepped directly in time on a fine grid (tests/reference/spring), which
// knows nothing about modes. The rest is checked against the arithmetic of the equations.
#include <cstdio>
#include <random>
#include "SqSpring.h"
using namespace SqSpring;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
#include "reference/spring/reference.inc"
static const double pi = 3.14159265358979323846;
static char line[800];

int main()
{
    std::printf ("== A short spring against the equations stepped directly in time ==\n");
    {
        const double fs = 192000; const int n = (int) (sizeof (ref_out) / sizeof (ref_out[0])); Spring sp; sp.coilDiameter = ref_coil; sp.wireDiameter = ref_wire; sp.helixLength = ref_helix; sp.turns = ref_turns;
        Loss loss; loss.seconds = 6.907755278982137 / ref_sigma; loss.halfAtHz = 1e12; Bank b; const int modes = b.design (sp, loss, fs, 0.0, 94000.0); const double L = sp.wireLength(); double rigid = 0, peak = 0, worst = 0, first = 0;
        for (int i = 0; i < n; ++i) { const double t = i / fs, x = (t - ref_t0) / ref_width, f = -x * std::exp (-0.5 * x * x); rigid = rigid * std::exp (-2 * ref_sigma / fs) + f; const double y = (2.0 / L) / fs * b.process ((float) f) + (1.0 / L) / fs * rigid; peak = std::max (peak, std::abs (ref_out[i])); worst = std::max (worst, std::abs (y - ref_out[i])); if (i < 1920) first = worst; }
        std::snprintf (line, sizeof (line), "a push at one end, the other end's motion over 30 ms (five echoes): the %d modes follow the direct solution to %.0f dB below the peak, sample for sample (%.0f dB over the first 10 ms)", modes, -20 * std::log10 (worst / peak), -20 * std::log10 (first / peak));
        check (20 * std::log10 (worst / peak) < -55.0, line);
    }
    std::printf ("== The equations' own arithmetic (a real tank's first spring: 303 turns, 4.4 mm coil, 0.35 mm wire) ==\n");
    const Spring sp = longSpring (0); const double L = sp.wireLength(), c = sp.c(), r = sp.r(), k = sp.k();
    {
        double worst = 0, lowestHigh = 1e300;
        for (int n = 1; n <= 1200; ++n) { const double b = n * pi / L, w = sp.omega (b), c2 = c * c, S = 0.25 * c2 * r * r * (b * b - k * k) * (b * b - k * k); const double lhs = (w * w - c2 * b * b) * (w * w - c2 * k * k - S), rhs = c2 * c2 * k * k * b * b; worst = std::max (worst, std::abs (lhs - rhs) / rhs);
            const double B = c2 * b * b + c2 * k * k + S; lowestHigh = std::min (lowestHigh, std::sqrt (B - w * w) / (2 * pi)); }
        std::snprintf (line, sizeof (line), "every mode satisfies the wave relation (worst mismatch %.1e); the other, fast branch starts at %.0f kHz, far beyond hearing, and is left out", worst, lowestHigh / 1000); check (worst < 1e-6 && lowestHigh > 100000.0, line);
        const double spacing = (sp.omega (2 * pi / L) - sp.omega (pi / L)) / (2 * pi), echo = sp.echoSeconds();
        std::snprintf (line, sizeof (line), "long waves: the lowest modes sit %.3f Hz apart, so the echoes come %.2f ms apart; the published formula 4 L R / (r c) gives %.2f ms", spacing, 1000.0 / spacing, echo * 1000); check (std::abs (1.0 / spacing - echo) < 0.003 * echo, line);
        const double fc = sp.transitionHz(), firstOrder = 3.0 * r * c / (16.0 * std::sqrt (5.0) * pi * (0.5 * sp.coilDiameter) * (0.5 * sp.coilDiameter));
        std::snprintf (line, sizeof (line), "the transition frequency, where waves stop travelling: %.0f Hz by search; the published first-order formula gives %.0f Hz", fc, firstOrder); check (std::abs (fc - firstOrder) < 0.02 * firstOrder, line);
    }
    std::printf ("== Every echo is a chirp ==\n");
    {
        const double fs = 48000; Loss loss; loss.seconds = 3.0; Bank bank; bank.design (sp, loss, fs); char d[300] = ""; size_t used = 0; bool ok = true; double prevT = 0;
        for (double f : { 500.0, 2000.0, 3500.0, 4200.0 })
        {
            // the wavenumber on the first rise of the slow branch with this frequency, and the speed of a packet there
            double lo = 0, hi = 0; { double a = 0, b = k; for (int i = 0; i < 80; ++i) { const double m1 = a + (b - a) / 3, m2 = b - (b - a) / 3; if (sp.omega (m1) < sp.omega (m2)) a = m1; else b = m2; } hi = 0.5 * (a + b); }
            for (int i = 0; i < 80; ++i) { const double mid = 0.5 * (lo + hi); (sp.omega (mid) < 2 * pi * f ? lo : hi) = mid; }
            const double db = 1e-3 * k, vg = (sp.omega (lo + db) - sp.omega (lo - db)) / (2 * db), want = L / vg;
            bank.reset(); const double width = 0.0012, t0 = 0.005; const int N = (int) ((want + 0.05) * fs); double best = 0, at = 0; double er = 0, ei = 0; const double a = std::exp (-1.0 / (0.0005 * fs));
            for (int i = 0; i < N; ++i) { const double t = i / fs, x = (t - t0) / width, y = bank.process ((float) (std::exp (-0.5 * x * x) * std::sin (2 * pi * f * t))); er = a * er + (1 - a) * y * std::cos (2 * pi * f * t); ei = a * ei + (1 - a) * y * std::sin (2 * pi * f * t); const double env = er * er + ei * ei; const double arrive = t - t0 - 0.0005; if (arrive > 0.8 * want && arrive < 1.25 * want && env > best) { best = env; at = arrive; } }
            ok = ok && std::abs (at - want) < 0.05 * want && want > prevT; prevT = want; used += (size_t) std::snprintf (d + used, sizeof (d) - used, " %.0f Hz %.1f ms (%.1f)", f, at * 1000, want * 1000);
        }
        std::snprintf (line, sizeof (line), "a short burst arrives at the far end later the higher its pitch:%s - in brackets, the wire's length over the packet's speed from the wave relation", d); check (ok, line);
    }
    std::printf ("== Decay, level, rate ==\n");
    {
        const double fs = 48000; Loss loss; loss.seconds = 2.0; loss.halfAtHz = 4000.0; Bank bank; bank.design (sp, loss, fs);
        // one mode on its own (the bank is asked for a 2 Hz slice of the spring), struck and timed
        auto slope = [&] (double near, double& f) { Bank one; int n = 0; for (double lo = near; lo < near + 60.0 && n != 1; lo += 0.5) n = one.design (sp, loss, fs, lo, lo + 2.0); if (n != 1) return 0.0; f = one.hz[0]; double early = 0, late = 0;
            for (int i = 0; i < (int) (1.25 * fs); ++i) { one.process (i == 0 ? 1.0f : 0.0f); const double mag = std::sqrt ((double) one.re[0] * one.re[0] + (double) one.im[0] * one.im[0]); if (i == (int) (0.25 * fs)) early = mag; if (i == (int) (1.25 * fs) - 1) late = mag; } return -20 * std::log10 (late / early); };
        double f1 = 0, f2 = 0; const double s500 = slope (500.0, f1), s4k = slope (4000.0, f2), w1 = 30.0 * (1.0 + (f1 / 4000.0) * (f1 / 4000.0)), w2 = 30.0 * (1.0 + (f2 / 4000.0) * (f2 / 4000.0));
        std::snprintf (line, sizeof (line), "with a 2 s decay asked for and the loss doubling by 4 kHz: the mode at %.1f Hz dies at %.2f dB a second (the law says %.2f) and the one at %.1f Hz at %.2f (%.2f)", f1, s500, w1, f2, s4k, w2);
        check (std::abs (s500 - w1) < 0.05 && std::abs (s4k - w2) < 0.1 && s500 > 29.0, line);
        bank.reset(); std::mt19937 rng (7); std::normal_distribution<double> g (0.0, 1.0); double sum = 0; const int N = 480000; for (int i = 0; i < N; ++i) { const double y = bank.process ((float) g (rng)); if (i > 240000) sum += y * y; }
        const double measured = std::sqrt (sum / (N - 240001)), predicted = bank.level();
        std::snprintf (line, sizeof (line), "noise in, the level out is what the modes' sum predicts (%.0f against %.0f) - used to set the plug-in's level", measured, predicted); check (std::abs (measured / predicted - 1.0) < 0.06, line);
        Bank b44, b96; const int n44 = b44.design (sp, loss, 44100.0), n96 = b96.design (sp, loss, 96000.0); double worst = 0; for (int i = 0; i < std::min (n44, n96); ++i) worst = std::max (worst, (double) std::abs (b44.hz[(size_t) i] - b96.hz[(size_t) i]));
        std::snprintf (line, sizeof (line), "the same %d modes at 44.1 and 96 kHz, to the hertz (largest difference %.4f Hz): nothing is tuned to the sample rate, and being a sum of resonators it cannot alias", n44, worst); check (n44 == n96 && worst < 0.01 && n44 > 800, line);
        bank.design (sp, loss, fs); bank.reset(); double biggest = 0, late = 0; for (int i = 0; i < 20 * 48000; ++i) { const double y = bank.process (i == 0 ? 1.0f : 0.0f); biggest = std::max (biggest, std::abs (y)); if (i > 19 * 48000) late = std::max (late, std::abs (y)); }
        std::snprintf (line, sizeof (line), "struck once it rings down and stays down: %.0f dB after twenty seconds", 20 * std::log10 (late / biggest + 1e-30)); check (late < 1e-6 * biggest, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
