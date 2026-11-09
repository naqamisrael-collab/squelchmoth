// Measures Shard and Murmuration (Source/SqColor111cDsp.h).
// g++ -O2 -std=c++17 -ISource tests/color111c_test.cpp -o /tmp/c111c && /tmp/c111c
#include "SqColor111cDsp.h"
#include <cstdio>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const char* what, double v = 0) { std::printf ("%s  %-80s %10.3f\n", ok ? "PASS" : "FAIL", what, v); ok ? ++passes : ++fails; }
static const double SR = 48000;
static double rms (const std::vector<float>& x, size_t a, size_t b) { double e = 0; b = std::min (b, x.size()); for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i]; return std::sqrt (e / (double) std::max<size_t> (1, b - a)); }
static double db (double g) { return 20 * std::log10 (std::max (1e-12, g)); }
static std::vector<float> sine (double hz, double secs, float amp = 0.25f) { std::vector<float> v ((size_t) (SR * secs)); for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2 * pi * hz * i / SR); return v; }
static double freqOf (const std::vector<float>& x, size_t a, size_t b)
{
    // dominant frequency by a Goertzel sweep (robust to splice seams)
    double best = 0, bf = 0;
    for (double f = 40; f < 4000; f *= 1.003)
    {
        const double w = 2 * pi * f / SR; double s1 = 0, s2 = 0, c = 2 * std::cos (w);
        for (size_t i = a; i < b; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
        const double pw = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (pw > best) { best = pw; bf = f; }
    }
    return bf;
}
template <typename P> static void runShard (P& s, std::vector<float>& l, std::vector<float>& r, const std::vector<float>* key = nullptr)
{
    for (size_t i = 0; i < l.size(); i += 256)
    {
        const int m = (int) std::min<size_t> (256, l.size() - i);
        float* ch[2] = { l.data() + i, r.data() + i }; const float* k[2] = { key ? key->data() + i : nullptr, key ? key->data() + i : nullptr };
        s.process (ch, 2, key ? k : nullptr, m);
    }
}
static void runMurm (Murm::Processor& m, std::vector<float>& l, std::vector<float>& r)
{
    for (size_t i = 0; i < l.size(); i += 256) { const int n = (int) std::min<size_t> (256, l.size() - i); float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, n); }
}

