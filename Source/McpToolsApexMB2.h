// Squelchmoth 1.14 - Claude's `apex_multiband` tool: Apex Multiband 2 by name. Included inside the McpTools class body.

juce::var describeApexMB2 (SqApexMB2Plugin& m)
{
    using P = SqApexMB2Plugin;
    juce::var o (new juce::DynamicObject());
    const int n = juce::jlimit (1, P::maxBands, m.numBands.get());
    set (o, "plugin", "Apex Multiband 2"); set (o, "bands", n); set (o, "phase", m.phase.get() == 1 ? "linear" : "minimum");
    set (o, "detector", P::detectorNames()[juce::jlimit (0, 4, m.detector.get())]);
    set (o, "input_db", m.bank[P::input]); set (o, "output_db", m.bank[P::output]); set (o, "mix_pct", m.bank[P::mix]); set (o, "lookahead_ms", m.bank[P::lookahead]);
    set (o, "latency_ms", m.getLatencySeconds() * 1000.0);
    juce::Array<juce::var> xs; const auto x = SqMB2::sanitise (m.params(), 48000.0); for (int i = 0; i < n - 1; ++i) xs.add (x[(size_t) i]);
    set (o, "crossovers_hz", xs);
    juce::Array<juce::var> bands;
    for (int k = 0; k < n; ++k)
    {
        juce::var b (new juce::DynamicObject());
        set (b, "band", k + 1); set (b, "mode", SqMB2::modeName (m.mode[(size_t) k].get())); set (b, "place", SqMB2::placeName (m.place[(size_t) k].get()));
        set (b, "key", m.keyOf[(size_t) k].get() ? "side-chain" : "itself");
        set (b, "threshold_db", m.bp (k, P::thr)); set (b, "ratio", m.bp (k, P::ratio)); set (b, "range_db", m.bp (k, P::range)); set (b, "knee_db", m.bp (k, P::knee));
        set (b, "attack_ms", m.bp (k, P::attack)); set (b, "release_ms", m.bp (k, P::release)); set (b, "gain_db", m.bp (k, P::gain));
        set (b, "solo", m.solo[(size_t) k].get()); set (b, "bypass", m.bypass[(size_t) k].get());
        set (b, "gain_change_now_db", m.grDb[(size_t) k].load()); set (b, "level_now_db", m.levelDb[(size_t) k].load());
        bands.add (b);
    }
    set (o, "band_settings", bands);
    return o;
}

