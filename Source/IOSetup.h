#pragma once
// Squelchmoth - Setup > I/O Setup, laid out like Pro Tools: Input, Output, Bus and MIDI pages. Every path
// can be renamed (the names appear in every Input / Output selector), switched between mono and stereo,
// and reset to the default names. Busses are named after their Aux Input track until you rename them.

#include "IORouting.h"

class IOSetupContent : public juce::Component, private juce::Timer
{
public:
    explicit IOSetupContent (Session& s) : session (s)
    {
        const char* tabNames[] = { "Input", "Output", "Bus", "MIDI" };
        for (int i = 0; i < 4; ++i)
        {
            auto& b = tabs[i];
            b.setButtonText (tabNames[i]);
            b.setClickingTogglesState (false);
            b.setColour (juce::TextButton::buttonOnColourId, Theme::violet());
            b.onClick = [this, i] { page = i; rebuild(); };
            addAndMakeVisible (b);
        }
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&rows, false);
        viewport.setScrollBarsShown (true, false);

        for (auto* b : { &btnDefault, &btnAllMono, &btnAllStereo, &btnNewBus, &btnEngine })
            addAndMakeVisible (b);
        btnDefault.onClick   = [this] { if (page == 2) { for (auto bn : IO::allBusses (session)) IO::setBusName (session, bn, {}); } else IO::resetAllNames (session.getEngine()); rebuild(); };
        btnAllMono.onClick   = [this] { IO::relayoutAll (session, page == 0, 1); juce::Timer::callAfterDelay (700, [sp = juce::Component::SafePointer<IOSetupContent> (this)] { if (sp) sp->rebuild(); }); };
        btnAllStereo.onClick = [this] { IO::relayoutAll (session, page == 0, 2); juce::Timer::callAfterDelay (700, [sp = juce::Component::SafePointer<IOSetupContent> (this)] { if (sp) sp->rebuild(); }); };
        btnNewBus.onClick    = [this] { IO::promptNewAux (session, "Bus " + juce::String (IO::allBusses (session).size() + 1), [sp = juce::Component::SafePointer<IOSetupContent> (this)] (te::AudioTrack*) { if (sp) sp->rebuild(); }); };
        btnEngine.onClick    = [this] { if (onShowAudioSettings) onShowAudioSettings(); };
        btnDefault.setTooltip ("Give every path its default Pro Tools name (In 1-2, Out 1-2...)");
        btnAllMono.setTooltip ("Split every pair of interface channels into mono paths (In 1, In 2...)");
        btnAllStereo.setTooltip ("Join the interface channels into stereo pairs (In 1-2, In 3-4...)");

        setSize (780, 520);
        rebuild();
        startTimerHz (1);
    }

    std::function<void()> onShowAudioSettings;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        g.setColour (Theme::dim());
        g.setFont (Theme::font (11.0f, true));
        auto hdr = headerArea;
        const char* cols[4][4] = { { "NAME", "FORMAT", "CHANNELS", "USED BY" }, { "NAME", "FORMAT", "CHANNELS", "USED BY" },
                                   { "NAME", "AUX INPUT TRACK", "", "BUS  /  FED BY" }, { "NAME", "TYPE", "", "ENABLED" } };
        int xs[4] = { 8, 238, 378, 470 };
        for (int i = 0; i < 4; ++i) g.drawText (cols[page][i], hdr.getX() + xs[i], hdr.getY(), 220, hdr.getHeight(), juce::Justification::centredLeft);
        g.setColour (Theme::line());
        g.drawHorizontalLine (hdr.getBottom(), (float) hdr.getX(), (float) hdr.getRight());
        g.setColour (Theme::dim());
        g.setFont (Theme::font (11.0f));
        g.drawFittedText (hintText(), hintArea, juce::Justification::centredLeft, 2);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto tabRow = r.removeFromTop (28);
        for (auto& b : tabs) { b.setBounds (tabRow.removeFromLeft (110)); tabRow.removeFromLeft (4); }
        r.removeFromTop (8);
        auto bottom = r.removeFromBottom (28);
        btnEngine.setBounds (bottom.removeFromRight (200));
        bottom.removeFromRight (6);
        for (auto* b : { &btnDefault, &btnAllMono, &btnAllStereo, &btnNewBus })
            if (b->isVisible()) { b->setBounds (bottom.removeFromLeft (b == &btnDefault ? 130 : 120)); bottom.removeFromLeft (6); }
        r.removeFromBottom (6);
        hintArea = r.removeFromBottom (32);
        r.removeFromBottom (4);
        headerArea = r.removeFromTop (20);
        viewport.setBounds (r);
        layoutRows();
    }

