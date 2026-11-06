#pragma once
// Squelchmoth - Insert Chain presets (.sqmchain): save a channel strip's whole insert chain (every plug-in, its settings and
// bypass state, in order) and load it onto any track - added after the existing inserts or replacing them. FL Studio and
// Studio One users rely on FX-chain presets; Pro Tools only has per-plug-in presets.

#include "Session.h"
#include "Theme.h"

namespace InsertChains
{
    inline juce::File folder()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Squelchmoth").getChildFile ("Insert Chains");
        d.createDirectory();
        return d;
    }

    inline juce::Array<juce::File> list()
    {
        auto files = folder().findChildFiles (juce::File::findFiles, false, "*.sqmchain");
        files.sort();
        return files;
    }

    inline juce::File fileFor (const juce::String& name)
    {
        return folder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".sqmchain");
    }

    inline te::PluginList& listFor (Session& s, te::Track* row)   { return row != nullptr ? row->pluginList : s.edit->getMasterPluginList(); }
    inline te::AudioTrack* audioOf (te::Track* row)               { return dynamic_cast<te::AudioTrack*> (row); }

    /** Saves the strip's inserts (nullptr row = Master). Returns the number of plug-ins saved, or -1 on failure. */
    inline int save (Session& s, te::Track* row, const juce::String& name, juce::String& err)
    {
        auto& l = listFor (s, row);
        auto inserts = Session::getInserts (l, audioOf (row));
        if (inserts.isEmpty()) { err = "There are no inserts on " + (row ? row->getName() : juce::String ("the Master")) + " to save."; return -1; }
        juce::ValueTree chain ("SQM_CHAIN");
        chain.setProperty ("name", name, nullptr);
        chain.setProperty ("version", JUCE_APPLICATION_VERSION_STRING, nullptr);
        for (auto p : inserts)
        {
            if (p->isSynth()) continue;   // instruments belong to Track Presets, not FX chains
            s.edit->flushPluginStateIfNeeded (*p);
            auto copy = p->state.createCopy();
            copy.removeProperty (te::IDs::id, nullptr);
            chain.appendChild (copy, nullptr);
        }
        if (chain.getNumChildren() == 0) { err = "Only an instrument is inserted - save it as a Track Preset instead."; return -1; }
        auto f = fileFor (name);
        auto xml = chain.createXml();
        if (xml == nullptr || ! xml->writeTo (f)) { err = "Couldn't write " + f.getFullPathName(); return -1; }
        return chain.getNumChildren();
    }

    /** Loads a chain onto a strip, after its existing inserts (or replacing them). Returns plug-ins added, or -1. */
    inline int load (Session& s, te::Track* row, const juce::File& f, bool replace, juce::String& err)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("SQM_CHAIN")) { err = f.getFileName() + " isn't an insert chain."; return -1; }
        auto chain = juce::ValueTree::fromXml (*xml);
        auto& l = listFor (s, row);
        auto* um = &s.edit->getUndoManager();
        um->beginNewTransaction ("Load insert chain");
        if (replace)
            for (auto p : Session::getInserts (l, audioOf (row)))
                if (! p->isSynth()) p->deleteFromParent();
        int added = 0, missing = 0;
        for (auto child : chain)
        {
            auto v = child.createCopy();
            te::EditItemID::remapIDs (v, nullptr, *s.edit);
            auto p = s.edit->getPluginCache().createNewPlugin (v);
            if (p == nullptr) { ++missing; continue; }
            const int idx = row != nullptr ? Session::insertEndIndex (l, row) : -1;
            l.insertPlugin (p, idx, nullptr);
            ++added;
        }
        if (missing > 0) err = juce::String (missing) + " plug-in(s) in the chain aren't installed on this computer and were skipped.";
        s.log.add ("You", "Loaded insert chain " + f.getFileNameWithoutExtension() + " on " + (row ? row->getName() : juce::String ("Master")) + " (" + juce::String (added) + " plug-ins)");
        return added;
    }

    /** "Insert Chain" sub-menu for a channel strip: save, load (add / replace) and show the folder. Ids 9100+. */
    inline juce::PopupMenu menu (juce::Array<juce::File>& filesOut)
    {
        juce::PopupMenu m, add, rep;
        filesOut = list();
        for (int i = 0; i < filesOut.size(); ++i)
        {
            add.addItem (9200 + i, filesOut[i].getFileNameWithoutExtension());
            rep.addItem (9400 + i, filesOut[i].getFileNameWithoutExtension());
        }
        m.addItem (9100, "Save insert chain...");
        m.addSubMenu ("Load chain (after existing inserts)", add, ! filesOut.isEmpty());
        m.addSubMenu ("Load chain (replace inserts)", rep, ! filesOut.isEmpty());
        m.addItem (9101, "Show Insert Chains folder");
        return m;
    }

    /** Handles a result from menu(). Returns true if it was one of ours. */
    inline bool handle (Session& s, te::Track* row, int r, const juce::Array<juce::File>& files, std::function<void()> changed)
    {
        if (r == 9101) { folder().revealToUser(); return true; }
        if (r == 9100)
        {
            auto* aw = new juce::AlertWindow ("Save insert chain", "Name for this chain of plug-ins:", juce::MessageBoxIconType::NoIcon);
            aw->addTextEditor ("name", (row ? row->getName() : juce::String ("Master")) + " chain");
            aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            te::Track::Ptr keep (row);
            aw->enterModalState (true, juce::ModalCallbackFunction::create ([&s, aw, keep] (int res)
            {
                if (res != 1) return;
                juce::String err;
                auto n = save (s, keep.get(), aw->getTextEditorContents ("name"), err);
                if (n < 0) Ui::message (juce::MessageBoxIconType::WarningIcon, "Insert chain", err);
                else s.log.add ("You", "Saved insert chain " + aw->getTextEditorContents ("name") + " (" + juce::String (n) + " plug-ins)");
            }), true);
            return true;
        }
        if ((r >= 9200 && r < 9200 + files.size()) || (r >= 9400 && r < 9400 + files.size()))
        {
            const bool replace = r >= 9400;
            juce::String err;
            load (s, row, files[r - (replace ? 9400 : 9200)], replace, err);
            if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Insert chain", err);
            if (changed) changed();
            return true;
        }
        return false;
    }
}
