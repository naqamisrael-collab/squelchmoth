// Squelchmoth 1.14 - Claude's `fissure_filter` tool: the filter flagship by name. Included inside the McpTools class body.

juce::var describeFissure (SqFissureFilterPlugin& f)
{
    using P = SqFissureFilterPlugin;
    juce::var o (new juce::DynamicObject());
    set (o, "plugin", "Fissure Filter"); set (o, "routing", P::routingNames()[juce::jlimit (0, 3, f.routing.get())]);
    juce::Array<juce::var> slots;
    for (int k = 0; k < P::numSlots; ++k)
    {
        juce::var v (new juce::DynamicObject()); const auto s = f.slotParams (k);
        set (v, "filter", k + 1); set (v, "on", s.on); set (v, "type", SqFissure::typeName (s.type)); set (v, "slope_db_oct", s.slope);
        set (v, "cutoff_hz", s.cutoffHz); set (v, "resonance_pct", s.resonance * 100.0); set (v, "drive_db", s.driveDb); set (v, "gain_db", s.gainDb);
        set (v, "pan_pct", s.pan * 100.0); set (v, "mix_pct", s.mix * 100.0); set (v, "cutoff_now_hz", f.cutNow[(size_t) k].load());
        slots.add (v);
    }
    set (o, "filters", slots);
    juce::var mod (new juce::DynamicObject());
    set (mod, "lfo1_shape", P::lfoNames()[juce::jlimit (0, 4, f.lfo1Shape.get())]); set (mod, "lfo1_sync", P::syncNames()[juce::jlimit (0, 11, f.lfo1Sync.get())]); set (mod, "lfo1_rate_hz", f.lfoRate (0));
    set (mod, "lfo2_shape", P::lfoNames()[juce::jlimit (0, 4, f.lfo2Shape.get())]); set (mod, "lfo2_sync", P::syncNames()[juce::jlimit (0, 11, f.lfo2Sync.get())]); set (mod, "lfo2_rate_hz", f.lfoRate (1));
    set (mod, "env_attack_ms", f.bank[P::envAttack]); set (mod, "env_release_ms", f.bank[P::envRelease]); set (mod, "env_gain_db", f.bank[P::envGain]);
    juce::Array<juce::var> ms;
    for (int m = 0; m < P::numMods; ++m)
    {
        juce::var v (new juce::DynamicObject()); const int t = f.modDst[(size_t) m].get();
        set (v, "slot", m + 1); set (v, "source", P::sourceNames()[juce::jlimit (0, 3, f.modSrc[(size_t) m].get())]); set (v, "target", SqFissure::targetName (t));
        set (v, "amount", f.bank[P::mod1 + m] * P::modScale (t)); set (v, "units", P::modScale (t) > 1 ? "octaves" : "fraction");
        ms.add (v);
    }
    set (mod, "matrix", ms); set (o, "modulation", mod);
    set (o, "mix_pct", f.bank[P::mix]); set (o, "output_db", f.bank[P::output]);
    return o;
}

