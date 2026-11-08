// Squelchmoth 1.7 - Claude tools for Session Setup (sample rate, bit depth, timecode, pan depth), the tempo / meter map,
// rulers and counters, track heights, polarity, Auto-Save / Session File Backups, mix snapshots, insert-chain presets,
// stem export and the uninstall information. Included inside the McpTools class body.

/** Rows named in "tracks" (names or numbers), or "track", or else the selected tracks. */
juce::Array<te::Track*> rowsArg (const juce::var& a, bool fallBackToSelection = true)
{
    juce::Array<te::Track*> out;
    auto addRef = [&] (const juce::var& v)
    {
        if (v.toString().trim().equalsIgnoreCase ("master")) { if (auto m = edit().getMasterTrack()) out.addIfNotAlreadyThere (m); return; }
        if (auto t = session.findRow (v)) out.addIfNotAlreadyThere (t);
    };
    if (auto arr = a["tracks"].getArray()) for (auto& v : *arr) addRef (v);
    else if (a["tracks"].toString().equalsIgnoreCase ("all")) for (auto t : session.getRows()) out.add (t);
    if (has (a, "track")) addRef (a["track"]);
    if (out.isEmpty() && fallBackToSelection && ui.state != nullptr && ! has (a, "tracks") && ! has (a, "track"))
        for (auto t : session.getRows()) if (ui.state->isTrackSelected (t)) out.add (t);
    return out;
}