void registerApexMB2Tools()
{
    add ("apex_multiband",
         "Apex Multiband 2, the multiband dynamics flagship (Mastering Suite): up to 6 bands, minimum or linear phase, per band compress / compress up / expand / expand up, "
         "a stereo place (stereo, left, right, mid, side) and an optional side-chain key. action: add (optional preset), get (all settings + live gain change per band), "
         "set (global), band (one band: band = 1..6), preset (name or 'list'), open.\n"
         " set: bands (1-6), phase (minimum | linear), detector (smart | rms | average | peak | fast), input / output (dB), mix (%), lookahead (0-20 ms), crossovers (array of Hz, low to high).\n"
         " band: band (1-6) and any of mode (compress | compress up | expand | expand up), place (stereo | left | right | mid | side), key (itself | sidechain), "
         "threshold (dB), ratio (1-20), range (dB 0-36: the most it may change), knee (dB), attack (ms), release (ms), gain (dB), solo, bypass.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","band","preset","open"]},
            "bands":{"type":"integer"},"phase":{"type":"string"},"detector":{"type":"string"},"input":{"type":"number"},"output":{"type":"number"},"mix":{"type":"number"},
            "lookahead":{"type":"number"},"crossovers":{"type":"array","items":{"type":"number"}},"band":{"type":"integer"},"mode":{"type":"string"},"place":{"type":"string"},
            "key":{"type":"string"},"threshold":{"type":"number"},"ratio":{"type":"number"},"range":{"type":"number"},"knee":{"type":"number"},"attack":{"type":"number"},
            "release":{"type":"number"},"gain":{"type":"number"},"solo":{"type":"boolean"},"bypass":{"type":"boolean"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqApexMB2Plugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqApexMB2Plugin* m = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqApexMB2Plugin*> (pl)) { m = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Apex Multiband 2 preset"); Signature::applyPreset (*m, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (m == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Apex Multiband 2", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Apex Multiband 2"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     m = dynamic_cast<SqApexMB2Plugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeApexMB2 (*m)));
             }
             if (m == nullptr) return ToolResult::error ("No Apex Multiband 2 on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeApexMB2 (*m)));
             if (action == "open") { m->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             auto* um = &session.edit->getUndoManager();
             auto setP = [&] (int id, double x) { auto& p = *m->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
             if (action == "set")
             {
                 um->beginNewTransaction ("Apex Multiband 2 settings");
                 if (has (a, "bands")) m->numBands.setValue (juce::jlimit (1, P::maxBands, (int) num (a, "bands")), um);
                 if (has (a, "phase")) m->phase.setValue (str (a, "phase").containsIgnoreCase ("lin") ? 1 : 0, um);
                 if (has (a, "detector"))
                 {
                     const int i = P::detectorNames().indexOf (str (a, "detector"), true);
                     if (i < 0) return ToolResult::error ("Detectors: " + P::detectorNames().joinIntoString (", "));
                     m->detector.setValue (i, um);
                 }
                 for (auto [k, id] : { std::pair<const char*, int> { "input", P::input }, { "output", P::output }, { "mix", P::mix }, { "lookahead", P::lookahead } })
                     if (has (a, k)) setP (id, num (a, k));
                 if (auto* arr = a["crossovers"].getArray())
                     for (int i = 0; i < juce::jmin (P::maxBands - 1, arr->size()); ++i) setP (P::x1 + i, (double) (*arr)[i]);
                 return ToolResult::ok (juce::JSON::toString (describeApexMB2 (*m)));
             }
             if (action == "band")
             {
                 const int k = (int) num (a, "band") - 1;
                 if (k < 0 || k >= juce::jlimit (1, P::maxBands, m->numBands.get())) return ToolResult::error ("band must be 1.." + juce::String (m->numBands.get()));
                 um->beginNewTransaction ("Apex Multiband 2 band " + juce::String (k + 1));
                 if (has (a, "mode"))
                 {
                     const auto v = str (a, "mode").toLowerCase();
                     const int md = v.contains ("exp") ? (v.contains ("up") ? 3 : 2) : (v.contains ("up") ? 1 : 0);
                     m->mode[(size_t) k].setValue (md, um);
                 }
                 if (has (a, "place"))
                 {
                     const auto v = str (a, "place").toLowerCase(); int pl = -1;
                     for (int i = 0; i < 5; ++i) if (v.startsWith (juce::String (SqMB2::placeName (i)).toLowerCase())) pl = i;
                     if (pl < 0) return ToolResult::error ("place: stereo, left, right, mid, side");
                     m->place[(size_t) k].setValue (pl, um);
                 }
                 if (has (a, "key")) m->keyOf[(size_t) k].setValue (str (a, "key").containsIgnoreCase ("side") ? 1 : 0, um);
                 for (auto [nm, sub] : { std::pair<const char*, int> { "threshold", P::thr }, { "ratio", P::ratio }, { "range", P::range }, { "knee", P::knee },
                                         { "attack", P::attack }, { "release", P::release }, { "gain", P::gain } })
                     if (has (a, nm)) setP (m->paramOf (k, sub), num (a, nm));
                 if (has (a, "solo")) m->solo[(size_t) k].setValue ((bool) a["solo"], um);
                 if (has (a, "bypass")) m->bypass[(size_t) k].setValue ((bool) a["bypass"], um);
                 return ToolResult::ok (juce::JSON::toString (describeApexMB2 (*m)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
