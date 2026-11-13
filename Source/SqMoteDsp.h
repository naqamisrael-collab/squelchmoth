#pragma once
// Mote - DSP core of the one-oscillator synth (1.14 "Studio Line", phase K). No UI, no te:: types: tests/mote_test.cpp
// measures it; SqMote.h puts it on the shared synth scaffolding (voices, MIDI, glide, unison, mod matrix).
//
// One voice:
//  * one oscillator whose Shape sweeps continuously sine -> triangle -> saw -> square, with a Width control that narrows
//    the square into a pulse (50 % .. 5 %). Every wave is band-limited per sample (polyBLEP for the steps, polyBLAMP for
//    the triangle's corners), so the shape can sweep at audio-rate modulation without zipper or aliasing.
//  * a sub oscillator one or two octaves down (sine or square), and noise with a colour control (white .. pink-ish).
//  * drive before the filter (tanh, scaled so a full-level oscillator keeps its peak: more harmonics, same level).
//  * the Fissure filter (zero-delay SVF cascades: low-pass / high-pass at 12, 24 or 48 dB/oct, band-pass, notch, comb,
//    vowel), cutoff driven by its own envelope (octaves), key tracking, velocity and the LFO.
//  * two envelopes with honest times: attack is a straight rise that reaches the top in exactly the attack time; decay and
//    release are exponential and cover 99 % of their distance in exactly the time set (-40 dB for the release).
//  * the LFO arrives from outside (one value per chunk) with a per-voice delay / fade-in.

#include "SqFissureDsp.h"

namespace SqMote
{
    using SqFissure::pi;
    inline const char* shapeName (float s)
    {
        static const char* n[] = { "Sine", "Triangle", "Saw", "Square" };
        return n[juce::jlimit (0, 3, (int) std::lround (s))];
    }
    inline juce::StringArray filterTypeNames() { return { "Low-Pass", "High-Pass", "Band-Pass", "Notch", "Comb", "Vowel" }; }
    inline int fissureType (int t) { static const int m[] = { SqFissure::lowPass, SqFissure::highPass, SqFissure::bandPass, SqFissure::notch, SqFissure::comb, SqFissure::vowel }; return m[juce::jlimit (0, 5, t)]; }

