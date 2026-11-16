#pragma once
// Squelchmoth 1.8 - the Channel Rack model.
//
// The rule: a rack channel IS a track, a pattern IS a clip group. Nothing new is invented underneath the session:
//   * a rack channel is an instrument / sampler track flagged "sqmRack" (any track can join the rack);
//   * a pattern is an engine Scene (name + colour); every rack track holds that pattern's steps as a StepClip
//     (or a MIDI clip once sent to the piano roll) in the Scene's ClipSlot - the engine's clip launcher plays it
//     in Pattern mode, looping in sync, and the timeline is silenced on those tracks while a slot plays;
//   * placing a pattern on the timeline copies each channel's slot clip to a Pattern Clip tagged with the pattern's
//     id; editing the pattern re-syncs every linked Pattern Clip ("Make unique" breaks the link for one clip);
//   * Song mode = nothing launched, the timeline plays as always; Bounce / Export Stems always render the timeline.
// Steps go through the StepClip engine path (sqm* lanes), so swing, shift, repeats and the null tests hold.

#include "BeatLab.h"
#include "DrumKit.h"
#include "Session.h"
#include "SqRackChannel.h"
#include "SqHivePads.h"

namespace Rack
{
    //==========================================================================
    // Channels
    inline bool isRackTrack (const te::Track& t)                   { return (bool) t.state.getProperty ("sqmRack", false); }
    inline void setRackTrack (te::AudioTrack& t, bool on)          { t.state.setProperty ("sqmRack", on, &t.edit.getUndoManager()); }
    inline juce::String channelType (const te::Track& t)           { return t.state.getProperty ("sqmRackType", "instrument").toString(); }
    inline int rootNote (const te::Track& t)                       { return (int) t.state.getProperty ("sqmRackNote", 60); }
    inline void setRootNote (te::AudioTrack& t, int n)             { t.state.setProperty ("sqmRackNote", juce::jlimit (0, 127, n), &t.edit.getUndoManager()); }
    inline bool isZipped (const te::Track& t)                      { return (bool) t.state.getProperty ("sqmRackZip", false); }
    inline void setZipped (te::AudioTrack& t, bool z)              { t.state.setProperty ("sqmRackZip", z, &t.edit.getUndoManager()); }
    inline juce::String group (const te::Track& t)                 { return t.state.getProperty ("sqmRackGroup", "").toString(); }
    inline void setGroup (te::AudioTrack& t, const juce::String& g){ t.state.setProperty ("sqmRackGroup", g, &t.edit.getUndoManager()); }

    inline juce::Array<te::AudioTrack*> channels (te::Edit& edit)
    {
        juce::Array<te::AudioTrack*> out;
        for (auto t : te::getAudioTracks (edit)) if (isRackTrack (*t)) out.add (t);
        return out;
    }

    inline int indexOfChannel (te::Edit& edit, te::AudioTrack* t)   { return channels (edit).indexOf (t); }

    inline SqRackChannelPlugin* rackChannelPlugin (te::AudioTrack& t, bool create)
    {
        for (auto p : t.pluginList.getPlugins()) if (auto rc = dynamic_cast<SqRackChannelPlugin*> (p)) return rc;
        if (! create) return nullptr;
        if (auto p = t.edit.getPluginCache().createNewPlugin (SqRackChannelPlugin::xmlTypeName, {}))
        {
            t.pluginList.insertPlugin (p, 0, nullptr);
            return dynamic_cast<SqRackChannelPlugin*> (p.get());
        }
        return nullptr;
    }

    inline te::Plugin* instrumentOf (te::AudioTrack& t)
    {
        for (auto p : t.pluginList.getPlugins())
            if (p->isSynth() || dynamic_cast<te::SamplerPlugin*> (p) != nullptr || dynamic_cast<te::ExternalPlugin*> (p) != nullptr) return p;
        return nullptr;
    }

    //==========================================================================
    // Patterns (engine scenes)
    inline te::SceneList& scenes (te::Edit& edit)                  { return edit.getSceneList(); }
    inline int numPatterns (te::Edit& edit)                        { return scenes (edit).getNumScenes(); }

    inline juce::String patternId (te::Scene& s)
    {
        if (! s.state.hasProperty ("sqmPatternId")) s.state.setProperty ("sqmPatternId", juce::Uuid().toString(), nullptr);
        return s.state.getProperty ("sqmPatternId").toString();
    }

    inline te::Scene* pattern (te::Edit& edit, int i)
    {
        auto all = scenes (edit).getScenes();
        return juce::isPositiveAndBelow (i, all.size()) ? all[i] : nullptr;
    }

    inline void ensurePatterns (te::Edit& edit, int n)
    {
        auto& sl = scenes (edit);
        const int before = sl.getNumScenes();
        sl.ensureNumberOfScenes (juce::jmax (1, n));
        auto all = sl.getScenes();
        for (int i = before; i < all.size(); ++i)
        {
            if (all[i]->name.get().isEmpty()) all[i]->name = "Pattern " + juce::String (i + 1);
            if (all[i]->colour.get().isTransparent() || all[i]->colour.get() == juce::Colours::transparentBlack)
            {
                static const juce::uint32 cols[] = { 0xffff3ad1, 0xff3ad1ff, 0xffb14dff, 0xffffb23a, 0xff58ff3a, 0xffff5a3a, 0xff19e3ff, 0xffffd23a };
                all[i]->colour = juce::Colour (cols[i % 8]);
            }
            patternId (*all[i]);
        }
    }

