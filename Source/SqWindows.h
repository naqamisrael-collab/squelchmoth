#pragma once
// Squelchmoth - the floating windows of the Window menu: Transport, Big Counter, Memory Locations,
// Clip List, Groups, Undo History, Session Setup, I/O Setup, Playback Engine, Plugins, Color Palette,
// Preferences (incl. Display colours) and the Welcome / Quick Start window.

#include "TransportController.h"
#include "PluginScanner.h"

//==============================================================================
/** A floating tool window that hides (rather than deletes) when closed and remembers its position. */
#if JUCE_MAC
extern "C" void sqmMakeFloatingPanel (void* nsViewHandle, bool floating);
extern "C" void sqmActivateApp();
#endif

class SqWindow : public juce::DocumentWindow
{
public:
    SqWindow (const juce::String& name, const juce::String& prefKey, bool resizable = true)
        : juce::DocumentWindow (name, Theme::panel(), juce::DocumentWindow::closeButton | (resizable ? juce::DocumentWindow::minimiseButton : 0)), key (prefKey)
    {
        setUsingNativeTitleBar (true);
        setResizable (resizable, false);
        setAlwaysOnTop (false);
    }

    void closeButtonPressed() override
    {
        Prefs::set (("win_" + key + "_pos").toRawUTF8(), getBounds().toString());
        setVisible (false);
        if (onHidden) onHidden();
    }

    void showAndRestore (juce::Component* nearTo = nullptr)
    {
        auto& displays = juce::Desktop::getInstance().getDisplays();
        auto saved = Prefs::getString (("win_" + key + "_pos").toRawUTF8());
        // never restore a window smaller than its content was designed for (windows grow between versions)
        if (! designSizeKnown) { designSize = { getWidth(), getHeight() }; designSizeKnown = true; }
        bool placed = false;
        if (saved.isNotEmpty())
        {
            auto b = juce::Rectangle<int>::fromString (saved);
            if (auto* d = displays.getDisplayForPoint (b.getCentre()))
            {
                auto area = d->userArea;
                if (b.getWidth() > 60 && b.getHeight() > 40 && area.intersects (b))
                {
                    if (isResizable()) setBounds (b.withSize (juce::jmax (designSize.x, 200, b.getWidth()), juce::jmax (designSize.y, 120, b.getHeight())));
                    else setTopLeftPosition (b.getTopLeft());
                    placed = true;
                }
            }
        }
        if (! placed)
        {
            if (nearTo != nullptr) setCentrePosition (nearTo->getScreenBounds().getCentre());
            else centreWithSize (getWidth(), getHeight());
        }
        keepOnScreen();

        setVisible (true);
        applyFloating();
        toFront (true);
       #if JUCE_MAC
        sqmActivateApp();
       #endif
    }

    /** Pro Tools floating windows stay above the Edit/Mix window (and, on macOS, hide while another app is
        in front). Big "page" windows such as Mastering behave as normal windows. */
    void setFloating (bool f)                     { floating = f; if (isOnDesktop()) applyFloating(); }
    bool isFloating() const                       { return floating; }

    /** Make sure the whole title bar is reachable on the display it sits on (screens change, laptops get
        undocked - a window restored off-screen looks like a window that never opened). */
    void keepOnScreen()
    {
        auto& displays = juce::Desktop::getInstance().getDisplays();
        auto* d = displays.getDisplayForRect (getScreenBounds());
        if (d == nullptr) d = displays.getPrimaryDisplay();
        if (d == nullptr) return;
        auto area = d->userArea.reduced (8);
        auto b = getBounds();
        if (b.getWidth() > area.getWidth())   b.setWidth (area.getWidth());
        if (b.getHeight() > area.getHeight()) b.setHeight (area.getHeight());
        b = b.constrainedWithin (area);
        if (b != getBounds()) setBounds (b);
    }

    std::function<void()> onHidden;

protected:
    juce::String key;
    bool floating = true;
    bool designSizeKnown = false;
    juce::Point<int> designSize;

    void applyFloating()
    {
       #if JUCE_MAC
        if (auto* peer = getPeer()) sqmMakeFloatingPanel (peer->getNativeHandle(), floating);
       #elif JUCE_LINUX
        setAlwaysOnTop (floating);
       #endif
    }
};

//==============================================================================
class TransportWindowContent : public juce::Component,
                               private juce::Timer,
                               private juce::ChangeListener
{
public:
    TransportWindowContent (Session& s, TimelineState& st, TransportController& tc) : session (s), state (st), transport (tc)
    {
        auto setup = [this] (juce::TextButton& b, const juce::String& tip, juce::Colour on)
        {
            addAndMakeVisible (b);
            b.setTooltip (tip);
            b.setColour (juce::TextButton::buttonOnColourId, on);
            b.setClickingTogglesState (false);
        };
        setup (online, "Online: slave to external timecode (needs MTC/LTC hardware - not available on this system)", Theme::mint());
        online.setEnabled (false);
        setup (rtz, "Return to zero (Return)", Theme::violet());
        setup (rew, "Rewind one bar (hold to repeat)", Theme::violet());
        setup (stop, "Stop (Space)", Theme::violet());
        setup (play, "Play (Space) - plays from the Edit selection", Theme::mint().darker (0.4f));
        setup (ffwd, "Fast forward one bar", Theme::violet());
        setup (toEnd, "Go to end of session", Theme::violet());
        setup (rec, "Record (" SQ_CMD "+Space / F12)", Theme::red());
        setup (loop, "Loop Playback (" SQ_CMD "+Shift+L) - loops the Edit selection", Theme::mint().darker (0.4f));
        setup (preRoll, "Pre-roll: play this much before the selection", Theme::violet());
        setup (postRoll, "Post-roll: keep going this much after the selection", Theme::violet());
        setup (click, "Click / metronome (C)", Theme::violet());
        setup (countOff, "Count-off before recording (click to cycle: off / 1 bar / 2 bars)", Theme::violet());
        setup (midiMerge, "MIDI Merge: new MIDI recordings merge into the existing clip instead of replacing it", Theme::violet());
        setup (waitNote, "Wait for Note: recording starts on the first MIDI note", Theme::violet());
        setup (conductor, "Conductor: follow the session tempo map (off = manual tempo field)", Theme::violet());
        setup (recModeBtn, "Record mode: click to cycle Normal / Loop Record / QuickPunch", Theme::amber().darker (0.4f));

        rtz.onClick = [this] { transport.returnToZero(); };
        rew.onClick = [this] { transport.skipBars (-1); };
        ffwd.onClick = [this] { transport.skipBars (1); };
        toEnd.onClick = [this] { transport.goToEnd(); };
        stop.onClick = [this] { transport.stop(); };
        play.onClick = [this] { transport.togglePlay(); };
        rec.onClick = [this] { transport.toggleRecord(); };
        loop.onClick = [this] { transport.setLoopPlayback (! (session.edit && session.edit->getTransport().looping.get())); };
        preRoll.onClick = [this] { transport.setPreRoll (! Prefs::preRollEnabled()); };
        postRoll.onClick = [this] { transport.setPostRoll (! Prefs::postRollEnabled()); };
        click.onClick = [this] { transport.setClick (! (session.edit && session.edit->clickTrackEnabled.get())); };
        countOff.onClick = [this] { transport.setCountOff ((Prefs::countOffBars() + 1) % 3); };
        midiMerge.onClick = [this] { transport.setMidiMerge (! Prefs::midiMerge()); };
        waitNote.onClick = [this] { transport.setWaitForNote (! Prefs::waitForNote()); };
        conductor.onClick = [this] { Prefs::set ("op_conductor", ! Prefs::getBool ("op_conductor", true)); };
        recModeBtn.onClick = [this]
        {
            if (Prefs::loopRecord()) { transport.setLoopRecord (false); transport.setQuickPunch (true); }
            else if (Prefs::quickPunch()) { transport.setQuickPunch (false); }
            else transport.setLoopRecord (true);
        };

        for (auto* l : { &counter, &subCounter })
        {
            addAndMakeVisible (l);
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::backgroundColourId, Theme::bg());
            l->setColour (juce::Label::outlineColourId, Theme::line());
        }
        counter.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 30.0f, juce::Font::bold)));
        counter.setColour (juce::Label::textColourId, Theme::acid());
        subCounter.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain)));
        subCounter.setColour (juce::Label::textColourId, Theme::dim());

        for (auto* e : { &preRollField, &postRollField, &tempoField, &meterField })
        {
            addAndMakeVisible (e);
            e->setEditable (true);
            e->setJustificationType (juce::Justification::centred);
            e->setColour (juce::Label::backgroundColourId, Theme::panel2());
            e->setFont (Theme::font (13.0f, true));
        }
        preRollField.onTextChange = [this] { Prefs::set ("op_preRoll", juce::jlimit (0.0, 60.0, preRollField.getText().getDoubleValue())); };
        postRollField.onTextChange = [this] { Prefs::set ("op_postRoll", juce::jlimit (0.0, 60.0, postRollField.getText().getDoubleValue())); };
        tempoField.onTextChange = [this]
        {
            if (session.edit) { auto bpm = tempoField.getText().upToFirstOccurrenceOf (" ", false, false).getDoubleValue(); if (bpm >= 20 && bpm <= 400) session.edit->tempoSequence.getTempo (0)->setBpm (bpm); }
        };
        meterField.onTextChange = [this]
        {
            if (session.edit)
            {
                auto parts = juce::StringArray::fromTokens (meterField.getText(), "/", "");
                if (parts.size() == 2 && parts[0].getIntValue() > 0 && parts[1].getIntValue() > 0)
                { session.edit->tempoSequence.getTimeSig (0)->numerator = parts[0].getIntValue(); session.edit->tempoSequence.getTimeSig (0)->denominator = parts[1].getIntValue(); }
            }
        };
        preRollField.setTooltip ("Pre-roll length in seconds"); postRollField.setTooltip ("Post-roll length in seconds");
        tempoField.setTooltip ("Session tempo (BPM) - type to change"); meterField.setTooltip ("Meter (time signature)");

        status.setFont (Theme::font (11.0f));
        status.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (status);

        Prefs::broadcaster().addChangeListener (this);
        startTimerHz (15);
        setSize (820, 150);
    }

    ~TransportWindowContent() override    { Prefs::broadcaster().removeChangeListener (this); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        g.setColour (Theme::dim());
        g.setFont (Theme::font (9.0f, true));
        g.drawText ("TRANSPORT", 10, 4, 200, 10, juce::Justification::left);
        g.drawText ("COUNTERS", countersX, 4, 200, 10, juce::Justification::left);
        g.drawText ("PRE / POST ROLL", 10, 66, 200, 10, juce::Justification::left);
        g.drawText ("RECORD MODES & MIDI", 300, 66, 200, 10, juce::Justification::left);
        g.drawText ("TEMPO / METER", tempoX, 66, 200, 10, juce::Justification::left);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto row1 = r.removeFromTop (46).withTrimmedTop (10);
        online.setBounds (row1.removeFromLeft (62)); row1.removeFromLeft (6);
        for (auto* b : { &rtz, &rew, &stop, &play, &ffwd, &toEnd, &rec }) { b->setBounds (row1.removeFromLeft (44)); row1.removeFromLeft (3); }
        row1.removeFromLeft (6);
        loop.setBounds (row1.removeFromLeft (54)); row1.removeFromLeft (12);
        countersX = row1.getX();
        auto cnt = row1.removeFromLeft (230);
        counter.setBounds (cnt);
        row1.removeFromLeft (6);
        subCounter.setBounds (row1.removeFromLeft (120));

        r.removeFromTop (12);
        auto row2 = r.removeFromTop (28).withTrimmedTop (4);
        preRoll.setBounds (row2.removeFromLeft (52)); row2.removeFromLeft (3);
        preRollField.setBounds (row2.removeFromLeft (54)); row2.removeFromLeft (8);
        postRoll.setBounds (row2.removeFromLeft (52)); row2.removeFromLeft (3);
        postRollField.setBounds (row2.removeFromLeft (54)); row2.removeFromLeft (14);
        recModeBtn.setBounds (row2.removeFromLeft (96)); row2.removeFromLeft (3);
        waitNote.setBounds (row2.removeFromLeft (46)); row2.removeFromLeft (3);
        midiMerge.setBounds (row2.removeFromLeft (52)); row2.removeFromLeft (3);
        click.setBounds (row2.removeFromLeft (48)); row2.removeFromLeft (3);
        countOff.setBounds (row2.removeFromLeft (66)); row2.removeFromLeft (14);
        tempoX = row2.getX();
        conductor.setBounds (row2.removeFromLeft (54)); row2.removeFromLeft (3);
        tempoField.setBounds (row2.removeFromLeft (84)); row2.removeFromLeft (3);
        meterField.setBounds (row2.removeFromLeft (50));
        status.setBounds (r.reduced (0, 2));
    }

