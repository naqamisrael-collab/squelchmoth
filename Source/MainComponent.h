#pragma once
// Squelchmoth - main window content: transport bar + Edit / Mix / Claude views, the Pro Tools menu
// structure (File, Edit, View, Track, Clip, Event, AudioSuite, Options, Setup, Window, Claude, Help),
// floating windows, global keyboard shortcuts, session templates and the welcome window.

#include "TransportBar.h"
#include "TimelineView.h"
#include "MixerView.h"
#include "PianoRoll.h"
#include "StepGrid.h"
#include "ChannelRack.h"
#include "BeatPage.h"
#include "ClaudePanel.h"
#include "Licence.h"
#include "SqWindows.h"
#include "Performance.h"
#include "OutputWindow.h"
#include "IOSetup.h"
#include "NewTracks.h"
#include "QuickNew.h"
#include "EQChart.h"
#include "Mastering.h"
#include "TuneGraph.h"
#include "AudioEditor.h"
#include "FactoryLoops.h"
#include "UnlockWindow.h"
#include "Molt.h"
#include "AutoSave.h"
#include "MixSnapshots.h"
#include "BounceExport.h"

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent (Session& s, McpTools& t, McpServer& srv, Tunnel& tun, PluginScanner& sc, juce::PropertiesFile& p, Licence& lic)
        : session (s), tools (t), server (srv), tunnel (tun), scanner (sc), props (p), licence (lic),
          timeline (s), transportCtl (s, timeline.state), transport (s, transportCtl), mixer (s, &timeline.state), claude (s, srv, tun, p), beat (s), mastering (s), splitDivider (true)
    {
        setLookAndFeel (&lnf);
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

        addAndMakeVisible (transport);
        addChildComponent (timeline);
        addChildComponent (mixer);
        addChildComponent (claude);
        addChildComponent (beat);
        addChildComponent (splitDivider);
        beat.onSelectTrack = [this] (te::AudioTrack& t) { timeline.state.selectTrack (&t, false); timeline.repaint(); mixer.repaint(); };
        splitDivider.onDrag = [this] (int d) { splitRatio = juce::jlimit (0.15f, 0.85f, splitRatio + (float) d / (float) juce::jmax (1, layoutMode == "side" ? getWidth() : getHeight())); Prefs::set ("layout_splitRatio", splitRatio); resized(); };
        layoutMode = Prefs::getString ("layout_mode", "single"); splitRatio = (float) Prefs::getDouble ("layout_splitRatio", 0.6);
        // torn-off pages come back the way they were left (survives a relaunch and a session reload)
        juce::Timer::callAfterDelay (600, [sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (sp == nullptr) return;
            for (auto& k : { "mix", "beat", "claude" }) if (Prefs::getBool (("layout_torn_" + juce::String (k)).toRawUTF8(), false)) sp->tearOff (k);
        });
        addAndMakeVisible (status);
        status.setFont (Theme::font (12.0f));
        status.setColour (juce::Label::textColourId, Theme::dim());

        transport.onShowView = [this] (const juce::String& v) { showView (v); };
        claude.onStatus = [this] (const juce::String& st, bool live) { transport.setClaudeStatus (st, live); };
        timeline.onOpenMidiClip = [this] (te::MidiClip& c) { openPianoRoll (c); };
        timeline.onOpenStepClip = [this] (te::StepClip& c) { openStepGrid (c); };

        tools.ui.snapshot = [this] (const juce::String& view)
        {
            auto prev = currentView;
            if (view != "current" && view.isNotEmpty()) showView (view);
            auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
            if (prev != currentView) showView (prev);
            return img;
        };
        tools.ui.showView = [this] (const juce::String& v) { showView (v); };
        tools.ui.zoomTimeline = [this] (double bars, double first) { timeline.zoomTo (bars, first); };
        tools.ui.state = &timeline.state;
        tools.ui.transport = &transportCtl;
        tools.ui.setEditMode = [this] (int m) { timeline.setEditMode ((EditMode) juce::jlimit (0, 3, m)); };
        tools.ui.fitHeights = [this] (juce::Array<te::Track*> ts) { timeline.fitHeights (ts); };
        tools.ui.setTool = [this] (int tl) { timeline.setTool ((EditTool) juce::jlimit (0, 6, tl)); };
        tools.ui.showWindow = [this] (const juce::String& w, bool show) { if (w == "beat") showView ("beat"); else toggleWindow (w, show); };
        tools.ui.onRackChanged = [this] { refreshRack(); };
        Skins::loadKitHook = [this] (int kit) { return DrumKit::ensureKit (session, kit); };
        SqHivePadsPlugin::kitFiles = [this] (int kit) { return DrumKit::ensureKit (session, kit); };
        FactoryLoops::install();   // Mandible's genre loops render from the kits on first use
        tools.ui.mastering = [this] { return &mastering; };
        tools.ui.refresh = [this] { timeline.repaint(); mixer.repaint(); };
        tools.ui.menuTree = [this] { return menuTree(); };
        tools.ui.menuCommand = [this] (const juce::String& path) { return runMenuPath (path); };
        tools.ui.openClipEditor = [this] (te::Clip& c)
        {
            if (auto sc = dynamic_cast<te::StepClip*> (&c)) openStepGrid (*sc);
            else if (auto mc = dynamic_cast<te::MidiClip*> (&c)) openPianoRoll (*mc);
        };
        tools.ui.openTuneGraph = [this] (te::Plugin& p) { openTuneGraph (p); };
        tools.ui.openCutLab = [this] (const juce::File& f) -> void* { return openCutLabGated (f); };
        LaneCanvas::openInCutLab = [this] (te::Clip& c) { if (Edition::allows (Edition::Feature::cutLab)) openCutLabForClip (c); else showUnlock ("CORE"); };
        Edition::showUnlock() = [this] (const juce::String& code) { showUnlock (code); };
        Skins::Hooks::openTuneGraph = [this] (te::Plugin& p) { openTuneGraph (p); };
        Skins::Hooks::slicerDump = [this] (te::Plugin& p, int deck) { slicerDumpToPianoRoll (p, deck); };
        Skins::Hooks::slicerToChannels = [this] (te::Plugin& p, int deck) { slicerToRackChannels (p, deck); };
        Skins::Hooks::detectKey = [this] (te::Plugin& p)
        {
            auto tu = dynamic_cast<SqTunePlugin*> (&p);
            auto k = TuneGraph::detectKey (p.edit, tu);
            TuneGraph::applyKey (p.edit, tu, k);
            session.log.add ("You", "Detected key: " + SqScales::noteNames()[k.root] + (k.minor ? " minor" : " major") + " (set on Moth Tune and as the session key)");
        };
        tools.ui.listWindows = [this]
        {
            juce::StringArray open;
            for (auto& w : tuneGraphs) if (w->isVisible()) open.add ("moth tune graph");
            if (cutLab != nullptr && cutLab->isVisible()) open.add ("cut lab");
            for (auto& [n, w] : windows) if (w->isVisible()) open.add (n);
            for (auto& w : pianoRolls) if (w->isVisible()) open.add ("midi editor");
            for (auto& w : stepGrids) if (w->isVisible()) open.add ("beat maker");
            return open;
        };

        scanner.onFinished = [this]
        {
            session.log.add ("Squelchmoth", "Plugin scan finished: " + scanner.getStatus());
            menuItemsChanged();
        };

        session.addChangeListener (this);
        Theme::broadcaster().addChangeListener (this);
        session.getEngine().getDeviceManager().addChangeListener (this);
        timeline.onNewTracks = [this] { newTracksDialog(); };
        Perf::apply (session.getEngine(), session.edit.get(), true);
        juce::Timer::callAfterDelay (2500, [this, sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp) findMissingPlugins(); });
        perfMonitor.onStatus = [this] (const juce::String& s)
        {
            perfStatusText = s; perfStatusTime = juce::Time::currentTimeMillis();
            session.log.add ("Squelchmoth", s, false);
        };
        transport.onCpuClicked = [this] { toggleWindow ("usage", true); };
        tools.ui.perfReset = [this] { perfMonitor.reset(); };
        tools.ui.perfStats = [this]
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("load_percent", juce::roundToInt (perfMonitor.load * 100));
            o->setProperty ("peak_percent", juce::roundToInt (juce::jmin (1.0f, perfMonitor.peakLoad) * 100));
            o->setProperty ("session_peak_percent", juce::roundToInt (perfMonitor.sessionPeak * 100));
            o->setProperty ("overloads", perfMonitor.overloads);
            o->setProperty ("interface_dropouts", perfMonitor.dropouts);
            o->setProperty ("automatic_buffer_raises", perfMonitor.bufferRaises);
            o->setProperty ("last_overload", perfMonitor.lastEvent);
            return juce::var (o);
        };
        session.showNewTracksHook = [this] { newTracksDialog(); };
        session.quickNewHook = [this] (int id, int base) { runQuickNew (id, base); };
        session.showOutputWindowHook = [this] (te::Track* t) { showOutputWindow (t); };
        session.defaultInputAssigner = [this] (te::AudioTrack& t, int index, bool stereo) { IO::assignDefaultInput (session, t, index, stereo); };
        showView ("edit");
        juce::Timer::callAfterDelay (400, [this, sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (sp == nullptr) return;
            IO::restoreInputs (session);
            TrackOps::setInputOnlyMonitoring (session, TrackOps::inputOnlyMonitoring());   // also clears device-wide "on" left by older builds
        });
        setWantsKeyboardFocus (true);
        startTimerHz (2);

       #if JUCE_MAC
        // The native Mac menu bar (like Pro Tools)...
        juce::MenuBarModel::setMacMainMenu (this);
       #endif
        // ...plus the same menus inside the window. On macOS this is a backup that can be hidden from
        // View > Menu Bar inside window; on Windows/Linux it is the only menu bar.
        menuBar = std::make_unique<juce::MenuBarComponent> (this);
        addChildComponent (*menuBar);
        menuBar->setVisible (inWindowMenuBarWanted());
        setSize (1500, 900);

        if (Prefs::showWelcome())
            juce::Timer::callAfterDelay (600, [this] { toggleWindow ("welcome", true); });
    }

    ~MainComponent() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #endif
        session.showNewTracksHook = nullptr;
        session.quickNewHook = nullptr;
        session.showOutputWindowHook = nullptr;
        tools.ui.perfStats = nullptr;
        tools.ui.perfReset = nullptr;
        tools.ui.openClipEditor = nullptr;
        tools.ui.openTuneGraph = nullptr;
        LaneCanvas::openInCutLab = nullptr; Skins::Hooks::openTuneGraph = nullptr; Skins::Hooks::detectKey = nullptr; Skins::Hooks::slicerDump = nullptr; Skins::Hooks::slicerToChannels = nullptr;
        tuneGraphs.clear();
        moltWindow.reset();
        session.removeChangeListener (this);
        Theme::broadcaster().removeChangeListener (this);
        session.getEngine().getDeviceManager().removeChangeListener (this);
        windows.clear();
        pianoRolls.clear();
        stepGrids.clear();
        setLookAndFeel (nullptr);
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    //==========================================================================
    /** Help > Licences and Upgrades, and every lock in the app: the product cards, prices, Buy, and the key field. */
    void showLicenceDialog (bool = false)   { showUnlock ({}); }
    void showUnlock (const juce::String& productCode)
    {
        toggleWindow ("unlock", true);
        if (auto w = findWindow ("unlock")) if (auto c = dynamic_cast<UnlockContent*> (w->getContentComponent())) c->focusProduct (productCode);
    }
    /** Save, then reopen the current session so placeholders become the plug-ins that were just unlocked. */
    void reopenSessionForUnlock()
    {
        auto f = session.getFile();
        if (! f.existsAsFile()) { session.log.add ("Squelchmoth", "Unlocked - new plug-ins are ready"); return; }
        session.save();
        if (session.openSession (f)) session.log.add ("Squelchmoth", "Reopened " + f.getFileNameWithoutExtension() + " - unlocked plug-ins are live");
    }

    //==========================================================================
    // Pages and layouts: EDIT · MIX · BEAT · CLAUDE, Edit + Mix split, torn-off pages, five layout slots
    juce::String layoutMode = "single";   // single | stacked | side
    float splitRatio = 0.6f;
    SplitDivider splitDivider;
    std::map<juce::String, std::unique_ptr<SqWindow>> tornWindows;   // "mix" / "beat" / "claude" in their own windows

    juce::Component& pageComponent (const juce::String& v)   { return v == "mix" ? (juce::Component&) mixer : v == "beat" ? (juce::Component&) beat : v == "claude" ? (juce::Component&) claude : (juce::Component&) timeline; }
    bool isTornOff (const juce::String& v) const   { return tornWindows.count (v) > 0; }

    void showView (const juce::String& v)
    {
        if (isTornOff (v)) { if (auto& w = tornWindows[v]) { w->showAndRestore (this); } return; }
        if (v == "beat" && Rack::channels (*session.edit).isEmpty() && Prefs::getBool ("beat_autoKit", true) && session.edit != nullptr) { /* an empty rack opens empty; Make a Beat fills it */ }
        currentView = v;
        transport.setCurrentView (v);
        resized();
        if (v == "edit") timeline.grabKeyboardFocus();
        if (v == "beat")
        {
            // the Channel Rack is its own window and opens with the page (FL style); it follows the page away again
            if (! isWindowVisible ("rack")) rackOpenedByPage = true;
            toggleWindow ("rack", true);
            if (rackWindowComp) { beat.setRack (rackWindowComp); rackWindowComp->refresh(); rackWindowComp->grabKeyboardFocus(); }
        }
        else if (rackOpenedByPage && isWindowVisible ("rack")) { toggleWindow ("rack", false); rackOpenedByPage = false; }
    }
    bool rackOpenedByPage = false;

    void cyclePages()
    {
        juce::StringArray order { "edit", "mix", "beat" };
        order.removeString (isTornOff ("mix") ? "mix" : ""); order.removeString (isTornOff ("beat") ? "beat" : "");
        const int i = order.indexOf (currentView);
        showView (order[(i + 1) % order.size()]);
    }

    void setLayoutMode (const juce::String& mode)
    {
        layoutMode = mode; Prefs::set ("layout_mode", mode);
        if (mode != "single" && isTornOff ("mix")) dockPage ("mix");
        if (mode != "single" && currentView != "edit" && currentView != "mix") currentView = "edit";
        resized(); menuItemsChanged();
    }

    void tearOff (const juce::String& v)
    {
        if (v == "edit" || isTornOff (v)) return;
        auto& comp = pageComponent (v);
        removeChildComponent (&comp);
        auto w = std::make_unique<SqWindow> (v == "mix" ? "Mix - Squelchmoth" : v == "beat" ? "Beat - Squelchmoth" : "Claude - Squelchmoth", "torn_" + v);
        w->setContentNonOwned (&comp, false);
        w->setFloating (false);
        w->setResizable (true, false);
        comp.setVisible (true);
        w->setSize (juce::jmax (900, getWidth() - 100), juce::jmax (600, getHeight() - 100));
        w->onHidden = [this, v] { juce::MessageManager::callAsync ([this, v] { dockPage (v); }); };
        auto* raw = w.get();
        tornWindows[v] = std::move (w);
        raw->showAndRestore (this);
        if (layoutMode != "single" && v == "mix") layoutMode = "single";
        if (currentView == v) currentView = "edit";
        transport.setTabVisible (v, false);
        resized(); menuItemsChanged();
        Prefs::set (("layout_torn_" + v).toRawUTF8(), true);
    }

    void dockPage (const juce::String& v)
    {
        auto it = tornWindows.find (v); if (it == tornWindows.end()) return;
        auto& comp = pageComponent (v);
        it->second->setContentNonOwned (nullptr, false);
        it->second->setVisible (false);
        tornWindows.erase (it);
        comp.setTransform ({});
        addChildComponent (comp);
        transport.setTabVisible (v, true);
        Prefs::set (("layout_torn_" + v).toRawUTF8(), false);
        resized(); menuItemsChanged();
    }

    //==========================================================================
    // five layout slots + presets
    juce::var captureLayout()
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("mode", layoutMode); o->setProperty ("ratio", splitRatio); o->setProperty ("view", currentView);
        juce::Array<juce::var> torn; for (auto& [k, w] : tornWindows) { auto t = new juce::DynamicObject(); t->setProperty ("page", k); t->setProperty ("bounds", w->getBounds().toString()); torn.add (juce::var (t)); }
        o->setProperty ("torn", torn);
        juce::Array<juce::var> wins; for (auto& [n, w] : windows) if (w->isVisible()) { auto t = new juce::DynamicObject(); t->setProperty ("name", n); t->setProperty ("bounds", w->getBounds().toString()); wins.add (juce::var (t)); }
        o->setProperty ("windows", wins);
        if (auto* top = getTopLevelComponent()) o->setProperty ("main", top->getBounds().toString());
        o->setProperty ("beat_browser", beat.panelShown ("browser")); o->setProperty ("beat_side", beat.panelShown ("side")); o->setProperty ("beat_playlist", beat.panelShown ("playlist")); o->setProperty ("beat_mixer", beat.panelShown ("mixer"));
        return juce::var (o);
    }
    void applyLayout (const juce::var& v)
    {
        if (! v.isObject()) return;
        for (auto& k : { "mix", "beat", "claude" }) if (isTornOff (k)) dockPage (k);
        layoutMode = v.getProperty ("mode", "single").toString(); splitRatio = (float) (double) v.getProperty ("ratio", 0.6);
        Prefs::set ("layout_mode", layoutMode); Prefs::set ("layout_splitRatio", splitRatio);
        if (auto* torn = v.getProperty ("torn", juce::var()).getArray())
            for (auto& t : *torn) { auto page = t.getProperty ("page", "").toString(); tearOff (page); if (auto w = tornWindows.count (page) ? tornWindows[page].get() : nullptr) { auto b = juce::Rectangle<int>::fromString (t.getProperty ("bounds", "").toString()); if (b.getWidth() > 100) w->setBounds (b); } }
        for (auto& [n, w] : windows) if (w->isVisible()) w->closeButtonPressed();
        if (auto* wins = v.getProperty ("windows", juce::var()).getArray())
            for (auto& t : *wins) { auto n = t.getProperty ("name", "").toString(); toggleWindow (n, true); if (auto w = findWindow (n)) { auto b = juce::Rectangle<int>::fromString (t.getProperty ("bounds", "").toString()); if (b.getWidth() > 100) w->setBounds (b); } }
        beat.setPanel ("browser", v.getProperty ("beat_browser", true)); beat.setPanel ("side", v.getProperty ("beat_side", true)); beat.setPanel ("playlist", v.getProperty ("beat_playlist", true)); beat.setPanel ("mixer", v.getProperty ("beat_mixer", false));
        auto view = v.getProperty ("view", "edit").toString();
        showView (isTornOff (view) ? "edit" : view);
        menuItemsChanged();
    }
    void saveLayoutSlot (int slot)   { Prefs::set (("layout_slot_" + juce::String (slot)).toRawUTF8(), juce::JSON::toString (captureLayout(), true)); session.log.add ("You", "Layout saved to slot " + juce::String (slot)); }
    void recallLayoutSlot (int slot)
    {
        auto j = Prefs::getString (("layout_slot_" + juce::String (slot)).toRawUTF8());
        if (j.isEmpty()) { Ui::message (juce::MessageBoxIconType::InfoIcon, "Layouts", "Layout slot " + juce::String (slot) + " is empty. Window > Layouts > Save Layout " + juce::String (slot) + " stores the current one."); return; }
        applyLayout (juce::JSON::parse (j));
        Prefs::set ("layout_last", slot);
    }
    void applyLayoutPreset (const juce::String& name)
    {
        for (auto& k : { "mix", "beat", "claude" }) if (isTornOff (k)) dockPage (k);
        if (name == "tracking")      { setLayoutMode ("side"); splitRatio = 0.72f; Prefs::set ("mx_narrowStrips", true); showView ("edit"); }
        else if (name == "mixing")   { setLayoutMode ("stacked"); splitRatio = 0.3f; showView ("mix"); }
        else if (name == "beatmaking") { setLayoutMode ("single"); beat.setPanel ("mixer", true); beat.setPanel ("browser", true); showView ("beat"); }
        else if (name == "mastering") { setLayoutMode ("single"); showView ("edit"); toggleWindow ("mastering", true); }
        else if (name == "two displays") { setLayoutMode ("single"); showView ("edit"); tearOff ("mix"); }
        resized();
    }

    void paint (juce::Graphics& g) override    { g.fillAll (Theme::bg()); }

    void resized() override
    {
        auto r = getLocalBounds();
        transport.setBounds (r.removeFromTop (64));
        status.setBounds (r.removeFromBottom (22).reduced (10, 0));
        if (menuBar != nullptr && menuBar->isVisible()) menuBar->setBounds (r.removeFromTop (22));
        const bool split = layoutMode != "single" && (currentView == "edit" || currentView == "mix") && ! isTornOff ("mix");
        for (auto* c : std::initializer_list<juce::Component*> { &timeline, &mixer, &claude, &beat }) if (c->getParentComponent() == this) c->setVisible (false);
        splitDivider.setVisible (split);
        if (split)
        {
            auto a = r;
            if (layoutMode == "stacked") { auto top = a.removeFromTop ((int) (a.getHeight() * splitRatio)); splitDivider.setBounds (a.removeFromTop (6)); timeline.setBounds (top); mixer.setBounds (a); }
            else { auto left = a.removeFromLeft ((int) (a.getWidth() * splitRatio)); splitDivider.setBounds (a.removeFromLeft (6)); timeline.setBounds (left); mixer.setBounds (a); }
            timeline.setVisible (true); mixer.setVisible (true);
        }
        else
        {
            auto& page = pageComponent (currentView);
            if (page.getParentComponent() == this) { page.setBounds (r); page.setVisible (true); }
        }
    }

    //==========================================================================
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (session.edit == nullptr) return false;
        auto& tr = session.edit->getTransport();
        auto mods = k.getModifiers();
        auto code = k.getKeyCode();
        bool cmd = mods.isCommandDown(), shift = mods.isShiftDown(), plain = ! mods.isAnyModifierKeyDown();
        auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());

        if (k == juce::KeyPress::spaceKey || (plain && code == juce::KeyPress::numberPad0))
        {
            if (cmd && ! shift) transportCtl.toggleRecord();
            else if (shift && ! cmd) { tr.setPosition (0s); transportCtl.play(); }
            else transportCtl.togglePlay();
            return true;
        }
        if (code == juce::KeyPress::F12Key || code == juce::KeyPress::numberPad3 || (cmd && shift && code == 'R')) { transportCtl.toggleRecord(); return true; }
        if (cmd && shift && code == 'M') { toggleWindow ("mastering", true); return true; }
        if (k == juce::KeyPress::returnKey && plain)   { transportCtl.returnToZero(); return true; }
        if (plain && code == juce::KeyPress::numberPad1) { transportCtl.skipBars (-1); return true; }
        if (plain && code == juce::KeyPress::numberPad2) { transportCtl.skipBars (1); return true; }
        if (plain && code == juce::KeyPress::numberPad4) { transportCtl.setLoopPlayback (! tr.looping.get()); return true; }
        if (plain && code == juce::KeyPress::numberPad5) { transportCtl.setLoopRecord (! Prefs::loopRecord()); return true; }
        if (plain && code == juce::KeyPress::numberPad6) { transportCtl.setQuickPunch (! Prefs::quickPunch()); return true; }
        if (plain && code == juce::KeyPress::numberPad7) { transportCtl.setClick (! session.edit->clickTrackEnabled.get()); return true; }
        if (plain && code == juce::KeyPress::numberPad8) { transportCtl.setCountOff (Prefs::countOffBars() ? 0 : 2); return true; }
        if (plain && code == juce::KeyPress::numberPad9) { transportCtl.setMidiMerge (! Prefs::midiMerge()); return true; }
        if (cmd && code == 'Z') { shift ? session.edit->redo() : session.edit->undo(); return true; }
        if (cmd && code == 'S' && ! shift) { if (session.save()) session.log.add ("You", "Saved"); else if (! session.savingAllowed()) showLicenceDialog (true); return true; }
        if (cmd && code == 'S' && shift) { menuItemSelected (4, 0); return true; }
        if (cmd && code == 'N' && ! shift) { menuItemSelected (1, 0); return true; }
        if (cmd && code == 'O') { menuItemSelected (2, 0); return true; }
        if (cmd && code == 'Q') { juce::JUCEApplication::getInstance()->systemRequestedQuit(); return true; }
        if (cmd && code == 'W') { closeAllWindows(); return true; }
        if (cmd && code == ',') { toggleWindow ("preferences", true); return true; }
        if (cmd && code == '=') { cyclePages(); return true; }
        if (cmd && shift && code == 'B') { showView ("beat"); return true; }
        if (cmd && mods.isAltDown() && code >= '1' && code <= '5') { recallLayoutSlot (code - '0'); return true; }
        if (cmd && shift && code == 'N') { newTracksDialog(); return true; }
        if (cmd && shift && code == 'L') { transportCtl.setLoopPlayback (! tr.looping.get()); return true; }
        if (cmd && shift && code == 'I') { menuItemSelected (5, 0); return true; }
        if (cmd && mods.isAltDown() && code == 'B') { menuItemSelected (6, 0); return true; }
        if (cmd && code == 'G' && ! shift) { toggleWindow ("groups", true); return true; }
        if (cmd && code == 'K' && ! shift) { transportCtl.setPreRoll (! Prefs::preRollEnabled()); return true; }
        if (cmd && code == 'K' && shift) { transportCtl.setPostRoll (! Prefs::postRollEnabled()); return true; }
        if (cmd && shift && code == 'J') { Prefs::set ("op_linkTimelineEdit", ! Prefs::linkTimelineAndEdit()); return true; }
        if (cmd && code == '1' && ! shift) { toggleWindow ("transport", true); return true; }
        if (cmd && code == '3' && ! shift) { toggleWindow ("bigcounter", true); return true; }
        if (cmd && code == '5' && ! shift) { toggleWindow ("memory", true); return true; }
        if (cmd && code == '6' && ! shift) { toggleWindow ("rack", true); return true; }
        if (cmd && shift && code == 'K') { toggleWindow ("colors", true); return true; }
        if (cmd && shift && (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)) { if (! selectedRows().isEmpty()) menuItemSelected (308, 3); return true; }
        if (plain && ch == 'l') { transportCtl.setLoopPlayback (! tr.looping.get()); return true; }
        if (plain && ch == 'c') { transportCtl.setClick (! session.edit->clickTrackEnabled.get()); return true; }
        if (plain && ch == 'q') { timeline.addMemoryLocationAtPlayhead(); return true; }
        if (code == juce::KeyPress::homeKey) { transportCtl.returnToZero(); return true; }
        if (code == juce::KeyPress::endKey) { transportCtl.goToEnd(); return true; }
        if (plain && (code == juce::KeyPress::pageDownKey)) { transportCtl.goToMarker (true); return true; }
        if (plain && (code == juce::KeyPress::pageUpKey)) { transportCtl.goToMarker (false); return true; }
        if (currentView == "edit") return timeline.keyPressed (k);
        if (currentView == "mix")
        {
            if (shift && ! cmd && (ch == 's' || ch == 'm' || ch == 'r'))
                for (auto t : session.getRows())
                    if (timeline.state.isTrackSelected (t))
                    {
                        if (ch == 's') session.setTrackSolo (*t, ! t->isSolo (false));
                        else if (ch == 'm') session.setTrackMute (*t, ! t->isMuted (false));
                        else if (auto at = dynamic_cast<te::AudioTrack*> (t)) session.setArmed (*at, ! session.isArmed (*at));
                    }
            return true;
        }
        return false;
    }

    //==========================================================================
    juce::StringArray getMenuBarNames() override    { return { "File", "Edit", "View", "Track", "Clip", "Event", "AudioSuite", "Options", "Setup", "Window", "Claude", "Help" }; }

    juce::PopupMenu getMenuForIndex (int index, const juce::String&) override
    {
        juce::PopupMenu m;
        auto& st = timeline.state;
        bool hasEdit = session.edit != nullptr;
        bool loopOn = hasEdit && session.edit->getTransport().looping.get();

        // Context, Pro Tools style: commands that need a selection are greyed out until there is one.
        bool clipSel = ! st.selectedClips.empty(), audioSel = false, midiSel = false, stepSel = false, trackSel = false;
        for (auto& c : st.selectedClips)
        {
            if (dynamic_cast<te::AudioClipBase*> (c.get())) audioSel = true;
            if (dynamic_cast<te::MidiClip*> (c.get())) midiSel = true;
            if (dynamic_cast<te::StepClip*> (c.get())) stepSel = true;
        }
        bool timeSel = st.timeSelection.has_value() && st.timeSelection->getLength() > 0.0001s;
        if (hasEdit) for (auto t : session.getRows()) if (st.isTrackSelected (t)) { trackSel = true; break; }
        bool anySel = clipSel || timeSel;

        if (index == 0)   // File
        {
            m.addItem (1, "New Session...  (Cmd+N)");
            m.addItem (8, "New Session from Template...");
            m.addItem (18, "Make a Beat  (new session, kit, Beat page)");
            m.addItem (2, "Open Session...  (Cmd+O)");
            juce::PopupMenu recent;
            int id = 50;
            for (auto f : session.sessionsDir.findChildFiles (juce::File::findFiles, false, "*.squelch"))
                if (id < 80) recent.addItem (id++, f.getFileNameWithoutExtension());
            m.addSubMenu ("Open Recent", recent);
            m.addSeparator();
            m.addItem (3, "Save  (Cmd+S)", session.savingAllowed());
            m.addItem (4, "Save As...  (Cmd+Shift+S)", session.savingAllowed());
            m.addItem (9, "Save Copy In...", session.savingAllowed());
            m.addItem (10, "Save As Template...");
            m.addItem (11, "Revert to Saved...");
            m.addItem (16, "Restore from Backup...");
            m.addItem (17, "Back Up Now", session.edit != nullptr);
            m.addSeparator();
            m.addItem (5, "Import Audio / MIDI...  (Cmd+Shift+I)");
            m.addItem (12, "Import Session Data (tracks from another session)...");
            m.addItem (6, "Bounce Mix...  (Cmd+Opt+B)", session.savingAllowed());
            m.addItem (13, "Export Stems...  (each track or folder as its own file)", session.savingAllowed());
            m.addItem (14, "Export MIDI...");
            m.addSeparator();
            m.addItem (7, "Show Session Folder");
            m.addItem (15, "Session Setup  (Cmd+2)");
        }
        else if (index == 1)   // Edit
        {
            m.addItem (100, "Undo  (Cmd+Z)", hasEdit && session.edit->getUndoManager().canUndo());
            m.addItem (101, "Redo  (Cmd+Shift+Z)", hasEdit && session.edit->getUndoManager().canRedo());
            m.addItem (102, "Undo History...");
            m.addSeparator();
            m.addItem (103, "Cut  (Cmd+X)", anySel); m.addItem (104, "Copy  (Cmd+C)", anySel); m.addItem (105, "Paste  (Cmd+V)", EditOps::canPaste (session));
            m.addItem (106, "Clear / Delete  (Delete)", anySel); m.addItem (107, "Delete and close gap  (Shift+Delete)", anySel);
            m.addSeparator();
            m.addItem (108, "Select All  (Cmd+A)");
            m.addItem (109, "Deselect  (Esc)", anySel || trackSel);
            m.addSeparator();
            m.addItem (110, "Duplicate  (Cmd+D)", anySel); m.addItem (111, "Repeat...  (Opt+R)", anySel);
            m.addItem (112, "Separate Clip at Selection / Playhead  (Cmd+E)", clipSel || timeSel);
            m.addItem (124, "Separate Clip at Transients", audioSel);
            m.addItem (113, "Heal Separation  (Cmd+H)", clipSel);
            m.addItem (114, "Trim Clip to Selection  (Cmd+T)", clipSel && timeSel);
            m.addItem (115, "Trim Clip Start to Insertion  (A)", clipSel); m.addItem (116, "Trim Clip End to Insertion  (S)", clipSel);
            m.addItem (117, "Strip Silence...  (Cmd+U)", audioSel);
            m.addItem (118, "Consolidate Clip  (Opt+Shift+3)", clipSel || timeSel);
            m.addItem (119, "Fades...  (Cmd+F)", audioSel);
            m.addItem (120, "Insert Silence (selection length)", timeSel);
            m.addSeparator();
            juce::PopupMenu modes;
            modes.addItem (130, "Shuffle  (F1)", true, st.editMode == EditMode::shuffle);
            modes.addItem (131, "Slip  (F2)", true, st.editMode == EditMode::slip);
            modes.addItem (132, "Spot  (F3)", true, st.editMode == EditMode::spot);
            modes.addItem (133, "Grid  (F4)", true, st.editMode == EditMode::grid);
            modes.addSeparator();
            modes.addItem (134, "Relative Grid", true, st.gridRelative);
            m.addSubMenu ("Edit Mode", modes);
            juce::PopupMenu toolsMenu;
            const char* tn[] = { "Zoomer  (F5)", "Trim  (F6)", "Selector  (F7)", "Grabber  (F8)", "Scrubber  (F9)", "Pencil  (F10)", "Smart Tool  (F6+F7)" };
            for (int i = 0; i < 7; ++i) toolsMenu.addItem (140 + i, tn[i], true, (int) st.tool == i);
            m.addSubMenu ("Edit Tool", toolsMenu);
        }
        else if (index == 2)   // View
        {
            m.addItem (200, "Edit Window", true, currentView == "edit");
            m.addItem (201, "Mix Window  (Cmd+=)", true, currentView == "mix");
            m.addItem (207, "Beat Page  (Cmd+Shift+B)", true, currentView == "beat");
            m.addItem (202, "Claude Panel", true, currentView == "claude");
            {
                juce::PopupMenu lay;
                lay.addItem (8850, "One page at a time", true, layoutMode == "single");
                lay.addItem (8851, "Edit + Mix (stacked)", true, layoutMode == "stacked");
                lay.addItem (8852, "Edit + Mix (side by side)", true, layoutMode == "side");
                lay.addSeparator();
                lay.addItem (8853, isTornOff ("mix") ? "Dock the Mix window back" : "Mix in its own window");
                lay.addItem (8854, isTornOff ("beat") ? "Dock the Beat window back" : "Beat in its own window");
                lay.addItem (8855, isTornOff ("claude") ? "Dock the Claude window back" : "Claude in its own window");
                m.addSubMenu ("Layout", lay);
                juce::PopupMenu bp;
                bp.addItem (8860, "Browser", true, beat.panelShown ("browser")); bp.addItem (8861, "Instrument panel", true, beat.panelShown ("side"));
                bp.addItem (8863, "Beat Mixer", true, beat.panelShown ("mixer"));
                bp.addSeparator(); bp.addItem (8864, "Graph Editor  (Cmd+K in the rack)", true, beat.getRack() != nullptr && beat.getRack()->isGraphVisible());
                m.addSubMenu ("Beat Page Panels", bp);
            }
            m.addSeparator();
            m.addItem (203, "Zoom In  (Cmd+])"); m.addItem (204, "Zoom Out  (Cmd+[)");
            m.addItem (205, "Fit Session  (Cmd+Opt+F)"); m.addItem (206, "Fit Selection  (Opt+F)");
            m.addSeparator();
            juce::PopupMenu heights;
            for (int i = 0; i < (int) TimelineState::heightPresets().size(); ++i)
                heights.addItem (8820 + i, juce::String (TimelineState::heightPresets()[(size_t) i].name) + "   (" + juce::String (TimelineState::heightPresets()[(size_t) i].px) + " px)");
            heights.addSeparator();
            heights.addItem (8830, "Fit All Tracks to Window");
            heights.addItem (8831, "Ctrl+Up / Ctrl+Down: selected tracks bigger / smaller", false);
            m.addSubMenu ("Track Height (all)", heights);
            juce::PopupMenu wave;
            const int wv = Prefs::getInt ("ed_waveView", 0);
            wave.addItem (8840, "Peak", true, wv == 0);
            wave.addItem (8841, "Rectified (louder parts easier to see)", true, wv == 1);
            wave.addItem (8842, "Power (log scale - quiet detail visible)", true, wv == 2);
            m.addSubMenu ("Waveform View", wave);
            juce::PopupMenu ev;
            ev.addItem (250, "I/O", true, EditViews::io());
            ev.addItem (251, "Inserts A-E", true, EditViews::insAE());
            ev.addItem (252, "Inserts F-J", true, EditViews::insFJ());
            ev.addItem (253, "Sends A-E", true, EditViews::sendAE());
            ev.addItem (254, "Sends F-J", true, EditViews::sendFJ());
            ev.addItem (255, "Comments", true, EditViews::comments());
            ev.addSeparator();
            ev.addItem (256, "All");
            ev.addItem (257, "None");
            m.addSubMenu ("Edit Window Views", ev);
            m.addItem (216, "Clip Names", true, Prefs::clipNamesVisible());
            m.addItem (217, "Clip Gain Line", true, Prefs::clipGainLineVisible());
            m.addItem (218, "Narrow Mix Strips", true, Prefs::narrowStrips());
            juce::PopupMenu mv;
            mv.addItem (219, "Inserts A-E", true, Prefs::showInsertsInMix());
            mv.addItem (221, "Inserts F-J (10 insert slots)", true, Prefs::getBool ("mx_insertsFJ", false));
            mv.addItem (220, "Sends A-E", true, Prefs::showSendsInMix());
            mv.addItem (222, "Sends F-J (10 send slots)", true, Prefs::getBool ("mx_sendsFJ", false));
            m.addSubMenu ("Mix Window Views", mv);
           #if JUCE_MAC
            m.addItem (223, "Menu Bar inside the window (backup for the Mac menu bar)", true, inWindowMenuBarWanted());
           #endif
            m.addSeparator();
            juce::PopupMenu themes;
            for (int i = 0; i < (int) Theme::presets().size(); ++i) themes.addItem (230 + i, Theme::presets()[(size_t) i].name, true, Theme::current().name == Theme::presets()[(size_t) i].name);
            themes.addSeparator();
            themes.addItem (229, "Customise colours...");
            m.addSubMenu ("Theme", themes);
        }
        else if (index == 3)   // Track
        {
            bool anyInactive = false, allSafe = trackSel, allSoloSafe = trackSel, anyFrozen = false, anyAudioSel = false, anyStereoSel = false;
            for (auto t : selectedRows())
            {
                if (TrackOps::isInactive (*t)) anyInactive = true;
                if (! TrackOps::isRecordSafe (*t)) allSafe = false;
                if (! TrackOps::isSoloSafe (*t)) allSoloSafe = false;
                if (auto at = dynamic_cast<te::AudioTrack*> (t))
                {
                    anyAudioSel = true;
                    if (at->isFrozen (te::Track::individualFreeze)) anyFrozen = true;
                    if (IO::formatOf (*at) == "stereo" || session.trackIsStereo (*at)) anyStereoSel = true;
                }
            }
            m.addItem (341, "New...  (Cmd+Shift+N)");
            m.addItem (307, "Group...  (Cmd+G)", trackSel);
            m.addItem (306, "Duplicate...", trackSel);
            m.addItem (342, "Split into Mono", anyStereoSel);
            m.addItem (343, anyInactive ? "Make Active" : "Make Inactive", trackSel);
            m.addItem (308, "Delete...  (Cmd+Shift+Delete)", trackSel);
            m.addSeparator();
            m.addItem (304, "Move to New Routing Folder...", trackSel);
            m.addItem (358, "Move to New Basic Folder...", trackSel);
            m.addItem (305, "New VCA Master (from selected tracks)", trackSel);
            m.addSeparator();
            m.addItem (344, "Input Only Monitoring", true, TrackOps::inputOnlyMonitoring());
            m.addItem (345, "Auto Input Monitoring", true, ! TrackOps::inputOnlyMonitoring());
            m.addItem (346, allSafe && trackSel ? "Clear Record Safe" : "Set Record Safe", trackSel);
            m.addItem (347, allSoloSafe && trackSel ? "Clear Solo Safe" : "Set Solo Safe", trackSel);
            m.addSeparator();
            m.addItem (348, anyFrozen ? "Unfreeze" : "Freeze", anyAudioSel);
            m.addItem (349, "Commit...", anyAudioSel);
            m.addItem (350, "Bounce...", anyAudioSel);
            m.addSeparator();
            m.addItem (309, "Solo  (Shift+S)", trackSel); m.addItem (310, "Mute  (Shift+M)", trackSel); m.addItem (311, "Record Arm  (Shift+R)", trackSel);
            m.addItem (312, "Clear All Solos"); m.addItem (313, "Clear All Mutes");
            m.addItem (351, "Clear All Clip Indicators");
            m.addSeparator();
            juce::PopupMenu am;
            auto names = Session::automationModeNames();
            for (int i = 0; i < names.size(); ++i) am.addItem (320 + i, names[i] + " (selected tracks)", trackSel);
            m.addSubMenu ("Automation Mode", am);
            juce::PopupMenu el;
            auto en = Session::elasticModeNames();
            for (int i = 0; i < en.size(); ++i) el.addItem (330 + i, en[i] + " (selected tracks)", trackSel);
            m.addSubMenu ("Elastic Audio", el);
            juce::PopupMenu tb;
            tb.addItem (352, "Samples (absolute time)", anyAudioSel);
            tb.addItem (353, "Ticks (follows tempo)", anyAudioSel);
            m.addSubMenu ("Timebase", tb);
            m.addSeparator();
            m.addItem (354, "Hide", trackSel);
            m.addItem (355, "Show All Tracks");
            m.addItem (356, "Scroll to Track...  (Cmd+Opt+F)");
            m.addItem (357, TrackOps::hasMasterFader (session) ? "Delete Master Fader" : "New Master Fader");
            m.addSeparator();
            m.addSubMenu ("Quick New", QuickNew::build (session, quickNewBase));
            m.addItem (340, "Save Track Preset...", trackSel);
        }
        else if (index == 4)   // Clip
        {
            m.addItem (400, "Mute / Unmute  (Cmd+M)", clipSel); m.addItem (401, "Lock / Unlock position", clipSel);
            m.addItem (402, "Rename...", clipSel);
            m.addItem (403, "Spot...", clipSel);
            m.addSeparator();
            juce::PopupMenu gain;
            gain.addItem (410, "+1 dB", audioSel); gain.addItem (411, "-1 dB", audioSel); gain.addItem (412, "+3 dB", audioSel); gain.addItem (413, "-3 dB", audioSel); gain.addItem (414, "Reset", audioSel); gain.addItem (415, "Normalise to -1 dBFS (clip gain)", audioSel);
            m.addSubMenu ("Clip Gain", gain);
            m.addItem (420, "Fades (default length)  (Cmd+F)", audioSel); m.addItem (421, "Remove Fades", audioSel);
            m.addItem (422, "Reverse", audioSel);
            m.addSeparator();
            m.addItem (430, "Conform to Session Tempo (Elastic Audio)", audioSel); m.addItem (431, "Follow Session Tempo on/off", audioSel);
            m.addItem (432, "Half Speed", audioSel); m.addItem (433, "Double Speed", audioSel);
            m.addSeparator();
            m.addItem (440, "Loop Clip to Selection End", clipSel && timeSel);
            m.addItem (441, "Bring to Front / Send to Back", clipSel);
            m.addSeparator();
            juce::PopupMenu col;
            for (int i = 0; i < Theme::trackPalette().size(); ++i) col.addColouredItem (450 + i, "Colour " + juce::String (i + 1), Theme::trackPalette()[i], clipSel);
            col.addItem (449, "Use track colour", clipSel);
            m.addSubMenu ("Clip Colour", col);
        }
        else if (index == 5)   // Event
        {
            m.addItem (500, "Quantize Selected MIDI Clips  (1/16)", midiSel);
            m.addItem (501, "Quantize...  (choose grid)", midiSel);
            m.addItem (502, "Transpose Selected MIDI +1 semitone  (Shift+Up)", midiSel);
            m.addItem (503, "Transpose Selected MIDI -1 semitone  (Shift+Down)", midiSel);
            m.addItem (504, "Transpose +12", midiSel); m.addItem (505, "Transpose -12", midiSel);
            m.addItem (506, "Velocity +10", midiSel); m.addItem (507, "Velocity -10", midiSel);
            m.addItem (508, "Legato (join notes)", midiSel);
            m.addItem (509, "Remove Duplicate Notes", midiSel);
            m.addSeparator();
            m.addItem (510, "Tempo...");
            m.addItem (511, "Meter...");
            m.addItem (512, "Identify Beat (set tempo from Edit selection = 1 bar)", timeSel);
            m.addItem (513, "Tap Tempo...");
            m.addSeparator();
            m.addItem (520, "New Memory Location  (Shift+Return / Q)");
            m.addItem (521, "Memory Locations Window  (Cmd+5)");
            m.addItem (522, "Go to Next Marker  (Page Down)"); m.addItem (523, "Go to Previous Marker  (Page Up)");
        }
        else if (index == 6)   // AudioSuite
        {
            m.addItem (608, "Molt (Time & Pitch)...", true);
            m.addSeparator();
            m.addItem (600, "Normalise (render)", audioSel); m.addItem (601, "Reverse (render)", audioSel); m.addItem (602, "Invert Phase (render)", audioSel); m.addItem (603, "Make Mono (render)", audioSel); m.addItem (604, "Pitch Shift... (render)", audioSel);
            m.addSeparator();
            m.addSubMenu ("Render through plugin...", PluginMenu::build (session.getEngine(), PluginMenu::effects), audioSel);
            m.addSeparator();
            m.addItem (605, "Remove all rendered effects from selected clips", audioSel);
            m.addItem (606, "Strip Silence...  (Cmd+U)", audioSel);
            m.addItem (607, "Separate at Transients", audioSel);
        }
        else if (index == 7)   // Options
        {
            m.addItem (700, "Loop Playback  (Cmd+Shift+L)", true, loopOn);
            m.addItem (701, "Loop Record", true, Prefs::loopRecord());
            m.addItem (702, "QuickPunch", true, Prefs::quickPunch());
            m.addSeparator();
            m.addItem (703, "Pre-Roll  (Cmd+K)", true, Prefs::preRollEnabled());
            m.addItem (704, "Post-Roll  (Cmd+Shift+K)", true, Prefs::postRollEnabled());
            m.addItem (705, "Count-off (2 bars)", true, Prefs::countOffBars() > 0);
            m.addItem (706, "Click  (C)", true, hasEdit && session.edit->clickTrackEnabled.get());
            m.addSeparator();
            m.addItem (707, "MIDI Merge", true, Prefs::midiMerge());
            m.addItem (708, "Wait for Note", true, Prefs::waitForNote());
            m.addItem (709, "MIDI Thru", true, Prefs::midiThru());
            m.addSeparator();
            m.addItem (710, "Link Timeline and Edit Selection  (Cmd+Shift+J)", true, Prefs::linkTimelineAndEdit());
            m.addItem (711, "Link Track and Edit Selection", true, st.linkTrackAndEditSelection);
            m.addItem (712, "Insertion Follows Playback", true, Prefs::insertionFollowsPlayback());
            m.addItem (713, "Scrolling: Page", true, Prefs::scrollMode() == 1);
            m.addItem (741, "Scrolling: Continuous (playhead centred)", true, Prefs::scrollMode() == 2);
            m.addItem (742, "Scrolling: No Auto-Scrolling", true, Prefs::scrollMode() == 0);
            m.addItem (714, "Tab to Transients", true, st.tabToTransients);
            m.addItem (715, "Mix Groups Active", true, session.groupsEnabled);
            m.addItem (716, "Solo Latch", true, Prefs::soloLatching());
            m.addItem (717, "Auto-fades on edits", true, Prefs::autoFadesOnEdits());
            m.addSeparator();
            m.addItem (718, "EQ Frequency Chart (reference for mixing)...", true, isWindowVisible ("eqchart"));
            m.addItem (719, "Mastering Window  (Cmd+Shift+M)", true, isWindowVisible ("mastering"));
        }
        else if (index == 8)   // Setup
        {
            m.addItem (800, "Playback Engine (audio device)...");
            m.addItem (801, "I/O Setup...");
            m.addItem (802, "Session Setup  (Cmd+2)");
            m.addSeparator();
            m.addItem (803, scanner.isScanning() ? "Scanning plugins..." : "Scan for New Plugins", ! scanner.isScanning());
            m.addItem (804, "Rescan All Plugins");
            m.addItem (805, "Plugins Window...");
            {
                juce::PopupMenu off;
                auto list = CrashGuard::disabledPlugins();
                for (int i = 0; i < list.size() && i < 40; ++i) off.addItem (840 + i, "Turn back on: " + list[i]);
                if (list.isEmpty()) off.addItem (839, "(none - every plug-in is on)", false);
                m.addSubMenu ("Plug-ins Switched Off" + juce::String (list.isEmpty() ? "" : "  (" + juce::String (list.size()) + ")"), off);
            }
            if (CrashGuard::state().safeMode) m.addItem (838, "Leave Safe Mode (load third-party plug-ins)");
            m.addItem (837, "Show Last Crash Report", CrashGuard::reportFile().existsAsFile());
            m.addSeparator();
            m.addItem (806, "Preferences...  (Cmd+,)");
        }
        else if (index == 9)   // Window
        {
            m.addItem (900, "Edit", true, currentView == "edit");
            m.addItem (901, "Mix  (Cmd+=)", true, currentView == "mix");
            m.addSeparator();
            m.addItem (902, "Transport  (Cmd+1)", true, isWindowVisible ("transport"));
            m.addItem (903, "Big Counter  (Cmd+3)", true, isWindowVisible ("bigcounter"));
            m.addItem (904, "Memory Locations  (Cmd+5)", true, isWindowVisible ("memory"));
            m.addItem (905, "Clip List", true, isWindowVisible ("cliplist"));
            m.addItem (906, "Groups", true, isWindowVisible ("groups"));
            m.addItem (907, "Undo History", true, isWindowVisible ("undo"));
            m.addItem (908, "Color Palette  (Cmd+Shift+K)", true, isWindowVisible ("colors"));
            m.addSeparator();
            m.addItem (909, "Session Setup", true, isWindowVisible ("session"));
            m.addItem (910, "I/O Setup", true, isWindowVisible ("io"));
            m.addItem (911, "Playback Engine", true, isWindowVisible ("engine"));
            m.addItem (912, "Plugins", true, isWindowVisible ("plugins"));
            m.addItem (913, "Preferences", true, isWindowVisible ("preferences"));
            m.addSeparator();
            m.addItem (914, midiSel ? "MIDI Editor (selected MIDI clip)" : "MIDI Editor  (select or double-click a MIDI clip)", midiSel);
            m.addItem (915, stepSel ? "Beat Maker (selected drum clip)" : "Beat Maker  (select or double-click a drum clip)", stepSel);
            m.addItem (916, "Claude Panel", true, currentView == "claude");
            m.addItem (923, "Channel Rack  (Cmd+6)", hasEdit, isWindowVisible ("rack"));
            m.addItem (924, "Cut Lab - audio editor  (Cmd+Shift+E)", true, cutLab != nullptr && cutLab->isVisible());
            {
                juce::PopupMenu lay;
                for (int i = 1; i <= 5; ++i) lay.addItem (8870 + i, "Recall Layout " + juce::String (i) + "  (Cmd+Opt+" + juce::String (i) + ")", Prefs::getString (("layout_slot_" + juce::String (i)).toRawUTF8()).isNotEmpty());
                lay.addSeparator();
                for (int i = 1; i <= 5; ++i) lay.addItem (8880 + i, "Save Layout " + juce::String (i));
                lay.addSeparator();
                lay.addItem (8890, "Preset: Tracking (Edit + narrow Mix)"); lay.addItem (8891, "Preset: Mixing (Mix + Edit strip)"); lay.addItem (8892, "Preset: Beatmaking (Beat + Mixer)");
                lay.addItem (8893, "Preset: Mastering"); lay.addItem (8894, "Preset: Two displays (Mix torn off)");
                m.addSubMenu ("Layouts", lay);
            }
            m.addItem (917, "Quick Start / Welcome", true, isWindowVisible ("welcome"));
            m.addItem (919, "Mastering  (Cmd+Shift+M)", true, isWindowVisible ("mastering"));
            m.addItem (920, "EQ Frequency Chart", true, isWindowVisible ("eqchart"));
            m.addItem (921, "System Usage  (CPU, memory, heaviest plug-ins)", true, isWindowVisible ("usage"));
            m.addItem (922, "Mix Snapshots (A-H)", hasEdit, isWindowVisible ("snapshots"));
            m.addSeparator();
            m.addItem (918, "Close All Floating Windows  (Cmd+W)");
        }
        else if (index == 10)   // Claude
        {
            m.addItem (60, tunnel.isActive() ? "Disconnect Claude" : "Connect Claude");
            m.addItem (61, "Copy Claude Link", claude.getConnectorUrl().isNotEmpty());
            m.addItem (62, "Open claude.ai Connectors page");
            m.addItem (63, "Run connection self-test");
            m.addItem (64, "Copy diagnostics for support");
            m.addSeparator();
            m.addItem (65, "Show Claude Panel");
        }
        else if (index == 11)   // Help
        {
            m.addItem (70, "About Squelchmoth");
            m.addItem (74, "Keyboard Shortcuts...");
            m.addItem (75, "Quick Start Window");
            m.addItem (76, "Open Setup Guide (SETUP.md)");
            m.addItem (71, "Open Sessions Folder");
            m.addSeparator();
            m.addItem (72, "Licences and Upgrades...  (" + Edition::editionName() + ")");
            m.addItem (73, Edition::isDemo() ? "Buy Squelchmoth..." : "Visit the Squelchmoth Store...");
            m.addSeparator();
            m.addItem (77, "Licence Agreement (EULA)");
            m.addItem (78, "Privacy Policy");
            m.addItem (79, "Third-Party Notices");
            m.addSeparator();
            m.addItem (80, "Uninstall Squelchmoth...");
        }
        return m;
    }

    void menuItemSelected (int id, int topLevelIndex) override
    {
        auto& st = timeline.state;
        auto repaintViews = [this] { timeline.repaint(); mixer.repaint(); };

        // Every menu command is logged (Claude panel > activity, and Help > Copy diagnostics) so a
        // command that appears to do nothing can be traced.
        session.log.add ("You", "Menu: " + menuItemPath (id, topLevelIndex));
        lastMenuHandled = true;

        // ---- File
        if (id == 1) newSessionDialog (false);
        else if (id == 8) newSessionDialog (true);
        else if (id == 2)
        {
            chooser = std::make_unique<juce::FileChooser> ("Open Session", session.sessionsDir, "*.squelch;*.tracktionedit");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().existsAsFile()) session.openSession (fc.getResult());
            });
        }
        else if (id >= 50 && id < 80 && id != 60 && id != 61 && id != 62 && id != 63 && id != 64 && id != 65 && id < 70)
        {
            auto files = session.sessionsDir.findChildFiles (juce::File::findFiles, false, "*.squelch");
            if (id - 50 < files.size()) session.openSession (files[id - 50]);
        }
        else if (id == 3) { if (! session.save() && ! session.savingAllowed()) showLicenceDialog (true); }
        else if (id == 4 || id == 9)
        {
            chooser = std::make_unique<juce::FileChooser> (id == 4 ? "Save Session As" : "Save a Copy", session.sessionsDir, "*.squelch");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, id] (const juce::FileChooser& fc)
            {
                if (fc.getResult() == juce::File()) return;
                if (id == 4) session.saveAs (fc.getResult());
                else te::EditFileOperations (*session.edit).writeToFile (fc.getResult().withFileExtension (".squelch"), false);
            });
        }
        else if (id == 10) saveAsTemplate();
        else if (id == 11)
        {
            auto f = session.getFile();
            if (f.existsAsFile())
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Revert to Saved",
                    "Discard every change since the session was last saved?", "Revert", "Cancel", nullptr,
                    juce::ModalCallbackFunction::create ([this, f] (int r) { if (r == 1) { session.saveOnNextReplace = false; session.openSession (f); } }));
        }
        else if (id == 16) { windows.erase ("backups"); toggleWindow ("backups", true); }
        else if (id == 17) { juce::String err; if (auto a = AutoSaver::instance(); a == nullptr || ! a->backupNow (&err)) Ui::message (juce::MessageBoxIconType::WarningIcon, "Back Up Now", err.isNotEmpty() ? err : "No session is open."); }
        else if (id == 5)
        {
            chooser = std::make_unique<juce::FileChooser> ("Import", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                           session.getEngine().getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats() + ";*.mid;*.midi");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [this] (const juce::FileChooser& fc)
            {
                for (auto f : fc.getResults())
                {
                    session.nextAudioTrackStereo = ! f.hasFileExtension ("mid;midi") && te::AudioFile (session.getEngine(), f).getInfo().numChannels >= 2;
                    auto res = tools.call ("import_audio", juce::JSON::parse ("{\"track\":\"" + juce::String (session.addTrack ("audio", f.getFileNameWithoutExtension())->getName()) + "\",\"path\":" + juce::JSON::toString (f.getFullPathName()) + "}"));
                    juce::ignoreUnused (res);
                }
            });
        }
        else if (id == 12) importSessionData();
        else if (id == 6) showBounceDialog (false);
        else if (id == 13) showBounceDialog (true);
        else if (id == 14) exportMidi();
        else if (id == 7) session.sessionsDir.revealToUser();
        else if (id == 15) toggleWindow ("session", true);

        // ---- Edit
        else if (id == 100 && session.edit) session.edit->undo();
        else if (id == 101 && session.edit) session.edit->redo();
        else if (id == 102) toggleWindow ("undo", true);
        else if (id == 103) { EditOps::cut (session, st); repaintViews(); }
        else if (id == 104) EditOps::copy (session, st);
        else if (id == 105) { EditOps::paste (session, st); repaintViews(); }
        else if (id == 106) { EditOps::deleteSelection (session, st); repaintViews(); }
        else if (id == 107) { EditOps::deleteSelection (session, st, true); repaintViews(); }
        else if (id == 108) { EditOps::selectAll (session, st); repaintViews(); }
        else if (id == 109) { st.deselectClips(); st.timeSelection.reset(); repaintViews(); }
        else if (id == 110) { EditOps::duplicate (session, st); repaintViews(); }
        else if (id == 111) timeline.keyPressed (juce::KeyPress ('R', juce::ModifierKeys::altModifier, 0));
        else if (id == 112) { EditOps::separate (session, st); repaintViews(); }
        else if (id == 124 || id == 607) separateAtTransients();
        else if (id == 113) { EditOps::heal (session, st); repaintViews(); }
        else if (id == 114) { EditOps::trimToSelection (session, st); repaintViews(); }
        else if (id == 115) { EditOps::trimToPlayhead (session, st, true); repaintViews(); }
        else if (id == 116) { EditOps::trimToPlayhead (session, st, false); repaintViews(); }
        else if (id == 117 || id == 606) timeline.keyPressed (juce::KeyPress ('U', juce::ModifierKeys::commandModifier, 0));
        else if (id == 118) { juce::String err; EditOps::consolidate (session, st, err); if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Consolidate", err); repaintViews(); }
        else if (id == 119) { EditOps::fade (session, st); repaintViews(); }
        else if (id == 120) { if (st.timeSelection && st.timeSelection->getLength() > 0.001s) te::insertSpaceIntoEdit (*session.edit, *st.timeSelection); repaintViews(); }
        else if (id >= 130 && id <= 133) timeline.setEditMode ((EditMode) (id - 130));
        else if (id == 134) { st.gridRelative = ! st.gridRelative; timeline.setEditMode (st.editMode); }
        else if (id >= 140 && id <= 146) timeline.setTool ((EditTool) (id - 140));

        // ---- View
        else if (id == 200 || id == 900) showView ("edit");
        else if (id == 201 || id == 901) showView ("mix");
        else if (id == 202 || id == 916 || id == 65) showView ("claude");
        else if (id == 207) showView ("beat");
        else if (id == 18) makeABeat();
        else if (id == 8850) setLayoutMode ("single");
        else if (id == 8851) setLayoutMode ("stacked");
        else if (id == 8852) setLayoutMode ("side");
        else if (id == 8853) { if (isTornOff ("mix")) dockPage ("mix"); else tearOff ("mix"); }
        else if (id == 8854) { if (isTornOff ("beat")) dockPage ("beat"); else tearOff ("beat"); }
        else if (id == 8855) { if (isTornOff ("claude")) dockPage ("claude"); else tearOff ("claude"); }
        else if (id >= 8860 && id <= 8863) { const char* names[] = { "browser", "side", "playlist", "mixer" }; beat.setPanel (names[id - 8860], ! beat.panelShown (names[id - 8860])); }
        else if (id == 8864) { if (auto rk = beat.getRack()) rk->setGraphVisible (! rk->isGraphVisible()); }
        else if (id >= 8871 && id <= 8875) recallLayoutSlot (id - 8870);
        else if (id >= 8881 && id <= 8885) saveLayoutSlot (id - 8880);
        else if (id >= 8890 && id <= 8894) { const char* names[] = { "tracking", "mixing", "beatmaking", "mastering", "two displays" }; applyLayoutPreset (names[id - 8890]); }
        else if (id == 203) timeline.keyPressed (juce::KeyPress (']', juce::ModifierKeys::commandModifier, 0));
        else if (id == 204) timeline.keyPressed (juce::KeyPress ('[', juce::ModifierKeys::commandModifier, 0));
        else if (id == 205) timeline.zoomToFit();
        else if (id == 206) timeline.zoomToSelection();
        else if (id >= 8820 && id < 8820 + (int) TimelineState::heightPresets().size()) timeline.setTrackHeightsAll (TimelineState::heightPresets()[(size_t) (id - 8820)].px);
        else if (id == 8830) timeline.fitHeights (session.getRows (&st.collapsedFolders));
        else if (id >= 8840 && id <= 8842) { Prefs::set ("ed_waveView", id - 8840); timeline.repaint(); }
        else if (id == 216) { Prefs::set ("ed_clipNames", ! Prefs::clipNamesVisible()); repaintViews(); }
        else if (id == 217) { Prefs::set ("ed_clipGainLine", ! Prefs::clipGainLineVisible()); repaintViews(); }
        else if (id == 218) Prefs::set ("mx_narrow", ! Prefs::narrowStrips());
        else if (id == 219) Prefs::set ("mx_showInserts", ! Prefs::showInsertsInMix());
        else if (id == 220) Prefs::set ("mx_showSends", ! Prefs::showSendsInMix());
        else if (id == 221) Prefs::set ("mx_insertsFJ", ! Prefs::getBool ("mx_insertsFJ", false));
        else if (id == 222) Prefs::set ("mx_sendsFJ", ! Prefs::getBool ("mx_sendsFJ", false));
        else if (id == 223) { Prefs::set ("ui_menuBarInWindow", ! inWindowMenuBarWanted()); if (menuBar) menuBar->setVisible (inWindowMenuBarWanted()); resized(); }
        else if (id == 229) { toggleWindow ("preferences", true); if (auto w = findWindow ("preferences")) if (auto pc = dynamic_cast<PreferencesContent*> (w->getContentComponent())) pc->showTab ("Display"); }
        else if (id >= 230 && id < 230 + (int) Theme::presets().size()) { Theme::applyPreset (Theme::presets()[(size_t) (id - 230)]); Theme::save (props); Theme::changed(); }

        // ---- Track
        else if (id == 301) session.addTrack ("instrument", {}, "4osc");
        else if (id == 302) { TrackOps::NewSpec sp; sp.type = TrackOps::NewType::aux; sp.stereo = true; juce::String note; TrackOps::createTracks (session, sp, nullptr, note); }
        else if (id == 303) { if (auto t = DrumKit::createDrumTrack (session, {}, 4)) if (auto sc = DrumKit::findStepClip (*t)) openStepGrid (*sc); }
        else if (id == 304 || id == 305) { juce::Array<te::Track*> members; for (auto t : session.getRows()) if (st.isTrackSelected (t)) members.add (t); session.addFolder ({}, id == 304, members); }
        else if (id == 306) TrackOps::duplicateDialog (session, selectedRows(), {});
        else if (id == 341) newTracksDialog();
        else if (id == 342) { for (auto at : selectedAudio()) { juce::String err; TrackOps::splitIntoMono (session, *at, err); if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Split into Mono", err); } }
        else if (id == 343) { bool anyInactive = false; for (auto t : selectedRows()) if (TrackOps::isInactive (*t)) anyInactive = true; for (auto t : selectedRows()) if (! t->isMasterTrack()) TrackOps::setInactive (*t, ! anyInactive); }
        else if (id == 344 || id == 345) TrackOps::setInputOnlyMonitoring (session, id == 344);
        else if (id == 346) { bool all = true; for (auto t : selectedRows()) if (! TrackOps::isRecordSafe (*t)) all = false; for (auto t : selectedRows()) TrackOps::setRecordSafe (session, *t, ! all); }
        else if (id == 347) { bool all = true; for (auto t : selectedRows()) if (! TrackOps::isSoloSafe (*t)) all = false; for (auto t : selectedRows()) TrackOps::setSoloSafe (*t, ! all); }
        else if (id == 348)
        {
            bool anyFrozen = false;
            for (auto at : selectedAudio()) if (at->isFrozen (te::Track::individualFreeze)) anyFrozen = true;
            juce::StringArray errs;
            for (auto at : selectedAudio()) { auto err = TrackOps::freeze (*at, ! anyFrozen); if (err.isNotEmpty()) errs.add (err); }
            if (! errs.isEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Freeze", errs.joinIntoString ("\n"));
        }
        else if (id == 349) TrackOps::commitDialog (session, selectedAudio(), {});
        else if (id == 350) TrackOps::bounceTracks (session, selectedAudio());
        else if (id == 351) { clipIndicatorResetCounter()++; session.log.add ("You", "Cleared all clip indicators"); }
        else if (id == 352 || id == 353) for (auto at : selectedAudio()) TrackOps::setTimebase (*at, id == 353);
        else if (id == 354) { for (auto t : selectedRows()) { if (t->isMasterTrack()) session.edit->state.setProperty ("sqmMasterHidden", true, nullptr); else t->setHidden (true); } }
        else if (id == 355) timeline.showAllTracks();
        else if (id == 356) scrollToTrackDialog();
        else if (id == 357) TrackOps::setMasterFader (session, ! TrackOps::hasMasterFader (session));
        else if (id == 358) { auto f = session.addFolder ({}, false, selectedRows()); if (f != nullptr) { if (auto vca = f->getVCAPlugin()) vca->deleteFromParent(); f->state.setProperty ("sqmKind", "basicFolder", nullptr); f->setName ("Folder " + juce::String (session.getFolderTracks().size())); } }
        else if (id >= 250 && id <= 257)
        {
            const char* keys[] = { "ev_io", "ev_insAE", "ev_insFJ", "ev_sendAE", "ev_sendFJ", "ev_comments" };
            if (id <= 255) Prefs::set (keys[id - 250], ! Prefs::getBool (keys[id - 250], id == 250));
            else for (auto k : keys) Prefs::set (k, id == 256);
        }
        else if (id == 307) toggleWindow ("groups", true);
        else if (id == 308)
        {
            auto victims = selectedRows();
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete Tracks", "Delete " + juce::String (victims.size()) + " track(s)? Undo brings them back.", "Delete", "Cancel", nullptr,
                juce::ModalCallbackFunction::create ([this, victims] (int ok)
                {
                    if (ok != 1 || session.edit == nullptr) return;
                    for (auto t : victims) { if (t->isMasterTrack()) TrackOps::setMasterFader (session, false); else { timeline.state.dropReferencesTo (t); session.edit->deleteTrack (t); } }
                }));
        }
        else if (id == 309 || id == 310 || id == 311)
        {
            for (auto t : session.getRows())
                if (st.isTrackSelected (t))
                {
                    if (id == 309) session.setTrackSolo (*t, ! t->isSolo (false));
                    else if (id == 310) session.setTrackMute (*t, ! t->isMuted (false));
                    else if (auto at = dynamic_cast<te::AudioTrack*> (t)) session.setArmed (*at, ! session.isArmed (*at));
                }
        }
        else if (id == 312) for (auto t : session.getRows()) t->setSolo (false);
        else if (id == 313) for (auto t : session.getRows()) t->setMute (false);
        else if (id >= 320 && id <= 324) { for (auto t : session.getRows()) if (st.isTrackSelected (t)) Session::setAutomationModeIndex (*t, id - 320); }
        else if (id >= 330 && id <= 335) { for (auto t : session.getTracks()) if (st.isTrackSelected (t)) session.setElasticMode (*t, id - 330); }
        else if (id == 340) saveTrackPresetDialog();
        else if (id == 300) { TrackOps::NewSpec sp; juce::String note; TrackOps::createTracks (session, sp, nullptr, note); }

        // ---- Clip
        else if (id >= 400 && id < 500) clipMenu (id);

        // ---- Event
        else if (id == 521) toggleWindow ("memory", true);
        else if (id >= 500 && id < 600) eventMenu (id);

        // ---- AudioSuite
        else if (id >= 600 && id <= 605) audioSuiteMenu (id);
        else if (id == 608) openMolt();

        // ---- Options
        else if (id == 700) transportCtl.setLoopPlayback (! session.edit->getTransport().looping.get());
        else if (id == 701) transportCtl.setLoopRecord (! Prefs::loopRecord());
        else if (id == 702) transportCtl.setQuickPunch (! Prefs::quickPunch());
        else if (id == 703) transportCtl.setPreRoll (! Prefs::preRollEnabled());
        else if (id == 704) transportCtl.setPostRoll (! Prefs::postRollEnabled());
        else if (id == 705) transportCtl.setCountOff (Prefs::countOffBars() ? 0 : 2);
        else if (id == 706) transportCtl.setClick (! session.edit->clickTrackEnabled.get());
        else if (id == 707) transportCtl.setMidiMerge (! Prefs::midiMerge());
        else if (id == 708) transportCtl.setWaitForNote (! Prefs::waitForNote());
        else if (id == 709) Prefs::set ("mi_thru", ! Prefs::midiThru());
        else if (id == 710) Prefs::set ("op_linkTimelineEdit", ! Prefs::linkTimelineAndEdit());
        else if (id == 711) st.linkTrackAndEditSelection = ! st.linkTrackAndEditSelection;
        else if (id == 712) Prefs::set ("op_returnToStart", ! Prefs::returnToStartOnStop());
        else if (id == 713) Prefs::set ("op_scrollMode", 1);
        else if (id == 741) Prefs::set ("op_scrollMode", 2);
        else if (id == 742) Prefs::set ("op_scrollMode", 0);
        else if (id == 714) { st.tabToTransients = ! st.tabToTransients; timeline.repaint(); }
        else if (id == 715) session.groupsEnabled = ! session.groupsEnabled;
        else if (id == 716) Prefs::set ("mx_soloLatch", ! Prefs::soloLatching());
        else if (id == 717) Prefs::set ("ed_autoFades", ! Prefs::autoFadesOnEdits());
        else if (id == 718) toggleWindow ("eqchart", true);
        else if (id == 719 || id == 919) toggleWindow ("mastering", true);

        // ---- Setup
        else if (id == 800 || id == 911) toggleWindow ("engine", true);
        else if (id == 921) toggleWindow ("usage", true);
        else if (id == 837) CrashGuard::reportFile().revealToUser();
        else if (id == 838) { CrashGuard::state().safeMode = false; reloadSessionForPlugins ("Safe Mode is off: third-party plug-ins are loading."); }
        else if (id >= 840 && id < 880)
        {
            auto list = CrashGuard::disabledPlugins();
            if (id - 840 < list.size())
            {
                auto name = list[id - 840];
                CrashGuard::setPluginSwitchedOff (name, false);
                reloadSessionForPlugins (name + " is on again.");
            }
        }
        else if (id == 801 || id == 910) toggleWindow ("io", true);
        else if (id == 802 || id == 909) toggleWindow ("session", true);
        else if (id == 803) { scanner.startScan (false); session.log.add ("You", "Plugin scan started"); toggleWindow ("plugins", true); }
        else if (id == 804) { scanner.startScan (true); session.log.add ("You", "Full plugin rescan started"); toggleWindow ("plugins", true); }
        else if (id == 805 || id == 912) toggleWindow ("plugins", true);
        else if (id == 806 || id == 913) toggleWindow ("preferences", true);

        // ---- Window
        else if (id == 902) toggleWindow ("transport");
        else if (id == 903) toggleWindow ("bigcounter");
        else if (id == 904) toggleWindow ("memory");
        else if (id == 905) toggleWindow ("cliplist");
        else if (id == 906) toggleWindow ("groups");
        else if (id == 907) toggleWindow ("undo");
        else if (id == 908) toggleWindow ("colors");
        else if (id == 914) { if (auto mc = dynamic_cast<te::MidiClip*> (st.selectedClip.get())) openPianoRoll (*mc); else Ui::message (juce::MessageBoxIconType::InfoIcon, "MIDI Editor", "Select a MIDI clip first (or double-click one)."); }
        else if (id == 915) { if (auto sc = dynamic_cast<te::StepClip*> (st.selectedClip.get())) openStepGrid (*sc); else Ui::message (juce::MessageBoxIconType::InfoIcon, "Beat Maker", "Select a drum (beat) clip first (or double-click one)."); }
        else if (id == 917 || id == 75) toggleWindow ("welcome");
        else if (id == 918) closeAllWindows();
        else if (id == 920) toggleWindow ("eqchart", true);
        else if (id == 922) toggleWindow ("snapshots", true);
        else if (id == 923) toggleWindow ("rack", true);
        else if (id == 924) openCutLabGated ({});

        // ---- Claude
        else if (id == 60) { tunnel.isActive() ? tunnel.stopTunnel() : tunnel.startTunnel (server.getPort()); showView ("claude"); }
        else if (id == 61) juce::SystemClipboard::copyTextToClipboard (claude.getConnectorUrl());
        else if (id == 62) juce::URL (ClaudePanel::connectorsPage).launchInDefaultBrowser();
        else if (id == 63) { showView ("claude"); claude.runSelfTest(); }
        else if (id == 64) { juce::SystemClipboard::copyTextToClipboard (claude.diagnosticsText()); Ui::message (juce::MessageBoxIconType::InfoIcon, "Diagnostics copied", "Paste them into your chat with Claude and it will tell you what to fix."); }

        // ---- Help
        else if (id == 70)
            Ui::message (juce::MessageBoxIconType::InfoIcon, "Squelchmoth " + juce::String (JUCE_APPLICATION_VERSION_STRING),
                "Created and owned by Jamar Stewart.\n(c) 2026 Jamar Stewart. All rights reserved.\n\n"
                "Powered by Tracktion Engine.  Made with JUCE.\n"
                "Includes Signalsmith Stretch, the AirWindows effects and other open-source components - see Help > Third-Party Notices.");
        else if (id == 77) openBundledDoc ("license.txt");
        else if (id == 78) openBundledDoc ("PRIVACY.txt");
        else if (id == 79) openBundledDoc ("THIRD-PARTY-NOTICES.txt");
        else if (id == 80) launchUninstaller();
        else if (id == 71) session.sessionsDir.revealToUser();
        else if (id == 72) showUnlock ({});
        else if (id == 73) juce::URL (Edition::storeUrl ("CORE")).launchInDefaultBrowser();
        else if (id == 74) showShortcuts();
        else if (id == 76)
        {
            auto f = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getChildFile ("Contents/Resources/SETUP.md");
            if (! f.existsAsFile()) f = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getSiblingFile ("SETUP.md");
            if (f.existsAsFile()) f.startAsProcess(); else juce::URL ("https://claude.ai/artifact/8CLN6X5QQBg9FaebVfeVJX").launchInDefaultBrowser();
        }
        else if (id > quickNewBase && id < quickNewBase + 100) runQuickNew (id);
        else if (PluginMenu::isSearch (id))
        {
            auto f = PluginMenu::searchFilter (id);
            PluginMenu::showSearch (session.getEngine(), f, true, [this, f] (int pid)
            {
                if (f == PluginMenu::instruments) runQuickNew (pid);
                else if (auto p = PluginMenu::createFromResult (session, pid)) timeline.getCanvas().audioSuitePlugin (p);
            });
        }
        else if (auto p = PluginMenu::createFromResult (session, id))
        {
            // Track > Quick New > Stereo Instrument Track with...  or AudioSuite > Render through plugin...
            if (p->isSynth()) runQuickNew (id);
            else timeline.getCanvas().audioSuitePlugin (p);
        }
        else
        {
            lastMenuHandled = false;
            session.log.add ("Squelchmoth", "Menu item " + juce::String (id) + " has no handler", false);
        }
        menuItemsChanged();
        repaintViews();
    }

    bool lastMenuHandled = true;

    //==========================================================================
    juce::Array<te::Track*> selectedRows()
    {
        juce::Array<te::Track*> out;
        if (session.edit == nullptr) return out;
        for (auto t : session.getRows()) if (timeline.state.isTrackSelected (t)) out.add (t);
        if (auto m = session.edit->getMasterTrack()) if (timeline.state.isTrackSelected (m)) out.add (m);
        return out;
    }

    juce::Array<te::AudioTrack*> selectedAudio()
    {
        juce::Array<te::AudioTrack*> out;
        for (auto t : selectedRows()) if (auto at = dynamic_cast<te::AudioTrack*> (t)) out.add (at);
        return out;
    }

    /** Plug-ins that were not loaded (Safe Mode / switched off after a crash) load when the session is reopened. */
    void reloadSessionForPlugins (const juce::String& note)
    {
        auto f = session.getFile();
        if (session.edit != nullptr && session.savingAllowed()) session.save();
        if (f.existsAsFile()) session.openSession (f);
        session.log.add ("Squelchmoth", note);
        menuItemsChanged();
    }

    /** Plug-ins a session uses that are not on the plug-in list yet are found (incremental scan) and loaded
        automatically - no manual rescan needed. */
    void findMissingPlugins()
    {
        if (session.edit == nullptr) return;
        int missing = 0;
        for (auto p : te::getAllPlugins (*session.edit, false))
            if (auto ep = dynamic_cast<te::ExternalPlugin*> (p)) if (ep->isMissing() && ! ep->isDisabled()) ++missing;
        if (missing == 0) return;
        session.log.add ("Squelchmoth", juce::String (missing) + " plug-in(s) in this session are not on the plug-in list yet - finding them now");
        scanner.runAfterScan ([this, sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (sp == nullptr || session.edit == nullptr) return;
            int loaded = 0, still = 0;
            for (auto p : te::getAllPlugins (*session.edit, false))
                if (auto ep = dynamic_cast<te::ExternalPlugin*> (p))
                    if (ep->isMissing() && ! ep->isDisabled())
                    {
                        ep->forceFullReinitialise();
                        if (ep->isMissing()) ++still; else ++loaded;
                    }
            if (loaded > 0) session.log.add ("Squelchmoth", "Loaded " + juce::String (loaded) + " plug-in(s) the session needed");
            if (still > 0) session.log.add ("Squelchmoth", juce::String (still) + " plug-in(s) in this session are not installed on this computer", false);
            timeline.repaint(); mixer.repaint();
        });
        scanner.startScan (false);
    }

    /** Pro Tools Output window for a track (clicking another track's button re-targets the open window). */
    void showOutputWindow (te::Track* t)
    {
        if (t == nullptr) return;
        auto w = findWindow ("output");
        if (w == nullptr) w = createWindow ("output");
        if (w == nullptr) return;
        if (auto c = dynamic_cast<OutputWindowContent*> (w->getContentComponent())) c->showTrack (t);
        w->showAndRestore (this);
        menuItemsChanged();
    }

    static constexpr int quickNewBase = 4000;

    /** Track > Quick New, the + TRACK right-click menu and the right-click "New" menus all end up here. */
    void runQuickNew (int id, int base = quickNewBase)
    {
        te::Track* after = nullptr;
        for (auto t : selectedRows()) if (! t->isMasterTrack()) after = t;
        auto made = QuickNew::handle (session, id, after, [this] { newTracksDialog(); }, [this] (te::StepClip& sc) { openStepGrid (sc); }, base);
        if (! made.isEmpty()) { timeline.state.selectTrack (made.getLast()); timeline.scrollToTrack (made.getLast()); }
        timeline.repaint(); mixer.repaint();
    }

    void newTracksDialog()
    {
        te::Track* after = nullptr;
        auto sel = selectedRows();
        for (auto t : sel) if (! t->isMasterTrack()) after = t;
        NewTracksContent::show (session, after, [this] (const juce::Array<te::Track*>& made)
        {
            if (! made.isEmpty()) { timeline.state.selectTrack (made.getLast()); timeline.scrollToTrack (made.getLast()); }
        });
    }

    void scrollToTrackDialog()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Scroll to Track", "Track number or name:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("t", "1");
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            auto txt = aw->getTextEditorContents ("t").trim();
            te::Track* found = nullptr;
            auto rows = session.getRows();
            if (txt.containsOnly ("0123456789") && txt.isNotEmpty()) found = rows[txt.getIntValue() - 1];
            if (found == nullptr) for (auto t : rows) if (t->getName().equalsIgnoreCase (txt)) found = t;
            if (found == nullptr) for (auto t : rows) if (t->getName().containsIgnoreCase (txt)) found = t;
            if (found == nullptr) { Ui::message (juce::MessageBoxIconType::InfoIcon, "Scroll to Track", "No track matches \"" + txt + "\""); return; }
            if (found->isHidden()) found->setHidden (false);
            timeline.state.selectTrack (found);
            showView ("edit");
            timeline.scrollToTrack (found);
            timeline.repaint();
        }));
    }

    //==========================================================================
    // Menu introspection: lets Claude (menu_command / menu_list) and the self-test drive the exact same
    // menu commands the user clicks, and gives the activity log readable names.
    bool inWindowMenuBarWanted() const
    {
       #if JUCE_MAC
        return Prefs::getBool ("ui_menuBarInWindow", false);   // the Mac menu bar at the top of the screen, like Pro Tools
       #else
        return true;
       #endif
    }

    static juce::String cleanMenuText (const juce::String& t)
    {
        auto s = t;
        auto p = s.indexOf ("  (");                    // strip the "  (Cmd+X)" shortcut hint
        if (p > 0) s = s.substring (0, p);
        return s.trim();
    }

    static bool findMenuItem (const juce::PopupMenu& menu, int id, juce::String& pathOut)
    {
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            auto& item = it.getItem();
            if (item.subMenu != nullptr)
            {
                juce::String sub;
                if (findMenuItem (*item.subMenu, id, sub)) { pathOut = cleanMenuText (item.text) + " > " + sub; return true; }
            }
            else if (item.itemID == id && ! item.isSeparator) { pathOut = cleanMenuText (item.text); return true; }
        }
        return false;
    }

    juce::String menuItemPath (int id, int topLevelIndex)
    {
        auto names = getMenuBarNames();
        juce::String path;
        if (juce::isPositiveAndBelow (topLevelIndex, names.size()) && findMenuItem (getMenuForIndex (topLevelIndex, names[topLevelIndex]), id, path))
            return names[topLevelIndex] + " > " + path;
        for (int i = 0; i < names.size(); ++i)
            if (findMenuItem (getMenuForIndex (i, names[i]), id, path))
                return names[i] + " > " + path;
        return "item " + juce::String (id);
    }

    static void appendMenuTree (const juce::PopupMenu& menu, const juce::String& prefix, juce::Array<juce::var>& out, bool parentEnabled = true)
    {
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            auto& item = it.getItem();
            if (item.isSeparator || item.isSectionHeader) continue;
            auto text = cleanMenuText (item.text);
            if (item.subMenu != nullptr) { appendMenuTree (*item.subMenu, prefix + text + " > ", out, parentEnabled && item.isEnabled); continue; }
            auto o = new juce::DynamicObject();
            o->setProperty ("path", prefix + text);
            o->setProperty ("enabled", parentEnabled && item.isEnabled);
            if (item.isTicked) o->setProperty ("checked", true);
            out.add (juce::var (o));
        }
    }

    juce::var menuTree()
    {
        juce::Array<juce::var> items;
        auto names = getMenuBarNames();
        for (int i = 0; i < names.size(); ++i)
            appendMenuTree (getMenuForIndex (i, names[i]), names[i] + " > ", items);
        return items;
    }

    /** Runs a command by its menu path, e.g. "Window > Transport" or "Edit > Edit Mode > Grid". Matching
        is case-insensitive and each segment may be a prefix ("win > trans"). Returns an error text or {}. */
    juce::String runMenuPath (const juce::String& path)
    {
        auto segs = juce::StringArray::fromTokens (path, ">", "");
        segs.trim(); segs.removeEmptyStrings();
        if (segs.isEmpty()) return "Give a menu path like \"Window > Transport\"";
        auto names = getMenuBarNames();
        int top = -1;
        for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (segs[0])) top = i;
        if (top < 0) for (int i = 0; i < names.size(); ++i) if (names[i].startsWithIgnoreCase (segs[0])) top = i;
        if (top < 0) return "No menu called \"" + segs[0] + "\". Menus: " + names.joinIntoString (", ");

        juce::PopupMenu menu = getMenuForIndex (top, names[top]);
        const juce::PopupMenu::Item* found = nullptr;
        juce::String foundPath = names[top];
        for (int depth = 1; depth < segs.size(); ++depth)
        {
            const juce::PopupMenu::Item* exact = nullptr; const juce::PopupMenu::Item* prefix = nullptr;
            for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
            {
                auto& item = it.getItem();
                if (item.isSeparator || item.isSectionHeader) continue;
                auto text = cleanMenuText (item.text);
                if (text.equalsIgnoreCase (segs[depth]) && exact == nullptr) exact = &item;
                else if (text.startsWithIgnoreCase (segs[depth]) && prefix == nullptr) prefix = &item;
            }
            auto hit = exact != nullptr ? exact : prefix;
            if (hit == nullptr) return "Nothing called \"" + segs[depth] + "\" under " + foundPath;
            foundPath += " > " + cleanMenuText (hit->text);
            if (! hit->isEnabled) return foundPath + " is greyed out right now (it needs a selection, or is not available)";
            if (hit->subMenu != nullptr && depth + 1 < segs.size()) { juce::PopupMenu sub (*hit->subMenu); menu = sub; continue; }
            if (hit->subMenu != nullptr) return foundPath + " is a sub-menu - add one of its items to the path";
            found = hit;
            break;
        }
        if (found == nullptr) return "\"" + path + "\" names a menu, not a command";
        if (! found->isEnabled) return foundPath + " is greyed out right now (it needs a selection, or is not available)";
        const int id = found->itemID;
        if (id == 80) return "Uninstalling is something only you can do - Claude never uninstalls Squelchmoth. Use Help > Uninstall Squelchmoth... yourself.";
        menuItemSelected (id, top);
        if (! lastMenuHandled) return foundPath + " has no handler (please report this)";
        return {};
    }

    //==========================================================================
    /** Window registry: name -> floating window, created on demand. */
    SqWindow* findWindow (const juce::String& name)
    {
        auto it = windows.find (name);
        return it != windows.end() ? it->second.get() : nullptr;
    }

    bool isWindowVisible (const juce::String& name)   { auto w = findWindow (name); return w != nullptr && w->isVisible(); }

    void toggleWindow (const juce::String& name, std::optional<bool> show = {})
    {
        auto w = findWindow (name);
        if (w == nullptr) w = createWindow (name);
        if (w == nullptr) return;
        // Toggle like Pro Tools' Window menu - but a window that is open and merely sitting behind another
        // one is brought to the front instead of being closed.
        bool target = show.has_value() ? *show
                                       : ! (w->isVisible() && w->isShowing() && (w->isFloating() || w->isActiveWindow()));
        if (target) w->showAndRestore (this);
        else w->closeButtonPressed();
        menuItemsChanged();
    }

    void closeAllWindows()
    {
        for (auto& [n, w] : windows) if (w->isVisible()) w->closeButtonPressed();
    }

