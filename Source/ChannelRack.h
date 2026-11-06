#pragma once
// Squelchmoth 1.8 - the Channel Rack (FL Studio layout, Squelchmoth theme). Model: Rack.h.
//
//   Toolbar : pattern selector (< name >, + new; right-click: rename / colour / clone / delete / select unused),
//             pattern length (steps), steps per beat, swing, PAT / SONG, Graph Editor (Cmd+K), Keyboard Editor,
//             + add channel (instruments, kits, samples, layer, MIDI out), Beat Starter, zip all, Claude.
//   Row     : mute light (Cmd-click solo, Shift-click lock) | pan | volume | mixer target | channel button
//             (left: instrument window, right: channel menu) | selector LED | 16 steps per bar (4 colours of 4),
//             or the piano-roll preview when the channel holds a MIDI clip (click opens the piano roll).
//   Steps   : left-click on, right-click off, drag paints, Shift-drag moves the row, Alt-click accent,
//             Alt-hover auditions, double-click a step opens its roll menu.
//   Graph   : Note, Velocity, Release, Fine Pitch, Pan, Mod X, Mod Y, Shift, Repeat, Chance, Gate - drag to draw,
//             right-drag ramps, Alt-click resets, Ctrl-drag scales the whole lane.
//   Keyboard: the Note lane drawn as a mini keyboard per step; Shift-click adds a chord note (up to 4).
// Every change is undoable and saved with the session; the Edit window shows the same tracks.

#include "Behaviours.h"
#include "Rack.h"
#include "BurnToMidi.h"

/** The Channel Rack look: FL Studio's layout, drawn in Squelchmoth's own palette - soft bevels, glossy steps, glowing LEDs. */
namespace RackLook
{
    inline juce::Colour panelDark()   { return Theme::bg().darker (0.25f); }
    inline juce::Colour rowFace()     { return Theme::panel().brighter (0.06f); }
    inline juce::Colour stepLight()   { return Theme::panel2().brighter (0.75f).interpolatedWith (Theme::amber(), 0.08f); }   // the "4 steps on" tint (FL's lighter pills)
    inline juce::Colour stepDark()    { return Theme::panel2().brighter (0.12f); }   // the "4 steps off" tint

    /** A bevelled, slightly glossy rounded button face. */
    inline void bevel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float radius = 4.0f, bool pressed = false)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.translated (0.0f, 1.2f), radius);   // drop shadow
        g.setGradientFill (juce::ColourGradient (pressed ? base.darker (0.25f) : base.brighter (0.18f), r.getX(), r.getY(), pressed ? base.brighter (0.05f) : base.darker (0.22f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (pressed ? 0.04f : 0.18f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX(), r.getCentreY(), false));
        g.fillRoundedRectangle (r.reduced (1.0f).withHeight (r.getHeight() * 0.5f), radius - 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (r, radius, 1.0f);
    }

    /** FL's round mute light: a lamp with a glow when on. */
    inline void lamp (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour on, bool lit, bool dimmed = false)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (r.expanded (1.5f));
        if (lit) { g.setColour (on.withAlpha (0.28f)); g.fillEllipse (r.expanded (4.0f)); }
        g.setGradientFill (juce::ColourGradient (lit ? on.brighter (0.5f) : Theme::panel2().brighter (0.1f), r.getX() + r.getWidth() * 0.3f, r.getY() + r.getHeight() * 0.25f,
                                                 lit ? on.darker (0.35f) : Theme::panel2().darker (0.4f), r.getRight(), r.getBottom(), true));
        g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.55f : 0.12f)); g.fillEllipse (r.reduced (r.getWidth() * 0.3f).translated (-r.getWidth() * 0.12f, -r.getHeight() * 0.14f));
        if (dimmed) { g.setColour (Theme::amber()); g.drawEllipse (r, 1.2f); }
    }

    /** FL's little pan / volume dial: a dark cap, an amber arc and a pointer. */
    inline void miniKnob (juce::Graphics& g, juce::Rectangle<float> r, float norm, bool bipolar, juce::Colour accent, bool hover)
    {
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        auto c = r.getCentre(); const float rad = r.getWidth() * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (r.expanded (1.0f).translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (Theme::panel2().brighter (0.35f), c.x - rad * 0.4f, c.y - rad * 0.5f, Theme::panel2().darker (0.5f), c.x + rad, c.y + rad, true));
        g.fillEllipse (r);
        juce::Path track; track.addCentredArc (c.x, c.y, rad * 0.78f, rad * 0.78f, 0.0f, a0, a1, true);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.strokePath (track, juce::PathStrokeType (2.2f));
        juce::Path lit; const float av = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, norm);
        if (bipolar) { const float mid = (a0 + a1) * 0.5f; lit.addCentredArc (c.x, c.y, rad * 0.78f, rad * 0.78f, 0.0f, juce::jmin (mid, av), juce::jmax (mid, av), true); }
        else lit.addCentredArc (c.x, c.y, rad * 0.78f, rad * 0.78f, 0.0f, a0, av, true);
        g.setColour (hover ? accent.brighter (0.3f) : accent); g.strokePath (lit, juce::PathStrokeType (2.2f));
        const auto tip = c.getPointOnCircumference (rad * 0.62f, av), base = c.getPointOnCircumference (rad * 0.2f, av);
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.drawLine (base.x, base.y, tip.x, tip.y, 1.6f);
    }

    /** A step button: FL's pill in two alternating tints, lit in the channel colour with a gloss. */
    inline void step (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool lightGroup, juce::Colour channel, float velocity, bool playing, bool hover, bool ghost)
    {
        const float rad = 3.5f;
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0.0f, 1.0f), rad);
        juce::Colour base = on ? channel.interpolatedWith (juce::Colours::black, (1.0f - velocity) * 0.45f) : (lightGroup ? stepLight() : stepDark());
        if (ghost) base = base.withAlpha (0.4f);
        g.setGradientFill (juce::ColourGradient (base.brighter (on ? 0.35f : 0.12f), r.getX(), r.getY(), base.darker (on ? 0.15f : 0.25f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        if (on) { g.setColour (channel.withAlpha (0.35f)); g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.0f, 1.5f); }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.35f : 0.14f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r.reduced (1.5f, 1.5f).withHeight (r.getHeight() * 0.42f), rad - 1.0f);
        if (hover) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRoundedRectangle (r, rad); }
        if (playing) { g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.5f); }
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawRoundedRectangle (r, rad, 1.0f);
    }

    /** The channel name button: track colour, bevelled, name in bold. */
    inline void channelButton (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, const juce::String& name, const juce::String& badge, bool selected, bool missing, float fontSize)
    {
        bevel (g, r, missing ? Theme::red() : colour.darker (0.1f), 4.0f, false);
        if (selected) { g.setColour (juce::Colours::white.withAlpha (0.55f)); g.drawRoundedRectangle (r, 4.0f, 1.2f); }
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.setFont (Theme::font (fontSize, true));
        g.drawFittedText (name, r.toNearestInt().reduced (8, 0).translated (0, 1), juce::Justification::centredLeft, 1);
        g.setColour (colour.getPerceivedBrightness() > 0.6f ? juce::Colours::black.withAlpha (0.85f) : juce::Colours::white);
        g.drawFittedText (name, r.toNearestInt().reduced (8, 0), juce::Justification::centredLeft, 1);
        if (badge.isNotEmpty()) { g.setFont (Theme::font (7.5f, true)); g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawText (badge, r.toNearestInt().reduced (6, 0), juce::Justification::centredRight); }
    }

    inline void sunkenBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour textCol, float fontSize = 9.0f, bool bold = true)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (textCol); g.setFont (Theme::font (fontSize, bold)); g.drawText (text, r.toNearestInt(), juce::Justification::centred);
    }
}

