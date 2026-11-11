// Squelchmoth 1.13 - Claude's `score` tool: read MIDI as notation, set the display, write notes by name, export /
// import MusicXML, PDF and PNG. Included inside the McpTools class body.

void registerScoreTools()
{
    add ("score",
         "The Score (notation) of MIDI tracks. action 'read' (default): every bar as text - spelled note names, durations (quarter, dotted eighth, triplet eighth...), ties (~), rests, key and clef. "
         "'set': display options grid (4|8|16|32), triplets (bool), staves (auto|treble|bass|grand, all tracks or 'track'), key ('Eb major', 'F# minor' or 'session'), spelling (key|sharps|flats). "
         "'write': add notes to 'track' from bar 'bar' (1-based, default 1) in order - notes: ['C4 quarter', 'E4 eighth', 'rest eighth', '[C4 E4 G4] half', 'Bb3 dotted quarter', 'D5 triplet eighth', 'F#4 1/16'], "
         "replace (bool) clears those bars first. 'export': format musicxml|pdf|png, path optional (default the Bounces folder). 'import': path of a .musicxml / .xml file (adds one instrument track per part). "
         "'open': shows the Score window.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["read","set","write","export","import","open"]},"track":{"type":["string","integer"]},"tracks":{"type":"array","items":{"type":["string","integer"]}},
                "bars":{"type":"integer","description":"read: at most this many bars (default 64)"},"grid":{"type":"integer"},"triplets":{"type":"boolean"},"staves":{"type":"string"},"key":{"type":"string"},"spelling":{"type":"string"},
                "bar":{"type":"number"},"notes":{"type":"array","items":{"type":"string"}},"replace":{"type":"boolean"},"format":{"type":"string","enum":["musicxml","pdf","png"]},"path":{"type":"string"},"letter":{"type":"boolean"}},"required":[]})json",
         [this] (const juce::var& a)
         {
             if (session.edit == nullptr) return ToolResult::error ("No session open");
             const auto act = str (a, "action", "read");
             juce::Array<te::AudioTrack*> tracks;
             juce::String err;
             if (auto arr = a["tracks"].getArray()) { for (auto& v : *arr) if (auto t = dynamic_cast<te::AudioTrack*> (session.findTrack (v))) tracks.add (t); }
             else if (has (a, "track")) { if (auto t = trackArg (a, err)) tracks.add (t); else return ToolResult::error (err); }
             auto& um = session.edit->getUndoManager();

             if (act == "open") { if (ui.showWindow) ui.showWindow ("score", true); return ToolResult::ok ("Score window open"); }

             if (act == "set")
             {
                 juce::StringArray done;
                 if (has (a, "grid")) { const int g = (int) num (a, "grid"); if (g != 4 && g != 8 && g != 16 && g != 32) return ToolResult::error ("grid: 4, 8, 16 or 32"); ScoreModel::setOption (session, "grid", g); done.add ("grid 1/" + juce::String (g)); }
                 if (has (a, "triplets")) { ScoreModel::setOption (session, "triplets", flag (a, "triplets")); done.add (flag (a, "triplets") ? "triplets on" : "triplets off"); }
                 if (has (a, "spelling")) { auto sp = str (a, "spelling").toLowerCase(); ScoreModel::setOption (session, "spelling", sp.startsWith ("sharp") ? 1 : sp.startsWith ("flat") ? -1 : 0); done.add ("spelling " + sp); }
                 if (has (a, "staves"))
                 {
                     auto sv = str (a, "staves").toLowerCase();
                     const int m = sv.startsWith ("treble") ? 1 : sv.startsWith ("bass") ? 2 : sv.startsWith ("grand") || sv.startsWith ("piano") ? 3 : 0;
                     if (! tracks.isEmpty()) for (auto t : tracks) t->state.setProperty ("sqmScoreStaves", m, &um);
                     else ScoreModel::setOption (session, "staves", m);
                     done.add ("staves " + sv);
                 }
                 if (has (a, "key"))
                 {
                     auto v = ScoreModel::optionsTree (session);
                     if (str (a, "key").equalsIgnoreCase ("session")) { v.removeProperty ("fifths", nullptr); v.removeProperty ("minor", nullptr); }
                     else
                     {
                         Notation::Key k; if (! ScoreModel::parseKey (str (a, "key"), k)) return ToolResult::error ("key: e.g. 'Eb major', 'F# minor' or 'session'");
                         v.setProperty ("fifths", k.fifths, nullptr); v.setProperty ("minor", k.minor, nullptr);
                     }
                     done.add ("key " + ScoreModel::keyName (ScoreModel::key (session)));
                 }
                 return ToolResult::ok ("Score: " + done.joinIntoString (", "));
             }

             if (act == "write")
             {
                 if (tracks.size() != 1) return ToolResult::error ("write needs one 'track'");
                 auto t = tracks[0];
                 auto& ts = session.edit->tempoSequence;
                 const int bar = juce::jmax (1, (int) num (a, "bar", 1.0)) - 1;
                 double beat = ts.toBeats (ts.toTime (te::tempo::BarsAndBeats { bar, te::BeatDuration(), 0 })).inBeats();
                 const double startBeat = beat;
                 auto arr = a["notes"].getArray();
                 if (arr == nullptr || arr->isEmpty()) return ToolResult::error ("notes: e.g. [\"C4 quarter\", \"[E4 G4] half\", \"rest eighth\"]");
                 // parse everything first, so a typo writes nothing
                 struct W { std::vector<int> pitches; double len; };
                 std::vector<W> ws;
                 for (auto& v : *arr)
                 {
                     auto s = v.toString().trim();
                     W w;
                     juce::String pitchPart, durPart;
                     if (s.startsWith ("["))
                     {
                         pitchPart = s.fromFirstOccurrenceOf ("[", false, false).upToFirstOccurrenceOf ("]", false, false);
                         durPart = s.fromFirstOccurrenceOf ("]", false, false).trim();
                     }
                     else { pitchPart = s.upToFirstOccurrenceOf (" ", false, false); durPart = s.fromFirstOccurrenceOf (" ", false, false).trim(); }
                     if (! pitchPart.equalsIgnoreCase ("rest"))
                         for (auto& pn : juce::StringArray::fromTokens (pitchPart, " ,", ""))
                         {
                             if (pn.isEmpty()) continue;
                             const int p = Notation::parsePitch (pn);
                             if (p < 0) return ToolResult::error ("Can't read the note '" + pn + "' in '" + s + "' (use names like C4, F#3, Bb5)");
                             w.pitches.push_back (p);
                         }
                     w.len = Notation::parseDuration (durPart.isEmpty() ? "quarter" : durPart);
                     if (w.len <= 0) return ToolResult::error ("Can't read the length '" + durPart + "' in '" + s + "' (whole, half, quarter, eighth, 16th, 32nd, dotted ..., triplet ..., or 3/16)");
                     ws.push_back (w);
                 }
                 double total = 0; for (auto& w : ws) total += w.len;
                 um.beginNewTransaction ("Score: write notes");
                 if (flag (a, "replace"))
                 {
                     auto p = ScoreModel::gather (session, *t);
                     std::vector<ScoreModel::NoteRef> kill;
                     for (size_t i = 0; i < p.notes.size(); ++i) if (p.notes[i].start >= startBeat - 1.0e-6 && p.notes[i].start < startBeat + total - 1.0e-6) kill.push_back (p.refs[i]);
                     ScoreModel::remove (session, kill);
                 }
                 int added = 0;
                 for (auto& w : ws)
                 {
                     for (int p : w.pitches) if (ScoreModel::addNote (session, *t, beat, w.len, p) != nullptr) ++added;
                     beat += w.len;
                 }
                 auto parts = ScoreModel::build (session, { t });
                 juce::String text = parts.empty() ? juce::String() : Notation::toText (parts[0].score, 64);
                 return ToolResult::ok ("Wrote " + juce::String (added) + " notes on " + t->getName() + " from bar " + juce::String (bar + 1) + " (" + juce::String (total, 2) + " beats). The score now reads:\n" + text);
             }

             if (tracks.isEmpty()) tracks = ScoreModel::midiTracks (session);
             if (tracks.isEmpty() && act != "import") return ToolResult::error ("No MIDI tracks yet - create_midi_clip or score write first");

             if (act == "export")
             {
                 const auto fmt = str (a, "format", "musicxml").toLowerCase();
                 const auto ext = fmt == "pdf" ? ".pdf" : fmt == "png" ? ".png" : ".musicxml";
                 juce::File dest = has (a, "path") ? juce::File (str (a, "path")) : session.bouncesDir.getNonexistentChildFile (juce::File::createLegalFileName (session.getFile().getFileNameWithoutExtension() + " score"), ext, false);
                 if (fmt == "musicxml")
                 {
                     auto parts = ScoreModel::build (session, tracks);
                     if (! dest.replaceWithText (ScoreModel::musicXml (session, parts, session.getFile().getFileNameWithoutExtension(), {}))) return ToolResult::error ("Couldn't write " + dest.getFullPathName());
                     return ToolResult::ok ("Exported MusicXML to " + dest.getFullPathName() + " (" + juce::String ((int) parts.size()) + " part(s), " + juce::String ((int) parts[0].score.measures.size()) + " bars)");
                 }
                 int pages = 0;
                 if (! ScoreExport::writePages (session, tracks, dest, fmt == "pdf", flag (a, "letter"), err, &pages)) return ToolResult::error (err);
                 return ToolResult::ok ("Exported " + juce::String (pages) + " page(s) as " + fmt.toUpperCase() + " to " + dest.withFileExtension (ext).getFullPathName());
             }

             if (act == "import")
             {
                 juce::File f (str (a, "path"));
                 if (! f.existsAsFile()) return ToolResult::error ("path: a .musicxml or .xml file");
                 um.beginNewTransaction ("Import MusicXML");
                 auto made = ScoreModel::importMusicXml (session, f, err);
                 if (made.isEmpty()) return ToolResult::error (err);
                 juce::StringArray names; for (auto t : made) names.add (t->getName());
                 return ToolResult::ok ("Imported " + juce::String (made.size()) + " part(s) as tracks: " + names.joinIntoString (", "));
             }

             // read
             auto parts = ScoreModel::build (session, tracks);
             juce::String out;
             const int maxBars = (int) num (a, "bars", 64.0);
             for (auto& p : parts) out << "== " << p.name << " ==\n" << Notation::toText (p.score, maxBars) << "\n";
             return ToolResult::ok (out);
         });
}
