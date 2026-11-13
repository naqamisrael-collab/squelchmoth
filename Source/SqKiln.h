#pragma once
// Kiln (1.14 "Studio Line", phase H): multiband saturation on the Cinder engine. DSP: SqKilnDsp.h (tests/kiln_test.cpp).
// Face: SkinsKiln.inl. Claude: `kiln` (McpToolsKiln.h).

#include "SqColour.h"
#include "SqKilnDsp.h"
#include <deque>

class SqKilnPlugin : public SqColourBase, private juce::AsyncUpdater
{
public:
    static constexpr int maxBands = SqKiln::maxBands, perBand = 5, numSlots = SqKiln::numSlots;
    enum P { input, output, mix, x1, x2, x3, x4, x5, lfoRate, envAttack, envRelease, slot1, slot2, slot3, slot4, bandBase };   // then per band: drive, bias, dynamics, mix, output
    enum BP { drive = 0, bias, dynamics, bandMix, bandOut };
    SQ_COLOUR_IDENTITY (SqKilnPlugin, "Kiln", "sqmKiln", "Kiln")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "input",  "Input",  -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "output", "Output", -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "mix",    "Mix",      0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
                { "x1", "Crossover 1", 20.0f, 20000.0f, 200.0f, 0.25f, " Hz", 0 },
                { "x2", "Crossover 2", 20.0f, 20000.0f, 2500.0f, 0.25f, " Hz", 0 },
                { "x3", "Crossover 3", 20.0f, 20000.0f, 6000.0f, 0.25f, " Hz", 0 },
                { "x4", "Crossover 4", 20.0f, 20000.0f, 10000.0f, 0.25f, " Hz", 0 },
                { "x5", "Crossover 5", 20.0f, 20000.0f, 15000.0f, 0.25f, " Hz", 0 },
                { "lfoRate",    "LFO Rate",     0.02f, 20.0f, 1.0f, 0.3f, " Hz", 2 },
                { "envAttack",  "Env Attack",   0.1f, 200.0f, 10.0f, 0.3f, " ms", 1 },
                { "envRelease", "Env Release",  5.0f, 2000.0f, 150.0f, 0.3f, " ms", 0 },
                { "slot1", "Mod 1 Amount", -1.0f, 1.0f, 0.0f, 1.0f, "", 2 },
                { "slot2", "Mod 2 Amount", -1.0f, 1.0f, 0.0f, 1.0f, "", 2 },
                { "slot3", "Mod 3 Amount", -1.0f, 1.0f, 0.0f, 1.0f, "", 2 },
                { "slot4", "Mod 4 Amount", -1.0f, 1.0f, 0.0f, 1.0f, "", 2 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            for (int b = 1; b <= maxBands; ++b)
            {
                const auto n = std::to_string (b);
                v.push_back ({ addS ("b" + n + "drive"), addS ("Band " + n + " Drive"),   -12.0f, 36.0f, 6.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("b" + n + "bias"),  addS ("Band " + n + " Bias"),     -1.0f, 1.0f, 0.0f, 1.0f, "", 2 });
                v.push_back ({ addS ("b" + n + "dyn"),   addS ("Band " + n + " Dynamics"),  0.0f, 100.0f, 30.0f, 1.0f, " %", 0 });
                v.push_back ({ addS ("b" + n + "mix"),   addS ("Band " + n + " Mix"),       0.0f, 100.0f, 100.0f, 1.0f, " %", 0 });
                v.push_back ({ addS ("b" + n + "out"),   addS ("Band " + n + " Output"),  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 });
            }
            return v;
        }();
        return s;
    }
    static juce::StringArray modelNames() { juce::StringArray a; for (int m = 0; m < Cinder::numModels; ++m) a.add (Cinder::modelName (m)); return a; }
    static juce::StringArray osNames()    { return { "1x", "2x", "4x", "8x", "16x" }; }
    static juce::StringArray lfoNames()   { return { "Sine", "Triangle", "Square", "Random" }; }
    static juce::StringArray sourceNames(){ return { "Off", "LFO", "Envelope" }; }
    /** A slot amount (-1..1) in the target's units: drive targets +-36 dB, mix targets +-100 %. */
    static float slotScale (int target) { return target >= 1 && target <= 6 ? 36.0f : 1.0f; }

    juce::CachedValue<int> numBands, oversample, lfoShape;
    juce::CachedValue<bool> linearOs, autoGain;
    std::array<juce::CachedValue<int>, maxBands> model;
    std::array<juce::CachedValue<bool>, maxBands> solo, bypass;
    std::array<juce::CachedValue<int>, numSlots> slotSource, slotTarget;

    SqKilnPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        numBands.referTo (state, "bands", um, 3); oversample.referTo (state, "oversample", um, 2); lfoShape.referTo (state, "lfoShape", um, 0);
        linearOs.referTo (state, "linearOs", um, false); autoGain.referTo (state, "autoGain", um, true);
        static const int defModels[maxBands] = { Cinder::transformerIron, Cinder::triodeBox, Cinder::tapeHysteresis, Cinder::tapePreamp, Cinder::britishChannel, Cinder::tapePreamp };
        for (int b = 0; b < maxBands; ++b)
        {
            const auto n = juce::String (b + 1);
            model[(size_t) b].referTo (state, juce::Identifier ("model" + n), um, defModels[b]);
            solo[(size_t) b].referTo (state, juce::Identifier ("solo" + n), um, false);
            bypass[(size_t) b].referTo (state, juce::Identifier ("bypass" + n), um, false);
        }
        for (int s = 0; s < numSlots; ++s)
        {
            const auto n = juce::String (s + 1);
            slotSource[(size_t) s].referTo (state, juce::Identifier ("src" + n), um, 0);
            slotTarget[(size_t) s].referTo (state, juce::Identifier ("dst" + n), um, 0);
        }
        scope.fill (0);
    }
    ~SqKilnPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        numBands.setValue (3, um); oversample.setValue (2, um); lfoShape.setValue (0, um); linearOs.setValue (false, um); autoGain.setValue (true, um);
        static const int defModels[maxBands] = { Cinder::transformerIron, Cinder::triodeBox, Cinder::tapeHysteresis, Cinder::tapePreamp, Cinder::britishChannel, Cinder::tapePreamp };
        for (int b = 0; b < maxBands; ++b) { model[(size_t) b].setValue (defModels[b], um); solo[(size_t) b].setValue (false, um); bypass[(size_t) b].setValue (false, um); }
        for (int s = 0; s < numSlots; ++s) { slotSource[(size_t) s].setValue (0, um); slotTarget[(size_t) s].setValue (0, um); }
    }
    float bp (int band, int k) const { return bank[(size_t) (bandBase + band * perBand + k)]; }
    int paramOf (int band, int k) const { return bandBase + band * perBand + k; }

    double getLatencySeconds() override
    {
        // a probe engine answers for the settings as they are now, even before the audio engine has seen them
        if (! probeReady) { probe.prepare (sr, 64); probeReady = true; }
        fill (probe.p); probe.refresh();
        return (double) probe.latency() / juce::jmax (1.0, sr);
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        fill (engine.p); engine.prepare (sr, juce::jmax (64, info.blockSizeSamples)); lastLatency = engine.latency();
        probe.prepare (sr, 64); probeReady = true;
    }
    void deinitialise() override {}
    void reset() override { engine.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        fill (engine.p);
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        engine.process (ch, nch, n);
        if (engine.latency() != lastLatency) { lastLatency = engine.latency(); triggerAsyncUpdate(); }
        int sw = scopeW.load();
        for (int i = 0; i < n; ++i) scope[(size_t) ((sw + i) & (scopeLen - 1))] = 0.5f * (ch[0][i] + ch[nch > 1 ? 1 : 0][i]);
        scopeW = sw + n;
        for (int k = 0; k < maxBands; ++k) { activity[(size_t) k] = engine.meterActivity[(size_t) k]; driveNow[(size_t) k] = engine.meterDriveDb[(size_t) k]; levelDb[(size_t) k] = engine.meterLevelDb[(size_t) k]; }
        lfoNow = engine.meterLfo; envNow = engine.meterEnv;
    }

    SqKiln::Params params() const { SqKiln::Params p; fill (p); return p; }
    double sampleRate() const { return sr; }

    static constexpr int scopeLen = 4096;
    std::array<float, scopeLen> scope {}; std::atomic<int> scopeW { 0 };
    std::array<std::atomic<float>, maxBands> activity {}, driveNow {}, levelDb {};
    std::atomic<float> lfoNow { 0 }, envNow { 0 };