private:
    Session& session;
    TimelineState& state;
    TransportController& transport;
    juce::TextButton online { "ONLINE" }, rtz { "|<" }, rew { "<<" }, stop { "STOP" }, play { "PLAY" }, ffwd { ">>" }, toEnd { ">|" }, rec { "REC" }, loop { "LOOP" },
                     preRoll { "PRE" }, postRoll { "POST" }, click { "CLICK" }, countOff { "COUNT" }, midiMerge { "MERGE" }, waitNote { "WAIT" }, conductor { "COND" }, recModeBtn { "NORMAL" };
    juce::Label counter, subCounter, preRollField, postRollField, tempoField, meterField, status;
    int countersX = 0, tempoX = 0;

    void changeListenerCallback (juce::ChangeBroadcaster*) override   { refreshFlags(); }

    void refreshFlags()
    {
        preRoll.setToggleState (Prefs::preRollEnabled(), juce::dontSendNotification);
        postRoll.setToggleState (Prefs::postRollEnabled(), juce::dontSendNotification);
        if (! preRollField.isBeingEdited()) preRollField.setText (juce::String (Prefs::preRollSeconds(), 1) + " s", juce::dontSendNotification);
        if (! postRollField.isBeingEdited()) postRollField.setText (juce::String (Prefs::postRollSeconds(), 1) + " s", juce::dontSendNotification);
        midiMerge.setToggleState (Prefs::midiMerge(), juce::dontSendNotification);
        waitNote.setToggleState (Prefs::waitForNote(), juce::dontSendNotification);
        conductor.setToggleState (Prefs::getBool ("op_conductor", true), juce::dontSendNotification);
        auto co = Prefs::countOffBars();
        countOff.setButtonText (co == 0 ? "COUNT" : (co == 1 ? "COUNT 1" : "COUNT 2"));
        countOff.setToggleState (co > 0, juce::dontSendNotification);
        recModeBtn.setButtonText (Prefs::loopRecord() ? "LOOP REC" : Prefs::quickPunch() ? "QUICKPUNCH" : "NORMAL");
        recModeBtn.setToggleState (Prefs::loopRecord() || Prefs::quickPunch(), juce::dontSendNotification);
    }

    void timerCallback() override
    {
        auto e = session.edit.get();
        if (e == nullptr) return;
        auto& t = e->getTransport();
        auto pos = t.getPosition();
        counter.setText (session.barsBeatsString (pos), juce::dontSendNotification);
        auto secs = pos.inSeconds();
        subCounter.setText (juce::String::formatted ("%02d:%02d.%03d", (int) secs / 60, (int) secs % 60, (int) (std::fmod (secs, 1.0) * 1000)), juce::dontSendNotification);
        play.setToggleState (t.isPlaying(), juce::dontSendNotification);
        rec.setToggleState (t.isRecording() || transport.isWaitingForNote(), juce::dontSendNotification);
        rec.setButtonText (transport.isWaitingForNote() ? "WAIT" : "REC");
        loop.setToggleState (t.looping.get(), juce::dontSendNotification);
        click.setToggleState (e->clickTrackEnabled.get(), juce::dontSendNotification);
        counter.setColour (juce::Label::textColourId, t.isRecording() ? Theme::red() : Theme::acid());
        if (! tempoField.isBeingEdited()) tempoField.setText (juce::String (e->tempoSequence.getTempo (0)->getBpm(), 1) + " BPM", juce::dontSendNotification);
        if (! meterField.isBeingEdited()) meterField.setText (juce::String (e->tempoSequence.getTimeSig (0)->numerator.get()) + "/" + juce::String (e->tempoSequence.getTimeSig (0)->denominator.get()), juce::dontSendNotification);
        juce::String st;
        if (state.timeSelection && state.timeSelection->getLength() > 0.01s) st << "Edit selection " << session.barsBeatsString (state.timeSelection->getStart()) << " - " << session.barsBeatsString (state.timeSelection->getEnd()) << "   ";
        st << (t.isRecording() ? "RECORDING" : t.isPlaying() ? "Playing" : "Stopped");
        if (transport.isWaitingForNote()) st << "   waiting for MIDI note";
        status.setText (st, juce::dontSendNotification);
        static int n = 0; if (++n % 15 == 0) refreshFlags();
    }
};

//==============================================================================
class BigCounterContent : public juce::Component, private juce::Timer
{
public:
    explicit BigCounterContent (Session& s) : session (s) { startTimerHz (20); setSize (520, 150); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg());
        if (session.edit == nullptr) return;
        auto& t = session.edit->getTransport();
        auto pos = t.getPosition();
        juce::String txt;
        if (mode == 0) txt = session.barsBeatsString (pos);
        else if (mode == 1) { auto s = pos.inSeconds(); txt = juce::String::formatted ("%02d:%02d:%02d.%03d", (int) s / 3600, ((int) s / 60) % 60, (int) s % 60, (int) (std::fmod (s, 1.0) * 1000)); }
        else if (mode == 2) { auto s = pos.inSeconds(); int fps = 30; txt = juce::String::formatted ("%02d:%02d:%02d:%02d", (int) s / 3600, ((int) s / 60) % 60, (int) s % 60, (int) (std::fmod (s, 1.0) * fps)); }
        else txt = juce::String ((juce::int64) (pos.inSeconds() * session.getEngine().getDeviceManager().getSampleRate()));
        g.setColour (t.isRecording() ? Theme::red() : Theme::acid());
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), juce::jmin (96.0f, getWidth() / (float) juce::jmax (8, txt.length()) * 1.7f), juce::Font::bold)));
        g.drawText (txt, getLocalBounds().withTrimmedBottom (18), juce::Justification::centred);
        g.setColour (Theme::dim());
        g.setFont (Theme::font (10.0f));
        const char* names[] = { "BARS | BEATS", "MIN : SECS", "TIMECODE 30fps", "SAMPLES" };
        g.drawText (juce::String (names[mode]) + "   (click to change)", getLocalBounds().removeFromBottom (18), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { mode = (mode + 1) % 4; Prefs::set ("win_bigcounter_mode", mode); repaint(); }
private:
    Session& session;
    int mode = Prefs::getInt ("win_bigcounter_mode", 0);
    void timerCallback() override { repaint(); }
};

//==============================================================================
class MemoryLocationsContent : public juce::Component, private juce::TableListBoxModel, private juce::Timer
{
public:
    MemoryLocationsContent (Session& s, TimelineState& st) : session (s), state (st)
    {
        addAndMakeVisible (table);
        table.setModel (this);
        table.getHeader().addColumn ("#", 1, 40);
        table.getHeader().addColumn ("Name", 2, 220);
        table.getHeader().addColumn ("Bar|Beat", 3, 100);
        table.getHeader().addColumn ("Min:Sec", 4, 90);
        table.getHeader().addColumn ("Type", 5, 80);
        table.setColour (juce::ListBox::backgroundColourId, Theme::bg());
        table.setRowHeight (22);
        for (auto* b : { &newBtn, &editBtn, &deleteBtn, &goBtn, &selectBtn })
            addAndMakeVisible (b);
        newBtn.onClick = [this] { if (onNew) onNew(); refresh(); };
        editBtn.onClick = [this] { if (auto m = selected()) if (onEdit) onEdit (*m); };
        deleteBtn.onClick = [this] { if (auto m = selected()) { m->removeFromParent(); refresh(); } };
        goBtn.onClick = [this] { goToSelected(); };
        selectBtn.onClick = [this] { if (auto m = selected()) { if (m->getPosition().getLength().inSeconds() > 0.05) { state.timeSelection = m->getPosition().time; if (onChanged) onChanged(); } } };
        hint.setText ("Shift+Return (or numpad Enter) adds a memory location at the playhead. Double-click a row to go there.", juce::dontSendNotification);
        hint.setFont (Theme::font (10.5f));
        hint.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (hint);
        startTimerHz (2);
        setSize (560, 360);
    }

