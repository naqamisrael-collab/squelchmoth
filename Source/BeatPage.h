#pragma once
// Squelchmoth 1.8 - the Beat page (EDIT · MIX · BEAT · CLAUDE): the FL workspace in one screen.
//
//   left    : the Browser - instruments, kits, presets, samples (your folders) - double-click or drag onto the rack
//   centre  : the Playlist - pattern clips over bars, Pattern / Song mode
//   right   : the selected channel's instrument panel (Squelchmoth instruments embed; VST/AU open in their window)
//   bottom  : the Beat Mixer strip
//   window  : the Channel Rack (FL layout) opens automatically as its own floating window the moment the Beat page is shown
//             (Cmd+6 brings it back; it closes again when you leave the page unless you opened it yourself)
// Everything here is the same session data as the Edit and Mix pages: a channel is a track, a pattern clip is a clip,
// the mixer is the mixer. Panels resize with the dividers and can be hidden (View > Beat Page).

#include "ChannelRack.h"
#include "MixerView.h"
#include "PluginSkins.h"

//==============================================================================
/** Browser items carry "sqmBrowser|<kind>|<what>|<extra>" as the drag description. */
class BeatBrowser : public juce::Component,
                    private juce::Timer
{
public:
    struct Item : public juce::TreeViewItem
    {
        Item (BeatBrowser& o, juce::String k, juce::String n, juce::String w = {}, juce::String x = {}, bool folder = false) : owner (o), kind (k), name (n), what (w), extra (x), isFolder (folder) {}
        BeatBrowser& owner; juce::String kind, name, what, extra; bool isFolder; bool populated = false;
        bool mightContainSubItems() override   { return isFolder; }
        void itemOpennessChanged (bool open) override   { if (open && ! populated) { populated = true; owner.populate (*this); } }
        void paintItem (juce::Graphics& g, int w, int h) override
        {
            if (isSelected()) { g.setColour (Theme::violet().withAlpha (0.35f)); g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 3.0f); }
            g.setColour (isFolder ? Theme::dim() : Theme::text()); g.setFont (Theme::font (11.0f, isFolder));
            juce::Colour dot = kind == "instrument" ? Theme::violet() : kind == "kit" || kind == "sound" ? Theme::amber() : kind == "preset" ? Theme::mint() : kind == "sample" ? Theme::acid() : Theme::dim();
            if (! isFolder) { g.setColour (dot); g.fillEllipse (4.0f, h * 0.5f - 3.0f, 6.0f, 6.0f); g.setColour (Theme::text()); }
            g.drawFittedText (name, isFolder ? 2 : 14, 0, w - 16, h, juce::Justification::centredLeft, 1);
        }
        juce::var getDragSourceDescription() override   { return isFolder ? juce::var() : juce::var ("sqmBrowser|" + kind + "|" + what + "|" + extra); }
        void itemDoubleClicked (const juce::MouseEvent&) override   { if (! isFolder && owner.onActivate) owner.onActivate (kind, what, extra); }
        void itemClicked (const juce::MouseEvent& e) override       { if (! isFolder && e.mods.isAltDown() && owner.onPreview) owner.onPreview (kind, what, extra); }
        int getItemHeight() const override   { return 20; }
    };

    BeatBrowser (Session& s) : session (s)
    {
        search.setTextToShowWhenEmpty ("Search instruments, kits, presets, samples...", Theme::dim());
        search.onTextChange = [this] { rebuild(); };
        addAndMakeVisible (search);
        tree.setRootItemVisible (false); tree.setDefaultOpenness (false); tree.setIndentSize (14);
        tree.setColour (juce::TreeView::backgroundColourId, Theme::panel());
        addAndMakeVisible (tree);
        addFolder.setButtonText ("+ FOLDER"); addFolder.setTooltip ("Add a folder of samples / loops to the browser");
        addFolder.onClick = [this]
        {
            auto fc = std::make_shared<juce::FileChooser> ("Add a sample folder", juce::File::getSpecialLocation (juce::File::userMusicDirectory));
            fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, fc] (const juce::FileChooser& c)
            {
                if (c.getResult().isDirectory()) { auto f = Prefs::getString ("beat_browser_folders"); auto list = juce::StringArray::fromTokens (f, "|", ""); list.addIfNotAlreadyThere (c.getResult().getFullPathName()); Prefs::set ("beat_browser_folders", list.joinIntoString ("|")); rebuild(); }
            });
        };
        addAndMakeVisible (addFolder);
        rebuild();
        startTimer (1500);
    }
    ~BeatBrowser() override   { tree.setRootItem (nullptr); }

    std::function<void (const juce::String& kind, const juce::String& what, const juce::String& extra)> onActivate, onPreview;

    void rebuild()
    {
        tree.setRootItem (nullptr);
        root = std::make_unique<Item> (*this, "root", "Browser", "", "", true);
        root->populated = true;
        const auto q = search.getText().trim().toLowerCase();
        auto add = [&] (juce::String k, juce::String n, juce::String w, juce::String x, bool folder) { auto* it = new Item (*this, k, n, w, x, folder); root->addSubItem (it); return it; };
        // instruments
        auto inst = add ("folder", "INSTRUMENTS", "", "", true); inst->populated = true;
        for (auto& e : Signature::entries()) if (juce::String (e.category) == "Instruments" && juce::String (e.type) != "sampler")
            if (q.isEmpty() || juce::String (e.name).toLowerCase().contains (q) || juce::String (e.tagline).toLowerCase().contains (q)) inst->addSubItem (new Item (*this, "instrument", e.name, e.name, e.tagline));
        juce::StringArray tp; for (auto& d : session.engine.getPluginManager().knownPluginList.getTypes()) if (d.isInstrument) tp.add (d.name); tp.sort (true);
        if (! tp.isEmpty()) { auto third = add ("folder", "YOUR VST3 / AU INSTRUMENTS", "", "", true); third->populated = true; for (auto& n : tp) if (q.isEmpty() || n.toLowerCase().contains (q)) third->addSubItem (new Item (*this, "instrument", n, n)); }
        // kits
        auto kits = add ("folder", "KITS", "", "", true); kits->populated = true;
        for (int k = 0; k < DrumKit::numKits; ++k)
            if (q.isEmpty() || juce::String (DrumKit::kits[k].name).toLowerCase().contains (q) || juce::String (DrumKit::kits[k].description).toLowerCase().contains (q))
            {
                auto* kit = new Item (*this, "kit", DrumKit::kits[k].name, juce::String (k), DrumKit::kits[k].description, true);
                kits->addSubItem (kit);
            }
        // presets by instrument
        auto pres = add ("folder", "PRESETS", "", "", true); pres->populated = true;
        for (auto& e : Signature::entries()) if (juce::String (e.category) == "Instruments" && ! e.presets.empty())
        {
            auto* pi = new Item (*this, "folder", e.name, e.name, "", true); pi->populated = true; int shown = 0;
            juce::StringArray cats;
            for (auto& p : e.presets) { auto c = juce::String (p.name).contains (" | ") ? juce::String (p.name).upToFirstOccurrenceOf (" | ", false, false) : juce::String (p.advanced ? "Advanced" : "Basic"); if (! cats.contains (c)) cats.add (c); }
            for (auto& c : cats)
            {
                auto* ci = new Item (*this, "folder", c, "", "", true); ci->populated = true; int inCat = 0;
                for (auto& p : e.presets)
                {
                    auto full = juce::String (p.name); auto cat = full.contains (" | ") ? full.upToFirstOccurrenceOf (" | ", false, false) : juce::String (p.advanced ? "Advanced" : "Basic");
                    auto nm = full.contains (" | ") ? full.fromFirstOccurrenceOf (" | ", false, false) : full;
                    if (cat != c) continue;
                    if (q.isNotEmpty() && ! nm.toLowerCase().contains (q) && ! c.toLowerCase().contains (q) && ! juce::String (e.name).toLowerCase().contains (q)) continue;
                    ci->addSubItem (new Item (*this, "preset", nm, e.name, full)); ++inCat; ++shown;
                }
                if (inCat > 0) pi->addSubItem (ci); else delete ci;
            }
            if (shown > 0) pres->addSubItem (pi); else delete pi;
        }
        // samples: kit folders + user folders
        auto smp = add ("folder", "SAMPLES", "", "", true); smp->populated = true;
        auto kitsDir = session.sessionsDir.getChildFile ("Kits");
        if (kitsDir.isDirectory()) smp->addSubItem (new Item (*this, "dir", "Squelchmoth Kits", kitsDir.getFullPathName(), "", true));
        for (auto& f : juce::StringArray::fromTokens (Prefs::getString ("beat_browser_folders"), "|", "")) if (juce::File (f).isDirectory()) smp->addSubItem (new Item (*this, "dir", juce::File (f).getFileName(), f, "", true));
        tree.setRootItem (root.get());
        for (int i = 0; i < root->getNumSubItems(); ++i) root->getSubItem (i)->setOpen (true);
        if (q.isNotEmpty()) tree.setDefaultOpenness (true);
    }

    void populate (Item& it)
    {
        if (it.kind == "kit")
        {
            const int k = it.what.getIntValue();
            it.addSubItem (new Item (*this, "kit", "Add 8-channel starter", it.what, "8"));
            it.addSubItem (new Item (*this, "kit", "Add all 12 sounds", it.what, "12"));
            auto files = DrumKit::ensureKit (session, k);
            for (int i = 0; i < DrumKit::numSounds; ++i) it.addSubItem (new Item (*this, "sound", DrumKit::sounds[i].name, files[i].getFullPathName(), juce::String (DrumKit::sounds[i].note)));
        }
        else if (it.kind == "dir")
        {
            juce::File dir (it.what);
            juce::Array<juce::File> dirs, files;
            dir.findChildFiles (dirs, juce::File::findDirectories, false); dir.findChildFiles (files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            dirs.sort(); files.sort();
            for (auto& d : dirs) it.addSubItem (new Item (*this, "dir", d.getFileName(), d.getFullPathName(), "", true));
            for (auto& f : files) it.addSubItem (new Item (*this, "sample", f.getFileNameWithoutExtension(), f.getFullPathName()));
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        auto top = r.removeFromTop (26);
        addFolder.setBounds (top.removeFromRight (74).reduced (2, 1));
        search.setBounds (top.reduced (0, 1));
        r.removeFromTop (4);
        tree.setBounds (r);
    }
    void paint (juce::Graphics& g) override   { g.fillAll (Theme::panel()); }
    void timerCallback() override   { if (! tree.isMouseOverOrDragging (true) && Signature::entries().size() != lastCount) { lastCount = (int) Signature::entries().size(); } }

private:
    Session& session;
    juce::TextEditor search;
    juce::TreeView tree;
    juce::TextButton addFolder;
    std::unique_ptr<Item> root;
    int lastCount = 0;
};

//==============================================================================
/** The mini playlist: bars across, one lane per pattern colour; click to place the current pattern, right-click a clip for options. */
class MiniPlaylist : public juce::Component,
                     private juce::Timer
{
public:
    MiniPlaylist (Session& s) : session (s) { startTimerHz (15); }
    te::Edit& edit()   { return *session.edit; }
    std::function<void()> onChanged;

    struct Block { te::Clip* clip; juce::String id; double startBar, endBar; int lane; juce::Colour colour; juce::String name; bool unique; };

    std::vector<Block> blocks()
    {
        std::vector<Block> out;
        std::map<juce::String, int> lanes;
        auto& ed = edit();
        for (int i = 0; i < Rack::numPatterns (ed); ++i) if (auto sc = Rack::pattern (ed, i)) lanes[Rack::patternId (*sc)] = i;
        std::set<std::pair<juce::String, juce::int64>> seen;
        for (auto t : Rack::channels (ed))
            for (auto c : t->getClips())
            {
                const auto id = Rack::clipPatternId (*c); if (id.isEmpty()) continue;
                const auto startBar = session.timeToBar (c->getPosition().getStart());
                auto key = std::make_pair (id, (juce::int64) std::llround (startBar * 1000.0));
                if (seen.count (key)) continue;
                seen.insert (key);
                const int lane = lanes.count (id) ? lanes[id] : 0;
                out.push_back ({ c, id, startBar, session.timeToBar (c->getPosition().getEnd()), lane, c->getColour(), c->getName(), Rack::isUnique (*c) });
            }
        return out;
    }

    int numLanes()   { return juce::jmax (4, Rack::numPatterns (edit())); }
    double barsVisible() const   { return 32.0; }
    float xForBar (double bar) const   { return (float) leftW + (float) ((bar - 1.0) / barsVisible()) * (float) (getWidth() - leftW); }
    double barForX (float x) const     { return 1.0 + (x - leftW) / (float) (getWidth() - leftW) * barsVisible(); }
    int laneH()   { return juce::jlimit (22, 44, (getHeight() - headerH) / numLanes()); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        auto& ed = edit();
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f, true));
        g.setColour (Theme::panel().darker (0.2f)); g.fillRect (0, 0, getWidth(), headerH);
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f, true));
        g.drawText ("PLAYLIST", 8, 0, leftW - 12, headerH, juce::Justification::centredLeft);
        // bar grid
        for (int b = 1; b <= (int) barsVisible(); ++b)
        {
            const float x = xForBar (b);
            g.setColour (Theme::line().withAlpha ((b - 1) % 4 == 0 ? 0.8f : 0.3f)); g.drawVerticalLine ((int) x, (float) headerH, (float) getHeight());
            if ((b - 1) % 4 == 0) { g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f)); g.drawText (juce::String (b), (int) x + 3, 2, 30, headerH - 2, juce::Justification::centredLeft); }
        }
        const int lh = laneH();
        for (int l = 0; l < numLanes(); ++l)
        {
            auto lane = juce::Rectangle<int> (0, headerH + l * lh, getWidth(), lh);
            g.setColour (l % 2 ? Theme::panel2().withAlpha (0.25f) : juce::Colours::transparentBlack); g.fillRect (lane.withLeft (leftW));
            if (l < Rack::numPatterns (ed))
            {
                g.setColour (Rack::patternColour (ed, l).withAlpha (l == Rack::currentPattern (ed) ? 0.9f : 0.5f)); g.fillRoundedRectangle (lane.removeFromLeft (leftW - 4).reduced (4, 2).toFloat(), 3.0f);
                g.setColour (juce::Colours::black); g.setFont (Theme::font (9.5f, l == Rack::currentPattern (ed)));
                g.drawFittedText (Rack::patternName (ed, l), juce::Rectangle<int> (8, headerH + l * lh, leftW - 14, lh), juce::Justification::centredLeft, 1);
            }
        }
        for (auto& b : blocks())
        {
            auto r = juce::Rectangle<float> (xForBar (b.startBar), (float) (headerH + b.lane * lh) + 2.0f, juce::jmax (4.0f, xForBar (b.endBar) - xForBar (b.startBar) - 1.0f), (float) lh - 4.0f);
            g.setColour (b.colour.withAlpha (b.unique ? 0.55f : 0.9f)); g.fillRoundedRectangle (r, 3.0f);
            if (b.unique) { g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawRoundedRectangle (r, 3.0f, 1.0f); }
            g.setColour (juce::Colours::black); g.setFont (Theme::font (9.0f, true)); g.drawFittedText (b.name + (b.unique ? " *" : ""), r.toNearestInt().reduced (4, 0), juce::Justification::centredLeft, 1);
        }
        // playhead
        const auto bar = session.timeToBar (ed.getTransport().getPosition());
        g.setColour (Theme::acid()); g.drawVerticalLine ((int) xForBar (bar), (float) headerH, (float) getHeight());
        // loop range
        if (ed.getTransport().looping)
        {
            auto lr = ed.getTransport().getLoopRange();
            g.setColour (Theme::violet().withAlpha (0.25f)); g.fillRect (xForBar (session.timeToBar (lr.getStart())), (float) headerH, xForBar (session.timeToBar (lr.getEnd())) - xForBar (session.timeToBar (lr.getStart())), 3.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y < headerH) { if (e.x > leftW) { edit().getTransport().setPosition (session.barToTime (std::floor (barForX ((float) e.x)))); } return; }
        const int lane = juce::jlimit (0, numLanes() - 1, (e.y - headerH) / laneH());
        if (e.x < leftW) { if (lane < Rack::numPatterns (edit())) { Rack::selectPattern (edit(), lane); if (onChanged) onChanged(); repaint(); } return; }
        for (auto& b : blocks())
            if (b.lane == lane && e.x >= xForBar (b.startBar) && e.x < xForBar (b.endBar))
            {
                if (e.mods.isPopupMenu()) { showClipMenu (b); return; }
                if (e.mods.isAltDown()) { for (auto c : Rack::linkedGroup (*b.clip)) c->removeFromParent(); if (onChanged) onChanged(); repaint(); return; }
                Rack::selectPattern (edit(), b.lane); edit().getTransport().setPosition (session.barToTime (b.startBar)); if (onChanged) onChanged(); repaint(); return;
            }
        if (e.mods.isPopupMenu() || lane >= Rack::numPatterns (edit())) return;
        dragLane = lane; dragStartBar = std::floor (barForX ((float) e.x)); dragEndBar = dragStartBar;
        Rack::selectPattern (edit(), lane);
        Rack::placePattern (session, lane, dragStartBar, 1);
        placedBars = 1;
        if (onChanged) onChanged(); repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragLane < 0) return;
        const double lenBars = Rack::patternLengthBeats (edit(), dragLane) / 4.0;
        const int want = juce::jmax (1, (int) std::floor ((barForX ((float) e.x) - dragStartBar) / lenBars) + 1);
        if (want != placedBars)
        {
            // remove what we placed and place again with the new repeat count
            for (auto t : Rack::channels (edit())) for (auto c : t->getClips()) if (Rack::clipPatternId (*c) == Rack::patternId (*Rack::pattern (edit(), dragLane)) && session.timeToBar (c->getPosition().getStart()) >= dragStartBar - 0.001 && session.timeToBar (c->getPosition().getStart()) < dragStartBar + lenBars * placedBars - 0.001) c->removeFromParent();
            Rack::placePattern (session, dragLane, dragStartBar, want);
            placedBars = want; repaint();
        }
    }
    void mouseUp (const juce::MouseEvent&) override   { dragLane = -1; if (onChanged) onChanged(); }

    void showClipMenu (Block b)
    {
        juce::PopupMenu m;
        m.addItem (1, b.unique ? "Already unique (edits do not follow the pattern)" : "Make unique", ! b.unique);
        m.addItem (2, "Delete these pattern clips"); m.addItem (3, "Repeat once more after");
        m.addItem (4, "Open this pattern in the rack");
        te::Clip::Ptr keep (b.clip);
        m.showMenuAsync (juce::PopupMenu::Options(), [this, keep, b] (int r)
        {
            if (keep == nullptr || r <= 0) return;
            if (r == 1) for (auto c : Rack::linkedGroup (*keep)) Rack::makeUnique (*c);
            else if (r == 2) for (auto c : Rack::linkedGroup (*keep)) c->removeFromParent();
            else if (r == 3) Rack::placePattern (session, b.lane, b.endBar, 1);
            else if (r == 4) Rack::selectPattern (edit(), b.lane);
            if (onChanged) onChanged(); repaint();
        });
    }
    void timerCallback() override   { if (edit().getTransport().isPlaying() || juce::Time::getMillisecondCounter() - lastRepaint > 600) { lastRepaint = juce::Time::getMillisecondCounter(); repaint(); } }

