// Squelchmoth - Claude tools for Pro Tools tracks and I/O. Included inside the McpTools class body.
// (new_tracks, track_options, set_input, set_output, io_setup, edit_window_views, master_fader)

/** Finds an interface input path by its name ("In 1", "In 3-4", "Vocal Mic") - stereo pairs and mono paths. */
std::optional<std::pair<int, int>> findInputPath (const juce::String& want)
{
    auto& eng = session.getEngine();
    int n = IO::numInterfaceChannels (eng, true);
    auto w = want.trim();
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int ch = 0; ch + 1 < n; ch += 2)
        {
            auto nm = IO::pathName (true, ch, 2);
            if (pass == 0 ? nm.equalsIgnoreCase (w) || IO::defaultPathName (true, ch, 2).equalsIgnoreCase (w) : nm.containsIgnoreCase (w)) return std::make_pair (ch, 2);
        }
        for (int ch = 0; ch < n; ++ch)
        {
            auto nm = IO::pathName (true, ch, 1);
            if (pass == 0 ? nm.equalsIgnoreCase (w) || IO::defaultPathName (true, ch, 1).equalsIgnoreCase (w) : nm.containsIgnoreCase (w)) return std::make_pair (ch, 1);
        }
    }
    return std::nullopt;
}

int findBusByName (const juce::String& want)
{
    auto w = want.trim();
    for (auto b : IO::allBusses (session)) if (IO::busName (session, b).equalsIgnoreCase (w)) return b;
    for (auto b : IO::allBusses (session)) if (IO::busName (session, b).containsIgnoreCase (w)) return b;
    if (w.startsWithIgnoreCase ("bus ")) { auto n = w.substring (4).getIntValue(); if (n > 0) return n - 1; }
    return -1;
}

/** Shared by set_input and update_track: routes a named input path to a track. */
ToolResult routeInput (te::AudioTrack& t, const juce::String& want, bool forceLayout = false)
{
    auto w = want.trim();
    if (w.equalsIgnoreCase ("none") || w.equalsIgnoreCase ("no input")) { IO::setNoInput (session, t); return ToolResult::ok (t.getName() + " input: no input"); }
    if (IO::usesMidiInput (t))
    {
        for (auto in : edit().getAllInputDevices())
        {
            auto type = in->getInputDevice().getDeviceType();
            if (type != te::InputDevice::physicalMidiDevice && type != te::InputDevice::virtualMidiDevice) continue;
            auto nm = IO::midiName (in->getInputDevice());
            if (nm.equalsIgnoreCase (w) || nm.containsIgnoreCase (w) || (w.equalsIgnoreCase ("all midi") && nm == "All"))
            {
                auto err = IO::setMidiInput (session, t, &in->getInputDevice());
                return err.isEmpty() ? ToolResult::ok (t.getName() + " MIDI input: " + nm) : ToolResult::error (err);
            }
        }
        return ToolResult::error ("No MIDI input named '" + w + "'. Call list_inputs.");
    }
    if (auto path = findInputPath (w))
    {
        auto [first, count] = *path;
        auto conflicts = IO::layoutConflicts (session, t, first, count);
        if (! conflicts.isEmpty() && ! forceLayout)
            return ToolResult::error ("Using " + IO::pathName (true, first, count) + " needs the interface channels re-laid out (" + (count == 2 ? "joined into a stereo pair" : "split into mono")
                                      + "), which would also change: " + conflicts.joinIntoString (", ") + ". Ask the user, then call again with force=true - or pick a free input (list_inputs).");
        IO::setInterfaceInput (session, t, first, count, [this] (juce::String err) { if (err.isNotEmpty()) session.log.add ("Squelchmoth", err, false); });
        return ToolResult::ok (t.getName() + " input: " + IO::pathName (true, first, count) + (IO::findWaveIn (session.getEngine(), first, count) == nullptr ? " (the interface pair is being re-laid out; ready in a moment)" : ""));
    }
    auto b = findBusByName (w);
    if (b >= 0)
    {
        auto err = IO::setBusInput (session, t, b);
        return err.isEmpty() ? ToolResult::ok (t.getName() + " input: " + IO::busName (session, b)) : ToolResult::error (err);
    }
    return ToolResult::error ("No input path named '" + w + "'. Call list_inputs for the names (e.g. 'In 1', 'In 1-2', a bus name, 'no input').");
}

