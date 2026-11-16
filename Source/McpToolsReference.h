// Squelchmoth 1.15 - the `analyze_reference` tool: the Reference Listener (Beat Intelligence L1). Included inside the McpTools class body.

/** The source file and range of a track's main audio clip (the longest unmuted one). */
bool referenceFromTrack (te::AudioTrack& t, juce::File& f, double& startSec, double& lengthSec, juce::String& err)
{
    te::AudioClipBase* best = nullptr; double bestLen = 0;
    for (auto c : t.getClips())
        if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
            if (! ac->isMuted() && ac->getPosition().getLength().inSeconds() > bestLen) { best = ac; bestLen = ac->getPosition().getLength().inSeconds(); }
    if (best == nullptr) { err = "No audio clip on " + t.getName() + " - drop the instrumental on an audio track first, or pass file"; return false; }
    f = best->getAudioFile().getFile();
    if (! f.existsAsFile()) { err = "The clip's audio file is missing: " + f.getFullPathName(); return false; }
    const double speed = juce::jmax (0.01, best->getSpeedRatio());
    startSec = best->getPosition().getOffset().inSeconds() * speed;
    lengthSec = bestLen * speed;
    return true;
}

/** Resolves file / track (or another pair of keys) to an analysis job. */
std::shared_ptr<RefListen::Job> referenceJob (const juce::var& a, const char* fileKey, const char* trackKey, juce::String& err, te::AudioTrack** trackOut = nullptr)
{
    double start = num (a, "start_seconds", 0.0), len = num (a, "length_seconds", 0.0);
    juce::File f; juce::String label;
    if (has (a, fileKey))
    {
        f = juce::File (str (a, fileKey));
        if (! f.existsAsFile()) { err = "No file at " + f.getFullPathName(); return nullptr; }
        label = f.getFileName();
    }
    else if (has (a, trackKey))
    {
        juce::var ta (new juce::DynamicObject()); ta.getDynamicObject()->setProperty ("track", a[trackKey]);
        auto t = trackArg (ta, err); if (t == nullptr) return nullptr;
        double s0 = 0, l0 = 0;
        if (! referenceFromTrack (*t, f, s0, l0, err)) return nullptr;
        start += s0; len = len > 0 ? juce::jmin (len, l0) : l0;
        label = t->getName() + " (" + f.getFileName() + ")";
        if (trackOut != nullptr) *trackOut = t;
    }
    else { err = "Pass file (a path) or track (the track the instrumental is on)"; return nullptr; }
    return RefListen::Store::get().start (f, start, len, label);
}

/** Waits for a job (the analysis takes about a second per minute of audio). */
static bool waitFor (const std::shared_ptr<RefListen::Job>& j, juce::String& err)
{
    if (! j->done.wait (120000)) { err = "The analysis is still running - try again in a moment"; return false; }
    if (! j->result.ok) { err = j->error.isNotEmpty() ? j->error : juce::String ("No steady beat found"); return false; }
    return true;
}

