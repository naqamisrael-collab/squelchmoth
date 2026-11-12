#pragma once
// Torque Comp - DSP core (1.14 "Studio Line", phase B). No UI, no te:: types: tests/torque_test.cpp measures it alone.
//
// One engine, fourteen styles. A style is not a preset: it sets HOW the compressor behaves - the detector (peak,
// RMS window, hybrid), the topology (feed-forward or feedback), the knee's shape, how attack and release react to
// the programme, how the ratio behaves with level, and the colour the gain stage adds. The user's controls then set
// how much: threshold, ratio, knee (0-72 dB), range, attack, release (or auto), hold, lookahead (0-20 ms), the key
// (internal, external, MIDI notes, tempo pulse) through a 6-band key EQ (Spectra EQ's analog-matched bands), stereo
// link and Mid / Side routing, character drive (oversampled up to 32x, before or after the gain stage), auto
// threshold, auto gain, mix (0-100 % blends dry and compressed; 100-200 % deepens the gain change, 200 % = twice the
// gain reduction) and output.
//
// Signal flow per sample:  key -> key EQ -> detector -> gain computer (dB) -> ballistics -> link / M-S ->
//                          main (delayed by the lookahead) x gain -> character (oversampled) -> makeup -> mix with
//                          the dry signal delayed by the same latency -> output.

#include <JuceHeader.h>
#include "SqSpectraDsp.h"
#include "SqApexDsp.h"
#include <array>
#include <cmath>
#include <vector>

namespace SqTorque
{
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    enum class Key { internal, external, midi, tempo };
    enum class Routing { stereo, midOnly, sideOnly, midToSide, sideToMid };
    enum class Colour { off, warm, bright, iron };   // even harmonics / odd with presence / transformer-like low thickening
    inline juce::StringArray keyNames()     { return { "Internal", "External", "MIDI", "Tempo" }; }
    inline juce::StringArray routingNames() { return { "Stereo", "Mid Only", "Side Only", "Mid to Side", "Side to Mid" }; }
    inline juce::StringArray colourNames()  { return { "Off", "Warm", "Bright", "Iron" }; }

