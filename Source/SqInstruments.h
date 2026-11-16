#pragma once
// Squelchmoth native instruments (original DSP, no samples):
//
//  Moth 808  - a dedicated 808 bass. Sine body with a pitch "punch" envelope, harmonics, sub octave and a click
//              layer, then drive in four flavours (tube, tape, hard clip, fold) at 4x oversampling so it doesn't
//              alias, a tone filter, and real glides: FL-style slides when notes overlap, always-glide, or off.
//  Blossom   - a physical-modelling string instrument.
//              Each voice has two coupled waveguide strings (bridge and nut sections with the exciter between them),
//              stiffness (dispersion) and damping set from a material pad, pluck / strike / bow exciters (the bow is a
//              real slip-stick friction junction), two pickups, an 8-mode body resonator with seven body models,
//              sympathetic strings shared by every voice, vibrato, stereo spread, chorus, delay and reverb. 32 voices.
//
// Both use no CPU while silent, protect against denormals, and expose every setting to automation and Claude.

#include <JuceHeader.h>

namespace te = tracktion;

namespace SqDsp
{
    struct ParamSpec { const char* id; const char* name; float lo, hi, def, skew; const char* unit; int decimals; };

    /** A set of automatable parameters backed by the plugin's state (saved with the session, undoable). */
    /** A number with a fixed count of decimals (0 = a whole number - juce::String (x, 0) would print every digit). */
    inline juce::String numText (double v, int decimals)   { return decimals <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals); }

    struct ParamBank
    {
        std::vector<std::unique_ptr<juce::CachedValue<float>>> values;
        std::vector<te::AutomatableParameter::Ptr> params;
        std::vector<ParamSpec> specs;

        void create (te::Plugin& p, const std::vector<ParamSpec>& list)
        {
            specs = list;
            for (auto& s : specs)
            {
                auto cv = std::make_unique<juce::CachedValue<float>> ();
                cv->referTo (p.state, s.id, p.getUndoManager(), s.def);
                const auto unit = juce::String (s.unit);
                const int dec = s.decimals;
                auto toText = [unit, dec] (float v) { return numText (v, dec) + unit; };
                auto fromText = [] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getFloatValue(); };
                auto ap = p.addParam (s.id, s.name, juce::NormalisableRange<float> (s.lo, s.hi, 0.0f, s.skew), toText, fromText);
                ap->attachToCurrentValue (*cv);
                values.push_back (std::move (cv));
                params.push_back (ap);
            }
        }
        void detach()   { for (auto& ap : params) ap->detachFromCurrentValue(); }
        float operator[] (size_t i) const   { return params[i]->getCurrentValue(); }
    };

    /** Implemented by Squelchmoth's own plug-ins: presets start from defaults (parameters AND mode settings). */
    struct BankOwner
    {
        virtual ~BankOwner() = default;
        virtual ParamBank& getBank() = 0;
        virtual void resetProps (juce::UndoManager*) {}
    };

    inline float fastTanh (float x)   { return std::tanh (x); }

    /** Two-pole state-variable lowpass (TPT), stable under fast modulation. */
    struct SVF
    {
        float ic1 = 0, ic2 = 0, g = 0, k = 1.414f, a1 = 0, a2 = 0, a3 = 0;
        void set (double sr, float cutoff, float q = 0.707f)
        {
            g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.49, (double) cutoff) / sr);
            k = 1.0f / juce::jmax (0.1f, q);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        float lowpass (float v0)
        {
            float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            return v2;
        }
        float bandpass (float v0)
        {
            float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            return v1;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    /** Fractional delay line with cubic (Hermite) interpolation. */
    struct Delay
    {
        std::vector<float> buf; int w = 0, mask = 0;
        void prepare (int maxSamples)
        {
            int n = 1; while (n < maxSamples + 4) n <<= 1;
            buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        float read (float delay) const
        {
            delay = juce::jlimit (2.0f, (float) mask - 3.0f, delay);   // >= 2 so the cubic never reads the slot about to be written
            const float rp = (float) w - delay;
            int i = (int) std::floor (rp);
            const float f = rp - (float) i;
            auto at = [this] (int k) { return buf[(size_t) (k & mask)]; };
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
    };

    inline double noteHz (double note)   { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
}

//==============================================================================
//==============================================================================
class Sq808Plugin : public te::Plugin
{
public:
    enum P { tune, fine, punch, punchTime, decay, release, click, sub, harmonics, drive, tone, glide, velSens, level, numParams };

    Sq808Plugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        bank.create (*this, {
            { "tune",      "Tune",        -24.0f, 24.0f,  0.0f,  1.0f, " st", 1 },
            { "fine",      "Fine",        -100.0f, 100.0f, 0.0f, 1.0f, " ct", 0 },
            { "punch",     "Punch",       0.0f,  36.0f, 12.0f,  1.0f, " st", 1 },
            { "punchTime", "Punch Time",  0.005f, 0.3f, 0.045f, 0.4f, " s", 3 },
            { "decay",     "Decay",       0.05f, 8.0f,  1.6f,   0.4f, " s", 2 },
            { "release",   "Release",     0.01f, 3.0f,  0.18f,  0.4f, " s", 2 },
            { "click",     "Click",       0.0f,  1.0f,  0.25f,  1.0f, "", 2 },
            { "sub",       "Sub",         0.0f,  1.0f,  0.0f,   1.0f, "", 2 },
            { "harmonics", "Harmonics",   0.0f,  1.0f,  0.2f,   1.0f, "", 2 },
            { "drive",     "Drive",       0.0f,  1.0f,  0.25f,  1.0f, "", 2 },
            { "tone",      "Tone Cutoff", 100.0f, 20000.0f, 9000.0f, 0.3f, " Hz", 0 },
            { "glide",     "Glide",       0.0f,  1.0f,  0.12f,  0.5f, " s", 3 },
            { "velSens",   "Velocity",    0.0f,  1.0f,  0.5f,   1.0f, "", 2 },
            { "level",     "Level",       -24.0f, 6.0f, 0.0f,   1.0f, " dB", 1 } });
        auto um = getUndoManager();
        driveType.referTo (state, "driveType", um, 0);
        glideMode.referTo (state, "glideMode", um, 0);
        polyMode.referTo (state, "polyMode", um, 0);
        bendRange.referTo (state, "bendRange", um, 12);
    }
    ~Sq808Plugin() override { notifyListenersOfDeletion(); bank.detach(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Moth 808"); }
    static constexpr const char* xmlTypeName = "sqm808";
    juce::String getName() const override               { return "Moth 808"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "808"; }
    juce::String getSelectableDescription() override    { return "Moth 808 bass"; }
    bool isSynth() override                             { return true; }
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool needsConstantBufferSize() override             { return false; }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override               { return 8.0; }

    static juce::StringArray driveTypeNames()   { return { "TUBE", "TAPE", "HARD", "FOLD" }; }
    static juce::StringArray glideModeNames()   { return { "SLIDE", "ALWAYS", "OFF" }; }

    juce::CachedValue<int> driveType, glideMode, polyMode, bendRange;
    SqDsp::ParamBank bank;

    // for the editor's live display
    std::atomic<float> displayNote { -1.0f }, displayEnv { 0.0f };
    std::array<float, 512> scope {};
    std::atomic<int> scopeWrite { 0 };

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        maxBlock = juce::jmax (64, info.blockSizeSamples);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing ((size_t) chunk);
        mono.setSize (1, chunk);
        for (auto& v : voices) v = {};
        dcX = dcY = 0;
        toneL.reset();
    }
    void deinitialise() override { oversampler.reset(); }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        for (auto ap : getAutomatableParameters()) ap->updateFromAttachedValue();
    }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || oversampler == nullptr) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& out = *fc.destBuffer;
        const int start = fc.bufferStartSample, num = fc.bufferNumSamples;
        for (int c = 0; c < out.getNumChannels(); ++c) out.clear (c, start, num);

        // gather MIDI with sample positions
        midiEvents.clearQuick();
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) { held.clearQuick(); for (auto& v : voices) v.releasing = true; }
            for (auto& m : *fc.bufferForMidiMessages)
                midiEvents.add ({ juce::jlimit (0, num - 1, juce::roundToInt (m.getTimeStamp() * sr)), juce::MidiMessage (m) });
        }

        bool anyActive = false;
        for (auto& v : voices) anyActive = anyActive || v.active;
        if (! anyActive && midiEvents.isEmpty()) { displayEnv = 0.0f; return; }

        int ev = 0;
        for (int pos = 0; pos < num; pos += chunk)
        {
            const int n = juce::jmin (chunk, num - pos);
            auto* m = mono.getWritePointer (0);
            for (int i = 0; i < n; ++i)
            {
                while (ev < midiEvents.size() && midiEvents[ev].sample <= pos + i) handleMidi (midiEvents[ev++].msg);
                m[i] = renderSample();
            }
            // drive at 4x so the harmonics it creates don't fold back
            juce::dsp::AudioBlock<float> block (mono.getArrayOfWritePointers(), 1, (size_t) n);
            auto up = oversampler->processSamplesUp (block);
            const float drv = bank[drive];
            const float g = 1.0f + drv * drv * 29.0f;
            const int type = driveType.get();
            auto* u = up.getChannelPointer (0);
            for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = shape (u[i], g, type);
            oversampler->processSamplesDown (block);

            toneL.set (sr, bank[tone], 0.707f);
            const float gain = juce::Decibels::decibelsToGain (bank[level]);
            auto* l = out.getWritePointer (0, start + pos);
            auto* r = out.getNumChannels() > 1 ? out.getWritePointer (1, start + pos) : nullptr;
            int sw = scopeWrite.load();
            for (int i = 0; i < n; ++i)
            {
                // DC blocker (asymmetric tape drive makes some), then the tone filter
                float x = m[i];
                float y = x - dcX + 0.9995f * dcY; dcX = x; dcY = y;
                float s = toneL.lowpass (y) * gain;
                l[i] = s; if (r) r[i] = s;
                scope[(size_t) (sw & 511)] = s; ++sw;
            }
            scopeWrite = sw;
        }
        while (ev < midiEvents.size()) handleMidi (midiEvents[ev++].msg);
    }