    inline int currentPattern (te::Edit& edit)                     { ensurePatterns (edit, 1); return juce::jlimit (0, numPatterns (edit) - 1, (int) edit.state.getProperty ("sqmPattern", 0)); }
    inline void setCurrentPattern (te::Edit& edit, int i)          { ensurePatterns (edit, i + 1); edit.state.setProperty ("sqmPattern", juce::jlimit (0, numPatterns (edit) - 1, i), nullptr); }

    inline juce::String patternName (te::Edit& edit, int i)        { auto s = pattern (edit, i); return s ? (s->name.get().isEmpty() ? "Pattern " + juce::String (i + 1) : s->name.get()) : juce::String(); }
    inline juce::Colour patternColour (te::Edit& edit, int i)      { auto s = pattern (edit, i); return s ? s->colour.get() : juce::Colours::grey; }
    inline void setPatternName (te::Edit& edit, int i, const juce::String& n)   { if (auto s = pattern (edit, i)) s->name = n; }
    inline void setPatternColour (te::Edit& edit, int i, juce::Colour c)       { if (auto s = pattern (edit, i)) s->colour = c; }

    // pattern-wide settings live on the scene: steps (0 = auto), beats per step, swing
    inline int patternSteps (te::Edit& edit, int i)                { auto s = pattern (edit, i); return s ? (int) s->state.getProperty ("sqmSteps", 16) : 16; }
    inline double patternDivision (te::Edit& edit, int i)          { auto s = pattern (edit, i); return s ? (double) s->state.getProperty ("sqmDivision", 0.25) : 0.25; }
    inline float patternSwing (te::Edit& edit, int i)              { auto s = pattern (edit, i); return s ? (float) (double) s->state.getProperty ("sqmSwing", 0.0) : 0.0f; }
    inline double patternLengthBeats (te::Edit& edit, int i)       { return juce::jmax (1, patternSteps (edit, i)) * patternDivision (edit, i); }

    //==========================================================================
    // The slot clip for (channel, pattern)
    inline te::ClipSlot* slotFor (te::AudioTrack& t, int patternIndex)
    {
        ensurePatterns (t.edit, patternIndex + 1);
        auto slots = t.getClipSlotList().getClipSlots();
        if (! juce::isPositiveAndBelow (patternIndex, slots.size())) { t.getClipSlotList().ensureNumberOfSlots (patternIndex + 1); slots = t.getClipSlotList().getClipSlots(); }
        return juce::isPositiveAndBelow (patternIndex, slots.size()) ? slots[patternIndex] : nullptr;
    }

    inline te::Clip* slotClip (te::AudioTrack& t, int patternIndex)
    {
        auto s = slotFor (t, patternIndex);
        return s ? s->getClip() : nullptr;
    }

    /** Builds a one-row step clip for a channel: the row plays the channel's root note. */
    inline void initStepClip (te::StepClip& clip, te::AudioTrack& t, int patternIndex)
    {
        auto& edit = t.edit;
        while (clip.getChannels().size() < 1) clip.insertNewChannel (0);
        while (clip.getChannels().size() > 1) clip.removeChannel (clip.getChannels().size() - 1);
        auto ch = clip.getChannels()[0];
        ch->noteNumber = rootNote (t); ch->name = t.getName(); ch->noteValue = 127;
        auto p = clip.getPattern (0);
        p.setNumNotes (juce::jmax (1, patternSteps (edit, patternIndex)));
        p.setNoteLength (te::BeatDuration::fromBeats (patternDivision (edit, patternIndex)));
        DrumKit::fillWithPattern (clip);
        BeatLab::setSwing (clip, patternSwing (edit, patternIndex));
        if (auto sc = pattern (edit, patternIndex)) clip.state.setProperty ("sqmPatternId", patternId (*sc), nullptr);
    }

    inline te::StepClip* stepClip (te::AudioTrack& t, int patternIndex, bool create)
    {
        auto slot = slotFor (t, patternIndex);
        if (slot == nullptr) return nullptr;
        if (auto c = slot->getClip())
            return dynamic_cast<te::StepClip*> (c);   // may be a MIDI clip (piano roll channel) -> nullptr
        if (! create) return nullptr;
        auto len = te::BeatDuration::fromBeats (patternLengthBeats (t.edit, patternIndex));
        auto clip = dynamic_cast<te::StepClip*> (te::insertNewClip (*slot, te::TrackItem::Type::step, patternName (t.edit, patternIndex), te::BeatRange (te::BeatPosition(), len)));
        if (clip == nullptr) return nullptr;
        initStepClip (*clip, t, patternIndex);
        return clip;
    }

    inline te::MidiClip* midiClip (te::AudioTrack& t, int patternIndex)
    {
        return dynamic_cast<te::MidiClip*> (slotClip (t, patternIndex));
    }

