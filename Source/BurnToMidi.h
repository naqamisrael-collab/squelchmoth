#pragma once
// Burn to MIDI: plays a channel's MIDI clips through a private copy of its Channel Settings (the Rack Channel plug-in:
// arpeggiator, echo, gate, time shift, velocity, pitch) offline, writes what comes out back into the clips as real notes,
// and switches the arpeggiator and echo off so nothing plays twice. Undoable as one step.
#include "Rack.h"

namespace BurnToMidi
{
    struct Result { int clips = 0, notesIn = 0, notesOut = 0; juce::String error; };

    inline Result burn (te::AudioTrack& t)
    {
        Result res;
        SqRackChannelPlugin* rc = nullptr;
        for (auto p : t.pluginList) if ((rc = dynamic_cast<SqRackChannelPlugin*> (p)) != nullptr) break;
        if (rc == nullptr) { res.error = t.getName() + " has no Channel Settings (Rack Channel) to burn"; return res; }
        auto param = [&] (const char* id) -> te::AutomatableParameter* { for (auto ap : rc->getAutomatableParameters()) if (ap->paramID == id) return ap; return nullptr; };
        auto* arp = param ("arpMode"); auto* echo = param ("echoCount");
        const bool arpOn = arp != nullptr && arp->getCurrentValue() > 0.5f, echoOn = echo != nullptr && echo->getCurrentValue() > 0.5f;
        if (! arpOn && ! echoOn) { res.error = "The arpeggiator and echo are off on " + t.getName() + " - nothing to burn"; return res; }

        auto& edit = t.edit; auto& um = edit.getUndoManager(); um.beginNewTransaction ("Burn to MIDI");
        const double sr = 48000.0; const int block = 128;
        for (auto c : t.getClips())
        {
            auto mc = dynamic_cast<te::MidiClip*> (c); if (mc == nullptr || mc->getSequence().getNotes().isEmpty()) continue;
            // a private copy of the channel settings, cut groups off so live channels are never silenced by the burn
            auto st = rc->state.createCopy(); edit.createNewItemID().writeID (st, nullptr);
            auto tmpPtr = edit.getPluginCache().createNewPlugin (st);
            auto tmp = dynamic_cast<SqRackChannelPlugin*> (tmpPtr.get()); if (tmp == nullptr) continue;
            for (auto ap : tmp->getAutomatableParameters()) if (ap->paramID == "cutGroup" || ap->paramID == "cutBy") ap->setParameter (0.0f, juce::dontSendNotification);
            const auto clipRange = mc->getEditTimeRange();
            tmp->baseClassInitialise ({ clipRange.getStart(), sr, block });

            struct Ev { double t; juce::MidiMessage m; };
            std::vector<Ev> in;
            for (auto n : mc->getSequence().getNotes())
            {
                if (n->isMute()) continue;
                auto r = n->getEditTimeRange (*mc).getIntersectionWith (clipRange); if (r.isEmpty()) continue;
                in.push_back ({ r.getStart().inSeconds(), juce::MidiMessage::noteOn (1, n->getNoteNumber(), (juce::uint8) juce::jlimit (1, 127, n->getVelocity())) });
                in.push_back ({ r.getEnd().inSeconds() - 1.0e-6, juce::MidiMessage::noteOff (1, n->getNoteNumber()) });
                ++res.notesIn;
            }
            std::stable_sort (in.begin(), in.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });

            // run the copy block by block; collect what it emits as (time, message)
            std::vector<Ev> out; size_t next = 0;
            juce::AudioBuffer<float> dummy (2, block); te::MidiMessageArray midi;
            const double tail = 8.0 * 60.0 / juce::jmax (20.0, edit.tempoSequence.getBpmAt (clipRange.getEnd()));   // two bars for echoes to finish
            for (double t0 = clipRange.getStart().inSeconds(); t0 < clipRange.getEnd().inSeconds() + tail; t0 += block / sr)
            {
                const double t1 = t0 + block / sr;
                midi.clear();
                while (next < in.size() && in[next].t < t1) { midi.addMidiMessage (in[next].m, juce::jmax (0.0, in[next].t - t0), {}); ++next; }
                dummy.clear();
                te::PluginRenderContext fc (&dummy, 0, block, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (t0), te::TimePosition::fromSeconds (t1)), true, false, true, false);
                tmp->applyToBuffer (fc);
                for (auto& m : midi) out.push_back ({ t0 + m.getTimeStamp(), m });
            }
            tmp->baseClassDeinitialise();

            // pair note-ons with note-offs into notes (anything left hanging ends at the clip end), keep it inside the clip
            struct On { double t; int vel; }; std::map<int, std::vector<On>> open; std::vector<std::tuple<int, double, double, int>> notes;
            for (auto& e : out)
            {
                if (e.m.isNoteOn()) open[e.m.getNoteNumber()].push_back ({ e.t, e.m.getVelocity() });
                else if (e.m.isNoteOff()) { auto& v = open[e.m.getNoteNumber()]; if (! v.empty()) { auto on = v.front(); v.erase (v.begin()); notes.push_back ({ e.m.getNoteNumber(), on.t, e.t, on.vel }); } }
            }
            for (auto& [nn, v] : open) for (auto& on : v) notes.push_back ({ nn, on.t, clipRange.getEnd().inSeconds(), on.vel });
            auto& seq = mc->getSequence();
            seq.clear (&um);
            for (auto& [nn, s, e, vel] : notes)
            {
                if (s >= clipRange.getEnd().inSeconds() - 1.0e-6) continue;   // echoes past the clip end are dropped, as in the pattern
                const auto b0 = mc->getContentBeatAtTime (te::TimePosition::fromSeconds (s)), b1 = mc->getContentBeatAtTime (te::TimePosition::fromSeconds (juce::jmin (e, clipRange.getEnd().inSeconds())));
                seq.addNote (nn, b0, juce::jmax (te::BeatDuration::fromBeats (0.01), b1 - b0), juce::jlimit (1, 127, vel), 0, &um);
                ++res.notesOut;
            }
            ++res.clips;
        }
        if (res.clips == 0) { res.error = t.getName() + " has no MIDI clips with notes (convert the steps with to_piano_roll first)"; return res; }
        if (arp != nullptr) arp->setParameter (0.0f, juce::sendNotification);
        if (echo != nullptr) echo->setParameter (0.0f, juce::sendNotification);
        return res;
    }
}
