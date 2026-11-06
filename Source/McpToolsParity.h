// Squelchmoth - Claude tools for the Pro Tools-parity layer. Included inside the McpTools class body.
// (edit modes/tools, Edit selection, separate/cut/copy/paste/duplicate/repeat/fade/strip/consolidate/heal/nudge,
//  markers, automation modes, elastic audio, folders/VCAs, groups, playlists/takes, clip gain, AudioSuite,
//  record modes, pre/post-roll, count-off, windows, theme & colours, preferences, templates, track presets)

void registerParityTools()
{
    auto st = [this] () -> TimelineState* { return ui.state; };
    auto needUi = [] { return ToolResult::error ("The Edit window is not available yet."); };
    auto refresh = [this] { if (ui.refresh) ui.refresh(); };

    //==========================================================================
    add ("set_edit_mode", "Set the Pro Tools edit mode: shuffle, slip, spot or grid (optionally relative grid). Shuffle closes gaps and butts clips together; Grid snaps everything to the grid value.",
         R"json({"type":"object","properties":{"mode":{"type":"string","enum":["shuffle","slip","spot","grid"]},"relative_grid":{"type":"boolean"},"grid":{"type":"string","description":"grid value name e.g. '1 bar','1/4 note','1/8 note','1/16 note','1/8 triplet','1 second','10 ms','Auto (follows zoom)'"},"nudge":{"type":"string","description":"nudge value name, same names as grid"},"snap":{"type":"boolean"},"grid_combined":{"type":"boolean","description":"Pro Tools Shift-click: Shuffle or Slip combined with Grid (moves snap to the grid while keeping Shuffle/Slip behaviour)"},"shuffle_lock":{"type":"boolean","description":"Pro Tools Shuffle Lock: when on, Shuffle mode can't be entered by accident"}},"required":[]})json",
         [this, st, needUi, refresh] (const juce::var& a)
         {
             auto s = st(); if (s == nullptr) return needUi();
             if (has (a, "shuffle_lock"))
             {
                 s->shuffleLock = flag (a, "shuffle_lock");
                 if (s->shuffleLock && s->editMode == EditMode::shuffle && ! has (a, "mode")) s->editMode = EditMode::slip;
             }
             if (has (a, "mode"))
             {
                 auto m = str (a, "mode").toLowerCase();
                 int idx = m == "shuffle" ? 0 : m == "slip" ? 1 : m == "spot" ? 2 : 3;
                 if (idx == 0 && s->shuffleLock) return ToolResult::error ("Shuffle is locked (Shuffle Lock). Pass shuffle_lock:false to unlock it first.");
                 if (ui.setEditMode) ui.setEditMode (idx); else s->editMode = (EditMode) idx;
             }
             if (has (a, "relative_grid")) s->gridRelative = flag (a, "relative_grid");
             if (has (a, "snap")) s->snap = flag (a, "snap");
             if (has (a, "grid_combined")) s->snap = flag (a, "grid_combined") && s->editMode != EditMode::grid;
             auto findGrid = [] (const juce::String& name) { for (int i = 0; i < (int) std::size (gridValues); ++i) if (juce::String (gridValues[i].name).equalsIgnoreCase (name) || juce::String (gridValues[i].name).startsWithIgnoreCase (name)) return i; return -1; };
             if (has (a, "grid")) { auto i = findGrid (str (a, "grid")); if (i < 0) return ToolResult::error ("Unknown grid value. Use e.g. '1 bar', '1/4 note', '1/16 note', '1/8 triplet', '1 second', '10 ms'."); s->gridIndex = i; }
             if (has (a, "nudge")) { auto i = findGrid (str (a, "nudge")); if (i < 0) return ToolResult::error ("Unknown nudge value."); s->nudgeIndex = i; }
             refresh();
             return ToolResult::ok (juce::String ("Edit mode: ") + editModeName (s->editMode) + (s->gridRelative ? " (relative)" : "") + ", grid: " + gridValues[s->gridIndex].name + ", nudge: " + gridValues[s->nudgeIndex].name + ", grid combined " + (s->snap ? "on" : "off") + ", shuffle lock " + (s->shuffleLock ? "on" : "off"));
         });

    add ("set_tool", "Select the edit tool the user sees: zoomer, trim, selector, grabber, scrubber, pencil or smart.",
         R"json({"type":"object","properties":{"tool":{"type":"string","enum":["zoomer","trim","selector","grabber","scrubber","pencil","smart"]}},"required":["tool"]})json",
         [this, st, needUi] (const juce::var& a)
         {
             auto s = st(); if (s == nullptr) return needUi();
             static const juce::StringArray names { "zoomer", "trim", "selector", "grabber", "scrubber", "pencil", "smart" };
             int idx = names.indexOf (str (a, "tool").toLowerCase());
             if (idx < 0) return ToolResult::error ("Unknown tool");
             if (ui.setTool) ui.setTool (idx); else s->tool = (EditTool) idx;
             return ToolResult::ok (juce::String ("Tool: ") + editToolName (s->tool));
         });

    //==========================================================================
    add ("set_selection", "Make the Edit selection: a time range (bars or seconds) on one or more tracks, and/or select clips by name. Most edit commands (separate, clear, duplicate, fade, consolidate, bounce) act on it. Omit range to just select tracks/clips; clear=true removes the selection.",
         R"json({"type":"object","properties":{"start_bar":{"type":"number"},"end_bar":{"type":"number"},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"},"tracks":{"type":"array","items":{"type":["string","integer"]}},"clips":{"type":"array","items":{"type":"string"},"description":"clip names (on the given tracks, or anywhere)"},"all_clips_in_range":{"type":"boolean"},"clear":{"type":"boolean"},"link_loop":{"type":"boolean","description":"also set the loop range to this selection (default true)"}}})json",
         [this, st, needUi, refresh] (const juce::var& a)
         {
             auto s = st(); if (s == nullptr) return needUi();
             if (flag (a, "clear")) { s->deselectClips(); s->timeSelection.reset(); s->selectedTrackIDs.clear(); refresh(); return ToolResult::ok ("Selection cleared"); }
             juce::StringArray report;
             juce::Array<te::AudioTrack*> tracks;
             if (auto arr = a["tracks"].getArray()) for (auto& tv : *arr) if (auto t = session.findTrack (tv)) tracks.add (t);
             if (! tracks.isEmpty())
             {
                 s->selectedTrackIDs.clear();
                 for (auto t : tracks) s->selectedTrackIDs.push_back (t->itemID);
                 s->selectedRow = tracks.getFirst(); s->selectedTrack = tracks.getFirst();
                 report.add (juce::String (tracks.size()) + " track(s)");
             }
             auto start = timeArg (a, "start"), end = timeArg (a, "end");
             if (start || end)
             {
                 auto sPos = start.value_or (edit().getTransport().getPosition());
                 auto ePos = end.value_or (sPos);
                 if (ePos < sPos) std::swap (sPos, ePos);
                 s->timeSelection = te::TimeRange (sPos, ePos);
                 if (s->selectedTrackIDs.empty()) for (auto t : session.getTracks()) s->selectedTrackIDs.push_back (t->itemID);
                 if (flag (a, "link_loop", true) && ePos > sPos) edit().getTransport().setLoopRange (*s->timeSelection);
                 report.add (session.barsBeatsString (sPos) + " to " + session.barsBeatsString (ePos));
                 if (flag (a, "all_clips_in_range"))
                 {
                     s->deselectClips();
                     for (auto t : EditOps::targetTracks (session, *s)) for (auto& c : EditOps::clipsIn (*t, *s->timeSelection)) s->selectClip (c, true);
                     report.add (juce::String ((int) s->selectedClips.size()) + " clip(s) in range");
                 }
             }
             if (auto arr = a["clips"].getArray())
             {
                 s->deselectClips();
                 auto searchTracks = tracks.isEmpty() ? session.getTracks() : tracks;
                 for (auto& cv : *arr)
                     for (auto t : searchTracks)
                         for (auto c : t->getClips())
                             if (c->getName().equalsIgnoreCase (cv.toString()) || c->getName().containsIgnoreCase (cv.toString())) { s->selectClip (c, true); if (s->selectedTrackIDs.empty()) s->selectTrack (t, true); }
                 report.add (juce::String ((int) s->selectedClips.size()) + " clip(s) selected");
                 if (! s->selectedClips.empty() && ! (start || end))
                 {
                     auto r = s->selectedClips.front()->getPosition().time;
                     for (auto& c : s->selectedClips) r = r.getUnionWith (c->getPosition().time);
                     s->timeSelection = r;
                 }
             }
             refresh();
             return ToolResult::ok ("Selected: " + report.joinIntoString (", "));
         });

    add ("get_selection", "Describe the current Edit selection: time range, selected tracks and clips, edit mode and tool.",
         R"json({"type":"object","properties":{}})json",
         [this, st, needUi] (const juce::var&)
         {
             auto s = st(); if (s == nullptr) return needUi();
             auto o = obj();
             set (o, "edit_mode", editModeName (s->editMode)); set (o, "tool", editToolName (s->tool));
             set (o, "grid", gridValues[s->gridIndex].name); set (o, "nudge", gridValues[s->nudgeIndex].name);
             if (s->timeSelection) { set (o, "start", session.describeTime (s->timeSelection->getStart())); set (o, "end", session.describeTime (s->timeSelection->getEnd())); set (o, "length_seconds", s->timeSelection->getLength().inSeconds()); }
             juce::Array<juce::var> tr, cl;
             for (auto t : session.getRows()) if (s->isTrackSelected (t)) tr.add (t->getName());
             for (auto& c : s->selectedClips) cl.add (c->getName() + " @ " + session.barsBeatsString (c->getPosition().getStart()));
             set (o, "tracks", tr); set (o, "clips", cl);
             return ToolResult::ok (o);
         });

    //==========================================================================
    add ("edit_command", "Run a Pro Tools edit command on the current selection (make one with set_selection first): separate, clear, delete_close_gap, cut, copy, paste, duplicate, repeat (times), fade (length_seconds), remove_fades, trim_to_selection, trim_start_to_playhead, trim_end_to_playhead, strip_silence (threshold_db, min_length_seconds, pad_seconds), consolidate, heal, nudge (direction -1/1, by_grid), select_all, insert_silence, separate_at_transients (sensitivity 0-1), mute_clips, normalize_clip_gain, reverse.",
         R"json({"type":"object","properties":{"command":{"type":"string"},"times":{"type":"integer"},"length_seconds":{"type":"number"},"threshold_db":{"type":"number"},"min_length_seconds":{"type":"number"},"pad_seconds":{"type":"number"},"direction":{"type":"integer"},"by_grid":{"type":"boolean"},"sensitivity":{"type":"number"}},"required":["command"]})json",
         [this, st, needUi, refresh] (const juce::var& a)
         {
             auto s = st(); if (s == nullptr) return needUi();
             auto cmd = str (a, "command").toLowerCase().replaceCharacter (' ', '_');
             int n = 0; juce::String err;
             if (cmd == "separate" || cmd == "split") n = EditOps::separate (session, *s);
             else if (cmd == "clear" || cmd == "delete") n = EditOps::deleteSelection (session, *s, false);
             else if (cmd == "delete_close_gap" || cmd == "shuffle_delete") n = EditOps::deleteSelection (session, *s, true);
             else if (cmd == "cut") n = EditOps::cut (session, *s) ? 1 : 0;
             else if (cmd == "copy") n = EditOps::copy (session, *s) ? 1 : 0;
             else if (cmd == "paste") n = EditOps::paste (session, *s) ? 1 : 0;
             else if (cmd == "duplicate") n = EditOps::duplicate (session, *s, 1);
             else if (cmd == "repeat") n = EditOps::duplicate (session, *s, juce::jlimit (1, 500, (int) num (a, "times", 2)));
             else if (cmd == "fade") n = EditOps::fade (session, *s, num (a, "length_seconds", -1));
             else if (cmd == "remove_fades") { for (auto& c : s->selectedClips) if (auto ac = dynamic_cast<te::AudioClipBase*> (c.get())) { ac->setFadeIn ({}); ac->setFadeOut ({}); ++n; } }
             else if (cmd == "trim_to_selection") n = EditOps::trimToSelection (session, *s);
             else if (cmd == "trim_start_to_playhead") n = EditOps::trimToPlayhead (session, *s, true);
             else if (cmd == "trim_end_to_playhead") n = EditOps::trimToPlayhead (session, *s, false);
             else if (cmd == "strip_silence") n = EditOps::stripSilence (session, *s, has (a, "threshold_db") ? num (a, "threshold_db") : -1000, num (a, "min_length_seconds", -1), num (a, "pad_seconds", -1));
             else if (cmd == "consolidate") n = EditOps::consolidate (session, *s, err);
             else if (cmd == "heal") n = EditOps::heal (session, *s);
             else if (cmd == "nudge") n = EditOps::nudge (session, *s, (int) num (a, "direction", 1) >= 0 ? 1 : -1, flag (a, "by_grid"));
             else if (cmd == "select_all") { EditOps::selectAll (session, *s); n = (int) s->selectedClips.size(); }
             else if (cmd == "insert_silence") { if (s->timeSelection && s->timeSelection->getLength() > 0.001s) { te::insertSpaceIntoEdit (edit(), *s->timeSelection); n = 1; } else err = "Needs a time selection"; }
             else if (cmd == "mute_clips") { for (auto& c : s->selectedClips) { c->setMuted (! c->isMuted()); ++n; } }
             else if (cmd == "reverse") { for (auto& c : s->selectedClips) if (auto ac = dynamic_cast<te::AudioClipBase*> (c.get())) { ac->setIsReversed (! ac->getIsReversed()); ++n; } }
             else if (cmd == "separate_at_transients")
             {
                 for (auto& c : s->selectedClips)
                     if (auto wc = dynamic_cast<te::WaveAudioClip*> (c.get()))
                         if (auto t = dynamic_cast<te::ClipTrack*> (wc->getClipTrack()))
                         {
                             auto pts = Transients::find (session.getEngine(), *wc, (float) num (a, "sensitivity", 0.5));
                             te::Clip::Ptr cur (wc);
                             for (auto p : pts)
                             {
                                 if (p <= cur->getPosition().getStart().inSeconds() + 0.02 || p >= cur->getPosition().getEnd().inSeconds() - 0.02) continue;
                                 if (auto nc = t->splitClip (*cur, te::TimePosition::fromSeconds (p))) { cur = nc; ++n; }
                             }
                         }
             }
             else if (cmd == "normalize_clip_gain" || cmd == "normalise_clip_gain")
             {
                 for (auto& c : s->selectedClips)
                     if (auto wc = dynamic_cast<te::WaveAudioClip*> (c.get()))
                     {
                         te::AudioFile af (wc->getPlaybackFile());
                         if (auto reader = std::unique_ptr<juce::AudioFormatReader> (session.getEngine().getAudioFileFormatManager().readFormatManager.createReaderFor (af.getFile())))
                         {
                             float peak = 0; juce::AudioBuffer<float> buf ((int) reader->numChannels, 65536);
                             for (juce::int64 p = 0; p < (juce::int64) reader->lengthInSamples; p += 65536)
                             {
                                 auto chunk = (int) juce::jmin ((juce::int64) 65536, (juce::int64) reader->lengthInSamples - p);
                                 reader->read (&buf, 0, chunk, p, true, true);
                                 for (int ch = 0; ch < buf.getNumChannels(); ++ch) peak = juce::jmax (peak, buf.getMagnitude (ch, 0, chunk));
                             }
                             if (peak > 0.00001f) { wc->setGainDB (juce::Decibels::gainToDecibels (juce::Decibels::decibelsToGain (-1.0f) / peak)); ++n; }
                         }
                     }
             }
             else return ToolResult::error ("Unknown command '" + cmd + "'.");
             refresh();
             if (err.isNotEmpty()) return ToolResult::error (err);
             return ToolResult::ok (cmd + ": " + juce::String (n) + " item(s) affected");
         });

    //==========================================================================
    add ("spot_clip", "Spot (move) a clip so its start or end lands exactly on a bar or time.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"to_bar":{"type":"number"},"to_seconds":{"type":"number"},"anchor":{"type":"string","enum":["start","end"]}},"required":["track"]})json",
         [this, refresh] (const juce::var& a)
         {
             juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
             auto c = clipArg (*t, a, err); if (! c) return ToolResult::error (err);
             auto to = timeArg (a, "to"); if (! to) return ToolResult::error ("Give to_bar or to_seconds");
             EditOps::spot (*c, *to, str (a, "anchor", "start") == "end" ? 1 : 0);
             refresh();
             return ToolResult::ok ("Spotted '" + c->getName() + "' " + str (a, "anchor", "start") + " to " + session.describeTime (*to));
         });

    add ("set_clip_gain", "Set or adjust clip gain (dB) on a clip, independent of the track fader; optional fade in/out lengths, mute, colour (palette index 1-16) and name.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"gain_db":{"type":"number"},"adjust_db":{"type":"number"},"fade_in_seconds":{"type":"number"},"fade_out_seconds":{"type":"number"},"fade_shape":{"type":"string","enum":["linear","convex","concave","s-curve"]},"mute":{"type":"boolean"},"colour":{"type":"integer"},"name":{"type":"string"},"lock":{"type":"boolean"}},"required":["track"]})json",
         [this, refresh] (const juce::var& a)
         {
             juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
             auto c = clipArg (*t, a, err); if (! c) return ToolResult::error (err);
             if (has (a, "name")) c->setName (str (a, "name"));
             if (has (a, "mute")) c->setMuted (flag (a, "mute"));
             if (has (a, "lock")) c->state.setProperty ("sqmLocked", flag (a, "lock"), um());
             if (has (a, "colour")) { int i = (int) num (a, "colour") - 1; if (i >= 0 && i < Theme::trackPalette().size()) c->setColour (Theme::trackPalette()[i]); }
             if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
             {
                 if (has (a, "gain_db")) ac->setGainDB ((float) juce::jlimit (-60.0, 24.0, num (a, "gain_db")));
                 if (has (a, "adjust_db")) ac->setGainDB ((float) juce::jlimit (-60.0, 24.0, ac->getGainDB() + num (a, "adjust_db")));
                 if (has (a, "fade_shape"))
                 {
                     auto sh = str (a, "fade_shape");
                     auto type = sh == "s-curve" ? te::AudioFadeCurve::sCurve : sh == "convex" ? te::AudioFadeCurve::convex : sh == "concave" ? te::AudioFadeCurve::concave : te::AudioFadeCurve::linear;
                     ac->setFadeInType (type); ac->setFadeOutType (type);
                 }
                 if (has (a, "fade_in_seconds")) ac->setFadeIn (te::TimeDuration::fromSeconds (juce::jmax (0.0, num (a, "fade_in_seconds"))));
                 if (has (a, "fade_out_seconds")) ac->setFadeOut (te::TimeDuration::fromSeconds (juce::jmax (0.0, num (a, "fade_out_seconds"))));
                 refresh();
                 return ToolResult::ok ("'" + c->getName() + "': gain " + juce::String (ac->getGainDB(), 1) + " dB, fade in " + juce::String (ac->getFadeIn().inSeconds(), 3) + " s, fade out " + juce::String (ac->getFadeOut().inSeconds(), 3) + " s" + (c->isMuted() ? ", muted" : ""));
             }
             refresh();
             return ToolResult::ok ("'" + c->getName() + "' updated");
         });

    add ("audiosuite", "Render an AudioSuite-style effect onto audio clips (non-destructive render chain): normalise, reverse, invert, make_mono, pitch_shift (semitones), or any effect plugin by name (plugin). remove_all clears the chain. Acts on the named clip or on the selected clips.",
         R"json({"type":"object","properties":{"effect":{"type":"string","description":"normalise | reverse | invert | make_mono | pitch_shift | plugin | remove_all"},"plugin":{"type":"string"},"semitones":{"type":"number"},"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]}},"required":["effect"]})json",
         [this, st, refresh] (const juce::var& a)
         {
             std::vector<te::AudioClipBase*> targets;
             juce::String err;
             if (has (a, "track"))
             {
                 auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
                 auto c = clipArg (*t, a, err); if (! c) return ToolResult::error (err);
                 if (auto ac = dynamic_cast<te::AudioClipBase*> (c)) targets.push_back (ac);
             }
             else if (st()) for (auto& c : st()->selectedClips) if (auto ac = dynamic_cast<te::AudioClipBase*> (c.get())) targets.push_back (ac);
             if (targets.empty()) return ToolResult::error ("No audio clip given or selected.");

             auto effect = str (a, "effect").toLowerCase();
             te::Plugin::Ptr plugin;
             if (effect == "plugin") { plugin = session.createPluginByName (str (a, "plugin"), err); if (plugin == nullptr) return ToolResult::error (err); }

             for (auto ac : targets)
             {
                 if (effect == "remove_all") { ac->enableEffects (false, false); continue; }
                 ac->enableEffects (true, false);
                 auto fx = ac->state.getChildWithName (te::IDs::EFFECTS);
                 if (! fx.isValid()) continue;
                 using T = te::ClipEffect::EffectType;
                 if (effect == "normalise" || effect == "normalize") fx.addChild (te::ClipEffect::create (T::normalise), -1, um());
                 else if (effect == "reverse") fx.addChild (te::ClipEffect::create (T::reverse), -1, um());
                 else if (effect == "invert") fx.addChild (te::ClipEffect::create (T::invert), -1, um());
                 else if (effect == "make_mono") fx.addChild (te::ClipEffect::create (T::makeMono), -1, um());
                 else if (effect == "pitch_shift")
                 {
                     auto v = te::ClipEffect::create (T::pitchShift);
                     auto ps = te::PitchShiftPlugin::create();
                     ps.setProperty (te::IDs::semitonesUp, (float) juce::jlimit (-24.0, 24.0, num (a, "semitones", 0)), nullptr);
                     v.addChild (ps, -1, nullptr);
                     fx.addChild (v, -1, um());
                 }
                 else if (effect == "plugin")
                 {
                     auto v = te::ClipEffect::create (T::filter);
                     plugin->flushPluginStateToValueTree();
                     v.addChild (plugin->state.createCopy(), -1, nullptr);
                     fx.addChild (v, -1, um());
                 }
                 else return ToolResult::error ("Unknown effect '" + effect + "'");
             }
             refresh();
             return ToolResult::ok ("Rendered " + effect + (plugin ? " (" + plugin->getName() + ")" : juce::String()) + " on " + juce::String ((int) targets.size()) + " clip(s)");
         });

    //==========================================================================
    add ("set_elastic_audio", "Set a track's Elastic Audio algorithm (none, polyphonic, rhythmic, monophonic, varispeed, molt - 'x-form' is accepted too) so its clips follow tempo changes; optionally conform (tempo-detect & stretch) existing clips, or set a clip speed ratio / pitch.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"mode":{"type":"string"},"conform_clips":{"type":"boolean"},"clip":{"type":["string","integer"]},"speed_ratio":{"type":"number"},"pitch_semitones":{"type":"number"},"follow_tempo":{"type":"boolean"}},"required":["track"]})json",
         [this, refresh] (const juce::var& a)
         {
             juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
             if (has (a, "mode"))
             {
                 auto names = Session::elasticModeNames();
                 int idx = -1;
                 for (int i = 0; i < names.size(); ++i) if (names[i].startsWithIgnoreCase (str (a, "mode"))) idx = i;
                 if (str (a, "mode").containsIgnoreCase ("x-form") || str (a, "mode").containsIgnoreCase ("xform")) idx = 5;
                 if (idx < 0) return ToolResult::error ("Unknown mode. Use none, polyphonic, rhythmic, monophonic, varispeed or molt.");
                 session.setElasticMode (*t, idx);
             }
             if (flag (a, "conform_clips"))
                 for (auto c : t->getClips()) if (auto ac = dynamic_cast<te::AudioClipBase*> (c)) { if (ac->getTimeStretchMode() == te::TimeStretcher::disabled) ac->setTimeStretchMode (Session::elasticToStretchMode (1)); ac->performTempoDetect(); ac->setAutoTempo (true); }
             if (has (a, "clip"))
             {
                 auto c = clipArg (*t, a, err); if (! c) return ToolResult::error (err);
                 if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
                 {
                     if (ac->getTimeStretchMode() == te::TimeStretcher::disabled) ac->setTimeStretchMode (Session::elasticToStretchMode (1));
                     if (has (a, "speed_ratio")) ac->setSpeedRatio (juce::jlimit (0.1, 10.0, num (a, "speed_ratio")));
                     if (has (a, "pitch_semitones")) ac->setPitchChange ((float) juce::jlimit (-24.0, 24.0, num (a, "pitch_semitones")));
                     if (has (a, "follow_tempo")) ac->setAutoTempo (flag (a, "follow_tempo"));
                 }
             }
             refresh();
             return ToolResult::ok (t->getName() + ": Elastic Audio = " + Session::elasticModeNames()[Session::getElasticMode (*t)]);
         });

    add ("set_automation_mode", "Set a track's automation mode: off, read, touch, latch or write (Pro Tools style). Writing modes record fader/pan/plugin moves during playback.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"mode":{"type":"string","enum":["off","read","touch","latch","write"]},"all_tracks":{"type":"boolean"}},"required":["mode"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto names = Session::automationModeNames();
             int idx = -1;
             for (int i = 0; i < names.size(); ++i) if (names[i].equalsIgnoreCase (str (a, "mode"))) idx = i;
             if (idx < 0) return ToolResult::error ("Unknown mode");
             if (flag (a, "all_tracks")) { for (auto t : session.getRows()) Session::setAutomationModeIndex (*t, idx); refresh(); return ToolResult::ok ("All tracks: " + names[idx]); }
             juce::String err; auto t = session.findRow (a["track"]); if (! t) { trackArg (a, err); return ToolResult::error (err); }
             Session::setAutomationModeIndex (*t, idx);
             refresh();
             return ToolResult::ok (t->getName() + ": automation " + names[idx]);
         });

    //==========================================================================
    add ("add_marker", "Add a memory location (marker) at a bar/time, or a selection memory location with a length. Returns its number.",
         R"json({"type":"object","properties":{"name":{"type":"string"},"bar":{"type":"number"},"seconds":{"type":"number"},"length_bars":{"type":"number"},"length_seconds":{"type":"number"}},"required":["name"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto at = has (a, "bar") ? session.barToTime (num (a, "bar")) : has (a, "seconds") ? te::TimePosition::fromSeconds (num (a, "seconds")) : edit().getTransport().getPosition();
             te::TimeDuration len;
             if (auto d = durationArg (a, "length", at)) len = *d;
             auto m = session.addMarker (at, str (a, "name"), len);
             if (m == nullptr) return ToolResult::error ("Could not create marker");
             refresh();
             return ToolResult::ok ("Marker " + juce::String (m->getMarkerID()) + " '" + m->getName() + "' at " + session.describeTime (at) + (len.inSeconds() > 0.05 ? " (selection, " + juce::String (len.inSeconds(), 2) + " s)" : ""));
         });

    add ("list_markers", "List all memory locations / markers with number, name and position.",
         R"json({"type":"object","properties":{}})json",
         [this] (const juce::var&)
         {
             juce::Array<juce::var> out;
             for (auto m : session.getMarkers())
             {
                 auto o = obj();
                 set (o, "number", m->getMarkerID()); set (o, "name", m->getName());
                 set (o, "position", session.describeTime (m->getPosition().getStart()));
                 set (o, "bar", session.timeToBar (m->getPosition().getStart()));
                 if (m->getPosition().getLength().inSeconds() > 0.05) set (o, "length_seconds", m->getPosition().getLength().inSeconds());
                 out.add (o);
             }
             return ToolResult::ok (juce::var (out));
         });

    add ("edit_marker", "Rename, move or delete a memory location by number (or name); go_to=true locates the playhead there.",
         R"json({"type":"object","properties":{"marker":{"type":["integer","string"]},"name":{"type":"string"},"bar":{"type":"number"},"seconds":{"type":"number"},"delete":{"type":"boolean"},"go_to":{"type":"boolean"}},"required":["marker"]})json",
         [this, refresh] (const juce::var& a)
         {
             te::MarkerClip* found = nullptr;
             for (auto m : session.getMarkers())
                 if ((a["marker"].isInt() && m->getMarkerID() == (int) a["marker"]) || m->getName().equalsIgnoreCase (a["marker"].toString()) || juce::String (m->getMarkerID()) == a["marker"].toString())
                     found = m;
             if (found == nullptr) return ToolResult::error ("No such marker. Call list_markers.");
             if (flag (a, "delete")) { auto n = found->getName(); found->removeFromParent(); refresh(); return ToolResult::ok ("Deleted marker " + n); }
             if (has (a, "name")) found->setName (str (a, "name"));
             if (has (a, "bar")) found->setStart (session.barToTime (num (a, "bar")), false, true);
             else if (has (a, "seconds")) found->setStart (te::TimePosition::fromSeconds (num (a, "seconds")), false, true);
             if (flag (a, "go_to")) edit().getTransport().setPosition (found->getPosition().getStart());
             refresh();
             return ToolResult::ok ("Marker " + juce::String (found->getMarkerID()) + " '" + found->getName() + "' at " + session.describeTime (found->getPosition().getStart()));
         });

    //==========================================================================
    add ("create_folder", "Create a Folder track (routing/submix folder - has its own fader and inserts) or a VCA master (controls member faders) containing the given tracks.",
         R"json({"type":"object","properties":{"name":{"type":"string"},"kind":{"type":"string","enum":["folder","vca"]},"tracks":{"type":"array","items":{"type":["string","integer"]}}},"required":["kind"]})json",
         [this, refresh] (const juce::var& a)
         {
             juce::Array<te::Track*> members;
             if (auto arr = a["tracks"].getArray()) for (auto& tv : *arr) if (auto t = session.findRow (tv)) members.add (t);
             auto f = session.addFolder (str (a, "name"), str (a, "kind", "folder") == "folder", members);
             if (f == nullptr) return ToolResult::error ("Could not create the folder");
             refresh();
             return ToolResult::ok ((f->isSubmixFolder() ? "Folder '" : "VCA '") + f->getName() + "' created with " + juce::String (members.size()) + " member track(s)");
         });

    add ("move_track_to_folder", "Move a track into a folder/VCA (or to the top level with folder='none').",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"folder":{"type":"string"}},"required":["track","folder"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto t = session.findRow (a["track"]); if (! t) return ToolResult::error ("No such track");
             auto fname = str (a, "folder");
             if (fname.equalsIgnoreCase ("none") || fname.equalsIgnoreCase ("top")) { edit().moveTrack (t, te::TrackInsertPoint (nullptr, t->getParentFolderTrack())); refresh(); return ToolResult::ok (t->getName() + " moved to the top level"); }
             te::FolderTrack* f = nullptr;
             for (auto ft : session.getFolderTracks()) if (ft->getName().equalsIgnoreCase (fname)) f = ft;
             if (f == nullptr) return ToolResult::error ("No folder named '" + fname + "'");
             edit().moveTrack (t, te::TrackInsertPoint (f, nullptr));
             refresh();
             return ToolResult::ok (t->getName() + " moved into " + f->getName());
         });

    add ("mix_group", "Create, edit or delete a mix group that links faders / mutes / solos / arms across tracks (like the Pro Tools Groups window). action: create | add | remove | delete | set | list. attributes: volume, mute, solo, arm, colour (booleans).",
         R"json({"type":"object","properties":{"action":{"type":"string"},"group":{"type":"string","description":"group id letter or name"},"name":{"type":"string"},"tracks":{"type":"array","items":{"type":["string","integer"]}},"active":{"type":"boolean"},"volume":{"type":"boolean"},"mute":{"type":"boolean"},"solo":{"type":"boolean"},"arm":{"type":"boolean"},"colour":{"type":"boolean"}},"required":["action"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto action = str (a, "action").toLowerCase();
             auto groups = session.getGroups();
             if (action == "list")
             {
                 juce::Array<juce::var> out;
                 for (auto& g : groups)
                 {
                     auto o = obj(); set (o, "id", g.id); set (o, "name", g.name); set (o, "active", g.active);
                     juce::Array<juce::var> names; for (auto& id : g.tracks) if (auto t = te::findTrackForID (edit(), id)) names.add (t->getName());
                     set (o, "tracks", names); set (o, "links", juce::String (g.linkVolume ? "volume " : "") + (g.linkMute ? "mute " : "") + (g.linkSolo ? "solo " : "") + (g.linkArm ? "arm " : "") + (g.linkColour ? "colour" : ""));
                     out.add (o);
                 }
                 return ToolResult::ok (juce::var (out));
             }
             Session::Group* g = nullptr;
             for (auto& gr : groups) if (gr.id.equalsIgnoreCase (str (a, "group")) || gr.name.equalsIgnoreCase (str (a, "group"))) g = &gr;
             Session::Group fresh;
             if (action == "create")
             {
                 fresh.id = session.nextGroupId(); fresh.name = str (a, "name", "Group " + fresh.id.toUpperCase());
                 g = &fresh;
             }
             if (g == nullptr) return ToolResult::error ("No such group. Use action=list.");
             if (action == "delete") { session.removeGroup (g->id); refresh(); return ToolResult::ok ("Deleted group " + g->id); }
             if (auto arr = a["tracks"].getArray())
                 for (auto& tv : *arr)
                     if (auto t = session.findRow (tv))
                     {
                         auto it = std::find (g->tracks.begin(), g->tracks.end(), t->itemID);
                         if (action == "remove") { if (it != g->tracks.end()) g->tracks.erase (it); }
                         else if (it == g->tracks.end()) g->tracks.push_back (t->itemID);
                     }
             if (has (a, "name")) g->name = str (a, "name");
             if (has (a, "active")) g->active = flag (a, "active");
             if (has (a, "volume")) g->linkVolume = flag (a, "volume");
             if (has (a, "mute")) g->linkMute = flag (a, "mute");
             if (has (a, "solo")) g->linkSolo = flag (a, "solo");
             if (has (a, "arm")) g->linkArm = flag (a, "arm");
             if (has (a, "colour")) g->linkColour = flag (a, "colour");
             session.setGroup (*g);
             refresh();
             return ToolResult::ok ("Group " + g->id + " '" + g->name + "' now has " + juce::String ((int) g->tracks.size()) + " track(s)" + (g->active ? "" : " (inactive)"));
         });

    add ("playlists", "Work with takes / playlists on a track's clip: list, select (take index 1-based), expand_to_tracks, expand_to_lanes, or new_playlist (duplicates the track as an alternate).",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"action":{"type":"string","enum":["list","select","expand_to_tracks","expand_to_lanes","new_playlist"]},"take":{"type":"integer"},"clip":{"type":["string","integer"]}},"required":["track","action"]})json",
         [this, refresh] (const juce::var& a)
         {
             juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
             auto action = str (a, "action");
             if (action == "new_playlist")
             {
                 auto stt = t->state.createCopy();
                 te::EditItemID::remapIDs (stt, nullptr, edit());
                 stt.setProperty (te::IDs::name, t->getName() + ".01", nullptr);
                 edit().insertTrack (te::TrackInsertPoint (t->getParentFolderTrack(), t), stt, nullptr);
                 refresh();
                 return ToolResult::ok ("New playlist track '" + t->getName() + ".01' created below " + t->getName());
             }
             te::AudioClipBase* ac = nullptr;
             if (has (a, "clip")) { auto c = clipArg (*t, a, err); if (! c) return ToolResult::error (err); ac = dynamic_cast<te::AudioClipBase*> (c); }
             else for (auto c : t->getClips()) if (auto a2 = dynamic_cast<te::AudioClipBase*> (c)) if (a2->hasAnyTakes()) { ac = a2; break; }
             if (ac == nullptr || ! ac->hasAnyTakes()) return ToolResult::ok ("No takes on this track yet. Enable Loop Record (set_record_mode) and record over a selection to create takes.");
             auto takes = ac->getTakes();
             if (action == "list") return ToolResult::ok ("'" + ac->getName() + "' has " + juce::String (takes.size()) + " take(s); current take " + juce::String (ac->getCurrentTake() + 1));
             if (action == "select") { int i = (int) num (a, "take", 1) - 1; if (i < 0 || i >= takes.size()) return ToolResult::error ("take out of range"); ac->setCurrentTake (i); refresh(); return ToolResult::ok ("Take " + juce::String (i + 1) + " selected"); }
             if (action == "expand_to_tracks") { ac->unpackTakes (true); refresh(); return ToolResult::ok ("Takes expanded to new tracks"); }
             if (action == "expand_to_lanes") { ac->unpackTakes (false); refresh(); return ToolResult::ok ("Takes expanded on the track"); }
             return ToolResult::error ("Unknown action");
         });

    //==========================================================================
    add ("set_record_mode", "Configure how recording works: mode normal | loop | quickpunch, plus pre_roll / post_roll (seconds; 0 = off), count_off_bars (0,1,2), wait_for_note, midi_merge, click.",
         R"json({"type":"object","properties":{"mode":{"type":"string","enum":["normal","loop","quickpunch"]},"pre_roll_seconds":{"type":"number"},"post_roll_seconds":{"type":"number"},"count_off_bars":{"type":"integer"},"wait_for_note":{"type":"boolean"},"midi_merge":{"type":"boolean"},"click":{"type":"boolean"},"loop_playback":{"type":"boolean"}}})json",
         [this] (const juce::var& a)
         {
             auto tc = ui.transport;
             if (has (a, "mode"))
             {
                 auto m = str (a, "mode");
                 Prefs::set ("op_loopRecord", m == "loop"); Prefs::set ("op_quickPunch", m == "quickpunch");
             }
             if (has (a, "pre_roll_seconds")) { auto v = num (a, "pre_roll_seconds"); Prefs::set ("op_preRollOn", v > 0); if (v > 0) Prefs::set ("op_preRoll", v); }
             if (has (a, "post_roll_seconds")) { auto v = num (a, "post_roll_seconds"); Prefs::set ("op_postRollOn", v > 0); if (v > 0) Prefs::set ("op_postRoll", v); }
             if (has (a, "count_off_bars")) Prefs::set ("op_countOffBars", juce::jlimit (0, 2, (int) num (a, "count_off_bars")));
             if (has (a, "wait_for_note")) Prefs::set ("op_waitForNote", flag (a, "wait_for_note"));
             if (has (a, "midi_merge")) Prefs::set ("op_midiMerge", flag (a, "midi_merge"));
             if (has (a, "click")) edit().clickTrackEnabled = flag (a, "click");
             if (has (a, "loop_playback")) { if (tc) tc->setLoopPlayback (flag (a, "loop_playback")); else edit().getTransport().looping = flag (a, "loop_playback"); }
             if (tc) tc->applyRecordModes();
             return ToolResult::ok (juce::String ("Record mode: ") + (Prefs::loopRecord() ? "Loop Record" : Prefs::quickPunch() ? "QuickPunch" : "Normal")
                                    + ", pre-roll " + (Prefs::preRollEnabled() ? juce::String (Prefs::preRollSeconds(), 1) + " s" : "off")
                                    + ", post-roll " + (Prefs::postRollEnabled() ? juce::String (Prefs::postRollSeconds(), 1) + " s" : "off")
                                    + ", count-off " + juce::String (Prefs::countOffBars()) + " bar(s), wait for note " + (Prefs::waitForNote() ? "on" : "off") + ", MIDI merge " + (Prefs::midiMerge() ? "on" : "off")
                                    + ", click " + (edit().clickTrackEnabled.get() ? "on" : "off") + ", loop playback " + (edit().getTransport().looping.get() ? "on" : "off"));
         });

    add ("menu_list", "List every command in Squelchmoth's menu bar (File, Edit, View, Track, Clip, Event, AudioSuite, Options, Setup, Window, Claude, Help) as paths like \"Window > Transport\", with enabled/checked state. Greyed-out commands need a selection first.",
         R"json({"type":"object","properties":{"filter":{"type":"string","description":"only paths containing this text"}}})json",
         [this] (const juce::var& a)
         {
             if (! ui.menuTree) return ToolResult::error ("UI not available");
             auto all = ui.menuTree();
             auto f = str (a, "filter");
             juce::Array<juce::var> out;
             if (auto* arr = all.getArray())
                 for (auto& v : *arr)
                     if (f.isEmpty() || v["path"].toString().containsIgnoreCase (f)) out.add (v);
             return ToolResult::ok (juce::JSON::toString (juce::var (out), true));
         });

    add ("menu_command", "Run any menu command exactly as if the user clicked it, by its path, e.g. \"Window > Transport\", \"Edit > Edit Mode > Grid\", \"Options > Mastering Window\", \"Clip > Clip Gain > +3 dB\". Segments are case-insensitive and may be prefixes. Use menu_list to see every path.",
         R"json({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})json",
         [this] (const juce::var& a)
         {
             if (! ui.menuCommand) return ToolResult::error ("UI not available");
             auto err = ui.menuCommand (str (a, "path"));
             if (err.isNotEmpty()) return ToolResult::error (err);
             juce::String open = ui.listWindows ? ui.listWindows().joinIntoString (", ") : juce::String();
             return ToolResult::ok ("Ran " + str (a, "path") + (open.isNotEmpty() ? ". Open windows: " + open : juce::String()));
         });

    add ("list_windows", "List the floating windows that are open right now (transport, memory, mastering, eqchart, plug-in editors are listed by the plugins tools).",
         R"json({"type":"object","properties":{}})json",
         [this] (const juce::var&)
         {
             if (! ui.listWindows) return ToolResult::error ("UI not available");
             auto w = ui.listWindows();
             return ToolResult::ok (w.isEmpty() ? juce::String ("No floating windows open") : "Open: " + w.joinIntoString (", "));
         });

    add ("show_window", "Open or close one of the floating windows: transport, bigcounter, memory, cliplist, groups, undo, colors, session, io, engine (Playback Engine), usage (System Usage: CPU, dropouts, heaviest plug-ins), plugins, preferences, welcome, eqchart (EQ frequency reference chart), mastering (album mastering window), snapshots (Mix Snapshots A-H), backups (Restore from Backup).",
         R"json({"type":"object","properties":{"window":{"type":"string"},"show":{"type":"boolean"}},"required":["window"]})json",
         [this] (const juce::var& a)
         {
             if (! ui.showWindow) return ToolResult::error ("UI not available");
             auto w = str (a, "window").toLowerCase().trim().replaceCharacters ("_-/", "   ").removeCharacters (" ");
             static const std::map<juce::String, juce::String> alias {
                 { "iosetup", "io" }, { "i", "io" }, { "playbackengine", "engine" }, { "audio", "engine" }, { "audiodevice", "engine" },
                 { "sessionsetup", "session" }, { "colorpalette", "colors" }, { "colours", "colors" }, { "colourpalette", "colors" }, { "color", "colors" },
                 { "memorylocations", "memory" }, { "markers", "memory" }, { "cliplist", "cliplist" }, { "clips", "cliplist" },
                 { "bigcounter", "bigcounter" }, { "counter", "bigcounter" }, { "undohistory", "undo" }, { "prefs", "preferences" }, { "settings", "preferences" },
                 { "eqfrequencychart", "eqchart" }, { "quickstart", "welcome" }, { "plugin", "plugins" }, { "pluginswindow", "plugins" },
                 { "systemusage", "usage" }, { "cpu", "usage" }, { "performance", "usage" }, { "systemusagewindow", "usage" },
                 { "mixsnapshots", "snapshots" }, { "snapshot", "snapshots" }, { "scenes", "snapshots" }, { "mixscenes", "snapshots" },
                 { "backup", "backups" }, { "restorefrombackup", "backups" } };
             if (auto it = alias.find (w); it != alias.end()) w = it->second;
             static const juce::StringArray known { "transport", "bigcounter", "memory", "cliplist", "groups", "undo", "colors", "session", "io", "engine",
                                                    "plugins", "preferences", "welcome", "eqchart", "mastering", "usage", "snapshots", "backups" };
             if (! known.contains (w))
                 return ToolResult::error ("Unknown window '" + str (a, "window") + "'. Use one of: " + known.joinIntoString (", ")
                                           + ". (Mix / Edit / Claude are views: use show_view. MIDI editor and Beat Maker open from their clips.)");
             ui.showWindow (w, flag (a, "show", true));
             return ToolResult::ok ((flag (a, "show", true) ? "Opened " : "Closed ") + w + " window");
         });

    add ("eq_reference", "Look up the EQ frequency chart: an instrument or voice (e.g. 'snare', 'male vocal', 'synth bass'), a problem word ('mud', 'boxy', 'sibilance', 'harsh'), a frequency ('300', '4k', '3.15 kHz') or a note ('A4'). Returns note ranges, harmonic reach, usual high-pass, and every boost / cut band with exact Hz ranges, or, for a frequency, its note, 1/3-octave band, region, wavelength and every instrument band living there. Empty query lists all 40 instruments. open_chart=true also opens Options > EQ Frequency Chart with the query in Find.",
         R"json({"type":"object","properties":{"query":{"type":"string"},"open_chart":{"type":"boolean"}}})json",
         [this] (const juce::var& a)
         {
             auto q = str (a, "query");
             if (flag (a, "open_chart", false) && ui.showWindow)
             {
                 EQChartContent::requestQuery (q);
                 ui.showWindow ("eqchart", true);
             }
             return ToolResult::ok (EQRef::lookup (q));
         });

    //==========================================================================
    add ("set_theme", "Change the look: a preset (Moth, Carbon, Daylight, Midnight Blue, Studio Grey) and/or individual colours as hex (edit_background, mix_background, panel, accent, text, waveform, grid), track palette (array of hex), ui_scale 0.8-1.6. Changes apply instantly and are saved.",
         R"json({"type":"object","properties":{"preset":{"type":"string"},"edit_background":{"type":"string"},"mix_background":{"type":"string"},"panel":{"type":"string"},"accent":{"type":"string"},"text":{"type":"string"},"waveform":{"type":"string"},"grid":{"type":"string"},"palette":{"type":"array","items":{"type":"string"}},"ui_scale":{"type":"number"}}})json",
         [this] (const juce::var& a)
         {
             auto& c = Theme::current();
             if (has (a, "preset"))
             {
                 bool found = false;
                 for (auto& p : Theme::presets()) if (juce::String (p.name).equalsIgnoreCase (str (a, "preset"))) { Theme::applyPreset (p); found = true; }
                 if (! found) return ToolResult::error ("Unknown preset. Presets: Moth, Carbon, Daylight, Midnight Blue, Studio Grey");
             }
             auto col = [&] (const char* key, juce::Colour& target) { if (has (a, key)) { auto s = str (a, key).trim().removeCharacters ("#"); if (s.length() == 6) s = "ff" + s; target = juce::Colour::fromString (s); c.name = "Custom"; } };
             col ("edit_background", c.bg); col ("mix_background", c.mixBg); col ("panel", c.panel); col ("accent", c.acid); col ("text", c.text); col ("waveform", c.waveform); col ("grid", c.grid);
             if (auto arr = a["palette"].getArray())
             {
                 juce::Array<juce::Colour> pal;
                 for (auto& v : *arr) { auto s = v.toString().trim().removeCharacters ("#"); if (s.length() == 6) s = "ff" + s; pal.add (juce::Colour::fromString (s)); }
                 if (pal.size() >= 2) c.trackPalette = pal;
             }
             if (has (a, "ui_scale")) c.uiScale = (float) juce::jlimit (0.8, 1.6, num (a, "ui_scale"));
             if (Prefs::file()) Theme::save (*Prefs::file());
             Theme::changed();
             return ToolResult::ok ("Theme now '" + c.name + "': edit bg " + c.bg.toDisplayString (false) + ", mix bg " + c.mixBg.toDisplayString (false) + ", accent " + c.acid.toDisplayString (false));
         });

    add ("set_colour", "Colour tracks or clips: palette index (1-16) or hex colour; applies to the named tracks (or all selected) and optionally to a clip.",
         R"json({"type":"object","properties":{"tracks":{"type":"array","items":{"type":["string","integer"]}},"track":{"type":["string","integer"]},"clip":{"type":["string","integer"]},"palette_index":{"type":"integer"},"hex":{"type":"string"}}})json",
         [this, st, refresh] (const juce::var& a)
         {
             juce::Colour colour;
             if (has (a, "palette_index")) { int i = (int) num (a, "palette_index") - 1; if (i < 0 || i >= Theme::trackPalette().size()) return ToolResult::error ("palette_index 1-" + juce::String (Theme::trackPalette().size())); colour = Theme::trackPalette()[i]; }
             else if (has (a, "hex")) { auto s = str (a, "hex").removeCharacters ("#"); if (s.length() == 6) s = "ff" + s; colour = juce::Colour::fromString (s); }
             else return ToolResult::error ("Give palette_index or hex");
             int n = 0;
             if (has (a, "track") && has (a, "clip"))
             {
                 juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
                 auto c = clipArg (*t, a, err); if (! c) return ToolResult::error (err);
                 c->setColour (colour); ++n;
             }
             else
             {
                 juce::Array<te::Track*> targets;
                 if (auto arr = a["tracks"].getArray()) for (auto& tv : *arr) if (auto t = session.findRow (tv)) targets.add (t);
                 if (has (a, "track")) if (auto t = session.findRow (a["track"])) targets.add (t);
                 if (targets.isEmpty() && st()) for (auto t : session.getRows()) if (st()->isTrackSelected (t)) targets.add (t);
                 for (auto t : targets) { t->setColour (colour); ++n; }
             }
             refresh();
             return ToolResult::ok ("Coloured " + juce::String (n) + " item(s) " + colour.toDisplayString (false));
         });

    add ("set_preference", "Read or change any Squelchmoth preference by key (same keys as Setup > Preferences), e.g. op_playFromSelection, op_followPlayhead, ed_fadeCmd, ed_autoFades, mx_newTrackDb, mi_velocity, cl_provider, pl_extraPaths. Omit value to read. list=true lists all current values.",
         R"json({"type":"object","properties":{"key":{"type":"string"},"value":{},"list":{"type":"boolean"}}})json",
         [this] (const juce::var& a)
         {
             if (flag (a, "list"))
             {
                 juce::String s;
                 if (auto f = Prefs::file()) for (auto& k : f->getAllProperties().getAllKeys()) if (! k.startsWith ("win_") && k != "mcpToken" && k != "licenceKey" && k != "cl_ngrokToken") s << k << " = " << f->getValue (k) << "\n";
                 return ToolResult::ok (s);
             }
             if (! has (a, "key")) return ToolResult::error ("Give key (and value to change it)");
             auto key = str (a, "key");
             if (key == "mcpToken" || key == "licenceKey" || key == "cl_ngrokToken") return ToolResult::error ("That setting is private.");
             if (has (a, "value")) Prefs::set (key.toRawUTF8(), a["value"]);
             return ToolResult::ok (key + " = " + Prefs::getString (key.toRawUTF8()));
         });

    add ("track_preset", "Save a track as a preset (inserts, sends, I/O, colour; no clips), load one onto a new track, or list presets.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["save","load","list"]},"track":{"type":["string","integer"]},"name":{"type":"string"}},"required":["action"]})json",
         [this, refresh] (const juce::var& a)
         {
             auto dir = session.sessionsDir.getChildFile ("Track Presets"); dir.createDirectory();
             auto action = str (a, "action");
             if (action == "list") { juce::Array<juce::var> out; for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.sqmtrack")) out.add (f.getFileNameWithoutExtension()); return ToolResult::ok (juce::var (out)); }
             if (action == "save")
             {
                 juce::String err; auto t = trackArg (a, err); if (! t) return ToolResult::error (err);
                 auto stt = t->state.createCopy();
                 for (int i = stt.getNumChildren(); --i >= 0;)
                     if (stt.getChild (i).hasType (te::IDs::MIDICLIP) || stt.getChild (i).hasType (te::IDs::AUDIOCLIP) || stt.getChild (i).hasType (te::IDs::STEPCLIP) || stt.getChild (i).hasType (te::IDs::TRACK))
                         stt.removeChild (i, nullptr);
                 auto f = dir.getChildFile (juce::File::createLegalFileName (str (a, "name", t->getName())) + ".sqmtrack");
                 if (auto xml = stt.createXml()) xml->writeTo (f);
                 return ToolResult::ok ("Saved preset " + f.getFileNameWithoutExtension());
             }
             auto f = dir.getChildFile (juce::File::createLegalFileName (str (a, "name")) + ".sqmtrack");
             if (! f.existsAsFile()) return ToolResult::error ("No preset named '" + str (a, "name") + "'");
             auto xml = juce::XmlDocument::parse (f);
             if (xml == nullptr) return ToolResult::error ("Preset file unreadable");
             auto stt = juce::ValueTree::fromXml (*xml);
             te::EditItemID::remapIDs (stt, nullptr, edit());
             auto tops = te::getTopLevelTracks (edit());
             auto nt = edit().insertTrack (te::TrackInsertPoint (nullptr, tops.isEmpty() ? nullptr : tops.getLast()), stt, nullptr);
             if (nt) nt->setName (has (a, "track") ? str (a, "track") : f.getFileNameWithoutExtension());
             refresh();
             return ToolResult::ok ("Loaded preset onto new track '" + (nt ? nt->getName() : juce::String()) + "'");
         });

    add ("list_folders", "List folder tracks and VCA masters with their member tracks.",
         R"json({"type":"object","properties":{}})json",
         [this] (const juce::var&)
         {
             juce::Array<juce::var> out;
             for (auto f : session.getFolderTracks())
             {
                 auto o = obj(); set (o, "name", f->getName()); set (o, "kind", f->isSubmixFolder() ? "folder" : "vca");
                 juce::Array<juce::var> m; for (auto t : f->getAllSubTracks (false)) m.add (t->getName());
                 set (o, "tracks", m);
                 out.add (o);
             }
             return ToolResult::ok (juce::var (out));
         });
}
