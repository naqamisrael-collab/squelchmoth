// Squelchmoth 1.16 - Prism Split tools: `separate` and `vocal_remove`. Included inside the McpTools class body.

ToolResult splitStatus (const std::shared_ptr<PrismSplit::Job>& j, bool import, double waitSeconds)
{
    if (waitSeconds > 0) j->done.wait ((int) (juce::jlimit (0.0, 240.0, waitSeconds) * 1000.0));
    if (! j->finished) return ToolResult::ok ("Separation " + juce::String (j->id) + " of " + j->label + ": " + juce::String (juce::roundToInt (j->progress.load() * 100)) + " % - call separate action status (wait_seconds up to 60) again");
    if (j->error.isNotEmpty()) return ToolResult::error (j->error);
    juce::String s = "Separation " + juce::String (j->id) + " done: " + juce::String (j->seconds, 1) + " s of audio in " + juce::String (j->tookSeconds, 1) + " s (" + juce::String (j->studio ? "Studio" : "Draft") + " quality). Stems sum back to the mix: null " + juce::String (j->nullDb, 1) + " dB.\nFiles in " + j->folder.getFullPathName() + ":";
    for (auto& f : j->stems) s << "\n  " << f.getFileName();
    if (j->wantMinus) s << "\n  " << j->instrumental.getFileName() << "\n  " << j->acapella.getFileName();
    if (import && ! j->imported.exchange (true) && session.edit != nullptr)
    {
        juce::StringArray made; juce::Array<juce::File> files;
        if (j->wantMinus) { files.add (j->instrumental); files.add (j->acapella); } else for (auto& f : j->stems) files.add (f);
        for (auto& f : files)
        {
            const auto name = f.getFileNameWithoutExtension();
            juce::var t (new juce::DynamicObject()); set (t, "kind", "audio"); set (t, "name", name);
            const int idx = firstNumber (call ("add_track", t).text); if (idx <= 0) continue;
            juce::var im (new juce::DynamicObject()); set (im, "track", idx); set (im, "path", f.getFullPathName()); set (im, "start_seconds", j->placeAtSeconds);
            auto r = call ("import_audio", im); if (! r.isError) made.add (juce::String (idx) + " " + name);
        }
        if (ui.refresh) ui.refresh();
        s << "\nOn the timeline: tracks " << made.joinIntoString (", ") << " (aligned with the source; mute the original to hear them alone).";
    }
    if (j->studio) s << "\nStudio quality: the neural separator (stems are written at 44.1 kHz). Vocals, drums and bass are clean on most records; dense mixes can still leave traces in Other. Its model is for personal and research use.";
    else s << "\nDraft quality is spectral masking (centre / harmonic / percussive): good for practice tracks, remixes and analysis; expect some bleed between stems. " << (PrismSplit::StudioPack::installed() ? "Pass quality studio for the neural separator." : "For clean stems install Studio quality once: separate action install_studio (a 165 MB download).");
    return ToolResult::ok (s);
}

