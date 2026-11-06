#pragma once
// Squelchmoth - transport bar: logo, transport buttons, big counter, tempo/meter, loop, click, CPU.

#include "Behaviours.h"
#include "TransportController.h"
#include "Rack.h"

class LogoComponent : public juce::Component
{
public:
    LogoComponent()
    {
        if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::logo_svg, BinaryData::logo_svgSize)))
            drawable = juce::Drawable::createFromSVG (*svg);
    }
    void paint (juce::Graphics& g) override
    {
        if (drawable != nullptr)
            drawable->drawWithin (g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    }
    std::unique_ptr<juce::Drawable> drawable;
};

class TransportBar : public juce::Component,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    TransportBar (Session& s, TransportController& tc) : session (s), transport (tc)
    {
        Theme::broadcaster().addChangeListener (this);
        addAndMakeVisible (logo);
        title.setText ("SQUELCHMOTH", juce::dontSendNotification);
        title.setFont (Theme::font (19.0f, true));
        title.setColour (juce::Label::textColourId, Theme::acid());
        addAndMakeVisible (title);

        for (auto* b : { &rtz, &rew, &stop, &play, &ffwd, &toEnd, &rec, &loop, &click })
            addAndMakeVisible (b);
        for (auto* b : { &preBtn, &postBtn, &modeBtn })
            addAndMakeVisible (b);

        rtz.setTooltip ("Return to zero (Return)");  rew.setTooltip ("Rewind a bar");  stop.setTooltip ("Stop (Space)");
        play.setTooltip ("Play / stop (Space) - plays from the Edit selection");  ffwd.setTooltip ("Forward a bar");  toEnd.setTooltip ("Go to end");
        rec.setTooltip ("Record (Cmd+Space). Record mode: see the MODE button");  loop.setTooltip ("Loop Playback (Cmd+Shift+L)");  click.setTooltip ("Click (C)");
        preBtn.setTooltip ("Pre-roll on/off (length in Transport window or Preferences)");  postBtn.setTooltip ("Post-roll on/off");
        modeBtn.setTooltip ("Record mode: Normal / Loop Record / QuickPunch. Count-off and Wait for Note are in the Transport window (Window menu).");

        rtz.onClick  = [this] { transport.returnToZero(); };
        rew.onClick  = [this] { transport.skipBars (-1); };
        ffwd.onClick = [this] { transport.skipBars (1); };
        toEnd.onClick = [this] { transport.goToEnd(); };
        stop.onClick = [this] { transport.stop(); };
        play.onClick = [this] { transport.togglePlay(); };
        rec.onClick  = [this] { transport.toggleRecord(); };
        loop.setClickingTogglesState (true);
        loop.onClick = [this] { transport.setLoopPlayback (loop.getToggleState()); };
        click.setClickingTogglesState (true);
        click.onClick = [this] { transport.setClick (click.getToggleState()); };
        patSong.setClickingTogglesState (true);
        patSong.setTooltip ("PAT: loop the rack's current pattern (FL pattern mode).  SONG: play the timeline. Space follows it.");
        patSong.onClick = [this]
        {
            if (auto e = edit())
            {
                const bool pm = patSong.getToggleState();
                Rack::setPatternMode (*e, pm);
                if (pm && ! e->getTransport().isPlaying()) { e->getTransport().setPosition (te::TimePosition()); e->getTransport().play (false); }
            }
        };
        addAndMakeVisible (patSong);
        preBtn.setClickingTogglesState (true);  preBtn.onClick = [this] { transport.setPreRoll (preBtn.getToggleState()); };
        postBtn.setClickingTogglesState (true); postBtn.onClick = [this] { transport.setPostRoll (postBtn.getToggleState()); };
        modeBtn.onClick = [this]
        {
            juce::PopupMenu m;
            m.addItem (1, "Normal (nondestructive) record", true, ! Prefs::loopRecord() && ! Prefs::quickPunch());
            m.addItem (2, "Loop Record (each pass = new take / playlist)", true, Prefs::loopRecord());
            m.addItem (3, "QuickPunch (record only inside the Edit selection)", true, Prefs::quickPunch());
            m.addSeparator();
            m.addItem (4, "Wait for Note", true, Prefs::waitForNote());
            m.addItem (5, "MIDI Merge", true, Prefs::midiMerge());
            m.addSeparator();
            m.addItem (6, "Count-off: off", true, Prefs::countOffBars() == 0);
            m.addItem (7, "Count-off: 1 bar", true, Prefs::countOffBars() == 1);
            m.addItem (8, "Count-off: 2 bars", true, Prefs::countOffBars() == 2);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (modeBtn), [this] (int r)
            {
                if (r == 1) { transport.setLoopRecord (false); transport.setQuickPunch (false); }
                else if (r == 2) transport.setLoopRecord (true);
                else if (r == 3) transport.setQuickPunch (true);
                else if (r == 4) transport.setWaitForNote (! Prefs::waitForNote());
                else if (r == 5) transport.setMidiMerge (! Prefs::midiMerge());
                else if (r >= 6) transport.setCountOff (r - 6);
            });
        };
        preBtn.setColour (juce::TextButton::buttonOnColourId, Theme::violet());
        postBtn.setColour (juce::TextButton::buttonOnColourId, Theme::violet());
        modeBtn.setColour (juce::TextButton::buttonOnColourId, Theme::amber().darker (0.4f));

        rec.setColour (juce::TextButton::buttonOnColourId, Theme::red());
        play.setColour (juce::TextButton::buttonOnColourId, Theme::mint().darker (0.4f));
        loop.setColour (juce::TextButton::buttonOnColourId, Theme::mint().darker (0.4f));
        click.setColour (juce::TextButton::buttonOnColourId, Theme::violet());

        counter.setJustificationType (juce::Justification::centred);
        counter.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 26.0f, juce::Font::bold)));
        counter.setColour (juce::Label::textColourId, Theme::acid());
        counter.setColour (juce::Label::backgroundColourId, Theme::bg());
        addAndMakeVisible (counter);

        timecode.setJustificationType (juce::Justification::centred);
        timecode.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
        timecode.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (timecode);

        tempo.setEditable (true);
        tempo.setJustificationType (juce::Justification::centred);
        tempo.setColour (juce::Label::backgroundColourId, Theme::panel2());
        tempo.setFont (Theme::font (15.0f, true));
        tempo.onTextChange = [this]
        {
            if (auto e = edit())
            {
                auto txt = tempo.getText().upToFirstOccurrenceOf (" ", false, false);
                auto bpm = txt.getDoubleValue();
                if (bpm >= 20 && bpm <= 400) e->tempoSequence.getTempoAt (e->getTransport().getPosition()).setBpm (bpm);   // the tempo in effect at the playhead
            }
        };
        addAndMakeVisible (tempo);

        meter.setEditable (true);
        meter.setJustificationType (juce::Justification::centred);
        meter.setColour (juce::Label::backgroundColourId, Theme::panel2());
        meter.setFont (Theme::font (15.0f, true));
        meter.onTextChange = [this]
        {
            if (auto e = edit())
            {
                auto parts = juce::StringArray::fromTokens (meter.getText(), "/", "");
                if (parts.size() == 2 && parts[0].getIntValue() > 0 && parts[1].getIntValue() > 0)
                {
                    auto& sig = e->tempoSequence.getTimeSigAt (e->getTransport().getPosition());
                    sig.numerator = parts[0].getIntValue();
                    sig.denominator = parts[1].getIntValue();
                }
            }
        };
        addAndMakeVisible (meter);

        cpu.setFont (Theme::font (11.0f));
        cpu.setColour (juce::Label::textColourId, Theme::dim());
        cpu.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (cpu);
        cpu.setTooltip ("Audio engine load - click for System Usage (heaviest plug-ins, dropouts, memory)");
        cpu.addMouseListener (this, false);

        for (auto* b : { &editTab, &mixTab, &beatTab, &claudeTab })
        {
            addAndMakeVisible (b);
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, Theme::violet());
        }
        editTab.onClick = [this] { if (onShowView) onShowView ("edit"); };
        mixTab.onClick = [this] { if (onShowView) onShowView ("mix"); };
        beatTab.onClick = [this] { if (onShowView) onShowView ("beat"); };
        beatTab.setTooltip ("Beat page (Cmd+Shift+B): browser, Channel Rack, instrument, playlist - the FL workspace");
        claudeTab.onClick = [this] { if (onShowView) onShowView ("claude"); };

        startTimerHz (20);
        applyTheme();
    }

    ~TransportBar() override    { Theme::broadcaster().removeChangeListener (this); }

    std::function<void (const juce::String&)> onShowView;
    std::function<void()> onCpuClicked;   // opens Window > System Usage

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &cpu && onCpuClicked) onCpuClicked();
    }

    void applyTheme()
    {
        title.setColour (juce::Label::textColourId, Theme::acid());
        counter.setColour (juce::Label::textColourId, Theme::acid());
        counter.setColour (juce::Label::backgroundColourId, Theme::bg());
        timecode.setColour (juce::Label::textColourId, Theme::dim());
        tempo.setColour (juce::Label::backgroundColourId, Theme::panel2());
        meter.setColour (juce::Label::backgroundColourId, Theme::panel2());
        cpu.setColour (juce::Label::textColourId, Theme::dim());
        rec.setColour (juce::TextButton::buttonOnColourId, Theme::red());
        play.setColour (juce::TextButton::buttonOnColourId, Theme::mint().darker (0.4f));
        loop.setColour (juce::TextButton::buttonOnColourId, Theme::mint().darker (0.4f));
        click.setColour (juce::TextButton::buttonOnColourId, Theme::violet());
        for (auto* b : { &editTab, &mixTab, &beatTab, &claudeTab }) b->setColour (juce::TextButton::buttonOnColourId, Theme::violet());
        repaint();
    }

    void setTabVisible (const juce::String& v, bool on)
    {
        if (v == "mix") mixTab.setVisible (on); else if (v == "beat") beatTab.setVisible (on); else if (v == "claude") claudeTab.setVisible (on);
        resized();
    }

    void setCurrentView (const juce::String& v)
    {
        editTab.setToggleState (v == "edit", juce::dontSendNotification);
        mixTab.setToggleState (v == "mix", juce::dontSendNotification);
        beatTab.setToggleState (v == "beat", juce::dontSendNotification);
        claudeTab.setToggleState (v == "claude", juce::dontSendNotification);
    }

    void setClaudeStatus (const juce::String& s, bool live)
    {
        claudeTab.setButtonText (live ? "CLAUDE  *" : "CLAUDE");
        claudeTab.setColour (juce::TextButton::textColourOffId, live ? Theme::mint() : Theme::text());
        claudeTab.setTooltip (s);
    }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (Theme::panel2(), 0, 0, Theme::panel(), 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (Theme::line());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        logo.setBounds (r.removeFromLeft (48));
        r.removeFromLeft (4);
        title.setBounds (r.removeFromLeft (150));
        r.removeFromLeft (8);

        juce::Array<juce::TextButton*> shown;
        for (auto* b : { &editTab, &mixTab, &beatTab, &claudeTab }) if (b->isVisible()) shown.add (b);
        auto tabs = r.removeFromRight (shown.size() * 92);
        auto tw = tabs.getWidth() / juce::jmax (1, shown.size());
        for (int i = 0; i < shown.size(); ++i) shown[i]->setBounds ((i + 1 == shown.size() ? tabs : tabs.removeFromLeft (tw)).reduced (3, 8));
        cpu.setBounds (r.removeFromRight (80));

        for (auto* b : { &rtz, &rew, &stop, &play, &ffwd, &toEnd, &rec })
            b->setBounds (r.removeFromLeft (40).reduced (2, 8));
        loop.setBounds (r.removeFromLeft (54).reduced (2, 8));
        click.setBounds (r.removeFromLeft (54).reduced (2, 8));
        patSong.setBounds (r.removeFromLeft (54).reduced (2, 8));
        r.removeFromLeft (6);
        auto modes = r.removeFromLeft (120);
        auto top = modes.removeFromTop (modes.getHeight() / 2);
        preBtn.setBounds (top.removeFromLeft (60).reduced (2, 3));
        postBtn.setBounds (top.reduced (2, 3));
        modeBtn.setBounds (modes.reduced (2, 3));
        r.removeFromLeft (8);

        auto cnt = r.removeFromLeft (200);
        counter.setBounds (cnt.removeFromTop (cnt.getHeight() * 2 / 3));
        timecode.setBounds (cnt);
        r.removeFromLeft (8);
        tempo.setBounds (r.removeFromLeft (96).reduced (2, 10));
        meter.setBounds (r.removeFromLeft (56).reduced (2, 10));
    }

