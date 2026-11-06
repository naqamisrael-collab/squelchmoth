#pragma once
// Squelchmoth - volume, pan and track-view helpers shared by the Edit window (track headers, the I/O view and the
// automation lanes), the pop-up fader / pan sliders and the Output windows.
//
// Pro Tools' Edit window gives every track: a volume indicator and pan indicator(s) in the I/O view - click for a
// pop-up fader or pan slider -, an Output Window button that opens the track's own fader window, and a Track View
// selector that turns the lane into a volume, pan, send-level or any plug-in parameter automation graph.

#include "Session.h"
#include "TrackOps.h"
#include "Theme.h"

namespace MixCtl
{
    //==========================================================================
    inline te::VolumeAndPanPlugin* volumePlugin (Session& s, te::Track& t)
    {
        if (t.isMasterTrack()) return s.edit != nullptr ? s.edit->getMasterVolumePlugin().get() : nullptr;
        if (auto a = dynamic_cast<te::AudioTrack*> (&t)) return a->getVolumePlugin();
        if (auto f = dynamic_cast<te::FolderTrack*> (&t)) return f->getVolumePlugin();
        return nullptr;
    }

    inline te::AutomatableParameter* volumeParam (Session& s, te::Track& t)
    {
        if (auto f = dynamic_cast<te::FolderTrack*> (&t)) if (auto vca = f->getVCAPlugin()) return vca->volParam.get();
        if (auto vp = volumePlugin (s, t)) return vp->volParam.get();
        return nullptr;
    }

    inline float getVolumeDb (Session& s, te::Track& t)
    {
        if (auto f = dynamic_cast<te::FolderTrack*> (&t)) if (auto vca = f->getVCAPlugin()) return vca->getVolumeDb();
        if (auto vp = volumePlugin (s, t)) return vp->getVolumeDb();
        return 0.0f;
    }

    inline void setVolumeDb (Session& s, te::Track& t, float db)
    {
        db = juce::jlimit (-100.0f, 6.0f, db);
        if (auto a = dynamic_cast<te::AudioTrack*> (&t)) { s.setTrackVolume (*a, db); return; }
        if (auto f = dynamic_cast<te::FolderTrack*> (&t)) if (auto vca = f->getVCAPlugin()) { vca->setVolumeDb (db); return; }
        if (auto vp = volumePlugin (s, t)) vp->setVolumeDb (db);
    }

    inline juce::String dbText (float db)   { return db <= -99.0f ? juce::String ("-inf") : (db > 0.0f ? "+" : "") + juce::String (db, 1); }

    inline juce::String panText (float v)
    {
        if (std::abs (v) < 0.005f) return "<0>";
        return v < 0 ? "<" + juce::String (juce::roundToInt (-v * 100.0f)) : juce::String (juce::roundToInt (v * 100.0f)) + ">";
    }

    inline SqStereoPanPlugin* stereoPan (te::Track& t)
    {
        if (auto a = dynamic_cast<te::AudioTrack*> (&t)) return Session::getStereoPan (*a);
        return nullptr;
    }

    inline float getPan (Session& s, te::Track& t)               { if (auto vp = volumePlugin (s, t)) return vp->getPan(); return 0.0f; }
    inline void setPan (Session& s, te::Track& t, float v)       { if (auto vp = volumePlugin (s, t)) vp->setPan (juce::jlimit (-1.0f, 1.0f, v)); }

    inline juce::String panSummary (Session& s, te::Track& t)
    {
        if (auto sp = stereoPan (t)) return panText (sp->panL->getCurrentValue()) + " " + panText (sp->panR->getCurrentValue());
        return panText (getPan (s, t));
    }

    //==========================================================================
    // Track View selector (Pro Tools): which automation graph the lane shows
    struct ViewChoice { juce::String id, label; te::AutomatableParameter* param = nullptr; };

    inline juce::String currentView (te::Track& t)               { return t.state.getProperty ("sqmView", "waveform").toString(); }
    inline void setView (te::Track& t, const juce::String& v)    { t.state.setProperty ("sqmView", v, nullptr); }

