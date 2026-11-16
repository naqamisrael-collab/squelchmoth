// Squelchmoth - Claude tools for Moth Tune (and the session key). Included inside the McpTools class body.

SqTunePlugin* findTune (te::AudioTrack& t)
{
    for (auto p : t.pluginList) if (auto tu = dynamic_cast<SqTunePlugin*> (p)) return tu;
    return nullptr;
}

static int pitchClassArg (const juce::var& v)
{
    if (v.isInt() || v.isDouble()) return ((int) v % 12 + 12) % 12;
    return MidiTools::rootIndex (v.toString());
}

static int maskArg (const juce::var& v)
{
    int m = 0;
    if (v.isArray()) for (auto& e : *v.getArray()) m |= 1 << pitchClassArg (e);
    else for (auto& t : juce::StringArray::fromTokens (v.toString(), " ,", "")) if (t.isNotEmpty()) m |= 1 << pitchClassArg (t);
    return m;
}

static juce::String maskText (int m)
{
    juce::StringArray s;
    for (int i = 0; i < 12; ++i) if ((m >> i) & 1) s.add (SqScales::noteNames()[i]);
    return s.joinIntoString (" ");
}

juce::var describeTune (SqTunePlugin& tu)
{
    auto o = obj();
    set (o, "track", tu.getOwnerTrack() != nullptr ? tu.getOwnerTrack()->getName() : juce::String());
    set (o, "engine", SqTunePlugin::engineNames()[tu.engine.get()].toLowerCase());
    set (o, "latency_ms", SqTunePlugin::latencyFor (tu.engine.get(), tu.voiceType.get(), tu.currentRate()) * 1000.0 / tu.currentRate());
    set (o, "character", SqTunePlugin::characterNames()[tu.character.get()].toLowerCase());
    set (o, "voice_type", SqTunePlugin::voiceNames()[tu.voiceType.get()]);
    set (o, "key", SqScales::noteNames()[tu.keyRoot.get()]);
    set (o, "scale", SqTunePlugin::scaleNames()[tu.scaleIndex.get()]);
    set (o, "notes_corrected_to", maskText (tu.allowedMask()));
    set (o, "removed", maskText (tu.removeMask.get()));
    set (o, "bypassed", maskText (tu.bypassMask.get()));
    set (o, "auto_key", tu.autoKey.get());
    set (o, "formant_preserve", tu.preserve.get());
    set (o, "conform_transpose", tu.conform.get());
    set (o, "vibrato_shape", SqTunePlugin::vibShapeNames()[tu.vibShape.get()].toLowerCase());
    set (o, "midi", SqTunePlugin::midiModeNames()[tu.midiMode.get()].toLowerCase());
    set (o, "midi_track", tu.midiTrack.get());
    set (o, "lead_follows_midi", tu.midiLead.get());
    set (o, "graph_notes", (int) tu.getGraphNotes().size());
    set (o, "graph_enabled", tu.graphOn.get());
    // Moth Tune 2
    set (o, "dynamic_vibrato", SqTunePlugin::dvModeNames()[juce::jlimit (0, 3, tu.dvMode.get())].toLowerCase());
    set (o, "vibrato_fx_straight_notes_only", tu.vibFxStraight.get());
    set (o, "formant_link", tu.formantLink.get());
    set (o, "auto_range", tu.autoRange.get());
    { float lo, hi; if (tu.detectedRange (lo, hi)) { set (o, "detected_range", Skins::noteLabel (lo) + " - " + Skins::noteLabel (hi)); const int sv = tu.suggestedVoiceType(); if (sv >= 0) set (o, "suggested_voice_type", SqTunePlugin::voiceNames()[sv]); } }
    set (o, "harmony_trigger", SqTunePlugin::triggerNames()[juce::jlimit (0, 2, tu.harmTrigger.get())].toLowerCase());
    set (o, "harmony_cc", tu.harmCC.get()); set (o, "harmony_latch", tu.harmLatch.get()); set (o, "harmony_open", tu.harmGateOpen.load());
    auto params = obj();
    for (size_t i = 0; i < tu.bank.params.size(); ++i)
        set (params, tu.bank.specs[i].name, SqDsp::numText (tu.bank[i], tu.bank.specs[i].decimals) + tu.bank.specs[i].unit);
    set (o, "parameters", params);
    juce::Array<juce::var> harm;
    for (int i = 0; i < 4; ++i)
    {
        auto h = obj();
        set (h, "voice", i + 1);
        set (h, "mode", SqTunePlugin::harmModeNames()[tu.harmMode[(size_t) i].get()].toLowerCase());
        harm.add (h);
    }
    set (o, "harmony", harm);
    auto live = obj();
    set (live, "singing", Skins::noteLabel (tu.liveIn.load()));
    set (live, "target", Skins::noteLabel (tu.liveTarget.load()));
    set (live, "correction_cents", tu.liveCents.load());
    set (live, "vibrato_rate_hz", tu.liveVibRate.load()); set (live, "vibrato_depth_cents", tu.liveVibDepth.load()); set (live, "vibrato_acted_on", tu.liveVibAmount.load());
    set (o, "live", live);
    return o;
}

