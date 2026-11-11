#pragma once
// Squelchmoth - the Mastering window (Window > Mastering, Cmd+Shift+M).
//
// A remake of the Studio One / Fender Studio Pro Mastering (Project) page inside Squelchmoth:
//   * Track column: album tracks in order with title, artist, ISRC, pause, length, loudness readings, enable/disable
//   * Track lane: the album on one timeline with pauses, track markers, volume curves (fades) and a playhead
//   * Track Device Rack (per-track inserts + track fader) and Master Device Rack (pre-fader inserts + master fader)
//   * Metering: spectrum (octave / 1/3 octave / FFT), level + EBU R128 loudness (M / S / I / LRA / true peak),
//     phase correlation + goniometer, K-system scales, clip indicator
//   * Album and track meta-information (CD-Text, ISRC auto-increment, UPC/EAN, genre, year, artwork)
//   * Sessions: add the current Squelchmoth session (bounced automatically) and update it later
//   * Loudness detection per track (pre and post FX), Target Loudness presets for streaming services
//   * Publishing: Digital Release (WAV / AIFF / FLAC / OGG per track, tagged), Image (one WAV + CUE sheet),
//     DDP 2.00 fileset for replication plants (DDPID, DDPMS, PQDESCR, IMAGE.DAT, CDTEXT.BIN, checksums)
//
// Everything is driven by Claude too (mastering_* commands).

#include "Session.h"
#include "MonitorChain.h"
#include "Analyzer.h"
#include "PluginMenu.h"
#include "Behaviours.h"
#include "SqMp3Encoder.h"

//==============================================================================
struct LoudnessTarget { const char* name; float lufs; float truePeak; };
inline const std::vector<LoudnessTarget>& loudnessTargets()
{
    static const std::vector<LoudnessTarget> t {
        { "Off (no adjustment)", 0.0f, 0.0f }, { "Spotify  -14 LUFS / -1 dBTP", -14.0f, -1.0f }, { "Apple Music  -16 LUFS / -1 dBTP", -16.0f, -1.0f },
        { "YouTube  -14 LUFS / -1 dBTP", -14.0f, -1.0f }, { "Amazon Music  -14 LUFS / -2 dBTP", -14.0f, -2.0f }, { "Tidal  -14 LUFS / -1 dBTP", -14.0f, -1.0f },
        { "Deezer  -15 LUFS / -1 dBTP", -15.0f, -1.0f }, { "SoundCloud  -14 LUFS / -1 dBTP", -14.0f, -1.0f }, { "Podcast (Apple)  -16 LUFS / -1 dBTP", -16.0f, -1.0f },
        { "Broadcast EBU R128  -23 LUFS / -1 dBTP", -23.0f, -1.0f }, { "CD / club loud  -9 LUFS / -0.3 dBTP", -9.0f, -0.3f }, { "Custom", -14.0f, -1.0f } };
    return t;
}

//==============================================================================
class MasteringProject : public juce::ChangeBroadcaster
{
public:
    explicit MasteringProject (Session& s) : session (s)
    {
        root = session.sessionsDir.getChildFile ("Mastering");
        root.createDirectory();
    }

    ~MasteringProject() override   { closeProject(); }

    Session& session;
    juce::File root, file;
    std::unique_ptr<te::Edit> edit;
    AnalysisHub hub;
    SqAnalyzerPlugin* tap = nullptr;

    //==========================================================================
    void newProject (const juce::String& name)
    {
        auto f = root.getNonexistentChildFile (juce::File::createLegalFileName (name.isNotEmpty() ? name : "Album"), ".sqmaster", false);
        auto e = te::createEmptyEdit (session.getEngine(), f);
        for (auto t : te::getAudioTracks (*e)) e->deleteTrack (t);
        attach (std::move (e), f);
        edit->state.setProperty ("mAlbum", name.isNotEmpty() ? name : "Album", nullptr);
        if (auto mv = edit->getMasterVolumePlugin()) mv->setVolumeDb (0.0f);
        save();
        sendChangeMessage();
    }

    bool openProject (const juce::File& f)
    {
        if (! f.existsAsFile()) return false;
        auto e = te::loadEditFromFile (session.getEngine(), f);
        if (e == nullptr) return false;
        attach (std::move (e), f);
        sequence();
        sendChangeMessage();
        return true;
    }

    void closeProject()
    {
        if (edit == nullptr) return;
        edit->getTransport().stop (false, false);
        edit->getTransport().freePlaybackContext();
        if (tap != nullptr) tap->hub = nullptr;
        tap = nullptr;
        edit.reset();
    }

    bool save()
    {
        if (edit == nullptr || ! session.savingAllowed()) return false;
        return te::EditFileOperations (*edit).writeToFile (file, false);
    }

    juce::File projectDir() const           { return file.getParentDirectory().getChildFile (file.getFileNameWithoutExtension() + " Files"); }
    juce::String projectName() const        { return file.getFileNameWithoutExtension(); }
    bool isOpen() const                     { return edit != nullptr; }

    //==========================================================================
    // album meta
    juce::String album (const juce::Identifier& key, const juce::String& def = {}) const   { return edit != nullptr ? edit->state.getProperty (key, def).toString() : def; }
    void setAlbum (const juce::Identifier& key, const juce::var& v)                         { if (edit) { edit->state.setProperty (key, v, &edit->getUndoManager()); sendChangeMessage(); save(); } }

    // tracks
    juce::Array<te::AudioTrack*> tracks() const   { return edit != nullptr ? te::getAudioTracks (*edit) : juce::Array<te::AudioTrack*>(); }
    int numTracks() const                         { return tracks().size(); }
    te::AudioTrack* track (int i) const           { auto t = tracks(); return i >= 0 && i < t.size() ? t[i] : nullptr; }
    static te::WaveAudioClip* clipOf (te::AudioTrack& t)
    {
        for (auto c : t.getClips()) if (auto wc = dynamic_cast<te::WaveAudioClip*> (c)) return wc;
        return nullptr;
    }
    static juce::String meta (te::AudioTrack& t, const juce::Identifier& key, const juce::String& def = {})   { return t.state.getProperty (key, def).toString(); }
    static double pauseOf (te::AudioTrack& t)        { return (double) t.state.getProperty ("mPause", 2.0); }
    static bool enabledOf (te::AudioTrack& t)        { return (bool) t.state.getProperty ("mEnabled", true); }
    void setMeta (te::AudioTrack& t, const juce::Identifier& key, const juce::var& v)   { t.state.setProperty (key, v, &edit->getUndoManager()); if (key == juce::Identifier ("mPause") || key == juce::Identifier ("mEnabled")) sequence(); sendChangeMessage(); save(); }

    double lengthOf (te::AudioTrack& t) const        { auto c = clipOf (t); return c != nullptr ? c->getPosition().getLength().inSeconds() : 0.0; }
    double startOf (te::AudioTrack& t) const         { auto c = clipOf (t); return c != nullptr ? c->getPosition().getStart().inSeconds() : 0.0; }
    double endOf (te::AudioTrack& t) const           { return startOf (t) + lengthOf (t); }
    double totalLength() const
    {
        double e = 0.0;
        for (auto t : tracks()) if (enabledOf (*t)) e = juce::jmax (e, endOf (*t));
        return e;
    }

    /** Lays the tracks out one after another with their pauses (Red Book: first pause >= 2 s). */
    void sequence()
    {
        if (edit == nullptr) return;
        double pos = 0.0;
        bool first = true;
        for (auto t : tracks())
        {
            auto c = clipOf (*t);
            if (c == nullptr) continue;
            if (! enabledOf (*t)) { t->setMute (true); continue; }
            t->setMute (false);
            auto pause = juce::jlimit (first ? 2.0 : 0.0, 10.0, pauseOf (*t));
            pos += pause;
            c->setStart (te::TimePosition::fromSeconds (pos), false, true);
            pos += c->getPosition().getLength().inSeconds();
            first = false;
        }
        ensureTap();
    }

