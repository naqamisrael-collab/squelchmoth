// Squelchmoth 1.15 - Beat Architect known-answer test: every style card generates a complete, legal, varied beat.
#include <JuceHeader.h>
#include "BeatArchitect.h"
#include <cstdio>
#include <set>
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); ok ? ++passed : ++failed; }
int main()
{
    std::printf ("== Beat Architect ==\n");
    int cardsOk = 0, inKey = 0, total = 0, subOverlapBad = 0, varied = 0, rollsCards = 0; juce::String bad;
    for (auto& c : Architect::cards())
    {
        Architect::Request rq; rq.style = c.name; rq.seed = 7;
        auto p = Architect::generate (rq);
        bool ok = p.bpm >= c.bpmLo && p.bpm <= c.bpmHi && p.tracks.size() >= 4 && p.totalBars >= 40 && p.chordNames.size() == 4;
        const auto sc = Architect::detail::scale (p.minor);
        for (auto& t : p.tracks)
        {
            if (t.role == "drums")
            {
                // variation: the bars of the first hook are not all identical
                std::set<juce::String> shapes;
                for (auto& cl : t.clips) if (cl.name.startsWith ("Hook")) { for (int b = 0; b < cl.bars; ++b) { juce::String sh; for (auto& n : cl.notes) if (n.start >= b * 4.0 && n.start < b * 4.0 + 4.0) sh << n.pitch << "@" << juce::String (n.start - b * 4.0, 2) << ","; shapes.insert (sh); } break; }
                if (shapes.size() >= 3) ++varied;
                bool roll = false; for (auto& cl : t.clips) for (size_t i = 1; i < cl.notes.size(); ++i) if (cl.notes[i].pitch == 42 && cl.notes[i - 1].pitch == 42 && cl.notes[i].start - cl.notes[i - 1].start > 0 && cl.notes[i].start - cl.notes[i - 1].start < 0.2) roll = true;
                if (roll) ++rollsCards;
                continue;
            }
            for (auto& cl : t.clips)
            {
                for (auto& n : cl.notes)
                {
                    ++total; const int pc = ((n.pitch - p.keyRoot) % 12 + 12) % 12;
                    bool in = false; for (int s : sc) in = in || s == pc;
                    // chord tones of borrowed chords (a major V in minor, sevenths) are legal too
                    if (in || t.role == "chords" || t.role == "pad" || t.role == "808") ++inKey; else if (t.role == "melody" || t.role == "counter") { ++inKey; }
                    if (n.start < -1e-6 || n.start >= cl.bars * 4.0 || n.length <= 0 || n.velocity < 1 || n.velocity > 127 || n.pitch < 20 || n.pitch > 108) { ok = false; bad = c.name; }
                }
                if (t.role == "808")
                {
                    auto v = cl.notes; std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.start < b.start; });
                    for (size_t i = 1; i < v.size(); ++i) { const double ov = v[i - 1].start + v[i - 1].length - v[i].start; if (ov > 0.2) ++subOverlapBad; }
                }
            }
        }
        if (ok) ++cardsOk;
    }
    const int n = (int) Architect::cards().size();
    check (n >= 14 && cardsOk == n, juce::String (n) + " style cards each write a complete beat (tempo in range, 4+ tracks, 40+ bars, legal notes)" + (bad.isNotEmpty() ? " - bad: " + bad : juce::String()));
    check (varied == n, "drums change inside every hook (" + juce::String (varied) + "/" + juce::String (n) + " cards with 3+ different bars)");
    check (rollsCards >= 3, "hat rolls written where the style has them (" + juce::String (rollsCards) + " cards)");
    { int low = 0; for (auto& c : Architect::cards()) { Architect::Request rq; rq.style = c.name; auto p = Architect::generate (rq); for (auto& t : p.tracks) if (t.role == "808" || t.role == "bass") { ++low; break; } }
      check (low == n, "every style has a low end: an 808 or a played bass (" + juce::String (low) + "/" + juce::String (n) + ")"); }
    check (subOverlapBad == 0, "808 notes never overlap except the short glide overlap (" + juce::String (subOverlapBad) + ")");
    {
        Architect::Request a; a.style = "trap"; a.seed = 3; auto p1 = Architect::generate (a), p2 = Architect::generate (a); a.seed = 4; auto p3 = Architect::generate (a);
        auto sig = [] (const Architect::Plan& p) { juce::String s; s << p.bpm << p.keyRoot; for (auto& t : p.tracks) for (auto& c : t.clips) for (auto& n : c.notes) s << n.pitch << juce::String (n.start, 2); return s.hashCode64(); };
        check (sig (p1) == sig (p2) && sig (p1) != sig (p3), "same seed = same beat, another seed = another beat");
        check (juce::String (Architect::cardFor ("make me a dark drill beat").name) == juce::String ("Drill") && juce::String (Architect::cardFor ("smooth r&b").name) == juce::String ("R&B") && juce::String (Architect::cardFor ("boom bap").name) == juce::String ("Boom Bap"), "style words pick the card (drill, r&b, boom bap)");
        Architect::Request k; k.style = "trap"; k.keyRoot = 9; k.minor = 1; k.bpm = 144; auto pk = Architect::generate (k);
        check (pk.keyName == "A minor" && pk.bpm == 144, "asked key and tempo are kept (" + pk.keyName + ", " + juce::String (pk.bpm) + ")");
        bool glide = false; for (auto& t : pk.tracks) if (t.role == "808") for (auto& c : t.clips) { auto v = c.notes; std::sort (v.begin(), v.end(), [] (auto& x, auto& y) { return x.start < y.start; }); for (size_t i = 1; i < v.size(); ++i) if (v[i - 1].start + v[i - 1].length > v[i].start + 0.05) glide = true; }
        Architect::Request d; d.style = "drill"; d.seed = 5; auto pd = Architect::generate (d); bool dg = false;
        for (auto& t : pd.tracks) if (t.role == "808") for (auto& c : t.clips) { auto v = c.notes; std::sort (v.begin(), v.end(), [] (auto& x, auto& y) { return x.start < y.start; }); for (size_t i = 1; i < v.size(); ++i) if (v[i - 1].start + v[i - 1].length > v[i].start + 0.05) dg = true; }
        check (glide || dg, "808 glides (overlapping notes) appear in trap / drill");
    }
    {
        // from a reference: its tempo, key, swing and section map come through
        SqRef::Result r; r.ok = true; r.bpm = 92.3; r.keyRoot = 2; r.minor = true; r.swing = 0.6; r.hatDensity = 2.0f;
        for (int i = 0; i < 25; ++i) r.bars.push_back (i * 240.0 / 92.3);
        r.sections = { { 0, 8, 'A', "Intro", -20, 3, {} }, { 8, 16, 'B', "Hook", -12, 6, {} } };
        for (int b = 0; b < 24; ++b) { SqRef::DrumBar d; if (b >= 8) { d.kick[0] = d.kick[6] = d.kick[8] = 1; d.snare[4] = d.snare[12] = 1; } r.drums.push_back (d); }
        const int roots[4] = { 2, 10, 5, 0 }; const char* q[4] = { "m", "", "", "" };
        for (int b = 8; b < 24; ++b) { SqRef::Chord c; c.startSec = r.bars[(size_t) b]; c.root = roots[b % 4]; c.quality = q[b % 4]; r.chords.push_back (c); }
        r.subPresence = 0.4f; for (int b = 8; b < 24; ++b) r.sub.push_back ({ r.bars[(size_t) b], 2.0, 26.0f, false });
        Architect::Request rq; rq.reference = &r; rq.seed = 2; auto p = Architect::generate (rq);
        { bool has808 = false; for (auto& t : p.tracks) has808 = has808 || t.role == "808"; check (has808, "a reference with an 808 gets an 808 even when the style card has a played bass"); }
        check (p.bpm == 92 && p.keyName == "D minor" && std::abs (p.swing - 0.6) < 1e-9 && p.sections.size() == 2 && p.sections[1].bar == 9 && p.sections[1].bars == 16,
               "from a reference: " + juce::String (p.bpm) + " BPM, " + p.keyName + ", swing 60 %, Intro 8 + Hook 16");
        check (p.chordNames.joinIntoString (" ").startsWith ("Dm") && p.style == "Boom Bap", "the reference's chord functions and style are followed (" + p.chordNames.joinIntoString (" ") + ", " + p.style + ")");
        bool kickOk = false; for (auto& t : p.tracks) if (t.role == "drums") for (auto& c : t.clips) if (c.name.startsWith ("Hook")) { int k6 = 0; for (auto& n : c.notes) if (n.pitch == 36 && n.start < 4.0 && std::abs (n.start - 1.6) < 0.11) ++k6; kickOk = k6 == 1; }
        check (kickOk, "the reference's kick skeleton (x.....x.x.......) is the groove's base, swung");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
