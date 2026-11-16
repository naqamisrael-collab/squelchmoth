// Squelchmoth 1.14 - Claude's `kiln` tool: multiband saturation by name. Included inside the McpTools class body.

juce::var describeKiln (SqKilnPlugin& k)
{
    using P = SqKilnPlugin;
    juce::var o (new juce::DynamicObject());
    const int n = juce::jlimit (1, P::maxBands, k.numBands.get());
    set (o, "plugin", "Kiln"); set (o, "bands", n); set (o, "oversampling", P::osNames()[juce::jlimit (0, 4, k.oversample.get())]);
    set (o, "linear_phase_oversampling", k.linearOs.get()); set (o, "auto_gain", k.autoGain.get());
    set (o, "input_db", k.bank[P::input]); set (o, "output_db", k.bank[P::output]); set (o, "mix_pct", k.bank[P::mix]); set (o, "latency_ms", k.getLatencySeconds() * 1000.0);
    SqMB2::Params sp; sp.numBands = n; for (int i = 0; i < 5; ++i) sp.xover[(size_t) i] = k.bank[P::x1 + i];
    const auto x = SqMB2::sanitise (sp, 48000.0); juce::Array<juce::var> xs; for (int i = 0; i < n - 1; ++i) xs.add (x[(size_t) i]);
    set (o, "crossovers_hz", xs);
    juce::Array<juce::var> bands;
    for (int b = 0; b < n; ++b)
    {
        juce::var v (new juce::DynamicObject());
        set (v, "band", b + 1); set (v, "model", Cinder::modelName (k.model[(size_t) b].get()));
        set (v, "drive_db", k.bp (b, P::drive)); set (v, "bias", k.bp (b, P::bias)); set (v, "dynamics_pct", k.bp (b, P::dynamics));
        set (v, "mix_pct", k.bp (b, P::bandMix)); set (v, "output_db", k.bp (b, P::bandOut)); set (v, "solo", k.solo[(size_t) b].get()); set (v, "bypass", k.bypass[(size_t) b].get());
        set (v, "drive_now_db", k.driveNow[(size_t) b].load()); set (v, "activity", k.activity[(size_t) b].load());
        bands.add (v);
    }
    set (o, "band_settings", bands);
    juce::var mod (new juce::DynamicObject());
    set (mod, "lfo_shape", P::lfoNames()[juce::jlimit (0, 3, k.lfoShape.get())]); set (mod, "lfo_rate_hz", k.bank[P::lfoRate]);
    set (mod, "env_attack_ms", k.bank[P::envAttack]); set (mod, "env_release_ms", k.bank[P::envRelease]);
    juce::Array<juce::var> slots;
    for (int s = 0; s < P::numSlots; ++s)
    {
        juce::var v (new juce::DynamicObject()); const int t = k.slotTarget[(size_t) s].get();
        set (v, "slot", s + 1); set (v, "source", P::sourceNames()[juce::jlimit (0, 2, k.slotSource[(size_t) s].get())]); set (v, "target", SqKiln::targetName (t));
        set (v, "amount", k.bank[P::slot1 + s] * P::slotScale (t)); set (v, "amount_units", P::slotScale (t) > 1 ? "dB" : "fraction of mix");
        slots.add (v);
    }
    set (mod, "slots", slots); set (o, "modulation", mod);
    return o;
}