/** Shared by set_output and update_track. */
ToolResult routeOutput (te::AudioTrack& t, const juce::var& v)
{
    auto w = v.toString().trim();
    auto& dm = session.getEngine().getDeviceManager();
    if (w.equalsIgnoreCase ("none") || w.equalsIgnoreCase ("no output")) { t.getOutput().setOutputToNone(); return ToolResult::ok (t.getName() + " output: no output"); }
    if (w.equalsIgnoreCase ("master") || w.equalsIgnoreCase ("main") || w.isEmpty()) { t.getOutput().setOutputToDefaultDevice (false); return ToolResult::ok (t.getName() + " output: " + IO::outputLabel (session, t) + " (main)"); }
    for (auto od : dm.getWaveOutputDevices())
        if (od != nullptr && (IO::nameOf (*od).equalsIgnoreCase (w) || IO::defaultPathName (false, IO::rangeOf (od->getChannels()).first, IO::rangeOf (od->getChannels()).second).equalsIgnoreCase (w)))
        {
            if (dm.getDefaultWaveOutDevice() == od) t.getOutput().setOutputToDefaultDevice (false);
            else t.getOutput().setOutputToDeviceID (od->getDeviceID());
            return ToolResult::ok (t.getName() + " output: " + IO::nameOf (*od));
        }
    for (int i = 0; i < dm.getNumMidiOutDevices(); ++i)
        if (auto md = dm.getMidiOutDevice (i)) if (md->getAliasOrName().equalsIgnoreCase (w)) { t.getOutput().setOutputToDeviceID (md->getDeviceID()); return ToolResult::ok (t.getName() + " output: " + md->getAliasOrName()); }
    auto b = findBusByName (w);
    if (b >= 0)
    {
        auto err = IO::setOutputToBus (session, t, b);
        return err.isEmpty() ? ToolResult::ok (t.getName() + " output: " + IO::busName (session, b)) : ToolResult::error (err);
    }
    if (auto dest = session.findTrack (v); dest != nullptr && dest != &t)
    {
        if (IO::wouldFeedback (t, *dest)) return ToolResult::error ("That would feed back into itself.");
        t.getOutput().setOutputToTrack (dest);
        return ToolResult::ok (t.getName() + " output: " + dest->getName());
    }
    return ToolResult::error ("No output named '" + w + "'. Use an output path ('Out 1-2'), a bus name, a track name, 'master' or 'none'.");
}

