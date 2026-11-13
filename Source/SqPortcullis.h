#pragma once
// Portcullis Gate (1.14 "Studio Line", phase G): the gate / expander / ducker flagship. DSP: SqPortcullisDsp.h
// (tests/portcullis_test.cpp). Face: SkinsPortcullis.inl. Claude: `portcullis_gate` (McpToolsPortcullis.h).

#include "SqColour.h"
#include "SqPortcullisDsp.h"

class SqPortcullisGatePlugin : public SqColourBase, private juce::AsyncUpdater
{
public:
    enum P { threshold, hysteresis, range, ratio, attack, hold, release, lookahead, keyHp, keyLp, output, numParams };
    SQ_COLOUR_IDENTITY (SqPortcullisGatePlugin, "Portcullis Gate", "sqmPortcullis", "Portcullis")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "threshold",  "Threshold",  -80.0f, 0.0f, -40.0f, 1.0f, " dB", 1 },
            { "hysteresis", "Hysteresis",   0.0f, 24.0f, 6.0f, 1.0f, " dB", 1 },
            { "range",      "Range",        0.0f, 100.0f, 60.0f, 1.0f, " dB", 0 },
            { "ratio",      "Ratio",        1.0f, 10.0f, 3.0f, 0.5f, ":1", 1 },
            { "attack",     "Attack",       0.01f, 100.0f, 0.5f, 0.3f, " ms", 2 },
            { "hold",       "Hold",         0.0f, 1000.0f, 30.0f, 0.4f, " ms", 0 },
            { "release",    "Release",      1.0f, 3000.0f, 120.0f, 0.35f, " ms", 0 },
            { "lookahead",  "Lookahead",    0.0f, 10.0f, 0.0f, 0.6f, " ms", 1 },
            { "keyHp",      "Key High-Pass", 20.0f, 4000.0f, 20.0f, 0.3f, " Hz", 0 },
            { "keyLp",      "Key Low-Pass", 200.0f, 20000.0f, 20000.0f, 0.3f, " Hz", 0 },
            { "output",     "Output",     -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray modeNames()  { return { "Gate", "Expand", "Duck" }; }
    static juce::StringArray keyNames()   { return { "Input", "Side-Chain", "MIDI" }; }
    static juce::StringArray curveNames() { return { "Linear", "Smooth", "Punchy" }; }

    juce::CachedValue<int> mode, keySource, curve;
    juce::CachedValue<bool> listenKey, flip, velocity;

    SqPortcullisGatePlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        mode.referTo (state, "mode", um, 0); keySource.referTo (state, "key", um, 0); curve.referTo (state, "curve", um, 1);
        listenKey.referTo (state, "listenKey", um, false); flip.referTo (state, "flip", um, false); velocity.referTo (state, "velocity", um, false);
        histKey.fill (-100.0f); histGain.fill (0.0f); histIn.fill (-100.0f);
    }
    ~SqPortcullisGatePlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        mode.setValue (0, um); keySource.setValue (0, um); curve.setValue (1, um); listenKey.setValue (false, um); flip.setValue (false, um); velocity.setValue (false, um);
    }
    bool takesMidiInput() override { return true; }

    double getLatencySeconds() override { return (double) std::lround (juce::jlimit (0.0f, 10.0f, bank[lookahead]) * 0.001 * sr) / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        fill (engine.p); engine.prepare (sr, juce::jmax (64, info.blockSizeSamples)); lastLatency = engine.latency();
    }
    void deinitialise() override {}
    void reset() override { engine.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        fill (engine.p);
        if (engine.latency() != lastLatency) { lastLatency = engine.latency(); triggerAsyncUpdate(); }
        if (fc.bufferForMidiMessages != nullptr)
            for (auto& m : *fc.bufferForMidiMessages)
            {
                const int at = juce::jlimit (0, fc.bufferNumSamples - 1, (int) std::lround ((m.getTimeStamp() - fc.midiBufferOffset) * sr));
                if (m.isNoteOn()) engine.noteOn (at, m.getFloatVelocity());
                else if (m.isNoteOff()) engine.noteOff (at);
            }
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        float inPk = 0; for (int c = 0; c < nch; ++c) inPk = juce::jmax (inPk, b.getMagnitude (c, s0, n));
        engine.process (ch, nch, haveKey ? key : nullptr, haveKey ? 2 : 0, n);
        const int w = histW.load();
        histKey[(size_t) (w & (histLen - 1))] = engine.meterKeyDb; histGain[(size_t) (w & (histLen - 1))] = engine.meterGainDb;
        histIn[(size_t) (w & (histLen - 1))] = (float) SqPortcullis::gainToDb (inPk);
        histW = w + 1;
        keyNow = engine.meterKeyDb; gainNow = engine.meterGainDb; openNow = engine.meterOpen;
    }

    /** The settled curve, for the face's transfer graph. */
    double curveDb (double x) const { SqPortcullis::Engine e; fill (e.p); return e.curveDb (x); }

    static constexpr int histLen = 512;
    std::array<float, histLen> histKey {}, histGain {}, histIn {};
    std::atomic<int> histW { 0 };
    std::atomic<float> keyNow { -100 }, gainNow { 0 }, openNow { 0 };

private:
    SqPortcullis::Engine engine; int lastLatency = 0;
    void fill (SqPortcullis::Params& p) const
    {
        p.thresholdDb = bank[threshold]; p.hysteresisDb = bank[hysteresis]; p.rangeDb = bank[range]; p.ratio = bank[ratio];
        p.attackMs = bank[attack]; p.holdMs = bank[hold]; p.releaseMs = bank[release]; p.lookaheadMs = bank[lookahead];
        p.keyHpHz = bank[keyHp]; p.keyLpHz = bank[keyLp]; p.outputDb = bank[output];
        p.mode = juce::jlimit (0, 2, mode.get()); p.key = juce::jlimit (0, 2, keySource.get()); p.curve = juce::jlimit (0, 2, curve.get());
        p.listenKey = listenKey.get(); p.flip = flip.get(); p.velocity = velocity.get();
    }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
};
