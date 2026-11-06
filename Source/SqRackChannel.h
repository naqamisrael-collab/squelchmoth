#pragma once
// Squelchmoth 1.8 - the Rack Channel plug-in: FL's Channel Settings (Levels / Polyphony / Time / Tracking /
// Cut groups / Arpeggiator / Echo / Layer) implemented as a MIDI processor that sits at slot 0 of a rack track,
// in front of the instrument. Because it works on MIDI it serves every instrument the same way - Squelchmoth's
// own, third-party VST3 / AU, samplers - it is undoable, saved with the session and visible to Claude.
//
//   Levels    : Mod X / Mod Y (CC 74 / 71 sent on every note and when moved), Pitch (coarse semitones by note
//               transposition, the fine remainder as pitch wheel), Velocity scale.
//   Polyphony : Max polyphony (0 = unlimited; oldest note stolen), Mono, Legato, Porta (pitch-wheel slide of
//               Slide time between mono notes).
//   Time      : Gate (maximum note length in beats, 0 = off), Shift (delay every note by a part of a 16th).
//   Tracking  : Velocity -> Pan / Cut / Mod X / Mod Y and Key -> the same (around Key Mid), sent as CCs.
//   Cut       : Cut group (this channel is cut by...), Cut by (notes of this channel cut group...), Cut itself.
//               Cut groups work across channels with a lock-free generation counter per group: a note-on in a
//               "cut by g" channel silences every "cut group g" channel at the start of its next block.
//   Arp       : Off / Up / Down / Up-Down / Up-Down x2 / Random / As played; Time (beats), Gate, Range 1-4 octaves,
//               Repeat, Chord (none, major, minor, maj7, min7, power, sus4, octave). Synced to the song beat.
//   Echo      : note echoes: count, time (beats), feed (velocity decay), pan ping-pong (CC 10), pitch per echo,
//               cutoff decrement per echo (CC 74).
//   Layer     : a layer channel forwards its notes to its children (Rack::layerChildren) through a lock-free FIFO.

#include "SqInstruments.h"
#include "SqHivePads.h"

