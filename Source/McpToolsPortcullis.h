// Squelchmoth 1.14 - Claude's `portcullis_gate` tool: the gate / expander / ducker flagship. Included inside the McpTools class body.

juce::var describePortcullis (SqPortcullisGatePlugin& g)
{
    using P = SqPortcullisGatePlugin;
    juce::var o (new juce::DynamicObject());
    set (o, "plugin", "Portcullis Gate");
    set (o, "mode", P::modeNames()[juce::jlimit (0, 2, g.mode.get())]); set (o, "key", P::keyNames()[juce::jlimit (0, 2, g.keySource.get())]);
    set (o, "curve", P::curveNames()[juce::jlimit (0, 2, g.curve.get())]);
    set (o, "open_threshold_db", g.bank[P::threshold]); set (o, "close_threshold_db", g.bank[P::threshold] - g.bank[P::hysteresis]);
    set (o, "hysteresis_db", g.bank[P::hysteresis]); set (o, "range_db", g.bank[P::range]); set (o, "ratio", g.bank[P::ratio]);
    set (o, "attack_ms", g.bank[P::attack]); set (o, "hold_ms", g.bank[P::hold]); set (o, "release_ms", g.bank[P::release]); set (o, "lookahead_ms", g.bank[P::lookahead]);
    set (o, "key_highpass_hz", g.bank[P::keyHp] <= 20.5f ? juce::var ("off") : juce::var (g.bank[P::keyHp]));
    set (o, "key_lowpass_hz", g.bank[P::keyLp] >= 19999.0f ? juce::var ("off") : juce::var (g.bank[P::keyLp]));
    set (o, "velocity", g.velocity.get()); set (o, "listen_key", g.listenKey.get()); set (o, "flip", g.flip.get()); set (o, "output_db", g.bank[P::output]);
    set (o, "latency_ms", g.getLatencySeconds() * 1000.0);
    juce::var m (new juce::DynamicObject());
    set (m, "key_level_db", g.keyNow.load()); set (m, "gain_db", g.gainNow.load()); set (m, "openness", g.openNow.load());
    set (o, "meters", m);
    return o;
}

void registerPortcullisTools()
{
    add ("portcullis_gate",
         "Portcullis Gate, the gate / expander / ducker flagship. action: add (optional preset), get (settings + live key level, gain and openness), set, preset (name or 'list'), open.\n"
         " set (any of): mode (gate | expand | duck), key (input | sidechain | midi: note-ons open it), curve (linear | smooth | punchy), threshold (dB: where it opens), "
         "hysteresis (dB: it closes this far below the threshold), range (dB: how far it closes), ratio (expand mode, 1-10), attack / hold / release (ms), "
         "lookahead (0-10 ms), key_highpass / key_lowpass (Hz; 20 / 20000 = off), velocity (bool, MIDI key), listen_key (bool), flip (bool: hear what it removes), output (dB).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","preset","open"]},
            "mode":{"type":"string"},"key":{"type":"string"},"curve":{"type":"string"},"threshold":{"type":"number"},"hysteresis":{"type":"number"},"range":{"type":"number"},
            "ratio":{"type":"number"},"attack":{"type":"number"},"hold":{"type":"number"},"release":{"type":"number"},"lookahead":{"type":"number"},
            "key_highpass":{"type":"number"},"key_lowpass":{"type":"number"},"velocity":{"type":"boolean"},"listen_key":{"type":"boolean"},"flip":{"type":"boolean"},
            "output":{"type":"number"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqPortcullisGatePlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqPortcullisGatePlugin* g = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqPortcullisGatePlugin*> (pl)) { g = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Portcullis preset"); Signature::applyPreset (*g, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (g == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Portcullis Gate", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Portcullis Gate"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     g = dynamic_cast<SqPortcullisGatePlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describePortcullis (*g)));
             }
             if (g == nullptr) return ToolResult::error ("No Portcullis Gate on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describePortcullis (*g)));
             if (action == "open") { g->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             if (action == "set")
             {
                 auto* um = &session.edit->getUndoManager(); um->beginNewTransaction ("Portcullis settings");
                 auto setP = [&] (int id, double x) { auto& p = *g->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
                 for (auto [k, id] : { std::pair<const char*, int> { "threshold", P::threshold }, { "hysteresis", P::hysteresis }, { "range", P::range }, { "ratio", P::ratio },
                                        { "attack", P::attack }, { "hold", P::hold }, { "release", P::release }, { "lookahead", P::lookahead },
                                        { "key_highpass", P::keyHp }, { "key_lowpass", P::keyLp }, { "output", P::output } })
                     if (has (a, k)) setP (id, num (a, k));
                 auto pick = [&] (const char* key, const juce::StringArray& names, juce::CachedValue<int>& cv) -> bool
                 {
                     if (! has (a, key)) return true;
                     const auto v = str (a, key).removeCharacters ("- ");
                     for (int i = 0; i < names.size(); ++i) if (names[i].removeCharacters ("- ").equalsIgnoreCase (v) || names[i].startsWithIgnoreCase (v.substring (0, 3))) { cv.setValue (i, um); return true; }
                     return false;
                 };
                 if (! pick ("mode", P::modeNames(), g->mode)) return ToolResult::error ("mode: gate | expand | duck");
                 if (! pick ("key", P::keyNames(), g->keySource)) return ToolResult::error ("key: input | sidechain | midi");
                 if (! pick ("curve", P::curveNames(), g->curve)) return ToolResult::error ("curve: linear | smooth | punchy");
                 if (has (a, "velocity")) g->velocity.setValue ((bool) a["velocity"], um);
                 if (has (a, "listen_key")) g->listenKey.setValue ((bool) a["listen_key"], um);
                 if (has (a, "flip")) g->flip.setValue ((bool) a["flip"], um);
                 return ToolResult::ok (juce::JSON::toString (describePortcullis (*g)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
