#pragma once
// Fissure Filter (1.14 "Studio Line", phase J): the filter flagship. DSP: SqFissureDsp.h (tests/fissure_test.cpp).
// Face: SkinsFissure.inl. Claude: `fissure_filter` (McpToolsFissure.h).

#include "SqColour.h"
#include "SqFissureDsp.h"
#include <deque>

class SqFissureFilterPlugin : public SqColourBase
{
public:
    static constexpr int numSlots = SqFissure::numSlots, perSlot = 6, numMods = SqFissure::numMods;
    enum P { lfo1Rate, lfo2Rate, envAttack, envRelease, envGain, mix, output, mod1, slotBase = mod1 + numMods };   // per slot: cutoff, res, drive, gain, pan, mix
    enum SP { cutoff = 0, res, drive, gain, pan, slotMix };
    SQ_COLOUR_IDENTITY (SqFissureFilterPlugin, "Fissure Filter", "sqmFissure", "Fissure")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "lfo1Rate",   "LFO 1 Rate",   0.02f, 30.0f, 1.0f, 0.3f, " Hz", 2 },
                { "lfo2Rate",   "LFO 2 Rate",   0.02f, 30.0f, 0.25f, 0.3f, " Hz", 2 },
                { "envAttack",  "Env Attack",   0.1f, 200.0f, 5.0f, 0.3f, " ms", 1 },
                { "envRelease", "Env Release",  5.0f, 2000.0f, 120.0f, 0.3f, " ms", 0 },
                { "envGain",    "Env Gain",   -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "mix",        "Mix",          0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
                { "output",     "Output",     -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            for (int m = 1; m <= numMods; ++m) v.push_back ({ addS ("mod" + std::to_string (m)), addS ("Mod " + std::to_string (m) + " Amount"), -1.0f, 1.0f, 0.0f, 1.0f, "", 2 });
            static const float defCut[numSlots] = { 1000.0f, 3000.0f, 500.0f, 8000.0f };
            for (int k = 1; k <= numSlots; ++k)
            {
                const auto n = std::to_string (k);
                v.push_back ({ addS ("f" + n + "cut"),   addS ("Filter " + n + " Cutoff"),   20.0f, 20000.0f, defCut[k - 1], 0.25f, " Hz", 0 });
                v.push_back ({ addS ("f" + n + "res"),   addS ("Filter " + n + " Resonance"), 0.0f, 100.0f, 20.0f, 1.0f, " %", 0 });
                v.push_back ({ addS ("f" + n + "drive"), addS ("Filter " + n + " Drive"),     0.0f, 36.0f, 0.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("f" + n + "gain"),  addS ("Filter " + n + " Gain"),    -24.0f, 24.0f, 6.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("f" + n + "pan"),   addS ("Filter " + n + " Pan"),    -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 });
                v.push_back ({ addS ("f" + n + "mix"),   addS ("Filter " + n + " Mix"),       0.0f, 100.0f, 100.0f, 1.0f, " %", 0 });
            }
            return v;
        }();
        return s;
    }
    static juce::StringArray typeNames()    { juce::StringArray a; for (int t = 0; t < SqFissure::numTypes; ++t) a.add (SqFissure::typeName (t)); return a; }
    static juce::StringArray routingNames() { return { "Serial", "Parallel", "Two Pairs", "Mid / Side" }; }
    static juce::StringArray lfoNames()     { return { "Sine", "Triangle", "Saw", "Square", "Random" }; }
    static juce::StringArray syncNames()    { return { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 dotted", "1/8 dotted", "1/4 triplet", "1/8 triplet" }; }
    static double syncBeats (int i) { static const double b[12] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25, 1.5, 0.75, 2.0 / 3, 1.0 / 3 }; return b[juce::jlimit (0, 11, i)]; }
    static juce::StringArray sourceNames()  { return { "Off", "LFO 1", "LFO 2", "Envelope" }; }
    /** Mod amounts are -1..1; cutoff targets read them as +-6 octaves. */
    static float modScale (int target) { return target >= 1 && target <= 4 ? 6.0f : 1.0f; }

    juce::CachedValue<int> routing, lfo1Shape, lfo2Shape, lfo1Sync, lfo2Sync;
    std::array<juce::CachedValue<bool>, numSlots> slotOn;
    std::array<juce::CachedValue<int>, numSlots> slotType, slotSlope;
    std::array<juce::CachedValue<int>, numMods> modSrc, modDst;

    SqFissureFilterPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        routing.referTo (state, "routing", um, 0); lfo1Shape.referTo (state, "lfo1Shape", um, 0); lfo2Shape.referTo (state, "lfo2Shape", um, 1);
        lfo1Sync.referTo (state, "lfo1Sync", um, 0); lfo2Sync.referTo (state, "lfo2Sync", um, 0);
        static const int defType[numSlots] = { SqFissure::lowPass, SqFissure::peak, SqFissure::highPass, SqFissure::lowPass };
        for (int k = 0; k < numSlots; ++k)
        {
            const auto n = juce::String (k + 1);
            slotOn[(size_t) k].referTo (state, juce::Identifier ("on" + n), um, k == 0);
            slotType[(size_t) k].referTo (state, juce::Identifier ("type" + n), um, defType[k]);
            slotSlope[(size_t) k].referTo (state, juce::Identifier ("slope" + n), um, 24);
        }
        for (int m = 0; m < numMods; ++m)
        {
            const auto n = juce::String (m + 1);
            modSrc[(size_t) m].referTo (state, juce::Identifier ("src" + n), um, 0);
            modDst[(size_t) m].referTo (state, juce::Identifier ("dst" + n), um, 0);
        }
        scope.fill (0);
    }
    ~SqFissureFilterPlugin() override { notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        routing.setValue (0, um); lfo1Shape.setValue (0, um); lfo2Shape.setValue (1, um); lfo1Sync.setValue (0, um); lfo2Sync.setValue (0, um);
        static const int defType[numSlots] = { SqFissure::lowPass, SqFissure::peak, SqFissure::highPass, SqFissure::lowPass };
        for (int k = 0; k < numSlots; ++k) { slotOn[(size_t) k].setValue (k == 0, um); slotType[(size_t) k].setValue (defType[k], um); slotSlope[(size_t) k].setValue (24, um); }
        for (int m = 0; m < numMods; ++m) { modSrc[(size_t) m].setValue (0, um); modDst[(size_t) m].setValue (0, um); }
    }
    float sp (int slot, int k) const { return bank[(size_t) (slotBase + slot * perSlot + k)]; }
    int paramOf (int slot, int k) const { return slotBase + slot * perSlot + k; }
    double lfoRate (int i) const
    {
        const int s = i == 0 ? lfo1Sync.get() : lfo2Sync.get();
        if (s > 0) return juce::jmax (20.0, bpm.load()) / 60.0 / syncBeats (s);
        return bank[i == 0 ? lfo1Rate : lfo2Rate];
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        fill (engine.p); engine.prepare (sr, juce::jmax (64, info.blockSizeSamples));
    }
    void deinitialise() override {}
    void reset() override { engine.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        bpm = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
        fill (engine.p);
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk; }
        engine.process (ch, nch, n);
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelOut = pk; }
        int sw = scopeW.load();
        for (int i = 0; i < n; ++i) scope[(size_t) ((sw + i) & (scopeLen - 1))] = 0.5f * (ch[0][i] + ch[nch > 1 ? 1 : 0][i]);
        scopeW = sw + n;
        for (int k = 0; k < numSlots; ++k) { cutNow[(size_t) k] = engine.meterCutoff[(size_t) k]; resNow[(size_t) k] = engine.meterRes[(size_t) k]; }
        lfo1Now = engine.meterLfo[0]; lfo2Now = engine.meterLfo[1]; envNow = engine.meterEnv;
    }

    SqFissure::Slot slotParams (int k) const
    {
        SqFissure::Slot s; s.on = slotOn[(size_t) k].get(); s.type = juce::jlimit (0, SqFissure::numTypes - 1, slotType[(size_t) k].get());
        s.slope = slotSlope[(size_t) k].get() >= 48 ? 48 : slotSlope[(size_t) k].get() >= 24 ? 24 : 12;
        s.cutoffHz = sp (k, cutoff); s.resonance = sp (k, res) / 100.0; s.driveDb = sp (k, drive); s.gainDb = sp (k, gain); s.pan = sp (k, pan) / 100.0; s.mix = sp (k, slotMix) / 100.0;
        return s;
    }
    double sampleRate() const { return sr; }

    static constexpr int scopeLen = 4096;
    std::array<float, scopeLen> scope {}; std::atomic<int> scopeW { 0 };
    std::array<std::atomic<float>, numSlots> cutNow {}, resNow {};
    std::atomic<float> lfo1Now { 0 }, lfo2Now { 0 }, envNow { 0 };
    std::atomic<double> bpm { 120.0 };

private:
    SqFissure::Engine engine;
    void fill (SqFissure::Params& p) const
    {
        p.routing = juce::jlimit (0, 3, routing.get());
        for (int k = 0; k < numSlots; ++k) p.slot[(size_t) k] = slotParams (k);
        p.lfoShape = { juce::jlimit (0, 4, lfo1Shape.get()), juce::jlimit (0, 4, lfo2Shape.get()) }; p.lfoHz = { lfoRate (0), lfoRate (1) };
        p.envAttackMs = bank[envAttack]; p.envReleaseMs = bank[envRelease]; p.envGainDb = bank[envGain];
        for (int m = 0; m < numMods; ++m)
        {
            auto& md = p.mod[(size_t) m]; md.source = juce::jlimit (0, 3, modSrc[(size_t) m].get()); md.target = juce::jlimit (0, 13, modDst[(size_t) m].get());
            md.amount = bank[mod1 + m] * modScale (md.target);
        }
        p.mix = bank[mix] / 100.0; p.outputDb = bank[output];
    }
};
