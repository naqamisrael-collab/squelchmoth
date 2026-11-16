// Wingbeat Echo measurements. g++ -O2 -std=c++17 -I../Source wingbeat_test.cpp -o /tmp/wb && /tmp/wb
#include "SqWingbeatDsp.h"
#include <chrono>
#include <cstdio>
#include <string>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const std::string& w) { (ok ? passes : fails)++; std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", w.c_str()); }
static std::string f2 (double v, int d = 2) { char b[64]; std::snprintf (b, sizeof b, "%.*f", d, v); return b; }
static void run (Wingbeat::Processor& p, std::vector<float>& l, std::vector<float>& r, double bpm = 120, int block = 256)
{ for (size_t i = 0; i + block <= l.size(); i += block) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, nullptr, block, bpm); } }
static std::vector<int> peaks (const std::vector<float>& x, float thr, int minGap)
{ std::vector<int> v; int last = -minGap; for (int i = 1; i + 1 < (int) x.size(); ++i) if (std::abs (x[i]) > thr && std::abs (x[i]) >= std::abs (x[i - 1]) && std::abs (x[i]) >= std::abs (x[i + 1]) && i - last >= minGap) { v.push_back (i); last = i; } return v; }

int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    const double sr = 48000;
    std::printf ("== Wingbeat Echo ==\n");
    // 1. Digital Clean single: an impulse repeats at exactly the time, each repeat x feedback
    {
        Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p; p.style = 14; p.timeMs = 250; p.feedback = 0.5f; p.mix = 1; p.limiter = Wingbeat::limitOff;
        w.configure (120, true);
        std::vector<float> l (sr * 2, 0.0f), r; l[0] = 0.5f; r = l; run (w, l, r);
        auto pk = peaks (l, 0.01f, 1000);
        bool timing = pk.size() >= 4; double ratio = 0;
        for (size_t k = 0; k + 1 < pk.size() && k < 4; ++k) timing = timing && std::abs ((pk[k] - (int) (k + 1) * 12000)) <= 1;
        if (pk.size() >= 2) ratio = l[pk[1]] / l[pk[0]];
        check (timing && std::abs (ratio - 0.5) < 0.02, "Digital Clean: repeats at 250 / 500 / 750 ms to the sample, each x " + f2 (ratio, 3) + " (feedback 0.5)");
    }
    // 2. 30 s maximum and note values
    {
        Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p; p.style = 14; p.timeMs = 30000; p.feedback = 0; p.mix = 1; w.configure (120, true);
        std::vector<float> l (sr * 31, 0.0f), r; l[100] = 0.8f; r = l; run (w, l, r, 120, 1024);
        auto pk = peaks (l, 0.1f, 1000);
        check (! pk.empty() && std::abs (pk[0] - (100 + 30 * 48000)) <= 1, "30 s delay: the echo lands at " + f2 (pk.empty() ? 0 : (pk[0] - 100) / sr, 4) + " s");
        Wingbeat::Processor q; q.prepare (sr, 256); q.p.style = 14; q.p.syncL = 10; q.p.feedback = 0; q.p.mix = 1; q.configure (100, true);   // dotted 1/8 at 100 BPM = 450 ms
        std::vector<float> a (sr, 0.0f), b; a[0] = 0.8f; b = a; run (q, a, b, 100);
        auto pq = peaks (a, 0.1f, 1000);
        check (! pq.empty() && std::abs (pq[0] - 21600) <= 1, "note value 1/8 dotted at 100 BPM: " + f2 (pq.empty() ? 0 : pq[0] / sr * 1000, 2) + " ms (450)");
    }
    // 3. ping-pong alternates sides
    {
        Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p; p.style = 14; p.mode = Wingbeat::pingPong; p.timeMs = 200; p.feedback = 0.7f; p.mix = 1; w.configure (120, true);
        std::vector<float> l (sr, 0.0f), r (sr, 0.0f); l[0] = r[0] = 0.5f; run (w, l, r);
        const bool ok = std::abs (l[9600]) > 0.1f && std::abs (r[9600]) < 0.01f && std::abs (r[19200]) > 0.1f && std::abs (l[19200]) < 0.01f;
        check (ok, "ping-pong: first repeat left only, second right only");
    }
    // 4. self-oscillation at feedback 110 % stays bounded with the soft limiter, and freeze holds
    {
        Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p; p.style = 0; p.timeMs = 120; p.feedback = 1.1f; p.mix = 1; w.configure (120, true);
        std::vector<float> l, r; TestSignals::pink (l, r, sr, 8.0, -18); run (w, l, r);
        float pk = 0; bool fin = true; for (float x : l) { pk = std::max (pk, std::abs (x)); fin = fin && std::isfinite (x); }
        check (fin && pk < 2.0f, "feedback 110 %: self-oscillates but stays bounded (peak " + f2 (gainToDb (pk), 1) + " dBFS)");
        Wingbeat::Processor f; f.prepare (sr, 256); f.p.style = 14; f.p.timeMs = 300; f.p.feedback = 0.3f; f.p.mix = 1; f.p.limiter = Wingbeat::limitOff; f.configure (120, true);
        std::vector<float> a (sr * 6, 0.0f), b; for (int i = 0; i < 9000; ++i) a[i] = 0.3f * (float) std::sin (2 * pi * 440 * i / sr); b = a;
        for (size_t i = 0; i + 256 <= a.size(); i += 256) { if (i > sr) f.p.freeze = true; float* ch[2] = { a.data() + i, b.data() + i }; f.process (ch, 2, nullptr, 256, 120); }
        double e1 = 0, e2 = 0; for (int i = sr * 2; i < sr * 3; ++i) e1 += a[i] * a[i]; for (int i = sr * 5; i < sr * 6; ++i) e2 += a[i] * a[i];
        check (e1 > 0 && std::abs (10 * std::log10 (e2 / e1)) < 0.5, "freeze: the loop holds (level change over 3 s " + f2 (10 * std::log10 (e2 / e1), 2) + " dB)");
    }
    // 5. ducker
    {
        Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p; p.style = 14; p.timeMs = 100; p.feedback = 0.6f; p.mix = 1; p.duckDb = 18; w.configure (120, true);
        std::vector<float> l, r; TestSignals::pink (l, r, sr, 2.0, -12); run (w, l, r);
        check (w.duckGainDb() < -12.0f, "ducker: a -12 dBFS input pushes the echoes down " + f2 (w.duckGainDb(), 1) + " dB (set 18)");
    }
    // 6. glide changes pitch, jump does not glide, crossfade has no click
    {
        auto runChange = [&] (int mode)
        {
            Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p; p.style = 14; p.timeMs = 300; p.feedback = 0; p.mix = 1; p.timeChange = mode; w.configure (120, true);
            std::vector<float> l (sr * 2), r; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.3f * (float) std::sin (2 * pi * 1010 * i / sr); r = l;
            float maxStep = 0;
            for (size_t i = 0; i + 256 <= l.size(); i += 256)
            {
                if (i >= (size_t) sr) w.p.timeMs = 150;
                float* ch[2] = { l.data() + i, r.data() + i }; w.process (ch, 2, nullptr, 256, 120);
            }
            for (size_t i = sr - 256; i < sr * 1.2; ++i) maxStep = std::max (maxStep, std::abs (l[i] - l[i - 1]));
            return maxStep;
        };
        const float g = runChange (Wingbeat::glideChange), x = runChange (Wingbeat::crossfadeChange), j = runChange (Wingbeat::jumpChange);
        const float sineStep = 0.3f * (float) (2 * pi * 1010 / sr);
        check (x < sineStep * 1.6f && j > sineStep * 2.0f && g < sineStep * 3.0f, "time change: crossfade max step " + f2 (x / sineStep, 2) + "x a clean sine, jump " + f2 (j / sineStep, 2) + "x (a click), glide " + f2 (g / sineStep, 2) + "x (pitch sweep)");
    }
    // 7. rhythm: 32-tap cloud produces 32 taps; pattern taps land on the grid
    {
        Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p; p.style = 14; p.mode = Wingbeat::rhythm; p.pattern = 0; p.feedback = 0; p.mix = 1; w.configure (120, true);
        std::vector<float> l (sr * 3, 0.0f), r (sr * 3, 0.0f); l[0] = r[0] = 0.8f; run (w, l, r);
        auto pk = peaks (l, 0.02f, 2000);
        const int six = 6000;                                     // 1/16 at 120 BPM
        bool grid = pk.size() >= 4 && std::abs (pk[0] - 3 * six) <= 2 && std::abs (pk[1] - 4 * six) <= 2 && std::abs (pk[2] - 8 * six) <= 2 && std::abs (pk[3] - 12 * six) <= 2;
        check (grid, "rhythm 'Quarter + Dotted Eighth': taps at 3, 4, 8, 12 sixteenths (" + std::to_string (pk.size()) + " taps)");
        Wingbeat::Processor c; c.prepare (sr, 256); c.p.style = 14; c.p.mode = Wingbeat::rhythm; c.p.pattern = 11; c.p.feedback = 0; c.p.mix = 1; c.configure (120, true);
        std::vector<float> a (sr * 6, 0.0f), b (sr * 6, 0.0f); a[0] = b[0] = 0.8f; run (c, a, b);
        auto pc = peaks (a, 0.0005f, 1500);
        check (pc.size() >= 32, "rhythm '32-Tap Cloud': " + std::to_string (pc.size()) + " separate taps (the competitor stops at 16)");
    }
    // 8. every style is finite, bounded and in level on pink noise at the default settings; CPU
    {
        std::vector<float> l0, r0; TestSignals::pink (l0, r0, sr, 4.0, -18);
        double worstCpu = 0; std::string worst; bool ok = true;
        for (int s = 0; s < Wingbeat::numStyles(); ++s)
        {
            Wingbeat::Processor w; w.prepare (sr, 128); w.p.style = s; w.p.feedback = 0.5f; w.p.mix = 0.3f; w.configure (120, true);
            auto l = l0, r = r0;
            auto t0 = std::chrono::steady_clock::now(); run (w, l, r, 120, 128); auto t1 = std::chrono::steady_clock::now();
            const double cpu = std::chrono::duration<double> (t1 - t0).count() / 4.0 * 100;
            float pk = 0; bool fin = true; for (float x : l) { pk = std::max (pk, std::abs (x)); fin = fin && std::isfinite (x); }
            if (! fin || pk > 1.5f) { ok = false; std::printf ("      %s: peak %.1f dBFS%s\n", Wingbeat::style (s).name, gainToDb (pk), fin ? "" : " NaN"); }
            if (cpu > worstCpu) { worstCpu = cpu; worst = Wingbeat::style (s).name; }
        }
        check (ok, "all 36 styles: finite and bounded at feedback 50 %");
        check (worstCpu < 2.5, "CPU per instance, worst style " + worst + ": " + f2 (worstCpu, 2) + " % of one core");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passes, fails);
    return fails ? 1 : 0;
}