void registerTrackTools2()
{
    //==========================================================================
    add ("new_tracks", "Pro Tools Track > New: create tracks of any type. type: audio | aux | master_fader | vca | midi | instrument | basic_folder | routing_folder. format: mono | stereo (audio, aux, instrument, master_fader and routing_folder; mono tracks are summed to one channel like Pro Tools). timebase: samples | ticks. instrument (for instrument tracks): mothsynth | sampler | none. preset: the name of a saved Track Preset to create from instead of a type. Audio tracks get the next interface input (In 1, In 2... or In 1-2 for stereo).",
         R"json({"type":"object","properties":{"type":{"type":"string","enum":["audio","aux","master_fader","vca","midi","instrument","basic_folder","routing_folder"]},"count":{"type":"integer","minimum":1,"maximum":64},"format":{"type":"string","enum":["mono","stereo"]},"timebase":{"type":"string","enum":["samples","ticks"]},"name":{"type":"string"},"instrument":{"type":"string"},"preset":{"type":"string"},"after_track":{"type":["string","integer"]}}})json",
         [this] (const juce::var& a)
         {
             TrackOps::NewSpec sp;
             const juce::StringArray types { "audio", "aux", "master_fader", "vca", "midi", "instrument", "basic_folder", "routing_folder" };
             auto ti = types.indexOf (str (a, "type", "audio"));
             if (ti < 0) return ToolResult::error ("Unknown type");
             if (has (a, "preset"))
             {
                 for (auto& f : TrackOps::trackPresetFiles (session))
                     if (f.getFileNameWithoutExtension().equalsIgnoreCase (str (a, "preset"))) sp.preset = f;
                 if (! sp.preset.existsAsFile()) return ToolResult::error ("No track preset named '" + str (a, "preset") + "'. Use track_preset with action list.");
             }
             sp.type = (TrackOps::NewType) ti;
             sp.count = (int) num (a, "count", 1);
             sp.stereo = str (a, "format", sp.type == TrackOps::NewType::audio ? "mono" : "stereo") == "stereo";
             sp.ticks = str (a, "timebase", (sp.type == TrackOps::NewType::midi || sp.type == TrackOps::NewType::instrument) ? "ticks" : "samples") == "ticks";
             sp.name = str (a, "name");
             auto inst = str (a, "instrument", "mothsynth").toLowerCase();
             sp.instrument = inst == "none" ? juce::String() : inst.contains ("sampl") ? juce::String ("sampler") : juce::String ("4osc");
             te::Track* after = has (a, "after_track") ? session.findRow (a["after_track"]) : nullptr;
             juce::String note;
             auto made = TrackOps::createTracks (session, sp, after, note);
             juce::StringArray names;
             for (auto t : made) names.add (t->isMasterTrack() ? TrackOps::masterName (session) : t->getName());
             if (made.isEmpty()) return ToolResult::error (note.isNotEmpty() ? note : "No tracks were created");
             if (ui.refresh) ui.refresh();
             return ToolResult::ok ("Created " + names.joinIntoString (", ") + (note.isNotEmpty() ? ". " + note : juce::String()));
         });

    add ("track_options", "Pro Tools track commands on one or more tracks: inactive (true/false), hidden, record_safe, solo_safe, freeze (true/false), commit (renders to a new track; source hidden+inactive), bounce (track to a file), split_mono, timebase (samples|ticks), comments (text), view (Track View: waveform, volume, pan, pan left/right, a send level or 'Plugin > Parameter'), input_monitor (this track's I button), duplicate (number of copies, with copy_clips/copy_automation/copy_inserts/copy_sends/copy_groups), delete.",
         R"json({"type":"object","properties":{"tracks":{"type":"array","items":{"type":["string","integer"]}},"track":{"type":["string","integer"]},"inactive":{"type":"boolean"},"hidden":{"type":"boolean"},"record_safe":{"type":"boolean"},"solo_safe":{"type":"boolean"},"freeze":{"type":"boolean"},"commit":{"type":"boolean"},"bounce":{"type":"boolean"},"split_mono":{"type":"boolean"},"timebase":{"type":"string","enum":["samples","ticks"]},"comments":{"type":"string"},"view":{"type":"string"},"input_monitor":{"type":"boolean"},"duplicate":{"type":"integer","minimum":1,"maximum":32},"copy_clips":{"type":"boolean"},"copy_automation":{"type":"boolean"},"copy_inserts":{"type":"boolean"},"copy_sends":{"type":"boolean"},"copy_groups":{"type":"boolean"},"delete":{"type":"boolean"}}})json",
         [this] (const juce::var& a)
         {
             juce::Array<te::Track*> ts;
             if (auto arr = a["tracks"].getArray()) { for (auto& v : *arr) if (auto t = session.findRow (v)) ts.add (t); }
             if (has (a, "track")) if (auto t = session.findRow (a["track"])) ts.addIfNotAlreadyThere (t);
             if (ts.isEmpty()) return ToolResult::error ("Name the track(s) with 'track' or 'tracks'.");
             juce::StringArray done, errs;
             for (auto t : ts)
             {
                 auto at = dynamic_cast<te::AudioTrack*> (t);
                 if (has (a, "inactive"))    { TrackOps::setInactive (*t, flag (a, "inactive")); done.add (t->getName() + (flag (a, "inactive") ? " inactive" : " active")); }
                 if (has (a, "hidden"))      { t->setHidden (flag (a, "hidden")); done.add (t->getName() + (flag (a, "hidden") ? " hidden" : " shown")); }
                 if (has (a, "record_safe")) { TrackOps::setRecordSafe (session, *t, flag (a, "record_safe")); done.add (t->getName() + " record safe " + (flag (a, "record_safe") ? "on" : "off")); }
                 if (has (a, "solo_safe"))   { TrackOps::setSoloSafe (*t, flag (a, "solo_safe")); done.add (t->getName() + " solo safe " + (flag (a, "solo_safe") ? "on" : "off")); }
                 if (has (a, "comments"))    { TrackOps::setComments (*t, str (a, "comments")); done.add (t->getName() + " comments set"); }
                 if (has (a, "view"))
                 {
                     auto want = str (a, "view").trim();
                     juce::String found;
                     for (auto& c : MixCtl::viewChoices (session, *t))
                         if (c.id.equalsIgnoreCase (want) || c.label.equalsIgnoreCase (want)) { found = c.id; break; }
                     if (found.isEmpty())
                         for (auto& c : MixCtl::viewChoices (session, *t))
                             if (c.label.containsIgnoreCase (want)) { found = c.id; break; }
                     if (found.isEmpty()) errs.add (t->getName() + ": no track view '" + want + "'");
                     else { MixCtl::setView (session, *t, found); done.add (t->getName() + " shows " + MixCtl::viewLabel (session, *t)); }
                 }
                 if (has (a, "input_monitor") && at != nullptr) { TrackOps::setInputMonitor (session, *at, flag (a, "input_monitor")); done.add (t->getName() + " input monitor " + (flag (a, "input_monitor") ? "on" : "off")); }
                 if (at != nullptr)
                 {
                     if (has (a, "timebase")) { TrackOps::setTimebase (*at, str (a, "timebase") == "ticks"); done.add (t->getName() + " timebase " + str (a, "timebase")); }
                     if (has (a, "freeze"))   { auto e = TrackOps::freeze (*at, flag (a, "freeze")); if (e.isNotEmpty()) errs.add (e); else done.add (t->getName() + (flag (a, "freeze") ? " frozen" : " unfrozen")); }
                     if (flag (a, "bounce"))  { juce::String e; auto f = TrackOps::bounceTrack (session, *at, e); if (f.existsAsFile()) done.add (t->getName() + " bounced to " + f.getFullPathName()); else errs.add (e); }
                     if (flag (a, "split_mono")) { juce::String e; auto made = TrackOps::splitIntoMono (session, *at, e); if (e.isNotEmpty()) errs.add (e); else done.add (t->getName() + " split into " + juce::String (made.size()) + " mono tracks"); }
                     if (flag (a, "commit"))  { juce::String e; auto nt = TrackOps::commit (session, *at, e); if (nt) done.add (t->getName() + " committed to " + nt->getName()); else errs.add (e); }
                 }
                 if (has (a, "duplicate"))
                 {
                     TrackOps::DupOptions o;
                     o.copies = (int) num (a, "duplicate", 1);
                     o.clips = flag (a, "copy_clips", true); o.automation = flag (a, "copy_automation", true); o.inserts = flag (a, "copy_inserts", true);
                     o.sends = flag (a, "copy_sends", true); o.groups = flag (a, "copy_groups", true);
                     auto made = TrackOps::duplicate (session, *t, o);
                     done.add (t->getName() + " duplicated x" + juce::String (made.size()));
                 }
                 if (flag (a, "delete")) { if (ui.state) ui.state->dropReferencesTo (t); done.add ("deleted " + t->getName()); edit().deleteTrack (t); }
             }
             if (ui.refresh) ui.refresh();
             if (done.isEmpty() && ! errs.isEmpty()) return ToolResult::error (errs.joinIntoString ("; "));
             return ToolResult::ok (done.joinIntoString ("; ") + (errs.isEmpty() ? juce::String() : ". Problems: " + errs.joinIntoString ("; ")));
         });

    add ("set_input", "Set a track's input path exactly like the Pro Tools Input selector: an interface path ('In 1', 'In 1-2' or its renamed name), a bus name (Aux tracks listen to it; audio tracks record it), a MIDI device / 'All' for instrument and MIDI tracks, or 'no input'.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"input":{"type":"string"},"force":{"type":"boolean","description":"allow splitting/joining interface channels even if other tracks' inputs change"}},"required":["track","input"]})json",
         [this] (const juce::var& a)
         {
             auto t = session.findTrack (a["track"]);
             if (t == nullptr) return ToolResult::error ("No such track");
             return routeInput (*t, str (a, "input"), flag (a, "force", false));
         });

    add ("set_output", "Set a track's output path like the Pro Tools Output selector: an interface output ('Out 1-2' = main), a bus name (feeds that Aux track), another track's name, a MIDI device (MIDI tracks), 'master' or 'none'. Use target 'master' with output to choose the Master Fader's output path.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"output":{"type":["string","integer"]},"new_aux":{"type":"string","description":"create a new stereo Aux Input track with this name and route the output to it"}},"required":["track"]})json",
         [this] (const juce::var& a)
         {
             if (str (a, "track").equalsIgnoreCase ("master"))
             {
                 auto& dm = session.getEngine().getDeviceManager();
                 for (auto od : dm.getWaveOutputDevices())
                     if (od != nullptr && IO::nameOf (*od).equalsIgnoreCase (str (a, "output"))) { dm.setDefaultWaveOutDevice (od->getDeviceID()); return ToolResult::ok ("Master Fader output: " + IO::nameOf (*od)); }
                 return ToolResult::error ("No output path named '" + str (a, "output") + "'. Call io_setup with action 'list'.");
             }
             auto t = session.findTrack (a["track"]);
             if (t == nullptr) return ToolResult::error ("No such track");
             if (has (a, "new_aux"))
             {
                 auto aux = session.addTrack ("bus", str (a, "new_aux"));
                 if (aux == nullptr) return ToolResult::error ("Could not create the Aux track");
                 aux->state.setProperty ("sqmKind", "aux", nullptr); aux->state.setProperty ("sqmFormat", "stereo", nullptr);
                 t->getOutput().setOutputToTrack (aux);
                 return ToolResult::ok (t->getName() + " output: new Aux track " + aux->getName() + " (bus " + IO::busName (session, session.getBusNumberOfTrack (*aux)) + ")");
             }
             return routeOutput (*t, a["output"]);
         });

    add ("io_setup", "Pro Tools I/O Setup. action: list (every input/output path, bus and MIDI device with its name, format and users) | rename_input / rename_output (channels like '1-2' or '3', name) | rename_bus (bus name or number, name) | input_format (channels, format mono|stereo) | all_inputs (format mono|stereo) | all_outputs (format) | default_names | new_bus (name).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","rename_input","rename_output","rename_bus","input_format","all_inputs","all_outputs","default_names","new_bus"]},"channels":{"type":"string"},"bus":{"type":["string","integer"]},"name":{"type":"string"},"format":{"type":"string","enum":["mono","stereo"]}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto& eng = session.getEngine();
             auto& dm = eng.getDeviceManager();
             auto action = str (a, "action");
             auto parseChannels = [&] (const juce::String& c) -> std::pair<int, int>
             {
                 auto parts = juce::StringArray::fromTokens (c.retainCharacters ("0123456789-"), "-", "");
                 int first = juce::jmax (1, parts[0].getIntValue()) - 1;
                 int last = parts.size() > 1 ? juce::jmax (first + 1, parts[1].getIntValue()) - 1 : first;
                 return { first, last - first + 1 };
             };
             if (action == "list")
             {
                 auto o = obj();
                 juce::Array<juce::var> ins, outs, busses, midi;
                 for (auto d : dm.getWaveInputDevices())
                     if (d && ! d->isTrackDevice())
                     {
                         auto r = IO::rangeOf (d->getChannels());
                         auto io = obj();
                         set (io, "name", IO::nameOf (*d)); set (io, "channels", IO::rangeText (r.first, r.second)); set (io, "format", r.second >= 2 ? "stereo" : "mono"); set (io, "enabled", d->isEnabled());
                         juce::StringArray users;
                         if (auto inst = IO::instanceFor (edit(), *d)) for (auto id : inst->getTargets()) if (auto tt = te::findTrackForID (edit(), id)) users.add (tt->getName());
                         set (io, "used_by", users.joinIntoString (", "));
                         ins.add (io);
                     }
                 for (auto d : dm.getWaveOutputDevices())
                     if (d)
                     {
                         auto r = IO::rangeOf (d->getChannels());
                         auto io = obj();
                         set (io, "name", IO::nameOf (*d)); set (io, "channels", IO::rangeText (r.first, r.second)); set (io, "format", r.second >= 2 ? "stereo" : "mono");
                         set (io, "main", dm.getDefaultWaveOutDevice() == d);
                         outs.add (io);
                     }
                 for (auto b : IO::allBusses (session))
                 {
                     auto bo = obj();
                     set (bo, "bus", b + 1); set (bo, "name", IO::busName (session, b));
                     if (auto aux = session.findBusTrack (b)) set (bo, "aux_track", aux->getName());
                     busses.add (bo);
                 }
                 for (auto& md : dm.getMidiInDevices()) if (md) { auto mo = obj(); set (mo, "name", IO::midiName (*md)); set (mo, "direction", "in"); set (mo, "enabled", md->isEnabled()); midi.add (mo); }
                 for (int i = 0; i < dm.getNumMidiOutDevices(); ++i) if (auto md = dm.getMidiOutDevice (i)) { auto mo = obj(); set (mo, "name", md->getAliasOrName()); set (mo, "direction", "out"); set (mo, "enabled", md->isEnabled()); midi.add (mo); }
                 set (o, "inputs", ins); set (o, "outputs", outs); set (o, "busses", busses); set (o, "midi", midi);
                 set (o, "master_fader_output", IO::masterOutputLabel (session));
                 return ToolResult::ok (o);
             }
             if (action == "rename_input" || action == "rename_output")
             {
                 auto [first, count] = parseChannels (str (a, "channels"));
                 IO::setPathName (eng, action == "rename_input", first, count, str (a, "name"));
                 return ToolResult::ok ((action == "rename_input" ? "In " : "Out ") + IO::rangeText (first, count) + " is now named " + IO::pathName (action == "rename_input", first, count));
             }
             if (action == "rename_bus")
             {
                 int b = a["bus"].isInt() ? (int) a["bus"] - 1 : findBusByName (str (a, "bus"));
                 if (b < 0) return ToolResult::error ("No such bus");
                 IO::setBusName (session, b, str (a, "name"));
                 return ToolResult::ok ("Bus " + juce::String (b + 1) + " is now named " + IO::busName (session, b));
             }
             if (action == "input_format")
             {
                 auto [first, count] = parseChannels (str (a, "channels"));
                 juce::ignoreUnused (count);
                 IO::setInputPathFormat (session, first, str (a, "format") == "stereo" ? 2 : 1);
                 return ToolResult::ok ("Input " + IO::rangeText (first, 1) + " set to " + str (a, "format") + " (tracks on it follow)");
             }
             if (action == "all_inputs" || action == "all_outputs")
             {
                 IO::relayoutAll (session, action == "all_inputs", str (a, "format") == "stereo" ? 2 : 1);
                 return ToolResult::ok (juce::String (action == "all_inputs" ? "Inputs" : "Outputs") + " set to " + (str (a, "format") == "stereo" ? "stereo pairs" : "mono paths"));
             }
             if (action == "default_names")
             {
                 IO::resetAllNames (eng);
                 for (auto b : IO::allBusses (session)) IO::setBusName (session, b, {});
                 return ToolResult::ok ("All paths use their default names (In 1-2, Out 1-2, busses named after their Aux tracks)");
             }
             if (action == "new_bus")
             {
                 auto aux = session.addTrack ("bus", str (a, "name"));
                 if (aux == nullptr) return ToolResult::error ("Could not create the bus");
                 aux->state.setProperty ("sqmKind", "aux", nullptr); aux->state.setProperty ("sqmFormat", "stereo", nullptr);
                 return ToolResult::ok ("New bus " + IO::busName (session, session.getBusNumberOfTrack (*aux)) + " with Aux Input track " + aux->getName());
             }
             return ToolResult::error ("Unknown action");
         });

    add ("edit_window_views", "Show or hide the Edit window's optional columns (View > Edit Window Views) and the Mix window's insert/send rows: io, inserts_ae, inserts_fj, sends_ae, sends_fj, comments (Edit window); mix_inserts, mix_sends (show the rows in the Mix window), mix_inserts_fj, mix_sends_fj (Mix window, 10 slots instead of 5).",
         R"json({"type":"object","properties":{"io":{"type":"boolean"},"inserts_ae":{"type":"boolean"},"inserts_fj":{"type":"boolean"},"sends_ae":{"type":"boolean"},"sends_fj":{"type":"boolean"},"comments":{"type":"boolean"},"mix_inserts":{"type":"boolean"},"mix_sends":{"type":"boolean"},"mix_inserts_fj":{"type":"boolean"},"mix_sends_fj":{"type":"boolean"}}})json",
         [this] (const juce::var& a)
         {
             const char* map[][2] = { { "io", "ev_io" }, { "inserts_ae", "ev_insAE" }, { "inserts_fj", "ev_insFJ" }, { "sends_ae", "ev_sendAE" }, { "sends_fj", "ev_sendFJ" }, { "comments", "ev_comments" },
                                      { "mix_inserts", "mx_showInserts" }, { "mix_sends", "mx_showSends" }, { "mix_inserts_fj", "mx_insertsFJ" }, { "mix_sends_fj", "mx_sendsFJ" } };
             juce::StringArray changed;
             for (auto& m : map) if (has (a, m[0])) { Prefs::set (m[1], flag (a, m[0])); changed.add (juce::String (m[0]) + (flag (a, m[0]) ? " on" : " off")); }
             return ToolResult::ok (changed.isEmpty() ? juce::String ("Nothing changed") : changed.joinIntoString (", "));
         });

    add ("master_fader", "The session's Master Fader: create or delete it (delete removes its inserts and resets it to 0 dB), rename it, set its volume_db, or pick its output path.",
         R"json({"type":"object","properties":{"exists":{"type":"boolean"},"name":{"type":"string"},"volume_db":{"type":"number"},"output":{"type":"string"}}})json",
         [this] (const juce::var& a)
         {
             if (has (a, "exists")) TrackOps::setMasterFader (session, flag (a, "exists"));
             if (has (a, "name")) TrackOps::setMasterName (session, str (a, "name"));
             if (has (a, "volume_db")) if (auto mv = edit().getMasterVolumePlugin()) mv->setVolumeDb ((float) num (a, "volume_db"));
             if (has (a, "output"))
             {
                 auto& dm = session.getEngine().getDeviceManager();
                 bool ok = false;
                 for (auto od : dm.getWaveOutputDevices()) if (od != nullptr && IO::nameOf (*od).equalsIgnoreCase (str (a, "output"))) { dm.setDefaultWaveOutDevice (od->getDeviceID()); ok = true; }
                 if (! ok) return ToolResult::error ("No output path named '" + str (a, "output") + "'");
             }
             auto o = obj();
             set (o, "exists", TrackOps::hasMasterFader (session));
             set (o, "name", TrackOps::masterName (session));
             if (auto mv = edit().getMasterVolumePlugin()) set (o, "volume_db", juce::roundToInt (mv->getVolumeDb() * 10) / 10.0);
             set (o, "output", IO::masterOutputLabel (session));
             juce::Array<juce::var> ins;
             for (auto p : Session::getInserts (edit().getMasterPluginList(), nullptr)) ins.add (Signature::displayName (*p));
             set (o, "inserts", ins);
             if (ui.refresh) ui.refresh();
             return ToolResult::ok (o);
         });
}

