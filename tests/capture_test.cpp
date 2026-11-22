// Known-answer test for SqCapture (sweep a unit, fit a model, play it). No dependencies.
// The known answer: a made-up unit that IS a sum of filtered powers, whose filters the fit must give back; then units
// that are not (the overdrive stage's circuit; a filter - soft curve - filter chain), judged on programme material.
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include "SqCapture.h"
#include "SqDrive.h"
using namespace SqCapture;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static char line[1000];
using Unit = std::function<double (double)>;
static const double fs = 48000.0;

struct OnePole { double a = 0, z = 0; bool high = false; void set (double hz, bool hp) { a = 1.0 - std::exp (-2 * pi * hz / fs); high = hp; z = 0; } double run (double x) { z += a * (x - z); return high ? x - z : z; }
                 double mag (double f) const { const cd e = std::polar (1.0, -2 * pi * f / fs), lp = a / (1.0 - (1.0 - a) * e); return std::abs (high ? cd (1.0) - lp : lp); } };

/** The made-up unit: 1.0 x (40 Hz high-pass, 12 kHz low-pass) u  +  0.10 x (3 kHz low-pass) u^2  -  0.15 x (200 Hz high-pass) u^3. */
struct Known
{
    Known (double levelIn) : level (levelIn) { for (auto& o : os) o.prepare (8); a.set (40, true); b.set (12000, false); c.set (3000, false); d.set (200, true); delay.assign (96, 0.0); }
    double run (double x)
    {
        const double u = x / level, lin = delay[(size_t) at]; delay[(size_t) at] = u; if (++at >= 96) at = 0;
        const double sq = os[0].process (u, [] (double p) { return p * p; }), cu = os[1].process (u, [] (double p) { return p * p * p; });
        return b.run (a.run (lin)) + 0.10 * c.run (sq) - 0.15 * d.run (cu);
    }
    double level; SqTube::Oversampler os[2]; OnePole a, b, c, d; std::vector<double> delay; int at = 0;
};
static double kernelMag (const Model& m, int k, double hz) { cd acc = 0; const auto& g = m.g[(size_t) (k - 1)]; for (int i = 0; i < m.taps; ++i) acc += (double) g[(size_t) i] * std::polar (1.0, -2 * pi * hz * i / fs); return std::abs (acc); }