    /** How a style behaves. */
    struct Style
    {
        const char* name;
        const char* about;
        double rmsMs;           // 0 = peak detector; else an RMS window (ms)
        double peakBlend;       // 0..1 of peak mixed into the RMS detector (hybrid)
        double feedback;        // 0 = feed-forward, 1 = pure feedback
        double kneeExtra;       // dB added to the user's knee (the style's natural softness)
        double attackLaw;       // 0 = fixed; >0: attack slows by this factor on slow-rising programme
        int releaseLaw;         // 0 fixed, 1 programme-dependent (crest), 2 two-stage opto, 3 shaped pump
        double muLaw;           // 0 = fixed ratio; >0: the ratio grows from 1 to its setting over this many dB of overshoot
        Colour colour;          // the style's own colour (used when Character is on and set to "Style")
        double colourByGr;      // colour that rises with gain reduction (dB of drive per dB of GR)
        bool upward;            // upward compression (quiet parts brought up)
        double lookaheadMin;    // ms of lookahead the style always uses (0 for most)
    };
    inline const std::array<Style, 14>& styles()
    {
        static const std::array<Style, 14> s { {
            { "Clean",            "Transparent all-rounder: hybrid peak / RMS detection, feed-forward",                 2.0, 0.5, 0.0,  0.0, 0.0, 1, 0.0,  Colour::off,    0.00, false, 0.0 },
            { "Punch",            "Lets the hit through, then grips: peak detection, attack eases on slow swells",       0.0, 1.0, 0.0,  0.0, 2.0, 1, 0.0,  Colour::bright, 0.05, false, 0.0 },
            { "Glue",             "Bus glue: feedback topology, RMS, soft and slow to react",                            10.0, 0.2, 0.7, 6.0, 0.0, 1, 0.0,  Colour::warm,   0.03, false, 0.0 },
            { "Opto",             "Optical leveller: smooth onset, fast-then-slow release that remembers how hard it worked", 8.0, 0.3, 0.6, 9.0, 1.0, 2, 0.0, Colour::warm, 0.02, false, 0.0 },
            { "Variable Mu",      "Tube-style: the harder you push, the higher the ratio; thick, gentle",               6.0, 0.3, 0.3, 12.0, 0.5, 1, 18.0, Colour::warm,   0.08, false, 0.0 },
            { "FET",              "Fast and forward: peak detection, colour that grows with gain reduction",              0.0, 1.0, 0.0,  2.0, 0.0, 1, 0.0,  Colour::bright, 0.12, false, 0.0 },
            { "Bus",              "Solid VCA bus compression: RMS, feed-forward, tight and predictable",                 5.0, 0.4, 0.0,  3.0, 0.0, 0, 0.0,  Colour::iron,   0.02, false, 0.0 },
            { "Vocal",            "Rides a voice: RMS with peak awareness, programme-dependent release",                 15.0, 0.35, 0.2, 8.0, 1.0, 1, 0.0,  Colour::warm,   0.02, false, 0.0 },
            { "Mastering",        "Barely there: long RMS, very soft knee, 3 ms of lookahead built in",                  30.0, 0.2, 0.0, 12.0, 0.5, 1, 0.0,  Colour::off,    0.00, false, 3.0 },
            { "Leveler",          "Evens out a whole performance: a 300 ms window, slow and musical",                   300.0, 0.0, 0.0, 10.0, 0.0, 0, 0.0,  Colour::off,    0.00, false, 0.0 },
            { "Upward",           "Brings the quiet parts up instead of pushing the loud parts down",                    10.0, 0.3, 0.0,  6.0, 0.0, 1, 0.0,  Colour::off,    0.00, true,  0.0 },
            { "Pump",             "Breathes in time: peak key, a release curve shaped to swell back",                     0.0, 1.0, 0.0,  0.0, 0.0, 3, 0.0,  Colour::off,    0.00, false, 0.0 },
            { "Smash",            "Room-mic destruction: very fast, hard knee, heavy colour as it digs in",               0.0, 1.0, 0.1,  0.0, 0.0, 1, 0.0,  Colour::bright, 0.25, false, 0.0 },
            { "Transparent Peak", "Catches every peak before it happens: 5 ms lookahead, clean",                          0.0, 1.0, 0.0,  1.0, 0.0, 1, 0.0,  Colour::off,    0.00, false, 5.0 },
        } };
        return s;
    }

    struct Params
    {
        int style = 0;
        double threshDb = -18, ratio = 4, kneeDb = 6, rangeDb = 60;
        double attackMs = 10, releaseMs = 120, holdMs = 0, lookaheadMs = 0;
        bool autoRelease = false, autoThreshold = false, autoGain = false;
        Key key = Key::internal;
        Routing routing = Routing::stereo;
        double link = 1.0;                       // 0..1
        int characterMode = 0;                   // 0 off, 1 style's colour, 2 warm, 3 bright, 4 iron
        double driveDb = 0;                      // 0..24
        bool characterPost = true;
        int oversampleLog2 = 2;                  // 0..5 (1x..32x)
        double mix = 1.0;                        // 0..2
        double makeupDb = 0, outputDb = 0;
        bool audition = false;                   // hear the key (after its EQ)
        double tempoDivBeats = 1.0;              // tempo pulse length (beats)
        std::array<SqSpectra::Band, 6> keyEq {};
    };

