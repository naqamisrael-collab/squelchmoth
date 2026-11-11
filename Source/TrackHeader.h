#pragma once
// Squelchmoth - Edit window track header: name, colour, R/I/S/M, fader, automation mode, Elastic Audio,
// playlists, folder collapse, context menu (I/O, inserts, colour, groups, folders, presets...).

#include "EditOps.h"
#include "TrackSlots.h"
#include "TrackOps.h"
#include "QuickNew.h"
#include "TrackMixControls.h"

//==============================================================================
/** The Pro Tools "Edit Window Views" columns beside a track name: I/O, Inserts A-E / F-J, Sends A-E / F-J,
    Comments. Shares the insert / send slot behaviour with the Mix window. */
class HeaderColumns : public juce::Component,
                      public SlotController
{
public:
    HeaderColumns (Session& s, te::Track& t) : SlotController (s, t.isMasterTrack() ? nullptr : &t), owner (&t), isMaster (t.isMasterTrack())
    {
        for (auto* b : { &inBtn, &outBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, Theme::bg());
            b->setColour (juce::TextButton::textColourOffId, Theme::text());
            addChildComponent (b);
        }
        inBtn.setTooltip ("Input path");
        outBtn.setTooltip ("Output path");
        for (auto* b : { &volBtn, &panBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, Theme::bg().brighter (0.05f));
            b->setColour (juce::TextButton::textColourOffId, Theme::acid());
            addChildComponent (b);
        }
        addChildComponent (outWinBtn);
        volBtn.setTooltip ("Volume - click for the fader, Opt-click for 0 dB");
        panBtn.setTooltip ("Pan - click for the pan slider(s)");
        outWinBtn.setTooltip ("Output Window: this track's fader, pan, meter, mute and solo in a window of its own");
        volBtn.onClick = [this]
        {
            if (owner == nullptr) return;
            if (juce::ModifierKeys::currentModifiers.isAltDown()) { MixCtl::setVolumeDb (session, *owner, 0.0f); refresh(); return; }
            MixCtl::showFaderPopup (session, *owner, volBtn);
        };
        panBtn.onClick = [this]
        {
            if (owner == nullptr) return;
            if (juce::ModifierKeys::currentModifiers.isAltDown()) { MixCtl::setPan (session, *owner, 0.0f); refresh(); return; }
            MixCtl::showPanPopup (session, *owner, panBtn);
        };
        outWinBtn.onClick = [this] { if (owner != nullptr && session.showOutputWindowHook) session.showOutputWindowHook (owner.get()); };
        inBtn.onClick  = [this] { if (track) IO::showInputMenu (session, *track, &inBtn, [this] { refresh(); }); };
        outBtn.onClick = [this]
        {
            if (track) IO::showOutputMenu (session, *track, &outBtn, [this] { refresh(); });
            else if (isMaster) IO::showMasterOutputMenu (session, &outBtn, [this] { refresh(); });
        };
        comments.setEditable (false, true);
        comments.setFont (Theme::font (11.0f));
        comments.setJustificationType (juce::Justification::topLeft);
        comments.setColour (juce::Label::textColourId, Theme::text());
        comments.setColour (juce::Label::backgroundColourId, Theme::bg().withAlpha (0.6f));
        comments.setTooltip ("Track comments - double-click to edit");
        comments.onTextChange = [this]
        {
            if (owner == nullptr) return;
            if (isMaster) session.edit->state.setProperty ("sqmMasterComments", comments.getText(), nullptr);
            else TrackOps::setComments (*owner, comments.getText());
        };
        addChildComponent (comments);
        build();
    }

    void slotsChanged() override   { build(); }

    void build()
    {
        inserts.clear(); sends.clear();
        auto mk = [this] (bool enabled, int base, bool isSend)
        {
            if (! enabled) return;
            if (isSend && track == nullptr) return;
            if (! isSend && ! hasInserts()) return;
            auto list = ! isSend ? Session::getInserts (getPluginList(), track) : juce::Array<te::Plugin*>();
            auto snd = isSend ? Session::getSends (*track) : juce::Array<te::AuxSendPlugin*>();
            for (int i = base; i < base + 5; ++i)
            {
                if (isSend) { auto c = std::make_unique<SendSlot> (*this, i, i < snd.size() ? snd[i] : nullptr, true); addAndMakeVisible (*c); sends.push_back (std::move (c)); }
                else        { auto c = std::make_unique<InsertSlot> (*this, i, i < list.size() ? te::Plugin::Ptr (list[i]) : nullptr, true); addAndMakeVisible (*c); inserts.push_back (std::move (c)); }
            }
        };
        mk (EditViews::insAE(), 0, false);
        mk (EditViews::insFJ(), 5, false);
        mk (EditViews::sendAE(), 0, true);
        mk (EditViews::sendFJ(), 5, true);
        signature = currentSignature();
        refresh();
        resized();
    }

    juce::String currentSignature()
    {
        juce::String sig;
        if (hasInserts()) for (auto p : Session::getInserts (getPluginList(), track)) sig << p->itemID.toString() << (p->isEnabled() ? "+" : "-");
        if (track) for (auto snd : Session::getSends (*track)) sig << snd->itemID.toString() << snd->getBusNumber();
        return sig;
    }

    void refresh()
    {
        if (currentSignature() != signature) { build(); return; }
        if (! EditViews::io()) for (auto* b : std::initializer_list<juce::Component*> { &volBtn, &panBtn, &outWinBtn }) b->setVisible (false);
        inBtn.setVisible (EditViews::io() && track != nullptr);
        outBtn.setVisible (EditViews::io() && (track != nullptr || isMaster));
        if (owner != nullptr)
        {
            volBtn.setButtonText (MixCtl::dbText (MixCtl::getVolumeDb (session, *owner)));
            panBtn.setButtonText (MixCtl::panSummary (session, *owner));
        }
        if (track) { inBtn.setButtonText (IO::inputLabel (session, *track)); outBtn.setButtonText (IO::outputLabel (session, *track)); }
        else if (isMaster) outBtn.setButtonText (IO::masterOutputLabel (session));
        comments.setVisible (EditViews::comments());
        if (! comments.isBeingEdited() && owner != nullptr)
            comments.setText (isMaster ? session.edit->state.getProperty ("sqmMasterComments").toString() : TrackOps::comments (*owner), juce::dontSendNotification);
        for (auto& c : inserts) c->refresh();
        for (auto& c : sends) c->refresh();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::panel().darker (0.15f));
        g.fillAll();
        g.setColour (Theme::line());
        for (auto x : dividers) g.drawVerticalLine (x, 0.0f, (float) getHeight());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        dividers.clear();
        auto r = getLocalBounds();
        int h = getHeight();
        const int slotH = juce::jlimit (6, 15, (h - 6) / 5);
        auto column = [&] (int w) { auto c = r.removeFromLeft (w); dividers.push_back (c.getX()); return c.reduced (4, 3); };
        if (EditViews::io())
        {
            auto c = column (EditViews::ioW);
            const bool three = c.getHeight() >= 3 * 14 + 4;
            auto rowH = juce::jmin (18, juce::jmax (12, (c.getHeight() - 4) / (three ? 3 : 2)));
            inBtn.setBounds (c.removeFromTop (rowH));
            c.removeFromTop (2);
            outBtn.setBounds (c.removeFromTop (rowH));
            const bool mixRow = three && owner != nullptr && MixCtl::volumePlugin (session, *owner) != nullptr;
            for (auto* b : std::initializer_list<juce::Component*> { &volBtn, &panBtn, &outWinBtn }) b->setVisible (mixRow);
            if (mixRow)
            {
                c.removeFromTop (2);
                auto row = c.removeFromTop (rowH);
                outWinBtn.setBounds (row.removeFromRight (18));
                row.removeFromRight (2);
                volBtn.setBounds (row.removeFromLeft (row.getWidth() / 2 - 1));
                row.removeFromLeft (2);
                panBtn.setBounds (row);
            }
        }
        auto placeSlots = [&] (auto& vec, int from)
        {
            auto c = column (EditViews::slotW);
            for (int i = from; i < from + 5 && i < (int) vec.size(); ++i) { vec[(size_t) i]->setBounds (c.removeFromTop (slotH)); c.removeFromTop (1); }
        };
        int insIdx = 0, sendIdx = 0;
        if (EditViews::insAE())  { if (hasInserts()) placeSlots (inserts, insIdx); else column (EditViews::slotW); insIdx += 5; }
        if (EditViews::insFJ())  { if (hasInserts()) placeSlots (inserts, insIdx); else column (EditViews::slotW); insIdx += 5; }
        if (EditViews::sendAE()) { if (track) placeSlots (sends, sendIdx); else column (EditViews::slotW); sendIdx += 5; }
        if (EditViews::sendFJ()) { if (track) placeSlots (sends, sendIdx); else column (EditViews::slotW); sendIdx += 5; }
        if (EditViews::comments()) comments.setBounds (column (EditViews::commentsW));
    }

