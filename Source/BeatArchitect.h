#pragma once
// Squelchmoth 1.15 "Beat Intelligence" - A1: the Beat Architect (te-free, tested by tests/architect_test.cpp).
//
// Style cards as data, and a generator that writes a whole arranged beat from a card (or from a Reference Listener
// blueprint): sections, drums per bar with velocity motion / ghost notes / rolls / fills, an 808 line that follows the
// kick and the chord roots with glides, voice-led chords, a motif-based melody and a counter-melody. Every part is
// written new from rules and a seed; nothing is copied from a reference except its measured feel.

#include "SqReferenceDsp.h"
#include <random>

namespace Architect
{
    //==========================================================================
    struct ChordDeg { int semis; bool minor; };                     // semitones above the tonic, triad quality
    struct SectionSpec { const char* name; int bars; };

    struct StyleCard
    {
        const char* name; const char* aliases;                      // aliases: lower-case words that pick this card
        int bpmLo, bpmHi; bool halfTime; double swing;              // swing: off-16th position in its 8th (0.5 straight)
        const char* kit;
        std::vector<const char*> kicks;                             // 16-step rows; one is picked as A, another as the variation
        const char* snare; bool clapLayer; const char* rim;         // backbeat row, clap on top of it, ghost / rim row
        int hatRate; bool tripletHats; double rollProb; const char* openHat; const char* perc;
        bool sub; bool longSub; double glideProb, octaveProb;
        bool minorKey; std::vector<std::vector<ChordDeg>> progressions;
        bool sevenths; bool ninths; int chordRhythm;                // 0 sustain, 1 stabs, 2 arpeggio
        const char* chordPreset; const char* padPreset; const char* leadPreset; const char* counterPreset;
        double melodyDensity;                                       // notes per beat in the hook motif
        std::vector<SectionSpec> form;
        double lufs;
    };

