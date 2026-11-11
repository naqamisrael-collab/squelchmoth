#pragma once
// Squelchmoth - Session: owns the current Edit (the "session" in Pro Tools terms)
// and offers the lookup helpers shared by the UI and the Claude control server.

#include <JuceHeader.h>
#include "BuiltinPlugins.h"
#include "Signature.h"
#include "Edition.h"
#include "Prefs.h"
#include "StereoPan.h"
#include "ExternalFx.h"
#include "Analyzer.h"
#include "CrashGuard.h"
#include "PluginCatalog.h"
#include "TimeFormat.h"

namespace te = tracktion;
using namespace std::literals;

//==============================================================================
/** A rolling log of everything Claude (or the user) did - shown in the Claude panel. */
class ActivityLog : public juce::ChangeBroadcaster
{
public:
    struct Entry { juce::Time time; juce::String source, text; bool ok = true; };

    void add (const juce::String& source, const juce::String& text, bool ok = true)
    {
        {
            const juce::ScopedLock sl (lock);
            entries.push_back ({ juce::Time::getCurrentTime(), source, text, ok });
            if (entries.size() > 400) entries.erase (entries.begin());
        }
        sendChangeMessage();
    }

    std::vector<Entry> getEntries() const   { const juce::ScopedLock sl (lock); return entries; }

private:
    juce::CriticalSection lock;
    std::vector<Entry> entries;
};

//==============================================================================
class Session : public juce::ChangeBroadcaster   // broadcasts when the Edit object is replaced
{
public:
    explicit Session (te::Engine& e) : engine (e)
    {
        sessionsDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("Squelchmoth Sessions");
        sessionsDir.createDirectory();
        bouncesDir = sessionsDir.getChildFile ("Bounces");
        bouncesDir.createDirectory();
    }

    ~Session() override
    {
        if (edit != nullptr)
        {
            edit->getTransport().stop (false, true);
            save();
        }
        edit.reset();
    }

    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    ActivityLog log;
    juce::File sessionsDir, bouncesDir;

    double newSessionSampleRate = 0;    // set by the New Session dialog just before newSession()
    int newSessionBitDepth = 0;         // 16, 24 or 32 (float); 0 = keep the default (32-bit float)

    juce::File getFile() const          { return edit != nullptr ? edit->editFileRetriever() : juce::File(); }

    /** Returns true when saving/bouncing is allowed (licensed, or trial still running). */
    std::function<bool()> canSaveAndBounce;
    bool savingAllowed() const          { return canSaveAndBounce == nullptr || canSaveAndBounce(); }

    //==============================================================================
    void newSession (const juce::String& name = {})
    {
        auto f = sessionsDir.getNonexistentChildFile (name.isNotEmpty() ? juce::File::createLegalFileName (name)
                                                                        : "Untitled Session", ".squelch", false);
        replaceEdit (te::createEmptyEdit (engine, f), f);

        for (auto t : getTracks())   // drop the engine's default empty track
            edit->deleteTrack (t);

        applyNewSessionDefaults();

        // A fresh session gets 4 audio tracks + 2 instrument tracks, like a template.
        for (int i = 0; i < 4; ++i)
            addTrack ("audio", "Audio " + juce::String (i + 1));

        addTrack ("instrument", "Synth 1", "4osc");
        edit->getTransport().setPosition (0s);
        save();
    }

    bool openSession (const juce::File& f)
    {
        if (! f.existsAsFile())
            return false;

        CrashGuard::setPhase ("loading " + f.getFullPathName());
        auto newEdit = te::loadEditFromFile (engine, f);
        if (newEdit == nullptr)
        {
            CrashGuard::setPhase ("running");
            return false;
        }

        replaceEdit (std::move (newEdit), f);
        CrashGuard::setPhase ("running " + f.getFullPathName());
        return true;
    }

    /** Saves atomically: a temporary file is written and checked, then renamed over the session, so a crash or a full
        disk mid-save can never leave a half-written session behind. */
    bool save()
    {
        if (edit == nullptr || ! savingAllowed()) return false;
        auto f = getFile();
        if (f == juce::File()) return false;
        auto tmp = f.getSiblingFile ("." + f.getFileName() + ".saving");
        tmp.deleteFile();
        if (! te::EditFileOperations (*edit).writeToFile (tmp, false)) { tmp.deleteFile(); return false; }
        if (tmp.getSize() < 64) { tmp.deleteFile(); return false; }
        if (auto xml = juce::parseXML (tmp); xml == nullptr || ! xml->hasTagName ("EDIT")) { tmp.deleteFile(); return false; }
        if (! tmp.moveFileTo (f)) { tmp.deleteFile(); return false; }
        edit->resetChangedStatus();
        return true;
    }

    bool saveAs (const juce::File& f)
    {
        if (edit == nullptr || ! savingAllowed()) return false;
        auto nf = f.withFileExtension (".squelch");
        if (! te::EditFileOperations (*edit).writeToFile (nf, false)) return false;
        edit->editFileRetriever = [nf] { return nf; };
        sendChangeMessage();
        return true;
    }

    //==============================================================================
    // Tracks
    juce::Array<te::AudioTrack*> getTracks() const
    {
        return edit != nullptr ? te::getAudioTracks (*edit) : juce::Array<te::AudioTrack*>();
    }

    /** Every row the Edit/Mix windows show, in display order: audio tracks and folder/VCA tracks,
        children listed after their folder (and hidden when the folder is collapsed). */
    juce::Array<te::Track*> getRows (const std::set<te::EditItemID>* collapsed = nullptr) const
    {
        juce::Array<te::Track*> out;
        if (edit == nullptr) return out;
        std::function<void (const juce::Array<te::Track*>&)> walk = [&] (const juce::Array<te::Track*>& tracks)
        {
            for (auto t : tracks)
            {
                if (! (t->isAudioTrack() || t->isFolderTrack())) continue;
                out.add (t);
                if (t->isFolderTrack() && ! (collapsed != nullptr && collapsed->count (t->itemID)))
                    walk (t->getAllSubTracks (false));
            }
        };
        walk (te::getTopLevelTracks (*edit));
        return out;
    }

    static int depthOf (te::Track* t)
    {
        int d = 0;
        for (auto p = t != nullptr ? t->getParentFolderTrack() : nullptr; p != nullptr; p = p->getParentFolderTrack()) ++d;
        return d;
    }