    /** Resizes every channel clip of a pattern to the pattern's length / steps / division. */
    inline void applyPatternSettings (te::Edit& edit, int patternIndex)
    {
        const auto len = te::BeatDuration::fromBeats (patternLengthBeats (edit, patternIndex));
        for (auto t : channels (edit))
        {
            if (auto sc = stepClip (*t, patternIndex, false))
            {
                auto p = sc->getPattern (0);
                const int steps = juce::jmax (1, patternSteps (edit, patternIndex));
                if (p.getNumNotes() != steps) BeatLab::setNumSteps (*sc, steps);
                if (std::abs (p.getNoteLength().inBeats() - patternDivision (edit, patternIndex)) > 1.0e-6) BeatLab::setResolution (*sc, patternDivision (edit, patternIndex));
                BeatLab::setSwing (*sc, patternSwing (edit, patternIndex));
                sc->setLoopRangeBeats ({ te::BeatPosition(), len });
                sc->setLength (edit.tempoSequence.toTime (te::BeatPosition() + len) - edit.tempoSequence.toTime (te::BeatPosition()), true);
            }
            else if (auto mc = midiClip (*t, patternIndex))
                mc->setLoopRangeBeats ({ te::BeatPosition(), len });
        }
    }

    inline void setPatternSteps (te::Edit& edit, int i, int steps)        { if (auto s = pattern (edit, i)) { s->state.setProperty ("sqmSteps", juce::jlimit (1, 512, steps), &edit.getUndoManager()); applyPatternSettings (edit, i); } }
    inline void setPatternDivision (te::Edit& edit, int i, double beats)  { if (auto s = pattern (edit, i)) { s->state.setProperty ("sqmDivision", juce::jlimit (1.0 / 16.0, 4.0, beats), &edit.getUndoManager()); applyPatternSettings (edit, i); } }
    inline void setPatternSwing (te::Edit& edit, int i, float sw)         { if (auto s = pattern (edit, i)) { s->state.setProperty ("sqmSwing", juce::jlimit (0.0f, 1.0f, sw), &edit.getUndoManager()); applyPatternSettings (edit, i); } }

    //==========================================================================
    // Pattern management
    inline int newPattern (te::Edit& edit, const juce::String& name = {})
    {
        const int i = numPatterns (edit);
        ensurePatterns (edit, i + 1);
        if (name.isNotEmpty()) setPatternName (edit, i, name);
        for (auto t : channels (edit)) stepClip (*t, i, true);
        return i;
    }

    inline int clonePattern (te::Edit& edit, int from)
    {
        const int i = newPattern (edit, patternName (edit, from) + " copy");
        if (auto src = pattern (edit, from)) if (auto dst = pattern (edit, i))
        {
            dst->colour = src->colour.get();
            for (auto& prop : { "sqmSteps", "sqmDivision", "sqmSwing" }) if (src->state.hasProperty (prop)) dst->state.setProperty (prop, src->state.getProperty (prop), nullptr);
        }
        for (auto t : channels (edit))
        {
            auto s = slotClip (*t, from); auto d = slotFor (*t, i);
            if (s == nullptr || d == nullptr) continue;
            if (auto old = d->getClip()) old->removeFromParent();
            auto st = s->state.createCopy();
            te::EditItemID::remapIDs (st, nullptr, edit);
            if (auto nc = te::insertClipWithState (*d, st)) if (auto sc = pattern (edit, i)) nc->state.setProperty ("sqmPatternId", patternId (*sc), nullptr);
        }
        return i;
    }

    inline void deletePattern (te::Edit& edit, int i)
    {
        if (numPatterns (edit) <= 1) return;
        if (auto s = pattern (edit, i)) scenes (edit).deleteScene (*s);
        if (currentPattern (edit) >= numPatterns (edit)) setCurrentPattern (edit, numPatterns (edit) - 1);
    }

    inline bool patternHasNotes (te::Edit& edit, int i)
    {
        for (auto t : channels (edit))
        {
            if (auto sc = stepClip (*t, i, false)) { if (BeatLab::patternHasNotes (*sc, 0)) return true; }
            else if (auto mc = midiClip (*t, i)) { if (mc->getSequence().getNumNotes() > 0) return true; }
        }
        return false;
    }