    /** The static curve: gain change (dB) for a detector level x (dB) - shared by the engine, the face and the tests. */
    inline double staticGainDb (double x, double T, double R, double W, double range, bool upward, double muLaw)
    {
        R = std::max (1.0, R); W = std::max (0.0, W);
        if (upward)
        {
            // mirror: below the threshold the level is raised; the knee rounds the corner at T
            const double d = T - x;                              // how far below
            double g;
            if (2 * d < -W) g = 0;
            else if (2 * std::abs (d) <= W && W > 0) g = (1 - 1 / R) * (d + W / 2) * (d + W / 2) / (2 * W);
            else g = (1 - 1 / R) * d;
            return std::min (range, std::max (0.0, g));
        }
        double over = x - T;
        double r = R;
        if (muLaw > 0 && over > 0) r = 1 + (R - 1) * std::min (1.0, (over + W / 2) / muLaw);   // ratio grows with the push
        double g;
        if (2 * over < -W) g = 0;
        else if (2 * std::abs (over) <= W && W > 0) g = (1 / r - 1) * (over + W / 2) * (over + W / 2) / (2 * W);
        else g = (1 / r - 1) * over;
        return std::max (-range, std::min (0.0, g));
    }

    //==========================================================================
    class Engine
    {
    public:
        Params p;

        void prepare (double sampleRate, int maxBlock)
        {
            fs = sampleRate; block = std::max (64, maxBlock);
            const int maxLook = (int) std::ceil (0.020 * fs) + 8;
            for (auto& d : look) d.assign ((size_t) (maxLook + 1), 0.0f);
            dryLen = maxLook + 2048 + 8;
            for (auto& d : dry) d.assign ((size_t) dryLen, 0.0f);
            for (int i = 0; i < 6; ++i) for (auto& kb : keyBands[i]) kb = {};
            for (int k = 0; k <= 5; ++k)
            {
                os[(size_t) k] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) k, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
                os[(size_t) k]->initProcessing ((size_t) block);
            }
            loud.prepare (fs);
            scratch.setSize (2, block * 2);
            designedKey.fill ({}); keyDirty = true;
            reset();
        }
        void reset()
        {
            for (auto& d : look) std::fill (d.begin(), d.end(), 0.0f);
            for (auto& d : dry) std::fill (d.begin(), d.end(), 0.0f);
            lookW = dryW = 0; grDb[0] = grDb[1] = 0; env[0] = env[1] = 0; rms[0] = rms[1] = 0; slowRel[0] = slowRel[1] = 0; holdLeft[0] = holdLeft[1] = 0;
            lastGain[0] = lastGain[1] = 1; avgIn = -60; avgGr = 0; crestPk[0] = crestPk[1] = crestRm[0] = crestRm[1] = 0; pulseEnv = 0;
            for (auto& o : os) if (o) o->reset();
            for (auto& b : keyBands) for (auto& kb : b) kb.reset();
            loud.reset();
        }

        /** Latency (samples) this setting adds: lookahead + the oversampler (only while character is on). */
        int latency() const { return lookSamples() + osLatency(); }
        int lookSamples() const { return (int) std::lround (std::max (p.lookaheadMs, styles()[(size_t) clampStyle()].lookaheadMin) * 0.001 * fs); }
        int osLatency() const
        {
            if (! characterOn() || p.oversampleLog2 <= 0) return 0;
            return (int) std::lround (os[(size_t) std::min (5, p.oversampleLog2)]->getLatencyInSamples());
        }
        bool characterOn() const { return p.characterMode > 0 && (p.driveDb > 0.01 || styles()[(size_t) clampStyle()].colourByGr > 0); }

        /** MIDI key: a note-on at sample offset 'at' in the next block, velocity 0..1. */
        void midiTrigger (int at, float velocity) { if (nTrig < (int) trig.size()) trig[(size_t) nTrig++] = { at, velocity }; }
        /** Tempo key: the beat position at the start of the next block and the tempo. */
        void setTempo (double ppqAtStart, double bpm, bool playing) { ppq = ppqAtStart; tempoBpm = bpm > 1 ? bpm : 120; isPlaying = playing; }

        // meters (read by the face and Claude)
        float meterGrDb = 0, meterInDb = -100, meterOutDb = -100, meterKeyDb = -100, momentaryLufs = -100, autoGainDb = 0, autoThreshDb = -18;

