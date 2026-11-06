#pragma once
// Squelchmoth MIDI Editor (piano roll) - Pro Tools' MIDI editor with FL Studio's piano-roll speed.
//
//  Tools (keys P / E / D / C, like FL):  Draw  Select  Erase  Slice
//    Draw:   click empty space = note (or a whole chord when Chord Stamp is on); drag = length;
//            click a note = select it, drag = move, drag its right edge = resize; Alt-drag = copy
//    Select: drag on empty space = rubber-band; Shift adds; then move / resize like Draw
//    Erase:  click or drag over notes        Slice: click a note to cut it at the grid line
//    Right-click / right-drag deletes in any tool. Click the keyboard to hear a note.
//  Key + scale: highlights the rows in the key; Scale snap keeps drawn and moved notes in key.
//  Chord Stamp: draws any of 21 chord types with inversions, bottom-up (clicked key = root) or top-down (clicked key stays on top).
//  Chords menu: quick progressions by mood (with bass / 808). Progression Tool: the full Chord Progression Tool window (ChordTool.h).
//  Key labels: the keyboard shows pad / kit sound names on a drum channel; right-click a key to rename it (stored on the track).
//  Tools menu: quantise (strength, swing), humanise, strum, arpeggiate, flam, legato, 808 slides, chop,
//              snap / transpose in scale, velocity, flip, riff starter. Ghost notes show other MIDI tracks.
//  Velocity lane underneath: drag to draw velocities. Keys: Cmd+A/C/V/D, Delete, arrows (Shift = octave), Q, Cmd+Z.

#include "Behaviours.h"
#include "MidiTools.h"
#include "KeyLabels.h"
#include "ChordTool.h"

class PianoRollGrid : public juce::Component
{
public:
    PianoRollGrid (te::MidiClip& c) : clip (c)
    {
        setWantsKeyboardFocus (true);
        scaleRoot = (int) clip.state.getProperty ("sqmScaleRoot", 0);
        scaleIndex = (int) clip.state.getProperty ("sqmScale", 0);
    }

    static constexpr int keyH = 14, keysW = 84;
    double pxPerBeat = 80.0, gridBeats = 0.25;
    int topNote = 127;
    bool snapOn = true, scaleSnap = false, showGhosts = true;
    int scaleRoot = 0, scaleIndex = 0;          // 0 = chromatic (no highlight)
    int chordType = -1, chordInversion = 0;     // -1 = Chord Stamp off
    bool chordTopDown = false;                  // Chord Stamp mode: false = bottom-up (clicked note is the root), true = top-down (clicked note on top)
    enum class Tool { draw, select, erase, slice } tool = Tool::draw;
    std::function<void()> onSelectionChanged;

    te::MidiClip& clip;
    juce::Array<te::MidiNote*> selection;

    //==========================================================================
    void setScale (int root, int scale)
    {
        scaleRoot = root; scaleIndex = scale;
        clip.state.setProperty ("sqmScaleRoot", root, nullptr);
        clip.state.setProperty ("sqmScale", scale, nullptr);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg());
        auto lenBeats = clipLengthBeats();
        auto num = juce::jmax (1, clip.edit.tempoSequence.getTimeSig (0)->numerator.get());
        const bool scaled = scaleIndex > 0;
        auto clipArea = g.getClipBounds();

