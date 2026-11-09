#pragma once
// 1.11 Color & Space, second wave part 3: Larva Filter 2 (C6), Twelve Phase 2 (C7), Hive Rack (C16).
// DSP: SqColor111dDsp.h (tests/color111d_test.cpp). Hive Rack hosts the other plug-ins (any Signature effect).
// Larva Filter 2 and Twelve Phase 2 are new plug-ins next to Larva Filter and Twelve Phase, which stay as they are so
// saved sessions and Creative FX owners keep them.

#include "SqColor111c.h"
#include "SqColor111dDsp.h"
#include "SqHiveChains.h"

/** What Hive Rack needs from the plug-in catalogue and the edition gate (defined in SqHiveHost.inl, after Signature.h). */
namespace HiveHost
{
    inline juce::StringArray slotChoices();
    /** Resolves a Signature name (exact, then partial) to its brand name and engine type; false when there is none. */
    inline bool resolve (const juce::String& name, juce::String& outName, juce::String& outType);
    /** Applies the named preset (or, with none named, the first) to a fresh plug-in; returns the preset used. */
    inline juce::String applyPreset (te::Plugin& p, const juce::String& type, const juce::String& presetName);
    inline bool isTypeAllowed (const juce::String& type);
    inline juce::String lockedBy (const juce::String& type);
}

/** The Wing Mod controls both filter / phaser plug-ins share: modes, shapes, sync, smoothing, trigger, rhythm steps. */
struct SqWingModProps
{
    juce::CachedValue<int> mode, shape, sync, smoothing, trigger, numSteps, stepDivision, pattern; juce::CachedValue<juce::String> steps;
    SqStepStore store;
    void init (juce::ValueTree& st, juce::UndoManager* um, int defMode)
    {
        mode.referTo (st, "modMode", um, defMode); shape.referTo (st, "modShape", um, 0); sync.referTo (st, "modSync", um, 0); smoothing.referTo (st, "modSmooth", um, 0);
        trigger.referTo (st, "modTrigger", um, 0); numSteps.referTo (st, "numSteps", um, 16); stepDivision.referTo (st, "stepDivision", um, 6); pattern.referTo (st, "pattern", um, 1);
        steps.referTo (st, "steps", um, {});
    }
    void reset (juce::UndoManager* um, int defMode)
    {
        mode.setValue (defMode, um); shape.setValue (0, um); sync.setValue (0, um); smoothing.setValue (0, um); trigger.setValue (0, um); numSteps.setValue (16, um);
        stepDivision.setValue (6, um); pattern.setValue (1, um); steps.setValue ({}, um);
    }
    void reparse() { const auto& pats = Tremor::patterns(); store.set (steps.get(), pats[(size_t) juce::jlimit (0, (int) pats.size() - 1, pattern.get())].v); }
    bool isProp (const juce::Identifier& id) const { return id == juce::Identifier ("steps") || id == juce::Identifier ("pattern"); }
    /** rate Hz, groove / feel 0..1, envelope attack / release ms, sensitivity dB, ADSR (ms, ms, 0..1, ms). */
    void fill (Eng::WingMod::Settings& s, float rate, float groove, float feel, float eA, float eR, float eS, float a, float d, float sus, float r) const
    {
        s.mode = juce::jlimit (0, (int) Eng::WingMod::numModes - 1, mode.get()); s.shape = juce::jlimit (0, (int) Eng::WingMod::numShapes - 1, shape.get());
        s.sync = sync.get() > 0; s.syncIndex = juce::jmax (0, sync.get() - 1); s.rateHz = rate; s.smoothing = juce::jlimit (0, 4, smoothing.get());
        s.trigger = juce::jlimit (0, 2, trigger.get()); s.numSteps = juce::jlimit (1, 16, numSteps.get()); s.stepDivision = juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, stepDivision.get());
        s.swing = groove; s.feel = feel; s.attackMs = eA; s.releaseMs = eR; s.sensitivityDb = eS; s.aMs = a; s.dMs = d; s.sLevel = sus; s.rMs = r;
        s.bipolar = true; s.retrigger = s.trigger != 0;
    }
    static juce::StringArray modeNames() { juce::StringArray a; for (int i = 0; i < Eng::WingMod::numModes; ++i) a.add (Eng::WingMod::modeName (i)); return a; }
    static juce::StringArray smoothNames() { juce::StringArray a; for (int i = 0; i < Eng::WingMod::numSmoothing; ++i) a.add (Eng::WingMod::smoothingName (i)); return a; }
    static juce::StringArray triggerNames() { return { "Audio", "MIDI", "Button" }; }
};

