#pragma once
// Squelchmoth 1.8.1 instrument shelf, part 2 - all on SqSynthCore:
//
//  Hexwing     - six-operator FM / RM / subtractive hybrid: per-operator wave, ratio, level, feedback and envelope,
//                8 algorithms, a filter after the operators, 2 LFOs, mod matrix, unison, FX
//  Toxin FM    - the same six operators with a 16-step pitch / gate / accent sequencer and a filter
//  Lantern     - vintage polysynth: two drifting oscillators, self-oscillating ladder, ensemble, 8-voice "vintage" mode
//  Lantern Exo - the vintage synth re-imagined: wavetable + analogue oscillators, dual filters, 32 voices, mod matrix
//  Larva Bass  - acid bass: saw / square, diode-style ladder, accent, slide, a 16-step sequencer with slide + accent per step
//  Thorn Bass  - electric bass model: plucked string + pickup + amp, slap, palm mute, fret noise, round-robin picks
//
// Every one is original DSP, exposes everything to automation and Claude, and uses no CPU while silent.

#include "SqSynths.h"

namespace SqSynth
{
    //==========================================================================
    // Six-operator FM
    static constexpr int numOps = 6;
    inline juce::StringArray opWaveNames()    { return { "SINE", "SOFT", "SHARP", "SQUARE", "HALF", "CUBED" }; }
    inline juce::StringArray algorithmNames() { return { "1  Stack 6>5>4>3>2>1", "2  Two stacks (1, 3)", "3  Three pairs (1, 2, 3)", "4  Fan-in to 1", "5  Two 3-stacks (1, 2)", "6  Three pairs (1, 3, 5)", "7  Op 6 drives five", "8  Six carriers (additive)" }; }

