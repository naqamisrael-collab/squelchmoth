#pragma once
// Squelchmoth - the Mix window: the 1.10 "Silver / Fader" console.
//
// Every strip is a chrome panel with a COMPACT / STANDARD / EXPANDED width (saved per track), a COMP selector
// (cyan) and a LIMIT selector (pink) built from Squelchmoth's own dynamics, real gain-reduction meters for both,
// a segmented LED level meter with the fader riding over it, a gain readout with a live-signal dot, and M / S.
// Expanded strips add METER TYPE (Peak, RMS, LUFS momentary / short-term) and the CHANNEL GAIN footer.
//
// Views: Dynamics (the console), Full (adds the Pro Tools section on top of every strip: automation mode,
// Inserts A-J, Sends A-J, I/O, pan, group, Rec / Input / polarity / mono) and Overview (every strip Compact).
// Channel Layout: Flexible, All Compact / Standard / Expanded, plus layouts saved in the session.
// Drag a strip's name bar sideways to reorder tracks. Dragging the fader of a selected strip moves every
// selected strip by the same amount (hold Opt / Alt for just that one).

#include "TrackSlots.h"
#include "TrackOps.h"
#include "QuickNew.h"
#include "EditState.h"
#include "Console.h"

//==============================================================================
/** Peak reader on a Tracktion LevelMeasurer, with console ballistics (decay and peak hold from Preferences). */
class PeakReader
{
public:
    ~PeakReader()   { setMeasurer (nullptr); }
    void setMeasurer (te::LevelMeasurer* m, bool oldOneStillExists = true)
    {
        if (m == measurer) return;
        if (measurer != nullptr && oldOneStillExists) measurer->removeClient (client);
        measurer = m;
        if (measurer != nullptr) measurer->addClient (client);
    }
    bool hasMeasurer() const   { return measurer != nullptr; }
    void tick()
    {
        if (seenReset != clipIndicatorResetCounter()) resetPeak();
        if (measurer == nullptr) return;
        feed (client.getAndClearAudioLevel (0).dB, client.getAndClearAudioLevel (1).dB);
    }
    /** Feed one reading per channel (used for tap-based meters too, so ballistics match exactly). */
    void feed (float l0, float l1)
    {
        const float decay = (float) Prefs::meterDecayDbPerSec() / 30.0f;
        const int holdTicksMax = juce::jmax (1, Prefs::meterHoldMs() / 33);
        const float in[2] = { l0, l1 };
        for (int ch = 0; ch < 2; ++ch)
        {
            const float l = in[ch];
            level[ch] = juce::jmax (l, level[ch] - decay);
            if (l > peakHold[ch]) { peakHold[ch] = l; holdTicks[ch] = holdTicksMax; }
            else if (--holdTicks[ch] <= 0) peakHold[ch] = juce::jmax (-100.0f, peakHold[ch] - 0.8f);
            if (l >= 0.0f) clip = true;
            maxPeak = juce::jmax (maxPeak, l);
        }
        recent = juce::jmax (l0, l1);
    }
    void resetPeak()   { maxPeak = -100.0f; clip = false; peakHold[0] = peakHold[1] = -100.0f; seenReset = clipIndicatorResetCounter(); }

    float level[2] = { -100.0f, -100.0f }, peakHold[2] = { -100.0f, -100.0f }, maxPeak = -100.0f, recent = -100.0f;
    bool clip = false;
private:
    te::LevelMeasurer* measurer = nullptr;
    te::LevelMeasurer::Client client;
    int holdTicks[2] = { 0, 0 };
    int seenReset = clipIndicatorResetCounter();
};

//==============================================================================
/** Pro Tools-style pan knob: shows "<100", "C", "R32"; double-click centres; drag vertically. (Full view.) */
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
        juce::ColourGradient bez (juce::Colour (0xffd9e1ea), knob.getX(), knob.getY(), juce::Colour (0xff3e4855), knob.getX(), knob.getBottom(), false);
        g.setGradientFill (bez);
        g.fillEllipse (knob);
        g.setColour (Console::Look::well());
        g.fillEllipse (knob.reduced (2.0f));
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - 4.0f, radius - 4.0f, 0.0f, a0, a1, true);
        g.setColour (Console::Look::faint());
        g.strokePath (arc, juce::PathStrokeType (2.0f));
        juce::Path fill;
        auto mid = (a0 + a1) * 0.5f;
        fill.addCentredArc (centre.x, centre.y, radius - 4.0f, radius - 4.0f, 0.0f, juce::jmin (mid, angle), juce::jmax (mid, angle), true);
        g.setColour (Console::Look::cyan());
        g.strokePath (fill, juce::PathStrokeType (2.2f));
        auto tip = centre.getPointOnCircumference (radius - 6.0f, angle);
        g.setColour (Console::Look::text());
        g.drawLine (centre.x, centre.y, tip.x, tip.y, 1.8f);
        g.setFont (Theme::font (9.5f, true));
        g.drawText (panText (getValue()), getLocalBounds().removeFromBottom (12), juce::Justification::centred);
    }
};

//==============================================================================
/** Console fader: an invisible track (the LED meter shows through) with a slim slot and a chrome cap. */
struct ConsoleFaderLookAndFeel : public juce::LookAndFeel_V4
{
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto slot = juce::Rectangle<float> ((float) x + (float) w * 0.5f - 2.0f, (float) y, 4.0f, (float) h);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (slot, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (slot, 2.0f, 0.8f);
        // cap: chrome, 18 px tall, a centre line that turns amber above 0 dB
        const float capW = juce::jmin ((float) w - 2.0f, 36.0f), capH = 18.0f;
        juce::Rectangle<float> cap ((float) x + (float) w * 0.5f - capW * 0.5f, pos - capH * 0.5f, capW, capH);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
        juce::ColourGradient chrome (juce::Colour (0xfff4f8fb), cap.getX(), cap.getY(), juce::Colour (0xff59636f), cap.getX(), cap.getBottom(), false);
        chrome.addColour (0.45, juce::Colour (0xffb6c0cb));
        chrome.addColour (0.55, juce::Colour (0xff7d8894));
        g.setGradientFill (chrome);
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (cap, 3.0f, 1.0f);
        g.setColour (s.getValue() > 0.05 ? Console::Look::orange() : juce::Colours::white);
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 0.75f, cap.getWidth() - 6.0f, 1.5f);
        if (s.hasKeyboardFocus (false)) { g.setColour (Console::Look::cyan()); g.drawRoundedRectangle (cap.expanded (1.5f), 4.0f, 1.0f); }
    }
    int getSliderThumbRadius (juce::Slider&) override   { return 9; }
};
inline ConsoleFaderLookAndFeel& consoleFaderLookAndFeel()   { static ConsoleFaderLookAndFeel l; return l; }

