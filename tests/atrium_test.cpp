// Atrium Verb DSP core: measured. Build: scripts/build_atrium_test.sh
#include "SqAtriumDsp.h"
#include <cstdio>
#include <chrono>
#include <functional>

using namespace SqAtrium;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }

static int spaceIndex (const char* name) { for (int i = 0; i < (int) spaces().size(); ++i) if (juce::String (spaces()[(size_t) i].name) == name) return i; return 0; }

/** Runs a stereo signal through a fresh engine. Returns L and R. */
static void run (Params p, double fs, const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>& outL, std::vector<float>& outR,
                 std::function<void (Engine&, int)> perBlock = {})
{
    Engine e; e.p = p; e.prepare (fs, 512);
    outL = inL; outR = inR;
    for (int s = 0; s < (int) outL.size(); s += 512)
    {
        const int n = std::min (512, (int) outL.size() - s);
        if (perBlock) perBlock (e, s);
        float* ch[2] = { outL.data() + s, outR.data() + s };
        e.process (ch, 2, n);
    }
}

static Params lateOnly (int space, double decay)
{
    Params p; p.space = space; p.decaySec = decay; p.mix = 1.0; p.early = 0; p.late = 1; p.predelayMs = 0; p.lowCutHz = 10; p.highCutHz = 20000;
    p.lowMult = 1; p.highMult = 1; p.character = 0.3; p.width = 1; return p;
}

/** Band RT60 by Schroeder backward integration (T20 on -5..-25 dB, x3) of the band-passed impulse response. */
static double rt60 (const std::vector<float>& ir, double fs, double fc)
{
    Biquad a, b; a.bandpass (fs, fc, 1.41); b.bandpass (fs, fc, 1.41);
    std::vector<double> e (ir.size());
    for (size_t i = 0; i < ir.size(); ++i) { const double y = b.process (a.process (ir[i])); e[i] = y * y; }
    for (int i = (int) e.size() - 2; i >= 0; --i) e[(size_t) i] += e[(size_t) i + 1];
    const double e0 = e[0]; double t5 = -1, t25 = -1;
    for (size_t i = 0; i < e.size(); ++i)
    {
        const double d = 10 * std::log10 (std::max (1e-300, e[i] / e0));
        if (t5 < 0 && d <= -5) t5 = i / fs;
        if (t25 < 0 && d <= -25) { t25 = i / fs; break; }
    }
    return t25 > 0 && t5 >= 0 ? 3.0 * (t25 - t5) : -1;
}

static std::vector<float> impulse (Params p, double fs, double seconds, std::vector<float>* right = nullptr)
{
    std::vector<float> l ((size_t) (fs * seconds), 0.0f), r = l; l[0] = 1; r[0] = 1;
    std::vector<float> ol, orr; run (p, fs, l, r, ol, orr);
    if (right) *right = orr;
    return ol;
}

static void noise (std::vector<float>& l, std::vector<float>& r, double fs, double seconds, double gain, int seed = 3)
{
    juce::Random rnd (seed); const int n = (int) (fs * seconds); l.resize ((size_t) n); r.resize ((size_t) n);
    for (int i = 0; i < n; ++i) { l[(size_t) i] = (float) ((rnd.nextDouble() * 2 - 1) * gain); r[(size_t) i] = (float) ((rnd.nextDouble() * 2 - 1) * gain); }
}

static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }

