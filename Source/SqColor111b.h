#pragma once
// 1.11 Color & Space, second wave part 1: Vellum EQ (C12), Tremor (C8), Orbit Pan (C9), Mirror Shift + Lite (C14),
// Shutter Delay + Lite (C13). DSP: SqColor111bDsp.h (tests/color111b_test.cpp).

#include "SqColour.h"
#include "SqColor111bDsp.h"

/** Sixteen values kept as text in the state ("1,0.4,..."), parsed on the message thread, picked up by the audio thread. */
struct SqStepStore
{
    std::array<float, 16> pending {}; std::atomic<bool> fresh { true };
    static std::array<float, 16> parse (const juce::String& s, const float* def)
    {
        std::array<float, 16> v {}; for (int i = 0; i < 16; ++i) v[(size_t) i] = def[i];
        auto t = juce::StringArray::fromTokens (s, ",", "");
        for (int i = 0; i < juce::jmin (16, t.size()); ++i) if (t[i].trim().isNotEmpty()) v[(size_t) i] = juce::jlimit (-1.0f, 1.0f, t[i].getFloatValue());
        return v;
    }
    static juce::String toText (const float* v, int n = 16) { juce::StringArray a; for (int i = 0; i < n; ++i) a.add (juce::String (v[i], 3)); return a.joinIntoString (","); }
    void set (const juce::String& s, const float* def) { pending = parse (s, def); fresh = true; }
    void take (float* dst) { if (fresh.exchange (false)) std::copy (pending.begin(), pending.end(), dst); }
};

/** Shared plumbing for the five: transport, key input, meters. */
class SqColor111bBase : public SqColourBase
{
public:
    explicit SqColor111bBase (te::PluginCreationInfo info) : SqColourBase (info) {}
    double bpm = 120, ppq = 0; bool playing = false;
    void readTransport (const te::PluginRenderContext& fc)
    {
        const double b = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
        bpm = b > 1 ? b : 120.0; ppq = edit.tempoSequence.toBeats (fc.editTime.getStart()).inBeats(); playing = fc.isPlaying;
    }
    static juce::StringArray syncNames (bool withFree)
    {
        juce::StringArray a; if (withFree) a.add ("Free");
        for (int i = 0; i < Eng::WingMod::numSyncDivisions(); ++i) a.add (Eng::WingMod::syncName (i));
        return a;
    }
    static juce::StringArray shapeNames() { juce::StringArray a; for (int i = 0; i < Eng::WingMod::numShapes; ++i) a.add (Eng::WingMod::shapeName (i)); return a; }
protected:
    float sm[24] {}; bool smInit = false;
    void smooth (float* v, int n, bool snap)
    {
        if (snap || ! smInit) { std::copy (v, v + n, sm); smInit = true; }
        else for (int i = 0; i < n; ++i) sm[i] += 0.35f * (v[i] - sm[i]);
        std::copy (sm, sm + n, v);
    }
    template <typename Fn> void render (const te::PluginRenderContext& fc, bool keyOn, Fn&& fn)
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = keyOn && b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk;
        readTransport (fc);
        fn (ch, nch, haveKey ? key : nullptr, n);
        float po = 0; for (int c = 0; c < nch; ++c) po = juce::jmax (po, b.getMagnitude (c, s0, n)); levelOut = po;
    }
    void addKeyChannels (juce::StringArray* ins) { if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); } }
};

