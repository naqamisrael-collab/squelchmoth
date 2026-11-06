#pragma once
// Squelchmoth - the Mix window: Pro Tools-style channel strips.
//
// Top to bottom like a Pro Tools strip: colour bar, name, automation mode, Inserts A-E (F-J optional),
// Sends A-E (F-J optional), I/O, pan (single or stereo L/R), long-throw fader with a dB scale next to a
// peak-hold meter, volume readout, Rec / Solo / Mute. Every insert and send slot has a menu (arrow or
// right-click) to remove, bypass, move, replace, copy and paste. Empty slots click to add.

#include "TrackSlots.h"
#include "TrackOps.h"
#include "QuickNew.h"
#include "EditState.h"

//==============================================================================
class LevelMeter : public juce::Component
{
public:
    void setMeasurer (te::LevelMeasurer* m, bool oldOneStillExists = true)
    {
        if (m == measurer) return;
        if (measurer != nullptr && oldOneStillExists) measurer->removeClient (client);
        measurer = m;
        if (measurer != nullptr) measurer->addClient (client);
    }

    ~LevelMeter() override   { setMeasurer (nullptr); }

    void tick()
    {
        if (seenReset != clipIndicatorResetCounter()) resetPeak();
        if (measurer == nullptr) return;
        const float decay = (float) Prefs::meterDecayDbPerSec() / 30.0f;
        const int holdTicksMax = juce::jmax (1, Prefs::meterHoldMs() / 33);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto l = client.getAndClearAudioLevel (ch).dB;
            level[ch] = juce::jmax (l, level[ch] - decay);
            if (l > peakHold[ch]) { peakHold[ch] = l; holdTicks[ch] = holdTicksMax; }
            else if (--holdTicks[ch] <= 0) peakHold[ch] = juce::jmax (-100.0f, peakHold[ch] - 0.8f);
            if (l >= 0.0f) clip = true;
            maxPeak = juce::jmax (maxPeak, l);
        }
        repaint();
    }

    float getMaxPeak() const        { return maxPeak; }
    void resetPeak()                { maxPeak = -100.0f; clip = false; seenReset = clipIndicatorResetCounter(); }
    int seenReset = clipIndicatorResetCounter();
    void mouseDown (const juce::MouseEvent&) override   { resetPeak(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::bg());
        g.fillRoundedRectangle (r, 2.0f);
        auto clipLed = r.removeFromTop (5.0f);
        g.setColour (clip ? Theme::red() : Theme::line());
        g.fillRect (clipLed.reduced (1.0f, 0.0f));
        r.removeFromTop (2.0f);

        auto w = (r.getWidth() - 3.0f) / 2.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto bar = juce::Rectangle<float> (r.getX() + 1.0f + ch * (w + 1.0f), r.getY(), w, r.getHeight());
            auto prop = dbToProp (level[ch]);
            auto filled = bar.withTop (bar.getBottom() - bar.getHeight() * prop);
            juce::ColourGradient grad (Theme::mint(), 0, bar.getBottom(), Theme::red(), 0, bar.getY(), false);
            grad.addColour (0.75, Theme::acid());
            grad.addColour (0.9, Theme::amber());
            g.setGradientFill (grad);
            g.fillRect (filled);
            auto py = bar.getBottom() - bar.getHeight() * dbToProp (peakHold[ch]);
            g.setColour (Theme::text());
            g.fillRect (bar.getX(), py, bar.getWidth(), 1.5f);
        }
    }

    // meter scale: +6 .. -60 dB, same curve as the fader so the two line up
    static float dbToProp (float db)    { return juce::jlimit (0.0f, 1.0f, std::pow ((db + 60.0f) / 66.0f, 1.6f)); }

private:
    te::LevelMeasurer* measurer = nullptr;
    te::LevelMeasurer::Client client;
    float level[2] = { -100.0f, -100.0f }, peakHold[2] = { -100.0f, -100.0f }, maxPeak = -100.0f;
    int holdTicks[2] = { 0, 0 };
    bool clip = false;
};

//==============================================================================
/** Fader scale drawn beside the fader: +12, +6, 0, -5, -10, -20, -30, -40, -60 like a console. */
class FaderScale : public juce::Component
{
public:
    juce::Slider* fader = nullptr;
    void paint (juce::Graphics& g) override
    {
        if (fader == nullptr) return;
        g.setFont (Theme::font (8.5f));
        for (float db : { 6.0f, 0.0f, -5.0f, -10.0f, -20.0f, -30.0f, -40.0f, -60.0f })
        {
            auto prop = fader->valueToProportionOfLength (db);
            auto y = (float) fader->getPositionOfValue (db) + (float) fader->getY() - (float) getY();
            juce::ignoreUnused (prop);
            g.setColour (db == 0.0f ? Theme::text() : Theme::dim());
            g.fillRect (getWidth() - 5.0f, y, 4.0f, 1.0f);
            g.drawText (db == 0.0f ? "0" : juce::String ((int) db), 0, (int) y - 6, getWidth() - 7, 12, juce::Justification::centredRight);
        }
    }
};

