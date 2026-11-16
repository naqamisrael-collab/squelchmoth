// Measures Hive Choir, Double Wing 2 and Mutant (Source/SqVoice111fDsp.h). Built by scripts/build_voice111e_test.sh tests/voice111f_test.cpp voice111f_test.
#include "JuceHeader.h"
#include "SqVoice111fDsp.h"
#include <cstdio>
#include <chrono>
using namespace Eng;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what, double v) { std::printf ("%s  %-100s %10.3f\n", ok ? "PASS" : "FAIL", what, v); (ok ? passed : failed)++; }
static const double SR = 48000, twoPi = 6.283185307179586;
static std::vector<float> voice (size_t n, double hz, double vibCents = 0)
{
    std::vector<float> v (n); double ph = 0; juce::Random rnd (3);
    for (size_t i = 0; i < n; ++i)
    {
        const double f = hz * std::pow (2.0, vibCents / 1200.0 * std::sin (twoPi * 5.5 * (double) i / SR));
        ph += f / SR; double y = 0; for (int h = 1; h <= 12; ++h) y += std::sin (twoPi * h * ph) / h;
        v[i] = 0.08f * (float) y + 0.002f * (rnd.nextFloat() * 2 - 1);
    }
    return v;
}
static double powerAt (const std::vector<float>& v, size_t a, size_t b, double hz)
{ const double w = twoPi * hz / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = a; i < b; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a) / (double) (b - a); }
static double db (double p) { return 10 * std::log10 (p + 1e-30); }
template <typename P> static void run (P& p, std::vector<float>& l, std::vector<float>& r, int blk = 256)
{ for (size_t i = 0; i + (size_t) blk <= l.size(); i += (size_t) blk) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, blk); } }