//==============================================================================
class ChannelStrip : public juce::Component,
                     public SlotController
{
public:
    using Width = Console::Width;
    using MeterType = Console::MeterType;

    ChannelStrip (Session& s, te::Track* t, TimelineState* st = nullptr) : SlotController (s, t), keepAlive (t), state (st)
    {
        namespace L = Console::Look;
        name.setJustificationType (juce::Justification::centredLeft);
        name.setFont (L::caps (13.0f));
        name.setMinimumHorizontalScale (1.0f);
        name.setColour (juce::Label::textColourId, L::text());
        name.setColour (juce::Label::backgroundWhenEditingColourId, L::well());
        name.setColour (juce::Label::textWhenEditingColourId, L::text());
        name.setEditable (false, row != nullptr);
        name.setInterceptsMouseClicks (row != nullptr, false);
        name.onTextChange = [this] { if (row) row->setName (name.getText()); };
        name.setTooltip ("Double-click to rename. Drag the name bar sideways to move the track.");
        name.addMouseListener (this, false);
        addAndMakeVisible (name);

        widthSel.setTooltip ("Strip width: Compact / Standard / Expanded");
        widthSel.onChange = [this] (Width w) { Console::Model::setWidth (session, row, w); if (onWidthChanged) onWidthChanged(); };
        addAndMakeVisible (widthSel);

        compSel.setTooltip ("COMP slot - choose the compressor on this channel (click the name again to open it)");
        compSel.onClick = [this] { showDynamicsMenu (false); };
        limitSel.setTooltip ("LIMIT slot - choose the limiter on this channel, placed last before the fader");
        limitSel.onClick = [this] { showDynamicsMenu (true); };
        meterSel.setTooltip ("Meter type: Peak, RMS (300 ms), LUFS momentary (400 ms) or LUFS short-term (3 s), BS.1770-4");
        meterSel.onClick = [this] { showMeterTypeMenu(); };
        for (auto* c : { &compSel, &limitSel, &meterSel }) addAndMakeVisible (*c);

        compGr.setTooltip ("Compressor gain reduction, 0 to -24 dB (click to reset the peak)");
        limitGr.setTooltip ("Limiter gain reduction, 0 to -24 dB (click to reset the peak)");
        compDial.setTooltip ("COMP gain reduction");
        limitDial.setTooltip ("LIMIT gain reduction");
        for (auto* c : { (juce::Component*) &compGr, (juce::Component*) &limitGr, (juce::Component*) &compDial, (juce::Component*) &limitDial }) addAndMakeVisible (c);

        compScale.kind = limitScale.kind = Console::Scale::gr;
        compScale.just = juce::Justification::centredRight;
        limitScale.just = juce::Justification::centredLeft;
        for (auto* c : { &levelScale, &compScale, &limitScale }) { c->setInterceptsMouseClicks (false, false); addAndMakeVisible (*c); }

        led.onClick = [this] { reader.resetPeak(); compGr.resetPeak(); limitGr.resetPeak(); if (tap) tap->resetIntegrated(); };
        addAndMakeVisible (led);

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setNormalisableRange (Console::Map::faderRange());
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::commandModifier);   // Cmd/Ctrl-drag = fine
        fader.onValueChange = [this]
        {
            const float v = (float) fader.getValue();
            applyVolume (v);
            const float delta = v - lastFaderValue;
            lastFaderValue = v;
            if (! applyingGang && onGang && ! juce::ModifierKeys::currentModifiers.isAltDown() && std::abs (delta) > 0.0001f) onGang (*this, delta);
            updateGainText();
        };
        fader.onDragStart = [this] { if (auto p = volParam()) p->parameterChangeGestureBegin(); };
        fader.onDragEnd   = [this] { if (auto p = volParam()) p->parameterChangeGestureEnd(); };
        fader.setLookAndFeel (&consoleFaderLookAndFeel());
        fader.setTooltip ("Fader - double-click for 0 dB, " SQ_CMD "-drag for fine control. Selected strips move together (hold " SQ_OPT " for one).");
        addAndMakeVisible (fader);

        gainText.setJustificationType (juce::Justification::centred);
        gainText.setFont (Theme::font (13.0f, true));
        gainText.setColour (juce::Label::textColourId, L::text());
        gainText.setEditable (false, true);
        gainText.setTooltip ("Channel gain - double-click to type a value");
        gainText.onTextChange = [this] { fader.setValue (gainText.getText().retainCharacters ("-0123456789.").getDoubleValue()); };
        addAndMakeVisible (gainText);

        if (row != nullptr)
        {
            for (auto* b : { &solo, &mute }) addAndMakeVisible (b);
            solo.setTooltip ("Solo");  mute.setTooltip ("Mute");
            solo.onClick = [this] { session.setTrackSolo (*row, ! row->isSolo (false)); refresh(); };
            mute.onClick = [this] { session.setTrackMute (*row, ! row->isMuted (false)); refresh(); };
            autoBtn.setTooltip ("Automation mode (off / read / touch / latch / write)");
            autoBtn.onClick = [this] { showAutomationMenu(); };
            if (track != nullptr && IO::kindOf (*track) != IO::Kind::aux)
            {
                rec.setTooltip ("Record enable");
                mon.setTooltip ("Input Monitor - hear this track's live input (this track only)");
                rec.onClick = [this] { if (track) { bool arm = ! session.isArmed (*track); session.setArmed (*track, arm); for (auto m : session.groupMates (*track, 3)) session.setArmed (*m, arm); } refresh(); };
                mon.onClick = [this] { if (track) TrackOps::setInputMonitor (session, *track, ! TrackOps::isInputMonitorOn (*track)); refresh(); };
                hasRecButtons = true;
            }
            if (TrackOps::polarityStage (*row) != nullptr && ! (track != nullptr && IO::kindOf (*track) == IO::Kind::midi))
            {
                hasPolarity = true;
                polarityBtn.setTooltip ("Polarity (phase) invert");
                polarityBtn.onClick = [this] { TrackOps::setPolarityInverted (*row, ! TrackOps::isPolarityInverted (*row)); refresh(); };
            }
        }
        if (track != nullptr && IO::kindOf (*track) != IO::Kind::midi)
        {
            hasMono = true;
            monoBtn.setTooltip ("Mono: sum this track to one channel before its inserts (track format)");
            monoBtn.onClick = [this] { if (track) TrackOps::setTrackFormat (session, *track, TrackOps::isMonoFormat (session, *track)); refresh(); };
        }
        else if (row == nullptr)
        {
            hasMono = true;
            monoBtn.setTooltip ("Mono check: fold the whole mix to mono");
            monoBtn.onClick = [this] { if (auto m = session.edit->getMasterTrack()) TrackOps::setTrackFormat (session, *m, TrackOps::isMonoFormat (session, *m)); refresh(); };
        }

        for (auto* b : { &inputBtn, &outputBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, L::well());
            b->setColour (juce::TextButton::textColourOffId, L::text());
        }
        if (row == nullptr)
        {
            outputBtn.setTooltip ("Master Fader output path (I/O Setup > Output names these)");
            outputBtn.onClick = [this] { IO::showMasterOutputMenu (session, &outputBtn, [this] { refresh(); }); };
        }
        if (track != nullptr)
        {
            inputBtn.setTooltip ("Input path - interface input, bus, or MIDI device");
            outputBtn.setTooltip ("Output path - interface output, bus, or another track");
            inputBtn.onClick = [this] { IO::showInputMenu (session, *track, &inputBtn, [this] { refresh(); }); };
            outputBtn.onClick = [this] { IO::showOutputMenu (session, *track, &outputBtn, [this] { refresh(); }); };
            reader.setMeasurer (track->getLevelMeterPlugin() != nullptr ? &track->getLevelMeterPlugin()->measurer : nullptr);
        }
        else if (folder != nullptr)
        {
            for (auto q : folder->pluginList.getPlugins())
                if (auto lm = dynamic_cast<te::LevelMeterPlugin*> (q)) reader.setMeasurer (&lm->measurer);
        }

        // the Full view's Pro Tools section: shown / hidden by resized()
        for (auto* c : std::initializer_list<juce::Component*> { &autoBtn, &inputBtn, &outputBtn, &rec, &mon, &polarityBtn, &monoBtn }) addChildComponent (c);

        for (auto* k : { &panL, &panR })
        {
            k->onValueChange = [this, k] { panChanged (k); };
            k->onRightClick = [this, k] { showPanMenu (k); };
            k->setTooltip ("Pan - drag, double-click to centre, right-click for pan depth and stereo pan");
            addChildComponent (*k);
        }

        lastFaderValue = (float) fader.getValue();
        buildSlots();
        refresh();
    }

    ~ChannelStrip() override
    {
        fader.setLookAndFeel (nullptr);
        if (row == nullptr)
        {
            auto epc = session.edit != nullptr ? session.edit->getCurrentPlaybackContext() : nullptr;
            reader.setMeasurer (nullptr, epc != nullptr && epc == lastContext);
        }
    }

    te::AudioTrack* getTrack() const    { return track; }
    te::Track* getRow() const           { return row; }
    void slotsChanged() override        { buildSlots(); }

    std::function<void()> onWidthChanged;                       // the console re-lays out
    std::function<void (ChannelStrip&, float)> onGang;           // fader moved by delta dB on a selected strip
    std::function<void (ChannelStrip&, const juce::MouseEvent&, bool finished)> onReorderDrag;

    /** Output windows show the strip without insert / send rows (Pro Tools' Output window). Set before buildSlots(). */
    bool outputWindowStyle = false;
    /** Overrides the stored width (Output window). */
    std::optional<Width> forcedWidth;

    Width widthNow() const              { return forcedWidth.value_or (Console::Model::effectiveWidth (session, row)); }
    bool fullView() const               { return outputWindowStyle || Console::Model::view() == Console::View::full; }
    int preferredWidth() const          { return Console::widthPixels (widthNow()); }
    bool isSelected() const             { return state != nullptr && row != nullptr && state->isTrackSelected (row); }

    /** Moves this fader by delta dB without re-triggering the gang (the console calls it on the other selected strips). */
    void nudgeFader (float delta)
    {
        const juce::ScopedValueSetter<bool> svs (applyingGang, true);
        fader.setValue (juce::jlimit (-70.0, 12.0, fader.getValue() + (double) delta), juce::sendNotificationSync);
    }
    float faderDb() const   { return (float) fader.getValue(); }
    /** As if the user dragged this fader to db (self-test; runs the gang like a real drag). */
    void userMoveFader (float db)   { fader.setValue (db, juce::sendNotificationSync); }

    static juce::String dbText (float db)
    {
        if (db <= -69.9f) return "-inf dB";
        return (db >= 0.05f ? "+" : (db > -0.05f ? "+" : "")) + juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1) + " dB";
    }

    juce::Rectangle<int> fjBox (int labelY) const   { return { getWidth() - 40, labelY, 30, 11 }; }

    //==========================================================================
    void buildSlots()
    {
        insertSlots.clear();
        sendSlots.clear();
        const bool full = fullView() && ! outputWindowStyle;
        const int numInserts = full && Prefs::showInsertsInMix() && hasInserts() ? (Prefs::getBool ("mx_insertsFJ", false) ? 10 : 5) : 0;
        const int numSends   = full && Prefs::showSendsInMix() && track != nullptr ? (Prefs::getBool ("mx_sendsFJ", false) ? 10 : 5) : 0;

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

    /** Model -> controls. Called every 200 ms and after every change. */
    void refresh()
    {
        if (folder != nullptr && folder->getVCAPlugin() != nullptr)
        {
            if (! fader.isMouseButtonDown()) setFaderQuietly (folder->getVCAPlugin()->getVolumeDb());
        }
        else if (auto vp = getVolumePlugin())
        {
            if (! fader.isMouseButtonDown()) setFaderQuietly (vp->getVolumeDb());
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
        updateGainText();

        // number + name
        if (row != nullptr)
        {
            if (! name.isBeingEdited()) name.setText (row->getName().toUpperCase(), juce::dontSendNotification);
            number = session.getRows().indexOf (row) + 1;
            solo.setToggleState (row->isSolo (false), juce::dontSendNotification);
            mute.setToggleState (row->isMuted (false), juce::dontSendNotification);
            polarityBtn.setToggleState (TrackOps::isPolarityInverted (*row), juce::dontSendNotification);
            auto am = Session::getAutomationModeIndex (*row);
            static const char* names[] = { "auto off", "auto read", "auto touch", "auto latch", "auto write" };
            autoBtn.setButtonText (names[juce::jlimit (0, 4, am)]);
            autoBtn.setColour (juce::TextButton::buttonColourId, am <= 1 ? Console::Look::well() : (am == 4 ? Theme::red().darker (0.4f) : Theme::amber().darker (0.6f)));
        }
        else
        {
            name.setText (TrackOps::masterName (session).toUpperCase(), juce::dontSendNotification);
            number = 0;
        }
        if (track != nullptr)
        {
            rec.setToggleState (session.isArmed (*track), juce::dontSendNotification);
            mon.setToggleState (TrackOps::isInputMonitorOn (*track), juce::dontSendNotification);
            monoBtn.setToggleState (TrackOps::isMonoFormat (session, *track), juce::dontSendNotification);
            inputBtn.setButtonText (IO::inputLabel (session, *track));
            outputBtn.setButtonText (IO::outputLabel (session, *track));
        }
        else if (row == nullptr)
        {
            outputBtn.setButtonText (IO::masterOutputLabel (session));
            if (auto m = session.edit->getMasterTrack()) monoBtn.setToggleState (TrackOps::isMonoFormat (session, *m), juce::dontSendNotification);
            auto epc = session.edit->getCurrentPlaybackContext();
            if (epc != lastContext)
            {
                reader.setMeasurer (nullptr, false);   // the old context (and its measurer) may be gone
                lastContext = epc;
                if (epc != nullptr) reader.setMeasurer (&epc->masterLevels);
            }
        }

        // dynamics slots
        if (hasInserts())
        {
            compPlugin = Console::Model::findDynamics (getPluginList(), track, false);
            limitPlugin = Console::Model::findDynamics (getPluginList(), track, true);
        }
        else { compPlugin = nullptr; limitPlugin = nullptr; }
        {
            const bool shortNames = widthNow() != Width::expanded;
            auto cl = Console::Model::labelFor (compPlugin.get(), false), ll = Console::Model::labelFor (limitPlugin.get(), true);
            compSel.setText (hasInserts() ? (shortNames ? Console::Model::shortLabel (cl) : cl) : juce::String ("-"), compPlugin != nullptr);
            limitSel.setText (hasInserts() ? (shortNames ? Console::Model::shortLabel (ll) : ll) : juce::String ("-"), limitPlugin != nullptr);
            compSel.setTooltip ("COMP slot: " + cl + ". Click to choose the compressor, open or bypass it.");
            limitSel.setTooltip ("LIMIT slot: " + ll + ". Click to choose the limiter (last before the fader), open or bypass it.");
        }
        compSel.setEnabled (hasInserts()); limitSel.setEnabled (hasInserts());

        // meter type + tap
        meterType = Console::Model::canTap (row) ? Console::Model::meterType (session, row) : MeterType::peak;
        meterSel.setText (Console::meterTypeNames()[(int) meterType], true);
        const bool wantTap = Console::Model::canTap (row) && (meterType != MeterType::peak || row == nullptr);
        if (wantTap && session.edit != nullptr) tap = Console::Model::ensureTap (session, row);
        else tap = nullptr;
        widthSel.setWidth (widthNow());

        for (auto& s : sendSlots) s->refresh();
        for (auto& s : insertSlots) s->refresh();
        repaint();
    }

    /** 30 Hz: meters, GR, live dot. */
    void tick()
    {
        reader.tick();
        float l0 = -100.0f, l1 = -100.0f;
        const float post = row == nullptr ? masterFaderDb() : 0.0f;   // master tap sits before the master fader
        if (tap != nullptr && meterType != MeterType::peak)
        {
            if (meterType == MeterType::rms) { l0 = tap->getRmsDb (0) + post; l1 = tap->getRmsDb (1) + post; }
            else
            {
                const float v = (meterType == MeterType::lufsM ? tap->getMomentary() : tap->getShortTerm());
                l0 = l1 = v > -99.0f ? v + post : -100.0f;
            }
            shown.feed (l0, l1);
            led.setLevels (shown.level[0], shown.level[1], reader.peakHold[0], reader.peakHold[1], reader.clip);
        }
        else
            led.setLevels (reader.level[0], reader.level[1], reader.peakHold[0], reader.peakHold[1], reader.clip);

        const bool liveNow = reader.recent > -60.0f;
        liveTicks = liveNow ? 10 : juce::jmax (0, liveTicks - 1);
        if ((liveTicks > 0) != liveShown) { liveShown = liveTicks > 0; repaint (liveDotArea.expanded (4)); repaint (footerArea); }

        auto cg = Console::Model::gainReduction (compPlugin.get()), lg = Console::Model::gainReduction (limitPlugin.get());
        if (compPlugin == nullptr) cg = 0.0f;
        if (limitPlugin == nullptr) lg = 0.0f;
        compGr.setGr (cg); limitGr.setGr (lg); compDial.setGr (cg); limitDial.setGr (lg);
        compGr.setTooltip (cg ? "Compressor gain reduction: " + juce::String (*cg, 1) + " dB" : juce::String ("This compressor does not report gain reduction"));
        limitGr.setTooltip (lg ? "Limiter gain reduction: " + juce::String (*lg, 1) + " dB" : juce::String ("This limiter does not report gain reduction"));

        if (row == nullptr && tap != nullptr && ++lufsTicks % 15 == 0)
        {
            const float i = tap->getIntegrated();
            const juce::String t = i > -99.0f ? juce::String (i + post, 1) + " LUFS-I" : juce::String ("-- LUFS-I");
            if (t != integratedText) { integratedText = t; repaint (integratedArea); }
        }
    }

    float masterFaderDb()
    {
        if (session.edit == nullptr) return 0.0f;
        auto vp = session.edit->getMasterVolumePlugin();
        return vp != nullptr ? vp->getVolumeDb() : 0.0f;
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        namespace L = Console::Look;
        const auto w = widthNow();
        auto body = getLocalBounds().toFloat().reduced (3.0f);
        L::drawChromePanel (g, body, 10.0f, isSelected() ? 0.85f : 0.20f);

        // name bar: number, colour line, chevron
        if (! nameBar.isEmpty())
        {
            g.setFont (L::caps (w == Width::compact ? 11.5f : 13.0f));
            g.setColour (L::text());
            if (number > 0) g.drawText (juce::String (number), numberArea, juce::Justification::centred);
            else g.drawText ("M", numberArea, juce::Justification::centred);
            L::drawChevron (g, chevronArea.toFloat(), L::text());
            auto c = row != nullptr ? row->getColour() : L::cyan();
            g.setColour (c.withAlpha (0.85f));
            g.fillRoundedRectangle (nameBar.toFloat().removeFromBottom (2.0f).reduced (4.0f, 0.0f), 1.0f);
        }

        // captions
        auto caption = [&] (const juce::String& t, juce::Rectangle<int> r, juce::Colour c, juce::Justification j = juce::Justification::centred)
        {
            if (r.isEmpty()) return;
            g.setColour (c);
            g.setFont (L::caps (w == Width::expanded ? 10.5f : 9.5f));
            g.drawFittedText (t, r, j, 2, 1.0f);
        };
        if (w != Width::compact)
        {
            caption ("COMP", compCaption, L::dim());
            caption ("LIMIT", limitCaption, L::dim());
            caption (w == Width::expanded ? "COMPRESSOR\nGR" : "COMP\nGR", compColCaption, L::cyan());
            caption (w == Width::expanded ? "LEVEL " + juce::String::fromUTF8 ("\xc2\xb7") + " FADER" : "LEVEL", levelCaption, L::text());
            caption (w == Width::expanded ? "LIMITER\nGR" : "LIMIT\nGR", limitColCaption, L::pink());
            if (w == Width::expanded) caption ("METER TYPE", meterCaption, L::dim(), juce::Justification::centredRight);
        }
        else
        {
            caption ("C", compColCaption, L::cyan());
            caption ("L", limitColCaption, L::pink());
            caption ("GR", compFoot, L::dim());
            caption ("GR", limitFoot, L::dim());
        }
        // level well and foot captions
        if (! levelWell.isEmpty()) L::drawWell (g, levelWell.toFloat(), 4.0f);
        g.setFont (Theme::font (9.5f));
        g.setColour (L::dim());
        for (auto r : { dbFootLevel, dbFootComp, dbFootLimit }) if (! r.isEmpty()) g.drawText ("dB", r, juce::Justification::centred);

        // readout well / footer
        if (! readoutWell.isEmpty())
        {
            L::drawWell (g, readoutWell.toFloat(), 6.0f);
            g.setColour (liveShown ? L::live() : L::faint());
            g.fillEllipse (liveDotArea.toFloat());
            if (liveShown) { g.setColour (L::live().withAlpha (0.25f)); g.fillEllipse (liveDotArea.toFloat().expanded (2.5f)); }
        }
        if (! footerArea.isEmpty())
        {
            L::drawWell (g, footerArea.toFloat(), 6.0f);
            g.setColour (L::dim());
            g.setFont (L::caps (10.0f));
            g.drawText ("CHANNEL GAIN", footerCaption, juce::Justification::centredLeft);
            auto pill = livePill.toFloat();
            g.setColour (liveShown ? L::live().withAlpha (0.15f) : juce::Colours::transparentBlack);
            g.fillRoundedRectangle (pill, 5.0f);
            g.setColour (liveShown ? L::live() : L::faint());
            g.drawRoundedRectangle (pill.reduced (0.5f), 5.0f, 1.2f);
            g.setFont (L::caps (10.5f));
            g.drawText (juce::String::fromUTF8 ("\xe2\x80\xa2 LIVE"), pill, juce::Justification::centred);
        }
        if (! integratedArea.isEmpty())
        {
            g.setColour (L::cyan());
            g.setFont (Theme::font (10.5f, true));
            g.drawText (integratedText, integratedArea, juce::Justification::centred);
        }

        // Full view: routing labels and the group box
        if (fullView())
        {
            g.setFont (L::caps (9.0f));
            auto label = [&] (const juce::String& t, int y)
            {
                g.setColour (L::faint());
                g.fillRect (10, y + 11, getWidth() - 20, 1);
                g.setColour (L::dim());
                g.drawText (t, 11, y, getWidth() - 22, 11, juce::Justification::centredLeft);
            };
            auto fjToggle = [&] (int y, bool on)
            {
                auto box = fjBox (y);
                g.setColour (on ? L::cyan().withAlpha (0.35f) : L::well());
                g.fillRoundedRectangle (box.toFloat(), 2.0f);
                g.setColour (on ? L::text() : L::cyan());
                g.setFont (L::caps (8.5f));
                g.drawText (on ? "A-J" : "+F-J", box, juce::Justification::centred);
            };
            if (! insertSlots.empty()) { label ("INSERTS", insertsLabelY); fjToggle (insertsLabelY, Prefs::getBool ("mx_insertsFJ", false)); }
            if (! sendSlots.empty())   { label ("SENDS", sendsLabelY);     fjToggle (sendsLabelY, Prefs::getBool ("mx_sendsFJ", false)); }
            if (ioLabelY > 0) label ("I/O", ioLabelY);
            if (panL.isVisible()) label (stereoPan ? "PAN  L / R" : "PAN", panLabelY);
            if (! groupBox.isEmpty())
            {
                juce::String groups;
                for (auto& gr : session.getGroups()) if (std::find (gr.tracks.begin(), gr.tracks.end(), row->itemID) != gr.tracks.end()) groups << gr.id;
                L::drawWell (g, groupBox.toFloat(), 3.0f);
                g.setColour (groups.isNotEmpty() ? L::live() : L::dim());
                g.setFont (L::caps (9.0f));
                auto txt = groups.isNotEmpty() ? "GRP " + groups : juce::String ("NO GROUP");
                if (folder != nullptr) txt = (folder->isSubmixFolder() ? "FOLDER  " : "VCA  ") + txt;
                g.drawText (txt, groupBox, juce::Justification::centred);
            }
        }
    }

    /** Inactive tracks are greyed out like Pro Tools (Track > Make Active brings them back). Drag target marker. */
    void paintOverChildren (juce::Graphics& g) override
    {
        if (row != nullptr && ! row->isProcessing (false))
        {
            g.setColour (Console::Look::bodyBot().withAlpha (0.66f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 10.0f);
            g.setColour (Console::Look::dim());
            g.setFont (Console::Look::caps (11.0f));
            g.drawText ("INACTIVE", getLocalBounds().withHeight (60).withY (getHeight() / 2 - 30), juce::Justification::centred);
        }
        if (dropMarker != 0)
        {
            g.setColour (Console::Look::cyan());
            g.fillRect (dropMarker < 0 ? 0 : getWidth() - 3, 8, 3, getHeight() - 16);
        }
    }
    int dropMarker = 0;   // -1 = drop before this strip, +1 = after

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto p = e.getEventRelativeTo (this).getPosition();
        if (fullView())
        {
            if (! insertSlots.empty() && fjBox (insertsLabelY).expanded (3).contains (p)) { Prefs::set ("mx_insertsFJ", ! Prefs::getBool ("mx_insertsFJ", false)); return; }
            if (! sendSlots.empty()   && fjBox (sendsLabelY).expanded (3).contains (p))   { Prefs::set ("mx_sendsFJ",   ! Prefs::getBool ("mx_sendsFJ", false)); return; }
        }
        if (chevronArea.expanded (4).contains (p) || (e.mods.isPopupMenu() && nameBar.contains (p))) { showStripMenu(); return; }
        if (! readoutWell.isEmpty() && readoutWell.contains (p) && ! gainText.getBounds().contains (p)) { reader.resetPeak(); return; }
        draggingName = nameBar.contains (p) && row != nullptr;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingName && std::abs (e.getDistanceFromDragStartX()) > 10 && onReorderDrag) onReorderDrag (*this, e, false);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (draggingName && std::abs (e.getDistanceFromDragStartX()) > 10 && onReorderDrag) onReorderDrag (*this, e, true);
        draggingName = false;
    }

    //==========================================================================
    void resized() override
    {
        clearAreas();
        const auto w = widthNow();
        auto r = getLocalBounds().reduced (10, 10);

        // name bar (expanded: the width selector sits at its right like the reference)
        nameBar = r.removeFromTop (26);
        {
            auto nb = nameBar;
            if (w == Width::expanded) { widthSel.setBounds (nb.removeFromRight (78).withSizeKeepingCentre (78, 22)); nb.removeFromRight (6); }
            numberArea = nb.removeFromLeft (w == Width::compact ? 14 : 22);
            chevronArea = nb.removeFromRight (14);
            nb.removeFromLeft (w == Width::compact ? 1 : 6);
            name.setBounds (nb.withSizeKeepingCentre (nb.getWidth(), 18));   // one line: long names end in an ellipsis, never wrap
            name.setFont (w == Width::compact ? Theme::font (11.5f, true) : Console::Look::caps (13.0f));
            name.setJustificationType (w == Width::standard ? juce::Justification::centred : juce::Justification::centredLeft);
        }
        if (w != Width::expanded)
        {
            r.removeFromTop (6);
            widthSel.setBounds (r.removeFromTop (22).withSizeKeepingCentre (juce::jmin (r.getWidth(), 78), 22));
        }
        r.removeFromTop (8);

        // Full view: the Pro Tools section
        const bool full = fullView();
        for (auto* c : { (juce::Component*) &autoBtn, (juce::Component*) &inputBtn, (juce::Component*) &outputBtn, (juce::Component*) &rec, (juce::Component*) &mon,
                         (juce::Component*) &polarityBtn, (juce::Component*) &panL, (juce::Component*) &panR })
            c->setVisible (false);
        monoBtn.setVisible (false);
        if (full) layoutFullSection (r);

        // dynamics header
        const bool dyn = hasInserts();
        compDial.setVisible (w == Width::compact); limitDial.setVisible (w == Width::compact);
        compSel.setVisible (w != Width::compact); limitSel.setVisible (w != Width::compact);
        meterSel.setVisible (w == Width::expanded && Console::Model::canTap (row));
        if (w == Width::compact)
        {
            auto dials = r.removeFromTop (36);
            const int d = juce::jmin (36, (dials.getWidth() - 6) / 2);
            compDial.setBounds (dials.removeFromLeft (dials.getWidth() / 2).withSizeKeepingCentre (d, d));
            limitDial.setBounds (dials.withSizeKeepingCentre (d, d));
            r.removeFromTop (8);
        }
        else
        {
            if (w == Width::expanded)
            {
                auto row1 = r.removeFromTop (24);
                meterCaption = row1.removeFromLeft (row1.getWidth() * 2 / 5);
                row1.removeFromLeft (10);
                meterSel.setBounds (row1);
                if (! meterSel.isVisible()) meterCaption = {};
                r.removeFromTop (10);
            }
            auto caps = r.removeFromTop (12);
            auto sels = r.removeFromTop (w == Width::expanded ? 28 : 26);
            const int half = (sels.getWidth() - 6) / 2;
            compCaption = caps.removeFromLeft (half); limitCaption = caps.removeFromRight (half);
            compSel.setBounds (sels.removeFromLeft (half));
            limitSel.setBounds (sels.removeFromRight (half));
            r.removeFromTop (10);
        }
        juce::ignoreUnused (dyn);

        // bottom: M / S and the readout (compact / standard) or the CHANNEL GAIN footer (expanded)
        const bool hasMS = row != nullptr;
        if (w == Width::expanded)
        {
            footerArea = r.removeFromBottom (32);
            auto f = footerArea.reduced (8, 4);
            if (hasMS)
            {
                solo.setBounds (f.removeFromRight (26).withSizeKeepingCentre (26, 22)); f.removeFromRight (4);
                mute.setBounds (f.removeFromRight (26).withSizeKeepingCentre (26, 22)); f.removeFromRight (8);
            }
            livePill = f.removeFromRight (54).withSizeKeepingCentre (54, 20); f.removeFromRight (6);
            footerCaption = f.removeFromLeft (juce::jmin (104, f.getWidth() / 2));
            gainText.setBounds (f);
            gainText.setJustificationType (juce::Justification::centredLeft);
            if (row == nullptr) { r.removeFromBottom (4); integratedArea = r.removeFromBottom (14); }
            r.removeFromBottom (8);
        }
        else
        {
            if (hasMS)
            {
                auto ms = r.removeFromBottom (24);
                const int bw = w == Width::compact ? juce::jmin (26, (ms.getWidth() - 6) / 2) : 30;
                auto pair = ms.withSizeKeepingCentre (bw * 2 + (w == Width::compact ? 6 : 14), 24);
                mute.setBounds (pair.removeFromLeft (bw));
                solo.setBounds (pair.removeFromRight (bw));
                r.removeFromBottom (8);
            }
            readoutWell = r.removeFromBottom (28);
            auto rw = readoutWell.reduced (6, 0);
            liveDotArea = rw.removeFromRight (10).withSizeKeepingCentre (8, 8);
            gainText.setBounds (rw);
            gainText.setJustificationType (juce::Justification::centred);
            gainText.setFont (Theme::font (w == Width::compact ? 11.5f : 13.5f, true));
            if (row == nullptr) { r.removeFromBottom (4); integratedArea = r.removeFromBottom (14); }
            r.removeFromBottom (10);
        }
        mute.setVisible (hasMS); solo.setVisible (hasMS);

        // meter section
        layoutMeters (r, w);
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

    void showDynamicsMenu (bool limit)
    {
        if (! hasInserts()) return;
        auto current = limit ? limitPlugin : compPlugin;
        juce::PopupMenu m;
        m.addSectionHeader (limit ? "LIMIT - last before the fader" : "COMP - before the limiter");
        if (current != nullptr) { m.addItem (1, "Open " + Console::Model::labelFor (current.get(), limit)); m.addItem (2, current->isEnabled() ? "Bypass" : "Un-bypass"); m.addSeparator(); }
        int id = 100;
        for (auto& c : (limit ? Console::Model::limitChoices() : Console::Model::compChoices()))
        {
            const bool allowed = Edition::isTypeAllowed (c.type);
            const bool isCur = current != nullptr && Console::Model::labelFor (current.get(), limit) == c.name;
            m.addItem (id++, juce::String (c.name) + (allowed ? juce::String() : "   (locked - " + juce::String (Edition::productForType (c.type)->name) + ")"), true, isCur);
        }
        m.addSeparator();
        m.addItem (3, "None (remove)", current != nullptr);
        auto* target = limit ? (juce::Component*) &limitSel : (juce::Component*) &compSel;
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this, limit, current, alive = aliveFlag] (int r)
        {
            if (! *alive || r == 0) return;
            if (r == 1 && current != nullptr) { current->showWindowExplicitly(); return; }
            if (r == 2 && current != nullptr) { current->setEnabled (! current->isEnabled()); refresh(); return; }
            if (r == 3) { Console::Model::setDynamics (session, row, limit, {}); refresh(); return; }
            auto& list = limit ? Console::Model::limitChoices() : Console::Model::compChoices();
            const int idx = r - 100;
            if (idx < 0 || idx >= (int) list.size()) return;
            auto& c = list[(size_t) idx];
            if (! Edition::isTypeAllowed (c.type)) { if (Edition::showUnlock()) Edition::showUnlock() (Edition::productForType (c.type)->code); return; }
            auto err = Console::Model::setDynamics (session, row, limit, c.name);
            if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Squelchmoth", err);
            else if (Prefs::getBool ("pl_openWindowOnInsert", true))
                if (auto p = Console::Model::findDynamics (getPluginList(), track, limit)) p->showWindowExplicitly();
            refresh();
        });
    }

    void showMeterTypeMenu()
    {
        if (! Console::Model::canTap (row)) return;
        juce::PopupMenu m;
        auto names = Console::meterTypeNames();
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, (int) meterType == i);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&meterSel), [this, alive = aliveFlag] (int r)
        {
            if (! *alive || r <= 0) return;
            Console::Model::setMeterType (session, row, (MeterType) (r - 1));
            refresh();
        });
    }

    void showStripMenu()
    {
        juce::PopupMenu m, widths, meters;
        auto wn = Console::widthNames();
        for (int i = 0; i < 3; ++i) widths.addItem (10 + i, wn[i], true, (int) Console::Model::storedWidth (session, row) == i);
        m.addSubMenu ("Strip width", widths);
        if (Console::Model::canTap (row))
        {
            auto mn = Console::meterTypeNames();
            for (int i = 0; i < mn.size(); ++i) meters.addItem (20 + i, mn[i], true, (int) meterType == i);
            m.addSubMenu ("Meter type", meters);
        }
        m.addSeparator();
        if (row != nullptr) m.addItem (1, "Rename...");
        if (hasPolarity) m.addItem (2, "Polarity invert", true, TrackOps::isPolarityInverted (*row));
        if (hasMono) m.addItem (3, "Mono (track format)", true, monoBtn.getToggleState());
        m.addItem (4, "Reset meter peaks");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (chevronArea)), [this, alive = aliveFlag] (int r)
        {
            if (! *alive || r == 0) return;
            if (r >= 10 && r < 13) { Console::Model::setWidth (session, row, (Width) (r - 10)); if (onWidthChanged) onWidthChanged(); }
            else if (r >= 20 && r < 24) Console::Model::setMeterType (session, row, (MeterType) (r - 20));
            else if (r == 1) name.showEditor();
            else if (r == 2) polarityBtn.onClick();
            else if (r == 3) monoBtn.onClick();
            else if (r == 4) led.onClick();
            refresh();
        });
    }

