#pragma once
// Squelchmoth 1.13 - the Score window (Window > Score, Cmd+Opt+N): MIDI tracks as an engraved score you can read,
// edit, print and exchange. Edits go straight to the MIDI (one undo step each), so the piano roll, the score and
// Claude always agree.
//
// Mouse: click a note to select (Shift adds), drag up / down to change pitch, Delete removes. With Note Entry on, a
// click on a staff adds a note of the palette's length at that line and beat (sharps / flats from the key; hold Alt for
// a sharp, Shift+Alt for a flat). Keys: arrows up / down = semitone (Shift = octave), left / right = move by the grid,
// 1-7 = whole ... 64th... (1 whole, 2 half, 3 quarter, 4 eighth, 5 16th, 6 32nd), . = dot, T = triplet,
// N = note entry, Cmd+Z undo, +/- zoom.

#include "ScoreModel.h"
#include "ScoreRender.h"
#include "Theme.h"
#include "EditState.h"
#include "ScoreExport.h"

class ScoreView : public juce::Component, private juce::Timer
{
public:
    ScoreView (Session& s) : session (s) { setWantsKeyboardFocus (true); startTimerHz (15); }

    std::function<void()> onChanged;
    juce::Array<te::AudioTrack*> tracks;   // empty = every track with MIDI
    bool pageView = false, noteEntry = false;
    float zoom = 1.0f;
    double paletteBeats = 1.0;
    std::set<std::pair<int, int>> selected;   // (part, source)

    void rebuild()
    {
        if (session.edit == nullptr) { parts.clear(); views.clear(); repaint(); return; }
        auto ts = tracks.isEmpty() ? ScoreModel::midiTracks (session) : tracks;
        parts = ScoreModel::build (session, ts);
        views.clear();
        for (size_t i = 0; i < parts.size(); ++i) views.push_back ({ parts[i].name, &parts[i].score, (int) i });
        relayout();
    }

    void relayout()
    {
        const float sp = 7.0f * zoom;
        const float w = juce::jmax (500.0f, (float) (getParentWidth() > 0 ? getParentWidth() - 16 : 1000));
        layoutData = ScoreRender::layout (views, pageView ? juce::jmin (w, sp * 120.0f) : w, sp, pageView ? juce::jmin (w, sp * 120.0f) * 1.414f : 0.0f,
                                          session.getFile().getFileNameWithoutExtension(), {});
        setSize ((int) layoutData.width, juce::jmax (200, (int) layoutData.totalHeight + 20));
        repaint();
    }

    juce::String summary() const
    {
        int notes = 0; for (auto& p : parts) notes += (int) p.notes.size();
        return juce::String ((int) parts.size()) + " part(s), " + juce::String (parts.empty() ? 0 : (int) parts[0].score.measures.size()) + " bars, " + juce::String (notes) + " notes";
    }

    std::vector<ScoreModel::NoteRef> selectedRefs() const
    {
        std::vector<ScoreModel::NoteRef> out;
        for (auto& [part, src] : selected)
            if (part >= 0 && part < (int) parts.size() && src >= 0 && src < (int) parts[(size_t) part].refs.size()) out.push_back (parts[(size_t) part].refs[(size_t) src]);
        return out;
    }

