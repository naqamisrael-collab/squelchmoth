#pragma once
// Squelchmoth 1.25 - the Hearing window: one note, seen as a colour and as numbers.
// A wheel of fifths in the colour language (pitch class = hue, octave = lightness), the note's numbers, its harmonic
// series, the beating against a second tone, and the standing waves of a room. Closed until it is asked for; every
// panel answers one question and says so in a plain sentence.

#include <JuceHeader.h>
#include "Console.h"
#include "SqHearing.h"

class HearingContent : public juce::Component
{
public:
    HearingContent()
    {
        namespace L = Console::Look;
        auto styleEdit = [this] (juce::TextEditor& e, const juce::String& text, const juce::String& hint)
        {
            addAndMakeVisible (e);
            e.setText (text, false); e.setTextToShowWhenEmpty (hint, L::faint());
            e.setFont (Theme::font (14.0f)); e.setJustification (juce::Justification::centred); e.setSelectAllWhenFocused (true);
            e.setColour (juce::TextEditor::backgroundColourId, L::well()); e.setColour (juce::TextEditor::textColourId, L::text());
            e.setColour (juce::TextEditor::outlineColourId, L::faint()); e.setColour (juce::TextEditor::focusedOutlineColourId, L::cyan());
            e.onReturnKey = [this] { readFields(); }; e.onFocusLost = [this] { readFields(); };
        };
        styleEdit (noteEdit, "A4", "note or Hz");
        styleEdit (secondEdit, "443", "Hz");
        styleEdit (lenEdit, "5", "m"); styleEdit (widEdit, "4", "m"); styleEdit (heiEdit, "2.5", "m");
        addAndMakeVisible (visionBtn);
        visionBtn.setColour (juce::TextButton::buttonColourId, L::well()); visionBtn.setColour (juce::TextButton::textColourOffId, L::text());
        visionBtn.onClick = [this] { vision = vision == SqHear::Vision::normal ? SqHear::Vision::protan : vision == SqHear::Vision::protan ? SqHear::Vision::deutan : SqHear::Vision::normal; updateVisionText(); repaint(); };
        updateVisionText();
        setWantsKeyboardFocus (true);
        setSize (980, 640);
    }

    //==========================================================================
    /** Sets the note everything shows (also what the assistant's tool calls). */
    void setFrequency (double hz)
    {
        if (hz < 8.0 || hz > 24000.0) return;
        freq = hz; noteEdit.setText (text2 (hz) + " Hz", false); repaint();
    }
    void setSecond (double hz)   { if (hz > 0) { second = hz; secondEdit.setText (text2 (hz), false); repaint(); } }
    void setRoom (double l, double w, double h) { if (l > 0 && w > 0 && h > 0) { roomL = l; roomW = w; roomH = h; lenEdit.setText (juce::String (l), false); widEdit.setText (juce::String (w), false); heiEdit.setText (juce::String (h), false); repaint(); } }
    double frequency() const     { return freq; }
    juce::String readout() const { const auto n = SqHear::nearestNote (freq); return n.name + " " + cents (n.cents) + ", " + text2 (freq) + " Hz, colour " + SqHear::noteColour (n.midi).hex(); }
    void setVision (SqHear::Vision v) { vision = v; updateVisionText(); repaint(); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        namespace L = Console::Look;
        g.setGradientFill (juce::ColourGradient (L::bodyTop(), 0.0f, 0.0f, L::bodyBot(), 0.0f, (float) getHeight(), false)); g.fillAll();
        g.setColour (L::text()); g.setFont (L::caps (15.0f));
        g.drawText ("HEARING", header.withTrimmedLeft (18), juce::Justification::centredLeft);
        g.setColour (L::dim()); g.setFont (Theme::font (12.0f));
        g.drawText ("One note as a colour and as numbers. The same note has the same colour everywhere in Squelchmoth.", header.withTrimmedLeft (110).withTrimmedRight (200), juce::Justification::centredLeft);

        paintWheel (g);
        paintNote (g);
        paintHarmonics (g);
        paintBeats (g);
        paintRoom (g);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        header = r.removeFromTop (46);
        visionBtn.setBounds (header.getRight() - 196, header.getY() + 9, 180, 28);
        r.reduce (14, 4); r.removeFromBottom (10);
        auto left = r.removeFromLeft (juce::jmin (r.getHeight(), r.getWidth() * 42 / 100));
        wheelArea = left.reduced (4);
        r.removeFromLeft (12);
        noteArea = r.removeFromTop (150);
        r.removeFromTop (10);
        auto bottom = r.removeFromBottom (juce::jmax (176, r.getHeight() * 44 / 100));
        r.removeFromBottom (10);
        harmArea = r;
        beatArea = bottom.removeFromLeft (bottom.getWidth() * 42 / 100); bottom.removeFromLeft (10); roomArea = bottom;

        noteEdit.setBounds (noteArea.getRight() - 150, noteArea.getY() + 14, 132, 28);
        secondEdit.setBounds (beatArea.getX() + 14, beatArea.getY() + 44, 92, 26);
        int x = roomArea.getX() + 14;
        for (auto* e : { &lenEdit, &widEdit, &heiEdit }) { e->setBounds (x, roomArea.getY() + 44, 56, 26); x += 64; }
    }

