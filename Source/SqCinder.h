#pragma once
// Cinder (1.11 Color & Space, C1): ten-model saturation with oversampling, Model Blend, Bias, Dynamics, Mid/Side,
// sidechain-ducked drive, Delta listen and a live harmonic readout. The DSP is SqCinderDsp.h (measured in
// tests/cinder_test.cpp and the self-test).

#include "SqColour.h"
#include "SqCinderDsp.h"

class SqCinderPlugin : public SqColourBase
{
public:
    enum P { drive, blend, bias, dynamics, lowCut, thump, tone, highCut, output, mix, duck, numParams };
    SQ_COLOUR_IDENTITY (SqCinderPlugin, "Cinder", "sqmCinder", "Cinder")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "drive",    "Drive",       -12.0f, 36.0f,   6.0f, 1.0f, " dB", 1 },
            { "blend",    "Model Blend",   0.0f, 100.0f,  0.0f, 1.0f, " %", 0 },
            { "bias",     "Bias",       -100.0f, 100.0f,  0.0f, 1.0f, " %", 0 },
            { "dynamics", "Dynamics",      0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "lowCut",   "Low Cut",      20.0f, 1000.0f, 20.0f, 0.35f, " Hz", 0 },
            { "thump",    "Thump",         0.0f, 6.0f,    0.0f, 1.0f, " dB", 1 },
            { "tone",     "Tone",         -6.0f, 6.0f,    0.0f, 1.0f, " dB", 1 },
            { "highCut",  "High Cut",   1000.0f, 22000.0f, 22000.0f, 0.4f, " Hz", 0 },
            { "output",   "Output",      -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 },
            { "mix",      "Mix",           0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "duck",     "Duck Drive",    0.0f, 24.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray modelNames()  { juce::StringArray a; for (int m = 0; m < Cinder::numModels; ++m) a.add (Cinder::modelName (m)); return a; }
    static juce::StringArray msNames()     { return { "Stereo", "Mid/Side", "Mid only", "Side only" }; }
    static juce::StringArray osNames()     { return { "1x", "2x", "4x", "8x", "16x" }; }

    SqCinderPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        modelA.referTo (state, "model", um, (int) Cinder::triodeBox); modelB.referTo (state, "modelB", um, (int) Cinder::tapePreamp);
        punish.referTo (state, "punishOn", um, false); steep.referTo (state, "steepOn", um, false); autoGain.referTo (state, "autoGainOn", um, true);
        msMode.referTo (state, "msMode", um, 0); oversample.referTo (state, "oversample", um, 2); linearPhase.referTo (state, "linearPhaseOn", um, false);
        delta.referTo (state, "deltaOn", um, false); keyOn.referTo (state, "keyOn", um, false);
    }
    ~SqCinderPlugin() override { notifyListenersOfDeletion(); bank.detach(); }

    juce::CachedValue<int> modelA, modelB, msMode, oversample;
    juce::CachedValue<bool> punish, steep, autoGain, linearPhase, delta, keyOn;
    void resetProps (juce::UndoManager* um) override
    {
        modelA.setValue ((int) Cinder::triodeBox, um); modelB.setValue ((int) Cinder::tapePreamp, um);
        punish.setValue (false, um); steep.setValue (false, um); autoGain.setValue (true, um);
        msMode.setValue (0, um); oversample.setValue (2, um); linearPhase.setValue (false, um); delta.setValue (false, um); keyOn.setValue (false, um);
    }

    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override
    {
        te::Plugin::getChannelNames (ins, outs);
        if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); }
    }
    double getLatencySeconds() override   { return proc.latencySamples() / juce::jmax (1.0, sr); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        readParams (proc.p, true);
        proc.prepare (sr, juce::jmax (64, info.blockSizeSamples));
    }
    void deinitialise() override {}
    void reset() override   { proc.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        readParams (proc.p, false);
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = keyOn.get() && b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        float pk = 0;
        for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n));
        levelIn = pk;
        proc.process (ch, nch, haveKey ? key : nullptr, n);
        float po = 0;
        for (int c = 0; c < nch; ++c) po = juce::jmax (po, b.getMagnitude (c, s0, n));
        levelOut = po; activity = proc.driveActivity();
        autoGainDb = proc.autoGainDb();
    }

    /** Harmonic profile of the current settings (message thread): see Cinder::Processor::harmonics. */
    double harmonicProfile (float levelDb, float* h10) const
    {
        Cinder::Processor q; readParams (q.p, true);
        q.prepare (48000.0, 480);
        return q.harmonics (levelDb, h10);
    }
    std::atomic<float> autoGainDb { 0 };

private:
    Cinder::Processor proc;
    float sm[numParams] {}; bool smInit = false;

    void readParams (Cinder::Params& p, bool snap) const
    {
        auto* self = const_cast<SqCinderPlugin*> (this);
        float v[numParams];
        for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        // per-block smoothing of the continuous controls (about 30 ms), so automation never zips
        if (snap || ! smInit) { std::copy (v, v + numParams, self->sm); self->smInit = true; }
        else
        {
            const float k = 0.35f;
            for (int i = 0; i < numParams; ++i) self->sm[i] += k * (v[i] - self->sm[i]);
        }
        const float* s = sm;
        p.modelA = juce::jlimit (0, (int) Cinder::numModels - 1, modelA.get()); p.modelB = juce::jlimit (0, (int) Cinder::numModels - 1, modelB.get());
        p.blend = s[blend] / 100.0f; p.driveDb = s[drive]; p.biasAmt = s[bias] / 100.0f; p.dynamics = s[dynamics] / 100.0f;
        p.punish = punish.get(); p.lowCutHz = s[lowCut]; p.thumpDb = s[thump]; p.toneDb = s[tone]; p.highCutHz = s[highCut]; p.steep = steep.get();
        p.outputDb = s[output]; p.mix = s[mix] / 100.0f; p.autoGain = autoGain.get(); p.ms = juce::jlimit (0, 3, msMode.get());
        p.duckDb = s[duck]; p.keyOn = keyOn.get(); p.oversampleLog2 = juce::jlimit (0, 4, oversample.get()); p.linearPhase = linearPhase.get(); p.delta = delta.get();
    }
};
