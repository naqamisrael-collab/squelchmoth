#pragma once
// Squelchmoth - the plug-in menus: every plug-in in its Pro Tools category (or by maker / flat), plus search.

#include "Session.h"
#include "Signature.h"
#include "PluginCatalog.h"
#include "Theme.h"

namespace PluginMenu
{
    enum Filter { all, instruments, effects };
    static constexpr int searchBase = 49900;          // + Filter: "Search plug-ins..."
    static constexpr int signatureBase = 50000;
    static constexpr int builtinBase = 100000;
    static constexpr int thirdPartyBase = 200000;

    inline bool isSearch (int r)              { return r >= searchBase && r < searchBase + 3; }
    inline Filter searchFilter (int r)        { return (Filter) (r - searchBase); }

    /** The third-party list the last menu was built from, so a result always maps to the plug-in that was shown
        even if a background scan changes the list while the menu is open. */
    inline juce::Array<juce::PluginDescription>& menuSnapshot()   { static juce::Array<juce::PluginDescription> a; return a; }

    /** Setup > Preferences > Plugins > Organize plug-in menus by: Category / Manufacturer / Category and Manufacturer / Flat list. */
    inline juce::String organisation()
    {
        static const char* names[] = { "category", "manufacturer", "both", "flat" };
        return names[juce::jlimit (0, 3, Prefs::getInt ("pl_menuOrg", 0))];
    }

    /** Editions: a locked item keeps its place in the menu (so people see what the full program has) with the product
        that unlocks it; choosing it opens Licences and Upgrades instead of inserting. */
    inline juce::String lockLabel (const juce::String& name, const juce::String& type)
    {
        if (Edition::isTypeAllowed (type)) return name;
        auto* p = Edition::productForType (type);
        return name + "   [locked - " + juce::String (p->name) + "]";
    }
    inline bool refuseLocked (const juce::String& type)
    {
        if (Edition::isTypeAllowed (type)) return false;
        if (Edition::showUnlock()) Edition::showUnlock() (Edition::productForType (type)->code);
        return true;
    }

    /** Instrument / plug-in favourites (Studio One wish): names kept in the settings file, shown at the top of every
        plug-in menu. A name matches the menu label without its lock or format suffix. */
    inline juce::StringArray favourites()
    {
        juce::StringArray a; a.addLines (Prefs::getString ("pl_favourites"));
        a.removeEmptyStrings(); a.trim();
        return a;
    }
    inline bool isFavourite (const juce::String& name)   { return favourites().contains (name, true); }
    inline void setFavourite (const juce::String& name, bool on)
    {
        auto a = favourites();
        if (on) a.addIfNotAlreadyThere (name, true); else a.removeString (name, true);
        Prefs::set ("pl_favourites", a.joinIntoString ("\n"));
    }
    inline juce::String plainLabel (const juce::String& label)
    {
        return label.upToFirstOccurrenceOf ("   [locked", false, false).upToFirstOccurrenceOf ("  (", false, false).trim();
    }

    /** Every entry a menu can offer: label, result id, Pro Tools category, maker. */
    struct Entry { juce::String label, category, vendor, group; int id = 0; bool instrument = false; };

