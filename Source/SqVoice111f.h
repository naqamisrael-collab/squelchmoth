#pragma once
// 1.11 Voice, second wave part 2: Hive Choir (V4), Double Wing 2 (V16), Mutant (V14). DSP: SqVoice111fDsp.h (tests/voice111f_test.cpp).

#include "SqVoice111e.h"
#include "SqVoice111fDsp.h"

//==============================================================================
class SqHiveChoirPlugin : public SqColor111bBase, private juce::Timer, private juce::AsyncUpdater
{
public:
    // per voice: interval, level, pan, throat, unison, pitch var, time var, timbre, spread, vib rate, vib depth (11 x 4), then the globals
    static constexpr int perVoice = 11;
    enum G { lead = perVoice * 4, humanPitch, humanTime, consonants, width, mix, output, numParams };
    enum V { vInterval, vLevel, vPan, vThroat, vUnison, vPitchVar, vTimeVar, vTimbre, vSpread, vVibRate, vVibDepth };
    SQ_COLOUR_IDENTITY (SqHiveChoirPlugin, "Hive Choir", "sqmHiveChoir", "Choir")
    static int idx (int voice, int v) { return voice * perVoice + v; }
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            // ids and names live as long as the program (ParamSpec keeps const char*): reserved up front so they never move
            static std::vector<std::string> keep; keep.reserve (perVoice * 4 * 2);
            std::vector<SqDsp::ParamSpec> out;
            const float defInt[4] = { 4, 7, -5, 12 }, defPan[4] = { -0.5f, 0.5f, -0.25f, 0.25f };
            for (int k = 0; k < 4; ++k)
            {
                auto add = [&] (const char* id, const char* nm, float lo, float hi, float def, float skew, const char* unit, int dec)
                {
                    keep.push_back ("v" + std::to_string (k + 1) + id); const char* idp = keep.back().c_str();
                    keep.push_back ("Voice " + std::to_string (k + 1) + " " + nm); const char* nmp = keep.back().c_str();
                    out.push_back ({ idp, nmp, lo, hi, def, skew, unit, dec });
                };
                add ("Interval", "Interval", -24.0f, 24.0f, defInt[k], 1.0f, " st", 0);
                add ("Level", "Level", -60.0f, 6.0f, -4.0f, 1.0f, " dB", 1);
                add ("Pan", "Pan", -100.0f, 100.0f, defPan[k] * 100.0f, 1.0f, " %", 0);
                add ("Throat", "Throat", -12.0f, 12.0f, 0.0f, 1.0f, " st", 1);
                add ("Unison", "Singers", 1.0f, 32.0f, 1.0f, 1.0f, "", 0);
                add ("PitchVar", "Pitch Variation", 0.0f, 50.0f, 8.0f, 1.0f, " ct", 0);
                add ("TimeVar", "Time Variation", 0.0f, 80.0f, 15.0f, 1.0f, " ms", 0);
                add ("Timbre", "Timbre Variation", 0.0f, 100.0f, 30.0f, 1.0f, " %", 0);
                add ("Spread", "Spread", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0);
                add ("VibRate", "Vibrato Rate", 2.0f, 9.0f, 5.2f, 1.0f, " Hz", 1);
                add ("VibDepth", "Vibrato Depth", 0.0f, 100.0f, 0.0f, 1.0f, " ct", 0);
            }
            out.push_back ({ "lead", "Lead Level", -60.0f, 6.0f, 0.0f, 1.0f, " dB", 1 });
            out.push_back ({ "humanPitch", "Humanize Pitch", 0.0f, 30.0f, 6.0f, 1.0f, " ct", 0 });
            out.push_back ({ "humanTime", "Humanize Time", 0.0f, 50.0f, 12.0f, 1.0f, " ms", 0 });
            out.push_back ({ "consonants", "Consonants", 0.0f, 100.0f, 35.0f, 1.0f, " %", 0 });
            out.push_back ({ "width", "Width", 0.0f, 100.0f, 100.0f, 1.0f, " %", 0 });
            out.push_back ({ "mix", "Mix", 0.0f, 100.0f, 100.0f, 1.0f, " %", 0 });
            out.push_back ({ "output", "Output", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 });
            return out;
        }();
        return s;
    }
    static juce::StringArray modeNames() { juce::StringArray a; for (int i = 0; i < HiveChoir::numModes; ++i) a.add (HiveChoir::modeName (i)); return a; }
    static juce::StringArray engineNames() { return { "Live", "Studio" }; }
    static juce::StringArray voiceNames()  { juce::StringArray s; for (auto& v : SqVocal::voiceTypes()) s.add (v.name); return s; }
    SqHiveChoirPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        static const int defMode[4] = { 1, 1, 0, 0 };
        for (int k = 0; k < 4; ++k) mode[(size_t) k].referTo (state, "mode" + juce::String (k + 1), um, defMode[k]);
        engine.referTo (state, "engine", um, 1); voiceType.referTo (state, "voiceType", um, 1); autoKey.referTo (state, "autoKeyOn", um, true);
        keyRoot.referTo (state, "keyRoot", um, 0); scale.referTo (state, "scale", um, 1); seed.referTo (state, "seed", um, 1);
        startTimerHz (4);
    }
    ~SqHiveChoirPlugin() override { stopTimer(); cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }
    std::array<juce::CachedValue<int>, 4> mode; juce::CachedValue<int> engine, voiceType, keyRoot, scale, seed; juce::CachedValue<bool> autoKey;
    void resetProps (juce::UndoManager* um) override
    {
        static const int defMode[4] = { 1, 1, 0, 0 };
        for (int k = 0; k < 4; ++k) mode[(size_t) k].setValue (defMode[k], um);
        engine.setValue (1, um); voiceType.setValue (1, um); autoKey.setValue (true, um); keyRoot.setValue (0, um); scale.setValue (1, um); seed.setValue (1, um);
    }
    bool takesMidiInput() override { return true; }
    double getLatencySeconds() override { return (double) SqVocal::TuneProcessor::latencyFor (juce::jlimit (0, 1, engine.get()), juce::jlimit (0, 5, voiceType.get()), sr) / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; sessionKey.poll (edit); read(); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); held.fill (false); }
    void deinitialise() override {}
    void reset() override { proc.reset(); held.fill (false); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read();
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) held.fill (false);
            for (auto& m : *fc.bufferForMidiMessages)
            { if (m.isNoteOn()) held[(size_t) m.getNoteNumber()] = true; else if (m.isNoteOff()) held[(size_t) m.getNoteNumber()] = false; else if (m.isAllNotesOff() || m.isAllSoundOff()) held.fill (false); }
        }
        proc.setHeld (held);
        render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); liveHz = proc.inputHz(); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && (id == juce::Identifier ("engine") || id == juce::Identifier ("voiceType"))) triggerAsyncUpdate();
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    HiveChoir::Params knobParams() const { HiveChoir::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    HiveChoir::Processor proc; SqSessionKey sessionKey; std::atomic<float> liveHz { 0 };
private:
    std::array<bool, 128> held {};
    void timerCallback() override { sessionKey.poll (edit); }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
    void fill (const float* v, HiveChoir::Params& p) const
    {
        p.engine = juce::jlimit (0, 1, engine.get()); p.voiceType = juce::jlimit (0, 5, voiceType.get());
        const bool sess = autoKey.get() && sessionKey.known.load();
        p.keyRoot = sess ? sessionKey.root.load() : juce::jlimit (0, 11, keyRoot.get());
        p.scaleMask = SqSessionKey::relMask (sess ? sessionKey.scale.load() : scale.get());
        for (int k = 0; k < 4; ++k)
        {
            auto& x = p.v[k]; const float* b = v + k * perVoice;
            x.mode = juce::jlimit (0, HiveChoir::numModes - 1, mode[(size_t) k].get());
            x.interval = b[vInterval]; x.levelDb = b[vLevel]; x.pan = b[vPan] / 100.0f; x.throatSt = b[vThroat]; x.unison = juce::jlimit (1, 32, juce::roundToInt (b[vUnison]));
            x.pitchVar = b[vPitchVar]; x.timeVar = b[vTimeVar]; x.timbreVar = b[vTimbre] / 100.0f; x.spread = b[vSpread] / 100.0f; x.vibHz = b[vVibRate]; x.vibCents = b[vVibDepth];
        }
        p.leadDb = v[lead]; p.humanCents = v[humanPitch]; p.humanMs = v[humanTime]; p.consonants = v[consonants] / 100.0f; p.width = v[width] / 100.0f;
        p.mix = v[mix] / 100.0f; p.outputDb = v[output]; p.seed = (uint32_t) juce::jmax (1, seed.get());
    }
    void read() { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, proc.p); }
};

