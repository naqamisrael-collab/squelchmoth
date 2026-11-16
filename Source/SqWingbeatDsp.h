#pragma once
// Wingbeat Echo (super prompt 1.11, C2): the delay engine. Standard C++ only (measured in tests/wingbeat_test.cpp).
//
//   Modes: Single, Dual (independent L / R), Ping-Pong, Rhythm (up to 32 taps with level, pan, filter and pitch each)
//   36 styles: 30 generic archetypes (studio tape at 30 / 15 / 7.5 ips, tube and multi-head spring tape echoes, bucket
//   pedals, oil can, drum echo, chorus box, radios, telephone, digital racks ...) and 6 of our own (Lunar Tape, Ice
//   Diffuse, Granule, Dust Cassette, Reverse Bloom, Infinite Wash)
//   In the loop: style EQ, diffusion, wow and flutter (separate), 2x-oversampled anti-aliased saturation (six types),
//   three limiter settings, band-limiting / companding for the bucket styles
//   Plus: up to 30 s, note values, groove, feel, prime-number times, width, L/R offset, accent, warp (tape stretch
//   under loud signals), ducking with an optional external key, freeze, time-change behaviour (tape glide, crossfade
//   or jump) and a head selector for the multi-head styles.

#include "SqEngine111Dsp.h"

namespace Wingbeat
{
    using namespace Eng;

    enum Mode { single, dual, pingPong, rhythm, numModes };
    enum TimeChange { glideChange, crossfadeChange, jumpChange };
    enum SatType { satClean, satTape, satWarm, satPump, satDirt, satHardLimit, numSatTypes };
    enum ReadMode { readNormal, readReverse, readGranular };
    enum WowShape { wowSine, wowTriangle, wowRandom, wowDrift, wowSquare };
    enum LimitMode { limitOff, limitSoft, limitHard };

    inline const char* modeName (int m)  { static const char* n[] = { "Single", "Dual", "Ping-Pong", "Rhythm" }; return n[clamp (m, 0, 3)]; }
    inline const char* satName (int s)   { static const char* n[] = { "Clean", "Tape", "Warm", "Pump", "Dirt", "Hard Limit" }; return n[clamp (s, 0, 5)]; }

    struct Style
    {
        const char* name; const char* archetype;
        int sat; float satDrive;                  // saturation type and drive at Saturation 50 % (dB)
        float lowCut, highCut;                    // loop band (Hz)
        float wowMs, wowHz; int wowShape;         // wow depth (ms peak), rate, shape
        float flutterMs, flutterHz;               // flutter depth, rate
        float diffusion;                          // 0..1 in-loop allpass diffusion
        float noiseDb;                            // hiss floor in the loop (dBFS), -200 = none
        float bandHz;                             // bucket / digital band-limit (Hz, 0 = none) - adds clock-ish grit
        int bits;                                 // 0 = none, else quantise the loop (digital styles)
        int heads;                                // >0: multi-head machine (3 heads, ratios 1 : 2 : 3)
        int read;                                 // ReadMode
        float stereoSpread;                       // 0..1 decorrelates L / R wobble
        float bandpassQ;                          // >0: radio / phone bandpass around sqrt(low * high)
    };

