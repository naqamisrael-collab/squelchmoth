// Squelchmoth 1.10 "Console" - Claude tools for the new console and the wished features built in 1.10:
// mixer_layout, channel_dynamics, modulate (host LFOs), automation_humanise, sort_tracks, uncommit, render_queue.
// Included inside the McpTools class body.

te::Track* rowOrMaster (const juce::var& a, juce::String& err, bool& isMaster)
{
    isMaster = isMasterRef (a);
    if (isMaster) return nullptr;
    if (! has (a, "track")) { err = "Name the track (number, name, or 'master')."; return nullptr; }
    auto r = session.findRow (a["track"]);
    if (r == nullptr) err = "No track matches '" + a["track"].toString() + "'. Call get_session for the list.";
    return r;
}

juce::var describeStrip (te::Track* row)
{
    auto o = obj();
    set (o, "track", row != nullptr ? row->getName() : juce::String ("Master"));
    set (o, "width", Console::widthNames()[(int) Console::Model::storedWidth (session, row)].toLowerCase());
    set (o, "width_shown", Console::widthNames()[(int) Console::Model::effectiveWidth (session, row)].toLowerCase());
    if (Console::Model::canTap (row)) set (o, "meter_type", juce::StringArray { "peak", "rms", "lufs_m", "lufs_s" }[(int) Console::Model::meterType (session, row)]);
    auto& list = row != nullptr ? row->pluginList : edit().getMasterPluginList();
    auto* at = dynamic_cast<te::AudioTrack*> (row);
    for (bool limit : { false, true })
    {
        auto p = Console::Model::findDynamics (list, at, limit);
        auto d = obj();
        set (d, "plugin", Console::Model::labelFor (p, limit));
        if (p != nullptr)
        {
            set (d, "bypassed", ! p->isEnabled());
            if (auto gr = Console::Model::gainReduction (p)) set (d, "gain_reduction_db", juce::roundToInt (*gr * 10.0f) / 10.0);
            else set (d, "gain_reduction_db", "not reported by this plug-in");
        }
        set (o, limit ? "limit" : "comp", d);
    }
    if (auto tap = Console::Model::findTap (list))
    {
        const float post = row == nullptr ? edit().getMasterVolumePlugin()->getVolumeDb() : 0.0f;
        auto m = obj();
        auto lu = [&] (float v) { return v > -99.0f ? juce::var (juce::roundToInt ((v + post) * 10.0f) / 10.0) : juce::var ("--"); };
        set (m, "rms_db_l", lu (tap->getRmsDb (0))); set (m, "rms_db_r", lu (tap->getRmsDb (1)));
        set (m, "lufs_momentary", lu (tap->getMomentary())); set (m, "lufs_short_term", lu (tap->getShortTerm()));
        set (m, "lufs_integrated", lu (tap->getIntegrated()));
        set (o, "meters", m);
    }
    return o;
}

