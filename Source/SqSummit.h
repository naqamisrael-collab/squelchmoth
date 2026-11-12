#pragma once
// Summit Limiter (1.14 "Studio Line", phase C): the flagship true-peak limiter. DSP: SqSummitDsp.h (tests/summit_test.cpp).
// Face: SkinsSummit.inl. Claude: `summit_limiter` (McpToolsSummit.h). Loudness: the Apex series' BS.1770-4 meter.

#include "SqColour.h"
#include "SqSummitDsp.h"
#include "SqApexDsp.h"

class SqSummitLimiterPlugin : public SqColourBase, private juce::AsyncUpdater
{
public:
    enum P { input, ceiling, release, lookahead, transientLink, releaseLink, numParams };
    SQ_COLOUR_IDENTITY (SqSummitLimiterPlugin, "Summit Limiter", "sqmSummitLim", "Summit")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "input",         "Input",          -12.0f, 30.0f,  0.0f, 1.0f, " dB", 1 },
            { "ceiling",       "Ceiling",        -12.0f, 0.0f,  -1.0f, 1.0f, " dBTP", 2 },
            { "release",       "Release",         25.0f, 400.0f, 100.0f, 0.5f, " %", 0 },
            { "lookahead",     "Lookahead",       0.1f, 10.0f,   2.0f, 0.5f, " ms", 2 },
            { "transientLink", "Transient Link",  0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "releaseLink",   "Release Link",    0.0f, 100.0f, 100.0f, 1.0f, " %", 0 } };
        return s;
    }

    /** Loudness targets: what the meter compares the integrated loudness with (generic, platform-neutral names). */
    struct Target { const char* name; float lufs, tp; };
    static const std::vector<Target>& targets()
    {
        static const std::vector<Target> t { { "Streaming (-14 LUFS, -1 dBTP)", -14, -1 }, { "Streaming quiet (-16 LUFS, -1 dBTP)", -16, -1 },
                                             { "Podcast (-16 LUFS, -1 dBTP)", -16, -1 }, { "Broadcast EU (-23 LUFS, -1 dBTP)", -23, -1 },
                                             { "Broadcast US (-24 LKFS, -2 dBTP)", -24, -2 }, { "Club (-8 LUFS, -0.3 dBTP)", -8, -0.3f }, { "Loud master (-9 LUFS, -0.1 dBTP)", -9, -0.1f } };
        return t;
    }

    juce::CachedValue<int> algorithm, oversample, ditherBits, noiseShape, target;
    juce::CachedValue<bool> truePeak, bandLimit, dcFilter, unity, listenRemoved, linkOwn;

    SqSummitLimiterPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        algorithm.referTo (state, "algorithm", um, 0); oversample.referTo (state, "oversample", um, 0); ditherBits.referTo (state, "ditherBits", um, 0);
        noiseShape.referTo (state, "noiseShape", um, 0); target.referTo (state, "target", um, 0);
        truePeak.referTo (state, "truePeak", um, true); bandLimit.referTo (state, "bandLimit", um, true); dcFilter.referTo (state, "dcFilter", um, true);
        unity.referTo (state, "unity", um, false); listenRemoved.referTo (state, "listenRemoved", um, false); linkOwn.referTo (state, "linkOwn", um, true);
    }
    ~SqSummitLimiterPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        algorithm.setValue (0, um); oversample.setValue (0, um); ditherBits.setValue (0, um); noiseShape.setValue (0, um); target.setValue (0, um);
        truePeak.setValue (true, um); bandLimit.setValue (true, um); dcFilter.setValue (true, um); unity.setValue (false, um); listenRemoved.setValue (false, um); linkOwn.setValue (true, um);
    }
    static juce::StringArray algorithmNames() { juce::StringArray a; for (auto& x : SqSummit::algorithms()) a.add (x.name); return a; }
    static juce::StringArray oversampleNames() { return { "Off", "2x", "4x", "8x", "16x", "32x" }; }
    static juce::StringArray ditherNames() { return { "Off", "16 bit", "20 bit", "24 bit" }; }
    static int ditherBitsFor (int i) { static const int b[4] = { 0, 16, 20, 24 }; return b[juce::jlimit (0, 3, i)]; }
    static juce::StringArray shapeNames() { return { "No shaping", "Gentle", "Strong" }; }

    double getLatencySeconds() override
    {
        if (! probeReady) { probe.prepare (sr, 64); probeReady = true; }
        fill (probe.p); return (double) probe.latency() / juce::jmax (1.0, sr);
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        fill (engine.p); engine.prepare (sr, juce::jmax (64, info.blockSizeSamples));
        probe.prepare (sr, 64); probeReady = true;
        loud.prepare (sr); lastLatency = engine.latency(); tpMax = -100;
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
        float inPk = 0; for (int c = 0; c < nch; ++c) inPk = juce::jmax (inPk, b.getMagnitude (c, s0, n));
        engine.process (ch, nch, n);
        for (int s = 0; s < n; ++s) loud.process (ch[0][s], nch > 1 ? ch[1][s] : ch[0][s]);
        float outPk = 0; for (int c = 0; c < nch; ++c) outPk = juce::jmax (outPk, b.getMagnitude (c, s0, n));
        levelIn = inPk; levelOut = outPk;
        grNow = engine.meterGrDb; tpNow = engine.meterOutTpDb; tpMax = juce::jmax (tpMax.load(), engine.meterOutTpDb);
        momentary = loud.getMomentary(); shortTerm = loud.getShortTerm(); integrated = loud.getIntegrated(); lra = loud.getRange();
        const int w = histW.load();
        histIn[(size_t) (w & (histLen - 1))] = (float) SqSummit::gainToDb (inPk * SqSummit::dbToGain (bank[input]));
        histOut[(size_t) (w & (histLen - 1))] = (float) SqSummit::gainToDb (outPk);
        histGr[(size_t) (w & (histLen - 1))] = engine.meterGrDb;
        histW = w + 1;
    }
    void resetMeters() { loud.reset(); tpMax = -100; }

    static constexpr int histLen = 1024;
    std::array<float, histLen> histIn {}, histOut {}, histGr {};
    std::atomic<int> histW { 0 };
    std::atomic<float> grNow { 0 }, tpNow { -100 }, tpMax { -100 }, momentary { -100 }, shortTerm { -100 }, integrated { -100 }, lra { 0 };

private:
    SqSummit::Engine engine, probe;
    Apex::Loudness loud;
    int lastLatency = 0; bool probeReady = false;
    void fill (SqSummit::Params& p) const
    {
        p.algorithm = juce::jlimit (0, 9, algorithm.get()); p.inputDb = bank[input]; p.ceilingDb = bank[ceiling]; p.releaseScale = bank[release] / 100.0;
        p.lookaheadMs = bank[lookahead]; p.truePeak = truePeak.get(); p.bandLimit = bandLimit.get(); p.dcFilter = dcFilter.get();
        p.transientLink = linkOwn.get() ? -1.0 : bank[transientLink] / 100.0; p.releaseLink = linkOwn.get() ? -1.0 : bank[releaseLink] / 100.0;
        p.oversampleLog2 = juce::jlimit (0, 5, oversample.get()); p.ditherBits = ditherBitsFor (ditherBits.get()); p.noiseShape = juce::jlimit (0, 2, noiseShape.get());
        p.unityAudition = unity.get(); p.listenRemoved = listenRemoved.get();
    }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
};