//==============================================================================
class SqDoubleWing2Plugin : public SqColor111bBase
{
public:
    enum P { pitchVar, timeVar, timbre, spread, vibRate, vibDepth, doublesLevel, lead, width, output, numParams };
    SQ_COLOUR_IDENTITY (SqDoubleWing2Plugin, "Double Wing 2", "sqmDoubleWing2", "Double 2")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "pitchVar",     "Pitch Variation",  0.0f, 40.0f,   9.0f, 1.0f, " ct", 0 },
            { "timeVar",      "Timing Variation", 2.0f, 80.0f,  22.0f, 1.0f, " ms", 0 },
            { "timbre",       "Timbre Variation", 0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "spread",       "Spread",           0.0f, 100.0f, 80.0f, 1.0f, " %", 0 },
            { "vibRate",      "Vibrato Rate",     2.0f, 9.0f,    5.4f, 1.0f, " Hz", 1 },
            { "vibDepth",     "Vibrato Depth",    0.0f, 80.0f,   0.0f, 1.0f, " ct", 0 },
            { "doublesLevel", "Doubles Level",  -60.0f, 6.0f,   -3.0f, 1.0f, " dB", 1 },
            { "lead",         "Lead Level",     -60.0f, 6.0f,    0.0f, 1.0f, " dB", 1 },
            { "width",        "Width",            0.0f, 200.0f, 100.0f, 1.0f, " %", 0 },
            { "output",       "Output",         -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray countNames() { return { "1 Double", "2 Doubles", "3 Doubles", "4 Doubles" }; }
    SqDoubleWing2Plugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        doubles.referTo (state, "doubles", um, 2); voiceRange.referTo (state, "voiceRange", um, 1); vibStraight.referTo (state, "vibStraightOn", um, true); seed.referTo (state, "seed", um, 3);
    }
    ~SqDoubleWing2Plugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> doubles, voiceRange, seed; juce::CachedValue<bool> vibStraight;
    void resetProps (juce::UndoManager* um) override { doubles.setValue (2, um); voiceRange.setValue (1, um); vibStraight.setValue (true, um); seed.setValue (3, um); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); }); }
    DoubleWing2::Params knobParams() const { DoubleWing2::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    DoubleWing2::Processor proc;
private:
    void fill (const float* v, DoubleWing2::Params& p) const
    {
        p.doubles = juce::jlimit (1, 4, doubles.get()); p.voiceRange = juce::jlimit (0, 5, voiceRange.get()); p.vibStraightOnly = vibStraight.get(); p.seed = (uint32_t) juce::jmax (1, seed.get());
        p.pitchVar = v[pitchVar]; p.timeVar = v[timeVar]; p.timbreVar = v[timbre] / 100.0f; p.spread = v[spread] / 100.0f; p.vibHz = v[vibRate]; p.vibCents = v[vibDepth];
        p.doublesDb = v[doublesLevel]; p.leadDb = v[lead]; p.width = v[width] / 100.0f; p.outputDb = v[output];
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        // the variation settings set up the doubles (no smoothing there); levels and width are smoothed
        float lv[3] = { v[doublesLevel], v[lead], v[width] }; smooth (lv, 3, snap); v[doublesLevel] = lv[0]; v[lead] = lv[1]; v[width] = lv[2];
        fill (v, proc.p);
    }
};