    void mouseDown (const juce::MouseEvent& e) override   { pickFromWheel (e.position); grabKeyboardFocus(); }
    void mouseDrag (const juce::MouseEvent& e) override   { pickFromWheel (e.position); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        // arrows: left / right a semitone, up / down an octave
        const int step = k == juce::KeyPress::rightKey ? 1 : k == juce::KeyPress::leftKey ? -1 : k == juce::KeyPress::upKey ? 12 : k == juce::KeyPress::downKey ? -12 : 0;
        if (step == 0) return false;
        setFrequency (SqHear::midiToHz (juce::jlimit (12, 119, SqHear::nearestNote (freq).midi + step)));
        return true;
    }

private:
    static juce::String text2 (double v)   { return juce::String (v, v >= 1000.0 ? 1 : 2); }
    static juce::String cents (double c)   { return (c >= 0 ? "+" : "") + juce::String (c, 1) + " cents"; }
    juce::Colour colourOf (int midi, double db = -6.0) const { const auto c = SqHear::seenAs (SqHear::noteColour (midi, db), vision); return juce::Colour (c.r, c.g, c.b); }
    void updateVisionText() { visionBtn.setButtonText (vision == SqHear::Vision::normal ? "View: full colour vision" : vision == SqHear::Vision::protan ? "View: red-blind" : "View: green-blind"); }

    void readFields()
    {
        const double hz = SqHear::parseNoteOrHz (noteEdit.getText());
        if (hz >= 8.0 && hz <= 24000.0) freq = hz;
        const double s = secondEdit.getText().getDoubleValue(); if (s > 0) second = s;
        const double l = lenEdit.getText().getDoubleValue(), w = widEdit.getText().getDoubleValue(), h = heiEdit.getText().getDoubleValue();
        if (l > 0 && w > 0 && h > 0) { roomL = l; roomW = w; roomH = h; }
        repaint();
    }

