#pragma once
// Squelchmoth - Pro Tools style I/O: named input / output / bus paths and the track Input and Output
// selectors used by the Edit window, the Mix window, the track menus, I/O Setup and Claude.
//
//   Inputs   In 1-2 (stereo path)  In 1, In 2 ... (mono)      renamed in Setup > I/O Setup > Input
//   Outputs  Out 1-2 (main) ...                               renamed in Setup > I/O Setup > Output
//   Busses   named after their Aux Input track ("Verb", "Drum Bus") or "Bus 3"  - I/O Setup > Bus
//   MIDI     All, or a single MIDI device
//
// Tracktion keeps every interface channel in exactly one device (mono or a stereo pair), so asking for
// "In 1" on a pair that is currently stereo splits the pair (and asking for "In 1-2" joins it); tracks
// that used the old layout are moved to the matching new path and the change is logged.

#include "Session.h"
#include "EditState.h"

namespace IO
{
    //==========================================================================
    // Track kinds and formats (Pro Tools New Tracks dialog)
    enum class Kind { audio, aux, instrument, midi };

    inline Kind kindOf (te::AudioTrack& t)
    {
        auto k = t.state.getProperty ("sqmKind").toString();
        for (auto p : t.pluginList.getPlugins())
            if (dynamic_cast<te::AuxReturnPlugin*> (p) != nullptr) return Kind::aux;
        if (k == "aux") return Kind::aux;
        if (k == "midi") return Kind::midi;
        if (k == "instrument") return Kind::instrument;
        for (auto p : t.pluginList.getPlugins()) if (p->isSynth()) return Kind::instrument;
        return Kind::audio;
    }

    inline bool usesMidiInput (te::AudioTrack& t)   { auto k = kindOf (t); return k == Kind::instrument || k == Kind::midi; }

    inline juce::String kindName (Kind k)
    {
        switch (k) { case Kind::aux: return "Aux Input"; case Kind::instrument: return "Instrument"; case Kind::midi: return "MIDI"; default: return "Audio"; }
    }

    /** "mono" or "stereo" - set by the New Tracks dialog, otherwise taken from the input path. */
    inline juce::String formatOf (te::AudioTrack& t)
    {
        auto f = t.state.getProperty ("sqmFormat").toString();
        if (f.isNotEmpty()) return f;
        for (auto in : t.edit.getAllInputDevices())
                if (te::isOnTargetTrack (*in, t, 0))
                    if (auto wd = dynamic_cast<te::WaveInputDevice*> (&in->getInputDevice()))
                        return wd->getChannels().size() >= 2 ? "stereo" : "mono";
        return kindOf (t) == Kind::audio ? "mono" : "stereo";
    }

    //==========================================================================
    // Path names
    inline juce::String rangeText (int first, int count)
    {
        return count <= 1 ? juce::String (first + 1) : juce::String (first + 1) + "-" + juce::String (first + count);
    }

    inline std::pair<int, int> rangeOf (const te::ChannelConfiguration& c)
    {
        auto r = c.getChannelRange();
        return { r.first, juce::jmax (1, r.second - r.first) };
    }

    inline juce::String customKey (bool input, int first, int count)   { return juce::String (input ? "io_in_" : "io_out_") + juce::String (first) + "_" + juce::String (count); }

    /** The Pro Tools path name for a range of interface channels: "In 1-2", "In 3", "Out 1-2" (or the user's name). */
    inline juce::String pathName (bool input, int first, int count)
    {
        auto custom = Prefs::getString (customKey (input, first, count).toRawUTF8());
        if (custom.isNotEmpty()) return custom;
        return juce::String (input ? "In " : "Out ") + rangeText (first, count);
    }

    inline juce::String defaultPathName (bool input, int first, int count)   { return juce::String (input ? "In " : "Out ") + rangeText (first, count); }

    inline void setPathName (te::Engine& eng, bool input, int first, int count, const juce::String& name);

    inline juce::String nameOf (te::WaveInputDevice& d)   { auto r = rangeOf (d.getChannels()); return pathName (true, r.first, r.second); }
    inline juce::String nameOf (te::WaveOutputDevice& d)  { auto r = rangeOf (d.getChannels()); return pathName (false, r.first, r.second); }

    inline juce::String midiName (te::InputDevice& d)
    {
        if (d.getDeviceID() == "all_midi_in" || d.getName() == "All MIDI Ins") return "All";
        auto a = d.getAliasIfSet();
        return a.isNotEmpty() ? a : d.getName();
    }

    /** Name shown for any input device (interface path, MIDI, or another track's output). */
    inline juce::String inputDeviceName (te::InputDevice& d)
    {
        if (auto w = dynamic_cast<te::WaveInputDevice*> (&d))
        {
            if (w->isTrackDevice()) return d.getName();
            return nameOf (*w);
        }
        return midiName (d);
    }

    /** Copies the Pro Tools path names onto the engine's device aliases so every list in the app agrees. */
    inline void applyNames (te::Engine& eng)
    {
        auto& dm = eng.getDeviceManager();
        for (auto d : dm.getWaveInputDevices())  if (d != nullptr && ! d->isTrackDevice()) { auto n = nameOf (*d); if (d->getAliasIfSet() != n) d->setAlias (n); }
        for (auto d : dm.getWaveOutputDevices()) if (d != nullptr) { auto n = nameOf (*d); if (d->getAliasIfSet() != n) d->setAlias (n); }
    }