    void paint (juce::Graphics& g) override
    {
        ScoreRender::Style st;
        g.fillAll (Theme::bg());
        if (views.empty())
        {
            g.setColour (Theme::dim()); g.setFont (Theme::font (15.0f));
            g.drawText ("No MIDI yet. Record or draw MIDI on an instrument track, or import a MusicXML score.", getLocalBounds(), juce::Justification::centred);
            return;
        }
        auto isSel = [this] (int part, int src) { return selected.count ({ part, src }) > 0; };
        double play = -1;
        if (session.edit != nullptr && session.edit->getTransport().isPlaying())
            play = session.edit->tempoSequence.toBeats (session.edit->getTransport().getPosition()).inBeats();
        if (pageView && layoutData.pages)
        {
            allHits.clear();
            for (int p = 0; p < layoutData.numPages; ++p)
            {
                const float y = p * (layoutData.pageHeight + 16.0f);
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRect (6.0f, y + 6.0f, layoutData.width, layoutData.pageHeight);
                g.setColour (st.paper); g.fillRect (0.0f, y, layoutData.width, layoutData.pageHeight);
                juce::Graphics::ScopedSaveState ss (g);
                g.setOrigin (0, juce::roundToInt (y + p * layoutData.pageHeight));
                ScoreRender::paint (g, layoutData, views, p, st, isSel, play);
                for (auto h : layoutData.hits) { h.box = h.box.translated (0, y + p * layoutData.pageHeight); allHits.push_back (h); }
            }
        }
        else
        {
            g.setColour (st.paper); g.fillRect (getLocalBounds());
            ScoreRender::paint (g, layoutData, views, -1, st, isSel, play);
            allHits = layoutData.hits;
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        dragStartY = e.position.y; dragSemis = 0;
        for (auto& h : allHits)
            if (h.box.contains (e.position))
            {
                const std::pair<int, int> key { h.part, h.source };
                if (! e.mods.isShiftDown() && ! selected.count (key)) selected.clear();
                if (e.mods.isShiftDown() && selected.count (key)) selected.erase (key); else selected.insert (key);
                previewSelected();
                repaint(); return;
            }
        if (noteEntry && enterNoteAt (e.position, e.mods)) return;
        selected.clear(); repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (selected.empty() || noteEntry) return;
        const float sp = 7.0f * zoom;
        const int semis = juce::roundToInt ((dragStartY - e.position.y) / (sp * 0.5f));
        if (semis != dragSemis) { ScoreModel::transpose (session, selectedRefs(), semis - dragSemis); dragSemis = semis; changedEdit(); }
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        const auto code = k.getKeyCode(); const bool shift = k.getModifiers().isShiftDown(), cmd = k.getModifiers().isCommandDown();
        const double gridBeats = 4.0 / ScoreModel::options (session).grid;
        if (cmd && code == 'Z') { if (shift) session.edit->redo(); else session.edit->undo(); changedEdit(); return true; }
        if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { session.edit->getUndoManager().beginNewTransaction ("Delete notes"); ScoreModel::remove (session, selectedRefs()); selected.clear(); changedEdit(); return true; }
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
        { session.edit->getUndoManager().beginNewTransaction ("Transpose"); ScoreModel::transpose (session, selectedRefs(), (code == juce::KeyPress::upKey ? 1 : -1) * (shift ? 12 : 1)); changedEdit(); return true; }
        if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
        { session.edit->getUndoManager().beginNewTransaction ("Move notes"); ScoreModel::moveBy (session, selectedRefs(), (code == juce::KeyPress::rightKey ? 1 : -1) * gridBeats); changedEdit(); return true; }
        const auto ch = k.getTextCharacter();
        if (ch >= '1' && ch <= '6') { setPalette (4.0 / std::pow (2.0, ch - '1'), false, false); return true; }
        if (ch == '.') { setPalette (paletteBase, ! paletteDot, paletteTriplet); return true; }
        if (ch == 't' || ch == 'T') { setPalette (paletteBase, paletteDot, ! paletteTriplet); return true; }
        if (ch == 'n' || ch == 'N') { noteEntry = ! noteEntry; if (onChanged) onChanged(); return true; }
        if (ch == '+' || ch == '=') { zoom = juce::jmin (3.0f, zoom * 1.15f); relayout(); return true; }
        if (ch == '-') { zoom = juce::jmax (0.5f, zoom / 1.15f); relayout(); return true; }
        return false;
    }

    /** The palette: base value in beats, dot, triplet. Applies to selected notes too. */
    void setPalette (double base, bool dot, bool triplet)
    {
        paletteBase = base; paletteDot = dot; paletteTriplet = triplet;
        paletteBeats = base * (dot ? 1.5 : 1.0) * (triplet ? 2.0 / 3.0 : 1.0);
        if (! selected.empty())
        {
            session.edit->getUndoManager().beginNewTransaction ("Note length");
            ScoreModel::setLength (session, selectedRefs(), paletteBeats);
            changedEdit();
        }
        if (onChanged) onChanged();
    }
    double paletteBase = 1.0; bool paletteDot = false, paletteTriplet = false;

    /** Note entry: the staff line under the mouse -> pitch (key accidentals, Alt = sharp, Shift+Alt = flat); x -> beat. */
    bool enterNoteAt (juce::Point<float> p, juce::ModifierKeys mods)
    {
        float yOff = 0; int pageOf = -1;
        if (pageView && layoutData.pages)
        {
            pageOf = juce::jlimit (0, layoutData.numPages - 1, (int) (p.y / (layoutData.pageHeight + 16.0f)));
            yOff = pageOf * 16.0f;
        }
        const float sp = layoutData.sp;
        for (auto& sys : layoutData.systems)
        {
            if (pageOf >= 0 && sys.page != pageOf) continue;
            for (size_t si = 0; si < sys.staffTops.size(); ++si)
            {
                const float top = sys.staffTops[si] + yOff, bottom = top + 4 * sp;
                if (p.y < top - sp * 3 || p.y > bottom + sp * 3) continue;
                for (auto& ml : sys.measures)
                {
                    if (p.x < ml.x || p.x >= ml.x + ml.width) continue;
                    const int part = sys.staffPart[si], staffIdx = sys.staffIndex[si];
                    if (part >= (int) parts.size()) return false;
                    auto& pt = parts[(size_t) part];
                    const auto& m = pt.score.measures[(size_t) ml.index];
                    const double grid = 4.0 / ScoreModel::options (session).grid;
                    const double beatInBar = juce::jlimit (0.0, m.lengthBeats - grid, std::round (ScoreRender::beatForX (ml, p.x) / grid) * grid);
                    const int line = juce::roundToInt ((bottom - p.y) / (sp * 0.5f));
                    const bool bass = pt.score.clefs[(size_t) juce::jmin (staffIdx, (int) pt.score.clefs.size() - 1)] == Notation::Clef::bass;
                    const int diat = line + (bass ? 4 + 7 * 2 : 2 + 7 * 4);
                    const int step = ((diat % 7) + 7) % 7, oct = (int) std::floor (diat / 7.0);
                    int alter = Notation::keyAlters (pt.score.key.fifths)[(size_t) step];
                    if (mods.isAltDown()) alter = mods.isShiftDown() ? -1 : 1;
                    const int pitch = (oct + 1) * 12 + Notation::stepToSemis (step) + alter;
                    session.edit->getUndoManager().beginNewTransaction ("Enter note");
                    ScoreModel::addNote (session, *pt.track, m.startBeat + beatInBar, juce::jmin (paletteBeats, m.lengthBeats - beatInBar + 64.0), pitch);
                    changedEdit();
                    return true;
                }
            }
        }
        return false;
    }

    void previewSelected()
    {
        // play the selected notes through their track for a moment, like clicking a note in the piano roll
        for (auto& r : selectedRefs())
            if (r.clip != nullptr && r.note != nullptr)
                if (auto t = dynamic_cast<te::AudioTrack*> (r.clip->getTrack()))
                {
                    static const auto src = te::createUniqueMPESourceID();
                    t->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, r.note->getNoteNumber(), (juce::uint8) 96), src);
                    auto off = juce::MidiMessage::noteOff (1, r.note->getNoteNumber());
                    juce::Timer::callAfterDelay (350, [tw = te::SafeSelectable<te::AudioTrack> (t), off] { if (auto* tr = tw.get()) tr->injectLiveMidiMessage (off, src); });
                }
    }

    void changedEdit() { rebuild(); if (onChanged) onChanged(); }