    inline const std::vector<StyleCard>& cards()
    {
        static const std::vector<ChordDeg> i_VI_III_VII { { 0, true }, { 8, false }, { 3, false }, { 10, false } },
                                           i_iv_VI_v    { { 0, true }, { 5, true }, { 8, false }, { 7, true } },
                                           i_VII_VI_VII { { 0, true }, { 10, false }, { 8, false }, { 10, false } },
                                           i_VI_iv_V    { { 0, true }, { 8, false }, { 5, true }, { 7, false } },
                                           i_i_VI_VII   { { 0, true }, { 0, true }, { 8, false }, { 10, false } },
                                           iv_v_i_i     { { 5, true }, { 7, true }, { 0, true }, { 0, true } },
                                           I_V_vi_IV    { { 0, false }, { 7, false }, { 9, true }, { 5, false } },
                                           ii_V_I_vi    { { 2, true }, { 7, false }, { 0, false }, { 9, true } },
                                           IV_V_iii_vi  { { 5, false }, { 7, false }, { 4, true }, { 9, true } },
                                           I_vi_ii_V    { { 0, false }, { 9, true }, { 2, true }, { 7, false } },
                                           vi_IV_I_V    { { 9, true }, { 5, false }, { 0, false }, { 7, false } };
        static const std::vector<SectionSpec> rapForm { { "Intro", 8 }, { "Hook", 16 }, { "Verse", 16 }, { "Hook", 16 }, { "Verse", 16 }, { "Hook", 16 }, { "Outro", 8 } },
                                              rnbForm { { "Intro", 8 }, { "Verse", 16 }, { "Hook", 16 }, { "Verse", 16 }, { "Hook", 16 }, { "Bridge", 8 }, { "Hook", 16 }, { "Outro", 8 } },
                                              clubForm { { "Intro", 16 }, { "Verse", 16 }, { "Hook", 16 }, { "Bridge", 8 }, { "Hook", 16 }, { "Outro", 16 } };
        static const std::vector<StyleCard> c = {
            { "Modern Trap", "trap atlanta 808 dark hard", 130, 160, true, 0.5, "Neon Trap",
              { "x.....x...x.....", "x..x..x...x..x..", "x.....x..x....x.", "x...x.....x..x.." }, "........x.......", true, ".......g........",
              16, false, 0.6, "..............x.", "....x.......x...", true, true, 0.35, 0.25,
              true, { i_VI_III_VII, i_iv_VI_v, i_VII_VI_VII, i_i_VI_VII }, false, false, 0,
              "Dark Bell", "Dark Drone", "Trap Flute", "Plucked Guitar", 1.0, rapForm, -8.0 },
            { "Melodic Trap", "melodic emotional guitar sad pain", 130, 150, true, 0.5, "Chrome Trap",
              { "x.....x...x.....", "x......x..x.....", "x.....x......x.." }, "........x.......", true, "................",
              16, false, 0.4, "......x.........", "................", true, true, 0.45, 0.15,
              true, { i_VI_III_VII, i_VI_iv_V, i_iv_VI_v }, true, false, 2,
              "Plucked Guitar", "Warm Analog Pad Dark", "Eerie Keys", "Glass Bell", 0.75, rapForm, -9.0 },
            { "Drill", "drill uk ny brooklyn sliding", 138, 145, true, 0.5, "Drill Smoke",
              { "x.....x...x.....", "x..x......x..x..", "x.....x....x...." }, "........x.....x.", false, "...g......g.....",
              8, true, 0.3, "....x...........", "..x......x......", true, true, 0.7, 0.3,
              true, { i_VI_III_VII, i_iv_VI_v, iv_v_i_i }, false, false, 0,
              "Drill Piano Dark", "Drill Strings Low", "Drill Bell", "Drill Vox", 0.75, rapForm, -8.0 },
            { "Boom Bap", "boom bap 90s east coast golden sample dusty", 84, 96, false, 0.58, "Boom Bap Crates",
              { "x.......x.x.....", "x......x..x.....", "x.x.....x.x.....", "x.......x..x...." }, "....x.......x...", false, ".......g.g......",
              8, false, 0.0, "..............x.", "................", false, false, 0.0, 0.0,
              true, { ii_V_I_vi, i_iv_VI_v, i_VI_iv_V }, true, false, 1,
              "Electric Piano", "Strings Dark", "Marimba Vibes", "Horn Section", 0.75, rapForm, -9.5 },
            { "Lo-Fi", "lofi lo-fi chill study tape", 70, 90, false, 0.62, "Velvet Lo-Fi",
              { "x.......x.......", "x.....x.x.......", "x.......x.x....." }, "....x.......x...", false, "..g.......g.....",
              8, false, 0.0, "................", "................", false, false, 0.0, 0.0,
              false, { ii_V_I_vi, I_vi_ii_V, IV_V_iii_vi }, true, true, 0,
              "Electric Piano Tremolo", "Warm Analog Pad", "Soft Keys", "Harp Nylon", 0.5, { { "Intro", 4 }, { "Hook", 16 }, { "Verse", 16 }, { "Hook", 16 }, { "Outro", 4 } }, -13.0 },
            { "West Coast", "west coast g-funk gfunk la bounce", 88, 100, false, 0.54, "Mothwing 808",
              { "x..x....x.x.....", "x..x..x.x.......", "x.......x.x..x.." }, "....x.......x...", true, "................",
              8, false, 0.0, "......x.......x.", "..x...x...x...x.", false, false, 0.0, 0.0,
              true, { i_VII_VI_VII, i_iv_VI_v }, true, false, 1,
              "Funk Keys", "Synth Strings 80s", "Lead Synth", "Synth Brass Soft", 0.75, rapForm, -9.0 },
            { "R&B", "rnb r&b contemporary slow smooth soul", 60, 80, false, 0.56, "R&B Velvet",
              { "x.........x.....", "x......x..x.....", "x.....x...x....." }, "....x.......x...", false, ".......g.......g",
              16, false, 0.15, "..............x.", "................", true, true, 0.3, 0.1,
              false, { ii_V_I_vi, IV_V_iii_vi, vi_IV_I_V, I_vi_ii_V }, true, true, 0,
              "Electric Piano Bell", "Warm Analog Pad Wide", "Glass Bell Long", "Vox Pad", 0.5, rnbForm, -10.0 },
            { "Neo-Soul", "neo soul neosoul live gospel jazzy", 70, 95, false, 0.62, "Neo-Soul Dust",
              { "x.......x.x.....", "x..x....x.......", "x.......x..x...." }, "....x.......x...", false, "..g....g..g.....",
              8, false, 0.0, "..............x.", "................", false, false, 0.0, 0.0,
              false, { ii_V_I_vi, IV_V_iii_vi, I_vi_ii_V }, true, true, 1,
              "Electric Piano Phased", "Organ Rotary", "Plucked Guitar Muted", "Horn Section", 0.5, rnbForm, -12.0 },
            { "Afrobeats", "afrobeats afro afroswing naija", 100, 115, false, 0.55, "Afro Sun",
              { "x..x..x...x.....", "x.....x..x..x...", "x..x......x..x.." }, "....x.......x...", false, "..x..x....x..x..",
              16, false, 0.0, "......x.......x.", "x..x..x.x..x..x.", false, false, 0.0, 0.0,
              false, { I_V_vi_IV, vi_IV_I_V, IV_V_iii_vi }, false, false, 1,
              "Marimba Kalimba", "Warm Analog Pad Bright", "Plucked Guitar", "Synth Vox", 1.0, clubForm, -9.0 },
            { "Amapiano", "amapiano piano log south africa", 110, 115, false, 0.5, "Amapiano Log",
              { "x.......x.......", "x.....x.x.......", "x.......x.....x." }, "....x.......x...", false, "..x...x...x...x.",
              16, false, 0.0, "................", "x.x.x.x.x.x.x.x.", true, false, 0.0, 0.4,
              true, { i_iv_VI_v, ii_V_I_vi, i_VII_VI_VII }, true, true, 1,
              "Electric Piano", "Choir Ooh", "Vocal Keys", "Whisper Pad", 0.5, clubForm, -10.0 },
            { "Reggaeton", "reggaeton dembow latin perreo", 88, 100, false, 0.5, "Dembow Heat",
              { "x...x...x...x...", "x...x...x...x.x.", "x...x..xx...x..." }, "...x..x....x..x.", false, "................",
              8, false, 0.0, "..............x.", "................", true, false, 0.0, 0.0,
              true, { i_VI_III_VII, i_iv_VI_v, i_VI_iv_V }, false, false, 1,
              "Tight Pluck", "Strings", "Lead Synth", "Marimba", 0.75, clubForm, -8.0 },
            { "Jersey Club", "jersey club bounce", 130, 145, false, 0.5, "Jersey Bounce",
              { "x..x..x...x..x..", "x..x..x...x.x...", "x..x..x..x...x.." }, "....x.......x...", true, "................",
              8, false, 0.0, "..x...x...x...x.", "................", false, false, 0.0, 0.0,
              true, { i_VI_III_VII, i_VII_VI_VII }, false, false, 1,
              "Organ Stab", "Glass Pad", "Table Pluck Vocal", "Glass Bell Short", 0.75, clubForm, -8.0 },
            { "House", "house dance four floor club deep", 120, 128, false, 0.5, "House Silk",
              { "x...x...x...x...", "x...x...x...x..x", "x...x...x..xx..." }, "....x.......x...", false, "................",
              16, false, 0.0, "..x...x...x...x.", "................", false, false, 0.0, 0.0,
              true, { i_VI_III_VII, ii_V_I_vi, i_iv_VI_v }, true, false, 1,
              "Organ Stab", "Warm Analog Pad Wide", "Future Pluck", "Table Pluck Vocal", 0.75, clubForm, -8.0 },
            { "Plugg", "plugg pluggnb rage pillow", 140, 162, true, 0.5, "Plugg Pillow",
              { "x.....x...x.....", "x..x......x.....", "x.....x.....x..." }, "........x.......", true, "................",
              8, false, 0.35, "......x.........", "................", true, false, 0.2, 0.3,
              false, { I_V_vi_IV, IV_V_iii_vi, vi_IV_I_V }, true, false, 0,
              "Glass Bell", "Glass Pad Shimmer", "Music Box", "Table Pluck Glass", 0.75, rapForm, -8.0 } };
        return c;
    }