void registerEngineTools()
{
    add ("playback_engine", "Setup > Playback Engine: show or change the audio interface - device_type (CoreAudio, ASIO, ALSA...), device (name), sample_rate, buffer_size (H/W Buffer Size), input_channels / output_channels (how many interface channels Squelchmoth opens) - and the Host Engine: host_processors (CPU cores for audio, 0 = all), cpu_usage_limit (40-99 %), ignore_errors, dynamic_plugin_processing, low_buffer_optimization, overload_protection (raise the buffer automatically on repeated dropouts), elastic_background, disk_cache_seconds (6/15/30/60).",
         R"json({"type":"object","properties":{"device_type":{"type":"string"},"device":{"type":"string"},"sample_rate":{"type":"number"},"buffer_size":{"type":"integer"},"input_channels":{"type":"integer","minimum":0,"maximum":256},"output_channels":{"type":"integer","minimum":0,"maximum":256},"host_processors":{"type":"integer","minimum":0,"maximum":128},"cpu_usage_limit":{"type":"integer","minimum":40,"maximum":99},"ignore_errors":{"type":"boolean"},"dynamic_plugin_processing":{"type":"boolean"},"low_buffer_optimization":{"type":"boolean"},"overload_protection":{"type":"boolean"},"elastic_background":{"type":"boolean"},"disk_cache_seconds":{"type":"integer","minimum":1,"maximum":60}}})json",
         [this] (const juce::var& a)
         {
             bool hostChanged = false, rebuild = false;
             if (has (a, "host_processors"))   { Prefs::set ("pe_hostProcessors", juce::jlimit (0, Prefs::numCpus(), (int) num (a, "host_processors"))); hostChanged = true; }
             if (has (a, "cpu_usage_limit"))   { Prefs::set ("pe_cpuLimit", juce::jlimit (40, 99, (int) num (a, "cpu_usage_limit"))); hostChanged = true; }
             if (has (a, "ignore_errors"))     { Prefs::set ("pe_ignoreErrors", flag (a, "ignore_errors")); hostChanged = true; }
             if (has (a, "dynamic_plugin_processing")) { Prefs::set ("pe_dynamicPlugins", flag (a, "dynamic_plugin_processing")); hostChanged = true; }
             if (has (a, "low_buffer_optimization"))   { Prefs::set ("pe_lowBufferOpt", flag (a, "low_buffer_optimization")); hostChanged = rebuild = true; }
             if (has (a, "overload_protection"))       { Prefs::set ("pe_overloadProtection", flag (a, "overload_protection")); hostChanged = true; }
             if (has (a, "elastic_background"))        { Prefs::set ("pe_elasticBackground", flag (a, "elastic_background")); hostChanged = rebuild = true; }
             if (has (a, "disk_cache_seconds"))        { Prefs::set ("pe_diskCacheSeconds", juce::jlimit (1, 60, (int) num (a, "disk_cache_seconds"))); hostChanged = true; }
             if (hostChanged) Perf::apply (session.getEngine(), session.edit.get(), rebuild);
             auto& adm = session.getEngine().getDeviceManager().deviceManager;
             if (has (a, "device_type")) adm.setCurrentAudioDeviceType (str (a, "device_type"), true);
             auto setup = adm.getAudioDeviceSetup();
             bool change = false;
             if (has (a, "device")) { setup.inputDeviceName = setup.outputDeviceName = str (a, "device"); change = true; }
             if (has (a, "sample_rate")) { setup.sampleRate = num (a, "sample_rate"); change = true; }
             if (has (a, "buffer_size")) { setup.bufferSize = (int) num (a, "buffer_size"); change = true; }
             if (has (a, "input_channels"))  { setup.useDefaultInputChannels = false;  setup.inputChannels.clear();  setup.inputChannels.setRange (0, (int) num (a, "input_channels"), true); change = true; }
             if (has (a, "output_channels")) { setup.useDefaultOutputChannels = false; setup.outputChannels.clear(); setup.outputChannels.setRange (0, (int) num (a, "output_channels"), true); change = true; }
             if (change)
             {
                 auto err = adm.setAudioDeviceSetup (setup, true);
                 if (err.isNotEmpty()) return ToolResult::error (err);
             }
             auto o = obj();
             set (o, "device_type", adm.getCurrentAudioDeviceType());
             if (auto dev = adm.getCurrentAudioDevice())
             {
                 set (o, "device", dev->getName());
                 set (o, "sample_rate", dev->getCurrentSampleRate());
                 set (o, "buffer_size", dev->getCurrentBufferSizeSamples());
                 set (o, "input_channels", dev->getActiveInputChannels().countNumberOfSetBits());
                 set (o, "output_channels", dev->getActiveOutputChannels().countNumberOfSetBits());
                 set (o, "latency_ms", juce::roundToInt ((dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples()) * 1000.0 / juce::jmax (1.0, dev->getCurrentSampleRate())));
             }
             else set (o, "device", "none");
             juce::Array<juce::var> names;
             if (auto type = adm.getCurrentDeviceTypeObject()) for (auto& n : type->getDeviceNames()) names.add (n);
             set (o, "available_devices", names);
             juce::Array<juce::var> sizes;
             for (auto sz : Perf::bufferSizes (session.getEngine())) sizes.add (sz);
             set (o, "available_buffer_sizes", sizes);
             auto host = obj();
             set (host, "host_processors", Prefs::hostProcessors());
             set (host, "cpu_cores", Prefs::numCpus());
             set (host, "cpu_usage_limit", Prefs::cpuUsageLimitPercent());
             set (host, "ignore_errors", Prefs::ignoreErrors());
             set (host, "dynamic_plugin_processing", Prefs::dynamicPluginProcessing());
             set (host, "low_buffer_optimization", Prefs::lowBufferOptimisation());
             set (host, "overload_protection", Prefs::overloadProtection());
             set (host, "elastic_background", Prefs::elasticBackground());
             set (host, "disk_cache_seconds", Prefs::diskCacheSeconds());
             set (o, "host_engine", host);
             return ToolResult::ok (o);
         });

    add ("crash_recovery", "Crash protection: status (did the last run crash, the plug-in that caused it, Safe Mode, plug-ins switched off after a crash, crash report), turn_on_plugin / switch_off_plugin (name as listed, e.g. 'My EQ (VST3)'), leave_safe_mode. Turning plug-ins back on reopens the session after saving it.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["status","turn_on_plugin","switch_off_plugin","leave_safe_mode"]},"plugin":{"type":"string"}}})json",
         [this] (const juce::var& a)
         {
             auto action = str (a, "action", "status");
             auto reload = [this]
             {
                 auto f = session.getFile();
                 if (session.edit != nullptr && session.savingAllowed()) session.save();
                 if (f.existsAsFile()) session.openSession (f);
                 if (ui.refresh) ui.refresh();
             };
             if (action == "leave_safe_mode") { CrashGuard::state().safeMode = false; reload(); }
             else if (action == "turn_on_plugin" || action == "switch_off_plugin")
             {
                 auto want = str (a, "plugin").trim();
                 if (want.isEmpty()) return ToolResult::error ("Give 'plugin'.");
                 juce::String match = want;
                 for (auto& d : CrashGuard::disabledPlugins()) if (d.containsIgnoreCase (want)) match = d;
                 if (action == "switch_off_plugin" && ! match.contains (" ("))
                     for (auto& d : session.getEngine().getPluginManager().knownPluginList.getTypes())
                         if (d.name.equalsIgnoreCase (want)) match = d.name + " (" + d.pluginFormatName + ")";
                 if (action == "switch_off_plugin" && ! match.contains (" ("))
                     return ToolResult::error ("'" + want + "' is not a scanned third-party plug-in. Only VST3 / Audio Unit plug-ins can be switched off (built-in Squelchmoth plug-ins never crash the app this way).");
                 CrashGuard::setPluginSwitchedOff (match, action == "switch_off_plugin");
                 reload();
             }
             auto& st = CrashGuard::state();
             auto o = obj();
             set (o, "last_run_crashed", st.crashedLastTime);
             set (o, "crashed_while_opening_session", st.crashedWhileLoading);
             juce::Array<juce::var> culprits; for (auto& c : st.culprits) culprits.add (c);
             set (o, "crash_plugins", culprits);
             set (o, "safe_mode", st.safeMode);
             juce::Array<juce::var> off; for (auto& d : CrashGuard::disabledPlugins()) off.add (d);
             set (o, "plugins_switched_off", off);
             set (o, "crash_report", CrashGuard::reportFile().existsAsFile() ? CrashGuard::reportFile().getFullPathName() : juce::String());
             return ToolResult::ok (o);
         });

    add ("system_usage", "Window > System Usage as data: audio engine load (now / peak), overloads and interface dropouts, automatic buffer raises, memory, disk cache and the heaviest plug-ins (share of each audio buffer, with their tracks). Use it to find what to freeze or bypass when a session gets heavy.",
         R"json({"type":"object","properties":{"reset_counters":{"type":"boolean"}}})json",
         [this] (const juce::var& a)
         {
             if (flag (a, "reset_counters", false) && ui.perfReset) ui.perfReset();
             auto o = ui.perfStats ? ui.perfStats() : juce::var (new juce::DynamicObject());
             auto& e = session.getEngine();
             set (o, "buffer_size", Perf::currentBufferSize (e));
             set (o, "sample_rate", Perf::currentSampleRate (e));
             set (o, "host_processors", Prefs::hostProcessors());
             set (o, "cpu_cores", Prefs::numCpus());
             set (o, "cpu_usage_limit", Prefs::cpuUsageLimitPercent());
             set (o, "memory_mb", juce::roundToInt (Perf::processMemoryMB()));
             set (o, "computer_memory_gb", juce::SystemStats::getMemorySizeInMegabytes() / 1024);
             set (o, "dynamic_processing_blocks_skipped", (juce::int64) ::dynamicPluginBlocksSkipped().load());
             set (o, "disk_cache_mb", juce::roundToInt (e.getAudioFileManager().cache.getBytesInUse() / (1024.0 * 1024.0)));
             juce::Array<juce::var> heavy;
             for (auto& h : Perf::heaviestPlugins (session, 10))
             {
                 auto p = obj();
                 set (p, "plugin", h.plugin); set (p, "track", h.track);
                 set (p, "cpu_percent", juce::roundToInt (h.cpu * 1000.0) / 10.0);
                 set (p, "bypassed", ! h.enabled);
                 heavy.add (p);
             }
             set (o, "heaviest_plugins", heavy);
             return ToolResult::ok (o);
         });
}
