// Duet Filter / Speck Filter DSP core: measured. Build: scripts/build_duet_test.sh
#include "SqDuetDsp.h"
#include <cstdio>
#include <chrono>
#include <functional>

using namespace SqDuet;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }
static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }
static const double fs = 48000;

static Params base()
{
    Params p; p.slot[0] = { true, SqFissure::lowPass, 24, 1000, 0, 0, 0, 0, 1 }; p.slot[1] = { false, SqFissure::highPass, 24, 1000, 0, 0, 0, 0, 1 };
    return p;
}
/** Steady sines in L and R through the engine; returns {gain L, gain R} in dB. */
static std::pair<double, double> gains (const Params& p, double fL, double fR)
{
    Engine e; e.p = p; e.prepare (fs, 256);
    const int N = (int) (0.6 * fs); std::vector<float> l ((size_t) N), r ((size_t) N);
    for (int i = 0; i < N; ++i) { l[(size_t) i] = (float) (0.25 * std::sin (2 * pi * fL * i / fs)); r[(size_t) i] = (float) (0.25 * std::sin (2 * pi * fR * i / fs)); }
    const auto il = l, ir = r;
    for (int s = 0; s < N; s += 256) { float* ch[2] = { l.data() + s, r.data() + s }; e.process (ch, 2, std::min (256, N - s)); }
    const int a = (int) (0.4 * fs);
    return { db (rms (l, a, N) / rms (il, a, N)), db (rms (r, a, N) / rms (ir, a, N)) };
}