    std::function<void()> onNew, onChanged;
    std::function<void (te::MarkerClip&)> onEdit;

    void refresh()  { markers = session.getMarkers(); table.updateContent(); table.repaint(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto row = r.removeFromBottom (26);
        for (auto* b : { &newBtn, &editBtn, &deleteBtn, &goBtn, &selectBtn }) { b->setBounds (row.removeFromLeft (84)); row.removeFromLeft (4); }
        r.removeFromBottom (4);
        hint.setBounds (r.removeFromBottom (18));
        table.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel()); }

private:
    Session& session;
    TimelineState& state;
    juce::TableListBox table;
    juce::ReferenceCountedArray<te::MarkerClip> markers;
    juce::TextButton newBtn { "New" }, editBtn { "Edit..." }, deleteBtn { "Delete" }, goBtn { "Go to" }, selectBtn { "Select" };
    juce::Label hint;

    te::MarkerClip* selected()  { auto r = table.getSelectedRow(); return r >= 0 && r < markers.size() ? markers[r].get() : nullptr; }
    void goToSelected()
    {
        if (auto m = selected())
        {
            session.edit->getTransport().setPosition (m->getPosition().getStart());
            if (m->getPosition().getLength().inSeconds() > 0.05) state.timeSelection = m->getPosition().time;
            if (onChanged) onChanged();
        }
    }
    int getNumRows() override   { return markers.size(); }
    void paintRowBackground (juce::Graphics& g, int row, int, int, bool sel) override { g.fillAll (sel ? Theme::violet().withAlpha (0.4f) : (row % 2 ? Theme::panel() : Theme::bg())); }
    void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool) override
    {
        if (row >= markers.size()) return;
        auto m = markers[row];
        g.setColour (Theme::text());
        g.setFont (Theme::font (12.0f));
        juce::String s;
        auto secs = m->getPosition().getStart().inSeconds();
        if (col == 1) s = juce::String (m->getMarkerID());
        else if (col == 2) s = m->getName();
        else if (col == 3) s = session.barsBeatsString (m->getPosition().getStart());
        else if (col == 4) s = juce::String::formatted ("%02d:%02d.%03d", (int) secs / 60, (int) secs % 60, (int) (std::fmod (secs, 1.0) * 1000));
        else s = m->getPosition().getLength().inSeconds() > 0.05 ? "Selection" : "Marker";
        g.drawText (s, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
    }
    void cellDoubleClicked (int row, int, const juce::MouseEvent&) override { table.selectRow (row); goToSelected(); }
    void timerCallback() override { if (markers.size() != session.getMarkers().size()) refresh(); }
};

//==============================================================================
class ClipListContent : public juce::Component, private juce::TableListBoxModel, private juce::Timer
{
public:
    // 1.10 (Pro Tools wish #1): clips can be filed in bins - folders in the Clip List. A clip's bin is saved with it
    // ("sqmBin"); bins are listed A-Z with Unfiled last, each one collapsible. Right-click a clip to file it.
    static inline const juce::Identifier binId { "sqmBin" };
    static juce::String binOf (te::Clip& c)   { return c.state.getProperty (binId).toString(); }

    ClipListContent (Session& s, TimelineState& st) : session (s), state (st)
    {
        addAndMakeVisible (table);
        table.setModel (this);
        table.getHeader().addColumn ("Clip", 1, 220);
        table.getHeader().addColumn ("Bin", 7, 110);
        table.getHeader().addColumn ("Track", 2, 140);
        table.getHeader().addColumn ("Start", 3, 90);
        table.getHeader().addColumn ("Length", 4, 90);
        table.getHeader().addColumn ("Type", 5, 70);
        table.getHeader().addColumn ("Source file", 6, 240);
        table.setRowHeight (22);
        table.setMultipleSelectionEnabled (true);
        search.setTextToShowWhenEmpty ("Find clip...", Theme::dim());
        search.onTextChange = [this] { refresh(); };
        addAndMakeVisible (search);
        addAndMakeVisible (revealBtn);
        revealBtn.onClick = [this] { if (auto c = selected()) if (auto wc = dynamic_cast<te::WaveAudioClip*> (c)) wc->getSourceFileReference().getFile().revealToUser(); };
        addAndMakeVisible (binBtn);
        binBtn.setTooltip ("File the selected clips in a bin (or right-click a clip)");
        binBtn.onClick = [this] { showBinMenu (&binBtn); };
        startTimerHz (1);
        setSize (980, 440);
    }
    std::function<void()> onChanged;
    void refresh()
    {
        rows.clear();
        auto filter = search.getText().trim();
        std::map<juce::String, std::vector<Row>> byBin;
        for (auto t : session.getTracks())
            for (auto c : t->getClips())
                if (filter.isEmpty() || c->getName().containsIgnoreCase (filter) || t->getName().containsIgnoreCase (filter) || binOf (*c).containsIgnoreCase (filter))
                    byBin[binOf (*c)].push_back ({ c, t, {}, false });
        juce::StringArray names;
        for (auto& [k, v] : byBin) if (k.isNotEmpty()) names.add (k);
        names.sortNatural();
        if (byBin.count ({}) > 0) names.add ({});
        const bool anyBins = names.size() > 1 || (names.size() == 1 && names[0].isNotEmpty());
        for (auto& n : names)
        {
            auto& list = byBin[n];
            if (anyBins) rows.push_back ({ nullptr, nullptr, n.isEmpty() ? juce::String ("Unfiled") : n, true, (int) list.size() });
            if (anyBins && collapsed.count (n.isEmpty() ? juce::String ("Unfiled") : n)) continue;
            for (auto& r : list) rows.push_back (r);
        }
        table.updateContent(); table.repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto top = r.removeFromTop (26);
        revealBtn.setBounds (top.removeFromRight (150));
        top.removeFromRight (6);
        binBtn.setBounds (top.removeFromRight (120));
        top.removeFromRight (6);
        search.setBounds (top);
        r.removeFromTop (6);
        table.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel()); }

    /** Files clips in a bin ("" = unfile). Undoable. */
    static void fileClips (Session& s, const juce::Array<te::Clip*>& cs, const juce::String& bin)
    {
        for (auto c : cs)
        {
            if (bin.isEmpty()) c->state.removeProperty (binId, &s.edit->getUndoManager());
            else c->state.setProperty (binId, bin, &s.edit->getUndoManager());
        }
    }
    static juce::StringArray allBins (Session& s)
    {
        juce::StringArray b;
        for (auto t : s.getTracks()) for (auto c : t->getClips()) if (binOf (*c).isNotEmpty()) b.addIfNotAlreadyThere (binOf (*c));
        b.sortNatural();
        return b;
    }

private:
    struct Row { te::Clip::Ptr clip; te::AudioTrack* track; juce::String header; bool isHeader = false; int count = 0; };
    Session& session; TimelineState& state;
    juce::TableListBox table; juce::TextEditor search; juce::TextButton revealBtn { SQ_FILE_BROWSER_SHOW }, binBtn { "File in bin..." };
    std::vector<Row> rows;
    std::set<juce::String> collapsed;
    size_t lastCount = 0;

    te::Clip* selected() { auto r = table.getSelectedRow(); return r >= 0 && r < (int) rows.size() ? rows[(size_t) r].clip.get() : nullptr; }
    juce::Array<te::Clip*> selectedClips()
    {
        juce::Array<te::Clip*> out;
        auto sel = table.getSelectedRows();
        for (int i = 0; i < sel.size(); ++i) { auto r = sel[i]; if (r >= 0 && r < (int) rows.size() && rows[(size_t) r].clip != nullptr) out.add (rows[(size_t) r].clip.get()); }
        return out;
    }
    void showBinMenu (juce::Component* target)
    {
        auto cs = selectedClips();
        if (cs.isEmpty()) { Ui::message (juce::MessageBoxIconType::InfoIcon, "Clip List", "Select one or more clips first (Shift / " SQ_CMD "-click for several)."); return; }
        juce::PopupMenu m;
        auto bins = allBins (session);
        for (int i = 0; i < bins.size(); ++i) m.addItem (100 + i, "Move to " + bins[i]);
        if (! bins.isEmpty()) m.addSeparator();
        m.addItem (1, "New bin...");
        m.addItem (2, "Unfile (no bin)");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this, cs, bins] (int r)
        {
            if (r >= 100) { fileClips (session, cs, bins[r - 100]); refresh(); }
            else if (r == 2) { fileClips (session, cs, {}); refresh(); }
            else if (r == 1)
            {
                auto* aw = new juce::AlertWindow ("New bin", "Name the bin for the " + juce::String (cs.size()) + " selected clip" + (cs.size() == 1 ? "" : "s") + ":", juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", "Bin " + juce::String (allBins (session).size() + 1));
                aw->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, cs] (int ok)
                {
                    auto n = aw->getTextEditorContents ("name").trim();
                    if (ok == 1 && n.isNotEmpty() && n != "Unfiled") { fileClips (session, cs, n); refresh(); }
                }), true);
            }
        });
    }
    int getNumRows() override { return (int) rows.size(); }
    void paintRowBackground (juce::Graphics& g, int row, int, int, bool sel) override
    {
        if (row < (int) rows.size() && rows[(size_t) row].isHeader) { g.fillAll (Theme::panel2().brighter (0.05f)); g.setColour (Theme::line()); g.drawHorizontalLine (0, 0.0f, (float) g.getClipBounds().getRight()); return; }
        g.fillAll (sel ? Theme::violet().withAlpha (0.4f) : (row % 2 ? Theme::panel() : Theme::bg()));
    }
    void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool) override
    {
        if (row >= (int) rows.size()) return;
        auto& r = rows[(size_t) row];
        if (r.isHeader)
        {
            if (col != 1) return;
            g.setColour (Theme::text()); g.setFont (Theme::font (12.5f, true));
            const bool closed = collapsed.count (r.header) > 0;
            g.drawText (juce::String::fromUTF8 (closed ? "\xe2\x96\xb8  " : "\xe2\x96\xbe  ") + r.header + "   (" + juce::String (r.count) + ")", 6, 0, w + 200, h, juce::Justification::centredLeft, true);
            return;
        }
        g.setColour (Theme::text()); g.setFont (Theme::font (12.0f));
        juce::String s;
        if (col == 1) s = (rows.front().isHeader ? "      " : "") + r.clip->getName();
        else if (col == 7) s = binOf (*r.clip);
        else if (col == 2) s = r.track->getName();
        else if (col == 3) s = session.barsBeatsString (r.clip->getPosition().getStart());
        else if (col == 4) s = juce::String (r.clip->getPosition().getLength().inSeconds(), 2) + " s";
        else if (col == 5) s = dynamic_cast<te::MidiClip*> (r.clip.get()) ? "MIDI" : dynamic_cast<te::StepClip*> (r.clip.get()) ? "Beat" : "Audio";
        else if (auto wc = dynamic_cast<te::WaveAudioClip*> (r.clip.get())) s = wc->getSourceFileReference().getFile().getFileName();
        g.drawText (s, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
    }
    void cellClicked (int row, int, const juce::MouseEvent& e) override
    {
        if (row >= (int) rows.size()) return;
        auto& r = rows[(size_t) row];
        if (r.isHeader) { if (collapsed.count (r.header)) collapsed.erase (r.header); else collapsed.insert (r.header); refresh(); return; }
        if (e.mods.isPopupMenu()) { if (! table.isRowSelected (row)) table.selectRow (row); showBinMenu (&table); return; }
        state.selectClip (r.clip); state.selectTrack (r.track); state.timeSelection = r.clip->getPosition().time;
        if (onChanged) onChanged();
    }
    void cellDoubleClicked (int row, int, const juce::MouseEvent&) override
    {
        if (row >= (int) rows.size() || rows[(size_t) row].isHeader) return;
        session.edit->getTransport().setPosition (rows[(size_t) row].clip->getPosition().getStart());
    }
    void timerCallback() override
    {
        size_t n = 0; for (auto t : session.getTracks()) n += (size_t) t->getClips().size();
        if (n != lastCount && search.getText().isEmpty()) { lastCount = n; refresh(); }
    }
};