juce::String applyTuneSetting (SqTunePlugin& tu, const juce::String& keyIn, const juce::var& v)
{
    auto* um = &tu.edit.getUndoManager();
    const auto key = keyIn.toLowerCase().replaceCharacter (' ', '_').replaceCharacter ('-', '_');
    auto pickIndex = [&] (const juce::StringArray& names) -> int
    {
        if (v.isInt() || v.isDouble() || v.isBool()) return juce::jlimit (0, names.size() - 1, (int) v);
        for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (v.toString().trim())) return i;
        for (int i = 0; i < names.size(); ++i) if (names[i].containsIgnoreCase (v.toString().trim()) && v.toString().trim().isNotEmpty()) return i;
        return -1;
    };
    auto setIdx = [&] (juce::CachedValue<int>& cv, const juce::StringArray& names) -> juce::String
    {
        const int i = pickIndex (names);
        if (i < 0) return "unknown value '" + v.toString() + "' for " + keyIn + " (choose " + names.joinIntoString (", ") + ")";
        cv.setValue (i, um); return {};
    };
    auto asBool = [&] { return v.isBool() ? (bool) v : (v.toString().equalsIgnoreCase ("on") || v.toString().equalsIgnoreCase ("true") || (double) v > 0.5); };

    if (key == "engine")             return setIdx (tu.engine, SqTunePlugin::engineNames());
    if (key == "character" || key == "mode") return setIdx (tu.character, SqTunePlugin::characterNames());
    if (key == "voice_type" || key == "voice" || key == "input_type") return setIdx (tu.voiceType, SqTunePlugin::voiceNames());
    if (key == "key" || key == "root") { tu.keyRoot.setValue (pitchClassArg (v), um); return {}; }
    if (key == "scale")
    {
        const int i = v.toString().equalsIgnoreCase ("minor") ? 2 : v.toString().equalsIgnoreCase ("custom") ? (int) SqScales::scales().size() : MidiTools::scaleIndex (v.toString());
        tu.scaleIndex.setValue (i, um); return {};
    }
    if (key == "custom_scale" || key == "custom_notes") { tu.customMask.setValue (maskArg (v), um); tu.scaleIndex.setValue ((int) SqScales::scales().size(), um); return {}; }
    if (key == "remove" || key == "removed")   { tu.removeMask.setValue (maskArg (v), um); return {}; }
    if (key == "bypass" || key == "bypassed")  { tu.bypassMask.setValue (maskArg (v), um); return {}; }
    if (key == "auto_key")           { tu.autoKey.setValue (asBool(), um); return {}; }
    if (key == "learn_scale")        { tu.learnScale.setValue (asBool(), um); return {}; }
    if (key == "formant_preserve" || key == "preserve") { tu.preserve.setValue (asBool(), um); return {}; }
    if (key == "conform" || key == "conform_transpose") { tu.conform.setValue (asBool(), um); return {}; }
    if (key == "all_octaves")        { tu.allOctaves.setValue (asBool(), um); return {}; }
    if (key == "graph" || key == "graph_enabled") { tu.graphOn.setValue (asBool(), um); return {}; }
    if (key == "lead_follows_midi")  { tu.midiLead.setValue (asBool(), um); return {}; }
    if (key == "vibrato_shape")      return setIdx (tu.vibShape, SqTunePlugin::vibShapeNames());
    if (key == "dynamic_vibrato" || key == "dv_mode" || key == "dvmode") return setIdx (tu.dvMode, SqTunePlugin::dvModeNames());
    if (key == "vibrato_fx_straight_notes_only" || key == "vibfxstraight") { tu.vibFxStraight.setValue (asBool(), um); return {}; }
    if (key == "formant_link" || key == "formantlink") { tu.formantLink.setValue (asBool(), um); return {}; }
    if (key == "auto_range" || key == "autorange") { tu.autoRange.setValue (asBool(), um); if (asBool()) tu.clearRange(); return {}; }
    if (key == "harmony_trigger" || key == "harmtrigger") return setIdx (tu.harmTrigger, SqTunePlugin::triggerNames());
    if (key == "harmony_cc" || key == "harmcc") { tu.harmCC.setValue (juce::jlimit (0, 127, (int) v), um); return {}; }
    if (key == "harmony_latch" || key == "harmlatch") { tu.harmLatch.setValue (asBool(), um); return {}; }
    if (key == "midi" || key == "midi_targets")
    {
        auto t = v.toString().toLowerCase();
        tu.midiMode.setValue (t == "off" || t == "false" || t == "0" ? 0 : t.contains ("named") || t == "2" ? 2 : 1, um); return {};
    }
    if (key == "midi_track")         { tu.midiTrack.setValue (v.toString(), um); if (v.toString().isNotEmpty()) tu.midiMode.setValue (2, um); return {}; }
    // automatable parameters, by name or id
    if (auto ap = Signature::findParam (tu, keyIn.replaceCharacter ('_', ' ')))
    {
        ap->setParameter (ap->getValueRange().clipValue ((float) (double) v), juce::sendNotification);
        return {};
    }
    return "unknown setting '" + keyIn + "'";
}

