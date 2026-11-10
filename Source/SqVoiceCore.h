#pragma once
// Squelchmoth 1.11 voice core (split from SqEngine111.h so the Voice plug-ins can use it before Signature exists):
//   SqVoiceCore::Core   Moth Tune's pitch tracker as a shared service: pitch, voicing, sibilance, formants F1-F4,
//                       harmonic-to-noise ratio and a fixed-latency harmonic / noise split.
#include <JuceHeader.h>
#include "SqEngine111Dsp.h"
#include "SqVocalDsp.h"

namespace SqVoiceCore
{
    struct Snapshot
    {
        float hz = 0, confidence = 0, rmsDb = -120, hnrDb = 0;
        bool voiced = false, sibilant = false;
        float formants[4] {};        // Hz, 0 when not found
        juce::int64 position = 0;    // input sample the frame describes
    };

    class Core
    {
    public:
        /** Latency of split(): the comb looks one longest period (60 Hz) either side. */
        int splitLatency() const   { return maxPeriod; }

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate > 0 ? sampleRate : 48000.0;
            tracker.prepare (sr);
            tracker.setRange (60.0f, 1100.0f);
            D = juce::jmax (1, juce::roundToInt (sr / 11025.0));
            fsd = sr / D;
            frameLen = 256; hopLen = 64;                                     // ~23 ms frame, ~6 ms hop at 11 kHz
            dec.assign ((size_t) frameLen * 2, 0.0f);
            frame.assign ((size_t) frameLen, 0.0f);
            tmp.reserve ((size_t) frameLen + 8);
            aa1.set (sr, juce::jmin (4800.0, fsd * 0.44)); aa2.set (sr, juce::jmin (4800.0, fsd * 0.44));
            maxPeriod = (int) std::ceil (sr / 60.0) + 4;
            hist.assign ((size_t) juce::nextPowerOfTwo (maxPeriod * 3 + juce::jmax (64, maxBlock) + 8), 0.0f);
            histMask = (int) hist.size() - 1;
            reset();
        }
        void reset()
        {
            tracker.reset(); aa1.reset(); aa2.reset();
            std::fill (dec.begin(), dec.end(), 0.0f); decW = 0; decCount = 0; hopCount = 0;
            std::fill (hist.begin(), hist.end(), 0.0f); hw = 0;
            snap = {}; voicedMix = 0; periodSmooth = 0; eH = eN = 1.0e-9f;
        }

