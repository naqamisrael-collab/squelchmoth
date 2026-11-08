#pragma once
// Wingbeat Echo and Wingbeat Jr. (1.11 Color & Space, C2). DSP: SqWingbeatDsp.h (tests/wingbeat_test.cpp).

#include "SqColour.h"
#include "SqWingbeatDsp.h"

/** Shared plumbing: tempo, tail, the processor. */
class SqWingbeatBase : public SqColourBase
{
public:
    explicit SqWingbeatBase (te::PluginCreationInfo info) : SqColourBase (info) {}
    bool producesAudioWhenNoAudioInput() override   { return true; }
    bool noTail() override                           { return false; }
    double getTailLength() const override
    {
        const double t = juce::jmax (proc.p.timeMs, proc.p.timeRMs) * 0.001;
        const double fb = juce::jlimit (0.0, 0.98, (double) proc.p.feedback);
        return juce::jlimit (0.1, 60.0, t * (fb > 0.01 ? std::log (0.001) / std::log (fb) : 1.0) + 0.5);
    }
    double bpmAt (const te::PluginRenderContext& fc)
    {
        const double b = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
        return b > 1 ? b : 120.0;
    }
    Wingbeat::Processor proc;
    std::atomic<float> liveTimeMs { 0 }, liveDuckDb { 0 };
    double lastBpm = 120;

protected:
    float sm[32] {}; bool smInit = false;
    void smooth (float* v, int n, bool snap)
    {
        if (snap || ! smInit) { std::copy (v, v + n, sm); smInit = true; }
        else for (int i = 0; i < n; ++i) sm[i] += 0.35f * (v[i] - sm[i]);
        std::copy (sm, sm + n, v);
    }
    void render (const te::PluginRenderContext& fc, bool keyAllowed, bool keyOnNow)
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = keyAllowed && keyOnNow && b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk;
        lastBpm = bpmAt (fc);
        proc.process (ch, nch, haveKey ? key : nullptr, n, lastBpm);
        float po = 0; for (int c = 0; c < nch; ++c) po = juce::jmax (po, b.getMagnitude (c, s0, n)); levelOut = po;
        liveTimeMs = (float) proc.currentTimeMs (0); liveDuckDb = proc.duckGainDb();
    }
};