//==============================================================================
/** Pro Tools-style pan knob: shows "<100", "C", "R32"; double-click centres; drag vertically. */
class PanKnob : public juce::Slider
{
public:
    PanKnob()
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setRange (-1.0, 1.0, 0.01);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setDoubleClickReturnValue (true, 0.0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setPopupDisplayEnabled (false, false, nullptr);
    }
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
    static juce::String panText (double v)
    {
        if (std::abs (v) < 0.005) return "C";
        return v < 0 ? "<" + juce::String (juce::roundToInt (-v * 100)) : juce::String (juce::roundToInt (v * 100)) + ">";
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float side = juce::jmin (r.getWidth(), r.getHeight() - 12.0f);
        auto knob = r.removeFromTop (r.getHeight() - 12.0f).withSizeKeepingCentre (side, side).reduced (2.0f);
        auto centre = knob.getCentre();
        auto radius = knob.getWidth() / 2.0f;
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        auto prop = (float) valueToProportionOfLength (getValue());
        auto angle = a0 + prop * (a1 - a0);
        g.setColour (Theme::panel2().darker (0.3f));
        g.fillEllipse (knob);
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, a0, a1, true);
        g.setColour (Theme::line());
        g.strokePath (arc, juce::PathStrokeType (2.0f));
        juce::Path fill;
        auto mid = (a0 + a1) * 0.5f;
        fill.addCentredArc (centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, juce::jmin (mid, angle), juce::jmax (mid, angle), true);
        g.setColour (accent);
        g.strokePath (fill, juce::PathStrokeType (2.5f));
        auto tip = centre.getPointOnCircumference (radius - 4.0f, angle);
        g.setColour (Theme::text());
        g.drawLine (centre.x, centre.y, tip.x, tip.y, 2.0f);
        g.fillEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
        g.setFont (Theme::font (9.5f, true));
        g.setColour (Theme::text());
        g.drawText (panText (getValue()), getLocalBounds().removeFromBottom (12), juce::Justification::centred);
    }
    juce::Colour accent { Theme::acid() };
};

//==============================================================================
/** Console-style fader cap and track for the strip faders. */
struct FaderLookAndFeel : public juce::LookAndFeel_V4
{
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto track = juce::Rectangle<float> ((float) x + (float) w * 0.5f - 2.0f, (float) y, 4.0f, (float) h);
        g.setColour (Theme::bg().darker (0.4f));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (Theme::line());
        g.drawRoundedRectangle (track, 2.0f, 1.0f);
        // unity marker
        auto unityY = (float) s.getPositionOfValue (0.0);
        g.setColour (Theme::dim());
        g.fillRect ((float) x + 2.0f, unityY, (float) w - 4.0f, 1.0f);
        // cap
        auto capW = juce::jmin ((float) w - 2.0f, 26.0f), capH = 34.0f;
        juce::Rectangle<float> cap ((float) x + (float) w * 0.5f - capW * 0.5f, pos - capH * 0.5f, capW, capH);
        juce::ColourGradient grad (Theme::panel2().brighter (0.35f), cap.getX(), cap.getY(), Theme::panel2().darker (0.5f), cap.getRight(), cap.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (Theme::bg().darker (0.6f));
        g.drawRoundedRectangle (cap, 3.0f, 1.0f);
        g.setColour (s.getValue() > 0.05 ? Theme::amber() : Theme::acid());
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
        g.setColour (Theme::line().withAlpha (0.5f));
        for (float dy : { -10.0f, -6.0f, 6.0f, 10.0f }) g.fillRect (cap.getX() + 5.0f, cap.getCentreY() + dy, cap.getWidth() - 10.0f, 1.0f);
    }
    int getSliderThumbRadius (juce::Slider&) override   { return 17; }
};

inline FaderLookAndFeel& faderLookAndFeel()   { static FaderLookAndFeel l; return l; }

