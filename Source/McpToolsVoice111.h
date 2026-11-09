// Squelchmoth - Claude tools for the 1.11 Voice family, first wave: vocal_fx (Siren Comp, Siren EQ, Hush De-Ess 2,
// Moth Key) and detect_key. Included inside the McpTools class body.

static juce::String voiceTypeFor (const juce::String& k)
{
    const auto s = k.toLowerCase().removeCharacters (" -_.");
    if (s.contains ("verb") || s.contains ("vocalreverb")) return SqSirenVerbPlugin::xmlTypeName;
    if (s.contains ("miccloset") || s.contains ("micmod") || s == "mic" || s.contains ("closet")) return SqMicClosetPlugin::xmlTypeName;
    if (s.contains ("larynx") || s.contains ("throat") || s.contains ("tract")) return SqLarynxPlugin::xmlTypeName;
    if (s.contains ("exhale") || s.contains ("breath")) return SqExhalePlugin::xmlTypeName;
    if (s.contains ("comp")) return SqSirenCompPlugin::xmlTypeName;
    if (s.contains ("eq")) return SqSirenEqPlugin::xmlTypeName;
    if (s.contains ("ess") || s.contains ("hush")) return SqHushDeEss2Plugin::xmlTypeName;
    if (s.contains ("key")) return SqMothKeyPlugin::xmlTypeName;
    return {};
}

/** Everything Siren Comp / Siren EQ / Hush De-Ess 2 / Moth Key keep as settings (so 'set' works before a value was stored). */
static const juce::StringArray& voiceSettings()
{
    static const juce::StringArray s = []
    {
        juce::StringArray k { "model1", "model2", "fet1", "fet2", "dualOn", "autoMakeupOn", "punchOn", "keyOn", "assistAmount", "previewOn",
                              "hpSlope", "lpSlope", "linkOn", "soloOn", "listenOn", "autoApplyOn", "tempoOn",
                              "algo", "reverseOn", "autoEqOn", "delayMode", "delaySync", "shimmerPitch", "mixLockOn", "source", "target", "pattern", "sourceProxOn", "levelMatchOn" };
        for (int b = 1; b <= 6; ++b) for (auto x : { "On", "Type", "Mode", "Harm", "Expand", "Solo", "Mute" }) k.add ("b" + juce::String (b) + x);
        return k;
    }();
    return s;
}

/** Audio for key detection: a file, one track, or every unmuted audio track mixed, between start and end (seconds). */
std::vector<float> keyAudio (const juce::var& a, double& fs, double& offset, juce::String& err)
{
    offset = 0;
    if (has (a, "file"))
    {
        juce::File f (str (a, "file"));
        if (! f.existsAsFile()) { err = "No file at " + f.getFullPathName(); return {}; }
        auto v = MothKeyAnalysis::readFile (f, fs);
        if (v.empty()) err = "Couldn't read " + f.getFileName() + " (WAV, AIFF, FLAC, MP3 or OGG)";
        return v;
    }
    double start = 0, end = session.edit->getLength().inSeconds();
    if (auto t0 = timeArg (a, "start")) start = t0->inSeconds();
    if (auto t1 = timeArg (a, "end")) end = t1->inSeconds();
    if (end <= start + 1.0) { err = "The range is shorter than a second"; return {}; }
    offset = start;
    std::vector<float> mix;
    auto addTrack = [&] (te::AudioTrack& t)
    {
        auto v = MothKeyAnalysis::readTrack (t, start, end, fs);
        if (mix.size() < v.size()) mix.resize (v.size(), 0.0f);
        for (size_t i = 0; i < v.size(); ++i) mix[i] += v[i];
    };
    if (has (a, "track"))
    {
        auto t = trackArg (a, err); if (t == nullptr) return {};
        addTrack (*t);
        if (mix.empty()) err = "No readable audio clips on " + t->getName();
    }
    else
    {
        for (auto t : te::getAudioTracks (*session.edit)) if (! t->isMuted (false)) addTrack (*t);
        if (mix.empty()) err = "No audio clips in the session";
    }
    return mix;
}