//==============================================================================
class GroupsContent : public juce::Component, private juce::TableListBoxModel, private juce::Timer
{
public:
    GroupsContent (Session& s, TimelineState& st) : session (s), state (st)
    {
        addAndMakeVisible (table);
        table.setModel (this);
        table.getHeader().addColumn ("ID", 1, 36);
        table.getHeader().addColumn ("Name", 2, 160);
        table.getHeader().addColumn ("Active", 3, 60);
        table.getHeader().addColumn ("Vol", 4, 44);
        table.getHeader().addColumn ("Mute", 5, 48);
        table.getHeader().addColumn ("Solo", 6, 48);
        table.getHeader().addColumn ("Arm", 7, 44);
        table.getHeader().addColumn ("Colour", 8, 56);
        table.getHeader().addColumn ("Members", 9, 300);
        table.setRowHeight (22);
        for (auto* b : { &newBtn, &deleteBtn, &suspendBtn, &selectBtn, &renameBtn })
            addAndMakeVisible (b);
        newBtn.onClick = [this] { createFromSelection(); };
        deleteBtn.onClick = [this] { if (auto g = selected()) { session.removeGroup (g->id); refresh(); } };
        renameBtn.onClick = [this] { renameSelected(); };
        suspendBtn.setClickingTogglesState (true);
        suspendBtn.setColour (juce::TextButton::buttonOnColourId, Theme::amber().darker (0.4f));
        suspendBtn.onClick = [this] { session.groupsEnabled = ! suspendBtn.getToggleState(); };
        selectBtn.onClick = [this]
        {
            if (auto g = selected())
            {
                state.selectedTrackIDs.clear();
                for (auto& id : g->tracks) state.selectedTrackIDs.push_back (id);
                if (! g->tracks.empty()) { state.selectedRow = te::findTrackForID (*session.edit, g->tracks.front()); state.selectedTrack = dynamic_cast<te::AudioTrack*> (state.selectedRow); }
                if (onChanged) onChanged();
            }
        };
        hint.setText ("Click a cell in Active / Vol / Mute / Solo / Arm / Colour to toggle it. Grouped faders, mutes and solos move together.", juce::dontSendNotification);
        hint.setFont (Theme::font (10.5f)); hint.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (hint);
        startTimerHz (2);
        setSize (820, 360);
    }
    std::function<void()> onChanged;
    void refresh() { groups = session.getGroups(); table.updateContent(); table.repaint(); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto row = r.removeFromBottom (26);
        for (auto* b : { &newBtn, &renameBtn, &deleteBtn, &selectBtn, &suspendBtn }) { b->setBounds (row.removeFromLeft (120)); row.removeFromLeft (4); }
        r.removeFromBottom (4);
        hint.setBounds (r.removeFromBottom (18));
        table.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel()); }
private:
    Session& session; TimelineState& state;
    juce::TableListBox table;
    std::vector<Session::Group> groups;
    juce::TextButton newBtn { "New from selection" }, deleteBtn { "Delete" }, suspendBtn { "Suspend all groups" }, selectBtn { "Select members" }, renameBtn { "Rename..." };
    juce::Label hint;
    Session::Group* selected() { auto r = table.getSelectedRow(); return r >= 0 && r < (int) groups.size() ? &groups[(size_t) r] : nullptr; }
    void createFromSelection()
    {
        Session::Group g;
        g.id = session.nextGroupId(); g.name = "Group " + g.id.toUpperCase();
        for (auto t : session.getRows()) if (state.isTrackSelected (t)) g.tracks.push_back (t->itemID);
        if (g.tracks.empty()) { Ui::message (juce::MessageBoxIconType::InfoIcon, "Groups", "Select some tracks first (click their names, Shift-click for a range)."); return; }
        session.setGroup (g); refresh();
    }
    void renameSelected()
    {
        auto g = selected(); if (g == nullptr) return;
        auto aw = std::make_shared<juce::AlertWindow> ("Rename Group", "", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", g->name);
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        auto copy = *g;
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, copy] (int r) mutable { aw->setVisible (false); if (r == 1) { copy.name = aw->getTextEditorContents ("name"); session.setGroup (copy); refresh(); } }));
    }
    int getNumRows() override { return (int) groups.size(); }
    void paintRowBackground (juce::Graphics& g, int row, int, int, bool sel) override { g.fillAll (sel ? Theme::violet().withAlpha (0.4f) : (row % 2 ? Theme::panel() : Theme::bg())); }
    void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool) override
    {
        if (row >= (int) groups.size()) return;
        auto& gr = groups[(size_t) row];
        g.setColour (Theme::text()); g.setFont (Theme::font (12.0f));
        auto tick = [&] (bool on) { g.setColour (on ? Theme::mint() : Theme::dim()); g.drawText (on ? "ON" : "-", 0, 0, w, h, juce::Justification::centred); };
        if (col == 1) g.drawText (gr.id, 6, 0, w, h, juce::Justification::centredLeft);
        else if (col == 2) g.drawText (gr.name, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
        else if (col == 3) tick (gr.active);
        else if (col == 4) tick (gr.linkVolume);
        else if (col == 5) tick (gr.linkMute);
        else if (col == 6) tick (gr.linkSolo);
        else if (col == 7) tick (gr.linkArm);
        else if (col == 8) tick (gr.linkColour);
        else
        {
            juce::StringArray names;
            for (auto& id : gr.tracks) if (auto t = te::findTrackForID (*session.edit, id)) names.add (t->getName());
            g.drawText (names.joinIntoString (", "), 6, 0, w - 8, h, juce::Justification::centredLeft, true);
        }
    }
    void cellClicked (int row, int col, const juce::MouseEvent&) override
    {
        if (row >= (int) groups.size()) return;
        auto gr = groups[(size_t) row];
        if (col == 3) gr.active = ! gr.active; else if (col == 4) gr.linkVolume = ! gr.linkVolume; else if (col == 5) gr.linkMute = ! gr.linkMute;
        else if (col == 6) gr.linkSolo = ! gr.linkSolo; else if (col == 7) gr.linkArm = ! gr.linkArm; else if (col == 8) gr.linkColour = ! gr.linkColour; else return;
        session.setGroup (gr); refresh();
    }
    void timerCallback() override { if (groups.size() != session.getGroups().size()) refresh(); }
};

//==============================================================================
class UndoHistoryContent : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit UndoHistoryContent (Session& s) : session (s)
    {
        addAndMakeVisible (list);
        list.setModel (this);
        list.setRowHeight (20);
        for (auto* b : { &undoBtn, &redoBtn, &clearBtn }) addAndMakeVisible (b);
        undoBtn.onClick = [this] { if (session.edit) session.edit->undo(); refresh(); };
        redoBtn.onClick = [this] { if (session.edit) session.edit->redo(); refresh(); };
        clearBtn.onClick = [this] { if (session.edit) session.edit->getUndoManager().clearUndoHistory(); refresh(); };
        startTimerHz (2);
        setSize (420, 380);
    }
    void refresh()
    {
        items.clear();
        if (session.edit)
        {
            auto& um = session.edit->getUndoManager();
            auto undos = um.getUndoDescriptions(), redos = um.getRedoDescriptions();
            for (int i = undos.size(); --i >= 0;) items.add ("  " + (undos[i].isEmpty() ? "Edit" : undos[i]));
            splitIndex = items.size();
            for (auto& r : redos) items.add ("  (redo) " + (r.isEmpty() ? "Edit" : r));
        }
        list.updateContent(); list.repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto row = r.removeFromBottom (26);
        for (auto* b : { &undoBtn, &redoBtn, &clearBtn }) { b->setBounds (row.removeFromLeft (110)); row.removeFromLeft (4); }
        r.removeFromBottom (6);
        list.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel()); }
private:
    Session& session; juce::ListBox list; juce::StringArray items; int splitIndex = 0;
    juce::TextButton undoBtn { "Undo (" SQ_CMD "+Z)" }, redoBtn { "Redo (" SQ_CMD "+Shift+Z)" }, clearBtn { "Clear history" };
    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
    {
        g.fillAll (row % 2 ? Theme::panel() : Theme::bg());
        g.setColour (row < splitIndex ? Theme::text() : Theme::dim());
        g.setFont (Theme::font (12.0f));
        g.drawText (items[row], 6, 0, w - 8, h, juce::Justification::centredLeft, true);
        if (row == splitIndex - 1) { g.setColour (Theme::acid()); g.drawHorizontalLine (h - 1, 0.0f, (float) w); }
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (session.edit == nullptr) return;
        // undo/redo back to the clicked step
        if (row < splitIndex) { int steps = splitIndex - 1 - row; for (int i = 0; i < steps; ++i) session.edit->undo(); }
        else { int steps = row - splitIndex + 1; for (int i = 0; i < steps; ++i) session.edit->redo(); }
        refresh();
    }
    void timerCallback() override { if (session.edit && items.size() != session.edit->getUndoManager().getUndoDescriptions().size() + session.edit->getUndoManager().getRedoDescriptions().size()) refresh(); }
};

