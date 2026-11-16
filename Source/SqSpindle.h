#pragma once
// Spindle Delay (1.14 "Studio Line", phase I): the delay flagship. DSP: SqSpindleDsp.h (tests/spindle_test.cpp).
// Face: SkinsSpindle.inl. Claude: `spindle_delay` (McpToolsSpindle.h).

#include "SqColour.h"
#include "SqSpindleDsp.h"

class SqSpindleDelayPlugin : public SqColourBase
{
public:
    enum P { timeL, timeR, feedback, crossFeed, lowCut, highCut, drive, wow, flutter, diffusion, width, duck, duckRelease, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqSpindleDelayPlugin, "Spindle Delay", "sqmSpindle", "Spindle")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "timeL",       "Time Left",   1.0f, 4000.0f, 375.0f, 0.35f, " ms", 1 },
            { "timeR",       "Time Right",  1.0f, 4000.0f, 375.0f, 0.35f, " ms", 1 },
            { "feedback",    "Feedback",    0.0f, 110.0f, 45.0f, 1.0f, " %", 0 },
            { "crossFeed",   "Cross Feed",  0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "lowCut",      "Low Cut",    10.0f, 2000.0f, 80.0f, 0.35f, " Hz", 0 },
            { "highCut",     "High Cut",  500.0f, 20000.0f, 9000.0f, 0.35f, " Hz", 0 },
            { "drive",       "Drive",       0.0f, 100.0f, 20.0f, 1.0f, " %", 0 },
            { "wow",         "Wow",         0.0f, 100.0f, 35.0f, 1.0f, " %", 0 },
            { "flutter",     "Flutter",     0.0f, 100.0f, 25.0f, 1.0f, " %", 0 },
            { "diffusion",   "Diffusion",   0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "width",       "Width",       0.0f, 200.0f, 100.0f, 1.0f, " %", 0 },
            { "duck",        "Ducking",     0.0f, 30.0f, 0.0f, 1.0f, " dB", 1 },
            { "duckRelease", "Duck Release", 20.0f, 2000.0f, 300.0f, 0.4f, " ms", 0 },
            { "mix",         "Mix",         0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "output",      "Output",    -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray characterNames() { return { "Digital", "Tape", "Analog", "Diffuse" }; }
    static juce::StringArray routingNames()   { return { "Normal", "Ping-Pong", "Cross" }; }
    static juce::StringArray syncNames()      { return { "Free", "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar", "1/16 dotted", "1/8 dotted", "1/4 dotted", "1/16 triplet", "1/8 triplet", "1/4 triplet" }; }
    static double syncBeats (int i) { static const double b[13] = { 0, 0.125, 0.25, 0.5, 1, 2, 4, 0.375, 0.75, 1.5, 1.0 / 6, 1.0 / 3, 2.0 / 3 }; return b[juce::jlimit (0, 12, i)]; }

    juce::CachedValue<int> character, routing, syncL, syncR;
    juce::CachedValue<bool> linkLR, freeze;

    SqSpindleDelayPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        character.referTo (state, "character", um, 1); routing.referTo (state, "routing", um, 0);
        syncL.referTo (state, "syncL", um, 3); syncR.referTo (state, "syncR", um, 3);
        linkLR.referTo (state, "linkLR", um, true); freeze.referTo (state, "freeze", um, false);
        histIn.fill (-100.0f); histWet.fill (-100.0f);
    }
    ~SqSpindleDelayPlugin() override { notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        character.setValue (1, um); routing.setValue (0, um); syncL.setValue (3, um); syncR.setValue (3, um); linkLR.setValue (true, um); freeze.setValue (false, um);
    }

    /** The time actually used (ms): the knob, or the synced division at the current tempo. */
    double timeMs (int side) const
    {
        const int s = side == 0 || linkLR.get() ? syncL.get() : syncR.get();
        if (s > 0) return juce::jlimit (1.0, 4000.0, syncBeats (s) * 60000.0 / juce::jmax (20.0, bpm.load()));
        return side == 0 || linkLR.get() ? bank[timeL] : bank[timeR];
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
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk; }   // the face's IN meter
        engine.process (ch, nch, n);
        { float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelOut = pk; }  // and OUT
        const int w = histW.load();
        histIn[(size_t) (w & (histLen - 1))] = engine.meterInDb; histWet[(size_t) (w & (histLen - 1))] = engine.meterWetDb;
        histW = w + 1;
        duckNow = engine.meterDuckDb; liveL = engine.meterTimeLMs; liveR = engine.meterTimeRMs;
    }

    static constexpr int histLen = 512;
    std::array<float, histLen> histIn {}, histWet {};
    std::atomic<int> histW { 0 };
    std::atomic<float> duckNow { 0 }, liveL { 0 }, liveR { 0 };
    std::atomic<double> bpm { 120.0 };

private:
    SqSpindle::Engine engine;
    void fill (SqSpindle::Params& p) const
    {
        p.character = juce::jlimit (0, 3, character.get()); p.routing = juce::jlimit (0, 2, routing.get());
        p.timeLMs = timeMs (0); p.timeRMs = timeMs (1);
        p.feedback = bank[feedback] / 100.0; p.crossFeed = bank[crossFeed] / 100.0; p.lowCutHz = bank[lowCut]; p.highCutHz = bank[highCut];
        p.drive = bank[drive] / 100.0; p.wow = bank[wow] / 100.0; p.flutter = bank[flutter] / 100.0; p.diffusion = bank[diffusion] / 100.0;
        p.width = bank[width] / 100.0; p.duckDb = bank[duck]; p.duckReleaseMs = bank[duckRelease]; p.mix = bank[mix] / 100.0; p.outputDb = bank[output];
        p.freeze = freeze.get();
    }
};
