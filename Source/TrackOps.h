#pragma once
// Squelchmoth - Pro Tools Track commands: New Tracks (every track type, mono / stereo, samples / ticks),
// Master Fader, Make Active / Inactive, Hide / Show, Record Safe, Solo Safe, input monitoring, Freeze,
// Commit, Bounce, Split into Mono, Duplicate with options, Move to New Folder.

#include "IORouting.h"

namespace TrackOps
{
    //==========================================================================
    // Master Fader (the session's main output fader, shown as a track like Pro Tools)
    inline bool hasMasterFader (Session& s)               { return s.edit != nullptr && (bool) s.edit->state.getProperty ("sqmMasterFader", true); }
    inline juce::String masterName (Session& s)           { auto n = s.edit != nullptr ? s.edit->state.getProperty ("sqmMasterName").toString() : juce::String(); return n.isNotEmpty() ? n : juce::String ("Master 1"); }
    inline void setMasterName (Session& s, const juce::String& n) { if (s.edit) s.edit->state.setProperty ("sqmMasterName", n.trim(), &s.edit->getUndoManager()); }

    inline void setMasterFader (Session& s, bool on)
    {
        if (s.edit == nullptr) return;
        s.edit->state.setProperty ("sqmMasterFader", on, &s.edit->getUndoManager());
        if (! on)
        {
            // Pro Tools: deleting the Master Fader removes its inserts and returns the bus to unity
            for (auto p : Session::getInserts (s.edit->getMasterPluginList(), nullptr)) p->deleteFromParent();
            for (auto p : s.edit->getMasterPluginList().getPlugins()) if (dynamic_cast<SqMonoSumPlugin*> (p) != nullptr) p->deleteFromParent();
            s.edit->state.removeProperty ("sqmMasterFormat", nullptr);
            if (auto mv = s.edit->getMasterVolumePlugin()) { mv->setVolumeDb (0.0f); mv->setPan (0.0f); }
        }
    }

    //==========================================================================
    // State flags
    inline bool isInactive (te::Track& t)                 { return ! t.isProcessing (false); }
    inline void setInactive (te::Track& t, bool inactive) { t.setProcessing (! inactive); }

    inline bool isRecordSafe (te::Track& t)               { return (bool) t.state.getProperty ("sqmRecSafe", false); }
    inline void setRecordSafe (Session& s, te::Track& t, bool safe)
    {
        t.state.setProperty ("sqmRecSafe", safe, &t.edit.getUndoManager());
        if (safe) if (auto at = dynamic_cast<te::AudioTrack*> (&t)) if (s.isArmed (*at)) s.setArmed (*at, false);
    }

    /** Polarity (Ø): inverts the track's signal in its channel strip (Volume/Pan stage). Undoable, saved with the session. */
    inline te::VolumeAndPanPlugin* polarityStage (te::Track& t)
    {
        if (t.isMasterTrack()) return t.edit.getMasterVolumePlugin().get();
        if (auto at = dynamic_cast<te::AudioTrack*> (&t)) return at->getVolumePlugin();
        if (auto ft = dynamic_cast<te::FolderTrack*> (&t)) return ft->getVolumePlugin();
        return nullptr;
    }
    inline bool isPolarityInverted (te::Track& t)         { auto v = polarityStage (t); return v != nullptr && v->polarity.get(); }
    inline bool setPolarityInverted (te::Track& t, bool inverted)
    {
        auto v = polarityStage (t);
        if (v == nullptr) return false;
        v->polarity.setValue (inverted, &t.edit.getUndoManager());
        return true;
    }

    inline bool isSoloSafe (te::Track& t)                 { return t.isSoloIsolate (false); }
    inline void setSoloSafe (te::Track& t, bool safe)     { t.setSoloIsolate (safe); }

    inline juce::String comments (te::Track& t)           { return t.state.getProperty ("sqmComments").toString(); }
    inline void setComments (te::Track& t, const juce::String& c) { t.state.setProperty ("sqmComments", c, &t.edit.getUndoManager()); }

