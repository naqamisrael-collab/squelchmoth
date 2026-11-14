// Squelchmoth 1.15 - the Beat Architect tools: `build_beat` and `style_card` (Beat Intelligence A1). Included inside the McpTools class body.

static int firstNumber (const juce::String& s)
{
    for (int i = 0; i < s.length(); ++i) if (juce::CharacterFunctions::isDigit (s[i])) return s.substring (i).getIntValue();
    return -1;
}

void registerArchitectTools()
{
    add ("style_card",
         "The Beat Architect's style cards: what a style is made of (tempo range, half-time, swing, kit, kick and snare rows, hat rate and rolls, 808 behaviour, key and chord extensions, "
         "chord rhythm, sounds, song form, master loudness). action list (names) or get (style = a name or words like 'dark drill', 'smooth r&b').",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","get"]},"style":{"type":"string"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             if (str (a, "action", has (a, "style") ? "get" : "list") == "list")
             {
                 juce::StringArray n; for (auto& c : Architect::cards()) n.add (juce::String (c.name) + " (" + juce::String (c.bpmLo) + "-" + juce::String (c.bpmHi) + " BPM)");
                 return ToolResult::ok ("Style cards: " + n.joinIntoString (", "));
             }
             return ToolResult::ok (juce::JSON::toString (Architect::cardJson (Architect::cardFor (str (a, "style")))));
         });

    add ("build_beat",
         "The Beat Architect: writes a whole arranged, original beat into the session in one call - markers for every section, a drum track (the style's kit) with per-section MIDI "
         "(velocity motion, ghost notes, open hats, hat rolls, fills, a kick drop before each hook), a Moth 808 line on the kick and the chord roots with glides, voice-led chords, a pad, "
         "a motif-based melody and a counter-melody, with starting levels and pans. Every part is editable MIDI.\n"
         " style: a style card name or words ('trap', 'dark drill', 'smooth r&b', 'boom bap', 'afrobeats'...; style_card list). \n"
         " from_reference true: build from the last analyze_reference blueprint instead (or reference_file / reference_track): its tempo, key, swing, section map, kick / snare skeleton, "
         "hat rate, rolls, 808 glides and chord functions, with new parts. \n"
         " Optional: bpm, key ('A', 'F#'...), minor (bool), bars (shorter form), seed (another number = another beat), start_bar (default 1), preview true = return the plan without writing.\n"
         "After it: listen (analyze_mix or analyze_reference compare), then refine like a producer - swap sounds (instrument preset), rewrite a part (add_midi_notes replace), mix with the role chains, master.",
         R"json({"type":"object","properties":{"style":{"type":"string"},"from_reference":{"type":"boolean"},"reference_file":{"type":"string"},"reference_track":{"type":["string","integer"]},
            "bpm":{"type":"number"},"key":{"type":"string"},"minor":{"type":"boolean"},"bars":{"type":"integer"},"seed":{"type":"integer"},"start_bar":{"type":"integer"},"preview":{"type":"boolean"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             if (session.edit == nullptr) return ToolResult::error ("No session open (new_session first)");
             Architect::Request rq; rq.style = str (a, "style"); rq.bpm = num (a, "bpm", 0.0); rq.seed = (juce::uint32) juce::jmax (1, (int) num (a, "seed", 1)); rq.bars = (int) num (a, "bars", 0);
             if (has (a, "minor")) rq.minor = flag (a, "minor") ? 1 : 0;
             if (has (a, "key"))
             {
                 auto k = str (a, "key").trim(); const bool m = k.endsWithIgnoreCase ("m") || k.containsIgnoreCase ("min");
                 k = k.upToFirstOccurrenceOf (" ", false, false); if (k.endsWithIgnoreCase ("m") && k.length() > 1) k = k.dropLastCharacters (1);
                 static const char* flats[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" }; static const char* sharps[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                 for (int i = 0; i < 12; ++i) if (k.equalsIgnoreCase (flats[i]) || k.equalsIgnoreCase (sharps[i])) rq.keyRoot = i;
                 if (rq.keyRoot < 0) return ToolResult::error ("key: a note name like A, F# or Bb (add minor true/false)");
                 if (m && ! has (a, "minor")) rq.minor = 1;
             }
             std::shared_ptr<RefListen::Job> job; juce::String err;
             if (has (a, "reference_file") || has (a, "reference_track"))
             {
                 juce::var b (new juce::DynamicObject());
                 if (has (a, "reference_file")) b.getDynamicObject()->setProperty ("file", a["reference_file"]);
                 if (has (a, "reference_track")) b.getDynamicObject()->setProperty ("track", a["reference_track"]);
                 job = referenceJob (b, "file", "track", err); if (job == nullptr) return ToolResult::error (err);
             }
             else if (flag (a, "from_reference")) { job = RefListen::Store::get().latest(); if (job == nullptr) return ToolResult::error ("No reference analysed yet - analyze_reference first, or pass reference_file / reference_track"); }
             if (job != nullptr) { if (! waitFor (job, err)) return ToolResult::error (err); rq.reference = &job->result; }

             const auto plan = Architect::generate (rq);
             if (flag (a, "preview")) return ToolResult::ok (Architect::describe (plan));

             const int off = juce::jmax (1, (int) num (a, "start_bar", 1)) - 1;
             auto J = [] (const juce::String& json) { return juce::JSON::parse (json); };
             juce::StringArray problems;
             auto run = [&] (const juce::String& tool, const juce::var& args) -> juce::String { auto r = call (tool, args); if (r.isError) problems.addIfNotAlreadyThere (tool + ": " + r.text.substring (0, 120)); return r.text; };

             { juce::var t (new juce::DynamicObject()); set (t, "bpm", plan.bpm); run ("set_tempo", t); }
             session.edit->state.setProperty ("sqmKeyRoot", plan.keyRoot, &session.edit->getUndoManager());
             session.edit->state.setProperty ("sqmKeyScale", plan.minor ? 2 : 1, &session.edit->getUndoManager());
             for (auto& m : plan.sections) { juce::var t (new juce::DynamicObject()); set (t, "name", m.name); set (t, "bar", m.bar + off); set (t, "length_bars", m.bars); run ("add_marker", t); }

             juce::StringArray made;
             for (auto& tr : plan.tracks)
             {
                 int index = -1;
                 if (tr.role == "drums")
                 {
                     juce::var t (new juce::DynamicObject()); set (t, "name", tr.name); set (t, "bars", 1); set (t, "kit", plan.kit);
                     index = firstNumber (run ("add_drum_track", t).fromFirstOccurrenceOf ("track", false, false));
                     if (index > 0) { juce::var d (new juce::DynamicObject()); set (d, "track", index); set (d, "clip", 1); call ("delete_clip", d); }   // the starter step pattern
                 }
                 else
                 {
                     juce::var t (new juce::DynamicObject()); set (t, "kind", "instrument"); set (t, "name", tr.name); set (t, "instrument", tr.instrument);
                     index = firstNumber (run ("add_track", t));
                     if (index > 0 && tr.preset.isNotEmpty()) { juce::var pr (new juce::DynamicObject()); set (pr, "action", "preset"); set (pr, "track", index); set (pr, "name", tr.preset); run ("instrument", pr); }
                 }
                 if (index <= 0) { problems.add ("Couldn't add " + tr.name); continue; }
                 int notes = 0;
                 for (auto& c : tr.clips)
                 {
                     juce::var cv (new juce::DynamicObject()); set (cv, "track", index); set (cv, "start_bar", c.startBar + off); set (cv, "length_bars", c.bars); set (cv, "name", c.name);
                     juce::Array<juce::var> ns;
                     for (auto& n : c.notes) { juce::var nv (new juce::DynamicObject()); set (nv, "pitch", n.pitch); set (nv, "start_beat", n.start); set (nv, "length_beats", n.length); set (nv, "velocity", n.velocity); ns.add (nv); }
                     set (cv, "notes", ns); run ("create_midi_clip", cv); notes += (int) c.notes.size();
                 }
                 { juce::var u (new juce::DynamicObject()); set (u, "track", index); set (u, "volume_db", tr.volumeDb); set (u, "pan", tr.pan); run ("update_track", u); }
                 made.add (juce::String (index) + " " + tr.name + " (" + juce::String (notes) + " notes)");
             }
             juce::ignoreUnused (J);
             if (ui.refresh) ui.refresh();
             juce::String out = Architect::describe (plan) + "\nWritten: tracks " + made.joinIntoString (", ") + ".";
             if (! problems.isEmpty()) out << "\nProblems: " << problems.joinIntoString ("; ");
             out << "\nNext: play it; " << (plan.fromReference ? "analyze_reference action compare to see the gaps against the reference; " : "analyze_mix to check levels; ")
                 << "then refine sounds, parts and the mix (target " << juce::String (plan.lufs, 0) << " LUFS at mastering). Another seed gives a different beat.";
             return ToolResult::ok (out);
         });
}