int main()
{
    juce::ScopedNoDenormals nd;
    const double fs = 48000;
    std::printf ("Atrium Verb DSP\n");

    // 1. Mid-band decay time = the Decay control, across spaces and decays
    {
        double worst = 0; juce::String worstAt;
        for (auto name : { "Concert Hall", "Plate", "Chamber", "Cathedral", "Studio Room" })
            for (double T : { 0.6, 2.0, 6.0 })
            {
                const auto ir = impulse (lateOnly (spaceIndex (name), T), fs, T * 1.6 + 0.6);
                const double m = rt60 (ir, fs, 1000);
                const double err = std::abs (m / T - 1);
                std::printf ("    %-13s T=%.1f s -> RT60(1k) %.3f s\n", name, T, m);
                if (err > worst) { worst = err; worstAt = juce::String (name) + " " + juce::String (T, 1) + " s"; }
            }
        check (worst < 0.10, "RT60 at 1 kHz within 10 % of Decay in every space (worst " + juce::String (worst * 100, 1) + " % at " + worstAt + ")");
    }
    // 2. Decay per band: low and high multipliers
    {
        auto p = lateOnly (spaceIndex ("Concert Hall"), 2.0); p.lowMult = 2.0; p.highMult = 0.5; p.lowXHz = 300; p.highXHz = 3000;
        const auto ir = impulse (p, fs, 6.0);
        const double tl = rt60 (ir, fs, 100), tm = rt60 (ir, fs, 1000), th = rt60 (ir, fs, 10000);
        std::printf ("    low x2 / high x0.5: 100 Hz %.2f s, 1 kHz %.2f s, 10 kHz %.2f s\n", tl, tm, th);
        check (tl > 3.4 && tl < 4.4, "Low decay x2 -> 100 Hz decays " + juce::String (tl, 2) + " s (target 4.0)");
        check (th > 0.8 && th < 1.25, "High decay x0.5 -> 10 kHz decays " + juce::String (th, 2) + " s (target 1.0)");
        Engine e; e.p = p; e.prepare (fs, 512);
        const double a100 = e.decayAtHz (100), a1k = e.decayAtHz (1000), a10k = e.decayAtHz (10000);
        check (std::abs (a100 / tl - 1) < 0.12 && std::abs (a1k / tm - 1) < 0.10 && std::abs (a10k / th - 1) < 0.15,
               "The face's decay curve matches the measurement (" + juce::String (a100, 2) + " / " + juce::String (a1k, 2) + " / " + juce::String (a10k, 2) + " s)");
    }
    // 3. Echo density: a smooth tail, not discrete echoes (normalised echo density over 20 ms windows)
    {
        auto p = lateOnly (spaceIndex ("Concert Hall"), 2.5); p.diffusion = 1.0;
        const auto ir = impulse (p, fs, 1.0);
        double ned = 0; int k = 0;
        for (int start = (int) (0.10 * fs); start + 960 < (int) (0.40 * fs); start += 480, ++k)
        {
            double s2 = 0; for (int i = start; i < start + 960; ++i) s2 += (double) ir[(size_t) i] * ir[(size_t) i];
            const double sd = std::sqrt (s2 / 960); int over = 0;
            for (int i = start; i < start + 960; ++i) if (std::abs (ir[(size_t) i]) > sd) ++over;
            ned += (over / 960.0) / 0.3173;
        }
        ned /= k;
        check (ned > 0.85, "Echo density of the tail 100-400 ms: " + juce::String (ned, 2) + " (1.0 = Gaussian noise)");
        auto pp = lateOnly (spaceIndex ("Plate"), 2.0); pp.diffusion = 1.0;
        const auto ip = impulse (pp, fs, 0.2);
        int firstDense = -1;
        for (int start = 0; start + 480 < (int) ip.size(); start += 120)
        {
            double s2 = 0; for (int i = start; i < start + 480; ++i) s2 += (double) ip[(size_t) i] * ip[(size_t) i];
            const double sd = std::sqrt (s2 / 480); int over = 0; for (int i = start; i < start + 480; ++i) if (std::abs (ip[(size_t) i]) > sd) ++over;
            if ((over / 480.0) / 0.3173 > 0.7) { firstDense = start; break; }
        }
        check (firstDense >= 0 && firstDense < (int) (0.05 * fs), "Plate is dense within " + juce::String (firstDense / fs * 1000, 1) + " ms");
    }
    // 4. Stereo: decorrelated tail at width 100 %, mono at 0 %
    {
        std::vector<float> R;
        auto p = lateOnly (spaceIndex ("Concert Hall"), 2.0);
        const auto L = impulse (p, fs, 1.5, &R);
        auto corr = [&] (const std::vector<float>& a, const std::vector<float>& b)
        { double ab = 0, aa = 0, bb = 0; for (int i = (int) (0.05 * fs); i < (int) (1.2 * fs); ++i) { ab += (double) a[(size_t) i] * b[(size_t) i]; aa += (double) a[(size_t) i] * a[(size_t) i]; bb += (double) b[(size_t) i] * b[(size_t) i]; } return ab / std::sqrt (aa * bb + 1e-30); };
        const double c1 = corr (L, R);
        p.width = 0; std::vector<float> R0; const auto L0 = impulse (p, fs, 1.5, &R0); const double c0 = corr (L0, R0);
        check (std::abs (c1) < 0.2, "Width 100 %: tail correlation L/R " + juce::String (c1, 3));
        check (c0 > 0.999, "Width 0 %: mono (correlation " + juce::String (c0, 4) + ")");
    }
    // 5. Level stays musical across decay times (wet RMS of noise)
    {
        double lo = 1e9, hi = -1e9;
        for (double T : { 0.4, 1.0, 2.5, 6.0, 15.0 })
        {
            std::vector<float> l, r, ol, orr; noise (l, r, fs, 4.0, 0.25);
            auto p = lateOnly (spaceIndex ("Concert Hall"), T); p.early = 1;
            run (p, fs, l, r, ol, orr);
            const double lv = db (rms (ol, (int) (3.0 * fs), (int) (4.0 * fs)) / rms (l, (int) (3.0 * fs), (int) (4.0 * fs)));
            std::printf ("    T=%4.1f s: wet %.1f dB re dry\n", T, lv);
            lo = std::min (lo, lv); hi = std::max (hi, lv);
        }
        check (hi - lo < 10 && hi < 6 && lo > -12, "Wet level for steady noise spans " + juce::String (lo, 1) + " .. " + juce::String (hi, 1) + " dB re dry over 0.4-15 s");
    }
    // 6. Freeze holds the tail
    {
        std::vector<float> l, r, ol, orr; noise (l, r, fs, 9.0, 0.25);
        for (int i = (int) (1.0 * fs); i < (int) l.size(); ++i) { l[(size_t) i] = 0; r[(size_t) i] = 0; }
        auto p = lateOnly (spaceIndex ("Concert Hall"), 1.0);
        run (p, fs, l, r, ol, orr, [&] (Engine& e, int s) { e.p.freeze = s >= (int) (0.95 * fs); });
        const double a = rms (ol, (int) (1.2 * fs), (int) (1.7 * fs)), b = rms (ol, (int) (8.3 * fs), (int) (8.8 * fs));
        check (b > 0 && std::abs (db (b / a)) < 3.0, "Freeze: level after 7 more seconds " + juce::String (db (b / a), 2) + " dB (Decay 1 s would be -420 dB)");
    }
    // 7. Stability at every extreme, every space
    {
        bool ok = true; double worstPk = 0;
        for (int si = 0; si < (int) spaces().size(); ++si)
        {
            std::vector<float> l, r, ol, orr; noise (l, r, fs, 20.0, 0.5, si + 5);
            for (int i = (int) (2.0 * fs); i < (int) l.size(); ++i) { l[(size_t) i] = 0; r[(size_t) i] = 0; }
            Params p; p.space = si; p.decaySec = 30; p.size = 1; p.character = 1; p.lowMult = 4; p.highMult = 2; p.lowXHz = 1000; p.highXHz = 1000;
            p.diffusion = 1; p.mix = 1; p.early = 2; p.late = 2; p.width = 2;
            run (p, fs, l, r, ol, orr, [&] (Engine& e, int s) { e.p.size = (s / 24000) % 2 ? 1.0 : 0.0; e.p.predelayMs = (s / 36000) % 2 ? 500 : 0; });
            double pk = 0; bool fin = true;
            for (size_t i = 0; i < ol.size(); ++i) { if (! std::isfinite (ol[i]) || ! std::isfinite (orr[i])) fin = false; pk = std::max (pk, (double) std::abs (ol[i])); }
            double e1 = 0, e2 = 0; for (int sec = 3; sec < 6; ++sec) e1 = std::max (e1, rms (ol, (int) (sec * fs), (int) ((sec + 1) * fs)) + rms (orr, (int) (sec * fs), (int) ((sec + 1) * fs)));
            for (int sec = 6; sec < 20; ++sec) e2 = std::max (e2, rms (ol, (int) (sec * fs), (int) ((sec + 1) * fs)) + rms (orr, (int) (sec * fs), (int) ((sec + 1) * fs)));
            if (! fin || pk > 20 || e2 > e1) { ok = false; std::printf ("    unstable: %s pk %.2f e1 %.4f e2 %.4f\n", spaces()[(size_t) si].name, pk, e1, e2); }
            worstPk = std::max (worstPk, pk);
        }
        check (ok, "All 12 spaces stable at the extremes for 18 s of tail (30 s, Size and Predelay flipping, character 100, low x4, high x2, width 200 %): peak " + juce::String (worstPk, 2));
    }
    // 8. Mix 0 is the dry signal, bit for bit
    {
        std::vector<float> l, r, ol, orr; noise (l, r, fs, 1.0, 0.5);
        Params p; p.mix = 0; run (p, fs, l, r, ol, orr);
        bool same = true; for (size_t i = 0; i < l.size(); ++i) if (ol[i] != l[i] || orr[i] != r[i]) same = false;
        check (same, "Mix 0 % = dry, bit-exact");
    }
    // 9. Predelay: the first wet energy arrives at the predelay
    {
        bool ok = true; juce::String msg;
        for (double pd : { 0.0, 40.0, 250.0 })
        {
            auto p = lateOnly (spaceIndex ("Plate"), 1.0); p.predelayMs = pd; p.diffusion = 0;
            const auto ir = impulse (p, fs, 0.6);
            int first = -1; for (int i = 0; i < (int) ir.size(); ++i) if (std::abs (ir[(size_t) i]) > 1e-4) { first = i; break; }
            const double ms = first / fs * 1000;
            if (ms < pd || ms > pd + 5.0) ok = false;   // the onset tap: the first two diffusers, 3.8 ms
            msg << juce::String (pd, 0) << " ms -> " << juce::String (ms, 1) << " ms; ";
        }
        check (ok, "Predelay: the Plate starts within 5 ms after the predelay (" + msg.trimEnd() + ")");
    }
    // 10. Ducking: the wet drops under the dry signal and comes back
    {
        std::vector<float> l, r, ol0, orr0, ol1, orr1; noise (l, r, fs, 4.0, 0.5);
        for (int i = (int) (2.0 * fs); i < (int) l.size(); ++i) { l[(size_t) i] = 0; r[(size_t) i] = 0; }
        auto p = lateOnly (spaceIndex ("Concert Hall"), 3.0);
        run (p, fs, l, r, ol0, orr0);
        p.duckDb = 12; p.duckThreshDb = -30; p.duckReleaseMs = 200; run (p, fs, l, r, ol1, orr1);
        const double during = db (rms (ol1, (int) (1.5 * fs), (int) (2.0 * fs)) / rms (ol0, (int) (1.5 * fs), (int) (2.0 * fs)));
        const double after = db (rms (ol1, (int) (3.0 * fs), (int) (3.5 * fs)) / rms (ol0, (int) (3.0 * fs), (int) (3.5 * fs)));
        check (during < -10 && during > -13, "Ducking 12 dB: wet " + juce::String (during, 1) + " dB while the dry plays");
        check (after > -1.0, "...and back to " + juce::String (after, 2) + " dB a second after it stops");
    }
    // 11. Gate space: a dense burst that stops
    {
        std::vector<float> l, r, ol, orr; noise (l, r, fs, 2.0, 0.5);
        for (int i = (int) (0.3 * fs); i < (int) l.size(); ++i) { l[(size_t) i] = 0; r[(size_t) i] = 0; }
        auto p = lateOnly (spaceIndex ("Gate"), 2.5); p.early = 1;
        run (p, fs, l, r, ol, orr);
        const double body = rms (ol, (int) (0.2 * fs), (int) (0.3 * fs)), after = rms (ol, (int) ((0.3 + 0.28 + 0.08) * fs), (int) ((0.3 + 0.28 + 0.18) * fs));
        check (db (after / body) < -40, "Gate: tail " + juce::String (db (after / body), 1) + " dB 80 ms after the hold");
    }
    // 12. Post EQ
    {
        std::vector<float> l, r, o0, o0r, o1, o1r; noise (l, r, fs, 3.0, 0.25);
        auto p = lateOnly (spaceIndex ("Concert Hall"), 1.5); run (p, fs, l, r, o0, o0r);
        p.lowCutHz = 300; p.highCutHz = 3000; run (p, fs, l, r, o1, o1r);
        auto band = [&] (const std::vector<float>& v, double f) { Biquad a, b; a.bandpass (fs, f, 4); b.bandpass (fs, f, 4); double s = 0; for (int i = (int) fs; i < (int) v.size(); ++i) { const double y = b.process (a.process (v[(size_t) i])); s += y * y; } return std::sqrt (s); };
        const double d60 = db (band (o1, 60) / band (o0, 60)), d1k = db (band (o1, 1000) / band (o0, 1000)), d12k = db (band (o1, 12000) / band (o0, 12000));
        check (d60 < -40 && std::abs (d1k) < 1.0 && d12k < -40, "Low cut 300 / high cut 3k: 60 Hz " + juce::String (d60, 1) + " dB, 1 kHz " + juce::String (d1k, 2) + " dB, 12 kHz " + juce::String (d12k, 1) + " dB");
    }
    // 13. Sample-rate independence
    {
        const auto a = impulse (lateOnly (spaceIndex ("Concert Hall"), 2.0), 44100, 4.0), b = impulse (lateOnly (spaceIndex ("Concert Hall"), 2.0), 96000, 4.0);
        const double ta = rt60 (a, 44100, 1000), tb = rt60 (b, 96000, 1000);
        check (std::abs (ta / tb - 1) < 0.06, "RT60 at 44.1 kHz " + juce::String (ta, 3) + " s, at 96 kHz " + juce::String (tb, 3) + " s");
    }
    // 14. Bloom swells: the tail's energy peaks later than the plain hall's
    {
        auto peakAt = [&] (int si)
        {
            auto p = lateOnly (si, 3.0); const auto ir = impulse (p, fs, 1.0);
            double best = 0; int at = 0; for (int s = 0; s + 2400 < (int) ir.size(); s += 1200) { const double v = rms (ir, s, s + 2400); if (v > best) { best = v; at = s; } }
            return at / fs * 1000;
        };
        const double hall = peakAt (spaceIndex ("Concert Hall")), bloom = peakAt (spaceIndex ("Bloom"));
        check (bloom > hall + 60, "Bloom: the tail peaks at " + juce::String (bloom, 0) + " ms (Concert Hall " + juce::String (hall, 0) + " ms)");
    }
    // 15. CPU
    {
        std::vector<float> l, r, ol, orr; noise (l, r, fs, 20.0, 0.25);
        Params p; p.space = spaceIndex ("Concert Hall"); p.mix = 0.3; p.character = 0.6; p.lowCutHz = 80; p.highCutHz = 12000; p.duckDb = 6;
        const auto t0 = std::chrono::steady_clock::now(); run (p, fs, l, r, ol, orr);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (sec / 20.0 < 0.03, "CPU: " + juce::String (sec / 20.0 * 100, 2) + " % of one core at 48 kHz stereo");
    }

    std::printf ("== %d passed, %d failed ==\n", passed, failed);
    return failed ? 1 : 0;
}
