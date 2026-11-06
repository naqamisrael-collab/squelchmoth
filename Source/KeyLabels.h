#pragma once
// Key labels for the piano roll's keyboard column.
//  - User labels: right-click a key > Rename. Stored on the track as "sqmKeyLabels" ("36=Kick;38=Snare").
//  - Automatic labels: a Hive Pads channel labels every key that has a pad with a sound (the pad's name,
//    which is the kit sound's name after Load Kit); a rack channel labels its root key with the channel name.
//  User labels win over automatic ones.

#include "SqHivePads.h"
#include "Rack.h"

namespace KeyLabels
{
    inline std::map<int, juce::String> userLabels (const te::Track& t)
    {
        std::map<int, juce::String> out;
        juce::StringArray items; items.addTokens (t.state.getProperty ("sqmKeyLabels").toString(), ";", "");
        for (auto& it : items)
        {
            auto eq = it.indexOfChar ('=');
            if (eq > 0) out[it.substring (0, eq).getIntValue()] = it.substring (eq + 1).trim();
        }
        return out;
    }

    inline void setUserLabel (te::Track& t, int note, const juce::String& label)
    {
        auto m = userLabels (t);
        if (label.trim().isEmpty()) m.erase (note); else m[note] = label.trim().replace (";", ",").replace ("=", ":");
        juce::StringArray items;
        for (auto& [n, l] : m) items.add (juce::String (n) + "=" + l);
        if (items.isEmpty()) t.state.removeProperty ("sqmKeyLabels", &t.edit.getUndoManager());
        else t.state.setProperty ("sqmKeyLabels", items.joinIntoString (";"), &t.edit.getUndoManager());
    }

    inline juce::String userLabel (const te::Track& t, int note)
    {
        auto m = userLabels (t);
        auto it = m.find (note);
        return it == m.end() ? juce::String() : it->second;
    }

    /** The automatic label for a key (pad name / channel name), or "". */
    inline juce::String autoLabel (te::Track& t, int note)
    {
        if (auto* at = dynamic_cast<te::AudioTrack*> (&t))
        {
            for (auto p : at->pluginList)
                if (auto* pads = dynamic_cast<SqHivePadsPlugin*> (p))
                {
                    const int pad = pads->padForNote (note);
                    if (pad >= 0) return pads->padName (pad);
                }
            if (Rack::isRackTrack (t) && Rack::rootNote (t) == note)
            {
                // a kit channel (one sound) is labelled by its channel name; a full pads kit is labelled per pad above
                bool hasPads = false;
                for (auto p : at->pluginList) if (dynamic_cast<SqHivePadsPlugin*> (p)) hasPads = true;
                if (! hasPads || Rack::channelType (t) == "sample") return t.getName();
            }
        }
        return {};
    }

    /** User label, else automatic label, else "". */
    inline juce::String labelFor (te::Track& t, int note)
    {
        auto u = userLabel (t, note);
        return u.isNotEmpty() ? u : autoLabel (t, note);
    }

    /** All labelled keys for a track: note -> label (user + automatic). */
    inline std::map<int, juce::String> allLabels (te::Track& t)
    {
        std::map<int, juce::String> out;
        for (int n = 0; n < 128; ++n) { auto l = labelFor (t, n); if (l.isNotEmpty()) out[n] = l; }
        return out;
    }
}