    inline void setPathName (te::Engine& eng, bool input, int first, int count, const juce::String& name)
    {
        auto n = name.trim().substring (0, 31);
        Prefs::set (customKey (input, first, count).toRawUTF8(), n == defaultPathName (input, first, count) ? juce::String() : n);
        applyNames (eng);
    }

    inline void resetAllNames (te::Engine& eng)
    {
        if (auto* f = Prefs::file())
        {
            auto keys = f->getAllProperties().getAllKeys();
            for (auto& k : keys) if (k.startsWith ("io_in_") || k.startsWith ("io_out_")) f->removeValue (k);
            f->saveIfNeeded();
        }
        applyNames (eng);
    }

    /** Total channels the current audio interface offers (inputs or outputs). */
    inline int numInterfaceChannels (te::Engine& eng, bool input)
    {
        int n = 0;
        auto& dm = eng.getDeviceManager();
        if (input) { for (auto d : dm.getWaveInputDevices())  if (d && ! d->isTrackDevice()) n = juce::jmax (n, rangeOf (d->getChannels()).first + rangeOf (d->getChannels()).second); }
        else       { for (auto d : dm.getWaveOutputDevices()) if (d) n = juce::jmax (n, rangeOf (d->getChannels()).first + rangeOf (d->getChannels()).second); }
        return n;
    }

    //==========================================================================
    // Busses (an Aux Input track listens on a bus; sends and track outputs feed it)
    inline juce::Identifier busNameID (int bus)    { return juce::Identifier ("sqmBusName" + juce::String (bus)); }

    inline juce::String busName (Session& s, int bus)
    {
        if (s.edit == nullptr || bus < 0) return "no bus";
        auto custom = s.edit->state.getProperty (busNameID (bus)).toString();
        if (custom.isNotEmpty()) return custom;
        if (auto t = s.findBusTrack (bus)) return t->getName();
        return "Bus " + juce::String (bus + 1);
    }

    inline void setBusName (Session& s, int bus, const juce::String& name)
    {
        if (s.edit == nullptr || bus < 0) return;
        auto n = name.trim();
        auto aux = s.findBusTrack (bus);
        if (aux != nullptr && n == aux->getName()) n = {};
        if (n.isEmpty()) s.edit->state.removeProperty (busNameID (bus), &s.edit->getUndoManager());
        else             s.edit->state.setProperty (busNameID (bus), n, &s.edit->getUndoManager());
    }

    /** Every bus in use (by an Aux Input track or a send), sorted. */
    inline juce::Array<int> allBusses (Session& s)
    {
        juce::Array<int> out;
        for (auto t : s.getTracks())
        {
            if (auto b = s.getBusNumberOfTrack (*t); b >= 0) out.addIfNotAlreadyThere (b);
            for (auto snd : Session::getSends (*t)) out.addIfNotAlreadyThere (snd->getBusNumber());
        }
        out.sort();
        return out;
    }

    //==========================================================================
    // Input assignment
    inline juce::Array<te::InputDeviceInstance*> inputsOf (te::Edit& e, te::AudioTrack& t)
    {
        juce::Array<te::InputDeviceInstance*> r;
        for (auto in : e.getAllInputDevices()) if (te::isOnTargetTrack (*in, t, 0)) r.add (in);
        return r;
    }

    inline te::InputDeviceInstance* instanceFor (te::Edit& e, te::InputDevice& d)
    {
        for (auto in : e.getAllInputDevices()) if (&in->getInputDevice() == &d) return in;
        return nullptr;
    }

    inline te::WaveInputDevice* findWaveIn (te::Engine& eng, int first, int count)
    {
        for (auto d : eng.getDeviceManager().getWaveInputDevices())
            if (d != nullptr && ! d->isTrackDevice() && rangeOf (d->getChannels()) == std::make_pair (first, count)) return d;
        return nullptr;
    }

    inline te::WaveInputDevice* waveInContaining (te::Engine& eng, int channel)
    {
        for (auto d : eng.getDeviceManager().getWaveInputDevices())
            if (d != nullptr && ! d->isTrackDevice() && d->getChannels().containsDeviceChannel (channel)) return d;
        return nullptr;
    }

    inline void clearInputs (te::Edit& e, te::AudioTrack& t)
    {
        for (auto in : inputsOf (e, t))
            [[maybe_unused]] auto res = in->removeTarget (t.itemID, &e.getUndoManager());
    }

    /** Assigns an existing input device to the track (keeps the record-arm state). */
    inline bool assignDevice (te::Edit& e, te::AudioTrack& t, te::InputDevice& d)
    {
        bool wasArmed = false;
        for (auto in : inputsOf (e, t)) if (in->isRecordingEnabled (t.itemID)) wasArmed = true;
        auto inst = instanceFor (e, d);
        if (inst == nullptr) return false;
        clearInputs (e, t);
        if (! inst->setTarget (t.itemID, false, &e.getUndoManager(), 0)) return false;
        if (wasArmed) inst->setRecordingEnabled (t.itemID, true);
        return true;
    }

