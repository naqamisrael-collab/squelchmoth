#pragma once
// Radiant / Radiant Pre (C10) and Maw / Maw Lite (C11). DSP: SqRadiantMawDsp.h (tests/radiantmaw_test.cpp).

#include "SqColour.h"
#include "SqRadiantMawDsp.h"

/** Per-block smoothing of the continuous controls, shared by the four. */
struct SqSmoother
{
    float sm[16] {}; bool init = false;
    void apply (float* v, int n, bool snap)
    {
        if (snap || ! init) { std::copy (v, v + n, sm); init = true; }
        else for (int i = 0; i < n; ++i) sm[i] += 0.35f * (v[i] - sm[i]);
        std::copy (sm, sm + n, v);
    }
};

class SqRadiantPlugin : public SqColourBase
{
public:
    enum P { input, bass, treble, output, mix, trim, numParams };
    SQ_COLOUR_IDENTITY (SqRadiantPlugin, "Radiant", "sqmRadiant", "Radiant")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "input",  "Input",  -12.0f, 24.0f,   0.0f, 1.0f, " dB", 1 },
            { "bass",   "Bass",   -12.0f, 12.0f,   0.0f, 1.0f, " dB", 1 },
            { "treble", "Treble", -12.0f, 12.0f,   0.0f, 1.0f, " dB", 1 },
            { "output", "Output", -24.0f, 24.0f,   0.0f, 1.0f, " dB", 1 },
            { "mix",    "Mix",      0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "trim",   "Trim",   -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray archNames() { return { "60s Tube Mixer", "50s Broadcast Pre", "Tube Line Amp" }; }
    static juce::StringArray msNames()   { return { "Stereo", "Mid/Side", "Mid only", "Side only" }; }
    static juce::StringArray osNames()   { return { "1x", "2x", "4x", "8x" }; }
    SqRadiantPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        arch.referTo (state, "arch", um, 0); mic.referTo (state, "micOn", um, false); noisy.referTo (state, "noisyOn", um, false);
        oversample.referTo (state, "oversample", um, 2); ms.referTo (state, "msMode", um, 0);
    }
    ~SqRadiantPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> arch, oversample, ms; juce::CachedValue<bool> mic, noisy;
    void resetProps (juce::UndoManager* um) override { arch.setValue (0, um); mic.setValue (false, um); noisy.setValue (false, um); oversample.setValue (2, um); ms.setValue (0, um); }
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        read (false);
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        levelIn = b.getMagnitude (0, s0, n);
        proc.process (ch, nch, n);
        levelOut = b.getMagnitude (0, s0, n); heatDb = proc.heatDb();
    }
    double thd() { Radiant::Processor q; read (true, &q.p); return q.thd(); }
    Radiant::Processor proc; std::atomic<float> heatDb { -100 };
private:
    SqSmoother smo;
    void read (bool snap, Radiant::Params* target = nullptr)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        if (target == nullptr) smo.apply (v, numParams, snap);
        auto& p = target != nullptr ? *target : proc.p;
        p.arch = juce::jlimit (0, 2, arch.get()); p.singleStage = false; p.inputDb = v[input]; p.bassDb = v[bass]; p.trebleDb = v[treble]; p.outputDb = v[output];
        p.mix = v[mix] / 100.0f; p.trimDb = v[trim]; p.mic = mic.get(); p.noisy = noisy.get(); p.oversampleLog2 = juce::jlimit (0, 3, oversample.get()); p.ms = juce::jlimit (0, 3, ms.get());
    }
};

class SqRadiantPrePlugin : public SqColourBase
{
public:
    enum P { gain, output, mix, numParams };
    SQ_COLOUR_IDENTITY (SqRadiantPrePlugin, "Radiant Pre", "sqmRadiantPre", "Radiant Pre")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "gain",   "Gain",   -12.0f, 30.0f,   6.0f, 1.0f, " dB", 1 },
            { "output", "Output", -24.0f, 24.0f,   0.0f, 1.0f, " dB", 1 },
            { "mix",    "Mix",      0.0f, 100.0f, 100.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqRadiantPrePlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager(); bias.referTo (state, "biasOn", um, false); noisy.referTo (state, "noisyOn", um, false);
    }
    ~SqRadiantPrePlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<bool> bias, noisy;
    void resetProps (juce::UndoManager* um) override { bias.setValue (false, um); noisy.setValue (false, um); }
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        read (false);
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        levelIn = b.getMagnitude (0, s0, n); proc.process (ch, nch, n); levelOut = b.getMagnitude (0, s0, n); heatDb = proc.heatDb();
    }
    double thd() { Radiant::Processor q; read (true, &q.p); return q.thd(); }
    Radiant::Processor proc; std::atomic<float> heatDb { -100 };