    te::AudioTrack* addAudioFile (const juce::File& f, const juce::String& title = {})
    {
        if (edit == nullptr || ! f.existsAsFile()) return nullptr;
        te::AudioFile af (session.getEngine(), f);
        if (af.getLength() <= 0.0) return nullptr;
        auto t = edit->insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr, true);
        if (t == nullptr) return nullptr;
        t->setName (title.isNotEmpty() ? title : f.getFileNameWithoutExtension());
        t->setColour (Theme::nextTrackColour (numTracks() - 1));
        t->insertWaveClip (t->getName(), f, { { te::TimePosition(), te::TimeDuration::fromSeconds (af.getLength()) }, {} }, false);
        t->state.setProperty ("mSource", f.getFullPathName(), nullptr);
        t->state.setProperty ("mPause", numTracks() == 1 ? 2.0 : 0.0, nullptr);
        t->state.setProperty ("mEnabled", true, nullptr);
        t->state.setProperty ("mArtist", album ("mAlbumArtist"), nullptr);
        if (auto vp = t->getVolumePlugin()) vp->setVolumeDb (0.0f);
        sequence();
        sendChangeMessage();
        save();
        session.log.add ("Mastering", "Added " + t->getName());
        return t.get();
    }

    /** Bounces the current Squelchmoth session to the project folder and adds it as a track (linked for updates). */
    te::AudioTrack* addCurrentSession()
    {
        if (edit == nullptr || session.edit == nullptr) return nullptr;
        auto f = bounceSession (session.getFile().getFileNameWithoutExtension());
        if (f == juce::File()) return nullptr;
        auto t = addAudioFile (f, session.getFile().getFileNameWithoutExtension());
        if (t != nullptr) t->state.setProperty ("mSourceSession", session.getFile().getFullPathName(), nullptr);
        return t;
    }

    /** Re-bounces a track that came from a session (Update mastering file). */
    bool updateFromSession (te::AudioTrack& t)
    {
        auto sess = juce::File (meta (t, "mSourceSession"));
        if (! sess.existsAsFile() || session.edit == nullptr) return false;
        if (session.getFile() != sess) return false;   // the session must be the one open in the Edit window
        auto f = bounceSession (sess.getFileNameWithoutExtension());
        if (f == juce::File()) return false;
        if (auto c = clipOf (t))
        {
            te::AudioFile af (session.getEngine(), f);
            c->getSourceFileReference().setToFile (f, te::SourceFileReference::PathStyle::alwaysAbsolute, false);
            c->setLength (te::TimeDuration::fromSeconds (af.getLength()), true);
            t.state.setProperty ("mSource", f.getFullPathName(), nullptr);
            t.state.removeProperty ("mLufsPre", nullptr); t.state.removeProperty ("mLufsPost", nullptr);
        }
        sequence(); sendChangeMessage();
        return true;
    }

    juce::File bounceSession (const juce::String& baseName)
    {
        auto& e = *session.edit;
        projectDir().getChildFile ("Audio").createDirectory();
        auto dest = projectDir().getChildFile ("Audio").getChildFile (juce::File::createLegalFileName (baseName) + ".wav");
        if (dest.existsAsFile()) dest.deleteFile();
        auto& afm = session.getEngine().getAudioFileFormatManager();
        te::Renderer::Parameters p (e);
        p.destFile = dest;
        p.audioFormat = afm.getWavFormat();
        p.bitDepth = 32;
        p.sampleRateForAudio = session.getEngine().getDeviceManager().getSampleRate();
        p.time = { te::TimePosition(), e.getLength() + te::TimeDuration::fromSeconds (0.5) };
        p.useMasterPlugins = true; p.usePlugins = true; p.canRenderInMono = false;
        p.tracksToDo = te::toBitSet (te::getAllTracks (e));
        MonitorChain::PrintGuard guard (session);
        auto f = te::Renderer::renderToFile ("Mastering bounce", p);
        return f.existsAsFile() ? f : juce::File();
    }

    void removeTrack (te::AudioTrack& t)      { if (edit) { edit->deleteTrack (&t); sequence(); sendChangeMessage(); save(); } }
    void moveTrack (te::AudioTrack& t, int delta)
    {
        auto ts = tracks();
        auto i = ts.indexOf (&t);
        auto j = i + delta;
        if (i < 0 || j < 0 || j >= ts.size()) return;
        auto parent = t.state.getParent();
        parent.moveChild (parent.indexOf (t.state), parent.indexOf (ts[j]->state), &edit->getUndoManager());
        sequence(); sendChangeMessage(); save();
    }

    /** Sets a track's fade in / out (volume curve) in seconds. */
    void setFades (te::AudioTrack& t, double in, double out)
    {
        if (auto c = clipOf (t)) { c->setFadeIn (te::TimeDuration::fromSeconds (juce::jmax (0.0, in))); c->setFadeOut (te::TimeDuration::fromSeconds (juce::jmax (0.0, out))); sendChangeMessage(); }
    }

    /** Auto-increments ISRCs from the first track's code (country + registrant + year + 5-digit designation). */
    int autoIncrementIsrc (const juce::String& firstIsrc)
    {
        auto base = firstIsrc.trim().toUpperCase().removeCharacters ("-");
        if (base.length() != 12) return 0;
        auto prefix = base.substring (0, 7);
        auto num = base.substring (7).getIntValue();
        int n = 0;
        for (auto t : tracks())
        {
            auto code = prefix + juce::String (num + n).paddedLeft ('0', 5);
            t->state.setProperty ("mIsrc", code, nullptr);
            ++n;
        }
        sendChangeMessage();
        return n;
    }

    //==========================================================================
    // Loudness detection (pre-FX reads the file; post-FX renders the track through its inserts + master)
    FileLoudness detectLoudness (te::AudioTrack& t, bool postFx)
    {
        FileLoudness r;
        if (! postFx)
        {
            r = measureFile (juce::File (meta (t, "mSource")));
            if (r.ok) { t.state.setProperty ("mLufsPre", r.integrated, nullptr); t.state.setProperty ("mLraPre", r.range, nullptr); t.state.setProperty ("mTpPre", r.truePeak, nullptr); t.state.setProperty ("mRmsPre", r.rms, nullptr); }
        }
        else
        {
            auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sqm_loud_" + juce::String (juce::Random::getSystemRandom().nextInt (99999)) + ".wav");
            if (renderRange (temp, startOf (t), endOf (t), 32, 48000.0, &t))
            {
                r = measureFile (temp);
                temp.deleteFile();
                if (r.ok) { t.state.setProperty ("mLufsPost", r.integrated, nullptr); t.state.setProperty ("mLraPost", r.range, nullptr); t.state.setProperty ("mTpPost", r.truePeak, nullptr); t.state.setProperty ("mRmsPost", r.rms, nullptr); }
            }
        }
        sendChangeMessage();
        return r;
    }

    /** Renders a time range of the album (all enabled tracks through their racks and the master rack). */
    bool renderRange (const juce::File& dest, double start, double end, int bitDepth, double sampleRate, te::AudioTrack* only = nullptr, const juce::String& format = "wav")
    {
        if (edit == nullptr) return false;
        if (dest.existsAsFile()) dest.deleteFile();
        auto& afm = session.getEngine().getAudioFileFormatManager();
        te::Renderer::Parameters p (*edit);
        p.destFile = dest;
        p.audioFormat = format == "aiff" ? afm.getAiffFormat() : format == "flac" ? afm.getFlacFormat() : format == "ogg" ? afm.getOggFormat() : afm.getWavFormat();
        p.bitDepth = format == "flac" ? juce::jmin (24, bitDepth) : bitDepth;
        p.sampleRateForAudio = sampleRate;
        p.time = { te::TimePosition::fromSeconds (start), te::TimePosition::fromSeconds (end) };
        p.useMasterPlugins = true; p.usePlugins = true; p.canRenderInMono = false;
        juce::Array<te::Track*> ts;
        for (auto t : tracks()) if (enabledOf (*t) && (only == nullptr || t == only)) ts.add (t);
        p.tracksToDo = te::toBitSet (ts);
        auto f = te::Renderer::renderToFile ("Mastering render", p);
        return f.existsAsFile();
    }

    //==========================================================================
    struct PublishOptions
    {
        juce::String format = "wav";      // wav | aiff | flac | ogg | mp3
        int bitDepth = 24;
        int mp3Bitrate = 320, mp3VbrQuality = -1;   // 1.12 MP3: CBR kb/s, or VBR V0-V4 when >= 0
        double sampleRate = 44100.0;
        int loudnessTarget = 0;           // index into loudnessTargets(); 0 = off
        float customLufs = -14.0f, customTruePeak = -1.0f;
        bool albumMode = false;           // measure the album as a whole, apply one gain
        bool trackNumbers = true, artistInNames = false;
        juce::File folder;                // output folder (defaults to <Mastering>/<Artist - Album>)
    };

    juce::File defaultReleaseFolder() const
    {
        auto artist = album ("mAlbumArtist"), disc = album ("mAlbum", projectName());
        auto name = artist.isNotEmpty() ? artist + " - " + disc : disc;
        return root.getChildFile (juce::File::createLegalFileName (name));
    }

    juce::StringPairArray tagsFor (te::AudioTrack& t, int number)
    {
        juce::StringPairArray m;
        m.set (juce::WavAudioFormat::riffInfoTitle, t.getName());
        m.set (juce::WavAudioFormat::riffInfoArtist, meta (t, "mArtist", album ("mAlbumArtist")));
        m.set (juce::WavAudioFormat::riffInfoProductName, album ("mAlbum"));
        m.set (juce::WavAudioFormat::riffInfoTrackNumber, juce::String (number));
        m.set (juce::WavAudioFormat::riffInfoGenre, album ("mGenre"));
        m.set (juce::WavAudioFormat::riffInfoDateCreated, album ("mYear"));
        m.set (juce::WavAudioFormat::riffInfoComment, album ("mComment"));
        m.set (juce::WavAudioFormat::riffInfoCopyright, album ("mCopyright"));
        m.set (juce::WavAudioFormat::riffInfoMusicBy, meta (t, "mComposer"));
        m.set (juce::WavAudioFormat::riffInfoWrittenBy, meta (t, "mSongwriter"));
        m.set (juce::WavAudioFormat::ISRC, meta (t, "mIsrc"));
        m.set (juce::WavAudioFormat::bwavDescription, t.getName() + " - " + album ("mAlbum"));
        m.set (juce::WavAudioFormat::bwavOriginator, "Squelchmoth");
        m.set (juce::WavAudioFormat::bwavOriginationDate, juce::Time::getCurrentTime().formatted ("%Y-%m-%d"));
        m.set (juce::WavAudioFormat::bwavOriginationTime, juce::Time::getCurrentTime().formatted ("%H:%M:%S"));
        return m;
    }

    /** Digital Release: one tagged file per enabled track. Returns the files written. */
    juce::Array<juce::File> publishDigital (const PublishOptions& o, juce::String& report)
    {
        juce::Array<juce::File> out;
        if (edit == nullptr) return out;
        auto folder = o.folder != juce::File() ? o.folder : defaultReleaseFolder();
        folder.createDirectory();
        auto& targets = loudnessTargets();
        float targetLufs = o.loudnessTarget > 0 ? (o.loudnessTarget == (int) targets.size() - 1 ? o.customLufs : targets[(size_t) o.loudnessTarget].lufs) : 0.0f;
        float maxTp = o.loudnessTarget > 0 ? (o.loudnessTarget == (int) targets.size() - 1 ? o.customTruePeak : targets[(size_t) o.loudnessTarget].truePeak) : 0.0f;
        struct Rendered { te::AudioTrack* t; juce::File f; FileLoudness l; int number; };
        std::vector<Rendered> rendered;
        int n = 0;
        for (auto t : tracks())
        {
            if (! enabledOf (*t)) continue;
            ++n;
            juce::String base;
            if (o.trackNumbers) base << juce::String (n).paddedLeft ('0', 2) << " ";
            if (o.artistInNames) base << meta (*t, "mArtist", album ("mAlbumArtist")) << " - ";
            base << t->getName();
            const bool mp3 = o.format == "mp3";   // MP3: render a float WAV, level it, then encode with the tags
            auto f = folder.getChildFile (juce::File::createLegalFileName (base) + (mp3 ? " (mp3 source).wav" : "." + o.format));
            // render with a small tail so reverbs finish, then the loudness pass
            if (! renderRange (f, startOf (*t), endOf (*t) + (endOf (*t) < totalLength() ? 0.0 : 0.5), mp3 ? 32 : o.bitDepth, o.sampleRate, t, mp3 ? "wav" : o.format)) { report << "Failed: " << f.getFileName() << "\n"; continue; }
            rendered.push_back ({ t, f, measureFile (f), n });
        }
        // loudness adjustment
        if (o.loudnessTarget > 0 && ! rendered.empty())
        {
            float albumGain = 0.0f;
            if (o.albumMode)
            {
                double sum = 0.0; int c = 0; float tp = -100.0f;
                for (auto& r : rendered) if (r.l.ok) { sum += std::pow (10.0, r.l.integrated / 10.0); ++c; tp = juce::jmax (tp, r.l.truePeak); }
                auto albumLufs = c > 0 ? (float) (10.0 * std::log10 (sum / c)) : -100.0f;
                albumGain = targetLufs - albumLufs;
                if (tp + albumGain > maxTp) albumGain = maxTp - tp;
            }
            for (auto& r : rendered)
            {
                if (! r.l.ok) continue;
                float gain = o.albumMode ? albumGain : targetLufs - r.l.integrated;
                if (! o.albumMode && r.l.truePeak + gain > maxTp) gain = maxTp - r.l.truePeak;   // max true peak always wins
                if (std::abs (gain) > 0.05f) applyGainToFile (r.f, gain, o.format == "wav" ? tagsFor (*r.t, r.number) : juce::StringPairArray());
                report << r.f.getFileName() << ":  " << juce::String (r.l.integrated, 1) << " LUFS -> " << juce::String (r.l.integrated + gain, 1) << " LUFS  (" << (gain >= 0 ? "+" : "") << juce::String (gain, 1) << " dB)"
                       << (r.l.truePeak + gain > maxTp - 0.05f ? "  [true-peak limited]" : "") << "\n";
            }
        }
        else
        {
            for (auto& r : rendered)
            {
                if (o.format == "wav") applyGainToFile (r.f, 0.0f, tagsFor (*r.t, r.number));   // rewrite only to embed the tags
                report << r.f.getFileName() << ":  " << juce::String (r.l.integrated, 1) << " LUFS, TP " << juce::String (r.l.truePeak, 1) << " dBTP\n";
            }
        }
        if (o.format == "mp3")
            for (auto& r : rendered)
            {
                SqMp3::Tags tg;
                tg.title = r.t->getName(); tg.artist = meta (*r.t, "mArtist", album ("mAlbumArtist")); tg.album = album ("mAlbum");
                tg.track = juce::String (r.number) + "/" + juce::String ((int) rendered.size()); tg.year = album ("mYear"); tg.genre = album ("mGenre");
                tg.isrc = meta (*r.t, "mIsrc"); tg.comment = album ("mComment");
                auto dst = r.f.getSiblingFile (r.f.getFileName().upToLastOccurrenceOf (" (mp3 source)", false, false) + ".mp3");
                juce::String err;
                if (SqMp3::encodeMp3 (r.f, dst, o.mp3Bitrate, o.mp3VbrQuality, tg, err)) { r.f.deleteFile(); r.f = dst; }
                else report << "MP3 failed: " << dst.getFileName() << " " << err << "\n";
            }
        for (auto& r : rendered) out.add (r.f);
        writeAlbumSheet (folder, rendered.size());
        session.log.add ("Mastering", "Digital release: " + juce::String (out.size()) + " files in " + folder.getFullPathName());
        return out;
    }

    void writeAlbumSheet (const juce::File& folder, size_t n)
    {
        juce::String s;
        s << album ("mAlbumArtist") << " - " << album ("mAlbum") << "\n";
        if (album ("mYear").isNotEmpty()) s << "Year: " << album ("mYear") << "   ";
        if (album ("mGenre").isNotEmpty()) s << "Genre: " << album ("mGenre") << "   ";
        if (album ("mUpc").isNotEmpty()) s << "UPC/EAN: " << album ("mUpc");
        s << "\n\n";
        int i = 0;
        for (auto t : tracks())
            if (enabledOf (*t))
                s << juce::String (++i).paddedLeft ('0', 2) << "  " << t->getName() << "   " << timeText (lengthOf (*t)) << (meta (*t, "mIsrc").isNotEmpty() ? "   ISRC " + meta (*t, "mIsrc") : juce::String()) << "\n";
        s << "\nMastered in Squelchmoth (created by Jamar Stewart). " << juce::String ((int) n) << " tracks.\n";
        folder.getChildFile ("Album.txt").replaceWithText (s);
    }

    static juce::String timeText (double secs)   { int m = (int) secs / 60; double s = secs - m * 60; return juce::String (m) + ":" + juce::String (s, 2).paddedLeft ('0', 5); }
    static juce::String msf (double secs)        { int frames = (int) std::llround (secs * 75.0); int m = frames / (75 * 60); int s = (frames / 75) % 60; int f = frames % 75; return juce::String (m).paddedLeft ('0', 2) + ":" + juce::String (s).paddedLeft ('0', 2) + ":" + juce::String (f).paddedLeft ('0', 2); }

    /** Image: one continuous 16-bit 44.1 kHz WAV of the whole album plus a CUE sheet. */
    bool publishImage (const juce::File& folder, juce::String& report)
    {
        if (edit == nullptr) return false;
        folder.createDirectory();
        auto wav = folder.getChildFile (juce::File::createLegalFileName (album ("mAlbum", projectName())) + ".wav");
        if (! renderRange (wav, 0.0, totalLength(), 16, 44100.0)) { report << "Render failed\n"; return false; }
        auto cue = wav.withFileExtension ("cue");
        cue.replaceWithText (cueSheet (wav.getFileName(), "WAVE"));
        report << wav.getFileName() << " + " << cue.getFileName() << "\n";
        session.log.add ("Mastering", "Image written: " + wav.getFullPathName());
        return true;
    }

    juce::String cueSheet (const juce::String& fileName, const juce::String& type)
    {
        juce::String s;
        if (album ("mUpc").isNotEmpty()) s << "CATALOG " << album ("mUpc") << "\n";
        if (album ("mAlbum").isNotEmpty()) s << "TITLE \"" << album ("mAlbum") << "\"\n";
        if (album ("mAlbumArtist").isNotEmpty()) s << "PERFORMER \"" << album ("mAlbumArtist") << "\"\n";
        s << "FILE \"" << fileName << "\" " << type << "\n";
        int n = 0;
        for (auto t : tracks())
        {
            if (! enabledOf (*t)) continue;
            ++n;
            s << "  TRACK " << juce::String (n).paddedLeft ('0', 2) << " AUDIO\n";
            s << "    TITLE \"" << t->getName() << "\"\n";
            auto artist = meta (*t, "mArtist", album ("mAlbumArtist")); if (artist.isNotEmpty()) s << "    PERFORMER \"" << artist << "\"\n";
            if (meta (*t, "mSongwriter").isNotEmpty()) s << "    SONGWRITER \"" << meta (*t, "mSongwriter") << "\"\n";
            if (meta (*t, "mIsrc").isNotEmpty()) s << "    ISRC " << meta (*t, "mIsrc").removeCharacters ("-") << "\n";
            juce::StringArray flags; if ((bool) t->state.getProperty ("mPre", false)) flags.add ("PRE"); if ((bool) t->state.getProperty ("mDcp", false)) flags.add ("DCP"); if ((bool) t->state.getProperty ("mScms", false)) flags.add ("SCMS");
            if (! flags.isEmpty()) s << "    FLAGS " << flags.joinIntoString (" ") << "\n";
            auto pause = n == 1 ? startOf (*t) : pauseOf (*t);
            if (n == 1) s << "    INDEX 00 00:00:00\n";
            else if (pause > 0.0) s << "    INDEX 00 " << msf (startOf (*t) - pause) << "\n";
            s << "    INDEX 01 " << msf (startOf (*t)) << "\n";
        }
        return s;
    }

    //==========================================================================
    // DDP 2.00 (layout as documented by the ddp-reverse-eng project, matching HOFA / Sonoris output)
    static juce::String fixed (const juce::String& s, int width, bool rightAlignNumber = false, bool zeroPad = false)
    {
        auto t = s.substring (0, width);
        if (t.isEmpty()) return juce::String::repeatedString (" ", width);   // an unset field is all spaces
        if (rightAlignNumber) return t.paddedLeft (zeroPad ? '0' : ' ', width);
        return t.paddedRight (' ', width);
    }

    static juce::uint32 crc32 (const juce::MemoryBlock& data)
    {
        static juce::uint32 table[256];
        static bool init = false;
        if (! init) { for (juce::uint32 i = 0; i < 256; ++i) { juce::uint32 c = i; for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
        juce::uint32 c = 0xFFFFFFFFu;
        auto* p = (const juce::uint8*) data.getData();
        for (size_t i = 0; i < data.getSize(); ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    }

    static juce::uint16 cdTextCrc (const juce::uint8* d, int n)
    {
        juce::uint16 crc = 0;
        for (int i = 0; i < n; ++i) { crc ^= (juce::uint16) (d[i] << 8); for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? (juce::uint16) ((crc << 1) ^ 0x1021) : (juce::uint16) (crc << 1); }
        return (juce::uint16) ~crc;
    }

    /** Builds CDTEXT.BIN packs (title, performer, songwriter, composer + size information). */
    juce::MemoryBlock buildCdText (const juce::Array<te::AudioTrack*>& ts)
    {
        juce::MemoryBlock out;
        struct Pack { juce::uint8 type, track, seq, pos; juce::uint8 payload[12]; };
        std::vector<Pack> packs;
        int seq = 0;
        int counts[16] = { 0 };
        auto latin1 = [] (const juce::String& s) { juce::MemoryBlock m; auto w = s.toWideCharPointer(); for (int i = 0; i < s.length(); ++i) { auto c = (juce::uint32) w[i]; juce::uint8 b = c < 256 ? (juce::uint8) c : (juce::uint8) '?'; m.append (&b, 1); } return m; };
        auto addType = [&] (juce::uint8 type, const juce::String& discText, std::function<juce::String (te::AudioTrack&)> trackText)
        {
            bool any = discText.isNotEmpty(); for (auto t : ts) any = any || trackText (*t).isNotEmpty();
            if (! any) return;
            // build the byte stream: disc string, NUL, each track string, NUL - remembering which track each byte belongs to
            std::vector<std::pair<juce::uint8, juce::uint8>> bytes;   // (byte, trackNumber)
            std::vector<int> charPos;                                 // position of each byte within its string
            auto push = [&] (const juce::String& s, juce::uint8 trk) { auto m = latin1 (s); for (size_t i = 0; i < m.getSize(); ++i) { bytes.push_back ({ ((juce::uint8*) m.getData())[i], trk }); charPos.push_back ((int) i); } bytes.push_back ({ 0, trk }); charPos.push_back ((int) m.getSize()); };
            push (discText, 0);
            int n = 0; for (auto t : ts) push (trackText (*t), (juce::uint8) ++n);
            for (size_t i = 0; i < bytes.size(); i += 12)
            {
                Pack p {}; p.type = type; p.track = bytes[i].second; p.seq = (juce::uint8) seq++; p.pos = (juce::uint8) juce::jmin (15, charPos[i]);
                for (int k = 0; k < 12; ++k) p.payload[k] = i + (size_t) k < bytes.size() ? bytes[i + (size_t) k].first : 0;
                packs.push_back (p); counts[type - 0x80]++;
            }
        };
        addType (0x80, album ("mAlbum"), [] (te::AudioTrack& t) { return t.getName(); });
        addType (0x81, album ("mAlbumArtist"), [this] (te::AudioTrack& t) { return meta (t, "mArtist", album ("mAlbumArtist")); });
        addType (0x82, {}, [] (te::AudioTrack& t) { return meta (t, "mSongwriter"); });
        addType (0x83, {}, [] (te::AudioTrack& t) { return meta (t, "mComposer"); });
        if (packs.empty()) return out;
        // size information: 3 packs
        juce::uint8 info[36] = { 0 };
        info[0] = 0; info[1] = 1; info[2] = (juce::uint8) ts.size(); info[3] = 0;
        counts[15] = 3;
        for (int i = 0; i < 16; ++i) info[4 + i] = (juce::uint8) counts[i];
        info[20] = (juce::uint8) (seq + 2); info[28] = 0x09;
        for (int i = 0; i < 3; ++i) { Pack p {}; p.type = 0x8F; p.track = (juce::uint8) i; p.seq = (juce::uint8) seq++; p.pos = 0; std::memcpy (p.payload, info + i * 12, 12); packs.push_back (p); }
        for (auto& p : packs)
        {
            juce::uint8 b[18]; b[0] = p.type; b[1] = p.track; b[2] = p.seq; b[3] = p.pos; std::memcpy (b + 4, p.payload, 12);
            auto crc = cdTextCrc (b, 16); b[16] = (juce::uint8) (crc >> 8); b[17] = (juce::uint8) (crc & 0xFF);
            out.append (b, 18);
        }
        return out;
    }

    bool publishDDP (const juce::File& folder, juce::String& report)
    {
        if (edit == nullptr) return false;
        folder.createDirectory();
        juce::Array<te::AudioTrack*> ts; for (auto t : tracks()) if (enabledOf (*t)) ts.add (t);
        if (ts.isEmpty()) { report << "No tracks\n"; return false; }
        // Red Book checks
        for (int i = 0; i < ts.size(); ++i)
        {
            if (lengthOf (*ts[i]) < 4.0) { report << "Track " << (i + 1) << " is shorter than 4 seconds (Red Book minimum)\n"; return false; }
            if (meta (*ts[i], "mIsrc").isNotEmpty() && meta (*ts[i], "mIsrc").removeCharacters ("-").length() != 12) { report << "Track " << (i + 1) << " ISRC must be 12 characters\n"; return false; }
        }
        if (startOf (*ts[0]) < 2.0) { report << "The first pause must be at least 2 seconds\n"; return false; }
        if (totalLength() > 99 * 60 + 59) { report << "Program longer than 99:59 - too long for a CD\n"; return false; }
        auto upc = album ("mUpc").removeCharacters ("- ");
        if (upc.isNotEmpty() && upc.length() == 12) upc = "0" + upc;
        if (upc.isNotEmpty() && upc.length() != 13) { report << "UPC/EAN must be 12 or 13 digits\n"; return false; }

        // 1. render the program to a temp WAV and convert to raw 16-bit 44.1 kHz sectors
        auto temp = folder.getChildFile ("_render.wav");
        if (! renderRange (temp, 0.0, totalLength(), 16, 44100.0)) { report << "Render failed\n"; return false; }
        juce::AudioFormatManager afm; afm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (afm.createReaderFor (temp));
        if (reader == nullptr) { report << "Could not read the render\n"; return false; }
        auto image = folder.getChildFile ("IMAGE.DAT");
        juce::int64 totalFrames = reader->lengthInSamples;
        juce::int64 sectors = (totalFrames + 587) / 588;
        {
            juce::FileOutputStream os (image);
            if (! os.openedOk()) { report << "Cannot write IMAGE.DAT\n"; return false; }
            const int block = 588 * 64;
            juce::AudioBuffer<float> buf (2, block);
            std::vector<juce::int16> raw ((size_t) block * 2);
            for (juce::int64 pos = 0; pos < sectors * 588; pos += block)
            {
                auto n = (int) juce::jmin ((juce::int64) block, sectors * 588 - pos);
                buf.clear();
                auto avail = (int) juce::jmax ((juce::int64) 0, juce::jmin ((juce::int64) n, totalFrames - pos));
                if (avail > 0) reader->read (&buf, 0, avail, pos, true, true);
                for (int i = 0; i < n; ++i)
                    for (int ch = 0; ch < 2; ++ch)
                        raw[(size_t) (i * 2 + ch)] = (juce::int16) juce::jlimit (-32768, 32767, juce::roundToInt (buf.getSample (juce::jmin (ch, buf.getNumChannels() - 1), i) * 32767.0f));
                os.write (raw.data(), (size_t) n * 4);
            }
        }
        reader.reset();
        temp.deleteFile();

        // 2. PQ descriptor
        juce::MemoryBlock pq;
        auto packet = [&] (const juce::String& trk, const juce::String& idx, double secs, const juce::String& c1, const juce::String& isrc, const juce::String& upcField)
        {
            juce::String s;
            int frames = (int) std::llround (secs * 75.0);
            auto time = juce::String (frames / (75 * 60)).paddedLeft ('0', 2) + juce::String ((frames / 75) % 60).paddedLeft ('0', 2) + juce::String (frames % 75).paddedLeft ('0', 2);
            s << "VVVS" << fixed (trk, 2) << fixed (idx, 2) << fixed (time, 8, true, false) << fixed (c1, 2) << "  " << fixed (isrc, 12) << fixed (upcField, 13) << fixed ({}, 19);
            jassert (s.length() == 64);
            pq.append (s.toRawUTF8(), 64);
        };
        packet ("00", "00", 0.0, "01", {}, upc);
        for (int i = 0; i < ts.size(); ++i)
        {
            auto& t = *ts[i];
            auto isrc = meta (t, "mIsrc").removeCharacters ("-").toUpperCase();
            int control = ((bool) t.state.getProperty ("mPre", false) ? 1 : 0) | ((bool) t.state.getProperty ("mDcp", false) ? 2 : 0);
            juce::String c1 = juce::String::toHexString (control).toUpperCase() + ((bool) t.state.getProperty ("mScms", false) ? "S" : "1");
            auto trk = juce::String (i + 1).paddedLeft ('0', 2);
            auto start = startOf (t);
            auto pause = i == 0 ? start : pauseOf (t);
            if (i == 0) packet (trk, "00", 0.0, c1, isrc, upc);
            else if (pause > 0.0) packet (trk, "00", start - pause, c1, isrc, upc);
            packet (trk, "01", start, c1, i == 0 || pause > 0.0 ? juce::String() : isrc, upc);
        }
        double leadOut = (double) sectors / 75.0;
        packet ("AA", "01", leadOut, "01", {}, upc);
        packet ("AA", "01", leadOut, "01", {}, upc);
        folder.getChildFile ("PQDESCR").replaceWithData (pq.getData(), pq.getSize());

        // 3. CD-Text
        auto cdtext = buildCdText (ts);
        if (cdtext.getSize() > 0) folder.getChildFile ("CDTEXT.BIN").replaceWithData (cdtext.getData(), cdtext.getSize());

        // 4. DDPMS
        juce::MemoryBlock ms;
        auto mapPacket = [&] (const juce::String& dst, juce::int64 dsl, const juce::String& dss, const juce::String& sub, const juce::String& cdm, const juce::String& ssm, const juce::String& scr, const juce::String& pre2, const juce::String& trk, const juce::String& dsi)
        {
            juce::String s;
            s << "VVVM" << fixed (dst, 2) << fixed ({}, 8) << fixed (juce::String (dsl), 8, true, true) << fixed (dss, 8, true, true) << fixed (sub, 8) << fixed (cdm, 2) << fixed (ssm, 1) << fixed (scr, 1)
              << fixed ({}, 4) << fixed (pre2, 4, true, false) << fixed ({}, 4) << " " << fixed (trk, 2) << fixed ({}, 2) << fixed ({}, 12) << fixed ("17", 3, true, true) << fixed (dsi, 17) << fixed ({}, 1) << fixed ({}, 4) << fixed ({}, 8) << fixed ({}, 9) << fixed ({}, 15);
            jassert (s.length() == 128);
            ms.append (s.toRawUTF8(), 128);
        };
        if (cdtext.getSize() > 0) mapPacket ("S0", (juce::int64) cdtext.getSize(), {}, "CDTEXT", {}, {}, {}, {}, "00", "CDTEXT.BIN");
        mapPacket ("S0", (juce::int64) pq.getSize(), {}, "PQ DESCR", {}, {}, {}, {}, {}, "PQDESCR");
        mapPacket ("D0", sectors, "0", {}, "DA", "7", "0", juce::String ((int) std::llround (startOf (*ts[0]) * 75.0)), {}, "IMAGE.DAT");
        folder.getChildFile ("DDPMS").replaceWithData (ms.getData(), ms.getSize());

        // 5. DDPID
        juce::String id;
        id << "DDP 2.00" << fixed (upc, 13) << fixed ({}, 8) << fixed ({}, 8) << " " << fixed (album ("mAlbumArtist") + " - " + album ("mAlbum"), 48) << " " << "CD" << fixed ({}, 4) << fixed ({}, 2) << fixed ({}, 33);
        jassert (id.length() == 128);
        folder.getChildFile ("DDPID").replaceWithText (id, false, false, "\n");

        // 6. companions: cue sheet + checksums
        folder.getChildFile ("IMAGE.CUE").replaceWithText (cueSheet ("IMAGE.DAT", "BINARY"));
        juce::String md5s, crcs = "[CRC32 Checksum]\n";
        for (auto* name : { "DDPID", "DDPMS", "CDTEXT.BIN", "PQDESCR", "IMAGE.DAT" })
        {
            auto f = folder.getChildFile (name);
            if (! f.existsAsFile()) continue;
            juce::MemoryBlock data; f.loadFileAsData (data);
            md5s << juce::MD5 (data).toHexString() << " *" << name << "\n";
            crcs << name << "=" << juce::String::toHexString ((int) crc32 (data)).toUpperCase().paddedLeft ('0', 8) << "\n";
        }
        folder.getChildFile ("CHECKSUM.MD5").replaceWithText (md5s, false, false, "\n");
        folder.getChildFile ("CHECKSUM.TXT").replaceWithText (crcs, false, false, "\n");
        report << "DDP fileset written to " << folder.getFullPathName() << "  (" << ts.size() << " tracks, " << juce::String ((int) (leadOut / 60)) << ":" << juce::String ((int) leadOut % 60).paddedLeft ('0', 2) << ", " << juce::String (sectors) << " sectors)\n";
        session.log.add ("Mastering", "DDP written: " + folder.getFullPathName());
        return true;
    }

    //==========================================================================
    // playback
    te::TransportControl* transport()   { return edit != nullptr ? &edit->getTransport() : nullptr; }
    void play (double from = -1.0)
    {
        if (edit == nullptr) return;
        if (session.edit != nullptr) session.edit->getTransport().stop (false, false);   // one transport at a time
        ensureTap();
        hub.reset();
        auto& tc = edit->getTransport();
        tc.ensureContextAllocated();
        if (from >= 0.0) tc.setPosition (te::TimePosition::fromSeconds (from));
        tc.play (false);
    }
    void stop()        { if (edit) edit->getTransport().stop (false, false); }
    bool isPlaying()   { return edit != nullptr && edit->getTransport().isPlaying(); }
    double position()  { return edit != nullptr ? edit->getTransport().getPosition().inSeconds() : 0.0; }
    void locate (double secs)   { if (edit) edit->getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, secs))); }

    /** Master inserts (everything on the master list except our analyzer tap). */
    juce::Array<te::Plugin*> masterInserts()
    {
        juce::Array<te::Plugin*> out;
        if (edit == nullptr) return out;
        for (auto p : edit->getMasterPluginList().getPlugins())
            if (dynamic_cast<SqAnalyzerPlugin*> (p) == nullptr && dynamic_cast<te::VolumeAndPanPlugin*> (p) == nullptr && dynamic_cast<te::LevelMeterPlugin*> (p) == nullptr) out.add (p);
        return out;
    }