    inline std::vector<Entry> entries (te::Engine& engine, Filter filter, bool stereoTarget)
    {
        std::vector<Entry> out;
        auto keep = [filter] (bool inst) { return filter == all || (filter == instruments) == inst; };

        auto& es = Signature::entries();
        for (int i = 0; i < (int) es.size(); ++i)
        {
            bool inst = juce::String (es[(size_t) i].category) == "Instruments";
            if (! keep (inst)) continue;
            out.push_back ({ lockLabel (es[(size_t) i].name, es[(size_t) i].type), PluginCatalog::classifyBuiltIn (es[(size_t) i].name, es[(size_t) i].category), "Squelchmoth", "signature", signatureBase + i, inst });
        }
        for (int i = 0; i < (int) std::size (builtinPlugins); ++i)
        {
            auto& b = builtinPlugins[i];
            bool inst = juce::String (b.category) == "Instrument";
            if (! keep (inst)) continue;
            juce::String cat (b.category);
            if (cat == "AirWindows")
                out.push_back ({ lockLabel (juce::String (b.name).fromFirstOccurrenceOf ("AW ", false, false), b.type), PluginCatalog::classify (juce::String (b.name).fromFirstOccurrenceOf ("AW ", false, false), {}, false),
                                 "AirWindows", "airwindows", builtinBase + i, false });
            else
                out.push_back ({ lockLabel (b.name, b.type), PluginCatalog::classifyBuiltIn (b.name, cat), "Squelchmoth", "builtin", builtinBase + i, inst });
        }
        auto types = engine.getPluginManager().knownPluginList.getTypes();
        menuSnapshot() = types;
        for (auto& it : PluginCatalog::thirdPartyItems (types, stereoTarget))
        {
            if (! keep (it.isInstrument)) continue;
            auto label = it.name + (PluginCatalog::formatPreference() == "both" ? "  (" + it.format + ")" : juce::String());
            out.push_back ({ label, it.category, it.vendor, "thirdparty", thirdPartyBase + it.typeIndex, it.isInstrument });
        }
        return out;
    }

    /** Pro Tools-style plug-in menu: by Category (default), Manufacturer, Category and Manufacturer, or a flat list
        (Setup > Preferences > Plugins). stereoTarget picks the right channel layout of shell plug-ins (Waves...). */
    inline juce::PopupMenu build (te::Engine& engine, Filter filter, bool stereoTarget = true)
    {
        juce::PopupMenu root;
        root.addItem (searchBase + (int) filter, "Search plug-ins...");
        root.addSeparator();

        auto all = entries (engine, filter, stereoTarget);
        {
            const auto favs = favourites();
            juce::PopupMenu fm;
            for (auto& f : favs) for (auto& e : all) if (plainLabel (e.label).equalsIgnoreCase (f)) { fm.addItem (e.id, e.label); break; }
            if (fm.getNumItems() > 0) { root.addSubMenu (juce::String::fromUTF8 ("\xe2\x98\x85 Favourites"), fm); root.addSeparator(); }
        }
        const auto org = organisation();
        int numThird = 0;
        for (auto& e : all) if (e.group == "thirdparty") ++numThird;

        auto addAirWindows = [&]
        {
            std::map<juce::String, juce::PopupMenu> byLetter;
            int n = 0;
            for (auto& e : all) if (e.group == "airwindows") { byLetter[e.label.substring (0, 1).toUpperCase()].addItem (e.id, e.label); ++n; }
            if (n == 0) return;
            juce::PopupMenu air;
            for (auto& [letter, m] : byLetter) air.addSubMenu (letter, m);
            root.addSubMenu ("AirWindows (" + juce::String (n) + ")", air);
        };
        auto addLettered = [] (juce::PopupMenu& into, std::vector<const Entry*> items, bool showVendor)
        {
            std::sort (items.begin(), items.end(), [] (auto a, auto b) { return a->label.compareNatural (b->label) < 0; });
            auto text = [showVendor] (const Entry* e) { return e->label + (showVendor && e->group == "thirdparty" ? "   -  " + e->vendor : juce::String()); };
            if (items.size() <= 40) { for (auto e : items) into.addItem (e->id, text (e)); return; }
            std::map<juce::String, juce::PopupMenu> byLetter;
            for (auto e : items)
            {
                auto c = e->label.substring (0, 1).toUpperCase();
                if (! juce::CharacterFunctions::isLetter (c[0])) c = "#";
                byLetter[c].addItem (e->id, text (e));
            }
            for (auto& [letter, m] : byLetter) into.addSubMenu (letter, m);
        };

        if (org == "flat")
        {
            std::vector<const Entry*> items;
            for (auto& e : all) if (e.group != "airwindows") items.push_back (&e);
            addLettered (root, items, true);
            addAirWindows();
        }
        else if (org == "manufacturer")
        {
            std::map<juce::String, std::vector<const Entry*>> byVendor;
            juce::PopupMenu sq;
            for (auto& e : all)
            {
                if (e.group == "airwindows") continue;
                if (e.group == "thirdparty") byVendor[e.vendor].push_back (&e);
            }
            // Squelchmoth's own: by category
            for (auto& cat : PluginCatalog::categories())
            {
                juce::PopupMenu m;
                for (auto& e : all) if ((e.group == "signature" || e.group == "builtin") && e.category == cat) m.addItem (e.id, e.label);
                if (m.getNumItems() > 0) sq.addSubMenu (cat, m);
            }
            root.addSubMenu ("Squelchmoth", sq);
            for (auto& [vendor, items] : byVendor)
            {
                juce::PopupMenu vm;
                if (items.size() > 30)   // big collections (Waves, UAD...) are split by category inside the maker
                    for (auto& cat : PluginCatalog::categories())
                    {
                        std::vector<const Entry*> inCat;
                        for (auto e : items) if (e->category == cat) inCat.push_back (e);
                        if (! inCat.empty()) { juce::PopupMenu cm; addLettered (cm, inCat, false); vm.addSubMenu (cat, cm); }
                    }
                else addLettered (vm, items, false);
                root.addSubMenu (vendor, vm);
            }
            addAirWindows();
        }
        else   // category, or category and manufacturer
        {
            for (auto& cat : PluginCatalog::categories())
            {
                juce::PopupMenu m;
                std::vector<const Entry*> own, third;
                for (auto& e : all)
                {
                    if (e.category != cat || e.group == "airwindows") continue;
                    if (e.group == "thirdparty") third.push_back (&e); else own.push_back (&e);
                }
                if (own.empty() && third.empty()) continue;
                if (! own.empty())
                {
                    m.addSectionHeader ("Squelchmoth");
                    for (auto e : own) m.addItem (e->id, e->label);
                }
                if (! third.empty())
                {
                    m.addSectionHeader ("Your plug-ins");
                    if (org == "both")
                    {
                        std::map<juce::String, std::vector<const Entry*>> byVendor;
                        for (auto e : third) byVendor[e->vendor].push_back (e);
                        for (auto& [vendor, items] : byVendor) { juce::PopupMenu vm; addLettered (vm, items, false); m.addSubMenu (vendor, vm); }
                    }
                    else addLettered (m, third, true);
                }
                root.addSubMenu (cat, m);
            }
            addAirWindows();
        }

        if (numThird == 0)
        {
            root.addSeparator();
            root.addItem (-1, "Your plug-ins: none found yet (Setup > Scan for New Plugins)", false);
        }
        return root;
    }

