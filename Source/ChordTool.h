#pragma once
// Chord Progression Tool - a window that builds chord progressions by key and genre, lets you edit the
// chords (reorder, nudge the degree, force a quality), hear them, and write them into a MIDI clip.
//
//  Left:   genre filter + the progression library (click one to load it); "Type your own" takes "I - V - vi - IV".
//  Centre: the chord strip - one tile per chord (name, roman numeral). Click a tile, then nudge / move / change its quality.
//  Right:  options - bars per chord, sevenths, bass / 808 line, voice leading, strum, humanise, octave, velocity.
//  Bottom: Preview (plays the chords on the clip's track), Insert (writes them), Replace clip notes.
//
// Everything the window does is also reachable through the MCP 'chords' tool (McpToolsRack.h).

#include "Theme.h"
#include "MidiTools.h"

class ChordToolWindow : public juce::DocumentWindow,
                        private juce::Timer
{
public:
    struct State
    {
        int keyRoot = 0; bool minorKey = false;
        std::vector<MidiTools::ProgChord> chords { { 1, -1 }, { 5, -1 }, { 6, -1 }, { 4, -1 } };
        MidiTools::ChordWriteOptions opts;
        bool replaceNotes = false;
        int selected = 0;
        juce::String libraryName = "Pop Anthem (I - V - vi - IV)";
    };

    ChordToolWindow (te::MidiClip& c, int keyRoot, int scaleIndex)
        : DocumentWindow ("Chord Progression Tool - " + c.getName(), Theme::bg(), DocumentWindow::closeButton),
          clipRef (&c), content (*this, c)
    {
        content.st.keyRoot = juce::jlimit (0, 11, keyRoot);
        content.st.minorKey = (scaleIndex == 2 || scaleIndex == 4 || scaleIndex == 6);   // minor / dorian / phrygian keys read as minor
        content.refreshAll();
        setUsingNativeTitleBar (true);
        setContentNonOwned (&content, false);
        setResizable (true, true);
        centreWithSize (1120, 640);
        setVisible (true);
        toFront (true);
        startTimerHz (30);
    }

    ~ChordToolWindow() override   { content.stopPreview(); }

    void closeButtonPressed() override   { content.stopPreview(); if (onClose) onClose(); }
    std::function<void()> onClose;
    std::function<void (juce::StringArray names, bool minorKey)> onWritten;   // the piano roll re-reads the key and scrolls
    te::Clip::Ptr clipRef;

    State& state()   { return content.st; }

private:
    //==========================================================================
    struct ChordStrip : public juce::Component
    {
        State* st = nullptr;
        std::function<void (int)> onSelect;
        std::function<void()> onChanged;
        int previewIndex = -1;

        static juce::String roman (const MidiTools::ProgChord& pc, int keyRoot, bool minorKey, bool sevenths)
        {
            static const char* up[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
            auto [r, t] = MidiTools::resolveChord (pc, keyRoot, minorKey, sevenths);
            juce::String s = up[juce::jlimit (0, 6, pc.degree - 1)];
            auto& ct = MidiTools::chordTypes()[(size_t) t];
            const bool minorish = ct.intervals.size() > 1 && ct.intervals[1] == 3;
            if (minorish) s = s.toLowerCase();
            juce::String sym (ct.symbol);
            if (sym == "m" || sym.isEmpty()) sym = {};
            else if (sym.startsWith ("m") && sym != "maj7" && sym != "maj9") sym = sym.substring (1);
            return s + sym;
        }

        juce::Rectangle<int> tileBounds (int i) const
        {
            if (st == nullptr || st->chords.empty()) return {};
            const int n = (int) st->chords.size();
            const int gap = 8, w = juce::jmin (150, (getWidth() - gap * (n - 1)) / juce::jmax (1, n));
            return { i * (w + gap), 0, w, getHeight() };
        }

        void paint (juce::Graphics& g) override
        {
            if (st == nullptr) return;
            auto names = MidiTools::chordNamesFor (st->chords, st->keyRoot, st->minorKey, st->opts.sevenths);
            for (int i = 0; i < (int) st->chords.size(); ++i)
            {
                auto r = tileBounds (i).toFloat().reduced (1.0f);
                const bool sel = i == st->selected, playing = i == previewIndex;
                auto base = Theme::panel2();
                if (playing) base = Theme::acid().withAlpha (0.35f);
                g.setColour (base);
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (sel ? Theme::acid() : Theme::line());
                g.drawRoundedRectangle (r, 8.0f, sel ? 2.0f : 1.0f);
                g.setColour (Theme::dim());
                g.setFont (Theme::font (12.0f, true));
                g.drawText (roman (st->chords[(size_t) i], st->keyRoot, st->minorKey, st->opts.sevenths), r.reduced (6, 6), juce::Justification::topLeft);
                g.drawText (juce::String (i + 1), r.reduced (6, 6), juce::Justification::topRight);
                g.setColour (Theme::text());
                g.setFont (Theme::font (r.getWidth() > 90 ? 26.0f : 18.0f, true));
                g.drawText (names[i], r, juce::Justification::centred);
                // the notes it will play
                auto ns = MidiTools::progressionNotes (st->chords, st->keyRoot, st->minorKey, st->opts.sevenths, st->opts.octave, st->opts.voiceLead);
                juce::StringArray nn; for (auto n : ns[(size_t) i]) nn.add (MidiTools::noteName (n, MidiTools::keyUsesFlats (st->keyRoot, st->minorKey)));
                g.setColour (Theme::dim());
                g.setFont (Theme::font (10.0f));
                g.drawFittedText (nn.joinIntoString (" "), r.reduced (4, 6).toNearestInt(), juce::Justification::centredBottom, 2);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (st == nullptr) return;
            for (int i = 0; i < (int) st->chords.size(); ++i)
                if (tileBounds (i).contains (e.getPosition())) { st->selected = i; dragFrom = i; if (onSelect) onSelect (i); repaint(); return; }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (st == nullptr || dragFrom < 0) return;
            for (int i = 0; i < (int) st->chords.size(); ++i)
                if (tileBounds (i).contains (e.getPosition()) && i != dragFrom)
                {
                    std::swap (st->chords[(size_t) i], st->chords[(size_t) dragFrom]);
                    st->selected = dragFrom = i;
                    if (onChanged) onChanged();
                    repaint();
                    return;
                }
        }
        void mouseUp (const juce::MouseEvent&) override   { dragFrom = -1; }
        int dragFrom = -1;
    };

    //==========================================================================
    struct Library : public juce::ListBoxModel
    {
        juce::Array<int> rows;   // indices into progressions()
        std::function<void (int)> onPick;
        int getNumRows() override   { return rows.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
        {
            if (row < 0 || row >= rows.size()) return;
            auto& p = MidiTools::progressions()[(size_t) rows[row]];
            g.fillAll (sel ? Theme::acid().withAlpha (0.25f) : (row % 2 ? Theme::panel() : Theme::bg()));
            g.setColour (Theme::text()); g.setFont (Theme::font (12.0f, sel));
            g.drawText (p.name, 8, 0, w - 90, h, juce::Justification::centredLeft);
            g.setColour (Theme::dim()); g.setFont (Theme::font (10.0f));
            g.drawText (juce::String (p.minorKey ? "minor" : "major") + (p.sevenths ? " 7ths" : ""), w - 86, 0, 80, h, juce::Justification::centredRight);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override   { if (row >= 0 && row < rows.size() && onPick) onPick (rows[row]); }
        void selectedRowsChanged (int row) override                            { if (row >= 0 && row < rows.size() && onPick) onPick (rows[row]); }
    };

    //==========================================================================
    struct Content : public juce::Component
    {
        Content (ChordToolWindow& w, te::MidiClip& c) : owner (w), clip (c)
        {
            // key
            for (int i = 0; i < 12; ++i)
                rootBox.addItem (MidiTools::pitchClassName (i, false) + (juce::MidiMessage::isMidiNoteBlack (i) ? " / " + MidiTools::pitchClassName (i, true) : juce::String()), i + 1);
            rootBox.onChange = [this] { st.keyRoot = rootBox.getSelectedId() - 1; refreshStrip(); };
            majorBtn.setClickingTogglesState (true); minorBtn.setClickingTogglesState (true);
            majorBtn.setRadioGroupId (77); minorBtn.setRadioGroupId (77);
            majorBtn.onClick = [this] { if (majorBtn.getToggleState()) { st.minorKey = false; refreshStrip(); } };
            minorBtn.onClick = [this] { if (minorBtn.getToggleState()) { st.minorKey = true; refreshStrip(); } };

            // library
            genreBox.addItem ("All genres", 1);
            auto genres = MidiTools::progressionGenres();
            for (int i = 0; i < genres.size(); ++i) genreBox.addItem (genres[i], i + 2);
            genreBox.setSelectedId (1, juce::dontSendNotification);
            genreBox.onChange = [this] { fillLibrary(); };
            library.onPick = [this] (int idx)
            {
                auto& p = MidiTools::progressions()[(size_t) idx];
                st.chords = p.chords; st.minorKey = p.minorKey; st.opts.sevenths = p.sevenths; st.libraryName = p.name;
                st.selected = 0;
                refreshAll();
            };
            libraryBox.setModel (&library);
            libraryBox.setRowHeight (24);
            libraryBox.setColour (juce::ListBox::backgroundColourId, Theme::bg());
            libraryBox.setColour (juce::ListBox::outlineColourId, Theme::line());
            libraryBox.setOutlineThickness (1);
            customEd.setTextToShowWhenEmpty ("Type your own: I - V - vi - IV  (or 1 5 6 4, i VI III VII, ii7 V7 Imaj7)", Theme::dim());
            customEd.setFont (Theme::font (12.0f));
            customEd.onReturnKey = [this] { applyCustom(); };
            applyBtn.onClick = [this] { applyCustom(); };

            // strip editing
            strip.st = &st;
            strip.onSelect = [this] (int) { refreshQuality(); };
            strip.onChanged = [this] { refreshStrip(); };
            degDown.onClick = [this] { nudge (-1); };
            degUp.onClick = [this] { nudge (1); };
            moveL.onClick = [this] { move (-1); };
            moveR.onClick = [this] { move (1); };
            addBtn.onClick = [this] { if (st.chords.size() < 16) { st.chords.insert (st.chords.begin() + juce::jmin ((int) st.chords.size(), st.selected + 1), st.chords[(size_t) juce::jlimit (0, (int) st.chords.size() - 1, st.selected)]); st.selected = juce::jmin ((int) st.chords.size() - 1, st.selected + 1); refreshStrip(); } };
            removeBtn.onClick = [this] { if (st.chords.size() > 1) { st.chords.erase (st.chords.begin() + juce::jlimit (0, (int) st.chords.size() - 1, st.selected)); st.selected = juce::jlimit (0, (int) st.chords.size() - 1, st.selected); refreshStrip(); } };
            qualityBox.addItem ("Quality: from the key", 1);
            for (int i = 0; i < (int) MidiTools::chordTypes().size(); ++i) qualityBox.addItem (MidiTools::chordTypes()[(size_t) i].name, i + 2);
            qualityBox.onChange = [this] { if (st.selected >= 0 && st.selected < (int) st.chords.size()) { st.chords[(size_t) st.selected].forceType = qualityBox.getSelectedId() - 2; refreshStrip(); } };

            // options
            barsBox.addItem ("1/2 bar per chord", 1); barsBox.addItem ("1 bar per chord", 2); barsBox.addItem ("2 bars per chord", 3); barsBox.addItem ("4 bars per chord", 4);
            barsBox.setSelectedId (2, juce::dontSendNotification);
            barsBox.onChange = [this] { refreshStrip(); };
            seventhsT.onClick = [this] { st.opts.sevenths = seventhsT.getToggleState(); refreshStrip(); };
            bassT.setToggleState (true, juce::dontSendNotification);
            bassT.onClick = [this] { st.opts.withBass = bassT.getToggleState(); };
            voiceT.setToggleState (true, juce::dontSendNotification);
            voiceT.onClick = [this] { st.opts.voiceLead = voiceT.getToggleState(); refreshStrip(); };
            humanT.onClick = [this] { st.opts.humanise = humanT.getToggleState(); };
            replaceT.onClick = [this] { st.replaceNotes = replaceT.getToggleState(); };
            strumBox.addItem ("Strum: off", 1); strumBox.addItem ("Strum up, fast (1/64)", 2); strumBox.addItem ("Strum up (1/32)", 3);
            strumBox.addItem ("Strum up, slow (1/16)", 4); strumBox.addItem ("Strum down, fast (1/64)", 5); strumBox.addItem ("Strum down (1/32)", 6);
            strumBox.setSelectedId (1, juce::dontSendNotification);
            strumBox.onChange = [this]
            {
                const double b[] = { 0.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0, 1.0 / 8.0 };
                const int id = strumBox.getSelectedId() - 1;
                st.opts.strumBeats = b[juce::jlimit (0, 5, id)]; st.opts.strumUp = id < 4;
            };
            for (int o = 2; o <= 6; ++o) octaveBox.addItem ("Octave " + juce::String (o), o);
            octaveBox.setSelectedId (4, juce::dontSendNotification);
            octaveBox.onChange = [this] { st.opts.octave = octaveBox.getSelectedId(); refreshStrip(); };
            velSlider.setRange (1, 127, 1); velSlider.setValue (92, juce::dontSendNotification);
            velSlider.setSliderStyle (juce::Slider::LinearHorizontal);
            velSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
            velSlider.onValueChange = [this] { st.opts.velocity = (int) velSlider.getValue(); };

            previewBtn.onClick = [this] { if (previewing) stopPreview(); else startPreview(); };
            insertBtn.onClick = [this] { insert(); };
            insertBtn.setColour (juce::TextButton::buttonColourId, Theme::acid());
            insertBtn.setColour (juce::TextButton::textColourOffId, Theme::bg());
            closeBtn.onClick = [this] { owner.closeButtonPressed(); };

            for (auto* comp : std::initializer_list<juce::Component*> { &rootBox, &majorBtn, &minorBtn, &genreBox, &libraryBox, &customEd, &applyBtn, &strip,
                                                                       &degDown, &degUp, &moveL, &moveR, &addBtn, &removeBtn, &qualityBox,
                                                                       &barsBox, &seventhsT, &bassT, &voiceT, &humanT, &replaceT, &strumBox, &octaveBox, &velSlider,
                                                                       &previewBtn, &insertBtn, &closeBtn, &status, &keyLabel, &velLabel, &libLabel, &optLabel, &editLabel })
                addAndMakeVisible (comp);
            for (auto* l : { &keyLabel, &velLabel, &libLabel, &optLabel, &editLabel }) { l->setColour (juce::Label::textColourId, Theme::dim()); l->setFont (Theme::font (11.0f, true)); }
            keyLabel.setText ("KEY", juce::dontSendNotification);
            velLabel.setText ("Velocity", juce::dontSendNotification);
            libLabel.setText ("LIBRARY - click a progression", juce::dontSendNotification);
            optLabel.setText ("OPTIONS", juce::dontSendNotification);
            editLabel.setText ("EDIT THE SELECTED CHORD  (drag tiles to reorder)", juce::dontSendNotification);
            status.setColour (juce::Label::textColourId, Theme::text());
            status.setFont (Theme::font (12.0f));
            setSize (1120, 640);
        }

        ~Content() override   { stopPreview(); }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::bg());
            g.setColour (Theme::panel());
            g.fillRect (leftCol);
            g.fillRect (rightCol);
            g.setColour (Theme::line());
            g.drawVerticalLine (leftCol.getRight(), 0.0f, (float) getHeight());
            g.drawVerticalLine (rightCol.getX() - 1, 0.0f, (float) getHeight());
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto bottom = r.removeFromBottom (44).reduced (8, 6);
            leftCol = r.removeFromLeft (300);
            rightCol = r.removeFromRight (250);
            auto l = leftCol.reduced (8);
            libLabel.setBounds (l.removeFromTop (18));
            genreBox.setBounds (l.removeFromTop (24)); l.removeFromTop (6);
            auto custom = l.removeFromBottom (26);
            applyBtn.setBounds (custom.removeFromRight (60)); custom.removeFromRight (4);
            customEd.setBounds (custom); l.removeFromBottom (6);
            libraryBox.setBounds (l);

            auto o = rightCol.reduced (8);
            optLabel.setBounds (o.removeFromTop (18));
            barsBox.setBounds (o.removeFromTop (24)); o.removeFromTop (6);
            seventhsT.setBounds (o.removeFromTop (22)); bassT.setBounds (o.removeFromTop (22)); voiceT.setBounds (o.removeFromTop (22)); humanT.setBounds (o.removeFromTop (22));
            o.removeFromTop (6);
            strumBox.setBounds (o.removeFromTop (24)); o.removeFromTop (6);
            octaveBox.setBounds (o.removeFromTop (24)); o.removeFromTop (6);
            velLabel.setBounds (o.removeFromTop (16));
            velSlider.setBounds (o.removeFromTop (24)); o.removeFromTop (10);
            replaceT.setBounds (o.removeFromTop (22));

            auto c = r.reduced (12, 8);
            keyLabel.setBounds (c.removeFromTop (18));
            auto keyRow = c.removeFromTop (26);
            rootBox.setBounds (keyRow.removeFromLeft (110)); keyRow.removeFromLeft (6);
            majorBtn.setBounds (keyRow.removeFromLeft (70)); keyRow.removeFromLeft (2);
            minorBtn.setBounds (keyRow.removeFromLeft (70));
            c.removeFromTop (12);
            strip.setBounds (c.removeFromTop (juce::jmax (120, c.getHeight() - 150)));
            c.removeFromTop (10);
            editLabel.setBounds (c.removeFromTop (18));
            auto e = c.removeFromTop (26);
            degDown.setBounds (e.removeFromLeft (90)); e.removeFromLeft (2); degUp.setBounds (e.removeFromLeft (90)); e.removeFromLeft (10);
            qualityBox.setBounds (e.removeFromLeft (juce::jmin (200, e.getWidth())));
            c.removeFromTop (6);
            auto e2 = c.removeFromTop (26);
            moveL.setBounds (e2.removeFromLeft (90)); e2.removeFromLeft (2); moveR.setBounds (e2.removeFromLeft (90)); e2.removeFromLeft (10);
            addBtn.setBounds (e2.removeFromLeft (90)); e2.removeFromLeft (2); removeBtn.setBounds (e2.removeFromLeft (90));

            closeBtn.setBounds (bottom.removeFromRight (80)); bottom.removeFromRight (6);
            insertBtn.setBounds (bottom.removeFromRight (150)); bottom.removeFromRight (6);
            previewBtn.setBounds (bottom.removeFromRight (110)); bottom.removeFromRight (10);
            status.setBounds (bottom);
        }

        //======================================================================
        void fillLibrary()
        {
            library.rows.clear();
            const int g = genreBox.getSelectedId();
            auto genres = MidiTools::progressionGenres();
            for (int i = 0; i < (int) MidiTools::progressions().size(); ++i)
                if (g <= 1 || juce::String (MidiTools::progressions()[(size_t) i].mood) == genres[g - 2]) library.rows.add (i);
            libraryBox.updateContent();
            libraryBox.repaint();
        }

        void refreshAll()
        {
            rootBox.setSelectedId (st.keyRoot + 1, juce::dontSendNotification);
            majorBtn.setToggleState (! st.minorKey, juce::dontSendNotification);
            minorBtn.setToggleState (st.minorKey, juce::dontSendNotification);
            seventhsT.setToggleState (st.opts.sevenths, juce::dontSendNotification);
            if (library.rows.isEmpty()) fillLibrary();
            refreshStrip();
        }

        void refreshStrip()
        {
            st.selected = juce::jlimit (0, juce::jmax (0, (int) st.chords.size() - 1), st.selected);
            refreshQuality();
            auto names = MidiTools::chordNamesFor (st.chords, st.keyRoot, st.minorKey, st.opts.sevenths);
            status.setText (MidiTools::pitchClassName (st.keyRoot, MidiTools::keyUsesFlats (st.keyRoot, st.minorKey)) + (st.minorKey ? " minor:  " : " major:  ")
                            + names.joinIntoString ("  -  ") + "    (" + juce::String (barsPerChord() * (int) st.chords.size()) + " bars)", juce::dontSendNotification);
            strip.repaint();
        }

        void refreshQuality()
        {
            if (st.selected >= 0 && st.selected < (int) st.chords.size())
                qualityBox.setSelectedId (st.chords[(size_t) st.selected].forceType + 2, juce::dontSendNotification);
        }

        void nudge (int d)
        {
            if (st.selected < 0 || st.selected >= (int) st.chords.size()) return;
            auto& pc = st.chords[(size_t) st.selected];
            pc.degree = ((pc.degree - 1 + d) % 7 + 7) % 7 + 1;
            refreshStrip();
        }

        void move (int d)
        {
            const int to = st.selected + d;
            if (st.selected < 0 || st.selected >= (int) st.chords.size() || to < 0 || to >= (int) st.chords.size()) return;
            std::swap (st.chords[(size_t) st.selected], st.chords[(size_t) to]);
            st.selected = to;
            refreshStrip();
        }

        void applyCustom()
        {
            auto parsed = MidiTools::parseDegrees (customEd.getText());
            if (parsed.empty()) { status.setText ("Couldn't read that - try  I - V - vi - IV  or  1 5 6 4", juce::dontSendNotification); return; }
            st.chords = parsed; st.selected = 0; st.libraryName = "Custom: " + customEd.getText().trim();
            refreshStrip();
        }

        int barsPerChord() const   { const int id = barsBox.getSelectedId(); return id == 1 ? 1 : id == 3 ? 2 : id == 4 ? 4 : 1; }
        double beatsPerChord() const
        {
            const int bpb = juce::jmax (1, clip.edit.tempoSequence.getTimeSig (0)->numerator.get());
            const int id = barsBox.getSelectedId();
            return id == 1 ? bpb * 0.5 : id == 3 ? bpb * 2.0 : id == 4 ? bpb * 4.0 : (double) bpb;
        }

        void insert()
        {
            stopPreview();
            auto& um = clip.edit.getUndoManager();
            um.beginNewTransaction ("Chord progression");
            if (st.replaceNotes) { auto all = clip.getSequence().getNotes(); for (int i = all.size(); --i >= 0;) clip.getSequence().removeNote (*all[i], &um); }
            auto o = st.opts;
            o.keyRoot = st.keyRoot; o.minorKey = st.minorKey; o.beatsPerChord = beatsPerChord();
            o.totalBeats = juce::jmax (o.beatsPerChord * (double) st.chords.size(), clip.getLengthInBeats().inBeats() + clip.getOffsetInBeats().inBeats());
            // fill whole repeats only
            o.totalBeats = std::floor (o.totalBeats / (o.beatsPerChord * (double) st.chords.size()) + 1e-6) * o.beatsPerChord * (double) st.chords.size();
            if (o.totalBeats < o.beatsPerChord) o.totalBeats = o.beatsPerChord * (double) st.chords.size();
            o.seed = juce::Random::getSystemRandom().nextInt();
            auto names = MidiTools::writeChords (clip, st.chords, o);
            status.setText ("Wrote " + names.joinIntoString (" - ") + (o.withBass ? " with a bass line" : ""), juce::dontSendNotification);
            if (owner.onWritten) owner.onWritten (names, st.minorKey);
        }

        //======================================================================
        // Preview: plays the chords live on the clip's track, in time with the session tempo
        void startPreview()
        {
            auto* at = dynamic_cast<te::AudioTrack*> (clip.getTrack());
            if (at == nullptr) return;
            previewNotes = MidiTools::progressionNotes (st.chords, st.keyRoot, st.minorKey, st.opts.sevenths, st.opts.octave, st.opts.voiceLead);
            previewing = true; previewStep = -1;
            previewStart = juce::Time::getMillisecondCounterHiRes();
            previewBtn.setButtonText ("Stop");
        }

        void stopPreview()
        {
            if (! previewing) return;
            previewing = false;
            if (auto* at = dynamic_cast<te::AudioTrack*> (clip.getTrack()))
                for (auto n : sounding) at->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, n), sourceID);
            sounding.clear();
            strip.previewIndex = -1; strip.repaint();
            previewBtn.setButtonText ("Preview");
        }

        void tick()
        {
            if (! previewing) return;
            auto* at = dynamic_cast<te::AudioTrack*> (clip.getTrack());
            if (at == nullptr || previewNotes.empty()) { stopPreview(); return; }
            const double bpm = juce::jmax (20.0, clip.edit.tempoSequence.getTempo (0)->getBpm());
            const double beatsElapsed = (juce::Time::getMillisecondCounterHiRes() - previewStart) / 1000.0 * bpm / 60.0;
            const double per = beatsPerChord();
            const int step = (int) std::floor (beatsElapsed / per);
            const int idx = step % (int) previewNotes.size();
            if (step != previewStep)
            {
                for (auto n : sounding) at->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, n), sourceID);
                sounding.clear();
                auto& ns = previewNotes[(size_t) idx];
                int k = 0;
                for (auto n : ns)
                {
                    if (n < 0 || n > 127) continue;
                    at->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, n, (juce::uint8) juce::jlimit (1, 127, st.opts.velocity)), sourceID);
                    sounding.add (n); ++k;
                }
                if (st.opts.withBass)
                {
                    auto [root, type] = MidiTools::resolveChord (st.chords[(size_t) idx], st.keyRoot, st.minorKey, st.opts.sevenths);
                    const int b = 24 + root + (root > 6 ? 0 : 12);
                    at->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, b, (juce::uint8) juce::jmin (127, st.opts.velocity + 12)), sourceID);
                    sounding.add (b);
                }
                previewStep = step;
                strip.previewIndex = idx; strip.repaint();
            }
            else if (beatsElapsed - step * per > per * 0.96 && ! sounding.isEmpty())
            {
                for (auto n : sounding) at->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, n), sourceID);
                sounding.clear();
            }
        }

        ChordToolWindow& owner;
        te::MidiClip& clip;
        State st;
        juce::Rectangle<int> leftCol, rightCol;

        juce::ComboBox rootBox, genreBox, qualityBox, barsBox, strumBox, octaveBox;
        juce::TextButton majorBtn { "Major" }, minorBtn { "Minor" }, applyBtn { "Use" }, degDown { "Degree -" }, degUp { "Degree +" },
                         moveL { "< Move" }, moveR { "Move >" }, addBtn { "+ Add" }, removeBtn { "- Remove" }, previewBtn { "Preview" }, insertBtn { "Insert into clip" }, closeBtn { "Close" };
        juce::ToggleButton seventhsT { "Sevenths (7th chords)" }, bassT { "Bass / 808 line" }, voiceT { "Voice leading" }, humanT { "Humanise" }, replaceT { "Replace the clip's notes" };
        juce::ListBox libraryBox;
        Library library;
        juce::TextEditor customEd;
        ChordStrip strip;
        juce::Slider velSlider;
        juce::Label status, keyLabel, velLabel, libLabel, optLabel, editLabel;

        bool previewing = false;
        int previewStep = -1;
        double previewStart = 0;
        std::vector<juce::Array<int>> previewNotes;
        juce::Array<int> sounding;
        te::MPESourceID sourceID = te::createUniqueMPESourceID();
    };

    Content content;

    void timerCallback() override
    {
        if (clipRef == nullptr || clipRef->getTrack() == nullptr) { stopTimer(); content.stopPreview(); if (onClose) onClose(); return; }
        content.tick();
    }
};