        for (int i = juce::jmax (0, clipArea.getY() / keyH); i * keyH < clipArea.getBottom() && i < 128; ++i)
        {
            int note = topNote - i;
            bool black = juce::MidiMessage::isMidiNoteBlack (note);
            bool inKey = ! scaled || MidiTools::inScale (note, scaleRoot, scaleIndex);
            bool root = scaled && ((note - scaleRoot) % 12 + 12) % 12 == 0;
            auto row = black ? Theme::panel().darker (0.3f) : Theme::panel().withAlpha (0.6f);
            if (scaled) row = inKey ? Theme::panel().withAlpha (0.75f) : Theme::bg().darker (0.25f);
            if (root) row = row.interpolatedWith (Theme::violet(), 0.18f);
            g.setColour (row);
            g.fillRect (keysW, i * keyH, getWidth() - keysW, keyH);
            g.setColour (Theme::line().withAlpha (note % 12 == 0 ? 0.9f : 0.4f));
            g.drawHorizontalLine (i * keyH, (float) keysW, (float) getWidth());

            auto label = keyLabel (note);
            g.setColour (black ? juce::Colour (0xff1a1424) : juce::Colour (0xffd8d2e8));
            if (label.isNotEmpty()) g.setColour (black ? Theme::mint().darker (0.6f) : Theme::mint().brighter (0.5f));
            if (previewing == note) g.setColour (Theme::acid());
            g.fillRect (0, i * keyH, keysW - 2, keyH - 1);
            if (scaled && inKey) { g.setColour (root ? Theme::violet() : Theme::mint().withAlpha (0.8f)); g.fillRect (keysW - 6, i * keyH + 2, 3, keyH - 5); }
            if (label.isNotEmpty())
            {
                g.setColour (black ? juce::Colour (0xffe8f4ee) : juce::Colour (0xff10261c));
                g.setFont (Theme::font (10.0f, true));
                g.drawFittedText (label, 3, i * keyH, keysW - 30, keyH - 1, juce::Justification::centredLeft, 1, 0.8f);
                g.setColour ((black ? juce::Colour (0xffe8f4ee) : juce::Colour (0xff10261c)).withAlpha (0.7f));
                g.setFont (Theme::font (9.0f));
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4), keysW - 30, i * keyH, 22, keyH, juce::Justification::right);
            }
            else if (note % 12 == 0)
            {
                g.setColour (Theme::bg());
                g.setFont (Theme::font (10.0f, true));
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4), 2, i * keyH, keysW - 9, keyH, juce::Justification::right);
            }
        }

        for (double b = 0; b <= lenBeats + 0.001; b += gridBeats)
        {
            auto x = beatToX (b);
            if (x < clipArea.getX() - 2 || x > clipArea.getRight() + 2) continue;
            bool bar = std::fmod (b + 1e-9, (double) num) < 1e-6, beat = std::fmod (b + 1e-9, 1.0) < 1e-6;
            g.setColour (bar ? Theme::line().brighter (0.4f) : (beat ? Theme::line() : Theme::line().withAlpha (0.35f)));
            g.drawVerticalLine (juce::roundToInt (x), 0.0f, (float) getHeight());
            if (bar)
            {
                g.setColour (Theme::dim());
                g.setFont (Theme::font (10.0f));
                g.drawText (juce::String ((int) (b / num) + 1), (int) x + 3, 0, 30, 12, juce::Justification::left);
            }
        }

        g.setColour (Theme::red().withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (beatToX (lenBeats)), 0.0f, (float) getHeight());

        if (showGhosts) paintGhosts (g);

        validateSelection();
        for (auto n : clip.getSequence().getNotes())
        {
            auto r = noteRect (*n);
            if (! r.intersects (clipArea.toFloat())) continue;
            const bool sel = selection.contains (n);
            auto c = Theme::acid().interpolatedWith (Theme::magenta(), 1.0f - n->getVelocity() / 127.0f);
            if (n->isMute()) c = Theme::dim();
            g.setColour (sel ? Theme::text() : c);
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (sel ? Theme::acid() : Theme::bg());
            g.drawRoundedRectangle (r, 2.0f, sel ? 1.5f : 1.0f);
            if (r.getWidth() > 30 && keyH >= 12)
            {
                g.setColour (Theme::bg());
                g.setFont (Theme::font (9.0f, true));
                g.drawText (MidiTools::noteName (n->getNoteNumber(), useFlats()), r.reduced (3, 0), juce::Justification::centredLeft);
            }
        }

        if (! band.isEmpty())
        {
            g.setColour (Theme::acid().withAlpha (0.15f)); g.fillRect (band);
            g.setColour (Theme::acid()); g.drawRect (band, 1.0f);
        }

        // playhead
        auto& tr = clip.edit.getTransport();
        auto beat = clip.getContentBeatAtTime (tr.getPosition()).inBeats();
        if (beat >= 0 && beat <= lenBeats)
        {
            g.setColour (Theme::acid());
            g.fillRect ((float) beatToX (beat), 0.0f, 2.0f, (float) getHeight());
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        band = {};
        dragMode = DragMode::none;
        if (e.x < keysW)
        {
            if (e.mods.isPopupMenu()) { showKeyMenu (yToNote (e.y)); return; }
            previewNote (yToNote (e.y));
            return;
        }

        validateSelection();
        auto n = noteAt (e.position);

        if (e.mods.isPopupMenu() || tool == Tool::erase)
        {
            if (n != nullptr) removeNote (n);
            dragMode = DragMode::erase;
            return;
        }
        if (tool == Tool::slice)
        {
            if (n != nullptr) sliceAt (n, snap (xToBeat (e.position.x), false));
            return;
        }

        if (n == nullptr)
        {
            if (tool == Tool::select || e.mods.isShiftDown())
            {
                if (! e.mods.isShiftDown()) setSelection ({});
                bandStart = e.position;
                dragMode = DragMode::band;
                return;
            }
            // draw a note or a chord
            auto beat = snapOn ? snap (xToBeat (e.position.x), true) : xToBeat (e.position.x);
            int pitch = yToNote (e.y);
            if (scaleSnap && scaleIndex > 0) pitch = MidiTools::snapToScale (pitch, scaleRoot, scaleIndex);
            juce::Array<te::MidiNote*> made;
            if (chordType >= 0 && chordTopDown)
                made = MidiTools::stampChordTopDown (clip, pitch, chordType, juce::jmax (0.0, beat), lastLength, lastVelocity);
            else if (chordType >= 0)
                made = MidiTools::stampChord (clip, pitch, chordType, chordInversion, juce::jmax (0.0, beat), lastLength, lastVelocity);
            else
                made.add (clip.getSequence().addNote (pitch, te::BeatPosition::fromBeats (juce::jmax (0.0, beat)), te::BeatDuration::fromBeats (lastLength), lastVelocity, 0, um()));
            setSelection (made);
            if (! made.isEmpty()) previewNote (made.getFirst()->getNoteNumber());
            dragMode = DragMode::resize;
        }
        else
        {
            if (e.mods.isShiftDown()) { auto s = selection; if (s.contains (n)) s.removeFirstMatchingValue (n); else s.add (n); setSelection (s); }
            else if (! selection.contains (n)) setSelection ({ n });
            auto r = noteRect (*n);
            dragMode = (r.getRight() - e.position.x < 6) ? DragMode::resize : DragMode::move;
            lastLength = n->getLengthBeats().inBeats();
            lastVelocity = n->getVelocity();
            previewNote (n->getNoteNumber());
            if (e.mods.isAltDown() && dragMode == DragMode::move)
            {
                // Alt-drag copies: duplicates stay where they were, the drag moves the copies
                juce::Array<te::MidiNote*> copies;
                for (auto* s : selection)
                    copies.add (clip.getSequence().addNote (s->getNoteNumber(), s->getStartBeat(), s->getLengthBeats(), s->getVelocity(), 0, um()));
                setSelection (copies);
            }
        }
        dragAnchor = n;
        dragStart.clear();
        for (auto* s : selection) dragStart.push_back ({ s, s->getStartBeat().inBeats(), s->getLengthBeats().inBeats(), s->getNoteNumber() });
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMode == DragMode::erase)
        {
            if (auto n = noteAt (e.position)) removeNote (n);
            return;
        }
        if (dragMode == DragMode::band)
        {
            band = juce::Rectangle<float> (bandStart, e.position);
            juce::Array<te::MidiNote*> s = e.mods.isShiftDown() ? selection : juce::Array<te::MidiNote*>();
            for (auto* n : clip.getSequence().getNotes()) if (band.intersects (noteRect (*n))) s.addIfNotAlreadyThere (n);
            selection = s;
            repaint();
            return;
        }
        if (dragStart.empty()) return;
        auto dBeat = (e.position.x - e.mouseDownPosition.x) / pxPerBeat;
        if (dragMode == DragMode::move)
        {
            int dNote = -juce::roundToInt ((e.position.y - e.mouseDownPosition.y) / keyH);
            // snap the anchor's start, move everything by the same amount
            double anchorStart = dragStart.front().start;
            for (auto& d : dragStart) if (d.note == dragAnchor) anchorStart = d.start;
            double target = anchorStart + dBeat;
            if (snapOn && ! e.mods.isCommandDown()) target = snap (target, false);
            const double delta = target - anchorStart;
            for (auto& d : dragStart)
            {
                d.note->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (0.0, d.start + delta)), d.note->getLengthBeats(), um());
                int nn = juce::jlimit (0, 127, d.pitch + dNote);
                if (scaleSnap && scaleIndex > 0) nn = MidiTools::snapToScale (nn, scaleRoot, scaleIndex);
                if (nn != d.note->getNoteNumber()) d.note->setNoteNumber (nn, um());
            }
            if (dragAnchor != nullptr && previewing != dragAnchor->getNoteNumber()) previewNote (dragAnchor->getNoteNumber());
        }
        else if (dragMode == DragMode::resize)
        {
            for (auto& d : dragStart)
            {
                auto len = d.length + dBeat;
                if (snapOn && ! e.mods.isCommandDown()) len = snap (d.start + len, false) - d.start;
                len = juce::jmax (snapOn ? gridBeats * 0.5 : 1.0 / 64.0, len);
                d.note->setStartAndLength (d.note->getStartBeat(), te::BeatDuration::fromBeats (len), um());
                lastLength = len;
            }
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        stopPreview();
        if (dragMode == DragMode::band && onSelectionChanged) onSelectionChanged();
        band = {};
        dragMode = DragMode::none;
        dragStart.clear();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (tool == Tool::draw)
            if (auto n = noteAt (e.position)) removeNote (n);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown())
        {
            pxPerBeat = juce::jlimit (12.0, 600.0, pxPerBeat * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15));
            if (onZoom) onZoom();
            return;
        }
        juce::Component::mouseWheelMove (e, w);
    }
    std::function<void()> onZoom;

    bool keyPressed (const juce::KeyPress& k) override
    {
        validateSelection();
        auto& undo = clip.edit.getUndoManager();
        const auto cmd = juce::ModifierKeys::commandModifier;
        if (k == juce::KeyPress ('z', cmd, 0))                                           { undo.undo(); setSelection ({}); return true; }
        if (k == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0))      { undo.redo(); setSelection ({}); return true; }
        if (k == juce::KeyPress ('a', cmd, 0))  { juce::Array<te::MidiNote*> all; for (auto* n : clip.getSequence().getNotes()) all.add (n); setSelection (all); return true; }
        if (k == juce::KeyPress ('c', cmd, 0))  { copySelection(); return true; }
        if (k == juce::KeyPress ('v', cmd, 0))  { paste(); return true; }
        if (k == juce::KeyPress ('d', cmd, 0))  { duplicateSelection(); return true; }
        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
        {
            for (auto* n : selection) clip.getSequence().removeNote (*n, um());
            setSelection ({});
            return true;
        }
        if (! selection.isEmpty() && (k.isKeyCode (juce::KeyPress::upKey) || k.isKeyCode (juce::KeyPress::downKey)))
        {
            const int dir = k.isKeyCode (juce::KeyPress::upKey) ? 1 : -1;
            const int amt = k.getModifiers().isShiftDown() ? 12 : 1;
            if (scaleSnap && scaleIndex > 0 && amt == 1) MidiTools::transposeInScale (clip, selection, dir, scaleRoot, scaleIndex);
            else for (auto* n : selection) n->setNoteNumber (juce::jlimit (0, 127, n->getNoteNumber() + dir * amt), um());
            if (selection.size() == 1) { previewNote (selection.getFirst()->getNoteNumber()); juce::Timer::callAfterDelay (150, [safe = juce::Component::SafePointer<PianoRollGrid> (this)] { if (safe) safe->stopPreview(); }); }
            repaint();
            return true;
        }
        if (! selection.isEmpty() && (k.isKeyCode (juce::KeyPress::leftKey) || k.isKeyCode (juce::KeyPress::rightKey)))
        {
            const double d = (k.isKeyCode (juce::KeyPress::leftKey) ? -1.0 : 1.0) * gridBeats;
            for (auto* n : selection) MidiTools::setStart (clip, n, n->getStartBeat().inBeats() + d);
            repaint();
            return true;
        }
        if (k.getModifiers().isCommandDown()) return false;
        switch (juce::CharacterFunctions::toLowerCase (k.getTextCharacter()))
        {
            case 'p': setTool (Tool::draw); return true;
            case 'e': setTool (Tool::select); return true;
            case 'd': setTool (Tool::erase); return true;
            case 'c': setTool (Tool::slice); return true;
            case 'q': MidiTools::quantise (clip, selection, gridBeats); repaint(); return true;
            default: break;
        }
        return false;
    }

    bool useFlats() const   { return scaleIndex > 0 && MidiTools::keyUsesFlats (scaleRoot, ! MidiTools::inScale (scaleRoot + 4, scaleRoot, scaleIndex)); }

    std::function<void()> onToolChanged;
    void setTool (Tool t) { tool = t; if (onToolChanged) onToolChanged(); }

    double beatToX (double b) const    { return keysW + b * pxPerBeat; }
    double xToBeat (float x) const     { return (x - keysW) / pxPerBeat; }
    int yToNote (int y) const          { return juce::jlimit (0, 127, topNote - y / keyH); }
    double clipLengthBeats() const     { return clip.getLengthInBeats().inBeats() + clip.getOffsetInBeats().inBeats(); }

    void setSelection (juce::Array<te::MidiNote*> s)
    {
        selection = s;
        if (onSelectionChanged) onSelectionChanged();
        repaint();
    }
    void validateSelection()
    {
        auto& all = clip.getSequence().getNotes();
        for (int i = selection.size(); --i >= 0;) if (! all.contains (selection[i])) selection.remove (i);
    }

    void stopPreview()
    {
        if (previewing >= 0)
            if (auto at = dynamic_cast<te::AudioTrack*> (clip.getTrack()))
                at->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, previewing), sourceID);
        previewing = -1;
        repaint (0, 0, keysW, getHeight());
    }

    int lastVelocity = 100;
    double lastLength = 0.25;

    /** The label on a key: user label, else the pad / channel name, else "". Cached per paint cycle (refreshed by the window's timer). */
    juce::String keyLabel (int note)
    {
        if (labelsDirty) { labels = clip.getTrack() != nullptr ? KeyLabels::allLabels (*clip.getTrack()) : std::map<int, juce::String>(); labelsDirty = false; }
        auto it = labels.find (note);
        return it == labels.end() ? juce::String() : it->second;
    }
    void refreshKeyLabels()   { labelsDirty = true; repaint (0, 0, keysW, getHeight()); }

    void showKeyMenu (int note)
    {
        auto* t = clip.getTrack();
        if (t == nullptr) return;
        juce::PopupMenu m;
        m.addSectionHeader (juce::MidiMessage::getMidiNoteName (note, true, true, 4) + (keyLabel (note).isNotEmpty() ? "  -  " + keyLabel (note) : juce::String()));
        m.addItem (1, "Rename key...");
        m.addItem (2, "Clear label", KeyLabels::userLabel (*t, note).isNotEmpty());
        m.addSeparator();
        m.addItem (3, "Select every note on this key");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> (localPointToGlobal (juce::Point<int> (keysW, (topNote - note) * keyH)), juce::Point<int> (1, keyH))),
                         [this, note, t] (int r)
        {
            if (r == 1)
            {
                auto aw = std::make_shared<juce::AlertWindow> ("Rename key " + juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                                                               "The label shows on the keyboard. Drum names come from the loaded kit automatically; this overrides them.",
                                                               juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("label", KeyLabels::labelFor (*t, note), "Label");
                aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, note, t] (int res)
                {
                    aw->setVisible (false);
                    if (res == 1 && t != nullptr) { KeyLabels::setUserLabel (*t, note, aw->getTextEditorContents ("label")); refreshKeyLabels(); }
                }));
            }
            else if (r == 2) { KeyLabels::setUserLabel (*t, note, {}); refreshKeyLabels(); }
            else if (r == 3) { juce::Array<te::MidiNote*> s; for (auto* n : clip.getSequence().getNotes()) if (n->getNoteNumber() == note) s.add (n); setSelection (s); }
        });
    }

