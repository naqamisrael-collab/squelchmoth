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
    if (s.contains ("shard") || s.contains ("crystal")) return SqShardPlugin::xmlTypeName;
    if (s.contains ("murmur") || s.contains ("cloud") || s.contains ("swarm")) return SqMurmurationPlugin::xmlTypeName;
    if (s.contains ("masquerade") || s.contains ("voicechanger") || s.contains ("masq")) return SqMasqueradePlugin::xmlTypeName;
    if (s.contains ("vellum")) return SqVellumPlugin::xmlTypeName;
    if (s.contains ("tremor") || s.contains ("tremolo")) return SqTremorPlugin::xmlTypeName;
    if (s.contains ("orbit") || s.contains ("autopan")) return SqOrbitPanPlugin::xmlTypeName;
    if (s.contains ("mirrorshiftlite") || s.contains ("mirrorlite")) return SqMirrorShiftLitePlugin::xmlTypeName;
    if (s.contains ("mirror") || s.contains ("widen")) return SqMirrorShiftPlugin::xmlTypeName;
    if (s.contains ("shutterlite")) return SqShutterLitePlugin::xmlTypeName;
    if (s.contains ("shutter") || s.contains ("digitaldelay")) return SqShutterPlugin::xmlTypeName;
    if (s.contains ("cinder")) return SqCinderPlugin::xmlTypeName;
    if (s.contains ("radiantpre")) return SqRadiantPrePlugin::xmlTypeName;
    if (s.contains ("radiant")) return SqRadiantPlugin::xmlTypeName;
    if (s.contains ("mawlite")) return SqMawLitePlugin::xmlTypeName;
    if (s.contains ("maw")) return SqMawPlugin::xmlTypeName;
    if (s.contains ("platelite") || s.contains ("foillite")) return SqFoilPlateLitePlugin::xmlTypeName;
    if (s.contains ("foil") || s.contains ("plate")) return SqFoilPlatePlugin::xmlTypeName;
    if (s.contains ("wingbeatjr") || s.contains ("jr")) return SqWingbeatJrPlugin::xmlTypeName;
    if (s.contains ("wingbeat") || s.contains ("echo")) return SqWingbeatPlugin::xmlTypeName;
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
                              "sync", "poles", "envFast", "lfoShape", "stages",
                              "model", "modelB", "punishOn", "steepOn", "autoGainOn", "msMode", "oversample", "linearPhaseOn", "deltaOn", "keyOn",
                              "style", "syncL", "syncR", "timeChange", "heads", "limiter", "pattern", "freezeOn", "primeOn", "tapData", "tapLength", "sync",
                              "plate", "input", "lowSlope", "highSlope", "darkeningOn", "monoInOn", "arch", "micOn", "noisyOn", "biasOn", "fastOn",
                              "lowFreq", "midFreq", "highFreq", "mid2On", "steppedOn", "target", "shape", "syncOn", "division", "stepDivision", "numSteps", "dynMode", "analog",
                              "harmonicOn", "retrigOn", "steps", "custom", "triggerOn", "divider", "binauralOn", "panLaw", "multiply", "algo", "bits", "syncA", "syncB", "smoothOn", "pattern",
                              "engine", "fbMode", "dynMode", "quantizeOn", "autoKeyOn", "keyRoot", "scale", "spliceSync", "delaySync", "lfo1Target", "lfo1Shape", "lfo1Sync",
                              "lfo2Target", "lfo2Shape", "lfo2Sync", "envTarget", "modShape", "shimmerSt", "freezeMode", "infiniteOn", "customEnvOn", "envShape",
                              "linkOn", "voiceType", "robotNote", "driveVoicing" };
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
    if (auto wp = dynamic_cast<SqWingbeatBase*> (&p))
    {
        const auto& pp = wp->proc.p;
        if (auto we = dynamic_cast<SqWingbeatPlugin*> (&p)) { set (o, "style", SqWingbeatPlugin::styleNames()[juce::jlimit (0, Wingbeat::numStyles() - 1, we->style.get())]); set (o, "mode", SqWingbeatPlugin::modeNames()[juce::jlimit (0, 3, we->mode.get())]); }
        if (auto wj = dynamic_cast<SqWingbeatJrPlugin*> (&p)) { set (o, "style", SqWingbeatJrPlugin::styleNames()[juce::jlimit (0, 7, wj->style.get())]); set (o, "mode", SqWingbeatJrPlugin::modeNames()[juce::jlimit (0, 2, wj->mode.get())]); }
        set (o, "live_time_ms", wp->liveTimeMs.load()); set (o, "ducking_db", wp->liveDuckDb.load()); set (o, "tempo_bpm", wp->lastBpm);
        if (pp.mode == Wingbeat::rhythm) set (o, "pattern", pp.customTaps.empty() ? juce::String (Wingbeat::patterns()[(size_t) juce::jlimit (0, (int) Wingbeat::patterns().size() - 1, pp.pattern)].name) : juce::String ("custom (" + juce::String ((int) pp.customTaps.size()) + " taps)"));
    }
    if (auto rp = dynamic_cast<SqRadiantPlugin*> (&p)) { set (o, "circuit", SqRadiantPlugin::archNames()[juce::jlimit (0, 2, rp->arch.get())]); set (o, "thd_percent_at_minus18", rp->thd()); set (o, "heat_db", rp->heatDb.load()); set (o, "latency_ms", rp->getLatencySeconds() * 1000.0); }
    if (auto rpp = dynamic_cast<SqRadiantPrePlugin*> (&p)) { set (o, "thd_percent_at_minus18", rpp->thd()); set (o, "heat_db", rpp->heatDb.load()); }
    if (auto mp = dynamic_cast<SqMawBase*> (&p)) { set (o, "gain_reduction_db", mp->grDb.load()); set (o, "attack_ms", Maw::Processor::attackMs); set (o, "latency_ms", mp->getLatencySeconds() * 1000.0); }
    if (auto fp = dynamic_cast<SqFoilPlateBase*> (&p))
    {
        float lo, mid, hi; fp->proc.bandDecays (lo, mid, hi);
        if (auto full = dynamic_cast<SqFoilPlatePlugin*> (&p))
        {
            set (o, "plate", SqFoilPlatePlugin::archetypeNames()[juce::jlimit (0, 6, full->plate.get())]); set (o, "input", SqFoilPlatePlugin::inputNames()[juce::jlimit (0, 3, full->input.get())]);
            set (o, "input_thd_percent_at_minus18", full->inputThd());
        }
        set (o, "rt60_120hz_s", lo); set (o, "rt60_500hz_s", mid); set (o, "rt60_6khz_s", hi); set (o, "auto_decay_x", fp->liveDecayMul.load());
    }
    if (auto cp = dynamic_cast<SqCinderPlugin*> (&p))
    {
        set (o, "model", SqCinderPlugin::modelNames()[juce::jlimit (0, 9, cp->modelA.get())]);
        set (o, "model_b", SqCinderPlugin::modelNames()[juce::jlimit (0, 9, cp->modelB.get())]);
        set (o, "mid_side", SqCinderPlugin::msNames()[juce::jlimit (0, 3, cp->msMode.get())]);
        set (o, "oversampling", SqCinderPlugin::osNames()[juce::jlimit (0, 4, cp->oversample.get())] + juce::String (cp->linearPhase.get() ? " linear phase" : " minimum phase"));
        set (o, "latency_ms", cp->getLatencySeconds() * 1000.0); set (o, "auto_gain_db", cp->autoGainDb.load()); set (o, "drive_now_db", cp->activity.load());
        float h[10]; const double thd = cp->harmonicProfile (-18.0f, h);
        set (o, "thd_percent_at_minus18", thd); juce::Array<juce::var> hv; for (int k = 1; k < 10; ++k) hv.add (juce::roundToInt (h[k] * 10) / 10.0); set (o, "harmonics_h2_h10_db", hv);
    }
    if (auto rp = dynamic_cast<SqRingPlugin*> (&p)) set (o, "live_frequency_hz", rp->liveFreq.load());
    if (auto ap = dynamic_cast<SqAuroraPlugin*> (&p)) set (o, "room", SqAuroraPlugin::roomNames()[juce::jlimit (0, 7, ap->roomType.get())]);
    if (auto ph = dynamic_cast<SqPhase12Plugin*> (&p)) set (o, "stages", SqPhase12Plugin::stageCount (ph->stages.get()));
    if (auto ve = dynamic_cast<SqVellumPlugin*> (&p))
    {
        auto kp = ve->knobParams();
        set (o, "low_hz", Vellum::lowFreqs()[kp.lowIdx]); set (o, "mid_hz", Vellum::midFreqs()[kp.midIdx]); set (o, "high_hz", Vellum::highFreqs()[kp.highIdx]);
        set (o, "low_curve", kp.lowDb < 0 ? "shelf cut" : "very-low bell"); set (o, "high_curve", kp.highDb < 0 ? "gentle low-pass" : "very-high bell");
        set (o, "mid_q", Vellum::midQ (Vellum::detent (kp.midDb, 2, kp.stepped))); set (o, "target", Vellum::targetName (kp.target)); set (o, "latency_ms", ve->getLatencySeconds() * 1000.0);
    }
    if (auto tr = dynamic_cast<SqTremorPlugin*> (&p)) { set (o, "mode", Tremor::modeName (tr->mode.get())); set (o, "shape", Eng::WingMod::shapeName (tr->shape.get())); set (o, "rate_hz", tr->proc.cycleHz()); set (o, "live_gain_db", Eng::gainToDb (tr->proc.currentGain())); }
    if (auto ob = dynamic_cast<SqOrbitPanPlugin*> (&p)) { set (o, "mode", Orbit::modeName (ob->mode.get())); set (o, "pan_law", Orbit::lawName (SqOrbitPanPlugin::lawFor (ob->panLaw.get()))); set (o, "live_position_deg", ob->proc.position() * 90.0f); }
    if (auto ms = dynamic_cast<SqMirrorShiftPlugin*> (&p)) { set (o, "style", Mirror::style (ms->style.get()).name); set (o, "correlation", ms->proc.correlation()); set (o, "mono_sum_loss_db", ms->proc.monoLossDb()); set (o, "duck_db", ms->proc.duckGainDb()); }
    if (auto ml = dynamic_cast<SqMirrorShiftLitePlugin*> (&p)) { set (o, "style", Mirror::style (ml->style.get()).name); set (o, "correlation", ml->proc.correlation()); set (o, "mono_sum_loss_db", ml->proc.monoLossDb()); }
    if (auto sd = dynamic_cast<SqShardPlugin*> (&p))
    {
        const auto kp = sd->knobParams();
        set (o, "engine", Shard::engineName (kp.engine)); set (o, "recycle_mode", Shard::fbName (kp.fbMode));
        set (o, "pitch_st_used", Shard::quantisedPitch (kp.pitchSt, kp.quantize, kp.keyRoot, kp.scaleMask)); set (o, "key", SqSessionKey::rootNames()[kp.keyRoot] + (kp.quantize ? " (quantize on)" : " (quantize off)"));
        set (o, "splices_started", (double) sd->proc.splicesStarted()); set (o, "active_grains", sd->proc.activeGrains());
    }
    if (auto mu = dynamic_cast<SqMurmurationPlugin*> (&p))
    {
        const auto kp = mu->knobParams();
        set (o, "taps", Murm::tapsFor (kp.texture) * 2); set (o, "time_ms", mu->proc.timeMsTarget());
        juce::String env; for (int i = 0; i <= 8; ++i) env << juce::String (Murm::envelopeAt (kp, i / 8.0f), 2) << (i < 8 ? " " : "");
        set (o, "envelope_0_to_1", env); set (o, "frozen", kp.freeze); set (o, "infinite", kp.infinite);
    }
    if (auto mq = dynamic_cast<SqMasqueradePlugin*> (&p))
    {
        const auto kp = mq->knobParams();
        set (o, "mode", Masquerade::modeName (kp.mode)); set (o, "latency_ms", mq->getLatencySeconds() * 1000.0); set (o, "input_hz", mq->liveHz.load());
        set (o, "key", SqSessionKey::rootNames()[kp.keyRoot]); set (o, "formant_follows_pitch", kp.link);
    }
    if (auto sh = dynamic_cast<SqShutterBase*> (&p))
    {
        if (! p.edit.getTransport().isPlaying()) sh->refreshSettings();
        set (o, "bandwidth_hz", Shutter::bandwidthHz (sh->proc.p.multiply)); set (o, "clock_hz", sh->proc.lineRate());
        set (o, "time_a_ms", sh->proc.totalMs (0)); set (o, "time_b_ms", sh->proc.totalMs (1)); set (o, "algorithm", Shutter::algoName (sh->proc.p.algo));
    }
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
         " pulse (Pulse EQ, passive program EQ + mid section) | oilcan (Oil Can Delay) | ladder (Silk Ladder) | ring (Halo Ring) | phaser (Twelve Phase) | bbd (Bucket Brigade) |\n"
         " cinder (Cinder, 1.11: ten-model saturation, 140 presets - 'get' also returns the harmonic profile H2-H10 and THD at -18 dBFS, latency and auto gain) |\n"
         " wingbeat (Wingbeat Echo, 1.11: 36 styles, 160 presets) | wingbeatjr (Wingbeat Jr., 8 styles, 60 presets) |\n"
         " foilplate (Foil Plate, 1.11: seven plates, 120 presets - 'get' reports RT60 per band and the input stage THD) | foilplatelite (Foil Plate Lite, 30 presets) |\n"
         " radiant (Radiant, 1.11 tube mixer, 90 presets) | radiantpre (Radiant Pre, 40 presets) | maw (Maw, 1.11 crushing limiter, 110 presets) | mawlite (Maw Lite, 30 presets) |\n"
         " vellum (Vellum EQ, broadcast EQ, 80 presets) | tremor (Tremor, tremolo / gate, 100 presets) | orbit (Orbit Pan, auto-pan, 90 presets) |\n"
         " mirror (Mirror Shift, micro-pitch widener, 60 presets - 'get' returns correlation and mono-sum loss) | mirrorlite (Mirror Shift Lite, 20) |\n"
         " shutter (Shutter Delay, digital rack delay, 100 presets) | shutterlite (Shutter Lite, 30) |\n"
         " shard (Shard, granular reverse pitch echo, 220 presets) | murmuration (Murmuration, tap-cloud space, 120 presets) | masquerade (Masquerade, voice changer, 100 presets).\n"
         " Shard settings: engine (0 Vintage 1 Clean), fbMode (0 Mixed 1 Dual 2 Ping-Pong), dynMode (0 off 1 gate 2 duck), keyOn, quantizeOn, autoKeyOn (follow Moth Key's key) or keyRoot (0-11) + scale,\n"
         "   spliceSync / delaySync (0 free, 1-27 division), lfo1Target / lfo2Target / envTarget (0 off 1 pitch 2 splice 3 delay 4 recycle 5 spray 6 mix 7 reverse), lfo1Shape / lfo2Shape (0-8), lfo1Sync / lfo2Sync.\n"
         " Murmuration settings: sync (0 free, 1-27 division; or the Beats knob), modShape, shimmerSt (0 -24, 1 -12, 2 +12, 3 +24 st), freezeOn, freezeMode (0 loop 1 spectral), infiniteOn,\n"
         "   customEnvOn + envShape (16 comma-separated levels 0-1). Masquerade settings: mode (0 Transpose 1 Quantize 2 Robot 3 MIDI), linkOn (formant follows pitch), engine (0 Live 1 Studio),\n"
         "   voiceType (0-5), autoKeyOn / keyRoot / scale, robotNote (MIDI 24-84), driveVoicing (0 off, 1-7 Clean Fat Squash Dirt Crunch Shred Pump).\n"
         " Vellum settings: lowFreq / midFreq / highFreq (step 0-5: low 35-330 Hz, mid 700-5600 Hz, high 3.3-15 kHz), mid2On, steppedOn, target (0 Stereo 1 Mid 2 Side 3 Left 4 Right), oversample (0-3).\n"
         " Tremor settings: mode (0 LFO, 1 Rhythm), shape (0-8), syncOn, division / stepDivision (0-26: 1/64 .. 4 bars T), numSteps, dynMode (0 off 1 env 2 gate 3 inverse), analog (0 off, 1-7 voicing),\n"
         "   harmonicOn, keyOn, retrigOn, pattern (0-23 factory rhythm), steps (16 comma-separated per-step depths 0-1, overrides pattern), custom (16 shape points -1..1).\n"
         " Orbit settings: mode (0 LFO 1 Rhythm Step 2 Rhythm Shape 3 Ping-Pong 4 Random 5 Random Step 6 Circle), shape, syncOn, division, stepDivision, numSteps, triggerOn, divider (1-16),\n"
         "   keyOn, binauralOn, panLaw (0 session, 1 0 dB, 2 -3 dB, 3 -4.5 dB, 4 -6 dB), pattern (0-11), steps (16 positions -1..1, overrides pattern).\n"
         " Mirror settings: style (0 Micro A, 1 Micro B, 2 Rack Wander, 3 Tape ADT, 4 Prism), keyOn. Shutter settings: multiply (0-3 = 1x 2x 4x 8x), algo (0 Classic 1 Parallel 2 Reverb\n"
         "   3 Series 4 Criss-Cross 5 Ping-Pong), bits (0 clean 1 12-bit 2 8-bit), syncA / syncB (0 free, 1-27 division), freezeOn, smoothOn.\n"
         " Actions: add (optional preset), get, set {name: value}, preset (name or 'list'), open.\n"
         " Parameters by name, e.g. Aurora: Pre-Delay, Size, Decay, Low Decay, High Decay, Diffusion, Early Level, Tail Level, Modulation, Mod Rate, Width, Low Cut, High Cut, Ducking, Mix;\n"
         "   Opto: Peak Reduction, Gain, Emphasis, Tube, Mix; Pulse: Low Boost, Low Atten, High Boost, Bandwidth, High Atten, Mid Low Peak, Mid Dip, Mid High Peak, Output, Tube;\n"
         "   Oil Can: Delay, Variation, Sustain, Tone, Drive, Wobble Rate, Mix; Ladder: Cutoff, Resonance, Env Amount, Drive, Output, Mix; Ring: Frequency, LFO Rate, LFO Amount, Drive, Output, Mix;\n"
         "   Phaser: Rate, Sweep, Centre, Resonance, Stereo Spread, Mix; BBD: Time, Feedback, Filter, Drive, Mod Rate, Mod Depth, Mix;\n"
         "   Cinder: Drive (-12..36 dB), Model Blend, Bias, Dynamics, Low Cut (20 = off), Thump, Tone (tilt, +-6 dB), High Cut, Output, Mix, Duck Drive.\n"
         " Cinder settings: model / modelB (Tape Preamp, British Channel, Germanium Class-A, Triode Box, Pentode Box, Transformer Iron, Op-Amp Rail, Diode Fuzz,\n"
         "   Tape Hysteresis, Broadcast Limiter Amp, or 0-9), punishOn (+20 dB), steepOn (30 dB/oct high cut), autoGainOn, msMode (Stereo, Mid/Side, Mid only, Side only),\n"
         "   oversample (1x 2x 4x 8x 16x or 0-4), linearPhaseOn, deltaOn (hear only what the saturation adds), keyOn (sidechain ducks the drive by Duck Drive).\n"
         " Wingbeat Echo: Time, Time R (ms, used when sync is free), Feedback (0-110), Mix, Saturation, Groove, Feel, Width, L/R Offset, Accent, Warp, Low Cut, High Cut,\n"
         "   Wow, Flutter, Diffusion, Duck, Duck Release, Input, Output. Settings: mode (Single, Dual, Ping-Pong, Rhythm), style (name or 0-35: Studio Tape 30, Studio Tape 15,\n"
         "   Tube Tape Echo, Spring Tape Echo, Bucket Pedal, Oil Can, Drum Echo, Chorus Box, AM Radio, Telephone, Digital Clean, Lunar Tape, Reverse Bloom, Infinite Wash ...),\n"
         "   syncL / syncR ('1/4', '1/8.', '1/16T', '2 bars' ... or 'free'), timeChange (0 tape glide, 1 crossfade, 2 jump), heads (1-7, multi-head styles), limiter (0-2),\n"
         "   pattern (rhythm pattern name or index), tapData (custom rhythm: [{sixteenths, level, pan, cutoff, pitch}], up to 32), tapLength (sixteenths), freezeOn, primeOn, keyOn.\n"
         " Wingbeat Jr.: Time, Feedback, Mix, Saturation, Tone, Output; settings style (0-7), mode (Normal, Wide, Ping-Pong), sync, timeChange (0 glide, 1 stretch).\n"
         " Foil Plate: Decay (RT60 at 500 Hz, 0.3-60 s), Pre-Delay, Low Cut, High Cut, Mod, Mod Rate, Mix (dry fades above 70 %), Width, Balance, Low EQ, High EQ,\n"
         "   Auto Decay (-100 shortens while the input plays and blooms in the gaps .. +100 lengthens), Auto Threshold, Input Drive, Output. Settings: plate (Steel Classic,\n"
         "   Gold Foil, Studio Steel, Light Plate, Solid-State Plate, Silver Mist, Iron Mammoth), input (Tube, Solid State, Clean, Tape), lowSlope / highSlope (6, 12, 24),\n"
         "   freezeOn, darkeningOn (a frozen tail keeps darkening), monoInOn (sum the input), keyOn (auto-decay follows the key input). Lite: Decay, Low Cut, Mod, Mix, Output.\n"
         " Radiant: Input (drive into tube 1), Bass, Treble (Baxandall +-12), Output (level and drive into tube 2), Mix, Trim; settings arch (60s Tube Mixer, 50s Broadcast Pre,\n"
         "   Tube Line Amp), micOn (+10 dB), noisyOn (-68 dBu floor), oversample (0-3), msMode. Radiant Pre: Gain, Output, Mix; biasOn (choppy breakup), noisyOn.\n"
         " Maw: Crush, Crunch, Darkness (Hz low-pass), Mix, Release (0.05-30 s), Stretch (release stretches as it saturates), Key HPF, Output; settings fastOn, keyOn, oversample.\n"
         "   Maw Lite: Crush, Crunch, Output.\n"
         " Settings: roomType (Hall/Chamber/Room/Plate/Church/Cathedral/Studio/Ambience or 0-7), freeze, limitMode, meterMode, lowFreq/highFreq/attenFreq/midLowFreq/dipFreq/midHighFreq (Hz - snaps to the\n"
         "   nearest switch position), mode (0 echo / 1 doubler), sync (0 free, 1 1/16, 2 1/8, 3 dotted 1/8, 4 1/4, 5 1/2), poles (0 two / 1 four), envFast, lfoShape (0 sine / 1 square), stages (4, 6, 8 or 12).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"which":{"type":"string"},"action":{"type":"string","enum":["add","get","set","preset","open"]},
            "settings":{"type":"object"},"preset":{"type":"string"}},"required":["track","which","action"]})json",
         [this] (const juce::var& a)
         {
             const auto type = colourTypeFor (str (a, "which"));
             if (type.isEmpty()) return ToolResult::error ("which must be aurora, opto, pulse, oilcan, ladder, ring, phaser, bbd, cinder, wingbeat wingbeatjr, foilplate, foilplatelite, radiant, radiantpre, maw or mawlite");
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
                     // through the session, so the editions apply (a plug-in this copy doesn't own is refused with the product that unlocks it)
                     juce::String why;
                     auto np = entry() != nullptr ? session.createPluginByName (entry()->name, why) : session.edit->getPluginCache().createNewPlugin (type, {});
                     if (np == nullptr) return ToolResult::error (why.isNotEmpty() ? why : juce::String ("Couldn't create the plug-in"));
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
                     // an exact parameter name / ID wins; a known setting comes next; only then a partial parameter name
                     te::AutomatableParameter* exact = nullptr;
                     for (auto ap : p->getAutomatableParameters()) if (ap->paramID.equalsIgnoreCase (key) || ap->getParameterName().equalsIgnoreCase (key)) exact = ap;
                     bool isSetting = knownPluginSettings().contains (key, true);
                     for (int i = 0; i < p->state.getNumProperties() && ! isSetting; ++i) isSetting = p->state.getPropertyName (i).toString().equalsIgnoreCase (key);
                     if (exact != nullptr) { exact->setParameter (exact->getValueRange().clipValue ((float) (double) v), juce::sendNotification); continue; }
                     if (! isSetting) if (auto ap = Signature::findParam (*p, key)) { ap->setParameter (ap->getValueRange().clipValue ((float) (double) v), juce::sendNotification); continue; }
                     // settings with names
                     juce::String id = key;
                     if (key.equalsIgnoreCase ("mode") && v.isString() && dynamic_cast<SqWingbeatBase*> (p) == nullptr) v = SqApexDynamicsPlugin::modeNames().indexOf (v.toString().toUpperCase());
                     if (key.equalsIgnoreCase ("detection") && v.isString()) v = SqApexDynamicsPlugin::detectionNames().indexOf (v.toString().toUpperCase());
                     if (key.equalsIgnoreCase ("driveType") && v.isString()) v = SqApexSubPlugin::driveNames().indexOf (v.toString().toUpperCase());
                     if (key.equalsIgnoreCase ("target") && v.isString()) { int ti = 0; for (int i = 0; i < (int) SqApexLimiterPlugin::targets().size(); ++i) if (juce::String (SqApexLimiterPlugin::targets()[(size_t) i].name).containsIgnoreCase (v.toString())) ti = i; v = ti; }
                     if (key.equalsIgnoreCase ("roomType") && v.isString()) v = juce::jmax (0, SqAuroraPlugin::roomNames().indexOf (v.toString(), true));
                     if ((key.equalsIgnoreCase ("model") || key.equalsIgnoreCase ("modelB")) && v.isString())
                     { int mi = -1; auto names = SqCinderPlugin::modelNames(); for (int i = 0; i < names.size(); ++i) if (names[i].containsIgnoreCase (v.toString()) || v.toString().containsIgnoreCase (names[i])) { mi = i; break; } v = juce::jmax (0, mi); }
                     if (key.equalsIgnoreCase ("style") && v.isString())
                     {
                         const auto names = dynamic_cast<SqWingbeatJrPlugin*> (p) != nullptr ? SqWingbeatJrPlugin::styleNames() : SqWingbeatPlugin::styleNames();
                         int si = -1; for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (v.toString())) si = i;
                         if (si < 0) for (int i = 0; i < names.size(); ++i) if (names[i].containsIgnoreCase (v.toString())) { si = i; break; }
                         v = juce::jmax (0, si);
                     }
                     if ((key.equalsIgnoreCase ("syncL") || key.equalsIgnoreCase ("syncR") || key.equalsIgnoreCase ("sync")) && v.isString())
                     { int si = 0; for (int i = 0; i < Eng::WingMod::numSyncDivisions(); ++i) if (v.toString().trim().equalsIgnoreCase (Eng::WingMod::syncName (i))) si = i + 1; v = si; }
                     if (key.equalsIgnoreCase ("mode") && v.isString() && dynamic_cast<SqWingbeatBase*> (p) != nullptr)
                         v = juce::jmax (0, (dynamic_cast<SqWingbeatJrPlugin*> (p) != nullptr ? SqWingbeatJrPlugin::modeNames() : SqWingbeatPlugin::modeNames()).indexOf (v.toString(), true));
                     if (key.equalsIgnoreCase ("arch") && v.isString()) { int ai = 0; auto nm = SqRadiantPlugin::archNames(); for (int i = 0; i < nm.size(); ++i) if (nm[i].containsIgnoreCase (v.toString())) { ai = i; break; } v = ai; }
                     if (key.equalsIgnoreCase ("plate") && v.isString()) { int pi = 0; auto nm = SqFoilPlatePlugin::archetypeNames(); for (int i = 0; i < nm.size(); ++i) if (nm[i].containsIgnoreCase (v.toString())) { pi = i; break; } v = pi; }
                     if (key.equalsIgnoreCase ("input") && v.isString()) v = juce::jmax (0, SqFoilPlatePlugin::inputNames().indexOf (v.toString(), true));
                     if ((key.equalsIgnoreCase ("lowSlope") || key.equalsIgnoreCase ("highSlope")) && (int) v > 2) v = (int) v >= 24 ? 2 : ((int) v >= 12 ? 1 : 0);
                     if (key.equalsIgnoreCase ("pattern") && v.isString()) v = juce::jmax (0, SqWingbeatPlugin::patternNames().indexOf (v.toString(), true));
                     if (key.equalsIgnoreCase ("tapData") && v.isArray())
                     {
                         juce::StringArray parts;
                         for (auto& t : *v.getArray()) parts.add (juce::String ((double) t["sixteenths"]) + ":" + juce::String (t.hasProperty ("level") ? (double) t["level"] : 0.7) + ":"
                                                                   + juce::String (t.hasProperty ("pan") ? (double) t["pan"] : 0.0) + ":" + juce::String (t.hasProperty ("cutoff") ? (double) t["cutoff"] : 14000.0) + ":"
                                                                   + juce::String (t.hasProperty ("pitch") ? (double) t["pitch"] : 0.0));
                         p->state.setProperty ("tapData", parts.joinIntoString (";"), um); continue;
                     }
                     if (key.equalsIgnoreCase ("msMode") && v.isString()) v = juce::jmax (0, SqCinderPlugin::msNames().indexOf (v.toString(), true));
                     if (key.equalsIgnoreCase ("oversample") && v.isString()) v = juce::jmax (0, SqCinderPlugin::osNames().indexOf (v.toString().trim().toLowerCase(), true));
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