    inline const StyleCard& cardFor (const juce::String& want)
    {
        const auto w = want.toLowerCase().trim();
        auto& cs = cards();
        for (auto& c : cs) if (w == juce::String (c.name).toLowerCase()) return c;
        int best = 0, bi = 0;
        for (int i = 0; i < (int) cs.size(); ++i)
        {
            int score = 0;
            juce::StringArray words; words.addTokens (juce::String (cs[(size_t) i].name).toLowerCase() + " " + cs[(size_t) i].aliases, " ", "");
            for (auto& x : words) if (x.length() > 2 && w.contains (x)) score += x.length();
            if (score > best) { best = score; bi = i; }
        }
        return cs[(size_t) bi];
    }

    //==========================================================================
    struct Note { int pitch; double start, length; int velocity; };          // beats from the clip start
    struct Clip { int startBar = 1, bars = 4; juce::String name; std::vector<Note> notes; };
    struct Track { juce::String name, role, instrument, preset; double volumeDb = 0, pan = 0; std::vector<Clip> clips; };
    struct Marker { juce::String name; int bar; int bars; };
    struct Plan
    {
        juce::String style, keyName; double bpm = 120, swing = 0.5; int keyRoot = 0; bool minor = true; int totalBars = 0;
        juce::String kit; std::vector<Marker> sections; std::vector<Track> tracks; juce::StringArray chordNames; double lufs = -9;
        juce::uint32 seed = 0; bool fromReference = false;
    };

    struct Request
    {
        juce::String style; double bpm = 0; int keyRoot = -1; int minor = -1; juce::uint32 seed = 1; int bars = 0;
        const SqRef::Result* reference = nullptr;
    };

    namespace detail
    {
        enum { nKick = 36, nRim = 37, nSnare = 38, nClap = 39, nHat = 42, nOpen = 46, nCrash = 49, nPerc = 56 };

        struct Rng
        {
            std::mt19937 g; explicit Rng (juce::uint32 s) : g (s) {}
            double uni() { return std::uniform_real_distribution<double> (0.0, 1.0) (g); }
            int range (int lo, int hi) { return lo + (int) (uni() * (hi - lo + 1 - 1e-9)); }
            bool chance (double p) { return uni() < p; }
        };

        inline bool on (const juce::String& row, int step) { return step < row.length() && (row[step] == 'x' || row[step] == 'X'); }

        /** Position of 16th step i in beats from the bar start, with swing on the off-16ths. */
        inline double stepBeat (int i, double swing) { return i % 2 == 1 ? (i - 1) * 0.25 + swing * 0.5 : i * 0.25; }

        inline int clampVel (int v) { return juce::jlimit (1, 127, v); }

        /** Scale tones (semitones above the tonic). */
        inline std::vector<int> scale (bool minor) { return minor ? std::vector<int> { 0, 2, 3, 5, 7, 8, 10 } : std::vector<int> { 0, 2, 4, 5, 7, 9, 11 }; }
        inline std::vector<int> penta (bool minor) { return minor ? std::vector<int> { 0, 3, 5, 7, 10 } : std::vector<int> { 0, 2, 4, 7, 9 }; }

        struct Chord { int rootPc; bool minor; std::vector<int> tones; juce::String name; };   // tones: pitch classes

