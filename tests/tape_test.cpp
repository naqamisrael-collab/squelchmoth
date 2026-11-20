// Squelchmoth 1.40 - Voltage Tape: the tape machine's parts against their references.
//  - what the tape keeps: against the same record process run in a circuit simulator (tests/reference/tape/check_ngspice.py)
//  - the flux curve: against the standard's time constants; replay losses: against their formulas
//  - the whole channel: response, level, harmonics, aliasing, wow and flutter
#include "SqTape.h"
#include "SqTube.h"
#include <cstdio>
#include <chrono>
using namespace SqTape;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
struct Point { double hs, hb, kept; };
static const Point simulator[] = { { 0.2, 0.0, 0.027318 }, { 0.5, 0.0, 0.203710 }, { 0.1, 0.05, 0.067215 }, { 0.4, 0.2, 0.135047 }, { 0.8, 0.5, 0.156458 }, { 1.5, 1.0, 0.184264 }, { 2.0, 3.0, 0.144613 } };

struct Rig
{
    Channel ch; SqTube::Oversampler os; Transport tr; int factor = 4; double fs = 48000.0;
    void prepare (double rate, int f, const Settings& s, double wow = 0, double flutter = 0) { fs = rate; factor = f; ch.prepare (rate, f); ch.set (s); os.prepare (f); tr.prepare (rate * f, 7); tr.set (wow, flutter); }
    double process (double x) { return ch.processOuter (os.process (x, [this] (double v) { return ch.processInner (v, tr.next()); })); }
    int latency() const { return os.latency() + Channel::latency (fs); }
};
static std::vector<double> tone (Rig& r, double amp, double hz, int n, int settle)
{
    std::vector<double> y ((size_t) n);
    for (int i = 0; i < settle + n; ++i) { const double v = r.process (amp * std::sin (2.0 * pi * hz * i / r.fs)); if (i >= settle) y[(size_t) (i - settle)] = v; }
    return y;
}
static double bin (const std::vector<double>& y, double cyclesInWindow) { std::complex<double> a = 0; const int n = (int) y.size(); for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -2.0 * pi * cyclesInWindow * (double) i / n); return 20 * std::log10 (std::abs (a) / (n / 2.0) + 1e-300); }