//==============================================================================
class ChannelStrip : public juce::Component,
                     public SlotController
{
public:
    ChannelStrip (Session& s, te::Track* t, TimelineState* st = nullptr) : SlotController (s, t), keepAlive (t), state (st)
    {
        name.setJustificationType (juce::Justification::centred);
        name.setFont (Theme::font (12.5f, true));
        name.setEditable (false, row != nullptr);
        name.onTextChange = [this] { if (row) row->setName (name.getText()); };
        name.setTooltip ("Double-click to rename");
        addAndMakeVisible (name);

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setRange (-70.0, 12.0, 0.1);
        fader.setSkewFactorFromMidPoint (-10.0);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.onValueChange = [this]
        {
            if (track) session.setTrackVolume (*track, (float) fader.getValue());
            else if (folder && folder->getVCAPlugin()) folder->getVCAPlugin()->setVolumeDb ((float) fader.getValue());
            else if (auto vp = getVolumePlugin()) vp->setVolumeDb ((float) fader.getValue());
            volText.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
        };
        fader.onDragStart = [this] { if (auto p = volParam()) p->parameterChangeGestureBegin(); };
        fader.onDragEnd   = [this] { if (auto p = volParam()) p->parameterChangeGestureEnd(); };
        fader.setLookAndFeel (&faderLookAndFeel());
        fader.setTooltip ("Volume fader - double-click for 0 dB, Cmd-drag for fine control");
        addAndMakeVisible (fader);
        scale.fader = &fader;
        addAndMakeVisible (scale);

        volText.setJustificationType (juce::Justification::centred);
        volText.setFont (Theme::font (10.5f, true));
        volText.setEditable (false, true);
        volText.setColour (juce::Label::backgroundColourId, Theme::bg());
        volText.setTooltip ("Fader level - double-click to type a value");
        volText.onTextChange = [this] { fader.setValue (volText.getText().retainCharacters ("-0123456789.").getDoubleValue()); };
        addAndMakeVisible (volText);

        peakText.setJustificationType (juce::Justification::centred);
        peakText.setFont (Theme::font (9.0f));
        peakText.setColour (juce::Label::textColourId, Theme::dim());
        peakText.setTooltip ("Peak hold - click the meter to reset");
        addAndMakeVisible (peakText);

        for (auto* k : { &panL, &panR })
        {
            k->onValueChange = [this, k] { panChanged (k); };
            k->onRightClick = [this, k] { showPanMenu (k); };
            k->setTooltip ("Pan - drag, double-click to centre, right-click for pan depth and stereo pan");
            addAndMakeVisible (*k);
        }
        addAndMakeVisible (meter);

        if (row != nullptr)
        {
            for (auto* b : { &solo, &mute }) addAndMakeVisible (b);
            if (track != nullptr && IO::kindOf (*track) != IO::Kind::aux) { addAndMakeVisible (rec); addAndMakeVisible (mon); }
            mon.setColour (juce::TextButton::buttonOnColourId, Theme::mint().darker (0.3f));
            mon.setTooltip ("Input Monitor - hear this track's live input (this track only)");
            mon.onClick = [this] { if (track) TrackOps::setInputMonitor (session, *track, ! TrackOps::isInputMonitorOn (*track)); refresh(); };
            rec.setColour (juce::TextButton::buttonOnColourId, Theme::red());
            solo.setColour (juce::TextButton::buttonOnColourId, Theme::amber());
            mute.setColour (juce::TextButton::buttonOnColourId, Theme::violet());
            rec.setTooltip ("Record enable");  solo.setTooltip ("Solo");  mute.setTooltip ("Mute");
            rec.onClick  = [this] { if (track) { bool arm = ! session.isArmed (*track); session.setArmed (*track, arm); for (auto m : session.groupMates (*track, 3)) session.setArmed (*m, arm); } refresh(); };
            solo.onClick = [this] { session.setTrackSolo (*row, ! row->isSolo (false)); refresh(); };
            mute.onClick = [this] { session.setTrackMute (*row, ! row->isMuted (false)); refresh(); };
            if (TrackOps::polarityStage (*row) != nullptr && ! (track != nullptr && IO::kindOf (*track) == IO::Kind::midi))
            {
                addAndMakeVisible (polarityBtn);
                polarityBtn.setClickingTogglesState (false);
                polarityBtn.setColour (juce::TextButton::buttonOnColourId, Theme::acid().darker (0.2f));
                polarityBtn.setTooltip ("Polarity (phase) invert");
                polarityBtn.onClick = [this] { TrackOps::setPolarityInverted (*row, ! TrackOps::isPolarityInverted (*row)); refresh(); };
            }

            addAndMakeVisible (autoBtn);
            autoBtn.setTooltip ("Automation mode (off / read / touch / latch / write)");
            autoBtn.onClick = [this] { showAutomationMenu(); };
        }
        for (auto* b : { &inputBtn, &outputBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, Theme::bg());
            b->setColour (juce::TextButton::textColourOffId, Theme::text());
        }
        if (row == nullptr)
        {
            // Master Fader: its output is the interface's main output path
            addAndMakeVisible (outputBtn);
            outputBtn.setTooltip ("Master Fader output path (I/O Setup > Output names these)");
            outputBtn.onClick = [this] { IO::showMasterOutputMenu (session, &outputBtn, [this] { refresh(); }); };
        }
        if (track != nullptr)
        {
            addAndMakeVisible (inputBtn);
            addAndMakeVisible (outputBtn);
            inputBtn.setTooltip ("Input path - interface input, bus, or MIDI device");
            outputBtn.setTooltip ("Output path - interface output, bus, or another track");
            inputBtn.onClick = [this] { IO::showInputMenu (session, *track, &inputBtn, [this] { refresh(); }); };
            outputBtn.onClick = [this] { IO::showOutputMenu (session, *track, &outputBtn, [this] { refresh(); }); };
            meter.setMeasurer (track->getLevelMeterPlugin() != nullptr ? &track->getLevelMeterPlugin()->measurer : nullptr);
        }
        buildSlots();
        refresh();
    }

    ~ChannelStrip() override
    {
        fader.setLookAndFeel (nullptr);
        if (row == nullptr)
        {
            auto epc = session.edit != nullptr ? session.edit->getCurrentPlaybackContext() : nullptr;
            meter.setMeasurer (nullptr, epc != nullptr && epc == lastContext);
        }
    }

    te::AudioTrack* getTrack() const    { return track; }
    te::Track* getRow() const           { return row; }
    void slotsChanged() override        { buildSlots(); }

    juce::Rectangle<int> fjBox (int labelY) const   { return { getWidth() - 36, labelY, 30, 11 }; }

    /** Inactive tracks are greyed out like Pro Tools (Track > Make Active brings them back). */
    void paintOverChildren (juce::Graphics& g) override
    {
        if (row == nullptr || row->isProcessing (false)) return;
        g.setColour (Theme::bg().withAlpha (0.62f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 5.0f);
        g.setColour (Theme::dim());
        g.setFont (Theme::font (11.0f, true));
        g.drawText ("INACTIVE", getLocalBounds().withHeight (60).withY (getHeight() / 2 - 30), juce::Justification::centred);
    }

    /** The small "+F-J" box on the INSERTS / SENDS labels shows the second bank of 5 (like View > Mix Window Views). */
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto p = e.getPosition();
        if (! insertSlots.empty() && fjBox (insertsLabelY).expanded (3).contains (p)) { Prefs::set ("mx_insertsFJ", ! Prefs::getBool ("mx_insertsFJ", false)); return; }
        if (! sendSlots.empty()   && fjBox (sendsLabelY).expanded (3).contains (p))   { Prefs::set ("mx_sendsFJ",   ! Prefs::getBool ("mx_sendsFJ", false)); return; }
    }

    static juce::String dbText (float db)   { return db <= -69.9f ? juce::String ("-inf") : juce::String (db, 1); }

    //==========================================================================
    void buildSlots()
    {
        insertSlots.clear();
        sendSlots.clear();
        const int numInserts = ! outputWindowStyle && Prefs::showInsertsInMix() && hasInserts() ? (Prefs::getBool ("mx_insertsFJ", false) ? 10 : 5) : 0;
        const int numSends   = ! outputWindowStyle && Prefs::showSendsInMix() && track != nullptr ? (Prefs::getBool ("mx_sendsFJ", false) ? 10 : 5) : 0;

        auto inserts = hasInserts() ? Session::getInserts (getPluginList(), track) : juce::Array<te::Plugin*>();
        for (int i = 0; i < juce::jmax (numInserts, inserts.size()); ++i)
        {
            if (numInserts == 0) break;
            auto slot = std::make_unique<InsertSlot> (*this, i, i < inserts.size() ? te::Plugin::Ptr (inserts[i]) : nullptr);
            addAndMakeVisible (*slot);
            insertSlots.push_back (std::move (slot));
        }

        if (track != nullptr && numSends > 0)
        {
            auto sends = Session::getSends (*track);
            for (int i = 0; i < juce::jmax (numSends, sends.size()); ++i)
            {
                auto slot = std::make_unique<SendSlot> (*this, i, i < sends.size() ? sends[i] : nullptr);
                addAndMakeVisible (*slot);
                sendSlots.push_back (std::move (slot));
            }
        }
        resized();
    }

    void refresh()
    {
        if (folder != nullptr && folder->getVCAPlugin() != nullptr)
        {
            if (! fader.isMouseButtonDown()) fader.setValue (folder->getVCAPlugin()->getVolumeDb(), juce::dontSendNotification);
            panL.setVisible (false); panR.setVisible (false);
        }
        else if (auto vp = getVolumePlugin())
        {
            if (! fader.isMouseButtonDown()) fader.setValue (vp->getVolumeDb(), juce::dontSendNotification);
            auto sp = track != nullptr ? Session::getStereoPan (*track) : nullptr;
            bool stereo = sp != nullptr;
            if (stereo != stereoPan) { stereoPan = stereo; resized(); }
            if (sp != nullptr)
            {
                if (! panL.isMouseButtonDown()) panL.setValue (sp->panL->getCurrentValue(), juce::dontSendNotification);
                if (! panR.isMouseButtonDown()) panR.setValue (sp->panR->getCurrentValue(), juce::dontSendNotification);
            }
            else if (! panL.isMouseButtonDown()) panL.setValue (vp->getPan(), juce::dontSendNotification);
        }
        volText.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
        peakText.setText (meter.getMaxPeak() <= -99.0f ? juce::String ("-") : juce::String (meter.getMaxPeak(), 1), juce::dontSendNotification);
        peakText.setColour (juce::Label::textColourId, meter.getMaxPeak() >= 0.0f ? Theme::red() : Theme::dim());

        if (row != nullptr)
        {
            if (! name.isBeingEdited()) name.setText (row->getName(), juce::dontSendNotification);
            solo.setToggleState (row->isSolo (false), juce::dontSendNotification);
            mute.setToggleState (row->isMuted (false), juce::dontSendNotification);
            polarityBtn.setToggleState (TrackOps::isPolarityInverted (*row), juce::dontSendNotification);
            auto am = Session::getAutomationModeIndex (*row);
            static const char* names[] = { "auto off", "auto read", "auto touch", "auto latch", "auto write" };
            autoBtn.setButtonText (names[juce::jlimit (0, 4, am)]);
            autoBtn.setColour (juce::TextButton::buttonColourId, am <= 1 ? Theme::panel2() : (am == 4 ? Theme::red().darker (0.4f) : Theme::amber().darker (0.6f)));
        }
        if (track != nullptr)
        {
            rec.setToggleState (session.isArmed (*track), juce::dontSendNotification);
            mon.setToggleState (TrackOps::isInputMonitorOn (*track), juce::dontSendNotification);
            inputBtn.setButtonText (IO::inputLabel (session, *track));
            outputBtn.setButtonText (IO::outputLabel (session, *track));
        }
        else if (row == nullptr)
        {
            name.setText ("Master 1", juce::dontSendNotification);
            outputBtn.setButtonText (IO::masterOutputLabel (session));
            auto epc = session.edit->getCurrentPlaybackContext();
            if (epc != lastContext)
            {
                meter.setMeasurer (nullptr, false);   // the old context (and its measurer) may be gone
                lastContext = epc;
                if (epc != nullptr) meter.setMeasurer (&epc->masterLevels);
            }
        }
        for (auto& s : sendSlots) s->refresh();
        for (auto& s : insertSlots) s->refresh();
    }

    void tick()     { meter.tick(); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        bool selected = state != nullptr && row != nullptr && state->isTrackSelected (row);
        g.setColour (row == nullptr ? Theme::panel2() : (selected ? Theme::panel2().brighter (0.06f) : Theme::panel()));
        g.fillRoundedRectangle (r, 5.0f);
        auto colour = row != nullptr ? row->getColour() : Theme::acid();
        g.setColour (colour);
        g.fillRoundedRectangle (r.removeFromTop (5.0f), 2.0f);
        if (selected) { g.setColour (Theme::acid().withAlpha (0.8f)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 5.0f, 1.5f); }

        g.setFont (Theme::font (9.0f, true));
        auto label = [&] (const juce::String& t, int y, juce::Colour c)
        {
            g.setColour (Theme::line());
            g.fillRect (6, y + 11, getWidth() - 12, 1);
            g.setColour (c);
            g.drawText (t, 7, y, getWidth() - 14, 11, juce::Justification::centredLeft);
        };
        auto fjToggle = [&] (int y, bool on)
        {
            auto box = fjBox (y);
            g.setColour (on ? Theme::violet().withAlpha (0.5f) : Theme::bg());
            g.fillRoundedRectangle (box.toFloat(), 2.0f);
            g.setColour (on ? Theme::text() : Theme::acid());
            g.setFont (Theme::font (8.5f, true));
            g.drawText (on ? "A-J" : "+F-J", box, juce::Justification::centred);
        };
        if (! insertSlots.empty()) { label ("INSERTS", insertsLabelY, Theme::dim()); fjToggle (insertsLabelY, Prefs::getBool ("mx_insertsFJ", false)); }
        if (! sendSlots.empty())   { label ("SENDS", sendsLabelY, Theme::dim());     fjToggle (sendsLabelY, Prefs::getBool ("mx_sendsFJ", false)); }
        if (track != nullptr || row == nullptr) label ("I/O", ioLabelY, Theme::dim());
        if (panL.isVisible())      label (stereoPan ? "PAN  L / R" : "PAN", panLabelY, Theme::dim());
        if (folder != nullptr)
        {
            g.setColour (folder->getColour());
            g.drawText (folder->isSubmixFolder() ? "FOLDER" : "VCA", 7, panLabelY, getWidth() - 14, 11, juce::Justification::centredRight);
        }
        // Pro Tools-style group ID box above the fader
        juce::String groups;
        if (row != nullptr) for (auto& gr : session.getGroups()) if (std::find (gr.tracks.begin(), gr.tracks.end(), row->itemID) != gr.tracks.end()) groups << gr.id;
        if (row != nullptr)
        {
            auto gb = juce::Rectangle<int> (7, groupY, getWidth() - 14, 14);
            g.setColour (Theme::bg());
            g.fillRoundedRectangle (gb.toFloat(), 2.0f);
            g.setColour (groups.isNotEmpty() ? Theme::mint() : Theme::dim());
            g.setFont (Theme::font (9.0f, true));
            g.drawText (groups.isNotEmpty() ? "grp " + groups : "no group", gb, juce::Justification::centred);
        }
        // fader track background
        g.setColour (Theme::bg().withAlpha (0.5f));
        g.fillRoundedRectangle (fader.getBounds().toFloat(), 3.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 7);
        r.removeFromTop (3);
        name.setBounds (r.removeFromTop (20));
        r.removeFromTop (3);

        if (row != nullptr) { autoBtn.setBounds (r.removeFromTop (16)); r.removeFromTop (4); }
        const int slotH = 17;
        if (! insertSlots.empty())
        {
            insertsLabelY = r.getY();
            r.removeFromTop (13);
            for (auto& s : insertSlots) { s->setBounds (r.removeFromTop (slotH)); r.removeFromTop (1); }
            r.removeFromTop (5);
        }
        if (! sendSlots.empty())
        {
            sendsLabelY = r.getY();
            r.removeFromTop (13);
            for (auto& s : sendSlots) { s->setBounds (r.removeFromTop (slotH + 6)); r.removeFromTop (1); }
            r.removeFromTop (5);
        }
        if (track != nullptr || row == nullptr)
        {
            ioLabelY = r.getY();
            {
                auto lab = r.removeFromTop (13);
                polarityBtn.setBounds (lab.removeFromRight (20).reduced (0, 0));
            }
            if (track != nullptr) inputBtn.setBounds (r.removeFromTop (17));
            else r.removeFromTop (17);
            r.removeFromTop (1);
            outputBtn.setBounds (r.removeFromTop (17));
            r.removeFromTop (5);
        }

        panLabelY = r.getY();
        r.removeFromTop (12);
        auto panArea = r.removeFromTop (52);
        bool showPan = ! (folder != nullptr && folder->getVCAPlugin() != nullptr);
        panL.setVisible (showPan);
        panR.setVisible (showPan && stereoPan);
        if (stereoPan)
        {
            auto half = panArea.getWidth() / 2;
            panL.setBounds (panArea.removeFromLeft (half).withSizeKeepingCentre (juce::jmin (half, 50), 52));
            panR.setBounds (panArea.withSizeKeepingCentre (juce::jmin (half, 50), 52));
        }
        else panL.setBounds (panArea.withSizeKeepingCentre (50, 52));
        r.removeFromTop (3);

        if (row != nullptr) { groupY = r.getY(); r.removeFromTop (17); }

        if (row != nullptr)
        {
            auto btns = r.removeFromBottom (22);
            if (rec.isVisible())
            {
                auto w = btns.getWidth() / 4;
                rec.setBounds (btns.removeFromLeft (w).reduced (1, 0));
                mon.setBounds (btns.removeFromLeft (w).reduced (1, 0));
                solo.setBounds (btns.removeFromLeft (w).reduced (1, 0));
                mute.setBounds (btns.reduced (1, 0));
            }
            else
            {
                auto w = btns.getWidth() / 2;
                solo.setBounds (btns.removeFromLeft (w).reduced (1, 0));
                mute.setBounds (btns.reduced (1, 0));
            }
            r.removeFromBottom (4);
        }
        peakText.setBounds (r.removeFromBottom (12));
        volText.setBounds (r.removeFromBottom (17));
        r.removeFromBottom (3);

        auto meterArea = r.removeFromRight (14);
        meter.setBounds (meterArea);
        scale.setBounds (r.removeFromLeft (juce::jmin (26, r.getWidth() / 3)));
        fader.setBounds (r.reduced (2, 0));
    }

    //==========================================================================
    // menus shared by the slots
    void showPanMenu (PanKnob* which)
    {
        juce::PopupMenu m;
        auto law = session.getPanLaw();
        juce::PopupMenu depth;
        auto names = Session::panLawNames();
        for (int i = 0; i < names.size(); ++i) depth.addItem (100 + i, names[i], true, law == i);
        m.addSubMenu ("Pan depth (session)", depth);
        if (track != nullptr)
        {
            m.addItem (1, "Stereo pan (two knobs, L / R)", true, stereoPan);
            if (stereoPan)
            {
                auto sp = Session::getStereoPan (*track);
                m.addItem (2, "Link L and R", true, sp != nullptr && sp->linked.get());
                m.addItem (3, "Inverse link (mirror)", true, sp != nullptr && sp->inverse.get());
                m.addItem (4, "Hard left / right (L100, R100)");
                m.addItem (5, "Both centre (mono)");
            }
        }
        m.addItem (6, "Centre");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (which), [this] (int r)
        {
            if (r >= 100) session.setPanLaw (r - 100);
            else if (r == 1 && track) session.setStereoPanEnabled (*track, ! stereoPan);
            else if (r == 2 && track) { if (auto sp = Session::getStereoPan (*track)) sp->linked = ! sp->linked.get(); }
            else if (r == 3 && track) { if (auto sp = Session::getStereoPan (*track)) sp->inverse = ! sp->inverse.get(); }
            else if (r == 4 && track) { if (auto sp = Session::getStereoPan (*track)) { sp->panL->setParameter (-1.0f, juce::sendNotification); sp->panR->setParameter (1.0f, juce::sendNotification); } }
            else if (r == 5 && track) { if (auto sp = Session::getStereoPan (*track)) { sp->panL->setParameter (0.0f, juce::sendNotification); sp->panR->setParameter (0.0f, juce::sendNotification); } }
            else if (r == 6) { panL.setValue (0.0); panR.setValue (0.0); }
            refresh();
        });
    }