private:
    struct Voice
    {
        bool active = false, releasing = false;
        int note = 60;
        double curNote = 60, targetNote = 60, glideStep = 0;
        double phase = 0, subPhase = 0, t = 0;
        float vel = 1, env = 0, clickT = 1;
        juce::Random rnd;
    };
    struct Ev { int sample; juce::MidiMessage msg; };

    std::array<Voice, 8> voices;
    juce::Array<int> held;
    juce::Array<Ev> midiEvents;
    double sr = 44100.0;
    int maxBlock = 512;
    static constexpr int chunk = 256;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> mono;
    SqDsp::SVF toneL;
    float dcX = 0, dcY = 0, bend = 0;

    static float shape (float x, float g, int type)
    {
        switch (type)
        {
            case 1:  { const float b = 0.18f; return (std::tanh (g * x + b) - std::tanh (b)) / std::tanh (g + b); }        // tape: asymmetric, even harmonics
            case 2:  return juce::jlimit (-1.0f, 1.0f, g * x) * 0.9f;                                                       // hard clip
            case 3:  return std::sin (juce::jlimit (-8.0f, 8.0f, g * x) * 1.5707963f) * 0.9f;                             // wavefold
            default: return std::tanh (g * x) / std::tanh (g);                                                            // tube
        }
    }

    Voice* freeVoice()
    {
        for (auto& v : voices) if (! v.active) return &v;
        Voice* best = &voices[0];
        for (auto& v : voices) if (v.releasing && v.env < best->env) best = &v;
        return best;
    }

    void startVoice (Voice& v, int note, float velocity, bool retrigger, bool glideFromCurrent)
    {
        const double glideSec = bank[glide];
        v.targetNote = note;
        if (! glideFromCurrent || glideSec <= 0.0005) v.curNote = note;
        v.glideStep = glideSec > 0.0005 ? std::abs (v.targetNote - v.curNote) / (glideSec * sr) : 1e9;
        v.note = note;
        if (retrigger)
        {
            v.t = 0; v.env = 1.0f; v.clickT = 0;
            v.vel = 1.0f - bank[velSens] * (1.0f - velocity);
            if (! v.active) { v.phase = 0; v.subPhase = 0; }
        }
        v.active = true; v.releasing = false;
    }

    void handleMidi (const juce::MidiMessage& m)
    {
        const bool poly = polyMode.get() == 1;
        const int mode = glideMode.get();
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            const float vel = m.getFloatVelocity();
            if (poly)
            {
                startVoice (*freeVoice(), note, vel, true, false);
                return;
            }
            auto& v = voices[0];
            const bool legato = ! held.isEmpty() && v.active && ! v.releasing;
            held.removeFirstMatchingValue (note);
            held.add (note);
            if (mode == 0 && legato)       startVoice (v, note, vel, false, true);    // FL slide: overlapping notes glide, no retrigger
            else if (mode == 1)            startVoice (v, note, vel, true, v.active); // always glide from the last pitch
            else                           startVoice (v, note, vel, true, false);
        }
        else if (m.isNoteOff())
        {
            const int note = m.getNoteNumber();
            if (poly)
            {
                for (auto& v : voices) if (v.active && v.note == note && ! v.releasing) v.releasing = true;
                return;
            }
            held.removeFirstMatchingValue (note);
            auto& v = voices[0];
            if (! held.isEmpty() && v.active)
            {
                if (v.note == note) startVoice (v, held.getLast(), v.vel, false, mode != 2);   // fall back to the held note
            }
            else if (v.note == note) v.releasing = true;
        }
        else if (m.isPitchWheel())
            bend = (float) ((m.getPitchWheelValue() - 8192) / 8192.0) * (float) bendRange.get();
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clearQuick();
            for (auto& v : voices) v.releasing = true;
        }
    }

    float renderSample()
    {
        const float pTune = bank[tune] + bank[fine] / 100.0f + bend;
        const float pPunch = bank[punch], pPunchT = juce::jmax (0.001f, bank[punchTime]);
        const float decayK = (float) std::exp (-6.9077553 / (juce::jmax (0.02f, bank[decay]) * sr));     // -60 dB over 'decay'
        const float relK = (float) std::exp (-6.9077553 / (juce::jmax (0.005f, bank[release]) * sr));
        const float pClick = bank[click], pSub = bank[sub], pHarm = bank[harmonics];
        float sum = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (v.curNote != v.targetNote)
            {
                const double d = v.targetNote - v.curNote;
                v.curNote = std::abs (d) <= v.glideStep ? v.targetNote : v.curNote + (d > 0 ? v.glideStep : -v.glideStep);
            }
            const double pitch = v.curNote + pTune + pPunch * std::exp (-v.t / pPunchT);
            const double f = SqDsp::noteHz (pitch);
            v.phase += f / sr; if (v.phase >= 1.0) v.phase -= 1.0;
            v.subPhase += 0.5 * f / sr; if (v.subPhase >= 1.0) v.subPhase -= 1.0;
            const double tp = juce::MathConstants<double>::twoPi;
            float s = (float) std::sin (tp * v.phase);
            if (pHarm > 0) s += pHarm * (0.35f * (float) std::sin (2.0 * tp * v.phase) + 0.18f * (float) std::sin (3.0 * tp * v.phase));
            if (pSub > 0)  s += pSub * 0.6f * (float) std::sin (tp * v.subPhase);
            float c = 0.0f;
            if (v.clickT < 0.012f && pClick > 0)
            {
                const float ce = 1.0f - v.clickT / 0.012f;
                c = pClick * ce * ce * (0.6f * (v.rnd.nextFloat() * 2.0f - 1.0f) + 0.8f * (float) std::sin (tp * 1600.0 * v.clickT));
                v.clickT += (float) (1.0 / sr);
            }
            v.env *= v.releasing ? relK : decayK;
            sum += (s * v.env + c) * v.vel * 0.9f;
            v.t += 1.0 / sr;
            if (v.env < 1.0e-4f) { v.active = false; v.releasing = false; }
        }
        if (voices[0].active) { displayNote = (float) voices[0].curNote; displayEnv = voices[0].env; }
        else displayEnv = displayEnv.load() * 0.999f;
        return sum;
    }
};

