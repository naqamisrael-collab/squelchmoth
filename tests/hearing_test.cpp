// Squelchmoth 1.25 - hearing in colour and number: known answers.
#include <JuceHeader.h>
#include "SqHearing.h"
#include <cstdio>
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); ok ? ++passed : ++failed; }
static bool near (double a, double b, double tol) { return std::abs (a - b) <= tol; }
int main()
{
    using namespace SqHear;
    std::printf ("== Hearing: numbers ==\n");
    { const auto n = nearestNote (440.0); check (n.name == "A4" && near (n.cents, 0.0, 1e-9), "440 Hz is A4, 0 cents"); }
    { const auto n = nearestNote (445.0); check (n.name == "A4" && near (n.cents, 19.56, 0.01), "445 Hz is A4 +" + juce::String (n.cents, 2) + " cents (19.56)"); }
    { const auto n = nearestNote (261.6256); check (n.name == "C4" && near (n.cents, 0.0, 0.01), "261.63 Hz is C4 (middle C)"); }
    check (near (midiToHz (60), 261.6256, 0.0005) && near (hzToMidi (880.0), 81.0, 1e-9), "note number and frequency convert both ways");
    {
        const auto h = harmonics (110.0, 16);
        check (h[2].note.name == "E4" && near (h[2].note.cents, 1.955, 0.005) && h[2].ratioNum == 3 && h[2].ratioDen == 2 && h[2].interval == "perfect fifth", "3rd harmonic of A2: E4 +1.96 cents, 3:2, a perfect fifth");
        check (h[4].note.name == "C#5" && near (h[4].note.cents, -13.686, 0.005) && h[4].interval == "major third", "5th harmonic: C#5 -13.69 cents, 5:4, a major third");
        check (h[6].note.name == "G5" && near (h[6].note.cents, -31.174, 0.005) && h[6].ratioNum == 7 && h[6].ratioDen == 4, "7th harmonic: G5 -31.17 cents, 7:4");
        check (h[7].note.name == "A5" && near (h[7].note.cents, 0.0, 1e-6) && h[7].ratioNum == 1 && h[11].ratioNum == 3 && h[11].ratioDen == 2, "8th harmonic is an octave of the fundamental; the 12th folds to 3:2");
    }
    check (near (parseNoteOrHz ("A4"), 440.0, 1e-9) && near (parseNoteOrHz ("c#3"), midiToHz (49), 1e-9) && near (parseNoteOrHz ("Bb2"), midiToHz (46), 1e-9) && near (parseNoteOrHz ("440 Hz"), 440.0, 1e-9)
           && near (parseNoteOrHz ("82.41"), 82.41, 1e-9) && parseNoteOrHz ("hello") == 0.0 && parseNoteOrHz ("") == 0.0 && near (parseNoteOrHz ("C-1"), midiToHz (0), 1e-9),
           "typed notes and frequencies are read: A4, c#3, Bb2, 440 Hz, 82.41; nonsense is refused");
    check (near (erbWidth (1000.0), 132.639, 0.001) && near (erbWidth (100.0), 35.494, 0.001), "ERB width: 132.6 Hz at 1 kHz, 35.5 Hz at 100 Hz");
    check (near (erbNumber (1000.0), 15.621, 0.002) && near (erbNumber (0.0), 0.0, 1e-12), "ERB-number: 15.62 at 1 kHz");
    check (near (bark (1000.0), 8.5, 1e-9) && near (bark (100.0), 1.0, 1e-9) && near (bark (15500.0), 24.0, 1e-9) && near (bark (50.0), 0.5, 1e-9), "Bark: 1 kHz is half way through the 9th critical band (920 to 1080 Hz)");
    check (near (sonesFromPhons (40.0), 1.0, 1e-12) && near (sonesFromPhons (50.0), 2.0, 1e-12) && near (sonesFromPhons (70.0), 8.0, 1e-12) && near (phonsFromSones (4.0), 60.0, 1e-9), "sones: 40 phons = 1, every 10 phons doubles");
    check (near (beatHz (440.0, 443.0), 3.0, 1e-12), "440 and 443 Hz beat 3 times a second");
    {
        const auto m = roomModes (5.0, 4.0, 2.5, 120.0);
        check (m.size() > 4 && near (m[0].hz, 34.3, 0.001) && m[0].p == 1 && m[0].q == 0 && juce::String (m[0].kind) == "axial", "5 x 4 x 2.5 m room: lowest mode 34.3 Hz along the length");
        check (near (m[1].hz, 42.875, 0.001) && m[1].q == 1 && near (m[2].hz, 54.908, 0.002) && juce::String (m[2].kind) == "tangential", "then 42.9 Hz across the width and 54.9 Hz (length and width together)");
        bool sorted = true, hasHeight = false; for (size_t i = 1; i < m.size(); ++i) { if (m[i].hz < m[i - 1].hz) sorted = false; }
        for (auto& x : m) if (x.p == 0 && x.q == 0 && x.r == 1 && near (x.hz, 68.6, 0.001)) hasHeight = true;
        check (sorted && hasHeight, "modes come lowest first; the floor-to-ceiling mode is 68.6 Hz");
    }

    std::printf ("== Hearing: two sounds in the same place ==\n");
    {
        const double fs = 48000.0; const int n = 48000 * 3;
        auto tones = [&] (std::initializer_list<std::pair<double, double>> parts) { std::vector<float> v ((size_t) n, 0.0f); for (auto& p : parts) for (int i = 0; i < n; ++i) v[(size_t) i] += (float) (p.second * std::sin (2.0 * juce::MathConstants<double>::pi * p.first * i / fs)); return v; };
        const auto A = tones ({ { 230.0, 0.4 }, { 3000.0, 0.1 } }), B = tones ({ { 260.0, 0.2 }, { 800.0, 0.1 } }), C = tones ({ { 100.0, 0.3 } }), D = tones ({ { 5000.0, 0.3 } });
        const auto sa = barkSpectrum (A.data(), A.data(), n, fs), sb = barkSpectrum (B.data(), B.data(), n, fs), sc = barkSpectrum (C.data(), C.data(), n, fs), sd = barkSpectrum (D.data(), D.data(), n, fs);
        check (near (sa.db (2), -11.0, 0.3) && sa.db (2) > sa.db (1) + 30.0 && near (sa.db (15), -23.0, 0.3), "a 230 Hz tone at 0.4 reads -11 dBFS in the 200-300 Hz band and nowhere else nearby (" + juce::String (sa.db (2), 1) + ")");
        const auto o = findOverlap (sa, sb);
        check (o.found && o.firstBand == 2 && o.lastBand == 2 && near (o.lowHz, 200.0, 1e-9) && near (o.highHz, 300.0, 1e-9) && near (o.centreHz, 244.95, 0.01), "two sounds that both live at 200-300 Hz: that is the overlap found (centre " + juce::String (o.centreHz, 0) + " Hz, Q " + juce::String (o.q, 1) + ")");
        check (o.keepA && near (o.levelA - o.levelB, 6.02, 0.1) && near (o.cutDb, 2.99, 0.06), "the louder one there keeps the range; the other is cut " + juce::String (o.cutDb, 1) + " dB (levels " + juce::String (o.levelA, 1) + " and " + juce::String (o.levelB, 1) + " dBFS)");
        check (! findOverlap (sc, sd).found, "a 100 Hz sound and a 5 kHz sound do not overlap");
        const std::vector<float> z ((size_t) n, 0.0f);
        check (! findOverlap (sa, barkSpectrum (z.data(), z.data(), n, fs)).found, "nothing overlaps with silence");
    }
    std::printf ("== Hearing: colour ==\n");
    check (fifthsPosition (60) == 0 && fifthsPosition (67) == 1 && fifthsPosition (62) == 2 && fifthsPosition (65) == 11 && fifthsPosition (66) == 6, "pitch classes sit on the circle of fifths: C, G, D ... F");
    {
        const auto c4 = noteColour (60), c5 = noteColour (72), c2 = noteColour (36);
        check (toLab (c5).L > toLab (c4).L + 3.0 && toLab (c4).L > toLab (c2).L + 6.0, "higher octaves are lighter: C2 " + c2.hex() + ", C4 " + c4.hex() + ", C5 " + c5.hex());
        const auto quiet = toLab (noteColour (60, -50.0)), loud = toLab (noteColour (60, -3.0));
        check (std::hypot (loud.a, loud.b) > std::hypot (quiet.a, quiet.b) + 20.0, "louder is more vivid (chroma " + juce::String (std::hypot (quiet.a, quiet.b), 0) + " at -50 dB, " + juce::String (std::hypot (loud.a, loud.b), 0) + " at -3 dB)");
        check (noteColour (60).hex() == noteColour (60).hex() && difference (noteColour (60), noteColour (60 + 12 * 0)) == 0.0 && noteColour (61).hex() != noteColour (60).hex(), "the same note is always the same colour");
        const double fifth = difference (noteColour (60, -6), noteColour (67, -6)), tritone = difference (noteColour (60, -6), noteColour (66, -6));
        check (tritone > fifth * 1.5, "a fifth apart is a near colour, a tritone apart is a far one (" + juce::String (fifth, 0) + " against " + juce::String (tritone, 0) + ")");
    }
    {
        const double n = smallestClassDifference (Vision::normal);
        check (n >= 15.0, "normal vision: the two closest pitch classes differ by " + juce::String (n, 1) + " (15 or more wanted)");
        // colour-deficient viewers: every octave from 1 to 7, at working loudness and for quiet notes
        double loudWorst = 1e9, quietWorst = 1e9;
        for (int o = 24; o <= 96; o += 12)
            for (auto v : { Vision::protan, Vision::deutan })
            {
                for (double l : { -12.0, -3.0 }) loudWorst = std::min (loudWorst, smallestClassDifference (v, o, l));
                quietWorst = std::min (quietWorst, smallestClassDifference (v, o, -30.0));
            }
        check (loudWorst >= 5.0, "red-blind and green-blind vision, octaves 1 to 7, -12 to -3 dB: the closest pair of pitch classes differs by " + juce::String (loudWorst, 1) + " (5 = clearly different; wanted 5 or more)");
        check (quietWorst >= 3.0, "the same for quiet notes at -30 dB, which are pale by design: " + juce::String (quietWorst, 1) + " (3 or more wanted; note names are always shown beside the colour)");
        const auto red = RGB { 255, 0, 0 }; const auto s = seenAs (red, Vision::protan);
        check (s.r == s.g && s.r > 80 && s.r < 110, "the simulation maps pure red to a dark yellow for red-blind vision (" + s.hex() + ")");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