    //==========================================================================
    inline double polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }
    /** Integrated polyBLEP: smooths a corner (a jump in slope). */
    inline double polyBlamp (double t, double dt)
    {
        if (t < dt)       { t = t / dt - 1.0; return -1.0 / 3.0 * t * t * t; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt + 1.0; return 1.0 / 3.0 * t * t * t; }
        return 0.0;
    }
    inline double wrap01 (double t) { return t - std::floor (t); }

    /** The morphing oscillator: shape 0 sine, 1 triangle, 2 saw, 3 square (pulse by width); between them a crossfade of the
        two neighbours at the same phase. */
    struct Osc
    {
        double phase = 0, inc = 0;
        void set (double hz, double fs) { inc = juce::jlimit (0.0, 0.48, hz / fs); }
        static double sine (double t) { return std::sin (2 * pi * t); }
        static double tri (double t, double dt)
        {
            double v = 4.0 * std::abs (t - 0.5) - 1.0;           // +1 at 0, -1 at 0.5
            v -= 4.0 * dt * polyBlamp (t, dt);                    // slope +4 -> -4 at t = 0 (corner of 8 dt per sample; this cubic carries x2)
            v += 4.0 * dt * polyBlamp (wrap01 (t + 0.5), dt);     // slope -4 -> +4 at t = 0.5
            return v;
        }
        static double saw (double t, double dt) { return 2.0 * t - 1.0 - polyBlep (t, dt); }
        static double pulse (double t, double dt, double w)
        {
            double v = t < w ? 1.0 : -1.0;
            v += polyBlep (t, dt); v -= polyBlep (wrap01 (t + 1.0 - w), dt);
            return v - (2.0 * w - 1.0);                            // remove the DC of a narrow pulse
        }
        inline double value (double shape, double width) const
        {
            const double t = phase, dt = std::max (1e-7, inc);
            shape = juce::jlimit (0.0, 3.0, shape);
            const int a = std::min (2, (int) shape); const double f = shape - a;
            auto w = [&] (int k)
            {
                switch (k) { case 0: return sine (t); case 1: return tri (t, dt); case 2: return saw (t, dt); default: return pulse (t, dt, width); }
            };
            return f < 1e-6 ? w (a) : (f > 1.0 - 1e-6 ? w (a + 1) : w (a) * (1.0 - f) + w (a + 1) * f);
        }
        inline bool advance() { phase += inc; if (phase >= 1.0) { phase -= 1.0; return true; } return false; }
    };

    //==========================================================================
    struct Env
    {
        enum Stage { idle, attack, decay, sustain, release };
        Stage stage = idle; double level = 0, fs = 48000, aStep = 0, dCoef = 0, rCoef = 0, sus = 1;
        void prepare (double rate) { fs = rate; }
        void set (double aSec, double dSec, double s, double rSec)
        {
            aStep = 1.0 / std::max (1.0, aSec * fs);
            dCoef = std::exp (std::log (0.01) / std::max (1.0, dSec * fs));   // 99 % of the way in dSec
            rCoef = std::exp (std::log (0.01) / std::max (1.0, rSec * fs));
            sus = juce::jlimit (0.0, 1.0, s);
        }
        void noteOn()  { stage = attack; }
        void noteOff() { if (stage != idle) stage = release; }
        void kill()    { stage = idle; level = 0; }
        bool active() const { return stage != idle; }
        inline double next()
        {
            switch (stage)
            {
                case attack:  level += aStep; if (level >= 1.0) { level = 1.0; stage = decay; } break;
                case decay:   level = sus + (level - sus) * dCoef; if (level - sus < 1e-4) { level = sus; stage = sustain; } break;
                case sustain: level = sus; break;
                case release: level *= rCoef; if (level < 1e-5) { level = 0; stage = idle; } break;
                default: break;
            }
            return level;
        }
    };

    //==========================================================================
    /** Everything a voice reads, refreshed once per chunk by the plug-in. */
    struct Settings
    {
        double shape = 2, width = 0.5;                  // 0..3, 0.05..0.5
        double oscLevel = 1, subLevel = 0, noiseLevel = 0, noiseColour = 0; int subOct = 1, subWave = 0;   // subWave 0 sine 1 square
        double driveDb = 0;
        int filterType = 0, slope = 24; double cutoffHz = 2000, res = 0.15, envOct = 2, keyTrack = 0.5, velOct = 0;
        double aEnv[4] { 0.005, 0.3, 0.8, 0.25 }, fEnv[4] { 0.002, 0.35, 0.2, 0.3 };   // A D S R (seconds, sustain 0..1)
        double ampVel = 0.6, gain = 0.5;
        double lfoPitch = 0, lfoCutOct = 0, lfoShape = 0, lfoWidth = 0, lfoAmp = 0, lfoDelay = 0;   // semitones, octaves, shape units, width, 0..1, seconds
    };
    /** Per-voice modulation offsets from the matrix (already scaled to units). */
    struct Mods { double pitch = 0, cutOct = 0, res = 0, shape = 0, width = 0, amp = 0; };

    struct Voice
    {
        Osc osc, sub; Env amp, flt; SqFissure::SlotState filt; double fs = 48000;
        double noiseZ = 0, sinceOn = 0; unsigned rng = 0x2545F491u;
        float lastAmp = 0, lastFlt = 0, lastCut = 1000;

        void prepare (double rate) { fs = rate; amp.prepare (fs); flt.prepare (fs); filt.prepare (fs); }
        void start (bool retriggerPhase)
        {
            if (retriggerPhase) { osc.phase = 0; sub.phase = 0; }
            if (! amp.active()) { filt.reset(); noiseZ = 0; }
            amp.noteOn(); flt.noteOn(); sinceOn = 0;
        }
        void release() { amp.noteOff(); flt.noteOff(); }
        void kill() { amp.kill(); flt.kill(); }
        bool active() const { return amp.active(); }

        /** Adds n samples to l / r at frequency hz. lfo: -1..1 for this chunk; vel 0..1; note for key tracking. */
        void render (float* l, float* r, int n, double hz, const Settings& s, const Mods& m, double lfo, double vel, int note, double panL, double panR)
        {
            amp.set (s.aEnv[0], s.aEnv[1], s.aEnv[2], s.aEnv[3]); flt.set (s.fEnv[0], s.fEnv[1], s.fEnv[2], s.fEnv[3]);
            const double fade = s.lfoDelay > 0.001 ? juce::jlimit (0.0, 1.0, sinceOn / s.lfoDelay) : 1.0;
            const double lv = lfo * fade;
            const double f = hz * std::pow (2.0, (m.pitch + s.lfoPitch * lv) / 12.0);
            osc.set (f, fs); sub.set (f / (s.subOct >= 2 ? 4.0 : 2.0), fs);
            const double shape = juce::jlimit (0.0, 3.0, s.shape + m.shape + s.lfoShape * lv);
            const double width = juce::jlimit (0.05, 0.5, s.width + m.width + s.lfoWidth * lv * 0.45);
            const double driveG = SqFissure::dbToGain (s.driveDb), driveN = s.driveDb > 0.01 ? 0.5 / std::tanh (0.5 * driveG) : 1.0;   // a full-level oscillator (+-0.5 here) keeps its peak
            const double velG = 1.0 - s.ampVel + s.ampVel * vel;
            const double ampMod = juce::jlimit (0.0, 2.0, 1.0 + m.amp - s.lfoAmp * 0.5 * (1.0 - lv));
            SqFissure::Slot slot; slot.type = fissureType (s.filterType); slot.slope = s.slope;
            const double nc = juce::jlimit (0.0, 0.99, s.noiseColour) ;
            const int sub8 = 8;
            for (int i0 = 0; i0 < n; i0 += sub8)
            {
                // filter coefficients every 8 samples (the SVF follows fast sweeps cleanly)
                const double fe = flt.level;
                const double oct = s.envOct * fe + s.keyTrack * (note - 60) / 12.0 + s.velOct * (vel - 0.5) * 2.0 + m.cutOct + s.lfoCutOct * lv;
                const double fc = juce::jlimit (20.0, std::min (20000.0, fs * 0.45), s.cutoffHz * std::pow (2.0, oct));
                filt.set (slot, fs, fc, juce::jlimit (0.0, 1.0, s.res + m.res)); lastCut = (float) fc;
                const int e = std::min (n, i0 + sub8);
                for (int i = i0; i < e; ++i)
                {
                    double x = s.oscLevel * osc.value (shape, width);
                    if (s.subLevel > 1e-4) x += s.subLevel * (s.subWave == 0 ? Osc::sine (sub.phase) : Osc::pulse (sub.phase, std::max (1e-7, sub.inc), 0.5));
                    if (s.noiseLevel > 1e-4)
                    {
                        rng = rng * 1664525u + 1013904223u; const double w = (double) (rng >> 8) / 8388608.0 - 1.0;
                        noiseZ = noiseZ * nc + w * (1.0 - nc);                       // colour: a gentle low-pass tilt
                        x += s.noiseLevel * noiseZ * (1.0 + nc * 2.0);
                    }
                    osc.advance(); sub.advance();
                    x *= 0.5;
                    if (s.driveDb > 0.01) x = std::tanh (x * driveG) * driveN;
                    const double y = filt.process (slot, x);
                    const double a = amp.next() * velG * ampMod * s.gain;
                    flt.next();
                    const double o = y * a;
                    l[i] += (float) (o * panL); r[i] += (float) (o * panR);
                }
            }
            sinceOn += n / fs;
            lastAmp = (float) amp.level; lastFlt = (float) flt.level;
        }
    };
}