//==============================================================================
class SqLarvaFilter2Plugin : public SqColor111bBase
{
public:
    enum P { cutoff, resonance, freqMod, resMod, lrOffset, drive, mix, output, input, s1, s2, s3, s4, s5, s6, s7, s8,
             modRate, groove, feel, envAttack, envRelease, envSens, adsrA, adsrD, adsrS, adsrR, numParams };
    SQ_COLOUR_IDENTITY (SqLarvaFilter2Plugin, "Larva Filter 2", "sqmLarvaFilter2", "Larva 2")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "cutoff",     "Cutoff",       20.0f, 20000.0f, 800.0f, 0.25f, " Hz", 0 },
            { "resonance",  "Resonance",     0.0f, 100.0f,    40.0f, 1.0f, " %", 0 },
            { "freqMod",    "Freq Mod",    -10.0f, 10.0f,      2.0f, 1.0f, " oct", 2 },
            { "resMod",     "Res Mod",    -100.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "lrOffset",   "L/R Offset",    0.0f, 2.0f,       0.0f, 1.0f, " oct", 2 },
            { "drive",      "Drive",         0.0f, 100.0f,    50.0f, 1.0f, " %", 0 },
            { "mix",        "Mix",           0.0f, 100.0f,   100.0f, 1.0f, " %", 0 },
            { "output",     "Output",      -24.0f, 24.0f,      0.0f, 1.0f, " dB", 1 },
            { "input",      "Input",       -24.0f, 12.0f,      0.0f, 1.0f, " dB", 1 },
            { "s1", "Stage 1 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 }, { "s2", "Stage 2 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 },
            { "s3", "Stage 3 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 }, { "s4", "Stage 4 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 },
            { "s5", "Stage 5 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 }, { "s6", "Stage 6 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 },
            { "s7", "Stage 7 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 }, { "s8", "Stage 8 Offset", -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 },
            { "modRate",    "Mod Rate",      0.01f, 100.0f,    1.0f, 0.25f, " Hz", 2 },
            { "groove",     "Groove",        0.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "feel",       "Feel",          0.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "envAttack",  "Env Attack",    0.1f, 1000.0f,   10.0f, 0.3f, " ms", 1 },
            { "envRelease", "Env Release",   1.0f, 5000.0f,  150.0f, 0.3f, " ms", 0 },
            { "envSens",    "Env Sensitivity", -24.0f, 24.0f,  0.0f, 1.0f, " dB", 1 },
            { "adsrA",      "Attack",        0.1f, 5000.0f,    5.0f, 0.3f, " ms", 1 },
            { "adsrD",      "Decay",         1.0f, 5000.0f,  150.0f, 0.3f, " ms", 0 },
            { "adsrS",      "Sustain",       0.0f, 100.0f,    60.0f, 1.0f, " %", 0 },
            { "adsrR",      "Release",       1.0f, 5000.0f,  300.0f, 0.3f, " ms", 0 } };
        return s;
    }
    static juce::StringArray responseNames() { return { "LP", "HP", "BP", "Notch" }; }
    static juce::StringArray modelNames() { return { "Transistor", "Diode", "Sallen-Key", "SVF" }; }
    static juce::StringArray routingNames() { return { "Series", "Parallel", "4 + 4" }; }
    SqLarvaFilter2Plugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        stages.referTo (state, "stages", um, 2); routing.referTo (state, "routing", um, 0); analog.referTo (state, "analog", um, 0); analogPost.referTo (state, "analogPostOn", um, false);
        keyOn.referTo (state, "keyOn", um, false);
        for (int i = 0; i < 8; ++i) { resp[(size_t) i].referTo (state, "resp" + juce::String (i + 1), um, defResp (i)); model[(size_t) i].referTo (state, "model" + juce::String (i + 1), um, i % 4); }
        wm.init (state, um, Eng::WingMod::lfo);
    }
    ~SqLarvaFilter2Plugin() override { notifyListenersOfDeletion(); bank.detach(); }
    static int defResp (int i) { static const int r[8] { 0, 0, 2, 1, 0, 2, 3, 0 }; return r[i & 7]; }
    juce::CachedValue<int> stages, routing, analog; juce::CachedValue<bool> analogPost, keyOn; std::array<juce::CachedValue<int>, 8> resp, model; SqWingModProps wm;
    void resetProps (juce::UndoManager* um) override
    {
        stages.setValue (2, um); routing.setValue (0, um); analog.setValue (0, um); analogPost.setValue (false, um); keyOn.setValue (false, um);
        for (int i = 0; i < 8; ++i) { resp[(size_t) i].setValue (defResp (i), um); model[(size_t) i].setValue (i % 4, um); }
        wm.reset (um, Eng::WingMod::lfo);
    }
    bool takesMidiInput() override { return true; }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; wm.reparse(); read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        if (fc.bufferForMidiMessages != nullptr && wm.trigger.get() == 1)
            for (auto& m : *fc.bufferForMidiMessages) { if (m.isNoteOn()) proc.trigger(); else if (m.isNoteOff()) proc.release(); }
        if (manualTrigger.exchange (false)) proc.trigger();
        render (fc, keyOn.get(), [this] (float** ch, int nch, const float* const* key, int n) { proc.setTransport (bpm, ppq, playing); proc.process (ch, nch, key, n); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override { if (v == state && wm.isProp (id)) wm.reparse(); SqColor111bBase::valueTreePropertyChanged (v, id); }
    Larva2::Params knobParams() const { Larva2::Params p = proc.p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Larva2::Processor proc; std::atomic<bool> manualTrigger { false };
private:
    void fill (const float* v, Larva2::Params& p) const
    {
        p.stages = juce::jlimit (1, 8, stages.get()); p.routing = juce::jlimit (0, 2, routing.get());
        for (int i = 0; i < 8; ++i) { p.response[i] = juce::jlimit (0, 3, resp[(size_t) i].get()); p.model[i] = juce::jlimit (0, 3, model[(size_t) i].get()); p.stageOct[i] = v[s1 + i]; }
        p.cutoffHz = v[cutoff]; p.resonance = v[resonance] / 100.0f; p.freqModOct = v[freqMod]; p.resMod = v[resMod] / 100.0f; p.lrOffsetOct = v[lrOffset];
        p.analog = juce::jlimit (0, (int) Eng::AnalogStyle::numVoicings, analog.get()); p.analogAmount = v[drive] / 100.0f; p.analogPost = analogPost.get();
        p.mix = v[mix] / 100.0f; p.outputDb = v[output]; p.inputDb = v[input]; p.keyOn = keyOn.get();
        wm.fill (p.mod, v[modRate], v[groove] / 100.0f, v[feel] / 100.0f, v[envAttack], v[envRelease], v[envSens], v[adsrA], v[adsrD], v[adsrS] / 100.0f, v[adsrR]);
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        fill (v, proc.p); wm.store.take (proc.p.mod.steps); for (auto& s : proc.p.mod.steps) s = juce::jlimit (0.0f, 1.0f, s);
    }
};

//==============================================================================
class SqTwelvePhase2Plugin : public SqColor111bBase
{
public:
    enum P { centre, sweep, resonance, depth, spread, mix, output, drive, modRate, groove, feel, envAttack, envRelease, envSens, adsrA, adsrD, adsrS, adsrR, numParams };
    SQ_COLOUR_IDENTITY (SqTwelvePhase2Plugin, "Twelve Phase 2", "sqmPhase12v2", "Phase 2")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "centre",     "Centre",       50.0f, 10000.0f, 900.0f, 0.3f, " Hz", 0 },
            { "sweep",      "Sweep",         0.0f, 8.0f,       3.0f, 1.0f, " oct", 2 },
            { "resonance",  "Resonance",     0.0f, 100.0f,    50.0f, 1.0f, " %", 0 },
            { "depth",      "Intensity",     0.0f, 100.0f,   100.0f, 1.0f, " %", 0 },
            { "spread",     "Stereo Phase",  0.0f, 180.0f,    90.0f, 1.0f, " deg", 0 },
            { "mix",        "Mix",           0.0f, 100.0f,   100.0f, 1.0f, " %", 0 },
            { "output",     "Output",      -24.0f, 12.0f,      0.0f, 1.0f, " dB", 1 },
            { "drive",      "Drive",         0.0f, 100.0f,    50.0f, 1.0f, " %", 0 },
            { "modRate",    "Mod Rate",      0.01f, 100.0f,    0.4f, 0.25f, " Hz", 2 },
            { "groove",     "Groove",        0.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "feel",       "Feel",          0.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "envAttack",  "Env Attack",    0.1f, 1000.0f,   10.0f, 0.3f, " ms", 1 },
            { "envRelease", "Env Release",   1.0f, 5000.0f,  200.0f, 0.3f, " ms", 0 },
            { "envSens",    "Env Sensitivity", -24.0f, 24.0f,  0.0f, 1.0f, " dB", 1 },
            { "adsrA",      "Attack",        0.1f, 5000.0f,    5.0f, 0.3f, " ms", 1 },
            { "adsrD",      "Decay",         1.0f, 5000.0f,  150.0f, 0.3f, " ms", 0 },
            { "adsrS",      "Sustain",       0.0f, 100.0f,    60.0f, 1.0f, " %", 0 },
            { "adsrR",      "Release",       1.0f, 5000.0f,  300.0f, 0.3f, " ms", 0 } };
        return s;
    }
    static juce::StringArray styleNames() { juce::StringArray a; for (auto& s : Phase2::styles()) a.add (s.name); return a; }
    static juce::StringArray stageNames() { juce::StringArray a; for (int i = 2; i <= 24; ++i) a.add (juce::String (i)); return a; }
    SqTwelvePhase2Plugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        style.referTo (state, "style", um, 30); stages.referTo (state, "stages", um, 12); classic.referTo (state, "classicOn", um, false); resMode.referTo (state, "resMode", um, 0);
        polarity.referTo (state, "polarity", um, 0); tz.referTo (state, "tzOn", um, false); analog.referTo (state, "analog", um, 0); analogPost.referTo (state, "analogPostOn", um, false);
        keyOn.referTo (state, "keyOn", um, false);
        wm.init (state, um, Eng::WingMod::lfo);
    }
    ~SqTwelvePhase2Plugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> style, stages, resMode, polarity, analog; juce::CachedValue<bool> classic, tz, analogPost, keyOn; SqWingModProps wm;
    void resetProps (juce::UndoManager* um) override
    {
        style.setValue (30, um); stages.setValue (12, um); classic.setValue (false, um); resMode.setValue (0, um); polarity.setValue (0, um); tz.setValue (false, um);
        analog.setValue (0, um); analogPost.setValue (false, um); keyOn.setValue (false, um); wm.reset (um, Eng::WingMod::lfo);
    }
    /** Picking a style loads its archetype: stage count, colour, resonance routing, polarity, through-zero, and its sweep. */
    void applyStyle (int i, juce::UndoManager* um)
    {
        const auto& sv = Phase2::styles()[(size_t) juce::jlimit (0, (int) Phase2::styles().size() - 1, i)];
        style.setValue (i, um); stages.setValue (sv.stages, um); classic.setValue (sv.classic, um); resMode.setValue (sv.resMode, um); polarity.setValue (sv.polarity, um); tz.setValue (sv.tz, um);
        wm.shape.setValue (sv.shape, um); wm.mode.setValue (Eng::WingMod::lfo, um);
        auto set = [this] (int k, float val) { bank.params[(size_t) k]->setParameter (val, juce::sendNotification); };
        set (centre, sv.centreHz); set (sweep, sv.sweepOct); set (resonance, sv.resonance * 100.0f); set (spread, sv.spreadDeg); set (modRate, sv.rateHz);
    }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    bool takesMidiInput() override { return true; }
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; wm.reparse(); read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, keyOn.get(), [this] (float** ch, int nch, const float* const* key, int n) { proc.setTransport (bpm, ppq, playing); proc.process (ch, nch, key, n); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override { if (v == state && wm.isProp (id)) wm.reparse(); SqColor111bBase::valueTreePropertyChanged (v, id); }
    Phase2::Params knobParams() const { Phase2::Params p = proc.p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Phase2::Processor proc;
private:
    void fill (const float* v, Phase2::Params& p) const
    {
        p.stages = juce::jlimit (2, 24, stages.get()); p.classic = classic.get(); p.resMode = juce::jlimit (0, 1, resMode.get()); p.polarity = juce::jlimit (0, 1, polarity.get()); p.throughZero = tz.get();
        p.centreHz = v[centre]; p.sweepOct = v[sweep]; p.resonance = v[resonance] / 100.0f; p.depth = v[depth] / 100.0f; p.spreadDeg = v[spread]; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
        p.analog = juce::jlimit (0, (int) Eng::AnalogStyle::numVoicings, analog.get()); p.analogAmount = v[drive] / 100.0f; p.analogPost = analogPost.get(); p.keyOn = keyOn.get();
        wm.fill (p.mod, v[modRate], v[groove] / 100.0f, v[feel] / 100.0f, v[envAttack], v[envRelease], v[envSens], v[adsrA], v[adsrD], v[adsrS] / 100.0f, v[adsrR]);
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        fill (v, proc.p); wm.store.take (proc.p.mod.steps); for (auto& s : proc.p.mod.steps) s = juce::jlimit (0.0f, 1.0f, s);
    }
};

//==============================================================================
/** Hive Rack: eight slots holding any Signature effect (Color & Space, Voice, or the older ones), serial or two lanes in
    parallel, global Input / Output / Mix, Recycle (the whole rack fed back, free or synced, through a limiter), eight
    macros mapped to any slot parameters, and 200 factory chains. Slots a copy does not own are shown locked with the
    product that unlocks them and are passed through. */
class SqHiveRackPlugin : public SqColor111bBase, private juce::Timer
{
public:
    enum P { input, output, mix, recycle, recycleTime, laneBalance, m1, m2, m3, m4, m5, m6, m7, m8, numParams };
    SQ_COLOUR_IDENTITY (SqHiveRackPlugin, "Hive Rack", "sqmHiveRack", "Hive Rack")
    static constexpr int numSlots = 8;
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "input",       "Input",       -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 },
            { "output",      "Output",      -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 },
            { "mix",         "Mix",           0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "recycle",     "Recycle",       0.0f, 95.0f,    0.0f, 1.0f, " %", 0 },
            { "recycleTime", "Recycle Time", 10.0f, 2000.0f, 250.0f, 0.4f, " ms", 0 },
            { "laneBalance", "Lane Balance",-100.0f, 100.0f,  0.0f, 1.0f, " %", 0 },
            { "m1", "Macro 1", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 }, { "m2", "Macro 2", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "m3", "Macro 3", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 }, { "m4", "Macro 4", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "m5", "Macro 5", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 }, { "m6", "Macro 6", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "m7", "Macro 7", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 }, { "m8", "Macro 8", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqHiveRackPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        routing.referTo (state, "routing", um, 0); syncOn.referTo (state, "syncOn", um, false); syncDiv.referTo (state, "syncDiv", um, 12); chain.referTo (state, "chain", um, -1);
        macroMap.referTo (state, "macroMap", um, {});
        rebuildFromState();
        startTimerHz (20);
    }
    ~SqHiveRackPlugin() override
    {
        stopTimer();
        { const juce::ScopedLock sl (slotLock); for (auto& s : slots) { if (s.plugin != nullptr && initialised) s.plugin->baseClassDeinitialise(); s.plugin = nullptr; } }
        notifyListenersOfDeletion(); bank.detach();
    }
    juce::CachedValue<int> routing, syncDiv, chain; juce::CachedValue<bool> syncOn; juce::CachedValue<juce::String> macroMap;
    void resetProps (juce::UndoManager* um) override
    {
        routing.setValue (0, um); syncOn.setValue (false, um); syncDiv.setValue (12, um); macroMap.setValue ({}, um);
        for (int i = 0; i < numSlots; ++i) clearSlot (i);
        loadedChain = -1; chain.setValue (-1, um);
    }
    static juce::StringArray routingNames() { return { "Serial", "Two Lanes" }; }
    /** Plug-ins a slot can hold: every Signature effect except the rack itself (instruments and utilities excluded). */
    static juce::StringArray slotChoices() { return HiveHost::slotChoices(); }
    // ---------------------------------------------------------------- slots (message thread)
    struct SlotInfo { juce::String name, type, preset, lockedBy; bool allowed = true; };
    SlotInfo slotInfo (int i) const
    {
        SlotInfo si; if (i < 0 || i >= numSlots) return si;
        const auto& s = slots[(size_t) i];
        si.name = s.name; si.type = s.type; si.preset = s.preset; si.allowed = s.allowed;
        if (! s.allowed && s.type.isNotEmpty()) si.lockedBy = HiveHost::lockedBy (s.type);
        return si;
    }
    te::Plugin* slotPlugin (int i) const { return (i >= 0 && i < numSlots) ? slots[(size_t) i].plugin.get() : nullptr; }
    /** Puts a Signature effect (by name) in a slot, optionally with one of its presets. Returns an error text or empty. */
    juce::String setSlot (int i, const juce::String& signatureName, const juce::String& presetName = {})
    {
        if (i < 0 || i >= numSlots) return "Slot must be 1-8";
        if (signatureName.isEmpty() || signatureName == "Empty") { clearSlot (i); return {}; }
        juce::String nm, ty;
        if (! HiveHost::resolve (signatureName, nm, ty)) return "No Signature plug-in called '" + signatureName + "'";
        if (ty == xmlTypeName) return "Hive Rack can't hold itself";
        auto np = edit.engine.getPluginManager().createNewPlugin (edit, ty, {});
        if (np == nullptr) return "Couldn't create " + nm;
        const auto usedPreset = HiveHost::applyPreset (*np, ty, presetName);
        install (i, np, nm, ty, usedPreset);
        return {};
    }
    void clearSlot (int i) { if (i >= 0 && i < numSlots) install (i, nullptr, {}, {}, {}); }
    /** Loads factory chain k (SqHiveChains.h): its routing and every slot's plug-in and preset. */
    void loadChain (int k)
    {
        const auto& cs = HiveChains::chains();
        if (k < 0 || k >= (int) cs.size()) { loadedChain = -1; return; }
        const auto& c = cs[(size_t) k];
        for (int i = 0; i < numSlots; ++i)
            if (i < (int) c.slots.size()) setSlot (i, c.slots[(size_t) i].plugin, c.slots[(size_t) i].preset); else clearSlot (i);
        routing.setValue (c.routing, nullptr);
        bank.params[(size_t) recycle]->setParameter (c.recycle, juce::sendNotification);
        bank.params[(size_t) recycleTime]->setParameter (c.recycleMs, juce::sendNotification);
        loadedChain = k;
    }
    /** Macro map text: "macro:slot:param:lo:hi;..." (1-based macro and slot, parameter by name or index, lo / hi 0..1). */
    struct MacroTarget { int macro = 0, slot = 0; juce::String param; float lo = 0, hi = 1; };
    std::vector<MacroTarget> macroTargets() const
    {
        std::vector<MacroTarget> v;
        for (auto& t : juce::StringArray::fromTokens (macroMap.get(), ";", ""))
        {
            auto f = juce::StringArray::fromTokens (t, ":", ""); if (f.size() < 3) continue;
            v.push_back ({ juce::jlimit (1, 8, f[0].getIntValue()), juce::jlimit (1, numSlots, f[1].getIntValue()), f[2].trim(), f.size() > 3 ? f[3].getFloatValue() : 0.0f, f.size() > 4 ? f[4].getFloatValue() : 1.0f });
        }
        return v;
    }
    juce::String mapMacro (int macro, int slot, const juce::String& param, float lo, float hi)
    {
        auto* p = slotPlugin (slot - 1); if (p == nullptr) return "Slot " + juce::String (slot) + " is empty";
        if (findParam (*p, param) == nullptr) return "No parameter '" + param + "' in " + slots[(size_t) slot - 1].name;
        auto s = macroMap.get(); if (s.isNotEmpty() && ! s.endsWith (";")) s << ";";
        s << macro << ":" << slot << ":" << param << ":" << juce::String (lo, 3) << ":" << juce::String (hi, 3);
        macroMap.setValue (s, getUndoManager()); lastMacro.fill (-1.0f);
        return {};
    }
    static te::AutomatableParameter* findParam (te::Plugin& p, const juce::String& nameOrIndex)
    {
        auto ps = p.getAutomatableParameters();
        if (nameOrIndex.containsOnly ("0123456789")) { const int k = nameOrIndex.getIntValue(); return k >= 0 && k < ps.size() ? ps[k] : nullptr; }
        for (auto ap : ps) if (ap->getParameterName().equalsIgnoreCase (nameOrIndex) || ap->paramID.equalsIgnoreCase (nameOrIndex)) return ap;
        return nullptr;
    }
    // ---------------------------------------------------------------- engine
    bool producesAudioWhenNoAudioInput() override { return true; }
    bool noTail() override { return false; }
    double getTailLength() const override
    {
        double t = 0; for (auto& s : slots) if (s.plugin != nullptr) t = juce::jmax (t, s.plugin->getTailLength());
        return juce::jlimit (0.0, 60.0, t + (bank[recycle] > 1.0f ? 4.0 : 0.0));
    }
    double getLatencySeconds() override
    {
        double serial = 0, laneA = 0, laneB = 0;
        for (int i = 0; i < numSlots; ++i) if (auto* p = slots[(size_t) i].plugin.get()) { const double l = p->getLatencySeconds(); serial += l; (i < 4 ? laneA : laneB) += l; }
        return routing.get() == 1 ? juce::jmax (laneA, laneB) : serial;
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; lastInfo = info;
        const juce::ScopedLock sl (slotLock);
        for (auto& s : slots) if (s.plugin != nullptr) s.plugin->baseClassInitialise (info);
        initialised = true;
        const int mx = juce::jmax (64, info.blockSizeSamples);
        lane.setSize (2, mx); dryBuf.setSize (2, mx); recBuf.setSize (2, (int) (sr * 2.1) + mx); recBuf.clear(); recW = 0; limG = 1;
    }
    void deinitialise() override
    {
        const juce::ScopedLock sl (slotLock);
        for (auto& s : slots) if (s.plugin != nullptr) s.plugin->baseClassDeinitialise();
        initialised = false;
    }
    void reset() override { const juce::ScopedLock sl (slotLock); for (auto& s : slots) if (s.plugin != nullptr) s.plugin->reset(); recBuf.clear(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        readTransport (fc);
        float* io[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk;
        const float inG = Eng::dbToGain (bank[input]), outG = Eng::dbToGain (bank[output]), mx = bank[mix] / 100.0f, rc = bank[recycle] / 100.0f;
        if (dryBuf.getNumSamples() < n) return;      // initialise sizes it; never allocate here
        for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) dryBuf.setSample (c, i, io[c][i]);
        // Recycle: the rack's output, Recycle Time ago, back into its input (through a limiter so it can't run away)
        const int recLen = recBuf.getNumSamples() - n - 1;
        const double recMs = syncOn.get() ? Eng::WingMod::syncBeats (juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, syncDiv.get())) * 60000.0 / bpm : (double) bank[recycleTime];
        const int recD = juce::jlimit (n, recLen, (int) (recMs * 0.001 * sr));
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < nch; ++c)
            {
                float v = io[c][i] * inG;
                if (rc > 0.001f) v += rc * limG * recBuf.getSample (c, (recW + i - recD + recBuf.getNumSamples()) % recBuf.getNumSamples());
                io[c][i] = v;
            }
        juce::AudioBuffer<float> view (io, nch, n);
        const juce::ScopedTryLock sl (slotLock);
        if (sl.isLocked())
        {
            auto runSlot = [&] (int i, juce::AudioBuffer<float>& buf)
            {
                auto& s = slots[(size_t) i];
                if (s.plugin == nullptr || ! s.allowed || s.plugin->isEnabled() == false) return;
                te::PluginRenderContext c (&buf, 0, n, fc.bufferForMidiMessages, fc.midiBufferOffset, fc.editTime, fc.isPlaying, fc.isScrubbing, fc.isRendering, false);
                s.plugin->applyToBuffer (c);
            };
            if (routing.get() == 1)
            {
                // two lanes: slots 1-4 and 5-8, each fed the same input, mixed by Lane Balance
                for (int c = 0; c < nch; ++c) lane.copyFrom (c, 0, io[c], n);
                juce::AudioBuffer<float> laneB (lane.getArrayOfWritePointers(), nch, n);
                for (int i = 0; i < 4; ++i) runSlot (i, view);
                for (int i = 4; i < numSlots; ++i) runSlot (i, laneB);
                const float bal = bank[laneBalance] / 100.0f, ga = std::sqrt (0.5f * (1.0f - bal)), gb = std::sqrt (0.5f * (1.0f + bal));
                bool anyB = false; for (int i = 4; i < numSlots; ++i) anyB = anyB || (slots[(size_t) i].plugin != nullptr && slots[(size_t) i].allowed);
                bool anyA = false; for (int i = 0; i < 4; ++i) anyA = anyA || (slots[(size_t) i].plugin != nullptr && slots[(size_t) i].allowed);
                if (anyA && anyB) for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) io[c][i] = ga * io[c][i] + gb * laneB.getSample (c, i);
                else if (anyB) for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) io[c][i] = laneB.getSample (c, i);
            }
            else for (int i = 0; i < numSlots; ++i) runSlot (i, view);
        }
        // the recycle limiter and the output
        float peak = 0;
        for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) { if (! std::isfinite (io[c][i])) io[c][i] = 0; peak = juce::jmax (peak, std::abs (io[c][i])); }
        const float want = peak * rc > 0.7f ? 0.7f / (peak * rc) : 1.0f;
        limG += (want < limG ? 0.5f : 0.02f) * (want - limG);
        for (int i = 0; i < n; ++i) { for (int c = 0; c < nch; ++c) recBuf.setSample (c, (recW + i) % recBuf.getNumSamples(), io[c][i]); }
        recW = (recW + n) % recBuf.getNumSamples();
        for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) { const float d = dryBuf.getSample (c, i); io[c][i] = (d + mx * (io[c][i] - d)) * outG; }
        float po = 0; for (int c = 0; c < nch; ++c) po = juce::jmax (po, b.getMagnitude (c, s0, n)); levelOut = po;
    }
    void flushPluginStateToValueTree() override
    {
        SqColor111bBase::flushPluginStateToValueTree();
        writeSlotsToState();
    }
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        restoring = true;
        SqColor111bBase::restorePluginStateFromValueTree (v);
        // the saved slots come with the state
        for (int i = state.getNumChildren(); --i >= 0;) if (state.getChild (i).hasType ("HIVESLOT")) state.removeChild (i, nullptr);
        for (int i = 0; i < v.getNumChildren(); ++i) if (v.getChild (i).hasType ("HIVESLOT")) state.appendChild (v.getChild (i).createCopy(), nullptr);
        rebuildFromState();
        restoring = false;
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && id == juce::Identifier ("chain") && ! restoring && (int) state.getProperty ("chain", -1) != loadedChain) loadChain ((int) state.getProperty ("chain", -1));
        if (v == state && id == juce::Identifier ("macroMap")) lastMacro.fill (-1.0f);
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    int loadedChain = -1;

