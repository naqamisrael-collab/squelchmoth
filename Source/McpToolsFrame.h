// Squelchmoth 1.21 - Frame, stage 0: `video_import` and `video_export`. Included inside the McpTools class body.

juce::File frameVideoFolder()
{
    juce::File base = session.edit != nullptr && session.getFile().existsAsFile() ? session.getFile().getParentDirectory() : juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Squelchmoth");
    return base.getChildFile ("Video Sound");
}

ToolResult frameExportStatus (const std::shared_ptr<Frame::Job>& j, double waitSeconds)
{
    if (waitSeconds > 0) j->done.wait ((int) (juce::jlimit (0.0, 240.0, waitSeconds) * 1000.0));
    if (! j->finished) return ToolResult::ok ("Video export " + juce::String (j->id) + ": " + juce::String (juce::roundToInt (j->progress.load() * 100)) + " % - call video_export action status (wait_seconds up to 60) again");
    if (j->error.isNotEmpty()) return ToolResult::error (j->error);
    const auto& o = j->options;
    return ToolResult::ok ("Video exported to " + o.out.getFullPathName() + " (" + juce::String (o.out.getSize() / (1024 * 1024)) + " MB, " + juce::RelativeTime::seconds (o.seconds).getDescription()
                           + ", picture: " + o.picture + ", " + (o.picture == "source" && ! o.resize && std::abs (o.videoOffset) < 0.0005 ? juce::String ("original picture copied without re-encoding") : juce::String (o.width) + " x " + juce::String (o.height) + " " + j->encoder)
                           + ", AAC 320 kb/s) in " + juce::String (j->tookSeconds, 1) + " s.");
}

