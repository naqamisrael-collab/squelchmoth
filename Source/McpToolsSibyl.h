// Squelchmoth 1.14 - Claude's `sibyl_deess` tool: the de-esser flagship by name. Included inside the McpTools class body.

juce::var describeSibyl (SqSibylDeEssPlugin& s)
{
    using P = SqSibylDeEssPlugin;
    juce::var o (new juce::DynamicObject());
    set (o, "plugin", "Sibyl De-Ess");
    set (o, "shape", P::shapeNames()[juce::jlimit (0, 1, s.shape.get())]); set (o, "detection", P::detectNames()[juce::jlimit (0, 1, s.detect.get())]);
    set (o, "stereo", P::stereoNames()[juce::jlimit (0, 2, s.stereoMode.get())]); set (o, "listen", P::listenNames()[juce::jlimit (0, 2, s.listen.get())]);
    set (o, "track", s.track.get()); set (o, "sidechain", s.sidechain.get());
    set (o, "threshold_db", s.bank[P::threshold]); set (o, "range_db", s.bank[P::range]); set (o, "knee_db", s.bank[P::knee]);
    set (o, "release_ms", s.bank[P::release]); set (o, "lookahead_ms", s.bank[P::lookahead]);
    set (o, "low_hz", s.bank[P::low]); set (o, "high_hz", s.bank[P::high]); set (o, "output_db", s.bank[P::output]);
    set (o, "latency_ms", s.getLatencySeconds() * 1000.0);
    juce::var m (new juce::DynamicObject());
    set (m, "reduction_now_db", s.grNow.load()); set (m, "detected_db", s.detNow.load()); if (s.track.get()) set (m, "tracking_hz", s.trackHz.load());
    set (o, "meters", m);
    return o;
}

void registerSibylTools()
{
    add ("sibyl_deess",
         "Sibyl De-Ess, the de-esser flagship (Vocal Suite). action: add (optional preset), get (settings + how much it is taking now), set, preset (name or 'list'), open.\n"
         " set (any of): threshold (dB), range (dB, the most it takes), knee (dB), release (ms), lookahead (0-15 ms), low / high (Hz: the band it listens to and cuts), "
         "shape (split: only the band | wide: the whole voice), detection (relative: the s against the whole voice, level-independent | absolute: dBFS), "
         "follow (bool: Track mode - the cut follows the loudest s inside the band), stereo (linked | mid | side), sidechain (bool: listen to the side-chain input), "
         "listen (output | detection | removed), output (dB).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","preset","open"]},
            "threshold":{"type":"number"},"range":{"type":"number"},"knee":{"type":"number"},"release":{"type":"number"},"lookahead":{"type":"number"},
            "low":{"type":"number"},"high":{"type":"number"},"output":{"type":"number"},"shape":{"type":"string"},"detection":{"type":"string"},
            "follow":{"type":"boolean"},"stereo":{"type":"string"},"sidechain":{"type":"boolean"},"listen":{"type":"string"},"preset":{"type":"string"}},
            "required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqSibylDeEssPlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqSibylDeEssPlugin* s = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqSibylDeEssPlugin*> (pl)) { s = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Sibyl preset"); Signature::applyPreset (*s, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (s == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Sibyl De-Ess", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Sibyl De-Ess"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     s = dynamic_cast<SqSibylDeEssPlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeSibyl (*s)));
             }
             if (s == nullptr) return ToolResult::error ("No Sibyl De-Ess on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeSibyl (*s)));
             if (action == "open") { s->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             if (action == "set")
             {
                 auto* um = &session.edit->getUndoManager(); um->beginNewTransaction ("Sibyl settings");
                 auto setP = [&] (int id, double x) { auto& p = *s->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
                 for (auto [k, id] : { std::pair<const char*, int> { "threshold", P::threshold }, { "range", P::range }, { "knee", P::knee }, { "release", P::release },
                                        { "lookahead", P::lookahead }, { "low", P::low }, { "high", P::high }, { "output", P::output } })
                     if (has (a, k)) setP (id, num (a, k));
                 auto pick = [&] (const char* key, const juce::StringArray& names, juce::CachedValue<int>& cv) -> bool
                 {
                     if (! has (a, key)) return true;
                     const auto v = str (a, key); for (int i = 0; i < names.size(); ++i) if (names[i].startsWithIgnoreCase (v.substring (0, 3))) { cv.setValue (i, um); return true; }
                     return false;
                 };
                 if (! pick ("shape", P::shapeNames(), s->shape)) return ToolResult::error ("shape: split | wide");
                 if (! pick ("detection", P::detectNames(), s->detect)) return ToolResult::error ("detection: relative | absolute");
                 if (! pick ("stereo", P::stereoNames(), s->stereoMode)) return ToolResult::error ("stereo: linked | mid | side");
                 if (! pick ("listen", P::listenNames(), s->listen)) return ToolResult::error ("listen: output | detection | removed");
                 if (has (a, "follow")) s->track.setValue ((bool) a["follow"], um);
                 if (has (a, "sidechain")) s->sidechain.setValue ((bool) a["sidechain"], um);
                 return ToolResult::ok (juce::JSON::toString (describeSibyl (*s)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
