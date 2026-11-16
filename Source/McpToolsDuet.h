// Squelchmoth 1.14 - Claude's `duet_filter`, `speck_filter` and `mote` tools (phase K small tools). Included inside the McpTools class body.

/** Parses a filter type name ("low-pass", "lp", "band pass", "vowel" ...); -1 if unknown. */
static int smallFilterType (const juce::String& raw)
{
    const auto v = raw.removeCharacters ("- ").toLowerCase();
    if (v == "lp" || v == "lowpass") return SqFissure::lowPass; if (v == "hp" || v == "highpass") return SqFissure::highPass; if (v == "bp" || v == "bandpass") return SqFissure::bandPass;
    for (int i = 0; i < SqFissure::numTypes; ++i) if (juce::String (SqFissure::typeName (i)).removeCharacters ("- ").toLowerCase().startsWith (v.substring (0, 4))) return i;
    return -1;
}
static int smallFilterSync (const juce::String& raw) { const auto v = raw.trim(); return v.equalsIgnoreCase ("free") ? 0 : SqSmallFilterCommon::syncNames().indexOf (v, true); }

juce::var describeDuet (SqDuetFilterPlugin& f)
{
    using P = SqDuetFilterPlugin;
    juce::var o (new juce::DynamicObject()); const auto e = f.snapshot();
    set (o, "plugin", "Duet Filter"); set (o, "routing", P::routingNames()[juce::jlimit (0, 3, f.routing.get())]);
    set (o, "link", f.link.get()); set (o, "link_interval_oct", f.bank[P::linkOct]);
    juce::Array<juce::var> fs;
    for (int k = 0; k < 2; ++k)
    {
        juce::var v (new juce::DynamicObject()); const auto s = e.drawnSlot (k);
        set (v, "filter", k + 1); set (v, "on", s.on); set (v, "type", SqFissure::typeName (s.type)); set (v, "slope_db_oct", s.slope);
        set (v, "cutoff_hz", s.cutoffHz); set (v, "resonance_pct", s.resonance * 100.0); set (v, "drive_db", s.driveDb); set (v, "gain_db", s.gainDb); set (v, "mix_pct", s.mix * 100.0);
        set (v, "lfo_depth_oct", f.fp (k, P::lfoDepth)); set (v, "env_depth_oct", f.fp (k, P::envDepth));
        set (v, "level_at_cutoff_db", SqFissure::Engine::responseDb (s, f.sampleRate(), s.cutoffHz));
        set (v, "cutoff_now_left_hz", f.cutNow[(size_t) (k * 2)].load()); set (v, "cutoff_now_right_hz", f.cutNow[(size_t) (k * 2 + 1)].load());
        fs.add (v);
    }
    set (o, "filters", fs);
    set (o, "lfo_shape", SqSmallFilterCommon::lfoNames()[juce::jlimit (0, 4, f.lfoShape.get())]); set (o, "lfo_sync", SqSmallFilterCommon::syncNames()[juce::jlimit (0, 11, f.lfoSync.get())]);
    set (o, "lfo_rate_hz", f.lfoHz()); set (o, "lfo_stereo_phase_deg", f.bank[P::lfoPhase]);
    set (o, "env_source", f.envKey.get() ? "sidechain" : "input"); set (o, "env_attack_ms", f.bank[P::envAttack]); set (o, "env_release_ms", f.bank[P::envRelease]); set (o, "env_sensitivity_db", f.bank[P::envGain]);
    set (o, "env_level_db", f.envDb.load()); set (o, "mix_pct", f.bank[P::mix]); set (o, "output_db", f.bank[P::output]);
    set (o, "input_peak_db", juce::Decibels::gainToDecibels (f.levelIn.load(), -100.0f)); set (o, "output_peak_db", juce::Decibels::gainToDecibels (f.levelOut.load(), -100.0f));
    return o;
}
juce::var describeSpeck (SqSpeckFilterPlugin& f)
{
    using P = SqSpeckFilterPlugin;
    juce::var o (new juce::DynamicObject()); const auto s = f.slotParams();
    set (o, "plugin", "Speck Filter"); set (o, "type", SqFissure::typeName (s.type)); set (o, "slope_db_oct", s.slope);
    set (o, "cutoff_hz", s.cutoffHz); set (o, "resonance_pct", s.resonance * 100.0); set (o, "drive_db", s.driveDb); set (o, "gain_db", s.gainDb);
    set (o, "level_at_cutoff_db", SqFissure::Engine::responseDb (s, f.sampleRate(), s.cutoffHz));
    set (o, "cutoff_now_left_hz", f.cutNow[0].load()); set (o, "cutoff_now_right_hz", f.cutNow[1].load());
    set (o, "env_source", f.envKey.get() ? "sidechain" : "input"); set (o, "env_depth_oct", f.bank[P::envDepth]); set (o, "env_attack_ms", f.bank[P::envAttack]);
    set (o, "env_release_ms", f.bank[P::envRelease]); set (o, "env_sensitivity_db", f.bank[P::envGain]); set (o, "env_level_db", f.envDb.load());
    set (o, "lfo_shape", SqSmallFilterCommon::lfoNames()[juce::jlimit (0, 4, f.lfoShape.get())]); set (o, "lfo_sync", SqSmallFilterCommon::syncNames()[juce::jlimit (0, 11, f.lfoSync.get())]);
    set (o, "lfo_rate_hz", f.lfoHz()); set (o, "lfo_depth_oct", f.bank[P::lfoDepth]); set (o, "lfo_stereo_phase_deg", f.bank[P::lfoPhase]);
    set (o, "mix_pct", f.bank[P::mix]); set (o, "output_db", f.bank[P::output]);
    set (o, "input_peak_db", juce::Decibels::gainToDecibels (f.levelIn.load(), -100.0f)); set (o, "output_peak_db", juce::Decibels::gainToDecibels (f.levelOut.load(), -100.0f));
    return o;
}

