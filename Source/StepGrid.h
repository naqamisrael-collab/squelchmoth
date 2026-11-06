#pragma once
// Squelchmoth Beat Maker - the Beat Lab Channel Rack (see BeatLab.h for the model).
//
//   Grid:   click = add a step (drag to paint)    right-click / right-drag = erase    Alt-click = accent
//           double-click a step = roll (2, 3, 4, 6, 8 hits - trap hats)
//   Rows:   click a name to hear it and edit its lanes; right-click a name for the row tools
//           (fill every 2/4/8, Euclidean, rotate, dice, humanise, length, swing amount, copy / paste, clear)
//           L = lock (kept when the Beat Starter rolls), M = mute, S = solo, level and pan per sound
//   Lanes:  velocity, gate, chance, shift (micro-timing) and repeat for the selected row - drag to draw,
//           right-drag for a straight line, Alt-click resets a step
//   Top:    patterns A-H, the chain ("A A B A"), play the chain or loop the pattern you are editing,
//           steps, resolution, swing, kit; Beat Starter genre + dice; copy / paste / double / clear; to Piano Roll
// Everything is undoable (Cmd+Z here or in the Edit window) and saves with the session.

#include "Behaviours.h"
#include "BeatLab.h"

class StepGridComponent : public juce::Component,
                          private juce::Timer
{
    struct PatternButton : public juce::TextButton
    {
        using juce::TextButton::TextButton;
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
            juce::TextButton::mouseDown (e);
        }
    };