        void process (float* const* ch, int numCh, int n, const float* const* ext = nullptr, int extCh = 0)
        {
            const bool stereo = numCh >= 2;
            const auto& st = styles()[(size_t) clampStyle()];
            if (keyDirty || ! (designedKey == p.keyEq)) designKey();
            const int L = lookSamples(), osLat = osLatency(), totalLat = L + osLat;
            const double W = p.kneeDb + st.kneeExtra;
            const double atkMs = std::max (0.005, p.attackMs), relMs = std::max (1.0, p.releaseMs);
            const double aCoef = std::exp (-1.0 / (atkMs * 0.001 * fs)), holdN = p.holdMs * 0.001 * fs;
            const double rmsCoef = st.rmsMs > 0 ? std::exp (-1.0 / (st.rmsMs * 0.001 * fs)) : 0.0;
            const double crestCoefFast = std::exp (-1.0 / (0.002 * fs)), crestCoefSlow = std::exp (-1.0 / (0.3 * fs));
            const double avgCoef = std::exp (-1.0 / (3.0 * fs));
            // auto threshold: the threshold becomes relative to the programme's 3 s average
            const double T = p.autoThreshold ? avgIn + p.threshDb : p.threshDb;
            autoThreshDb = (float) T;
            float grMax = 0, inPk = 0, outPk = 0, keyPk = 0;
            // character before the gain stage: colour the input (oversampled) and keep the clean copy for the mix
            const bool pre = characterOn() && ! p.characterPost && ! p.audition;
            if (pre)
            {
                for (int c = 0; c < 2; ++c) scratch.copyFrom (c, 0, ch[stereo ? c : 0], n);
                applyCharacter (ch, numCh, n, -lastGrDb);
            }

            // tempo pulse state for this block
            const double beatsPerSample = tempoBpm / 60.0 / fs;
            int trigIdx = 0;

            for (int s = 0; s < n; ++s)
            {
                float in[2] = { ch[0][s], stereo ? ch[1][s] : ch[0][s] };
                const float clean[2] = { pre ? scratch.getSample (0, s) : in[0], pre ? scratch.getSample (1, s) : in[1] };
                inPk = std::max (inPk, std::max (std::abs (in[0]), std::abs (in[1])));
                // ---- the key
                double k[2];
                switch (p.key)
                {
                    case Key::external: k[0] = ext != nullptr && extCh > 0 ? ext[0][s] : 0.0; k[1] = ext != nullptr && extCh > 1 ? ext[1][s] : k[0]; break;
                    case Key::midi:
                    {
                        while (trigIdx < nTrig && trig[(size_t) trigIdx].at <= s) { pulseEnv = std::max (pulseEnv, (double) trig[(size_t) trigIdx].vel); ++trigIdx; }
                        k[0] = k[1] = pulseEnv; pulseEnv *= std::exp (-1.0 / (0.030 * fs));
                        break;
                    }
                    case Key::tempo:
                    {
                        const double beat = ppq + s * beatsPerSample, div = std::max (1.0 / 16.0, p.tempoDivBeats);
                        const double ph = std::fmod (beat, div) / div;
                        k[0] = k[1] = isPlaying && ph < 0.12 ? 1.0 : 0.0;
                        break;
                    }
                    default: k[0] = in[0]; k[1] = in[1]; break;
                }
                // feedback topology: the detector also hears the compressed output
                if (st.feedback > 0 && p.key == Key::internal)
                    for (int c = 0; c < 2; ++c) k[c] = (1 - st.feedback) * k[c] + st.feedback * k[c] * lastGain[c];
                // key EQ
                for (int b = 0; b < 6; ++b) if (p.keyEq[(size_t) b].enabled) for (int c = 0; c < 2; ++c) k[c] = keyBands[(size_t) b][(size_t) c].run (0, k[c]);
                keyPk = std::max (keyPk, (float) std::max (std::abs (k[0]), std::abs (k[1])));
                // ---- M/S routing of the detector
                double det[2];
                switch (p.routing)
                {
                    case Routing::midOnly: case Routing::midToSide: det[0] = det[1] = 0.5 * (k[0] + k[1]); break;
                    case Routing::sideOnly: case Routing::sideToMid: det[0] = det[1] = 0.5 * (k[0] - k[1]); break;
                    default: det[0] = k[0]; det[1] = stereo ? k[1] : k[0]; break;
                }
                // ---- detector + gain computer + ballistics, per channel
                double target[2];
                for (int c = 0; c < 2; ++c)
                {
                    const double a = std::abs (det[c]);
                    double lev;
                    if (st.rmsMs > 0) { rms[c] = rmsCoef * rms[c] + (1 - rmsCoef) * a * a; lev = st.peakBlend * a + (1 - st.peakBlend) * std::sqrt (rms[c]) * 1.41421356; }
                    else lev = a;
                    crestPk[c] = std::max (a, crestPk[c] * crestCoefFast);
                    crestRm[c] = crestCoefSlow * crestRm[c] + (1 - crestCoefSlow) * a * a;
                    target[c] = staticGainDb (gainToDb (lev), T, p.ratio, W, p.rangeDb, st.upward, st.muLaw);
                }
                // stereo link (in dB, towards the channel that wants the most gain change)
                if (p.routing == Routing::stereo)
                {
                    const double strongest = st.upward ? std::min (target[0], target[1]) : std::min (target[0], target[1]);
                    for (int c = 0; c < 2; ++c) target[c] = target[c] + p.link * (strongest - target[c]);
                }
                for (int c = 0; c < 2; ++c)
                {
                    const bool moreWork = st.upward ? target[c] > grDb[c] : target[c] < grDb[c];
                    if (moreWork)
                    {
                        double ac = aCoef;
                        if (st.attackLaw > 0)
                        {
                            // slow swells (low crest) get a slower attack; transients the set one
                            const double crest = crestPk[c] / std::sqrt (crestRm[c] + 1e-12);
                            const double slow = 1.0 + st.attackLaw * juce::jlimit (0.0, 1.0, (3.0 - crest) / 2.0);
                            ac = std::exp (-1.0 / (atkMs * slow * 0.001 * fs));
                        }
                        grDb[c] = ac * grDb[c] + (1 - ac) * target[c];
                        holdLeft[c] = holdN;
                        slowRel[c] = std::min (1.0, slowRel[c] + 1.0 / (2.0 * fs));    // opto memory charges while working
                    }
                    else if (holdLeft[c] > 0) holdLeft[c] -= 1;
                    else
                    {
                        double rms_ = relMs;
                        if (p.autoRelease || st.releaseLaw == 1)
                        {
                            const double crest = crestPk[c] / std::sqrt (crestRm[c] + 1e-12);
                            const double fast = juce::jlimit (0.0, 1.0, (crest - 1.5) / 4.0);   // spiky material: release faster
                            rms_ = p.autoRelease ? juce::jmap (fast, relMs * 3.0, relMs * 0.3) : juce::jmap (fast, relMs * 1.6, relMs * 0.7);
                        }
                        double rc = std::exp (-1.0 / (rms_ * 0.001 * fs));
                        if (st.releaseLaw == 2)
                        {
                            // opto: the first half returns fast (60 ms), the rest at a speed set by how long it worked
                            const double depth = std::abs (grDb[c]) / std::max (0.1, std::abs (target[c] - grDb[c]) + std::abs (grDb[c]));
                            const double slowMs = relMs * (1.0 + 8.0 * slowRel[c]);
                            rc = depth > 0.5 ? std::exp (-1.0 / (60.0 * 0.001 * fs)) : std::exp (-1.0 / (slowMs * 0.001 * fs));
                            slowRel[c] = std::max (0.0, slowRel[c] - 1.0 / (6.0 * fs));
                        }
                        if (st.releaseLaw == 3)
                        {
                            // pump: slow at first, then swells back quickly (an S-shaped return)
                            const double t = juce::jlimit (0.0, 1.0, 1.0 - std::abs (grDb[c]) / std::max (0.5, p.rangeDb));
                            rc = std::exp (-1.0 / (relMs * (1.6 - 1.2 * t) * 0.001 * fs));
                        }
                        grDb[c] = rc * grDb[c] + (1 - rc) * target[c];
                    }
                    if (! std::isfinite (grDb[c])) grDb[c] = 0;
                }
                // ---- gain applied to the delayed main signal
                look[0][(size_t) lookW] = in[0]; look[1][(size_t) lookW] = in[1];
                const int rd = (lookW - L + (int) look[0].size()) % (int) look[0].size();
                lookW = (lookW + 1) % (int) look[0].size();
                double x0 = look[0][(size_t) rd], x1 = look[1][(size_t) rd];
                // Mix above 100 % deepens the gain change itself (200 % = twice the gain reduction); below 100 % it blends
                const double depth = std::max (1.0, juce::jlimit (0.0, 2.0, p.mix));
                const double g0 = dbToGain (grDb[0] * depth), g1 = dbToGain (grDb[1] * depth);
                switch (p.routing)
                {
                    case Routing::stereo: x0 *= g0; x1 *= g1; break;
                    case Routing::midOnly: case Routing::sideToMid: { double m = 0.5 * (x0 + x1), sd = 0.5 * (x0 - x1); m *= g0; x0 = m + sd; x1 = m - sd; break; }
                    case Routing::sideOnly: case Routing::midToSide: { double m = 0.5 * (x0 + x1), sd = 0.5 * (x0 - x1); sd *= g0; x0 = m + sd; x1 = m - sd; break; }
                }
                lastGain[0] = g0; lastGain[1] = g1;
                grMax = (float) (st.upward ? std::max ((double) grMax, std::max (grDb[0], grDb[1])) : std::min ((double) grMax, std::min (grDb[0], grDb[1])));
                ch[0][s] = (float) (p.audition ? k[0] : x0);
                if (stereo) ch[1][s] = (float) (p.audition ? k[1] : x1);
                // the dry copy for the mix, delayed with the main path
                dry[0][(size_t) dryW] = clean[0]; dry[1][(size_t) dryW] = clean[1];
                dryW = (dryW + 1) % dryLen;
                // programme statistics for auto threshold / auto gain
                const double inDb = gainToDb (std::max (std::abs (in[0]), std::abs (in[1])) + 1e-9);
                if (inDb > -70) { avgIn = avgCoef * avgIn + (1 - avgCoef) * inDb; avgGr = avgCoef * avgGr + (1 - avgCoef) * 0.5 * (grDb[0] + grDb[1]); }
            }
            nTrig = 0;
            ppq += n * beatsPerSample;

            // ---- character (oversampled), on the compressed signal (post) - or on its input when 'pre'
            if (characterOn() && p.characterPost && ! p.audition) applyCharacter (ch, numCh, n, (double) -grMax);
            lastGrDb = grMax;

            // ---- makeup, auto gain, mix with the aligned dry, output
            autoGainDb = p.autoGain ? (float) juce::jlimit (-24.0, 24.0, -avgGr * 0.9) : 0.0f;
            const double makeup = dbToGain (p.makeupDb + autoGainDb), out = dbToGain (p.outputDb), m = juce::jlimit (0.0, 1.0, p.mix);
            for (int s = 0; s < n; ++s)
            {
                const int rd = ((dryW - n + s - totalLat) % dryLen + 2 * dryLen) % dryLen;
                for (int c = 0; c < (stereo ? 2 : 1); ++c)
                {
                    const double d = dry[(size_t) c][(size_t) rd], w = ch[c][s] * makeup;
                    double y = p.audition ? ch[c][s] : d + m * (w - d);
                    y *= out;
                    ch[c][s] = (float) (std::isfinite (y) ? y : 0.0);
                    outPk = std::max (outPk, (float) std::abs (y));
                }
                loud.process (ch[0][s], stereo ? ch[1][s] : ch[0][s]);
            }
            meterGrDb = grMax; meterInDb = (float) gainToDb (inPk); meterOutDb = (float) gainToDb (outPk); meterKeyDb = (float) gainToDb (keyPk);
            momentaryLufs = loud.getMomentary();
        }