int main()
{
    juce::ScopedNoDenormals nd;
    std::printf ("Duet / Speck DSP\n");

    // 1. One filter (Speck): -3 dB at the cutoff, and the face's curve matches the sound with resonance
    {
        auto p = base(); const double g = gains (p, 1000, 1000).first;
        p.slot[0].resonance = 0.5; Engine e; e.p = p; e.prepare (fs, 256);
        const double meas = gains (p, 1000, 1000).first, drawn = e.responseDb (1000);
        check (std::abs (g + 3.01) < 0.15, "Single low-pass 24 dB at 1 kHz: " + juce::String (g, 2) + " dB at the cutoff");
        check (std::abs (meas - drawn) < 0.3, "Resonance 50 %: measured " + juce::String (meas, 2) + " dB, the face draws " + juce::String (drawn, 2) + " dB");
    }
    // 2. Link: filter 2 rides one octave above filter 1
    {
        auto p = base(); p.slot[1].on = true; p.slot[1].type = SqFissure::lowPass; p.slot[1].cutoffHz = 9999; p.link = true; p.linkOct = 1.0; p.routing = stereoSplit;
        const auto g = gains (p, 1000, 2000);   // left through filter 1 at its cutoff, right through filter 2 at its (linked) cutoff
        Engine e; e.p = p; e.prepare (fs, 256);
        check (std::abs (g.first + 3.01) < 0.15 && std::abs (g.second + 3.01) < 0.15 && std::abs (e.baseCut (1) - 2000) < 0.01,
               "Link +1 oct, Stereo routing: left at 1 kHz " + juce::String (g.first, 2) + " dB, right at 2 kHz " + juce::String (g.second, 2) + " dB (filter 2 sits at " + juce::String (e.baseCut (1), 1) + " Hz)");
    }
    // 3. Routing: serial LP 1k + HP 1k is a band at 1 kHz (-6 dB); parallel is the average; mid/side filters only the side
    {
        auto p = base(); p.slot[1].on = true; p.slot[1].cutoffHz = 1000;
        p.routing = serial; const double ser = gains (p, 1000, 1000).first, serLo = gains (p, 100, 100).first;
        p.routing = parallel; const double par100 = gains (p, 100, 100).first, par10k = gains (p, 10000, 10000).first;
        auto ms = base(); ms.routing = midSide; ms.slot[0].on = false; ms.slot[1] = { true, SqFissure::highPass, 24, 2000, 0, 0, 0, 0, 1 };
        // a mono 200 Hz sine (all mid) passes untouched; the side is high-passed
        const auto mono = gains (ms, 200, 200);
        check (std::abs (ser + 6.02) < 0.2 && serLo < -30, "Serial LP 1k -> HP 1k: " + juce::String (ser, 2) + " dB at 1 kHz, " + juce::String (serLo, 1) + " dB at 100 Hz");
        check (std::abs (par100 + 6.02) < 0.3 && std::abs (par10k + 6.02) < 0.3, "Parallel LP 1k + HP 1k: " + juce::String (par100, 2) + " dB at 100 Hz, " + juce::String (par10k, 2) + " dB at 10 kHz (half each)");
        check (std::abs (mono.first) < 0.01 && std::abs (mono.second) < 0.01, "Mid / Side, high-pass 2 kHz on the side: a centred 200 Hz tone stays at " + juce::String (mono.first, 3) + " dB");
    }
    // 4. LFO: the cutoff swings by its octaves, and the stereo phase offset spreads left and right
    {
        auto p = base(); p.lfoHz = 2; p.lfoOct = { 2.0, 0.0 }; p.lfoPhaseDeg = 180;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l (256, 0.1f), r (256, 0.1f); float lo = 1e9f, hi = 0, maxSpread = 0;
        for (int b = 0; b < (int) fs / 256; ++b)
        {
            float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, 256);
            lo = std::min (lo, e.meterCutoff[0][0]); hi = std::max (hi, e.meterCutoff[0][0]);
            maxSpread = std::max (maxSpread, (float) std::abs (std::log2 (e.meterCutoff[0][0] / e.meterCutoff[0][1])));
        }
        check (std::abs (lo - 250) < 8 && std::abs (hi - 4000) < 120, "LFO +-2 oct around 1 kHz: cutoff swings " + juce::String (juce::roundToInt (lo)) + " .. " + juce::String (juce::roundToInt (hi)) + " Hz");
        check (std::abs (maxSpread - 4.0f) < 0.15f, "Stereo phase 180 deg: left and right up to " + juce::String (maxSpread, 2) + " octaves apart");
    }
    // 5. Envelope follower: loud input opens the filter by its octaves; a sidechain key drives it with a silent input
    {
        auto p = base(); p.slot[0].cutoffHz = 200; p.envOct = { 4.0, 0.0 }; p.envAttackMs = 1; p.envReleaseMs = 50;
        Engine e; e.p = p; e.prepare (fs, 256);
        std::vector<float> l (256), r (256); for (int i = 0; i < 256; ++i) l[(size_t) i] = r[(size_t) i] = (float) std::sin (2 * pi * 1000 * i / fs);   // 0 dBFS
        for (int b = 0; b < 40; ++b) { float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, 256); for (int i = 0; i < 256; ++i) l[(size_t) i] = r[(size_t) i] = (float) std::sin (2 * pi * 1000 * (b * 256 + i) / fs); }
        const float open = e.meterCutoff[0][0];
        auto k = base(); k.slot[0].cutoffHz = 200; k.envOct = { 4.0, 0.0 }; k.envAttackMs = 1; k.envFromKey = true;
        Engine ek; ek.p = k; ek.prepare (fs, 256);
        std::vector<float> sl (256, 0.0f), sr (256, 0.0f), key (256, 0.9f);
        for (int b = 0; b < 40; ++b) { float* ch[2] = { sl.data(), sr.data() }; const float* kk[1] = { key.data() }; ek.process (ch, 2, 256, kk, 1); }
        check (open > 2600 && open < 3300, "Envelope +4 oct from 200 Hz on a 0 dB tone: opens to " + juce::String (juce::roundToInt (open)) + " Hz");
        check (ek.meterCutoff[0][0] > 2600, "Sidechain key at -1 dB, silent input: the filter still opens to " + juce::String (juce::roundToInt (ek.meterCutoff[0][0])) + " Hz");
    }
    // 6. Stability: resonance 100 %, drive, fast random LFO over 6 octaves - no blow-up; CPU
    {
        auto p = base(); p.slot[0].resonance = 1.0; p.slot[0].driveDb = 24; p.slot[0].slope = 48; p.slot[1].on = true; p.slot[1].type = SqFissure::bandPass; p.slot[1].resonance = 1.0;
        p.lfoShape = SqFissure::randomSteps; p.lfoHz = 30; p.lfoOct = { 3.0, -3.0 }; p.lfoPhaseDeg = 90;
        Engine e; e.p = p; e.prepare (fs, 256); juce::Random rnd (5);
        std::vector<float> l (256), r (256); double peak = 0; const int blocks = (int) (10 * fs / 256);
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i) { l[(size_t) i] = rnd.nextFloat() * 2 - 1; r[(size_t) i] = rnd.nextFloat() * 2 - 1; }
            float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, 256);
            for (int i = 0; i < 256; ++i) peak = std::max (peak, (double) std::abs (l[(size_t) i]));
        }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (std::isfinite (peak) && peak < 40, "Worst case (full resonance, +24 dB drive, 30 Hz random LFO): peak " + juce::String (db (peak), 1) + " dBFS, stable");
        check (secs / 10.0 < 0.05, "CPU, both filters at 48 dB/oct in stereo: " + juce::String (secs / 10.0 * 100.0, 2) + " % of one core");
    }
    std::printf ("Duet / Speck: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
