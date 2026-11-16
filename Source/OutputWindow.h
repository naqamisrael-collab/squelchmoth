#pragma once
// Squelchmoth - the Pro Tools Output window: one track's fader, meter, pan, mute, solo, record / input monitor,
// automation mode and input / output paths in a small floating window. Open it with the fader button in the
// Edit window's I/O view; the track menu at the top switches it to any other track (Pro Tools' target behaviour).

#include "MixerView.h"

class OutputWindowContent : public juce::Component, private juce::Timer
{
public:
    OutputWindowContent (Session& s, TimelineState* st) : session (s), state (st)
    {
        addAndMakeVisible (trackMenu);
        trackMenu.setTooltip ("Which track this Output window controls");
        trackMenu.onChange = [this]
        {
            auto rows = allRows();
            auto i = trackMenu.getSelectedId() - 1;
            if (i >= 0 && i < rows.size()) showTrack (rows[i]);
        };
        setSize (168, 760);
        startTimerHz (2);
    }

    void showTrack (te::Track* t)
    {
        if (t == nullptr) return;
        current = t;
        strip = std::make_unique<ChannelStrip> (session, t->isMasterTrack() ? nullptr : t, state);
        strip->outputWindowStyle = true;
        strip->forcedWidth = Console::Width::standard;
        strip->buildSlots();
        addAndMakeVisible (*strip);
        fillMenu();
        resized();
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
            w->setName ("Output: " + (t->isMasterTrack() ? TrackOps::masterName (session) : t->getName()));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        trackMenu.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        if (strip) strip->setBounds (r);
    }

    void paint (juce::Graphics& g) override   { g.fillAll (Theme::mixBg()); }

private:
    Session& session;
    TimelineState* state;
    te::SafeSelectable<te::Track> current;
    std::unique_ptr<ChannelStrip> strip;
    juce::ComboBox trackMenu;

    juce::Array<te::Track*> allRows()
    {
        juce::Array<te::Track*> rows;
        for (auto t : session.getRows()) rows.add (t);
        if (session.edit != nullptr && TrackOps::hasMasterFader (session)) if (auto m = session.edit->getMasterTrack()) rows.add (m);
        return rows;
    }

    void fillMenu()
    {
        trackMenu.clear (juce::dontSendNotification);
        auto rows = allRows();
        for (int i = 0; i < rows.size(); ++i)
            trackMenu.addItem (rows[i]->isMasterTrack() ? TrackOps::masterName (session) : rows[i]->getName(), i + 1);
        trackMenu.setSelectedId (rows.indexOf (current.get()) + 1, juce::dontSendNotification);
    }

    void timerCallback() override
    {
        if (current.get() == nullptr || ! allRows().contains (current.get()))
        {
            // the track was deleted or the session changed: close
            if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->closeButtonPressed();
            return;
        }
        if (trackMenu.getNumItems() != allRows().size()) fillMenu();
    }
};