class ChannelRackComponent : public juce::Component,
                             private juce::Timer
{
public:
    ChannelRackComponent (Session& s) : session (s)
    {
        setWantsKeyboardFocus (true);
        // ---- toolbar
        auto mkButton = [this] (juce::TextButton& b, const char* text, const char* tip, std::function<void()> fn)
        {
            b.setButtonText (text); b.setTooltip (tip); b.onClick = std::move (fn); addAndMakeVisible (b);
        };
        mkButton (prevPat, "<", "Previous pattern", [this] { Rack::selectPattern (edit(), Rack::currentPattern (edit()) - 1); refresh(); });
        mkButton (nextPat, ">", "Next pattern (makes a new one after the last)", [this] { const int i = Rack::currentPattern (edit()) + 1; if (i >= Rack::numPatterns (edit())) Rack::newPattern (edit()); Rack::selectPattern (edit(), i); refresh(); });
        patName.onLeft = [this] { showPatternPicker(); };
        patName.onRight = [this] { showPatternMenu(); };
        addAndMakeVisible (patName);
        mkButton (addPat, "+", "New pattern", [this] { const int i = Rack::newPattern (edit()); Rack::selectPattern (edit(), i); refresh(); });

        lengthBox.setTooltip ("Pattern length in steps");
        for (int st : { 4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 256 }) lengthBox.addItem (juce::String (st), st);
        lengthBox.setEditableText (true);
        lengthBox.onChange = [this] { const int v = lengthBox.getText().getIntValue(); if (v > 0) { Rack::setPatternSteps (edit(), pat(), v); Rack::syncPatternClips (edit(), pat()); refresh(); } };
        addAndMakeVisible (lengthBox);
        divBox.setTooltip ("Steps per beat (step division)");
        const char* divs[] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
        for (int i = 0; i < 8; ++i) divBox.addItem (divs[i], i + 1);
        divBox.onChange = [this] { static const double b[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 }; const int i = divBox.getSelectedId() - 1; if (i >= 0) { Rack::setPatternDivision (edit(), pat(), b[i]); Rack::syncPatternClips (edit(), pat()); refresh(); } };
        addAndMakeVisible (divBox);
        swing.setSliderStyle (juce::Slider::LinearHorizontal); swing.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        swing.setRange (0, 100, 1); swing.setTooltip ("Swing (odd 16ths pushed late, FL style)");
        swing.onValueChange = [this] { Rack::setPatternSwing (edit(), pat(), (float) (swing.getValue() / 100.0)); Rack::syncPatternClips (edit(), pat()); };
        addAndMakeVisible (swing);
        mkButton (patMode, "PAT", "Pattern mode: loop this pattern (the timeline stays silent on rack channels)", [this] { setPatternMode (true); });
        mkButton (songMode, "SONG", "Song mode: play the timeline (pattern clips)", [this] { setPatternMode (false); });
        patMode.setClickingTogglesState (false); songMode.setClickingTogglesState (false);
        mkButton (graphBtn, "GRAPH", "Graph Editor (Cmd+K): per-step lanes for the selected channel", [this] { setGraphVisible (! graphVisible); });
        mkButton (keysBtn, "KEYS", "Keyboard Editor: per-step notes and chords", [this] { setGraphVisible (true); graphLane = Lane::note; graphTabs.setCurrentTabIndex ((int) Lane::note); repaint(); });
        mkButton (addBtn, "+", "Add a channel: instrument, kit, sample, layer, MIDI out", [this] { showAddMenu(); });
        addBtn.setColour (juce::TextButton::buttonColourId, Theme::acid()); addBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        filterBox.setTooltip ("Display filter: which channel group the rack shows");
        filterBox.onChange = [this] { filterGroup = filterBox.getSelectedId() <= 1 ? juce::String() : filterBox.getText(); refresh(); };
        addAndMakeVisible (filterBox);
        mkButton (starterBtn, "BEAT STARTER", "Fill the kit channels with a groove in a genre (dice again for another)", [this] { showStarterMenu(); });
        mkButton (zipBtn, "ZIP", "Zip / unzip all channels (compact rows)", [this] { bool any = false; for (auto t : channels()) any = any || ! Rack::isZipped (*t); for (auto t : channels()) Rack::setZipped (*t, any); refresh(); });
        mkButton (claudeBtn, "CLAUDE", "Ask Claude to work on this rack", [this] { if (onClaude) onClaude(); });
        mkButton (pianoBtn, "PIANO ROLL", "Open the selected channel in the piano roll (steps are converted)", [this] { if (auto t = selected()) { if (auto mc = Rack::sendToPianoRoll (*t, pat())) { if (onOpenPianoRoll) onOpenPianoRoll (*mc); } refresh(); } });

        // ---- graph editor tabs
        const char* laneNames[] = { "NOTE", "VELOCITY", "RELEASE", "FINE PITCH", "PAN", "MOD X", "MOD Y", "SHIFT", "REPEAT", "CHANCE", "GATE" };
        for (int i = 0; i < numLanes; ++i) graphTabs.addTab (laneNames[i], Theme::panel2(), -1);
        graphTabs.setCurrentTabIndex ((int) Lane::velocity, juce::dontSendNotification);   // the lane the rack opens on (FL opens on Velocity too)
        graphTabs.onTabChanged = [this] (int i) { graphLane = (Lane) i; repaint(); };
        addChildComponent (graphTabs);
        graphTabs.setVisible (false);

        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&rows, false);
        viewport.setScrollBarsShown (true, true);
        rows.owner = this;

        Rack::ensurePatterns (edit(), 1);
        refresh();
        startTimerHz (20);
    }

    ~ChannelRackComponent() override   { stopTimer(); }

    //==========================================================================
    std::function<void (te::Plugin&)> onOpenInstrument;
    std::function<void (te::MidiClip&)> onOpenPianoRoll;
    std::function<void (te::AudioTrack&)> onOpenChannelSettings;
    std::function<void()> onClaude;
    std::function<void (te::AudioTrack&)> onSelectTrack;

    te::Edit& edit()                             { return *session.edit; }
    int pat()                                    { return Rack::currentPattern (edit()); }
    juce::Array<te::AudioTrack*> channels()
    {
        auto all = Rack::channels (edit());
        if (filterGroup.isEmpty()) return all;
        juce::Array<te::AudioTrack*> out;
        for (auto t : all) if (filterGroup == "Unsorted" ? Rack::group (*t).isEmpty() : Rack::group (*t) == filterGroup) out.add (t);
        return out;
    }
    te::AudioTrack* selected()                   { auto ch = channels(); return juce::isPositiveAndBelow (selectedRow, ch.size()) ? ch[selectedRow] : nullptr; }
    void selectRow (int i)                       { selectedRow = i; if (auto t = selected()) if (onSelectTrack) onSelectTrack (*t); repaint(); }

    void setGraphVisible (bool v)                { graphVisible = v; graphTabs.setVisible (v); graphBtn.setToggleState (v, juce::dontSendNotification); resized(); repaint(); }
    bool isGraphVisible() const                  { return graphVisible; }

    void setPatternMode (bool on)
    {
        Rack::setPatternMode (edit(), on);
        auto& tr = edit().getTransport();
        if (on)
        {
            if (! tr.isPlaying()) { tr.setPosition (te::TimePosition()); tr.play (false); }
            launchBeat = edit().tempoSequence.toBeats (tr.getPosition()).inBeats();
        }
        refresh();
    }

    /** Re-reads the model into the widgets. Cheap; called by the timer when something changed and by the MCP tools. */
    void refresh()
    {
        auto& ed = edit();
        const int p = pat();
        patName.setText (Rack::patternName (ed, p), Rack::patternColour (ed, p), juce::String (p + 1) + "/" + juce::String (Rack::numPatterns (ed)));
        if (auto* w = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent())) if (w->getName().startsWith ("Channel Rack")) w->setName ("Channel Rack  -  " + Rack::patternName (ed, p) + (Rack::patternMode (ed) ? "   (PAT)" : "   (SONG)"));
        lengthBox.setText (juce::String (Rack::patternSteps (ed, p)), juce::dontSendNotification);
        {
            static const double b[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
            int best = 3; for (int i = 0; i < 8; ++i) if (std::abs (b[i] - Rack::patternDivision (ed, p)) < 1.0e-6) best = i;
            divBox.setSelectedId (best + 1, juce::dontSendNotification);
        }
        swing.setValue (Rack::patternSwing (ed, p) * 100.0, juce::dontSendNotification);
        {
            juce::StringArray groups; for (auto t : Rack::channels (ed)) if (Rack::group (*t).isNotEmpty()) groups.addIfNotAlreadyThere (Rack::group (*t));
            const auto cur = filterGroup;
            filterBox.clear (juce::dontSendNotification); filterBox.addItem ("All", 1); filterBox.addItem ("Unsorted", 2); int id = 3; for (auto& gname : groups) filterBox.addItem (gname, id++);
            int sel = 1; for (int i = 0; i < filterBox.getNumItems(); ++i) if (filterBox.getItemText (i) == cur) sel = filterBox.getItemId (i);
            if (cur.isEmpty()) sel = 1;
            filterBox.setSelectedId (sel, juce::dontSendNotification);
        }
        const bool pm = Rack::patternMode (ed);
        patMode.setToggleState (pm, juce::dontSendNotification); songMode.setToggleState (! pm, juce::dontSendNotification);
        rows.rebuild();
        resized();
        repaint();
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (RackLook::panelDark());
        auto top = getLocalBounds().removeFromTop (toolbarH);
        g.setGradientFill (juce::ColourGradient (Theme::panel().brighter (0.08f), 0.0f, 0.0f, Theme::panel().darker (0.15f), 0.0f, (float) toolbarH, false));
        g.fillRect (top);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawHorizontalLine (toolbarH - 1, 0.0f, (float) getWidth());
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawHorizontalLine (0, 0.0f, (float) getWidth());
        g.setFont (Theme::font (8.0f, true)); g.setColour (Theme::dim());
        auto cap = [&] (juce::Component& c, const juce::String& t) { g.drawText (t, c.getBounds().withY (2).withHeight (10).expanded (24, 0), juce::Justification::centred); };
        cap (patName, "PATTERN"); cap (lengthBox, "STEPS"); cap (divBox, "DIV"); cap (swing, "SWING  " + juce::String ((int) swing.getValue()) + "%");
        // footer: + channel and the display filter, FL style
        auto foot = getLocalBounds().withTrimmedBottom (graphVisible ? graphH : 0).removeFromBottom (footerH);
        g.setColour (Theme::panel().darker (0.1f)); g.fillRect (foot);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawHorizontalLine (foot.getY(), 0.0f, (float) getWidth());
        if (graphVisible) paintGraph (g, graphArea());
        if (channels().isEmpty())
        {
            g.setColour (Theme::dim()); g.setFont (Theme::font (13.0f));
            g.drawFittedText ("Empty rack.  Press  +  to add an instrument, a kit or a sample, or BEAT STARTER for a groove in one click.\nEvery channel is a track in the Edit window; every pattern is a clip group on the timeline.",
                              viewport.getBounds().reduced (30), juce::Justification::centred, 3);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (toolbarH).reduced (6, 0).withTrimmedTop (11);
        auto place = [&] (juce::Component& c, int w) { c.setBounds (top.removeFromLeft (w).reduced (2, 4)); };
        place (prevPat, 22); place (patName, 164); place (nextPat, 22); place (addPat, 22);
        top.removeFromLeft (10);
        place (swing, 110); place (lengthBox, 58); place (divBox, 64);
        top.removeFromLeft (10);
        place (patMode, 42); place (songMode, 50);
        top.removeFromLeft (10);
        place (graphBtn, 54); place (keysBtn, 46); place (pianoBtn, 80);
        top.removeFromLeft (10);
        place (starterBtn, 104); place (zipBtn, 36); place (claudeBtn, 60);
        if (graphVisible)
        {
            auto gArea = r.removeFromBottom (graphH);
            graphTabs.setBounds (gArea.removeFromTop (22));
            graphBody = gArea;
        }
        auto foot = r.removeFromBottom (footerH).reduced (6, 3);
        addBtn.setBounds (foot.removeFromLeft (30));
        foot.removeFromLeft (6);
        filterBox.setBounds (foot.removeFromLeft (120));
        viewport.setBounds (r);
        rows.setSize (juce::jmax (r.getWidth() - 2, rows.wantedWidth()), rows.wantedHeight());
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress ('k', juce::ModifierKeys::commandModifier, 0)) { setGraphVisible (! graphVisible); return true; }
        if (k.getKeyCode() == juce::KeyPress::upKey && k.getModifiers().isAltDown()) { if (auto t = selected()) { Rack::moveChannel (edit(), *t, selectedRow - 1); selectedRow = juce::jmax (0, selectedRow - 1); refresh(); } return true; }
        if (k.getKeyCode() == juce::KeyPress::downKey && k.getModifiers().isAltDown()) { if (auto t = selected()) { Rack::moveChannel (edit(), *t, selectedRow + 1); selectedRow = juce::jmin (channels().size() - 1, selectedRow + 1); refresh(); } return true; }
        if (k.getKeyCode() == juce::KeyPress::upKey)   { selectRow (juce::jmax (0, selectedRow - 1)); return true; }
        if (k.getKeyCode() == juce::KeyPress::downKey) { selectRow (juce::jmin (channels().size() - 1, selectedRow + 1)); return true; }
        return false;
    }

    //==========================================================================
    // the shared step colours (4 beats of 4), FL style but in the Squelchmoth palette
    static juce::Colour stepColour (int step, bool on, juce::Colour trackColour)
    {
        const int beat = (step / 4) % 2;
        if (! on) return beat == 0 ? Theme::panel2().brighter (0.08f) : Theme::panel2().darker (0.15f);
        return beat == 0 ? trackColour : trackColour.interpolatedWith (juce::Colours::white, 0.25f);
    }

