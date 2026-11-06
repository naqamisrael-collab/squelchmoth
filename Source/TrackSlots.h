#pragma once
// Squelchmoth - Pro Tools insert and send slots (A-E and F-J), shared by the Mix window channel strips
// and the Edit window's Inserts / Sends views. A SlotController owns the menus: add / remove / bypass /
// replace / move / copy / paste / preset for inserts; bus, level, pre/post, mute, polarity for sends.

#include "Behaviours.h"
#include "PluginMenu.h"
#include "IORouting.h"
#include "InsertChains.h"

class SlotController
{
public:
    SlotController (Session& s, te::Track* t)
        : session (s), row (t), track (dynamic_cast<te::AudioTrack*> (t)), folder (dynamic_cast<te::FolderTrack*> (t)) {}
    virtual ~SlotController() { *aliveFlag = false; }

    /** Turns false when this controller is destroyed, so async callbacks (plug-in search) never touch a dead strip. */
    std::shared_ptr<bool> aliveFlag = std::make_shared<bool> (true);

    /** Called after any change so the owner can rebuild its slots. */
    virtual void slotsChanged() = 0;

    Session& session;
    te::Track* row;                 // nullptr = the Master Fader
    te::AudioTrack* track;
    te::FolderTrack* folder;
    juce::Array<juce::File> chainFiles;   // Insert chain menu

    bool hasInserts() const             { return track != nullptr || row == nullptr || (folder != nullptr && folder->isSubmixFolder()); }
    te::PluginList& getPluginList()     { return row != nullptr ? row->pluginList : session.edit->getMasterPluginList(); }
    juce::String ownerName() const      { return row != nullptr ? row->getName() : juce::String ("Master"); }

    static juce::String slotLetter (int i)   { return juce::String::charToString ((juce::juce_wchar) ('A' + i)); }

    //==========================================================================
    void insertPluginAt (int slotIndex, te::Plugin::Ptr p, te::Plugin::Ptr replacing)
    {
        if (p == nullptr) return;
        auto& list = getPluginList();
        int idx;
        if (replacing != nullptr) { idx = list.indexOf (replacing.get()); replacing->deleteFromParent(); }
        else
        {
            auto inserts = Session::getInserts (list, track);
            if (slotIndex < inserts.size()) idx = list.indexOf (inserts[slotIndex]);
            else if (row != nullptr) idx = p->isSynth() && track != nullptr ? 0 : Session::insertEndIndex (list, row);
            else idx = -1;
        }
        list.insertPlugin (p, idx, nullptr);
        session.log.add ("You", "Inserted " + Signature::displayName (*p) + " on " + ownerName() + " (insert " + slotLetter (slotIndex) + ")");
        if (Prefs::getBool ("pl_openWindowOnInsert", true)) p->showWindowExplicitly();
        slotsChanged();
    }

