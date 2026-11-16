// Squelchmoth 1.17 - `audio_to_midi`: the drums and the 808 / bass line of a recording written as MIDI. Included inside the McpTools class body.

void registerNotesTools()
{
    add ("audio_to_midi",
         "Turns recorded drums and bass into MIDI, lined up with the audio: the kick, snare / clap and hi-hat hits (with velocities) onto a new drum track, and the 808 / bass notes (with glides as overlaps) onto a new Moth 808 track. "
         "Works on a full mix, and best on separated stems (run separate first, then this on the Drums and Bass tracks).\n"
         " file or track (the audio); what: drums | bass | both (default both); kit (drum kit name, default Mothwing 808); set_tempo true also sets the session tempo to the detected one; "
         "start_seconds / length_seconds optional. The notes sit at the audio's own times whatever the session tempo is.",
         R"json({"type":"object","properties":{"file":{"type":"string"},"track":{"type":["string","integer"]},"what":{"type":"string","enum":["drums","bass","both"]},"kit":{"type":"string"},
            "set_tempo":{"type":"boolean"},"start_seconds":{"type":"number"},"length_seconds":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             if (session.edit == nullptr) return ToolResult::error ("No session open");
             juce::String err; te::AudioTrack* src = nullptr;
             auto job = referenceJob (a, "file", "track", err, &src);
             if (job == nullptr) return ToolResult::error (err);
             if (! waitFor (job, err)) return ToolResult::error (err);
             const auto& r = job->result;
             double placeAt = 0;
             if (src != nullptr) for (auto c : src->getClips()) if (auto ac = dynamic_cast<te::AudioClipBase*> (c)) if (ac->getAudioFile().getFile() == job->file) { placeAt = ac->getPosition().getStart().inSeconds(); break; }
             if (flag (a, "set_tempo")) { juce::var t (new juce::DynamicObject()); set (t, "bpm", std::round (r.bpm * 100) / 100); call ("set_tempo", t); }
             const double bps = session.edit->tempoSequence.getTempo (0)->getBpm() / 60.0;      // session beats per second
             const auto what = str (a, "what", "both").toLowerCase();
             const auto base = src != nullptr ? src->getName() : job->file.getFileNameWithoutExtension();
             juce::StringArray made;
             auto writeClip = [&] (int track, const juce::String& name, const juce::Array<juce::var>& notes)
             {
                 juce::var cv (new juce::DynamicObject()); set (cv, "track", track); set (cv, "start_seconds", placeAt); set (cv, "length_beats", std::ceil (r.seconds * bps)); set (cv, "name", name); set (cv, "notes", notes);
                 return ! call ("create_midi_clip", cv).isError;
             };
             auto note = [] (int pitch, double startBeat, double lenBeats, int vel)
             {
                 juce::var nv (new juce::DynamicObject()); set (nv, "pitch", pitch); set (nv, "start_beat", startBeat); set (nv, "length_beats", juce::jmax (0.05, lenBeats)); set (nv, "velocity", juce::jlimit (1, 127, vel)); return nv;
             };
             int kicks = 0, snares = 0, hats = 0, bassNotes = 0;
             if (what != "bass")
             {
                 juce::Array<juce::var> ns;
                 for (size_t b = 0; b < r.drums.size() && b + 1 < r.bars.size(); ++b)
                 {
                     const double step = (r.bars[b + 1] - r.bars[b]) / 16.0;
                     for (int i = 0; i < 16; ++i)
                     {
                         const double t = (r.bars[b] + i * step) * bps; const auto& d = r.drums[b];
                         if (d.kick[(size_t) i] > 0)  { ns.add (note (36, t, 0.25, juce::roundToInt (60 + 67 * d.kick[(size_t) i]))); ++kicks; }
                         if (d.snare[(size_t) i] > 0) { ns.add (note (38, t, 0.25, juce::roundToInt (60 + 67 * d.snare[(size_t) i]))); ++snares; }
                         if (d.hat[(size_t) i] > 0)   { ns.add (note (42, t, 0.2, juce::roundToInt (50 + 70 * d.hat[(size_t) i]))); ++hats; }
                     }
                 }
                 if (ns.isEmpty()) made.add ("no drum hits found");
                 else
                 {
                     juce::var t (new juce::DynamicObject()); set (t, "name", base + " drums MIDI"); set (t, "bars", 1); set (t, "kit", str (a, "kit", "Mothwing 808"));
                     const int idx = firstNumber (call ("add_drum_track", t).text.fromFirstOccurrenceOf ("track", false, false));
                     if (idx > 0)
                     {
                         juce::var d (new juce::DynamicObject()); set (d, "track", idx); set (d, "clip", 1); call ("delete_clip", d);
                         if (writeClip (idx, base + " drums", ns)) made.add ("track " + juce::String (idx) + " drums: " + juce::String (kicks) + " kicks, " + juce::String (snares) + " snares, " + juce::String (hats) + " hats");
                     }
                 }
             }
             if (what != "drums")
             {
                 juce::Array<juce::var> ns;
                 // one held note that a kick broke into pieces is one note: join same-pitch neighbours less than 0.3 s apart
                 std::vector<SqRef::SubNote> sub;
                 for (auto& n : r.sub)
                 {
                     if (! sub.empty() && ! n.glide && std::lround (n.midi) == std::lround (sub.back().midi) && n.startSec - (sub.back().startSec + sub.back().lengthSec) < 0.3)
                         sub.back().lengthSec = n.startSec + n.lengthSec - sub.back().startSec;
                     else sub.push_back (n);
                 }
                 for (size_t i = 0; i < sub.size(); ++i)
                 {
                     const auto& n = sub[i];
                     double len = n.lengthSec;
                     if (i + 1 < sub.size() && sub[i + 1].glide) len = sub[i + 1].startSec - n.startSec + 0.06;    // overlap into a glide
                     ns.add (note ((int) std::lround (n.midi), n.startSec * bps, len * bps, 108)); ++bassNotes;
                 }
                 if (ns.isEmpty()) made.add ("no bass / 808 notes found");
                 else
                 {
                     juce::var t (new juce::DynamicObject()); set (t, "kind", "instrument"); set (t, "name", base + " bass MIDI"); set (t, "instrument", "Moth 808");
                     const int idx = firstNumber (call ("add_track", t).text);
                     if (idx > 0 && writeClip (idx, base + " bass", ns)) made.add ("track " + juce::String (idx) + " bass: " + juce::String (bassNotes) + " notes, " + juce::String (r.subGlides) + " glides");
                 }
             }
             if (ui.refresh) ui.refresh();
             return ToolResult::ok ("Audio to MIDI from " + job->label + " (" + juce::String (r.bpm, 1) + " BPM, " + r.keyName + "): " + made.joinIntoString ("; ")
                                    + ". Hits are placed on the detected 16th grid (swing and rolls are not written yet); mute the audio to hear the MIDI alone.");
         });
}