    inline int numStyles() { return 36; }
    inline const Style& style (int i)
    {
        static const Style s[] = {
            // name                   archetype                                                              sat         drv   lowCut  highCut  wowMs wowHz shape        flutMs flutHz diff  noise   band   bits heads read          spread bpQ
            { "Studio Tape 30",        "two-track studio tape at 30 ips: clean, tight, a little glue",         satTape,     2.0f,  30,    18000,  0.04f, 0.6f, wowDrift,    0.012f, 9.0f, 0.0f, -96.0f, 0,     0, 0, readNormal,   0.2f, 0 },
            { "Studio Tape 15",        "studio tape at 15 ips: rounder top, more head bump",                   satTape,     4.0f,  40,    14000,  0.07f, 0.5f, wowDrift,    0.018f, 8.0f, 0.0f, -90.0f, 0,     0, 0, readNormal,   0.2f, 0 },
            { "Studio Tape 7.5",       "studio tape at 7.5 ips: dark, soft, obvious wow",                      satTape,     6.0f,  50,    9000,   0.14f, 0.45f, wowDrift,   0.03f,  7.0f, 0.0f, -84.0f, 0,     0, 0, readNormal,   0.3f, 0 },
            { "Tube Tape Echo",        "1960s three-head tube tape echo: warm preamp, gentle wow",              satWarm,     6.0f,  60,    8000,   0.18f, 0.55f, wowSine,    0.03f,  6.5f, 0.05f, -82.0f, 0,    0, 0, readNormal,   0.3f, 0 },
            { "Spring Tape Echo",      "1970s multi-head tape echo with spring-loaded tape: thick, wobbly",    satTape,     7.0f,  80,    6500,   0.22f, 0.7f, wowDrift,    0.05f,  8.5f, 0.1f, -80.0f, 0,     0, 7, readNormal,   0.4f, 0 },
            { "Bucket Pedal",          "bucket-brigade pedal: dark, compander breath, clock grit",              satWarm,     5.0f,  60,    5000,   0.10f, 0.4f, wowTriangle, 0.0f,   0.0f, 0.0f, -86.0f, 12000, 0, 0, readNormal,   0.1f, 0 },
            { "Bucket Dark",           "long bucket-brigade line at its slowest clock: murky and lo-fi",        satDirt,     6.0f,  80,    2800,   0.12f, 0.35f, wowTriangle, 0.0f,  0.0f, 0.0f, -80.0f, 6000,  0, 0, readNormal,   0.1f, 0 },
            { "Oil Can",               "electrostatic oil-can echo: smeared, wobbling, liquid",                 satWarm,     4.0f,  100,   6000,   0.35f, 3.5f, wowSine,     0.08f, 11.0f, 0.35f, -86.0f, 0,    0, 0, readNormal,   0.6f, 0 },
            { "Drum Echo",             "magnetic drum echo: tight wow, honky mids",                             satWarm,     5.0f,  150,   7000,   0.06f, 4.0f, wowSine,     0.02f, 16.0f, 0.0f, -84.0f, 0,     0, 3, readNormal,   0.2f, 0 },
            { "Chorus Box",            "analog chorus-echo box: short times swim, long times shimmer",          satClean,    0.0f,  40,    9000,   1.20f, 0.8f, wowTriangle, 0.0f,   0.0f, 0.0f, -100.0f, 14000, 0, 0, readNormal,  1.0f, 0 },
            { "AM Radio",              "AM radio: narrow, distorted, a hint of static",                         satDirt,     10.0f, 300,   4500,   0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.0f, -60.0f, 0,     0, 0, readNormal,   0.0f, 0.9f },
            { "Telephone",             "landline telephone: 300 Hz to 3.4 kHz, edgy",                           satHardLimit, 8.0f, 300,   3400,   0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.0f, -100.0f, 0,    0, 0, readNormal,   0.0f, 1.2f },
            { "Shortwave",             "shortwave receiver: drifting, phasey, crackling",                      satDirt,     8.0f,  250,   3800,   0.6f, 0.15f, wowRandom,   0.05f,  5.0f, 0.2f, -54.0f, 0,     0, 0, readNormal,   0.8f, 0.7f },
            { "Transistor Radio",      "pocket transistor radio: thin, buzzy, small speaker",                   satDirt,     9.0f,  450,   5000,   0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.0f, -80.0f, 0,     0, 0, readNormal,   0.0f, 0.8f },
            { "Digital Clean",         "pristine digital delay: no colour at all",                              satClean,    0.0f,  10,    22000,  0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.0f, -200.0f, 0,    0, 0, readNormal,   0.0f, 0 },
            { "Digital 12-Bit",        "early 12-bit digital rack delay: crisp, a little gritty",               satClean,    0.0f,  20,    12000,  0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.0f, -200.0f, 16000, 12, 0, readNormal, 0.0f, 0 },
            { "Rack Digital 1980",     "1980 rack digital delay: bright, modulated, slightly crushed",          satClean,    1.0f,  30,    14000,  0.15f, 0.6f, wowTriangle, 0.0f,   0.0f, 0.0f, -200.0f, 18000, 14, 0, readNormal, 0.5f, 0 },
            { "Distressed",            "an old, mistreated tape echo: dropouts of tone, heavy wow",             satDirt,     8.0f,  120,   4500,   0.45f, 0.35f, wowRandom,  0.08f,  6.0f, 0.1f, -70.0f, 0,     0, 0, readNormal,   0.6f, 0 },
            { "Diffused",              "repeats soften into a cloud: allpass diffusion in the loop",            satTape,     2.0f,  60,    10000,  0.05f, 0.4f, wowDrift,    0.0f,   0.0f, 0.7f, -100.0f, 0,    0, 0, readNormal,   0.4f, 0 },
            { "Ambient",               "dark, diffused, slowly moving: echoes that become a pad",               satWarm,     2.0f,  100,   6000,   0.25f, 0.2f, wowDrift,    0.0f,   0.0f, 0.55f, -100.0f, 0,   0, 0, readNormal,   0.8f, 0 },
            { "Splatter",              "overloaded tape echo: each repeat drives the next harder",              satDirt,     14.0f, 90,    7000,   0.10f, 0.5f, wowDrift,    0.02f,  7.0f, 0.0f, -84.0f, 0,     0, 0, readNormal,   0.3f, 0 },
            { "Warm Analog Pedal",     "compact analog delay pedal: warm, soft, forgiving",                     satWarm,     3.0f,  50,    6500,   0.06f, 0.5f, wowTriangle, 0.0f,   0.0f, 0.0f, -92.0f, 10000, 0, 0, readNormal,   0.1f, 0 },
            { "Plate Head Echo",       "tube tape echo with a moving record head: rich, slightly unstable",     satWarm,     5.0f,  70,    7500,   0.20f, 0.6f, wowDrift,    0.04f,  7.5f, 0.0f, -84.0f, 0,     0, 0, readNormal,   0.4f, 0 },
            { "Ensemble Echo",         "modulated bucket echo: wide chorus on every repeat",                    satWarm,     2.0f,  60,    7000,   0.60f, 0.9f, wowTriangle, 0.0f,   0.0f, 0.1f, -90.0f, 11000, 0, 0, readNormal,  1.0f, 0 },
            { "Lo-Fi Sampler",         "8-bit-era sampler loop: aliasy, crunchy, punchy",                       satHardLimit, 4.0f, 60,    9000,   0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.0f, -200.0f, 9000, 8, 0, readNormal,   0.0f, 0 },
            { "Rockabilly Slap",       "single-head tape slap: bright, short, a touch of drive",                satTape,     5.0f,  120,   10000,  0.05f, 0.6f, wowDrift,    0.015f, 9.0f, 0.0f, -90.0f, 0,     0, 0, readNormal,   0.2f, 0 },
            { "Dub Siren Tape",        "worn dub tape echo: dark, saturating, made to be fed back hard",        satPump,     7.0f,  90,    3800,   0.16f, 0.4f, wowDrift,    0.03f,  6.0f, 0.15f, -80.0f, 0,    0, 0, readNormal,   0.4f, 0 },
            { "Broken Tape",           "crinkled tape: random pitch drops and smeared tone",                    satDirt,     6.0f,  100,   5500,   0.90f, 0.6f, wowRandom,   0.10f,  9.0f, 0.1f, -76.0f, 0,     0, 0, readNormal,   0.5f, 0 },
            { "Pitch Drift",           "slow wandering pitch on every repeat: seasick and beautiful",           satTape,     2.0f,  60,    9000,   1.40f, 0.12f, wowDrift,   0.0f,   0.0f, 0.1f, -96.0f, 0,     0, 0, readNormal,   0.7f, 0 },
            { "Small Studio Tape",     "semi-pro quarter-inch deck: honest, slightly soft",                     satTape,     3.0f,  45,    12000,  0.08f, 0.55f, wowDrift,   0.02f,  8.0f, 0.0f, -88.0f, 0,     0, 0, readNormal,   0.2f, 0 },
            // ---- Squelchmoth originals
            { "Lunar Tape",            "tape in low gravity: very slow wow, wide drift, soft top",              satTape,     3.0f,  40,    8500,   0.80f, 0.07f, wowDrift,   0.0f,   0.0f, 0.2f, -94.0f, 0,     0, 0, readNormal,   1.0f, 0 },
            { "Ice Diffuse",           "crystalline: bright repeats that freeze into diffusion",               satClean,    0.0f,  200,   16000,  0.03f, 0.3f, wowSine,     0.0f,   0.0f, 0.85f, -200.0f, 0,   0, 0, readNormal,   0.6f, 0 },
            { "Granule",               "every repeat is re-read as scattered grains",                           satWarm,     2.0f,  60,    11000,  0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.2f, -100.0f, 0,    0, 0, readGranular, 0.8f, 0 },
            { "Dust Cassette",         "a dusty cassette: hiss, dark wobble, tired transport",                  satTape,     5.0f,  80,    6000,   0.30f, 0.5f, wowRandom,   0.06f,  10.0f, 0.0f, -66.0f, 0,    0, 0, readNormal,   0.5f, 0 },
            { "Reverse Bloom",         "repeats play backwards and swell into the beat",                        satWarm,     2.0f,  60,    12000,  0.0f, 0.0f, wowSine,      0.0f,   0.0f, 0.15f, -100.0f, 0,   0, 0, readReverse,  0.3f, 0 },
            { "Infinite Wash",         "near-endless, heavily diffused tail that never quite clips",            satPump,     1.0f,  120,   7000,   0.20f, 0.15f, wowDrift,   0.0f,   0.0f, 0.75f, -100.0f, 0,   0, 0, readNormal,   1.0f, 0 },
        };
        return s[clamp (i, 0, numStyles() - 1)];
    }

