#pragma once
// Squelchmoth's shared musical scales (used by the piano roll, MIDI tools, Beat Lab and Moth Tune).
#include <JuceHeader.h>

namespace SqScales
{
    struct Scale { const char* name; std::vector<int> steps; };
    inline const std::vector<Scale>& scales()
    {
        static const std::vector<Scale> s {
            { "Chromatic",        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
            { "Major",            { 0, 2, 4, 5, 7, 9, 11 } },
            { "Natural Minor",    { 0, 2, 3, 5, 7, 8, 10 } },
            { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 } },
            { "Melodic Minor",    { 0, 2, 3, 5, 7, 9, 11 } },
            { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 } },
            { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 } },
            { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 } },
            { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 } },
            { "Locrian",          { 0, 1, 3, 5, 6, 8, 10 } },
            { "Major Pentatonic", { 0, 2, 4, 7, 9 } },
            { "Minor Pentatonic", { 0, 3, 5, 7, 10 } },
            { "Blues",            { 0, 3, 5, 6, 7, 10 } },
            { "Phrygian Dominant",{ 0, 1, 4, 5, 7, 8, 10 } },
            { "Hungarian Minor",  { 0, 2, 3, 6, 7, 8, 11 } },
            { "Japanese (In Sen)",{ 0, 1, 5, 7, 10 } } };
        return s;
    }
    inline const juce::StringArray& noteNames()   { static juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n; }

    /** 12-bit pitch-class mask of a scale on a root (bit 0 = C). */
    inline int maskFor (int root, int scale)
    {
        auto& s = scales();
        if (! juce::isPositiveAndBelow (scale, (int) s.size())) return 0xfff;
        int m = 0;
        for (int st : s[(size_t) scale].steps) m |= 1 << ((root + st) % 12);
        return m;
    }

    /** Krumhansl-Schmuckler key finding from a pitch-class weight histogram. Returns { root, isMinor, score }. */
    struct KeyGuess { int root = 0; bool minor = false; double score = 0; };
    inline KeyGuess guessKey (const std::array<double, 12>& hist)
    {
        static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto corr = [&] (const double* prof, int root)
        {
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += hist[(size_t) i]; my += prof[i]; }
            mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double x = hist[(size_t) ((i + root) % 12)] - mx, y = prof[i] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return sxx > 0 && syy > 0 ? sxy / std::sqrt (sxx * syy) : 0.0;
        };
        KeyGuess best; best.score = -2;
        for (int r = 0; r < 12; ++r)
        {
            if (auto c = corr (maj, r); c > best.score) best = { r, false, c };
            if (auto c = corr (mnr, r); c > best.score) best = { r, true, c };
        }
        return best;
    }
}
