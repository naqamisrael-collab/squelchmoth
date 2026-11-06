#pragma once
// Squelchmoth - Track > New... (Cmd+Shift+N): the Pro Tools New Tracks dialog.
//
// Each row reads like Pro Tools:  Create [n] new [Mono / Stereo] [type] in [Samples / Ticks] named [name]
//   types:  Audio Track, Aux Input, Master Fader, VCA Master, MIDI Track, Instrument Track, Basic Folder,
//           Routing Folder, and every saved Track Preset.
//   rows:   + adds a row below, - removes it, drag the handle on the left to re-order.
//   keys:   Cmd+Up/Down track type, Cmd+Left/Right format, Cmd+Opt+Up/Down timebase,
//           Cmd+N or Cmd+Shift+Up add a row, Cmd+Shift+Down remove the last row, Return create, Esc cancel.
// The dialog remembers the rows you used last time, as Pro Tools does.

#include "TrackOps.h"

class NewTracksContent : public juce::Component, private juce::KeyListener
{
public:
    NewTracksContent (Session& s, te::Track* after) : session (s), insertAfter (after)
    {
        presets = TrackOps::trackPresetFiles (s);
        addAndMakeVisible (afterToggle);
        afterToggle.setButtonText ("Insert after last selected track");
        afterToggle.setToggleState (insertAfter != nullptr, juce::dontSendNotification);
        afterToggle.setEnabled (insertAfter != nullptr);
        addAndMakeVisible (createBtn);
        addAndMakeVisible (cancelBtn);
        createBtn.setColour (juce::TextButton::buttonColourId, Theme::violet());
        createBtn.onClick = [this] { create(); };
        cancelBtn.onClick = [this] { close(); };
        createBtn.setTooltip ("Create the tracks (Return)");
        cancelBtn.setTooltip ("Close without creating anything (Esc)");
        addAndMakeVisible (hint);
        hint.setFont (Theme::font (11.0f));
        hint.setColour (juce::Label::textColourId, Theme::dim());
        hint.setText ("" SQ_CMD "+Up/Down type  -  " SQ_CMD "+Left/Right mono/stereo  -  " SQ_CMD "+" SQ_OPT "+Up/Down timebase  -  " SQ_CMD "+N add row", juce::dontSendNotification);
        restoreLastRows();
        setSize (940, 150);
        setWantsKeyboardFocus (true);
        addKeyListener (this);
        relayout();
    }