private:
    std::map<int, juce::String> labels;
    bool labelsDirty = true;
    enum class DragMode { none, move, resize, band, erase } dragMode = DragMode::none;
    struct DragNote { te::MidiNote* note; double start, length; int pitch; };
    std::vector<DragNote> dragStart;
    te::MidiNote* dragAnchor = nullptr;
    juce::Point<float> bandStart;
    juce::Rectangle<float> band;
    int previewing = -1;
    te::MPESourceID sourceID = te::createUniqueMPESourceID();

    struct ClipNote { int pitch; double start, length; int vel; };
    static std::vector<ClipNote>& clipboard()   { static std::vector<ClipNote> c; return c; }

    juce::UndoManager* um()     { return &clip.edit.getUndoManager(); }

    double snap (double b, bool floorIt) const
    {
        return floorIt ? std::floor (b / gridBeats + 1e-9) * gridBeats : std::round (b / gridBeats) * gridBeats;
    }

    juce::Rectangle<float> noteRect (te::MidiNote& n) const
    {
        return { (float) beatToX (n.getStartBeat().inBeats()), (float) ((topNote - n.getNoteNumber()) * keyH + 1),
                 (float) juce::jmax (4.0, n.getLengthBeats().inBeats() * pxPerBeat), (float) keyH - 2 };
    }

    te::MidiNote* noteAt (juce::Point<float> p)
    {
        auto& notes = clip.getSequence().getNotes();
        for (int i = notes.size(); --i >= 0;)
            if (noteRect (*notes[i]).contains (p))
                return notes[i];
        return nullptr;
    }

    void removeNote (te::MidiNote* n)
    {
        selection.removeFirstMatchingValue (n);
        clip.getSequence().removeNote (*n, um());
        repaint();
    }

    void sliceAt (te::MidiNote* n, double beat)
    {
        const double s = n->getStartBeat().inBeats(), e = n->getEndBeat().inBeats();
        if (beat <= s + 1e-6 || beat >= e - 1e-6) return;
        n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (beat - s), um());
        clip.getSequence().addNote (n->getNoteNumber(), te::BeatPosition::fromBeats (beat), te::BeatDuration::fromBeats (e - beat), n->getVelocity(), 0, um());
        repaint();
    }

    void copySelection()
    {
        auto& cb = clipboard();
        cb.clear();
        if (selection.isEmpty()) return;
        double first = 1e9;
        for (auto* n : selection) first = juce::jmin (first, n->getStartBeat().inBeats());
        for (auto* n : selection) cb.push_back ({ n->getNoteNumber(), n->getStartBeat().inBeats() - first, n->getLengthBeats().inBeats(), n->getVelocity() });
    }

    void paste()
    {
        auto& cb = clipboard();
        if (cb.empty()) return;
        double at = 0;
        auto beat = clip.getContentBeatAtTime (clip.edit.getTransport().getPosition()).inBeats();
        if (! selection.isEmpty()) { for (auto* n : selection) at = juce::jmax (at, n->getEndBeat().inBeats()); at = snap (at, false); }
        else if (beat >= 0) at = snap (beat, true);
        juce::Array<te::MidiNote*> made;
        for (auto& c : cb) made.add (clip.getSequence().addNote (c.pitch, te::BeatPosition::fromBeats (at + c.start), te::BeatDuration::fromBeats (c.length), c.vel, 0, um()));
        setSelection (made);
    }

    void duplicateSelection()
    {
        if (selection.isEmpty()) return;
        double first = 1e9, last = 0;
        for (auto* n : selection) { first = juce::jmin (first, n->getStartBeat().inBeats()); last = juce::jmax (last, n->getEndBeat().inBeats()); }
        // duplicate after the selection, rounded up to the next bar-friendly grid line
        const double span = juce::jmax (gridBeats, std::ceil ((last - first) / gridBeats - 1e-6) * gridBeats);
        juce::Array<te::MidiNote*> made;
        for (auto* n : selection)
            made.add (clip.getSequence().addNote (n->getNoteNumber(), n->getStartBeat() + te::BeatDuration::fromBeats (span), n->getLengthBeats(), n->getVelocity(), 0, um()));
        setSelection (made);
    }

    void previewNote (int note)
    {
        stopPreview();
        if (auto at = dynamic_cast<te::AudioTrack*> (clip.getTrack()))
        {
            at->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), sourceID);
            previewing = note;
            repaint (0, 0, keysW, getHeight());
        }
    }

    /** Notes from the other MIDI tracks, drawn hollow (FL ghost notes), lined up in time. */
    void paintGhosts (juce::Graphics& g)
    {
        auto* myTrack = clip.getTrack();
        for (auto* t : te::getAudioTracks (clip.edit))
        {
            if (t == myTrack) continue;
            for (auto* c : t->getClips())
            {
                auto* mc = dynamic_cast<te::MidiClip*> (c);
                if (mc == nullptr) continue;
                auto range = mc->getEditTimeRange();
                if (! range.overlaps (clip.getEditTimeRange())) continue;
                g.setColour (t->getColour().withAlpha (0.45f));
                for (auto* n : mc->getSequence().getNotes())
                {
                    auto st = clip.getContentBeatAtTime (n->getEditStartTime (*mc)).inBeats();
                    auto en = clip.getContentBeatAtTime (n->getEditEndTime (*mc)).inBeats();
                    juce::Rectangle<float> r ((float) beatToX (st), (float) ((topNote - n->getNoteNumber()) * keyH + 2),
                                              (float) juce::jmax (3.0, (en - st) * pxPerBeat), (float) keyH - 4);
                    g.drawRoundedRectangle (r, 2.0f, 1.0f);
                }
            }
        }
    }
};

