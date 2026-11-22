#pragma once
// SqVinyl: checks on a mix before it goes to be cut to a record, and line-up tones. Pure C++ (no JUCE).
//
// A groove is cut sideways for what is the same in both channels and up-and-down for what differs, so bass that is
// not the same left and right makes the cutter dig and lift (the needle can jump); very strong highs - "s" sounds
// above all - are more than a cutter and a needle can follow cleanly, worst near the label where the groove runs
// slowest; and the longer a side, the quieter and thinner it has to be cut. The measurements are plain; where each
// is called a problem is OUR judgement, except the side lengths, which are a pressing plant's own published guidance.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "SqCapture.h"

namespace SqVinyl
{
    constexpr double pi = 3.14159265358979323846;
    /** Second-order Butterworth, low- or high-pass. */
    struct Biquad
    {
        void set (double hz, double fs, bool high) { const double w = 2 * pi * hz / fs, c = std::cos (w), a = std::sin (w) / std::sqrt (2.0), a0 = 1 + a; b0 = (high ? (1 + c) / 2 : (1 - c) / 2) / a0; b1 = (high ? -(1 + c) : (1 - c)) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - a) / a0; z1 = z2 = 0; }
        double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    };
    /** Three of them in a row: steep enough that a 60 Hz bass does not count as "below 30 Hz". */
    struct Steep { void set (double hz, double fs, bool high) { for (auto& b : s) b.set (hz, fs, high); } double run (double x) { for (auto& b : s) x = b.run (x); return x; } Biquad s[3]; };

    enum class Format { lp33, lp45, seven33, seven45 };
    struct Lengths { double ideal, fine, most; const char* name; };
    /** Minutes a side: ideal, still good, and where trouble starts - as one pressing plant publishes them. */
    inline Lengths lengthsFor (Format f)
    {
        switch (f) { case Format::lp45: return { 12.0, 14.0, 15.0, "12-inch at 45" }; case Format::seven33: return { 6.0, 6.0, 6.0, "7-inch at 33 1/3" }; case Format::seven45: return { 4.5, 4.5, 4.5, "7-inch at 45" }; default: return { 18.0, 20.0, 22.0, "12-inch at 33 1/3" }; }
    }

    struct Report
    {
        double seconds = 0, peakDb = -200;
        double bassSideDb = -200;        // under 150 Hz: what differs between the channels against what they share, whole programme
        double bassSideWorstDb = -200;   // the same over the worst second (where there is bass to speak of)
        double worstMatch = 1.0;         // lowest left-right agreement over any half second (1 = the same, 0 = unrelated, -1 = opposite)
        double oppositeShare = 0;        // share of the time the channels are more opposite than alike
        double essDb = -200;             // loudest 20 ms in the 5 to 10 kHz band, dBFS (rms)
        double essOverDb = -200;         // how far that stands above the band's own average
        double essShare = 0;             // share of 20 ms frames where that band carries most of the level and is loud
        double airDb = -200, subDb = -200;   // above 16 kHz and below 30 Hz, against the whole
        double frontMinusBackDb = 0;     // loudness of the first half of the side minus the second
    };

    inline Report analyze (const float* L, const float* R, int n, double fs)
    {
        Report r; r.seconds = n / fs; if (n < (int) fs / 2) return r;
        Steep loM, loS, air, sub; Biquad hpA, hpB, lpA, lpB; loM.set (150, fs, false); loS.set (150, fs, false);
        hpA.set (5000, fs, true); hpB.set (5000, fs, true); lpA.set (std::min (10000.0, 0.45 * fs), fs, false); lpB.set (std::min (10000.0, 0.45 * fs), fs, false); air.set (std::min (16000.0, 0.45 * fs), fs, true); sub.set (30, fs, false);
        const int sec = (int) fs, half = (int) (fs / 2), frame = (int) (fs / 50);
        double m2 = 0, s2 = 0, wm = 0, ws = 0, cLR = 0, cLL = 0, cRR = 0, all = 0, airE = 0, subE = 0, pk = 0, fE = 0, fA = 0, essSum = 0, first = 0, second = 0; int halves = 0, opposite = 0, frames = 0, hot = 0; double worstSide = -200, loudEss = 0;
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i], rr = R[i], mid = 0.5 * (l + rr), side = 0.5 * (l - rr); pk = std::max (pk, std::max (std::abs (l), std::abs (rr)));
            const double bm = loM.run (mid), bs = loS.run (side); m2 += bm * bm; s2 += bs * bs; wm += bm * bm; ws += bs * bs;
            if ((i + 1) % sec == 0) { if (wm / sec > 1.0e-5) worstSide = std::max (worstSide, 10 * std::log10 ((ws + 1e-30) / (wm + 1e-30))); wm = ws = 0; }
            cLR += l * rr; cLL += l * l; cRR += rr * rr;
            if ((i + 1) % half == 0) { if (cLL + cRR > half * 2.0e-6) { const double c = cLR / (std::sqrt (cLL * cRR) + 1e-30); r.worstMatch = std::min (r.worstMatch, c); ++halves; if (c < 0) ++opposite; } cLR = cLL = cRR = 0; }
            const double e = lpA.run (hpA.run (l)), e2 = lpB.run (hpB.run (rr)); fE += 0.5 * (e * e + e2 * e2); fA += 0.5 * (l * l + rr * rr);
            if ((i + 1) % frame == 0) { const double ess = fE / frame, tot = fA / frame; essSum += ess; ++frames; loudEss = std::max (loudEss, ess); if (ess > 0.5 * tot && ess > 1.0e-3) ++hot; fE = fA = 0; }
            const double a = air.run (mid), sb = sub.run (mid); airE += a * a; subE += sb * sb; all += mid * mid + side * side; (i < n / 2 ? first : second) += l * l + rr * rr;
        }
        r.peakDb = 20 * std::log10 (pk + 1e-30); r.bassSideDb = 10 * std::log10 ((s2 + 1e-30) / (m2 + 1e-30)); r.bassSideWorstDb = worstSide; r.oppositeShare = halves > 0 ? (double) opposite / halves : 0.0;
        r.essDb = 10 * std::log10 (loudEss + 1e-30); r.essOverDb = frames > 0 ? r.essDb - 10 * std::log10 (essSum / frames + 1e-30) : 0.0; r.essShare = frames > 0 ? (double) hot / frames : 0.0;
        r.airDb = 10 * std::log10 ((airE + 1e-30) / (all + 1e-30)); r.subDb = 10 * std::log10 ((subE + 1e-30) / (all + 1e-30)); r.frontMinusBackDb = 10 * std::log10 ((first + 1e-30) / (second + 1e-30));
        return r;
    }

    struct Finding { bool problem; std::string text; };
    static inline std::string num (double v, int places = 1) { char b[40]; std::snprintf (b, sizeof (b), "%.*f", places, v); return b; }
    /** What to do about it, in plain words. The thresholds are ours (see the top of the file); the side lengths are the plant's. */
    inline std::vector<Finding> advise (const Report& r, Format f)
    {
        std::vector<Finding> out; const auto len = lengthsFor (f); const double minutes = r.seconds / 60.0;
        if (minutes > len.most) out.push_back ({ true, "Side length " + num (minutes) + " min: over the " + num (len.most, len.most == std::floor (len.most) ? 0 : 1) + " min where a " + len.name + " side runs into trouble (ideal " + num (len.ideal, len.ideal == std::floor (len.ideal) ? 0 : 1) + "). It would have to be cut quieter and thinner - split it or drop a song." });
        else if (minutes > len.fine) out.push_back ({ true, "Side length " + num (minutes) + " min: past the " + num (len.fine, 0) + " min that is still good for a " + len.name + " side; expect a quieter cut." });
        else if (minutes > len.ideal) out.push_back ({ false, "Side length " + num (minutes) + " min: fine for a " + len.name + " side (ideal is " + num (len.ideal, 0) + " or less)." });
        else out.push_back ({ false, "Side length " + num (minutes) + " min: inside the ideal for a " + len.name + " side." });
        const double bass = std::max (r.bassSideDb, r.bassSideWorstDb);
        if (bass > -12.0) out.push_back ({ true, "Bass is not centred: under 150 Hz the difference between the channels is only " + num (-bass) + " dB below what they share (worst second). Make everything under about 150 Hz mono - pan bass and kick to the middle, or narrow the low band - or the cutter has to cut it up-and-down." });
        else out.push_back ({ false, "Bass is centred: under 150 Hz the channels' difference is " + num (-bass) + " dB below what they share." });
        if (r.worstMatch < -0.3 || r.oppositeShare > 0.05) out.push_back ({ true, "Out-of-phase passages: the channels run opposite to each other " + num (r.oppositeShare * 100.0, 0) + " % of the time (worst half second " + num (r.worstMatch, 2) + ", where -1 is fully opposite). Check wide stereo effects and flipped polarity." });
        else out.push_back ({ false, "No out-of-phase passages to worry about (lowest left-right agreement " + num (r.worstMatch, 2) + ")." });
        if (r.essDb > -15.0 || (r.essDb > -24.0 && r.essOverDb > 15.0)) out.push_back ({ true, "Hot highs: the 5 to 10 kHz band reaches " + num (r.essDb) + " dBFS in its loudest moment, " + num (r.essOverDb) + " dB over its own average. De-ess the vocal and tame cymbals and hats, or they will smear - most of all near the end of a side." });
        else out.push_back ({ false, "Highs are under control (5 to 10 kHz peaks at " + num (r.essDb) + " dBFS)." });
        if (r.subDb > -20.0) out.push_back ({ true, "A lot below 30 Hz (" + num (r.subDb) + " dB against the whole mix): it takes groove room without being heard. A low cut around 30 Hz gives the cut more level." });
        if (r.airDb > -25.0) out.push_back ({ true, "A lot above 16 kHz (" + num (r.airDb) + " dB against the whole mix): the cutting room will likely limit it; ease it yourself and keep control of how." });
        if (r.frontMinusBackDb < -2.0) out.push_back ({ false, "The second half of this side is " + num (-r.frontMinusBackDb) + " dB louder than the first. Records sound best at the outside: put the loudest, brightest songs first on each side." });
        return out;
    }

    // ---- line-up tones
    /** A sine at a level (dBFS peak), with 10 ms fades. */
    inline std::vector<float> tone (double hz, double seconds, double levelDb, double fs)
    {
        const int n = (int) (seconds * fs), fade = (int) (0.01 * fs); std::vector<float> x ((size_t) n); const double a = std::pow (10.0, levelDb / 20.0);
        for (int i = 0; i < n; ++i) { double g = 1.0; if (i < fade) g = (double) i / fade; if (i > n - fade) g = (double) (n - i) / fade; x[(size_t) i] = (float) (a * g * std::sin (2 * pi * hz * i / fs)); }
        return x;
    }
    /** Pink noise (equal level in every octave, 20 Hz to 20 kHz) at an rms level in dBFS; the same every time for a seed. */
    inline std::vector<float> pink (double seconds, double rmsDb, double fs, unsigned seed = 1)
    {
        size_t N = 1; while (N < (size_t) (seconds * fs)) N <<= 1; std::vector<SqCapture::cd> s (N); unsigned st = seed * 2654435761u + 12345u;
        auto rnd = [&st] { st = st * 1664525u + 1013904223u; return (double) (st >> 8) / 16777216.0; };
        for (size_t k = 1; k < N / 2; ++k) { const double f = k * fs / N; if (f < 20.0 || f > 20000.0) continue; s[k] = std::polar (1.0 / std::sqrt (f), 2 * pi * rnd()); s[N - k] = std::conj (s[k]); }
        SqCapture::fft (s, true); const int n = (int) (seconds * fs), fade = (int) (0.01 * fs); std::vector<float> x ((size_t) n); double e = 0; for (int i = 0; i < n; ++i) e += s[(size_t) i].real() * s[(size_t) i].real();
        const double g = std::pow (10.0, rmsDb / 20.0) / std::sqrt (e / n + 1e-300);
        for (int i = 0; i < n; ++i) { double w = 1.0; if (i < fade) w = (double) i / fade; if (i > n - fade) w = (double) (n - i) / fade; x[(size_t) i] = (float) (g * w * s[(size_t) i].real()); }
        return x;
    }
}