public:
    explicit StepGridComponent (te::StepClip& c) : clip (c)
    {
        setWantsKeyboardFocus (true);
        BeatLab::ensurePatterns (clip, 1);

        // ---- patterns and chain
        for (int i = 0; i < BeatLab::maxPatterns; ++i)
        {
            auto b = std::make_unique<PatternButton> (BeatLab::patternName (i));
            b->setClickingTogglesState (false);
            b->onRightClick = [this, i] { showPatternMenu (i); };
            b->setTooltip ("Edit pattern " + BeatLab::patternName (i) + " (right-click: copy / paste / clear / add to chain)");
            b->onClick = [this, i] { selectPattern (i); };
            addAndMakeVisible (*b);
            patternButtons.push_back (std::move (b));
        }
        chainBox.setText (BeatLab::chainText (clip), juce::dontSendNotification);
        chainBox.setTooltip ("Pattern chain: the order the patterns play in, e.g. A A B A. It repeats to fill the clip.");
        chainBox.setInputRestrictions (64, "ABCDEFGHabcdefgh ,");
        chainBox.onReturnKey = chainBox.onFocusLost = [this] { BeatLab::setChain (clip, chainBox.getText()); chainBox.setText (BeatLab::chainText (clip), juce::dontSendNotification); repaint(); };
        addAndMakeVisible (chainBox);
        chainLabel.setText ("Chain", juce::dontSendNotification);
        addAndMakeVisible (chainLabel);

        playMode.addItem ("Play the chain", 1);
        playMode.addItem ("Loop this pattern", 2);
        playMode.setSelectedId (BeatLab::loopsEditPattern (clip) ? 2 : 1, juce::dontSendNotification);
        playMode.setTooltip ("Play the whole chain, or keep looping the pattern you are editing (FL Studio pattern mode)");
        playMode.onChange = [this] { BeatLab::setLoopPattern (clip, playMode.getSelectedId() == 2); repaint(); };
        addAndMakeVisible (playMode);

        stepsBtn.onClick = [this] { showStepsMenu(); };
        stepsBtn.setTooltip ("Pattern length in steps (1-128)");
        addAndMakeVisible (stepsBtn);

        resBox.addItem ("1/8", 1); resBox.addItem ("1/16", 2); resBox.addItem ("1/32", 3); resBox.addItem ("1/8 T", 4); resBox.addItem ("1/16 T", 5);
        resBox.setTooltip ("Step resolution (how long one step is)");
        resBox.onChange = [this]
        {
            static const double r[] = { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
            BeatLab::setResolution (clip, r[juce::jlimit (0, 4, resBox.getSelectedId() - 1)]);
            repaint();
        };
        addAndMakeVisible (resBox);

        swingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        swingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
        swingSlider.setRange (0.0, 100.0, 1.0);
        swingSlider.setTextValueSuffix ("%");
        swingSlider.setColour (juce::Slider::backgroundColourId, Theme::line());
        swingSlider.setColour (juce::Slider::trackColourId, Theme::acid().withAlpha (0.7f));
        swingSlider.setTooltip ("Swing: delays every second step (0% straight, 100% = hard triplet shuffle). Per-row amount in the row menu.");
        swingSlider.onValueChange = [this] { BeatLab::setSwing (clip, (float) swingSlider.getValue() / 100.0f); };
        addAndMakeVisible (swingSlider);
        swingLabel.setText ("Swing", juce::dontSendNotification);
        addAndMakeVisible (swingLabel);

        for (int i = 0; i < DrumKit::numKits; ++i) kitBox.addItem (DrumKit::kits[i].name, i + 1);
        auto current = clip.getTrack() != nullptr ? clip.getTrack()->state.getProperty ("sqmKit").toString() : juce::String();
        kitBox.setSelectedId (juce::jmax (0, DrumKit::kitIndexFor (current)) + 1, juce::dontSendNotification);
        kitBox.setTooltip ("Sound kit for this track (keeps the pattern)");
        kitBox.onChange = [this] { if (onKitChange) onKitChange (kitBox.getSelectedId() - 1); refreshRows(); };
        addAndMakeVisible (kitBox);

        // ---- Beat Starter
        starterLabel.setText ("Beat Starter", juce::dontSendNotification);
        addAndMakeVisible (starterLabel);
        auto& gs = BeatLab::genres();
        for (int i = 0; i < (int) gs.size(); ++i) genreBox.addItem (gs[(size_t) i].name, i + 1);
        auto g = BeatLab::genreIndex (clip.state.getProperty ("sqmGenre").toString());
        genreBox.setSelectedId (g >= 0 ? g + 1 : 1, juce::dontSendNotification);
        genreBox.setTooltip ("Genre for the Beat Starter");
        addAndMakeVisible (genreBox);
        diceBtn.setTooltip ("Roll a new groove in this genre (locked rows are kept). Right-click a row name to re-roll just that row.");
        diceBtn.onClick = [this] { roll (-1); };
        addAndMakeVisible (diceBtn);
        tempoToggle.setTooltip ("Also set the session tempo to the genre's usual BPM");
        addAndMakeVisible (tempoToggle);

        copyBtn.onClick   = [this] { BeatLab::copyPattern (clip); };
        pasteBtn.onClick  = [this] { if (BeatLab::paste (clip, -1)) { refreshTop(); repaint(); } };
        doubleBtn.onClick = [this] { BeatLab::doublePattern (clip); refreshTop(); repaint(); };
        clearBtn.onClick  = [this] { auto p = BeatLab::pattern (clip); for (int ch = 0; ch < clip.getChannels().size(); ++ch) if (! BeatLab::isLocked (clip, ch)) BeatLab::clearRow (clip, p, ch); repaint(); };
        pianoBtn.onClick  = [this] { sendToPianoRoll(); };
        copyBtn.setTooltip ("Copy this pattern");
        pasteBtn.setTooltip ("Paste the copied pattern here");
        doubleBtn.setTooltip ("Double the pattern length by repeating it");
        clearBtn.setTooltip ("Clear this pattern (locked rows are kept)");
        pianoBtn.setTooltip ("Turn the beat into a MIDI clip and open it in the piano roll (undo brings the steps back)");
        for (auto* b : { &copyBtn, &pasteBtn, &doubleBtn, &clearBtn, &pianoBtn }) addAndMakeVisible (b);

        // ---- lanes
        for (int i = 0; i < 5; ++i)
        {
            auto b = std::make_unique<juce::TextButton> (BeatLab::laneName ((BeatLab::Lane) i));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (771);
            b->onClick = [this, i] { lane = (BeatLab::Lane) i; repaint(); };
            addAndMakeVisible (*b);
            laneButtons.push_back (std::move (b));
        }
        laneButtons[0]->setToggleState (true, juce::dontSendNotification);

        hint.setText ("click = step, drag = paint | right-click = erase | Alt-click = accent | double-click = roll | click a name to hear it, right-click for row tools",
                      juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, Theme::dim());
        hint.setFont (Theme::font (11.0f));
        addAndMakeVisible (hint);
        for (auto* l : { &chainLabel, &swingLabel, &starterLabel })
        {
            l->setColour (juce::Label::textColourId, Theme::dim());
            l->setFont (Theme::font (11.0f, true));
        }

        refreshRows();
        refreshTop();
        setSize (1240, topH + headH + rowH * juce::jmax (1, clip.getChannels().size()) + laneH + 34);
        startTimerHz (30);
    }

    static constexpr int labelW = 268, topH = 82, headH = 20, rowH = 34, laneH = 150;

    te::StepClip& clip;
    std::function<void (int)> onKitChange;
    std::function<void (te::MidiClip&)> onOpenPianoRoll;

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg());
        g.setColour (Theme::panel());
        g.fillRect (getLocalBounds().removeFromTop (topH));
        g.setColour (Theme::line());
        g.drawHorizontalLine (topH - 1, 0.0f, (float) getWidth());

        auto p = BeatLab::pattern (clip);
        auto chans = clip.getChannels();
        const int n = juce::jmax (1, p.getNumNotes());
        auto area = gridArea();
        const float cw = area.getWidth() / (float) n;
        const int playing = playingStep();

        // step numbers
        g.setFont (Theme::font (9.5f));
        for (int i = 0; i < n; ++i)
        {
            auto x = area.getX() + i * cw;
            bool beat = (i % 4) == 0;
            g.setColour (i == playing ? Theme::acid() : (beat ? Theme::text() : Theme::dim()));
            if (beat || cw >= 18.0f) g.drawText (juce::String (i + 1), juce::Rectangle<float> (x, (float) topH + 3, cw, (float) headH - 4), juce::Justification::centred);
        }

        bool anySolo = false;
        for (int ch = 0; ch < chans.size(); ++ch) anySolo = anySolo || BeatLab::isSoloed (clip, ch);

        for (int ch = 0; ch < chans.size(); ++ch)
        {
            const int y = area.getY() + ch * rowH;
            const bool sel = ch == selRow;
            const bool silent = BeatLab::isMuted (clip, ch) || (anySolo && ! BeatLab::isSoloed (clip, ch));
            g.setColour (sel ? Theme::panel2() : (ch % 2 ? Theme::panel() : Theme::panel().darker (0.15f)));
            g.fillRect (0, y, getWidth(), rowH);
            if (sel) { g.setColour (Theme::acid()); g.fillRect (0, y, 3, rowH); }

            // row header: lock / mute / solo lamps, name
            auto hdr = juce::Rectangle<int> (6, y, labelW - 12, rowH);
            auto lamp = [&] (int idx, const char* t, bool on, juce::Colour c)
            {
                auto r = lampRect (ch, idx).toFloat();
                g.setColour (on ? c : Theme::line());
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (on ? Theme::bg() : Theme::dim());
                g.setFont (Theme::font (10.0f, true));
                g.drawText (t, r, juce::Justification::centred);
            };
            lamp (0, "L", BeatLab::isLocked (clip, ch), Theme::violet().brighter (0.4f));
            lamp (1, "M", BeatLab::isMuted (clip, ch), Theme::amber());
            lamp (2, "S", BeatLab::isSoloed (clip, ch), Theme::mint());
            g.setColour (silent ? Theme::dim() : (sel ? Theme::acid() : Theme::text()));
            g.setFont (Theme::font (12.5f, true));
            auto nameR = hdr.withTrimmedLeft (78).withWidth (110);
            g.drawFittedText (chans[ch]->name.get(), nameR, juce::Justification::centredLeft, 1);
            const int len = BeatLab::rowLength (p, ch);
            if (len != n)
            {
                g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f));
                g.drawText (juce::String (len) + " st", nameR.withTrimmedLeft (nameR.getWidth() - 34), juce::Justification::centredRight);
            }

            // steps
            for (int i = 0; i < n; ++i)
            {
                juce::Rectangle<float> cell (area.getX() + i * cw + 1.5f, (float) y + 4.0f, cw - 3.0f, rowH - 8.0f);
                const bool group = ((i / 4) % 2) == 0;
                const bool beyond = i >= len;
                g.setColour (group ? Theme::line().brighter (0.12f) : Theme::line());
                g.fillRoundedRectangle (cell, 3.0f);
                if (p.getNote (ch, i) && ! beyond)
                {
                    const int vel = p.getVelocity (ch, i);
                    auto c = vel >= 120 ? Theme::magenta() : Theme::acid();
                    c = c.withAlpha (vel < 70 ? 0.55f : 0.95f);
                    if (silent) c = c.withAlpha (0.3f);
                    // velocity shown as fill height
                    auto fill = cell.withTrimmedTop (cell.getHeight() * (1.0f - juce::jlimit (0.25f, 1.0f, vel / 127.0f)));
                    g.setColour (c.withAlpha (c.getFloatAlpha() * 0.35f));
                    g.fillRoundedRectangle (cell, 3.0f);
                    g.setColour (c);
                    g.fillRoundedRectangle (fill, 3.0f);
                    const int rep = BeatLab::getRepeat (p, ch, i);
                    if (rep > 1)
                    {
                        g.setColour (Theme::bg().withAlpha (0.8f));
                        for (int k = 1; k < rep; ++k) g.drawVerticalLine ((int) (cell.getX() + cell.getWidth() * k / rep), cell.getY() + 2, cell.getBottom() - 2);
                    }
                    const float prob = p.getProbability (ch, i);
                    if (prob < 0.999f)
                    {
                        g.setColour (Theme::bg());
                        g.setFont (Theme::font (8.5f, true));
                        if (cw > 20) g.drawText (juce::String (juce::roundToInt (prob * 100)) + "%", cell, juce::Justification::centredTop);
                        else g.drawText ("?", cell, juce::Justification::centredTop);
                    }
                    const float sh = BeatLab::getShift (p, ch, i);
                    if (std::abs (sh) > 0.001f)
                    {
                        auto mx = cell.getCentreX() + sh * cell.getWidth();
                        juce::Path tri; tri.addTriangle (mx - 3.5f, cell.getBottom(), mx + 3.5f, cell.getBottom(), mx, cell.getBottom() - 5.0f);
                        g.setColour (Theme::bg()); g.fillPath (tri);
                    }
                }
                if (beyond) { g.setColour (Theme::bg().withAlpha (0.6f)); g.fillRoundedRectangle (cell, 3.0f); }
            }

            // playhead (each row follows its own length)
            if (playing >= 0)
            {
                const int s = len > 0 ? playingGlobal % len : playing;
                if (s >= 0 && s < n)
                {
                    g.setColour (Theme::text().withAlpha (0.18f));
                    g.fillRect (juce::Rectangle<float> (area.getX() + s * cw, (float) y, cw, (float) rowH));
                }
            }
        }

        paintLanes (g, p);
    }

    void resized() override
    {
        auto r = getLocalBounds().removeFromTop (topH).reduced (10, 6);
        auto row1 = r.removeFromTop (30);
        r.removeFromTop (6);
        auto row2 = r.removeFromTop (30);

        for (auto& b : patternButtons) { b->setBounds (row1.removeFromLeft (30).reduced (1, 2)); }
        row1.removeFromLeft (10);
        chainLabel.setBounds (row1.removeFromLeft (40));
        chainBox.setBounds (row1.removeFromLeft (140).reduced (0, 3));
        row1.removeFromLeft (6);
        playMode.setBounds (row1.removeFromLeft (150).reduced (0, 2));
        row1.removeFromLeft (10);
        stepsBtn.setBounds (row1.removeFromLeft (84).reduced (0, 2));
        row1.removeFromLeft (6);
        resBox.setBounds (row1.removeFromLeft (76).reduced (0, 2));
        row1.removeFromLeft (10);
        swingLabel.setBounds (row1.removeFromLeft (42));
        swingSlider.setBounds (row1.removeFromLeft (150));
        row1.removeFromLeft (10);
        kitBox.setBounds (row1.removeFromLeft (juce::jmin (170, juce::jmax (110, row1.getWidth()))).reduced (0, 2));

        starterLabel.setBounds (row2.removeFromLeft (84));
        genreBox.setBounds (row2.removeFromLeft (150).reduced (0, 2));
        row2.removeFromLeft (6);
        diceBtn.setBounds (row2.removeFromLeft (100).reduced (0, 2));
        row2.removeFromLeft (6);
        tempoToggle.setBounds (row2.removeFromLeft (100));
        row2.removeFromLeft (16);
        for (auto* b : { &copyBtn, &pasteBtn, &doubleBtn, &clearBtn }) { b->setBounds (row2.removeFromLeft (70).reduced (0, 2)); row2.removeFromLeft (4); }
        row2.removeFromLeft (10);
        pianoBtn.setBounds (row2.removeFromLeft (130).reduced (0, 2));

        auto area = gridArea();
        for (int ch = 0; ch < (int) levels.size(); ++ch)
        {
            const int y = area.getY() + ch * rowH;
            levels[(size_t) ch]->setBounds (196, y + 8, 40, rowH - 16);
            pans[(size_t) ch]->setBounds (238, y + 2, 28, rowH - 4);
        }

        auto lanes = laneArea();
        auto tabs = lanes.removeFromTop (24).withTrimmedLeft (10);
        for (auto& b : laneButtons) { b->setBounds (tabs.removeFromLeft (78).reduced (1, 1)); }
        hint.setBounds (getLocalBounds().removeFromBottom (24).reduced (10, 2));
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        dragMode = DragMode::none;
        auto pt = e.position;
        auto p = BeatLab::pattern (clip);

        // row header
        for (int ch = 0; ch < clip.getChannels().size(); ++ch)
            for (int k = 0; k < 3; ++k)
                if (lampRect (ch, k).contains (pt.toInt()))
                {
                    const char* props[] = { "sqmLock", "sqmMute", "sqmSolo" };
                    BeatLab::setRowFlag (clip, ch, props[k], ! BeatLab::rowFlag (clip, ch, props[k]));
                    repaint();
                    return;
                }
        if (pt.x < labelW && gridArea().contains (juce::Point<int> (labelW + 1, (int) pt.y)))
        {
            const int ch = (int) ((pt.y - gridArea().getY()) / rowH);
            if (ch < clip.getChannels().size())
            {
                selRow = ch;
                if (e.mods.isPopupMenu()) showRowMenu (ch);
                else audition (ch);
                repaint();
            }
            return;
        }

        // grid
        auto [ch, step] = cellAt (pt);
        if (ch >= 0)
        {
            selRow = ch;
            if (e.mods.isAltDown())
            {
                if (! p.getNote (ch, step)) BeatLab::setStep (clip, p, ch, step, true, 127);
                else p.setVelocity (ch, step, p.getVelocity (ch, step) >= 120 ? 100 : 127);
            }
            else if (e.mods.isPopupMenu())
            {
                dragMode = DragMode::erase;
                if (p.getNote (ch, step)) BeatLab::setStep (clip, p, ch, step, false);
            }
            else
            {
                dragMode = p.getNote (ch, step) ? DragMode::erase : DragMode::paint;
                BeatLab::setStep (clip, p, ch, step, dragMode == DragMode::paint, 100);
                if (dragMode == DragMode::paint) audition (ch);
            }
            dragRow = ch;
            repaint();
            return;
        }

        // lanes
        if (laneArea().withTrimmedTop (24).contains (pt.toInt()))
        {
            dragMode = e.mods.isPopupMenu() ? DragMode::laneLine : DragMode::lane;
            lineStart = pt;
            applyLane (pt, e.mods.isAltDown());
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto p = BeatLab::pattern (clip);
        if (dragMode == DragMode::paint || dragMode == DragMode::erase)
        {
            auto [ch, step] = cellAt ({ e.position.x, (float) (gridArea().getY() + dragRow * rowH + rowH / 2) });
            if (ch >= 0 && p.getNote (ch, step) != (dragMode == DragMode::paint))
            {
                BeatLab::setStep (clip, p, ch, step, dragMode == DragMode::paint, 100);
                repaint();
            }
        }
        else if (dragMode == DragMode::lane)
        {
            applyLane (e.position, e.mods.isAltDown());
            repaint();
        }
        else if (dragMode == DragMode::laneLine)
        {
            lineEnd = e.position;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragMode == DragMode::laneLine) applyLaneLine();
        dragMode = DragMode::none;
        lineEnd = {};
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto [ch, step] = cellAt (e.position);
        if (ch < 0 || e.mods.isPopupMenu()) return;
        auto p = BeatLab::pattern (clip);
        static const int cycle[] = { 1, 2, 3, 4, 6, 8 };
        if (! p.getNote (ch, step)) BeatLab::setStep (clip, p, ch, step, true, 92);
        const int cur = BeatLab::getRepeat (p, ch, step);
        int next = 2;
        for (int k = 0; k < 6; ++k) if (cycle[k] == cur) next = cycle[(k + 1) % 6];
        BeatLab::setRepeat (clip, p, ch, step, next);
        repaint();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        auto& undo = clip.edit.getUndoManager();
        if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))                                  { undo.undo(); refreshTop(); repaint(); return true; }
        if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { undo.redo(); refreshTop(); repaint(); return true; }
        if (k == juce::KeyPress::spaceKey)
        {
            auto& t = clip.edit.getTransport();
            if (t.isPlaying()) t.stop (false, false); else t.play (false);
            return true;
        }
        if (k.getTextCharacter() >= 'a' && k.getTextCharacter() <= 'h' && ! k.getModifiers().isCommandDown()) { selectPattern (k.getTextCharacter() - 'a'); return true; }
        if (k == juce::KeyPress::upKey)   { selRow = juce::jmax (0, selRow - 1); repaint(); return true; }
        if (k == juce::KeyPress::downKey) { selRow = juce::jmin (clip.getChannels().size() - 1, selRow + 1); repaint(); return true; }
        return false;
    }

