#pragma once
// Squelchmoth 1.16 "Prism Split" - Studio quality: a hybrid-transformer separation model run through ONNX Runtime.
//
// The runtime is loaded at run time from a shared library (no link-time dependency: the app starts and works without
// it), the model is one .onnx file (waveform in, four stems out: 44.1 kHz stereo, 7.8 s segments). This class has the
// same push / finish / sink shape as the Draft splitter, streams in constant memory, cross-fades overlapping segments
// and puts whatever the model's stems do not explain into Other, so the four stems still sum back to the mix exactly.

#include <JuceHeader.h>
#include "onnxruntime/onnxruntime_c_api.h"
#include "SqSplitDsp.h"
#include <vector>
#include <array>
#include <functional>

namespace SqNeural
{
    constexpr double modelRate = 44100.0;
    constexpr int segment = 343980, overlap = segment / 4, stride = segment - overlap;

    /** The loaded runtime + model. One per process is enough; run() is called from one thread at a time. */
    class Engine
    {
    public:
        ~Engine() { unload(); }

        int stems = 4;   // 4 (drums, bass, other, vocals) or 6 (+ guitar, piano): the model's output count

        bool load (const juce::File& runtimeLib, const juce::File& model, int threads, juce::String& err, int numModelStems = 4)
        {
            unload(); stems = numModelStems;
            if (! runtimeLib.existsAsFile()) { err = "The separation runtime is missing (" + runtimeLib.getFileName() + ")"; return false; }
            if (! model.existsAsFile()) { err = "The Studio separation model is not installed"; return false; }
            if (! lib.open (runtimeLib.getFullPathName())) { err = "Couldn't load " + runtimeLib.getFileName(); return false; }
            using GetBase = const OrtApiBase* (ORT_API_CALL*)();
            auto getBase = (GetBase) lib.getFunction ("OrtGetApiBase");
            if (getBase == nullptr) { err = "Not an ONNX Runtime library"; return false; }
            const OrtApiBase* base = getBase();
            for (uint32_t v = ORT_API_VERSION; v >= 13 && api == nullptr; --v) api = base->GetApi (v);
            if (api == nullptr) { err = "The separation runtime is too old"; return false; }
            if (! ok (api->CreateEnv (ORT_LOGGING_LEVEL_ERROR, "squelchmoth", &env), err)) return false;
            OrtSessionOptions* so = nullptr;
            if (! ok (api->CreateSessionOptions (&so), err)) return false;
            api->SetIntraOpNumThreads (so, juce::jlimit (1, 16, threads));
            // measured: full graph optimisation of this model needs several GB while loading; without it the peak is
            // about 1.2 GB and a segment runs at the same speed
            api->SetSessionGraphOptimizationLevel (so, ORT_DISABLE_ALL);
            api->DisableMemPattern (so);
           #if JUCE_WINDOWS
            const auto path = model.getFullPathName().toWideCharPointer();
           #else
            const auto path = model.getFullPathName().toRawUTF8();
           #endif
            const bool made = ok (api->CreateSession (env, path, so, &session), err);
            api->ReleaseSessionOptions (so);
            if (! made) return false;
            if (! ok (api->CreateCpuMemoryInfo (OrtArenaAllocator, OrtMemTypeDefault, &mem), err)) return false;
            return true;
        }

        bool isLoaded() const { return session != nullptr; }

        /** in: [2][segment] planar; out: [stems][2][segment] planar (model order: drums, bass, other, vocals, then guitar, piano). */
        bool run (const float* in, float* out, juce::String& err)
        {
            if (session == nullptr) { err = "No model loaded"; return false; }
            const int64_t shape[3] = { 1, 2, segment };
            OrtValue* input = nullptr;
            if (! ok (api->CreateTensorWithDataAsOrtValue (mem, (void*) in, sizeof (float) * 2 * (size_t) segment, shape, 3, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input), err)) return false;
            const char* inNames[] = { "mix" }; const char* outNames[] = { "stems" };
            OrtValue* output = nullptr;
            const bool ran = ok (api->Run (session, nullptr, inNames, (const OrtValue* const*) &input, 1, outNames, 1, &output), err);
            api->ReleaseValue (input);
            if (! ran) return false;
            float* data = nullptr; bool good = ok (api->GetTensorMutableData (output, (void**) &data), err);
            if (good) std::memcpy (out, data, sizeof (float) * 2 * (size_t) stems * (size_t) segment);
            api->ReleaseValue (output);
            return good;
        }

        void unload()
        {
            if (api != nullptr)
            {
                if (mem != nullptr) api->ReleaseMemoryInfo (mem);
                if (session != nullptr) api->ReleaseSession (session);
                if (env != nullptr) api->ReleaseEnv (env);
            }
            mem = nullptr; session = nullptr; env = nullptr; api = nullptr; lib.close();
        }

    private:
        bool ok (OrtStatus* st, juce::String& err)
        {
            if (st == nullptr) return true;
            err = juce::String ("Separation runtime: ") + api->GetErrorMessage (st); api->ReleaseStatus (st); return false;
        }
        juce::DynamicLibrary lib; const OrtApi* api = nullptr; OrtEnv* env = nullptr; OrtSession* session = nullptr; OrtMemoryInfo* mem = nullptr;
    };

