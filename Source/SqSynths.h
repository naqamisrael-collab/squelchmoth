#pragma once
// Squelchmoth 1.8 instrument shelf, part 1 - the subtractive family and the kick synth, all on SqSynthCore:
//
//  Venom       - modern subtractive: 2 morphing oscillators + sub + noise, two filters (series / parallel), 3 envelopes,
//                2 LFOs, 8-slot mod matrix, unison, trance gate, drive, chorus, delay, reverb
//  Mothsynth 2 - the flagship analogue-style synth: 3 oscillators with sync / ring / FM, sub, noise, dual filters,
//                3 envelopes, 2 LFOs, mod matrix, unison, FX
//  Sim Moth    - the simple three-oscillator synth: osc levels / pitches / PW, one filter, two envelopes, LFO
//  Mini Moth   - the one-screen, ultra-light synth: one osc + sub, filter, envelope, LFO, chorus
//  Thump       - kick synth: pitch sweep, body, click, sub layer, drive, tone
//
// Every one is original DSP, exposes everything to automation and Claude, and uses no CPU while silent.

#include "SqSynthCore.h"

namespace SqSynth
{
    //==========================================================================
    /** Chunk settings for the subtractive voice (computed once per chunk by the plugin). */
    struct SubSettings
    {
        struct OscS { Wave wave = Wave::saw; float level = 0.0f, semis = 0.0f, cents = 0.0f, pw = 0.5f; bool sync = false; float fm = 0.0f; float morph = 0.0f; };
        OscS osc[3];
        float drift = 0.0f;          // vintage oscillator drift, cents (random per voice, slowly wandering)
        float filterDrift = 0.0f;    // vintage filter tolerance, octaves (random per voice)
        float subLevel = 0.0f; int subOct = 1; Wave subWave = Wave::sine; float noiseLevel = 0.0f; float ring = 0.0f;
        FilterType f1 = FilterType::lp24; float f1Cut = 2000.0f, f1Res = 0.1f, f1Env = 0.0f, f1Key = 0.0f, f1Drive = 1.0f;
        FilterType f2 = FilterType::hp12; float f2Cut = 40.0f, f2Res = 0.0f; int routing = 0; float f2Env = 0.0f;   // 0 off, 1 series, 2 parallel
        float env[3][4] = { { 0.005f, 0.3f, 0.8f, 0.2f }, { 0.005f, 0.4f, 0.0f, 0.2f }, { 0.01f, 0.5f, 0.5f, 0.3f } };
        float ampVel = 0.6f, pan = 0.0f, gain = 0.5f, unisonCents = 12.0f, unisonSpread = 0.6f;
        float lfo1Pitch = 0.0f, lfo1Cut = 0.0f, lfo2Pw = 0.0f, lfo2Amp = 0.0f;
        float drive = 0.0f; int driveType = 0;
    };

    struct SubVoice : public VoiceBase
    {
        Osc osc[3], sub, noise; MultiFilter f1, f2; float ring = 0;
        // modulation offsets set by prepareVoice
        float mPitch = 0, mCut = 0, mRes = 0, mPw = 0, mAmp = 0, mPan = 0, mFm = 0, mMix = 0, mMorph = 0;
        float driftPhase = 0.0f;
        const SubSettings* set = nullptr;
        juce::uint32 seed = 1;

        void start (int n, float vel, float glideSecs, float fromHz)
        {
            startBase (n, vel, glideSecs, fromHz);
            for (auto& o : osc) o.reset (juce::Random::getSystemRandom().nextFloat() * 0.0f);
            sub.reset(); f1.reset(); f2.reset();
        }

