#pragma once
// Squelchmoth synth core - the shared engine behind the 1.8 instrument shelf (Venom, Mini Moth, Sim Moth, Dial 10,
// Thump, Mothsynth 2 ...). Everything here is original DSP:
//
//  * PolyBLEP oscillators (saw / square with PWM / triangle / sine / noise) with hard sync and a sub, band-limited so a
//    saw at the top of the keyboard has no audible aliasing; an optional 2x "HD" path for the nonlinear stages.
//  * Zero-delay-feedback filters: the SVF from SqDsp (LP / HP / BP / notch, 12 or 24 dB) and a 4-pole ladder with
//    resonance compensation and drive.
//  * ADSR envelopes with analogue-style curves, LFOs (sine / tri / saw / square / S&H / drift, free or tempo-synced),
//    a mod matrix (sources: envelopes, LFOs, velocity, key, mod wheel, aftertouch, Mod X / Mod Y (CC 74 / 71), macros)
//    and four macros per instrument.
//  * A voice manager: poly with voice stealing, mono, legato, glide, unison (up to 8 voices with detune and spread).
//  * An FX rack: drive, chorus, delay (tempo-synced), reverb, tilt EQ.
//  * SynthPluginBase: the te::Plugin scaffolding - parameters (SqDsp::ParamBank), sample-accurate MIDI, chunked rendering,
//    silence detection (no CPU while idle), Eco / HD quality, A/B, and everything exposed to automation and Claude.

#include "SqInstruments.h"