//==============================================================================
/** Velocity bars under the notes; drag to draw (selected notes only when there is a selection). */
class VelocityLane : public juce::Component
{
public:
    explicit VelocityLane (PianoRollGrid& g) : grid (g) {}
    int scrollX = 0;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        g.setColour (Theme::line());
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
        g.setColour (Theme::dim());
        g.setFont (Theme::font (10.0f));
        g.drawText ("Velocity", 4, 4, PianoRollGrid::keysW - 6, 14, juce::Justification::left);
        auto h = (float) getHeight() - 8.0f;
        for (auto* n : grid.clip.getSequence().getNotes())
        {
            auto x = (float) grid.beatToX (n->getStartBeat().inBeats()) - scrollX;
            if (x < PianoRollGrid::keysW - 2 || x > getWidth()) continue;
            auto v = n->getVelocity() / 127.0f;
            const bool sel = grid.selection.contains (n);
            g.setColour ((sel ? Theme::text() : Theme::acid().interpolatedWith (Theme::magenta(), 1.0f - v)).withAlpha (0.9f));
            g.fillRect (x, 4.0f + h * (1.0f - v), 3.0f, h * v);
            g.fillEllipse (x - 2.0f, 4.0f + h * (1.0f - v) - 2.5f, 7.0f, 5.0f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override   { apply (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override   { apply (e.position); }

private:
    PianoRollGrid& grid;
    void apply (juce::Point<float> p)
    {
        auto h = (float) getHeight() - 8.0f;
        int v = juce::jlimit (1, 127, juce::roundToInt ((1.0f - (p.y - 4.0f) / h) * 127.0f));
        grid.validateSelection();
        for (auto* n : grid.clip.getSequence().getNotes())
        {
            auto x = (float) grid.beatToX (n->getStartBeat().inBeats()) - scrollX;
            if (std::abs (x - p.x) <= 4.0f && (grid.selection.isEmpty() || grid.selection.contains (n)))
                n->setVelocity (v, &grid.clip.edit.getUndoManager());
        }
        grid.lastVelocity = v;
        repaint();
        grid.repaint();
    }
};

//==============================================================================
class PianoRollWindow : public juce::DocumentWindow,
                        private juce::Timer
{
public:
    PianoRollWindow (te::MidiClip& c)
        : DocumentWindow ("MIDI Editor - " + c.getName() + " (" + (c.getTrack() != nullptr ? c.getTrack()->getName() : juce::String()) + ")",
                          Theme::bg(), DocumentWindow::allButtons),
          clipRef (&c), content (c)
    {
        setUsingNativeTitleBar (true);
        setContentNonOwned (&content, false);
        setResizable (true, true);
        centreWithSize (1240, 720);
        setVisible (true);
        toFront (true);
        startTimerHz (20);
    }

    void closeButtonPressed() override  { if (onClose) onClose(); }
    std::function<void()> onClose;
    te::Clip::Ptr clipRef;

private:
    struct Viewer : public juce::Viewport
    {
        std::function<void()> onScroll;
        void visibleAreaChanged (const juce::Rectangle<int>&) override   { if (onScroll) onScroll(); }
    };

    struct Content : public juce::Component
    {
        explicit Content (te::MidiClip& c) : grid (c), velocity (grid)
        {
            viewport.setViewedComponent (&grid, false);
            viewport.onScroll = [this] { velocity.scrollX = viewport.getViewPositionX(); velocity.repaint(); };
            addAndMakeVisible (viewport);
            addAndMakeVisible (velocity);
            grid.onZoom = [this] { layout(); };
            grid.onToolChanged = [this] { refreshTools(); };
            grid.onSelectionChanged = [this] { velocity.repaint(); };

            const char* toolNames[] = { "Draw", "Select", "Erase", "Slice" };
            const char* toolTips[]  = { "Draw notes (P)", "Select notes / rubber-band (E)", "Erase notes (D)", "Slice notes at the grid (C)" };
            for (int i = 0; i < 4; ++i)
            {
                auto b = std::make_unique<juce::TextButton> (toolNames[i]);
                b->setTooltip (toolTips[i]);
                b->onClick = [this, i] { grid.setTool ((PianoRollGrid::Tool) i); };
                addAndMakeVisible (*b);
                toolButtons.push_back (std::move (b));
            }

            gridBtn.onClick = [this] { showGridMenu(); };
            snapToggle.setToggleState (true, juce::dontSendNotification);
            snapToggle.onClick = [this] { grid.snapOn = snapToggle.getToggleState(); };
            snapToggle.setTooltip ("Snap to the grid (hold Cmd while dragging to move freely)");

            for (int i = 0; i < 12; ++i)
                rootBox.addItem (MidiTools::pitchClassName (i, false) + (juce::MidiMessage::isMidiNoteBlack (i) ? " / " + MidiTools::pitchClassName (i, true) : juce::String()), i + 1);
            for (int i = 0; i < (int) MidiTools::scales().size(); ++i) scaleBox.addItem (MidiTools::scales()[(size_t) i].name, i + 1);
            rootBox.setSelectedId (grid.scaleRoot + 1, juce::dontSendNotification);
            scaleBox.setSelectedId (grid.scaleIndex + 1, juce::dontSendNotification);
            rootBox.setTooltip ("Key root");
            scaleBox.setTooltip ("Scale: highlights the rows in key (Chromatic = no highlight)");
            rootBox.onChange = scaleBox.onChange = [this] { grid.setScale (rootBox.getSelectedId() - 1, scaleBox.getSelectedId() - 1); };
            scaleSnapToggle.setTooltip ("Keep drawn and moved notes in the key; arrow keys move by scale steps");
            scaleSnapToggle.onClick = [this] { grid.scaleSnap = scaleSnapToggle.getToggleState(); };

            chordBox.addItem ("Chord stamp: off", 1);
            for (int i = 0; i < (int) MidiTools::chordTypes().size(); ++i) chordBox.addItem (MidiTools::chordTypes()[(size_t) i].name, i + 2);
            chordBox.setSelectedId (1, juce::dontSendNotification);
            chordBox.setTooltip ("Chord Stamp: Draw places this chord on the note you click");
            chordBox.onChange = [this] { grid.chordType = chordBox.getSelectedId() - 2; };
            for (int i = 0; i < 4; ++i) invBox.addItem (i == 0 ? "Root" : "Inv " + juce::String (i), i + 1);
            invBox.setSelectedId (1, juce::dontSendNotification);
            invBox.setTooltip ("Chord inversion (bottom-up mode)");
            invBox.onChange = [this] { grid.chordInversion = invBox.getSelectedId() - 1; };
            stampModeBox.addItem ("Bottom-up", 1);
            stampModeBox.addItem ("Top-down", 2);
            stampModeBox.setSelectedId (1, juce::dontSendNotification);
            stampModeBox.setTooltip ("Chord Stamp mode. Bottom-up: the key you click is the chord's root (harmony-led). Top-down: the key you click stays on top as the melody note and the chord is built underneath it (melody-led).");
            stampModeBox.onChange = [this] { grid.chordTopDown = stampModeBox.getSelectedId() == 2; invBox.setEnabled (! grid.chordTopDown); };

            toolsBtn.onClick = [this] { showToolsMenu(); };
            toolsBtn.setTooltip ("Quantise, humanise, strum, arpeggiate, flam, legato, 808 slides, chop, scale tools, velocity, flip, riff starter (acts on the selection, or every note)");
            chordsBtn.onClick = [this] { showChordsMenu(); };
            chordsBtn.setTooltip ("Write a chord progression by mood in the key you picked (with a bass / 808 line)");
            progBtn.onClick = [this] { openProgressionTool(); };
            progBtn.setTooltip ("Chord Progression Tool: pick a key and a genre, build / edit a progression chord by chord, hear it, insert it");
            progBtn.setColour (juce::TextButton::buttonColourId, Theme::violet().withAlpha (0.55f));
            ghostToggle.setToggleState (true, juce::dontSendNotification);
            ghostToggle.setTooltip ("Ghost notes: show the other MIDI tracks' notes, hollow");
            ghostToggle.onClick = [this] { grid.showGhosts = ghostToggle.getToggleState(); grid.repaint(); };
            zoomIn.onClick = [this] { grid.pxPerBeat = juce::jmin (600.0, grid.pxPerBeat * 1.3); layout(); };
            zoomOut.onClick = [this] { grid.pxPerBeat = juce::jmax (12.0, grid.pxPerBeat / 1.3); layout(); };
            quant.onClick = [this] { MidiTools::quantise (grid.clip, grid.selection, grid.gridBeats); grid.repaint(); };
            quant.setTooltip ("Quantise the selection (or every note) to the grid (Q)");

            for (auto* comp : std::initializer_list<juce::Component*> { &gridBtn, &snapToggle, &rootBox, &scaleBox, &scaleSnapToggle, &chordBox, &invBox, &stampModeBox,
                                                                       &toolsBtn, &chordsBtn, &progBtn, &ghostToggle, &zoomIn, &zoomOut, &quant, &keyLabel, &hint })
                addAndMakeVisible (comp);
            keyLabel.setText ("Key", juce::dontSendNotification);
            keyLabel.setColour (juce::Label::textColourId, Theme::dim());
            hint.setText ("P draw  E select  D erase  C slice | right-click deletes | Alt-drag copies | Cmd+D duplicate | arrows move (Shift = octave) | Cmd+wheel zooms",
                          juce::dontSendNotification);
            hint.setColour (juce::Label::textColourId, Theme::dim());
            hint.setFont (Theme::font (11.0f));
            setSize (1240, 720);
            refreshTools();
        }

        void refreshTools()
        {
            for (int i = 0; i < (int) toolButtons.size(); ++i)
            {
                const bool on = (int) grid.tool == i;
                toolButtons[(size_t) i]->setColour (juce::TextButton::buttonColourId, on ? Theme::acid() : Theme::panel2());
                toolButtons[(size_t) i]->setColour (juce::TextButton::textColourOffId, on ? Theme::bg() : Theme::text());
            }
            grid.setMouseCursor (grid.tool == PianoRollGrid::Tool::erase ? juce::MouseCursor::CrosshairCursor
                                 : grid.tool == PianoRollGrid::Tool::slice ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto tb = r.removeFromTop (36).reduced (6, 5);
            for (auto& b : toolButtons) { b->setBounds (tb.removeFromLeft (62)); tb.removeFromLeft (2); }
            tb.removeFromLeft (10);
            gridBtn.setBounds (tb.removeFromLeft (104)); tb.removeFromLeft (4);
            snapToggle.setBounds (tb.removeFromLeft (64)); tb.removeFromLeft (10);
            keyLabel.setBounds (tb.removeFromLeft (30));
            rootBox.setBounds (tb.removeFromLeft (84)); tb.removeFromLeft (4);
            scaleBox.setBounds (tb.removeFromLeft (140)); tb.removeFromLeft (4);
            scaleSnapToggle.setBounds (tb.removeFromLeft (100)); tb.removeFromLeft (10);
            chordBox.setBounds (tb.removeFromLeft (150)); tb.removeFromLeft (4);
            invBox.setBounds (tb.removeFromLeft (70)); tb.removeFromLeft (4);
            stampModeBox.setBounds (tb.removeFromLeft (100)); tb.removeFromLeft (10);
            auto tb2 = r.removeFromTop (32).reduced (6, 4);
            toolsBtn.setBounds (tb2.removeFromLeft (90)); tb2.removeFromLeft (4);
            chordsBtn.setBounds (tb2.removeFromLeft (90)); tb2.removeFromLeft (4);
            progBtn.setBounds (tb2.removeFromLeft (140)); tb2.removeFromLeft (4);
            quant.setBounds (tb2.removeFromLeft (84)); tb2.removeFromLeft (10);
            ghostToggle.setBounds (tb2.removeFromLeft (110)); tb2.removeFromLeft (10);
            zoomOut.setBounds (tb2.removeFromLeft (30)); tb2.removeFromLeft (2);
            zoomIn.setBounds (tb2.removeFromLeft (30)); tb2.removeFromLeft (12);
            hint.setBounds (tb2);
            velocity.setBounds (r.removeFromBottom (96));
            viewport.setBounds (r);
            layout();
        }

        void layout()
        {
            auto lenBeats = grid.clipLengthBeats();
            grid.setSize (juce::jmax (viewport.getWidth(), (int) (PianoRollGrid::keysW + (lenBeats + 4) * grid.pxPerBeat)), 128 * PianoRollGrid::keyH);
            grid.repaint();
            velocity.scrollX = viewport.getViewPositionX();
            velocity.repaint();
        }

        void scrollToNotes()
        {
            // centre the view on the notes (a drum clip at C2 shows C2, not C4-C6)
            int hi = -1, lo = 128;
            for (auto n : grid.clip.getSequence().getNotes()) { hi = juce::jmax (hi, n->getNoteNumber()); lo = juce::jmin (lo, n->getNoteNumber()); }
            if (hi < 0) { hi = 72; lo = 48; }
            const int visible = juce::jmax (1, viewport.getViewHeight() / PianoRollGrid::keyH);
            int top = hi + juce::jmax (3, (visible - (hi - lo + 1)) / 2);
            top = juce::jlimit (juce::jmin (127, visible - 1), 127, top);
            viewport.setViewPosition (0, juce::jmax (0, (127 - top) * PianoRollGrid::keyH));
        }

        void showGridMenu()
        {
            juce::PopupMenu m;
            const double grids[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 16.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };
            const char* names[] = { "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/4 triplet", "1/8 triplet", "1/16 triplet", "1/32 triplet" };
            for (int i = 0; i < 11; ++i) m.addItem (i + 1, names[i], true, std::abs (grid.gridBeats - grids[i]) < 1e-6);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (gridBtn), [this, grids, names] (int r)
            {
                if (r > 0) { grid.gridBeats = grids[r - 1]; gridBtn.setButtonText ("Grid " + juce::String (names[r - 1])); grid.repaint(); }
            });
        }

        void showToolsMenu()
        {
            grid.validateSelection();
            juce::PopupMenu m, q, hum, str, arp, sl, ch, tr, vel;
            q.addItem (100, "100% (hard)"); q.addItem (101, "75%"); q.addItem (102, "50%"); q.addItem (103, "Starts and ends"); q.addItem (104, "With 50% swing"); q.addItem (105, "With 25% swing");
            hum.addItem (110, "Light"); hum.addItem (111, "Medium"); hum.addItem (112, "Heavy");
            str.addItem (120, "Up, fast (1/64)"); str.addItem (121, "Up (1/32)"); str.addItem (122, "Up, slow (1/16)"); str.addItem (123, "Down, fast (1/64)"); str.addItem (124, "Down (1/32)");
            arp.addItem (130, "Up 1/16"); arp.addItem (131, "Down 1/16"); arp.addItem (132, "Up-down 1/16"); arp.addItem (133, "Random 1/16");
            arp.addItem (134, "Up 1/8"); arp.addItem (135, "Up 1/32"); arp.addItem (136, "Up 1/16 triplet"); arp.addItem (137, "Up 1/16 over 2 octaves");
            sl.addItem (140, "Short glide (1/32)"); sl.addItem (141, "Medium glide (1/16)"); sl.addItem (142, "Long glide (1/8)");
            ch.addItem (150, "1/8"); ch.addItem (151, "1/16"); ch.addItem (152, "1/32"); ch.addItem (153, "1/16 triplet");
            tr.addItem (160, "Up one scale step"); tr.addItem (161, "Down one scale step"); tr.addItem (162, "Up a third (in key)"); tr.addItem (163, "Down a third (in key)");
            tr.addItem (164, "Octave up"); tr.addItem (165, "Octave down");
            vel.addItem (170, "Louder (+10%)"); vel.addItem (171, "Softer (-10%)"); vel.addItem (172, "Even out (towards 100)"); vel.addItem (173, "All 127"); vel.addItem (174, "All 100");
            const bool any = grid.clip.getSequence().getNumNotes() > 0;
            m.addSectionHeader (grid.selection.isEmpty() ? "Acts on every note" : "Acts on " + juce::String (grid.selection.size()) + " selected note(s)");
            m.addSubMenu ("Quantise", q, any);
            m.addSubMenu ("Humanise", hum, any);
            m.addSubMenu ("Strum chords", str, any);
            m.addSubMenu ("Arpeggiate chords", arp, any);
            m.addItem (180, "Flam (grace hit before each note)", any);
            m.addItem (181, "Legato (each note to the next)", any);
            m.addSubMenu ("808 slides (glide into the next note)", sl, any);
            m.addSubMenu ("Chop", ch, any);
            m.addSeparator();
            m.addItem (182, "Snap to scale (" + rootBox.getText() + " " + scaleBox.getText() + ")", any && grid.scaleIndex > 0);
            m.addSubMenu ("Transpose", tr, any);
            m.addSubMenu ("Velocity", vel, any);
            m.addItem (183, "Flip (mirror pitches)", any);
            m.addSeparator();
            m.addItem (184, "Riff starter: random melody in key over the clip");
            m.addItem (185, "Select all");
            m.addItem (186, "Delete selected", ! grid.selection.isEmpty());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (toolsBtn), [this] (int r) { runTool (r); });
        }

        void runTool (int r)
        {
            if (r == 0) return;
            auto& c = grid.clip;
            grid.validateSelection();
            auto sel = grid.selection;
            const int root = grid.scaleRoot, scale = juce::jmax (1, grid.scaleIndex);
            switch (r)
            {
                case 100: MidiTools::quantise (c, sel, grid.gridBeats); break;
                case 101: MidiTools::quantise (c, sel, grid.gridBeats, 0.75f); break;
                case 102: MidiTools::quantise (c, sel, grid.gridBeats, 0.5f); break;
                case 103: MidiTools::quantise (c, sel, grid.gridBeats, 1.0f, 0.0f, true); break;
                case 104: MidiTools::quantise (c, sel, grid.gridBeats, 1.0f, 0.5f); break;
                case 105: MidiTools::quantise (c, sel, grid.gridBeats, 1.0f, 0.25f); break;
                case 110: MidiTools::humanise (c, sel, 0.01, 6, juce::Random::getSystemRandom().nextInt()); break;
                case 111: MidiTools::humanise (c, sel, 0.02, 12, juce::Random::getSystemRandom().nextInt()); break;
                case 112: MidiTools::humanise (c, sel, 0.04, 20, juce::Random::getSystemRandom().nextInt()); break;
                case 120: MidiTools::strum (c, sel, 1.0 / 16.0, true); break;
                case 121: MidiTools::strum (c, sel, 1.0 / 8.0, true); break;
                case 122: MidiTools::strum (c, sel, 1.0 / 4.0, true); break;
                case 123: MidiTools::strum (c, sel, 1.0 / 16.0, false); break;
                case 124: MidiTools::strum (c, sel, 1.0 / 8.0, false); break;
                case 130: MidiTools::arpeggiate (c, sel, 0.25, MidiTools::ArpMode::up); break;
                case 131: MidiTools::arpeggiate (c, sel, 0.25, MidiTools::ArpMode::down); break;
                case 132: MidiTools::arpeggiate (c, sel, 0.25, MidiTools::ArpMode::upDown); break;
                case 133: MidiTools::arpeggiate (c, sel, 0.25, MidiTools::ArpMode::random); break;
                case 134: MidiTools::arpeggiate (c, sel, 0.5, MidiTools::ArpMode::up); break;
                case 135: MidiTools::arpeggiate (c, sel, 0.125, MidiTools::ArpMode::up); break;
                case 136: MidiTools::arpeggiate (c, sel, 1.0 / 6.0, MidiTools::ArpMode::up); break;
                case 137: MidiTools::arpeggiate (c, sel, 0.25, MidiTools::ArpMode::up, 0.8f, 2); break;
                case 140: MidiTools::slides (c, sel, 0.125); break;
                case 141: MidiTools::slides (c, sel, 0.25); break;
                case 142: MidiTools::slides (c, sel, 0.5); break;
                case 150: MidiTools::chop (c, sel, 0.5); break;
                case 151: MidiTools::chop (c, sel, 0.25); break;
                case 152: MidiTools::chop (c, sel, 0.125); break;
                case 153: MidiTools::chop (c, sel, 1.0 / 6.0); break;
                case 160: MidiTools::transposeInScale (c, sel, 1, root, scale); break;
                case 161: MidiTools::transposeInScale (c, sel, -1, root, scale); break;
                case 162: MidiTools::transposeInScale (c, sel, 2, root, scale); break;
                case 163: MidiTools::transposeInScale (c, sel, -2, root, scale); break;
                case 164: for (auto* n : MidiTools::notesOrAll (c, sel)) n->setNoteNumber (juce::jmin (127, n->getNoteNumber() + 12), &c.edit.getUndoManager()); break;
                case 165: for (auto* n : MidiTools::notesOrAll (c, sel)) n->setNoteNumber (juce::jmax (0, n->getNoteNumber() - 12), &c.edit.getUndoManager()); break;
                case 170: MidiTools::scaleVelocities (c, sel, 1.1f); break;
                case 171: MidiTools::scaleVelocities (c, sel, 0.9f); break;
                case 172: for (auto* n : MidiTools::notesOrAll (c, sel)) n->setVelocity ((n->getVelocity() + 100) / 2, &c.edit.getUndoManager()); break;
                case 173: MidiTools::scaleVelocities (c, sel, 0.0f, 127); break;
                case 174: MidiTools::scaleVelocities (c, sel, 0.0f, 100); break;
                case 180: MidiTools::flam (c, sel); break;
                case 181: MidiTools::legato (c, sel); break;
                case 182: MidiTools::snapNotesToScale (c, sel, grid.scaleRoot, grid.scaleIndex); break;
                case 183: MidiTools::flip (c, sel); break;
                case 184:
                {
                    auto made = MidiTools::randomMelody (c, root, scale, 0.0, grid.clipLengthBeats(), juce::jmax (0.125, grid.gridBeats), 60, 79, 0.6f,
                                                         juce::Random::getSystemRandom().nextInt());
                    grid.setSelection (made);
                    break;
                }
                case 185: { juce::Array<te::MidiNote*> all; for (auto* n : c.getSequence().getNotes()) all.add (n); grid.setSelection (all); break; }
                case 186: for (auto* n : sel) c.getSequence().removeNote (*n, &c.edit.getUndoManager()); grid.setSelection ({}); break;
                default: break;
            }
            grid.validateSelection();
            grid.repaint();
            velocity.repaint();
        }

        void showChordsMenu()
        {
            juce::PopupMenu m;
            m.addItem (4, "Chord Progression Tool... (build and edit by genre, preview, insert)");
            m.addSeparator();
            m.addSectionHeader ("Quick write - key of " + rootBox.getText() + " (minor progressions use " + rootBox.getText() + " minor)");
            m.addItem (1, "Add a bass / 808 line under the chords", true, withBass);
            m.addItem (2, "One chord every 2 bars", true, twoBars);
            m.addItem (3, "Replace the notes already in the clip", true, replaceNotes);
            m.addSeparator();
            juce::StringArray moods;
            for (auto& p : MidiTools::progressions()) moods.addIfNotAlreadyThere (p.mood);
            for (auto& mood : moods)
            {
                juce::PopupMenu sub;
                for (int i = 0; i < (int) MidiTools::progressions().size(); ++i)
                    if (mood == MidiTools::progressions()[(size_t) i].mood) sub.addItem (100 + i, MidiTools::progressions()[(size_t) i].name);
                m.addSubMenu (mood, sub);
            }
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (chordsBtn), [this] (int r)
            {
                if (r == 1) { withBass = ! withBass; juce::MessageManager::callAsync ([this] { showChordsMenu(); }); return; }
                if (r == 2) { twoBars = ! twoBars; juce::MessageManager::callAsync ([this] { showChordsMenu(); }); return; }
                if (r == 3) { replaceNotes = ! replaceNotes; juce::MessageManager::callAsync ([this] { showChordsMenu(); }); return; }
                if (r == 4) { openProgressionTool(); return; }
                if (r < 100) return;
                auto& c = grid.clip;
                if (replaceNotes) { auto all = c.getSequence().getNotes(); for (int i = all.size(); --i >= 0;) c.getSequence().removeNote (*all[i], &c.edit.getUndoManager()); }
                const int bpb = juce::jmax (1, c.edit.tempoSequence.getTimeSig (0)->numerator.get());
                auto names = MidiTools::writeProgression (c, r - 100, grid.scaleRoot, 0.0, (twoBars ? 2 : 1) * bpb, grid.clipLengthBeats(), 4, withBass);
                auto& prog = MidiTools::progressions()[(size_t) (r - 100)];
                grid.setScale (grid.scaleRoot, prog.minorKey ? 2 : 1);
                scaleBox.setSelectedId (grid.scaleIndex + 1, juce::dontSendNotification);
                grid.setSelection ({});
                hint.setText ("Wrote " + names.joinIntoString (" - ") + (withBass ? " with a bass line" : ""), juce::dontSendNotification);
                scrollToNotes();
            });
        }

        void openProgressionTool()
        {
            if (chordTool != nullptr) { chordTool->toFront (true); return; }
            chordTool = std::make_unique<ChordToolWindow> (grid.clip, grid.scaleRoot, grid.scaleIndex);
            chordTool->onClose = [this] { juce::MessageManager::callAsync ([this] { chordTool.reset(); }); };
            chordTool->onWritten = [this] (juce::StringArray names, bool minorKey)
            {
                grid.setScale (chordTool != nullptr ? chordTool->state().keyRoot : grid.scaleRoot, minorKey ? 2 : 1);
                rootBox.setSelectedId (grid.scaleRoot + 1, juce::dontSendNotification);
                scaleBox.setSelectedId (grid.scaleIndex + 1, juce::dontSendNotification);
                grid.setSelection ({});
                hint.setText ("Wrote " + names.joinIntoString (" - "), juce::dontSendNotification);
                layout();
                scrollToNotes();
            };
        }

        PianoRollGrid grid;
        Viewer viewport;
        VelocityLane velocity;
        std::vector<std::unique_ptr<juce::TextButton>> toolButtons;
        juce::TextButton gridBtn { "Grid 1/16" }, toolsBtn { "Tools" }, chordsBtn { "Chords" }, progBtn { "Progression Tool" }, zoomIn { "+" }, zoomOut { "-" }, quant { "Quantise" };
        juce::ToggleButton snapToggle { "Snap" }, scaleSnapToggle { "Scale snap" }, ghostToggle { "Ghost notes" };
        juce::ComboBox rootBox, scaleBox, chordBox, invBox, stampModeBox;
        juce::Label keyLabel, hint;
        bool withBass = true, twoBars = false, replaceNotes = false;
        std::unique_ptr<ChordToolWindow> chordTool;
    };

    Content content;
    bool scrolled = false;
    int ticks = 0;

    void timerCallback() override
    {
        if (clipRef == nullptr || clipRef->getTrack() == nullptr) { stopTimer(); closeButtonPressed(); return; }
        if (! scrolled) { content.grid.topNote = 127; content.layout(); content.scrollToNotes(); scrolled = true; }
        if (clipRef->edit.getTransport().isPlaying() || (++ticks % 4) == 0) { content.grid.repaint(); if (ticks % 4 == 0) content.velocity.repaint(); }
        if (ticks % 40 == 0) content.grid.refreshKeyLabels();   // kit / pad names can change while the editor is open
    }
};