private:
    te::Track::Ptr owner;
    bool isMaster;
    /** Pro Tools' Output Window button: a tiny fader icon. */
    struct FaderIconButton : public juce::Button
    {
        FaderIconButton() : juce::Button ("Output Window") {}
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (down ? Theme::acid().darker (0.4f) : over ? Theme::panel2().brighter (0.2f) : Theme::bg().brighter (0.05f));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (Theme::acid());
            auto cx = r.getCentreX();
            g.fillRect (cx - 0.5f, r.getY() + 2.0f, 1.0f, r.getHeight() - 4.0f);
            g.fillRoundedRectangle (cx - 3.5f, r.getCentreY() - 1.5f, 7.0f, 4.0f, 1.0f);
        }
    };
    juce::TextButton inBtn, outBtn, volBtn, panBtn;
    FaderIconButton outWinBtn;
    juce::Label comments;
    std::vector<std::unique_ptr<InsertSlot>> inserts;
    std::vector<std::unique_ptr<SendSlot>> sends;
    std::vector<int> dividers;
    juce::String signature;
};

class TrackHeader : public juce::Component,
                    private juce::Timer
{
public:
    TrackHeader (Session& s, TimelineState& st, te::Track& t) : session (s), state (st), keepAlive (&t), track (t)
    {
        audio = dynamic_cast<te::AudioTrack*> (&t);
        folder = dynamic_cast<te::FolderTrack*> (&t);
        master = t.isMasterTrack();

        name.setText (master ? TrackOps::masterName (session) : t.getName(), juce::dontSendNotification);
        name.setFont (Theme::font (13.5f, true));
        name.setEditable (false, true);
        name.onTextChange = [this] { if (master) TrackOps::setMasterName (session, name.getText()); else track.setName (name.getText()); };
        name.setTooltip ("Double-click to rename");
        addAndMakeVisible (name);

        for (auto* b : { &rec, &input, &solo, &mute })
        {
            b->setClickingTogglesState (false);
            addChildComponent (b);
        }
        rec.setVisible (audio != nullptr);  input.setVisible (audio != nullptr);
        polarityBtn.setClickingTogglesState (false);
        polarityBtn.setTooltip ("Polarity (phase) invert - flips this track's signal. Use it when two mics on one source cancel each other "
                                "(snare top/bottom, kick in/out). Glitch-free, undoable, saved with the session.");
        polarityBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1fa8c4));   // console cyan
        polarityBtn.onClick = [this]
        {
            const bool inv = ! TrackOps::isPolarityInverted (track);
            for (auto t : targets()) TrackOps::setPolarityInverted (*t, inv);
            refresh();
        };
        addChildComponent (polarityBtn);
        solo.setVisible (! master);          mute.setVisible (! master);

        rec.setTooltip ("Record arm (Shift+R)");    input.setTooltip ("Input monitor");
        solo.setTooltip ("Solo (Shift+S)");         mute.setTooltip ("Mute (Shift+M)");
        rec.onClick   = [this] { if (audio) { bool arm = ! session.isArmed (*audio); session.setArmed (*audio, arm); for (auto m : session.groupMates (*audio, 3)) session.setArmed (*m, arm); } refresh(); };
        input.onClick = [this] { toggleMonitor(); };
        solo.onClick  = [this] { session.setTrackSolo (track, ! track.isSolo (false)); refresh(); };
        mute.onClick  = [this] { session.setTrackMute (track, ! track.isMuted (false)); refresh(); };

        // the same key colours as the console strips: R red, S yellow, M orange, I live green, polarity cyan
        rec.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe03030));
        solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd8bd1e));
        mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe07e1c));
        input.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1fb86a));

        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        MixCtl::setupVolumeSlider (volume);
        volume.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 16);
        volume.setLookAndFeel (&MixCtl::hFaderLookAndFeel());
        volume.setColour (juce::Slider::textBoxTextColourId, Theme::text());
        volume.setColour (juce::Slider::textBoxBackgroundColourId, Theme::bg());
        volume.setColour (juce::Slider::textBoxOutlineColourId, Theme::line());
        volume.setTooltip ("Volume fader - drag, double-click for 0 dB, click the number to type a value. Grouped tracks move together. "
                           "Cmd-drag for fine control.");
        volume.setVelocityModeParameters (1.0, 1, 0.0, false, juce::ModifierKeys::commandModifier);
        volume.onValueChange = [this] { MixCtl::setVolumeDb (session, track, (float) volume.getValue()); };
        panBtn.setTooltip ("Pan - click for the pan slider" );
        panBtn.onClick = [this] { MixCtl::showPanPopup (session, track, panBtn); };
        addChildComponent (panBtn);
        viewBtn.setTooltip ("Track View: waveform, volume, pan, send levels or any plug-in parameter (automation)");
        viewBtn.onClick = [this] { showViewMenu(); };
        addChildComponent (viewBtn);
        volume.onDragStart = [this] { if (auto vp = volParam()) vp->parameterChangeGestureBegin(); };
        volume.onDragEnd   = [this] { if (auto vp = volParam()) vp->parameterChangeGestureEnd(); };
        addAndMakeVisible (volume);

        autoBtn.setTooltip ("Automation mode: Off / Read / Touch / Latch / Write");
        autoBtn.onClick = [this] { showAutomationMenu(); };
        addAndMakeVisible (autoBtn);

        if (audio != nullptr)
        {
            elasticBtn.setTooltip ("Elastic Audio algorithm for this track");
            elasticBtn.onClick = [this] { showElasticMenu(); };
            addAndMakeVisible (elasticBtn);
            playlistBtn.setTooltip ("Playlists / takes on this track");
            playlistBtn.onClick = [this] { showPlaylistMenu(); };
            addAndMakeVisible (playlistBtn);
        }

        if (folder != nullptr)
        {
            collapse.setTooltip ("Show / hide the tracks inside this folder");
            collapse.onClick = [this]
            {
                if (state.collapsedFolders.count (track.itemID)) state.collapsedFolders.erase (track.itemID);
                else state.collapsedFolders.insert (track.itemID);
                if (onStructureChanged) onStructureChanged();
            };
            addAndMakeVisible (collapse);
        }

        viewsChanged();
        refresh();
        startTimerHz (6);
    }

    /** View > Edit Window Views changed: add / remove the I/O, Inserts, Sends and Comments columns. */
    void viewsChanged()
    {
        if (EditViews::width() > 0) { if (cols == nullptr) { cols = std::make_unique<HeaderColumns> (session, track); addAndMakeVisible (*cols); } else cols->build(); }
        else cols.reset();
        resized();
    }

    te::Track& getTrack()                 { return track; }
    te::AudioTrack* getAudioTrack()       { return audio; }
    std::function<void()> onSelect, onStructureChanged;
    std::function<void (te::Track&, const juce::MouseEvent&)> onDragReorder;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().withWidth (baseHeaderWidth);
        bool selected = state.isTrackSelected (&track);
        int depth = Session::depthOf (&track);

        // 1.10 console finish: a lit top edge and a soft gradient body; the track colour is a rounded bar with a glow
        {
            auto base = selected ? Theme::panel2().brighter (0.08f) : Theme::panel();
            g.setGradientFill (juce::ColourGradient (base.brighter (0.06f), 0.0f, 0.0f, base.darker (0.12f), 0.0f, (float) getHeight(), false));
            g.fillRect (r);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine (0, (float) depth * 10.0f, (float) r.getRight());
        }
        if (depth > 0)
        {
            g.setColour (Theme::panel2().withAlpha (0.6f));
            g.fillRect (0, 0, depth * 10, getHeight());
        }
        {
            auto bar = juce::Rectangle<float> ((float) depth * 10.0f + 1.0f, 3.0f, 4.0f, (float) getHeight() - 7.0f);
            g.setColour (track.getColour().withAlpha (0.25f));
            g.fillRoundedRectangle (bar.expanded (2.0f, 1.0f), 3.0f);
            g.setColour (track.getColour());
            g.fillRoundedRectangle (bar, 2.0f);
        }
        if (folder != nullptr)
        {
            g.setColour (track.getColour().withAlpha (0.12f));
            g.fillRect (r);
        }
        g.setColour (Theme::line());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        if (selected)
        {   // the same cyan rim the console uses for a selected strip
            auto sr = r.withTrimmedLeft (depth * 10 + 7).toFloat().reduced (1.0f, 1.5f);
            g.setColour (juce::Colour (0xff38e1ff).withAlpha (0.18f));
            g.drawRoundedRectangle (sr.expanded (1.0f), 5.0f, 2.0f);
            g.setColour (juce::Colour (0xff38e1ff).withAlpha (0.85f));
            g.drawRoundedRectangle (sr, 4.0f, 1.2f);
        }
        if (! master && TrackOps::isInactive (track))
        {
            g.setColour (Theme::bg().withAlpha (0.55f));
            g.fillRect (r);
        }

        if (getHeight() > 44)
        {
            g.setColour (Theme::dim());
            g.setFont (Theme::font (10.0f));
            juce::String kind;
            if (master) kind = juce::String ("MASTER FADER ") + (TrackOps::isMonoFormat (session, track) ? "MONO" : "ST");
            else if (folder != nullptr)
            {
                auto k = folder->state.getProperty ("sqmKind").toString();
                kind = folder->isSubmixFolder() ? juce::String ("ROUTING FOLDER ") + (TrackOps::isMonoFormat (session, track) ? "MONO" : "ST")
                                                : juce::String (k == "basicFolder" || folder->getVCAPlugin() == nullptr ? "FOLDER" : "VCA");
            }
            else if (audio != nullptr)
            {
                auto k = IO::kindOf (*audio);
                kind = k == IO::Kind::aux ? "AUX" : k == IO::Kind::instrument ? "INST" : k == IO::Kind::midi ? "MIDI" : "AUDIO";
                if (k != IO::Kind::midi) kind << " " << (IO::formatOf (*audio) == "stereo" ? "ST" : "MONO");
                if (TrackOps::isTicks (track)) kind << "  ticks";
            }
            if (! master && TrackOps::isInactive (track)) kind << "  INACTIVE";
            if (TrackOps::isRecordSafe (track)) kind << "  REC SAFE";
            if (TrackOps::isSoloSafe (track)) kind << "  SOLO SAFE";
            if (audio != nullptr && audio->isFrozen (te::Track::individualFreeze)) kind << "  FROZEN";
            auto idx = master ? 0 : session.getRows().indexOf (&track) + 1;
            juce::String groups;
            for (auto& gr : session.getGroups()) if (std::find (gr.tracks.begin(), gr.tracks.end(), track.itemID) != gr.tracks.end()) groups << gr.id;
            g.drawText ((idx > 0 ? juce::String (idx) + "  " : juce::String()) + kind + (groups.isNotEmpty() ? "   grp " + groups : ""), depth * 10 + 12, getHeight() - 15, baseHeaderWidth - depth * 10 - 16, 12, juce::Justification::left);
        }
    }

    void resized() override
    {
        int depth = Session::depthOf (&track);
        auto full = getLocalBounds();
        if (cols != nullptr) cols->setBounds (full.removeFromRight (juce::jmax (0, getWidth() - baseHeaderWidth)));
        auto r = full.reduced (6, 4).withTrimmedLeft (6 + depth * 10);
        auto top = r.removeFromTop (20);
        if (folder != nullptr) { collapse.setBounds (top.removeFromLeft (18)); top.removeFromLeft (2); }
        autoBtn.setBounds (top.removeFromRight (44));
        top.removeFromRight (3);
        if (audio != nullptr) { elasticBtn.setBounds (top.removeFromRight (30)); top.removeFromRight (3); playlistBtn.setBounds (top.removeFromRight (24)); top.removeFromRight (3); }
        name.setBounds (top);

        if (getHeight() < 46) { for (auto* b : { &rec, &input, &solo, &mute, &polarityBtn }) b->setVisible (false); volume.setVisible (false); panBtn.setVisible (false); viewBtn.setVisible (false); return; }
        rec.setVisible (audio != nullptr && IO::kindOf (*audio) != IO::Kind::aux); input.setVisible (audio != nullptr && IO::kindOf (*audio) != IO::Kind::aux);
        solo.setVisible (! master); mute.setVisible (! master); volume.setVisible (true);
        polarityBtn.setVisible (hasPolarity() && getHeight() >= 46);

        r.removeFromTop (3);
        auto row = r.removeFromTop (20);
        for (auto* b : { &rec, &input, &solo, &mute, &polarityBtn })
        {
            if (! b->isVisible()) continue;
            b->setBounds (row.removeFromLeft (26));
            row.removeFromLeft (3);
        }
        const bool roomy = getHeight() >= 68;   // Pro Tools-style: buttons + track view, then a fader row
        viewBtn.setVisible (roomy);
        panBtn.setVisible (roomy);
        if (roomy)
        {
            viewBtn.setBounds (row.withTrimmedLeft (2));
            r.removeFromTop (4);
            auto frow = r.removeFromTop (18);
            panBtn.setBounds (frow.removeFromRight (master || folder != nullptr ? 0 : 52));
            frow.removeFromRight (4);
            volume.setBounds (frow);
        }
        else volume.setBounds (row.withTrimmedLeft (2));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        bool add = e.mods.isShiftDown() || e.mods.isCommandDown();
        if (add && state.isTrackSelected (&track) && e.mods.isCommandDown())
        {
            state.selectedTrackIDs.erase (std::remove (state.selectedTrackIDs.begin(), state.selectedTrackIDs.end(), track.itemID), state.selectedTrackIDs.end());
            if (state.selectedRow == &track) { state.selectedRow = nullptr; state.selectedTrack = nullptr; }
        }
        else if (e.mods.isShiftDown() && state.selectedRow != nullptr && state.selectedRow != &track)
        {
            // range-select between the primary selected track and this one
            auto rows = session.getRows (&state.collapsedFolders);
            int a = rows.indexOf (state.selectedRow), b = rows.indexOf (&track);
            if (a >= 0 && b >= 0)
                for (int i = juce::jmin (a, b); i <= juce::jmax (a, b); ++i)
                    if (! state.isTrackSelected (rows[i])) state.selectedTrackIDs.push_back (rows[i]->itemID);
        }
        else
            state.selectTrack (&track, add);

        if (onSelect) onSelect();
        if (e.mods.isPopupMenu()) showMenu();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || name.isBeingEdited()) return;
        if (std::abs (e.getDistanceFromDragStartY()) > 12 && onDragReorder) onDragReorder (track, e);
    }

    void refresh()
    {
        if (audio != nullptr)
        {
            rec.setToggleState (session.isArmed (*audio), juce::dontSendNotification);
            input.setToggleState (isMonitoring(), juce::dontSendNotification);
            auto idx = Session::getElasticMode (track);
            elasticBtn.setButtonText (idx == 0 ? "EL" : juce::String (Session::elasticModeNames()[idx]).substring (0, 2).toUpperCase());
            elasticBtn.setColour (juce::TextButton::buttonColourId, idx == 0 ? Theme::panel2() : Theme::mint().darker (0.5f));
            int takes = 0;
            for (auto c : audio->getClips()) if (auto a = dynamic_cast<te::AudioClipBase*> (c)) if (a->hasAnyTakes()) takes = juce::jmax (takes, (int) a->getTakes().size());
            playlistBtn.setButtonText (takes > 0 ? "P" + juce::String (takes) : "P");
            playlistBtn.setColour (juce::TextButton::buttonColourId, takes > 0 ? Theme::violet().darker (0.3f) : Theme::panel2());
        }
        solo.setToggleState (track.isSolo (false), juce::dontSendNotification);
        mute.setToggleState (track.isMuted (false), juce::dontSendNotification);
        polarityBtn.setToggleState (TrackOps::isPolarityInverted (track), juce::dontSendNotification);
        auto shownName = master ? TrackOps::masterName (session) : track.getName();
        if (! name.isBeingEdited() && name.getText() != shownName)
            name.setText (shownName, juce::dontSendNotification);
        rec.setAlpha (TrackOps::isRecordSafe (track) ? 0.35f : 1.0f);
        if (cols != nullptr) cols->refresh();

        if (! volume.isMouseButtonDown())
            volume.setValue (MixCtl::getVolumeDb (session, track), juce::dontSendNotification);
        panBtn.setButtonText (MixCtl::panSummary (session, track));
        viewBtn.setButtonText (MixCtl::viewLabel (session, track) + "  v");
        viewBtn.setColour (juce::TextButton::buttonColourId, MixCtl::currentView (track) == "waveform" ? Theme::panel2() : Theme::acid().darker (0.65f));

        auto am = Session::getAutomationModeIndex (track);
        static const char* names[] = { "off", "read", "touch", "latch", "write" };
        autoBtn.setButtonText (names[juce::jlimit (0, 4, am)]);
        autoBtn.setColour (juce::TextButton::buttonColourId, am <= 1 ? Theme::panel2() : (am == 4 ? Theme::red().darker (0.4f) : Theme::amber().darker (0.6f)));
        if (folder) collapse.setButtonText (state.collapsedFolders.count (track.itemID) ? ">" : "v");
    }

