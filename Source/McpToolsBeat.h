// Squelchmoth - Claude tools for the Beat Lab (Beat Maker). Included inside the McpTools class body.

void registerBeatLabTools()
{
    add ("beat_lab",
         "Program the Beat Maker (FL-style Channel Rack) on a drum track. Actions:\n"
         " get - read patterns, chain, steps, swing and every row (x hit, X accent, g ghost, 2-8 roll, ? chance <100%).\n"
         " starter - roll a genre groove (genre: Trap, Drill, Boom Bap, Lo-Fi, R&B, Afrobeats, Amapiano, Reggaeton, Jersey Club, House, Techno, Phonk, Pop, Drum & Bass, Dancehall, Rock, Funk / Neo-Soul; set_tempo true also sets the BPM; row re-rolls one row; locked rows are kept).\n"
         " pattern - choose the pattern being edited (pattern 'A'..'H'); chain - set the play order ('A A B A'), loop_pattern true loops the edited pattern instead.\n"
         " steps - steps 1-128 and/or resolution '1/8','1/16','1/32','1/8T','1/16T'; double true doubles the pattern.\n"
         " swing - amount 0-100 (%); with row, sets that row's share of the swing (0-100).\n"
         " row - op on one row: fill (every, offset), euclid (hits, of, rotate), rotate (by), dice (density 0-1), humanise (amount 0-1), clear, mute/solo/lock (value true/false), length (steps, polyrhythm), copy, paste.\n"
         " write - rows: {name: 'x..xX.g.3...'} written into the edited pattern (x hit, X accent, g ghost, ? 50% chance, 2-8 roll); replace (default true) clears other rows.\n"
         " step - one step on a row (step is 1-based): on, velocity 1-127, gate 0-2 (1 = full step), chance 0-100, shift -50..50 (% of a step, + = late), repeat 1-16.\n"
         " to_piano_roll - turn the beat into a MIDI clip; open - show the Beat Maker window.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["get","starter","pattern","chain","steps","swing","row","write","step","to_piano_roll","open"]},
            "genre":{"type":"string"},"set_tempo":{"type":"boolean"},"row":{"type":["string","integer"]},"pattern":{"type":"string"},"chain":{"type":"string"},"loop_pattern":{"type":"boolean"},
            "steps":{"type":"integer"},"resolution":{"type":"string"},"double":{"type":"boolean"},"amount":{"type":"number"},"op":{"type":"string"},"every":{"type":"integer"},"offset":{"type":"integer"},
            "hits":{"type":"integer"},"of":{"type":"integer"},"rotate":{"type":"integer"},"by":{"type":"integer"},"density":{"type":"number"},"value":{"type":"boolean"},
            "rows":{"type":"object"},"replace":{"type":"boolean"},"step":{"type":"integer"},"on":{"type":"boolean"},"velocity":{"type":"integer"},"gate":{"type":"number"},
            "chance":{"type":"number"},"shift":{"type":"number"},"repeat":{"type":"integer"},"seed":{"type":"integer"}},"required":["track","action"]})json",
         [this] (const juce::var& a)
         {
             juce::String err;
             auto t = trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             auto clip = DrumKit::findStepClip (*t);
             if (clip == nullptr) return ToolResult::error ("Track '" + t->getName() + "' has no Beat Maker clip. Use add_drum_track first.");
             auto& c = *clip;
             BeatLab::ensurePatterns (c, 1);
             auto action = str (a, "action").toLowerCase();
             juce::Random rnd (has (a, "seed") ? (juce::int64) num (a, "seed", 0) : juce::Random::getSystemRandom().nextInt64());

             auto rowIndex = [&] () -> int
             {
                 if (! has (a, "row")) return -1;
                 return DrumKit::channelIndexFor (c, a["row"].toString());
             };
             auto rowString = [&] (te::StepClip::Pattern& p, int ch)
             {
                 juce::String s;
                 const int len = BeatLab::rowLength (p, ch);
                 for (int i = 0; i < p.getNumNotes(); ++i)
                 {
                     if (i >= len) { s << ' '; continue; }
                     if (! p.getNote (ch, i)) { s << '.'; continue; }
                     auto rep = BeatLab::getRepeat (p, ch, i);
                     if (rep > 1) { s << juce::String (juce::jmin (9, rep)); continue; }
                     if (p.getProbability (ch, i) < 0.999f) { s << '?'; continue; }
                     auto v = p.getVelocity (ch, i);
                     s << (v >= 120 ? 'X' : v < 70 ? 'g' : 'x');
                 }
                 return s;
             };

             if (action == "get")
             {
                 auto p = BeatLab::pattern (c);
                 auto o = obj();
                 set (o, "editing_pattern", BeatLab::patternName (BeatLab::editPattern (c)));
                 juce::StringArray used;
                 for (int i = 0; i < c.getNumPatterns(); ++i) if (BeatLab::patternHasNotes (c, i)) used.add (BeatLab::patternName (i));
                 set (o, "patterns_with_notes", used.joinIntoString (" "));
                 set (o, "chain", BeatLab::chainText (c));
                 set (o, "loop_pattern", BeatLab::loopsEditPattern (c));
                 set (o, "steps", p.getNumNotes());
                 set (o, "step_beats", p.getNoteLength().inBeats());
                 set (o, "swing_percent", juce::roundToInt (BeatLab::swing (c) * 100.0f));
                 set (o, "genre", c.state.getProperty ("sqmGenre").toString());
                 auto rows = obj();
                 for (int ch = 0; ch < c.getChannels().size(); ++ch)
                 {
                     juce::String extra;
                     if (BeatLab::isMuted (c, ch)) extra << " [muted]";
                     if (BeatLab::isSoloed (c, ch)) extra << " [solo]";
                     if (BeatLab::isLocked (c, ch)) extra << " [locked]";
                     if (BeatLab::rowLength (p, ch) != p.getNumNotes()) extra << " [length " << BeatLab::rowLength (p, ch) << "]";
                     set (rows, c.getChannels()[ch]->name.get(), rowString (p, ch) + extra);
                 }
                 set (o, "rows", rows);
                 return ToolResult::ok (o);
             }
             if (action == "starter")
             {
                 auto g = BeatLab::genreIndex (str (a, "genre", "Trap"));
                 if (g < 0) return ToolResult::error ("Unknown genre. Use one of the genres listed in the tool description.");
                 auto msg = BeatLab::starter (c, g, rnd, rowIndex(), flag (a, "set_tempo", false));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok ("Rolled " + msg + (flag (a, "set_tempo", false) ? ". Tempo " + juce::String (c.edit.tempoSequence.getTempo (0)->getBpm(), 1) + " BPM" : juce::String()));
             }
             if (action == "pattern")
             {
                 auto s = str (a, "pattern").trim().toUpperCase();
                 if (s.isEmpty() || s[0] < 'A' || s[0] > 'H') return ToolResult::error ("pattern must be A..H");
                 BeatLab::setEditPattern (c, s[0] - 'A');
                 if (BeatLab::loopsEditPattern (c)) BeatLab::rebuildSequence (c);
                 return ToolResult::ok ("Editing pattern " + s.substring (0, 1));
             }
             if (action == "chain")
             {
                 if (has (a, "chain")) BeatLab::setChain (c, str (a, "chain"));
                 if (has (a, "loop_pattern")) BeatLab::setLoopPattern (c, flag (a, "loop_pattern"));
                 return ToolResult::ok ("Chain: " + BeatLab::chainText (c) + (BeatLab::loopsEditPattern (c) ? " (looping pattern " + BeatLab::patternName (BeatLab::editPattern (c)) + " for now)" : juce::String()));
             }
             if (action == "steps")
             {
                 if (has (a, "resolution"))
                 {
                     auto r = str (a, "resolution").removeCharacters (" ").toUpperCase();
                     double v = r == "1/8" ? 0.5 : r == "1/32" ? 0.125 : r == "1/8T" ? 1.0 / 3.0 : r == "1/16T" ? 1.0 / 6.0 : 0.25;
                     BeatLab::setResolution (c, v);
                 }
                 if (has (a, "steps")) BeatLab::setNumSteps (c, (int) num (a, "steps", 16));
                 if (flag (a, "double")) BeatLab::doublePattern (c);
                 auto p = BeatLab::pattern (c);
                 return ToolResult::ok ("Pattern " + BeatLab::patternName (BeatLab::editPattern (c)) + ": " + juce::String (p.getNumNotes()) + " steps of " + juce::String (p.getNoteLength().inBeats(), 3) + " beats");
             }
             if (action == "swing")
             {
                 auto ch = rowIndex();
                 if (ch >= 0) { BeatLab::setSwingMix (c, ch, (float) num (a, "amount", 100) / 100.0f); return ToolResult::ok ("Row swing amount set"); }
                 BeatLab::setSwing (c, (float) num (a, "amount", 0) / 100.0f);
                 return ToolResult::ok ("Swing " + juce::String (juce::roundToInt (BeatLab::swing (c) * 100)) + "%");
             }
             if (action == "row")
             {
                 auto ch = rowIndex();
                 if (ch < 0) return ToolResult::error ("row not found (use a name like kick, snare, closed hat, or a 1-based number)");
                 auto p = BeatLab::pattern (c);
                 auto op = str (a, "op").toLowerCase();
                 if (op == "fill")          BeatLab::fillEvery (c, p, ch, juce::jmax (1, (int) num (a, "every", 4)), (int) num (a, "offset", 0));
                 else if (op == "euclid")   BeatLab::fillEuclid (c, p, ch, (int) num (a, "hits", 3), (int) num (a, "of", 8), (int) num (a, "rotate", 0));
                 else if (op == "rotate")   BeatLab::rotateRow (c, p, ch, (int) num (a, "by", 1));
                 else if (op == "dice")     BeatLab::diceRow (c, p, ch, (float) num (a, "density", 0.3), rnd);
                 else if (op == "humanise" || op == "humanize") BeatLab::humanise (c, p, ch, (float) num (a, "amount", 0.5), rnd);
                 else if (op == "clear")    BeatLab::clearRow (c, p, ch);
                 else if (op == "mute")     BeatLab::setRowFlag (c, ch, "sqmMute", flag (a, "value", true));
                 else if (op == "solo")     BeatLab::setRowFlag (c, ch, "sqmSolo", flag (a, "value", true));
                 else if (op == "lock")     BeatLab::setRowFlag (c, ch, "sqmLock", flag (a, "value", true));
                 else if (op == "length")   BeatLab::setRowLength (c, p, ch, (int) num (a, "steps", p.getNumNotes()));
                 else if (op == "copy")     BeatLab::copyRow (p, ch);
                 else if (op == "paste")    BeatLab::paste (c, ch);
                 else return ToolResult::error ("Unknown op. Use fill, euclid, rotate, dice, humanise, clear, mute, solo, lock, length, copy, paste.");
                 return ToolResult::ok (c.getChannels()[ch]->name.get() + ": " + rowString (p, ch));
             }
             if (action == "write")
             {
                 auto p = BeatLab::pattern (c);
                 if (flag (a, "replace", true))
                     for (int ch = 0; ch < c.getChannels().size(); ++ch) BeatLab::clearRow (c, p, ch);
                 juce::StringArray unknown, done;
                 int longest = 0;
                 if (auto o = a["rows"].getDynamicObject())
                     for (auto& kv : o->getProperties()) longest = juce::jmax (longest, kv.value.toString().removeCharacters (" |").length());
                 if (longest > p.getNumNotes()) { p.setNumNotes (juce::jmin (128, longest)); BeatLab::rebuildSequence (c); }
                 if (auto o = a["rows"].getDynamicObject())
                     for (auto& kv : o->getProperties())
                     {
                         auto ch = DrumKit::channelIndexFor (c, kv.name.toString());
                         if (ch < 0) { unknown.add (kv.name.toString()); continue; }
                         BeatLab::writeGrooveString (c, p, ch, kv.value.toString().removeCharacters (" |"), rnd);
                         done.add (c.getChannels()[ch]->name.get());
                     }
                 return ToolResult::ok ("Wrote " + done.joinIntoString (", ") + " into pattern " + BeatLab::patternName (BeatLab::editPattern (c))
                                        + (unknown.isEmpty() ? juce::String() : ". Unknown rows: " + unknown.joinIntoString (", ")));
             }
             if (action == "step")
             {
                 auto ch = rowIndex();
                 auto p = BeatLab::pattern (c);
                 const int i = (int) num (a, "step", 1) - 1;
                 if (ch < 0 || ! juce::isPositiveAndBelow (i, p.getNumNotes())) return ToolResult::error ("row or step out of range");
                 if (has (a, "on") && ! flag (a, "on")) { BeatLab::setStep (c, p, ch, i, false); return ToolResult::ok ("Step off"); }
                 if (! p.getNote (ch, i)) BeatLab::setStep (c, p, ch, i, true, 100);
                 if (has (a, "velocity")) p.setVelocity (ch, i, juce::jlimit (1, 127, (int) num (a, "velocity", 100)));
                 if (has (a, "gate"))     p.setGate (ch, i, juce::jlimit (0.05, 2.0, num (a, "gate", 1.0)));
                 if (has (a, "chance"))   p.setProbability (ch, i, (float) juce::jlimit (0.0, 1.0, num (a, "chance", 100) / 100.0));
                 if (has (a, "shift"))    BeatLab::setShift (c, p, ch, i, (float) (num (a, "shift", 0) / 100.0));
                 if (has (a, "repeat"))   BeatLab::setRepeat (c, p, ch, i, (int) num (a, "repeat", 1));
                 return ToolResult::ok (c.getChannels()[ch]->name.get() + ": " + rowString (p, ch));
             }
             if (action == "to_piano_roll")
             {
                 auto name = t->getName();
                 auto mc = BeatLab::sendToPianoRoll (c);
                 if (mc == nullptr) return ToolResult::error ("Could not convert");
                 return ToolResult::ok ("The beat on " + name + " is now a MIDI clip with " + juce::String (mc->getSequence().getNumNotes()) + " notes (undo restores the steps)");
             }
             if (action == "open")
             {
                 if (! ui.openClipEditor) return ToolResult::error ("UI not available");
                 ui.openClipEditor (c);
                 return ToolResult::ok ("Beat Maker open for " + t->getName());
             }
             return ToolResult::error ("Unknown action");
         });

    add ("midi_tools",
         "Piano-roll power tools on a MIDI clip (acts on every note, or only notes inside from_beat..to_beat / pitch_low..pitch_high). Actions:\n"
         " list - scales, chord types and progressions.\n"
         " key - set the clip's key (root 'C'..'B', scale e.g. 'Natural Minor', 'Major', 'Dorian', 'Minor Pentatonic').\n"
         " progression - write a chord progression (progression: name or number from list, or a mood like 'trap', 'r&b', 'lo-fi'; root key; bars_per_chord 1/2; bass true adds a bass / 808 line; replace true clears first; octave 3-5).\n"
         " chord - stamp one chord (root e.g. 'A3' or 57, type e.g. 'Minor 7', inversion 0-3, beat, length beats).\n"
         " quantize (grid '1/16', strength 0-100, swing 0-100, ends), humanize (amount light/medium/heavy), strum (speed '1/32', direction up/down), arpeggiate (rate '1/16', mode up/down/updown/random, octaves, gate 0-100),\n"
         " flam, legato, slides (glide '1/32' - 808 glides: notes overlap the next so Moth 808 glides), chop (rate '1/16'), snap_scale, transpose_scale (steps), velocity (scale %, or set 1-127), flip,\n"
         " riff (random melody in the clip's key: low 'C4', high 'G5', grid '1/16', density 0-100), open (show the MIDI editor).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"action":{"type":"string"},
            "root":{"type":["string","integer"]},"scale":{"type":"string"},"progression":{"type":["string","integer"]},"bars_per_chord":{"type":"number"},"bass":{"type":"boolean"},"replace":{"type":"boolean"},"octave":{"type":"integer"},
            "type":{"type":"string"},"inversion":{"type":"integer"},"beat":{"type":"number"},"length":{"type":"number"},"grid":{"type":"string"},"strength":{"type":"number"},"swing":{"type":"number"},"ends":{"type":"boolean"},
            "amount":{"type":"string"},"speed":{"type":"string"},"direction":{"type":"string"},"rate":{"type":"string"},"mode":{"type":"string"},"octaves":{"type":"integer"},"gate":{"type":"number"},
            "glide":{"type":"string"},"steps":{"type":"integer"},"set":{"type":"integer"},"low":{"type":["string","integer"]},"high":{"type":["string","integer"]},"density":{"type":"number"},"seed":{"type":"integer"},
            "from_beat":{"type":"number"},"to_beat":{"type":"number"},"pitch_low":{"type":["string","integer"]},"pitch_high":{"type":["string","integer"]}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto action = str (a, "action").toLowerCase();
             auto gridOf = [&] (const char* key, double def) -> double
             {
                 if (! has (a, key)) return def;
                 auto g = str (a, key).removeCharacters (" ").toUpperCase();
                 const bool trip = g.endsWithChar ('T');
                 if (trip) g = g.dropLastCharacters (1);
                 double v = def;
                 if (g.startsWith ("1/")) v = 4.0 / juce::jmax (1, g.substring (2).getIntValue());
                 else if (g.containsOnly ("0123456789.")) v = g.getDoubleValue();
                 return trip ? v * 2.0 / 3.0 : v;
             };
             if (action == "list")
             {
                 juce::StringArray sc, ch, pr;
                 for (auto& x : MidiTools::scales()) sc.add (x.name);
                 for (auto& x : MidiTools::chordTypes()) ch.add (x.name);
                 for (int i = 0; i < (int) MidiTools::progressions().size(); ++i) pr.add (juce::String (i + 1) + ". " + MidiTools::progressions()[(size_t) i].name + " [" + MidiTools::progressions()[(size_t) i].mood + "]");
                 return ToolResult::ok ("Scales: " + sc.joinIntoString (", ") + "\nChords: " + ch.joinIntoString (", ") + "\nProgressions:\n" + pr.joinIntoString ("\n"));
             }
             juce::String err;
             auto t = trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             te::MidiClip* mc = nullptr;
             if (has (a, "clip")) mc = dynamic_cast<te::MidiClip*> (clipArg (*t, a, err));
             else for (auto c : t->getClips()) if ((mc = dynamic_cast<te::MidiClip*> (c)) != nullptr) break;
             if (mc == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "No MIDI clip on this track - create_midi_clip first.");
             auto& c = *mc;
             auto& undo = c.edit.getUndoManager();
             int root = (int) c.state.getProperty ("sqmScaleRoot", 0), scale = (int) c.state.getProperty ("sqmScale", 0);

             // which notes
             MidiTools::Notes sel;
             const bool filtered = has (a, "from_beat") || has (a, "to_beat") || has (a, "pitch_low") || has (a, "pitch_high");
             if (filtered)
             {
                 const double fb = num (a, "from_beat", -1e9), tb = num (a, "to_beat", 1e9);
                 const int pl = has (a, "pitch_low") ? parsePitch (a["pitch_low"]) : 0, ph = has (a, "pitch_high") ? parsePitch (a["pitch_high"]) : 127;
                 for (auto* n : c.getSequence().getNotes())
                     if (n->getStartBeat().inBeats() >= fb && n->getStartBeat().inBeats() < tb && n->getNoteNumber() >= pl && n->getNoteNumber() <= ph) sel.add (n);
                 if (sel.isEmpty()) return ToolResult::error ("No notes match the filter");
             }
             auto done = [&] (const juce::String& what) { if (ui.refresh) ui.refresh(); return ToolResult::ok (what + " - clip now has " + juce::String (c.getSequence().getNumNotes()) + " notes"); };

             if (action == "key")
             {
                 if (has (a, "root")) root = MidiTools::rootIndex (a["root"].toString());
                 if (has (a, "scale")) scale = MidiTools::scaleIndex (str (a, "scale"));
                 c.state.setProperty ("sqmScaleRoot", root, nullptr);
                 c.state.setProperty ("sqmScale", scale, nullptr);
                 return ToolResult::ok ("Key: " + MidiTools::noteNames()[root] + " " + MidiTools::scales()[(size_t) scale].name);
             }
             if (action == "progression")
             {
                 int idx = -1;
                 auto pv = a["progression"];
                 auto& ps = MidiTools::progressions();
                 if (pv.isInt() || pv.isDouble() || (pv.isString() && pv.toString().containsOnly ("0123456789") && pv.toString().isNotEmpty())) idx = (int) pv - 1;
                 else
                 {
                     auto q = pv.toString();
                     for (int i = 0; i < (int) ps.size() && idx < 0; ++i) if (juce::String (ps[(size_t) i].name).containsIgnoreCase (q)) idx = i;
                     for (int i = 0; i < (int) ps.size() && idx < 0; ++i) if (juce::String (ps[(size_t) i].mood).containsIgnoreCase (q)) idx = i;
                     if (idx < 0 && q.containsIgnoreCase ("rnb")) idx = 7;
                 }
                 if (! juce::isPositiveAndBelow (idx, (int) ps.size())) return ToolResult::error ("Unknown progression - action list shows them");
                 if (has (a, "root")) root = MidiTools::rootIndex (a["root"].toString());
                 if (flag (a, "replace")) c.getSequence().clear (&undo);
                 const int bpb = juce::jmax (1, c.edit.tempoSequence.getTimeSig (0)->numerator.get());
                 auto names = MidiTools::writeProgression (c, idx, root, 0.0, num (a, "bars_per_chord", 1.0) * bpb, c.getLengthInBeats().inBeats(),
                                                           (int) num (a, "octave", 4), flag (a, "bass", true));
                 c.state.setProperty ("sqmScaleRoot", root, nullptr);
                 c.state.setProperty ("sqmScale", ps[(size_t) idx].minorKey ? 2 : 1, nullptr);
                 return done ("Wrote " + juce::String (ps[(size_t) idx].name) + " in " + MidiTools::pitchClassName (root, MidiTools::keyUsesFlats (root, ps[(size_t) idx].minorKey))
                             + (ps[(size_t) idx].minorKey ? " minor: " : " major: ") + names.joinIntoString (" - "));
             }
             if (action == "chord")
             {
                 auto made = MidiTools::stampChord (c, parsePitch (a["root"]), MidiTools::chordIndex (str (a, "type", "Major")), (int) num (a, "inversion", 0),
                                                    num (a, "beat", 0.0), num (a, "length", 4.0), (int) num (a, "velocity", 96));
                 return done ("Stamped " + MidiTools::chordName (parsePitch (a["root"]), MidiTools::chordIndex (str (a, "type", "Major"))));
             }
             if (action == "quantize" || action == "quantise")
             {
                 MidiTools::quantise (c, sel, gridOf ("grid", 0.25), (float) num (a, "strength", 100) / 100.0f, (float) num (a, "swing", 0) / 100.0f, flag (a, "ends"));
                 return done ("Quantised");
             }
             if (action == "humanize" || action == "humanise")
             {
                 auto amt = str (a, "amount", "medium").toLowerCase();
                 const double tm = amt == "light" ? 0.01 : amt == "heavy" ? 0.04 : 0.02;
                 const int vr = amt == "light" ? 6 : amt == "heavy" ? 20 : 12;
                 MidiTools::humanise (c, sel, tm, vr, (int) num (a, "seed", 1));
                 return done ("Humanised (" + amt + ")");
             }
             if (action == "strum")    { MidiTools::strum (c, sel, gridOf ("speed", 0.125), str (a, "direction", "up") != "down"); return done ("Strummed"); }
             if (action == "arpeggiate" || action == "arp")
             {
                 auto m = str (a, "mode", "up").toLowerCase();
                 auto mode = m == "down" ? MidiTools::ArpMode::down : m.startsWith ("updown") || m == "up-down" ? MidiTools::ArpMode::upDown : m == "random" ? MidiTools::ArpMode::random : MidiTools::ArpMode::up;
                 MidiTools::arpeggiate (c, sel, gridOf ("rate", 0.25), mode, (float) num (a, "gate", 80) / 100.0f, juce::jlimit (1, 4, (int) num (a, "octaves", 1)));
                 return done ("Arpeggiated");
             }
             if (action == "flam")      { MidiTools::flam (c, sel); return done ("Flammed"); }
             if (action == "legato")    { MidiTools::legato (c, sel); return done ("Legato"); }
             if (action == "slides" || action == "slide") { MidiTools::slides (c, sel, gridOf ("glide", 0.125)); return done ("Slides added (each note glides into the next on Moth 808)"); }
             if (action == "chop")      { MidiTools::chop (c, sel, gridOf ("rate", 0.25)); return done ("Chopped"); }
             if (action == "snap_scale")
             {
                 if (scale == 0) return ToolResult::error ("Set a key first (action key)");
                 MidiTools::snapNotesToScale (c, sel, root, scale); return done ("Snapped to " + MidiTools::noteNames()[root] + " " + MidiTools::scales()[(size_t) scale].name);
             }
             if (action == "transpose_scale") { MidiTools::transposeInScale (c, sel, (int) num (a, "steps", 1), root, juce::jmax (1, scale)); return done ("Transposed in key"); }
             if (action == "velocity")
             {
                 if (has (a, "set")) MidiTools::scaleVelocities (c, sel, 0.0f, juce::jlimit (1, 127, (int) num (a, "set", 100)));
                 else MidiTools::scaleVelocities (c, sel, (float) num (a, "strength", 100) / 100.0f);
                 return done ("Velocities changed");
             }
             if (action == "flip")      { MidiTools::flip (c, sel); return done ("Flipped"); }
             if (action == "riff")
             {
                 auto made = MidiTools::randomMelody (c, root, juce::jmax (1, scale), 0.0, c.getLengthInBeats().inBeats(), gridOf ("grid", 0.25),
                                                      has (a, "low") ? parsePitch (a["low"]) : 60, has (a, "high") ? parsePitch (a["high"]) : 79,
                                                      (float) num (a, "density", 60) / 100.0f, (int) num (a, "seed", juce::Random::getSystemRandom().nextInt()));
                 return done ("Riff of " + juce::String (made.size()) + " notes in " + MidiTools::noteNames()[root] + " " + MidiTools::scales()[(size_t) juce::jmax (1, scale)].name);
             }
             if (action == "open")
             {
                 if (! ui.openClipEditor) return ToolResult::error ("UI not available");
                 ui.openClipEditor (c);
                 return ToolResult::ok ("MIDI editor open");
             }
             return ToolResult::error ("Unknown action - see the tool description");
         });
}