//==============================================================================
class SessionSetupContent : public juce::Component, private juce::Timer
{
public:
    explicit SessionSetupContent (Session& s) : session (s)
    {
        for (auto* l : { &nameL, &fileL, &rateL, &deviceL, &lengthL, &tracksL, &clipsL, &warnL })
        {
            addAndMakeVisible (l);
            l->setFont (Theme::font (12.5f));
            l->setColour (juce::Label::textColourId, Theme::text());
        }
        warnL.setColour (juce::Label::textColourId, Theme::amber());
        for (auto* f : { &tempoField, &meterField, &tcStartField })
        {
            addAndMakeVisible (f);
            f->setEditable (true);
            f->setColour (juce::Label::backgroundColourId, Theme::panel2());
            f->setColour (juce::Label::textColourId, Theme::text());
        }
        tempoField.setTooltip ("Tempo at bar 1 (more tempo changes: the Tempo ruler)");
        meterField.setTooltip ("Meter at bar 1 (more meter changes: the Meter ruler)");
        tcStartField.setTooltip ("Session start timecode, e.g. 01:00:00:00 for picture");
        tempoField.onTextChange = [this] { if (session.edit) { auto bpm = tempoField.getText().getDoubleValue(); if (bpm >= 20 && bpm <= 400) session.edit->tempoSequence.getTempo (0)->setBpm (bpm); } };
        meterField.onTextChange = [this]
        {
            if (session.edit) { auto p = juce::StringArray::fromTokens (meterField.getText(), "/", ""); if (p.size() == 2 && p[0].getIntValue() > 0 && p[1].getIntValue() > 0) { session.edit->tempoSequence.getTimeSig (0)->numerator = p[0].getIntValue(); session.edit->tempoSequence.getTimeSig (0)->denominator = p[1].getIntValue(); } }
        };
        tcStartField.onTextChange = [this]
        {
            if (session.edit == nullptr) return;
            auto parts = juce::StringArray::fromTokens (tcStartField.getText().replaceCharacter (';', ':'), ":", "");
            if (parts.size() != 4) return;
            const auto& r = TimeFmt::tcRates()[(size_t) TimeFmt::tcRateIndex (*session.edit)];
            session.edit->state.setProperty ("sqmTcStart", parts[0].getIntValue() * 3600.0 + parts[1].getIntValue() * 60.0 + parts[2].getIntValue() + parts[3].getIntValue() / r.fps,
                                             &session.edit->getUndoManager());
        };

        addAndMakeVisible (rateBox);
        for (int i = 0; i < (int) std::size (rates); ++i) rateBox.addItem (juce::String (rates[i] / 1000.0, 1) + " kHz", i + 1);
        rateBox.setTooltip ("Session sample rate - the interface is switched to it, and recordings and bounces use it");
        rateBox.onChange = [this] { session.setSessionSampleRate (rates[rateBox.getSelectedId() - 1]); timerCallback(); };

        addAndMakeVisible (bitDepth);
        bitDepth.addItemList ({ "16-bit", "24-bit", "32-bit float (can't clip)" }, 1);
        bitDepth.setTooltip ("Bit depth of new recordings");
        bitDepth.onChange = [this] { const int d[] = { 16, 24, 32 }; session.setRecordBitDepth (d[bitDepth.getSelectedId() - 1]); };

        addAndMakeVisible (tcRate);
        for (int i = 0; i < (int) TimeFmt::tcRates().size(); ++i) tcRate.addItem (juce::String (TimeFmt::tcRates()[(size_t) i].name) + " fps", i + 1);
        tcRate.setTooltip ("Timecode rate for the Timecode ruler and counters");
        tcRate.onChange = [this] { if (session.edit) session.edit->state.setProperty ("sqmTcRate", tcRate.getSelectedId() - 1, &session.edit->getUndoManager()); };

        addAndMakeVisible (panDepth);
        panDepth.addItemList (Session::panLawNames(), 1);
        panDepth.setTooltip ("Pan depth: how much a centred signal is lowered when it is panned, so panning keeps equal loudness (Pro Tools default -3 dB)");
        panDepth.onChange = [this] { session.setPanLaw (panDepth.getSelectedId() - 1); };

        addAndMakeVisible (revealBtn); revealBtn.onClick = [this] { session.getFile().revealToUser(); };
        addAndMakeVisible (audioBtn);  audioBtn.onClick = [this] { if (onShowAudioSettings) onShowAudioSettings(); };
        addAndMakeVisible (matchBtn);  matchBtn.onClick = [this] { session.matchInterfaceToSessionRate(); timerCallback(); };
        startTimerHz (2);
        setSize (600, 430);
        timerCallback();
    }
    std::function<void()> onShowAudioSettings;
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        auto row = [&] (juce::Component& c) { c.setBounds (r.removeFromTop (22)); r.removeFromTop (3); };
        row (nameL); row (fileL); row (rateL); row (deviceL); row (lengthL); row (tracksL); row (clipsL);
        r.removeFromTop (16);
        auto t = r.removeFromTop (24);
        rateBox.setBounds (t.removeFromLeft (120)); t.removeFromLeft (8); bitDepth.setBounds (t.removeFromLeft (200)); t.removeFromLeft (8); tcRate.setBounds (t.removeFromLeft (120)); t.removeFromLeft (8); tcStartField.setBounds (t);
        r.removeFromTop (16);
        auto t2 = r.removeFromTop (24);
        tempoField.setBounds (t2.removeFromLeft (120)); t2.removeFromLeft (8); meterField.setBounds (t2.removeFromLeft (80)); t2.removeFromLeft (8); panDepth.setBounds (t2.removeFromLeft (240));
        r.removeFromTop (6);
        warnL.setBounds (r.removeFromTop (34));
        auto b = r.removeFromBottom (26);
        revealBtn.setBounds (b.removeFromLeft (190)); b.removeFromLeft (8); audioBtn.setBounds (b.removeFromLeft (200)); b.removeFromLeft (8); matchBtn.setBounds (b);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f, true));
        auto y1 = rateBox.getY() - 12, y2 = tempoField.getY() - 12;
        g.drawText ("SAMPLE RATE", rateBox.getX(), y1, 120, 10, juce::Justification::left);
        g.drawText ("RECORD BIT DEPTH", bitDepth.getX(), y1, 200, 10, juce::Justification::left);
        g.drawText ("TIMECODE RATE", tcRate.getX(), y1, 120, 10, juce::Justification::left);
        g.drawText ("START TIMECODE", tcStartField.getX(), y1, 140, 10, juce::Justification::left);
        g.drawText ("TEMPO AT BAR 1", tempoField.getX(), y2, 120, 10, juce::Justification::left);
        g.drawText ("METER", meterField.getX(), y2, 80, 10, juce::Justification::left);
        g.drawText ("PAN DEPTH (mono and dual stereo panners)", panDepth.getX(), y2, 300, 10, juce::Justification::left);
    }
private:
    static constexpr double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    Session& session;
    juce::Label nameL, fileL, rateL, deviceL, lengthL, tracksL, clipsL, warnL, tempoField, meterField, tcStartField;
    juce::ComboBox rateBox, bitDepth, tcRate, panDepth;
    juce::TextButton revealBtn { SQ_FILE_BROWSER_SHOW_SESSION }, audioBtn { "Playback Engine (audio device)..." }, matchBtn { "Set interface to session rate" };
    void timerCallback() override
    {
        if (session.edit == nullptr) return;
        auto& e = *session.edit;
        auto& dm = session.getEngine().getDeviceManager();
        nameL.setText ("Session:  " + session.getFile().getFileNameWithoutExtension(), juce::dontSendNotification);
        fileL.setText ("File:  " + session.getFile().getFullPathName(), juce::dontSendNotification);
        rateL.setText ("Interface running at:  " + juce::String (dm.getSampleRate() / 1000.0, 1) + " kHz     Buffer:  " + juce::String (dm.getBlockSize()) + " samples", juce::dontSendNotification);
        auto dev = dm.deviceManager.getCurrentAudioDevice();
        deviceL.setText ("Audio device:  " + (dev ? dev->getName() : juce::String ("none")), juce::dontSendNotification);
        auto len = e.getLength().inSeconds();
        lengthL.setText ("Length:  " + session.barsBeatsString (te::toPosition (e.getLength())) + "  (" + juce::String::formatted ("%d:%02d", (int) len / 60, (int) len % 60) + ")", juce::dontSendNotification);
        int clips = 0; for (auto t : session.getTracks()) clips += t->getClips().size();
        tracksL.setText ("Tracks:  " + juce::String (session.getTracks().size()) + "   Folders/VCAs: " + juce::String (session.getFolderTracks().size()) + "   Groups: " + juce::String ((int) session.getGroups().size()) + "   Markers: " + juce::String (session.getMarkers().size()), juce::dontSendNotification);
        clipsL.setText ("Clips:  " + juce::String (clips) + "     Squelchmoth " + JUCE_APPLICATION_VERSION_STRING + " - created and owned by Jamar Stewart", juce::dontSendNotification);
        if (! tempoField.isBeingEdited()) tempoField.setText (juce::String (e.tempoSequence.getTempo (0)->getBpm(), 2), juce::dontSendNotification);
        if (! meterField.isBeingEdited()) meterField.setText (juce::String (e.tempoSequence.getTimeSig (0)->numerator.get()) + "/" + juce::String (e.tempoSequence.getTimeSig (0)->denominator.get()), juce::dontSendNotification);
        if (! tcStartField.isBeingEdited()) tcStartField.setText (TimeFmt::timecode (e, 0.0), juce::dontSendNotification);

        const double sr = TimeFmt::sampleRate (e);
        int ri = 0; for (int i = 0; i < (int) std::size (rates); ++i) if (std::abs (rates[i] - sr) < 1) ri = i + 1;
        if (! rateBox.isPopupActive()) rateBox.setSelectedId (ri, juce::dontSendNotification);
        const int bd = session.recordBitDepth();
        if (! bitDepth.isPopupActive()) bitDepth.setSelectedId (bd == 16 ? 1 : bd == 24 ? 2 : 3, juce::dontSendNotification);
        if (! tcRate.isPopupActive()) tcRate.setSelectedId (TimeFmt::tcRateIndex (e) + 1, juce::dontSendNotification);
        if (! panDepth.isPopupActive()) panDepth.setSelectedId (session.getPanLaw() + 1, juce::dontSendNotification);

        const bool mismatch = dm.getSampleRate() > 0 && std::abs (dm.getSampleRate() - sr) > 1;
        warnL.setText (mismatch ? "The interface is at " + juce::String (dm.getSampleRate() / 1000.0, 1) + " kHz, the session is " + juce::String (sr / 1000.0, 1)
                                  + " kHz - playback is resampled. Click \"Set interface to session rate\" or pick an interface that supports it."
                                : juce::String ("Interface and session sample rates match."), juce::dontSendNotification);
        warnL.setColour (juce::Label::textColourId, mismatch ? Theme::amber() : Theme::dim());
        matchBtn.setEnabled (mismatch);
    }
};

