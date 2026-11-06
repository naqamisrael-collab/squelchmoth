// Squelchmoth - Claude tools for the Apex Series (and loudness). Included inside the McpTools class body.

static juce::String apexTypeFor (const juce::String& k)
{
    const auto s = k.toLowerCase();
    if (s.contains ("lim")) return SqApexLimiterPlugin::xmlTypeName;
    if (s.contains ("multi") || s.contains ("band")) return SqApexMultibandPlugin::xmlTypeName;
    if (s.contains ("sub")) return SqApexSubPlugin::xmlTypeName;
    return SqApexDynamicsPlugin::xmlTypeName;
}

static juce::String colourTypeFor (const juce::String& k)
{
    const auto s = k.toLowerCase().removeCharacters (" -_");
    if (s.contains ("aurora") || s.contains ("verb")) return SqAuroraPlugin::xmlTypeName;
    if (s.contains ("opto") || s.contains ("level")) return SqOptoPlugin::xmlTypeName;
    if (s.contains ("pulse") || s.contains ("eq")) return SqPulseEqPlugin::xmlTypeName;
    if (s.contains ("oil") || s.contains ("can")) return SqOilCanPlugin::xmlTypeName;
    if (s.contains ("ladder") || s.contains ("silk") || s.contains ("lowpass")) return SqLadderPlugin::xmlTypeName;
    if (s.contains ("ring") || s.contains ("halo")) return SqRingPlugin::xmlTypeName;
    if (s.contains ("phase") || s.contains ("twelve")) return SqPhase12Plugin::xmlTypeName;
    if (s.contains ("bucket") || s.contains ("bbd") || s.contains ("analog")) return SqBucketPlugin::xmlTypeName;
    return {};
}

/** Every mode / switch setting the Apex and colour plug-ins keep in their state (so 'set' works before a value was ever stored). */
static const juce::StringArray& knownPluginSettings()
{
    static const juce::StringArray s = []
    {
        juce::StringArray k { "mode", "detection", "scFilter", "autoMakeup", "attenListen", "keyListen", "externalKey", "link", "autoRelease", "target",
                              "bands", "linearPhase", "range", "midiTune", "driveType",
                              "roomType", "freeze", "limitMode", "meterMode", "lowFreq", "highFreq", "attenFreq", "midLowFreq", "dipFreq", "midHighFreq",
                              "sync", "poles", "envFast", "lfoShape", "stages" };
        for (int b = 1; b <= 6; ++b) for (auto pre : { "solo", "bypass", "flip", "auto" }) k.add (juce::String (pre) + juce::String (b));
        return k;
    }();
    return s;
}

juce::var describeApex (te::Plugin& p)
{
    auto o = obj();
    set (o, "plugin", p.getName());
    auto params = obj();
    if (auto bo = dynamic_cast<SqDsp::BankOwner*> (&p))
        for (size_t i = 0; i < bo->getBank().params.size(); ++i)
        {
            const auto& sp = bo->getBank().specs[i];
            set (params, sp.name, SqDsp::numText (bo->getBank()[i], sp.decimals) + sp.unit);
        }
    auto props = obj();
    for (int i = 0; i < p.state.getNumProperties(); ++i)
    {
        auto id = p.state.getPropertyName (i).toString();
        if (id == "type" || id == "id" || id == "enabled" || id.startsWith ("windowX") || id.startsWith ("windowY") || id == "windowLocked" || id == "remapOnTempoChange" || id == "presetName" || id == "presetType" || id == "presetFile") continue;
        bool isParam = false;
        if (auto bo = dynamic_cast<SqDsp::BankOwner*> (&p)) for (auto& sp : bo->getBank().specs) isParam = isParam || id == sp.id;
        if (! isParam) set (props, id, p.state.getProperty (id));
    }
    set (o, "parameters", params);
    set (o, "settings", props);
    if (auto d = dynamic_cast<SqApexDynamicsPlugin*> (&p))
    {
        set (o, "mode", SqApexDynamicsPlugin::modeNames()[d->mode.get()].toLowerCase());
        set (o, "ratio", Apex::Curve::ratioText (d->bank[SqApexDynamicsPlugin::ratio], d->mode.get()));
        set (o, "gain_reduction_db", d->grDb.load());
        set (o, "key_input", d->getSidechainSourceName());
    }
    if (auto l = dynamic_cast<SqApexLimiterPlugin*> (&p))
    {
        auto m = obj();
        set (m, "integrated_lufs", l->integrated.load()); set (m, "short_term_lufs", l->shortTerm.load()); set (m, "momentary_lufs", l->momentary.load());
        set (m, "range_lu", l->lra.load()); set (m, "true_peak_max_dbtp", l->tpMax.load()); set (m, "gain_reduction_db", l->grDb.load());
        set (m, "target", SqApexLimiterPlugin::targets()[(size_t) juce::jlimit (0, (int) SqApexLimiterPlugin::targets().size() - 1, l->target.get())].name);
        set (o, "loudness", m);
    }
    if (auto mb = dynamic_cast<SqApexMultibandPlugin*> (&p))
    {
        juce::Array<juce::var> bands;
        for (int b = 0; b < mb->numBands.get(); ++b) { auto bo = obj(); set (bo, "band", b + 1); set (bo, "gain_change_db", mb->bandGr[(size_t) b].load()); bands.add (bo); }
        set (o, "bands_live", bands);
    }
    if (auto sb = dynamic_cast<SqApexSubPlugin*> (&p)) set (o, "tracked_hz", sb->trackedHz.load());
    if (auto op = dynamic_cast<SqOptoPlugin*> (&p)) { set (o, "gain_reduction_db", op->grDb.load()); set (o, "output_vu_db", op->outVu.load()); }
    if (auto lp = dynamic_cast<SqLadderPlugin*> (&p)) set (o, "live_cutoff_hz", lp->liveCutoff.load());
    if (auto rp = dynamic_cast<SqRingPlugin*> (&p)) set (o, "live_frequency_hz", rp->liveFreq.load());
    if (auto ap = dynamic_cast<SqAuroraPlugin*> (&p)) set (o, "room", SqAuroraPlugin::roomNames()[juce::jlimit (0, 7, ap->roomType.get())]);
    if (auto ph = dynamic_cast<SqPhase12Plugin*> (&p)) set (o, "stages", SqPhase12Plugin::stageCount (ph->stages.get()));
    return o;
}

