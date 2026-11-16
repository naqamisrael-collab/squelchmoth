#pragma once
// Squelchmoth 1.16 "Prism Split" in the app: background separation jobs (file range -> stem WAVs), streamed in chunks so
// length never matters. Draft quality today (SqSplitDsp.h); the neural Studio / Master stages plug in here.

#include "SqSplitDsp.h"
#include "SqNeuralSplit.h"
#include <memory>
#include <mutex>
#include <atomic>

namespace PrismSplit
{
    //==========================================================================
    /** Studio quality = the separation runtime + one model file, installed on request into the user's data folder
        (not part of the installer). SQUELCH_STUDIO_PACK points somewhere else (tests). */
    namespace StudioPack
    {
        struct Item { const char* url; const char* sha256; const char* inner; const char* saveAs; double megabytes; };
        inline juce::String libName()
        {
           #if JUCE_WINDOWS
            return "onnxruntime.dll";
           #elif JUCE_MAC
            return "libonnxruntime.1.17.3.dylib";
           #else
            return "libonnxruntime.so.1.17.3";
           #endif
        }
        inline Item runtimeItem()
        {
           #if JUCE_WINDOWS
            return { "https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-win-x64-1.17.3.zip", "356a33d024f2709786bebd5d4ca06cd5392875da95daa0455aae72edc8993256", "lib/onnxruntime.dll", "onnxruntime.dll", 62 };
           #elif JUCE_MAC && JUCE_ARM
            return { "https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-osx-arm64-1.17.3.tgz", "236c49c9065213b0ec9dec874e3619da3d01cbc8b984bb24291247293454d0f4", "lib/libonnxruntime.1.17.3.dylib", "libonnxruntime.1.17.3.dylib", 7 };
           #elif JUCE_MAC
            return { "https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-osx-x86_64-1.17.3.tgz", "6292ad3d2e095b54b012a9fce7361f39fbac0b75fb6e9b1d9c320874515182e8", "lib/libonnxruntime.1.17.3.dylib", "libonnxruntime.1.17.3.dylib", 8 };
           #else
            return { "https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-linux-x64-1.17.3.tgz", "f2f11f9da1e3e19b22a8b378b9af57a58433f40e3db6a803e75c0ec0eba97a20", "lib/libonnxruntime.so.1.17.3", "libonnxruntime.so.1.17.3", 6 };
           #endif
        }
        inline Item model6Item() { return { "https://huggingface.co/StemSplitio/htdemucs-6s-onnx/resolve/main/htdemucs_6s_fp16weights.onnx", "7ce55792e2231c93fbf92de95f5fd5b3a5e6c89f7db690dfd693e8f1dce56869", "", "prism-split-6.onnx", 130 }; }
        inline Item modelItem() { return { "https://huggingface.co/StemSplitio/htdemucs-onnx/resolve/main/htdemucs_fp16weights.onnx", "d05c269d0178d2a72ad484b10b11dd370193fc923201c3b27a99f848745db70a", "", "prism-split-4.onnx", 158 }; }

        inline juce::File folder()
        {
            const auto env = juce::SystemStats::getEnvironmentVariable ("SQUELCH_STUDIO_PACK", {});
            if (env.isNotEmpty()) return juce::File (env);
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Squelchmoth").getChildFile ("Prism Split");
        }
        inline juce::File libFile()   { return folder().getChildFile (libName()); }
        inline juce::File modelFile() { return folder().getChildFile ("prism-split-4.onnx"); }
        inline juce::File model6File(){ return folder().getChildFile ("prism-split-6.onnx"); }
        inline bool installed()       { return libFile().existsAsFile() && modelFile().existsAsFile(); }
        inline bool installed6()      { return libFile().existsAsFile() && model6File().existsAsFile(); }

        struct Install { std::atomic<float> progress { 0.0f }; std::atomic<bool> running { false }, finished { false }; juce::String error, stage; juce::WaitableEvent done { true }; };
        inline Install& state() { static Install s; return s; }