private:
    Session& session;
    TimelineState& state;
    te::Track::Ptr keepAlive;   // the track object must outlive this header even if it is deleted mid-paint
    te::Track& track;
    te::AudioTrack* audio = nullptr;
    te::FolderTrack* folder = nullptr;
    bool master = false;
    std::unique_ptr<HeaderColumns> cols;
    juce::Label name;
    juce::TextButton polarityBtn { juce::String::fromUTF8 ("\xc3\x98") };
    bool hasPolarity()  { return ! master && TrackOps::polarityStage (track) != nullptr && ! (audio != nullptr && IO::kindOf (*audio) == IO::Kind::midi); }
    juce::TextButton rec { "R" }, input { "I" }, solo { "S" }, mute { "M" }, autoBtn { "read" }, elasticBtn { "EL" }, playlistBtn { "P" }, collapse { "v" };
    juce::Slider volume;
    juce::TextButton panBtn { "<0>" }, viewBtn { "waveform" };
    juce::StringArray viewIds;

    void timerCallback() override   { refresh(); }

    void showViewMenu()
    {
        viewIds.clear();
        auto m = MixCtl::viewMenu (session, track, 1, viewIds);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (viewBtn), [this, sp = juce::Component::SafePointer<juce::Component> (this)] (int r)
        {
            if (sp == nullptr || r <= 0 || r - 1 >= viewIds.size()) return;
            MixCtl::setView (session, track, viewIds[r - 1]);
            refresh();
            if (onStructureChanged) onStructureChanged();
        });
    }

    te::AutomatableParameter* volParam()
    {
        if (audio) { if (auto vp = audio->getVolumePlugin()) return vp->volParam.get(); }
        else if (master) { if (auto mv = session.edit->getMasterVolumePlugin()) return mv->volParam.get(); }
        else if (folder) { if (auto vca = folder->getVCAPlugin()) return vca->volParam.get(); if (auto vp = folder->getVolumePlugin()) return vp->volParam.get(); }
        return nullptr;
    }

    bool isMonitoring()   { return TrackOps::isInputMonitorOn (track); }

    void toggleMonitor()
    {
        if (audio == nullptr) return;
        // per track, like Pro Tools: other tracks on the same input (or the same MIDI device) are not affected
        TrackOps::setInputMonitor (session, track, ! isMonitoring());
        refresh();
    }

    void showAutomationMenu()
    {
        juce::PopupMenu m;
        auto names = Session::automationModeNames();
        auto cur = Session::getAutomationModeIndex (track);
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, cur == i);
        m.addSeparator();
        viewIds.clear();
        m.addSubMenu ("Track View (show automation)", MixCtl::viewMenu (session, track, 200, viewIds));
        m.addItem (102, "Clear automation shown in this lane", MixCtl::currentView (track) != "waveform" || (MixCtl::volumeParam (session, track) != nullptr && MixCtl::volumeParam (session, track)->hasAutomationPoints()));
        m.addItem (101, "Clear all automation on this track");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (autoBtn), [this] (int r)
        {
            if (r >= 1 && r <= 5) Session::setAutomationModeIndex (track, r - 1);
            else if (r == 101) { for (auto p : track.pluginList.getPlugins()) for (auto ap : p->getAutomatableParameters()) ap->getCurve().clear (&track.edit.getUndoManager()); }
            else if (r == 102) { if (auto p = MixCtl::viewParam (session, track)) p->getCurve().clear (&track.edit.getUndoManager()); }
            else if (r >= 200 && r - 200 < viewIds.size()) { MixCtl::setView (session, track, viewIds[r - 200]); if (onStructureChanged) onStructureChanged(); }
            refresh();
        });
    }

    void showElasticMenu()
    {
        juce::PopupMenu m;
        auto names = Session::elasticModeNames();
        auto cur = Session::getElasticMode (track);
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, cur == i);
        m.addSeparator();
        m.addItem (50, "Conform clips to session tempo (tempo detect)");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (elasticBtn), [this] (int r)
        {
            if (audio == nullptr) return;
            if (r >= 1 && r <= 6) session.setElasticMode (*audio, r - 1);
            else if (r == 50)
                for (auto c : audio->getClips())
                    if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
                    {
                        if (! ac->getAutoTempo()) { if (Session::getElasticMode (track) == 0) session.setElasticMode (*audio, 1); }
                        ac->performTempoDetect();
                        ac->setAutoTempo (true);
                    }
            refresh();
        });
    }

    void showPlaylistMenu()
    {
        if (audio == nullptr) return;
        juce::PopupMenu m;
        te::AudioClipBase* takeClip = nullptr;
        for (auto c : audio->getClips()) if (auto a = dynamic_cast<te::AudioClipBase*> (c)) if (a->hasAnyTakes()) { takeClip = a; break; }
        if (takeClip == nullptr)
        {
            m.addItem (1, "No alternate playlists yet - loop record to create takes", false);
            m.addItem (2, "Duplicate track as new playlist");
        }
        else
        {
            auto takes = takeClip->getTakes();
            for (int i = 0; i < takes.size(); ++i) m.addItem (10 + i, "Playlist " + juce::String (i + 1) + "  (" + takeClip->getName() + ")", true, takeClip->getCurrentTake() == i);
            m.addSeparator();
            m.addItem (3, "Expand all takes to new tracks");
            m.addItem (4, "Expand takes to lanes on this track (comp)");
            m.addItem (5, "Keep current take only");
        }
        te::Clip::Ptr tc (takeClip);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (playlistBtn), [this, tc] (int r)
        {
            if (r == 2)
            {
                auto st = track.state.createCopy();
                te::EditItemID::remapIDs (st, nullptr, track.edit);
                st.setProperty (te::IDs::name, track.getName() + ".01", nullptr);
                track.edit.insertTrack (te::TrackInsertPoint (track.getParentFolderTrack(), &track), st, nullptr);
            }
            else if (tc != nullptr)
            {
                if (r >= 10) tc->setCurrentTake (r - 10);
                else if (r == 3) tc->unpackTakes (true);
                else if (r == 4) tc->unpackTakes (false);
                else if (r == 5) if (auto wc = dynamic_cast<te::WaveAudioClip*> (tc.get())) wc->deleteAllUnusedTakes (false);
            }
            refresh();
        });
    }

    /** The tracks a command applies to: every selected track, plus this one (Pro Tools behaviour). */
    juce::Array<te::Track*> targets()
    {
        juce::Array<te::Track*> out;
        for (auto t : session.getRows()) if (state.isTrackSelected (t)) out.add (t);
        if (! out.contains (&track)) out.add (&track);
        return out;
    }