    //==========================================================================
    // Steps (row 0 of the channel's step clip)
    inline te::StepClip::Pattern rowPattern (te::StepClip& c)     { return c.getPattern (0); }
    inline bool step (te::AudioTrack& t, int patternIndex, int i) { auto c = stepClip (t, patternIndex, false); if (! c) return false; auto p = rowPattern (*c); return juce::isPositiveAndBelow (i, p.getNumNotes()) && p.getNote (0, i); }
    inline void setStep (te::AudioTrack& t, int patternIndex, int i, bool on, int vel = 100)
    {
        if (auto c = stepClip (t, patternIndex, true)) { auto p = rowPattern (*c); if (juce::isPositiveAndBelow (i, p.getNumNotes())) BeatLab::setStep (*c, p, 0, i, on, vel); }
    }
    /** Lane values in natural units: velocity 1..127, gate 0.05..2 (steps), chance 0..1, shift -0.5..0.5 (steps), repeat 1..16. */
    inline float lane (te::AudioTrack& t, int patternIndex, BeatLab::Lane l, int i)
    {
        auto c = stepClip (t, patternIndex, false); if (! c) return 0.0f; auto p = rowPattern (*c);
        if (! juce::isPositiveAndBelow (i, p.getNumNotes())) return 0.0f;
        switch (l)
        {
            case BeatLab::Lane::velocity: return (float) p.getVelocity (0, i);
            case BeatLab::Lane::gate:     return (float) p.getGate (0, i);
            case BeatLab::Lane::chance:   return p.getProbability (0, i);
            case BeatLab::Lane::shift:    return BeatLab::getShift (p, 0, i);
            case BeatLab::Lane::repeat:   return (float) BeatLab::getRepeat (p, 0, i);
        }
        return 0.0f;
    }
    inline void setLane (te::AudioTrack& t, int patternIndex, BeatLab::Lane l, int i, float v)
    {
        auto c = stepClip (t, patternIndex, true); if (! c) return; auto p = rowPattern (*c);
        if (! juce::isPositiveAndBelow (i, p.getNumNotes()) || ! p.getNote (0, i)) return;   // lanes belong to steps that are on (setting a velocity would switch the step on)
        switch (l)
        {
            case BeatLab::Lane::velocity: p.setVelocity (0, i, juce::jlimit (1, 127, juce::roundToInt (v))); break;
            case BeatLab::Lane::gate:     p.setGate (0, i, juce::jlimit (0.05, 2.0, (double) v)); break;
            case BeatLab::Lane::chance:   p.setProbability (0, i, juce::jlimit (0.0f, 1.0f, v)); break;
            case BeatLab::Lane::shift:    BeatLab::setShift (*c, p, 0, i, juce::jlimit (-0.5f, 0.5f, v)); break;
            case BeatLab::Lane::repeat:   BeatLab::setRepeat (*c, p, 0, i, juce::jlimit (1, 16, juce::roundToInt (v))); break;
        }
        BeatLab::rebuildSequence (*c);
    }

    // the 1.8 lanes stored as per-step token lists on the pattern row (see the engine patch)
    inline juce::String extraLaneProp (const juce::String& lane)
    {
        if (lane == "release") return "sqmRelease"; if (lane == "pitch") return "sqmPitch"; if (lane == "pan") return "sqmPan";
        if (lane == "modx") return "sqmModX"; if (lane == "mody") return "sqmModY"; if (lane == "note") return "sqmNotes";
        return {};
    }
    inline juce::StringArray extraLane (te::AudioTrack& t, int patternIndex, const juce::String& lane)
    {
        auto c = stepClip (t, patternIndex, false); if (! c) return {};
        auto row = rowPattern (*c).state.getChild (0);
        return juce::StringArray::fromTokens (row.getProperty (extraLaneProp (lane)).toString(), false);
    }
    inline void setExtraLaneValue (te::AudioTrack& t, int patternIndex, const juce::String& lane, int i, const juce::String& value)
    {
        auto c = stepClip (t, patternIndex, true); if (! c) return;
        auto p = rowPattern (*c); auto row = p.state.getChild (0);
        const auto prop = extraLaneProp (lane); if (prop.isEmpty() || ! row.isValid()) return;
        auto toks = juce::StringArray::fromTokens (row.getProperty (prop).toString(), false);
        const juce::String def = lane == "note" ? "0" : (lane == "release" ? "1" : "0");
        while (toks.size() < p.getNumNotes()) toks.add (def);
        if (juce::isPositiveAndBelow (i, toks.size())) toks.set (i, value);
        row.setProperty (prop, toks.joinIntoString (" "), &c->edit.getUndoManager());
        BeatLab::rebuildSequence (*c);
    }
    inline float extraLaneValue (te::AudioTrack& t, int patternIndex, const juce::String& lane, int i)
    {
        auto toks = extraLane (t, patternIndex, lane);
        const float def = lane == "release" ? 1.0f : 0.0f;
        return juce::isPositiveAndBelow (i, toks.size()) ? toks[i].getFloatValue() : def;
    }
    inline void clearExtraLane (te::AudioTrack& t, int patternIndex, const juce::String& lane)
    {
        if (auto c = stepClip (t, patternIndex, false)) { auto row = rowPattern (*c).state.getChild (0); row.removeProperty (extraLaneProp (lane), &c->edit.getUndoManager()); BeatLab::rebuildSequence (*c); }
    }

    //==========================================================================
    // Pattern clips on the timeline (linked copies of the slot clips)
    inline juce::String clipPatternId (te::Clip& c)   { return c.state.getProperty ("sqmPatternId").toString(); }
    inline bool isUnique (te::Clip& c)                 { return (bool) c.state.getProperty ("sqmUnique", false); }
    inline void makeUnique (te::Clip& c)               { c.state.setProperty ("sqmUnique", true, &c.edit.getUndoManager()); }