        inline bool download (const juce::String& url, const juce::File& dest, double expectMb, float p0, float p1, juce::String& err)
        {
            auto in = juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (20000).withNumRedirectsToFollow (8));
            if (in == nullptr) { err = "Couldn't connect to download " + url.fromLastOccurrenceOf ("/", false, false); return false; }
            dest.deleteFile(); juce::FileOutputStream out (dest);
            if (! out.openedOk()) { err = "Couldn't write " + dest.getFullPathName(); return false; }
            const double total = in->getTotalLength() > 0 ? (double) in->getTotalLength() : expectMb * 1048576.0;
            juce::HeapBlock<char> buf (1 << 16); juce::int64 got = 0;
            while (! in->isExhausted())
            {
                const int n = in->read (buf, 1 << 16); if (n <= 0) break;
                if (! out.write (buf, (size_t) n)) { err = "The disk is full or not writable"; return false; }
                got += n; state().progress = p0 + (p1 - p0) * (float) juce::jmin (1.0, (double) got / total);
            }
            out.flush();
            return got > 0;
        }

        /** Pulls one regular file out of a .tgz (ustar) whose path ends with `inner`. */
        inline bool untarOne (const juce::File& tgz, const juce::String& inner, const juce::File& dest)
        {
            juce::FileInputStream fin (tgz); if (! fin.openedOk()) return false;
            juce::GZIPDecompressorInputStream gz (&fin, false, juce::GZIPDecompressorInputStream::gzipFormat);
            juce::HeapBlock<char> h (512), buf (1 << 16);
            for (;;)
            {
                if (gz.read (h, 512) != 512 || h[0] == 0) return false;
                const juce::String name = juce::String::fromUTF8 (h + 345, (int) std::min<size_t> (155, strnlen (h + 345, 155))) + (h[345] != 0 ? "/" : "") + juce::String::fromUTF8 (h, (int) strnlen (h, 100));
                const juce::int64 size = juce::String::fromUTF8 (h + 124, 12).trim().getIntValue() == 0 && h[124] == 0 ? 0 : (juce::int64) std::strtoll (std::string (h + 124, 12).c_str(), nullptr, 8);
                const char type = h[156];
                const juce::int64 padded = (size + 511) / 512 * 512;
                if ((type == '0' || type == 0) && name.endsWith (inner))
                {
                    dest.deleteFile(); juce::FileOutputStream out (dest); if (! out.openedOk()) return false;
                    for (juce::int64 left = size; left > 0;) { const int n = gz.read (buf, (int) juce::jmin ((juce::int64) (1 << 16), left)); if (n <= 0) return false; out.write (buf, (size_t) n); left -= n; }
                    out.flush(); return true;
                }
                for (juce::int64 left = padded; left > 0;) { const int n = gz.read (buf, (int) juce::jmin ((juce::int64) (1 << 16), left)); if (n <= 0) return false; left -= n; }
            }
        }

        inline bool fetch (const Item& it, float p0, float p1, juce::String& err)
        {
            const auto dir = folder(); dir.createDirectory();
            const auto finalFile = dir.getChildFile (it.saveAs);
            if (finalFile.existsAsFile()) return true;
            const auto tmp = dir.getChildFile (juce::String (it.saveAs) + ".download");
            if (! download (it.url, tmp, it.megabytes, p0, p1, err)) { tmp.deleteFile(); return false; }
            if (! juce::SHA256 (tmp).toHexString().equalsIgnoreCase (it.sha256)) { tmp.deleteFile(); err = juce::String (it.saveAs) + ": the download did not match its checksum (try again)"; return false; }
            bool ok = false;
            if (juce::String (it.inner).isEmpty()) ok = tmp.moveFileTo (finalFile);
            else if (juce::String (it.url).endsWith (".zip"))
            {
                juce::ZipFile zip (tmp);
                for (int i = 0; i < zip.getNumEntries(); ++i)
                    if (zip.getEntry (i)->filename.replaceCharacter ('\\', '/').endsWith (it.inner))
                        if (auto in = std::unique_ptr<juce::InputStream> (zip.createStreamForEntry (i))) { finalFile.deleteFile(); juce::FileOutputStream out (finalFile); if (out.openedOk()) { out.writeFromInputStream (*in, -1); out.flush(); ok = true; } }
                tmp.deleteFile();
            }
            else { ok = untarOne (tmp, it.inner, finalFile); tmp.deleteFile(); }
            if (! ok) err = juce::String ("Couldn't unpack ") + it.saveAs;
            return ok;
        }