        inline Chord makeChord (int keyRoot, bool keyMinor, ChordDeg d, bool sevenths, bool ninths)
        {
            Chord c; c.rootPc = (keyRoot + d.semis) % 12; c.minor = d.minor;
            c.tones = { c.rootPc, (c.rootPc + (d.minor ? 3 : 4)) % 12, (c.rootPc + 7) % 12 };
            const auto sc = scale (keyMinor);
            auto inKey = [&] (int pc) { for (int s : sc) if ((keyRoot + s) % 12 == pc) return true; return false; };
            juce::String ext;
            if (sevenths)
            {
                const int maj7 = (c.rootPc + 11) % 12, min7 = (c.rootPc + 10) % 12;
                if (inKey (maj7) && ! d.minor) { c.tones.push_back (maj7); ext = "maj7"; }
                else { c.tones.push_back (min7); ext = "7"; }
            }
            if (ninths && inKey ((c.rootPc + 2) % 12)) { c.tones.push_back ((c.rootPc + 2) % 12); ext = ext == "maj7" ? "maj9" : ext == "7" ? "9" : "add9"; }
            c.name = juce::String (SqRef::noteNames[c.rootPc]) + (d.minor ? "m" : "") + ext;
            return c;
        }

        /** A voicing near `centre`: each chord tone placed at the octave closest to the previous voicing's centre. */
        inline std::vector<int> voice (const Chord& c, double centre)
        {
            std::vector<int> v;
            for (int pc : c.tones)
            {
                int p = pc + 12 * (int) std::round ((centre - pc) / 12.0);
                while (p < 52) p += 12; while (p > 76) p -= 12;
                v.push_back (p);
            }
            std::sort (v.begin(), v.end()); v.erase (std::unique (v.begin(), v.end()), v.end());
            return v;
        }
    }