private:
    void attach (std::unique_ptr<te::Edit> e, const juce::File& f)
    {
        closeProject();
        edit = std::move (e);
        file = f;
        projectDir().createDirectory();
        ensureTap();
    }

    void ensureTap()
    {
        if (edit == nullptr) return;
        auto& list = edit->getMasterPluginList();
        SqAnalyzerPlugin* found = nullptr;
        for (auto p : list.getPlugins()) if (auto a = dynamic_cast<SqAnalyzerPlugin*> (p)) found = a;
        if (found == nullptr)
        {
            if (auto p = edit->getPluginCache().createNewPlugin (SqAnalyzerPlugin::xmlTypeName, {}))
            {
                list.insertPlugin (p, -1, nullptr);
                found = dynamic_cast<SqAnalyzerPlugin*> (p.get());
            }
        }
        else if (list.getPlugins().getLast() != found)
        {
            auto parent = found->state.getParent();
            parent.moveChild (parent.indexOf (found->state), -1, nullptr);
        }
        tap = found;
        if (tap != nullptr) tap->hub = &hub;
    }
};

//==============================================================================
// 1.12 - Mastering track macros (Studio One's Track Editor macros): a saved recipe of a track's whole device rack,
// fader and fades, applied to any tracks in one step. Files: Documents/Squelchmoth/Mastering Macros/<name>.sqmmacro
namespace MasteringMacros
{
    inline juce::File folder()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Squelchmoth").getChildFile ("Mastering Macros");
        d.createDirectory();
        return d;
    }
    inline juce::File fileFor (const juce::String& name)   { return folder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".sqmmacro"); }
    inline juce::StringArray names()
    {
        juce::StringArray out;
        for (auto& f : folder().findChildFiles (juce::File::findFiles, false, "*.sqmmacro")) out.add (f.getFileNameWithoutExtension());
        out.sort (true);
        return out;
    }

    /** Saves the track's rack, fader and fades. Returns the number of rack plug-ins saved, or -1. */
    inline int save (MasteringProject& p, te::AudioTrack& t, const juce::String& name, juce::String& err)
    {
        if (name.trim().isEmpty()) { err = "Give the macro a name."; return -1; }
        juce::ValueTree m ("SQM_MMACRO");
        m.setProperty ("name", name.trim(), nullptr);
        m.setProperty ("version", JUCE_APPLICATION_VERSION_STRING, nullptr);
        if (auto vp = t.getVolumePlugin()) m.setProperty ("faderDb", vp->getVolumeDb(), nullptr);
        if (auto c = MasteringProject::clipOf (t)) { m.setProperty ("fadeIn", c->getFadeIn().inSeconds(), nullptr); m.setProperty ("fadeOut", c->getFadeOut().inSeconds(), nullptr); }
        for (auto pl : Session::getInserts (t.pluginList, &t))
        {
            p.edit->flushPluginStateIfNeeded (*pl);
            auto copy = pl->state.createCopy();
            copy.removeProperty (te::IDs::id, nullptr);
            m.appendChild (copy, nullptr);
        }
        auto f = fileFor (name);
        auto xml = m.createXml();
        if (xml == nullptr || ! xml->writeTo (f)) { err = "Couldn't write " + f.getFullPathName(); return -1; }
        return m.getNumChildren();
    }

    /** Applies a macro to one track: replaces its rack, sets the fader and the fades. Returns plug-ins added, or -1. */
    inline int apply (MasteringProject& p, te::AudioTrack& t, const juce::String& name, juce::String& err, bool fades = true)
    {
        auto f = fileFor (name);
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("SQM_MMACRO")) { err = "No mastering macro called '" + name + "'"; return -1; }
        auto m = juce::ValueTree::fromXml (*xml);
        for (auto pl : Session::getInserts (t.pluginList, &t)) pl->deleteFromParent();
        int added = 0, missing = 0;
        for (auto child : m)
        {
            auto v = child.createCopy();
            te::EditItemID::remapIDs (v, nullptr, *p.edit);
            auto pl = p.edit->getPluginCache().createNewPlugin (v);
            if (pl == nullptr) { ++missing; continue; }
            t.pluginList.insertPlugin (pl, Session::insertEndIndex (t.pluginList, &t), nullptr);
            ++added;
        }
        if (m.hasProperty ("faderDb")) if (auto vp = t.getVolumePlugin()) vp->setVolumeDb ((float) (double) m["faderDb"]);
        if (fades && m.hasProperty ("fadeIn")) p.setFades (t, (double) m["fadeIn"], (double) m["fadeOut"]);
        if (missing > 0) err = juce::String (missing) + " plug-in(s) in the macro aren't installed on this computer and were skipped.";
        p.sendChangeMessage();
        return added;
    }

    inline bool remove (const juce::String& name)   { return fileFor (name).deleteFile(); }
}


