// Squelchmoth 1.11 Voice phase C measurements: Moth Key, Siren Comp, Siren EQ, Hush De-Ess 2.
// g++ -O2 -std=c++17 -I../Source voice111_test.cpp -o /tmp/v111 && /tmp/v111
#include "SqVoice111Dsp.h"
#include <chrono>
#include <cstdio>
#include <string>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const std::string& w) { (ok ? passes : fails)++; std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", w.c_str()); }
static std::string f2 (double v, int d = 2) { char b[64]; std::snprintf (b, sizeof b, "%.*f", d, v); return b; }
static std::string keyName (int r, bool m) { return std::string (Voice::noteName (r)) + (m ? " minor" : " major"); }

// ---- a chord progression played on a soft additive "piano" (5 harmonics, decaying), with bass and a simple melody
static void addNote (std::vector<float>& y, double sr, double t0, double dur, double midi, float amp)
{
    const double f = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
    const size_t a = (size_t) (t0 * sr), b = std::min (y.size(), (size_t) ((t0 + dur) * sr));
    for (size_t i = a; i < b; ++i)
    {
        const double t = (double) (i - a) / sr, env = std::exp (-t * 1.6) * std::min (1.0, t * 200.0) * std::min (1.0, (double) (b - i) / (0.01 * sr));
        double v = 0; for (int h = 1; h <= 5; ++h) v += std::sin (2 * pi * f * h * t) / (h * h * 0.7 + 0.3);
        y[i] += (float) (amp * env * v);
    }
}
// chords as semitone offsets from the key root (scale degrees), 2 s each
static void progression (std::vector<float>& y, double sr, double t0, double seconds, int root, bool minor)
{
    static const int majP[4][3] = { { 0, 4, 7 }, { 5, 9, 12 }, { 7, 11, 14 }, { 0, 4, 7 } };     // I IV V I
    static const int minP[4][3] = { { 0, 3, 7 }, { 5, 8, 12 }, { 7, 11, 14 }, { 0, 3, 7 } };     // i iv V i (harmonic minor V)
    const int (*P)[3] = minor ? minP : majP;
    static const int majMel[8] = { 0, 2, 4, 5, 7, 9, 11, 12 }, minMel[8] = { 0, 2, 3, 5, 7, 8, 11, 12 };
    int bar = 0;
    for (double t = t0; t < t0 + seconds - 0.1; t += 2.0, ++bar)
    {
        const auto& c = P[bar % 4];
        for (int k = 0; k < 3; ++k) addNote (y, sr, t, 1.9, 60 + root % 12 + c[k], 0.05f);
        addNote (y, sr, t, 1.9, 36 + root % 12 + c[0], 0.08f);
        for (int q = 0; q < 4; ++q) addNote (y, sr, t + q * 0.5, 0.45, 72 + root % 12 + (minor ? minMel : majMel)[(bar * 3 + q * 2) % 8], 0.035f);
    }
}
static void clicks (std::vector<float>& y, double sr, double bpm)
{
    const double beat = 60.0 / bpm; Eng::TestSignals::Rng rng;
    for (double t = 0.1; t < y.size() / sr - 0.3; t += beat / 2)
    {
        const bool down = std::fmod (t - 0.1 + 1e-6, beat) < beat * 0.25;
        for (size_t i = (size_t) (t * sr); i < y.size() && i < (size_t) ((t + 0.12) * sr); ++i)
        {
            const double u = (double) i / sr - t;
            y[i] += (float) ((down ? 0.5 * std::sin (2 * pi * (50 + 120 * std::exp (-u * 30)) * u) * std::exp (-u * 20) : 0.0) + 0.15 * rng.next() * std::exp (-u * 60));
        }
    }
}
static void runStereo (std::function<void (float* const*, int)> f, std::vector<float>& l, std::vector<float>& r, int blk = 256)
{
    for (size_t i = 0; i < l.size(); i += (size_t) blk) { const int n = (int) std::min ((size_t) blk, l.size() - i); float* ch[2] = { l.data() + i, r.data() + i }; f (ch, n); }
}
static double rmsDb (const std::vector<float>& x, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i]; return 10 * std::log10 (e / (double) (b - a) + 1e-30); }
static std::vector<float> sine (double sr, double sec, double hz, float db) { std::vector<float> v ((size_t) (sr * sec)); const float a = dbToGain (db); for (size_t i = 0; i < v.size(); ++i) v[i] = a * (float) std::sin (2 * pi * hz * i / sr); return v; }