//==============================================================================
class SqWingbeatPlugin : public SqWingbeatBase
{
public:
    enum P { time, timeR, feedback, mix, saturation, groove, feel, width, offset, accent, warp, lowCut, highCut, wow, flutter, diffusion, duck, duckRelease, input, output, numParams };
    SQ_COLOUR_IDENTITY (SqWingbeatPlugin, "Wingbeat Echo", "sqmWingbeat", "Wingbeat")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "time",        "Time",          1.0f, 30000.0f, 375.0f, 0.22f, " ms", 1 },
            { "timeR",       "Time R",        1.0f, 30000.0f, 500.0f, 0.22f, " ms", 1 },
            { "feedback",    "Feedback",      0.0f, 110.0f,  35.0f, 1.0f, " %", 0 },
            { "mix",         "Mix",           0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "saturation",  "Saturation",    0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "groove",      "Groove",      -50.0f, 50.0f,    0.0f, 1.0f, " %", 0 },
            { "feel",        "Feel",        -10.0f, 10.0f,    0.0f, 1.0f, " ms", 1 },
            { "width",       "Width",         0.0f, 150.0f, 100.0f, 1.0f, " %", 0 },
            { "offset",      "L/R Offset",    0.0f, 25.0f,    0.0f, 1.0f, " ms", 1 },
            { "accent",      "Accent",        0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "warp",        "Warp",          0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "lowCut",      "Low Cut",      20.0f, 2000.0f, 20.0f, 0.35f, " Hz", 0 },
            { "highCut",     "High Cut",    500.0f, 20000.0f, 20000.0f, 0.4f, " Hz", 0 },
            { "wow",         "Wow",           0.0f, 200.0f, 100.0f, 1.0f, " %", 0 },
            { "flutter",     "Flutter",       0.0f, 200.0f, 100.0f, 1.0f, " %", 0 },
            { "diffusion",   "Diffusion",     0.0f, 100.0f,   0.0f, 1.0f, " %", 0 },
            { "duck",        "Duck",          0.0f, 30.0f,    0.0f, 1.0f, " dB", 1 },
            { "duckRelease", "Duck Release", 20.0f, 2000.0f, 250.0f, 0.4f, " ms", 0 },
            { "input",       "Input",       -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 },
            { "output",      "Output",      -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray modeNames()   { return { "Single", "Dual", "Ping-Pong", "Rhythm" }; }
    static juce::StringArray styleNames()  { juce::StringArray a; for (int i = 0; i < Wingbeat::numStyles(); ++i) a.add (Wingbeat::style (i).name); return a; }
    static juce::StringArray syncNames()   { juce::StringArray a { "Free (ms)" }; for (int i = 0; i < Eng::WingMod::numSyncDivisions(); ++i) a.add (Eng::WingMod::syncName (i)); return a; }
    static juce::StringArray changeNames() { return { "Tape Glide", "Crossfade", "Jump" }; }
    static juce::StringArray headNames()   { return { "Head 1", "Head 2", "Heads 1+2", "Head 3", "Heads 1+3", "Heads 2+3", "All Three" }; }
    static juce::StringArray limitNames()  { return { "Off", "Soft", "Hard" }; }
    static juce::StringArray patternNames() { juce::StringArray a; for (auto& p : Wingbeat::patterns()) a.add (p.name); return a; }

    SqWingbeatPlugin (te::PluginCreationInfo info) : SqWingbeatBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        mode.referTo (state, "mode", um, 0); style.referTo (state, "style", um, 0); syncL.referTo (state, "syncL", um, 0); syncR.referTo (state, "syncR", um, 0);
        timeChange.referTo (state, "timeChange", um, 0); heads.referTo (state, "heads", um, 1); limiter.referTo (state, "limiter", um, 1); pattern.referTo (state, "pattern", um, 0);
        freeze.referTo (state, "freezeOn", um, false); prime.referTo (state, "primeOn", um, false); keyOn.referTo (state, "keyOn", um, false);
        tapData.referTo (state, "tapData", um, {}); tapLength.referTo (state, "tapLength", um, 16.0f);
    }
    ~SqWingbeatPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> mode, style, syncL, syncR, timeChange, heads, limiter, pattern;   // syncL/R: 0 = free, 1.. = WingMod division + 1
    juce::CachedValue<bool> freeze, prime, keyOn;
    juce::CachedValue<juce::String> tapData; juce::CachedValue<float> tapLength;
    void resetProps (juce::UndoManager* um) override
    {
        mode.setValue (0, um); style.setValue (0, um); syncL.setValue (0, um); syncR.setValue (0, um); timeChange.setValue (0, um); heads.setValue (1, um);
        limiter.setValue (1, um); pattern.setValue (0, um); freeze.setValue (false, um); prime.setValue (false, um); keyOn.setValue (false, um);
        tapData.setValue ({}, um); tapLength.setValue (16.0f, um);
    }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override
    {
        te::Plugin::getChannelNames (ins, outs);
        if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); }
    }
    /** Custom rhythm taps as text: "sixteenths:level:pan:cutoffHz:pitchSemis;..." (empty = the chosen pattern). */
    static std::vector<Wingbeat::Tap> parseTaps (const juce::String& s)
    {
        std::vector<Wingbeat::Tap> v;
        for (auto& t : juce::StringArray::fromTokens (s, ";", ""))
        {
            auto f = juce::StringArray::fromTokens (t, ":", "");
            if (f.size() < 1 || f[0].trim().isEmpty()) continue;
            v.push_back ({ f[0].getFloatValue(), f.size() > 1 ? f[1].getFloatValue() : 0.7f, f.size() > 2 ? f[2].getFloatValue() : 0.0f,
                           f.size() > 3 ? f[3].getFloatValue() : 14000.0f, f.size() > 4 ? f[4].getFloatValue() : 0.0f });
            if (v.size() >= 32) break;
        }
        return v;
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        readParams (true);
        proc.prepare (sr, juce::jmax (64, info.blockSizeSamples));
        readParams (true);
        proc.configure (edit.tempoSequence.getBpmAt (info.startTime), true);
    }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        readParams (false);
        render (fc, true, keyOn.get());
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && id == juce::Identifier ("tapData")) parseIntoPending();    // message thread: parse here, never on the audio thread
        SqWingbeatBase::valueTreePropertyChanged (v, id);
    }