private:
    Session& session;
    std::vector<ScoreModel::Part> parts;
    std::vector<ScoreRender::PartView> views;
    ScoreRender::Layout layoutData;
    std::vector<ScoreRender::HeadHit> allHits;
    float dragStartY = 0; int dragSemis = 0;
    juce::int64 lastStamp = 0;

    void timerCallback() override
    {
        // follow the edit: rebuild when MIDI changed elsewhere, repaint the playhead while playing
        if (session.edit == nullptr) return;
        juce::int64 stamp = 0;
        for (auto t : session.getTracks()) for (auto c : t->getClips()) if (auto mc = dynamic_cast<te::MidiClip*> (c))
        {
            stamp = stamp * 31 + mc->getSequence().getNumNotes();
            for (auto n : mc->getSequence().getNotes()) stamp = stamp * 1000003 + n->getNoteNumber() * 7919 + (juce::int64) (n->getStartBeat().inBeats() * 960) * 31 + (juce::int64) (n->getLengthBeats().inBeats() * 960);
        }
        stamp += (juce::int64) ScoreModel::optionsTree (session).toXmlString().hashCode64();
        if (stamp != lastStamp) { lastStamp = stamp; rebuild(); }
        else if (session.edit->getTransport().isPlaying()) repaint();
    }
};

