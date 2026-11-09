// Measures the 1.11 Color & Space second wave, part 1 (Source/SqColor111bDsp.h): Vellum EQ, Tremor, Orbit Pan,
// Mirror Shift and Shutter Delay. g++ -O2 -std=c++17 -ISource tests/color111b_test.cpp -o /tmp/c111b && /tmp/c111b
#include "SqColor111bDsp.h"
#include <cstdio>
#include <functional>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const char* what, double v = 0) { std::printf ("%s  %-78s %10.3f\n", ok ? "PASS" : "FAIL", what, v); ok ? ++passes : ++fails; }
static const double SR = 48000;
static double rms (const std::vector<float>& x, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i]; return std::sqrt (e / (double) std::max<size_t> (1, b - a)); }
static double db (double g) { return 20 * std::log10 (std::max (1e-12, g)); }
static std::vector<float> sine (double hz, double secs, float amp = 0.25f) { std::vector<float> v ((size_t) (SR * secs)); for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2 * pi * hz * i / SR); return v; }
template <typename P> static void run (P& proc, std::vector<float>& l, std::vector<float>& r, int blk = 256, const std::vector<float>* kl = nullptr)
{
    for (size_t i = 0; i < l.size(); i += (size_t) blk)
    {
        const int m = (int) std::min<size_t> ((size_t) blk, l.size() - i);
        float* ch[2] = { l.data() + i, r.data() + i };
        if constexpr (std::is_same_v<P, Vellum::Processor> || std::is_same_v<P, Shutter::Processor>) proc.process (ch, 2, m);
        else
        {
            const float* k[2] = { kl ? kl->data() + i : nullptr, kl ? kl->data() + i : nullptr };
            proc.process (ch, 2, kl ? k : nullptr, m);
        }
    }
}
static double freqOf (const std::vector<float>& x, size_t a, size_t b)
{
    // zero crossings with linear interpolation
    double first = -1, last = -1; int count = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { const double t = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]); if (first < 0) first = t; else ++count; last = t; }
    return count > 0 ? count * SR / (last - first) : 0;
}

