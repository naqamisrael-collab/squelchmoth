#pragma once
// Molt - Squelchmoth's time & pitch processor (the X-Form-class AudioSuite tool), offline DSP.
//
//  Polyphonic       spectral stretch (Signalsmith Stretch, MIT) - chords, mixes, drums, anything
//  Poly (Faster)    the same engine with larger hops - quick previews and long files
//  Monophonic       Squelchmoth's pitch-synchronous engine (PSOLA): formant-exact for vocals, bass and solo lines
//
//  Time is set as a ratio (from length, tempo or Shift %), pitch in semitones and cents (+/-36 st), formants
//  either follow the pitch or stay put (Formant In) with their own +/-36 st shift. Every channel of a multichannel
//  buffer is processed together, so stems stay phase-coherent. Transients: in the polyphonic modes the audio is
//  cut at its onsets and each piece is stretched from a fresh start, so attacks stay sharp ("Sensitivity").

#include <JuceHeader.h>
#include <tracktion_engine/3rd_party/signalsmith-stretch/signalsmith-stretch.h>
#include "SqVocalDsp.h"

namespace MoltDsp
{
    struct Params
    {
        double timeRatio = 1.0;      // output length / input length
        double semitones = 0.0;      // including cents (e.g. 2.5 = 2 st 50 ct)
        bool formantIn = false;      // keep formants where they are (then shift them by formantSt)
        double formantSt = 0.0;
        int type = 0;                // 0 Polyphonic, 1 Monophonic, 2 Poly (Faster)
        double sensitivity = 50.0;   // 0-100: transient sensitivity (polyphonic modes; 0 = smooth, never cut)
        double windowMs = 25.0;      // 10-100 ms: monophonic analysis window (longest period it looks for)
        double gainDb = 0.0;         // 0 to -6 dB input attenuation
    };

    inline juce::StringArray typeNames()   { return { "Polyphonic", "Monophonic", "Poly (Faster)" }; }

    /** Onsets (sample positions) found by a rise in short-term energy; sens 0-100. */
    inline std::vector<int> findOnsets (const juce::AudioBuffer<float>& in, double sr, double sens, int minGap)
    {
        std::vector<int> out;
        if (sens <= 0.5) return out;
        const int hop = juce::jmax (32, (int) (sr * 0.005)), n = in.getNumSamples();
        std::vector<float> e;
        for (int i = 0; i + hop <= n; i += hop)
        {
            double s = 0;
            for (int c = 0; c < in.getNumChannels(); ++c) { auto* p = in.getReadPointer (c, i); for (int k = 0; k < hop; ++k) s += p[k] * p[k]; }
            e.push_back ((float) (10.0 * std::log10 (s / (hop * in.getNumChannels()) + 1e-12)));
        }
        const float thr = (float) (3.0 + (100.0 - sens) * 0.12);
        int last = -minGap;
        for (size_t k = 6; k < e.size(); ++k)
        {
            float mn = e[k - 6]; for (size_t j = k - 6; j < k; ++j) mn = juce::jmin (mn, e[j]);
            if (e[k] - mn > thr && e[k] > -55.0f && (int) k * hop - last >= minGap)
            {
                // refine to the steepest rise
                last = (int) k * hop;
                out.push_back (juce::jmax (0, last - hop));
            }
        }
        return out;
    }

    /** One spectral stretch of a whole buffer (exactly outLen samples out). */
    inline void stretchExact (const juce::AudioBuffer<float>& in, int start, int len, double sr, const Params& p, bool cheaper, juce::AudioBuffer<float>& out, int outLen)
    {
        const int nch = in.getNumChannels();
        signalsmith::stretch::SignalsmithStretch<float> st (42);
        if (cheaper) st.presetCheaper (nch, (float) sr); else st.presetDefault (nch, (float) sr);
        st.setTransposeSemitones ((float) p.semitones, 8000.0f / (float) sr);
        if (p.formantIn) { st.setFormantSemitones ((float) p.formantSt, true); st.setFormantBase (0); }
        else st.setFormantFactor (1.0f, false);
        std::vector<const float*> ip ((size_t) nch);
        std::vector<float*> op ((size_t) nch);
        for (int c = 0; c < nch; ++c) { ip[(size_t) c] = in.getReadPointer (c, start); op[(size_t) c] = out.getWritePointer (c); }
        out.setSize (nch, outLen, false, false, true);
        for (int c = 0; c < nch; ++c) op[(size_t) c] = out.getWritePointer (c);
        if (! st.exact (ip.data(), len, op.data(), outLen))
        {
            // too short for the spectral engine: plain resample-and-hold fallback
            for (int c = 0; c < nch; ++c)
                for (int i = 0; i < outLen; ++i)
                {
                    const double src = (double) i / juce::jmax (1, outLen) * len;
                    const int a = juce::jlimit (0, len - 1, (int) src);
                    op[(size_t) c][i] = ip[(size_t) c][a];
                }
        }
    }

