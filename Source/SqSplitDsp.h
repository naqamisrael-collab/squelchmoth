#pragma once
// Squelchmoth 1.16 "Prism Split" - stage 0: the Draft separator (te-free, tested by tests/split_test.cpp).
//
// Splits a stereo mix into four stems that always sum back to the mix exactly: vocals / centre lead, drums, bass, other.
// No model: spectral masks from three measurements per time-frequency bin -
//   harmonic vs percussive (median along time vs median along frequency),
//   centre vs sides (inter-channel similarity, phase included),
//   and the frequency region (bass below ~150 Hz, the voice band).
// The masks add up to one in every bin, so the null test (stems minus mix) is silence by construction. This is the
// fast Draft quality and the frame the neural Studio / Master stages plug into (master prompt 2.0, P2).
// Streaming: constant memory whatever the length (a ring of 17 frames), so hours of audio are fine.

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <complex>
#include <functional>
#include <algorithm>
#include <cmath>

namespace SqSplit
{
    enum Stem { vocals = 0, drums, bass, other, numStems };
    static const char* stemNames[numStems] = { "Vocals", "Drums", "Bass", "Other" };

    /** Streaming four-stem splitter. push() stereo blocks; it calls `out` with matching-length blocks per stem, delayed by
        latency() samples; finish() flushes the tail. */
    class Splitter
    {
    public:
        static constexpr int order = 12, N = 1 << order, hop = N / 4, bins = N / 2 + 1, ctx = 8, ring = 2 * ctx + 1;
        using Sink = std::function<void (const float* const* stemsLR /* [numStems * 2] */, int n)>;

        explicit Splitter (double sampleRate) : sr (sampleRate), fft (order)
        {
            window.resize ((size_t) N);
            for (int i = 0; i < N; ++i) window[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / N));   // sqrt-Hann: analysis x synthesis = Hann, 75 % overlap sums to 2
            for (auto& ch : in) ch.assign ((size_t) N, 0.0f);
            for (auto& f : frames) { f.L.assign ((size_t) bins, {}); f.R.assign ((size_t) bins, {}); f.mag.assign ((size_t) bins, 0.0f); }
            for (auto& o : ola) o.assign ((size_t) N, 0.0f);
            wLow.resize ((size_t) bins); wVoice.resize ((size_t) bins);
            for (int b = 0; b < bins; ++b)
            {
                const double hz = b * sr / N;
                wLow[(size_t) b] = (float) juce::jlimit (0.0, 1.0, (200.0 - hz) / 80.0);                                 // 1 below 120 Hz, 0 above 200 Hz
                wVoice[(size_t) b] = (float) (juce::jlimit (0.0, 1.0, (hz - 150.0) / 100.0) * juce::jlimit (0.0, 1.0, (12000.0 - hz) / 3000.0));
            }
            scratch.resize ((size_t) (2 * N));
            for (auto& s : outBlock) s.assign ((size_t) hop, 0.0f);
        }

        int latency() const { return N - hop + ctx * hop; }

        void push (const float* L, const float* R, int n, const Sink& out)
        {
            for (int i = 0; i < n; ++i)
            {
                in[0][(size_t) fill] = L[i]; in[1][(size_t) fill] = R != nullptr ? R[i] : L[i];
                if (++fill == N) { analyse(); if (framesIn > ctx) synthesise (out); std::memmove (in[0].data(), in[0].data() + hop, sizeof (float) * (size_t) (N - hop)); std::memmove (in[1].data(), in[1].data() + hop, sizeof (float) * (size_t) (N - hop)); fill = N - hop; }
            }
        }

        /** Flushes what is still inside (latency() samples of zeros through the same path). */
        void finish (const Sink& out)
        {
            std::vector<float> z ((size_t) 4096, 0.0f);
            for (int left = latency() + N; left > 0; left -= 4096) push (z.data(), z.data(), juce::jmin (4096, left), out);
        }

    private:
        struct Frame { std::vector<std::complex<float>> L, R; std::vector<float> mag; };