    private:
        double fs = 48000; int block = 512;
        std::array<std::vector<float>, 2> look, dry; int lookW = 0, dryW = 0, dryLen = 1;
        double grDb[2] {}, env[2] {}, rms[2] {}, slowRel[2] {}, holdLeft[2] {}, lastGain[2] { 1, 1 }, crestPk[2] {}, crestRm[2] {};
        double lastGrDb = 0;
        double avgIn = -60, avgGr = 0, pulseEnv = 0, ppq = 0, tempoBpm = 120; bool isPlaying = false;
        struct Trig { int at; float vel; };
        std::array<Trig, 256> trig {}; int nTrig = 0;
        std::array<std::array<SqSpectra::DigitalBand, 2>, 6> keyBands;
        std::array<SqSpectra::Band, 6> designedKey {}; bool keyDirty = true;
        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 6> os;
        Apex::Loudness loud;
        juce::AudioBuffer<float> scratch;
        std::vector<float> monoScratch;

        int clampStyle() const { return juce::jlimit (0, (int) styles().size() - 1, p.style); }
        void designKey()
        {
            for (int b = 0; b < 6; ++b)
                if (p.keyEq[(size_t) b].enabled)
                    for (int c = 0; c < 2; ++c) keyBands[(size_t) b][(size_t) c].design (SqSpectra::prototype (p.keyEq[(size_t) b], p.keyEq[(size_t) b].gainDb), fs, 0);
            designedKey = p.keyEq; keyDirty = false;
        }