    void showPluginChooser (juce::Component* target, int slotIndex, te::Plugin::Ptr replacing)
    {
        const bool stereoTarget = track == nullptr || IO::formatOf (*track) == "stereo";
        auto m = PluginMenu::build (session.getEngine(), track != nullptr && Session::getInserts (getPluginList(), track).isEmpty() ? PluginMenu::all : PluginMenu::effects, stereoTarget);
        juce::PopupMenu full;
        full.addItem (999999, "no insert", replacing != nullptr, replacing == nullptr);
        full.addSeparator();
        full.addSubMenu ("plug-in", m);
        full.addSeparator();
        if (copiedInsert().isValid()) full.addItem (999998, "Paste insert (" + copiedInsert().getProperty (te::IDs::type).toString() + ")");
        full.addSeparator();
        full.addSubMenu ("Insert chain", InsertChains::menu (chainFiles));
        full.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this, slotIndex, replacing, stereoTarget] (int r)
        {
            if (r == 0) return;
            if (r == 999999) { if (replacing) replacing->deleteFromParent(); slotsChanged(); return; }
            if (r == 999998) { pasteInsert (slotIndex, replacing); return; }
            if (InsertChains::handle (session, row, r, chainFiles, [this] { slotsChanged(); })) return;
            auto alive = aliveFlag;
            PluginMenu::handle (session.getEngine(), r, stereoTarget, [this, alive, slotIndex, replacing] (int id)
            {
                if (*alive) insertPluginAt (slotIndex, PluginMenu::createFromResult (session, id), replacing);
            });
        });
    }

    static juce::ValueTree& copiedInsert()   { static juce::ValueTree v; return v; }

    void pasteInsert (int slotIndex, te::Plugin::Ptr replacing)
    {
        auto v = copiedInsert().createCopy();
        if (! v.isValid()) return;
        v.removeProperty (te::IDs::id, nullptr);
        if (auto p = session.edit->getPluginCache().createNewPlugin (v))
            insertPluginAt (slotIndex, p, replacing);
    }

    void showInsertOptions (juce::Component* target, int slotIndex, te::Plugin::Ptr p)
    {
        if (p == nullptr) { showPluginChooser (target, slotIndex, nullptr); return; }
        juce::PopupMenu m;
        m.addItem (1, "no insert (remove)");
        m.addSeparator();
        m.addItem (2, "Open plug-in window");
        m.addItem (3, "Bypass", true, ! p->isEnabled());
        m.addItem (4, "Replace with...");
        m.addSeparator();
        m.addItem (5, "Move up", slotIndex > 0);
        m.addItem (6, "Move down");
        m.addSeparator();
        m.addItem (7, "Copy insert");
        m.addItem (8, "Paste insert here (replace)", copiedInsert().isValid());
        m.addSeparator();
        m.addItem (9, "Save as preset...");
        m.addSeparator();
        m.addSubMenu ("Insert chain (all inserts)", InsertChains::menu (chainFiles));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this, target, slotIndex, p] (int r)
        {
            if (InsertChains::handle (session, row, r, chainFiles, [this] { slotsChanged(); })) return;
            if (r == 1) { p->deleteFromParent(); session.log.add ("You", "Removed " + Signature::displayName (*p) + " from " + ownerName()); }
            else if (r == 2) p->showWindowExplicitly();
            else if (r == 3) p->setEnabled (! p->isEnabled());
            else if (r == 4) { showPluginChooser (target, slotIndex, p); return; }
            else if (r == 5 || r == 6)
            {
                auto inserts = Session::getInserts (getPluginList(), track);
                auto i = inserts.indexOf (p.get());
                auto j = r == 5 ? i - 1 : i + 1;
                if (j >= 0 && j < inserts.size())
                {
                    auto parent = p->state.getParent();
                    parent.moveChild (parent.indexOf (p->state), parent.indexOf (inserts[j]->state), &session.edit->getUndoManager());
                }
            }
            else if (r == 7) { session.edit->flushPluginStateIfNeeded (*p); copiedInsert() = p->state.createCopy(); }
            else if (r == 8) { pasteInsert (slotIndex, p); return; }
            else if (r == 9) savePluginPreset (p);
            slotsChanged();
        });
    }

    void savePluginPreset (te::Plugin::Ptr p)
    {
        auto dir = session.sessionsDir.getChildFile ("Plugin Presets").getChildFile (Signature::displayName (*p));
        dir.createDirectory();
        auto w = std::make_shared<juce::AlertWindow> ("Save preset", "Preset name for " + Signature::displayName (*p), juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", "My preset");
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        auto& sess = session;
        w->enterModalState (true, juce::ModalCallbackFunction::create ([&sess, w, p, dir] (int r)
        {
            w->setVisible (false);
            if (r != 1) return;
            sess.edit->flushPluginStateIfNeeded (*p);
            auto f = dir.getChildFile (juce::File::createLegalFileName (w->getTextEditorContents ("name")) + ".sqmpreset");
            if (auto xml = p->state.createXml()) xml->writeTo (f);
            sess.log.add ("You", "Saved preset " + f.getFileNameWithoutExtension());
        }), false);
    }

    //==========================================================================
    void addSend (int busNumber, int slotIndex)
    {
        if (track == nullptr) return;
        auto p = session.edit->getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
        if (p == nullptr) return;
        auto sends = Session::getSends (*track);
        int idx = slotIndex < sends.size() ? track->pluginList.indexOf (sends[slotIndex])
                                           : (Prefs::sendsPostFader() ? track->pluginList.indexOf (track->getLevelMeterPlugin()) : track->pluginList.indexOf (track->getVolumePlugin()));
        track->pluginList.insertPlugin (p, idx, nullptr);
        if (auto s = dynamic_cast<te::AuxSendPlugin*> (p.get())) { s->busNumber = busNumber; s->setGainDb ((float) Prefs::newSendDb()); }
        session.log.add ("You", "Send " + slotLetter (slotIndex) + " on " + track->getName() + " -> " + IO::busName (session, busNumber));
        slotsChanged();
    }

    void showSendChooser (juce::Component* target, int slotIndex, te::AuxSendPlugin* existing)
    {
        if (track == nullptr) return;
        juce::PopupMenu m;
        m.addItem (1, "no send", existing != nullptr, existing == nullptr);
        m.addSeparator();
        std::map<int, int> busMap;
        int id = 10;
        juce::PopupMenu busses;
        for (auto bn : IO::allBusses (session))
        {
            auto bt = session.findBusTrack (bn);
            if (bt == track) continue;
            busses.addItem (id, IO::busName (session, bn), true, existing != nullptr && existing->getBusNumber() == bn);
            busMap[id++] = bn;
        }
        m.addSubMenu ("bus", busses, busMap.size() > 0);
        m.addSeparator();
        m.addItem (2, "new track...");
        te::Plugin::Ptr keepExisting (existing);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this, slotIndex, existing, keepExisting, busMap] (int r)
        {
            if (r == 0) return;
            if (r == 1) { if (existing) existing->deleteFromParent(); slotsChanged(); return; }
            auto apply = [this, slotIndex, existing] (int bus)
            {
                if (bus < 0) return;
                if (existing != nullptr) { existing->busNumber = bus; slotsChanged(); }
                else addSend (bus, slotIndex);
            };
            if (r == 2)
            {
                IO::promptNewAux (session, track->getName() + " FX", [this, apply] (te::AudioTrack* bt) { if (bt) apply (session.getBusNumberOfTrack (*bt)); });
                return;
            }
            if (busMap.count (r)) apply (busMap.at (r));
        });
    }

    void showSendOptions (juce::Component* target, int slotIndex, te::AuxSendPlugin* s)
    {
        if (s == nullptr) { showSendChooser (target, slotIndex, nullptr); return; }
        bool post = isSendPostFader (*s);
        juce::PopupMenu m;
        m.addItem (1, "no send (remove)");
        m.addSeparator();
        m.addItem (2, "Mute send", true, ! s->isEnabled());
        m.addItem (3, "Pre-fader", true, ! post);
        m.addItem (4, "Post-fader", true, post);
        m.addItem (5, "Invert polarity", true, s->invertPhase.get());
        m.addSeparator();
        m.addItem (6, "Change bus...");
        m.addItem (7, "Set level to 0 dB");
        m.addItem (8, "Set level to -inf");
        m.addItem (9, "Select the bus's Aux track");
        te::Plugin::Ptr keep (s);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this, target, slotIndex, s, keep] (int r)
        {
            if (r == 1) s->deleteFromParent();
            else if (r == 2) s->setEnabled (! s->isEnabled());
            else if (r == 3 || r == 4) setSendPostFader (*s, r == 4);
            else if (r == 5) s->invertPhase = ! s->invertPhase.get();
            else if (r == 6) { showSendChooser (target, slotIndex, s); return; }
            else if (r == 7) s->setGainDb (0.0f);
            else if (r == 8) s->setGainDb (-100.0f);
            else if (r == 9) { if (onSelectTrack) if (auto bt = session.findBusTrack (s->getBusNumber())) onSelectTrack (bt); }
            slotsChanged();
        });
    }

    std::function<void (te::Track*)> onSelectTrack;

    bool isSendPostFader (te::AuxSendPlugin& s)
    {
        if (track == nullptr) return true;
        auto& list = track->pluginList;
        return list.indexOf (&s) > list.indexOf (track->getVolumePlugin());
    }

    void setSendPostFader (te::AuxSendPlugin& s, bool post)
    {
        if (track == nullptr) return;
        auto parent = s.state.getParent();
        auto volIdx = parent.indexOf (track->getVolumePlugin()->state);
        auto meterIdx = parent.indexOf (track->getLevelMeterPlugin()->state);
        auto cur = parent.indexOf (s.state);
        int dest = post ? meterIdx : volIdx;
        if (post && cur > volIdx) return;
        if (! post && cur < volIdx) return;
        parent.moveChild (cur, cur < dest ? dest - 1 : dest, &session.edit->getUndoManager());
    }
};

