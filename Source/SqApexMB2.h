#pragma once
// Apex Multiband 2 (1.14 "Studio Line", phase E): the multiband dynamics flagship. DSP: SqApexMB2Dsp.h (tests/mb2_test.cpp).
// Face: SkinsApexMB2.inl. Claude: `apex_multiband` (McpToolsApexMB2.h). The original Apex Multiband stays for old sessions.

#include "SqColour.h"
#include "SqApexMB2Dsp.h"
#include <deque>

class SqApexMB2Plugin : public SqColourBase, private juce::AsyncUpdater, private juce::Timer
{
public:
    static constexpr int maxBands = SqMB2::maxBands, perBand = 7;
    enum P { input, output, mix, lookahead, x1, x2, x3, x4, x5, bandBase };   // then per band: thr, ratio, range, knee, attack, release, gain
    enum BP { thr = 0, ratio, range, knee, attack, release, gain };
    SQ_COLOUR_IDENTITY (SqApexMB2Plugin, "Apex Multiband 2", "sqmApexMB2", "ApexMB2")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "input",     "Input",       -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "output",    "Output",      -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "mix",       "Mix",           0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
                { "lookahead", "Lookahead",     0.0f, 20.0f, 0.0f, 0.5f, " ms", 1 },
                { "x1", "Crossover 1", 20.0f, 20000.0f, 120.0f, 0.25f, " Hz", 0 },
                { "x2", "Crossover 2", 20.0f, 20000.0f, 500.0f, 0.25f, " Hz", 0 },
                { "x3", "Crossover 3", 20.0f, 20000.0f, 2000.0f, 0.25f, " Hz", 0 },
                { "x4", "Crossover 4", 20.0f, 20000.0f, 6000.0f, 0.25f, " Hz", 0 },
                { "x5", "Crossover 5", 20.0f, 20000.0f, 12000.0f, 0.25f, " Hz", 0 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            for (int b = 1; b <= maxBands; ++b)
            {
                const auto n = std::to_string (b);
                v.push_back ({ addS ("b" + n + "thr"),   addS ("Band " + n + " Threshold"), -60.0f, 0.0f, -20.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("b" + n + "ratio"), addS ("Band " + n + " Ratio"),       1.0f, 20.0f, 2.0f, 0.4f, ":1", 1 });
                v.push_back ({ addS ("b" + n + "range"), addS ("Band " + n + " Range"),       0.0f, 36.0f, 12.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("b" + n + "knee"),  addS ("Band " + n + " Knee"),        0.0f, 24.0f, 6.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("b" + n + "att"),   addS ("Band " + n + " Attack"),      0.1f, 500.0f, 10.0f, 0.3f, " ms", 1 });
                v.push_back ({ addS ("b" + n + "rel"),   addS ("Band " + n + " Release"),     5.0f, 5000.0f, 150.0f, 0.3f, " ms", 0 });
                v.push_back ({ addS ("b" + n + "gain"),  addS ("Band " + n + " Gain"),      -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 });
            }
            return v;
        }();
        return s;
    }

    juce::CachedValue<int> numBands, phase, detector;
    std::array<juce::CachedValue<int>, maxBands> mode, place, keyOf;
    std::array<juce::CachedValue<bool>, maxBands> solo, bypass;

    SqApexMB2Plugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        numBands.referTo (state, "bands", um, 4); phase.referTo (state, "phase", um, 0); detector.referTo (state, "detector", um, 0);
        for (int b = 0; b < maxBands; ++b)
        {
            const auto n = juce::String (b + 1);
            mode[(size_t) b].referTo (state, juce::Identifier ("mode" + n), um, 0);
            place[(size_t) b].referTo (state, juce::Identifier ("place" + n), um, 0);
            keyOf[(size_t) b].referTo (state, juce::Identifier ("key" + n), um, 0);
            solo[(size_t) b].referTo (state, juce::Identifier ("solo" + n), um, false);
            bypass[(size_t) b].referTo (state, juce::Identifier ("bypass" + n), um, false);
        }
        scopeIn.fill (0); scopeOut.fill (0);
        startTimerHz (10);   // builds new linear-phase filters off the audio thread
    }
    ~SqApexMB2Plugin() override { stopTimer(); cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        numBands.setValue (4, um); phase.setValue (0, um); detector.setValue (0, um);
        for (int b = 0; b < maxBands; ++b) { mode[(size_t) b].setValue (0, um); place[(size_t) b].setValue (0, um); keyOf[(size_t) b].setValue (0, um); solo[(size_t) b].setValue (false, um); bypass[(size_t) b].setValue (false, um); }
    }
    static juce::StringArray detectorNames() { return { "Smart", "RMS", "Average", "Peak", "Fast" }; }
    float bp (int band, int k) const { return bank[(size_t) (bandBase + band * perBand + k)]; }
    int paramOf (int band, int k) const { return bandBase + band * perBand + k; }

    double getLatencySeconds() override
    {
        SqMB2::Params p; fill (p);
        const int L = (int) std::lround (juce::jlimit (0.0, 20.0, p.lookaheadMs) * 0.001 * sr);
        return (double) (p.phase == 1 ? std::max (SqMB2::Engine::linearLatency (sr), L) : L) / juce::jmax (1.0, sr);
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        fill (engine.p); engine.prepare (sr, juce::jmax (64, info.blockSizeSamples));
        lastLatency = engine.latency();
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
        for (int i = 0; i < n; ++i) scopeIn[(size_t) ((sw + i) & (scopeLen - 1))] = 0.5f * (ch[0][i] + ch[1][i]);
        engine.process (ch, nch, haveKey ? key : nullptr, haveKey ? 2 : 0, n);
        for (int i = 0; i < n; ++i) scopeOut[(size_t) ((sw + i) & (scopeLen - 1))] = 0.5f * (ch[0][i] + ch[1][i]);
        scopeW = sw + n;
        for (int k = 0; k < maxBands; ++k) { grDb[(size_t) k] = engine.meterGrDb[(size_t) k].load(); levelDb[(size_t) k] = engine.meterLevelDb[(size_t) k].load(); }
    }

    /** The settings the face draws from (the same ones the audio uses). */
    SqMB2::Params params() const { SqMB2::Params p; fill (p); return p; }
    double sampleRate() const { return sr; }

    static constexpr int scopeLen = 4096;
    std::array<float, scopeLen> scopeIn {}, scopeOut {};
    std::atomic<int> scopeW { 0 };
    std::array<std::atomic<float>, maxBands> grDb {}, levelDb {};

