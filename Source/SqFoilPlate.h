#pragma once
// Foil Plate and Foil Plate Lite (1.11 Color & Space, C4). DSP: SqFoilPlateDsp.h (tests/foilplate_test.cpp).

#include "SqColour.h"
#include "SqFoilPlateDsp.h"

class SqFoilPlateBase : public SqColourBase
{
public:
    explicit SqFoilPlateBase (te::PluginCreationInfo info) : SqColourBase (info) {}
    bool producesAudioWhenNoAudioInput() override   { return true; }
    bool noTail() override                           { return false; }
    double getTailLength() const override            { return proc.p.freeze ? 60.0 : juce::jlimit (0.2, 60.0, (double) proc.p.decayS * 1.6 + 0.3); }
    FoilPlate::Processor proc;
    std::atomic<float> liveDecayMul { 1 };
protected:
    float sm[24] {}; bool smInit = false;
    void smooth (float* v, int n, bool snap)
    {
        if (snap || ! smInit) { std::copy (v, v + n, sm); smInit = true; }
        else for (int i = 0; i < n; ++i) sm[i] += 0.35f * (v[i] - sm[i]);
        std::copy (sm, sm + n, v);
    }
    void render (const te::PluginRenderContext& fc, bool keyNow)
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = keyNow && b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk;
        proc.process (ch, nch, haveKey ? key : nullptr, n);
        float po = 0; for (int c = 0; c < nch; ++c) po = juce::jmax (po, b.getMagnitude (c, s0, n)); levelOut = po;
        liveDecayMul = proc.autoDecayMul();
    }
};

//==============================================================================
class SqFoilPlatePlugin : public SqFoilPlateBase
{
public:
    enum P { decay, preDelay, lowCut, highCut, mod, modRate, mix, width, balance, eqLow, eqHigh, autoDecay, autoThreshold, drive, output, numParams };
    SQ_COLOUR_IDENTITY (SqFoilPlatePlugin, "Foil Plate", "sqmFoilPlate", "Foil Plate")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "decay",         "Decay",          0.3f, 60.0f,   2.0f, 0.3f, " s", 2 },
            { "preDelay",      "Pre-Delay",      0.0f, 250.0f, 10.0f, 0.5f, " ms", 1 },
            { "lowCut",        "Low Cut",       20.0f, 1000.0f, 80.0f, 0.35f, " Hz", 0 },
            { "highCut",       "High Cut",    1000.0f, 20000.0f, 12000.0f, 0.4f, " Hz", 0 },
            { "mod",           "Mod",            0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "modRate",       "Mod Rate",       0.2f, 8.0f,    0.8f, 0.5f, " Hz", 2 },
            { "mix",           "Mix",            0.0f, 100.0f,  25.0f, 1.0f, " %", 0 },
            { "width",         "Width",          0.0f, 150.0f, 100.0f, 1.0f, " %", 0 },
            { "balance",       "Balance",     -100.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "eqLow",         "Low EQ",       -12.0f, 12.0f,    0.0f, 1.0f, " dB", 1 },
            { "eqHigh",        "High EQ",      -12.0f, 12.0f,    0.0f, 1.0f, " dB", 1 },
            { "autoDecay",     "Auto Decay",  -100.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "autoThreshold", "Auto Threshold", -60.0f, 0.0f, -30.0f, 1.0f, " dB", 1 },
            { "drive",         "Input Drive",    0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "output",        "Output",       -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray archetypeNames() { juce::StringArray a; for (int i = 0; i < FoilPlate::numArchetypes(); ++i) a.add (FoilPlate::archetype (i).name); return a; }
    static juce::StringArray inputNames()     { return { "Tube", "Solid State", "Clean", "Tape" }; }
    static juce::StringArray slopeNames()     { return { "6 dB/oct", "12 dB/oct", "24 dB/oct" }; }

    SqFoilPlatePlugin (te::PluginCreationInfo info) : SqFoilPlateBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        plate.referTo (state, "plate", um, 0); input.referTo (state, "input", um, 0); lowSlope.referTo (state, "lowSlope", um, 1); highSlope.referTo (state, "highSlope", um, 1);
        freeze.referTo (state, "freezeOn", um, false); darkening.referTo (state, "darkeningOn", um, false); monoIn.referTo (state, "monoInOn", um, false); keyOn.referTo (state, "keyOn", um, false);
    }
    ~SqFoilPlatePlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> plate, input, lowSlope, highSlope;
    juce::CachedValue<bool> freeze, darkening, monoIn, keyOn;
    void resetProps (juce::UndoManager* um) override
    {
        plate.setValue (0, um); input.setValue (0, um); lowSlope.setValue (1, um); highSlope.setValue (1, um);
        freeze.setValue (false, um); darkening.setValue (false, um); monoIn.setValue (false, um); keyOn.setValue (false, um);
    }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override
    {
        te::Plugin::getChannelNames (ins, outs);
        if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); }
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        readParams (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); readParams (true); proc.configure (true);
    }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { readParams (false); render (fc, keyOn.get()); }
    /** Input stage THD at -18 dBFS for the current settings (message thread). */
    double inputThd() { FoilPlate::Processor q; q.p = proc.p; readInto (q.p, true); return q.inputThd(); }