    /** Rhythm patterns (taps in 1/16 notes within the pattern) - the pattern bank Claude and presets pick from. */
    struct Tap { float sixteenths, level, pan, cutoff, pitch; };
    struct Pattern { const char* name; float lengthSixteenths; std::vector<Tap> taps; };
    inline const std::vector<Pattern>& patterns()
    {
        static const std::vector<Pattern> p {
            { "Quarter + Dotted Eighth", 16, { { 3, 0.8f, -0.5f, 12000, 0 }, { 4, 0.6f, 0.5f, 10000, 0 }, { 8, 0.45f, -0.3f, 8000, 0 }, { 12, 0.3f, 0.3f, 6000, 0 } } },
            { "Gallop",                  16, { { 2, 0.7f, -0.6f, 12000, 0 }, { 3, 0.5f, 0.6f, 10000, 0 }, { 6, 0.5f, -0.4f, 9000, 0 }, { 7, 0.35f, 0.4f, 8000, 0 }, { 10, 0.35f, -0.2f, 7000, 0 }, { 11, 0.25f, 0.2f, 6000, 0 } } },
            { "Tresillo 3-3-2",          16, { { 3, 0.8f, -0.7f, 14000, 0 }, { 6, 0.65f, 0.7f, 11000, 0 }, { 8, 0.5f, 0.0f, 9000, 0 } } },
            { "Clave 3-2",               32, { { 3, 0.7f, -0.6f, 12000, 0 }, { 6, 0.6f, 0.6f, 11000, 0 }, { 10, 0.55f, -0.3f, 10000, 0 }, { 12, 0.5f, 0.3f, 9000, 0 } } },
            { "Triplet Cascade",         16, { { 2.6667f, 0.7f, -0.6f, 12000, 0 }, { 5.3333f, 0.55f, 0.6f, 9500, 0 }, { 8, 0.45f, -0.3f, 8000, 0 }, { 10.6667f, 0.35f, 0.3f, 6500, 0 }, { 13.3333f, 0.25f, 0.0f, 5000, 0 } } },
            { "Build-Up Roll",           16, { { 4, 0.3f, 0, 6000, 0 }, { 8, 0.4f, 0, 8000, 0 }, { 10, 0.5f, 0, 9000, 0 }, { 12, 0.6f, 0, 11000, 0 }, { 13, 0.7f, 0, 12000, 0 }, { 14, 0.8f, 0, 14000, 0 }, { 15, 0.9f, 0, 16000, 0 } } },
            { "Octave Ladder",           16, { { 4, 0.6f, -0.5f, 14000, 12 }, { 8, 0.45f, 0.5f, 12000, 24 }, { 12, 0.3f, 0.0f, 10000, -12 } } },
            { "Fifths Shimmer",          16, { { 3, 0.55f, -0.6f, 15000, 7 }, { 6, 0.45f, 0.6f, 15000, 12 }, { 9, 0.35f, -0.3f, 14000, 19 }, { 12, 0.25f, 0.3f, 12000, 24 } } },
            { "Wide Stereo Pairs",       16, { { 1.5f, 0.6f, -1, 12000, 0 }, { 2, 0.6f, 1, 12000, 0 }, { 5.5f, 0.4f, -1, 9000, 0 }, { 6, 0.4f, 1, 9000, 0 } } },
            { "Dub Skank",               16, { { 2, 0.6f, 0.5f, 3500, 0 }, { 6, 0.5f, -0.5f, 3000, 0 }, { 10, 0.4f, 0.5f, 2500, 0 }, { 14, 0.3f, -0.5f, 2000, 0 } } },
            { "Swing Eighths",           16, { { 2.667f, 0.7f, -0.4f, 12000, 0 }, { 4, 0.55f, 0.4f, 11000, 0 }, { 6.667f, 0.4f, -0.4f, 9000, 0 }, { 8, 0.3f, 0.4f, 8000, 0 } } },
            { "32-Tap Cloud",            32, [] { std::vector<Tap> v; for (int k = 0; k < 32; ++k) v.push_back ({ 0.5f + k * 0.97f, 0.55f * std::pow (0.93f, (float) k), (k % 2 ? 1.0f : -1.0f) * (0.2f + 0.8f * (float) ((k * 7) % 10) / 10.0f), 14000.0f * std::pow (0.95f, (float) k), 0 }); return v; }() },
        };
        return p;
    }