private:
    te::Track::Ptr keepAlive;
    TimelineState* state;
    juce::Label name, volText, peakText;
    juce::Slider fader;
    FaderScale scale;
    PanKnob panL, panR;
    LevelMeter meter;
public:
    /** Output windows show the strip without insert / send rows (Pro Tools' Output window). Set before rebuildSlots(). */
    bool outputWindowStyle = false;
private:
    juce::TextButton rec { "R" }, mon { "I" }, solo { "S" }, mute { "M" }, autoBtn { "auto read" }, inputBtn, outputBtn;
    juce::TextButton polarityBtn { juce::String::fromUTF8 ("\xc3\x98") };
    std::vector<std::unique_ptr<InsertSlot>> insertSlots;
    std::vector<std::unique_ptr<SendSlot>> sendSlots;
    int insertsLabelY = 0, sendsLabelY = 0, ioLabelY = 0, panLabelY = 0, groupY = 0;
    bool stereoPan = false;
    te::EditPlaybackContext* lastContext = nullptr;

    te::VolumeAndPanPlugin* getVolumePlugin()
    {
        if (track != nullptr) return track->getVolumePlugin();
        if (folder != nullptr) return folder->getVolumePlugin();
        return session.edit->getMasterVolumePlugin().get();
    }
    te::AutomatableParameter* volParam()
    {
        if (folder != nullptr && folder->getVCAPlugin() != nullptr) return folder->getVCAPlugin()->volParam.get();
        if (auto vp = getVolumePlugin()) return vp->volParam.get();
        return nullptr;
    }

    void panChanged (PanKnob* k)
    {
        if (juce::ModifierKeys::currentModifiers.isPopupMenu()) return;
        if (track != nullptr)
            if (auto sp = Session::getStereoPan (*track))
            {
                auto v = (float) k->getValue();
                auto& mine = k == &panL ? sp->panL : sp->panR;
                auto& other = k == &panL ? sp->panR : sp->panL;
                auto delta = v - mine->getCurrentValue();
                mine->setParameter (v, juce::sendNotification);
                if (sp->linked.get())  other->setParameter (juce::jlimit (-1.0f, 1.0f, other->getCurrentValue() + (sp->inverse.get() ? -delta : delta)), juce::sendNotification);
                return;
            }
        if (auto vp = getVolumePlugin()) vp->setPan ((float) k->getValue());
    }

    void showAutomationMenu()
    {
        if (row == nullptr) return;
        juce::PopupMenu m;
        auto names = Session::automationModeNames();
        auto cur = Session::getAutomationModeIndex (*row);
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, cur == i);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&autoBtn), [this] (int r) { if (r >= 1) Session::setAutomationModeIndex (*row, r - 1); refresh(); });
    }

};

