#pragma once
// Squelchmoth 1.15 "Beat Intelligence" - L1: the Reference Listener's DSP core (te-free, tested by tests/reference_test.cpp).
//
// Hears a finished instrumental the way a producer does, from the stereo mix alone:
//   tempo + beat grid (onset envelope -> tempogram -> dynamic-programming beat tracker), downbeats and bar grid,
//   swing, a 16-step drum grid per bar (kick / snare-clap / hat bands, hat rolls), key (chroma vs key profiles),
//   chords per half bar (templates incl. 7ths / sus, bass-weighted) as Roman numerals, sections (bar self-similarity +
//   novelty, repeated parts get the same letter, named Intro / Hook / Verse / Bridge / Outro by energy and repetition),
//   the sub / 808 line (pitch, glides, octave jumps), the element count per section, and the mix profile (spectral
//   balance per band, per-band stereo width, sidechain pump depth).
// Everything is measured; nothing of the recording is stored or copied (super prompt 1.15 section 2).

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <map>

namespace SqRef
{
    constexpr double analysisRate = 22050.0;
    constexpr int fftOrder = 11, fftSize = 1 << fftOrder, hop = 256;   // 93 ms window, 11.6 ms hop
    constexpr double onsetLag = 0.024;   // a flux peak lands this long before the attack's frame centre (measured on test hits)

    static const char* noteNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    //==========================================================================
    struct DrumBar { std::array<float, 16> kick {}, snare {}, hat {}; int hatRolls = 0; };   // 0..1 hit strength per step
    struct Chord   { double startSec = 0; int root = -1; juce::String quality, name, roman; float confidence = 0; };
    struct Section { int startBar = 0, bars = 0; char letter = 'A'; juce::String name; float energyDb = -100; int elements = 0; juce::StringArray elementList; };
    struct SubNote { double startSec = 0, lengthSec = 0; float midi = 0; bool glide = false; };

    struct Result
    {
        bool ok = false; juce::String error;
        double seconds = 0, sampleRate = 0;
        double bpm = 0, bpmConfidence = 0; juce::Array<double> bpmCandidates;
        std::vector<double> beats;          // seconds
        int downbeatPhase = 0;              // beat index of the first downbeat
        std::vector<double> bars;           // bar start times (seconds)
        double swing = 0.5;                 // 0.5 = straight 16ths, 0.66 = triplet swing (off-16th position in the 8th)
        int keyRoot = 0; bool minor = false; float keyConfidence = 0; juce::String keyName;
        std::vector<Chord> chords;
        std::vector<DrumBar> drums;
        std::vector<Section> sections;
        std::vector<SubNote> sub;
        int subGlides = 0, subOctaveJumps = 0; float subPresence = 0;   // sub share of total energy
        // mix profile
        std::array<float, 6> bandDb {};     // sub 20-60, low 60-250, low-mid 250-2k, presence 2-6k, air 6-11k (rel. to total), + total dB
        std::array<float, 3> widthBands {}; // side/mid energy ratio: < 150 Hz, 150 Hz - 2 kHz, > 2 kHz
        float pumpDb = 0;                   // gain swing at the beat period in the 300 Hz - 3 kHz band (sidechain feel)
        float hatDensity = 0;               // hat hits per beat (average)
    };

    //==========================================================================
    namespace detail
    {
        inline std::vector<float> resample (const float* in, int n, double fromRate, double toRate)
        {
            if (std::abs (fromRate - toRate) < 1.0) return std::vector<float> (in, in + n);
            // anti-alias low-pass (simple windowed-sinc FIR) then linear interpolation
            const double ratio = fromRate / toRate;
            std::vector<float> filtered (in, in + n);
            if (ratio > 1.0)
            {
                const int taps = 63; const double fc = 0.45 / ratio; std::vector<float> h (taps); double sum = 0;
                for (int i = 0; i < taps; ++i) { const double x = i - (taps - 1) / 2.0; const double w = 0.42 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (taps - 1)) + 0.08 * std::cos (4 * juce::MathConstants<double>::pi * i / (taps - 1));
                    h[(size_t) i] = (float) ((x == 0 ? 2 * fc : std::sin (2 * juce::MathConstants<double>::pi * fc * x) / (juce::MathConstants<double>::pi * x)) * w); sum += h[(size_t) i]; }
                for (auto& v : h) v = (float) (v / sum);
                for (int i = 0; i < n; ++i) { double acc = 0; for (int k = 0; k < taps; ++k) { const int j = i - k + taps / 2; if (j >= 0 && j < n) acc += h[(size_t) k] * in[j]; } filtered[(size_t) i] = (float) acc; }
            }
            const int outN = (int) std::floor ((n - 1) / ratio);
            std::vector<float> out ((size_t) juce::jmax (0, outN));
            for (int i = 0; i < outN; ++i) { const double p = i * ratio; const int a = (int) p; const double f = p - a; out[(size_t) i] = (float) (filtered[(size_t) a] * (1 - f) + filtered[(size_t) juce::jmin (n - 1, a + 1)] * f); }
            return out;
        }
        inline float percentile (std::vector<float> v, float q)
        {
            if (v.empty()) return 0;
            const size_t k = (size_t) std::min<double> ((double) v.size() - 1, std::floor (juce::jlimit (0.0f, 1.0f, q) * (double) (v.size() - 1)));
            std::nth_element (v.begin(), v.begin() + (long) k, v.end()); return v[k];
        }
        inline double hzToMidi (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
    }

    //==========================================================================
    /** Frame features computed once from the STFT (the spectra themselves are not kept). */
    struct Frames
    {
        int count = 0; double frameRate = analysisRate / hop;
        std::vector<float> flux, fluxKick, fluxSnare, fluxHat;          // onset strengths
        std::vector<float> eKick, eSnare, eHat, eSub, eMid, eAll;         // band energies (linear)
        std::vector<std::array<float, 12>> chroma, bassChroma;
        std::vector<float> hatFast; double hatFastRate = 0;   // fine-grained high-band onset curve (2.9 ms) for rolls
        std::vector<float> eK, eB, eS, eH;                     // fine band energies (2.9 ms blocks): kick 35-120, body 150-400, snare 1-5k, hat 6k+
        std::vector<float> tonalness;                                     // chroma peakiness (melodic content)
        std::vector<float> sustainMid;                                    // slowly varying mid energy (pads)
    };