    /** Pro Tools Track > Input Only Monitoring / Auto Input Monitoring (applies to every interface input). */
    inline bool inputOnlyMonitoring()                     { return Prefs::getBool ("tr_inputOnly", false); }
    inline void setInputOnlyMonitoring (Session& s, bool inputOnly)
    {
        Prefs::set ("tr_inputOnly", inputOnly);
        if (s.edit == nullptr) return;
        for (auto in : s.edit->getAllInputDevices())
        {
            auto& dev = in->getInputDevice();
            if (dev.getDeviceType() == te::InputDevice::waveDevice)
                dev.setMonitorMode (inputOnly ? te::InputDevice::MonitorMode::on : te::InputDevice::MonitorMode::automatic);
            else if (dev.getMonitorMode() == te::InputDevice::MonitorMode::on)
                dev.setMonitorMode (te::InputDevice::MonitorMode::automatic);   // older builds switched whole devices on
        }
    }

    /** MIDI Thru (Setup > MIDI Thru): the keyboard plays the first selected MIDI / instrument track, as in Pro Tools.
        Returns true if the target changed. Playback is rebuilt only while stopped (no glitches mid-song). */
    inline bool updateMidiThruTarget (Session& s, te::AudioTrack* wanted)
    {
        if (s.edit == nullptr) return false;
        if (! Prefs::midiThru()) wanted = nullptr;
        bool changed = false;
        for (auto t : s.getTracks())
        {
            const bool on = t == wanted;
            if ((bool) t->state.getProperty ("sqmMidiThru", false) != on) { t->state.setProperty ("sqmMidiThru", on, nullptr); changed = true; }
        }
        if (changed) s.edit->restartPlayback();
        return changed;
    }

    /** Pro Tools' per-track Input Monitor ("I") button: only this track hears its live input. */
    inline bool isInputMonitorOn (te::Track& t)           { return (bool) t.state.getProperty ("sqmInputMonitor", false); }
    inline void setInputMonitor (Session& s, te::Track& t, bool on)
    {
        t.state.setProperty ("sqmInputMonitor", on, nullptr);
        if (s.edit != nullptr) s.edit->restartPlayback();
    }

    //==========================================================================
    // Timebase (Samples = clips stay at absolute time, Ticks = clips follow tempo changes)
    inline bool isTicks (te::Track& t)                    { return t.state.getProperty ("sqmTimebase").toString() == "ticks"; }
    inline void applyTimebase (te::Clip& c, bool ticks)   { c.setSyncType (ticks ? te::Clip::syncBarsBeats : te::Clip::syncAbsolute); }
    inline void setTimebase (te::AudioTrack& t, bool ticks)
    {
        t.state.setProperty ("sqmTimebase", ticks ? "ticks" : "samples", &t.edit.getUndoManager());
        for (auto c : t.getClips()) applyTimebase (*c, ticks);
    }

    //==========================================================================
    // New Tracks
    enum class NewType { audio, aux, masterFader, vca, midi, instrument, basicFolder, routingFolder };
    inline juce::StringArray newTypeNames()   { return { "Audio Track", "Aux Input", "Master Fader", "VCA Master", "MIDI Track", "Instrument Track", "Basic Folder", "Routing Folder" }; }

    struct NewSpec
    {
        int count = 1;
        bool stereo = false;
        NewType type = NewType::audio;
        bool ticks = false;
        juce::String name;
        juce::String instrument = "4osc";   // Instrument Track: built-in type, "" = empty
        juce::File preset;                   // Track Presets: create from a saved .sqmtrack instead of a type
    };

    inline juce::File trackPresetsDir (Session& s)       { auto d = s.sessionsDir.getChildFile ("Track Presets"); d.createDirectory(); return d; }
    inline juce::Array<juce::File> trackPresetFiles (Session& s)
    {
        auto files = trackPresetsDir (s).findChildFiles (juce::File::findFiles, false, "*.sqmtrack");
        files.sort();
        return files;
    }