    inline std::vector<ViewChoice> viewChoices (Session& s, te::Track& t)
    {
        std::vector<ViewChoice> out;
        bool hasClips = dynamic_cast<te::AudioTrack*> (&t) != nullptr;
        out.push_back ({ "waveform", hasClips ? "waveform" : "blocks", nullptr });
        if (auto vp = volumeParam (s, t)) out.push_back ({ "volume", "volume", vp });
        if (auto sp = stereoPan (t))
        {
            out.push_back ({ "panL", "pan left", sp->panL.get() });
            out.push_back ({ "panR", "pan right", sp->panR.get() });
        }
        else if (auto vp = volumePlugin (s, t)) out.push_back ({ "pan", "pan", vp->panParam.get() });
        if (auto a = dynamic_cast<te::AudioTrack*> (&t))
        {
            auto sends = Session::getSends (*a);
            for (int i = 0; i < sends.size(); ++i)
                for (auto p : sends[i]->getAutomatableParameters())
                    out.push_back ({ "send:" + sends[i]->itemID.toString() + ":" + p->paramID, "send " + juce::String::charToString ((juce::juce_wchar) ('a' + i)) + " " + p->getParameterName().toLowerCase(), p });
        }
        auto& list = t.isMasterTrack() && s.edit != nullptr ? s.edit->getMasterPluginList() : t.pluginList;
        for (auto p : Session::getInserts (list, dynamic_cast<te::AudioTrack*> (&t)))
            for (auto ap : p->getAutomatableParameters())
                out.push_back ({ "plugin:" + p->itemID.toString() + ":" + ap->paramID, Signature::displayName (*p) + " > " + ap->getParameterName(), ap });
        return out;
    }

    /** The parameter the lane draws and the Pencil edits: the chosen view, or volume in waveform view. */
    inline te::AutomatableParameter* viewParam (Session& s, te::Track& t, bool* isAutomationView = nullptr)
    {
        auto v = currentView (t);
        if (isAutomationView) *isAutomationView = v != "waveform";
        if (v != "waveform")
            for (auto& c : viewChoices (s, t)) if (c.id == v && c.param != nullptr) return c.param;
        if (isAutomationView) *isAutomationView = false;
        return volumeParam (s, t);
    }

    inline juce::String viewLabel (Session& s, te::Track& t)
    {
        auto v = currentView (t);
        for (auto& c : viewChoices (s, t)) if (c.id == v) return c.label;
        return "waveform";
    }

    /** Menu of every view for a track; ids start at base. Returns the id->view map through 'ids'. */
    inline juce::PopupMenu viewMenu (Session& s, te::Track& t, int base, juce::StringArray& ids)
    {
        juce::PopupMenu m, sendsM, pluginsM;
        std::map<juce::String, juce::PopupMenu> byPlugin;
        auto cur = currentView (t);
        for (auto& c : viewChoices (s, t))
        {
            ids.add (c.id);
            int id = base + ids.size() - 1;
            if (c.id.startsWith ("send:")) sendsM.addItem (id, c.label, true, c.id == cur);
            else if (c.id.startsWith ("plugin:")) byPlugin[c.label.upToFirstOccurrenceOf (" > ", false, false)].addItem (id, c.label.fromFirstOccurrenceOf (" > ", false, false), true, c.id == cur);
            else m.addItem (id, c.label, true, c.id == cur);
        }
        if (sendsM.getNumItems() > 0) m.addSubMenu ("sends", sendsM);
        for (auto& [name, pm] : byPlugin) pluginsM.addSubMenu (name, pm);
        if (pluginsM.getNumItems() > 0) m.addSubMenu ("plug-ins", pluginsM);
        return m;
    }