    inline int patternIndexForId (te::Edit& edit, const juce::String& id)
    {
        auto all = scenes (edit).getScenes();
        for (int i = 0; i < all.size(); ++i) if (patternId (*all[i]) == id) return i;
        return -1;
    }

    /** Copies the musical content (channels, patterns, sequence, sqm lanes) of src into dst, keeping dst's position. */
    inline void copyPatternContent (te::Clip& src, te::Clip& dst)
    {
        auto* um = &dst.edit.getUndoManager();
        for (auto id : { "CHANNELS", "PATTERNS", "SEQUENCE" })
        {
            auto s = src.state.getChildWithName (id); auto d = dst.state.getChildWithName (id);
            if (d.isValid()) dst.state.removeChild (d, um);
            if (s.isValid()) dst.state.addChild (s.createCopy(), -1, um);
        }
        for (auto prop : { "sqmSwing", "repeatSequence", "sqmLoopPattern", "sqmEditPattern" })
            if (src.state.hasProperty (prop)) dst.state.setProperty (prop, src.state.getProperty (prop), um);
        if (auto mSrc = dynamic_cast<te::MidiClip*> (&src)) if (auto mDst = dynamic_cast<te::MidiClip*> (&dst))
            mDst->getSequence().copyFrom (mSrc->getSequence(), um);
    }

    /** Every linked Pattern Clip of a pattern follows the slot clips again (call after editing the pattern).
        A channel added after the pattern was placed gets its clips created at every placed position, and a
        channel whose slot content changed type (steps <-> piano roll) gets its clips replaced. */
    inline void syncPatternClips (te::Edit& edit, int patternIndex)
    {
        auto sc = pattern (edit, patternIndex); if (! sc) return;
        const auto id = patternId (*sc);
        // every start position where this pattern is placed (any channel, linked or unique)
        std::vector<std::pair<te::TimePosition, te::TimeDuration>> places;
        for (auto t : channels (edit))
            for (auto c : t->getClips())
                if (clipPatternId (*c) == id)
                {
                    bool known = false; for (auto& pl : places) if (std::abs ((pl.first - c->getPosition().getStart()).inSeconds()) < 0.002) known = true;
                    if (! known) places.push_back ({ c->getPosition().getStart(), c->getPosition().getLength() });
                }
        for (auto t : channels (edit))
        {
            auto src = slotClip (*t, patternIndex); if (! src) continue;
            for (auto& [start, len] : places)
            {
                te::Clip* existing = nullptr;
                for (auto c : t->getClips()) if (clipPatternId (*c) == id && std::abs ((c->getPosition().getStart() - start).inSeconds()) < 0.002) existing = c;
                if (existing != nullptr && isUnique (*existing)) continue;
                if (existing != nullptr && typeid (*existing) == typeid (*src)) { copyPatternContent (*src, *existing); continue; }
                if (existing != nullptr) existing->removeFromParent();
                auto st = src->state.createCopy();
                te::EditItemID::remapIDs (st, nullptr, edit);
                st.removeProperty ("sqmUnique", nullptr);
                st.setProperty (te::IDs::start, start.inSeconds(), nullptr);
                st.setProperty (te::IDs::length, len.inSeconds(), nullptr);
                st.setProperty (te::IDs::offset, 0.0, nullptr);
                if (auto nc = t->insertClipWithState (st)) { nc->setName (patternName (edit, patternIndex)); nc->setColour (patternColour (edit, patternIndex)); nc->state.setProperty ("sqmPatternId", id, nullptr); }
            }
        }
    }

    /** Places pattern i on the timeline at a bar: one clip per channel, all tagged with the pattern id. */
    inline juce::Array<te::Clip*> placePattern (Session& session, int patternIndex, double bar, int repeats = 1)
    {
        juce::Array<te::Clip*> out;
        auto& edit = *session.edit;
        auto sc = pattern (edit, patternIndex); if (! sc) return out;
        const auto id = patternId (*sc);
        const auto lenBeats = patternLengthBeats (edit, patternIndex);
        const auto startBeat = edit.tempoSequence.toBeats (session.barToTime (bar));
        for (auto t : channels (edit))
        {
            auto src = slotClip (*t, patternIndex);
            if (src == nullptr) src = stepClip (*t, patternIndex, true);
            if (src == nullptr) continue;
            for (int r = 0; r < juce::jmax (1, repeats); ++r)
            {
                auto st = src->state.createCopy();
                te::EditItemID::remapIDs (st, nullptr, edit);
                st.removeProperty ("sqmUnique", nullptr);
                const auto b0 = startBeat + te::BeatDuration::fromBeats (lenBeats * r), b1 = b0 + te::BeatDuration::fromBeats (lenBeats);
                const auto t0 = edit.tempoSequence.toTime (b0), t1 = edit.tempoSequence.toTime (b1);
                st.setProperty (te::IDs::start, t0.inSeconds(), nullptr);
                st.setProperty (te::IDs::length, (t1 - t0).inSeconds(), nullptr);
                st.setProperty (te::IDs::offset, 0.0, nullptr);
                if (auto nc = t->insertClipWithState (st))
                {
                    nc->setName (patternName (edit, patternIndex));
                    nc->setColour (patternColour (edit, patternIndex));
                    nc->state.setProperty ("sqmPatternId", id, nullptr);
                    out.add (nc);
                }
            }
        }
        return out;
    }

