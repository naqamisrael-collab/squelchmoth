#pragma once
// Squelchmoth - transport logic shared by the Transport bar, Transport window, keyboard and Claude:
// play from the Edit selection, pre-roll / post-roll, count-off, Loop Record, QuickPunch, Wait for
// Note, MIDI Merge, Loop Playback, return-to-start, locate / nudge, markers navigation.

#include "EditOps.h"

class TransportController : private juce::Timer,
                            private juce::MidiKeyboardStateListener
{
public:
    TransportController (Session& s, TimelineState& st) : session (s), state (st)   { startTimerHz (30); }
    ~TransportController() override                                                   { detachMidi(); }

    //==========================================================================
    bool isPlaying() const      { return session.edit != nullptr && session.edit->getTransport().isPlaying(); }
    bool isRecording() const    { return session.edit != nullptr && session.edit->getTransport().isRecording(); }
    bool isWaitingForNote() const { return waitingForNote; }
    bool isCountingOff() const  { return countingOff; }

    void togglePlay()           { isPlaying() ? stop() : play(); }
    void toggleRecord()         { isRecording() ? stop() : record(); }

    void play()
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        if (tr.isPlaying()) return;
        applyRecordModes();
        auto start = EditOps::playStartPosition (session, state);
        stopAt.reset();

        if (Prefs::preRollEnabled() && state.timeSelection)
            start = juce::jmax (te::TimePosition(), start - te::TimeDuration::fromSeconds (Prefs::preRollSeconds()));
        if (Prefs::postRollEnabled() && state.timeSelection && state.timeSelection->getLength() > 0.01s && ! tr.looping.get())
            stopAt = state.timeSelection->getEnd() + te::TimeDuration::fromSeconds (Prefs::postRollSeconds());
        else if (state.timeSelection && state.timeSelection->getLength() > 0.01s && ! tr.looping.get() && Prefs::playFromSelection() && stopAtSelectionEnd)
            stopAt = state.timeSelection->getEnd();

        startedFrom = start;
        tr.setPosition (start);
        tr.play (false);
    }

    /** Plays from an exact position (Claude / locate-and-play): no selection or pre-roll logic, Stop returns here. */
    void playFrom (te::TimePosition pos)
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        if (tr.isPlaying()) tr.stop (false, false);
        applyRecordModes();
        stopAt.reset();
        startedFrom = pos;
        tr.setPosition (pos);
        tr.play (false);
    }

    void record()
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        if (tr.isRecording()) return;
        applyRecordModes();
        auto start = EditOps::playStartPosition (session, state);
        stopAt.reset();

        if (Prefs::quickPunch() && state.timeSelection && state.timeSelection->getLength() > 0.01s)
        {
            // QuickPunch: play from pre-roll, record only inside the Edit selection
            session.edit->recordingPunchInOut = true;
            tr.setLoopRange (*state.timeSelection);
            if (Prefs::postRollEnabled()) stopAt = state.timeSelection->getEnd() + te::TimeDuration::fromSeconds (Prefs::postRollSeconds());
        }
        else
            session.edit->recordingPunchInOut = false;

        if (Prefs::preRollEnabled() && state.timeSelection)
            start = juce::jmax (te::TimePosition(), start - te::TimeDuration::fromSeconds (Prefs::preRollSeconds()));

        if (Prefs::waitForNote())
        {
            waitingForNote = true;
            pendingStart = start;
            attachMidi();
            session.log.add ("Transport", "Waiting for a MIDI note to start recording...");
            return;
        }

        startedFrom = start;
        tr.setPosition (start);
        countingOff = Prefs::countOffBars() > 0;
        rememberClips();
        tr.record (false, true);
    }

    void stop (bool discardRecording = false)
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        waitingForNote = false;
        countingOff = false;
        detachMidi();
        bool wasPlaying = tr.isPlaying() || tr.isRecording();
        const bool wasRecording = tr.isRecording();
        tr.stop (discardRecording, false);
        if (wasRecording && ! discardRecording) resolveNewTakes();
        stopAt.reset();
        if (wasPlaying && Prefs::returnToStartOnStop()) tr.setPosition (startedFrom);
    }

    void returnToZero()         { if (session.edit) { stop(); session.edit->getTransport().setPosition (0s); } }
    void goToEnd()              { if (session.edit) { stop(); session.edit->getTransport().setPosition (te::toPosition (session.edit->getLength())); } }

    /** Rewind / fast-forward by one bar (or hold for continuous via repeated calls). */
    void skipBars (int dir)
    {
        if (session.edit == nullptr) return;
        auto& t = session.edit->getTransport();
        auto bar = session.timeToBar (t.getPosition());
        auto target = dir > 0 ? std::floor (bar + 1.0001) : std::ceil (bar - 1.0001);
        t.setPosition (session.barToTime (juce::jmax (1.0, target)));
    }

    void goToMarker (bool next)
    {
        if (session.edit == nullptr) return;
        auto& mm = session.edit->getMarkerManager();
        auto pos = session.edit->getTransport().getPosition();
        auto m = next ? mm.getNextMarker (pos + 0.01s) : mm.getPrevMarker (pos - 0.01s);
        if (m != nullptr) session.edit->getTransport().setPosition (m->getPosition().getStart());
    }

    void setLoopPlayback (bool on)
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        if (on && state.timeSelection && state.timeSelection->getLength() > 0.01s) tr.setLoopRange (*state.timeSelection);
        tr.looping = on;
        Prefs::set ("op_loopPlayback", on);
    }

    void setClick (bool on)         { if (session.edit) session.edit->clickTrackEnabled = on; }
    void setCountOff (int bars)     { Prefs::set ("op_countOffBars", bars); applyRecordModes(); }
    void setMidiMerge (bool on)     { Prefs::set ("op_midiMerge", on); applyRecordModes(); }
    void setLoopRecord (bool on)    { Prefs::set ("op_loopRecord", on); if (on) Prefs::set ("op_quickPunch", false); }
    void setQuickPunch (bool on)    { Prefs::set ("op_quickPunch", on); if (on) Prefs::set ("op_loopRecord", false); }
    void setWaitForNote (bool on)   { Prefs::set ("op_waitForNote", on); }
    void setPreRoll (bool on)       { Prefs::set ("op_preRollOn", on); }
    void setPostRoll (bool on)      { Prefs::set ("op_postRollOn", on); }

    bool stopAtSelectionEnd = true;    // Pro Tools plays a timeline selection and stops at its end (unless Loop Playback / post-roll)

    /** Pushes the preference flags into the engine (count-in, MIDI merge, loop record). */
    void applyRecordModes()
    {
        if (session.edit == nullptr) return;
        auto& e = *session.edit;
        auto bars = Prefs::countOffBars();
        e.setCountInMode (bars >= 2 ? te::Edit::CountIn::twoBar : bars == 1 ? te::Edit::CountIn::oneBar : te::Edit::CountIn::none);

        for (auto in : e.getAllInputDevices())
            if (auto mi = dynamic_cast<te::MidiInputDevice*> (&in->getInputDevice()))
            {
                mi->mergeRecordings = Prefs::midiMerge();
                mi->replaceExistingClips = false;
            }

        if (Prefs::loopRecord())
        {
            // Loop Record: loop the Edit selection while recording - each pass becomes a take (playlist)
            auto& tr = e.getTransport();
            if (state.timeSelection && state.timeSelection->getLength() > 0.01s) tr.setLoopRange (*state.timeSelection);
            tr.looping = true;
            e.recordingPunchInOut = true;
        }
    }

    /** Pro Tools: a new recording covers what was underneath it on the track (one clip sounds at a time). */
    void rememberClips()
    {
        clipsBeforeRecord.clear();
        if (session.edit == nullptr) return;
        for (auto t : session.getTracks()) for (auto c : t->getClips()) clipsBeforeRecord.insert (c->itemID);
    }

    void resolveNewTakes()
    {
        juce::Timer::callAfterDelay (400, [this, before = clipsBeforeRecord]
        {
            if (session.edit == nullptr) return;
            EditOps::Clips fresh;
            for (auto t : session.getTracks()) for (auto c : t->getClips()) if (before.count (c->itemID) == 0) fresh.push_back (c);
            EditOps::resolveOverlaps (fresh);
        });
    }