private:
    Session& session;
    McpTools& tools;
    McpServer& server;
    Tunnel& tunnel;
    PluginScanner& scanner;
    juce::PropertiesFile& props;
    Licence& licence;
    SquelchmothLookAndFeel lnf;

    TimelineView timeline;
    TransportController transportCtl;
    TransportBar transport;
    MixerView mixer;
    ClaudePanel claude;
    BeatPage beat;
    MasteringProject mastering;
    juce::Label status;
    Perf::Monitor perfMonitor { session };
    juce::String perfStatusText;
    juce::int64 perfStatusTime = 0;
    juce::String currentView;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<PianoRollWindow>> pianoRolls;
    std::vector<std::unique_ptr<TuneGraph::Window>> tuneGraphs;
    std::unique_ptr<AudioEditor::Window> cutLab;
    std::unique_ptr<Molt::Window> moltWindow;
    std::vector<std::unique_ptr<StepGridWindow>> stepGrids;
    std::map<juce::String, std::unique_ptr<SqWindow>> windows;
    juce::TooltipWindow tooltips { this, 600 };

    SqWindow* createWindow (const juce::String& name)
    {
        std::unique_ptr<SqWindow> w;
        auto& st = timeline.state;
        auto repaintViews = [this] { timeline.repaint(); mixer.repaint(); };

        if (name == "transport")      { w = std::make_unique<SqWindow> ("Transport", name, false); w->setContentOwned (new TransportWindowContent (session, st, transportCtl), true); }
        else if (name == "bigcounter"){ w = std::make_unique<SqWindow> ("Big Counter", name); w->setContentOwned (new BigCounterContent (session), true); }
        else if (name == "memory")
        {
            auto c = new MemoryLocationsContent (session, st);
            c->onNew = [this] { timeline.addMemoryLocationAtPlayhead(); };
            c->onEdit = [this] (te::MarkerClip& m) { timeline.editMarker (m); };
            c->onChanged = repaintViews;
            w = std::make_unique<SqWindow> ("Memory Locations", name); w->setContentOwned (c, true);
        }
        else if (name == "cliplist")  { auto c = new ClipListContent (session, st); c->onChanged = repaintViews; w = std::make_unique<SqWindow> ("Clip List", name); w->setContentOwned (c, true); }
        else if (name == "groups")    { auto c = new GroupsContent (session, st); c->onChanged = repaintViews; w = std::make_unique<SqWindow> ("Groups", name); w->setContentOwned (c, true); }
        else if (name == "undo")      { w = std::make_unique<SqWindow> ("Undo History", name); w->setContentOwned (new UndoHistoryContent (session), true); }
        else if (name == "colors")
        {
            auto c = new ColorPaletteContent (session, st);
            c->onChanged = repaintViews;
            c->onEditPalette = [this] { menuItemSelected (229, 0); };
            w = std::make_unique<SqWindow> ("Color Palette", name); w->setContentOwned (c, true);
        }
        else if (name == "snapshots") { w = std::make_unique<SqWindow> ("Mix Snapshots", name); w->setContentOwned (new MixSnapshotsContent (session), true); }
        else if (name == "rack")
        {
            auto c = new ChannelRackComponent (session);
            wireRack (*c);
            c->setSize (1180, 560);
            w = std::make_unique<SqWindow> ("Channel Rack", name); w->setContentOwned (c, true); w->setFloating (true);
            rackWindowComp = c;
            beat.setRack (c);
        }
        else if (name == "backups")
        {
            auto c = new BackupBrowser (session);
            c->onDone = [this] { toggleWindow ("backups", false); };
            w = std::make_unique<SqWindow> ("Restore from Backup", name); w->setContentOwned (c, true);
        }
        else if (name == "session")   { auto c = new SessionSetupContent (session); c->onShowAudioSettings = [this] { toggleWindow ("engine", true); }; w = std::make_unique<SqWindow> ("Session Setup", name); w->setContentOwned (c, true); }
        else if (name == "io")        { auto c = new IOSetupContent (session); c->onShowAudioSettings = [this] { toggleWindow ("engine", true); }; w = std::make_unique<SqWindow> ("I/O Setup", name); w->setContentOwned (c, true); }
        else if (name == "engine")
        {
            auto c = new Perf::PlaybackEngineContent (session, perfMonitor);
            c->onShowUsage = [this] { toggleWindow ("usage", true); };
            w = std::make_unique<SqWindow> ("Playback Engine", name); w->setContentOwned (c, true);
        }
        else if (name == "output")
        {
            auto c = new OutputWindowContent (session, &timeline.state);
            w = std::make_unique<SqWindow> ("Output", name); w->setContentOwned (c, true);
        }
        else if (name == "usage")
        {
            auto c = new Perf::SystemUsageContent (session, perfMonitor);
            c->onShowEngine = [this] { toggleWindow ("engine", true); };
            w = std::make_unique<SqWindow> ("System Usage", name); w->setContentOwned (c, true);
        }
        else if (name == "plugins")   { w = std::make_unique<SqWindow> ("Plugins", name); w->setContentOwned (new PluginsContent (session, scanner), true); }
        else if (name == "preferences") { w = std::make_unique<SqWindow> ("Preferences", name); w->setContentOwned (new PreferencesContent (session), true); }
        else if (name == "eqchart")   { w = std::make_unique<SqWindow> ("EQ Frequency Chart", name); w->setContentOwned (new EQChartContent(), true); }
        else if (name == "unlock")
        {
            auto c = new UnlockContent();
            c->onActivate = [this] (const juce::String& k) { auto e = licence.activate (k); if (e.isEmpty()) { session.log.add ("Squelchmoth", "Activated: " + licence.statusText()); menuItemsChanged(); repaint(); } return e; };
            c->ownedKeys = [this] { juce::StringArray a; for (auto& k : licence.keys()) a.add (k.product + (k.serial.isNotEmpty() ? " " + k.serial : juce::String())); return a; };
            c->onReopen = [this] { reopenSessionForUnlock(); };
            w = std::make_unique<SqWindow> ("Licences and Upgrades - Squelchmoth", name, false); w->setContentOwned (c, true);
        }
        else if (name == "mastering" && ! Edition::allows (Edition::Feature::mastering)) { juce::MessageManager::callAsync ([this] { showUnlock ("CORE"); }); return nullptr; }
        else if (name == "mastering") { w = std::make_unique<SqWindow> ("Mastering - Squelchmoth", name); w->setContentOwned (new MasteringContent (mastering), true); w->setFloating (false); }
        else if (name == "welcome")
        {
            auto c = new WelcomeContent();
            c->onNew = [this] { toggleWindow ("welcome", false); newSessionDialog (false); };
            c->onOpen = [this] { toggleWindow ("welcome", false); menuItemSelected (2, 0); };
            c->onTemplate = [this] { toggleWindow ("welcome", false); newSessionDialog (true); };
            c->onClaude = [this] { toggleWindow ("welcome", false); if (! tunnel.isActive()) tunnel.startTunnel (server.getPort()); showView ("claude"); };
            c->onTour = [this] { toggleWindow ("welcome", false); showTour(); };
            c->onBeat = [this] { toggleWindow ("welcome", false); makeABeat(); };
            w = std::make_unique<SqWindow> ("Welcome to Squelchmoth", name, false); w->setContentOwned (c, true);
        }
        if (w == nullptr) return nullptr;
        w->onHidden = [this] { menuItemsChanged(); };
        auto* raw = w.get();
        windows[name] = std::move (w);
        return raw;
    }

    //==========================================================================
    void changeListenerCallback (juce::ChangeBroadcaster* b) override
    {
        if (b == &Theme::broadcaster())
        {
            lnf.refresh();
            sendLookAndFeelChange();
            status.setColour (juce::Label::textColourId, Theme::dim());
            repaint();
            for (auto& [n, w] : windows) { w->setBackgroundColour (Theme::panel()); w->sendLookAndFeelChange(); w->repaint(); if (auto c = w->getContentComponent()) c->repaint(); }
            if (auto top = getTopLevelComponent()) if (auto dw = dynamic_cast<juce::DocumentWindow*> (top)) dw->setBackgroundColour (Theme::bg());
            return;
        }
        if (b == &session.getEngine().getDeviceManager())
        {
            // interface changed / re-laid out: keep the Pro Tools path names and the tracks' chosen paths
            IO::applyNames (session.getEngine());
            juce::Timer::callAfterDelay (300, [this, sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp) IO::restoreInputs (session); });
            return;
        }
        pianoRolls.clear();   // the Edit was replaced
        tuneGraphs.clear();
        moltWindow.reset();
        juce::Timer::callAfterDelay (300, [this, sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp) IO::restoreInputs (session); });
        juce::Timer::callAfterDelay (1200, [this, sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp) findMissingPlugins(); });
        stepGrids.clear();
        for (auto& [n, w] : windows) if (n == "memory" || n == "cliplist" || n == "groups" || n == "undo" || n == "session" || n == "io") { w->setVisible (false); }
        windows.erase ("memory"); windows.erase ("cliplist"); windows.erase ("groups"); windows.erase ("undo"); windows.erase ("session"); windows.erase ("io"); windows.erase ("colors"); windows.erase ("transport"); windows.erase ("bigcounter");
        if (auto ow = findWindow ("output")) ow->setVisible (false);
        windows.erase ("output");   // it points at a track of the session that just closed
        if (auto w = findParentComponentOfClass<juce::DocumentWindow>())
            w->setName ("Squelchmoth - " + session.getFile().getFileNameWithoutExtension());
    }

    void timerCallback() override
    {
        // MIDI Thru follows the first selected MIDI / instrument track (only re-routed while stopped)
        if (session.edit != nullptr && ! session.edit->getTransport().isPlaying())
        {
            te::AudioTrack* thru = nullptr;
            for (auto t : selectedRows())
                if (auto at = dynamic_cast<te::AudioTrack*> (t))
                    if (IO::kindOf (*at) == IO::Kind::instrument || IO::kindOf (*at) == IO::Kind::midi) { thru = at; break; }
            if (thru != nullptr || selectedRows().isEmpty()) TrackOps::updateMidiThruTarget (session, thru);
        }
        juce::String s;
        s << session.getFile().getFileNameWithoutExtension();
        if (auto dev = session.getEngine().getDeviceManager().deviceManager.getCurrentAudioDevice())
            s << "   |   " << dev->getName() << "  " << juce::String (dev->getCurrentSampleRate() / 1000.0, 1) << " kHz / "
              << (session.recordBitDepth() == 32 ? juce::String ("32-bit float") : juce::String (session.recordBitDepth()) + "-bit")
              << " / " << dev->getCurrentBufferSizeSamples() << " samples";
        else
            s << "   |   No audio device - Setup > Playback Engine";
        s << "   |   Plugins: " << scanner.getStatus();
        s << "   |   Claude: " << (tunnel.isActive() ? tunnel.getState() : juce::String ("off")) << " (" << server.getRequestCount() << " requests)";
        s << "   |   " << licence.statusText();
        s << "   |   (c) 2026 Jamar Stewart";
        if (CrashGuard::state().safeMode) s = "SAFE MODE - third-party plug-ins are off (Setup > Leave Safe Mode)   |   " + s;
        const bool perfRecent = perfStatusText.isNotEmpty() && juce::Time::currentTimeMillis() - perfStatusTime < 12000;
        if (perfRecent) s = perfStatusText + "   |   " + s;
        status.setColour (juce::Label::textColourId, perfRecent || CrashGuard::state().safeMode ? Theme::amber() : Theme::dim());
        status.setText (s, juce::dontSendNotification);

        pianoRolls.erase (std::remove_if (pianoRolls.begin(), pianoRolls.end(),
                                          [] (auto& w) { return w->clipRef == nullptr || w->clipRef->getClipTrack() == nullptr; }),
                          pianoRolls.end());
        stepGrids.erase (std::remove_if (stepGrids.begin(), stepGrids.end(),
                                         [] (auto& w) { return w->clipRef == nullptr || w->clipRef->getClipTrack() == nullptr; }),
                         stepGrids.end());
    }

    ChannelRackComponent* rackWindowComp = nullptr;
    void wireRack (ChannelRackComponent& c)
    {
        c.onOpenInstrument = [] (te::Plugin& p) { p.showWindowExplicitly(); };
        c.onOpenPianoRoll = [this] (te::MidiClip& mc) { openPianoRoll (mc); };
        c.onOpenChannelSettings = [] (te::AudioTrack& t) { if (auto p = Rack::instrumentOf (t)) p->showWindowExplicitly(); };
        c.onClaude = [this] { showView ("claude"); };
        c.onSelectTrack = [this] (te::AudioTrack& t) { timeline.state.selectTrack (&t, false); timeline.repaint(); mixer.repaint(); };
    }
    void refreshRack()   { if (rackWindowComp != nullptr) rackWindowComp->refresh(); }

    void openStepGrid (te::StepClip& c)
    {
        for (auto& w : stepGrids)
            if (w->clipRef.get() == &c) { w->toFront (true); return; }
        auto w = std::make_unique<StepGridWindow> (c);
        auto* raw = w.get();
        w->onKitChange = [this, raw] (int kit) { if (auto t = dynamic_cast<te::AudioTrack*> (raw->clipRef != nullptr ? raw->clipRef->getTrack() : nullptr)) { DrumKit::loadKit (session, *t, kit); session.log.add ("You", "Kit: " + juce::String (DrumKit::kits[kit].name) + " on " + t->getName()); } };
        w->onOpenPianoRoll = [this] (te::MidiClip& mc) { openPianoRoll (mc); };
        w->onClose = [this, raw]
        {
            juce::MessageManager::callAsync ([this, raw]
            {
                stepGrids.erase (std::remove_if (stepGrids.begin(), stepGrids.end(), [raw] (auto& p) { return p.get() == raw; }), stepGrids.end());
            });
        };
        stepGrids.push_back (std::move (w));
    }

    /** Mandible: one note per slice into a new MIDI clip on the slicer's track, from the edit cursor's bar. */
    void slicerDumpToPianoRoll (te::Plugin& p, int deck)
    {
        auto mp = dynamic_cast<SqSynth::SqMandiblePlugin*> (&p); auto t = dynamic_cast<te::AudioTrack*> (p.getOwnerTrack());
        if (mp == nullptr || t == nullptr) return;
        auto beats = mp->sliceBeats (deck); if (beats.isEmpty()) { session.log.add ("Squelchmoth", "Mandible: load a loop first"); return; }
        const double totalBeats = (double) mp->decks[(size_t) deck].getProperty ("bars", 1) * 4.0;
        auto start = session.edit->getTransport().getPosition(); auto bar = std::floor (session.timeToBar (start));
        auto t0 = session.barToTime (bar), t1 = session.barToTime (bar + std::ceil (totalBeats / 4.0));
        auto clip = t->insertMIDIClip ("Slices " + juce::String (deck == 0 ? "A" : "B"), { t0, t1 }, nullptr); if (clip == nullptr) return;
        auto& seq = clip->getSequence(); auto* um = &session.edit->getUndoManager();
        for (int i = 0; i < beats.size(); ++i) seq.addNote (SqSynth::SqMandiblePlugin::deckBaseNote[deck] + i, te::BeatPosition::fromBeats (beats[i]), te::BeatDuration::fromBeats (juce::jmax (0.05, (i + 1 < beats.size() ? beats[i + 1] : totalBeats) - beats[i] + 0.02)), 127, 0, um);
        if (auto* ap = p.getAutomatableParameters()[deck == 0 ? SqSynth::SqMandiblePlugin::aStretch : SqSynth::SqMandiblePlugin::bStretch]) ap->setParameter (1.0f, juce::sendNotification);
        session.log.add ("Squelchmoth", "Mandible: " + juce::String (beats.size()) + " slices to the piano roll at bar " + juce::String ((int) bar));
    }
    /** Mandible: one rack sample channel per slice. */
    void slicerToRackChannels (te::Plugin& p, int deck)
    {
        auto mp = dynamic_cast<SqSynth::SqMandiblePlugin*> (&p); if (mp == nullptr) return;
        auto smp = mp->deckSample (deck); if (smp == nullptr) return;
        auto f = mp->deckFile (deck); auto dir = f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension() + " slices"); dir.createDirectory();
        juce::WavAudioFormat wav; int made = 0;
        for (int i = 0; i < mp->numSlices (deck); ++i)
        {
            auto s = mp->slice (deck, i); const int st = (int) s.getProperty ("start"), en = (int) s.getProperty ("end");
            auto out = dir.getChildFile ("slice " + juce::String (i + 1).paddedLeft ('0', 2) + ".wav"); out.deleteFile();
            if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.release(), smp->sampleRate, (unsigned) smp->buffer.getNumChannels(), 24, {}, 0)))
                    writer->writeFromAudioSampleBuffer (smp->buffer, st, en - st);
            if (Rack::addSampleChannel (session, out, f.getFileNameWithoutExtension() + " " + juce::String (i + 1)) != nullptr) ++made;
        }
        session.log.add ("Squelchmoth", "Mandible: " + juce::String (made) + " slices became rack channels");
    }
    /** Cut Lab (the audio editor): one window; opens a file, or the audio of a clip. */
    AudioEditor::Window* openCutLabGated (const juce::File& f)
    {
        if (! Edition::allows (Edition::Feature::cutLab)) { showUnlock ("CORE"); return nullptr; }
        return &openCutLab (f);
    }
    AudioEditor::Window& openCutLab (const juce::File& f)
    {
        if (cutLab == nullptr)
        {
            cutLab = std::make_unique<AudioEditor::Window> (session, f);
            cutLab->targetTrack = [this] { return timeline.state.selectedTrack; };
            cutLab->onClose = [this] { juce::MessageManager::callAsync ([this] { cutLab.reset(); }); };
        }
        else { cutLab->toFront (true); if (f.existsAsFile()) cutLab->open (f); }
        return *cutLab;
    }
    void openCutLabForClip (te::Clip& c)
    {
        if (auto ac = dynamic_cast<te::WaveAudioClip*> (&c)) openCutLab (ac->getSourceFileReference().getFile());
    }
    void openTuneGraph (te::Plugin& p)
    {
        auto tu = dynamic_cast<SqTunePlugin*> (&p);
        if (tu == nullptr) return;
        for (auto& w : tuneGraphs)
            if (w->pluginRef.get() == &p) { w->toFront (true); return; }
        auto w = std::make_unique<TuneGraph::Window> (*tu);
        auto* raw = w.get();
        w->onClose = [this, raw]
        {
            juce::MessageManager::callAsync ([this, raw]
            {
                tuneGraphs.erase (std::remove_if (tuneGraphs.begin(), tuneGraphs.end(), [raw] (auto& q) { return q.get() == raw; }), tuneGraphs.end());
            });
        };
        tuneGraphs.push_back (std::move (w));
    }

    void openPianoRoll (te::MidiClip& c)
    {
        for (auto& w : pianoRolls)
            if (w->clipRef.get() == &c) { w->toFront (true); return; }
        auto w = std::make_unique<PianoRollWindow> (c);
        auto* raw = w.get();
        w->onClose = [this, raw]
        {
            juce::MessageManager::callAsync ([this, raw]
            {
                pianoRolls.erase (std::remove_if (pianoRolls.begin(), pianoRolls.end(), [raw] (auto& p) { return p.get() == raw; }), pianoRolls.end());
            });
        };
        pianoRolls.push_back (std::move (w));
    }

    //==========================================================================
    // Session templates
    juce::File templatesDir() const   { auto d = session.sessionsDir.getChildFile ("Templates"); d.createDirectory(); return d; }

    static juce::StringArray builtinTemplates()   { return { "Empty", "Songwriter (4 audio + synth)", "Beat Making (drums + bass + keys + 2 vocals)", "Band Recording (8 inputs + 2 busses)", "Podcast / Voice (2 mics + music bed)", "Film Score (strings, brass, perc, 2 fx busses)", "Mixing (16 audio + 4 aux)" }; }

    /** Make a Beat: the 60-second flow - a new session, an 8-channel kit in the rack, Pattern mode looping, click on, the Beat page. */
    void makeABeat (const juce::String& genre = "Trap", bool newSession = true)
    {
        if (newSession)
        {
            auto name = session.sessionsDir.getNonexistentChildFile ("Beat", ".squelch", false).getFileNameWithoutExtension();
            session.newSession (name);
        }
        const auto& genres = BeatLab::genres();
        const BeatLab::Genre* g = &genres.front();
        for (auto& gg : genres) if (juce::String (gg.name).equalsIgnoreCase (genre)) g = &gg;
        if (Rack::channels (*session.edit).isEmpty()) Rack::addKitChannels (session, g->kit, 8);
        if (auto ts = session.edit->tempoSequence.getTempo (0)) ts->setBpm ((g->bpmLo + g->bpmHi) / 2.0);
        session.edit->clickTrackEnabled = true;
        showView ("beat");
        if (auto rk = beat.getRack()) { rk->refresh(); rk->setPatternMode (true); }
        session.log.add ("Squelchmoth", "Make a Beat: " + juce::String (g->name) + " kit, pattern looping - click steps, Beat Starter for a groove, + CHANNEL for instruments");
    }

    void newSessionDialog (bool withTemplate)
    {
        auto aw = std::make_shared<juce::AlertWindow> ("New Session", "Name your session" + juce::String (withTemplate ? " and pick a template:" : ":"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "Untitled Session");
        juce::StringArray options = builtinTemplates();
        auto userTemplates = templatesDir().findChildFiles (juce::File::findFiles, false, "*.squelch");
        for (auto& f : userTemplates) options.add ("Template: " + f.getFileNameWithoutExtension());
        if (withTemplate) aw->addComboBox ("template", options, "Template");
        aw->addComboBox ("tempo", { "120 BPM", "90 BPM", "100 BPM", "128 BPM", "140 BPM", "150 BPM", "174 BPM", "70 BPM", "80 BPM", "110 BPM" }, "Tempo");
        {
            // Pro Tools New Session: the session sample rate (the interface is switched to it)
            const double cur = session.getEngine().getDeviceManager().getSampleRate();
            juce::StringArray rates { "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz", "176.4 kHz", "192 kHz" };
            aw->addComboBox ("rate", rates, "Sample rate");
            const double vals[] = { 44100, 48000, 88200, 96000, 176400, 192000 };
            int sel = 1;
            for (int i = 0; i < 6; ++i) if (std::abs (vals[i] - cur) < 1) sel = i;
            aw->getComboBoxComponent ("rate")->setSelectedItemIndex (sel, juce::dontSendNotification);
            // Pro Tools Create Session: bit depth of every recording in the session
            aw->addComboBox ("depth", { "16-bit", "24-bit", "32-bit float (recommended - recordings can't clip)" }, "Bit depth");
            aw->getComboBoxComponent ("depth")->setSelectedItemIndex (2, juce::dontSendNotification);
        }
        aw->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, withTemplate, userTemplates] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            auto name = aw->getTextEditorContents ("name");
            int tmpl = withTemplate ? aw->getComboBoxComponent ("template")->getSelectedItemIndex() : 1;
            auto bpm = aw->getComboBoxComponent ("tempo")->getText().getDoubleValue();
            const double rateVals[] = { 44100, 48000, 88200, 96000, 176400, 192000 };
            session.newSessionSampleRate = rateVals[juce::jlimit (0, 5, aw->getComboBoxComponent ("rate")->getSelectedItemIndex())];
            const int depthVals[] = { 16, 24, 32 };
            session.newSessionBitDepth = depthVals[juce::jlimit (0, 2, aw->getComboBoxComponent ("depth")->getSelectedItemIndex())];
            auto nBuiltin = builtinTemplates().size();
            if (tmpl >= nBuiltin)
            {
                auto f = userTemplates[tmpl - nBuiltin];
                auto dest = session.sessionsDir.getNonexistentChildFile (juce::File::createLegalFileName (name), ".squelch", false);
                f.copyFileTo (dest);
                session.openSession (dest);
                if (session.newSessionSampleRate > 0) session.setSessionSampleRate (session.newSessionSampleRate);
                if (session.newSessionBitDepth > 0) session.setRecordBitDepth (session.newSessionBitDepth);
                session.newSessionSampleRate = 0;
                session.newSessionBitDepth = 0;
            }
            else
                createFromBuiltinTemplate (name, tmpl, bpm);
            session.log.add ("You", "New session: " + name);
        }));
    }

    void createFromBuiltinTemplate (const juce::String& name, int tmpl, double bpm)
    {
        auto f = session.sessionsDir.getNonexistentChildFile (name.isNotEmpty() ? juce::File::createLegalFileName (name) : "Untitled Session", ".squelch", false);
        session.replaceEdit (te::createEmptyEdit (session.getEngine(), f), f);
        for (auto t : session.getTracks()) session.edit->deleteTrack (t);
        session.applyNewSessionDefaults();
        if (bpm >= 20) session.edit->tempoSequence.getTempo (0)->setBpm (bpm);

        switch (tmpl)
        {
            case 0: break;
            case 1: for (int i = 0; i < 4; ++i) session.addTrack ("audio", "Audio " + juce::String (i + 1)); session.addTrack ("instrument", "Synth 1", "4osc"); break;
            case 2:
                DrumKit::createDrumTrack (session, "Drums", 8);
                session.addTrack ("instrument", "808 Bass", "4osc");
                session.addTrack ("instrument", "Keys", "4osc");
                session.addTrack ("audio", "Lead Vocal"); session.addTrack ("audio", "Vocal Double");
                session.addTrack ("bus", "Vocal Reverb");
                break;
            case 3:
                for (auto n : { "Kick", "Snare", "OH L", "OH R", "Bass DI", "Guitar", "Keys", "Vocal" }) session.addTrack ("audio", n);
                session.addTrack ("bus", "Drum Bus"); session.addTrack ("bus", "Reverb");
                break;
            case 4: session.addTrack ("audio", "Host Mic"); session.addTrack ("audio", "Guest Mic"); session.addTrack ("audio", "Music Bed"); session.addTrack ("bus", "Voice Bus"); break;
            case 5:
                for (auto n : { "Strings Hi", "Strings Lo", "Brass", "Woodwinds", "Perc", "Piano", "Choir" }) session.addTrack ("instrument", n, "4osc");
                session.addTrack ("bus", "Hall Reverb"); session.addTrack ("bus", "Plate");
                break;
            case 6: for (int i = 0; i < 16; ++i) session.addTrack ("audio", "Audio " + juce::String (i + 1)); for (int i = 0; i < 4; ++i) session.addTrack ("bus", "Aux " + juce::String (i + 1)); break;
            default: break;
        }
        session.edit->getTransport().setPosition (0s);
        session.save();
    }

    void saveAsTemplate()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Save As Template", "Saves the track layout, routing, inserts and settings (clips removed) for new sessions.", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", session.getFile().getFileNameWithoutExtension() + " Template");
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            aw->setVisible (false);
            if (r != 1 || session.edit == nullptr) return;
            auto st = session.edit->state.createCopy();
            auto tmp = te::loadEditFromState (session.getEngine(), st);
            if (tmp == nullptr) return;
            te::removeAllContentClips (*tmp);
            auto f = templatesDir().getChildFile (juce::File::createLegalFileName (aw->getTextEditorContents ("name")) + ".squelch");
            te::EditFileOperations (*tmp).writeToFile (f, false);
            session.log.add ("You", "Saved template " + f.getFileNameWithoutExtension());
        }));
    }

    void saveTrackPresetDialog()
    {
        te::Track* track = nullptr;
        for (auto t : session.getRows()) if (timeline.state.isTrackSelected (t)) { track = t; break; }
        if (track == nullptr) return;
        auto aw = std::make_shared<juce::AlertWindow> ("Save Track Preset", "Saves \"" + track->getName() + "\" - inserts, sends, I/O, colour and settings (no clips).", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", track->getName());
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, trackName = track->getName()] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            auto args = juce::var (new juce::DynamicObject());
            args.getDynamicObject()->setProperty ("action", "save");
            args.getDynamicObject()->setProperty ("track", trackName);
            args.getDynamicObject()->setProperty ("name", aw->getTextEditorContents ("name"));
            auto res = tools.call ("track_preset", args);
            Ui::message (res.isError ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon, "Track preset", res.text);
        }));
    }

    void importSessionData()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import tracks from another session", session.sessionsDir, "*.squelch;*.tracktionedit");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (! f.existsAsFile() || session.edit == nullptr) return;
            auto other = te::loadEditFromFile (session.getEngine(), f);
            if (other == nullptr) return;
            int n = 0;
            for (auto t : te::getAudioTracks (*other))
            {
                auto st = t->state.createCopy();
                te::EditItemID::remapIDs (st, nullptr, *session.edit);
                session.edit->insertTrack (te::TrackInsertPoint (nullptr, te::getTopLevelTracks (*session.edit).getLast()), st, nullptr);
                ++n;
            }
            session.log.add ("You", "Imported " + juce::String (n) + " track(s) from " + f.getFileNameWithoutExtension());
        });
    }

    /** A text file shipped in the app bundle (Contents/Resources), or next to the binary / in the source docs when developing. */
    static juce::File bundledDoc (const juce::String& name)
    {
        auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        if (auto f = app.getChildFile ("Contents/Resources/" + name); f.existsAsFile()) return f;
        if (auto f = app.getSiblingFile (name); f.existsAsFile()) return f;
        // developer builds: walk up to the source tree
        for (auto d = app.getParentDirectory(); d.getParentDirectory() != d; d = d.getParentDirectory())
            for (auto sub : { "docs/legal/", "scripts/pkg/", "scripts/uninstaller/" })
                if (auto f = d.getChildFile (juce::String (sub) + name); f.existsAsFile()) return f;
        return {};
    }

    void openBundledDoc (const juce::String& name)
    {
        auto f = bundledDoc (name);
        if (! f.existsAsFile()) { Ui::message (juce::MessageBoxIconType::WarningIcon, name, "The file is missing from this copy of Squelchmoth - reinstall to restore it."); return; }
       #if JUCE_MAC
        juce::File ("/usr/bin/open").startAsProcess ("-e \"" + f.getFullPathName() + "\"");   // TextEdit
       #else
        f.startAsProcess();
       #endif
    }

    /** Help > Uninstall Squelchmoth...: runs the uninstaller (it asks for confirmation itself and quits the app first).
        Claude can never trigger this - it isn't reachable from any tool or from menu_command. */
    void launchUninstaller()
    {
       #if JUCE_MAC
        auto app = juce::File ("/Applications/Uninstall Squelchmoth.app");
        if (app.isDirectory()) { juce::File ("/usr/bin/open").startAsProcess ("\"" + app.getFullPathName() + "\""); return; }
        auto script = bundledDoc ("uninstall.sh");
        if (script.existsAsFile())
        {
            juce::ChildProcess* p = new juce::ChildProcess();   // deliberately not owned: it must outlive the app it removes
            if (p->start (juce::StringArray { "/bin/bash", script.getFullPathName() }, 0)) return;
            delete p;
        }
        Ui::message (juce::MessageBoxIconType::WarningIcon, "Uninstall Squelchmoth", "The uninstaller is missing. Drag Squelchmoth from Applications to the Trash instead.");
       #elif JUCE_WINDOWS
        // the Windows installer puts its uninstaller next to the app; it asks for confirmation itself
        auto un = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("Uninstall Squelchmoth.exe");
        if (un.existsAsFile() && un.startAsProcess()) { juce::JUCEApplication::getInstance()->systemRequestedQuit(); return; }
        Ui::message (juce::MessageBoxIconType::WarningIcon, "Uninstall Squelchmoth", "The uninstaller is missing. Use Windows Settings > Apps > Installed apps > Squelchmoth > Uninstall.");
       #else
        Ui::message (juce::MessageBoxIconType::InfoIcon, "Uninstall Squelchmoth", "To uninstall, quit Squelchmoth and delete its folder. Your sessions in Documents/Squelchmoth Sessions are not touched.");
       #endif
    }

    /** File > Bounce Mix... and File > Export Stems... (Pro Tools Bounce dialog). */
    void showBounceDialog (bool stems)
    {
        if (session.edit == nullptr) return;
        if (! session.savingAllowed()) { Ui::message (juce::MessageBoxIconType::WarningIcon, stems ? "Export Stems" : "Bounce Mix", "The 30-day trial has ended - buy a licence (Help > Buy Squelchmoth) to bounce."); return; }
        auto& st = timeline.state;
        bool anySel = false;
        for (auto t : session.getRows()) if (st.isTrackSelected (t)) anySel = true;
        std::optional<te::TimeRange> sel;
        if (st.timeSelection && st.timeSelection->getLength() > 0.01s) sel = *st.timeSelection;
        auto c = new BounceExport::Dialog (session, stems, [this] (te::Track* t) { return timeline.state.isTrackSelected (t); }, anySel, sel);
        c->onDone = [stems] (const juce::String& summary, const juce::File& reveal)
        {
            Ui::message (juce::MessageBoxIconType::InfoIcon, stems ? "Export Stems" : "Bounce Mix", summary.substring (0, 1500));
            if (reveal.exists()) reveal.revealToUser();
        };
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (c);
        o.dialogTitle = stems ? "Export Stems" : "Bounce Mix";
        o.dialogBackgroundColour = Theme::panel();
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.componentToCentreAround = this;
        o.launchAsync();
    }

    void exportMidi()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export MIDI", session.sessionsDir, "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File() || session.edit == nullptr) return;
            juce::MidiFile mf;
            mf.setTicksPerQuarterNote (960);
            for (auto t : session.getTracks())
            {
                juce::MidiMessageSequence seq;
                bool any = false;
                for (auto c : t->getClips())
                    if (auto mc = dynamic_cast<te::MidiClip*> (c))
                    {
                        for (auto n : mc->getSequence().getNotes())
                        {
                            auto s = (mc->getStartBeat() + te::BeatDuration::fromBeats (n->getStartBeat().inBeats())).inBeats() * 960.0;
                            auto e = (mc->getStartBeat() + te::BeatDuration::fromBeats (n->getEndBeat().inBeats())).inBeats() * 960.0;
                            seq.addEvent (juce::MidiMessage::noteOn (1, n->getNoteNumber(), (juce::uint8) n->getVelocity()), s);
                            seq.addEvent (juce::MidiMessage::noteOff (1, n->getNoteNumber()), e);
                            any = true;
                        }
                    }
                if (any) { seq.updateMatchedPairs(); mf.addTrack (seq); }
            }
            juce::FileOutputStream out (f.withFileExtension (".mid"));
            if (out.openedOk()) { out.setPosition (0); out.truncate(); mf.writeTo (out); }
        });
    }

    //==========================================================================
    void clipMenu (int id)
    {
        auto& st = timeline.state;
        auto forAudio = [&] (std::function<void (te::AudioClipBase&)> fn) { for (auto& c : st.selectedClips) if (auto a = dynamic_cast<te::AudioClipBase*> (c.get())) fn (*a); };
        if (id == 400) for (auto& c : st.selectedClips) c->setMuted (! c->isMuted());
        else if (id == 401) for (auto& c : st.selectedClips) c->state.setProperty ("sqmLocked", ! (bool) c->state.getProperty ("sqmLocked", false), nullptr);
        else if (id == 402) { if (st.selectedClip) timeline.getCanvas().renameClip (st.selectedClip); }
        else if (id == 403) { if (st.selectedClip) timeline.getCanvas().spotDialog (st.selectedClip); }
        else if (id == 410) forAudio ([] (auto& a) { a.setGainDB (a.getGainDB() + 1); });
        else if (id == 411) forAudio ([] (auto& a) { a.setGainDB (a.getGainDB() - 1); });
        else if (id == 412) forAudio ([] (auto& a) { a.setGainDB (a.getGainDB() + 3); });
        else if (id == 413) forAudio ([] (auto& a) { a.setGainDB (a.getGainDB() - 3); });
        else if (id == 414) forAudio ([] (auto& a) { a.setGainDB (0); });
        else if (id == 415) forAudio ([this] (auto& a) { timeline.getCanvas().normaliseClipGain (a); });
        else if (id == 420) EditOps::fade (session, st);
        else if (id == 421) forAudio ([] (auto& a) { a.setFadeIn ({}); a.setFadeOut ({}); });
        else if (id == 422) forAudio ([] (auto& a) { a.setIsReversed (! a.getIsReversed()); });
        else if (id == 430) forAudio ([] (auto& a) { if (a.getTimeStretchMode() == te::TimeStretcher::disabled) a.setTimeStretchMode (Session::elasticToStretchMode (1)); a.performTempoDetect(); a.setAutoTempo (true); });
        else if (id == 431) forAudio ([] (auto& a) { if (! a.getAutoTempo() && a.getTimeStretchMode() == te::TimeStretcher::disabled) a.setTimeStretchMode (Session::elasticToStretchMode (1)); a.setAutoTempo (! a.getAutoTempo()); });
        else if (id == 432 || id == 433) forAudio ([id] (auto& a) { if (a.getTimeStretchMode() == te::TimeStretcher::disabled) a.setTimeStretchMode (Session::elasticToStretchMode (1)); a.setSpeedRatio (juce::jlimit (0.1, 10.0, a.getSpeedRatio() * (id == 432 ? 0.5 : 2.0))); });
        else if (id == 440) { if (st.timeSelection) for (auto& c : st.selectedClips) if (st.timeSelection->getEnd() > c->getPosition().getEnd()) { if (auto a = dynamic_cast<te::AudioClipBase*> (c.get())) { a->setLoopRange (a->getPosition().time.withStart (a->getPosition().getStart())); } c->setEnd (st.timeSelection->getEnd(), true); } }
        else if (id == 441) { for (auto& c : st.selectedClips) if (auto t = dynamic_cast<te::ClipTrack*> (c->getClipTrack())) { auto parent = c->state.getParent(); parent.moveChild (parent.indexOf (c->state), -1, &session.edit->getUndoManager()); } }
        else if (id == 449) for (auto& c : st.selectedClips) c->state.removeProperty (te::IDs::colour, nullptr);
        else if (id >= 450 && id < 450 + Theme::trackPalette().size()) for (auto& c : st.selectedClips) c->setColour (Theme::trackPalette()[id - 450]);
    }

    void eventMenu (int id)
    {
        auto& st = timeline.state;
        auto forMidi = [&] (std::function<void (te::MidiClip&)> fn) { for (auto& c : st.selectedClips) if (auto m = dynamic_cast<te::MidiClip*> (c.get())) fn (*m); };
        if (id == 500) forMidi ([this] (auto& m) { EditOps::quantizeMidi (m, 0.25, 1.0, &session.edit->getUndoManager()); });
        else if (id == 501)
        {
            auto aw = std::make_shared<juce::AlertWindow> ("Quantize", "Grid:", juce::MessageBoxIconType::NoIcon);
            aw->addComboBox ("g", { "1/4 note", "1/8 note", "1/16 note", "1/32 note", "1/8 triplet", "1/16 triplet" });
            aw->getComboBoxComponent ("g")->setSelectedItemIndex (2);
            aw->addButton ("Quantize", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, forMidi] (int r)
            {
                aw->setVisible (false);
                if (r != 1) return;
                const double grids[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
                auto g = grids[juce::jlimit (0, 5, aw->getComboBoxComponent ("g")->getSelectedItemIndex())];
                forMidi ([g, this] (auto& m) { EditOps::quantizeMidi (m, g, 1.0, &session.edit->getUndoManager()); });
                timeline.repaint();
            }));
        }
        else if (id >= 502 && id <= 505)
        {
            int semis = id == 502 ? 1 : id == 503 ? -1 : id == 504 ? 12 : -12;
            forMidi ([semis, this] (auto& m) { for (auto n : m.getSequence().getNotes()) n->setNoteNumber (juce::jlimit (0, 127, n->getNoteNumber() + semis), &session.edit->getUndoManager()); });
        }
        else if (id == 506 || id == 507) forMidi ([id, this] (auto& m) { for (auto n : m.getSequence().getNotes()) n->setVelocity (juce::jlimit (1, 127, n->getVelocity() + (id == 506 ? 10 : -10)), &session.edit->getUndoManager()); });
        else if (id == 508) forMidi ([this] (auto& m)
        {
            auto notes = m.getSequence().getNotes();
            std::sort (notes.begin(), notes.end(), [] (auto a, auto b) { return a->getStartBeat() < b->getStartBeat(); });
            for (int i = 0; i + 1 < notes.size(); ++i)
                if (notes[i + 1]->getStartBeat() > notes[i]->getStartBeat())
                    notes[i]->setStartAndLength (notes[i]->getStartBeat(), notes[i + 1]->getStartBeat() - notes[i]->getStartBeat(), &session.edit->getUndoManager());
        });
        else if (id == 509) forMidi ([this] (auto& m)
        {
            auto& seq = m.getSequence();
            auto notes = seq.getNotes();
            for (int i = notes.size(); --i > 0;)
                for (int j = i; --j >= 0;)
                    if (notes[i]->getNoteNumber() == notes[j]->getNoteNumber() && std::abs ((notes[i]->getStartBeat() - notes[j]->getStartBeat()).inBeats()) < 0.01)
                    { seq.removeNote (*notes[i], &session.edit->getUndoManager()); break; }
        });
        else if (id == 510 || id == 511)
        {
            auto aw = std::make_shared<juce::AlertWindow> (id == 510 ? "Tempo" : "Meter", id == 510 ? "BPM:" : "Time signature (e.g. 4/4, 3/4, 6/8):", juce::MessageBoxIconType::NoIcon);
            aw->addTextEditor ("v", id == 510 ? juce::String (session.edit->tempoSequence.getTempo (0)->getBpm(), 2) : juce::String (session.edit->tempoSequence.getTimeSig (0)->numerator.get()) + "/" + juce::String (session.edit->tempoSequence.getTimeSig (0)->denominator.get()));
            aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, id] (int r)
            {
                aw->setVisible (false);
                if (r != 1 || session.edit == nullptr) return;
                auto v = aw->getTextEditorContents ("v");
                if (id == 510) { auto bpm = v.getDoubleValue(); if (bpm >= 20 && bpm <= 400) session.edit->tempoSequence.getTempo (0)->setBpm (bpm); }
                else { auto p = juce::StringArray::fromTokens (v, "/", ""); if (p.size() == 2 && p[0].getIntValue() > 0 && p[1].getIntValue() > 0) { session.edit->tempoSequence.getTimeSig (0)->numerator = p[0].getIntValue(); session.edit->tempoSequence.getTimeSig (0)->denominator = p[1].getIntValue(); } }
            }));
        }
        else if (id == 512)
        {
            if (st.timeSelection && st.timeSelection->getLength() > 0.1s)
            {
                auto num = session.edit->tempoSequence.getTimeSig (0)->numerator.get();
                auto bpm = 60.0 * num / st.timeSelection->getLength().inSeconds();
                if (bpm >= 20 && bpm <= 400) session.edit->tempoSequence.getTempo (0)->setBpm (bpm);
            }
            else Ui::message (juce::MessageBoxIconType::InfoIcon, "Identify Beat", "Select exactly one bar of audio with the Selector first, then run Identify Beat.");
        }
        else if (id == 513) tapTempo();
        else if (id == 520) timeline.addMemoryLocationAtPlayhead();
        else if (id == 522) transportCtl.goToMarker (true);
        else if (id == 523) transportCtl.goToMarker (false);
    }

    void tapTempo()
    {
        struct Tapper : public juce::Component
        {
            std::function<void (double)> onBpm;
            std::vector<double> taps;
            void paint (juce::Graphics& g) override { g.fillAll (Theme::panel()); g.setColour (Theme::text()); g.setFont (Theme::font (16.0f, true)); g.drawFittedText ("Tap here (or press T) in time.\n" + (taps.size() > 1 ? juce::String (bpm(), 1) + " BPM" : juce::String ("...")), getLocalBounds(), juce::Justification::centred, 3); }
            double bpm() const { if (taps.size() < 2) return 0; return 60.0 / ((taps.back() - taps.front()) / (double) (taps.size() - 1)); }
            void tap() { auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0; if (! taps.empty() && now - taps.back() > 3.0) taps.clear(); taps.push_back (now); if (taps.size() > 8) taps.erase (taps.begin()); if (taps.size() > 1 && onBpm) onBpm (bpm()); repaint(); }
            void mouseDown (const juce::MouseEvent&) override { tap(); }
            bool keyPressed (const juce::KeyPress& k) override { if (k.getTextCharacter() == 't' || k.getTextCharacter() == 'T' || k == juce::KeyPress::spaceKey) { tap(); return true; } return false; }
        };
        auto t = std::make_unique<Tapper>();
        t->setSize (300, 160);
        t->setWantsKeyboardFocus (true);
        t->onBpm = [this] (double bpm) { if (session.edit && bpm >= 20 && bpm <= 400) session.edit->tempoSequence.getTempo (0)->setBpm (std::round (bpm * 10) / 10); };
        auto& box = juce::CallOutBox::launchAsynchronously (std::move (t), transport.getScreenBounds(), nullptr);
        box.grabKeyboardFocus();
    }

    void openMolt()
    {
        if (! Edition::allows (Edition::Feature::molt)) { showUnlock ("CORE"); return; }
        if (moltWindow == nullptr)
            moltWindow = std::make_unique<Molt::Window> (*session.edit, session.getEngine().getDeviceManager().deviceManager,
                                                         [this] { juce::Array<te::Clip::Ptr> a; for (auto& c : timeline.state.selectedClips) a.add (c); return a; });
        moltWindow->setVisible (true);
        moltWindow->toFront (true);
    }

    void audioSuiteMenu (int id)
    {
        auto& c = timeline.getCanvas();
        if (id == 605) c.applyAudioSuite (55);
        else c.applyAudioSuite (50 + (id - 600));
    }

    void separateAtTransients()
    {
        auto n = timeline.getCanvas().separateAtTransients (0.5f);
        session.log.add ("You", "Separated at " + juce::String (n) + " transient(s)");
    }

    void showShortcuts()
    {
        juce::String s;
        s << "TRANSPORT\n Space play/stop   Cmd+Space / F12 record   Return RTZ   Shift+Space play from start   L loop   C click   Cmd+Shift+L loop playback\n Cmd+K pre-roll   Cmd+Shift+K post-roll   Page Up/Down previous/next marker   Q / Shift+Return new memory location\n Numeric keypad: 0 play  1/2 rewind/forward  3 record  4 loop  5 loop record  6 QuickPunch  7 click  8 count-off  9 MIDI merge\n\n"
          << "EDIT MODES & TOOLS\n F1 Shuffle  F2 Slip  F3 Spot  F4 Grid   |   F5 Zoomer  F6 Trim  F7 Selector  F8 Grabber  F9 Scrubber  F10 Pencil   (F6/F7/F8 again = Smart Tool)\n Hold Cmd while dragging to suspend the grid.\n\n"
          << "EDITING\n Cmd+E separate   Cmd+D duplicate   Opt+R repeat   Cmd+F fades   Cmd+U strip silence   Cmd+H heal   Cmd+T trim to selection   A / S trim start/end to playhead\n Opt+Shift+3 consolidate   Cmd+M mute clip   Cmd+X/C/V cut/copy/paste   Delete clear   Shift+Delete delete & close gap   Cmd+A select all   Esc deselect\n + / - nudge   , / . nudge by grid   Tab / Opt+Tab next/previous transient or clip edge   Shift+Tab extend selection\n\n"
          << "ZOOM & VIEW\n Cmd+] / Cmd+[ zoom   T / R zoom   Opt+F fit selection   Cmd+Opt+F fit session   Cmd+Opt+Up/Down track heights   Cmd+= Edit/Mix\n\n"
          << "TRACKS\n Cmd+Shift+N new track   Cmd+Shift+Delete delete selected   Shift+S/M/R solo/mute/arm selected   Up/Down (P / ;) select track   Shift+Up/Down extend\n Cmd+G groups   Cmd+Shift+K colour palette\n\n"
          << "WINDOWS\n Cmd+1 Transport   Cmd+2 Session Setup   Cmd+3 Big Counter   Cmd+5 Memory Locations   Cmd+, Preferences   Cmd+W close floating windows\n\n"
          << "FILE\n Cmd+N new   Cmd+O open   Cmd+S save   Cmd+Shift+S save as   Cmd+Shift+I import   Cmd+Opt+B bounce   Cmd+Z / Cmd+Shift+Z undo / redo";
        auto te2 = new juce::TextEditor();
        te2->setMultiLine (true); te2->setReadOnly (true); te2->setText (s);
        te2->setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
        te2->setColour (juce::TextEditor::backgroundColourId, Theme::bg()); te2->setColour (juce::TextEditor::textColourId, Theme::text());
        te2->setSize (900, 520);
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Keyboard Shortcuts"; o.dialogBackgroundColour = Theme::panel(); o.content.setOwned (te2); o.useNativeTitleBar = true; o.resizable = true;
        o.launchAsync();
    }

    void showTour()
    {
        const char* tips[] = {
            "1/5  THE EDIT WINDOW\n\nThe toolbar has the four Edit Modes (Shuffle, Slip, Spot, Grid) and seven tools, exactly like Pro Tools. The Smart Tool is on by default: top half of a clip selects, bottom half grabs, edges trim, corners fade.",
            "2/5  SELECTIONS & TRANSPORT\n\nDrag with the Selector to make an Edit selection across tracks - Play starts there, Loop plays it, and Delete, Cmd+E, Cmd+D act on it. Pre/post-roll, count-off, Loop Record and QuickPunch live on the MODE button and in Window > Transport.",
            "3/5  TRACKS\n\nRight-click a track name for I/O, inserts, colour, groups, folders, VCAs, Elastic Audio, playlists and track presets. Each header has an automation mode button (off / read / touch / latch / write).",
            "4/5  CLAUDE\n\nOpen the CLAUDE tab and press Connect. Copy the link into claude.ai > Settings > Connectors > Add custom connector. Claude can then build beats, mix, insert your plugins and edit for you (say \"use Squelchmoth\").",
            "5/5  MAKE IT YOURS\n\nSetup > Preferences > Display changes the Edit and Mix window backgrounds, every accent colour and the track palette. Window > Color Palette recolours selected tracks and clips. Help > Keyboard Shortcuts lists everything." };
        for (int i = 0; i < 5; ++i)
            juce::Timer::callAfterDelay (i * 120, [t = juce::String (tips[i])] { Ui::message (juce::MessageBoxIconType::InfoIcon, "Squelchmoth tour", t); });
    }
};