//==============================================================================
class MixerView : public juce::Component,
                  private juce::Timer,
                  private juce::ValueTree::Listener,
                  private juce::AsyncUpdater,
                  private juce::ChangeListener
{
public:
    explicit MixerView (Session& s, TimelineState* st = nullptr) : session (s), state (st)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&strips, false);
        viewport.setScrollBarsShown (true, true);
        strips.addMouseListener (this, false);   // right-click on empty console space = new track menu
        strips.addAndMakeVisible (newTrackBtn);
        newTrackBtn.setTooltip ("New Tracks (Cmd+Shift+N) - right-click for the quick track menu");
        newTrackBtn.onClick = [this]
        {
            if (juce::ModifierKeys::currentModifiers.isPopupMenu()) showQuickNew();
            else if (session.showNewTracksHook) session.showNewTracksHook();
        };
        session.addChangeListener (this);
        Prefs::broadcaster().addChangeListener (this);
        Theme::broadcaster().addChangeListener (this);
        attach();
        startTimerHz (30);
    }
    std::function<bool (te::Track&)> trackFilter;   // Beat page: only the rack's channels
    void refreshStrips();

    ~MixerView() override
    {
        session.removeChangeListener (this);
        Prefs::broadcaster().removeChangeListener (this);
        Theme::broadcaster().removeChangeListener (this);
        if (watched.isValid()) watched.removeListener (this);
    }

    void paint (juce::Graphics& g) override    { g.fillAll (Theme::mixBg()); }

    void resized() override
    {
        auto r = getLocalBounds();
        if (master != nullptr) master->setBounds (r.removeFromRight (stripWidth() + 10).reduced (4, 4));
        viewport.setBounds (r);
        layoutStrips();
    }

    void showQuickNew()
    {
        QuickNew::build (session).showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this] (int r)
        {
            if (r > 0 && session.quickNewHook) session.quickNewHook (r, 0);
        });
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && e.eventComponent == &strips) { showQuickNew(); return; }
        // click on a strip's background selects that track (Shift = add)
        if (state == nullptr) return;
        for (auto& c : channel)
            if (c->getRow() != nullptr && c->getScreenBounds().contains (e.getScreenPosition()))
            {
                state->selectTrack (c->getRow(), e.mods.isShiftDown() || e.mods.isCommandDown());
                for (auto& s : channel) s->repaint();
            }
    }

    int stripWidth() const    { return Prefs::narrowStrips() ? 84 : 124; }
    int minStripHeight() const
    {
        int h = 500;   // keeps at least ~230 px of fader travel; the Mix window scrolls vertically below this
        if (Prefs::showInsertsInMix()) h += 13 + (Prefs::getBool ("mx_insertsFJ", false) ? 10 : 5) * 18 + 5;
        if (Prefs::showSendsInMix())   h += 13 + (Prefs::getBool ("mx_sendsFJ", false) ? 10 : 5) * 24 + 5;
        return h;
    }