    struct Params
    {
        int mode = single, style = 0, timeChange = glideChange, heads = 1, limiter = limitSoft, pattern = 0;
        double timeMs = 375, timeRMs = 500; int syncL = -1, syncR = -1;   // sync index into WingMod::syncBeats, -1 = free
        float feedback = 0.35f, mix = 0.3f, saturation = 0.5f, groove = 0, feelMs = 0, width = 1, offsetMs = 0, accent = 0, warp = 0;
        float lowCut = 20, highCut = 20000, wow = 1, flutter = 1, diffusion = 0, duckDb = 0, duckReleaseMs = 250, inputDb = 0, outputDb = 0;
        bool freeze = false, prime = false, keyOn = false;
        std::vector<Tap> customTaps; float customLength = 16;              // Rhythm with custom taps (empty = pattern bank)
    };

    // ---------------------------------------------------------------- loop colour (one per channel)
    struct LoopChain
    {
        Biquad hp, lp, bp1, bp2; float apZ[4] {}; int apD[4] {}; std::vector<float> apBuf[4]; int apW[4] {};
        Nonlinear::Stage stage; HalfBand up, dn; DcBlock dc;
        float bandHold = 0, bandPhase = 0, bandStep = 1; int bits = 0; float noiseG = 0; uint32_t rng = 12345u;
        int sat = satClean; float satDrive = 1, diffusion = 0; int limiter = limitSoft; bool bandpass = false;
        double sr = 48000;
        void prepare (double rate, int seed)
        {
            sr = rate; rng = 12345u + (uint32_t) seed * 7919u;
            static const float apMs[4] = { 4.7f, 7.3f, 11.1f, 15.9f };
            for (int k = 0; k < 4; ++k) { apD[k] = (int) (apMs[k] * (1.0f + 0.07f * seed) * 0.001 * sr); apBuf[k].assign ((size_t) apD[k] + 1, 0.0f); apW[k] = 0; }
            const auto co = HalfBandDesign::design (90.0, 0.1);
            up.setCoefs (co); dn.setCoefs (co);
            dc.prepare (sr, 6.0);
            for (int t = 0; t < numSatTypes; ++t) { const auto m = satModel (t); Nonlinear::warm (m.first, m.second); }
        }
        static std::pair<int, float> satModel (int t)
        {
            switch (t)
            {
                case satTape: return { Nonlinear::tape, 0.6f };
                case satWarm: return { Nonlinear::glow, 0.4f };
                case satPump: return { Nonlinear::console, 0.3f };
                case satDirt: return { Nonlinear::diode, 0.6f };
                case satHardLimit: return { Nonlinear::brick, 0.15f };
                default: return { Nonlinear::tape, 0.0f };
            }
        }
        void configure (const Style& st, const Params& p)
        {
            const double lo = std::max ((double) st.lowCut, (double) p.lowCut), hi = std::min ((double) st.highCut, (double) p.highCut);
            hp.set (Biquad::highpass, sr, lo, 0.6); lp.set (Biquad::lowpass, sr, std::max (lo * 1.5, hi), 0.6);
            bandpass = st.bandpassQ > 0;
            if (bandpass) { const double c = std::sqrt (lo * hi); bp1.set (Biquad::bandpass, sr, c, st.bandpassQ); bp2.set (Biquad::peak, sr, c * 1.6, 1.0, 4.0); }
            sat = st.sat; satDrive = dbToGain (st.satDrive * p.saturation * 2.0f);
            const auto m = satModel (sat); stage.model = m.first; stage.character = m.second; stage.bias = 0.1f; stage.rebuild(); stage.prepare (sr * 2.0);
            diffusion = clamp (st.diffusion + p.diffusion, 0.0f, 0.95f);
            bands (st.bandHz); bits = st.bits; noiseG = st.noiseDb > -150 ? dbToGain (st.noiseDb) : 0.0f; limiter = p.limiter;
        }
        void bands (float hz) { bandStep = hz > 0 ? (float) std::min (1.0, hz * 2.0 / sr) : 1.0f; }
        void reset() { hp.reset(); lp.reset(); bp1.reset(); bp2.reset(); for (int k = 0; k < 4; ++k) { std::fill (apBuf[k].begin(), apBuf[k].end(), 0.0f); apW[k] = 0; } up.reset(); dn.reset(); stage.reset(); dc.reset(); bandHold = bandPhase = 0; }
        inline float noise() { rng = rng * 1664525u + 1013904223u; return (float) ((rng >> 8) & 0xffff) / 32767.5f - 1.0f; }