int main()
{
    char line[600];
    std::printf ("== What the tape keeps, against the circuit simulator ==\n");
    {
        double worst = 0; for (const auto& p : simulator) { Curve c; c.setBias (p.hb); worst = std::max (worst, std::abs (c (p.hs) - p.kept)); }
        std::snprintf (line, sizeof (line), "seven signal / bias points from no bias to heavy overbias: within %.1e of the simulator (of a largest value of 0.32)", worst); check (worst < 3.0e-4, line);
        Curve none, low, good, over; none.setBias (0.0); low.setBias (0.05); good.setBias (0.2); over.setBias (1.0);
        std::snprintf (line, sizeof (line), "no bias: the tape keeps %.0f %% of what it keeps at the best bias for a quiet tone, and its 3rd harmonic is %.1f dB below the tone (a dead zone, as on a real unbiased tape)", 100.0 * none.sineGain (0.1) / low.sineGain (0.1), -none.harmonicDb (0.1, 3));
        check (none.sineGain (0.1) < 0.15 * low.sineGain (0.1) && none.harmonicDb (0.1, 3) > -20.0, line);
        std::snprintf (line, sizeof (line), "with bias it is linear for quiet tones (3rd harmonic %.1f dB down at the reference level) and bends as they get loud (%.1f dB at four times, %.1f dB at eight times)", -good.harmonicDb (0.1, 3), -good.harmonicDb (0.4, 3), -good.harmonicDb (0.8, 3));
        check (good.harmonicDb (0.1, 3) < -40.0 && good.harmonicDb (0.4, 3) > good.harmonicDb (0.1, 3) + 10.0 && good.harmonicDb (0.8, 3) > good.harmonicDb (0.4, 3), line);
        double bestBias = 0, best = 0; for (double b = 0.0; b <= 1.0; b += 0.005) { Curve c; c.setBias (b); if (c.sineGain (0.1) > best) { best = c.sineGain (0.1); bestBias = b; } }
        std::snprintf (line, sizeof (line), "sensitivity peaks at a bias of %.3f and falls with more: %.0f %% left at four times that, %.0f %% at twenty times (overbias)", bestBias, 100.0 * good.sineGain (0.1) / best, 100.0 * over.sineGain (0.1) / best);
        check (bestBias > 0.02 && bestBias < 0.1 && good.sineGain (0.1) < 0.8 * best && over.sineGain (0.1) < 0.4 * best, line);
        std::snprintf (line, sizeof (line), "more bias, less distortion at the same level: 3rd harmonic %.1f, %.1f, %.1f dB down at bias 0.05, 0.2, 1.0", -low.harmonicDb (0.4, 3), -good.harmonicDb (0.4, 3), -over.harmonicDb (0.4, 3));
        check (low.harmonicDb (0.4, 3) > good.harmonicDb (0.4, 3) && good.harmonicDb (0.4, 3) > over.harmonicDb (0.4, 3), line);
        bool odd = true, bounded = true, rising = true; double last = 0; for (double h = 0.0; h < 30.0; h += 0.01) { const double v = good (h); odd = odd && std::abs (good (-h) + v) < 1e-15; bounded = bounded && v < 0.4; rising = rising && v >= last - 1e-12; last = v; }
        check (odd && bounded && rising, "the curve is odd, never falls, and stays below saturation far past the simulated range");
        std::snprintf (line, sizeof (line), "only odd harmonics: the 2nd is %.0f dB down", -good.harmonicDb (0.4, 2)); check (good.harmonicDb (0.4, 2) < -200.0, line);
    }
    std::printf ("== The standards' flux curves ==\n");
    {
        double h, l; bool ok = true;
        timeConstants (Speed::ips7_5, Eq::nab, h, l); ok = ok && h == 50.0e-6 && l == 3180.0e-6; timeConstants (Speed::ips7_5, Eq::iec, h, l); ok = ok && h == 70.0e-6 && l == 0;
        timeConstants (Speed::ips15, Eq::nab, h, l); ok = ok && h == 50.0e-6 && l == 3180.0e-6; timeConstants (Speed::ips15, Eq::iec, h, l); ok = ok && h == 35.0e-6 && l == 0;
        timeConstants (Speed::ips30, Eq::nab, h, l); ok = ok && h == 17.5e-6 && l == 0;
        check (ok, "time constants: 7.5 in/s 3180 + 50 us or 70 us; 15 in/s 3180 + 50 us or 35 us; 30 in/s 17.5 us");
        double worst = 0, worstBass = 0;
        for (double tau : { 17.5e-6, 35.0e-6, 50.0e-6, 70.0e-6 }) for (double fs : { 176400.0, 192000.0, 384000.0 })
        {
            First f; f.lowpass (tau, fs); const double ref = f.level (400.0, fs) / fluxLevel (400.0, tau, 0);
            for (double hz : { 1000.0, 3150.0, 4500.0, 10000.0, 16000.0, 20000.0 }) worst = std::max (worst, std::abs (20 * std::log10 (f.level (hz, fs) / ref / fluxLevel (hz, tau, 0))));
            First b; b.bassRise (3180.0e-6, 10.0, fs); for (double hz : { 10.0, 20.0, 50.0, 100.0, 1000.0 }) worstBass = std::max (worstBass, std::abs (20 * std::log10 (b.level (hz, fs) * fluxLevel (hz, 1e-12, 0) / fluxLevel (hz, 1e-12, 3180.0e-6))));
        }
        std::snprintf (line, sizeof (line), "the treble corner follows the standard's curve within %.3f dB from 1 to 20 kHz at 4x of 44.1 and 48 kHz and above (gate 0.25); the bass rise within %.4f dB", worst, worstBass);
        check (worst <= 0.25 && worstBass <= 0.01, line);
        std::snprintf (line, sizeof (line), "at the corner the level is 3 dB down: %.2f dB at 3183 Hz for 50 us, %.2f dB at 4547 Hz for 35 us; 3180 us lifts 50 Hz by %.2f dB", 20 * std::log10 (fluxLevel (3183.1, 50e-6, 0)), 20 * std::log10 (fluxLevel (4547.3, 35e-6, 0)), 20 * std::log10 (fluxLevel (50.05, 1e-12, 3180e-6)));
        check (std::abs (20 * std::log10 (fluxLevel (3183.1, 50e-6, 0)) + 3.01) < 0.01 && std::abs (20 * std::log10 (fluxLevel (4547.3, 35e-6, 0)) + 3.01) < 0.01 && std::abs (20 * std::log10 (fluxLevel (50.05, 1e-12, 3180e-6)) - 2.97) < 0.05, line);
        First a, b; a.lowpass (35.0e-6, 192000.0); b.bassRise (3180.0e-6, 10.0, 192000.0); double err = 0; unsigned s = 1;
        for (int i = 0; i < 20000; ++i) { s = s * 1664525u + 1013904223u; const double x = (double) (s >> 8) / 8388608.0 - 1.0; err = std::max (err, std::abs (a.inverse (b.inverse (b.forward (a.forward (x)))) - x)); }
        std::snprintf (line, sizeof (line), "the curve and its undoing cancel exactly on noise (largest error %.1e)", err); check (err < 1.0e-9, line);
    }
    std::printf ("== Replay losses ==\n");
    {
        Head hd;
        std::snprintf (line, sizeof (line), "spacing: %.1f dB when the gap between head and tape is one wavelength, %.2f dB at a tenth; gap: nothing comes back when the wavelength equals the gap (%.1e), half-way there %.2f dB; thick coating at long wavelengths loses nothing (%.4f)",
                       20 * std::log10 (spacingLoss (1.0e-6, 1.0e-6)), 20 * std::log10 (spacingLoss (1.0e-5, 1.0e-6)), gapLoss (3.0e-6, 3.0e-6), 20 * std::log10 (gapLoss (6.0e-6, 3.0e-6)), thicknessLoss (1.0, 12.0e-6));
        check (std::abs (20 * std::log10 (spacingLoss (1.0e-6, 1.0e-6)) + 55.0) < 1e-9 && gapLoss (3.0e-6, 3.0e-6) < 1e-12 && std::abs (20 * std::log10 (gapLoss (6.0e-6, 3.0e-6)) + 3.92) < 0.01 && thicknessLoss (1.0, 12.0e-6) > 0.9999, line);
        double worst = 0, at10k = 0; char detail[300] = ""; size_t used = 0;
        for (Speed sp : { Speed::ips7_5, Speed::ips15, Speed::ips30 }) for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            const double v = metresPerSecond (sp); LossFir f; f.design (fs, [v, hd] (double hz) { return replayLevel (std::min (hz, 24000.0), v, hd); });
            for (double hz : { 20.0, 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 20000.0 }) worst = std::max (worst, std::abs (20 * std::log10 (f.level (hz, fs) / replayLevel (hz, v, hd))));
            at10k = std::max (at10k, std::abs (20 * std::log10 (f.level (10000.0, fs))));
            if (fs == 48000.0) used += (size_t) std::snprintf (detail + used, sizeof (detail) - used, " %s in/s: %+.2f dB at 5 kHz, %+.2f at 15 kHz, %+.2f at 20 kHz;", sp == Speed::ips7_5 ? "7.5" : sp == Speed::ips15 ? "15" : "30", 20 * std::log10 (replayLevel (5000.0, v, hd)), 20 * std::log10 (replayLevel (15000.0, v, hd)), 20 * std::log10 (replayLevel (20000.0, v, hd)));
        }
        std::snprintf (line, sizeof (line), "the replay filter follows the loss formulas within %.3f dB from 20 Hz to 20 kHz at three speeds and three rates (gate 0.25); 10 kHz is level within %.3f dB.%s", worst, at10k, detail);
        check (worst <= 0.25 && at10k <= 0.1, line);
        check (replayLevel (20000.0, metresPerSecond (Speed::ips7_5), hd) < replayLevel (20000.0, metresPerSecond (Speed::ips15), hd) && replayLevel (20000.0, metresPerSecond (Speed::ips15), hd) <= replayLevel (20000.0, metresPerSecond (Speed::ips30), hd) + 0.05, "slower tape has less top left at 20 kHz");
    }
    std::printf ("== The whole channel ==\n");
    const double fs = 48000.0;
    {
        Settings s; s.bump = 0.0; Rig r; r.prepare (fs, 4, s);
        int peakAt = -1; double peak = 0; for (int i = 0; i < 2000; ++i) { const double v = r.process (i == 0 ? 0.01 : 0.0); if (std::abs (v) > peak) { peak = std::abs (v); peakAt = i; } }
        std::snprintf (line, sizeof (line), "a click comes out %d samples later, as latency() says (%d: oversampler 96, replay filter 127, the tape path's resting delay %d)", peakAt, r.latency(), Channel::centreSamples (fs));
        check (peakAt == r.latency(), line);
        double worst = 0; for (double hz : { 100.0, 1000.0, 5000.0, 10000.0, 15000.0 }) { Rig q; q.prepare (fs, 4, s); const auto y = tone (q, 0.01, hz, 4800, 9600); worst = std::max (worst, std::abs (bin (y, hz / 10.0) - 20 * std::log10 (0.01) - 20 * std::log10 (q.ch.response (hz)))); }
        std::snprintf (line, sizeof (line), "quiet tones from 100 Hz to 15 kHz come back at the level the loss curve says, within %.3f dB (so the tape itself is level-matched)", worst); check (worst <= 0.25, line);
    }
    {
        // harmonics through everything, against what the tape's curve and the filters around it predict
        double worst = 0; int n = 0; char detail[200] = "";
        for (Eq eq : { Eq::nab, Eq::iec }) for (double amp : { 1.0, 2.0, 4.0 })
        {
            Settings s; s.bump = 0.0; s.eq = eq; s.fieldAtFullScale = 0.1 * amp; Rig r; r.prepare (fs, 4, s); const auto y = tone (r, 1.0, 1000.0, 4800, 9600);
            double high, low; timeConstants (s.speed, s.eq, high, low);
            First hi, lo; hi.lowpass (high, fs * 4); if (low > 0) lo.bassRise (low, 10.0, fs * 4); else lo.flat();
            auto pre = [&] (double hz) { return hi.level (hz, fs * 4) * lo.level (hz, fs * 4); };
            const double field = s.fieldAtFullScale * pre (1000.0);
            for (int k : { 3, 5 })
            {
                const double want = r.ch.tape().harmonicDb (field, k) + 20 * std::log10 (pre (1000.0) / pre (1000.0 * k)) + 20 * std::log10 (r.ch.response (1000.0 * k) / r.ch.response (1000.0));
                const double got = bin (y, 100.0 * k) - bin (y, 100.0); worst = std::max (worst, std::abs (got - want)); ++n;
                if (eq == Eq::nab && amp == 4.0 && k == 3) std::snprintf (detail, sizeof (detail), " (e.g. 12 dB hot: 3rd harmonic %.2f dB against %.2f predicted)", got, want);
            }
        }
        std::snprintf (line, sizeof (line), "3rd and 5th harmonics of 1 kHz at three levels and both equalisations: within %.3f dB of what the tape's curve and the filters predict, %d comparisons%s", worst, n, detail);
        check (worst <= 0.5, line);
    }
    {
        // NAB lifts the bass onto the tape, so bass saturates sooner than with IEC at the same level
        Settings a; a.bump = 0; a.eq = Eq::nab; a.fieldAtFullScale = 0.4; Settings b = a; b.eq = Eq::iec; Rig ra, rb; ra.prepare (fs, 4, a); rb.prepare (fs, 4, b);
        const auto ya = tone (ra, 1.0, 40.0, 12000, 24000), yb = tone (rb, 1.0, 40.0, 12000, 24000);
        std::snprintf (line, sizeof (line), "40 Hz, hot: 3rd harmonic %.1f dB down with the 3180 us curve, %.1f dB with the other (the bass is recorded hotter)", -(bin (ya, 30) - bin (ya, 10)), -(bin (yb, 30) - bin (yb, 10)));
        check (bin (ya, 30) - bin (ya, 10) > bin (yb, 30) - bin (yb, 10) + 3.0, line);
        Settings c; c.bump = 1.0; Rig rc; rc.prepare (fs, 4, c); Settings d; d.bump = 0.0; Rig rd; rd.prepare (fs, 4, d);
        std::snprintf (line, sizeof (line), "head bump at 15 in/s, full: %+.2f dB at 55 Hz, %+.2f dB at 110 Hz, %+.2f dB at 20 Hz, %+.3f dB at 1 kHz; off: flat (%+.4f dB at 55 Hz)", 20 * std::log10 (rc.ch.response (55.0) / rd.ch.response (55.0)), 20 * std::log10 (rc.ch.response (110.0) / rd.ch.response (110.0)), 20 * std::log10 (rc.ch.response (20.0) / rd.ch.response (20.0)), 20 * std::log10 (rc.ch.response (1000.0) / rd.ch.response (1000.0)), 20 * std::log10 (rd.ch.response (55.0) / rd.ch.response (400.0)));
        check (rc.ch.response (55.0) / rd.ch.response (55.0) > 1.1 && rc.ch.response (110.0) / rd.ch.response (110.0) < 1.02 && rc.ch.response (20.0) / rd.ch.response (20.0) < 0.95 && std::abs (20 * std::log10 (rc.ch.response (1000.0) / rd.ch.response (1000.0))) < 0.05, line);
    }
    std::printf ("== Aliasing (5 kHz sine; the largest part below 20 kHz that is not a harmonic, against the fundamental) ==\n");
    {
        double a[3][3]; const double hot[3] = { 1.0, 4.0, 10.0 };
        for (int oi = 0; oi < 3; ++oi) for (int ai = 0; ai < 3; ++ai)
        {
            Settings s; s.bump = 0; s.fieldAtFullScale = 0.1 * hot[ai]; Rig r; r.prepare (fs, oi == 0 ? 4 : oi == 1 ? 8 : 16, s); const auto y = tone (r, 1.0, 5000.0, 48000, 48000);
            double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k)); a[oi][ai] = bin (y, 5000) - worst;
        }
        for (int oi = 0; oi < 3; ++oi) std::printf ("  NOTE %2dx: %.1f dB down at the reference level, %.1f dB 12 dB hot, %.1f dB 20 dB hot\n", oi == 0 ? 4 : oi == 1 ? 8 : 16, a[oi][0], a[oi][1], a[oi][2]);
        std::snprintf (line, sizeof (line), "4x: %.1f dB down at the reference level and %.1f dB 12 dB hot (gate 100)", a[0][0], a[0][1]); check (a[0][0] >= 100.0 && a[0][1] >= 100.0, line);
        Settings s; s.bump = 0; s.bias = 0.0; s.fieldAtFullScale = 0.4; Rig r; r.prepare (fs, 4, s); const auto y = tone (r, 1.0, 5000.0, 48000, 48000);
        double worst = -400; for (int k = 100; k <= 20000; k += 100) if (k % 5000 != 0) worst = std::max (worst, bin (y, k));
        std::printf ("  NOTE no bias at all, 12 dB hot, 4x: %.1f dB down (the dead zone makes strong high harmonics)\n", bin (y, 5000) - worst);
    }
    std::printf ("== Wow and flutter ==\n");
    {
        Transport t; t.prepare (192000.0, 3); t.set (0.004, 0.0); double sp = 0, peak = 0, late = 0; for (int i = 0; i < 192000 * 20; ++i) { late = std::max (late, std::abs (t.next (&sp))); peak = std::max (peak, std::abs (sp)); }
        std::snprintf (line, sizeof (line), "wow set to 0.40 %%: the speed error peaks at %.3f %% over 20 s; the sound moves at most %.2f ms (the path allows 6)", peak * 100.0, late * 1000.0);
        check (peak > 0.0032 && peak <= 0.004001 && late < 0.003 && t.reach() < 0.006, line);
        Transport worstCase; worstCase.prepare (192000.0, 3); worstCase.set (0.01, 0.005);
        std::snprintf (line, sizeof (line), "at the most the controls allow (wow 1 %%, flutter 0.5 %%) the sound moves at most %.2f ms: inside the path's 6 ms", worstCase.reach() * 1000.0); check (worstCase.reach() < 0.0055, line);
        // heard: a 3 kHz tone through the machine; its frequency read back from its phase
        for (int which = 0; which < 2; ++which)
        {
            const double wow = which == 0 ? 0.003 : 0.0, fl = which == 0 ? 0.0 : 0.002; Settings s; s.bump = 0; Rig r; r.prepare (fs, 4, s, wow, fl);
            const int n = (int) fs * 12; const double hz = 3000.0; std::vector<std::complex<double>> z ((size_t) n);
            for (int i = 0; i < n; ++i) z[(size_t) i] = r.process (0.05 * std::sin (2.0 * pi * hz * i / fs)) * std::polar (1.0, -2.0 * pi * hz * i / fs);
            const int win = 96; std::vector<double> phase; std::complex<double> acc = 0; for (int i = 0; i < n; ++i) { acc += z[(size_t) i]; if (i >= win) acc -= z[(size_t) (i - win)]; if (i >= 4800 && i % 48 == 0) phase.push_back (std::arg (acc)); }
            double peak = 0; for (size_t i = 1; i < phase.size(); ++i) { double d = phase[i] - phase[i - 1]; while (d > pi) d -= 2 * pi; while (d < -pi) d += 2 * pi; peak = std::max (peak, std::abs (d / (2.0 * pi) * (fs / 48.0) / hz)); }
            Transport same; same.prepare (fs * 4, 7); same.set (wow, fl); double want = 0, sp2 = 0; for (long long i = 0; i < (long long) n * 4; ++i) { same.next (&sp2); want = std::max (want, std::abs (sp2)); }
            std::snprintf (line, sizeof (line), "%s: a 3 kHz tone comes back wandering by up to %.3f %% in pitch; the transport's speed error peaks at %.3f %%", which == 0 ? "wow 0.3 %" : "flutter 0.2 %", peak * 100.0, want * 100.0);
            check (std::abs (peak - want) < 0.15 * want, line);
        }
        Settings s; s.bump = 0; Rig still, moving; still.prepare (fs, 4, s); moving.prepare (fs, 4, s, 0.01, 0.005);
        const auto a = tone (still, 0.05, 1000.0, 48000, 9600), b = tone (moving, 0.05, 1000.0, 48000, 9600); double pa = 0, pb = 0; for (double v : a) pa += v * v; for (double v : b) pb += v * v;
        std::snprintf (line, sizeof (line), "the wandering changes pitch, not loudness: level differs by %.3f dB with both at their most", 10 * std::log10 (pb / pa)); check (std::abs (10 * std::log10 (pb / pa)) < 0.05, line);
    }
    std::printf ("== Behaviour ==\n");
    {
        Settings s; Rig r; r.prepare (fs, 4, s, 0.003, 0.002); bool finite = true; double big = 0;
        for (int i = 0; i < 48000; ++i) { const double v = r.process (50.0 * std::sin (i * 0.37)); finite = finite && std::isfinite (v); big = std::max (big, std::abs (v)); }
        std::snprintf (line, sizeof (line), "a signal 34 dB over full scale stays finite (largest %.1f) - the tape saturates, it does not fold", big); check (finite && big < 60.0, line);
        double tail = 0; for (int i = 0; i < 96000; ++i) { const double v = r.process (0.0); if (i > 90000) tail = std::max (tail, std::abs (v)); }
        std::snprintf (line, sizeof (line), "and it is silent again afterwards (%.1e)", tail); check (tail < 1.0e-6, line);
        Rig q; q.prepare (fs, 4, s); for (int i = 0; i < 4800; ++i) q.process (0.3 * std::sin (i * 0.05));
        Settings s2 = s; s2.speed = Speed::ips7_5; s2.eq = Eq::iec; s2.bias = 0.6; q.ch.set (s2); finite = true; for (int i = 0; i < 48000; ++i) finite = finite && std::isfinite (q.process (0.3 * std::sin (i * 0.05)));
        check (finite, "changing speed, equalisation and bias while running stays finite");
        Rig t; t.prepare (fs, 4, s, 0.002, 0.001); const int n = 48000 * 4; const auto t0 = std::chrono::steady_clock::now(); double acc = 0; for (int i = 0; i < n; ++i) acc += t.process (0.5 * std::sin (i * 0.13));
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  NOTE speed here: one channel at 4x runs %.1f times faster than real time (%.1f %% of one core); acc %.2f\n", 4.0 / secs, secs / 4.0 * 100.0, acc);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