void registerApexTools()
{
    add ("apex",
         "The Apex Series (Squelchmoth's Pro Series): which = dynamics | limiter | multiband | sub on a track or 'master'. Actions:\n"
         " add (optional preset), get (parameters, settings, live meters and - for the limiter - integrated/short-term/momentary LUFS, LRA and true-peak max),\n"
         " set - settings {name: value}: any parameter by name (Threshold, Ratio 1-20 / 21 = LMTR / 22-41 negative, Knee, Attack, Release, Hold, Hysteresis, Depth, Lookahead, Dry Mix, Makeup, Key Filter Freq/Q;\n"
         "   limiter Input, Ceiling, Release, Character; multiband Input/Output/Mix, Crossover 1-5, Band N Threshold/Depth/Gain/Slope/Attack/Release; sub Lower/Upper/Direct/High-pass/Low-pass/Drive/Mix)\n"
         "   or a setting: mode (compress/expand/gate/duck/upward), detection (smart/rms/average/peak/fast), scFilter (0-4), autoMakeup, attenListen, keyListen, externalKey, link, autoRelease, target (0-6 or name),\n"
         "   bands (1-6), linearPhase, soloN/bypassN/flipN/autoN, range (0-5), midiTune, driveType (0-5).\n"
         " key - dynamics key input from another track: source (track name, or none).\n"
         " match - limiter: set Input and Ceiling for the chosen loudness target from what it measured (play the song first). reset_meter - limiter loudness restart.\n"
         " preset - load a preset by name (or 'list').",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"which":{"type":"string"},"action":{"type":"string","enum":["add","get","set","key","match","reset_meter","preset","open"]},
            "settings":{"type":"object"},"source":{"type":"string"},"preset":{"type":"string"},"target":{"type":["string","integer"]}},"required":["track","which","action"]})json",
         [this] (const juce::var& a) { return runSeriesTool (a, apexTypeFor (str (a, "which"))); });

    add ("studio_fx",
         "Squelchmoth's colour plug-ins (originals standing in for Avid's paid bundle). which = aurora (Aurora Verb, FDN reverb) | opto (Opto Leveler) |\n"
         " pulse (Pulse EQ, passive program EQ + mid section) | oilcan (Oil Can Delay) | ladder (Silk Ladder) | ring (Halo Ring) | phaser (Twelve Phase) | bbd (Bucket Brigade).\n"
         " Actions: add (optional preset), get, set {name: value}, preset (name or 'list'), open.\n"
         " Parameters by name, e.g. Aurora: Pre-Delay, Size, Decay, Low Decay, High Decay, Diffusion, Early Level, Tail Level, Modulation, Mod Rate, Width, Low Cut, High Cut, Ducking, Mix;\n"
         "   Opto: Peak Reduction, Gain, Emphasis, Tube, Mix; Pulse: Low Boost, Low Atten, High Boost, Bandwidth, High Atten, Mid Low Peak, Mid Dip, Mid High Peak, Output, Tube;\n"
         "   Oil Can: Delay, Variation, Sustain, Tone, Drive, Wobble Rate, Mix; Ladder: Cutoff, Resonance, Env Amount, Drive, Output, Mix; Ring: Frequency, LFO Rate, LFO Amount, Drive, Output, Mix;\n"
         "   Phaser: Rate, Sweep, Centre, Resonance, Stereo Spread, Mix; BBD: Time, Feedback, Filter, Drive, Mod Rate, Mod Depth, Mix.\n"
         " Settings: roomType (Hall/Chamber/Room/Plate/Church/Cathedral/Studio/Ambience or 0-7), freeze, limitMode, meterMode, lowFreq/highFreq/attenFreq/midLowFreq/dipFreq/midHighFreq (Hz - snaps to the\n"
         "   nearest switch position), mode (0 echo / 1 doubler), sync (0 free, 1 1/16, 2 1/8, 3 dotted 1/8, 4 1/4, 5 1/2), poles (0 two / 1 four), envFast, lfoShape (0 sine / 1 square), stages (4, 6, 8 or 12).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"which":{"type":"string"},"action":{"type":"string","enum":["add","get","set","preset","open"]},
            "settings":{"type":"object"},"preset":{"type":"string"}},"required":["track","which","action"]})json",
         [this] (const juce::var& a)
         {
             const auto type = colourTypeFor (str (a, "which"));
             if (type.isEmpty()) return ToolResult::error ("which must be aurora, opto, pulse, oilcan, ladder, ring, phaser or bbd");
             return runSeriesTool (a, type);
         });
}

