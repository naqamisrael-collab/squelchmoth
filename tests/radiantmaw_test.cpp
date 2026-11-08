// Radiant and Maw measurements. g++ -O2 -std=c++17 -I../Source radiantmaw_test.cpp -o /tmp/rm && /tmp/rm
#include "SqRadiantMawDsp.h"
#include <chrono>
#include <cstdio>
#include <string>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const std::string& w) { (ok ? passes : fails)++; std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", w.c_str()); }
static std::string f2 (double v, int d = 2) { char b[64]; std::snprintf (b, sizeof b, "%.*f", d, v); return b; }
int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    const double sr = 48000;
    std::printf ("== Radiant ==\n");
    for (int a = 0; a < Radiant::numArch; ++a)
    {
        Radiant::Processor r; r.p.arch = a; r.p.singleStage = a == Radiant::preamp; r.p.inputDb = 0; r.prepare (sr, 256);
        const double t0 = r.thd(); Radiant::Processor h; h.p = r.p; h.p.inputDb = 18; h.prepare (sr, 256); const double t18 = h.thd();
        check (t0 > 0.05 && t18 > t0 * 2, std::string (Radiant::archName (a)) + ": THD " + f2 (t0, 2) + " % at Input 0, " + f2 (t18, 1) + " % at +18 (-18 dBFS)");
    }
    {   // Baxandall +12 dB bass at 50 Hz, unity at 1 kHz on a quiet signal (clean region)
        auto gainAt = [&] (double hz, float bass, float treb)
        {
            Radiant::Processor r; r.p.arch = Radiant::lineAmp; r.p.inputDb = -12; r.p.bassDb = bass; r.p.trebleDb = treb; r.prepare (sr, 256);
            std::vector<float> l (sr), rr; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.01f * (float) std::sin (2 * pi * hz * i / sr); rr = l; auto in = l;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, rr.data() + i }; r.process (ch, 2, 256); }
            double e = 0, ei = 0; for (size_t i = 24000; i < 47000; ++i) { e += l[i] * l[i]; ei += in[i] * in[i]; } return 10 * std::log10 (e / ei);
        };
        const double b = gainAt (40, 12, 0), m = gainAt (1000, 12, 12), t = gainAt (16000, 0, 12);
        check (b > 9 && std::abs (m) < 2.0 && t > 8, "Baxandall: bass +12 gives " + f2 (b, 1) + " dB at 40 Hz, treble +12 gives " + f2 (t, 1) + " dB at 16 kHz, 1 kHz " + f2 (m, 2) + " dB");
    }
    {   // Pre: bias makes choppy breakup (much more THD), noise floor about -86 dBFS
        Radiant::Processor a; a.p.arch = Radiant::preamp; a.p.singleStage = true; a.p.inputDb = 0; a.prepare (sr, 256);
        Radiant::Processor b; b.p = a.p; b.p.bias = true; b.prepare (sr, 256);
        const double ta = a.thd(), tb = b.thd();
        check (tb > ta * 1.5, "Radiant Pre: Bias raises THD from " + f2 (ta, 1) + " % to " + f2 (tb, 1) + " % (choppy breakup)");
        Radiant::Processor nz; nz.p.noisy = true; nz.prepare (sr, 256); std::vector<float> l (sr, 0.0f), r (sr, 0.0f);
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; nz.process (ch, 2, 256); }
        double e = 0; for (size_t i = 0; i < l.size(); ++i) e += l[i] * l[i];
        const double db = 10 * std::log10 (e / l.size()) + 4.77;  // uniform noise rms -> peak-ish reference
        check (db > -96 && db < -80, "noise floor switched on: " + f2 (db, 1) + " dBFS (about -68 dBu)");
    }
    std::printf ("== Maw ==\n");
    {
        // attack: step from silence to a loud tone; time for GR to reach 63 % of its final value
        Maw::Processor m; m.p.crush = 0.6f; m.p.crunch = 0; m.p.mix = 1; m.prepare (sr, 64);
        std::vector<float> l (sr * 4, 0.0f), r; for (size_t i = sr / 2; i < (size_t) sr * 2; ++i) l[i] = 0.5f * (float) std::sin (2 * pi * 200 * i / sr); r = l;
        std::vector<float> gr (l.size());
        for (size_t i = 0; i + 64 <= l.size(); i += 64) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, nullptr, 64); for (int k = 0; k < 64; ++k) gr[i + k] = m.gainReductionDb(); }
        const float fin = gr[sr * 2 - 100]; int t63 = -1; for (size_t i = sr / 2; i < l.size(); ++i) if (gr[i] >= 0.632f * fin) { t63 = (int) (i - sr / 2); break; }
        const double atkMs = t63 / sr * 1000.0;
        check (std::abs (atkMs - 1.3) < 0.35, "attack: GR reaches 63 % in " + f2 (atkMs, 2) + " ms (1.3 +- 0.2, block of 64 samples)");
        // release at modest GR: time constant ~1.7 s; at heavy GR it stretches
        auto relTau = [&] (float crush, float stretch)
        {
            Maw::Processor q; q.p.crush = crush; q.p.crunch = 0; q.p.stretch = stretch; q.prepare (sr, 64);
            std::vector<float> a (sr * 12, 0.0f), b; for (size_t i = 0; i < (size_t) sr; ++i) a[i] = 0.5f * (float) std::sin (2 * pi * 200 * i / sr); b = a;
            std::vector<float> g (a.size());
            for (size_t i = 0; i + 64 <= a.size(); i += 64) { float* ch[2] = { a.data() + i, b.data() + i }; q.process (ch, 2, nullptr, 64); for (int k = 0; k < 64; ++k) g[i + k] = q.gainReductionDb(); }
            const float g0 = g[sr - 1]; for (size_t i = sr; i < g.size(); ++i) if (g[i] <= g0 * 0.368f) return (i - sr) / sr;
            return 99.0;
        };
        const double tLight = relTau (0.25f, 0.0f), tHeavy = relTau (0.9f, 1.0f);
        check (std::abs (tLight - 1.7) < 0.25 && tHeavy > 4.0, "release: " + f2 (tLight, 2) + " s at light crush (1.7), " + f2 (tHeavy, 1) + " s when saturated (stretches toward 22 s; 99 = still above 37 % after 11 s)");
        // crush levels the input: a 20 dB jump in input becomes a small jump in output
        auto outDb = [&] (float inDb)
        {
            Maw::Processor q; q.p.crush = 0.7f; q.p.crunch = 0.2f; q.prepare (sr, 256);
            std::vector<float> a, b; TestSignals::pink (a, b, sr, 3.0, inDb);
            for (size_t i = 0; i + 256 <= a.size(); i += 256) { float* ch[2] = { a.data() + i, b.data() + i }; q.process (ch, 2, nullptr, 256); }
            double e = 0; for (size_t i = sr * 2; i < a.size(); ++i) e += a[i] * a[i]; return 10 * std::log10 (e / sr);
        };
        const double d = outDb (-10) - outDb (-30);
        check (d < 4.0, "crush: 20 dB more input comes out " + f2 (d, 1) + " dB louder");
        // CPU
        Maw::Processor q; q.prepare (sr, 128); std::vector<float> a, b; TestSignals::drums (a, b, sr, 10.0);
        auto t0 = std::chrono::steady_clock::now(); for (size_t i = 0; i + 128 <= a.size(); i += 128) { float* ch[2] = { a.data() + i, b.data() + i }; q.process (ch, 2, nullptr, 128); } auto t1 = std::chrono::steady_clock::now();
        Radiant::Processor rq; rq.prepare (sr, 128); TestSignals::drums (a, b, sr, 10.0);
        auto t2 = std::chrono::steady_clock::now(); for (size_t i = 0; i + 128 <= a.size(); i += 128) { float* ch[2] = { a.data() + i, b.data() + i }; rq.process (ch, 2, 128); } auto t3 = std::chrono::steady_clock::now();
        const double cm = std::chrono::duration<double> (t1 - t0).count() * 10, cr = std::chrono::duration<double> (t3 - t2).count() * 10;
        check (cm < 3 && cr < 3, "CPU at 4x: Maw " + f2 (cm, 2) + " %, Radiant " + f2 (cr, 2) + " % of one core");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passes, fails);
    return fails ? 1 : 0;
}
