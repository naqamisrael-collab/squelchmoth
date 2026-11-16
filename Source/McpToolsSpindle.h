// Squelchmoth 1.14 - Claude's `spindle_delay` tool: the delay flagship by name. Included inside the McpTools class body.

juce::var describeSpindle (SqSpindleDelayPlugin& d)
{
    using P = SqSpindleDelayPlugin;
    juce::var o (new juce::DynamicObject());
    set (o, "plugin", "Spindle Delay");
    set (o, "character", P::characterNames()[juce::jlimit (0, 3, d.character.get())]); set (o, "routing", P::routingNames()[juce::jlimit (0, 2, d.routing.get())]);
    set (o, "link_lr", d.linkLR.get());
    set (o, "time_left_ms", d.timeMs (0)); set (o, "time_right_ms", d.timeMs (1));
    set (o, "sync_left", P::syncNames()[juce::jlimit (0, 12, d.syncL.get())]); set (o, "sync_right", P::syncNames()[juce::jlimit (0, 12, d.syncR.get())]);
    for (auto [k, id] : { std::pair<const char*, int> { "feedback_pct", P::feedback }, { "cross_feed_pct", P::crossFeed }, { "low_cut_hz", P::lowCut }, { "high_cut_hz", P::highCut },
                           { "drive_pct", P::drive }, { "wow_pct", P::wow }, { "flutter_pct", P::flutter }, { "diffusion_pct", P::diffusion }, { "width_pct", P::width },
                           { "ducking_db", P::duck }, { "duck_release_ms", P::duckRelease }, { "mix_pct", P::mix }, { "output_db", P::output } })
        set (o, k, d.bank[id]);
    set (o, "freeze", d.freeze.get());
    return o;
}

void registerSpindleTools()
{
    add ("spindle_delay",
         "Spindle Delay, the delay flagship (Color & Space): four characters, normal / ping-pong / cross routing, filters, drive, wow, flutter and diffusion in the loop, ducking, freeze. "
         "action: add (optional preset), get, set, preset (name or 'list'), open.\n"
         " set (any of): character (digital | tape | analog | diffuse - sets wow, flutter and diffusion to the character's own unless you give them), routing (normal | pingpong | cross), "
         "sync (free | 1/32 | 1/16 | 1/8 | 1/4 | 1/2 | 1 bar | 1/16 dotted | 1/8 dotted | 1/4 dotted | 1/16 triplet | 1/8 triplet | 1/4 triplet; with link_lr false, sync_right for the right), "
         "time / time_right (ms: sets that side free), link_lr (bool), feedback (0-110 %), cross_feed (%), low_cut / high_cut (Hz, in the loop), drive, wow, flutter, diffusion, width (%), "
         "ducking (dB), duck_release (ms), mix (%), output (dB), freeze (bool).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","preset","open"]},
            "character":{"type":"string"},"routing":{"type":"string"},"sync":{"type":"string"},"sync_right":{"type":"string"},"time":{"type":"number"},"time_right":{"type":"number"},
            "link_lr":{"type":"boolean"},"feedback":{"type":"number"},"cross_feed":{"type":"number"},"low_cut":{"type":"number"},"high_cut":{"type":"number"},"drive":{"type":"number"},
            "wow":{"type":"number"},"flutter":{"type":"number"},"diffusion":{"type":"number"},"width":{"type":"number"},"ducking":{"type":"number"},"duck_release":{"type":"number"},
            "mix":{"type":"number"},"output":{"type":"number"},"freeze":{"type":"boolean"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqSpindleDelayPlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqSpindleDelayPlugin* d = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqSpindleDelayPlugin*> (pl)) { d = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Spindle preset"); Signature::applyPreset (*d, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (d == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Spindle Delay", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Spindle Delay"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     d = dynamic_cast<SqSpindleDelayPlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeSpindle (*d)));
             }
             if (d == nullptr) return ToolResult::error ("No Spindle Delay on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeSpindle (*d)));
             if (action == "open") { d->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             if (action == "set")
             {
                 auto* um = &session.edit->getUndoManager(); um->beginNewTransaction ("Spindle settings");
                 auto setP = [&] (int id, double x) { auto& p = *d->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
                 if (has (a, "character"))
                 {
                     const int c = P::characterNames().indexOf (str (a, "character"), true);
                     if (c < 0) return ToolResult::error ("character: digital, tape, analog, diffuse");
                     d->character.setValue (c, um);
                     const auto def = SqSpindle::defaultsFor (c);
                     setP (P::wow, def.wow * 100.0); setP (P::flutter, def.flutter * 100.0); setP (P::diffusion, def.diffusion * 100.0);
                 }
                 if (has (a, "routing"))
                 {
                     const auto v = str (a, "routing").toLowerCase().removeCharacters ("- ");
                     d->routing.setValue (v.startsWith ("ping") ? 1 : v.startsWith ("cross") ? 2 : 0, um);
                 }
                 if (has (a, "link_lr")) d->linkLR.setValue ((bool) a["link_lr"], um);
                 auto syncIndex = [&] (const juce::String& v) { const auto w = v.trim(); if (w.equalsIgnoreCase ("free") || w.equalsIgnoreCase ("off")) return 0; return P::syncNames().indexOf (w, true); };
                 if (has (a, "sync")) { const int i = syncIndex (str (a, "sync")); if (i < 0) return ToolResult::error ("sync: " + P::syncNames().joinIntoString (", ")); d->syncL.setValue (i, um); }
                 if (has (a, "sync_right")) { const int i = syncIndex (str (a, "sync_right")); if (i < 0) return ToolResult::error ("sync_right: " + P::syncNames().joinIntoString (", ")); d->syncR.setValue (i, um); }
                 if (has (a, "time")) { setP (P::timeL, num (a, "time")); if (! has (a, "sync")) d->syncL.setValue (0, um); }
                 if (has (a, "time_right")) { setP (P::timeR, num (a, "time_right")); if (! has (a, "sync_right")) d->syncR.setValue (0, um); }
                 for (auto [k, id] : { std::pair<const char*, int> { "feedback", P::feedback }, { "cross_feed", P::crossFeed }, { "low_cut", P::lowCut }, { "high_cut", P::highCut },
                                        { "drive", P::drive }, { "wow", P::wow }, { "flutter", P::flutter }, { "diffusion", P::diffusion }, { "width", P::width },
                                        { "ducking", P::duck }, { "duck_release", P::duckRelease }, { "mix", P::mix }, { "output", P::output } })
                     if (has (a, k)) setP (id, num (a, k));
                 if (has (a, "freeze")) d->freeze.setValue ((bool) a["freeze"], um);
                 return ToolResult::ok (juce::JSON::toString (describeSpindle (*d)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
