// Squelchmoth 1.8.1 - Claude tools for Chrysalis 2 / Chrysalis SF (sampler), Mandible / Mandible Lite (slicer),
// Time Weaver slots, Pollen packs. Included inside the McpTools class body.

template <typename T>
T* instrumentOfType (const juce::var& a, juce::String& err, const char* what)
{
    te::AudioTrack* t = nullptr;
    if (has (a, "channel")) { t = rackChannelArg (a, err); if (t == nullptr) return nullptr; }
    else { t = trackArg (a, err); if (t == nullptr) return nullptr; }
    if (has (a, "slot")) { if (auto p = dynamic_cast<T*> (insertArg (a, err))) return p; }
    for (auto p : t->pluginList) if (auto x = dynamic_cast<T*> (p)) return x;
    err = juce::String ("No ") + what + " on " + t->getName() + " (insert one with instrument insert, or add_track with instrument).";
    return nullptr;
}

juce::File fileArg181 (const juce::var& a, const char* key = "file")
{
    auto s = str (a, key);
    if (s.isEmpty()) return {};
    juce::File f (s);
    if (f.existsAsFile() || f.isDirectory()) return f;
    // relative to the session folder, the bounces folder or the home folder
    for (auto base : { session.sessionsDir, session.bouncesDir, juce::File::getSpecialLocation (juce::File::userHomeDirectory), juce::File::getSpecialLocation (juce::File::userMusicDirectory) })
        if (auto c = base.getChildFile (s); c.exists()) return c;
    return f;
}

