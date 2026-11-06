#pragma once
// Squelchmoth - the tool surface Claude uses to drive the whole DAW.
// Every handler runs on the JUCE message thread (the McpServer marshals calls there).

#include "Session.h"
#include "PluginScanner.h"
#include "DrumKit.h"
#include "EditOps.h"
#include "TransportController.h"
#include "LaneCanvas.h"
#include "Mastering.h"
#include "TrackOps.h"
#include "Performance.h"
#include "TrackMixControls.h"
#include "EQChart.h"
#include "BeatLab.h"
#include "TuneGraph.h"
#include "Molt.h"
#include "MidiTools.h"
#include "AutoSave.h"
#include "MixSnapshots.h"
#include "InsertChains.h"
#include "BounceExport.h"
#include "Rack.h"
#include "KeyLabels.h"
#include "AudioEditor.h"
#include "BurnToMidi.h"

//==============================================================================
struct ToolResult
{
    juce::String text;
    bool isError = false;
    juce::String imageBase64, imageMime;   // optional image content

    static ToolResult ok (const juce::String& t)            { return { t, false, {}, {} }; }
    static ToolResult ok (const char* t)                    { return { juce::String (t), false, {}, {} }; }
    static ToolResult ok (const juce::var& json)            { return { juce::JSON::toString (json, false), false, {}, {} }; }
    static ToolResult error (const juce::String& t)         { return { t, true, {}, {} }; }
};

/** Hooks into the GUI that some tools need (screenshots, switching windows). */
struct UiHooks
{
    std::function<juce::Image (const juce::String& view)> snapshot;
    std::function<void (const juce::String& view)> showView;
    std::function<void (double barsVisible, double firstBar)> zoomTimeline;
    std::function<void (int mode)> setEditMode;
    std::function<void (int tool)> setTool;
    std::function<void (const juce::String& window, bool show)> showWindow;
    std::function<void()> refresh;
    TimelineState* state = nullptr;
    TransportController* transport = nullptr;
    std::function<MasteringProject*()> mastering;
    std::function<juce::var()> menuTree;                                  // every menu command: path, enabled, checked
    std::function<juce::String (const juce::String& path)> menuCommand;   // run one by path; returns error text or {}
    std::function<juce::StringArray()> listWindows;                       // floating windows currently open
    std::function<juce::var()> perfStats;                                 // overload monitor counters (System Usage)
    std::function<void()> perfReset;
    std::function<void (te::Clip&)> openClipEditor;                        // Beat Maker for step clips, piano roll for MIDI clips
    std::function<void (te::Plugin&)> openTuneGraph;                        // Moth Tune Graph window
    std::function<void* (const juce::File&)> openCutLab;                    // the audio editor window (returns the AudioEditor::Window*)
    std::function<void (juce::Array<te::Track*>)> fitHeights;
    std::function<void()> onRackChanged;                                    // Channel Rack / Beat page refresh               // Track Height > Fit to Window
};

//==============================================================================
class McpTools
{
public:
    McpTools (Session& s, PluginScanner& ps) : session (s), scanner (ps)    { registerAll(); }

    UiHooks ui;

    struct Tool
    {
        juce::String name, description;
        juce::var schema;
        std::function<ToolResult (const juce::var&)> handler;
    };

    const std::vector<Tool>& getTools() const   { return tools; }

    ToolResult call (const juce::String& name, const juce::var& args)
    {
        for (auto& t : tools)
            if (t.name == name)
            {
                if (session.edit == nullptr && name != "new_session" && name != "open_session" && name != "list_sessions")
                    return ToolResult::error ("No session is open. Call new_session or open_session first.");
                // Editions (1.9): the Demo has the basics; these whole features are part of Squelchmoth
                if (Edition::isDemo())
                {
                    std::optional<Edition::Feature> f;
                    if (name.startsWith ("mastering_")) f = Edition::Feature::mastering;
                    else if (name == "export_stems") f = Edition::Feature::stems;
                    else if (name == "molt") f = Edition::Feature::molt;
                    else if (name == "cut_lab") f = Edition::Feature::cutLab;
                    if (f) return ToolResult::error (Edition::featureLockMessage (*f) + " (Help > Licences and Upgrades). Tell the user this is locked in the free Demo.");
                }

                try
                {
                    auto r = t.handler (args.isObject() ? args : juce::var (new juce::DynamicObject()));
                    session.log.add ("Claude", name + "  " + summariseArgs (args) + (r.isError ? "  -> ERROR: " + r.text.substring (0, 160) : juce::String()), ! r.isError);
                    return r;
                }
                catch (std::exception& e)
                {
                    session.log.add ("Claude", name + " -> exception: " + e.what(), false);
                    return ToolResult::error (juce::String ("Internal error: ") + e.what());
                }
            }

        return ToolResult::error ("Unknown tool: " + name);
    }

private:
    Session& session;
    PluginScanner& scanner;
    std::vector<Tool> tools;

    te::Edit& edit()            { return *session.edit; }
    juce::UndoManager* um()     { return &session.edit->getUndoManager(); }

    //==============================================================================
    static juce::var obj()      { return juce::var (new juce::DynamicObject()); }
    static void set (juce::var& o, const juce::Identifier& k, const juce::var& v)  { o.getDynamicObject()->setProperty (k, v); }
    static bool has (const juce::var& a, const char* k)   { return a.isObject() && a.getDynamicObject()->hasProperty (k) && ! a[k].isVoid(); }
    static juce::String str (const juce::var& a, const char* k, const juce::String& d = {})  { return has (a, k) ? a[k].toString() : d; }
    static double num (const juce::var& a, const char* k, double d = 0)  { return has (a, k) ? (double) a[k] : d; }
    static bool flag (const juce::var& a, const char* k, bool d = false)
    {
        if (! has (a, k)) return d;
        auto v = a[k];
        if (v.isString()) return v.toString().equalsIgnoreCase ("true") || v.toString() == "1" || v.toString().equalsIgnoreCase ("yes");
        return (bool) v;
    }

    static juce::String summariseArgs (const juce::var& a)
    {
        auto s = juce::JSON::toString (a, true);
        return s.length() > 140 ? s.substring (0, 140) + "..." : s;
    }

    void add (const juce::String& name, const juce::String& desc, const juce::String& schemaJson,
              std::function<ToolResult (const juce::var&)> fn)
    {
        auto schema = juce::JSON::parse (schemaJson);
        jassert (schema.isObject());   // malformed schema literal
        if (! schema.isObject())
            schema = juce::JSON::parse (R"({"type":"object","properties":{}})");
        tools.push_back ({ name, desc, schema, std::move (fn) });
    }

    //==============================================================================
    // Shared argument parsing
    std::optional<te::TimePosition> timeArg (const juce::var& a, const juce::String& prefix)
    {
        auto barKey = prefix + "_bar", secKey = prefix + "_seconds";
        if (has (a, barKey.toRawUTF8()))  return session.barToTime ((double) a[barKey.toRawUTF8()]);
        if (has (a, secKey.toRawUTF8()))  return te::TimePosition::fromSeconds ((double) a[secKey.toRawUTF8()]);
        return {};
    }

    std::optional<te::TimeDuration> durationArg (const juce::var& a, const juce::String& prefix, te::TimePosition from)
    {
        auto barKey = prefix + "_bars", secKey = prefix + "_seconds", beatKey = prefix + "_beats";
        if (has (a, barKey.toRawUTF8()))
        {
            auto startBar = session.timeToBar (from);
            return session.barToTime (startBar + (double) a[barKey.toRawUTF8()]) - from;
        }
        if (has (a, beatKey.toRawUTF8()))
        {
            auto& ts = edit().tempoSequence;
            auto b = ts.toBeats (from) + te::BeatDuration::fromBeats ((double) a[beatKey.toRawUTF8()]);
            return ts.toTime (b) - from;
        }
        if (has (a, secKey.toRawUTF8()))  return te::TimeDuration::fromSeconds ((double) a[secKey.toRawUTF8()]);
        return {};
    }

    te::AudioTrack* trackArg (const juce::var& a, juce::String& err, const char* key = "track")
    {
        if (! has (a, key)) { err = juce::String ("Missing '") + key + "' (track number starting at 1, or track name)."; return nullptr; }
        auto t = session.findTrack (a[key]);
        if (t == nullptr) err = "No track matches '" + a[key].toString() + "'. Call get_session to see the track list.";
        return t;
    }

    bool isMasterRef (const juce::var& a, const char* key = "track")
    {
        return has (a, key) && a[key].toString().trim().equalsIgnoreCase ("master");
    }

    te::PluginList* pluginListArg (const juce::var& a, juce::String& err, te::AudioTrack*& trackOut)
    {
        trackOut = nullptr;
        if (isMasterRef (a)) return &edit().getMasterPluginList();
        trackOut = trackArg (a, err);
        return trackOut != nullptr ? &trackOut->pluginList : nullptr;
    }

    te::Plugin* insertArg (const juce::var& a, juce::String& err)
    {
        te::AudioTrack* t = nullptr;
        auto list = pluginListArg (a, err, t);
        if (list == nullptr) return nullptr;

        auto inserts = Session::getInserts (*list, t);

        if (has (a, "slot"))
        {
            auto v = a["slot"];
            if (v.isString() && ! v.toString().containsOnly ("0123456789"))
            {
                for (auto p : inserts) if (p->getName().equalsIgnoreCase (v.toString()) || Signature::displayName (*p).equalsIgnoreCase (v.toString())) return p;
                for (auto p : inserts) if (p->getName().containsIgnoreCase (v.toString()) || Signature::displayName (*p).containsIgnoreCase (v.toString())) return p;
            }
            else if (auto p = inserts[(int) v - 1])
                return p;
        }
        else if (has (a, "plugin"))
        {
            for (auto p : inserts) if (p->getName().containsIgnoreCase (str (a, "plugin")) || Signature::displayName (*p).containsIgnoreCase (str (a, "plugin"))) return p;
        }

        err = "No insert matches. Inserts on this track: " + describeInserts (inserts);
        return nullptr;
    }

    te::Clip* clipArg (te::AudioTrack& t, const juce::var& a, juce::String& err)
    {
        auto clips = t.getClips();
        if (clips.isEmpty()) { err = "Track '" + t.getName() + "' has no clips."; return nullptr; }

        if (! has (a, "clip"))
        {
            if (clips.size() == 1) return clips.getFirst();
            err = "Track has several clips; pass 'clip' (number starting at 1, or name). Call list_clips.";
            return nullptr;
        }

        auto v = a["clip"];
        if (v.isString() && ! v.toString().containsOnly ("0123456789"))
        {
            for (auto c : clips) if (c->getName().equalsIgnoreCase (v.toString()) || c->itemID.toString() == v.toString()) return c;
            for (auto c : clips) if (c->getName().containsIgnoreCase (v.toString())) return c;
        }
        else if (auto c = clips[(int) v - 1])
            return c;

        err = "No clip matches '" + v.toString() + "' on track '" + t.getName() + "'.";
        return nullptr;
    }