private:
    std::set<te::EditItemID> clipsBeforeRecord;
    Session& session;
    TimelineState& state;
    std::optional<te::TimePosition> stopAt;
    te::TimePosition startedFrom, pendingStart;
    bool waitingForNote = false, countingOff = false;
    std::vector<te::MidiInputDevice*> attached;

    void attachMidi()
    {
        detachMidi();
        if (session.edit == nullptr) return;
        for (auto in : session.edit->getAllInputDevices())
            if (auto mi = dynamic_cast<te::MidiInputDevice*> (&in->getInputDevice()))
            {
                mi->keyboardState.addListener (this);
                attached.push_back (mi);
            }
    }

    void detachMidi()
    {
        for (auto mi : attached) mi->keyboardState.removeListener (this);
        attached.clear();
    }

    void handleNoteOn (juce::MidiKeyboardState*, int, int, float) override
    {
        if (! waitingForNote) return;
        waitingForNote = false;
        juce::MessageManager::callAsync ([this]
        {
            detachMidi();
            if (session.edit == nullptr) return;
            auto& tr = session.edit->getTransport();
            startedFrom = pendingStart;
            tr.setPosition (pendingStart);
            rememberClips();
            tr.record (false, true);
            session.log.add ("Transport", "Note received - recording");
        });
    }

    void handleNoteOff (juce::MidiKeyboardState*, int, int, float) override {}

    void timerCallback() override
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        if (countingOff && tr.isRecording() && tr.getPosition() >= startedFrom) countingOff = false;
        if (stopAt && (tr.isPlaying() || tr.isRecording()) && tr.getPosition() >= *stopAt)
        {
            auto s = *stopAt;
            stop();
            tr.setPosition (Prefs::returnToStartOnStop() ? startedFrom : s);
        }
    }
};