    /** Linked siblings of a pattern clip (same pattern id, same start) - moved / trimmed / deleted together. */
    inline juce::Array<te::Clip*> linkedGroup (te::Clip& c)
    {
        juce::Array<te::Clip*> out;
        const auto id = clipPatternId (c); if (id.isEmpty()) return out;
        const auto start = c.getPosition().getStart();
        for (auto t : te::getAudioTracks (c.edit))
            for (auto o : t->getClips())
                if (clipPatternId (*o) == id && std::abs ((o->getPosition().getStart() - start).inSeconds()) < 0.002) out.add (o);
        return out;
    }

    //==========================================================================
    // Pattern / Song mode (the clip launcher does the work)
    inline bool patternMode (te::Edit& edit)   { return (bool) edit.state.getProperty ("sqmPatternMode", false); }

    inline void stopAllSlots (te::Edit& edit)
    {
        for (auto t : te::getAudioTracks (edit))
        {
            for (auto s : t->getClipSlotList().getClipSlots())
                if (auto c = s->getClip()) if (auto lh = c->getLaunchHandle()) lh->stop ({});
            t->playSlotClips = false;
        }
    }

    inline void launchPattern (te::Edit& edit, int patternIndex)
    {
        edit.getLaunchQuantisation().type = te::LaunchQType::none;
        for (auto t : te::getAudioTracks (edit))
        {
            for (auto s : t->getClipSlotList().getClipSlots())
                if (auto c = s->getClip()) if (auto lh = c->getLaunchHandle()) if (s->getIndex() != patternIndex) lh->stop ({});
        }
        for (auto t : channels (edit))
        {
            auto c = slotClip (*t, patternIndex);
            if (c == nullptr) c = stepClip (*t, patternIndex, true);
            if (c != nullptr) if (auto lh = c->getLaunchHandle()) lh->play ({});
            t->playSlotClips = true;
        }
    }

    inline void setPatternMode (te::Edit& edit, bool on)
    {
        edit.state.setProperty ("sqmPatternMode", on, nullptr);
        if (on) launchPattern (edit, currentPattern (edit));
        else stopAllSlots (edit);
    }

    /** Switching pattern while in Pattern mode re-launches the new one. */
    inline void selectPattern (te::Edit& edit, int i)
    {
        setCurrentPattern (edit, i);
        if (patternMode (edit)) launchPattern (edit, currentPattern (edit));
    }

    //==========================================================================
    // Adding channels
    inline void tagAsRackTrack (te::AudioTrack& t, const juce::String& type, int note)
    {
        auto* um = &t.edit.getUndoManager();
        t.state.setProperty ("sqmRack", true, um);
        t.state.setProperty ("sqmRackType", type, um);
        t.state.setProperty ("sqmRackNote", note, um);
        ensurePatterns (t.edit, 1);
        for (int i = 0; i < numPatterns (t.edit); ++i) stepClip (t, i, true);
        rackChannelPlugin (t, true);
    }

    inline juce::Array<te::EditItemID> layerChildren (te::AudioTrack& t)
    {
        juce::Array<te::EditItemID> out;
        for (auto& s : juce::StringArray::fromTokens (t.state.getProperty ("sqmLayerChildren").toString(), false)) out.add (te::EditItemID::fromString (s));
        return out;
    }
    inline void setLayerChildren (te::AudioTrack& t, const juce::Array<te::AudioTrack*>& kids)
    {
        juce::StringArray ids;
        for (auto k : kids) if (k != &t) { ids.add (k->itemID.toString()); rackChannelPlugin (*k, true); }
        t.state.setProperty ("sqmLayerChildren", ids.joinIntoString (" "), &t.edit.getUndoManager());
    }

    /** Instrument channel: a Squelchmoth instrument (by Signature name or type) or a scanned VST3/AU. */
    inline te::AudioTrack* addInstrumentChannel (Session& session, const juce::String& instrument, const juce::String& name = {})
    {
        te::AudioTrack* t = nullptr;
        const Signature::Entry* sigEntry = nullptr;
        if (auto d = session.findPluginDescription (instrument); d != nullptr && d->isInstrument)
            t = session.addTrack ("instrument", name.isNotEmpty() ? name : instrument, {}, d.get());
        else
        {
            juce::String type = "4osc";
            if (auto se = Signature::findByName (instrument); se != nullptr && juce::String (se->category) == "Instruments" && juce::String (se->type) != "sampler") { type = se->type; sigEntry = se; }
            else if (instrument.isNotEmpty() && instrument != "Mothsynth")
                for (auto& e : Signature::entries()) if (juce::String (e.type).equalsIgnoreCase (instrument)) { type = e.type; sigEntry = &e; break; }
            t = session.addTrack ("instrument", name.isNotEmpty() ? name : (sigEntry ? juce::String (sigEntry->name) : instrument.isNotEmpty() ? instrument : juce::String ("Mothsynth")), type);
            if (t != nullptr && sigEntry != nullptr && ! sigEntry->presets.empty())
                for (auto p : t->pluginList.getPlugins())
                    if (p->getPluginType() == type) { Signature::applyPreset (*p, sigEntry->presets.front()); break; }
        }
        if (t != nullptr) tagAsRackTrack (*t, dynamic_cast<te::ExternalPlugin*> (instrumentOf (*t)) != nullptr ? "plugin" : "instrument", 60);
        return t;
    }