    static juce::String describeInserts (const juce::Array<te::Plugin*>& inserts)
    {
        if (inserts.isEmpty()) return "(none)";
        juce::StringArray s;
        for (int i = 0; i < inserts.size(); ++i) s.add (juce::String (i + 1) + ": " + Signature::displayName (*inserts[i]));
        return s.joinIntoString (", ");
    }

    static int parsePitch (const juce::var& v)
    {
        if (! v.isString()) return juce::jlimit (0, 127, (int) v);
        auto s = v.toString().trim().toUpperCase();
        if (s.containsOnly ("0123456789")) return juce::jlimit (0, 127, s.getIntValue());

        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int noteIdx = -1, len = 0;
        for (int i = 11; i >= 0; --i)
            if (s.startsWith (names[i])) { noteIdx = i; len = (int) strlen (names[i]); break; }
        if (noteIdx < 0) return 60;

        auto rest = s.substring (len);
        if (rest.startsWith ("B") && len == 1) { noteIdx = (noteIdx + 11) % 12; rest = rest.substring (1); }   // flats: Db, Eb...
        if (rest.startsWith ("B")) rest = rest.substring (1);
        int octave = rest.getIntValue();   // C4 = 60
        return juce::jlimit (0, 127, (octave + 1) * 12 + noteIdx);
    }

    juce::var describeTrack (te::AudioTrack& t)
    {
        auto o = obj();
        set (o, "number", session.indexOf (&t));
        set (o, "name", t.getName());
        auto busNum = session.getBusNumberOfTrack (t);
        auto kind = IO::kindOf (t);
        set (o, "type", kind == IO::Kind::aux ? "aux" : kind == IO::Kind::instrument ? "instrument" : kind == IO::Kind::midi ? "midi" : "audio");
        set (o, "format", IO::formatOf (t));
        if (busNum >= 0) { set (o, "bus_number", busNum + 1); set (o, "bus_name", IO::busName (session, busNum)); }
        if (TrackOps::isInactive (t)) set (o, "inactive", true);
        if (t.isHidden()) set (o, "hidden", true);
        if (TrackOps::isRecordSafe (t)) set (o, "record_safe", true);
        if (TrackOps::isSoloSafe (t)) set (o, "solo_safe", true);
        if (t.isFrozen (te::Track::individualFreeze)) set (o, "frozen", true);
        set (o, "timebase", TrackOps::isTicks (t) ? "ticks" : "samples");
        if (TrackOps::comments (t).isNotEmpty()) set (o, "comments", TrackOps::comments (t));
        set (o, "input_monitor", TrackOps::isInputMonitorOn (t));
        set (o, "track_view", MixCtl::viewLabel (session, t));
        set (o, "pan_display", MixCtl::panSummary (session, t));

        if (auto vp = t.getVolumePlugin())
        {
            set (o, "volume_db", juce::roundToInt (vp->getVolumeDb() * 10) / 10.0);
            set (o, "pan", juce::roundToInt (vp->getPan() * 100) / 100.0);
        }
        set (o, "mute", t.isMuted (false));
        set (o, "solo", t.isSolo (false));
        set (o, "armed", session.isArmed (t));
        set (o, "input", IO::inputLabel (session, t));
        set (o, "output", IO::outputLabel (session, t));

        juce::Array<juce::var> ins;
        for (auto p : Session::getInserts (t.pluginList, &t))
            ins.add (Signature::displayName (*p) + (p->isEnabled() ? "" : " (bypassed)"));
        set (o, "inserts", ins);

        juce::Array<juce::var> sends;
        for (auto s : Session::getSends (t))
        {
            auto so = obj();
            set (so, "bus", s->getBusNumber() + 1);
            auto bt = session.findBusTrack (s->getBusNumber());
            set (so, "bus_track", bt != nullptr ? bt->getName() : juce::String ("(no return track)"));
            set (so, "level_db", juce::roundToInt (s->getGainDb() * 10) / 10.0);
            sends.add (so);
        }
        if (! sends.isEmpty()) set (o, "sends", sends);
        set (o, "clips", t.getClips().size());
        return o;
    }

    juce::var describeClip (te::Clip& c, int index)
    {
        auto o = obj();
        auto pos = c.getPosition();
        set (o, "clip", index);
        set (o, "name", c.getName());
        set (o, "type", dynamic_cast<te::MidiClip*> (&c) ? "midi" : "audio");
        set (o, "start", session.describeTime (pos.getStart()));
        set (o, "start_bar", juce::roundToInt (session.timeToBar (pos.getStart()) * 1000) / 1000.0);
        set (o, "end_bar", juce::roundToInt (session.timeToBar (pos.getEnd()) * 1000) / 1000.0);
        set (o, "length_seconds", juce::roundToInt (pos.getLength().inSeconds() * 1000) / 1000.0);

        if (auto ac = dynamic_cast<te::AudioClipBase*> (&c))
        {
            set (o, "gain_db", ac->getGainDB());
            set (o, "fade_in_seconds", ac->getFadeIn().inSeconds());
            set (o, "fade_out_seconds", ac->getFadeOut().inSeconds());
            if (auto wc = dynamic_cast<te::WaveAudioClip*> (&c))
                set (o, "file", wc->getOriginalFile().getFullPathName());
        }
        if (auto mc = dynamic_cast<te::MidiClip*> (&c))
            set (o, "notes", mc->getSequence().getNumNotes());
        return o;
    }

    juce::var describeParam (te::AutomatableParameter& p, int index)
    {
        auto o = obj();
        set (o, "index", index);
        set (o, "name", p.getParameterName());
        set (o, "value", juce::roundToInt (p.getCurrentNormalisedValue() * 10000) / 10000.0);
        set (o, "display", p.getCurrentValueAsStringWithLabel());
        auto r = p.getValueRange();
        set (o, "range", juce::String (r.getStart()) + " .. " + juce::String (r.getEnd()));
        if (p.hasAutomationPoints()) set (o, "automated", true);
        return o;
    }

    te::AutomatableParameter* findParam (te::Plugin& plugin, const juce::var& ref)
    {
        auto params = plugin.getAutomatableParameters();
        if (ref.isInt() || ref.isDouble() || (ref.isString() && ref.toString().containsOnly ("0123456789") && ref.toString().isNotEmpty()))
            return params[(int) ref];
        auto s = ref.toString();
        for (auto p : params) if (p->getParameterName().equalsIgnoreCase (s) || p->paramID.equalsIgnoreCase (s)) return p;
        for (auto p : params) if (p->getParameterName().containsIgnoreCase (s)) return p;
        return nullptr;
    }

    //==============================================================================
    void registerAll()
    {
        registerSessionTools();
        registerTransportTools();
        registerTrackTools();
        registerClipTools();
        registerMidiTools();
        registerPluginTools();
        registerMixTools();
        registerViewTools();
        registerBeatTools();
        registerBeatLabTools();
        registerVocalTools();
        registerVocalFxTools();
        registerMoltTools();
        registerApexTools();
        registerParityTools();
        register17Tools();
        registerMasteringTools();
        registerTrackTools2();
        registerEngineTools();
        registerRackTools();
        register181Tools();
    }

    #include "McpToolsParity.h"
    #include "McpToolsBeat.h"
    #include "McpToolsVocal.h"
    #include "McpToolsApex.h"
    #include "McpToolsMastering.h"
    #include "McpToolsTracks.h"
    #include "McpTools17.h"
    #include "McpToolsRack.h"
    #include "McpTools181.h"

    //==============================================================================
    void registerSessionTools()
    {
        add ("get_session",
             "Overview of the open session: file, tempo, meter, playhead, transport state, loop, and every track with its volume, pan, mute/solo/arm, inputs/outputs, inserts (plugins), sends and clip count. Call this first to orient yourself.",
             R"({"type":"object","properties":{}})",
             [this] (const juce::var&)
             {
                 auto& e = edit();
                 auto& tr = e.getTransport();
                 auto o = obj();
                 set (o, "session_file", session.getFile().getFullPathName());
                 set (o, "tempo_bpm", e.tempoSequence.getTempo (0)->getBpm());
                 set (o, "time_signature", juce::String (e.tempoSequence.getTimeSig (0)->numerator.get()) + "/" + juce::String (e.tempoSequence.getTimeSig (0)->denominator.get()));
                 set (o, "playhead", session.describeTime (tr.getPosition()));
                 set (o, "state", tr.isRecording() ? "recording" : (tr.isPlaying() ? "playing" : "stopped"));
                 set (o, "loop", tr.looping.get() ? ("on: bar " + juce::String (session.timeToBar (tr.getLoopRange().getStart()), 2) + " to " + juce::String (session.timeToBar (tr.getLoopRange().getEnd()), 2)) : juce::String ("off"));
                 set (o, "click", e.clickTrackEnabled.get());
                 set (o, "length_seconds", e.getLength().inSeconds());
                 juce::Array<juce::var> tracks;
                 for (auto t : session.getTracks()) tracks.add (describeTrack (*t));
                 set (o, "tracks", tracks);
                 juce::Array<juce::var> master;
                 for (auto p : Session::getInserts (e.getMasterPluginList(), nullptr)) master.add (Signature::displayName (*p));
                 set (o, "master_inserts", master);
                 if (auto mv = e.getMasterVolumePlugin()) set (o, "master_volume_db", mv->getVolumeDb());
                 return ToolResult::ok (o);
             });

        add ("new_session", "Create and open a new empty session (4 audio tracks + 1 synth track), like Pro Tools Create Session. Optional sample_rate (44100-192000, default: the interface rate) and bit_depth (16, 24 or 32 = 32-bit float, the default). The current session is saved first.",
             R"({"type":"object","properties":{"name":{"type":"string","description":"Session name"},"sample_rate":{"type":"number"},"bit_depth":{"type":"integer","enum":[16,24,32]}}})",
             [this] (const juce::var& a)
             {
                 if (has (a, "sample_rate"))
                 {
                     static const double ok[] = { 44100, 48000, 88200, 96000, 176400, 192000 };
                     const double sr = num (a, "sample_rate", 0.0);
                     if (std::find (std::begin (ok), std::end (ok), sr) == std::end (ok)) return ToolResult::error ("sample_rate must be 44100, 48000, 88200, 96000, 176400 or 192000.");
                     session.newSessionSampleRate = sr;
                 }
                 if (has (a, "bit_depth"))
                 {
                     const int b = (int) num (a, "bit_depth", 32);
                     if (b != 16 && b != 24 && b != 32) return ToolResult::error ("bit_depth must be 16, 24 or 32 (32-bit float).");
                     session.newSessionBitDepth = b;
                 }
                 session.newSession (str (a, "name"));
                 return ToolResult::ok ("Created session at " + session.getFile().getFullPathName() + " (" + juce::String (TimeFmt::sampleRate (*session.edit) / 1000.0, 1) + " kHz, "
                                        + (session.recordBitDepth() == 32 ? juce::String ("32-bit float") : juce::String (session.recordBitDepth()) + "-bit") + ")");
             });

        add ("open_session", "Open a saved session: 'path' (full path to a .squelch file) or 'name' (a session in the Squelchmoth Sessions folder, as list_sessions shows it). The current session is saved first.",
             R"({"type":"object","properties":{"path":{"type":"string"},"name":{"type":"string"}}})",
             [this] (const juce::var& a)
             {
                 auto p = str (a, "path").trim();
                 if (p.isEmpty()) p = str (a, "name").trim();
                 if (p.isEmpty()) return ToolResult::error ("Pass 'path' or 'name' (see list_sessions).");
                 juce::File f = juce::File::isAbsolutePath (p) ? juce::File (p) : juce::File();
                 if (! f.existsAsFile()) f = session.sessionsDir.getChildFile (juce::File::isAbsolutePath (p) ? juce::File (p).getFileName() : p).withFileExtension (".squelch");
                 return session.openSession (f) ? ToolResult::ok ("Opened " + f.getFullPathName())
                                                : ToolResult::error ("Could not open " + f.getFullPathName());
             });

        add ("save_session", "Save the session. Pass 'path' to Save As a new file.",
             R"({"type":"object","properties":{"path":{"type":"string"}}})",
             [this] (const juce::var& a)
             {
                 if (! session.savingAllowed())
                     return ToolResult::error ("Saving is switched off. Tell the user to check Help > Licences and Upgrades or enter their key (Help > Enter Licence Key).");
                 if (has (a, "path"))
                 {
                     juce::File f (str (a, "path"));
                     if (! juce::File::isAbsolutePath (str (a, "path"))) f = session.sessionsDir.getChildFile (str (a, "path"));
                     return session.saveAs (f) ? ToolResult::ok ("Saved as " + session.getFile().getFullPathName()) : ToolResult::error ("Save failed");
                 }
                 return session.save() ? ToolResult::ok ("Saved " + session.getFile().getFullPathName()) : ToolResult::error ("Save failed");
             });

        add ("list_sessions", "List saved sessions in the Squelchmoth Sessions folder.",
             R"({"type":"object","properties":{}})",
             [this] (const juce::var&)
             {
                 juce::Array<juce::var> out;
                 for (auto f : session.sessionsDir.findChildFiles (juce::File::findFiles, false, "*.squelch"))
                     out.add (f.getFullPathName());
                 return ToolResult::ok (juce::var (out));
             });

        add ("undo", "Undo (or redo) the last edit operations.",
             R"({"type":"object","properties":{"redo":{"type":"boolean"},"steps":{"type":"integer","minimum":1}}})",
             [this] (const juce::var& a)
             {
                 int steps = (int) num (a, "steps", 1);
                 for (int i = 0; i < steps; ++i)
                     flag (a, "redo") ? edit().redo() : edit().undo();
                 return ToolResult::ok (juce::String (flag (a, "redo") ? "Redid " : "Undid ") + juce::String (steps) + " step(s).");
             });
    }

