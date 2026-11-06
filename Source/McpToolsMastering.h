// Squelchmoth - Claude commands for the Mastering window. Included inside class McpTools (see McpTools.h).
// Every command here needs the mastering project hook (ui.mastering).

void registerMasteringTools()
{
    auto proj = [this] (juce::String& err) -> MasteringProject*
    {
        auto p = ui.mastering ? ui.mastering() : nullptr;
        if (p == nullptr) { err = "Mastering is not available (UI not running)"; return nullptr; }
        return p;
    };
    auto needOpen = [proj] (juce::String& err) -> MasteringProject*
    {
        auto p = proj (err);
        if (p != nullptr && ! p->isOpen()) p->newProject ("Album");
        return p;
    };
    auto trackArgM = [] (MasteringProject& p, const juce::var& a, juce::String& err) -> te::AudioTrack*
    {
        auto v = a["track"];
        if (v.isInt() || v.isDouble()) { auto t = p.track ((int) v - 1); if (t == nullptr) err = "No album track " + v.toString(); return t; }
        auto name = v.toString().trim();
        for (auto t : p.tracks()) if (t->getName().equalsIgnoreCase (name)) return t;
        for (auto t : p.tracks()) if (t->getName().containsIgnoreCase (name) && name.length() >= 3) return t;
        err = "No album track matches '" + name + "'";
        return nullptr;
    };
    auto describe = [] (MasteringProject& p)
    {
        auto res = obj();
        set (res, "project", p.projectName());
        set (res, "file", p.file.getFullPathName());
        set (res, "album", p.album ("mAlbum")); set (res, "artist", p.album ("mAlbumArtist")); set (res, "genre", p.album ("mGenre")); set (res, "year", p.album ("mYear")); set (res, "upc", p.album ("mUpc"));
        set (res, "total_length", MasteringProject::timeText (p.totalLength()));
        set (res, "playing", p.isPlaying());
        juce::Array<juce::var> ts;
        int i = 0;
        for (auto t : p.tracks())
        {
            auto o = obj();
            set (o, "number", ++i); set (o, "title", t->getName()); set (o, "artist", MasteringProject::meta (*t, "mArtist")); set (o, "isrc", MasteringProject::meta (*t, "mIsrc"));
            set (o, "enabled", MasteringProject::enabledOf (*t)); set (o, "pause_seconds", MasteringProject::pauseOf (*t));
            set (o, "start", MasteringProject::timeText (p.startOf (*t))); set (o, "length", MasteringProject::timeText (p.lengthOf (*t)));
            set (o, "source", MasteringProject::meta (*t, "mSource")); if (MasteringProject::meta (*t, "mSourceSession").isNotEmpty()) set (o, "session", MasteringProject::meta (*t, "mSourceSession"));
            if (t->state.hasProperty ("mLufsPre")) { set (o, "lufs_pre", (float) t->state.getProperty ("mLufsPre")); set (o, "lra_pre", (float) t->state.getProperty ("mLraPre")); set (o, "true_peak_pre", (float) t->state.getProperty ("mTpPre")); }
            if (t->state.hasProperty ("mLufsPost")) { set (o, "lufs_post", (float) t->state.getProperty ("mLufsPost")); set (o, "true_peak_post", (float) t->state.getProperty ("mTpPost")); }
            juce::StringArray ins; for (auto pl : Session::getInserts (t->pluginList, t)) ins.add (Signature::displayName (*pl));
            set (o, "inserts", ins.joinIntoString (", "));
            if (auto vp = t->getVolumePlugin()) set (o, "fader_db", vp->getVolumeDb());
            ts.add (o);
        }
        set (res, "tracks", ts);
        juce::StringArray mi; for (auto pl : p.masterInserts()) mi.add (Signature::displayName (*pl));
        set (res, "master_inserts", mi.joinIntoString (", "));
        if (p.edit != nullptr) if (auto mv = p.edit->getMasterVolumePlugin()) set (res, "master_fader_db", mv->getVolumeDb());
        return res;
    };

    add ("mastering_status", "The Mastering window's album project: tracks in order with pauses, ISRCs, loudness readings, inserts, and the master rack. Opens the Mastering window.",
         R"json({"type":"object","properties":{}})json",
         [this, proj, describe] (const juce::var&)
         {
             juce::String err; auto p = proj (err); if (! p) return ToolResult::error (err);
             if (ui.showWindow) ui.showWindow ("mastering", true);
             if (! p->isOpen()) return ToolResult::ok ("No mastering project open yet - call mastering_new or mastering_add_tracks.");
             return ToolResult::ok (describe (*p));
         });

    add ("mastering_new", "Start a new album mastering project (name = album title) or open an existing .sqmaster file (path).",
         R"json({"type":"object","properties":{"name":{"type":"string"},"path":{"type":"string"}},"required":[]})json",
         [this, proj, describe] (const juce::var& a)
         {
             juce::String err; auto p = proj (err); if (! p) return ToolResult::error (err);
             if (has (a, "path")) { if (! p->openProject (juce::File (str (a, "path")))) return ToolResult::error ("Could not open " + str (a, "path")); }
             else p->newProject (str (a, "name"));
             if (ui.showWindow) ui.showWindow ("mastering", true);
             return ToolResult::ok (describe (*p));
         });

    add ("mastering_add_tracks", "Add album tracks: audio file paths (WAV/AIFF/FLAC/MP3/OGG) and/or 'current_session' to bounce the session open in the Edit window. Tracks are placed in order with Red Book pauses (2 s before track 1).",
         R"json({"type":"object","properties":{"paths":{"type":"array","items":{"type":"string"}},"current_session":{"type":"boolean"}},"required":[]})json",
         [this, needOpen, describe] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             juce::StringArray added, failed;
             if (auto arr = a["paths"].getArray())
                 for (auto& v : *arr) { auto f = juce::File (v.toString()); if (auto t = p->addAudioFile (f)) added.add (t->getName()); else failed.add (f.getFileName()); }
             if (flag (a, "current_session")) { if (auto t = p->addCurrentSession()) added.add (t->getName() + " (bounced from the session)"); else failed.add ("current session"); }
             if (added.isEmpty()) return ToolResult::error ("Nothing added" + (failed.isEmpty() ? juce::String() : ": " + failed.joinIntoString (", ")));
             if (ui.showWindow) ui.showWindow ("mastering", true);
             auto res = describe (*p); set (res, "added", added.joinIntoString (", ")); if (! failed.isEmpty()) set (res, "failed", failed.joinIntoString (", "));
             return ToolResult::ok (res);
         });

    add ("mastering_set_track", "Edit an album track: title, artist, songwriter, composer, isrc, pause_seconds (gap before it), enabled, fade_in / fade_out seconds, fader_db, flags (pre_emphasis, copy_permitted, copy_protected), move_to (new position 1-based), remove.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]},"title":{"type":"string"},"artist":{"type":"string"},"songwriter":{"type":"string"},"composer":{"type":"string"},"isrc":{"type":"string"},"pause_seconds":{"type":"number"},"enabled":{"type":"boolean"},"fade_in":{"type":"number"},"fade_out":{"type":"number"},"fader_db":{"type":"number"},"pre_emphasis":{"type":"boolean"},"copy_permitted":{"type":"boolean"},"copy_protected":{"type":"boolean"},"move_to":{"type":"integer"},"remove":{"type":"boolean"},"auto_isrc":{"type":"boolean","description":"with isrc on track 1: number the rest of the album from it"}},"required":["track"]})json",
         [this, needOpen, trackArgM, describe] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             auto t = trackArgM (*p, a, err); if (! t) return ToolResult::error (err);
             if (flag (a, "remove")) { auto n = t->getName(); p->removeTrack (*t); return ToolResult::ok ("Removed " + n); }
             if (has (a, "title")) t->setName (str (a, "title"));
             if (has (a, "artist")) p->setMeta (*t, "mArtist", str (a, "artist"));
             if (has (a, "songwriter")) p->setMeta (*t, "mSongwriter", str (a, "songwriter"));
             if (has (a, "composer")) p->setMeta (*t, "mComposer", str (a, "composer"));
             if (has (a, "isrc")) { p->setMeta (*t, "mIsrc", str (a, "isrc").trim().toUpperCase().removeCharacters ("-")); if (flag (a, "auto_isrc")) p->autoIncrementIsrc (str (a, "isrc")); }
             if (has (a, "pause_seconds")) p->setMeta (*t, "mPause", num (a, "pause_seconds"));
             if (has (a, "enabled")) p->setMeta (*t, "mEnabled", flag (a, "enabled"));
             if (has (a, "fade_in") || has (a, "fade_out")) { auto c = MasteringProject::clipOf (*t); p->setFades (*t, has (a, "fade_in") ? num (a, "fade_in") : (c ? c->getFadeIn().inSeconds() : 0.0), has (a, "fade_out") ? num (a, "fade_out") : (c ? c->getFadeOut().inSeconds() : 0.0)); }
             if (has (a, "fader_db")) if (auto vp = t->getVolumePlugin()) vp->setVolumeDb ((float) num (a, "fader_db"));
             if (has (a, "pre_emphasis")) p->setMeta (*t, "mPre", flag (a, "pre_emphasis"));
             if (has (a, "copy_permitted")) p->setMeta (*t, "mDcp", flag (a, "copy_permitted"));
             if (has (a, "copy_protected")) p->setMeta (*t, "mScms", flag (a, "copy_protected"));
             if (has (a, "move_to")) { auto cur = p->tracks().indexOf (t); p->moveTrack (*t, (int) num (a, "move_to") - 1 - cur); }
             p->sequence();
             return ToolResult::ok (describe (*p));
         });

    add ("mastering_set_album", "Album meta-information: album, artist, genre, year, upc (12-13 digits), label, catalog, copyright, comment, artwork (image path). Used for CD-Text, file tags, DDP and folder names.",
         R"json({"type":"object","properties":{"album":{"type":"string"},"artist":{"type":"string"},"genre":{"type":"string"},"year":{"type":"string"},"upc":{"type":"string"},"label":{"type":"string"},"catalog":{"type":"string"},"copyright":{"type":"string"},"comment":{"type":"string"},"artwork":{"type":"string"}},"required":[]})json",
         [this, needOpen, describe] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             const std::pair<const char*, const char*> keys[] = { { "album", "mAlbum" }, { "artist", "mAlbumArtist" }, { "genre", "mGenre" }, { "year", "mYear" }, { "upc", "mUpc" }, { "label", "mLabel" }, { "catalog", "mCatalog" }, { "copyright", "mCopyright" }, { "comment", "mComment" }, { "artwork", "mArtwork" } };
             for (auto& [k, id] : keys) if (has (a, k)) p->setAlbum (id, str (a, k));
             p->save();
             return ToolResult::ok (describe (*p));
         });

    add ("mastering_detect_loudness", "Measure EBU R128 loudness of album tracks: integrated LUFS, loudness range, true peak and RMS, pre-FX (the file) and post-FX (through the track and master racks). track: one track, or omit for all.",
         R"json({"type":"object","properties":{"track":{"type":["string","integer"]}},"required":[]})json",
         [this, needOpen, trackArgM] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             juce::Array<te::AudioTrack*> ts;
             if (has (a, "track")) { auto t = trackArgM (*p, a, err); if (! t) return ToolResult::error (err); ts.add (t); } else ts = p->tracks();
             juce::Array<juce::var> out;
             for (auto t : ts)
             {
                 auto pre = p->detectLoudness (*t, false), post = p->detectLoudness (*t, true);
                 auto o = obj(); set (o, "track", t->getName());
                 set (o, "pre_lufs", pre.integrated); set (o, "pre_lra", pre.range); set (o, "pre_true_peak", pre.truePeak); set (o, "pre_rms", pre.rms);
                 set (o, "post_lufs", post.integrated); set (o, "post_lra", post.range); set (o, "post_true_peak", post.truePeak); set (o, "post_rms", post.rms);
                 out.add (o);
             }
             auto res = obj(); set (res, "loudness", out);
             return ToolResult::ok (res);
         });

    add ("mastering_insert", "Insert, remove or bypass a plugin in the Mastering window racks. target: 'master' or an album track. plugin: any Signature / built-in / installed plugin name. action: insert (default), remove, bypass, unbypass. slot: plugin name or index for remove/bypass.",
         R"json({"type":"object","properties":{"target":{"type":["string","integer"]},"plugin":{"type":"string"},"action":{"type":"string","enum":["insert","remove","bypass","unbypass"]},"slot":{"type":["string","integer"]},"preset":{"type":"string"}},"required":["target"]})json",
         [this, needOpen, trackArgM, describe] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             te::AudioTrack* t = nullptr;
             te::PluginList* list = nullptr;
             bool master = str (a, "target").equalsIgnoreCase ("master");
             if (master) list = &p->edit->getMasterPluginList();
             else { auto ta = obj(); set (ta, "track", a["target"]); t = trackArgM (*p, ta, err); if (! t) return ToolResult::error (err); list = &t->pluginList; }
             auto inserts = master ? p->masterInserts() : Session::getInserts (*list, t);
             auto action = str (a, "action", "insert");
             if (action == "insert")
             {
                 auto pl = session.createPluginByName (str (a, "plugin"), err);
                 if (pl == nullptr) return ToolResult::error (err);
                 int idx = master ? (p->tap != nullptr ? list->indexOf (p->tap) : -1) : Session::insertEndIndex (*list, t);
                 list->insertPlugin (pl, idx, nullptr);
                 if (has (a, "preset")) if (auto e = Signature::findForPlugin (*pl)) for (auto& pr : e->presets) if (juce::String (pr.name).equalsIgnoreCase (str (a, "preset"))) Signature::applyPreset (*pl, pr);
                 p->sendChangeMessage();
                 return ToolResult::ok ("Inserted " + Signature::displayName (*pl) + " on " + (master ? juce::String ("the master rack") : t->getName()));
             }
             te::Plugin* target = nullptr;
             auto sv = a["slot"];
             if (sv.isInt()) { auto i = (int) sv - 1; if (i >= 0 && i < inserts.size()) target = inserts[i]; }
             else for (auto pl : inserts) if (Signature::displayName (*pl).equalsIgnoreCase (sv.toString()) || Signature::displayName (*pl).containsIgnoreCase (sv.toString())) { target = pl; break; }
             if (target == nullptr) return ToolResult::error ("No insert matches slot " + sv.toString());
             auto name = Signature::displayName (*target);
             if (action == "remove") target->deleteFromParent();
             else if (action == "bypass") target->setEnabled (false);
             else if (action == "unbypass") target->setEnabled (true);
             p->sendChangeMessage();
             return ToolResult::ok (name + " " + action + (action == "remove" ? "d" : "ed"));
         });

    add ("mastering_master_fader", "Set the Mastering window's master fader (dB).",
         R"json({"type":"object","properties":{"db":{"type":"number"}},"required":["db"]})json",
         [needOpen] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             if (auto mv = p->edit->getMasterVolumePlugin()) mv->setVolumeDb ((float) num (a, "db"));
             return ToolResult::ok ("Master fader " + juce::String (num (a, "db"), 1) + " dB");
         });

    add ("mastering_transport", "Play / stop / locate the album in the Mastering window (stops the Edit window's transport). position_seconds optional.",
         R"json({"type":"object","properties":{"action":{"type":"string","enum":["play","stop","locate","status"]},"position_seconds":{"type":"number"}},"required":["action"]})json",
         [needOpen] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             auto act = str (a, "action");
             if (act == "play") p->play (has (a, "position_seconds") ? num (a, "position_seconds") : -1.0);
             else if (act == "stop") p->stop();
             else if (act == "locate") p->locate (num (a, "position_seconds"));
             auto res = obj(); set (res, "playing", p->isPlaying()); set (res, "position", MasteringProject::timeText (p->position()));
             return ToolResult::ok (res);
         });

    add ("mastering_meters", "Live readings from the Mastering window meters while playing: momentary / short-term / integrated LUFS, loudness range, true peak, peak L/R, phase correlation, clip indicator.",
         R"json({"type":"object","properties":{"reset":{"type":"boolean"}},"required":[]})json",
         [this, needOpen] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             if (flag (a, "reset")) p->hub.reset();
             p->hub.update();
             auto& lm = p->hub.loudness;
             auto res = obj();
             set (res, "momentary_lufs", lm.getMomentary()); set (res, "short_term_lufs", lm.getShortTerm()); set (res, "integrated_lufs", lm.getIntegrated()); set (res, "loudness_range_lu", lm.getRange()); set (res, "true_peak_dbtp", lm.getTruePeak());
             set (res, "peak_l_db", p->hub.peak[0]); set (res, "peak_r_db", p->hub.peak[1]); set (res, "correlation", p->hub.phase.correlation); set (res, "clipped", p->hub.clipped); set (res, "playing", p->isPlaying()); 
             return ToolResult::ok (res);
         });

    add ("mastering_publish", "Export the album. kind: 'digital' (one tagged file per track; format wav|aiff|flac|ogg, bit_depth 16|24|32, sample_rate, loudness_target = a service name like 'spotify', 'apple', 'youtube', 'amazon', 'tidal', 'deezer', 'cd' or 'custom' with target_lufs / max_true_peak, album_mode), 'image' (one WAV + CUE), 'ddp' (DDP 2.00 fileset for the pressing plant). folder optional.",
         R"json({"type":"object","properties":{"kind":{"type":"string","enum":["digital","image","ddp"]},"format":{"type":"string"},"bit_depth":{"type":"integer"},"sample_rate":{"type":"number"},"loudness_target":{"type":"string"},"target_lufs":{"type":"number"},"max_true_peak":{"type":"number"},"album_mode":{"type":"boolean"},"folder":{"type":"string"}},"required":["kind"]})json",
         [needOpen] (const juce::var& a)
         {
             juce::String err; auto p = needOpen (err); if (! p) return ToolResult::error (err);
             if (p->numTracks() == 0) return ToolResult::error ("The album has no tracks yet");
             auto kind = str (a, "kind");
             juce::String report;
             auto folder = has (a, "folder") ? juce::File (str (a, "folder")) : juce::File();
             if (kind == "image") { auto f = folder != juce::File() ? folder : p->defaultReleaseFolder().getChildFile ("Image"); if (! p->publishImage (f, report)) return ToolResult::error (report); auto res = obj(); set (res, "folder", f.getFullPathName()); set (res, "report", report); return ToolResult::ok (res); }
             if (kind == "ddp") { auto f = folder != juce::File() ? folder : p->defaultReleaseFolder().getChildFile (p->projectName() + " DDP"); if (! p->publishDDP (f, report)) return ToolResult::error (report); auto res = obj(); set (res, "folder", f.getFullPathName()); set (res, "report", report); return ToolResult::ok (res); }
             MasteringProject::PublishOptions o;
             o.format = str (a, "format", "wav").toLowerCase(); o.bitDepth = (int) num (a, "bit_depth", 24); o.sampleRate = num (a, "sample_rate", 44100.0); o.albumMode = flag (a, "album_mode"); o.folder = folder;
             auto lt = str (a, "loudness_target").toLowerCase();
             auto& targets = loudnessTargets();
             if (lt.isNotEmpty() && lt != "off")
             {
                 for (size_t i = 1; i < targets.size(); ++i) if (juce::String (targets[i].name).toLowerCase().startsWith (lt.substring (0, 4))) { o.loudnessTarget = (int) i; break; }
                 if (lt == "custom" || has (a, "target_lufs")) { o.loudnessTarget = (int) targets.size() - 1; o.customLufs = (float) num (a, "target_lufs", -14.0); o.customTruePeak = (float) num (a, "max_true_peak", -1.0); }
                 if (o.loudnessTarget == 0) return ToolResult::error ("Unknown loudness target '" + lt + "'. Use spotify, apple, youtube, amazon, tidal, deezer, soundcloud, podcast, broadcast, cd or custom.");
             }
             auto files = p->publishDigital (o, report);
             if (files.isEmpty()) return ToolResult::error ("Nothing exported: " + report);
             auto res = obj(); juce::StringArray names; for (auto& f : files) names.add (f.getFullPathName());
             set (res, "files", names.joinIntoString ("\n")); set (res, "report", report);
             return ToolResult::ok (res);
         });
}