        /** Starts the install on a background thread (runtime, then the model unless runtimeOnly). */
        inline void startInstall (bool runtimeOnly, bool sixStems = false)
        {
            auto& st = state();
            if (st.running.exchange (true)) return;
            st.finished = false; st.error = {}; st.progress = 0; st.done.reset();
            juce::Thread::launch ([runtimeOnly, sixStems]
            {
                auto& s = state(); juce::String err;
                s.stage = "separation runtime";
                bool ok = fetch (runtimeItem(), 0.0f, runtimeOnly ? 1.0f : 0.15f, err);
                if (ok && ! runtimeOnly && ! sixStems) { s.stage = "separation model (158 MB)"; ok = fetch (modelItem(), 0.15f, 1.0f, err); }
                if (ok && ! runtimeOnly && sixStems) { s.stage = "6-stem model with guitar and piano (130 MB)"; ok = fetch (model6Item(), 0.15f, 1.0f, err); }
                s.error = ok ? juce::String() : err; s.progress = 1.0f; s.finished = true; s.running = false; s.done.signal();
            });
        }

        /** The loaded engine (kept for the session; one model in memory at a time). */
        inline SqNeural::Engine* engine (juce::String& err, int stems = 4)
        {
            static SqNeural::Engine e; static std::mutex m; std::lock_guard<std::mutex> lk (m);
            if (e.isLoaded() && e.stems == stems) return &e;
            if (stems == 6 ? ! installed6() : ! installed()) { err = stems == 6 ? "The 6-stem model is not installed yet - separate action install_studio with stems 6" : "Studio quality is not installed yet - separate action install_studio"; return nullptr; }
            return e.load (libFile(), stems == 6 ? model6File() : modelFile(), juce::jlimit (1, 8, juce::SystemStats::getNumPhysicalCpus()), err, stems) ? &e : nullptr;
        }
    }

    static const char* const stemNames6[6] = { "Vocals", "Drums", "Bass", "Other", "Guitar", "Piano" };

    struct Job
    {
        juce::File source, folder; double startSec = 0, lengthSec = 0; juce::String label; bool wantMinus = false, studio = false;
        std::vector<juce::File> stems; int numStems = 4; juce::File instrumental, acapella;
        std::atomic<float> progress { 0.0f }; std::atomic<bool> finished { false }, imported { false };
        juce::String error; double seconds = 0, tookSeconds = 0; double nullDb = -200.0;
        juce::WaitableEvent done { true };
        int id = 0; double placeAtSeconds = 0;   // where the stems go on the timeline
    };

    inline void run (Job& j)
    {
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (j.source));
        if (rd == nullptr || rd->lengthInSamples <= 0) { j.error = "Couldn't read " + j.source.getFileName() + " (WAV, AIFF, FLAC, MP3 or OGG)"; return; }
        const double srIn = rd->sampleRate;
        SqNeural::Engine* eng = nullptr;
        if (j.studio) { eng = StudioPack::engine (j.error, j.numStems); if (eng == nullptr) return; }
        const int N = j.studio ? j.numStems : 4; j.numStems = N; j.stems.resize ((size_t) N);
        const double sr = j.studio ? SqNeural::modelRate : srIn;   // the rate the stems are written at
        const juce::int64 s0 = juce::jlimit ((juce::int64) 0, rd->lengthInSamples, (juce::int64) (j.startSec * srIn));
        juce::int64 nIn = rd->lengthInSamples - s0; if (j.lengthSec > 0) nIn = juce::jmin (nIn, (juce::int64) (j.lengthSec * srIn));
        if (nIn < (juce::int64) srIn) { j.error = "Less than a second of audio to separate"; return; }
        j.seconds = nIn / srIn;
        const juce::int64 n = (juce::int64) std::llround (nIn * sr / srIn);   // samples to write
        if (! j.folder.createDirectory()) { j.error = "Couldn't create " + j.folder.getFullPathName(); return; }
        juce::WavAudioFormat wav;
        auto open = [&] (const juce::File& f) -> std::unique_ptr<juce::AudioFormatWriter>
        {
            f.deleteFile(); auto os = f.createOutputStream(); if (os == nullptr) return nullptr;
            auto* w = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0); if (w != nullptr) os.release();
            return std::unique_ptr<juce::AudioFormatWriter> (w);
        };
        std::vector<std::unique_ptr<juce::AudioFormatWriter>> w ((size_t) N); std::unique_ptr<juce::AudioFormatWriter> wi, wa;
        for (int s = 0; s < N; ++s) { j.stems[(size_t) s] = j.folder.getChildFile (j.label + " - " + stemNames6[s] + ".wav"); w[(size_t) s] = open (j.stems[(size_t) s]); if (w[(size_t) s] == nullptr) { j.error = "Couldn't write into " + j.folder.getFullPathName(); return; } }
        if (j.wantMinus) { j.instrumental = j.folder.getChildFile (j.label + " - Instrumental.wav"); j.acapella = j.folder.getChildFile (j.label + " - Acapella.wav"); wi = open (j.instrumental); wa = open (j.acapella); }

        std::unique_ptr<SqSplit::Splitter> draft; std::unique_ptr<SqNeural::Splitter> neural;
        if (eng != nullptr) neural = std::make_unique<SqNeural::Splitter> (*eng); else draft = std::make_unique<SqSplit::Splitter> (sr);
        juce::int64 written = 0; std::vector<float> tmpL, tmpR;
        // the stems come out later than the input goes in: a delay line of the dry mix would be needed for a null figure,
        // so the null is measured by re-adding the stems against the mix kept in a small ring
        std::vector<float> dryL, dryR; juce::int64 dryBase = 0; double errE = 0, mixE = 0;
        auto sink = [&] (const float* const* st, int m)
        {
            const int take = (int) juce::jmin ((juce::int64) m, n - written); if (take <= 0) return;
            for (int s = 0; s < N; ++s) { const float* chans[2] = { st[2 * s], st[2 * s + 1] }; w[(size_t) s]->writeFromFloatArrays (chans, 2, take); }
            if (wi != nullptr && wa != nullptr)
            {
                tmpL.resize ((size_t) take); tmpR.resize ((size_t) take);
                for (int i = 0; i < take; ++i) { float l = 0, r = 0; for (int q = 1; q < N; ++q) { l += st[2 * q][i]; r += st[2 * q + 1][i]; } tmpL[(size_t) i] = l; tmpR[(size_t) i] = r; }
                const float* ic[2] = { tmpL.data(), tmpR.data() }; wi->writeFromFloatArrays (ic, 2, take);
                const float* ac[2] = { st[0], st[1] }; wa->writeFromFloatArrays (ac, 2, take);
            }
            for (int i = 0; i < take; ++i)
            {
                const juce::int64 idx = written + i - dryBase; if (idx < 0 || idx >= (juce::int64) dryL.size()) continue;
                double sum = 0; for (int q = 0; q < N; ++q) sum += st[2 * q][i];
                const double d = sum - dryL[(size_t) idx];
                errE += d * d; mixE += (double) dryL[(size_t) idx] * dryL[(size_t) idx];
            }
            written += take;
            // drop the part of the dry ring that has been compared
            const juce::int64 drop = written - dryBase; if (drop > (juce::int64) (1 << 18) && drop <= (juce::int64) dryL.size()) { dryL.erase (dryL.begin(), dryL.begin() + (std::ptrdiff_t) drop); dryR.erase (dryR.begin(), dryR.begin() + (std::ptrdiff_t) drop); dryBase += drop; }
        };
        auto feed = [&] (const float* l, const float* r, int m)
        {
            dryL.insert (dryL.end(), l, l + m); dryR.insert (dryR.end(), r, r + m);
            if (neural != nullptr) neural->push (l, r, m, sink); else draft->push (l, r, m, sink);
        };
        const bool resample = std::abs (srIn - sr) > 0.5; const double ratio = srIn / sr;
        juce::WindowedSincInterpolator rsL, rsR; std::vector<float> fifoL, fifoR, outL, outR;
        const int chunk = 1 << 16; juce::AudioBuffer<float> buf ((int) juce::jmax (1u, rd->numChannels), chunk);
        for (juce::int64 done = 0; done < nIn; done += chunk)
        {
            const int m = (int) juce::jmin ((juce::int64) chunk, nIn - done);
            rd->read (&buf, 0, m, s0 + done, true, true);
            const float* l = buf.getReadPointer (0); const float* r = buf.getReadPointer (juce::jmin (1, buf.getNumChannels() - 1));
            if (! resample) feed (l, r, m);
            else
            {
                fifoL.insert (fifoL.end(), l, l + m); fifoR.insert (fifoR.end(), r, r + m);
                const bool last = done + chunk >= nIn;
                if (last) { fifoL.insert (fifoL.end(), 64, 0.0f); fifoR.insert (fifoR.end(), 64, 0.0f); }
                const int numOut = (int) (((double) fifoL.size() - 16.0) / ratio);
                if (numOut > 0)
                {
                    outL.resize ((size_t) numOut); outR.resize ((size_t) numOut);
                    const int used = rsL.process (ratio, fifoL.data(), outL.data(), numOut); rsR.process (ratio, fifoR.data(), outR.data(), numOut);
                    fifoL.erase (fifoL.begin(), fifoL.begin() + juce::jmin ((int) fifoL.size(), used)); fifoR.erase (fifoR.begin(), fifoR.begin() + juce::jmin ((int) fifoR.size(), used));
                    feed (outL.data(), outR.data(), numOut);
                }
            }
            j.progress = (float) ((double) (done + m) / (double) nIn) * (neural != nullptr ? 0.999f : 1.0f);
            if (neural != nullptr && neural->error.isNotEmpty()) { j.error = neural->error; return; }
        }
        if (neural != nullptr) { neural->finish (sink); if (neural->error.isNotEmpty()) { j.error = neural->error; return; } } else draft->finish (sink);
        for (auto& x : w) x.reset(); wi.reset(); wa.reset();
        j.nullDb = mixE > 0 ? 10.0 * std::log10 ((errE + 1e-20) / mixE) : -200.0;
        j.tookSeconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    }

    class Store
    {
    public:
        static Store& get() { static Store s; return s; }
        std::shared_ptr<Job> start (const juce::File& src, double startSec, double lengthSec, const juce::File& folder, const juce::String& label, bool minus, double placeAt, bool studio = false, int numStems = 4)
        {
            auto j = std::make_shared<Job>();
            j->source = src; j->startSec = startSec; j->lengthSec = lengthSec; j->folder = folder; j->label = label; j->wantMinus = minus; j->placeAtSeconds = placeAt; j->studio = studio; j->numStems = studio ? numStems : 4; j->stems.resize ((size_t) j->numStems);
            { std::lock_guard<std::mutex> lk (lock); j->id = ++nextId; jobs.push_back (j); while (jobs.size() > 8) jobs.erase (jobs.begin()); }
            pool.addJob ([j] { run (*j); j->progress = 1.0f; j->finished = true; j->done.signal(); });
            return j;
        }
        std::shared_ptr<Job> find (int id) { std::lock_guard<std::mutex> lk (lock); for (auto& j : jobs) if (j->id == id || id <= 0) { if (id > 0) return j; } return jobs.empty() ? nullptr : jobs.back(); }
    private:
        std::mutex lock; std::vector<std::shared_ptr<Job>> jobs; int nextId = 0; juce::ThreadPool pool { 1 };
    };
}
