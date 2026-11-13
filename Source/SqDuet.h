#pragma once
// Duet Filter and Speck Filter (1.14 "Studio Line", phase K small tools). DSP: SqDuetDsp.h (tests/duet_test.cpp).
// Faces: SkinsDuet.inl. Claude: `duet_filter` and `speck_filter` (McpToolsDuet.h).
//  - Duet Filter: two filters, Serial / Parallel / Stereo / Mid-Side, link with an interval, one LFO with its depth per
//    filter and a stereo phase offset, an envelope follower (input or sidechain) with its depth per filter.
//  - Speck Filter: one filter with an envelope follower and an LFO - the quick tool.

#include "SqColour.h"
#include "SqDuetDsp.h"
#include <deque>

/** What both small filters share: tempo sync names, the LFO rate, the sidechain plumbing and the meters. */
struct SqSmallFilterCommon
{
    static juce::StringArray typeNames()   { juce::StringArray a; for (int t = 0; t < SqFissure::numTypes; ++t) a.add (SqFissure::typeName (t)); return a; }
    static juce::StringArray lfoNames()    { return { "Sine", "Triangle", "Saw", "Square", "Random" }; }
    static juce::StringArray syncNames()   { return { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 dotted", "1/8 dotted", "1/4 triplet", "1/8 triplet" }; }
    static double syncBeats (int i)        { static const double b[12] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25, 1.5, 0.75, 2.0 / 3, 1.0 / 3 }; return b[juce::jlimit (0, 11, i)]; }
    static constexpr int scopeLen = 4096;
};

//==============================================================================
class SqDuetFilterPlugin : public SqColourBase, public SqSmallFilterCommon
{
public:
    enum P { linkOct, lfoRate, lfoPhase, envAttack, envRelease, envGain, mix, output, filterBase };
    enum FP { cutoff = 0, res, drive, gain, fMix, lfoDepth, envDepth, perFilter };
    SQ_COLOUR_IDENTITY (SqDuetFilterPlugin, "Duet Filter", "sqmDuetFilter", "Duet")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "linkOct",    "Link Interval", -4.0f, 4.0f, 1.0f, 1.0f, " oct", 2 },
                { "lfoRate",    "LFO Rate",      0.02f, 30.0f, 0.5f, 0.3f, " Hz", 2 },
                { "lfoPhase",   "LFO Stereo Phase", 0.0f, 180.0f, 0.0f, 1.0f, " deg", 0 },
                { "envAttack",  "Env Attack",    0.1f, 200.0f, 5.0f, 0.3f, " ms", 1 },
                { "envRelease", "Env Release",   5.0f, 2000.0f, 150.0f, 0.3f, " ms", 0 },
                { "envGain",    "Env Sensitivity", -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "mix",        "Mix",           0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
                { "output",     "Output",      -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            static const float defCut[2] = { 800.0f, 1600.0f };
            for (int k = 1; k <= 2; ++k)
            {
                const auto n = std::to_string (k);
                v.push_back ({ addS ("f" + n + "cut"),   addS ("Filter " + n + " Cutoff"),    20.0f, 20000.0f, defCut[k - 1], 0.25f, " Hz", 0 });
                v.push_back ({ addS ("f" + n + "res"),   addS ("Filter " + n + " Resonance"),  0.0f, 100.0f, 25.0f, 1.0f, " %", 0 });
                v.push_back ({ addS ("f" + n + "drive"), addS ("Filter " + n + " Drive"),      0.0f, 36.0f, 0.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("f" + n + "gain"),  addS ("Filter " + n + " Gain"),     -24.0f, 24.0f, 6.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("f" + n + "mix"),   addS ("Filter " + n + " Mix"),        0.0f, 100.0f, 100.0f, 1.0f, " %", 0 });
                v.push_back ({ addS ("f" + n + "lfo"),   addS ("Filter " + n + " LFO Depth"), -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 });
                v.push_back ({ addS ("f" + n + "env"),   addS ("Filter " + n + " Env Depth"), -6.0f, 6.0f, 0.0f, 1.0f, " oct", 2 });
            }
            return v;
        }();
        return s;
    }
    static juce::StringArray routingNames() { return { "Serial", "Parallel", "Stereo", "Mid / Side" }; }

    juce::CachedValue<int> routing, lfoShape, lfoSync;
    juce::CachedValue<bool> link, envKey;
    std::array<juce::CachedValue<bool>, 2> on;
    std::array<juce::CachedValue<int>, 2> type, slope;

    SqDuetFilterPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        routing.referTo (state, "routing", um, 0); lfoShape.referTo (state, "lfoShape", um, 0); lfoSync.referTo (state, "lfoSync", um, 0);
        link.referTo (state, "link", um, false); envKey.referTo (state, "envKey", um, false);
        for (int k = 0; k < 2; ++k)
        {
            const auto n = juce::String (k + 1);
            on[(size_t) k].referTo (state, juce::Identifier ("on" + n), um, true);
            type[(size_t) k].referTo (state, juce::Identifier ("type" + n), um, k == 0 ? SqFissure::lowPass : SqFissure::bandPass);
            slope[(size_t) k].referTo (state, juce::Identifier ("slope" + n), um, 24);
        }
        scope.fill (0);
    }
    ~SqDuetFilterPlugin() override { notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        routing.setValue (0, um); lfoShape.setValue (0, um); lfoSync.setValue (0, um); link.setValue (false, um); envKey.setValue (false, um);
        for (int k = 0; k < 2; ++k) { on[(size_t) k].setValue (true, um); type[(size_t) k].setValue (k == 0 ? SqFissure::lowPass : SqFissure::bandPass, um); slope[(size_t) k].setValue (24, um); }
    }
    float fp (int f, int k) const { return bank[(size_t) (filterBase + f * perFilter + k)]; }
    int paramOf (int f, int k) const { return filterBase + f * perFilter + k; }
    double lfoHz() const { const int s = lfoSync.get(); return s > 0 ? juce::jmax (20.0, bpm.load()) / 60.0 / syncBeats (s) : (double) bank[lfoRate]; }

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
        const float* key[2] = { nullptr, nullptr }; int nKey = 0;
        if (b.getNumChannels() > 2) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); nKey = 2; }
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk; }
        engine.process (ch, nch, n, key, nKey);
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelOut = pk; }
        int sw = scopeW.load();
        for (int i = 0; i < n; ++i) scope[(size_t) ((sw + i) & (scopeLen - 1))] = 0.5f * (ch[0][i] + ch[nch > 1 ? 1 : 0][i]);
        scopeW = sw + n;
        for (int k = 0; k < 2; ++k) for (int c = 0; c < 2; ++c) cutNow[(size_t) (k * 2 + c)] = engine.meterCutoff[(size_t) k][(size_t) c];
        lfoNow = engine.meterLfo; envNow = engine.meterEnv; envDb = engine.meterEnvDb;
    }

    SqFissure::Slot slotParams (int k) const
    {
        SqFissure::Slot s; s.on = on[(size_t) k].get(); s.type = juce::jlimit (0, SqFissure::numTypes - 1, type[(size_t) k].get());
        s.slope = slope[(size_t) k].get() >= 48 ? 48 : slope[(size_t) k].get() >= 24 ? 24 : 12;
        s.cutoffHz = fp (k, cutoff); s.resonance = fp (k, res) / 100.0; s.driveDb = fp (k, drive); s.gainDb = fp (k, gain); s.mix = fp (k, fMix) / 100.0;
        return s;
    }
    /** An engine copy with the current settings (for the face's curves and the tool's readout). */
    SqDuet::Engine snapshot() const { SqDuet::Engine e; fill (e.p); e.prepare (sr, 64); return e; }
    double sampleRate() const { return sr; }

    std::array<float, scopeLen> scope {}; std::atomic<int> scopeW { 0 };
    std::array<std::atomic<float>, 4> cutNow {};   // [filter * 2 + channel]
    std::atomic<float> lfoNow { 0 }, envNow { 0 }, envDb { -120 };
    std::atomic<double> bpm { 120.0 };

private:
    SqDuet::Engine engine;
    void fill (SqDuet::Params& p) const
    {
        p.routing = juce::jlimit (0, 3, routing.get());
        for (int k = 0; k < 2; ++k) { p.slot[(size_t) k] = slotParams (k); p.lfoOct[(size_t) k] = fp (k, lfoDepth); p.envOct[(size_t) k] = fp (k, envDepth); }
        p.link = link.get(); p.linkOct = bank[linkOct];
        p.lfoShape = juce::jlimit (0, 4, lfoShape.get()); p.lfoHz = lfoHz(); p.lfoPhaseDeg = bank[lfoPhase];
        p.envAttackMs = bank[envAttack]; p.envReleaseMs = bank[envRelease]; p.envGainDb = bank[envGain]; p.envFromKey = envKey.get();
        p.mix = bank[mix] / 100.0; p.outputDb = bank[output];
    }
};