void register17Tools()
{
    auto refresh = [this] { if (ui.refresh) ui.refresh(); };

    //==========================================================================
    add ("session_setup", "Read or change Session Setup (like Pro Tools Setup > Session): sample_rate (44100-192000; the interface is switched to it and recordings / bounces use it), "
         "bit_depth for new recordings (16, 24, 32 = 32-bit float, the default - it can't clip), timecode_rate (23.976, 24, 25, 29.97, 29.97 Drop, 30), timecode_start (e.g. '01:00:00:00') and "
         "pan_depth (0 = linear/0 dB, 1 = -2.5 dB, 2 = -3 dB (default), 3 = -4.5 dB, 4 = -6 dB; applies to mono and dual panners only). Omit all to read.",
         R"json({"type":"object","properties":{"sample_rate":{"type":"number"},"bit_depth":{"type":["integer","string"]},"timecode_rate":{"type":"string"},"timecode_start":{"type":"string"},"pan_depth":{"type":"integer","minimum":0,"maximum":4}},"required":[]})json",
         [this, refresh] (const juce::var& a)
         {
             auto& e = edit();
             juce::StringArray notes;
             if (has (a, "sample_rate"))
             {
                 static const double ok[] = { 44100, 48000, 88200, 96000, 176400, 192000 };
                 const double sr = (double) a["sample_rate"];
                 if (std::find (std::begin (ok), std::end (ok), sr) == std::end (ok)) return ToolResult::error ("sample_rate must be 44100, 48000, 88200, 96000, 176400 or 192000.");
                 if (! session.setSessionSampleRate (sr))
                     notes.add ("The audio interface can't run at " + juce::String (sr, 0) + " Hz (it is at " + juce::String (e.engine.getDeviceManager().getSampleRate(), 0)
                                + " Hz), so playback is resampled. Recordings and bounces still use " + juce::String (sr, 0) + " Hz. Pick another interface in Setup > Playback Engine.");
             }
             if (has (a, "bit_depth"))
             {
                 auto b = a["bit_depth"].toString().retainCharacters ("0123456789").getIntValue();
                 if (b != 16 && b != 24 && b != 32) return ToolResult::error ("bit_depth must be 16, 24 or 32 (float).");
                 session.setRecordBitDepth (b);
             }
             if (has (a, "timecode_rate"))
             {
                 auto want = str (a, "timecode_rate").trim();
                 int idx = -1;
                 for (int i = 0; i < (int) TimeFmt::tcRates().size(); ++i)
                     if (want.equalsIgnoreCase (TimeFmt::tcRates()[(size_t) i].name) || (want.containsIgnoreCase ("drop") && i == 4)) { idx = i; break; }
                 if (idx < 0) return ToolResult::error ("timecode_rate must be one of 23.976, 24, 25, 29.97, 29.97 Drop, 30.");
                 e.state.setProperty ("sqmTcRate", idx, &e.getUndoManager());
             }
             if (has (a, "timecode_start"))
             {
                 auto parts = juce::StringArray::fromTokens (str (a, "timecode_start").replaceCharacter (';', ':'), ":", "");
                 if (parts.size() != 4) return ToolResult::error ("timecode_start must look like hh:mm:ss:ff, e.g. 01:00:00:00.");
                 const auto& r = TimeFmt::tcRates()[(size_t) TimeFmt::tcRateIndex (e)];
                 const double secs = parts[0].getIntValue() * 3600.0 + parts[1].getIntValue() * 60.0 + parts[2].getIntValue() + parts[3].getIntValue() / r.fps;
                 e.state.setProperty ("sqmTcStart", secs, &e.getUndoManager());
             }
             if (has (a, "pan_depth")) session.setPanLaw ((int) a["pan_depth"]);
             refresh();

             auto o = new juce::DynamicObject();
             o->setProperty ("sample_rate", TimeFmt::sampleRate (e));
             o->setProperty ("interface_sample_rate", e.engine.getDeviceManager().getSampleRate());
             o->setProperty ("record_bit_depth", session.recordBitDepth() == 32 ? juce::String ("32-bit float") : juce::String (session.recordBitDepth()) + "-bit");
             o->setProperty ("timecode_rate", TimeFmt::tcRates()[(size_t) TimeFmt::tcRateIndex (e)].name);
             o->setProperty ("timecode_start", TimeFmt::timecode (e, 0.0));
             o->setProperty ("pan_depth", Session::panLawNames()[session.getPanLaw()]);
             if (! notes.isEmpty()) o->setProperty ("note", notes.joinIntoString (" "));
             return ToolResult::ok (juce::var (o));
         });

    //==========================================================================
    add ("tempo_map", "Work with the tempo and meter rulers. action: list | set_tempo (bar, bpm - changes the tempo event at that bar or adds one) | "
         "delete_tempo (index from list, not the first) | set_meter (bar, meter like '7/8') | delete_meter (index, not the first). Bars are 1-based and "
         "follow the existing meter map.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","set_tempo","delete_tempo","set_meter","delete_meter"]},"bar":{"type":"number"},"bpm":{"type":"number"},"meter":{"type":"string"},"index":{"type":"integer"}},"required":["action"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto& e = edit();
             auto& ts = e.tempoSequence;
             auto act = str (a, "action").toLowerCase();
             auto atBar = [&] { return session.barToTime (juce::jmax (1.0, has (a, "bar") ? (double) a["bar"] : 1.0)); };

             if (act == "set_tempo")
             {
                 const double bpm = (double) a["bpm"];
                 if (bpm < 20 || bpm > 400) return ToolResult::error ("bpm must be between 20 and 400.");
                 auto t = atBar();
                 const auto beat = ts.toBeats (t);
                 te::TempoSetting* hit = nullptr;
                 for (int i = 0; i < ts.getNumTempos(); ++i) if (std::abs ((ts.getTempo (i)->getStartBeat() - beat).inBeats()) < 0.01) hit = ts.getTempo (i);
                 if (hit != nullptr) hit->setBpm (bpm); else ts.insertTempo (beat, bpm, 0.0f);
             }
             else if (act == "delete_tempo")
             {
                 const int i = (int) a["index"];
                 if (i < 1 || i >= ts.getNumTempos()) return ToolResult::error ("index must be 1.." + juce::String (ts.getNumTempos() - 1) + " (the first tempo can be changed but not deleted).");
                 ts.removeTempo (i, false);
             }
             else if (act == "set_meter")
             {
                 auto p = juce::StringArray::fromTokens (str (a, "meter"), "/", "");
                 const int n = p[0].getIntValue(), d = p[1].getIntValue();
                 if (n < 1 || n > 32 || ! (d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32)) return ToolResult::error ("meter must look like 4/4, 3/4, 6/8 or 7/8.");
                 auto t = atBar();
                 te::TimeSigSetting* target = nullptr;
                 for (int i = 0; i < ts.getNumTimeSigs(); ++i) if (std::abs ((ts.getTimeSig (i)->getStartBeat() - ts.toBeats (t)).inBeats()) < 0.01) target = ts.getTimeSig (i);
                 if (target == nullptr) if (auto ns = ts.insertTimeSig (t)) target = ns.get();
                 if (target == nullptr) return ToolResult::error ("Couldn't add a meter there.");
                 target->numerator = n; target->denominator = d;
             }
             else if (act == "delete_meter")
             {
                 const int i = (int) a["index"];
                 if (i < 1 || i >= ts.getNumTimeSigs()) return ToolResult::error ("index must be 1.." + juce::String (ts.getNumTimeSigs() - 1) + " (the first meter can be changed but not deleted).");
                 ts.removeTimeSig (i);
             }
             else if (act != "list") return ToolResult::error ("Unknown action.");
             refresh();

             juce::Array<juce::var> tempos, meters;
             for (int i = 0; i < ts.getNumTempos(); ++i)
             {
                 auto tp = ts.getTempo (i);
                 auto o = new juce::DynamicObject();
                 o->setProperty ("index", i);
                 o->setProperty ("at", TimeFmt::barsBeats (e, ts.toTime (tp->getStartBeat())));
                 o->setProperty ("bpm", tp->getBpm());
                 tempos.add (juce::var (o));
             }
             for (int i = 0; i < ts.getNumTimeSigs(); ++i)
             {
                 auto sg = ts.getTimeSig (i);
                 auto o = new juce::DynamicObject();
                 o->setProperty ("index", i);
                 o->setProperty ("at", TimeFmt::barsBeats (e, ts.toTime (sg->getStartBeat())));
                 o->setProperty ("meter", juce::String (sg->numerator.get()) + "/" + juce::String (sg->denominator.get()));
                 meters.add (juce::var (o));
             }
             auto o = new juce::DynamicObject();
             o->setProperty ("tempos", tempos);
             o->setProperty ("meters", meters);
             return ToolResult::ok (juce::var (o));
         });

    //==========================================================================
    add ("rulers", "Choose which rulers the Edit window shows and the Main / Sub counter formats. show: any of bars_beats, min_secs, timecode, samples, "
         "tempo, meter, markers. main_counter / sub_counter: bars_beats, min_secs, timecode or samples. Omit everything to read the current setup.",
         R"json({"type":"object","properties":{"show":{"type":"array","items":{"type":"string","enum":["bars_beats","min_secs","timecode","samples","tempo","meter","markers"]}},"main_counter":{"type":"string","enum":["bars_beats","min_secs","timecode","samples"]},"sub_counter":{"type":"string","enum":["bars_beats","min_secs","timecode","samples"]}},"required":[]})json",
         [this, refresh] (const juce::var& a)
         {
             static const juce::StringArray rows { "bars_beats", "min_secs", "timecode", "samples", "tempo", "meter", "markers" };
             if (auto arr = a["show"].getArray())
             {
                 juce::StringArray ids;
                 for (int i = 0; i < rows.size(); ++i) for (auto& v : *arr) if (v.toString().equalsIgnoreCase (rows[i])) { ids.add (juce::String (i)); break; }
                 if (ids.isEmpty()) return ToolResult::error ("Show at least one ruler.");
                 Prefs::set ("ed_rulers", ids.joinIntoString (","));
             }
             auto fmtIdx = [] (const juce::String& n) { return rows.indexOf (n.toLowerCase()); };
             if (has (a, "main_counter")) { auto i = fmtIdx (str (a, "main_counter")); if (i < 0 || i > 3) return ToolResult::error ("Unknown counter format."); Prefs::set ("ed_mainCounterFmt", i); }
             if (has (a, "sub_counter"))  { auto i = fmtIdx (str (a, "sub_counter"));  if (i < 0 || i > 3) return ToolResult::error ("Unknown counter format."); Prefs::set ("ed_subCounterFmt", i); }
             refresh();

             juce::Array<juce::var> shown;
             for (auto& s : juce::StringArray::fromTokens (Prefs::getString ("ed_rulers", "0,1,4,5,6"), ",", "")) if (s.getIntValue() >= 0 && s.getIntValue() < rows.size()) shown.add (rows[s.getIntValue()]);
             auto o = new juce::DynamicObject();
             o->setProperty ("rulers", shown);
             o->setProperty ("main_counter", rows[juce::jlimit (0, 3, Prefs::getInt ("ed_mainCounterFmt", 0))]);
             o->setProperty ("sub_counter", rows[juce::jlimit (0, 3, Prefs::getInt ("ed_subCounterFmt", 1))]);
             auto& e = edit();
             const auto now = e.getTransport().getPosition();
             o->setProperty ("playhead", TimeFmt::format (e, now, (TimeFmt::Kind) juce::jlimit (0, 3, Prefs::getInt ("ed_mainCounterFmt", 0))));
             return ToolResult::ok (juce::var (o));
         });

    //==========================================================================
    add ("track_height", "Set track heights like Pro Tools: micro, mini, small, medium, large, jumbo, extreme, a pixel value, or 'fit' (the tracks share "
         "the window). tracks: names/numbers, 'all', or omit for the selected tracks. Heights are saved with the session.",
         R"json({"type":"object","properties":{"tracks":{"type":["array","string"],"items":{"type":["string","integer"]}},"height":{"type":["string","integer"]}},"required":["height"]})json",
         [this, refresh] (const juce::var& a)
         {
             if (ui.state == nullptr) return ToolResult::error ("The Edit window is not available yet.");
             auto rows = rowsArg (a);
             if (rows.isEmpty()) rows = session.getRows();
             auto h = a["height"].toString().trim().toLowerCase();
             if (h == "fit")
             {
                 if (! ui.fitHeights) return ToolResult::error ("The Edit window is not available yet.");
                 ui.fitHeights (rows);
             }
             else
             {
                 int px = h.containsOnly ("0123456789") ? h.getIntValue() : 0;
                 for (auto& p : TimelineState::heightPresets()) if (h == juce::String (p.name).toLowerCase()) px = p.px;
                 if (px < 22 || px > 1200) return ToolResult::error ("height must be micro, mini, small, medium, large, jumbo, extreme, fit or 22-1200 pixels.");
                 for (auto t : rows) ui.state->setHeight (t, px);
             }
             refresh();
             juce::StringArray out;
             for (auto t : rows) out.add (t->getName() + " " + juce::String (ui.state->heightFor (t)) + " px");
             return ToolResult::ok (out.joinIntoString (", "));
         });

    //==========================================================================
    add ("polarity", "Invert (or restore) the polarity (phase) of tracks - e.g. a snare bottom mic against the top mic. tracks: names/numbers or omit for the "
         "selected tracks; inverted true/false (omit to toggle). Glitch-free, undoable, saved with the session.",
         R"json({"type":"object","properties":{"tracks":{"type":"array","items":{"type":["string","integer"]}},"track":{"type":["string","integer"]},"inverted":{"type":"boolean"}},"required":[]})json",
         [this, refresh] (const juce::var& a)
         {
             auto rows = rowsArg (a);
             if (rows.isEmpty()) return ToolResult::error ("Name the tracks (tracks: [...]) or select some first.");
             juce::StringArray out;
             for (auto t : rows)
             {
                 const bool inv = has (a, "inverted") ? flag (a, "inverted") : ! TrackOps::isPolarityInverted (*t);
                 if (! TrackOps::setPolarityInverted (*t, inv)) { out.add (t->getName() + ": no polarity control (MIDI / VCA)"); continue; }
                 out.add (t->getName() + (inv ? ": inverted" : ": normal"));
             }
             refresh();
             return ToolResult::ok (out.joinIntoString (", "));
         });

    //==========================================================================
    add ("backups", "Auto-Save and Session File Backups. action: status | list | backup_now | restore (index from list - opens the backup as a new "
         "'(restored)' session; the original is untouched) | settings (auto_save, auto_save_minutes, auto_backup, backup_minutes, keep, system_drive_copy). "
         "Saving never happens while playing or recording.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["status","list","backup_now","restore","settings"]},"index":{"type":"integer"},"auto_save":{"type":"boolean"},"auto_save_minutes":{"type":"integer","minimum":1,"maximum":99},"auto_backup":{"type":"boolean"},"backup_minutes":{"type":"integer","minimum":1,"maximum":99},"keep":{"type":"integer","minimum":1,"maximum":999},"system_drive_copy":{"type":"boolean"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto saver = AutoSaver::instance();
             if (saver == nullptr) return ToolResult::error ("Auto-Save isn't running yet.");
             auto act = str (a, "action").toLowerCase();
             auto f = session.getFile();
             if (act == "settings")
             {
                 if (has (a, "auto_save")) Prefs::set ("op_autoSave", flag (a, "auto_save"));
                 if (has (a, "auto_save_minutes")) Prefs::set ("op_autoSaveMinutes", juce::jlimit (1, 99, (int) a["auto_save_minutes"]));
                 if (has (a, "auto_backup")) Prefs::set ("op_autoBackup", flag (a, "auto_backup"));
                 if (has (a, "backup_minutes")) Prefs::set ("op_autoBackupMinutes", juce::jlimit (1, 99, (int) a["backup_minutes"]));
                 if (has (a, "keep")) Prefs::set ("op_autoBackupKeep", juce::jlimit (1, 999, (int) a["keep"]));
                 if (has (a, "system_drive_copy")) Prefs::set ("op_autoBackupMirror", flag (a, "system_drive_copy"));
                 return ToolResult::ok (saver->status());
             }
             if (act == "status") return ToolResult::ok (saver->status());
             if (f == juce::File()) return ToolResult::error ("Save the session first - backups live next to the session file.");
             if (act == "backup_now")
             {
                 juce::String err;
                 return saver->backupNow (&err) ? ToolResult::ok (saver->status()) : ToolResult::error ("Backup failed: " + err);
             }
             auto list = AutoSaver::listBackups (f);
             if (act == "list")
             {
                 juce::Array<juce::var> arr;
                 for (int i = 0; i < (int) list.size(); ++i)
                 {
                     auto o = new juce::DynamicObject();
                     o->setProperty ("index", i + 1);
                     o->setProperty ("file", list[(size_t) i].file.getFileName());
                     o->setProperty ("saved", list[(size_t) i].time.toString (true, true));
                     o->setProperty ("tracks", list[(size_t) i].tracks);
                     o->setProperty ("kb", (int) (list[(size_t) i].bytes / 1024));
                     o->setProperty ("system_drive_copy", list[(size_t) i].mirror);
                     arr.add (juce::var (o));
                 }
                 return ToolResult::ok (juce::var (arr));
             }
             if (act == "restore")
             {
                 const int i = (int) a["index"];
                 if (i < 1 || i > (int) list.size()) return ToolResult::error ("index must be 1.." + juce::String ((int) list.size()) + " - call backups with action list first.");
                 juce::String err;
                 if (! saver->restore (list[(size_t) (i - 1)].file, &err)) return ToolResult::error ("Restore failed: " + err);
                 return ToolResult::ok ("Restored " + list[(size_t) (i - 1)].file.getFileName() + " - now open as " + session.getFile().getFileName());
             }
             return ToolResult::error ("Unknown action.");
         });

    //==========================================================================
    add ("uninstall_info", "Explains how to uninstall Squelchmoth (information only - Claude never uninstalls the app). Lists what the uninstaller removes "
         "and what it keeps.",
         R"json({"type":"object","properties":{},"required":[]})json",
         [] (const juce::var&)
         {
             return ToolResult::ok (juce::String (
                 "To uninstall: Help > Uninstall Squelchmoth... in the app, or open \"Uninstall Squelchmoth\" in Applications. It asks for your Mac password once, then removes "
                 "Squelchmoth.app, the uninstaller, the installer receipt, preferences, caches, logs and the licence file. Your sessions, recordings, "
                 "Session File Backups and presets in Documents/Squelchmoth are KEPT unless you tick \"Also delete my sessions\". "
                 "Claude can't run the uninstaller - you have to choose it yourself."));
         });

    //==========================================================================
    add ("mix_snapshot", "Mix Snapshots A-H (saved in the session): store / recall whole mixes - every fader, pan, mute, solo, polarity, send level, "
         "insert bypass and plug-in setting, master included. action: list | store (slot, optional name) | recall (slot) | compare (flip between the last "
         "two recalled) | undo (go back to the mix before the last recall). recall can glide (glide_seconds) | rename (slot, name) | clear (slot). Slots are A-H.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","store","recall","compare","undo","rename","clear"]},"slot":{"type":"string"},"name":{"type":"string"},"glide_seconds":{"type":"number","description":"recall only: move continuous controls there smoothly over this many seconds (0-30)"}},"required":["action"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto act = str (a, "action").toLowerCase();
             if (act == "list") return ToolResult::ok (MixSnap::describe (session));
             if (act == "compare") { auto p = MixSnap::compare (session); refresh(); return p < 0 ? ToolResult::error ("Recall two different snapshots first.") : ToolResult::ok ("Now hearing snapshot " + MixSnap::slotName (p)); }
             if (act == "undo") { auto n = MixSnap::recall (session, MixSnap::undoSlot); refresh(); return n < 0 ? ToolResult::error ("Nothing to undo - no snapshot has been recalled.") : ToolResult::ok ("Back to the mix before the last recall (" + juce::String (n) + " controls changed)"); }
             const int i = MixSnap::slotFromText (str (a, "slot"));
             if (i < 0) return ToolResult::error ("slot must be A-H.");
             if (act == "store") { MixSnap::store (session, i, str (a, "name")); return ToolResult::ok ("Stored the current mix as snapshot " + MixSnap::slotName (i)); }
             if (act == "recall") { auto n = MixSnap::recall (session, i, juce::roundToInt (juce::jlimit (0.0, 30.0, num (a, "glide_seconds", 0.0)) * 1000.0)); refresh(); return n < 0 ? ToolResult::error ("Snapshot " + MixSnap::slotName (i) + " is empty.") : ToolResult::ok ("Recalled snapshot " + MixSnap::slotName (i) + " (" + juce::String (n) + " controls changed)"); }
             if (act == "rename") { if (! MixSnap::slot (edit(), i).isValid()) return ToolResult::error ("Snapshot " + MixSnap::slotName (i) + " is empty."); MixSnap::rename (session, i, str (a, "name")); return ToolResult::ok ("Renamed"); }
             if (act == "clear") { MixSnap::clear (session, i); return ToolResult::ok ("Cleared snapshot " + MixSnap::slotName (i)); }
             return ToolResult::error ("Unknown action.");
         });

    //==========================================================================
    add ("insert_chain", "Insert Chain presets (.sqmchain, in Documents/Squelchmoth/Insert Chains): save a track's whole insert chain (every plug-in with its "
         "settings and bypass, in order) or load one onto any track (after its inserts, or replace=true to swap them). action: list | save (track, name) | "
         "load (track, name, replace). track 'master' = the Master Fader.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","save","load"]},"track":{"type":["string","integer"]},"name":{"type":"string"},"replace":{"type":"boolean"}},"required":["action"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto act = str (a, "action").toLowerCase();
             if (act == "list") { juce::Array<juce::var> out; for (auto& f : InsertChains::list()) out.add (f.getFileNameWithoutExtension()); return ToolResult::ok (juce::var (out)); }
             te::Track* row = nullptr;
             if (! isMasterRef (a))
             {
                 row = session.findRow (a["track"]);
                 if (row == nullptr) return ToolResult::error ("Name the track (track: name or number, or 'master').");
             }
             juce::String err;
             if (act == "save")
             {
                 auto name = str (a, "name", (row ? row->getName() : juce::String ("Master")) + " chain");
                 auto n = InsertChains::save (session, row, name, err);
                 return n < 0 ? ToolResult::error (err) : ToolResult::ok ("Saved insert chain '" + name + "' (" + juce::String (n) + " plug-ins)");
             }
             if (act == "load")
             {
                 auto f = InsertChains::fileFor (str (a, "name"));
                 if (! f.existsAsFile()) return ToolResult::error ("No insert chain named '" + str (a, "name") + "'. Use action list.");
                 auto n = InsertChains::load (session, row, f, flag (a, "replace", false), err);
                 refresh();
                 if (n < 0) return ToolResult::error (err);
                 return ToolResult::ok ("Loaded " + juce::String (n) + " plug-ins onto " + (row ? row->getName() : juce::String ("Master")) + (err.isNotEmpty() ? ". " + err : juce::String()));
             }
             return ToolResult::error ("Unknown action.");
         });

    //==========================================================================
    add ("export_stems", "Export stems (offline): every track - or the selected tracks, the routing folders, or a list - as its own file, all starting at the "
         "same point and the same length so they line up in any DAW. Defaults: WAV 24-bit dithered at the session sample rate, no master inserts, name "
         "template '{n} {track}' ({n} {track} {session} {bpm} {date} {sr}), into Bounces/<session> Stems <date>. with_send_effects=true also prints the "
         "reverbs / delays each track sends to. skip_silent=true deletes stems that rendered silent.",
         R"json({"type":"object","properties":{"source":{"type":"string","enum":["all","selected","folders"]},"tracks":{"type":"array","items":{"type":["string","integer"]}},"name_template":{"type":"string"},"format":{"type":"string","enum":["wav","aiff","flac"]},"bit_depth":{"type":"integer","enum":[16,24,32]},"sample_rate":{"type":"number"},"dither":{"type":"boolean"},"master_inserts":{"type":"boolean"},"with_send_effects":{"type":"boolean"},"skip_silent":{"type":"boolean","description":"delete stems that rendered silent"},"tail_seconds":{"type":"number"},"use_selection":{"type":"boolean"},"folder":{"type":"string"}},"required":[]})json",
         [this] (const juce::var& a)
         {
             BounceExport::StemOptions o;
             o.format = str (a, "format", "wav").toLowerCase();
             o.bitDepth = (int) num (a, "bit_depth", 24);
             o.sampleRate = num (a, "sample_rate", 0.0);
             o.dither = flag (a, "dither", true);
             o.masterInserts = flag (a, "master_inserts", false);
             o.withSendEffects = flag (a, "with_send_effects", false);
             o.skipSilent = flag (a, "skip_silent", false);
             o.tailSeconds = num (a, "tail_seconds", 2.0);
             o.nameTemplate = str (a, "name_template", "{n} {track}");
             if (has (a, "folder")) o.folder = juce::File (str (a, "folder"));
             if (flag (a, "use_selection", false) && ui.state != nullptr && ui.state->timeSelection) o.range = *ui.state->timeSelection;
             auto src = str (a, "source", "all").toLowerCase();
             o.source = src == "selected" ? BounceExport::StemSource::selectedTracks : src == "folders" ? BounceExport::StemSource::folders : BounceExport::StemSource::allTracks;
             if (auto arr = a["tracks"].getArray()) for (auto& v : *arr) if (auto t = session.findRow (v)) o.explicitTracks.add (t);
             auto res = BounceExport::exportStems (session, o, [this] (te::Track* t) { return ui.state != nullptr && ui.state->isTrackSelected (t); });
             if (res.written == 0) return ToolResult::error ("No stems written. " + res.lines.joinIntoString ("; "));
             auto out = new juce::DynamicObject();
             out->setProperty ("folder", res.folder.getFullPathName());
             out->setProperty ("written", res.written);
             out->setProperty ("failed", res.failed);
             if (res.silent > 0) out->setProperty ("skipped_silent", res.silent);
             juce::Array<juce::var> files; for (auto& l : res.lines) files.add (l);
             out->setProperty ("files", files);
             return ToolResult::ok (juce::var (out));
         });
}
