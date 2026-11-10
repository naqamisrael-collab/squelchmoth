// Measures Vox Moth, Talk Wing and Clean Room (Source/SqVoice111gDsp.h). Built by scripts/build_voice111e_test.sh tests/voice111g_test.cpp voice111g_test.
#include "JuceHeader.h"
#include "SqVoice111gDsp.h"
#include <cstdio>
#include <chrono>
using namespace Eng;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what, double v) { std::printf ("%s  %-104s %10.3f\n", ok ? "PASS" : "FAIL", what, v); (ok ? passed : failed)++; }
static const double SR = 48000, twoPi = 6.283185307179586;
static double powerAt (const std::vector<float>& v, size_t a, size_t b, double hz)
{ const double w = twoPi * hz / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = a; i < b; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; } return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a) / (double) (b - a); }
static double db (double p) { return 10 * std::log10 (p + 1e-30); }
static double rms (const std::vector<float>& v, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; ++i) e += (double) v[i] * v[i]; return std::sqrt (e / (double) (b - a)); }
/** A vowel: glottal-ish harmonics of f0 shaped by two formants (Hz). */
static std::vector<float> vowel (size_t n, double f0, double F1, double F2, float amp = 0.1f)
{
    std::vector<float> v (n); double ph = 0;
    for (size_t i = 0; i < n; ++i)
    {
        ph += f0 / SR; double y = 0;
        for (int h = 1; h * f0 < 9000; ++h)
        {
            const double f = h * f0;
            const double g = 1.0 / (1.0 + std::pow ((f - F1) / 90.0, 2)) + 0.6 / (1.0 + std::pow ((f - F2) / 140.0, 2)) + 0.02;
            y += g * std::sin (twoPi * h * ph) / std::sqrt ((double) h);
        }
        v[i] = amp * (float) y;
    }
    return v;
}
static double band (const std::vector<float>& v, size_t a, size_t b, double lo, double hi) { double e = 0; for (double f = lo; f < hi; f += 20) e += powerAt (v, a, b, f); return e; }
template <typename P> static void run (P& p, std::vector<float>& l, std::vector<float>& r, const std::vector<float>* kl = nullptr, int blk = 256)
{
    for (size_t i = 0; i + (size_t) blk <= l.size(); i += (size_t) blk)
    {
        float* ch[2] = { l.data() + i, r.data() + i };
        if (kl != nullptr) { const float* key[2] = { kl->data() + i, kl->data() + i }; p.process (ch, 2, key, blk); }
        else p.process (ch, 2, nullptr, blk);
    }
}