//==============================================================================
class SqVellumPlugin : public SqColor111bBase
{
public:
    enum P { lowGain, midGain, mid2Freq, mid2Gain, highGain, drive, output, numParams };
    SQ_COLOUR_IDENTITY (SqVellumPlugin, "Vellum EQ", "sqmVellum", "Vellum")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "lowGain",  "Low",        -15.0f, 15.0f,     0.0f, 1.0f, " dB", 1 },
            { "midGain",  "Mid",         -8.0f,  8.0f,     0.0f, 1.0f, " dB", 1 },
            { "mid2Freq", "Mid 2 Freq", 120.0f, 12000.0f, 3000.0f, 0.3f, " Hz", 0 },
            { "mid2Gain", "Mid 2",       -8.0f,  8.0f,     0.0f, 1.0f, " dB", 1 },
            { "highGain", "High",       -15.0f, 15.0f,     0.0f, 1.0f, " dB", 1 },
            { "drive",    "Drive",        0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "output",   "Output",     -24.0f, 12.0f,     0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray freqNames (const float* f) { juce::StringArray a; for (int i = 0; i < 6; ++i) a.add (f[i] >= 1000 ? juce::String (f[i] / 1000.0f, 1) + "k" : juce::String ((int) f[i])); return a; }
    static juce::StringArray targetNames() { juce::StringArray a; for (int i = 0; i < Vellum::numTargets; ++i) a.add (Vellum::targetName (i)); return a; }
    static juce::StringArray osNames() { return { "1x", "2x", "4x", "8x" }; }
    SqVellumPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        lowFreq.referTo (state, "lowFreq", um, 2); midFreq.referTo (state, "midFreq", um, 1); highFreq.referTo (state, "highFreq", um, 3);
        mid2On.referTo (state, "mid2On", um, false); stepped.referTo (state, "steppedOn", um, false); target.referTo (state, "target", um, 0); oversample.referTo (state, "oversample", um, 2);
    }
    ~SqVellumPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> lowFreq, midFreq, highFreq, target, oversample; juce::CachedValue<bool> mid2On, stepped;
    void resetProps (juce::UndoManager* um) override
    {
        lowFreq.setValue (2, um); midFreq.setValue (1, um); highFreq.setValue (3, um); mid2On.setValue (false, um); stepped.setValue (false, um); target.setValue (0, um); oversample.setValue (2, um);
    }
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); });
    }
    Vellum::Params knobParams() const { Vellum::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Vellum::Processor proc;
private:
    void fill (const float* v, Vellum::Params& p) const
    {
        p.lowIdx = juce::jlimit (0, 5, lowFreq.get()); p.midIdx = juce::jlimit (0, 5, midFreq.get()); p.highIdx = juce::jlimit (0, 5, highFreq.get());
        p.lowDb = v[lowGain]; p.midDb = v[midGain]; p.mid2Hz = v[mid2Freq]; p.mid2Db = v[mid2Gain]; p.highDb = v[highGain]; p.mid2On = mid2On.get();
        p.stepped = stepped.get(); p.drive = v[drive] / 100.0f; p.target = juce::jlimit (0, 4, target.get()); p.oversampleLog2 = juce::jlimit (0, 3, oversample.get()); p.outputDb = v[output];
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};

//==============================================================================
class SqTremorPlugin : public SqColor111bBase
{
public:
    enum P { depth, rate, feel, groove, accent, smoothing, width, threshold, attack, release, rateMod, rateModHz, drive, stereo, xover, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqTremorPlugin, "Tremor", "sqmTremor", "Tremor")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "depth",     "Depth",        0.0f, 100.0f,  60.0f, 1.0f, " %", 0 },
            { "rate",      "Rate",         0.1f, 40.0f,    4.0f, 0.35f, " Hz", 2 },
            { "feel",      "Feel",      -100.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "groove",    "Groove",       0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "accent",    "Accent",       0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "smoothing", "Smoothing",    0.1f, 50.0f,    2.0f, 0.4f, " ms", 1 },
            { "width",     "Pulse Width",  2.0f, 98.0f,   50.0f, 1.0f, " %", 0 },
            { "threshold", "Threshold",  -60.0f, 0.0f,   -30.0f, 1.0f, " dB", 1 },
            { "attack",    "Attack",       0.0f, 5000.0f, 10.0f, 0.25f, " ms", 1 },
            { "release",   "Release",      0.0f, 5000.0f, 200.0f, 0.25f, " ms", 0 },
            { "rateMod",   "Rate Mod",     0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "rateModHz", "Rate Mod Hz",  0.01f, 2000.0f, 5.0f, 0.2f, " Hz", 2 },
            { "drive",     "Drive",        0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "stereo",    "Stereo Phase", 0.0f, 180.0f,   0.0f, 1.0f, " deg", 0 },
            { "xover",     "Crossover",  100.0f, 5000.0f, 800.0f, 0.4f, " Hz", 0 },
            { "mix",       "Mix",          0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",    "Output",     -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray modeNames()   { return { "LFO", "Rhythm" }; }
    static juce::StringArray dynNames()    { return { "Off", "Env", "Gate", "Inverse" }; }
    static juce::StringArray analogNames() { juce::StringArray a { "Off" }; for (int i = 0; i < Eng::AnalogStyle::numVoicings; ++i) a.add (Eng::AnalogStyle::voicingName (i)); return a; }
    SqTremorPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        mode.referTo (state, "mode", um, 0); shape.referTo (state, "shape", um, 0); syncOn.referTo (state, "syncOn", um, true); division.referTo (state, "division", um, 9);
        stepDivision.referTo (state, "stepDivision", um, 6); numSteps.referTo (state, "numSteps", um, 16); dynMode.referTo (state, "dynMode", um, 0); analog.referTo (state, "analog", um, 0);
        harmonicOn.referTo (state, "harmonicOn", um, false); keyOn.referTo (state, "keyOn", um, false); retrigOn.referTo (state, "retrigOn", um, false);
        steps.referTo (state, "steps", um, {}); custom.referTo (state, "custom", um, {}); pattern.referTo (state, "pattern", um, 1);
    }
    ~SqTremorPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> mode, shape, division, stepDivision, numSteps, dynMode, analog, pattern; juce::CachedValue<bool> syncOn, harmonicOn, keyOn, retrigOn;
    static juce::StringArray patternNames() { juce::StringArray a; for (auto& p : Tremor::patterns()) a.add (p.name); return a; }
    juce::CachedValue<juce::String> steps, custom;
    void resetProps (juce::UndoManager* um) override
    {
        mode.setValue (0, um); shape.setValue (0, um); syncOn.setValue (true, um); division.setValue (9, um); stepDivision.setValue (6, um); numSteps.setValue (16, um);
        dynMode.setValue (0, um); analog.setValue (0, um); harmonicOn.setValue (false, um); keyOn.setValue (false, um); retrigOn.setValue (false, um); steps.setValue ({}, um); custom.setValue ({}, um); pattern.setValue (1, um);
    }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; reparse(); read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, keyOn.get(), [this] (float** ch, int nch, const float* const* key, int n) { proc.setTransport (bpm, ppq, playing); proc.process (ch, nch, key, n); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && (id == juce::Identifier ("steps") || id == juce::Identifier ("custom") || id == juce::Identifier ("pattern"))) reparse();
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    Tremor::Params knobParams() const { Tremor::Params p = proc.p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Tremor::Processor proc;
private:
    SqStepStore stepStore, customStore;
    void reparse()
    {
        const Tremor::Params d; const auto& pats = Tremor::patterns();
        stepStore.set (steps.get(), pats[(size_t) juce::jlimit (0, (int) pats.size() - 1, pattern.get())].v); customStore.set (custom.get(), d.custom);
    }
    void fill (const float* v, Tremor::Params& p) const
    {
        p.mode = juce::jlimit (0, 1, mode.get()); p.shape = juce::jlimit (0, (int) Eng::WingMod::numShapes - 1, shape.get());
        p.sync = syncOn.get(); p.syncIndex = juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, division.get()); p.rateHz = v[rate];
        p.stepDivision = juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, stepDivision.get()); p.numSteps = juce::jlimit (1, 16, numSteps.get());
        p.depth = v[depth] / 100.0f; p.feel = v[feel] / 100.0f; p.groove = v[groove] / 100.0f; p.accent = v[accent] / 100.0f; p.smoothMs = v[smoothing]; p.pulseWidth = v[width] / 100.0f;
        p.dyn = juce::jlimit (0, 3, dynMode.get()); p.thresholdDb = v[threshold]; p.attackMs = v[attack]; p.releaseMs = v[release];
        p.rateModAmt = v[rateMod] / 100.0f; p.rateModHz = v[rateModHz]; p.analog = juce::jlimit (0, (int) Eng::AnalogStyle::numVoicings, analog.get()); p.analogAmount = v[drive] / 100.0f;
        p.stereoDeg = v[stereo]; p.xoverHz = v[xover]; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
        p.harmonic = harmonicOn.get(); p.keyOn = keyOn.get(); p.retrigger = retrigOn.get();
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        fill (v, proc.p);
        stepStore.take (proc.p.steps); customStore.take (proc.p.custom);
        for (auto& s : proc.p.steps) s = juce::jlimit (0.0f, 1.0f, s);
    }
};