void register110Tools()
{
    auto refresh = [this] { if (ui.refresh) ui.refresh(); };

    //==========================================================================
    add ("mixer_layout",
         "The Mix window console. Actions: get (view, channel layout, saved layouts and every strip's width and meter type); "
         "set_width (track or 'master', width compact|standard|expanded); set_meter (track or 'master', meter peak|rms|lufs_m|lufs_s - RMS and LUFS are BS.1770-4); "
         "set_view (view dynamics|full|overview - Full adds inserts, sends, I/O and pan above the dynamics); set_layout (layout flexible|all_compact|all_standard|all_expanded); "
         "save (name) / recall (name) / delete (name) a mixer layout stored in the session.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["get","set_width","set_meter","set_view","set_layout","save","recall","delete"]},"track":{"type":["string","integer"]},
            "width":{"type":"string","enum":["compact","standard","expanded"]},"meter":{"type":"string","enum":["peak","rms","lufs_m","lufs_s"]},"view":{"type":"string","enum":["dynamics","full","overview"]},
            "layout":{"type":"string","enum":["flexible","all_compact","all_standard","all_expanded"]},"name":{"type":"string"}},"required":["action"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto act = str (a, "action").toLowerCase();
             juce::String err; bool isMaster = false;
             if (act == "get")
             {
                 auto o = obj();
                 set (o, "view", Console::viewNames()[(int) Console::Model::view()].toLowerCase());
                 set (o, "channel_layout", juce::StringArray { "flexible", "all_compact", "all_standard", "all_expanded" }[(int) Console::Model::layout()]);
                 juce::Array<juce::var> saved; for (auto& n : Console::Model::layoutList (session)) saved.add (n);
                 set (o, "saved_layouts", saved);
                 juce::Array<juce::var> strips;
                 for (auto r : session.getRows()) if (! r->isHidden()) strips.add (describeStrip (r));
                 if (TrackOps::hasMasterFader (session)) strips.add (describeStrip (nullptr));
                 set (o, "strips", strips);
                 return ToolResult::ok (o);
             }
             if (act == "set_width" || act == "set_meter")
             {
                 auto row = rowOrMaster (a, err, isMaster);
                 if (row == nullptr && ! isMaster) return ToolResult::error (err);
                 if (act == "set_width")
                 {
                     auto w = Console::widthNames().indexOf (str (a, "width"), true);
                     if (w < 0) return ToolResult::error ("width must be compact, standard or expanded");
                     Console::Model::setWidth (session, row, (Console::Width) w);
                 }
                 else
                 {
                     auto m = juce::StringArray { "peak", "rms", "lufs_m", "lufs_s" }.indexOf (str (a, "meter").toLowerCase());
                     if (m < 0) return ToolResult::error ("meter must be peak, rms, lufs_m or lufs_s");
                     if (! Console::Model::canTap (row) && m != 0) return ToolResult::error ("Folder and VCA strips meter peak only.");
                     Console::Model::setMeterType (session, row, (Console::MeterType) m);
                     if (m != 0) Console::Model::ensureTap (session, row);
                 }
                 refresh();
                 return ToolResult::ok (describeStrip (row));
             }
             if (act == "set_view")
             {
                 auto v = Console::viewNames().indexOf (str (a, "view"), true);
                 if (v < 0) return ToolResult::error ("view must be dynamics, full or overview");
                 Console::Model::setView ((Console::View) v);
                 return ToolResult::ok ("Mix window view: " + Console::viewNames()[v]);
             }
             if (act == "set_layout")
             {
                 auto l = juce::StringArray { "flexible", "all_compact", "all_standard", "all_expanded" }.indexOf (str (a, "layout").toLowerCase());
                 if (l < 0) return ToolResult::error ("layout must be flexible, all_compact, all_standard or all_expanded");
                 Console::Model::setLayout ((Console::Layout) l);
                 return ToolResult::ok ("Channel layout: " + Console::layoutNames()[l]);
             }
             auto name = str (a, "name").trim();
             if (name.isEmpty()) return ToolResult::error ("Give the layout a name.");
             if (act == "save")   { Console::Model::saveLayout (session, name); return ToolResult::ok ("Saved mixer layout '" + name + "'"); }
             if (act == "recall") { if (! Console::Model::recallLayout (session, name)) return ToolResult::error ("No layout named '" + name + "'"); refresh(); return ToolResult::ok ("Recalled mixer layout '" + name + "'"); }
             if (act == "delete") { if (! Console::Model::deleteLayout (session, name)) return ToolResult::error ("No layout named '" + name + "'"); return ToolResult::ok ("Deleted mixer layout '" + name + "'"); }
             return ToolResult::error ("Unknown action");
         });

    //==========================================================================
    add ("channel_dynamics",
         "A channel's COMP and LIMIT slots (the console strip selectors). get: the plug-ins, bypass state and live gain reduction in dB "
         "(Cocoon Compressor, Apex Dynamics / Limiter / Multiband / Triad and Opto Leveler report it; others say so). "
         "set: comp and/or limit = a name or 'none'. COMP choices: Cocoon Compressor, Apex Dynamics, Opto Leveler, Apex Multiband, Moth Press, Pupa Buss Comp, Vari-Mu Tube. "
         "LIMIT choices: Apex Limiter, Cocoon Limit (Cocoon at infinite ratio, -1 dBFS), Clamp Limiter, Apex Triad. COMP goes before the limiter; LIMIT is last before the fader. "
         "To change their settings use set_plugin_parameter on the slot get_session lists.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["get","set"]},"track":{"type":["string","integer"]},"comp":{"type":"string"},"limit":{"type":"string"}},"required":["action","track"]})json",
         [this, refresh] (const juce::var& a)
         {
             juce::String err; bool isMaster = false;
             auto row = rowOrMaster (a, err, isMaster);
             if (row == nullptr && ! isMaster) return ToolResult::error (err);
             if (row != nullptr && dynamic_cast<te::AudioTrack*> (row) == nullptr && ! (dynamic_cast<te::FolderTrack*> (row) != nullptr && dynamic_cast<te::FolderTrack*> (row)->isSubmixFolder()))
                 return ToolResult::error (row->getName() + " has no inserts (VCA), so it has no COMP / LIMIT slots.");
             if (str (a, "action").equalsIgnoreCase ("set"))
             {
                 juce::StringArray done;
                 for (bool limit : { false, true })
                 {
                     const char* key = limit ? "limit" : "comp";
                     if (! has (a, key)) continue;
                     auto e = Console::Model::setDynamics (session, row, limit, str (a, key));
                     if (e.isNotEmpty()) return ToolResult::error (e + (done.isEmpty() ? juce::String() : " (already done: " + done.joinIntoString (", ") + ")"));
                     done.add (juce::String (limit ? "LIMIT" : "COMP") + " = " + str (a, key));
                 }
                 if (done.isEmpty()) return ToolResult::error ("Pass comp and/or limit.");
                 refresh();
             }
             return ToolResult::ok (describeStrip (row));
         });

    //==========================================================================
    add ("modulate",
         "Host modulation (Studio One / Logic wish): an LFO on a track that moves any parameter - volume, pan or a plug-in parameter (slot + param). "
         "add: wave sine|triangle|saw_up|saw_down|square|random (default sine); rate_hz (free-running) or sync ('1/4', '1/8', '1/16', '1/2', '1 bar', '2 bars', '4 bars', with T or D for triplet / dotted, e.g. '1/8 D'); "
         "depth 0..1 (how far it moves the parameter, default 0.5); bipolar (default true: swings both ways around the set value); phase 0..1. "
         "list: every LFO on the track and what it moves. remove: lfo (number from list) or all=true. Modulation plays back and prints into bounces.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["add","list","remove"]},"track":{"type":["string","integer"]},"target":{"type":"string","description":"volume, pan or plugin"},
            "slot":{"type":["string","integer"]},"param":{"type":["string","integer"]},"wave":{"type":"string"},"rate_hz":{"type":"number"},"sync":{"type":"string"},"depth":{"type":"number"},"bipolar":{"type":"boolean"},
            "phase":{"type":"number"},"lfo":{"type":"integer"},"all":{"type":"boolean"}},"required":["action","track"]})json",
         [this] (const juce::var& a)
         {
             juce::String err;
             auto t = trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             auto ml = t->getModifierList();
             if (ml == nullptr) return ToolResult::error ("This track cannot hold modulators.");
             auto act = str (a, "action").toLowerCase();

             auto paramsOfTrack = [t]
             {
                 juce::Array<te::AutomatableParameter*> ps;
                 for (auto p : t->pluginList) for (auto ap : p->getAutomatableParameters()) ps.add (ap);
                 return ps;
             };

             if (act == "list")
             {
                 juce::Array<juce::var> out;
                 int i = 0;
                 for (auto lfo : te::getModifiersOfType<te::LFOModifier> (*ml))
                 {
                     ++i;
                     auto o = obj();
                     set (o, "lfo", i);
                     set (o, "wave", te::LFOModifier::getWaveNames()[(int) lfo->wave.get()]);
                     const bool synced = (int) lfo->syncType.get() != 0;
                     set (o, "rate", synced ? te::ModifierCommon::getRateTypeChoices()[(int) lfo->rateType.get()] : juce::String (lfo->rate.get(), 2) + " Hz");
                     juce::Array<juce::var> targets;
                     for (auto ap : paramsOfTrack())
                         for (auto as : ap->getAssignments())
                             if (as->isForModifierSource (*lfo))
                             {
                                 auto tg = obj();
                                 auto* pl = ap->getPlugin();
                                 set (tg, "parameter", (pl == nullptr || dynamic_cast<te::VolumeAndPanPlugin*> (pl) != nullptr ? juce::String ("Track") : Signature::displayName (*pl)) + " > " + ap->getParameterName());
                                 set (tg, "depth", juce::roundToInt (as->value.get() * 100.0f) / 100.0);
                                 targets.add (tg);
                             }
                     set (o, "moves", targets);
                     out.add (o);
                 }
                 return ToolResult::ok (juce::var (out));
             }
             if (act == "remove")
             {
                 auto lfos = te::getModifiersOfType<te::LFOModifier> (*ml);
                 int removed = 0;
                 for (int i = lfos.size(); --i >= 0;)
                 {
                     if (! flag (a, "all") && (int) num (a, "lfo", -1) != i + 1) continue;
                     for (auto ap : paramsOfTrack()) ap->removeModifier (*lfos[i]);
                     lfos[i]->remove();
                     ++removed;
                 }
                 if (removed == 0) return ToolResult::error ("No matching LFO. Use action list (lfo numbers start at 1) or all=true.");
                 return ToolResult::ok ("Removed " + juce::String (removed) + " LFO" + (removed == 1 ? "" : "s") + " from " + t->getName());
             }
             if (act != "add") return ToolResult::error ("Unknown action");

             // target parameter
             te::AutomatableParameter* param = nullptr;
             auto target = str (a, "target", has (a, "param") ? "plugin" : "volume").toLowerCase();
             if (target == "volume") param = t->getVolumePlugin()->volParam.get();
             else if (target == "pan") param = t->getVolumePlugin()->panParam.get();
             else
             {
                 auto p = insertArg (a, err);
                 if (p == nullptr) return ToolResult::error (err);
                 param = findParam (*p, a["param"]);
             }
             if (param == nullptr) return ToolResult::error ("Parameter not found. Use get_plugin_parameters to see the names.");

             auto mod = ml->insertModifier (juce::ValueTree (te::IDs::LFO), -1, nullptr);
             auto lfo = dynamic_cast<te::LFOModifier*> (mod.get());
             if (lfo == nullptr) return ToolResult::error ("Could not create the LFO.");
             auto waveName = str (a, "wave", "sine").toLowerCase().replace ("_", " ");
             static const std::map<juce::String, int> waves { { "sine", 0 }, { "triangle", 1 }, { "saw up", 2 }, { "saw", 2 }, { "saw down", 3 }, { "square", 4 }, { "random", 9 }, { "noise", 10 } };
             auto wi = waves.find (waveName);
             lfo->waveParam->setParameter ((float) (wi != waves.end() ? wi->second : 0), juce::sendNotification);
             juce::String rateDesc;
             if (has (a, "sync"))
             {
                 auto s = str (a, "sync").trim().toLowerCase().replace ("bars", "bar").replace ("1 bar", "1 bar");
                 auto choices = te::ModifierCommon::getRateTypeChoices();
                 int idx = -1;
                 for (int i = 1; i < choices.size(); ++i) if (choices[i].toLowerCase().replace ("bars", "bar").removeCharacters (" ") == s.removeCharacters (" ")) idx = i;
                 if (idx < 0) { lfo->remove(); return ToolResult::error ("sync must be like 1/4, 1/8 T, 1/16 D, 1 bar, 2 bars or 4 bars"); }
                 lfo->syncTypeParam->setParameter ((float) te::ModifierCommon::transport, juce::sendNotification);
                 lfo->rateTypeParam->setParameter ((float) idx, juce::sendNotification);
                 rateDesc = choices[idx] + " (tempo-synced)";
             }
             else
             {
                 const float hz = (float) juce::jlimit (0.01, 50.0, num (a, "rate_hz", 1.0));
                 lfo->syncTypeParam->setParameter ((float) te::ModifierCommon::free, juce::sendNotification);
                 lfo->rateTypeParam->setParameter ((float) te::ModifierCommon::hertz, juce::sendNotification);
                 lfo->rateParam->setParameter (hz, juce::sendNotification);
                 rateDesc = juce::String (hz, 2) + " Hz";
             }
             const bool bip = flag (a, "bipolar", true);
             lfo->bipolarParam->setParameter (bip ? 1.0f : 0.0f, juce::sendNotification);
             lfo->depthParam->setParameter (1.0f, juce::sendNotification);
             if (has (a, "phase")) lfo->phaseParam->setParameter ((float) juce::jlimit (0.0, 1.0, num (a, "phase")), juce::sendNotification);
             const float depth = (float) juce::jlimit (0.0, 1.0, num (a, "depth", 0.5));
             param->addModifier (*lfo, depth);
             session.log.add ("Claude", "LFO on " + t->getName() + " moves " + param->getParameterName());
             return ToolResult::ok ("Added a " + te::LFOModifier::getWaveNames()[(int) lfo->wave.get()] + " LFO at " + rateDesc + " moving " + param->getParameterName()
                                    + " on " + t->getName() + " (depth " + juce::String (depth, 2) + (bip ? ", bipolar" : ", unipolar") + ").");
         });

    //==========================================================================
    add ("automation_humanise",
         "Humanise or randomise existing automation (Studio One wish): nudges every point of a track's volume / pan / plug-in parameter curve in time by up to "
         "time_ms (default 10) and in value by up to amount (volume: dB, default 0.5; pan and plug-in parameters: fraction of the full range, default 0.02). "
         "Optional start_bar / end_bar limit the range; seed makes it repeatable. Points never cross each other. Undoable.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"target":{"type":"string"},"slot":{"type":["string","integer"]},"param":{"type":["string","integer"]},
            "time_ms":{"type":"number"},"amount":{"type":"number"},"start_bar":{"type":"number"},"end_bar":{"type":"number"},"seed":{"type":"integer"}},"required":["track"]})json",
         [this] (const juce::var& a)
         {
             juce::String err;
             te::AutomatableParameter* param = nullptr;
             auto target = str (a, "target", has (a, "param") ? "plugin" : "volume").toLowerCase();
             bool isVolume = false;
             if (isMasterRef (a) && (target == "volume" || target == "pan"))
             {
                 auto vp = edit().getMasterVolumePlugin();
                 param = target == "volume" ? vp->volParam.get() : vp->panParam.get(); isVolume = target == "volume";
             }
             else if (target == "volume" || target == "pan")
             {
                 auto t = trackArg (a, err);
                 if (t == nullptr) return ToolResult::error (err);
                 param = target == "volume" ? t->getVolumePlugin()->volParam.get() : t->getVolumePlugin()->panParam.get();
                 isVolume = target == "volume";
             }
             else if (auto p = insertArg (a, err)) param = findParam (*p, a["param"]);
             if (param == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "Parameter not found");
             auto& curve = param->getCurve();
             const int n = curve.getNumPoints();
             if (n == 0) return ToolResult::error (param->getParameterName() + " has no automation points to humanise. Write some with write_automation first.");

             juce::Random rng (has (a, "seed") ? (juce::int64) num (a, "seed") : juce::Time::currentTimeMillis());
             const double maxShift = juce::jlimit (0.0, 500.0, num (a, "time_ms", 10.0)) / 1000.0;
             const auto range = param->getValueRange();
             const float amt = (float) num (a, "amount", isVolume ? 0.5 : 0.02);
             const double t0 = has (a, "start_bar") ? session.barToTime (num (a, "start_bar")).inSeconds() : -1.0e9;
             const double t1 = has (a, "end_bar") ? session.barToTime (num (a, "end_bar")).inSeconds() : 1.0e9;
             int changed = 0;
             for (int i = 0; i < curve.getNumPoints(); ++i)
             {
                 const double t = curve.getPointTime (i).inSeconds();
                 if (t < t0 || t > t1) continue;
                 const double prev = i > 0 ? curve.getPointTime (i - 1).inSeconds() : 0.0;
                 const double next = i < curve.getNumPoints() - 1 ? curve.getPointTime (i + 1).inSeconds() : 1.0e9;
                 const double nt = juce::jlimit (prev + 0.001, next - 0.001, juce::jmax (0.0, t + (rng.nextDouble() * 2.0 - 1.0) * maxShift));
                 float v = curve.getPointValue (i);
                 const float jitter = (rng.nextFloat() * 2.0f - 1.0f) * amt;
                 if (isVolume) v = te::decibelsToVolumeFaderPosition (juce::jlimit (-96.0f, 12.0f, te::volumeFaderPositionToDB (v) + jitter));
                 else v = range.clipValue (v + jitter * range.getLength());
                 curve.movePoint (i, te::TimePosition::fromSeconds (nt), v, std::nullopt, false, um());
                 ++changed;
             }
             return ToolResult::ok ("Humanised " + juce::String (changed) + " of " + juce::String (n) + " points on " + param->getParameterName()
                                    + " (time +/-" + juce::String (maxShift * 1000.0, 1) + " ms, value +/-" + juce::String (amt, 3) + (isVolume ? " dB" : " of range") + ").");
         });

    //==========================================================================
    add ("sort_tracks",
         "Sort the session's top-level tracks (Studio One wish): by name (A-Z), colour (hue), or type (audio, instrument, MIDI, aux, folders). "
         "reverse=true flips the order. Folders keep their contents. Undoable.",
         R"json({"type":"object","properties":{"by":{"type":"string","enum":["name","colour","type"]},"reverse":{"type":"boolean"}},"required":["by"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto by = str (a, "by", "name").toLowerCase();
             juce::Array<te::Track*> rows, all = te::getTopLevelTracks (edit());
             te::Track* anchor = nullptr;
             for (auto t : all)
             {
                 if (t->isAudioTrack() || t->isFolderTrack()) rows.add (t);
                 else if (rows.isEmpty()) anchor = t;   // tempo, marker, chord tracks stay above
             }
             if (rows.size() < 2) return ToolResult::ok ("Nothing to sort.");
             auto typeRank = [] (te::Track* t)
             {
                 if (t->isFolderTrack()) return 4;
                 auto at = dynamic_cast<te::AudioTrack*> (t);
                 auto k = IO::kindOf (*at);
                 return k == IO::Kind::audio ? 0 : k == IO::Kind::instrument ? 1 : k == IO::Kind::midi ? 2 : 3;
             };
             std::vector<te::Track*> v (rows.begin(), rows.end());
             std::stable_sort (v.begin(), v.end(), [&] (te::Track* x, te::Track* y)
             {
                 if (by == "colour" || by == "color")
                 {
                     auto hx = x->getColour().getHue(), hy = y->getColour().getHue();
                     if (std::abs (hx - hy) > 0.001f) return hx < hy;
                 }
                 else if (by == "type")
                 {
                     if (typeRank (x) != typeRank (y)) return typeRank (x) < typeRank (y);
                 }
                 return x->getName().compareNatural (y->getName()) < 0;
             });
             if (flag (a, "reverse")) std::reverse (v.begin(), v.end());
             te::Track* prev = anchor;
             for (auto t : v) { edit().moveTrack (t, te::TrackInsertPoint (nullptr, prev)); prev = t; }
             refresh();
             juce::StringArray names; for (auto t : v) names.add (t->getName());
             return ToolResult::ok ("Sorted by " + by + ": " + names.joinIntoString (", "));
         });

    //==========================================================================
    add ("uncommit",
         "Reverse a Commit (FL Studio wish: reversible render-in-place): brings the source track back (visible and active) and removes the rendered '.cm' track. "
         "track = the committed track or its source. keep_rendered=true keeps the rendered audio track too.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"keep_rendered":{"type":"boolean"}},"required":["track"]})json",
         [this, refresh] (const juce::var& a)
         {
             te::Track* t = session.findRow (a["track"]);
             if (t == nullptr)   // hidden sources are not rows: match by name across every track
                 for (auto x : te::getAllTracks (edit())) if (x->getName().equalsIgnoreCase (a["track"].toString())) t = x;
             if (t == nullptr) return ToolResult::error ("No track matches '" + a["track"].toString() + "'.");
             juce::String err;
             auto src = TrackOps::uncommit (session, *t, flag (a, "keep_rendered"), err);
             if (src == nullptr) return ToolResult::error (err);
             refresh();
             return ToolResult::ok ("Restored " + src->getName() + (flag (a, "keep_rendered") ? " (kept the rendered track)" : ""));
         });

    //==========================================================================
    add ("render_queue",
         "Render queue (Logic and Studio One wish): line up bounces and stem exports, then run them in one go. "
         "add: job 'bounce' or 'stems' with options = the same arguments bounce / export_stems take (e.g. {\"start_bar\":1,\"end_bar\":33,\"normalize_lufs\":-14} or {\"source\":\"folders\",\"skip_silent\":true}), optional label. "
         "list; remove (index from 1); clear; run (runs every job in order, keeps going after a failure, reports each result, then empties the queue unless keep=true). "
         "The queue is saved with the session.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["add","list","remove","clear","run"]},"job":{"type":"string","enum":["bounce","stems"]},"options":{"type":"object"},
            "label":{"type":"string"},"index":{"type":"integer"},"keep":{"type":"boolean"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto readQueue = [this]
             {
                 auto v = juce::JSON::parse (edit().state.getProperty ("sqmRenderQueue").toString());
                 return v.isArray() ? *v.getArray() : juce::Array<juce::var>();
             };
             auto writeQueue = [this] (const juce::Array<juce::var>& q)
             {
                 if (q.isEmpty()) edit().state.removeProperty ("sqmRenderQueue", nullptr);
                 else edit().state.setProperty ("sqmRenderQueue", juce::JSON::toString (juce::var (q), true), nullptr);
             };
             auto q = readQueue();
             auto act = str (a, "action").toLowerCase();
             if (act == "list") return ToolResult::ok (juce::var (q));
             if (act == "clear") { writeQueue ({}); return ToolResult::ok ("Render queue cleared."); }
             if (act == "add")
             {
                 auto job = str (a, "job").toLowerCase();
                 if (job != "bounce" && job != "stems") return ToolResult::error ("job must be bounce or stems");
                 auto o = obj();
                 set (o, "job", job);
                 set (o, "label", str (a, "label", job == "bounce" ? "Mix bounce" : "Stems"));
                 set (o, "options", a["options"].isObject() ? a["options"] : obj());
                 q.add (o);
                 writeQueue (q);
                 return ToolResult::ok ("Queued job " + juce::String (q.size()) + ": " + o["label"].toString() + ". Run it with action run.");
             }
             if (act == "remove")
             {
                 const int i = (int) num (a, "index", 0) - 1;
                 if (i < 0 || i >= q.size()) return ToolResult::error ("index must be 1.." + juce::String (q.size()));
                 q.remove (i); writeQueue (q);
                 return ToolResult::ok ("Removed job " + juce::String (i + 1) + ". " + juce::String (q.size()) + " left.");
             }
             if (act == "run")
             {
                 if (q.isEmpty()) return ToolResult::error ("The render queue is empty.");
                 juce::Array<juce::var> results;
                 int ok = 0;
                 for (int i = 0; i < q.size(); ++i)
                 {
                     auto& job = q.getReference (i);
                     auto r = call (job["job"].toString() == "stems" ? "export_stems" : "bounce", job["options"]);
                     auto o = obj();
                     set (o, "job", i + 1); set (o, "label", job["label"]); set (o, "ok", ! r.isError);
                     set (o, "result", r.text.substring (0, 600));
                     results.add (o);
                     if (! r.isError) ++ok;
                 }
                 if (! flag (a, "keep")) writeQueue ({});
                 auto out = obj();
                 set (out, "ran", q.size()); set (out, "succeeded", ok); set (out, "jobs", results);
                 return ok > 0 ? ToolResult::ok (out) : ToolResult { juce::JSON::toString (out, false), true, {}, {} };
             }
             return ToolResult::error ("Unknown action");
         });

    //==========================================================================
    add ("monitor_chain",
         "The Control Room (Logic wish): Master inserts that you hear on playback but that NEVER print into bounces, stems or the mastering bounce - room correction, headphone EQ, a reference trim. "
         "list; add (plugin name: inserted on the Master as monitor-only, after the meter tap so LUFS readings stay what prints); set (slot = Master insert number from list, monitor true/false); "
         "master_fader_prints false keeps the Master fader out of every render (turn the room down without changing the printed level), true restores normal printing.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","add","set","master_fader_prints"]},"plugin":{"type":"string"},"slot":{"type":"integer"},"monitor":{"type":"boolean"},"value":{"type":"boolean"}},"required":["action"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto act = str (a, "action").toLowerCase();
             auto& list = edit().getMasterPluginList();
             auto inserts = Session::getInserts (list, nullptr);
             if (act == "list")
             {
                 auto o = obj();
                 juce::Array<juce::var> arr;
                 for (int i = 0; i < inserts.size(); ++i)
                 {
                     auto e = obj(); set (e, "slot", i + 1); set (e, "plugin", Signature::displayName (*inserts[i]));
                     set (e, "prints", ! MonitorChain::isMonitorOnly (*inserts[i])); arr.add (e);
                 }
                 set (o, "master_inserts", arr);
                 set (o, "master_fader_prints", MonitorChain::masterFaderPrints());
                 return ToolResult::ok (o);
             }
             if (act == "master_fader_prints")
             {
                 Prefs::set ("bx_masterFaderOff", ! flag (a, "value", true));
                 return ToolResult::ok (MonitorChain::masterFaderPrints() ? "The Master fader prints into bounces again." : "Renders now ignore the Master fader (it only sets your listening level).");
             }
             if (act == "add")
             {
                 juce::String err;
                 auto p = session.createPluginByName (str (a, "plugin"), err);
                 if (p == nullptr) return ToolResult::error (err.isNotEmpty() ? err : "No plug-in named '" + str (a, "plugin") + "'");
                 list.insertPlugin (p, -1, nullptr);
                 MonitorChain::setMonitorOnly (session, *p, true);
                 refresh();
                 return ToolResult::ok (Signature::displayName (*p) + " is on the Master as a Control Room insert: heard on playback, never printed.");
             }
             if (act == "set")
             {
                 const int slot = (int) num (a, "slot", 0) - 1;
                 if (slot < 0 || slot >= inserts.size()) return ToolResult::error ("slot must be 1.." + juce::String (inserts.size()) + " (action list)");
                 MonitorChain::setMonitorOnly (session, *inserts[slot], flag (a, "monitor", true));
                 refresh();
                 return ToolResult::ok (Signature::displayName (*inserts[slot]) + (flag (a, "monitor", true) ? " is now monitor-only." : " prints again."));
             }
             return ToolResult::error ("Unknown action");
         });

    //==========================================================================
    add ("plugin_favourites",
         "Plug-in favourites (Studio One wish): names shown in a Favourites submenu at the top of every plug-in menu. list; add (name); remove (name). Names as list_plugins shows them.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","add","remove"]},"name":{"type":"string"}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             auto act = str (a, "action").toLowerCase();
             if (act != "list")
             {
                 auto n = str (a, "name").trim();
                 if (n.isEmpty()) return ToolResult::error ("Give the plug-in name.");
                 PluginMenu::setFavourite (n, act == "add");
             }
             juce::Array<juce::var> arr; for (auto& f : PluginMenu::favourites()) arr.add (f);
             return ToolResult::ok (juce::var (arr));
         });

    //==========================================================================
    add ("midi_cc",
         "MIDI controller data in a MIDI clip (Logic wish: CC lanes). cc: a controller number 0-127 (1 mod wheel, 7 volume, 10 pan, 11 expression, 64 sustain, 74 brightness), or 'pitch' / 'pressure'. "
         "action read: the events (beat, value). write: points [{beat, value}] (0-127; pitch -8192..8191) - replace=true clears that controller first. "
         "ramp: from_beat, to_beat, from, to, steps (default 16) - a smooth line, e.g. a filter sweep. clear: remove that controller. Beats are relative to the clip start. Undoable.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["read","write","ramp","clear"]},"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"cc":{"type":["string","integer"]},
            "points":{"type":"array","items":{"type":"object","properties":{"beat":{"type":"number"},"value":{"type":"number"}},"required":["beat","value"]}},"replace":{"type":"boolean"},
            "from_beat":{"type":"number"},"to_beat":{"type":"number"},"from":{"type":"number"},"to":{"type":"number"},"steps":{"type":"integer"}},"required":["action","track","cc"]})json",
         [this] (const juce::var& a)
         {
             juce::String err;
             auto t = trackArg (a, err);
             if (t == nullptr) return ToolResult::error (err);
             te::MidiClip* mc = nullptr;
             if (has (a, "clip")) mc = dynamic_cast<te::MidiClip*> (clipArg (*t, a, err));
             else for (auto c : t->getClips()) if (auto m = dynamic_cast<te::MidiClip*> (c)) { mc = m; break; }
             if (mc == nullptr) return ToolResult::error (err.isNotEmpty() ? err : t->getName() + " has no MIDI clip (create_midi_clip first).");
             auto ccs = a["cc"].toString().trim().toLowerCase();
             int type; bool pitch = false;
             if (ccs == "pitch" || ccs == "pitchbend" || ccs == "pitch wheel") { type = te::MidiControllerEvent::pitchWheelType; pitch = true; }
             else if (ccs == "pressure" || ccs == "aftertouch") type = te::MidiControllerEvent::channelPressureType;
             else { type = ccs.getIntValue(); if (type < 0 || type > 127 || ! ccs.containsOnly ("0123456789")) return ToolResult::error ("cc must be 0-127, 'pitch' or 'pressure'"); }
             auto& seq = mc->getSequence();
             auto toStored = [pitch] (double v) { return pitch ? juce::jlimit (0, 16383, juce::roundToInt (v) + 8192) : juce::jlimit (0, 16383, juce::roundToInt (juce::jlimit (0.0, 127.0, v)) << 7); };
             auto fromStored = [pitch] (int v) { return pitch ? v - 8192 : v >> 7; };
             auto clearType = [&]
             {
                 juce::Array<te::MidiControllerEvent*> kill;
                 for (auto e : seq.getControllerEvents()) if (e->getType() == type) kill.add (e);
                 for (auto e : kill) seq.removeControllerEvent (*e, um());
                 return kill.size();
             };
             auto act = str (a, "action").toLowerCase();
             if (act == "clear") return ToolResult::ok ("Removed " + juce::String (clearType()) + " events.");
             if (act == "write" || act == "ramp")
             {
                 if (act == "write" && flag (a, "replace")) clearType();
                 int n = 0;
                 if (act == "write")
                 {
                     if (auto arr = a["points"].getArray())
                         for (auto& p : *arr) { seq.addControllerEvent (te::BeatPosition::fromBeats ((double) p["beat"]), type, toStored ((double) p["value"]), um()); ++n; }
                 }
                 else
                 {
                     const double b0 = num (a, "from_beat", 0.0), b1 = num (a, "to_beat", b0 + 4.0), v0 = num (a, "from", 0.0), v1 = num (a, "to", 127.0);
                     const int steps = juce::jlimit (2, 512, (int) num (a, "steps", 16));
                     if (b1 <= b0) return ToolResult::error ("to_beat must be after from_beat");
                     for (int i = 0; i <= steps; ++i)
                     {
                         const double f = (double) i / steps;
                         seq.addControllerEvent (te::BeatPosition::fromBeats (b0 + f * (b1 - b0)), type, toStored (v0 + f * (v1 - v0)), um()); ++n;
                     }
                 }
                 return ToolResult::ok ("Wrote " + juce::String (n) + " " + te::MidiControllerEvent::getControllerTypeName (type) + " events in " + mc->getName() + ".");
             }
             juce::Array<juce::var> arr;
             for (auto e : seq.getControllerEvents())
                 if (e->getType() == type) { auto o = obj(); set (o, "beat", e->getBeatPosition().inBeats()); set (o, "value", fromStored (e->getControllerValue())); arr.add (o); }
             return ToolResult::ok (juce::var (arr));
         });

    //==========================================================================
    add ("clip_bins",
         "Clip List bins (Pro Tools wish): folders for clips. list: every bin and its clips (plus unfiled). file: put clips in a bin - track + clip (one) or clips [{track, clip}] - bin name ('' or 'none' unfiles). "
         "rename (bin, to). The Clip List window (Window > Clip List) shows bins as collapsible folders. Undoable.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["list","file","rename"]},"bin":{"type":"string"},"to":{"type":"string"},"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},
            "clips":{"type":"array","items":{"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]}}}}},"required":["action"]})json",
         [this] (const juce::var& a)
         {
             static const juce::Identifier binId ("sqmBin");
             auto act = str (a, "action").toLowerCase();
             if (act == "list")
             {
                 std::map<juce::String, juce::Array<juce::var>> bins;
                 for (auto t : session.getTracks())
                     for (auto c : t->getClips()) bins[c->state.getProperty (binId).toString()].add (t->getName() + " > " + c->getName());
                 auto o = obj();
                 for (auto& [k, v] : bins) set (o, k.isEmpty() ? juce::Identifier ("(unfiled)") : juce::Identifier (k.replaceCharacters (" .", "__")), juce::var (v));
                 juce::Array<juce::var> names; for (auto& [k, v] : bins) if (k.isNotEmpty()) names.add (k);
                 auto out = obj(); set (out, "bins", names); set (out, "contents", o);
                 return ToolResult::ok (out);
             }
             if (act == "rename")
             {
                 auto from = str (a, "bin"), to = str (a, "to").trim();
                 if (to.isEmpty()) return ToolResult::error ("Give the new name (to).");
                 int n = 0;
                 for (auto t : session.getTracks()) for (auto c : t->getClips()) if (c->state.getProperty (binId).toString() == from) { c->state.setProperty (binId, to, um()); ++n; }
                 return n > 0 ? ToolResult::ok ("Renamed bin '" + from + "' to '" + to + "' (" + juce::String (n) + " clips).") : ToolResult::error ("No bin named '" + from + "'.");
             }
             if (act != "file") return ToolResult::error ("Unknown action");
             auto bin = str (a, "bin").trim();
             if (bin.equalsIgnoreCase ("none") || bin.equalsIgnoreCase ("unfiled")) bin = {};
             juce::Array<juce::var> refs;
             if (auto arr = a["clips"].getArray()) refs = *arr; else refs.add (a);
             int n = 0;
             for (auto& r : refs)
             {
                 juce::String err;
                 auto t = trackArg (r, err);
                 if (t == nullptr) return ToolResult::error (err);
                 auto c = clipArg (*t, r, err);
                 if (c == nullptr) return ToolResult::error (err);
                 if (bin.isEmpty()) c->state.removeProperty (binId, um()); else c->state.setProperty (binId, bin, um());
                 ++n;
             }
             return ToolResult::ok (juce::String (n) + " clip" + (n == 1 ? "" : "s") + (bin.isEmpty() ? " unfiled." : " filed in '" + bin + "'."));
         });
}