private:
    Session& session;
    static constexpr int leftW = 130, headerH = 22;
    int dragLane = -1, placedBars = 0; double dragStartBar = 1, dragEndBar = 1;
    juce::uint32 lastRepaint = 0;
};

//==============================================================================
/** Divider the user drags to resize two panels. */
class SplitDivider : public juce::Component
{
public:
    SplitDivider (bool vertical) : vert (vertical) { setMouseCursor (vert ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor); }
    std::function<void (int delta)> onDrag;
    void paint (juce::Graphics& g) override { g.fillAll (Theme::bg()); g.setColour (Theme::line()); auto r = getLocalBounds().toFloat(); if (vert) g.fillRect (r.withSizeKeepingCentre (1.0f, r.getHeight())); else g.fillRect (r.withSizeKeepingCentre (r.getWidth(), 1.0f)); }
    void mouseDown (const juce::MouseEvent&) override { last = 0; }
    void mouseDrag (const juce::MouseEvent& e) override { const int d = vert ? e.getDistanceFromDragStartX() : e.getDistanceFromDragStartY(); if (onDrag) onDrag (d - last); last = d; }
private:
    bool vert; int last = 0;
};

//==============================================================================
class BeatPage : public juce::Component,
                 public juce::DragAndDropContainer,
                 public juce::DragAndDropTarget,
                 public juce::FileDragAndDropTarget,
                 private juce::Timer
{
public:
    BeatPage (Session& s) : session (s), browser (s), playlist (s), mixer (s, nullptr), divL (true), divR (true), divB (false)
    {
        addAndMakeVisible (browser); addAndMakeVisible (playlist); addAndMakeVisible (divL); addAndMakeVisible (divR); addAndMakeVisible (divB);
        addChildComponent (mixer);
        mixer.trackFilter = [] (te::Track& t) { return Rack::isRackTrack (t); };
        addAndMakeVisible (sidePanel);
        sidePanel.onOpenWindow = [this] { if (auto t = selectedChannel()) if (auto p = Rack::instrumentOf (*t)) p->showWindowExplicitly(); };
        sidePanel.onOpenSettings = [this] { if (auto t = selectedChannel()) if (auto rc = Rack::rackChannelPlugin (*t, true)) rc->showWindowExplicitly(); };
        browserW = Prefs::getInt ("beat_browserW", 240); sideW = Prefs::getInt ("beat_sideW", 420); bottomH = Prefs::getInt ("beat_bottomH", 150);
        showMixer = Prefs::getBool ("beat_showMixer", false); showBrowser = Prefs::getBool ("beat_showBrowser", true); showSide = Prefs::getBool ("beat_showSide", false); showPlaylist = true;
        divL.onDrag = [this] (int d) { browserW = juce::jlimit (140, 600, browserW + d); Prefs::set ("beat_browserW", browserW); resized(); };
        divR.onDrag = [this] (int d) { sideW = juce::jlimit (200, 900, sideW - d); Prefs::set ("beat_sideW", sideW); resized(); };
        divB.onDrag = [this] (int d) { bottomH = juce::jlimit (80, 500, bottomH - d); Prefs::set ("beat_bottomH", bottomH); resized(); };
        browser.onActivate = [this] (const juce::String& kind, const juce::String& what, const juce::String& extra) { activate (kind, what, extra); };
        browser.onPreview = [this] (const juce::String& kind, const juce::String& what, const juce::String&) { if (kind == "sample" || kind == "sound") session.previewFile (juce::File (what)); };
        playlist.onChanged = [this] { if (rack) rack->refresh(); };
        startTimerHz (4);
    }

    std::function<void (te::AudioTrack&)> onSelectTrack;
    /** The Channel Rack lives in its own floating window (opened with the page); the page talks to it through this. */
    void setRack (ChannelRackComponent* r)
    {
        rack = r;
        if (rack) rack->onSelectTrack = [this] (te::AudioTrack& t) { if (onSelectTrack) onSelectTrack (t); sidePanel.show (Rack::instrumentOf (t), &t); };
    }
    ChannelRackComponent* getRack()   { return rack; }
    te::AudioTrack* selectedChannel() { return rack ? rack->selected() : nullptr; }
    MiniPlaylist& getPlaylist()       { return playlist; }

    void setPanel (const juce::String& which, bool on)
    {
        if (which == "browser") { showBrowser = on; Prefs::set ("beat_showBrowser", on); }
        else if (which == "side") { showSide = on; Prefs::set ("beat_showSide", on); }
        else if (which == "playlist") { showPlaylist = true; }
        else if (which == "mixer") { showMixer = on; Prefs::set ("beat_showMixer", on); if (on) mixer.refreshStrips(); }
        resized();
    }
    bool panelShown (const juce::String& which) const   { return which == "browser" ? showBrowser : which == "side" ? showSide : which == "playlist" ? showPlaylist : which == "mixer" ? showMixer : false; }

    void activate (const juce::String& kind, const juce::String& what, const juce::String& extra)
    {
        te::AudioTrack* made = nullptr;
        if (kind == "instrument") made = Rack::addInstrumentChannel (session, what);
        else if (kind == "kit") { auto chans = Rack::addKitChannels (session, what.getIntValue(), extra == "12" ? -1 : 8); if (! chans.isEmpty()) made = chans.getFirst(); }
        else if (kind == "sound") made = Rack::addSampleChannel (session, juce::File (what), {}, extra.getIntValue() > 0 ? extra.getIntValue() : 60);
        else if (kind == "sample") made = Rack::addSampleChannel (session, juce::File (what));
        else if (kind == "preset")
        {
            // a preset on the selected channel if it holds that instrument, else a new channel
            auto t = selectedChannel();
            te::Plugin* p = t ? Rack::instrumentOf (*t) : nullptr;
            if (p == nullptr || Signature::displayName (*p) != what) { made = Rack::addInstrumentChannel (session, what); p = made ? Rack::instrumentOf (*made) : nullptr; }
            if (p != nullptr) if (auto se = Signature::findByName (what)) for (auto& pr : se->presets) if (juce::String (pr.name) == extra) { Signature::applyPreset (*p, pr); if (t && ! made) t->setName (extra.fromFirstOccurrenceOf (" | ", false, false)); }
        }
        if (made != nullptr && kind == "preset") made->setName (extra.fromFirstOccurrenceOf (" | ", false, false));
        if (rack) rack->refresh();
        if (made != nullptr) { if (rack) rack->selectRow (Rack::indexOfChannel (*session.edit, made)); sidePanel.show (Rack::instrumentOf (*made), made); }
    }

    // drag from the browser onto the rack
    bool isInterestedInDragSource (const SourceDetails& d) override   { return d.description.toString().startsWith ("sqmBrowser|"); }
    void itemDropped (const SourceDetails& d) override
    {
        auto parts = juce::StringArray::fromTokens (d.description.toString(), "|", ""); if (parts.size() < 3) return;
        activate (parts[1], parts[2], parts.size() > 3 ? parts[3] : juce::String());
    }
    bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true; return false; }
    void filesDropped (const juce::StringArray& files, int, int) override   { for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) Rack::addSampleChannel (session, juce::File (f)); if (rack) rack->refresh(); }

    void paint (juce::Graphics& g) override   { g.fillAll (Theme::bg()); }
    void resized() override
    {
        auto r = getLocalBounds();
        if (showMixer) { mixer.setBounds (r.removeFromBottom (bottomH + 110)); divB.setBounds (r.removeFromBottom (5)); mixer.setVisible (true); }
        else { mixer.setVisible (false); divB.setBounds ({}); }
        if (showBrowser) { browser.setBounds (r.removeFromLeft (browserW)); divL.setBounds (r.removeFromLeft (5)); browser.setVisible (true); } else { browser.setVisible (false); divL.setBounds ({}); }
        if (showSide) { sidePanel.setBounds (r.removeFromRight (sideW)); divR.setBounds (r.removeFromRight (5)); sidePanel.setVisible (true); } else { sidePanel.setVisible (false); divR.setBounds ({}); }
        playlist.setBounds (r);
    }