private:
    te::Track::Ptr keepAlive;
    TimelineState* state;
    juce::Label name, gainText;
    Console::WidthSelector widthSel;
    Console::Selector compSel { Console::Look::cyan() }, limitSel { Console::Look::pink() }, meterSel { Console::Look::cyan().withAlpha (0.8f) };
    Console::GrMeter compGr { Console::Look::cyan() }, limitGr { Console::Look::pink() };
    Console::VuDial compDial { Console::Look::cyan() }, limitDial { Console::Look::pink() };
    Console::Scale levelScale, compScale, limitScale;
    Console::LedMeter led;
    juce::Slider fader;
    PanKnob panL, panR;
    PeakReader reader, shown;
    Console::KeyButton solo { "S", Console::Look::yellow() }, mute { "M", Console::Look::orange() },
                       rec { "R", Console::Look::red() }, mon { "I", Console::Look::live() },
                       polarityBtn { juce::String::fromUTF8 ("\xc3\x98"), Console::Look::cyan() },
                       monoBtn { "mono", Console::Look::cyan() };
    juce::TextButton autoBtn { "auto read" }, inputBtn, outputBtn;
    std::vector<std::unique_ptr<InsertSlot>> insertSlots;
    std::vector<std::unique_ptr<SendSlot>> sendSlots;
    te::Plugin::Ptr compPlugin, limitPlugin;
    SqStripTapPlugin* tap = nullptr;   // owned by the plug-in list; refreshed every 200 ms
    MeterType meterType = MeterType::peak;
    int insertsLabelY = 0, sendsLabelY = 0, ioLabelY = 0, panLabelY = 0, number = 0, liveTicks = 0, lufsTicks = 0;
    bool stereoPan = false, hasRecButtons = false, hasPolarity = false, hasMono = false, liveShown = false, applyingGang = false, draggingName = false;
    float lastFaderValue = 0.0f;
    juce::String integratedText { "-- LUFS-I" };
    te::EditPlaybackContext* lastContext = nullptr;
    juce::Rectangle<int> nameBar, numberArea, chevronArea, compCaption, limitCaption, meterCaption, compColCaption, levelCaption, limitColCaption,
                         compFoot, limitFoot, levelWell, dbFootLevel, dbFootComp, dbFootLimit, readoutWell, liveDotArea, footerArea, footerCaption,
                         livePill, integratedArea, groupBox;

    void clearAreas()
    {
        for (auto* a : { &nameBar, &numberArea, &chevronArea, &compCaption, &limitCaption, &meterCaption, &compColCaption, &levelCaption, &limitColCaption,
                         &compFoot, &limitFoot, &levelWell, &dbFootLevel, &dbFootComp, &dbFootLimit, &readoutWell, &liveDotArea, &footerArea, &footerCaption,
                         &livePill, &integratedArea, &groupBox })
            *a = {};
        insertsLabelY = sendsLabelY = ioLabelY = panLabelY = 0;
    }

    void layoutFullSection (juce::Rectangle<int>& r)
    {
        if (row != nullptr) { autoBtn.setVisible (true); autoBtn.setBounds (r.removeFromTop (20)); r.removeFromTop (6); }
        const int slotH = 18;
        if (! insertSlots.empty())
        {
            insertsLabelY = r.getY();
            r.removeFromTop (14);
            for (auto& s : insertSlots) { s->setBounds (r.removeFromTop (slotH)); r.removeFromTop (1); }
            r.removeFromTop (6);
        }
        if (! sendSlots.empty())
        {
            sendsLabelY = r.getY();
            r.removeFromTop (14);
            for (auto& s : sendSlots) { s->setBounds (r.removeFromTop (slotH + 6)); r.removeFromTop (1); }
            r.removeFromTop (6);
        }
        if (track != nullptr || row == nullptr)
        {
            ioLabelY = r.getY();
            auto lab = r.removeFromTop (14);
            juce::ignoreUnused (lab);
            if (track != nullptr) { inputBtn.setVisible (true); inputBtn.setBounds (r.removeFromTop (20)); r.removeFromTop (2); }
            outputBtn.setVisible (true);
            outputBtn.setBounds (r.removeFromTop (20));
            r.removeFromTop (6);
        }
        // pan
        const bool showPan = ! (folder != nullptr && folder->getVCAPlugin() != nullptr) && widthNow() != Width::compact;
        if (showPan)
        {
            panLabelY = r.getY();
            r.removeFromTop (13);
            auto panArea = r.removeFromTop (50);
            panL.setVisible (true);
            panR.setVisible (stereoPan);
            if (stereoPan)
            {
                auto half = panArea.getWidth() / 2;
                panL.setBounds (panArea.removeFromLeft (half).withSizeKeepingCentre (juce::jmin (half, 46), 50));
                panR.setBounds (panArea.withSizeKeepingCentre (juce::jmin (half, 46), 50));
            }
            else panL.setBounds (panArea.withSizeKeepingCentre (46, 50));
            r.removeFromTop (6);
        }
        else if (widthNow() == Width::compact && ! (folder != nullptr && folder->getVCAPlugin() != nullptr))
        {
            panL.setVisible (true);
            panLabelY = r.getY();
            r.removeFromTop (13);
            panL.setBounds (r.removeFromTop (44).withSizeKeepingCentre (40, 44));
            r.removeFromTop (6);
        }
        // buttons row: R I Ø Mono
        juce::Array<juce::Component*> btns;
        if (hasRecButtons) { btns.add (&rec); btns.add (&mon); }
        if (hasPolarity) btns.add (&polarityBtn);
        if (hasMono) btns.add (&monoBtn);
        if (! btns.isEmpty())
        {
            auto b = r.removeFromTop (22);
            const int bw = juce::jmin (28, (b.getWidth() - 4 * (btns.size() - 1)) / btns.size());
            auto rowArea = b.withSizeKeepingCentre (bw * btns.size() + 4 * (btns.size() - 1), 22);
            for (auto* c : btns) { c->setVisible (true); c->setBounds (rowArea.removeFromLeft (bw)); rowArea.removeFromLeft (4); }
            r.removeFromTop (6);
        }
        if (row != nullptr) { groupBox = r.removeFromTop (16); r.removeFromTop (8); }
    }

    void layoutMeters (juce::Rectangle<int> r, Width w)
    {
        const int capH = w == Width::compact ? 14 : 26, footH = 14;
        auto caps = r.removeFromTop (capH);
        r.removeFromTop (4);
        auto foot = r.removeFromBottom (footH);
        r.removeFromBottom (6);   // room for the bottom scale label above the dB foot
        // r is now the shared vertical span of every meter (so 0 dB and 0 GR align across columns)
        if (w == Width::compact)
        {
            const int grW = 9, gap = 4;
            auto cCol = r.removeFromLeft (grW); r.removeFromLeft (gap);
            auto lCol = r.removeFromRight (grW); r.removeFromRight (gap);
            compGr.setBounds (cCol); limitGr.setBounds (lCol);
            compGr.segmented = limitGr.segmented = false;
            compColCaption = caps.withX (cCol.getX() - 3).withWidth (grW + 6);
            limitColCaption = caps.withX (lCol.getX() - 3).withWidth (grW + 6);
            compFoot = foot.withX (cCol.getX() - 4).withWidth (grW + 8);
            limitFoot = foot.withX (lCol.getX() - 4).withWidth (grW + 8);
            levelWell = r;
            auto inner = r.reduced (3, 3);
            led.setBounds (inner);
            fader.setBounds (inner.expanded (0, 9));
            levelScale.setVisible (false); compScale.setVisible (false); limitScale.setVisible (false);
        }
        else
        {
            const bool ex = w == Width::expanded;
            const int scaleW = ex ? 22 : 20, grBar = ex ? 46 : 10, gap = ex ? 8 : 4;
            const int grColW = scaleW + grBar;
            auto cCol = r.removeFromLeft (grColW); r.removeFromLeft (gap);
            auto lCol = r.removeFromRight (grColW); r.removeFromRight (gap);
            auto cScale = cCol.removeFromLeft (scaleW), lScale = lCol.removeFromRight (scaleW);
            compGr.setBounds (cCol); limitGr.setBounds (lCol);
            compGr.segmented = limitGr.segmented = true;
            compScale.setBounds (cScale.expanded (0, 8)); compScale.meterArea = cCol.reduced (0, 2);
            limitScale.setBounds (lScale.expanded (0, 8)); limitScale.meterArea = lCol.reduced (0, 2);
            const int capW = ex ? 92 : 50;   // captions may overhang the narrow GR columns, centred on them
            compColCaption = caps.withWidth (capW).withCentre ({ cScale.getX() + grColW / 2, caps.getCentreY() });
            limitColCaption = caps.withWidth (capW).withCentre ({ lCol.getX() + grColW / 2, caps.getCentreY() });
            compColCaption.setX (juce::jmax (compColCaption.getX(), 8));
            limitColCaption.setX (juce::jmin (limitColCaption.getX(), getWidth() - 8 - capW));
            dbFootComp = foot.withX (cScale.getX()).withWidth (scaleW);
            dbFootLimit = foot.withX (lScale.getX()).withWidth (scaleW);
            levelWell = r;
            levelCaption = caps.withX (r.getX()).withWidth (r.getWidth());
            // keep the overhanging GR captions clear of the LEVEL caption
            levelCaption = levelCaption.withTrimmedLeft (juce::jmax (0, compColCaption.getRight() - levelCaption.getX() + 2))
                                       .withTrimmedRight (juce::jmax (0, levelCaption.getRight() - limitColCaption.getX() + 2));
            auto inner = r.reduced (3, 3);
            auto lsc = inner.removeFromLeft (ex ? 24 : 20);
            inner.removeFromLeft (2);
            levelScale.setBounds (lsc.expanded (0, 8));
            levelScale.meterArea = inner.withTrimmedTop (6);   // the LED's clip light takes the top 6 px
            led.setBounds (inner);
            fader.setBounds (inner.withTrimmedTop (6).expanded (0, 9));
            dbFootLevel = foot.withX (r.getX()).withWidth (r.getWidth());
            levelScale.setVisible (true); compScale.setVisible (true); limitScale.setVisible (true);
        }
        led.stereo = true;
    }

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
    void applyVolume (float v)
    {
        if (track) session.setTrackVolume (*track, v);
        else if (folder && folder->getVCAPlugin()) folder->getVCAPlugin()->setVolumeDb (v);
        else if (auto vp = getVolumePlugin()) vp->setVolumeDb (v);
    }
    void setFaderQuietly (float db)
    {
        fader.setValue (db, juce::dontSendNotification);
        lastFaderValue = (float) fader.getValue();
    }
    void updateGainText()
    {
        if (! gainText.isBeingEdited()) gainText.setText (dbText ((float) fader.getValue()), juce::dontSendNotification);
        gainText.setColour (juce::Label::textColourId, reader.maxPeak >= 0.0f ? Console::Look::red() : Console::Look::text());
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
/** Console toolbar: View (Dynamics / Full / Overview), Channel Layout (with saved layouts) and the more menu. */
class ConsoleToolbar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ConsoleToolbar (Session& s) : session (s) {}
    std::function<void()> onChange;

    void paint (juce::Graphics& g) override
    {
        namespace L = Console::Look;
        auto r = getLocalBounds().toFloat().reduced (4.0f, 4.0f);
        L::drawChromePanel (g, r, 9.0f, 0.12f);
        g.setFont (L::caps (11.5f, false));
        g.setColour (L::text());
        g.drawText ("View:", viewCaption, juce::Justification::centredRight);
        // view buttons
        for (int i = 0; i < 3; ++i)
        {
            auto b = viewButtons[i].toFloat();
            const bool on = (int) Console::Model::view() == i;
            L::drawPill (g, b, L::cyan(), on, hover == i);
            drawViewGlyph (g, i, b.reduced (8.0f, 6.0f), on ? L::cyan() : L::text());
        }
        // channel layout pill
        auto lp = layoutPill.toFloat();
        L::drawPill (g, lp, L::dim(), false, hover == 10);
        auto txt = lp.reduced (12.0f, 0.0f);
        auto chev = txt.removeFromRight (14.0f);
        L::drawChevron (g, chev, L::text());
        g.setColour (L::text());
        g.setFont (Theme::font (12.0f));
        g.drawText ("Channel Layout:  " + currentLayoutName(), txt, juce::Justification::centredLeft);
        // more
        auto mp = moreButton.toFloat();
        L::drawPill (g, mp, L::dim(), false, hover == 11);
        g.setColour (L::text());
        for (int i = -1; i <= 1; ++i) g.fillEllipse (mp.getCentreX() - 1.6f, mp.getCentreY() + (float) i * 5.5f - 1.6f, 3.2f, 3.2f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 9);
        viewCaption = r.removeFromLeft (42);
        r.removeFromLeft (8);
        for (int i = 0; i < 3; ++i) { viewButtons[i] = r.removeFromLeft (44); r.removeFromLeft (4); }
        moreButton = r.removeFromRight (38);
        r.removeFromRight (8);
        layoutPill = r.removeFromRight (juce::jmin (280, r.getWidth()));
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1;
        for (int i = 0; i < 3; ++i) if (viewButtons[i].contains (e.getPosition())) h = i;
        if (layoutPill.contains (e.getPosition())) h = 10;
        if (moreButton.contains (e.getPosition())) h = 11;
        if (h != hover) { hover = h; repaint(); }
        static const char* tips[] = { "Dynamics: the console strips", "Full: adds inserts, sends, I/O, pan and track buttons", "Overview: every strip Compact" };
        setTooltip (h >= 0 && h < 3 ? tips[h] : (h == 10 ? "Channel Layout: per-strip widths, all one width, or a saved layout" : (h == 11 ? "More console options" : "")));
    }
    void mouseExit (const juce::MouseEvent&) override   { hover = -1; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        auto p = e.getPosition();
        for (int i = 0; i < 3; ++i) if (viewButtons[i].contains (p)) { Console::Model::setView ((Console::View) i); changed(); return; }
        if (layoutPill.contains (p)) { showLayoutMenu(); return; }
        if (moreButton.contains (p)) { showMoreMenu(); return; }
    }

private:
    Session& session;
    juce::Rectangle<int> viewCaption, viewButtons[3], layoutPill, moreButton;
    int hover = -1;
    juce::String lastRecalled;

    void changed()   { repaint(); if (onChange) onChange(); }

    juce::String currentLayoutName() const
    {
        auto l = Console::Model::layout();
        if (l == Console::Layout::flexible && lastRecalled.isNotEmpty()) return lastRecalled;
        return Console::layoutNames()[(int) l];
    }

    static void drawViewGlyph (juce::Graphics& g, int i, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        if (i == 0)   // strips: three faders
        {
            for (int k = 0; k < 3; ++k)
            {
                const float x = r.getX() + r.getWidth() * (0.2f + 0.3f * (float) k);
                g.fillRect (x - 0.75f, r.getY(), 1.5f, r.getHeight());
                const float y = r.getY() + r.getHeight() * (k == 1 ? 0.25f : (k == 0 ? 0.6f : 0.45f));
                g.fillRoundedRectangle (x - 3.0f, y - 2.0f, 6.0f, 4.0f, 1.0f);
            }
        }
        else if (i == 1)   // brackets: full strip with a section on top
        {
            g.drawRoundedRectangle (r, 2.0f, 1.3f);
            g.fillRect (r.getX() + 2.0f, r.getY() + r.getHeight() * 0.38f, r.getWidth() - 4.0f, 1.3f);
            g.fillRect (r.getCentreX() - 0.65f, r.getY() + r.getHeight() * 0.38f, 1.3f, r.getHeight() * 0.62f - 2.0f);
        }
        else   // table: overview grid
        {
            g.drawRoundedRectangle (r, 2.0f, 1.3f);
            for (int k = 1; k < 3; ++k) g.fillRect (r.getX(), r.getY() + r.getHeight() * (float) k / 3.0f, r.getWidth(), 1.0f);
            g.fillRect (r.getX() + r.getWidth() * 0.35f, r.getY(), 1.0f, r.getHeight());
        }
    }

    void showLayoutMenu()
    {
        juce::PopupMenu m;
        auto names = Console::layoutNames();
        for (int i = 0; i < names.size(); ++i) m.addItem (1 + i, names[i], true, (int) Console::Model::layout() == i && lastRecalled.isEmpty());
        auto saved = Console::Model::layoutList (session);
        m.addSeparator();
        if (! saved.isEmpty())
        {
            m.addSectionHeader ("Saved layouts");
            for (int i = 0; i < saved.size(); ++i) m.addItem (100 + i, saved[i], true, saved[i] == lastRecalled);
            juce::PopupMenu del;
            for (int i = 0; i < saved.size(); ++i) del.addItem (200 + i, saved[i]);
            m.addSubMenu ("Delete layout", del);
        }
        m.addItem (50, "Save current layout...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (layoutPill)), [this, saved] (int r)
        {
            if (r >= 1 && r <= 4) { lastRecalled = {}; Console::Model::setLayout ((Console::Layout) (r - 1)); changed(); }
            else if (r >= 100 && r < 200) { if (Console::Model::recallLayout (session, saved[r - 100])) lastRecalled = saved[r - 100]; changed(); }
            else if (r >= 200) { Console::Model::deleteLayout (session, saved[r - 200]); if (lastRecalled == saved[r - 200]) lastRecalled = {}; changed(); }
            else if (r == 50) askLayoutName();
        });
    }

    void askLayoutName()
    {
        auto* aw = new juce::AlertWindow ("Save mixer layout", "Name this layout (widths, meter types and view are saved in the session).", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "Layout " + juce::String (Console::Model::layoutList (session).size() + 1));
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            if (r == 1)
            {
                auto n = aw->getTextEditorContents ("name").trim();
                if (n.isNotEmpty()) { Console::Model::saveLayout (session, n); lastRecalled = n; changed(); }
            }
        }), true);
    }

    void showMoreMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Inserts in Full view", true, Prefs::showInsertsInMix());
        m.addItem (2, "Sends in Full view", true, Prefs::showSendsInMix());
        m.addItem (3, "Inserts F-J (10 slots)", true, Prefs::getBool ("mx_insertsFJ", false));
        m.addItem (4, "Sends F-J (10 slots)", true, Prefs::getBool ("mx_sendsFJ", false));
        m.addSeparator();
        m.addItem (5, "Reset all meter peaks");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (moreButton)), [this] (int r)
        {
            if (r == 1) Prefs::set ("mx_showInserts", ! Prefs::showInsertsInMix());
            else if (r == 2) Prefs::set ("mx_showSends", ! Prefs::showSendsInMix());
            else if (r == 3) Prefs::set ("mx_insertsFJ", ! Prefs::getBool ("mx_insertsFJ", false));
            else if (r == 4) Prefs::set ("mx_sendsFJ", ! Prefs::getBool ("mx_sendsFJ", false));
            else if (r == 5) clipIndicatorResetCounter()++;
            changed();
        });
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
    explicit MixerView (Session& s, TimelineState* st = nullptr) : session (s), state (st), toolbar (s)
    {
        addAndMakeVisible (toolbar);
        toolbar.onChange = [this] { rebuild(); };
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&strips, false);
        viewport.setScrollBarsShown (true, true);
        strips.addMouseListener (this, false);   // right-click on empty console space = new track menu
        strips.addAndMakeVisible (newTrackBtn);
        newTrackBtn.setTooltip ("New Tracks (" SQ_CMD "+Shift+N) - right-click for the quick track menu");
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

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::mixBg());
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a0f16), 0.0f, 0.0f, juce::Colour (0xff05080c), 0.0f, (float) getHeight(), false));
        g.fillRect (getLocalBounds().withTrimmedTop (toolbar.getBottom()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        toolbar.setBounds (r.removeFromTop (toolbarHeight));
        if (master != nullptr) master->setBounds (r.removeFromRight (master->preferredWidth() + 12).reduced (4, 4).withTrimmedBottom (bracketHeight));
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
        // click on a strip's background selects that track (Shift / Cmd = add)
        if (state == nullptr) return;
        for (auto& c : channel)
            if (c->getRow() != nullptr && c->getScreenBounds().contains (e.getScreenPosition()))
            {
                state->selectTrack (c->getRow(), e.mods.isShiftDown() || e.mods.isCommandDown());
                for (auto& s : channel) s->repaint();
            }
    }

    int stripHeightMin() const
    {
        int h = 470;   // the console: >= 230 px of fader travel
        if (Console::Model::view() == Console::View::full)
        {
            h += 26 + 52 + 64 + 28 + 24;
            if (Prefs::showInsertsInMix()) h += 14 + (Prefs::getBool ("mx_insertsFJ", false) ? 10 : 5) * 19 + 6;
            if (Prefs::showSendsInMix())   h += 14 + (Prefs::getBool ("mx_sendsFJ", false) ? 10 : 5) * 25 + 6;
        }
        return h;
    }

    // used by tests / MCP
    juce::Array<ChannelStrip*> getStrips()   { juce::Array<ChannelStrip*> a; for (auto& c : channel) a.add (c.get()); return a; }
    ChannelStrip* getMasterStrip()           { return master.get(); }

private:
    Session& session;
    TimelineState* state;
    ConsoleToolbar toolbar;
    juce::Viewport viewport;
    juce::TextButton newTrackBtn { "+ TRACK" };
    juce::Component strips;
    std::vector<std::unique_ptr<ChannelStrip>> channel;
    std::unique_ptr<ChannelStrip> master;
    juce::ValueTree watched;
    int refreshCounter = 0;
    size_t collapsedHash = 0;
    static constexpr int toolbarHeight = 50, bracketHeight = 34, stripGap = 6;

    struct Bracket { int x0, x1; Console::Width w; };
    std::vector<Bracket> brackets;

    struct BracketPainter : public juce::Component
    {
        std::vector<Bracket>* list = nullptr;
        void paint (juce::Graphics& g) override
        {
            if (list == nullptr) return;
            namespace L = Console::Look;
            auto names = juce::StringArray { "COMPACT", "STANDARD", "EXPANDED" };
            for (auto& b : *list)
            {
                const float x0 = (float) b.x0 + 6.0f, x1 = (float) b.x1 - 6.0f, y = 6.0f;
                g.setColour (L::dim().withAlpha (0.8f));
                juce::Path p;
                p.startNewSubPath (x0, y); p.lineTo (x0, y + 7.0f); p.lineTo (x1, y + 7.0f); p.lineTo (x1, y);
                g.strokePath (p, juce::PathStrokeType (1.2f));
                g.setColour (L::text().withAlpha (0.85f));
                g.setFont (L::caps (11.0f, false).withExtraKerningFactor (0.25f));
                g.drawText (names[(int) b.w], juce::Rectangle<float> (x0, y + 9.0f, x1 - x0, 16.0f), juce::Justification::centred);
            }
        }
    } bracketPainter;

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
            || id == juce::Identifier ("sqmMasterFader") || id == juce::Identifier ("sqmMasterHidden")
            || id == Console::Model::widthId || id == Console::Model::masterWidthId || id == Console::Model::roleId) triggerAsyncUpdate();
        juce::ignoreUnused (v);
    }

    static bool isRelevant (const juce::ValueTree& c)
    {
        if (c.hasType (te::IDs::PLUGIN) && c.getProperty (te::IDs::type).toString() == SqStripTapPlugin::xmlTypeName) return false;   // meters are not layout
        return c.hasType (te::IDs::TRACK) || c.hasType (te::IDs::PLUGIN) || c.hasType (te::IDs::FOLDERTRACK);
    }

    void handleAsyncUpdate() override   { rebuild(); }

    size_t hashCollapsed() const
    {
        size_t h = 0;
        if (state != nullptr) for (auto& id : state->collapsedFolders) h = h * 31 + std::hash<juce::String>() (id.toString());
        return h;
    }

    void rebuild()
    {
        channel.clear();
        master.reset();
        brackets.clear();
        if (session.edit == nullptr) { resized(); return; }
        collapsedHash = hashCollapsed();
        for (auto t : session.getRows (state != nullptr ? &state->collapsedFolders : nullptr))   // FL wish: folders collapse in the mixer too
        {
            if (t->isHidden()) continue;
            if (trackFilter && ! trackFilter (*t)) continue;
            auto s = std::make_unique<ChannelStrip> (session, t, state);
            wire (*s);
            strips.addAndMakeVisible (*s);
            channel.push_back (std::move (s));
        }
        if (! trackFilter && TrackOps::hasMasterFader (session) && ! (bool) session.edit->state.getProperty ("sqmMasterHidden", false))
        {
            master = std::make_unique<ChannelStrip> (session, nullptr, state);
            master->onWidthChanged = [this] { resized(); };
            addAndMakeVisible (*master);
        }
        resized();
    }

    void wire (ChannelStrip& s)
    {
        s.addMouseListener (this, false);
        s.onWidthChanged = [this] { layoutStrips(); for (auto& c : channel) c->resized(); if (master) master->resized(); };
        s.onGang = [this] (ChannelStrip& from, float delta)
        {
            if (! from.isSelected()) return;
            for (auto& c : channel)
                if (c.get() != &from && c->isSelected()) c->nudgeFader (delta);
        };
        s.onReorderDrag = [this] (ChannelStrip& from, const juce::MouseEvent& e, bool finished) { reorderDrag (from, e, finished); };
    }

    /** Logic wish: drag-reorder strips. Drop position = the strip under the mouse, left or right half. */
    void reorderDrag (ChannelStrip& from, const juce::MouseEvent& e, bool finished)
    {
        auto pos = e.getEventRelativeTo (&strips).getPosition();
        ChannelStrip* target = nullptr;
        for (auto& c : channel) { c->dropMarker = 0; if (c->getBounds().contains (pos.withY (c->getY() + 10))) target = c.get(); }
        int side = 0;
        if (target != nullptr && target != &from) side = pos.x < target->getBounds().getCentreX() ? -1 : 1;
        if (target != nullptr && target != &from) target->dropMarker = side;
        for (auto& c : channel) c->repaint();
        if (! finished || target == nullptr || target == &from || session.edit == nullptr) { if (finished) for (auto& c : channel) { c->dropMarker = 0; c->repaint(); } return; }
        for (auto& c : channel) c->dropMarker = 0;
        auto* t = from.getRow();
        auto* dest = target->getRow();
        if (t == nullptr || dest == nullptr) return;
        auto parent = dest->getParentFolderTrack();
        auto siblings = parent != nullptr ? parent->getAllSubTracks (false) : te::getTopLevelTracks (*session.edit);
        siblings.removeFirstMatchingValue (t);
        int di = siblings.indexOf (dest);
        te::Track* preceding = side < 0 ? (di > 0 ? siblings[di - 1] : nullptr) : dest;
        session.edit->moveTrack (t, te::TrackInsertPoint (parent, preceding));
        session.log.add ("You", "Moved " + t->getName() + (side < 0 ? " before " : " after ") + dest->getName() + " (mixer)");
    }

    void layoutStrips()
    {
        const int h = juce::jmax (stripHeightMin(), viewport.getHeight() - 14 - bracketHeight);
        int x = 4;
        brackets.clear();
        for (auto& c : channel)
        {
            const int w = c->preferredWidth();
            c->setBounds (x, 4, w, h);
            const auto cw = c->widthNow();
            if (! brackets.empty() && brackets.back().w == cw && brackets.back().x1 + stripGap >= x) brackets.back().x1 = x + w;
            else brackets.push_back ({ x, x + w, cw });
            x += w + stripGap;
        }
        newTrackBtn.setBounds (x + 6, 8, 96, 34);
        strips.setSize (juce::jmax (viewport.getWidth(), x + 112), h + 8 + bracketHeight);
        bracketPainter.list = &brackets;
        if (bracketPainter.getParentComponent() != &strips) strips.addAndMakeVisible (bracketPainter);
        bracketPainter.setBounds (0, h + 6, strips.getWidth(), bracketHeight);
        bracketPainter.setInterceptsMouseClicks (false, false);
        bracketPainter.repaint();
    }

    void timerCallback() override
    {
        for (auto& c : channel) c->tick();
        if (master) master->tick();
        if (++refreshCounter % 6 == 0)
        {
            for (auto& c : channel) c->refresh();
            if (master) master->refresh();
            if (hashCollapsed() != collapsedHash) rebuild();
        }
    }
};
inline void MixerView::refreshStrips()   { rebuild(); }