//==============================================================================
class SqSpeckFilterPlugin : public SqColourBase, public SqSmallFilterCommon
{
public:
    enum P { cutoff, res, drive, gain, envDepth, envAttack, envRelease, envGain, lfoDepth, lfoRate, lfoPhase, mix, output };
    SQ_COLOUR_IDENTITY (SqSpeckFilterPlugin, "Speck Filter", "sqmSpeckFilter", "Speck")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> v {
            { "cutoff",     "Cutoff",        20.0f, 20000.0f, 1200.0f, 0.25f, " Hz", 0 },
            { "res",        "Resonance",      0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "drive",      "Drive",          0.0f, 36.0f, 0.0f, 1.0f, " dB", 1 },
            { "gain",       "Gain",         -24.0f, 24.0f, 6.0f, 1.0f, " dB", 1 },
            { "envDepth",   "Env Depth",     -6.0f, 6.0f, 0.0f, 1.0f, " oct", 2 },
            { "envAttack",  "Env Attack",     0.1f, 200.0f, 5.0f, 0.3f, " ms", 1 },
            { "envRelease", "Env Release",    5.0f, 2000.0f, 150.0f, 0.3f, " ms", 0 },
            { "envGain",    "Env Sensitivity", -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
            { "lfoDepth",   "LFO Depth",     -4.0f, 4.0f, 0.0f, 1.0f, " oct", 2 },
            { "lfoRate",    "LFO Rate",       0.02f, 30.0f, 1.0f, 0.3f, " Hz", 2 },
            { "lfoPhase",   "LFO Stereo Phase", 0.0f, 180.0f, 0.0f, 1.0f, " deg", 0 },
            { "mix",        "Mix",            0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",     "Output",       -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
        return v;
    }

    juce::CachedValue<int> type, slope, lfoShape, lfoSync;
    juce::CachedValue<bool> envKey;

    SqSpeckFilterPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        type.referTo (state, "type", um, SqFissure::lowPass); slope.referTo (state, "slope", um, 24);
        lfoShape.referTo (state, "lfoShape", um, 0); lfoSync.referTo (state, "lfoSync", um, 0); envKey.referTo (state, "envKey", um, false);
        scope.fill (0);
    }
    ~SqSpeckFilterPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    void resetProps (juce::UndoManager* um) override
    { type.setValue (SqFissure::lowPass, um); slope.setValue (24, um); lfoShape.setValue (0, um); lfoSync.setValue (0, um); envKey.setValue (false, um); }
    double lfoHz() const { const int s = lfoSync.get(); return s > 0 ? juce::jmax (20.0, bpm.load()) / 60.0 / syncBeats (s) : (double) bank[lfoRate]; }

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
        const float* key[2] = { nullptr, nullptr }; int nKey = 0;
        if (b.getNumChannels() > 2) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); nKey = 2; }
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk; }
        engine.process (ch, nch, n, key, nKey);
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelOut = pk; }
        int sw = scopeW.load();
        for (int i = 0; i < n; ++i) scope[(size_t) ((sw + i) & (scopeLen - 1))] = 0.5f * (ch[0][i] + ch[nch > 1 ? 1 : 0][i]);
        scopeW = sw + n;
        cutNow[0] = engine.meterCutoff[0][0]; cutNow[1] = engine.meterCutoff[0][1];
        lfoNow = engine.meterLfo; envNow = engine.meterEnv; envDb = engine.meterEnvDb;
    }
    SqFissure::Slot slotParams() const
    {
        SqFissure::Slot s; s.on = true; s.type = juce::jlimit (0, SqFissure::numTypes - 1, type.get());
        s.slope = slope.get() >= 48 ? 48 : slope.get() >= 24 ? 24 : 12;
        s.cutoffHz = bank[cutoff]; s.resonance = bank[res] / 100.0; s.driveDb = bank[drive]; s.gainDb = bank[gain]; s.mix = 1.0;
        return s;
    }
    SqDuet::Engine snapshot() const { SqDuet::Engine e; fill (e.p); e.prepare (sr, 64); return e; }
    double sampleRate() const { return sr; }

    std::array<float, scopeLen> scope {}; std::atomic<int> scopeW { 0 };
    std::array<std::atomic<float>, 2> cutNow {};
    std::atomic<float> lfoNow { 0 }, envNow { 0 }, envDb { -120 };
    std::atomic<double> bpm { 120.0 };

private:
    SqDuet::Engine engine;
    void fill (SqDuet::Params& p) const
    {
        p.routing = SqDuet::serial; p.slot[0] = slotParams(); p.slot[1].on = false; p.link = false;
        p.lfoOct = { bank[lfoDepth], 0.0 }; p.envOct = { bank[envDepth], 0.0 };
        p.lfoShape = juce::jlimit (0, 4, lfoShape.get()); p.lfoHz = lfoHz(); p.lfoPhaseDeg = bank[lfoPhase];
        p.envAttackMs = bank[envAttack]; p.envReleaseMs = bank[envRelease]; p.envGainDb = bank[envGain]; p.envFromKey = envKey.get();
        p.mix = bank[mix] / 100.0; p.outputDb = bank[output];
    }
};