void registerReferenceTools()
{
    add ("analyze_reference",
         "The Reference Listener: hears a finished instrumental (a beat, a song's instrumental, a loop) the way a producer does and returns its blueprint - "
         "tempo (and half-time feel), key, swing, the arrangement (sections with bar numbers, energy and which elements play: kick, snare/clap, hats, 808/sub, chords/keys, pad, lead), "
         "the chord loop per section (names + Roman numerals), the kick / snare / hat step grid per section, hat rolls, the 808 line (notes, glides, octave jumps), "
         "and the mix profile (band balance, stereo width per band, sidechain pump). Only measurements are kept; nothing of the recording is copied.\n"
         "ALWAYS call this first when the user wants a beat 'like', 'similar to' or 'in the style of' a track they dropped in, then follow the rebuild plan: "
         "write ORIGINAL parts (new drums, new chords voicings, new melody, new 808 line) that match the measured feel, arrangement, density and mix - never sample or re-create the recording itself.\n"
         " action analyze (default): file (path) or track (the track the instrumental sits on); start_seconds / length_seconds optional; detail 'summary' (default) or 'full' (adds every bar's drums, every chord and every 808 note).\n"
         " action plan: the step-by-step rebuild plan in Squelchmoth's tools for the last (or given) reference.\n"
         " action compare: your beat against the reference - reference = file / track (default: the last analysed); yours = against_file / against_track, or nothing to render the session mix "
         "(the reference's own track is left out). Returns the gaps (tempo, key, swing, rolls, 808 glides, band balance, width, pump, sections, layers) with what to fix.\n"
         " action last: the last analysis again.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["analyze","plan","compare","last"]},"file":{"type":"string"},"track":{"type":["string","integer"]},
            "start_seconds":{"type":"number"},"length_seconds":{"type":"number"},"detail":{"type":"string","enum":["summary","full"]},
            "against_file":{"type":"string"},"against_track":{"type":["string","integer"]},"start_bar":{"type":"number"},"end_bar":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const auto action = str (a, "action", "analyze").toLowerCase();
             juce::String err;
             te::AudioTrack* refTrack = nullptr;
             std::shared_ptr<RefListen::Job> job;
             if (has (a, "file") || has (a, "track")) job = referenceJob (a, "file", "track", err, &refTrack);
             else if (action != "analyze") job = RefListen::Store::get().latest();
             if (job == nullptr) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("No reference analysed yet - pass file or track"));
             if (! waitFor (job, err)) return ToolResult::error (err);
             const auto& r = job->result;

             if (action == "analyze" || action == "last")
             {
                 auto o = RefListen::toJson (r, str (a, "detail") == "full");
                 return ToolResult::ok (RefListen::summary (r, job->label) + "\n" + RefListen::rebuildPlan (r) + "\nBLUEPRINT JSON:\n" + juce::JSON::toString (o, true));
             }
             if (action == "plan") return ToolResult::ok (RefListen::rebuildPlan (r));
             if (action == "compare")
             {
                 std::shared_ptr<RefListen::Job> mine;
                 if (has (a, "against_file") || has (a, "against_track"))
                 {
                     juce::var b (new juce::DynamicObject());
                     if (has (a, "against_file")) b.getDynamicObject()->setProperty ("file", a["against_file"]);
                     if (has (a, "against_track")) b.getDynamicObject()->setProperty ("track", a["against_track"]);
                     mine = referenceJob (b, "file", "track", err);
                     if (mine == nullptr) return ToolResult::error (err);
                 }
                 else
                 {
                     if (session.edit == nullptr) return ToolResult::error ("No session open");
                     // render the session mix without the reference's own track
                     if (refTrack == nullptr)
                         for (auto t : te::getAudioTracks (*session.edit))
                             for (auto c : t->getClips())
                                 if (auto ac = dynamic_cast<te::AudioClipBase*> (c))
                                     if (ac->getAudioFile().getFile() == job->file) refTrack = t;
                     juce::Array<te::Track*> render;
                     for (auto t : te::getAllTracks (*session.edit)) if (t != refTrack) render.add (t);
                     BounceExport::Options o; o.format = "wav"; o.bitDepth = 32; o.dither = false;
                     if (has (a, "start_bar") || has (a, "end_bar"))
                     {
                         const double t0 = session.barToTime (num (a, "start_bar", 1.0)).inSeconds();
                         const double t1 = has (a, "end_bar") ? session.barToTime (num (a, "end_bar")).inSeconds() : session.edit->getLength().inSeconds();
                         o.range = te::TimeRange (te::TimePosition::fromSeconds (t0), te::TimePosition::fromSeconds (juce::jmax (t0 + 4.0, t1)));
                     }
                     auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Squelchmoth compare mix.wav");
                     auto p = BounceExport::params (session, o, dest);
                     p.tracksToDo = te::toBitSet (render);
                     auto f = te::Renderer::renderToFile ("Compare mix", p);
                     if (! f.existsAsFile()) return ToolResult::error ("Couldn't render the session mix to compare");
                     mine = RefListen::Store::get().start (f, 0, 0, "your mix");
                 }
                 if (! waitFor (mine, err)) return ToolResult::error ("Your beat: " + err);
                 return ToolResult::ok (RefListen::compare (r, mine->result));
             }
             return ToolResult::error ("Unknown action '" + action + "' (analyze, plan, compare, last)");
         });
}