void registerVocalTools()
{
    add ("moth_tune",
         "Moth Tune - Squelchmoth's flagship vocal tuner. Works on the named track's Moth Tune (action add inserts one). Actions:\n"
         " get - every setting, live readout (note being sung, target, correction in cents) and latency.\n"
         " add - insert Moth Tune on the track (optionally with preset).\n"
         " set - settings: {name: value}. Names: any parameter (Moth Tune 2: Vibrato Threshold ct, Vibrato Detect Delay s, Vibrato Attack s, Vibrato Irregularity %, Pitch Depth %, Amplitude Depth %, Timbre Depth %; Retune Speed 0-400 ms, Humanize %, Flex Tune %, Vibrato Keep %, Natural Vibrato %, Correction %, Transpose st, Formant st, Throat %, Detune ct, Tracking, Mix %, Output dB, Vibrato Rate/Depth/Delay/Onset/Variation, Voice N Level/Pan/Interval/Formant, Harmony Detune, Harmony Timing) or engine (live/studio), character (modern/classic), voice_type (soprano, alto/tenor, low male, instrument, bass instrument, auto), key, scale, custom_scale ['C','E','G'...], remove [...], bypass [...], auto_key, formant_preserve, conform, vibrato_shape, midi (off/this/named), midi_track, lead_follows_midi, all_octaves, graph,\n"
         "   dynamic_vibrato (off / preserve - keeps the singer's vibrato exactly, even with Retune 0 / cut / boost), vibrato_fx_straight_notes_only (created vibrato only on straight notes),\n"
         "   formant_link (Formant follows Transpose), auto_range (detect the voice range and pick the voice type), harmony_trigger (always / midi note / midi cc), harmony_cc (0-127), harmony_latch, motion_style (hard - steps jump, soft - steps glide and rests fade, legato - long glides, rests hold the note, the pattern runs on across phrases).\n"
         " key - set key (root) and scale; session true also makes it the session key (Auto-Key follows it).\n"
         " detect_key - find the key from the session's MIDI and this track's singing; apply true sets it.\n"
         " harmony - voice 1-4: mode (off, interval, scale, midi), interval (semitones, or scale steps in scale mode), level dB, pan -100..100, formant st.\n"
         " graph_analyse - read the track's audio and replace the graph notes with the notes found; graph_list - the notes; graph_correct - snap notes to the scale (strength 0-100, notes [indexes] or all); graph_edit - edits [{index, pitch (note name or MIDI), shift (semitones), retune ms, vibrato %, formant st, drift %}]; graph_clear.\n"
         " preset - load a factory preset by name (list with preset name 'list'); open / open_graph - show the plug-in or the Graph window.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["get","add","set","key","detect_key","harmony","graph_analyse","graph_list","graph_correct","graph_edit","graph_clear","preset","open","open_graph"]},
            "settings":{"type":"object"},"root":{"type":["string","integer"]},"key":{"type":["string","integer"]},"scale":{"type":"string"},"session":{"type":"boolean"},"apply":{"type":"boolean"},
            "voice":{"type":"integer"},"mode":{"type":"string"},"interval":{"type":"number"},"level":{"type":"number"},"pan":{"type":"number"},"formant":{"type":"number"},
            "strength":{"type":"number"},"notes":{"type":"array"},"edits":{"type":"array"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a)
         {
             juce::String err;
             auto t = trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             const auto action = str (a, "action").toLowerCase();
             auto* tu = findTune (*t);
             auto* um = &session.edit->getUndoManager();
             if (action == "add")
             {
                 if (tu == nullptr)
                 {
                     auto p = session.createPluginByName ("Moth Tune", err);
                     if (p == nullptr) return ToolResult::error (err);
                     t->pluginList.insertPlugin (p, Session::insertEndIndex (t->pluginList, t), nullptr);
                     tu = dynamic_cast<SqTunePlugin*> (p.get());
                 }
                 if (tu != nullptr && has (a, "preset"))
                     if (auto e = Signature::findByName ("Moth Tune"))
                         for (auto& pr : e->presets) if (juce::String (pr.name).containsIgnoreCase (str (a, "preset"))) { Signature::applyPreset (*tu, pr); break; }
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeTune (*tu)));
             }
             if (tu == nullptr) return ToolResult::error ("No Moth Tune on '" + t->getName() + "'. Use action add first.");

             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeTune (*tu)));
             if (action == "set")
             {
                 auto s = a["settings"];
                 if (! s.isObject()) return ToolResult::error ("settings must be an object like {\"Retune Speed\": 10, \"scale\": \"minor\"}");
                 juce::StringArray problems;
                 for (auto& p : s.getDynamicObject()->getProperties())
                     if (auto e = applyTuneSetting (*tu, p.name.toString(), p.value); e.isNotEmpty()) problems.add (e);
                 auto out = describeTune (*tu);
                 return problems.isEmpty() ? ToolResult::ok (juce::JSON::toString (out)) : ToolResult::error (problems.joinIntoString ("; ") + "\n" + juce::JSON::toString (out));
             }
             if (action == "key")
             {
                 if (has (a, "root") || has (a, "key")) tu->keyRoot.setValue (pitchClassArg (has (a, "root") ? a["root"] : a["key"]), um);   // "key" accepted as an alias
                 if (has (a, "scale")) applyTuneSetting (*tu, "scale", a["scale"]);
                 if (flag (a, "session"))
                 {
                     session.edit->state.setProperty ("sqmKeyRoot", tu->keyRoot.get(), um);
                     session.edit->state.setProperty ("sqmKeyScale", tu->scaleIndex.get(), um);
                 }
                 return ToolResult::ok ("Moth Tune key: " + SqScales::noteNames()[tu->keyRoot.get()] + " " + SqTunePlugin::scaleNames()[tu->scaleIndex.get()]
                                        + " (corrects to " + maskText (tu->allowedMask()) + ")" + (flag (a, "session") ? " - also the session key" : ""));
             }
             if (action == "detect_key")
             {
                 auto k = TuneGraph::detectKey (*session.edit, tu);
                 if (flag (a, "apply", true)) TuneGraph::applyKey (*session.edit, tu, k);
                 return ToolResult::ok ("Key: " + SqScales::noteNames()[k.root] + (k.minor ? " minor" : " major") + " (confidence " + juce::String (k.score, 2) + ")"
                                        + (flag (a, "apply", true) ? " - set on Moth Tune and as the session key" : ""));
             }
             if (action == "harmony")
             {
                 const int v = (int) num (a, "voice", 1) - 1;
                 if (! juce::isPositiveAndBelow (v, 4)) return ToolResult::error ("voice must be 1-4");
                 const auto n = juce::String (v + 1);
                 if (has (a, "mode"))
                 {
                     const auto m = str (a, "mode").toLowerCase();
                     const int idx = m == "off" ? 0 : m.startsWith ("int") ? 1 : m.startsWith ("sc") ? 2 : m.startsWith ("mid") ? 3 : -1;
                     if (idx < 0) return ToolResult::error ("mode: off, interval, scale or midi");
                     tu->harmMode[(size_t) v].setValue (idx, um);
                 }
                 for (auto [k, pn] : std::initializer_list<std::pair<const char*, juce::String>> { { "interval", "Voice " + n + " Interval" }, { "level", "Voice " + n + " Level" }, { "pan", "Voice " + n + " Pan" }, { "formant", "Voice " + n + " Formant" } })
                     if (has (a, k)) applyTuneSetting (*tu, pn, a[k]);
                 return ToolResult::ok (juce::JSON::toString (describeTune (*tu)["harmony"]));
             }
             if (action == "graph_analyse")
             {
                 const int nn = TuneGraph::analyse (*tu, true, err);
                 if (nn < 0) return ToolResult::error (err);
                 return ToolResult::ok ("Found " + juce::String (nn) + " notes on " + t->getName() + ". graph_list shows them; graph_correct snaps them to the scale.");
             }
             if (action == "graph_list")
             {
                 juce::Array<juce::var> out;
                 auto notes = tu->getGraphNotes();
                 for (size_t i = 0; i < notes.size(); ++i)
                 {
                     auto& gn = notes[i]; auto o = obj();
                     set (o, "index", (int) i); set (o, "start_s", gn.start); set (o, "end_s", gn.end);
                     set (o, "sung", Skins::noteLabel (gn.orig) + juce::String::formatted (" %+.0f ct", (gn.orig - std::round (gn.orig)) * 100.0f));
                     set (o, "pitch", Skins::noteLabel (gn.pitch) + juce::String::formatted (" %+.0f ct", (gn.pitch - std::round (gn.pitch)) * 100.0f));
                     set (o, "retune_ms", gn.retuneMs); set (o, "vibrato_pct", gn.vibScale * 100.0f); set (o, "formant_st", gn.formantSt); set (o, "drift_pct", gn.drift * 100.0f);
                     out.add (o);
                 }
                 return ToolResult::ok (juce::JSON::toString (juce::var (out)));
             }
             if (action == "graph_correct")
             {
                 auto notes = tu->getGraphNotes();
                 std::vector<int> only;
                 if (a["notes"].isArray()) for (auto& v : *a["notes"].getArray()) only.push_back ((int) v);
                 TuneGraph::correctToScale (notes, tu->allowedMask(), (float) num (a, "strength", 100) / 100.0f, only.empty() ? nullptr : &only);
                 um->beginNewTransaction ("Graph: correct"); tu->setGraphNotes (notes, um);
                 return ToolResult::ok ("Corrected " + juce::String (only.empty() ? (int) notes.size() : (int) only.size()) + " notes to " + maskText (tu->allowedMask()));
             }
             if (action == "graph_edit")
             {
                 auto notes = tu->getGraphNotes();
                 auto edits = a["edits"];
                 if (! edits.isArray()) return ToolResult::error ("edits must be an array of {index, pitch|shift, retune, vibrato, formant, drift}");
                 for (auto& e : *edits.getArray())
                 {
                     const int i = (int) e.getProperty ("index", -1);
                     if (! juce::isPositiveAndBelow (i, (int) notes.size())) return ToolResult::error ("note index " + juce::String (i) + " out of range (0-" + juce::String ((int) notes.size() - 1) + ")");
                     auto& gn = notes[(size_t) i];
                     if (e.hasProperty ("pitch")) { auto pv = e["pitch"]; gn.pitch = pv.isString() ? (float) parsePitch (pv) : (float) (double) pv; }
                     if (e.hasProperty ("shift")) gn.pitch += (float) (double) e["shift"];
                     if (e.hasProperty ("retune")) gn.retuneMs = (float) (double) e["retune"];
                     if (e.hasProperty ("vibrato")) gn.vibScale = (float) (double) e["vibrato"] / 100.0f;
                     if (e.hasProperty ("formant")) gn.formantSt = (float) (double) e["formant"];
                     if (e.hasProperty ("drift")) gn.drift = (float) (double) e["drift"] / 100.0f;
                 }
                 um->beginNewTransaction ("Graph: edit"); tu->setGraphNotes (notes, um);
                 return ToolResult::ok ("Edited " + juce::String (edits.size()) + " notes");
             }
             if (action == "graph_clear") { um->beginNewTransaction ("Graph: clear"); tu->setGraphNotes ({}, um); return ToolResult::ok ("Graph cleared - Moth Tune is fully automatic again"); }
             if (action == "preset")
             {
                 auto e = Signature::findByName ("Moth Tune");
                 juce::StringArray names; for (auto& pr : e->presets) names.add (pr.name);
                 const auto want = str (a, "preset");
                 for (auto& pr : e->presets) if (want.isNotEmpty() && juce::String (pr.name).containsIgnoreCase (want)) { Signature::applyPreset (*tu, pr); return ToolResult::ok ("Loaded '" + juce::String (pr.name) + "'"); }
                 return ToolResult::ok ("Presets: " + names.joinIntoString (", "));
             }
             if (action == "open") { tu->showWindowExplicitly(); return ToolResult::ok ("Moth Tune window open"); }
             if (action == "open_graph") { if (ui.openTuneGraph) ui.openTuneGraph (*tu); return ToolResult::ok ("Graph window open"); }
             return ToolResult::error ("unknown action");
         });
}