    /** Inserts a track built from a saved track preset (inserts, sends, I/O, colour, format; no clips). */
    inline te::Track* createFromPreset (Session& s, const juce::File& f, const juce::String& name, te::Track* after)
    {
        if (s.edit == nullptr) return nullptr;
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr) return nullptr;
        auto st = juce::ValueTree::fromXml (*xml);
        if (! st.isValid()) return nullptr;
        te::EditItemID::remapIDs (st, nullptr, *s.edit);
        if (after == nullptr) { auto tops = te::getTopLevelTracks (*s.edit); for (auto t : tops) if (! t->isMasterTrack() && ! t->isMarkerTrack() && ! t->isTempoTrack() && ! t->isChordTrack() && ! t->isArrangerTrack()) after = t; }
        auto nt = s.edit->insertTrack (te::TrackInsertPoint (after != nullptr ? after->getParentFolderTrack() : nullptr, after), st, nullptr);
        if (nt != nullptr) nt->setName (name.isNotEmpty() ? name : f.getFileNameWithoutExtension());
        return nt.get();
    }

    inline juce::String defaultBaseName (NewType t)
    {
        switch (t)
        {
            case NewType::aux: return "Aux";              case NewType::masterFader: return "Master";
            case NewType::vca: return "VCA";              case NewType::midi: return "MIDI";
            case NewType::instrument: return "Inst";      case NewType::basicFolder: return "Folder";
            case NewType::routingFolder: return "Routing Folder";
            default: return "Audio";
        }
    }

    /** Pro Tools track format. A mono track is truly mono: a hidden summing stage sits in front of its inserts
        (after the instrument / Aux return that feeds it), so stereo sources become one channel. */
    inline te::PluginList* formatListFor (Session& s, te::Track& t)
    {
        if (t.isMasterTrack()) return s.edit != nullptr ? &s.edit->getMasterPluginList() : nullptr;
        return &t.pluginList;
    }

    inline bool isMonoFormat (Session& s, te::Track& t)
    {
        if (auto list = formatListFor (s, t))
            for (auto p : list->getPlugins()) if (dynamic_cast<SqMonoSumPlugin*> (p) != nullptr) return true;
        return false;
    }

    inline void setTrackFormat (Session& s, te::Track& t, bool stereo)
    {
        auto list = formatListFor (s, t);
        if (list == nullptr || s.edit == nullptr) return;
        if (! t.isMasterTrack()) t.state.setProperty ("sqmFormat", stereo ? "stereo" : "mono", nullptr);
        else s.edit->state.setProperty ("sqmMasterFormat", stereo ? "stereo" : "mono", nullptr);
        bool had = false;
        for (auto p : list->getPlugins())
            if (dynamic_cast<SqMonoSumPlugin*> (p) != nullptr) { had = true; if (stereo) p->deleteFromParent(); break; }
        if (! stereo && ! had)
        {
            int idx = 0;   // after the sources (instrument, Aux return), before every insert
            auto plugins = list->getPlugins();
            while (idx < plugins.size() && (plugins[idx]->isSynth() || dynamic_cast<te::AuxReturnPlugin*> (plugins[idx].get()) != nullptr)) ++idx;
            if (auto p = s.edit->getPluginCache().createNewPlugin (SqMonoSumPlugin::xmlTypeName, {}))
                list->insertPlugin (p, idx, nullptr);
        }
        s.applyPanLaws();
    }

    /** Creates the tracks; returns the new rows (and a note when something could not be created). */
    inline juce::Array<te::Track*> createTracks (Session& s, const NewSpec& spec, te::Track* insertAfter, juce::String& note)
    {
        juce::Array<te::Track*> made;
        if (s.edit == nullptr) return made;
        auto base = spec.name.trim().isNotEmpty() ? spec.name.trim() : defaultBaseName (spec.type);

        auto nameFor = [&] (int i)
        {
            if (spec.count == 1 && spec.name.trim().isNotEmpty()) return base;
            // Pro Tools numbers new tracks Audio 1, Audio 2 ... continuing after existing ones
            int n = i + 1;
            juce::StringArray existing;
            for (auto t : te::getAllTracks (*s.edit)) existing.add (t->getName());
            while (existing.contains (base + " " + juce::String (n))) ++n;
            return base + " " + juce::String (n);
        };

        if (spec.preset.existsAsFile())
        {
            auto pbase = spec.name.trim().isNotEmpty() ? spec.name.trim() : spec.preset.getFileNameWithoutExtension();
            te::Track* prev = insertAfter;
            for (int i = 0; i < juce::jlimit (1, 128, spec.count); ++i)
            {
                juce::String nm = pbase;
                if (spec.count > 1)
                {
                    int n = i + 1;
                    juce::StringArray existing;
                    for (auto t : te::getAllTracks (*s.edit)) existing.add (t->getName());
                    while (existing.contains (pbase + " " + juce::String (n))) ++n;
                    nm = pbase + " " + juce::String (n);
                }
                if (auto t = createFromPreset (s, spec.preset, nm, prev)) { made.add (t); prev = t; }
            }
            if (made.isEmpty()) note = "Could not read the track preset " + spec.preset.getFileNameWithoutExtension();
            return made;
        }

        if (spec.type == NewType::masterFader)
        {
            if (hasMasterFader (s)) note = "This session already has its Master Fader (the main output).";
            else
            {
                setMasterFader (s, true);
                if (spec.name.trim().isNotEmpty()) setMasterName (s, spec.name);
                if (auto m = s.edit->getMasterTrack()) setTrackFormat (s, *m, spec.stereo);
            }
            if (auto m = s.edit->getMasterTrack()) made.add (m);
            return made;
        }

        for (int i = 0; i < juce::jlimit (1, 128, spec.count); ++i)
        {
            auto nm = nameFor (i);
            te::Track* created = nullptr;
            switch (spec.type)
            {
                case NewType::basicFolder:
                case NewType::routingFolder:
                case NewType::vca:
                {
                    auto f = s.addFolder (nm, spec.type == NewType::routingFolder);
                    if (f != nullptr && spec.type == NewType::basicFolder)
                        if (auto vca = f->getVCAPlugin()) vca->deleteFromParent();
                    if (f != nullptr) f->state.setProperty ("sqmKind", spec.type == NewType::basicFolder ? "basicFolder" : spec.type == NewType::vca ? "vca" : "routingFolder", nullptr);
                    if (f != nullptr && spec.type == NewType::routingFolder) setTrackFormat (s, *f, spec.stereo);
                    created = f;
                    break;
                }
                case NewType::aux:
                {
                    auto t = s.addTrack ("bus", nm);
                    if (t) { t->state.setProperty ("sqmKind", "aux", nullptr); setTrackFormat (s, *t, spec.stereo); if (spec.stereo) s.setStereoPanEnabled (*t, true); }
                    created = t;
                    break;
                }
                case NewType::instrument:
                {
                    auto t = s.addTrack ("instrument", nm, spec.instrument.isNotEmpty() ? spec.instrument : juce::String ("4osc"));
                    if (t != nullptr && spec.instrument.isEmpty())
                        for (auto p : t->pluginList.getPlugins()) if (p->isSynth()) { p->deleteFromParent(); break; }
                    if (t) { t->state.setProperty ("sqmKind", "instrument", nullptr); setTrackFormat (s, *t, spec.stereo); if (spec.stereo) s.setStereoPanEnabled (*t, true); }
                    created = t;
                    break;
                }
                case NewType::midi:
                {
                    s.nextTrackNoAudioInput = true;
                    auto t = s.addTrack ("audio", nm);
                    s.nextTrackNoAudioInput = false;
                    if (t != nullptr)
                    {
                        t->state.setProperty ("sqmKind", "midi", nullptr);
                        t->state.removeProperty ("sqmFormat", nullptr);
                        s.assignMidiInput (*t);
                        // a MIDI track plays an Instrument track (or a MIDI device) - pick the newest instrument
                        te::AudioTrack* inst = nullptr;
                        for (auto other : s.getTracks())
                            if (other != t && IO::kindOf (*other) == IO::Kind::instrument) inst = other;
                        if (inst != nullptr) t->getOutput().setOutputToTrack (inst);
                    }
                    created = t;
                    break;
                }
                case NewType::audio:
                default:
                {
                    s.nextAudioTrackStereo = spec.stereo;   // addTrack picks the first free mono or stereo input path
                    auto t = s.addTrack ("audio", nm);
                    s.nextAudioTrackStereo = false;
                    if (t != nullptr) { setTrackFormat (s, *t, spec.stereo); t->state.setProperty ("sqmFormatChosen", true, nullptr); if (spec.stereo) s.setStereoPanEnabled (*t, true); }
                    created = t;
                    break;
                }
            }
            if (created == nullptr) continue;
            if (auto at = dynamic_cast<te::AudioTrack*> (created)) if (spec.ticks) setTimebase (*at, true);
            if (spec.ticks) created->state.setProperty ("sqmTimebase", "ticks", nullptr);
            made.add (created);
        }

        // Pro Tools inserts new tracks after the last selected track
        if (insertAfter != nullptr && ! made.isEmpty())
        {
            te::Track* prev = insertAfter;
            for (auto t : made)
            {
                if (t->isMasterTrack()) continue;
                s.edit->moveTrack (t, te::TrackInsertPoint (insertAfter->getParentFolderTrack(), prev));
                prev = t;
            }
        }
        return made;
    }

    //==========================================================================
    // Duplicate (Track > Duplicate... options)
    struct DupOptions { int copies = 1; bool clips = true, automation = true, inserts = true, sends = true, groups = true; };

    inline juce::Array<te::Track*> duplicate (Session& s, te::Track& t, const DupOptions& o)
    {
        juce::Array<te::Track*> out;
        if (s.edit == nullptr || t.isMasterTrack()) return out;
        te::Track* after = &t;
        for (int i = 0; i < juce::jlimit (1, 32, o.copies); ++i)
        {
            auto st = t.state.createCopy();
            for (int c = st.getNumChildren(); --c >= 0;)
            {
                auto child = st.getChild (c);
                bool isClip = child.hasType (te::IDs::AUDIOCLIP) || child.hasType (te::IDs::MIDICLIP) || child.hasType (te::IDs::STEPCLIP) || child.hasType (te::IDs::EDITCLIP);
                if (! o.clips && isClip) st.removeChild (c, nullptr);
                if (child.hasType (te::IDs::PLUGIN))
                {
                    auto type = child.getProperty (te::IDs::type).toString();
                    bool isSend = type == te::AuxSendPlugin::xmlTypeName;
                    bool core = type == te::VolumeAndPanPlugin::xmlTypeName || type == te::LevelMeterPlugin::xmlTypeName || type == te::AuxReturnPlugin::xmlTypeName
                                || type == SqMonoSumPlugin::xmlTypeName || type == SqMuteAutoPlugin::xmlTypeName || type == SqStereoPanPlugin::xmlTypeName || type == SqStripTapPlugin::xmlTypeName;
                    if ((! o.sends && isSend) || (! o.inserts && ! isSend && ! core)) st.removeChild (c, nullptr);
                    else if (! o.automation)
                        for (int pc = child.getNumChildren(); --pc >= 0;)
                            if (child.getChild (pc).hasType (te::IDs::AUTOMATIONCURVE)) child.removeChild (pc, nullptr);
                }
                if (child.hasType (te::IDs::AUTOMATIONCURVE) && ! o.automation) st.removeChild (c, nullptr);
            }
            te::EditItemID::remapIDs (st, nullptr, *s.edit);
            st.setProperty (te::IDs::name, t.getName() + ".dup" + juce::String (i + 1), nullptr);
            if (auto nt = s.edit->insertTrack (te::TrackInsertPoint (t.getParentFolderTrack(), after), st, nullptr))
            {
                if (o.groups)
                    for (auto g : s.getGroups())
                        if (std::find (g.tracks.begin(), g.tracks.end(), t.itemID) != g.tracks.end())
                        { g.tracks.push_back (nt->itemID); s.setGroup (g); }
                out.add (nt.get());
                after = nt.get();
            }
        }
        return out;
    }

    //==========================================================================
    // Freeze / Commit / Bounce / Split into Mono
    inline juce::String freeze (te::AudioTrack& t, bool on)
    {
        auto r = t.setFrozen (on, te::Track::individualFreeze);
        return r.failed() ? r.getErrorMessage() : juce::String();
    }

    /** Renders the track through its inserts (pre-fader, no master plug-ins) to a WAV file. */
    inline juce::File renderTrack (Session& s, te::AudioTrack& t, const juce::File& dest, juce::String& err)
    {
        auto& e = *s.edit;
        auto end = te::toPosition (e.getLength());
        if (end <= 0s) { err = "The session is empty"; return {}; }
        auto vp = t.getVolumePlugin();
        float oldVol = vp ? vp->getVolumeDb() : 0.0f, oldPan = vp ? vp->getPan() : 0.0f;
        if (vp) { vp->setVolumeDb (0.0f); vp->setPan (0.0f); }
        bool wasMuted = t.isMuted (false);
        t.setMute (false);
        te::Renderer::Parameters p (e);
        p.destFile = dest;
        p.audioFormat = s.getEngine().getAudioFileFormatManager().getWavFormat();
        p.bitDepth = 24;
        p.sampleRateForAudio = 48000.0;
        if (auto dev = s.getEngine().getDeviceManager().deviceManager.getCurrentAudioDevice()) p.sampleRateForAudio = dev->getCurrentSampleRate();
        p.time = { 0s, end };
        p.useMasterPlugins = false;
        p.usePlugins = true;
        p.canRenderInMono = false;
        juce::Array<te::Track*> ts { &t };
        p.tracksToDo = te::toBitSet (ts);
        auto f = te::Renderer::renderToFile ("Commit", p);
        if (vp) { vp->setVolumeDb (oldVol); vp->setPan (oldPan); }
        t.setMute (wasMuted);
        if (! f.existsAsFile()) err = "Nothing to render on " + t.getName();
        return f;
    }

    inline juce::File audioFilesDir (Session& s)
    {
        auto d = s.getFile().getSiblingFile (s.getFile().getFileNameWithoutExtension() + " Audio Files");
        d.createDirectory();
        return d;
    }

    /** Pro Tools Commit: renders the track to a new audio track; the source is hidden and made inactive. */
    inline te::AudioTrack* commit (Session& s, te::AudioTrack& t, juce::String& err)
    {
        if (s.edit == nullptr) { err = "No session"; return nullptr; }
        auto f = renderTrack (s, t, audioFilesDir (s).getNonexistentChildFile (juce::File::createLegalFileName (t.getName() + ".cm"), ".wav"), err);
        if (! f.existsAsFile()) return nullptr;
        auto st = t.state.createCopy();
        for (int c = st.getNumChildren(); --c >= 0;)
        {
            auto child = st.getChild (c);
            bool keep = child.hasType (te::IDs::PLUGIN) && (child[te::IDs::type].toString() == te::VolumeAndPanPlugin::xmlTypeName
                                                              || child[te::IDs::type].toString() == te::LevelMeterPlugin::xmlTypeName
                                                              || child[te::IDs::type].toString() == te::AuxSendPlugin::xmlTypeName);
            if (! keep && ! child.hasType (te::IDs::OUTPUTDEVICES)) st.removeChild (c, nullptr);
        }
        te::EditItemID::remapIDs (st, nullptr, *s.edit);
        st.setProperty (te::IDs::name, t.getName() + ".cm", nullptr);
        st.setProperty ("sqmKind", "audio", nullptr);
        st.setProperty ("sqmCommittedFrom", t.itemID.toString(), nullptr);   // 1.10: Uncommit finds its source
        auto nt = s.edit->insertTrack (te::TrackInsertPoint (t.getParentFolderTrack(), &t), st, nullptr);
        auto at = dynamic_cast<te::AudioTrack*> (nt.get());
        if (at == nullptr) { err = "Could not create the committed track"; return nullptr; }
        te::AudioFile af (s.getEngine(), f);
        at->insertWaveClip (t.getName() + ".cm", f, { { 0s, te::TimeDuration::fromSeconds (af.getLength()) }, {} }, false);
        t.setHidden (true);
        setInactive (t, true);
        s.log.add ("You", "Committed " + t.getName() + " to " + at->getName() + " (source hidden and made inactive)");
        return at;
    }

    /** 1.10 (FL wish: reversible commit): brings the source of a committed track back (visible, active) and removes
        the rendered ".cm" track unless keepRendered. Accepts either the committed track or its source. */
    inline te::Track* uncommit (Session& s, te::Track& either, bool keepRendered, juce::String& err)
    {
        if (s.edit == nullptr) { err = "No session"; return nullptr; }
        te::Track* rendered = nullptr; te::Track* source = nullptr;
        auto all = te::getAllTracks (*s.edit);
        auto sourceOf = [&] (te::Track& r) -> te::Track*
        {
            auto id = r.state.getProperty ("sqmCommittedFrom").toString();
            for (auto t : all) if (id.isNotEmpty() && t->itemID.toString() == id) return t;
            if (r.getName().endsWith (".cm"))
                for (auto t : all) if (t != &r && t->getName() == r.getName().dropLastCharacters (3) && t->isHidden()) return t;
            return nullptr;
        };
        if (auto src = sourceOf (either)) { rendered = &either; source = src; }
        else
            for (auto t : all)
                if (t != &either && sourceOf (*t) == &either) { rendered = t; source = &either; break; }
        if (source == nullptr) { err = either.getName() + " is not a committed track (and no committed copy of it was found)"; return nullptr; }
        source->setHidden (false);
        setInactive (*source, false);
        juce::String what = "Uncommitted " + source->getName();
        if (rendered != nullptr && ! keepRendered) { s.edit->deleteTrack (rendered); what << " (removed the rendered track)"; }
        s.log.add ("You", what);
        return source;
    }

    inline juce::File bounceTrack (Session& s, te::AudioTrack& t, juce::String& err)
    {
        auto dest = s.bouncesDir.getNonexistentChildFile (juce::File::createLegalFileName (t.getName() + " bounce"), ".wav");
        return renderTrack (s, t, dest, err);
    }

    /** Pro Tools Split into Mono: a stereo track becomes two mono tracks (.L and .R). */
    inline juce::Array<te::AudioTrack*> splitIntoMono (Session& s, te::AudioTrack& t, juce::String& err)
    {
        juce::Array<te::AudioTrack*> out;
        if (s.edit == nullptr) { err = "No session"; return out; }
        auto& afm = s.getEngine().getAudioFileFormatManager();
        auto dir = audioFilesDir (s);
        std::map<juce::String, std::pair<juce::File, juce::File>> cache;
        bool anyStereo = false;
        for (auto c : t.getClips()) if (auto w = dynamic_cast<te::WaveAudioClip*> (c)) if (w->getWaveInfo().numChannels >= 2) anyStereo = true;
        if (! anyStereo) { err = t.getName() + " has no stereo audio to split"; return out; }

        te::AudioTrack* lr[2] = {};
        for (int side = 0; side < 2; ++side)
        {
            auto st = t.state.createCopy();
            for (int c = st.getNumChildren(); --c >= 0;)
                if (st.getChild (c).hasType (te::IDs::AUDIOCLIP) || st.getChild (c).hasType (te::IDs::MIDICLIP) || st.getChild (c).hasType (te::IDs::STEPCLIP)) st.removeChild (c, nullptr);
            te::EditItemID::remapIDs (st, nullptr, *s.edit);
            st.setProperty (te::IDs::name, t.getName() + (side == 0 ? ".L" : ".R"), nullptr);
            st.setProperty ("sqmFormat", "mono", nullptr);
            auto nt = s.edit->insertTrack (te::TrackInsertPoint (t.getParentFolderTrack(), side == 0 ? (te::Track*) &t : (te::Track*) lr[0]), st, nullptr);
            lr[side] = dynamic_cast<te::AudioTrack*> (nt.get());
            if (lr[side] == nullptr) { err = "Could not create the mono tracks"; return out; }
            if (auto sp = Session::getStereoPan (*lr[side])) sp->deleteFromParent();
            if (auto vp = lr[side]->getVolumePlugin()) vp->setPan (side == 0 ? -1.0f : 1.0f);
            out.add (lr[side]);
        }

        for (auto c : t.getClips())
        {
            auto w = dynamic_cast<te::WaveAudioClip*> (c);
            if (w == nullptr) continue;
            auto src = w->getOriginalFile();
            if (! src.existsAsFile()) continue;
            auto key = src.getFullPathName();
            if (! cache.count (key))
            {
                std::unique_ptr<juce::AudioFormatReader> reader (afm.readFormatManager.createReaderFor (src));
                if (reader == nullptr) continue;
                juce::File files[2] = { dir.getNonexistentChildFile (src.getFileNameWithoutExtension() + ".L", ".wav"), dir.getNonexistentChildFile (src.getFileNameWithoutExtension() + ".R", ".wav") };
                for (int side = 0; side < 2; ++side)
                {
                    std::unique_ptr<juce::OutputStream> os (files[side].createOutputStream());
                    if (os == nullptr) continue;
                    auto opts = juce::AudioFormatWriterOptions().withSampleRate (reader->sampleRate).withNumChannels (1).withBitsPerSample (24);
                    auto writer = afm.getWavFormat()->createWriterFor (os, opts);
                    if (writer == nullptr) continue;
                    juce::AudioBuffer<float> buf ((int) reader->numChannels, 65536);
                    for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += buf.getNumSamples())
                    {
                        auto n = (int) juce::jmin ((juce::int64) buf.getNumSamples(), reader->lengthInSamples - pos);
                        reader->read (&buf, 0, n, pos, true, true);
                        const float* ch[1] = { buf.getReadPointer (juce::jmin (side, (int) reader->numChannels - 1)) };
                        writer->writeFromFloatArrays (ch, 1, n);
                    }
                }
                cache[key] = { files[0], files[1] };
            }
            auto& files = cache[key];
            for (int side = 0; side < 2; ++side)
            {
                auto f = side == 0 ? files.first : files.second;
                if (! f.existsAsFile()) continue;
                auto nc = lr[side]->insertWaveClip (w->getName() + (side == 0 ? ".L" : ".R"), f, w->getPosition(), false);
                if (nc != nullptr)
                {
                    nc->setGainDB (w->getGainDB());
                    nc->setFadeIn (w->getFadeIn()); nc->setFadeOut (w->getFadeOut());
                    nc->setMuted (w->isMuted());
                }
            }
        }
        s.log.add ("You", "Split " + t.getName() + " into mono tracks " + lr[0]->getName() + " / " + lr[1]->getName());
        return out;
    }

    //==========================================================================
    /** Track > Duplicate... (Pro Tools options: number, playlist, automation, inserts, sends, groups). */
    inline void duplicateDialog (Session& s, juce::Array<te::Track*> tracks, std::function<void()> done)
    {
        struct Options : public juce::Component
        {
            Options()
            {
                const char* names[] = { "Active playlist (clips)", "Automation", "Inserts", "Sends", "Group assignments" };
                for (int i = 0; i < 5; ++i) { toggles[i].setButtonText (names[i]); toggles[i].setToggleState (true, juce::dontSendNotification); addAndMakeVisible (toggles[i]); }
                setSize (300, 5 * 24);
            }
            void resized() override { for (int i = 0; i < 5; ++i) toggles[i].setBounds (0, i * 24, getWidth(), 22); }
            juce::ToggleButton toggles[5];
        };
        std::vector<te::SafeSelectable<te::Track>> keep;   // weak: the session may change while the dialog is open
        for (auto t : tracks) if (t != nullptr && ! t->isMasterTrack()) keep.emplace_back (t);
        if (keep.empty()) return;
        auto opts = std::make_shared<Options>();
        auto aw = std::make_shared<juce::AlertWindow> ("Duplicate Tracks", "Duplicate " + juce::String ((int) keep.size()) + " track(s). Copy:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", "1", "Number of duplicates");
        aw->addCustomComponent (opts.get());
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([&s, aw, keep, opts, done] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            DupOptions o;
            o.copies = juce::jlimit (1, 32, aw->getTextEditorContents ("n").getIntValue());
            o.clips = opts->toggles[0].getToggleState(); o.automation = opts->toggles[1].getToggleState(); o.inserts = opts->toggles[2].getToggleState();
            o.sends = opts->toggles[3].getToggleState(); o.groups = opts->toggles[4].getToggleState();
            for (auto& t : keep) if (auto* tt = t.get()) duplicate (s, *tt, o);
            s.log.add ("You", "Duplicated " + juce::String ((int) keep.size()) + " track(s) x" + juce::String (o.copies));
            if (done) done();
        }));
    }

    /** Track > Commit... with the confirmation Pro Tools shows. */
    inline void commitDialog (Session& s, juce::Array<te::AudioTrack*> tracks, std::function<void()> done)
    {
        std::vector<te::SafeSelectable<te::Track>> keep;
        for (auto t : tracks) keep.emplace_back (t);
        if (keep.empty()) return;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Commit",
            "Render " + juce::String ((int) keep.size()) + " track(s) through their inserts to new audio tracks? The source tracks are hidden and made inactive (Track > Show All Tracks and Make Active bring them back).",
            "Commit", "Cancel", nullptr, juce::ModalCallbackFunction::create ([&s, keep, done] (int ok)
            {
                if (ok != 1) return;
                juce::StringArray errs;
                for (auto& t : keep) if (auto at = dynamic_cast<te::AudioTrack*> (t.get())) { juce::String err; commit (s, *at, err); if (err.isNotEmpty()) errs.add (err); }
                if (! errs.isEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Commit", errs.joinIntoString ("\n"));
                if (done) done();
            }));
    }

    inline void bounceTracks (Session& s, juce::Array<te::AudioTrack*> tracks)
    {
        juce::StringArray made, errs;
        for (auto at : tracks) { juce::String err; auto f = bounceTrack (s, *at, err); if (f.existsAsFile()) made.add (f.getFileName()); else errs.add (err); }
        Ui::message (errs.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "Bounce",
                     (made.isEmpty() ? juce::String() : "Bounced to the Bounces folder:\n" + made.joinIntoString ("\n")) + (errs.isEmpty() ? juce::String() : "\n" + errs.joinIntoString ("\n")));
        if (! made.isEmpty()) s.bouncesDir.revealToUser();
    }
}