class SqRackChannelPlugin : public te::Plugin,
                            public SqDsp::BankOwner
{
public:
    enum P { modX, modY, pitch, velScale,
             maxPoly, slideTime,
             gate, shift,
             velToPan, velToCut, velToModX, velToModY, keyToPan, keyToCut, keyToModX, keyToModY, keyMid,
             cutGroup, cutBy,
             arpMode, arpTime, arpGate, arpRange, arpRepeat, arpChord,
             echoCount, echoTime, echoFeed, echoPan, echoPitch, echoCut,
             numParams };

    static constexpr const char* xmlTypeName = "sqmRackChannel";
    static const char* getPluginName()   { return NEEDS_TRANS("Rack Channel"); }
    juce::String getName() const override { return "Rack Channel"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Rack"; }
    juce::String getSelectableDescription() override { return "Rack Channel settings"; }

    SqRackChannelPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        bank.create (*this, {
            { "modX", "Mod X", 0, 1, 0.5f, 1, "", 2 }, { "modY", "Mod Y", 0, 1, 0.5f, 1, "", 2 }, { "pitch", "Pitch", -4800, 4800, 0, 1, " ct", 0 }, { "velScale", "Velocity", 0, 2, 1, 1, "x", 2 },
            { "maxPoly", "Max Poly", 0, 32, 0, 1, "", 0 }, { "slideTime", "Slide Time", 0, 2, 0.1f, 0.5f, " s", 2 },
            { "gate", "Gate", 0, 4, 0, 0.5f, " beats", 2 }, { "shift", "Shift", 0, 1, 0, 1, "", 2 },
            { "velToPan", "Vel > Pan", -1, 1, 0, 1, "", 2 }, { "velToCut", "Vel > Cut", -1, 1, 0, 1, "", 2 }, { "velToModX", "Vel > Mod X", -1, 1, 0, 1, "", 2 }, { "velToModY", "Vel > Mod Y", -1, 1, 0, 1, "", 2 },
            { "keyToPan", "Key > Pan", -1, 1, 0, 1, "", 2 }, { "keyToCut", "Key > Cut", -1, 1, 0, 1, "", 2 }, { "keyToModX", "Key > Mod X", -1, 1, 0, 1, "", 2 }, { "keyToModY", "Key > Mod Y", -1, 1, 0, 1, "", 2 }, { "keyMid", "Key Mid", 0, 127, 60, 1, "", 0 },
            { "cutGroup", "Cut Group", 0, 99, 0, 1, "", 0 }, { "cutBy", "Cut By", 0, 99, 0, 1, "", 0 },
            { "arpMode", "Arp Mode", 0, 6, 0, 1, "", 0 }, { "arpTime", "Arp Time", 0.0625f, 4, 0.25f, 0.4f, " beats", 4 }, { "arpGate", "Arp Gate", 0.05f, 2, 0.8f, 1, "", 2 }, { "arpRange", "Arp Range", 1, 4, 1, 1, " oct", 0 }, { "arpRepeat", "Arp Repeat", 1, 4, 1, 1, "", 0 }, { "arpChord", "Arp Chord", 0, 7, 0, 1, "", 0 },
            { "echoCount", "Echoes", 0, 16, 0, 1, "", 0 }, { "echoTime", "Echo Time", 0.0625f, 4, 0.5f, 0.4f, " beats", 4 }, { "echoFeed", "Echo Feed", 0, 1, 0.5f, 1, "", 2 }, { "echoPan", "Echo Pan", 0, 1, 0, 1, "", 2 }, { "echoPitch", "Echo Pitch", -12, 12, 0, 1, " st", 0 }, { "echoCut", "Echo Cut", 0, 1, 0, 1, "", 2 } });
        auto um = getUndoManager();
        mono.referTo (state, "mono", um, false); legato.referTo (state, "legato", um, false); porta.referTo (state, "porta", um, false);
        cutSelf.referTo (state, "cutSelf", um, false); pitchOn.referTo (state, "pitchOn", um, true); echoPingPong.referTo (state, "echoPingPong", um, true);
        layerMode.referTo (state, "layerMode", um, 0);
        registry().add (this);
    }
    ~SqRackChannelPlugin() override   { registry().remove (this); notifyListenersOfDeletion(); bank.detach(); }

    SqDsp::ParamBank& getBank() override   { return bank; }
    void resetProps (juce::UndoManager* um) override
    {
        mono.setValue (false, um); legato.setValue (false, um); porta.setValue (false, um); cutSelf.setValue (false, um); pitchOn.setValue (true, um); echoPingPong.setValue (true, um); layerMode.setValue (0, um);
    }

    static juce::StringArray arpModeNames()  { return { "OFF", "UP", "DOWN", "UP-DOWN", "UP-DOWN x2", "RANDOM", "AS PLAYED" }; }
    static juce::StringArray arpChordNames() { return { "NONE", "MAJOR", "MINOR", "MAJ 7", "MIN 7", "POWER", "SUS 4", "OCTAVE" }; }
    static juce::StringArray layerModeNames(){ return { "ALL", "RANDOM", "CROSSFADE", "SEQUENTIAL" }; }

    // te::Plugin
    bool isSynth() override                             { return false; }
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return true; }
    bool producesAudioWhenNoAudioInput() override       { return true; }    // never skipped by dynamic processing: echoes, arp steps and gates keep running without input
    bool needsConstantBufferSize() override             { return false; }
    int getNumOutputChannelsGivenInputs (int n) override { return n; }
    bool canBeAddedToClip() override                    { return false; }
    bool canBeAddedToRack() override                    { return false; }
    double getTailLength() const override               { return 0.0; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate;
        juce::MessageManager::callAsync ([wr = juce::WeakReference<te::Selectable> (this)] { if (auto rc = dynamic_cast<SqRackChannelPlugin*> (wr.get())) if (rc->state.getParent().isValid()) rc->syncCutToInstrument(); });   // a weak reference: never keeps the plug-in (or a closed edit) alive
        held.fill (0.0f); heldOrder.clear(); pending.clear(); sounding.clear(); arpStepIndex = 0; lastArpBeat = -1; clock = 0; lastCutGen = cutGeneration ((int) std::lround (bank[cutGroup])).load();
        injected.reset();
    }
    void deinitialise() override {}
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (auto& cv : bank.values) if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
        for (auto* cv : std::initializer_list<juce::CachedValue<bool>*> { &mono, &legato, &porta, &cutSelf, &pitchOn, &echoPingPong })
            if (v.hasProperty (cv->getPropertyID())) cv->setValue (v.getProperty (cv->getPropertyID()), nullptr);
        if (v.hasProperty (layerMode.getPropertyID())) layerMode.setValue (v.getProperty (layerMode.getPropertyID()), nullptr);
    }

    //==========================================================================
    // cross-channel cut groups: one generation counter per group
    static std::atomic<int>& cutGeneration (int group)   { return SqHivePadsPlugin::cutGeneration (group); }

    /** Tells the Hive Pads on this track which cut group to watch (message thread; called when the Cut Group knob moves). */
    void syncCutToInstrument()
    {
        if (auto t = getOwnerTrack())
            for (auto p : t->pluginList.getPlugins())
                if (auto hp = dynamic_cast<SqHivePadsPlugin*> (p)) hp->watchCutGroup.store ((int) std::lround (bank[cutGroup]));
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (id == juce::Identifier ("cutGroup")) syncCutToInstrument();
    }

    // layer children: the parent pushes raw note events here
    struct Injected { juce::MidiMessage msg; };
    struct InjectQueue
    {
        juce::AbstractFifo fifo { 256 }; std::array<juce::MidiMessage, 256> slots;
        void reset()   { fifo.reset(); }
        bool push (const juce::MidiMessage& m) { int s1, n1, s2, n2; fifo.prepareToWrite (1, s1, n1, s2, n2); if (n1 < 1) return false; slots[(size_t) s1] = m; fifo.finishedWrite (1); return true; }
        bool pop (juce::MidiMessage& m)        { int s1, n1, s2, n2; fifo.prepareToRead (1, s1, n1, s2, n2); if (n1 < 1) return false; m = slots[(size_t) s1]; fifo.finishedRead (1); return true; }
    } injected;

    struct Registry
    {
        juce::SpinLock lock; juce::Array<SqRackChannelPlugin*> all;
        void add (SqRackChannelPlugin* p)    { const juce::SpinLock::ScopedLockType sl (lock); all.addIfNotAlreadyThere (p); }
        void remove (SqRackChannelPlugin* p) { const juce::SpinLock::ScopedLockType sl (lock); all.removeFirstMatchingValue (p); }
        SqRackChannelPlugin* forTrack (te::EditItemID trackID)
        {
            const juce::SpinLock::ScopedTryLockType sl (lock);
            if (! sl.isLocked()) return nullptr;
            for (auto p : all) if (auto t = p->getOwnerTrack()) if (t->itemID == trackID) return p;
            return nullptr;
        }
    };
    static Registry& registry()   { static Registry r; return r; }

    /** Children of a layer channel: track IDs stored on the owning track as "sqmLayerChildren". Set from the UI / MCP. */
    juce::Array<te::EditItemID> layerChildren() const
    {
        juce::Array<te::EditItemID> out;
        if (auto t = getOwnerTrack())
            for (auto& s : juce::StringArray::fromTokens (t->state.getProperty ("sqmLayerChildren").toString(), false))
                out.add (te::EditItemID::fromString (s));
        return out;
    }

    //==========================================================================
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.bufferForMidiMessages == nullptr) return;
        auto& midi = *fc.bufferForMidiMessages;
        const int n = fc.bufferNumSamples;
        const double blockSecs = n / sr;
        const double bpm = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
        const double beatsPerSec = bpm / 60.0;
        const double startBeat = fc.isPlaying ? edit.tempoSequence.toBeats (fc.editTime.getStart()).inBeats() : clock * beatsPerSec;
        const double endBeat = startBeat + blockSecs * beatsPerSec;
        out.clear();
        const int chan = 1;

        auto emit = [&] (const juce::MidiMessage& m, double secsIntoBlock) { out.addMidiMessage (m, juce::jlimit (0.0, juce::jmax (0.0, blockSecs - 1.0e-6), secsIntoBlock), {}); };
        auto beatToSecs = [&] (double beat) { return (beat - startBeat) / beatsPerSec; };

        // 1. cut group hit by another channel -> silence everything we started
        {
            auto& gen = cutGeneration ((int) std::lround (bank[cutGroup]));
            const int g = gen.load();
            if (g != lastCutGen)
            {
                lastCutGen = g;
                for (auto& s : sounding) emit (juce::MidiMessage::noteOff (chan, s.note), 0.0);
                sounding.clear();
                emit (juce::MidiMessage::allNotesOff (chan), 0.0);   // one-shot samplers (Hive Pads) stop on this even without a note-off
            }
        }
        if (midi.isAllNotesOff) { held.fill (0.0f); heldOrder.clear(); sounding.clear(); pending.clear(); }

        // 2. layer children: injected events arrive as if played on this channel
        juce::Array<juce::MidiMessage> input;
        for (auto& m : midi) { auto c = m; c.setTimeStamp (juce::jlimit (0.0, blockSecs, m.getTimeStamp())); input.add (c); }
        { juce::MidiMessage m; while (injected.pop (m)) { m.setTimeStamp (0.0); input.add (m); } }
        std::stable_sort (input.begin(), input.end(), [] (const juce::MidiMessage& a, const juce::MidiMessage& b) { return a.getTimeStamp() < b.getTimeStamp(); });

        // layer parent: forward raw notes to the children (and play nothing itself)
        const bool isLayer = getOwnerTrack() != nullptr && getOwnerTrack()->state.getProperty ("sqmRackType").toString() == "layer";
        if (isLayer)
        {
            auto kids = layerChildren();
            if (! kids.isEmpty())
            {
                const int mode = layerMode.get();
                for (auto& m : input)
                {
                    if (! m.isNoteOnOrOff() && ! m.isController() && ! m.isPitchWheel()) continue;
                    if (m.isNoteOn())
                    {
                        int pick = -1;
                        if (mode == 1) pick = juce::Random::getSystemRandom().nextInt (kids.size());
                        else if (mode == 2) pick = juce::jlimit (0, kids.size() - 1, (int) (m.getFloatVelocity() * (float) kids.size()));
                        else if (mode == 3) { pick = layerSeq % kids.size(); ++layerSeq; }
                        noteOwner[(size_t) m.getNoteNumber()] = pick;
                    }
                    for (int k = 0; k < kids.size(); ++k)
                    {
                        const int owner = m.isNoteOnOrOff() ? noteOwner[(size_t) m.getNoteNumber()] : -1;
                        if (owner >= 0 && owner != k) continue;
                        if (auto child = registry().forTrack (kids[k])) child->injected.push (m);
                    }
                }
            }
            midi.clear();
            clock += blockSecs;
            return;
        }

        const int   coarse = pitchOn.get() ? (int) std::lround (bank[pitch] / 100.0f) : 0;
        const float fine = pitchOn.get() ? bank[pitch] - coarse * 100.0f : 0.0f;
        const bool  arpOn = bank[arpMode] > 0.5f;
        const int   echoes = (int) std::lround (bank[echoCount]);
        const float vScale = bank[velScale];
        const int   maxP = (int) std::lround (bank[maxPoly]);
        const double gateBeats = bank[gate];
        const double shiftSecs = bank[shift] * 0.25 / beatsPerSec;

        auto sendTracking = [&] (int note, float vel, double t)
        {
            const float v = vel * 2.0f - 1.0f, k = juce::jlimit (-1.0f, 1.0f, (float) (note - bank[keyMid]) / 48.0f);
            auto cc = [&] (int num, float base, float amount) { if (std::abs (amount) > 1.0e-4f) emit (juce::MidiMessage::controllerEvent (chan, num, juce::jlimit (0, 127, juce::roundToInt ((base + amount) * 127.0f))), t); };
            cc (10, 0.5f, bank[velToPan] * v * 0.5f + bank[keyToPan] * k * 0.5f);
            cc (74, bank[modX], bank[velToCut] * v * 0.5f + bank[keyToCut] * k * 0.5f + bank[velToModX] * v * 0.5f + bank[keyToModX] * k * 0.5f);
            cc (71, bank[modY], bank[velToModY] * v * 0.5f + bank[keyToModY] * k * 0.5f);
        };

        // mod X / Y and fine pitch follow the knobs live
        if (std::abs (bank[modX] - sentModX) > 0.004f) { sentModX = bank[modX]; emit (juce::MidiMessage::controllerEvent (chan, 74, juce::roundToInt (sentModX * 127.0f)), 0.0); }
        if (std::abs (bank[modY] - sentModY) > 0.004f) { sentModY = bank[modY]; emit (juce::MidiMessage::controllerEvent (chan, 71, juce::roundToInt (sentModY * 127.0f)), 0.0); }
        if (std::abs (fine - sentFine) > 0.5f) { sentFine = fine; emit (juce::MidiMessage::pitchWheel (chan, juce::jlimit (0, 16383, 8192 + juce::roundToInt (fine / 200.0f * 8192.0f))), 0.0); }

        auto startNote = [&] (int note, float vel, double t, bool fromArp)
        {
            // polyphony / cut self / mono
            if (cutSelf.get() || mono.get() || (maxP == 1))
            {
                for (auto& s : sounding) emit (juce::MidiMessage::noteOff (chan, s.note), t);
                if (cutSelf.get() && ! sounding.empty()) emit (juce::MidiMessage::allNotesOff (chan), juce::jmax (0.0, t - 1.0e-5));
            }
            if (cutSelf.get() || mono.get() || maxP == 1) sounding.clear();
            else if (maxP > 1) while ((int) sounding.size() >= maxP) { emit (juce::MidiMessage::noteOff (chan, sounding.front().note), t); sounding.erase (sounding.begin()); }
            if ((int) std::lround (bank[cutBy]) > 0) cutGeneration ((int) std::lround (bank[cutBy])).fetch_add (1);
            // porta: slide from the previous mono note with the pitch wheel (2-semitone range -> only within +-2 st, else jump)
            if (porta.get() && mono.get() && lastNote >= 0 && std::abs (note - lastNote) <= 2 && bank[slideTime] > 0.001f)
                pending.push_back ({ t, juce::MidiMessage::pitchWheel (chan, juce::jlimit (0, 16383, 8192 + juce::roundToInt ((float) (lastNote - note) / 2.0f * 8192.0f))), true });
            sendTracking (note, vel, t);
            const int outNote = juce::jlimit (0, 127, note + coarse);
            emit (juce::MidiMessage::noteOn (chan, outNote, juce::jlimit (0.01f, 1.0f, vel * vScale)), t);
            sounding.push_back ({ outNote, note });
            lastNote = note;
            if (gateBeats > 0.0001) pending.push_back ({ t + gateBeats / beatsPerSec, juce::MidiMessage::noteOff (chan, outNote), false });
            if (! fromArp && echoes > 0)
                for (int e = 1; e <= echoes; ++e)
                {
                    const float ev = vel * std::pow (bank[echoFeed], (float) e);
                    if (ev < 0.02f) break;
                    const double te0 = t + e * bank[echoTime] / beatsPerSec;
                    const int en = juce::jlimit (0, 127, outNote + e * (int) std::lround (bank[echoPitch]));
                    if (bank[echoPan] > 0.001f) pending.push_back ({ te0 - 1.0e-4, juce::MidiMessage::controllerEvent (chan, 10, juce::jlimit (0, 127, juce::roundToInt (64.0f + (echoPingPong.get() && (e % 2) ? -1.0f : 1.0f) * bank[echoPan] * 63.0f))), false });
                    if (bank[echoCut] > 0.001f) pending.push_back ({ te0 - 1.0e-4, juce::MidiMessage::controllerEvent (chan, 74, juce::jlimit (0, 127, juce::roundToInt ((bank[modX] - bank[echoCut] * e / (float) juce::jmax (1, echoes)) * 127.0f))), false });
                    pending.push_back ({ te0, juce::MidiMessage::noteOn (chan, en, ev * vScale), false });
                    pending.push_back ({ te0 + juce::jmax (0.02, gateBeats > 0 ? gateBeats / beatsPerSec : 0.9 * bank[echoTime] / beatsPerSec), juce::MidiMessage::noteOff (chan, en), false });
                }
        };
        auto stopNote = [&] (int note, double t)
        {
            for (auto it = sounding.begin(); it != sounding.end();)
            {
                if (it->source == note) { emit (juce::MidiMessage::noteOff (chan, it->note), t); it = sounding.erase (it); }
                else ++it;
            }
            if (mono.get() && ! heldOrder.empty() && legato.get())
            {
                // legato: fall back to the previous held note
                const int back = heldOrder.back();
                if (back != note) startNote (back, held[(size_t) back], t, false);
            }
        };

        for (auto& m : input)
        {
            const double t = m.getTimeStamp() + shiftSecs;
            if (m.isNoteOn())
            {
                held[(size_t) m.getNoteNumber()] = m.getFloatVelocity();
                heldOrder.erase (std::remove (heldOrder.begin(), heldOrder.end(), m.getNoteNumber()), heldOrder.end()); heldOrder.push_back (m.getNoteNumber());
                if (arpOn) continue;
                startNote (m.getNoteNumber(), m.getFloatVelocity(), t, false);
            }
            else if (m.isNoteOff())
            {
                heldOrder.erase (std::remove (heldOrder.begin(), heldOrder.end(), m.getNoteNumber()), heldOrder.end());
                held[(size_t) m.getNoteNumber()] = 0.0f;
                if (arpOn) continue;
                if (gateBeats > 0.0001) continue;   // the gate already scheduled the note-off
                stopNote (m.getNoteNumber(), t);
            }
            else emit (m, m.getTimeStamp());
        }

        // 3. arpeggiator
        if (arpOn)
        {
            if (heldOrder.empty()) { for (auto& s : sounding) emit (juce::MidiMessage::noteOff (chan, s.note), 0.0); sounding.clear(); arpStepIndex = 0; lastArpBeat = -1; }
            else
            {
                const double stepBeats = juce::jmax (0.03125, (double) bank[arpTime]);
                double b = std::ceil (startBeat / stepBeats - 1.0e-6) * stepBeats;
                if (b < startBeat - 1.0e-6) b += stepBeats;
                for (; b < endBeat; b += stepBeats)
                {
                    if (std::abs (b - lastArpBeat) < 1.0e-6) continue;
                    lastArpBeat = b;
                    auto seq = arpSequence();
                    if (seq.empty()) break;
                    const int rep = juce::jmax (1, (int) std::lround (bank[arpRepeat]));
                    const int idx = (arpStepIndex / rep) % (int) seq.size();
                    const int note = seq[(size_t) idx];
                    ++arpStepIndex;
                    const double t = beatToSecs (b);
                    for (auto& s : sounding) emit (juce::MidiMessage::noteOff (chan, s.note), juce::jmax (0.0, t - 1.0e-4));
                    sounding.clear();
                    const float vel = held[(size_t) juce::jlimit (0, 127, note)] > 0.0f ? held[(size_t) juce::jlimit (0, 127, note)] : held[(size_t) heldOrder.back()];
                    for (int cn : chordNotes (note))
                    {
                        const int on = juce::jlimit (0, 127, cn + coarse);
                        sendTracking (cn, vel, t);
                        emit (juce::MidiMessage::noteOn (chan, on, juce::jlimit (0.01f, 1.0f, vel * vScale)), t);
                        sounding.push_back ({ on, cn });
                        pending.push_back ({ t + juce::jmin (0.98, (double) bank[arpGate]) * stepBeats / beatsPerSec, juce::MidiMessage::noteOff (chan, on), false });
                    }
                }
            }
        }

        // 4. scheduled events (gates, echoes, porta)
        for (auto it = pending.begin(); it != pending.end();)
        {
            if (it->time < blockSecs) { emit (it->msg, juce::jmax (0.0, it->time)); it = pending.erase (it); }
            else { it->time -= blockSecs; ++it; }
        }
        for (auto it = sounding.begin(); it != sounding.end();) { ++it; }

        out.sortByTimestamp();
        midi.swapWith (out);
        clock += blockSecs;
    }

    //==========================================================================
    SqDsp::ParamBank bank;
    juce::CachedValue<bool> mono, legato, porta, cutSelf, pitchOn, echoPingPong;
    juce::CachedValue<int> layerMode;