ToolResult splitStart (const juce::var& a, bool minus)
{
    juce::String err; juce::File f; double start = num (a, "start_seconds", 0.0), len = num (a, "length_seconds", 0.0), placeAt = 0; juce::String label;
    if (has (a, "file")) { f = juce::File (str (a, "file")); if (! f.existsAsFile()) return ToolResult::error ("No file at " + f.getFullPathName()); label = f.getFileNameWithoutExtension(); }
    else if (has (a, "track"))
    {
        auto t = trackArg (a, err); if (t == nullptr) return ToolResult::error (err);
        double s0 = 0, l0 = 0; if (! referenceFromTrack (*t, f, s0, l0, err)) return ToolResult::error (err);
        start += s0; len = len > 0 ? juce::jmin (len, l0) : l0; label = t->getName();
        for (auto c : t->getClips()) if (auto ac = dynamic_cast<te::AudioClipBase*> (c)) if (ac->getAudioFile().getFile() == f) { placeAt = ac->getPosition().getStart().inSeconds(); break; }
    }
    else return ToolResult::error ("Pass file (a path) or track (the track the song is on)");
    if (! f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg") && ! Frame::isVideo (f)) return ToolResult::error ("Pass an audio file (WAV, AIFF, FLAC, MP3, OGG) or a video file (MP4, MOV, MKV, WEBM, AVI).");
    juce::File base = session.edit != nullptr && session.getFile().existsAsFile() ? session.getFile().getParentDirectory() : juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Squelchmoth");
    if (has (a, "folder")) base = juce::File (str (a, "folder"));
    if (Frame::isVideo (f))
    {
        // a video: separate its sound (kept beside the stems)
        if (label.isEmpty()) label = f.getFileNameWithoutExtension();
        f = Frame::audioOf (f, base.getChildFile ("Separated").getChildFile (juce::File::createLegalFileName (label).substring (0, 60)), 44100.0, err);
        if (! f.existsAsFile()) return ToolResult::error (err);
    }
    label = juce::File::createLegalFileName (label).substring (0, 60);
    auto folder = base.getChildFile ("Separated").getChildFile (label);
    const auto q = str (a, "quality").toLowerCase();
    if (q == "studio" && ! PrismSplit::StudioPack::installed()) return ToolResult::error ("Studio quality is not installed yet. Run separate action install_studio first (a 165 MB download, once), or use quality draft.");
    const int wantStems = (int) num (a, "stems", 4);
    if (wantStems == 6 && ! PrismSplit::StudioPack::installed6()) return ToolResult::error ("The 6-stem model (guitar and piano) is not installed yet. Run separate action install_studio with stems 6 first (a 130 MB download, once).");
    const bool studio = wantStems == 6 || q == "studio" || (q != "draft" && PrismSplit::StudioPack::installed());
    auto j = PrismSplit::Store::get().start (f, start, len, folder, label, minus, placeAt, studio, wantStems == 6 ? 6 : 4);
    return splitStatus (j, flag (a, "import", true), num (a, "wait_seconds", 20.0));
}

void registerSplitTools()
{
    add ("separate",
         "Prism Split: separates a song into stems - Vocals (the centre lead), Drums, Bass and Other - as 24-bit WAV files that sum back to the original exactly, and puts each on its own track, lined up with the source.\n"
         " action start (default): file (path to WAV / AIFF / FLAC / MP3 / OGG, or a video file: MP4 / MOV / MKV / WEBM, its sound is separated) or track (the track the song is on); start_seconds / length_seconds optional; import (default true) adds the stem tracks; "
         "folder optional (default: a Separated folder beside the session). It runs in the background and waits up to wait_seconds (default 20) for it to finish.\n"
         " action status: job (the number; default the last one), wait_seconds - progress, then the files and the tracks.\n"
         " quality: studio (the neural separator: clean vocals, drums and bass; roughly real time on a laptop CPU; needs the one-time install) or draft (fast spectral masking, about 20x real time, some bleed). Default: studio when installed.\n"
         " stems 6 (Studio only): adds Guitar and Piano stems from the 6-stem model (guitar is usable, piano is the weakest stem; install it once with action install_studio and stems 6, 130 MB).\n"
         " action install_studio: downloads Studio quality once (about 165 MB) in the background; action studio_status: its progress. Any length works: both qualities stream.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["start","status","install_studio","studio_status"]},"quality":{"type":"string","enum":["studio","draft"]},"stems":{"type":"integer","enum":[4,6]},"runtime_only":{"type":"boolean"},"file":{"type":"string"},"track":{"type":["string","integer"]},"start_seconds":{"type":"number"},
            "length_seconds":{"type":"number"},"import":{"type":"boolean"},"folder":{"type":"string"},"job":{"type":"integer"},"wait_seconds":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const auto action = str (a, "action", "start");
             if (action == "install_studio" || action == "studio_status")
             {
                 namespace SP = PrismSplit::StudioPack; auto& st = SP::state();
                 const bool runtimeOnly = flag (a, "runtime_only"); const bool six = (int) num (a, "stems", 4) == 6;
                 const bool have = runtimeOnly ? SP::libFile().existsAsFile() : six ? SP::installed6() : SP::installed();
                 if (have && ! st.running) return ToolResult::ok (juce::String ("Studio quality is installed") + (runtimeOnly ? " (runtime)" : "") + " in " + SP::folder().getFullPathName() + ". separate now uses it by default (quality draft for the fast one).");
                 if (action == "install_studio") SP::startInstall (runtimeOnly, six);
                 if (! st.running && ! st.finished) return ToolResult::ok ("Studio quality is not installed. separate action install_studio downloads it once: the separation runtime (ONNX Runtime, MIT licence) and the model (158 MB). The model's weights are published under an open licence but were trained in part on research-only recordings, so treat Studio quality as for personal and research use; for commercial releases use Draft or your own judgement. Installs into " + SP::folder().getFullPathName() + ".");
                 st.done.wait ((int) (juce::jlimit (0.0, 240.0, num (a, "wait_seconds", 20.0)) * 1000.0));
                 if (! st.finished) return ToolResult::ok ("Installing Studio quality: " + st.stage + ", " + juce::String (juce::roundToInt (st.progress.load() * 100)) + " % - ask again with separate action studio_status");
                 if (st.error.isNotEmpty()) return ToolResult::error ("Studio quality install failed: " + st.error);
                 return ToolResult::ok ("Studio quality installed in " + SP::folder().getFullPathName() + ". separate now uses it by default.");
             }
             if (action == "status")
             {
                 auto j = PrismSplit::Store::get().find ((int) num (a, "job", 0)); if (j == nullptr) return ToolResult::error ("No separation has been started");
                 return splitStatus (j, flag (a, "import", true), num (a, "wait_seconds", 10.0));
             }
             return splitStart (a, false);
         });

    add ("vocal_remove",
         "Vocal remover: makes an Instrumental (everything but the lead vocal) and an Acapella (the lead vocal) from a song, as WAV files and two new tracks. file or track; start_seconds / length_seconds optional; "
         "wait_seconds (default 20). window true opens the Vocal Remover window instead (drop zone, the two lanes with waveforms and volume sliders, play, save). Also writes the four stems. quality studio | draft as in separate (studio when installed); check progress with separate action status.",
         R"json({"type":"object","properties":{"window":{"type":"boolean"},"quality":{"type":"string","enum":["studio","draft"]},"file":{"type":"string"},"track":{"type":["string","integer"]},"start_seconds":{"type":"number"},"length_seconds":{"type":"number"},"import":{"type":"boolean"},"folder":{"type":"string"},"wait_seconds":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             if (flag (a, "window"))
             {
                 if (! ui.openVocalRemover) return ToolResult::error ("UI not available");
                 juce::File f; if (has (a, "file")) { f = juce::File (str (a, "file")); if (! f.existsAsFile()) return ToolResult::error ("No file at " + f.getFullPathName()); }
                 else if (has (a, "track")) { juce::String err; auto t = trackArg (a, err); if (t == nullptr) return ToolResult::error (err); double s0 = 0, l0 = 0; if (! referenceFromTrack (*t, f, s0, l0, err)) return ToolResult::error (err); }
                 ui.openVocalRemover (f);
                 return ToolResult::ok (f.existsAsFile() ? "Vocal Remover window open, separating " + f.getFileName() + ": two lanes (Music, Vocal) with volume sliders, Play, Save and Send to session appear when it is done."
                                                         : juce::String ("Vocal Remover window open: drop a song on it."));
             }
             return splitStart (a, true);
         });
}
