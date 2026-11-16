// Squelchmoth 1.14 - Claude's `summit_limiter` tool: the flagship true-peak limiter by name. Included inside the McpTools class body.

juce::var describeSummit (SqSummitLimiterPlugin& s)
{
    using P = SqSummitLimiterPlugin;
    juce::var o (new juce::DynamicObject());
    const auto& al = SqSummit::algorithms()[(size_t) juce::jlimit (0, 9, s.algorithm.get())];
    const auto& t = P::targets()[(size_t) juce::jlimit (0, (int) P::targets().size() - 1, s.target.get())];
    set (o, "plugin", "Summit Limiter"); set (o, "algorithm", al.name); set (o, "algorithm_about", al.about);
    set (o, "input_db", s.bank[P::input]); set (o, "ceiling_dbtp", s.bank[P::ceiling]); set (o, "release_pct", s.bank[P::release]); set (o, "lookahead_ms", s.bank[P::lookahead]);
    set (o, "linking", s.linkOwn.get() ? "set by the algorithm" : "set by hand");
    if (! s.linkOwn.get()) { set (o, "transient_link_pct", s.bank[P::transientLink]); set (o, "release_link_pct", s.bank[P::releaseLink]); }
    set (o, "true_peak", s.truePeak.get()); set (o, "band_limit_guard", s.bandLimit.get()); set (o, "dc_filter", s.dcFilter.get());
    set (o, "oversampling", P::oversampleNames()[juce::jlimit (0, 5, s.oversample.get())]);
    set (o, "dither", P::ditherNames()[juce::jlimit (0, 3, s.ditherBits.get())]); set (o, "noise_shaping", P::shapeNames()[juce::jlimit (0, 2, s.noiseShape.get())]);
    set (o, "unity_audition", s.unity.get()); set (o, "listen_removed", s.listenRemoved.get());
    set (o, "latency_ms", s.getLatencySeconds() * 1000.0);
    set (o, "target", t.name);
    juce::var m (new juce::DynamicObject());
    set (m, "gain_reduction_db", s.grNow.load()); set (m, "true_peak_now_dbtp", s.tpNow.load()); set (m, "true_peak_max_dbtp", s.tpMax.load());
    set (m, "momentary_lufs", s.momentary.load()); set (m, "short_term_lufs", s.shortTerm.load()); set (m, "integrated_lufs", s.integrated.load()); set (m, "loudness_range_lu", s.lra.load());
    if (s.integrated.load() > -70) set (m, "lu_from_target", s.integrated.load() - t.lufs);
    set (o, "meters", m);
    return o;
}

