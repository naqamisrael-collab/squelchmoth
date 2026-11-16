#pragma once
// 1.11 Color & Space, second wave part 2: Shard (C5), Murmuration (C3), Masquerade (C15).
// DSP: SqColor111cDsp.h (tests/color111c_test.cpp) and SqMasqueradeDsp.h (tests/masquerade_test.cpp).

#include "SqColor111b.h"
#include "SqColor111cDsp.h"
#include "SqMasqueradeDsp.h"
#include "Scales.h"

/** Follows the session key Moth Key writes (sqmKeyRoot / sqmKeyScale / sqmKeyMap) on the message thread. */
struct SqSessionKey
{
    std::atomic<int> root { 0 }, scale { 1 }; std::atomic<bool> known { false };
    void poll (te::Edit& edit)
    {
        auto es = edit.state;
        if (! es.hasProperty ("sqmKeyRoot")) { known = false; return; }
        int r = (int) es["sqmKeyRoot"], sc = (int) es.getProperty ("sqmKeyScale", 1);
        const auto map = es.getProperty ("sqmKeyMap").toString();
        if (map.isNotEmpty())
        {
            const double t = edit.getTransport().getPosition().inSeconds();
            for (auto& e : juce::StringArray::fromTokens (map, ";", ""))
            {
                auto p = juce::StringArray::fromTokens (e, ":", ""); if (p.size() < 3) continue;
                if (p[0].getDoubleValue() <= t + 1e-6) { r = p[1].getIntValue(); sc = p[2].getIntValue() ? 2 : 1; }
            }
        }
        root = juce::jlimit (0, 11, r); scale = juce::jlimit (0, (int) SqScales::scales().size() - 1, sc); known = true;
    }
    /** The scale as a mask relative to its root (bit 0 = the root). */
    static int relMask (int scale) { int m = 0; for (int s : SqScales::scales()[(size_t) juce::jlimit (0, (int) SqScales::scales().size() - 1, scale)].steps) m |= 1 << (s % 12); return m; }
    static juce::StringArray rootNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    static juce::StringArray scaleNames() { juce::StringArray a; for (auto& s : SqScales::scales()) a.add (s.name); return a; }
};

