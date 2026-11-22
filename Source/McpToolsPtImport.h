// Squelchmoth 1.54 - import a Pro Tools session (included inside the tool server class).
void registerPtImportTools()
{
    add ("import_pro_tools",
         "Opens a Pro Tools session file (.ptx, .ptf, .pts; versions 5 to 12) and rebuilds it here: one track per audio track with its clips in place (which file, where on the timeline, from where in the file, how long), and its MIDI notes on instrument tracks. "
         "path: the session file; its audio is looked for in the 'Audio Files' folder beside it (audio_folder overrides). preview true only reports what is in it. bpm: the session's tempo if you know it - "
         "sessions placed by bars and beats do not give their tempo to the reader, so it is worked out from the clips. "
         "What does NOT come across: fades, volume, pan, automation, plug-ins, sends, groups, tempo changes, video. Clips whose audio file is missing are listed, not placed. Recordings the reader cannot find on a track (it happens) are still brought in, on tracks marked '(place me)' at 0 s. "
         "It adds to the open session (use new_session first for a clean start) and sets the session tempo. Experimental: it rests on a community-made reader of an undocumented format.",
         R"json({"type":"object","properties":{"path":{"type":"string"},"audio_folder":{"type":"string"},"bpm":{"type":"number"},"preview":{"type":"boolean"},"midi":{"type":"boolean"}},"required":["path"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const juce::File f (str (a, "path")); if (! f.existsAsFile()) return ToolResult::error ("No file at " + f.getFullPathName());
             juce::String err; const auto json = a.hasProperty ("_listing") ? a["_listing"].toString() : PtImport::list (f, err); if (json.isEmpty()) return ToolResult::error (err);
             const auto folder = str (a, "audio_folder").isNotEmpty() ? juce::File (str (a, "audio_folder")) : f.getParentDirectory();
             auto p = PtImport::plan (json, folder, num (a, "bpm", 0.0)); if (! p.ok) return ToolResult::error (p.error);
             juce::String s = PtImport::describe (p);
             if (flag (a, "preview")) { s << "\nTracks:"; for (auto& t : p.audio) s << "\n- " << t.name << ": " << (int) t.clips.size() << (t.clips.size() == 1 ? " clip" : " clips") << ", first at " << juce::String (t.clips.front().start, 2) << " s"; for (auto& t : p.midi) s << "\n- " << t.name << " (MIDI): " << (int) t.midi.size() << " clips"; return ToolResult::ok (s + "\nNothing was changed (preview)."); }
             auto& e = *session.edit; if (p.bpm > 0.0) { juce::var t (new juce::DynamicObject()); set (t, "bpm", p.bpm); call ("set_tempo", t); }
             int placed = 0, tracksMade = 0, notesMade = 0;
             for (auto& t : p.audio)
             {
                 juce::var nt (new juce::DynamicObject()); set (nt, "kind", "audio"); set (nt, "name", t.name.isNotEmpty() ? t.name : juce::String ("Pro Tools track")); const int idx = firstNumber (call ("add_track", nt).text); if (idx <= 0) continue; ++tracksMade;
                 auto* tr = dynamic_cast<te::AudioTrack*> (session.findTrack (juce::var (idx))); if (tr == nullptr) continue;
                 for (auto& c : t.clips) if (c.found.existsAsFile() && c.length > 0.0)
                     if (tr->insertWaveClip (c.name.isNotEmpty() ? c.name : c.found.getFileNameWithoutExtension(), c.found, { { te::TimePosition::fromSeconds (c.start), te::TimeDuration::fromSeconds (c.length) }, te::TimeDuration::fromSeconds (c.offset) }, false) != nullptr) ++placed;
             }
             int loose = 0;
             for (auto& c : p.unplaced) if (c.found.existsAsFile())       // kept, on a track of its own from 0 s, so nothing recorded is lost - but its position must be set by hand
             {
                 juce::var nt (new juce::DynamicObject()); set (nt, "kind", "audio"); set (nt, "name", c.name + " (place me)"); const int idx = firstNumber (call ("add_track", nt).text); if (idx <= 0) continue; ++tracksMade;
                 juce::var im (new juce::DynamicObject()); set (im, "track", idx); set (im, "path", c.found.getFullPathName()); set (im, "start_seconds", 0.0); if (! call ("import_audio", im).isError) ++loose;
             }
             if (flag (a, "midi", true)) for (auto& t : p.midi)
             {
                 juce::var nt (new juce::DynamicObject()); set (nt, "kind", "instrument"); set (nt, "name", t.name.isNotEmpty() ? t.name : juce::String ("Pro Tools MIDI")); const int idx = firstNumber (call ("add_track", nt).text); if (idx <= 0) continue; ++tracksMade;
                 for (auto& mc : t.midi)
                 {
                     double len = 1.0; juce::Array<juce::var> notes; for (auto& n : mc.notes) { juce::var o (new juce::DynamicObject()); set (o, "pitch", n.pitch); set (o, "start_beat", n.beat); set (o, "length_beats", n.beats); set (o, "velocity", n.velocity); notes.add (o); len = std::max (len, n.beat + n.beats); }
                     juce::var mcv (new juce::DynamicObject()); set (mcv, "track", idx); set (mcv, "start_seconds", mc.startBeat * 60.0 / juce::jmax (20.0, p.bpm)); set (mcv, "length_beats", std::ceil (len)); set (mcv, "name", mc.name); mcv.getDynamicObject()->setProperty ("notes", notes);
                     if (! call ("create_midi_clip", mcv).isError) notesMade += notes.size();
                 }
             }
             juce::ignoreUnused (e); if (ui.refresh) ui.refresh();
             s << "\nBuilt " << tracksMade << " tracks; placed " << placed << " of " << p.clips << " audio clips" << (notesMade > 0 ? " and " + juce::String (notesMade) + " MIDI notes" : juce::String()) << "." << (loose > 0 ? " " + juce::String (loose) + (loose == 1 ? " file" : " files") + " whose position the reader could not find went onto " + (loose == 1 ? "a track" : "tracks") + " marked '(place me)' at 0 s - slide " + (loose == 1 ? "it" : "them") + " into place by ear." : juce::String())
               << " Not brought across: fades, volume, pan, automation, plug-ins, sends. Check a few clip positions against the original before trusting the rest.";
             return ToolResult::ok (s);
         });
}