    /** One device change at a time (each needs the engine to rebuild its device list). */
    struct Queue
    {
        static Queue& get()   { static Queue q; return q; }
        void add (std::function<void (std::function<void()>)> job)   { jobs.push_back (std::move (job)); pump(); }
    private:
        std::deque<std::function<void (std::function<void()>)>> jobs;
        bool busy = false;
        void pump()
        {
            if (busy || jobs.empty()) return;
            busy = true;
            auto job = std::move (jobs.front());
            jobs.pop_front();
            job ([this] { busy = false; juce::MessageManager::callAsync ([this] { pump(); }); });
        }
    };

    /** Waits (up to ~3 s) for a device covering exactly [first, first+count) to appear in the Edit, then calls fn. */
    inline void whenInputReady (Session& s, int first, int count, std::function<void (te::WaveInputDevice*)> fn, int tries = 30)
    {
        auto* editBefore = s.edit.get();
        juce::Timer::callAfterDelay (100, [&s, first, count, fn, tries, editBefore]
        {
            if (s.edit.get() != editBefore) { fn (nullptr); return; }
            auto d = findWaveIn (s.getEngine(), first, count);
            if (d != nullptr && ! d->isEnabled()) s.getEngine().getDeviceManager().setDeviceEnabled (*d, true);
            if (d != nullptr && d->isEnabled() && instanceFor (*s.edit, *d) != nullptr) { fn (d); return; }
            if (tries <= 0) { fn (nullptr); return; }
            whenInputReady (s, first, count, fn, tries - 1);
        });
    }

    /** Interface channels claimed by input changes that are still being applied (so a burst of new tracks
        each gets its own input instead of all landing on In 1). */
    inline std::multiset<int>& reservedChannels()   { static std::multiset<int> r; return r; }

    /** Other tracks whose input would change if [first, first+count) were re-laid out for this track
        (empty when the path already exists as asked, or nobody else uses those channels). */
    inline juce::StringArray layoutConflicts (Session& s, te::AudioTrack& track, int first, int count)
    {
        juce::StringArray names;
        if (s.edit == nullptr || findWaveIn (s.getEngine(), first, count) != nullptr) return names;
        for (int ch = first; ch < first + count; ++ch)
            if (auto d = waveInContaining (s.getEngine(), ch))
                if (auto inst = instanceFor (*s.edit, *d))
                    for (auto id : inst->getTargets())
                        if (id != track.itemID)
                            if (auto t = te::findTrackForID (*s.edit, id)) names.addIfNotAlreadyThere (t->getName() + " (" + nameOf (*d) + ")");
        return names;
    }

    /** Routes interface channels [first, first+count) into a track, re-laying out the interface if needed.
        Other tracks that used the old layout move to the closest new path. */
    inline void setInterfaceInput (Session& s, te::AudioTrack& track, int first, int count, std::function<void (juce::String)> done = {})
    {
        if (s.edit == nullptr) return;
        te::EditItemID tid = track.itemID;
        track.state.setProperty ("sqmIn", juce::String (first) + "," + juce::String (count), nullptr);
        auto* edit = s.edit.get();
        for (int ch = first; ch < first + count; ++ch) reservedChannels().insert (ch);

        Queue::get().add ([&s, tid, first, count, done, edit] (std::function<void()> finished)
        {
            auto finish = [finished, done, first, count] (const juce::String& msg)
            {
                for (int ch = first; ch < first + count; ++ch) if (auto it = reservedChannels().find (ch); it != reservedChannels().end()) reservedChannels().erase (it);
                if (done) done (msg);
                finished();
            };
            if (s.edit.get() != edit) { finish ("The session changed"); return; }
            auto& eng = s.getEngine();
            auto& dm = eng.getDeviceManager();
            auto findTrack = [&s] (te::EditItemID id) { return s.edit != nullptr ? dynamic_cast<te::AudioTrack*> (te::findTrackForID (*s.edit, id)) : nullptr; };

            if (auto d = findWaveIn (eng, first, count))
            {
                if (! d->isEnabled()) dm.setDeviceEnabled (*d, true);
                whenInputReady (s, first, count, [&s, tid, findTrack, finish] (te::WaveInputDevice* dev)
                {
                    auto t = findTrack (tid);
                    if (t == nullptr || dev == nullptr) { finish ("That input is not available"); return; }
                    assignDevice (*s.edit, *t, *dev);
                    finish ({});
                }, 30);
                return;
            }

            // The interface currently groups these channels differently: split or join the pair.
            std::vector<std::pair<te::EditItemID, std::pair<int, int>>> moves;
            juce::Array<te::WaveInputDevice*> affected;
            for (int ch = first; ch < first + count; ++ch)
                if (auto d = waveInContaining (eng, ch)) affected.addIfNotAlreadyThere (d);
            if (affected.isEmpty()) { finish ("Your audio interface has no input " + rangeText (first, count) + " - see Setup > Playback Engine"); return; }

            for (auto d : affected)
                if (auto inst = instanceFor (*s.edit, *d))
                    for (auto targetID : inst->getTargets())
                        if (targetID != tid)
                        {
                            auto r = rangeOf (d->getChannels());
                            // a split pair keeps its left channel; joined monos become the new pair
                            auto newRange = count == 1 ? std::make_pair (r.first, 1) : std::make_pair (first, count);
                            moves.push_back ({ targetID, newRange });
                        }

            if (count == 1) dm.setDeviceNumChannels (*affected.getFirst(), 1);
            else
            {
                auto lead = waveInContaining (eng, first);
                if (lead != nullptr) dm.setDeviceNumChannels (*lead, (uint32_t) count);
            }

            whenInputReady (s, first, count, [&s, tid, findTrack, finish, moves, first, count] (te::WaveInputDevice* dev)
            {
                auto t = findTrack (tid);
                if (t == nullptr || dev == nullptr) { finish ("The interface could not be re-laid out for " + pathName (true, first, count)); return; }
                assignDevice (*s.edit, *t, *dev);
                juce::StringArray movedNames;
                for (auto& [id, range] : moves)
                    if (auto other = findTrack (id))
                        if (auto od = findWaveIn (s.getEngine(), range.first, range.second))
                        {
                            assignDevice (*s.edit, *other, *od);
                            other->state.setProperty ("sqmIn", juce::String (range.first) + "," + juce::String (range.second), nullptr);
                            movedNames.add (other->getName() + " -> " + nameOf (*od));
                        }
                applyNames (s.getEngine());
                if (movedNames.size() > 0)
                    s.log.add ("Squelchmoth", "Input paths re-laid out for " + pathName (true, first, count) + ": " + movedNames.joinIntoString (", "));
                finish ({});
            });
        });
    }