    /** Sample channel (FL Channel Sampler class): one sound in a Sampler, played across the keyboard around its root. */
    /** Puts one sample on a Hive Pads instance as the channel's only pad (pad 1 answers to the root note across the keyboard). */
    inline void setChannelSample (te::AudioTrack& t, const juce::File& file, const juce::String& name, int root, float gainDb)
    {
        auto hp = dynamic_cast<SqHivePadsPlugin*> (instrumentOf (t));
        if (hp == nullptr)
        {
            if (auto old = instrumentOf (t)) old->deleteFromParent();
            if (auto p = t.edit.getPluginCache().createNewPlugin (SqHivePadsPlugin::xmlTypeName, {})) { t.pluginList.insertPlugin (p, -1, nullptr); hp = dynamic_cast<SqHivePadsPlugin*> (p.get()); }
        }
        if (hp == nullptr) return;
        hp->clearPad (0);
        hp->setPadProperty (0, "name", name.isNotEmpty() ? name : file.getFileNameWithoutExtension());
        hp->setPadProperty (0, "note", root);
        hp->setPadProperty (0, "gain", (double) gainDb);
        hp->addLayer (0, file, 60, 0.0f);
        t.state.setProperty ("sqmSampleFile", file.getFullPathName(), nullptr);
    }

    inline te::AudioTrack* addSampleChannel (Session& session, const juce::File& file, const juce::String& name = {}, int root = 60, float gainDb = 0.0f)
    {
        auto t = session.addTrack ("instrument", name.isNotEmpty() ? name : file.getFileNameWithoutExtension(), SqHivePadsPlugin::xmlTypeName);
        if (t == nullptr) return nullptr;
        setChannelSample (*t, file, name, root, gainDb);
        t->state.setProperty ("sqmSampleFile", file.getFullPathName(), nullptr);
        t->setColour (juce::Colour (0xffffb23a));
        tagAsRackTrack (*t, "sample", root);
        return t;
    }

    /** A whole kit as sample channels (FL style: one channel per sound). sounds < 0 = all twelve. */
    inline juce::Array<te::AudioTrack*> addKitChannels (Session& session, int kit, int sounds = 8)
    {
        if (! Edition::isKitAllowed (kit)) { session.lastError = juce::String (DrumKit::kits[juce::jlimit (0, DrumKit::numKits - 1, kit)].name) + " is locked. " + Edition::kitLockReason (kit) + " - " + Edition::storeUrl (kit >= 17 ? "BEAT" : "CORE"); return {}; }
        juce::Array<te::AudioTrack*> out;
        auto files = DrumKit::ensureKit (session, juce::jlimit (0, DrumKit::numKits - 1, kit));
        const int n = sounds < 0 ? DrumKit::numSounds : juce::jmin (sounds, DrumKit::numSounds);
        // FL-style order for an 8-channel starter: kick, snare, clap, closed hat, open hat, perc, 808, rim
        static const int order8[] = { 0, 1, 3, 4, 5, 10, 11, 2, 6, 7, 8, 9 };
        for (int k = 0; k < n; ++k)
        {
            const int i = n <= 8 ? order8[k] : k;
            if (auto t = addSampleChannel (session, files[i], DrumKit::sounds[i].name, DrumKit::sounds[i].note, DrumKit::gainsDb[i]))
            {
                t->state.setProperty ("sqmSampleKit", DrumKit::kits[kit].name, nullptr);
                t->state.setProperty ("sqmKitSound", i, nullptr);
                out.add (t);
            }
        }
        return out;
    }

    /** Layer channel: plays its children (tracks whose sqmLayerParent is this track). */
    inline te::AudioTrack* addLayerChannel (Session& session, const juce::String& name = "Layer")
    {
        auto t = session.addTrack ("instrument", name, "4osc");
        if (t == nullptr) return nullptr;
        for (auto p : t->pluginList.getPlugins()) if (p->isSynth()) p->deleteFromParent();   // a layer makes no sound of its own
        tagAsRackTrack (*t, "layer", 60);
        return t;
    }

    /** MIDI Out channel: notes leave on a MIDI output device / channel (no instrument). */
    inline te::AudioTrack* addMidiOutChannel (Session& session, const juce::String& name = "MIDI Out")
    {
        auto t = session.addTrack ("instrument", name, "4osc");
        if (t == nullptr) return nullptr;
        for (auto p : t->pluginList.getPlugins()) if (p->isSynth()) p->deleteFromParent();
        tagAsRackTrack (*t, "midiout", 60);
        return t;
    }

