#pragma once
// Squelchmoth 1.41 - Console Mode: the whole session through a console in one step. Turning it on puts a Voltage Console
// (the channel's input transformer) at the end of every track's inserts and one at the head of the master (the summing
// stage's output transformer, with a little crosstalk between the two rails). Those it places are marked, so turning it
// off removes exactly those and nothing the user put there by hand. One undo step each way.
#include "Session.h"

namespace ConsoleMode
{
    inline const juce::Identifier& mark() { static const juce::Identifier id ("consoleMode"); return id; }
    struct Status { int tracks = 0, channels = 0; bool bus = false; bool on() const { return bus || channels > 0; } bool complete() const { return bus && channels == tracks; } };

    inline SqVoltageConsolePlugin* placedOn (te::PluginList& list, const char* role)
    {
        for (auto p : list.getPlugins()) if (auto c = dynamic_cast<SqVoltageConsolePlugin*> (p)) if (c->state[mark()].toString() == role) return c;
        return nullptr;
    }

    inline Status status (Session& s)
    {
        Status st; if (s.edit == nullptr) return st;
        for (auto t : s.getTracks()) { ++st.tracks; if (placedOn (t->pluginList, "channel") != nullptr) ++st.channels; }
        st.bus = placedOn (s.edit->getMasterPluginList(), "bus") != nullptr; return st;
    }

    struct Options { float drive = 0.0f, iron = 50.0f, busIron = 70.0f, crosstalk = -80.0f; };

    /** Places what is missing (tracks added since it was last switched on get theirs). Returns how many were placed, or -1 with err. */
    inline int enable (Session& s, const Options& o, juce::String& err)
    {
        if (s.edit == nullptr) { err = "No session is open."; return -1; }
        s.edit->getUndoManager().beginNewTransaction ("Console Mode on"); int placed = 0;
        auto make = [&] (const char* role, float iron, float leak) -> te::Plugin::Ptr
        {
            auto p = s.createPluginByName ("Voltage Console", err); if (p == nullptr) return {};
            if (auto c = dynamic_cast<SqVoltageConsolePlugin*> (p.get()))
            {
                c->state.setProperty (mark(), role, nullptr);
                c->drive->setParameter (o.drive, juce::dontSendNotification); c->iron->setParameter (iron, juce::dontSendNotification); c->crosstalk->setParameter (leak, juce::dontSendNotification);
            }
            return p;
        };
        for (auto t : s.getTracks())
        {
            if (placedOn (t->pluginList, "channel") != nullptr) continue;
            auto p = make ("channel", o.iron, -120.0f); if (p == nullptr) return -1;
            t->pluginList.insertPlugin (p, Session::insertEndIndex (t->pluginList, t), nullptr); ++placed;
        }
        auto& master = s.edit->getMasterPluginList();
        if (placedOn (master, "bus") == nullptr) { auto p = make ("bus", o.busIron, o.crosstalk); if (p == nullptr) return -1; master.insertPlugin (p, 0, nullptr); ++placed; }
        return placed;
    }

    /** Removes the ones Console Mode placed. Returns how many. */
    inline int disable (Session& s)
    {
        if (s.edit == nullptr) return 0;
        s.edit->getUndoManager().beginNewTransaction ("Console Mode off"); int removed = 0;
        auto clear = [&removed] (te::PluginList& list)
        {
            juce::Array<te::Plugin::Ptr> gone; for (auto p : list.getPlugins()) if (dynamic_cast<SqVoltageConsolePlugin*> (p) != nullptr && p->state.hasProperty (mark())) gone.add (p);
            for (auto& p : gone) { p->deleteFromParent(); ++removed; }
        };
        for (auto t : s.getTracks()) clear (t->pluginList);
        clear (s.edit->getMasterPluginList()); return removed;
    }

    inline juce::String describe (const Status& st)
    {
        if (! st.on()) return "Console Mode is off.";
        return "Console Mode is on: a transformer on " + juce::String (st.channels) + " of " + juce::String (st.tracks) + " tracks" + (st.bus ? ", and the summing stage on the master." : "; the master has none.")
               + (st.complete() ? juce::String() : " Some are missing (tracks added since, or removed by hand): turn it on again to fill them in.");
    }
}