        inline float process (float x)
        {
            float y = lp.process (hp.process (x));
            if (bandpass) y = bp2.process (bp1.process (y)) * 1.4f;
            // diffusion: four nested allpasses (Schroeder), gain follows the amount
            if (diffusion > 0.001f)
            {
                const float g = 0.35f + 0.35f * diffusion;
                float d = y;
                for (int k = 0; k < 4; ++k)
                {
                    auto& b = apBuf[k]; const int n = (int) b.size();
                    const float delayed = b[(size_t) apW[k]];
                    const float v = d + g * delayed;
                    b[(size_t) apW[k]] = v;
                    apW[k] = (apW[k] + 1) % n;
                    d = delayed - g * v;
                }
                y = y + diffusion * (d - y);
            }
            // bucket / digital band-limit: sample-and-hold at the style's clock (the grit those lines have)
            if (bandStep < 1.0f) { bandPhase += bandStep; if (bandPhase >= 1.0f) { bandPhase -= 1.0f; bandHold = y; } y = bandHold; }
            if (bits > 0) { const float q = (float) (1 << (bits - 1)); y = std::round (y * q) / q; }
            // saturation at 2x with ADAA (unity small-signal: drive changes the harmonics, not the level)
            if (sat != satClean)
            {
                float a, b; up.up (y, a, b);
                stage.drive = satDrive;
                a = stage.process (a) / satDrive; b = stage.process (b) / satDrive;
                y = dn.down (a, b);
            }
            if (noiseG > 0) y += noiseG * noise();
            // limiter: soft keeps runaway feedback musical, hard clamps at 0 dBFS
            if (limiter == limitSoft) y = (float) (1.2 * Nonlinear::softAlgD (y / 1.2));
            else if (limiter == limitHard) y = clamp (y, -1.0f, 1.0f);
            else y = clamp (y, -8.0f, 8.0f);                                              // safety only
            return dc.process (undenorm (y));
        }
    };

    // ---------------------------------------------------------------- the processor
    class Processor
    {
    public:
        static constexpr double maxSeconds = 30.0;

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; block = std::max (16, maxBlock);
            int n = 1; while (n < (int) (sr * (maxSeconds + 1.0))) n <<= 1;
            for (int c = 0; c < 2; ++c) { buf[c].assign ((size_t) n, 0.0f); loop[c].prepare (sr, c); outColour[c].prepare (sr, c + 2); }
            mask = n - 1; w = 0;
            for (int c = 0; c < 2; ++c) { curT[c] = targetT[c] = oldT[c] = 0; xf[c] = 1; }
            applied = {}; applied.style = -1; configure (120.0, true);
            reset();
        }
        void reset()
        {
            for (int c = 0; c < 2; ++c) { std::fill (buf[c].begin(), buf[c].end(), 0.0f); loop[c].reset(); outColour[c].reset(); fbState[c] = 0; }
            w = 0; duckEnv = 0; wowPh = 0; flutPh = 0; for (auto& r : rnd) r = {}; warpEnv = 0; grainClock = 0;
            for (auto& g : grains) g = {};
            for (int c = 0; c < 2; ++c) { curT[c] = targetT[c]; oldT[c] = targetT[c]; xf[c] = 1; }
        }
        Params p;