//==============================================================================
class SqMutantPlugin : public SqColor111bBase, private juce::AsyncUpdater
{
public:
    enum P { pitch, throat, ringMix, ringRatio, ringFallback, alien, alienGlide, stutter, drive, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqMutantPlugin, "Mutant", "sqmMutant", "Mutant")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "pitch",        "Pitch",          -24.0f, 24.0f,    0.0f, 1.0f, " st", 2 },
            { "throat",       "Throat",         -24.0f, 24.0f,    0.0f, 1.0f, " st", 2 },
            { "ringMix",      "Ring Mod",         0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "ringRatio",    "Ring Ratio",     0.125f, 8.0f,     1.0f, 0.4f, " x", 3 },
            { "ringFallback", "Ring Unvoiced",   40.0f, 2000.0f, 220.0f, 0.4f, " Hz", 0 },
            { "alien",        "Alienization",     0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "alienGlide",   "Alien Glide",      0.0f, 500.0f,  30.0f, 0.5f, " ms", 0 },
            { "stutter",      "Stutter",          0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "drive",        "Drive",            0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "mix",          "Mix",              0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",       "Output",         -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray engineNames() { return { "Live", "Studio" }; }
    static juce::StringArray voiceNames()  { juce::StringArray s; for (auto& v : SqVocal::voiceTypes()) s.add (v.name); return s; }
    static juce::StringArray driveNames()  { juce::StringArray a { "Off" }; for (int i = 0; i < Eng::AnalogStyle::numVoicings; ++i) a.add (Eng::AnalogStyle::voicingName (i)); return a; }
    static juce::StringArray repeatNames() { return { "2", "3", "4", "6", "8", "12", "16" }; }
    static int repeatsFor (int i) { static const int r[] = { 2, 3, 4, 6, 8, 12, 16 }; return r[juce::jlimit (0, 6, i)]; }
    SqMutantPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        engine.referTo (state, "engine", um, 0); voiceType.referTo (state, "voiceType", um, 1); alienDiv.referTo (state, "alienDiv", um, 9); stutterDiv.referTo (state, "stutterDiv", um, 6);
        repeats.referTo (state, "repeats", um, 2); driveVoicing.referTo (state, "driveVoicing", um, 0); seed.referTo (state, "seed", um, 5);
    }
    ~SqMutantPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> engine, voiceType, alienDiv, stutterDiv, repeats, driveVoicing, seed;
    void resetProps (juce::UndoManager* um) override
    { engine.setValue (0, um); voiceType.setValue (1, um); alienDiv.setValue (9, um); stutterDiv.setValue (6, um); repeats.setValue (2, um); driveVoicing.setValue (0, um); seed.setValue (5, um); }
    double getLatencySeconds() override
    {
        const int L = SqVocal::TuneProcessor::latencyFor (juce::jlimit (0, 1, engine.get()), juce::jlimit (0, 5, voiceType.get()), sr);
        return (double) L / juce::jmax (1.0, sr);
    }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.setTransport (bpm, ppq, playing); proc.process (ch, nch, n); liveHz = proc.inputHz(); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && (id == juce::Identifier ("engine") || id == juce::Identifier ("voiceType") || id == juce::Identifier ("driveVoicing"))) triggerAsyncUpdate();
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    Mutant::Params knobParams() const { Mutant::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Mutant::Processor proc; std::atomic<float> liveHz { 0 };
private:
    void handleAsyncUpdate() override { edit.restartPlayback(); }
    void fill (const float* v, Mutant::Params& p) const
    {
        p.engine = juce::jlimit (0, 1, engine.get()); p.voiceType = juce::jlimit (0, 5, voiceType.get());
        p.pitchSt = v[pitch]; p.throatSt = v[throat]; p.ringMix = v[ringMix] / 100.0f; p.ringRatio = v[ringRatio]; p.ringFallbackHz = v[ringFallback];
        p.alien = v[alien] / 100.0f; p.alienDiv = juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, alienDiv.get()); p.alienGlideMs = v[alienGlide];
        p.stutter = v[stutter] / 100.0f; p.stutterDiv = juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, stutterDiv.get()); p.stutterRepeats = repeatsFor (repeats.get());
        p.driveVoicing = juce::jlimit (0, (int) Eng::AnalogStyle::numVoicings, driveVoicing.get()); p.drive = v[drive] / 100.0f;
        p.mix = v[mix] / 100.0f; p.outputDb = v[output]; p.seed = (uint32_t) juce::jmax (1, seed.get());
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};
