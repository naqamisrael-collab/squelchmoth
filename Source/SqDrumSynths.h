#pragma once
// Squelchmoth 1.8.1 drum synths, all on SqSynthCore:
//
//  Carapace        - percussion modelling drum synth: 16 modelled voices (membrane, metal, noise, wood, tom) across the
//                    keys from C2, each with pitch, pitch envelope, decay, tone, noise, snap, drive, level, pan and choke group
//  Live Drum Synth - the classic two-oscillator + noise drum synth: 12 editable voices across the keys
//  Drum Pad Model  - one modelled percussion voice, every control on the panel - fast to tweak per rack channel
//  Thump Live      - the kick with drawable pitch, amplitude and distortion envelopes (four breakpoints each),
//                    a click layer and a 16-step kick pattern (BassDrum)
//
// Every one is original DSP, exposes everything to automation and Claude, and uses no CPU while silent.

#include "SqSynths2.h"

namespace SqSynth
{
    //==========================================================================
    inline juce::StringArray drumModelNames()   { return { "MEMBRANE", "METAL", "NOISE", "WOOD", "TOM" }; }
    inline juce::StringArray drumOscNames()     { return { "SINE", "TRI", "SAW", "SQUARE", "NOISE" }; }

    /** One drum sound's settings (both engines share the struct; the classic engine uses the osc fields). */
    struct DrumSound
    {
        int model = 0;                       // modelled engine: 0 membrane 1 metal 2 noise 3 wood 4 tom
        float pitch = 60.0f, pitchEnv = 24.0f, pitchTime = 0.04f, decay = 0.4f, tone = 0.5f, noise = 0.2f, snap = 0.3f, drive = 0.0f, level = 0.8f, pan = 0.0f;
        int choke = 0;
        // classic engine (Live Drum Synth): two oscillators + noise
        int o1Wave = 0, o2Wave = 0; float o1Pitch = 55.0f, o1PitchEnv = 30.0f, o1Decay = 0.5f, o2Pitch = 180.0f, o2Level = 0.3f, o2Decay = 0.1f, nzLevel = 0.3f, nzColour = 0.5f, nzDecay = 0.15f;
    };

    /** Drive for percussive voices: identity at 0, blends into the shaper so a touch of drive adds grit without dropping level. */
    inline float drumDrive (float x, float amount, int type)
    {
        const float mix = juce::jmin (1.0f, amount * 4.0f);
        const float shaped = driveShape (x, amount, type) * (type == 1 ? 1.8f : 1.2f);
        return x + (shaped - x) * mix;
    }

    struct DrumVoice : public VoiceBase
    {
        const DrumSound* snd = nullptr; int soundIndex = 0; bool classic = false;
        double ph = 0, ph2 = 0, metal[6] = {}; float ampEnv = 0, pitchEnvV = 0, nzEnv = 0, o2Env = 0, snapEnv = 0, sampleT = 0; juce::uint32 rng = 11;
        SqDsp::SVF bp, hp, lp, nzFilt; float woodState = 0;
        void start (int n, float vel, float, float)
        {
            startBase (n, vel, 0, 0);
            ph = ph2 = 0; for (auto& m : metal) m = 0; ampEnv = 1.0f; pitchEnvV = 1.0f; nzEnv = 1.0f; o2Env = 1.0f; snapEnv = 1.0f; sampleT = 0; bp.reset(); hp.reset(); lp.reset(); nzFilt.reset();
        }
        inline float noiseSample()   { rng = rng * 1664525u + 1013904223u; return (float) ((rng >> 8) & 0xffffff) / 8388608.0f - 1.0f; }