    /** Monophonic engine: pitch-synchronous overlap-add over the whole buffer. */
    inline juce::AudioBuffer<float> psola (const juce::AudioBuffer<float>& in, double sr, const Params& p, int outLen)
    {
        const int nch = in.getNumChannels(), n = in.getNumSamples();
        juce::AudioBuffer<float> out (nch, outLen); out.clear();
        if (n < 16) return out;
        // pitch track
        SqVocal::PitchTracker tr; tr.prepare (sr);
        const float lo = (float) juce::jlimit (30.0, 200.0, 1000.0 / juce::jlimit (10.0, 100.0, p.windowMs));
        tr.setRange (lo, 1600.0f); tr.setTracking (55.0f);
        std::vector<std::pair<int, float>> frames;   // pos, period (0 = unvoiced)
        std::vector<float> lp ((size_t) n);
        float l1 = 0, l2 = 0;
        for (int i = 0; i < n; ++i)
        {
            float m = 0; for (int c = 0; c < nch; ++c) m += in.getSample (c, i); m /= (float) nch;
            if (tr.push (m)) { auto& f = tr.latest(); frames.push_back ({ (int) f.pos, f.voiced ? (float) (sr / f.hz) : 0.0f }); }
            l1 += 0.06f * (m - l1); l2 += 0.06f * (l1 - l2); lp[(size_t) i] = l2;
        }
        auto periodAt = [&] (int pos) -> float
        {
            if (frames.empty()) return 0.0f;
            auto it = std::lower_bound (frames.begin(), frames.end(), pos, [] (const std::pair<int, float>& a, int v) { return a.first < v; });
            if (it == frames.end()) --it;
            return it->second;
        };
        // analysis marks
        struct Mark { int pos; float T; bool voiced; };
        std::vector<Mark> marks;
        const float Tu = (float) (sr * juce::jlimit (0.004, 0.02, p.windowMs * 0.0005));
        int pos = 0;
        marks.push_back ({ 0, Tu, false });
        while (pos < n)
        {
            const float T = periodAt (pos);
            const bool v = T > 0;
            const float step = v ? T : Tu;
            int next = pos + (int) std::round (step);
            if (v)
            {
                int best = next; float bv = -1e9f;
                for (int k = next - (int) (T * 0.25f); k <= next + (int) (T * 0.25f); ++k)
                    if (k > pos + (int) (T * 0.6f) && k < n && lp[(size_t) k] > bv) { bv = lp[(size_t) k]; best = k; }
                next = best;
            }
            if (next >= n) break;
            marks.push_back ({ next, step, v });
            pos = next;
        }
        // synthesis
        const double pr = std::pow (2.0, p.semitones / 12.0);
        const double g = p.formantIn ? std::pow (2.0, p.formantSt / 12.0) : pr;
        double ts = 0; size_t mi = 0;
        while (ts < outLen)
        {
            const double ta = ts / p.timeRatio;
            while (mi + 1 < marks.size() && std::abs (marks[mi + 1].pos - ta) <= std::abs (marks[mi].pos - ta)) ++mi;
            while (mi > 0 && std::abs (marks[mi - 1].pos - ta) < std::abs (marks[mi].pos - ta)) --mi;
            const auto& mk = marks[mi];
            float T = mk.T;
            if (mi > 0 && mi + 1 < marks.size()) T = juce::jlimit (4.0f, mk.T * 1.6f, 0.5f * (float) (marks[mi + 1].pos - marks[mi - 1].pos));
            const double gg = mk.voiced ? juce::jlimit (0.4, 2.5, g) : 1.0;
            const double Lo = juce::jlimit ((double) T * 0.5, (double) T * 1.25, (double) T / gg);
            const int k0 = juce::jmax (0, (int) std::ceil (ts - Lo)), k1 = juce::jmin (outLen - 1, (int) std::floor (ts + Lo));
            for (int k = k0; k <= k1; ++k)
            {
                const double u = (k - ts) / Lo;
                const float w = 0.5f + 0.5f * (float) std::cos (juce::MathConstants<double>::pi * u);
                const double ip = mk.pos + (k - ts) * gg;
                const int i0 = (int) std::floor (ip); const float fr = (float) (ip - i0);
                if (i0 < 0 || i0 + 1 >= n) continue;
                for (int c = 0; c < nch; ++c)
                {
                    auto* src = in.getReadPointer (c);
                    out.getWritePointer (c)[k] += w * (src[i0] + (src[i0 + 1] - src[i0]) * fr);
                }
            }
            ts += mk.voiced ? T / pr : T;
        }
        return out;
    }