//==============================================================================
class PluginsContent : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    PluginsContent (Session& s, PluginScanner& sc) : session (s), scanner (sc)
    {
        auto& pm = session.getEngine().getPluginManager();
        list = std::make_unique<juce::PluginListComponent> (pm.pluginFormatManager, pm.knownPluginList, PluginScanner::deadMansPedalFile(), nullptr, true);
        addAndMakeVisible (*list);
        for (auto* b : { &scanBtn, &rescanBtn, &cancelBtn, &addPathBtn, &clearDeadBtn }) addAndMakeVisible (b);
        scanBtn.onClick = [this] { scanner.startScan (false); };
        rescanBtn.onClick = [this] { scanner.startScan (true); };
        cancelBtn.onClick = [this] { scanner.cancel(); };
        clearDeadBtn.onClick = [this] { PluginScanner::deadMansPedalFile().deleteFile(); session.log.add ("You", "Cleared the list of plugins that crashed during scanning"); };
        addPathBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Add a folder to scan for plugins");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().isDirectory())
                {
                    auto paths = Prefs::getString ("pl_extraPaths");
                    Prefs::set ("pl_extraPaths", (paths.isEmpty() ? "" : paths + ";") + fc.getResult().getFullPathName());
                    scanner.startScan (false);
                }
            });
        };
        addAndMakeVisible (progress);
        addAndMakeVisible (status);
        status.setFont (Theme::font (11.5f));
        status.setColour (juce::Label::textColourId, Theme::text());
        scanner.addChangeListener (this);
        startTimerHz (4);
        setSize (960, 620);
    }
    ~PluginsContent() override { scanner.removeChangeListener (this); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto top = r.removeFromTop (26);
        for (auto* b : { &scanBtn, &rescanBtn, &cancelBtn, &addPathBtn, &clearDeadBtn }) { b->setBounds (top.removeFromLeft (150)); top.removeFromLeft (4); }
        r.removeFromTop (6);
        progress.setBounds (r.removeFromTop (18));
        r.removeFromTop (4);
        status.setBounds (r.removeFromTop (20));
        r.removeFromTop (4);
        list->setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel()); }
private:
    Session& session; PluginScanner& scanner;
    std::unique_ptr<juce::PluginListComponent> list;
    juce::TextButton scanBtn { "Scan for new plugins" }, rescanBtn { "Rescan everything" }, cancelBtn { "Cancel scan" }, addPathBtn { "Add plugin folder..." }, clearDeadBtn { "Forget crashed plugins" };
    double prog = 0; juce::ProgressBar progress { prog }; juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { timerCallback(); }
    void timerCallback() override
    {
        prog = scanner.isScanning() ? scanner.getProgress() : (scanner.getLastScanTime().toMilliseconds() > 0 ? 1.0 : 0.0);
        auto n = session.getEngine().getPluginManager().knownPluginList.getNumTypes();
        juce::String s = scanner.isScanning() ? "Scanning: " + juce::File (scanner.getCurrentPlugin()).getFileNameWithoutExtension()
                                              : juce::String (n) + " third-party plugins ready (VST3 / Audio Units found automatically)";
        if (! scanner.isScanning() && scanner.getLastScanTime().toMilliseconds() > 0) s << "   -   last scan " << scanner.getLastScanTime().formatted ("%d %b %H:%M");
        if (auto f = scanner.getFailed(); ! f.isEmpty()) s << "   -   " << f.size() << " failed/crashed (skipped)";
        status.setText (s, juce::dontSendNotification);
        cancelBtn.setEnabled (scanner.isScanning()); scanBtn.setEnabled (! scanner.isScanning()); rescanBtn.setEnabled (! scanner.isScanning());
    }
};

//==============================================================================
/** Preferences: Display (colours, palette, UI scale), Operation, Editing, Mixing, MIDI, Plugins, Claude. */
class PreferencesContent : public juce::Component
{
public:
    explicit PreferencesContent (Session& s) : session (s)
    {
        tabs.setTabBarDepth (30);
        tabs.setOutline (0);
        tabs.addTab ("Display", Theme::panel(), buildDisplay(), true);
        tabs.addTab ("Operation", Theme::panel(), buildPanel ({
            { "bool", "op_playFromSelection", "Play starts at the Edit selection (otherwise at the playhead)", 1 },
            { "bool", "op_linkTimelineEdit", "Link Timeline and Edit selection (selection drives the loop range)", 1 },
            { "choice", "op_scrollMode", "Edit window scrolling during playback", 1, 0, 2, "No Auto-Scrolling;Page;Continuous (playhead centred)" },
            { "bool", "op_returnToStart", "Stop returns to where play started (off = Timeline Insertion Follows Playback)", 1 },
            { "bool", "op_scrollToTrack", "Scroll to a track when it is selected", 1 },
            { "bool", "op_numericKeypad", "Numeric keypad transport mode (0 play/stop, 3 record, Enter new marker)", 1 },
            { "bool", "op_autoSave", "Auto-Save the session (atomic, never during playback or recording)", 1 },
            { "num", "op_autoSaveMinutes", "Auto-Save every (minutes)", 2, 1, 99 },
            { "bool", "op_autoBackup", "Auto Backup: keep timed copies in the Session File Backups folder", 1 },
            { "num", "op_autoBackupMinutes", "Auto Backup every (minutes)", 3, 1, 99 },
            { "num", "op_autoBackupKeep", "Auto Backup: keep this many most recent", 20, 1, 999 },
            { "bool", "op_autoBackupMirror", "Auto Backup: also keep a copy on the system drive", 1 },
            { "num", "op_preRoll", "Pre-roll length (seconds)", 2, 0, 60 },
            { "num", "op_postRoll", "Post-roll length (seconds)", 2, 0, 60 },
            { "choice", "op_countOffBars", "Count-off before recording", 0, 0, 2, "Off;1 bar;2 bars" },
            { "bool", "op_showWelcome", "Show the Quick Start window at launch", 1 } }), true);
        tabs.addTab ("Editing", Theme::panel(), buildPanel ({
            { "num", "ed_fadeCmd", "Fade length for " SQ_CMD "+F / clip menu fades (seconds)", 0.25, 0.005, 10 },
            { "choice", "ed_fadeShape", "Fade shape", 0, 0, 3, "linear;convex;concave;s-curve" },
            { "bool", "ed_autoFades", "Add 4 ms anti-click fades on every separate / trim / move", 1 },
            { "num", "ed_stripThresholdDb", "Strip Silence threshold (dBFS)", -48, -96, 0 },
            { "num", "ed_stripMinLen", "Strip Silence minimum length (seconds)", 0.25, 0.01, 5 },
            { "num", "ed_stripPad", "Strip Silence clip padding (seconds)", 0.02, 0, 1 },
            { "bool", "ed_clipNames", "Show clip names", 1 },
            { "bool", "ed_clipGainLine", "Show clip gain line", 1 },
            { "bool", "ed_zoomToggle", "Zoom keys centre on the Edit selection", 1 },
            { "num", "ed_trackHeight", "Default track height (pixels)", 84, 30, 320 },
            { "bool", "ed_recordNewPlaylist", "Record to a new playlist (take) on every pass", 0 } }), true);
        tabs.addTab ("Mixing", Theme::panel(), buildPanel ({
            { "num", "mx_newTrackDb", "New track fader level (dB)", 0, -60, 6 },
            { "num", "mx_newSendDb", "New send level (dB)", -10, -60, 6 },
            { "bool", "mx_sendsPost", "Sends default to post-fader", 1 },
            { "num", "mx_meterHold", "Meter peak hold (ms)", 1500, 0, 10000 },
            { "num", "mx_meterDecay", "Meter decay (dB per second)", 36, 6, 120 },
            { "bool", "mx_soloLatch", "Solo latching (several tracks can be soloed)", 1 },
            { "bool", "mx_groupsActive", "Mix groups active (link grouped faders / mutes / solos)", 1 },
            { "bool", "mx_narrow", "Narrow mixer strips", 0 },
            { "bool", "mx_showInserts", "Show inserts in the Mix window", 1 },
            { "bool", "mx_showSends", "Show sends in the Mix window", 1 } }), true);
        tabs.addTab ("MIDI", Theme::panel(), buildPanel ({
            { "num", "mi_velocity", "Default note velocity", 100, 1, 127 },
            { "choice", "mi_quantizeDiv", "Default quantize", 2, 0, 5, "1/4;1/8;1/16;1/32;1/8 triplet;1/16 triplet" },
            { "bool", "mi_thru", "MIDI Thru (play instruments live from your keyboard)", 1 },
            { "bool", "mi_auditionNotes", "Play notes while editing them in the MIDI editor", 1 },
            { "bool", "op_midiMerge", "MIDI Merge (new recordings merge into existing clips)", 0 },
            { "bool", "op_waitForNote", "Wait for Note when recording", 0 } }), true);
        tabs.addTab ("Plugins", Theme::panel(), buildPanel ({
            { "choice", "pl_menuOrg", "Organize plug-in menus by", 0, 0, 3, "Category (Pro Tools);Manufacturer;Category and Manufacturer;Flat list" },
            { "choice", "pl_formatPref", "Plug-ins installed as both VST3 and Audio Unit: show", 0, 0, 2, "VST3 only;Audio Unit only;Both" },
            { "bool", "pl_scanAtLaunch", "Find new plugins automatically at every launch", 1 },
            { "text", "pl_extraPaths", "Extra plugin folders (separate with ;)", 0 },
            { "bool", "pl_openWindowOnInsert", "Open the plugin window when a plugin is inserted", 1 },
            { "bool", "pl_outOfProcessScan", "Scan each plugin in a separate process (safer, slower)", 1 } }), true);
        tabs.addTab ("Claude", Theme::panel(), buildPanel ({
            { "bool", "cl_autoConnect", "Connect to Claude automatically at launch", 1 },
            { "choice", "cl_provider", "Link provider", 0, 0, 2, "Cloudflare quick tunnel (no account);ngrok (free account, most reliable, fixed address);Pinggy over SSH port 443 (no account, 60-min sessions)" },
            { "text", "cl_ngrokToken", "ngrok authtoken (only for the ngrok provider)", 0 },
            { "text", "cl_ngrokDomain", "ngrok static domain, e.g. yourname.ngrok-free.app (optional)", 0 },
            { "bool", "cl_screenshots", "Claude may take screenshots of the app", 1 },
            { "bool", "cl_deleteTracks", "Claude may delete tracks and clips", 1 },
            { "bool", "cl_save", "Claude may save and bounce", 1 },
            { "bool", "cl_scan", "Claude may start plugin scans", 1 },
            { "bool", "cl_verbose", "Verbose Claude log (every request in the Claude panel)", 0 } }), true);
        addAndMakeVisible (tabs);
        setSize (820, 600);
    }

