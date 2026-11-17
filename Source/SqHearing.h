#pragma once
// Squelchmoth 1.25 - hearing in colour and number, part 1.
//
// One colour language for sound: pitch class = hue, placed around the circle of fifths (notes a fifth apart are
// neighbours in colour); octave = lightness; loudness = how vivid the colour is. And the numbers a listener uses:
// note name and cents, critical-band scales (Bark, ERB), sones, beat frequency, the harmonic series, room modes.
//
// Sources (docs/internal/research/P6-hearing.md): ERB width and ERB-number after Glasberg and Moore; Bark from the
// 24 critical-band edges of Zwicker; colour-vision-deficiency simulation with the matrices of Vienot, Brettel and
// Mollon (1999). Pure functions, no engine or GUI types: tested on their own in tests/hearing_test.cpp.

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace SqHear
{
    //============================================================================== numbers
    inline double hzToMidi (double hz)   { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
    inline double midiToHz (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }

    inline juce::String noteName (int midi)
    {
        static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String ((int) std::floor (midi / 12.0) - 1);
    }

    struct Note { int midi = 69; double cents = 0, hz = 440; juce::String name; };

    /** The nearest equal-tempered note (A4 = 440 Hz) and how many cents the frequency is above it. */
    inline Note nearestNote (double hz)
    {
        Note n; n.hz = hz; if (hz <= 0) return n;
        const double m = hzToMidi (hz); n.midi = (int) std::lround (m); n.cents = (m - n.midi) * 100.0; n.name = noteName (n.midi);
        return n;
    }

    /** Reads "A4", "c#3", "Bb2", "440" or "440 Hz": the frequency in Hz, or 0 when it is neither a note nor a number. */
    inline double parseNoteOrHz (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase().upToFirstOccurrenceOf ("hz", false, false).trim();
        if (t.isEmpty()) return 0.0;
        if (t.containsOnly ("0123456789.")) { const double v = t.getDoubleValue(); return v > 0 ? v : 0.0; }
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };     // a b c d e f g
        const auto c = t[0]; if (c < 'a' || c > 'g') return 0.0;
        int pc = base[c - 'a'], i = 1;
        if (i < t.length() && t[i] == '#') { ++pc; ++i; } else if (i < t.length() && t[i] == 'b') { --pc; ++i; }
        const auto rest = t.substring (i).trim();
        if (rest.isEmpty() || ! rest.containsOnly ("-0123456789")) return 0.0;
        return midiToHz ((rest.getIntValue() + 1) * 12 + pc);
    }

    /** Width in Hz of the ear's filter centred on f (equivalent rectangular bandwidth). */
    inline double erbWidth (double hz) { return 24.7 * (4.37 * hz / 1000.0 + 1.0); }
    /** How many ERBs lie below f: the ERB-number scale. */
    inline double erbNumber (double hz) { return 21.4 * std::log10 (0.00437 * hz + 1.0); }

    inline const double barkEdges[25] = { 0, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500 };
    /** Position on the Bark scale: the critical band the frequency falls in (0 to 24) and how far through it. */
    inline double bark (double hz)
    {
        if (hz <= 0) return 0.0;
        for (int b = 0; b < 24; ++b) if (hz < barkEdges[b + 1]) return b + (hz - barkEdges[b]) / (barkEdges[b + 1] - barkEdges[b]);
        return 24.0;
    }

    /** Loudness in sones from loudness level in phons, by definition: 40 phons = 1 sone, +10 phons doubles it (40 phons and up). */
    inline double sonesFromPhons (double phons) { return std::pow (2.0, (phons - 40.0) / 10.0); }
    inline double phonsFromSones (double sones) { return 40.0 + 10.0 * std::log2 (sones); }

    /** Two close tones beat at the difference of their frequencies. */
    inline double beatHz (double f1, double f2) { return std::abs (f1 - f2); }

    struct Partial { int number = 1; double hz = 0; Note note; int ratioNum = 1, ratioDen = 1; juce::String interval; };

    /** The harmonic series on a fundamental: each partial, the note it falls nearest and how far off, and its ratio folded into one octave. */
    inline std::vector<Partial> harmonics (double fundamentalHz, int count)
    {
        std::vector<Partial> v;
        for (int k = 1; k <= count; ++k)
        {
            Partial p; p.number = k; p.hz = fundamentalHz * k; p.note = nearestNote (p.hz);
            int den = 1; while (den * 2 <= k) den *= 2;
            int num = k; while (num % 2 == 0 && den > 1) { num /= 2; den /= 2; }
            p.ratioNum = num; p.ratioDen = den;
            const int key = num * 100 + den;
            p.interval = key == 101 ? "octave of the fundamental" : key == 302 ? "perfect fifth" : key == 504 ? "major third" : key == 704 ? "harmonic seventh"
                       : key == 908 ? "major second" : key == 1108 ? "harmonic eleventh (between fourth and tritone)" : key == 1308 ? "harmonic thirteenth (between sixths)"
                       : key == 1508 ? "major seventh" : juce::String (num) + ":" + juce::String (den);
            v.push_back (p);
        }
        return v;
    }

    struct RoomMode { double hz = 0; int p = 0, q = 0, r = 0; const char* kind = "axial"; };

    /** Standing waves of a rectangular room (metres; speed of sound 343 m/s at 20 C), lowest first, up to maxHz. */
    inline std::vector<RoomMode> roomModes (double length, double width, double height, double maxHz = 300.0, double c = 343.0)
    {
        std::vector<RoomMode> v;
        if (length <= 0 || width <= 0 || height <= 0) return v;
        const int top = (int) std::ceil (2.0 * std::max ({ length, width, height }) * maxHz / c) + 1;
        for (int p = 0; p <= top; ++p) for (int q = 0; q <= top; ++q) for (int r = 0; r <= top; ++r)
        {
            if (p + q + r == 0) continue;
            const double f = 0.5 * c * std::sqrt ((p / length) * (p / length) + (q / width) * (q / width) + (r / height) * (r / height));
            if (f > maxHz) continue;
            const int nz = (p > 0) + (q > 0) + (r > 0);
            v.push_back ({ f, p, q, r, nz == 1 ? "axial" : nz == 2 ? "tangential" : "oblique" });
        }
        std::sort (v.begin(), v.end(), [] (const RoomMode& a, const RoomMode& b) { return a.hz < b.hz; });
        return v;
    }

    //============================================================================== two sounds in the same place
    /** A sound's energy in each of the 24 critical bands (mean square of the mono sum). */
    struct BarkSpectrum
    {
        double power[24] = {};
        double db (int b) const   { return 10.0 * std::log10 (power[b] + 1e-20); }
        double peakDb() const     { double m = -200.0; for (int b = 0; b < 24; ++b) m = std::max (m, db (b)); return m; }
        bool silent() const       { return peakDb() < -80.0; }
        double regionDb (int first, int last) const { double p = 0; for (int b = first; b <= last; ++b) p += power[b]; return 10.0 * std::log10 (p + 1e-20); }
    };

    inline BarkSpectrum barkSpectrum (const float* L, const float* R, int n, double fs)
    {
        BarkSpectrum s; if (n <= 0 || fs <= 0) return s;
        if (R == nullptr) R = L;
        const int order = 13, N = 1 << order, hop = N / 2;
        juce::dsp::FFT fft (order);
        std::vector<float> win ((size_t) N), buf ((size_t) (2 * N));
        double wsum = 0; for (int i = 0; i < N; ++i) { win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N); wsum += (double) win[(size_t) i] * win[(size_t) i]; }
        int frames = 0;
        for (int p = 0; p < n; p += hop)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            const int have = std::min (N, n - p);
            for (int i = 0; i < have; ++i) buf[(size_t) i] = 0.5f * (L[p + i] + R[p + i]) * win[(size_t) i];
            fft.performRealOnlyForwardTransform (buf.data(), true);
            int band = 0;
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = k * fs / N; if (f >= barkEdges[24]) break;
                while (band < 23 && f >= barkEdges[band + 1]) ++band;
                s.power[band] += ((double) buf[(size_t) (2 * k)] * buf[(size_t) (2 * k)] + (double) buf[(size_t) (2 * k + 1)] * buf[(size_t) (2 * k + 1)]) * 2.0 / (N * wsum);
            }
            ++frames;
            if (p + N >= n) break;
        }
        if (frames > 0) for (auto& v : s.power) v /= frames;
        return s;
    }

    /** Where two sounds sit on top of each other, and a starting point for moving one out of the way. */
    struct Overlap
    {
        bool found = false; int firstBand = 0, lastBand = 0;
        double lowHz = 0, highHz = 0, centreHz = 0, q = 1.0;
        double levelA = -200, levelB = -200;      // each sound's level inside the region, dBFS
        bool keepA = true;                        // the louder one there keeps the range
        double cutDb = 3.0;                       // suggested cut on the other
    };

    /** The critical bands that matter to BOTH sounds (within 18 dB of each one's own strongest band); the region where
        that is most true, up to three bands wide. Energy overlap on the Bark scale - not a masked-threshold model. */
    inline Overlap findOverlap (const BarkSpectrum& a, const BarkSpectrum& b)
    {
        Overlap o; if (a.silent() || b.silent()) return o;
        const double pa = a.peakDb(), pb = b.peakDb();
        bool relevant[24]; int best = -1; double bestScore = -1e9;
        for (int i = 0; i < 24; ++i)
        {
            const double ra = a.db (i) - pa, rb = b.db (i) - pb;
            relevant[i] = ra >= -18.0 && rb >= -18.0 && a.db (i) > -70.0 && b.db (i) > -70.0;
            if (relevant[i] && std::min (ra, rb) > bestScore) { bestScore = std::min (ra, rb); best = i; }
        }
        if (best < 0) return o;
        o.found = true; o.firstBand = o.lastBand = best;
        if (best > 0 && relevant[best - 1]) o.firstBand = best - 1;
        if (best < 23 && relevant[best + 1]) o.lastBand = best + 1;
        o.lowHz = std::max (20.0, barkEdges[o.firstBand]); o.highHz = barkEdges[o.lastBand + 1];
        o.centreHz = std::sqrt (o.lowHz * o.highHz); o.q = juce::jlimit (0.7, 6.0, o.centreHz / (o.highHz - o.lowHz));
        o.levelA = a.regionDb (o.firstBand, o.lastBand); o.levelB = b.regionDb (o.firstBand, o.lastBand);
        o.keepA = o.levelA >= o.levelB;
        o.cutDb = juce::jlimit (2.0, 6.0, 6.0 - 0.5 * std::abs (o.levelA - o.levelB));
        return o;
    }

    //============================================================================== colour
    struct RGB
    {
        juce::uint8 r = 0, g = 0, b = 0;
        juce::String hex() const { return "#" + juce::String::toHexString ((int) r).paddedLeft ('0', 2) + juce::String::toHexString ((int) g).paddedLeft ('0', 2) + juce::String::toHexString ((int) b).paddedLeft ('0', 2); }
    };
    struct Lab { double L = 0, a = 0, b = 0; };

    namespace detail
    {
        inline double toLinear (double v)  { return v <= 0.04045 ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4); }
        inline double toGamma (double v)   { return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow (v, 1.0 / 2.4) - 0.055; }
        inline void labToLinear (const Lab& c, double out[3])
        {
            const double fy = (c.L + 16.0) / 116.0, fx = fy + c.a / 500.0, fz = fy - c.b / 200.0;
            auto f = [] (double t) { return t * t * t > 0.008856 ? t * t * t : (t - 16.0 / 116.0) / 7.787; };
            const double x = 0.95047 * f (fx), y = f (fy), z = 1.08883 * f (fz);
            out[0] = 3.2404542 * x - 1.5371385 * y - 0.4985314 * z; out[1] = -0.9692660 * x + 1.8760108 * y + 0.0415560 * z; out[2] = 0.0556434 * x - 0.2040259 * y + 1.0572252 * z;
        }
        inline Lab linearToLab (const double in[3])
        {
            const double x = 0.4124564 * in[0] + 0.3575761 * in[1] + 0.1804375 * in[2], y = 0.2126729 * in[0] + 0.7151522 * in[1] + 0.0721750 * in[2], z = 0.0193339 * in[0] + 0.1191920 * in[1] + 0.9503041 * in[2];
            auto f = [] (double t) { return t > 0.008856 ? std::cbrt (t) : 7.787 * t + 16.0 / 116.0; };
            const double fx = f (x / 0.95047), fy = f (y), fz = f (z / 1.08883);
            return { 116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz) };
        }
        inline bool inGamut (const double c[3]) { for (int i = 0; i < 3; ++i) if (c[i] < -1e-4 || c[i] > 1.0 + 1e-4) return false; return true; }
        inline RGB pack (const double lin[3])
        {
            RGB o; juce::uint8* ch[3] = { &o.r, &o.g, &o.b };
            for (int i = 0; i < 3; ++i) *ch[i] = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (255.0 * toGamma (juce::jlimit (0.0, 1.0, lin[i]))));
            return o;
        }
        inline void unpack (RGB c, double lin[3]) { lin[0] = toLinear (c.r / 255.0); lin[1] = toLinear (c.g / 255.0); lin[2] = toLinear (c.b / 255.0); }
    }

    inline Lab toLab (RGB c) { double lin[3]; detail::unpack (c, lin); return detail::linearToLab (lin); }
    /** Colour difference (CIE 1976): about 2 is just visible side by side, 5 is clear, 10 and up is obvious. */
    inline double difference (RGB a, RGB b) { const auto x = toLab (a), y = toLab (b); return std::sqrt ((x.L - y.L) * (x.L - y.L) + (x.a - y.a) * (x.a - y.a) + (x.b - y.b) * (x.b - y.b)); }

    /** Where a pitch class sits on the circle of fifths: C 0, G 1, D 2, A 3, E 4, B 5, F# 6, C# 7, G# 8, D# 9, A# 10, F 11. */
    inline int fifthsPosition (int midi) { return ((((midi % 12) + 12) % 12) * 7) % 12; }

    // A small lightness step per pitch class, chosen by search so that the twelve colours stay apart for viewers with
    // the two common colour-vision deficiencies as well (measured in the test).
    inline const double classLightness[12] = { 8, -8, 0, 8, 8, -8, -2, 0, 0, -8, 2, -4 };

    /** The colour of a note. midi: note number (60 = C4). loudnessDb: -60 (pale) to 0 (vivid). */
    inline RGB noteColour (int midi, double loudnessDb = -12.0)
    {
        const int k = fifthsPosition (midi);
        const int octave = (int) std::floor (midi / 12.0) - 1;
        const double L = juce::jlimit (22.0, 88.0, 34.0 + 5.5 * juce::jlimit (0, 8, octave) + classLightness[k]);
        const double want = 18.0 + 57.0 * juce::jlimit (0.0, 1.0, (loudnessDb + 60.0) / 60.0);
        const double h = juce::degreesToRadians (25.0 + 30.0 * k);
        double lin[3]; double lo = 0.0, hi = want;
        detail::labToLinear ({ L, want * std::cos (h), want * std::sin (h) }, lin);
        if (! detail::inGamut (lin))
        {
            for (int i = 0; i < 22; ++i) { const double c = 0.5 * (lo + hi); detail::labToLinear ({ L, c * std::cos (h), c * std::sin (h) }, lin); if (detail::inGamut (lin)) lo = c; else hi = c; }
            detail::labToLinear ({ L, lo * std::cos (h), lo * std::sin (h) }, lin);
        }
        return detail::pack (lin);
    }

    enum class Vision { normal, protan, deutan };

    /** How a colour looks with red-blind (protan) or green-blind (deutan) vision. */
    inline RGB seenAs (RGB c, Vision v)
    {
        if (v == Vision::normal) return c;
        static const double P[9] = { 0.11238, 0.88762, 0.0, 0.11238, 0.88762, 0.0, 0.00401, -0.00401, 1.0 };
        static const double D[9] = { 0.29275, 0.70725, 0.0, 0.29275, 0.70725, 0.0, -0.02234, 0.02234, 1.0 };
        const double* m = v == Vision::protan ? P : D;
        double in[3], out[3]; detail::unpack (c, in);
        for (int i = 0; i < 3; ++i) out[i] = m[3 * i] * in[0] + m[3 * i + 1] * in[1] + m[3 * i + 2] * in[2];
        return detail::pack (out);
    }

    /** The smallest colour difference between any two of the twelve pitch classes (same octave and loudness) for a kind of vision. */
    inline double smallestClassDifference (Vision v, int octaveMidiC = 60, double loudnessDb = -6.0)
    {
        double best = 1e9;
        for (int i = 0; i < 12; ++i) for (int j = i + 1; j < 12; ++j)
            best = std::min (best, difference (seenAs (noteColour (octaveMidiC + i, loudnessDb), v), seenAs (noteColour (octaveMidiC + j, loudnessDb), v)));
        return best;
    }
}