//==============================================================================
class ScoreContent : public juce::Component
{
public:
    ScoreContent (Session& s) : session (s), view (s)
    {
        auto combo = [this] (juce::ComboBox& c, const juce::StringArray& items, const juce::String& tip)
        { c.addItemList (items, 1); c.setTooltip (tip); addAndMakeVisible (c); };
        combo (tracksBox, { "All MIDI tracks" }, "Which tracks to show");
        combo (gridBox, { "Quantise display: 1/4", "1/8", "1/16", "1/32" }, "Display quantise (the MIDI itself isn't changed)");
        combo (staffBox, { "Staves: automatic", "Treble", "Bass", "Grand staff" }, "Clef / staves for every part");
        combo (keyBox, {}, "Key signature");
        combo (spellBox, { "Spell from the key", "Prefer sharps", "Prefer flats" }, "How black keys are spelled");
        keyBox.addItem ("Key: from the session", 1);
        for (int f = -7; f <= 7; ++f) keyBox.addItem (ScoreModel::keyName ({ f, false }), 100 + f + 7);
        for (int f = -7; f <= 7; ++f) keyBox.addItem (ScoreModel::keyName ({ f, true }), 200 + f + 7);
        tripletBtn.setClickingTogglesState (true); tripletBtn.setTooltip ("Show eighth-note triplets where they fit");
        pageBtn.setClickingTogglesState (true); pageBtn.setTooltip ("Page view (as printed) or one long scroll");
        entryBtn.setClickingTogglesState (true); entryBtn.setTooltip ("Note entry (N): click a staff to add a note of the palette's length");
        for (auto* b : { &tripletBtn, &pageBtn, &entryBtn, &pdfBtn, &pngBtn, &xmlBtn, &importBtn, &dotBtn, &tripBtn }) addAndMakeVisible (b);
        static const char* pal[] = { "1", "1/2", "1/4", "1/8", "1/16", "1/32" };
        for (int i = 0; i < 6; ++i)
        {
            auto* b = palette.add (new juce::TextButton (pal[i]));
            b->setRadioGroupId (4711); b->setClickingTogglesState (true);
            b->onClick = [this, i] { view.setPalette (4.0 / std::pow (2.0, i), dotBtn.getToggleState(), tripBtn.getToggleState()); };
            addAndMakeVisible (b);
        }
        palette[2]->setToggleState (true, juce::dontSendNotification);
        dotBtn.setClickingTogglesState (true); tripBtn.setClickingTogglesState (true);
        dotBtn.onClick = tripBtn.onClick = [this] { view.setPalette (view.paletteBase, dotBtn.getToggleState(), tripBtn.getToggleState()); };

        gridBox.onChange = [this] { static const int g[] = { 4, 8, 16, 32 }; ScoreModel::setOption (session, "grid", g[gridBox.getSelectedItemIndex()]); };
        staffBox.onChange = [this] { ScoreModel::setOption (session, "staves", staffBox.getSelectedItemIndex()); };
        spellBox.onChange = [this] { ScoreModel::setOption (session, "spelling", spellBox.getSelectedItemIndex() == 1 ? 1 : spellBox.getSelectedItemIndex() == 2 ? -1 : 0); };
        tripletBtn.onClick = [this] { ScoreModel::setOption (session, "triplets", tripletBtn.getToggleState()); };
        keyBox.onChange = [this]
        {
            auto v = ScoreModel::optionsTree (session); const int id = keyBox.getSelectedId();
            if (id == 1) { v.removeProperty ("fifths", nullptr); v.removeProperty ("minor", nullptr); }
            else { v.setProperty ("fifths", (id % 100) - 7, nullptr); v.setProperty ("minor", id >= 200, nullptr); }
        };
        pageBtn.onClick = [this] { view.pageView = pageBtn.getToggleState(); view.relayout(); };
        entryBtn.onClick = [this] { view.noteEntry = entryBtn.getToggleState(); view.grabKeyboardFocus(); };
        tracksBox.onChange = [this]
        {
            view.tracks.clear();
            const int i = tracksBox.getSelectedItemIndex();
            if (i > 0 && i - 1 < trackList.size()) view.tracks.add (trackList[i - 1]);
            view.rebuild();
        };
        pdfBtn.onClick = [this] { exportPages (true); };
        pngBtn.onClick = [this] { exportPages (false); };
        xmlBtn.onClick = [this] { exportXml(); };
        importBtn.onClick = [this] { importXml(); };
        view.onChanged = [this] { entryBtn.setToggleState (view.noteEntry, juce::dontSendNotification); status.setText (view.summary(), juce::dontSendNotification); };
        status.setColour (juce::Label::textColourId, Theme::dim()); status.setFont (Theme::font (12.0f));
        addAndMakeVisible (status);
        viewport.setViewedComponent (&view, false);
        viewport.setScrollBarsShown (true, true);
        addAndMakeVisible (viewport);
        refreshControls();
        view.rebuild();
        status.setText (view.summary(), juce::dontSendNotification);
        setSize (1180, 760);
    }

