// Spindle Delay DSP core: measured. Build: scripts/build_spindle_test.sh
#include "SqSpindleDsp.h"
#include <cstdio>
#include <chrono>
#include <functional>

using namespace SqSpindle;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }

static void run (Engine& e, std::vector<float>& l, std::vector<float>& r, std::function<void (Engine&, int)> per = {})
{
    for (int s = 0; s < (int) l.size(); s += 256)
    {
        const int n = std::min (256, (int) l.size() - s);
        if (per) per (e, s);
        float* ch[2] = { l.data() + s, r.data() + s }; e.process (ch, 2, n);
    }
}
static Params plain()
{
    Params p; p.character = digital; p.mix = 1; p.feedback = 0.5; p.lowCutHz = 10; p.highCutHz = 20000; p.drive = 0; p.wow = 0; p.flutter = 0; p.diffusion = 0; p.width = 1; return p;
}
/** Peaks of the echoes of an impulse: (time in samples, |amplitude|) for each local maximum above 1e-6. */
static std::vector<std::pair<int, double>> echoes (const std::vector<float>& v, int spacing)
{
    std::vector<std::pair<int, double>> e;
    for (int start = spacing / 2; start + spacing <= (int) v.size(); start += spacing)
    {
        int at = start; double m = 0; for (int i = start; i < start + spacing; ++i) if (std::abs (v[(size_t) i]) > m) { m = std::abs (v[(size_t) i]); at = i; }
        if (m > 1e-6) e.push_back ({ at, m });
    }
    return e;
}