//==============================================================================
//==============================================================================
class SqBlossomPlugin : public te::Plugin
{
public:
    enum P { tune, position, hardness, noise, bowPressure, bowSpeed, stiffness, damping, decay, s2Ratio, s2Detune, s2Mix, coupling,
             pickup, bodySize, bodyMix, sympathetic, attack, release, vibRate, vibDepth, spread, chorus, delayMix, delayTime, delayFb,
             reverbMix, reverbSize, velSens, level, numParams };

    SqBlossomPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        bank.create (*this, {
            { "tune",        "Tune",          -24.0f, 24.0f, 0.0f, 1.0f, " st", 1 },
            { "position",    "Position",      0.03f, 0.5f, 0.14f, 1.0f, "", 2 },
            { "hardness",    "Hardness",      0.0f, 1.0f, 0.55f, 1.0f, "", 2 },
            { "noise",       "Noise",         0.0f, 1.0f, 0.15f, 1.0f, "", 2 },
            { "bowPressure", "Bow Pressure",  0.0f, 1.0f, 0.5f, 1.0f, "", 2 },
            { "bowSpeed",    "Bow Speed",     0.0f, 1.0f, 0.55f, 1.0f, "", 2 },
            { "stiffness",   "Stiffness",     0.0f, 1.0f, 0.1f, 1.0f, "", 2 },
            { "damping",     "Damping",       0.0f, 1.0f, 0.35f, 1.0f, "", 2 },
            { "decay",       "Decay",         0.1f, 30.0f, 4.0f, 0.35f, " s", 2 },
            { "s2Ratio",     "String 2 Ratio", 0.5f, 4.0f, 1.0f, 0.5f, "x", 3 },
            { "s2Detune",    "String 2 Detune", -50.0f, 50.0f, 4.0f, 1.0f, " ct", 1 },
            { "s2Mix",       "String 2 Mix",  0.0f, 1.0f, 0.4f, 1.0f, "", 2 },
            { "coupling",    "Coupling",      0.0f, 1.0f, 0.0f, 1.0f, "", 2 },
            { "pickup",      "Pickup",        0.0f, 1.0f, 0.3f, 1.0f, "", 2 },
            { "bodySize",    "Body Size",     0.5f, 2.0f, 1.0f, 0.6f, "x", 2 },
            { "bodyMix",     "Body",          0.0f, 1.0f, 0.45f, 1.0f, "", 2 },
            { "sympathetic", "Sympathetic",   0.0f, 1.0f, 0.15f, 1.0f, "", 2 },
            { "attack",      "Attack",        0.0f, 3.0f, 0.0f, 0.4f, " s", 3 },
            { "release",     "Release",       0.01f, 10.0f, 0.6f, 0.35f, " s", 2 },
            { "vibRate",     "Vibrato Rate",  0.1f, 10.0f, 5.0f, 0.6f, " Hz", 2 },
            { "vibDepth",    "Vibrato",       0.0f, 1.0f, 0.0f, 1.0f, " st", 2 },
            { "spread",      "Stereo Spread", 0.0f, 1.0f, 0.5f, 1.0f, "", 2 },
            { "chorus",      "Chorus",        0.0f, 1.0f, 0.0f, 1.0f, "", 2 },
            { "delayMix",    "Delay",         0.0f, 1.0f, 0.0f, 1.0f, "", 2 },
            { "delayTime",   "Delay Time",    0.02f, 1.5f, 0.375f, 0.5f, " s", 3 },
            { "delayFb",     "Delay Feedback", 0.0f, 0.9f, 0.35f, 1.0f, "", 2 },
            { "reverbMix",   "Reverb",        0.0f, 1.0f, 0.18f, 1.0f, "", 2 },
            { "reverbSize",  "Reverb Size",   0.0f, 1.0f, 0.55f, 1.0f, "", 2 },
            { "velSens",     "Velocity",      0.0f, 1.0f, 0.7f, 1.0f, "", 2 },
            { "level",       "Level",         -24.0f, 6.0f, 0.0f, 1.0f, " dB", 1 } });
        auto um = getUndoManager();
        exciterType.referTo (state, "exciterType", um, 0);
        bodyType.referTo (state, "bodyType", um, 0);
    }
    ~SqBlossomPlugin() override { notifyListenersOfDeletion(); bank.detach(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Blossom 2"); }
    static constexpr const char* xmlTypeName = "sqmBlossom";
    juce::String getName() const override               { return "Blossom 2"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Blsm"; }
    juce::String getSelectableDescription() override    { return "Blossom physical-model strings"; }
    bool isSynth() override                             { return true; }
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool needsConstantBufferSize() override             { return false; }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override               { return 12.0; }

    static juce::StringArray exciterNames()   { return { "PLUCK", "STRIKE", "BOW" }; }
    static juce::StringArray bodyNames()      { return { "GUITAR", "VIOLIN", "CELLO", "KOTO", "KALIMBA", "PIANO", "GLASS", "NONE" }; }

    juce::CachedValue<int> exciterType, bodyType;
    SqDsp::ParamBank bank;
    static constexpr int maxVoices = 32;

    struct BodyMode { float f, q, g; };
    /** Modal frequencies (Hz), Qs and levels for each body model (typical measured resonances of each family). */
    static const std::array<std::array<BodyMode, 8>, 8>& bodyTable()
    {
        static const std::array<std::array<BodyMode, 8>, 8> bodies {{
            {{ { 98, 12, 1.0f }, { 204, 18, 0.9f }, { 290, 20, 0.7f }, { 410, 22, 0.55f }, { 550, 25, 0.45f }, { 1050, 30, 0.3f }, { 2100, 30, 0.22f }, { 3200, 28, 0.15f } }},  // guitar
            {{ { 275, 20, 1.0f }, { 460, 22, 0.85f }, { 550, 25, 0.8f }, { 700, 25, 0.6f }, { 1000, 30, 0.5f }, { 2000, 30, 0.45f }, { 2800, 30, 0.35f }, { 4100, 28, 0.2f } }}, // violin
            {{ { 110, 15, 1.0f }, { 185, 18, 0.9f }, { 230, 20, 0.7f }, { 330, 22, 0.6f }, { 520, 25, 0.45f }, { 900, 28, 0.3f }, { 1700, 28, 0.2f }, { 2600, 25, 0.12f } }},  // cello
            {{ { 180, 25, 1.0f }, { 340, 30, 0.8f }, { 620, 35, 0.6f }, { 980, 35, 0.5f }, { 1500, 40, 0.4f }, { 2300, 40, 0.3f }, { 3400, 40, 0.2f }, { 5200, 35, 0.12f } }},  // koto
            {{ { 230, 40, 1.0f }, { 470, 45, 0.6f }, { 820, 50, 0.4f }, { 1300, 50, 0.3f }, { 2100, 55, 0.25f }, { 3100, 55, 0.18f }, { 4500, 50, 0.12f }, { 6200, 45, 0.08f } }}, // kalimba box
            {{ { 85, 10, 1.0f }, { 160, 12, 0.9f }, { 300, 14, 0.75f }, { 520, 16, 0.6f }, { 900, 18, 0.45f }, { 1600, 20, 0.35f }, { 2800, 22, 0.25f }, { 4400, 22, 0.15f } }},  // piano soundboard
            {{ { 880, 120, 1.0f }, { 1760, 150, 0.7f }, { 2400, 160, 0.6f }, { 3520, 180, 0.5f }, { 4700, 200, 0.4f }, { 6100, 200, 0.3f }, { 7900, 220, 0.2f }, { 9800, 220, 0.15f } }}, // glass
            {{ { 100, 1, 0 }, { 100, 1, 0 }, { 100, 1, 0 }, { 100, 1, 0 }, { 100, 1, 0 }, { 100, 1, 0 }, { 100, 1, 0 }, { 100, 1, 0 } }} }};                                          // none
        return bodies;
    }



    // editor display
    std::atomic<float> displayEnergy { 0.0f };
    std::atomic<int> activeVoices { 0 };

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        const int maxDelay = (int) (sr / 20.0) + 8;     // lowest note ~20 Hz
        for (auto& v : voices) { v.s1.prepare (maxDelay); v.s2.prepare (maxDelay); v.active = false; }
        for (auto& s : symp) s.d.prepare ((int) (sr / 30.0) + 8);
        chorusL.prepare ((int) (sr * 0.05)); chorusR.prepare ((int) (sr * 0.05));
        delayL.prepare ((int) (sr * 1.6)); delayR.prepare ((int) (sr * 1.6));
        reverb.setSampleRate (sr);
        reverb.reset();
        for (auto& m : modes) { m.y1 = m.y2 = 0; }
        dcL = dcR = dcXL = dcXR = 0;
        tailSamples = 0;
        lastBody = -1; lastBodySize = -1.0f;
        tmpL.setSize (1, 4096); tmpR.setSize (1, 4096);
    }
    void deinitialise() override {}

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        for (auto ap : getAutomatableParameters()) ap->updateFromAttachedValue();
    }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& out = *fc.destBuffer;
        const int start = fc.bufferStartSample, num = fc.bufferNumSamples;
        for (int c = 0; c < out.getNumChannels(); ++c) out.clear (c, start, num);

        midiEvents.clearQuick();
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) for (auto& v : voices) v.gate = false;
            for (auto& m : *fc.bufferForMidiMessages)
                midiEvents.add ({ juce::jlimit (0, num - 1, juce::roundToInt (m.getTimeStamp() * sr)), juce::MidiMessage (m) });
        }

        int active = 0;
        for (auto& v : voices) active += v.active ? 1 : 0;
        const bool fxTail = tailSamples > 0;
        if (active == 0 && midiEvents.isEmpty() && ! fxTail) { displayEnergy = 0.0f; activeVoices = 0; return; }

        updateBody();
        auto* l = out.getWritePointer (0, start);
        auto* r = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : l;
        int ev = 0;
        float energy = 0.0f;

        // per-block constants
        const float pTune = bank[tune] + bend;
        const float stiff = bank[stiffness], damp = bank[damping], t60 = bank[decay];
        const float pos = bank[position], hard = bank[hardness], nz = bank[noise];
        const float ratio2 = bank[s2Ratio] * std::pow (2.0f, bank[s2Detune] / 1200.0f), mix2 = bank[s2Mix], coup = bank[coupling] * 0.08f;
        const float pick = bank[pickup], spreadAmt = bank[spread];
        const float attackS = bank[attack], releaseS = bank[release];
        const float vibR = bank[vibRate], vibD = bank[vibDepth] + modWheel;
        const int exc = juce::jlimit (0, 2, exciterType.get());
        const float bowP = bank[bowPressure], bowS = bank[bowSpeed];
        // loop filter: damping sets the one-pole lowpass, stiffness sets the dispersion allpasses
        // (the floor keeps the loop's high-frequency gain safely below 1: cubic interpolation can overshoot slightly)
        const float lpPole = juce::jlimit (0.15f, 0.95f, 0.15f + damp * 0.8f);
        const float apC = -juce::jlimit (0.0f, 0.6f, stiff * 0.6f);
        const float attK = attackS <= 0.0005f ? 1.0f : (float) (1.0 / (attackS * sr));
        const float relK = (float) std::exp (-6.9 / (juce::jmax (0.01f, releaseS) * sr));

        for (int i = 0; i < num; ++i)
        {
            while (ev < midiEvents.size() && midiEvents[ev].sample <= i) handleMidi (midiEvents[ev++].msg, exc);
            vibPhase += vibR / sr; if (vibPhase >= 1.0) vibPhase -= 1.0;
            const float vib = vibD * (float) std::sin (juce::MathConstants<double>::twoPi * vibPhase);
            float sumL = 0.0f, sumR = 0.0f, sumMono = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                // amplitude envelope (bow and sustain shaping; plucks ring on their own)
                if (v.gate) v.env = juce::jmin (1.0f, v.env + attK);
                else        v.env *= relK;
                const double note = v.note + pTune + vib;
                float s = renderVoice (v, note, ratio2, mix2, coup, pos, hard, nz, lpPole, apC, t60, pick, exc, bowP, bowS);
                if (! std::isfinite (s) || std::abs (s) > 8.0f)
                {
                    // watchdog: a voice that ever runs away is silenced and cleared, never passed on
                    v.s1.clear(); v.s2.clear(); v.active = false; s = 0.0f;
                    continue;
                }
                const float o = s * ((exc == 2 || attackS > 0.0005f) ? v.env : (v.gate ? 1.0f : v.env));
                const float pan = 0.5f + (float) (v.note - 60) / 48.0f * spreadAmt;
                sumL += o * std::cos (juce::jlimit (0.0f, 1.0f, pan) * 1.5707963f) * 1.4142f;
                sumR += o * std::sin (juce::jlimit (0.0f, 1.0f, pan) * 1.5707963f) * 1.4142f;
                sumMono += o;
                // voice ends when it has died away
                v.peak = juce::jmax (std::abs (o), v.peak * 0.9995f);
                if (! v.gate && v.peak < 1.0e-5f && v.age > (int) (sr * 0.05)) v.active = false;
                ++v.age;
            }
            // body resonator and sympathetic strings on the sum
            float bodyOut = 0.0f;
            for (auto& m : modes) bodyOut += m.process (sumMono) * m.gain;
            const float bm = bank[bodyMix];
            float symOut = 0.0f;
            const float sy = bank[sympathetic];
            if (sy > 0.001f)
                for (auto& sString : symp)
                {
                    const float y = sString.d.read (sString.len);
                    sString.lp = sString.lp * 0.5f + y * 0.5f;
                    sString.d.push (sumMono * 0.02f * sy + sString.lp * 0.996f);
                    symOut += y;
                }
            const float add = bodyOut * bm * 0.6f + symOut * sy * 0.5f;
            const float xl = sumL * (1.0f - bm * 0.4f) + add, xr = sumR * (1.0f - bm * 0.4f) + add;
            // DC blocker (struck strings leave a slowly decaying offset in the loop)
            dcL = xl - dcXL + 0.9995f * dcL; dcXL = xl;
            dcR = xr - dcXR + 0.9995f * dcR; dcXR = xr;
            l[i] = dcL;
            r[i] = dcR;
            energy = juce::jmax (energy, std::abs (l[i]));
        }
        while (ev < midiEvents.size()) handleMidi (midiEvents[ev++].msg, exc);

        applyEffects (l, r, num);
        int act = 0;
        for (auto& v : voices) act += v.active ? 1 : 0;
        activeVoices = act;
        displayEnergy = juce::jmax (energy, displayEnergy.load() * 0.9f);
        tailSamples = act > 0 ? (int) (sr * 6.0) : juce::jmax (0, tailSamples - num);
    }