        /** Delay times in samples for the current settings and tempo (L, R). */
        void times (double bpm, double& tl, double& tr) const
        {
            auto ms = [&] (int sync, double free) { return sync >= 0 ? 60000.0 / bpm * WingMod::syncBeats (sync) : free; };
            double l = ms (p.syncL, p.timeMs);
            double r = p.mode == dual ? ms (p.syncR, p.timeRMs) : l;
            if (p.mode == pingPong || p.mode == dual) r = r * (1.0 + p.groove);          // groove: swing the alternate repeats
            l += p.feelMs; r += p.feelMs + p.offsetMs;
            tl = clamp (l, 1.0, maxSeconds * 1000.0) * 0.001 * sr; tr = clamp (r, 1.0, maxSeconds * 1000.0) * 0.001 * sr;
            if (p.prime) { tl = nearestPrime ((int) std::round (tl)); tr = nearestPrime ((int) std::round (tr)); }
        }
        void configure (double bpm, bool force = false)
        {
            const auto& st = style (p.style);
            if (force || p.style != applied.style || p.lowCut != applied.lowCut || p.highCut != applied.highCut || p.saturation != applied.saturation
                || p.diffusion != applied.diffusion || p.limiter != applied.limiter)
                for (int c = 0; c < 2; ++c) { loop[c].configure (st, p); outColour[c].configure (st, p); outColour[c].diffusion = 0; outColour[c].noiseG = 0; }
            double tl, tr; times (bpm, tl, tr);
            for (int c = 0; c < 2; ++c)
            {
                const double t = c == 0 ? tl : tr;
                if (std::abs (t - targetT[c]) > 0.5)
                {
                    if (force || p.timeChange == jumpChange || targetT[c] <= 0) { curT[c] = t; xf[c] = 1; }
                    else if (p.timeChange == crossfadeChange) { oldT[c] = curT[c]; curT[c] = t; xf[c] = 0; }
                    targetT[c] = t;
                }
            }
            bpmNow = bpm;
            applied = p;
        }
        double latencySamples() const { return 0; }
        float tapLevel (int i) const   { return i >= 0 && i < 32 ? tapMeter[i] : 0.0f; }
        double currentTimeMs (int c) const { return curT[clamp (c, 0, 1)] / sr * 1000.0; }
        float duckGainDb() const { return gainToDb (duckGain); }