    void refreshControls()
    {
        if (session.edit == nullptr) return;
        auto o = ScoreModel::options (session);
        gridBox.setSelectedItemIndex (o.grid == 4 ? 0 : o.grid == 8 ? 1 : o.grid == 32 ? 3 : 2, juce::dontSendNotification);
        staffBox.setSelectedItemIndex ((int) o.staves, juce::dontSendNotification);
        spellBox.setSelectedItemIndex (o.spellPreference > 0 ? 1 : o.spellPreference < 0 ? 2 : 0, juce::dontSendNotification);
        tripletBtn.setToggleState (o.triplets, juce::dontSendNotification);
        auto v = ScoreModel::optionsTree (session);
        keyBox.setSelectedId (v.hasProperty ("fifths") ? ((bool) v["minor"] ? 200 : 100) + (int) v["fifths"] + 7 : 1, juce::dontSendNotification);
        trackList = ScoreModel::midiTracks (session);
        tracksBox.clear (juce::dontSendNotification);
        tracksBox.addItem ("All MIDI tracks", 1);
        for (int i = 0; i < trackList.size(); ++i) tracksBox.addItem (trackList[i]->getName(), i + 2);
        tracksBox.setSelectedId (1, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto row1 = r.removeFromTop (26); r.removeFromTop (6);
        auto row2 = r.removeFromTop (26); r.removeFromTop (6);
        tracksBox.setBounds (row1.removeFromLeft (180)); row1.removeFromLeft (6);
        gridBox.setBounds (row1.removeFromLeft (160)); row1.removeFromLeft (6);
        tripletBtn.setBounds (row1.removeFromLeft (80)); row1.removeFromLeft (6);
        staffBox.setBounds (row1.removeFromLeft (150)); row1.removeFromLeft (6);
        keyBox.setBounds (row1.removeFromLeft (150)); row1.removeFromLeft (6);
        spellBox.setBounds (row1.removeFromLeft (150)); row1.removeFromLeft (6);
        pageBtn.setBounds (row1.removeFromLeft (70));
        entryBtn.setBounds (row2.removeFromLeft (100)); row2.removeFromLeft (10);
        for (auto* b : palette) { b->setBounds (row2.removeFromLeft (44)); row2.removeFromLeft (2); }
        dotBtn.setBounds (row2.removeFromLeft (40)); row2.removeFromLeft (2);
        tripBtn.setBounds (row2.removeFromLeft (40)); row2.removeFromLeft (14);
        importBtn.setBounds (row2.removeFromRight (130)); row2.removeFromRight (4);
        xmlBtn.setBounds (row2.removeFromRight (130)); row2.removeFromRight (4);
        pngBtn.setBounds (row2.removeFromRight (70)); row2.removeFromRight (4);
        pdfBtn.setBounds (row2.removeFromRight (70)); row2.removeFromRight (8);
        status.setBounds (row2);
        viewport.setBounds (r);
        view.relayout();
    }

    void paint (juce::Graphics& g) override   { g.fillAll (Theme::panel()); }

    ScoreView& getView()   { return view; }

private:
    Session& session;
    ScoreView view;
    juce::Viewport viewport;
    juce::ComboBox tracksBox, gridBox, staffBox, keyBox, spellBox;
    juce::TextButton tripletBtn { "Triplets" }, pageBtn { "Pages" }, entryBtn { "Note Entry" }, pdfBtn { "PDF..." }, pngBtn { "PNG..." },
                     xmlBtn { "MusicXML..." }, importBtn { "Import MusicXML..." }, dotBtn { "." }, tripBtn { "3" };
    juce::OwnedArray<juce::TextButton> palette;
    juce::Label status;
    juce::Array<te::AudioTrack*> trackList;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::File defaultFile (const juce::String& ext) const
    {
        return session.bouncesDir.getChildFile (juce::File::createLegalFileName (session.getFile().getFileNameWithoutExtension() + " score") + ext);
    }

    void exportPages (bool pdf)
    {
        chooser = std::make_unique<juce::FileChooser> (pdf ? "Export Score as PDF" : "Export Score as PNG", defaultFile (pdf ? ".pdf" : ".png"), pdf ? "*.pdf" : "*.png");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, pdf] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) return;
            juce::String err;
            if (! ScoreExport::writePages (session, view.tracks, f, pdf, false, err)) Ui::message (juce::MessageBoxIconType::WarningIcon, "Export Score", err);
            else f.revealToUser();
        });
    }
    void exportXml()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export MusicXML", defaultFile (".musicxml"), "*.musicxml;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) return;
            auto parts = ScoreModel::build (session, view.tracks.isEmpty() ? ScoreModel::midiTracks (session) : view.tracks);
            f.replaceWithText (ScoreModel::musicXml (session, parts, session.getFile().getFileNameWithoutExtension(), {}));
            f.revealToUser();
        });
    }
    void importXml()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import MusicXML", juce::File(), "*.musicxml;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (! f.existsAsFile()) return;
            juce::String err;
            session.edit->getUndoManager().beginNewTransaction ("Import MusicXML");
            auto made = ScoreModel::importMusicXml (session, f, err);
            if (made.isEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Import MusicXML", err);
            refreshControls(); view.rebuild();
        });
    }
};