void registerVoice111Tools()
{
    add ("detect_key",
         "Moth Key's analysis: the key, scale and tempo of audio, with key changes over time, confidence and alternative keys.\n"
         " Source: file (path to WAV / AIFF / FLAC / MP3), or track (one track's audio clips), or nothing (every unmuted audio track mixed).\n"
         " Range: start_bar / end_bar (or start_seconds / end_seconds), default the whole session.\n"
         " apply (default true): writes the key and its changes to the session's key track (edit key + key map) and points every Moth Tune at it\n"
         " (auto key on, so they follow key changes as the song plays). tempo true also sets the session tempo when the beat is clear.",
         R"json({"type":"object","properties":{"file":{"type":"string"},"track":{"type":["string","integer"]},"start_bar":{"type":"number"},"end_bar":{"type":"number"},
            "start_seconds":{"type":"number"},"end_seconds":{"type":"number"},"apply":{"type":"boolean"},"tempo":{"type":"boolean"}}})json",
         [this] (const juce::var& a)
         {
             juce::String err; double fs = 11025, offset = 0;
             auto audio = keyAudio (a, fs, offset, err);
             if (audio.empty()) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("No audio to analyse"));
             if (audio.size() < (size_t) (fs * 4)) return ToolResult::error ("Need at least 4 seconds of audio");
             Voice::KeyFinder kf; const auto r = kf.analyse (audio.data(), audio.size(), fs);
             auto out = MothKeyAnalysis::toVar (r, has (a, "file") ? 0.0 : offset);
             if (flag (a, "apply", true) && ! has (a, "file"))
             {
                 auto* um = &session.edit->getUndoManager();
                 um->beginNewTransaction ("Detect key");
                 MothKeyAnalysis::writeToSession (*session.edit, r, offset, flag (a, "tempo"));
                 juce::StringArray tunes;
                 for (auto t : te::getAudioTracks (*session.edit))
                     for (auto p : t->pluginList)
                         if (auto tu = dynamic_cast<SqTunePlugin*> (p))
                         { tu->keyRoot.setValue (r.root, um); tu->scaleIndex.setValue (r.minor ? 2 : 1, um); tu->autoKey.setValue (true, um); tunes.add (t->getName()); }
                 set (out, "applied_to_session", true);
                 set (out, "moth_tunes_following", tunes.joinIntoString (", "));
             }
             else if (flag (a, "apply", true) && has (a, "file"))
             {
                 auto* um = &session.edit->getUndoManager();
                 session.edit->state.setProperty ("sqmKeyRoot", r.root, um); session.edit->state.setProperty ("sqmKeyScale", r.minor ? 2 : 1, um);
                 set (out, "applied_to_session", true);
             }
             return ToolResult::ok (out);
         });

    add ("vocal_fx",
         "The 1.11 Voice plug-ins. which = sirencomp (Siren Comp, vocal compressor, 110 presets) | sireneq (Siren EQ, pitch-tracking dynamic vocal EQ, 100 presets) |\n"
         " deess2 (Hush De-Ess 2, Hard / Soft de-esser, 60 presets) | mothkey (Moth Key, key / scale / tempo detection) |\n"
         " sirenverb (Siren Verb, vocal reverb, 140 presets) | miccloset (Mic Closet, microphone modelling, 80 presets) | larynx (Larynx, vocal-tract modeller, 60 presets) |\n"
         " exhale (Exhale, breathiness control, 30 presets).\n"
         " Siren Verb: Size, Decay (s), Pre-Delay, Damping, Early, Mod Rate, Mod Depth, Width, Low Cut, High Cut, Mix, Output, Pre Pitch (st), Throat, Tube, De-Ess, Reverse Time,\n"
         "   Auto-EQ Depth, Tone, Compressor, Gate, Ducking, Shimmer, Echo Time / Feedback / Level; settings algo (Hall, Plate, Room, Chamber, Spring, Ambience, Shimmer),\n"
         "   reverseOn, autoEqOn (cuts the singer's pitch out of the tail), delayMode (Off, Mono, Ping-Pong), delaySync (0 free, 1-27 division), shimmerPitch (+7, +12, +19, +24, -12 st),\n"
         "   mixLockOn (presets keep your Mix). Actions assist (vibe: Intimate, Pop Sheen, Ballad, Epic, Dark, Retro - from the tempo and the voice it heard) and dice.\n"
         " Mic Closet: Distance (cm), Low Cut, Pad, Colour, Self-Noise, Mix, Output; settings source / target (microphone name or index - 'preset list' shows presets;\n"
         "   get lists the microphones), pattern (Mic's Own, Cardioid, Omni, Figure-8, Supercardioid), sourceProxOn (the source was recorded at Distance too).\n"
         " Larynx: Tract Length (%), Glottis, Lower Throat, Upper Throat, Pharynx, Back Tongue, Mid Tongue, Front Tongue, Lips (-100..100 narrower / wider), Glottal Hardness,\n"
         "   Pulse Width, Breathiness, Breath Freq, Precision, Mix, Output; settings levelMatchOn. Exhale: Breath (dB, -60 = none), Breath Tone, Consonants, Body, Mix, Output.\n"
         " Actions: add (optional preset), get (parameters, settings, live meters), set {name: value}, preset (name or 'list'), open,\n"
         " learn_start / learn_stop (Assist or Vocal Learning: play the vocal between them; learn_stop returns the suggestion), preview true/false (audition the suggestion\n"
         " before committing - A/B by toggling), apply (commit the suggestion to the controls), amount (Siren Comp Assist: minimal, controlled, aggressive),\n"
         " analyse (Moth Key: what it heard, or a track with track_source), apply_key (Moth Key: write its last result to the session).\n"
         " Siren Comp: Threshold 1/2, Attack 1/2 (ms), Release 1/2 (ms), Ratio 1/2 (Modern), Knee, Warm, Mix, Make-Up, Output, Punch Gain, Impact, Ceiling, Duck Depth;\n"
         "   settings model1 / model2 (FET, Opto A, Opto B, Modern or 0-3), fet1 / fet2 (4:1, 8:1, 12:1, 20:1, All or 0-4), dualOn (series second stage), autoMakeupOn,\n"
         "   punchOn, keyOn (the key input is the vocal: the pitch filter ducks this track at the vocal's harmonics - set the key with 'key').\n"
         " Siren EQ: Band 1-6 Freq / Gain / Q / Threshold / Range, Low Cut, High Cut, Air Freq (2.5-40 kHz), Air, Tilt, Output;\n"
         "   settings bNOn, bNType (Bell, Low Shelf, High Shelf, Notch), bNMode (Fixed, Pitch - locked to harmonic bNHarm of the singer, Dynamic), bNHarm (1-16),\n"
         "   bNExpand (dynamic band expands instead of cutting), bNSolo, bNMute, hpSlope / lpSlope (6, 12, 18, 24 dB/oct).\n"
         " Hush De-Ess 2: Split (Hz), Hard Threshold, Soft Threshold, Hard Range, Soft Range, Sensitivity, Output; settings linkOn (one threshold moves both), soloOn (hear what is removed).\n"
         " Moth Key: Listen Length (s); settings listenOn, autoApplyOn (write each result to the session), tempoOn (also set the tempo).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"which":{"type":"string"},
            "action":{"type":"string","enum":["add","get","set","preset","open","key","learn_start","learn_stop","preview","apply","analyse","apply_key","assist","dice"]},"vibe":{"type":"string"},
            "settings":{"type":"object"},"preset":{"type":"string"},"on":{"type":"boolean"},"amount":{"type":"string"},"source":{"type":"string"},
            "track_source":{"type":["string","integer"]},"start_bar":{"type":"number"},"end_bar":{"type":"number"}},"required":["track","which","action"]})json",
         [this] (const juce::var& a)
         {
             const auto type = voiceTypeFor (str (a, "which"));
             if (type.isEmpty()) return ToolResult::error ("which must be sirencomp, sireneq, deess2, mothkey, sirenverb, miccloset, larynx or exhale");
             const auto action = str (a, "action").toLowerCase();
             juce::String err; te::AudioTrack* t = nullptr;
             auto list = pluginListArg (a, err, t);
             if (list == nullptr) return ToolResult::error (err);
             te::Plugin* p = nullptr; for (auto pl : *list) if (pl->getPluginType() == type) { p = pl; break; }
             auto describe = [&] (te::Plugin& pl)
             {
                 auto o = describeApex (pl);
                 if (auto c = dynamic_cast<SqSirenCompPlugin*> (&pl))
                 {
                     set (o, "stage_1", SqSirenCompPlugin::modelNames()[juce::jlimit (0, 3, c->model1.get())] + (c->model1.get() == 0 ? " " + SqSirenCompPlugin::fetNames()[juce::jlimit (0, 4, c->fet1.get())] : juce::String()));
                     if (c->dual.get()) set (o, "stage_2", SqSirenCompPlugin::modelNames()[juce::jlimit (0, 3, c->model2.get())] + (c->model2.get() == 0 ? " " + SqSirenCompPlugin::fetNames()[juce::jlimit (0, 4, c->fet2.get())] : juce::String()));
                     set (o, "gain_reduction_1_db", c->gr1.load()); set (o, "gain_reduction_2_db", c->gr2.load()); set (o, "punch_gr_db", c->punchGr.load()); set (o, "output_peak_dbfs", c->outDb.load());
                     set (o, "learning", c->learning.load()); set (o, "has_suggestion", c->hasSuggestion()); set (o, "previewing", c->preview.get());
                     if (c->hasSuggestion())
                     {
                         const auto s = c->suggestionCopy(); auto so = obj();
                         set (so, "amount", SqSirenCompPlugin::amountNames()[juce::jlimit (0, 2, c->amount.get())].toLowerCase());
                         set (so, "stage_1", juce::String (Voice::Comp::modelName (s.s1.model)) + " threshold " + juce::String (s.s1.thresholdDb, 1) + " dB");
                         if (s.dual) set (so, "stage_2", juce::String (Voice::Comp::modelName (s.s2.model)) + " threshold " + juce::String (s.s2.thresholdDb, 1) + " dB");
                         set (so, "make_up_db", s.makeupDb); set (o, "suggestion", so);
                     }
                     if (c->keyOn.get()) set (o, "key_pitch_hz", c->keyHz.load());
                 }
                 if (auto e = dynamic_cast<SqSirenEqPlugin*> (&pl))
                 {
                     set (o, "singer_pitch_hz", e->pitchHz.load());
                     juce::Array<juce::var> bands;
                     for (int b = 0; b < 6; ++b)
                     {
                         if (! e->on[(size_t) b].get()) continue;
                         auto bo = obj(); set (bo, "band", b + 1); set (bo, "type", SqSirenEqPlugin::typeNames()[juce::jlimit (0, 3, e->type[(size_t) b].get())].toLowerCase());
                         set (bo, "mode", SqSirenEqPlugin::modeNames()[juce::jlimit (0, 2, e->mode[(size_t) b].get())].toLowerCase());
                         set (bo, "frequency_now_hz", e->bandHz[(size_t) b].load()); set (bo, "dynamic_gain_db", e->bandDyn[(size_t) b].load());
                         bands.add (bo);
                     }
                     set (o, "bands_on", bands); set (o, "learning", e->learning.load()); set (o, "has_suggestion", e->hasSuggestion());
                     if (e->hasSuggestion()) set (o, "suggestion", e->suggestionText.get());
                 }
                 if (auto d = dynamic_cast<SqHushDeEss2Plugin*> (&pl))
                 {
                     set (o, "gain_reduction_db", d->grDb.load()); set (o, "detecting", juce::StringArray { "none", "hard", "soft" }[juce::jlimit (0, 2, d->kindNow.load())]);
                     set (o, "hard_events", d->proc.countHard); set (o, "soft_events", d->proc.countSoft);
                     set (o, "learning", d->learning.load()); set (o, "has_suggestion", d->hasSuggestion());
                     if (d->hasSuggestion()) { set (o, "suggested_hard_threshold_db", d->suggestedHard()); set (o, "suggested_soft_threshold_db", d->suggestedSoft()); }
                 }
                 if (auto sv = dynamic_cast<SqSirenVerbPlugin*> (&pl))
                 {
                     const auto kp = sv->knobParams();
                     set (o, "algorithm", SirenVerb::algoName (kp.algo)); set (o, "singer_pitch_hz", sv->proc.liveHz()); set (o, "auto_eq", kp.autoEq);
                     set (o, "tail_compressor_gr_db", sv->proc.compGrDb()); set (o, "mix_locked", sv->mixLockOn.get());
                     set (o, "echo", SqSirenVerbPlugin::delayNames()[juce::jlimit (0, 2, kp.delayMode)]);
                 }
                 if (auto mc = dynamic_cast<SqMicClosetPlugin*> (&pl))
                 {
                     const auto kp = mc->knobParams();
                     set (o, "source_mic", MicCloset::mics()[(size_t) kp.source].name); set (o, "target_mic", MicCloset::mics()[(size_t) kp.target].name);
                     MicCloset::Processor tmp; tmp.p = kp; tmp.prepare (48000.0, 256);     // the settings as they are now, playing or not
                     set (o, "pattern", MicCloset::patternName (tmp.effectivePattern()));
                     juce::String resp; for (double hz : { 60.0, 120.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 12000.0 }) resp << juce::String ((int) hz) << " Hz " << juce::String (tmp.responseDb (hz), 1) << " dB, ";
                     set (o, "response", resp.dropLastCharacters (2));
                     juce::StringArray all; for (int i = 0; i < MicCloset::numMics(); ++i) all.add (juce::String (i) + " " + MicCloset::mics()[(size_t) i].name);
                     set (o, "microphones", all.joinIntoString ("; "));
                 }
                 if (auto lx = dynamic_cast<SqLarynxPlugin*> (&pl)) { set (o, "latency_ms", lx->getLatencySeconds() * 1000.0); set (o, "level_match", lx->levelMatchOn.get()); }
                 if (auto ex = dynamic_cast<SqExhalePlugin*> (&pl))
                 { set (o, "voiced", ex->proc.voiced()); set (o, "harmonic_to_noise_db", ex->proc.hnrDb()); set (o, "breath_gain_db", ex->proc.breathGainDb()); set (o, "latency_ms", ex->getLatencySeconds() * 1000.0); }
                 if (auto k = dynamic_cast<SqMothKeyPlugin*> (&pl))
                 {
                     set (o, "heard_seconds", k->heardSeconds()); set (o, "status", k->status()); set (o, "analysing", k->isAnalysing());
                     if (k->hasResult()) set (o, "result", MothKeyAnalysis::toVar (k->lastResult(), juce::jmax (0.0, k->lastOffset())));
                     else if (k->resultJson.get().isNotEmpty()) set (o, "result", juce::JSON::parse (k->resultJson.get()));
                 }
                 return o;
             };
             if (action == "add" || action == "get" || action == "open" || action == "preset" || action == "key")
             {
                 auto r = runSeriesTool (a, type);
                 if (r.isError || action == "open" || action == "preset" || action == "key") return r;
                 for (auto pl : *list) if (pl->getPluginType() == type) return ToolResult::ok (describe (*pl));
                 return r;
             }
             if (p == nullptr) return ToolResult::error ("Not on that track - use action add");
             auto* um = &session.edit->getUndoManager();
             if (action == "set")
             {
                 auto s = a["settings"]; if (! s.isObject()) return ToolResult::error ("settings must be an object");
                 juce::StringArray problems;
                 auto pickName = [] (const juce::StringArray& names, const juce::var& v) { if (! v.isString()) return (int) v; for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (v.toString().trim())) return i; for (int i = 0; i < names.size(); ++i) if (names[i].containsIgnoreCase (v.toString().trim())) return i; return -1; };
                 for (auto& pr : s.getDynamicObject()->getProperties())
                 {
                     const auto key = pr.name.toString(); auto v = pr.value;
                     te::AutomatableParameter* exact = nullptr;
                     for (auto ap : p->getAutomatableParameters()) if (ap->paramID.equalsIgnoreCase (key) || ap->getParameterName().equalsIgnoreCase (key)) exact = ap;
                     if (exact != nullptr) { exact->setParameter (exact->getValueRange().clipValue ((float) (double) v), juce::sendNotification); continue; }
                     juce::String id; for (auto& k : voiceSettings()) if (k.equalsIgnoreCase (key)) id = k;
                     if (id.isEmpty()) { if (auto ap = Signature::findParam (*p, key)) { ap->setParameter (ap->getValueRange().clipValue ((float) (double) v), juce::sendNotification); continue; } problems.add ("unknown setting '" + key + "'"); continue; }
                     if (id == "algo") v = pickName (SqSirenVerbPlugin::algoNames(), v);
                     else if (id == "delayMode") v = pickName (SqSirenVerbPlugin::delayNames(), v);
                     else if (id == "shimmerPitch") { if (v.isString()) v = pickName (SqSirenVerbPlugin::shimmerNames(), v.toString().replace ("st", "").trim() + " st"); }
                     else if (id == "source" || id == "target") v = pickName (SqMicClosetPlugin::micNames(), v);
                     else if (id == "pattern") v = pickName (SqMicClosetPlugin::patternNames(), v);
                     else if (id.startsWith ("model")) v = pickName (SqSirenCompPlugin::modelNames(), v.isString() ? juce::var (v.toString().toUpperCase().replace ("OPTOA", "OPTO A").replace ("OPTOB", "OPTO B")) : v);
                     else if (id.startsWith ("fet")) { if (v.isString()) v = pickName (SqSirenCompPlugin::fetNames(), v.toString().toUpperCase()); else if ((int) v > 4) { const int r = (int) v; v = r >= 30 ? 4 : r >= 20 ? 3 : r >= 12 ? 2 : r >= 8 ? 1 : 0; } }
                     else if (id.endsWith ("Type")) v = pickName (SqSirenEqPlugin::typeNames(), v);
                     else if (id.endsWith ("Mode")) v = pickName (SqSirenEqPlugin::modeNames(), v.isString() && v.toString().containsIgnoreCase ("track") ? juce::var ("PITCH") : v);
                     else if (id.endsWith ("Slope")) { const int sl = (int) v; v = sl >= 6 ? juce::jlimit (0, 3, sl / 6 - 1) : sl; }
                     else if (id == "assistAmount") v = pickName (SqSirenCompPlugin::amountNames(), v);
                     if (v.isBool()) v = (bool) v;
                     if (! v.isBool() && (int) v < 0) { problems.add ("bad value for " + key); continue; }
                     p->state.setProperty (id, v, (id.endsWith ("Solo") || id == "soloOn" || id == "previewOn") ? nullptr : um);
                 }
                 auto out = describe (*p);
                 return problems.isEmpty() ? ToolResult::ok (out) : ToolResult::error (problems.joinIntoString ("; ") + "\n" + juce::JSON::toString (out));
             }
             auto amountArg = [&] { const auto s = str (a, "amount").toLowerCase(); return s.startsWith ("min") ? 0 : s.startsWith ("agg") ? 2 : 1; };
             if (auto c = dynamic_cast<SqSirenCompPlugin*> (p))
             {
                 if (action == "learn_start") { c->startLearning(); return ToolResult::ok ("Siren Comp Assist is listening - play the vocal (10-30 s is plenty), then learn_stop"); }
                 if (action == "learn_stop") { c->stopLearning(); c->makeSuggestion (has (a, "amount") ? amountArg() : c->amount.get()); c->preview.setValue (true, nullptr); return ToolResult::ok (describe (*c)); }
                 if (action == "preview") { if (has (a, "amount")) c->makeSuggestion (amountArg()); c->preview.setValue (flag (a, "on", true) && c->hasSuggestion(), nullptr); return ToolResult::ok (describe (*c)); }
                 if (action == "apply") { if (! c->hasSuggestion()) return ToolResult::error ("No suggestion yet - learn_start, play, learn_stop"); um->beginNewTransaction ("Siren Comp: apply Assist"); c->applySuggestion(); return ToolResult::ok (describe (*c)); }
             }
             if (auto e = dynamic_cast<SqSirenEqPlugin*> (p))
             {
                 if (action == "learn_start") { e->startLearning(); return ToolResult::ok ("Vocal Learning is listening - play the vocal, then learn_stop"); }
                 if (action == "learn_stop") { const auto txt = e->finishLearning(); if (e->hasSuggestion()) e->preview.setValue (true, nullptr); auto o = describe (*e); set (o, "learned", txt); return ToolResult::ok (o); }
                 if (action == "preview") { e->preview.setValue (flag (a, "on", true) && e->hasSuggestion(), nullptr); return ToolResult::ok (describe (*e)); }
                 if (action == "apply") { if (! e->hasSuggestion()) return ToolResult::error ("No suggestion yet - learn_start, play, learn_stop"); um->beginNewTransaction ("Siren EQ: apply Vocal Learning"); e->applySuggestion(); return ToolResult::ok (describe (*e)); }
             }
             if (auto d = dynamic_cast<SqHushDeEss2Plugin*> (p))
             {
                 if (action == "learn_start") { d->startLearning(); return ToolResult::ok ("Hush Assist is listening - play the vocal, then learn_stop"); }
                 if (action == "learn_stop") { auto o = describe (*d); set (o, "learned", d->finishLearning()); d->preview.setValue (true, nullptr); return ToolResult::ok (describe (*d)); }
                 if (action == "preview") { d->preview.setValue (flag (a, "on", true) && d->hasSuggestion(), nullptr); return ToolResult::ok (describe (*d)); }
                 if (action == "apply") { if (! d->hasSuggestion()) return ToolResult::error ("No suggestion yet - learn_start, play, learn_stop"); um->beginNewTransaction ("Hush: apply Assist"); d->applySuggestion(); return ToolResult::ok (describe (*d)); }
             }
             if (auto sv = dynamic_cast<SqSirenVerbPlugin*> (p))
             {
                 if (action == "assist")
                 {
                     const int vi = juce::jmax (0, SqSirenVerbPlugin::vibeNames().indexOf (str (a, "vibe", "Pop Sheen"), true));
                     um->beginNewTransaction ("Siren Verb: Assist"); sv->assist (vi, um);
                     auto o = describe (*sv); set (o, "assist", SqSirenVerbPlugin::vibeNames()[vi] + " (undo to go back, or A/B in the window)"); return ToolResult::ok (o);
                 }
                 if (action == "dice") { um->beginNewTransaction ("Siren Verb: Dice"); sv->dice (um); return ToolResult::ok (describe (*sv)); }
             }
             if (auto k = dynamic_cast<SqMothKeyPlugin*> (p))
             {
                 if (action == "analyse")
                 {
                     bool ok = false;
                     if (has (a, "track_source"))
                     {
                         juce::var a2 (new juce::DynamicObject()); a2.getDynamicObject()->setProperty ("track", a["track_source"]);
                         auto src = trackArg (a2, err); if (src == nullptr) return ToolResult::error (err);
                         double s0 = 0, s1 = session.edit->getLength().inSeconds();
                         if (auto t0 = timeArg (a, "start")) s0 = t0->inSeconds();
                         if (auto t1 = timeArg (a, "end")) s1 = t1->inSeconds();
                         ok = k->analyseTrack (*src, s0, s1);
                     }
                     else ok = k->analyseHeard();
                     if (! ok) return ToolResult::error (k->status());
                     for (int i = 0; i < 400 && k->isAnalysing(); ++i) juce::Thread::sleep (10);   // usually well under a second
                     return ToolResult::ok (describe (*k));
                 }
                 if (action == "apply_key") { if (! k->hasResult()) return ToolResult::error ("Nothing analysed yet"); um->beginNewTransaction ("Moth Key: apply"); k->applyToSession(); return ToolResult::ok ("Session key: " + MothKeyAnalysis::keyText (k->lastResult().root, k->lastResult().minor)); }
             }
             return ToolResult::error ("unknown action for " + p->getName());
         });
}