int main()
{
    // ---------------------------------------------------------------- Vox Moth
    {
        auto best = [] (const std::vector<float>& v, double lo, double hi) { double bp = 0, bf = 0; for (double f = lo; f < hi; f *= 1.002) { const double pw = powerAt (v, 60000, 140000, f); if (pw > bp) { bp = pw; bf = f; } } return bf; };
        {
            VoxMoth::Processor p; p.p.carrier = VoxMoth::midiOrVoice; p.prepare (SR, 256); std::array<bool, 128> h {}; h[60] = true; p.setHeld (h);
            auto l = vowel (144000, 220, 700, 1200), r = l; run (p, l, r);
            const double f = best (l, 150, 400);
            check (std::abs (1200 * std::log2 (f / 261.63)) < 25, "Vox Moth: a held C4 plays the voice's words on C4 (Hz, expect 261.6)", f);
        }
        {
            VoxMoth::Processor p; p.p.carrier = VoxMoth::voiceTrack; p.p.tune = 1; p.p.retuneMs = 0; p.prepare (SR, 256);
            auto l = vowel (144000, 226, 700, 1200), r = l; run (p, l, r);
            const double f = best (l, 150, 400);
            check (std::abs (1200 * std::log2 (f / 220.0)) < 20, "Vox Moth: the built-in tuner pulls a sharp A3 (226 Hz) onto A in C major (Hz, expect 220)", f);
        }
        auto vowelDiff = [] (int bands, float mouthMix)
        {
            auto run1 = [&] (double F1, double F2) { VoxMoth::Processor p; p.p.carrier = VoxMoth::voiceTrack; p.p.bands = bands; p.p.mix = mouthMix; p.prepare (SR, 256); auto l = vowel (144000, 196, F1, F2), r = l; run (p, l, r); return db (band (l, 60000, 140000, 1900, 2700)) - db (band (l, 60000, 140000, 500, 900)); };
            return run1 (300, 2300) - run1 (750, 1150);
        };
        const double d16 = vowelDiff (16, 1);
        check (d16 > 10, "Vox Moth: the vowels come through: 'ee' vs 'ah' at 2-2.7 kHz relative to 0.5-0.9 kHz, 16 bands (dB)", d16);
        {
            // the whitened carrier: a saw (falls 6 dB / oct) and a narrow pulse give the same band balance on the same voice
            auto tilt = [] (int wave, float pw) { VoxMoth::Processor p; p.p.carrier = VoxMoth::voiceTrack; p.p.wave1 = wave; p.p.pw = pw; p.p.osc2Db = -60; p.p.bands = 20; p.prepare (SR, 256); auto l = vowel (144000, 196, 700, 1200), r = l; run (p, l, r); return db (band (l, 60000, 140000, 2500, 4000)) - db (band (l, 60000, 140000, 300, 800)); };
            const double a = tilt (0, 0.5f), b = tilt (1, 0.12f);
            check (std::abs (a - b) < 4, "Vox Moth: carrier whitening: saw and narrow pulse give the same high / low balance (dB apart)", std::abs (a - b));
        }
        {
            // Slide: +12 st on the synthesis bank moves the formant region up an octave
            auto cen = [] (float slide) { VoxMoth::Processor p; p.p.carrier = VoxMoth::voiceTrack; p.p.slideSt = slide; p.p.bands = 24; p.prepare (SR, 256); auto l = vowel (144000, 150, 600, 1000), r = l; run (p, l, r);
                                          double num = 0, den = 0; for (double f = 150; f < 6000; f += 50) { const double pw = powerAt (l, 60000, 140000, f); num += f * pw; den += pw; } return num / den; };
            const double ratio = cen (12) / cen (0);
            check (ratio > 1.45, "Vox Moth: Slide +12 st raises the spectral centre (ratio)", ratio);
        }
        {
            // unvoiced pass-through: noise in, sibilance out
            auto hiss = [] (float uv) { VoxMoth::Processor p; p.p.carrier = VoxMoth::voiceTrack; p.p.unvoiced = uv; p.p.noiseCarrier = 0; p.prepare (SR, 256);
                                        std::vector<float> l (96000); juce::Random rnd (5); for (auto& x : l) x = 0.05f * (rnd.nextFloat() * 2 - 1); auto r = l; run (p, l, r); return db (band (l, 40000, 90000, 5000, 9000)); };
            const double d = hiss (1) - hiss (0);
            check (d > 10, "Vox Moth: Unvoiced lets an 'S' through (dB more 5-9 kHz)", d);
        }
        {
            VoxMoth::Processor p; p.p.carrier = VoxMoth::midiOrVoice; p.p.bands = 64; p.p.chorus = 0.5f; p.p.ring = 0.2f; p.p.drive = 0.4f; p.p.driveVoicing = 2; p.prepare (SR, 128);
            std::array<bool, 128> h {}; for (int k : { 48, 55, 60, 64, 67, 71, 74, 79 }) h[(size_t) k] = true; p.setHeld (h);
            auto l = vowel (480000, 196, 700, 1200), r = l;
            const auto t0 = std::chrono::high_resolution_clock::now(); run (p, l, r, nullptr, 128);
            const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            check (sec / 10.0 * 100 < 15, "Vox Moth: CPU, 64 bands, 8-voice chord, drive + chorus + ring (% of one core)", sec / 10.0 * 100);
        }
    }
    // ---------------------------------------------------------------- Talk Wing
    {
        auto saw = [] (size_t n, double hz) { std::vector<float> v (n); double ph = 0; for (size_t i = 0; i < n; ++i) { ph += hz / SR; ph -= std::floor (ph); v[i] = 0.2f * (float) (2 * ph - 1); } return v; };
        auto talk = [&] (double F1, double F2, float mouth, float formant = 0)
        {
            TalkWing::Processor p; p.p.mouth = mouth; p.p.formantSt = formant; p.p.tube = 0; p.p.sibilance = 0; p.prepare (SR, 256);
            auto l = saw (144000, 110), r = l; auto key = vowel (144000, 196, F1, F2); run (p, l, r, &key);
            return l;
        };
        auto ratio = [] (const std::vector<float>& v) { return db (band (v, 60000, 140000, 1900, 2700)) - db (band (v, 60000, 140000, 500, 900)); };
        const double d = ratio (talk (300, 2300, 1)) - ratio (talk (750, 1150, 1));
        check (d > 10, "Talk Wing: the guitar says the vowel: 'ee' vs 'ah' at 2-2.7 kHz vs 0.5-0.9 kHz (dB)", d);
        const double d0 = ratio (talk (300, 2300, 0)) - ratio (talk (750, 1150, 0));
        check (std::abs (d0) < 1.5, "Talk Wing: Mouth 0 leaves the carrier's own tone (dB)", d0);
        {
            auto cen = [] (const std::vector<float>& l) { double num = 0, den = 0; for (double f = 110; f < 6000; f += 55) { const double pw = powerAt (l, 60000, 140000, f); num += f * pw; den += pw; } return num / den; };
            const double r5 = cen (talk (700, 1200, 1, 5)) / cen (talk (700, 1200, 1, 0));
            check (r5 > 1.12, "Talk Wing: Formant +5 st raises the mouth (spectral centre ratio)", r5);
        }
        {
            // amplitude transfer: the voice stops, the instrument stops
            TalkWing::Processor p; p.p.tube = 0; p.prepare (SR, 256);
            auto l = saw (192000, 110), r = l; auto key = vowel (192000, 196, 700, 1200); for (size_t i = 96000; i < key.size(); ++i) key[i] = 0;
            run (p, l, r, &key);
            const double on = rms (l, 40000, 90000), off = rms (l, 120000, 190000);
            check (20 * std::log10 (off / on + 1e-12) < -50, "Talk Wing: when the voice stops, the instrument stops (dB)", 20 * std::log10 (off / on + 1e-12));
        }
        {
            // no key: the voice drives the built-in buzz at its own pitch
            TalkWing::Processor p; p.p.source = TalkWing::voiceOnKey; p.p.buzzOctave = 0; p.prepare (SR, 256);
            auto l = vowel (144000, 220, 700, 1200), r = l; run (p, l, r);
            double bp = 0, bf = 0; for (double f = 150; f < 400; f *= 1.002) { const double pw = powerAt (l, 60000, 140000, f); if (pw > bp) { bp = pw; bf = f; } }
            check (std::abs (1200 * std::log2 (bf / 220.0)) < 25 && p.mode() == TalkWing::buzz, "Talk Wing: with nothing on the key input the voice drives the buzz at its own pitch (Hz)", bf);
        }
        {
            TalkWing::Processor p; p.prepare (SR, 128);
            auto l = saw (480000, 110), r = l; auto key = vowel (480000, 196, 700, 1200);
            const auto t0 = std::chrono::high_resolution_clock::now(); run (p, l, r, &key, 128);
            const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            check (sec / 10.0 * 100 < 12, "Talk Wing: CPU, Studio engine, stereo carrier (% of one core)", sec / 10.0 * 100);
        }
    }
    // ---------------------------------------------------------------- Clean Room
    {
        auto setAll = [] (CleanRoom::Params& q, float v) { q.hum = q.buzz = q.hiss = q.noise = q.room = q.clicks = v; };
        {
            // transparent at Clean 0: the input comes out delayed by exactly the reported latency
            CleanRoom::Processor p; p.p.clean = 0; p.prepare (SR, 256);
            std::vector<float> l (48000, 0.0f); l[10000] = 1; auto r = l; run (p, l, r);
            size_t pk = 0; float m = 0; for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > m) { m = std::abs (l[i]); pk = i; }
            check ((int) pk - 10000 == p.latencySamples() && std::abs (m - 1) < 0.01, "Clean Room: Clean 0 passes the sound untouched, latency as reported (samples)", (double) pk - 10000);
        }
        {
            // hum: a voice (233 Hz, away from the mains harmonics) with 60 Hz hum and its harmonics at -26 dB
            auto make = [] (size_t n, double mainsHz) { auto v = vowel (n, 233, 700, 1200); for (size_t i = 0; i < n; ++i) { double h = 0; for (int k = 1; k <= 6; ++k) h += std::sin (twoPi * mainsHz * k * (double) i / SR) / k; v[i] += 0.05f * (float) h; } return v; };
            CleanRoom::Processor p; p.p.clean = 0.5f; setAll (p.p, 0); p.p.hum = 1; p.p.buzz = 1; p.prepare (SR, 256);
            auto in = make (192000, 60), l = in, r = in; run (p, l, r);
            const size_t L = (size_t) p.latencySamples();
            double hum = 0; for (int k = 1; k <= 3; ++k) hum += db (powerAt (l, 100000 + L, 180000 + L, 60.0 * k)) - db (powerAt (in, 100000, 180000, 60.0 * k));
            double voice = 0; for (int k = 1; k <= 4; ++k) voice += std::abs (db (powerAt (l, 100000 + L, 180000 + L, 233.0 * k)) - db (powerAt (in, 100000, 180000, 233.0 * k)));
            std::printf ("  voice harmonics moved %.2f dB avg\n", voice / 4);
            check (hum / 3 < -25 && voice / 4 < 1.0, "Clean Room: 60 Hz hum and harmonics down (dB avg), the voice's harmonics within 1 dB", hum / 3);
            CleanRoom::Processor q; q.p.clean = 0.5f; setAll (q.p, 0); q.p.hum = 1; q.prepare (SR, 256);
            auto in5 = make (96000, 50), l5 = in5, r5 = in5; run (q, l5, r5);
            check (q.mainsHz() == 50, "Clean Room: Auto finds 50 Hz mains (Hz)", q.mainsHz());
        }
        {
            // broadband noise: a voice that stops and starts, under white noise 15 dB down
            const size_t n = 288000; auto v = vowel (n, 196, 700, 1200); for (size_t i = 0; i < n; ++i) if ((i / 24000) % 2 == 1) v[i] = 0;
            std::vector<float> nz (n); juce::Random rnd (9); for (auto& x : nz) x = 0.012f * (rnd.nextFloat() * 2 - 1);
            std::vector<float> in (n); for (size_t i = 0; i < n; ++i) in[i] = v[i] + nz[i];
            CleanRoom::Processor p; p.p.clean = 0.7f; setAll (p.p, 0); p.p.noise = 1; p.p.hiss = 0.5f; p.prepare (SR, 256);
            auto l = in, r = in; run (p, l, r); const size_t L = (size_t) p.latencySamples();
            const double gapIn = rms (in, 216000 + 2000, 240000 - 2000), gapOut = rms (l, 216000 + 2000 + L, 240000 - 2000 + L);
            check (20 * std::log10 (gapOut / gapIn) < -12, "Clean Room: noise between phrases down (dB)", 20 * std::log10 (gapOut / gapIn));
            double vh = 0; for (int k = 1; k <= 4; ++k) vh += db (powerAt (l, 194000 + L, 214000 + L, 196.0 * k)) - db (powerAt (in, 194000, 214000, 196.0 * k));
            check (std::abs (vh / 4) < 1.5, "Clean Room: the voice's harmonics stay (dB avg change)", vh / 4);
        }
        {
            // clicks: 20 sharp clicks on a sung tone are found and repaired
            const size_t n = 192000; auto in = vowel (n, 196, 700, 1200); for (int k = 0; k < 20; ++k) { const size_t at = 20000 + (size_t) k * 8000; in[at] += 0.6f; in[at + 1] -= 0.4f; }
            CleanRoom::Processor p; p.p.clean = 0.5f; setAll (p.p, 0); p.p.clicks = 1; p.prepare (SR, 256);
            auto l = in, r = in; run (p, l, r);
            check (p.clicksRepaired() >= 18 && p.clicksRepaired() <= 24, "Clean Room: clicks found and repaired (of 20)", p.clicksRepaired());
            CleanRoom::Processor q; q.p.clean = 0.5f; setAll (q.p, 0); q.p.clicks = 1; q.prepare (SR, 256);
            auto cl = vowel (n, 196, 700, 1200), cr = cl; run (q, cl, cr);
            check (q.clicksRepaired() == 0, "Clean Room: a clean sung tone gets no false repairs", q.clicksRepaired());
        }
        {
            // room: short bursts in a 1.2 s room; the tail between them comes down
            const size_t n = 240000; std::vector<float> dryv (n, 0.0f); auto vv = vowel (n, 196, 700, 1200);
            for (size_t i = 0; i < n; ++i) if ((i % 48000) < 6000) dryv[i] = vv[i];
            std::vector<float> in (n, 0.0f); juce::Random rnd (3); const double dec = std::log (1000.0) / (1.2 * SR);
            std::vector<float> ir (57600); for (size_t i = 0; i < ir.size(); ++i) ir[i] = (i == 0 ? 1.0f : 0.0f) + 0.012f * (rnd.nextFloat() * 2 - 1) * (float) std::exp (-dec * (double) i) * (i > 480 ? 1.0f : 0.0f);
            for (size_t i = 0; i < n; ++i) if (dryv[i] != 0) for (size_t k = 0; k < ir.size() && i + k < n; k += 1) in[i + k] += dryv[i] * ir[k];
            CleanRoom::Processor p; p.p.clean = 0.7f; setAll (p.p, 0); p.p.room = 1; p.p.roomDecay = 1.2f; p.prepare (SR, 256);
            auto l = in, r = in; run (p, l, r); const size_t L = (size_t) p.latencySamples();
            const double tIn = rms (in, 144000 + 9000, 144000 + 30000), tOut = rms (l, 144000 + 9000 + L, 144000 + 30000 + L);
            check (20 * std::log10 (tOut / tIn) < -6, "Clean Room: the room's tail between phrases down (dB)", 20 * std::log10 (tOut / tIn));
        }
        {
            CleanRoom::Processor p; p.p.clean = 1; p.prepare (SR, 128);
            auto l = vowel (480000, 196, 700, 1200), r = l;
            const auto t0 = std::chrono::high_resolution_clock::now(); run (p, l, r, nullptr, 128);
            const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            check (sec / 10.0 * 100 < 12, "Clean Room: CPU, Studio engine, every class on (% of one core)", sec / 10.0 * 100);
        }
    }
    std::printf ("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
