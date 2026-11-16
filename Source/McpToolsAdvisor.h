// Squelchmoth 1.23 - `session_advice`: the engineer at your shoulder. Included inside the McpTools class body.

bool adviseRender (const juce::Array<te::Track*>& tracks, te::TimeRange range, bool masterInserts, SqAdvise::Stats& out, juce::String& err)
{
    BounceExport::Options o; o.format = "wav"; o.bitDepth = 32; o.dither = false; o.masterInserts = masterInserts; o.range = range;
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("squelchmoth-advice", ".wav", false);
    auto p = BounceExport::params (session, o, tmp);
    p.tracksToDo = te::toBitSet (tracks.isEmpty() ? te::getAllTracks (*session.edit) : tracks);
    juce::File f;
    { MonitorChain::PrintGuard guard (session); f = BounceExport::render ("Advice", p, o, err); }
    if (! f.existsAsFile()) { out = {}; return true; }          // nothing audible on these tracks in the range: silent
    std::vector<float> L, R; double fs = 0; juce::String e2;
    const bool ok = RefListen::readStereo (f, 0.0, 0.0, L, R, fs, e2);
    f.deleteFile(); tmp.deleteFile();
    if (! ok) { err = e2; return false; }
    out = SqAdvise::measure (L.data(), R.data(), (int) L.size(), fs);
    return true;
}

void registerAdvisorTools()
{
    add ("session_advice",
         "The engineer at your shoulder: measures the session's own audio and says what to fix, with the numbers. Use it before a bounce, after recording a take, and whenever the user asks how the mix or a take sounds.\n"
         " stage mixing (default): renders the mix and each track with audio (up to 24, up to 90 seconds from start_seconds; default the start of the session) and reports headroom, dynamic range, mono compatibility, low-end width, "
         "low-mid build-up, which tracks fight over the same frequency band, tracks that clip, and tracks carrying rumble they do not need.\n"
         " stage recording: track (required) - reads the take on that track: clipping, level, noise floor between phrases, DC offset, polarity.\n"
         " Advice is guidance from measurements, not a verdict: say what was measured, make the change, then run it again to confirm. For loudness targets and album work use the mastering tools; to compare with a reference song use analyze_reference.",
         R"json({"type":"object","properties":{"stage":{"type":"string","enum":["mixing","recording"]},"track":{"type":["string","integer"]},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"},"start_bar":{"type":"number"},"end_bar":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             auto& e = edit(); juce::String err;
             if (str (a, "stage", "mixing").toLowerCase() == "recording")
             {
                 auto t = trackArg (a, err); if (t == nullptr) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("stage recording needs track"));
                 juce::File f; double s0 = 0, l0 = 0; if (! referenceFromTrack (*t, f, s0, l0, err)) return ToolResult::error (err);
                 std::vector<float> L, R; double fs = 0;
                 if (! RefListen::readStereo (f, s0, juce::jmin (l0, 600.0), L, R, fs, err)) return ToolResult::error (err);
                 const auto st = SqAdvise::measure (L.data(), R.data(), (int) L.size(), fs);
                 const auto adv = SqAdvise::recordingAdvice (st);
                 juce::String s = "Take on " + t->getName() + " (" + f.getFileName() + ", " + juce::String (st.seconds, 1) + " s): " + SqAdvise::statsLine (st) + ".";
                 if (adv.isEmpty()) s << "\nNothing to fix: clean, at a good level, no clipping.";
                 for (auto& l : adv) s << "\n- " << l;
                 return ToolResult::ok (s);
             }

             e.getTransport().stop (false, false);
             const auto start = timeArg (a, "start").value_or (0s);
             auto end = timeArg (a, "end").value_or (te::toPosition (e.getLength()));
             if (end > start + 90s) end = start + 90s;
             if (end <= start + 0.5s) return ToolResult::error ("Nothing to measure in that range (the session is empty or the range is too short)");
             const te::TimeRange range (start, end);
             SqAdvise::Stats mix; if (! adviseRender ({}, range, true, mix, err)) return ToolResult::error (err);
             std::vector<SqAdvise::Named> named; int skipped = 0;
             for (auto* t : te::getAudioTracks (e))
             {
                 bool any = false; for (auto c : t->getClips()) if (c->getEditTimeRange().overlaps (range)) { any = true; break; }
                 if (! any || t->isMuted (true)) continue;
                 if ((int) named.size() >= 24) { ++skipped; continue; }
                 SqAdvise::Stats st; juce::Array<te::Track*> set1; for (auto x : BounceExport::renderSetFor (session, *t, false)) set1.addIfNotAlreadyThere (x);
                 if (! adviseRender (set1, range, false, st, err)) continue;
                 if (! st.silent) named.push_back ({ t->getName(), st });
             }
             const auto adv = SqAdvise::mixAdvice (mix, named);
             juce::String s = "Mix, " + juce::String (start.inSeconds(), 1) + " to " + juce::String (end.inSeconds(), 1) + " s, " + juce::String ((int) named.size()) + " tracks measured" + (skipped > 0 ? " (" + juce::String (skipped) + " more not measured)" : juce::String()) + ": "
                              + SqAdvise::statsLine (mix) + ".\nBands: " + SqAdvise::bandsLine (mix) + ".";
             if (adv.isEmpty()) s << "\nNothing stands out in the measurements: headroom, dynamics, mono and band balance are all in a healthy range. Trust your ears from here.";
             for (auto& l : adv) s << "\n- " << l;
             return ToolResult::ok (s);
         });
}