//==============================================================================
class SqOrbitPanPlugin : public SqColor111bBase
{
public:
    enum P { rate, offset, width, smoothing, depth, haas, threshold, detHp, detLp, sourceWidth, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqOrbitPanPlugin, "Orbit Pan", "sqmOrbitPan", "Orbit")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "rate",        "Rate",          0.05f, 20.0f,    0.5f, 0.35f, " Hz", 2 },
            { "offset",      "Offset",      -105.0f, 105.0f,   0.0f, 1.0f, " deg", 0 },
            { "width",       "Width",          0.0f, 210.0f, 180.0f, 1.0f, " deg", 0 },
            { "smoothing",   "Smoothing",      0.1f, 500.0f,  20.0f, 0.35f, " ms", 1 },
            { "depth",       "Depth",          0.0f, 100.0f,  70.0f, 1.0f, " %", 0 },
            { "haas",        "Haas",           0.0f, 2.0f,     0.6f, 1.0f, " ms", 2 },
            { "threshold",   "Threshold",    -60.0f, 0.0f,   -24.0f, 1.0f, " dB", 1 },
            { "detHp",       "Detector HP",   20.0f, 2000.0f, 20.0f, 0.35f, " Hz", 0 },
            { "detLp",       "Detector LP",  200.0f, 20000.0f, 20000.0f, 0.35f, " Hz", 0 },
            { "sourceWidth", "Source Width",   0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "mix",         "Mix",            0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",      "Output",       -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray modeNames() { juce::StringArray a; for (int i = 0; i < Orbit::numModes; ++i) a.add (Orbit::modeName (i)); return a; }
    static juce::StringArray lawNames()  { return { "Session", "0 dB", "-3 dB", "-4.5 dB", "-6 dB" }; }
    SqOrbitPanPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        mode.referTo (state, "mode", um, 0); shape.referTo (state, "shape", um, 0); syncOn.referTo (state, "syncOn", um, false); division.referTo (state, "division", um, 12);
        stepDivision.referTo (state, "stepDivision", um, 9); numSteps.referTo (state, "numSteps", um, 8); triggerOn.referTo (state, "triggerOn", um, false);
        divider.referTo (state, "divider", um, 1); keyOn.referTo (state, "keyOn", um, false); binauralOn.referTo (state, "binauralOn", um, false); panLaw.referTo (state, "panLaw", um, 0);
        steps.referTo (state, "steps", um, {}); pattern.referTo (state, "pattern", um, 0);
    }
    ~SqOrbitPanPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> mode, shape, division, stepDivision, numSteps, divider, panLaw, pattern; juce::CachedValue<bool> syncOn, triggerOn, keyOn, binauralOn;
    static juce::StringArray patternNames() { juce::StringArray a; for (auto& p : Orbit::patterns()) a.add (p.name); return a; }
    juce::CachedValue<juce::String> steps;
    void resetProps (juce::UndoManager* um) override
    {
        mode.setValue (0, um); shape.setValue (0, um); syncOn.setValue (false, um); division.setValue (12, um); stepDivision.setValue (9, um); numSteps.setValue (8, um);
        triggerOn.setValue (false, um); divider.setValue (1, um); keyOn.setValue (false, um); binauralOn.setValue (false, um); panLaw.setValue (0, um); steps.setValue ({}, um); pattern.setValue (0, um);
    }
    /** The law in use: the session's (the engine default pan law) unless one is chosen here. */
    static int lawFor (int choice)
    {
        if (choice > 0) return juce::jlimit (0, 3, choice - 1);
        switch (te::getDefaultPanLaw())
        {
            case te::PanLawLinear: return Orbit::law0dB;
            case te::PanLaw4point5dBCenter: return Orbit::law4p5dB;
            case te::PanLaw6dBCenter: return Orbit::law6dB;
            default: return Orbit::law3dB;
        }
    }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; reparse(); read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, keyOn.get(), [this] (float** ch, int nch, const float* const* key, int n) { proc.setTransport (bpm, ppq, playing); proc.process (ch, nch, key, n); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && (id == juce::Identifier ("steps") || id == juce::Identifier ("pattern"))) reparse();
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    Orbit::Params knobParams() const { Orbit::Params p = proc.p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Orbit::Processor proc;
private:
    SqStepStore stepStore;
    void reparse() { const auto& pats = Orbit::patterns(); stepStore.set (steps.get(), pats[(size_t) juce::jlimit (0, (int) pats.size() - 1, pattern.get())].v); }
    void fill (const float* v, Orbit::Params& p) const
    {
        p.mode = juce::jlimit (0, Orbit::numModes - 1, mode.get()); p.shape = juce::jlimit (0, (int) Eng::WingMod::numShapes - 1, shape.get());
        p.sync = syncOn.get(); p.syncIndex = juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, division.get());
        p.stepDivision = juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, stepDivision.get()); p.numSteps = juce::jlimit (1, 16, numSteps.get());
        p.rateHz = v[rate]; p.offsetDeg = v[offset]; p.widthDeg = v[width]; p.smoothMs = v[smoothing]; p.depth = v[depth] / 100.0f; p.haasMs = v[haas];
        p.trigThreshDb = v[threshold]; p.detHpHz = v[detHp]; p.detLpHz = v[detLp]; p.sourceWidth = v[sourceWidth] / 100.0f; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
        p.audioTrigger = triggerOn.get(); p.divider = juce::jlimit (1, 16, divider.get()); p.keyOn = keyOn.get(); p.binaural = binauralOn.get(); p.law = lawFor (panLaw.get());
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        fill (v, proc.p); stepStore.take (proc.p.steps);
    }
};