int main()
{
    // ---------------------------------------------------------------- Hive Choir
    {
        auto harmony = [] (int mode, float interval, std::array<bool, 128> held = {})
        {
            HiveChoir::Processor p; p.p.engine = 1; p.p.voiceType = 1; p.p.leadDb = -60; p.p.v[0].mode = mode; p.p.v[0].interval = interval; p.p.v[0].levelDb = 0;
            p.p.v[0].unison = 1; p.p.humanCents = 0; p.p.humanMs = 0; p.prepare (SR, 256); p.setHeld (held);
            auto l = voice (144000, 220), r = l; run (p, l, r);
            return l;
        };
        auto best = [] (const std::vector<float>& v, double lo, double hi) { double bp = 0, bf = 0; for (double f = lo; f < hi; f *= 1.002) { const double pw = powerAt (v, 60000, 140000, f); if (pw > bp) { bp = pw; bf = f; } } return bf; };
        const double third = best (harmony (HiveChoir::interval, 4), 180, 340);
        check (std::abs (1200 * std::log2 (third / 277.18)) < 20, "Hive Choir: Interval +4 st turns a 220 Hz voice into (Hz, expect 277.2)", third);
        const double step = best (harmony (HiveChoir::scaleStep, 2), 180, 340);
        check (std::abs (1200 * std::log2 (step / 261.63)) < 25, "Hive Choir: Scale Step +2 in C major from A3 lands on C4 (Hz, expect 261.6)", step);
        std::array<bool, 128> held {}; held[64] = true;
        const double midi = best (harmony (HiveChoir::midiChord, 0, held), 180, 400);
        check (std::abs (1200 * std::log2 (midi / 329.63)) < 25, "Hive Choir: MIDI Chord voice sings the held E4 (Hz, expect 329.6)", midi);
        // the lead at its own level: off = no 220 Hz
        auto h = harmony (HiveChoir::interval, 7);
        check (db (powerAt (h, 60000, 140000, 220)) - db (powerAt (h, 60000, 140000, 329.6)) < -15, "Hive Choir: Lead off leaves only the harmony (dB of 220 Hz vs the fifth)", db (powerAt (h, 60000, 140000, 220)) - db (powerAt (h, 60000, 140000, 329.6)));
        // a choir is wide and decorrelated; one singer is centred and correlated
        auto corr = [] (int unison)
        {
            HiveChoir::Processor p; p.p.leadDb = -60; p.p.v[0].mode = HiveChoir::interval; p.p.v[0].interval = 0; p.p.v[0].unison = unison; p.p.v[0].spread = 1; p.p.v[0].levelDb = 0; p.prepare (SR, 256);
            auto l = voice (144000, 220), r = l; run (p, l, r);
            double lr = 0, ll = 0, rr = 0; for (size_t i = 60000; i < 140000; ++i) { lr += (double) l[i] * r[i]; ll += (double) l[i] * l[i]; rr += (double) r[i] * r[i]; }
            return lr / std::sqrt (ll * rr + 1e-30);
        };
        const double c1 = corr (1), c16 = corr (16);
        check (c1 > 0.98 && c16 < 0.8, "Hive Choir: 16 singers spread wide (L/R correlation; one singer stays near 1)", c16);
        // timbre variation: singers really differ in tone (HF balance moves with Timbre)
        auto hfRatio = [] (float timbre)
        {
            HiveChoir::Processor p; p.p.leadDb = -60; p.p.v[0].mode = HiveChoir::interval; p.p.v[0].unison = 2; p.p.v[0].pitchVar = 0; p.p.v[0].timbreVar = timbre; p.p.v[0].levelDb = 0; p.p.v[0].spread = 1; p.prepare (SR, 256);
            auto l = voice (144000, 220), r = l; run (p, l, r);
            double lh = 0, rh = 0; for (int k = 12; k <= 20; ++k) { lh += powerAt (l, 60000, 140000, 220.0 * k); rh += powerAt (r, 60000, 140000, 220.0 * k); }
            return std::abs (db (lh) - db (rh));
        };
        check (hfRatio (1.0f) > hfRatio (0.0f) + 0.5, "Hive Choir: Timbre variation gives the singers different tone (dB HF L/R difference)", hfRatio (1.0f));
        // CPU: 4 voices x 8 singers = 32 singers, Studio engine, 48 kHz / 128
        {
            HiveChoir::Processor p; p.p.engine = 1; for (int k = 0; k < 4; ++k) { p.p.v[k].mode = HiveChoir::interval; p.p.v[k].interval = (float) (k * 3 + 3); p.p.v[k].unison = 8; p.p.v[k].vibCents = 20; }
            p.prepare (SR, 128); auto l = voice (480000, 220), r = l;
            const auto t0 = std::chrono::high_resolution_clock::now(); run (p, l, r, 128);
            const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            check (sec / 10.0 * 100 < 8, "Hive Choir: CPU, 4 voices x 8 singers (32), Studio engine (% of one core)", sec / 10.0 * 100);
        }
    }
    // ---------------------------------------------------------------- Double Wing 2
    {
        auto make = [] (float leadDb, bool straightOnly, float vibCents, double inVib, float& vibAmt, double& corr)
        {
            DoubleWing2::Processor p; p.p.doubles = 2; p.p.leadDb = leadDb; p.p.doublesDb = 0; p.p.vibCents = vibCents; p.p.vibStraightOnly = straightOnly; p.prepare (SR, 256);
            auto l = voice (144000, 220, inVib), r = l; run (p, l, r); vibAmt = p.vibratoAmount();
            double lr = 0, ll = 0, rr = 0; for (size_t i = 60000; i < 140000; ++i) { lr += (double) l[i] * r[i]; ll += (double) l[i] * l[i]; rr += (double) r[i] * r[i]; }
            corr = lr / std::sqrt (ll * rr + 1e-30); return l;
        };
        float va = 0; double c = 0;
        make (-60, true, 30, 0, va, c);
        check (c < 0.7, "Double Wing 2: two doubles alone are a wide pair (L/R correlation)", c);
        check (va > 0.8, "Double Wing 2: a straight note gets the added vibrato (amount)", va);
        float vb = 0; make (-60, true, 30, 60, vb, c);
        check (vb < 0.3, "Double Wing 2: a note already sung with vibrato gets none added (amount)", vb);
        // timing: the doubles are late by the Timing variation (cross-correlation peak of a double vs the input)
        {
            DoubleWing2::Processor p; p.p.doubles = 1; p.p.leadDb = -60; p.p.timeVar = 30; p.p.pitchVar = 0; p.p.timbreVar = 0; p.p.spread = 0; p.prepare (SR, 256);
            std::vector<float> l (96000, 0.0f); for (size_t i = 10000; i < 10200; ++i) l[i] = (float) std::sin (twoPi * 1000.0 * (double) (i - 10000) / SR); auto r = l; auto in = l;
            run (p, l, r);
            size_t pk = 0; float m = 0; for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > m) { m = std::abs (l[i]); pk = i; }
            const double ms = ((double) pk - 10050.0) / SR * 1000.0;
            check (ms > 9 && ms < 32, "Double Wing 2: the double is late by the Timing range (ms)", ms);
        }
    }
    // ---------------------------------------------------------------- Mutant
    {
        auto best = [] (const std::vector<float>& v, double lo, double hi) { double bp = 0, bf = 0; for (double f = lo; f < hi; f *= 1.002) { const double pw = powerAt (v, 60000, 140000, f); if (pw > bp) { bp = pw; bf = f; } } return bf; };
        {
            Mutant::Processor p; p.p.pitchSt = 12; p.prepare (SR, 256); auto l = voice (144000, 220), r = l; run (p, l, r);
            const double f = best (l, 300, 600);
            check (std::abs (1200 * std::log2 (f / 440.0)) < 20, "Mutant: Pitch +12 st (Hz, expect 440)", f);
        }
        {
            // the ring modulator follows the singer: carrier at f0 x 1.5 puts energy between the harmonics (0.5 f0 multiples)
            Mutant::Processor p; p.p.ringMix = 1; p.p.ringRatio = 1.5f; p.prepare (SR, 256); auto l = voice (144000, 220), r = l; run (p, l, r);
            Mutant::Processor q; q.prepare (SR, 256); auto l0 = voice (144000, 220), r0 = l0; run (q, l0, r0);
            const double between = db (powerAt (l, 60000, 140000, 330)) - db (powerAt (l0, 60000, 140000, 330));
            check (between > 20, "Mutant: pitch-tracking ring mod (dB new energy at 1.5 f0)", between);
        }
        {
            Mutant::Processor p; p.p.alien = 1; p.p.alienDiv = 9; p.prepare (SR, 256);
            std::vector<float> seen; auto l = voice (192000, 220), r = l;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.setTransport (120, (double) i / SR * 2.0, true); p.process (ch, 2, 256); seen.push_back (p.alienPitchNow()); }
            float lo = 99, hi = -99; for (auto v : seen) { lo = std::min (lo, v); hi = std::max (hi, v); }
            check (hi - lo > 6, "Mutant: Alienization jumps the pitch on the eighth notes (st range)", hi - lo);
        }
        {
            Mutant::Processor p; p.p.stutter = 1; p.p.stutterDiv = 9; p.p.stutterRepeats = 4; p.prepare (SR, 256);
            auto l = voice (192000, 220), r = l; bool any = false;
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.setTransport (120, (double) i / SR * 2.0, true); p.process (ch, 2, 256); any = any || p.stuttering(); }
            check (any, "Mutant: grain stutter fires on the beat", any ? 1 : 0);
        }
    }
    std::printf ("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