int main()
{
    //======================================================================== Vellum
    {
        Vellum::Processor v; v.prepare (SR, 256);
        auto resp = [&] (double hz) { return v.magnitudeDb (hz); };
        auto setUp = [&] { float* ch[2]; std::vector<float> a (16), b (16); ch[0] = a.data(); ch[1] = b.data(); v.process (ch, 2, 16); };
        v.p = {}; v.p.lowIdx = 2; v.p.lowDb = -12; setUp();
        check (std::abs (resp (25) + 12) < 1.5 && std::abs (resp (800)) < 0.5, "Vellum: low cut is a shelf (-12 dB below, flat above) at 100 Hz", resp (25));
        v.p = {}; v.p.lowIdx = 2; v.p.lowDb = 12; setUp();
        check (std::abs (resp (100) - 12) < 0.3 && resp (20) < 9 && resp (600) < 1.5, "Vellum: low boost is a very-low bell (+12 at 100 Hz, falling back below)", resp (20));
        v.p = {}; v.p.highIdx = 3; v.p.highDb = -15; setUp();
        check (resp (20000) < resp (14000) && resp (14000) < resp (10000) && resp (10000) < -3 && std::abs (resp (1000)) < 0.5, "Vellum: high cut is a gentle low-pass that keeps falling", resp (20000));
        v.p = {}; v.p.highIdx = 3; v.p.highDb = -3; setUp(); const double light = resp (10000);
        check (light > -1.5 && light < 0, "Vellum: a light high cut barely touches the marked frequency", light);
        v.p = {}; v.p.highIdx = 3; v.p.highDb = 12; setUp();
        check (std::abs (resp (10000) - 12) < 0.3 && resp (2000) < 2.5, "Vellum: high boost is a very-high bell", resp (2000));
        auto bw = [&] (float g) { v.p = {}; v.p.midIdx = 3; v.p.midDb = g; setUp(); double lo = 2300, hi = 2300; while (resp (lo) > g * 0.5) lo /= 1.01; while (resp (hi) > g * 0.5) hi *= 1.01; return std::log2 (hi / lo); };
        const double b2 = bw (2), b8 = bw (8);
        check (b8 < b2 * 0.6, "Vellum: mid Q narrows as gain rises (octaves at 8 dB vs 2 dB)", b8 / b2);
        v.p = {}; v.p.midIdx = 3; v.p.midDb = 8; setUp();
        check (std::abs (resp (2300) - 8) < 0.2, "Vellum: mid at 2300 Hz reaches +8 dB", resp (2300));
        v.p = {}; v.p.stepped = true; v.p.lowDb = 7.4f; v.p.midDb = 4.9f; setUp();
        check (v.lowDbUsed() == 6 && v.midDbUsed() == 4, "Vellum: Stepped detents (3 dB low / high, 2 dB mids)", v.lowDbUsed());
        v.p = {}; v.p.mid2On = true; v.p.mid2Hz = 450; v.p.mid2Db = -6; setUp();
        check (std::abs (resp (450) + 6) < 0.3, "Vellum: the second mid band sweeps (450 Hz, -6 dB)", resp (450));
        // Side only: a mono input passes untouched
        {
            Vellum::Processor s; s.prepare (SR, 256); s.p.target = Vellum::side; s.p.lowDb = 12; s.p.highDb = 12; s.p.oversampleLog2 = 0;
            auto l = sine (100, 0.5), r = l, ref = l; run (s, l, r);
            double err = 0; for (size_t i = 0; i < l.size(); ++i) err = std::max (err, (double) std::abs (l[i] - ref[i]));
            check (err < 1e-4, "Vellum: Side-only EQ leaves a mono signal untouched", err);
            Vellum::Processor m; m.prepare (SR, 256); m.p.target = Vellum::mid; m.p.lowIdx = 2; m.p.lowDb = 12; m.p.oversampleLog2 = 0;
            auto l2 = sine (100, 0.5), r2 = l2; run (m, l2, r2);
            check (std::abs (db (rms (l2, 12000, 24000) / rms (ref, 12000, 24000)) - 12) < 0.5, "Vellum: Mid-only EQ boosts the mono signal", db (rms (l2, 12000, 24000) / rms (ref, 12000, 24000)));
        }
        // Drive holds level (auto gain) and adds harmonics
        {
            Vellum::Processor d; d.prepare (SR, 256); d.p.drive = 0.8f;
            auto l = sine (220, 3.0, 0.3f), r = l, ref = l; run (d, l, r);
            const double lv = db (rms (l, 96000, 144000) / rms (ref, 96000, 144000));
            check (std::abs (lv) < 1.0, "Vellum: Drive 80 % holds the level (auto gain)", lv);
            // harmonic content: energy left after removing the fundamental by projection
            double s = 0, c = 0; for (size_t i = 96000; i < 144000; ++i) { s += l[i] * std::sin (2 * pi * 220 * i / SR); c += l[i] * std::cos (2 * pi * 220 * i / SR); }
            const double a1 = 2 * std::sqrt (s * s + c * c) / 48000, tot = rms (l, 96000, 144000) * std::sqrt (2.0);
            const double thd = std::sqrt (std::max (0.0, tot * tot - a1 * a1)) / a1;
            check (thd > 0.01 && thd < 0.5, "Vellum: Drive adds harmonics (THD)", thd);
            check (d.latencySamples() > 0 && d.latencySamples() < 20, "Vellum: the oversampled Drive reports its latency (samples)", d.latencySamples());
        }
    }
    //======================================================================== Tremor
    {
        auto envOf = [] (const std::vector<float>& y, size_t a, size_t b, size_t win) { std::vector<double> e; for (size_t i = a; i + win <= b; i += win) e.push_back (rms (y, i, i + win)); return e; };
        {
            Tremor::Processor t; t.prepare (SR, 256); t.p.depth = 1; t.p.shape = WingMod::square; t.p.sync = false; t.p.rateHz = 4; t.p.smoothMs = 0.5f;
            std::vector<float> l (SR * 2, 0.25f), r = l; run (t, l, r);
            float mn = 1; for (size_t i = 24000; i < l.size(); ++i) mn = std::min (mn, std::abs (l[i]));
            check (db (mn / 0.25) < -60, "Tremor: depth 100 % square is a full gate (dB at the bottom)", db (mn / 0.25));
            // rate: count gate openings over 1.5 s
            int opens = 0; for (size_t i = 24001; i < l.size(); ++i) if (l[i - 1] < 0.125f && l[i] >= 0.125f) ++opens;
            check (std::abs (opens / 1.5 - 4.0) < 0.7, "Tremor: free rate 4 Hz (openings per second)", opens / 1.5);
        }
        {
            Tremor::Processor t; t.prepare (SR, 256); t.p.depth = 1; t.p.shape = WingMod::square; t.p.sync = true; t.p.syncIndex = 9; t.p.smoothMs = 0.5f;   // 1/8 at 120 = 4 Hz
            t.setTransport (120, 0, true);
            std::vector<float> l (SR * 2, 0.25f), r = l;
            for (size_t i = 0; i < l.size(); i += 256) { t.setTransport (120, (double) i / SR * 2.0, true); float* ch[2] = { l.data() + i, r.data() + i }; t.process (ch, 2, nullptr, 256); }
            int opens = 0; for (size_t i = 1; i < l.size(); ++i) if (l[i - 1] < 0.125f && l[i] >= 0.125f) ++opens;
            check (opens == 8 || opens == 7, "Tremor: synced 1/8 at 120 BPM locked to the bar (openings in 2 s)", opens);
        }
        {
            Tremor::Processor t; t.prepare (SR, 256); t.p.depth = 1; t.p.sync = false; t.p.rateHz = 3; t.p.stereoDeg = 180;
            std::vector<float> l (SR * 2, 0.25f), r = l; run (t, l, r);
            double c = 0, a = 0, b = 0; for (size_t i = 24000; i < l.size(); ++i) { const double x = l[i] - 0.125, y = r[i] - 0.125; c += x * y; a += x * x; b += y * y; }
            check (c / std::sqrt (a * b) < -0.95, "Tremor: stereo phase 180 deg moves L and R in opposition", c / std::sqrt (a * b));
        }
        {
            Tremor::Processor t; t.prepare (SR, 256); t.p.depth = 1; t.p.sync = false; t.p.rateHz = 2; t.p.harmonic = true; t.p.xoverHz = 700;
            auto lo = sine (150, 2.0), hi = sine (3000, 2.0); std::vector<float> l (lo.size()); for (size_t i = 0; i < l.size(); ++i) l[i] = lo[i] + hi[i]; auto r = l;
            run (t, l, r);
            // split the output back with steep filters and compare envelopes
            Biquad f1, f2, f3, f4; f1.set (Biquad::lowpass, SR, 400, 0.7); f2.set (Biquad::lowpass, SR, 400, 0.7); f3.set (Biquad::highpass, SR, 1500, 0.7); f4.set (Biquad::highpass, SR, 1500, 0.7);
            std::vector<float> a (l.size()), b (l.size()); for (size_t i = 0; i < l.size(); ++i) { a[i] = f2.process (f1.process (l[i])); b[i] = f4.process (f3.process (l[i])); }
            auto ea = envOf (a, 24000, l.size(), 960), eb = envOf (b, 24000, l.size(), 960);
            double ma = 0, mb = 0; for (size_t i = 0; i < ea.size(); ++i) { ma += ea[i]; mb += eb[i]; } ma /= ea.size(); mb /= eb.size();
            double c = 0, va = 0, vb = 0; for (size_t i = 0; i < ea.size(); ++i) { c += (ea[i] - ma) * (eb[i] - mb); va += (ea[i] - ma) * (ea[i] - ma); vb += (eb[i] - mb) * (eb[i] - mb); }
            check (c / std::sqrt (va * vb) < -0.8, "Tremor: harmonic mode moves lows and highs in opposition", c / std::sqrt (va * vb));
        }
        {
            Tremor::Processor t; t.prepare (SR, 256); t.p.mode = Tremor::rhythm; t.p.depth = 1; t.p.shape = WingMod::square; t.p.smoothMs = 0.5f;
            for (int s = 0; s < 16; ++s) t.p.steps[s] = (s % 2 == 0) ? 1.0f : 0.0f;
            std::vector<float> l (SR * 2, 0.25f), r = l; run (t, l, r);
            // steps are 1/16 at 120 = 125 ms; odd steps have zero depth so they never drop
            int dropsInOdd = 0, dropsInEven = 0;
            for (int s = 2; s < 15; ++s) { float mn = 1; for (size_t i = (size_t) (s * 6000 + 3200); i < (size_t) (s * 6000 + 5800); ++i) mn = std::min (mn, l[i]); if (mn < 0.05f) (s % 2 ? dropsInOdd : dropsInEven)++; }
            check (dropsInOdd == 0 && dropsInEven >= 5, "Tremor: per-step depth in Rhythm mode (odd steps at 0 never gate)", dropsInEven);
        }
        {
            Tremor::Processor t; t.prepare (SR, 256); t.p.depth = 1; t.p.sync = false; t.p.rateHz = 5; t.p.dyn = Tremor::dynGate; t.p.thresholdDb = -20;
            std::vector<float> l (SR * 1, 0.01f), r = l; run (t, l, r);
            float mn = 1; for (size_t i = 12000; i < l.size(); ++i) mn = std::min (mn, l[i]);
            check (mn > 0.0099f, "Tremor: Gate dynamics: quiet input below threshold is not modulated", mn / 0.01f);
            Tremor::Processor k; k.prepare (SR, 256); k.p.depth = 1; k.p.sync = false; k.p.rateHz = 5; k.p.dyn = Tremor::dynGate; k.p.thresholdDb = -20; k.p.keyOn = true;
            std::vector<float> l2 (SR * 1, 0.01f), r2 = l2, key (SR * 1, 0.5f); run (k, l2, r2, 256, &key);
            float mn2 = 1; for (size_t i = 12000; i < l2.size(); ++i) mn2 = std::min (mn2, l2[i]);
            check (mn2 < 0.001f, "Tremor: an external key opens the Gate on that same quiet input", mn2 / 0.01f);
        }
        {
            Tremor::Processor t; t.prepare (SR, 256); t.p.depth = 0.8f; t.p.mix = 0; std::vector<float> l = sine (440, 0.5), r = l, ref = l; run (t, l, r);
            double err = 0; for (size_t i = 0; i < l.size(); ++i) err = std::max (err, (double) std::abs (l[i] - ref[i]));
            check (err < 1e-6, "Tremor: Mix 0 % is the dry signal", err);
            Tremor::Processor a; a.prepare (SR, 256); a.p.rateModAmt = 1; a.p.rateModHz = 1500; a.p.analog = 3; a.p.analogAmount = 0.8f; a.p.depth = 1;
            auto l2 = sine (440, 1.0), r2 = l2; run (a, l2, r2); bool fin = true; for (auto x : l2) fin = fin && std::isfinite (x);
            check (fin && rms (l2, 12000, 48000) > 0.01, "Tremor: audio-rate Rate Mod + Analog Style stay finite", rms (l2, 12000, 48000));
        }
    }
    //======================================================================== Orbit Pan
    {
        {
            Orbit::Processor o; o.prepare (SR, 256); o.p.rateHz = 1; o.p.widthDeg = 180; o.p.smoothMs = 1;
            auto l = sine (500, 3.0), r = l; run (o, l, r);
            // L energy peaks once a second
            std::vector<double> e; for (size_t i = 48000; i + 2400 <= l.size(); i += 2400) e.push_back (rms (l, i, i + 2400) - rms (r, i, i + 2400));
            int cross = 0; for (size_t i = 1; i < e.size(); ++i) if (e[i - 1] < 0 && e[i] >= 0) ++cross;
            check (cross == 2, "Orbit: LFO 1 Hz pans L -> R -> L once a second (crossings in 2 s)", cross);
        }
        {
            float gl, gr; Orbit::lawGains (Orbit::law3dB, 0, gl, gr);
            check (std::abs (db (gl) + 3.01) < 0.05, "Orbit: -3 dB law centre gain", db (gl));
            Orbit::lawGains (Orbit::law4p5dB, 0, gl, gr); check (std::abs (db (gl) + 4.5) < 0.1, "Orbit: -4.5 dB law centre gain", db (gl));
            Orbit::lawGains (Orbit::law6dB, 0, gl, gr); check (std::abs (db (gl) + 6.02) < 0.05, "Orbit: -6 dB law centre gain", db (gl));
            Orbit::lawGains (Orbit::law3dB, 1, gl, gr); check (gl < 1e-6 && std::abs (gr - 1) < 1e-6, "Orbit: hard right is unity on R, silent on L", gr);
        }
        {
            Orbit::Processor o; o.prepare (SR, 256); o.p.mode = Orbit::pingPong; o.p.stepDivision = 9; o.p.smoothMs = 1; o.p.widthDeg = 180;   // 1/8 at 120 = 250 ms
            std::vector<float> l (SR * 2, 0.2f), r = l; run (o, l, r);
            int flips = 0; bool left = l[12000] > r[12000]; for (size_t i = 12000; i < l.size(); i += 240) { const bool now = l[i] > r[i]; if (now != left) { ++flips; left = now; } }
            check (flips >= 6 && flips <= 8, "Orbit: Ping-Pong flips every 1/8 at 120 BPM (flips in 1.75 s)", flips);
        }
        {
            Orbit::Processor o; o.prepare (SR, 256); o.p.mode = Orbit::randomStep; o.p.audioTrigger = true; o.p.divider = 3; o.p.trigThreshDb = -20;
            std::vector<float> l (SR * 3, 0.0f), r;
            for (int k = 0; k < 12; ++k) for (size_t i = (size_t) (k * 0.2 * SR); i < (size_t) (k * 0.2 * SR + 2000); ++i) l[i] = 0.5f * (float) std::sin (i * 0.1);
            r = l; run (o, l, r);
            check (o.onsets() == 12, "Orbit: the audio trigger counts every onset", o.onsets());
            Orbit::Processor q; q.prepare (SR, 256); q.p = o.p; std::vector<float> l2 (l.size(), 0.0f); for (int k = 0; k < 12; ++k) for (size_t i = (size_t) (k * 0.2 * SR); i < (size_t) (k * 0.2 * SR + 2000); ++i) l2[i] = 0.5f * (float) std::sin (i * 0.1);
            auto r2 = l2; int changes = 0; float lastPos = 99;
            for (size_t i = 0; i < l2.size(); i += 256) { float* ch[2] = { l2.data() + i, r2.data() + i }; q.process (ch, 2, nullptr, (int) std::min<size_t> (256, l2.size() - i)); if (q.currentStep() != (int) lastPos) { ++changes; lastPos = (float) q.currentStep(); } }
            check (changes >= 4 && changes <= 5, "Orbit: Divider 3 advances on every third onset (steps from 12 onsets)", changes);
        }
        {
            // Circle: behind is darker
            Orbit::Processor o; o.prepare (SR, 256); o.p.mode = Orbit::circle; o.p.rateHz = 0.5f; o.p.depth = 1; o.p.smoothMs = 1;
            std::vector<float> l (SR * 4), r; ModShapes::Rand rn; for (auto& x : l) x = 0.2f * rn.next(); r = l; run (o, l, r);
            // front at phase 0 (t = 2 s, 4 s), back at phase 0.5 (t = 1 s, 3 s): high-band energy
            auto hiE = [&] (size_t a) { Biquad h; h.set (Biquad::highpass, SR, 6000, 0.7); double e = 0; for (size_t i = a - 4800; i < a + 4800; ++i) { const float y = h.process (l[i] + r[i]); if (i > a - 2400) e += y * y; } return e; };
            const double front = hiE (96000), back = hiE (144000);
            check (10 * std::log10 (back / front) < -6, "Orbit: Circle mode darkens the image behind (high band, dB back vs front)", 10 * std::log10 (back / front));
        }
        {
            // Binaural: hard right gives the far (left) ear a delay of about 0.66 ms
            Orbit::Processor o; o.prepare (SR, 256); o.p.binaural = true; o.p.offsetDeg = 90; o.p.widthDeg = 0; o.p.smoothMs = 0.1f;
            std::vector<float> l (SR, 0.0f), r; ModShapes::Rand rn; for (auto& x : l) x = 0.3f * rn.next(); r = l; run (o, l, r);
            int best = 0; double bc = -1; for (int lag = 0; lag < 60; ++lag) { double c = 0; for (size_t i = 20000; i < 40000; ++i) c += (double) r[i] * l[i + lag]; if (c > bc) { bc = c; best = lag; } }
            check (std::abs (best / SR * 1000 - 0.66) < 0.06, "Orbit: binaural ITD on a hard-right source (ms)", best / SR * 1000);
            Orbit::Processor b; b.prepare (SR, 256); b.p.offsetDeg = 105; b.p.widthDeg = 0; b.p.smoothMs = 0.1f;
            std::vector<float> l2 (SR, 0.0f), r2; for (auto& x : l2) x = 0.3f * rn.next(); r2 = l2; run (b, l2, r2);
            double c = 0; for (size_t i = 20000; i < 40000; ++i) c += (double) l2[i] * r2[i];
            check (c < 0, "Orbit: 105 deg goes beyond the speaker (anti-phase on the far side)", c);
        }
    }
    //======================================================================== Mirror Shift
    {
        for (int st = 0; st < 3; ++st)
        {
            Mirror::Processor m; m.prepare (SR, 256); m.p.style = st; m.p.detune = 1; m.p.delay = 0.3f; m.p.mix = 1;
            if (st == 2) { auto c = m.centsFor (0); (void) c; }
            auto l = sine (1000, 2.0), r = l, dry = l; run (m, l, r);
            // wet-only = out - dry
            std::vector<float> wl (l.size()), wr (l.size()); for (size_t i = 0; i < l.size(); ++i) { wl[i] = l[i] - dry[i]; wr[i] = r[i] - dry[i]; }
            const double fl = freqOf (wl, 24000, 96000), fr = freqOf (wr, 24000, 96000);
            const double cl = 1200 * std::log2 (fl / 1000), cr = 1200 * std::log2 (fr / 1000);
            const double want = Mirror::style (st).maxCents;
            char buf[128]; std::snprintf (buf, sizeof buf, "Mirror: %s detune 100 %% shifts L up / R down (cents L)", Mirror::style (st).name);
            check (std::abs (cl - want) < want * 0.25 + 1 && std::abs (cr + want) < want * 0.25 + 1, buf, cl);
        }
        {
            Mirror::Processor m; m.prepare (SR, 256); m.p.focusHz = 2000; m.p.detune = 1; m.p.mix = 1;
            auto l = sine (100, 1.0), r = l, dry = l; run (m, l, r);
            double err = 0; for (size_t i = 24000; i < l.size(); ++i) err = std::max (err, (double) std::abs (l[i] - dry[i]));
            check (err < 0.004, "Mirror: Focus 2 kHz leaves a 100 Hz tone alone (max wet)", err);
        }
        {
            Mirror::Processor m; m.prepare (SR, 256); m.p.mix = 0; std::vector<float> l (SR, 0), r; ModShapes::Rand rn; for (auto& x : l) x = 0.2f * rn.next(); r = l; run (m, l, r);
            check (m.correlation() > 0.999 && std::abs (m.monoLossDb()) < 0.01, "Mirror: meter reads correlation 1 / 0 dB loss on mono at Mix 0", m.correlation());
            Mirror::Processor w; w.prepare (SR, 256); w.p.mix = 1; w.p.detune = 1; w.p.delay = 0.6f; std::vector<float> l2 (SR), r2; for (auto& x : l2) x = 0.2f * rn.next(); r2 = l2; run (w, l2, r2);
            check (w.correlation() < 0.9 && w.monoLossDb() < -0.1 && w.monoLossDb() > -3.0, "Mirror: widened mono reads lower correlation and a real mono-sum loss (dB)", w.monoLossDb());
        }
        {
            Mirror::Processor m; m.prepare (SR, 256); m.p.mix = 1; m.p.detune = 1; m.p.duckDb = 18; m.p.duckThreshDb = -30; m.p.keyOn = true;
            auto l = sine (1000, 1.0), r = l; std::vector<float> key (l.size(), 0.5f); run (m, l, r, 256, &key);
            check (m.duckGainDb() < -17, "Mirror: a loud key ducks the width (dB)", m.duckGainDb());
            Mirror::Processor t; t.prepare (SR, 256); t.p.style = 3; t.p.detune = 0.7f; t.p.mix = 1; auto l2 = sine (300, 1.0), r2 = l2; run (t, l2, r2);
            bool fin = true; for (auto x : l2) fin = fin && std::isfinite (x); check (fin, "Mirror: Tape ADT and Prism run finite", 1);
        }
    }
    //======================================================================== Shutter Delay
    {
        for (int mlt = 0; mlt < 4; ++mlt)
        {
            const double bw = Shutter::bandwidthHz (mlt);
            auto gainAt = [&] (double hz)
            {
                Shutter::Processor s; s.prepare (SR, 256); s.p.multiply = mlt; s.p.timeAMs = 5; s.p.feedbackA = 0; s.p.mix = 1; s.p.bits = 0;
                auto l = sine (hz, 0.6, 0.2f), r = l; run (s, l, r); return db (rms (l, 14400, 28800) / (0.2 / std::sqrt (2.0)));
            };
            const double in = gainAt (bw * 0.5), out = gainAt (bw * 1.5);
            char buf[128]; std::snprintf (buf, sizeof buf, "Shutter: Multiply x%d bandwidth %.1f kHz (dB at 1.5x bw vs 0.5x)", 1 << mlt, bw / 1000);
            check (std::abs (in) < 1.5 && out - in < -25, buf, out - in);
        }
        for (int mlt = 0; mlt < 4; ++mlt)
        {
            Shutter::Processor s; s.prepare (SR, 256); s.p.multiply = mlt; s.p.timeAMs = 100; s.p.feedbackA = 0; s.p.mix = 1; s.p.bits = 0;
            std::vector<float> l ((size_t) (SR * 1.2), 0.0f), r; for (int i = 0; i < 48; ++i) l[(size_t) i] = 0.8f * (float) std::sin (pi * i / 48.0); r = l; run (s, l, r);
            size_t pk = 0; for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > std::abs (l[pk])) pk = i;
            const double ms = (double) pk / SR * 1000 - 0.5, want = 100.0 * (1 << mlt);
            char buf[128]; std::snprintf (buf, sizeof buf, "Shutter: Time 100 ms x%d echoes at %.0f ms (measured ms)", 1 << mlt, want);
            check (std::abs (ms - want) < 1.5, buf, ms);
        }
        {
            Shutter::Processor s; s.prepare (SR, 256); s.p.multiply = 3; s.p.timeAMs = 1250;
            check (std::abs (s.totalMs (0) - 10000) < 1, "Shutter: Time 1250 ms x8 = a 10 s line", s.totalMs (0));
        }
        {
            Shutter::Processor s; s.prepare (SR, 256); s.p.algo = Shutter::pingPong; s.p.timeAMs = 100; s.p.timeBMs = 100; s.p.feedbackA = 0.6f; s.p.mix = 1; s.p.bits = 0;
            std::vector<float> l ((size_t) (SR * 0.5), 0.0f), r; for (int i = 0; i < 48; ++i) l[(size_t) i] = 0.8f * (float) std::sin (pi * i / 48.0); r = l; run (s, l, r);
            auto e = [&] (std::vector<float>& x, double t) { return rms (x, (size_t) (t * SR) - 200, (size_t) (t * SR) + 400); };
            check (e (l, 0.1) > 5 * e (r, 0.1) && e (r, 0.2) > 5 * e (l, 0.2), "Shutter: Ping-Pong goes L then R", e (l, 0.1) / std::max (1e-9, e (r, 0.1)));
        }
        {
            Shutter::Processor s; s.prepare (SR, 256); s.p.timeAMs = 300; s.p.feedbackA = 0.5f; s.p.mix = 1; s.p.bits = 0;
            ModShapes::Rand rn; std::vector<float> l ((size_t) (SR * 4), 0.0f), r; for (size_t i = 0; i < (size_t) (SR * 0.3); ++i) l[i] = 0.2f * rn.next(); r = l;
            for (size_t i = 0; i < l.size(); i += 256) { if (i >= (size_t) (SR * 0.35)) s.p.freeze = true; float* ch[2] = { l.data() + i, r.data() + i }; s.process (ch, 2, 256); }
            const double e1 = rms (l, (size_t) (SR * 1.0), (size_t) (SR * 1.6)), e2 = rms (l, (size_t) (SR * 3.1), (size_t) (SR * 3.7));
            check (std::abs (db (e2 / e1)) < 1.0 && e1 > 0.01, "Shutter: Freeze holds the loop (dB change over 2 s)", db (e2 / e1));
        }
        {
            auto worstStep = [] (bool smooth)
            {
                Shutter::Processor s; s.prepare (SR, 256); s.p.timeAMs = 200; s.p.feedbackA = 0; s.p.mix = 1; s.p.bits = 0; s.p.smooth = smooth;
                auto l = sine (200, 2.0, 0.3f), r = l; double worst = 0;
                for (size_t i = 0; i < l.size(); i += 256) { if (i >= (size_t) (SR * 1.0)) s.p.timeAMs = 333; float* ch[2] = { l.data() + i, r.data() + i }; s.process (ch, 2, 256); }
                for (size_t i = (size_t) (SR * 0.9); i < (size_t) (SR * 1.3); ++i) worst = std::max (worst, (double) std::abs (l[i] - l[i - 1]));
                return worst;
            };
            const double sm = worstStep (true), gl = worstStep (false);
            check (sm < 0.012 && gl > sm * 1.5, "Shutter: Smooth time change has no jump; Glitch does (worst sample step, smooth)", sm);
        }
        {
            auto snr = [] (int bits) { auto x = sine (440, 0.3, 0.25f); double e = 0, n = 0; for (auto v : x) { const float q = Shutter::quantise (v, bits); e += (double) v * v; n += (double) (q - v) * (q - v); } return 10 * std::log10 (e / n); };
            const double s12 = snr (1), s8 = snr (2);
            check (s12 > 55 && s8 > 25 && s8 < 45, "Shutter: 12-bit / 8-bit companding colour (SNR dB, 8-bit)", s8);
            check (std::abs (Shutter::quantise (Shutter::quantise (0.3f, 2), 2) - Shutter::quantise (0.3f, 2)) < 1e-6, "Shutter: the quantiser is idempotent (Freeze stays put)", 0);
        }
        for (int a = 0; a < Shutter::numAlgos; ++a)
        {
            Shutter::Processor s; s.prepare (SR, 256); s.p.algo = a; s.p.feedbackA = 1.1f; s.p.feedbackB = 1.1f; s.p.lfoHz = 256; s.p.lfoDepth = 1; s.p.mix = 1; s.p.bits = 2; s.p.multiply = a % 4;
            ModShapes::Rand rn; std::vector<float> l ((size_t) (SR * 3)), r; for (auto& x : l) x = 0.3f * rn.next(); r = l; run (s, l, r);
            float pk = 0; bool fin = true; for (size_t i = 0; i < l.size(); ++i) { fin = fin && std::isfinite (l[i]) && std::isfinite (r[i]); pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
            char buf[128]; std::snprintf (buf, sizeof buf, "Shutter: %s at 110 %% feedback stays bounded (peak)", Shutter::algoName (a));
            check (fin && pk < 2.0f, buf, pk);
        }
    }
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