        /** In place. ch[0], ch[1]; key may be nullptr. */
        void process (float* const* ch, int nch, const float* const* key, int n, double bpm)
        {
            configure (bpm);
            const auto& st = style (p.style);
            float* L = ch[0]; float* R = nch > 1 ? ch[1] : ch[0];
            const float inG = dbToGain (p.inputDb), outG = dbToGain (p.outputDb);
            const float fb = p.freeze ? 1.0f : clamp (p.feedback, 0.0f, 1.1f);
            const float glideK = (float) (1.0 - std::exp (-1.0 / (0.12 * sr)));
            const float xfStep = (float) (1.0 / (0.06 * sr));
            const float duckA = (float) (1.0 - std::exp (-1.0 / (0.005 * sr))), duckR = (float) (1.0 - std::exp (-1.0 / (std::max (20.0f, p.duckReleaseMs) * 0.001 * sr)));
            const float warpA = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
            const double wowInc = st.wowHz / sr, flutInc = st.flutterHz / sr;
            const float wowS = (float) (st.wowMs * p.wow * 0.001 * sr), flutS = (float) (st.flutterMs * p.flutter * 0.001 * sr);
            const int heads = st.heads > 0 ? clamp (p.heads, 1, 7) : 1;
            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i] * inG, inR = R[i] * inG;
                // ducker: the input (or the key) pushes the wet signal down
                float det = 0.5f * (std::abs (inL) + std::abs (inR));
                if (p.keyOn && key != nullptr) det = 0.5f * (std::abs (key[0][i]) + std::abs (key[1][i]));
                duckEnv += (det > duckEnv ? duckA : duckR) * (det - duckEnv);
                duckGain = p.duckDb > 0 ? dbToGain (-p.duckDb * clamp ((gainToDb (duckEnv + 1.0e-9f) + 40.0f) / 34.0f, 0.0f, 1.0f)) : 1.0f;
                // warp: loud input stretches the tape (the transport slows a touch)
                warpEnv += warpA * (det - warpEnv);
                const float warpS = p.warp * warpEnv * (float) (0.004 * sr);
                // wow / flutter
                wowPh += wowInc; if (wowPh >= 1) wowPh -= 1; flutPh += flutInc; if (flutPh >= 1) flutPh -= 1;
                float mod[2];
                for (int c = 0; c < 2; ++c)
                {
                    const double ph = wowPh + (c ? st.stereoSpread * 0.25 : 0.0);
                    mod[c] = wowS * wowValue (st.wowShape, ph, c) + flutS * (float) std::sin (2 * pi * (flutPh + (c ? st.stereoSpread * 0.3 : 0.0)));
                }
                // time glide / crossfade
                for (int c = 0; c < 2; ++c)
                {
                    if (p.timeChange == glideChange) curT[c] += glideK * (targetT[c] - curT[c]);
                    if (xf[c] < 1) xf[c] = std::min (1.0f, xf[c] + xfStep);
                }
                float wetL = 0, wetR = 0;
                if (p.mode == rhythm) rhythmTick (inL, inR, fb, mod, wetL, wetR);
                else
                {
                    float y[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        auto readAt = [&] (double t) { return readHeads (c, t + mod[c] + warpS, heads, st.read); };
                        float v = readAt (curT[c]);
                        if (xf[c] < 1) v = v * xf[c] + readAt (oldT[c]) * (1 - xf[c]);
                        y[c] = loop[c].process (v);
                    }
                    if (p.mode == single) { const float m = 0.5f * (y[0] + y[1]); y[0] = y[1] = m; }
                    // write: input + feedback (ping-pong crosses the channels; a mono source starts on the left)
                    const float feedIn = p.freeze ? 0.0f : 1.0f;
                    float wl, wr;
                    if (p.mode == pingPong) { wl = feedIn * 0.5f * (inL + inR) * (1.0f + p.accent * 0.5f) + fb * y[1]; wr = fb * y[0]; }
                    else { wl = feedIn * inL + fb * y[0]; wr = feedIn * inR + fb * y[1]; }
                    if (p.mode == single) { const float m = 0.5f * (wl + wr); wl = wr = m; }
                    buf[0][(size_t) w] = wl; buf[1][(size_t) w] = wr;
                    wetL = y[0]; wetR = y[1];
                }
                // width (M/S), duck, mix
                const float m = 0.5f * (wetL + wetR), s = 0.5f * (wetL - wetR) * p.width;
                wetL = (m + s) * duckGain; wetR = (m - s) * duckGain;
                L[i] = (L[i] * (1.0f - p.mix) + wetL * p.mix) * outG;
                if (nch > 1) R[i] = (R[i] * (1.0f - p.mix) + wetR * p.mix) * outG;
                w = (w + 1) & mask;
            }
            for (auto& t : tapMeter) t *= 0.9f;
        }

    private:
        double sr = 48000, bpmNow = 120; int block = 512;
        std::vector<float> buf[2]; int mask = 0, w = 0;
        LoopChain loop[2], outColour[2];
        double curT[2] {}, targetT[2] {}, oldT[2] {}; float xf[2] { 1, 1 };
        float fbState[2] {}, duckEnv = 0, duckGain = 1, warpEnv = 0;
        double wowPh = 0, flutPh = 0;
        struct RandomWalk { float a = 0, b = 0, ph = 1; uint32_t s = 777u; } rnd[2];
        float tapMeter[32] {};
        struct Grain { double pos = 0; double len = 0; double age = 0; float pan = 0; bool on = false; } grains[8];
        double grainClock = 0; uint32_t grng = 4242u;
        Params applied;

        static int nearestPrime (int n)
        {
            auto isPrime = [] (int v) { if (v < 2) return false; for (int d = 2; d * d <= v; ++d) if (v % d == 0) return false; return true; };
            for (int k = 0; k < 1000; ++k) { if (isPrime (n - k)) return n - k; if (isPrime (n + k)) return n + k; }
            return n;
        }
        float wowValue (int shape, double ph, int c)
        {
            ph -= std::floor (ph);
            switch (shape)
            {
                case wowTriangle: return (float) (ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph);
                case wowSquare:   return (float) std::tanh (6.0 * std::sin (2 * pi * ph)) ;
                case wowDrift:    return (float) (0.6 * std::sin (2 * pi * ph) + 0.3 * std::sin (2 * pi * ph * 2.31 + 1.0) + 0.1 * std::sin (2 * pi * ph * 5.77 + 2.0));
                case wowRandom:
                {
                    auto& r = rnd[c];
                    const double inc = style (p.style).wowHz / sr;
                    r.ph += (float) inc;
                    if (r.ph >= 1) { r.ph -= 1; r.a = r.b; r.s = r.s * 1664525u + 1013904223u; r.b = (float) ((r.s >> 8) & 0xffff) / 32767.5f - 1.0f; }
                    const float t = 0.5f - 0.5f * (float) std::cos (pi * r.ph);
                    return r.a + (r.b - r.a) * t;
                }
                default: return (float) std::sin (2 * pi * ph);
            }
        }
        /** Hermite read t samples ago. */
        inline float read (int c, double t) const
        {
            t = clamp (t, 2.0, (double) mask - 4);
            const double rp = (double) w - t; const int64_t i = (int64_t) std::floor (rp); const float f = (float) (rp - (double) i);
            auto at = [&] (int64_t k) { return buf[c][(size_t) (k & mask)]; };
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
        /** Normal / multi-head / reverse / granular read of one channel. */
        float readHeads (int c, double t, int heads, int mode)
        {
            if (mode == readReverse)
            {
                // two reversed windows: the delay grows at 2 samples per sample (playback backwards), Hann crossfade
                const double T = std::max (64.0, t);
                double ph = std::fmod ((double) w / T, 1.0);
                float y = 0;
                for (int k = 0; k < 2; ++k)
                {
                    double q = ph + 0.5 * k; q -= std::floor (q);
                    const float win = 0.5f - 0.5f * (float) std::cos (2 * pi * q);
                    y += win * read (c, T * 0.5 + q * T);
                }
                return y;
            }
            if (mode == readGranular) return granular (c, t);
            if (heads <= 1) return read (c, t);
            // multi-head machine: heads at 1x, 2x, 3x the time; mask bits choose them
            float y = 0; int on = 0;
            for (int h = 0; h < 3; ++h) if (heads & (1 << h)) { y += read (c, t * (h + 1)); ++on; }
            return on > 0 ? y / std::sqrt ((float) on) : 0.0f;
        }
        float granular (int c, double t)
        {
            // 8 grains of 40-90 ms scattered +-20 % around the delay time, each Hann-windowed
            if (c == 0)
            {
                grainClock -= 1;
                if (grainClock <= 0)
                {
                    grainClock = 0.012 * sr;
                    for (auto& g : grains) if (! g.on)
                    {
                        grng = grng * 1664525u + 1013904223u; const float r1 = (float) ((grng >> 8) & 0xffff) / 65535.0f;
                        grng = grng * 1664525u + 1013904223u; const float r2 = (float) ((grng >> 8) & 0xffff) / 65535.0f;
                        g.on = true; g.age = 0; g.len = (0.04 + 0.05 * r1) * sr; g.pos = t * (0.8 + 0.4 * r2); g.pan = r1 * 2 - 1; break;
                    }
                }
                for (auto& g : grains) if (g.on && (g.age += 1) >= g.len) g.on = false;
            }
            float y = 0;
            for (auto& g : grains) if (g.on)
            {
                const float win = 0.5f - 0.5f * (float) std::cos (2 * pi * g.age / g.len);
                const float pan = c == 0 ? 1.0f - std::max (0.0f, g.pan) : 1.0f + std::min (0.0f, g.pan);
                y += win * pan * read (c, g.pos);
            }
            return y * 0.6f;
        }
        void rhythmTick (float inL, float inR, float fb, const float* mod, float& outL, float& outR)
        {
            const auto& pat = patterns()[(size_t) clamp (p.pattern, 0, (int) patterns().size() - 1)];
            const auto& taps = p.customTaps.empty() ? pat.taps : p.customTaps;
            const float lenSix = p.customTaps.empty() ? pat.lengthSixteenths : p.customLength;
            const double six = 60.0 / bpmNow / 4.0 * sr;                             // samples per 1/16
            const double L = std::max (16.0, lenSix * six);
            // the pattern repeats every pattern length, with feedback through the loop colour
            const float back0 = loop[0].process (read (0, L + mod[0])), back1 = loop[1].process (read (1, L + mod[1]));
            const float feedIn = p.freeze ? 0.0f : 1.0f;
            buf[0][(size_t) w] = feedIn * inL + fb * back0; buf[1][(size_t) w] = feedIn * inR + fb * back1;
            float l = 0, r = 0; int k = 0;
            for (const auto& tp : taps)
            {
                if (k >= 32) break;
                double t = tp.sixteenths * six + (k % 2 ? p.groove * six : 0.0);
                float vl = tp.pitch != 0 ? pitched (0, k, t, tp.pitch) : read (0, t + mod[0]);
                float vr = tp.pitch != 0 ? pitched (1, k, t, tp.pitch) : read (1, t + mod[1]);
                // one-pole tap filter
                const float g = (float) (1.0 - std::exp (-2.0 * pi * std::min (20000.0f, tp.cutoff) / sr));
                tapLp[k][0] += g * (vl - tapLp[k][0]); tapLp[k][1] += g * (vr - tapLp[k][1]);
                const float lvl = tp.level * (k == 0 ? 1.0f + p.accent : 1.0f);
                const float pan = clamp (tp.pan, -1.0f, 1.0f);
                const float gl = std::sqrt (0.5f * (1.0f - pan)) * 1.41421f, gr = std::sqrt (0.5f * (1.0f + pan)) * 1.41421f;
                const float m = 0.5f * (tapLp[k][0] + tapLp[k][1]);
                l += lvl * gl * (0.5f * tapLp[k][0] + 0.5f * m); r += lvl * gr * (0.5f * tapLp[k][1] + 0.5f * m);
                tapMeter[k] = std::max (tapMeter[k], std::abs (lvl * m));
                ++k;
            }
            outL = outColour[0].process (l); outR = outColour[1].process (r);
        }
        float tapLp[32][2] {};
        double pitchPh[32] {};
        /** Delay-line pitch shifter: two read heads sweep a 50 ms window in opposite phase (Hann crossfade). */
        float pitched (int c, int slot, double t, float semis)
        {
            const double ratio = std::pow (2.0, semis / 12.0), W = 0.05 * sr;
            if (c == 0) { pitchPh[slot] += (1.0 - ratio) / W; pitchPh[slot] -= std::floor (pitchPh[slot]); }
            float y = 0;
            for (int h = 0; h < 2; ++h)
            {
                double q = pitchPh[slot] + 0.5 * h; q -= std::floor (q);
                y += (0.5f - 0.5f * (float) std::cos (2 * pi * q)) * read (c, t + q * W);
            }
            return y;
        }
    };
}