SqTuneFxPlugin* findTuneFx (te::AudioTrack& t)
{
    for (auto p : t.pluginList) if (auto f = dynamic_cast<SqTuneFxPlugin*> (p)) return f;
    return nullptr;
}

juce::var describeTuneFx (SqTuneFxPlugin& fx)
{
    auto o = obj();
    set (o, "track", fx.getOwnerTrack() != nullptr ? fx.getOwnerTrack()->getName() : juce::String());
    set (o, "engine", SqTunePlugin::engineNames()[fx.engine.get()].toLowerCase());
    set (o, "key", SqScales::noteNames()[fx.keyRoot.get()] + " " + SqTunePlugin::scaleNames()[fx.scaleIndex.get()]);
    auto mo = obj();
    set (mo, "mode", SqTuneFxPlugin::motionModeNames()[fx.motionMode.get()].toLowerCase());
    set (mo, "sync", SqTuneFxPlugin::motionSyncNames()[fx.motionSync.get()]);
    set (mo, "length", fx.motionLen.get());
    { auto toks = juce::StringArray::fromTokens (fx.motionSteps.get(), " ", ""); toks.removeRange (fx.motionLen.get(), 99); set (mo, "steps", toks.joinIntoString (" ")); }
    set (mo, "scale_steps", fx.motionDiatonic.get());
    set (mo, "latch", fx.motionLatch.get());
    set (mo, "style", SqTuneFxPlugin::motionStyleNames()[juce::jlimit (0, 2, fx.motionStyle.get())].toLowerCase());
    set (o, "motion", mo);
    juce::Array<juce::var> slots;
    for (int i = 0; i < SqTuneFxPlugin::numSlots; ++i)
    {
        auto so = obj();
        const int t = fx.slotType[(size_t) i].get();
        set (so, "slot", i + 1); set (so, "module", SqVocalFx::moduleNames()[t].toLowerCase()); set (so, "on", fx.slotOn[(size_t) i].get());
        auto ax = SqVocalFx::axisNames (t);
        set (so, juce::String (ax.first).toLowerCase() + " (x)", fx.bank[(size_t) (SqTuneFxPlugin::s1x + i * 3)]);
        set (so, juce::String (ax.second).toLowerCase() + " (y)", fx.bank[(size_t) (SqTuneFxPlugin::s1y + i * 3)]);
        set (so, "mix_pct", fx.bank[(size_t) (SqTuneFxPlugin::s1mix + i * 3)]);
        set (so, "trigger", SqTuneFxPlugin::slotTrigNames()[juce::jlimit (0, 3, fx.slotTrig[(size_t) i].get())].toLowerCase());
        slots.add (so);
    }
    set (o, "rack", slots);
    auto params = obj();
    for (size_t i = 0; i < (size_t) SqTuneFxPlugin::s1x; ++i) set (params, fx.bank.specs[i].name, SqDsp::numText (fx.bank[i], fx.bank.specs[i].decimals) + fx.bank.specs[i].unit);
    set (o, "parameters", params);
    return o;
}