    juce::Array<te::FolderTrack*> getFolderTracks() const
    {
        juce::Array<te::FolderTrack*> out;
        if (edit != nullptr)
            for (auto t : te::getAllTracks (*edit))
                if (auto f = dynamic_cast<te::FolderTrack*> (t)) out.add (f);
        return out;
    }

    /** Creates a Folder (routing/submix folder) or VCA (basic folder with a VCA fader). */
    te::FolderTrack* addFolder (const juce::String& name, bool submix, const juce::Array<te::Track*>& members = {})
    {
        if (edit == nullptr) return nullptr;
        te::Track* preceding = nullptr;
        if (! members.isEmpty())
        {
            auto all = te::getAllTracks (*edit);
            int idx = all.indexOf (members.getFirst());
            if (idx > 0) preceding = all[idx - 1];
            if (preceding != nullptr && preceding->getParentFolderTrack() != nullptr) preceding = preceding->getParentFolderTrack();
        }
        else
        {
            auto tops = te::getTopLevelTracks (*edit);
            for (int i = tops.size(); --i >= 0;) if (tops[i]->isAudioTrack() || tops[i]->isFolderTrack()) { preceding = tops[i]; break; }
        }
        auto f = edit->insertNewFolderTrack (te::TrackInsertPoint (nullptr, preceding), nullptr, submix);
        if (f == nullptr) return nullptr;
        f->setName (name.isNotEmpty() ? name : (submix ? "Folder " : "VCA ") + juce::String (getFolderTracks().size()));
        f->setColour (submix ? juce::Colour (0xff3ad1ff) : juce::Colour (0xffffb23a));
        te::Track* prev = nullptr;
        for (auto m : members)
        {
            if (m == nullptr || m == f.get()) continue;
            edit->moveTrack (m, te::TrackInsertPoint (f.get(), prev));
            prev = m;
        }
        return f.get();
    }

    te::Track* findRow (const juce::var& ref) const
    {
        if (auto t = findTrack (ref)) return t;
        auto s = ref.toString().trim();
        for (auto f : getFolderTracks())
            if (f->getName().equalsIgnoreCase (s) || f->itemID.toString() == s) return f;
        return nullptr;
    }

    //==============================================================================
    // Mix groups (Pro Tools Groups window): linked faders / mutes / solos / record arms.
    struct Group { juce::String id, name; bool active = true, linkVolume = true, linkMute = true, linkSolo = true, linkArm = false, linkColour = false; std::vector<te::EditItemID> tracks; };

    static juce::Identifier groupsID()      { return "SQM_GROUPS"; }

    std::vector<Group> getGroups() const
    {
        std::vector<Group> out;
        if (edit == nullptr) return out;
        auto gs = edit->state.getChildWithName (groupsID());
        for (auto g : gs)
        {
            Group grp;
            grp.id = g["id"].toString(); grp.name = g["name"].toString();
            grp.active = (bool) g.getProperty ("active", true);
            grp.linkVolume = (bool) g.getProperty ("vol", true); grp.linkMute = (bool) g.getProperty ("mute", true);
            grp.linkSolo = (bool) g.getProperty ("solo", true); grp.linkArm = (bool) g.getProperty ("arm", false);
            grp.linkColour = (bool) g.getProperty ("colour", false);
            for (auto& s : juce::StringArray::fromTokens (g["tracks"].toString(), ",", "")) if (s.isNotEmpty()) grp.tracks.push_back (te::EditItemID::fromVar (s));
            out.push_back (grp);
        }
        return out;
    }

    void setGroup (const Group& grp)
    {
        if (edit == nullptr) return;
        auto gs = edit->state.getOrCreateChildWithName (groupsID(), &edit->getUndoManager());
        auto g = gs.getChildWithProperty ("id", grp.id);
        if (! g.isValid()) { g = juce::ValueTree ("GROUP"); g.setProperty ("id", grp.id, nullptr); gs.appendChild (g, &edit->getUndoManager()); }
        g.setProperty ("name", grp.name, &edit->getUndoManager());
        g.setProperty ("active", grp.active, &edit->getUndoManager());
        g.setProperty ("vol", grp.linkVolume, &edit->getUndoManager()); g.setProperty ("mute", grp.linkMute, &edit->getUndoManager());
        g.setProperty ("solo", grp.linkSolo, &edit->getUndoManager()); g.setProperty ("arm", grp.linkArm, &edit->getUndoManager());
        g.setProperty ("colour", grp.linkColour, &edit->getUndoManager());
        juce::StringArray ids; for (auto& t : grp.tracks) ids.add (t.toString());
        g.setProperty ("tracks", ids.joinIntoString (","), &edit->getUndoManager());
    }

    void removeGroup (const juce::String& id)
    {
        if (edit == nullptr) return;
        auto gs = edit->state.getChildWithName (groupsID());
        auto g = gs.getChildWithProperty ("id", id);
        if (g.isValid()) gs.removeChild (g, &edit->getUndoManager());
    }

    juce::String nextGroupId() const
    {
        auto groups = getGroups();
        for (char c = 'a'; c <= 'z'; ++c)
        {
            bool used = false;
            for (auto& g : groups) if (g.id == juce::String::charToString (c)) used = true;
            if (! used) return juce::String::charToString (c);
        }
        return "z" + juce::String (groups.size());
    }

    /** Tracks linked to this one through active groups (not including itself). */
    juce::Array<te::AudioTrack*> groupMates (te::Track& t, int attribute /* 0 vol, 1 mute, 2 solo, 3 arm, 4 colour */) const
    {
        juce::Array<te::AudioTrack*> out;
        if (! groupsEnabled) return out;
        for (auto& g : getGroups())
        {
            if (! g.active) continue;
            bool linked = attribute == 0 ? g.linkVolume : attribute == 1 ? g.linkMute : attribute == 2 ? g.linkSolo : attribute == 3 ? g.linkArm : g.linkColour;
            if (! linked) continue;
            if (std::find (g.tracks.begin(), g.tracks.end(), t.itemID) == g.tracks.end()) continue;
            for (auto& id : g.tracks)
                if (id != t.itemID)
                    if (auto at = dynamic_cast<te::AudioTrack*> (te::findTrackForID (*edit, id)))
                        out.addIfNotAlreadyThere (at);
        }
        return out;
    }

