#pragma once
// Torque Comp (1.14 "Studio Line", phase B): the flagship compressor. DSP: SqTorqueDsp.h (tests/torque_test.cpp).
// Face: SkinsTorque.inl. Claude: `torque_comp` (McpToolsTorque.h).

#include "SqColour.h"
#include "SqTorqueDsp.h"

class SqTorqueCompPlugin : public SqColourBase, private juce::AsyncUpdater
{
public:
    enum P { threshold, ratio, knee, range, attack, release, hold, lookahead, link, drive, mix, makeup, output,
             k1Freq, k1Gain, k1Q, k2Freq, k2Gain, k2Q, k3Freq, k3Gain, k3Q, k4Freq, k4Gain, k4Q, k5Freq, k5Gain, k5Q, k6Freq, k6Gain, k6Q, numParams };
    SQ_COLOUR_IDENTITY (SqTorqueCompPlugin, "Torque Comp", "sqmTorqueComp", "Torque")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s = [] {
            std::vector<SqDsp::ParamSpec> v {
                { "threshold", "Threshold", -60.0f, 0.0f,   -18.0f, 1.0f,  " dB", 1 },
                { "ratio",     "Ratio",       1.0f, 100.0f,   4.0f, 0.3f,  ":1", 2 },
                { "knee",      "Knee",        0.0f, 72.0f,    6.0f, 0.5f,  " dB", 1 },
                { "range",     "Range",       0.0f, 60.0f,   60.0f, 1.0f,  " dB", 1 },
                { "attack",    "Attack",      0.005f, 250.0f, 10.0f, 0.25f, " ms", 3 },
                { "release",   "Release",     5.0f, 5000.0f, 120.0f, 0.3f,  " ms", 0 },
                { "hold",      "Hold",        0.0f, 500.0f,   0.0f, 0.4f,  " ms", 0 },
                { "lookahead", "Lookahead",   0.0f, 20.0f,    0.0f, 0.6f,  " ms", 2 },
                { "link",      "Stereo Link", 0.0f, 100.0f, 100.0f, 1.0f,  " %", 0 },
                { "drive",     "Drive",       0.0f, 24.0f,    0.0f, 1.0f,  " dB", 1 },
                { "mix",       "Mix",         0.0f, 200.0f, 100.0f, 1.0f,  " %", 0 },
                { "makeup",    "Makeup",    -24.0f, 36.0f,    0.0f, 1.0f,  " dB", 1 },
                { "output",    "Output",    -36.0f, 36.0f,    0.0f, 1.0f,  " dB", 1 } };
            static std::vector<std::string> keep; keep.reserve (40);
            auto c = [] (const juce::String& t) { keep.push_back (t.toStdString()); return keep.back().c_str(); };
            for (int b = 1; b <= 6; ++b)
            {
                const auto n = juce::String (b);
                v.push_back ({ c ("k" + n + "Freq"), c ("Key EQ " + n + " Freq"), 10.0f, 30000.0f, (float) (60.0 * std::pow (2.5, b - 1)), 0.18f, " Hz", 0 });
                v.push_back ({ c ("k" + n + "Gain"), c ("Key EQ " + n + " Gain"), -30.0f, 30.0f, 0.0f, 1.0f, " dB", 1 });
                v.push_back ({ c ("k" + n + "Q"),    c ("Key EQ " + n + " Q"),    0.025f, 40.0f, 0.707f, 0.25f, "", 3 });
            }
            return v;
        }();
        return s;
    }

    juce::CachedValue<int> style, keySource, routing, characterMode, oversample, tempoDiv;
    juce::CachedValue<bool> autoRelease, autoThreshold, autoGain, characterPost, audition;
    struct KeyBandProps { juce::CachedValue<bool> on; juce::CachedValue<int> shape, slope; };
    std::array<KeyBandProps, 6> keyProps;

    SqTorqueCompPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        style.referTo (state, "style", um, 0); keySource.referTo (state, "keySource", um, 0); routing.referTo (state, "routing", um, 0);
        characterMode.referTo (state, "character", um, 0); oversample.referTo (state, "oversample", um, 2); tempoDiv.referTo (state, "tempoDiv", um, 2);
        autoRelease.referTo (state, "autoRelease", um, false); autoThreshold.referTo (state, "autoThreshold", um, false); autoGain.referTo (state, "autoGain", um, false);
        characterPost.referTo (state, "characterPost", um, true); audition.referTo (state, "audition", um, false);
        for (int b = 0; b < 6; ++b)
        {
            const auto n = juce::String (b + 1);
            keyProps[(size_t) b].on.referTo (state, "k" + n + "On", um, false); keyProps[(size_t) b].shape.referTo (state, "k" + n + "Shape", um, b == 0 ? 4 : 0); keyProps[(size_t) b].slope.referTo (state, "k" + n + "Slope", um, 12);
        }
    }
    ~SqTorqueCompPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        style.setValue (0, um); keySource.setValue (0, um); routing.setValue (0, um); characterMode.setValue (0, um); oversample.setValue (2, um); tempoDiv.setValue (2, um);
        autoRelease.setValue (false, um); autoThreshold.setValue (false, um); autoGain.setValue (false, um); characterPost.setValue (true, um); audition.setValue (false, um);
        for (int b = 0; b < 6; ++b) { keyProps[(size_t) b].on.setValue (false, um); keyProps[(size_t) b].shape.setValue (b == 0 ? 4 : 0, um); keyProps[(size_t) b].slope.setValue (12, um); }
    }

    static juce::StringArray styleNames() { juce::StringArray a; for (auto& s : SqTorque::styles()) a.add (s.name); return a; }
    static juce::StringArray characterNames() { return { "Off", "Style", "Warm", "Bright", "Iron" }; }
    static juce::StringArray oversampleNames() { return { "1x", "2x", "4x", "8x", "16x", "32x" }; }
    static juce::StringArray tempoNames() { return { "1/16", "1/8", "1/4", "1/2", "1 bar" }; }
    static double tempoBeats (int i) { static const double b[5] = { 0.25, 0.5, 1.0, 2.0, 4.0 }; return b[juce::jlimit (0, 4, i)]; }

    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); } }
    bool takesMidiInput() override { return true; }
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
        lastLatency = engine.latency();
    }
    void deinitialise() override {}
    void reset() override { engine.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        fill (engine.p);
        if (engine.latency() != lastLatency) { lastLatency = engine.latency(); triggerAsyncUpdate(); }   // the engine re-plans with the new delay
        // MIDI key: note-ons become trigger pulses
        if (fc.bufferForMidiMessages != nullptr)
            for (auto& m : *fc.bufferForMidiMessages)
                if (m.isNoteOn())
                {
                    const int at = juce::jlimit (0, fc.bufferNumSamples - 1, (int) std::lround ((m.getTimeStamp() - fc.midiBufferOffset) * sr));
                    engine.midiTrigger (at, m.getFloatVelocity());
                }
        const double bpm = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
        engine.setTempo (edit.tempoSequence.toBeats (fc.editTime.getStart()).inBeats(), bpm, fc.isPlaying);
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        engine.process (ch, nch, n, haveKey ? key : nullptr, haveKey ? 2 : 0);
        // meters and the history the face draws
        levelIn = (float) SqTorque::dbToGain (engine.meterInDb); levelOut = (float) SqTorque::dbToGain (engine.meterOutDb);
        grNow = engine.meterGrDb; inNow = engine.meterInDb; outNow = engine.meterOutDb; lufsNow = engine.momentaryLufs; autoGainNow = engine.autoGainDb; autoThreshNow = engine.autoThreshDb;
        const int w = histW.load();
        histGr[(size_t) (w & (histLen - 1))] = engine.meterGrDb; histIn[(size_t) (w & (histLen - 1))] = engine.meterInDb; histOut[(size_t) (w & (histLen - 1))] = engine.meterOutDb;
        histW = w + 1;
    }

    /** The static curve the face and Claude show: output level for an input level (dB). */
    double curveOutDb (double inDb) const
    {
        const auto& st = SqTorque::styles()[(size_t) juce::jlimit (0, 13, style.get())];
        const double T = autoThreshold.get() ? autoThreshNow.load() : bank[threshold];
        const double depth = std::max (1.0, bank[mix] / 100.0);
        return inDb + depth * SqTorque::staticGainDb (inDb, T, bank[ratio], bank[knee] + st.kneeExtra, bank[range], st.upward, st.muLaw) + bank[makeup] + (autoGain.get() ? autoGainNow.load() : 0.0f);
    }

    static constexpr int histLen = 1024;
    std::array<float, histLen> histGr {}, histIn {}, histOut {};
    std::atomic<int> histW { 0 };
    std::atomic<float> grNow { 0 }, inNow { -100 }, outNow { -100 }, lufsNow { -100 }, autoGainNow { 0 }, autoThreshNow { -18 };
    double rate() const { return sr; }