/** Shared: find / add the plug-in, presets. */
template <typename T>
ToolResult smallTool (const juce::var& a, const char* displayName, const char* undoName, std::function<juce::var (T&)> describe, std::function<ToolResult (T&, const juce::String&)> act)
{
    const auto action = str (a, "action").toLowerCase();
    juce::String err; te::AudioTrack* tr = nullptr;
    auto list = pluginListArg (a, err, tr);
    if (list == nullptr) return ToolResult::error (err);
    T* f = nullptr;
    for (auto pl : *list) if (auto c = dynamic_cast<T*> (pl)) { f = c; break; }
    auto loadPreset = [&] (const juce::String& want) -> juce::String
    {
        for (auto& e : Signature::entries()) if (juce::String (e.type) == T::xmlTypeName)
        {
            juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
            for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction (juce::String (undoName) + " preset"); Signature::applyPreset (*f, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
            return "Presets: " + names.joinIntoString (", ");
        }
        return "No presets";
    };
    if (action == "add")
    {
        if (f == nullptr)
        {
            juce::String why; auto np = session.createPluginByName (displayName, why);
            if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create ") + displayName);
            list->insertPlugin (np, np->isSynth() && tr != nullptr ? 0 : (tr != nullptr ? Session::insertEndIndex (*list, tr) : -1), nullptr);
            f = dynamic_cast<T*> (np.get());
            if (f == nullptr) return ToolResult::error (juce::String ("Couldn't create ") + displayName);
        }
        if (has (a, "preset")) loadPreset (str (a, "preset"));
        if (ui.refresh) ui.refresh();
        return ToolResult::ok (juce::JSON::toString (describe (*f)));
    }
    if (f == nullptr) return ToolResult::error (juce::String ("No ") + displayName + " on that track - use action add");
    if (action == "get") return ToolResult::ok (juce::JSON::toString (describe (*f)));
    if (action == "open") { f->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
    if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
    session.edit->getUndoManager().beginNewTransaction (juce::String (undoName) + " settings");
    auto r = act (*f, action);
    if (r.isError) return r;
    return ToolResult::ok (juce::JSON::toString (describe (*f)));
}

void registerDuetTools()
{
    add ("duet_filter",
         "Duet Filter (core): two filters (low-pass, high-pass, band-pass, notch, peak, comb, vowel; 12 / 24 / 48 dB/oct) routed serial, parallel, stereo (1 left, 2 right) "
         "or mid / side; link (filter 2 follows filter 1 at an interval in octaves); one LFO with a depth per filter and a stereo phase; an envelope follower (input or "
         "sidechain) with a depth per filter. action: add (optional preset), get, set, filter, preset (name or 'list'), open.\n"
         " set: routing (serial | parallel | stereo | mid side), link (bool), link_interval (oct, -4..4), lfo_shape (sine | triangle | saw | square | random), lfo_rate (Hz), "
         "lfo_sync (free | 4 bars | 2 bars | 1 bar | 1/2 | 1/4 | 1/8 | 1/16 | 1/4 dotted | 1/8 dotted | 1/4 triplet | 1/8 triplet), lfo_phase (deg 0..180), env_source (input | sidechain), "
         "env_attack / env_release (ms), env_sensitivity (dB), mix (%), output (dB).\n"
         " filter: filter (1 | 2) with any of on, type, slope, cutoff (Hz), resonance (%), drive (dB), gain (dB, peak), filter_mix (%), lfo_depth (oct, -4..4), env_depth (oct, -6..6).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","filter","preset","open"]},
            "routing":{"type":"string"},"link":{"type":"boolean"},"link_interval":{"type":"number"},"lfo_shape":{"type":"string"},"lfo_rate":{"type":"number"},"lfo_sync":{"type":"string"},
            "lfo_phase":{"type":"number"},"env_source":{"type":"string"},"env_attack":{"type":"number"},"env_release":{"type":"number"},"env_sensitivity":{"type":"number"},
            "mix":{"type":"number"},"output":{"type":"number"},"filter":{"type":"integer"},"on":{"type":"boolean"},"type":{"type":"string"},"slope":{"type":"integer"},
            "cutoff":{"type":"number"},"resonance":{"type":"number"},"drive":{"type":"number"},"gain":{"type":"number"},"filter_mix":{"type":"number"},
            "lfo_depth":{"type":"number"},"env_depth":{"type":"number"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqDuetFilterPlugin;
             return smallTool<P> (a, "Duet Filter", "Duet", [this] (P& f) { return describeDuet (f); }, [this, &a] (P& f, const juce::String& action) -> ToolResult
             {
                 auto* um = &session.edit->getUndoManager();
                 auto setP = [&] (int id, double x) { auto& p = *f.bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
                 if (action == "set")
                 {
                     if (has (a, "routing")) { const auto v = str (a, "routing").toLowerCase(); f.routing.setValue (v.startsWith ("par") ? 1 : v.startsWith ("st") ? 2 : (v.contains ("mid") || v.contains ("side")) ? 3 : 0, um); }
                     if (has (a, "link")) f.link.setValue ((bool) a["link"], um);
                     if (has (a, "link_interval")) { setP (P::linkOct, num (a, "link_interval")); if (! has (a, "link")) f.link.setValue (true, um); }
                     if (has (a, "lfo_shape")) { const int i = SqSmallFilterCommon::lfoNames().indexOf (str (a, "lfo_shape"), true); if (i < 0) return ToolResult::error ("lfo_shape: sine, triangle, saw, square, random"); f.lfoShape.setValue (i, um); }
                     if (has (a, "lfo_rate")) { setP (P::lfoRate, num (a, "lfo_rate")); f.lfoSync.setValue (0, um); }
                     if (has (a, "lfo_sync")) { const int i = smallFilterSync (str (a, "lfo_sync")); if (i < 0) return ToolResult::error ("lfo_sync: " + SqSmallFilterCommon::syncNames().joinIntoString (", ")); f.lfoSync.setValue (i, um); }
                     if (has (a, "lfo_phase")) setP (P::lfoPhase, num (a, "lfo_phase"));
                     if (has (a, "env_source")) f.envKey.setValue (str (a, "env_source").toLowerCase().startsWith ("side") || str (a, "env_source").toLowerCase().startsWith ("key"), um);
                     if (has (a, "env_attack")) setP (P::envAttack, num (a, "env_attack")); if (has (a, "env_release")) setP (P::envRelease, num (a, "env_release"));
                     if (has (a, "env_sensitivity")) setP (P::envGain, num (a, "env_sensitivity"));
                     if (has (a, "mix")) setP (P::mix, num (a, "mix")); if (has (a, "output")) setP (P::output, num (a, "output"));
                     return ToolResult::ok (juce::String());
                 }
                 if (action == "filter")
                 {
                     const int k = (int) num (a, "filter") - 1; if (k < 0 || k > 1) return ToolResult::error ("filter must be 1 or 2");
                     if (has (a, "on")) f.on[(size_t) k].setValue ((bool) a["on"], um);
                     if (has (a, "type")) { const int t = smallFilterType (str (a, "type")); if (t < 0) return ToolResult::error ("type: low-pass, high-pass, band-pass, notch, peak, comb, vowel"); f.type[(size_t) k].setValue (t, um); }
                     if (has (a, "slope")) { const int sl = (int) num (a, "slope"); f.slope[(size_t) k].setValue (sl >= 48 ? 48 : sl >= 24 ? 24 : 12, um); }
                     for (auto [nm, sub] : { std::pair<const char*, int> { "cutoff", P::cutoff }, { "resonance", P::res }, { "drive", P::drive }, { "gain", P::gain }, { "filter_mix", P::fMix }, { "lfo_depth", P::lfoDepth }, { "env_depth", P::envDepth } })
                         if (has (a, nm)) setP (f.paramOf (k, sub), num (a, nm));
                     return ToolResult::ok (juce::String());
                 }
                 return ToolResult::error ("Unknown action '" + action + "'");
             });
         });

    add ("speck_filter",
         "Speck Filter (core): one filter (low-pass, high-pass, band-pass, notch, peak, comb, vowel; 12 / 24 / 48 dB/oct) with an envelope follower (input or sidechain) and an LFO "
         "(stereo phase for left / right movement). action: add (optional preset), get, set, preset (name or 'list'), open.\n"
         " set: any of type, slope, cutoff (Hz), resonance (%), drive (dB), gain (dB, peak), env_depth (oct, -6..6), env_source (input | sidechain), env_attack / env_release (ms), "
         "env_sensitivity (dB), lfo_depth (oct, -4..4), lfo_shape, lfo_rate (Hz), lfo_sync, lfo_phase (deg), mix (%), output (dB).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","preset","open"]},
            "type":{"type":"string"},"slope":{"type":"integer"},"cutoff":{"type":"number"},"resonance":{"type":"number"},"drive":{"type":"number"},"gain":{"type":"number"},
            "env_depth":{"type":"number"},"env_source":{"type":"string"},"env_attack":{"type":"number"},"env_release":{"type":"number"},"env_sensitivity":{"type":"number"},
            "lfo_depth":{"type":"number"},"lfo_shape":{"type":"string"},"lfo_rate":{"type":"number"},"lfo_sync":{"type":"string"},"lfo_phase":{"type":"number"},
            "mix":{"type":"number"},"output":{"type":"number"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqSpeckFilterPlugin;
             return smallTool<P> (a, "Speck Filter", "Speck", [this] (P& f) { return describeSpeck (f); }, [this, &a] (P& f, const juce::String& action) -> ToolResult
             {
                 if (action != "set") return ToolResult::error ("Unknown action '" + action + "'");
                 auto* um = &session.edit->getUndoManager();
                 auto setP = [&] (int id, double x) { auto& p = *f.bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
                 if (has (a, "type")) { const int t = smallFilterType (str (a, "type")); if (t < 0) return ToolResult::error ("type: low-pass, high-pass, band-pass, notch, peak, comb, vowel"); f.type.setValue (t, um); }
                 if (has (a, "slope")) { const int sl = (int) num (a, "slope"); f.slope.setValue (sl >= 48 ? 48 : sl >= 24 ? 24 : 12, um); }
                 if (has (a, "lfo_shape")) { const int i = SqSmallFilterCommon::lfoNames().indexOf (str (a, "lfo_shape"), true); if (i < 0) return ToolResult::error ("lfo_shape: sine, triangle, saw, square, random"); f.lfoShape.setValue (i, um); }
                 if (has (a, "lfo_rate")) { setP (P::lfoRate, num (a, "lfo_rate")); f.lfoSync.setValue (0, um); }
                 if (has (a, "lfo_sync")) { const int i = smallFilterSync (str (a, "lfo_sync")); if (i < 0) return ToolResult::error ("lfo_sync: " + SqSmallFilterCommon::syncNames().joinIntoString (", ")); f.lfoSync.setValue (i, um); }
                 if (has (a, "env_source")) f.envKey.setValue (str (a, "env_source").toLowerCase().startsWith ("side") || str (a, "env_source").toLowerCase().startsWith ("key"), um);
                 for (auto [nm, id] : { std::pair<const char*, int> { "cutoff", P::cutoff }, { "resonance", P::res }, { "drive", P::drive }, { "gain", P::gain }, { "env_depth", P::envDepth },
                                        { "env_attack", P::envAttack }, { "env_release", P::envRelease }, { "env_sensitivity", P::envGain }, { "lfo_depth", P::lfoDepth }, { "lfo_phase", P::lfoPhase },
                                        { "mix", P::mix }, { "output", P::output } })
                     if (has (a, nm)) setP (id, num (a, nm));
                 return ToolResult::ok (juce::String());
             });
         });

    add ("mote",
         "Mote (core instrument): the one-oscillator synth - one oscillator that sweeps sine > triangle > saw > square (width narrows the square to a pulse), a sub one or two "
         "octaves down, noise with colour, drive, a filter (low-pass, high-pass, band-pass, notch, comb, vowel; 12 / 24 / 48 dB/oct) with its own envelope, key track and velocity, "
         "an amp envelope, an LFO (to pitch, cutoff, shape, width, tremolo; with a fade-in delay), poly / mono / legato, glide, unison up to 8, Eco / HD, and an 8-slot matrix. "
         "action: add (on an instrument track; optional preset), get, set, mod, preset (name or 'list'), open.\n"
         " set: any parameter by its id or name with its value in units (shape 0..3, width %, octave, semi, fine ct, oscLevel %, subLevel %, noise %, noiseColour %, drive dB, "
         "cutoff Hz, res %, fEnvDepth oct, keyTrack %, velDepth oct, fA/fD/fR ms, fS %, aA/aD/aR ms, aS %, ampVel %, lfoRate Hz, lfoPitch st, lfoCut oct, lfoShapeAmt, lfoWidth, "
         "lfoAmp %, lfoDelay ms, glide ms, uniDetune ct, uniSpread %, level dB) given as a 'params' object, and/or filter_type, slope, sub_octave (1 | 2), sub_wave (sine | square), "
         "lfo_wave (sine | triangle | saw | square | s&h | drift), lfo_sync, voice (poly | mono | legato), glide_mode (off | always | legato), unison (1-8), quality (eco | hd).\n"
         " mod: slot (1-8), source (off | env 1 | env 2 | lfo 1 | velocity | key | mod wheel | aftertouch | mod x | mod y | random), target (pitch | cutoff | resonance | shape | width | amp | none), amount (-1..1).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","mod","preset","open"]},
            "params":{"type":"object"},"filter_type":{"type":"string"},"slope":{"type":"integer"},"sub_octave":{"type":"integer"},"sub_wave":{"type":"string"},"lfo_wave":{"type":"string"},
            "lfo_sync":{"type":"string"},"voice":{"type":"string"},"glide_mode":{"type":"string"},"unison":{"type":"integer"},"quality":{"type":"string"},
            "slot":{"type":"integer"},"source":{"type":"string"},"target":{"type":"string"},"amount":{"type":"number"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqMotePlugin;
             auto describe = [] (P& m) -> juce::var
             {
                 juce::var o (new juce::DynamicObject()); auto& bk = m.synthBank();
                 set (o, "plugin", "Mote");
                 juce::var ps (new juce::DynamicObject());
                 for (size_t i = 0; i < bk.specs.size(); ++i) set (ps, bk.specs[i].id, bk.params[i]->getCurrentValue());
                 set (o, "params", ps);
                 set (o, "filter_type", SqMote::filterTypeNames()[juce::jlimit (0, 5, m.filterType.get())]); set (o, "slope_db_oct", m.slope.get());
                 set (o, "sub_octave", m.subOct.get()); set (o, "sub_wave", m.subWave.get() == 0 ? "sine" : "square");
                 set (o, "lfo_wave", P::lfoNames()[juce::jlimit (0, 5, m.lfoWave.get())]); set (o, "lfo_sync", P::syncNames()[juce::jlimit (0, 11, m.lfoSync.get())]);
                 set (o, "voice", SqSynth::polyModeNames()[juce::jlimit (0, 2, m.polyModeValue().get())]); set (o, "glide_mode", SqSynth::glideModeNames()[juce::jlimit (0, 2, m.glideModeValue().get())]);
                 set (o, "unison", m.unisonValue().get()); set (o, "quality", m.qualityValue().get() == 1 ? "HD" : "Eco");
                 set (o, "shape_name", SqMote::shapeName (bk[P::shape]));
                 juce::Array<juce::var> mm;
                 for (int i = 0; i < SqSynth::numModSlots; ++i)
                 {
                     juce::var v (new juce::DynamicObject()); set (v, "slot", i + 1);
                     set (v, "source", SqSynth::modSourceNames()[juce::jlimit (0, (int) SqSynth::ModSource::numSources - 1, m.modSourceValue (i).get())]);
                     set (v, "target", m.modDestValue (i).get().isEmpty() ? juce::String ("none") : m.modDestValue (i).get()); set (v, "amount", m.modDepthValue (i).get());
                     mm.add (v);
                 }
                 set (o, "matrix", mm);
                 set (o, "voices_sounding", m.activeVoiceCount()); set (o, "output_peak_db", juce::Decibels::gainToDecibels (m.outPeak.load(), -100.0f));
                 return o;
             };
             return smallTool<P> (a, "Mote", "Mote", describe, [this, &a] (P& m, const juce::String& action) -> ToolResult
             {
                 auto* um = &session.edit->getUndoManager(); auto& bk = m.synthBank();
                 if (action == "set")
                 {
                     if (auto* obj = a["params"].getDynamicObject())
                         for (auto& pr : obj->getProperties())
                         {
                             const auto key = pr.name.toString(); int found = -1;
                             for (size_t i = 0; i < bk.specs.size(); ++i) if (key.equalsIgnoreCase (bk.specs[i].id) || key.equalsIgnoreCase (bk.specs[i].name)) found = (int) i;
                             if (found < 0) return ToolResult::error ("Unknown Mote parameter '" + key + "'");
                             auto& p = *bk.params[(size_t) found]; p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, (float) (double) pr.value), juce::sendNotification);
                         }
                     if (has (a, "filter_type")) { const int i = SqMote::filterTypeNames().indexOf (str (a, "filter_type"), true); const int t = i >= 0 ? i : smallFilterType (str (a, "filter_type"));
                                                   static const int back[] = { 0, 1, 2, 3, -1, 4, 5 }; const int mt = i >= 0 ? i : (t >= 0 ? back[t] : -1);
                                                   if (mt < 0) return ToolResult::error ("filter_type: low-pass, high-pass, band-pass, notch, comb, vowel"); m.filterType.setValue (mt, um); }
                     if (has (a, "slope")) { const int sl = (int) num (a, "slope"); m.slope.setValue (sl >= 48 ? 48 : sl >= 24 ? 24 : 12, um); }
                     if (has (a, "sub_octave")) m.subOct.setValue (juce::jlimit (1, 2, (int) num (a, "sub_octave")), um);
                     if (has (a, "sub_wave")) m.subWave.setValue (str (a, "sub_wave").toLowerCase().startsWith ("sq") ? 1 : 0, um);
                     if (has (a, "lfo_wave")) { const int i = P::lfoNames().indexOf (str (a, "lfo_wave"), true); if (i < 0) return ToolResult::error ("lfo_wave: " + P::lfoNames().joinIntoString (", ")); m.lfoWave.setValue (i, um); }
                     if (has (a, "lfo_sync")) { const int i = smallFilterSync (str (a, "lfo_sync")); if (i < 0) return ToolResult::error ("lfo_sync: " + P::syncNames().joinIntoString (", ")); m.lfoSync.setValue (i, um); }
                     if (has (a, "voice")) { const auto v = str (a, "voice").toLowerCase(); m.polyModeValue().setValue (v.startsWith ("mono") ? 1 : v.startsWith ("leg") ? 2 : 0, um); }
                     if (has (a, "glide_mode")) { const auto v = str (a, "glide_mode").toLowerCase(); m.glideModeValue().setValue (v.startsWith ("al") ? 1 : v.startsWith ("leg") ? 2 : 0, um); }
                     if (has (a, "unison")) m.unisonValue().setValue (juce::jlimit (1, 8, (int) num (a, "unison")), um);
                     if (has (a, "quality")) m.qualityValue().setValue (str (a, "quality").toLowerCase().startsWith ("hd") ? 1 : 0, um);
                     return ToolResult::ok (juce::String());
                 }
                 if (action == "mod")
                 {
                     const int s = (int) num (a, "slot") - 1; if (s < 0 || s >= SqSynth::numModSlots) return ToolResult::error ("slot must be 1-8");
                     if (has (a, "source"))
                     {
                         const auto v = str (a, "source").toLowerCase().removeCharacters (" "); int found = -1;
                         const auto names = SqSynth::modSourceNames(); for (int i = 0; i < names.size(); ++i) if (names[i].toLowerCase().removeCharacters (" ") == v) found = i;
                         if (found < 0) return ToolResult::error ("source: " + names.joinIntoString (", ")); m.modSourceValue (s).setValue (found, um);
                     }
                     if (has (a, "target")) { const auto v = str (a, "target").toLowerCase(); if (v != "none" && ! m.modDestinations().contains (v)) return ToolResult::error ("target: " + m.modDestinations().joinIntoString (", ") + " or none"); m.modDestValue (s).setValue (v == "none" ? juce::String() : v, um); }
                     if (has (a, "amount")) m.modDepthValue (s).setValue ((float) juce::jlimit (-1.0, 1.0, num (a, "amount")), um);
                     return ToolResult::ok (juce::String());
                 }
                 return ToolResult::error ("Unknown action '" + action + "'");
             });
         });
}