        /** Analyse a block (mono). Cheap enough to run on the audio thread of every plug-in that needs it. */
        void analyse (const float* x, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                if (tracker.push (x[i]))
                {
                    const auto& f = tracker.latest();
                    snap.hz = f.voiced ? f.hz : 0.0f; snap.confidence = f.conf; snap.voiced = f.voiced; snap.sibilant = f.sibilant;
                    snap.rmsDb = juce::Decibels::gainToDecibels (f.rms, -120.0f); snap.position = f.pos;
                }
                const float y = aa2.lp (aa1.lp (x[i]));
                if (++decCount >= D)
                {
                    decCount = 0;
                    dec[(size_t) (decW++ % (int) dec.size())] = y;
                    if (++hopCount >= hopLen) { hopCount = 0; formantFrame(); }
                }
            }
        }

        /** Harmonic / noise split (mono, in -> harm + noise, both delayed by splitLatency()). Call after analyse(). */
        void split (const float* in, float* harm, float* noise, int n)
        {
            const float targetPeriod = snap.voiced && snap.hz > 0 ? (float) (sr / snap.hz) : 0.0f;
            const float targetMix = snap.voiced ? juce::jlimit (0.0f, 1.0f, (snap.confidence - 0.3f) / 0.5f) : 0.0f;
            const float k = (float) (1.0 - std::exp (-1.0 / (0.008 * sr)));
            for (int i = 0; i < n; ++i)
            {
                hist[(size_t) (hw & histMask)] = in[i];
                if (targetPeriod > 0) periodSmooth = periodSmooth <= 0 ? targetPeriod : periodSmooth + k * (targetPeriod - periodSmooth);
                voicedMix += k * (targetMix - voicedMix);
                const double centre = (double) hw - maxPeriod;
                const float c = at (centre);
                float h = 0;
                if (periodSmooth > 2.0f)
                {
                    const float T = juce::jmin (periodSmooth, (float) maxPeriod - 2.0f);
                    h = 0.25f * at (centre - T) + 0.5f * c + 0.25f * at (centre + T);   // zero at odd multiples of f0/2
                }
                const float hh = h * voicedMix;
                harm[i] = hh; noise[i] = c - hh;
                eH = 0.9995f * eH + 0.0005f * hh * hh; eN = 0.9995f * eN + 0.0005f * (c - hh) * (c - hh);
                ++hw;
            }
            snap.hnrDb = 10.0f * std::log10 ((eH + 1.0e-12f) / (eN + 1.0e-12f));
        }

        /** The singer's range for the tracker (Hz). */
        void setRange (float lo, float hi)  { tracker.setRange (lo, hi); }
        const Snapshot& latest() const      { return snap; }
        const SqVocal::PitchTracker& pitchTracker() const { return tracker; }
        int analysisLag() const             { return tracker.lagSamples(); }

    private:
        double sr = 48000, fsd = 11025; int D = 4, frameLen = 256, hopLen = 64, decW = 0, decCount = 0, hopCount = 0;
        SqVocal::PitchTracker tracker;
        SqVocal::Svf aa1, aa2;
        std::vector<float> dec, frame, tmp, hist; int histMask = 0; juce::int64 hw = 0; int maxPeriod = 800;
        Snapshot snap; float voicedMix = 0, periodSmooth = 0, eH = 1.0e-9f, eN = 1.0e-9f;

        float at (double pos) const
        {
            const juce::int64 i = (juce::int64) std::floor (pos); const float f = (float) (pos - (double) i);
            const float a = hist[(size_t) (i & histMask)], b = hist[(size_t) ((i + 1) & histMask)];
            return a + (b - a) * f;
        }

        /** LPC (order 12 at 11 kHz) -> spectral envelope -> the four lowest peaks between 150 Hz and 4.8 kHz. */
        void formantFrame()
        {
            for (int i = 0; i < frameLen; ++i) frame[(size_t) i] = dec[(size_t) ((decW - frameLen + i + (int) dec.size() * 4) % (int) dec.size())];
            constexpr int order = 12; float k[SqVocal::Lpc::maxOrder] {}; float err = 0;
            if (! SqVocal::Lpc::analyse (frame.data(), frameLen, order, fsd, k, err, tmp, 0.0) || ! snap.voiced)
            { std::fill (std::begin (snap.formants), std::end (snap.formants), 0.0f); return; }
            // reflection -> direct form (step-up)
            double a[order + 1] {}, t[order + 1] {}; a[0] = 1;
            for (int m = 1; m <= order; ++m)
            {
                for (int i = 0; i <= m; ++i) t[i] = a[i];
                for (int i = 1; i < m; ++i) a[i] = t[i] + k[m - 1] * t[m - i];
                a[m] = k[m - 1];
            }
            constexpr int bins = 192; double env[bins];
            for (int b = 0; b < bins; ++b)
            {
                const double w = juce::MathConstants<double>::pi * (double) b / (bins - 1) * juce::jmin (1.0, 4800.0 / (fsd * 0.5));
                double re = 0, im = 0; for (int i = 0; i <= order; ++i) { re += a[i] * std::cos (w * i); im -= a[i] * std::sin (w * i); }
                env[b] = -10.0 * std::log10 (re * re + im * im + 1e-30);
            }
            int found = 0; float fm[4] {};
            const double hzPerBin = juce::jmin (4800.0, fsd * 0.5) / (bins - 1);
            for (int b = 1; b < bins - 1 && found < 4; ++b)
            {
                if (env[b] > env[b - 1] && env[b] >= env[b + 1])
                {
                    const double d = 0.5 * (env[b - 1] - env[b + 1]) / (env[b - 1] - 2 * env[b] + env[b + 1] - 1e-12);   // parabolic
                    const double hz = (b + juce::jlimit (-0.5, 0.5, d)) * hzPerBin;
                    if (hz > 150.0) fm[found++] = (float) hz;
                }
            }
            for (int i = 0; i < 4; ++i) snap.formants[i] = fm[i];
        }
    };
}