    //==========================================================================
    inline Plan generate (const Request& rq)
    {
        using namespace detail;
        const SqRef::Result* ref = rq.reference != nullptr && rq.reference->ok ? rq.reference : nullptr;
        // the card: asked for, or read from the reference's measurements
        juce::String styleWord = rq.style;
        if (styleWord.isEmpty() && ref != nullptr)
        {
            int rolls = 0; for (auto& d : ref->drums) rolls += d.hatRolls;
            const double b = ref->bpm;
            styleWord = (b >= 136 && b <= 148 && ref->subGlides > 3 && rolls < 4) ? "drill" : (b >= 125 && (rolls > 2 || ref->subGlides > 1)) ? "trap"
                      : (b >= 118 && b <= 130 && ref->pumpDb > 2) ? "house" : (b >= 100 && b < 118) ? "afrobeats" : (b >= 84 && b < 100) ? (ref->swing > 0.55 ? "boom bap" : "west coast")
                      : (b < 84) ? (ref->subPresence > 0.3f ? "r&b" : "lo-fi") : "trap";
        }
        const StyleCard& card = cardFor (styleWord.isEmpty() ? juce::String ("trap") : styleWord);
        Rng rng (rq.seed * 2654435761u + 12345u);
        Plan p; p.style = card.name; p.seed = rq.seed; p.kit = card.kit; p.lufs = card.lufs; p.fromReference = ref != nullptr;
        p.bpm = rq.bpm > 0 ? rq.bpm : ref != nullptr ? std::round (ref->bpm) : (double) rng.range (card.bpmLo, card.bpmHi);
        p.swing = ref != nullptr ? juce::jlimit (0.5, 0.68, ref->swing < 0.53 ? 0.5 : ref->swing) : card.swing;
        p.minor = rq.minor >= 0 ? rq.minor != 0 : ref != nullptr ? ref->minor : card.minorKey;
        p.keyRoot = rq.keyRoot >= 0 ? rq.keyRoot % 12 : ref != nullptr ? ref->keyRoot : rng.range (0, 11);
        p.keyName = juce::String (SqRef::noteNames[p.keyRoot]) + (p.minor ? " minor" : " major");

        // ---- form: the reference's sections, or the card's (trimmed to `bars` when asked)
        if (ref != nullptr && ! ref->sections.empty())
            for (auto& s : ref->sections) p.sections.push_back ({ s.name.isEmpty() ? juce::String ("Part") : s.name, s.startBar + 1, juce::jmax (1, s.bars) });
        else
        {
            int bar = 1;
            for (auto& s : card.form) { if (rq.bars > 0 && bar - 1 + s.bars > rq.bars) break; p.sections.push_back ({ s.name, bar, s.bars }); bar += s.bars; }
            if (p.sections.empty()) p.sections.push_back ({ "Hook", 1, juce::jmax (4, rq.bars) });
        }
        p.totalBars = p.sections.back().bar - 1 + p.sections.back().bars;

        // ---- harmony: one progression for the song (the reference's chord functions when there is one), a second
        //      for a bridge
        std::vector<ChordDeg> prog = card.progressions[(size_t) rng.range (0, (int) card.progressions.size() - 1)];
        if (ref != nullptr)
        {
            // the most common section's chord roots, as scale-degree functions in the reference key
            std::vector<ChordDeg> fromRef;
            const SqRef::Section* hook = nullptr; for (auto& s : ref->sections) if (s.name == "Hook" && hook == nullptr) hook = &s;
            if (hook == nullptr && ! ref->sections.empty()) hook = &ref->sections[ref->sections.size() / 2];
            if (hook != nullptr)
                for (auto& c : ref->chords)
                {
                    int bar = 0; for (size_t i = 0; i < ref->bars.size(); ++i) if (ref->bars[i] <= c.startSec + 0.06) bar = (int) i;
                    if (bar < hook->startBar || bar >= hook->startBar + hook->bars || c.root < 0) continue;
                    fromRef.push_back ({ ((c.root - ref->keyRoot) % 12 + 12) % 12, c.quality.startsWith ("m") && ! c.quality.startsWith ("maj") });
                    if (fromRef.size() >= 4) break;
                }
            if (fromRef.size() >= 2) { while (fromRef.size() < 4) fromRef.push_back (fromRef[fromRef.size() % 2]); prog = fromRef; }
        }
        std::vector<ChordDeg> bridgeProg = card.progressions[(size_t) rng.range (0, (int) card.progressions.size() - 1)];
        std::vector<Chord> chords, bridgeChords;
        for (auto d : prog) chords.push_back (makeChord (p.keyRoot, p.minor, d, card.sevenths, card.ninths));
        for (auto d : bridgeProg) bridgeChords.push_back (makeChord (p.keyRoot, p.minor, d, card.sevenths, card.ninths));
        for (auto& c : chords) p.chordNames.add (c.name);

        // ---- drum grammar: pattern A and a variation; the reference's own typical rows as the skeleton when given
        juce::String kickA = card.kicks[(size_t) rng.range (0, (int) card.kicks.size() - 1)], kickB = kickA;
        for (int tries = 0; tries < 8 && kickB == kickA; ++tries) kickB = card.kicks[(size_t) rng.range (0, (int) card.kicks.size() - 1)];
        juce::String snareRow = card.snare; int hatRate = card.hatRate; double rollProb = card.rollProb, glideProb = card.glideProb;
        if (ref != nullptr)
        {
            const SqRef::Section* big = nullptr; for (auto& s : ref->sections) if (big == nullptr || s.elements > big->elements) big = &s;
            if (big != nullptr)
            {
                const auto k = SqRef::typicalRow (ref->drums, big->startBar, big->startBar + big->bars, 0), s = SqRef::typicalRow (ref->drums, big->startBar, big->startBar + big->bars, 1);
                if (k.containsChar ('x')) kickA = k;
                if (s.containsChar ('x')) snareRow = s;
            }
            hatRate = ref->hatDensity > 3.0f ? 16 : 8;
            int rolls = 0; for (auto& d : ref->drums) rolls += d.hatRolls;
            rollProb = rolls > 0 ? juce::jlimit (0.25, 0.8, rolls / (double) juce::jmax (1, (int) ref->drums.size() / 4)) : 0.0;
            glideProb = ref->subGlides > 0 ? juce::jlimit (0.2, 0.7, ref->subGlides / (double) juce::jmax (1, (int) ref->drums.size() / 4)) : 0.0;
        }

        // ---- the melody motif: 2 bars on the pentatonic, rhythm from the density, landing on chord tones
        const auto pen = penta (p.minor);
        struct MNote { double start, len; int degree; };   // degree: index into an extended pentatonic ladder
        std::vector<MNote> motif;
        {
            const double grid = card.melodyDensity >= 1.0 ? 0.5 : 1.0; int deg = rng.range (5, 7); double t = 0;
            while (t < 8.0 - 1e-6)
            {
                const bool rest = rng.chance (card.melodyDensity >= 1.0 ? 0.22 : 0.3) && t > 0;
                double len = grid * (rng.chance (0.3) ? 2 : 1); if (t + len > 8.0) len = 8.0 - t;
                if (! rest) { deg = juce::jlimit (3, 10, deg + rng.range (-2, 2)); motif.push_back ({ t, len, deg }); }
                t += len;
            }
            if (motif.empty()) motif.push_back ({ 0, 2, 5 });
        }
        auto ladder = [&] (int degree, int base) { const int n = (int) pen.size(); return base + p.keyRoot + 12 * (degree / n) + pen[(size_t) (degree % n)]; };

        // the low end: an 808 where the style (or the reference) has one, a played bass line everywhere else
        bool useSub = card.sub, longSub = card.longSub;
        if (ref != nullptr)
        {
            double len = 0; for (auto& n : ref->sub) len += n.lengthSec;
            useSub = ref->subPresence > 0.2f || ref->sub.size() >= 8;
            if (useSub && ! ref->sub.empty()) longSub = len / (double) ref->sub.size() > 0.75 * 60.0 / juce::jmax (40.0, ref->bpm);
        }
        Track bass { "Bass", "bass", "Pollen", "Pluck Bass Soft", -6.0, 0.0, {} };
        Track drums { "Drums", "drums", "kit", card.kit, -3.0, 0.0, {} };
        Track sub { "808", "808", "Moth 808", {}, -4.0, 0.0, {} };
        Track keys { "Chords", "chords", "Pollen", card.chordPreset, -9.0, -0.1, {} };
        Track pad { "Pad", "pad", "Pollen", card.padPreset, -14.0, 0.15, {} };
        Track lead { "Melody", "melody", "Pollen", card.leadPreset, -10.0, 0.1, {} };
        Track counter { "Counter", "counter", "Pollen", card.counterPreset, -13.0, -0.25, {} };

        double voiceCentre = 62.0;
        for (size_t si = 0; si < p.sections.size(); ++si)
        {
            const auto& sec = p.sections[si];
            const juce::String nm = sec.name;
            const bool intro = nm == "Intro", hook = nm == "Hook", verse = nm == "Verse", bridge = nm == "Bridge", outro = nm == "Outro";
            const bool nextIsHook = si + 1 < p.sections.size() && p.sections[si + 1].name == "Hook";
            const auto& ch = bridge ? bridgeChords : chords;
            const bool hasDrums = ! intro || sec.bars > 8, fullDrums = hook || verse || (! intro && ! outro && ! bridge);
            Clip dc { sec.bar, sec.bars, nm + " drums", {} }, sc { sec.bar, sec.bars, nm + " 808", {} }, kc { sec.bar, sec.bars, nm + " chords", {} },
                 bc { sec.bar, sec.bars, nm + " bass", {} }, pc { sec.bar, sec.bars, nm + " pad", {} }, lc { sec.bar, sec.bars, nm + " melody", {} }, cc { sec.bar, sec.bars, nm + " counter", {} };

            for (int b = 0; b < sec.bars; ++b)
            {
                const double b0 = b * 4.0;
                const bool lastBar = b == sec.bars - 1, phraseEnd = b % 4 == 3, eighth = b % 8 == 7;
                const Chord& chord = ch[(size_t) (b % (int) ch.size())];
                const juce::String kick = phraseEnd ? kickB : kickA;
                const bool dropKick = lastBar && nextIsHook;                 // silence before the hook hits
                const bool halfDrums = (intro && b >= sec.bars / 2) || (outro && b < sec.bars / 2) || bridge;

                // ---------------- drums
                if (hasDrums && (fullDrums || halfDrums))
                {
                    if (b == 0 && (hook || verse)) dc.notes.push_back ({ nCrash, b0, 1.0, hook ? 110 : 92 });
                    for (int i = 0; i < 16; ++i)
                    {
                        const double t = b0 + stepBeat (i, p.swing);
                        const bool fillZone = lastBar && i >= 12 && ! bridge;
                        if (fullDrums && on (kick, i) && ! (dropKick && i >= 8)) dc.notes.push_back ({ nKick, t, 0.25, clampVel ((i == 0 ? 118 : 104) + rng.range (-5, 5)) });
                        if (fullDrums && on (snareRow, i) && ! fillZone)
                        {
                            dc.notes.push_back ({ nSnare, t, 0.25, clampVel (112 + rng.range (-4, 4)) });
                            if (card.clapLayer || hook) dc.notes.push_back ({ nClap, t + 0.012, 0.25, clampVel (98 + rng.range (-5, 5)) });
                        }
                        if (fullDrums && i < (int) juce::String (card.rim).length() && card.rim[i] == 'g' && rng.chance (0.7)) dc.notes.push_back ({ nSnare, t, 0.2, clampVel (38 + rng.range (-8, 8)) });
                        if (fullDrums && i < (int) juce::String (card.rim).length() && card.rim[i] == 'x' && (hook || rng.chance (0.6))) dc.notes.push_back ({ nRim, t, 0.2, clampVel (80 + rng.range (-8, 8)) });
                        if ((hook || (verse && b >= sec.bars / 2)) && on (card.perc, i)) dc.notes.push_back ({ nPerc, t, 0.2, clampVel (74 + rng.range (-10, 10)) });
                        if ((hook || verse) && on (card.openHat, i) && (hook || phraseEnd)) dc.notes.push_back ({ nOpen, t, 0.5, clampVel (88 + rng.range (-6, 6)) });
                    }
                    // hats: the rate's grid with accent motion; triplet-feel grouping (3 + 3 + 2) for drill; rolls on phrase ends
                    const bool rollHere = phraseEnd && ! lastBar && rng.chance (rollProb) && fullDrums;
                    const int rollBeat = 3;
                    if (card.tripletHats)
                    {
                        static const int pat[] = { 0, 3, 6, 8, 11, 14 };
                        for (int i : pat) if (! (rollHere && i >= rollBeat * 4)) dc.notes.push_back ({ nHat, b0 + i * 0.25, 0.2, clampVel ((i % 8 == 0 ? 104 : 84) + rng.range (-6, 6)) });
                    }
                    else
                        for (int i = 0; i < 16; i += hatRate == 16 ? 1 : 2)
                        {
                            if (rollHere && i >= rollBeat * 4) continue;
                            if (halfDrums && ! fullDrums && i % 4 == 2 && hatRate == 16) continue;
                            const int v = i % 4 == 0 ? 104 : i % 2 == 0 ? 88 : 70;
                            dc.notes.push_back ({ nHat, b0 + stepBeat (i, p.swing), 0.2, clampVel (v + rng.range (-7, 7)) });
                        }
                    if (rollHere)
                    {
                        const bool trip = rng.chance (0.35); const int n = trip ? 6 : 8; const bool up = rng.chance (0.5);
                        for (int k = 0; k < n; ++k) dc.notes.push_back ({ nHat, b0 + rollBeat + k * (1.0 / n), 0.1, clampVel (up ? 62 + k * 6 : 108 - k * 6) });
                    }
                    // fill into the next section: snare 16ths rising (toms would need the kit's toms tuned; the snare is safe)
                    if (lastBar && fullDrums && ! bridge && si + 1 < p.sections.size())
                        for (int i = 12; i < 16; ++i) if (i != 12 || rng.chance (0.6)) dc.notes.push_back ({ nSnare, b0 + i * 0.25, 0.2, clampVel (70 + (i - 12) * 13) });
                    juce::ignoreUnused (eighth);
                }

                // ---------------- 808: on the kick hits, the chord root in octave 1, each note held to the next hit
                if (! useSub && fullDrums)
                {
                    // a played bass: roots in octave 2 on the kick hits, an approach note into the next chord on phrase ends
                    const int root = 36 + chord.rootPc;
                    std::vector<int> hits; for (int i = 0; i < 16; ++i) if (on (kick, i) && ! (dropKick && i >= 8)) hits.push_back (i);
                    for (size_t h = 0; h < hits.size(); ++h)
                    {
                        const double t = b0 + stepBeat (hits[h], p.swing);
                        const double nextT = h + 1 < hits.size() ? b0 + stepBeat (hits[h + 1], p.swing) : b0 + 4.0;
                        bc.notes.push_back ({ root + (h > 0 && rng.chance (0.15) ? 12 : 0), t, juce::jlimit (0.2, 1.0, nextT - t - 0.05), clampVel ((h == 0 ? 106 : 94) + rng.range (-6, 6)) });
                    }
                    if (phraseEnd && ! dropKick)
                    {
                        const Chord& nx = ch[(size_t) ((b + 1) % (int) ch.size())];
                        int target = 36 + nx.rootPc, approach = target + (rng.chance (0.5) ? -2 : 7);
                        if (approach > 50) approach -= 12;
                        bc.notes.push_back ({ approach, b0 + 3.5, 0.45, 88 });
                    }
                }
                if (useSub && fullDrums)
                {
                    int rootMidi = 24 + chord.rootPc; if (rootMidi < 29) rootMidi += 12;         // F1 .. E2
                    std::vector<int> hits; for (int i = 0; i < 16; ++i) if (on (kick, i) && ! (dropKick && i >= 8)) hits.push_back (i);
                    for (size_t h = 0; h < hits.size(); ++h)
                    {
                        const double t = b0 + stepBeat (hits[h], p.swing);
                        const double nextT = h + 1 < hits.size() ? b0 + stepBeat (hits[h + 1], p.swing) : b0 + 4.0;
                        double len = longSub ? nextT - t - 0.03 : juce::jmin (0.75, nextT - t - 0.03);
                        const bool lastHit = h + 1 == hits.size();
                        int pitch = rootMidi;
                        if (! lastHit && h > 0 && rng.chance (card.octaveProb)) pitch += 12;
                        sc.notes.push_back ({ pitch, t, juce::jmax (0.2, len), clampVel (112 + rng.range (-4, 4)) });
                        // a glide on the phrase's last note: an overlapping note (Moth 808 glides on overlaps) up a fifth / an
                        // octave, or into the next chord's root
                        if (lastHit && phraseEnd && longSub && len > 0.9 && rng.chance (glideProb))
                        {
                            const Chord& nx = ch[(size_t) ((b + 1) % (int) ch.size())];
                            int target = 24 + nx.rootPc; if (target < 29) target += 12;
                            if (target == pitch) target = pitch + (rng.chance (0.5) ? 12 : 7);
                            const double gs = t + len * 0.55;
                            sc.notes.back().length = gs - t + 0.12;                                // overlap = glide
                            sc.notes.push_back ({ target, gs, t + len - gs, 106 });
                        }
                    }
                }

                // ---------------- chords: voice-led, the card's rhythm; thinner in verses, pad under hooks and intros
                {
                    auto v = voice (chord, voiceCentre);
                    double sum = 0; for (int n : v) sum += n; voiceCentre = 0.7 * voiceCentre + 0.3 * (sum / (double) v.size());
                    const int rhythm = (intro || outro || bridge) ? 0 : card.chordRhythm;
                    const int velBase = hook ? 86 : 74;
                    if (rhythm == 0) for (int n : v) kc.notes.push_back ({ n, b0, 3.9, clampVel (velBase + rng.range (-5, 5)) });
                    else if (rhythm == 1)
                    {
                        static const double hitsA[] = { 0.0, 1.5, 2.5 }, hitsB[] = { 0.0, 0.75, 2.0, 3.5 };
                        const bool alt = phraseEnd;
                        for (double h : (alt ? std::vector<double> (hitsB, hitsB + 4) : std::vector<double> (hitsA, hitsA + 3)))
                            for (int n : v) kc.notes.push_back ({ n, b0 + h, h == 0.0 ? 1.2 : 0.7, clampVel (velBase + (h == 0.0 ? 6 : -4) + rng.range (-5, 5)) });
                    }
                    else
                    {
                        std::vector<int> arp (v); arp.push_back (v.front() + 12);
                        for (int k = 0; k < 8; ++k)
                        {
                            const int idx = phraseEnd ? (int) arp.size() - 1 - (k % (int) arp.size()) : k % (int) arp.size();
                            kc.notes.push_back ({ arp[(size_t) idx], b0 + k * 0.5, 0.9, clampVel (velBase - 4 + (k % 4 == 0 ? 10 : 0) + rng.range (-5, 5)) });
                        }
                    }
                    if (hook || intro || bridge || outro)
                    {
                        pc.notes.push_back ({ 36 + chord.rootPc + (chord.rootPc < 5 ? 12 : 0), b0, 3.95, 62 });
                        for (size_t k = 0; k < v.size() && k < 3; ++k) pc.notes.push_back ({ v[k] + (v[k] < 60 ? 12 : 0), b0, 3.95, 58 });
                    }
                }

                // ---------------- melody: the motif (2 bars) A A' A B; hooks carry it, an octave up the last time; verses get
                //                  a sparse counter line in the motif's gaps; intro and outro quote its first bar
                {
                    const int mb = b % 2; const int phrase = (b / 2) % 4;                // which motif bar, which repeat
                    const bool melodyHere = hook || (intro && b >= sec.bars / 2) || outro || bridge;
                    const int lift = hook && si + 1 >= p.sections.size() - 1 ? 12 : 0;
                    auto place = [&] (Clip& into, int base, int velBase, bool sparse)
                    {
                        for (auto& m : motif)
                        {
                            if (m.start < mb * 4.0 || m.start >= (mb + 1) * 4.0) continue;
                            if (sparse && (std::fmod (m.start, 2.0) > 0.01)) continue;
                            int deg = m.degree;
                            if (phrase == 1 && m.start >= 6.0) deg += 1;                    // A': a new ending
                            if (phrase == 3) deg = juce::jlimit (3, 10, m.degree + (mb == 1 ? -2 : 1));   // B: answer phrase
                            int pitch = ladder (deg, base);
                            // strong beats land on a chord tone (nearest one)
                            if (std::fmod (m.start, 2.0) < 0.01)
                            {
                                int best = pitch, bd = 99;
                                for (int tpc : chord.tones) for (int o = -1; o <= 1; ++o) { const int cand = tpc + 12 * ((pitch / 12) + o); if (std::abs (cand - pitch) < bd) { bd = std::abs (cand - pitch); best = cand; } }
                                if (bd <= 2) pitch = best;
                            }
                            into.notes.push_back ({ pitch + lift, b0 + (m.start - mb * 4.0), juce::jmax (0.2, m.len * 0.92), clampVel (velBase + rng.range (-7, 7)) });
                        }
                    };
                    if (melodyHere) place (lc, 60, hook ? 98 : 84, intro || outro);
                    if (verse && b >= 4) place (cc, 48, 76, true);
                    if (hook && phrase == 3) place (cc, 72, 70, true);                      // a high answer at the end of each 8
                }
            }
            auto keep = [] (Track& t, Clip& c) { if (! c.notes.empty()) t.clips.push_back (std::move (c)); };
            keep (drums, dc); keep (sub, sc); keep (bass, bc); keep (keys, kc); keep (pad, pc); keep (lead, lc); keep (counter, cc);
        }
        for (auto* t : { &drums, &sub, &bass, &keys, &pad, &lead, &counter }) if (! t->clips.empty()) p.tracks.push_back (std::move (*t));
        return p;
    }