    inline te::AudioTrack* addChannel (Session& session, const juce::String& type, const juce::String& what, const juce::String& name = {})
    {
        if (type == "sample")  return addSampleChannel (session, juce::File (what), name);
        if (type == "layer")   return addLayerChannel (session, name.isNotEmpty() ? name : "Layer");
        if (type == "midiout") return addMidiOutChannel (session, name.isNotEmpty() ? name : "MIDI Out");
        if (type == "kit")     { auto made = addKitChannels (session, DrumKit::kitIndexFor (what), -1); return made.isEmpty() ? nullptr : made.getFirst(); }
        return addInstrumentChannel (session, what, name);
    }

    inline te::AudioTrack* cloneChannel (Session& session, te::AudioTrack& src)
    {
        auto& edit = *session.edit;
        auto st = src.state.createCopy();
        te::EditItemID::remapIDs (st, nullptr, edit);
        auto t = dynamic_cast<te::AudioTrack*> (edit.insertTrack (te::TrackInsertPoint (nullptr, &src), st, nullptr).get());
        if (t != nullptr) { t->setName (src.getName() + " 2"); session.assignMidiInput (*t); }
        return t;
    }

    inline void removeChannel (Session& session, te::AudioTrack& t)
    {
        session.edit->deleteTrack (&t);
    }

    inline void moveChannel (te::Edit& edit, te::AudioTrack& t, int newRackIndex)
    {
        auto ch = channels (edit);
        const int from = ch.indexOf (&t); if (from < 0) return;
        newRackIndex = juce::jlimit (0, ch.size() - 1, newRackIndex);
        if (newRackIndex == from) return;
        auto* target = ch[newRackIndex];
        te::TrackInsertPoint ip (nullptr, newRackIndex > from ? target : (newRackIndex > 0 ? ch[newRackIndex - 1] : nullptr));
        if (newRackIndex == 0) ip = te::TrackInsertPoint (target->getParentTrack(), nullptr);
        edit.moveTrack (&t, ip);
    }

    //==========================================================================
    // Steps -> piano roll
    /** The channel's pattern becomes an editable MIDI clip in the same slot (the steps are converted). */
    inline te::MidiClip* sendToPianoRoll (te::AudioTrack& t, int patternIndex)
    {
        auto slot = slotFor (t, patternIndex); if (! slot) return nullptr;
        if (auto mc = dynamic_cast<te::MidiClip*> (slot->getClip())) return mc;
        auto sc = stepClip (t, patternIndex, false);
        auto& edit = t.edit;
        const auto len = te::BeatDuration::fromBeats (patternLengthBeats (edit, patternIndex));
        juce::MidiMessageSequence seq;
        if (sc != nullptr) seq = sc->generateMidiSequence (te::MidiList::TimeBase::beats);
        const auto id = sc != nullptr ? clipPatternId (*sc) : juce::String();
        if (sc != nullptr) sc->removeFromParent();
        auto mc = dynamic_cast<te::MidiClip*> (te::insertNewClip (*slot, te::TrackItem::Type::midi, patternName (edit, patternIndex), te::BeatRange (te::BeatPosition(), len)));
        if (mc == nullptr) return nullptr;
        seq.updateMatchedPairs();
        auto& list = mc->getSequence();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* e = seq.getEventPointer (i);
            if (! e->message.isNoteOn() || e->noteOffObject == nullptr) continue;
            list.addNote (e->message.getNoteNumber(), te::BeatPosition::fromBeats (e->message.getTimeStamp()),
                          te::BeatDuration::fromBeats (juce::jmax (0.01, e->noteOffObject->message.getTimeStamp() - e->message.getTimeStamp())),
                          e->message.getVelocity(), 0, &edit.getUndoManager());
        }
        mc->setLoopRangeBeats ({ te::BeatPosition(), len });
        if (id.isNotEmpty()) mc->state.setProperty ("sqmPatternId", id, nullptr);
        return mc;
    }

    /** Back from the piano roll to steps (notes on the grid become steps; others are dropped). */
    inline te::StepClip* sendToSteps (te::AudioTrack& t, int patternIndex)
    {
        auto slot = slotFor (t, patternIndex); if (! slot) return nullptr;
        auto mc = dynamic_cast<te::MidiClip*> (slot->getClip());
        if (mc == nullptr) return stepClip (t, patternIndex, true);
        juce::Array<std::tuple<double, int, int, double>> notes;
        for (auto n : mc->getSequence().getNotes()) notes.add ({ n->getStartBeat().inBeats(), n->getNoteNumber(), n->getVelocity(), n->getLengthBeats().inBeats() });
        mc->removeFromParent();
        auto sc = stepClip (t, patternIndex, true); if (! sc) return nullptr;
        auto p = rowPattern (*sc);
        const double div = patternDivision (t.edit, patternIndex);
        for (auto& [start, note, vel, len] : notes)
        {
            const int i = juce::roundToInt (start / div);
            if (! juce::isPositiveAndBelow (i, p.getNumNotes())) continue;
            BeatLab::setStep (*sc, p, 0, i, true, vel);
            if (note != rootNote (t)) setExtraLaneValue (t, patternIndex, "note", i, juce::String (note));
            p.setGate (0, i, juce::jlimit (0.05, 1.0, len / div));
        }
        BeatLab::rebuildSequence (*sc);
        return sc;
    }
}
