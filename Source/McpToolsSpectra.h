// Squelchmoth 1.14 - Claude's `spectra_eq` tool: the flagship 24-band EQ by name. Included inside the McpTools class body.

static int spectraShapeIndex (const juce::String& s)
{
    const auto k = s.toLowerCase().removeCharacters (" -_");
    static const char* names[] = { "bell", "notch", "lowshelf", "highshelf", "lowcut", "highcut", "bandpass", "tiltshelf", "flattilt", "allpass" };
    for (int i = 0; i < 10; ++i) if (k == names[i]) return i;
    if (k == "peak" || k == "peaking") return 0;
    if (k == "hp" || k == "highpass" || k == "hpf") return 4;
    if (k == "lp" || k == "lowpass" || k == "lpf") return 5;
    if (k == "tilt") return 7;
    return -1;
}
static int spectraPlaceIndex (const juce::String& s)
{
    const auto k = s.toLowerCase();
    if (k.startsWith ("l")) return 1; if (k.startsWith ("r")) return 2; if (k.startsWith ("m")) return 3; if (k.startsWith ("s") && ! k.startsWith ("st")) return 4;
    return 0;
}

juce::var describeSpectra (SqSpectraEqPlugin& eq)
{
    juce::var o (new juce::DynamicObject());
    set (o, "plugin", "Spectra EQ");
    set (o, "phase", SqSpectra::phaseNames()[juce::jlimit (0, 2, eq.phaseMode.get())]);
    if (eq.phaseMode.get() == 2) set (o, "linear_quality", SqSpectraEqPlugin::qualityNames()[juce::jlimit (0, 4, eq.quality.get())]);
    set (o, "latency_ms", eq.getLatencySeconds() * 1000.0);
    set (o, "output_db", eq.bank[(size_t) SqSpectraEqPlugin::outputParam]);
    set (o, "auto_gain", eq.autoGain.get()); if (eq.autoGain.get()) set (o, "auto_gain_db", eq.appliedAutoGain.load());
    set (o, "analyser", SqSpectraEqPlugin::analyserNames()[juce::jlimit (0, 3, eq.analyser.get())]);
    juce::Array<juce::var> bands;
    for (int i = 0; i < SqSpectraEqPlugin::numBands; ++i)
    {
        const auto b = eq.band (i); if (! b.enabled) continue;
        juce::var bo (new juce::DynamicObject());
        set (bo, "band", i + 1); set (bo, "shape", SqSpectra::shapeNames()[(int) b.shape]); set (bo, "freq_hz", juce::roundToInt (b.freq * 10) / 10.0);
        if (b.hasGain()) set (bo, b.shape == SqSpectra::Shape::flatTilt ? "slope_db_per_oct" : "gain_db", juce::roundToInt (b.gainDb * 100) / 100.0);
        set (bo, "q", juce::roundToInt (b.q * 1000) / 1000.0);
        if (b.shape == SqSpectra::Shape::lowCut || b.shape == SqSpectra::Shape::highCut || b.shape == SqSpectra::Shape::bandPass || b.shape == SqSpectra::Shape::notch)
            set (bo, "slope", b.slope <= 0 ? juce::var ("brickwall") : juce::var (b.slope));
        set (bo, "placement", SqSpectra::placeNames()[(int) b.place]);
        if (b.dynamic)
        {
            set (bo, "dynamic", true); set (bo, "threshold_db", b.threshDb); set (bo, "range_db", b.rangeDb); set (bo, "attack_ms", b.attackMs); set (bo, "release_ms", b.releaseMs);
            set (bo, "key", b.externalKey ? "external" : "band"); set (bo, "moving_now_db", juce::roundToInt (eq.dynDb[(size_t) i].load() * 100) / 100.0);
        }
        bands.add (bo);
    }
    set (o, "bands", bands);
    // the curve at a few landmark frequencies, so Claude can reason about the result without the window
    juce::var curve (new juce::DynamicObject());
    for (double hz : { 30.0, 60.0, 120.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 12000.0, 16000.0 })
        set (curve, juce::String ((int) hz), juce::roundToInt (eq.curveDb (hz) * 100) / 100.0);
    set (o, "curve_db", curve);
    return o;
}