private:
    SqKiln::Engine engine, probe; int lastLatency = 0; bool probeReady = false;
    void fill (SqKiln::Params& p) const
    {
        p.numBands = juce::jlimit (1, maxBands, numBands.get()); p.oversampleLog2 = juce::jlimit (0, 4, oversample.get());
        p.linearPhaseOs = linearOs.get(); p.autoGain = autoGain.get();
        for (int i = 0; i < maxBands - 1; ++i) p.xover[(size_t) i] = bank[x1 + i];
        p.inputDb = bank[input]; p.outputDb = bank[output]; p.mix = bank[mix] / 100.0f;
        p.lfoShape = juce::jlimit (0, 3, lfoShape.get()); p.lfoHz = bank[lfoRate]; p.envAttackMs = bank[envAttack]; p.envReleaseMs = bank[envRelease];
        for (int k = 0; k < maxBands; ++k)
        {
            auto& b = p.band[(size_t) k];
            b.model = juce::jlimit (0, Cinder::numModels - 1, model[(size_t) k].get());
            b.driveDb = bp (k, drive); b.bias = bp (k, bias); b.dynamics = bp (k, dynamics) / 100.0f; b.mix = bp (k, bandMix) / 100.0f; b.outputDb = bp (k, bandOut);
            b.solo = solo[(size_t) k].get(); b.bypass = bypass[(size_t) k].get();
        }
        for (int s = 0; s < numSlots; ++s)
        {
            auto& sl = p.slot[(size_t) s];
            sl.source = juce::jlimit (0, 2, slotSource[(size_t) s].get()); sl.target = juce::jlimit (0, 13, slotTarget[(size_t) s].get());
            sl.amount = bank[slot1 + s] * slotScale (sl.target);
        }
    }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
};