    inline Frames computeFrames (const std::vector<float>& mono)
    {
        Frames f;
        juce::dsp::FFT fft (fftOrder);
        std::vector<float> window ((size_t) fftSize), buf ((size_t) fftSize * 2), prevMag ((size_t) fftSize / 2 + 1, 0.0f);
        for (int i = 0; i < fftSize; ++i) window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (fftSize - 1)));
        const int n = (int) mono.size();
        f.count = juce::jmax (0, (n - fftSize) / hop + 1);
        auto resize = [&] (auto& v) { v.resize ((size_t) f.count); };
        resize (f.flux); resize (f.fluxKick); resize (f.fluxSnare); resize (f.fluxHat);
        resize (f.eKick); resize (f.eSnare); resize (f.eHat); resize (f.eSub); resize (f.eMid); resize (f.eAll);
        resize (f.chroma); resize (f.bassChroma); resize (f.tonalness); resize (f.sustainMid);
        const double binHz = analysisRate / fftSize;
        auto bin = [&] (double hz) { return juce::jlimit (1, fftSize / 2, (int) std::round (hz / binHz)); };
        const int kLo = bin (35), kHi = bin (130), sLo = bin (180), sHi = bin (4500), hLo = bin (6500), hHi = bin (10500), subHi = bin (100), mLo = bin (300), mHi = bin (3000);
        const int sfLo = bin (1200), sfHi = bin (6000);   // snare onset band (above most melodic fundamentals)
        // pitch-class map for chroma bins (80 Hz - 4 kHz melodic, 35-200 Hz bass)
        std::vector<int> pc ((size_t) fftSize / 2 + 1, -1);
        for (int b = 1; b <= fftSize / 2; ++b) { const double hz = b * binHz; if (hz >= 35 && hz <= 4200) pc[(size_t) b] = ((int) std::lround (detail::hzToMidi (hz)) % 12 + 12) % 12; }
        float sustain = 0;
        for (int fr = 0; fr < f.count; ++fr)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < fftSize; ++i) buf[(size_t) i] = mono[(size_t) (fr * hop + i)] * window[(size_t) i];
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            float fl = 0, fk = 0, fs = 0, fh = 0, ek = 0, es = 0, eh = 0, esub = 0, emid = 0, eall = 0;
            int sPos = 0, hPos = 0;
            std::array<float, 12> ch {}, bch {};
            for (int b = 1; b <= fftSize / 2; ++b)
            {
                const float m = buf[(size_t) b], e = m * m;
                const float lm = std::log1p (100.0f * m), lp = std::log1p (100.0f * prevMag[(size_t) b]);
                const float d = juce::jmax (0.0f, lm - lp);
                fl += d; eall += e;
                if (b >= kLo && b <= kHi) { fk += d; ek += e; }
                if (b >= sLo && b <= sHi) es += e;
                if (b >= sfLo && b <= sfHi) { fs += d; if (d > 0.15f) ++sPos; }
                if (b >= hLo && b <= hHi) { fh += d; eh += e; if (d > 0.15f) ++hPos; }
                if (b <= subHi) esub += e;
                if (b >= mLo && b <= mHi) emid += e;
                if (const int p = pc[(size_t) b]; p >= 0)
                {
                    const double hz = b * binHz;
                    if (hz >= 80) ch[(size_t) p] += m;
                    if (hz >= 70 && hz <= 260) bch[(size_t) p] += m;   // below ~70 Hz the bins are wider than a semitone: the sub tracker covers that
                }
                prevMag[(size_t) b] = m;
            }
            const float sFrac = (float) sPos / (float) (sfHi - sfLo + 1), hFrac = (float) hPos / (float) (hHi - hLo + 1);
            fs *= sFrac * sFrac * 4.0f; fh *= juce::jmin (1.0f, hFrac * 2.0f);   // broadband (percussive) onsets only
            f.flux[(size_t) fr] = fl; f.fluxKick[(size_t) fr] = fk; f.fluxSnare[(size_t) fr] = fs; f.fluxHat[(size_t) fr] = fh;
            f.eKick[(size_t) fr] = ek; f.eSnare[(size_t) fr] = es; f.eHat[(size_t) fr] = eh; f.eSub[(size_t) fr] = esub; f.eMid[(size_t) fr] = emid; f.eAll[(size_t) fr] = eall;
            float cs = 0, cmax = 0; for (auto v : ch) { cs += v; cmax = juce::jmax (cmax, v); }
            f.tonalness[(size_t) fr] = cs > 1e-9f ? cmax / cs * 12.0f / 12.0f : 0.0f;
            f.chroma[(size_t) fr] = ch; f.bassChroma[(size_t) fr] = bch;
            sustain = 0.97f * sustain + 0.03f * emid; f.sustainMid[(size_t) fr] = sustain;
        }
        return f;
    }

    //==========================================================================
    /** Tempo from the onset envelope's autocorrelation, weighted towards 75-160 BPM (where hip-hop / R&B / pop live), with
        half / double candidates reported. */
    inline void estimateTempo (const std::vector<float>& onset, double frameRate, Result& r)
    {
        const int n = (int) onset.size();
        if (n < 64) return;
        // normalise: subtract local mean
        std::vector<float> o ((size_t) n);
        const float mean = std::accumulate (onset.begin(), onset.end(), 0.0f) / (float) n;
        for (int i = 0; i < n; ++i) o[(size_t) i] = juce::jmax (0.0f, onset[(size_t) i] - mean);
        const int minLag = (int) std::floor (frameRate * 60.0 / 200.0), maxLag = (int) std::ceil (frameRate * 60.0 / 55.0);
        std::vector<double> ac ((size_t) maxLag + 2, 0.0);
        for (int lag = minLag; lag <= maxLag + 1 && lag < n; ++lag) { double s = 0; for (int i = lag; i < n; ++i) s += (double) o[(size_t) i] * o[(size_t) (i - lag)]; ac[(size_t) lag] = s / (n - lag); }
        // comb-enhanced score: a tempo whose 2x and 4x lags also agree scores higher (meter-aware), with a perceptual prior
        auto acAt = [&] (double lag) { const int a = (int) lag; if (a < 1 || a + 1 >= (int) ac.size()) return 0.0; const double fr = lag - a; return ac[(size_t) a] * (1 - fr) + ac[(size_t) a + 1] * fr; };
        double best = 0, bestLag = 0;
        std::vector<std::pair<double, double>> scored;
        for (double lag = minLag; lag <= maxLag; lag += 0.25)
        {
            const double bpm = 60.0 * frameRate / lag;
            const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 110.0) / 0.9, 2.0));
            const double s = (acAt (lag) + 0.5 * acAt (2 * lag) + 0.25 * acAt (4 * lag) + 0.3 * acAt (lag / 2)) * prior;
            scored.push_back ({ s, bpm });
            if (s > best) { best = s; bestLag = lag; }
        }
        if (bestLag <= 0) return;
        r.bpm = 60.0 * frameRate / bestLag;
        double total = 0; for (auto& s : scored) total += s.first;
        r.bpmConfidence = total > 0 ? best / (total / (double) scored.size()) / 10.0 : 0;
        r.bpmCandidates = { r.bpm, r.bpm * 2, r.bpm / 2 };
    }

    /** Beat tracking by dynamic programming (penalising deviation from the tempo period), then refine BPM from the beats. */
    inline void trackBeats (const std::vector<float>& onset, double frameRate, Result& r)
    {
        const int n = (int) onset.size();
        if (r.bpm <= 0 || n < 16) return;
        const double period = 60.0 * frameRate / r.bpm;
        std::vector<double> score ((size_t) n), o ((size_t) n);
        double mx = 1e-9; for (int i = 0; i < n; ++i) { o[(size_t) i] = onset[(size_t) i]; mx = std::max (mx, o[(size_t) i]); }
        for (auto& v : o) v /= mx;
        std::vector<int> back ((size_t) n, -1);
        const double tightness = 100.0;
        for (int i = 0; i < n; ++i)
        {
            double bestPrev = 0; int bp = -1;
            const int lo = i - (int) std::round (2.0 * period), hi = i - (int) std::round (0.5 * period);
            for (int j = juce::jmax (0, lo); j <= hi && j < i; ++j)
            {
                const double dev = std::log ((i - j) / period);
                const double s = score[(size_t) j] - tightness * dev * dev;
                if (bp < 0 || s > bestPrev) { bestPrev = s; bp = j; }
            }
            score[(size_t) i] = o[(size_t) i] + (bp >= 0 ? bestPrev : 0);
            back[(size_t) i] = bp;
        }
        // end at the best score within the last period
        int end = n - 1; double bs = -1e300;
        for (int i = juce::jmax (0, n - (int) period - 1); i < n; ++i) if (score[(size_t) i] > bs) { bs = score[(size_t) i]; end = i; }
        std::vector<int> idx; for (int i = end; i >= 0; i = back[(size_t) i]) { idx.push_back (i); if (back[(size_t) i] < 0) break; }
        std::reverse (idx.begin(), idx.end());
        r.beats.clear();
        for (auto i : idx) r.beats.push_back ((i * hop + fftSize / 2) / analysisRate + onsetLag + 0.007);   // +7 ms: the measured median beat offset on the test songs
        // refined tempo: median inter-beat interval
        if (r.beats.size() > 8)
        {
            std::vector<double> ibi; for (size_t i = 1; i < r.beats.size(); ++i) ibi.push_back (r.beats[i] - r.beats[i - 1]);
            std::nth_element (ibi.begin(), ibi.begin() + (long) ibi.size() / 2, ibi.end());
            const double med = ibi[ibi.size() / 2];
            // least-squares slope over the beats for sub-frame precision
            const double nb = (double) r.beats.size(); double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (size_t i = 0; i < r.beats.size(); ++i) { sx += (double) i; sy += r.beats[i]; sxx += (double) i * i; sxy += (double) i * r.beats[i]; }
            const double slope = (nb * sxy - sx * sy) / (nb * sxx - sx * sx);
            r.bpm = std::abs (slope - med) < 0.05 * med ? 60.0 / slope : 60.0 / med;
            // the tracker's first few beats settle late (it has no past to lean on): pull any beat that sits off the
            // grid its neighbours agree on back onto it, from the 5th beat backwards
            const double per = 60.0 / r.bpm;
            for (size_t i = juce::jmin ((size_t) 4, r.beats.size() - 3) + 1; i-- > 0;)
                if (std::abs (r.beats[i] - (r.beats[i + 1] - per)) > 0.02 && std::abs (r.beats[i + 1] - (r.beats[i + 2] - per)) < 0.02) r.beats[i] = r.beats[i + 1] - per;
            while (! r.beats.empty() && r.beats.front() < 0) r.beats.erase (r.beats.begin());
        }
    }

    /** Downbeat: the beat phase (mod 4) with the strongest kick + low-end onsets and the most chord changes. */
    inline void findDownbeats (const Frames& f, Result& r)
    {
        if (r.beats.size() < 8) return;
        auto frameOf = [&] (double t) { return juce::jlimit (0, f.count - 1, (int) std::lround ((t - onsetLag) * analysisRate / hop - (double) fftSize / (2 * hop))); };
        std::array<double, 4> sc {};
        for (size_t b = 0; b < r.beats.size(); ++b)
        {
            const int fr = frameOf (r.beats[b]);
            double k = 0; for (int d = -2; d <= 2; ++d) { const int i = juce::jlimit (0, f.count - 1, fr + d); k = std::max (k, (double) f.fluxKick[(size_t) i]); }
            // chroma change into this beat
            const int pr = frameOf (b > 0 ? r.beats[b - 1] : 0.0);
            double cd = 0; for (int p = 0; p < 12; ++p) cd += std::abs (f.chroma[(size_t) fr][(size_t) p] - f.chroma[(size_t) pr][(size_t) p]);
            sc[b % 4] += k + 0.02 * cd;
        }
        int best = 0; for (int p = 1; p < 4; ++p) if (sc[(size_t) p] > sc[(size_t) best]) best = p;
        r.downbeatPhase = best;
        r.bars.clear();
        for (size_t b = (size_t) best; b < r.beats.size(); b += 4) r.bars.push_back (r.beats[b]);
        // a pickup before the first full bar: extrapolate back
        if (! r.bars.empty() && r.bars.front() > 60.0 / r.bpm * 4 * 0.75) r.bars.insert (r.bars.begin(), r.bars.front() - 240.0 / r.bpm);
    }

    //==========================================================================
    /** Key by correlating the whole-song chroma (plus bass chroma) with Krumhansl-Kessler profiles, 24 keys. */
    inline void estimateKey (const Frames& f, Result& r)
    {
        static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        std::array<double, 12> c {};
        for (int i = 0; i < f.count; ++i)
        {
            double s = 0; for (auto v : f.chroma[(size_t) i]) s += v;
            double bs = 0; for (auto v : f.bassChroma[(size_t) i]) bs += v;
            if (s <= 1e-9) continue;
            for (int p = 0; p < 12; ++p) c[(size_t) p] += f.chroma[(size_t) i][(size_t) p] / s + (bs > 1e-9 ? 0.5 * f.bassChroma[(size_t) i][(size_t) p] / bs : 0.0);
        }
        auto corr = [] (const std::array<double, 12>& x, const double* prof, int root)
        {
            double mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += x[(size_t) i]; my += prof[i]; } mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i) { const double a = x[(size_t) ((i + root) % 12)] - mx, b = prof[i] - my; sxy += a * b; sxx += a * a; syy += b * b; }
            return sxy / std::sqrt (sxx * syy + 1e-12);
        };
        double best = -2, second = -2; int bk = 0; bool bm = false;
        for (int k = 0; k < 12; ++k)
            for (int m = 0; m < 2; ++m)
            {
                const double s = corr (c, m ? mnr : maj, k);
                if (s > best) { second = best; best = s; bk = k; bm = m == 1; } else if (s > second) second = s;
            }
        r.keyRoot = bk; r.minor = bm; r.keyConfidence = (float) juce::jlimit (0.0, 1.0, (best - second) * 5.0 + 0.5 * best);
        r.keyName = juce::String (noteNames[bk]) + (bm ? " minor" : " major");
    }

    /** Roman numeral of a chord in the key (minor keys counted from the minor tonic: i, iv, v, VI, VII, III...). */
    inline juce::String romanFor (int root, const juce::String& q, int keyRoot, bool minorKey)
    {
        static const char* inMajor[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
        static const char* inMinor[12] = { "I", "bII", "II", "III", "#III", "IV", "#IV", "V", "VI", "#VI", "VII", "#VII" };
        const int deg = (root - keyRoot + 12) % 12;
        juce::String rn = (minorKey ? inMinor : inMajor)[deg];
        const bool minorQ = (q.startsWith ("m") && ! q.startsWith ("maj")) || q == "dim";
        if (minorQ) rn = rn.toLowerCase();
        if (q == "dim") return rn + "o";
        if (q == "m7") return rn + "7";
        if (q == "m") return rn;
        return rn + q;
    }

    /** Relative major / minor share the same notes; the chords decide: the tonic chord (right quality) at the start of the
        song and on 4-bar phrase starts, and how often it comes back, outweigh the profile correlation. */
    inline void refineKeyWithChords (Result& r)
    {
        if (r.chords.empty()) return;
        const int relRoot = r.minor ? (r.keyRoot + 3) % 12 : (r.keyRoot + 9) % 12;
        auto tonicScore = [&] (int root, bool minorKey)
        {
            double s = 0;
            const double barLen = 240.0 / juce::jmax (1.0, r.bpm);
            for (size_t i = 0; i < r.chords.size(); ++i)
            {
                const auto& c = r.chords[i];
                const bool isMinorQ = c.quality.startsWith ("m") && ! c.quality.startsWith ("maj");
                if (c.root != root || isMinorQ != minorKey) continue;
                const double barPos = r.bars.empty() ? 0.0 : (c.startSec - r.bars.front()) / barLen;
                const bool phraseStart = std::abs (barPos / 4.0 - std::round (barPos / 4.0)) < 0.05;
                s += 1.0 + (i == 0 ? 2.0 : 0.0) + (phraseStart ? 1.0 : 0.0);
            }
            return s;
        };
        const double cur = tonicScore (r.keyRoot, r.minor), rel = tonicScore (relRoot, ! r.minor);
        if (rel > cur * 1.15)
        {
            r.keyRoot = relRoot; r.minor = ! r.minor;
            r.keyName = juce::String (noteNames[r.keyRoot]) + (r.minor ? " minor" : " major");
        }
        for (auto& c : r.chords) c.roman = romanFor (c.root, c.quality, r.keyRoot, r.minor);
    }

    /** Chords per half bar: templates (maj, min, dim, sus2, sus4, maj7, m7, 7) on chroma, the bass weighted for the root. */
    inline void estimateChords (const Frames& f, Result& r)
    {
        r.chords.clear();
        if (r.bars.size() < 2) return;
        struct T { const char* q; std::vector<int> iv; };
        static const T templ[] = { { "", { 0, 4, 7 } }, { "m", { 0, 3, 7 } }, { "dim", { 0, 3, 6 } }, { "sus2", { 0, 2, 7 } }, { "sus4", { 0, 5, 7 } },
                                   { "maj7", { 0, 4, 7, 11 } }, { "m7", { 0, 3, 7, 10 } }, { "7", { 0, 4, 7, 10 } } };
        auto frameOf = [&] (double t) { return juce::jlimit (0, f.count - 1, (int) std::lround ((t - onsetLag) * analysisRate / hop - (double) fftSize / (2 * hop))); };
        const double barLen = 240.0 / juce::jmax (1.0, r.bpm);
        struct Seg { std::array<double, 12> c {}, bc {}; bool ok = false; };
        auto gather = [&] (double t0, double t1)
        {
            Seg sg;
            for (int i = frameOf (t0); i < frameOf (t1); ++i) for (int p = 0; p < 12; ++p) { sg.c[(size_t) p] += f.chroma[(size_t) i][(size_t) p]; sg.bc[(size_t) p] += f.bassChroma[(size_t) i][(size_t) p]; }
            double cs = 0, bs = 0; for (int p = 0; p < 12; ++p) { cs += sg.c[(size_t) p]; bs += sg.bc[(size_t) p]; }
            if (cs < 1e-6) return sg;
            for (auto& v : sg.c) v /= cs;
            if (bs > 1e-9) for (auto& v : sg.bc) v /= bs;
            // the tracked sub line: notes weighted by overlap, the one sounding on the segment's start counts
            // double (the bass on the downbeat defines the chord), glide targets count half
            std::array<double, 12> sb {}; double sw = 0;
            for (auto& sn : r.sub)
            {
                const double ov = juce::jmin (t1, sn.startSec + sn.lengthSec) - juce::jmax (t0, sn.startSec);
                if (ov <= 0.02) continue;
                double w = ov * (sn.startSec <= t0 + 0.08 ? 2.0 : 1.0) * (sn.glide ? 0.5 : 1.0);
                sb[(size_t) ((((int) std::lround (sn.midi)) % 12 + 12) % 12)] += w; sw += w;
            }
            if (sw > 0.25 * (t1 - t0))
            {
                for (int p = 0; p < 12; ++p) sg.bc[(size_t) p] = 0.3 * sg.bc[(size_t) p] + 0.7 * sb[(size_t) p] / sw;
            }
            sg.ok = true; return sg;
        };
        auto score = [&] (const Seg& sg, int root, const T& t)
        {
            double in = 0; for (int iv : t.iv) in += sg.c[(size_t) ((root + iv) % 12)];
            const double out = 1.0 - in;
            const juce::String qs (t.q);
            return in / (double) t.iv.size() * 3.0 - out / (12.0 - (double) t.iv.size()) * 3.0 + 1.0 * sg.bc[(size_t) root]
                 - (t.iv.size() > 3 ? 0.05 : 0.0) - (qs.startsWith ("sus") ? 0.08 : 0.0) - (qs == "dim" ? 0.05 : 0.0);
        };
        auto bestOf = [&] (const Seg& sg, int& br, int& bt)
        {
            double best = -1e9; br = 0; bt = 0;
            for (int root = 0; root < 12; ++root)
                for (int ti = 0; ti < (int) (sizeof (templ) / sizeof (templ[0])); ++ti)
                {
                    const double sc = score (sg, root, templ[ti]);
                    if (sc > best) { best = sc; br = root; bt = ti; }
                }
            return best;
        };
        auto push = [&] (double t0, int root, int ti, double conf)
        {
            Chord ch; ch.startSec = t0; ch.root = root; ch.quality = templ[ti].q; ch.confidence = (float) conf;
            ch.name = juce::String (noteNames[root]) + templ[ti].q;
            if (! r.chords.empty() && r.chords.back().name == ch.name) return;   // merge repeats
            r.chords.push_back (ch);
        };
        // one chord per bar unless both halves clearly disagree with it (a real two-chord bar),
        // so a melody note over the second half doesn't turn C into Fsus2
        for (size_t b = 0; b < r.bars.size(); ++b)
        {
            const double t0 = r.bars[b];
            if (t0 + barLen > r.seconds + 0.01) break;
            const Seg whole = gather (t0, t0 + barLen);
            if (! whole.ok) continue;
            int wr, wt; const double ws = bestOf (whole, wr, wt);
            const Seg h1 = gather (t0, t0 + barLen / 2), h2 = gather (t0 + barLen / 2, t0 + barLen);
            bool split = false; int r1 = wr, t1 = wt, r2 = wr, t2 = wt; double s1 = ws, s2 = ws;
            if (h1.ok && h2.ok)
            {
                s1 = bestOf (h1, r1, t1); s2 = bestOf (h2, r2, t2);
                const double m1 = s1 - score (h1, wr, templ[wt]), m2 = s2 - score (h2, wr, templ[wt]);
                const bool rootsDiffer = r1 != r2 && h1.bc[(size_t) r1] > 0.2 && h2.bc[(size_t) r2] > 0.2;   // the bass moves too
                split = rootsDiffer && m1 > 0.35 && m2 > 0.35 && juce::jmax (m1, m2) > 0.5;
            }
            if (split) { push (t0, r1, t1, s1); push (t0 + barLen / 2, r2, t2, s2); }
            else push (t0, wr, wt, ws);
        }
    }

    //==========================================================================
    /** Swing and the drum grid: each bar's 16 steps sampled from the three band onset curves. */
    inline void transcribeDrums (const Frames& f, Result& r)
    {
        r.drums.clear();
        if (r.bars.size() < 2) return;
        auto frameOf = [&] (double t) { return juce::jlimit (0, f.count - 1, (int) std::lround ((t - onsetLag) * analysisRate / hop - (double) fftSize / (2 * hop))); };
        auto peak = [&] (const std::vector<float>& v, double t, double win) { float m = 0; for (int i = frameOf (t - win); i <= frameOf (t + win); ++i) m = juce::jmax (m, v[(size_t) i]); return m; };
        // thresholds from the bands' own distributions (each band's strong onsets)
        std::vector<float> k, s, h;
        for (size_t b = 0; b + 1 < r.bars.size(); ++b)
        {
            const double step = (r.bars[b + 1] - r.bars[b]) / 16.0;
            for (int i = 0; i < 16; ++i) { const double t = r.bars[b] + i * step; k.push_back (peak (f.fluxKick, t, 0.025)); s.push_back (peak (f.fluxSnare, t, 0.025)); h.push_back (peak (f.fluxHat, t, 0.02)); }
        }
        const float kMax = detail::percentile (k, 0.98f) + 1e-6f, sMax = detail::percentile (s, 0.98f) + 1e-6f, hMax = detail::percentile (h, 0.98f) + 1e-6f;
        const float kT = 0.35f * kMax, sT = 0.35f * sMax, hT = 0.25f * hMax;
        float hatFastT = 1e9f;
        if (! f.hatFast.empty()) { std::vector<float> tmp (f.hatFast); hatFastT = 0.3f * detail::percentile (tmp, 0.995f) + 1e-6f; }
        // ---- attacks from the fine band energies: peak in [t - 15 ms, t + 30 ms] (later for swung off-steps) over the
        //      floor in [t - 70 ms, t - 15 ms]. A held 808 under a kick, or a pad under a snare, only raises the floor.
        struct Att { float peak = 0, riseDb = 0, sharpDb = 0; };
        const double fr = f.hatFastRate > 0 ? f.hatFastRate : analysisRate / 64.0;
        auto attack = [&] (const std::vector<float>& e, double t, double after, double sharpSec) -> Att
        {
            Att a; if (e.empty()) return a;
            const int n = (int) e.size();
            const int p0 = juce::jlimit (0, n - 1, (int) ((t - 0.015) * fr)), p1 = juce::jlimit (0, n - 1, (int) ((t + after) * fr));
            const int b0 = juce::jlimit (0, n - 1, (int) ((t - 0.070) * fr));
            float pk = 0, fl = 1e30f; int ip = p0;
            for (int i = p0; i <= p1; ++i) if (e[(size_t) i] > pk) { pk = e[(size_t) i]; ip = i; }
            for (int i = b0; i < p0; ++i) fl = juce::jmin (fl, e[(size_t) i]);
            if (fl > 1e29f) fl = pk;
            a.peak = pk; a.riseDb = (float) (10.0 * std::log10 ((pk + 1e-12) / (fl + 1e-12)));
            // how fast it got there: a hit jumps, a swell or two notes beating creep up
            const int back = juce::jmax (1, (int) (sharpSec * fr));
            a.sharpDb = (float) (10.0 * std::log10 ((pk + 1e-12) / (e[(size_t) juce::jmax (0, ip - back)] + 1e-12)));
            return a;
        };
        auto refOf = [&] (const std::vector<float>& e) { if (e.empty()) return 1e-9f; std::vector<float> t2 (e); return juce::jmax (1e-12f, detail::percentile (t2, 0.995f)); };
        const float refK = refOf (f.eK), refS = refOf (f.eS), refH = refOf (f.eH);
        auto dbRel = [] (float pk, float ref) { return (float) (10.0 * std::log10 ((pk + 1e-15) / ref)); };
        struct StepFeat { Att k, b, s, h; };
        std::vector<std::array<StepFeat, 16>> feats;
        std::vector<float> candX; std::vector<char> candHat;
        for (size_t b = 0; b + 1 < r.bars.size(); ++b)
        {
            std::array<StepFeat, 16> row;
            const double step = (r.bars[b + 1] - r.bars[b]) / 16.0;
            for (int i = 0; i < 16; ++i)
            {
                const double t = r.bars[b] + i * step, after = i % 2 == 1 ? juce::jmax (0.03, step * 0.55) : 0.03;
                row[(size_t) i] = { attack (f.eK, t, after, 0.035), attack (f.eB, t, after, 0.015), attack (f.eS, t, after, 0.010), attack (f.eH, t, after, 0.008) };
                const auto& sf = row[(size_t) i];
                if (sf.s.riseDb >= 6.0f && sf.s.sharpDb >= 4.0f && dbRel (sf.s.peak, refS) > -18.0f) { candX.push_back (std::log (sf.s.peak + 1e-15f)); candHat.push_back (sf.h.riseDb >= 6.0f ? 1 : 0); }
            }
            feats.push_back (row);
        }
        // snare-band hits split by loudness (2-means on log energy). When the quiet group is mostly hats landing on the
        // same step (white-noise hats spill into 1-5 kHz), a quiet hit counts as a snare only with a body (150-400 Hz) attack.
        float splitX = -1e30f; bool quietNeedsBody = false;
        if (candX.size() >= 8)
        {
            float lo = *std::min_element (candX.begin(), candX.end()), hi = *std::max_element (candX.begin(), candX.end());
            for (int it = 0; it < 20; ++it)
            {
                double sl = 0, sh = 0; int nl = 0, nh = 0; const float mid = 0.5f * (lo + hi);
                for (auto x : candX) { if (x < mid) { sl += x; ++nl; } else { sh += x; ++nh; } }
                if (nl == 0 || nh == 0) break;
                lo = (float) (sl / nl); hi = (float) (sh / nh);
            }
            splitX = 0.5f * (lo + hi);
            int nq = 0, nqHat = 0; for (size_t i = 0; i < candX.size(); ++i) if (candX[i] < splitX) { ++nq; nqHat += candHat[i]; }
            quietNeedsBody = hi - lo > 1.6f && nq > 0 && nqHat > 0.7f * nq;
        }
        auto vel = [&] (float pk, float ref) { return juce::jlimit (0.1f, 1.0f, 1.0f + dbRel (pk, ref) / 24.0f); };
        // swing: where does the off-16th land inside each 8th? (search 0.5..0.75 of the 8th)
        double swingSum = 0, swingW = 0; int offEvents = 0, offSlots = 0;
        int hatHits = 0, beatsCounted = 0;
        for (size_t b = 0; b + 1 < r.bars.size(); ++b)
        {
            DrumBar db;
            const double step = (r.bars[b + 1] - r.bars[b]) / 16.0;
            for (int i = 0; i < 16; ++i)
            {
                const auto& sf = feats[b][(size_t) i];
                const bool kickHit = sf.k.riseDb >= 6.0f && sf.k.sharpDb >= 4.0f && dbRel (sf.k.peak, refK) > -22.0f;
                const bool hatHit = sf.h.riseDb >= 6.0f && sf.h.sharpDb >= 4.0f && dbRel (sf.h.peak, refH) > -32.0f;
                bool snareHit = sf.s.riseDb >= 6.0f && sf.s.sharpDb >= 4.0f && dbRel (sf.s.peak, refS) > -18.0f;
                // the quiet group, when it is mostly hats spilling into 1-5 kHz: on a step with a hat it is the hat
                if (snareHit && quietNeedsBody && hatHit && std::log (sf.s.peak + 1e-15f) < splitX) snareHit = false;
                db.kick[(size_t) i] = kickHit ? vel (sf.k.peak, refK) : 0.0f;
                db.snare[(size_t) i] = snareHit ? vel (sf.s.peak, refS) : 0.0f;
                db.hat[(size_t) i] = hatHit ? vel (sf.h.peak, refH) : 0.0f;
                if (i % 2 == 1)
                {
                    ++offSlots;
                    const double e0 = r.bars[b] + (i - 1) * step;
                    double bestT = 0.5; float bv = 0;
                    for (double fr = 0.45; fr <= 0.78; fr += 0.01) { const float v = peak (f.fluxHat, e0 + fr * 2 * step, 0.006); if (v > bv) { bv = v; bestT = fr; } }
                    if (bv > hT) { swingSum += bestT * bv; swingW += bv; ++offEvents; }
                }
            }
            // hat rolls: hat onsets faster than 16ths inside a beat, counted on the fine curve (the 93 ms
            // frames smear 1/32 hits together)
            for (int beat = 0; beat < 4; ++beat)
            {
                int onsets = 0;
                const double bt0 = r.bars[b] + beat * 4 * step - onsetLag * 0.5, bt1 = bt0 + 4 * step;
                if (! f.hatFast.empty())
                {
                    const int i0 = juce::jmax (1, (int) (bt0 * f.hatFastRate)), i1 = juce::jmin ((int) f.hatFast.size() - 1, (int) (bt1 * f.hatFastRate));
                    const int gap = juce::jmax (1, (int) (0.022 * f.hatFastRate));   // 22 ms: faster than any played roll
                    int lastHit = -100000;
                    for (int i = i0; i < i1; ++i)
                    {
                        const float v = f.hatFast[(size_t) i];
                        if (v < hatFastT || i - lastHit < gap) continue;
                        bool isMax = true;
                        for (int j = juce::jmax (0, i - gap / 2); j <= juce::jmin ((int) f.hatFast.size() - 1, i + gap / 2); ++j) if (f.hatFast[(size_t) j] > v) { isMax = false; break; }
                        if (isMax) { ++onsets; lastHit = i; }
                    }
                }
                if (onsets >= 6) ++db.hatRolls;
                hatHits += onsets; ++beatsCounted;
            }
            r.drums.push_back (db);
        }
        r.swing = swingW > 0 && offEvents > offSlots / 4 ? swingSum / swingW : 0.5;   // needs real off-16th hits to measure
        r.hatDensity = beatsCounted > 0 ? (float) hatHits / (float) beatsCounted : 0.0f;
    }

    //==========================================================================
    /** The sub / 808 line: autocorrelation pitch on a 25-110 Hz band, every 23 ms; notes, glides, octave jumps. */
    inline void trackSub (const std::vector<float>& mono, Result& r)
    {
        r.sub.clear(); r.subGlides = r.subOctaveJumps = 0;
        // 2nd-order low-pass at 120 Hz, twice
        juce::IIRFilter lp1, lp2;
        auto c = juce::IIRCoefficients::makeLowPass (analysisRate, 120.0);
        lp1.setCoefficients (c); lp2.setCoefficients (c);
        std::vector<float> s (mono);
        lp1.processSamples (s.data(), (int) s.size()); lp2.processSamples (s.data(), (int) s.size());
        const int win = 2048, step = 512;
        const int minLag = (int) (analysisRate / 110.0), maxLag = (int) (analysisRate / 25.0), lowestLag = (int) (analysisRate / 27.5);   // A0 is the lowest real 808
        double subE = 0, allE = 0; for (size_t i = 0; i < mono.size(); ++i) { subE += (double) s[i] * s[i]; allE += (double) mono[i] * mono[i]; }
        r.subPresence = allE > 0 ? (float) (subE / allE) : 0.0f;
        std::vector<std::pair<double, float>> track;   // time, midi (0 = none)
        float rmsMax = 0;
        std::vector<float> rmsv;
        for (int p = 0; p + win + maxLag < (int) s.size(); p += step) { double e = 0; for (int i = 0; i < win; ++i) e += (double) s[(size_t) (p + i)] * s[(size_t) (p + i)]; rmsv.push_back ((float) std::sqrt (e / win)); rmsMax = juce::jmax (rmsMax, rmsv.back()); }
        int k = 0;
        for (int p = 0; p + win + maxLag < (int) s.size(); p += step, ++k)
        {
            if (rmsv[(size_t) k] < 0.12f * rmsMax) { track.push_back ({ p / analysisRate, 0.0f }); continue; }
            double best = 0; int bl = 0;
            std::vector<double> acv ((size_t) (maxLag + 2), 0.0);
            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double acc = 0, n1 = 0, n2 = 0;
                for (int i = 0; i < win; i += 2) { const double a = s[(size_t) (p + i)], b = s[(size_t) (p + i + lag)]; acc += a * b; n1 += a * a; n2 += b * b; }
                const double v = acc / std::sqrt (n1 * n2 + 1e-12);
                acv[(size_t) lag] = v;
                if (v > best) { best = v; bl = lag; }
            }
            // the period is the SHORTEST lag that correlates about as well as the best one (every multiple of the period
            // correlates too, and a square-ish saturated 808 makes them nearly equal: that is the octave-down error)
            for (int lag = minLag + 1; lag < maxLag; ++lag)
                if (acv[(size_t) lag] >= 0.9 * best && acv[(size_t) lag] >= acv[(size_t) lag - 1] && acv[(size_t) lag] >= acv[(size_t) lag + 1]) { bl = lag; break; }
            track.push_back ({ p / analysisRate, best > 0.6 && bl > 0 && bl <= lowestLag ? (float) detail::hzToMidi (analysisRate / bl) : 0.0f });
        }
        // median-of-3 smoothing, then notes = stable runs (within 0.6 st of the run's start, >= 4 frames ~ 90 ms). A kick's
        // fast sweep never forms a stable run. Between two consecutive notes: a monotonic move through the frames in between
        // (or adjacent notes with no unvoiced gap and a smooth slope) is a glide; 12 +- 0.8 st is an octave jump.
        std::vector<float> m ((size_t) track.size());
        for (size_t i = 0; i < track.size(); ++i)
        {
            float a0 = track[i > 0 ? i - 1 : i].second, a1 = track[i].second, a2 = track[i + 1 < track.size() ? i + 1 : i].second;
            if (a0 <= 0 || a1 <= 0 || a2 <= 0) { m[i] = a1; continue; }
            m[i] = std::max (std::min (a0, a1), std::min (std::max (a0, a1), a2));
        }
        struct Run { size_t a, z; float pitch; };
        std::vector<Run> runs;
        for (size_t i = 0; i < m.size();)
        {
            if (m[i] <= 0) { ++i; continue; }
            size_t j = i; const float p0 = m[i];
            while (j + 1 < m.size() && m[j + 1] > 0 && std::abs (m[j + 1] - p0) < 0.6f) ++j;
            if (j - i + 1 >= 4)
            {
                std::vector<float> v (m.begin() + (long) i, m.begin() + (long) j + 1);
                std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
                runs.push_back ({ i, j, v[v.size() / 2] });
            }
            i = j + 1;
        }
        // a kick landing on a held 808 splits the run: rejoin neighbours within a semitone and a couple of frames apart
        {
            std::vector<Run> merged;
            for (auto& rn : runs)
            {
                if (! merged.empty() && rn.a - merged.back().z <= 3 && std::abs (rn.pitch - merged.back().pitch) < 1.0f)
                {
                    auto& mb = merged.back();
                    const float la = (float) (mb.z - mb.a + 1), lb = (float) (rn.z - rn.a + 1);
                    mb.pitch = la >= lb ? mb.pitch : rn.pitch; mb.z = rn.z;
                }
                else merged.push_back (rn);
            }
            runs.swap (merged);
        }
        const double fdt = (double) step / analysisRate;
        for (size_t k = 0; k < runs.size(); ++k)
        {
            SubNote nt; nt.startSec = track[runs[k].a].first; nt.lengthSec = (runs[k].z - runs[k].a + 1) * fdt; nt.midi = runs[k].pitch;
            if (k > 0)
            {
                const auto& pr = runs[k - 1];
                const size_t gap = runs[k].a - pr.z - 1;
                const float d = runs[k].pitch - pr.pitch;
                bool voicedBetween = true, monotonic = true;
                for (size_t i = pr.z + 1; i < runs[k].a; ++i)
                {
                    if (m[i] <= 0) voicedBetween = false;
                    if (i > pr.z + 1 && m[i] > 0 && m[i - 1] > 0 && (m[i] - m[i - 1]) * d < -0.2f) monotonic = false;
                }
                // a re-triggered note has an energy rise into it (a new hit); a glide keeps the level gliding
                float lowest = 1e9f; for (size_t i = pr.z; i <= runs[k].a && i < rmsv.size(); ++i) lowest = juce::jmin (lowest, rmsv[i]);
                const float into = runs[k].a + 1 < rmsv.size() ? juce::jmax (rmsv[runs[k].a], rmsv[runs[k].a + 1]) : lowest;
                const bool retrigger = into > 1.25f * lowest;
                const bool octave = std::abs (std::abs (d) - 12.0f) < 0.8f;
                if (octave && gap <= 2) ++r.subOctaveJumps;
                else if (std::abs (d) >= 1.5f && std::abs (d) < 12.5f && gap <= 16)
                {
                    // the pitch travelling between the two notes: frames strictly between them, in order, in the
                    // glide's direction (a kick on top can break the run or add energy, but it can't fake that)
                    // ... and it leaves FROM the old note (the frame before the first in-between one sounds the old pitch;
                    // a re-struck note's attack smear starts from silence)
                    int between = 0; float last = pr.pitch; bool inOrder = true, departs = false;
                    float before = pr.pitch;
                    for (size_t i = pr.z + 1; i < runs[k].a; ++i)
                    {
                        const float v = m[i];
                        const bool inside = v > 0 && (d > 0 ? (v > pr.pitch + 0.7f && v < runs[k].pitch - 0.7f) : (v < pr.pitch - 0.7f && v > runs[k].pitch + 0.7f));
                        if (! inside) { if (between == 0) before = v; continue; }
                        if (between == 0) departs = before > 0 && std::abs (before - pr.pitch) < 1.0f;
                        if ((v - last) * d < -0.3f) inOrder = false;
                        last = v; ++between;
                    }
                    const bool travelled = between >= 2 && inOrder && departs;
                    const bool held = pr.z - pr.a + 1 >= 6;   // a glide leaves a held note, not a kick's few frames on top of the 808
                    if (travelled || (held && voicedBetween && monotonic && ! retrigger && gap <= 12)) { nt.glide = true; ++r.subGlides; }
                }
            }
            r.sub.push_back (nt);
        }
    }

    //==========================================================================
    /** Sections from a bar-level self-similarity matrix: novelty peaks on 4-bar boundaries, repeated parts share a letter. */
    inline void findSections (const Frames& f, Result& r)
    {
        r.sections.clear();
        const int nb = (int) r.bars.size() - 1;
        if (nb < 4) { if (nb > 0) r.sections.push_back ({ 0, nb, 'A', "Loop" }); return; }
        auto frameOf = [&] (double t) { return juce::jlimit (0, f.count - 1, (int) std::lround ((t - onsetLag) * analysisRate / hop - (double) fftSize / (2 * hop))); };
        // per-bar feature: chroma (12) + band energies (5, log) + densities (kick / snare / hat onsets)
        std::vector<std::vector<double>> feat ((size_t) nb);
        std::vector<double> barEnergy ((size_t) nb);
        for (int b = 0; b < nb; ++b)
        {
            std::vector<double> v (20, 0.0);
            const int a = frameOf (r.bars[(size_t) b]), z = juce::jmax (a + 1, frameOf (r.bars[(size_t) b + 1]));
            double e = 0;
            for (int i = a; i < z; ++i)
            {
                double cs = 0; for (auto x : f.chroma[(size_t) i]) cs += x;
                if (cs > 1e-9) for (int p = 0; p < 12; ++p) v[(size_t) p] += f.chroma[(size_t) i][(size_t) p] / cs;
                v[12] += f.eKick[(size_t) i]; v[13] += f.eSnare[(size_t) i]; v[14] += f.eHat[(size_t) i]; v[15] += f.eSub[(size_t) i]; v[16] += f.eMid[(size_t) i];
                v[17] += f.fluxKick[(size_t) i]; v[18] += f.fluxSnare[(size_t) i]; v[19] += f.fluxHat[(size_t) i];
                e += f.eAll[(size_t) i];
            }
            const double len = (double) (z - a);
            for (int p = 0; p < 12; ++p) v[(size_t) p] /= len;
            for (int q = 12; q < 20; ++q) v[(size_t) q] = std::log10 (1e-9 + v[(size_t) q] / len) * 0.25;
            feat[(size_t) b] = v; barEnergy[(size_t) b] = 10 * std::log10 (1e-12 + e / len);
        }
        // standardise each dimension
        for (int d = 0; d < 20; ++d)
        {
            double m = 0, s = 0; for (int b = 0; b < nb; ++b) m += feat[(size_t) b][(size_t) d]; m /= nb;
            for (int b = 0; b < nb; ++b) s += std::pow (feat[(size_t) b][(size_t) d] - m, 2); s = std::sqrt (s / nb) + 1e-9;
            for (int b = 0; b < nb; ++b) feat[(size_t) b][(size_t) d] = (feat[(size_t) b][(size_t) d] - m) / s;
        }
        auto sim = [&] (int a, int b) { double dot = 0, na = 0, nbb = 0; for (int d = 0; d < 20; ++d) { dot += feat[(size_t) a][(size_t) d] * feat[(size_t) b][(size_t) d]; na += std::pow (feat[(size_t) a][(size_t) d], 2); nbb += std::pow (feat[(size_t) b][(size_t) d], 2); } return dot / std::sqrt (na * nbb + 1e-12); };
        // novelty with a 4-bar checkerboard kernel
        const int K = 4;
        std::vector<double> nov ((size_t) nb + 1, 0.0);
        for (int c = K; c <= nb - K; ++c)
        {
            double s = 0;
            for (int i = -K; i < K; ++i) for (int j = -K; j < K; ++j) { const double sign = ((i < 0) == (j < 0)) ? 1.0 : -1.0; s += sign * sim (c + i, c + j); }
            nov[(size_t) c] = s;
        }
        std::vector<int> bounds { 0 };
        double nmax = *std::max_element (nov.begin(), nov.end());
        for (int c = K; c <= nb - K; ++c)
        {
            const bool peakHere = nov[(size_t) c] > 0.35 * nmax && nov[(size_t) c] >= nov[(size_t) c - 1] && nov[(size_t) c] >= nov[(size_t) c + 1];
            if (! peakHere) continue;
            // snap to the nearest 4-bar line when within 1 bar (arrangements move in 4s and 8s)
            int snapped = (int) std::lround (c / 4.0) * 4; if (std::abs (snapped - c) > 1) snapped = c;
            if (snapped - bounds.back() >= 4 && nb - snapped >= 2) bounds.push_back (snapped);
        }
        bounds.push_back (nb);
        // letters: a segment similar (mean sim > 0.55) to an earlier one reuses its letter
        std::vector<char> letters;
        auto segSim = [&] (int a0, int a1, int b0, int b1) { double s = 0; int n = 0; const int la = a1 - a0, lb = b1 - b0, l = juce::jmin (la, lb); for (int i = 0; i < l; ++i) { s += sim (a0 + i, b0 + i); ++n; } return n > 0 ? s / n : 0.0; };
        char next = 'A';
        for (size_t i = 0; i + 1 < bounds.size(); ++i)
        {
            char L = 0;
            for (size_t j = 0; j < i; ++j) if (segSim (bounds[j], bounds[j + 1], bounds[i], bounds[i + 1]) > 0.55) { L = letters[j]; break; }
            if (L == 0) L = next++;
            letters.push_back (L);
            Section sec; sec.startBar = bounds[i]; sec.bars = bounds[i + 1] - bounds[i]; sec.letter = L;
            double e = 0; for (int b = bounds[i]; b < bounds[i + 1]; ++b) e += barEnergy[(size_t) b]; sec.energyDb = (float) (e / juce::jmax (1, sec.bars));
            r.sections.push_back (sec);
        }
        // names: the loudest repeated letter is the Hook; first quiet part Intro; last quiet part Outro; other repeated Verse
        std::map<char, int> count; std::map<char, double> energy;
        for (auto& s : r.sections) { count[s.letter]++; energy[s.letter] = juce::jmax (energy.count (s.letter) ? energy[s.letter] : -1e9, (double) s.energyDb); }
        char hook = 0; double he = -1e9;
        for (auto& kv : count) if (energy[kv.first] > he && (kv.second > 1 || count.size() <= 2)) { he = energy[kv.first]; hook = kv.first; }
        double maxE = -1e9; for (auto& s : r.sections) maxE = juce::jmax (maxE, (double) s.energyDb);
        for (size_t i = 0; i < r.sections.size(); ++i)
        {
            auto& s = r.sections[i];
            if (s.letter == hook) s.name = "Hook";
            else if (i == 0 && s.energyDb < maxE - 3) s.name = "Intro";
            else if (i + 1 == r.sections.size() && s.energyDb < maxE - 3) s.name = "Outro";
            else if (count[s.letter] > 1) s.name = "Verse";
            else s.name = "Bridge";
        }
    }

    /** Elements per section from band activity: kick, snare/clap, hats, 808/sub, chords/keys, pad, lead/melody. */
    inline void countElements (const Frames& f, Result& r)
    {
        auto frameOf = [&] (double t) { return juce::jlimit (0, f.count - 1, (int) std::lround ((t - onsetLag) * analysisRate / hop - (double) fftSize / (2 * hop))); };
        float allMax = 0; for (auto v : f.eAll) allMax = juce::jmax (allMax, v);
        for (auto& s : r.sections)
        {
            s.elementList.clear();
            int kick = 0, snare = 0, hat = 0;
            for (int b = s.startBar; b < s.startBar + s.bars && b < (int) r.drums.size(); ++b)
                for (int i = 0; i < 16; ++i) { kick += r.drums[(size_t) b].kick[(size_t) i] > 0; snare += r.drums[(size_t) b].snare[(size_t) i] > 0; hat += r.drums[(size_t) b].hat[(size_t) i] > 0; }
            const double perBar = 1.0 / juce::jmax (1, s.bars);
            if (kick * perBar >= 1.0) s.elementList.add ("kick");
            if (snare * perBar >= 0.8) s.elementList.add ("snare/clap");
            if (hat * perBar >= 3.0) s.elementList.add ("hats");
            const int a = frameOf (r.bars[(size_t) s.startBar]), z = frameOf (r.bars[(size_t) juce::jmin ((int) r.bars.size() - 1, s.startBar + s.bars)]);
            double sub = 0, all = 0, ton = 0, sus = 0, mid = 0;
            for (int i = a; i < z; ++i) { sub += f.eSub[(size_t) i]; all += f.eAll[(size_t) i]; ton += f.tonalness[(size_t) i]; sus += f.sustainMid[(size_t) i]; mid += f.eMid[(size_t) i]; }
            const double n = juce::jmax (1, z - a);
            if (all > 0 && sub / all > 0.25) s.elementList.add ("808/sub");
            if (ton / n > 0.18) s.elementList.add ("chords/keys");
            if (mid > 0 && sus / mid > 0.85) s.elementList.add ("pad");
            if (ton / n > 0.28) s.elementList.add ("lead/melody");
            s.elements = s.elementList.size();
        }
    }

    //==========================================================================
    /** Mix profile from the original-rate stereo: band balance, per-band width, pump depth at the beat period. */
    inline void mixProfile (const float* L, const float* R, int n, double sr, Result& r)
    {
        const int order = 12, N = 1 << order; juce::dsp::FFT fft (order);
        std::vector<float> wm ((size_t) N * 2), ws ((size_t) N * 2), win ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (N - 1)));
        const double edges[6] = { 20, 60, 250, 2000, 6000, 11000 };
        std::array<double, 5> band {}; std::array<double, 3> mid3 {}, side3 {};
        double total = 0;
        for (int p = 0; p + N <= n; p += N)
        {
            std::fill (wm.begin(), wm.end(), 0.0f); std::fill (ws.begin(), ws.end(), 0.0f);
            for (int i = 0; i < N; ++i) { const float l = L[p + i], rr = R != nullptr ? R[p + i] : l; wm[(size_t) i] = 0.5f * (l + rr) * win[(size_t) i]; ws[(size_t) i] = 0.5f * (l - rr) * win[(size_t) i]; }
            fft.performFrequencyOnlyForwardTransform (wm.data(), true); fft.performFrequencyOnlyForwardTransform (ws.data(), true);
            for (int b = 1; b <= N / 2; ++b)
            {
                const double hz = b * sr / N, m2 = (double) wm[(size_t) b] * wm[(size_t) b], s2 = (double) ws[(size_t) b] * ws[(size_t) b];
                total += m2 + s2;
                for (int k = 0; k < 5; ++k) if (hz >= edges[k] && hz < edges[k + 1]) band[(size_t) k] += m2 + s2;
                const int w = hz < 150 ? 0 : hz < 2000 ? 1 : 2;
                mid3[(size_t) w] += m2; side3[(size_t) w] += s2;
            }
        }
        for (int k = 0; k < 5; ++k) r.bandDb[(size_t) k] = (float) (10 * std::log10 ((band[(size_t) k] + 1e-12) / (total + 1e-12)));
        r.bandDb[5] = (float) (10 * std::log10 (total + 1e-12));
        for (int w = 0; w < 3; ++w) r.widthBands[(size_t) w] = (float) (side3[(size_t) w] / (mid3[(size_t) w] + 1e-12));
        // pump: 300 Hz - 3 kHz envelope (10 ms), folded on the beat period; depth = max - min of the average shape (dB)
        if (r.bpm > 0)
        {
            juce::IIRFilter hp, lp; hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 300.0)); lp.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 3000.0));
            std::vector<float> x ((size_t) n); for (int i = 0; i < n; ++i) x[(size_t) i] = 0.5f * (L[i] + (R != nullptr ? R[i] : L[i]));
            hp.processSamples (x.data(), n); lp.processSamples (x.data(), n);
            const int blk = (int) (sr * 0.01); const double period = sr * 60.0 / r.bpm; const int bins = 24;
            // per phase bin, the 30th percentile of the level (dB): drum hits are sparse in each bin and drop out, the
            // sustained parts' ducking stays. Pump = recovered level late in the beat minus the dip just after it.
            std::vector<std::vector<float>> foldDb ((size_t) bins);
            const double t0 = r.beats.empty() ? 0.0 : r.beats.front() * sr;
            for (int p = 0; p + blk < n; p += blk)
            {
                double e = 0; for (int i = 0; i < blk; ++i) e += (double) x[(size_t) (p + i)] * x[(size_t) (p + i)];
                const double ph = std::fmod ((p - t0) / period + 1000.0, 1.0);
                const int bi = juce::jlimit (0, bins - 1, (int) (ph * bins));
                foldDb[(size_t) bi].push_back ((float) (10 * std::log10 (e / blk + 1e-15)));
            }
            std::vector<float> curve ((size_t) bins, -150.0f);
            for (int i = 0; i < bins; ++i) if (! foldDb[(size_t) i].empty()) curve[(size_t) i] = detail::percentile (foldDb[(size_t) i], 0.3f);
            // ducking pulls the level BELOW its usual line once a beat; hits on the grid (hats on every 8th, a kick on
            // every beat) push it ABOVE. So pump = the curve's median minus its lowest bin (phase-free).
            std::vector<float> sorted (curve); std::sort (sorted.begin(), sorted.end());
            const double mx = sorted[sorted.size() / 2], mn = sorted.front();
            r.pumpDb = mx > mn ? (float) (mx - mn) : 0.0f;
        }
    }

    //==========================================================================
    /** The whole analysis. L / R at any rate; R may be null (mono). */
    inline Result analyse (const float* L, const float* R, int n, double sampleRate)
    {
        Result r; r.sampleRate = sampleRate; r.seconds = n / sampleRate;
        if (n < (int) (sampleRate * 4)) { r.error = "Too short to analyse (needs at least 4 seconds)."; return r; }
        std::vector<float> mono ((size_t) n);
        for (int i = 0; i < n; ++i) mono[(size_t) i] = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
        auto m22 = detail::resample (mono.data(), n, sampleRate, analysisRate);
        auto f = computeFrames (m22);
        {
            // fine band energies in 64-sample blocks (2.9 ms): the drum grid reads attacks from these (the 93 ms analysis
            // frames smear a kick under a held 808 and a hat into the snare band); the hat band's positive log
            // difference counts rolls
            using C = juce::dsp::IIR::Coefficients<float>;
            auto mk = [] (C::Ptr a, C::Ptr b) { std::array<juce::dsp::IIR::Filter<float>, 2> f; f[0].coefficients = a; f[1].coefficients = b; return f; };
            auto fk = mk (C::makeLowPass (analysisRate, 120.0), C::makeHighPass (analysisRate, 35.0));
            auto fb = mk (C::makeHighPass (analysisRate, 150.0), C::makeLowPass (analysisRate, 400.0));
            auto fs = mk (C::makeHighPass (analysisRate, 1000.0), C::makeLowPass (analysisRate, 5000.0));
            auto fh = mk (C::makeHighPass (analysisRate, 6000.0), C::makeHighPass (analysisRate, 6000.0));
            juce::dsp::IIR::Filter<float> fk2; fk2.coefficients = C::makeLowPass (analysisRate, 120.0);
            const int blk = 64; const int nb = (int) m22.size() / blk;
            f.hatFast.assign ((size_t) juce::jmax (0, nb), 0.0f); f.hatFastRate = analysisRate / blk;
            for (auto* v : { &f.eK, &f.eB, &f.eS, &f.eH }) v->assign ((size_t) juce::jmax (0, nb), 0.0f);
            float prev = 0;
            for (int bI = 0; bI < nb; ++bI)
            {
                double ek = 0, eb = 0, es = 0, eh = 0;
                for (int i = 0; i < blk; ++i)
                {
                    const float x = m22[(size_t) (bI * blk + i)];
                    const float k = fk2.processSample (fk[1].processSample (fk[0].processSample (x)));
                    const float bo = fb[1].processSample (fb[0].processSample (x));
                    const float sn = fs[1].processSample (fs[0].processSample (x));
                    const float hh = fh[1].processSample (fh[0].processSample (x));
                    ek += k * k; eb += bo * bo; es += sn * sn; eh += hh * hh;
                }
                f.eK[(size_t) bI] = (float) (ek / blk); f.eB[(size_t) bI] = (float) (eb / blk); f.eS[(size_t) bI] = (float) (es / blk); f.eH[(size_t) bI] = (float) (eh / blk);
                const float lv = std::log1p (1.0e4f * (float) (eh / blk));
                f.hatFast[(size_t) bI] = juce::jmax (0.0f, lv - prev); prev = lv;
            }
            // a 2.9 ms block's energy follows a low note's waveform: average over about one period of the band's lowest
            // frequency (kick 40 ms, body 9 ms), centred, so the envelope is the note's level
            auto smooth = [] (std::vector<float>& v, int w)
            {
                if (w <= 1 || v.empty()) return;
                std::vector<float> o (v.size()); double acc = 0; const int h = w / 2;
                std::vector<double> cs (v.size() + 1, 0.0); for (size_t i = 0; i < v.size(); ++i) cs[i + 1] = cs[i] + v[i];
                for (size_t i = 0; i < v.size(); ++i) { const size_t a = i >= (size_t) h ? i - (size_t) h : 0, b = juce::jmin (v.size(), i + (size_t) (w - h)); o[i] = (float) ((cs[b] - cs[a]) / (double) (b - a)); }
                juce::ignoreUnused (acc); v.swap (o);
            };
            smooth (f.eK, 14); smooth (f.eB, 3);   // kick: 40 ms, longer than a kick-against-808 beating period
        }
        // the onset envelope: broadband flux plus the drum bands (they carry the beat)
        std::vector<float> onset ((size_t) f.count);
        auto norm = [] (const std::vector<float>& v) { float m = 1e-9f; for (auto x : v) m = juce::jmax (m, x); return m; };
        const float a = norm (f.flux), k = norm (f.fluxKick), s = norm (f.fluxSnare), h = norm (f.fluxHat);
        for (int i = 0; i < f.count; ++i) onset[(size_t) i] = f.flux[(size_t) i] / a + f.fluxKick[(size_t) i] / k + f.fluxSnare[(size_t) i] / s + 0.5f * f.fluxHat[(size_t) i] / h;
        estimateTempo (onset, f.frameRate, r);
        trackBeats (onset, f.frameRate, r);
        findDownbeats (f, r);
        estimateKey (f, r);
        trackSub (m22, r);       // before the chords: the 808/sub line is the most reliable bass evidence
        estimateChords (f, r);
        refineKeyWithChords (r);
        transcribeDrums (f, r);
        findSections (f, r);
        countElements (f, r);
        mixProfile (L, R, n, sampleRate, r);
        r.ok = r.bpm > 0 && ! r.bars.empty();
        if (! r.ok) r.error = "No steady beat found.";
        return r;
    }

    /** Step string for a drum row ("x...x...x...x..."): X strong, x hit, '.' none. */
    inline juce::String stepString (const std::array<float, 16>& row)
    {
        juce::String s; for (auto v : row) s << (v >= 0.7f ? "X" : v > 0.0f ? "x" : "."); return s;
    }

    /** The most common pattern of a row across a range of bars (for the summary). */
    inline juce::String typicalRow (const std::vector<DrumBar>& d, int from, int to, int which)
    {
        std::map<juce::String, int> c;
        for (int b = from; b < to && b < (int) d.size(); ++b) { auto& row = which == 0 ? d[(size_t) b].kick : which == 1 ? d[(size_t) b].snare : d[(size_t) b].hat; c[stepString (row).replace ("X", "x")]++; }
        juce::String best; int bc = 0; for (auto& kv : c) if (kv.second > bc) { bc = kv.second; best = kv.first; }
        return best;
    }
}