//==============================================================================
// UI pieces
//==============================================================================
class RackStrip : public juce::Component   // a device rack: insert slots + fader
{
public:
    RackStrip (MasteringProject& p, const juce::String& title) : project (p), name (title)
    {
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setRange (-70.0, 12.0, 0.1);
        fader.setSkewFactorFromMidPoint (-10.0);
        fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        fader.setTextValueSuffix (" dB");
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.onValueChange = [this] { if (auto vp = volumePlugin()) vp->setVolumeDb ((float) fader.getValue()); };
        addAndMakeVisible (fader);
        allBtn.setTooltip ("Activate / bypass every insert in this rack (A/B the processing)");
        allBtn.onClick = [this] { bool any = false; for (auto p : inserts()) any = any || p->isEnabled(); for (auto p : inserts()) p->setEnabled (! any); rebuild(); };
        addAndMakeVisible (allBtn);
        rebuild();
    }

    void setTrack (te::AudioTrack* t)   { track = t; rebuild(); }

    te::PluginList* list()   { if (project.edit == nullptr) return nullptr; if (track != nullptr) return &track->pluginList; return isMaster ? &project.edit->getMasterPluginList() : nullptr; }
    te::VolumeAndPanPlugin* volumePlugin()   { if (project.edit == nullptr) return nullptr; if (track != nullptr) return track->getVolumePlugin(); return isMaster ? project.edit->getMasterVolumePlugin().get() : nullptr; }
    bool isMaster = false;
    juce::Array<te::Plugin*> inserts()
    {
        if (track != nullptr) return Session::getInserts (track->pluginList, track);
        if (isMaster) return project.masterInserts();
        return {};
    }

    void rebuild()
    {
        slots.clear();
        auto ins = inserts();
        fader.setVisible (isMaster || track != nullptr);
        allBtn.setVisible (isMaster || track != nullptr);
        for (int i = 0; i < (isMaster || track != nullptr ? juce::jmax (6, ins.size()) : 0); ++i)
        {
            auto s = std::make_unique<Slot> (*this, i, i < ins.size() ? te::Plugin::Ptr (ins[i]) : nullptr);
            addAndMakeVisible (*s);
            slots.push_back (std::move (s));
        }
        if (auto vp = volumePlugin()) fader.setValue (vp->getVolumeDb(), juce::dontSendNotification);
        resized();
        repaint();
    }

    void refresh()   { if (auto vp = volumePlugin()) if (! fader.isMouseButtonDown()) fader.setValue (vp->getVolumeDb(), juce::dontSendNotification); for (auto& s : slots) s->repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::panel());
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (Theme::acid()); g.setFont (Theme::font (10.0f, true));
        g.drawText (name.toUpperCase(), 8, 6, getWidth() - 16, 14, juce::Justification::centredLeft);
        g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f, true));
        g.drawText ("INSERTS", 8, 24, 100, 12, juce::Justification::centredLeft);
        if (isMaster && project.edit != nullptr) { g.setFont (Theme::font (9.0f)); g.drawText ("Analyzer tap runs after the fader", 8, getHeight() - 14, getWidth() - 16, 12, juce::Justification::centredLeft); }
        if (! isMaster && track == nullptr) { g.setFont (Theme::font (10.0f)); g.drawText ("Select a track in the list to see its rack", 8, getHeight() - 16, getWidth() - 16, 14, juce::Justification::centredLeft); }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        r.removeFromTop (32);
        for (auto& s : slots) { s->setBounds (r.removeFromTop (18)); r.removeFromTop (2); }
        r.removeFromTop (4);
        allBtn.setBounds (r.removeFromTop (18));
        r.removeFromTop (6);
        r.removeFromBottom (14);
        fader.setBounds (r.withSizeKeepingCentre (70, r.getHeight()));
    }

    void insertAt (int slotIndex, te::Plugin::Ptr p, te::Plugin::Ptr replacing)
    {
        auto l = list(); if (l == nullptr || p == nullptr) return;
        int idx = -1;
        if (replacing != nullptr) { idx = l->indexOf (replacing.get()); replacing->deleteFromParent(); }
        else
        {
            auto ins = inserts();
            if (slotIndex < ins.size()) idx = l->indexOf (ins[slotIndex]);
            else if (track != nullptr) idx = Session::insertEndIndex (*l, track);
            else idx = project.tap != nullptr ? l->indexOf (project.tap) : -1;   // keep the analyzer last
        }
        l->insertPlugin (p, idx, nullptr);
        if (Prefs::getBool ("pl_openWindowOnInsert", true)) p->showWindowExplicitly();
        rebuild();
    }

    struct Slot : public juce::Component
    {
        Slot (RackStrip& o, int i, te::Plugin::Ptr p) : owner (o), index (i), plugin (p) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            bool on = plugin != nullptr && plugin->isEnabled();
            g.setColour (plugin == nullptr ? Theme::bg().withAlpha (0.6f) : (on ? Theme::violet().withAlpha (0.55f) : Theme::panel2()));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (plugin == nullptr ? Theme::dim().withAlpha (0.6f) : (on ? Theme::text() : Theme::dim()));
            g.setFont (Theme::font (10.0f, plugin != nullptr));
            g.drawText (plugin ? Signature::displayName (*plugin) : juce::String::charToString ((juce::juce_wchar) ('A' + index)), getLocalBounds().withTrimmedLeft (4).withTrimmedRight (12), juce::Justification::centredLeft);
            juce::Path tri; tri.addTriangle ((float) getWidth() - 9.0f, 6.5f, (float) getWidth() - 3.0f, 6.5f, (float) getWidth() - 6.0f, 10.5f);
            g.setColour (Theme::dim()); g.fillPath (tri);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (plugin != nullptr && ! e.mods.isPopupMenu() && e.x < getWidth() - 13) { plugin->showWindowExplicitly(); return; }
            if (plugin == nullptr)
            {
                auto m = PluginMenu::build (owner.project.session.getEngine(), PluginMenu::effects);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
                {
                    auto sp = juce::Component::SafePointer<juce::Component> (this);
                    PluginMenu::handle (owner.project.session.getEngine(), r, true, [this, sp] (int id) { if (sp != nullptr) owner.insertAt (index, PluginMenu::createFromResult (owner.project.session, id), nullptr); });
                });
                return;
            }
            juce::PopupMenu m;
            m.addItem (1, "No Insert (remove)"); m.addSeparator(); m.addItem (2, "Open window"); m.addItem (3, "Bypass", true, ! plugin->isEnabled()); m.addItem (4, "Replace with..."); m.addSeparator(); m.addItem (5, "Move up"); m.addItem (6, "Move down");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
            {
                auto p = plugin;
                if (r == 1) p->deleteFromParent();
                else if (r == 2) p->showWindowExplicitly();
                else if (r == 3) p->setEnabled (! p->isEnabled());
                else if (r == 4) { auto pm = PluginMenu::build (owner.project.session.getEngine(), PluginMenu::effects); pm.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, p] (int rr)
                {
                    auto sp = juce::Component::SafePointer<juce::Component> (this);
                    PluginMenu::handle (owner.project.session.getEngine(), rr, true, [this, sp, p] (int id) { if (sp != nullptr) owner.insertAt (index, PluginMenu::createFromResult (owner.project.session, id), p); });
                }); return; }
                else if (r == 5 || r == 6)
                {
                    auto ins = owner.inserts(); auto i = ins.indexOf (p.get()); auto j = r == 5 ? i - 1 : i + 1;
                    if (j >= 0 && j < ins.size()) { auto parent = p->state.getParent(); parent.moveChild (parent.indexOf (p->state), parent.indexOf (ins[j]->state), nullptr); }
                }
                owner.rebuild();
            });
        }
        RackStrip& owner; int index; te::Plugin::Ptr plugin;
    };

    MasteringProject& project;
    juce::String name;
    te::AudioTrack* track = nullptr;
    juce::Slider fader;
    juce::TextButton allBtn { "Activate all / bypass all" };
    std::vector<std::unique_ptr<Slot>> slots;
};

