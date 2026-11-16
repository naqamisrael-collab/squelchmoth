// Squelchmoth 1.8 - Claude tools for the Channel Rack, patterns and the instrument shelf. Included inside the McpTools class body.

te::AudioTrack* rackChannelArg (const juce::var& a, juce::String& err, const char* key = "channel")
{
    if (! has (a, key)) { err = juce::String ("Missing '") + key + "' (rack channel number starting at 1, channel name, or track number)."; return nullptr; }
    auto chans = Rack::channels (edit());
    auto v = a[key];
    if (v.isInt() || v.isDouble() || (v.isString() && v.toString().containsOnly ("0123456789") && v.toString().isNotEmpty()))
    {
        const int i = (int) v - 1;
        if (juce::isPositiveAndBelow (i, chans.size())) return chans[i];
    }
    for (auto t : chans) if (t->getName().equalsIgnoreCase (v.toString())) return t;
    for (auto t : chans) if (t->getName().containsIgnoreCase (v.toString())) return t;
    if (auto t = session.findTrack (v)) return t;
    err = "No rack channel matches '" + v.toString() + "'. Call rack with action 'list'.";
    return nullptr;
}

int patternArg (const juce::var& a, const char* key = "pattern")
{
    if (! has (a, key)) return Rack::currentPattern (edit());
    auto v = a[key];
    if (v.isInt() || v.isDouble() || (v.isString() && v.toString().containsOnly ("0123456789") && v.toString().isNotEmpty())) return juce::jlimit (0, juce::jmax (0, Rack::numPatterns (edit()) - 1), (int) v - 1);
    for (int i = 0; i < Rack::numPatterns (edit()); ++i) if (Rack::patternName (edit(), i).equalsIgnoreCase (v.toString())) return i;
    return Rack::currentPattern (edit());
}

juce::var rackChannelJson (te::AudioTrack& t, int pat, bool withSteps)
{
    auto o = obj();
    set (o, "channel", Rack::indexOfChannel (edit(), &t) + 1);
    set (o, "track", session.indexOf (&t));
    set (o, "name", t.getName());
    set (o, "type", Rack::channelType (t));
    if (auto p = Rack::instrumentOf (t)) set (o, "instrument", Signature::displayName (*p));
    set (o, "colour", t.getColour().toDisplayString (false));
    set (o, "mute", t.isMuted (false)); set (o, "solo", t.isSolo (false));
    set (o, "volume_db", MixCtl::getVolumeDb (session, t)); set (o, "pan", MixCtl::getPan (session, t));
    set (o, "root_note", Rack::rootNote (t));
    set (o, "zipped", Rack::isZipped (t));
    if (Rack::group (t).isNotEmpty()) set (o, "group", Rack::group (t));
    if (auto mc = Rack::midiClip (t, pat)) { set (o, "content", "piano roll"); set (o, "notes", mc->getSequence().getNumNotes()); }
    else if (auto sc = Rack::stepClip (t, pat, false))
    {
        set (o, "content", "steps");
        auto p = sc->getPattern (0);
        juce::String row;
        for (int i = 0; i < p.getNumNotes(); ++i)
        {
            if (! p.getNote (0, i)) { row << (i % 4 == 0 && i > 0 ? " ." : "."); continue; }
            const int v = p.getVelocity (0, i);
            juce::String ch = v >= 110 ? "X" : v <= 60 ? "g" : "x";
            if (p.getProbability (0, i) < 0.999f) ch = "?";
            const int rep = BeatLab::getRepeat (p, 0, i); if (rep > 1) ch = juce::String (juce::jmin (9, rep));
            row << (i % 4 == 0 && i > 0 ? " " : "") << ch;
        }
        set (o, "steps", row);
        set (o, "length", BeatLab::rowLength (p, 0) > 0 ? BeatLab::rowLength (p, 0) : p.getNumNotes());
        if (withSteps)
        {
            juce::Array<juce::var> steps;
            for (int i = 0; i < p.getNumNotes(); ++i)
            {
                if (! p.getNote (0, i)) continue;
                auto s = obj(); set (s, "step", i + 1); set (s, "velocity", p.getVelocity (0, i)); set (s, "gate", p.getGate (0, i)); set (s, "chance", juce::roundToInt (p.getProbability (0, i) * 100));
                set (s, "shift", juce::roundToInt (BeatLab::getShift (p, 0, i) * 100)); set (s, "repeat", BeatLab::getRepeat (p, 0, i));
                for (auto lane : { "note", "release", "pitch", "pan", "modx", "mody" })
                {
                    auto toks = Rack::extraLane (t, pat, lane);
                    if (juce::isPositiveAndBelow (i, toks.size())) set (s, lane, juce::String (lane) == "note" ? juce::var (toks[i]) : juce::var (toks[i].getFloatValue()));
                }
                steps.add (s);
            }
            set (o, "step_detail", steps);
        }
    }
    else set (o, "content", "empty");
    return o;
}

juce::var patternJson (int i)
{
    auto o = obj();
    set (o, "pattern", i + 1); set (o, "name", Rack::patternName (edit(), i)); set (o, "colour", Rack::patternColour (edit(), i).toDisplayString (false));
    set (o, "steps", Rack::patternSteps (edit(), i)); set (o, "beats_per_step", Rack::patternDivision (edit(), i)); set (o, "swing", juce::roundToInt (Rack::patternSwing (edit(), i) * 100));
    set (o, "bars", Rack::patternLengthBeats (edit(), i) / 4.0);
    set (o, "has_notes", Rack::patternHasNotes (edit(), i));
    set (o, "current", i == Rack::currentPattern (edit()));
    int placed = 0; const auto id = Rack::pattern (edit(), i) ? Rack::patternId (*Rack::pattern (edit(), i)) : juce::String();
    for (auto t : Rack::channels (edit())) for (auto c : t->getClips()) if (Rack::clipPatternId (*c) == id) ++placed;
    set (o, "placed_clips", placed);
    return o;
}

