// Squelchmoth 1.14 - Claude's `mirrorwing` tool: the flagship synth by name. Included inside the McpTools class body (after McpToolsDuet.h).

juce::var describeMirrorwing (SqMirrorwingPlugin& m)
{
    using P = SqMirrorwingPlugin; auto& bk = m.theBank();
    juce::var o (new juce::DynamicObject());
    set (o, "plugin", "Mirrorwing");
    juce::Array<juce::var> oscs;
    for (int k = 0; k < P::numOsc; ++k)
    {
        juce::var v (new juce::DynamicObject());
        set (v, "osc", k + 1); set (v, "on", m.oscOn[(size_t) k].get()); set (v, "shape_name", SqMote::shapeName (bk[(size_t) m.po (k, P::oShape)]));
        set (v, "route", P::routeNames()[juce::jlimit (0, 2, m.oscRoute[(size_t) k].get())]); set (v, "unison", m.oscUnison[(size_t) k].get());
        set (v, "sync_to_osc1", k > 0 && m.oscSync[(size_t) k].get()); set (v, "free_phase", m.oscFree[(size_t) k].get());
        for (int p = 0; p < P::perOsc; ++p) set (v, juce::String (bk.specs[(size_t) m.po (k, p)].id).substring (2), bk[(size_t) m.po (k, p)]);
        oscs.add (v);
    }
    set (o, "oscillators", oscs);
    juce::Array<juce::var> fs;
    for (int f = 0; f < P::numF; ++f)
    {
        juce::var v (new juce::DynamicObject());
        set (v, "filter", f + 1); set (v, "on", m.fOn[(size_t) f].get()); set (v, "type", SqFissure::typeName (m.fType[(size_t) f].get())); set (v, "slope_db_oct", m.fSlope[(size_t) f].get());
        set (v, "cutoff_hz", bk[(size_t) m.pf (f, P::fCut)]); set (v, "resonance_pct", bk[(size_t) m.pf (f, P::fRes)]); set (v, "drive_db", bk[(size_t) m.pf (f, P::fDrive)]);
        set (v, "gain_db", bk[(size_t) m.pf (f, P::fGain)]); set (v, "key_track_pct", bk[(size_t) m.pf (f, P::fKey)]);
        fs.add (v);
    }
    set (o, "filters", fs); set (o, "filter_routing", m.parallel.get() ? "parallel" : "series");
    juce::Array<juce::var> es;
    for (int e = 0; e < P::numEnv; ++e)
    {
        juce::var v (new juce::DynamicObject()); set (v, "env", e + 1);
        set (v, "delay_ms", bk[(size_t) m.pe (e, P::eDelay)]); set (v, "attack_ms", bk[(size_t) m.pe (e, P::eAttack)]); set (v, "hold_ms", bk[(size_t) m.pe (e, P::eHold)]);
        set (v, "decay_ms", bk[(size_t) m.pe (e, P::eDecay)]); set (v, "sustain_pct", bk[(size_t) m.pe (e, P::eSustain)]); set (v, "release_ms", bk[(size_t) m.pe (e, P::eRelease)]);
        es.add (v);
    }
    set (o, "envelopes", es);
    juce::Array<juce::var> ls;
    for (int l = 0; l < P::numLfo; ++l)
    {
        juce::var v (new juce::DynamicObject()); set (v, "lfo", l + 1);
        set (v, "shape", SqMirror::lfoShapeNames()[juce::jlimit (0, SqMirror::numLfoShapes - 1, m.lfoShape[(size_t) l].get())]); set (v, "sync", P::syncNames()[juce::jlimit (0, 11, m.lfoSync[(size_t) l].get())]);
        set (v, "rate_hz", m.lfoHz (l)); set (v, "retrigger", m.lfoRetrig[(size_t) l].get()); set (v, "phase_deg", bk[(size_t) m.pl (l, P::lPhase)]); set (v, "fade_ms", bk[(size_t) m.pl (l, P::lFade)]);
        ls.add (v);
    }
    set (o, "lfos", ls);
    juce::Array<juce::var> ms;
    for (int s = 0; s < P::numSlots; ++s)
    {
        const int src = m.modSrc[(size_t) s].get(), t = m.modDst[(size_t) s].get(); if (src == 0 && t == 0) continue;
        juce::var v (new juce::DynamicObject()); const double a = bk[(size_t) (P::modBase + s)];
        set (v, "slot", s + 1); set (v, "source", SqMirror::sourceNames()[juce::jlimit (0, SqMirror::numSources - 1, src)]); set (v, "target", SqMirror::targetName (t));
        set (v, "amount", a); set (v, "amount_in_units", a * SqMirror::targetScale (t)); set (v, "units", juce::String (SqMirror::targetUnit (t)).isEmpty() ? juce::String ("fraction") : juce::String (SqMirror::targetUnit (t)));
        ms.add (v);
    }
    set (o, "matrix", ms);
    juce::var g (new juce::DynamicObject());
    for (int p : { P::noise, P::noiseColour, P::glide, P::ampVel, P::fxChorus, P::fxDelay, P::fxDelayFb, P::fxReverb, P::fxSize, P::width, P::level, P::macroBase, P::macroBase + 1, P::macroBase + 2, P::macroBase + 3 })
        set (g, bk.specs[(size_t) p].id, bk[(size_t) p]);
    set (g, "delay_time", P::delayNames()[juce::jlimit (0, 5, m.delayTime.get())]);
    set (o, "global", g);
    set (o, "voice", SqSynth::polyModeNames()[juce::jlimit (0, 2, m.polyModeValue().get())]); set (o, "glide_mode", SqSynth::glideModeNames()[juce::jlimit (0, 2, m.glideModeValue().get())]);
    set (o, "quality", m.qualityValue().get() == 1 ? "HD" : "Eco"); set (o, "voices_sounding", m.activeVoiceCount());
    set (o, "output_peak_db", juce::Decibels::gainToDecibels (m.outPeak.load(), -100.0f));
    return o;
}

