// Measures Siren Verb, Mic Closet, Larynx and Exhale (Source/SqVoice111eDsp.h). Built by scripts/build_voice111e_test.sh.
#include "JuceHeader.h"
#include "SqVoice111eDsp.h"
#include <cstdio>
using namespace Eng;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what, double v) { std::printf ("%s  %-96s %10.3f\n", ok ? "PASS" : "FAIL", what, v); (ok ? passed : failed)++; }
static const double SR = 48000, twoPi = 6.283185307179586;
template <typename F> static void runBlocks (std::vector<float>& l, std::vector<float>& r, F f) { for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; f (ch); } }
static double rmsDb (const std::vector<float>& v, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; ++i) e += (double) v[i] * v[i]; return 10 * std::log10 (e / (double) (b - a) + 1e-30); }
static double goertzelDb (const std::vector<float>& v, size_t a, size_t b, double hz)
{ const double w = twoPi * hz / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = a; i < b; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } return 10 * std::log10 ((s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a) / (double) (b - a) + 1e-30); }
static std::vector<float> voice (size_t n, double hz, double noiseDb, unsigned seed = 7)
{
    std::vector<float> v (n); double ph = 0; std::mt19937 rng (seed); std::normal_distribution<float> nd (0, 1); const float ng = (float) std::pow (10.0, noiseDb / 20);
    for (size_t i = 0; i < n; ++i) { ph += hz / SR; double y = 0; for (int h = 1; h <= 12; ++h) y += std::sin (twoPi * h * ph) / h; v[i] = 0.08f * (float) y + ng * nd (rng); }
    return v;
}

