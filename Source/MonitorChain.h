#pragma once
// Squelchmoth 1.10 - the Control Room (Logic wish: a monitor-only chain that never prints).
//
// Any Master insert can be marked "monitor only" (room correction, headphone EQ, a reference-level trim). It is heard
// on playback but bypassed for every bounce, stem export and mastering bounce. Optionally the Master fader can stay out
// of renders too, so pulling the master down for a quiet room never changes the printed level.
// Monitor-only inserts sit after the console's meter tap, so the master's LUFS reading is what gets printed.

#include "Session.h"

namespace MonitorChain
{
    static const juce::Identifier monitorId ("sqmMonitorOnly");

    inline bool isMonitorOnly (te::Plugin& p)   { return (bool) p.state.getProperty (monitorId, false); }

    inline juce::Array<te::Plugin*> plugins (Session& s)
    {
        juce::Array<te::Plugin*> out;
        if (s.edit == nullptr) return out;
        for (auto p : s.edit->getMasterPluginList().getPlugins()) if (isMonitorOnly (*p)) out.add (p);
        return out;
    }

    /** Marks a Master insert monitor-only (moved after the meter tap) or prints it again. No undo step for the flag. */
    inline void setMonitorOnly (Session& s, te::Plugin& p, bool on)
    {
        if (s.edit == nullptr) return;
        p.state.setProperty (monitorId, on, nullptr);
        auto parent = p.state.getParent();
        if (! parent.isValid()) return;
        if (on) parent.moveChild (parent.indexOf (p.state), -1, nullptr);   // last: after the tap and the printed chain
        else
        {   // back into the printed chain, just before the meter tap / first monitor-only insert
            int target = -1;
            for (int i = 0; i < parent.getNumChildren(); ++i)
            {
                auto c = parent.getChild (i);
                if (c == p.state) continue;
                if (c.getProperty (te::IDs::type).toString() == "sqmStripTap" || (bool) c.getProperty (monitorId, false)) { target = i; break; }
            }
            if (target >= 0) parent.moveChild (parent.indexOf (p.state), juce::jmax (0, target - (parent.indexOf (p.state) < target ? 1 : 0)), nullptr);
        }
    }

    inline bool masterFaderPrints()        { return ! Prefs::getBool ("bx_masterFaderOff", false); }

    /** While alive, renders hear neither the monitor-only inserts nor (optionally) the Master fader. Restores both. */
    struct PrintGuard
    {
        explicit PrintGuard (Session& s) : session (s)
        {
            if (s.edit == nullptr) return;
            for (auto p : plugins (s))
                if (p->isEnabled()) { p->state.setProperty (te::IDs::enabled, false, nullptr); bypassed.add (p); }
            if (! masterFaderPrints())
                if (auto vp = s.edit->getMasterVolumePlugin())
                {
                    savedFader = vp->getVolumeDb();
                    if (std::abs (*savedFader) > 0.001f) vp->setVolumeDb (0.0f); else savedFader.reset();
                }
        }
        ~PrintGuard()
        {
            for (auto& p : bypassed) if (p != nullptr) p->state.setProperty (te::IDs::enabled, true, nullptr);
            if (savedFader && session.edit != nullptr)
                if (auto vp = session.edit->getMasterVolumePlugin()) vp->setVolumeDb (*savedFader);
        }
        Session& session;
        juce::Array<te::Plugin::Ptr> bypassed;
        std::optional<float> savedFader;
    };
}