public:
    std::function<void (te::Track*)> onScrollTo;
    std::function<void (juce::Array<te::Track*>)> onFitHeights;   // "Fit to Window": share the visible height between these tracks

    void buildHeightMenu (juce::PopupMenu& heights)
    {
        const int cur = state.heightFor (&track);
        for (int i = 0; i < (int) TimelineState::heightPresets().size(); ++i)
        {
            auto& p = TimelineState::heightPresets()[(size_t) i];
            heights.addItem (8800 + i, juce::String (p.name) + "   (" + juce::String (p.px) + " px)", true, cur == p.px);
        }
        heights.addSeparator();
        heights.addItem (8810, "Fit to Window", onFitHeights != nullptr);
        heights.addSeparator();
        heights.addItem (8811, "Ctrl+Up / Ctrl+Down steps through these sizes", false);
    }

    void showMenu()
    {
        juce::PopupMenu m, colours, groups, folders, presets, heights, timebaseMenu;
        enum
        {
            idRename = 1, idDelete = 3, idDuplicate = 6, idHide = 13, idHideInactive = 14, idInactive = 15, idSplitMono = 16, idFreeze = 17, idCommit = 18,
            idBounce = 19, idRecSafe = 20, idSoloSafe = 21, idComments = 22, idInput = 23, idOutput = 24, idSamples = 25, idTicks = 26, idDeleteMaster = 27
        };

        m.addItem (40, "New...  (" SQ_CMD "+Shift+N)");
        m.addSubMenu ("Quick New", QuickNew::build (session, 7000, false));
        m.addSeparator();
        m.addItem (idRename, "Rename...");
        if (master)
        {
            m.addItem (idOutput, "Output: " + IO::masterOutputLabel (session) + "...");
            m.addItem (idHide, "Hide");
            m.addSeparator();
            buildHeightMenu (heights);
            m.addSubMenu ("Track Height", heights);
            m.addItem (idComments, "Comments...");
            m.addSeparator();
            m.addItem (idDeleteMaster, "Delete Master Fader");
        }
        else
        {
            bool inactive = TrackOps::isInactive (track);
            m.addItem (idHide, "Hide");
            m.addItem (idHideInactive, "Hide and Make Inactive");
            m.addItem (idInactive, inactive ? "Make Active" : "Make Inactive");
            m.addSeparator();
            m.addItem (idDuplicate, "Duplicate...");
            if (audio != nullptr)
            {
                bool frozen = audio->isFrozen (te::Track::individualFreeze);
                m.addItem (idSplitMono, "Split into Mono", IO::formatOf (*audio) == "stereo" || session.trackIsStereo (*audio));
                m.addItem (idFreeze, frozen ? "Unfreeze" : "Freeze", true, frozen);
                m.addItem (idCommit, "Commit...");
                m.addItem (idBounce, "Bounce...");
            }
            m.addItem (idDelete, "Delete...");
            m.addSeparator();
            if (audio != nullptr)
            {
                m.addItem (idInput, "Input: " + IO::inputLabel (session, *audio) + "...");
                m.addItem (idOutput, "Output: " + IO::outputLabel (session, *audio) + "...");
                m.addSubMenu ("Insert plug-in", PluginMenu::build (session.getEngine(), PluginMenu::all, IO::formatOf (*audio) == "stereo"));
                m.addItem (idRecSafe, "Record Safe", IO::kindOf (*audio) != IO::Kind::aux, TrackOps::isRecordSafe (track));
            }
            m.addItem (idSoloSafe, "Solo Safe", true, TrackOps::isSoloSafe (track));
            if (audio != nullptr)
            {
                timebaseMenu.addItem (idSamples, "Samples (absolute time)", true, ! TrackOps::isTicks (track));
                timebaseMenu.addItem (idTicks, "Ticks (follows tempo)", true, TrackOps::isTicks (track));
                m.addSubMenu ("Timebase", timebaseMenu);
            }
            m.addItem (idComments, "Comments...");
            m.addSeparator();

            auto& pal = Theme::trackPalette();
            for (int i = 0; i < pal.size(); ++i) colours.addColouredItem (3000 + i, "Colour " + juce::String (i + 1), pal[i]);
            colours.addSeparator();
            colours.addItem (3999, "Custom colour...");
            m.addSubMenu ("Color", colours);

            auto grps = session.getGroups();
            groups.addItem (4000, "New group with selected tracks...");
            for (int i = 0; i < (int) grps.size(); ++i)
            {
                bool member = std::find (grps[(size_t) i].tracks.begin(), grps[(size_t) i].tracks.end(), track.itemID) != grps[(size_t) i].tracks.end();
                groups.addItem (4001 + i, grps[(size_t) i].id + ": " + grps[(size_t) i].name + (member ? "  (remove)" : "  (add)"), true, member);
            }
            m.addSubMenu ("Groups", groups);

            folders.addItem (5000, "Move to New Routing Folder");
            folders.addItem (5003, "Move to New Basic Folder");
            folders.addItem (5001, "New VCA controlling selected tracks");
            folders.addItem (5002, "Move to top level", track.getParentFolderTrack() != nullptr);
            int id = 5010;
            std::map<int, te::FolderTrack*> folderMap;
            for (auto f : session.getFolderTracks())
                if (f != &track) { folders.addItem (id, "Move into " + f->getName()); folderMap[id++] = f; }
            m.addSubMenu ("Folders / VCA", folders);
            folderMapForMenu = folderMap;

            buildHeightMenu (heights);
            m.addSubMenu ("Track Height", heights);

            presets.addItem (6000, "Save track preset...");
            auto presetDir = session.sessionsDir.getChildFile ("Track Presets");
            id = 6001;
            presetFilesForMenu = presetDir.findChildFiles (juce::File::findFiles, false, "*.sqmtrack");
            for (auto& f : presetFilesForMenu) presets.addItem (id++, "Load " + f.getFileNameWithoutExtension());
            m.addSubMenu ("Track Presets", presets);
            groupsForMenu = grps;
        }

        te::Track::Ptr keep (&track);
        m.showMenuAsync ({}, [this, keep] (int r) { if (r != 0) handleMenu (r); });
    }