private:
    void readParams (bool snap) { readInto (proc.p, snap); }
    void readInto (FoilPlate::Params& p, bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        if (&p == &proc.p) smooth (v, numParams, snap);
        p.archetype = juce::jlimit (0, FoilPlate::numArchetypes() - 1, plate.get()); p.input = juce::jlimit (0, 3, input.get());
        p.lowSlope = juce::jlimit (0, 2, lowSlope.get()); p.highSlope = juce::jlimit (0, 2, highSlope.get());
        p.decayS = v[decay]; p.preDelayMs = v[preDelay]; p.lowCut = v[lowCut]; p.highCut = v[highCut]; p.mod = v[mod] / 100.0f; p.modRateHz = v[modRate];
        p.mix = v[mix] / 100.0f; p.width = v[width] / 100.0f; p.balance = v[balance] / 100.0f; p.eqLowDb = v[eqLow]; p.eqHighDb = v[eqHigh];
        p.autoDecay = v[autoDecay] / 100.0f; p.autoThresholdDb = v[autoThreshold]; p.inputDrive = v[drive] / 100.0f; p.outputDb = v[output];
        p.freeze = freeze.get(); p.darkening = darkening.get(); p.monoIn = monoIn.get(); p.keyOn = keyOn.get();
    }
};

//==============================================================================
/** Foil Plate Lite: Steel Classic only - Decay, Low Cut, Mod, Mix. The Demo's plate. */
class SqFoilPlateLitePlugin : public SqFoilPlateBase
{
public:
    enum P { decay, lowCut, mod, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqFoilPlateLitePlugin, "Foil Plate Lite", "sqmFoilPlateLite", "Plate Lite")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "decay",  "Decay",    0.3f, 10.0f,  1.8f, 0.4f, " s", 2 },
            { "lowCut", "Low Cut", 20.0f, 1000.0f, 100.0f, 0.35f, " Hz", 0 },
            { "mod",    "Mod",      0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "mix",    "Mix",      0.0f, 100.0f, 25.0f, 1.0f, " %", 0 },
            { "output", "Output", -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqFoilPlateLitePlugin (te::PluginCreationInfo info) : SqFoilPlateBase (info) { bank.create (*this, paramSpecs()); }
    ~SqFoilPlateLitePlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        readParams (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); readParams (true); proc.configure (true);
    }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { readParams (false); render (fc, false); }
private:
    void readParams (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smooth (v, numParams, snap);
        auto& p = proc.p;
        p.archetype = 0; p.input = FoilPlate::tube; p.decayS = v[decay]; p.lowCut = v[lowCut]; p.mod = v[mod] / 100.0f; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
        p.highCut = 12000; p.preDelayMs = 8; p.modRateHz = 0.8f;
    }
};
