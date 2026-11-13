#pragma once
// Mote (1.14 "Studio Line", phase K): the one-oscillator synth. DSP: SqMoteDsp.h (tests/mote_test.cpp), on the shared
// synth scaffolding (SqSynthCore.h: MIDI, voices, poly / mono / legato, glide, unison, HD, the 8-slot mod matrix).
// Face: SkinsMote.inl. Claude: `mote` (McpToolsMote.h).

#include "SqSynthCore.h"
#include "SqMoteDsp.h"

namespace SqSynth
{
    struct MoteVoice : public VoiceBase
    {
        SqMote::Voice dsp; const SqMote::Settings* set = nullptr; SqMote::Mods m; double unisonCents = 8, spread = 0.6, tune = 0;
        void prepare (double rate) { VoiceBase::prepare (rate); dsp.prepare (rate); }
        void start (int n, float vel, float glideSecs, float fromHz) { const bool fresh = ! active; startBase (n, vel, glideSecs, fromHz); dsp.start (fresh); }
        void restartEnvelopes() override { VoiceBase::restartEnvelopes(); dsp.amp.noteOn(); dsp.flt.noteOn(); dsp.sinceOn = 0; }
        void release() override { VoiceBase::release(); dsp.release(); }
        void kill() override { VoiceBase::kill(); dsp.kill(); }
        inline void render (float* l, float* r, int n)
        {
            if (set == nullptr) return;
            const double hz = chunkFrequency (bend + (float) tune, (float) (unisonDetune * unisonCents));
            const float panPos = juce::jlimit (-1.0f, 1.0f, (float) (unisonPan * spread));
            const float a = (panPos + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            dsp.render (l, r, n, hz, *set, m, mods.get ((int) ModSource::lfo1), velocity, note, std::cos (a) * 1.41421356f * unisonGain, std::sin (a) * 1.41421356f * unisonGain);
            env[0].level = dsp.lastAmp; env[1].level = dsp.lastFlt;   // the matrix sources Env 1 / Env 2 follow Mote's envelopes
            if (! dsp.active()) active = false;
        }
    };

    class SqMotePlugin : public SynthPluginBase<MoteVoice, 16>
    {
    public:
        using Base = SynthPluginBase<MoteVoice, 16>;
        enum P { shape, width, octave, semi, fine, oscLevel, subLevel, noise, noiseColour, drive,
                 cutoff, res, fEnvDepth, keyTrack, velDepth,
                 fA, fD, fS, fR, aA, aD, aS, aR, ampVel,
                 lfoRate, lfoPitch, lfoCut, lfoShapeAmt, lfoWidth, lfoAmp, lfoDelay,
                 glide, uniDetune, uniSpread, level, numParams };
        static constexpr const char* xmlTypeName = "sqmMote";
        static const char* getPluginName()   { return NEEDS_TRANS("Mote"); }
        juce::String getName() const override { return "Mote"; }
        juce::String getPluginType() override { return xmlTypeName; }
        juce::String getShortName (int) override { return "Mote"; }
        juce::String getSelectableDescription() override { return "Mote one-oscillator synth"; }