private:
    SqMB2::Engine engine; int lastLatency = 0;

    void fill (SqMB2::Params& p) const
    {
        p.numBands = juce::jlimit (1, maxBands, numBands.get()); p.phase = juce::jlimit (0, 1, phase.get()); p.detector = juce::jlimit (0, 4, detector.get());
        for (int i = 0; i < maxBands - 1; ++i) p.xover[(size_t) i] = bank[x1 + i];
        p.lookaheadMs = bank[lookahead]; p.inputDb = bank[input]; p.outputDb = bank[output]; p.mix = bank[mix] / 100.0;
        for (int k = 0; k < maxBands; ++k)
        {
            auto& b = p.band[(size_t) k];
            b.thrDb = bp (k, thr); b.ratio = bp (k, ratio); b.rangeDb = bp (k, range); b.kneeDb = bp (k, knee);
            b.attackMs = bp (k, attack); b.releaseMs = bp (k, release); b.gainDb = bp (k, gain);
            b.mode = juce::jlimit (0, 3, mode[(size_t) k].get()); b.place = juce::jlimit (0, 4, place[(size_t) k].get()); b.key = juce::jlimit (0, 1, keyOf[(size_t) k].get());
            b.solo = solo[(size_t) k].get(); b.bypass = bypass[(size_t) k].get();
        }
    }
    void timerCallback() override { SqMB2::Params p; fill (p); if (p.phase == 1) engine.buildLinearIfNeeded (p); }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
};