private:
    std::map<int, te::FolderTrack*> folderMapForMenu;
    juce::Array<juce::File> presetFilesForMenu;
    std::vector<Session::Group> groupsForMenu;

    void handleMenu (int r)
    {
        auto& e = track.edit;
        auto all = targets();
        auto changed = [this] { if (onStructureChanged) onStructureChanged(); refresh(); repaint(); };
        auto audioTargets = [&all] { juce::Array<te::AudioTrack*> a; for (auto t : all) if (auto at = dynamic_cast<te::AudioTrack*> (t)) a.add (at); return a; };

        if (r == 40) { if (session.showNewTracksHook) session.showNewTracksHook(); return; }
        if (r > 7000 && r < 7100) { if (session.quickNewHook) session.quickNewHook (r, 7000); return; }
        if (r == 1) { name.showEditor(); return; }
        if (r == 27) { TrackOps::setMasterFader (session, false); session.log.add ("You", "Deleted the Master Fader"); changed(); return; }
        if (r == 23 && audio) { IO::showInputMenu (session, *audio, this, [this] { refresh(); }); return; }
        if (r == 24)
        {
            if (audio) IO::showOutputMenu (session, *audio, this, [this] { refresh(); });
            else if (master) IO::showMasterOutputMenu (session, this, [this] { refresh(); });
            return;
        }
        if (r == 22) { editComments(); return; }
        if (r == 13 || r == 14)
        {
            for (auto t : all) { t->setHidden (true); if (r == 14 && ! t->isMasterTrack()) TrackOps::setInactive (*t, true); }
            if (master) session.edit->state.setProperty ("sqmMasterHidden", true, nullptr);
            session.log.add ("You", "Hid " + juce::String (all.size()) + " track(s) - Track > Show All Tracks brings them back");
            changed(); return;
        }
        if (r == 15) { bool make = ! TrackOps::isInactive (track); for (auto t : all) TrackOps::setInactive (*t, make); changed(); return; }
        if (r == 6) { duplicateDialog(); return; }
        if (r == 16 && audio)
        {
            juce::String err;
            TrackOps::splitIntoMono (session, *audio, err);
            if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Split into Mono", err);
            changed(); return;
        }
        if (r == 17)
        {
            bool on = audio != nullptr && ! audio->isFrozen (te::Track::individualFreeze);
            juce::StringArray errs;
            for (auto at : audioTargets()) { auto err = TrackOps::freeze (*at, on); if (err.isNotEmpty()) errs.add (err); }
            if (! errs.isEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Freeze", errs.joinIntoString ("\n"));
            changed(); return;
        }
        if (r == 18) { TrackOps::commitDialog (session, audioTargets(), [this] { if (onStructureChanged) onStructureChanged(); }); return; }
        if (r == 19) { TrackOps::bounceTracks (session, audioTargets()); return; }
        if (r == 3)
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete Tracks",
                "Delete " + juce::String (all.size()) + " track(s)? Their clips and plug-ins are removed (Undo brings them back).", "Delete", "Cancel", nullptr,
                juce::ModalCallbackFunction::create ([this, all] (int ok)
                {
                    if (ok != 1) return;
                    for (auto t : all) if (! t->isMasterTrack()) { state.dropReferencesTo (t); t->edit.deleteTrack (t); }
                }));
            return;
        }
        if (r == 20) { bool safe = ! TrackOps::isRecordSafe (track); for (auto t : all) TrackOps::setRecordSafe (session, *t, safe); changed(); return; }
        if (r == 21) { bool safe = ! TrackOps::isSoloSafe (track); for (auto t : all) TrackOps::setSoloSafe (*t, safe); changed(); return; }
        if (r == 25 || r == 26) { for (auto at : audioTargets()) TrackOps::setTimebase (*at, r == 26); changed(); return; }
        if (r >= 8800 && r < 8800 + (int) TimelineState::heightPresets().size()) { for (auto t : all) state.setHeight (t, TimelineState::heightPresets()[(size_t) (r - 8800)].px); changed(); return; }
        if (r == 8810 && onFitHeights) { onFitHeights (all); return; }
        if (r >= 3000 && r < 3000 + Theme::trackPalette().size())
        {
            auto col = Theme::trackPalette()[r - 3000];
            for (auto t : all) { t->setColour (col); for (auto mt : session.groupMates (*t, 4)) mt->setColour (col); }
            changed(); return;
        }
        if (r == 3999) { pickCustomColour(); return; }
        if (r == 4000) { createGroupDialog(); return; }
        if (r > 4000 && r <= 4000 + (int) groupsForMenu.size())
        {
            auto g = groupsForMenu[(size_t) (r - 4001)];
            auto it = std::find (g.tracks.begin(), g.tracks.end(), track.itemID);
            if (it != g.tracks.end()) g.tracks.erase (it); else g.tracks.push_back (track.itemID);
            session.setGroup (g);
            changed(); return;
        }
        if (r == 5000 || r == 5001 || r == 5003)
        {
            juce::Array<te::Track*> members;
            for (auto t : all) if (! t->isMasterTrack()) members.add (t);
            auto f = session.addFolder ({}, r == 5000, members);
            if (f != nullptr && r == 5003) { if (auto vca = f->getVCAPlugin()) vca->deleteFromParent(); f->state.setProperty ("sqmKind", "basicFolder", nullptr); f->setName ("Folder " + juce::String (session.getFolderTracks().size())); }
            changed(); return;
        }
        if (r == 5002) { e.moveTrack (&track, te::TrackInsertPoint (nullptr, track.getParentFolderTrack())); changed(); return; }
        if (folderMapForMenu.count (r)) { e.moveTrack (&track, te::TrackInsertPoint (folderMapForMenu.at (r), nullptr)); changed(); return; }
        if (PluginMenu::isSearch (r))
        {
            auto sp = juce::Component::SafePointer<juce::Component> (this);
            PluginMenu::showSearch (session.getEngine(), PluginMenu::searchFilter (r), audio == nullptr || IO::formatOf (*audio) == "stereo",
                                    [this, sp] (int id) { if (sp != nullptr) handleMenu (id); });
            return;
        }
        if (r == 6000) { saveTrackPreset(); return; }
        if (r > 6000 && r - 6001 < presetFilesForMenu.size()) { loadTrackPreset (presetFilesForMenu[r - 6001]); changed(); return; }
        if (auto p = PluginMenu::createFromResult (session, r))
        {
            if (audio)
            {
                audio->pluginList.insertPlugin (p, p->isSynth() ? 0 : Session::insertEndIndex (audio->pluginList, audio), nullptr);
                p->showWindowExplicitly();
            }
            changed();
        }
    }

    void editComments()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Comments", "Notes for " + (master ? TrackOps::masterName (session) : track.getName()) + " (shown in View > Edit Window Views > Comments)", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("c", master ? session.edit->state.getProperty ("sqmMasterComments").toString() : TrackOps::comments (track));
        aw->getTextEditor ("c")->setMultiLine (true);
        aw->getTextEditor ("c")->setSize (360, 80);
        aw->addButton ("OK", 1); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        te::Track::Ptr keep (&track);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, keep] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            if (master) session.edit->state.setProperty ("sqmMasterComments", aw->getTextEditorContents ("c"), nullptr);
            else TrackOps::setComments (track, aw->getTextEditorContents ("c"));
            refresh();
        }));
    }

    void duplicateDialog()
    {
        TrackOps::duplicateDialog (session, targets(), [this] { if (onStructureChanged) onStructureChanged(); });
    }

    void pickCustomColour()
    {
        auto cs = std::make_unique<juce::ColourSelector> (juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders | juce::ColourSelector::showColourAtTop);
        cs->setCurrentColour (track.getColour());
        cs->setSize (300, 360);
        auto* raw = cs.get();
        te::Track::Ptr tp (&track);
        struct L : public juce::ChangeListener { te::Track::Ptr t; void changeListenerCallback (juce::ChangeBroadcaster* b) override { if (auto c = dynamic_cast<juce::ColourSelector*> (b)) t->setColour (c->getCurrentColour()); } };
        auto listener = std::make_shared<L>(); listener->t = tp;
        raw->addChangeListener (listener.get());
        auto& box = juce::CallOutBox::launchAsynchronously (std::move (cs), getScreenBounds(), nullptr);
        box.setDismissalMouseClicksAreAlwaysConsumed (true);
        // keep the listener alive as long as the box
        struct Holder : public juce::ComponentListener { std::shared_ptr<L> l; void componentBeingDeleted (juce::Component&) override { delete this; } };
        auto h = new Holder(); h->l = listener; box.addComponentListener (h);
    }

    void createGroupDialog()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("New Group", "Name the group. The selected tracks (and this one) become members.", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "Group " + session.nextGroupId().toUpperCase());
        aw->addComboBox ("attrs", { "Volume + Mute + Solo", "Volume only", "Mute + Solo only", "Everything (incl. arm, colour)" }, "Link");
        aw->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            Session::Group g;
            g.id = session.nextGroupId(); g.name = aw->getTextEditorContents ("name");
            int attrs = aw->getComboBoxComponent ("attrs")->getSelectedItemIndex();
            g.linkVolume = attrs != 2; g.linkMute = attrs != 1; g.linkSolo = attrs != 1; g.linkArm = attrs == 3; g.linkColour = attrs == 3;
            for (auto t : session.getRows()) if (state.isTrackSelected (t)) g.tracks.push_back (t->itemID);
            if (std::find (g.tracks.begin(), g.tracks.end(), track.itemID) == g.tracks.end()) g.tracks.push_back (track.itemID);
            session.setGroup (g);
            repaint();
        }));
    }

    void saveTrackPreset()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Save Track Preset", "Saves this track's inserts, sends, colour, I/O and settings (no clips) for reuse in any session.", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", track.getName());
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            auto st = track.state.createCopy();
            for (int i = st.getNumChildren(); --i >= 0;)
                if (st.getChild (i).hasType (te::IDs::MIDICLIP) || st.getChild (i).hasType (te::IDs::AUDIOCLIP) || st.getChild (i).hasType (te::IDs::STEPCLIP) || st.getChild (i).hasType (te::IDs::TRACK))
                    st.removeChild (i, nullptr);
            auto dir = session.sessionsDir.getChildFile ("Track Presets");
            dir.createDirectory();
            auto f = dir.getChildFile (juce::File::createLegalFileName (aw->getTextEditorContents ("name")) + ".sqmtrack");
            if (auto xml = st.createXml()) xml->writeTo (f);
            session.log.add ("You", "Saved track preset " + f.getFileNameWithoutExtension());
        }));
    }

    void loadTrackPreset (const juce::File& f)
    {
        if (auto xml = juce::XmlDocument::parse (f))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            if (! st.isValid()) return;
            te::EditItemID::remapIDs (st, nullptr, track.edit);
            auto newTrack = track.edit.insertTrack (te::TrackInsertPoint (track.getParentFolderTrack(), &track), st, nullptr);
            if (newTrack != nullptr) newTrack->setName (f.getFileNameWithoutExtension());
        }
    }
};