void registerRackTools()
{
    add ("rack",
         "The Channel Rack (FL-style; every channel is a track, every pattern is a clip group). Actions:\n"
         " list - channels with their steps for the current (or 'pattern') pattern; detail true adds per-step lanes.\n"
         " add - add a channel: type 'instrument' (what = 'Mothsynth 2', 'Venom', 'Sim Moth', 'Mini Moth', 'Thump', 'Dial 10', 'Moth 808', 'Blossom 2', 'Mothsynth', or any VST3/AU instrument), 'sample' (what = file path), 'kit' (what = kit name/genre; count = sounds 1-12, default all), 'layer', 'midiout'. name/colour optional. Returns the new channel number(s).\n"
         " add_existing - put an existing track (track) in the rack. remove / clone / move (to = new position 1-based) / set (name, colour, mute, solo, volume_db, pan, root_note, zip, group).\n"
         " step - one step: step 1-based, on true/false, velocity 1-127, gate 0-2, chance 0-100, shift -50..50, repeat 1-16, note (MIDI number or 'C4', chords 'C4+E4+G4'), release 0-1, pitch -100..100 cents, pan -1..1, modx 0-1, mody 0-1.\n"
         " write - steps: 'x..xX.g.3...' on a channel (x hit, X accent, g ghost, ? 50% chance, 2-8 roll); append false (default) replaces the row.\n"
         " lane - get/set a whole Graph Editor lane: lane velocity|gate|chance|shift|repeat|note|release|pitch|pan|modx|mody, values array (one per step) or value for every step; omit values to read.\n"
         " fill (every 2/4/8, offset) / euclid (hits, of, rotate) / rotate (by) / clear / dice (density 0-1) / humanise (amount 0-1) / double / halve / length (steps: per-channel polymeter length) / copy / paste on a channel.\n"
         " starter - Beat Starter: fill the kit channels of a pattern with a genre groove (genre) - channels named Kick/Snare/Clap/Hat... are recognised.\n"
         " to_piano_roll / to_steps - convert the channel's pattern content. burn_to_midi - play the channel's MIDI clips through its Channel Settings (arpeggiator, echo, gate, shift) and write the result as real notes (arp and echo then switch off). notes - piano-roll channel: notes [{pitch, start_beat, length_beats, velocity}] (replace true clears first; omit notes to read). audition - play the channel's root note once (velocity).\n"
         " open - show the Channel Rack window (or the Beat page with page true).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","add","add_existing","remove","clone","move","set","step","write","lane","fill","euclid","rotate","clear","dice","humanise","double","halve","length","copy","paste","starter","to_piano_roll","to_steps","burn_to_midi","notes","audition","open"]},
            "channel":{"type":["string","integer"]},"track":{"type":["string","integer"]},"pattern":{"type":["string","integer"]},"type":{"type":"string"},"what":{"type":"string"},"name":{"type":"string"},"colour":{"type":"string"},"count":{"type":"integer"},"to":{"type":"integer"},
            "mute":{"type":"boolean"},"solo":{"type":"boolean"},"volume_db":{"type":"number"},"pan":{"type":"number"},"root_note":{"type":["string","integer"]},"zip":{"type":"boolean"},"group":{"type":"string"},
            "step":{"type":"integer"},"on":{"type":"boolean"},"velocity":{"type":"integer"},"gate":{"type":"number"},"chance":{"type":"number"},"shift":{"type":"number"},"repeat":{"type":"integer"},"note":{"type":["string","integer"]},"release":{"type":"number"},"pitch":{"type":"number"},"modx":{"type":"number"},"mody":{"type":"number"},
            "steps":{"type":["string","integer"]},"append":{"type":"boolean"},"notes":{"type":"array"},"replace":{"type":"boolean"},"lane":{"type":"string"},"values":{"type":"array"},"value":{"type":["number","string"]},"detail":{"type":"boolean"},
            "every":{"type":"integer"},"offset":{"type":"integer"},"hits":{"type":"integer"},"of":{"type":"integer"},"rotate":{"type":"integer"},"by":{"type":"integer"},"density":{"type":"number"},"amount":{"type":"number"},"genre":{"type":"string"},"seed":{"type":"integer"},"page":{"type":"boolean"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto& ed = edit();
             auto action = str (a, "action").toLowerCase();
             const int pat = patternArg (a);
             juce::String err;

             if (action == "list")
             {
                 juce::Array<juce::var> out;
                 for (auto t : Rack::channels (ed)) out.add (rackChannelJson (*t, pat, flag (a, "detail")));
                 auto res = obj(); set (res, "pattern", patternJson (pat)); set (res, "pattern_mode", Rack::patternMode (ed)); set (res, "channels", out); set (res, "patterns", Rack::numPatterns (ed));
                 return ToolResult::ok (res);
             }
             if (action == "add")
             {
                 auto type = str (a, "type", "instrument").toLowerCase();
                 juce::StringArray made;
                 if (type == "kit")
                 {
                     auto chans = Rack::addKitChannels (session, DrumKit::kitIndexFor (str (a, "what", "Trap")), has (a, "count") ? (int) num (a, "count") : -1);
                     for (auto t : chans) made.add (juce::String (Rack::indexOfChannel (ed, t) + 1) + ": " + t->getName());
                 }
                 else
                 {
                     auto t = Rack::addChannel (session, type, str (a, "what"), str (a, "name"));
                     if (t == nullptr) return ToolResult::error ("Could not add a '" + type + "' channel (" + str (a, "what") + ").");
                     if (has (a, "colour")) t->setColour (juce::Colour::fromString (str (a, "colour").startsWith ("#") ? "ff" + str (a, "colour").substring (1) : str (a, "colour")));
                     made.add (juce::String (Rack::indexOfChannel (ed, t) + 1) + ": " + t->getName());
                 }
                 if (ui.onRackChanged) ui.onRackChanged();
                 return ToolResult::ok ("Added rack channel " + made.joinIntoString (", "));
             }
             if (action == "add_existing")
             {
                 auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
                 Rack::tagAsRackTrack (*t, dynamic_cast<te::ExternalPlugin*> (Rack::instrumentOf (*t)) ? "plugin" : "instrument", Rack::rootNote (*t));
                 if (ui.onRackChanged) ui.onRackChanged();
                 return ToolResult::ok (t->getName() + " is now rack channel " + juce::String (Rack::indexOfChannel (ed, t) + 1));
             }
             if (action == "open")
             {
                 if (ui.showWindow) ui.showWindow (flag (a, "page") ? "beat" : "rack", true);
                 return ToolResult::ok ("OK");
             }
             if (action == "starter")
             {
                 juce::Random rnd (has (a, "seed") ? (juce::int64) num (a, "seed", 0) : juce::Random::getSystemRandom().nextInt64());
                 const auto& genres = BeatLab::genres();
                 const BeatLab::Genre* g = nullptr;
                 for (auto& gg : genres) if (juce::String (gg.name).equalsIgnoreCase (str (a, "genre"))) g = &gg;
                 if (g == nullptr) for (auto& gg : genres) if (juce::String (gg.name).containsIgnoreCase (str (a, "genre"))) { g = &gg; break; }
                 if (g == nullptr) g = &genres[(size_t) rnd.nextInt ((int) genres.size())];
                 juce::StringArray written;
                 for (auto t : Rack::channels (ed))
                 {
                     const int soundIdx = t->state.hasProperty ("sqmKitSound") ? (int) t->state.getProperty ("sqmKitSound") : -1;
                     int rowIdx = soundIdx;
                     if (rowIdx < 0) { for (int k = 0; k < DrumKit::numSounds; ++k) if (t->getName().containsIgnoreCase (DrumKit::sounds[k].name)) { rowIdx = k; break; } }
                     if (rowIdx < 0) continue;
                     for (auto& ri : g->rows)
                     {
                         if (ri.channel != rowIdx || ri.options.empty()) continue;
                         auto sc = Rack::stepClip (*t, pat, true); if (! sc) break;
                         auto p = sc->getPattern (0);
                         BeatLab::clearRow (*sc, p, 0);
                         BeatLab::writeGrooveString (*sc, p, 0, ri.options[(size_t) rnd.nextInt ((int) ri.options.size())], rnd);
                         written.add (t->getName());
                         break;
                     }
                 }
                 Rack::setPatternSwing (ed, pat, g->swing);
                 Rack::syncPatternClips (ed, pat);
                 if (ui.onRackChanged) ui.onRackChanged();
                 return ToolResult::ok (juce::String (g->name) + ": " + written.joinIntoString (", "));
             }

             auto t = rackChannelArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             auto done = [&] (const juce::String& msg) { Rack::syncPatternClips (ed, pat); if (ui.onRackChanged) ui.onRackChanged(); return ToolResult::ok (msg); };

             if (action == "remove") { auto n = t->getName(); Rack::removeChannel (session, *t); return done ("Removed channel " + n); }
             if (action == "clone")  { auto c = Rack::cloneChannel (session, *t); return c ? done ("Cloned as channel " + juce::String (Rack::indexOfChannel (ed, c) + 1)) : ToolResult::error ("Clone failed"); }
             if (action == "move")   { Rack::moveChannel (ed, *t, (int) num (a, "to", 1) - 1); return done ("Moved " + t->getName() + " to position " + juce::String (Rack::indexOfChannel (ed, t) + 1)); }
             if (action == "set")
             {
                 if (has (a, "name")) t->setName (str (a, "name"));
                 if (has (a, "colour")) t->setColour (juce::Colour::fromString (str (a, "colour").startsWith ("#") ? "ff" + str (a, "colour").substring (1) : str (a, "colour")));
                 if (has (a, "mute")) t->setMute (flag (a, "mute"));
                 if (has (a, "solo")) t->setSolo (flag (a, "solo"));
                 if (has (a, "volume_db")) MixCtl::setVolumeDb (session, *t, (float) num (a, "volume_db"));
                 if (has (a, "pan")) MixCtl::setPan (session, *t, (float) num (a, "pan"));
                 if (has (a, "root_note")) { Rack::setRootNote (*t, parsePitch (a["root_note"])); for (int i = 0; i < Rack::numPatterns (ed); ++i) if (auto sc = Rack::stepClip (*t, i, false)) { sc->getChannels()[0]->noteNumber = Rack::rootNote (*t); BeatLab::rebuildSequence (*sc); } }
                 if (has (a, "zip")) Rack::setZipped (*t, flag (a, "zip"));
                 if (has (a, "group")) Rack::setGroup (*t, str (a, "group"));
                 return done ("Updated channel " + t->getName());
             }
             if (action == "to_piano_roll") { auto mc = Rack::sendToPianoRoll (*t, pat); if (mc && ui.openClipEditor) ui.openClipEditor (*mc); return done (mc ? "Channel " + t->getName() + " now holds a piano-roll clip for " + Rack::patternName (ed, pat) : "Nothing to convert"); }
             if (action == "to_steps") { Rack::sendToSteps (*t, pat); return done ("Channel " + t->getName() + " is back on steps"); }
             if (action == "burn_to_midi")
             {
                 auto r = BurnToMidi::burn (*t); if (r.error.isNotEmpty()) return ToolResult::error (r.error);
                 auto res = obj(); set (res, "channel", t->getName()); set (res, "clips", r.clips); set (res, "notes_in", r.notesIn); set (res, "notes_out", r.notesOut);
                 set (res, "note", "Arpeggiator and echo are now off - the notes play exactly what you heard"); return ToolResult::ok (res);
             }
             if (action == "notes")
             {
                 auto mc = Rack::midiClip (*t, pat);
                 if (mc == nullptr) mc = Rack::sendToPianoRoll (*t, pat);
                 if (mc == nullptr) return ToolResult::error ("No piano-roll clip on " + t->getName());
                 auto* um = &ed.getUndoManager();
                 if (has (a, "notes"))
                 {
                     if (flag (a, "replace")) mc->getSequence().clear (um);
                     if (auto arr = a["notes"].getArray())
                         for (auto& nv : *arr)
                             mc->getSequence().addNote (parsePitch (nv["pitch"]), te::BeatPosition::fromBeats ((double) nv.getProperty ("start_beat", 0.0)), te::BeatDuration::fromBeats (juce::jmax (0.01, (double) nv.getProperty ("length_beats", 1.0))), juce::jlimit (1, 127, (int) nv.getProperty ("velocity", 100)), 0, um);
                     return done (juce::String (mc->getSequence().getNumNotes()) + " notes on " + t->getName());
                 }
                 juce::Array<juce::var> out;
                 for (auto n : mc->getSequence().getNotes()) { auto o = obj(); set (o, "pitch", n->getNoteNumber()); set (o, "name", juce::MidiMessage::getMidiNoteName (n->getNoteNumber(), true, true, 4)); set (o, "start_beat", n->getStartBeat().inBeats()); set (o, "length_beats", n->getLengthBeats().inBeats()); set (o, "velocity", n->getVelocity()); out.add (o); }
                 auto res = obj(); set (res, "channel", t->getName()); set (res, "notes", out); return ToolResult::ok (res);
             }
             if (action == "audition")
             {
                 if (auto p = Rack::instrumentOf (*t))
                 {
                     const int note = has (a, "note") ? parsePitch (a["note"]) : Rack::rootNote (*t);
                     const int vel = (int) num (a, "velocity", 100);
                     t->playGuideNote (note, te::MidiChannel (1), vel, true, true);
                     juce::Timer::callAfterDelay (300, [sp = te::Track::Ptr (t), note] { if (auto at = dynamic_cast<te::AudioTrack*> (sp.get())) at->turnOffGuideNotes(); });
                     return ToolResult::ok ("Auditioned " + t->getName());
                 }
                 return ToolResult::error ("Channel has no instrument");
             }

             auto sc = Rack::stepClip (*t, pat, action != "lane" || has (a, "values") || has (a, "value"));
             if (sc == nullptr) return ToolResult::error ("Channel " + t->getName() + " holds a piano-roll clip for this pattern - use to_steps first, or edit the MIDI clip.");
             auto p = sc->getPattern (0);
             const int n = p.getNumNotes();

             if (action == "step")
             {
                 const int i = (int) num (a, "step", 1) - 1;
                 if (! juce::isPositiveAndBelow (i, n)) return ToolResult::error ("step must be 1.." + juce::String (n));
                 if (has (a, "on")) BeatLab::setStep (*sc, p, 0, i, flag (a, "on"), has (a, "velocity") ? (int) num (a, "velocity") : 100);
                 else if (! p.getNote (0, i)) BeatLab::setStep (*sc, p, 0, i, true, has (a, "velocity") ? (int) num (a, "velocity") : 100);
                 if (has (a, "velocity")) p.setVelocity (0, i, juce::jlimit (1, 127, (int) num (a, "velocity")));
                 if (has (a, "gate")) p.setGate (0, i, juce::jlimit (0.05, 2.0, num (a, "gate")));
                 if (has (a, "chance")) p.setProbability (0, i, (float) juce::jlimit (0.0, 1.0, num (a, "chance") / 100.0));
                 if (has (a, "shift")) BeatLab::setShift (*sc, p, 0, i, (float) juce::jlimit (-0.5, 0.5, num (a, "shift") / 100.0));
                 if (has (a, "repeat")) BeatLab::setRepeat (*sc, p, 0, i, juce::jlimit (1, 16, (int) num (a, "repeat")));
                 if (has (a, "note"))
                 {
                     juce::StringArray parts = juce::StringArray::fromTokens (a["note"].toString(), "+", ""); juce::StringArray nums;
                     for (auto& pt : parts) nums.add (juce::String (parsePitch (juce::var (pt.trim()))));
                     Rack::setExtraLaneValue (*t, pat, "note", i, nums.joinIntoString ("+"));
                 }
                 if (has (a, "release")) Rack::setExtraLaneValue (*t, pat, "release", i, juce::String (juce::jlimit (0.0, 1.0, num (a, "release")), 3));
                 if (has (a, "pitch"))   Rack::setExtraLaneValue (*t, pat, "pitch", i, juce::String (juce::jlimit (-100.0, 100.0, num (a, "pitch")), 1));
                 if (has (a, "pan"))     Rack::setExtraLaneValue (*t, pat, "pan", i, juce::String (juce::jlimit (-1.0, 1.0, num (a, "pan")), 3));
                 if (has (a, "modx"))    Rack::setExtraLaneValue (*t, pat, "modx", i, juce::String (juce::jlimit (0.0, 1.0, num (a, "modx")), 3));
                 if (has (a, "mody"))    Rack::setExtraLaneValue (*t, pat, "mody", i, juce::String (juce::jlimit (0.0, 1.0, num (a, "mody")), 3));
                 BeatLab::rebuildSequence (*sc);
                 return done ("Step " + juce::String (i + 1) + " on " + t->getName() + " updated");
             }
             if (action == "write")
             {
                 if (! flag (a, "append")) BeatLab::clearRow (*sc, p, 0);
                 { juce::Random r2; BeatLab::writeGrooveString (*sc, p, 0, a["steps"].toString().removeCharacters (" |"), r2); }
                 return done (t->getName() + ": " + a["steps"].toString());
             }
             if (action == "lane")
             {
                 auto lane = str (a, "lane", "velocity").toLowerCase();
                 const bool extra = Rack::extraLaneProp (lane).isNotEmpty();
                 BeatLab::Lane bl = BeatLab::Lane::velocity;
                 if (lane == "gate") bl = BeatLab::Lane::gate; else if (lane == "chance") bl = BeatLab::Lane::chance; else if (lane == "shift") bl = BeatLab::Lane::shift; else if (lane == "repeat") bl = BeatLab::Lane::repeat;
                 else if (! extra && lane != "velocity") return ToolResult::error ("lane must be velocity, gate, chance, shift, repeat, note, release, pitch, pan, modx or mody");
                 if (has (a, "values") || has (a, "value"))
                 {
                     for (int i = 0; i < n; ++i)
                     {
                         juce::var v = has (a, "value") ? a["value"] : (a["values"].isArray() && i < a["values"].size() ? a["values"][i] : juce::var());
                         if (v.isVoid()) continue;
                         if (extra) Rack::setExtraLaneValue (*t, pat, lane, i, lane == "note" ? juce::String (parsePitch (v)) : juce::String ((double) v, 3));
                         else if (bl == BeatLab::Lane::chance || bl == BeatLab::Lane::shift) Rack::setLane (*t, pat, bl, i, (float) ((double) v / 100.0));
                         else Rack::setLane (*t, pat, bl, i, (float) (double) v);
                     }
                     BeatLab::rebuildSequence (*sc);
                     return done ("Lane " + lane + " written on " + t->getName());
                 }
                 juce::Array<juce::var> vals;
                 for (int i = 0; i < n; ++i)
                 {
                     if (extra) { auto toks = Rack::extraLane (*t, pat, lane); vals.add (juce::isPositiveAndBelow (i, toks.size()) ? juce::var (toks[i]) : juce::var (lane == "release" ? "1" : "0")); }
                     else { float v = Rack::lane (*t, pat, bl, i); vals.add (bl == BeatLab::Lane::chance || bl == BeatLab::Lane::shift ? juce::var (juce::roundToInt (v * 100)) : juce::var (v)); }
                 }
                 auto res = obj(); set (res, "channel", t->getName()); set (res, "lane", lane); set (res, "values", vals);
                 return ToolResult::ok (res);
             }
             juce::Random rnd (has (a, "seed") ? (juce::int64) num (a, "seed", 0) : juce::Random::getSystemRandom().nextInt64());
             if (action == "fill")     { BeatLab::fillEvery (*sc, p, 0, juce::jmax (1, (int) num (a, "every", 4)), (int) num (a, "offset", 0), (int) num (a, "velocity", 100)); return done ("Filled every " + juce::String ((int) num (a, "every", 4))); }
             if (action == "euclid")   { BeatLab::fillEuclid (*sc, p, 0, (int) num (a, "hits", 3), (int) num (a, "of", n), (int) num (a, "rotate", 0), (int) num (a, "velocity", 100)); return done ("Euclidean fill"); }
             if (action == "rotate")   { BeatLab::rotateRow (*sc, p, 0, (int) num (a, "by", 1)); return done ("Rotated"); }
             if (action == "clear")    { BeatLab::clearRow (*sc, p, 0); for (auto lane : { "note", "release", "pitch", "pan", "modx", "mody" }) Rack::clearExtraLane (*t, pat, lane); return done ("Cleared " + t->getName()); }
             if (action == "dice")     { BeatLab::diceRow (*sc, p, 0, (float) num (a, "density", 0.4), rnd); return done ("Diced"); }
             if (action == "humanise") { BeatLab::humanise (*sc, p, 0, (float) num (a, "amount", 0.3), rnd); return done ("Humanised"); }
             if (action == "double")   { Rack::setPatternSteps (ed, pat, juce::jmin (512, n * 2)); for (auto c : Rack::channels (ed)) if (auto s2 = Rack::stepClip (*c, pat, false)) { auto p0 = s2->getPattern (0); auto d = BeatLab::readRow (p0, 0); std::vector<BeatLab::StepData> dd (d); dd.insert (dd.end(), d.begin(), d.begin() + (long) juce::jmin ((int) d.size(), n)); auto pp = s2->getPattern (0); BeatLab::writeRow (*s2, pp, 0, dd); } return done ("Pattern doubled to " + juce::String (n * 2) + " steps"); }
             if (action == "halve")    { Rack::setPatternSteps (ed, pat, juce::jmax (1, n / 2)); return done ("Pattern halved to " + juce::String (n / 2) + " steps"); }
             if (action == "length")   { BeatLab::setRowLength (*sc, p, 0, juce::jlimit (0, n, (int) num (a, "steps", 0))); BeatLab::rebuildSequence (*sc); return done ("Channel length set"); }
             if (action == "copy")     { BeatLab::copyRow (p, 0); return ToolResult::ok ("Copied"); }
             if (action == "paste")    { BeatLab::paste (*sc, 0); return done ("Pasted"); }
             return ToolResult::error ("Unknown action " + action);
         });

    add ("pattern",
         "Patterns of the Channel Rack (each pattern is an engine scene; placing it on the timeline makes linked Pattern Clips). Actions: list; new (name, colour); clone (pattern); rename (pattern, name); colour (pattern, colour); delete (pattern); select (pattern - the one the rack edits and Pattern mode loops); "
         "settings (pattern, steps 1-512, beats_per_step e.g. 0.25 = 1/16 or division '1/16','1/8T','1/32', swing 0-100); place (pattern, bar, repeats) - put it on the timeline as linked clips; make_unique (track, clip) - break one clip's link; "
         "sync (pattern) - push the pattern into every linked clip; mode (mode 'pattern' or 'song') - Pattern mode loops the selected pattern through the clip launcher, Song mode plays the timeline; select_unused - which patterns are empty; find (name).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","new","clone","rename","colour","delete","select","settings","place","make_unique","sync","mode","select_unused","find"]},
            "pattern":{"type":["string","integer"]},"name":{"type":"string"},"colour":{"type":"string"},"steps":{"type":"integer"},"beats_per_step":{"type":"number"},"division":{"type":"string"},"swing":{"type":"number"},
            "bar":{"type":"number"},"repeats":{"type":"integer"},"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"mode":{"type":"string"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto& ed = edit();
             auto action = str (a, "action").toLowerCase();
             Rack::ensurePatterns (ed, 1);
             auto refresh = [&] { if (ui.onRackChanged) ui.onRackChanged(); };
             if (action == "list" || action == "select_unused")
             {
                 juce::Array<juce::var> out;
                 for (int i = 0; i < Rack::numPatterns (ed); ++i) if (action == "list" || ! Rack::patternHasNotes (ed, i)) out.add (patternJson (i));
                 auto res = obj(); set (res, "patterns", out); set (res, "current", Rack::currentPattern (ed) + 1); set (res, "mode", Rack::patternMode (ed) ? "pattern" : "song");
                 return ToolResult::ok (res);
             }
             if (action == "new")
             {
                 const int i = Rack::newPattern (ed, str (a, "name"));
                 if (has (a, "colour")) Rack::setPatternColour (ed, i, juce::Colour::fromString (str (a, "colour").startsWith ("#") ? "ff" + str (a, "colour").substring (1) : str (a, "colour")));
                 Rack::selectPattern (ed, i); refresh();
                 return ToolResult::ok ("Pattern " + juce::String (i + 1) + " (" + Rack::patternName (ed, i) + ") created and selected");
             }
             if (action == "find")
             {
                 for (int i = 0; i < Rack::numPatterns (ed); ++i) if (Rack::patternName (ed, i).containsIgnoreCase (str (a, "name"))) return ToolResult::ok (patternJson (i));
                 return ToolResult::error ("No pattern named " + str (a, "name"));
             }
             if (action == "mode")
             {
                 const bool pm = str (a, "mode", "pattern").toLowerCase().startsWith ("p");
                 Rack::setPatternMode (ed, pm);
                 if (pm && ! ed.getTransport().isPlaying()) ed.getTransport().play (false);
                 refresh();
                 return ToolResult::ok (pm ? "Pattern mode: looping " + Rack::patternName (ed, Rack::currentPattern (ed)) : "Song mode: the timeline plays");
             }
             const int i = patternArg (a);
             if (action == "clone")  { const int c = Rack::clonePattern (ed, i); Rack::selectPattern (ed, c); refresh(); return ToolResult::ok ("Cloned as pattern " + juce::String (c + 1)); }
             if (action == "rename") { Rack::setPatternName (ed, i, str (a, "name")); for (auto t : Rack::channels (ed)) if (auto c = Rack::slotClip (*t, i)) c->setName (str (a, "name")); refresh(); return ToolResult::ok ("Renamed"); }
             if (action == "colour") { Rack::setPatternColour (ed, i, juce::Colour::fromString (str (a, "colour").startsWith ("#") ? "ff" + str (a, "colour").substring (1) : str (a, "colour"))); refresh(); return ToolResult::ok ("Coloured"); }
             if (action == "delete") { if (Rack::numPatterns (ed) <= 1) return ToolResult::error ("The last pattern stays."); Rack::deletePattern (ed, i); refresh(); return ToolResult::ok ("Deleted pattern " + juce::String (i + 1)); }
             if (action == "select") { Rack::selectPattern (ed, i); refresh(); return ToolResult::ok ("Pattern " + juce::String (i + 1) + " selected"); }
             if (action == "settings")
             {
                 if (has (a, "steps")) Rack::setPatternSteps (ed, i, (int) num (a, "steps"));
                 if (has (a, "beats_per_step")) Rack::setPatternDivision (ed, i, num (a, "beats_per_step"));
                 if (has (a, "division"))
                 {
                     auto d = str (a, "division").trim().toLowerCase(); double beats = 0.25;
                     if (d == "1/4") beats = 1.0; else if (d == "1/8") beats = 0.5; else if (d == "1/16") beats = 0.25; else if (d == "1/32") beats = 0.125; else if (d == "1/64") beats = 0.0625;
                     else if (d == "1/8t") beats = 1.0 / 3.0; else if (d == "1/16t") beats = 1.0 / 6.0; else if (d == "1/32t") beats = 1.0 / 12.0; else if (d == "1/4t") beats = 2.0 / 3.0;
                     Rack::setPatternDivision (ed, i, beats);
                 }
                 if (has (a, "swing")) Rack::setPatternSwing (ed, i, (float) (num (a, "swing") / 100.0));
                 Rack::syncPatternClips (ed, i); refresh();
                 return ToolResult::ok (patternJson (i));
             }
             if (action == "place")
             {
                 auto clips = Rack::placePattern (session, i, num (a, "bar", 1.0), (int) num (a, "repeats", 1));
                 refresh();
                 return ToolResult::ok ("Placed " + Rack::patternName (ed, i) + " at bar " + juce::String (num (a, "bar", 1.0)) + ": " + juce::String (clips.size()) + " linked clips");
             }
             if (action == "sync") { Rack::syncPatternClips (ed, i); refresh(); return ToolResult::ok ("Synced"); }
             if (action == "make_unique")
             {
                 juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
                 te::Clip* clip = nullptr; auto clips = t->getClips();
                 if (has (a, "clip")) { auto v = a["clip"]; if (v.isInt() || v.isDouble()) { int k = (int) v - 1; if (juce::isPositiveAndBelow (k, clips.size())) clip = clips[k]; } else for (auto c : clips) if (c->getName().equalsIgnoreCase (v.toString())) clip = c; }
                 if (clip == nullptr) return ToolResult::error ("Pass clip (number from list_clips or name)");
                 for (auto c : Rack::linkedGroup (*clip)) Rack::makeUnique (*c);
                 return ToolResult::ok (clip->getName() + " at that bar is now unique (edits no longer follow the pattern)");
             }
             return ToolResult::error ("Unknown action " + action);
         });

    add ("instrument",
         "The instrument shelf. Actions: list - every built-in instrument with its preset count and what it does; insert (track or channel, name) - put an instrument on a track (replacing its current one); "
         "parameters (track, slot optional) - every parameter with id, name, value and text; set (track, params: [{param, real|value|text}]); preset (track, name or index; list true lists the bank by category); "
         "randomise (track, amount 0-1, seed) - a musical random patch; macros (track, macro1..macro4 0-1); mod (track, slot 1-8, source, destination, depth -1..1) - the mod matrix; voice (track, poly_mode poly|mono|legato, glide_mode off|always|legato, unison 1-8, max_voices, bend_range, quality eco|hd); compare (track, action copy_to_b|swap).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","insert","parameters","set","preset","randomise","macros","mod","voice","compare"]},"track":{"type":["string","integer"]},"channel":{"type":["string","integer"]},"slot":{"type":["string","integer"]},"name":{"type":"string"},"index":{"type":"integer"},"list":{"type":"boolean"},
            "params":{"type":"array"},"amount":{"type":"number"},"seed":{"type":"integer"},"macro1":{"type":"number"},"macro2":{"type":"number"},"macro3":{"type":"number"},"macro4":{"type":"number"},
            "mod_slot":{"type":"integer"},"source":{"type":"string"},"destination":{"type":"string"},"depth":{"type":"number"},"poly_mode":{"type":"string"},"glide_mode":{"type":"string"},"unison":{"type":"integer"},"max_voices":{"type":"integer"},"bend_range":{"type":"integer"},"quality":{"type":"string"},"compare":{"type":"string"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto action = str (a, "action").toLowerCase();
             if (action == "list")
             {
                 juce::Array<juce::var> out;
                 for (auto& e : Signature::entries())
                 {
                     if (juce::String (e.category) != "Instruments") continue;
                     auto o = obj(); set (o, "name", e.name); set (o, "type", e.type); set (o, "what", e.tagline); set (o, "presets", (int) e.presets.size());
                     out.add (o);
                 }
                 auto res = obj(); set (res, "instruments", out); return ToolResult::ok (res);
             }
             juce::String err;
             te::AudioTrack* t = has (a, "channel") ? rackChannelArg (a, err) : trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             if (action == "insert")
             {
                 auto se = Signature::findByName (str (a, "name"));
                 juce::String type = se ? se->type : juce::String();
                 std::unique_ptr<juce::PluginDescription> desc;
                 if (type.isEmpty()) desc = session.findPluginDescription (str (a, "name"));
                 if (type.isEmpty() && desc == nullptr) return ToolResult::error ("No instrument named " + str (a, "name"));
                 if (auto old = Rack::instrumentOf (*t)) old->deleteFromParent();
                 te::Plugin::Ptr p = desc ? edit().getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, *desc) : edit().getPluginCache().createNewPlugin (type, {});
                 if (p == nullptr) return ToolResult::error ("Could not create " + str (a, "name"));
                 t->pluginList.insertPlugin (p, 0, nullptr);
                 if (se && ! se->presets.empty()) Signature::applyPreset (*p, se->presets.front());
                 if (ui.onRackChanged) ui.onRackChanged();
                 return ToolResult::ok (str (a, "name") + " inserted on " + t->getName());
             }
             te::Plugin* p = Rack::instrumentOf (*t);
             if (has (a, "slot")) { auto copy = a.clone(); copy.getDynamicObject()->setProperty ("track", session.indexOf (t)); p = insertArg (copy, err); }
             if (p == nullptr) return ToolResult::error (t->getName() + " has no instrument");
             auto* sc = dynamic_cast<SqSynth::SynthCommon*> (p);
             auto* um = &edit().getUndoManager();

             if (action == "parameters")
             {
                 juce::Array<juce::var> out;
                 for (auto ap : p->getAutomatableParameters()) { auto o = obj(); set (o, "id", ap->paramID); set (o, "name", ap->getParameterName()); set (o, "value", ap->getCurrentValue()); set (o, "normalised", ap->getCurrentNormalisedValue()); set (o, "text", ap->getCurrentValueAsString()); out.add (o); }
                 auto res = obj(); set (res, "instrument", Signature::displayName (*p)); set (res, "parameters", out);
                 if (sc)
                 {
                     auto v = obj(); set (v, "poly_mode", SqSynth::polyModeNames()[juce::jlimit (0, 2, sc->polyModeValue().get())].toLowerCase()); set (v, "glide_mode", SqSynth::glideModeNames()[juce::jlimit (0, 2, sc->glideModeValue().get())].toLowerCase());
                     set (v, "unison", sc->unisonValue().get()); set (v, "max_voices", sc->maxPolyValue().get()); set (v, "bend_range", sc->bendRangeValue().get()); set (v, "quality", sc->qualityValue().get() == 1 ? "hd" : "eco");
                     set (res, "voice", v);
                     juce::Array<juce::var> mods; auto names = SqSynth::modSourceNames();
                     for (int i = 0; i < SqSynth::numModSlots; ++i) { auto m = obj(); set (m, "slot", i + 1); set (m, "source", names[juce::jlimit (0, names.size() - 1, sc->modSourceValue (i).get())]); set (m, "destination", sc->modDestValue (i).get()); set (m, "depth", sc->modDepthValue (i).get()); mods.add (m); }
                     set (res, "mod_matrix", mods); set (res, "mod_destinations", sc->modDestinations());
                 }
                 return ToolResult::ok (res);
             }
             if (action == "set")
             {
                 auto copy = a.clone(); copy.getDynamicObject()->setProperty ("track", session.indexOf (t)); if (! has (a, "slot")) copy.getDynamicObject()->setProperty ("slot", Signature::displayName (*p));
                 return call ("set_plugin_parameters", copy);
             }
             if (action == "preset")
             {
                 auto copy = a.clone(); copy.getDynamicObject()->setProperty ("track", session.indexOf (t)); if (! has (a, "slot")) copy.getDynamicObject()->setProperty ("slot", Signature::displayName (*p));
                 copy.getDynamicObject()->setProperty ("action", flag (a, "list") || (! has (a, "name") && ! has (a, "index")) ? "list" : "load");
                 return call ("plugin_presets", copy);
             }
             if (action == "randomise")
             {
                 juce::Random rnd (has (a, "seed") ? (juce::int64) num (a, "seed", 0) : juce::Random::getSystemRandom().nextInt64());
                 const float amt = (float) juce::jlimit (0.0, 1.0, num (a, "amount", 0.5));
                 int changed = 0;
                 for (auto ap : p->getAutomatableParameters())
                 {
                     const auto id = ap->paramID;
                     if (id == "level" || id.startsWith ("macro") || id == "width" || id == "pan") continue;     // keep it usable
                     float v = ap->getCurrentNormalisedValue();
                     v = juce::jlimit (0.0f, 1.0f, v + (rnd.nextFloat() * 2.0f - 1.0f) * amt * (id.contains ("A") && id.length() == 2 ? 0.3f : 1.0f));
                     ap->setNormalisedParameter (v, juce::sendNotification); ++changed;
                 }
                 return ToolResult::ok ("Randomised " + juce::String (changed) + " parameters on " + Signature::displayName (*p));
             }
             if (action == "macros")
             {
                 for (int i = 1; i <= 4; ++i) if (has (a, ("macro" + juce::String (i)).toRawUTF8())) if (auto ap = Signature::findParam (*p, "macro" + juce::String (i))) ap->setParameter ((float) juce::jlimit (0.0, 1.0, num (a, ("macro" + juce::String (i)).toRawUTF8())), juce::sendNotification);
                 return ToolResult::ok ("Macros set");
             }
             if (sc == nullptr) return ToolResult::error (Signature::displayName (*p) + " is not a 1.8 synth (no mod matrix / voice page). Use parameters / set.");
             if (action == "mod")
             {
                 const int slot = juce::jlimit (0, SqSynth::numModSlots - 1, (int) num (a, "mod_slot", 1) - 1);
                 if (has (a, "source")) { auto names = SqSynth::modSourceNames(); int idx = 0; for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (str (a, "source")) || names[i].removeCharacters (" ").equalsIgnoreCase (str (a, "source").removeCharacters (" "))) idx = i; sc->modSourceValue (slot).setValue (idx, um); }
                 if (has (a, "destination")) sc->modDestValue (slot).setValue (str (a, "destination").toLowerCase(), um);
                 if (has (a, "depth")) sc->modDepthValue (slot).setValue ((float) juce::jlimit (-1.0, 1.0, num (a, "depth")), um);
                 return ToolResult::ok ("Mod slot " + juce::String (slot + 1) + " set");
             }
             if (action == "voice")
             {
                 if (has (a, "poly_mode")) { auto m = str (a, "poly_mode").toLowerCase(); sc->polyModeValue().setValue (m.startsWith ("m") ? 1 : m.startsWith ("l") ? 2 : 0, um); }
                 if (has (a, "glide_mode")) { auto m = str (a, "glide_mode").toLowerCase(); sc->glideModeValue().setValue (m.startsWith ("a") ? 1 : m.startsWith ("l") ? 2 : 0, um); }
                 if (has (a, "unison")) sc->unisonValue().setValue (juce::jlimit (1, 8, (int) num (a, "unison")), um);
                 if (has (a, "max_voices")) sc->maxPolyValue().setValue (juce::jlimit (1, sc->maxVoiceCount(), (int) num (a, "max_voices")), um);
                 if (has (a, "bend_range")) sc->bendRangeValue().setValue (juce::jlimit (1, 48, (int) num (a, "bend_range")), um);
                 if (has (a, "quality")) sc->qualityValue().setValue (str (a, "quality").toLowerCase().startsWith ("h") ? 1 : 0, um);
                 return ToolResult::ok ("Voice settings updated");
             }
             if (action == "compare") return ToolResult::error ("A/B lives in the plug-in window header (A/B button); over MCP save a preset instead.");
             return ToolResult::error ("Unknown action " + action);
         });

    add ("channel_settings",
         "FL-style Channel Settings for a rack channel (the Rack Channel plug-in in front of the instrument; works for every instrument incl. VST3/AU). Actions: get; set with any of: "
         "levels {mod_x 0-1, mod_y 0-1, pitch_cents -4800..4800, pitch_on, velocity_scale 0-2}; polyphony {max_poly 0=unlimited..32, mono, legato, porta, slide_time s}; time {gate beats (0 off), shift 0-1 of a 16th}; "
         "tracking {vel_pan, vel_cut, vel_modx, vel_mody, key_pan, key_cut, key_modx, key_mody -1..1, key_mid note}; cut {cut_group 0-99, cut_by 0-99, cut_self}; "
         "arp {mode off|up|down|updown|updown2|random|asplayed, time beats, gate 0-2, range 1-4, repeat 1-4, chord none|major|minor|maj7|min7|power|sus4|octave}; echo {count 0-16, time beats, feed 0-1, pan 0-1, ping_pong, pitch st, cut 0-1}; "
         "layer {mode all|random|crossfade|sequential, children: [channel numbers or names]} (the channel must be a Layer channel). Flat keys work too (e.g. {cut_group: 1}). reset restores defaults.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["get","set","reset"]},"channel":{"type":["string","integer"]},"track":{"type":["string","integer"]},
            "levels":{"type":"object"},"polyphony":{"type":"object"},"time":{"type":"object"},"tracking":{"type":"object"},"cut":{"type":"object"},"arp":{"type":"object"},"echo":{"type":"object"},"layer":{"type":"object"},
            "mod_x":{"type":"number"},"mod_y":{"type":"number"},"pitch_cents":{"type":"number"},"pitch_on":{"type":"boolean"},"velocity_scale":{"type":"number"},"max_poly":{"type":"integer"},"mono":{"type":"boolean"},"legato":{"type":"boolean"},"porta":{"type":"boolean"},"slide_time":{"type":"number"},
            "gate":{"type":"number"},"shift":{"type":"number"},"cut_group":{"type":"integer"},"cut_by":{"type":"integer"},"cut_self":{"type":"boolean"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             juce::String err;
             te::AudioTrack* t = has (a, "channel") ? rackChannelArg (a, err) : trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             auto* rc = Rack::rackChannelPlugin (*t, true);
             if (rc == nullptr) return ToolResult::error ("Could not create the Rack Channel plug-in on " + t->getName());
             auto action = str (a, "action", "get").toLowerCase();
             auto* um = &edit().getUndoManager();
             auto setP = [&] (const char* id, double v) { if (auto ap = Signature::findParam (*rc, id)) ap->setParameter (ap->getValueRange().clipValue ((float) v), juce::sendNotification); };
             auto getP = [&] (const char* id) { auto ap = Signature::findParam (*rc, id); return ap ? (double) ap->getCurrentValue() : 0.0; };
             if (action == "reset")
             {
                 for (size_t i = 0; i < rc->bank.params.size(); ++i) rc->bank.params[i]->setParameter (rc->bank.specs[i].def, juce::sendNotification);
                 rc->resetProps (um);
                 return ToolResult::ok ("Channel settings reset on " + t->getName());
             }
             if (action == "set")
             {
                 // merge the sections and flat keys into one map
                 juce::NamedValueSet all;
                 for (auto sec : { "levels", "polyphony", "time", "tracking", "cut", "arp", "echo", "layer" })
                     if (auto o = a[sec].getDynamicObject()) for (auto& kv : o->getProperties()) all.set (juce::Identifier (juce::String (sec) + "." + kv.name.toString()), kv.value);
                 if (auto o = a.getDynamicObject()) for (auto& kv : o->getProperties()) if (! kv.value.isObject() && kv.name.toString() != "action" && kv.name.toString() != "channel" && kv.name.toString() != "track") all.set (kv.name, kv.value);
                 auto g = [&] (const char* key, const char* alt = "") -> const juce::var* { if (auto v = all.getVarPointer (juce::Identifier (key))) return v; if (*alt && all.getVarPointer (juce::Identifier (alt))) return all.getVarPointer (juce::Identifier (alt)); return nullptr; };
                 if (auto v = g ("levels.mod_x", "mod_x")) setP ("modX", *v); if (auto v = g ("levels.mod_y", "mod_y")) setP ("modY", *v);
                 if (auto v = g ("levels.pitch_cents", "pitch_cents")) setP ("pitch", *v); if (auto v = g ("levels.pitch_on", "pitch_on")) rc->pitchOn.setValue ((bool) *v, um);
                 if (auto v = g ("levels.velocity_scale", "velocity_scale")) setP ("velScale", *v);
                 if (auto v = g ("polyphony.max_poly", "max_poly")) setP ("maxPoly", *v); if (auto v = g ("polyphony.mono", "mono")) rc->mono.setValue ((bool) *v, um);
                 if (auto v = g ("polyphony.legato", "legato")) rc->legato.setValue ((bool) *v, um); if (auto v = g ("polyphony.porta", "porta")) rc->porta.setValue ((bool) *v, um);
                 if (auto v = g ("polyphony.slide_time", "slide_time")) setP ("slideTime", *v);
                 if (auto v = g ("time.gate", "gate")) setP ("gate", *v); if (auto v = g ("time.shift", "shift")) setP ("shift", *v);
                 for (auto [k, id] : std::initializer_list<std::pair<const char*, const char*>> { { "vel_pan", "velToPan" }, { "vel_cut", "velToCut" }, { "vel_modx", "velToModX" }, { "vel_mody", "velToModY" }, { "key_pan", "keyToPan" }, { "key_cut", "keyToCut" }, { "key_modx", "keyToModX" }, { "key_mody", "keyToModY" } })
                     if (auto v = g ((juce::String ("tracking.") + k).toRawUTF8(), k)) setP (id, *v);
                 if (auto v = g ("tracking.key_mid", "key_mid")) setP ("keyMid", parsePitch (*v));
                 if (auto v = g ("cut.cut_group", "cut_group")) setP ("cutGroup", *v); if (auto v = g ("cut.cut_by", "cut_by")) setP ("cutBy", *v); if (auto v = g ("cut.cut_self", "cut_self")) rc->cutSelf.setValue ((bool) *v, um);
                 if (auto v = g ("arp.mode", "arp_mode")) { auto m = v->toString().toLowerCase().removeCharacters (" -_"); int idx = m == "up" ? 1 : m == "down" ? 2 : m == "updown" ? 3 : m == "updownx2" || m == "updown2" ? 4 : m == "random" ? 5 : m == "asplayed" ? 6 : 0; setP ("arpMode", idx); }
                 if (auto v = g ("arp.time", "arp_time")) setP ("arpTime", *v); if (auto v = g ("arp.gate", "arp_gate")) setP ("arpGate", *v); if (auto v = g ("arp.range", "arp_range")) setP ("arpRange", *v); if (auto v = g ("arp.repeat", "arp_repeat")) setP ("arpRepeat", *v);
                 if (auto v = g ("arp.chord", "arp_chord")) { auto names = SqRackChannelPlugin::arpChordNames(); int idx = 0; for (int i = 0; i < names.size(); ++i) if (names[i].removeCharacters (" ").equalsIgnoreCase (v->toString().removeCharacters (" "))) idx = i; setP ("arpChord", idx); }
                 if (auto v = g ("echo.count", "echo_count")) setP ("echoCount", *v); if (auto v = g ("echo.time", "echo_time")) setP ("echoTime", *v); if (auto v = g ("echo.feed", "echo_feed")) setP ("echoFeed", *v);
                 if (auto v = g ("echo.pan", "echo_pan")) setP ("echoPan", *v); if (auto v = g ("echo.ping_pong", "echo_ping_pong")) rc->echoPingPong.setValue ((bool) *v, um); if (auto v = g ("echo.pitch", "echo_pitch")) setP ("echoPitch", *v); if (auto v = g ("echo.cut", "echo_cut")) setP ("echoCut", *v);
                 if (auto v = g ("layer.mode", "layer_mode")) { auto m = v->toString().toLowerCase(); rc->layerMode.setValue (m.startsWith ("ran") ? 1 : m.startsWith ("cross") ? 2 : m.startsWith ("seq") ? 3 : 0, um); }
                 if (auto v = g ("layer.children", "children"))
                 {
                     juce::Array<te::AudioTrack*> kids;
                     if (auto arr = v->getArray()) for (auto& c : *arr) { juce::var one (c); auto copy = juce::var (new juce::DynamicObject()); copy.getDynamicObject()->setProperty ("channel", one); juce::String e2; if (auto k = rackChannelArg (copy, e2)) kids.add (k); }
                     Rack::setLayerChildren (*t, kids);
                     if (Rack::channelType (*t) != "layer") t->state.setProperty ("sqmRackType", "layer", um);
                 }
                 rc->syncCutToInstrument();
                 if (ui.onRackChanged) ui.onRackChanged();
             }
             auto o = obj(); set (o, "channel", t->getName());
             auto lv = obj(); set (lv, "mod_x", getP ("modX")); set (lv, "mod_y", getP ("modY")); set (lv, "pitch_cents", getP ("pitch")); set (lv, "pitch_on", rc->pitchOn.get()); set (lv, "velocity_scale", getP ("velScale")); set (o, "levels", lv);
             auto po = obj(); set (po, "max_poly", (int) getP ("maxPoly")); set (po, "mono", rc->mono.get()); set (po, "legato", rc->legato.get()); set (po, "porta", rc->porta.get()); set (po, "slide_time", getP ("slideTime")); set (o, "polyphony", po);
             auto tm = obj(); set (tm, "gate", getP ("gate")); set (tm, "shift", getP ("shift")); set (o, "time", tm);
             auto tr = obj(); for (auto [k, id] : std::initializer_list<std::pair<const char*, const char*>> { { "vel_pan", "velToPan" }, { "vel_cut", "velToCut" }, { "vel_modx", "velToModX" }, { "vel_mody", "velToModY" }, { "key_pan", "keyToPan" }, { "key_cut", "keyToCut" }, { "key_modx", "keyToModX" }, { "key_mody", "keyToModY" } }) set (tr, k, getP (id)); set (tr, "key_mid", (int) getP ("keyMid")); set (o, "tracking", tr);
             auto cu = obj(); set (cu, "cut_group", (int) getP ("cutGroup")); set (cu, "cut_by", (int) getP ("cutBy")); set (cu, "cut_self", rc->cutSelf.get()); set (o, "cut", cu);
             auto ar = obj(); set (ar, "mode", SqRackChannelPlugin::arpModeNames()[juce::jlimit (0, 6, (int) getP ("arpMode"))].toLowerCase()); set (ar, "time", getP ("arpTime")); set (ar, "gate", getP ("arpGate")); set (ar, "range", (int) getP ("arpRange")); set (ar, "repeat", (int) getP ("arpRepeat")); set (ar, "chord", SqRackChannelPlugin::arpChordNames()[juce::jlimit (0, 7, (int) getP ("arpChord"))].toLowerCase()); set (o, "arp", ar);
             auto ec = obj(); set (ec, "count", (int) getP ("echoCount")); set (ec, "time", getP ("echoTime")); set (ec, "feed", getP ("echoFeed")); set (ec, "pan", getP ("echoPan")); set (ec, "ping_pong", rc->echoPingPong.get()); set (ec, "pitch", getP ("echoPitch")); set (ec, "cut", getP ("echoCut")); set (o, "echo", ec);
             { auto dbg = obj(); set (dbg, "cut_group_generation", SqRackChannelPlugin::cutGeneration ((int) getP ("cutGroup")).load()); set (dbg, "cut_by_generation", SqRackChannelPlugin::cutGeneration ((int) getP ("cutBy")).load());
               for (auto p : t->pluginList.getPlugins()) if (auto hp = dynamic_cast<SqHivePadsPlugin*> (p)) set (dbg, "pads_watch_group", hp->watchCutGroup.load());
               juce::StringArray order; for (auto p : t->pluginList.getPlugins()) order.add (p->getName()); set (dbg, "plugin_order", order); set (o, "debug", dbg); }
             auto la = obj(); set (la, "mode", SqRackChannelPlugin::layerModeNames()[juce::jlimit (0, 3, rc->layerMode.get())].toLowerCase()); juce::StringArray kidNames; for (auto id : Rack::layerChildren (*t)) for (auto k : Rack::channels (edit())) if (k->itemID == id) kidNames.add (k->getName()); set (la, "children", kidNames); set (o, "layer", la);
             return ToolResult::ok (o);
         });

    //==========================================================================
    add ("kits",
         "Sound kits and packs. Actions: 'list' - every Hive kit (name, genre notes, 12 sounds with their keys, whether it's rendered yet); "
         "'sounds' - the 12 sound slots and their MIDI keys (C2 kick ... B2); 'load' - load a kit onto a Hive Pads instrument (track or rack channel; kit name or genre word; bank A-D) - the pads take the sound names, keys and choke groups; "
         "'render' - render a kit's samples now (they're rendered on first use otherwise); 'channels' - add one rack channel per sound of a kit (same as rack add type kit). "
         "Packs: the Pollen packs arrive in 1.8.1/1.8.2; 'packs' lists what's installed (today: the instrument preset banks by category).",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","sounds","load","render","channels","packs"]},"kit":{"type":["string","integer"]},"track":{"type":["string","integer"]},"channel":{"type":["string","integer"]},"bank":{"type":"string"},"count":{"type":"integer"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             const auto action = str (a, "action", "list");
             auto kitIdx = [&] () -> int
             {
                 if (! has (a, "kit")) return -1;
                 if (a["kit"].isInt() || a["kit"].isDouble()) return juce::jlimit (0, DrumKit::numKits - 1, (int) a["kit"] - 1);
                 return DrumKit::kitIndexFor (str (a, "kit"));
             };
             if (action == "list")
             {
                 juce::Array<juce::var> out;
                 auto kitsDir = session.sessionsDir.getChildFile ("Kits");
                 for (int i = 0; i < DrumKit::numKits; ++i)
                 {
                     auto o = obj(); set (o, "index", i + 1); set (o, "name", DrumKit::kits[i].name); set (o, "description", DrumKit::kits[i].description);
                     set (o, "rendered", kitsDir.getChildFile (DrumKit::kits[i].name).findChildFiles (juce::File::findFiles, false, "*.wav").size() >= DrumKit::numSounds);
                     out.add (o);
                 }
                 auto res = obj(); set (res, "kits", out); set (res, "count", DrumKit::numKits);
                 juce::Array<juce::var> snd; for (int i = 0; i < DrumKit::numSounds; ++i) { auto s = obj(); set (s, "sound", DrumKit::sounds[i].name); set (s, "key", juce::MidiMessage::getMidiNoteName (DrumKit::sounds[i].note, true, true, 4)); set (s, "note", DrumKit::sounds[i].note); snd.add (s); }
                 set (res, "sounds", snd);
                 return ToolResult::ok (res);
             }
             if (action == "sounds")
             {
                 juce::Array<juce::var> snd; for (int i = 0; i < DrumKit::numSounds; ++i) { auto s = obj(); set (s, "sound", DrumKit::sounds[i].name); set (s, "key", juce::MidiMessage::getMidiNoteName (DrumKit::sounds[i].note, true, true, 4)); set (s, "note", DrumKit::sounds[i].note); set (s, "trim_db", DrumKit::gainsDb[i]); snd.add (s); }
                 return ToolResult::ok (juce::var (snd));
             }
             if (action == "render")
             {
                 const int k = kitIdx();
                 if (k < 0) { int n = 0; for (int i = 0; i < DrumKit::numKits; ++i) n += DrumKit::ensureKit (session, i).size(); return ToolResult::ok ("Rendered / verified " + juce::String (n) + " samples for " + juce::String (DrumKit::numKits) + " kits"); }
                 auto files = DrumKit::ensureKit (session, k);
                 return ToolResult::ok (juce::String (DrumKit::kits[k].name) + ": " + juce::String (files.size()) + " samples in " + session.sessionsDir.getChildFile ("Kits").getChildFile (DrumKit::kits[k].name).getFullPathName());
             }
             if (action == "channels")
             {
                 auto copy = a.clone(); copy.getDynamicObject()->setProperty ("action", "add"); copy.getDynamicObject()->setProperty ("type", "kit");
                 copy.getDynamicObject()->setProperty ("what", has (a, "kit") ? a["kit"] : juce::var ("Mothwing 808"));
                 return call ("rack", copy);
             }
             if (action == "load")
             {
                 juce::String err;
                 te::AudioTrack* t = has (a, "channel") ? rackChannelArg (a, err) : trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 SqHivePadsPlugin* pads = nullptr;
                 for (auto p : t->pluginList) if (auto hp = dynamic_cast<SqHivePadsPlugin*> (p)) pads = hp;
                 if (pads == nullptr)
                 {
                     // put Hive Pads on the track (replacing the instrument) so the kit has somewhere to go
                     auto copy = juce::var (new juce::DynamicObject()); copy.getDynamicObject()->setProperty ("action", "insert"); copy.getDynamicObject()->setProperty ("track", session.indexOf (t)); copy.getDynamicObject()->setProperty ("name", "Hive Pads");
                     call ("instrument", copy);
                     for (auto p : t->pluginList) if (auto hp = dynamic_cast<SqHivePadsPlugin*> (p)) pads = hp;
                     if (pads == nullptr) return ToolResult::error ("Couldn't put Hive Pads on " + t->getName());
                 }
                 const int k = kitIdx();
                 if (k < 0) return ToolResult::error ("Give kit (name, genre word or 1-" + juce::String (DrumKit::numKits) + ")");
                 if (has (a, "bank")) { auto b = str (a, "bank").toUpperCase().trim(); if (b.isNotEmpty() && b[0] >= 'A' && b[0] <= 'D') pads->currentBank = b[0] - 'A'; }
                 pads->loadKitByIndex (k);
                 if (ui.onRackChanged) ui.onRackChanged();
                 auto o = obj(); set (o, "track", t->getName()); set (o, "kit", DrumKit::kits[k].name); set (o, "bank", juce::String::charToString ((juce::juce_wchar) ('A' + pads->currentBank.get())));
                 juce::Array<juce::var> padsOut; for (int i = 0; i < DrumKit::numSounds; ++i) { const int pad = pads->currentBank.get() * 16 + i; auto p = obj(); set (p, "pad", pad + 1); set (p, "name", pads->padName (pad)); set (p, "key", juce::MidiMessage::getMidiNoteName (pads->padNote (pad), true, true, 4)); padsOut.add (p); }
                 set (o, "pads", padsOut);
                 return ToolResult::ok (o);
             }
             if (action == "packs")
             {
                 // today: the instrument preset banks (by instrument and category); the Pollen packs land in 1.8.1 / 1.8.2
                 juce::Array<juce::var> out;
                 for (auto& e : Signature::entries())
                 {
                     if (juce::String (e.category) != "Instruments") continue;
                     auto& bank = SignaturePresets::forName (e.name);
                     if (bank.empty()) continue;
                     std::map<juce::String, int> cats;
                     for (auto& p : bank) { auto n = juce::String (p.name); auto bar = n.indexOf ("|"); cats[bar > 0 ? n.substring (0, bar).trim() : "Classic"]++; }
                     auto o = obj(); set (o, "instrument", e.name); set (o, "presets", (int) bank.size());
                     juce::Array<juce::var> cs; for (auto& [c, n] : cats) { auto co = obj(); set (co, "category", c); set (co, "count", n); cs.add (co); }
                     set (o, "categories", cs); out.add (o);
                 }
                 auto res = obj(); set (res, "packs", out); set (res, "note", "Pollen sound packs: 11 packs ship in 1.8.1 (insert Pollen and use plugin_presets), more in 1.8.2; the preset banks above are what ships today.");
                 return ToolResult::ok (res);
             }
             return ToolResult::error ("Unknown action " + action);
         });

    //==========================================================================
    add ("chords",
         "Chord tools (the Chord Progression Tool and Chord Stamp, from Claude). Actions: "
         "'genres' lists the library genres; 'list' lists library progressions (filter with genre); "
         "'preview' returns the chord names and note numbers a progression would get in a key without writing anything (key, minor, progression | degrees, sevenths, octave); "
         "'generate' writes a progression into a MIDI clip (track + clip, or rack channel + pattern; with no clip it creates one at bar 1): "
         "progression = library name or index, or degrees = 'I - V - vi - IV' / '1 5 6 4' / 'ii7 V7 Imaj7' (lowercase = minor, 7 = seventh), key ('C'..'B' or 0-11), minor, "
         "bars_per_chord (0.5/1/2/4), bars (total; default = the clip), bass (808 line), voice_lead, sevenths, strum ('off'|'up'|'down', strum_beats), humanise, octave, velocity, replace; "
         "'stamp' writes one chord at a beat: note + type ('Minor 7', 'm7'...), mode 'bottom_up' (note = root, inversion) or 'top_down' (note stays on top), start_beat, length_beats, velocity; "
         "'nudge' moves the chord(s) at a beat (or every chord) by scale degrees in the key (degrees: +1 / -1), keeping the top note on top when keep_top=true; "
         "'detect' names the chords in a clip (per start beat); 'labels' reads / sets key labels on a track (label + note to set, clear=true to remove). "
         "Chord tracks are planned for 1.8.2 - until then write chords into the clip of each instrument.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["genres","list","preview","generate","stamp","nudge","detect","labels"]},
            "genre":{"type":"string"},"progression":{"type":["string","integer"]},"degrees":{"type":"string"},"key":{"type":["string","integer"]},"minor":{"type":"boolean"},"sevenths":{"type":"boolean"},
            "track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"channel":{"type":["string","integer"]},"pattern":{"type":["string","integer"]},
            "bars_per_chord":{"type":"number"},"bars":{"type":"number"},"start_bar":{"type":"number"},"bass":{"type":"boolean"},"voice_lead":{"type":"boolean"},"strum":{"type":"string"},"strum_beats":{"type":"number"},"humanise":{"type":"boolean"},
            "octave":{"type":"integer"},"velocity":{"type":"integer"},"replace":{"type":"boolean"},
            "note":{"type":["string","integer"]},"type":{"type":"string"},"mode":{"type":"string","enum":["bottom_up","top_down"]},"inversion":{"type":"integer"},"start_beat":{"type":"number"},"length_beats":{"type":"number"},
            "at_beat":{"type":"number"},"keep_top":{"type":"boolean"},"label":{"type":"string"},"clear":{"type":"boolean"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             const auto action = str (a, "action", "list");
             juce::String err;
             auto keyOf = [&] (int def) -> int
             {
                 if (! has (a, "key")) return def;
                 if (a["key"].isInt() || a["key"].isDouble()) return ((int) a["key"] % 12 + 12) % 12;
                 return parsePitch (a["key"]) % 12;
             };
             auto chordsFromArgs = [&] (bool& minorKey, bool& sevenths, juce::String& name) -> std::vector<MidiTools::ProgChord>
             {
                 if (has (a, "degrees")) { name = "Custom: " + str (a, "degrees"); return MidiTools::parseDegrees (str (a, "degrees")); }
                 auto& lib = MidiTools::progressions();
                 int idx = -1;
                 if (a["progression"].isInt() || a["progression"].isDouble()) idx = (int) a["progression"] - 1;
                 else
                 {
                     auto want = str (a, "progression");
                     for (int i = 0; i < (int) lib.size() && idx < 0; ++i) if (juce::String (lib[(size_t) i].name).equalsIgnoreCase (want)) idx = i;
                     for (int i = 0; i < (int) lib.size() && idx < 0; ++i) if (juce::String (lib[(size_t) i].name).containsIgnoreCase (want) && want.isNotEmpty()) idx = i;
                     // "I - V - vi - IV" given as the progression
                     if (idx < 0 && want.isNotEmpty()) { auto parsed = MidiTools::parseDegrees (want); if (! parsed.empty()) { name = "Custom: " + want; return parsed; } }
                 }
                 if (idx < 0 || idx >= (int) lib.size()) return {};
                 auto& p = lib[(size_t) idx];
                 if (! has (a, "minor")) minorKey = p.minorKey;
                 if (! has (a, "sevenths")) sevenths = p.sevenths;
                 name = p.name;
                 return p.chords;
             };
             // the clip to write into: track + clip, or rack channel + pattern
             auto targetClip = [&] (bool create) -> te::MidiClip*
             {
                 te::AudioTrack* t = nullptr;
                 if (has (a, "channel")) { t = rackChannelArg (a, err); if (! t) return nullptr; const int pat = patternArg (a); if (auto mc = Rack::midiClip (*t, pat)) return mc; if (auto sc = Rack::stepClip (*t, pat, false)) { err = "That rack channel's pattern holds steps, not notes - use 'rack to_piano_roll' first."; return nullptr; } }
                 else { t = trackArg (a, err); if (! t) return nullptr; }
                 if (has (a, "clip")) return dynamic_cast<te::MidiClip*> (clipArg (*t, a, err));
                 for (auto c : t->getClips()) if (auto mc = dynamic_cast<te::MidiClip*> (c)) return mc;
                 if (! create) { err = "No MIDI clip on " + t->getName(); return nullptr; }
                 const double bars = num (a, "bars", 4.0);
                 auto start = session.barToTime (num (a, "start_bar", 1.0));
                 auto len = session.barToTime (num (a, "start_bar", 1.0) + bars) - start;
                 auto clip = t->insertMIDIClip ("Chords", { start, start + len }, nullptr);
                 if (clip == nullptr) err = "Could not create a MIDI clip on " + t->getName();
                 return clip.get();
             };

             if (action == "genres") return ToolResult::ok (juce::var (MidiTools::progressionGenres()));
             if (action == "list")
             {
                 juce::Array<juce::var> out;
                 auto& lib = MidiTools::progressions();
                 for (int i = 0; i < (int) lib.size(); ++i)
                 {
                     auto& p = lib[(size_t) i];
                     if (has (a, "genre") && ! juce::String (p.mood).containsIgnoreCase (str (a, "genre"))) continue;
                     auto o = obj(); set (o, "index", i + 1); set (o, "name", p.name); set (o, "genre", p.mood); set (o, "minor_key", p.minorKey); set (o, "sevenths", p.sevenths);
                     juce::StringArray deg; for (auto& c : p.chords) deg.add (juce::String (c.degree) + (c.forceType >= 0 ? juce::String (" (") + MidiTools::chordTypes()[(size_t) c.forceType].name + ")" : juce::String()));
                     set (o, "degrees", deg.joinIntoString (" - "));
                     set (o, "in_c", MidiTools::chordNamesFor (p.chords, 0, p.minorKey, p.sevenths).joinIntoString (" - "));
                     out.add (o);
                 }
                 return ToolResult::ok (juce::var (out));
             }
             if (action == "preview" || action == "generate")
             {
                 bool minorKey = flag (a, "minor"), sevenths = flag (a, "sevenths"); juce::String name;
                 auto chords = chordsFromArgs (minorKey, sevenths, name);
                 if (chords.empty()) return ToolResult::error ("Give a progression (library name / index) or degrees like 'I - V - vi - IV'. 'chords list' shows the library.");
                 const int key = keyOf (0), octave = juce::jlimit (1, 7, (int) num (a, "octave", 4));
                 const bool voiceLead = has (a, "voice_lead") ? flag (a, "voice_lead") : true;
                 auto names = MidiTools::chordNamesFor (chords, key, minorKey, sevenths);
                 auto notes = MidiTools::progressionNotes (chords, key, minorKey, sevenths, octave, voiceLead);
                 auto o = obj(); set (o, "progression", name); set (o, "key", MidiTools::pitchClassName (key, MidiTools::keyUsesFlats (key, minorKey)) + (minorKey ? " minor" : " major"));
                 juce::Array<juce::var> cs;
                 for (int i = 0; i < (int) chords.size(); ++i)
                 {
                     auto c = obj(); set (c, "name", names[i]); set (c, "degree", chords[(size_t) i].degree);
                     juce::Array<juce::var> ns; juce::StringArray nn; for (auto n : notes[(size_t) i]) { ns.add (n); nn.add (juce::MidiMessage::getMidiNoteName (n, true, true, 4)); }
                     set (c, "notes", ns); set (c, "note_names", nn); cs.add (c);
                 }
                 set (o, "chords", cs);
                 if (action == "preview") return ToolResult::ok (o);

                 auto mc = targetClip (true);
                 if (mc == nullptr) return ToolResult::error (err);
                 MidiTools::ChordWriteOptions wo;
                 wo.keyRoot = key; wo.minorKey = minorKey; wo.sevenths = sevenths; wo.octave = octave; wo.voiceLead = voiceLead;
                 const int bpb = juce::jmax (1, mc->edit.tempoSequence.getTimeSig (0)->numerator.get());
                 wo.beatsPerChord = juce::jmax (0.25, num (a, "bars_per_chord", 1.0)) * bpb;
                 const double clipBeats = mc->getLengthInBeats().inBeats() + mc->getOffsetInBeats().inBeats();
                 wo.totalBeats = has (a, "bars") ? num (a, "bars", 4.0) * bpb : juce::jmax (wo.beatsPerChord * (double) chords.size(), std::floor (clipBeats / (wo.beatsPerChord * (double) chords.size()) + 1e-6) * wo.beatsPerChord * (double) chords.size());
                 wo.withBass = has (a, "bass") ? flag (a, "bass") : true;
                 wo.humanise = flag (a, "humanise");
                 wo.velocity = juce::jlimit (1, 127, (int) num (a, "velocity", 92));
                 auto strum = str (a, "strum", "off").toLowerCase();
                 if (strum == "up" || strum == "down") { wo.strumBeats = juce::jmax (0.01, num (a, "strum_beats", 0.125)); wo.strumUp = strum == "up"; }
                 wo.seed = juce::Random::getSystemRandom().nextInt();
                 if (flag (a, "replace")) mc->getSequence().clear (um());
                 MidiTools::Notes made;
                 auto written = MidiTools::writeChords (*mc, chords, wo, &made);
                 mc->state.setProperty ("sqmScaleRoot", key, um()); mc->state.setProperty ("sqmScale", minorKey ? 2 : 1, um());
                 set (o, "clip", mc->getName()); set (o, "track", mc->getTrack() != nullptr ? mc->getTrack()->getName() : juce::String());
                 set (o, "notes_written", made.size()); set (o, "bars_written", wo.totalBeats / bpb); set (o, "wrote", written.joinIntoString (" - "));
                 if (ui.onRackChanged) ui.onRackChanged();
                 return ToolResult::ok (o);
             }
             if (action == "stamp")
             {
                 auto mc = targetClip (true);
                 if (mc == nullptr) return ToolResult::error (err);
                 if (! has (a, "note")) return ToolResult::error ("Give the note (e.g. 'E4' or 64).");
                 const int note = parsePitch (a["note"]), type = MidiTools::chordIndex (str (a, "type", "Major"));
                 const double start = juce::jmax (0.0, num (a, "start_beat", 0.0)), len = juce::jmax (0.0625, num (a, "length_beats", 4.0));
                 const int vel = juce::jlimit (1, 127, (int) num (a, "velocity", 96));
                 MidiTools::Notes made = str (a, "mode", "bottom_up") == "top_down" ? MidiTools::stampChordTopDown (*mc, note, type, start, len, vel)
                                                                                     : MidiTools::stampChord (*mc, note, type, juce::jlimit (0, 6, (int) num (a, "inversion", 0)), start, len, vel);
                 juce::Array<juce::var> ns; juce::Array<int> plain; for (auto* n : made) { ns.add (n->getNoteNumber()); plain.add (n->getNoteNumber()); }
                 auto o = obj(); set (o, "chord", MidiTools::detectChord (plain)); set (o, "notes", ns); set (o, "mode", str (a, "mode", "bottom_up"));
                 if (ui.onRackChanged) ui.onRackChanged();
                 return ToolResult::ok (o);
             }
             if (action == "nudge")
             {
                 auto mc = targetClip (false);
                 if (mc == nullptr) return ToolResult::error (err);
                 const int steps = (int) num (a, "degrees", 1);
                 const int root = (int) mc->state.getProperty ("sqmScaleRoot", keyOf (0)), scale = juce::jmax (1, (int) mc->state.getProperty ("sqmScale", flag (a, "minor") ? 2 : 1));
                 const bool keepTop = flag (a, "keep_top");
                 MidiTools::Notes sel;
                 for (auto* n : mc->getSequence().getNotes())
                     if (! has (a, "at_beat") || std::abs (n->getStartBeat().inBeats() - num (a, "at_beat", 0.0)) < 1e-3) sel.add (n);
                 if (sel.isEmpty()) return ToolResult::error ("No notes at that beat.");
                 if (keepTop)
                 {
                     // group by start beat; move everything but the highest note of each group
                     std::map<long long, MidiTools::Notes> groups;
                     for (auto* n : sel) groups[(long long) std::llround (n->getStartBeat().inBeats() * 1000.0)].add (n);
                     MidiTools::Notes movable;
                     for (auto& [k, g] : groups) { te::MidiNote* top = nullptr; for (auto* n : g) if (top == nullptr || n->getNoteNumber() > top->getNoteNumber()) top = n; for (auto* n : g) if (n != top) movable.add (n); }
                     MidiTools::transposeInScale (*mc, movable, steps, root, scale);
                 }
                 else MidiTools::transposeInScale (*mc, sel, steps, root, scale);
                 if (ui.onRackChanged) ui.onRackChanged();
                 return ToolResult::ok ("Moved " + juce::String (sel.size()) + " notes by " + juce::String (steps) + " scale degree(s)" + (keepTop ? " (top notes kept)" : ""));
             }
             if (action == "detect")
             {
                 auto mc = targetClip (false);
                 if (mc == nullptr) return ToolResult::error (err);
                 std::map<long long, juce::Array<int>> groups;
                 for (auto* n : mc->getSequence().getNotes()) groups[(long long) std::llround (n->getStartBeat().inBeats() * 1000.0)].add (n->getNoteNumber());
                 const bool flats = MidiTools::keyUsesFlats ((int) mc->state.getProperty ("sqmScaleRoot", 0), (int) mc->state.getProperty ("sqmScale", 1) == 2);
                 juce::Array<juce::var> out;
                 for (auto& [k, ns] : groups)
                 {
                     if (ns.size() < 2) continue;
                     auto o = obj(); set (o, "start_beat", k / 1000.0); set (o, "chord", MidiTools::detectChord (ns, flats));
                     juce::Array<juce::var> nv; for (auto n : ns) nv.add (n); set (o, "notes", nv); out.add (o);
                 }
                 return ToolResult::ok (juce::var (out));
             }
             if (action == "labels")
             {
                 te::AudioTrack* t = has (a, "channel") ? rackChannelArg (a, err) : trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 if (has (a, "note") && (has (a, "label") || flag (a, "clear")))
                     KeyLabels::setUserLabel (*t, parsePitch (a["note"]), flag (a, "clear") ? juce::String() : str (a, "label"));
                 juce::Array<juce::var> out;
                 for (auto& [n, l] : KeyLabels::allLabels (*t)) { auto o = obj(); set (o, "note", n); set (o, "name", juce::MidiMessage::getMidiNoteName (n, true, true, 4)); set (o, "label", l); set (o, "user", KeyLabels::userLabel (*t, n).isNotEmpty()); out.add (o); }
                 return ToolResult::ok (juce::var (out));
             }
             return ToolResult::error ("Unknown action " + action);
         });
}