        static const std::vector<SqDsp::ParamSpec>& paramSpecs()
        {
            static const std::vector<SqDsp::ParamSpec> v {
                { "shape", "Osc Shape", 0.0f, 3.0f, 2.0f, 1.0f, "", 2 }, { "width", "Osc Width", 5.0f, 50.0f, 50.0f, 1.0f, " %", 0 },
                { "octave", "Osc Octave", -3.0f, 3.0f, 0.0f, 1.0f, "", 0 }, { "semi", "Osc Semitone", -12.0f, 12.0f, 0.0f, 1.0f, " st", 0 }, { "fine", "Osc Fine", -100.0f, 100.0f, 0.0f, 1.0f, " ct", 0 },
                { "oscLevel", "Osc Level", 0.0f, 100.0f, 100.0f, 1.0f, " %", 0 }, { "subLevel", "Sub Level", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
                { "noise", "Noise Level", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 }, { "noiseColour", "Noise Colour", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
                { "drive", "Drive", 0.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "cutoff", "Filter Cutoff", 20.0f, 20000.0f, 1800.0f, 0.25f, " Hz", 0 }, { "res", "Filter Resonance", 0.0f, 100.0f, 20.0f, 1.0f, " %", 0 },
                { "fEnvDepth", "Filter Env Depth", -6.0f, 6.0f, 2.0f, 1.0f, " oct", 2 }, { "keyTrack", "Filter Key Track", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
                { "velDepth", "Filter Velocity", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 },
                { "fA", "Filter Attack", 0.5f, 10000.0f, 2.0f, 0.25f, " ms", 1 }, { "fD", "Filter Decay", 1.0f, 10000.0f, 350.0f, 0.25f, " ms", 0 },
                { "fS", "Filter Sustain", 0.0f, 100.0f, 20.0f, 1.0f, " %", 0 }, { "fR", "Filter Release", 1.0f, 15000.0f, 300.0f, 0.25f, " ms", 0 },
                { "aA", "Amp Attack", 0.5f, 10000.0f, 3.0f, 0.25f, " ms", 1 }, { "aD", "Amp Decay", 1.0f, 10000.0f, 400.0f, 0.25f, " ms", 0 },
                { "aS", "Amp Sustain", 0.0f, 100.0f, 80.0f, 1.0f, " %", 0 }, { "aR", "Amp Release", 1.0f, 15000.0f, 250.0f, 0.25f, " ms", 0 },
                { "ampVel", "Velocity", 0.0f, 100.0f, 60.0f, 1.0f, " %", 0 },
                { "lfoRate", "LFO Rate", 0.02f, 30.0f, 5.0f, 0.3f, " Hz", 2 }, { "lfoPitch", "LFO Pitch", 0.0f, 12.0f, 0.0f, 0.5f, " st", 2 },
                { "lfoCut", "LFO Cutoff", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 }, { "lfoShapeAmt", "LFO Shape", -3.0f, 3.0f, 0.0f, 1.0f, "", 2 },
                { "lfoWidth", "LFO Width", -1.0f, 1.0f, 0.0f, 1.0f, "", 2 }, { "lfoAmp", "LFO Tremolo", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
                { "lfoDelay", "LFO Delay", 0.0f, 5000.0f, 0.0f, 0.4f, " ms", 0 },
                { "glide", "Glide", 0.0f, 2000.0f, 0.0f, 0.4f, " ms", 0 }, { "uniDetune", "Unison Detune", 0.0f, 50.0f, 8.0f, 0.6f, " ct", 1 },
                { "uniSpread", "Unison Spread", 0.0f, 100.0f, 60.0f, 1.0f, " %", 0 }, { "level", "Level", -24.0f, 6.0f, -6.0f, 1.0f, " dB", 1 } };
            return v;
        }
        static juce::StringArray lfoNames()   { return { "Sine", "Triangle", "Saw", "Square", "S&H", "Drift" }; }
        static juce::StringArray syncNames()  { return { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 dotted", "1/8 dotted", "1/4 triplet", "1/8 triplet" }; }
        static float syncBeats (int i)        { static const float b[12] = { 0, 16, 8, 4, 2, 1, 0.5f, 0.25f, 1.5f, 0.75f, 2.0f / 3, 1.0f / 3 }; return b[juce::jlimit (0, 11, i)]; }

        juce::CachedValue<int> filterType, slope, subOct, subWave, lfoWave, lfoSync;

        SqMotePlugin (te::PluginCreationInfo info) : Base (info)
        {
            bank.create (*this, paramSpecs());
            auto um = getUndoManager();
            filterType.referTo (state, "filterType", um, 0); slope.referTo (state, "slope", um, 24); subOct.referTo (state, "subOct", um, 1); subWave.referTo (state, "subWave", um, 0);
            lfoWave.referTo (state, "lfoWave", um, 0); lfoSync.referTo (state, "lfoSync", um, 0);
            for (auto& v : voices) v.set = &settings;
            outPeak = 0;
        }
        ~SqMotePlugin() override {}
        void resetProps (juce::UndoManager* um) override
        {
            Base::resetProps (um);
            filterType.setValue (0, um); slope.setValue (24, um); subOct.setValue (1, um); subWave.setValue (0, um); lfoWave.setValue (0, um); lfoSync.setValue (0, um);
        }
        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            Base::restorePluginStateFromValueTree (v);
            for (auto* cv : { &filterType, &slope, &subOct, &subWave, &lfoWave, &lfoSync }) if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
        }
        juce::StringArray modDestinations() const override { return { "pitch", "cutoff", "resonance", "shape", "width", "amp" }; }
        void prepareVoice (MoteVoice& v) override
        {
            const auto& m = v.mods;
            v.m.pitch = modFor ("pitch", m) * 24.0;
            v.m.cutOct = modFor ("cutoff", m) * 4.0 + m.get ((int) ModSource::modX) * 2.0;
            v.m.res = modFor ("resonance", m) + m.get ((int) ModSource::modY) * 0.3;
            v.m.shape = modFor ("shape", m) * 3.0; v.m.width = modFor ("width", m) * 0.45; v.m.amp = modFor ("amp", m);
            v.unisonCents = bank[uniDetune]; v.spread = bank[uniSpread] / 100.0;
            v.tune = bank[octave] * 12.0 + bank[semi] + bank[fine] / 100.0;
        }
        void updateChunk (const ModValues&) override
        {
            auto& s = settings;
            s.shape = bank[shape]; s.width = bank[width] / 100.0; s.oscLevel = bank[oscLevel] / 100.0; s.subLevel = bank[subLevel] / 100.0;
            s.subOct = juce::jlimit (1, 2, subOct.get()); s.subWave = juce::jlimit (0, 1, subWave.get());
            s.noiseLevel = bank[noise] / 100.0; s.noiseColour = bank[noiseColour] / 100.0; s.driveDb = bank[drive];
            s.filterType = juce::jlimit (0, 5, filterType.get()); s.slope = slope.get() >= 48 ? 48 : slope.get() >= 24 ? 24 : 12;
            s.cutoffHz = bank[cutoff]; s.res = bank[res] / 100.0; s.envOct = bank[fEnvDepth]; s.keyTrack = bank[keyTrack] / 100.0; s.velOct = bank[velDepth];
            const double fe[4] = { bank[fA] / 1000.0, bank[fD] / 1000.0, bank[fS] / 100.0, bank[fR] / 1000.0 }, ae[4] = { bank[aA] / 1000.0, bank[aD] / 1000.0, bank[aS] / 100.0, bank[aR] / 1000.0 };
            for (int i = 0; i < 4; ++i) { s.fEnv[i] = fe[i]; s.aEnv[i] = ae[i]; }
            s.ampVel = bank[ampVel] / 100.0; s.gain = 0.7;
            s.lfoPitch = bank[lfoPitch]; s.lfoCutOct = bank[lfoCut]; s.lfoShape = bank[lfoShapeAmt]; s.lfoWidth = bank[lfoWidth]; s.lfoAmp = bank[lfoAmp] / 100.0; s.lfoDelay = bank[lfoDelay] / 1000.0;
            lfoShape1 = (LfoShape) juce::jlimit (0, 5, lfoWave.get());
            setLfo (lfo1, bank[lfoRate], lfoSync.get() > 0, syncBeats (lfoSync.get()));
            glideTime = bank[glide] / 1000.0f; unisonSpread = 1.0f;
            lfoValue = globalMods.get ((int) ModSource::lfo1);
        }
        void renderMaster (float* l, float* r, int n) override
        {
            const float g = juce::Decibels::decibelsToGain (bank[level]); float pl = 0, pr = 0;
            for (int i = 0; i < n; ++i) { l[i] *= g; r[i] *= g; pl = juce::jmax (pl, std::abs (l[i])); pr = juce::jmax (pr, std::abs (r[i])); }
            outPeak = juce::jmax (pl, pr);
            peakL = juce::jmax (peakL.load(), pl); peakR = juce::jmax (peakR.load(), pr);
        }
        /** The face reads (and clears) the loudest left / right samples since its last look. */
        void takePeaks (float& l, float& r) { l = peakL.exchange (0.0f); r = peakR.exchange (0.0f); }
        /** Envelope levels of the newest sounding voice (for the face's moving dots). */
        void newestVoice (float& amp, float& flt, float& cut, int& note) const
        {
            const MoteVoice* best = nullptr;
            for (auto& v : voices) if (v.active && (best == nullptr || v.ageCounter > best->ageCounter)) best = &v;
            if (best == nullptr) { amp = flt = 0; cut = (float) settings.cutoffHz; note = -1; return; }
            amp = best->dsp.lastAmp; flt = best->dsp.lastFlt; cut = best->dsp.lastCut; note = best->note;
        }
        const SqMote::Settings& currentSettings() const { return settings; }
        std::atomic<float> outPeak { 0 }, peakL { 0 }, peakR { 0 }, lfoValue { 0 };
        double sampleRate() const { return sr; }

    protected:
        SqMote::Settings settings;
    };
}
using SqMotePlugin = SqSynth::SqMotePlugin;