int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    //======================================================================
    std::printf ("== Moth Key ==\n");
    {
        const double sr = 44100; Voice::KeyFinder kf;
        int right = 0, total = 0, relErr = 0; std::string wrong;
        for (int root = 0; root < 12; ++root)
            for (int m = 0; m < 2; ++m)
            {
                std::vector<float> y ((size_t) (sr * 24), 0.0f); progression (y, sr, 0, 24, root, m == 1);
                const auto r = kf.analyse (y.data(), y.size(), sr);
                ++total; if (r.root == root && r.minor == (m == 1)) ++right; else { wrong += " " + keyName (root, m) + "->" + keyName (r.root, r.minor); if ((r.root + (r.minor ? 3 : 9)) % 12 == root) ++relErr; }
            }
        check (right >= 23, "all 24 keys (I-IV-V-I / i-iv-V-i, 24 s each, 44.1 kHz): " + std::to_string (right) + "/24 right" + wrong);
        {
            std::vector<float> y ((size_t) (sr * 30), 0.0f); progression (y, sr, 0, 30, 9, true); clicks (y, sr, 100);
            const auto r = kf.analyse (y.data(), y.size(), sr);
            check (r.root == 9 && r.minor, "A minor under a 100 BPM drum loop: " + keyName (r.root, r.minor) + ", confidence " + f2 (r.confidence, 2));
            check (std::abs (r.bpm - 100) < 1.0, "tempo of that loop: " + f2 (r.bpm, 2) + " BPM (100)");
        }
        for (double bpm : { 72.0, 128.0, 140.0, 174.0 / 2 })
        {
            std::vector<float> y ((size_t) (sr * 30), 0.0f); clicks (y, sr, bpm);
            const auto r = kf.analyse (y.data(), y.size(), sr);
            check (std::abs (r.bpm - bpm) < 1.0, "tempo " + f2 (bpm, 0) + " BPM clicks: " + f2 (r.bpm, 2));
        }
        {   // key change: 36 s in C major, then 36 s in E major
            std::vector<float> y ((size_t) (sr * 72), 0.0f); progression (y, sr, 0, 36, 0, false); progression (y, sr, 36, 36, 4, false);
            const auto r = kf.analyse (y.data(), y.size(), sr);
            std::string s; for (auto& g : r.segments) s += " [" + f2 (g.start, 1) + "-" + f2 (g.end, 1) + " " + keyName (g.root, g.minor) + "]";
            const bool ok = r.segments.size() == 2 && r.segments[0].root == 0 && ! r.segments[0].minor && r.segments[1].root == 4 && ! r.segments[1].minor && std::abs (r.segments[1].start - 36) < 6;
            check (ok, "key change C -> E at 36 s found:" + s);
        }
        {   // a sung line (TestSignals vocal) plus a held A minor pad: key from voice and pad
            std::vector<float> l, r; Eng::TestSignals::vocal (l, r, sr, 20);
            const auto k = kf.analyse (l.data(), l.size(), sr);
            check (k.confidence >= 0 && k.alternatives.size() == 4, "generated vocal: " + keyName (k.root, k.minor) + " (" + f2 (k.confidence, 2) + "), 4 alternatives listed");
        }
        {   // speed
            std::vector<float> y ((size_t) (sr * 180), 0.0f); progression (y, sr, 0, 180, 2, false);
            const auto t0 = std::chrono::steady_clock::now(); const auto r = kf.analyse (y.data(), y.size(), sr);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            check (ms < 3000 && r.root == 2, "a 3-minute song analysed in " + f2 (ms, 0) + " ms (" + keyName (r.root, r.minor) + ")");
        }
    }
    //======================================================================
    std::printf ("== Siren Comp ==\n");
    {
        using namespace Voice::Comp; const double sr = 48000;
        {   // static curve, Modern, hard knee: -8 dBFS sine, threshold -20, 4:1 -> 9 dB GR
            Processor c; c.p.s1.model = modern; c.p.s1.thresholdDb = -20; c.p.s1.ratio = 4; c.p.s1.kneeDb = 0; c.p.s1.attackMs = 1; c.p.s1.releaseMs = 200; c.p.autoMakeup = false; c.prepare (sr, 256);
            auto l = sine (sr, 1.0, 1000, -8), r = l; runStereo ([&] (float* const* ch, int n) { c.process (ch, 2, nullptr, n); }, l, r);
            check (std::abs (c.gr1() - 9.0f) < 0.6f, "Modern 4:1, -8 dBFS into -20 threshold: GR " + f2 (c.gr1(), 2) + " dB (9)");
        }
        auto stepGr = [&] (int model, int fetIdx, float atkMs, float relMs, double tOn, double tOff, std::vector<float>& grTrace)
        {
            Processor c; c.p.s1.model = model; c.p.s1.fetIdx = fetIdx; c.p.s1.thresholdDb = -30; c.p.s1.ratio = 4; c.p.s1.attackMs = atkMs; c.p.s1.releaseMs = relMs; c.p.autoMakeup = false; c.prepare (sr, 32);
            const size_t n = (size_t) (sr * 4); std::vector<float> l (n), r (n);
            for (size_t i = 0; i < n; ++i) { const double t = i / sr; const float a = t >= tOn && t < tOff ? 0.5f : 0.005f; l[i] = r[i] = a * (float) std::sin (2 * pi * 1000 * t); }
            grTrace.clear();
            for (size_t i = 0; i < n; i += 32) { float* ch[2] = { l.data() + i, r.data() + i }; c.process (ch, 2, nullptr, 32); grTrace.push_back (c.gr1()); }
        };
        auto timeTo = [&] (const std::vector<float>& g, double from, float frac, bool rising) { const size_t a = (size_t) (from * sr / 32); float ref = rising ? 0 : g[a]; float pk = 0; for (size_t i = a; i < g.size() && i < a + (size_t) (sr / 32 * 0.2); ++i) pk = std::max (pk, g[i]); if (rising) ref = pk; for (size_t i = a; i < g.size(); ++i) if (rising ? g[i] >= frac * ref : g[i] <= frac * ref) return (double) (i - a) * 32 / sr * 1000; return 1e9; };
        {
            std::vector<float> g; stepGr (fet, 0, 0.1f, 300, 1.0, 2.0, g);
            const double t90 = timeTo (g, 1.0, 0.9f, true);
            check (t90 < 1.0, "FET attack 0.1 ms: 90 % of the gain reduction within " + f2 (std::max (t90, 32 / sr * 1000), 2) + " ms (one 32-sample block)");
            std::vector<float> ga; stepGr (fet, 4, 0.1f, 300, 1.0, 2.0, ga); std::vector<float> g4; stepGr (fet, 0, 0.1f, 300, 1.0, 2.0, g4);
            check (ga[(size_t) (1.5 * sr / 32)] > g4[(size_t) (1.5 * sr / 32)] + 3, "FET All-buttons reduces more than 4:1: " + f2 (ga[(size_t) (1.5 * sr / 32)], 1) + " vs " + f2 (g4[(size_t) (1.5 * sr / 32)], 1) + " dB");
        }
        {
            std::vector<float> g; stepGr (optoA, 0, 10, 100, 1.0, 2.0, g);
            const double t63 = timeTo (g, 1.0, 0.63f, true);
            check (t63 > 4 && t63 < 25, "Opto attack: 63 % in " + f2 (t63, 1) + " ms (cell ~10 ms)");
            const double half = timeTo (g, 2.0, 0.5f, false); const float at = g[(size_t) (2.0 * sr / 32) - 1], left = g[(size_t) (3.0 * sr / 32)];
            check (half < 150 && left > 0.1f * at, "Opto two-stage release: half of " + f2 (at, 1) + " dB back in " + f2 (half, 0) + " ms, " + f2 (left, 1) + " dB still held 1 s later");
        }
        {   // unity / mix 0
            Processor c; c.p.s1.model = modern; c.p.s1.thresholdDb = 0; c.p.autoMakeup = false; c.prepare (sr, 256);
            auto l = sine (sr, 0.5, 440, -20), r = l, ref = l; runStereo ([&] (float* const* ch, int n) { c.process (ch, 2, nullptr, n); }, l, r);
            double md = 0; for (size_t i = 0; i < l.size(); ++i) md = std::max (md, (double) std::abs (l[i] - ref[i]));
            check (md < 1e-5, "below threshold, no warm: output equals input (max diff " + f2 (md * 1e6, 2) + " x1e-6)");
            Processor d; d.p.s1.model = fet; d.p.s1.thresholdDb = -40; d.p.warm = 0.8f; d.p.mix = 0; d.p.autoMakeup = false; d.prepare (sr, 256);
            l = ref; r = l; runStereo ([&] (float* const* ch, int n) { d.process (ch, 2, nullptr, n); }, l, r);
            md = 0; for (size_t i = 0; i < l.size(); ++i) md = std::max (md, (double) std::abs (l[i] - ref[i]));
            check (md < 1e-6, "Mix 0 is the dry signal (warm on, max diff " + f2 (md * 1e6, 3) + " x1e-6)");
        }
        {   // Assist on the vocal: suggestions land near their GR targets and never clip
            std::vector<float> vl, vr; Eng::TestSignals::vocal (vl, vr, sr, 6);
            Processor c; c.prepare (sr, 256); c.learning = true;
            auto l = vl, r = vr; runStereo ([&] (float* const* ch, int n) { c.process (ch, 2, nullptr, n); }, l, r); c.learning = false;
            const char* nm[] = { "Minimal", "Controlled", "Aggressive" };
            float prev = -1;
            for (int a = 0; a < 3; ++a)
            {
                Processor s; s.p = c.suggest (a); s.prepare (sr, 256);
                l = vl; r = vr; double grSum = 0; int grN = 0; float pk = 0;
                for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; s.process (ch, 2, nullptr, 256); if (i > sr) { grSum += s.gr1() + s.gr2(); ++grN; } for (int k = 0; k < 256; ++k) pk = std::max (pk, std::abs (l[i + (size_t) k])); }
                const float avg = (float) (grSum / std::max (1, grN));
                check (avg > prev && gainToDb (pk) < 0.0f, std::string ("Assist ") + nm[a] + ": average GR " + f2 (avg, 1) + " dB (" + modelName (s.p.s1.model) + (s.p.dual ? std::string (" + ") + modelName (s.p.s2.model) : std::string()) + "), peak " + f2 (gainToDb (pk), 1) + " dBFS");
                prev = avg;
            }
        }
        {   // Punch ceiling: +12 dB into a -1 dBFS ceiling
            Processor c; c.p.s1.thresholdDb = 0; c.p.autoMakeup = false; c.p.punch = true; c.p.punchGainDb = 12; c.p.punchImpact = 0.5f; c.p.punchCeilingDb = -1; c.prepare (sr, 256);
            std::vector<float> l, r; Eng::TestSignals::drums (l, r, sr, 3);
            float held = 0; runStereo ([&] (float* const* ch, int n) { c.process (ch, 2, nullptr, n); held = std::max (held, c.punchGrDb()); }, l, r);
            float pk = 0; for (size_t i = (size_t) (0.1 * sr); i < l.size(); ++i) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i])));
            check (gainToDb (pk) <= -0.99f && held > 3, "Punch +12 dB into a -1 dBFS ceiling: peak " + f2 (gainToDb (pk), 2) + " dBFS, up to " + f2 (held, 1) + " dB held back");
        }
        {   // pitch-filter duck: a 220 Hz key ducks the beat's 220 Hz harmonics, not the rest
            Processor c; c.p.s1.thresholdDb = 0; c.p.autoMakeup = false; c.p.keyOn = true; c.p.duckDepthDb = 9; c.prepare (sr, 256); c.setKeyPitch (220);
            auto l = sine (sr, 1.0, 440, -12), r = l; auto o = sine (sr, 1.0, 550, -12), o2 = o;
            auto key = sine (sr, 1.0, 220, -10), key2 = key;
            for (size_t i = 0; i < l.size(); i += 256) { const int n = (int) std::min ((size_t) 256, l.size() - i); float* ch[2] = { l.data() + i, r.data() + i }; const float* k[2] = { key.data() + i, key2.data() + i }; c.process (ch, 2, k, n); }
            Processor d; d.p = c.p; d.prepare (sr, 256); d.setKeyPitch (220);
            for (size_t i = 0; i < o.size(); i += 256) { const int n = (int) std::min ((size_t) 256, o.size() - i); float* ch[2] = { o.data() + i, o2.data() + i }; const float* k[2] = { key.data() + i, key2.data() + i }; d.process (ch, 2, k, n); }
            const double hit = rmsDb (l, 24000, 48000) + 15.0, miss = rmsDb (o, 24000, 48000) + 15.0;
            check (hit < -5 && std::abs (miss) < 1.0, "pitch duck (key at 220 Hz): its 2nd harmonic 440 Hz " + f2 (hit, 1) + " dB, an unrelated 550 Hz " + f2 (miss, 2) + " dB");
        }
        {   // CPU, dual + warm + punch
            Processor c; c.p.dual = true; c.p.s1.model = fet; c.p.s2.model = optoA; c.p.warm = 0.5f; c.p.punch = true; c.prepare (48000, 128);
            std::vector<float> l, r; Eng::TestSignals::pink (l, r, 48000, 10, -14);
            const auto t0 = std::chrono::steady_clock::now(); runStereo ([&] (float* const* ch, int n) { c.process (ch, 2, nullptr, n); }, l, r, 128);
            const double pc = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 10.0 * 100;
            check (pc < 3.0, "CPU dual + warm + punch: " + f2 (pc, 2) + " % of a core");
        }
    }
    //======================================================================
    std::printf ("== Siren EQ ==\n");
    {
        using namespace Voice::Eq; const double sr = 48000;
        auto gainAt = [&] (Processor& e, double hz, float db = -24)
        {
            auto l = sine (sr, 1.0, hz, db), r = l, in = l; runStereo ([&] (float* const* ch, int n) { e.process (ch, 2, n); }, l, r);
            return rmsDb (l, 24000, 48000) - rmsDb (in, 24000, 48000);
        };
        {
            Processor e; e.prepare (sr, 256); const double g = gainAt (e, 1000);
            check (std::abs (g) < 0.01, "all bands off: flat (" + f2 (g, 3) + " dB at 1 kHz)");
        }
        {
            Processor e; e.p.band[0].on = true; e.p.band[0].freq = 1000; e.p.band[0].gainDb = 6; e.p.band[0].q = 1; e.prepare (sr, 256);
            const double g = gainAt (e, 1000), g2 = gainAt (e, 4000);
            check (std::abs (g - 6) < 0.2 && g2 < 1.5, "bell +6 dB at 1 kHz: " + f2 (g, 2) + " dB there, " + f2 (g2, 2) + " dB at 4 kHz");
        }
        for (int slope = 0; slope < 4; ++slope)
        {
            Processor e; e.p.hpHz = 100; e.p.hpSlope = slope; e.prepare (sr, 256);
            const double g = gainAt (e, 50), want = -10 * std::log10 (1 + std::pow (2.0, 2 * (slope + 1)));
            check (std::abs (g - want) < 1.0, "high-pass " + std::to_string (6 * (slope + 1)) + " dB/oct at 100 Hz: " + f2 (g, 1) + " dB at 50 Hz (Butterworth " + f2 (want, 1) + ")");
        }
        {   // pitch tracking: band on harmonic 2 follows the voice
            Processor e; Band& b = e.p.band[1]; b.on = true; b.mode = pitchTrack; b.harmonic = 2; b.gainDb = -4; e.prepare (sr, 256);
            std::vector<float> l (4800, 0.0f), r = l;
            e.setPitch (220); for (int k = 0; k < 20; ++k) { float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, 240); }
            const float a = e.bandFreq (1);
            e.setPitch (330); for (int k = 0; k < 20; ++k) { float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, 240); }
            const float c = e.bandFreq (1);
            check (std::abs (a - 440) < 2 && std::abs (c - 660) < 2, "pitch-tracking band on the 2nd harmonic: " + f2 (a, 1) + " Hz at 220 Hz, " + f2 (c, 1) + " Hz at 330 Hz");
        }
        {   // dynamic band: only cuts when the band is loud
            auto dyn = [&] (float db)
            {
                Processor e; Band& b = e.p.band[2]; b.on = true; b.mode = dynamic; b.freq = 3000; b.q = 2; b.thresholdDb = -30; b.rangeDb = 6; e.prepare (sr, 256);
                const double g = gainAt (e, 3000, db); return std::make_pair (g, e.bandDynGain (2));
            };
            const auto lo = dyn (-50), hi = dyn (-10);
            check (std::abs (lo.first) < 0.3 && hi.first < -5.0 && hi.first > -7.0, "dynamic band 3 kHz, threshold -30, range 6: " + f2 (lo.first, 2) + " dB at -50 dBFS, " + f2 (hi.first, 2) + " dB at -10 dBFS");
        }
        {   // Vocal Learning finds an injected resonance and the pitch range, and preview auditions it
            std::vector<float> l, r; Eng::TestSignals::pink (l, r, sr, 6, -20);
            Biquad res; res.set (Biquad::peak, sr, 2500, 8, 9); Biquad res2 = res; for (size_t i = 0; i < l.size(); ++i) { l[i] = res.process (l[i]); r[i] = res2.process (r[i]); }
            Processor e; e.prepare (sr, 256); e.learning = true;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { e.learnPitch (180.0f + 60.0f * (float) std::sin (i / sr)); float* ch[2] = { l.data() + i, r.data() + i }; e.process (ch, 2, 256); }
            const auto lr = e.learnResult();
            bool found = false; float fq = 0, gd = 0; for (auto& b : lr.bands) if (std::abs (std::log2 (b.freq / 2500.0)) < 0.1) { found = true; fq = b.freq; gd = b.gainDb; }
            check (lr.ok && found && gd < -2, "Learning: resonance at 2.5 kHz found (" + f2 (fq, 0) + " Hz, cut " + f2 (gd, 1) + " dB), pitch range " + f2 (lr.lowHz, 0) + "-" + f2 (lr.highHz, 0) + " Hz, HP " + f2 (lr.hpHz, 0) + " Hz");
            Processor pv; for (size_t i = 0; i < lr.bands.size(); ++i) pv.p.suggested[i] = lr.bands[i]; pv.p.suggestedHp = lr.hpHz; pv.p.preview = true; pv.prepare (sr, 256);
            const double g = gainAt (pv, fq), g0 = [&] { Processor q; q.prepare (sr, 256); return gainAt (q, fq); }();
            check (g < g0 - 2, "preview auditions the suggestion before it is applied: " + f2 (g, 1) + " dB at the resonance (bands untouched: " + f2 (g0, 1) + ")");
        }
        {   // Learning on a sung vocal: its harmonics are not resonances; an injected 3 kHz resonance is
            std::vector<float> l, r; Eng::TestSignals::vocal (l, r, sr, 6);
            Biquad res; res.set (Biquad::peak, sr, 3000, 6, 9); Biquad res2 = res; for (size_t i = 0; i < l.size(); ++i) { l[i] = res.process (l[i]); r[i] = res2.process (r[i]); }
            Processor e; e.prepare (sr, 256); e.learning = true;
            static const float notes[] = { 57, 59, 60, 62, 64, 62, 60, 59 };
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { e.learnPitch (440.0f * std::pow (2.0f, (notes[(int) (i / sr / 0.5) % 8] - 69) / 12.0f)); float* ch[2] = { l.data() + i, r.data() + i }; e.process (ch, 2, 256); }
            const auto lr = e.learnResult(); std::string list; bool found = false; int falsePos = 0;
            for (auto& b : lr.bands) { list += " " + f2 (b.freq, 0) + " Hz " + f2 (b.gainDb, 1) + " dB;"; if (std::abs (std::log2 (b.freq / 3000.0)) < 0.12) found = true; else if (b.freq < 1500) ++falsePos; }
            check (found && falsePos == 0, "Learning on a sung vocal finds the 3 kHz resonance and no harmonics below 1.5 kHz:" + list);
        }
        {
            Processor e; for (int b = 0; b < 6; ++b) { e.p.band[(size_t) b].on = true; e.p.band[(size_t) b].mode = b % 2 ? dynamic : pitchTrack; e.p.band[(size_t) b].harmonic = b + 1; } e.p.hpHz = 80; e.p.hpSlope = 3; e.p.airDb = 3; e.p.tiltDb = 2; e.prepare (48000, 128); e.setPitch (200);
            std::vector<float> l, r; Eng::TestSignals::pink (l, r, 48000, 10, -18);
            const auto t0 = std::chrono::steady_clock::now(); runStereo ([&] (float* const* ch, int n) { e.process (ch, 2, n); }, l, r, 128);
            const double pc = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 10.0 * 100;
            check (pc < 3.0, "CPU, 6 tracking / dynamic bands + filters: " + f2 (pc, 2) + " % of a core");
        }
    }
    //======================================================================
    std::printf ("== Hush De-Ess 2 ==\n");
    {
        using namespace Voice::DeEss; const double sr = 48000;
        auto hissy = [&] (double sec, double fadeMs, float db, double from, double len)
        {
            std::vector<float> v ((size_t) (sr * sec), 0.0f); Eng::TestSignals::Rng rng; Biquad h1, h2; h1.set (Biquad::highpass, sr, 6000, 0.7); h2 = h1;
            const float a = dbToGain (db) * 2.2f;
            for (size_t i = 0; i < v.size(); ++i)
            {
                const double t = i / sr; double env = 0;
                if (t >= from && t < from + len) env = std::min (1.0, (t - from) * 1000.0 / std::max (0.01, fadeMs)) * std::min (1.0, (from + len - t) / 0.003);
                v[i] = (float) (h2.process (h1.process (rng.next())) * a * env);
            }
            return v;
        };
        {   // a low tone alone passes untouched (the LR4 split sums flat)
            Processor d; d.prepare (sr, 256); auto l = sine (sr, 1.0, 300, -10), r = l, in = l;
            runStereo ([&] (float* const* ch, int n) { d.process (ch, 2, n); }, l, r);
            const double g = rmsDb (l, 12000, 48000) - rmsDb (in, 12000, 48000);
            check (std::abs (g) < 0.05 && d.gainReductionDb() < 0.01f, "a 300 Hz tone alone: " + f2 (g, 3) + " dB, no reduction");
        }
        {   // sustained "sss" over a 300 Hz tone: highs come down, the 300 Hz part does not move
            Processor d; d.p.softThresholdDb = -40; d.prepare (sr, 256);
            auto tone = sine (sr, 1.5, 300, -14); auto s = hissy (1.5, 30, -16, 0.2, 1.1);
            auto l = tone; for (size_t i = 0; i < l.size(); ++i) l[i] += s[i]; auto r = l;
            float grMax = 0; int sawSoft = 0;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; d.process (ch, 2, 256); grMax = std::max (grMax, d.gainReductionDb()); if (d.currentKind() == soft) ++sawSoft; }
            // the low band of the output vs the tone: low-pass both at 1 kHz
            Biquad a1, a2, b1, b2; a1.set (Biquad::lowpass, sr, 1000, 0.7); a2 = a1; b1 = a1; b2 = a1;
            std::vector<float> lo (l.size()), lt (l.size()); for (size_t i = 0; i < l.size(); ++i) { lo[i] = a2.process (a1.process (l[i])); lt[i] = b2.process (b1.process (tone[i])); }
            const double dl = rmsDb (lo, 24000, 60000) - rmsDb (lt, 24000, 60000);
            const double hiOut = rmsDb (l, 24000, 60000), hiIn = [&] { auto x = tone; for (size_t i = 0; i < x.size(); ++i) x[i] += s[i]; return rmsDb (x, 24000, 60000); }();
            check (grMax > 4 && sawSoft > 20, "sustained S: classified Soft, " + f2 (grMax, 1) + " dB reduction of the highs");
            check (std::abs (dl) < 0.2, "under that S the 300 Hz part moves " + f2 (dl, 3) + " dB (total level " + f2 (hiOut - hiIn, 1) + " dB)");
        }
        {   // a "t" burst after silence is Hard; a fricative faded in over 40 ms is Soft
            Processor d; d.prepare (sr, 256);
            auto l = hissy (0.6, 0.2, -14, 0.2, 0.025), r = l;
            runStereo ([&] (float* const* ch, int n) { d.process (ch, 2, n); }, l, r);
            Processor e; e.prepare (sr, 256);
            auto l2 = hissy (0.8, 40, -14, 0.2, 0.4), r2 = l2;
            runStereo ([&] (float* const* ch, int n) { e.process (ch, 2, n); }, l2, r2);
            check (d.countHard >= 1 && d.countSoft == 0 && e.countSoft >= 1 && e.countHard == 0,
                   "burst -> Hard (" + std::to_string (d.countHard) + " hard, " + std::to_string (d.countSoft) + " soft); faded fricative -> Soft (" + std::to_string (e.countSoft) + " soft, " + std::to_string (e.countHard) + " hard)");
        }
        {   // Solo plays only what is removed; the history records events
            Processor d; d.p.solo = true; d.p.softThresholdDb = -40; d.prepare (sr, 256);
            auto tone = sine (sr, 1.0, 300, -14); auto s = hissy (1.0, 30, -16, 0.2, 0.6); auto l = tone; for (size_t i = 0; i < l.size(); ++i) l[i] += s[i]; auto r = l;
            runStereo ([&] (float* const* ch, int n) { d.process (ch, 2, n); }, l, r);
            const double toneOnly = rmsDb (l, 2000, 8000), during = rmsDb (l, 24000, 36000);
            std::vector<Event> h (Processor::historyLen); d.historyCopy (h.data()); int ev = 0; for (auto& e : h) ev += e.kind != none;
            check (toneOnly < -80 && during > -40 && ev > 10, "Solo: " + f2 (toneOnly, 0) + " dB before the S, " + f2 (during, 1) + " dB during; " + std::to_string (ev) + " events in the history");
        }
        {   // Assist: thresholds set so the loudest sibilants get ~6 dB
            Processor d; d.prepare (sr, 256); d.learning = true; d.p.hardThresholdDb = d.p.softThresholdDb = 0;
            auto l = hissy (1.5, 30, -20, 0.2, 1.0), r = l; runStereo ([&] (float* const* ch, int n) { d.process (ch, 2, n); }, l, r);
            const auto s = d.suggest();
            Processor e; e.p = s; e.prepare (sr, 256); l = hissy (1.5, 30, -20, 0.2, 1.0); r = l; float g = 0; int cnt = 0;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; e.process (ch, 2, 256); if (i > 0.4 * sr && i < 1.1 * sr) { g += e.gainReductionDb(); ++cnt; } }
            check (s.softThresholdDb < -20 && g / cnt > 3 && g / cnt < 10, "Assist: soft threshold " + f2 (s.softThresholdDb, 1) + " dB, then " + f2 (g / cnt, 1) + " dB average reduction on that S");
        }
        {
            Processor d; d.prepare (48000, 128); std::vector<float> l, r; Eng::TestSignals::vocal (l, r, 48000, 10);
            const auto t0 = std::chrono::steady_clock::now(); runStereo ([&] (float* const* ch, int n) { d.process (ch, 2, n); }, l, r, 128);
            const double pc = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 10.0 * 100;
            check (pc < 1.5, "CPU: " + f2 (pc, 2) + " % of a core");
        }
    }
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