private:
    std::array<Wingbeat::Tap, 32> pending {}; std::atomic<int> pendingCount { 0 }; std::atomic<bool> pendingNew { true };
    void parseIntoPending()
    {
        auto t = parseTaps (tapData.get());
        const int n = juce::jmin (32, (int) t.size());
        for (int i = 0; i < n; ++i) pending[(size_t) i] = t[(size_t) i];
        pendingCount = n; pendingNew = true;
    }
    void readParams (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smooth (v, numParams, snap);
        auto& p = proc.p;
        p.mode = juce::jlimit (0, 3, mode.get()); p.style = juce::jlimit (0, Wingbeat::numStyles() - 1, style.get());
        p.timeMs = v[time]; p.timeRMs = v[timeR]; p.syncL = syncL.get() - 1; p.syncR = syncR.get() - 1;
        p.feedback = v[feedback] / 100.0f; p.mix = v[mix] / 100.0f; p.saturation = v[saturation] / 100.0f; p.groove = v[groove] / 100.0f; p.feelMs = v[feel];
        p.width = v[width] / 100.0f; p.offsetMs = v[offset]; p.accent = v[accent] / 100.0f; p.warp = v[warp] / 100.0f; p.lowCut = v[lowCut]; p.highCut = v[highCut];
        p.wow = v[wow] / 100.0f; p.flutter = v[flutter] / 100.0f; p.diffusion = v[diffusion] / 100.0f; p.duckDb = v[duck]; p.duckReleaseMs = v[duckRelease];
        p.inputDb = v[input]; p.outputDb = v[output];
        p.timeChange = juce::jlimit (0, 2, timeChange.get()); p.heads = juce::jlimit (1, 7, heads.get()); p.limiter = juce::jlimit (0, 2, limiter.get());
        p.pattern = juce::jlimit (0, (int) Wingbeat::patterns().size() - 1, pattern.get()); p.freeze = freeze.get(); p.prime = prime.get(); p.keyOn = keyOn.get();
        if (snap) parseIntoPending();
        if (pendingNew.exchange (false))
        {
            p.customTaps.reserve (32);                                   // capacity 32: assign below never reallocates after the first time
            p.customTaps.assign (pending.begin(), pending.begin() + pendingCount.load());
        }
        p.customLength = juce::jmax (1.0f, tapLength.get());
    }
};

//==============================================================================
/** Wingbeat Jr.: eight styles, Normal / Wide / Ping-Pong, glide or stretch on time change. */
class SqWingbeatJrPlugin : public SqWingbeatBase
{
public:
    enum P { time, feedback, mix, saturation, tone, output, numParams };
    SQ_COLOUR_IDENTITY (SqWingbeatJrPlugin, "Wingbeat Jr.", "sqmWingbeatJr", "Wingbeat Jr")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "time",       "Time",       1.0f, 4000.0f, 375.0f, 0.3f, " ms", 1 },
            { "feedback",   "Feedback",   0.0f, 100.0f,  35.0f, 1.0f, " %", 0 },
            { "mix",        "Mix",        0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "saturation", "Saturation", 0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "tone",       "Tone",     500.0f, 20000.0f, 20000.0f, 0.4f, " Hz", 0 },
            { "output",     "Output",   -24.0f, 12.0f,    0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static const std::array<int, 8>& styleMap()   { static const std::array<int, 8> m { 1, 3, 4, 5, 7, 14, 10, 30 }; return m; }
    static juce::StringArray styleNames()  { juce::StringArray a; for (int i : styleMap()) a.add (Wingbeat::style (i).name); return a; }
    static juce::StringArray modeNames()   { return { "Normal", "Wide", "Ping-Pong" }; }
    static juce::StringArray changeNames() { return { "Glide", "Stretch" }; }

    SqWingbeatJrPlugin (te::PluginCreationInfo info) : SqWingbeatBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        style.referTo (state, "style", um, 0); mode.referTo (state, "mode", um, 0); sync.referTo (state, "sync", um, 0); change.referTo (state, "timeChange", um, 0);
    }
    ~SqWingbeatJrPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> style, mode, sync, change;
    void resetProps (juce::UndoManager* um) override { style.setValue (0, um); mode.setValue (0, um); sync.setValue (0, um); change.setValue (0, um); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        readParams (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); readParams (true);
        proc.configure (edit.tempoSequence.getBpmAt (info.startTime), true);
    }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override   { readParams (false); render (fc, false, false); }

private:
    void readParams (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smooth (v, numParams, snap);
        auto& p = proc.p;
        const int m = juce::jlimit (0, 2, mode.get());
        p.mode = m == 2 ? Wingbeat::pingPong : (m == 1 ? Wingbeat::dual : Wingbeat::single);
        p.style = styleMap()[(size_t) juce::jlimit (0, 7, style.get())];
        p.timeMs = v[time]; p.timeRMs = v[time]; p.offsetMs = m == 1 ? 17.0f : 0.0f; p.width = m == 1 ? 1.3f : 1.0f;
        p.syncL = p.syncR = sync.get() - 1;
        p.feedback = v[feedback] / 100.0f; p.mix = v[mix] / 100.0f; p.saturation = v[saturation] / 100.0f; p.highCut = v[tone]; p.outputDb = v[output];
        p.timeChange = change.get() == 1 ? Wingbeat::crossfadeChange : Wingbeat::glideChange; p.limiter = Wingbeat::limitSoft;
    }
};
