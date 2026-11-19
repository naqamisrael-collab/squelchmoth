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
         "The picture shows in the Video window and as a lane of stills on the timeline; video_move moves it. TO CUT THE PICTURE, EDIT ITS SOUND: the picture follows the clips of the video's sound, "
         "so split_clip, edit_clip (trim, move), delete_clip and duplicate_clip on that track cut, trim, move, remove and repeat the picture in step, with undo; video_cuts lists the pieces. When the computer has no FFmpeg, run video_export action install once. separate and vocal_remove also take video files directly.",
         R"json({"type":"object","properties":{"path":{"type":"string"},"start_seconds":{"type":"number"},"sound":{"type":"boolean"}},"required":["path"]})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const juce::File f (str (a, "path")); if (! f.existsAsFile()) return ToolResult::error ("No file at " + f.getFullPathName());
             if (! Frame::ffmpeg().existsAsFile()) return ToolResult::error (Frame::missingText());
             const auto in = Frame::probe (f); if (! in.ok) return ToolResult::error (in.error);
             auto& e = edit(); const double at = num (a, "start_seconds", 0.0);
             juce::String s = f.getFileName() + ": " + in.text() + ".";
             if (in.hasVideo) { e.state.setProperty ("frameVideo", f.getFullPathName(), nullptr); e.state.setProperty ("frameVideoAt", at, nullptr); e.state.removeProperty ("frameVideoSlip", nullptr); e.state.removeProperty ("frameVideoWav", nullptr); e.state.removeProperty ("frameVideoClip", nullptr); s << "\nRemembered as the session's picture (video_export picture source)."; }
             if (flag (a, "sound", true) && in.hasAudio)
             {
                 juce::String err; const auto wav = frameVideoFolder().getNonexistentChildFile (juce::File::createLegalFileName (f.getFileNameWithoutExtension()).substring (0, 60), ".wav", false);
                 if (! Frame::extractAudio (f, wav, TimeFmt::sampleRate (e), err)) return ToolResult::error (err);
                 juce::var t (new juce::DynamicObject()); set (t, "kind", "audio"); set (t, "name", f.getFileNameWithoutExtension().substring (0, 40));
                 const int idx = firstNumber (call ("add_track", t).text); if (idx <= 0) return ToolResult::error ("Couldn't add a track for the video's sound");
                 juce::var im (new juce::DynamicObject()); set (im, "track", idx); set (im, "path", wav.getFullPathName()); set (im, "start_seconds", at);
                 auto r = call ("import_audio", im); if (r.isError) return r;
                 // remember which clip is the video's own sound, so moving the video can move it too
                 if (auto* tr = dynamic_cast<te::AudioTrack*> (session.findTrack (juce::var (idx))))
                 {
                     te::Clip* last = nullptr; for (auto c : tr->getClips()) last = c;
                     if (last != nullptr && in.hasVideo)
                     {
                         e.state.setProperty ("frameVideoClip", last->itemID.toString(), nullptr);
                         // from here the picture follows every clip that plays this file: cutting the sound cuts the picture
                         if (auto* w = dynamic_cast<te::WaveAudioClip*> (last)) e.state.setProperty ("frameVideoWav", w->getOriginalFile().getFullPathName(), nullptr);
                     }
                 }
                 if (ui.refresh) ui.refresh();
                 s << "\nIts sound is on track " << idx << " at " << juce::String (at, 2) << " s (" << wav.getFileName() << ").";
             }
             else if (! in.hasAudio) s << "\nIt has no sound to import.";
             return ToolResult::ok (s);
         });

    add ("video_proxy",
         "Frame: the proxy of the session's video - a small copy (up to 640 wide) made in parts in the background, so stills, the strip, the timeline lane, contact sheets and scrubbing stay fast anywhere "
         "in a long or large video. Frame numbers are the same as in the video. Saved frames (video_frame path) and video_export always use the video itself, never the proxy. "
         "action: status (default), make (wait_seconds to wait for it, up to 600), remove (deletes its files), auto with on true/false (make proxies unasked for videos wider than 1280 or ten minutes and longer; on by default). "
         "Proxies live in the app's data folder and ones unused for a month are cleared.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["status","make","remove","auto"]},"on":{"type":"boolean"},"wait_seconds":{"type":"number"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             const auto action = str (a, "action", "status");
             if (action == "auto")
             {
                 if (has (a, "on")) Prefs::set ("frame_autoProxy", flag (a, "on", true) ? 1 : 0);
                 return ToolResult::ok (VideoProxy::autoOn() ? "Proxies are made without asking for videos wider than 1280 or ten minutes and longer." : "Proxies are only made when asked for.");
             }
             auto& e = edit(); const juce::File video (e.state["frameVideo"].toString());
             if (! video.existsAsFile()) return ToolResult::error ("This session has no video yet: bring one in with video_import");
             if (! Frame::ffmpeg().existsAsFile()) return ToolResult::error (Frame::missingText());
             auto& px = Frame::Proxies::get();
             if (action == "remove") { px.remove (video); return ToolResult::ok ("The proxy of " + video.getFileName() + " is removed; stills come from the video itself again."); }
             const auto in = Frame::probe (video); if (! in.ok || ! in.hasVideo) return ToolResult::error (video.getFileName() + " has no picture");
             auto st = px.open (video, in, action == "make");
             if (action == "make")
             {
                 const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) (juce::jlimit (0.0, 600.0, num (a, "wait_seconds", 0.0)) * 1000.0);
                 while (! st.complete() && st.error.isEmpty() && juce::Time::getMillisecondCounter() < until) { juce::Thread::sleep (50); st = px.status (video); }
             }
             else if (action != "status") return ToolResult::error ("action is status, make, remove or auto");
             if (st.error.isNotEmpty()) return ToolResult::error (VideoProxy::sentence (video, st));
             return ToolResult::ok (VideoProxy::sentence (video, st));
         });

    add ("video_text",
         "Frame: words on the picture - titles, lower thirds and captions. They show in the Video window as they will look and video_export burns them into the picture (its text option: burn (default), "
         "file = a caption file (.srt) beside the video instead, both, none). Times are session seconds.\n"
         " action add: text (use \\n for a new line), start_seconds (default the playhead), length_seconds or end_seconds (default 4 s for a title, 3 s otherwise), style caption | title | lower third (default caption), "
         "place top | middle | bottom (default: title in the middle, others at the bottom), size (a factor, 1 = normal), colour (hex, default white).\n"
         " action list (default). action edit: number (from the list) plus any of the same fields. action remove: number, or all true.\n"
         " action import_srt: path of a caption file, offset_seconds (where its zero falls on the timeline, default 0), replace true to remove the present captions first. action export_srt: path, from_seconds (default 0).\n"
         "There is no automatic captioning from speech. Texts stay where they are when the video is moved or cut. One undo step each.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["add","list","edit","remove","import_srt","export_srt"]},"text":{"type":"string"},"start_seconds":{"type":"number"},"end_seconds":{"type":"number"},
            "length_seconds":{"type":"number"},"style":{"type":"string"},"place":{"type":"string"},"size":{"type":"number"},"colour":{"type":"string"},"number":{"type":"integer"},"all":{"type":"boolean"},
            "path":{"type":"string"},"offset_seconds":{"type":"number"},"from_seconds":{"type":"number"},"replace":{"type":"boolean"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             auto& e = edit(); auto items = VideoText::read (e);
             const auto action = str (a, "action", has (a, "text") ? "add" : "list");
             auto listText = [&] (const std::vector<SqText::Item>& v)
             {
                 if (v.empty()) return juce::String ("There are no texts on the picture.");
                 juce::String s = juce::String ((int) v.size()) + (v.size() == 1 ? " text" : " texts") + " on the picture:";
                 for (size_t i = 0; i < v.size() && i < 200; ++i) s << "\n" << VideoText::line ((int) i + 1, v[i]);
                 if (v.size() > 200) s << "\n... and " << (int) v.size() - 200 << " more";
                 return s;
             };
             auto apply = [&] (SqText::Item& it)
             {
                 if (has (a, "text")) it.text = str (a, "text").replace ("\\n", "\n").toStdString();
                 if (has (a, "style")) it.style = SqText::styleFrom (str (a, "style").toStdString());
                 if (has (a, "place")) it.place = SqText::placeFrom (str (a, "place").toStdString());
                 if (has (a, "size")) it.size = juce::jlimit (0.25, 4.0, num (a, "size", 1.0));
                 if (has (a, "colour")) { auto c = str (a, "colour").trim().trimCharactersAtStart ("#"); if (c.length() == 6) c = "ff" + c; if (c.length() == 8 && c.containsOnly ("0123456789abcdefABCDEF")) it.colour = (unsigned int) c.getHexValue64(); }
                 const double length = it.end - it.start;
                 if (has (a, "start_seconds")) { it.start = num (a, "start_seconds"); if (! has (a, "end_seconds") && ! has (a, "length_seconds")) it.end = it.start + length; }
                 if (has (a, "end_seconds")) it.end = num (a, "end_seconds");
                 else if (has (a, "length_seconds")) it.end = it.start + num (a, "length_seconds");
                 SqText::tidy (it);
             };
             if (action == "list") return ToolResult::ok (listText (items));
             if (action == "add")
             {
                 if (str (a, "text").trim().isEmpty()) return ToolResult::error ("add needs text: the words to show");
                 SqText::Item it; it.style = SqText::styleFrom (str (a, "style").toStdString());
                 it.start = e.getTransport().getPosition().inSeconds(); it.end = it.start + (it.style == SqText::title ? 4.0 : 3.0);
                 apply (it); items.push_back (it); VideoText::write (e, items, "Add text");
                 return ToolResult::ok ("Added: " + VideoText::line ((int) items.size(), it));
             }
             if (action == "edit" || action == "remove")
             {
                 if (action == "remove" && flag (a, "all")) { const int n = (int) items.size(); VideoText::write (e, {}, "Remove all texts"); return ToolResult::ok ("Removed " + juce::String (n) + (n == 1 ? " text." : " texts.")); }
                 const int n = (int) num (a, "number", 0.0);
                 if (n < 1 || n > (int) items.size()) return ToolResult::error ("number is the text's place in the list (1 to " + juce::String ((int) items.size()) + "); video_text action list shows them");
                 if (action == "remove") { const auto gone = VideoText::line (n, items[(size_t) n - 1]); items.erase (items.begin() + (n - 1)); VideoText::write (e, items, "Remove text"); return ToolResult::ok ("Removed: " + gone); }
                 apply (items[(size_t) n - 1]); VideoText::write (e, items, "Edit text");
                 return ToolResult::ok ("Now: " + VideoText::line (n, items[(size_t) n - 1]));
             }
             if (action == "import_srt")
             {
                 const juce::File f (str (a, "path")); if (! f.existsAsFile()) return ToolResult::error ("No caption file at " + f.getFullPathName());
                 if (f.getSize() > 20 * 1024 * 1024) return ToolResult::error ("That file is too large to be a caption file");
                 const auto read = SqText::parseSrt (f.loadFileAsString().toStdString(), num (a, "offset_seconds", 0.0));
                 if (read.empty()) return ToolResult::error ("No captions were found in " + f.getFileName() + " (it should be a .srt caption file)");
                 if (flag (a, "replace")) items.erase (std::remove_if (items.begin(), items.end(), [] (const SqText::Item& i) { return i.style == SqText::caption; }), items.end());
                 items.insert (items.end(), read.begin(), read.end()); VideoText::write (e, items, "Import captions");
                 return ToolResult::ok ("Brought in " + juce::String ((int) read.size()) + " captions from " + f.getFileName() + ", from " + juce::String (read.front().start, 2) + " to " + juce::String (read.back().end, 2) + " s. " + juce::String ((int) items.size()) + " texts in all.");
             }
             if (action == "export_srt")
             {
                 if (! has (a, "path")) return ToolResult::error ("export_srt needs path: where to save the caption file");
                 if (items.empty()) return ToolResult::error ("There are no texts to write");
                 auto f = juce::File (str (a, "path")); if (! f.hasFileExtension ("srt")) f = f.withFileExtension ("srt");
                 f.getParentDirectory().createDirectory();
                 if (! f.replaceWithText (juce::String::fromUTF8 (SqText::writeSrt (items, num (a, "from_seconds", 0.0)).c_str()))) return ToolResult::error ("Couldn't write " + f.getFullPathName());
                 return ToolResult::ok ("Caption file written: " + f.getFullPathName());
             }
             return ToolResult::error ("action is add, list, edit, remove, import_srt or export_srt");
         });

    add ("video_cuts",
         "Frame: the session's picture as pieces - which stretch of the video shows where on the timeline, and the gaps. The picture follows the clips that play the video's own sound: to cut, trim, move, "
         "delete or repeat a piece of the picture, do it to that sound clip (split_clip, edit_clip, delete_clip, duplicate_clip; the mouse and keys in the timeline do the same). Where two pieces overlap the "
         "lower track shows; where there is no piece the picture is black. video_export exports the cut picture with the mix.",
         R"json({"type":"object","properties":{}})json",
         [this] (const juce::var&) -> ToolResult
         {
             auto& e = edit(); const juce::File video (e.state["frameVideo"].toString());
             if (video == juce::File()) return ToolResult::ok ("This session has no video.");
             double secs = 0; if (Frame::ffmpeg().existsAsFile() && video.existsAsFile()) { const auto in = Frame::probe (video); if (in.ok) secs = in.seconds; }
             const auto pic = VideoCuts::read (e, secs);
             juce::String s = VideoCuts::describe (pic, secs > 0 ? secs : 1.0e9);
             if (pic.followsSound) { juce::StringArray tracks; for (auto* c : VideoCuts::clips (e)) if (auto* t = c->getTrack()) tracks.addIfNotAlreadyThere (t->getName()); if (! tracks.isEmpty()) s << "\nIts sound clips are on: " << tracks.joinIntoString (", ") << "."; }
             else s << "\nNo sound clip is tied to this video (it has no sound, or it was brought in before version 1.30), so it stays one piece. Bring it in again with video_import to cut it.";
             return ToolResult::ok (s);
         });

    add ("video_move",
         "Frame: moves the session's whole video along the timeline (the same as dragging a block in the video lane). start_seconds puts the video's first frame there; by_seconds or by_frames (frames of the video) "
         "moves it from where it is. Every clip of its sound moves with it, so all its pieces keep their spacing; sound false instead slips the picture against its sound. Never before the start of the session. One undo step. "
         "To move ONE piece, move its sound clip (edit_clip).",
         R"json({"type":"object","properties":{"start_seconds":{"type":"number"},"by_seconds":{"type":"number"},"by_frames":{"type":"number"},"sound":{"type":"boolean"}}})json",
         [this] (const juce::var& a) -> ToolResult
         {
             auto& e = edit();
             if (e.state["frameVideo"].toString().isEmpty()) return ToolResult::error ("This session has no video yet: bring one in with video_import");
             const double at = VideoMove::currentStart (e); double to = at;
             if (has (a, "start_seconds")) to = num (a, "start_seconds");
             else if (has (a, "by_seconds")) to = at + num (a, "by_seconds");
             else if (has (a, "by_frames"))
             {
                 const juce::File video (e.state["frameVideo"].toString());
                 if (! Frame::ffmpeg().existsAsFile()) return ToolResult::error (Frame::missingText());
                 const auto in = Frame::probe (video); if (! in.ok || in.fps <= 0) return ToolResult::error ("Couldn't read the video's frame rate; use by_seconds");
                 to = at + num (a, "by_frames") / in.fps;
             }
             else return ToolResult::error ("Say where: start_seconds, by_seconds or by_frames");
             const auto s = VideoMove::moveTo (e, to, flag (a, "sound", true));
             if (ui.refresh) ui.refresh();
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
            "name":{"type":"string"},"path":{"type":"string"},"job":{"type":"integer"},"wait_seconds":{"type":"number"},"text":{"type":"string","enum":["burn","file","both","none"]}}})json",
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
             VideoCuts::Picture pic; Frame::Info vin; bool cutPicture = false;
             if (o.picture == "source" && ! has (a, "video"))
             {
                 vin = Frame::probe (video); if (! vin.ok || ! vin.hasVideo) return ToolResult::error (video.getFileName() + " has no picture");
                 pic = VideoCuts::read (e, vin.seconds);
                 if (pic.pieces.empty()) return ToolResult::error ("There is no picture to export: every clip of the video's sound was deleted. Undo, or use picture waveform / black");
                 cutPicture = ! SqCut::isWhole (pic.pieces, pic.slip, vin.seconds);
                 if (cutPicture && SqCut::hasSpeedChange (pic.pieces)) return ToolResult::error ("A piece of the video's sound has had its speed changed; the picture cannot be exported at a changed speed yet. Set that clip back to normal speed, or export with picture waveform / black");
                 if (! cutPicture) o.videoOffset = startS - pic.pieces[0].at;
                 else if (! o.resize) { o.width = juce::jmax (16, vin.width) & ~1; o.height = juce::jmax (16, vin.height) & ~1; if (! has (a, "fps")) o.fps = juce::jlimit (1.0, 120.0, vin.fps); }
                 o.sourceFps = vin.fps;
             }
             else if (o.picture == "source") o.videoOffset = startS;
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
             if (cutPicture) o.stretches = SqCut::plan (pic.pieces, pic.slip, vin.seconds, startS, startS + o.seconds, 0.5 / juce::jmax (1.0, o.fps));
             // ---- words on the picture: burned in, written beside as a caption file, or both
             juce::String textNote;
             {
                 const auto mode = str (a, "text", "burn").toLowerCase();
                 std::vector<SqText::Item> inRange;
                 for (auto& it : VideoText::read (e)) if (it.end > startS && it.start < startS + o.seconds && ! it.text.empty()) inRange.push_back (it);
                 if (! inRange.empty() && (mode == "file" || mode == "both"))
                 {
                     const auto srt = o.out.withFileExtension ("srt");
                     if (srt.replaceWithText (juce::String::fromUTF8 (SqText::writeSrt (inRange, startS).c_str()))) textNote << "\nCaption file: " << srt.getFullPathName() << " (" << (int) inRange.size() << " texts).";
                 }
                 if (! inRange.empty() && (mode == "burn" || mode == "both"))
                 {
                     if (inRange.size() > 150) return ToolResult::error ("This range has " + juce::String ((int) inRange.size()) + " texts; up to 150 can be burned into one export. Export with text file (a caption file beside the video) or a shorter range");
                     if (o.picture == "source")
                     {
                         if (! vin.ok) { vin = Frame::probe (video); if (! vin.ok || ! vin.hasVideo) return ToolResult::error (video.getFileName() + " has no picture"); o.sourceFps = vin.fps; }
                         if (! o.resize) { o.width = juce::jmax (16, vin.width) & ~1; o.height = juce::jmax (16, vin.height) & ~1; if (! has (a, "fps")) o.fps = juce::jlimit (1.0, 120.0, vin.fps); }
                         if (o.stretches.empty())
                         {
                             if (has (a, "video")) o.stretches.push_back ({ startS, o.seconds, startS, true });
                             else o.stretches = SqCut::plan (pic.pieces, pic.slip, vin.seconds, startS, startS + o.seconds, 0.5 / juce::jmax (1.0, o.fps));
                         }
                     }
                     const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Squelchmoth").getChildFile ("frame-text");
                     folder.deleteRecursively(); folder.createDirectory();
                     int k = 0;
                     for (auto& it : inRange)
                     {
                         const auto png = folder.getChildFile ("text" + juce::String (k++).paddedLeft ('0', 4) + ".png");
                         juce::FileOutputStream os (png);
                         if (! os.openedOk() || ! juce::PNGImageFormat().writeImageToStream (VideoText::layer (it, o.width, o.height), os)) return ToolResult::error ("Couldn't prepare the text for the video");
                         o.overlays.push_back ({ png, juce::jmax (0.0, it.start - startS), juce::jmin (o.seconds, it.end - startS) });
                     }
                     textNote << "\n" << (int) inRange.size() << (inRange.size() == 1 ? " text is" : " texts are") << " burned into the picture.";
                 }
             }
             if (textNote.isNotEmpty()) { auto r2 = frameExportStatus (Frame::Store::get().start (o, true), num (a, "wait_seconds", 20.0)); if (! r2.isError) r2.text << textNote; return r2; }
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
             const auto pic = VideoCuts::read (e, in.seconds);
             const double at = VideoMove::currentStart (e);
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
                 for (size_t i = 0; i < times.size(); ++i) s << "\n" << (int) (i + 1) << ". " << VideoContent::timecode (times[i], in.fps) << " in the video"
                                                               << (SqCut::whereIs (pic.pieces, times[i], pic.slip, in.seconds) >= 0 ? " = " + juce::String (SqCut::whereIs (pic.pieces, times[i], pic.slip, in.seconds), 2) + " s of the session" : juce::String (" (cut out of the session)"));
                 return ToolResult::ok (s);
             }
             double t = has (a, "seconds") ? num (a, "seconds") : e.getTransport().getPosition().inSeconds();
             juce::String moved;
             if (has (a, "step"))
             {
                 {
                     const auto h0 = SqCut::lookup (pic.pieces, t, pic.slip, in.seconds); const auto frames = (long long) num (a, "step", 0.0);
                     const double there = h0.shows() ? SqCut::whereIs (pic.pieces, SqStrip::stepFrames (h0.src, in.fps, frames, in.seconds), pic.slip, in.seconds) : -1.0;
                     t = there >= 0 ? there : juce::jmax (0.0, t + (double) frames / in.fps);
                 }
                 e.getTransport().setPosition (tracktion::TimePosition::fromSeconds (juce::jmax (0.0, t)));
                 moved = "Playhead moved to " + juce::String (t, 3) + " s. ";
             }
             const auto hitNow = SqCut::lookup (pic.pieces, t, pic.slip, in.seconds); const double inVideo = hitNow.src;
             if (! hitNow.shows())
             {
                 const double ps = SqCut::start (pic.pieces), pe = SqCut::end (pic.pieces);
                 if (pic.pieces.empty()) return ToolResult::ok ("There is no picture anywhere: every clip of the video's sound was deleted.");
                 if (t < ps) return ToolResult::ok ("At " + juce::String (t, 3) + " s the picture has not started: " + video.getFileName() + " begins at " + juce::String (ps, 3) + " s on the timeline.");
                 if (t >= pe) return ToolResult::ok ("At " + juce::String (t, 3) + " s the picture has ended: " + video.getFileName() + " runs from " + juce::String (ps, 3) + " to " + juce::String (pe, 3) + " s.");
                 return ToolResult::ok ("At " + juce::String (t, 3) + " s there is no picture: it falls in a gap between pieces (video_cuts lists them).");
             }
             const auto idx = Frame::frameIndexAt (inVideo, in.fps);
             juce::String s = moved + "At " + juce::String (t, 3) + " s of the session: frame " + juce::String (idx) + " of " + video.getFileName() + " (" + VideoContent::timecode (inVideo, in.fps) + " in the video, " + juce::String (in.width) + " x " + juce::String (in.height) + ", "
                              + juce::String (in.fps, in.fps == std::floor (in.fps) ? 0 : 2) + " fps).";
             if (has (a, "path"))
             {
                 juce::String err; const auto img = Frame::grabFrame (video, in, inVideo, juce::jlimit (64, 1920, (int) num (a, "width", 960.0)), err, false);   // a saved frame always comes from the video itself
                 if (! img.isValid()) return ToolResult::error (err);
                 auto out = juce::File (str (a, "path")); if (! out.hasFileExtension ("png")) out = out.withFileExtension ("png");
                 out.getParentDirectory().createDirectory(); out.deleteFile();
                 juce::FileOutputStream os (out); if (! os.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, os)) return ToolResult::error ("Couldn't write " + out.getFullPathName());
                 s << "\nSaved to " << out.getFullPathName() << " (" << img.getWidth() << " x " << img.getHeight() << ").";
             }
             return ToolResult::ok (s);
         });
}