static std::vector<float> through (Unit u, const std::vector<float>& x) { std::vector<float> y (x.size()); for (size_t i = 0; i < x.size(); ++i) y[i] = (float) u (x[i]); return y; }
static std::vector<float> programme (double peak)
{
    const int n = 1 << 18; std::vector<double> y ((size_t) n, 0.0); std::mt19937 rng (7); std::uniform_real_distribution<double> uni (-1.0, 1.0); const double notes[8] = { 110, 146.83, 196, 220, 293.66, 392, 440, 659.25 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs; const int step = (int) (t / 0.25); const double tn = t - step * 0.25, f = notes[(step * 5) % 8], tk = std::fmod (t, 0.5);
        double v = 0; for (int h = 1; h <= 6; ++h) v += std::sin (2 * pi * f * h * tn) / h * std::exp (-tn * (3.0 + h)); v *= 0.5;
        v += 0.9 * std::sin (2 * pi * (45.0 + 60.0 * std::exp (-tk * 40.0)) * tk) * std::exp (-tk * 9.0);
        const double th = std::fmod (t + 0.125, 0.25); static double hp = 0; const double nz = uni (rng); hp += 0.6 * (nz - hp); v += 0.25 * (nz - hp) * std::exp (-th * 60.0);
        y[(size_t) i] = v;
    }
    double pk = 0; for (double v : y) pk = std::max (pk, std::abs (v)); std::vector<float> out ((size_t) n); for (int i = 0; i < n; ++i) out[(size_t) i] = (float) (y[(size_t) i] * peak / pk); return out;
}
struct Score { double levelDb = 0, worstBandDb = 0, errorDb = 0, thdUnit = 0, thdModel = 0, worstThdGap = 0; };
static double thd (Unit u, double hz, double amp)
{
    const int n = 24000, m = 9600; std::vector<double> y; for (int i = 0; i < n; ++i) { const double v = u (amp * std::sin (2 * pi * hz * i / fs)); if (i >= n - m) y.push_back (v); }
    double h[8] = {}; for (int k = 1; k <= 7; ++k) { if (k * hz > 0.45 * fs) break; cd a = 0; for (int i = 0; i < m; ++i) a += y[(size_t) i] * std::polar (1.0, -2 * pi * hz * k * i / fs); h[k] = std::abs (a); }
    double s = 0; for (int k = 2; k <= 7; ++k) s += h[k] * h[k]; return 100.0 * std::sqrt (s) / (h[1] + 1e-30);
}
static Score judge (std::function<Unit()> makeUnit, const Model& mo, double peak)
{
    Score s; const auto x = programme (peak); auto unit = makeUnit(); const auto a = through (unit, x); Player p; p.prepare (mo); const auto b = through ([&p] (double v) { return p.process (v); }, x);
    const int shift = p.latency() - mo.latency, n = (int) x.size(); std::vector<cd> A ((size_t) n), B ((size_t) n); double ea = 0, eb = 0, ee = 0;
    for (int i = 4096; i < n; ++i) { const double u = i - shift >= 0 ? a[(size_t) (i - shift)] : 0.0, v = b[(size_t) i]; A[(size_t) i] = u; B[(size_t) i] = v; ea += u * u; eb += v * v; ee += (u - v) * (u - v); }
    s.levelDb = 10 * std::log10 (eb / ea); s.errorDb = 10 * std::log10 (ee / ea); fft (A, false); fft (B, false);
    for (double c : { 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 }) { double pa = 0, pb = 0; for (int k = (int) (c / 1.4142 * n / fs); k < (int) (c * 1.4142 * n / fs); ++k) { pa += std::norm (A[(size_t) k]); pb += std::norm (B[(size_t) k]); } const double d = 10 * std::log10 (pb / pa); if (std::abs (d) > std::abs (s.worstBandDb)) s.worstBandDb = d; }
    for (double hz : { 100.0, 1000.0, 3000.0 }) for (double amp : { peak, 0.5 * peak, 0.25 * peak })
    {
        auto u2 = makeUnit(); Player p2; p2.prepare (mo); const double tu = thd (u2, hz, amp), tm = thd ([&p2] (double v) { return p2.process (v); }, hz, amp);
        if (std::abs (tm - tu) > std::abs (s.worstThdGap)) { s.worstThdGap = tm - tu; s.thdUnit = tu; s.thdModel = tm; }
    }
    return s;
}
static Model capture (std::function<Unit()> makeUnit, double level, int order = 5) { Sweep sw; sw.level = level; const auto x = sw.make(); auto u = makeUnit(); const auto y = through (u, x); return fit (sw, y.data(), (int) y.size(), order); }

