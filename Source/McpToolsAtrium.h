// Squelchmoth 1.14 - Claude's `atrium_verb` tool: the flagship algorithmic reverb by name. Included inside the McpTools class body.

juce::var describeAtrium (SqAtriumVerbPlugin& v)
{
    using P = SqAtriumVerbPlugin;
    juce::var o (new juce::DynamicObject());
    const auto& sp = SqAtrium::spaces()[(size_t) juce::jlimit (0, (int) SqAtrium::spaces().size() - 1, v.space.get())];
    set (o, "plugin", "Atrium Verb"); set (o, "space", sp.name); set (o, "space_about", sp.about);
    set (o, "decay_s", v.bank[P::decay]); set (o, "size_pct", v.bank[P::size]);
    set (o, "predelay_ms", v.predelayMs()); set (o, "predelay_sync", P::syncNames()[juce::jlimit (0, 9, v.predelaySync.get())]);
    set (o, "diffusion_pct", v.bank[P::diffusion]); set (o, "width_pct", v.bank[P::width]); set (o, "character_pct", v.bank[P::character]);
    set (o, "low_decay_x", v.bank[P::lowMult]); set (o, "low_crossover_hz", v.bank[P::lowX]);
    set (o, "high_decay_x", v.bank[P::highMult]); set (o, "high_crossover_hz", v.bank[P::highX]);
    set (o, "early_pct", v.bank[P::early]); set (o, "late_pct", v.bank[P::late]);
    set (o, "ducking_db", v.bank[P::duck]); set (o, "duck_threshold_db", v.bank[P::duckThresh]); set (o, "duck_release_ms", v.bank[P::duckRelease]);
    set (o, "low_cut_hz", v.bank[P::lowCut] <= 10.5f ? juce::var ("off") : juce::var (v.bank[P::lowCut]));
    set (o, "high_cut_hz", v.bank[P::highCut] >= 19999.0f ? juce::var ("off") : juce::var (v.bank[P::highCut]));
    set (o, "mix_pct", v.bank[P::mix]); set (o, "output_db", v.bank[P::output]); set (o, "freeze", v.freeze.get());
    juce::var d (new juce::DynamicObject());
    for (auto [k, hz] : { std::pair<const char*, double> { "100_hz", 100.0 }, { "1_khz", 1000.0 }, { "8_khz", 8000.0 } }) set (d, k, v.decayAt (hz));
    set (o, "decay_by_frequency_s", d);
    set (o, "early_reflections", (int) v.taps().size());
    return o;
}

void registerAtriumTools()
{
    add ("atrium_verb",
         "Atrium Verb, the flagship algorithmic reverb (Color & Space): a 16-line network with per-band decay, 12 spaces, early reflections, "
         "freeze, ducking and post EQ. action: add (inserts it; optional preset), get (all settings + the real decay time at 100 Hz / 1 kHz / 8 kHz), set, preset (name or 'list'), open.\n"
         " set (any of): space (Concert Hall | Plate | Grand Hall | Chamber | Studio Room | Booth | Cathedral | Ambience | Vintage Digital | Bloom | Dark Space | Gate), "
         "decay (s, 0.1-30), size (0-100 %), predelay (ms, 0-500), predelay_sync (free | 1/64 | 1/32 | 1/16 | 1/8 | 1/4 | 1/16 dotted | 1/8 dotted | 1/16 triplet | 1/8 triplet), "
         "diffusion, width (0-200 %), character (modulation 0-100 %), low_decay / high_decay (multipliers of Decay, 0.25-4 / 0.1-2), low_crossover / high_crossover (Hz), "
         "early / late (0-200 %), ducking (dB 0-30), duck_threshold (dB), duck_release (ms), low_cut / high_cut (Hz; 10 / 20000 = off), mix (%), output (dB), freeze (bool).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","preset","open"]},
            "space":{"type":"string"},"decay":{"type":"number"},"size":{"type":"number"},"predelay":{"type":"number"},"predelay_sync":{"type":"string"},
            "diffusion":{"type":"number"},"width":{"type":"number"},"character":{"type":"number"},"low_decay":{"type":"number"},"high_decay":{"type":"number"},
            "low_crossover":{"type":"number"},"high_crossover":{"type":"number"},"early":{"type":"number"},"late":{"type":"number"},"ducking":{"type":"number"},
            "duck_threshold":{"type":"number"},"duck_release":{"type":"number"},"low_cut":{"type":"number"},"high_cut":{"type":"number"},"mix":{"type":"number"},
            "output":{"type":"number"},"freeze":{"type":"boolean"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqAtriumVerbPlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqAtriumVerbPlugin* v = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqAtriumVerbPlugin*> (pl)) { v = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Atrium preset"); Signature::applyPreset (*v, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (v == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Atrium Verb", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Atrium Verb"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     v = dynamic_cast<SqAtriumVerbPlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeAtrium (*v)));
             }
             if (v == nullptr) return ToolResult::error ("No Atrium Verb on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeAtrium (*v)));
             if (action == "open") { v->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             if (action == "set")
             {
                 auto* um = &session.edit->getUndoManager();
                 um->beginNewTransaction ("Atrium settings");
                 auto setP = [&] (int id, double x) { auto& p = *v->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
                 if (has (a, "space"))
                 {
                     const auto want = str (a, "space"); int found = P::spaceNames().indexOf (want, true);
                     if (found < 0) for (int i = 0; i < P::spaceNames().size(); ++i) if (P::spaceNames()[i].containsIgnoreCase (want)) { found = i; break; }
                     if (found < 0) return ToolResult::error ("Spaces: " + P::spaceNames().joinIntoString (", "));
                     v->space.setValue (found, um);
                 }
                 if (has (a, "predelay_sync"))
                 {
                     const auto want = str (a, "predelay_sync").toLowerCase().trim(); int found = -1;
                     if (want == "free" || want == "off" || want == "none") found = 0;
                     for (int i = 1; found < 0 && i < P::syncNames().size(); ++i) if (P::syncNames()[i].equalsIgnoreCase (want)) found = i;
                     if (found < 0) return ToolResult::error ("predelay_sync: " + P::syncNames().joinIntoString (", "));
                     v->predelaySync.setValue (found, um);
                 }
                 for (auto [k, id] : { std::pair<const char*, int> { "decay", P::decay }, { "size", P::size }, { "predelay", P::predelay }, { "diffusion", P::diffusion },
                                        { "width", P::width }, { "character", P::character }, { "low_decay", P::lowMult }, { "high_decay", P::highMult },
                                        { "low_crossover", P::lowX }, { "high_crossover", P::highX }, { "early", P::early }, { "late", P::late }, { "ducking", P::duck },
                                        { "duck_threshold", P::duckThresh }, { "duck_release", P::duckRelease }, { "low_cut", P::lowCut }, { "high_cut", P::highCut },
                                        { "mix", P::mix }, { "output", P::output } })
                     if (has (a, k)) setP (id, num (a, k));
                 if (has (a, "predelay") && ! has (a, "predelay_sync")) v->predelaySync.setValue (0, um);   // a time in ms means free
                 if (has (a, "freeze")) v->freeze.setValue ((bool) a["freeze"], um);
                 return ToolResult::ok (juce::JSON::toString (describeAtrium (*v)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
