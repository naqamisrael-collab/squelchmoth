#pragma once
// Sibyl De-Ess (1.14 "Studio Line", phase F): the de-esser flagship. DSP: SqSibylDsp.h (tests/sibyl_test.cpp).
// Face: SkinsSibyl.inl. Claude: `sibyl_deess` (McpToolsSibyl.h).

#include "SqColour.h"
#include "SqSibylDsp.h"

class SqSibylDeEssPlugin : public SqColourBase, private juce::AsyncUpdater
{
public:
    enum P { threshold, range, knee, release, lookahead, low, high, output, numParams };
    SQ_COLOUR_IDENTITY (SqSibylDeEssPlugin, "Sibyl De-Ess", "sqmSibylDeEss", "Sibyl")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "threshold", "Threshold", -60.0f, 0.0f, -20.0f, 1.0f, " dB", 1 },
            { "range",     "Range",       0.0f, 24.0f, 10.0f, 1.0f, " dB", 1 },
            { "knee",      "Knee",        0.0f, 18.0f, 6.0f, 1.0f, " dB", 1 },
            { "release",   "Release",     5.0f, 500.0f, 60.0f, 0.4f, " ms", 0 },
            { "lookahead", "Lookahead",   0.0f, 15.0f, 2.0f, 0.6f, " ms", 1 },
            { "low",       "Low",      1000.0f, 16000.0f, 5000.0f, 0.4f, " Hz", 0 },
            { "high",      "High",     1500.0f, 20000.0f, 10000.0f, 0.4f, " Hz", 0 },
            { "output",    "Output",    -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray shapeNames()  { return { "Split", "Wide" }; }
    static juce::StringArray detectNames() { return { "Absolute", "Relative" }; }
    static juce::StringArray stereoNames() { return { "Linked", "Mid", "Side" }; }
    static juce::StringArray listenNames() { return { "Output", "Detection", "Removed" }; }

    juce::CachedValue<int> shape, detect, stereoMode, listen;
    juce::CachedValue<bool> track, sidechain;

    SqSibylDeEssPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        shape.referTo (state, "shape", um, 0); detect.referTo (state, "detect", um, 1); stereoMode.referTo (state, "stereo", um, 0); listen.referTo (state, "listen", um, 0);
        track.referTo (state, "track", um, false); sidechain.referTo (state, "sidechain", um, false);
        histDet.fill (-100.0f); histGr.fill (0.0f);
    }
    ~SqSibylDeEssPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        shape.setValue (0, um); detect.setValue (1, um); stereoMode.setValue (0, um); listen.setValue (0, um); track.setValue (false, um); sidechain.setValue (false, um);
    }

    double getLatencySeconds() override { return (double) std::lround (juce::jlimit (0.0f, 15.0f, bank[lookahead]) * 0.001 * sr) / juce::jmax (1.0, sr); }
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
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        int sw = scopeW.load();
        for (int i = 0; i < n; ++i) scope[(size_t) ((sw + i) & (scopeLen - 1))] = 0.5f * (ch[0][i] + ch[1][i]);
        scopeW = sw + n;
        engine.process (ch, nch, haveKey ? key : nullptr, haveKey ? 2 : 0, n);
        const int w = histW.load();
        histDet[(size_t) (w & (histLen - 1))] = engine.meterDetDb; histGr[(size_t) (w & (histLen - 1))] = engine.meterGrDb;
        histW = w + 1;
        grNow = engine.meterGrDb; detNow = engine.meterDetDb; trackHz = engine.meterTrackHz;
    }

    /** The cut's shape at f for a given gain change (dB) - the face draws the real thing. */
    double cutShapeDb (double f, double grDb) const { SqSibyl::Engine e; fill (e.p); e.meterTrackHz = trackHz.load(); return e.cutShapeDb (f, grDb); }
    double sampleRate() const { return sr; }

    static constexpr int histLen = 512, scopeLen = 4096;
    std::array<float, histLen> histDet {}, histGr {};
    std::array<float, scopeLen> scope {};
    std::atomic<int> histW { 0 }, scopeW { 0 };
    std::atomic<float> grNow { 0 }, detNow { -100 }, trackHz { 7000 };

private:
    SqSibyl::Engine engine; int lastLatency = 0;
    void fill (SqSibyl::Params& p) const
    {
        p.thresholdDb = bank[threshold]; p.rangeDb = bank[range]; p.kneeDb = bank[knee]; p.releaseMs = bank[release]; p.lookaheadMs = bank[lookahead];
        p.lowHz = bank[low]; p.highHz = juce::jmax (bank[low] * 1.05f, bank[high]); p.outputDb = bank[output];
        p.shape = juce::jlimit (0, 1, shape.get()); p.detect = juce::jlimit (0, 1, detect.get()); p.stereo = juce::jlimit (0, 2, stereoMode.get());
        p.listen = juce::jlimit (0, 2, listen.get()); p.track = track.get(); p.sidechain = sidechain.get();
    }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
};
