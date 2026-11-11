// Measures the notation engine (Source/Notation.h). Build + run: scripts/build_voice111e_test.sh tests/notation_test.cpp notation_test
#include "MusicXml.h"
#include <cstdio>
using namespace Notation;

static int fails = 0, passes = 0;
static void expect (const juce::String& name, const juce::String& got, const juce::String& want)
{
    if (got.trim() == want.trim()) { ++passes; return; }
    ++fails; std::printf ("FAIL %s\n  got:  %s\n  want: %s\n", name.toRawUTF8(), got.trim().replace ("\n", " / ").toRawUTF8(), want.trim().replace ("\n", " / ").toRawUTF8());
}
static juce::String bars (std::vector<InNote> n, int num = 4, int den = 4, Key k = {}, Options o = {}, int show = 0)
{
    auto sc = build (n, { { 0, num, den } }, k, o, show);
    auto t = toText (sc);
    return t.fromFirstOccurrenceOf ("\n", false, false);   // drop the key line
}

int main()
{
    // 1. quarters in 4/4
    expect ("quarters", bars ({ { 0, 1, 60 }, { 1, 1, 62 }, { 2, 1, 64 }, { 3, 1, 65 } }), "Bar 1 (4/4): C4 quarter D4 quarter E4 quarter F4 quarter");
    // 2. whole, half + half
    expect ("whole", bars ({ { 0, 4, 67 } }), "Bar 1 (4/4): G4 whole");
    expect ("halves", bars ({ { 0, 2, 67 }, { 2, 2, 69 } }), "Bar 1 (4/4): G4 half A4 half");
    // 3. dotted quarter + eighth
    expect ("dotted", bars ({ { 0, 1.5, 60 }, { 1.5, 0.5, 62 }, { 2, 2, 64 } }), "Bar 1 (4/4): C4 dotted quarter D4 eighth E4 half");
    // 4. syncopation: eighth, quarter, eighth -> eighth, eighth~eighth, eighth
    expect ("syncopation", bars ({ { 0, 0.5, 60 }, { 0.5, 1, 62 }, { 1.5, 0.5, 64 }, { 2, 2, 65 } }),
            "Bar 1 (4/4): C4 eighth D4 eighth~ D4 eighth E4 eighth F4 half");
    // 5. half note on beat 2 shows the middle of the bar
    expect ("middle", bars ({ { 0, 1, 60 }, { 1, 2, 62 }, { 3, 1, 64 } }), "Bar 1 (4/4): C4 quarter D4 quarter~ D4 quarter E4 quarter");
    // 6. rests: beat 1 rest, then notes
    expect ("rests", bars ({ { 1, 1, 60 }, { 3, 0.5, 62 } }), "Bar 1 (4/4): rest quarter C4 quarter rest quarter D4 eighth rest eighth");
    // 7. tie across the bar line
    expect ("barTie", bars ({ { 3, 2, 60 } }), "Bar 1 (4/4): rest half rest quarter C4 quarter~\nBar 2: C4 quarter rest quarter rest half");
    // 8. empty bar -> whole-bar rest
    expect ("emptyBar", bars ({ { 0, 1, 60 } }, 4, 4, {}, {}, 2), "Bar 1 (4/4): C4 quarter rest quarter rest half\nBar 2: whole-bar rest");
    // 9. chord
    expect ("chord", bars ({ { 0, 4, 60 }, { 0, 4, 64 }, { 0, 4, 67 } }), "Bar 1 (4/4): [C4 E4 G4] whole");
    // 10. accidentals carry through the bar and get cancelled
    expect ("accidentals", bars ({ { 0, 1, 61 }, { 1, 1, 61 }, { 2, 1, 60 }, { 3, 1, 61 } }), "Bar 1 (4/4): C#4 quarter C#4 quarter C4 quarter C#4 quarter");
    {
        auto sc = build ({ { 0, 1, 61 }, { 1, 1, 61 }, { 2, 1, 60 }, { 3, 1, 61 } }, { { 0, 4, 4 } }, {}, {});
        auto& ev = sc.measures[0].staves[0].events;
        juce::String shown; for (auto& e : ev) if (! e.rest) shown << (e.heads[0].showAccidental ? juce::String (e.heads[0].shownAccidental) : juce::String ("-")) << " ";
        expect ("accidental display", shown, "1 - 0 1");
    }
    // 11. key of F: Bb is in the key (no accidental), B natural shows one
    {
        auto sc = build ({ { 0, 1, 70 }, { 1, 1, 71 }, { 2, 1, 70 } }, { { 0, 4, 4 } }, { -1, false }, {});
        auto& ev = sc.measures[0].staves[0].events;
        juce::String t; for (auto& e : ev) { if (e.rest) { t << "rest "; continue; } t << headName (e.heads[0]) << (e.heads[0].showAccidental ? "(" + juce::String (e.heads[0].shownAccidental) + ")" : juce::String()) << " "; }
        expect ("key of F", t, "Bb4 B4(0) Bb4(-1) rest");
    }
    // 12. Eb minor (6 flats): Gb, Cb spelled in key
    expect ("Eb minor", bars ({ { 0, 1, 63 }, { 1, 1, 66 }, { 2, 1, 70 }, { 3, 1, 71 } }, 4, 4, { -6, true }), "Bar 1 (4/4): Eb4 quarter Gb4 quarter Bb4 quarter Cb5 quarter");
    // 13. E major: D# spelled sharp, black keys outside the key sharp
    expect ("E major", bars ({ { 0, 1, 63 }, { 1, 1, 70 }, { 2, 2, 64 } }, 4, 4, { 4, false }), "Bar 1 (4/4): D#4 quarter A#4 quarter E4 half");
    // 14. C major black keys: C# Eb F# G# Bb
    expect ("C major spelling", bars ({ { 0, 0.5, 61 }, { 0.5, 0.5, 63 }, { 1, 0.5, 66 }, { 1.5, 0.5, 68 }, { 2, 2, 70 } }),
            "Bar 1 (4/4): C#4 eighth Eb4 eighth F#4 eighth G#4 eighth Bb4 half");
    // 15. 3/4: dotted half, half on beat 2 allowed
    expect ("3/4", bars ({ { 0, 3, 60 }, { 3, 1, 62 }, { 4, 2, 64 } }, 3, 4), "Bar 1 (3/4): C4 dotted half\nBar 2: D4 quarter E4 half");
    // 16. 6/8: dotted quarters, quarter + eighth
    expect ("6/8", bars ({ { 0, 1.5, 60 }, { 1.5, 1, 62 }, { 2.5, 0.5, 64 } }, 6, 8), "Bar 1 (6/8): C4 dotted quarter D4 quarter E4 eighth");
    // 17. triplets: three eighth-note triplets on beat 1
    expect ("triplets", bars ({ { 0, 1.0 / 3, 60 }, { 1.0 / 3, 1.0 / 3, 62 }, { 2.0 / 3, 1.0 / 3, 64 }, { 1, 3, 65 } }),
            "Bar 1 (4/4): C4 triplet eighth D4 triplet eighth E4 triplet eighth F4 quarter~ F4 half");
    // 18. sixteenths and a dotted-eighth / sixteenth figure
    expect ("16ths", bars ({ { 0, 0.75, 60 }, { 0.75, 0.25, 62 }, { 1, 0.25, 64 }, { 1.25, 0.25, 65 }, { 1.5, 0.5, 67 }, { 2, 2, 72 } }),
            "Bar 1 (4/4): C4 dotted eighth D4 16th E4 16th F4 16th G4 eighth C5 half");
    // 19. display quantise: slightly late / early notes snap
    expect ("quantise", bars ({ { 0.02, 0.97, 60 }, { 0.98, 1.05, 62 }, { 2.01, 1.98, 64 } }), "Bar 1 (4/4): C4 quarter D4 quarter E4 half");
    // 20. overlapping legato is written as one voice
    expect ("legato", bars ({ { 0, 1.5, 60 }, { 1, 1.5, 62 }, { 2, 2, 64 } }), "Bar 1 (4/4): C4 quarter D4 quarter E4 half");
    // 21. grand staff splits at middle C
    {
        auto sc = build ({ { 0, 4, 48 }, { 0, 4, 72 } }, { { 0, 4, 4 } }, {}, {});
        expect ("grand staff", toText (sc).fromFirstOccurrenceOf ("\n", false, false), "Bar 1 (4/4): RH C5 whole | LH C3 whole");
        expect ("grand staff clefs", juce::String (sc.numStaves) + " " + juce::String ((int) sc.clefs[1]), "2 1");
    }
    // 22. bass clef chosen for low parts, staff lines
    {
        auto sc = build ({ { 0, 4, 43 } }, { { 0, 4, 4 } }, {}, {});
        expect ("bass clef", juce::String ((int) sc.clefs[0]) + " line " + juce::String (sc.measures[0].staves[0].events[0].heads[0].staffLine), "1 line 0");
        auto tr = build ({ { 0, 4, 64 } }, { { 0, 4, 4 } }, {}, {});
        expect ("treble E4 bottom line", juce::String (tr.measures[0].staves[0].events[0].heads[0].staffLine), "0");
    }
    // 23. beams: four eighths in 4/4 beam in pairs by beat; stems
    {
        auto sc = build ({ { 0, 0.5, 72 }, { 0.5, 0.5, 74 }, { 1, 0.5, 72 }, { 1.5, 0.5, 74 }, { 2, 2, 60 } }, { { 0, 4, 4 } }, {}, {});
        juce::String b; for (auto& e : sc.measures[0].staves[0].events) b << e.beamGroup << (e.stemUp ? "u " : "d ");
        expect ("beams + stems", b, "0d 0d 1d 1d -1u");
    }
    // 24. tied accidental across the bar keeps its spelling and hides the sign
    {
        auto sc = build ({ { 3, 2, 66 }, { 5, 1, 66 } }, { { 0, 4, 4 } }, {}, {});
        auto& e1 = sc.measures[1].staves[0].events;
        juce::String t; for (auto& e : e1) if (! e.rest) t << headName (e.heads[0]) << (e.heads[0].showAccidental ? "!" : "") << " ";
        expect ("tie across bar keeps F#", t, "F#4 F#4!");
    }
    // 25. meter change
    {
        auto sc = build ({ { 0, 4, 60 }, { 4, 3, 62 } }, { { 0, 4, 4 }, { 1, 3, 4 } }, {}, {});
        expect ("meter change", toText (sc).fromFirstOccurrenceOf ("\n", false, false), "Bar 1 (4/4): C4 whole\nBar 2 (3/4): D4 dotted half");
    }
    // 26. parsing
    expect ("parse", juce::String (parsePitch ("C#5")) + " " + juce::String (parsePitch ("Bb3")) + " " + juce::String (parsePitch ("c4")) + " " + juce::String (parseDuration ("dotted eighth"))
            + " " + juce::String (parseDuration ("triplet quarter"), 3) + " " + juce::String (parseDuration ("3/16")), "73 58 60 0.75 0.667 0.75");
    // 27. MusicXML round trip: melody with ties, triplets, accidentals, chords, rests, grand staff, meter change
    {
        std::vector<InNote> in { { 0, 1.5, 61 }, { 1.5, 0.5, 63 }, { 2, 2.5, 66 }, { 4.5, 0.5, 68 }, { 5, 1.0 / 3, 70 }, { 5 + 1.0 / 3, 1.0 / 3, 72 }, { 5 + 2.0 / 3, 1.0 / 3, 74 },
                                 { 6, 2, 48 }, { 6, 2, 76 }, { 8, 3, 55 } };
        std::vector<Meter> meters { { 0, 4, 4 }, { 2, 3, 4 } };
        auto sc = build (in, meters, { 2, false }, {});
        auto xml = MusicXml::exportScore ({ { "Lead", sc } }, "Round Trip", "Jamar Stewart", 96);
        juce::String err;
        auto back = MusicXml::importScore (xml, err);
        juce::String got, want;
        auto key = [] (const InNote& n) { return juce::String (n.start, 3) + ":" + juce::String (n.length, 3) + ":" + juce::String (n.pitch); };
        auto sorted = [&] (std::vector<InNote> v) { std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.start < b.start - 1e-6 || (std::abs (a.start - b.start) < 1e-6 && a.pitch < b.pitch); }); juce::String t; for (auto& n : v) t << key (n) << " "; return t; };
        want = sorted (in);
        got = back.empty() ? juce::String ("(none) " + err) : sorted (back[0].notes);
        expect ("MusicXML round trip notes", got, want);
        expect ("MusicXML round trip header", back.empty() ? juce::String() : back[0].name + " " + juce::String (back[0].key.fifths) + " " + juce::String (back[0].tempo, 0) + " " + juce::String ((int) back[0].meters.size()) + " " + juce::String (back[0].meters.back().num), "Lead 2 96 2 3");
        expect ("MusicXML parses", juce::String (juce::XmlDocument::parse (xml) != nullptr ? 1 : 0) + (xml.contains ("<tuplet type=\"start\"") ? " tuplet" : "") + (xml.contains ("<staves>2</staves>") ? " grand" : "") + (xml.contains ("<accidental>") ? " acc" : ""), "1 tuplet grand acc");
        juce::File ("/tmp/notation_roundtrip.musicxml").replaceWithText (xml);
    }
    std::printf ("notation: %d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