private:
    struct Waveguide
    {
        SqDsp::Delay nut, bridge;      // nut side and bridge side of the exciter point
        float lp = 0, ap[4] {}, apX[4] {};
        void prepare (int n) { nut.prepare (n); bridge.prepare (n); }
        void clear() { nut.clear(); bridge.clear(); lp = 0; for (auto& a : ap) a = 0; for (auto& a : apX) a = 0; }
    };
    struct Voice
    {
        bool active = false, gate = false;
        int note = 60, age = 0;
        float vel = 1, env = 0, peak = 0, exciteT = 0, exciteLen = 0;
        Waveguide s1, s2;
        juce::Random rnd;
        float excLp = 0;
        int ctrl = 0;                                   // control-rate countdown
        float n1 = 10, b1 = 10, n2 = 10, b2 = 10, loss1 = 0.99f, loss2 = 0.99f, apC = 0, coupC = 0;
        bool mute2 = false;                             // string 2 too high to model at this sample rate
    };
    struct Mode
    {
        float a1 = 0, a2 = 0, y1 = 0, y2 = 0, g = 0, gain = 1;
        void set (double sr, double f, double q, float level)
        {
            f = juce::jlimit (20.0, sr * 0.45, f);
            const double r = std::exp (-juce::MathConstants<double>::pi * f / (q * sr));
            a1 = (float) (-2.0 * r * std::cos (juce::MathConstants<double>::twoPi * f / sr));
            a2 = (float) (r * r);
            g = (float) (1.0 - r);
            gain = level;
        }
        float process (float x) { const float y = g * x - a1 * y1 - a2 * y2; y2 = y1; y1 = y; return y; }
    };
    struct Symp { SqDsp::Delay d; float len = 100, lp = 0; };
    struct Ev { int sample; juce::MidiMessage msg; };

    std::array<Voice, maxVoices> voices;
    std::array<Mode, 8> modes;
    std::array<Symp, 6> symp;
    juce::Array<Ev> midiEvents;
    SqDsp::Delay chorusL, chorusR, delayL, delayR;
    juce::Reverb reverb;
    juce::AudioBuffer<float> tmpL, tmpR;
    double sr = 44100.0, vibPhase = 0, chorusPhase = 0;
    int lastBody = -1, tailSamples = 0;
    float lastBodySize = -1.0f, bend = 0, modWheel = 0, dcL = 0, dcR = 0, dcXL = 0, dcXR = 0;
    bool sustain = false;
    juce::Array<int> sustained;

    void updateBody()
    {
        const int b = juce::jlimit (0, 7, bodyType.get());
        const float size = bank[bodySize];
        if (b == lastBody && std::abs (size - lastBodySize) < 1.0e-3f) return;
        lastBody = b; lastBodySize = size;
        const auto& bodies = bodyTable();
        for (int k = 0; k < 8; ++k)
            modes[(size_t) k].set (sr, bodies[b][k].f / size, bodies[b][k].q, bodies[b][k].g);
        // sympathetic strings: open strings of the instrument family (guitar standard tuning by default)
        static const float tunings[8][6] = { { 40, 45, 50, 55, 59, 64 }, { 55, 62, 69, 76, 62, 69 }, { 36, 43, 50, 57, 43, 50 }, { 50, 55, 57, 62, 64, 69 },
                                             { 60, 64, 67, 72, 76, 79 }, { 36, 43, 48, 55, 60, 67 }, { 72, 76, 79, 84, 88, 91 }, { 40, 45, 50, 55, 59, 64 } };
        for (int k = 0; k < 6; ++k) symp[(size_t) k].len = (float) (sr / SqDsp::noteHz (tunings[b][k]));
    }

    Voice& allocate (int note)
    {
        for (auto& v : voices) if (v.active && v.note == note && ! v.gate) return v;   // re-strike the same string
        for (auto& v : voices) if (! v.active) return v;
        Voice* best = nullptr;
        for (auto& v : voices) if (! v.gate && (best == nullptr || v.peak < best->peak)) best = &v;
        if (best == nullptr) { best = &voices[0]; for (auto& v : voices) if (v.age > best->age) best = &v; }
        return *best;
    }

    void handleMidi (const juce::MidiMessage& m, int exc)
    {
        if (m.isNoteOn())
        {
            auto& v = allocate (m.getNoteNumber());
            const bool restrike = v.active && v.note == m.getNoteNumber();
            if (! restrike) { v.s1.clear(); v.s2.clear(); v.peak = 0; }
            v.ctrl = 0;
            v.active = true; v.gate = true; v.note = m.getNoteNumber(); v.age = 0;
            v.vel = 1.0f - bank[velSens] * (1.0f - m.getFloatVelocity());
            v.env = (exc == 2 || bank[attack] > 0.0005f) ? (restrike ? v.env : 0.0f) : 1.0f;
            v.exciteT = 0;
            const double period = sr / SqDsp::noteHz (v.note);
            // hammer contact time shortens with hardness and is never longer than half a period (no pumping on high notes)
            v.exciteLen = exc == 1 ? (float) juce::jlimit (3.0, juce::jmax (3.0, period * 0.5), (1.0 - bank[hardness]) * sr * 0.004 + 6.0) : (float) juce::jmax (4.0, period * 0.5);
            sustained.removeFirstMatchingValue (v.note);
        }
        else if (m.isNoteOff())
        {
            if (sustain) { sustained.addIfNotAlreadyThere (m.getNoteNumber()); return; }
            for (auto& v : voices) if (v.active && v.gate && v.note == m.getNoteNumber()) v.gate = false;
        }
        else if (m.isSustainPedalOn())  sustain = true;
        else if (m.isSustainPedalOff())
        {
            sustain = false;
            for (auto n : sustained) for (auto& v : voices) if (v.active && v.gate && v.note == n) v.gate = false;
            sustained.clearQuick();
        }
        else if (m.isPitchWheel())  bend = (float) ((m.getPitchWheelValue() - 8192) / 8192.0) * 2.0f;
        else if (m.isControllerOfType (1)) modWheel = m.getControllerValue() / 127.0f * 0.5f;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) v.gate = false;
    }

    /** First half of a string step: the waves arriving at the exciter point and the reflections at both ends
        (the bridge end carries the loss, the damping lowpass and the dispersion allpasses that make it stiff). */
    static void reflect (Waveguide& w, float nutLen, float bridgeLen, float lpPole, float apC, float loss,
                         float& fromBridge, float& bridgeRefl, float& nutRefl)
    {
        fromBridge = w.bridge.read (bridgeLen);
        const float fromNut = w.nut.read (nutLen);
        float b = fromBridge;
        w.lp = (1.0f - lpPole) * b + lpPole * w.lp;
        b = w.lp;
        for (int k = 0; k < 4; ++k)
        {
            const float y = apC * b + w.apX[k] - apC * w.ap[k];
            w.apX[k] = b; w.ap[k] = y; b = y;
        }
        bridgeRefl = -b * loss;
        nutRefl = -fromNut;
    }

    /** Second half: the exciter (an injected pulse, or the bow's slip-stick friction) and the writes. */
    static void inject (Waveguide& w, float bridgeRefl, float nutRefl, float excitation, bool bowing, float bowVel, float bowSlope, float& junction)
    {
        float in = excitation;
        if (bowing)
        {
            const float stringVel = bridgeRefl + nutRefl;
            const float dv = bowVel - stringVel;
            float f = std::abs (dv * bowSlope) + 0.75f;
            f = 1.0f / (f * f * f * f);
            in += dv * juce::jmin (1.0f, f);
        }
        w.nut.push (bridgeRefl + in);
        w.bridge.push (nutRefl + in);
        junction = bridgeRefl + nutRefl;
    }

    float renderVoice (Voice& v, double note, float ratio2, float mix2, float coup, float pos, float hard, float nz,
                       float lpPole, float apC, float t60, float pick, int exc, float bowP, float bowS)
    {
        // frequency-dependent loop settings, refreshed at control rate (every 16 samples)
        if (--v.ctrl <= 0)
        {
            v.ctrl = 16;
            const double f1 = SqDsp::noteHz (note);
            const double f2 = f1 * ratio2;
            const double period = sr / f1;
            // stiffness (dispersion) is limited on high notes so the loop always fits inside one period
            v.apC = apC * (float) juce::jlimit (0.0, 1.0, period / 60.0);
            const float filterDelay = lpPole / (1.0f - lpPole) + 4.0f * (1.0f - v.apC) / (1.0f + v.apC);
            auto split = [&] (double f, float& nutLen, float& bridgeLen)
            {
                // the two sections make one round trip each way: nut + bridge = one period (minus the loop filter's delay)
                const float total = juce::jmax (4.0f, (float) (sr / f) - filterDelay);
                bridgeLen = juce::jmax (2.0f, total * pos);
                nutLen = juce::jmax (2.0f, total - bridgeLen);
            };
            split (f1, v.n1, v.b1);
            split (f2, v.n2, v.b2);
            v.mute2 = (float) (sr / f2) - filterDelay < 8.0f;
            v.loss1 = (float) std::pow (10.0, -3.0 / (juce::jmax (0.05f, t60) * f1));
            // coupling per round trip, scaled so the energy exchange per second is the same at every pitch
            v.coupC = juce::jlimit (0.0f, 0.2f, coup * 0.02f * (float) (220.0 / f1));
            v.loss2 = (float) std::pow (10.0, -3.0 / (juce::jmax (0.05f, t60) * f2));
        }

        // excitation
        float e = 0.0f;
        const bool bowing = exc == 2 && v.gate;
        if (exc != 2 && v.exciteT < v.exciteLen)
        {
            const float ph = v.exciteT / v.exciteLen;
            if (exc == 1)   // strike: a raised-cosine hammer pulse
                e = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * ph)) * 0.9f;
            else            // pluck: a short, filtered noise-and-pulse burst - harder = brighter
            {
                const float raw = (1.0f - nz) * (ph < 0.5f ? 1.0f : -1.0f) + nz * (v.rnd.nextFloat() * 2.0f - 1.0f);
                const float a = 0.15f + hard * 0.8f;
                v.excLp += a * (raw - v.excLp);
                e = v.excLp * 0.7f;
            }
            e *= v.vel;
            v.exciteT += 1.0f;
        }
        const float bowVel = bowing ? (0.02f + bowS * 0.12f) * v.vel * v.env + nz * 0.004f * (v.rnd.nextFloat() - 0.5f) : 0.0f;
        const float bowSlope = 5.0f - 4.0f * bowP;

        // both strings reflect, then the bridge couples them through a passive (energy-preserving) mixing matrix:
        // eigenvalues 1 and 1 - 2c, so coupling can never make the strings ring louder - only beat and share sustain
        const float bowLoss = bowing ? 0.985f : 1.0f;
        float o1, br1, nr1, o2, br2, nr2;
        reflect (v.s1, v.n1, v.b1, lpPole, v.apC, v.loss1 * bowLoss, o1, br1, nr1);
        reflect (v.s2, v.n2, v.b2, lpPole, v.apC, v.loss2 * bowLoss, o2, br2, nr2);
        if (v.coupC > 0.0f)
        {
            const float c = v.coupC;
            const float m1 = br1 * (1.0f - c) + br2 * c, m2 = br2 * (1.0f - c) + br1 * c;
            br1 = m1; br2 = m2;
        }
        float j1 = 0, j2 = 0;
        if (v.mute2) mix2 = 0.0f;
        inject (v.s1, br1, nr1, e, bowing, bowVel, bowSlope, j1);
        inject (v.s2, br2, nr2, e * mix2, bowing && mix2 > 0.05f, bowVel * mix2, bowSlope, j2);
        // pickups: bridge pickup (bright, thin) crossfaded with a pickup at the exciter point (round, full)
        const float out1 = o1 * (1.0f - pick) + j1 * pick * 0.7f;
        const float out2 = o2 * (1.0f - pick) + j2 * pick * 0.7f;
        return (out1 + out2 * mix2) * 0.9f;
    }

    void applyEffects (float* l, float* r, int num)
    {
        const float ch = bank[chorus], dm = bank[delayMix], rm = bank[reverbMix];
        if (ch > 0.001f)
        {
            for (int i = 0; i < num; ++i)
            {
                chorusPhase += 0.6 / sr; if (chorusPhase >= 1.0) chorusPhase -= 1.0;
                const float m1 = (float) std::sin (juce::MathConstants<double>::twoPi * chorusPhase);
                const float m2 = (float) std::sin (juce::MathConstants<double>::twoPi * (chorusPhase + 0.25));
                chorusL.push (l[i]); chorusR.push (r[i]);
                const float dl = (float) (sr * (0.012 + 0.004 * m1)), dr = (float) (sr * (0.013 + 0.004 * m2));
                const float wl = chorusL.read (dl), wr = chorusR.read (dr);
                l[i] = l[i] * (1.0f - ch * 0.5f) + wl * ch * 0.7f;
                r[i] = r[i] * (1.0f - ch * 0.5f) + wr * ch * 0.7f;
            }
        }
        if (dm > 0.001f)
        {
            const float dt = (float) (bank[delayTime] * sr), fb = bank[delayFb];
            for (int i = 0; i < num; ++i)
            {
                const float wl = delayL.read (dt), wr = delayR.read (dt * 1.02f);
                delayL.push (l[i] + wr * fb);    // ping-pong
                delayR.push (r[i] + wl * fb);
                l[i] += wl * dm; r[i] += wr * dm;
            }
        }
        if (rm > 0.001f)
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.3f + bank[reverbSize] * 0.68f; p.damping = 0.45f; p.width = 1.0f;
            p.wetLevel = rm * 0.5f; p.dryLevel = 1.0f - rm * 0.35f; p.freezeMode = 0.0f;
            reverb.setParameters (p);
            reverb.processStereo (l, r, num);
        }
        const float gain = juce::Decibels::decibelsToGain (bank[level]);
        for (int i = 0; i < num; ++i)
        {
            l[i] = std::isfinite (l[i]) ? std::tanh (l[i] * gain * 0.7f) : 0.0f;   // make-up gain into a gentle safety saturation instead of hard clipping
            r[i] = std::isfinite (r[i]) ? std::tanh (r[i] * gain * 0.7f) : 0.0f;
        }
    }
};