    void resized() override { tabs.setBounds (getLocalBounds()); }
    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel()); }

    void showTab (const juce::String& name)
    {
        for (int i = 0; i < tabs.getNumTabs(); ++i) if (tabs.getTabNames()[i] == name) tabs.setCurrentTabIndex (i);
    }

private:
    Session& session;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    struct Item { const char* type; const char* key; const char* label; double def; double lo = 0, hi = 1; const char* choices = ""; };

    /** A property panel whose Values read/write Prefs live. */
    struct BoundPanel : public juce::Component, private juce::Value::Listener
    {
        BoundPanel (std::vector<Item> items)
        {
            addAndMakeVisible (panel);
            juce::Array<juce::PropertyComponent*> comps;
            for (auto& it : items)
            {
                auto v = std::make_shared<juce::Value>();
                juce::String type (it.type);
                if (type == "bool") { v->setValue (Prefs::getBool (it.key, it.def != 0)); comps.add (new juce::BooleanPropertyComponent (*v, it.label, "")); }
                else if (type == "num")
                {
                    v->setValue (Prefs::getDouble (it.key, it.def));
                    auto sp = new juce::SliderPropertyComponent (*v, it.label, it.lo, it.hi, (it.hi - it.lo) > 50 ? 1.0 : 0.005);
                    comps.add (sp);
                }
                else if (type == "choice")
                {
                    juce::StringArray names = juce::StringArray::fromTokens (juce::String (it.choices), ";", "");
                    juce::Array<juce::var> vals;
                    bool stringKeyed = juce::String (it.key) == "ed_fadeShape" || juce::String (it.key) == "cl_provider";
                    juce::StringArray providerKeys { "cloudflare", "ngrok", "pinggy" };
                    for (int i = 0; i < names.size(); ++i) vals.add (stringKeyed ? juce::var (juce::String (it.key) == "cl_provider" ? providerKeys[i] : names[i]) : juce::var (i));
                    if (stringKeyed) v->setValue (Prefs::getString (it.key, juce::String (it.key) == "cl_provider" ? "cloudflare" : names[(int) it.def]));
                    else v->setValue (Prefs::getInt (it.key, (int) it.def));
                    comps.add (new juce::ChoicePropertyComponent (*v, it.label, names, vals));
                }
                else { v->setValue (Prefs::getString (it.key)); comps.add (new juce::TextPropertyComponent (*v, it.label, 400, false)); }
                v->addListener (this);
                values.push_back ({ v, it.key });
            }
            panel.addProperties (comps);
        }
        void resized() override { panel.setBounds (getLocalBounds().reduced (10)); }
        void valueChanged (juce::Value& v) override
        {
            for (auto& [val, key] : values) if (val->refersToSameSourceAs (v)) { Prefs::set (key, v.getValue()); return; }
        }
        juce::PropertyPanel panel;
        std::vector<std::pair<std::shared_ptr<juce::Value>, const char*>> values;
    };

    juce::Component* buildPanel (std::vector<Item> items)   { return new BoundPanel (std::move (items)); }

    //==========================================================================
    struct ColourSwatch : public juce::Component
    {
        ColourSwatch (const juce::String& n, std::function<juce::Colour()> g, std::function<void (juce::Colour)> s) : name (n), get (std::move (g)), set (std::move (s)) {}
        void paint (juce::Graphics& gr) override
        {
            auto r = getLocalBounds().toFloat();
            gr.setColour (get()); gr.fillRoundedRectangle (r.removeFromLeft (44.0f).reduced (2.0f), 4.0f);
            gr.setColour (Theme::line()); gr.drawRoundedRectangle (getLocalBounds().toFloat().removeFromLeft (44.0f).reduced (2.0f), 4.0f, 1.0f);
            gr.setColour (Theme::text()); gr.setFont (Theme::font (12.0f));
            gr.drawText (name, getLocalBounds().withTrimmedLeft (50), juce::Justification::centredLeft);
        }
        void mouseDown (const juce::MouseEvent&) override
        {
            auto cs = std::make_unique<juce::ColourSelector> (juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders | juce::ColourSelector::showColourAtTop | juce::ColourSelector::editableColour);
            cs->setCurrentColour (get());
            cs->setSize (320, 380);
            struct L : public juce::ChangeListener { std::function<void (juce::Colour)> set; void changeListenerCallback (juce::ChangeBroadcaster* b) override { if (auto c = dynamic_cast<juce::ColourSelector*> (b)) set (c->getCurrentColour()); } };
            auto l = std::make_shared<L>(); l->set = set;
            cs->addChangeListener (l.get());
            auto& box = juce::CallOutBox::launchAsynchronously (std::move (cs), getScreenBounds(), nullptr);
            struct Holder : public juce::ComponentListener { std::shared_ptr<L> l; void componentBeingDeleted (juce::Component&) override { delete this; } };
            auto h = new Holder(); h->l = l; box.addComponentListener (h);
        }
        juce::String name; std::function<juce::Colour()> get; std::function<void (juce::Colour)> set;
    };

    struct DisplayPanel : public juce::Component
    {
        explicit DisplayPanel (Session& s) : session (s)
        {
            presetBox.addItem ("Custom", 1);
            for (int i = 0; i < (int) Theme::presets().size(); ++i) presetBox.addItem (Theme::presets()[(size_t) i].name, i + 2);
            presetBox.onChange = [this]
            {
                auto idx = presetBox.getSelectedId() - 2;
                if (idx >= 0 && idx < (int) Theme::presets().size()) { Theme::applyPreset (Theme::presets()[(size_t) idx]); commit(); }
            };
            addAndMakeVisible (presetBox);
            addAndMakeVisible (presetLabel);
            presetLabel.setText ("Theme preset", juce::dontSendNotification);
            presetLabel.setFont (Theme::font (12.0f));

            auto& c = Theme::current();
            auto add = [&] (const juce::String& n, juce::Colour& ref)
            {
                auto sw = std::make_unique<ColourSwatch> (n, [&ref] { return ref; }, [this, &ref] (juce::Colour col) { ref = col; Theme::current().name = "Custom"; commit(); });
                addAndMakeVisible (*sw);
                swatches.push_back (std::move (sw));
            };
            add ("Edit window background", c.bg);
            add ("Mix window background", c.mixBg);
            add ("Panels (headers, toolbars)", c.panel);
            add ("Raised panels / fields", c.panel2);
            add ("Lines & grid base", c.line);
            add ("Text", c.text);
            add ("Dim text", c.dim);
            add ("Accent 1 - playhead, selection (acid)", c.acid);
            add ("Accent 2 - play, loop (mint)", c.mint);
            add ("Accent 3 (magenta)", c.magenta);
            add ("Buttons / selected (violet)", c.violet);
            add ("Record / clip (red)", c.red);
            add ("Solo / markers (amber)", c.amber);
            add ("Waveforms (transparent = track colour)", c.waveform);
            add ("Grid lines (transparent = from lines)", c.grid);

            for (int i = 0; i < c.trackPalette.size(); ++i)
            {
                auto sw = std::make_unique<ColourSwatch> ("", [i] { return Theme::trackPalette()[i]; }, [this, i] (juce::Colour col) { Theme::current().trackPalette.set (i, col); commit(); });
                addAndMakeVisible (*sw);
                palette.push_back (std::move (sw));
            }
            paletteLabel.setText ("Track colour palette (new tracks cycle through these; right-click a track > Colour)", juce::dontSendNotification);
            paletteLabel.setFont (Theme::font (12.0f));
            addAndMakeVisible (paletteLabel);

            scale.setRange (0.8, 1.6, 0.05);
            scale.setValue (c.uiScale, juce::dontSendNotification);
            scale.setTextValueSuffix ("x");
            scale.onValueChange = [this] { Theme::current().uiScale = (float) scale.getValue(); commit(); };
            addAndMakeVisible (scale);
            scaleLabel.setText ("Text size", juce::dontSendNotification);
            scaleLabel.setFont (Theme::font (12.0f));
            addAndMakeVisible (scaleLabel);

            resetBtn.onClick = [this] { Theme::applyPreset (Theme::presets()[0]); Theme::current().trackPalette = Theme::Colours().trackPalette; Theme::current().waveform = juce::Colour(); Theme::current().grid = juce::Colour(); Theme::current().uiScale = 1.0f; commit(); };
            addAndMakeVisible (resetBtn);
            refreshPreset();
        }
        void commit()
        {
            if (Prefs::file()) Theme::save (*Prefs::file());
            Theme::changed();
            refreshPreset();
            repaint();
            for (auto& s : swatches) s->repaint();
            for (auto& s : palette) s->repaint();
        }
        void refreshPreset()
        {
            int id = 1;
            for (int i = 0; i < (int) Theme::presets().size(); ++i) if (Theme::current().name == Theme::presets()[(size_t) i].name) id = i + 2;
            presetBox.setSelectedId (id, juce::dontSendNotification);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (12);
            auto top = r.removeFromTop (26);
            presetLabel.setBounds (top.removeFromLeft (100)); presetBox.setBounds (top.removeFromLeft (220)); top.removeFromLeft (20); resetBtn.setBounds (top.removeFromLeft (180));
            r.removeFromTop (10);
            auto cols = r.removeFromTop (8 * 28);
            auto left = cols.removeFromLeft (cols.getWidth() / 2), right = cols;
            for (size_t i = 0; i < swatches.size(); ++i)
            {
                auto& area = i < 8 ? left : right;
                swatches[i]->setBounds (area.removeFromTop (26)); area.removeFromTop (2);
            }
            r.removeFromTop (12);
            paletteLabel.setBounds (r.removeFromTop (20));
            auto pal = r.removeFromTop (30);
            for (auto& p : palette) { p->setBounds (pal.removeFromLeft (46).withWidth (44)); }
            r.removeFromTop (14);
            auto sc = r.removeFromTop (26);
            scaleLabel.setBounds (sc.removeFromLeft (100)); scale.setBounds (sc.removeFromLeft (300));
        }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel());
            g.setColour (Theme::dim()); g.setFont (Theme::font (10.5f));
            g.drawText ("Click any swatch to pick a colour. Changes apply instantly to the Edit and Mix windows and are saved.", 12, getHeight() - 26, getWidth() - 24, 20, juce::Justification::left);
        }
        Session& session;
        juce::ComboBox presetBox; juce::Label presetLabel, paletteLabel, scaleLabel; juce::Slider scale; juce::TextButton resetBtn { "Reset to Moth defaults" };
        std::vector<std::unique_ptr<ColourSwatch>> swatches, palette;
    };

    juce::Component* buildDisplay()   { return new DisplayPanel (session); }
};