private:
    //==========================================================================
    struct Row : public juce::Component
    {
        juce::Label name, info, used;
        juce::ComboBox format;
        juce::ToggleButton enabled;
        juce::TextButton remove { "Delete" };
        void paint (juce::Graphics& g) override
        {
            g.setColour (Theme::line().withAlpha (0.5f));
            g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 3);
            name.setBounds (r.removeFromLeft (222)); r.removeFromLeft (8);
            if (format.isVisible()) format.setBounds (r.removeFromLeft (128)); else info.setBounds (r.removeFromLeft (128));
            r.removeFromLeft (12);
            if (format.isVisible()) info.setBounds (r.removeFromLeft (80)); else r.removeFromLeft (80);
            r.removeFromLeft (12);
            if (remove.isVisible()) remove.setBounds (r.removeFromRight (64));
            if (enabled.isVisible()) enabled.setBounds (r.removeFromLeft (80));
            else used.setBounds (r);
        }
    };

    Session& session;
    int page = 0;
    juce::TextButton tabs[4];
    juce::Viewport viewport;
    juce::Component rows;
    std::vector<std::unique_ptr<Row>> rowComps;
    juce::TextButton btnDefault { "Default names" }, btnAllMono { "All mono" }, btnAllStereo { "All stereo" }, btnNewBus { "New Bus..." }, btnEngine { "Playback Engine (interface)..." };
    juce::Rectangle<int> headerArea, hintArea;
    juce::String signature;

    juce::String hintText() const
    {
        switch (page)
        {
            case 0:  return "Interface inputs. Double-click a name to rename it - the name appears in every track's Input selector. Mono paths record one channel (a mic); stereo paths record a pair (keys, a stereo mix).";
            case 1:  return "Interface outputs. The Main output (the first path) is where the Master Fader plays. Rename paths to match your speakers / headphone mixes.";
            case 2:  return "Busses carry sends and track outputs to an Aux Input track. A bus is named after its Aux track until you rename it here.";
            default: return "MIDI controllers and keyboards. Disabled devices are ignored by every track. 'All' merges every enabled MIDI input.";
        }
    }

    static void styleLabel (juce::Label& l, bool editable)
    {
        l.setFont (Theme::font (13.0f, editable));
        l.setColour (juce::Label::textColourId, editable ? Theme::text() : Theme::dim());
        l.setEditable (false, editable);
        if (editable) { l.setColour (juce::Label::backgroundColourId, Theme::bg()); l.setTooltip ("Double-click to rename"); }
    }

    juce::String currentSignature()
    {
        juce::String sig; sig << page << "|";
        auto& dm = session.getEngine().getDeviceManager();
        if (page == 0) for (auto d : dm.getWaveInputDevices())  if (d && ! d->isTrackDevice()) sig << d->getDeviceID() << (d->isEnabled() ? "1" : "0");
        if (page == 1) for (auto d : dm.getWaveOutputDevices()) if (d) sig << d->getDeviceID() << (d->isEnabled() ? "1" : "0");
        if (page == 2) for (auto b : IO::allBusses (session)) sig << b << IO::busName (session, b);
        if (page == 3) { sig << dm.getNumMidiInDevices() << dm.getNumMidiOutDevices(); }
        for (auto t : session.getTracks()) sig << t->getName() << IO::inputLabel (session, *t) << IO::outputLabel (session, *t);
        return sig;
    }

    void timerCallback() override
    {
        for (auto& r : rowComps) if (r->name.isBeingEdited()) return;
        if (currentSignature() != signature) rebuild();
    }

    juce::String tracksUsingInput (te::InputDevice& d)
    {
        juce::StringArray names;
        if (session.edit != nullptr)
            if (auto inst = IO::instanceFor (*session.edit, d))
                for (auto id : inst->getTargets())
                    if (auto t = te::findTrackForID (*session.edit, id)) names.add (t->getName());
        return names.isEmpty() ? juce::String ("-") : names.joinIntoString (", ");
    }

    juce::String tracksUsingOutput (te::WaveOutputDevice& d)
    {
        juce::StringArray names;
        bool isMain = session.getEngine().getDeviceManager().getDefaultWaveOutDevice() == &d;
        if (isMain) names.add ("Master Fader");
        for (auto t : session.getTracks())
        {
            auto& o = t->getOutput();
            if (o.getDestinationTrack() != nullptr || o.outputsToNone()) continue;
            if (o.getOutputDeviceID() == d.getDeviceID()) names.add (t->getName());
        }
        return names.isEmpty() ? juce::String ("-") : names.joinIntoString (", ");
    }

    void rebuild()
    {
        for (int i = 0; i < 4; ++i) tabs[i].setToggleState (page == i, juce::dontSendNotification);
        btnAllMono.setVisible (page <= 1); btnAllStereo.setVisible (page <= 1);
        btnNewBus.setVisible (page == 2); btnDefault.setVisible (page <= 2);
        rowComps.clear();
        rows.removeAllChildren();
        auto& eng = session.getEngine();
        auto& dm = eng.getDeviceManager();

        auto addRow = [this] () -> Row&
        {
            auto r = std::make_unique<Row>();
            for (auto* c : { (juce::Component*) &r->name, (juce::Component*) &r->info, (juce::Component*) &r->used }) r->addAndMakeVisible (c);
            r->addChildComponent (r->format);
            r->addChildComponent (r->enabled);
            r->addChildComponent (r->remove);
            r->used.setFont (Theme::font (12.0f));
            r->used.setColour (juce::Label::textColourId, Theme::dim());
            r->info.setFont (Theme::font (12.0f));
            r->info.setColour (juce::Label::textColourId, Theme::dim());
            rows.addAndMakeVisible (*r);
            rowComps.push_back (std::move (r));
            return *rowComps.back();
        };

        if (page == 0 || page == 1)
        {
            bool input = page == 0;
            std::vector<std::pair<int, int>> ranges;
            std::vector<te::IODevice*> devs;
            if (input) { for (auto d : dm.getWaveInputDevices()) if (d && ! d->isTrackDevice()) { ranges.push_back (IO::rangeOf (d->getChannels())); devs.push_back (d); } }
            else       { for (auto d : dm.getWaveOutputDevices()) if (d) { ranges.push_back (IO::rangeOf (d->getChannels())); devs.push_back (d); } }
            for (size_t i = 0; i < devs.size(); ++i)
            {
                auto [first, count] = ranges[i];
                auto& row = addRow();
                styleLabel (row.name, true);
                row.name.setText (IO::pathName (input, first, count), juce::dontSendNotification);
                row.name.onTextChange = [this, &row, input, first = first, count = count]
                {
                    auto n = row.name.getText().trim();
                    IO::setPathName (session.getEngine(), input, first, count, n.isEmpty() ? IO::defaultPathName (input, first, count) : n);
                    session.log.add ("You", juce::String (input ? "Input" : "Output") + " path " + IO::rangeText (first, count) + " renamed to " + IO::pathName (input, first, count));
                };
                row.format.setVisible (true);
                row.format.addItem ("Mono", 1);
                row.format.addItem ("Stereo", 2);
                row.format.setSelectedId (count >= 2 ? 2 : 1, juce::dontSendNotification);
                row.format.setEnabled (input || IO::numInterfaceChannels (eng, false) > 1);
                auto* dev = devs[i];
                row.format.onChange = [this, &row, input, first = first, dev]
                {
                    int want = row.format.getSelectedId();
                    if (input)
                    {
                        // re-lay out through the track input machinery so tracks follow
                        juce::ignoreUnused (dev);
                        IO::setInputPathFormat (session, first, want, [sp = juce::Component::SafePointer<IOSetupContent> (this)] { if (sp) sp->rebuild(); });
                    }
                    else if (auto w = dynamic_cast<te::WaveOutputDevice*> (dev))
                    {
                        session.getEngine().getDeviceManager().setDeviceNumChannels (*w, (uint32_t) want);
                        juce::Timer::callAfterDelay (500, [this, sp = juce::Component::SafePointer<IOSetupContent> (this)] { if (sp) { IO::applyNames (session.getEngine()); sp->rebuild(); } });
                    }
                };
                row.info.setText ((input ? "In " : "Out ") + IO::rangeText (first, count) + (dev->isEnabled() ? "" : "  (off)"), juce::dontSendNotification);
                if (input) { if (auto w = dynamic_cast<te::WaveInputDevice*> (dev)) row.used.setText (tracksUsingInput (*w), juce::dontSendNotification); }
                else if (auto w = dynamic_cast<te::WaveOutputDevice*> (dev)) row.used.setText (tracksUsingOutput (*w), juce::dontSendNotification);
            }
            if (devs.empty())
            {
                auto& row = addRow();
                styleLabel (row.name, false);
                row.name.setText ("No audio interface is open", juce::dontSendNotification);
                row.used.setText ("Click Playback Engine to choose your interface", juce::dontSendNotification);
            }
        }
        else if (page == 2)
        {
            for (auto b : IO::allBusses (session))
            {
                auto& row = addRow();
                styleLabel (row.name, true);
                row.name.setText (IO::busName (session, b), juce::dontSendNotification);
                row.name.onTextChange = [this, &row, b] { IO::setBusName (session, b, row.name.getText()); session.log.add ("You", "Bus " + juce::String (b + 1) + " renamed to " + IO::busName (session, b)); };
                auto aux = session.findBusTrack (b);
                row.info.setText (aux != nullptr ? aux->getName() : juce::String ("(none)"), juce::dontSendNotification);
                int sends = 0; juce::StringArray outs;
                for (auto t : session.getTracks())
                {
                    for (auto snd : Session::getSends (*t)) if (snd->getBusNumber() == b) ++sends;
                    if (aux != nullptr && t->getOutput().outputsToDestTrack (*aux)) outs.add (t->getName());
                }
                juce::String fed;
                fed << "Bus " << (b + 1) << "     " << sends << " send" << (sends == 1 ? "" : "s");
                if (! outs.isEmpty()) fed << ", outputs: " << outs.joinIntoString (", ");
                row.used.setText (fed, juce::dontSendNotification);
                row.remove.setVisible (aux != nullptr);
                row.remove.setTooltip ("Delete this bus and its Aux Input track (sends to it are removed)");
                te::Track::Ptr auxKeep (aux);
                row.remove.onClick = [this, auxKeep, b]
                {
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete bus", "Delete " + IO::busName (session, b) + " and its Aux Input track? Sends to it are removed.", "Delete", "Cancel", nullptr,
                        juce::ModalCallbackFunction::create ([this, auxKeep, b] (int ok)
                        {
                            if (ok != 1 || session.edit == nullptr) return;
                            for (auto t : session.getTracks()) for (auto snd : Session::getSends (*t)) if (snd->getBusNumber() == b) snd->deleteFromParent();
                            for (auto t : session.getTracks()) if (auxKeep != nullptr && t->getOutput().outputsToDestTrack (*dynamic_cast<te::AudioTrack*> (auxKeep.get()))) t->getOutput().setOutputToDefaultDevice (false);
                            if (auxKeep != nullptr) session.edit->deleteTrack (auxKeep.get());
                            IO::setBusName (session, b, {});
                            rebuild();
                        }));
                };
            }
            if (rowComps.empty())
            {
                auto& row = addRow();
                styleLabel (row.name, false);
                row.name.setText ("No busses yet", juce::dontSendNotification);
                row.used.setText ("Click New Bus..., or choose 'new track...' in any Output or Send selector", juce::dontSendNotification);
            }
        }
        else
        {
            for (auto& md : dm.getMidiInDevices())
            {
                if (md == nullptr) continue;
                auto& row = addRow();
                styleLabel (row.name, true);
                row.name.setText (IO::midiName (*md), juce::dontSendNotification);
                std::weak_ptr<te::MidiInputDevice> weak = md;
                row.name.onTextChange = [&row, weak] { if (auto d = weak.lock()) d->setAlias (row.name.getText()); };
                row.info.setText (md->getDeviceType() == te::InputDevice::virtualMidiDevice ? "MIDI in (virtual)" : "MIDI in", juce::dontSendNotification);
                row.enabled.setVisible (true);
                row.enabled.setButtonText ("on");
                row.enabled.setToggleState (md->isEnabled(), juce::dontSendNotification);
                row.enabled.onClick = [&row, weak] { if (auto d = weak.lock()) d->setEnabled (row.enabled.getToggleState()); };
            }
            for (int i = 0; i < dm.getNumMidiOutDevices(); ++i)
                if (auto mo = dm.getMidiOutDevice (i))
                {
                    auto& row = addRow();
                    styleLabel (row.name, true);
                    row.name.setText (mo->getAliasOrName(), juce::dontSendNotification);
                    row.name.onTextChange = [&row, mo] { mo->setAlias (row.name.getText()); };
                    row.info.setText ("MIDI out", juce::dontSendNotification);
                    row.enabled.setVisible (true);
                    row.enabled.setButtonText ("on");
                    row.enabled.setToggleState (mo->isEnabled(), juce::dontSendNotification);
                    row.enabled.onClick = [&row, mo] { mo->setEnabled (row.enabled.getToggleState()); };
                }
            if (rowComps.empty())
            {
                auto& row = addRow();
                styleLabel (row.name, false);
                row.name.setText ("No MIDI devices found", juce::dontSendNotification);
                row.used.setText ("Connect a keyboard or controller - it appears here automatically", juce::dontSendNotification);
            }
        }
        signature = currentSignature();
        resized();
        repaint();
    }

    void layoutRows()
    {
        int y = 0;
        auto w = viewport.getMaximumVisibleWidth();
        for (auto& r : rowComps) { r->setBounds (0, y, w, 32); y += 32; }
        rows.setSize (w, juce::jmax (y, viewport.getHeight()));
    }
};