namespace SqSynth
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    //==========================================================================
    // Oscillators
    enum class Wave { sine = 0, triangle, saw, square, pulse, noise, table, numWaves };
    inline juce::StringArray waveNames()   { return { "SINE", "TRI", "SAW", "SQUARE", "PULSE", "NOISE", "TABLE" }; }

    /** A bank of 8 band-limited wavetables (2048 samples, mip-mapped per octave) built additively, for Lantern Exo
        and anything that wants a morphing oscillator. Tables: Sine, Fold, Saw-Tri, Vocal, Organ, Bright Pulse, Glass, Digital. */
    struct WaveTableBank
    {
        static constexpr int numTables = 8, size = 2048, mips = 9;
        std::vector<float> data;   // [table][mip][size]
        static juce::StringArray names()   { return { "Sine", "Fold", "Saw-Tri", "Vocal", "Organ", "Bright", "Glass", "Digital" }; }
        static const WaveTableBank& get()  { static WaveTableBank b; return b; }
        WaveTableBank()
        {
            data.assign ((size_t) numTables * mips * size, 0.0f);
            juce::Random rnd (1234);
            float digital[64]; for (auto& d : digital) d = rnd.nextFloat();
            for (int t = 0; t < numTables; ++t)
                for (int m = 0; m < mips; ++m)
                {
                    const int maxHarm = juce::jmax (1, 256 >> m);
                    float* out = &data[(size_t) ((t * mips + m) * size)];
                    double peak = 0;
                    for (int h = 1; h <= juce::jmin (maxHarm, 128); ++h)
                    {
                        const float a = harmonic (t, h, digital);
                        if (std::abs (a) < 1.0e-5f) continue;
                        for (int i = 0; i < size; ++i) out[i] += a * std::sin (twoPi * (float) h * (float) i / (float) size);
                    }
                    for (int i = 0; i < size; ++i) peak = juce::jmax (peak, (double) std::abs (out[i]));
                    if (peak > 0) for (int i = 0; i < size; ++i) out[i] = (float) (out[i] / peak);
                }
        }
        static float harmonic (int table, int h, const float* digital)
        {
            switch (table)
            {
                case 0: return h == 1 ? 1.0f : 0.0f;
                case 1: return (h % 2 == 1) ? (1.0f / (float) h) * (1.0f + 0.6f * std::sin ((float) h * 0.7f)) : 0.0f;      // folded odd harmonics
                case 2: return (1.0f / (float) h) * (h % 2 == 1 ? 1.0f : 0.35f);                                            // between saw and triangle
                case 3: { const float f1 = std::exp (-std::pow ((float) (h - 3), 2.0f) / 3.0f), f2 = std::exp (-std::pow ((float) (h - 9), 2.0f) / 6.0f); return (f1 + 0.6f * f2) / (float) std::sqrt ((float) h); }   // two formants
                case 4: return (h == 1 || h == 2 || h == 3 || h == 4 || h == 6 || h == 8) ? (h == 1 ? 1.0f : 0.5f) : 0.0f;   // drawbars
                case 5: return (1.0f / std::sqrt ((float) h)) * (h % 3 == 0 ? 0.2f : 1.0f) * (h < 40 ? 1.0f : 0.0f);         // bright pulse-ish
                case 6: return (h == 1 || h == 3 || h == 7 || h == 11 || h == 13) ? 1.0f / (0.6f * (float) h) : 0.0f;       // glassy partials
                default: return h <= 64 ? digital[h - 1] * digital[h - 1] / std::sqrt ((float) h) : 0.0f;                 // digital noise spectrum
            }
        }
        /** Reads a table at phase (0..1) with linear interpolation; mip picked from the increment. */
        inline float read (int table, double phase, double inc) const
        {
            int m = 0; double f = inc * 256.0; while (f > 1.0 && m < mips - 1) { f *= 0.5; ++m; }   // more harmonics than fit -> coarser mip
            const float* tb = &data[(size_t) ((juce::jlimit (0, numTables - 1, table) * mips + m) * size)];
            const double p = (phase - std::floor (phase)) * size; const int i = (int) p; const float fr = (float) (p - i);
            return tb[i] + (tb[(i + 1) & (size - 1)] - tb[i]) * fr;
        }
        /** Morphs between neighbouring tables: morph 0..1 spans all eight. */
        inline float readMorph (float morph, double phase, double inc) const
        {
            const float pos = juce::jlimit (0.0f, 1.0f, morph) * (numTables - 1); const int a = (int) pos; const float fr = pos - (float) a;
            const float va = read (a, phase, inc);
            return fr < 0.001f ? va : va + (read (a + 1, phase, inc) - va) * fr;
        }
    };

    struct Osc
    {
        double phase = 0.0, inc = 0.0;
        float lastTri = 0.0f;        // for the integrated-square triangle
        juce::uint32 rng = 0x9e3779b9u;
        float morph = 0.0f;          // wavetable position when the wave is Wave::table

        void reset (double ph = 0.0)   { phase = ph; lastTri = 0.0f; }
        void setFrequency (double hz, double sr)   { inc = juce::jlimit (0.0, 0.49, hz / sr); }

        static inline float polyBlep (double t, double dt)
        {
            if (t < dt)        { t /= dt; return (float) (t + t - t * t - 1.0); }
            if (t > 1.0 - dt)  { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
            return 0.0f;
        }

        inline float whiteNoise()
        {
            rng = rng * 1664525u + 1013904223u;
            return (float) ((rng >> 8) & 0xffffff) / 8388608.0f - 1.0f;
        }

        /** Advances the phase; returns true when it wrapped (used for hard sync). */
        inline bool advance()
        {
            phase += inc;
            if (phase >= 1.0) { phase -= 1.0; return true; }
            return false;
        }

        /** The sample for the current phase (call before advance()). pw 0.05..0.95 for pulse. */
        inline float value (Wave w, float pw = 0.5f)
        {
            const double t = phase, dt = juce::jmax (1.0e-6, inc);
            switch (w)
            {
                case Wave::sine:     return std::sin ((float) t * twoPi);
                case Wave::saw:      return (float) (2.0 * t - 1.0) - polyBlep (t, dt);
                case Wave::square:
                case Wave::pulse:
                {
                    const double width = w == Wave::square ? 0.5 : juce::jlimit (0.05, 0.95, (double) pw);
                    float v = t < width ? 1.0f : -1.0f;
                    v += polyBlep (t, dt);
                    double t2 = t + (1.0 - width); if (t2 >= 1.0) t2 -= 1.0;
                    v -= polyBlep (t2, dt);
                    return v;
                }
                case Wave::triangle:
                {
                    // leaky integration of a BLEP square - a band-limited triangle
                    float sq = t < 0.5 ? 1.0f : -1.0f;
                    sq += polyBlep (t, dt);
                    double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
                    sq -= polyBlep (t2, dt);
                    lastTri = lastTri * 0.9995f + (float) (4.0 * inc) * sq;
                    return juce::jlimit (-1.0f, 1.0f, lastTri);
                }
                case Wave::table:    return WaveTableBank::get().readMorph (morph, t, dt);
                case Wave::noise: default: return whiteNoise();
            }
        }
    };

    //==========================================================================
    // Envelopes
    struct ADSR
    {
        enum Stage { idle, attack, decay, sustain, release };
        Stage stage = idle;
        float level = 0.0f, a = 0.01f, d = 0.2f, s = 0.7f, r = 0.2f;
        double sr = 48000.0;
        float aCoef = 0, dCoef = 0, rCoef = 0, aTarget = 1.3f;

        void prepare (double rate)   { sr = rate; }
        void set (float attack, float decay, float sustain, float rel)
        {
            a = juce::jmax (0.0005f, attack); d = juce::jmax (0.001f, decay); s = juce::jlimit (0.0f, 1.0f, sustain); r = juce::jmax (0.001f, rel);
            // analogue-style: attack is a fast exponential aimed above 1, decay / release exponential towards target
            aCoef = 1.0f - std::exp (-1.0f / (float) (a * sr) * 2.2f);
            dCoef = 1.0f - std::exp (-1.0f / (float) (d * sr) * 3.5f);
            rCoef = 1.0f - std::exp (-1.0f / (float) (r * sr) * 3.5f);
        }
        void noteOn()    { stage = attack; }
        void noteOff()   { if (stage != idle) stage = release; }
        void kill()      { stage = idle; level = 0.0f; }
        bool isActive() const   { return stage != idle; }

        inline float next()
        {
            switch (stage)
            {
                case attack:  level += (aTarget - level) * aCoef; if (level >= 1.0f) { level = 1.0f; stage = decay; } break;
                case decay:   level += (s - level) * dCoef; if (level <= s + 0.0005f) { level = s; stage = sustain; } break;
                case sustain: level = s; break;
                case release: level += (0.0f - level) * rCoef; if (level <= 0.0002f) { level = 0.0f; stage = idle; } break;
                default: break;
            }
            return level;
        }
    };

    //==========================================================================
    // LFO
    enum class LfoShape { sine = 0, triangle, saw, square, sampleHold, drift, numShapes };
    inline juce::StringArray lfoShapeNames()   { return { "SINE", "TRI", "SAW", "SQUARE", "S&H", "DRIFT" }; }

    struct LFO
    {
        double phase = 0.0, inc = 0.0; float held = 0.0f, driftTarget = 0.0f, driftVal = 0.0f; juce::uint32 rng = 0x1234567u;
        void setRate (double hz, double sr)   { inc = juce::jlimit (0.0, 0.5, hz / sr); }
        void reset()   { phase = 0.0; }
        inline float next (LfoShape s)
        {
            const double t = phase;
            phase += inc; bool wrapped = false; if (phase >= 1.0) { phase -= 1.0; wrapped = true; }
            switch (s)
            {
                case LfoShape::sine:       return std::sin ((float) t * twoPi);
                case LfoShape::triangle:   return (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t);
                case LfoShape::saw:        return (float) (1.0 - 2.0 * t);
                case LfoShape::square:     return t < 0.5 ? 1.0f : -1.0f;
                case LfoShape::sampleHold: if (wrapped || held == 0.0f) { rng = rng * 1664525u + 1013904223u; held = (float) ((rng >> 8) & 0xffffff) / 8388608.0f - 1.0f; } return held;
                case LfoShape::drift: default:
                    if (wrapped) { rng = rng * 1664525u + 1013904223u; driftTarget = (float) ((rng >> 8) & 0xffffff) / 8388608.0f - 1.0f; }
                    driftVal += (driftTarget - driftVal) * (float) juce::jmin (1.0, inc * 8.0);
                    return driftVal;
            }
        }
    };

    //==========================================================================
    // Filters
    enum class FilterType { lp24 = 0, lp12, hp24, hp12, bp, notch, ladder, numTypes };
    inline juce::StringArray filterTypeNames()   { return { "LP 24", "LP 12", "HP 24", "HP 12", "BAND", "NOTCH", "LADDER" }; }

    /** Four-pole ladder (Huovilainen-style, tanh in the loop) with resonance compensation. */
    struct Ladder
    {
        float s[4] = { 0, 0, 0, 0 }, g = 0.0f, k = 0.0f;
        void set (double sr, float cutoff, float resonance)
        {
            const float fc = (float) juce::jlimit (10.0, sr * 0.45, (double) cutoff) / (float) sr;
            g = 1.0f - std::exp (-twoPi * fc);
            k = juce::jlimit (0.0f, 4.2f, resonance * 4.2f);
        }
        inline float process (float in, float drive = 1.0f)
        {
            const float fb = s[3];
            float x = std::tanh ((in - k * fb) * drive) ;
            for (int i = 0; i < 4; ++i) { s[i] += g * (x - s[i]); x = s[i]; }
            return x * (1.0f + k * 0.35f);   // make-up for the resonance scoop
        }
        void reset()   { s[0] = s[1] = s[2] = s[3] = 0.0f; }
    };

    /** One filter stage that can be any of the FilterType shapes. */
    struct MultiFilter
    {
        SqDsp::SVF a, b; Ladder ladder;
        FilterType type = FilterType::lp24;
        void set (double sr, FilterType t, float cutoff, float res)
        {
            type = t;
            const float q = 0.5f + res * res * 24.0f;
            a.set (sr, cutoff, t == FilterType::lp24 || t == FilterType::hp24 ? std::sqrt (q) : q);
            b.set (sr, cutoff, std::sqrt (q));
            ladder.set (sr, cutoff, res);
        }
        inline float process (float x)
        {
            switch (type)
            {
                case FilterType::lp12:  return a.lowpass (x);
                case FilterType::lp24:  return b.lowpass (a.lowpass (x));
                case FilterType::hp12:  return hp (a, x);
                case FilterType::hp24:  { float h1 = hp (a, x); return hp (b, h1); }
                case FilterType::bp:    return a.bandpass (x) * 1.5f;
                case FilterType::notch: { float v = x; float bp = a.bandpass (v); return v - a.k * bp; }
                case FilterType::ladder: default: return ladder.process (x);
            }
        }
        void reset()   { a.reset(); b.reset(); ladder.reset(); }
    private:
        static inline float hp (SqDsp::SVF& f, float x)
        {
            // high-pass from the TPT SVF: v0 - k*v1 - v2 (recomputed inline)
            float v3 = x - f.ic2, v1 = f.a1 * f.ic1 + f.a2 * v3, v2 = f.ic2 + f.a2 * f.ic1 + f.a3 * v3;
            f.ic1 = 2 * v1 - f.ic1; f.ic2 = 2 * v2 - f.ic2;
            return x - f.k * v1 - v2;
        }
    };

    //==========================================================================
    // Modulation
    enum class ModSource { none = 0, env1, env2, env3, lfo1, lfo2, velocity, key, modWheel, aftertouch, modX, modY, macro1, macro2, macro3, macro4, random, numSources };
    inline juce::StringArray modSourceNames()   { return { "Off", "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "Velocity", "Key", "Mod Wheel", "Aftertouch", "Mod X", "Mod Y", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Random" }; }

    struct ModSlot { int source = 0; juce::String dest; float depth = 0.0f; };
    static constexpr int numModSlots = 8;

    inline juce::StringArray driveTypeNames()  { return { "SOFT", "TUBE", "HARD", "FOLD" }; }
    inline juce::StringArray polyModeNames()   { return { "POLY", "MONO", "LEGATO" }; }
    inline juce::StringArray glideModeNames()  { return { "OFF", "ALWAYS", "LEGATO" }; }
    inline juce::StringArray qualityNames()    { return { "ECO", "HD" }; }

    /** The type-erased face of every 1.8 synth: what the skin, the MCP tools and the self-tests talk to
        without knowing the voice type. */
    struct SynthCommon
    {
        virtual ~SynthCommon() = default;
        virtual juce::CachedValue<int>& polyModeValue() = 0;
        virtual juce::CachedValue<int>& glideModeValue() = 0;
        virtual juce::CachedValue<int>& unisonValue() = 0;
        virtual juce::CachedValue<int>& qualityValue() = 0;
        virtual juce::CachedValue<int>& maxPolyValue() = 0;
        virtual juce::CachedValue<int>& bendRangeValue() = 0;
        virtual juce::CachedValue<int>& modSourceValue (int slot) = 0;
        virtual juce::CachedValue<juce::String>& modDestValue (int slot) = 0;
        virtual juce::CachedValue<float>& modDepthValue (int slot) = 0;
        virtual int maxVoiceCount() const = 0;
        virtual int activeVoiceCount() const = 0;
        virtual const std::array<float, 512>& scopeData() const = 0;
        virtual int scopeWritePos() const = 0;
        virtual float displayLevel() const = 0;
        virtual int lastNote() const = 0;
        /** Parameter IDs the mod matrix can target (the subclass lists what it honours in prepareVoice). */
        virtual juce::StringArray modDestinations() const = 0;
        virtual SqDsp::ParamBank& synthBank() = 0;
    };

    /** Per-voice modulation values, refreshed once per chunk. */
    struct ModValues
    {
        float v[(int) ModSource::numSources] = {};
        float get (int src) const   { return juce::isPositiveAndBelow (src, (int) ModSource::numSources) ? v[src] : 0.0f; }
    };

    //==========================================================================
    // FX rack
    struct Chorus
    {
        SqDsp::Delay dl, dr; LFO lfo; double sr = 48000;
        void prepare (double rate) { sr = rate; dl.prepare ((int) (sr * 0.05)); dr.prepare ((int) (sr * 0.05)); }
        inline void process (float& l, float& r, float rate, float depth, float mix)
        {
            lfo.setRate (rate, sr);
            const float m = lfo.next (LfoShape::sine);
            dl.push (l); dr.push (r);
            const float base = (float) (sr * 0.012), dep = (float) (sr * 0.0055) * depth;
            const float wl = dl.read (base + dep * (m + 1.0f)), wr = dr.read (base + dep * (1.0f - m));
            l = l * (1.0f - mix * 0.5f) + wl * mix; r = r * (1.0f - mix * 0.5f) + wr * mix;
        }
    };

    struct StereoDelay
    {
        SqDsp::Delay dl, dr; SqDsp::SVF lpL, lpR; double sr = 48000; float fbl = 0, fbr = 0;
        void prepare (double rate) { sr = rate; dl.prepare ((int) (sr * 2.5)); dr.prepare ((int) (sr * 2.5)); lpL.set (sr, 6000); lpR.set (sr, 6000); }
        inline void process (float& l, float& r, float timeL, float timeR, float feedback, float tone, float mix, bool pingPong)
        {
            lpL.set (sr, tone); lpR.set (sr, tone);
            const float outL = dl.read (juce::jmax (1.0f, timeL * (float) sr)), outR = dr.read (juce::jmax (1.0f, timeR * (float) sr));
            if (pingPong) { dl.push (l + lpL.lowpass (outR) * feedback); dr.push (r + lpR.lowpass (outL) * feedback); }
            else          { dl.push (l + lpL.lowpass (outL) * feedback); dr.push (r + lpR.lowpass (outR) * feedback); }
            l += outL * mix; r += outR * mix;
        }
    };

    struct Reverb
    {
        juce::Reverb rv; juce::Reverb::Parameters p;
        void prepare (double sr) { rv.setSampleRate (sr); rv.reset(); }
        void set (float size, float damp, float width, float mix) { p.roomSize = size; p.damping = damp; p.width = width; p.wetLevel = mix; p.dryLevel = 1.0f - mix * 0.6f; p.freezeMode = 0; rv.setParameters (p); }
        void process (float* l, float* r, int n) { if (p.wetLevel <= 0.0005f) return; rv.processStereo (l, r, n); }
    };

    inline float driveShape (float x, float amount, int type)
    {
        const float g = 1.0f + amount * amount * 24.0f;
        float y = x * g;
        switch (type)
        {
            case 0: y = std::tanh (y); break;                                                          // soft
            case 1: y = y / (1.0f + std::abs (y)); break;                                              // tube-ish
            case 2: y = juce::jlimit (-1.0f, 1.0f, y); break;                                          // hard
            default: y = std::sin (juce::jlimit (-juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, y)); break; // fold
        }
        return y / std::sqrt (g);   // keep perceived level steady
    }

    //==========================================================================
    /** The scaffolding every 1.8 synth shares. Subclasses declare parameters and voices; this class does MIDI,
        voice allocation, glide, unison, modulation sources, FX, quality and silence handling. */
    template <typename VoiceT, int MaxVoices>
    class SynthPluginBase : public te::Plugin,
                            public SqDsp::BankOwner,
                            public SynthCommon
    {
    public:
        SynthPluginBase (te::PluginCreationInfo info) : te::Plugin (info)
        {
            auto um = getUndoManager();
            polyMode.referTo (state, "polyMode", um, 0);        // 0 poly, 1 mono, 2 legato
            glideMode.referTo (state, "glideMode", um, 0);      // 0 off, 1 always, 2 legato only
            unisonVoices.referTo (state, "unison", um, 1);
            quality.referTo (state, "quality", um, 1);          // 0 eco, 1 HD (default: HD, 2x oversampled voices)
            maxPoly.referTo (state, "maxPoly", um, MaxVoices);
            bendRange.referTo (state, "bendRange", um, 2);
            for (int i = 0; i < numModSlots; ++i)
            {
                modSrc[i].referTo (state, "modSrc" + juce::String (i), um, 0);
                modDest[i].referTo (state, "modDest" + juce::String (i), um, juce::String());
                modDepth[i].referTo (state, "modDepth" + juce::String (i), um, 0.0f);
            }
        }

        ~SynthPluginBase() override   { notifyListenersOfDeletion(); bank.detach(); }

        SqDsp::ParamBank& getBank() override   { return bank; }

        // SynthCommon
        juce::CachedValue<int>& polyModeValue() override                 { return polyMode; }
        juce::CachedValue<int>& glideModeValue() override                { return glideMode; }
        juce::CachedValue<int>& unisonValue() override                   { return unisonVoices; }
        juce::CachedValue<int>& qualityValue() override                  { return quality; }
        juce::CachedValue<int>& maxPolyValue() override                  { return maxPoly; }
        juce::CachedValue<int>& bendRangeValue() override                { return bendRange; }
        juce::CachedValue<int>& modSourceValue (int i) override          { return modSrc[juce::jlimit (0, numModSlots - 1, i)]; }
        juce::CachedValue<juce::String>& modDestValue (int i) override   { return modDest[juce::jlimit (0, numModSlots - 1, i)]; }
        juce::CachedValue<float>& modDepthValue (int i) override         { return modDepth[juce::jlimit (0, numModSlots - 1, i)]; }
        int maxVoiceCount() const override                               { return MaxVoices; }
        int activeVoiceCount() const override                            { return getActiveVoiceCount(); }
        const std::array<float, 512>& scopeData() const override         { return scope; }
        int scopeWritePos() const override                               { return scopeWrite.load(); }
        float displayLevel() const override                              { return displayEnv; }
        int lastNote() const override                                    { return lastNoteNumber.load(); }
        juce::StringArray modDestinations() const override               { return { "pitch", "cutoff", "resonance", "pw", "amp", "pan", "fm" }; }
        SqDsp::ParamBank& synthBank() override                           { return bank; }

        void resetProps (juce::UndoManager* um) override
        {
            polyMode.setValue (0, um); glideMode.setValue (0, um); unisonVoices.setValue (1, um); bendRange.setValue (2, um); quality.setValue (defaultQuality, um); maxPoly.setValue (MaxVoices, um);
            for (int i = 0; i < numModSlots; ++i) { modSrc[i].setValue (0, um); modDest[i].setValue ({}, um); modDepth[i].setValue (0.0f, um); }
        }

        // te::Plugin
        bool isSynth() override                             { return true; }
        bool takesMidiInput() override                      { return true; }
        bool takesAudioInput() override                     { return false; }
        bool producesAudioWhenNoAudioInput() override       { return true; }
        bool needsConstantBufferSize() override             { return false; }
        int getNumOutputChannelsGivenInputs (int) override  { return 2; }
        BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
        double getTailLength() const override               { return 6.0; }
        bool canBeAddedToClip() override                    { return false; }
        bool canBeAddedToRack() override                    { return true; }

        void initialise (const te::PluginInitialisationInfo& info) override
        {
            sr = info.sampleRate;
            mono.setSize (2, chunk * 2); os2.setSize (2, chunk * 2); hdActive = false; decimL.reset(); decimR.reset();
            for (auto& v : voices) { v.prepare (sr); v.active = false; }
            chorus.prepare (sr); delay.prepare (sr); reverb.prepare (sr);
            lfo1.reset(); lfo2.reset();
            held.clearQuick();
            prepared (sr);
        }
        void deinitialise() override {}
        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            for (auto& cv : bank.values) if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
            for (auto* cv : std::initializer_list<juce::CachedValue<int>*> { &polyMode, &glideMode, &unisonVoices, &quality, &maxPoly, &bendRange })
                if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
            for (int i = 0; i < numModSlots; ++i)
            {
                if (v.hasProperty (modSrc[i].getPropertyID())) modSrc[i].setValue (v.getProperty (modSrc[i].getPropertyID()), nullptr);
                if (v.hasProperty (modDest[i].getPropertyID())) modDest[i].setValue (v.getProperty (modDest[i].getPropertyID()), nullptr);
                if (v.hasProperty (modDepth[i].getPropertyID())) modDepth[i].setValue (v.getProperty (modDepth[i].getPropertyID()), nullptr);
            }
        }

        //==========================================================================
        // Hooks for the instrument
        virtual void prepared (double) {}
        /** Per-chunk parameter refresh (read the bank once, compute coefficients). */
        virtual void updateChunk (const ModValues& global) = 0;
        /** Called for every sounding voice just before it renders a chunk: apply the mod matrix to voice fields. */
        virtual void prepareVoice (VoiceT&) {}
        /** Render the FX / master stage into l and r for n samples (voices are already summed into them). */
        virtual void renderMaster (float* l, float* r, int n) = 0;
        /** Scope access for the editor. */
        const std::array<float, 512>& getScope() const   { return scope; }
        int getScopeWritePos() const                     { return scopeWrite.load(); }
        float getDisplayLevel() const                    { return displayEnv; }
        int getActiveVoiceCount() const                  { int n = 0; for (auto& v : voices) n += v.active ? 1 : 0; return n; }

        /** Modulation of a parameter id by the matrix: returns the summed bipolar offset (depth units) for this voice. */
        float modFor (const juce::String& paramID, const ModValues& mv) const
        {
            float sum = 0.0f;
            for (int i = 0; i < numModSlots; ++i)
                if (modSrc[i].get() != 0 && modDest[i].get() == paramID) sum += mv.get (modSrc[i].get()) * modDepth[i].get();
            return sum;
        }

        //==========================================================================
        void applyToBuffer (const te::PluginRenderContext& fc) override
        {
            if (fc.destBuffer == nullptr) return;
            SCOPED_REALTIME_CHECK
            juce::ScopedNoDenormals noDenormals;
            auto& out = *fc.destBuffer;
            const int start = fc.bufferStartSample, num = fc.bufferNumSamples;
            for (int c = 0; c < out.getNumChannels(); ++c) out.clear (c, start, num);

            midiEvents.clearQuick();
            // an event whose time rounds to the very end of the block belongs to the first sample of the next one (rounding
            // in the block timestamps would otherwise start it a sample early - sample-exact slicers notice)
            for (auto& e : carryEvents) midiEvents.add ({ juce::jlimit (0, num - 1, e.sample), e.msg });
            carryEvents.clearQuick();
            if (fc.bufferForMidiMessages != nullptr)
            {
                if (fc.bufferForMidiMessages->isAllNotesOff) { held.clearQuick(); carryEvents.clearQuick(); midiEvents.clearQuick(); for (auto& v : voices) if (v.active) v.release(); }
                for (auto& m : *fc.bufferForMidiMessages)
                {
                    const int at = juce::jmax (0, juce::roundToInt (m.getTimeStamp() * sr));
                    if (at >= num) carryEvents.add ({ at - num, juce::MidiMessage (m) }); else midiEvents.add ({ at, juce::MidiMessage (m) });
                }
            }
            bool anyActive = false;
            for (auto& v : voices) anyActive = anyActive || v.active;
            if (! anyActive && midiEvents.isEmpty() && tailSamples <= 0) { displayEnv = 0.0f; return; }

            // tempo for synced LFOs / delays; beat position for the sequencer instruments (Toxin FM, Larva Bass)
            bpm = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
            blockStartBeat = edit.tempoSequence.toBeats (fc.editTime.getStart()).inBeats();
            transportPlaying = fc.isPlaying;
            if (const bool hd = quality.get() == 1; hd != hdActive)
            {
                hdActive = hd;
                for (auto& v : voices) { v.prepare (hd ? sr * 2.0 : sr); v.active = false; }
                decimL.reset(); decimR.reset();
            }

            int ev = 0;
            for (int pos = 0; pos < num; pos += chunk)
            {
                const int n = juce::jmin (chunk, num - pos);
                chunkStartSample = pos;
                // deliver MIDI that falls inside this chunk (at chunk resolution - 32 samples)
                while (ev < midiEvents.size() && midiEvents[ev].sample < pos + n) { eventOffsetInChunk = midiEvents[ev].sample - pos; handleMidi (midiEvents[ev++].msg); }
                eventOffsetInChunk = 0;

                globalMods.v[(int) ModSource::lfo1] = lfo1.next (lfoShape1);
                globalMods.v[(int) ModSource::lfo2] = lfo2.next (lfoShape2);
                globalMods.v[(int) ModSource::modWheel] = modWheel;
                globalMods.v[(int) ModSource::aftertouch] = aftertouch;
                globalMods.v[(int) ModSource::modX] = modX;
                globalMods.v[(int) ModSource::modY] = modY;
                for (int i = 0; i < 4; ++i) globalMods.v[(int) ModSource::macro1 + i] = macroValue (i);
                updateChunk (globalMods);

                auto* l = mono.getWritePointer (0); auto* r = mono.getWritePointer (1);
                juce::FloatVectorOperations::clear (l, n); juce::FloatVectorOperations::clear (r, n);
                // HD: the voices run at twice the sample rate and are decimated with a half-band filter (aliasing from
                // sync / FM / ring / drive folds back far lower); Eco runs them at the session rate.
                const bool hd = hdActive;
                float* vl = l; float* vr = r; const int vn = hd ? n * 2 : n;
                if (hd) { vl = os2.getWritePointer (0); vr = os2.getWritePointer (1); juce::FloatVectorOperations::clear (vl, vn); juce::FloatVectorOperations::clear (vr, vn); }
                bool active = false;
                for (auto& v : voices)
                    if (v.active) { v.mods = globalMods; v.mods.v[(int) ModSource::velocity] = v.velocity; v.mods.v[(int) ModSource::key] = (v.note - 60) / 60.0f; v.mods.v[(int) ModSource::random] = v.random;
                                    v.mods.v[(int) ModSource::env1] = v.envLevel (0); v.mods.v[(int) ModSource::env2] = v.envLevel (1); v.mods.v[(int) ModSource::env3] = v.envLevel (2);
                                    prepareVoice (v); v.render (vl, vr, vn); active = true; }
                if (hd) { decimL.process (vl, l, n); decimR.process (vr, r, n); }
                renderMaster (l, r, n);
                tailSamples = active ? (int) (sr * 4) : tailSamples - n;

                auto* ol = out.getWritePointer (0, start + pos);
                auto* orr = out.getNumChannels() > 1 ? out.getWritePointer (1, start + pos) : nullptr;
                int sw = scopeWrite.load(); float pk = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    ol[i] = l[i]; if (orr) orr[i] = r[i];
                    scope[(size_t) (sw & 511)] = (l[i] + r[i]) * 0.5f; ++sw;
                    pk = juce::jmax (pk, std::abs (l[i]));
                }
                scopeWrite = sw;
                displayEnv = juce::jmax (pk, displayEnv * 0.9f);
            }
            while (ev < midiEvents.size()) handleMidi (midiEvents[ev++].msg);
        }

        //==========================================================================
        // Voice allocation (poly / mono / legato / glide / unison)
        virtual void noteOn (int note, float vel)
        {
            held.removeAllInstancesOf (note); held.add (note);
            const int mode = polyMode.get();
            const int uni = juce::jlimit (1, 8, unisonVoices.get());
            const bool legatoPlaying = mode != 0 && anyVoiceSounding();
            if (mode != 0 && legatoPlaying)
            {
                // mono / legato: every sounding voice (unison) takes the new pitch; mono retriggers the envelopes, legato doesn't
                for (auto& v : voices) if (v.active && ! v.releasing) { v.retarget (note, glideSeconds (true)); if (mode == 1) v.restartEnvelopes(); v.velocity = vel; }
                lastNoteHz = (float) SqDsp::noteHz (note); lastNoteNumber = note;
                return;
            }
            float lastHz = lastNoteHz;
            for (int u = 0; u < uni; ++u)
            {
                auto* v = allocateVoice();
                if (v == nullptr) break;
                const float spread = uni > 1 ? ((float) u / (float) (uni - 1) * 2.0f - 1.0f) : 0.0f;
                v->unisonDetune = spread; v->unisonPan = spread * unisonSpread; v->unisonIndex = u; v->unisonGain = 1.0f / std::sqrt ((float) uni);
                v->random = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                v->bend = pitchBend;
                v->start (note, vel, glideMode.get() == 1 || (glideMode.get() == 2 && legatoPlaying) ? glideSeconds (true) : 0.0f, lastHz);
            }
            lastNoteHz = (float) SqDsp::noteHz (note);
            lastNoteNumber = note;
        }

        virtual void noteOff (int note)
        {
            held.removeAllInstancesOf (note);
            const int mode = polyMode.get();
            if (mode != 0 && ! held.isEmpty())
            {
                // mono: fall back to the previous held note
                const int back = held.getLast();
                for (auto& v : voices) if (v.active && ! v.releasing) { v.retarget (back, glideSeconds (true)); if (mode == 1) v.restartEnvelopes(); }
                return;
            }
            for (auto& v : voices) if (v.active && v.note == note && ! v.releasing) v.release();
        }

        void handleMidi (const juce::MidiMessage& m)
        {
            if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) noteOff (m.getNoteNumber());
            else if (m.isPitchWheel()) { pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * (float) bendRange.get(); for (auto& v : voices) v.bend = pitchBend; }
            else if (m.isController())
            {
                const int cc = m.getControllerNumber(); const float val = m.getControllerValue() / 127.0f;
                if (cc == 1) modWheel = val;
                else if (cc == 74) modX = val * 2.0f - 1.0f;       // FL Mod X convention: cutoff
                else if (cc == 71) modY = val * 2.0f - 1.0f;       // Mod Y: resonance
                else if (cc == 64) { sustainPedal = val > 0.5f; if (! sustainPedal) for (auto& v : voices) if (v.active && ! held.contains (v.note)) v.release(); }
                else if (cc == 123 || cc == 120) { for (auto& v : voices) v.release(); held.clearQuick(); }
            }
            else if (m.isChannelPressure()) aftertouch = m.getChannelPressureValue() / 127.0f;
            else if (m.isAftertouch()) aftertouch = m.getAfterTouchValue() / 127.0f;
        }

        float glideSeconds (bool) const   { return glideTime; }
        float glideTime = 0.08f, unisonSpread = 0.6f, unisonDetuneCents = 12.0f;

    protected:
        SqDsp::ParamBank bank;
        juce::CachedValue<int> polyMode, glideMode, unisonVoices, quality, maxPoly, bendRange;
        int defaultQuality = 1;   // what a preset load resets Quality to (samplers and slicers: Eco, they play samples 1:1)
        juce::CachedValue<int> modSrc[numModSlots];
        juce::CachedValue<juce::String> modDest[numModSlots];
        juce::CachedValue<float> modDepth[numModSlots];
        std::array<VoiceT, (size_t) MaxVoices> voices;
        LFO lfo1, lfo2; LfoShape lfoShape1 = LfoShape::sine, lfoShape2 = LfoShape::triangle;
        Chorus chorus; StereoDelay delay; Reverb reverb;
        ModValues globalMods;
        double sr = 48000.0, bpm = 120.0, blockStartBeat = 0.0;
        bool transportPlaying = false;
        int chunkStartSample = 0;
        /** The song beat at the start of the chunk being rendered (for synced sequencers). */
        double chunkBeat() const   { return blockStartBeat + (double) chunkStartSample / sr * bpm / 60.0; }
        static constexpr int chunk = 32;
        juce::AudioBuffer<float> mono, os2;
        bool hdActive = false;
        /** 2:1 half-band decimator (31-tap windowed sinc, ~ -80 dB stop band). */
        struct Decimator
        {
            static constexpr int taps = 31; float hist[taps * 2] = {}; int pos = 0;
            static const std::array<float, taps>& coeffs()
            {
                static const std::array<float, taps> c = []
                {
                    std::array<float, taps> h {}; double sum = 0;
                    for (int i = 0; i < taps; ++i)
                    {
                        const int k = i - taps / 2;
                        const double sinc = k == 0 ? 0.5 : std::sin (juce::MathConstants<double>::pi * 0.5 * k) / (juce::MathConstants<double>::pi * k);
                        const double w = 0.42 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (taps - 1)) + 0.08 * std::cos (4.0 * juce::MathConstants<double>::pi * i / (taps - 1));
                        h[(size_t) i] = (float) (sinc * w); sum += h[(size_t) i];
                    }
                    for (auto& v : h) v = (float) (v / sum);
                    return h;
                }();
                return c;
            }
            void reset()   { std::fill (std::begin (hist), std::end (hist), 0.0f); pos = 0; }
            inline void process (const float* in2x, float* out, int n)
            {
                const auto& h = coeffs();
                for (int i = 0; i < n; ++i)
                {
                    for (int k = 0; k < 2; ++k) { hist[pos] = hist[pos + taps] = in2x[i * 2 + k]; pos = (pos + 1) % taps; }
                    // hist[pos .. pos+taps) is the last `taps` inputs, oldest first
                    float acc = 0.0f; const float* w = hist + pos;
                    for (int t = 0; t < taps; ++t) acc += w[t] * h[(size_t) (taps - 1 - t)];
                    out[i] = acc;
                }
            }
        } decimL, decimR;
        float modWheel = 0, aftertouch = 0, modX = 0, modY = 0, pitchBend = 0, lastNoteHz = 220.0f, displayEnv = 0;
        bool sustainPedal = false;
        int tailSamples = 0;
        juce::Array<int> held;
        struct Ev { int sample; juce::MidiMessage msg; };
        juce::Array<Ev> midiEvents, carryEvents;
        int eventOffsetInChunk = 0;   // where inside the current 32-sample chunk the MIDI event being handled falls (samplers use it for sample-accurate starts)
        std::array<float, 512> scope {};
        std::atomic<int> scopeWrite { 0 };
        std::atomic<int> lastNoteNumber { -1 };

        virtual float macroValue (int) const   { return 0.0f; }

        /** LFO rate helper: hz, or a tempo division when synced (division in beats). The LFO advances once per chunk. */
        void setLfo (LFO& l, float hz, bool sync, float beats)
        {
            const double rate = sync ? (bpm / 60.0) / juce::jmax (0.0625f, beats) : (double) hz;
            l.setRate (rate * chunk, sr);   // one step per chunk
        }

        bool anyVoiceSounding() const   { for (auto& v : voices) if (v.active && ! v.releasing) return true; return false; }

        VoiceT* allocateVoice()
        {
            const int limit = juce::jlimit (1, juce::jmin (MaxVoices, voiceCap), maxPoly.get());
            int count = 0; VoiceT* freeVoice = nullptr; VoiceT* quietest = nullptr; VoiceT* oldest = nullptr;
            for (auto& v : voices)
            {
                if (! v.active) { if (freeVoice == nullptr) freeVoice = &v; continue; }
                ++count;
                if (v.releasing && (quietest == nullptr || v.ageCounter < quietest->ageCounter)) quietest = &v;
                if (oldest == nullptr || v.ageCounter < oldest->ageCounter) oldest = &v;
            }
            if (freeVoice != nullptr && count < limit) return freeVoice;
            auto* steal = quietest != nullptr ? quietest : oldest;
            if (steal != nullptr) steal->kill();
            return steal;
        }
        int voiceCap = MaxVoices;   // an instrument may cap the polyphony below MaxVoices from the audio thread (Lantern's vintage mode) without touching the Max Voices property
    };

    //==========================================================================
    /** Base for a voice: note bookkeeping, glide and the envelopes. Instruments add oscillators and filters. */
    struct VoiceBase
    {
        bool active = false, releasing = false;
        int note = 60, unisonIndex = 0; float velocity = 1.0f, bend = 0.0f, random = 0.0f, unisonDetune = 0.0f, unisonPan = 0.0f, unisonGain = 1.0f;
        double sr = 48000.0, hz = 440.0, targetHz = 440.0, glideCoef = 0.0;
        juce::int64 ageCounter = 0;
        static inline juce::int64 ageClock = 0;
        ModValues mods;
        ADSR env[3];

        void prepare (double rate)   { sr = rate; for (auto& e : env) e.prepare (rate); }

        void startBase (int n, float vel, float glideSecs, float fromHz)
        {
            note = n; velocity = vel; active = true; releasing = false; ageCounter = ageClock++;
            targetHz = SqDsp::noteHz (n);
            if (glideSecs > 0.0005f && fromHz > 0) { hz = fromHz; glideCoef = 1.0 - std::exp (-1.0 / (glideSecs * sr) * 32.0); }   // per chunk
            else { hz = targetHz; glideCoef = 1.0; }
            for (auto& e : env) e.noteOn();
        }
        void retarget (int n, float glideSecs)
        {
            note = n; targetHz = SqDsp::noteHz (n);
            glideCoef = glideSecs > 0.0005f ? 1.0 - std::exp (-1.0 / (glideSecs * sr) * 32.0) : 1.0;
        }
        virtual ~VoiceBase() = default;
        virtual void restartEnvelopes()   { for (auto& e : env) e.noteOn(); releasing = false; }
        virtual void release()            { releasing = true; for (auto& e : env) e.noteOff(); }
        virtual void kill()               { active = false; releasing = false; for (auto& e : env) e.kill(); }
        float envLevel (int i) const   { return env[i].level; }

        /** Call once per chunk: glides the pitch, returns the current frequency with bend applied. */
        inline double chunkFrequency (float bendSemis, float detuneCents = 0.0f)
        {
            hz += (targetHz - hz) * glideCoef;
            return hz * std::pow (2.0, (bendSemis + detuneCents / 100.0) / 12.0);
        }
    };
}
