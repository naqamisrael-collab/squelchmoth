#pragma once
// Atrium Verb (1.14 "Studio Line", phase D): the flagship algorithmic reverb. DSP: SqAtriumDsp.h (tests/atrium_test.cpp).
// Face: SkinsAtrium.inl. Claude: `atrium_verb` (McpToolsAtrium.h).

#include "SqColour.h"
#include "SqAtriumDsp.h"

class SqAtriumVerbPlugin : public SqColourBase
{
public:
    enum P { decay, size, predelay, diffusion, width, character, lowMult, highMult, lowX, highX, early, late,
             duck, duckThresh, duckRelease, lowCut, highCut, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqAtriumVerbPlugin, "Atrium Verb", "sqmAtriumVerb", "Atrium")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "decay",       "Decay",          0.1f,  30.0f,   2.5f, 0.3f,  " s", 2 },
            { "size",        "Size",           0.0f,  100.0f,  60.0f, 1.0f, " %", 0 },
            { "predelay",    "Predelay",       0.0f,  500.0f,  10.0f, 0.4f, " ms", 1 },
            { "diffusion",   "Diffusion",      0.0f,  100.0f,  85.0f, 1.0f, " %", 0 },
            { "width",       "Width",          0.0f,  200.0f, 100.0f, 1.0f, " %", 0 },
            { "character",   "Character",      0.0f,  100.0f,  40.0f, 1.0f, " %", 0 },
            { "lowMult",     "Low Decay",      0.25f, 4.0f,    1.2f, 0.5f,  " x", 2 },
            { "highMult",    "High Decay",     0.1f,  2.0f,    0.6f, 0.6f,  " x", 2 },
            { "lowX",        "Low Crossover",  50.0f, 1000.0f, 250.0f, 0.4f, " Hz", 0 },
            { "highX",       "High Crossover", 1000.0f, 16000.0f, 4000.0f, 0.4f, " Hz", 0 },
            { "early",       "Early",          0.0f,  200.0f, 100.0f, 1.0f, " %", 0 },
            { "late",        "Late",           0.0f,  200.0f, 100.0f, 1.0f, " %", 0 },
            { "duck",        "Ducking",        0.0f,  30.0f,   0.0f, 1.0f,  " dB", 1 },
            { "duckThresh",  "Duck Threshold", -60.0f, 0.0f, -30.0f, 1.0f,  " dB", 1 },
            { "duckRelease", "Duck Release",   20.0f, 2000.0f, 250.0f, 0.4f, " ms", 0 },
            { "lowCut",      "Low Cut",        10.0f, 1000.0f, 10.0f, 0.35f, " Hz", 0 },
            { "highCut",     "High Cut",       1000.0f, 20000.0f, 20000.0f, 0.4f, " Hz", 0 },
            { "mix",         "Mix",            0.0f,  100.0f,  30.0f, 1.0f, " %", 0 },
            { "output",      "Output",        -24.0f, 12.0f,   0.0f, 1.0f,  " dB", 1 } };
        return s;
    }

    static juce::StringArray spaceNames() { juce::StringArray a; for (auto& s : SqAtrium::spaces()) a.add (s.name); return a; }
    static juce::StringArray syncNames() { return { "Free", "1/64", "1/32", "1/16", "1/8", "1/4", "1/16 dotted", "1/8 dotted", "1/16 triplet", "1/8 triplet" }; }
    static double syncBeats (int i) { static const double b[10] = { 0, 1.0 / 16, 1.0 / 8, 0.25, 0.5, 1.0, 0.375, 0.75, 1.0 / 6, 1.0 / 3 }; return b[juce::jlimit (0, 9, i)]; }

    juce::CachedValue<int> space, predelaySync;
    juce::CachedValue<bool> freeze;

    SqAtriumVerbPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        space.referTo (state, "space", um, 0); predelaySync.referTo (state, "predelaySync", um, 0);
        freeze.referTo (state, "freeze", um, false);
        histIn.fill (-100.0f); histWet.fill (-100.0f); histDuck.fill (0.0f);
    }
    ~SqAtriumVerbPlugin() override { notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        space.setValue (0, um); predelaySync.setValue (0, um); freeze.setValue (false, um);
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        fill (engine.p); engine.prepare (sr, juce::jmax (64, info.blockSizeSamples));
        { const juce::ScopedLock sl (faceLock); fill (face.p); face.prepare (sr, 64); }
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
        histIn[(size_t) (w & (histLen - 1))] = engine.meterInDb;
        histWet[(size_t) (w & (histLen - 1))] = engine.meterWetDb;
        histDuck[(size_t) (w & (histLen - 1))] = engine.meterDuckDb;
        histW = w + 1;
        duckNow = engine.meterDuckDb;
    }

    /** The predelay actually used (ms): the knob, or the synced division at the current tempo. */
    double predelayMs() const
    {
        const int s = predelaySync.get();
        if (s <= 0) return bank[predelay];
        return juce::jlimit (0.0, 500.0, syncBeats (s) * 60000.0 / juce::jmax (20.0, bpm.load()));
    }

    /** The face asks the design questions (decay per frequency, early taps) of a copy that never runs audio. */
    double decayAt (double hz) { const juce::ScopedLock sl (faceLock); fill (face.p); face.prepareIfNeeded (sr); return face.decayAtHz (hz); }
    std::vector<SqAtrium::Tap> taps() { const juce::ScopedLock sl (faceLock); fill (face.p); face.prepareIfNeeded (sr); return face.currentTaps(); }

    static constexpr int histLen = 512;
    std::array<float, histLen> histIn {}, histWet {}, histDuck {};
    std::atomic<int> histW { 0 };
    std::atomic<float> duckNow { 0 };
    std::atomic<double> bpm { 120.0 };

private:
    struct FaceEngine : public SqAtrium::Engine
    {
        double preparedAt = 0;
        void prepareIfNeeded (double fs) { if (preparedAt != fs) { prepare (fs, 64); preparedAt = fs; } else refresh(); }
    };
    SqAtrium::Engine engine; FaceEngine face; juce::CriticalSection faceLock;

    void fill (SqAtrium::Params& p) const
    {
        p.space = juce::jlimit (0, (int) SqAtrium::spaces().size() - 1, space.get());
        p.decaySec = bank[decay]; p.size = bank[size] / 100.0; p.predelayMs = predelayMs(); p.diffusion = bank[diffusion] / 100.0;
        p.width = bank[width] / 100.0; p.character = bank[character] / 100.0; p.lowMult = bank[lowMult]; p.highMult = bank[highMult];
        p.lowXHz = bank[lowX]; p.highXHz = bank[highX]; p.early = bank[early] / 100.0; p.late = bank[late] / 100.0; p.freeze = freeze.get();
        p.duckDb = bank[duck]; p.duckThreshDb = bank[duckThresh]; p.duckReleaseMs = bank[duckRelease];
        p.lowCutHz = bank[lowCut]; p.highCutHz = bank[highCut]; p.mix = bank[mix] / 100.0; p.outputDb = bank[output];
    }
};