    bool groupsEnabled = true;    // the Groups "Suspend" toggle (inverted)

    /** Fader move with group linking: the same dB offset is applied to every group member. */
    void setTrackVolume (te::AudioTrack& t, float db, bool propagate = true)
    {
        auto vp = t.getVolumePlugin();
        if (vp == nullptr) return;
        auto delta = db - vp->getVolumeDb();
        vp->setVolumeDb (db);
        if (! propagate || std::abs (delta) < 0.0001f) return;
        for (auto m : groupMates (t, 0))
            if (auto mv = m->getVolumePlugin())
                mv->setVolumeDb (juce::jlimit (-100.0f, 6.0f, mv->getVolumeDb() + delta));
    }

    void setTrackMute (te::Track& t, bool mute, bool propagate = true)
    {
        t.setMute (mute);
        if (propagate) for (auto m : groupMates (t, 1)) m->setMute (mute);
    }

    void setTrackSolo (te::Track& t, bool solo, bool propagate = true)
    {
        t.setSolo (solo);
        if (propagate) for (auto m : groupMates (t, 2)) m->setSolo (solo);
    }

    //==============================================================================
    // Automation modes (Pro Tools: Off / Read / Touch / Latch / Touch-Latch / Write / Trim)
    static juce::StringArray automationModeNames()   { return { "Off", "Read", "Touch", "Latch", "Write" }; }

    static int getAutomationModeIndex (te::Track& t)
    {
        if (! t.state.getProperty ("sqmAutomationOn", true)) return 0;
        switch (t.automationMode.get())
        {
            case te::AutomationMode::touch: return 2;
            case te::AutomationMode::latch: return 3;
            case te::AutomationMode::write: return 4;
            default: return 1;
        }
    }

    static void setAutomationModeIndex (te::Track& t, int index)
    {
        auto um = &t.edit.getUndoManager();
        t.state.setProperty ("sqmAutomationOn", index != 0, um);
        if (index == 0) { t.automationMode = te::AutomationMode::read; return; }
        t.automationMode = index == 2 ? te::AutomationMode::touch : index == 3 ? te::AutomationMode::latch : index == 4 ? te::AutomationMode::write : te::AutomationMode::read;
        // Writing modes need the Edit's automation recording switched on
        if (index >= 2) t.edit.getAutomationRecordManager().setWritingAutomation (true);
    }

    //==============================================================================
    // Elastic Audio (per track time-stretch algorithm applied to every audio clip on it)
    static juce::StringArray elasticModeNames()      { return { "None", "Polyphonic", "Rhythmic", "Monophonic", "Varispeed", "Molt (render)" }; }

    static te::TimeStretcher::Mode elasticToStretchMode (int index)
    {
        using M = te::TimeStretcher::Mode;
        switch (index)
        {
            case 1: return te::TimeStretcher::checkModeIsAvailable (M::signalsmithDefault);
            case 2: return te::TimeStretcher::checkModeIsAvailable (M::signalsmithCheaper);   // shorter windows keep drum transients tight
            case 3: return te::TimeStretcher::checkModeIsAvailable (M::signalsmithDefault);
            case 4: return M::disabled;
            case 5: return te::TimeStretcher::checkModeIsAvailable (M::signalsmithDefault);
            default: return M::disabled;
        }
    }

    static int getElasticMode (te::Track& t)      { return (int) t.state.getProperty ("sqmElastic", 0); }

    void setElasticMode (te::AudioTrack& t, int index)
    {
        t.state.setProperty ("sqmElastic", index, &t.edit.getUndoManager());
        for (auto c : t.getClips())
            if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
            {
                ac->setTimeStretchMode (elasticToStretchMode (index));
                ac->setAutoTempo (index != 0 && index != 4);
                if (index == 4 && ac->getAutoTempo()) ac->setAutoTempo (false);
            }
    }

    //==============================================================================
    // Memory locations (markers)
    juce::ReferenceCountedObjectPtr<te::MarkerClip> addMarker (te::TimePosition at, const juce::String& name, te::TimeDuration length = {})
    {
        if (edit == nullptr) return {};
        auto& mm = edit->getMarkerManager();
        if (length < te::TimeDuration::fromSeconds (0.001)) length = te::TimeDuration::fromSeconds (0.001);
        auto m = mm.createMarker (mm.getNextUniqueID (1), at, length, nullptr);
        auto mc = dynamic_cast<te::MarkerClip*> (m.get());
        if (mc != nullptr && name.isNotEmpty()) mc->setName (name);
        return mc;
    }

    juce::ReferenceCountedArray<te::MarkerClip> getMarkers() const
    {
        if (edit == nullptr) return {};
        auto ms = edit->getMarkerManager().getMarkers();
        std::sort (ms.begin(), ms.end(), [] (auto a, auto b) { return a->getPosition().getStart() < b->getPosition().getStart(); });
        return ms;
    }

    /** Finds a track by 1-based number, exact name, case-insensitive name, or item ID. */
    te::AudioTrack* findTrack (const juce::var& ref) const
    {
        auto tracks = getTracks();

        if (ref.isInt() || ref.isInt64() || ref.isDouble())
            return tracks[(int) ref - 1];

        auto s = ref.toString().trim();
        if (s.isEmpty()) return nullptr;

        if (s.containsOnly ("0123456789"))
            return tracks[s.getIntValue() - 1];

        for (auto t : tracks) if (t->getName() == s) return t;
        for (auto t : tracks) if (t->getName().equalsIgnoreCase (s)) return t;
        for (auto t : tracks) if (t->itemID.toString() == s) return t;
        for (auto t : tracks) if (t->getName().containsIgnoreCase (s)) return t;
        return nullptr;
    }

    int indexOf (te::AudioTrack* t) const   { return getTracks().indexOf (t) + 1; }

    juce::String lastError;   // why the last addTrack / createPluginByName refused (Demo limits, locked types)