    /** Streaming four-stem splitter on the model (input at 44.1 kHz). Same sink as SqSplit::Splitter; output is aligned
        with the input and comes out one segment late. */
    class Splitter
    {
    public:
        using Sink = SqSplit::Splitter::Sink;
        explicit Splitter (Engine& e) : engine (e)
        {
            window.resize ((size_t) segment, 1.0f);
            for (int i = 0; i < overlap; ++i) { const float w = (float) (i + 1) / (float) (overlap + 1); window[(size_t) i] = w; window[(size_t) (segment - 1 - i)] = w; }
            nStems = engine.stems; acc.resize ((size_t) (2 * nStems));
            segIn.resize ((size_t) (2 * segment)); segOut.resize ((size_t) (2 * nStems * segment));
        }

        juce::String error;                       // set when a run fails; the rest of the stream is passed to Other

        void push (const float* L, const float* R, int n, const Sink& out)
        {
            dry[0].insert (dry[0].end(), L, L + n); dry[1].insert (dry[1].end(), R != nullptr ? R : L, (R != nullptr ? R : L) + n);
            total += n;
            while (total >= nextStart + segment) processSegment (out, false);
        }

        void finish (const Sink& out)
        {
            while (emitted < total) processSegment (out, true);
        }

    private:
        void processSegment (const Sink& out, bool padding)
        {
            // the segment [nextStart, nextStart + segment) from the dry ring (zero-padded at the end of the stream)
            const juce::int64 avail = total - nextStart;
            const int have = (int) juce::jlimit ((juce::int64) 0, (juce::int64) segment, avail);
            for (int ch = 0; ch < 2; ++ch)
            {
                std::fill (segIn.begin() + ch * segment, segIn.begin() + (ch + 1) * segment, 0.0f);
                if (have > 0) std::copy (dry[(size_t) ch].begin() + (std::ptrdiff_t) (nextStart - base), dry[(size_t) ch].begin() + (std::ptrdiff_t) (nextStart - base + have), segIn.begin() + ch * segment);
            }
            const juce::int64 accNeed = nextStart + segment - base;
            for (auto& a : acc) if ((juce::int64) a.size() < accNeed) a.resize ((size_t) accNeed, 0.0f);
            if ((juce::int64) wsum.size() < accNeed) wsum.resize ((size_t) accNeed, 0.0f);
            const bool ran = have > 0 && error.isEmpty() && engine.run (segIn.data(), segOut.data(), error);
            static const int toOurs[6] = { SqSplit::drums, SqSplit::bass, SqSplit::other, SqSplit::vocals, 4, 5 };   // model order -> ours (guitar 4, piano 5)
            const bool first = nextStart == 0;
            for (int i = 0; i < segment; ++i)
            {
                const float w = first && i < overlap ? 1.0f : window[(size_t) i];
                const size_t at = (size_t) (nextStart - base + i);
                wsum[at] += w;
                if (ran) for (int s = 0; s < nStems; ++s) for (int ch = 0; ch < 2; ++ch) acc[(size_t) (2 * toOurs[s] + ch)][at] += w * segOut[(size_t) ((s * 2 + ch) * segment + i)];
            }
            nextStart += stride;
            // everything before the next segment's start is final now (at the end of the stream: everything)
            const juce::int64 upTo = padding ? total : juce::jmin (total, nextStart);
            const int n = (int) (upTo - emitted);
            if (n > 0)
            {
                const size_t o = (size_t) (emitted - base);
                for (int i = 0; i < n; ++i)
                {
                    const float inv = 1.0f / juce::jmax (1e-6f, wsum[o + (size_t) i]);
                    float sumL = 0, sumR = 0;
                    for (int s = 0; s < nStems; ++s) { acc[(size_t) (2 * s)][o + (size_t) i] *= inv; acc[(size_t) (2 * s + 1)][o + (size_t) i] *= inv; sumL += acc[(size_t) (2 * s)][o + (size_t) i]; sumR += acc[(size_t) (2 * s + 1)][o + (size_t) i]; }
                    // what the model's stems do not explain goes to Other: the stems sum to the mix exactly
                    acc[(size_t) (2 * SqSplit::other)][o + (size_t) i] += dry[0][o + (size_t) i] - sumL;
                    acc[(size_t) (2 * SqSplit::other + 1)][o + (size_t) i] += dry[1][o + (size_t) i] - sumR;
                }
                const float* ptrs[12];
                for (int c = 0; c < 2 * nStems; ++c) ptrs[c] = acc[(size_t) c].data() + o;
                out (ptrs, n);
                emitted = upTo;
            }
            // drop what is both emitted and behind the next segment
            const juce::int64 keepFrom = juce::jmin (emitted, nextStart), drop = keepFrom - base;
            if (drop > 0)
            {
                for (auto& d : dry) d.erase (d.begin(), d.begin() + (std::ptrdiff_t) juce::jmin ((juce::int64) d.size(), drop));
                for (auto& a : acc) a.erase (a.begin(), a.begin() + (std::ptrdiff_t) juce::jmin ((juce::int64) a.size(), drop));
                wsum.erase (wsum.begin(), wsum.begin() + (std::ptrdiff_t) juce::jmin ((juce::int64) wsum.size(), drop));
                base = keepFrom;
            }
        }

        Engine& engine;
        std::vector<float> window, segIn, segOut, wsum;
        std::array<std::vector<float>, 2> dry;
        std::vector<std::vector<float>> acc; int nStems = 4;
        juce::int64 total = 0, nextStart = 0, emitted = 0, base = 0;
    };
}
