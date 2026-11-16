#pragma once
// Squelchmoth 1.23 - the engineer at your shoulder, first cut: advice written from measurements of the session's own
// audio (levels, six frequency bands, stereo correlation, low-end width, noise floor, clipped samples), never from
// generic tips. Pure DSP and text: no engine types here, so it is tested on its own (tests/advisor_test.cpp).

#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace SqAdvise
{
    constexpr int numBands = 6;
    inline const double bandEdges[numBands + 1] = { 20.0, 60.0, 120.0, 350.0, 2000.0, 6000.0, 20000.0 };
    inline const char* const bandNames[numBands] = { "sub (20-60 Hz)", "bass (60-120 Hz)", "low-mid (120-350 Hz)", "mid (350 Hz-2 kHz)", "presence (2-6 kHz)", "air (6-20 kHz)" };

    struct Stats
    {
        double seconds = 0, peakDb = -200, rmsDb = -200, crestDb = 0;
        double correlation = 1.0;          // +1 mono, 0 unrelated, -1 out of phase
        double lowSideDb = -200;           // below 120 Hz: side energy relative to mid
        double noiseDb = -200;             // the quietest tenth of 50 ms windows
        double dc = 0; int clippedRuns = 0;
        double bandPower[numBands] = {};   // mean square of the mono sum per band
        bool silent = true;
        double bandDb (int b) const { return 10.0 * std::log10 (bandPower[b] + 1e-20); }
    };

    inline double db (double power) { return 10.0 * std::log10 (power + 1e-20); }

    inline Stats measure (const float* L, const float* R, int n, double fs)
    {
        Stats s; if (n <= 0 || fs <= 0) return s;
        if (R == nullptr) R = L;
        s.seconds = n / fs;
        double sum2 = 0, sumLR = 0, sumL2 = 0, sumR2 = 0, sumDc = 0; float peak = 0; int run = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R[i]; const float a = std::max (std::abs (l), std::abs (r));
            peak = std::max (peak, a); sumL2 += (double) l * l; sumR2 += (double) r * r; sumLR += (double) l * r; sumDc += 0.5 * (l + r);
            if (a >= 0.999f) { if (++run == 3) ++s.clippedRuns; } else run = 0;
        }
        sum2 = 0.5 * (sumL2 + sumR2) / n;
        s.peakDb = 20.0 * std::log10 ((double) peak + 1e-10); s.rmsDb = db (sum2); s.crestDb = s.peakDb - s.rmsDb;
        s.silent = s.peakDb < -70.0; s.dc = sumDc / n;
        s.correlation = sumL2 > 1e-12 && sumR2 > 1e-12 ? sumLR / std::sqrt (sumL2 * sumR2) : 1.0;

        // noise floor: RMS of 50 ms windows, the tenth percentile
        {
            const int w = std::max (64, (int) (fs * 0.05)); std::vector<double> v;
            for (int p = 0; p + w <= n; p += w) { double e = 0; for (int i = p; i < p + w; ++i) e += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]); v.push_back (e / w); }
            if (! v.empty()) { std::sort (v.begin(), v.end()); s.noiseDb = db (v[v.size() / 10]); }
        }

        // band powers of the mono sum, and the side / mid balance under 120 Hz
        const int order = 12, N = 1 << order, hop = N / 2;
        juce::dsp::FFT fft (order);
        std::vector<float> win ((size_t) N), bm ((size_t) (2 * N)), bs ((size_t) (2 * N));
        double wsum = 0; for (int i = 0; i < N; ++i) { win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N); wsum += (double) win[(size_t) i] * win[(size_t) i]; }
        double lowMid = 0, lowSide = 0; int frames = 0;
        for (int p = 0; p < n; p += hop)
        {
            std::fill (bm.begin(), bm.end(), 0.0f); std::fill (bs.begin(), bs.end(), 0.0f);
            const int have = std::min (N, n - p);
            for (int i = 0; i < have; ++i) { bm[(size_t) i] = 0.5f * (L[p + i] + R[p + i]) * win[(size_t) i]; bs[(size_t) i] = 0.5f * (L[p + i] - R[p + i]) * win[(size_t) i]; }
            fft.performRealOnlyForwardTransform (bm.data(), true); fft.performRealOnlyForwardTransform (bs.data(), true);
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = k * fs / N;
                const double pm = ((double) bm[(size_t) (2 * k)] * bm[(size_t) (2 * k)] + (double) bm[(size_t) (2 * k + 1)] * bm[(size_t) (2 * k + 1)]) * 2.0 / (N * wsum);
                for (int b = 0; b < numBands; ++b) if (f >= bandEdges[b] && f < bandEdges[b + 1]) { s.bandPower[b] += pm; break; }
                if (f >= 20.0 && f < 120.0)
                {
                    lowMid += pm;
                    lowSide += ((double) bs[(size_t) (2 * k)] * bs[(size_t) (2 * k)] + (double) bs[(size_t) (2 * k + 1)] * bs[(size_t) (2 * k + 1)]) * 2.0 / (N * wsum);
                }
            }
            ++frames;
            if (p + N >= n) break;
        }
        if (frames > 0) { for (auto& b : s.bandPower) b /= frames; lowMid /= frames; lowSide /= frames; }
        s.lowSideDb = lowMid > 1e-12 ? db (lowSide) - db (lowMid) : -200.0;
        return s;
    }

    struct Named { juce::String name; Stats stats; };

    /** Two tracks carrying the same band at similar strength. */
    struct Clash { int band = 0; juce::String a, b; double shareA = 0, shareB = 0; };

    inline std::vector<Clash> clashes (const std::vector<Named>& tracks)
    {
        std::vector<Clash> out;
        for (int b = 0; b < numBands; ++b)
        {
            double total = 0; for (auto& t : tracks) total += t.stats.bandPower[b];
            if (db (total) < -55.0) continue;
            std::vector<std::pair<double, const Named*>> v;
            for (auto& t : tracks) v.push_back ({ t.stats.bandPower[b] / total, &t });
            std::sort (v.begin(), v.end(), [] (auto& x, auto& y) { return x.first > y.first; });
            if (v.size() >= 2 && v[1].first >= 0.25) out.push_back ({ b, v[0].second->name, v[1].second->name, v[0].first, v[1].first });
        }
        return out;
    }

    inline juce::String d1 (double v) { return juce::String (v, 1); }

    /** Advice for a mix: what was measured, then what to do about it. Empty list = nothing stood out. */
    inline juce::StringArray mixAdvice (const Stats& mix, const std::vector<Named>& tracks)
    {
        juce::StringArray a;
        if (mix.silent) { a.add ("The range is silent: nothing to advise on."); return a; }
        if (mix.clippedRuns > 0 || mix.peakDb >= -0.05) a.add ("The mix clips: peak " + d1 (mix.peakDb) + " dBFS with " + juce::String (mix.clippedRuns) + " flat-topped spots. Pull the master fader or the loudest tracks down until the peak sits near -6 dBFS; loudness comes back at mastering.");
        else if (mix.peakDb > -3.0) a.add ("Little headroom: the mix peaks at " + d1 (mix.peakDb) + " dBFS. For mastering, aim for peaks between -6 and -3 dBFS.");
        else if (mix.peakDb < -18.0) a.add ("The mix is low: it peaks at " + d1 (mix.peakDb) + " dBFS. Raise the master so peaks reach about -6 dBFS; nothing is gained by leaving 18 dB unused.");
        if (mix.crestDb < 7.0) a.add ("Very little dynamic range: peak to average is " + d1 (mix.crestDb) + " dB. The mix is already squeezed; ease the bus compression or limiting before mastering, or there is nothing left to master.");
        else if (mix.crestDb > 20.0) a.add ("Very wide dynamic range: peak to average is " + d1 (mix.crestDb) + " dB. A few hits stand far above the body of the song; tame the tallest peaks (the drum bus is the usual place) so the whole mix can come up.");
        if (mix.correlation < 0.1) a.add ("Mono problem: left and right correlate at " + juce::String (mix.correlation, 2) + ". Parts of this mix will thin out or vanish on a phone or a club system; find the track with the widener or the flipped phase and pull it back.");
        if (mix.lowSideDb > -12.0) a.add ("The low end is wide: under 120 Hz the side signal is only " + d1 (-mix.lowSideDb) + " dB below the centre. Keep kick, 808 and bass in the middle (mono below about 120 Hz) so the bottom stays solid on every system.");
        if (mix.bandDb (2) > mix.bandDb (3) + 4.0 && mix.bandDb (2) > mix.bandDb (1)) a.add ("Low-mid build-up: 120-350 Hz sits " + d1 (mix.bandDb (2) - mix.bandDb (3)) + " dB above the mids. That is where mixes turn muddy; listen for it and thin the pads, keys or guitars there before touching anything else.");
        if (mix.bandDb (4) < mix.bandDb (3) - 18.0) a.add ("Dull top: 2-6 kHz is " + d1 (mix.bandDb (3) - mix.bandDb (4)) + " dB under the mids. Vocals and snares get their definition there; check that nothing is filtering it away.");

        for (auto& c : clashes (tracks))
        {
            if (c.band <= 1) a.add (juce::String (bandNames[c.band]) + ": " + c.a + " (" + juce::String (juce::roundToInt (c.shareA * 100)) + " %) and " + c.b + " (" + juce::String (juce::roundToInt (c.shareB * 100)) + " %) both carry it. Decide which one owns this range, then duck the other from it (sidechain) or filter it out of the way.");
            else if (c.band == 2 || c.band == 4) a.add (juce::String (bandNames[c.band]) + ": " + c.a + " and " + c.b + " compete (" + juce::String (juce::roundToInt (c.shareA * 100)) + " % and " + juce::String (juce::roundToInt (c.shareB * 100)) + " % of it). A small cut on one where the other speaks, or panning them apart, lets both be heard.");
        }
        for (auto& t : tracks)
        {
            if (t.stats.silent) continue;
            if (t.stats.clippedRuns > 0) a.add (t.name + " clips inside the mix (" + juce::String (t.stats.clippedRuns) + " flat-topped spots, peak " + d1 (t.stats.peakDb) + " dBFS): lower its clip gain or the plug-in driving it.");
            // rumble: low energy that is audible on its own but is not what carries the bottom of the mix
            double low = t.stats.bandPower[0] + t.stats.bandPower[1], lowTotal = 0, rest = 0;
            for (auto& o : tracks) lowTotal += o.stats.bandPower[0] + o.stats.bandPower[1];
            for (int b = 2; b < numBands; ++b) rest += t.stats.bandPower[b];
            if (db (low) > -55.0 && lowTotal > 0 && low / lowTotal < 0.08 && rest > low * 4.0)
                a.add (t.name + " has low-end energy it does not need (" + d1 (db (low)) + " dBFS under 120 Hz, " + juce::String (juce::roundToInt (100.0 * low / lowTotal)) + " % of the mix's bottom): high-pass it around 100 Hz to clear room for the bass.");
        }
        return a;
    }

    /** Advice for one recorded take. */
    inline juce::StringArray recordingAdvice (const Stats& s)
    {
        juce::StringArray a;
        if (s.silent) { a.add ("The take is silent."); return a; }
        if (s.clippedRuns > 0) a.add ("The take clipped " + juce::String (s.clippedRuns) + " times (peak " + d1 (s.peakDb) + " dBFS). Clipping at the input cannot be repaired: turn the preamp down about " + juce::String (juce::roundToInt (std::max (6.0, s.peakDb + 10.0))) + " dB and record it again.");
        else if (s.peakDb > -3.0) a.add ("Recorded hot: peaks at " + d1 (s.peakDb) + " dBFS. It did not clip, but a louder phrase would have; set the preamp so the loudest moments reach about -10 dBFS.");
        else if (s.peakDb < -24.0) a.add ("Recorded low: peaks at " + d1 (s.peakDb) + " dBFS. Raise the preamp so the loudest moments reach about -10 dBFS; the noise floor comes up with every decibel added later.");
        const double range = s.peakDb - s.noiseDb;
        if (s.noiseDb > -50.0) a.add ("Noisy room or chain: the quiet between phrases sits at " + d1 (s.noiseDb) + " dBFS (" + d1 (range) + " dB under the peaks). Move closer to the microphone, turn off fans and air conditioning, and check the cable and preamp before the next take.");
        else if (s.noiseDb > -60.0 && range < 50.0) a.add ("Some background noise: the quiet between phrases sits at " + d1 (s.noiseDb) + " dBFS. Usable, but compression will lift it; a gate or a quieter room helps.");
        if (std::abs (s.dc) > 0.01) a.add ("DC offset of " + juce::String (s.dc, 3) + ": the waveform is not centred, which costs headroom. A high-pass at 20 Hz removes it.");
        if (s.correlation < 0.0) a.add ("Left and right are out of phase (correlation " + juce::String (s.correlation, 2) + "): one channel or one microphone is flipped. Flip its polarity.");
        return a;
    }

    inline juce::String statsLine (const Stats& s)
    {
        return "peak " + d1 (s.peakDb) + " dBFS, average " + d1 (s.rmsDb) + " dBFS, peak-to-average " + d1 (s.crestDb) + " dB, stereo correlation " + juce::String (s.correlation, 2)
             + ", quietest moments " + d1 (s.noiseDb) + " dBFS";
    }

    inline juce::String bandsLine (const Stats& s)
    {
        juce::StringArray p; for (int b = 0; b < numBands; ++b) p.add (juce::String (bandNames[b]).upToFirstOccurrenceOf (" ", false, false) + " " + d1 (s.bandDb (b)));
        return p.joinIntoString (", ") + " dBFS";
    }
}