private:
    //==========================================================================
    /** Right-hand panel: the selected channel's instrument, embedded. */
    struct SidePanel : public juce::Component
    {
        SidePanel()
        {
            openBtn.setButtonText ("OPEN WINDOW"); settingsBtn.setButtonText ("CHANNEL SETTINGS");
            openBtn.onClick = [this] { if (onOpenWindow) onOpenWindow(); }; settingsBtn.onClick = [this] { if (onOpenSettings) onOpenSettings(); };
            addAndMakeVisible (openBtn); addAndMakeVisible (settingsBtn);
        }
        std::function<void()> onOpenWindow, onOpenSettings;
        te::Plugin::Ptr current; te::Track::Ptr track;
        std::unique_ptr<juce::Component> editor;
        juce::TextButton openBtn, settingsBtn;
        void show (te::Plugin* p, te::AudioTrack* t)
        {
            track = t;
            if (p == current.get() && editor != nullptr) return;
            editor.reset(); current = p;
            if (p != nullptr && dynamic_cast<te::ExternalPlugin*> (p) == nullptr && Signature::findForPlugin (*p) != nullptr)
            {
                editor = std::make_unique<Skins::SkinEditor> (*p);
                addAndMakeVisible (*editor);
            }
            resized(); repaint();
        }
        void resized() override
        {
            auto r = getLocalBounds();
            auto top = r.removeFromTop (28).reduced (4, 2);
            settingsBtn.setBounds (top.removeFromRight (150)); top.removeFromRight (4); openBtn.setBounds (top.removeFromRight (110));
            if (editor)
            {
                // scale the faceplate to fit the panel width (they are designed wide)
                const float scale = juce::jmin (1.0f, (float) r.getWidth() / (float) juce::jmax (1, editor->getWidth()), (float) r.getHeight() / (float) juce::jmax (1, editor->getHeight()));
                editor->setTransform (juce::AffineTransform::scale (scale));
                editor->setTopLeftPosition (r.getX(), r.getY());
            }
        }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel());
            g.setColour (Theme::dim()); g.setFont (Theme::font (9.5f, true));
            g.drawText (track != nullptr ? track->getName().toUpperCase() + (current ? "  -  " + Signature::displayName (*current) : juce::String ("  -  no instrument")) : juce::String ("CHANNEL"), getLocalBounds().removeFromTop (28).reduced (8, 0), juce::Justification::centredLeft);
            if (editor == nullptr)
            {
                g.setColour (Theme::dim()); g.setFont (Theme::font (12.0f));
                g.drawFittedText (current != nullptr ? "This instrument opens in its own window (VST3 / AU).\nClick OPEN WINDOW." : "Select a channel in the rack to see its instrument here.", getLocalBounds().reduced (20), juce::Justification::centred, 3);
            }
        }
    } sidePanel;

    void timerCallback() override
    {
        // follow the rack selection; rebuild the beat mixer when the rack changes shape
        if (auto t = selectedChannel()) { if (sidePanel.track.get() != t || sidePanel.current.get() != Rack::instrumentOf (*t)) sidePanel.show (Rack::instrumentOf (*t), t); }
        const int n = Rack::channels (*session.edit).size();
        if (showMixer && n != lastRackCount) { lastRackCount = n; mixer.refreshStrips(); }
    }

    Session& session;
    BeatBrowser browser;
    ChannelRackComponent* rack = nullptr;
    MiniPlaylist playlist;
    MixerView mixer;
    SplitDivider divL, divR, divB;
    int browserW = 240, sideW = 420, bottomH = 150, lastRackCount = -1;
    bool showMixer = false, showBrowser = true, showSide = true, showPlaylist = true;
};