//==============================================================================
class SqMirrorShiftPlugin : public SqColor111bBase
{
public:
    enum P { detune, delay, focus, mix, duck, duckThresh, output, numParams };
    SQ_COLOUR_IDENTITY (SqMirrorShiftPlugin, "Mirror Shift", "sqmMirrorShift", "Mirror")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "detune",     "Detune",          0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "delay",      "Delay",           0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "focus",      "Focus",          20.0f, 10000.0f, 20.0f, 0.3f, " Hz", 0 },
            { "mix",        "Mix",             0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "duck",       "Duck",            0.0f, 24.0f,    0.0f, 1.0f, " dB", 1 },
            { "duckThresh", "Duck Threshold", -60.0f, 0.0f,  -30.0f, 1.0f, " dB", 1 },
            { "output",     "Output",        -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray styleNames (int n = Mirror::numStyles()) { juce::StringArray a; for (int i = 0; i < n; ++i) a.add (Mirror::style (i).name); return a; }
    SqMirrorShiftPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager(); style.referTo (state, "style", um, 0); keyOn.referTo (state, "keyOn", um, false);
    }
    ~SqMirrorShiftPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> style; juce::CachedValue<bool> keyOn;
    void resetProps (juce::UndoManager* um) override { style.setValue (0, um); keyOn.setValue (false, um); }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, keyOn.get(), [this] (float** ch, int nch, const float* const* key, int n) { proc.process (ch, nch, key, n); });
    }
    Mirror::Params knobParams() const { Mirror::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Mirror::Processor proc;
private:
    void fill (const float* v, Mirror::Params& p) const
    {
        p.style = juce::jlimit (0, Mirror::numStyles() - 1, style.get()); p.detune = v[detune] / 100.0f; p.delay = v[delay] / 100.0f; p.focusHz = v[focus];
        p.mix = v[mix] / 100.0f; p.duckDb = v[duck]; p.duckThreshDb = v[duckThresh]; p.outputDb = v[output]; p.keyOn = keyOn.get();
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};

class SqMirrorShiftLitePlugin : public SqColor111bBase
{
public:
    enum P { amount, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqMirrorShiftLitePlugin, "Mirror Shift Lite", "sqmMirrorShiftLite", "Mirror Lite")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "amount", "Amount",   0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "mix",    "Mix",      0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output", "Output", -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqMirrorShiftLitePlugin (te::PluginCreationInfo info) : SqColor111bBase (info) { bank.create (*this, paramSpecs()); style.referTo (state, "style", getUndoManager(), 0); }
    ~SqMirrorShiftLitePlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> style;
    void resetProps (juce::UndoManager* um) override { style.setValue (0, um); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, nullptr, n); }); }
    Mirror::Params knobParams() const { Mirror::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Mirror::Processor proc;
private:
    void fill (const float* v, Mirror::Params& p) const
    {
        p.style = juce::jlimit (0, 2, style.get()); p.detune = v[amount] / 100.0f; p.delay = v[amount] / 100.0f; p.focusHz = 20; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};

//==============================================================================
class SqShutterBase : public SqColor111bBase
{
public:
    explicit SqShutterBase (te::PluginCreationInfo info) : SqColor111bBase (info) {}
    bool producesAudioWhenNoAudioInput() override   { return true; }
    bool noTail() override                           { return false; }
    double getTailLength() const override
    {
        const double t = juce::jmax (proc.totalMs (0), proc.totalMs (1)) * 0.001, fb = juce::jlimit (0.0, 0.98, (double) juce::jmax (proc.p.feedbackA, proc.p.feedbackB));
        return juce::jlimit (0.1, 60.0, t * (fb > 0.01 ? std::log (0.001) / std::log (fb) : 1.0) + 0.5);
    }
    static juce::StringArray multiplyNames() { return { "1x", "2x", "4x", "8x" }; }
    Shutter::Processor proc;
    /** Re-reads the knobs into the processor's settings (reports and faces while the transport is stopped). */
    virtual void refreshSettings() = 0;
};

class SqShutterPlugin : public SqShutterBase
{
public:
    enum P { timeA, timeB, feedbackA, feedbackB, mix, lfoRate, lfoDepth, input, output, numParams };
    SQ_COLOUR_IDENTITY (SqShutterPlugin, "Shutter Delay", "sqmShutter", "Shutter")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "timeA",     "Time A",        1.0f, 1250.0f, 250.0f, 0.4f, " ms", 1 },
            { "timeB",     "Time B",        1.0f, 1250.0f, 375.0f, 0.4f, " ms", 1 },
            { "feedbackA", "Feedback A",    0.0f, 110.0f,  35.0f, 1.0f, " %", 0 },
            { "feedbackB", "Feedback B",    0.0f, 110.0f,  35.0f, 1.0f, " %", 0 },
            { "mix",       "Mix",           0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "lfoRate",   "LFO Rate",      0.1f, 256.0f,   0.5f, 0.25f, " Hz", 2 },
            { "lfoDepth",  "LFO Depth",     0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "input",     "Input",       -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 },
            { "output",    "Output",      -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray algoNames() { juce::StringArray a; for (int i = 0; i < Shutter::numAlgos; ++i) a.add (Shutter::algoName (i)); return a; }
    static juce::StringArray bitsNames() { return { "Clean", "12-bit", "8-bit" }; }
    SqShutterPlugin (te::PluginCreationInfo info) : SqShutterBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        multiply.referTo (state, "multiply", um, 0); algo.referTo (state, "algo", um, 0); bits.referTo (state, "bits", um, 1); syncA.referTo (state, "syncA", um, 0); syncB.referTo (state, "syncB", um, 0);
        freeze.referTo (state, "freezeOn", um, false); smoothOn.referTo (state, "smoothOn", um, true);
    }
    ~SqShutterPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> multiply, algo, bits, syncA, syncB; juce::CachedValue<bool> freeze, smoothOn;
    void resetProps (juce::UndoManager* um) override
    {
        multiply.setValue (0, um); algo.setValue (0, um); bits.setValue (1, um); syncA.setValue (0, um); syncB.setValue (0, um); freeze.setValue (false, um); smoothOn.setValue (true, um);
    }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.setTempo (bpm); proc.process (ch, nch, n); });
    }
    void refreshSettings() override { read (true); }
