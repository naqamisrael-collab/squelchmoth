// Squelchmoth 1.14 - Claude's `torque_comp` tool: the flagship compressor by name. Included inside the McpTools class body.

juce::var describeTorque (SqTorqueCompPlugin& t)
{
    using P = SqTorqueCompPlugin;
    juce::var o (new juce::DynamicObject());
    const auto& st = SqTorque::styles()[(size_t) juce::jlimit (0, 13, t.style.get())];
    set (o, "plugin", "Torque Comp"); set (o, "style", st.name); set (o, "style_about", st.about);
    for (auto [id, name] : { std::pair<int, const char*> { P::threshold, "threshold_db" }, { P::ratio, "ratio" }, { P::knee, "knee_db" }, { P::range, "range_db" },
                              { P::attack, "attack_ms" }, { P::release, "release_ms" }, { P::hold, "hold_ms" }, { P::lookahead, "lookahead_ms" }, { P::link, "stereo_link_pct" },
                              { P::drive, "drive_db" }, { P::mix, "mix_pct" }, { P::makeup, "makeup_db" }, { P::output, "output_db" } })
        set (o, name, juce::roundToInt (t.bank[(size_t) id] * 1000) / 1000.0);
    set (o, "auto_release", t.autoRelease.get()); set (o, "auto_threshold", t.autoThreshold.get()); set (o, "auto_gain", t.autoGain.get());
    set (o, "key", SqTorque::keyNames()[juce::jlimit (0, 3, t.keySource.get())]);
    if (t.keySource.get() == 3) set (o, "tempo_pulse", SqTorqueCompPlugin::tempoNames()[juce::jlimit (0, 4, t.tempoDiv.get())]);
    set (o, "routing", SqTorque::routingNames()[juce::jlimit (0, 4, t.routing.get())]);
    set (o, "colour", SqTorqueCompPlugin::characterNames()[juce::jlimit (0, 4, t.characterMode.get())]);
    set (o, "colour_position", t.characterPost.get() ? "after the gain stage" : "before the gain stage");
    set (o, "oversampling", SqTorqueCompPlugin::oversampleNames()[juce::jlimit (0, 5, t.oversample.get())]);
    set (o, "listen_to_key", t.audition.get());
    set (o, "latency_ms", t.getLatencySeconds() * 1000.0);
    juce::Array<juce::var> key;
    for (int b = 0; b < 6; ++b)
    {
        if (! t.keyProps[(size_t) b].on.get()) continue;
        juce::var k (new juce::DynamicObject());
        set (k, "band", b + 1); set (k, "shape", SqSpectra::shapeNames()[juce::jlimit (0, 9, t.keyProps[(size_t) b].shape.get())]);
        set (k, "freq_hz", t.bank[(size_t) (P::k1Freq + b * 3)]); set (k, "gain_db", t.bank[(size_t) (P::k1Gain + b * 3)]); set (k, "q", t.bank[(size_t) (P::k1Q + b * 3)]);
        key.add (k);
    }
    set (o, "key_eq", key);
    juce::var m (new juce::DynamicObject());
    set (m, "gain_reduction_db", t.grNow.load()); set (m, "input_db", t.inNow.load()); set (m, "output_db", t.outNow.load()); set (m, "momentary_lufs", t.lufsNow.load());
    if (t.autoGain.get()) set (m, "auto_gain_db", t.autoGainNow.load());
    if (t.autoThreshold.get()) set (m, "auto_threshold_db", t.autoThreshNow.load());
    set (o, "meters", m);
    juce::var c (new juce::DynamicObject());
    for (int in : { -48, -36, -24, -18, -12, -6, 0 }) set (c, juce::String (in), juce::roundToInt (t.curveOutDb (in) * 100) / 100.0);
    set (o, "curve_out_db_for_in_db", c);
    return o;
}