    //==========================================================================
    inline juce::String describe (const Plan& p)
    {
        juce::String s;
        s << "BEAT PLAN - " << p.style << (p.fromReference ? " (from the reference's feel)" : "") << ": " << juce::String (p.bpm, 0) << " BPM, " << p.keyName
          << ", swing " << juce::roundToInt (p.swing * 100) << " %, " << p.totalBars << " bars, kit " << p.kit << ", seed " << (int) p.seed << ".\n";
        s << "Chords: " << p.chordNames.joinIntoString (" - ") << ".\nSections:";
        for (auto& m : p.sections) s << " " << m.name << " " << m.bars << " (bar " << m.bar << "),";
        s = s.trimCharactersAtEnd (",") + ".\nTracks:";
        for (auto& t : p.tracks) { int n = 0; for (auto& c : t.clips) n += (int) c.notes.size(); s << "\n  " << t.name << " [" << (t.preset.isNotEmpty() ? t.preset : t.instrument) << "]: " << (int) t.clips.size() << " clips, " << n << " notes"; }
        return s;
    }

    inline juce::var cardJson (const StyleCard& c)
    {
        auto o = new juce::DynamicObject(); juce::var v (o);
        o->setProperty ("name", c.name); o->setProperty ("words", c.aliases); o->setProperty ("bpm", juce::String (c.bpmLo) + "-" + juce::String (c.bpmHi));
        o->setProperty ("half_time", c.halfTime); o->setProperty ("swing_percent", juce::roundToInt (c.swing * 100)); o->setProperty ("kit", c.kit);
        juce::StringArray k; for (auto* x : c.kicks) k.add (x); o->setProperty ("kick_rows", k.joinIntoString (" | "));
        o->setProperty ("snare_row", c.snare); o->setProperty ("hats", juce::String (c.tripletHats ? "3+3+2 grouping" : c.hatRate == 16 ? "16ths" : "8ths")); o->setProperty ("roll_chance", c.rollProb);
        o->setProperty ("sub_808", c.sub ? (c.longSub ? "long, on the kick" : "short, on the kick") : "none (use a bass instrument)"); o->setProperty ("glide_chance", c.glideProb);
        o->setProperty ("key", c.minorKey ? "minor" : "major"); o->setProperty ("extensions", c.ninths ? "7ths and 9ths" : c.sevenths ? "7ths" : "triads");
        o->setProperty ("chord_rhythm", c.chordRhythm == 0 ? "sustained" : c.chordRhythm == 1 ? "stabs" : "arpeggio");
        o->setProperty ("sounds", juce::String (c.chordPreset) + ", " + c.padPreset + ", " + c.leadPreset + ", " + c.counterPreset);
        juce::StringArray f; for (auto& s : c.form) f.add (juce::String (s.name) + " " + juce::String (s.bars)); o->setProperty ("form", f.joinIntoString (", "));
        o->setProperty ("master_lufs", c.lufs);
        return v;
    }
}