private:
    Session& session;
    TimelineState* state;
    juce::Viewport viewport;
    juce::TextButton newTrackBtn { "+ TRACK" };
    juce::Component strips;
    std::vector<std::unique_ptr<ChannelStrip>> channel;
    std::unique_ptr<ChannelStrip> master;
    juce::ValueTree watched;
    int refreshCounter = 0;

    void attach()
    {
        if (watched.isValid()) watched.removeListener (this);
        if (session.edit != nullptr) { watched = session.edit->state; watched.addListener (this); }
        rebuild();
    }

    void changeListenerCallback (juce::ChangeBroadcaster* b) override
    {
        if (b == &session) { attach(); return; }
        rebuild();   // preferences / theme changed
    }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c) override         { if (isRelevant (c)) triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int) override
    {
        if (c.hasType (te::IDs::TRACK) || c.hasType (te::IDs::FOLDERTRACK))
            channel.erase (std::remove_if (channel.begin(), channel.end(),
                                           [&c] (auto& s) { return s->getRow() != nullptr && s->getRow()->state == c; }),
                           channel.end());
        if (isRelevant (c)) triggerAsyncUpdate();
    }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override           { triggerAsyncUpdate(); }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (id == te::IDs::enabled || id == te::IDs::busNum || id == te::IDs::phase || id == te::IDs::hidden || id == te::IDs::process
            || id == juce::Identifier ("sqmMasterFader") || id == juce::Identifier ("sqmMasterHidden")) triggerAsyncUpdate();
        juce::ignoreUnused (v);
    }

    static bool isRelevant (const juce::ValueTree& c)
    {
        return c.hasType (te::IDs::TRACK) || c.hasType (te::IDs::PLUGIN) || c.hasType (te::IDs::FOLDERTRACK);
    }

    void handleAsyncUpdate() override   { rebuild(); }


    void rebuild()
    {
        channel.clear();
        master.reset();
        if (session.edit == nullptr) return;
        for (auto t : session.getRows())
        {
            if (t->isHidden()) continue;
            if (trackFilter && ! trackFilter (*t)) continue;
            auto s = std::make_unique<ChannelStrip> (session, t, state);
            s->addMouseListener (this, false);
            strips.addAndMakeVisible (*s);
            channel.push_back (std::move (s));
        }
        if (TrackOps::hasMasterFader (session) && ! (bool) session.edit->state.getProperty ("sqmMasterHidden", false))
        {
            master = std::make_unique<ChannelStrip> (session, nullptr, state);
            addAndMakeVisible (*master);
        }
        resized();
    }

    void layoutStrips()
    {
        int h = juce::jmax (minStripHeight(), viewport.getHeight() - 14);
        auto sw = stripWidth();
        strips.setSize (juce::jmax (viewport.getWidth(), (int) channel.size() * sw + 8), h + 8);
        int x = 4;
        for (auto& c : channel) { c->setBounds (x, 4, sw - 4, h); x += sw; }
        newTrackBtn.setBounds (x + 6, 8, juce::jmax (90, sw - 10), 34);
        if (strips.getWidth() < x + sw + 8) strips.setSize (x + sw + 8, strips.getHeight());
    }

    void timerCallback() override
    {
        for (auto& c : channel) c->tick();
        if (master) master->tick();
        if (++refreshCounter % 6 == 0)
        {
            for (auto& c : channel) c->refresh();
            if (master) master->refresh();
        }
    }
};
inline void MixerView::refreshStrips()   { rebuild(); }