    te::AudioTrack* addTrack (const juce::String& kind, const juce::String& name,
                              const juce::String& instrumentType = {}, const juce::PluginDescription* instrumentDesc = nullptr)
    {
        if (edit == nullptr) return nullptr;
        lastError.clear();
        // Demo: 16 tracks. (Folder / bus tracks don't count; Squelchmoth has no limit.)
        if (getTracks().size() >= Edition::maxTracks())
        {
            lastError = "The Demo holds " + juce::String (Edition::demoMaxTracks) + " tracks. Squelchmoth has no track limit - " + Edition::storeUrl ("CORE");
            if (Edition::showUnlock()) juce::MessageManager::callAsync ([] { if (Edition::showUnlock()) Edition::showUnlock() ("CORE"); });
            return nullptr;
        }
        if (kind == "instrument" && instrumentDesc == nullptr && instrumentType.isNotEmpty() && ! Edition::isTypeAllowed (instrumentType))
        {
            lastError = Signature::displayNameForType (instrumentType) + " is locked. " + Edition::lockReason (instrumentType) + " - " + Edition::storeUrl (Edition::productForType (instrumentType)->code);
            if (Edition::showUnlock()) juce::MessageManager::callAsync ([code = juce::String (Edition::productForType (instrumentType)->code)] { if (Edition::showUnlock()) Edition::showUnlock() (code); });
            return nullptr;
        }

        auto t = edit->insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr, true);
        if (t == nullptr) return nullptr;

        t->setName (name.isNotEmpty() ? name : ("Track " + juce::String (indexOf (t.get()))));

        if (kind == "instrument")
        {
            te::Plugin::Ptr p;
            if (instrumentDesc != nullptr)
                p = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, *instrumentDesc);
            else
                p = edit->getPluginCache().createNewPlugin (instrumentType.isNotEmpty() ? instrumentType : juce::String ("4osc"), {});

            if (p != nullptr)
                t->pluginList.insertPlugin (p, 0, nullptr);