    static void panel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& sentence)
    {
        namespace L = Console::Look;
        g.setColour (L::well().withAlpha (0.85f)); g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (L::faint()); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (L::cyan()); g.setFont (L::caps (11.5f)); g.drawText (title, r.getX() + 14, r.getY() + 10, r.getWidth() - 28, 16, juce::Justification::centredLeft);
        g.setColour (L::dim()); g.setFont (Theme::font (11.5f)); g.drawFittedText (sentence, r.getX() + 14, r.getBottom() - 34, r.getWidth() - 28, 28, juce::Justification::bottomLeft, 2, 0.9f);
    }

    //========================================================================== the wheel of fifths
    static constexpr int firstOctave = 1, lastOctave = 7;
    juce::Point<float> wheelCentre() const { return wheelArea.toFloat().getCentre(); }
    float wheelOuter() const { return juce::jmin (wheelArea.getWidth(), wheelArea.getHeight()) * 0.5f - 30.0f; }
    float wheelInner() const { return wheelOuter() * 0.20f; }

    void paintWheel (juce::Graphics& g)
    {
        namespace L = Console::Look;
        const auto c = wheelCentre(); const float R = wheelOuter(), r0 = wheelInner(); const int rings = lastOctave - firstOctave + 1;
        const auto sel = SqHear::nearestNote (freq);
        static const int classAt[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };     // going round by fifths from C
        static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int k = 0; k < 12; ++k)
        {
            const float a0 = juce::MathConstants<float>::twoPi * ((float) k - 0.5f) / 12.0f, a1 = juce::MathConstants<float>::twoPi * ((float) k + 0.5f) / 12.0f;
            for (int o = 0; o < rings; ++o)
            {
                const float ri = r0 + (R - r0) * (float) o / (float) rings, ro = r0 + (R - r0) * (float) (o + 1) / (float) rings;
                const int midi = (firstOctave + o + 1) * 12 + classAt[k];
                juce::Path p; p.addCentredArc (c.x, c.y, ro, ro, 0.0f, a0, a1, true); p.addCentredArc (c.x, c.y, ri, ri, 0.0f, a1, a0, false); p.closeSubPath();
                g.setColour (colourOf (midi)); g.fillPath (p);
                g.setColour (L::well().withAlpha (0.55f)); g.strokePath (p, juce::PathStrokeType (1.0f));
                if (midi == sel.midi) { g.setColour (juce::Colours::white); g.strokePath (p, juce::PathStrokeType (2.6f)); }
            }
            const float am = juce::MathConstants<float>::twoPi * (float) k / 12.0f;
            const auto at = c.getPointOnCircumference (R + 16.0f, am);
            const bool on = ((sel.midi % 12) + 12) % 12 == classAt[k];
            g.setColour (on ? L::text() : L::dim()); g.setFont (L::caps (on ? 14.0f : 12.5f));
            g.drawText (names[classAt[k]], juce::Rectangle<float> (40.0f, 18.0f).withCentre (at), juce::Justification::centred);
        }
        g.setColour (L::dim()); g.setFont (Theme::font (10.5f));
        g.drawText ("octave " + juce::String (firstOctave), juce::Rectangle<float> (70.0f, 14.0f).withCentre (c), juce::Justification::centred);
        g.drawText ("octave " + juce::String (lastOctave) + " at the rim", wheelArea.getX(), wheelArea.getBottom() - 30, wheelArea.getWidth(), 14, juce::Justification::centred);
        g.drawText ("Neighbours are a fifth apart. Click a cell, or use the arrow keys.", wheelArea.getX(), wheelArea.getBottom() - 16, wheelArea.getWidth(), 14, juce::Justification::centred);
    }

    void pickFromWheel (juce::Point<float> p)
    {
        const auto c = wheelCentre(); const float d = p.getDistanceFrom (c), R = wheelOuter(), r0 = wheelInner();
        if (d < r0 || d > R) return;
        static const int classAt[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };
        float ang = std::atan2 (p.x - c.x, -(p.y - c.y)); if (ang < 0) ang += juce::MathConstants<float>::twoPi;
        const int k = ((int) std::floor (ang / juce::MathConstants<float>::twoPi * 12.0f + 0.5f)) % 12;
        const int rings = lastOctave - firstOctave + 1, o = juce::jlimit (0, rings - 1, (int) ((d - r0) / (R - r0) * (float) rings));
        setFrequency (SqHear::midiToHz ((firstOctave + o + 1) * 12 + classAt[k]));
    }

    //========================================================================== the note
    void paintNote (juce::Graphics& g)
    {
        namespace L = Console::Look;
        const auto n = SqHear::nearestNote (freq);
        panel (g, noteArea, "THE NOTE", "Type a note (A4, C#3, Bb2) or a frequency. The colour is the note; lighter is higher.");
        auto sw = juce::Rectangle<int> (noteArea.getX() + 14, noteArea.getY() + 34, 84, 84).toFloat();
        g.setColour (colourOf (n.midi)); g.fillRoundedRectangle (sw, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawRoundedRectangle (sw.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (L::text()); g.setFont (L::caps (34.0f)); g.drawText (n.name, noteArea.getX() + 112, noteArea.getY() + 30, 130, 44, juce::Justification::centredLeft);
        g.setFont (Theme::font (15.0f)); g.setColour (std::abs (n.cents) < 5.0 ? L::green() : L::yellow());
        g.drawText (cents (n.cents), noteArea.getX() + 112, noteArea.getY() + 76, 130, 20, juce::Justification::centredLeft);
        g.setColour (L::dim()); g.setFont (Theme::font (12.0f)); g.drawText ("note number " + juce::String (n.midi) + "   " + SqHear::noteColour (n.midi).hex(), noteArea.getX() + 112, noteArea.getY() + 98, 190, 16, juce::Justification::centredLeft);

        const int x = noteArea.getX() + 300, w = noteArea.getWidth() - 300 - 14; int y = noteArea.getY() + 50;
        auto row = [&] (const juce::String& label, const juce::String& value)
        {
            g.setColour (L::dim()); g.setFont (Theme::font (12.0f)); g.drawText (label, x, y, w / 2 + 30, 17, juce::Justification::centredLeft);
            g.setColour (L::text()); g.setFont (Theme::font (13.0f, true)); g.drawText (value, x + w / 2 - 10, y, w / 2 + 10, 17, juce::Justification::centredRight);
            y += 18;
        };
        row ("Frequency", text2 (freq) + " Hz");
        row ("Critical band (Bark)", juce::String (SqHear::bark (freq), 2));
        row ("ERB number", juce::String (SqHear::erbNumber (freq), 2));
        row ("The ear's filter here is", juce::String (juce::roundToInt (SqHear::erbWidth (freq))) + " Hz wide");
    }

    //========================================================================== harmonics
    void paintHarmonics (juce::Graphics& g)
    {
        namespace L = Console::Look;
        panel (g, harmArea, "HARMONIC SERIES", "The overtones of this note: which note each one lands on, how many cents off, and its ratio.");
        const auto h = SqHear::harmonics (freq, 16);
        auto r = harmArea.reduced (14, 0).withTrimmedTop (32).withTrimmedBottom (30);
        const float cw = (float) r.getWidth() / 16.0f; const float barMax = (float) r.getHeight() - 50.0f;
        for (int i = 0; i < 16; ++i)
        {
            const auto& p = h[(size_t) i]; const float x = (float) r.getX() + cw * (float) i;
            const float bh = juce::jmax (8.0f, barMax / std::sqrt ((float) p.number));           // overtones fall away
            auto bar = juce::Rectangle<float> (x + 3.0f, (float) r.getY() + barMax - bh, cw - 6.0f, bh);
            g.setColour (colourOf (p.note.midi)); g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (L::text()); g.setFont (Theme::font (11.5f, true)); g.drawText (p.note.name, (int) x, r.getY() + (int) barMax + 3, (int) cw, 14, juce::Justification::centred);
            g.setColour (std::abs (p.note.cents) < 5.0 ? L::green() : std::abs (p.note.cents) < 20.0 ? L::yellow() : L::orange()); g.setFont (Theme::font (10.0f));
            g.drawText ((p.note.cents >= 0 ? "+" : "") + juce::String (juce::roundToInt (p.note.cents)), (int) x, r.getY() + (int) barMax + 17, (int) cw, 12, juce::Justification::centred);
            g.setColour (L::dim()); g.drawText (juce::String (p.ratioNum) + ":" + juce::String (p.ratioDen), (int) x, r.getY() + (int) barMax + 30, (int) cw, 12, juce::Justification::centred);
            g.setColour (L::well()); g.setFont (Theme::font (10.0f, true)); if (bh > 16.0f) g.drawText (juce::String (p.number), bar.withHeight (14.0f), juce::Justification::centred);
        }
    }

    //========================================================================== beats
    void paintBeats (juce::Graphics& g)
    {
        namespace L = Console::Look;
        panel (g, beatArea, "BEATS WITH A SECOND TONE", "Two close tones pulse at the difference of their frequencies.");
        const double b = SqHear::beatHz (freq, second);
        g.setColour (L::dim()); g.setFont (Theme::font (12.0f)); g.drawText ("Hz", secondEdit.getRight() + 6, secondEdit.getY(), 30, 26, juce::Justification::centredLeft);
        g.setColour (L::text()); g.setFont (L::caps (22.0f));
        g.drawText (juce::String (b, b < 10.0 ? 2 : 1) + " per second", beatArea.getX() + 14, beatArea.getY() + 78, beatArea.getWidth() - 28, 28, juce::Justification::centredLeft);
        g.setColour (L::dim()); g.setFont (Theme::font (12.0f));
        const juce::String what = b < 0.05 ? "In tune: no beating." : b < 15.0 ? "A slow pulse. Tune until it stops." : b < SqHear::erbWidth (0.5 * (freq + second)) ? "Too fast to count: heard as roughness." : "Far enough apart to be heard as two notes.";
        g.drawFittedText (what, beatArea.getX() + 14, beatArea.getY() + 106, beatArea.getWidth() - 28, 30, juce::Justification::topLeft, 2, 0.9f);
    }

    //========================================================================== room
    void paintRoom (juce::Graphics& g)
    {
        namespace L = Console::Look;
        panel (g, roomArea, "ROOM MODES", "Bass at these frequencies booms or vanishes depending on where you sit.");
        g.setColour (L::dim()); g.setFont (Theme::font (11.5f));
        g.drawText ("L x W x H (m)", heiEdit.getRight() + 10, heiEdit.getY(), roomArea.getRight() - heiEdit.getRight() - 20, 26, juce::Justification::centredLeft);
        const auto m = SqHear::roomModes (roomL, roomW, roomH, 200.0);
        const int x = roomArea.getX() + 14, y = roomArea.getY() + 78, colW = (roomArea.getWidth() - 28) / 2;
        const int rows = juce::jmax (1, (roomArea.getBottom() - 38 - y) / 17);
        for (int i = 0; i < (int) m.size() && i < rows * 2; ++i)
        {
            const auto& mode = m[(size_t) i]; const auto n = SqHear::nearestNote (mode.hz);
            const int cx = x + (i / rows) * colW, cy = y + (i % rows) * 17;
            g.setColour (colourOf (n.midi)); g.fillRoundedRectangle ((float) cx, (float) cy + 3.0f, 10.0f, 10.0f, 2.0f);
            g.setColour (L::text()); g.setFont (Theme::font (12.0f, true)); g.drawText (juce::String (mode.hz, 1) + " Hz", cx + 16, cy, 60, 16, juce::Justification::centredLeft);
            g.setColour (L::dim()); g.setFont (Theme::font (11.5f)); g.drawText (n.name + "  " + (juce::String (mode.kind) == "tangential" ? juce::String ("tangent.") : juce::String (mode.kind)), cx + 78, cy, colW - 80, 16, juce::Justification::centredLeft);
        }
    }

    juce::TextEditor noteEdit, secondEdit, lenEdit, widEdit, heiEdit;
    juce::TextButton visionBtn;
    juce::Rectangle<int> header, wheelArea, noteArea, harmArea, beatArea, roomArea;
    double freq = 440.0, second = 443.0, roomL = 5.0, roomW = 4.0, roomH = 2.5;
    SqHear::Vision vision = SqHear::Vision::normal;
};