    /** Which operators modulate which, per algorithm. mods[op] lists the operators feeding op (0-based). carrier[op] = heard. */
    struct Algorithm { int mods[numOps][3]; int numMods[numOps]; bool carrier[numOps]; };
    inline const Algorithm& algorithm (int i)
    {
        static const Algorithm a[8] = {
            { { { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { } },             { 1, 1, 1, 1, 1, 0 }, { true,  false, false, false, false, false } },
            { { { 1 }, { }, { 3 }, { 4 }, { 5 }, { } },               { 1, 0, 1, 1, 1, 0 }, { true,  false, true,  false, false, false } },
            { { { 3 }, { 4 }, { 5 }, { }, { }, { } },                 { 1, 1, 1, 0, 0, 0 }, { true,  true,  true,  false, false, false } },
            { { { 1, 2, 3 }, { 4 }, { 5 }, { }, { }, { } },           { 3, 1, 1, 0, 0, 0 }, { true,  false, false, false, false, false } },
            { { { 2 }, { 3 }, { 4 }, { 5 }, { }, { } },               { 1, 1, 1, 1, 0, 0 }, { true,  true,  false, false, false, false } },
            { { { 1 }, { }, { 3 }, { }, { 5 }, { } },                 { 1, 0, 1, 0, 1, 0 }, { true,  false, true,  false, true,  false } },
            { { { 5 }, { 5 }, { 5 }, { 5 }, { 5 }, { } },             { 1, 1, 1, 1, 1, 0 }, { true,  true,  true,  true,  true,  false } },
            { { { }, { }, { }, { }, { }, { } },                       { 0, 0, 0, 0, 0, 0 }, { true,  true,  true,  true,  true,  true  } } };
        return a[juce::jlimit (0, 7, i)];
    }

    struct OpSettings { int wave = 0; float ratio = 1.0f, fine = 0.0f, level = 0.0f, feedback = 0.0f, a = 0.005f, d = 0.5f, s = 0.7f, r = 0.3f, vel = 0.5f, keyScale = 0.0f; bool fixed = false; };
    struct Fm6Settings
    {
        OpSettings op[numOps];
        int algorithm = 0; float modDepth = 1.0f;
        FilterType fType = FilterType::lp24; float cutoff = 20000.0f, res = 0.0f, fEnv = 0.0f, fKey = 0.0f; float fAdsr[4] = { 0.005f, 0.4f, 0.0f, 0.3f };
        float lfo1Pitch = 0.0f, lfo1Index = 0.0f, lfo2Amp = 0.0f, lfo2Cut = 0.0f;
        float unisonCents = 8.0f, unisonSpread = 0.5f, pan = 0.0f, gain = 0.5f, drive = 0.0f; int driveType = 0;
    };

    inline float opWave (int w, float ph)
    {
        const float x = ph - std::floor (ph);
        switch (w)
        {
            case 0: return std::sin (x * twoPi);
            case 1: { const float s = std::sin (x * twoPi); return s * (1.1f - 0.1f * s * s) * 0.92f; }                  // soft: a touch of 3rd harmonic
            case 2: { const float s = std::sin (x * twoPi); return std::tanh (s * 2.2f) * 1.03f; }                     // sharp: saturated sine (odd harmonics)
            case 3: { const float s = std::sin (x * twoPi); return std::tanh (s * 9.0f); }                            // square-ish
            case 4: return x < 0.5f ? std::sin (x * twoPi) : 0.0f;                                                      // half sine
            default: { const float s = std::sin (x * twoPi); return s * s * s; }                                        // cubed
        }
    }

    struct Fm6Voice : public VoiceBase
    {
        double phase[numOps] = {}; float fb[numOps] = {}, lastOut[numOps] = {};
        ADSR opEnv[numOps];
        MultiFilter filter;
        float mPitch = 0, mIndex = 0, mCut = 0, mAmp = 0, mPan = 0, mFeedback = 0, mOpLevel[numOps] = {};
        const Fm6Settings* set = nullptr;
        int seqOffset = 0;   // semitone offset from a sequencer step (Toxin)

        void prepare (double rate)   { VoiceBase::prepare (rate); for (auto& e : opEnv) e.prepare (rate); }
        void start (int n, float vel, float glideSecs, float fromHz)
        {
            startBase (n, vel, glideSecs, fromHz);
            for (int i = 0; i < numOps; ++i) { phase[i] = 0.0; fb[i] = 0.0f; lastOut[i] = 0.0f; opEnv[i].noteOn(); }
            filter.reset();
        }
        void restartEnvelopes() override   { VoiceBase::restartEnvelopes(); for (auto& e : opEnv) e.noteOn(); }
        void release() override            { VoiceBase::release(); for (auto& e : opEnv) e.noteOff(); }
        void kill() override               { VoiceBase::kill(); for (auto& e : opEnv) e.kill(); }

        inline void render (float* l, float* r, int n)
        {
            if (set == nullptr) return;
            const auto& s = *set;
            const auto& alg = algorithm (s.algorithm);
            for (int i = 0; i < numOps; ++i) opEnv[i].set (s.op[i].a, s.op[i].d, s.op[i].s, s.op[i].r);
            env[0].set (s.fAdsr[0], s.fAdsr[1], s.fAdsr[2], s.fAdsr[3]);
            const double base = chunkFrequency (bend + mPitch + (float) seqOffset + s.lfo1Pitch * mods.get ((int) ModSource::lfo1), unisonDetune * s.unisonCents);
            double inc[numOps]; float lvl[numOps], fbk[numOps]; bool heard[numOps]; float velScale[numOps];
            for (int i = 0; i < numOps; ++i)
            {
                const auto& o = s.op[i];
                const double hz = o.fixed ? (double) o.ratio * 100.0 : base * (double) o.ratio * std::pow (2.0, (double) o.fine / 1200.0);
                inc[i] = juce::jlimit (0.0, 0.49, hz / sr);
                heard[i] = alg.carrier[i];
                velScale[i] = 1.0f - o.vel + o.vel * velocity;
                const float keyScale = std::pow (2.0f, -o.keyScale * (float) (note - 60) / 24.0f);   // bright keys get less index
                lvl[i] = juce::jlimit (0.0f, 1.5f, (o.level + mOpLevel[i]) * velScale[i] * (heard[i] ? 1.0f : keyScale));
                fbk[i] = juce::jlimit (0.0f, 1.0f, o.feedback + mFeedback);
            }
            const float idx = juce::jmax (0.0f, s.modDepth + mIndex + s.lfo1Index * mods.get ((int) ModSource::lfo1)) * 2.2f;   // phase cycles of modulation at level 1
            const float e1 = env[0].level;
            const float cutMul = std::pow (2.0f, s.fEnv * e1 * 5.0f + (float) (note - 60) / 12.0f * s.fKey + mCut * 4.0f + s.lfo2Cut * mods.get ((int) ModSource::lfo2) * 3.0f);
            const bool useFilter = s.fType != FilterType::lp24 || s.cutoff < 19000.0f || s.fEnv != 0.0f || mCut != 0.0f;
            if (useFilter) filter.set (sr, s.fType, juce::jlimit (20.0f, 20000.0f, s.cutoff * cutMul), juce::jlimit (0.0f, 1.0f, s.res));
            const float ampMod = juce::jlimit (0.0f, 2.0f, 1.0f + mAmp + s.lfo2Amp * mods.get ((int) ModSource::lfo2));
            const float panPos = juce::jlimit (-1.0f, 1.0f, s.pan + unisonPan * s.unisonSpread + mPan);
            const float gl = std::cos ((panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi), gr = std::sin ((panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            int carriers = 0; for (int i = 0; i < numOps; ++i) carriers += heard[i] ? 1 : 0;
            const float carrierNorm = 1.0f / std::sqrt ((float) juce::jmax (1, carriers));

            for (int smp = 0; smp < n; ++smp)
            {
                float out[numOps]; float mix = 0.0f; bool anyEnv = false;
                // modulators first (op 6 .. op 1): every algorithm only feeds lower-numbered operators
                for (int i = numOps - 1; i >= 0; --i)
                {
                    const auto& o = s.op[i];
                    float modIn = 0.0f;
                    for (int k = 0; k < alg.numMods[i]; ++k) modIn += out[alg.mods[i][k]];
                    const float e = opEnv[i].next(); anyEnv = anyEnv || opEnv[i].isActive();
                    const float ph = (float) phase[i] + modIn * idx + fb[i] * fbk[i] * 0.5f;
                    const float v = opWave (o.wave, ph) * e * lvl[i];
                    fb[i] = 0.5f * (v + lastOut[i]); lastOut[i] = v;   // averaged feedback (stable, no chirp)
                    out[i] = v;
                    phase[i] += inc[i]; if (phase[i] >= 1.0) phase[i] -= 1.0;
                    if (heard[i]) mix += v;
                }
                mix *= carrierNorm;
                if (s.drive > 0.001f) mix = driveShape (mix, s.drive, s.driveType);
                if (useFilter) mix = filter.process (mix);
                env[0].next(); env[1].next();
                const float y = mix * ampMod * s.gain * unisonGain;
                l[smp] += y * gl; r[smp] += y * gr;
                if (! anyEnv) { active = false; break; }
            }
            // a voice whose heard operators have all finished is done
            bool heardActive = false; for (int i = 0; i < numOps; ++i) if (heard[i] && opEnv[i].isActive()) heardActive = true;
            if (! heardActive) active = false;
        }
    };

    /** Sequencer step specs for an instrument: 16 x Note, 16 x On, 16 x Accent, 16 x Slide. Built once per tag (the
        ParamSpec strings must outlive every instance). */
    inline const std::vector<SqDsp::ParamSpec>& stepSpecs (const char* tag, int numSteps, float noteLo, const std::function<float (int kind, int step)>& def)
    {
        static std::map<juce::String, std::vector<SqDsp::ParamSpec>> cache;
        static std::vector<std::unique_ptr<juce::String>> store;
        auto it = cache.find (tag);
        if (it != cache.end()) return it->second;
        std::vector<SqDsp::ParamSpec> out;
        auto keep = [] (const juce::String& t) { store.push_back (std::make_unique<juce::String> (t)); return store.back()->toRawUTF8(); };
        static const char* keys[4] = { "Note", "On", "Acc", "Slide" }; static const char* names[4] = { "Step %d Note", "Step %d On", "Step %d Accent", "Step %d Slide" };
        for (int k = 0; k < 4; ++k)
            for (int i = 0; i < numSteps; ++i)
            {
                const char* id = keep ("step" + juce::String (i + 1) + keys[k]); const char* nm = keep (juce::String (names[k]).replace ("%d", juce::String (i + 1)));
                if (k == 0) out.push_back ({ id, nm, noteLo, 24, def (0, i), 1, " st", 0 });
                else out.push_back ({ id, nm, 0, 1, def (k, i), 1, "", 0 });
            }
        return cache[tag] = out;
    }

    /** Shared body of the two FM instruments: the operator parameter block, filter, LFOs, FX. */
    template <int MaxVoices>
    class Fm6Synth : public SynthPluginBase<Fm6Voice, MaxVoices>
    {
    public:
        using Base = SynthPluginBase<Fm6Voice, MaxVoices>;
        using Base::bank; using Base::voices; using Base::sr; using Base::chorus; using Base::delay; using Base::reverb; using Base::lfo1; using Base::lfo2; using Base::lfoShape1; using Base::lfoShape2;
        Fm6Synth (te::PluginCreationInfo i) : Base (i) { for (auto& v : voices) v.set = &settings; }

        juce::StringArray modDestinations() const override   { return { "pitch", "index", "cutoff", "amp", "pan", "feedback", "op1", "op2", "op3", "op4", "op5", "op6" }; }

        void prepareVoice (Fm6Voice& v) override
        {
            const auto& m = v.mods;
            v.mPitch = Base::modFor ("pitch", m) * 24.0f; v.mIndex = Base::modFor ("index", m) + m.get ((int) ModSource::modY) * 0.5f;
            v.mCut = Base::modFor ("cutoff", m) + m.get ((int) ModSource::modX) * 0.5f; v.mAmp = Base::modFor ("amp", m); v.mPan = Base::modFor ("pan", m); v.mFeedback = Base::modFor ("feedback", m);
            static const char* ids[numOps] = { "op1", "op2", "op3", "op4", "op5", "op6" };
            for (int i = 0; i < numOps; ++i) v.mOpLevel[i] = Base::modFor (ids[i], m);
        }

        /** The operator parameter specs (ids op1Wave .. op6A, then op1D .. op6Key). Built once: the ParamSpec strings must outlive every instance. */
        static const std::vector<SqDsp::ParamSpec>& opSpecs (const float (&levels)[numOps], const float (&ratios)[numOps])
        {
            static std::vector<std::unique_ptr<juce::String>> store;
            static const std::vector<SqDsp::ParamSpec> v = [&]
            {
                std::vector<SqDsp::ParamSpec> out;
                auto keep = [] (const juce::String& t) { store.push_back (std::make_unique<juce::String> (t)); return store.back()->toRawUTF8(); };
                for (int i = 0; i < numOps; ++i)
                {
                    const juce::String n = juce::String (i + 1);
                    out.push_back ({ keep ("op" + n + "Wave"), keep ("Op " + n + " Wave"), 0, 5, 0, 1, "", 0 });
                    out.push_back ({ keep ("op" + n + "Ratio"), keep ("Op " + n + " Ratio"), 0.25f, 16, ratios[i], 0.5f, "", 2 });
                    out.push_back ({ keep ("op" + n + "Fine"), keep ("Op " + n + " Fine"), -100, 100, 0, 1, " ct", 0 });
                    out.push_back ({ keep ("op" + n + "Level"), keep ("Op " + n + " Level"), 0, 1, levels[i], 1, "", 2 });
                    out.push_back ({ keep ("op" + n + "Fb"), keep ("Op " + n + " Feedback"), 0, 1, 0, 1, "", 2 });
                    out.push_back ({ keep ("op" + n + "A"), keep ("Op " + n + " Attack"), 0.001f, 4, 0.004f, 0.3f, " s", 3 });
                }
                for (int i = 0; i < numOps; ++i)
                {
                    const juce::String n = juce::String (i + 1);
                    out.push_back ({ keep ("op" + n + "D"), keep ("Op " + n + " Decay"), 0.001f, 8, i == 0 ? 1.5f : 0.6f, 0.3f, " s", 2 });
                    out.push_back ({ keep ("op" + n + "S"), keep ("Op " + n + " Sustain"), 0, 1, i == 0 ? 0.7f : 0.3f, 1, "", 2 });
                    out.push_back ({ keep ("op" + n + "R"), keep ("Op " + n + " Release"), 0.001f, 8, 0.4f, 0.3f, " s", 2 });
                    out.push_back ({ keep ("op" + n + "Vel"), keep ("Op " + n + " Velocity"), 0, 1, 0.5f, 1, "", 2 });
                    out.push_back ({ keep ("op" + n + "Key"), keep ("Op " + n + " Key Scale"), 0, 1, 0.2f, 1, "", 2 });
                }
                return out;
            }();
            return v;
        }
        /** Reads the operator block from the bank; the first op param index is 'first' (6 per op), the env block starts at 'envFirst' (5 per op). */
        void readOps (int first, int envFirst)
        {
            for (int i = 0; i < numOps; ++i)
            {
                auto& o = settings.op[i]; const size_t b = (size_t) (first + i * 6), e = (size_t) (envFirst + i * 5);
                o.wave = (int) std::lround (bank[b]); o.ratio = bank[b + 1]; o.fine = bank[b + 2]; o.level = bank[b + 3]; o.feedback = bank[b + 4]; o.a = bank[b + 5];
                o.d = bank[e]; o.s = bank[e + 1]; o.r = bank[e + 2]; o.vel = bank[e + 3]; o.keyScale = bank[e + 4];
            }
        }

        void renderFx (float* l, float* r, int n, float chorusMix, float delayMix, float delayBeats, float delayFb, float reverbMix, float reverbSize, float width, float levelDb)
        {
            const float dt = juce::jlimit (0.02f, 2.0f, (float) (60.0 / Base::bpm) * delayBeats);
            for (int i = 0; i < n; ++i)
            {
                if (chorusMix > 0.001f) chorus.process (l[i], r[i], 0.5f, 0.7f, chorusMix);
                if (delayMix > 0.001f) delay.process (l[i], r[i], dt, dt * 0.75f, delayFb, 5000.0f, delayMix, true);
                if (width != 1.0f) { const float mid = 0.5f * (l[i] + r[i]), side = 0.5f * (l[i] - r[i]) * width; l[i] = mid + side; r[i] = mid - side; }
            }
            reverb.set (reverbSize, 0.4f, 1.0f, reverbMix); reverb.process (l, r, n);
            const float g = juce::Decibels::decibelsToGain (levelDb);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }

        Fm6Settings settings;
    };

    //==========================================================================
    // Hexwing
    class SqHexwingPlugin : public Fm6Synth<32>
    {
    public:
        // op block: 6 x (wave, ratio, fine, level, fb, attack) = 36, then env block 6 x (D, S, R, vel, key) = 30
        enum P { opFirst = 0, envFirst = opFirst + numOps * 6, algorithmP = envFirst + numOps * 5, modDepth,
                 fType, cutoff, res, fEnv, fKey, fA, fD, fS, fR,
                 lfo1Shape, lfo1Rate, lfo1Sync, lfo1Pitch, lfo1Index, lfo2Shape, lfo2Rate, lfo2Sync, lfo2Amp, lfo2Cut,
                 uniDetune, uniSpread, glide, drive, driveType, pan,
                 fxChorus, fxDelay, fxDelayTime, fxDelayFb, fxReverb, fxSize, width, macro1, macro2, macro3, macro4, level, numParams };

        static constexpr const char* xmlTypeName = "sqmHexwing";
        static const char* getPluginName()   { return NEEDS_TRANS("Hexwing"); }
        juce::String getName() const override { return "Hexwing"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Hex"; }
        juce::String getSelectableDescription() override { return "Hexwing six-operator FM synth"; }

        SqHexwingPlugin (te::PluginCreationInfo info) : Fm6Synth<32> (info)
        {
            const float levels[numOps] = { 1.0f, 0.55f, 0, 0, 0, 0 }, ratios[numOps] = { 1, 2, 1, 3, 1, 5 };
            auto specs = opSpecs (levels, ratios);
            std::vector<SqDsp::ParamSpec> rest = {
                { "algorithm", "Algorithm", 0, 7, 0, 1, "", 0 }, { "modDepth", "Mod Depth", 0, 2, 1, 1, "", 2 },
                { "fType", "Filter Type", 0, 6, 0, 1, "", 0 }, { "cutoff", "Cutoff", 20, 20000, 20000, 0.3f, " Hz", 0 }, { "res", "Resonance", 0, 1, 0, 1, "", 2 }, { "fEnv", "Filter Env", -1, 1, 0, 1, "", 2 }, { "fKey", "Key Track", 0, 1, 0.3f, 1, "", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 6, 0.4f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.0f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "lfo1Shape", "LFO 1 Shape", 0, 5, 0, 1, "", 0 }, { "lfo1Rate", "LFO 1 Rate", 0.02f, 30, 5.0f, 0.4f, " Hz", 2 }, { "lfo1Sync", "LFO 1 Sync", 0, 1, 0, 1, "", 0 }, { "lfo1Pitch", "LFO1 > Pitch", 0, 1, 0, 1, "", 2 }, { "lfo1Index", "LFO1 > Index", 0, 1, 0, 1, "", 2 },
                { "lfo2Shape", "LFO 2 Shape", 0, 5, 1, 1, "", 0 }, { "lfo2Rate", "LFO 2 Rate", 0.02f, 30, 0.4f, 0.4f, " Hz", 2 }, { "lfo2Sync", "LFO 2 Sync", 0, 1, 0, 1, "", 0 }, { "lfo2Amp", "LFO2 > Amp", 0, 1, 0, 1, "", 2 }, { "lfo2Cut", "LFO2 > Cut", -1, 1, 0, 1, "", 2 },
                { "uniDetune", "Uni Detune", 0, 50, 8, 1, " ct", 0 }, { "uniSpread", "Uni Spread", 0, 1, 0.5f, 1, "", 2 }, { "glide", "Glide", 0, 2, 0.05f, 0.4f, " s", 3 }, { "drive", "Drive", 0, 1, 0, 1, "", 2 }, { "driveType", "Drive Type", 0, 3, 0, 1, "", 0 }, { "pan", "Pan", -1, 1, 0, 1, "", 2 },
                { "fxChorus", "Chorus", 0, 1, 0, 1, "", 2 }, { "fxDelay", "Delay", 0, 1, 0, 1, "", 2 }, { "fxDelayTime", "Delay Time", 0.125f, 2, 0.5f, 1, " beats", 3 }, { "fxDelayFb", "Delay FB", 0, 0.95f, 0.35f, 1, "", 2 }, { "fxReverb", "Reverb", 0, 1, 0, 1, "", 2 }, { "fxSize", "Reverb Size", 0, 1, 0.5f, 1, "", 2 }, { "width", "Width", 0, 2, 1, 1, "", 2 },
                { "macro1", "Macro 1", 0, 1, 0, 1, "", 2 }, { "macro2", "Macro 2", 0, 1, 0, 1, "", 2 }, { "macro3", "Macro 3", 0, 1, 0, 1, "", 2 }, { "macro4", "Macro 4", 0, 1, 0, 1, "", 2 },
                { "level", "Level", -24, 6, -6, 1, " dB", 1 } };
            specs.insert (specs.end(), rest.begin(), rest.end());
            jassert ((int) specs.size() == numParams);
            bank.create (*this, specs);
        }
        ~SqHexwingPlugin() override {}
        float macroValue (int i) const override   { return bank[(size_t) (macro1 + i)]; }

        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            readOps (opFirst, envFirst);
            s.algorithm = (int) std::lround (bank[algorithmP]); s.modDepth = bank[modDepth];
            s.fType = (FilterType) (int) std::lround (bank[fType]); s.cutoff = bank[cutoff]; s.res = bank[res]; s.fEnv = bank[fEnv]; s.fKey = bank[fKey];
            s.fAdsr[0] = bank[fA]; s.fAdsr[1] = bank[fD]; s.fAdsr[2] = bank[fS]; s.fAdsr[3] = bank[fR];
            lfoShape1 = (LfoShape) (int) std::lround (bank[lfo1Shape]); lfoShape2 = (LfoShape) (int) std::lround (bank[lfo2Shape]);
            setLfo (lfo1, bank[lfo1Rate], bank[lfo1Sync] > 0.5f, SqVenomPlugin::syncBeats (bank[lfo1Rate])); setLfo (lfo2, bank[lfo2Rate], bank[lfo2Sync] > 0.5f, SqVenomPlugin::syncBeats (bank[lfo2Rate]));
            s.lfo1Pitch = bank[lfo1Pitch] * 2.0f; s.lfo1Index = bank[lfo1Index]; s.lfo2Amp = bank[lfo2Amp]; s.lfo2Cut = bank[lfo2Cut];
            s.unisonCents = bank[uniDetune]; s.unisonSpread = bank[uniSpread]; glideTime = bank[glide]; s.drive = bank[drive]; s.driveType = (int) std::lround (bank[driveType]); s.pan = bank[pan]; s.gain = 0.7f;
        }
        void renderMaster (float* l, float* r, int n) override
        {
            renderFx (l, r, n, bank[fxChorus], bank[fxDelay], bank[fxDelayTime], bank[fxDelayFb], bank[fxReverb], bank[fxSize], bank[width], bank[level]);
        }
    };

    //==========================================================================
    // Toxin FM - six operators + a 16-step sequencer
    class SqToxinPlugin : public Fm6Synth<16>
    {
    public:
        static constexpr int seqSteps = 16;
        enum P { opFirst = 0, envFirst = opFirst + numOps * 6, algorithmP = envFirst + numOps * 5, modDepth,
                 fType, cutoff, res, fEnv, fKey, fA, fD, fS, fR,
                 seqOn, seqRate, seqSwing, seqGate, seqAccent, seqOctave, stepNote = seqOctave + 1, stepOn = stepNote + seqSteps, stepAcc = stepOn + seqSteps, stepSlide = stepAcc + seqSteps,
                 lfo1Rate = stepSlide + seqSteps, lfo1Pitch, lfo1Index, glide, drive, driveType, fxDelay, fxDelayTime, fxDelayFb, fxReverb, fxSize, macro1, macro2, macro3, macro4, level, numParams };

        static constexpr const char* xmlTypeName = "sqmToxin";
        static const char* getPluginName()   { return NEEDS_TRANS("Toxin FM"); }
        juce::String getName() const override { return "Toxin FM"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Toxn"; }
        juce::String getSelectableDescription() override { return "Toxin FM six-operator synth with sequencer"; }

        SqToxinPlugin (te::PluginCreationInfo info) : Fm6Synth<16> (info)
        {
            const float levels[numOps] = { 1.0f, 0.6f, 0, 0, 0, 0 }, ratios[numOps] = { 1, 1, 2, 3, 1, 7 };
            auto specs = opSpecs (levels, ratios);
            std::vector<SqDsp::ParamSpec> mid = {
                { "algorithm", "Algorithm", 0, 7, 0, 1, "", 0 }, { "modDepth", "Mod Depth", 0, 2, 1, 1, "", 2 },
                { "fType", "Filter Type", 0, 6, 6, 1, "", 0 }, { "cutoff", "Cutoff", 20, 20000, 4000, 0.3f, " Hz", 0 }, { "res", "Resonance", 0, 1, 0.15f, 1, "", 2 }, { "fEnv", "Filter Env", -1, 1, 0.4f, 1, "", 2 }, { "fKey", "Key Track", 0, 1, 0.3f, 1, "", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 6, 0.25f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.1f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.2f, 0.3f, " s", 2 },
                { "seqOn", "Sequencer", 0, 1, 0, 1, "", 0 }, { "seqRate", "Seq Rate", 0, 3, 1, 1, "", 0 }, { "seqSwing", "Seq Swing", 0, 100, 0, 1, " %", 0 }, { "seqGate", "Seq Gate", 0.05f, 1, 0.5f, 1, "", 2 }, { "seqAccent", "Accent", 0, 1, 0.5f, 1, "", 2 }, { "seqOctave", "Seq Octave", -2, 2, 0, 1, "", 0 } };
            specs.insert (specs.end(), mid.begin(), mid.end());
            auto& steps = stepSpecs ("toxin", seqSteps, -24, [] (int kind, int i) -> float
            {
                if (kind == 0) return (float) ((i * 7) % 12 > 7 ? (i * 7) % 12 - 12 : 0);
                if (kind == 1) return ((i % 4 == 0) || i == 6 || i == 10 || i == 13) ? 1.0f : 0.0f;
                return 0.0f;
            });
            specs.insert (specs.end(), steps.begin(), steps.end());
            std::vector<SqDsp::ParamSpec> rest = {
                { "lfo1Rate", "LFO Rate", 0.02f, 30, 5.0f, 0.4f, " Hz", 2 }, { "lfo1Pitch", "LFO > Pitch", 0, 1, 0, 1, "", 2 }, { "lfo1Index", "LFO > Index", 0, 1, 0, 1, "", 2 },
                { "glide", "Glide", 0, 2, 0.04f, 0.4f, " s", 3 }, { "drive", "Drive", 0, 1, 0.15f, 1, "", 2 }, { "driveType", "Drive Type", 0, 3, 1, 1, "", 0 },
                { "fxDelay", "Delay", 0, 1, 0, 1, "", 2 }, { "fxDelayTime", "Delay Time", 0.125f, 2, 0.75f, 1, " beats", 3 }, { "fxDelayFb", "Delay FB", 0, 0.95f, 0.4f, 1, "", 2 }, { "fxReverb", "Reverb", 0, 1, 0, 1, "", 2 }, { "fxSize", "Reverb Size", 0, 1, 0.5f, 1, "", 2 },
                { "macro1", "Macro 1", 0, 1, 0, 1, "", 2 }, { "macro2", "Macro 2", 0, 1, 0, 1, "", 2 }, { "macro3", "Macro 3", 0, 1, 0, 1, "", 2 }, { "macro4", "Macro 4", 0, 1, 0, 1, "", 2 },
                { "level", "Level", -24, 6, -6, 1, " dB", 1 } };
            specs.insert (specs.end(), rest.begin(), rest.end());
            jassert ((int) specs.size() == numParams);
            bank.create (*this, specs);
        }
        ~SqToxinPlugin() override {}
        float macroValue (int i) const override   { return bank[(size_t) (macro1 + i)]; }

        // the sequencer: when on and a key is held, steps retrigger the voice at the step pitch (held note + step offset)
        int currentStep() const   { return seqStep; }
        bool sequencerOn() const  { return bank[seqOn] > 0.5f; }

        void noteOn (int note, float vel) override
        {
            if (! sequencerOn()) { Base::noteOn (note, vel); return; }
            seqHeld.removeAllInstancesOf (note); seqHeld.add (note); seqVel = vel;
            if (seqHeld.size() == 1 && ! transportPlaying) { seqFreeBeat = 0.0; seqStep = -1; }   // a fresh press restarts the free-running clock
        }
        void noteOff (int note) override
        {
            if (! sequencerOn()) { Base::noteOff (note); return; }
            seqHeld.removeAllInstancesOf (note);
            if (seqHeld.isEmpty()) { for (auto& v : voices) if (v.active && ! v.releasing) v.release(); seqStep = -1; }
        }

        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            readOps (opFirst, envFirst);
            s.algorithm = (int) std::lround (bank[algorithmP]); s.modDepth = bank[modDepth];
            s.fType = (FilterType) (int) std::lround (bank[fType]); s.cutoff = bank[cutoff]; s.res = bank[res]; s.fEnv = bank[fEnv]; s.fKey = bank[fKey];
            s.fAdsr[0] = bank[fA]; s.fAdsr[1] = bank[fD]; s.fAdsr[2] = bank[fS]; s.fAdsr[3] = bank[fR];
            setLfo (lfo1, bank[lfo1Rate], false, 1); s.lfo1Pitch = bank[lfo1Pitch] * 2.0f; s.lfo1Index = bank[lfo1Index];
            glideTime = bank[glide]; s.drive = bank[drive]; s.driveType = (int) std::lround (bank[driveType]); s.gain = 0.7f; s.unisonCents = 6; s.unisonSpread = 0.4f;
            runSequencer();
        }

        void runSequencer()
        {
            if (! sequencerOn() || seqHeld.isEmpty()) return;
            static const double rates[4] = { 0.5, 0.25, 0.125, 1.0 / 6.0 };   // 1/8, 1/16, 1/32, 1/16 triplet in beats
            const double stepBeats = rates[juce::jlimit (0, 3, (int) std::lround (bank[seqRate]))];
            double beat;
            if (transportPlaying) beat = chunkBeat();
            else { beat = seqFreeBeat; seqFreeBeat += (double) chunk / sr * bpm / 60.0; }
            const double swing = bank[seqSwing] / 100.0 * 0.33;
            double pos = beat / stepBeats;
            int step = (int) std::floor (pos) ; double frac = pos - std::floor (pos);
            // swing: odd steps start late
            if ((step & 1) == 0 && frac >= 1.0 - swing) { /* still the even step, held longer */ }
            else if ((step & 1) == 1 && frac < swing) { step -= 1; frac = 1.0; }
            const int idx = ((step % seqSteps) + seqSteps) % seqSteps;
            const float gateLen = bank[seqGate];
            if (step != seqStep)
            {
                seqStep = step;
                const bool on = bank[(size_t) (stepOn + idx)] > 0.5f;
                const bool slide = bank[(size_t) (stepSlide + idx)] > 0.5f, accent = bank[(size_t) (stepAcc + idx)] > 0.5f;
                const int base = seqHeld.getLast() + 12 * (int) std::lround (bank[seqOctave]);
                const int note = juce::jlimit (0, 127, base + (int) std::lround (bank[(size_t) (stepNote + idx)]));
                if (on)
                {
                    const float vel = juce::jlimit (0.05f, 1.0f, seqVel * (accent ? 1.0f : 1.0f - bank[seqAccent] * 0.6f));
                    Fm6Voice* sounding = nullptr; for (auto& v : voices) if (v.active && ! v.releasing) sounding = &v;
                    if (slide && sounding != nullptr) { sounding->retarget (note, glideTime); sounding->velocity = vel; }
                    else
                    {
                        for (auto& v : voices) if (v.active && ! v.releasing) v.release();
                        if (auto* v = allocateVoice()) { v->random = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; v->bend = pitchBend; v->unisonGain = 1.0f; v->unisonDetune = 0; v->unisonPan = 0; v->start (note, vel, 0.0f, 0.0f); }
                    }
                    lastNoteNumber = note; gateOpen = true;
                }
                else gateOpen = false;
            }
            else if (gateOpen && frac > gateLen && ! (bank[(size_t) (stepSlide + ((idx + 1) % seqSteps))] > 0.5f && bank[(size_t) (stepOn + ((idx + 1) % seqSteps))] > 0.5f))
            {
                for (auto& v : voices) if (v.active && ! v.releasing) v.release();
                gateOpen = false;
            }
        }

        void renderMaster (float* l, float* r, int n) override
        {
            renderFx (l, r, n, 0.0f, bank[fxDelay], bank[fxDelayTime], bank[fxDelayFb], bank[fxReverb], bank[fxSize], 1.0f, bank[level]);
        }

    private:
        juce::Array<int> seqHeld; float seqVel = 1.0f; int seqStep = -1; double seqFreeBeat = 0; bool gateOpen = false;
    };

    //==========================================================================
    /** The 16-step sequencer shared by Toxin FM and Larva Bass: keys hold the root, steps retrigger the voice. */
    struct StepSeq
    {
        juce::Array<int> held; float vel = 1.0f; int step = -1; double freeBeat = 0; bool gateOpen = false;
        template <typename ReadStep, typename Trigger, typename GateOff>
        void advance (double beat, bool useBeat, double chunkBeats, double stepBeats, double swing, float gateLen, int numSteps, ReadStep read, Trigger trigger, GateOff gateOff)
        {
            if (held.isEmpty()) return;
            double b;
            if (useBeat) b = beat; else { b = freeBeat; freeBeat += chunkBeats; }
            const double pos = b / stepBeats;
            int st = (int) std::floor (pos); double frac = pos - std::floor (pos);
            if ((st & 1) == 1 && frac < swing) { st -= 1; frac = 1.0; }   // swing: odd steps start late
            const int idx = ((st % numSteps) + numSteps) % numSteps;
            bool on, slide, accent; int offset;
            read (idx, on, slide, accent, offset);
            if (st != step)
            {
                step = st;
                if (on) { trigger (held.getLast() + offset, accent, slide); gateOpen = true; }
                else gateOpen = false;
            }
            else if (gateOpen && frac > gateLen)
            {
                bool nOn, nSlide, nAcc; int nOff; read ((idx + 1) % numSteps, nOn, nSlide, nAcc, nOff);
                if (! (nOn && nSlide)) { gateOff(); gateOpen = false; }
            }
        }
    };

    //==========================================================================
    // Lantern - the vintage polysynth
    class SqLanternPlugin : public SubtractiveSynth<16>
    {
    public:
        enum P { o1Wave, o1Level, o1Oct, o1Pw, o2Wave, o2Level, o2Semi, o2Fine, o2Pw, o2Sync, subLevel, noiseLevel, drift, age, era,
                 fType, cutoff, res, fEnv, fKey, aA, aD, aS, aR, fA, fD, fS, fR, lfoShape, lfoRate, lfoPitch, lfoCut, lfoPw, lfoDelay,
                 ensemble, ensRate, vintage, glide, uniDetune, fxDelay, fxDelayTime, fxDelayFb, fxReverb, fxSize, macro1, macro2, macro3, macro4, level, numParams };
        static constexpr const char* xmlTypeName = "sqmLantern";
        static const char* getPluginName()   { return NEEDS_TRANS("Lantern"); }
        juce::String getName() const override { return "Lantern"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Lntn"; }
        juce::String getSelectableDescription() override { return "Lantern vintage polysynth"; }
        SqLanternPlugin (te::PluginCreationInfo info) : SubtractiveSynth<16> (info)
        {
            bank.create (*this, {
                { "o1Wave", "Osc 1 Wave", 0, 4, 2, 1, "", 0 }, { "o1Level", "Osc 1 Level", 0, 1, 0.8f, 1, "", 2 }, { "o1Oct", "Osc 1 Octave", -2, 2, 0, 1, "", 0 }, { "o1Pw", "Osc 1 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 },
                { "o2Wave", "Osc 2 Wave", 0, 4, 2, 1, "", 0 }, { "o2Level", "Osc 2 Level", 0, 1, 0.6f, 1, "", 2 }, { "o2Semi", "Osc 2 Semi", -24, 24, 0, 1, " st", 0 }, { "o2Fine", "Osc 2 Fine", -100, 100, 6, 1, " ct", 0 }, { "o2Pw", "Osc 2 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 }, { "o2Sync", "Osc 2 Sync", 0, 1, 0, 1, "", 0 },
                { "subLevel", "Sub", 0, 1, 0.2f, 1, "", 2 }, { "noiseLevel", "Noise", 0, 1, 0, 1, "", 2 }, { "drift", "Drift", 0, 1, 0.35f, 1, "", 2 }, { "age", "Age", 0, 1, 0.3f, 1, "", 2 }, { "era", "Era", 0, 2, 0, 1, "", 0 },
                { "fType", "Filter", 0, 1, 0, 1, "", 0 }, { "cutoff", "Cutoff", 20, 20000, 1600, 0.3f, " Hz", 0 }, { "res", "Resonance", 0, 1, 0.25f, 1, "", 2 }, { "fEnv", "Filter Env", -1, 1, 0.5f, 1, "", 2 }, { "fKey", "Key Track", 0, 1, 0.5f, 1, "", 2 },
                { "aA", "Attack", 0.001f, 4, 0.01f, 0.3f, " s", 3 }, { "aD", "Decay", 0.001f, 6, 0.5f, 0.3f, " s", 2 }, { "aS", "Sustain", 0, 1, 0.7f, 1, "", 2 }, { "aR", "Release", 0.001f, 8, 0.5f, 0.3f, " s", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.01f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 6, 0.6f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.3f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.5f, 0.3f, " s", 2 },
                { "lfoShape", "LFO Shape", 0, 5, 0, 1, "", 0 }, { "lfoRate", "LFO Rate", 0.02f, 20, 4.5f, 0.4f, " Hz", 2 }, { "lfoPitch", "LFO > Pitch", 0, 1, 0, 1, "", 2 }, { "lfoCut", "LFO > Cutoff", -1, 1, 0, 1, "", 2 }, { "lfoPw", "LFO > Width", 0, 1, 0, 1, "", 2 }, { "lfoDelay", "LFO Delay", 0, 3, 0, 1, " s", 2 },
                { "ensemble", "Ensemble", 0, 1, 0.4f, 1, "", 2 }, { "ensRate", "Ens Rate", 0.1f, 3, 0.6f, 1, " Hz", 2 }, { "vintage", "Vintage Mode", 0, 1, 1, 1, "", 0 }, { "glide", "Glide", 0, 2, 0, 0.4f, " s", 3 }, { "uniDetune", "Uni Detune", 0, 50, 10, 1, " ct", 0 },
                { "fxDelay", "Delay", 0, 1, 0, 1, "", 2 }, { "fxDelayTime", "Delay Time", 0.125f, 2, 0.75f, 1, " beats", 3 }, { "fxDelayFb", "Delay FB", 0, 0.95f, 0.35f, 1, "", 2 }, { "fxReverb", "Reverb", 0, 1, 0.1f, 1, "", 2 }, { "fxSize", "Reverb Size", 0, 1, 0.6f, 1, "", 2 },
                { "macro1", "Macro 1", 0, 1, 0, 1, "", 2 }, { "macro2", "Macro 2", 0, 1, 0, 1, "", 2 }, { "macro3", "Macro 3", 0, 1, 0, 1, "", 2 }, { "macro4", "Macro 4", 0, 1, 0, 1, "", 2 },
                { "level", "Level", -24, 6, -6, 1, " dB", 1 } });
        }
        ~SqLanternPlugin() override {}
        float macroValue (int i) const override   { return bank[(size_t) (macro1 + i)]; }
        static juce::StringArray eraNames()   { return { "'72 WARM", "'78 CLASSIC", "'83 TIGHT" }; }

        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            const int era = (int) std::lround (bank[SqLanternPlugin::era]);
            const float eraDrift = era == 0 ? 1.4f : era == 1 ? 1.0f : 0.5f;
            s.osc[0] = { (Wave) juce::jlimit (0, 4, (int) std::lround (bank[o1Wave])), bank[o1Level], bank[o1Oct] * 12.0f, 0, bank[o1Pw], false, 0, 0 };
            s.osc[1] = { (Wave) juce::jlimit (0, 4, (int) std::lround (bank[o2Wave])), bank[o2Level], bank[o2Semi], bank[o2Fine], bank[o2Pw], bank[o2Sync] > 0.5f, 0, 0 };
            s.osc[2].level = 0;
            s.subLevel = bank[subLevel]; s.subOct = 1; s.subWave = Wave::square; s.noiseLevel = bank[noiseLevel]; s.ring = 0; s.routing = 0;
            s.drift = bank[drift] * 18.0f * eraDrift; s.filterDrift = bank[age] * 0.35f * eraDrift;
            s.f1 = bank[fType] > 0.5f ? FilterType::lp24 : FilterType::ladder; s.f1Cut = bank[cutoff]; s.f1Res = bank[res]; s.f1Env = bank[fEnv]; s.f1Key = bank[fKey];
            s.env[0][0] = bank[aA]; s.env[0][1] = bank[aD]; s.env[0][2] = bank[aS]; s.env[0][3] = bank[aR];
            s.env[1][0] = bank[fA]; s.env[1][1] = bank[fD]; s.env[1][2] = bank[fS]; s.env[1][3] = bank[fR];
            lfoShape1 = (LfoShape) (int) std::lround (bank[lfoShape]); lfoShape2 = lfoShape1;
            setLfo (lfo1, bank[lfoRate], false, 1); setLfo (lfo2, bank[lfoRate], false, 1);
            s.lfo1Pitch = bank[lfoPitch] * 1.0f; s.lfo1Cut = bank[lfoCut]; s.lfo2Pw = bank[lfoPw]; s.lfo2Amp = 0;
            s.unisonCents = bank[uniDetune]; glideTime = bank[glide]; s.drive = era == 0 ? 0.12f : 0.04f; s.driveType = 1; s.ampVel = 0.5f; s.pan = 0; s.gain = 0.8f;
            // vintage mode: 8 voices, the way the originals were
            voiceCap = bank[vintage] > 0.5f ? 8 : 16;
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float ens = bank[ensemble], rate = bank[ensRate];
            if (ens > 0.001f)
                for (int i = 0; i < n; ++i) { chorus.process (l[i], r[i], rate, 0.8f, ens * 0.7f); ensemble2.process (l[i], r[i], rate * 1.61f, 0.5f, ens * 0.5f); }
            renderMasterFx (l, r, n, 0.0f, bank[fxDelay], bank[fxDelayTime], bank[fxDelayFb], bank[fxReverb], bank[fxSize], 1.0f);
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
        void prepared (double rate) override   { ensemble2.prepare (rate); }
    private:
        Chorus ensemble2;
    };

    //==========================================================================
    // Lantern Exo - wavetable + analogue, dual filters, 32 voices
    class SqLanternExoPlugin : public SubtractiveSynth<32>
    {
    public:
        enum P { o1Wave, o1Morph, o1Level, o1Semi, o1Fine, o1Pw, o2Wave, o2Morph, o2Level, o2Semi, o2Fine, o2Pw, o2Sync, subLevel, subWave, noiseLevel, ring, fmAmt,
                 f1Type, f1Cut, f1Res, f1Env, f1Key, f2Type, f2Cut, f2Res, f2Env, routing,
                 aA, aD, aS, aR, fA, fD, fS, fR, mA, mD, mS, mR,
                 lfo1Shape, lfo1Rate, lfo1Sync, lfo1Pitch, lfo1Cut, lfo2Shape, lfo2Rate, lfo2Sync, lfo2Pw, lfo2Amp,
                 drift, uniDetune, uniSpread, glide, drive, driveType, ampVel, pan,
                 fxChorus, fxDelay, fxDelayTime, fxDelayFb, fxReverb, fxSize, width, macro1, macro2, macro3, macro4, level, numParams };
        static constexpr const char* xmlTypeName = "sqmLanternExo";
        static const char* getPluginName()   { return NEEDS_TRANS("Lantern Exo"); }
        juce::String getName() const override { return "Lantern Exo"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Exo"; }
        juce::String getSelectableDescription() override { return "Lantern Exo wavetable + analogue synth"; }
        SqLanternExoPlugin (te::PluginCreationInfo info) : SubtractiveSynth<32> (info)
        {
            bank.create (*this, {
                { "o1Wave", "Osc 1 Wave", 0, 6, 6, 1, "", 0 }, { "o1Morph", "Osc 1 Morph", 0, 1, 0.3f, 1, "", 2 }, { "o1Level", "Osc 1 Level", 0, 1, 0.8f, 1, "", 2 }, { "o1Semi", "Osc 1 Semi", -24, 24, 0, 1, " st", 0 }, { "o1Fine", "Osc 1 Fine", -100, 100, 0, 1, " ct", 0 }, { "o1Pw", "Osc 1 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 },
                { "o2Wave", "Osc 2 Wave", 0, 6, 2, 1, "", 0 }, { "o2Morph", "Osc 2 Morph", 0, 1, 0.0f, 1, "", 2 }, { "o2Level", "Osc 2 Level", 0, 1, 0.5f, 1, "", 2 }, { "o2Semi", "Osc 2 Semi", -24, 24, 0, 1, " st", 0 }, { "o2Fine", "Osc 2 Fine", -100, 100, -7, 1, " ct", 0 }, { "o2Pw", "Osc 2 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 }, { "o2Sync", "Osc 2 Sync", 0, 1, 0, 1, "", 0 },
                { "subLevel", "Sub Level", 0, 1, 0.2f, 1, "", 2 }, { "subWave", "Sub Wave", 0, 3, 0, 1, "", 0 }, { "noiseLevel", "Noise", 0, 1, 0, 1, "", 2 }, { "ring", "Ring Mod", 0, 1, 0, 1, "", 2 }, { "fmAmt", "FM Amount", 0, 1, 0, 1, "", 2 },
                { "f1Type", "F1 Type", 0, 6, 0, 1, "", 0 }, { "f1Cut", "Cutoff", 20, 20000, 3000, 0.3f, " Hz", 0 }, { "f1Res", "Resonance", 0, 1, 0.15f, 1, "", 2 }, { "f1Env", "Filter Env", -1, 1, 0.4f, 1, "", 2 }, { "f1Key", "Key Track", 0, 1, 0.4f, 1, "", 2 },
                { "f2Type", "F2 Type", 0, 6, 3, 1, "", 0 }, { "f2Cut", "F2 Cutoff", 20, 20000, 80, 0.3f, " Hz", 0 }, { "f2Res", "F2 Res", 0, 1, 0, 1, "", 2 }, { "f2Env", "F2 Env", -1, 1, 0, 1, "", 2 }, { "routing", "Routing", 0, 2, 0, 1, "", 0 },
                { "aA", "Amp Attack", 0.001f, 4, 0.006f, 0.3f, " s", 3 }, { "aD", "Amp Decay", 0.001f, 6, 0.4f, 0.3f, " s", 2 }, { "aS", "Amp Sustain", 0, 1, 0.8f, 1, "", 2 }, { "aR", "Amp Release", 0.001f, 8, 0.4f, 0.3f, " s", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 6, 0.5f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.3f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.4f, 0.3f, " s", 2 },
                { "mA", "Mod Attack", 0.001f, 4, 0.5f, 0.3f, " s", 3 }, { "mD", "Mod Decay", 0.001f, 6, 0.5f, 0.3f, " s", 2 }, { "mS", "Mod Sustain", 0, 1, 0.5f, 1, "", 2 }, { "mR", "Mod Release", 0.001f, 8, 0.5f, 0.3f, " s", 2 },
                { "lfo1Shape", "LFO 1 Shape", 0, 5, 0, 1, "", 0 }, { "lfo1Rate", "LFO 1 Rate", 0.02f, 30, 5.0f, 0.4f, " Hz", 2 }, { "lfo1Sync", "LFO 1 Sync", 0, 1, 0, 1, "", 0 }, { "lfo1Pitch", "LFO1 > Pitch", 0, 1, 0, 1, "", 2 }, { "lfo1Cut", "LFO1 > Cut", -1, 1, 0, 1, "", 2 },
                { "lfo2Shape", "LFO 2 Shape", 0, 5, 1, 1, "", 0 }, { "lfo2Rate", "LFO 2 Rate", 0.02f, 30, 0.3f, 0.4f, " Hz", 2 }, { "lfo2Sync", "LFO 2 Sync", 0, 1, 0, 1, "", 0 }, { "lfo2Pw", "LFO2 > Width", 0, 1, 0, 1, "", 2 }, { "lfo2Amp", "LFO2 > Amp", 0, 1, 0, 1, "", 2 },
                { "drift", "Drift", 0, 1, 0.15f, 1, "", 2 }, { "uniDetune", "Uni Detune", 0, 50, 10, 1, " ct", 0 }, { "uniSpread", "Uni Spread", 0, 1, 0.6f, 1, "", 2 }, { "glide", "Glide", 0, 2, 0.05f, 0.4f, " s", 3 }, { "drive", "Drive", 0, 1, 0.05f, 1, "", 2 }, { "driveType", "Drive Type", 0, 3, 1, 1, "", 0 }, { "ampVel", "Velocity", 0, 1, 0.5f, 1, "", 2 }, { "pan", "Pan", -1, 1, 0, 1, "", 2 },
                { "fxChorus", "Chorus", 0, 1, 0.15f, 1, "", 2 }, { "fxDelay", "Delay", 0, 1, 0, 1, "", 2 }, { "fxDelayTime", "Delay Time", 0.125f, 2, 0.5f, 1, " beats", 3 }, { "fxDelayFb", "Delay FB", 0, 0.95f, 0.35f, 1, "", 2 }, { "fxReverb", "Reverb", 0, 1, 0.1f, 1, "", 2 }, { "fxSize", "Reverb Size", 0, 1, 0.5f, 1, "", 2 }, { "width", "Width", 0, 2, 1.2f, 1, "", 2 },
                { "macro1", "Macro 1", 0, 1, 0, 1, "", 2 }, { "macro2", "Macro 2", 0, 1, 0, 1, "", 2 }, { "macro3", "Macro 3", 0, 1, 0, 1, "", 2 }, { "macro4", "Macro 4", 0, 1, 0, 1, "", 2 },
                { "level", "Level", -24, 6, -6, 1, " dB", 1 } });
        }
        ~SqLanternExoPlugin() override {}
        float macroValue (int i) const override   { return bank[(size_t) (macro1 + i)]; }
        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            s.osc[0] = { (Wave) juce::jlimit (0, 6, (int) std::lround (bank[o1Wave])), bank[o1Level], bank[o1Semi], bank[o1Fine], bank[o1Pw], false, 0, bank[o1Morph] };
            s.osc[1] = { (Wave) juce::jlimit (0, 6, (int) std::lround (bank[o2Wave])), bank[o2Level], bank[o2Semi], bank[o2Fine], bank[o2Pw], bank[o2Sync] > 0.5f, bank[fmAmt], bank[o2Morph] };
            s.osc[2].level = 0;
            s.subLevel = bank[subLevel]; s.subOct = 1; s.subWave = (Wave) (int) std::lround (bank[subWave]); s.noiseLevel = bank[noiseLevel]; s.ring = bank[ring];
            s.drift = bank[drift] * 12.0f; s.filterDrift = bank[drift] * 0.15f;
            s.f1 = (FilterType) (int) std::lround (bank[f1Type]); s.f1Cut = bank[f1Cut]; s.f1Res = bank[f1Res]; s.f1Env = bank[f1Env]; s.f1Key = bank[f1Key];
            s.f2 = (FilterType) (int) std::lround (bank[f2Type]); s.f2Cut = bank[f2Cut]; s.f2Res = bank[f2Res]; s.f2Env = bank[f2Env]; s.routing = (int) std::lround (bank[routing]);
            s.env[0][0] = bank[aA]; s.env[0][1] = bank[aD]; s.env[0][2] = bank[aS]; s.env[0][3] = bank[aR];
            s.env[1][0] = bank[fA]; s.env[1][1] = bank[fD]; s.env[1][2] = bank[fS]; s.env[1][3] = bank[fR];
            s.env[2][0] = bank[mA]; s.env[2][1] = bank[mD]; s.env[2][2] = bank[mS]; s.env[2][3] = bank[mR];
            lfoShape1 = (LfoShape) (int) std::lround (bank[lfo1Shape]); lfoShape2 = (LfoShape) (int) std::lround (bank[lfo2Shape]);
            setLfo (lfo1, bank[lfo1Rate], bank[lfo1Sync] > 0.5f, SqVenomPlugin::syncBeats (bank[lfo1Rate])); setLfo (lfo2, bank[lfo2Rate], bank[lfo2Sync] > 0.5f, SqVenomPlugin::syncBeats (bank[lfo2Rate]));
            s.lfo1Pitch = bank[lfo1Pitch] * 2.0f; s.lfo1Cut = bank[lfo1Cut]; s.lfo2Pw = bank[lfo2Pw]; s.lfo2Amp = bank[lfo2Amp];
            s.unisonCents = bank[uniDetune]; s.unisonSpread = bank[uniSpread]; glideTime = bank[glide];
            s.drive = bank[drive]; s.driveType = (int) std::lround (bank[driveType]); s.ampVel = bank[ampVel]; s.pan = bank[pan]; s.gain = 0.8f;
        }
        void renderMaster (float* l, float* r, int n) override
        {
            renderMasterFx (l, r, n, bank[fxChorus], bank[fxDelay], bank[fxDelayTime], bank[fxDelayFb], bank[fxReverb], bank[fxSize], bank[width]);
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    };

    //==========================================================================
    // Larva Bass - the acid bass with its sequencer
    struct LarvaVoice : public VoiceBase
    {
        Osc osc; Ladder ladder; SqDsp::SVF hp; float decayEnv = 0, accentEnv = 0; bool accent = false;
        struct S { float wave = 0, cutoff = 600, res = 0.6f, envMod = 0.6f, decay = 0.3f, accentAmt = 0.6f, drive = 0.2f, tone = 60, gain = 0.8f, velSens = 0.3f, sub = 0.0f; int driveType = 1; } s;
        double subPhase = 0;
        void start (int n, float vel, float glideSecs, float fromHz)
        {
            startBase (n, vel, glideSecs, fromHz);
            decayEnv = 1.0f; accentEnv = accent ? 1.0f : 0.0f;
            if (glideSecs <= 0.0005f) { osc.reset(); ladder.reset(); }
        }
        void restartEnvelopes() override   { VoiceBase::restartEnvelopes(); decayEnv = 1.0f; accentEnv = accent ? 1.0f : 0.0f; }
        inline void render (float* l, float* r, int n)
        {
            const double f = chunkFrequency (bend);
            osc.setFrequency (f, sr);
            hp.set (sr, s.tone, 0.7f);
            // 303 envelope: a decay that keeps a little floor while held, accent adds a fast, deeper sweep
            const float decCoef = std::exp (-1.0f / (float) (juce::jmax (0.03f, s.decay) * sr) * 3.0f);
            const float accCoef = std::exp (-1.0f / (float) (0.12f * sr) * 3.0f);
            const float relCoef = std::exp (-1.0f / (float) (0.02f * sr) * 3.0f);
            const float vg = (1.0f - s.velSens + s.velSens * velocity) * s.gain * unisonGain;
            for (int i = 0; i < n; ++i)
            {
                const float e = decayEnv, ae = accentEnv;
                const float cutMul = std::pow (2.0f, s.envMod * e * 4.5f + ae * s.accentAmt * 2.5f);
                ladder.set (sr, juce::jlimit (30.0f, 18000.0f, s.cutoff * cutMul), juce::jlimit (0.0f, 1.0f, s.res + ae * s.accentAmt * 0.25f));
                float x = s.wave < 0.5f ? osc.value (Wave::saw) * (1.0f - s.wave * 2.0f) + osc.value (Wave::square) * (s.wave * 2.0f) : osc.value (Wave::square);
                if (s.wave < 0.5f && s.wave > 0.0f) x *= 0.85f;
                subPhase += f * 0.5 / sr; if (subPhase >= 1.0) subPhase -= 1.0;
                x += s.sub * (subPhase < 0.5 ? 0.6f : -0.6f);
                osc.advance();
                float y = ladder.process (x * 0.7f, 1.0f + ae * 0.8f);
                y -= hp.lowpass (y);   // tone: high-pass by subtracting the low-passed signal
                if (s.drive > 0.001f) y = driveShape (y, s.drive + ae * 0.2f, s.driveType);
                const float amp = (releasing ? e : juce::jmax (e, 0.35f)) * (1.0f + ae * s.accentAmt * 0.8f);
                y *= amp * vg;
                l[i] += y; r[i] += y;
                decayEnv *= releasing ? relCoef : decCoef; accentEnv *= accCoef;
            }
            if (releasing && decayEnv < 0.0005f) active = false;
        }
    };

    class SqLarvaBassPlugin : public SynthPluginBase<LarvaVoice, 2>
    {
    public:
        static constexpr int seqSteps = 16;
        enum P { wave, cutoff, res, envMod, decay, accentAmt, slideTime, drive, driveType, tone, sub, velSens,
                 seqOn, seqRate, seqSwing, seqGate, seqOctave, stepNote, stepOn = stepNote + seqSteps, stepAcc = stepOn + seqSteps, stepSlide = stepAcc + seqSteps,
                 fxDelay = stepSlide + seqSteps, fxDelayTime, fxDelayFb, level, numParams };
        static constexpr const char* xmlTypeName = "sqmLarvaBass";
        static const char* getPluginName()   { return NEEDS_TRANS("Larva Bass"); }
        juce::String getName() const override { return "Larva Bass"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Larva"; }
        juce::String getSelectableDescription() override { return "Larva acid bass with sequencer"; }
        SqLarvaBassPlugin (te::PluginCreationInfo info) : SynthPluginBase<LarvaVoice, 2> (info)
        {
            std::vector<SqDsp::ParamSpec> specs = {
                { "wave", "Wave", 0, 1, 0, 1, "", 2 }, { "cutoff", "Cutoff", 40, 8000, 520, 0.35f, " Hz", 0 }, { "res", "Resonance", 0, 1, 0.65f, 1, "", 2 }, { "envMod", "Env Mod", 0, 1, 0.55f, 1, "", 2 }, { "decay", "Decay", 0.03f, 2, 0.3f, 0.4f, " s", 2 },
                { "accentAmt", "Accent", 0, 1, 0.6f, 1, "", 2 }, { "slideTime", "Slide Time", 0.02f, 0.6f, 0.1f, 1, " s", 2 }, { "drive", "Drive", 0, 1, 0.25f, 1, "", 2 }, { "driveType", "Drive Type", 0, 3, 1, 1, "", 0 }, { "tone", "Tone", 20, 400, 60, 0.5f, " Hz", 0 }, { "sub", "Sub", 0, 1, 0, 1, "", 2 }, { "velSens", "Velocity", 0, 1, 0.3f, 1, "", 2 },
                { "seqOn", "Sequencer", 0, 1, 0, 1, "", 0 }, { "seqRate", "Seq Rate", 0, 3, 1, 1, "", 0 }, { "seqSwing", "Seq Swing", 0, 100, 0, 1, " %", 0 }, { "seqGate", "Seq Gate", 0.05f, 1, 0.6f, 1, "", 2 }, { "seqOctave", "Seq Octave", -2, 2, 0, 1, "", 0 } };
            auto& steps = stepSpecs ("larva", seqSteps, -12, [] (int kind, int i) -> float
            {
                static const int defNotes[seqSteps] = { 0, 0, 12, 0, 3, 0, 0, 12, 0, 0, 7, 0, 5, 3, 0, 12 };
                static const bool defOn[seqSteps] = { 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1 }, defAcc[seqSteps] = { 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0 }, defSlide[seqSteps] = { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 };
                if (kind == 0) return (float) defNotes[i];
                return (kind == 1 ? defOn[i] : kind == 2 ? defAcc[i] : defSlide[i]) ? 1.0f : 0.0f;
            });
            specs.insert (specs.end(), steps.begin(), steps.end());
            std::vector<SqDsp::ParamSpec> rest = { { "fxDelay", "Delay", 0, 1, 0, 1, "", 2 }, { "fxDelayTime", "Delay Time", 0.125f, 2, 0.75f, 1, " beats", 3 }, { "fxDelayFb", "Delay FB", 0, 0.95f, 0.4f, 1, "", 2 }, { "level", "Level", -24, 6, -6, 1, " dB", 1 } };
            specs.insert (specs.end(), rest.begin(), rest.end());
            jassert ((int) specs.size() == numParams);
            bank.create (*this, specs);
            polyMode = 2; glideMode = 2; maxPoly = 1;
        }
        ~SqLarvaBassPlugin() override {}
        void resetProps (juce::UndoManager* um) override   { SynthPluginBase::resetProps (um); polyMode.setValue (2, um); glideMode.setValue (2, um); maxPoly.setValue (1, um); }
        bool sequencerOn() const   { return bank[seqOn] > 0.5f; }
        int currentStep() const    { return seq.step; }

        void noteOn (int note, float vel) override
        {
            if (! sequencerOn()) { for (auto& v : voices) v.accent = vel > 0.85f; SynthPluginBase::noteOn (note, vel); return; }
            seq.held.removeAllInstancesOf (note); seq.held.add (note); seq.vel = vel;
            if (seq.held.size() == 1 && ! transportPlaying) { seq.freeBeat = 0.0; seq.step = -1; }
        }
        void noteOff (int note) override
        {
            if (! sequencerOn()) { SynthPluginBase::noteOff (note); return; }
            seq.held.removeAllInstancesOf (note);
            if (seq.held.isEmpty()) { for (auto& v : voices) if (v.active && ! v.releasing) v.release(); seq.step = -1; }
        }
        void updateChunk (const ModValues&) override
        {
            for (auto& v : voices)
            {
                auto& s = v.s;
                s.wave = bank[wave]; s.cutoff = bank[cutoff]; s.res = bank[res]; s.envMod = bank[envMod]; s.decay = bank[decay]; s.accentAmt = bank[accentAmt]; s.drive = bank[drive]; s.driveType = (int) std::lround (bank[driveType]);
                s.tone = bank[tone]; s.sub = bank[sub]; s.velSens = bank[velSens]; s.gain = 0.9f;
            }
            glideTime = bank[slideTime];
            if (sequencerOn())
            {
                static const double rates[4] = { 0.5, 0.25, 0.125, 1.0 / 6.0 };
                seq.advance (chunkBeat(), transportPlaying, (double) chunk / sr * bpm / 60.0, rates[juce::jlimit (0, 3, (int) std::lround (bank[seqRate]))], bank[seqSwing] / 100.0 * 0.33, bank[seqGate], seqSteps,
                    [this] (int idx, bool& on, bool& slide, bool& acc, int& off) { on = bank[(size_t) (stepOn + idx)] > 0.5f; slide = bank[(size_t) (stepSlide + idx)] > 0.5f; acc = bank[(size_t) (stepAcc + idx)] > 0.5f; off = (int) std::lround (bank[(size_t) (stepNote + idx)]) + 12 * (int) std::lround (bank[seqOctave]); },
                    [this] (int note, bool accent, bool slide)
                    {
                        note = juce::jlimit (0, 127, note);
                        LarvaVoice* sounding = nullptr; for (auto& v : voices) if (v.active && ! v.releasing) sounding = &v;
                        if (slide && sounding != nullptr) { sounding->retarget (note, glideTime); sounding->accent = accent; if (accent) sounding->accentEnv = 1.0f; }
                        else
                        {
                            for (auto& v : voices) v.kill();
                            if (auto* v = allocateVoice()) { v->accent = accent; v->bend = pitchBend; v->unisonGain = 1.0f; v->start (note, seq.vel, 0.0f, 0.0f); }
                        }
                        lastNoteNumber = note;
                    },
                    [this] { for (auto& v : voices) if (v.active && ! v.releasing) v.release(); });
            }
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float dm = bank[fxDelay];
            if (dm > 0.001f) { const float dt = juce::jlimit (0.02f, 2.0f, (float) (60.0 / bpm) * bank[fxDelayTime]); for (int i = 0; i < n; ++i) delay.process (l[i], r[i], dt, dt * 0.75f, bank[fxDelayFb], 4000.0f, dm, true); }
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    private:
        StepSeq seq;
    };

    //==========================================================================
    // Thorn Bass - the electric bass model (plucked string + pickup + amp)
    struct ThornVoice : public VoiceBase
    {
        std::vector<float> line; int writePos = 0; float loopState = 0, dcState = 0; SqDsp::Delay pickupDelay; juce::uint32 rng = 99;
        float burst = 0, burstLen = 0, burstPos = 0, fretNoise = 0, lastNoteHzV = 0; SqDsp::SVF pickLp, ampLp, bodyBp;
        struct S { float pickPos = 0.2f, pickupPos = 0.3f, tone = 0.6f, decay = 0.7f, body = 0.3f, pickupBlend = 0.5f, ampDrive = 0.3f, ampTone = 3500, slap = 0, mute = 0, fretNoiseAmt = 0.3f, velSens = 0.7f, gain = 0.9f, roundRobin = 0.5f; } s;
        float r1 = 0, r2 = 0;   // per-note random (round robin)
        float ageSamples = 0, peakEnv = 0;
        void prepare (double rate)   { VoiceBase::prepare (rate); line.assign ((size_t) (rate / 25.0) + 8, 0.0f); pickupDelay.prepare ((int) (rate / 25.0)); }
        void start (int n, float vel, float glideSecs, float fromHz)
        {
            startBase (n, vel, glideSecs, fromHz);
            rng = rng * 1664525u + 1013904223u; r1 = (float) ((rng >> 8) & 0xffff) / 65535.0f; rng = rng * 1664525u + 1013904223u; r2 = (float) ((rng >> 8) & 0xffff) / 65535.0f;
            if (glideSecs <= 0.0005f) { std::fill (line.begin(), line.end(), 0.0f); loopState = 0; }
            // the pick: a short, pick-position-coloured noise burst (velocity sets how hard)
            burst = 1.0f; burstPos = 0; ageSamples = 0; peakEnv = 1.0f;
            burstLen = juce::jmax (12.0f, (float) (sr / juce::jmax (30.0, SqDsp::noteHz (n))) * (0.5f + s.roundRobin * 0.5f * r1));
            fretNoise = s.fretNoiseAmt * (0.4f + 0.6f * r2);
        }
        inline void render (float* l, float* r, int n)
        {
            const double f = chunkFrequency (bend);
            const float period = (float) (sr / juce::jmax (25.0, f));
            const int len = (int) line.size();
            const float vg = (1.0f - s.velSens + s.velSens * velocity);
            // loop damping: brighter strings ring longer; palm mute kills it fast; decay scales the loss
            const float dampCoef = juce::jlimit (0.05f, 0.95f, (1.0f - s.tone) * 0.6f + s.mute * 0.35f + 0.05f);
            // loop loss per sample from the wanted ring time: open string 0.4-4 s, palm mute down to 60 ms, high notes a little shorter
            float t60 = juce::jmax (0.05f, (0.4f + s.decay * 3.6f) * std::pow (1.0f - s.mute, 2.5f) * std::pow (0.5f, (float) (note - 40) / 24.0f));
            if (releasing) t60 = juce::jmin (t60, 0.12f);   // the finger lifts: the string damps quickly
            const float loss = std::exp (-6.9f / (t60 * (float) sr));
            pickLp.set (sr, 1200.0f + 7000.0f * s.tone + 3000.0f * s.slap, 0.7f);
            ampLp.set (sr, s.ampTone, 0.8f); bodyBp.set (sr, 110.0f + 60.0f * r1, 2.5f);
            const float pickupDelaySamples = juce::jmax (1.0f, period * s.pickupPos);
            const float pickDelaySamples = juce::jmax (1.0f, period * s.pickPos);
            for (int i = 0; i < n; ++i)
            {
                // excitation
                float ex = 0.0f;
                if (burst > 0.0005f)
                {
                    rng = rng * 1664525u + 1013904223u; const float nz = (float) ((rng >> 8) & 0xffffff) / 8388608.0f - 1.0f;
                    const float env = 1.0f - burstPos / burstLen;
                    ex = pickLp.lowpass (nz) * env * (0.9f + s.slap * 1.5f) * vg;
                    // pick position comb: the string is plucked at a point, so harmonics with a node there are missing
                    ex -= pickupDelay.read (pickDelaySamples) * 0.6f;
                    burstPos += 1.0f; if (burstPos >= burstLen) burst = 0.0f;
                    if (fretNoise > 0.001f) { ex += nz * fretNoise * 0.08f * std::exp (-burstPos / (0.004f * (float) sr)); }
                }
                pickupDelay.push (ex);
                // Karplus-Strong loop with a one-pole damper and fractional read
                const float rp = (float) writePos - period; int ri = (int) std::floor (rp); const float fr = rp - (float) ri;
                auto at = [&] (int k) { k %= len; if (k < 0) k += len; return line[(size_t) k]; };
                const float sOut = at (ri) + (at (ri + 1) - at (ri)) * fr;
                loopState += (sOut - loopState) * (1.0f - dampCoef);
                peakEnv = juce::jmax (std::abs (sOut), peakEnv * 0.99995f);   // string level follower (a plucked string is a pulse per period)
                float fbv = loopState * loss;
                if (s.slap > 0.001f) fbv = std::tanh (fbv * (1.0f + s.slap * 3.0f)) / (1.0f + s.slap * 2.0f);
                line[(size_t) writePos] = ex + fbv; writePos = (writePos + 1) % len;
                // pickup: a comb from the pickup position, blended between neck (dark) and bridge (bright)
                const float pu = sOut - at (ri - (int) pickupDelaySamples) * (0.4f + 0.5f * s.pickupBlend);
                float y = pu * (1.0f - s.pickupBlend * 0.4f) + sOut * 0.3f;
                y += bodyBp.bandpass (y) * s.body * 0.8f;
                dcState += (y - dcState) * 0.002f; y -= dcState;
                if (s.ampDrive > 0.001f) y = driveShape (y, s.ampDrive, 1);
                y = ampLp.lowpass (y) * s.gain * unisonGain * 1.6f;
                l[i] += y; r[i] += y;
            }

            // done when the loop has died away
            // done when the loop has died away (never before the pluck has travelled the string a few times)
            ageSamples += n;
            if (ageSamples > period * 4.0f && burst <= 0.0005f)
            {
                if (releasing || s.mute > 0.5f) { if (peakEnv < 0.0003f) active = false; }
                else if (peakEnv < 0.00005f) active = false;
            }
        }
        void release() override   { VoiceBase::release(); }
    };

    class SqThornBassPlugin : public SynthPluginBase<ThornVoice, 6>
    {
    public:
        enum P { pickPos, pickupPos, pickupBlend, tone, decay, body, ampDrive, ampTone, slap, mute, fretNoiseAmt, roundRobin, velSens, glide, fxChorus, level, numParams };
        static constexpr const char* xmlTypeName = "sqmThornBass";
        static const char* getPluginName()   { return NEEDS_TRANS("Thorn Bass"); }
        juce::String getName() const override { return "Thorn Bass"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Thorn"; }
        juce::String getSelectableDescription() override { return "Thorn electric bass model"; }
        SqThornBassPlugin (te::PluginCreationInfo info) : SynthPluginBase<ThornVoice, 6> (info)
        {
            bank.create (*this, {
                { "pickPos", "Pick Position", 0.05f, 0.5f, 0.2f, 1, "", 2 }, { "pickupPos", "Pickup Position", 0.05f, 0.5f, 0.3f, 1, "", 2 }, { "pickupBlend", "Neck / Bridge", 0, 1, 0.5f, 1, "", 2 },
                { "tone", "Tone", 0, 1, 0.6f, 1, "", 2 }, { "decay", "Decay", 0, 1, 0.7f, 1, "", 2 }, { "body", "Body", 0, 1, 0.3f, 1, "", 2 },
                { "ampDrive", "Amp Drive", 0, 1, 0.3f, 1, "", 2 }, { "ampTone", "Amp Tone", 500, 12000, 3500, 0.4f, " Hz", 0 }, { "slap", "Slap", 0, 1, 0, 1, "", 2 }, { "mute", "Palm Mute", 0, 1, 0, 1, "", 2 },
                { "fretNoiseAmt", "Fret Noise", 0, 1, 0.3f, 1, "", 2 }, { "roundRobin", "Round Robin", 0, 1, 0.5f, 1, "", 2 }, { "velSens", "Velocity", 0, 1, 0.7f, 1, "", 2 }, { "glide", "Slide", 0, 1, 0.06f, 0.5f, " s", 3 },
                { "fxChorus", "Chorus", 0, 1, 0, 1, "", 2 }, { "level", "Level", -24, 6, -4, 1, " dB", 1 } });
            polyMode = 0; maxPoly = 4; glideMode = 2;
        }
        ~SqThornBassPlugin() override {}
        void resetProps (juce::UndoManager* um) override   { SynthPluginBase::resetProps (um); maxPoly.setValue (4, um); glideMode.setValue (2, um); }
        void updateChunk (const ModValues&) override
        {
            for (auto& v : voices)
            {
                auto& s = v.s;
                s.pickPos = bank[pickPos]; s.pickupPos = bank[pickupPos]; s.pickupBlend = bank[pickupBlend]; s.tone = bank[tone]; s.decay = bank[decay]; s.body = bank[body];
                s.ampDrive = bank[ampDrive]; s.ampTone = bank[ampTone]; s.slap = bank[slap]; s.mute = bank[mute]; s.fretNoiseAmt = bank[fretNoiseAmt]; s.roundRobin = bank[roundRobin]; s.velSens = bank[velSens]; s.gain = 0.8f;
            }
            glideTime = bank[glide];
        }
        void renderMaster (float* l, float* r, int n) override
        {
            if (bank[fxChorus] > 0.001f) for (int i = 0; i < n; ++i) chorus.process (l[i], r[i], 0.4f, 0.5f, bank[fxChorus]);
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    };
}