int main()
{
    juce::ScopedNoDenormals nd;
    const double fs = 48000;
    std::printf ("Spindle Delay DSP\n");

    // 1. Time and feedback: echoes land on time and fall by the feedback per repeat
    {
        auto p = plain(); p.timeLMs = p.timeRMs = 250; p.feedback = 0.5;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) (fs * 1.6), 0.0f); l[0] = 1; auto r = l; run (e, l, r);
        const auto ec = echoes (l, (int) (0.25 * fs));
        bool ok = ec.size() >= 5; double worstT = 0, worstG = 0;
        for (size_t k = 0; k + 1 < ec.size() && k < 5; ++k)
        {
            worstT = std::max (worstT, std::abs (ec[k].first - (k + 1) * 0.25 * fs));
            if (k > 0) worstG = std::max (worstG, std::abs (db (ec[k].second / ec[k - 1].second) - db (0.5)));
        }
        check (ok && worstT <= 1 && worstG < 0.05, "250 ms, feedback 50 %: repeats within " + juce::String (worstT, 0) + " sample(s) of the grid, each " + juce::String (db (0.5), 2) + " dB (+-" + juce::String (worstG, 3) + ")");
    }
    // 2. Ping-pong alternates sides
    {
        auto p = plain(); p.routing = pingPong; p.timeLMs = p.timeRMs = 100; p.feedback = 0.7;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) (fs * 0.65), 0.0f), r = l; l[0] = 1; r[0] = 1; run (e, l, r);
        bool ok = true; juce::String pat;
        for (int k = 1; k <= 5; ++k)
        {
            const int a = (int) (k * 0.1 * fs) - 200, b = a + 400; double ml = 0, mr = 0;
            for (int i = a; i < b; ++i) { ml = std::max (ml, (double) std::abs (l[(size_t) i])); mr = std::max (mr, (double) std::abs (r[(size_t) i])); }
            const bool left = ml > mr; pat << (left ? "L" : "R"); if (left != (k % 2 == 1)) ok = false;
        }
        check (ok, "Ping-pong: repeats alternate " + pat);
    }
    // 3. Filters in the loop: each repeat gets darker
    {
        auto p = plain(); p.timeLMs = p.timeRMs = 200; p.feedback = 0.9; p.highCutHz = 3000;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) (fs * 1.2), 0.0f); for (int i = 0; i < 480; ++i) l[(size_t) i] = (float) (0.5 * std::sin (2 * pi * 8000 * i / fs)); auto r = l; run (e, l, r);
        auto peak = [&] (int k) { double m = 0; const int a = (int) (k * 0.2 * fs); for (int i = a; i < a + 960; ++i) m = std::max (m, (double) std::abs (l[(size_t) i])); return db (m); };
        const double r1 = peak (1), r3 = peak (3);
        check (r3 < r1 - 15, "High cut 3 kHz in the loop: an 8 kHz burst is " + juce::String (r1, 1) + " dB on repeat 1, " + juce::String (r3, 1) + " dB on repeat 3");
    }
    // 4. Freeze holds the loop
    {
        auto p = plain(); p.timeLMs = p.timeRMs = 300; p.feedback = 0.5;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) (fs * 4), 0.0f); juce::Random rnd (1); for (int i = 0; i < (int) (0.25 * fs); ++i) l[(size_t) i] = (float) (rnd.nextDouble() - 0.5); auto r = l;
        run (e, l, r, [&] (Engine& en, int s) { en.p.freeze = s >= (int) (0.28 * fs); });
        auto rmsAt = [&] (double t) { double s = 0; const int a = (int) (t * fs); for (int i = a; i < a + (int) (0.3 * fs); ++i) s += (double) l[(size_t) i] * l[(size_t) i]; return std::sqrt (s / (0.3 * fs)); };
        const double a = rmsAt (0.6), b = rmsAt (3.6);
        check (std::abs (db (b / a)) < 0.5, "Freeze: the loop holds - " + juce::String (db (b / a), 2) + " dB after 3 more seconds (feedback 50 % alone: -60 dB)");
    }
    // 5. Runaway feedback stays bounded by the drive
    {
        auto p = plain(); p.timeLMs = 50; p.timeRMs = 73; p.feedback = 1.1; p.drive = 0.5; p.routing = cross; p.crossFeed = 0.5;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) (fs * 10), 0.0f); l[0] = 1; auto r = l; run (e, l, r);
        double pk = 0; bool fin = true; for (auto v : l) { pk = std::max (pk, (double) std::abs (v)); if (! std::isfinite (v)) fin = false; }
        check (fin && pk < 1.2, "Feedback 110 % with drive: it self-oscillates but stays bounded (peak " + juce::String (pk, 3) + ")");
    }
    // 6. Tape glide vs digital jump when the time changes
    {
        auto measure = [&] (int character, double& maxStep)
        {
            auto p = plain(); p.character = character; p.wow = 0; p.flutter = 0; p.timeLMs = p.timeRMs = 200; p.feedback = 0;
            Engine e; e.p = p; e.prepare (fs, 256);
            std::vector<float> l ((size_t) (fs * 2)); for (size_t i = 0; i < l.size(); ++i) l[i] = (float) (0.5 * std::sin (2 * pi * 440 * i / fs)); auto r = l;
            std::vector<float> times; run (e, l, r, [&] (Engine& en, int s) { if (s >= (int) fs) en.p.timeLMs = en.p.timeRMs = 300; times.push_back (en.meterTimeLMs); });
            maxStep = 0; for (size_t i = 8; i < times.size(); ++i) maxStep = std::max (maxStep, (double) std::abs (times[i] - times[i - 1]));
            return (double) times.back();
        };
        double stepT, stepD;
        const double tEnd = measure (tape, stepT), dEnd = measure (digital, stepD);
        check (std::abs (tEnd - 300) < 0.5 && stepT < 60 && std::abs (dEnd - 300) < 0.01 && stepD > 99, "Time 200 -> 300 ms: Tape glides (largest step per block " + juce::String (stepT, 1) + " ms), Digital jumps cleanly (" + juce::String (stepD, 0) + " ms, cross-faded)");
    }
    // 7. Wow and flutter: the pitch wobble of a tape echo, measured on a held tone
    {
        auto p = plain(); p.character = tape; p.wow = -1; p.flutter = -1; p.timeLMs = p.timeRMs = 300; p.feedback = 0; p.drive = 0; p.highCutHz = 20000;   // the character's own wow and flutter
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) (fs * 4)); for (size_t i = 0; i < l.size(); ++i) l[i] = (float) (0.5 * std::sin (2 * pi * 1000 * i / fs)); auto r = l; run (e, l, r);
        // zero-crossing periods of the echo
        std::vector<double> periods; double last = -1;
        for (int i = (int) fs; i < (int) l.size() - 1; ++i) if (l[(size_t) i] <= 0 && l[(size_t) i + 1] > 0) { const double t = i + l[(size_t) i] / (l[(size_t) i] - l[(size_t) i + 1]); if (last > 0) periods.push_back (t - last); last = t; }
        double lo = 1e9, hi = 0; for (auto pp : periods) { lo = std::min (lo, pp); hi = std::max (hi, pp); }
        const double cents = 1200 * std::log2 (hi / lo);
        check (cents > 3 && cents < 60, "Tape character: the echo of a held 1 kHz tone wobbles over " + juce::String (cents, 1) + " cents (digital: 0)");
    }
    // 8. Ducking
    {
        auto p = plain(); p.timeLMs = p.timeRMs = 150; p.feedback = 0.6; p.duckDb = 12; p.duckReleaseMs = 100;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) (fs * 3)); for (size_t i = 0; i < l.size(); ++i) l[i] = i < (size_t) (1.5 * fs) ? (float) (0.3 * std::sin (2 * pi * 300 * i / fs)) : 0.0f; auto r = l;
        auto noDuck = p; noDuck.duckDb = 0; Engine e2; e2.p = noDuck; e2.prepare (fs, 256); auto l2 = l, r2 = l;
        run (e, l, r); run (e2, l2, r2);
        auto rm = [&] (const std::vector<float>& v, double a, double b) { double s = 0; for (int i = (int) (a * fs); i < (int) (b * fs); ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / ((b - a) * fs)); };
        const double during = db (rm (l, 1.0, 1.4) / rm (l2, 1.0, 1.4)), after = db (rm (l, 1.9, 2.1) / rm (l2, 1.9, 2.1));
        check (during < -11 && after > -1, "Ducking 12 dB: echoes " + juce::String (during, 1) + " dB under the playing, " + juce::String (after, 2) + " dB once it stops");
    }
    // 9. Mix 0 is the dry signal
    {
        auto p = plain(); p.mix = 0; Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) fs); juce::Random rnd (4); for (auto& v : l) v = (float) (rnd.nextDouble() - 0.5); auto r = l; const auto in = l; run (e, l, r);
        bool same = true; for (size_t i = 0; i < l.size(); ++i) if (l[i] != in[i]) same = false;
        check (same, "Mix 0 % = dry, bit-exact");
    }
    // 10. CPU
    {
        auto p = plain(); p.character = diffuse; p.routing = cross; p.drive = 0.4; p.wow = 0.5; p.flutter = 0.5; p.duckDb = 6; p.lowCutHz = 100; p.highCutHz = 6000;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l ((size_t) fs * 20); juce::Random rnd (4); for (auto& v : l) v = (float) (rnd.nextDouble() - 0.5); auto r = l;
        const auto t0 = std::chrono::steady_clock::now(); run (e, l, r);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (sec / 20 < 0.02, "CPU, everything on: " + juce::String (sec / 20 * 100, 2) + " % of one core");
    }
    std::printf ("== %d passed, %d failed ==\n", passed, failed);
    return failed ? 1 : 0;
}