ToolResult runSeriesTool (const juce::var& a, const juce::String& type)
{
             juce::String err; te::AudioTrack* t = nullptr;
             auto list = pluginListArg (a, err, t);
             if (list == nullptr) return ToolResult::error (err);
             te::Plugin* p = nullptr;
             for (auto pl : *list) if (pl->getPluginType() == type) { p = pl; break; }
             const auto action = str (a, "action").toLowerCase();
             auto entry = [&] { for (auto& e : Signature::entries()) if (juce::String (e.type) == type) return &e; return (const Signature::Entry*) nullptr; };
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 auto e = entry(); juce::StringArray names; for (auto& pr : e->presets) names.add (pr.name);
                 for (auto& pr : e->presets) if (want.isNotEmpty() && juce::String (pr.name).containsIgnoreCase (want)) { Signature::applyPreset (*p, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                 return "Presets: " + names.joinIntoString (", ");
             };
             if (action == "add")
             {
                 if (p == nullptr)
                 {
                     auto np = session.edit->getPluginCache().createNewPlugin (type, {});
                     if (np == nullptr) return ToolResult::error ("Couldn't create the plug-in");
                     const int idx = t != nullptr ? Session::insertEndIndex (*list, t) : -1;
                     list->insertPlugin (np, idx, nullptr);
                     p = np.get();
                     if (auto e = entry(); e != nullptr && ! e->presets.empty() && ! has (a, "preset")) Signature::applyPreset (*p, e->presets.front());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (describeApex (*p));
             }
             if (p == nullptr) return ToolResult::error ("No " + juce::String (entry()->name) + " on that track - use action add");
             if (action == "get") return ToolResult::ok (describeApex (*p));
             if (action == "open") { p->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             auto* um = &session.edit->getUndoManager();
             if (action == "key")
             {
                 auto srcs = p->getSidechainSourceNames (true);
                 const auto want = str (a, "source");
                 int idx = 0;
                 for (int i = 1; i < srcs.size(); ++i) if (srcs[i].fromFirstOccurrenceOf (". ", false, false).equalsIgnoreCase (want) || srcs[i].containsIgnoreCase (want)) { idx = i; break; }
                 if (want.isNotEmpty() && ! want.equalsIgnoreCase ("none") && idx == 0) return ToolResult::error ("No track '" + want + "'. Sources: " + srcs.joinIntoString (", "));
                 p->setSidechainSourceByName (srcs[idx]);
                 p->state.setProperty ("externalKey", idx > 0, um);
                 return ToolResult::ok ("Key input: " + srcs[idx]);
             }
             if (action == "match" || action == "reset_meter")
             {
                 auto l = dynamic_cast<SqApexLimiterPlugin*> (p);
                 if (l == nullptr) return ToolResult::error ("match / reset_meter are for the Apex Limiter");
                 if (has (a, "target"))
                 {
                     auto tv = a["target"]; int ti = -1;
                     if (tv.isInt() || tv.isDouble()) ti = (int) tv;
                     else for (int i = 0; i < (int) SqApexLimiterPlugin::targets().size(); ++i) if (juce::String (SqApexLimiterPlugin::targets()[(size_t) i].name).containsIgnoreCase (tv.toString())) { ti = i; break; }
                     if (ti >= 0) l->target.setValue (ti, um);
                 }
                 if (action == "reset_meter") { l->resetRequest = true; return ToolResult::ok ("Loudness meter restarted"); }
                 return ToolResult::ok (l->matchTarget());
             }
             if (action == "set")
             {
                 auto s = a["settings"];
                 if (! s.isObject()) return ToolResult::error ("settings must be an object");
                 juce::StringArray problems;
                 for (auto& pr : s.getDynamicObject()->getProperties())
                 {
                     const auto key = pr.name.toString();
                     auto v = pr.value;
                     if (auto ap = Signature::findParam (*p, key)) { ap->setParameter (ap->getValueRange().clipValue ((float) (double) v), juce::sendNotification); continue; }
                     // settings with names
                     juce::String id = key;
                     if (key.equalsIgnoreCase ("mode") && v.isString()) v = SqApexDynamicsPlugin::modeNames().indexOf (v.toString().toUpperCase());
                     if (key.equalsIgnoreCase ("detection") && v.isString()) v = SqApexDynamicsPlugin::detectionNames().indexOf (v.toString().toUpperCase());
                     if (key.equalsIgnoreCase ("driveType") && v.isString()) v = SqApexSubPlugin::driveNames().indexOf (v.toString().toUpperCase());
                     if (key.equalsIgnoreCase ("target") && v.isString()) { int ti = 0; for (int i = 0; i < (int) SqApexLimiterPlugin::targets().size(); ++i) if (juce::String (SqApexLimiterPlugin::targets()[(size_t) i].name).containsIgnoreCase (v.toString())) ti = i; v = ti; }
                     if (key.equalsIgnoreCase ("roomType") && v.isString()) v = juce::jmax (0, SqAuroraPlugin::roomNames().indexOf (v.toString(), true));
                     if (key.equalsIgnoreCase ("stages") && (int) v >= 4) { const int want = (int) v; int best = 0; for (int i = 0; i < 4; ++i) if (std::abs (SqPhase12Plugin::stageCount (i) - want) < std::abs (SqPhase12Plugin::stageCount (best) - want)) best = i; v = best; }
                     if (dynamic_cast<SqPulseEqPlugin*> (p) != nullptr && key.endsWithIgnoreCase ("Freq") && (double) v > 10.0)
                     {
                         const std::vector<float>* tbl = nullptr;
                         if (key.equalsIgnoreCase ("lowFreq")) tbl = &SqPulseEqPlugin::lowFreqs();
                         else if (key.equalsIgnoreCase ("highFreq")) tbl = &SqPulseEqPlugin::highFreqs();
                         else if (key.equalsIgnoreCase ("attenFreq")) tbl = &SqPulseEqPlugin::attenFreqs();
                         else if (key.equalsIgnoreCase ("midLowFreq")) tbl = &SqPulseEqPlugin::midLowFreqs();
                         else if (key.equalsIgnoreCase ("dipFreq")) tbl = &SqPulseEqPlugin::dipFreqs();
                         else if (key.equalsIgnoreCase ("midHighFreq")) tbl = &SqPulseEqPlugin::midHighFreqs();
                         if (tbl != nullptr)
                         {
                             int best = 0; const double hz = (double) v;
                             for (int i = 0; i < (int) tbl->size(); ++i) if (std::abs (std::log ((*tbl)[(size_t) i] / hz)) < std::abs (std::log ((*tbl)[(size_t) best] / hz))) best = i;
                             v = best;
                         }
                     }
                     if (v.isBool()) v = (bool) v;
                     if (! p->state.hasProperty (id))
                     {
                         bool found = false;
                         for (int i = 0; i < p->state.getNumProperties(); ++i) if (p->state.getPropertyName (i).toString().equalsIgnoreCase (key)) { id = p->state.getPropertyName (i).toString(); found = true; }
                         if (! found) for (auto& k : knownPluginSettings()) if (k.equalsIgnoreCase (key)) { id = k; found = true; }
                         if (! found) { problems.add ("unknown setting '" + key + "'"); continue; }
                     }
                     if ((int) v < 0 && ! v.isBool() && ! v.isDouble()) { problems.add ("bad value for " + key); continue; }
                     p->state.setProperty (id, v, um);
                 }
                 auto out = describeApex (*p);
                 return problems.isEmpty() ? ToolResult::ok (out) : ToolResult::error (problems.joinIntoString ("; ") + "\n" + juce::JSON::toString (out));
             }
             return ToolResult::error ("unknown action");
}
