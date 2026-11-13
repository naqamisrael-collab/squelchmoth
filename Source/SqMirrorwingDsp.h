#pragma once
// Mirrorwing - DSP core of the flagship synth (1.14 "Studio Line", phase K). No UI, no te:: types:
// tests/mirrorwing_test.cpp measures it; SqMirrorwing.h puts it on the shared synth scaffolding (MIDI, voices, glide).
//
// One voice:
//  * four oscillators, each: the Mote morphing shape (sine -> triangle -> saw -> square, width to a pulse, band-limited),
//    a wavefolder, octave / semitone / fine, level, pan, its own unison (1-8 copies with detune and stereo spread, level
//    kept constant), phase (fixed or free), phase modulation from the oscillator before it (FM), hard sync to osc 1, and a
//    route into filter 1, filter 2 or both.
//  * noise with colour.
//  * two filters (the Fissure filter: seven types, 12 / 24 / 48 dB/oct, resonance, drive, gain, key tracking), run in
//    series (1 -> 2, anything routed to 2 joins after 1) or in parallel.
//  * four DAHDSR envelopes (delay, attack, hold, decay, sustain, release - attack straight, decay and release exponential
//    over 99 % of their distance in exactly the time set); envelope 1 is the amp.
//  * four LFOs per voice (sine, triangle, saw, square, sample & hold, smooth random), retriggered per note or free,
//    with a start phase and a fade-in.
//  * a 16-slot matrix: any source (4 envelopes, 4 LFOs, velocity, key, mod wheel, aftertouch, per-note random, 4 macros)
//    to any target (pitch, every oscillator's pitch / shape / width / fold / level / pan / FM, every filter's cutoff in
//    octaves / resonance / drive, amp, noise), bipolar amount. Evaluated every 8 samples.

#include "SqMoteDsp.h"

namespace SqMirror
{
    using SqFissure::pi;
    constexpr int numOsc = 4, numFilters = 2, numEnv = 4, numLfo = 4, numSlots = 16, maxUnison = 8, numMacros = 4;