void registerKilnTools()
{
    add ("kiln",
         "Kiln, multiband saturation on the Cinder engine (Color & Space): up to 6 bands, each with its own model, drive, bias, dynamics, mix and output; oversampling 1-16x; "
         "an LFO and an envelope follower routed by a 4-slot matrix to any band's drive or mix (or the global mix). action: add (optional preset), get, set (global), band (one band), "
         "mod (one matrix slot or the modulators), preset (name or 'list'), open.\n"
         " set: bands (1-6), crossovers (Hz array, low to high), oversampling (1 2 4 8 16), linear_phase (bool), auto_gain (bool), input / output (dB), mix (%).\n"
         " band: band (1-6) and any of model (Tape Preamp | British Channel | Germanium Class-A | Triode Box | Pentode Box | Transformer Iron | Op-Amp Rail | Diode Fuzz | "
         "Tape Hysteresis | Broadcast Limiter Amp), drive (dB -12..36), bias (-1..1), dynamics (%), mix (%), output (dB), solo, bypass.\n"
         " mod: lfo_shape (sine | triangle | square | random), lfo_rate (Hz), env_attack / env_release (ms); and/or slot (1-4) with source (off | lfo | envelope), "
         "target ('band N drive', 'band N mix', 'global mix' or 'none'), amount (dB for drive targets, -36..36; fraction for mix targets, -1..1).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","set","band","mod","preset","open"]},
            "bands":{"type":"integer"},"crossovers":{"type":"array","items":{"type":"number"}},"oversampling":{"type":"integer"},"linear_phase":{"type":"boolean"},"auto_gain":{"type":"boolean"},
            "input":{"type":"number"},"output":{"type":"number"},"mix":{"type":"number"},"band":{"type":"integer"},"model":{"type":"string"},"drive":{"type":"number"},"bias":{"type":"number"},
            "dynamics":{"type":"number"},"solo":{"type":"boolean"},"bypass":{"type":"boolean"},"lfo_shape":{"type":"string"},"lfo_rate":{"type":"number"},"env_attack":{"type":"number"},
            "env_release":{"type":"number"},"slot":{"type":"integer"},"source":{"type":"string"},"target":{"type":"string"},"amount":{"type":"number"},"preset":{"type":"string"}},
            "required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             using P = SqKilnPlugin;
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* tr = nullptr;
             auto list = pluginListArg (a, err, tr);
             if (list == nullptr) return ToolResult::error (err);
             SqKilnPlugin* k = nullptr;
             for (auto pl : *list) if (auto c = dynamic_cast<SqKilnPlugin*> (pl)) { k = c; break; }
             auto loadPreset = [&] (const juce::String& want) -> juce::String
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == P::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { session.edit->getUndoManager().beginNewTransaction ("Kiln preset"); Signature::applyPreset (*k, pr); return "Loaded '" + juce::String (pr.name) + "'"; }
                     return "Presets: " + names.joinIntoString (", ");
                 }
                 return "No presets";
             };
             if (action == "add")
             {
                 if (k == nullptr)
                 {
                     juce::String why; auto np = session.createPluginByName ("Kiln", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Kiln"));
                     list->insertPlugin (np, tr != nullptr ? Session::insertEndIndex (*list, tr) : -1, nullptr);
                     k = dynamic_cast<SqKilnPlugin*> (np.get());
                 }
                 if (has (a, "preset")) loadPreset (str (a, "preset"));
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeKiln (*k)));
             }
             if (k == nullptr) return ToolResult::error ("No Kiln on that track - use action add");
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeKiln (*k)));
             if (action == "open") { k->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset") return ToolResult::ok (loadPreset (str (a, "preset")));
             auto* um = &session.edit->getUndoManager();
             auto setP = [&] (int id, double x) { auto& p = *k->bank.params[(size_t) id]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, x), juce::sendNotification); };
             if (action == "set")
             {
                 um->beginNewTransaction ("Kiln settings");
                 if (has (a, "bands")) k->numBands.setValue (juce::jlimit (1, P::maxBands, (int) num (a, "bands")), um);
                 if (auto* arr = a["crossovers"].getArray()) for (int i = 0; i < juce::jmin (5, arr->size()); ++i) setP (P::x1 + i, (double) (*arr)[i]);
                 if (has (a, "oversampling")) { const int f = (int) num (a, "oversampling"); int lg = 0; while ((1 << lg) < f && lg < 4) ++lg; k->oversample.setValue (lg, um); }
                 if (has (a, "linear_phase")) k->linearOs.setValue ((bool) a["linear_phase"], um);
                 if (has (a, "auto_gain")) k->autoGain.setValue ((bool) a["auto_gain"], um);
                 for (auto [nm, id] : { std::pair<const char*, int> { "input", P::input }, { "output", P::output }, { "mix", P::mix } }) if (has (a, nm)) setP (id, num (a, nm));
                 return ToolResult::ok (juce::JSON::toString (describeKiln (*k)));
             }
             if (action == "band")
             {
                 const int b = (int) num (a, "band") - 1;
                 if (b < 0 || b >= juce::jlimit (1, P::maxBands, k->numBands.get())) return ToolResult::error ("band must be 1.." + juce::String (k->numBands.get()));
                 um->beginNewTransaction ("Kiln band " + juce::String (b + 1));
                 if (has (a, "model"))
                 {
                     const int m = P::modelNames().indexOf (str (a, "model"), true); int found = m;
                     if (found < 0) for (int i = 0; i < P::modelNames().size(); ++i) if (P::modelNames()[i].containsIgnoreCase (str (a, "model"))) { found = i; break; }
                     if (found < 0) return ToolResult::error ("Models: " + P::modelNames().joinIntoString (", "));
                     k->model[(size_t) b].setValue (found, um);
                 }
                 for (auto [nm, sub] : { std::pair<const char*, int> { "drive", P::drive }, { "bias", P::bias }, { "dynamics", P::dynamics }, { "mix", P::bandMix }, { "output", P::bandOut } })
                     if (has (a, nm)) setP (k->paramOf (b, sub), num (a, nm));
                 if (has (a, "solo")) k->solo[(size_t) b].setValue ((bool) a["solo"], um);
                 if (has (a, "bypass")) k->bypass[(size_t) b].setValue ((bool) a["bypass"], um);
                 return ToolResult::ok (juce::JSON::toString (describeKiln (*k)));
             }
             if (action == "mod")
             {
                 um->beginNewTransaction ("Kiln modulation");
                 if (has (a, "lfo_shape")) { const int i = P::lfoNames().indexOf (str (a, "lfo_shape"), true); if (i < 0) return ToolResult::error ("lfo_shape: sine, triangle, square, random"); k->lfoShape.setValue (i, um); }
                 if (has (a, "lfo_rate")) setP (P::lfoRate, num (a, "lfo_rate"));
                 if (has (a, "env_attack")) setP (P::envAttack, num (a, "env_attack"));
                 if (has (a, "env_release")) setP (P::envRelease, num (a, "env_release"));
                 if (has (a, "slot"))
                 {
                     const int s = (int) num (a, "slot") - 1; if (s < 0 || s >= P::numSlots) return ToolResult::error ("slot must be 1-4");
                     if (has (a, "source")) { const auto v = str (a, "source").toLowerCase(); k->slotSource[(size_t) s].setValue (v.startsWith ("lfo") ? 1 : v.startsWith ("env") ? 2 : 0, um); }
                     if (has (a, "target"))
                     {
                         int t = -1; const auto v = str (a, "target").toLowerCase();
                         for (int i = 0; i <= 13; ++i) if (SqKiln::targetName (i).toLowerCase() == v) t = i;
                         if (t < 0) return ToolResult::error ("target: 'band N drive', 'band N mix', 'global mix' or 'none'");
                         k->slotTarget[(size_t) s].setValue (t, um);
                     }
                     if (has (a, "amount")) setP (P::slot1 + s, num (a, "amount") / P::slotScale (k->slotTarget[(size_t) s].get()));
                 }
                 return ToolResult::ok (juce::JSON::toString (describeKiln (*k)));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