private:
    enum class DragMode { none, paint, erase, lane, laneLine };
    DragMode dragMode = DragMode::none;
    int dragRow = 0, selRow = 0, playingGlobal = -1;
    BeatLab::Lane lane = BeatLab::Lane::velocity;
    juce::Point<float> lineStart, lineEnd;
    juce::Random rnd;

    std::vector<std::unique_ptr<PatternButton>> patternButtons;
    std::vector<std::unique_ptr<juce::TextButton>> laneButtons;
    juce::TextEditor chainBox;
    juce::ComboBox playMode, resBox, kitBox, genreBox;
    juce::Slider swingSlider;
    juce::Label chainLabel, swingLabel, starterLabel, hint;
    juce::TextButton stepsBtn { "16 steps" }, diceBtn { "Roll beat" }, copyBtn { "Copy" }, pasteBtn { "Paste" },
                     doubleBtn { "Double" }, clearBtn { "Clear" }, pianoBtn { "To Piano Roll" };
    juce::ToggleButton tempoToggle { "Set tempo" };
    std::vector<std::unique_ptr<juce::Slider>> levels, pans;

    juce::Rectangle<int> gridArea() const   { return { labelW, topH + headH, getWidth() - labelW - 12, clip.getChannels().size() * rowH }; }
    juce::Rectangle<int> laneArea() const   { auto g = gridArea(); return { 0, g.getBottom() + 6, getWidth(), laneH }; }
    juce::Rectangle<int> lampRect (int ch, int idx) const { return { 10 + idx * 22, gridArea().getY() + ch * rowH + 8, 19, rowH - 16 }; }

    std::pair<int, int> cellAt (juce::Point<float> pt) const
    {
        auto area = gridArea().toFloat();
        if (! area.contains (pt)) return { -1, -1 };
        int n = clip.getPattern (BeatLab::editPattern (clip)).getNumNotes();
        int step = (int) ((pt.x - area.getX()) / (area.getWidth() / juce::jmax (1, n)));
        int ch = (int) ((pt.y - area.getY()) / rowH);
        if (ch >= clip.getChannels().size() || step >= n || step < 0) return { -1, -1 };
        return { ch, step };
    }

    te::SamplerPlugin* sampler() const
    {
        if (auto t = dynamic_cast<te::AudioTrack*> (clip.getTrack())) return t->pluginList.findFirstPluginOfType<te::SamplerPlugin>();
        return nullptr;
    }

    void refreshRows()
    {
        for (auto& s : levels) removeChildComponent (s.get());
        for (auto& s : pans) removeChildComponent (s.get());
        levels.clear(); pans.clear();
        auto* smp = sampler();
        for (int ch = 0; ch < clip.getChannels().size(); ++ch)
        {
            auto lv = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            lv->setRange (-30.0, 6.0, 0.1);
            lv->setDoubleClickReturnValue (true, 0.0);
            lv->setTooltip ("Level of this sound (dB)");
            lv->setColour (juce::Slider::backgroundColourId, Theme::line());
            auto pn = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            pn->setRange (-1.0, 1.0, 0.01);
            pn->setDoubleClickReturnValue (true, 0.0);
            pn->setTooltip ("Pan of this sound");
            const int idx = soundIndexFor (ch);
            if (smp != nullptr && idx >= 0)
            {
                lv->setValue (smp->getSoundGainDb (idx), juce::dontSendNotification);
                pn->setValue (smp->getSoundPan (idx), juce::dontSendNotification);
            }
            auto apply = [this, ch, l = lv.get(), pp = pn.get()]
            {
                if (auto* s = sampler()) { auto i = soundIndexFor (ch); if (i >= 0) s->setSoundGains (i, (float) l->getValue(), (float) pp->getValue()); }
            };
            lv->onValueChange = apply;
            pn->onValueChange = apply;
            addAndMakeVisible (*lv);
            addAndMakeVisible (*pn);
            levels.push_back (std::move (lv));
            pans.push_back (std::move (pn));
        }
        resized();
    }

    /** The Sampler sound that plays this row (matched by note number). */
    int soundIndexFor (int ch) const
    {
        auto* s = sampler();
        auto* c = BeatLab::channel (clip, ch);
        if (s == nullptr || c == nullptr) return -1;
        for (int i = 0; i < s->getNumSounds(); ++i) if (s->getKeyNote (i) == c->noteNumber.get()) return i;
        return ch < s->getNumSounds() ? ch : -1;
    }

    void refreshTop()
    {
        auto p = BeatLab::pattern (clip);
        stepsBtn.setButtonText (juce::String (p.getNumNotes()) + " steps");
        const auto nl = p.getNoteLength().inBeats();
        int res = 2;
        if (std::abs (nl - 0.5) < 1e-4) res = 1; else if (std::abs (nl - 0.125) < 1e-4) res = 3;
        else if (std::abs (nl - 1.0 / 3.0) < 1e-4) res = 4; else if (std::abs (nl - 1.0 / 6.0) < 1e-4) res = 5;
        resBox.setSelectedId (res, juce::dontSendNotification);
        swingSlider.setValue (BeatLab::swing (clip) * 100.0, juce::dontSendNotification);
        chainBox.setText (BeatLab::chainText (clip), juce::dontSendNotification);
        for (int i = 0; i < (int) patternButtons.size(); ++i)
        {
            const bool cur = i == BeatLab::editPattern (clip);
            const bool used = BeatLab::patternHasNotes (clip, i);
            auto& b = *patternButtons[(size_t) i];
            b.setColour (juce::TextButton::buttonColourId, cur ? Theme::acid() : (used ? Theme::panel2().brighter (0.2f) : Theme::panel2()));
            b.setColour (juce::TextButton::textColourOffId, cur ? Theme::bg() : (used ? Theme::text() : Theme::dim()));
        }
    }

    void selectPattern (int i)
    {
        BeatLab::setEditPattern (clip, i);
        if (BeatLab::loopsEditPattern (clip)) BeatLab::rebuildSequence (clip);
        refreshTop();
        repaint();
    }

    void showPatternMenu (int i)
    {
        juce::PopupMenu m;
        m.addItem (1, "Copy pattern " + BeatLab::patternName (i));
        m.addItem (2, "Paste into pattern " + BeatLab::patternName (i), ! BeatLab::clipboard().rows.empty());
        m.addItem (3, "Clear pattern " + BeatLab::patternName (i));
        m.addItem (4, "Add " + BeatLab::patternName (i) + " to the end of the chain");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (patternButtons[(size_t) i].get()), [this, i] (int r)
        {
            if (r == 0) return;
            const int prev = BeatLab::editPattern (clip);
            BeatLab::setEditPattern (clip, i);
            if (r == 1) BeatLab::copyPattern (clip);
            if (r == 2) BeatLab::paste (clip, -1);
            if (r == 3) { auto p = BeatLab::pattern (clip); for (int ch = 0; ch < clip.getChannels().size(); ++ch) BeatLab::clearRow (clip, p, ch); }
            if (r == 4) BeatLab::setChain (clip, BeatLab::chainText (clip) + " " + BeatLab::patternName (i));
            if (r == 1) BeatLab::setEditPattern (clip, prev);
            refreshTop();
            repaint();
        });
    }

    void showStepsMenu()
    {
        juce::PopupMenu m;
        auto cur = BeatLab::pattern (clip).getNumNotes();
        for (int n : { 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128 }) m.addItem (n, juce::String (n) + " steps", true, cur == n);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (stepsBtn), [this] (int r)
        {
            if (r > 0) { BeatLab::setNumSteps (clip, r); refreshTop(); repaint(); }
        });
    }

    void showRowMenu (int ch)
    {
        auto p = BeatLab::pattern (clip);
        juce::PopupMenu m, fill, euc, len, swing;
        for (int n : { 2, 3, 4, 8, 16 }) fill.addItem (100 + n, "Every " + juce::String (n) + " steps");
        fill.addItem (130, "Every 4 steps, offset 2 (offbeats)");
        for (auto [h, s] : { std::pair<int, int> { 3, 8 }, { 5, 8 }, { 3, 16 }, { 5, 16 }, { 7, 16 }, { 9, 16 }, { 11, 16 }, { 13, 16 } })
            euc.addItem (200 + h * 100 + s, juce::String (h) + " hits over " + juce::String (s) + " steps");
        for (int n : { 3, 5, 6, 7, 9, 10, 12, 14, 15 }) len.addItem (300 + n, juce::String (n) + " steps (polyrhythm)", true, BeatLab::rowLength (p, ch) == n);
        len.addItem (300, "Same as the pattern", true, BeatLab::rowLength (p, ch) == p.getNumNotes());
        for (int v : { 0, 25, 50, 75, 100 }) swing.addItem (400 + v, juce::String (v) + "% of the pattern swing", true, juce::roundToInt (BeatLab::swingMix (clip, ch) * 100) == v);

        m.addSectionHeader (clip.getChannels()[ch]->name.get());
        m.addItem (1, "Hear it");
        m.addItem (2, "Re-roll this row (" + genreBox.getText() + ")");
        m.addSubMenu ("Fill", fill);
        m.addSubMenu ("Euclidean", euc);
        m.addItem (3, "Rotate left");
        m.addItem (4, "Rotate right");
        m.addItem (5, "Dice (random)");
        m.addItem (6, "Humanise (timing and velocity)");
        m.addSeparator();
        m.addSubMenu ("Row length", len);
        m.addSubMenu ("Swing amount", swing);
        m.addSeparator();
        m.addItem (7, "Copy row");
        m.addItem (8, "Paste row", ! BeatLab::clipboard().rows.empty());
        m.addItem (9, "Clear row");
        m.showMenuAsync (juce::PopupMenu::Options(), [this, ch] (int r)
        {
            if (r == 0) return;
            auto pp = BeatLab::pattern (clip);
            if (r == 1) audition (ch);
            else if (r == 2) BeatLab::starter (clip, genreBox.getSelectedId() - 1, rnd, ch);
            else if (r == 3) BeatLab::rotateRow (clip, pp, ch, -1);
            else if (r == 4) BeatLab::rotateRow (clip, pp, ch, 1);
            else if (r == 5) BeatLab::diceRow (clip, pp, ch, 0.3f, rnd);
            else if (r == 6) BeatLab::humanise (clip, pp, ch, 0.6f, rnd);
            else if (r == 7) BeatLab::copyRow (pp, ch);
            else if (r == 8) BeatLab::paste (clip, ch);
            else if (r == 9) BeatLab::clearRow (clip, pp, ch);
            else if (r == 130) BeatLab::fillEvery (clip, pp, ch, 4, 2);
            else if (r >= 100 && r < 130) BeatLab::fillEvery (clip, pp, ch, r - 100);
            else if (r >= 300 && r < 400) BeatLab::setRowLength (clip, pp, ch, r == 300 ? pp.getNumNotes() : r - 300);
            else if (r >= 400 && r < 500) BeatLab::setSwingMix (clip, ch, (r - 400) / 100.0f);
            else if (r >= 500) BeatLab::fillEuclid (clip, pp, ch, (r - 200) / 100, (r - 200) % 100, 0);
            repaint();
        });
    }

    void roll (int row)
    {
        BeatLab::starter (clip, genreBox.getSelectedId() - 1, rnd, row, tempoToggle.getToggleState());
        refreshTop();
        repaint();
    }

    void audition (int ch)
    {
        auto* s = sampler();
        auto* c = BeatLab::channel (clip, ch);
        if (s == nullptr || c == nullptr) return;
        juce::BigInteger keys;
        keys.setBit (c->noteNumber.get());
        s->playNotes (keys);
        juce::Component::SafePointer<StepGridComponent> safe (this);
        juce::Timer::callAfterDelay (180, [safe] { if (safe != nullptr) if (auto* sp = safe->sampler()) sp->playNotes ({}); });
    }

    void sendToPianoRoll()
    {
        auto cb = onOpenPianoRoll;
        if (auto* mc = BeatLab::sendToPianoRoll (clip))
        {
            te::SafeSelectable<te::MidiClip> safe (mc);
            juce::MessageManager::callAsync ([cb, safe] { if (cb && safe != nullptr) cb (*safe); });
        }
    }

    /** Step under the playhead in the pattern being edited (-1 if another pattern is playing or stopped). */
    int playingStep()
    {
        playingGlobal = -1;
        auto& tr = clip.edit.getTransport();
        if (! tr.isPlaying() || clip.getTrack() == nullptr) return -1;
        auto beat = clip.getContentBeatAtTime (tr.getPosition()).inBeats();
        auto seq = BeatLab::loopsEditPattern (clip) ? juce::Array<int> { BeatLab::editPattern (clip) } : BeatLab::chain (clip);
        double total = 0;
        for (auto i : seq) { auto p = clip.getPattern (juce::jmin (i, clip.getNumPatterns() - 1)); total += p.getNumNotes() * p.getNoteLength().inBeats(); }
        if (total <= 0 || beat < 0) return -1;
        double pos = std::fmod (beat, total);
        for (auto i : seq)
        {
            auto p = clip.getPattern (juce::jmin (i, clip.getNumPatterns() - 1));
            auto len = p.getNumNotes() * p.getNoteLength().inBeats();
            if (pos < len)
            {
                if (i != BeatLab::editPattern (clip)) return -1;
                const int step = (int) (pos / p.getNoteLength().inBeats());
                // rows with their own length keep counting across repeats of the pattern
                const int patternsBefore = (int) std::floor (beat / juce::jmax (1e-6, len));
                playingGlobal = BeatLab::loopsEditPattern (clip) ? patternsBefore * p.getNumNotes() + step : step;
                return step;
            }
            pos -= len;
        }
        return -1;
    }

    //==========================================================================
    void paintLanes (juce::Graphics& g, te::StepClip::Pattern& p)
    {
        auto r = laneArea();
        g.setColour (Theme::panel());
        g.fillRect (r);
        g.setColour (Theme::line());
        g.drawHorizontalLine (r.getY(), 0.0f, (float) getWidth());
        auto chans = clip.getChannels();
        if (! juce::isPositiveAndBelow (selRow, chans.size())) return;

        auto tabs = r.removeFromTop (24);
        g.setColour (Theme::acid());
        g.setFont (Theme::font (12.0f, true));
        g.drawText (chans[selRow]->name.get() + "  -  " + BeatLab::laneName (lane), tabs.withTrimmedLeft (420).withTrimmedRight (10), juce::Justification::centredLeft);

        auto area = gridArea();
        auto lr = juce::Rectangle<float> ((float) area.getX(), (float) r.getY() + 6, (float) area.getWidth(), (float) r.getHeight() - 14);
        const int n = juce::jmax (1, p.getNumNotes());
        const float cw = lr.getWidth() / n;
        const bool centred = lane == BeatLab::Lane::shift;

        g.setColour (Theme::dim());
        g.setFont (Theme::font (10.0f));
        const char* top = lane == BeatLab::Lane::velocity ? "127" : lane == BeatLab::Lane::gate ? "200%" : lane == BeatLab::Lane::chance ? "100%" : lane == BeatLab::Lane::shift ? "late" : "x8";
        const char* bot = lane == BeatLab::Lane::velocity ? "1" : lane == BeatLab::Lane::gate ? "0%" : lane == BeatLab::Lane::chance ? "0%" : lane == BeatLab::Lane::shift ? "early" : "x1";
        g.drawText (top, juce::Rectangle<float> (lr.getX() - 54, lr.getY(), 48, 14), juce::Justification::centredRight);
        g.drawText (bot, juce::Rectangle<float> (lr.getX() - 54, lr.getBottom() - 14, 48, 14), juce::Justification::centredRight);
        g.setColour (Theme::line().withAlpha (0.6f));
        g.drawHorizontalLine ((int) lr.getCentreY(), lr.getX(), lr.getRight());

        for (int i = 0; i < n; ++i)
        {
            auto col = juce::Rectangle<float> (lr.getX() + i * cw + 1.5f, lr.getY(), cw - 3.0f, lr.getHeight());
            g.setColour (((i / 4) % 2) == 0 ? Theme::panel2().withAlpha (0.6f) : Theme::panel2().withAlpha (0.3f));
            g.fillRect (col);
            if (! p.getNote (selRow, i)) continue;
            const float v = BeatLab::laneValue (clip, p, lane, selRow, i);
            juce::Rectangle<float> bar;
            if (centred)
            {
                const float y = col.getY() + col.getHeight() * (1.0f - v);
                bar = juce::Rectangle<float> (col.getX(), juce::jmin (y, col.getCentreY()), col.getWidth(), std::abs (y - col.getCentreY()) + 2.0f);
            }
            else
                bar = col.withTrimmedTop (col.getHeight() * (1.0f - v));
            g.setColour (Theme::acid().withAlpha (0.85f));
            g.fillRect (bar);
            if (cw >= 26.0f)
            {
                g.setColour (Theme::text());
                g.setFont (Theme::font (9.0f));
                g.drawText (BeatLab::laneText (p, lane, selRow, i), col.removeFromBottom (12), juce::Justification::centred);
            }
        }
        if (dragMode == DragMode::laneLine && ! lineEnd.isOrigin())
        {
            g.setColour (Theme::magenta());
            g.drawLine ({ lineStart, lineEnd }, 1.5f);
        }
    }

    void applyLane (juce::Point<float> pt, bool reset)
    {
        auto p = BeatLab::pattern (clip);
        auto area = gridArea();
        auto lr = laneArea().withTrimmedTop (30).withTrimmedBottom (8).toFloat();
        const int n = juce::jmax (1, p.getNumNotes());
        const int i = (int) ((pt.x - area.getX()) / (area.getWidth() / (float) n));
        if (i < 0 || i >= n || ! p.getNote (selRow, i)) return;
        float v = 1.0f - (pt.y - lr.getY()) / lr.getHeight();
        if (reset)
            v = lane == BeatLab::Lane::velocity ? 100.0f / 127.0f : lane == BeatLab::Lane::gate ? 0.5f : lane == BeatLab::Lane::chance ? 1.0f : lane == BeatLab::Lane::shift ? 0.5f : 0.0f;
        BeatLab::setLaneValue (clip, p, lane, selRow, i, v);
    }

    void applyLaneLine()
    {
        if (lineEnd.isOrigin()) return;
        auto a = lineStart, b = lineEnd;
        if (a.x > b.x) std::swap (a, b);
        auto p = BeatLab::pattern (clip);
        auto area = gridArea();
        const int n = juce::jmax (1, p.getNumNotes());
        const float cw = area.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float x = area.getX() + (i + 0.5f) * cw;
            if (x < a.x || x > b.x) continue;
            const float t = (b.x - a.x) < 1.0f ? 0.0f : (x - a.x) / (b.x - a.x);
            applyLane ({ x, a.y + (b.y - a.y) * t }, false);
        }
    }

    void timerCallback() override
    {
        if (clip.getTrack() == nullptr)
        {
            // the clip was removed (sent to the piano roll, deleted, undone)
            if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) { stopTimer(); w->closeButtonPressed(); }
            return;
        }
        if (clip.edit.getTransport().isPlaying()) repaint (gridArea().withX (0).withWidth (getWidth()).withTop (topH));
    }
};

class StepGridWindow : public juce::DocumentWindow
{
public:
    explicit StepGridWindow (te::StepClip& c)
        : DocumentWindow ("Beat Maker - " + (c.getTrack() != nullptr ? c.getTrack()->getName() : juce::String()), Theme::bg(), DocumentWindow::allButtons),
          clipRef (&c)
    {
        setUsingNativeTitleBar (true);
        auto comp = new StepGridComponent (c);
        comp->onKitChange = [this] (int k) { if (onKitChange) onKitChange (k); };
        comp->onOpenPianoRoll = [this] (te::MidiClip& mc) { if (onOpenPianoRoll) onOpenPianoRoll (mc); };
        setContentOwned (comp, true);
        setResizable (true, true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        toFront (true);
    }
    void closeButtonPressed() override  { if (onClose) onClose(); }
    std::function<void()> onClose;
    std::function<void (int)> onKitChange;
    std::function<void (te::MidiClip&)> onOpenPianoRoll;
    te::Clip::Ptr clipRef;
};