int main()
{
    //======================================================================== Shard
    {
        auto pitchTest = [] (int engine, float st, double inHz)
        {
            Shard::Processor s; s.prepare (SR, 256); s.p.engine = engine; s.p.pitchSt = st; s.p.reverseProb = 0; s.p.recycle = 0; s.p.mix = 1; s.p.spliceMs = 500; s.p.delayMs = 0;
            auto l = sine (inHz, 2.0), r = l; runShard (s, l, r); return freqOf (l, 48000, 96000);
        };
        const double a = pitchTest (Shard::vintage, 12, 440), b = pitchTest (Shard::clean, 7, 440), c = pitchTest (Shard::vintage, -24, 440), d = pitchTest (Shard::clean, -12, 440);
        check (std::abs (a / 880 - 1) < 0.01, "Shard: Vintage +12 st turns 440 Hz into 880 Hz", a);
        check (std::abs (b / 659.26 - 1) < 0.012, "Shard: Clean +7 st turns 440 Hz into 659 Hz", b);
        check (std::abs (c / 110 - 1) < 0.01, "Shard: Vintage -24 st turns 440 Hz into 110 Hz", c);
        check (std::abs (d / 220 - 1) < 0.01, "Shard: Clean -12 st turns 440 Hz into 220 Hz", d);
    }
    {
        // Reverse: a rising ramp comes back falling
        auto ramp = [] (bool reverse)
        {
            Shard::Processor s; s.prepare (SR, 256); s.p.pitchSt = 0; s.p.reverseProb = reverse ? 1.0f : 0.0f; s.p.recycle = 0; s.p.mix = 1; s.p.spliceMs = 200; s.p.smoothing = 0.05f; s.p.lowCutHz = 1;
            std::vector<float> l ((size_t) SR * 2); for (size_t i = 0; i < l.size(); ++i) l[i] = 0.5f * (float) ((i % 2400) / 2400.0) - 0.25f; auto r = l;
            runShard (s, l, r); int up = 0, dn = 0; for (size_t i = 48001; i < 96000; ++i) { const float dd = l[i] - l[i - 1]; if (dd > 1e-6f) ++up; else if (dd < -1e-6f) ++dn; }
            return (double) dn / std::max (1, up + dn);
        };
        const double fw = ramp (false), rv = ramp (true);
        check (fw < 0.2 && rv > 0.8, "Shard: Reverse plays splices backwards (share of falling samples)", rv);
    }
    {
        Shard::Processor s; s.prepare (SR, 256); s.p.pitchSt = 0; s.p.reverseProb = 0; s.p.recycle = 0; s.p.mix = 1; s.p.spliceMs = 100; s.p.delayMs = 500;
        std::vector<float> l ((size_t) SR * 2, 0.0f); for (size_t i = 4800; i < 7200; ++i) l[i] = 0.5f * (float) std::sin (i * 0.05); auto r = l;
        runShard (s, l, r);
        size_t first = 0; for (size_t i = 7300; i < l.size(); ++i) if (std::abs (l[i]) > 0.01f) { first = i; break; }
        const double ms = (first - 4800) / SR * 1000.0;
        check (ms > 480 && ms < 720, "Shard: Delay 500 ms (+ splice) before the first splice sounds (ms)", ms);
    }
    {
        Shard::Processor s; s.prepare (SR, 256); s.p.pitchSt = 48; s.p.spliceMs = 8000; s.p.delayMs = 8000; s.p.recycle = 0.9f; s.p.mix = 1; s.p.density = 4; s.p.spray = 1; s.p.sizeJitter = 1;
        ModShapes::Rand rn; std::vector<float> l ((size_t) SR * 4), r; for (auto& x : l) x = 0.3f * rn.next(); r = l; runShard (s, l, r);
        bool fin = true; float pk = 0; for (auto x : l) { fin = fin && std::isfinite (x); pk = std::max (pk, std::abs (x)); }
        check (fin && pk < 4, "Shard: 8 s splice, 8 s delay, +48 st, 90 % recycle, spray stays finite and bounded (peak)", pk);
        Shard::Processor q; q.prepare (SR, 256); q.p.pitchSt = -48; q.p.engine = Shard::clean; q.p.mix = 1; auto l2 = sine (300, 1.5), r2 = l2; runShard (q, l2, r2);
        bool fin2 = true; for (auto x : l2) fin2 = fin2 && std::isfinite (x); check (fin2, "Shard: Clean -48 st finite", 1);
    }
    {
        const float q = Shard::quantisedPitch (6.0f, true, 0, 0xAB5), q2 = Shard::quantisedPitch (3.2f, true, 9, 0xAB5);
        check ((q == 5 || q == 7) && q2 == 3, "Shard: Quantize keeps the interval in the key (6 st in C major -> 5 or 7)", q);
    }
    {
        Shard::Processor s; s.prepare (SR, 256); s.p.pitchSt = 12; s.p.recycle = 0.7f; s.p.mix = 1; s.p.spliceMs = 150; s.p.delayMs = 100;
        std::vector<float> l ((size_t) SR * 3, 0.0f); for (size_t i = 0; i < 4800; ++i) l[i] = 0.4f * (float) std::sin (2 * pi * 220 * i / SR); auto r = l;
        runShard (s, l, r); const double late = rms (l, 96000, 120000);
        check (late > 0.003, "Shard: Recycle keeps feeding splices back (level 2 s after a 0.1 s burst)", db (late));
    }
    {
        Shard::Processor s; s.prepare (SR, 256); s.p.pitchSt = 0; s.p.mix = 1; s.p.dyn = Shard::dynGate; s.p.dynThreshDb = -20; s.p.keyOn = true; s.p.reverseProb = 0; s.p.dynReleaseMs = 50;
        auto l = sine (440, 2.0), r = l; std::vector<float> key (l.size(), 0.0f); for (size_t i = 48000; i < key.size(); ++i) key[i] = 0.5f;
        runShard (s, l, r, &key);
        const double closed = rms (l, 30000, 46000), open = rms (l, 70000, 90000);
        check (closed < 0.002 && open > 0.05, "Shard: Gate opens the wet only while the key is loud (dB open vs closed)", db (open / std::max (1e-9, closed)));
    }
    {
        Shard::Processor s; s.prepare (SR, 256); s.p.pitchSt = 0; s.p.lfo[0].target = Shard::tPitch; s.p.lfo[0].depth = 0.5f; s.p.lfo[0].rateHz = 2;
        auto l = sine (440, 1.0), r = l; float mn = 99, mx = -99;
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; s.process (ch, 2, nullptr, 256); mn = std::min (mn, s.pitchNow()); mx = std::max (mx, s.pitchNow()); }
        check (mx - mn > 20 && mx - mn < 25, "Shard: LFO 1 on Pitch at 50 % swings +-12 st (range st)", mx - mn);
    }
    //======================================================================== Murmuration
    {
        check (Murm::tapsFor (0) * 2 == 256 && Murm::tapsFor (1) * 2 == 2048, "Murmuration: Texture sets 256 .. 2048 taps (stereo)", Murm::tapsFor (1) * 2);
        Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 1000; m.p.texture = 0; m.p.y = 0; m.p.mix = 1; m.p.blur = 0; m.p.modDepth = 0;
        std::vector<float> l ((size_t) SR * 1.5, 0.0f); l[0] = 1; auto r = l; runMurm (m, l, r);
        int peaks = 0; std::vector<double> at;
        for (size_t i = 100; i + 100 < l.size(); i += 240) { const double e = rms (l, i, i + 240); if (e > 0.01) { if (at.empty() || (i - at.back()) > 4800) { at.push_back ((double) i); ++peaks; } } }
        check (peaks == 4, "Murmuration: Texture 0 is four discrete echoes across Time", peaks);
    }
    {
        Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 1500; m.p.texture = 0.6f; m.p.mix = 1;
        std::vector<float> l, r; TestSignals::pink (l, r, SR, 4.0, -18); auto in = l; runMurm (m, l, r);
        const double lv = db (rms (l, 96000, 192000) / rms (in, 96000, 192000));
        check (std::abs (lv) < 3.0, "Murmuration: the cloud keeps pink noise near its level (dB, wet only)", lv);
    }
    {
        auto lateShare = [] (float x)
        {
            Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 1000; m.p.texture = 1; m.p.x = x; m.p.y = 0.8f; m.p.mix = 1; m.p.modDepth = 0;
            std::vector<float> l ((size_t) SR * 1.2, 0.0f); l[0] = 1; auto r = l; runMurm (m, l, r);
            const double e1 = rms (l, 0, 16000), e3 = rms (l, 32000, 48000); return db (e3 / std::max (1e-9, e1));
        };
        const double swell = lateShare (0.95f), decay = lateShare (0.05f);
        check (swell > 6 && decay < -6, "Murmuration: X moves the envelope (swell: late third louder; decay: early louder, dB late vs early)", swell);
    }
    {
        Murm::Processor a; a.prepare (SR, 256); a.p.timeMs = 10; a.p.mix = 1; auto l = sine (300, 0.5), r = l; runMurm (a, l, r);
        Murm::Processor b; b.prepare (SR, 256); b.p.timeMs = 90000; b.p.mix = 1; b.p.texture = 1; auto l2 = sine (300, 2.0), r2 = l2; runMurm (b, l2, r2);
        bool fin = true; for (auto x : l) fin = fin && std::isfinite (x); for (auto x : l2) fin = fin && std::isfinite (x);
        check (fin && b.timeMsNow() > 89000, "Murmuration: Time 10 ms to 90 s, 2048 taps, finite", b.timeMsNow());
    }
    {
        Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 400; m.p.shimmer = 1; m.p.shimmerSt = 12; m.p.shimmerFade = 0; m.p.mix = 1; m.p.modDepth = 0; m.p.blur = 0;
        auto l = sine (500, 2.0), r = l; runMurm (m, l, r); const double f = freqOf (l, 48000, 96000);
        check (std::abs (f / 1000 - 1) < 0.02, "Murmuration: Shimmer +12 st lifts the cloud an octave (dominant Hz)", f);
    }
    {
        for (int mode = 0; mode < 2; ++mode)
        {
            Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 800; m.p.mix = 1; m.p.freezeMode = mode;
            std::vector<float> l, r; TestSignals::pink (l, r, SR, 5.0, -18);
            for (size_t i = (size_t) (SR * 1.5); i < l.size(); ++i) l[i] = r[i] = 0;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { m.p.freeze = i >= (size_t) (SR * 1.2); float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, 256); }
            const double e1 = rms (l, (size_t) (SR * 2.0), (size_t) (SR * 2.8)), e2 = rms (l, (size_t) (SR * 4.0), (size_t) (SR * 4.8));
            char buf[120]; std::snprintf (buf, sizeof buf, "Murmuration: %s Freeze holds the cloud after the input stops (dB change, level dBFS %.0f)", mode ? "spectral" : "loop", db (e1));
            check (std::abs (db (e2 / std::max (1e-9, e1))) < 2 && e1 > 0.01, buf, db (e2 / std::max (1e-9, e1)));
        }
        Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 600; m.p.mix = 1; m.p.infinite = true;
        std::vector<float> l, r; TestSignals::pink (l, r, SR, 8.0, -18); for (size_t i = (size_t) SR; i < l.size(); ++i) l[i] = r[i] = 0;
        runMurm (m, l, r); const double late = rms (l, (size_t) (SR * 6), (size_t) (SR * 7));
        bool fin = true; float pk = 0; for (auto x : l) { fin = fin && std::isfinite (x); pk = std::max (pk, std::abs (x)); }
        check (fin && late > 0.003 && pk < 2, "Murmuration: Infinite keeps the tail 5 s after the input stops (dBFS)", db (late));
    }
    {
        auto hfShare = [] (float color, bool late)
        {
            Murm::Processor m; m.prepare (SR, 256); m.p.timeMs = 1000; m.p.texture = 1; m.p.y = 0; m.p.color = color; m.p.mix = 1; m.p.modDepth = 0;
            std::vector<float> l ((size_t) SR * 1.2, 0.0f); for (int i = 0; i < 64; ++i) l[(size_t) i] = 0.8f; ModShapes::Rand rn; for (int i = 0; i < 2000; ++i) l[(size_t) i] += 0.3f * rn.next(); auto r = l; runMurm (m, l, r);
            Biquad h; h.set (Biquad::highpass, SR, 4000, 0.7); std::vector<float> hp (l.size()); for (size_t i = 0; i < l.size(); ++i) hp[i] = h.process (l[i]);
            const size_t a = late ? 38000 : 4000, b = a + 8000; return db (rms (hp, a, b) / std::max (1e-9, rms (l, a, b)));
        };
        const double early = hfShare (-1, false), later = hfShare (-1, true);
        check (later < early - 4, "Murmuration: Color darkens further along the tail (HF share late vs early, dB)", later - early);
    }
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