private:
    struct Slot { te::Plugin::Ptr plugin; juce::String name, type, preset; std::atomic<bool> allowed { true }; };
    std::array<Slot, numSlots> slots;
    juce::CriticalSection slotLock;
    bool initialised = false, restoring = false;
    te::PluginInitialisationInfo lastInfo {};
    juce::AudioBuffer<float> lane, recBuf, dryBuf; int recW = 0; float limG = 1;
    std::array<float, 8> lastMacro { -1, -1, -1, -1, -1, -1, -1, -1 };

    void install (int i, te::Plugin::Ptr np, const juce::String& name, const juce::String& type, const juce::String& preset)
    {
        if (np != nullptr && initialised) np->baseClassInitialise (lastInfo);
        te::Plugin::Ptr old;
        {
            const juce::ScopedLock sl (slotLock);
            auto& s = slots[(size_t) i];
            old = s.plugin; s.plugin = np; s.name = name; s.type = type; s.preset = preset; s.allowed = type.isEmpty() || HiveHost::isTypeAllowed (type);
        }
        if (old != nullptr && initialised) old->baseClassDeinitialise();
        if (! restoring) writeSlotsToState();
        lastMacro.fill (-1.0f);
        changed();
    }
    void writeSlotsToState()
    {
        for (int i = state.getNumChildren(); --i >= 0;) if (state.getChild (i).hasType ("HIVESLOT")) state.removeChild (i, nullptr);
        for (int i = 0; i < numSlots; ++i)
        {
            const auto& s = slots[(size_t) i]; if (s.plugin == nullptr) continue;
            s.plugin->flushPluginStateToValueTree();
            juce::ValueTree t ("HIVESLOT"); t.setProperty ("index", i, nullptr); t.setProperty ("name", s.name, nullptr); t.setProperty ("preset", s.preset, nullptr);
            t.appendChild (s.plugin->state.createCopy(), nullptr);
            state.appendChild (t, nullptr);
        }
    }
    void rebuildFromState()
    {
        std::array<juce::ValueTree, numSlots> saved;
        for (int i = 0; i < state.getNumChildren(); ++i) { auto t = state.getChild (i); if (t.hasType ("HIVESLOT")) { const int k = (int) t["index"]; if (k >= 0 && k < numSlots) saved[(size_t) k] = t; } }
        const bool was = restoring; restoring = true;
        for (int i = 0; i < numSlots; ++i)
        {
            if (! saved[(size_t) i].isValid() || saved[(size_t) i].getNumChildren() == 0) { install (i, nullptr, {}, {}, {}); continue; }
            auto pt = saved[(size_t) i].getChild (0).createCopy();
            pt.removeProperty (te::IDs::id, nullptr);
            auto np = edit.engine.getPluginManager().createNewPlugin (edit, pt);
            if (np == nullptr) { install (i, nullptr, {}, {}, {}); continue; }
            install (i, np, saved[(size_t) i]["name"].toString(), np->getPluginType(), saved[(size_t) i]["preset"].toString());
        }
        restoring = was;
    }
    void timerCallback() override
    {
        // a licence activated (or removed) while the rack is open unlocks (or locks) its slots
        for (auto& sl : slots) sl.allowed = sl.type.isEmpty() || HiveHost::isTypeAllowed (sl.type);
        // macros: push each moved macro to its targets (message thread, with notifications so faces follow)
        const auto ts = macroTargets(); if (ts.empty()) return;
        for (int m = 0; m < 8; ++m)
        {
            const float v = bank[(size_t) (m1 + m)] / 100.0f;
            if (std::abs (v - lastMacro[(size_t) m]) < 1.0e-4f) continue;
            lastMacro[(size_t) m] = v;
            for (auto& t : ts)
                if (t.macro == m + 1)
                    if (auto* p = slotPlugin (t.slot - 1)) if (auto* ap = findParam (*p, t.param)) ap->setNormalisedParameter (juce::jlimit (0.0f, 1.0f, t.lo + (t.hi - t.lo) * v), juce::sendNotification);
        }
    }
};