    //==========================================================================
    /** "Search plug-ins..." - type part of a name, maker or category; Return or double-click inserts. */
    class SearchContent : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener
    {
    public:
        SearchContent (std::vector<Entry> e, std::function<void (int)> chosen) : all (std::move (e)), onChosen (std::move (chosen))
        {
            addAndMakeVisible (field);
            field.setTextToShowWhenEmpty ("Type a plug-in, maker or category (e.g. 'cla', 'reverb', 'waves')", Theme::dim());
            field.addListener (this);
            field.setWantsKeyboardFocus (true);
            addAndMakeVisible (list);
            list.setModel (this);
            list.setRowHeight (22);
            list.setColour (juce::ListBox::backgroundColourId, Theme::bg());
            filterNow();
            setSize (520, 420);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (10);
            field.setBounds (r.removeFromTop (28));
            r.removeFromTop (8);
            list.setBounds (r);
        }
        void visibilityChanged() override { if (isShowing()) field.grabKeyboardFocus(); }

    private:
        std::vector<Entry> all, shown;
        std::function<void (int)> onChosen;
        juce::TextEditor field;
        juce::ListBox list;

        void filterNow()
        {
            shown.clear();
            auto words = juce::StringArray::fromTokens (field.getText().toLowerCase(), " ", "");
            words.removeEmptyStrings();
            for (auto& e : all)
            {
                auto hay = (e.label + " " + e.vendor + " " + e.category).toLowerCase();
                bool ok = true;
                for (auto& w : words) if (! hay.contains (w)) { ok = false; break; }
                if (ok) shown.push_back (e);
            }
            std::sort (shown.begin(), shown.end(), [] (auto& a, auto& b) { return a.label.compareNatural (b.label) < 0; });
            list.updateContent();
            list.selectRow (0);
            list.repaint();
        }
        void textEditorTextChanged (juce::TextEditor&) override  { filterNow(); }
        void textEditorReturnKeyPressed (juce::TextEditor&) override { choose (list.getSelectedRow()); }
        void textEditorEscapeKeyPressed (juce::TextEditor&) override  { close(); }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::downKey) { list.selectRow (juce::jmin ((int) shown.size() - 1, list.getSelectedRow() + 1)); return true; }
            if (k == juce::KeyPress::upKey)   { list.selectRow (juce::jmax (0, list.getSelectedRow() - 1)); return true; }
            return false;
        }
        int getNumRows() override { return (int) shown.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (row >= (int) shown.size()) return;
            auto& e = shown[(size_t) row];
            if (selected) g.fillAll (Theme::violet().withAlpha (0.5f));
            g.setColour (Theme::text());
            g.setFont (Theme::font (13.0f));
            g.drawText (e.label, 8, 0, w - 230, h, juce::Justification::centredLeft, true);
            g.setColour (Theme::dim());
            g.setFont (Theme::font (11.5f));
            g.drawText (e.category + "  -  " + e.vendor, w - 220, 0, 212, h, juce::Justification::centredRight, true);
        }
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { choose (row); }
        void returnKeyPressed (int row) override { choose (row); }
        void choose (int row)
        {
            if (row < 0 || row >= (int) shown.size()) return;
            auto id = shown[(size_t) row].id;
            auto cb = onChosen;
            close();
            if (cb) cb (id);
        }
        void close()
        {
            if (auto dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0);
        }
    };

    inline void showSearch (te::Engine& engine, Filter filter, bool stereoTarget, std::function<void (int)> chosen)
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Search Plug-ins";
        o.dialogBackgroundColour = Theme::panel();
        o.content.setOwned (new SearchContent (entries (engine, filter, stereoTarget), std::move (chosen)));
        o.useNativeTitleBar = true;
        o.resizable = true;
        o.escapeKeyTriggersCloseButton = true;
        o.launchAsync();
    }

    /** Handles a menu result: plug-in ids are passed straight on, "Search plug-ins..." opens the search first. */
    inline void handle (te::Engine& engine, int r, bool stereoTarget, std::function<void (int)> useId)
    {
        if (r <= 0) return;
        if (isSearch (r)) { showSearch (engine, searchFilter (r), stereoTarget, std::move (useId)); return; }
        useId (r);
    }

    inline te::Plugin::Ptr createFromResult (Session& session, int result)
    {
        if (session.edit == nullptr || result <= 0) return {};

        if (result >= signatureBase && result < builtinBase)
        {
            auto idx = result - signatureBase;
            if (idx < (int) Signature::entries().size())
            {
                if (refuseLocked (Signature::entries()[(size_t) idx].type)) return {};
                return Signature::create (*session.edit, Signature::entries()[(size_t) idx]);
            }
            return {};
        }
        if (result >= thirdPartyBase)
        {
            auto types = menuSnapshot().isEmpty() ? session.getEngine().getPluginManager().knownPluginList.getTypes() : menuSnapshot();
            auto idx = result - thirdPartyBase;
            if (idx < types.size())
                return session.edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, types.getReference (idx));
        }
        else if (result >= builtinBase && result - builtinBase < (int) std::size (builtinPlugins))
        {
            if (refuseLocked (builtinPlugins[result - builtinBase].type)) return {};
            return session.edit->getPluginCache().createNewPlugin (builtinPlugins[result - builtinBase].type, {});
        }

        return {};
    }
}
