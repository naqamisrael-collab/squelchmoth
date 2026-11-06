#pragma once
// Moth Tune - Squelchmoth's flagship vocal tuner (original design and code).
//
// One plug-in with everything: automatic correction (Retune Speed, Humanize, Flex Tune, Classic / Modern),
// Vibrato Keep (corrects the centre of a vibrato, never flattens it), Natural Vibrato, Create Vibrato, Formant and
// Throat, Transpose with Conform to Scale, Detune / reference, 16 scales + Custom with per-note Remove / Bypass,
// Auto-Key from the session, MIDI target notes (from this track or ANY MIDI track - no routing), four harmony
// voices (interval, scale-step or MIDI-note driven), and Graph notes that override the automatic mode where they
// exist - in BOTH engines:
//   Live   - cycle splice, ~0.7 ms, formants handled by an LPC envelope stage
//   Studio - PSOLA with peak-aligned marks (formant-exact), latency = 3 periods of the lowest note of the voice type

#include "SqVocalDsp.h"
#include "SqInstruments.h"
#include "Scales.h"

//==============================================================================
//==============================================================================
class SqTunePlugin : public te::Plugin,
                     public SqDsp::BankOwner,
                     private juce::Timer,
                     private juce::AsyncUpdater
{
public:
    enum P { retune, humanize, flex, vibKeep, natVib, strength, transpose, formant, throat, detune, tracking, mix, output,
             vibRate, vibDepth, vibDelay, vibOnset, vibVar,
             h1Level, h1Pan, h1Interval, h1Formant, h2Level, h2Pan, h2Interval, h2Formant,
             h3Level, h3Pan, h3Interval, h3Formant, h4Level, h4Pan, h4Interval, h4Formant,
             harmDetune, harmTiming, numParams };

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;   // ParamSpec keeps const char* - these live for the whole run
        static const std::vector<SqDsp::ParamSpec> specs = []
        {
            std::vector<SqDsp::ParamSpec> specs {
            { "retune",    "Retune Speed",   0.0f, 400.0f, 20.0f, 0.35f, " ms", 0 },
            { "humanize",  "Humanize",       0.0f, 100.0f, 0.0f,  1.0f, " %", 0 },
            { "flex",      "Flex Tune",      0.0f, 100.0f, 0.0f,  1.0f, " %", 0 },
            { "vibKeep",   "Vibrato Keep",   0.0f, 100.0f, 100.0f,1.0f, " %", 0 },
            { "natVib",    "Natural Vibrato",-100.0f, 200.0f, 0.0f, 1.0f, " %", 0 },
            { "strength",  "Correction",     0.0f, 100.0f, 100.0f,1.0f, " %", 0 },
            { "transpose", "Transpose",      -24.0f, 24.0f, 0.0f, 1.0f, " st", 0 },
            { "formant",   "Formant",        -12.0f, 12.0f, 0.0f, 1.0f, " st", 1 },
            { "throat",    "Throat",         50.0f, 150.0f, 100.0f, 1.0f, " %", 0 },
            { "detune",    "Detune",         -100.0f, 100.0f, 0.0f, 1.0f, " ct", 0 },
            { "tracking",  "Tracking",       0.0f, 100.0f, 50.0f, 1.0f, "", 0 },
            { "mix",       "Mix",            0.0f, 100.0f, 100.0f,1.0f, " %", 0 },
            { "output",    "Output",         -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "vibRate",   "Vibrato Rate",   0.5f, 12.0f, 5.5f,   0.6f, " Hz", 2 },
            { "vibDepth",  "Vibrato Depth",  0.0f, 200.0f, 0.0f,  0.6f, " ct", 0 },
            { "vibDelay",  "Vibrato Delay",  0.0f, 2.0f, 0.3f,    0.6f, " s", 2 },
            { "vibOnset",  "Vibrato Onset",  0.0f, 2.0f, 0.4f,    0.6f, " s", 2 },
            { "vibVar",    "Vibrato Variation", 0.0f, 100.0f, 20.0f, 1.0f, " %", 0 } };
        for (int i = 1; i <= 4; ++i)
        {
            auto addS = [] (const std::string& s) { names.push_back (s); return names.back().c_str(); };
            const auto n = std::to_string (i);
            specs.push_back ({ addS ("h" + n + "Level"),    addS ("Voice " + n + " Level"),    -60.0f, 6.0f, -6.0f, 2.0f, " dB", 1 });
            specs.push_back ({ addS ("h" + n + "Pan"),      addS ("Voice " + n + " Pan"),      -100.0f, 100.0f, i == 1 ? -40.0f : i == 2 ? 40.0f : i == 3 ? -70.0f : 70.0f, 1.0f, "", 0 });
            specs.push_back ({ addS ("h" + n + "Interval"), addS ("Voice " + n + " Interval"), -24.0f, 24.0f, i == 1 ? 4.0f : i == 2 ? 7.0f : i == 3 ? -5.0f : 12.0f, 1.0f, "", 0 });
            specs.push_back ({ addS ("h" + n + "Formant"),  addS ("Voice " + n + " Formant"),  -12.0f, 12.0f, 0.0f, 1.0f, " st", 1 });
        }
        specs.push_back ({ "harmDetune", "Harmony Detune", 0.0f, 30.0f, 6.0f, 1.0f, " ct", 0 });
        specs.push_back ({ "harmTiming", "Harmony Timing", 0.0f, 40.0f, 12.0f, 1.0f, " ms", 0 });
            return specs;
        }();
        return specs;
    }

    SqTunePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        bank.create (*this, paramSpecs());

        auto um = getUndoManager();
        engine.referTo (state, "engine", um, 1);
        character.referTo (state, "character", um, 0);
        voiceType.referTo (state, "voiceType", um, 1);
        keyRoot.referTo (state, "keyRoot", um, 0);
        scaleIndex.referTo (state, "scale", um, 0);
        customMask.referTo (state, "customMask", um, 0xfff);
        removeMask.referTo (state, "removeMask", um, 0);
        bypassMask.referTo (state, "bypassMask", um, 0);
        midiMode.referTo (state, "midiMode", um, 0);
        midiTrack.referTo (state, "midiTrack", um, {});
        midiLead.referTo (state, "midiLead", um, true);
        allOctaves.referTo (state, "allOctaves", um, true);
        preserve.referTo (state, "formantPreserve", um, true);
        conform.referTo (state, "conform", um, false);
        vibShape.referTo (state, "vibShape", um, 0);
        autoKey.referTo (state, "autoKey", um, false);
        learnScale.referTo (state, "learnScale", um, false);
        graphOn.referTo (state, "graphOn", um, true);
        for (int i = 0; i < 4; ++i) harmMode[(size_t) i].referTo (state, juce::Identifier ("harmMode" + juce::String (i + 1)), um, 0);
        startTimerHz (5);
        rebuildGraph();
    }

    ~SqTunePlugin() override { stopTimer(); cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Moth Tune"); }
    static constexpr const char* xmlTypeName = "sqmTune";
    juce::String getName() const override               { return "Moth Tune"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Tune"; }
    juce::String getSelectableDescription() override    { return "Moth Tune vocal tuner"; }
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return true; }
    bool needsConstantBufferSize() override             { return false; }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    double getTailLength() const override               { return 0.1; }
    double getLatencySeconds() override                 { return (double) latencyFor (engine.get(), voiceType.get(), sampleRate) / juce::jmax (1.0, sampleRate); }

    static juce::StringArray engineNames()     { return { "LIVE", "STUDIO" }; }
    static juce::StringArray characterNames()  { return { "MODERN", "CLASSIC" }; }
    static juce::StringArray voiceNames()      { juce::StringArray s; for (auto& v : SqVocal::voiceTypes()) s.add (v.name); return s; }
    static juce::StringArray scaleNames()      { juce::StringArray s; for (auto& x : SqScales::scales()) s.add (x.name); s.add ("Custom"); return s; }
    static juce::StringArray harmModeNames()   { return { "OFF", "INTERVAL", "SCALE", "MIDI" }; }
    static juce::StringArray vibShapeNames()   { return { "SINE", "TRI", "SQUARE", "SAW" }; }
    static juce::StringArray midiModeNames()   { return { "OFF", "THIS TRACK", "NAMED TRACK" }; }

    static int latencyFor (int eng, int vt, double sr)   { return SqVocal::TuneProcessor::latencyFor (eng, vt, sr); }

    double currentRate() const   { return sr > 0 ? sr : 48000.0; }
    SqDsp::ParamBank bank;
    juce::CachedValue<int> engine, character, voiceType, keyRoot, scaleIndex, customMask, removeMask, bypassMask, midiMode, vibShape;
    juce::CachedValue<juce::String> midiTrack;
    juce::CachedValue<bool> allOctaves, preserve, conform, autoKey, learnScale, graphOn, midiLead;

    SqDsp::ParamBank& getBank() override   { return bank; }
    void resetProps (juce::UndoManager* um) override
    {
        engine.setValue (1, um); character.setValue (0, um); scaleIndex.setValue (0, um); removeMask.setValue (0, um); bypassMask.setValue (0, um);
        midiMode.setValue (0, um); midiLead.setValue (true, um); allOctaves.setValue (true, um); preserve.setValue (true, um); conform.setValue (false, um);
        vibShape.setValue (0, um); learnScale.setValue (false, um);
        for (auto& h : harmMode) h.setValue (0, um);
    }
    std::array<juce::CachedValue<int>, 4> harmMode;

    /** The pitch classes Moth Tune is allowed to correct to (scale or custom, minus removed notes). */
    int allowedMask() const
    {
        const int sc = scaleIndex.get();
        int m = sc >= (int) SqScales::scales().size() ? (customMask.get() & 0xfff) : SqScales::maskFor (keyRoot.get(), sc);
        return m & ~(removeMask.get() & 0xfff);
    }

    //==========================================================================
    // Display data for the editor (written on the audio thread, read by the UI)
    struct Disp { float in = -1, out = -1, target = -1; float h[4] { -1, -1, -1, -1 }; float corr = 0; };
    static constexpr int dispSize = 1024;
    std::array<Disp, dispSize> disp {};
    std::atomic<int> dispWrite { 0 };
    std::atomic<float> liveIn { -1 }, liveOut { -1 }, liveTarget { -1 }, liveCents { 0 }, liveLevel { 0 };
    std::atomic<int> heldMaskUi { 0 };

    //==========================================================================
    // Graph notes (Graph mode). Stored in the plugin state, published to the audio thread as a snapshot.
    static inline const juce::Identifier graphId { "GRAPH" }, noteId { "NOTE" };
    juce::ValueTree graphTree()   { auto g = state.getChildWithName (graphId); if (! g.isValid()) { g = juce::ValueTree (graphId); state.appendChild (g, nullptr); } return g; }
    std::vector<SqVocal::GraphNote> getGraphNotes() const
    {
        std::vector<SqVocal::GraphNote> v;
        auto g = state.getChildWithName (graphId);
        for (auto n : g)
        {
            SqVocal::GraphNote gn;
            gn.start = n["start"]; gn.end = n["end"]; gn.orig = (float) (double) n["orig"]; gn.pitch = (float) (double) n.getProperty ("pitch", n["orig"]);
            gn.retuneMs = (float) (double) n.getProperty ("retune", 20.0); gn.vibScale = (float) (double) n.getProperty ("vib", 1.0);
            gn.formantSt = (float) (double) n.getProperty ("formant", 0.0); gn.drift = (float) (double) n.getProperty ("drift", 1.0);
            if (gn.end > gn.start) v.push_back (gn);
        }
        std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.start < b.start; });
        return v;
    }
    void setGraphNotes (const std::vector<SqVocal::GraphNote>& notes, juce::UndoManager* um)
    {
        auto g = graphTree();
        g.removeAllChildren (um);
        for (auto& n : notes)
        {
            juce::ValueTree t (noteId);
            t.setProperty ("start", n.start, nullptr); t.setProperty ("end", n.end, nullptr); t.setProperty ("orig", n.orig, nullptr);
            t.setProperty ("pitch", n.pitch, nullptr); t.setProperty ("retune", n.retuneMs, nullptr); t.setProperty ("vib", n.vibScale, nullptr);
            t.setProperty ("formant", n.formantSt, nullptr); t.setProperty ("drift", n.drift, nullptr);
            g.appendChild (t, um);
        }
    }
    /** Seconds to add to the block's edit time to get the edit time of the audio arriving at the input. */
    std::atomic<double> inputTimeOffset { 0.0 };

    //==========================================================================
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        proc.prepare (sr, engine.get(), voiceType.get());
        proc.onFrame = [this] (const SqVocal::FrameResult& r, const std::array<bool, 128>& held) { pushDisplay (r, held); };
        heldIn.fill (false);
    }
    void deinitialise() override {}

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        auto g = v.getChildWithName (graphId);
        if (g.isValid()) { auto mine = graphTree(); mine.copyPropertiesAndChildrenFrom (g, nullptr); }
        for (auto ap : getAutomatableParameters()) ap->updateFromAttachedValue();
        rebuildGraph();
    }

    //==========================================================================
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& buf = *fc.destBuffer;
        const int nch = juce::jmin (2, buf.getNumChannels());
        if (nch <= 0) return;

        // graph & named-track MIDI snapshots
        std::shared_ptr<const std::vector<SqVocal::GraphNote>> gnotes;
        std::shared_ptr<const std::vector<SqVocal::TimedNote>> tnotes;
        {
            const juce::SpinLock::ScopedTryLockType l (snapLock);
            if (l.isLocked()) { gnotes = graphSnap; tnotes = trackSnap; }
        }
        // MIDI from this track
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) heldIn.fill (false);
            for (auto& m : *fc.bufferForMidiMessages)
            {
                if (m.isNoteOn()) { heldIn[(size_t) m.getNoteNumber()] = true; if (learnScale.get()) learnedMask |= 1 << (m.getNoteNumber() % 12); }
                else if (m.isNoteOff()) heldIn[(size_t) m.getNoteNumber()] = false;
                else if (m.isAllNotesOff() || m.isAllSoundOff()) heldIn.fill (false);
            }
        }
        SqVocal::TuneProcessor::Context ctx;
        ctx.editStart = fc.editTime.getStart().inSeconds() + inputTimeOffset.load();
        ctx.playing = fc.isPlaying;
        ctx.graph = graphOn.get() && gnotes != nullptr && ! gnotes->empty() ? gnotes.get() : nullptr;
        ctx.trackNotes = midiMode.get() == 2 ? tnotes.get() : nullptr;
        ctx.heldIn = midiMode.get() == 1 ? &heldIn : nullptr;
        ctx.mix = bank[mix] / 100.0f; ctx.outGain = juce::Decibels::decibelsToGain (bank[output]); ctx.tracking = bank[tracking];

        float* io[2] = { buf.getWritePointer (0, fc.bufferStartSample), buf.getWritePointer (nch > 1 ? 1 : 0, fc.bufferStartSample) };
        liveLevel = buf.getMagnitude (fc.bufferStartSample, fc.bufferNumSamples);
        proc.process (io, nch, fc.bufferNumSamples, gatherSettings(), ctx);
    }

    /** The settings the audio thread uses (also used by Claude to report what Moth Tune is doing). */
    SqVocal::TuneSettings gatherSettings()
    {
        SqVocal::TuneSettings s;
        s.retuneMs = bank[retune]; s.humanize = bank[humanize] / 100.0f; s.flex = bank[flex] / 100.0f; s.vibKeep = bank[vibKeep] / 100.0f;
        s.natVib = bank[natVib] / 100.0f; s.strength = bank[strength] / 100.0f; s.transpose = std::round (bank[transpose]);
        s.formantSt = bank[formant]; s.throat = bank[throat] / 100.0f; s.ref = 440.0f * std::pow (2.0f, bank[detune] / 1200.0f);
        s.vibRate = bank[vibRate]; s.vibDepth = bank[vibDepth]; s.vibDelay = bank[vibDelay]; s.vibOnset = bank[vibOnset]; s.vibVar = bank[vibVar] / 100.0f;
        s.vibShape = vibShape.get();
        s.classic = character.get() == 1; s.conform = conform.get(); s.preserve = preserve.get();
        s.midiTargets = midiMode.get() != 0 && midiLead.get(); s.allOctaves = allOctaves.get();
        s.allowedMask = learnScale.get() && learnedMask != 0 ? learnedMask : allowedMask();
        s.bypassMask = bypassMask.get() & 0xfff; s.keyRoot = keyRoot.get();
        for (int i = 0; i < 4; ++i)
        {
            auto& h = s.harm[(size_t) i];
            h.mode = harmMode[(size_t) i].get();
            const float lv = bank[(size_t) (h1Level + i * 4)];
            h.gain = lv <= -59.5f ? 0.0f : juce::Decibels::decibelsToGain (lv);
            h.pan = bank[(size_t) (h1Pan + i * 4)] / 100.0f;
            h.interval = std::round (bank[(size_t) (h1Interval + i * 4)]);
            h.formantSt = bank[(size_t) (h1Formant + i * 4)];
        }
        s.harmDetune = bank[harmDetune]; s.harmTimingMs = bank[harmTiming];
        return s;
    }