private:
    SqSmoother smo;
    void read (bool snap, Radiant::Params* target = nullptr)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        if (target == nullptr) smo.apply (v, numParams, snap);
        auto& p = target != nullptr ? *target : proc.p;
        p.arch = Radiant::preamp; p.singleStage = true; p.inputDb = v[gain]; p.outputDb = 0; p.trimDb = v[output]; p.mix = v[mix] / 100.0f;
        p.bias = bias.get(); p.noisy = noisy.get(); p.oversampleLog2 = 2; p.ms = 0;
    }
};

//==============================================================================
class SqMawBase : public SqColourBase
{
public:
    explicit SqMawBase (te::PluginCreationInfo info) : SqColourBase (info) {}
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    Maw::Processor proc; std::atomic<float> grDb { 0 };
protected:
    void run (const te::PluginRenderContext& fc, bool keyNow)
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = keyNow && b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        levelIn = b.getMagnitude (0, s0, n); proc.process (ch, nch, haveKey ? key : nullptr, n); levelOut = b.getMagnitude (0, s0, n); grDb = proc.gainReductionDb();
    }
};

class SqMawPlugin : public SqMawBase
{
public:
    enum P { crush, crunch, darkness, mix, release, stretch, keyHpf, output, numParams };
    SQ_COLOUR_IDENTITY (SqMawPlugin, "Maw", "sqmMaw", "Maw")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "crush",    "Crush",      0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "crunch",   "Crunch",     0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "darkness", "Darkness", 1000.0f, 20000.0f, 20000.0f, 0.4f, " Hz", 0 },
            { "mix",      "Mix",        0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "release",  "Release",   0.05f, 30.0f,   1.7f, 0.3f, " s", 2 },
            { "stretch",  "Stretch",    0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "keyHpf",   "Key HPF",   20.0f, 500.0f,  20.0f, 0.5f, " Hz", 0 },
            { "output",   "Output",   -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqMawPlugin (te::PluginCreationInfo info) : SqMawBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager(); fast.referTo (state, "fastOn", um, false); keyOn.referTo (state, "keyOn", um, false); oversample.referTo (state, "oversample", um, 2);
    }
    ~SqMawPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<bool> fast, keyOn; juce::CachedValue<int> oversample;
    void resetProps (juce::UndoManager* um) override { fast.setValue (false, um); keyOn.setValue (false, um); oversample.setValue (2, um); }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); } }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); run (fc, keyOn.get()); }
private:
    SqSmoother smo;
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smo.apply (v, numParams, snap);
        auto& p = proc.p;
        p.crush = v[crush] / 100.0f; p.crunch = v[crunch] / 100.0f; p.darknessHz = v[darkness]; p.mix = v[mix] / 100.0f; p.releaseS = v[release];
        p.stretch = v[stretch] / 100.0f; p.keyHpfHz = v[keyHpf]; p.outputDb = v[output]; p.fastRelease = fast.get(); p.keyOn = keyOn.get(); p.oversampleLog2 = juce::jlimit (0, 3, oversample.get());
    }
};

class SqMawLitePlugin : public SqMawBase
{
public:
    enum P { crush, crunch, output, numParams };
    SQ_COLOUR_IDENTITY (SqMawLitePlugin, "Maw Lite", "sqmMawLite", "Maw Lite")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "crush",  "Crush",    0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "crunch", "Crunch",   0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "output", "Output", -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqMawLitePlugin (te::PluginCreationInfo info) : SqMawBase (info) { bank.create (*this, paramSpecs()); }
    ~SqMawLitePlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); run (fc, false); }
private:
    SqSmoother smo;
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smo.apply (v, numParams, snap);
        auto& p = proc.p; p.crush = v[crush] / 100.0f; p.crunch = v[crunch] / 100.0f; p.outputDb = v[output];
    }
};