        void analyse()
        {
            auto& f = frames[(size_t) (framesIn % ring)];
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < N; ++i) scratch[(size_t) i] = in[(size_t) ch][(size_t) i] * window[(size_t) i];
                std::fill (scratch.begin() + N, scratch.end(), 0.0f);
                fft.performRealOnlyForwardTransform (scratch.data(), true);
                auto& dst = ch == 0 ? f.L : f.R;
                for (int b = 0; b < bins; ++b) dst[(size_t) b] = { scratch[(size_t) (2 * b)], scratch[(size_t) (2 * b + 1)] };
            }
            for (int b = 0; b < bins; ++b) f.mag[(size_t) b] = std::sqrt (std::norm (f.L[(size_t) b]) + std::norm (f.R[(size_t) b]));
            ++framesIn;
        }

        /** Masks and resynthesis of the frame `ctx` frames back (its time neighbours on both sides are in the ring). */
        void synthesise (const Sink& out)
        {
            const long centre = framesIn - 1 - ctx;
            if (centre < 0) return;
            auto& f = frames[(size_t) (centre % ring)];
            const long first = std::max (0L, centre - ctx), last = framesIn - 1;
            std::array<float, ring> tv {}; std::array<float, 2 * ctx + 1> fv {};
            std::array<std::vector<std::complex<float>>, numStems * 2> spec;
            for (auto& s : spec) s.assign ((size_t) bins, {});
            for (int b = 0; b < bins; ++b)
            {
                int nt = 0; for (long k = first; k <= last; ++k) tv[(size_t) nt++] = frames[(size_t) (k % ring)].mag[(size_t) b];
                std::nth_element (tv.begin(), tv.begin() + nt / 2, tv.begin() + nt); const float H = tv[(size_t) (nt / 2)];
                int nf = 0; for (int j = std::max (0, b - ctx); j <= std::min (bins - 1, b + ctx); ++j) fv[(size_t) nf++] = f.mag[(size_t) j];
                std::nth_element (fv.begin(), fv.begin() + nf / 2, fv.begin() + nf); const float P = fv[(size_t) (nf / 2)];
                const float h2 = H * H, p2 = P * P;
                const float mh = h2 / (h2 + p2 + 1e-20f), mp = 1.0f - mh;
                // centre-ness: 1 when both channels carry the same thing in phase
                const auto l = f.L[(size_t) b], r = f.R[(size_t) b];
                const float e = std::norm (l) + std::norm (r);
                const float sim = e > 1e-20f ? 2.0f * (l.real() * r.real() + l.imag() * r.imag()) / e : 0.0f;
                float cm = juce::jlimit (0.0f, 1.0f, (sim - 0.7f) / 0.27f); cm = cm * cm * (3.0f - 2.0f * cm);
                const float lo = wLow[(size_t) b];
                const float mBass = mh * lo, mDrums = mp, mVox = mh * (1.0f - lo) * wVoice[(size_t) b] * cm;
                const float mOther = juce::jmax (0.0f, 1.0f - mBass - mDrums - mVox);
                const float m[numStems] = { mVox, mDrums, mBass, mOther };
                for (int s = 0; s < numStems; ++s) { spec[(size_t) (2 * s)][(size_t) b] = l * m[s]; spec[(size_t) (2 * s + 1)][(size_t) b] = r * m[s]; }
            }
            const float norm = 0.5f;   // sqrt-Hann squared at 75 % overlap sums to 2
            const float* ptrs[numStems * 2];
            for (int c = 0; c < numStems * 2; ++c)
            {
                for (int b = 0; b < bins; ++b) { scratch[(size_t) (2 * b)] = spec[(size_t) c][(size_t) b].real(); scratch[(size_t) (2 * b + 1)] = spec[(size_t) c][(size_t) b].imag(); }
                for (int b = bins; b < N; ++b) { scratch[(size_t) (2 * b)] = scratch[(size_t) (2 * (N - b))]; scratch[(size_t) (2 * b + 1)] = -scratch[(size_t) (2 * (N - b) + 1)]; }
                fft.performRealOnlyInverseTransform (scratch.data());
                auto& o = ola[(size_t) c];
                for (int i = 0; i < N; ++i) o[(size_t) i] += scratch[(size_t) i] * window[(size_t) i] * norm;
                std::copy (o.begin(), o.begin() + hop, outBlock[(size_t) c].begin());
                std::memmove (o.data(), o.data() + hop, sizeof (float) * (size_t) (N - hop)); std::fill (o.end() - hop, o.end(), 0.0f);
                ptrs[c] = outBlock[(size_t) c].data();
            }
            // the first N - hop samples out are the window's fade-in on silence padding: skip them so output lines up
            if (skip > 0) { const int k = juce::jmin (skip, hop); skip -= k; if (k < hop) { for (int c = 0; c < numStems * 2; ++c) ptrs[c] += k; out (ptrs, hop - k); } }
            else out (ptrs, hop);
        }

        double sr; juce::dsp::FFT fft;
        std::vector<float> window, scratch, wLow, wVoice;
        std::array<std::vector<float>, 2> in; int fill = N - hop; long framesIn = 0;   // primed with silence so the first real sample is fully overlapped
        std::array<Frame, ring> frames;
        std::array<std::vector<float>, numStems * 2> ola, outBlock;
        int skip = N - hop;
    };

    /** Whole buffers in, whole stems out (tests and short clips). Output is time-aligned with the input. */
    inline std::array<std::vector<float>, numStems * 2> splitBuffers (const float* L, const float* R, int n, double sr)
    {
        std::array<std::vector<float>, numStems * 2> o;
        Splitter sp (sr);
        auto sink = [&] (const float* const* s, int m) { for (int c = 0; c < numStems * 2; ++c) o[(size_t) c].insert (o[(size_t) c].end(), s[c], s[c] + m); };
        sp.push (L, R, n, sink); sp.finish (sink);
        for (auto& v : o) v.resize ((size_t) n, 0.0f);
        return o;
    }
}