void register181Tools()
{
    //==========================================================================
    add ("sampler",
         "Chrysalis 2 / Chrysalis SF zones (track or channel; slot optional). Actions: zones - list every zone; add (file, root, lo_key, hi_key, lo_vel, hi_vel) - one zone; "
         "map (folder or files[]) - auto-map a folder of samples across the keyboard from the note names in the file names ('Piano C4.wav', '... 60.wav') or the WAV root note; "
         "import (file .sfz or .sf2, preset index or name for SF2) - load a bank (replaces the zones); presets (file .sf2) - the bank's preset list; "
         "set (zone, any of: root, lo_key, hi_key, lo_vel, hi_vel, tune_cents, gain_db, pan, loop on|off, loop_start, loop_end, crossfade, round_robin, attack, decay, sustain, release); remove (zone); clear.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["zones","add","map","import","presets","set","remove","clear"]},"track":{"type":["string","integer"]},"channel":{"type":["string","integer"]},"slot":{"type":["string","integer"]},
            "file":{"type":"string"},"folder":{"type":"string"},"files":{"type":"array","items":{"type":"string"}},"preset":{"type":["integer","string"]},"zone":{"type":"integer"},
            "root":{"type":"integer"},"lo_key":{"type":"integer"},"hi_key":{"type":"integer"},"lo_vel":{"type":"integer"},"hi_vel":{"type":"integer"},"tune_cents":{"type":"number"},"gain_db":{"type":"number"},"pan":{"type":"number"},
            "loop":{"type":["boolean","string"]},"loop_start":{"type":"integer"},"loop_end":{"type":"integer"},"crossfade":{"type":"integer"},"round_robin":{"type":"integer"},"attack":{"type":"number"},"decay":{"type":"number"},"sustain":{"type":"number"},"release":{"type":"number"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             const auto action = str (a, "action"); juce::String err;
             if (action == "presets")
             {
                 auto f = fileArg181 (a); if (! f.existsAsFile()) return ToolResult::error ("Give file: an .sf2 bank");
                 juce::Array<juce::var> out; for (auto& p : SqSynth::SqChrysalis2Plugin::sf2Presets (f)) { auto o = obj(); set (o, "index", p.presetIndex); set (o, "name", p.name); set (o, "bank", p.bank); set (o, "program", p.program); out.add (o); }
                 auto res = obj(); set (res, "file", f.getFullPathName()); set (res, "presets", out); set (res, "count", out.size()); return ToolResult::ok (res);
             }
             auto sp = instrumentOfType<SqSynth::SqChrysalis2Plugin> (a, err, "Chrysalis 2 / Chrysalis SF"); if (sp == nullptr) return ToolResult::error (err);
             auto zonesJson = [&]
             {
                 juce::Array<juce::var> out;
                 for (int i = 0; i < sp->numZones(); ++i)
                 {
                     auto z = sp->zone (i); auto o = obj();
                     set (o, "zone", i + 1); set (o, "name", z.getProperty ("name").toString()); set (o, "file", z.getProperty ("file").toString());
                     set (o, "root", (int) z.getProperty ("root")); set (o, "lo_key", (int) z.getProperty ("loKey")); set (o, "hi_key", (int) z.getProperty ("hiKey")); set (o, "lo_vel", (int) z.getProperty ("loVel")); set (o, "hi_vel", (int) z.getProperty ("hiVel"));
                     set (o, "tune_cents", (double) z.getProperty ("tune", 0)); set (o, "gain_db", (double) z.getProperty ("gain", 0.0)); set (o, "pan", (double) z.getProperty ("pan", 0.0));
                     set (o, "loop", (int) z.getProperty ("loopMode", 0) > 0); set (o, "loop_start", (int) z.getProperty ("loopStart", -1)); set (o, "loop_end", (int) z.getProperty ("loopEnd", -1)); set (o, "crossfade", (int) z.getProperty ("xfade", 0)); set (o, "round_robin", (int) z.getProperty ("rr", 0));
                     out.add (o);
                 }
                 auto res = obj(); set (res, "instrument", sp->getName()); set (res, "bank", sp->bankName.get()); set (res, "zones", out); set (res, "count", out.size()); return res;
             };
             if (action == "zones") return ToolResult::ok (zonesJson());
             if (action == "clear") { sp->clearZones(); return ToolResult::ok ("Cleared every zone"); }
             if (action == "add")
             {
                 auto f = fileArg181 (a); if (! f.existsAsFile()) return ToolResult::error ("file not found: " + str (a, "file"));
                 const int root = (int) num (a, "root", 60);
                 const int zi = sp->addZone (f, root, (int) num (a, "lo_key", 0), (int) num (a, "hi_key", 127), (int) num (a, "lo_vel", 1), (int) num (a, "hi_vel", 127));
                 sp->applyLoopPointsFromFiles();
                 return ToolResult::ok ("Added zone " + juce::String (zi + 1) + ": " + f.getFileName() + " root " + juce::String (root));
             }
             if (action == "map")
             {
                 juce::Array<juce::File> files;
                 if (has (a, "folder")) { auto d = fileArg181 (a, "folder"); if (! d.isDirectory()) return ToolResult::error ("folder not found: " + str (a, "folder")); d.findChildFiles (files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"); }
                 if (auto* arr = a["files"].getArray()) for (auto& v : *arr) { juce::File f (v.toString()); if (f.existsAsFile()) files.add (f); }
                 if (files.isEmpty()) return ToolResult::error ("Give folder or files[]");
                 const int n = sp->addFilesAutoMapped (files);
                 auto res = zonesJson(); set (res, "added", n); return ToolResult::ok (res);
             }
             if (action == "import")
             {
                 auto f = fileArg181 (a); if (! f.existsAsFile()) return ToolResult::error ("file not found: " + str (a, "file"));
                 int n = 0;
                 if (f.hasFileExtension ("sfz")) n = sp->importSfz (f);
                 else if (f.hasFileExtension ("sf2"))
                 {
                     int preset = 0;
                     if (has (a, "preset")) { if (a["preset"].isString()) { for (auto& p : SqSynth::SqChrysalis2Plugin::sf2Presets (f)) if (p.name.containsIgnoreCase (a["preset"].toString())) { preset = p.presetIndex; break; } } else preset = (int) a["preset"]; }
                     n = sp->importSf2 (f, preset);
                 }
                 else return ToolResult::error ("import takes an .sfz or .sf2 file");
                 if (n == 0) return ToolResult::error ("Nothing usable in " + f.getFileName() + " (no regions with existing samples)");
                 auto res = zonesJson(); set (res, "imported", n); return ToolResult::ok (res);
             }
             const int zi = (int) num (a, "zone", 0) - 1;
             if (! juce::isPositiveAndBelow (zi, sp->numZones())) return ToolResult::error ("zone must be 1-" + juce::String (sp->numZones()));
             if (action == "remove") { sp->removeZone (zi); return ToolResult::ok ("Removed zone " + juce::String (zi + 1)); }
             if (action == "set")
             {
                 struct M { const char* key; const char* prop; bool isInt; };
                 static const M map[] = { { "root", "root", true }, { "lo_key", "loKey", true }, { "hi_key", "hiKey", true }, { "lo_vel", "loVel", true }, { "hi_vel", "hiVel", true }, { "tune_cents", "tune", false }, { "gain_db", "gain", false }, { "pan", "pan", false },
                                          { "loop_start", "loopStart", true }, { "loop_end", "loopEnd", true }, { "crossfade", "xfade", true }, { "round_robin", "rr", true }, { "attack", "attack", false }, { "decay", "decay", false }, { "sustain", "sustain", false }, { "release", "release", false } };
                 juce::StringArray done;
                 for (auto& m : map) if (has (a, m.key)) { sp->setZoneProperty (zi, m.prop, m.isInt ? juce::var ((int) num (a, m.key)) : juce::var (num (a, m.key))); done.add (m.key); }
                 if (has (a, "loop")) { const bool on = a["loop"].isBool() ? (bool) a["loop"] : str (a, "loop").equalsIgnoreCase ("on"); sp->setZoneProperty (zi, "loopMode", on ? 1 : 0); done.add ("loop"); }
                 return ToolResult::ok ("Zone " + juce::String (zi + 1) + " set: " + done.joinIntoString (", "));
             }
             return ToolResult::error ("Unknown action " + action);
         });

    //==========================================================================
    add ("slicer",
         "Mandible / Mandible Lite (track or channel; slot optional). Actions: load (file, deck A|B, mode transients|1/4|1/8|1/16|1/32|1/8T|1/16T, sensitivity 0-1, bars) - put a loop on a deck and slice it; "
         "slices (deck) - the slice list with start times; reslice (deck, mode, sensitivity); set (deck, slice, any of: pitch, reverse, gain_db, cutoff, decay, mute); "
         "dump (deck, start_bar, stretch true|false) - a MIDI clip on the track that plays the slices in order (one note per slice from C1 for deck A, C4 for deck B); "
         "to_channels (deck) - one rack sample channel per slice (Mandible only); trigger (deck, slice) - play a slice now.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["load","slices","reslice","set","dump","to_channels","trigger"]},"track":{"type":["string","integer"]},"channel":{"type":["string","integer"]},"slot":{"type":["string","integer"]},
            "file":{"type":"string"},"deck":{"type":"string"},"mode":{"type":"string"},"sensitivity":{"type":"number"},"bars":{"type":"integer"},"slice":{"type":"integer"},"pitch":{"type":"number"},"reverse":{"type":"boolean"},"gain_db":{"type":"number"},"cutoff":{"type":"number"},"decay":{"type":"number"},"mute":{"type":"boolean"},
            "start_bar":{"type":"number"},"stretch":{"type":"boolean"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             const auto action = str (a, "action"); juce::String err;
             auto mp = instrumentOfType<SqSynth::SqMandiblePlugin> (a, err, "Mandible / Mandible Lite"); if (mp == nullptr) return ToolResult::error (err);
             const int deck = str (a, "deck").trim().toUpperCase().startsWith ("B") && ! mp->isLite ? 1 : 0;
             auto modeArg = [&] (int def) { auto m = str (a, "mode").toLowerCase().replace (" ", ""); if (m.isEmpty()) return def; if (m.startsWith ("trans")) return 0; auto names = SqSynth::SqMandiblePlugin::sliceModeNames(); for (int i = 1; i < names.size(); ++i) if (names[i].toLowerCase().replace (" ", "") == m) return i; return def; };
             auto slicesJson = [&]
             {
                 juce::Array<juce::var> out; auto beats = mp->sliceBeats (deck); auto smp = mp->deckSample (deck); const double fsr = smp != nullptr ? smp->sampleRate : 48000.0;
                 for (int i = 0; i < mp->numSlices (deck); ++i)
                 {
                     auto s = mp->slice (deck, i); auto o = obj();
                     set (o, "slice", i + 1); set (o, "note", SqSynth::SqMandiblePlugin::deckBaseNote[deck] + i); set (o, "start_seconds", (int) s.getProperty ("start") / fsr); set (o, "end_seconds", (int) s.getProperty ("end") / fsr);
                     set (o, "beat", i < beats.size() ? beats[i] : 0.0); set (o, "pitch", (double) s.getProperty ("pitch", 0.0)); set (o, "reverse", (bool) s.getProperty ("reverse", false)); set (o, "gain_db", (double) s.getProperty ("gain", 0.0)); set (o, "cutoff", (double) s.getProperty ("cutoff", 20000.0)); set (o, "decay", (double) s.getProperty ("decay", 10.0)); set (o, "mute", (bool) s.getProperty ("mute", false));
                     out.add (o);
                 }
                 auto res = obj(); set (res, "instrument", mp->getName()); set (res, "deck", deck == 0 ? "A" : "B"); set (res, "file", mp->deckFile (deck).getFullPathName()); set (res, "bpm", mp->deckBpm (deck)); set (res, "bars", (int) mp->decks[(size_t) deck].getProperty ("bars", 0));
                 set (res, "mode", SqSynth::SqMandiblePlugin::sliceModeNames()[juce::jlimit (0, 6, (int) mp->decks[(size_t) deck].getProperty ("sliceMode", 0))]); set (res, "slices", out); set (res, "count", out.size()); return res;
             };
             if (action == "load")
             {
                 auto f = fileArg181 (a); if (! f.existsAsFile()) return ToolResult::error ("file not found: " + str (a, "file"));
                 if (! mp->loadFile (deck, f, modeArg (0), (float) num (a, "sensitivity", 0.5), (int) num (a, "bars", 0))) return ToolResult::error ("Could not read " + f.getFileName());
                 return ToolResult::ok (slicesJson());
             }
             if (action == "slices") return ToolResult::ok (slicesJson());
             if (action == "reslice") { mp->reslice (deck, modeArg ((int) mp->decks[(size_t) deck].getProperty ("sliceMode", 0)), (float) num (a, "sensitivity", (double) mp->decks[(size_t) deck].getProperty ("sensitivity", 0.5))); return ToolResult::ok (slicesJson()); }
             if (action == "set")
             {
                 const int si = (int) num (a, "slice", 0) - 1; if (! juce::isPositiveAndBelow (si, mp->numSlices (deck))) return ToolResult::error ("slice must be 1-" + juce::String (mp->numSlices (deck)));
                 juce::StringArray done;
                 if (has (a, "pitch")) { mp->setSliceProperty (deck, si, "pitch", num (a, "pitch")); done.add ("pitch"); }
                 if (has (a, "reverse")) { mp->setSliceProperty (deck, si, "reverse", flag (a, "reverse")); done.add ("reverse"); }
                 if (has (a, "gain_db")) { mp->setSliceProperty (deck, si, "gain", num (a, "gain_db")); done.add ("gain"); }
                 if (has (a, "cutoff")) { mp->setSliceProperty (deck, si, "cutoff", num (a, "cutoff")); done.add ("cutoff"); }
                 if (has (a, "decay")) { mp->setSliceProperty (deck, si, "decay", num (a, "decay")); done.add ("decay"); }
                 if (has (a, "mute")) { mp->setSliceProperty (deck, si, "mute", flag (a, "mute")); done.add ("mute"); }
                 return ToolResult::ok ("Slice " + juce::String (si + 1) + " set: " + done.joinIntoString (", "));
             }
             if (action == "trigger") { const int si = (int) num (a, "slice", 1) - 1; mp->triggerSlice (deck, si); return ToolResult::ok ("Playing slice " + juce::String (si + 1)); }
             if (action == "dump")
             {
                 auto t = dynamic_cast<te::AudioTrack*> (mp->getOwnerTrack()); if (t == nullptr) return ToolResult::error ("The slicer is not on a track");
                 auto beats = mp->sliceBeats (deck); if (beats.isEmpty()) return ToolResult::error ("No slices on deck " + juce::String (deck == 0 ? "A" : "B") + " - load a file first");
                 const double startBar = num (a, "start_bar", 1.0); const bool stretch = has (a, "stretch") ? flag (a, "stretch") : true;
                 const double deckBpm = juce::jmax (1.0, mp->deckBpm (deck)), songBpm = edit().tempoSequence.getBpmAt (session.barToTime (startBar));
                 // without stretch the slices keep their real length, so the notes land at the deck tempo converted to song beats
                 const double scale = stretch ? 1.0 : songBpm / deckBpm;
                 const double totalBeats = (double) mp->decks[(size_t) deck].getProperty ("bars", 1) * 4.0 * scale;
                 auto start = session.barToTime (startBar); auto endT = session.barToTime (startBar + std::ceil (totalBeats / 4.0));
                 auto clip = t->insertMIDIClip ("Slices " + juce::String (deck == 0 ? "A" : "B"), { start, endT }, nullptr);
                 if (clip == nullptr) return ToolResult::error ("Could not create the MIDI clip");
                 auto& seq = clip->getSequence(); auto* um = &edit().getUndoManager();
                 for (int i = 0; i < beats.size(); ++i)
                 {
                     const double b0 = beats[i] * scale, b1 = (i + 1 < beats.size() ? beats[i + 1] * scale : totalBeats);
                     seq.addNote (SqSynth::SqMandiblePlugin::deckBaseNote[deck] + i, te::BeatPosition::fromBeats (b0), te::BeatDuration::fromBeats (juce::jmax (0.05, b1 - b0 + 0.02)), 127, 0, um);   // a hair past the next slice: the slice ends itself (32-sample fade), the note-off never cuts it
                 }
                 if (stretch) { auto params = mp->getAutomatableParameters(); if (auto* p = params[deck == 0 ? SqSynth::SqMandiblePlugin::aStretch : SqSynth::SqMandiblePlugin::bStretch]) p->setParameter (1.0f, juce::sendNotification); }
                 auto res = obj(); set (res, "clip", clip->getName()); set (res, "notes", beats.size()); set (res, "start_bar", startBar); set (res, "stretch", stretch); set (res, "deck_bpm", deckBpm); return ToolResult::ok (res);
             }
             if (action == "to_channels")
             {
                 auto smp = mp->deckSample (deck); if (smp == nullptr) return ToolResult::error ("No file on the deck");
                 auto f = mp->deckFile (deck); auto dir = f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension() + " slices"); dir.createDirectory();
                 juce::WavAudioFormat wav; int made = 0; juce::StringArray names;
                 for (int i = 0; i < mp->numSlices (deck); ++i)
                 {
                     auto s = mp->slice (deck, i); const int st = (int) s.getProperty ("start"), en = (int) s.getProperty ("end");
                     auto out = dir.getChildFile ("slice " + juce::String (i + 1).paddedLeft ('0', 2) + ".wav"); out.deleteFile();
                     if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
                         if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.release(), smp->sampleRate, (unsigned) smp->buffer.getNumChannels(), 24, {}, 0)))
                             writer->writeFromAudioSampleBuffer (smp->buffer, st, en - st);
                     if (auto ch = Rack::addSampleChannel (session, out, f.getFileNameWithoutExtension() + " " + juce::String (i + 1))) { ++made; names.add (ch->getName()); }
                 }
                 return ToolResult::ok ("Made " + juce::String (made) + " rack channels: " + names.joinIntoString (", "));
             }
             return ToolResult::error ("Unknown action " + action);
         });

    //==========================================================================
    add ("time_weaver",
         "Time Weaver (track, slot). Actions: slots - the 36 time and 36 volume factory slots; copy (which time|volume, slot 1-36) - copy a factory slot into the editable USER steps; "
         "steps (which, values[32]) - write the USER steps directly (time steps in beats 0-8, volume 0-1). Slots are also plain parameters (Time Slot, Volume Slot; 0 = USER) and MIDI notes C1.. / C4.. pick them live.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["slots","copy","steps"]},"track":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"which":{"type":"string"},"slot_index":{"type":"integer"},"values":{"type":"array","items":{"type":"number"}}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             const auto action = str (a, "action");
             if (action == "slots")
             {
                 auto res = obj(); set (res, "time", juce::var (SqTimeWeaverPlugin::timeSlotNames())); set (res, "volume", juce::var (SqTimeWeaverPlugin::volumeSlotNames())); return ToolResult::ok (res);
             }
             juce::String err; auto p = dynamic_cast<SqTimeWeaverPlugin*> (insertArg (a, err)); if (p == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "That slot is not a Time Weaver");
             const bool time = ! str (a, "which").toLowerCase().startsWith ("v");
             if (action == "copy") { p->copySlotToUser (time, (int) num (a, "slot_index", 1)); return ToolResult::ok ("Copied into the USER steps"); }
             if (action == "steps")
             {
                 auto* arr = a["values"].getArray(); if (arr == nullptr) return ToolResult::error ("Give values[32]");
                 auto params = p->getAutomatableParameters();
                 for (int i = 0; i < juce::jmin (32, arr->size()); ++i) if (auto* ap = params[(time ? SqTimeWeaverPlugin::tStepFirst : SqTimeWeaverPlugin::vStepFirst) + i]) ap->setParameter ((float) (double) (*arr)[i], juce::sendNotification);
                 if (auto* ap = params[time ? SqTimeWeaverPlugin::timeSlot : SqTimeWeaverPlugin::volSlot]) ap->setParameter (0.0f, juce::sendNotification);
                 return ToolResult::ok ("Wrote " + juce::String (juce::jmin (32, arr->size())) + " steps");
             }
             return ToolResult::error ("Unknown action " + action);
         });
    //==========================================================================
    add ("cut_lab",
         "Cut Lab, the audio editor, for Claude: edits audio files offline (no window needed). Actions: info (file) - length, rate, channels, peak, RMS, loop and markers; "
         "edit (file, ops[], output) - run a chain of operations on the file and write the result (output defaults to '<name> edit.wav' next to it; in_place true overwrites). "
         "Each op: {op, start_seconds, end_seconds (default the whole file), ...}. ops: normalise (peak_db, default -0.3), gain (db), silence, fade_in, fade_out, reverse, remove_dc, "
         "trim (keep only the range), cut (remove the range), denoise (threshold_db 6, reduction_db -18), blur (amount_seconds 0.08, mix 1), convolve (ir file, mix), "
         "stretch (ratio = new length / old, semitones - time and pitch independently), loop (sets the WAV loop points to the range), marker (name - adds a cue at start_seconds). "
         "open (file, or track + clip) - shows it in the Cut Lab window. Send the result to the timeline with import_audio.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["info","edit","open"]},"file":{"type":"string"},"output":{"type":"string"},"in_place":{"type":"boolean"},"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"ops":{"type":"array","items":{"type":"object"}}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             const auto action = str (a, "action");
             auto f = fileArg181 (a);
             if (action == "open")
             {
                 if (! f.existsAsFile() && has (a, "track"))
                 {
                     juce::String err; auto t = trackArg (a, err); if (t == nullptr) return ToolResult::error (err);
                     const int ci = has (a, "clip") ? (int) num (a, "clip", 1) - 1 : 0; int k = 0;
                     for (auto c : t->getClips()) if (auto w = dynamic_cast<te::WaveAudioClip*> (c)) { if (k++ == ci) { f = w->getSourceFileReference().getFile(); break; } }
                 }
                 if (! f.existsAsFile()) return ToolResult::error ("Give file (a WAV / AIFF / FLAC) or track + clip");
                 if (! ui.openCutLab) return ToolResult::error ("Cut Lab needs the app window (the file tools - info, edit - work without it)");
                 ui.openCutLab (f); return ToolResult::ok ("Opened " + f.getFileName() + " in Cut Lab");
             }
             if (! f.existsAsFile()) return ToolResult::error ("Give file: an audio file");
             AudioEditor::Doc d; if (! AudioEditor::Ops::load (d, f)) return ToolResult::error ("Could not read " + f.getFileName());
             auto describe = [] (const AudioEditor::Doc& doc, const juce::File& file)
             {
                 auto o = obj(); float pk = 0; double sq = 0;
                 for (int c = 0; c < doc.buf.getNumChannels(); ++c) { pk = juce::jmax (pk, doc.buf.getMagnitude (c, 0, doc.length())); auto* p = doc.buf.getReadPointer (c); for (int i = 0; i < doc.length(); ++i) sq += (double) p[i] * p[i]; }
                 const double rms = std::sqrt (sq / juce::jmax (1.0, (double) doc.length() * doc.buf.getNumChannels()));
                 set (o, "file", file.getFullPathName()); set (o, "seconds", doc.length() / doc.sr); set (o, "samples", doc.length()); set (o, "sample_rate", doc.sr); set (o, "channels", doc.buf.getNumChannels());
                 set (o, "peak_db", juce::Decibels::gainToDecibels (pk, -150.0f)); set (o, "rms_db", juce::Decibels::gainToDecibels ((float) rms, -150.0f));
                 if (doc.loopEnd > doc.loopStart && doc.loopStart >= 0) { set (o, "loop_start_seconds", doc.loopStart / doc.sr); set (o, "loop_end_seconds", doc.loopEnd / doc.sr); }
                 juce::Array<juce::var> m; for (auto& r : doc.regions) { auto x = obj(); set (x, "name", r.name); set (x, "seconds", r.start / doc.sr); m.add (x); } set (o, "markers", m);
                 return o;
             };
             if (action == "info") return ToolResult::ok (describe (d, f));
             if (action != "edit") return ToolResult::error ("Unknown action " + action);
             auto* ops = a["ops"].getArray(); if (ops == nullptr || ops->isEmpty()) return ToolResult::error ("Give ops: a list like [{\"op\":\"normalise\",\"peak_db\":-1}]");
             juce::StringArray done;
             for (auto& op : *ops)
             {
                 const auto name = op["op"].toString().toLowerCase().replaceCharacter (' ', '_');
                 auto secs = [&] (const char* k, double def) { return op.hasProperty (k) ? (double) op[k] : def; };
                 int s0 = juce::jlimit (0, d.length(), (int) std::lround (secs ("start_seconds", 0.0) * d.sr));
                 int s1 = juce::jlimit (0, d.length(), (int) std::lround (secs ("end_seconds", d.length() / d.sr) * d.sr));
                 if (s1 <= s0 && name != "marker") return ToolResult::error (name + ": the range is empty");
                 auto val = [&] (const char* k, double def) { return op.hasProperty (k) ? (double) op[k] : def; };
                 if (name == "normalise" || name == "normalize") AudioEditor::Ops::normalise (d, s0, s1, (float) val ("peak_db", -0.3));
                 else if (name == "gain") AudioEditor::Ops::gain (d, s0, s1, (float) val ("db", 0.0));
                 else if (name == "silence") AudioEditor::Ops::silence (d, s0, s1);
                 else if (name == "fade_in") AudioEditor::Ops::fade (d, s0, s1, true);
                 else if (name == "fade_out") AudioEditor::Ops::fade (d, s0, s1, false);
                 else if (name == "reverse") AudioEditor::Ops::reverse (d, s0, s1);
                 else if (name == "remove_dc") AudioEditor::Ops::removeDc (d, s0, s1);
                 else if (name == "trim") AudioEditor::Ops::trim (d, s0, s1);
                 else if (name == "cut") AudioEditor::Ops::cut (d, s0, s1);
                 else if (name == "denoise") AudioEditor::Ops::denoise (d, s0, s1, (float) val ("threshold_db", 6.0), (float) val ("reduction_db", -18.0));
                 else if (name == "blur") AudioEditor::Ops::blur (d, s0, s1, (float) val ("amount_seconds", 0.08), (float) val ("mix", 1.0));
                 else if (name == "convolve") { juce::File ir (op["ir"].toString()); if (! AudioEditor::Ops::convolve (d, s0, s1, ir, (float) val ("mix", 1.0))) return ToolResult::error ("convolve: could not read the impulse response"); }
                 else if (name == "stretch") { if (! AudioEditor::Ops::stretch (session.engine, d, s0, s1, (float) val ("ratio", 1.0), (float) val ("semitones", 0.0))) return ToolResult::error ("stretch: no time-stretch engine available"); }
                 else if (name == "loop") { d.loopStart = s0; d.loopEnd = s1; }
                 else if (name == "marker") d.regions.push_back ({ s0, s0, op.hasProperty ("name") ? op["name"].toString() : "Marker " + juce::String ((int) d.regions.size() + 1) });
                 else return ToolResult::error ("Unknown op " + name);
                 done.add (name);
             }
             auto out = flag (a, "in_place") ? f : (has (a, "output") ? juce::File (juce::File::isAbsolutePath (str (a, "output")) ? str (a, "output") : f.getSiblingFile (str (a, "output")).getFullPathName())
                                                                   : f.getSiblingFile (f.getFileNameWithoutExtension() + " edit.wav"));
             if (out.getFileExtension().isEmpty()) out = out.withFileExtension ("wav");
             if (! AudioEditor::Ops::save (d, out, 24)) return ToolResult::error ("Could not write " + out.getFullPathName());
             auto res = describe (d, out); set (res, "ops", juce::var (done)); return ToolResult::ok (res);
         });
    //==========================================================================
    add ("licence",
         "What this copy of Squelchmoth owns (1.9 editions): edition, owned products, Demo limits, and what is locked. "
         "Demo = free forever with the basics (16 tracks, 16 basic effects, 6 instruments, kits 1-5). Squelchmoth ($189.99) unlocks the full core; packs add Vocal Suite, Mastering Suite, "
         "Instrument Collection, Beat Lab Expansion, Creative FX; Complete has everything. Keys are entered by the user in Help > Licences and Upgrades (never through this tool). "
         "action: status (default) | locked (every locked plug-in with the product that unlocks it).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["status","locked"]}}})json",
         [this] (const juce::var& a)
         {
             if (Edition::productTypes().isEmpty()) for (auto& e : Signature::entries()) Edition::productTypes().addIfNotAlreadyThere (e.type);
             auto res = obj();
             set (res, "edition", Edition::editionName()); set (res, "demo", Edition::isDemo());
             juce::Array<juce::var> owned;
             auto& o = Edition::owned();
             if (o.core) owned.add ("Squelchmoth"); if (o.vocal) owned.add ("Vocal Suite"); if (o.mastering) owned.add ("Mastering Suite");
             if (o.instruments) owned.add ("Instrument Collection"); if (o.beat) owned.add ("Beat Lab Expansion"); if (o.creative) owned.add ("Creative FX");
             set (res, "owns", owned);
             set (res, "max_tracks", Edition::isDemo() ? juce::var (Edition::demoMaxTracks) : juce::var ("unlimited"));
             set (res, "store", Edition::storeUrl());
             if (str (a, "action") == "locked")
             {
                 juce::Array<juce::var> locked;
                 for (auto& e : Signature::entries()) if (! Edition::isTypeAllowed (e.type)) { auto x = obj(); set (x, "plugin", juce::String (e.name)); set (x, "unlocked_by", juce::String (Edition::productForType (e.type)->name)); set (x, "price", juce::String (Edition::productForType (e.type)->price)); locked.add (x); }
                 set (res, "locked", locked);
                 set (res, "airwindows_collection", Edition::isDemo() ? "locked (part of Squelchmoth)" : "unlocked");
             }
             return ToolResult::ok (res);
         });
}