//==============================================================================
/** Color Palette window: click a colour to apply it to the selected tracks, clips or markers. */
class ColorPaletteContent : public juce::Component, private juce::ChangeListener
{
public:
    ColorPaletteContent (Session& s, TimelineState& st) : session (s), state (st)
    {
        target.addItemList ({ "Apply to: Tracks", "Apply to: Clips", "Apply to: Markers" }, 1);
        target.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (target);
        addAndMakeVisible (editBtn);
        editBtn.onClick = [this] { if (onEditPalette) onEditPalette(); };
        addAndMakeVisible (defaultBtn);
        defaultBtn.onClick = [this] { apply (juce::Colour()); };
        Theme::broadcaster().addChangeListener (this);
        setSize (420, 190);
    }
    ~ColorPaletteContent() override { Theme::broadcaster().removeChangeListener (this); }
    std::function<void()> onEditPalette, onChanged;
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        auto& pal = Theme::trackPalette();
        auto area = getLocalBounds().reduced (10).withTrimmedTop (34).withTrimmedBottom (10);
        int cols = 8, rows = (pal.size() + cols - 1) / cols;
        float w = area.getWidth() / (float) cols, h = area.getHeight() / (float) juce::jmax (1, rows);
        for (int i = 0; i < pal.size(); ++i)
        {
            auto r = juce::Rectangle<float> (area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h).reduced (3.0f);
            g.setColour (pal[i]); g.fillRoundedRectangle (r, 5.0f);
            if (i == hover) { g.setColour (Theme::text()); g.drawRoundedRectangle (r, 5.0f, 2.0f); }
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10).removeFromTop (26);
        target.setBounds (r.removeFromLeft (170)); r.removeFromLeft (6);
        defaultBtn.setBounds (r.removeFromLeft (90)); r.removeFromLeft (6);
        editBtn.setBounds (r);
    }
    void mouseMove (const juce::MouseEvent& e) override { hover = indexAt (e.position); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { auto i = indexAt (e.position); if (i >= 0) apply (Theme::trackPalette()[i]); }
private:
    Session& session; TimelineState& state; juce::ComboBox target; juce::TextButton editBtn { "Edit palette..." }, defaultBtn { "Default" }; int hover = -1;
    int indexAt (juce::Point<float> p)
    {
        auto& pal = Theme::trackPalette();
        auto area = getLocalBounds().reduced (10).withTrimmedTop (34).withTrimmedBottom (10);
        if (! area.toFloat().contains (p)) return -1;
        int cols = 8, rows = (pal.size() + cols - 1) / cols;
        float w = area.getWidth() / (float) cols, h = area.getHeight() / (float) juce::jmax (1, rows);
        int i = (int) ((p.y - area.getY()) / h) * cols + (int) ((p.x - area.getX()) / w);
        return i >= 0 && i < pal.size() ? i : -1;
    }
    void apply (juce::Colour c)
    {
        if (session.edit == nullptr) return;
        auto mode = target.getSelectedId();
        if (mode == 1) { for (auto t : session.getRows()) if (state.isTrackSelected (t)) t->setColour (c.isTransparent() ? Theme::nextTrackColour (session.getRows().indexOf (t)) : c); }
        else if (mode == 2) { for (auto& cl : state.selectedClips) { if (c.isTransparent()) cl->state.removeProperty (te::IDs::colour, nullptr); else cl->setColour (c); } }
        else { for (auto m : session.getMarkers()) if (state.timeSelection && state.timeSelection->contains (m->getPosition().getStart())) m->setColour (c); }
        if (onChanged) onChanged();
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
};

//==============================================================================
/** Quick Start / Welcome window. */
class WelcomeContent : public juce::Component
{
public:
    WelcomeContent()
    {
        if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::logo_svg, BinaryData::logo_svgSize)))
            logo = juce::Drawable::createFromSVG (*svg);
        banner = juce::ImageCache::getFromMemory (BinaryData::welcome_banner_jpg, BinaryData::welcome_banner_jpgSize);
        for (auto* b : { &newBtn, &openBtn, &templateBtn, &claudeBtn, &tourBtn, &beatBtn })
        {
            addAndMakeVisible (b);
            b->setColour (juce::TextButton::buttonColourId, Theme::panel2());
        }
        newBtn.setColour (juce::TextButton::buttonColourId, Theme::violet());
        beatBtn.setColour (juce::TextButton::buttonColourId, Theme::amber()); beatBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        beatBtn.setTooltip ("A new session with a kit in the Channel Rack, looping and ready for steps - the FL way, in under a minute");
        showAgain.setToggleState (Prefs::showWelcome(), juce::dontSendNotification);
        showAgain.onClick = [this] { Prefs::set ("op_showWelcome", showAgain.getToggleState()); };
        addAndMakeVisible (showAgain);
        setSize (760, 560);
    }
    std::function<void()> onNew, onOpen, onTemplate, onClaude, onTour, onBeat;
    void resized() override
    {
        auto r = getLocalBounds().reduced (24);
        r.removeFromTop (250);
        auto row = r.removeFromTop (44);
        auto w = (row.getWidth() - 4 * 8) / 5;
        newBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (8);
        beatBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (8);
        openBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (8);
        templateBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (8);
        claudeBtn.setBounds (row);
        r.removeFromTop (12);
        tourBtn.setBounds (r.removeFromTop (30).withSizeKeepingCentre (260, 28));
        beatBtn.onClick = [this] { if (onBeat) onBeat(); };
        showAgain.setBounds (getLocalBounds().reduced (24).removeFromBottom (24).removeFromLeft (260));
        newBtn.onClick = [this] { if (onNew) onNew(); };
        openBtn.onClick = [this] { if (onOpen) onOpen(); };
        templateBtn.onClick = [this] { if (onTemplate) onTemplate(); };
        claudeBtn.onClick = [this] { if (onClaude) onClaude(); };
        tourBtn.onClick = [this] { if (onTour) onTour(); };
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0d0714));
        if (banner.isValid())
        {
            auto area = getLocalBounds().removeFromTop (250).toFloat();
            g.drawImage (banner, area, juce::RectanglePlacement::fillDestination);
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0, 150, juce::Colour (0xff0d0714), 0, 250, false));
            g.fillRect (area);
        }
        if (logo) logo->drawWithin (g, juce::Rectangle<float> (24, 150, 86, 86), juce::RectanglePlacement::centred, 1.0f);
        g.setColour (juce::Colour (0xffd4ff3a)); g.setFont (Theme::font (30.0f, true));
        g.drawText ("SQUELCHMOTH", 124, 156, 460, 40, juce::Justification::left);
        g.setColour (juce::Colour (0xffece6ff)); g.setFont (Theme::font (13.5f));
        {
            int inst = 0, fx = 0; for (auto& e : Signature::entries()) (juce::String (e.category) == "Instruments" ? inst : fx)++;
            g.drawFittedText ("Pro Tools editing, Studio One mastering, the FL Studio Channel Rack - and Claude at the controls. " + juce::String (inst) + " instruments, "
                              + juce::String (fx) + " Signature effects, 186 more effects, 27 kits, the Cut Lab editor, patterns, pages and a live link to claude.ai.", 124, 194, 600, 40, juce::Justification::topLeft, 2);
        }
        g.setColour (juce::Colour (0xff8f84aa)); g.setFont (Theme::font (11.0f));
        g.drawText ("Created and owned by Jamar Stewart", 124, 234, 460, 16, juce::Justification::left);
        g.drawFittedText ("Keys: Space play  |  " SQ_CMD "+Space record  |  F1-F4 edit modes  |  F5-F10 tools  |  " SQ_CMD "+Shift+B Beat page  |  " SQ_CMD "+6 Channel Rack  |  " SQ_CMD "+= next page  |  " SQ_CMD "+, Preferences",
                          24, getHeight() - 92, getWidth() - 48, 30, juce::Justification::centred, 2);
    }
private:
    std::unique_ptr<juce::Drawable> logo;
    juce::Image banner;
    juce::TextButton newBtn { "New Session" }, openBtn { "Open Session..." }, templateBtn { "New from Template..." }, claudeBtn { "Connect Claude" }, tourBtn { "Show me around (5 tips)" }, beatBtn { "Make a Beat" };
    juce::ToggleButton showAgain { "Show this window at launch" };
};