private:
    double sr = 44100;
    SqVocal::TuneProcessor proc;
    std::array<bool, 128> heldIn {};
    int learnedMask = 0;
    juce::SpinLock snapLock;
    std::shared_ptr<const std::vector<SqVocal::GraphNote>> graphSnap;
    std::shared_ptr<const std::vector<SqVocal::TimedNote>> trackSnap;
    std::vector<std::shared_ptr<const void>> graveyard;   // old snapshots are released on the message thread

    void pushDisplay (const SqVocal::FrameResult& r, const std::array<bool, 128>& held)
    {
        if ((++dispDecim & 3) != 0) return;   // ~115 points per second
        Disp d;
        d.in = r.voiced ? r.inNote : -1; d.out = r.voiced ? r.outNote : -1; d.target = r.voiced ? r.target : -1; d.corr = r.corrCents;
        for (int i = 0; i < 4; ++i) d.h[i] = r.voiced && r.hOn[i] ? r.hNote[i] : -1;
        const int w = dispWrite.load();
        disp[(size_t) (w & (dispSize - 1))] = d;
        dispWrite = w + 1;
        liveIn = d.in; liveOut = d.out; liveTarget = d.target; liveCents = r.corrCents;
        int hm = 0; for (int n = 0; n < 128; ++n) if (held[(size_t) n]) hm |= 1 << (n % 12);
        heldMaskUi = hm;
    }
    int dispDecim = 0;

    //==========================================================================
    // message thread
    void timerCallback() override
    {
        graveyard.clear();
        // Auto-Key: follow the session key
        if (autoKey.get())
        {
            auto es = edit.state;
            if (es.hasProperty ("sqmKeyRoot"))
            {
                const int r = (int) es["sqmKeyRoot"], sc = (int) es.getProperty ("sqmKeyScale", 1);
                if (keyRoot.get() != r) keyRoot.setValue (r, nullptr);
                if (scaleIndex.get() != sc) scaleIndex.setValue (sc, nullptr);
            }
        }
        if (midiMode.get() == 2) rebuildTrackSnap();
    }
    void rebuildTrackSnap()
    {
        auto v = std::make_shared<std::vector<SqVocal::TimedNote>>();
        const auto want = midiTrack.get();
        for (auto t : te::getAudioTracks (edit))
        {
            if (! t->getName().equalsIgnoreCase (want)) continue;
            for (auto clip : t->getClips())
                if (auto mc = dynamic_cast<te::MidiClip*> (clip))
                {
                    const auto cr = mc->getEditTimeRange();
                    for (auto* n : mc->getSequence().getNotes())
                    {
                        auto tr = n->getEditTimeRange (*mc);
                        const double s0 = juce::jmax (tr.getStart().inSeconds(), cr.getStart().inSeconds()), e0 = juce::jmin (tr.getEnd().inSeconds(), cr.getEnd().inSeconds());
                        if (e0 > s0) v->push_back ({ s0, e0, n->getNoteNumber() });
                    }
                }
        }
        std::sort (v->begin(), v->end(), [] (auto& a, auto& b) { return a.start < b.start; });
        std::shared_ptr<const std::vector<SqVocal::TimedNote>> nv = v;
        { const juce::SpinLock::ScopedLockType l (snapLock); graveyard.push_back (trackSnap); trackSnap = nv; }
    }
public:
    void rebuildGraph()
    {
        std::shared_ptr<const std::vector<SqVocal::GraphNote>> nv = std::make_shared<std::vector<SqVocal::GraphNote>> (getGraphNotes());
        const juce::SpinLock::ScopedLockType l (snapLock);
        graveyard.push_back (graphSnap); graphSnap = nv;
    }
private:
    void handleAsyncUpdate() override   { edit.restartPlayback(); }

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state && (id == juce::Identifier ("engine") || id == juce::Identifier ("voiceType"))) triggerAsyncUpdate();
        if (v.hasType (noteId)) rebuildGraph();
        if (v == state && id == juce::Identifier ("learnScale")) learnedMask = 0;
    }
    void valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c) override
    {
        te::Plugin::valueTreeChildAdded (p, c);
        if (p.hasType (graphId) || c.hasType (graphId)) rebuildGraph();
    }
    void valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int i) override
    {
        te::Plugin::valueTreeChildRemoved (p, c, i);
        if (p.hasType (graphId) || c.hasType (graphId)) rebuildGraph();
    }
};