void registerFrameTools()
{
    add ("video_import",
         "Frame: brings a video file into the session. Its sound goes on a new audio track (24-bit, at the session rate) and the video is remembered as the session's picture, so video_export can put the new mix back under it. "
         "path (MP4, MOV, MKV, WEBM, AVI...); start_seconds (where it sits on the timeline, default 0); sound false only reads the facts (length, size, frame rate) and remembers the picture. "
         "There is no picture viewer on the timeline yet. When the computer has no FFmpeg, run video_export action install once. separate and vocal_remove also take video files directly.",
         R"json({"type":"object","properties":{"path":{"type":"string"},"start_seconds":{"type":"number"},"sound":{"type":"boolean"}},"required":["path"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const juce::File f (str (a, "path")); if (! f.existsAsFile()) return ToolResult::error ("No file at " + f.getFullPathName());
             if (! Frame::ffmpeg().existsAsFile()) return ToolResult::error (Frame::missingText());
             const auto in = Frame::probe (f); if (! in.ok) return ToolResult::error (in.error);
             auto& e = edit(); const double at = num (a, "start_seconds", 0.0);
             juce::String s = f.getFileName() + ": " + in.text() + ".";
             if (in.hasVideo) { e.state.setProperty ("frameVideo", f.getFullPathName(), nullptr); e.state.setProperty ("frameVideoAt", at, nullptr); s << "\nRemembered as the session's picture (video_export picture source)."; }
             if (flag (a, "sound", true) && in.hasAudio)
             {
                 juce::String err; const auto wav = frameVideoFolder().getNonexistentChildFile (juce::File::createLegalFileName (f.getFileNameWithoutExtension()).substring (0, 60), ".wav", false);
                 if (! Frame::extractAudio (f, wav, TimeFmt::sampleRate (e), err)) return ToolResult::error (err);
                 juce::var t (new juce::DynamicObject()); set (t, "kind", "audio"); set (t, "name", f.getFileNameWithoutExtension().substring (0, 40));
                 const int idx = firstNumber (call ("add_track", t).text); if (idx <= 0) return ToolResult::error ("Couldn't add a track for the video's sound");
                 juce::var im (new juce::DynamicObject()); set (im, "track", idx); set (im, "path", wav.getFullPathName()); set (im, "start_seconds", at);
                 auto r = call ("import_audio", im); if (r.isError) return r;
                 if (ui.refresh) ui.refresh();
                 s << "\nIts sound is on track " << idx << " at " << juce::String (at, 2) << " s (" << wav.getFileName() << ").";
             }
             else if (! in.hasAudio) s << "\nIt has no sound to import.";
             return ToolResult::ok (s);
         });

    add ("video_export",
         "Frame: exports the session's mix as an MP4 video (H.264 picture, AAC 320 kb/s sound). It bounces the mix first (master inserts included), then builds the video in the background.\n"
         " picture: source (the picture of the video brought in with video_import, or video = a path; copied without re-encoding when the export starts at the video's start and no size is given), "
         "image (image = a path to a cover; a still-picture video, the usual way to post a song or a long mix), waveform (a moving waveform), spectrum (a scrolling spectrogram), black. Default: source when the session has a picture, else waveform.\n"
         " width / height (default 1920 x 1080; 1080 x 1920 for vertical), fps (default 30; 2 is enough for image), start/end in seconds or bars (default the whole session), name or path, wait_seconds (default 20).\n"
         " action status: job (default the last), wait_seconds. It streams, so the length does not change the memory used.\n"
         " action install: downloads the video tools (FFmpeg, about 80 MB, once) when the computer has none; action install_status: its progress. Not yet here: picture editing, titles, upload.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["start","status","install","install_status"]},"picture":{"type":"string","enum":["source","image","waveform","spectrum","black"]},"video":{"type":"string"},"image":{"type":"string"},
            "width":{"type":"integer"},"height":{"type":"integer"},"fps":{"type":"number"},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"},"start_bar":{"type":"number"},"end_bar":{"type":"number"},
            "name":{"type":"string"},"path":{"type":"string"},"job":{"type":"integer"},"wait_seconds":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const auto action = str (a, "action", "start");
             if (action == "install" || action == "install_status")
             {
                 auto& st = Frame::installState();
                 if (Frame::ffmpeg().existsAsFile() && ! st.running) return ToolResult::ok ("Video is ready: FFmpeg at " + Frame::ffmpeg().getFullPathName());
                 if (action == "install") Frame::startInstall();
                 if (! st.running && ! st.finished) return ToolResult::ok ("The video tools are not installed. video_export action install downloads FFmpeg once (about " + juce::String (Frame::packItem().megabytes) + " MB).");
                 st.done.wait ((int) (juce::jlimit (0.0, 240.0, num (a, "wait_seconds", 30.0)) * 1000.0));
                 if (! st.finished) return ToolResult::ok ("Downloading the video tools: " + juce::String (juce::roundToInt (st.progress.load() * 100)) + " % - ask again with video_export action install_status");
                 if (st.error.isNotEmpty()) return ToolResult::error (st.error);
                 return ToolResult::ok ("Video is ready: FFmpeg installed in " + Frame::folder().getFullPathName());
             }
             if (action == "status")
             {
                 auto j = Frame::Store::get().find ((int) num (a, "job", 0)); if (j == nullptr) return ToolResult::error ("No video export has been started");
                 return frameExportStatus (j, num (a, "wait_seconds", 10.0));
             }
             if (! Frame::ffmpeg().existsAsFile()) return ToolResult::error (Frame::missingText());
             auto& e = edit(); Frame::ExportOptions o;
             juce::File video (has (a, "video") ? str (a, "video") : e.state["frameVideo"].toString());
             o.picture = str (a, "picture", video.existsAsFile() ? "source" : (has (a, "image") ? "image" : "waveform")).toLowerCase();
             if (o.picture == "source") { if (! video.existsAsFile()) return ToolResult::error ("There is no picture in this session: bring a video in with video_import, pass video (a path), or use picture image / waveform / spectrum / black"); o.sourceVideo = video; }
             if (o.picture == "image") { o.image = juce::File (str (a, "image")); if (! o.image.existsAsFile()) return ToolResult::error ("picture image needs image: the path of a PNG or JPEG"); }
             o.resize = has (a, "width") || has (a, "height");
             o.width = juce::jlimit (128, 7680, (int) num (a, "width", 1920.0)) & ~1; o.height = juce::jlimit (128, 4320, (int) num (a, "height", 1080.0)) & ~1;
             o.fps = juce::jlimit (1.0, 120.0, num (a, "fps", 30.0));
             const double startS = timeArg (a, "start").value_or (0s).inSeconds();
             if (o.picture == "source") o.videoOffset = startS - (has (a, "video") ? 0.0 : (double) e.state.getProperty ("frameVideoAt", 0.0));
             o.out = has (a, "path") ? juce::File (str (a, "path")) : session.bouncesDir.getNonexistentChildFile (juce::File::createLegalFileName (str (a, "name", session.getFile().getFileNameWithoutExtension() + " video")), ".mp4", false);
             if (! o.out.hasFileExtension ("mp4;mov;mkv")) o.out = o.out.withFileExtension ("mp4");

             // the mix, bounced the same way the bounce tool does it
             o.audio = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("squelchmoth-video-mix", ".wav", false);
             juce::var b (new juce::DynamicObject()); set (b, "path", o.audio.getFullPathName()); set (b, "format", "wav"); set (b, "bit_depth", 24);
             for (auto* k : { "start_seconds", "end_seconds", "start_bar", "end_bar" }) if (has (a, k)) set (b, k, a[juce::Identifier (k)]);
             auto r = call ("bounce", b); if (r.isError) return r;
             if (! o.audio.existsAsFile()) return ToolResult::error ("The mix could not be bounced for the video");
             if (auto reader = std::unique_ptr<juce::AudioFormatReader> (session.getEngine().getAudioFileFormatManager().readFormatManager.createReaderFor (o.audio)))
                 o.seconds = (double) reader->lengthInSamples / juce::jmax (1.0, reader->sampleRate);
             return frameExportStatus (Frame::Store::get().start (o, true), num (a, "wait_seconds", 20.0));
         });
    add ("video_frame",
         "Frame: the picture at a moment of the session. Gives the exact frame of the session's video (the one brought in with video_import) that is on screen at seconds (session time; default the playhead), "
         "with its frame number and timecode, and saves it as a PNG when path is given (width up to 1920, default 960). window true opens the Video window, which shows the picture following the playhead "
         "(Window menu > Video). Use it to check what is on screen at a cut, a marker or a hit. step moves the playhead by that many frames of the video first (negative goes back; "
         "it snaps to the frame on screen and stays inside the video). sheet N (2 to 64) with path saves a contact sheet instead: N stills evenly through the whole video in a grid "
         "(columns optional; width is then the width of one still, default 240), each with its timecode in the video underneath - read the saved picture to see what the video shows and where.",
         R"json({"type":"object","properties":{"seconds":{"type":"number"},"step":{"type":"integer"},"path":{"type":"string"},"width":{"type":"integer"},"sheet":{"type":"integer"},"columns":{"type":"integer"},"window":{"type":"boolean"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             auto& e = edit();
             if (flag (a, "window") && ui.openVideo) ui.openVideo();
             const juce::File video (e.state["frameVideo"].toString());
             if (! video.existsAsFile()) return ToolResult::error ("This session has no picture yet: bring a video in with video_import (or drop one on the Video window)");
             if (! Frame::ffmpeg().existsAsFile()) return ToolResult::error (Frame::missingText());
             const auto in = Frame::probe (video); if (! in.ok || ! in.hasVideo) return ToolResult::error (video.getFileName() + " has no picture");
             const double at = (double) e.state.getProperty ("frameVideoAt", 0.0);
             if (has (a, "sheet"))
             {
                 if (! has (a, "path")) return ToolResult::error ("A contact sheet needs path: where to save the picture");
                 const int count = juce::jlimit (2, 64, (int) num (a, "sheet", 12.0));
                 juce::String err; std::vector<double> times;
                 const auto img = Frame::contactSheet (video, in, count, (int) num (a, "columns", 0.0), juce::jlimit (64, 640, (int) num (a, "width", 240.0)),
                                                       [&in] (double t) { return VideoContent::timecode (t, in.fps); }, &times, err);
                 if (! img.isValid()) return ToolResult::error (err.isNotEmpty() ? err : juce::String ("Couldn't read the video's frames"));
                 auto out = juce::File (str (a, "path")); if (! out.hasFileExtension ("png")) out = out.withFileExtension ("png");
                 out.getParentDirectory().createDirectory(); out.deleteFile();
                 juce::FileOutputStream os (out); if (! os.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, os)) return ToolResult::error ("Couldn't write " + out.getFullPathName());
                 juce::String s = "Contact sheet of " + video.getFileName() + ": " + juce::String (count) + " stills, saved to " + out.getFullPathName() + " (" + juce::String (img.getWidth()) + " x " + juce::String (img.getHeight())
                                  + "). Left to right, top to bottom, with session times:";
                 for (size_t i = 0; i < times.size(); ++i) s << "\n" << (int) (i + 1) << ". " << VideoContent::timecode (times[i], in.fps) << " in the video = " << juce::String (at + times[i], 2) << " s of the session";
                 return ToolResult::ok (s);
             }
             double t = has (a, "seconds") ? num (a, "seconds") : e.getTransport().getPosition().inSeconds();
             juce::String moved;
             if (has (a, "step"))
             {
                 t = at + SqStrip::stepFrames (t - at, in.fps, (long long) num (a, "step", 0.0), in.seconds);
                 e.getTransport().setPosition (tracktion::TimePosition::fromSeconds (juce::jmax (0.0, t)));
                 moved = "Playhead moved to " + juce::String (t, 3) + " s. ";
             }
             const double inVideo = t - at;
             if (inVideo < 0) return ToolResult::ok ("At " + juce::String (t, 3) + " s the picture has not started: " + video.getFileName() + " begins at " + juce::String (at, 3) + " s on the timeline.");
             if (inVideo >= in.seconds) return ToolResult::ok ("At " + juce::String (t, 3) + " s the picture has ended: " + video.getFileName() + " runs from " + juce::String (at, 3) + " to " + juce::String (at + in.seconds, 3) + " s.");
             const auto idx = Frame::frameIndexAt (inVideo, in.fps);
             juce::String s = moved + "At " + juce::String (t, 3) + " s of the session: frame " + juce::String (idx) + " of " + video.getFileName() + " (" + VideoContent::timecode (inVideo, in.fps) + " in the video, " + juce::String (in.width) + " x " + juce::String (in.height) + ", "
                              + juce::String (in.fps, in.fps == std::floor (in.fps) ? 0 : 2) + " fps).";
             if (has (a, "path"))
             {
                 juce::String err; const auto img = Frame::grabFrame (video, in, inVideo, juce::jlimit (64, 1920, (int) num (a, "width", 960.0)), err);
                 if (! img.isValid()) return ToolResult::error (err);
                 auto out = juce::File (str (a, "path")); if (! out.hasFileExtension ("png")) out = out.withFileExtension ("png");
                 out.getParentDirectory().createDirectory(); out.deleteFile();
                 juce::FileOutputStream os (out); if (! os.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, os)) return ToolResult::error ("Couldn't write " + out.getFullPathName());
                 s << "\nSaved to " << out.getFullPathName() << " (" << img.getWidth() << " x " << img.getHeight() << ").";
             }
             return ToolResult::ok (s);
         });
}