//==============================================================================
class MeterPanel : public juce::Component   // spectrum + level/loudness + phase, like the Project page footer
{
public:
    explicit MeterPanel (MasteringProject& p) : project (p)
    {
        spectrumMode.addItemList ({ "Octave", "1/3 Octave", "1/12 Octave", "FFT", "FFT Curve", "Waterfall", "Sonogram" }, 1); spectrumMode.setSelectedId (2, juce::dontSendNotification);
        historyLen.addItemList ({ "5 s", "20 s" }, 1); historyLen.setSelectedId (1, juce::dontSendNotification);
        historyLen.setTooltip ("How much history the Waterfall and Sonogram show");
        segmentsBtn.setClickingTogglesState (true); segmentsBtn.setTooltip ("Segments: draw the band views as LED segments with a dB ruler");
        addAndMakeVisible (historyLen); addAndMakeVisible (segmentsBtn);
        levelMode.addItemList ({ "Peak / RMS", "EBU R128", "K-20", "K-14", "K-12" }, 1); levelMode.setSelectedId (2, juce::dontSendNotification);
        lufsUnit.addItemList ({ "LUFS", "LU (rel. -23)" }, 1); lufsUnit.setSelectedId (1, juce::dontSendNotification);
        resetBtn.onClick = [this] { project.hub.reset(); };
        for (auto* c : { (juce::Component*) &spectrumMode, (juce::Component*) &levelMode, (juce::Component*) &lufsUnit, (juce::Component*) &resetBtn }) addAndMakeVisible (c);
        freezeBtn.setClickingTogglesState (true); addAndMakeVisible (freezeBtn);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4);
        auto left = r.removeFromLeft (r.getWidth() * 45 / 100);
        auto ctl = left.removeFromTop (20);
        spectrumMode.setBounds (ctl.removeFromLeft (110)); ctl.removeFromLeft (6); freezeBtn.setBounds (ctl.removeFromLeft (60));
        ctl.removeFromLeft (6); segmentsBtn.setBounds (ctl.removeFromLeft (72)); ctl.removeFromLeft (6); historyLen.setBounds (ctl.removeFromLeft (64));
        spectrumArea = left.reduced (0, 2);
        r.removeFromLeft (8);
        auto mid = r.removeFromLeft (r.getWidth() * 62 / 100);
        auto ctl2 = mid.removeFromTop (20);
        levelMode.setBounds (ctl2.removeFromLeft (110)); ctl2.removeFromLeft (6); lufsUnit.setBounds (ctl2.removeFromLeft (110)); ctl2.removeFromLeft (6); resetBtn.setBounds (ctl2.removeFromLeft (60));
        levelArea = mid.reduced (0, 2);
        r.removeFromLeft (8);
        r.removeFromTop (20);
        phaseArea = r.reduced (0, 2);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::panel()); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        auto& hub = project.hub;
        drawSpectrum (g, spectrumArea.toFloat(), hub);
        drawLevels (g, levelArea.toFloat(), hub);
        drawPhase (g, phaseArea.toFloat(), hub);
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r, AnalysisHub& hub)
    {
        g.setColour (Theme::bg()); g.fillRoundedRectangle (r, 4.0f);
        auto plot = r.reduced (4.0f).withTrimmedBottom (12.0f);
        auto xFor = [plot] (float hz) { return plot.getX() + plot.getWidth() * (std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f)); };
        auto yFor = [plot] (float db) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * plot.getHeight(); };
        g.setColour (Theme::line().withAlpha (0.5f));
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f }) g.drawVerticalLine ((int) xFor (hz), plot.getY(), plot.getBottom());
        for (float db : { -72.0f, -54.0f, -36.0f, -18.0f, 0.0f }) g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
        g.setFont (Theme::font (8.0f)); g.setColour (Theme::dim());
        for (float hz : { 100.0f, 1000.0f, 10000.0f }) g.drawText (hz >= 1000 ? juce::String (juce::roundToInt (hz / 1000)) + "k" : juce::String ((int) hz), (int) xFor (hz) + 2, (int) plot.getBottom(), 30, 12, juce::Justification::left);
        auto& sp = hub.spectrum;
        if (! freezeBtn.getToggleState()) { history.maxRows = SpectrumHistory::rowsFor (historyLen.getSelectedId() == 2 ? 20.0 : 5.0, hub.sr); history.update (sp, hub.sr); }
        if (! sp.ready) return;
        const int mode = spectrumMode.getSelectedId();   // 1 oct, 2 third, 3 twelfth, 4 FFT, 5 FFT curve, 6 waterfall, 7 sonogram
        if (mode <= 3)
        {
            const int perOct = mode == 1 ? 1 : mode == 2 ? 3 : 12;
            auto bands = SpectrumBands::compute (sp, hub.sr, perOct);
            auto peakBands = SpectrumBands::compute (sp.peaks, SpectrumAnalyser::size / 2, SpectrumAnalyser::size, hub.sr, perOct);
            const bool seg = segmentsBtn.getToggleState();
            for (size_t i = 0; i < bands.size(); ++i)
            {
                auto& bd = bands[i];
                const float x0 = xFor (bd.lo), x1 = xFor (bd.hi), w = juce::jmax (1.5f, x1 - x0 - (perOct == 12 ? 0.5f : 2.0f));
                auto bar = juce::Rectangle<float> (x0 + (perOct == 12 ? 0.25f : 1.0f), yFor (bd.db), w, plot.getBottom() - yFor (bd.db));
                if (seg)
                {
                    // LED segments every 3 dB, coloured by height (green - yellow - red near 0 dBFS)
                    for (float db = -90.0f; db < bd.db; db += 3.0f)
                    {
                        auto segR = juce::Rectangle<float> (bar.getX(), yFor (db + 3.0f) + 1.0f, bar.getWidth(), juce::jmax (1.0f, yFor (db) - yFor (db + 3.0f) - 1.5f));
                        g.setColour (db >= -6.0f ? Theme::red() : db >= -18.0f ? Theme::amber() : Theme::mint());
                        g.fillRect (segR);
                    }
                }
                else
                {
                    g.setGradientFill (juce::ColourGradient (Theme::mint(), 0, plot.getBottom(), Theme::acid(), 0, plot.getY(), false));
                    g.fillRect (bar);
                }
                g.setColour (Theme::text().withAlpha (0.8f)); g.fillRect (bar.getX(), yFor (peakBands[i].db), bar.getWidth(), 1.5f);
            }
            if (seg)
            {
                g.setFont (Theme::font (8.0f)); g.setColour (Theme::dim());
                for (float db : { -72.0f, -54.0f, -36.0f, -18.0f, -6.0f, 0.0f }) g.drawText (juce::String ((int) db), (int) plot.getRight() - 22, (int) yFor (db) - 6, 20, 12, juce::Justification::centredRight);
            }
        }
        else if (mode <= 5)
        {
            juce::Path p, pk;
            bool first = true;
            for (int b = 1; b < SpectrumAnalyser::size / 2; ++b)
            {
                auto hz = sp.binHz (b, hub.sr);
                if (hz < 20.0f || hz > 20000.0f) continue;
                auto x = xFor (hz), y = yFor (sp.magnitudes[b]), y2 = yFor (sp.peaks[b]);
                if (first) { p.startNewSubPath (x, y); pk.startNewSubPath (x, y2); first = false; } else { p.lineTo (x, y); pk.lineTo (x, y2); }
            }
            if (mode == 5) { juce::Path f (p); f.lineTo (plot.getRight(), plot.getBottom()); f.lineTo (plot.getX(), plot.getBottom()); f.closeSubPath(); g.setColour (Theme::mint().withAlpha (0.25f)); g.fillPath (f); }
            g.setColour (mode == 5 ? Theme::mint() : Theme::text()); g.strokePath (p, juce::PathStrokeType (1.2f));
            g.setColour (Theme::acid().withAlpha (0.6f)); g.strokePath (pk, juce::PathStrokeType (1.0f));
            // -3 dB/oct reference slope for the FFT view
            g.setColour (Theme::dim().withAlpha (0.4f)); g.drawLine (xFor (20.0f), yFor (-18.0f), xFor (20000.0f), yFor (-48.0f), 1.0f);
        }
        else if (mode == 6) drawWaterfall (g, plot, xFor);
        else drawSonogram (g, plot, xFor);
    }

    /** 1.12: Waterfall - the newest spectrum at the front, older ones stacked behind, rising and fading. */
    template <typename XF> void drawWaterfall (juce::Graphics& g, juce::Rectangle<float> plot, XF xFor)
    {
        const int n = (int) history.rows.size();
        if (n == 0) return;
        const int shown = juce::jmin (n, 48), step = juce::jmax (1, n / shown);
        const float depth = plot.getHeight() * 0.45f, slant = plot.getWidth() * 0.12f;
        auto front = plot.withTrimmedTop (depth).withTrimmedRight (slant);
        for (int k = shown - 1; k >= 0; --k)
        {
            const int idx = n - 1 - k * step;
            if (idx < 0) continue;
            auto& row = history.rows[(size_t) idx];
            const float age = (float) k / (float) juce::jmax (1, shown - 1);
            const float dx = age * slant, dy = -age * depth;
            juce::Path p; bool first = true;
            for (int c = 0; c < SpectrumHistory::columns; ++c)
            {
                const float x = front.getX() + (xFor (SpectrumHistory::columnHz (c)) - plot.getX()) * (front.getWidth() / plot.getWidth()) + dx;
                const float y = front.getBottom() - juce::jlimit (0.0f, 1.0f, (row[(size_t) c] + 90.0f) / 90.0f) * front.getHeight() + dy;
                if (first) { p.startNewSubPath (x, y); first = false; } else p.lineTo (x, y);
            }
            juce::Path fill (p); fill.lineTo (front.getRight() + dx, front.getBottom() + dy); fill.lineTo (front.getX() + dx, front.getBottom() + dy); fill.closeSubPath();
            g.setColour (Theme::bg().withAlpha (0.92f)); g.fillPath (fill);   // hides the rows behind, like a 3-D surface
            g.setColour (Theme::mint().interpolatedWith (Theme::acid(), 1.0f - age).withAlpha (0.35f + 0.65f * (1.0f - age)));
            g.strokePath (p, juce::PathStrokeType (k == 0 ? 1.4f : 0.9f));
        }
    }

    /** 1.12: Sonogram - time runs right to left (newest at the right edge), frequency up, level as colour. */
    template <typename XF> void drawSonogram (juce::Graphics& g, juce::Rectangle<float> plot, XF)
    {
        const int n = (int) history.rows.size();
        if (n == 0) return;
        const int w = juce::jmax (1, (int) plot.getWidth()), h = juce::jmax (1, (int) plot.getHeight());
        if (sonoImage.getWidth() != w || sonoImage.getHeight() != h) sonoImage = juce::Image (juce::Image::RGB, w, h, true);
        juce::Image::BitmapData bd (sonoImage, juce::Image::BitmapData::writeOnly);
        for (int x = 0; x < w; ++x)
        {
            const int idx = juce::jlimit (0, n - 1, (int) ((float) x / (float) w * (float) history.maxRows) - (history.maxRows - n));   // the history length fills the width, newest at the right
            for (int y = 0; y < h; ++y)
            {
                juce::Colour col = Theme::bg();
                if ((int) ((float) x / (float) w * (float) history.maxRows) >= history.maxRows - n)
                {
                    const int c = juce::jlimit (0, SpectrumHistory::columns - 1, (int) ((1.0f - (float) y / (float) h) * SpectrumHistory::columns));
                    col = SpectrumHistory::colourFor (history.rows[(size_t) idx][(size_t) c]);
                }
                bd.setPixelColour (x, y, col);
            }
        }
        g.drawImage (sonoImage, plot);
        g.setFont (Theme::font (8.0f)); g.setColour (Theme::text().withAlpha (0.7f));
        for (float hz : { 100.0f, 1000.0f, 10000.0f })
        {
            const float y = plot.getBottom() - plot.getHeight() * (std::log2 (hz / 20.0f) * 24.0f / SpectrumHistory::columns);
            g.drawText (hz >= 1000 ? juce::String (juce::roundToInt (hz / 1000)) + "k" : juce::String ((int) hz), (int) plot.getX() + 2, (int) y - 6, 30, 12, juce::Justification::left);
        }
    }

    void drawLevels (juce::Graphics& g, juce::Rectangle<float> r, AnalysisHub& hub)
    {
        g.setColour (Theme::bg()); g.fillRoundedRectangle (r, 4.0f);
        auto inner = r.reduced (6.0f);
        auto bars = inner.removeFromTop (inner.getHeight() * 0.5f);
        auto mode = levelMode.getSelectedId();
        float topDb = 0.0f, bottomDb = -60.0f; juce::String scaleName = "dBFS";
        if (mode == 3) { topDb = 20.0f; bottomDb = -40.0f; scaleName = "K-20"; }
        else if (mode == 4) { topDb = 14.0f; bottomDb = -46.0f; scaleName = "K-14"; }
        else if (mode == 5) { topDb = 12.0f; bottomDb = -48.0f; scaleName = "K-12"; }
        auto offset = topDb;   // K-system: 0 on the scale = -20/-14/-12 dBFS
        auto xFor = [bars, topDb, bottomDb] (float db) { return bars.getX() + juce::jlimit (0.0f, 1.0f, (db - bottomDb) / (topDb - bottomDb)) * bars.getWidth(); };
        auto drawBar = [&] (juce::Rectangle<float> b, float peakDb, float rmsDb, const juce::String& label)
        {
            g.setColour (Theme::panel2()); g.fillRoundedRectangle (b, 2.0f);
            auto scaledPeak = peakDb + offset, scaledRms = rmsDb + offset;
            juce::ColourGradient grad (Theme::mint(), b.getX(), 0, Theme::red(), b.getRight(), 0, false);
            grad.addColour (0.78, Theme::acid()); grad.addColour (0.92, Theme::amber());
            g.setGradientFill (grad);
            g.fillRect (b.withRight (xFor (scaledPeak)).reduced (0.0f, 1.0f));
            g.setColour (Theme::text().withAlpha (0.5f)); g.fillRect (b.withRight (xFor (scaledRms)).reduced (0.0f, 4.0f));
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f, true)); g.drawText (label, b.toNearestInt().withWidth (14).translated (-16, 0), juce::Justification::centredRight);
        };
        if (mode == 2)
        {
            auto& lm = hub.loudness;
            bottomDb = -50.0f; topDb = 0.0f; offset = 0.0f;
            auto b1 = bars.removeFromTop (bars.getHeight() / 2).reduced (16.0f, 3.0f).withTrimmedLeft (4.0f), b2 = bars.reduced (16.0f, 3.0f).withTrimmedLeft (4.0f);
            drawBar (b1, lm.getShortTerm(), lm.getShortTerm(), "S");
            drawBar (b2, lm.getMomentary(), lm.getMomentary(), "M");
            scaleName = "LUFS";
        }
        else
        {
            auto b1 = bars.removeFromTop (bars.getHeight() / 2).reduced (16.0f, 3.0f).withTrimmedLeft (4.0f), b2 = bars.reduced (16.0f, 3.0f).withTrimmedLeft (4.0f);
            drawBar (b1, hub.peak[0], hub.rms[0], "L");
            drawBar (b2, hub.peak[1], hub.rms[1], "R");
        }
        // scale labels
        g.setFont (Theme::font (8.0f)); g.setColour (Theme::dim());
        auto sc = inner.removeFromTop (12.0f);
        for (float db = bottomDb; db <= topDb + 0.01f; db += (topDb - bottomDb) / 6.0f)
            g.drawText (juce::String ((int) db), (int) (sc.getX() + 20.0f + (db - bottomDb) / (topDb - bottomDb) * (sc.getWidth() - 20.0f)) - 12, (int) sc.getY(), 24, 11, juce::Justification::centred);
        g.drawText (scaleName, sc.toNearestInt(), juce::Justification::centredRight);
        // clip LED
        auto led = juce::Rectangle<float> (r.getRight() - 48.0f, r.getY() + 6.0f, 40.0f, 12.0f);
        g.setColour (hub.clipped ? Theme::red() : Theme::panel2()); g.fillRoundedRectangle (led, 2.0f);
        g.setColour (hub.clipped ? juce::Colours::white : Theme::dim()); g.setFont (Theme::font (8.0f, true)); g.drawText ("CLIP", led.toNearestInt(), juce::Justification::centred);
        // numbers
        auto& lm = hub.loudness;
        bool lu = lufsUnit.getSelectedId() == 2;
        auto fmt = [lu] (float v) { if (v <= -99.0f) return juce::String ("---"); return juce::String (lu ? v + 23.0f : v, 1); };
        auto nums = inner;
        auto cell = [&] (const juce::String& label, const juce::String& value, juce::Colour c)
        {
            auto cw = nums.getWidth() / 5.0f;
            auto cr = nums.removeFromLeft (cw);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true)); g.drawText (label, cr.toNearestInt().removeFromTop (12), juce::Justification::centred);
            g.setColour (c); g.setFont (Theme::font (16.0f, true)); g.drawText (value, cr.toNearestInt().withTrimmedTop (12), juce::Justification::centred);
        };
        cell ("INT", fmt (lm.getIntegrated()), Theme::acid());
        cell ("SHORT", fmt (lm.getShortTerm()), Theme::text());
        cell ("MOM", fmt (lm.getMomentary()), Theme::text());
        cell ("LRA", lm.getRange() > 0 ? juce::String (lm.getRange(), 1) : "---", Theme::mint());
        cell ("TRUE PK", lm.getTruePeak() > -99.0f ? juce::String (lm.getTruePeak(), 1) : "---", lm.getTruePeak() > -1.0f ? Theme::red() : Theme::amber());
    }

    void drawPhase (juce::Graphics& g, juce::Rectangle<float> r, AnalysisHub& hub)
    {
        g.setColour (Theme::bg()); g.fillRoundedRectangle (r, 4.0f);
        auto corr = r.removeFromBottom (18.0f).reduced (6.0f, 3.0f);
        auto gon = r.reduced (6.0f).withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()) - 12.0f, juce::jmin (r.getWidth(), r.getHeight()) - 12.0f);
        g.setColour (Theme::line().withAlpha (0.5f));
        g.drawLine (gon.getX(), gon.getBottom(), gon.getRight(), gon.getY()); g.drawLine (gon.getX(), gon.getY(), gon.getRight(), gon.getBottom());
        g.drawLine (gon.getCentreX(), gon.getY(), gon.getCentreX(), gon.getBottom());
        g.setFont (Theme::font (8.0f)); g.setColour (Theme::dim());
        g.drawText ("L", gon.toNearestInt().reduced (2), juce::Justification::topLeft); g.drawText ("R", gon.toNearestInt().reduced (2), juce::Justification::topRight);
        auto& ph = hub.phase;
        for (auto& pt : ph.points)
        {
            auto m = (pt.first + pt.second) * 0.5f, s = (pt.second - pt.first) * 0.5f;   // mid up, side across
            auto x = gon.getCentreX() + juce::jlimit (-1.0f, 1.0f, s) * gon.getWidth() * 0.48f, y = gon.getCentreY() - juce::jlimit (-1.0f, 1.0f, m) * gon.getHeight() * 0.48f;
            g.setColour (Theme::mint().withAlpha (0.55f)); g.fillRect (x, y, 1.5f, 1.5f);
        }
        // correlation bar -1..+1
        g.setColour (Theme::panel2()); g.fillRoundedRectangle (corr, 2.0f);
        auto cx = corr.getX() + (ph.correlation + 1.0f) * 0.5f * corr.getWidth();
        g.setColour (ph.correlation < 0.0f ? Theme::red() : Theme::mint()); g.fillRect (cx - 2.0f, corr.getY(), 4.0f, corr.getHeight());
        g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f, true));
        g.drawText ("-1", corr.toNearestInt(), juce::Justification::centredLeft); g.drawText ("+1", corr.toNearestInt(), juce::Justification::centredRight);
        g.drawText (juce::String (ph.correlation, 2), corr.toNearestInt(), juce::Justification::centred);
    }

    MasteringProject& project;
    juce::ComboBox spectrumMode, levelMode, lufsUnit, historyLen;
    juce::TextButton resetBtn { "Reset" }, freezeBtn { "Freeze" }, segmentsBtn { "Segments" };
    SpectrumHistory history;
    juce::Image sonoImage;
    juce::Rectangle<int> spectrumArea, levelArea, phaseArea;
};