        inline void render (float* l, float* r, int n)
        {
            if (set == nullptr) return;
            const auto& s = *set;
            for (int i = 0; i < 3; ++i) env[i].set (s.env[i][0], s.env[i][1], s.env[i][2], s.env[i][3]);
            // vintage drift: each voice wanders slowly around its own random offset
            driftPhase += 0.0007f * (1.0f + random * 0.3f); if (driftPhase > 6.2831853f) driftPhase -= 6.2831853f;
            const float driftCents = s.drift > 0.001f ? s.drift * (random * 0.6f + 0.4f * std::sin (driftPhase)) : 0.0f;
            const float uniCents = unisonDetune * s.unisonCents + driftCents;
            const double base = chunkFrequency (bend + mPitch + s.lfo1Pitch * mods.get ((int) ModSource::lfo1), uniCents);
            for (int i = 0; i < 3; ++i)
            {
                const double hz = base * std::pow (2.0, (s.osc[i].semis + s.osc[i].cents / 100.0) / 12.0);
                osc[i].setFrequency (hz, sr);
                osc[i].morph = juce::jlimit (0.0f, 1.0f, s.osc[i].morph + mMorph);
            }
            sub.setFrequency (base / (s.subOct == 2 ? 4.0 : 2.0), sr);
            const float e2 = env[1].level;
            const float keyTrack = (float) (note - 60) / 12.0f * s.f1Key;
            const float cutMul = std::pow (2.0f, s.f1Env * e2 * 5.0f + keyTrack + mCut * 4.0f + s.lfo1Cut * mods.get ((int) ModSource::lfo1) * 3.0f + s.filterDrift * random);
            f1.set (sr, s.f1, juce::jlimit (20.0f, 20000.0f, s.f1Cut * cutMul), juce::jlimit (0.0f, 1.0f, s.f1Res + mRes));
            if (s.routing != 0) f2.set (sr, s.f2, juce::jlimit (20.0f, 20000.0f, s.f2Cut * std::pow (2.0f, s.f2Env * e2 * 5.0f)), s.f2Res);
            const float pw = juce::jlimit (0.05f, 0.95f, s.osc[0].pw + mPw + s.lfo2Pw * mods.get ((int) ModSource::lfo2) * 0.4f);
            const float ampMod = juce::jlimit (0.0f, 2.0f, 1.0f + mAmp + s.lfo2Amp * mods.get ((int) ModSource::lfo2));
            const float velGain = 1.0f - s.ampVel + s.ampVel * velocity;
            const float panPos = juce::jlimit (-1.0f, 1.0f, s.pan + unisonPan * s.unisonSpread + mPan);
            const float gl = std::cos ((panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi), gr = std::sin ((panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            const float fm = s.osc[1].fm + mFm;

            for (int i = 0; i < n; ++i)
            {
                // oscillators: osc 2 can hard-sync to osc 1; osc 1 can be FM'd by osc 2
                float o2 = osc[1].value (s.osc[1].wave, s.osc[1].pw);
                if (fm > 0.0001f) osc[0].phase += (double) (o2 * fm * 0.05f);
                if (osc[0].phase < 0) osc[0].phase += 1.0; if (osc[0].phase >= 1.0) osc[0].phase -= 1.0;
                float o1 = osc[0].value (s.osc[0].wave, pw);
                float o3 = s.osc[2].level > 0.0001f ? osc[2].value (s.osc[2].wave, s.osc[2].pw) : 0.0f;
                const bool wrapped = osc[0].advance();
                if (s.osc[1].sync && wrapped) osc[1].reset (0.0);
                else osc[1].advance();
                osc[2].advance();
                float sb = s.subLevel > 0.0001f ? sub.value (s.subWave) : 0.0f; sub.advance();
                float nz = s.noiseLevel > 0.0001f ? noise.whiteNoise() : 0.0f;
                float mix = o1 * s.osc[0].level * (1.0f - s.ring) + o2 * s.osc[1].level * (1.0f - s.ring) + (o1 * o2) * s.ring + o3 * s.osc[2].level + sb * s.subLevel + nz * s.noiseLevel;
                mix *= 0.5f;
                if (s.drive > 0.001f) mix = driveShape (mix, s.drive, s.driveType);
                float y;
                if (s.routing == 1)      y = f2.process (f1.process (mix));
                else if (s.routing == 2) y = 0.5f * (f1.process (mix) + f2.process (mix));
                else                     y = f1.process (mix);
                const float a = env[0].next() * ampMod * velGain * s.gain * unisonGain;
                env[1].next(); env[2].next();
                y *= a;
                l[i] += y * gl; r[i] += y * gr;
            }
            if (! env[0].isActive()) active = false;
        }
    };

    //==========================================================================
    /** Shared plug-in body for the subtractive family. Subclasses define the parameter list and fill the settings. */
    template <int MaxVoices>
    class SubtractiveSynth : public SynthPluginBase<SubVoice, MaxVoices>
    {
    public:
        using Base = SynthPluginBase<SubVoice, MaxVoices>;
        using Base::bank; using Base::voices; using Base::sr; using Base::chorus; using Base::delay; using Base::reverb; using Base::quality;
        SubtractiveSynth (te::PluginCreationInfo i) : Base (i) { for (auto& v : voices) v.set = &settings; }

        void prepareVoice (SubVoice& v) override
        {
            const auto& m = v.mods;
            v.mPitch = Base::modFor ("pitch", m) * 24.0f;
            v.mCut   = Base::modFor ("cutoff", m) + m.get ((int) ModSource::modX) * 0.5f;
            v.mRes   = Base::modFor ("resonance", m) + m.get ((int) ModSource::modY) * 0.3f;
            v.mPw    = Base::modFor ("pw", m) * 0.45f;
            v.mAmp   = Base::modFor ("amp", m);
            v.mPan   = Base::modFor ("pan", m);
            v.mFm    = Base::modFor ("fm", m);
            v.mMorph = Base::modFor ("morph", m);
        }
        juce::StringArray modDestinations() const override   { return { "pitch", "cutoff", "resonance", "pw", "amp", "pan", "fm", "morph" }; }

        /** Master FX: tilt, chorus, delay, reverb from the common FX parameters (ids fxChorus, fxDelay, fxDelayTime, fxDelayFb, fxReverb, fxSize, width). */
        void renderMasterFx (float* l, float* r, int n, float chorusMix, float delayMix, float delayBeats, float delayFb, float reverbMix, float reverbSize, float width)
        {
            const float dt = juce::jlimit (0.02f, 2.0f, (float) (60.0 / Base::bpm) * delayBeats);
            for (int i = 0; i < n; ++i)
            {
                if (chorusMix > 0.001f) chorus.process (l[i], r[i], 0.6f, 0.7f, chorusMix);
                if (delayMix > 0.001f) delay.process (l[i], r[i], dt, dt * 0.75f, delayFb, 5000.0f, delayMix, true);
                if (width != 1.0f) { const float mid = 0.5f * (l[i] + r[i]), side = 0.5f * (l[i] - r[i]) * width; l[i] = mid + side; r[i] = mid - side; }
            }
            reverb.set (reverbSize, 0.4f, 1.0f, reverbMix);
            reverb.process (l, r, n);
        }

    protected:
        SubSettings settings;
    };

    //==========================================================================
    // Venom
    class SqVenomPlugin : public SubtractiveSynth<32>
    {
    public:
        enum P { o1Wave, o1Level, o1Semi, o1Fine, o1Pw, o2Wave, o2Level, o2Semi, o2Fine, o2Pw, o2Sync, subLevel, subOct, noiseLevel, ring, fmAmt,
                 f1Type, f1Cut, f1Res, f1Env, f1Key, f2Type, f2Cut, f2Res, routing,
                 aA, aD, aS, aR, fA, fD, fS, fR, mA, mD, mS, mR,
                 lfo1Shape, lfo1Rate, lfo1Sync, lfo1Pitch, lfo1Cut, lfo2Shape, lfo2Rate, lfo2Sync, lfo2Pw, lfo2Amp,
                 uniDetune, uniSpread, glide, drive, driveType, ampVel, pan,
                 gateOn, gateRate, gateDepth, gatePattern,
                 fxChorus, fxDelay, fxDelayTime, fxDelayFb, fxReverb, fxSize, width, macro1, macro2, macro3, macro4, level, numParams };

        static constexpr const char* xmlTypeName = "sqmVenom";
        static const char* getPluginName()   { return NEEDS_TRANS("Venom"); }
        juce::String getName() const override { return "Venom"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Vnm"; }
        juce::String getSelectableDescription() override { return "Venom subtractive synth"; }

        SqVenomPlugin (te::PluginCreationInfo info) : SubtractiveSynth<32> (info)
        {
            bank.create (*this, {
                { "o1Wave", "Osc 1 Wave", 0, 5, 2, 1, "", 0 }, { "o1Level", "Osc 1 Level", 0, 1, 0.8f, 1, "", 2 }, { "o1Semi", "Osc 1 Semi", -24, 24, 0, 1, " st", 0 }, { "o1Fine", "Osc 1 Fine", -100, 100, 0, 1, " ct", 0 }, { "o1Pw", "Osc 1 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 },
                { "o2Wave", "Osc 2 Wave", 0, 5, 2, 1, "", 0 }, { "o2Level", "Osc 2 Level", 0, 1, 0.6f, 1, "", 2 }, { "o2Semi", "Osc 2 Semi", -24, 24, 0, 1, " st", 0 }, { "o2Fine", "Osc 2 Fine", -100, 100, 7, 1, " ct", 0 }, { "o2Pw", "Osc 2 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 }, { "o2Sync", "Osc 2 Sync", 0, 1, 0, 1, "", 0 },
                { "subLevel", "Sub Level", 0, 1, 0.3f, 1, "", 2 }, { "subOct", "Sub Octave", 1, 2, 1, 1, "", 0 }, { "noiseLevel", "Noise", 0, 1, 0, 1, "", 2 }, { "ring", "Ring Mod", 0, 1, 0, 1, "", 2 }, { "fmAmt", "FM Amount", 0, 1, 0, 1, "", 2 },
                { "f1Type", "F1 Type", 0, 6, 0, 1, "", 0 }, { "f1Cut", "Cutoff", 20, 20000, 1800, 0.3f, " Hz", 0 }, { "f1Res", "Resonance", 0, 1, 0.2f, 1, "", 2 }, { "f1Env", "Filter Env", -1, 1, 0.5f, 1, "", 2 }, { "f1Key", "Key Track", 0, 1, 0.3f, 1, "", 2 },
                { "f2Type", "F2 Type", 0, 6, 3, 1, "", 0 }, { "f2Cut", "F2 Cutoff", 20, 20000, 60, 0.3f, " Hz", 0 }, { "f2Res", "F2 Res", 0, 1, 0, 1, "", 2 }, { "routing", "Routing", 0, 2, 0, 1, "", 0 },
                { "aA", "Amp Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "aD", "Amp Decay", 0.001f, 6, 0.3f, 0.3f, " s", 2 }, { "aS", "Amp Sustain", 0, 1, 0.8f, 1, "", 2 }, { "aR", "Amp Release", 0.001f, 8, 0.25f, 0.3f, " s", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 6, 0.35f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.1f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "mA", "Mod Attack", 0.001f, 4, 0.3f, 0.3f, " s", 3 }, { "mD", "Mod Decay", 0.001f, 6, 0.5f, 0.3f, " s", 2 }, { "mS", "Mod Sustain", 0, 1, 0.5f, 1, "", 2 }, { "mR", "Mod Release", 0.001f, 8, 0.4f, 0.3f, " s", 2 },
                { "lfo1Shape", "LFO 1 Shape", 0, 5, 0, 1, "", 0 }, { "lfo1Rate", "LFO 1 Rate", 0.02f, 30, 4.5f, 0.4f, " Hz", 2 }, { "lfo1Sync", "LFO 1 Sync", 0, 1, 0, 1, "", 0 }, { "lfo1Pitch", "LFO1 > Pitch", 0, 1, 0, 1, "", 2 }, { "lfo1Cut", "LFO1 > Cut", -1, 1, 0, 1, "", 2 },
                { "lfo2Shape", "LFO 2 Shape", 0, 5, 1, 1, "", 0 }, { "lfo2Rate", "LFO 2 Rate", 0.02f, 30, 0.4f, 0.4f, " Hz", 2 }, { "lfo2Sync", "LFO 2 Sync", 0, 1, 0, 1, "", 0 }, { "lfo2Pw", "LFO2 > Width", 0, 1, 0, 1, "", 2 }, { "lfo2Amp", "LFO2 > Amp", 0, 1, 0, 1, "", 2 },
                { "uniDetune", "Uni Detune", 0, 50, 12, 1, " ct", 0 }, { "uniSpread", "Uni Spread", 0, 1, 0.6f, 1, "", 2 }, { "glide", "Glide", 0, 2, 0.08f, 0.4f, " s", 3 }, { "drive", "Drive", 0, 1, 0, 1, "", 2 }, { "driveType", "Drive Type", 0, 3, 0, 1, "", 0 }, { "ampVel", "Velocity", 0, 1, 0.6f, 1, "", 2 }, { "pan", "Pan", -1, 1, 0, 1, "", 2 },
                { "gateOn", "Gate On", 0, 1, 0, 1, "", 0 }, { "gateRate", "Gate Rate", 0, 3, 1, 1, "", 0 }, { "gateDepth", "Gate Depth", 0, 1, 1, 1, "", 2 }, { "gatePattern", "Gate Pattern", 0, 7, 0, 1, "", 0 },
                { "fxChorus", "Chorus", 0, 1, 0, 1, "", 2 }, { "fxDelay", "Delay", 0, 1, 0, 1, "", 2 }, { "fxDelayTime", "Delay Time", 0.125f, 2, 0.75f, 1, " beats", 3 }, { "fxDelayFb", "Delay FB", 0, 0.95f, 0.4f, 1, "", 2 }, { "fxReverb", "Reverb", 0, 1, 0, 1, "", 2 }, { "fxSize", "Reverb Size", 0, 1, 0.5f, 1, "", 2 }, { "width", "Width", 0, 2, 1, 1, "", 2 },
                { "macro1", "Macro 1", 0, 1, 0, 1, "", 2 }, { "macro2", "Macro 2", 0, 1, 0, 1, "", 2 }, { "macro3", "Macro 3", 0, 1, 0, 1, "", 2 }, { "macro4", "Macro 4", 0, 1, 0, 1, "", 2 },
                { "level", "Level", -24, 6, -6, 1, " dB", 1 } });
        }
        ~SqVenomPlugin() override {}

        float macroValue (int i) const override   { return bank[(size_t) (macro1 + i)]; }

        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            for (int o = 0; o < 2; ++o)
            {
                auto& os = s.osc[o]; const int b = o == 0 ? (int) o1Wave : (int) o2Wave;
                os.wave = (Wave) juce::jlimit (0, 5, (int) std::lround (bank[(size_t) b])); os.level = bank[(size_t) (b + 1)]; os.semis = bank[(size_t) (b + 2)]; os.cents = bank[(size_t) (b + 3)]; os.pw = bank[(size_t) (b + 4)];
            }
            s.osc[1].sync = bank[o2Sync] > 0.5f; s.osc[1].fm = bank[fmAmt]; s.osc[2].level = 0.0f;
            s.subLevel = bank[subLevel]; s.subOct = (int) std::lround (bank[subOct]); s.noiseLevel = bank[noiseLevel]; s.ring = bank[ring];
            s.f1 = (FilterType) (int) std::lround (bank[f1Type]); s.f1Cut = bank[f1Cut]; s.f1Res = bank[f1Res]; s.f1Env = bank[f1Env]; s.f1Key = bank[f1Key];
            s.f2 = (FilterType) (int) std::lround (bank[f2Type]); s.f2Cut = bank[f2Cut]; s.f2Res = bank[f2Res]; s.routing = (int) std::lround (bank[routing]);
            s.env[0][0] = bank[aA]; s.env[0][1] = bank[aD]; s.env[0][2] = bank[aS]; s.env[0][3] = bank[aR];
            s.env[1][0] = bank[fA]; s.env[1][1] = bank[fD]; s.env[1][2] = bank[fS]; s.env[1][3] = bank[fR];
            s.env[2][0] = bank[mA]; s.env[2][1] = bank[mD]; s.env[2][2] = bank[mS]; s.env[2][3] = bank[mR];
            lfoShape1 = (LfoShape) (int) std::lround (bank[lfo1Shape]); lfoShape2 = (LfoShape) (int) std::lround (bank[lfo2Shape]);
            setLfo (lfo1, bank[lfo1Rate], bank[lfo1Sync] > 0.5f, syncBeats (bank[lfo1Rate])); setLfo (lfo2, bank[lfo2Rate], bank[lfo2Sync] > 0.5f, syncBeats (bank[lfo2Rate]));
            s.lfo1Pitch = bank[lfo1Pitch] * 2.0f; s.lfo1Cut = bank[lfo1Cut]; s.lfo2Pw = bank[lfo2Pw]; s.lfo2Amp = bank[lfo2Amp];
            s.unisonCents = bank[uniDetune]; s.unisonSpread = bank[uniSpread]; glideTime = bank[glide];
            s.drive = bank[drive]; s.driveType = (int) std::lround (bank[driveType]); s.ampVel = bank[ampVel]; s.pan = bank[pan]; s.gain = 0.9f;
        }
        static float syncBeats (float rateKnob)   { static const float div[] = { 4, 2, 1, 0.5f, 0.25f, 0.125f }; return div[juce::jlimit (0, 5, (int) (rateKnob / 30.0f * 5.99f))]; }

        void renderMaster (float* l, float* r, int n) override
        {
            // trance gate: 16-step rhythmic amplitude gate at 1/4, 1/8, 1/16 or 1/32
            if (bank[gateOn] > 0.5f)
            {
                static const juce::uint16 patterns[8] = { 0b1010101010101010, 0b1101101101101101, 0b1110111011101110, 0b1001001010010010, 0b1111000011110000, 0b1011011010110110, 0b1000100010001000, 0b1101011011010110 };
                const double stepsPerBeat = std::pow (2.0, bank[gateRate]) / 2.0;   // 0: 1/2 beat.. clamp
                const double samplesPerStep = sr * 60.0 / bpm / juce::jmax (0.5, stepsPerBeat * 2.0);
                const juce::uint16 pat = patterns[juce::jlimit (0, 7, (int) std::lround (bank[gatePattern]))];
                const float depth = bank[gateDepth];
                for (int i = 0; i < n; ++i)
                {
                    const int step = (int) (gatePos / samplesPerStep) & 15;
                    const bool on = (pat >> (15 - step)) & 1;
                    gateEnv += ((on ? 1.0f : 1.0f - depth) - gateEnv) * 0.002f;
                    l[i] *= gateEnv; r[i] *= gateEnv; gatePos += 1.0;
                }
            }
            renderMasterFx (l, r, n, bank[fxChorus], bank[fxDelay], bank[fxDelayTime], bank[fxDelayFb], bank[fxReverb], bank[fxSize], bank[width]);
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    private:
        double gatePos = 0; float gateEnv = 1.0f;
    };

    //==========================================================================
    // Mothsynth 2 - three oscillators, the flagship analogue-style synth
    class SqMothsynth2Plugin : public SubtractiveSynth<32>
    {
    public:
        enum P { o1Wave, o1Level, o1Semi, o1Fine, o1Pw, o2Wave, o2Level, o2Semi, o2Fine, o2Pw, o2Sync, o3Wave, o3Level, o3Semi, o3Fine, subLevel, subOct, subWave, noiseLevel, ring, fmAmt,
                 f1Type, f1Cut, f1Res, f1Env, f1Key, f2Type, f2Cut, f2Res, f2Env, routing,
                 aA, aD, aS, aR, fA, fD, fS, fR, mA, mD, mS, mR,
                 lfo1Shape, lfo1Rate, lfo1Sync, lfo1Pitch, lfo1Cut, lfo2Shape, lfo2Rate, lfo2Sync, lfo2Pw, lfo2Amp,
                 uniDetune, uniSpread, glide, drive, driveType, ampVel, pan,
                 fxChorus, fxDelay, fxDelayTime, fxDelayFb, fxReverb, fxSize, width, macro1, macro2, macro3, macro4, level, numParams };

        static constexpr const char* xmlTypeName = "sqmMothsynth2";
        static const char* getPluginName()   { return NEEDS_TRANS("Mothsynth 2"); }
        juce::String getName() const override { return "Mothsynth 2"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Moth2"; }
        juce::String getSelectableDescription() override { return "Mothsynth 2 analogue-style synth"; }

        SqMothsynth2Plugin (te::PluginCreationInfo info) : SubtractiveSynth<32> (info)
        {
            bank.create (*this, {
                { "o1Wave", "Osc 1 Wave", 0, 5, 2, 1, "", 0 }, { "o1Level", "Osc 1 Level", 0, 1, 0.8f, 1, "", 2 }, { "o1Semi", "Osc 1 Semi", -24, 24, 0, 1, " st", 0 }, { "o1Fine", "Osc 1 Fine", -100, 100, 0, 1, " ct", 0 }, { "o1Pw", "Osc 1 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 },
                { "o2Wave", "Osc 2 Wave", 0, 5, 2, 1, "", 0 }, { "o2Level", "Osc 2 Level", 0, 1, 0.7f, 1, "", 2 }, { "o2Semi", "Osc 2 Semi", -24, 24, 0, 1, " st", 0 }, { "o2Fine", "Osc 2 Fine", -100, 100, -8, 1, " ct", 0 }, { "o2Pw", "Osc 2 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 }, { "o2Sync", "Osc 2 Sync", 0, 1, 0, 1, "", 0 },
                { "o3Wave", "Osc 3 Wave", 0, 5, 3, 1, "", 0 }, { "o3Level", "Osc 3 Level", 0, 1, 0.0f, 1, "", 2 }, { "o3Semi", "Osc 3 Semi", -24, 24, -12, 1, " st", 0 }, { "o3Fine", "Osc 3 Fine", -100, 100, 0, 1, " ct", 0 },
                { "subLevel", "Sub Level", 0, 1, 0, 1, "", 2 }, { "subOct", "Sub Octave", 1, 2, 1, 1, "", 0 }, { "subWave", "Sub Wave", 0, 3, 0, 1, "", 0 }, { "noiseLevel", "Noise", 0, 1, 0, 1, "", 2 }, { "ring", "Ring Mod", 0, 1, 0, 1, "", 2 }, { "fmAmt", "FM Amount", 0, 1, 0, 1, "", 2 },
                { "f1Type", "F1 Type", 0, 6, 6, 1, "", 0 }, { "f1Cut", "Cutoff", 20, 20000, 2200, 0.3f, " Hz", 0 }, { "f1Res", "Resonance", 0, 1, 0.15f, 1, "", 2 }, { "f1Env", "Filter Env", -1, 1, 0.45f, 1, "", 2 }, { "f1Key", "Key Track", 0, 1, 0.4f, 1, "", 2 },
                { "f2Type", "F2 Type", 0, 6, 3, 1, "", 0 }, { "f2Cut", "F2 Cutoff", 20, 20000, 80, 0.3f, " Hz", 0 }, { "f2Res", "F2 Res", 0, 1, 0, 1, "", 2 }, { "f2Env", "F2 Env", -1, 1, 0, 1, "", 2 }, { "routing", "Routing", 0, 2, 0, 1, "", 0 },
                { "aA", "Amp Attack", 0.001f, 4, 0.006f, 0.3f, " s", 3 }, { "aD", "Amp Decay", 0.001f, 6, 0.4f, 0.3f, " s", 2 }, { "aS", "Amp Sustain", 0, 1, 0.7f, 1, "", 2 }, { "aR", "Amp Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 6, 0.5f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.2f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.4f, 0.3f, " s", 2 },
                { "mA", "Mod Attack", 0.001f, 4, 0.5f, 0.3f, " s", 3 }, { "mD", "Mod Decay", 0.001f, 6, 0.5f, 0.3f, " s", 2 }, { "mS", "Mod Sustain", 0, 1, 0.5f, 1, "", 2 }, { "mR", "Mod Release", 0.001f, 8, 0.5f, 0.3f, " s", 2 },
                { "lfo1Shape", "LFO 1 Shape", 0, 5, 0, 1, "", 0 }, { "lfo1Rate", "LFO 1 Rate", 0.02f, 30, 5.0f, 0.4f, " Hz", 2 }, { "lfo1Sync", "LFO 1 Sync", 0, 1, 0, 1, "", 0 }, { "lfo1Pitch", "LFO1 > Pitch", 0, 1, 0, 1, "", 2 }, { "lfo1Cut", "LFO1 > Cut", -1, 1, 0, 1, "", 2 },
                { "lfo2Shape", "LFO 2 Shape", 0, 5, 1, 1, "", 0 }, { "lfo2Rate", "LFO 2 Rate", 0.02f, 30, 0.3f, 0.4f, " Hz", 2 }, { "lfo2Sync", "LFO 2 Sync", 0, 1, 0, 1, "", 0 }, { "lfo2Pw", "LFO2 > Width", 0, 1, 0, 1, "", 2 }, { "lfo2Amp", "LFO2 > Amp", 0, 1, 0, 1, "", 2 },
                { "uniDetune", "Uni Detune", 0, 50, 10, 1, " ct", 0 }, { "uniSpread", "Uni Spread", 0, 1, 0.5f, 1, "", 2 }, { "glide", "Glide", 0, 2, 0.06f, 0.4f, " s", 3 }, { "drive", "Drive", 0, 1, 0.08f, 1, "", 2 }, { "driveType", "Drive Type", 0, 3, 1, 1, "", 0 }, { "ampVel", "Velocity", 0, 1, 0.5f, 1, "", 2 }, { "pan", "Pan", -1, 1, 0, 1, "", 2 },
                { "fxChorus", "Chorus", 0, 1, 0, 1, "", 2 }, { "fxDelay", "Delay", 0, 1, 0, 1, "", 2 }, { "fxDelayTime", "Delay Time", 0.125f, 2, 0.5f, 1, " beats", 3 }, { "fxDelayFb", "Delay FB", 0, 0.95f, 0.35f, 1, "", 2 }, { "fxReverb", "Reverb", 0, 1, 0, 1, "", 2 }, { "fxSize", "Reverb Size", 0, 1, 0.5f, 1, "", 2 }, { "width", "Width", 0, 2, 1, 1, "", 2 },
                { "macro1", "Macro 1", 0, 1, 0, 1, "", 2 }, { "macro2", "Macro 2", 0, 1, 0, 1, "", 2 }, { "macro3", "Macro 3", 0, 1, 0, 1, "", 2 }, { "macro4", "Macro 4", 0, 1, 0, 1, "", 2 },
                { "level", "Level", -24, 6, -6, 1, " dB", 1 } });
        }
        ~SqMothsynth2Plugin() override {}
        float macroValue (int i) const override   { return bank[(size_t) (macro1 + i)]; }

        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            const int bases[3] = { o1Wave, o2Wave, o3Wave };
            for (int o = 0; o < 3; ++o)
            {
                auto& os = s.osc[o]; const int b = bases[o];
                os.wave = (Wave) juce::jlimit (0, 5, (int) std::lround (bank[(size_t) b])); os.level = bank[(size_t) (b + 1)]; os.semis = bank[(size_t) (b + 2)]; os.cents = bank[(size_t) (b + 3)];
                os.pw = o < 2 ? bank[(size_t) (b + 4)] : 0.5f;
            }
            s.osc[1].sync = bank[o2Sync] > 0.5f; s.osc[1].fm = bank[fmAmt];
            s.subLevel = bank[subLevel]; s.subOct = (int) std::lround (bank[subOct]); s.subWave = (Wave) (int) std::lround (bank[subWave]); s.noiseLevel = bank[noiseLevel]; s.ring = bank[ring];
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
    // Sim Moth - the simple three-oscillator synth
    class SqSimMothPlugin : public SubtractiveSynth<16>
    {
    public:
        enum P { o1Wave, o1Level, o1Semi, o1Pw, o2Wave, o2Level, o2Semi, o2Fine, o3Wave, o3Level, o3Semi, o3Fine,
                 fType, cutoff, res, fEnv, aA, aD, aS, aR, fA, fD, fS, fR, lfoRate, lfoPitch, lfoCut, lfoPw, glide, chorusMix, level, numParams };
        static constexpr const char* xmlTypeName = "sqmSimMoth";
        static const char* getPluginName()   { return NEEDS_TRANS("Sim Moth"); }
        juce::String getName() const override { return "Sim Moth"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Sim"; }
        juce::String getSelectableDescription() override { return "Sim Moth three-oscillator synth"; }

        SqSimMothPlugin (te::PluginCreationInfo info) : SubtractiveSynth<16> (info)
        {
            bank.create (*this, {
                { "o1Wave", "Osc 1 Wave", 0, 5, 2, 1, "", 0 }, { "o1Level", "Osc 1 Level", 0, 1, 0.8f, 1, "", 2 }, { "o1Semi", "Osc 1 Semi", -24, 24, 0, 1, " st", 0 }, { "o1Pw", "Osc 1 Width", 0.05f, 0.95f, 0.5f, 1, "", 2 },
                { "o2Wave", "Osc 2 Wave", 0, 5, 2, 1, "", 0 }, { "o2Level", "Osc 2 Level", 0, 1, 0.6f, 1, "", 2 }, { "o2Semi", "Osc 2 Semi", -24, 24, 0, 1, " st", 0 }, { "o2Fine", "Osc 2 Fine", -100, 100, 9, 1, " ct", 0 },
                { "o3Wave", "Osc 3 Wave", 0, 5, 3, 1, "", 0 }, { "o3Level", "Osc 3 Level", 0, 1, 0.5f, 1, "", 2 }, { "o3Semi", "Osc 3 Semi", -24, 24, -12, 1, " st", 0 }, { "o3Fine", "Osc 3 Fine", -100, 100, 0, 1, " ct", 0 },
                { "fType", "Filter Type", 0, 6, 0, 1, "", 0 }, { "cutoff", "Cutoff", 20, 20000, 3000, 0.3f, " Hz", 0 }, { "res", "Resonance", 0, 1, 0.15f, 1, "", 2 }, { "fEnv", "Filter Env", -1, 1, 0.4f, 1, "", 2 },
                { "aA", "Attack", 0.001f, 4, 0.005f, 0.3f, " s", 3 }, { "aD", "Decay", 0.001f, 6, 0.3f, 0.3f, " s", 2 }, { "aS", "Sustain", 0, 1, 0.8f, 1, "", 2 }, { "aR", "Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "fA", "Flt Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "fD", "Flt Decay", 0.001f, 6, 0.4f, 0.3f, " s", 2 }, { "fS", "Flt Sustain", 0, 1, 0.2f, 1, "", 2 }, { "fR", "Flt Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "lfoRate", "LFO Rate", 0.02f, 20, 5, 0.4f, " Hz", 2 }, { "lfoPitch", "LFO > Pitch", 0, 1, 0, 1, "", 2 }, { "lfoCut", "LFO > Cutoff", -1, 1, 0, 1, "", 2 }, { "lfoPw", "LFO > Width", 0, 1, 0, 1, "", 2 },
                { "glide", "Glide", 0, 2, 0, 0.4f, " s", 3 }, { "chorusMix", "Chorus", 0, 1, 0.2f, 1, "", 2 }, { "level", "Level", -24, 6, -6, 1, " dB", 1 } });
        }
        ~SqSimMothPlugin() override {}
        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            s.osc[0] = { (Wave) (int) std::lround (bank[o1Wave]), bank[o1Level], bank[o1Semi], 0, bank[o1Pw], false, 0 };
            s.osc[1] = { (Wave) (int) std::lround (bank[o2Wave]), bank[o2Level], bank[o2Semi], bank[o2Fine], 0.5f, false, 0 };
            s.osc[2] = { (Wave) (int) std::lround (bank[o3Wave]), bank[o3Level], bank[o3Semi], bank[o3Fine], 0.5f, false, 0 };
            s.subLevel = 0; s.noiseLevel = 0; s.ring = 0; s.routing = 0;
            s.f1 = (FilterType) (int) std::lround (bank[fType]); s.f1Cut = bank[cutoff]; s.f1Res = bank[res]; s.f1Env = bank[fEnv]; s.f1Key = 0.3f;
            s.env[0][0] = bank[aA]; s.env[0][1] = bank[aD]; s.env[0][2] = bank[aS]; s.env[0][3] = bank[aR];
            s.env[1][0] = bank[fA]; s.env[1][1] = bank[fD]; s.env[1][2] = bank[fS]; s.env[1][3] = bank[fR];
            setLfo (lfo1, bank[lfoRate], false, 1); setLfo (lfo2, bank[lfoRate], false, 1);
            s.lfo1Pitch = bank[lfoPitch] * 2.0f; s.lfo1Cut = bank[lfoCut]; s.lfo2Pw = bank[lfoPw]; s.lfo2Amp = 0;
            s.unisonCents = 8; glideTime = bank[glide]; s.drive = 0; s.ampVel = 0.5f; s.pan = 0; s.gain = 0.7f;
        }
        void renderMaster (float* l, float* r, int n) override
        {
            renderMasterFx (l, r, n, bank[chorusMix], 0, 0.5f, 0, 0, 0.5f, 1.0f);
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    };

    //==========================================================================
    // Mini Moth - one screen, ultra light
    class SqMiniMothPlugin : public SubtractiveSynth<16>
    {
    public:
        enum P { wave, subLevel, cutoff, res, fEnv, attack, decay, sustain, release, lfoRate, lfoDepth, glide, chorusMix, level, numParams };
        static constexpr const char* xmlTypeName = "sqmMiniMoth";
        static const char* getPluginName()   { return NEEDS_TRANS("Mini Moth"); }
        juce::String getName() const override { return "Mini Moth"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Mini"; }
        juce::String getSelectableDescription() override { return "Mini Moth light synth"; }
        SqMiniMothPlugin (te::PluginCreationInfo info) : SubtractiveSynth<16> (info)
        {
            bank.create (*this, {
                { "wave", "Wave", 0, 5, 2, 1, "", 0 }, { "subLevel", "Sub", 0, 1, 0.4f, 1, "", 2 }, { "cutoff", "Cutoff", 20, 20000, 2500, 0.3f, " Hz", 0 }, { "res", "Resonance", 0, 1, 0.2f, 1, "", 2 }, { "fEnv", "Filter Env", -1, 1, 0.5f, 1, "", 2 },
                { "attack", "Attack", 0.001f, 4, 0.004f, 0.3f, " s", 3 }, { "decay", "Decay", 0.001f, 6, 0.35f, 0.3f, " s", 2 }, { "sustain", "Sustain", 0, 1, 0.6f, 1, "", 2 }, { "release", "Release", 0.001f, 8, 0.25f, 0.3f, " s", 2 },
                { "lfoRate", "LFO Rate", 0.02f, 20, 5, 0.4f, " Hz", 2 }, { "lfoDepth", "Vibrato", 0, 1, 0, 1, "", 2 }, { "glide", "Glide", 0, 2, 0, 0.4f, " s", 3 }, { "chorusMix", "Chorus", 0, 1, 0, 1, "", 2 }, { "level", "Level", -24, 6, -6, 1, " dB", 1 } });
        }
        ~SqMiniMothPlugin() override {}
        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            s.osc[0] = { (Wave) (int) std::lround (bank[wave]), 0.9f, 0, 0, 0.5f, false, 0 }; s.osc[1].level = 0; s.osc[2].level = 0;
            s.subLevel = bank[subLevel]; s.subOct = 1; s.subWave = Wave::square; s.noiseLevel = 0; s.ring = 0; s.routing = 0;
            s.f1 = FilterType::lp24; s.f1Cut = bank[cutoff]; s.f1Res = bank[res]; s.f1Env = bank[fEnv]; s.f1Key = 0.3f;
            s.env[0][0] = bank[attack]; s.env[0][1] = bank[decay]; s.env[0][2] = bank[sustain]; s.env[0][3] = bank[release];
            s.env[1][0] = 0.003f; s.env[1][1] = bank[decay]; s.env[1][2] = 0.0f; s.env[1][3] = bank[release];
            setLfo (lfo1, bank[lfoRate], false, 1);
            s.lfo1Pitch = bank[lfoDepth] * 0.5f; s.lfo1Cut = 0; s.lfo2Pw = 0; s.lfo2Amp = 0;
            glideTime = bank[glide]; s.drive = 0; s.ampVel = 0.5f; s.pan = 0; s.gain = 0.8f; s.unisonCents = 6;
        }
        void renderMaster (float* l, float* r, int n) override
        {
            renderMasterFx (l, r, n, bank[chorusMix], 0, 0.5f, 0, 0, 0.5f, 1.0f);
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    };

    //==========================================================================
    // Thump - the kick synth
    struct KickVoice : public VoiceBase
    {
        double phase = 0; float pitchEnv = 1.0f, ampEnv = 0, clickEnv = 0, bodyEnv = 0; juce::uint32 rng = 7; SqDsp::SVF clickHp, tone; float subPhase = 0;
        struct S { float startSemis = 36, endHz = 50, pitchTime = 0.06f, pitchCurve = 2.0f, decay = 0.6f, click = 0.3f, clickTone = 3000, body = 0.3f, bodyRatio = 1.5f, bodyDecay = 0.1f, sub = 0.3f, drive = 0.2f, tone = 8000, velSens = 0.5f, gain = 1.0f; int driveType = 0, wave = 0; } s;
        float t = 0; double sampleT = 0;
        void start (int n, float vel, float, float) { startBase (n, vel, 0, 0); phase = 0; subPhase = 0; sampleT = 0; ampEnv = 1.0f; clickEnv = 1.0f; bodyEnv = 1.0f; clickHp.reset(); }
        inline void render (float* l, float* r, int n)
        {
            const double baseHz = hz * (s.endHz / 50.0);   // note tracks: C3 = the End Hz
            const float vg = (1.0f - s.velSens + s.velSens * velocity) * s.gain * unisonGain;
            clickHp.set (sr, s.clickTone, 1.2f); tone.set (sr, s.tone, 0.707f);
            const float ampCoef = std::exp (-1.0f / (float) (juce::jmax (0.01f, s.decay) * sr) * 4.6f);
            const float bodyCoef = std::exp (-1.0f / (float) (juce::jmax (0.005f, s.bodyDecay) * sr) * 4.6f);
            const float clickCoef = std::exp (-1.0f / (float) (0.004f * sr) * 4.6f);
            for (int i = 0; i < n; ++i)
            {
                const double tt = sampleT / sr;
                const float pe = std::exp (-(float) tt / juce::jmax (0.002f, s.pitchTime) * s.pitchCurve);
                const double f = baseHz * std::pow (2.0, (double) (s.startSemis * pe) / 12.0);
                phase += f / sr; if (phase >= 1.0) phase -= 1.0;
                float body = s.wave == 0 ? std::sin ((float) phase * twoPi) : (s.wave == 1 ? (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase) : std::sin ((float) phase * twoPi) * (1.0f + 0.3f * std::sin ((float) phase * twoPi * 2.0f)));
                subPhase += (float) (f * 0.5 / sr); if (subPhase >= 1.0f) subPhase -= 1.0f;
                const float subv = std::sin (subPhase * twoPi) * s.sub;
                const float harm = std::sin ((float) phase * twoPi * s.bodyRatio) * bodyEnv * s.body;
                rng = rng * 1664525u + 1013904223u; const float nz = ((float) ((rng >> 8) & 0xffffff) / 8388608.0f - 1.0f);
                const float click = clickHp.bandpass (nz) * clickEnv * s.click * 2.0f;
                float y = (body + harm + subv) * ampEnv + click;
                if (s.drive > 0.001f) y = driveShape (y, s.drive, s.driveType);
                y = tone.lowpass (y) * vg;
                l[i] += y; r[i] += y;
                ampEnv *= ampCoef; bodyEnv *= bodyCoef; clickEnv *= clickCoef; sampleT += 1.0;
            }
            if (ampEnv < 0.0005f) active = false;
        }
    };

    class SqThumpPlugin : public SynthPluginBase<KickVoice, 8>
    {
    public:
        enum P { startPitch, endHz, pitchTime, pitchCurve, decay, click, clickTone, body, bodyRatio, bodyDecay, sub, drive, driveType, tone, wave, velSens, level, numParams };
        static constexpr const char* xmlTypeName = "sqmThump";
        static const char* getPluginName()   { return NEEDS_TRANS("Thump"); }
        juce::String getName() const override { return "Thump"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Thmp"; }
        juce::String getSelectableDescription() override { return "Thump kick synth"; }
        double getTailLength() const override { return 2.0; }
        SqThumpPlugin (te::PluginCreationInfo info) : SynthPluginBase<KickVoice, 8> (info)
        {
            bank.create (*this, {
                { "startPitch", "Pitch Sweep", 0, 60, 36, 1, " st", 0 }, { "endHz", "Pitch (C3)", 25, 200, 50, 0.5f, " Hz", 0 }, { "pitchTime", "Sweep Time", 0.005f, 0.5f, 0.06f, 0.4f, " s", 3 }, { "pitchCurve", "Sweep Curve", 0.5f, 6, 2, 1, "", 1 },
                { "decay", "Decay", 0.05f, 3, 0.6f, 0.4f, " s", 2 }, { "click", "Click", 0, 1, 0.3f, 1, "", 2 }, { "clickTone", "Click Tone", 500, 12000, 3000, 0.4f, " Hz", 0 },
                { "body", "Body Harm", 0, 1, 0.3f, 1, "", 2 }, { "bodyRatio", "Body Ratio", 1, 4, 1.5f, 1, "", 2 }, { "bodyDecay", "Body Decay", 0.005f, 1, 0.1f, 0.4f, " s", 3 }, { "sub", "Sub Layer", 0, 1, 0.3f, 1, "", 2 },
                { "drive", "Drive", 0, 1, 0.2f, 1, "", 2 }, { "driveType", "Drive Type", 0, 3, 0, 1, "", 0 }, { "tone", "Tone", 500, 20000, 8000, 0.3f, " Hz", 0 }, { "wave", "Wave", 0, 2, 0, 1, "", 0 }, { "velSens", "Velocity", 0, 1, 0.5f, 1, "", 2 }, { "level", "Level", -24, 6, -3, 1, " dB", 1 } });
            polyMode = 0; maxPoly = 2;
        }
        ~SqThumpPlugin() override {}
        void updateChunk (const ModValues&) override
        {
            for (auto& v : voices)
            {
                auto& s = v.s;
                s.startSemis = bank[startPitch]; s.endHz = bank[endHz]; s.pitchTime = bank[pitchTime]; s.pitchCurve = bank[pitchCurve]; s.decay = bank[decay]; s.click = bank[click]; s.clickTone = bank[clickTone];
                s.body = bank[body]; s.bodyRatio = bank[bodyRatio]; s.bodyDecay = bank[bodyDecay]; s.sub = bank[sub]; s.drive = bank[drive]; s.driveType = (int) std::lround (bank[driveType]); s.tone = bank[tone]; s.wave = (int) std::lround (bank[wave]); s.velSens = bank[velSens]; s.gain = 0.9f;
            }
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    };

    //==========================================================================
    // Dial 10 - two-operator FM (16 knobs)
    struct FmVoice : public VoiceBase
    {
        double cPhase = 0, mPhase = 0; float fb = 0;
        struct S { float ratio = 1, modLevel = 0.5f, feedback = 0, mA = 0.005f, mD = 0.3f, mS = 0.3f, mR = 0.3f, aA = 0.005f, aD = 0.5f, aS = 0.7f, aR = 0.3f, vibRate = 5, vibDepth = 0, octave = 0, fine = 0, velMod = 0.5f, brightKey = 0, gain = 0.6f; int cWave = 0; } s;
        void start (int n, float vel, float g, float from) { startBase (n, vel, g, from); cPhase = mPhase = 0; fb = 0; }
        inline void render (float* l, float* r, int n)
        {
            env[0].set (s.aA, s.aD, s.aS, s.aR); env[1].set (s.mA, s.mD, s.mS, s.mR);
            const double f = chunkFrequency (bend + s.octave * 12.0f + s.vibDepth * 0.3f * mods.get ((int) ModSource::lfo1), s.fine);
            const double cInc = f / sr, mInc = f * s.ratio / sr;
            const float velIdx = 1.0f - s.velMod + s.velMod * velocity;
            const float keyIdx = 1.0f + s.brightKey * (float) (note - 60) / 48.0f;
            for (int i = 0; i < n; ++i)
            {
                const float me = env[1].next();
                const float modOut = std::sin ((float) (mPhase * twoPi) + fb * s.feedback * 3.0f);
                fb = modOut;
                const float idx = s.modLevel * 8.0f * me * velIdx * keyIdx;
                float carrier = std::sin ((float) (cPhase * twoPi) + modOut * idx);
                if (s.cWave == 1) carrier = carrier * 0.7f + std::sin ((float) (cPhase * twoPi) * 2.0f + modOut * idx) * 0.3f;
                const float a = env[0].next() * (0.6f + 0.4f * velocity) * s.gain * unisonGain;
                const float y = carrier * a;
                l[i] += y; r[i] += y;
                cPhase += cInc; if (cPhase >= 1.0) cPhase -= 1.0;
                mPhase += mInc; if (mPhase >= 1.0) mPhase -= 1.0;
            }
            if (! env[0].isActive()) active = false;
        }
    };

    class SqDial10Plugin : public SynthPluginBase<FmVoice, 16>
    {
    public:
        enum P { ratio, modLevel, feedback, mA, mD, mS, mR, aA, aD, aS, aR, vibRate, vibDepth, octave, fine, velMod, brightKey, cWave, level, numParams };
        static constexpr const char* xmlTypeName = "sqmDial10";
        static const char* getPluginName()   { return NEEDS_TRANS("Dial 10"); }
        juce::String getName() const override { return "Dial 10"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "D10"; }
        juce::String getSelectableDescription() override { return "Dial 10 two-operator FM synth"; }
        SqDial10Plugin (te::PluginCreationInfo info) : SynthPluginBase<FmVoice, 16> (info)
        {
            bank.create (*this, {
                { "ratio", "Mod Ratio", 0.25f, 16, 1, 0.5f, "", 2 }, { "modLevel", "Mod Level", 0, 1, 0.5f, 1, "", 2 }, { "feedback", "Feedback", 0, 1, 0, 1, "", 2 },
                { "mA", "Mod Attack", 0.001f, 4, 0.005f, 0.3f, " s", 3 }, { "mD", "Mod Decay", 0.001f, 6, 0.3f, 0.3f, " s", 2 }, { "mS", "Mod Sustain", 0, 1, 0.3f, 1, "", 2 }, { "mR", "Mod Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "aA", "Attack", 0.001f, 4, 0.005f, 0.3f, " s", 3 }, { "aD", "Decay", 0.001f, 6, 0.5f, 0.3f, " s", 2 }, { "aS", "Sustain", 0, 1, 0.7f, 1, "", 2 }, { "aR", "Release", 0.001f, 8, 0.3f, 0.3f, " s", 2 },
                { "vibRate", "Vibrato Rate", 0.5f, 12, 5, 1, " Hz", 1 }, { "vibDepth", "Vibrato", 0, 1, 0, 1, "", 2 }, { "octave", "Octave", -2, 2, 0, 1, "", 0 }, { "fine", "Fine", -100, 100, 0, 1, " ct", 0 },
                { "velMod", "Vel > Mod", 0, 1, 0.5f, 1, "", 2 }, { "brightKey", "Key > Bright", -1, 1, 0, 1, "", 2 }, { "cWave", "Carrier", 0, 1, 0, 1, "", 0 }, { "level", "Level", -24, 6, -6, 1, " dB", 1 } });
        }
        ~SqDial10Plugin() override {}
        void updateChunk (const ModValues&) override
        {
            setLfo (lfo1, bank[vibRate], false, 1);
            for (auto& v : voices)
            {
                auto& s = v.s;
                s.ratio = bank[ratio]; s.modLevel = bank[modLevel]; s.feedback = bank[feedback]; s.mA = bank[mA]; s.mD = bank[mD]; s.mS = bank[mS]; s.mR = bank[mR];
                s.aA = bank[aA]; s.aD = bank[aD]; s.aS = bank[aS]; s.aR = bank[aR]; s.vibRate = bank[vibRate]; s.vibDepth = bank[vibDepth]; s.octave = bank[octave]; s.fine = bank[fine];
                s.velMod = bank[velMod]; s.brightKey = bank[brightKey]; s.cWave = (int) std::lround (bank[cWave]); s.gain = 0.5f;
            }
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    };
}