        inline void render (float* l, float* r, int n)
        {
            if (snd == nullptr) { active = false; return; }
            const auto& s = *snd;
            const float vg = (0.3f + 0.7f * velocity) * s.level * unisonGain;
            const float panPos = juce::jlimit (-1.0f, 1.0f, s.pan);
            const float gl = std::cos ((panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi), gr = std::sin ((panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            const float fsr = (float) sr;
            if (classic) renderClassic (l, r, n, s, vg, gl, gr, fsr);
            else renderModel (l, r, n, s, vg, gl, gr, fsr);
            if (ampEnv < 0.0003f && nzEnv < 0.0003f) active = false;
        }

        inline void renderModel (float* l, float* r, int n, const DrumSound& s, float vg, float gl, float gr, float fsr)
        {
            const float decayCoef = std::exp (-4.6f / (juce::jmax (0.01f, s.decay) * fsr));
            const float pitchCoef = std::exp (-4.6f / (juce::jmax (0.002f, s.pitchTime) * fsr));
            const float snapCoef = std::exp (-4.6f / (0.006f * fsr));
            const float nzCoef = std::exp (-4.6f / (juce::jmax (0.01f, s.decay * (s.model == 2 ? 1.0f : 0.35f)) * fsr));
            const float baseHz = (float) SqDsp::noteHz (s.pitch) * (float) (hz / SqDsp::noteHz (note));   // the sound's pitch; hz / noteHz (note) carries key tracking and pitch bend (1 for the kit synths)
            switch (s.model)
            {
                case 1: // metal: six inharmonic partials, band-passed and high-passed, like a hat / cymbal
                {
                    static const float ratios[6] = { 1.0f, 1.342f, 1.667f, 2.102f, 2.531f, 3.219f };
                    bp.set (fsr, juce::jlimit (400.0f, 16000.0f, baseHz * 4.0f * (0.5f + s.tone)), 1.5f + s.tone * 2.0f);
                    hp.set (fsr, juce::jlimit (200.0f, 12000.0f, baseHz * 2.0f), 0.7f);
                    for (int i = 0; i < n; ++i)
                    {
                        float m = 0.0f;
                        for (int k = 0; k < 6; ++k) { metal[k] += baseHz * ratios[k] / fsr; if (metal[k] >= 1.0) metal[k] -= 1.0; m += metal[k] < 0.5 ? 1.0f : -1.0f; }
                        m = m / 6.0f + noiseSample() * s.noise;
                        float y = bp.bandpass (m); y = y - hp.lowpass (y);
                        y = y * ampEnv * 1.5f + noiseSample() * snapEnv * s.snap * 0.5f;
                        if (s.drive > 0.001f) y = drumDrive (y, s.drive, 2);
                        y *= vg; l[i] += y * gl; r[i] += y * gr;
                        ampEnv *= decayCoef; snapEnv *= snapCoef;
                    }
                    break;
                }
                case 2: // noise: snares, claps, shakers - filtered noise with a snap and a short body tone
                {
                    nzFilt.set (fsr, 400.0f + 9000.0f * s.tone, 0.8f + s.tone);
                    bp.set (fsr, juce::jlimit (60.0f, 2000.0f, baseHz), 3.0f);
                    for (int i = 0; i < n; ++i)
                    {
                        const float nz = noiseSample();
                        float y = nzFilt.bandpass (nz) * 2.2f * nzEnv * (0.4f + 0.6f * s.noise);
                        y += std::sin ((float) ph * twoPi) * ampEnv * (1.0f - s.noise) * 0.8f; ph += baseHz / fsr; if (ph >= 1.0) ph -= 1.0;
                        y += nz * snapEnv * s.snap;
                        if (s.drive > 0.001f) y = drumDrive (y, s.drive, 1);
                        y *= vg; l[i] += y * gl; r[i] += y * gr;
                        ampEnv *= decayCoef * 0.9999f; nzEnv *= nzCoef; snapEnv *= snapCoef;
                    }
                    break;
                }
                case 3: // wood: a short, resonant knock (rim, block, clave, cowbell-ish)
                {
                    bp.set (fsr, juce::jlimit (100.0f, 6000.0f, baseHz), 6.0f + s.tone * 20.0f);
                    for (int i = 0; i < n; ++i)
                    {
                        const float excite = sampleT < 0.002f * fsr ? noiseSample() * 2.0f : 0.0f;
                        float y = bp.bandpass (excite) * 1.2f * (0.6f + ampEnv * 0.4f) + std::sin ((float) ph * twoPi) * ampEnv * 0.4f * (1.0f - s.noise);
                        ph += baseHz * 1.5f / fsr; if (ph >= 1.0) ph -= 1.0;
                        y += noiseSample() * snapEnv * s.snap * 0.6f;
                        if (s.drive > 0.001f) y = drumDrive (y, s.drive, 1);
                        y *= vg * ampEnv; l[i] += y * gl; r[i] += y * gr;
                        ampEnv *= decayCoef; snapEnv *= snapCoef; sampleT += 1.0f;
                    }
                    break;
                }
                default: // membrane (0) and tom (4): a sine body with a pitch sweep, a second mode, noise snap
                {
                    const float mode2 = s.model == 4 ? 1.5f : 2.3f, mode2Amt = s.model == 4 ? 0.35f : 0.12f + s.tone * 0.3f;
                    lp.set (fsr, 1500.0f + 10000.0f * s.tone, 0.7f);
                    for (int i = 0; i < n; ++i)
                    {
                        const float f = baseHz * std::pow (2.0f, s.pitchEnv * pitchEnvV / 12.0f);
                        ph += f / fsr; if (ph >= 1.0) ph -= 1.0;
                        ph2 += f * mode2 / fsr; if (ph2 >= 1.0) ph2 -= 1.0;
                        float y = std::sin ((float) ph * twoPi) + std::sin ((float) ph2 * twoPi) * mode2Amt * ampEnv;
                        y = y * ampEnv + noiseSample() * (snapEnv * s.snap + nzEnv * s.noise * 0.4f);
                        if (s.drive > 0.001f) y = drumDrive (y, s.drive, 1);
                        y = lp.lowpass (y) * vg; l[i] += y * gl; r[i] += y * gr;
                        ampEnv *= decayCoef; pitchEnvV *= pitchCoef; snapEnv *= snapCoef; nzEnv *= nzCoef;
                    }
                    break;
                }
            }
        }

        inline void renderClassic (float* l, float* r, int n, const DrumSound& s, float vg, float gl, float gr, float fsr)
        {
            const float d1 = std::exp (-4.6f / (juce::jmax (0.01f, s.o1Decay) * fsr)), d2 = std::exp (-4.6f / (juce::jmax (0.005f, s.o2Decay) * fsr));
            const float dn = std::exp (-4.6f / (juce::jmax (0.005f, s.nzDecay) * fsr)), pc = std::exp (-4.6f / (juce::jmax (0.002f, s.pitchTime) * fsr));
            nzFilt.set (fsr, 300.0f + 11000.0f * s.nzColour, 0.7f);
            auto wave = [] (int w, double p) -> float { switch (w) { case 0: return std::sin ((float) p * twoPi); case 1: return (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p); case 2: return (float) (2.0 * p - 1.0); default: return p < 0.5 ? 1.0f : -1.0f; } };
            for (int i = 0; i < n; ++i)
            {
                const float f1 = s.o1Pitch * std::pow (2.0f, s.o1PitchEnv * pitchEnvV / 12.0f);
                ph += f1 / fsr; if (ph >= 1.0) ph -= 1.0;
                ph2 += s.o2Pitch / fsr; if (ph2 >= 1.0) ph2 -= 1.0;
                float y = (s.o1Wave == 4 ? noiseSample() : wave (s.o1Wave, ph)) * ampEnv;
                y += (s.o2Wave == 4 ? noiseSample() : wave (s.o2Wave, ph2)) * o2Env * s.o2Level;
                y += nzFilt.lowpass (noiseSample()) * nzEnv * s.nzLevel * 1.5f;
                if (s.drive > 0.001f) y = drumDrive (y, s.drive, 1);
                y *= vg; l[i] += y * gl; r[i] += y * gr;
                ampEnv *= d1; o2Env *= d2; nzEnv *= dn; pitchEnvV *= pc;
            }
        }
    };

    //==========================================================================
    /** Base for the multi-sound drum synths: N sounds on consecutive keys from firstNote, choke groups, a master section. */
    template <int NumSounds, int MaxVoices>
    class DrumKitSynth : public SynthPluginBase<DrumVoice, MaxVoices>
    {
    public:
        using Base = SynthPluginBase<DrumVoice, MaxVoices>;
        using Base::bank; using Base::voices; using Base::sr;
        static constexpr int numSounds = NumSounds, firstNote = 36;
        DrumKitSynth (te::PluginCreationInfo i, bool classicEngine) : Base (i), classic (classicEngine) { for (auto& v : voices) v.classic = classicEngine; }

        juce::StringArray modDestinations() const override   { return { "pitch", "amp", "pan" }; }
        int soundForNote (int note) const   { const int i = note - firstNote; return juce::isPositiveAndBelow (i, numSounds) ? i : (note % numSounds); }
        int lastHitSound() const            { return lastHit.load(); }
        const DrumSound& sound (int i) const { return sounds[(size_t) juce::jlimit (0, numSounds - 1, i)]; }

        void noteOn (int note, float vel) override
        {
            const int si = soundForNote (note);
            // choke: a sound in a group silences the others in that group (closed / open hat)
            const int group = sounds[(size_t) si].choke;
            if (group > 0) for (auto& v : voices) if (v.active && v.soundIndex != si && sounds[(size_t) v.soundIndex].choke == group) v.kill();
            // the same sound retriggers (no stacking of a hat on itself)
            for (auto& v : voices) if (v.active && v.soundIndex == si) v.kill();
            if (auto* v = this->allocateVoice())
            {
                v->snd = &sounds[(size_t) si]; v->soundIndex = si; v->unisonGain = 1.0f; v->unisonDetune = 0; v->unisonPan = 0; v->bend = 0;
                v->start (note, vel, 0.0f, 0.0f);
                lastHit = si; this->lastNoteNumber = note;
            }
        }
        void noteOff (int) override {}   // drums are one-shots

        void renderMaster (float* l, float* r, int n) override
        {
            const float g = juce::Decibels::decibelsToGain (masterDb());
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
        virtual float masterDb() const = 0;

        /** Pad trigger from the editor (message thread): plays the sound through the normal path. */
        void triggerSound (int i, float vel = 1.0f)   { pendingHit = (juce::jlimit (0, numSounds - 1, i) + 1) | ((int) (vel * 127.0f) << 8); }

    protected:
        std::array<DrumSound, (size_t) NumSounds> sounds;
        bool classic; std::atomic<int> lastHit { 0 }, pendingHit { 0 };
        void handlePendingHit()   { const int h = pendingHit.exchange (0); if (h) noteOn (firstNote + (h & 0xff) - 1, ((h >> 8) & 0xff) / 127.0f); }
    };

    //==========================================================================
    // Carapace - 16 modelled voices
    class SqCarapacePlugin : public DrumKitSynth<16, 24>
    {
    public:
        static constexpr int perSound = 12;   // model, pitch, pitchEnv, pitchTime, decay, tone, noise, snap, drive, level, pan, choke
        enum P { soundFirst = 0, masterLevel = soundFirst + 16 * perSound, masterTone, masterDrive, masterVel, numParams };
        static constexpr const char* xmlTypeName = "sqmCarapace";
        static const char* getPluginName()   { return NEEDS_TRANS("Carapace"); }
        juce::String getName() const override { return "Carapace"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Crpc"; }
        juce::String getSelectableDescription() override { return "Carapace percussion modelling drum synth"; }
        static juce::StringArray soundNames()   { return { "Kick", "Snare", "Rim", "Clap", "Closed Hat", "Pedal Hat", "Open Hat", "Low Tom", "Mid Tom", "High Tom", "Crash", "Ride", "Clave", "Cowbell", "Conga", "Shaker" }; }

        SqCarapacePlugin (te::PluginCreationInfo info) : DrumKitSynth<16, 24> (info, false)
        {
            bank.create (*this, specs());
        }
        ~SqCarapacePlugin() override {}

        static const std::vector<SqDsp::ParamSpec>& specs()
        {
            static std::vector<std::unique_ptr<juce::String>> store;
            static const std::vector<SqDsp::ParamSpec> v = []
            {
                std::vector<SqDsp::ParamSpec> out;
                auto keep = [] (const juce::String& t) { store.push_back (std::make_unique<juce::String> (t)); return store.back()->toRawUTF8(); };
                // defaults per sound: model, pitch (MIDI note), pitchEnv, pitchTime, decay, tone, noise, snap, drive, level, pan, choke
                struct D { int model; float pitch, pEnv, pTime, decay, tone, noise, snap, drive, level, pan; int choke; };
                static const D d[16] = {
                    { 0, 36, 30, 0.04f, 0.45f, 0.4f, 0.05f, 0.4f, 0.1f, 0.9f, 0, 0 }, { 2, 60, 0, 0.02f, 0.25f, 0.55f, 0.6f, 0.5f, 0.05f, 0.8f, 0, 0 }, { 3, 84, 0, 0.01f, 0.08f, 0.5f, 0.2f, 0.4f, 0, 0.7f, -0.2f, 0 },
                    { 2, 72, 0, 0.02f, 0.18f, 0.7f, 0.9f, 0.6f, 0.1f, 0.75f, 0.1f, 0 }, { 1, 72, 0, 0.01f, 0.06f, 0.6f, 0.3f, 0.3f, 0, 0.6f, 0.3f, 1 }, { 1, 70, 0, 0.01f, 0.04f, 0.5f, 0.3f, 0.2f, 0, 0.5f, 0.3f, 1 },
                    { 1, 72, 0, 0.01f, 0.6f, 0.6f, 0.3f, 0.3f, 0, 0.6f, 0.3f, 1 }, { 4, 45, 10, 0.08f, 0.5f, 0.4f, 0.05f, 0.2f, 0, 0.8f, -0.4f, 0 }, { 4, 50, 10, 0.08f, 0.45f, 0.45f, 0.05f, 0.2f, 0, 0.8f, 0, 0 },
                    { 4, 55, 10, 0.08f, 0.4f, 0.5f, 0.05f, 0.2f, 0, 0.8f, 0.4f, 0 }, { 1, 76, 0, 0.01f, 1.6f, 0.75f, 0.5f, 0.3f, 0, 0.55f, -0.5f, 0 }, { 1, 79, 0, 0.01f, 1.2f, 0.65f, 0.2f, 0.2f, 0, 0.5f, 0.5f, 0 },
                    { 3, 95, 0, 0.01f, 0.1f, 0.8f, 0.1f, 0.3f, 0, 0.7f, 0.2f, 0 }, { 3, 76, 0, 0.01f, 0.25f, 0.3f, 0.1f, 0.3f, 0.1f, 0.7f, -0.2f, 0 }, { 4, 62, 4, 0.05f, 0.3f, 0.5f, 0.1f, 0.3f, 0, 0.75f, 0.3f, 0 },
                    { 2, 90, 0, 0.01f, 0.08f, 0.9f, 1.0f, 0.2f, 0, 0.55f, -0.3f, 0 } };
                auto names = soundNames();
                for (int i = 0; i < 16; ++i)
                {
                    const juce::String p = "s" + juce::String (i + 1), nm = names[i] + " ";
                    out.push_back ({ keep (p + "Model"), keep (nm + "Model"), 0, 4, (float) d[i].model, 1, "", 0 });
                    out.push_back ({ keep (p + "Pitch"), keep (nm + "Pitch"), 24, 108, d[i].pitch, 1, "", 0 });
                    out.push_back ({ keep (p + "PitchEnv"), keep (nm + "Pitch Env"), 0, 60, d[i].pEnv, 1, " st", 0 });
                    out.push_back ({ keep (p + "PitchTime"), keep (nm + "Pitch Time"), 0.002f, 0.5f, d[i].pTime, 0.4f, " s", 3 });
                    out.push_back ({ keep (p + "Decay"), keep (nm + "Decay"), 0.01f, 3, d[i].decay, 0.4f, " s", 2 });
                    out.push_back ({ keep (p + "Tone"), keep (nm + "Tone"), 0, 1, d[i].tone, 1, "", 2 });
                    out.push_back ({ keep (p + "Noise"), keep (nm + "Noise"), 0, 1, d[i].noise, 1, "", 2 });
                    out.push_back ({ keep (p + "Snap"), keep (nm + "Snap"), 0, 1, d[i].snap, 1, "", 2 });
                    out.push_back ({ keep (p + "Drive"), keep (nm + "Drive"), 0, 1, d[i].drive, 1, "", 2 });
                    out.push_back ({ keep (p + "Level"), keep (nm + "Level"), 0, 1, d[i].level, 1, "", 2 });
                    out.push_back ({ keep (p + "Pan"), keep (nm + "Pan"), -1, 1, d[i].pan, 1, "", 2 });
                    out.push_back ({ keep (p + "Choke"), keep (nm + "Choke"), 0, 4, (float) d[i].choke, 1, "", 0 });
                }
                out.push_back ({ "masterLevel", "Level", -24, 6, -3, 1, " dB", 1 });
                out.push_back ({ "masterTone", "Master Tone", 0, 1, 0.5f, 1, "", 2 });
                out.push_back ({ "masterDrive", "Master Drive", 0, 1, 0, 1, "", 2 });
                out.push_back ({ "masterVel", "Velocity", 0, 1, 0.7f, 1, "", 2 });
                return out;
            }();
            return v;
        }

        float masterDb() const override   { return bank[masterLevel]; }
        void updateChunk (const ModValues&) override
        {
            for (int i = 0; i < 16; ++i)
            {
                auto& s = sounds[(size_t) i]; const size_t b = (size_t) (soundFirst + i * perSound);
                s.model = (int) std::lround (bank[b]); s.pitch = bank[b + 1]; s.pitchEnv = bank[b + 2]; s.pitchTime = bank[b + 3]; s.decay = bank[b + 4];
                s.tone = juce::jlimit (0.0f, 1.0f, bank[b + 5] + (bank[masterTone] - 0.5f) * 0.6f); s.noise = bank[b + 6]; s.snap = bank[b + 7]; s.drive = juce::jmin (1.0f, bank[b + 8] + bank[masterDrive]); s.level = bank[b + 9]; s.pan = bank[b + 10]; s.choke = (int) std::lround (bank[b + 11]);
            }
            handlePendingHit();
        }
    };

    //==========================================================================
    // Live Drum Synth - 12 classic two-oscillator voices
    class SqLiveDrumSynthPlugin : public DrumKitSynth<12, 24>
    {
    public:
        static constexpr int perSound = 14;   // o1Wave o1Pitch o1PitchEnv pitchTime o1Decay o2Wave o2Pitch o2Level o2Decay nzLevel nzColour nzDecay level pan
        enum P { soundFirst = 0, masterLevel = soundFirst + 12 * perSound, masterDrive, numParams };
        static constexpr const char* xmlTypeName = "sqmLiveDrumSynth";
        static const char* getPluginName()   { return NEEDS_TRANS("Live Drum Synth"); }
        juce::String getName() const override { return "Live Drum Synth"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "LDS"; }
        juce::String getSelectableDescription() override { return "Live Drum Synth two-oscillator drum synth"; }
        static juce::StringArray soundNames()   { return { "Kick", "Snare", "Rim", "Clap", "Closed Hat", "Open Hat", "Low Tom", "High Tom", "Ride", "Crash", "Perc", "Zap" }; }

        SqLiveDrumSynthPlugin (te::PluginCreationInfo info) : DrumKitSynth<12, 24> (info, true)   { bank.create (*this, specs()); }
        ~SqLiveDrumSynthPlugin() override {}

        static const std::vector<SqDsp::ParamSpec>& specs()
        {
            static std::vector<std::unique_ptr<juce::String>> store;
            static const std::vector<SqDsp::ParamSpec> v = []
            {
                std::vector<SqDsp::ParamSpec> out;
                auto keep = [] (const juce::String& t) { store.push_back (std::make_unique<juce::String> (t)); return store.back()->toRawUTF8(); };
                struct D { int w1; float p1, pe, pt, d1; int w2; float p2, l2, d2, nl, nc, nd, lvl, pan; };
                static const D d[12] = {
                    { 0, 50, 36, 0.03f, 0.5f, 0, 180, 0.1f, 0.02f, 0.15f, 0.3f, 0.01f, 0.9f, 0 }, { 1, 180, 12, 0.02f, 0.18f, 0, 330, 0.3f, 0.1f, 0.7f, 0.6f, 0.25f, 0.8f, 0 }, { 3, 800, 0, 0.01f, 0.03f, 0, 1800, 0.4f, 0.02f, 0.3f, 0.7f, 0.03f, 0.6f, -0.2f },
                    { 4, 1000, 0, 0.01f, 0.01f, 4, 2000, 0.5f, 0.05f, 0.9f, 0.6f, 0.15f, 0.7f, 0.1f }, { 3, 4000, 0, 0.01f, 0.03f, 3, 6100, 0.5f, 0.03f, 0.7f, 0.9f, 0.05f, 0.6f, 0.3f }, { 3, 4000, 0, 0.01f, 0.3f, 3, 6100, 0.5f, 0.3f, 0.7f, 0.9f, 0.4f, 0.55f, 0.3f },
                    { 0, 90, 12, 0.05f, 0.5f, 1, 95, 0.2f, 0.3f, 0.1f, 0.3f, 0.05f, 0.8f, -0.4f }, { 0, 160, 12, 0.05f, 0.4f, 1, 170, 0.2f, 0.25f, 0.1f, 0.3f, 0.05f, 0.8f, 0.4f }, { 3, 5200, 0, 0.01f, 1.2f, 3, 8300, 0.4f, 0.8f, 0.3f, 0.8f, 0.6f, 0.5f, 0.5f },
                    { 4, 3000, 0, 0.01f, 1.5f, 3, 4900, 0.3f, 1.0f, 0.9f, 0.7f, 1.4f, 0.5f, -0.5f }, { 0, 400, 24, 0.02f, 0.15f, 1, 600, 0.2f, 0.05f, 0.2f, 0.5f, 0.05f, 0.7f, 0.2f }, { 2, 2000, 60, 0.08f, 0.2f, 2, 50, 0.3f, 0.2f, 0.1f, 0.5f, 0.05f, 0.7f, 0 } };
                auto names = soundNames();
                for (int i = 0; i < 12; ++i)
                {
                    const juce::String p = "s" + juce::String (i + 1), nm = names[i] + " ";
                    out.push_back ({ keep (p + "O1Wave"), keep (nm + "Osc 1"), 0, 4, (float) d[i].w1, 1, "", 0 });
                    out.push_back ({ keep (p + "O1Pitch"), keep (nm + "Osc 1 Pitch"), 20, 8000, d[i].p1, 0.3f, " Hz", 0 });
                    out.push_back ({ keep (p + "O1PitchEnv"), keep (nm + "Pitch Env"), 0, 72, d[i].pe, 1, " st", 0 });
                    out.push_back ({ keep (p + "PitchTime"), keep (nm + "Pitch Time"), 0.002f, 0.5f, d[i].pt, 0.4f, " s", 3 });
                    out.push_back ({ keep (p + "O1Decay"), keep (nm + "Osc 1 Decay"), 0.005f, 3, d[i].d1, 0.4f, " s", 2 });
                    out.push_back ({ keep (p + "O2Wave"), keep (nm + "Osc 2"), 0, 4, (float) d[i].w2, 1, "", 0 });
                    out.push_back ({ keep (p + "O2Pitch"), keep (nm + "Osc 2 Pitch"), 20, 12000, d[i].p2, 0.3f, " Hz", 0 });
                    out.push_back ({ keep (p + "O2Level"), keep (nm + "Osc 2 Level"), 0, 1, d[i].l2, 1, "", 2 });
                    out.push_back ({ keep (p + "O2Decay"), keep (nm + "Osc 2 Decay"), 0.005f, 3, d[i].d2, 0.4f, " s", 2 });
                    out.push_back ({ keep (p + "NzLevel"), keep (nm + "Noise"), 0, 1, d[i].nl, 1, "", 2 });
                    out.push_back ({ keep (p + "NzColour"), keep (nm + "Noise Colour"), 0, 1, d[i].nc, 1, "", 2 });
                    out.push_back ({ keep (p + "NzDecay"), keep (nm + "Noise Decay"), 0.005f, 3, d[i].nd, 0.4f, " s", 2 });
                    out.push_back ({ keep (p + "Level"), keep (nm + "Level"), 0, 1, d[i].lvl, 1, "", 2 });
                    out.push_back ({ keep (p + "Pan"), keep (nm + "Pan"), -1, 1, d[i].pan, 1, "", 2 });
                }
                out.push_back ({ "masterLevel", "Level", -24, 6, -3, 1, " dB", 1 });
                out.push_back ({ "masterDrive", "Drive", 0, 1, 0, 1, "", 2 });
                return out;
            }();
            return v;
        }
        float masterDb() const override   { return bank[masterLevel]; }
        void updateChunk (const ModValues&) override
        {
            for (int i = 0; i < 12; ++i)
            {
                auto& s = sounds[(size_t) i]; const size_t b = (size_t) (soundFirst + i * perSound);
                s.o1Wave = (int) std::lround (bank[b]); s.o1Pitch = bank[b + 1]; s.o1PitchEnv = bank[b + 2]; s.pitchTime = bank[b + 3]; s.o1Decay = bank[b + 4];
                s.o2Wave = (int) std::lround (bank[b + 5]); s.o2Pitch = bank[b + 6]; s.o2Level = bank[b + 7]; s.o2Decay = bank[b + 8];
                s.nzLevel = bank[b + 9]; s.nzColour = bank[b + 10]; s.nzDecay = bank[b + 11]; s.level = bank[b + 12]; s.pan = bank[b + 13]; s.drive = bank[masterDrive];
                s.choke = (i == 4 || i == 5) ? 1 : 0;   // hats choke each other
            }
            handlePendingHit();
        }
    };

    //==========================================================================
    // Drum Pad Model - one modelled voice, all knobs on the panel
    class SqDrumPadModelPlugin : public DrumKitSynth<1, 4>
    {
    public:
        enum P { model, pitch, pitchEnv, pitchTime, decay, tone, noise, snap, drive, keyTrack, velSens, level, numParams };
        static constexpr const char* xmlTypeName = "sqmDrumPadModel";
        static const char* getPluginName()   { return NEEDS_TRANS("Drum Pad Model"); }
        juce::String getName() const override { return "Drum Pad Model"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Pad"; }
        juce::String getSelectableDescription() override { return "Drum Pad Model percussion voice"; }
        SqDrumPadModelPlugin (te::PluginCreationInfo info) : DrumKitSynth<1, 4> (info, false)
        {
            bank.create (*this, {
                { "model", "Model", 0, 4, 0, 1, "", 0 }, { "pitch", "Pitch", 24, 108, 48, 1, "", 0 }, { "pitchEnv", "Pitch Env", 0, 60, 24, 1, " st", 0 }, { "pitchTime", "Pitch Time", 0.002f, 0.5f, 0.04f, 0.4f, " s", 3 },
                { "decay", "Decay", 0.01f, 3, 0.4f, 0.4f, " s", 2 }, { "tone", "Tone", 0, 1, 0.5f, 1, "", 2 }, { "noise", "Noise", 0, 1, 0.2f, 1, "", 2 }, { "snap", "Snap", 0, 1, 0.3f, 1, "", 2 }, { "drive", "Drive", 0, 1, 0, 1, "", 2 },
                { "keyTrack", "Key Track", 0, 1, 1, 1, "", 2 }, { "velSens", "Velocity", 0, 1, 0.7f, 1, "", 2 }, { "level", "Level", -24, 6, -3, 1, " dB", 1 } });
        }
        ~SqDrumPadModelPlugin() override {}
        int soundForNoteAny (int) const   { return 0; }
        void noteOn (int note, float vel) override
        {
            // every key plays the one sound; Key Track shifts its pitch from C3
            for (auto& v : voices) if (v.active) v.kill();
            if (auto* v = this->allocateVoice())
            {
                v->snd = &sounds[0]; v->soundIndex = 0; v->unisonGain = 1.0f; v->bend = 0;
                v->start (note, vel, 0.0f, 0.0f);
                v->hz = SqDsp::noteHz (note) * std::pow (2.0, (double) ((note - 60) * bank[keyTrack]) / 12.0); v->targetHz = v->hz;   // renderModel scales the sound's pitch by hz / noteHz (note)
                lastHit = 0; lastNoteNumber = note;
            }
        }
        float masterDb() const override   { return bank[level]; }
        void updateChunk (const ModValues&) override
        {
            auto& s = sounds[0];
            s.model = (int) std::lround (bank[model]); s.pitch = bank[pitch]; s.pitchEnv = bank[pitchEnv]; s.pitchTime = bank[pitchTime]; s.decay = bank[decay]; s.tone = bank[tone]; s.noise = bank[noise]; s.snap = bank[snap]; s.drive = bank[drive]; s.level = 0.9f; s.pan = 0; s.choke = 0;
            handlePendingHit();
        }
    };

    //==========================================================================
    // Thump Live - the kick with drawable envelopes and a kick pattern
    struct KickLiveVoice : public VoiceBase
    {
        double phase = 0, sampleT = 0; float clickEnv = 0; juce::uint32 rng = 5; SqDsp::SVF clickBp, tone;
        struct Env { float t[4], v[4]; };   // 4 breakpoints: (time seconds, value), t[0] = 0
        struct S { Env pitch, amp, dist; float baseHz = 50, clickLevel = 0.3f, clickTone = 3000, clickDecay = 0.005f, sub = 0.2f, toneHz = 9000, velSens = 0.5f, gain = 1.0f; int driveType = 1; } s;
        void start (int n, float vel, float, float)   { startBase (n, vel, 0, 0); phase = 0; sampleT = 0; clickEnv = 1.0f; clickBp.reset(); }
        static inline float envAt (const Env& e, float t)
        {
            for (int i = 0; i < 3; ++i)
                if (t < e.t[i + 1]) { const float span = juce::jmax (0.0005f, e.t[i + 1] - e.t[i]); return e.v[i] + (e.v[i + 1] - e.v[i]) * (t - e.t[i]) / span; }
            return e.v[3];
        }
        inline void render (float* l, float* r, int n)
        {
            const double baseHz = hz * (s.baseHz / (float) SqDsp::noteHz (48));   // C3 plays the base pitch
            clickBp.set (sr, s.clickTone, 1.5f); tone.set (sr, s.toneHz, 0.7f);
            const float clickCoef = std::exp (-4.6f / (juce::jmax (0.001f, s.clickDecay) * (float) sr));
            const float vg = (1.0f - s.velSens + s.velSens * velocity) * s.gain * unisonGain;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) (sampleT / sr);
                const float semis = envAt (s.pitch, t), amp = envAt (s.amp, t), dist = envAt (s.dist, t);
                const double f = baseHz * std::pow (2.0, (double) semis / 12.0);
                phase += f / sr; if (phase >= 1.0) phase -= 1.0;
                float body = std::sin ((float) phase * twoPi) + std::sin ((float) phase * twoPi * 0.5f) * s.sub;
                rng = rng * 1664525u + 1013904223u; const float nz = (float) ((rng >> 8) & 0xffffff) / 8388608.0f - 1.0f;
                float y = body * amp + clickBp.bandpass (nz) * clickEnv * s.clickLevel * 2.0f;
                if (dist > 0.001f) y = drumDrive (y, dist, s.driveType);
                y = tone.lowpass (y) * vg;
                l[i] += y; r[i] += y;
                clickEnv *= clickCoef; sampleT += 1.0;
            }
            if (sampleT / sr > s.amp.t[3] + 0.05 || (sampleT / sr > 0.05 && envAt (s.amp, (float) (sampleT / sr)) < 0.001f && sampleT / sr > s.amp.t[2])) active = false;
        }
    };

    class SqThumpLivePlugin : public SynthPluginBase<KickLiveVoice, 4>
    {
    public:
        static constexpr int seqSteps = 16;
        enum P { baseHz, pT1, pT2, pT3, pV0, pV1, pV2, pV3, aT1, aT2, aT3, aV0, aV1, aV2, aV3, dT1, dT2, dT3, dV0, dV1, dV2, dV3,
                 clickLevel, clickTone, clickDecay, sub, toneHz, driveType, velSens, patOn, patRate, patStep, level = patStep + seqSteps, numParams };
        static constexpr const char* xmlTypeName = "sqmThumpLive";
        static const char* getPluginName()   { return NEEDS_TRANS("Thump Live"); }
        juce::String getName() const override { return "Thump Live"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "ThLv"; }
        juce::String getSelectableDescription() override { return "Thump Live kick with drawable envelopes"; }
        double getTailLength() const override { return 2.0; }

        SqThumpLivePlugin (te::PluginCreationInfo info) : SynthPluginBase<KickLiveVoice, 4> (info)
        {
            std::vector<SqDsp::ParamSpec> specs = {
                { "baseHz", "Pitch (C3)", 25, 200, 50, 0.5f, " Hz", 0 },
                { "pT1", "Pitch T1", 0.001f, 1, 0.012f, 0.4f, " s", 3 }, { "pT2", "Pitch T2", 0.002f, 1.5f, 0.06f, 0.4f, " s", 3 }, { "pT3", "Pitch T3", 0.005f, 2, 0.3f, 0.4f, " s", 3 },
                { "pV0", "Pitch V0", 0, 72, 42, 1, " st", 0 }, { "pV1", "Pitch V1", 0, 72, 18, 1, " st", 0 }, { "pV2", "Pitch V2", 0, 72, 2, 1, " st", 0 }, { "pV3", "Pitch V3", 0, 72, 0, 1, " st", 0 },
                { "aT1", "Amp T1", 0.001f, 1, 0.004f, 0.4f, " s", 3 }, { "aT2", "Amp T2", 0.002f, 2, 0.15f, 0.4f, " s", 3 }, { "aT3", "Amp T3", 0.005f, 3, 0.6f, 0.4f, " s", 3 },
                { "aV0", "Amp V0", 0, 1, 0.6f, 1, "", 2 }, { "aV1", "Amp V1", 0, 1, 1.0f, 1, "", 2 }, { "aV2", "Amp V2", 0, 1, 0.5f, 1, "", 2 }, { "aV3", "Amp V3", 0, 1, 0.0f, 1, "", 2 },
                { "dT1", "Dist T1", 0.001f, 1, 0.01f, 0.4f, " s", 3 }, { "dT2", "Dist T2", 0.002f, 2, 0.1f, 0.4f, " s", 3 }, { "dT3", "Dist T3", 0.005f, 3, 0.5f, 0.4f, " s", 3 },
                { "dV0", "Dist V0", 0, 1, 0.4f, 1, "", 2 }, { "dV1", "Dist V1", 0, 1, 0.25f, 1, "", 2 }, { "dV2", "Dist V2", 0, 1, 0.1f, 1, "", 2 }, { "dV3", "Dist V3", 0, 1, 0.0f, 1, "", 2 },
                { "clickLevel", "Click", 0, 1, 0.3f, 1, "", 2 }, { "clickTone", "Click Tone", 500, 12000, 3000, 0.4f, " Hz", 0 }, { "clickDecay", "Click Decay", 0.001f, 0.05f, 0.005f, 0.5f, " s", 3 },
                { "sub", "Sub", 0, 1, 0.2f, 1, "", 2 }, { "toneHz", "Tone", 500, 20000, 9000, 0.3f, " Hz", 0 }, { "driveType", "Drive Type", 0, 3, 1, 1, "", 0 }, { "velSens", "Velocity", 0, 1, 0.5f, 1, "", 2 },
                { "patOn", "Pattern", 0, 1, 0, 1, "", 0 }, { "patRate", "Pattern Rate", 0, 2, 1, 1, "", 0 } };
            static std::vector<std::unique_ptr<juce::String>> store;
            static const std::vector<SqDsp::ParamSpec> stepSpecsV = []
            {
                std::vector<SqDsp::ParamSpec> v;
                for (int i = 0; i < seqSteps; ++i)
                {
                    store.push_back (std::make_unique<juce::String> ("pat" + juce::String (i + 1))); store.push_back (std::make_unique<juce::String> ("Pattern Step " + juce::String (i + 1)));
                    v.push_back ({ store[store.size() - 2]->toRawUTF8(), store.back()->toRawUTF8(), 0, 1, (i % 4 == 0 || i == 10) ? 1.0f : 0.0f, 1, "", 0 });
                }
                return v;
            }();
            specs.insert (specs.end(), stepSpecsV.begin(), stepSpecsV.end());
            specs.push_back ({ "level", "Level", -24, 6, -3, 1, " dB", 1 });
            jassert ((int) specs.size() == numParams);
            bank.create (*this, specs);
            maxPoly = 2;
        }
        ~SqThumpLivePlugin() override {}
        bool patternOn() const   { return bank[patOn] > 0.5f; }
        int currentStep() const  { return patStepNow; }

        void noteOn (int note, float vel) override
        {
            if (! patternOn()) { SynthPluginBase::noteOn (note, vel); return; }
            patHeld.removeAllInstancesOf (note); patHeld.add (note); patVel = vel;
            if (patHeld.size() == 1 && ! transportPlaying) { patFreeBeat = 0; patStepNow = -1; }
        }
        void noteOff (int note) override
        {
            if (! patternOn()) { SynthPluginBase::noteOff (note); return; }
            patHeld.removeAllInstancesOf (note); if (patHeld.isEmpty()) patStepNow = -1;
        }

        void updateChunk (const ModValues&) override
        {
            for (auto& v : voices)
            {
                auto& s = v.s;
                s.pitch.t[0] = 0; s.pitch.t[1] = bank[pT1]; s.pitch.t[2] = juce::jmax (bank[pT1], bank[pT2]); s.pitch.t[3] = juce::jmax (s.pitch.t[2], bank[pT3]); s.pitch.v[0] = bank[pV0]; s.pitch.v[1] = bank[pV1]; s.pitch.v[2] = bank[pV2]; s.pitch.v[3] = bank[pV3];
                s.amp.t[0] = 0; s.amp.t[1] = bank[aT1]; s.amp.t[2] = juce::jmax (bank[aT1], bank[aT2]); s.amp.t[3] = juce::jmax (s.amp.t[2], bank[aT3]); s.amp.v[0] = bank[aV0]; s.amp.v[1] = bank[aV1]; s.amp.v[2] = bank[aV2]; s.amp.v[3] = bank[aV3];
                s.dist.t[0] = 0; s.dist.t[1] = bank[dT1]; s.dist.t[2] = juce::jmax (bank[dT1], bank[dT2]); s.dist.t[3] = juce::jmax (s.dist.t[2], bank[dT3]); s.dist.v[0] = bank[dV0]; s.dist.v[1] = bank[dV1]; s.dist.v[2] = bank[dV2]; s.dist.v[3] = bank[dV3];
                s.baseHz = bank[baseHz]; s.clickLevel = bank[clickLevel]; s.clickTone = bank[clickTone]; s.clickDecay = bank[clickDecay]; s.sub = bank[sub]; s.toneHz = bank[toneHz]; s.driveType = (int) std::lround (bank[driveType]); s.velSens = bank[velSens]; s.gain = 0.9f;
            }
            if (patternOn() && ! patHeld.isEmpty())
            {
                static const double rates[3] = { 0.5, 0.25, 0.125 };
                const double stepBeats = rates[juce::jlimit (0, 2, (int) std::lround (bank[patRate]))];
                double beat; if (transportPlaying) beat = chunkBeat(); else { beat = patFreeBeat; patFreeBeat += (double) chunk / sr * bpm / 60.0; }
                const int st = (int) std::floor (beat / stepBeats);
                if (st != patStepNow)
                {
                    patStepNow = st;
                    if (bank[(size_t) (patStep + ((st % seqSteps) + seqSteps) % seqSteps)] > 0.5f)
                        if (auto* v = allocateVoice()) { v->unisonGain = 1.0f; v->bend = 0; v->start (patHeld.getLast(), patVel, 0, 0); lastNoteNumber = patHeld.getLast(); }
                }
            }
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float g = juce::Decibels::decibelsToGain (bank[level]);
            juce::FloatVectorOperations::multiply (l, g, n); juce::FloatVectorOperations::multiply (r, g, n);
        }
    private:
        juce::Array<int> patHeld; float patVel = 1.0f; int patStepNow = -1; double patFreeBeat = 0;
    };
}