    //==============================================================================
    void registerTransportTools()
    {
        add ("transport",
             "Control playback. action: play, stop, record (records on armed tracks), toggle, play_from_start, go_to_start, go_to_end. Optionally set the playhead first with start_bar or start_seconds.",
             R"({"type":"object","properties":{"action":{"type":"string","enum":["play","stop","record","toggle","play_from_start","go_to_start","go_to_end"]},"start_bar":{"type":"number"},"start_seconds":{"type":"number"}},"required":["action"]})",
             [this] (const juce::var& a)
             {
                 auto& tr = edit().getTransport();
                 const auto explicitStart = timeArg (a, "start");
                 if (explicitStart) tr.setPosition (*explicitStart);
                 auto act = str (a, "action");
                 // the same transport the user's space bar drives: plays the Edit selection and stops at its end,
                 // pre/post-roll, Stop returns to where play started (unless Insertion Follows Playback), record modes
                 auto* tc = explicitStart ? nullptr : ui.transport;

                 if (act == "play")                 { if (tc) tc->play(); else if (explicitStart && ui.transport) ui.transport->playFrom (*explicitStart); else tr.play (false); }
                 else if (act == "stop")            { if (ui.transport) ui.transport->stop(); else tr.stop (false, false); }
                 else if (act == "record")
                 {
                     bool anyArmed = false;
                     for (auto t : session.getTracks()) anyArmed = anyArmed || session.isArmed (*t);
                     if (! anyArmed) return ToolResult::error ("No tracks are armed. Use update_track with armed=true first.");
                     if (tc) tc->record(); else tr.record (false);
                 }
                 else if (act == "toggle")          { if (tr.isPlaying()) { if (ui.transport) ui.transport->stop(); else tr.stop (false, false); } else if (tc) tc->play(); else tr.play (false); }
                 else if (act == "play_from_start") { if (ui.transport) ui.transport->playFrom (0s); else tr.playFromStart (false); }
                 else if (act == "go_to_start")     tr.setPosition (0s);
                 else if (act == "go_to_end")       tr.setPosition (te::toPosition (edit().getLength()));
                 else return ToolResult::error ("Unknown action " + act);

                 return ToolResult::ok ("OK. Playhead at " + session.describeTime (tr.getPosition()));
             });

        add ("set_loop", "Turn loop playback on/off and/or set the loop range (bars are 1-based, e.g. start_bar 5, end_bar 9 loops bars 5-8).",
             R"({"type":"object","properties":{"enabled":{"type":"boolean"},"start_bar":{"type":"number"},"end_bar":{"type":"number"},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"}}})",
             [this] (const juce::var& a)
             {
                 auto& tr = edit().getTransport();
                 auto s = timeArg (a, "start"), e = timeArg (a, "end");
                 if (s && e && *e > *s) tr.setLoopRange ({ *s, *e });
                 if (has (a, "enabled")) tr.looping = flag (a, "enabled");
                 return ToolResult::ok ("Loop " + juce::String (tr.looping.get() ? "on" : "off") + ", bars "
                                        + juce::String (session.timeToBar (tr.getLoopRange().getStart()), 2) + " - "
                                        + juce::String (session.timeToBar (tr.getLoopRange().getEnd()), 2));
             });

        add ("set_tempo", "Set the session tempo (BPM) and/or time signature.",
             R"({"type":"object","properties":{"bpm":{"type":"number","minimum":20,"maximum":400},"numerator":{"type":"integer"},"denominator":{"type":"integer"}}})",
             [this] (const juce::var& a)
             {
                 auto& ts = edit().tempoSequence;
                 if (has (a, "bpm")) ts.getTempo (0)->setBpm (num (a, "bpm"));
                 if (has (a, "numerator"))   ts.getTimeSig (0)->numerator = (int) num (a, "numerator");
                 if (has (a, "denominator")) ts.getTimeSig (0)->denominator = (int) num (a, "denominator");
                 return ToolResult::ok ("Tempo " + juce::String (ts.getTempo (0)->getBpm()) + " BPM, "
                                        + juce::String (ts.getTimeSig (0)->numerator.get()) + "/" + juce::String (ts.getTimeSig (0)->denominator.get()));
             });

        add ("set_click", "Metronome (click track) on/off and level.",
             R"({"type":"object","properties":{"enabled":{"type":"boolean"},"gain_db":{"type":"number"},"recording_only":{"type":"boolean"}}})",
             [this] (const juce::var& a)
             {
                 auto& e = edit();
                 if (has (a, "enabled")) e.clickTrackEnabled = flag (a, "enabled");
                 if (has (a, "gain_db")) e.clickTrackGain = juce::Decibels::decibelsToGain ((float) num (a, "gain_db"));
                 if (has (a, "recording_only")) e.clickTrackRecordingOnly = flag (a, "recording_only");
                 return ToolResult::ok (juce::String ("Click ") + (e.clickTrackEnabled.get() ? "on" : "off"));
             });
    }

    //==============================================================================
    void registerTrackTools()
    {
        add ("add_track",
             "Add tracks. kind: 'audio' (record/import audio), 'instrument' (MIDI track with a virtual instrument - pass 'instrument' as a plugin name: 'Mothsynth', 'Moth 808' (808 bass with glides), 'Blossom 2' (physical-model strings), 'Chrysalis Sampler', or one of your installed VST3/AU instruments), or 'bus' (aux return track you can feed with add_send).",
             R"({"type":"object","properties":{"kind":{"type":"string","enum":["audio","instrument","bus"]},"name":{"type":"string"},"instrument":{"type":"string"},"count":{"type":"integer","minimum":1,"maximum":32}},"required":["kind"]})",
             [this] (const juce::var& a)
             {
                 auto kind = str (a, "kind", "audio");
                 int count = (int) num (a, "count", 1);
                 juce::StringArray made;
                 for (int i = 0; i < count; ++i)
                 {
                     auto name = str (a, "name");
                     if (count > 1 && name.isNotEmpty()) name << " " << (i + 1);

                     te::AudioTrack* t = nullptr;
                     if (kind == "instrument" && has (a, "instrument"))
                     {
                         juce::String err;
                         auto inst = str (a, "instrument");
                         if (auto d = session.findPluginDescription (inst); d != nullptr && d->isInstrument)
                             t = session.addTrack (kind, name, {}, d.get());
                         else
                         {
                             juce::String type = "4osc";
                             const Signature::Entry* sigEntry = nullptr;
                             if (auto se = Signature::findByName (inst); se != nullptr && juce::String (se->category) == "Instruments" && juce::String (se->type) != "sampler")
                                 { type = se->type; sigEntry = se; }
                             else
                                 for (auto& b : builtinPlugins)
                                     if (juce::String (b.name).containsIgnoreCase (inst) || inst.equalsIgnoreCase (b.type)) { type = b.type; break; }
                             t = session.addTrack (kind, name, type);
                             if (t != nullptr && sigEntry != nullptr && ! sigEntry->presets.empty())
                                 for (auto p : t->pluginList.getPlugins())
                                     if (p->getPluginType() == type) { Signature::applyPreset (*p, sigEntry->presets.front()); break; }
                         }
                     }
                     else
                         t = session.addTrack (kind, name);

                     if (t != nullptr) made.add (juce::String (session.indexOf (t)) + ": " + t->getName());
                     else if (session.lastError.isNotEmpty())
                     {
                         if (made.isEmpty()) return ToolResult::error (session.lastError);
                         return ToolResult::ok ("Added " + made.joinIntoString (", ") + ". Stopped: " + session.lastError);
                     }
                 }
                 return ToolResult::ok ("Added " + made.joinIntoString (", "));
             });

        add ("delete_track", "Delete a track (and its clips). Undoable.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto name = t->getName();
                 edit().deleteTrack (t);
                 return ToolResult::ok ("Deleted track " + name);
             });

        add ("update_track",
             "Change a track's mixer and routing settings. Any combination of: name, volume_db (-inf..+6), pan (-1 left .. +1 right), mute, solo, armed (record-enable), monitor (input monitoring), colour (hex like '#ff3ad1'), output (an output path like 'Out 1-2', a bus name, another track, 'master' or 'none'), input (a Pro Tools path like 'In 1' / 'In 1-2', a bus name, a MIDI device or 'All', or 'no input'), move_to (new track number).",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"name":{"type":"string"},"volume_db":{"type":"number"},"pan":{"type":"number","minimum":-1,"maximum":1},"mute":{"type":"boolean"},"solo":{"type":"boolean"},"armed":{"type":"boolean"},"monitor":{"type":"boolean"},"colour":{"type":"string"},"output":{"type":["string","integer"]},"input":{"type":"string"},"move_to":{"type":"integer"},"stereo_pan":{"type":"boolean","description":"Pro Tools style two-knob panner for stereo tracks"},"pan_l":{"type":"number","minimum":-1,"maximum":1},"pan_r":{"type":"number","minimum":-1,"maximum":1}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 if (isMasterRef (a))
                 {
                     if (has (a, "volume_db")) edit().getMasterVolumePlugin()->setVolumeDb ((float) num (a, "volume_db"));
                     if (has (a, "pan"))       edit().getMasterVolumePlugin()->setPan ((float) num (a, "pan"));
                     return ToolResult::ok ("Master updated: " + juce::String (edit().getMasterVolumePlugin()->getVolumeDb(), 1) + " dB");
                 }

                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);

                 if (has (a, "name"))      t->setName (str (a, "name"));
                 if (has (a, "volume_db")) t->getVolumePlugin()->setVolumeDb ((float) num (a, "volume_db"));
                 if (has (a, "pan"))       t->getVolumePlugin()->setPan ((float) juce::jlimit (-1.0, 1.0, num (a, "pan")));
                 if (has (a, "stereo_pan")) session.setStereoPanEnabled (*t, flag (a, "stereo_pan"));
                 if (has (a, "pan_l") || has (a, "pan_r"))
                 {
                     auto sp = session.setStereoPanEnabled (*t, true);
                     if (sp != nullptr && has (a, "pan_l")) sp->panL->setParameter ((float) juce::jlimit (-1.0, 1.0, num (a, "pan_l")), juce::sendNotification);
                     if (sp != nullptr && has (a, "pan_r")) sp->panR->setParameter ((float) juce::jlimit (-1.0, 1.0, num (a, "pan_r")), juce::sendNotification);
                 }
                 if (has (a, "mute"))      t->setMute (flag (a, "mute"));
                 if (has (a, "solo"))      t->setSolo (flag (a, "solo"));
                 if (has (a, "colour"))    t->setColour (juce::Colour::fromString ("ff" + str (a, "colour").removeCharacters ("#").getLastCharacters (6)));

                 if (has (a, "input"))
                 {
                     auto r = routeInput (*t, str (a, "input"));
                     if (r.isError) return r;
                 }

                 if (has (a, "armed") && ! session.setArmed (*t, flag (a, "armed")))
                     return ToolResult::error ("Track has no input assigned, so it can't be armed. Set 'input' first (see list_inputs).");

                 if (has (a, "monitor"))
                     TrackOps::setInputMonitor (session, *t, flag (a, "monitor"));   // this track only

                 if (has (a, "output"))
                 {
                     auto r = routeOutput (*t, a["output"]);
                     if (r.isError) return r;
                 }

                 if (has (a, "move_to"))
                 {
                     auto tracks = session.getTracks();
                     int target = juce::jlimit (1, tracks.size(), (int) num (a, "move_to"));
                     tracks.removeFirstMatchingValue (t);
                     te::Track* preceding = target > 1 ? tracks[target - 2] : nullptr;
                     edit().moveTrack (t, { nullptr, preceding });
                 }

                 return ToolResult::ok (describeTrack (*t));
             });

        add ("list_inputs", "List the audio and MIDI inputs available for recording (use with update_track input=...).",
             R"({"type":"object","properties":{}})",
             [this] (const juce::var&)
             {
                 juce::Array<juce::var> out;
                 for (auto in : edit().getAllInputDevices())
                 {
                     if (in->getInputDevice().isTrackDevice()) continue;
                     auto o = obj();
                     set (o, "name", IO::inputDeviceName (in->getInputDevice()));
                     if (auto w = dynamic_cast<te::WaveInputDevice*> (&in->getInputDevice())) set (o, "format", w->getChannels().size() >= 2 ? "stereo" : "mono");
                     auto type = in->getInputDevice().getDeviceType();
                     set (o, "type", type == te::InputDevice::waveDevice ? "audio" : "midi");
                     juce::StringArray targets;
                     for (auto id : in->getTargets())
                         if (auto tt = te::findTrackForID (edit(), id)) targets.add (tt->getName());
                     set (o, "assigned_to", targets.joinIntoString (", "));
                     out.add (o);
                 }
                 auto& dm = session.getEngine().getDeviceManager().deviceManager;
                 auto o = obj();
                 set (o, "inputs", out);
                 if (auto dev = dm.getCurrentAudioDevice())
                 {
                     set (o, "audio_device", dev->getName());
                     set (o, "sample_rate", dev->getCurrentSampleRate());
                     set (o, "buffer_size", dev->getCurrentBufferSizeSamples());
                 }
                 return ToolResult::ok (o);
             });
    }

    //==============================================================================
    void registerClipTools()
    {
        add ("list_clips", "List clips (regions) on one track or all tracks, with positions in bars and seconds.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]}}})",
             [this] (const juce::var& a)
             {
                 juce::Array<juce::var> out;
                 for (auto t : session.getTracks())
                 {
                     if (has (a, "track") && t != session.findTrack (a["track"])) continue;
                     auto o = obj();
                     set (o, "track", session.indexOf (t));
                     set (o, "track_name", t->getName());
                     juce::Array<juce::var> clips;
                     int i = 1;
                     for (auto c : t->getClips()) clips.add (describeClip (*c, i++));
                     set (o, "clips", clips);
                     out.add (o);
                 }
                 return ToolResult::ok (juce::var (out));
             });

        add ("import_audio", "Import an audio file (wav, aiff, flac, ogg, mp3) from disk onto a track at a position (default bar 1).",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"path":{"type":"string"},"start_bar":{"type":"number"},"start_seconds":{"type":"number"},"name":{"type":"string"}},"required":["track","path"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 juce::File f (str (a, "path"));
                 if (! f.existsAsFile()) return ToolResult::error ("File not found: " + f.getFullPathName());
                 te::AudioFile af (session.getEngine(), f);
                 if (! af.isValid()) return ToolResult::error ("Not a readable audio file: " + f.getFullPathName());
                 auto start = timeArg (a, "start").value_or (0s);
                 session.prepareTrackForFile (*t, af.getInfo().numChannels);
                 auto clip = t->insertWaveClip (str (a, "name", f.getFileNameWithoutExtension()), f,
                                                { { start, te::TimeDuration::fromSeconds (af.getLength()) }, {} }, false);
                 if (clip == nullptr) return ToolResult::error ("Import failed");
                 EditOps::resolveOverlaps (*clip);
                 return ToolResult::ok (describeClip (*clip, t->getClips().indexOf (clip.get()) + 1));
             });

        add ("edit_clip",
             "Move, trim or change a clip. Options: start_bar/start_seconds (move), length_bars/length_beats/length_seconds (trim end), name, gain_db, fade_in_seconds, fade_out_seconds, reverse, pitch_semitones (audio), loop (repeat the clip content), move_to_track, colour.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"start_bar":{"type":"number"},"start_seconds":{"type":"number"},"length_bars":{"type":"number"},"length_beats":{"type":"number"},"length_seconds":{"type":"number"},"name":{"type":"string"},"gain_db":{"type":"number"},"fade_in_seconds":{"type":"number"},"fade_out_seconds":{"type":"number"},"reverse":{"type":"boolean"},"pitch_semitones":{"type":"number"},"loop":{"type":"boolean"},"move_to_track":{"type":["string","integer"]},"colour":{"type":"string"}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto c = clipArg (*t, a, err);
                 if (c == nullptr) return ToolResult::error (err);

                 if (auto s = timeArg (a, "start"))  c->setStart (*s, false, true);
                 if (auto l = durationArg (a, "length", c->getPosition().getStart())) c->setLength (*l, true);
                 if (has (a, "name"))   c->setName (str (a, "name"));
                 if (has (a, "colour")) c->setColour (juce::Colour::fromString ("ff" + str (a, "colour").removeCharacters ("#").getLastCharacters (6)));

                 if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
                 {
                     if (has (a, "gain_db"))          ac->setGainDB ((float) num (a, "gain_db"));
                     if (has (a, "fade_in_seconds"))  ac->setFadeIn (te::TimeDuration::fromSeconds (num (a, "fade_in_seconds")));
                     if (has (a, "fade_out_seconds")) ac->setFadeOut (te::TimeDuration::fromSeconds (num (a, "fade_out_seconds")));
                     if (has (a, "reverse"))          ac->setIsReversed (flag (a, "reverse"));
                     if (has (a, "pitch_semitones"))  ac->setPitchChange ((float) num (a, "pitch_semitones"));
                 }

                 if (has (a, "loop"))
                 {
                     if (flag (a, "loop"))
                     {
                         if (auto mc = dynamic_cast<te::MidiClip*> (c))
                             mc->setLoopRangeBeats ({ te::BeatPosition(), te::BeatPosition::fromBeats (mc->getLengthInBeats().inBeats()) });
                         else if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
                             ac->setLoopRange ({ te::TimePosition(), te::toPosition (te::TimeDuration::fromSeconds (ac->getSourceLength().inSeconds())) });
                     }
                     else
                         c->disableLooping();
                 }

                 if (has (a, "move_to_track"))
                 {
                     auto dest = session.findTrack (a["move_to_track"]);
                     if (dest == nullptr) return ToolResult::error ("move_to_track not found");
                     c->moveTo (*dest);
                     t = dest;
                 }
                 return ToolResult::ok (describeClip (*c, t->getClips().indexOf (c) + 1));
             });

        add ("split_clip", "Split clip(s) on a track at a position (bar or seconds). Omit 'clip' to split whatever clip is under that position.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"at_bar":{"type":"number"},"at_seconds":{"type":"number"}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto at = timeArg (a, "at").value_or (edit().getTransport().getPosition());

                 te::Clip* target = nullptr;
                 if (has (a, "clip")) target = clipArg (*t, a, err);
                 else for (auto c : t->getClips()) if (c->getPosition().time.containsInclusive (at)) target = c;

                 if (target == nullptr) return ToolResult::error ("No clip at " + session.describeTime (at));
                 auto newClip = t->splitClip (*target, at);
                 return newClip != nullptr ? ToolResult::ok ("Split at " + session.describeTime (at))
                                           : ToolResult::error ("Split position must be inside the clip.");
             });

        add ("delete_clip", "Delete a clip. Undoable.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]}},"required":["track","clip"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto c = clipArg (*t, a, err);
                 if (c == nullptr) return ToolResult::error (err);
                 auto name = c->getName();
                 c->removeFromParent();
                 return ToolResult::ok ("Deleted clip " + name);
             });

        add ("duplicate_clip", "Duplicate a clip end-to-end 'times' times (like Pro Tools Repeat).",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"times":{"type":"integer","minimum":1,"maximum":64}},"required":["track","clip"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto c = clipArg (*t, a, err);
                 if (c == nullptr) return ToolResult::error (err);
                 int times = (int) num (a, "times", 1);
                 auto len = c->getPosition().getLength();
                 auto start = c->getPosition().getStart();
                 for (int i = 1; i <= times; ++i)
                 {
                     auto newState = c->state.createCopy();
                     te::EditItemID::remapIDs (newState, nullptr, edit());
                     if (auto nc = t->insertClipWithState (newState))
                         nc->setStart (start + len * (double) i, false, true);
                 }
                 return ToolResult::ok ("Duplicated " + c->getName() + " x" + juce::String (times));
             });
    }

    //==============================================================================
    void registerMidiTools()
    {
        add ("create_midi_clip",
             "Create a MIDI clip on an instrument track, optionally filled with notes. Notes use beats relative to the clip start (1 beat = 1 quarter note at 4/4). pitch may be a MIDI number (60) or a name ('C4', 'F#3', 'Bb2'); C4 = middle C = 60.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"start_bar":{"type":"number"},"start_seconds":{"type":"number"},"length_bars":{"type":"number"},"length_beats":{"type":"number"},"name":{"type":"string"},"notes":{"type":"array","items":{"type":"object","properties":{"pitch":{"type":["string","integer"]},"start_beat":{"type":"number"},"length_beats":{"type":"number"},"velocity":{"type":"integer","minimum":1,"maximum":127}},"required":["pitch","start_beat","length_beats"]}}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto start = timeArg (a, "start").value_or (session.barToTime (1));
                 auto len = durationArg (a, "length", start).value_or (session.barToTime (session.timeToBar (start) + 4) - start);
                 auto clip = t->insertMIDIClip (str (a, "name", "MIDI"), { start, start + len }, nullptr);
                 if (clip == nullptr) return ToolResult::error ("Could not create clip");
                 int n = addNotes (*clip, a["notes"]);
                 auto res = describeClip (*clip, t->getClips().indexOf (clip.get()) + 1);
                 set (res, "notes_added", n);
                 auto dest = t->getOutput().getDestinationTrack();
                 bool plays = Session::isInstrumentTrack (*t) || (dest != nullptr && Session::isInstrumentTrack (*dest)) || t->getOutput().canPlayMidi();
                 if (! plays)
                     set (res, "warning", "This track has no instrument plugin and its output is not an Instrument track, so the MIDI won't make sound. insert_plugin an instrument, or set_output to an Instrument track.");
                 return ToolResult::ok (res);
             });

        add ("add_midi_notes", "Add notes to an existing MIDI clip (beats relative to clip start). Set replace=true to clear the clip first.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"replace":{"type":"boolean"},"notes":{"type":"array","items":{"type":"object","properties":{"pitch":{"type":["string","integer"]},"start_beat":{"type":"number"},"length_beats":{"type":"number"},"velocity":{"type":"integer"}},"required":["pitch","start_beat","length_beats"]}}},"required":["track","notes"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto mc = dynamic_cast<te::MidiClip*> (clipArg (*t, a, err));
                 if (mc == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "That clip is not a MIDI clip.");
                 if (flag (a, "replace")) mc->getSequence().clear (um());
                 int n = addNotes (*mc, a["notes"]);
                 return ToolResult::ok ("Added " + juce::String (n) + " notes; clip now has " + juce::String (mc->getSequence().getNumNotes()));
             });

        add ("get_midi_notes", "Read all notes in a MIDI clip.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto mc = dynamic_cast<te::MidiClip*> (clipArg (*t, a, err));
                 if (mc == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "Not a MIDI clip.");
                 juce::Array<juce::var> out;
                 for (auto n : mc->getSequence().getNotes())
                 {
                     auto o = obj();
                     set (o, "pitch", n->getNoteNumber());
                     set (o, "name", juce::MidiMessage::getMidiNoteName (n->getNoteNumber(), true, true, 4));
                     set (o, "start_beat", n->getStartBeat().inBeats());
                     set (o, "length_beats", n->getLengthBeats().inBeats());
                     set (o, "velocity", n->getVelocity());
                     out.add (o);
                 }
                 return ToolResult::ok (juce::var (out));
             });

        add ("quantize_midi", "Quantize note starts in a MIDI clip to a grid (grid_beats: 1 = quarter, 0.5 = eighth, 0.25 = sixteenth). strength 0..1.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"grid_beats":{"type":"number"},"strength":{"type":"number"}},"required":["track","grid_beats"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto mc = dynamic_cast<te::MidiClip*> (clipArg (*t, a, err));
                 if (mc == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "Not a MIDI clip.");
                 double grid = juce::jmax (0.015625, num (a, "grid_beats", 0.25)), strength = juce::jlimit (0.0, 1.0, num (a, "strength", 1.0));
                 int n = 0;
                 for (auto note : mc->getSequence().getNotes())
                 {
                     auto s = note->getStartBeat().inBeats();
                     auto q = std::round (s / grid) * grid;
                     note->setStartAndLength (te::BeatPosition::fromBeats (s + (q - s) * strength), note->getLengthBeats(), um());
                     ++n;
                 }
                 return ToolResult::ok ("Quantized " + juce::String (n) + " notes to " + juce::String (grid) + " beats");
             });

        add ("transpose_midi", "Transpose every note in a MIDI clip by a number of semitones.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"semitones":{"type":"integer"}},"required":["track","semitones"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto mc = dynamic_cast<te::MidiClip*> (clipArg (*t, a, err));
                 if (mc == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "Not a MIDI clip.");
                 int st = (int) num (a, "semitones");
                 for (auto note : mc->getSequence().getNotes())
                     note->setNoteNumber (juce::jlimit (0, 127, note->getNoteNumber() + st), um());
                 return ToolResult::ok ("Transposed by " + juce::String (st));
             });
    }

    int addNotes (te::MidiClip& clip, const juce::var& notes)
    {
        int n = 0;
        if (auto arr = notes.getArray())
            for (auto& nv : *arr)
            {
                auto pitch = parsePitch (nv["pitch"]);
                auto startB = (double) nv["start_beat"];
                auto lenB = juce::jmax (0.01, (double) nv["length_beats"]);
                int vel = nv.hasProperty ("velocity") ? juce::jlimit (1, 127, (int) nv["velocity"]) : 100;
                clip.getSequence().addNote (pitch, te::BeatPosition::fromBeats (startB), te::BeatDuration::fromBeats (lenB), vel, 0, um());
                ++n;
            }
        return n;
    }

    //==============================================================================
    void registerPluginTools()
    {
        add ("list_plugins",
             "List every plugin available: your scanned VST3/AU plugins (Waves, FabFilter, etc.), the Squelchmoth Signature bundle (87: 27 instruments and 60 processors - Hexwing, Pollen, Mandible, Prism EQ, Apex Triad, Time Weaver, Ember Tape, Aurora Verb...), and the full AirWindows collection (186). Filter by text and/or category ('instrument', 'effect', 'signature', 'builtin', 'thirdparty'). Insert any of them by name with insert_plugin.",
             R"({"type":"object","properties":{"filter":{"type":"string"},"category":{"type":"string","enum":["all","instrument","effect","signature","builtin","thirdparty"]}}})",
             [this] (const juce::var& a)
             {
                 auto filter = str (a, "filter");
                 auto cat = str (a, "category", "all");
                 juce::Array<juce::var> out;

                 if (cat != "builtin" && cat != "signature")
                 {
                     // one line per plug-in, as the menus show them (layout variants and duplicate formats folded together)
                     auto types = session.getEngine().getPluginManager().knownPluginList.getTypes();
                     for (auto& it : PluginCatalog::thirdPartyItems (types, true))
                     {
                         if (cat == "instrument" && ! it.isInstrument) continue;
                         if (cat == "effect" && it.isInstrument) continue;
                         if (filter.isNotEmpty() && ! (it.name + " " + it.vendor + " " + it.category).containsIgnoreCase (filter)) continue;
                         auto o = obj();
                         set (o, "name", it.name);
                         set (o, "maker", it.vendor);
                         set (o, "format", it.format);
                         set (o, "category", it.category);
                         set (o, "kind", it.isInstrument ? "instrument" : "effect");
                         out.add (o);
                     }
                 }

                 if (cat != "thirdparty" && cat != "builtin")
                     for (auto& e : Signature::entries())
                     {
                         bool inst = juce::String (e.category) == "Instruments";
                         if (cat == "instrument" && ! inst) continue;
                         if (cat == "effect" && inst) continue;
                         if (filter.isNotEmpty() && ! (juce::String (e.name) + " " + e.category + " " + e.tagline).containsIgnoreCase (filter)) continue;
                         auto o = obj();
                         set (o, "name", e.name);
                         set (o, "maker", "Squelchmoth Signature");
                         set (o, "kind", inst ? "instrument" : e.category);
                         set (o, "what", e.tagline);
                         if (! e.presets.empty()) { juce::Array<juce::var> pr; for (auto& pp : e.presets) pr.add (pp.name); set (o, "presets", pr); }
                         out.add (o);
                     }

                 if (cat != "thirdparty" && cat != "signature")
                     for (auto& b : builtinPlugins)
                     {
                         bool inst = juce::String (b.category) == "Instrument";
                         if (cat == "instrument" && ! inst) continue;
                         if (cat == "effect" && inst) continue;
                         if (filter.isNotEmpty() && ! (juce::String (b.name) + " " + b.category).containsIgnoreCase (filter)) continue;
                         auto o = obj();
                         set (o, "name", b.name);
                         set (o, "maker", "Squelchmoth built-in");
                         set (o, "kind", inst ? "instrument" : b.category);
                         out.add (o);
                     }

                 auto res = obj();
                 set (res, "count", out.size());
                 set (res, "plugins", out);
                 set (res, "scan_status", scanner.getStatus());
                 return ToolResult::ok (res);
             });

        add ("scan_plugins", "Scan the Mac's VST3 and Audio Unit folders for installed plugins (runs in the background; poll with list_plugins to see scan_status). Crashing plugins are skipped automatically.",
             R"({"type":"object","properties":{"rescan_all":{"type":"boolean"}}})",
             [this] (const juce::var& a)
             {
                 scanner.startScan (flag (a, "rescan_all"));
                 return ToolResult::ok ("Scan started. " + scanner.getStatus());
             });

        add ("insert_plugin", "Insert a plugin (your VST3/AU or a built-in) on a track or 'master'. slot is 1-based (omit to append).",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"plugin":{"type":"string"},"slot":{"type":"integer"},"open_window":{"type":"boolean"}},"required":["track","plugin"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 te::AudioTrack* t = nullptr;
                 auto list = pluginListArg (a, err, t);
                 if (list == nullptr) return ToolResult::error (err);

                 auto p = session.createPluginByName (str (a, "plugin"), err);
                 if (p == nullptr) return ToolResult::error (err);

                 auto inserts = Session::getInserts (*list, t);
                 int index;
                 if (has (a, "slot") && inserts[(int) num (a, "slot") - 1] != nullptr)
                     index = list->indexOf (inserts[(int) num (a, "slot") - 1]);
                 else if (t != nullptr)
                     index = Session::insertEndIndex (*list, t);   // after existing inserts, before pan / sends / fader
                 else
                     index = -1;

                 if (p->isSynth() && t != nullptr) index = 0;   // instruments go first
                 list->insertPlugin (p, index, nullptr);

                 if (flag (a, "open_window")) p->showWindowExplicitly();

                 auto newInserts = Session::getInserts (*list, t);
                 return ToolResult::ok ("Inserted " + Signature::displayName (*p) + " in slot " + juce::String (newInserts.indexOf (p.get()) + 1)
                                        + ". Inserts now: " + describeInserts (newInserts)
                                        + ". It has " + juce::String (p->getNumAutomatableParameters()) + " parameters (get_plugin_parameters).");
             });

        add ("remove_plugin", "Remove an insert (by slot number or plugin name) from a track or master.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]}},"required":["track","slot"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 auto name = p->getName();
                 p->deleteFromParent();
                 return ToolResult::ok ("Removed " + name);
             });

        add ("bypass_plugin", "Bypass (disable) or re-enable an insert.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"bypassed":{"type":"boolean"}},"required":["track","slot","bypassed"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 p->setEnabled (! flag (a, "bypassed"));
                 return ToolResult::ok (Signature::displayName (*p) + (p->isEnabled() ? " active" : " bypassed"));
             });

        add ("move_plugin", "Reorder an insert to a different slot on the same track.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"to_slot":{"type":"integer"}},"required":["track","slot","to_slot"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 te::AudioTrack* t = nullptr;
                 auto list = pluginListArg (a, err, t);
                 auto inserts = Session::getInserts (*list, t);
                 int to = juce::jlimit (1, inserts.size(), (int) num (a, "to_slot"));
                 if (inserts[to - 1] == p) return ToolResult::ok ("Already there");
                 auto stateCopy = p->state;
                 auto parent = stateCopy.getParent();
                 auto targetIndex = parent.indexOf (inserts[to - 1]->state);
                 parent.moveChild (parent.indexOf (stateCopy), targetIndex, um());
                 return ToolResult::ok ("Inserts now: " + describeInserts (Session::getInserts (*list, t)));
             });

        add ("get_plugin_parameters", "List a plugin's parameters with current values (normalised 0-1 plus the plugin's own display text). Use 'filter' to narrow big plugins.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"filter":{"type":"string"}},"required":["track","slot"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 auto filter = str (a, "filter");
                 juce::Array<juce::var> out;
                 auto params = p->getAutomatableParameters();
                 for (int i = 0; i < params.size(); ++i)
                     if (filter.isEmpty() || params[i]->getParameterName().containsIgnoreCase (filter))
                         out.add (describeParam (*params[i], i));
                 auto res = obj();
                 set (res, "plugin", Signature::displayName (*p));
                 set (res, "bypassed", ! p->isEnabled());
                 if (auto ext = dynamic_cast<te::ExternalPlugin*> (p))
                 {
                     set (res, "preset", ext->getCurrentProgramName());
                     set (res, "num_presets", ext->getNumPrograms());
                 }
                 set (res, "parameters", out);
                 return ToolResult::ok (res);
             });

        add ("set_plugin_parameters",
             "Set one or more plugin parameters. Each item: {param: index or name, value: 0..1 normalised} OR {param, text: '-6 dB'} to set via the plugin's own text format OR {param, real: number} in the parameter's native range.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"params":{"type":"array","items":{"type":"object","properties":{"param":{"type":["string","integer"]},"value":{"type":"number"},"text":{"type":"string"},"real":{"type":"number"}},"required":["param"]}}},"required":["track","slot","params"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 juce::StringArray done, failed;
                 if (auto arr = a["params"].getArray())
                     for (auto& item : *arr)
                     {
                         auto param = findParam (*p, item["param"]);
                         if (param == nullptr) { failed.add (item["param"].toString()); continue; }

                         if (item.hasProperty ("text"))       param->setParameter (param->stringToValue (item["text"].toString()), juce::sendNotification);
                         else if (item.hasProperty ("real"))  param->setParameter ((float) (double) item["real"], juce::sendNotification);
                         else                                 param->setNormalisedParameter ((float) juce::jlimit (0.0, 1.0, (double) item["value"]), juce::sendNotification);

                         done.add (param->getParameterName() + " = " + param->getCurrentValueAsStringWithLabel());
                     }
                 edit().pluginChanged (*p);
                 auto msg = "Set: " + done.joinIntoString ("; ");
                 if (! failed.isEmpty()) msg << ". Not found: " << failed.joinIntoString (", ");
                 return failed.isEmpty() ? ToolResult::ok (msg) : ToolResult::error (msg);
             });

        add ("plugin_presets",
             "Work with a plugin's presets. action: 'list' (factory programs - every Signature plugin has a Basic and an Advanced bank), 'load' (by 'name' or 'index' from the list; 'preset' also works), 'save' (save the current state to a .smpreset file in the presets folder under 'name'), 'recall' (load a saved .smpreset by 'name').",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"action":{"type":"string","enum":["list","load","save","recall"]},"index":{"type":"integer"},"name":{"type":"string"},"preset":{"type":"string"}},"required":["track","slot"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 auto act = str (a, "action", has (a, "preset") || has (a, "name") || has (a, "index") ? "load" : "list");
                 if (has (a, "preset") && ! has (a, "name")) { auto copy = a.clone(); copy.getDynamicObject()->setProperty ("name", a["preset"]); return call ("plugin_presets", copy); }
                 auto presetDir = session.sessionsDir.getChildFile ("Presets").getChildFile (juce::File::createLegalFileName (p->getName()));

                 if (act == "save" || act == "recall")
                 {
                     presetDir.createDirectory();
                     auto f = presetDir.getChildFile (juce::File::createLegalFileName (str (a, "name", "Preset")) + ".smpreset");
                     if (act == "save")
                     {
                         edit().flushPluginStateIfNeeded (*p);
                         return f.replaceWithText (p->state.toXmlString()) ? ToolResult::ok ("Saved " + f.getFullPathName()) : ToolResult::error ("Write failed");
                     }
                     auto xml = juce::parseXML (f);
                     if (xml == nullptr) return ToolResult::error ("No preset " + f.getFullPathName() + ". Saved presets: " + juce::String (presetDir.findChildFiles (juce::File::findFiles, false, "*.smpreset").size()));
                     auto vt = juce::ValueTree::fromXml (*xml);
                     p->restorePluginStateFromValueTree (vt);
                     return ToolResult::ok ("Recalled " + f.getFileNameWithoutExtension());
                 }

                 auto ext = dynamic_cast<te::ExternalPlugin*> (p);
                 if (ext == nullptr)
                 {
                     // Signature plugins: factory banks (Basic + Advanced) by name or index
                     auto e = Signature::findForPlugin (*p);
                     if (e == nullptr || e->presets.empty()) return ToolResult::error ("This plugin has no factory presets; use save/recall.");
                     if (act == "list")
                     {
                         juce::Array<juce::var> out;
                         for (size_t i = 0; i < e->presets.size(); ++i)
                         {
                             auto o = obj(); const juce::String full (e->presets[i].name);
                             set (o, "index", (int) i);
                             set (o, "name", full.contains (" | ") ? full.fromFirstOccurrenceOf (" | ", false, false) : full);
                             if (full.contains (" | ")) set (o, "category", full.upToFirstOccurrenceOf (" | ", false, false));
                             set (o, "bank", e->presets[i].advanced ? "advanced" : "basic"); out.add (o);
                         }
                         auto res = obj(); set (res, "plugin", Signature::displayName (*p)); set (res, "presets", out);
                         return ToolResult::ok (res);
                     }
                     const Signature::Preset* chosen = nullptr;
                     if (has (a, "name")) for (auto& pr : e->presets) if (juce::String (pr.name).equalsIgnoreCase (str (a, "name")) || juce::String (pr.name).fromFirstOccurrenceOf (" | ", false, false).equalsIgnoreCase (str (a, "name"))) { chosen = &pr; break; }
                     if (chosen == nullptr && has (a, "name")) for (auto& pr : e->presets) if (juce::String (pr.name).containsIgnoreCase (str (a, "name"))) { chosen = &pr; break; }
                     if (chosen == nullptr && has (a, "index")) { auto i = (int) num (a, "index"); if (i >= 0 && i < (int) e->presets.size()) chosen = &e->presets[(size_t) i]; }
                     if (chosen == nullptr) return ToolResult::error ("No preset matches. Call with action 'list' to see the bank.");
                     Signature::applyPreset (*p, *chosen);
                     return ToolResult::ok ("Loaded preset: " + juce::String (chosen->name) + " on " + Signature::displayName (*p));
                 }

                 if (act == "load")
                 {
                     ext->setCurrentProgram ((int) num (a, "index"), true);
                     return ToolResult::ok ("Loaded preset: " + ext->getCurrentProgramName());
                 }

                 juce::Array<juce::var> out;
                 for (int i = 0; i < juce::jmin (ext->getNumPrograms(), 500); ++i)
                     out.add (juce::String (i) + ": " + ext->getProgramName (i));
                 for (auto f : presetDir.findChildFiles (juce::File::findFiles, false, "*.smpreset"))
                     out.add ("saved: " + f.getFileNameWithoutExtension());
                 return ToolResult::ok (juce::var (out));
             });

        add ("open_plugin_window", "Open (or close) a plugin's editor window on screen.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"close":{"type":"boolean"}},"required":["track","slot"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 if (flag (a, "close")) p->windowState->closeWindowExplicitly();
                 else                   p->showWindowExplicitly();
                 return ToolResult::ok ("OK");
             });
    }

    //==============================================================================
    void registerMixTools()
    {
        add ("add_send",
             "Create or update an aux send from a track to a bus. bus: a bus track name/number, a bus number, or 'new' to create a new bus track (e.g. for a shared reverb). level_db default -10.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"bus":{"type":["string","integer"]},"level_db":{"type":"number"},"bus_name":{"type":"string"}},"required":["track","bus"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);

                 int busNum = -1;
                 te::AudioTrack* busTrack = nullptr;
                 auto busRef = a["bus"];

                 if (busRef.toString().equalsIgnoreCase ("new"))
                 {
                     busTrack = session.addTrack ("bus", str (a, "bus_name"));
                     busNum = session.getBusNumberOfTrack (*busTrack);
                 }
                 else if (auto bt = session.findTrack (busRef); bt != nullptr && session.getBusNumberOfTrack (*bt) >= 0)
                 {
                     busTrack = bt;
                     busNum = session.getBusNumberOfTrack (*bt);
                 }
                 else if (busRef.isInt() || busRef.toString().containsOnly ("0123456789"))
                 {
                     busNum = (int) busRef - 1;
                     busTrack = session.findBusTrack (busNum);
                 }

                 if (busNum < 0) return ToolResult::error ("No bus matches. Use bus='new' or the name of a bus track.");

                 te::AuxSendPlugin* send = nullptr;
                 for (auto s : Session::getSends (*t))
                     if (s->getBusNumber() == busNum) send = s;

                 if (send == nullptr)
                 {
                     auto p = edit().getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
                     t->pluginList.insertPlugin (p, t->pluginList.indexOf (t->getLevelMeterPlugin()), nullptr);
                     send = dynamic_cast<te::AuxSendPlugin*> (p.get());
                     send->busNumber = busNum;
                 }
                 send->setGainDb ((float) num (a, "level_db", -10.0));
                 return ToolResult::ok ("Send from " + t->getName() + " to " + (busTrack != nullptr ? busTrack->getName() : "bus " + juce::String (busNum + 1))
                                        + " at " + juce::String (send->getGainDb(), 1) + " dB");
             });

        add ("write_automation",
             "Write automation points. target: 'volume' (dB), 'pan' (-1..1), or a plugin parameter via slot + param (value 0..1 normalised). Points: [{bar or seconds, value}]. clear_first removes existing points in that range.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"target":{"type":"string"},"slot":{"type":["string","integer"]},"param":{"type":["string","integer"]},"clear_first":{"type":"boolean"},"points":{"type":"array","items":{"type":"object","properties":{"bar":{"type":"number"},"seconds":{"type":"number"},"value":{"type":"number"},"curve":{"type":"number"}},"required":["value"]}}},"required":["track","points"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 te::AutomatableParameter* param = nullptr;
                 bool isVolume = false;
                 auto target = str (a, "target", has (a, "param") ? "plugin" : "volume");

                 te::VolumeAndPanPlugin* vp = nullptr;
                 if (isMasterRef (a)) vp = edit().getMasterVolumePlugin().get();
                 else if (target == "volume" || target == "pan")
                 {
                     auto t = trackArg (a, err);
                     if (t == nullptr) return ToolResult::error (err);
                     vp = t->getVolumePlugin();
                 }

                 if (target == "volume")      { param = vp->volParam.get(); isVolume = true; }
                 else if (target == "pan")    param = vp->panParam.get();
                 else
                 {
                     auto p = insertArg (a, err);
                     if (p == nullptr) return ToolResult::error (err);
                     param = findParam (*p, a["param"]);
                     if (param == nullptr) return ToolResult::error ("Parameter not found. Use get_plugin_parameters.");
                 }

                 auto& curve = param->getCurve();
                 juce::Array<std::pair<te::TimePosition, float>> pts;
                 if (auto arr = a["points"].getArray())
                     for (auto& pv : *arr)
                     {
                         te::TimePosition time = pv.hasProperty ("bar") ? session.barToTime ((double) pv["bar"])
                                                                         : te::TimePosition::fromSeconds ((double) pv["seconds"]);
                         float v = (float) (double) pv["value"];
                         if (isVolume)               v = te::decibelsToVolumeFaderPosition (v);
                         else if (target != "pan")   v = juce::jmap (juce::jlimit (0.0f, 1.0f, v), param->getValueRange().getStart(), param->getValueRange().getEnd());
                         pts.add ({ time, v });
                     }
                 if (pts.isEmpty()) return ToolResult::error ("No points");

                 if (flag (a, "clear_first"))
                     curve.removePointsInRegion ({ pts.getFirst().first, pts.getLast().first + 0.001s }, um());

                 for (auto& [time, v] : pts)
                     curve.addPoint (time, v, 0.0f, um());

                 return ToolResult::ok ("Wrote " + juce::String (pts.size()) + " points on " + param->getParameterName()
                                        + ". Curve now has " + juce::String (curve.getNumPoints()) + " points.");
             });

        add ("clear_automation", "Remove all automation from a track's volume/pan or a plugin parameter.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"target":{"type":"string"},"slot":{"type":["string","integer"]},"param":{"type":["string","integer"]}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto target = str (a, "target", has (a, "param") ? "plugin" : "volume");
                 te::AutomatableParameter* param = nullptr;
                 if (target == "volume" || target == "pan")
                 {
                     auto t = trackArg (a, err);
                     if (t == nullptr) return ToolResult::error (err);
                     param = target == "volume" ? t->getVolumePlugin()->volParam.get() : t->getVolumePlugin()->panParam.get();
                 }
                 else if (auto p = insertArg (a, err))
                     param = findParam (*p, a["param"]);
                 if (param == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "Parameter not found");
                 param->getCurve().clear (um());
                 return ToolResult::ok ("Cleared automation on " + param->getParameterName());
             });

        add ("bounce",
             "Bounce (export) the mix to an audio file - like Pro Tools Bounce Mix, offline. Defaults: whole session, WAV 24-bit with dither at the SESSION sample rate, master inserts included, into the Bounces folder. Optional: start/end in bars or seconds (or use_selection), tail_seconds for reverb tails, tracks (array) to bounce only some tracks, format wav|aiff|flac, bit_depth 16|24|32 (32 = float), sample_rate, dither, master_inserts, normalize_lufs (e.g. -14 for streaming; true peak held under -1 dBTP).",
             R"({"type":"object","properties":{"name":{"type":"string"},"path":{"type":"string"},"start_bar":{"type":"number"},"end_bar":{"type":"number"},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"},"use_selection":{"type":"boolean"},"tail_seconds":{"type":"number"},"format":{"type":"string","enum":["wav","aiff","flac"]},"bit_depth":{"type":"integer","enum":[16,24,32]},"sample_rate":{"type":"number"},"dither":{"type":"boolean"},"master_inserts":{"type":"boolean"},"normalize_lufs":{"type":"number"},"tracks":{"type":"array","items":{"type":["string","integer"]}}}})",
             [this] (const juce::var& a)
             {
                 auto& e = edit();
                 BounceExport::Options o;
                 o.format = str (a, "format", "wav").toLowerCase();
                 o.bitDepth = (int) num (a, "bit_depth", 24);
                 o.sampleRate = num (a, "sample_rate", 0.0);
                 o.dither = flag (a, "dither", true);
                 o.masterInserts = flag (a, "master_inserts", true);
                 o.tailSeconds = num (a, "tail_seconds", 0.0);
                 if (has (a, "normalize_lufs")) o.lufs = (float) num (a, "normalize_lufs");
                 if (flag (a, "use_selection", false) && ui.state != nullptr && ui.state->timeSelection) o.range = *ui.state->timeSelection;
                 else if (has (a, "start_bar") || has (a, "end_bar") || has (a, "start_seconds") || has (a, "end_seconds"))
                     o.range = te::TimeRange (timeArg (a, "start").value_or (0s), timeArg (a, "end").value_or (te::toPosition (e.getLength())));
                 if (o.range && o.range->getLength() <= 0.001s) return ToolResult::error ("Nothing to bounce (range invalid).");
                 const auto ext = "." + (o.format == "aiff" ? juce::String ("aif") : o.format);
                 juce::File dest = has (a, "path") ? juce::File (str (a, "path"))
                                                   : session.bouncesDir.getNonexistentChildFile (juce::File::createLegalFileName (str (a, "name", session.getFile().getFileNameWithoutExtension() + " mix")), ext, false);
                 juce::String err;
                 juce::File f;
                 if (auto arr = a["tracks"].getArray())
                 {
                     // only some tracks: same render path, limited to those tracks (and the folders they route through)
                     if (! session.savingAllowed()) return ToolResult::error ("Bouncing is switched off. Tell the user to check (Help > Buy Squelchmoth) or enter their key (Help > Enter Licence Key).");
                     e.getTransport().stop (false, false);
                     juce::Array<te::Track*> ts;
                     for (auto& tv : *arr) if (auto t = session.findRow (tv)) for (auto x : BounceExport::renderSetFor (session, *t, false)) ts.addIfNotAlreadyThere (x);
                     if (ts.isEmpty()) return ToolResult::error ("None of those tracks exist.");
                     auto p = BounceExport::params (session, o, dest);
                     p.tracksToDo = te::toBitSet (ts);
                     f = te::Renderer::renderToFile ("Bounce", p);
                     if (! f.existsAsFile()) err = "Bounce failed - is there any audio on those tracks in that range?";
                 }
                 else f = BounceExport::bounceMix (session, o, dest, err);
                 if (! f.existsAsFile()) return ToolResult::error (err.isNotEmpty() ? err : "Bounce failed.");
                 if (err.isNotEmpty()) return ToolResult::error (err);
                 const double sr = o.sampleRate > 0 ? o.sampleRate : TimeFmt::sampleRate (e);
                 return ToolResult::ok ("Bounced to " + f.getFullPathName() + " (" + juce::String (f.getSize() / 1024) + " KB, "
                                        + juce::String (sr / 1000.0, 1) + " kHz " + (o.bitDepth == 32 ? juce::String ("32-bit float") : juce::String (o.bitDepth) + "-bit")
                                        + (o.dither && o.bitDepth < 32 ? " dithered" : "") + ")");
             });

        add ("analyze_mix", "Offline-render a range and report peak and average level (dBFS) of the whole mix or chosen tracks - use it to check gain staging before a bounce.",
             R"({"type":"object","properties":{"start_bar":{"type":"number"},"end_bar":{"type":"number"},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"},"tracks":{"type":"array","items":{"type":["string","integer"]}}}})",
             [this] (const juce::var& a)
             {
                 auto& e = edit();
                 auto start = timeArg (a, "start").value_or (0s);
                 auto end = timeArg (a, "end").value_or (te::toPosition (e.getLength()));
                 if (end <= start) return ToolResult::error ("Empty range");
                 juce::Array<te::Track*> ts;
                 if (auto arr = a["tracks"].getArray()) { for (auto& tv : *arr) if (auto t = session.findTrack (tv)) ts.add (t); }
                 else ts = te::getAllTracks (e);
                 auto stats = te::Renderer::measureStatistics ("Analyse", e, { start, end }, te::toBitSet (ts), 512);
                 auto o = obj();
                 set (o, "peak_dbfs", juce::Decibels::gainToDecibels (stats.peak));
                 set (o, "average_dbfs", juce::Decibels::gainToDecibels (stats.average));
                 set (o, "audio_seconds", stats.audioDuration);
                 set (o, "clipping", stats.peak >= 1.0f);
                 return ToolResult::ok (o);
             });

        add ("get_meters", "Current live peak levels (dBFS) for every track and the master - useful while the session is playing.",
             R"({"type":"object","properties":{}})",
             [this] (const juce::var&)
             {
                 if (meterEdit != session.edit.get()) { meterClients.clear(); meterEdit = session.edit.get(); }
                 juce::Array<juce::var> out;
                 for (auto t : session.getTracks())
                 {
                     auto o = obj();
                     set (o, "track", t->getName());
                     auto it = meterClients.find (t->itemID);
                     if (auto lm = t->getLevelMeterPlugin())
                     {
                         if (it == meterClients.end())
                         {
                             auto c = std::make_unique<te::LevelMeasurer::Client>();
                             lm->measurer.addClient (*c);
                             it = meterClients.emplace (t->itemID, std::move (c)).first;
                         }
                         auto l = it->second->getAndClearAudioLevel (0), r = it->second->getAndClearAudioLevel (1);
                         set (o, "peak_db", juce::jmax (l.dB, r.dB));
                     }
                     out.add (o);
                 }
                 auto res = obj();
                 set (res, "tracks", out);
                 set (res, "note", "Levels are peaks since the previous get_meters call; call twice while playing for a fresh reading.");
                 return ToolResult::ok (res);
             });
    }

    //==============================================================================
    void registerBeatTools()
    {
        add ("add_drum_track",
             "Create a beat-making drum track (a step sequencer): a Sampler loaded with one of the 27 built-in Squelchmoth sound kits plus a Beat Maker pattern clip covering 'bars' bars. Kits: Mothwing 808, Hive 909, Chrome Trap, Velvet Lo-Fi, Dust Acoustic, Neon Trap, Drill Smoke, Afro Sun, Amapiano Log, Dembow Heat, Jersey Bounce, Phonk Cowbell, Boom Bap Crates, House Silk, Techno Steel, R&B Velvet, Rock Room, Memphis Tape, Plugg Pillow, Rage Red, Hyperpop Pink, Garage Swing, Grime Concrete, Baile Funk, Bossa Brush, Gospel Live, Neo-Soul Dust (a genre word like 'drill' or 'amapiano' also works). 12 channels: Kick, Snare, Rim, Clap, Closed Hat, Open Hat, Low Tom, High Tom, Ride, Crash, Perc, Sub 808. Then use beat_lab (starter / write) or set_beat.",
             R"({"type":"object","properties":{"name":{"type":"string"},"bars":{"type":"integer","minimum":1,"maximum":64},"kit":{"type":"string"}}})",
             [this] (const juce::var& a)
             {
                 int kit = DrumKit::kitIndexFor (str (a, "kit", "Mothwing 808"));
                 auto t = DrumKit::createDrumTrack (session, str (a, "name"), (int) num (a, "bars", 4), kit);
                 if (t == nullptr) return ToolResult::error ("Could not create drum track");
                 return ToolResult::ok ("Created drum track " + juce::String (session.indexOf (t)) + ": " + t->getName() + " with kit '" + DrumKit::kits[kit].name
                                        + "'. Channels: Kick, Snare, Rim, Clap, Closed Hat, Open Hat, Low Tom, High Tom, Ride, Crash, Perc, Sub 808. Now call set_beat.");
             });

        add ("set_drum_kit",
             "Swap the sound kit on a drum track (keeps the pattern) - any of the 27 kits (list=true lists them with descriptions).",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"kit":{"type":"string"},"list":{"type":"boolean"}}})",
             [this] (const juce::var& a)
             {
                 if (flag (a, "list") || ! has (a, "track"))
                 {
                     juce::String s;
                     for (int i = 0; i < DrumKit::numKits; ++i) s << DrumKit::kits[i].name << " - " << DrumKit::kits[i].description << "\n";
                     return ToolResult::ok (s);
                 }
                 juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
                 int kit = DrumKit::kitIndexFor (str (a, "kit"));
                 if (! DrumKit::loadKit (session, *t, kit)) return ToolResult::error ("That track has no Sampler - use add_drum_track first.");
                 return ToolResult::ok (t->getName() + " now uses kit '" + juce::String (DrumKit::kits[kit].name) + "'");
             });

        add ("set_beat",
             "Write a step pattern on a drum track. steps: object mapping channel name (kick, snare, rim, clap, closed hat, open hat, low tom, high tom, ride, crash, perc, sub) to a pattern string where x/X = hit (X = accent), . or - = rest. 16 characters = one 4/4 bar of 16ths; use 'steps_per_bar' for 8 or 32. Optional velocity (1-127), swing (0-1), replace (clear first, default true). Example: {kick:'x...x...x...x...', snare:'....x.......x...', 'closed hat':'x.x.x.x.x.x.x.x.'}",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"steps":{"type":"object"},"steps_per_bar":{"type":"integer","enum":[8,16,32]},"velocity":{"type":"integer"},"swing":{"type":"number"},"replace":{"type":"boolean"}},"required":["track","steps"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto clip = DrumKit::findStepClip (*t);
                 if (clip == nullptr) return ToolResult::error ("Track '" + t->getName() + "' has no step clip. Use add_drum_track first.");

                 int spb = (int) num (a, "steps_per_bar", 16);
                 auto p = BeatLab::pattern (*clip);
                 p.setNoteLength (te::BeatDuration::fromBeats (4.0 / spb));

                 // longest pattern decides the number of steps
                 int maxLen = 0;
                 if (auto o = a["steps"].getDynamicObject())
                     for (auto& kv : o->getProperties())
                         maxLen = juce::jmax (maxLen, kv.value.toString().removeCharacters (" |").length());
                 if (maxLen == 0) return ToolResult::error ("steps is empty");
                 int numSteps = ((maxLen + spb - 1) / spb) * spb;
                 p.setNumNotes (numSteps);
                 BeatLab::rebuildSequence (*clip);

                 if (flag (a, "replace", true)) p.clear();
                 int vel = juce::jlimit (1, 127, (int) num (a, "velocity", 100));
                 double swing = juce::jlimit (0.0, 1.0, num (a, "swing", 0.0));
                 juce::StringArray unknown, done;

                 if (auto o = a["steps"].getDynamicObject())
                     for (auto& kv : o->getProperties())
                     {
                         int ch = DrumKit::channelIndexFor (*clip, kv.name.toString());
                         if (ch < 0) { unknown.add (kv.name.toString()); continue; }
                         auto pat = kv.value.toString().removeCharacters (" |");
                         for (int i = 0; i < numSteps; ++i)
                         {
                             auto c = i < pat.length() ? pat[i] : '.';
                             bool hit = c == 'x' || c == 'X' || c == '1' || c == '*';
                             p.setNote (ch, i, hit);
                             if (hit) p.setVelocity (ch, i, c == 'X' ? 127 : vel);
                             if (hit) { p.setGate (ch, i, 1.0); p.setProbability (ch, i, 1.0f); }
                         }
                         done.add (clip->getChannels()[ch]->name.get());
                     }

                 if (has (a, "swing")) BeatLab::setSwing (*clip, (float) swing);

                 auto msg = "Pattern written (" + juce::String (numSteps) + " steps) on " + done.joinIntoString (", ");
                 if (! unknown.isEmpty()) msg << ". Unknown channels: " << unknown.joinIntoString (", ") << " (use kick, snare, closed hat, open hat, clap, low tom, high tom, ride)";
                 return ToolResult::ok (msg);
             });

        add ("get_beat", "Read the step pattern of a drum track as pattern strings.",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]}},"required":["track"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto clip = DrumKit::findStepClip (*t);
                 if (clip == nullptr) return ToolResult::error ("No step clip on this track");
                 auto p = BeatLab::pattern (*clip);
                 auto o = obj();
                 set (o, "steps", p.getNumNotes());
                 set (o, "step_length_beats", p.getNoteLength().inBeats());
                 auto chans = clip->getChannels();
                 for (int ch = 0; ch < chans.size(); ++ch)
                 {
                     juce::String row;
                     for (int i = 0; i < p.getNumNotes(); ++i)
                         row << (p.getNote (ch, i) ? (p.getVelocity (ch, i) > 110 ? "X" : "x") : ".");
                     set (o, chans[ch]->name.get(), row);
                 }
                 return ToolResult::ok (o);
             });

        add ("load_drum_sound", "Replace one drum channel's sound with an audio file from disk (your own samples).",
             R"({"type":"object","properties":{"track":{"type":["string","integer"]},"channel":{"type":"string"},"path":{"type":"string"},"name":{"type":"string"}},"required":["track","channel","path"]})",
             [this] (const juce::var& a)
             {
                 juce::String err;
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto clip = DrumKit::findStepClip (*t);
                 auto sampler = t->pluginList.findFirstPluginOfType<te::SamplerPlugin>();
                 if (clip == nullptr || sampler == nullptr) return ToolResult::error ("Not a drum track");
                 int ch = DrumKit::channelIndexFor (*clip, str (a, "channel"));
                 if (ch < 0 || ch >= sampler->getNumSounds()) return ToolResult::error ("Unknown channel");
                 juce::File f (str (a, "path"));
                 if (! f.existsAsFile()) return ToolResult::error ("File not found: " + f.getFullPathName());
                 sampler->setSoundMedia (ch, f.getFullPathName());
                 clip->getChannels()[ch]->name = str (a, "name", f.getFileNameWithoutExtension());
                 return ToolResult::ok ("Channel " + juce::String (ch + 1) + " now plays " + f.getFileName());
             });
    }

    std::map<te::EditItemID, std::unique_ptr<te::LevelMeasurer::Client>> meterClients;
    te::Edit* meterEdit = nullptr;

    //==============================================================================
    void registerViewTools()
    {
        add ("screenshot", "Take a picture of the Squelchmoth window so you can see the session. view: 'edit' (timeline), 'mix' (mixer), 'current' (whatever is showing).",
             R"({"type":"object","properties":{"view":{"type":"string","enum":["current","edit","mix","claude"]}}})",
             [this] (const juce::var& a)
             {
                 if (! ui.snapshot) return ToolResult::error ("UI not available");
                 auto img = ui.snapshot (str (a, "view", "current"));
                 if (! img.isValid()) return ToolResult::error ("Snapshot failed");

                 if (img.getWidth() > 1600)
                     img = img.rescaled (1600, img.getHeight() * 1600 / img.getWidth(), juce::Graphics::highResamplingQuality);

                 juce::MemoryOutputStream mo;
                 juce::PNGImageFormat png;
                 png.writeImageToStream (img, mo);
                 ToolResult r;
                 r.text = "Screenshot " + juce::String (img.getWidth()) + "x" + juce::String (img.getHeight());
                 r.imageBase64 = juce::Base64::toBase64 (mo.getData(), mo.getDataSize());
                 r.imageMime = "image/png";
                 return r;
             });

        add ("show_view", "Switch the main window between the Edit window (timeline), Mix window (mixer) and Claude panel; optionally zoom the timeline to show N bars starting at a bar.",
             R"({"type":"object","properties":{"view":{"type":"string","enum":["edit","mix","claude"]},"bars_visible":{"type":"number"},"first_bar":{"type":"number"}}})",
             [this] (const juce::var& a)
             {
                 if (has (a, "view") && ui.showView) ui.showView (str (a, "view"));
                 if (has (a, "bars_visible") && ui.zoomTimeline) ui.zoomTimeline (num (a, "bars_visible"), num (a, "first_bar", 1));
                 return ToolResult::ok ("OK");
             });
    }
};