//==============================================================================
class SqShardPlugin : public SqColor111bBase, private juce::Timer
{
public:
    enum P { splice, delay, pitch, recycle, mix, reverse, smoothing, spray, density, sizeJitter, pitchL, pitchR, spliceOffR, delayOffR,
             lowCut, highCut, dynThresh, dynRelease, lfo1Depth, lfo1Rate, lfo2Depth, lfo2Rate, envDepth, envAttack, envRelease, output, numParams };
    SQ_COLOUR_IDENTITY (SqShardPlugin, "Shard", "sqmShard", "Shard")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "splice",     "Splice",          10.0f, 8000.0f, 250.0f, 0.3f, " ms", 0 },
            { "delay",      "Delay",            0.0f, 8000.0f,   0.0f, 0.3f, " ms", 0 },
            { "pitch",      "Pitch",          -48.0f, 48.0f,    12.0f, 1.0f, " st", 2 },
            { "recycle",    "Recycle",          0.0f, 99.0f,    30.0f, 1.0f, " %", 0 },
            { "mix",        "Mix",              0.0f, 100.0f,   50.0f, 1.0f, " %", 0 },
            { "reverse",    "Reverse",          0.0f, 100.0f,  100.0f, 1.0f, " %", 0 },
            { "smoothing",  "Smoothing",        1.0f, 50.0f,    25.0f, 1.0f, " %", 0 },
            { "spray",      "Spray",            0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "density",    "Density",          1.0f, 4.0f,      1.0f, 1.0f, "x", 2 },
            { "sizeJitter", "Size Jitter",      0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "pitchL",     "Pitch L",       -100.0f, 100.0f,    0.0f, 1.0f, " ct", 0 },
            { "pitchR",     "Pitch R",       -100.0f, 100.0f,    0.0f, 1.0f, " ct", 0 },
            { "spliceOffR", "Splice R",       -50.0f, 50.0f,     0.0f, 1.0f, " %", 0 },
            { "delayOffR",  "Delay R",       -250.0f, 250.0f,    0.0f, 1.0f, " ms", 0 },
            { "lowCut",     "Low Cut",          1.0f, 5000.0f,   1.0f, 0.25f, " Hz", 0 },
            { "highCut",    "High Cut",       500.0f, 20000.0f, 20000.0f, 0.4f, " Hz", 0 },
            { "dynThresh",  "Gate/Duck Threshold", -60.0f, 0.0f, -30.0f, 1.0f, " dB", 1 },
            { "dynRelease", "Gate/Duck Release", 5.0f, 2000.0f, 200.0f, 0.4f, " ms", 0 },
            { "lfo1Depth",  "LFO 1 Depth",   -100.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "lfo1Rate",   "LFO 1 Rate",       0.01f, 20.0f,    0.5f, 0.3f, " Hz", 2 },
            { "lfo2Depth",  "LFO 2 Depth",   -100.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "lfo2Rate",   "LFO 2 Rate",       0.01f, 20.0f,    2.0f, 0.3f, " Hz", 2 },
            { "envDepth",   "Env Depth",     -100.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "envAttack",  "Env Attack",       0.1f, 500.0f,   10.0f, 0.4f, " ms", 1 },
            { "envRelease", "Env Release",      5.0f, 3000.0f, 250.0f, 0.4f, " ms", 0 },
            { "output",     "Output",         -24.0f, 12.0f,     0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray engineNames() { return { "Vintage", "Clean" }; }
    static juce::StringArray fbNames()     { return { "Mixed", "Dual", "Ping-Pong" }; }
    static juce::StringArray dynNames()    { return { "Off", "Gate", "Duck" }; }
    static juce::StringArray targetNames() { juce::StringArray a; for (int i = 0; i < Shard::numTargets; ++i) a.add (Shard::targetName (i)); return a; }
    SqShardPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        engine.referTo (state, "engine", um, 0); fbMode.referTo (state, "fbMode", um, 0); dynMode.referTo (state, "dynMode", um, 0); keyOn.referTo (state, "keyOn", um, false);
        quantize.referTo (state, "quantizeOn", um, false); autoKey.referTo (state, "autoKeyOn", um, true); keyRoot.referTo (state, "keyRoot", um, 0); scale.referTo (state, "scale", um, 1);
        spliceSync.referTo (state, "spliceSync", um, 0); delaySync.referTo (state, "delaySync", um, 0);
        lfo1Target.referTo (state, "lfo1Target", um, 0); lfo1Shape.referTo (state, "lfo1Shape", um, 0); lfo1Sync.referTo (state, "lfo1Sync", um, 0);
        lfo2Target.referTo (state, "lfo2Target", um, 0); lfo2Shape.referTo (state, "lfo2Shape", um, 1); lfo2Sync.referTo (state, "lfo2Sync", um, 0);
        envTarget.referTo (state, "envTarget", um, 0);
        startTimerHz (4);
    }
    ~SqShardPlugin() override { stopTimer(); notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> engine, fbMode, dynMode, keyRoot, scale, spliceSync, delaySync, lfo1Target, lfo1Shape, lfo1Sync, lfo2Target, lfo2Shape, lfo2Sync, envTarget;
    juce::CachedValue<bool> keyOn, quantize, autoKey;
    void resetProps (juce::UndoManager* um) override
    {
        engine.setValue (0, um); fbMode.setValue (0, um); dynMode.setValue (0, um); keyOn.setValue (false, um); quantize.setValue (false, um); autoKey.setValue (true, um);
        keyRoot.setValue (0, um); scale.setValue (1, um); spliceSync.setValue (0, um); delaySync.setValue (0, um);
        lfo1Target.setValue (0, um); lfo1Shape.setValue (0, um); lfo1Sync.setValue (0, um); lfo2Target.setValue (0, um); lfo2Shape.setValue (1, um); lfo2Sync.setValue (0, um); envTarget.setValue (0, um);
    }
    bool producesAudioWhenNoAudioInput() override   { return true; }
    bool noTail() override                           { return false; }
    double getTailLength() const override { const double t = (proc.spliceMsNow() + proc.delayMsNow()) * 0.001; const double r = juce::jlimit (0.0, 0.98, (double) proc.p.recycle);
                                            return juce::jlimit (0.2, 60.0, t * (r > 0.01 ? std::log (0.001) / std::log (r) : 1.0) + 0.5); }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; sessionKey.poll (edit); read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, keyOn.get(), [this] (float** ch, int nch, const float* const* key, int n) { proc.setTransport (bpm, ppq, playing); proc.process (ch, nch, key, n); });
    }
    Shard::Params knobParams() const { Shard::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Shard::Processor proc; SqSessionKey sessionKey;
private:
    void timerCallback() override { sessionKey.poll (edit); }
    void fill (const float* v, Shard::Params& p) const
    {
        p.spliceMs = v[splice]; p.delayMs = v[delay]; p.pitchSt = v[pitch]; p.recycle = v[recycle] / 100.0f; p.mix = v[mix] / 100.0f; p.reverseProb = v[reverse] / 100.0f;
        p.smoothing = v[smoothing] / 100.0f; p.spray = v[spray] / 100.0f; p.density = v[density]; p.sizeJitter = v[sizeJitter] / 100.0f;
        p.pitchOffsetLCents = v[pitchL]; p.pitchOffsetRCents = v[pitchR]; p.spliceOffsetR = v[spliceOffR] / 100.0f; p.delayOffsetRMs = v[delayOffR];
        p.lowCutHz = v[lowCut]; p.highCutHz = v[highCut]; p.dynThreshDb = v[dynThresh]; p.dynReleaseMs = v[dynRelease];
        p.engine = juce::jlimit (0, 1, engine.get()); p.fbMode = juce::jlimit (0, 2, fbMode.get()); p.dyn = juce::jlimit (0, 2, dynMode.get()); p.keyOn = keyOn.get();
        p.spliceSync = spliceSync.get() - 1; p.delaySync = delaySync.get() - 1;
        p.quantize = quantize.get();
        const bool sess = autoKey.get() && sessionKey.known.load();
        p.keyRoot = sess ? sessionKey.root.load() : juce::jlimit (0, 11, keyRoot.get());
        p.scaleMask = SqSessionKey::relMask (sess ? sessionKey.scale.load() : scale.get());
        auto lfo = [&] (Shard::Lfo& l, int tgt, int shp, int syn, float depth, float rate)
        { l.target = juce::jlimit (0, Shard::numTargets - 1, tgt); l.shape = juce::jlimit (0, (int) Eng::WingMod::numShapes - 1, shp); l.sync = syn > 0; l.syncIndex = juce::jmax (0, syn - 1); l.depth = depth / 100.0f; l.rateHz = rate; };
        lfo (p.lfo[0], lfo1Target.get(), lfo1Shape.get(), lfo1Sync.get(), v[lfo1Depth], v[lfo1Rate]);
        lfo (p.lfo[1], lfo2Target.get(), lfo2Shape.get(), lfo2Sync.get(), v[lfo2Depth], v[lfo2Rate]);
        p.envTarget = juce::jlimit (0, Shard::numTargets - 1, envTarget.get()); p.envDepth = v[envDepth] / 100.0f; p.envAttackMs = v[envAttack]; p.envReleaseMs = v[envRelease];
        p.outputDb = v[output];
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};

//==============================================================================
class SqMurmurationPlugin : public SqColor111bBase
{
public:
    enum P { time, beats, x, y, texture, warp, color, blur, grain, modRate, modDepth, shimmer, shimmerFade, tilt, lowCut, highCut, width, duck, preDelay, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqMurmurationPlugin, "Murmuration", "sqmMurmuration", "Murmur")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "time",        "Time",          10.0f, 90000.0f, 2000.0f, 0.2f, " ms", 0 },
            { "beats",       "Beats",          0.0f, 32.0f,      0.0f, 1.0f, " beats", 2 },
            { "x",           "Shape X",        0.0f, 100.0f,    25.0f, 1.0f, " %", 0 },
            { "y",           "Shape Y",        0.0f, 100.0f,    50.0f, 1.0f, " %", 0 },
            { "texture",     "Texture",        0.0f, 100.0f,    60.0f, 1.0f, " %", 0 },
            { "warp",        "Warp",           0.0f, 100.0f,    30.0f, 1.0f, " %", 0 },
            { "color",       "Color",       -100.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "blur",        "Blur",           0.0f, 100.0f,    40.0f, 1.0f, " %", 0 },
            { "grain",       "Grain",          0.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "modRate",     "Mod Rate",       0.01f, 20.0f,     0.3f, 0.3f, " Hz", 2 },
            { "modDepth",    "Mod Depth",      0.0f, 100.0f,    20.0f, 1.0f, " %", 0 },
            { "shimmer",     "Shimmer",        0.0f, 100.0f,     0.0f, 1.0f, " %", 0 },
            { "shimmerFade", "Shimmer Fade",   0.0f, 100.0f,    50.0f, 1.0f, " %", 0 },
            { "tilt",        "Tail Tilt",    -12.0f, 12.0f,      0.0f, 1.0f, " dB", 1 },
            { "lowCut",      "Low Cut",       20.0f, 2000.0f,   20.0f, 0.35f, " Hz", 0 },
            { "highCut",     "High Cut",    1000.0f, 20000.0f, 20000.0f, 0.4f, " Hz", 0 },
            { "width",       "Width",          0.0f, 200.0f,   100.0f, 1.0f, " %", 0 },
            { "duck",        "Ducking",        0.0f, 24.0f,      0.0f, 1.0f, " dB", 1 },
            { "preDelay",    "Pre-Delay",      0.0f, 1000.0f,    0.0f, 0.4f, " ms", 0 },
            { "mix",         "Mix",            0.0f, 100.0f,    40.0f, 1.0f, " %", 0 },
            { "output",      "Output",       -24.0f, 12.0f,      0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray shimmerNames() { return { "-24 st", "-12 st", "+12 st", "+24 st" }; }
    static int shimmerSemis (int i) { static const int s[] = { -24, -12, 12, 24 }; return s[juce::jlimit (0, 3, i)]; }
    static juce::StringArray freezeNames() { return { "Loop", "Spectral" }; }
    SqMurmurationPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        sync.referTo (state, "sync", um, 0); modShape.referTo (state, "modShape", um, 0); shimmerSt.referTo (state, "shimmerSt", um, 2);
        freeze.referTo (state, "freezeOn", um, false); freezeMode.referTo (state, "freezeMode", um, 0); infinite.referTo (state, "infiniteOn", um, false);
        customEnv.referTo (state, "customEnvOn", um, false); envShape.referTo (state, "envShape", um, {});
    }
    ~SqMurmurationPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> sync, modShape, shimmerSt, freezeMode; juce::CachedValue<bool> freeze, infinite, customEnv; juce::CachedValue<juce::String> envShape;
    void resetProps (juce::UndoManager* um) override
    {
        sync.setValue (0, um); modShape.setValue (0, um); shimmerSt.setValue (2, um); freeze.setValue (false, um); freezeMode.setValue (0, um); infinite.setValue (false, um);
        customEnv.setValue (false, um); envShape.setValue ({}, um);
    }
    bool producesAudioWhenNoAudioInput() override   { return true; }
    bool noTail() override                           { return false; }
    double getTailLength() const override { return infinite.get() || freeze.get() ? 60.0 : juce::jlimit (0.2, 95.0, proc.timeMsNow() * 0.001 + proc.p.preDelayMs * 0.001 + 0.5); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; reparse(); read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.setTransport (bpm, ppq, playing); proc.process (ch, nch, n); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && id == juce::Identifier ("envShape")) reparse();
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    Murm::Params knobParams() const { Murm::Params p = proc.p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Murm::Processor proc;
private:
    SqStepStore envStore;
    void reparse() { const Murm::Params d; envStore.set (envShape.get(), d.env); }
    void fill (const float* v, Murm::Params& p) const
    {
        p.timeMs = v[time]; p.timeBeats = v[beats] >= 0.25f ? v[beats] : 0.0f; p.sync = sync.get() - 1;
        p.x = v[x] / 100.0f; p.y = v[y] / 100.0f; p.texture = v[texture] / 100.0f; p.warp = v[warp] / 100.0f; p.color = v[color] / 100.0f; p.blur = v[blur] / 100.0f; p.grain = v[grain] / 100.0f;
        p.modRate = v[modRate]; p.modDepth = v[modDepth] / 100.0f; p.modShape = juce::jlimit (0, (int) Eng::WingMod::numShapes - 1, modShape.get());
        p.shimmer = v[shimmer] / 100.0f; p.shimmerFade = v[shimmerFade] / 100.0f; p.shimmerSt = shimmerSemis (shimmerSt.get());
        p.tiltDb = v[tilt]; p.lowCutHz = v[lowCut]; p.highCutHz = v[highCut]; p.width = v[width] / 100.0f; p.duckDb = v[duck]; p.preDelayMs = v[preDelay];
        p.mix = v[mix] / 100.0f; p.outputDb = v[output];
        p.freeze = freeze.get(); p.freezeMode = juce::jlimit (0, 1, freezeMode.get()); p.infinite = infinite.get(); p.customEnv = customEnv.get();
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        fill (v, proc.p); envStore.take (proc.p.env); for (auto& e : proc.p.env) e = juce::jlimit (0.0f, 1.0f, e);
    }
};

//==============================================================================
class SqMasqueradePlugin : public SqColor111bBase, private juce::Timer, private juce::AsyncUpdater
{
public:
    enum P { pitch, formant, drive, retune, chordLevel, chordSpread, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqMasqueradePlugin, "Masquerade", "sqmMasquerade", "Masq")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "pitch",       "Pitch",        -24.0f, 24.0f,    0.0f, 1.0f, " st", 2 },
            { "formant",     "Formant",      -24.0f, 24.0f,    0.0f, 1.0f, " st", 2 },
            { "drive",       "Drive",          0.0f, 100.0f,  40.0f, 1.0f, " %", 0 },
            { "retune",      "Retune",         0.0f, 200.0f,   0.0f, 0.5f, " ms", 0 },
            { "chordLevel",  "Chord Level",    0.0f, 100.0f,  70.0f, 1.0f, " %", 0 },
            { "chordSpread", "Chord Spread",   0.0f, 100.0f,  60.0f, 1.0f, " %", 0 },
            { "mix",         "Mix",            0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",      "Output",       -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray modeNames()   { juce::StringArray a; for (int i = 0; i < Masquerade::numModes; ++i) a.add (Masquerade::modeName (i)); return a; }
    static juce::StringArray engineNames() { return { "Live", "Studio" }; }
    static juce::StringArray voiceNames()  { juce::StringArray s; for (auto& v : SqVocal::voiceTypes()) s.add (v.name); return s; }
    static juce::StringArray driveNames()  { juce::StringArray a { "Off" }; for (int i = 0; i < Eng::AnalogStyle::numVoicings; ++i) a.add (Eng::AnalogStyle::voicingName (i)); return a; }
    static juce::StringArray noteNames()   { juce::StringArray a; for (int n = 24; n <= 84; ++n) a.add (SqSessionKey::rootNames()[n % 12] + juce::String (n / 12 - 1)); return a; }
    SqMasqueradePlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        mode.referTo (state, "mode", um, 0); link.referTo (state, "linkOn", um, false); engine.referTo (state, "engine", um, 0); voiceType.referTo (state, "voiceType", um, 1);
        autoKey.referTo (state, "autoKeyOn", um, true); keyRoot.referTo (state, "keyRoot", um, 0); scale.referTo (state, "scale", um, 1); robotNote.referTo (state, "robotNote", um, 48);
        driveVoicing.referTo (state, "driveVoicing", um, 0);
        startTimerHz (4);
    }
    ~SqMasqueradePlugin() override { stopTimer(); cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> mode, engine, voiceType, keyRoot, scale, robotNote, driveVoicing; juce::CachedValue<bool> link, autoKey;
    void resetProps (juce::UndoManager* um) override
    {
        mode.setValue (0, um); link.setValue (false, um); engine.setValue (0, um); voiceType.setValue (1, um); autoKey.setValue (true, um); keyRoot.setValue (0, um); scale.setValue (1, um);
        robotNote.setValue (48, um); driveVoicing.setValue (0, um);
    }
    bool takesMidiInput() override { return true; }
    double getLatencySeconds() override
    {
        const int L = SqVocal::TuneProcessor::latencyFor (juce::jlimit (0, 1, engine.get()), juce::jlimit (0, 5, voiceType.get()), sr)
                      + (driveVoicing.get() > 0 ? juce::roundToInt (proc.driveLatency()) : 0);
        return (double) L / juce::jmax (1.0, sr);
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; sessionKey.poll (edit); read (true);
        proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); held.fill (false);
    }
    void deinitialise() override {}
    void reset() override { proc.reset(); held.fill (false); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) held.fill (false);
            for (auto& m : *fc.bufferForMidiMessages)
            {
                if (m.isNoteOn()) held[(size_t) m.getNoteNumber()] = true;
                else if (m.isNoteOff()) held[(size_t) m.getNoteNumber()] = false;
                else if (m.isAllNotesOff() || m.isAllSoundOff()) held.fill (false);
            }
        }
        proc.setHeld (held);
        render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); liveHz = proc.inputHz(); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && (id == juce::Identifier ("engine") || id == juce::Identifier ("voiceType") || id == juce::Identifier ("driveVoicing"))) triggerAsyncUpdate();
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    Masquerade::Params knobParams() const { Masquerade::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Masquerade::Processor proc; SqSessionKey sessionKey; std::atomic<float> liveHz { 0 };
private:
    std::array<bool, 128> held {};
    void timerCallback() override { sessionKey.poll (edit); }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
    void fill (const float* v, Masquerade::Params& p) const
    {
        p.mode = juce::jlimit (0, Masquerade::numModes - 1, mode.get()); p.pitchSt = v[pitch]; p.formantSt = v[formant]; p.link = link.get();
        p.engine = juce::jlimit (0, 1, engine.get()); p.voiceType = juce::jlimit (0, 5, voiceType.get());
        const bool sess = autoKey.get() && sessionKey.known.load();
        p.keyRoot = sess ? sessionKey.root.load() : juce::jlimit (0, 11, keyRoot.get());
        p.scaleMask = SqSessionKey::relMask (sess ? sessionKey.scale.load() : scale.get());
        p.robotNote = juce::jlimit (24, 84, robotNote.get()); p.retuneMs = v[retune]; p.chordLevel = v[chordLevel] / 100.0f; p.chordSpread = v[chordSpread] / 100.0f;
        p.driveVoicing = juce::jlimit (0, (int) Eng::AnalogStyle::numVoicings, driveVoicing.get()); p.drive = v[drive] / 100.0f;
        p.mix = v[mix] / 100.0f; p.outputDb = v[output];
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};