void registerVocalFxTools()
{
    add ("moth_tune_fx",
         "Moth Tune FX 2 - creative tuning with an editable Motion pattern and a six-slot X/Y effects rack (21 modules, each slot with its own trigger), 140 presets. Actions:\n"
         " get; add (optional preset); preset (name, or 'list'); open.\n"
         " set - settings {name: value}: parameters (Retune Speed, Humanize, Transpose, Formant, Throat, Motion Rate, Motion Glide, Mix, Output), engine, key, scale, auto_key, formant_preserve,\n"
         "   motion_mode (off/auto/momentary/toggle/always), motion_sync (free,1/4,1/8,1/8T,1/16,1/16T,1/32), motion_steps ('0 2 4 x 7' - scale steps, x = rest), motion_length 1-16, pattern (factory name), scale_steps (true = diatonic steps, false = semitones), latch.\n"
         " slot - slot 1-6: module (empty, mutate, duet, vocode, tube, breath, reverb, echo, chorus, compress, low-pass, high-pass, tilt eq, octaver, bit-crush, talkbox, stutter, eq, phaser, flanger, auto-pan, fuzz), x 0-1, y 0-1, mix 0-100, on,\n"
         "   trigger (always | auto - while singing | momentary - while a MIDI note or HOLD is down | toggle - each MIDI note switches it).\n"
         " swap - move a slot: from, to (1-6).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["get","add","set","slot","swap","preset","open"]},
            "settings":{"type":"object"},"slot":{"type":"integer"},"module":{"type":"string"},"x":{"type":"number"},"y":{"type":"number"},"mix":{"type":"number"},"on":{"type":"boolean"},
            "from":{"type":"integer"},"to":{"type":"integer"},"preset":{"type":"string"},"trigger":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a)
         {
             juce::String err;
             auto t = trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             const auto action = str (a, "action").toLowerCase();
             auto* fx = findTuneFx (*t);
             auto* um = &session.edit->getUndoManager();
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 auto e = Signature::findByName ("Moth Tune FX 2");
                 juce::StringArray names; for (auto& pr : e->presets) names.add (pr.name);
                 for (auto& pr : e->presets) if (want.isNotEmpty() && juce::String (pr.name).containsIgnoreCase (want))
                 {
                     Signature::applyPreset (*fx, pr);
                     for (auto& prop : pr.props) if (juce::String (prop.id) == "motionPattern") fx->applyPattern ((int) prop.value, um);
                     return "Loaded '" + juce::String (pr.name) + "'";
                 }
                 return "Presets: " + names.joinIntoString (", ");
             };
             if (action == "add")
             {
                 if (fx == nullptr)
                 {
                     auto p = session.createPluginByName ("Moth Tune FX 2", err);
                     if (p == nullptr) return ToolResult::error (err);
                     t->pluginList.insertPlugin (p, Session::insertEndIndex (t->pluginList, t), nullptr);
                     fx = dynamic_cast<SqTuneFxPlugin*> (p.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (describeTuneFx (*fx));
             }
             if (fx == nullptr) return ToolResult::error ("No Moth Tune FX on '" + t->getName() + "'. Use action add first.");
             if (action == "get") return ToolResult::ok (describeTuneFx (*fx));
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             if (action == "open") { fx->showWindowExplicitly(); return ToolResult::ok ("Moth Tune FX window open"); }
             if (action == "swap")
             {
                 fx->swapSlots ((int) num (a, "from", 1) - 1, (int) num (a, "to", 1) - 1, um);
                 return ToolResult::ok (describeTuneFx (*fx)["rack"]);
             }
             if (action == "slot")
             {
                 const int i = (int) num (a, "slot", 1) - 1;
                 if (! juce::isPositiveAndBelow (i, SqTuneFxPlugin::numSlots)) return ToolResult::error ("slot must be 1-6");
                 if (has (a, "module"))
                 {
                     const int m = SqVocalFx::moduleNames().indexOf (str (a, "module").toUpperCase().replace ("LOWPASS", "LOW-PASS").replace ("HIGHPASS", "HIGH-PASS").replace ("BITCRUSH", "BIT-CRUSH").replace ("TILT", "TILT EQ").replace ("TILT EQ EQ", "TILT EQ").replace ("AUTOPAN", "AUTO-PAN").replace ("AUTO PAN", "AUTO-PAN"));
                     if (m < 0) return ToolResult::error ("unknown module - choose " + SqVocalFx::moduleNames().joinIntoString (", ").toLowerCase());
                     fx->slotType[(size_t) i].setValue (m, um); fx->slotOn[(size_t) i].setValue (true, um);
                 }
                 if (has (a, "on")) fx->slotOn[(size_t) i].setValue (flag (a, "on"), um);
                 if (has (a, "trigger"))
                 {
                     const int tr = SqTuneFxPlugin::slotTrigNames().indexOf (str (a, "trigger").toUpperCase());
                     if (tr < 0) return ToolResult::error ("trigger must be always, auto, momentary or toggle");
                     fx->slotTrig[(size_t) i].setValue (tr, um);
                 }
                 if (has (a, "x")) fx->bank.params[(size_t) (SqTuneFxPlugin::s1x + i * 3)]->setParameter ((float) juce::jlimit (0.0, 1.0, num (a, "x")), juce::sendNotification);
                 if (has (a, "y")) fx->bank.params[(size_t) (SqTuneFxPlugin::s1y + i * 3)]->setParameter ((float) juce::jlimit (0.0, 1.0, num (a, "y")), juce::sendNotification);
                 if (has (a, "mix")) fx->bank.params[(size_t) (SqTuneFxPlugin::s1mix + i * 3)]->setParameter ((float) juce::jlimit (0.0, 100.0, num (a, "mix")), juce::sendNotification);
                 return ToolResult::ok (describeTuneFx (*fx)["rack"]);
             }
             if (action == "set")
             {
                 auto s = a["settings"];
                 if (! s.isObject()) return ToolResult::error ("settings must be an object");
                 juce::StringArray problems;
                 for (auto& p : s.getDynamicObject()->getProperties())
                 {
                     const auto k = p.name.toString().toLowerCase().replaceCharacter (' ', '_');
                     const auto v = p.value;
                     auto pick = [&] (const juce::StringArray& names, juce::CachedValue<int>& cv) { for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (v.toString())) { cv.setValue (i, um); return true; } if (v.isInt() || v.isDouble()) { cv.setValue ((int) v, um); return true; } return false; };
                     auto asBool = [&] { return v.isBool() ? (bool) v : v.toString().equalsIgnoreCase ("on") || v.toString().equalsIgnoreCase ("true") || (double) v > 0.5; };
                     bool ok = true;
                     if (k == "engine") ok = pick (SqTunePlugin::engineNames(), fx->engine);
                     else if (k == "voice_type") ok = pick (SqTunePlugin::voiceNames(), fx->voiceType);
                     else if (k == "key") fx->keyRoot.setValue (pitchClassArg (v), um);
                     else if (k == "scale") fx->scaleIndex.setValue (v.toString().equalsIgnoreCase ("minor") ? 2 : MidiTools::scaleIndex (v.toString()), um);
                     else if (k == "auto_key") fx->autoKey.setValue (asBool(), um);
                     else if (k == "formant_preserve") fx->preserve.setValue (asBool(), um);
                     else if (k == "motion_mode") ok = pick (SqTuneFxPlugin::motionModeNames(), fx->motionMode);
                     else if (k == "motion_sync") ok = pick (SqTuneFxPlugin::motionSyncNames(), fx->motionSync);
                     else if (k == "motion_length") fx->motionLen.setValue (juce::jlimit (1, 16, (int) v), um);
                     else if (k == "scale_steps") fx->motionDiatonic.setValue (asBool(), um);
                     else if (k == "latch") fx->motionLatch.setValue (asBool(), um);
                     else if (k == "motion_style") ok = pick (SqTuneFxPlugin::motionStyleNames(), fx->motionStyle);
                     else if (k == "motion_steps")
                     {
                         juce::StringArray toks;
                         if (v.isArray()) for (auto& e : *v.getArray()) toks.add (e.isVoid() ? "x" : e.toString());
                         else toks = juce::StringArray::fromTokens (v.toString(), " ,", "");
                         const int len = toks.size();
                         while (toks.size() < 16) toks.add ("0");
                         fx->motionSteps.setValue (toks.joinIntoString (" "), um);
                         if (len > 0) fx->motionLen.setValue (juce::jlimit (1, 16, len), um);
                         if (fx->motionMode.get() == 0) fx->motionMode.setValue (1, um);
                     }
                     else if (k == "pattern")
                     {
                         int idx = -1; auto& fp = SqTuneFxPlugin::factoryPatterns();
                         for (int i = 0; i < (int) fp.size() && idx < 0; ++i) if (juce::String (fp[(size_t) i].name).containsIgnoreCase (v.toString())) idx = i;
                         if (idx < 0) { juce::StringArray n; for (auto& x : fp) n.add (x.name); problems.add ("patterns: " + n.joinIntoString (", ")); continue; }
                         fx->applyPattern (idx + 1, um);
                     }
                     else if (auto ap = Signature::findParam (*fx, p.name.toString().replaceCharacter ('_', ' '))) ap->setParameter (ap->getValueRange().clipValue ((float) (double) v), juce::sendNotification);
                     else ok = false;
                     if (! ok) problems.add ("couldn't set " + p.name.toString());
                 }
                 auto out = describeTuneFx (*fx);
                 return problems.isEmpty() ? ToolResult::ok (out) : ToolResult::error (problems.joinIntoString ("; ") + "\n" + juce::JSON::toString (out));
             }
             return ToolResult::error ("unknown action");
         });
}