private:
    Session& session;
    TransportController& transport;
    LogoComponent logo;

    void changeListenerCallback (juce::ChangeBroadcaster*) override   { applyTheme(); }
    juce::Label title, counter, timecode, tempo, meter, cpu;

    struct IconButton : public juce::TextButton
    {
        IconButton (const juce::String& n) : juce::TextButton (n) {}
        std::function<void()> onMouseDownLambda;

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            getLookAndFeel().drawButtonBackground (g, *this, findColour (getToggleState() ? buttonOnColourId : buttonColourId), over, down);
            auto r = getLocalBounds().toFloat().reduced (getWidth() * 0.3f, getHeight() * 0.28f);
            auto n = getButtonText();
            g.setColour (getToggleState() ? juce::Colours::white : Theme::text());
            juce::Path p;
            auto cy = r.getCentreY(), h = r.getHeight();

            if (n == ">")       p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), cy);
            else if (n == "[]") p.addRectangle (r.withSizeKeepingCentre (h * 0.85f, h * 0.85f));
            else if (n == "O")  { g.setColour (getToggleState() ? juce::Colours::white : Theme::red()); p.addEllipse (r.withSizeKeepingCentre (h, h)); }
            else if (n == "|<")
            {
                p.addRectangle (r.getX(), r.getY(), 2.5f, h);
                p.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX() + 3.0f, cy);
            }
            else if (n == "<<")
            {
                auto w = r.getWidth() / 2;
                p.addTriangle (r.getX() + w, r.getY(), r.getX() + w, r.getBottom(), r.getX(), cy);
                p.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX() + w, cy);
            }
            else if (n == ">>")
            {
                auto w = r.getWidth() / 2;
                p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getX() + w, cy);
                p.addTriangle (r.getX() + w, r.getY(), r.getX() + w, r.getBottom(), r.getRight(), cy);
            }
            else if (n == ">|")
            {
                p.addRectangle (r.getRight() - 2.5f, r.getY(), 2.5f, h);
                p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight() - 3.0f, cy);
            }
            else
            {
                g.setFont (Theme::font (11.0f, true));
                g.drawText (n, getLocalBounds(), juce::Justification::centred);
                return;
            }
            g.fillPath (p);
        }
    };

    IconButton rtz { "|<" }, rew { "<<" }, stop { "[]" }, play { ">" }, ffwd { ">>" }, toEnd { ">|" }, rec { "O" }, loop { "LOOP" }, click { "CLICK" }, patSong { "PAT" };
    juce::TextButton preBtn { "PRE" }, postBtn { "POST" }, modeBtn { "NORMAL" };
    juce::TextButton editTab { "EDIT" }, mixTab { "MIX" }, beatTab { "BEAT" }, claudeTab { "CLAUDE" };

    te::Edit* edit()    { return session.edit.get(); }

    void timerCallback() override
    {
        auto e = edit();
        if (e == nullptr) return;
        auto& t = e->getTransport();
        { const bool pm = Rack::patternMode (*e); if (patSong.getToggleState() != pm) patSong.setToggleState (pm, juce::dontSendNotification); patSong.setButtonText (pm ? "PAT" : "SONG"); }
        auto pos = t.getPosition();
        // Pro Tools: the transport counters follow the Main / Sub counter formats (View > Main Counter)
        const auto mainK = (TimeFmt::Kind) juce::jlimit (0, 3, Prefs::getInt ("ed_mainCounterFmt", 0));
        const auto subK  = (TimeFmt::Kind) juce::jlimit (0, 3, Prefs::getInt ("ed_subCounterFmt", 1));
        counter.setText (TimeFmt::format (*e, pos, mainK), juce::dontSendNotification);
        timecode.setText (TimeFmt::format (*e, pos, subK), juce::dontSendNotification);

        // tempo and meter in effect at the playhead (follows the tempo / meter rulers)
        if (! tempo.isBeingEdited())
            tempo.setText (juce::String (e->tempoSequence.getBpmAt (pos), 1) + " BPM", juce::dontSendNotification);
        if (! meter.isBeingEdited())
        {
            auto& sig = e->tempoSequence.getTimeSigAt (pos);
            meter.setText (juce::String (sig.numerator.get()) + "/" + juce::String (sig.denominator.get()), juce::dontSendNotification);
        }

        play.setToggleState (t.isPlaying(), juce::dontSendNotification);
        rec.setToggleState (t.isRecording() || transport.isWaitingForNote(), juce::dontSendNotification);
        loop.setToggleState (t.looping.get(), juce::dontSendNotification);
        click.setToggleState (e->clickTrackEnabled.get(), juce::dontSendNotification);
        preBtn.setToggleState (Prefs::preRollEnabled(), juce::dontSendNotification);
        postBtn.setToggleState (Prefs::postRollEnabled(), juce::dontSendNotification);
        juce::String mode = Prefs::loopRecord() ? "LOOP REC" : Prefs::quickPunch() ? "QUICKPUNCH" : "NORMAL";
        if (Prefs::waitForNote()) mode << " W";
        if (Prefs::countOffBars() > 0) mode << " C" << Prefs::countOffBars();
        modeBtn.setButtonText (mode);
        modeBtn.setToggleState (Prefs::loopRecord() || Prefs::quickPunch(), juce::dontSendNotification);
        counter.setColour (juce::Label::textColourId, t.isRecording() ? Theme::red() : Theme::acid());

        auto load = session.getEngine().getDeviceManager().getCpuUsage();
        cpu.setText ("CPU " + juce::String (juce::roundToInt (load * 100)) + "%", juce::dontSendNotification);
        cpu.setColour (juce::Label::textColourId, load > Prefs::cpuUsageLimitPercent() / 100.0f * 0.9f ? Theme::red() : load > 0.6f ? Theme::amber() : Theme::dim());
    }
};