    ~NewTracksContent() override { saveLastRows(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        g.setColour (Theme::line());
        g.drawHorizontalLine (getHeight() - 52, 10.0f, (float) getWidth() - 10.0f);
    }

    void resized() override
    {
        int y = 12;
        for (auto& r : rows) { r->setBounds (8, y, getWidth() - 16, rowH); y += rowH + 4; }
        auto b = getLocalBounds().removeFromBottom (44).reduced (14, 8);
        createBtn.setBounds (b.removeFromRight (100));
        b.removeFromRight (8);
        cancelBtn.setBounds (b.removeFromRight (100));
        b.removeFromRight (12);
        afterToggle.setBounds (b.removeFromLeft (250));
        hint.setBounds (b);
    }

    bool keyPressed (const juce::KeyPress& k) override { return handleKey (k); }

private:
    static constexpr int rowH = 30;

    //==========================================================================
    struct Row : public juce::Component
    {
        Row (NewTracksContent& o) : owner (o)
        {
            for (auto* c : { (juce::Component*) &handle, (juce::Component*) &count, (juce::Component*) &format, (juce::Component*) &type,
                             (juce::Component*) &timebase, (juce::Component*) &name, (juce::Component*) &instrument,
                             (juce::Component*) &remove, (juce::Component*) &add })
            {
                addAndMakeVisible (c);
                c->addKeyListener (&owner);
            }
            for (auto* l : { &lCreate, &lNew, &lIn, &lNamed })
            {
                addAndMakeVisible (l);
                l->setFont (Theme::font (12.0f));
                l->setColour (juce::Label::textColourId, Theme::dim());
                l->setJustificationType (juce::Justification::centred);
            }
            count.setText ("1", juce::dontSendNotification);
            count.setInputRestrictions (3, "0123456789");
            count.setJustification (juce::Justification::centred);
            count.setTooltip ("How many tracks to create in this row (Tab / Shift+Tab move between rows)");
            format.addItem ("Mono", 1);
            format.addItem ("Stereo", 2);
            format.setSelectedId (1, juce::dontSendNotification);
            format.setTooltip ("Track format (" SQ_CMD "+Left / " SQ_CMD "+Right)");
            auto names = TrackOps::newTypeNames();
            for (int i = 0; i < names.size(); ++i) type.addItem (names[i], i + 1);
            if (! owner.presets.isEmpty())
            {
                type.addSectionHeading ("Track Presets");
                for (int i = 0; i < owner.presets.size(); ++i) type.addItem (owner.presets[i].getFileNameWithoutExtension(), presetBase + i);
            }
            type.setSelectedId (1, juce::dontSendNotification);
            type.setTooltip ("Track type or a saved Track Preset (" SQ_CMD "+Up / " SQ_CMD "+Down)");
            timebase.addItem ("Samples", 1);
            timebase.addItem ("Ticks", 2);
            timebase.setSelectedId (1, juce::dontSendNotification);
            timebase.setTooltip ("Samples: clips stay at their time. Ticks: clips follow tempo changes (" SQ_CMD "+" SQ_OPT "+Up / Down)");
            instrument.addItem ("Mothsynth", 1);
            instrument.addItem ("Chrysalis Sampler", 2);
            instrument.addItem ("(none - add one later)", 3);
            instrument.setSelectedId (1, juce::dontSendNotification);
            instrument.setTooltip ("Instrument plug-in for Instrument Tracks");
            type.onChange = [this] { typeChanged (true); };
            format.onChange = [this] { formatTouched = true; };
            remove.setTooltip ("Remove this row (" SQ_CMD "+Shift+Down removes the last row)");
            add.setTooltip ("Add a row below (" SQ_CMD "+N)");
            remove.onClick = [this] { owner.removeRow (this); };
            add.onClick = [this] { owner.addRow (this); };
            handle.setTooltip ("Drag to re-order rows");
            handle.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            handle.onDrag = [this] (int screenY) { owner.dragRow (this, screenY); };
            typeChanged (false);
        }

        bool isPreset() const                { return type.getSelectedId() >= presetBase; }
        TrackOps::NewType kind() const       { return isPreset() ? TrackOps::NewType::audio : (TrackOps::NewType) (type.getSelectedId() - 1); }

        static bool typeHasFormat (TrackOps::NewType t)
        {
            using T = TrackOps::NewType;
            return t == T::audio || t == T::aux || t == T::masterFader || t == T::instrument || t == T::routingFolder;
        }
        static bool typeHasTimebase (TrackOps::NewType t)
        {
            using T = TrackOps::NewType;
            return t == T::audio || t == T::aux || t == T::midi || t == T::instrument;
        }

        void typeChanged (bool userChange)
        {
            auto t = kind();
            const bool preset = isPreset();
            name.setTextToShowWhenEmpty (preset ? owner.presets[type.getSelectedId() - presetBase].getFileNameWithoutExtension()
                                                : TrackOps::defaultBaseName (t), Theme::dim());
            const bool hasFormat = ! preset && typeHasFormat (t);
            format.setEnabled (hasFormat);
            if (! hasFormat) format.setText (preset ? "(preset)" : "-", juce::dontSendNotification);
            else if (format.getSelectedId() == 0 || (userChange && ! formatTouched))
                // Pro Tools' usual starting points: mono audio, stereo for everything that is normally a mix of sources
                format.setSelectedId (t == TrackOps::NewType::audio ? 1 : 2, juce::dontSendNotification);
            const bool hasTb = ! preset && typeHasTimebase (t);
            timebase.setEnabled (hasTb);
            if (! hasTb) timebase.setText ("-", juce::dontSendNotification);
            else if (timebase.getSelectedId() == 0 || userChange)
                timebase.setSelectedId (t == TrackOps::NewType::midi || t == TrackOps::NewType::instrument ? 2 : 1, juce::dontSendNotification);
            instrument.setVisible (! preset && t == TrackOps::NewType::instrument);
            count.setEnabled (preset || t != TrackOps::NewType::masterFader);
            if (! count.isEnabled()) count.setText ("1", juce::dontSendNotification);
            resized();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (0, 2);
            handle.setBounds (r.removeFromLeft (16));
            lCreate.setBounds (r.removeFromLeft (50));
            count.setBounds (r.removeFromLeft (42)); r.removeFromLeft (2);
            lNew.setBounds (r.removeFromLeft (34));
            format.setBounds (r.removeFromLeft (92)); r.removeFromLeft (6);
            type.setBounds (r.removeFromLeft (168));
            lIn.setBounds (r.removeFromLeft (24));
            timebase.setBounds (r.removeFromLeft (92));
            lNamed.setBounds (r.removeFromLeft (50));
            add.setBounds (r.removeFromRight (26)); r.removeFromRight (4);
            remove.setBounds (r.removeFromRight (26)); r.removeFromRight (8);
            if (instrument.isVisible()) { instrument.setBounds (r.removeFromRight (150)); r.removeFromRight (6); }
            name.setBounds (r);
        }

        void stepCombo (juce::ComboBox& c, int delta)
        {
            if (! c.isEnabled()) return;
            juce::Array<int> ids;
            for (int i = 0; i < c.getNumItems(); ++i) if (c.getItemId (i) != 0) ids.add (c.getItemId (i));
            auto idx = ids.indexOf (c.getSelectedId());
            idx = juce::jlimit (0, ids.size() - 1, idx + delta);
            c.setSelectedId (ids[idx], juce::sendNotificationSync);
        }

        TrackOps::NewSpec spec() const
        {
            TrackOps::NewSpec s;
            s.count = juce::jlimit (1, 128, count.getText().getIntValue());
            s.type = kind();
            s.stereo = format.getSelectedId() == 2;
            s.ticks = timebase.getSelectedId() == 2;
            s.name = name.getText();
            s.instrument = instrument.getSelectedId() == 1 ? "4osc" : instrument.getSelectedId() == 2 ? "sampler" : "";
            if (isPreset()) s.preset = owner.presets[type.getSelectedId() - presetBase];
            return s;
        }

        juce::String memo() const
        {
            return count.getText() + "," + juce::String (type.getSelectedId() < presetBase ? type.getSelectedId() : 1) + ","
                 + juce::String (format.getSelectedId()) + "," + juce::String (timebase.getSelectedId()) + "," + juce::String (instrument.getSelectedId());
        }
        void recall (const juce::String& m)
        {
            auto p = juce::StringArray::fromTokens (m, ",", "");
            if (p.size() < 5) return;
            count.setText (juce::String (juce::jlimit (1, 128, p[0].getIntValue())), juce::dontSendNotification);
            type.setSelectedId (juce::jlimit (1, 8, p[1].getIntValue()), juce::dontSendNotification);
            typeChanged (false);
            if (format.isEnabled() && p[2].getIntValue() > 0)   { format.setSelectedId (p[2].getIntValue(), juce::dontSendNotification); formatTouched = true; }
            if (timebase.isEnabled() && p[3].getIntValue() > 0) timebase.setSelectedId (p[3].getIntValue(), juce::dontSendNotification);
            if (p[4].getIntValue() > 0) instrument.setSelectedId (p[4].getIntValue(), juce::dontSendNotification);
        }

        struct Handle : public juce::Component, public juce::SettableTooltipClient
        {
            std::function<void (int)> onDrag;
            void paint (juce::Graphics& g) override
            {
                g.setColour (Theme::dim());
                for (int i = 0; i < 3; ++i) g.fillRect (3, getHeight() / 2 - 5 + i * 4, getWidth() - 6, 2);
            }
            void mouseDrag (const juce::MouseEvent& e) override { if (onDrag) onDrag (e.getScreenPosition().y); }
        };

        NewTracksContent& owner;
        static constexpr int presetBase = 100;
        bool formatTouched = false;
        Handle handle;
        juce::Label lCreate { {}, "Create" }, lNew { {}, "new" }, lIn { {}, "in" }, lNamed { {}, "named" };
        juce::TextEditor count, name;
        juce::ComboBox format, type, timebase, instrument;
        juce::TextButton remove { "-" }, add { "+" };
    };

    //==========================================================================
    Session& session;
    te::SafeSelectable<te::Track> insertAfter;   // weak: never keeps a track of a closed session alive
    juce::Array<juce::File> presets;
    std::vector<std::unique_ptr<Row>> rows;
    juce::TextButton createBtn { "Create" }, cancelBtn { "Cancel" };
    juce::ToggleButton afterToggle;
    juce::Label hint;

    Row* rowWithFocus() const
    {
        for (auto& r : rows) if (r->isParentOf (juce::Component::getCurrentlyFocusedComponent())) return r.get();
        return rows.empty() ? nullptr : rows.back().get();
    }

    bool keyPressed (const juce::KeyPress& k, juce::Component*) override { return handleKey (k); }

    bool handleKey (const juce::KeyPress& k)
    {
        const auto m = k.getModifiers();
        const auto code = k.getKeyCode();
        if (k == juce::KeyPress::returnKey && ! m.isAnyModifierKeyDown()) { create(); return true; }
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        if (! m.isCommandDown()) return false;
        auto* r = rowWithFocus();
        if (r == nullptr) return false;
        if (m.isShiftDown() && code == juce::KeyPress::upKey)   { addRow (r); return true; }
        if (m.isShiftDown() && code == juce::KeyPress::downKey) { removeRow (rows.back().get()); return true; }
        if ((code == 'N' || code == 'n') && ! m.isShiftDown())  { addRow (r); return true; }
        if (code == juce::KeyPress::numberPadAdd)               { addRow (r); return true; }
        if (code == '-' || code == juce::KeyPress::numberPadSubtract) { removeRow (rows.back().get()); return true; }
        if (m.isAltDown() && (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey))
        { r->stepCombo (r->timebase, code == juce::KeyPress::upKey ? -1 : 1); return true; }
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
        { r->stepCombo (r->type, code == juce::KeyPress::upKey ? -1 : 1); return true; }
        if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
        { r->stepCombo (r->format, code == juce::KeyPress::leftKey ? -1 : 1); r->formatTouched = true; return true; }
        return false;
    }

    void addRow (Row* after = nullptr)
    {
        if (rows.size() >= 16) return;
        auto r = std::make_unique<Row> (*this);
        auto* raw = r.get();
        addAndMakeVisible (*r);
        auto pos = rows.end();
        if (after != nullptr)
            for (auto it = rows.begin(); it != rows.end(); ++it) if (it->get() == after) { pos = it + 1; break; }
        if (after != nullptr) { raw->recall (after->memo()); }
        rows.insert (pos, std::move (r));
        relayout();
        raw->name.grabKeyboardFocus();
    }

    void removeRow (Row* r)
    {
        if (rows.size() <= 1 || r == nullptr) return;
        rows.erase (std::remove_if (rows.begin(), rows.end(), [r] (auto& p) { return p.get() == r; }), rows.end());
        relayout();
    }

    void dragRow (Row* r, int screenY)
    {
        auto y = screenY - getScreenY() - 12;
        int target = juce::jlimit (0, (int) rows.size() - 1, y / (rowH + 4));
        int current = 0;
        for (int i = 0; i < (int) rows.size(); ++i) if (rows[(size_t) i].get() == r) current = i;
        if (target == current) return;
        auto moved = std::move (rows[(size_t) current]);
        rows.erase (rows.begin() + current);
        rows.insert (rows.begin() + target, std::move (moved));
        resized();
        repaint();
    }

    void relayout()
    {
        setSize (getWidth(), 12 + (int) rows.size() * (rowH + 4) + 64);
        if (auto dw = findParentComponentOfClass<juce::DialogWindow>()) dw->setSize (dw->getWidth(), getHeight() + dw->getTitleBarHeight());
        resized();
    }

    void saveLastRows()
    {
        juce::StringArray m;
        for (auto& r : rows) m.add (r->memo());
        Prefs::set ("nt_lastRows", m.joinIntoString (";"));
    }

    void restoreLastRows()
    {
        auto saved = juce::StringArray::fromTokens (Prefs::getString ("nt_lastRows"), ";", "");
        saved.removeEmptyStrings();
        for (auto& m : saved)
        {
            if (rows.size() >= 16) break;
            auto r = std::make_unique<Row> (*this);
            addAndMakeVisible (*r);
            r->recall (m);
            rows.push_back (std::move (r));
        }
        if (rows.empty()) { auto r = std::make_unique<Row> (*this); addAndMakeVisible (*r); rows.push_back (std::move (r)); }
    }

    void create()
    {
        saveLastRows();
        juce::StringArray notes;
        juce::Array<te::Track*> all;
        te::Track* after = afterToggle.getToggleState() ? insertAfter.get() : nullptr;
        for (auto& r : rows)
        {
            juce::String note;
            auto made = TrackOps::createTracks (session, r->spec(), after, note);
            if (note.isNotEmpty()) notes.add (note);
            for (auto t : made) { all.add (t); if (! t->isMasterTrack()) after = t; }
        }
        session.log.add ("You", "New tracks: " + juce::String (all.size()));
        if (onCreated) onCreated (all);
        if (! notes.isEmpty()) Ui::message (juce::MessageBoxIconType::InfoIcon, "New Tracks", notes.joinIntoString ("\n"));
        close();
    }

    void close()
    {
        if (auto dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0);
    }

public:
    std::function<void (const juce::Array<te::Track*>&)> onCreated;

    static void show (Session& s, te::Track* after, std::function<void (const juce::Array<te::Track*>&)> created)
    {
        auto c = new NewTracksContent (s, after);
        c->onCreated = std::move (created);
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "New Tracks";
        o.dialogBackgroundColour = Theme::panel();
        o.content.setOwned (c);
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.escapeKeyTriggersCloseButton = true;
        if (auto* w = o.launchAsync())
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<juce::Component> (w)]
            {
                if (sp != nullptr) if (auto* nt = dynamic_cast<NewTracksContent*> (static_cast<juce::DialogWindow*> (sp.getComponent())->getContentComponent()))
                    if (! nt->rows.empty()) nt->rows.front()->count.grabKeyboardFocus();
            });
    }
};