void registerMoltTools()
{
    add ("molt",
         "Molt - the AudioSuite time & pitch tool (X-Form class). Renders the clips of a track (or the named clip; tracks [..] with multi_input true processes several tracks' clips as one phase-coherent stream) to new files, undoable.\n"
         " Time (pick one): shift_percent (processed length as % of original, any decimals - 100.1 = 0.1% pull-down), ratio, length_seconds, or from_bpm + to_bpm.\n"
         " Pitch: semitones (-36..36, decimals are cents), formant_in (keep the voice's character), formant (-36..36 st, needs formant_in).\n"
         " type: polyphonic (default), monophonic (vocals, bass, solo lines - formant-exact), fast. sensitivity 0-100 (transients, polyphonic), window_ms 10-100 (monophonic), gain_db 0..-6.\n"
         " preset: a factory preset name instead (action presets lists them). action open shows the window.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["render","presets","open"]},"track":{"type":["string","integer"]},"tracks":{"type":"array"},"clip":{"type":["string","integer"]},
            "multi_input":{"type":"boolean"},"shift_percent":{"type":"number"},"ratio":{"type":"number"},"length_seconds":{"type":"number"},"from_bpm":{"type":"number"},"to_bpm":{"type":"number"},
            "semitones":{"type":"number"},"formant_in":{"type":"boolean"},"formant":{"type":"number"},"type":{"type":"string"},"sensitivity":{"type":"number"},"window_ms":{"type":"number"},"gain_db":{"type":"number"},"preset":{"type":"string"}}})json",
         [this] (const juce::var& a)
         {
             const auto action = str (a, "action", "render").toLowerCase();
             if (action == "presets")
             {
                 juce::StringArray n; for (auto& p : MoltDsp::presets()) n.add (p.name);
                 return ToolResult::ok ("Molt presets: " + n.joinIntoString (" | "));
             }
             if (action == "open") { if (ui.menuCommand) { auto e = ui.menuCommand ("AudioSuite > Molt (Time & Pitch)..."); if (e.isNotEmpty()) return ToolResult::error (e); } return ToolResult::ok ("Molt window open"); }
             juce::String err;
             juce::Array<te::Clip::Ptr> clips;
             juce::Array<te::AudioTrack*> tracks;
             if (a["tracks"].isArray())
                 for (auto& tv : *a["tracks"].getArray()) { auto o = obj(); set (o, "track", tv); if (auto t = trackArg (o, err)) tracks.add (t); else return ToolResult::error (err); }
             else if (auto t = trackArg (a, err)) tracks.add (t);
             else return ToolResult::error (err);
             for (auto t : tracks)
             {
                 if (has (a, "clip") && tracks.size() == 1) { auto c = clipArg (*t, a, err); if (c == nullptr) return ToolResult::error (err); clips.add (c); }
                 else for (auto c : t->getClips()) if (dynamic_cast<te::AudioClipBase*> (c) != nullptr) clips.add (c);
             }
             if (clips.isEmpty()) return ToolResult::error ("No audio clips to process");
             MoltDsp::Params p;
             if (has (a, "preset"))
             {
                 bool found = false;
                 for (auto& pr : MoltDsp::presets())
                     if (juce::String (pr.name).containsIgnoreCase (str (a, "preset"))) { p.timeRatio = pr.timePct / 100.0; p.semitones = pr.semitones; p.formantIn = pr.formantIn; p.formantSt = pr.formantSt; found = true; break; }
                 if (! found) return ToolResult::error ("Unknown preset (action presets lists them)");
             }
             double origLen = 0; for (auto& c : clips) origLen = juce::jmax (origLen, c->getPosition().getLength().inSeconds());
             if (has (a, "shift_percent")) p.timeRatio = num (a, "shift_percent") / 100.0;
             else if (has (a, "ratio")) p.timeRatio = num (a, "ratio");
             else if (has (a, "length_seconds")) p.timeRatio = num (a, "length_seconds") / juce::jmax (1e-6, origLen);
             else if (has (a, "from_bpm") && has (a, "to_bpm")) p.timeRatio = MoltDsp::ratioFromTempo (num (a, "from_bpm"), num (a, "to_bpm"));
             if (! (p.timeRatio >= 0.125 && p.timeRatio <= 8.0)) return ToolResult::error ("Time must stay within 12.5% - 800% (8x range)");
             if (has (a, "semitones")) p.semitones = juce::jlimit (-36.0, 36.0, num (a, "semitones"));
             if (has (a, "formant_in")) p.formantIn = flag (a, "formant_in");
             if (has (a, "formant")) { p.formantSt = juce::jlimit (-36.0, 36.0, num (a, "formant")); p.formantIn = true; }
             const auto ty = str (a, "type").toLowerCase();
             p.type = ty.startsWith ("mono") ? 1 : (ty.startsWith ("fast") || ty.contains ("faster")) ? 2 : 0;
             p.sensitivity = juce::jlimit (0.0, 100.0, num (a, "sensitivity", 50));
             p.windowMs = juce::jlimit (10.0, 100.0, num (a, "window_ms", 25));
             p.gainDb = juce::jlimit (-6.0, 0.0, num (a, "gain_db", 0));
             float pk = 0;
             auto msg = Molt::render (*session.edit, clips, p, flag (a, "multi_input"), pk, err);
             if (msg.isEmpty()) return ToolResult::error (err);
             if (ui.refresh) ui.refresh();
             return ToolResult::ok (msg + juce::String::formatted (": time %.4f%%, pitch %+.2f st", p.timeRatio * 100.0, p.semitones)
                                    + (p.formantIn ? juce::String::formatted (", formant kept %+.2f st", p.formantSt) : juce::String (", formants follow pitch"))
                                    + ", " + MoltDsp::typeNames()[p.type] + (pk > 0.999f ? ". WARNING: clipped - undo and lower gain_db." : ". Peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS."));
         });
}