int main()
{
    std::printf ("== The sweep ==\n");
    {
        Sweep sw; const auto x = sw.make(); const double l = sw.L(); double pk = 0; for (float v : x) pk = std::max (pk, (double) std::abs (v));
        std::snprintf (line, sizeof (line), "%.2f s rising from %.0f Hz to %.0f kHz by the same ratio every second, then %.1f s of silence, and the same again 12 dB quieter; its time constant %.4f s makes %.0f whole turns at the start (so each harmonic's copy lines up), peak %.3f", sw.sweepSamples() / fs, sw.lowHz, sw.highHz / 1000, sw.tailSeconds, l, l * sw.lowHz, pk);
        check (std::abs (l * sw.lowHz - std::round (l * sw.lowHz)) < 1e-9 && std::abs (pk - sw.level) < 0.001 && std::abs (sw.sweepSamples() / fs - 8.0) < 0.3, line);
        check (std::abs (share (1, 1) - cd (1.0)) < 1e-12 && std::abs (share (2, 2) - cd (0.0, -0.5)) < 1e-12 && std::abs (share (1, 3) - cd (0.75)) < 1e-12 && std::abs (share (3, 3) - cd (-0.25)) < 1e-12 && std::abs (share (2, 3)) < 1e-12,
               "the shares are the textbook ones: sin^2 holds -cos(2x)/2, sin^3 holds 3/4 sin(x) - 1/4 sin(3x)");
    }
    std::printf ("== A unit that is a sum of filtered powers: the fit gives its filters back ==\n");
    {
        const double level = 0.5; auto make = [level] { auto k = std::make_shared<Known> (level); return Unit ([k] (double v) { return k->run (v); }); }; const Model mo = capture (make, level, 5);
        Known ref (level); double w1 = 0, w2 = 0, w3 = 0, rest = 0; 
        for (double hz : { 100.0, 300.0, 1000.0, 3000.0, 8000.0, 15000.0 })
        {
            w1 = std::max (w1, std::abs (20 * std::log10 (kernelMag (mo, 1, hz) / (ref.a.mag (hz) * ref.b.mag (hz)))));
            w2 = std::max (w2, std::abs (20 * std::log10 (kernelMag (mo, 2, hz) / (0.10 * ref.c.mag (hz)))));
            w3 = std::max (w3, std::abs (20 * std::log10 (kernelMag (mo, 3, hz) / (0.15 * ref.d.mag (hz)))));
            rest = std::max (rest, std::max (kernelMag (mo, 4, hz), kernelMag (mo, 5, hz)));
        }
        std::snprintf (line, sizeof (line), "100 Hz to 15 kHz: the 1st power's filter within %.3f dB of the truth, the 2nd's within %.3f dB, the 3rd's within %.3f dB; the 4th and 5th, which the unit does not have, come out at %.0f dB; delay found %d samples (true 96)", w1, w2, w3, 20 * std::log10 (rest + 1e-30), mo.latency);
        check (w1 < 0.1 && w2 < 0.5 && w3 < 0.25 && rest < 0.003 && mo.latency == 96, line);
        const Score s = judge (make, mo, 0.9 * level);
        std::snprintf (line, sizeof (line), "on programme material (notes, kicks and hats peaking just under the sweep's level): level %.3f dB off, worst octave band %.3f dB off, what is left over %.1f dB under the unit's output; distortion of test tones at most %.3f points apart (unit %.3f %%, model %.3f %%)", s.levelDb, s.worstBandDb, s.errorDb, s.worstThdGap, s.thdUnit, s.thdModel);
        check (std::abs (s.levelDb) < 0.1 && std::abs (s.worstBandDb) < 0.1 && s.errorDb < -30.0 && std::abs (s.worstThdGap) < 0.5, line);
    }
    std::printf ("== A circuit: the overdrive stage, whose distortion depends on pitch ==\n");
    for (int which = 0; which < 3; ++which)
    {
        const double rf = which == 2 ? 151000.0 : 51000.0, level = which == 0 ? 0.03 : 0.1;
        auto make = [rf] { struct U { SqDrive::Stage st; SqTube::Oversampler os; }; auto u = std::make_shared<U>(); SqDrive::Circuit c; c.rf = rf; u->st.prepare (fs * 8, {}, c); u->os.prepare (8); return Unit ([u] (double v) { return u->os.process (v, [&u] (double x) { return u->st.process (x); }); }); };
        const Model mo = capture (make, level, 7); const Score s = judge (make, mo, 0.9 * level);
        std::snprintf (line, sizeof (line), "%s, swept at %.0f mV: level %.2f dB off, worst octave band %.2f dB off, left over %.1f dB under; distortion at most %.2f points apart (unit %.2f %%, model %.2f %%); the sweep read its 3rd harmonic at %.1f dB; front filter %s (quiet pass within %.2f dB, %.2f without)",
                       which == 0 ? "least Drive, a quiet signal (barely clipping)" : which == 1 ? "least Drive (clipping)" : "more Drive (clipping hard)", level * 1000, s.levelDb, s.worstBandDb, s.errorDb, s.worstThdGap, s.thdUnit, s.thdModel, mo.harmonicDb[1], mo.front.empty() ? "not used" : "used", mo.quietMissDb, mo.quietMissPlainDb);
        const bool met = std::abs (s.levelDb) < 0.5 && std::abs (s.worstBandDb) < 0.5 && std::abs (s.worstThdGap) < 2.0;
        if (which == 0) check (met && ! mo.front.empty(), line); else std::printf ("  NOTE (hard clipping is more than seven powers can hold; the 0.5 dB / 2 %% target is %s) %s\n", met ? "met" : "NOT met", line);
    }
    std::printf ("== A soft curve and a filter after it; then the same with a filter in front as well ==\n");
    {
        const double level = 0.5; struct U { OnePole a, b; SqTube::Oversampler os; };
        auto make = [level] { auto u = std::make_shared<U>(); u->b.set (9000, false); u->os.prepare (8); return Unit ([u, level] (double v) { return u->b.run (level * u->os.process (v / level, [] (double x) { return std::tanh (1.2 * x) / 1.2; })); }); };
        const Model mo = capture (make, level, 7); const Score s = judge (make, mo, 0.9 * level);
        std::snprintf (line, sizeof (line), "a soft curve (3rd harmonic %.1f dB at the sweep's level) into a 9 kHz low-pass: level %.3f dB off, worst octave band %.3f dB off, left over %.1f dB under; distortion at most %.2f points apart (unit %.2f %%, model %.2f %%) - inside the 0.5 dB / 2 %% target", mo.harmonicDb[1], s.levelDb, s.worstBandDb, s.errorDb, s.worstThdGap, s.thdUnit, s.thdModel);
        check (std::abs (s.levelDb) < 0.5 && std::abs (s.worstBandDb) < 0.5 && std::abs (s.worstThdGap) < 2.0 && s.errorDb < -30.0, line);
        auto make2 = [level] { auto u = std::make_shared<U>(); u->a.set (80, true); u->b.set (9000, false); u->os.prepare (8); return Unit ([u, level] (double v) { return u->b.run (level * u->os.process (u->a.run (v / level), [] (double x) { return std::tanh (1.2 * x) / 1.2; })); }); };
        const Model m2 = capture (make2, level, 7); const Score s2 = judge (make2, m2, 0.9 * level);
        std::snprintf (line, sizeof (line), "with an 80 Hz high-pass BEFORE the curve (so bass drives it less): level %.2f dB off, worst octave band %.2f dB off, left over %.1f dB under; distortion at most %.2f points apart; front filter %s (the quiet pass predicted within %.2f dB with it, %.2f dB without)", s2.levelDb, s2.worstBandDb, s2.errorDb, s2.worstThdGap, m2.front.empty() ? "not used" : "used", m2.quietMissDb, m2.quietMissPlainDb);
        std::printf ("  NOTE (the front filter is found only roughly; the target is %s) %s\n", std::abs (s2.levelDb) < 0.5 && std::abs (s2.worstBandDb) < 0.5 && std::abs (s2.worstThdGap) < 2.0 ? "met" : "NOT met", line);
        Player p; p.prepare (mo); double out = 0; for (int i = 0; i < 20000; ++i) out = std::max (out, std::abs (p.process (4.0 * level * std::sin (2 * pi * 440.0 * i / fs))));
        std::snprintf (line, sizeof (line), "an input four times over the sweep's level is held at that level before the model (peak out %.3f): it never runs outside what was measured", out); check (out < 1.5 * level && out > 0.3 * level, line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