    //==========================================================================
    /** Horizontal fader cap for the Edit window track headers. */
    struct HFaderLookAndFeel : public juce::LookAndFeel_V4
    {
        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
        {
            auto track = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 2.0f, (float) w, 4.0f);
            g.setColour (Theme::bg().darker (0.4f));
            g.fillRoundedRectangle (track, 2.0f);
            g.setColour (Theme::acid().withAlpha (0.35f));
            g.fillRoundedRectangle (track.withRight (pos), 2.0f);
            auto unityX = (float) s.getPositionOfValue (0.0);
            g.setColour (Theme::dim());
            g.fillRect (unityX, (float) y + 2.0f, 1.0f, (float) h - 4.0f);
            auto capH = juce::jmin ((float) h - 2.0f, 16.0f), capW = 12.0f;
            juce::Rectangle<float> cap (pos - capW * 0.5f, (float) y + (float) h * 0.5f - capH * 0.5f, capW, capH);
            juce::ColourGradient grad (Theme::panel2().brighter (0.35f), cap.getX(), cap.getY(), Theme::panel2().darker (0.5f), cap.getRight(), cap.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (cap, 2.5f);
            g.setColour (Theme::bg().darker (0.6f));
            g.drawRoundedRectangle (cap, 2.5f, 1.0f);
            g.setColour (s.getValue() > 0.05 ? Theme::amber() : Theme::acid());
            g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 2.0f, 2.0f, cap.getHeight() - 4.0f);
        }
        int getSliderThumbRadius (juce::Slider&) override   { return 6; }
    };
    inline HFaderLookAndFeel& hFaderLookAndFeel()   { static HFaderLookAndFeel l; return l; }

    /** Console fader cap for the pop-up fader (same look as the Mix window). */
    struct VFaderLookAndFeel : public juce::LookAndFeel_V4
    {
        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
        {
            auto track = juce::Rectangle<float> ((float) x + (float) w * 0.5f - 2.0f, (float) y, 4.0f, (float) h);
            g.setColour (Theme::bg().darker (0.4f));
            g.fillRoundedRectangle (track, 2.0f);
            g.setColour (Theme::line());
            g.drawRoundedRectangle (track, 2.0f, 1.0f);
            g.setColour (Theme::dim());
            g.setFont (Theme::font (9.0f));
            for (float db : { 6.0f, 0.0f, -5.0f, -10.0f, -20.0f, -40.0f })
            {
                auto yy = (float) s.getPositionOfValue (db);
                g.fillRect ((float) x + 2.0f, yy, 8.0f, 1.0f);
                g.drawText (juce::String ((int) db), x + 10, (int) yy - 6, 24, 12, juce::Justification::centredLeft);
            }
            auto capW = juce::jmin ((float) w - 30.0f, 26.0f), capH = 30.0f;
            juce::Rectangle<float> cap ((float) x + (float) w * 0.5f - capW * 0.5f, pos - capH * 0.5f, capW, capH);
            juce::ColourGradient grad (Theme::panel2().brighter (0.35f), cap.getX(), cap.getY(), Theme::panel2().darker (0.5f), cap.getRight(), cap.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (Theme::bg().darker (0.6f));
            g.drawRoundedRectangle (cap, 3.0f, 1.0f);
            g.setColour (s.getValue() > 0.05 ? Theme::amber() : Theme::acid());
            g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
        }
        int getSliderThumbRadius (juce::Slider&) override   { return 15; }
    };
    inline VFaderLookAndFeel& vFaderLookAndFeel()   { static VFaderLookAndFeel l; return l; }

    /** Fader law used by every fader in the Edit window: -inf..+6 dB (the engine's fader range), unity near the top. */
    inline void setupVolumeSlider (juce::Slider& s)
    {
        s.setRange (-100.0, 6.0, 0.1);
        s.setSkewFactorFromMidPoint (-18.0);
        s.setDoubleClickReturnValue (true, 0.0);
        s.textFromValueFunction = [] (double v) { return dbText ((float) v); };
        s.valueFromTextFunction = [] (const juce::String& t) { return t.trim().startsWithIgnoreCase ("-inf") ? -100.0 : t.getDoubleValue(); };
    }

    //==========================================================================
    /** Pop-up fader (click a volume indicator in the I/O view). Opt/Alt-click or double-click = 0 dB. */
    class FaderPopup : public juce::Component, private juce::Timer
    {
    public:
        FaderPopup (Session& s, te::Track& t) : session (s), track (&t)
        {
            setupVolumeSlider (fader);
            fader.setSliderStyle (juce::Slider::LinearVertical);
            fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
            fader.setLookAndFeel (&vFaderLookAndFeel());
            fader.setColour (juce::Slider::textBoxTextColourId, Theme::acid());
            fader.setColour (juce::Slider::textBoxBackgroundColourId, Theme::bg());
            fader.setColour (juce::Slider::textBoxOutlineColourId, Theme::line());
            fader.setValue (getVolumeDb (s, t), juce::dontSendNotification);
            fader.updateText();
            fader.onValueChange = [this] { if (auto* tt = track.get()) setVolumeDb (session, *tt, (float) fader.getValue()); };
            fader.onDragStart = [this] { if (auto* tt = track.get()) if (auto p = volumeParam (session, *tt)) p->parameterChangeGestureBegin(); };
            fader.onDragEnd   = [this] { if (auto* tt = track.get()) if (auto p = volumeParam (session, *tt)) p->parameterChangeGestureEnd(); };
            addAndMakeVisible (fader);
            title.setText (t.isMasterTrack() ? TrackOps::masterName (s) : t.getName(), juce::dontSendNotification);
            title.setJustificationType (juce::Justification::centred);
            title.setFont (Theme::font (12.0f, true));
            addAndMakeVisible (title);
            setSize (96, 280);
            startTimerHz (15);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            title.setBounds (r.removeFromTop (18));
            fader.setBounds (r);
        }
        void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isAltDown()) fader.setValue (0.0); }
    private:
        Session& session;
        te::SafeSelectable<te::Track> track;
        juce::Slider fader;
        juce::Label title;
        void timerCallback() override
        {
            if (auto* tt = track.get()) { if (! fader.isMouseButtonDown()) fader.setValue (getVolumeDb (session, *tt), juce::dontSendNotification); }
            else if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
        }
    };

    /** Pop-up pan slider(s) (click a pan indicator). Two sliders for a stereo track with Pro Tools' dual panner. */
    class PanPopup : public juce::Component, private juce::Timer
    {
    public:
        PanPopup (Session& s, te::Track& t) : session (s), track (&t)
        {
            const bool dual = stereoPan (t) != nullptr;
            for (int i = 0; i < (dual ? 2 : 1); ++i)
            {
                auto sl = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                sl->setRange (-1.0, 1.0, 0.01);
                sl->setDoubleClickReturnValue (true, i == 0 && dual ? -1.0 : (dual ? 1.0 : 0.0));
                sl->textFromValueFunction = [] (double v) { return panText ((float) v); };
                sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
                sl->setLookAndFeel (&hFaderLookAndFeel());
                sl->setColour (juce::Slider::textBoxTextColourId, Theme::acid());
                sl->setColour (juce::Slider::textBoxBackgroundColourId, Theme::bg());
                sl->updateText();
                sl->onValueChange = [this, i, dual, raw = sl.get()] { apply (i, dual, (float) raw->getValue()); };
                addAndMakeVisible (*sl);
                sliders.push_back (std::move (sl));
            }
            readValues();
            for (auto& sl : sliders) sl->updateText();
            setSize (240, dual ? 64 : 36);
            startTimerHz (15);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6, 4);
            for (auto& s : sliders) { s->setBounds (r.removeFromTop (24)); r.removeFromTop (4); }
        }
    private:
        Session& session;
        te::SafeSelectable<te::Track> track;
        std::vector<std::unique_ptr<juce::Slider>> sliders;
        void apply (int i, bool dual, float v)
        {
            auto* tt = track.get();
            if (tt == nullptr) return;
            if (dual) { if (auto sp = stereoPan (*tt)) (i == 0 ? sp->panL : sp->panR)->setParameter (v, juce::sendNotification); }
            else setPan (session, *tt, v);
        }
        void readValues()
        {
            auto* tt = track.get();
            if (tt == nullptr) return;
            if (auto sp = stereoPan (*tt))
            {
                if (sliders.size() > 1 && ! sliders[0]->isMouseButtonDown()) sliders[0]->setValue (sp->panL->getCurrentValue(), juce::dontSendNotification);
                if (sliders.size() > 1 && ! sliders[1]->isMouseButtonDown()) sliders[1]->setValue (sp->panR->getCurrentValue(), juce::dontSendNotification);
            }
            else if (! sliders.empty() && ! sliders[0]->isMouseButtonDown()) sliders[0]->setValue (getPan (session, *tt), juce::dontSendNotification);
        }
        void timerCallback() override
        {
            if (track.get() == nullptr) { if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss(); return; }
            readValues();
        }
    };

    inline void showFaderPopup (Session& s, te::Track& t, juce::Component& target)
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<FaderPopup> (s, t), target.getScreenBounds(), nullptr);
    }

    inline void showPanPopup (Session& s, te::Track& t, juce::Component& target)
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<PanPopup> (s, t), target.getScreenBounds(), nullptr);
    }
}