private:
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        auto& p = proc.p;
        p.timeAMs = v[timeA]; p.timeBMs = v[timeB]; p.feedbackA = v[feedbackA] / 100.0f; p.feedbackB = v[feedbackB] / 100.0f; p.mix = v[mix] / 100.0f;
        p.lfoHz = v[lfoRate]; p.lfoDepth = v[lfoDepth] / 100.0f; p.inputDb = v[input]; p.outputDb = v[output];
        p.multiply = juce::jlimit (0, 3, multiply.get()); p.algo = juce::jlimit (0, Shutter::numAlgos - 1, algo.get()); p.bits = juce::jlimit (0, 2, bits.get());
        p.syncA = syncA.get() - 1; p.syncB = syncB.get() - 1; p.freeze = freeze.get(); p.smooth = smoothOn.get();
    }
};

class SqShutterLitePlugin : public SqShutterBase
{
public:
    enum P { time, feedback, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqShutterLitePlugin, "Shutter Lite", "sqmShutterLite", "Shutter Lite")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "time",     "Time",       1.0f, 1250.0f, 250.0f, 0.4f, " ms", 1 },
            { "feedback", "Feedback",   0.0f, 110.0f,  35.0f, 1.0f, " %", 0 },
            { "mix",      "Mix",        0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "output",   "Output",   -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqShutterLitePlugin (te::PluginCreationInfo info) : SqShutterBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager(); multiply.referTo (state, "multiply", um, 0); freeze.referTo (state, "freezeOn", um, false);
    }
    ~SqShutterLitePlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> multiply; juce::CachedValue<bool> freeze;
    void resetProps (juce::UndoManager* um) override { multiply.setValue (0, um); freeze.setValue (false, um); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.setTempo (bpm); proc.process (ch, nch, n); }); }
    void refreshSettings() override { read (true); }
private:
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        auto& p = proc.p;
        p.algo = Shutter::classic; p.timeAMs = v[time]; p.timeBMs = v[time]; p.feedbackA = v[feedback] / 100.0f; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
        p.multiply = juce::jlimit (0, 3, multiply.get()); p.bits = 1; p.freeze = freeze.get(); p.smooth = true; p.lfoDepth = 0;
    }
};