private:
    SqTorque::Engine engine, probe;
    int lastLatency = 0; bool probeReady = false;
    void fill (SqTorque::Params& p) const
    {
        p.style = juce::jlimit (0, 13, style.get()); p.threshDb = bank[threshold]; p.ratio = bank[ratio]; p.kneeDb = bank[knee]; p.rangeDb = bank[range];
        p.attackMs = bank[attack]; p.releaseMs = bank[release]; p.holdMs = bank[hold]; p.lookaheadMs = bank[lookahead]; p.link = bank[link] / 100.0;
        p.driveDb = bank[drive]; p.mix = bank[mix] / 100.0; p.makeupDb = bank[makeup]; p.outputDb = bank[output];
        p.autoRelease = autoRelease.get(); p.autoThreshold = autoThreshold.get(); p.autoGain = autoGain.get();
        p.key = (SqTorque::Key) juce::jlimit (0, 3, keySource.get()); p.routing = (SqTorque::Routing) juce::jlimit (0, 4, routing.get());
        p.characterMode = juce::jlimit (0, 4, characterMode.get()); p.characterPost = characterPost.get(); p.oversampleLog2 = juce::jlimit (0, 5, oversample.get());
        p.audition = audition.get(); p.tempoDivBeats = tempoBeats (tempoDiv.get());
        for (int b = 0; b < 6; ++b)
        {
            auto& k = p.keyEq[(size_t) b]; const auto& kp = keyProps[(size_t) b];
            k.enabled = kp.on.get(); k.shape = (SqSpectra::Shape) juce::jlimit (0, 9, kp.shape.get()); k.slope = kp.slope.get();
            k.freq = bank[(size_t) (k1Freq + b * 3)]; k.gainDb = bank[(size_t) (k1Gain + b * 3)]; k.q = bank[(size_t) (k1Q + b * 3)];
        }
    }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
};