void registerTorqueTools()
{
    add ("torque_comp",
         "Torque Comp, the flagship compressor (Mastering Suite). action: add (inserts it; optional preset), get (every setting, the live meters - gain reduction, input, output, momentary LUFS - "
         "and the static curve), set, key_band (shape the detector's EQ), preset (name or 'list'), open.\n"
         " set (any of): style (Clean | Punch | Glue | Opto | Variable Mu | FET | Bus | Vocal | Mastering | Leveler | Upward | Pump | Smash | Transparent Peak), threshold (dB), ratio (1-100), knee (0-72 dB), "
         "range (0-60 dB), attack (0.005-250 ms), release (5-5000 ms), hold (0-500 ms), lookahead (0-20 ms), link (0-100 %), drive (0-24 dB), mix (0-200 %: above 100 % deepens the gain reduction), "
         "makeup (dB), output (dB), auto_release / auto_threshold / auto_gain (bool), key (internal | external | midi | tempo), tempo_pulse (1/16 | 1/8 | 1/4 | 1/2 | 1 bar), "
         "routing (stereo | mid only | side only | mid to side | side to mid), colour (off | style | warm | bright | iron), colour_before (bool: before the gain stage), oversampling (1 2 4 8 16 32), listen (bool: hear the key).\n"
         " key_band: band (1-6), shape (low cut | high cut | bell | low shelf | high shelf | band pass | notch), freq, gain, q, slope, on (bool).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","key_band","preset","open"]},
            "style":{"type":"string"},"threshold":{"type":"number"},"ratio":{"type":"number"},"knee":{"type":"number"},"range":{"type":"number"},"attack":{"type":"number"},"release":{"type":"number"},
            "hold":{"type":"number"},"lookahead":{"type":"number"},"link":{"type":"number"},"drive":{"type":"number"},"mix":{"type":"number"},"makeup":{"type":"number"},"output":{"type":"number"},
            "auto_release":{"type":"boolean"},"auto_threshold":{"type":"boolean"},"auto_gain":{"type":"boolean"},"key":{"type":"string"},"tempo_pulse":{"type":"string"},"routing":{"type":"string"},
            "colour":{"type":"string"},"colour_before":{"type":"boolean"},"oversampling":{"type":"integer"},"listen":{"type":"boolean"},
            "band":{"type":"integer"},"shape":{"type":"string"},"freq":{"type":"number"},"gain":{"type":"number"},"q":{"type":"number"},"slope":{"type":"integer"},"on":{"type":"boolean"},"preset":{"type":"string"}},
            "required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqTorqueCompPlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqTorqueCompPlugin* t = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqTorqueCompPlugin*> (pl)) { t = c; break; }
             auto presets = [] () -> const Signature::Entry* { for (auto& e : Signature::entries()) if (juce::String (e.type) == SqTorqueCompPlugin::xmlTypeName) return &e; return nullptr; };
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 auto e = presets(); if (e == nullptr) return "No presets";
                 juce::StringArray names; for (auto& pr : e->presets) names.add (pr.name);
                 for (auto& pr : e->presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Torque Comp preset"); Signature::applyPreset (*t, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                 return "Presets: " + names.joinIntoString (", ");
             };
             if (action == "add")
             {
                 if (t == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Torque Comp", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Torque Comp"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     t = dynamic_cast<SqTorqueCompPlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeTorque (*t)));
             }
             if (t == nullptr) return ToolResult::error ("No Torque Comp on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeTorque (*t)));
             if (action == "open") { t->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             auto* um = &session.edit->getUndoManager();
             auto setP = [&] (int id, double v) { auto& p = *t->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, v), juce::sendNotification); };
             if (action == "set")
             {
                 um->beginNewTransaction ("Torque Comp settings");
                 if (has (a, "style"))
                 {
                     int found = -1; const auto want = str (a, "style").toLowerCase().removeCharacters (" -_");
                     for (int i = 0; i < 14; ++i) if (juce::String (SqTorque::styles()[(size_t) i].name).toLowerCase().removeCharacters (" -_") == want) found = i;
                     if (found < 0) for (int i = 0; i < 14; ++i) if (juce::String (SqTorque::styles()[(size_t) i].name).toLowerCase().removeCharacters (" -_").contains (want)) { found = i; break; }
                     if (found < 0) return ToolResult::error ("Unknown style. Styles: " + SqTorqueCompPlugin::styleNames().joinIntoString (", "));
                     t->style.setValue (found, um);
                 }
                 for (auto [k, id] : { std::pair<const char*, int> { "threshold", P::threshold }, { "ratio", P::ratio }, { "knee", P::knee }, { "range", P::range }, { "attack", P::attack },
                                        { "release", P::release }, { "hold", P::hold }, { "lookahead", P::lookahead }, { "link", P::link }, { "drive", P::drive }, { "mix", P::mix },
                                        { "makeup", P::makeup }, { "output", P::output } })
                     if (has (a, k)) setP (id, num (a, k));
                 if (has (a, "auto_release")) t->autoRelease.setValue ((bool) a["auto_release"], um);
                 if (has (a, "auto_threshold")) t->autoThreshold.setValue ((bool) a["auto_threshold"], um);
                 if (has (a, "auto_gain")) t->autoGain.setValue ((bool) a["auto_gain"], um);
                 if (has (a, "key")) { const auto k = str (a, "key").toLowerCase(); t->keySource.setValue (k.contains ("ext") ? 1 : k.contains ("midi") ? 2 : k.contains ("tempo") ? 3 : 0, um); }
                 if (has (a, "tempo_pulse")) { const auto v = str (a, "tempo_pulse"); const int i = SqTorqueCompPlugin::tempoNames().indexOf (v); if (i >= 0) t->tempoDiv.setValue (i, um); }
                 if (has (a, "routing")) { const auto r = str (a, "routing").toLowerCase(); t->routing.setValue (r.contains ("mid to") ? 3 : r.contains ("side to") ? 4 : r.startsWith ("mid") ? 1 : r.startsWith ("side") ? 2 : 0, um); }
                 if (has (a, "colour")) { const int i = SqTorqueCompPlugin::characterNames().indexOf (str (a, "colour"), true); if (i >= 0) t->characterMode.setValue (i, um); else return ToolResult::error ("colour: off, style, warm, bright or iron"); }
                 if (has (a, "colour_before")) t->characterPost.setValue (! (bool) a["colour_before"], um);
                 if (has (a, "oversampling")) { const int f = (int) num (a, "oversampling"); int lg = 0; while ((1 << lg) < f && lg < 5) ++lg; t->oversample.setValue (lg, um); }
                 if (has (a, "listen")) t->audition.setValue ((bool) a["listen"], um);
                 return ToolResult::ok (juce::JSON::toString (describeTorque (*t)));
             }
             if (action == "key_band")
             {
                 const int b = (int) num (a, "band", 1) - 1;
                 if (b < 0 || b >= 6) return ToolResult::error ("band must be 1-6");
                 um->beginNewTransaction ("Torque Comp key EQ");
                 auto& kp = t->keyProps[(size_t) b];
                 if (has (a, "shape")) { const int sh = spectraShapeIndex (str (a, "shape")); if (sh < 0) return ToolResult::error ("Unknown shape"); kp.shape.setValue (sh, um); }
                 if (has (a, "freq")) setP (P::k1Freq + b * 3, num (a, "freq"));
                 if (has (a, "gain")) setP (P::k1Gain + b * 3, num (a, "gain"));
                 if (has (a, "q")) setP (P::k1Q + b * 3, num (a, "q"));
                 if (has (a, "slope")) kp.slope.setValue ((int) num (a, "slope"), um);
                 kp.on.setValue (has (a, "on") ? (bool) a["on"] : true, um);
                 return ToolResult::ok (juce::JSON::toString (describeTorque (*t)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