    /** The selector's version: asks first when other tracks' inputs would change with the new layout. */
    inline void setInterfaceInputConfirmed (Session& s, te::AudioTrack& track, int first, int count, std::function<void (juce::String)> done)
    {
        auto conflicts = layoutConflicts (s, track, first, count);
        if (conflicts.isEmpty()) { setInterfaceInput (s, track, first, count, done); return; }
        te::Track::Ptr keep (&track);
        auto newName = pathName (true, first, count);
        auto msg = count == 2
            ? "Your interface carries " + rangeText (first, 1) + " and " + rangeText (first + 1, 1) + " as separate mono paths. Joining them into the stereo path " + newName
              + " also moves these tracks to " + newName + ":\n\n" + conflicts.joinIntoString ("\n") + "\n\n(Tip: give stereo sources their own pair of inputs.)"
            : "Your interface carries these channels as the stereo path " + pathName (true, (first / 2) * 2, 2) + ". Splitting it so this track records " + newName
              + " alone also moves these tracks to the left channel (mono):\n\n" + conflicts.joinIntoString ("\n");
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Change the input layout?", msg, count == 2 ? "Join to stereo" : "Split to mono", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([&s, keep, first, count, done] (int ok)
            {
                if (ok == 1) if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) setInterfaceInput (s, *t, first, count, done);
            }));
    }

    /** Aux Input tracks listen on a bus; audio tracks record what is on the bus (its Aux track's output). */
    inline juce::String setBusInput (Session& s, te::AudioTrack& t, int bus)
    {
        if (s.edit == nullptr) return "No session";
        if (kindOf (t) == Kind::aux)
        {
            for (auto p : t.pluginList.getPlugins())
                if (auto ar = dynamic_cast<te::AuxReturnPlugin*> (p)) { ar->busNumber = bus; t.state.setProperty ("sqmIn", "bus," + juce::String (bus), nullptr); return {}; }
            return "This Aux track has no bus return";
        }
        auto aux = s.findBusTrack (bus);
        if (aux == nullptr) return busName (s, bus) + " has no Aux Input track to record from";
        if (! assignDevice (*s.edit, t, aux->getWaveInputDevice())) return "Could not record from " + busName (s, bus);
        t.state.setProperty ("sqmIn", "bus," + juce::String (bus), nullptr);
        return {};
    }

    inline juce::String setMidiInput (Session& s, te::AudioTrack& t, te::InputDevice* d)
    {
        if (s.edit == nullptr) return "No session";
        if (d == nullptr) { clearInputs (*s.edit, t); t.state.setProperty ("sqmIn", "none", nullptr); return {}; }
        if (! assignDevice (*s.edit, t, *d)) return "That MIDI input is not available";
        t.state.setProperty ("sqmIn", "midi," + d->getDeviceID(), nullptr);
        return {};
    }

    inline void setNoInput (Session& s, te::AudioTrack& t)
    {
        if (s.edit == nullptr) return;
        clearInputs (*s.edit, t);
        t.state.setProperty ("sqmIn", "none", nullptr);
        if (kindOf (t) == Kind::aux)
            for (auto p : t.pluginList.getPlugins())
                if (auto ar = dynamic_cast<te::AuxReturnPlugin*> (p)) ar->busNumber = 31;   // listening on an unused bus = silent
    }

    /** Label for the track's input selector. */
    inline juce::String inputLabel (Session& s, te::AudioTrack& t)
    {
        if (s.edit == nullptr) return {};
        if (kindOf (t) == Kind::aux)
        {
            auto b = s.getBusNumberOfTrack (t);
            auto devs = inputsOf (*s.edit, t);
            if (! devs.isEmpty()) return inputDeviceName (devs.getFirst()->getInputDevice());
            return b >= 0 && b < 31 ? busName (s, b) : juce::String ("no input");
        }
        auto devs = inputsOf (*s.edit, t);
        if (devs.isEmpty()) return "no input";
        auto& d = devs.getFirst()->getInputDevice();
        if (auto w = dynamic_cast<te::WaveInputDevice*> (&d); w != nullptr && w->isTrackDevice())
        {
            for (auto other : s.getTracks())
                if (&other->getWaveInputDevice() == w) { auto b = s.getBusNumberOfTrack (*other); return b >= 0 ? busName (s, b) : other->getName(); }
        }
        return inputDeviceName (d);
    }

    //==========================================================================
    // Outputs
    inline juce::String outputLabel (Session& s, te::AudioTrack& t)
    {
        auto& out = t.getOutput();
        if (auto dest = out.getDestinationTrack())
        {
            auto b = s.getBusNumberOfTrack (*dest);
            return b >= 0 ? busName (s, b) : dest->getName();
        }
        if (out.outputsToNone()) return "no output";
        if (auto od = out.getOutputDevice (false))
        {
            if (auto w = dynamic_cast<te::WaveOutputDevice*> (od)) return nameOf (*w);
            return od->getAliasOrName();
        }
        if (auto def = s.getEngine().getDeviceManager().getDefaultWaveOutDevice()) return nameOf (*def);
        return "Out 1-2";
    }

    inline juce::String masterOutputLabel (Session& s)
    {
        if (auto def = s.getEngine().getDeviceManager().getDefaultWaveOutDevice()) return nameOf (*def);
        return "no output";
    }

    /** True if routing src into dest would loop back on itself. */
    inline bool wouldFeedback (te::AudioTrack& src, te::AudioTrack& dest)
    {
        if (&src == &dest) return true;
        for (auto t = &dest; t != nullptr; t = t->getOutput().getDestinationTrack())
            if (t == &src) return true;
        return dest.getOutput().feedsInto (&src);
    }

    inline juce::String setOutputToBus (Session& s, te::AudioTrack& t, int bus)
    {
        auto aux = s.findBusTrack (bus);
        if (aux == nullptr) return busName (s, bus) + " has no Aux Input track";
        if (wouldFeedback (t, *aux)) return "That would feed back into itself";
        t.getOutput().setOutputToTrack (aux);
        return {};
    }

    //==========================================================================
    // Selector menus - one implementation for the Edit window, the Mix window and the track menus.
    inline void promptNewAux (Session& s, const juce::String& suggested, std::function<void (te::AudioTrack*)> then)
    {
        auto w = std::make_shared<juce::AlertWindow> ("New Track", "Creates a stereo Aux Input track and a bus with this name.", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", suggested);
        w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([&s, w, then] (int r)
        {
            w->setVisible (false);
            if (r != 1) return;
            auto aux = s.addTrack ("bus", w->getTextEditorContents ("name").trim());
            if (aux != nullptr) { aux->state.setProperty ("sqmFormat", "stereo", nullptr); aux->state.setProperty ("sqmKind", "aux", nullptr); }
            if (then) then (aux);
        }));
    }

    inline void showInputMenu (Session& s, te::AudioTrack& track, juce::Component* target, std::function<void()> changed)
    {
        if (s.edit == nullptr) return;
        auto& eng = s.getEngine();
        te::Track::Ptr keep (&track);
        juce::PopupMenu m;
        auto current = inputLabel (s, track);
        m.addItem (1, "no input", true, current == "no input");
        m.addSeparator();

        std::map<int, std::function<void()>> actions;
        int id = 100;
        auto act = [&] (std::function<void()> f) { actions[id] = std::move (f); return id++; };
        auto done = [changed] (juce::String err) { if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Input", err); if (changed) changed(); };

        if (usesMidiInput (track))
        {
            juce::PopupMenu midi;
            for (auto in : s.edit->getAllInputDevices())
            {
                auto type = in->getInputDevice().getDeviceType();
                if (type != te::InputDevice::physicalMidiDevice && type != te::InputDevice::virtualMidiDevice) continue;
                auto* dev = &in->getInputDevice();
                auto nm = midiName (*dev);
                midi.addItem (act ([&s, keep, dev, done] { if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) done (setMidiInput (s, *t, dev)); }), nm, true, te::isOnTargetTrack (*in, track, 0));
            }
            if (midi.getNumItems() == 0) midi.addItem (2, "No MIDI devices connected", false);
            m.addSectionHeader ("MIDI");
            for (juce::PopupMenu::MenuItemIterator it (midi); it.next();) m.addItem (it.getItem());
        }
        else
        {
            const int numIn = numInterfaceChannels (eng, true);
            juce::PopupMenu iface;
            if (numIn == 0) iface.addItem (3, "No audio inputs - Setup > Playback Engine", false);
            juce::PopupMenu stereoPaths, monoPaths;
            for (int ch = 0; ch + 1 < numIn; ch += 2)
            {
                bool cur = findWaveIn (eng, ch, 2) != nullptr && current == pathName (true, ch, 2);
                stereoPaths.addItem (act ([&s, keep, ch, done] { if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) setInterfaceInputConfirmed (s, *t, ch, 2, done); }),
                                     pathName (true, ch, 2) + "  (Stereo)", true, cur);
            }
            for (int ch = 0; ch < numIn; ++ch)
            {
                bool cur = findWaveIn (eng, ch, 1) != nullptr && current == pathName (true, ch, 1);
                monoPaths.addItem (act ([&s, keep, ch, done] { if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) setInterfaceInputConfirmed (s, *t, ch, 1, done); }),
                                   pathName (true, ch, 1) + "  (Mono)", true, cur);
            }
            // Big interfaces: group the paths in banks of 8 channels so the menu stays usable
            auto addGrouped = [&iface, numIn] (juce::PopupMenu& src, bool stereo)
            {
                if (numIn <= 16) { for (juce::PopupMenu::MenuItemIterator it (src); it.next();) iface.addItem (it.getItem()); return; }
                juce::PopupMenu bank;
                int inBank = 0, bankStart = 1;
                const int perBank = stereo ? 4 : 8;
                for (juce::PopupMenu::MenuItemIterator it (src); it.next();)
                {
                    bank.addItem (it.getItem());
                    if (++inBank == perBank)
                    {
                        iface.addSubMenu (juce::String (stereo ? "Stereo " : "Mono ") + juce::String (bankStart) + "-" + juce::String (bankStart + 7), bank);
                        bank = {}; inBank = 0; bankStart += 8;
                    }
                }
                if (inBank > 0) iface.addSubMenu (juce::String (stereo ? "Stereo " : "Mono ") + juce::String (bankStart) + "-" + juce::String (numIn), bank);
            };
            bool stereoFirst = formatOf (track) == "stereo";
            if (stereoFirst) { addGrouped (stereoPaths, true); if (monoPaths.getNumItems()) iface.addSeparator(); addGrouped (monoPaths, false); }
            else             { addGrouped (monoPaths, false); if (stereoPaths.getNumItems()) iface.addSeparator(); addGrouped (stereoPaths, true); }
            m.addSubMenu ("interface", iface);

            juce::PopupMenu busses;
            for (auto b : allBusses (s))
            {
                auto aux = s.findBusTrack (b);
                if (aux == &track && kindOf (track) != Kind::aux) continue;
                bool cur = current == busName (s, b);
                busses.addItem (act ([&s, keep, b, done] { if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) done (setBusInput (s, *t, b)); }), busName (s, b), aux != nullptr || kindOf (track) == Kind::aux, cur);
            }
            if (kindOf (track) == Kind::aux)
            {
                busses.addSeparator();
                busses.addItem (act ([&s, keep, done]
                {
                    if (auto t = dynamic_cast<te::AudioTrack*> (keep.get()))
                    {
                        int b = 0;
                        auto used = allBusses (s);
                        while (used.contains (b) && b < 30) ++b;
                        done (setBusInput (s, *t, b));
                    }
                }), "new bus");
            }
            m.addSubMenu ("bus", busses, busses.getNumItems() > 0);
        }

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [&s, keep, actions, changed] (int r)
        {
            if (r == 1) { if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) setNoInput (s, *t); if (changed) changed(); return; }
            if (auto it = actions.find (r); it != actions.end()) it->second();
        });
    }

    inline void showOutputMenu (Session& s, te::AudioTrack& track, juce::Component* target, std::function<void()> changed)
    {
        if (s.edit == nullptr) return;
        auto& eng = s.getEngine();
        te::Track::Ptr keep (&track);
        juce::PopupMenu m;
        auto current = outputLabel (s, track);
        std::map<int, std::function<void()>> actions;
        int id = 100;
        auto act = [&] (std::function<void()> f) { actions[id] = std::move (f); return id++; };
        auto done = [changed] (juce::String err) { if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Output", err); if (changed) changed(); };
        auto withTrack = [keep] (std::function<void (te::AudioTrack&)> f) { return [keep, f] { if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) f (*t); }; };

        m.addItem (act (withTrack ([done] (te::AudioTrack& t) { t.getOutput().setOutputToNone(); done ({}); })), "no output", true, current == "no output");
        m.addSeparator();

        if (kindOf (track) == Kind::midi)
        {
            juce::PopupMenu inst, devices;
            for (auto t : s.getTracks())
                if (t != &track && kindOf (*t) == Kind::instrument)
                    inst.addItem (act (withTrack ([&s, t, done] (te::AudioTrack& src) { juce::ignoreUnused (s); src.getOutput().setOutputToTrack (t); done ({}); })), t->getName(), true, track.getOutput().outputsToDestTrack (*t));
            auto& dm = eng.getDeviceManager();
            for (int i = 0; i < dm.getNumMidiOutDevices(); ++i)
                if (auto md = dm.getMidiOutDevice (i))
                {
                    auto devID = md->getDeviceID();
                    devices.addItem (act (withTrack ([devID, done] (te::AudioTrack& t) { t.getOutput().setOutputToDeviceID (devID); done ({}); })), md->getAliasOrName(), md->isEnabled(), track.getOutput().getOutputDeviceID() == devID);
                }
            m.addSubMenu ("instrument track", inst, inst.getNumItems() > 0);
            m.addSubMenu ("MIDI device", devices, devices.getNumItems() > 0);
        }
        else
        {
            juce::PopupMenu iface;
            for (auto od : eng.getDeviceManager().getWaveOutputDevices())
            {
                if (od == nullptr) continue;
                auto devID = od->getDeviceID();
                bool isMain = eng.getDeviceManager().getDefaultWaveOutDevice() == od;
                iface.addItem (act (withTrack ([devID, isMain, done] (te::AudioTrack& t)
                {
                    if (isMain) t.getOutput().setOutputToDefaultDevice (false); else t.getOutput().setOutputToDeviceID (devID);
                    done ({});
                })), nameOf (*od) + (isMain ? "  (Main - Master Fader)" : ""), od->isEnabled(), current == nameOf (*od) && track.getOutput().getDestinationTrack() == nullptr);
            }
            if (iface.getNumItems() == 0) iface.addItem (3, "No audio outputs - Setup > Playback Engine", false);
            m.addSubMenu ("interface", iface);

            juce::PopupMenu busses;
            for (auto b : allBusses (s))
            {
                auto aux = s.findBusTrack (b);
                if (aux == nullptr || aux == &track) continue;
                busses.addItem (act (withTrack ([&s, b, done] (te::AudioTrack& t) { done (setOutputToBus (s, t, b)); })), busName (s, b),
                                ! wouldFeedback (track, *aux), track.getOutput().outputsToDestTrack (*aux));
            }
            m.addSubMenu ("bus", busses, busses.getNumItems() > 0);

            juce::PopupMenu tracks;
            for (auto t : s.getTracks())
                if (t != &track && s.getBusNumberOfTrack (*t) < 0)
                    tracks.addItem (act (withTrack ([t, done] (te::AudioTrack& src) { if (wouldFeedback (src, *t)) { done ("That would feed back into itself"); return; } src.getOutput().setOutputToTrack (t); done ({}); })),
                                    t->getName(), ! wouldFeedback (track, *t), track.getOutput().outputsToDestTrack (*t));
            m.addSubMenu ("track", tracks, tracks.getNumItems() > 0);
            m.addSeparator();
            m.addItem (act ([&s, keep, done]
            {
                promptNewAux (s, (keep != nullptr ? keep->getName() : juce::String()) + " Bus", [&s, keep, done] (te::AudioTrack* aux)
                {
                    if (aux == nullptr) return;
                    if (auto t = dynamic_cast<te::AudioTrack*> (keep.get())) t->getOutput().setOutputToTrack (aux);
                    juce::ignoreUnused (s);
                    done ({});
                });
            }), "new track...");
        }

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [actions] (int r)
        {
            if (auto it = actions.find (r); it != actions.end()) it->second();
        });
    }

    inline void showMasterOutputMenu (Session& s, juce::Component* target, std::function<void()> changed)
    {
        juce::PopupMenu m;
        auto& dm = s.getEngine().getDeviceManager();
        std::map<int, juce::String> ids;
        int id = 1;
        for (auto od : dm.getWaveOutputDevices())
            if (od != nullptr)
            {
                ids[id] = od->getDeviceID();
                m.addItem (id++, nameOf (*od), true, dm.getDefaultWaveOutDevice() == od);
            }
        if (ids.empty()) m.addItem (999, "No audio outputs - Setup > Playback Engine", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [&s, ids, changed] (int r)
        {
            if (auto it = ids.find (r); it != ids.end()) { s.getEngine().getDeviceManager().setDefaultWaveOutDevice (it->second); if (changed) changed(); }
        });
    }

    //==========================================================================
    /** After a session opens or the interface changes: put every track back on the path it asked for. */
    inline void assignDefaultInput (Session& s, te::AudioTrack& t, int index, bool stereo);

    inline void restoreInputs (Session& s)
    {
        if (s.edit == nullptr) return;
        applyNames (s.getEngine());
        int legacyIndex = 0;
        for (auto t : s.getTracks())
        {
            auto want = t->state.getProperty ("sqmIn").toString();
            // sessions made before 1.2 have audio tracks with no input at all: give them the next free path like Pro Tools does
            if (want.isEmpty() && ! t->state.hasProperty ("sqmIn") && kindOf (*t) == Kind::audio && inputsOf (*s.edit, *t).isEmpty()
                && ! t->isFrozen (te::Track::anyFreeze) && t->isProcessing (false))
            {
                assignDefaultInput (s, *t, legacyIndex++, formatOf (*t) == "stereo");
                continue;
            }
            // Instrument and MIDI tracks listen to every MIDI input (Pro Tools "All") unless "no input" was chosen
            if (want.isEmpty() && ! t->state.hasProperty ("sqmIn") && (kindOf (*t) == Kind::instrument || kindOf (*t) == Kind::midi)
                && inputsOf (*s.edit, *t).isEmpty() && t->isProcessing (false))
            {
                s.assignMidiInput (*t);
                continue;
            }
            if (want.isEmpty() || want == "none" || want.startsWith ("bus") || want.startsWith ("midi")) continue;
            auto parts = juce::StringArray::fromTokens (want, ",", "");
            if (parts.size() != 2) continue;
            auto d = findWaveIn (s.getEngine(), parts[0].getIntValue(), parts[1].getIntValue());
            if (d == nullptr || ! d->isEnabled()) continue;
            bool already = false;
            for (auto in : inputsOf (*s.edit, *t)) if (&in->getInputDevice() == d) already = true;
            if (! already) assignDevice (*s.edit, *t, *d);
        }
    }

    /** Switches one input path between mono and stereo (I/O Setup > Format), moving its tracks along. */
    inline void setInputPathFormat (Session& s, int first, int newCount, std::function<void()> done = {})
    {
        if (s.edit == nullptr) return;
        auto d = waveInContaining (s.getEngine(), first);
        if (d == nullptr) return;
        auto r = rangeOf (d->getChannels());
        int start = newCount == 2 ? (r.first / 2) * 2 : r.first;
        std::vector<te::EditItemID> users;
        juce::Array<te::WaveInputDevice*> touched { d };
        if (newCount == 2) if (auto partner = waveInContaining (s.getEngine(), start + 1)) touched.addIfNotAlreadyThere (partner);
        for (auto td : touched)
            if (auto inst = instanceFor (*s.edit, *td)) for (auto id : inst->getTargets()) users.push_back (id);
        if (newCount == 2) { if (auto lead = waveInContaining (s.getEngine(), start)) s.getEngine().getDeviceManager().setDeviceNumChannels (*lead, 2); }
        else s.getEngine().getDeviceManager().setDeviceNumChannels (*d, 1);
        whenInputReady (s, start, newCount, [&s, users, start, newCount, done] (te::WaveInputDevice* nd)
        {
            if (nd != nullptr)
                for (auto id : users)
                    if (auto t = dynamic_cast<te::AudioTrack*> (te::findTrackForID (*s.edit, id)))
                    {
                        assignDevice (*s.edit, *t, *nd);
                        t->state.setProperty ("sqmIn", juce::String (start) + "," + juce::String (newCount), nullptr);
                    }
            applyNames (s.getEngine());
            if (done) done();
        });
    }

    /** Sets every interface input (or output) to mono paths or stereo pairs, moving tracks along. */
    inline void relayoutAll (Session& s, bool input, int channels)
    {
        if (s.edit == nullptr) return;
        auto& dm = s.getEngine().getDeviceManager();
        if (! input)
        {
            dm.setAllWaveOutputsToNumChannels ((uint32_t) channels);
            juce::Timer::callAfterDelay (400, [&s] { applyNames (s.getEngine()); });
            return;
        }
        std::vector<std::pair<te::EditItemID, int>> firstChannel;
        for (auto t : s.getTracks())
            for (auto in : inputsOf (*s.edit, *t))
                if (auto w = dynamic_cast<te::WaveInputDevice*> (&in->getInputDevice()); w != nullptr && ! w->isTrackDevice())
                    firstChannel.push_back ({ t->itemID, rangeOf (w->getChannels()).first });
        dm.setAllWaveInputsToNumChannels ((uint32_t) channels);
        auto* edit = s.edit.get();
        juce::Timer::callAfterDelay (500, [&s, firstChannel, channels, edit]
        {
            if (s.edit.get() != edit) return;
            for (auto& [id, ch] : firstChannel)
                if (auto t = dynamic_cast<te::AudioTrack*> (te::findTrackForID (*s.edit, id)))
                {
                    int first = channels == 2 ? (ch / 2) * 2 : ch;
                    if (auto d = findWaveIn (s.getEngine(), first, channels))
                    {
                        assignDevice (*s.edit, *t, *d);
                        t->state.setProperty ("sqmIn", juce::String (first) + "," + juce::String (channels), nullptr);
                    }
                }
            applyNames (s.getEngine());
            s.log.add ("Squelchmoth", juce::String ("Interface inputs set to ") + (channels == 2 ? "stereo pairs" : "mono paths"));
        });
    }

    /** The default input for a new track, like Pro Tools: the first interface path no other track uses
        (In 1, In 2 ... for mono tracks, In 1-2, In 3-4 ... for stereo); when every path is taken, tracks share. */
    inline void assignDefaultInput (Session& s, te::AudioTrack& t, int index, bool stereo)
    {
        auto& eng = s.getEngine();
        auto n = numInterfaceChannels (eng, true);
        if (n == 0 || s.edit == nullptr) return;
        auto channelFree = [&] (int ch)
        {
            if (reservedChannels().count (ch) > 0) return false;
            auto d = waveInContaining (eng, ch);
            if (d == nullptr) return false;
            auto inst = instanceFor (*s.edit, *d);
            return inst == nullptr || inst->getTargets().isEmpty();
        };
        if (stereo && n >= 2)
        {
            for (int ch = 0; ch + 1 < n; ch += 2)
                if (channelFree (ch) && channelFree (ch + 1)) { setInterfaceInput (s, t, ch, 2); return; }
            int first = (index % juce::jmax (1, n / 2)) * 2;
            setInterfaceInput (s, t, first, 2);
            return;
        }
        for (int ch = 0; ch < n; ++ch)
            if (channelFree (ch)) { setInterfaceInput (s, t, ch, 1); return; }
        // every channel is in use: share, keeping whatever layout the interface has now
        int ch = index % n;
        if (auto d = waveInContaining (eng, ch)) { auto r = rangeOf (d->getChannels()); setInterfaceInput (s, t, r.first, r.second); }
    }
}