void registerSpectraTools()
{
    add ("spectra_eq",
         "Spectra EQ, the flagship 24-band EQ (Mastering Suite). action: add (inserts it; optional preset), get (every band, phase, latency, output, the curve at landmark frequencies, "
         "what each dynamic band is doing right now), band (create or edit one band), remove (band number, or 'all'), set (global), preset (name or 'list'), open.\n"
         " band: band (1-24; omit to use the next free one), shape (bell | notch | low shelf | high shelf | low cut | high cut | band pass | tilt shelf | flat tilt | all pass), "
         "freq (Hz), gain (dB; flat tilt: dB per octave, +/-6), q (0.025-40; cuts: 0.707 = flat), slope (6 12 18 24 30 36 48 72 96 dB/oct or 'brickwall'), "
         "placement (stereo | left | right | mid | side), dynamic (bool, bell / shelves / tilt), threshold (dB), range (dB, + boosts and - cuts when the band gets loud), attack / release (ms), "
         "key ('band' listens through the band itself, 'external' listens to the sidechain key), on (bool).\n"
         " set: phase (zero latency | natural | linear), quality (0-4: Linear FIR 2k..32k taps), output (dB), auto_gain (bool), analyser (off | post | pre | key), range (3 | 6 | 12 | 30 dB display).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["add","get","band","remove","set","preset","open"]},
            "band":{"type":["integer","string"]},"shape":{"type":"string"},"freq":{"type":"number"},"gain":{"type":"number"},"q":{"type":"number"},"slope":{"type":["integer","string"]},
            "placement":{"type":"string"},"dynamic":{"type":"boolean"},"threshold":{"type":"number"},"range":{"type":"number"},"attack":{"type":"number"},"release":{"type":"number"},
            "key":{"type":"string"},"on":{"type":"boolean"},"phase":{"type":"string"},"quality":{"type":"integer"},"output":{"type":"number"},"auto_gain":{"type":"boolean"},
            "analyser":{"type":"string"},"preset":{"type":"string"}},"required":["track","action"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* t = nullptr;
             auto list = pluginListArg (a, err, t);
             if (list == nullptr) return ToolResult::error (err);
             SqSpectraEqPlugin* eq = nullptr;
             for (auto pl : *list) if (auto e = dynamic_cast<SqSpectraEqPlugin*> (pl)) { eq = e; break; }
             if (action == "add")
             {
                 if (eq == nullptr)
                 {
                     juce::String why;
                     auto np = session.createPluginByName ("Spectra EQ", why);
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create Spectra EQ"));
                     list->insertPlugin (np, t != nullptr ? Session::insertEndIndex (*list, t) : -1, nullptr);
                     eq = dynamic_cast<SqSpectraEqPlugin*> (np.get());
                 }
                 if (has (a, "preset")) for (auto& e : Signature::entries()) if (juce::String (e.type) == SqSpectraEqPlugin::xmlTypeName)
                     for (auto& pr : e.presets) if (juce::String (pr.name).containsIgnoreCase (str (a, "preset"))) { Signature::applyPreset (*eq, pr); break; }
                 if (ui.refresh) ui.refresh();
                 return ToolResult::ok (juce::JSON::toString (describeSpectra (*eq)));
             }
             if (eq == nullptr) return ToolResult::error ("No Spectra EQ on that track - use action add");
             auto* um = &session.edit->getUndoManager();
             auto setParam = [&] (int band, int which, double v) { auto& p = *eq->bank.params[(size_t) (band * SqSpectraEqPlugin::perBand + which)]; p.setParameter ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, v), juce::sendNotification); };
             if (action == "get") return ToolResult::ok (juce::JSON::toString (describeSpectra (*eq)));
             if (action == "open") { eq->showWindowExplicitly(); return ToolResult::ok ("Window open"); }
             if (action == "preset")
             {
                 for (auto& e : Signature::entries()) if (juce::String (e.type) == SqSpectraEqPlugin::xmlTypeName)
                 {
                     juce::StringArray names; for (auto& pr : e.presets) names.add (pr.name);
                     const auto want = str (a, "preset");
                     for (auto& pr : e.presets) if (want.isNotEmpty() && ! want.equalsIgnoreCase ("list") && juce::String (pr.name).containsIgnoreCase (want)) { um->beginNewTransaction ("Spectra EQ preset"); Signature::applyPreset (*eq, pr); return ToolResult::ok ("Loaded '" + juce::String (pr.name) + "'\n" + juce::JSON::toString (describeSpectra (*eq))); }
                     return ToolResult::ok ("Presets: " + names.joinIntoString (", "));
                 }
                 return ToolResult::error ("No presets");
             }
             if (action == "remove")
             {
                 um->beginNewTransaction ("Spectra EQ remove band");
                 if (str (a, "band").equalsIgnoreCase ("all")) { for (auto& p : eq->props) p.on.setValue (false, um); return ToolResult::ok ("Every band removed"); }
                 const int b = (int) num (a, "band", 0) - 1;
                 if (b < 0 || b >= SqSpectraEqPlugin::numBands) return ToolResult::error ("band must be 1-24 or 'all'");
                 eq->props[(size_t) b].on.setValue (false, um);
                 return ToolResult::ok ("Band " + juce::String (b + 1) + " removed");
             }
             if (action == "set")
             {
                 um->beginNewTransaction ("Spectra EQ settings");
                 if (has (a, "phase")) { const auto p = str (a, "phase").toLowerCase(); eq->phaseMode.setValue (p.contains ("lin") ? 2 : p.contains ("nat") ? 1 : 0, um); }
                 if (has (a, "quality")) eq->quality.setValue (juce::jlimit (0, 4, (int) num (a, "quality")), um);
                 if (has (a, "auto_gain")) eq->autoGain.setValue ((bool) a["auto_gain"], um);
                 if (has (a, "analyser")) { const auto v = str (a, "analyser").toLowerCase(); eq->analyser.setValue (v.contains ("off") ? 0 : v.contains ("key") ? 3 : v.contains ("pre") ? 2 : 1, um); }
                 if (has (a, "range")) { const double r = num (a, "range"); eq->displayRange.setValue (r <= 3 ? 0 : r <= 6 ? 1 : r <= 12 ? 2 : 3, um); }
                 if (has (a, "output")) { auto& p = *eq->bank.params[(size_t) SqSpectraEqPlugin::outputParam]; p.setParameter ((float) juce::jlimit (-36.0, 36.0, num (a, "output")), juce::sendNotification); }
                 return ToolResult::ok (juce::JSON::toString (describeSpectra (*eq)));
             }
             if (action == "band")
             {
                 int b = has (a, "band") ? (int) num (a, "band") - 1 : eq->firstFreeBand();
                 if (b < 0 || b >= SqSpectraEqPlugin::numBands) return ToolResult::error (has (a, "band") ? "band must be 1-24" : "All 24 bands are in use");
                 auto& p = eq->props[(size_t) b];
                 um->beginNewTransaction ("Spectra EQ band");
                 if (has (a, "shape"))
                 {
                     const int sh = spectraShapeIndex (str (a, "shape"));
                     if (sh < 0) return ToolResult::error ("Unknown shape '" + str (a, "shape") + "'. Shapes: " + SqSpectra::shapeNames().joinIntoString (", "));
                     if (! p.on.get() && ! has (a, "q")) setParam (b, SqSpectraEqPlugin::pQ, sh == 4 || sh == 5 ? 0.707 : 1.0);
                     if (! p.on.get() && ! has (a, "slope") && (sh == 4 || sh == 5)) p.slope.setValue (24, um);
                     p.shape.setValue (sh, um);
                 }
                 if (has (a, "freq")) setParam (b, SqSpectraEqPlugin::pFreq, num (a, "freq"));
                 if (has (a, "gain")) setParam (b, SqSpectraEqPlugin::pGain, num (a, "gain"));
                 if (has (a, "q")) setParam (b, SqSpectraEqPlugin::pQ, num (a, "q"));
                 if (has (a, "slope"))
                 {
                     const auto sv = str (a, "slope"); int sl = sv.containsIgnoreCase ("brick") ? 0 : sv.getIntValue();
                     bool ok = false; for (auto x : SqSpectra::slopes()) if (x == sl) ok = true;
                     if (! ok) return ToolResult::error ("slope: 6, 12, 18, 24, 30, 36, 48, 72, 96 or brickwall");
                     p.slope.setValue (sl, um);
                 }
                 if (has (a, "placement")) p.place.setValue (spectraPlaceIndex (str (a, "placement")), um);
                 if (has (a, "dynamic")) p.dyn.setValue ((bool) a["dynamic"], um);
                 if (has (a, "threshold")) setParam (b, SqSpectraEqPlugin::pThresh, num (a, "threshold"));
                 if (has (a, "range")) { setParam (b, SqSpectraEqPlugin::pRange, num (a, "range")); if (! has (a, "dynamic")) p.dyn.setValue (true, um); }
                 if (has (a, "attack")) p.attack.setValue ((float) juce::jlimit (0.1, 500.0, num (a, "attack")), um);
                 if (has (a, "release")) p.release.setValue ((float) juce::jlimit (5.0, 5000.0, num (a, "release")), um);
                 if (has (a, "key")) p.extKey.setValue (str (a, "key").containsIgnoreCase ("ext"), um);
                 p.on.setValue (has (a, "on") ? (bool) a["on"] : true, um);
                 auto d = describeSpectra (*eq);
                 return ToolResult::ok ("Band " + juce::String (b + 1) + " set\n" + juce::JSON::toString (d));
             }
             return ToolResult::error ("Unknown action '" + action + "'");
         });
}