//==============================================================================
class TrackLane : public juce::Component, private juce::Timer
{
public:
    explicit TrackLane (MasteringProject& p) : project (p) { startTimerHz (20); }
    std::function<void (int)> onSelect;
    int selected = -1;

    void timerCallback() override   { repaint(); }

    float xFor (double secs) const   { auto total = juce::jmax (10.0, project.totalLength() + 2.0); return 6.0f + (float) (secs / total) * ((float) getWidth() - 12.0f); }
    double secsFor (float x) const   { auto total = juce::jmax (10.0, project.totalLength() + 2.0); return juce::jlimit (0.0, total, (double) ((x - 6.0f) / ((float) getWidth() - 12.0f)) * total); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::panel()); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        auto ruler = r.removeFromTop (16.0f);
        auto markers = r.removeFromTop (14.0f);
        g.setColour (Theme::bg()); g.fillRect (r);
        auto total = juce::jmax (10.0, project.totalLength() + 2.0);
        double step = total > 1800 ? 300 : total > 600 ? 60 : total > 120 ? 30 : 10;
        g.setFont (Theme::font (8.5f));
        for (double t = 0; t <= total; t += step)
        {
            auto x = xFor (t);
            g.setColour (Theme::line()); g.drawVerticalLine ((int) x, ruler.getY(), r.getBottom());
            g.setColour (Theme::dim()); g.drawText (MasteringProject::timeText (t).upToFirstOccurrenceOf (".", false, false), (int) x + 2, (int) ruler.getY(), 50, 14, juce::Justification::centredLeft);
        }
        int i = 0;
        for (auto t : project.tracks())
        {
            auto c = MasteringProject::clipOf (*t);
            if (c == nullptr || ! MasteringProject::enabledOf (*t)) { ++i; continue; }
            auto x0 = xFor (project.startOf (*t)), x1 = xFor (project.endOf (*t));
            auto block = juce::Rectangle<float> (x0, r.getY() + 2.0f, juce::jmax (3.0f, x1 - x0), r.getHeight() - 4.0f);
            auto base = t->getColour().getBrightness() < 0.15f ? Theme::nextTrackColour (i) : t->getColour();
            auto col = i == selected ? Theme::acid() : base.withMultipliedBrightness (1.1f);
            g.setColour (col.withAlpha (0.25f)); g.fillRoundedRectangle (block, 3.0f);
            // waveform
            auto& thumb = thumbs[t->itemID];
            te::AudioFile file (c->getPlaybackFile());
            if (thumb == nullptr || thumb->file != file) thumb = std::make_unique<te::SmartThumbnail> (project.session.getEngine(), file, *this, project.edit.get());
            if (! thumb->isGeneratingProxy() && ! thumb->isOutOfDate())
            {
                g.setColour (col.withAlpha (0.9f));
                thumb->drawChannels (g, block.reduced (1.0f, 6.0f).toNearestInt(), { te::TimePosition(), te::TimePosition::fromSeconds (project.lengthOf (*t)) }, 1.0f);
            }
            // fades (volume curve)
            auto fi = c->getFadeIn().inSeconds(), fo = c->getFadeOut().inSeconds();
            g.setColour (Theme::text().withAlpha (0.7f));
            if (fi > 0) g.drawLine (block.getX(), block.getBottom(), xFor (project.startOf (*t) + fi), block.getY(), 1.2f);
            if (fo > 0) g.drawLine (xFor (project.endOf (*t) - fo), block.getY(), block.getRight(), block.getBottom(), 1.2f);
            g.setColour (col); g.drawRoundedRectangle (block, 3.0f, i == selected ? 2.0f : 1.0f);
            g.setFont (Theme::font (10.0f, true)); g.setColour (Theme::text());
            g.drawFittedText (juce::String (i + 1) + "  " + t->getName(), block.toNearestInt().reduced (4, 2), juce::Justification::topLeft, 1);
            // track marker (blue like the automatic ones)
            g.setColour (juce::Colour (0xff4aa3ff));
            juce::Path m; m.addTriangle (x0 - 5.0f, markers.getY() + 1.0f, x0 + 5.0f, markers.getY() + 1.0f, x0, markers.getBottom() - 1.0f); g.fillPath (m);
            g.setFont (Theme::font (8.0f, true)); g.drawText (juce::String (i + 1), (int) x0 + 6, (int) markers.getY(), 20, 12, juce::Justification::centredLeft);
            ++i;
        }
        // playhead
        auto px = xFor (project.position());
        g.setColour (Theme::acid()); g.drawLine (px, ruler.getY(), px, r.getBottom(), 1.5f);
        if (project.numTracks() == 0)
        {
            g.setColour (Theme::dim()); g.setFont (Theme::font (12.0f));
            g.drawText ("Drop audio files here, or use Add Files... / Add Current Session", r.toNearestInt(), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto secs = secsFor (e.position.x);
        int i = 0;
        for (auto t : project.tracks())
        {
            if (MasteringProject::enabledOf (*t) && secs >= project.startOf (*t) && secs <= project.endOf (*t)) { selected = i; if (onSelect) onSelect (i); break; }
            ++i;
        }
        project.locate (secs);
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override   { mouseDown (e); project.play (secsFor (e.position.x)); }

    MasteringProject& project;
    std::map<te::EditItemID, std::unique_ptr<te::SmartThumbnail>> thumbs;
};

//==============================================================================
class MasteringContent : public juce::Component,
                         private juce::ChangeListener,
                         private juce::Timer,
                         public juce::FileDragAndDropTarget,
                         private juce::ListBoxModel
{
public:
    explicit MasteringContent (MasteringProject& p) : project (p), lane (p), meters (p), masterRack (p, "Master Device Rack"), trackRack (p, "Track Device Rack")
    {
        setSize (1280, 820);
        masterRack.isMaster = true;
        masterRack.rebuild();
        project.addChangeListener (this);
        lane.onSelect = [this] (int i) { trackList.selectRow (i); selectTrack (i); };
        addAndMakeVisible (lane); addAndMakeVisible (meters); addAndMakeVisible (masterRack); addAndMakeVisible (trackRack);

        trackList.setModel (this);
        trackList.setRowHeight (46);
        trackList.setColour (juce::ListBox::backgroundColourId, Theme::panel());
        addAndMakeVisible (trackList);

        auto btn = [this] (juce::TextButton& b, std::function<void()> fn, const juce::String& tip = {}) { b.onClick = fn; b.setTooltip (tip); addAndMakeVisible (b); };
        btn (newBtn, [this] { newProject(); }, "Start a new mastering project");
        btn (openBtn, [this] { openProject(); }, "Open a mastering project (.sqmaster)");
        btn (saveBtn, [this] { project.save(); }, "Save the mastering project");
        btn (addFilesBtn, [this] { addFiles(); }, "Add audio files (WAV, AIFF, FLAC, MP3, OGG) as album tracks");
        btn (addSessionBtn, [this] { ensureProject(); project.addCurrentSession(); }, "Bounce the session open in the Edit window and add it as a track; Update from Session re-bounces it later");
        btn (updateBtn, [this] { if (auto t = project.track (selectedRow)) { if (! project.updateFromSession (*t)) msg ("Update from Session", "Open the session this track came from in the Edit window first, then press Update."); } }, "Re-bounce this track from its session");
        btn (removeBtn, [this] { if (auto t = project.track (selectedRow)) project.removeTrack (*t); }, "Remove the selected track");
        btn (upBtn, [this] { if (auto t = project.track (selectedRow)) { project.moveTrack (*t, -1); trackList.selectRow (selectedRow - 1); } }, "Move the track earlier");
        btn (downBtn, [this] { if (auto t = project.track (selectedRow)) { project.moveTrack (*t, 1); trackList.selectRow (selectedRow + 1); } }, "Move the track later");
        btn (loudBtn, [this] { detectLoudness(); }, "Measure EBU R128 loudness (integrated, LRA, true peak) of the selected track, pre and post FX");
        btn (metaBtn, [this] { trackMetadata(); }, "Title, artist, songwriter, composer, ISRC, pause, fades, flags for the selected track");
        btn (albumBtn, [this] { albumMetadata(); }, "Album title, artist, genre, year, UPC/EAN, label, copyright, artwork");
        btn (playBtn, [this] { if (project.isPlaying()) project.stop(); else project.play(); }, "Play / stop the album from the playhead (Space)");
        btn (rtzBtn, [this] { project.locate (0.0); }, "Return to the start");
        btn (digitalBtn, [this] { publishDigital(); }, "Export one tagged file per track, with Target Loudness options");
        btn (imageBtn, [this] { publishImage(); }, "Export the whole album as one WAV + CUE sheet (disc image)");
        btn (ddpBtn, [this] { publishDDP(); }, "Export a DDP 2.00 fileset for the pressing plant");
        btn (burnBtn, [this] { msg ("Burn Audio CD", "Make an Image (WAV + CUE) or a DDP, then burn it with the Mac's built-in tools or your duplicator's software: Finder can burn the WAV; most CD apps open the CUE. Direct burning from inside Squelchmoth arrives in a later update."); }, "Burning a CD directly needs the Image or DDP export - see the note");

        for (auto* l : { &albumField, &artistField })
        {
            l->setEditable (true);
            l->setColour (juce::Label::backgroundColourId, Theme::panel2());
            l->setFont (Theme::font (12.0f));
            addAndMakeVisible (*l);
        }
        albumField.onTextChange = [this] { ensureProject(); project.setAlbum ("mAlbum", albumField.getText()); };
        artistField.onTextChange = [this] { ensureProject(); project.setAlbum ("mAlbumArtist", artistField.getText()); };
        timeLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold)));
        timeLabel.setColour (juce::Label::textColourId, Theme::acid());
        timeLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (timeLabel);
        targetBox.addItem ("Target loudness: off", 1);
        for (size_t i = 1; i < loudnessTargets().size(); ++i) targetBox.addItem (loudnessTargets()[i].name, (int) i + 1);
        targetBox.setSelectedId (Prefs::getInt ("ma_target", 1), juce::dontSendNotification);
        targetBox.onChange = [this] { Prefs::set ("ma_target", targetBox.getSelectedId()); };
        targetBox.setTooltip ("Target Loudness used by Digital Release: tracks are matched to the service's loudness, and the max true peak always wins");
        addAndMakeVisible (targetBox);