            t->setColour (pickColour ? pickColour (indexOf (t.get()) - 1, "instrument") : juce::Colour (0xff7a3aff));
            assignMidiInput (*t);
        }
        else if (kind == "bus" || kind == "aux")
        {
            // A bus is an audio track whose first insert is an Aux Return listening on a bus number.
            int busNum = getNextFreeBus();
            if (auto ret = edit->getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
            {
                t->pluginList.insertPlugin (ret, 0, nullptr);
                if (auto ar = dynamic_cast<te::AuxReturnPlugin*> (ret.get()))
                    ar->busNumber = busNum;
            }
            t->setColour (pickColour ? pickColour (indexOf (t.get()) - 1, "bus") : juce::Colour (0xff3affb0));
            t->setName (name.isNotEmpty() ? name : ("Aux " + juce::String (busNum + 1)));
        }
        else
        {
            t->setColour (pickColour ? pickColour (indexOf (t.get()) - 1, "audio") : juce::Colour (0xffd4ff3a));
            bool stereo = std::exchange (nextAudioTrackStereo, false);
            t->state.setProperty ("sqmFormat", stereo ? "stereo" : "mono", nullptr);
            t->state.setProperty ("sqmKind", "audio", nullptr);
            // a Pro Tools mono track really is mono: everything is summed to one channel before the inserts
            if (! stereo)
                if (auto ms = edit->getPluginCache().createNewPlugin (SqMonoSumPlugin::xmlTypeName, {}))
                    t->pluginList.insertPlugin (ms, 0, nullptr);
            int audioIndex = 0;
            for (auto o : getTracks()) if (o != t.get() && o->state.getProperty ("sqmKind").toString() == "audio") ++audioIndex;
            if (std::exchange (nextTrackNoAudioInput, false)) {}
            else if (defaultInputAssigner) defaultInputAssigner (*t, audioIndex, stereo);
            else assignAudioInput (*t, indexOf (t.get()) - 1);
        }

        if (auto vp = t->getVolumePlugin())
            vp->setVolumeDb ((float) Prefs::newTrackFaderDb());
        if (t->state.getProperty ("sqmFormat").toString() == "stereo") setStereoPanEnabled (*t, true);
        applyPanLaws();
        if (onTrackAdded) onTrackAdded (*t);
        return t.get();
    }

    /** Set by the app so new tracks take colours from the user's palette (Preferences > Display). */
    std::function<juce::Colour (int index, const juce::String& kind)> pickColour;
    std::function<void (te::AudioTrack&)> onTrackAdded;
    /** Set by the app: Pro Tools-style default input for a new audio track (first free In n / In n-n). */
    std::function<void (te::AudioTrack&, int index, bool stereo)> defaultInputAssigner;
    /** UI hooks set by the main window: open Track > New... and run a QuickNew menu result (id, id base). */
    std::function<void()> showNewTracksHook;
    std::function<void (int, int)> quickNewHook;
    std::function<void (te::Track*)> showOutputWindowHook;   // the track's Output window (fader, pan, meter, mute/solo)
    bool nextAudioTrackStereo = false;   // the next addTrack ("audio") makes a stereo track
    bool nextTrackNoAudioInput = false;  // the next addTrack ("audio") gets no interface input (MIDI tracks)

    int getNextFreeBus() const
    {
        int maxBus = -1;
        for (auto t : getTracks())
            for (auto p : t->pluginList.getPlugins())
                if (auto ar = dynamic_cast<te::AuxReturnPlugin*> (p))
                    maxBus = juce::jmax (maxBus, ar->busNumber.get());
        return juce::jlimit (0, 31, maxBus + 1);
    }

    te::AudioTrack* findBusTrack (int busNumber) const
    {
        for (auto t : getTracks())
            for (auto p : t->pluginList.getPlugins())
                if (auto ar = dynamic_cast<te::AuxReturnPlugin*> (p))
                    if (ar->busNumber.get() == busNumber)
                        return t;
        return nullptr;
    }

    int getBusNumberOfTrack (te::AudioTrack& t) const
    {
        for (auto p : t.pluginList.getPlugins())
            if (auto ar = dynamic_cast<te::AuxReturnPlugin*> (p))
                return ar->busNumber.get();
        return -1;
    }

    void assignAudioInput (te::AudioTrack& t, int preferredInputIndex)
    {
        if (edit == nullptr) return;
        edit->getTransport().ensureContextAllocated();

        juce::Array<te::InputDeviceInstance*> waves;
        for (auto in : edit->getAllInputDevices())
            if (in->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
                waves.add (in);

        if (waves.isEmpty()) return;
        auto in = waves[juce::jlimit (0, waves.size() - 1, preferredInputIndex)];
        [[maybe_unused]] auto r = in->setTarget (t.itemID, false, &edit->getUndoManager(), 0);
    }

    void assignMidiInput (te::AudioTrack& t)
    {
        if (edit == nullptr) return;
        edit->getTransport().ensureContextAllocated();

        for (auto in : edit->getAllInputDevices())
        {
            auto type = in->getInputDevice().getDeviceType();
            if (type == te::InputDevice::physicalMidiDevice || type == te::InputDevice::virtualMidiDevice)
            {
                [[maybe_unused]] auto r = in->setTarget (t.itemID, false, &edit->getUndoManager(), 0);
                return;
            }
        }
    }

    bool isArmed (te::AudioTrack& t) const
    {
        for (auto in : edit->getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, 0))
                return in->isRecordingEnabled (t.itemID);
        return false;
    }

    bool setArmed (te::AudioTrack& t, bool arm)
    {
        if (arm && ((bool) t.state.getProperty ("sqmRecSafe", false) || ! t.isProcessing (true)))
        {
            log.add ("Squelchmoth", t.getName() + (! t.isProcessing (true) ? " is inactive - make it active to record" : " is Record Safe - turn Record Safe off to arm it"), false);
            return false;
        }
        bool found = false;
        for (auto in : edit->getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, 0))
            {
                in->setRecordingEnabled (t.itemID, arm);
                found = true;
            }

        if (! found && arm)
        {
            if (isInstrumentTrack (t)) assignMidiInput (t);
            else if (defaultInputAssigner) { defaultInputAssigner (t, 0, t.state.getProperty ("sqmFormat").toString() == "stereo"); return true; }
            else                       assignAudioInput (t, indexOf (&t) - 1);

            for (auto in : edit->getAllInputDevices())
                if (te::isOnTargetTrack (*in, t, 0))
                {
                    in->setRecordingEnabled (t.itemID, true);
                    found = true;
                }
        }
        return found;
    }

    juce::String getInputName (te::AudioTrack& t) const
    {
        for (auto in : edit->getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, 0))
                return in->getInputDevice().getName();
        return "None";
    }

    static bool isInstrumentTrack (te::AudioTrack& t)
    {
        for (auto p : t.pluginList.getPlugins())
            if (p->isSynth())
                return true;
        return false;
    }

    //==============================================================================
    // Inserts: user-visible plugins on a track (excluding the built-in fader & meter)
    static juce::Array<te::Plugin*> getInserts (te::PluginList& list, te::AudioTrack* t)
    {
        juce::Array<te::Plugin*> out;
        for (auto p : list.getPlugins())
        {
            if (t != nullptr && (p == t->getVolumePlugin() || p == t->getLevelMeterPlugin()))
                continue;
            if (dynamic_cast<te::AuxSendPlugin*> (p) != nullptr || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr || dynamic_cast<te::VCAPlugin*> (p) != nullptr
                || dynamic_cast<SqStereoPanPlugin*> (p) != nullptr || dynamic_cast<SqMonoSumPlugin*> (p) != nullptr || dynamic_cast<SqMuteAutoPlugin*> (p) != nullptr || dynamic_cast<te::AuxReturnPlugin*> (p) != nullptr || dynamic_cast<SqAnalyzerPlugin*> (p) != nullptr)
                continue;
            if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr && t == nullptr)
                continue;
            out.add (p);
        }
        return out;
    }

    /** Where a new insert goes on a channel strip: after the existing inserts and before the dual panner, any send and the
        fader / meter (Pro Tools signal flow: inserts -> sends -> pan -> fader). -1 = end of the list (Master). */
    static int insertEndIndex (te::PluginList& list, te::Track* row)
    {
        te::Plugin* vol = nullptr; te::Plugin* meter = nullptr;
        if (auto at = dynamic_cast<te::AudioTrack*> (row)) { vol = at->getVolumePlugin(); meter = at->getLevelMeterPlugin(); }
        else if (auto ft = dynamic_cast<te::FolderTrack*> (row)) { vol = ft->getVolumePlugin(); for (auto q : ft->pluginList.getPlugins()) if (dynamic_cast<te::LevelMeterPlugin*> (q) != nullptr) meter = q; }
        auto plugins = list.getPlugins();
        for (int i = 0; i < plugins.size(); ++i)
        {
            auto p = plugins[i].get();
            if (dynamic_cast<SqStereoPanPlugin*> (p) != nullptr || dynamic_cast<SqMuteAutoPlugin*> (p) != nullptr || dynamic_cast<te::AuxSendPlugin*> (p) != nullptr || dynamic_cast<SqStripTapPlugin*> (p) != nullptr || (vol != nullptr && p == vol) || (meter != nullptr && p == meter)
                || p->state.getProperty ("sqmDynRole").toString() == "limit" || (bool) p->state.getProperty ("sqmMonitorOnly", false))   // 1.10: the console's LIMIT slot stays last before the fader
                return i;
        }
        return -1;
    }

    /** Sessions from before 1.7 could have inserts placed after the dual panner / pre-fader sends: put them back in
        Pro Tools order (inserts first), keeping their relative order. Returns how many strips were fixed. */
    int normaliseInsertOrder()
    {
        if (edit == nullptr) return 0;
        int fixed = 0;
        for (auto t : getTracks())
        {
            auto& list = t->pluginList;
            auto inserts = getInserts (list, t);
            bool moved = false;
            for (auto p : inserts)
            {
                const int end = insertEndIndex (list, t);
                const int at = list.indexOf (p);
                if (end >= 0 && at > end)
                {
                    auto parent = p->state.getParent();
                    parent.moveChild (parent.indexOf (p->state), parent.indexOf (list.getPlugins()[end]->state), nullptr);
                    moved = true;
                }
            }
            if (moved) ++fixed;
        }
        if (fixed > 0) log.add ("Squelchmoth", "Put the inserts on " + juce::String (fixed) + " track(s) back before the panner and sends (Pro Tools order)");
        return fixed;
    }

    //==============================================================================
    // Panning (Pro Tools style): session pan depth + optional dual panner on stereo tracks
    static SqStereoPanPlugin* getStereoPan (te::AudioTrack& t)
    {
        for (auto p : t.pluginList.getPlugins())
            if (auto sp = dynamic_cast<SqStereoPanPlugin*> (p)) return sp;
        return nullptr;
    }

    /** Switches a track between a single (balance) pan knob and Pro Tools' two-knob stereo panner. */
    SqStereoPanPlugin* setStereoPanEnabled (te::AudioTrack& t, bool enabled)
    {
        auto existing = getStereoPan (t);
        if (! enabled) { if (existing) existing->deleteFromParent(); return nullptr; }
        if (existing) return existing;
        auto p = edit->getPluginCache().createNewPlugin (SqStereoPanPlugin::xmlTypeName, {});
        if (p == nullptr) return nullptr;
        t.pluginList.insertPlugin (p, t.pluginList.indexOf (t.getVolumePlugin()), nullptr);
        if (auto sp = dynamic_cast<SqStereoPanPlugin*> (p.get()))
        {
            sp->panLawValue = getPanLaw();
            if (auto vp = t.getVolumePlugin()) vp->setPan (0.0f);
            return sp;
        }
        return nullptr;
    }

    //==============================================================================
    // 1.12: the Mute automation lane (a hidden gain stage just before the fader)
    static SqMuteAutoPlugin* getMuteAuto (te::Track& t)
    {
        for (auto p : t.pluginList.getPlugins())
            if (auto m = dynamic_cast<SqMuteAutoPlugin*> (p)) return m;
        return nullptr;
    }

    SqMuteAutoPlugin* ensureMuteAuto (te::Track& t)
    {
        if (auto m = getMuteAuto (t)) return m;
        if (edit == nullptr || t.isMasterTrack()) return nullptr;
        te::Plugin* vol = nullptr;
        if (auto a = dynamic_cast<te::AudioTrack*> (&t)) vol = a->getVolumePlugin();
        else if (auto f = dynamic_cast<te::FolderTrack*> (&t)) vol = f->getVolumePlugin();
        if (vol == nullptr) return nullptr;
        auto p = edit->getPluginCache().createNewPlugin (SqMuteAutoPlugin::xmlTypeName, {});
        if (p == nullptr) return nullptr;
        t.pluginList.insertPlugin (p, t.pluginList.indexOf (vol), nullptr);
        return dynamic_cast<SqMuteAutoPlugin*> (p.get());
    }

    int getPanLaw() const        { return edit != nullptr ? (int) edit->state.getProperty ("sqmPanLaw", Prefs::getInt ("mx_panLaw", (int) te::PanLaw3dBCenter)) : Prefs::getInt ("mx_panLaw", (int) te::PanLaw3dBCenter); }
    static juce::StringArray panLawNames()   { return { "Linear (0 dB)", "-2.5 dB", "-3 dB (Pro Tools default)", "-4.5 dB", "-6 dB" }; }

    //==============================================================================
    // Session Setup: the session's sample rate and record bit depth (Pro Tools stores both in the session)
    int recordBitDepth() const      { return edit != nullptr ? (int) edit->state.getProperty ("sqmBitDepth", 32) : 32; }

    /** Bit depth for new recordings on every interface input (32 = 32-bit float, can't clip). */
    void setRecordBitDepth (int bits)
    {
        bits = bits <= 16 ? 16 : bits <= 24 ? 24 : 32;
        if (edit != nullptr) edit->state.setProperty ("sqmBitDepth", bits, &edit->getUndoManager());
        applyRecordBitDepth();
    }
    void applyRecordBitDepth()
    {
        const int bits = recordBitDepth();
        for (auto w : getEngine().getDeviceManager().getWaveInputDevices())
            if (w != nullptr && w->getAvailableBitDepths().contains (bits) && w->getBitDepth() != bits) w->setBitDepth (bits);
    }

    /** Sets the session sample rate and asks the interface to run at it. Returns false if the interface refused
        (the session rate is still stored, so recordings / bounces default to it and the status bar warns). */
    bool setSessionSampleRate (double sr)
    {
        if (edit != nullptr) edit->state.setProperty ("sqmSampleRate", sr, &edit->getUndoManager());
        return matchInterfaceToSessionRate();
    }
    bool matchInterfaceToSessionRate()
    {
        if (edit == nullptr) return false;
        const double want = (double) edit->state.getProperty ("sqmSampleRate", 0.0);
        if (want <= 0) return true;
        auto& adm = getEngine().getDeviceManager().deviceManager;
        auto setup = adm.getAudioDeviceSetup();
        if (std::abs (setup.sampleRate - want) < 1.0) return true;
        if (auto dev = adm.getCurrentAudioDevice(); dev == nullptr || ! dev->getAvailableSampleRates().contains (want)) return false;
        setup.sampleRate = want;
        adm.setAudioDeviceSetup (setup, true);
        return std::abs (adm.getAudioDeviceSetup().sampleRate - want) < 1.0;
    }

    /** Session pan depth, applied to every track fader, the master and every stereo panner (like Session Setup > Pan Depth). */
    void setPanLaw (int law)
    {
        law = juce::jlimit (0, 4, law);
        Prefs::set ("mx_panLaw", law);
        te::setDefaultPanLaw ((te::PanLaw) law);
        if (edit == nullptr) return;
        edit->state.setProperty ("sqmPanLaw", law, &edit->getUndoManager());
        for (auto t : getTracks())
            if (auto sp = getStereoPan (*t)) sp->panLawValue = law;
        applyPanLaws();
    }

    //==============================================================================
    // Gain staging (Pro Tools): the pan depth only applies where a single channel is panned - a mono track's pan knob
    // and each of a stereo track's two panners. Stereo track faders, instrument / Aux / folder faders and the Master
    // Fader are balance controls that are unity at 0 dB and centre, so a stereo file on a stereo track plays bit-exact.
    static bool isMonoSummed (te::AudioTrack& t)
    {
        for (auto p : t.pluginList.getPlugins()) if (dynamic_cast<SqMonoSumPlugin*> (p) != nullptr) return true;
        return false;
    }

    void applyPanLaws()
    {
        if (edit == nullptr) return;
        const auto law = (te::PanLaw) getPanLaw();
        for (auto t : getTracks())
            if (auto vp = t->getVolumePlugin())
            {
                const auto want = isMonoSummed (*t) ? law : te::PanLawLinear;
                if (vp->getPanLaw() != want) vp->setPanLaw (want);
            }
        for (auto f : getFolderTracks()) if (auto vp = f->getVolumePlugin()) if (vp->getPanLaw() != te::PanLawLinear) vp->setPanLaw (te::PanLawLinear);
        if (auto mv = edit->getMasterVolumePlugin()) if (mv->getPanLaw() != te::PanLawLinear) mv->setPanLaw (te::PanLawLinear);
    }

    /** An empty mono audio track that receives a stereo file becomes a stereo track (Pro Tools would refuse the drop). */
    bool prepareTrackForFile (te::AudioTrack& t, int numChannels)
    {
        // only tracks nobody chose a format for (the session's starter tracks, quick "+ TRACK"): a track the user made
        // mono on purpose in New Tracks stays mono and sums the file, like Pro Tools
        if (edit == nullptr || numChannels < 2 || ! isMonoSummed (t) || ! t.getClips().isEmpty() || (bool) t.state.getProperty ("sqmFormatChosen", false)) return false;
        for (auto p : t.pluginList.getPlugins()) if (dynamic_cast<SqMonoSumPlugin*> (p) != nullptr) { p->deleteFromParent(); break; }
        t.state.setProperty ("sqmFormat", "stereo", &edit->getUndoManager());
        setStereoPanEnabled (t, true);
        applyPanLaws();
        log.add ("Squelchmoth", t.getName() + " was an empty mono track - it is now stereo to play the stereo file");
        return true;
    }

    /** Every new session: master at 0 dB with a unity (balance) law, 1.7 gain staging, and the session sample rate -
        the interface rate it was made at, or the rate the New Session dialog asked for. */
    void applyNewSessionDefaults()
    {
        if (edit == nullptr) return;
        if (auto mv = edit->getMasterVolumePlugin()) { mv->setVolumeDb (0.0f); mv->setPanLaw (te::PanLawLinear); }
        edit->state.setProperty ("sqmGainStaging", 2, nullptr);
        const double sr = newSessionSampleRate > 0 ? newSessionSampleRate : getEngine().getDeviceManager().getSampleRate();
        edit->state.setProperty ("sqmSampleRate", sr > 0 ? sr : 48000.0, nullptr);
        newSessionSampleRate = 0;
        edit->state.setProperty ("sqmBitDepth", newSessionBitDepth == 16 || newSessionBitDepth == 24 ? newSessionBitDepth : 32, nullptr);
        newSessionBitDepth = 0;
        applyRecordBitDepth();
        if (! matchInterfaceToSessionRate())
            log.add ("Squelchmoth", "The interface can't run at " + juce::String ((sr > 0 ? sr : 48000.0) / 1000.0, 1) + " kHz - playback is resampled. Pick another interface in Setup > Playback Engine.");
        applyPanLaws();
    }

    /** Sessions saved before 1.7: correct the gain staging (they played 3-6 dB quiet) and make "mono" tracks really mono. */
    void migrateGainStaging()
    {
        if (edit == nullptr || (int) edit->state.getProperty ("sqmGainStaging", 0) >= 2) return;
        int clips = 0;
        for (auto t : getTracks()) clips += t->getClips().size();
        if (clips == 0)   // a brand-new / empty session: nothing to correct, so no message
        {
            applyPanLaws();
            edit->state.setProperty ("sqmGainStaging", 2, nullptr);
            return;
        }
        int fixed = 0;
        for (auto t : getTracks())
        {
            if (t->state.getProperty ("sqmFormat").toString() != "mono" || isMonoSummed (*t)) continue;
            if (trackIsStereo (*t)) { t->state.setProperty ("sqmFormat", "stereo", nullptr); setStereoPanEnabled (*t, true); }
            else if (auto ms = edit->getPluginCache().createNewPlugin (SqMonoSumPlugin::xmlTypeName, {})) t->pluginList.insertPlugin (ms, 0, nullptr);
            ++fixed;
        }
        applyPanLaws();
        edit->state.setProperty ("sqmGainStaging", 2, nullptr);
        log.add ("Squelchmoth", "Gain staging updated for 1.7: the Master Fader and stereo faders are now unity at 0 dB (this session played 3-6 dB quiet before)"
                                + juce::String (fixed > 0 ? ", " + juce::String (fixed) + " track format(s) corrected" : juce::String()) + ".");
    }

    /** True when the track plays stereo material (stereo clip or stereo input), so a dual panner makes sense. */
    bool trackIsStereo (te::AudioTrack& t) const
    {
        for (auto c : t.getClips())
            if (auto ac = dynamic_cast<te::WaveAudioClip*> (c))
                if (ac->getWaveInfo().numChannels >= 2) return true;
        if (edit != nullptr)
            for (auto in : edit->getAllInputDevices())
                if (te::isOnTargetTrack (*in, t, 0))
                    if (auto wd = dynamic_cast<te::WaveInputDevice*> (&in->getInputDevice()))
                        if (wd->isStereoPair()) return true;
        return false;
    }

    static juce::Array<te::AuxSendPlugin*> getSends (te::AudioTrack& t)
    {
        juce::Array<te::AuxSendPlugin*> out;
        for (auto p : t.pluginList.getPlugins())
            if (auto s = dynamic_cast<te::AuxSendPlugin*> (p))
                out.add (s);
        return out;
    }

    /** Creates any plugin by name: a built-in type, or a scanned VST3/AU by name / identifier. */
    te::Plugin::Ptr createPluginByName (const juce::String& nameOrId, juce::String& error)
    {
        if (edit == nullptr) { error = "No session open"; return {}; }
        auto s = nameOrId.trim();
        // Editions: a type this copy doesn't own is refused with the product that unlocks it (no silent placeholder)
        auto locked = [&] (const juce::String& type) -> bool
        {
            if (Edition::isTypeAllowed (type)) return false;
            error = Signature::displayNameForType (type) + " is locked. " + Edition::lockReason (type) + ". Buy it at " + Edition::storeUrl (Edition::productForType (type)->code)
                    + " and enter the key in Help > Licences and Upgrades.";
            return true;
        };

        // 0. Squelchmoth Signature names (Prism EQ, Cocoon Compressor, Ember Tape...)
        if (auto e = Signature::findByName (s))
        {
            if (locked (e->type)) return {};
            if (auto p = Signature::create (*edit, *e))
                return p;
        }

        // 1. Built-in exact type or display name
        for (auto& b : builtinPlugins)
            if (s.equalsIgnoreCase (b.type) || s.equalsIgnoreCase (b.name)
                || s.equalsIgnoreCase (juce::String (b.name).fromFirstOccurrenceOf ("AW ", false, false)))
            {
                if (locked (b.type)) return {};
                if (auto p = edit->getPluginCache().createNewPlugin (b.type, {}))
                    return p;
            }

        // 2. Scanned third-party plugins
        if (auto d = findPluginDescription (s))
            if (auto p = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, *d))
                return p;

        // 3. Fuzzy built-in
        for (auto& b : builtinPlugins)
            if (juce::String (b.name).containsIgnoreCase (s))
            {
                if (locked (b.type)) return {};
                if (auto p = edit->getPluginCache().createNewPlugin (b.type, {}))
                    return p;
            }

        error = "No plugin matches '" + s + "'. Call list_plugins to see the Signature bundle, the AirWindows collection and your own VST3/AU plugins (run scan_plugins first if yours are missing).";
        return {};
    }

    std::unique_ptr<juce::PluginDescription> findPluginDescription (const juce::String& s) const
    {
        auto types = engine.getPluginManager().knownPluginList.getTypes();

        // a plug-in name as the menus show it ("H-Reverb", "RCompressor"): the right layout variant in the preferred format
        for (auto& it : PluginCatalog::thirdPartyItems (types, true))
            if (it.name.equalsIgnoreCase (s) || (it.vendor + " " + it.name).equalsIgnoreCase (s))
                return std::make_unique<juce::PluginDescription> (types.getReference (it.typeIndex));

        auto score = [&s] (const juce::PluginDescription& d) -> int
        {
            if (d.createIdentifierString() == s)               return 100;
            if (d.name.equalsIgnoreCase (s))                    return d.pluginFormatName == "VST3" ? 90 : 89;
            if ((d.manufacturerName + " " + d.name).equalsIgnoreCase (s)) return 85;
            if (d.descriptiveName.equalsIgnoreCase (s))         return 80;
            if (d.name.containsIgnoreCase (s))                  return 50;
            if ((d.manufacturerName + " " + d.name).containsIgnoreCase (s)) return 40;
            return 0;
        };

        int best = 0; const juce::PluginDescription* bestD = nullptr;
        for (auto& d : types)
            if (auto sc = score (d); sc > best) { best = sc; bestD = &d; }

        if (bestD != nullptr)
            return std::make_unique<juce::PluginDescription> (*bestD);
        return {};
    }

    //==============================================================================
    // Time helpers
    /** Bar number (1-based, fractional) to time - follows tempo AND meter changes. */
    te::TimePosition barToTime (double bar1Based) const
    {
        auto& ts = edit->tempoSequence;
        const int whole = juce::jmax (0, (int) std::floor (bar1Based - 1.0));
        auto t = ts.toTime (te::tempo::BarsAndBeats { whole, te::BeatDuration(), 0 });
        const double frac = (bar1Based - 1.0) - whole;
        if (frac > 1e-9)
        {
            const int num = juce::jmax (1, ts.getTimeSigAt (t).numerator.get());
            t = ts.toTime (ts.toBeats (t) + te::BeatDuration::fromBeats (frac * num));
        }
        return t;
    }

    double timeToBar (te::TimePosition t) const
    {
        auto bb = edit->tempoSequence.toBarsAndBeats (t);
        const int num = juce::jmax (1, bb.numerator > 0 ? bb.numerator : edit->tempoSequence.getTimeSigAt (t).numerator.get());
        return bb.bars + 1.0 + bb.beats.inBeats() / num;
    }

    juce::String describeTime (te::TimePosition t) const
    {
        return barsBeatsString (t) + "  (" + juce::String (t.inSeconds(), 3) + "s)";
    }

    juce::String barsBeatsString (te::TimePosition t) const    { return TimeFmt::barsBeats (*edit, t); }

    bool saveOnNextReplace = true;   // false for Revert to Saved / Restore: the current state must not be written first

    void replaceEdit (std::unique_ptr<te::Edit> newEdit, const juce::File& f)
    {
        selectionManager.deselectAll();

        if (edit != nullptr)
        {
            edit->getTransport().stop (false, true);
            if (std::exchange (saveOnNextReplace, true)) save();

            // Let every view drop its references to the old Edit's tracks/plugins/clips
            // while the old Edit is still alive, then destroy it.
            auto old = std::move (edit);
            sendSynchronousChangeMessage();
            old.reset();
        }

        edit = std::move (newEdit);
        if (edit == nullptr) return;

        edit->editFileRetriever = [f] { return f; };
        edit->playInStopEnabled = true;
        migrateGainStaging();
        normaliseInsertOrder();
        applyRecordBitDepth();
        if (! matchInterfaceToSessionRate())
            log.add ("Squelchmoth", "This session is " + juce::String ((double) edit->state.getProperty ("sqmSampleRate", 0.0) / 1000.0, 1)
                     + " kHz but the interface can't run at that rate - audio plays resampled. Choose another interface in Setup > Playback Engine.");
        edit->getTransport().ensureContextAllocated();
        if (onEditOpened) onEditOpened (f);
        sendSynchronousChangeMessage();
    }

    std::function<void (const juce::File&)> onEditOpened;

    te::Engine& getEngine() { return engine; }

    //==============================================================================
    /** Browser preview: plays a sound file once through the audio device alongside the engine (Alt-click in the Beat browser). */
    void previewFile (const juce::File& f)
    {
        stopPreview();
        if (! f.existsAsFile()) return;
        if (previewFormats.getNumKnownFormats() == 0) previewFormats.registerBasicFormats();
        auto* reader = previewFormats.createReaderFor (f);
        if (reader == nullptr) return;
        previewReader = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        previewTransport.setSource (previewReader.get(), 0, nullptr, reader->sampleRate);
        previewPlayer.setSource (&previewTransport);
        engine.getDeviceManager().deviceManager.addAudioCallback (&previewPlayer);
        previewCallbackAdded = true;
        previewTransport.setGain (0.8f);
        previewTransport.setPosition (0.0);
        previewTransport.start();
    }
    void stopPreview()
    {
        previewTransport.stop();
        if (previewCallbackAdded) { engine.getDeviceManager().deviceManager.removeAudioCallback (&previewPlayer); previewCallbackAdded = false; }
        previewPlayer.setSource (nullptr);
        previewTransport.setSource (nullptr);
        previewReader.reset();
    }
    juce::AudioFormatManager previewFormats;
    std::unique_ptr<juce::AudioFormatReaderSource> previewReader;
    juce::AudioTransportSource previewTransport;
    juce::AudioSourcePlayer previewPlayer;
    bool previewCallbackAdded = false;
};
