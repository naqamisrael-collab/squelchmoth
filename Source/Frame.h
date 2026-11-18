#pragma once
// Squelchmoth 1.21 "Frame", stage 0 - video in and out of the session through FFmpeg run as a separate program.
//
// What this stage does: reads a video's facts, takes its sound onto a track, lets the separator and the Vocal Remover
// accept video files, and exports the session's mix as a video (the original picture, a still image, a moving
// waveform or spectrum, or black). FFmpeg is not part of the app: it is found on the computer (or in the Frame
// folder) and run as its own process, streaming, so the length of the video does not change the memory used.
// Not in this stage: a picture viewer on the timeline, picture editing, upload.

#include <JuceHeader.h>
#include "SqStrip.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace Frame
{
    inline const char* videoExtensions = "mp4;mov;m4v;mkv;webm;avi;mpg;mpeg;wmv;flv";
    inline bool isVideo (const juce::File& f) { return f.hasFileExtension (videoExtensions); }

    inline juce::File folder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Squelchmoth").getChildFile ("Frame");
    }

    /** The FFmpeg program: SQUELCH_FFMPEG, then the Frame folder, then the PATH and the usual install places. */
    inline juce::File ffmpeg()
    {
       #if JUCE_WINDOWS
        const juce::String exe = "ffmpeg.exe"; const char sep = ';';
       #else
        const juce::String exe = "ffmpeg"; const char sep = ':';
       #endif
        const auto env = juce::SystemStats::getEnvironmentVariable ("SQUELCH_FFMPEG", {});
        const bool ownOnly = env == "none";      // SQUELCH_FFMPEG=none: only the one Squelchmoth downloaded (used by tests)
        if (env.isNotEmpty() && ! ownOnly) return juce::File (env).existsAsFile() ? juce::File (env) : juce::File();
        juce::StringArray dirs; dirs.add (folder().getFullPathName());
        if (! ownOnly)
        {
            dirs.addTokens (juce::SystemStats::getEnvironmentVariable ("PATH", {}), juce::String::charToString (sep), "");
           #if ! JUCE_WINDOWS
            dirs.add ("/opt/homebrew/bin"); dirs.add ("/usr/local/bin"); dirs.add ("/opt/local/bin"); dirs.add ("/usr/bin");
           #endif
        }
        for (auto& d : dirs)
        {
            if (d.trim().isEmpty() || ! juce::File::isAbsolutePath (d.trim())) continue;
            const auto f = juce::File (d.trim()).getChildFile (exe);
            if (f.existsAsFile()) return f;
        }
        return {};
    }

    inline juce::String missingText()
    {
        return "Video needs FFmpeg, and it is not on this computer yet. video_export action install downloads it once (about 80 MB) into " + folder().getFullPathName()
               + "; Squelchmoth runs it as a separate program. An FFmpeg already installed on the computer is used when there is one.";
    }

    /** Runs a program to the end. out keeps the last 16k characters of its output; onText (optional) sees every chunk. */
    inline int run (const juce::StringArray& args, juce::String& out, const std::function<void (const juce::String&)>& onText = {})
    {
        juce::ChildProcess p;
        if (! p.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr)) { out = "Couldn't start " + args[0]; return -1; }
        char buf[4096];
        for (;;)
        {
            const int n = p.readProcessOutput (buf, (int) sizeof (buf));
            if (n > 0)
            {
                const auto s = juce::String::fromUTF8 (buf, n);
                out << s; if (out.length() > 16000) out = out.substring (out.length() - 12000);
                if (onText) onText (s);
            }
            else if (! p.isRunning()) break;
            else juce::Thread::sleep (20);
        }
        p.waitForProcessToFinish (5000);
        return (int) p.getExitCode();
    }

    /** Downloads url to dest. Resumes where it stopped when the connection drops (HTTP range requests) and gives up only
        after six tries in a row bring nothing new. onProgress gets (bytes so far, total bytes or 0). */
    inline bool downloadTo (const juce::String& url, const juce::File& dest, const std::function<void (double, double)>& onProgress, juce::String& err)
    {
        dest.getParentDirectory().createDirectory(); dest.deleteFile();
        juce::int64 got = 0, total = 0; int stalls = 0; bool everConnected = false;
        for (int attempt = 0; attempt < 60; ++attempt)
        {
            int status = 0; const juce::int64 before = got; bool ended = false;
            const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (15000).withNumRedirectsToFollow (8).withStatusCode (&status)
                                  .withExtraHeaders (got > 0 ? "Range: bytes=" + juce::String (got) + "-" : juce::String());
            if (auto in = juce::URL (url).createInputStream (opts))
            {
                if (status == 200 || status == 206 || status == 0)
                {
                    everConnected = true;
                    if (status != 206 && got > 0) { got = 0; dest.deleteFile(); }          // the server sent the whole file again
                    const juce::int64 len = in->getTotalLength(); if (len > 0) total = got + len;
                    juce::FileOutputStream out (dest);                                      // opens at the end: appends
                    if (! out.openedOk()) { err = "Couldn't write " + dest.getFullPathName(); return false; }
                    juce::HeapBlock<char> buf (1 << 17);
                    while (! in->isExhausted())
                    {
                        const int n = in->read (buf, 1 << 17); if (n <= 0) break;
                        if (! out.write (buf, (size_t) n)) { err = "The disk is full or not writable"; return false; }
                        got += n; if (onProgress) onProgress ((double) got, (double) total);
                    }
                    out.flush(); ended = true;
                }
            }
            if (total > 0 && got >= total) return true;
            if (total <= 0 && ended && got > before) return true;                           // no length given: the stream ended
            if (status >= 400 && status < 500 && status != 408 && status != 429)            // not there: trying again will not help
            { err = "The download is not available from its source right now (answer " + juce::String (status) + ")."; return false; }
            if (got > before) stalls = 0; else if (++stalls >= 6) break;
            juce::Thread::sleep (juce::jmin (6000, 700 * (stalls + 1)));
        }
        err = everConnected ? "The download stopped at " + juce::String (got / 1048576) + " MB" + (total > 0 ? " of " + juce::String (total / 1048576) + " MB" : juce::String()) + " and could not be resumed. Check the internet connection and try again."
                            : juce::String ("Couldn't connect to download. Check the internet connection and try again.");
        return false;
    }

   #if JUCE_MAC
    /** macOS reads the sound of MP4 / M4V (and many MOV) files itself: no FFmpeg needed for those. */
    inline bool nativeAudio (const juce::File& video, const juce::File& destWav)
    {
        juce::CoreAudioFormat caf;
        std::unique_ptr<juce::AudioFormatReader> r (caf.createReaderFor (new juce::FileInputStream (video), true));
        if (r == nullptr || r->lengthInSamples <= 0 || r->numChannels == 0) return false;
        destWav.getParentDirectory().createDirectory(); destWav.deleteFile();
        juce::WavAudioFormat wav; auto os = std::make_unique<juce::FileOutputStream> (destWav);
        if (! os->openedOk()) return false;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), r->sampleRate, juce::jmin (2u, r->numChannels), 24, {}, 0));
        if (w == nullptr) return false;
        os.release();
        const bool ok = w->writeFromAudioReader (*r, 0, r->lengthInSamples);
        w.reset();
        if (! ok || destWav.getSize() < 1024) { destWav.deleteFile(); return false; }
        return true;
    }
   #endif

    //==============================================================================
    // The one-time download: a ready-made FFmpeg 6.1.1 program for this computer, checked against its SHA-256 before use.
    // It is a GPL build made by FFmpeg's own community builders; Squelchmoth does not contain it, it downloads it at the
    // user's request and runs it as a separate program.
    struct PackItem { const char* url; const char* sha256; int megabytes; };
    inline PackItem packItem()
    {
       #if JUCE_WINDOWS
        return { "https://github.com/eugeneware/ffmpeg-static/releases/download/b6.1.1/ffmpeg-win32-x64", "04e1307997530f9cf2fe35cba2ca7e8875ca91da02f89d6c7243df819c94ad00", 79 };
       #elif JUCE_MAC && JUCE_ARM
        return { "https://github.com/eugeneware/ffmpeg-static/releases/download/b6.1.1/ffmpeg-darwin-arm64", "a90e3db6a3fd35f6074b013f948b1aa45b31c6375489d39e572bea3f18336584", 44 };   // needs macOS 12
       #elif JUCE_MAC
        return { "https://github.com/eugeneware/ffmpeg-static/releases/download/b6.1.1/ffmpeg-darwin-x64", "ebdddc936f61e14049a2d4b549a412b8a40deeff6540e58a9f2a2da9e6b18894", 76 };     // macOS 10.13 and later
       #else
        return { "https://github.com/eugeneware/ffmpeg-static/releases/download/b6.1.1/ffmpeg-linux-x64", "e7e7fb30477f717e6f55f9180a70386c62677ef8a4d4d1a5d948f4098aa3eb99", 77 };
       #endif
    }

    struct Install { std::atomic<float> progress { 0.0f }; std::atomic<int> megabytes { 0 }; std::atomic<bool> running { false }, finished { false }; juce::String error; juce::WaitableEvent done { true }; };
    inline Install& installState() { static Install s; return s; }

    inline void startInstall()
    {
        auto& st = installState();
        if (st.running.exchange (true)) return;
        st.finished = false; st.error = {}; st.progress = 0; st.done.reset();
        juce::Thread::launch ([]
        {
            auto& s = installState(); juce::String err; const auto it = packItem();
           #if JUCE_WINDOWS
            const auto dest = folder().getChildFile ("ffmpeg.exe");
           #else
            const auto dest = folder().getChildFile ("ffmpeg");
           #endif
            folder().createDirectory();
            const auto tmp = folder().getChildFile ("ffmpeg.download"); tmp.deleteFile();
            downloadTo (it.url, tmp, [&s, it] (double got, double total) { s.megabytes = (int) (got / 1048576.0); s.progress = 0.97f * (float) juce::jmin (1.0, got / (total > 0 ? total : it.megabytes * 1048576.0)); }, err);
            if (err.isEmpty() && ! juce::SHA256 (tmp).toHexString().equalsIgnoreCase (it.sha256)) err = "The video tools download did not match its checksum (try again)";
            if (err.isEmpty()) { dest.deleteFile(); if (! tmp.moveFileTo (dest)) err = "Couldn't put the video tools in " + folder().getFullPathName(); else dest.setExecutePermission (true); }
            if (err.isEmpty())
            {
                juce::String out; const int code = run ({ dest.getFullPathName(), "-hide_banner", "-version" }, out);
                if (code != 0 || ! out.contains ("ffmpeg version")) { err = "The video tools were downloaded but do not run on this computer" + juce::String (
                   #if JUCE_MAC && JUCE_ARM
                    " (they need macOS 12 or later)"
                   #else
                    ""
                   #endif
                    ); dest.deleteFile(); }
            }
            tmp.deleteFile();
            s.error = err; s.progress = 1.0f; s.finished = true; s.running = false; s.done.signal();
        });
    }

    struct Info
    {
        bool ok = false, hasVideo = false, hasAudio = false;
        double seconds = 0, fps = 0; int width = 0, height = 0;
        juce::String videoCodec, audioCodec, error;
        juce::String text() const
        {
            juce::String s = juce::RelativeTime::seconds (seconds).getDescription();
            if (hasVideo) s << ", " << width << " x " << height << (fps > 0 ? ", " + juce::String (fps, fps == std::floor (fps) ? 0 : 2) + " fps" : juce::String()) << ", " << videoCodec;
            s << (hasAudio ? ", sound: " + audioCodec : juce::String (", no sound"));
            return s;
        }
    };

    /** Parses what `ffmpeg -i file` prints (no ffprobe needed). */
    inline Info parseInfo (const juce::String& text)
    {
        Info in;
        const int d = text.indexOf ("Duration: ");
        if (d >= 0)
        {
            const auto t = text.substring (d + 10, d + 30).upToFirstOccurrenceOf (",", false, false).trim();
            juce::StringArray p; p.addTokens (t, ":", "");
            if (p.size() == 3) in.seconds = p[0].getDoubleValue() * 3600.0 + p[1].getDoubleValue() * 60.0 + p[2].getDoubleValue();
        }
        for (auto& line : juce::StringArray::fromLines (text))
        {
            if (! line.contains ("Stream #")) continue;
            if (line.contains ("Video:") && ! in.hasVideo && ! line.contains ("attached pic"))
            {
                in.hasVideo = true;
                const auto v = line.fromFirstOccurrenceOf ("Video:", false, false).trim();
                in.videoCodec = v.upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf (",", false, false);
                juce::StringArray parts; parts.addTokens (v, ",", "");
                for (auto part : parts)
                {
                    part = part.trim();
                    const auto first = part.upToFirstOccurrenceOf (" ", false, false);
                    const int x = first.indexOfChar ('x');
                    if (in.width == 0 && x > 0 && first.substring (0, x).containsOnly ("0123456789") && first.substring (x + 1).containsOnly ("0123456789") && first.substring (x + 1).isNotEmpty())
                    { in.width = first.substring (0, x).getIntValue(); in.height = first.substring (x + 1).getIntValue(); }
                    if (part.endsWith (" fps")) in.fps = part.upToFirstOccurrenceOf (" ", false, false).getDoubleValue();
                }
            }
            else if (line.contains ("Audio:") && ! in.hasAudio)
            {
                in.hasAudio = true;
                in.audioCodec = line.fromFirstOccurrenceOf ("Audio:", false, false).trim().upToFirstOccurrenceOf (",", false, false).upToFirstOccurrenceOf (" ", false, false);
            }
        }
        in.ok = d >= 0 && (in.hasVideo || in.hasAudio);
        return in;
    }

    inline Info probe (const juce::File& f)
    {
        Info in; const auto ff = ffmpeg();
        if (! ff.existsAsFile()) { in.error = missingText(); return in; }
        juce::String out; run ({ ff.getFullPathName(), "-hide_banner", "-i", f.getFullPathName() }, out);
        in = parseInfo (out);
        if (! in.ok) in.error = "FFmpeg could not read " + f.getFileName();
        return in;
    }

    /** The sound of a video as a 24-bit stereo WAV at sampleRate (RF64 when it passes 4 GB). */
    inline bool extractAudio (const juce::File& video, const juce::File& destWav, double sampleRate, juce::String& err)
    {
        const auto ff = ffmpeg(); if (! ff.existsAsFile()) { err = missingText(); return false; }
        destWav.getParentDirectory().createDirectory(); destWav.deleteFile();
        juce::String out;
        const int code = run ({ ff.getFullPathName(), "-y", "-hide_banner", "-loglevel", "error", "-i", video.getFullPathName(), "-vn", "-map", "0:a:0", "-ac", "2",
                                "-ar", juce::String (juce::roundToInt (sampleRate)), "-c:a", "pcm_s24le", "-rf64", "auto", destWav.getFullPathName() }, out);
        if (code != 0 || destWav.getSize() < 1024)
        {
            destWav.deleteFile();
            err = out.containsIgnoreCase ("matches no streams") ? video.getFileName() + " has no sound" : "Couldn't read the sound of " + video.getFileName() + (out.trim().isNotEmpty() ? ": " + out.trim().substring (0, 300) : juce::String());
            return false;
        }
        return true;
    }

    /** For tools that take a song: an audio file comes back as it is; a video comes back as its sound (kept in folder). */
    inline juce::File audioOf (const juce::File& f, const juce::File& intoFolder, double sampleRate, juce::String& err)
    {
        if (! isVideo (f)) return f;
        const auto wav = intoFolder.getChildFile (juce::File::createLegalFileName (f.getFileNameWithoutExtension()).substring (0, 60) + " (sound).wav");
        if (wav.existsAsFile() && wav.getLastModificationTime() >= f.getLastModificationTime() && wav.getSize() > 1024) return wav;
       #if JUCE_MAC
        if (nativeAudio (f, wav)) return wav;          // no FFmpeg needed
       #endif
        return extractAudio (f, wav, sampleRate, err) ? wav : juce::File();
    }

    /** The H.264 encoder this FFmpeg has (builds differ), or MPEG-4 when it has none. */
    inline juce::String videoEncoder()
    {
        static juce::String cached;
        if (cached.isNotEmpty()) return cached;
        const auto ff = ffmpeg(); if (! ff.existsAsFile()) return "libx264";
        juce::String out; run ({ ff.getFullPathName(), "-hide_banner", "-encoders" }, out);
        // the list is long: run() keeps the tail, so ask again per name when it is not in what was kept
        for (auto* name : { "libx264", "h264_videotoolbox", "h264_mf", "libopenh264" })
        {
            juce::String one; const int code = run ({ ff.getFullPathName(), "-hide_banner", "-h", juce::String ("encoder=") + name }, one);
            if (code == 0 && one.contains ("Encoder " + juce::String (name))) { cached = name; return cached; }
        }
        cached = "mpeg4"; return cached;
    }

    struct ExportOptions
    {
        juce::File audio, out, sourceVideo, image;
        juce::String picture = "waveform";          // source | image | waveform | spectrum | black
        int width = 1920, height = 1080; double fps = 30.0;
        double videoOffset = 0;                      // source picture: where the range starts inside the video (negative: the video starts later)
        bool resize = false;                         // source picture: re-encode to width x height
        double seconds = 0;                          // length of the audio, for progress
    };

    inline juce::StringArray exportArgs (const ExportOptions& o, const juce::String& enc)
    {
        const auto ff = ffmpeg();
        const auto size = juce::String (o.width) + "x" + juce::String (o.height);
        const auto rate = juce::String (o.fps, 3);
        juce::StringArray a { ff.getFullPathName(), "-y", "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1" };
        juce::StringArray venc { "-c:v", enc };
        if (enc == "libx264") venc.addArray ({ "-preset", "medium", "-crf", "18" });
        else if (enc == "mpeg4") venc.addArray ({ "-q:v", "3" });
        else venc.addArray ({ "-b:v", juce::String (juce::jmax (2, o.width * o.height / 260000)) + "M" });
        venc.addArray ({ "-pix_fmt", "yuv420p" });
        const auto fit = "scale=" + juce::String (o.width) + ":" + juce::String (o.height) + ":force_original_aspect_ratio=decrease,pad=" + juce::String (o.width) + ":" + juce::String (o.height) + ":(ow-iw)/2:(oh-ih)/2,setsar=1";
        if (o.picture == "source")
        {
            const bool copy = ! o.resize && std::abs (o.videoOffset) < 0.0005;
            if (o.videoOffset > 0.0005) a.addArray ({ "-ss", juce::String (o.videoOffset, 3) });
            a.addArray ({ "-i", o.sourceVideo.getFullPathName(), "-i", o.audio.getFullPathName(), "-map", "0:v:0", "-map", "1:a:0" });
            if (copy) a.addArray ({ "-c:v", "copy" });
            else
            {
                juce::String vf = o.videoOffset < -0.0005 ? "tpad=start_duration=" + juce::String (-o.videoOffset, 3) : juce::String();
                if (o.resize) vf << (vf.isNotEmpty() ? "," : "") << fit;
                if (vf.isNotEmpty()) a.addArray ({ "-vf", vf });
                a.addArray (venc);
            }
        }
        else if (o.picture == "image")
        {
            a.addArray ({ "-loop", "1", "-framerate", rate, "-i", o.image.getFullPathName(), "-i", o.audio.getFullPathName(), "-map", "0:v:0", "-map", "1:a:0", "-vf", fit });
            a.addArray (venc); if (enc == "libx264") a.addArray ({ "-tune", "stillimage" });
        }
        else if (o.picture == "black")
        {
            a.addArray ({ "-f", "lavfi", "-i", "color=c=black:s=" + size + ":r=" + rate, "-i", o.audio.getFullPathName(), "-map", "0:v:0", "-map", "1:a:0" });
            a.addArray (venc);
        }
        else
        {
            const auto filter = o.picture == "spectrum" ? "[0:a]showspectrum=s=" + size + ":slide=scroll:color=fiery:scale=log:legend=0,fps=" + rate + ",format=yuv420p[v]"
                                                        : "[0:a]showwaves=s=" + size + ":mode=cline:rate=" + rate + ":colors=0xE8C46A|0xF2E3B5,format=yuv420p[v]";
            a.addArray ({ "-i", o.audio.getFullPathName(), "-filter_complex", filter, "-map", "[v]", "-map", "0:a:0" });
            a.addArray (venc);
        }
        a.addArray ({ "-c:a", "aac", "-b:a", "320k", "-shortest", "-movflags", "+faststart", o.out.getFullPathName() });
        return a;
    }

    //==============================================================================
    struct Job
    {
        int id = 0; ExportOptions options; juce::String encoder;
        std::atomic<float> progress { 0.0f }; std::atomic<bool> finished { false };
        juce::String error; double tookSeconds = 0; bool deleteAudioAfter = false;
        juce::WaitableEvent done { true };
    };

    class Store
    {
    public:
        static Store& get() { static Store s; return s; }

        std::shared_ptr<Job> start (const ExportOptions& o, bool deleteAudioAfter)
        {
            auto j = std::make_shared<Job>(); j->options = o; j->deleteAudioAfter = deleteAudioAfter;
            { const juce::ScopedLock sl (lock); j->id = ++lastId; jobs.push_back (j); if (jobs.size() > 16) jobs.erase (jobs.begin()); }
            juce::Thread::launch ([j]
            {
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                j->encoder = videoEncoder();
                j->options.out.getParentDirectory().createDirectory();
                juce::String out, carry;
                const int code = run (exportArgs (j->options, j->encoder), out, [&] (const juce::String& s)
                {
                    carry << s;
                    for (;;)
                    {
                        const int nl = carry.indexOfChar ('\n'); if (nl < 0) break;
                        const auto line = carry.substring (0, nl).trim(); carry = carry.substring (nl + 1);
                        if ((line.startsWith ("out_time_us=") || line.startsWith ("out_time_ms=")) && j->options.seconds > 0)
                            j->progress = (float) juce::jlimit (0.0, 0.999, line.fromFirstOccurrenceOf ("=", false, false).getDoubleValue() / 1.0e6 / j->options.seconds);
                    }
                });
                if (code != 0 || j->options.out.getSize() < 1024)
                {
                    juce::StringArray errs;
                    for (auto& l : juce::StringArray::fromLines (out)) if (l.trim().isNotEmpty() && ! l.containsChar ('=')) errs.add (l.trim());
                    j->error = "The video export failed" + (errs.isEmpty() ? juce::String() : ": " + errs.joinIntoString (" ").substring (0, 400));
                    j->options.out.deleteFile();
                }
                if (j->deleteAudioAfter) j->options.audio.deleteFile();
                j->tookSeconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
                j->progress = 1.0f; j->finished = true; j->done.signal();
            });
            return j;
        }

        std::shared_ptr<Job> find (int id)
        {
            const juce::ScopedLock sl (lock);
            if (jobs.empty()) return {};
            if (id <= 0) return jobs.back();
            for (auto& j : jobs) if (j->id == id) return j;
            return {};
        }

    private:
        juce::CriticalSection lock; std::vector<std::shared_ptr<Job>> jobs; int lastId = 0;
    };

    //============================================================================== the picture
    /** Runs a program and collects its standard output as bytes (the errors are not mixed in). */
    inline bool runBinary (const juce::StringArray& args, juce::MemoryBlock& out, size_t maxBytes)
    {
        juce::ChildProcess p; out.reset();
        if (! p.start (args, juce::ChildProcess::wantStdOut)) return false;
        juce::HeapBlock<char> buf (1 << 16);
        for (;;)
        {
            const int n = p.readProcessOutput (buf, 1 << 16);
            if (n > 0) { out.append (buf, (size_t) n); if (out.getSize() >= maxBytes) { p.kill(); break; } }
            else if (! p.isRunning()) break;
            else juce::Thread::sleep (5);
        }
        return out.getSize() > 0;
    }

    /** The size a picture is shown at: no wider than maxWidth, same shape, even numbers. */
    inline void fitSize (const Info& in, int maxWidth, int& w, int& h)
    {
        w = juce::jmax (16, juce::jmin (maxWidth, in.width)) & ~1;
        h = juce::jmax (16, juce::roundToInt ((double) w * in.height / juce::jmax (1, in.width))) & ~1;
    }

    inline juce::Image imageFromRgb (const juce::uint8* rgb, int w, int h)
    {
        juce::Image img (juce::Image::RGB, w, h, false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
        {
            const juce::uint8* src = rgb + (size_t) y * (size_t) w * 3;
            // macOS keeps an "RGB" picture with four bytes a pixel, so write in whatever layout the picture really has
            if (bd.pixelFormat == juce::Image::RGB) for (int x = 0; x < w; ++x, src += 3) ((juce::PixelRGB*) bd.getPixelPointer (x, y))->setARGB (255, src[0], src[1], src[2]);
            else if (bd.pixelFormat == juce::Image::ARGB) for (int x = 0; x < w; ++x, src += 3) ((juce::PixelARGB*) bd.getPixelPointer (x, y))->setARGB (255, src[0], src[1], src[2]);
            else for (int x = 0; x < w; ++x, src += 3) bd.setPixelColour (x, y, juce::Colour (src[0], src[1], src[2]));
        }
        return img;
    }

    /** Which frame is on screen at a time: frame k shows from k / fps up to (k + 1) / fps. */
    inline juce::int64 frameIndexAt (double seconds, double fps) { return (juce::int64) std::floor (juce::jmax (0.0, seconds) * fps + 1.0e-6); }

    /** One frame of a video, exactly the one on screen at that time. */
    inline juce::Image grabFrame (const juce::File& video, const Info& in, double seconds, int maxWidth, juce::String& err)
    {
        const auto ff = ffmpeg(); if (! ff.existsAsFile()) { err = missingText(); return {}; }
        if (! in.hasVideo || in.fps <= 0 || in.width <= 0) { err = video.getFileName() + " has no picture"; return {}; }
        int w = 0, h = 0; fitSize (in, maxWidth, w, h);
        const double at = juce::jmax (0.0, ((double) frameIndexAt (seconds, in.fps) - 0.25) / in.fps);
        juce::MemoryBlock raw; const size_t need = (size_t) w * (size_t) h * 3;
        runBinary ({ ff.getFullPathName(), "-hide_banner", "-loglevel", "error", "-ss", juce::String (at, 6), "-i", video.getFullPathName(), "-frames:v", "1",
                     "-vf", "scale=" + juce::String (w) + ":" + juce::String (h), "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1" }, raw, need);
        if (raw.getSize() < need) { err = seconds >= in.seconds ? "That is past the end of the video" : "Couldn't read a frame of " + video.getFileName(); return {}; }
        return imageFromRgb ((const juce::uint8*) raw.getData(), w, h);
    }

    /** Plays a video's picture against a clock it is told about. While the clock runs, frames are decoded ahead in a
        background thread; when it stops or jumps, the exact frame for that moment is fetched. Constant memory: a few
        frames are held at a time, whatever the length of the video. */
    class Player : private juce::Thread
    {
    public:
        Player() : juce::Thread ("Squelchmoth video") {}
        ~Player() override { stopStream(); }

        void setSource (const juce::File& f, int maxWidth = 960)
        {
            if (f == file && maxWidth == width) return;
            stopStream(); file = f; width = maxWidth; info = f.existsAsFile() ? probe (f) : Info();
            const juce::ScopedLock sl (lock); current = {}; currentIndex = -1; stillWanted = -1;
        }
        const juce::File& source() const { return file; }
        const Info& facts() const { return info; }
        bool ready() const { return info.ok && info.hasVideo && info.fps > 0; }

        /** Call from a timer. seconds: position inside the video; playing: the clock is running. Returns the frame to show
            (possibly the previous one while the right one is on its way) and its frame number in shownIndex. */
        juce::Image frameFor (double seconds, bool playing, juce::int64* shownIndex = nullptr)
        {
            if (! ready() || seconds < 0 || seconds >= info.seconds + 1.0 / info.fps) { if (shownIndex) *shownIndex = -1; return {}; }
            const auto want = frameIndexAt (seconds, info.fps);
            {
                const juce::ScopedLock sl (lock);
                if (playing)
                {
                    // drop what is already past; restart the decoder when the clock is outside what it is producing
                    while (queue.size() > 1 && queue[1].first <= want) queue.erase (queue.begin());
                    const bool outside = (! streaming.load() && ! ended.load()) || want < streamFirst || want > streamNext + (juce::int64) (info.fps * 0.75);
                    if (! queue.empty() && queue.front().first <= want) { current = queue.front().second; currentIndex = queue.front().first; }
                    if (outside) restartAt = want;
                }
                else restartAt = -1;
                wantPlaying = playing;
                if (! playing && want != currentIndex) stillWanted = want;
                if (shownIndex) *shownIndex = currentIndex;
            }
            if (! isThreadRunning()) startThread();
            notify();
            const juce::ScopedLock sl (lock);
            return current;
        }

        void stopStream() { signalThreadShouldExit(); notify(); { const juce::ScopedLock sl (procLock); if (proc) proc->kill(); } stopThread (4000); proc.reset(); streaming = false; }

    private:
        void run() override
        {
            int w = 0, h = 0; fitSize (info, width, w, h);
            const size_t frameBytes = (size_t) w * (size_t) h * 3;
            juce::MemoryBlock acc; juce::HeapBlock<char> buf (1 << 16);
            while (! threadShouldExit())
            {
                juce::int64 startAt = -1, still = -1; bool play = false;
                { const juce::ScopedLock sl (lock); startAt = restartAt; restartAt = -1; still = stillWanted; play = wantPlaying; }

                if (! play)
                {
                    if (streaming.load()) { const juce::ScopedLock sl (procLock); if (proc) proc->kill(); proc.reset(); streaming = false; const juce::ScopedLock s2 (lock); queue.clear(); }
                    if (still >= 0)
                    {
                        juce::String err; auto img = grabFrame (file, info, ((double) still + 0.5) / info.fps, width, err);
                        const juce::ScopedLock sl (lock);
                        if (img.isValid()) { current = img; currentIndex = still; }
                        if (stillWanted == still) stillWanted = -1;
                        continue;
                    }
                    wait (40); continue;
                }

                if (startAt >= 0 || (! streaming.load() && ! ended.load()))
                {
                    if (startAt < 0) { wait (10); continue; }
                    const auto ff = ffmpeg(); if (! ff.existsAsFile()) { wait (200); continue; }
                    { const juce::ScopedLock sl (procLock); if (proc) proc->kill(); proc = std::make_unique<juce::ChildProcess>(); }
                    { const juce::ScopedLock sl (lock); queue.clear(); streamFirst = streamNext = startAt; }
                    acc.reset(); ended = false;
                    const double at = juce::jmax (0.0, ((double) startAt - 0.25) / info.fps);
                    const bool ok = proc->start ({ ff.getFullPathName(), "-hide_banner", "-loglevel", "error", "-ss", juce::String (at, 6), "-i", file.getFullPathName(),
                                                    "-vf", "scale=" + juce::String (w) + ":" + juce::String (h), "-r", juce::String (info.fps, 4), "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1" }, juce::ChildProcess::wantStdOut);
                    streaming = ok; if (! ok) { wait (200); continue; }
                }

                // keep about half a second decoded ahead; the decoder waits on its pipe while we hold back
                bool full = false;
                { const juce::ScopedLock sl (lock); full = (int) queue.size() >= juce::jmax (6, (int) (info.fps * 0.5)); }
                if (full) { wait (8); continue; }
                if (! streaming.load()) { wait (20); continue; }
                const int n = proc->readProcessOutput (buf, 1 << 16);
                if (n > 0)
                {
                    acc.append (buf, (size_t) n);
                    while (acc.getSize() >= frameBytes)
                    {
                        auto img = imageFromRgb ((const juce::uint8*) acc.getData(), w, h);
                        acc.removeSection (0, frameBytes);
                        const juce::ScopedLock sl (lock); queue.push_back ({ streamNext++, img });
                    }
                }
                else if (! proc->isRunning()) { streaming = false; ended = true; wait (30); }      // the end of the video
                else wait (4);
            }
            { const juce::ScopedLock sl (procLock); if (proc) proc->kill(); }
            streaming = false;
        }

        juce::File file; Info info; int width = 960;
        juce::CriticalSection lock, procLock;
        std::vector<std::pair<juce::int64, juce::Image>> queue;
        juce::Image current; juce::int64 currentIndex = -1, stillWanted = -1, restartAt = -1, streamFirst = 0, streamNext = 0;
        bool wantPlaying = false; std::atomic<bool> streaming { false }, ended { false };
        std::unique_ptr<juce::ChildProcess> proc;
    };
    /** The thumbnail strip of a video: count stills, each the frame at the middle of its slot, fetched one by one in the
        background (coarse first), so the cost does not grow with the video's length. */
    class Strip : private juce::Thread
    {
    public:
        Strip() : juce::Thread ("Squelchmoth video strip") {}
        ~Strip() override { stopThread (6000); }

        void setSource (const juce::File& f, const Info& in, int count, int thumbWidth = 96)
        {
            if (f == file && count == (int) thumbs.size() && thumbWidth == width) return;
            stopThread (6000);
            file = f; info = in; width = thumbWidth;
            { const juce::ScopedLock sl (lock); thumbs.assign ((size_t) juce::jmax (0, count), juce::Image()); made = 0; }
            if (count > 0 && in.ok && in.hasVideo && in.fps > 0 && f.existsAsFile()) startThread();
        }
        /** While the session plays, fetching stills waits: the picture on the playhead comes first. */
        void setPaused (bool p)     { if (paused.exchange (p) != p) notify(); }
        int size() const            { const juce::ScopedLock sl (lock); return (int) thumbs.size(); }
        int done() const            { return made.load(); }
        bool finished() const       { return ! isThreadRunning(); }
        juce::Image thumb (int k) const { const juce::ScopedLock sl (lock); return k >= 0 && k < (int) thumbs.size() ? thumbs[(size_t) k] : juce::Image(); }

    private:
        void run() override
        {
            const int count = size();
            for (int k : SqStrip::fillOrder (count))
            {
                while (paused.load() && ! threadShouldExit()) wait (100);
                if (threadShouldExit()) return;
                if (! ffmpeg().existsAsFile()) { wait (300); continue; }
                juce::String err; auto img = grabFrame (file, info, SqStrip::slotTime (k, count, info.seconds), width, err);
                const juce::ScopedLock sl (lock);
                if (k < (int) thumbs.size()) thumbs[(size_t) k] = img;
                ++made;
            }
        }
        juce::File file; Info info; int width = 96;
        juce::CriticalSection lock; std::vector<juce::Image> thumbs; std::atomic<int> made { 0 }; std::atomic<bool> paused { false };
    };

    /** A contact sheet of a video: count stills evenly through it, each with its timecode underneath. */
    inline juce::Image contactSheet (const juce::File& video, const Info& in, int count, int cols, int cellW, std::function<juce::String (double)> label, std::vector<double>* times, juce::String& err)
    {
        const auto sh = SqStrip::sheet (count, cols, cellW, in.width, in.height);
        juce::Image out (juce::Image::RGB, sh.width, sh.height, true);
        juce::Graphics g (out); g.fillAll (juce::Colour (0xff101216));
        for (int k = 0; k < sh.count; ++k)
        {
            const double t = SqStrip::slotTime (k, sh.count, in.seconds);
            auto img = grabFrame (video, in, t, sh.cellW, err);
            if (! img.isValid()) return {};
            g.drawImage (img, juce::Rectangle<float> ((float) sh.cellX (k), (float) sh.cellY (k), (float) sh.cellW, (float) sh.imageH), juce::RectanglePlacement::stretchToFit);
            g.setColour (juce::Colour (0xffd8dde6)); g.setFont (juce::FontOptions (12.0f));
            g.drawText (label ? label (t) : juce::String (t, 2) + " s", sh.cellX (k), sh.cellY (k) + sh.imageH, sh.cellW, sh.labelH, juce::Justification::centred);
            if (times) times->push_back (t);
        }
        return out;
    }
}