void registerFissureTools()
{
    add ("fissure_filter",
         "Fissure Filter, the filter flagship (Color & Space): four filters (low-pass, high-pass, band-pass, notch, peak, comb, vowel; 12 / 24 / 48 dB/oct), routed serial, "
         "parallel, two pairs or mid / side; two LFOs (free or synced) and an envelope follower through an 8-slot matrix. action: add (optional preset), get, set (global), "
         "filter (one filter), mod (modulators / one matrix slot), preset (name or 'list'), open.\n"
         " set: routing (serial | parallel | two pairs | mid side), mix (%), output (dB).\n"
         " filter: filter (1-4) and any of on (bool), type, slope (12 | 24 | 48), cutoff (Hz), resonance (%), drive (dB), gain (dB, peak), pan (-100..100), mix (%).\n"
         " mod: lfo1_shape / lfo2_shape (sine | triangle | saw | square | random), lfo1_rate / lfo2_rate (Hz), lfo1_sync / lfo2_sync (free | 4 bars | 2 bars | 1 bar | 1/2 | 1/4 | 1/8 | 1/16 | "
         "1/4 dotted | 1/8 dotted | 1/4 triplet | 1/8 triplet), env_attack / env_release (ms), env_gain (dB); and/or slot (1-8) with source (off | lfo 1 | lfo 2 | envelope), "
         "target ('filter N cutoff', 'filter N resonance', 'filter N mix', 'global mix', 'none'), amount (octaves -6..6 for cutoff targets, otherwise -1..1).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","filter","mod","preset","open"]},
            "routing":{"type":"string"},"mix":{"type":"number"},"output":{"type":"number"},"filter":{"type":"integer"},"on":{"type":"boolean"},"type":{"type":"string"},"slope":{"type":"integer"},
            "cutoff":{"type":"number"},"resonance":{"type":"number"},"drive":{"type":"number"},"gain":{"type":"number"},"pan":{"type":"number"},
            "lfo1_shape":{"type":"string"},"lfo2_shape":{"type":"string"},"lfo1_rate":{"type":"number"},"lfo2_rate":{"type":"number"},"lfo1_sync":{"type":"string"},"lfo2_sync":{"type":"string"},
            "env_attack":{"type":"number"},"env_release":{"type":"number"},"env_gain":{"type":"number"},"slot":{"type":"integer"},"source":{"type":"string"},"target":{"type":"string"},
            "amount":{"type":"number"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqFissureFilterPlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqFissureFilterPlugin* f = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqFissureFilterPlugin*> (pl)) { f = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Fissure preset"); Signature::applyPreset (*f, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (f == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Fissure Filter", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Fissure Filter"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     f = dynamic_cast<SqFissureFilterPlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeFissure (*f)));
             }
             if (f == nullptr) return ToolResult::error ("No Fissure Filter on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeFissure (*f)));
             if (action == "open") { f->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             auto* um = &session.edit->getUndoManager();
             auto setP = [&] (int id, double x) { auto& p = *f->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
             if (action == "set")
             {
                 um->beginNewTransaction ("Fissure settings");
                 if (has (a, "routing"))
                 {
                     const auto v = str (a, "routing").toLowerCase();
                     f->routing.setValue (v.startsWith ("par") ? 1 : (v.contains ("pair") || v.startsWith ("two")) ? 2 : (v.contains ("mid") || v.contains ("side")) ? 3 : 0, um);
                 }
                 if (has (a, "mix")) setP (P::mix, num (a, "mix")); if (has (a, "output")) setP (P::output, num (a, "output"));
                 return ToolResult::ok (juce::JSON::toString (describeFissure (*f)));
             }
             if (action == "filter")
             {
                 const int k = (int) num (a, "filter") - 1; if (k < 0 || k >= P::numSlots) return ToolResult::error ("filter must be 1-4");
                 um->beginNewTransaction ("Fissure filter " + juce::String (k + 1));
                 if (has (a, "on")) f->slotOn[(size_t) k].setValue ((bool) a["on"], um);
                 if (has (a, "type"))
                 {
                     const auto v = str (a, "type").removeCharacters ("- ").toLowerCase(); int t = -1;
                     for (int i = 0; i < SqFissure::numTypes; ++i) if (juce::String (SqFissure::typeName (i)).removeCharacters ("- ").toLowerCase().startsWith (v.substring (0, 4))) { t = i; break; }
                     if (v == "lp" || v == "lowpass") t = 0; if (v == "hp" || v == "highpass") t = 1; if (v == "bp") t = 2;
                     if (t < 0) return ToolResult::error ("type: low-pass, high-pass, band-pass, notch, peak, comb, vowel");
                     f->slotType[(size_t) k].setValue (t, um); if (! has (a, "on")) f->slotOn[(size_t) k].setValue (true, um);
                 }
                 if (has (a, "slope")) { const int sl = (int) num (a, "slope"); f->slotSlope[(size_t) k].setValue (sl >= 48 ? 48 : sl >= 24 ? 24 : 12, um); }
                 for (auto [nm, sub] : { std::pair<const char*, int> { "cutoff", P::cutoff }, { "resonance", P::res }, { "drive", P::drive }, { "gain", P::gain }, { "pan", P::pan }, { "mix", P::slotMix } })
                     if (has (a, nm)) setP (f->paramOf (k, sub), num (a, nm));
                 return ToolResult::ok (juce::JSON::toString (describeFissure (*f)));
             }
             if (action == "mod")
             {
                 um->beginNewTransaction ("Fissure modulation");
                 for (int l = 0; l < 2; ++l)
                 {
                     const juce::String pre = "lfo" + juce::String (l + 1) + "_";
                     if (has (a, (pre + "shape").toRawUTF8())) { const int i = P::lfoNames().indexOf (str (a, (pre + "shape").toRawUTF8()), true); if (i < 0) return ToolResult::error ("shape: sine, triangle, saw, square, random"); (l == 0 ? f->lfo1Shape : f->lfo2Shape).setValue (i, um); }
                     if (has (a, (pre + "rate").toRawUTF8())) { setP (l == 0 ? P::lfo1Rate : P::lfo2Rate, num (a, (pre + "rate").toRawUTF8())); (l == 0 ? f->lfo1Sync : f->lfo2Sync).setValue (0, um); }
                     if (has (a, (pre + "sync").toRawUTF8()))
                     {
                         const auto v = str (a, (pre + "sync").toRawUTF8()).trim(); int i = v.equalsIgnoreCase ("free") ? 0 : P::syncNames().indexOf (v, true);
                         if (i < 0) return ToolResult::error ("sync: " + P::syncNames().joinIntoString (", ")); (l == 0 ? f->lfo1Sync : f->lfo2Sync).setValue (i, um);
                     }
                 }
                 if (has (a, "env_attack")) setP (P::envAttack, num (a, "env_attack")); if (has (a, "env_release")) setP (P::envRelease, num (a, "env_release")); if (has (a, "env_gain")) setP (P::envGain, num (a, "env_gain"));
                 if (has (a, "slot"))
                 {
                     const int m = (int) num (a, "slot") - 1; if (m < 0 || m >= P::numMods) return ToolResult::error ("slot must be 1-8");
                     if (has (a, "source")) { const auto v = str (a, "source").toLowerCase().removeCharacters (" "); f->modSrc[(size_t) m].setValue (v == "lfo1" ? 1 : v == "lfo2" ? 2 : v.startsWith ("env") ? 3 : 0, um); }
                     if (has (a, "target"))
                     {
                         int t = -1; const auto v = str (a, "target").toLowerCase();
                         for (int i = 0; i <= 13; ++i) if (SqFissure::targetName (i).toLowerCase() == v) t = i;
                         if (t < 0) return ToolResult::error ("target: 'filter N cutoff', 'filter N resonance', 'filter N mix', 'global mix' or 'none'");
                         f->modDst[(size_t) m].setValue (t, um);
                     }
                     if (has (a, "amount")) setP (P::mod1 + m, num (a, "amount") / P::modScale (f->modDst[(size_t) m].get()));
                 }
                 return ToolResult::ok (juce::JSON::toString (describeFissure (*f)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