void registerSummitTools()
{
    add ("summit_limiter",
         "Summit Limiter, the flagship true-peak limiter (Mastering Suite). action: add (inserts it; optional preset), get (settings + live meters: gain reduction, true peak now / max, "
         "momentary / short-term / integrated LUFS, loudness range, LU from the target), set, match (set Input so the integrated loudness hits the target - play the song through first), "
         "reset_meters, preset (name or 'list'), open.\n"
         " set (any of): algorithm (Transparent | Punch | Dense | Smooth | Aggressive | Wide | Bus | Safe | Modern | Clipper), input (dB, -12..30), ceiling (dBTP, -12..0), release (25-400 % of the algorithm's), "
         "lookahead (0.1-10 ms), linking (algorithm | manual), transient_link / release_link (0-100 %, manual linking), true_peak (bool), guard (bool: the band-limit guard), dc (bool), "
         "oversampling (0 1 2 4 8 16 32), dither (0 | 16 | 20 | 24), noise_shaping (none | gentle | strong), unity (bool), listen_removed (bool), "
         "target (streaming | streaming quiet | podcast | broadcast eu | broadcast us | club | loud master).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","match","reset_meters","preset","open"]},
            "algorithm":{"type":"string"},"input":{"type":"number"},"ceiling":{"type":"number"},"release":{"type":"number"},"lookahead":{"type":"number"},"linking":{"type":"string"},
            "transient_link":{"type":"number"},"release_link":{"type":"number"},"true_peak":{"type":"boolean"},"guard":{"type":"boolean"},"dc":{"type":"boolean"},"oversampling":{"type":"integer"},
            "dither":{"type":"integer"},"noise_shaping":{"type":"string"},"unity":{"type":"boolean"},"listen_removed":{"type":"boolean"},"target":{"type":"string"},"preset":{"type":"string"}},
            "required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqSummitLimiterPlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqSummitLimiterPlugin* s = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqSummitLimiterPlugin*> (pl)) { s = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Summit preset"); Signature::applyPreset (*s, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (s == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Summit Limiter", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Summit Limiter"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     s = dynamic_cast<SqSummitLimiterPlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeSummit (*s)));
             }
             if (s == nullptr) return ToolResult::error ("No Summit Limiter on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeSummit (*s)));
             if (action == "open") { s->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             if (action == "reset_meters") { s->resetMeters(); return ToolResult::ok ("Meters reset"); }
             auto* um = &session.edit->getUndoManager();
             auto setP = [&] (int id, double v) { auto& p = *s->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, v), juce::sendNotification); };
             if (action == "match")
             {
                 const auto& t = P::targets()[(size_t) juce::jlimit (0, (int) P::targets().size() - 1, s->target.get())];
                 const float I = s->integrated.load();
                 if (I <= -70) return ToolResult::error ("No integrated loudness yet - play the song through, then match");
                 um->beginNewTransaction ("Summit match");
                 setP (P::input, s->bank[P::input] + (t.lufs - I)); setP (P::ceiling, t.tp);
                 s->resetMeters();
                 return ToolResult::ok ("Input moved by " + juce::String (t.lufs - I, 1) + " dB and the ceiling set to " + juce::String (t.tp, 1) + " dBTP for " + t.name + ". Play it again to confirm.\n" + juce::JSON::toString (describeSummit (*s)));
             }
             if (action == "set")
             {
                 um->beginNewTransaction ("Summit settings");
                 if (has (a, "algorithm"))
                 {
                     const int i = P::algorithmNames().indexOf (str (a, "algorithm"), true);
                     if (i < 0) return ToolResult::error ("Algorithms: " + P::algorithmNames().joinIntoString (", "));
                     s->algorithm.setValue (i, um);
                 }
                 for (auto [k, id] : { std::pair<const char*, int> { "input", P::input }, { "ceiling", P::ceiling }, { "release", P::release }, { "lookahead", P::lookahead },
                                        { "transient_link", P::transientLink }, { "release_link", P::releaseLink } })
                     if (has (a, k)) setP (id, num (a, k));
                 if (has (a, "transient_link") || has (a, "release_link")) s->linkOwn.setValue (false, um);
                 if (has (a, "linking")) s->linkOwn.setValue (! str (a, "linking").containsIgnoreCase ("man") && ! str (a, "linking").containsIgnoreCase ("hand"), um);
                 if (has (a, "true_peak")) s->truePeak.setValue ((bool) a["true_peak"], um);
                 if (has (a, "guard")) s->bandLimit.setValue ((bool) a["guard"], um);
                 if (has (a, "dc")) s->dcFilter.setValue ((bool) a["dc"], um);
                 if (has (a, "oversampling")) { const int f = (int) num (a, "oversampling"); int lg = 0; while ((1 << lg) < f && lg < 5) ++lg; s->oversample.setValue (f <= 1 ? 0 : lg, um); }
                 if (has (a, "dither")) { const int b = (int) num (a, "dither"); s->ditherBits.setValue (b >= 24 ? 3 : b >= 20 ? 2 : b >= 16 ? 1 : 0, um); }
                 if (has (a, "noise_shaping")) { const auto v = str (a, "noise_shaping").toLowerCase(); s->noiseShape.setValue (v.contains ("strong") ? 2 : v.contains ("gentle") ? 1 : 0, um); }
                 if (has (a, "unity")) s->unity.setValue ((bool) a["unity"], um);
                 if (has (a, "listen_removed")) s->listenRemoved.setValue ((bool) a["listen_removed"], um);
                 if (has (a, "target"))
                 {
                     const auto want = str (a, "target").toLowerCase(); int found = -1;
                     for (int i = 0; i < (int) P::targets().size(); ++i) if (juce::String (P::targets()[(size_t) i].name).toLowerCase().startsWith (want)) { found = i; break; }
                     if (found < 0) for (int i = 0; i < (int) P::targets().size(); ++i) if (juce::String (P::targets()[(size_t) i].name).toLowerCase().contains (want)) { found = i; break; }
                     if (found < 0) return ToolResult::error ("Unknown target");
                     s->target.setValue (found, um);
                 }
                 return ToolResult::ok (juce::JSON::toString (describeSummit (*s)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