void registerMirrorwingTools()
{
    add ("mirrorwing",
         "Mirrorwing (core instrument), the flagship synth: 4 oscillators (morphing shape sine > triangle > saw > square, width, wavefold, octave / semi / fine, level, pan, "
         "unison 1-8 with detune and spread, FM from the oscillator before it, hard sync to osc 1, phase, route to filter 1 / 2 / both), noise, 2 filters (7 types, 12 / 24 / 48 dB/oct, "
         "series or parallel), 4 DAHDSR envelopes (env 1 = amp), 4 LFOs, a 16-slot matrix, 4 macros, chorus / delay / reverb. action: add (on an instrument track; optional preset), get, "
         "set, osc, filter, env, lfo, mod, preset (name or 'list'), open.\n"
         " set: 'params' object of any parameter id or name with its value in units (e.g. {\"o1shape\":2.5,\"f1cut\":800,\"e2dec\":300,\"macro1\":50,\"fxReverb\":30}); and/or voice (poly | mono | legato), "
         "glide_mode (off | always | legato), quality (eco | hd), filter_routing (series | parallel), delay_time (1/16 | 1/8 | 1/8 dotted | 1/4 | 1/4 dotted | 1/2).\n"
         " osc: osc (1-4) with any of on, shape (0..3), width (%), fold (%), octave, semi, fine (ct), level (%), pan (-100..100), unison (1-8), detune (ct), spread (%), fm (%), phase (deg), "
         "sync (bool), free_phase (bool), route (filter 1 | filter 2 | both).\n"
         " filter: filter (1 | 2) with on, type, slope, cutoff (Hz), resonance (%), drive (dB), gain (dB), key_track (%).\n"
         " env: env (1-4) with delay, attack, hold, decay, release (ms), sustain (%). lfo: lfo (1-4) with shape (sine | triangle | saw | square | s&h | smooth), rate (Hz), sync, retrigger (bool), phase (deg), fade (ms).\n"
         " mod: slot (1-16; omit to use the first free one), source (env 1-4 | lfo 1-4 | velocity | key | mod wheel | aftertouch | random | macro 1-4 | off), target (e.g. 'filter 1 cutoff', 'osc 2 fm', "
         "'pitch (all)', 'amp', 'noise', 'none'), amount (-1..1) or amount_units (in the target's units: st, oct, dB).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","osc","filter","env","lfo","mod","preset","open"]},
            "params":{"type":"object"},"voice":{"type":"string"},"glide_mode":{"type":"string"},"quality":{"type":"string"},"filter_routing":{"type":"string"},"delay_time":{"type":"string"},
            "osc":{"type":"integer"},"filter":{"type":"integer"},"env":{"type":"integer"},"lfo":{"type":"integer"},"slot":{"type":"integer"},
            "on":{"type":"boolean"},"shape":{"type":["number","string"]},"width":{"type":"number"},"fold":{"type":"number"},"octave":{"type":"number"},"semi":{"type":"number"},"fine":{"type":"number"},
            "level":{"type":"number"},"pan":{"type":"number"},"unison":{"type":"integer"},"detune":{"type":"number"},"spread":{"type":"number"},"fm":{"type":"number"},"phase":{"type":"number"},
            "sync":{"type":["boolean","string"]},"free_phase":{"type":"boolean"},"route":{"type":"string"},"type":{"type":"string"},"slope":{"type":"integer"},"cutoff":{"type":"number"},
            "resonance":{"type":"number"},"drive":{"type":"number"},"gain":{"type":"number"},"key_track":{"type":"number"},"delay":{"type":"number"},"attack":{"type":"number"},
            "hold":{"type":"number"},"decay":{"type":"number"},"sustain":{"type":"number"},"release":{"type":"number"},"rate":{"type":"number"},"retrigger":{"type":"boolean"},
            "fade":{"type":"number"},"source":{"type":"string"},"target":{"type":"string"},"amount":{"type":"number"},"amount_units":{"type":"number"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqMirrorwingPlugin;
             return smallTool<P> (a, "Mirrorwing", "Mirrorwing", [this] (P& m) { return describeMirrorwing (m); }, [this, &a] (P& m, const juce::String& action) -> ToolResult
             {
                 auto* um = &session.edit->getUndoManager(); auto& bk = m.theBank();
                 auto setP = [&] (int id, double x) { auto& p = *bk.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
                 auto idx = [&] (const char* key, int n) { const int i = (int) num (a, key) - 1; return i >= 0 && i < n ? i : -1; };
                 if (action == "set")
                 {
                     if (auto* obj = a["params"].getDynamicObject())
                         for (auto& pr : obj->getProperties())
                         {
                             const auto key = pr.name.toString(); int found = -1;
                             for (size_t i = 0; i < bk.specs.size(); ++i) if (key.equalsIgnoreCase (bk.specs[i].id) || key.equalsIgnoreCase (bk.specs[i].name)) found = (int) i;
                             if (found < 0) return ToolResult::error ("Unknown Mirrorwing parameter '" + key + "'");
                             setP (found, (double) pr.value);
                         }
                     if (has (a, "voice")) { const auto v = str (a, "voice").toLowerCase(); m.polyModeValue().setValue (v.startsWith ("mono") ? 1 : v.startsWith ("leg") ? 2 : 0, um); }
                     if (has (a, "glide_mode")) { const auto v = str (a, "glide_mode").toLowerCase(); m.glideModeValue().setValue (v.startsWith ("al") ? 1 : v.startsWith ("leg") ? 2 : 0, um); }
                     if (has (a, "quality")) m.qualityValue().setValue (str (a, "quality").toLowerCase().startsWith ("hd") ? 1 : 0, um);
                     if (has (a, "filter_routing")) m.parallel.setValue (str (a, "filter_routing").toLowerCase().startsWith ("par"), um);
                     if (has (a, "delay_time")) { const int i = P::delayNames().indexOf (str (a, "delay_time"), true); if (i < 0) return ToolResult::error ("delay_time: " + P::delayNames().joinIntoString (", ")); m.delayTime.setValue (i, um); }
                     return ToolResult::ok (juce::String());
                 }
                 if (action == "osc")
                 {
                     const int k = idx ("osc", P::numOsc); if (k < 0) return ToolResult::error ("osc must be 1-4");
                     if (has (a, "on")) m.oscOn[(size_t) k].setValue ((bool) a["on"], um);
                     if (has (a, "shape"))
                     {
                         const auto sv = a["shape"]; double v = (double) sv;
                         if (sv.isString()) { const auto t = sv.toString().toLowerCase(); v = t.startsWith ("sin") ? 0 : t.startsWith ("tri") ? 1 : t.startsWith ("saw") ? 2 : t.startsWith ("sq") || t.startsWith ("pul") ? 3 : sv.toString().getDoubleValue(); }
                         setP (m.po (k, P::oShape), v);
                     }
                     for (auto [nm, p] : { std::pair<const char*, int> { "width", P::oWidth }, { "fold", P::oFold }, { "octave", P::oOct }, { "semi", P::oSemi }, { "fine", P::oFine }, { "level", P::oLevel },
                                           { "pan", P::oPan }, { "detune", P::oUniDetune }, { "spread", P::oUniSpread }, { "fm", P::oFm }, { "phase", P::oPhase } })
                         if (has (a, nm)) setP (m.po (k, p), num (a, nm));
                     if (has (a, "unison")) m.oscUnison[(size_t) k].setValue (juce::jlimit (1, 8, (int) num (a, "unison")), um);
                     if (has (a, "sync")) { if (k == 0) return ToolResult::error ("osc 1 is the sync master; sync applies to osc 2-4"); m.oscSync[(size_t) k].setValue ((bool) a["sync"], um); }
                     if (has (a, "free_phase")) m.oscFree[(size_t) k].setValue ((bool) a["free_phase"], um);
                     if (has (a, "route")) { const auto v = str (a, "route").toLowerCase(); m.oscRoute[(size_t) k].setValue (v.contains ("both") ? 2 : v.contains ("2") ? 1 : 0, um); }
                     if (! has (a, "on")) m.oscOn[(size_t) k].setValue (true, um);
                     return ToolResult::ok (juce::String());
                 }
                 if (action == "filter")
                 {
                     const int f = idx ("filter", P::numF); if (f < 0) return ToolResult::error ("filter must be 1 or 2");
                     if (has (a, "on")) m.fOn[(size_t) f].setValue ((bool) a["on"], um);
                     if (has (a, "type")) { const int t = smallFilterType (str (a, "type")); if (t < 0) return ToolResult::error ("type: low-pass, high-pass, band-pass, notch, peak, comb, vowel"); m.fType[(size_t) f].setValue (t, um); if (! has (a, "on")) m.fOn[(size_t) f].setValue (true, um); }
                     if (has (a, "slope")) { const int sl = (int) num (a, "slope"); m.fSlope[(size_t) f].setValue (sl >= 48 ? 48 : sl >= 24 ? 24 : 12, um); }
                     for (auto [nm, p] : { std::pair<const char*, int> { "cutoff", P::fCut }, { "resonance", P::fRes }, { "drive", P::fDrive }, { "gain", P::fGain }, { "key_track", P::fKey } })
                         if (has (a, nm)) setP (m.pf (f, p), num (a, nm));
                     return ToolResult::ok (juce::String());
                 }
                 if (action == "env")
                 {
                     const int e = idx ("env", P::numEnv); if (e < 0) return ToolResult::error ("env must be 1-4");
                     for (auto [nm, p] : { std::pair<const char*, int> { "delay", P::eDelay }, { "attack", P::eAttack }, { "hold", P::eHold }, { "decay", P::eDecay }, { "sustain", P::eSustain }, { "release", P::eRelease } })
                         if (has (a, nm)) setP (m.pe (e, p), num (a, nm));
                     return ToolResult::ok (juce::String());
                 }
                 if (action == "lfo")
                 {
                     const int l = idx ("lfo", P::numLfo); if (l < 0) return ToolResult::error ("lfo must be 1-4");
                     if (has (a, "shape")) { const int i = SqMirror::lfoShapeNames().indexOf (str (a, "shape"), true); if (i < 0) return ToolResult::error ("shape: " + SqMirror::lfoShapeNames().joinIntoString (", ")); m.lfoShape[(size_t) l].setValue (i, um); }
                     if (has (a, "rate")) { setP (m.pl (l, P::lRate), num (a, "rate")); m.lfoSync[(size_t) l].setValue (0, um); }
                     if (has (a, "sync")) { const int i = smallFilterSync (str (a, "sync")); if (i < 0) return ToolResult::error ("sync: " + P::syncNames().joinIntoString (", ")); m.lfoSync[(size_t) l].setValue (i, um); }
                     if (has (a, "retrigger")) m.lfoRetrig[(size_t) l].setValue ((bool) a["retrigger"], um);
                     if (has (a, "phase")) setP (m.pl (l, P::lPhase), num (a, "phase")); if (has (a, "fade")) setP (m.pl (l, P::lFade), num (a, "fade"));
                     return ToolResult::ok (juce::String());
                 }
                 if (action == "mod")
                 {
                     int src = -1, t = -1;
                     if (has (a, "source")) { const auto v = str (a, "source").toLowerCase().removeCharacters (" "); const auto n = SqMirror::sourceNames(); for (int i = 0; i < n.size(); ++i) if (n[i].toLowerCase().removeCharacters (" ") == v) src = i; if (src < 0) return ToolResult::error ("source: " + n.joinIntoString (", ")); }
                     if (has (a, "target")) { const auto v = str (a, "target").toLowerCase().trim(); for (int i = 0; i < SqMirror::numTargets; ++i) if (SqMirror::targetName (i).toLowerCase() == v || (v == "pitch" && i == SqMirror::tGlobalPitch)) t = i; if (t < 0) return ToolResult::error ("target: e.g. 'filter 1 cutoff', 'osc 2 fm', 'pitch (all)', 'amp', 'noise' or 'none'"); }
                     int s = has (a, "slot") ? idx ("slot", P::numSlots) : -1;
                     if (has (a, "slot") && s < 0) return ToolResult::error ("slot must be 1-16");
                     if (s < 0) for (int i = 0; i < P::numSlots && s < 0; ++i) if (src >= 0 && t >= 0 && m.modSrc[(size_t) i].get() == src && m.modDst[(size_t) i].get() == t) s = i;
                     if (s < 0) for (int i = 0; i < P::numSlots && s < 0; ++i) if (m.modSrc[(size_t) i].get() == 0 || m.modDst[(size_t) i].get() == 0) s = i;
                     if (s < 0) return ToolResult::error ("All 16 matrix slots are in use - give a slot number to replace one");
                     if (src >= 0) m.modSrc[(size_t) s].setValue (src, um); if (t >= 0) m.modDst[(size_t) s].setValue (t, um);
                     if (has (a, "amount")) setP (P::modBase + s, num (a, "amount"));
                     if (has (a, "amount_units")) setP (P::modBase + s, num (a, "amount_units") / SqMirror::targetScale (m.modDst[(size_t) s].get()));
                     return ToolResult::ok (juce::String());
                 }
                 return ToolResult::error ("Unknown action '" + action + "'");
             });
         });
}