        setWantsKeyboardFocus (true);
        refreshAll();
        startTimerHz (30);
    }

    ~MasteringContent() override   { project.removeChangeListener (this); project.save(); }

    //==========================================================================
    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto top = r.removeFromTop (28);
        newBtn.setBounds (top.removeFromLeft (54)); top.removeFromLeft (4); openBtn.setBounds (top.removeFromLeft (60)); top.removeFromLeft (4); saveBtn.setBounds (top.removeFromLeft (54)); top.removeFromLeft (14);
        albumField.setBounds (top.removeFromLeft (220)); top.removeFromLeft (6); artistField.setBounds (top.removeFromLeft (180)); top.removeFromLeft (6); albumBtn.setBounds (top.removeFromLeft (120)); top.removeFromLeft (14);
        rtzBtn.setBounds (top.removeFromLeft (40)); top.removeFromLeft (4); playBtn.setBounds (top.removeFromLeft (70)); top.removeFromLeft (6); timeLabel.setBounds (top.removeFromLeft (120)); top.removeFromLeft (14);
        burnBtn.setBounds (top.removeFromRight (60)); top.removeFromRight (4); ddpBtn.setBounds (top.removeFromRight (56)); top.removeFromRight (4); imageBtn.setBounds (top.removeFromRight (70)); top.removeFromRight (4); digitalBtn.setBounds (top.removeFromRight (110)); top.removeFromRight (8);
        targetBox.setBounds (top.removeFromRight (250));
        r.removeFromTop (8);
        meters.setBounds (r.removeFromBottom (210));
        r.removeFromBottom (8);
        auto left = r.removeFromLeft (360);
        auto lb = left.removeFromBottom (82);
        auto row1 = lb.removeFromTop (24), row2 = lb.removeFromTop (24).translated (0, 4), row3 = lb.translated (0, 8).removeFromTop (24);
        addFilesBtn.setBounds (row1.removeFromLeft (90)); row1.removeFromLeft (4); addSessionBtn.setBounds (row1.removeFromLeft (130)); row1.removeFromLeft (4); updateBtn.setBounds (row1.removeFromLeft (126));
        removeBtn.setBounds (row2.removeFromLeft (70)); row2.removeFromLeft (4); upBtn.setBounds (row2.removeFromLeft (40)); row2.removeFromLeft (4); downBtn.setBounds (row2.removeFromLeft (50)); row2.removeFromLeft (4); metaBtn.setBounds (row2.removeFromLeft (120));
        loudBtn.setBounds (row3.removeFromLeft (130));
        left.removeFromBottom (6);
        trackList.setBounds (left);
        r.removeFromLeft (8);
        auto right = r.removeFromRight (250);
        masterRack.setBounds (right.removeFromTop (right.getHeight() / 2 - 4));
        right.removeFromTop (8);
        trackRack.setBounds (right);
        r.removeFromRight (8);
        lane.setBounds (r);
    }

    void paint (juce::Graphics& g) override   { g.fillAll (Theme::bg()); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::spaceKey) { if (project.isPlaying()) project.stop(); else project.play(); return true; }
        if (k == juce::KeyPress::returnKey) { project.locate (0.0); return true; }
        return false;
    }

    //==========================================================================
    // track list
    int getNumRows() override   { return project.numTracks(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        auto t = project.track (row); if (t == nullptr) return;
        bool on = MasteringProject::enabledOf (*t);
        g.setColour (sel ? Theme::acid().withAlpha (0.15f) : (row % 2 ? Theme::panel2().withAlpha (0.4f) : juce::Colours::transparentBlack)); g.fillRect (0, 0, w, h);
        g.setColour (t->getColour()); g.fillRect (0, 0, 4, h);
        g.setColour (on ? Theme::text() : Theme::dim()); g.setFont (Theme::font (12.0f, true));
        g.drawText (juce::String (row + 1).paddedLeft ('0', 2) + "  " + t->getName() + (on ? "" : "   (disabled)"), 10, 3, w - 100, 18, juce::Justification::centredLeft);
        g.setColour (Theme::dim()); g.setFont (Theme::font (10.0f));
        g.drawText (MasteringProject::timeText (project.lengthOf (*t)), w - 90, 3, 80, 18, juce::Justification::centredRight);
        juce::String line2 = "pause " + juce::String (MasteringProject::pauseOf (*t), 1) + " s";
        if (MasteringProject::meta (*t, "mArtist").isNotEmpty()) line2 << "   " << MasteringProject::meta (*t, "mArtist");
        if (MasteringProject::meta (*t, "mIsrc").isNotEmpty()) line2 << "   ISRC " << MasteringProject::meta (*t, "mIsrc");
        g.drawText (line2, 10, 20, w - 20, 13, juce::Justification::centredLeft);
        juce::String line3;
        if (t->state.hasProperty ("mLufsPre")) line3 << "pre: " << juce::String ((float) t->state.getProperty ("mLufsPre"), 1) << " LUFS  LRA " << juce::String ((float) t->state.getProperty ("mLraPre"), 1) << "  TP " << juce::String ((float) t->state.getProperty ("mTpPre"), 1);
        if (t->state.hasProperty ("mLufsPost")) line3 << "   post: " << juce::String ((float) t->state.getProperty ("mLufsPost"), 1) << " LUFS  TP " << juce::String ((float) t->state.getProperty ("mTpPost"), 1);
        if (line3.isEmpty()) line3 = "loudness: not measured (Detect Loudness)";
        g.setColour (line3.contains ("not measured") ? Theme::dim().withAlpha (0.7f) : Theme::mint()); g.setFont (Theme::font (9.5f));
        g.drawText (line3, 10, 32, w - 20, 12, juce::Justification::centredLeft);
    }
    void selectedRowsChanged (int row) override   { selectTrack (row); }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override   { selectTrack (row); trackMetadata(); }
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        auto t = project.track (row); if (t == nullptr) return;
        juce::PopupMenu m;
        m.addItem (1, MasteringProject::enabledOf (*t) ? "Disable Track" : "Enable Track");
        m.addItem (2, "Detect Loudness");
        m.addItem (3, "Track meta-information...");
        m.addItem (4, "Copy meta-information to all tracks");
        m.addItem (5, "Update from Session", MasteringProject::meta (*t, "mSourceSession").isNotEmpty());
        m.addItem (6, "Open source session in the Edit window", MasteringProject::meta (*t, "mSourceSession").isNotEmpty());
        m.addItem (7, "Reveal audio file");
        juce::PopupMenu macros; auto macroNames = MasteringMacros::names();
        macros.addItem (20, "Save this track as a macro...");
        if (! macroNames.isEmpty()) macros.addSeparator();
        for (int i = 0; i < macroNames.size(); ++i) macros.addItem (100 + i, "Apply \"" + macroNames[i] + "\" to this track");
        for (int i = 0; i < macroNames.size(); ++i) macros.addItem (300 + i, "Apply \"" + macroNames[i] + "\" to every enabled track");
        m.addSeparator(); m.addSubMenu ("Track Macros", macros);
        m.addSeparator(); m.addItem (8, "Remove");
        m.showMenuAsync ({}, [this, t, row, macroNames] (int r)
        {
            if (r == 20)
            {
                auto aw = std::make_shared<juce::AlertWindow> ("Save Track Macro", "Saves this track's device rack, fader and fades as a recipe you can apply to other tracks.", juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", t->getName() + " chain", "Name");
                aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, t] (int res)
                {
                    aw->setVisible (false);
                    if (res != 1) return;
                    juce::String err; if (MasteringMacros::save (project, *t, aw->getTextEditorContents ("name"), err) < 0) msg ("Track Macros", err);
                }));
                return;
            }
            if (r >= 100 && r < 100 + macroNames.size()) { juce::String err; MasteringMacros::apply (project, *t, macroNames[r - 100], err); if (err.isNotEmpty()) msg ("Track Macros", err); refreshAll(); return; }
            if (r >= 300 && r < 300 + macroNames.size())
            {
                juce::String err;
                for (auto o : project.tracks()) if (MasteringProject::enabledOf (*o)) MasteringMacros::apply (project, *o, macroNames[r - 300], err);
                if (err.isNotEmpty()) msg ("Track Macros", err);
                refreshAll(); return;
            }
            if (r == 1) project.setMeta (*t, "mEnabled", ! MasteringProject::enabledOf (*t));
            else if (r == 2) { selectTrack (row); detectLoudness(); }
            else if (r == 3) { selectTrack (row); trackMetadata(); }
            else if (r == 4) { for (auto o : project.tracks()) if (o != t) for (auto* k : { "mArtist", "mSongwriter", "mComposer", "mPre", "mDcp", "mScms" }) o->state.setProperty (k, t->state.getProperty (k), nullptr); refreshAll(); }
            else if (r == 5) project.updateFromSession (*t);
            else if (r == 6) project.session.openSession (juce::File (MasteringProject::meta (*t, "mSourceSession")));
            else if (r == 7) juce::File (MasteringProject::meta (*t, "mSource")).revealToUser();
            else if (r == 8) project.removeTrack (*t);
        });
    }

    void selectTrack (int row)
    {
        selectedRow = row;
        lane.selected = row;
        trackRack.setTrack (project.track (row));
        trackRack.name = project.track (row) != nullptr ? "Track Device Rack - " + project.track (row)->getName() : "Track Device Rack";
        trackRack.repaint();
    }

    //==========================================================================
    bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;squelch;sqmaster")) return true; return false; }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        for (auto& f : files)
        {
            juce::File file (f);
            if (file.hasFileExtension ("sqmaster")) { project.openProject (file); continue; }
            ensureProject();
            if (file.hasFileExtension ("squelch")) { if (project.session.openSession (file)) project.addCurrentSession(); }
            else project.addAudioFile (file);
        }
    }

    //==========================================================================
    void ensureProject()   { if (! project.isOpen()) project.newProject ("Album"); }

    void newProject()
    {
        auto w = std::make_shared<juce::AlertWindow> ("New mastering project", "Album / project name", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("n", "My Album");
        w->addButton ("Create", 1); w->addButton ("Cancel", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) { if (r == 1) project.newProject (w->getTextEditorContents ("n")); }), false);
    }

    void openProject()
    {
        auto fc = std::make_shared<juce::FileChooser> ("Open a mastering project", project.root, "*.sqmaster");
        fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc] (const juce::FileChooser& c) { if (c.getResult().existsAsFile()) project.openProject (c.getResult()); });
    }

    void addFiles()
    {
        ensureProject();
        auto fc = std::make_shared<juce::FileChooser> ("Add tracks", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [this, fc] (const juce::FileChooser& c)
        {
            for (auto& f : c.getResults()) project.addAudioFile (f);
        });
    }

    void detectLoudness()
    {
        auto t = project.track (selectedRow);
        if (t == nullptr) { msg ("Detect Loudness", "Select a track first."); return; }
        auto pre = project.detectLoudness (*t, false);
        auto post = project.detectLoudness (*t, true);
        juce::String s;
        s << t->getName() << "\n\nPre-FX:   " << juce::String (pre.integrated, 1) << " LUFS integrated,  LRA " << juce::String (pre.range, 1) << " LU,  true peak " << juce::String (pre.truePeak, 1) << " dBTP,  RMS " << juce::String (pre.rms, 1) << " dB"
          << "\nPost-FX:  " << juce::String (post.integrated, 1) << " LUFS integrated,  LRA " << juce::String (post.range, 1) << " LU,  true peak " << juce::String (post.truePeak, 1) << " dBTP,  RMS " << juce::String (post.rms, 1) << " dB";
        msg ("Loudness", s);
    }

    void trackMetadata()
    {
        auto t = project.track (selectedRow);
        if (t == nullptr) return;
        auto w = std::make_shared<juce::AlertWindow> ("Track " + juce::String (selectedRow + 1) + " meta-information", "CD-Text and tags for this track", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("title", t->getName(), "Title");
        w->addTextEditor ("artist", MasteringProject::meta (*t, "mArtist"), "Artist / performer");
        w->addTextEditor ("songwriter", MasteringProject::meta (*t, "mSongwriter"), "Songwriter");
        w->addTextEditor ("composer", MasteringProject::meta (*t, "mComposer"), "Composer");
        w->addTextEditor ("isrc", MasteringProject::meta (*t, "mIsrc"), "ISRC (12 characters)");
        w->addTextEditor ("pause", juce::String (MasteringProject::pauseOf (*t), 2), "Pause before (s, 2-10 for track 1)");
        auto c = MasteringProject::clipOf (*t);
        w->addTextEditor ("fadein", juce::String (c ? c->getFadeIn().inSeconds() : 0.0, 2), "Fade in (s)");
        w->addTextEditor ("fadeout", juce::String (c ? c->getFadeOut().inSeconds() : 0.0, 2), "Fade out (s)");
        w->addComboBox ("flags", { "no flags", "pre-emphasis", "digital copy permitted", "copy protected (SCMS)" }, "CD flags");
        w->getComboBoxComponent ("flags")->setSelectedId ((bool) t->state.getProperty ("mPre", false) ? 2 : (bool) t->state.getProperty ("mDcp", false) ? 3 : (bool) t->state.getProperty ("mScms", false) ? 4 : 1, juce::dontSendNotification);
        w->addButton ("OK", 1); w->addButton ("Cancel", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, t] (int r)
        {
            if (r != 1) return;
            t->setName (w->getTextEditorContents ("title"));
            for (auto* k : { "artist", "songwriter", "composer" }) t->state.setProperty ("m" + juce::String (k).substring (0, 1).toUpperCase() + juce::String (k).substring (1), w->getTextEditorContents (k), nullptr);
            auto isrc = w->getTextEditorContents ("isrc").trim().toUpperCase().removeCharacters ("-");
            auto wasEmpty = MasteringProject::meta (*t, "mIsrc").isEmpty();
            t->state.setProperty ("mIsrc", isrc, nullptr);
            auto fl = w->getComboBoxComponent ("flags")->getSelectedId();
            t->state.setProperty ("mPre", fl == 2, nullptr); t->state.setProperty ("mDcp", fl == 3, nullptr); t->state.setProperty ("mScms", fl == 4, nullptr);
            project.setFades (*t, w->getTextEditorContents ("fadein").getDoubleValue(), w->getTextEditorContents ("fadeout").getDoubleValue());
            project.setMeta (*t, "mPause", w->getTextEditorContents ("pause").getDoubleValue());
            if (selectedRow == 0 && isrc.length() == 12 && wasEmpty && project.numTracks() > 1)
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "ISRC codes", "Assign incrementing ISRC codes to the other tracks automatically?", "Yes", "No", nullptr,
                                                    juce::ModalCallbackFunction::create ([this, isrc] (int ok) { if (ok == 1) project.autoIncrementIsrc (isrc); }));
            refreshAll();
        }), false);
    }

    void albumMetadata()
    {
        ensureProject();
        auto w = std::make_shared<juce::AlertWindow> ("Album meta-information", "Used for CD-Text, file tags, folder names and the DDP", juce::MessageBoxIconType::NoIcon);
        const char* keys[] = { "mAlbum", "mAlbumArtist", "mGenre", "mYear", "mUpc", "mLabel", "mCatalog", "mCopyright", "mComment", "mArtwork" };
        const char* labels[] = { "Album / disc title", "Album artist", "Genre", "Year", "UPC / EAN (12-13 digits)", "Label", "Catalog number", "Copyright", "Comment", "Artwork image path (max 1400x1400)" };
        for (int i = 0; i < 10; ++i) w->addTextEditor (keys[i], project.album (keys[i]), labels[i]);
        w->addButton ("OK", 1); w->addButton ("Cancel", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r != 1) return;
            for (auto* k : { "mAlbum", "mAlbumArtist", "mGenre", "mYear", "mUpc", "mLabel", "mCatalog", "mCopyright", "mComment", "mArtwork" }) project.setAlbum (k, w->getTextEditorContents (k));
            refreshAll();
        }), false);
    }

    void publishDigital()
    {
        if (project.numTracks() == 0) { msg ("Digital Release", "Add tracks first."); return; }
        auto w = std::make_shared<juce::AlertWindow> ("Digital Release", "One file per track, tagged with the album and track meta-information", juce::MessageBoxIconType::NoIcon);
        w->addComboBox ("format", { "WAV", "AIFF", "FLAC", "OGG Vorbis", "MP3 320 kb/s", "MP3 VBR V0" }, "Format");
        w->addComboBox ("depth", { "16-bit", "24-bit", "32-bit float" }, "Resolution"); w->getComboBoxComponent ("depth")->setSelectedId (2, juce::dontSendNotification);
        w->addComboBox ("rate", { "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz" }, "Sample rate");
        juce::StringArray targets; for (auto& t : loudnessTargets()) targets.add (t.name);
        w->addComboBox ("target", targets, "Adjust loudness"); w->getComboBoxComponent ("target")->setSelectedId (targetBox.getSelectedId(), juce::dontSendNotification);
        w->addTextEditor ("lufs", "-14", "Custom max loudness (LUFS)"); w->addTextEditor ("tp", "-1", "Custom max true peak (dBTP)");
        w->addComboBox ("album", { "Per track (each track hits the target)", "Album mode (one gain for the whole album)" }, "Mode");
        w->addComboBox ("names", { "01 Title", "Title", "01 Artist - Title" }, "File names");
        w->addButton ("Export", 1); w->addButton ("Cancel", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r != 1) return;
            MasteringProject::PublishOptions o;
            const char* fmts[] = { "wav", "aiff", "flac", "ogg", "mp3", "mp3" }; o.format = fmts[w->getComboBoxComponent ("format")->getSelectedId() - 1];
            if (w->getComboBoxComponent ("format")->getSelectedId() == 6) o.mp3VbrQuality = 0;
            const int depths[] = { 16, 24, 32 }; o.bitDepth = depths[w->getComboBoxComponent ("depth")->getSelectedId() - 1];
            const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 }; o.sampleRate = rates[w->getComboBoxComponent ("rate")->getSelectedId() - 1];
            o.loudnessTarget = w->getComboBoxComponent ("target")->getSelectedId() - 1;
            o.customLufs = w->getTextEditorContents ("lufs").getFloatValue(); o.customTruePeak = w->getTextEditorContents ("tp").getFloatValue();
            o.albumMode = w->getComboBoxComponent ("album")->getSelectedId() == 2;
            auto nm = w->getComboBoxComponent ("names")->getSelectedId(); o.trackNumbers = nm != 2; o.artistInNames = nm == 3;
            juce::String report;
            auto files = project.publishDigital (o, report);
            msg ("Digital Release", juce::String (files.size()) + " files written to\n" + (o.folder != juce::File() ? o.folder : project.defaultReleaseFolder()).getFullPathName() + "\n\n" + report);
            if (! files.isEmpty()) files[0].getParentDirectory().revealToUser();
        }), false);
    }

    void publishImage()
    {
        if (project.numTracks() == 0) { msg ("Image", "Add tracks first."); return; }
        juce::String report;
        auto folder = project.defaultReleaseFolder().getChildFile ("Image");
        if (project.publishImage (folder, report)) { msg ("Image", "Written to " + folder.getFullPathName() + "\n" + report); folder.revealToUser(); }
        else msg ("Image", "Failed:\n" + report);
    }

    void publishDDP()
    {
        if (project.numTracks() == 0) { msg ("DDP", "Add tracks first."); return; }
        juce::String report;
        auto folder = project.defaultReleaseFolder().getChildFile (project.projectName() + " DDP");
        if (project.publishDDP (folder, report)) { msg ("DDP export", report + "\nCheck it in a DDP player before sending it to the plant."); folder.revealToUser(); }
        else msg ("DDP export", "Not written:\n" + report);
    }

    void msg (const juce::String& title, const juce::String& text)   { Ui::message (juce::MessageBoxIconType::InfoIcon, title, text); }

    //==========================================================================
    void refreshAll()
    {
        albumField.setText (project.album ("mAlbum", project.isOpen() ? project.projectName() : "Album title"), juce::dontSendNotification);
        artistField.setText (project.album ("mAlbumArtist", "Album artist"), juce::dontSendNotification);
        trackList.updateContent();
        trackList.repaint();
        masterRack.rebuild();
        if (selectedRow < 0 && project.numTracks() > 0) { trackList.selectRow (0); selectTrack (0); }
        else trackRack.setTrack (project.track (selectedRow));
        lane.repaint();
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override   { refreshAll(); }

    void timerCallback() override
    {
        project.hub.update();
        meters.repaint();
        playBtn.setButtonText (project.isPlaying() ? "STOP" : "PLAY");
        auto pos = project.position();
        auto t = project.track (selectedRow);
        juce::String tt = MasteringProject::timeText (pos).upToFirstOccurrenceOf (".", false, false);
        if (t != nullptr && pos >= project.startOf (*t) && pos <= project.endOf (*t)) tt << "   trk " << MasteringProject::timeText (pos - project.startOf (*t)).upToFirstOccurrenceOf (".", false, false);
        timeLabel.setText (tt, juce::dontSendNotification);
        if (++refreshTick % 10 == 0) { masterRack.refresh(); trackRack.refresh(); }
    }

    MasteringProject& project;
    TrackLane lane;
    MeterPanel meters;
    RackStrip masterRack, trackRack;
    juce::ListBox trackList;
    juce::TextButton newBtn { "New" }, openBtn { "Open..." }, saveBtn { "Save" }, addFilesBtn { "Add Files..." }, addSessionBtn { "Add Current Session" }, updateBtn { "Update from Session" },
                     removeBtn { "Remove" }, upBtn { "Up" }, downBtn { "Down" }, loudBtn { "Detect Loudness" }, metaBtn { "Track Info..." }, albumBtn { "Album Info..." },
                     playBtn { "PLAY" }, rtzBtn { "|<" }, digitalBtn { "Digital Release..." }, imageBtn { "Image..." }, ddpBtn { "DDP..." }, burnBtn { "Burn..." };
    juce::Label albumField, artistField, timeLabel;
    juce::ComboBox targetBox;
    int selectedRow = -1, refreshTick = 0;
};