//==============================================================================
struct InsertSlot : public juce::Component, public juce::SettableTooltipClient
{
    InsertSlot (SlotController& o, int idx, te::Plugin::Ptr p, bool compactMode = false) : owner (o), index (idx), plugin (p), compact (compactMode) { refresh(); }
    void refresh()
    {
        if (plugin != nullptr)
            setTooltip ("Insert " + SlotController::slotLetter (index) + ": " + Signature::displayName (*plugin) + (plugin->isEnabled() ? "" : " (bypassed)") + "\nclick: open window   |   arrow / right-click: remove, bypass, move, replace, copy   |   double-click: bypass");
        else
            setTooltip ("Insert " + SlotController::slotLetter (index) + " - click to add a plug-in");
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        bool on = plugin != nullptr && plugin->isEnabled();
        auto fill = plugin == nullptr ? Theme::bg().withAlpha (0.6f) : (on ? Theme::violet().withAlpha (0.55f) : Theme::panel2());
        g.setColour (fill);
        g.fillRoundedRectangle (r, compact ? 2.0f : 3.0f);
        if (plugin != nullptr && ! on) { g.setColour (Theme::amber().withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
        g.setFont (Theme::font (compact ? juce::jmin (10.0f, getHeight() * 0.8f) : 10.0f, plugin != nullptr));
        g.setColour (plugin == nullptr ? Theme::dim().withAlpha (0.6f) : (on ? Theme::text() : Theme::dim()));
        auto nameArea = getLocalBounds().withTrimmedRight (12).withTrimmedLeft (4);
        g.drawText (plugin != nullptr ? Signature::displayName (*plugin) : SlotController::slotLetter (index), nameArea, juce::Justification::centredLeft);
        if (getHeight() >= 9)
        {
            g.setColour (Theme::dim());
            juce::Path tri;
            auto cy = getHeight() * 0.5f;
            tri.addTriangle ((float) getWidth() - 9.0f, cy - 2.0f, (float) getWidth() - 3.0f, cy - 2.0f, (float) getWidth() - 6.0f, cy + 2.0f);
            g.fillPath (tri);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        bool arrow = e.x >= getWidth() - 13;
        if (e.mods.isPopupMenu() || arrow || plugin == nullptr) owner.showInsertOptions (this, index, plugin);
        else plugin->showWindowExplicitly();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override   { if (plugin) { plugin->setEnabled (! plugin->isEnabled()); refresh(); } }
    SlotController& owner;
    int index;
    te::Plugin::Ptr plugin;
    bool compact;
};

//==============================================================================
struct SendSlot : public juce::Component, public juce::SettableTooltipClient
{
    SendSlot (SlotController& o, int idx, te::AuxSendPlugin* s, bool compactMode = false) : owner (o), index (idx), send (s), keep (s), compact (compactMode)
    {
        level.setSliderStyle (juce::Slider::LinearBar);
        level.setRange (-60.0, 12.0, 0.1);
        level.setSkewFactorFromMidPoint (-12.0);
        level.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        level.setDoubleClickReturnValue (true, 0.0);
        level.setColour (juce::Slider::trackColourId, Theme::mint().withAlpha (0.6f));
        level.setPopupDisplayEnabled (true, false, this);
        level.textFromValueFunction = [] (double v) { return v <= -59.9 ? juce::String ("-inf") : juce::String (v, 1) + " dB"; };
        level.onValueChange = [this] { if (send) send->setGainDb ((float) level.getValue()); };
        addChildComponent (level);
        level.setVisible (send != nullptr && ! compact);
        refresh();
    }
    void refresh()
    {
        if (send != nullptr)
        {
            busName = IO::busName (owner.session, send->getBusNumber());
            if (! level.isMouseButtonDown()) level.setValue (send->getGainDb(), juce::dontSendNotification);
            post = owner.isSendPostFader (*send);
            setTooltip ("Send " + SlotController::slotLetter (index) + " > " + busName + "  (" + juce::String (send->getGainDb(), 1) + " dB, " + (post ? "post" : "pre") + "-fader" + (send->isEnabled() ? "" : ", muted") + ")\nclick: options" + (compact ? juce::String() : juce::String ("   |   drag the bar: level   |   double-click bar: 0 dB")));
        }
        else setTooltip ("Send " + SlotController::slotLetter (index) + " - click to add a send");
        repaint();
    }
    void resized() override   { level.setBounds (getLocalBounds().removeFromBottom (6).reduced (2, 0)); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (send == nullptr ? Theme::bg().withAlpha (0.6f) : (send->isEnabled() ? Theme::mint().withAlpha (0.18f) : Theme::panel2()));
        g.fillRoundedRectangle (r, compact ? 2.0f : 3.0f);
        g.setFont (Theme::font (compact ? juce::jmin (10.0f, getHeight() * 0.8f) : 10.0f, send != nullptr));
        g.setColour (send == nullptr ? Theme::dim().withAlpha (0.6f) : (send->isEnabled() ? Theme::text() : Theme::dim()));
        auto textH = compact ? getHeight() : getHeight() - 7;
        auto top = getLocalBounds().removeFromTop (textH).withTrimmedLeft (4).withTrimmedRight (26);
        g.drawText (send != nullptr ? busName : SlotController::slotLetter (index), top, juce::Justification::centredLeft);
        if (send != nullptr)
        {
            g.setFont (Theme::font (8.0f, true));
            g.setColour (post ? Theme::dim() : Theme::amber());
            g.drawText (post ? "post" : "PRE", getLocalBounds().removeFromTop (textH).withTrimmedRight (12).removeFromRight (22), juce::Justification::centredRight);
            if (! send->isEnabled()) { g.setColour (Theme::amber()); g.drawText ("M", getLocalBounds().removeFromTop (textH).removeFromRight (12), juce::Justification::centred); }
        }
        if (getHeight() >= 9)
        {
            g.setColour (Theme::dim());
            juce::Path tri;
            auto cy = juce::jmin (8.5f, textH * 0.5f);
            tri.addTriangle ((float) getWidth() - 9.0f, cy - 2.0f, (float) getWidth() - 3.0f, cy - 2.0f, (float) getWidth() - 6.0f, cy + 2.0f);
            if (send == nullptr || send->isEnabled()) g.fillPath (tri);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! compact && e.y >= getHeight() - 8 && send != nullptr) return;   // the level bar handles itself
        owner.showSendOptions (this, index, send);
    }
    SlotController& owner;
    int index;
    te::AuxSendPlugin* send;
    te::Plugin::Ptr keep;
    juce::Slider level;
    juce::String busName;
    bool post = true, compact;
};