        /** The colour stage: a waveshaper chosen by the style or the user, driven by Drive plus the style's
            gain-reduction-dependent drive, run at up to 32x so its harmonics don't fold back. Level-compensated. */
        void applyCharacter (float* const* ch, int numCh, int n, double grNow)
        {
            const auto& st = styles()[(size_t) clampStyle()];
            Colour c = p.characterMode == 1 ? st.colour : (Colour) juce::jlimit (0, 3, p.characterMode - 1);
            if (c == Colour::off) c = Colour::warm;
            const double driveDb = p.driveDb + st.colourByGr * grNow;
            if (driveDb < 0.01) return;
            const float drive = (float) dbToGain (driveDb), comp = 1.0f / shapeGain (c, drive);
            // mono: the second oversampler channel works on a scratch copy, never on the same buffer twice
            if (numCh < 2) monoScratch.assign (ch[0], ch[0] + n);
            float* chans[2] = { ch[0], numCh > 1 ? ch[1] : monoScratch.data() };
            juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
            auto& o = *os[(size_t) std::min (5, p.oversampleLog2)];
            if (p.oversampleLog2 <= 0) { for (int k = 0; k < 2; ++k) for (int s = 0; s < n; ++s) chans[k][s] = shape (c, chans[k][s] * drive) * comp; return; }
            auto up = o.processSamplesUp (blk);
            for (size_t k = 0; k < up.getNumChannels(); ++k)
            {
                auto* d = up.getChannelPointer (k);
                for (size_t s = 0; s < up.getNumSamples(); ++s) d[s] = shape (c, d[s] * drive) * comp;
            }
            o.processSamplesDown (blk);
        }
        static float shape (Colour c, float x)
        {
            switch (c)
            {
                case Colour::warm:   return std::tanh (x + 0.18f * x * x) - std::tanh (0.0f);                      // even + odd, asymmetric
                case Colour::bright: return x / std::sqrt (1.0f + x * x * 0.85f);                                  // odd, crisper top
                case Colour::iron:   { const float y = std::tanh (1.15f * x); return y + 0.06f * y * y - 0.06f * y * y * y; }   // thick low mids
                default: return x;
            }
        }
        /** Small-signal gain of the shaper at this drive (so colour doesn't change the level). */
        static float shapeGain (Colour c, float drive) { const float e = 1e-3f; return std::max (1e-3f, (shape (c, e * drive) - shape (c, -e * drive)) / (2 * e)); }
    };
}