private:
    //==========================================================================
    enum class Lane { note = 0, velocity, release, pitch, pan, modX, modY, shift, repeat, chance, gate };
    static constexpr int numLanes = 11;
    static constexpr int toolbarH = 46, graphH = 190, footerH = 26;
    static constexpr int rowH = 32, zipH = 19, stepW = 22, leftW = 286;

    Session& session;
    juce::TextButton prevPat, nextPat, addPat, patMode, songMode, graphBtn, keysBtn, pianoBtn, addBtn, starterBtn, zipBtn, claudeBtn;
    struct PatternNameButton : public juce::Component
    {
        juce::String text, sub; juce::Colour colour;
        std::function<void()> onLeft, onRight;
        void setText (const juce::String& t, juce::Colour c, const juce::String& s)   { text = t; colour = c; sub = s; repaint(); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (colour.withAlpha (0.85f)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::black); g.setFont (Theme::font (12.0f, true));
            g.drawFittedText (text, getLocalBounds().reduced (6, 0).withTrimmedRight (34), juce::Justification::centredLeft, 1);
            g.setFont (Theme::font (9.0f)); g.drawText (sub, getLocalBounds().reduced (6, 0), juce::Justification::centredRight);
        }
        void mouseDown (const juce::MouseEvent& e) override   { if (e.mods.isPopupMenu()) { if (onRight) onRight(); } else if (onLeft) onLeft(); }
    } patName;
    juce::ComboBox lengthBox, divBox, filterBox;
    juce::String filterGroup;
    juce::Slider swing;
    struct Tabs : public juce::TabbedButtonBar { Tabs() : juce::TabbedButtonBar (juce::TabbedButtonBar::TabsAtTop) {} std::function<void (int)> onTabChanged; void currentTabChanged (int i, const juce::String&) override { if (onTabChanged) onTabChanged (i); } } graphTabs;
    juce::Viewport viewport;
    juce::Rectangle<int> graphBody;
    bool graphVisible = false;
    Lane graphLane = Lane::velocity;
    int selectedRow = 0;
    double launchBeat = 0.0;
    juce::int64 lastHash = 0;

    //==========================================================================
    struct Rows : public juce::Component, public juce::SettableTooltipClient
    {
        ChannelRackComponent* owner = nullptr;
        int dragRow = -1, dragStep = -1; bool dragOn = true, dragMove = false; int moveFrom = 0; int hoverRow = -1, hoverStep = -1;
        enum class Knob { none, pan, vol }; Knob knobDrag = Knob::none; int knobRow = -1; float knobStart = 0; int hoverKnobRow = -1; Knob hoverKnob = Knob::none;

        int rowHeightOf (te::AudioTrack& t) const   { return Rack::isZipped (t) ? zipH : rowH; }
        int wantedHeight()   { int h = 8; if (owner) for (auto t : owner->channels()) h += rowHeightOf (*t) + 3; return h; }
        int wantedWidth()    { int steps = owner ? Rack::patternSteps (owner->edit(), owner->pat()) : 16; return leftW + steps * stepW + (steps / 4) * 5 + 24; }

        void rebuild()   { setSize (juce::jmax (getWidth(), wantedWidth()), wantedHeight()); repaint(); }

        juce::Rectangle<int> rowBounds (int i)
        {
            int y = 4; auto chans = owner->channels();
            for (int k = 0; k < i && k < chans.size(); ++k) y += rowHeightOf (*chans[k]) + 3;
            const int h = juce::isPositiveAndBelow (i, chans.size()) ? rowHeightOf (*chans[i]) : rowH;
            return { 0, y, getWidth(), h };
        }
        int rowAt (int y)
        {
            auto chans = owner->channels();
            for (int i = 0; i < chans.size(); ++i) if (rowBounds (i).contains (4, y)) return i;
            return -1;
        }
        int stepsX (int step) const   { return leftW + step * stepW + (step / 4) * 5; }
        int stepAt (int x) const      { if (x < leftW) return -1; int s = 0; while (stepsX (s + 1) <= x && s < 1024) ++s; return x < stepsX (s) + stepW - 2 ? s : -1; }
        // FL order: lamp | pan | vol | mixer target | channel button | selector
        juce::Rectangle<int> muteR (juce::Rectangle<int> r) const   { const int d = juce::jmin (14, r.getHeight() - 6); return { 8, r.getCentreY() - d / 2, d, d }; }
        juce::Rectangle<int> panR (juce::Rectangle<int> r) const    { const int d = juce::jmin (22, r.getHeight() - 4); return { 30, r.getCentreY() - d / 2, d, d }; }
        juce::Rectangle<int> volR (juce::Rectangle<int> r) const    { const int d = juce::jmin (22, r.getHeight() - 4); return { 58, r.getCentreY() - d / 2, d, d }; }
        juce::Rectangle<int> targetR (juce::Rectangle<int> r) const { return { 88, r.getY() + 4, 30, r.getHeight() - 8 }; }
        juce::Rectangle<int> nameR (juce::Rectangle<int> r) const   { return { 124, r.getY() + 3, 138, r.getHeight() - 6 }; }
        juce::Rectangle<int> ledR (juce::Rectangle<int> r) const    { return { 268, r.getCentreY() - 5, 10, 10 }; }

        void paint (juce::Graphics& g) override
        {
            auto chans = owner->channels();
            auto& ed = owner->edit();
            const int p = owner->pat();
            const int steps = Rack::patternSteps (ed, p);
            const int playStep = owner->currentPlayStep();
            auto& sess = owner->session;
            // playhead column behind the rows
            if (playStep >= 0 && ! chans.isEmpty()) { g.setColour (Theme::acid().withAlpha (0.10f)); g.fillRoundedRectangle ((float) stepsX (playStep) - 1.0f, 0.0f, (float) stepW, (float) getHeight(), 3.0f); }
            for (int i = 0; i < chans.size(); ++i)
            {
                auto& t = *chans[i]; auto r = rowBounds (i);
                const bool sel = i == owner->selectedRow, zip = Rack::isZipped (t);
                // the row plate
                auto plate = r.toFloat().reduced (3.0f, 0.0f);
                g.setGradientFill (juce::ColourGradient (RackLook::rowFace().brighter (sel ? 0.10f : 0.04f), 0.0f, plate.getY(), RackLook::rowFace().darker (0.12f), 0.0f, plate.getBottom(), false));
                g.fillRoundedRectangle (plate, 5.0f);
                g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (plate, 5.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) plate.getY() + 1, plate.getX() + 4.0f, plate.getRight() - 4.0f);
                // mute lamp (green), solo (amber ring)
                const bool muted = t.isMuted (false), solo = t.isSolo (false), locked = (bool) t.state.getProperty ("sqmRackLock", false);
                RackLook::lamp (g, muteR (r).toFloat(), solo ? Theme::amber() : Theme::acid(), ! muted, locked);
                // pan / volume dials
                const float pan = MixCtl::getPan (sess, t), vol = MixCtl::getVolumeDb (sess, t);
                RackLook::miniKnob (g, panR (r).toFloat(), (pan + 1.0f) * 0.5f, true, Theme::amber(), hoverKnobRow == i && hoverKnob == Knob::pan);
                RackLook::miniKnob (g, volR (r).toFloat(), juce::jlimit (0.0f, 1.0f, (vol + 60.0f) / 72.0f), false, Theme::amber(), hoverKnobRow == i && hoverKnob == Knob::vol);
                // mixer target
                RackLook::sunkenBox (g, targetR (r).toFloat(), juce::String (sess.indexOf (&t)), Theme::mint(), zip ? 8.0f : 9.5f);
                // channel button
                const bool missing = Rack::instrumentOf (t) == nullptr && Rack::channelType (t) != "layer" && Rack::channelType (t) != "midiout";
                const auto type = Rack::channelType (t);
                RackLook::channelButton (g, nameR (r).toFloat(), t.getColour(), t.getName(), zip ? juce::String() : (type == "sample" ? "SMP" : type == "plugin" ? "VST" : type == "layer" ? "LAYER" : type == "midiout" ? "MIDI" : ""), sel, missing, zip ? 9.5f : 11.5f);
                // selector LED
                RackLook::lamp (g, ledR (r).toFloat(), Theme::acid(), sel);
                // steps or piano-roll preview
                if (auto mc = Rack::midiClip (t, p))
                {
                    auto area = juce::Rectangle<int> (stepsX (0), r.getY() + 3, stepsX (steps) - stepsX (0) - 3, r.getHeight() - 6);
                    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (area.toFloat(), 4.0f);
                    const double len = Rack::patternLengthBeats (ed, p);
                    int lo = 127, hi = 0; for (auto n : mc->getSequence().getNotes()) { lo = juce::jmin (lo, n->getNoteNumber()); hi = juce::jmax (hi, n->getNoteNumber()); }
                    if (hi < lo) { lo = 48; hi = 72; }
                    for (int b = 0; b <= 4; ++b) { g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawVerticalLine (area.getX() + area.getWidth() * b / 4, (float) area.getY(), (float) area.getBottom()); }
                    for (auto n : mc->getSequence().getNotes())
                    {
                        const float x0 = (float) area.getX() + (float) (n->getStartBeat().inBeats() / len) * area.getWidth();
                        const float w = juce::jmax (2.0f, (float) (n->getLengthBeats().inBeats() / len) * area.getWidth());
                        const float y = (float) area.getBottom() - 2.0f - (float) (n->getNoteNumber() - lo + 1) / (float) (hi - lo + 2) * (area.getHeight() - 4);
                        g.setColour (t.getColour().brighter (0.2f)); g.fillRoundedRectangle (x0, y - 1.5f, w, 3.0f, 1.0f);
                    }
                    g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f, true)); g.drawText ("PIANO ROLL  -  click to edit", area.reduced (6, 0), juce::Justification::centredRight);
                }
                else
                {
                    auto sc = Rack::stepClip (t, p, false);
                    std::optional<te::StepClip::Pattern> pp; if (sc) pp.emplace (sc->getPattern (0));
                    const int rowLen = pp ? BeatLab::rowLength (*pp, 0) : 0;
                    auto notes = Rack::extraLane (t, p, "note");
                    for (int st = 0; st < steps; ++st)
                    {
                        auto cell = juce::Rectangle<float> ((float) stepsX (st), (float) r.getY() + 3.0f, (float) stepW - 3.0f, (float) r.getHeight() - 6.0f);
                        const bool on = pp && st < pp->getNumNotes() && pp->getNote (0, st);
                        const bool ghost = rowLen > 0 && st >= rowLen;
                        const float vel = on ? pp->getVelocity (0, st) / 127.0f : 1.0f;
                        RackLook::step (g, cell, on, ((st / 4) % 2) == 0, t.getColour(), vel, st == playStep, i == hoverRow && st == hoverStep, ghost);
                        if (on && pp)
                        {
                            const int rep = BeatLab::getRepeat (*pp, 0, st);
                            g.setFont (Theme::font (zip ? 7.5f : 8.5f, true));
                            if (rep > 1) { g.setColour (juce::Colours::black.withAlpha (0.75f)); g.drawText (juce::String (rep), cell.toNearestInt(), juce::Justification::centred); }
                            else if (pp->getProbability (0, st) < 0.999f) { g.setColour (juce::Colours::black.withAlpha (0.75f)); g.drawText ("?", cell.toNearestInt(), juce::Justification::centred); }
                            if (! zip && juce::isPositiveAndBelow (st, notes.size()) && notes[st].getIntValue() != 0)
                            { g.setColour (juce::Colours::black.withAlpha (0.8f)); g.setFont (Theme::font (7.0f, true)); g.drawText (juce::MidiMessage::getMidiNoteName (notes[st].upToFirstOccurrenceOf ("+", false, false).getIntValue(), true, true, 4) + (notes[st].contains ("+") ? "+" : ""), cell.toNearestInt().withTrimmedBottom (1), juce::Justification::centredBottom); }
                        }
                    }
                }
            }
        }

        Knob knobAt (juce::Point<int> p, int row)   { auto rb = rowBounds (row); if (panR (rb).expanded (2).contains (p)) return Knob::pan; if (volR (rb).expanded (2).contains (p)) return Knob::vol; return Knob::none; }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int r = rowAt (e.y), s = stepAt (e.x);
            const Knob k = r >= 0 ? knobAt (e.getPosition(), r) : Knob::none;
            if (r != hoverRow || s != hoverStep || k != hoverKnob)
            {
                hoverRow = r; hoverStep = s; hoverKnob = k; hoverKnobRow = r; repaint();
                if (e.mods.isAltDown() && r >= 0 && s >= 0) owner->audition (r);
            }
            if (r >= 0)
            {
                auto chans = owner->channels(); auto& t = *chans[r];
                if (k == Knob::pan) setTooltip ("Pan  " + MixCtl::panText (MixCtl::getPan (owner->session, t)) + "  (drag, double-click to centre)");
                else if (k == Knob::vol) setTooltip ("Volume  " + MixCtl::dbText (MixCtl::getVolumeDb (owner->session, t)) + " dB  (drag, double-click for 0 dB)");
                else if (s >= 0) setTooltip ("Step " + juce::String (s + 1) + "  -  left: on, right: off, drag paints, Alt: accent, double-click: roll / chance");
                else if (nameR (rowBounds (r)).contains (e.getPosition())) setTooltip (t.getName() + "  -  click: instrument, double-click: channel settings, right-click: channel menu");
                else if (muteR (rowBounds (r)).expanded (3).contains (e.getPosition())) setTooltip ("Mute  (Cmd-click: solo, Shift-click: lock for the Beat Starter)");
                else if (targetR (rowBounds (r)).contains (e.getPosition())) setTooltip ("Track " + juce::String (owner->session.indexOf (&t)) + " in the Edit / Mix window - click to select it there");
                else setTooltip ({});
            }
        }
        void mouseExit (const juce::MouseEvent&) override   { hoverRow = hoverStep = -1; hoverKnob = Knob::none; repaint(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int r = rowAt (e.y); if (r < 0) return;
            auto chans = owner->channels(); auto& t = *chans[r];
            auto rb = rowBounds (r);
            owner->selectRow (r);
            if (auto k = knobAt (e.getPosition(), r); k != Knob::none)
            {
                if (e.getNumberOfClicks() >= 2) { if (k == Knob::pan) MixCtl::setPan (owner->session, t, 0.0f); else MixCtl::setVolumeDb (owner->session, t, 0.0f); repaint(); return; }
                knobDrag = k; knobRow = r; knobStart = k == Knob::pan ? MixCtl::getPan (owner->session, t) : MixCtl::getVolumeDb (owner->session, t);
                return;
            }
            if (muteR (rb).expanded (3).contains (e.getPosition()))
            {
                if (e.mods.isCommandDown()) t.setSolo (! t.isSolo (false));
                else if (e.mods.isShiftDown()) t.state.setProperty ("sqmRackLock", ! (bool) t.state.getProperty ("sqmRackLock", false), nullptr);
                else t.setMute (! t.isMuted (false));
                repaint(); return;
            }
            if (nameR (rb).contains (e.getPosition()))
            {
                if (e.mods.isPopupMenu()) owner->showChannelMenu (r);
                else if (e.getNumberOfClicks() >= 2) { if (owner->onOpenChannelSettings) owner->onOpenChannelSettings (t); }
                else if (auto pl = Rack::instrumentOf (t)) { if (owner->onOpenInstrument) owner->onOpenInstrument (*pl); }
                else if (owner->onOpenChannelSettings) owner->onOpenChannelSettings (t);
                return;
            }
            if (targetR (rb).contains (e.getPosition())) { if (owner->onSelectTrack) owner->onSelectTrack (t); return; }
            const int s = stepAt (e.x);
            if (s < 0) return;
            if (Rack::midiClip (t, owner->pat()) != nullptr)
            {
                if (auto mc = Rack::midiClip (t, owner->pat())) if (owner->onOpenPianoRoll) owner->onOpenPianoRoll (*mc);
                return;
            }
            const int p = owner->pat();
            if (e.mods.isShiftDown()) { dragMove = true; moveFrom = s; dragRow = r; return; }
            if (e.getNumberOfClicks() >= 2 && ! e.mods.isPopupMenu()) { owner->showStepMenu (r, s); return; }
            const bool wasOn = Rack::step (t, p, s);
            dragRow = r; dragStep = s;
            if (e.mods.isPopupMenu()) { dragOn = false; Rack::setStep (t, p, s, false); }
            else if (e.mods.isAltDown()) { dragOn = true; Rack::setStep (t, p, s, true, 127); }
            else { dragOn = ! wasOn; Rack::setStep (t, p, s, dragOn, 100); if (dragOn && ! owner->edit().getTransport().isPlaying()) owner->audition (r); }
            Rack::syncPatternClips (owner->edit(), p);
            repaint();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (knobDrag != Knob::none)
            {
                auto chans = owner->channels(); if (! juce::isPositiveAndBelow (knobRow, chans.size())) return;
                auto& t = *chans[knobRow];
                const float d = (float) -e.getDistanceFromDragStartY() / 120.0f * (e.mods.isShiftDown() ? 0.2f : 1.0f);
                if (knobDrag == Knob::pan) MixCtl::setPan (owner->session, t, juce::jlimit (-1.0f, 1.0f, knobStart + d * 2.0f));
                else MixCtl::setVolumeDb (owner->session, t, juce::jlimit (-60.0f, 12.0f, knobStart + d * 36.0f));
                repaint(); return;
            }
            if (dragRow < 0) return;
            auto chans = owner->channels(); if (! juce::isPositiveAndBelow (dragRow, chans.size())) return;
            auto& t = *chans[dragRow]; const int p = owner->pat();
            const int s = stepAt (juce::jlimit (leftW, getWidth() - 1, e.x));
            if (s < 0) return;
            if (dragMove)
            {
                if (s != moveFrom) { if (auto sc = Rack::stepClip (t, p, false)) { auto pp = sc->getPattern (0); BeatLab::rotateRow (*sc, pp, 0, s - moveFrom); } moveFrom = s; repaint(); }
                return;
            }
            if (s != dragStep)
            {
                const int a = juce::jmin (dragStep, s), b = juce::jmax (dragStep, s);
                for (int k = a; k <= b; ++k) if (k != dragStep || Rack::step (t, p, k) != dragOn) Rack::setStep (t, p, k, dragOn, e.mods.isAltDown() ? 127 : 100);
                dragStep = s; repaint();
            }
        }
        void mouseUp (const juce::MouseEvent&) override   { if (dragRow >= 0) Rack::syncPatternClips (owner->edit(), owner->pat()); dragRow = -1; dragStep = -1; dragMove = false; knobDrag = Knob::none; }
    } rows;

    //==========================================================================
    int currentPlayStep()
    {
        auto& tr = edit().getTransport();
        if (! tr.isPlaying()) return -1;
        const double beats = edit().tempoSequence.toBeats (tr.getPosition()).inBeats();
        const double len = Rack::patternLengthBeats (edit(), pat()), div = Rack::patternDivision (edit(), pat());
        if (Rack::patternMode (edit()))
        {
            const double rel = std::fmod (juce::jmax (0.0, beats - launchBeat), len);
            return (int) (rel / div);
        }
        // song mode: show the step of the pattern clip under the playhead on the selected channel
        if (auto t = selected())
            for (auto c : t->getClips())
                if (c->getEditTimeRange().contains (tr.getPosition()) && Rack::clipPatternId (*c) == Rack::patternId (*Rack::pattern (edit(), pat())))
                {
                    const double rel = std::fmod (juce::jmax (0.0, beats - edit().tempoSequence.toBeats (c->getPosition().getStart()).inBeats()), len);
                    return (int) (rel / div);
                }
        return -1;
    }

    void audition (int row)
    {
        auto chans = channels(); if (! juce::isPositiveAndBelow (row, chans.size())) return;
        auto& t = *chans[row];
        if (Rack::instrumentOf (t) == nullptr) return;
        t.playGuideNote (Rack::rootNote (t), te::MidiChannel (1), 100, true, true);
        juce::Timer::callAfterDelay (250, [sp = te::Track::Ptr (&t)] { if (auto at = dynamic_cast<te::AudioTrack*> (sp.get())) at->turnOffGuideNotes(); });
    }

    juce::int64 modelHash()
    {
        juce::int64 h = Rack::currentPattern (edit()) * 7919 + Rack::numPatterns (edit()) * 31 + (Rack::patternMode (edit()) ? 1 : 0);
        for (auto t : channels())
        {
            h = h * 31 + (juce::int64) t->itemID.getRawID();
            h = h * 31 + t->getName().hashCode64() + (t->isMuted (false) ? 3 : 0) + (t->isSolo (false) ? 5 : 0) + (Rack::isZipped (*t) ? 7 : 0) + (juce::int64) t->getColour().getARGB();
            if (auto c = Rack::slotClip (*t, pat())) { h = h * 31 + c->state.getNumChildren(); for (auto id : { "PATTERNS", "SEQUENCE" }) { auto ch = c->state.getChildWithName (id); if (ch.isValid()) h = h * 31 + ch.toXmlString().hashCode64(); } if (auto mc = dynamic_cast<te::MidiClip*> (c)) h = h * 31 + mc->getSequence().getNumNotes(); }
        }
        h = h * 31 + Rack::patternSteps (edit(), pat()) + (juce::int64) (Rack::patternDivision (edit(), pat()) * 1000) + (juce::int64) (Rack::patternSwing (edit(), pat()) * 1000) + Rack::patternName (edit(), pat()).hashCode64();
        return h;
    }

    void timerCallback() override
    {
        const auto h = modelHash();
        if (h != lastHash) { lastHash = h; refresh(); }
        else if (edit().getTransport().isPlaying()) rows.repaint();
    }

    //==========================================================================
    // Graph / Keyboard editor
    juce::Rectangle<int> graphArea() const   { return graphBody; }

    float laneValue (te::AudioTrack& t, int s)
    {
        const int p = pat();
        switch (graphLane)
        {
            case Lane::note:     { auto toks = Rack::extraLane (t, p, "note"); const int n = juce::isPositiveAndBelow (s, toks.size()) ? toks[s].upToFirstOccurrenceOf ("+", false, false).getIntValue() : 0; return (float) (n > 0 ? n : Rack::rootNote (t)); }
            case Lane::velocity: return Rack::lane (t, p, BeatLab::Lane::velocity, s);
            case Lane::release:  return Rack::extraLaneValue (t, p, "release", s);
            case Lane::pitch:    return Rack::extraLaneValue (t, p, "pitch", s);
            case Lane::pan:      return Rack::extraLaneValue (t, p, "pan", s);
            case Lane::modX:     return Rack::extraLaneValue (t, p, "modx", s);
            case Lane::modY:     return Rack::extraLaneValue (t, p, "mody", s);
            case Lane::shift:    return Rack::lane (t, p, BeatLab::Lane::shift, s);
            case Lane::repeat:   return Rack::lane (t, p, BeatLab::Lane::repeat, s);
            case Lane::chance:   return Rack::lane (t, p, BeatLab::Lane::chance, s);
            case Lane::gate:     return Rack::lane (t, p, BeatLab::Lane::gate, s);
        }
        return 0.0f;
    }
    // lane ranges: lo, hi, default
    void laneRange (float& lo, float& hi, float& def, te::AudioTrack* t)
    {
        switch (graphLane)
        {
            case Lane::note:     lo = (float) juce::jmax (0, (t ? Rack::rootNote (*t) : 60) - 24); hi = lo + 48; def = (float) (t ? Rack::rootNote (*t) : 60); break;
            case Lane::velocity: lo = 1; hi = 127; def = 100; break;
            case Lane::release:  lo = 0; hi = 1; def = 1; break;
            case Lane::pitch:    lo = -100; hi = 100; def = 0; break;
            case Lane::pan:      lo = -1; hi = 1; def = 0; break;
            case Lane::modX: case Lane::modY: lo = 0; hi = 1; def = 0; break;
            case Lane::shift:    lo = -0.5f; hi = 0.5f; def = 0; break;
            case Lane::repeat:   lo = 1; hi = 8; def = 1; break;
            case Lane::chance:   lo = 0; hi = 1; def = 1; break;
            case Lane::gate:     lo = 0.05f; hi = 2; def = 1; break;
        }
    }
    void setLaneValue (te::AudioTrack& t, int s, float v)
    {
        const int p = pat();
        switch (graphLane)
        {
            case Lane::note:     Rack::setExtraLaneValue (t, p, "note", s, juce::String (juce::roundToInt (v))); break;
            case Lane::velocity: Rack::setLane (t, p, BeatLab::Lane::velocity, s, v); break;
            case Lane::release:  Rack::setExtraLaneValue (t, p, "release", s, juce::String (v, 3)); break;
            case Lane::pitch:    Rack::setExtraLaneValue (t, p, "pitch", s, juce::String (v, 1)); break;
            case Lane::pan:      Rack::setExtraLaneValue (t, p, "pan", s, juce::String (v, 3)); break;
            case Lane::modX:     Rack::setExtraLaneValue (t, p, "modx", s, juce::String (v, 3)); break;
            case Lane::modY:     Rack::setExtraLaneValue (t, p, "mody", s, juce::String (v, 3)); break;
            case Lane::shift:    Rack::setLane (t, p, BeatLab::Lane::shift, s, v); break;
            case Lane::repeat:   Rack::setLane (t, p, BeatLab::Lane::repeat, s, std::round (v)); break;
            case Lane::chance:   Rack::setLane (t, p, BeatLab::Lane::chance, s, v); break;
            case Lane::gate:     Rack::setLane (t, p, BeatLab::Lane::gate, s, v); break;
        }
    }
    juce::String laneText (float v, te::AudioTrack*)
    {
        switch (graphLane)
        {
            case Lane::note:     return juce::MidiMessage::getMidiNoteName (juce::roundToInt (v), true, true, 4);
            case Lane::velocity: return juce::String ((int) v);
            case Lane::release: case Lane::modX: case Lane::modY: case Lane::chance: return juce::String (juce::roundToInt (v * 100)) + "%";
            case Lane::pitch:    return juce::String (juce::roundToInt (v)) + " ct";
            case Lane::pan:      return std::abs (v) < 0.005f ? "C" : (v < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (v) * 100));
            case Lane::shift:    return juce::String (juce::roundToInt (v * 100)) + "%";
            case Lane::repeat:   return "x" + juce::String ((int) std::round (v));
            case Lane::gate:     return juce::String (v, 2);
        }
        return {};
    }

    void paintGraph (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (Theme::panel()); g.fillRect (area);
        g.setColour (Theme::line()); g.drawHorizontalLine (area.getY(), 0.0f, (float) getWidth());
        auto t = selected();
        if (t == nullptr) { g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f)); g.drawText ("Select a channel to edit its lanes", area, juce::Justification::centred); return; }
        const int steps = Rack::patternSteps (edit(), pat());
        float lo, hi, def; laneRange (lo, hi, def, t);
        auto in = area.reduced (0, 6);
        const int x0 = leftW - viewport.getViewPositionX();
        g.setColour (Theme::text()); g.setFont (Theme::font (10.0f, true));
        g.drawText (t->getName() + "  -  " + graphTabs.getCurrentTabName() + (graphLane == Lane::note ? "  (drag to pick a note, Shift-click adds a chord note, Alt-click resets)" : "  (drag to draw, right-drag ramps, Alt-click resets, Ctrl-drag scales all)"), area.getX() + 8, area.getY() + 4, leftW - 16, 14, juce::Justification::centredLeft);
        const bool bipolar = graphLane == Lane::pitch || graphLane == Lane::pan || graphLane == Lane::shift;
        const float zeroY = bipolar ? (float) in.getCentreY() : (float) in.getBottom();
        auto notes = graphLane == Lane::note ? Rack::extraLane (*t, pat(), "note") : juce::StringArray();
        for (int s = 0; s < steps; ++s)
        {
            const int x = x0 + rows.stepsX (s) - leftW;
            auto cell = juce::Rectangle<int> (x, in.getY(), stepW - 3, in.getHeight());
            if (cell.getRight() < leftW) continue;
            const bool on = Rack::step (*t, pat(), s);
            g.setColour (stepColour (s, false, t->getColour()).withAlpha (on ? 0.9f : 0.45f)); g.fillRoundedRectangle (cell.toFloat(), 2.0f);
            const float v = on ? laneValue (*t, s) : def;   // an off step shows the lane's default, dimmed
            const float norm = juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
            if (graphLane == Lane::note)
            {
                // mini keyboard: 48 keys, chord notes drawn as dots
                juce::StringArray chord = juce::isPositiveAndBelow (s, notes.size()) && notes[s].getIntValue() != 0 ? juce::StringArray::fromTokens (notes[s], "+", "") : juce::StringArray (juce::String (Rack::rootNote (*t)));
                for (int k = 0; k < 48; ++k)
                {
                    const int note = (int) lo + k; const int pc = note % 12; const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
                    const float ky = (float) cell.getBottom() - (float) (k + 1) / 48.0f * cell.getHeight();
                    g.setColour (black ? juce::Colours::black.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.06f));
                    g.fillRect ((float) cell.getX(), ky, (float) cell.getWidth(), cell.getHeight() / 48.0f);
                    if (pc == 0) { g.setColour (Theme::text().withAlpha (0.2f)); g.drawHorizontalLine ((int) ky, (float) cell.getX(), (float) cell.getRight()); }
                }
                if (on)
                    for (auto& cn : chord)
                    {
                        const float ny = (float) cell.getBottom() - (float) (cn.getIntValue() - (int) lo + 0.5f) / 48.0f * cell.getHeight();
                        g.setColour (t->getColour().brighter (0.3f)); g.fillRoundedRectangle ((float) cell.getX() + 1.0f, ny - 2.0f, (float) cell.getWidth() - 2.0f, 4.0f, 1.5f);
                    }
                if (on) { g.setColour (Theme::text()); g.setFont (Theme::font (7.5f, true)); g.drawText (laneText (v, t), cell.withTrimmedBottom (2), juce::Justification::centredBottom); }
                continue;
            }
            const float vy = bipolar ? zeroY - (norm - 0.5f) * in.getHeight() : (float) in.getBottom() - norm * in.getHeight();
            auto bar = juce::Rectangle<float> ((float) cell.getX() + 2.0f, juce::jmin (vy, zeroY), (float) cell.getWidth() - 4.0f, std::abs (zeroY - vy));
            g.setColour (on ? t->getColour() : Theme::dim().withAlpha (0.4f)); g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (Theme::text().withAlpha (on ? 0.9f : 0.4f)); g.setFont (Theme::font (7.5f, true));
            g.drawText (laneText (v, t), cell.withTrimmedBottom (2), juce::Justification::centredBottom);
        }
        if (bipolar) { g.setColour (Theme::text().withAlpha (0.25f)); g.drawHorizontalLine ((int) zeroY, (float) leftW, (float) getWidth()); }
    }

    int graphStepAt (int x)   { const int localX = x + viewport.getViewPositionX(); return rows.stepAt (localX); }
    bool inGraph (juce::Point<int> p) const   { return graphVisible && graphBody.contains (p) && p.x >= leftW; }

    int graphDragStep = -1; float graphDragStart = 0; bool graphRamp = false; int graphRampFrom = -1; float graphRampFromV = 0;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! inGraph (e.getPosition())) return;
        auto t = selected(); if (! t) return;
        const int s = graphStepAt (e.x); if (s < 0) return;
        float lo, hi, def; laneRange (lo, hi, def, t);
        if (e.mods.isAltDown()) { setLaneValue (*t, s, def); if (graphLane == Lane::note) Rack::setExtraLaneValue (*t, pat(), "note", s, "0"); Rack::syncPatternClips (edit(), pat()); repaint(); return; }
        if (graphLane == Lane::note && e.mods.isShiftDown())
        {
            // add a chord note at the clicked pitch
            const int note = (int) lo + juce::jlimit (0, 47, (int) ((graphBody.reduced (0, 6).getBottom() - e.y) / (float) graphBody.reduced (0, 6).getHeight() * 48.0f));
            auto toks = Rack::extraLane (*t, pat(), "note");
            juce::String cur = juce::isPositiveAndBelow (s, toks.size()) && toks[s].getIntValue() != 0 ? toks[s] : juce::String (Rack::rootNote (*t));
            auto parts = juce::StringArray::fromTokens (cur, "+", ""); if (! parts.contains (juce::String (note)) && parts.size() < 4) parts.add (juce::String (note));
            Rack::setExtraLaneValue (*t, pat(), "note", s, parts.joinIntoString ("+"));
            if (! Rack::step (*t, pat(), s)) Rack::setStep (*t, pat(), s, true);
            Rack::syncPatternClips (edit(), pat()); repaint(); return;
        }
        graphRamp = e.mods.isPopupMenu(); graphRampFrom = s; graphRampFromV = valueAtY (e.y, t);
        graphDragStep = s; graphDragStart = laneValue (*t, s);
        applyGraphPoint (e, *t, s);
    }
    float valueAtY (int y, te::AudioTrack* t)
    {
        float lo, hi, def; laneRange (lo, hi, def, t);
        auto in = graphBody.reduced (0, 6);
        return lo + juce::jlimit (0.0f, 1.0f, (float) (in.getBottom() - y) / (float) in.getHeight()) * (hi - lo);
    }
    void applyGraphPoint (const juce::MouseEvent& e, te::AudioTrack& t, int s)
    {
        float v = valueAtY (e.y, &t);
        if (graphLane == Lane::note) { v = std::round (v); if (! Rack::step (t, pat(), s)) Rack::setStep (t, pat(), s, true); }
        if (graphLane == Lane::repeat) v = std::round (v);
        setLaneValue (t, s, v);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (graphDragStep < 0) return;
        auto t = selected(); if (! t) return;
        const int s = graphStepAt (juce::jlimit (leftW, getWidth() - 1, e.x)); if (s < 0) return;
        if (e.mods.isCtrlDown())
        {
            // scale the whole lane around the start value
            float lo, hi, def; laneRange (lo, hi, def, t);
            const float target = valueAtY (e.y, t), ratio = graphDragStart > 1.0e-4f ? target / graphDragStart : 1.0f;
            for (int k = 0; k < Rack::patternSteps (edit(), pat()); ++k) setLaneValue (*t, k, juce::jlimit (lo, hi, laneValue (*t, k) * ratio));
            repaint(); return;
        }
        if (graphRamp)
        {
            const float v1 = valueAtY (e.y, t); const int a = juce::jmin (graphRampFrom, s), b = juce::jmax (graphRampFrom, s);
            for (int k = a; k <= b; ++k)
            {
                const float f = b == a ? 1.0f : (float) (k - graphRampFrom) / (float) (s - graphRampFrom);
                float v = graphRampFromV + (v1 - graphRampFromV) * juce::jlimit (0.0f, 1.0f, f);
                if (graphLane == Lane::repeat || graphLane == Lane::note) v = std::round (v);
                setLaneValue (*t, k, v);
            }
            repaint(); return;
        }
        applyGraphPoint (e, *t, s);
        graphDragStep = s; repaint();
    }
    void mouseUp (const juce::MouseEvent&) override   { if (graphDragStep >= 0) Rack::syncPatternClips (edit(), pat()); graphDragStep = -1; graphRamp = false; }

    //==========================================================================
    // Menus
    void showPatternPicker()
    {
        juce::PopupMenu m;
        for (int i = 0; i < Rack::numPatterns (edit()); ++i) m.addItem (i + 1, juce::String (i + 1) + "  " + Rack::patternName (edit(), i) + (Rack::patternHasNotes (edit(), i) ? "" : "   (empty)"), true, i == pat());
        m.addSeparator(); m.addItem (1000, "New pattern"); m.addItem (1001, "Clone this pattern");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patName), [this] (int r)
        {
            if (r == 1000) { Rack::selectPattern (edit(), Rack::newPattern (edit())); }
            else if (r == 1001) { Rack::selectPattern (edit(), Rack::clonePattern (edit(), pat())); }
            else if (r > 0) Rack::selectPattern (edit(), r - 1);
            refresh();
        });
    }
    void showPatternMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename..."); m.addItem (2, "Colour...");
        m.addItem (3, "Clone"); m.addItem (4, "Delete", Rack::numPatterns (edit()) > 1);
        m.addSeparator();
        m.addItem (5, "Place on the timeline at the playhead"); m.addItem (6, "Place on the timeline at bar 1, repeat 4x");
        m.addSeparator();
        m.addItem (7, "Select unused patterns..."); m.addItem (8, "Clear this pattern");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patName), [this] (int r)
        {
            const int p = pat();
            if (r == 1) askText ("Rename Pattern", "Name", Rack::patternName (edit(), p), [this, p] (const juce::String& s) { Rack::setPatternName (edit(), p, s); for (auto t : channels()) if (auto c = Rack::slotClip (*t, p)) c->setName (s); refresh(); });
            else if (r == 2) pickColour (Rack::patternColour (edit(), p), [this, p] (juce::Colour c) { Rack::setPatternColour (edit(), p, c); refresh(); });
            else if (r == 3) Rack::selectPattern (edit(), Rack::clonePattern (edit(), p));
            else if (r == 4) Rack::deletePattern (edit(), p);
            else if (r == 5) Rack::placePattern (session, p, session.timeToBar (edit().getTransport().getPosition()), 1);
            else if (r == 6) Rack::placePattern (session, p, 1.0, 4);
            else if (r == 7) { juce::StringArray empty; for (int i = 0; i < Rack::numPatterns (edit()); ++i) if (! Rack::patternHasNotes (edit(), i)) empty.add (Rack::patternName (edit(), i)); Ui::message (juce::MessageBoxIconType::InfoIcon, "Unused patterns", empty.isEmpty() ? "Every pattern has notes." : empty.joinIntoString (", ")); }
            else if (r == 8) { for (auto t : channels()) if (auto sc = Rack::stepClip (*t, p, false)) { auto pp = sc->getPattern (0); BeatLab::clearRow (*sc, pp, 0); } Rack::syncPatternClips (edit(), p); }
            refresh();
        });
    }
    void showAddMenu()
    {
        juce::PopupMenu m, inst, kits, samples;
        int id = 1; juce::StringArray instNames;
        for (auto& e : Signature::entries()) if (juce::String (e.category) == "Instruments" && juce::String (e.type) != "sampler") { inst.addItem (id++, Edition::isTypeAllowed (e.type) ? juce::String (e.name) : juce::String (e.name) + "   [locked - " + Edition::productForType (e.type)->name + "]"); instNames.add (e.name); }
        inst.addSeparator();
        juce::StringArray thirdParty;
        for (auto& d : session.engine.getPluginManager().knownPluginList.getTypes()) if (d.isInstrument) { thirdParty.add (d.name); }
        thirdParty.sort (true);
        for (auto& n : thirdParty) { inst.addItem (id++, n); instNames.add (n); }
        m.addSubMenu ("Instrument", inst);
        const int kitBase = 500;
        for (int k = 0; k < DrumKit::numKits; ++k) { juce::PopupMenu one; one.addItem (kitBase + k * 2, "8-channel starter (kick, snare, clap, hats, perc, 808, rim)"); one.addItem (kitBase + k * 2 + 1, "All 12 sounds"); kits.addSubMenu (DrumKit::kits[k].name, one); }
        m.addSubMenu ("Kit (sample channels)", kits);
        m.addItem (900, "Sample channel from file...");
        m.addItem (901, "Layer channel"); m.addItem (902, "MIDI Out channel");
        m.addSeparator();
        juce::PopupMenu existing; int ex = 1000;
        for (auto t : te::getAudioTracks (edit())) if (! Rack::isRackTrack (*t) && Rack::instrumentOf (*t) != nullptr) existing.addItem (ex++, t->getName());
        m.addSubMenu ("Add existing track to the rack", existing, ex > 1000);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addBtn), [this, instNames] (int r)
        {
            if (r <= 0) return;
            if (r < 500) Rack::addInstrumentChannel (session, instNames[r - 1]);
            else if (r < 900) { const int k = (r - 500) / 2; Rack::addKitChannels (session, k, (r - 500) % 2 == 0 ? 8 : -1); }
            else if (r == 900)
            {
                auto fc = std::make_shared<juce::FileChooser> ("Sample channel", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
                fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [this, fc] (const juce::FileChooser& c) { for (auto& f : c.getResults()) Rack::addSampleChannel (session, f); refresh(); });
                return;
            }
            else if (r == 901) Rack::addLayerChannel (session);
            else if (r == 902) Rack::addMidiOutChannel (session);
            else if (r >= 1000) { int ex = 1000; for (auto t : te::getAudioTracks (edit())) if (! Rack::isRackTrack (*t) && Rack::instrumentOf (*t) != nullptr) { if (ex++ == r) Rack::tagAsRackTrack (*t, dynamic_cast<te::ExternalPlugin*> (Rack::instrumentOf (*t)) ? "plugin" : "instrument", 60); } }
            selectedRow = channels().size() - 1;
            refresh();
        });
    }
    void showStarterMenu()
    {
        juce::PopupMenu m; int id = 1;
        for (auto& g : BeatLab::genres()) m.addItem (id++, g.name);
        m.addSeparator(); m.addItem (999, "Dice again (same genre)", lastGenre >= 0);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&starterBtn), [this] (int r)
        {
            if (r <= 0) return;
            if (r == 999) r = lastGenre + 1;
            lastGenre = r - 1;
            const auto& g = BeatLab::genres()[(size_t) juce::jlimit (0, (int) BeatLab::genres().size() - 1, r - 1)];
            if (channels().isEmpty()) Rack::addKitChannels (session, g.kit, 8);
            juce::Random rnd;
            for (auto t : channels())
            {
                if ((bool) t->state.getProperty ("sqmRackLock", false)) continue;
                int rowIdx = t->state.hasProperty ("sqmKitSound") ? (int) t->state.getProperty ("sqmKitSound") : -1;
                if (rowIdx < 0) for (int k = 0; k < DrumKit::numSounds; ++k) if (t->getName().containsIgnoreCase (DrumKit::sounds[k].name)) { rowIdx = k; break; }
                if (rowIdx < 0) continue;
                for (auto& ri : g.rows)
                {
                    if (ri.channel != rowIdx || ri.options.empty()) continue;
                    if (auto sc = Rack::stepClip (*t, pat(), true)) { auto pp = sc->getPattern (0); BeatLab::clearRow (*sc, pp, 0); BeatLab::writeGrooveString (*sc, pp, 0, ri.options[(size_t) rnd.nextInt ((int) ri.options.size())], rnd); }
                    break;
                }
            }
            Rack::setPatternSwing (edit(), pat(), g.swing);
            Rack::syncPatternClips (edit(), pat());
            refresh();
        });
    }
    int lastGenre = -1;

    void showStepMenu (int row, int s)
    {
        auto chans = channels(); if (! juce::isPositiveAndBelow (row, chans.size())) return;
        auto& t = *chans[row]; const int p = pat();
        juce::PopupMenu m;
        juce::PopupMenu roll; for (int r : { 1, 2, 3, 4, 6, 8 }) roll.addItem (100 + r, r == 1 ? "No roll" : "x" + juce::String (r)); m.addSubMenu ("Roll (repeat)", roll);
        juce::PopupMenu ch; for (int c : { 100, 75, 50, 25 }) ch.addItem (200 + c, juce::String (c) + "%"); m.addSubMenu ("Chance", ch);
        juce::PopupMenu vel; for (int v : { 127, 110, 100, 80, 60, 40 }) vel.addItem (300 + v, juce::String (v)); m.addSubMenu ("Velocity", vel);
        m.addItem (400, "Accent (127)"); m.addItem (401, "Ghost (50)");
        m.showMenuAsync (juce::PopupMenu::Options(), [this, &t, p, s] (int r)
        {
            if (r <= 0) return;
            if (! Rack::step (t, p, s)) Rack::setStep (t, p, s, true);
            if (r > 100 && r < 200) Rack::setLane (t, p, BeatLab::Lane::repeat, s, (float) (r - 100));
            else if (r > 200 && r < 300) Rack::setLane (t, p, BeatLab::Lane::chance, s, (float) (r - 200) / 100.0f);
            else if (r > 300 && r < 400) Rack::setLane (t, p, BeatLab::Lane::velocity, s, (float) (r - 300));
            else if (r == 400) Rack::setLane (t, p, BeatLab::Lane::velocity, s, 127);
            else if (r == 401) Rack::setLane (t, p, BeatLab::Lane::velocity, s, 50);
            Rack::syncPatternClips (edit(), p); repaint();
        });
    }

    void showChannelMenu (int row)
    {
        auto chans = channels(); if (! juce::isPositiveAndBelow (row, chans.size())) return;
        auto tp = te::Track::Ptr (chans[row]); const int p = pat();
        auto& t = *chans[row];
        juce::PopupMenu m;
        m.addItem (1, "Piano roll"); m.addItem (2, "Graph editor"); m.addItem (3, "Channel settings...");
        m.addSeparator();
        m.addItem (10, "Rename..."); m.addItem (11, "Colour..."); m.addItem (12, "Load sample...");
        juce::PopupMenu rep; int id = 100; juce::StringArray instNames;
        for (auto& e : Signature::entries()) if (juce::String (e.category) == "Instruments" && juce::String (e.type) != "sampler") { rep.addItem (id++, Edition::isTypeAllowed (e.type) ? juce::String (e.name) : juce::String (e.name) + "   [locked - " + Edition::productForType (e.type)->name + "]"); instNames.add (e.name); }
        m.addSubMenu ("Replace instrument", rep);
        m.addSeparator();
        m.addItem (20, "Insert channel above"); m.addItem (21, "Clone"); m.addItem (22, "Delete channel (removes the track)");
        m.addSeparator();
        m.addItem (30, Rack::midiClip (t, p) ? "Back to steps" : "Send to piano roll");
        m.addItem (31, "Burn to MIDI (arpeggiator / echo -> notes)");
        m.addSeparator();
        juce::PopupMenu fill; fill.addItem (40, "Fill each 2 steps"); fill.addItem (41, "Fill each 4 steps"); fill.addItem (42, "Fill each 8 steps"); fill.addItem (43, "Euclidean..."); m.addSubMenu ("Fill", fill);
        m.addItem (44, "Rotate left"); m.addItem (45, "Rotate right"); m.addItem (46, "Dice (random steps)"); m.addItem (47, "Humanise velocities"); m.addItem (48, "Clear steps");
        m.addItem (49, "Copy"); m.addItem (50, "Paste");
        m.addSeparator();
        juce::PopupMenu len; for (int l : { 0, 3, 5, 6, 7, 12 }) len.addItem (60 + l, l == 0 ? "Pattern length" : juce::String (l) + " steps (polymeter)"); m.addSubMenu ("Channel length", len);
        juce::PopupMenu tr; tr.addItem (70, "Octave up"); tr.addItem (71, "Octave down"); tr.addItem (72, "Semitone up"); tr.addItem (73, "Semitone down"); tr.addItem (74, "Set root note..."); m.addSubMenu ("Transpose / root note", tr);
        juce::PopupMenu grp; grp.addItem (80, "No group"); grp.addItem (81, "Drums"); grp.addItem (82, "Bass"); grp.addItem (83, "Melody"); grp.addItem (84, "FX"); grp.addItem (85, "New group..."); m.addSubMenu ("Group", grp);
        m.addItem (90, Rack::isZipped (t) ? "Unzip" : "Zip"); m.addItem (91, "Move up (Alt+Up)"); m.addItem (92, "Move down (Alt+Down)");
        m.addSeparator();
        juce::PopupMenu sort; sort.addItem (95, "Sort by colour"); sort.addItem (96, "Sort by name"); sort.addItem (97, "Sort by type"); m.addSubMenu ("Sort channels", sort);
        m.addItem (98, "Leave the rack (keep the track)");
        m.showMenuAsync (juce::PopupMenu::Options(), [this, tp, p, row, instNames] (int r)
        {
            auto t = dynamic_cast<te::AudioTrack*> (tp.get()); if (t == nullptr || r <= 0) return;
            auto sc = Rack::stepClip (*t, p, false);
            std::optional<te::StepClip::Pattern> pp; if (sc) pp.emplace (sc->getPattern (0));
            juce::Random rnd;
            switch (r)
            {
                case 1: if (auto mc = Rack::sendToPianoRoll (*t, p)) if (onOpenPianoRoll) onOpenPianoRoll (*mc); break;
                case 2: setGraphVisible (true); break;
                case 3: if (onOpenChannelSettings) onOpenChannelSettings (*t); break;
                case 10: askText ("Rename Channel", "Name", t->getName(), [tp] (const juce::String& s) { tp->setName (s); }); break;
                case 11: pickColour (t->getColour(), [tp] (juce::Colour c) { tp->setColour (c); }); break;
                case 12:
                {
                    auto fc = std::make_shared<juce::FileChooser> ("Load sample", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
                    fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc, tp] (const juce::FileChooser& c)
                    {
                        auto at = dynamic_cast<te::AudioTrack*> (tp.get()); if (! at || c.getResult() == juce::File()) return;
                        Rack::setChannelSample (*at, c.getResult(), c.getResult().getFileNameWithoutExtension(), Rack::rootNote (*at), 0.0f);
                        at->state.setProperty ("sqmRackType", "sample", nullptr);
                        refresh();
                    });
                    return;
                }
                case 20: { Rack::addInstrumentChannel (session, "Mothsynth 2"); auto ch = channels(); if (! ch.isEmpty()) Rack::moveChannel (edit(), *ch.getLast(), row); break; }
                case 21: Rack::cloneChannel (session, *t); break;
                case 22: Rack::removeChannel (session, *t); selectedRow = juce::jmax (0, row - 1); break;
                case 30: if (Rack::midiClip (*t, p)) Rack::sendToSteps (*t, p); else if (auto mc = Rack::sendToPianoRoll (*t, p)) if (onOpenPianoRoll) onOpenPianoRoll (*mc); break;
                case 31: { if (! Rack::midiClip (*t, p)) Rack::sendToPianoRoll (*t, p); auto r = BurnToMidi::burn (*t); if (r.error.isNotEmpty()) Ui::message (juce::MessageBoxIconType::InfoIcon, "Burn to MIDI", r.error); break; }
                case 40: case 41: case 42: if (pp) BeatLab::fillEvery (*sc, *pp, 0, r == 40 ? 2 : r == 41 ? 4 : 8); break;
                case 43: if (pp) BeatLab::fillEuclid (*sc, *pp, 0, 5, pp->getNumNotes(), 0); break;
                case 44: if (pp) BeatLab::rotateRow (*sc, *pp, 0, -1); break;
                case 45: if (pp) BeatLab::rotateRow (*sc, *pp, 0, 1); break;
                case 46: if (pp) BeatLab::diceRow (*sc, *pp, 0, 0.4f, rnd); break;
                case 47: if (pp) BeatLab::humanise (*sc, *pp, 0, 0.3f, rnd); break;
                case 48: if (pp) { BeatLab::clearRow (*sc, *pp, 0); for (auto lane : { "note", "release", "pitch", "pan", "modx", "mody" }) Rack::clearExtraLane (*t, p, lane); } break;
                case 49: if (pp) BeatLab::copyRow (*pp, 0); break;
                case 50: if (sc) BeatLab::paste (*sc, 0); break;
                case 70: Rack::setRootNote (*t, Rack::rootNote (*t) + 12); break;
                case 71: Rack::setRootNote (*t, Rack::rootNote (*t) - 12); break;
                case 72: Rack::setRootNote (*t, Rack::rootNote (*t) + 1); break;
                case 73: Rack::setRootNote (*t, Rack::rootNote (*t) - 1); break;
                case 74: askText ("Root Note", "Note (e.g. C4 or 60)", juce::MidiMessage::getMidiNoteName (Rack::rootNote (*t), true, true, 4), [this, tp] (const juce::String& s) { if (auto at = dynamic_cast<te::AudioTrack*> (tp.get())) { Rack::setRootNote (*at, parseNote (s)); applyRoot (*at); } }); break;
                case 80: Rack::setGroup (*t, {}); break;
                case 81: Rack::setGroup (*t, "Drums"); break; case 82: Rack::setGroup (*t, "Bass"); break; case 83: Rack::setGroup (*t, "Melody"); break; case 84: Rack::setGroup (*t, "FX"); break;
                case 85: askText ("New Group", "Group name", "", [tp] (const juce::String& s) { if (auto at = dynamic_cast<te::AudioTrack*> (tp.get())) Rack::setGroup (*at, s); }); break;
                case 90: Rack::setZipped (*t, ! Rack::isZipped (*t)); break;
                case 91: Rack::moveChannel (edit(), *t, row - 1); selectedRow = juce::jmax (0, row - 1); break;
                case 92: Rack::moveChannel (edit(), *t, row + 1); selectedRow = row + 1; break;
                case 95: case 96: case 97: sortChannels (r); break;
                case 98: Rack::setRackTrack (*t, false); break;
                default:
                    if (r >= 100 && r < 100 + instNames.size())
                    {
                        auto se = Signature::findByName (instNames[r - 100]);
                        if (se && ! Edition::isTypeAllowed (se->type)) { if (Edition::showUnlock()) Edition::showUnlock() (Edition::productForType (se->type)->code); break; }
                        if (se) { if (auto old = Rack::instrumentOf (*t)) old->deleteFromParent(); if (auto np = edit().getPluginCache().createNewPlugin (se->type, {})) { t->pluginList.insertPlugin (np, 0, nullptr); if (! se->presets.empty()) Signature::applyPreset (*np, se->presets.front()); } t->state.setProperty ("sqmRackType", "instrument", nullptr); }
                    }
                    break;
            }
            if (r >= 70 && r <= 73) applyRoot (*t);
            if (sc) BeatLab::rebuildSequence (*sc);
            Rack::syncPatternClips (edit(), p);
            refresh();
        });
    }
    std::function<void (te::AudioTrack&, int)> onBurnToMidi;

    void applyRoot (te::AudioTrack& t)
    {
        for (int i = 0; i < Rack::numPatterns (edit()); ++i)
            if (auto sc = Rack::stepClip (t, i, false)) { sc->getChannels()[0]->noteNumber = Rack::rootNote (t); BeatLab::rebuildSequence (*sc); }
    }
    static int parseNote (const juce::String& s)
    {
        if (s.containsOnly ("0123456789")) return juce::jlimit (0, 127, s.getIntValue());
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        auto u = s.trim().toUpperCase().replace ("DB", "C#").replace ("EB", "D#").replace ("GB", "F#").replace ("AB", "G#").replace ("BB", "A#");
        int pc = 0; int len = 1;
        for (int i = 11; i >= 0; --i) if (u.startsWith (names[i])) { pc = i; len = (int) std::strlen (names[i]); break; }
        const int oct = u.substring (len).getIntValue();
        return juce::jlimit (0, 127, (oct + 1) * 12 + pc);
    }
    void sortChannels (int by)
    {
        auto ch = channels();
        std::vector<te::AudioTrack*> v (ch.begin(), ch.end());
        std::stable_sort (v.begin(), v.end(), [by] (te::AudioTrack* a, te::AudioTrack* b)
        {
            if (by == 95) return a->getColour().getHue() < b->getColour().getHue();
            if (by == 96) return a->getName().compareIgnoreCase (b->getName()) < 0;
            return Rack::channelType (*a).compare (Rack::channelType (*b)) < 0;
        });
        for (size_t i = 0; i < v.size(); ++i) Rack::moveChannel (edit(), *v[i], (int) i);
    }

    void askText (const juce::String& title, const juce::String& label, const juce::String& initial, std::function<void (const juce::String&)> fn)
    {
        auto aw = std::make_shared<juce::AlertWindow> (title, "", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("t", initial, label);
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, fn, this] (int r) { if (r == 1) fn (aw->getTextEditorContents ("t")); refresh(); }), false);
    }
    void pickColour (juce::Colour initial, std::function<void (juce::Colour)> fn)
    {
        auto cs = std::make_unique<juce::ColourSelector> (juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders | juce::ColourSelector::showColourAtTop);
        cs->setCurrentColour (initial); cs->setSize (260, 300);
        struct L : public juce::ChangeListener { std::function<void (juce::Colour)> fn; void changeListenerCallback (juce::ChangeBroadcaster* b) override { if (auto c = dynamic_cast<juce::ColourSelector*> (b)) fn (c->getCurrentColour()); } };
        auto l = new L(); l->fn = fn; cs->addChangeListener (l);
        auto& box = juce::CallOutBox::launchAsynchronously (std::move (cs), patName.getScreenBounds(), nullptr);
        box.setDismissalMouseClicksAreAlwaysConsumed (true);
    }
};

//==============================================================================
class ChannelRackWindow : public juce::DocumentWindow
{
public:
    explicit ChannelRackWindow (Session& s)
        : DocumentWindow ("Channel Rack", Theme::bg(), DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        rack = new ChannelRackComponent (s);
        setContentOwned (rack, true);
        setResizable (true, true);
        setResizeLimits (720, 300, 4000, 3000);
        centreWithSize (1140, 520);
        setVisible (true);
        toFront (true);
    }
    void closeButtonPressed() override  { if (onClose) onClose(); }
    std::function<void()> onClose;
    ChannelRackComponent* rack = nullptr;
};