    /** Process a whole multichannel buffer. progress (0..1) may return false to cancel. */
    inline juce::AudioBuffer<float> process (const juce::AudioBuffer<float>& inRaw, double sr, const Params& p, float* peakOut = nullptr)
    {
        juce::AudioBuffer<float> in (inRaw);
        in.applyGain (juce::Decibels::decibelsToGain ((float) juce::jlimit (-6.0, 0.0, p.gainDb)));
        const int n = in.getNumSamples();
        const int outLen = juce::jmax (1, (int) std::llround (n * p.timeRatio));
        juce::AudioBuffer<float> out (in.getNumChannels(), outLen);
        if (p.type == 1) out = psola (in, sr, p, outLen);
        else
        {
            const bool cheaper = p.type == 2;
            auto onsets = findOnsets (in, sr, p.sensitivity, (int) (sr * 0.22));
            std::vector<int> cuts { 0 };
            for (int o : onsets) if (o - cuts.back() >= (int) (sr * 0.22) && n - o >= (int) (sr * 0.22)) cuts.push_back (o);
            cuts.push_back (n);
            out.clear();
            const int xf = (int) (sr * 0.005);
            for (size_t k = 0; k + 1 < cuts.size(); ++k)
            {
                const int s0 = k == 0 ? 0 : juce::jmax (0, cuts[k] - xf), s1 = cuts[k + 1];
                const int o0 = (int) std::llround (s0 * p.timeRatio);
                const int oLen = k + 2 == cuts.size() ? outLen - o0 : (int) std::llround (s1 * p.timeRatio) - o0;
                if (oLen <= 0) continue;
                juce::AudioBuffer<float> seg;
                stretchExact (in, s0, s1 - s0, sr, p, cheaper, seg, oLen);
                const int fadeLen = k == 0 ? 0 : (int) std::llround (xf * p.timeRatio);
                for (int c = 0; c < out.getNumChannels(); ++c)
                {
                    auto* d = out.getWritePointer (c); auto* sp = seg.getReadPointer (c);
                    for (int i = 0; i < oLen && o0 + i < outLen; ++i)
                    {
                        if (i < fadeLen) { const float t = (float) i / (float) fadeLen; d[o0 + i] = d[o0 + i] * (1.0f - t) + sp[i] * t; }
                        else d[o0 + i] = sp[i];
                    }
                }
            }
        }
        if (peakOut != nullptr) *peakOut = out.getMagnitude (0, out.getNumSamples());
        return out;
    }

    //==========================================================================
    // Time helpers (as the X-Form time section shows them)
    inline double ratioFromShiftPercent (double pct)        { return pct / 100.0; }
    inline double ratioFromTempo (double fromBpm, double toBpm) { return toBpm > 0 ? fromBpm / toBpm : 1.0; }
    struct PresetFactor { const char* name; double timePct; double semitones; bool formantIn; double formantSt; };
    inline const std::vector<PresetFactor>& presets()
    {
        static const std::vector<PresetFactor> p {
            { "Pull-down 0.1% (Film/24 > NTSC 23.976)",  100.1, 0, false, 0 },
            { "Pull-up 0.1% (NTSC 23.976 > Film/24)",    99.9000999001, 0, false, 0 },
            { "PAL 25 > NTSC 23.976 (4.27% longer)",     104.2708333333, 0, false, 0 },
            { "NTSC 23.976 > PAL 25 (4.1% shorter)",     95.904, 0, false, 0 },
            { "PAL 25 > Film 24 (4.17% longer)",         104.1666666667, 0, false, 0 },
            { "Film 24 > PAL 25 (4% shorter)",           96.0, 0, false, 0 },
            { "Double time (half length)",          50.0, 0, false, 0 },
            { "Half time (double length)",          200.0, 0, false, 0 },
            { "Octave up, natural voice",           100.0, 12, true, 0 },
            { "Octave down, natural voice",         100.0, -12, true, 0 },
            { "Chipmunk",                           100.0, 7, false, 0 },
            { "Monster",                            100.0, -7, true, -6 },
            { "Gender: deeper",                     100.0, -3, true, -3 },
            { "Gender: brighter",                   100.0, 3, true, 3 },
            { "Tape stop feel (slow 50%)",          200.0, -12, false, 0 } };
        return p;
    }
}