private:
    std::vector<int> arpSequence()
    {
        std::vector<int> base;
        const int mode = (int) std::lround (bank[arpMode]);
        if (mode == 6) base = heldOrder;
        else { for (int i = 0; i < 128; ++i) if (held[(size_t) i] > 0.0f) base.push_back (i); }
        if (base.empty()) return base;
        std::vector<int> ranged;
        const int range = juce::jmax (1, (int) std::lround (bank[arpRange]));
        for (int o = 0; o < range; ++o) for (int nn : base) ranged.push_back (juce::jlimit (0, 127, nn + o * 12));
        std::vector<int> seq;
        switch (mode)
        {
            case 2: seq.assign (ranged.rbegin(), ranged.rend()); break;
            case 3: seq = ranged; if (ranged.size() > 2) for (int i = (int) ranged.size() - 2; i >= 1; --i) seq.push_back (ranged[(size_t) i]); break;
            case 4: seq = ranged; for (auto it = ranged.rbegin(); it != ranged.rend(); ++it) seq.push_back (*it); break;
            case 5: { seq = ranged; juce::Random r ((juce::int64) arpStepIndex * 7919 + 17); for (size_t i = seq.size(); i > 1; --i) std::swap (seq[i - 1], seq[(size_t) r.nextInt ((int) i)]); break; }
            default: seq = ranged; break;
        }
        return seq;
    }
    std::vector<int> chordNotes (int root)
    {
        switch ((int) std::lround (bank[arpChord]))
        {
            case 1: return { root, root + 4, root + 7 };
            case 2: return { root, root + 3, root + 7 };
            case 3: return { root, root + 4, root + 7, root + 11 };
            case 4: return { root, root + 3, root + 7, root + 10 };
            case 5: return { root, root + 7 };
            case 6: return { root, root + 5, root + 7 };
            case 7: return { root, root + 12 };
            default: return { root };
        }
    }

    struct Sounding { int note; int source; };
    struct Pending { double time; juce::MidiMessage msg; bool porta; };
    double sr = 48000.0, clock = 0.0;
    std::array<float, 128> held {};
    std::vector<int> heldOrder;
    std::vector<Sounding> sounding;
    std::vector<Pending> pending;
    te::MidiMessageArray out;
    int arpStepIndex = 0, lastCutGen = 0, lastNote = -1, layerSeq = 0;
    double lastArpBeat = -1;
    float sentModX = -1, sentModY = -1, sentFine = 0;
    std::array<int, 128> noteOwner {};
};