    // ---- matrix sources
    enum Src { sOff = 0, sEnv1, sEnv2, sEnv3, sEnv4, sLfo1, sLfo2, sLfo3, sLfo4, sVelocity, sKey, sWheel, sAftertouch, sRandom, sMacro1, sMacro2, sMacro3, sMacro4, numSources };
    inline juce::StringArray sourceNames()
    { return { "Off", "Env 1", "Env 2", "Env 3", "Env 4", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Velocity", "Key", "Mod Wheel", "Aftertouch", "Random", "Macro 1", "Macro 2", "Macro 3", "Macro 4" }; }

    // ---- matrix targets: 0 none, 1 pitch, then 7 per oscillator, 3 per filter, amp, noise
    enum OscT { tPitch = 0, tShape, tWidth, tFold, tLevel, tPan, tFm, perOscT };
    enum FilT { tCut = 0, tRes, tDrive, perFilT };
    constexpr int tGlobalPitch = 1, tOscBase = 2, tFilBase = tOscBase + numOsc * perOscT, tAmp = tFilBase + numFilters * perFilT, tNoise = tAmp + 1, numTargets = tNoise + 1;
    inline juce::String targetName (int t)
    {
        static const char* on[] = { "Pitch", "Shape", "Width", "Fold", "Level", "Pan", "FM" }, * fn[] = { "Cutoff", "Resonance", "Drive" };
        if (t == tGlobalPitch) return "Pitch (all)";
        if (t >= tOscBase && t < tFilBase) return "Osc " + juce::String ((t - tOscBase) / perOscT + 1) + " " + on[(t - tOscBase) % perOscT];
        if (t >= tFilBase && t < tAmp) return "Filter " + juce::String ((t - tFilBase) / perFilT + 1) + " " + fn[(t - tFilBase) % perFilT];
        if (t == tAmp) return "Amp"; if (t == tNoise) return "Noise";
        return "None";
    }
    /** What an amount of 1.0 means at each target, and its unit (for the face and Claude). */
    inline double targetScale (int t)
    {
        if (t == tGlobalPitch) return 24.0;
        if (t >= tOscBase && t < tFilBase) { static const double s[] = { 24.0, 3.0, 0.45, 1.0, 1.0, 1.0, 1.0 }; return s[(t - tOscBase) % perOscT]; }
        if (t >= tFilBase && t < tAmp) { static const double s[] = { 6.0, 1.0, 24.0 }; return s[(t - tFilBase) % perFilT]; }
        return 1.0;
    }
    inline const char* targetUnit (int t)
    {
        if (t == tGlobalPitch) return "st";
        if (t >= tOscBase && t < tFilBase) { static const char* u[] = { "st", "", "", "", "", "", "" }; return u[(t - tOscBase) % perOscT]; }
        if (t >= tFilBase && t < tAmp) { static const char* u[] = { "oct", "", "dB" }; return u[(t - tFilBase) % perFilT]; }
        return "";
    }

    enum LfoShape { lSine = 0, lTriangle, lSaw, lSquare, lSampleHold, lSmooth, numLfoShapes };
    inline juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth" }; }

    struct OscSet
    {
        bool on = true, sync = false, freePhase = false; int route = 0, unison = 1;   // route 0 filter 1, 1 filter 2, 2 both
        double shape = 2, width = 0.5, fold = 0, semis = 0, cents = 0, level = 1, pan = 0, uniDetune = 12, uniSpread = 0.7, fm = 0, phase = 0;
    };
    struct FilterSet { bool on = true; int type = SqFissure::lowPass, slope = 24; double cutoffHz = 2000, res = 0.15, driveDb = 0, gainDb = 0, keyTrack = 0.5; };
    struct EnvSet { double delay = 0, attack = 0.005, hold = 0, decay = 0.3, sustain = 0.8, release = 0.3; };
    struct LfoSet { int shape = lSine; bool retrig = true; double hz = 2, startPhase = 0, fade = 0; };
    struct Slot { int src = sOff, dst = 0; double amount = 0; };
    struct Settings
    {
        std::array<OscSet, numOsc> osc {}; std::array<FilterSet, numFilters> filter {}; std::array<EnvSet, numEnv> env {}; std::array<LfoSet, numLfo> lfo {};
        std::array<Slot, numSlots> slot {};
        bool parallel = false; double noise = 0, noiseColour = 0, ampVel = 0.6, gain = 0.5;
        std::array<double, numMacros> macro {};
        double wheel = 0, aftertouch = 0;
        /** Free-running LFO phases (the plug-in advances them in song time), used when an LFO is not retriggered. */
        std::array<double, numLfo> freePhase {};
    };

    //==========================================================================
    struct Env
    {
        enum Stage { idle, delay, attack, hold, decay, sustain, release };
        Stage stage = idle; double level = 0, fs = 48000, t = 0, aStep = 0, dCoef = 0, rCoef = 0, sus = 1, dl = 0, hl = 0;
        void prepare (double r) { fs = r; }
        void set (const EnvSet& e)
        {
            dl = e.delay * fs; hl = e.hold * fs; aStep = 1.0 / std::max (1.0, e.attack * fs);
            dCoef = std::exp (std::log (0.01) / std::max (1.0, e.decay * fs)); rCoef = std::exp (std::log (0.01) / std::max (1.0, e.release * fs));
            sus = juce::jlimit (0.0, 1.0, e.sustain);
        }
        void noteOn() { stage = delay; t = 0; }
        void noteOff() { if (stage != idle) stage = release; }
        void kill() { stage = idle; level = 0; }
        bool active() const { return stage != idle; }
        inline double next()
        {
            switch (stage)
            {
                case delay:   if (++t > dl) { stage = attack; t = 0; } break;
                case attack:  level += aStep; if (level >= 1.0) { level = 1.0; stage = hold; t = 0; } break;
                case hold:    if (++t > hl) stage = decay; break;
                case decay:   level = sus + (level - sus) * dCoef; if (level - sus < 1e-4) { level = sus; stage = sustain; } break;
                case sustain: level = sus; break;
                case release: level *= rCoef; if (level < 1e-5) { level = 0; stage = idle; } break;
                default: break;
            }
            return level;
        }
    };

    struct Lfo
    {
        double ph = 0, held = 0, from = 0, to = 0, sinceOn = 0; unsigned rng = 0x51ed27u;
        double rnd() { rng = rng * 1664525u + 1013904223u; return (double) (rng >> 8) / 8388608.0 - 1.0; }
        void start (const LfoSet& s, double freePh, unsigned seed) { rng ^= seed * 2654435761u; ph = s.retrig ? s.startPhase : freePh; held = rnd(); from = held; to = rnd(); sinceOn = 0; }
        /** Advances by n samples and returns the value (-1..1), faded in. */
        double step (const LfoSet& s, double fs, int n)
        {
            ph += s.hz * n / fs; sinceOn += n / fs;
            while (ph >= 1.0) { ph -= 1.0; held = rnd(); from = to; to = rnd(); }
            double v;
            switch (s.shape)
            {
                case lTriangle:   v = 1.0 - 4.0 * std::abs (ph - 0.5); break;
                case lSaw:        v = 2.0 * ph - 1.0; break;
                case lSquare:     v = ph < 0.5 ? 1.0 : -1.0; break;
                case lSampleHold: v = held; break;
                case lSmooth:     { const double k = 0.5 - 0.5 * std::cos (pi * ph); v = from + (to - from) * k; break; }
                default:          v = std::sin (2 * pi * ph);
            }
            return s.fade > 0.001 ? v * juce::jlimit (0.0, 1.0, sinceOn / s.fade) : v;
        }
    };

    //==========================================================================
    struct Voice
    {
        double fs = 48000;
        std::array<std::array<double, maxUnison>, numOsc> ph {};
        std::array<double, numOsc> lastOut {};
        std::array<Env, numEnv> env; std::array<Lfo, numLfo> lfo;
        std::array<std::array<SqFissure::SlotState, numFilters>, 2> filt;   // [channel][filter]
        double noiseZ = 0, randomVal = 0; unsigned rng = 0x9E3779B9u;
        // live values for the face
        std::array<float, numEnv> envNow {}; std::array<float, numLfo> lfoNow {}; std::array<float, numFilters> cutNow {};
        std::array<double, numTargets> mod {};

        void prepare (double r) { fs = r; for (auto& e : env) e.prepare (fs); for (auto& c : filt) for (auto& f : c) f.prepare (fs); }
        void start (const Settings& s, double velocityRandomSeed, bool fresh)
        {
            rng ^= (unsigned) (velocityRandomSeed * 1.0e6) + 12345u;
            rng = rng * 1664525u + 1013904223u; randomVal = (double) (rng >> 8) / 8388608.0 - 1.0;
            for (int k = 0; k < numOsc; ++k)
                for (int u = 0; u < maxUnison; ++u)
                {
                    if (s.osc[(size_t) k].freePhase) { rng = rng * 1664525u + 1013904223u; ph[(size_t) k][(size_t) u] = (double) (rng >> 8) / 16777216.0; }
                    else if (fresh) ph[(size_t) k][(size_t) u] = std::fmod (s.osc[(size_t) k].phase + 0.137 * u, 1.0);   // unison copies start apart
                }
            if (fresh) { for (auto& c : filt) for (auto& f : c) f.reset(); noiseZ = 0; lastOut.fill (0); }
            for (int l = 0; l < numLfo; ++l) lfo[(size_t) l].start (s.lfo[(size_t) l], s.freePhase[(size_t) l], rng);
            for (auto& e : env) e.noteOn();
        }
        void release() { for (auto& e : env) e.noteOff(); }
        void kill() { for (auto& e : env) e.kill(); }
        bool active() const { return env[0].active(); }

        static double fold (double x, double amt)
        {
            if (amt <= 1e-4) return x;
            const double g = 1.0 + amt * 5.0;
            return x * (1.0 - amt) + std::sin (0.5 * pi * x * g) * amt;   // blends into a sine folder: stays within +-1
        }

        /** Adds n samples (stereo) to l / r. hz: the note frequency (glide and bend applied); vel 0..1; note for key tracking. */
        void render (float* l, float* r, int n, double hz, const Settings& s, double vel, int note)
        {
            for (int e = 0; e < numEnv; ++e) env[(size_t) e].set (s.env[(size_t) e]);
            const double velG = 1.0 - s.ampVel + s.ampVel * vel;
            const double nc = juce::jlimit (0.0, 0.99, s.noiseColour);
            for (int i0 = 0; i0 < n; i0 += 8)
            {
                const int len = std::min (8, n - i0);
                // ---- sources, then the matrix
                double src[numSources] {};
                for (int e = 0; e < numEnv; ++e) src[sEnv1 + e] = env[(size_t) e].level;
                for (int q = 0; q < numLfo; ++q) { const double v = lfo[(size_t) q].step (s.lfo[(size_t) q], fs, len); src[sLfo1 + q] = v; lfoNow[(size_t) q] = (float) v; }
                src[sVelocity] = vel; src[sKey] = (note - 60) / 60.0; src[sWheel] = s.wheel; src[sAftertouch] = s.aftertouch; src[sRandom] = randomVal;
                for (int m = 0; m < numMacros; ++m) src[sMacro1 + m] = s.macro[(size_t) m];
                mod.fill (0.0);
                for (auto& sl : s.slot) if (sl.src > 0 && sl.src < numSources && sl.dst > 0 && sl.dst < numTargets) mod[(size_t) sl.dst] += src[sl.src] * sl.amount * targetScale (sl.dst);
                // ---- filters (per block)
                for (int f = 0; f < numFilters; ++f)
                {
                    const auto& fsS = s.filter[(size_t) f]; const int b = tFilBase + f * perFilT;
                    const double fc = juce::jlimit (20.0, std::min (20000.0, fs * 0.45), fsS.cutoffHz * std::pow (2.0, mod[(size_t) (b + tCut)] + fsS.keyTrack * (note - 60) / 12.0));
                    SqFissure::Slot sl; sl.type = fsS.type; sl.slope = fsS.slope; sl.gainDb = fsS.gainDb;
                    for (int c = 0; c < 2; ++c) filt[(size_t) c][(size_t) f].set (sl, fs, fc, juce::jlimit (0.0, 1.0, fsS.res + mod[(size_t) (b + tRes)]));
                    cutNow[(size_t) f] = (float) fc;
                }
                // ---- per-oscillator block values
                struct OB { bool on; double inc[maxUnison]; double pan[maxUnison][2]; int uni; double shape, width, fold, level, fm; };
                OB ob[numOsc];
                for (int k = 0; k < numOsc; ++k)
                {
                    const auto& o = s.osc[(size_t) k]; const int b = tOscBase + k * perOscT; auto& x = ob[k];
                    x.on = o.on;   // an oscillator at level 0 still runs: it may be an FM modulator or the sync master
                    if (! x.on) continue;
                    x.uni = juce::jlimit (1, maxUnison, o.unison);
                    const double semis = o.semis + o.cents / 100.0 + mod[(size_t) tGlobalPitch] + mod[(size_t) (b + tPitch)];
                    const double base = hz * std::pow (2.0, semis / 12.0);
                    const double panC = juce::jlimit (-1.0, 1.0, o.pan + mod[(size_t) (b + tPan)]);
                    const double g = 1.0 / std::sqrt ((double) x.uni);
                    for (int u = 0; u < x.uni; ++u)
                    {
                        const double spread = x.uni > 1 ? (2.0 * u / (x.uni - 1) - 1.0) : 0.0;
                        x.inc[u] = juce::jlimit (0.0, 0.48, base * std::pow (2.0, spread * o.uniDetune / 1200.0) / fs);
                        const double pp = juce::jlimit (-1.0, 1.0, panC + spread * o.uniSpread), a = (pp + 1.0) * 0.25 * pi;
                        x.pan[u][0] = std::cos (a) * 1.41421356 * g; x.pan[u][1] = std::sin (a) * 1.41421356 * g;
                    }
                    x.shape = juce::jlimit (0.0, 3.0, o.shape + mod[(size_t) (b + tShape)]); x.width = juce::jlimit (0.05, 0.5, o.width + mod[(size_t) (b + tWidth)]);
                    x.fold = juce::jlimit (0.0, 1.0, o.fold + mod[(size_t) (b + tFold)]); x.level = juce::jlimit (0.0, 2.0, o.level + mod[(size_t) (b + tLevel)]);
                    x.fm = juce::jlimit (0.0, 2.0, o.fm + mod[(size_t) (b + tFm)]);
                }
                const double noiseL = juce::jlimit (0.0, 1.0, s.noise + mod[(size_t) tNoise]);
                const double ampMod = juce::jlimit (0.0, 2.0, 1.0 + mod[(size_t) tAmp]);
                SqFissure::Slot fsl[numFilters];
                for (int f = 0; f < numFilters; ++f) { fsl[f].type = s.filter[(size_t) f].type; fsl[f].slope = s.filter[(size_t) f].slope; fsl[f].gainDb = s.filter[(size_t) f].gainDb; }
                const double drv[numFilters] = { SqFissure::dbToGain (s.filter[0].driveDb + mod[(size_t) (tFilBase + tDrive)]), SqFissure::dbToGain (s.filter[1].driveDb + mod[(size_t) (tFilBase + perFilT + tDrive)]) };

                // ---- samples
                for (int i = i0; i < i0 + len; ++i)
                {
                    double bus[2][2] = { { 0, 0 }, { 0, 0 } };   // [filter][channel]
                    bool wrapped0 = false;
                    for (int k = 0; k < numOsc; ++k)
                    {
                        if (! ob[k].on) { lastOut[(size_t) k] = 0; continue; }
                        const auto& x = ob[k]; const auto& o = s.osc[(size_t) k];
                        const double pm = k > 0 && x.fm > 1e-4 ? x.fm * lastOut[(size_t) k - 1] * 0.5 : 0.0;
                        double sumL = 0, sumR = 0, mono = 0;
                        for (int u = 0; u < x.uni; ++u)
                        {
                            double& p = ph[(size_t) k][(size_t) u];
                            SqMote::Osc w; w.phase = SqMote::wrap01 (p + pm); w.inc = x.inc[u];
                            const double v = fold (w.value (x.shape, x.width), x.fold) * x.level;
                            sumL += v * x.pan[u][0]; sumR += v * x.pan[u][1]; if (u == 0) mono = v;
                            p += x.inc[u];
                            if (p >= 1.0) { p -= 1.0; if (k == 0 && u == 0) wrapped0 = true; }
                        }
                        if (k > 0 && o.sync && wrapped0) for (int u = 0; u < x.uni; ++u) ph[(size_t) k][(size_t) u] = 0.0;
                        lastOut[(size_t) k] = mono;
                        if (o.route == 0 || o.route == 2) { bus[0][0] += sumL; bus[0][1] += sumR; }
                        if (o.route == 1 || o.route == 2) { bus[1][0] += sumL; bus[1][1] += sumR; }
                    }
                    if (noiseL > 1e-4)
                    {
                        rng = rng * 1664525u + 1013904223u; const double w = (double) (rng >> 8) / 8388608.0 - 1.0;
                        noiseZ = noiseZ * nc + w * (1.0 - nc); const double v = noiseL * noiseZ * (1.0 + nc * 2.0);
                        bus[0][0] += v; bus[0][1] += v;
                    }
                    double out[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        auto run = [&] (int f, double xin)
                        {
                            if (! s.filter[(size_t) f].on) return xin;
                            const double d = drv[f] > 1.0001 ? std::tanh (xin * drv[f]) / std::tanh (drv[f]) : xin;   // a full-scale peak stays put
                            return filt[(size_t) c][(size_t) f].process (fsl[f], d);
                        };
                        if (s.parallel) out[c] = run (0, bus[0][c]) + run (1, bus[1][c]);
                        else            out[c] = run (1, run (0, bus[0][c]) + bus[1][c]);
                    }
                    double a = env[0].next(); for (int e = 1; e < numEnv; ++e) env[(size_t) e].next();
                    a *= velG * ampMod * s.gain;
                    l[i] += (float) (out[0] * a * 0.5); r[i] += (float) (out[1] * a * 0.5);
                }
            }
            for (int e = 0; e < numEnv; ++e) envNow[(size_t) e] = (float) env[(size_t) e].level;
        }
    };
}