int main()
{
    // ---------------------------------------------------------------- Siren Verb
    {
        auto rt60 = [] (int algo, float decay)
        {
            SirenVerb::Processor p; p.p.algo = algo; p.p.decayS = decay; p.p.mix = 1; p.p.early = 0; p.p.preDelayMs = 0; p.p.modDepth = 0; p.p.lowCutHz = 20; p.p.highCutHz = 20000; p.p.damping = 0;
            p.prepare (SR, 256);
            const size_t n = (size_t) (SR * (decay * 1.6 + 0.5)) / 256 * 256; std::vector<float> l (n, 0.0f), r; l[256] = 1; r = l;
            runBlocks (l, r, [&] (float** ch) { p.process (ch, 2, 256); });
            // Schroeder integration, T20 (-5 .. -25 dB) x 3
            std::vector<double> e (n); double acc = 0; for (size_t i = n; i-- > 0;) { acc += (double) l[i] * l[i] + (double) r[i] * r[i]; e[i] = acc; }
            const double e0 = e[300]; size_t i5 = 0, i25 = 0; for (size_t i = 300; i < n; ++i) { const double d = 10 * std::log10 (e[i] / e0 + 1e-30); if (! i5 && d <= -5) i5 = i; if (! i25 && d <= -25) { i25 = i; break; } }
            return i25 > i5 && i5 > 0 ? 3.0 * (double) (i25 - i5) / SR : 0.0;
        };
        const double hall = rt60 (SirenVerb::hall, 2.0f), room = rt60 (SirenVerb::room, 0.8f), chamber = rt60 (SirenVerb::chamber, 1.5f);
        check (std::abs (hall / 2.0 - 1) < 0.25, "Siren Verb: Hall set to 2.0 s decays in (RT60, s)", hall);
        check (std::abs (room / 0.8 - 1) < 0.3, "Siren Verb: Room set to 0.8 s decays in (RT60, s)", room);
        check (std::abs (chamber / 1.5 - 1) < 0.25, "Siren Verb: Chamber set to 1.5 s decays in (RT60, s)", chamber);
        int ok = 0;
        for (int a = 0; a < SirenVerb::numAlgos; ++a)
        {
            SirenVerb::Processor p; p.p.algo = a; p.p.decayS = 1.5f; p.p.mix = 0.5f; p.prepare (SR, 256);
            auto l = voice (96000, 220, -40), r = l;
            runBlocks (l, r, [&] (float** ch) { p.process (ch, 2, 256); });
            bool fin = true; for (auto v : l) fin = fin && std::isfinite (v);
            const double lv = rmsDb (l, 48000, 96000);
            if (fin && lv > -40 && lv < 0) ++ok;
        }
        check (ok == SirenVerb::numAlgos, "Siren Verb: all 7 algorithms run clean on a voice (count)", ok);
        // Auto-EQ: the singer's fundamental and 2nd harmonic come out of the tail
        auto tailAt = [&] (bool on)
        {
            SirenVerb::Processor p; p.p.algo = SirenVerb::hall; p.p.decayS = 2.5f; p.p.mix = 1; p.p.autoEq = on; p.p.autoEqDb = 12; p.prepare (SR, 256);
            auto l = voice (144000, 196, -50), r = l;
            runBlocks (l, r, [&] (float** ch) { p.process (ch, 2, 256); });
            return goertzelDb (l, 72000, 144000, 196) - goertzelDb (l, 72000, 144000, 196 * 3);
        };
        const double off = tailAt (false), on = tailAt (true);
        check (off - on > 6, "Siren Verb: Auto-EQ cuts the vocal's fundamental in the tail (dB vs 3rd harmonic)", off - on);
        // Reverse: the energy of a hit arrives late and swells in
        auto peakAt = [&] (bool rev)
        {
            SirenVerb::Processor p; p.p.algo = SirenVerb::room; p.p.decayS = 0.6f; p.p.mix = 1; p.p.reverse = rev; p.p.reverseMs = 300; p.p.early = 0; p.p.preDelayMs = 0; p.prepare (SR, 256);
            std::vector<float> l (96000, 0.0f), r; for (int i = 0; i < 2000; ++i) l[(size_t) (4800 + i)] = 0.3f * (float) std::sin (twoPi * 440 * i / SR) * (float) std::exp (-i / 400.0); r = l;
            runBlocks (l, r, [&] (float** ch) { p.process (ch, 2, 256); });
            size_t pk = 0; float m = 0; for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > m) { m = std::abs (l[i]); pk = i; }
            return (double) pk / SR * 1000.0;
        };
        const double fwd = peakAt (false), rev = peakAt (true);
        check (rev - fwd > 80, "Siren Verb: Reverse moves a hit's peak later (ms)", rev - fwd);
        // CPU at 48 kHz / 128 for the hall with shimmer + Auto-EQ on
        {
            SirenVerb::Processor p; p.p.algo = SirenVerb::shimmer; p.p.autoEq = true; p.p.delayMode = SirenVerb::delayPingPong; p.p.comp = 0.5f; p.prepare (SR, 128);
            auto l = voice (480000, 220, -40), r = l;
            const auto t0 = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i + 128 <= l.size(); i += 128) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 128); }
            const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            check (sec / 10.0 * 100 < 8, "Siren Verb: CPU, Shimmer + Auto-EQ + echo + comp (% of one core)", sec / 10.0 * 100);
        }
    }
    // ---------------------------------------------------------------- Mic Closet
    {
        MicCloset::Processor m; m.p.source = 0; m.p.target = 1; m.p.colour = 0; m.prepare (SR, 256);
        // the target archetype's curve, measured from the filters (presence peak where the archetype puts it)
        const auto& t = MicCloset::mics()[1];
        const double atPres = m.responseDb (t.presHz) - m.responseDb (1000);
        check (std::abs (atPres - (t.presDb - 0.0)) < 1.5, "Mic Closet: tube condenser presence peak over 1 kHz (dB)", atPres);
        // a mic converted to itself is flat (100 Hz - 8 kHz)
        // (when the source is the target, the shelves and peaks cancel exactly; only the target's top roll-off remains)
        {
            MicCloset::Processor q; q.p.source = 1; q.p.target = 1; q.p.colour = 0; q.prepare (SR, 256);
            double w = 0; for (double hz = 60; hz <= 12000; hz *= 1.15) w = std::max (w, std::abs (q.responseDb (hz)));
            check (w < 0.05, "Mic Closet: a mic converted to itself is flat 60 Hz - 12 kHz (worst dB)", w);
        }
        // proximity: 2 cm vs 40 cm at 100 Hz on a cardioid dynamic; nothing on an omni
        auto lowAt = [] (int mic, int pattern, float cm) { MicCloset::Processor q; q.p.source = 0; q.p.target = mic; q.p.pattern = pattern; q.p.distanceCm = cm; q.p.colour = 0; q.prepare (SR, 256); return q.responseDb (100) - q.responseDb (2000); };
        const double card = lowAt (11, 0, 2) - lowAt (11, 0, 40), omni = lowAt (11, 1, 2) - lowAt (11, 1, 40);
        check (card > 6 && std::abs (omni) < 0.5, "Mic Closet: proximity at 2 cm vs 40 cm, cardioid (dB; omni stays flat)", card);
        check (MicCloset::numMics() >= 44, "Mic Closet: microphone archetypes", MicCloset::numMics());
        // telephone handset: band-limited (3 kHz -> 6 kHz drop > 12 dB)
        MicCloset::Processor ph; ph.p.source = 0; ph.p.target = 30; ph.p.colour = 0; ph.prepare (SR, 256);
        check (ph.responseDb (2000) - ph.responseDb (6000) > 12, "Mic Closet: telephone handset rolls off above 3.4 kHz (dB 2k vs 6k)", ph.responseDb (2000) - ph.responseDb (6000));
        // CPU
        MicCloset::Processor q; q.p.target = 5; q.p.colour = 0.8f; q.p.lowCutHz = 80; q.prepare (SR, 128);
        auto l = voice (480000, 220, -40), r = l;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i + 128 <= l.size(); i += 128) { float* ch[2] = { l.data() + i, r.data() + i }; q.process (ch, 2, 128); }
        const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (sec / 10.0 * 100 < 1.5, "Mic Closet: CPU with colour (% of one core)", sec / 10.0 * 100);
    }
    // ---------------------------------------------------------------- Larynx
    {
        auto run = [] (Larynx::Params pr, std::vector<float> l)
        {
            Larynx::Processor p; p.p = pr; p.prepare (SR, 256); auto r = l;
            runBlocks (l, r, [&] (float** ch) { p.process (ch, 2, 256); });
            return l;
        };
        // a vowel: a 140 Hz pulse train through two resonances (700 Hz, 1200 Hz)
        std::vector<float> vowel (96000); { Biquad f1, f2; f1.set (Biquad::bandpass, SR, 700, 6); f2.set (Biquad::bandpass, SR, 1200, 8); double ph = 0;
            for (size_t i = 0; i < vowel.size(); ++i) { ph += 140 / SR; float e = 0; if (ph >= 1) { ph -= 1; e = 1; } vowel[i] = 0.6f * (f1.process (e) + 0.7f * f2.process (e)); } }
        Larynx::Params neutral; neutral.levelMatch = false;
        auto y = run (neutral, vowel);
        const int lat = Larynx::Channel::latency();
        double e = 0, d = 0; for (size_t i = 24000; i < 90000; ++i) { const double ref = vowel[i - (size_t) lat]; e += ref * ref; d += (y[i] - ref) * (y[i] - ref); }
        check (10 * std::log10 (d / e) < -20, "Larynx: neutral tract is transparent (residual dB)", 10 * std::log10 (d / e));
        auto centroid = [&] (const std::vector<float>& v)
        { double num = 0, den = 0; for (double hz = 300; hz < 3000; hz += 20) { const double p = std::pow (10.0, goertzelDb (v, 24000, 90000, hz) / 10); num += hz * p; den += p; } return num / den; };
        Larynx::Params shorter = neutral; shorter.length = 0.8f;
        const double c0 = centroid (y), c1 = centroid (run (shorter, vowel));
        check (c1 / c0 > 1.12, "Larynx: a 0.8x tract raises the formants (centroid ratio)", c1 / c0);
        Larynx::Params wild = neutral; for (int s = 0; s < 8; ++s) wild.shape[s] = s % 2 ? 1.0f : -1.0f; wild.glottal = 1; wild.levelMatch = true;
        auto yw = run (wild, vowel);
        float pi = 0, po = 0; bool fin = true; for (size_t i = 24000; i < 90000; ++i) { pi = std::max (pi, std::abs (vowel[i])); po = std::max (po, std::abs (yw[i])); fin = fin && std::isfinite (yw[i]); }
        check (fin && gainToDb (po / pi) < 9, "Larynx: extreme tract + hard glottis stays guarded (peak over input, dB)", gainToDb (po / pi));
        Larynx::Params br = neutral; br.breath = 0.8f; br.levelMatch = false;
        auto yb = run (br, vowel);
        const double between = goertzelDb (yb, 24000, 90000, 3010) - goertzelDb (y, 24000, 90000, 3010);
        check (between > 6, "Larynx: Breathiness fills between the harmonics near its band (dB)", between);
        Larynx::Processor p; p.prepare (SR, 128); auto l = voice (480000, 220, -40), r = l;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i + 128 <= l.size(); i += 128) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 128); }
        const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (sec / 10.0 * 100 < 6, "Larynx: CPU, stereo (% of one core)", sec / 10.0 * 100);
        check (lat == 1023, "Larynx: latency (samples, reported)", lat);
    }
    // ---------------------------------------------------------------- Exhale
    {
        auto run = [] (float breathDb, std::vector<float> l, int& lat)
        {
            Exhale::Processor p; p.p.breathDb = breathDb; p.prepare (SR, 256); lat = p.latencySamples(); auto r = l;
            runBlocks (l, r, [&] (float** ch) { p.process (ch, 2, 256); });
            return l;
        };
        auto v = voice (144000, 200, -32);
        int lat = 0;
        auto y0 = run (0, v, lat);
        double e = 0, d = 0; for (size_t i = 48000; i < 140000; ++i) { const double ref = v[i - (size_t) lat]; e += ref * ref; d += (y0[i] - ref) * (y0[i] - ref); }
        check (10 * std::log10 (d / e + 1e-30) < -60, "Exhale: Breath 0 dB is an exact pass (residual dB)", 10 * std::log10 (d / e + 1e-30));
        auto yOff = run (-60, v, lat), yUp = run (9, v, lat);
        // the noise between harmonics (at 1.5 x f0 multiples) and the harmonics themselves
        double nz0 = 0, nzOff = 0, nzUp = 0, h0 = 0, hOff = 0;
        for (int k = 3; k <= 9; ++k) { nz0 += goertzelDb (y0, 48000, 140000, 100.0 * (2 * k + 1)); nzOff += goertzelDb (yOff, 48000, 140000, 100.0 * (2 * k + 1)); nzUp += goertzelDb (yUp, 48000, 140000, 100.0 * (2 * k + 1)); }
        for (int k = 1; k <= 4; ++k) { h0 += goertzelDb (y0, 48000, 140000, 200.0 * k); hOff += goertzelDb (yOff, 48000, 140000, 200.0 * k); }
        nz0 /= 7; nzOff /= 7; nzUp /= 7; h0 /= 4; hOff /= 4;
        check (nz0 - nzOff > 10, "Exhale: Breath off takes the breath noise down (dB, between harmonics)", nz0 - nzOff);
        check (nzUp - nz0 > 6, "Exhale: Breath +9 dB brings it up (dB)", nzUp - nz0);
        check (std::abs (h0 - hOff) < 1.0, "Exhale: the harmonics are untouched (dB change)", h0 - hOff);
        check (lat > 0 && lat < 0.02 * SR, "Exhale: latency (samples)", lat);
    }
    std::printf ("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
