#pragma once
// Squelchmoth 1.13 - the notation engine: MIDI notes -> engraved-part structure (measures, voices, notes and rests with
// spelled durations, ties, tuplets, accidentals, clefs, beams and stems). No UI and no engine types here, so it is
// tested on its own (tests/notation_test.cpp) and shared by the Score window, MusicXML and the `score` tool.
//
// Rules it follows (common-practice engraving):
//   * Display quantise: starts and ends snap to a straight grid (default 1/16) or, where that fits better, to an
//     eighth-note-triplet grid; a beat that holds triplet positions is written as a triplet beat.
//   * One voice per staff: notes that start together form a chord; a note still sounding when the next one starts is
//     cut there (the piano roll keeps the real lengths - this is only how they're written).
//   * Durations: the largest note value that fits, starts on a multiple of its own (undotted) length and doesn't
//     cross the bar; the rest is tied (rests are split the same way, never tied). So syncopations show the beat.
//   * Pitch: spelled from the key (sharps in sharp keys, flats in flat keys; C major / A minor use C# Eb F# G# Bb);
//     accidentals carry through the bar and are cancelled by naturals; a tied note never repeats its accidental.
//   * Clef: treble, bass, or a grand staff split at middle C, from the notes' range.
//   * Beams join eighths and shorter within a beat; stems point away from the middle line.

#include <JuceHeader.h>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

namespace Notation
{
    //==========================================================================
    struct InNote  { double start = 0, length = 1; int pitch = 60, velocity = 100; int id = -1; };
    struct Meter   { int bar = 0; int num = 4, den = 4; };   // from this bar (0-based) on
    struct Key     { int fifths = 0; bool minor = false; };

    enum class Clef { treble, bass };
    enum class StaffMode { autoChoose, treble, bass, grand };

    struct Options
    {
        int grid = 16;              // straight display quantise: 4, 8, 16 or 32 (notes per whole)
        bool triplets = true;       // allow eighth-note triplets
        StaffMode staves = StaffMode::autoChoose;
        int spellPreference = 0;    // 0 = from the key, 1 = always sharps, -1 = always flats
    };

    struct Head
    {
        int pitch = 60;
        int step = 0;               // 0..6 = C D E F G A B
        int alter = 0;              // -2..2
        int octave = 4;             // scientific (C4 = 60)
        bool showAccidental = false;
        int shownAccidental = 0;    // -2 bb, -1 b, 0 natural, 1 #, 2 x
        bool tieToNext = false, tieFromPrev = false;
        int staffLine = 0;          // diatonic steps above the clef's bottom line (0 = bottom line)
        int source = -1;            // the InNote (id, or index when ids aren't set) this head shows
    };

    struct Event
    {
        double start = 0, length = 0;   // beats from the start of the bar (quarter = 1)
        bool rest = false;
        std::vector<Head> heads;        // low to high
        int value = 4;                  // 1 whole, 2 half, 4 quarter, 8, 16, 32
        int dots = 0;
        bool triplet = false;           // 3 in the time of 2
        int beamGroup = -1;             // events with the same id share a beam
        bool stemUp = true;
    };

    struct Staff  { Clef clef = Clef::treble; std::vector<Event> events; };
    struct Measure
    {
        int index = 0; double startBeat = 0, lengthBeats = 4; int num = 4, den = 4;
        bool showMeter = false, showKey = false;
        std::vector<Staff> staves;
    };
    struct Score { Key key; std::vector<Measure> measures; int numStaves = 1; std::vector<Clef> clefs; };

    //==========================================================================
    inline const char* stepNames = "CDEFGAB";
    inline int stepToSemis (int step)   { static const int s[7] = { 0, 2, 4, 5, 7, 9, 11 }; return s[juce::jlimit (0, 6, step)]; }

    /** The key's alteration for each step (sharps F C G D A E B, flats B E A D G C F). */
    inline std::array<int, 7> keyAlters (int fifths)
    {
        std::array<int, 7> a {};
        static const int sharpOrder[7] = { 3, 0, 4, 1, 5, 2, 6 }, flatOrder[7] = { 6, 2, 5, 1, 4, 0, 3 };
        for (int i = 0; i < std::abs (fifths) && i < 7; ++i) a[(size_t) (fifths > 0 ? sharpOrder[i] : flatOrder[i])] = fifths > 0 ? 1 : -1;
        return a;
    }

    /** Spells a MIDI pitch: in the key's scale if it belongs, otherwise by the key's direction. */
    inline void spell (int pitch, int fifths, int preference, int& step, int& alter, int& octave)
    {
        const int pc = ((pitch % 12) + 12) % 12;
        auto ka = keyAlters (fifths);
        // in the key?
        for (int s = 0; s < 7; ++s)
            if (((stepToSemis (s) + ka[(size_t) s]) % 12 + 12) % 12 == pc && preference == 0)
            {
                step = s; alter = ka[(size_t) s];
                octave = (pitch - stepToSemis (s) - alter) / 12 - 1;
                return;
            }
        const bool sharps = preference > 0 || (preference == 0 && fifths > 0);
        const bool flats = preference < 0 || (preference == 0 && fifths < 0);
        static const int natStep[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
        if (natStep[pc] >= 0) { step = natStep[pc]; alter = 0; }
        else if (sharps) { step = natStep[(pc + 11) % 12]; alter = 1; }
        else if (flats)  { step = natStep[(pc + 1) % 12]; alter = -1; }
        else
        {
            // C major / A minor: C# Eb F# G# Bb
            static const int cStep[12] = { 0, 0, 1, 2, 2, 3, 3, 4, 4, 5, 6, 6 }, cAlt[12] = { 0, 1, 0, -1, 0, 0, 1, 0, 1, 0, -1, 0 };
            step = cStep[pc]; alter = cAlt[pc];
        }
        octave = (pitch - stepToSemis (step) - alter) / 12 - 1;
    }

    inline juce::String headName (const Head& h)
    {
        juce::String s = juce::String::charToString ((juce::juce_wchar) stepNames[h.step]);
        if (h.alter == 1) s << "#"; else if (h.alter == 2) s << "x"; else if (h.alter == -1) s << "b"; else if (h.alter == -2) s << "bb";
        return s + juce::String (h.octave);
    }

    inline juce::String valueName (int value, int dots, bool triplet)
    {
        juce::String n = value == 1 ? "whole" : value == 2 ? "half" : value == 4 ? "quarter" : value == 8 ? "eighth" : value == 16 ? "16th" : "32nd";
        if (dots == 1) n = "dotted " + n;
        if (triplet) n = "triplet " + n;
        return n;
    }

    //==========================================================================
    namespace detail
    {
        constexpr double eps = 1.0e-6;
        inline bool nearlyEq (double a, double b)   { return std::abs (a - b) < 1.0e-4; }
        inline bool isMultiple (double x, double unit)   { const double r = x / unit; return std::abs (r - std::round (r)) < 1.0e-4; }

        /** Beat unit in quarters and whether the meter is compound (6/8, 9/8, 12/8). */
        inline void beatOf (int num, int den, double& beat, bool& compound)
        {
            compound = den == 8 && num % 3 == 0 && num > 3;
            beat = compound ? 1.5 : 4.0 / den;
        }

        struct Seg { double start, length; std::vector<int> pitches, ids; };

        /** Writes [p, p + d) inside one bar as note / rest values; triplet beats use triplet values. */
        inline void spellSpan (std::vector<Event>& out, double p, double d, bool rest, const std::vector<int>& pitches,
                               double beat, bool compound, double barLen, const std::vector<bool>& tripletBeat, int grid, const std::vector<int>& ids = {})
        {
            const double unit = 4.0 / grid;
            int guard = 0;
            while (d > eps && ++guard < 512)
            {
                const int bi = juce::jlimit (0, (int) tripletBeat.size() - 1, (int) std::floor (p / beat + eps));
                const bool trip = ! compound && bi < (int) tripletBeat.size() && tripletBeat[(size_t) bi];
                Event e; e.start = p; e.rest = rest;
                for (size_t pi = 0; pi < pitches.size(); ++pi) { Head h; h.pitch = pitches[pi]; h.source = pi < ids.size() ? ids[pi] : -1; e.heads.push_back (h); }
                double chosen = 0;
                if (trip)
                {
                    // inside a triplet beat: triplet quarter (2/3), eighth (1/3), 16th (1/6), never across the beat
                    const double beatEnd = (bi + 1) * beat;
                    const double room = std::min (d, beatEnd - p);
                    static const struct { double len; int value; } tv[] = { { 2.0 / 3.0, 4 }, { 1.0 / 3.0, 8 }, { 1.0 / 6.0, 16 } };
                    for (auto& t : tv)
                        if (t.len <= room + eps && isMultiple (p - bi * beat, t.len)) { chosen = t.len; e.value = t.value; break; }
                    if (chosen == 0) { chosen = std::min (room, 1.0 / 6.0); e.value = 16; }
                    e.triplet = true;
                }
                else
                {
                    struct V { double len; int value, dots; };
                    static const V vs[] = { { 6, 1, 1 }, { 4, 1, 0 }, { 3, 2, 1 }, { 2, 2, 0 }, { 1.5, 4, 1 }, { 1, 4, 0 }, { 0.75, 8, 1 }, { 0.5, 8, 0 },
                                            { 0.375, 16, 1 }, { 0.25, 16, 0 }, { 0.1875, 32, 1 }, { 0.125, 32, 0 } };
                    // don't run into a triplet beat
                    double room = std::min (d, barLen - p);
                    for (int b = bi + 1; b < (int) tripletBeat.size(); ++b) if (tripletBeat[(size_t) b]) { room = std::min (room, b * beat - p); break; }
                    for (auto& v : vs)
                    {
                        if (v.len > room + eps) continue;
                        bool ok;
                        if (v.len <= beat + eps)
                            ok = std::floor (p / beat + eps) == std::floor ((p + v.len - eps) / beat);   // stays inside its beat
                        else
                        {
                            ok = isMultiple (p, beat) && (! compound || isMultiple (v.len, beat));
                            const double half = barLen / 2;
                            const int beatsInBar = (int) std::round (barLen / beat);
                            if (beatsInBar % 2 == 0 && beatsInBar >= 4 && ! isMultiple (p, half) && p < half - eps && p + v.len > half + eps) ok = false;   // keep the middle of the bar visible
                            if (rest && v.dots && ! compound) ok = false;   // no dotted rests longer than a beat in simple time
                        }
                        if (ok) { chosen = v.len; e.value = v.value; e.dots = v.dots; break; }
                    }
                    if (chosen == 0) { chosen = std::min (unit, room); e.value = grid; }
                }
                e.length = chosen;
                p += chosen; d -= chosen;
                if (! rest && d > eps) for (auto& h : e.heads) h.tieToNext = true;
                out.push_back (e);
            }
        }
    }

    //==========================================================================
    inline Clef autoClef (const std::vector<InNote>& notes)
    {
        if (notes.empty()) return Clef::treble;
        double sum = 0; for (auto& n : notes) sum += n.pitch;
        return sum / (double) notes.size() < 57.0 ? Clef::bass : Clef::treble;
    }

    /** Builds the score for one MIDI part. barsToShow = 0: just enough bars for the notes. */
    inline Score build (std::vector<InNote> notes, const std::vector<Meter>& meters, Key key, const Options& opt, int barsToShow = 0)
    {
        Score sc; sc.key = key;
        // staves
        int lo = 127, hi = 0; for (auto& n : notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
        StaffMode mode = opt.staves;
        if (mode == StaffMode::autoChoose) mode = notes.empty() ? StaffMode::treble : (lo < 55 && hi > 67) ? StaffMode::grand : (autoClef (notes) == Clef::bass ? StaffMode::bass : StaffMode::treble);
        sc.numStaves = mode == StaffMode::grand ? 2 : 1;
        sc.clefs = mode == StaffMode::grand ? std::vector<Clef> { Clef::treble, Clef::bass } : std::vector<Clef> { mode == StaffMode::bass ? Clef::bass : Clef::treble };

        // display quantise: straight grid, or the triplet grid when it's clearly closer
        const double straight = 4.0 / opt.grid, trip = 1.0 / 3.0;
        auto q = [&] (double x)
        {
            const double a = std::round (x / straight) * straight, b = std::round (x / trip) * trip;
            return (opt.triplets && std::abs (b - x) + 1.0e-3 < std::abs (a - x) * 0.5) ? b : a;
        };
        for (auto& n : notes) { const double s = q (n.start), e = q (n.start + n.length); n.start = s; n.length = std::max (e - s, straight); }

        // bars
        auto meterAt = [&] (int bar) { Meter m; for (auto& mm : meters) if (mm.bar <= bar) m = mm; return m; };
        double endBeat = 0; for (auto& n : notes) endBeat = std::max (endBeat, n.start + n.length);
        std::vector<Measure> bars;
        double pos = 0; int bi = 0;
        while ((barsToShow > 0 ? bi < barsToShow : (pos < endBeat - detail::eps || bi == 0)) && bi < 10000)
        {
            Measure m; auto mt = meterAt (bi);
            m.index = bi; m.num = mt.num; m.den = mt.den; m.startBeat = pos; m.lengthBeats = mt.num * 4.0 / mt.den;
            m.showMeter = bi == 0 || meterAt (bi - 1).num != mt.num || meterAt (bi - 1).den != mt.den;
            m.showKey = bi == 0;
            bars.push_back (m); pos += m.lengthBeats; ++bi;
        }

        for (int s = 0; s < sc.numStaves; ++s)
        {
            // this staff's notes
            std::vector<InNote> sn;
            for (size_t ni = 0; ni < notes.size(); ++ni)
                if (sc.numStaves == 1 || (s == 0 ? notes[ni].pitch >= 60 : notes[ni].pitch < 60)) { sn.push_back (notes[ni]); if (sn.back().id < 0) sn.back().id = (int) ni; }
            std::sort (sn.begin(), sn.end(), [] (const InNote& a, const InNote& b) { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });
            // chords + single voice
            std::vector<detail::Seg> segs;
            for (size_t i = 0; i < sn.size();)
            {
                detail::Seg g; g.start = sn[i].start; double end = 1e9;
                size_t j = i;
                for (; j < sn.size() && detail::nearlyEq (sn[j].start, g.start); ++j)
                {
                    if (! g.pitches.empty() && g.pitches.back() == sn[j].pitch) continue;   // a doubled note is drawn once
                    g.pitches.push_back (sn[j].pitch); g.ids.push_back (sn[j].id); end = std::min (end, sn[j].start + sn[j].length);
                }
                if (j < sn.size()) end = std::min (end, sn[j].start);
                g.length = end - g.start;
                if (g.length > detail::eps) segs.push_back (g);
                i = j;
            }
            // per bar: triplet beats, then notes and rests
            for (auto& m : bars)
            {
                Staff st; st.clef = sc.clefs[(size_t) s];
                double beat; bool compound; detail::beatOf (m.num, m.den, beat, compound);
                const int nBeats = std::max (1, (int) std::round (m.lengthBeats / beat));
                std::vector<bool> tripletBeat ((size_t) nBeats, false);
                const double m0 = m.startBeat, m1 = m.startBeat + m.lengthBeats;
                for (auto& g : segs)
                    for (double x : { g.start, g.start + g.length })
                        if (x > m0 - detail::eps && x < m1 - detail::eps && ! detail::isMultiple (x - m0, straight) && detail::isMultiple (x - m0, trip))
                            tripletBeat[(size_t) std::min (nBeats - 1, (int) std::floor ((x - m0) / beat + detail::eps))] = true;
                double cur = 0;
                for (auto& g : segs)
                {
                    const double a = std::max (g.start, m0), b = std::min (g.start + g.length, m1);
                    if (b <= a + detail::eps) continue;
                    if (a - m0 > cur + detail::eps) detail::spellSpan (st.events, cur, a - m0 - cur, true, {}, beat, compound, m.lengthBeats, tripletBeat, opt.grid);
                    const size_t first = st.events.size();
                    detail::spellSpan (st.events, a - m0, b - a, false, g.pitches, beat, compound, m.lengthBeats, tripletBeat, opt.grid, g.ids);
                    if (g.start < m0 - detail::eps && first < st.events.size()) for (auto& h : st.events[first].heads) h.tieFromPrev = true;
                    if (g.start + g.length > m1 + detail::eps && ! st.events.empty()) for (auto& h : st.events.back().heads) h.tieToNext = true;
                    cur = b - m0;
                }
                if (cur < m.lengthBeats - detail::eps)
                {
                    if (cur < detail::eps) { Event e; e.start = 0; e.length = m.lengthBeats; e.rest = true; e.value = 1; st.events.push_back (e); }   // whole-bar rest
                    else detail::spellSpan (st.events, cur, m.lengthBeats - cur, true, {}, beat, compound, m.lengthBeats, tripletBeat, opt.grid);
                }
                // tieFromPrev inside the bar
                for (size_t i = 1; i < st.events.size(); ++i)
                    if (! st.events[i].rest && ! st.events[i - 1].rest)
                        for (auto& h : st.events[i].heads)
                            for (auto& p : st.events[i - 1].heads) if (p.pitch == h.pitch && p.tieToNext) h.tieFromPrev = true;
                m.staves.push_back (st);
            }
        }

        // pitch spelling, accidentals, staff positions, beams, stems
        auto ka = keyAlters (key.fifths);
        std::map<int, int> carried;   // pitch -> spelled (step*10+alter+2) for ties across bars
        for (auto& m : bars)
            for (auto& st : m.staves)
            {
                std::map<int, int> state;   // step + 7 * octave -> current alter in this bar
                for (auto& e : st.events)
                    for (auto& h : e.heads)
                    {
                        int step, alter, oct;
                        if (h.tieFromPrev && carried.count (h.pitch)) { const int c = carried[h.pitch]; step = c / 10; alter = c % 10 - 2; oct = (h.pitch - stepToSemis (step) - alter) / 12 - 1; }
                        else spell (h.pitch, key.fifths, opt.spellPreference, step, alter, oct);
                        h.step = step; h.alter = alter; h.octave = oct;
                        const int k = step + 7 * oct;
                        const int current = state.count (k) ? state[k] : ka[(size_t) step];
                        if (h.tieFromPrev) {}   // a tied-over note never shows its accidental, and doesn't carry it to later notes in the new bar
                        else if (current != alter) { h.showAccidental = true; h.shownAccidental = alter; state[k] = alter; }
                        if (h.tieToNext) carried[h.pitch] = step * 10 + alter + 2;
                        // staff position: treble bottom line E4 (step 2, oct 4), bass bottom line G2 (step 4, oct 2)
                        const int diat = step + 7 * oct;
                        h.staffLine = st.clef == Clef::treble ? diat - (2 + 7 * 4) : diat - (4 + 7 * 2);
                    }
                // beams: consecutive notes shorter than a quarter inside one beat
                double beat; bool compound; detail::beatOf (m.num, m.den, beat, compound);
                int nextId = 0;
                for (size_t i = 0; i < st.events.size();)
                {
                    auto beamable = [] (const Event& e) { return ! e.rest && e.value >= 8; };
                    if (! beamable (st.events[i])) { ++i; continue; }
                    const int b = (int) std::floor (st.events[i].start / beat + detail::eps);
                    size_t j = i;
                    while (j < st.events.size() && beamable (st.events[j]) && (int) std::floor (st.events[j].start / beat + detail::eps) == b) ++j;
                    if (j - i >= 2) { for (size_t k = i; k < j; ++k) st.events[k].beamGroup = nextId; ++nextId; }
                    i = j == i ? i + 1 : j;
                }
                // stems: away from the middle line (line 4), per beam group
                std::map<int, double> groupSum; std::map<int, int> groupN;
                for (auto& e : st.events) if (! e.rest && e.beamGroup >= 0) for (auto& h : e.heads) { groupSum[e.beamGroup] += h.staffLine - 4; groupN[e.beamGroup]++; }
                for (auto& e : st.events)
                {
                    if (e.rest || e.heads.empty()) continue;
                    double avg = 0; for (auto& h : e.heads) avg += h.staffLine - 4; avg /= (double) e.heads.size();
                    if (e.beamGroup >= 0) avg = groupSum[e.beamGroup] / std::max (1, groupN[e.beamGroup]);
                    e.stemUp = avg < 0;
                }
            }
        sc.measures = std::move (bars);
        return sc;
    }

    //==========================================================================
    /** Plain-text notation of a score (what the `score` tool returns). */
    inline juce::String toText (const Score& sc, int maxBars = 64)
    {
        juce::String out;
        static const char* majors[] = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
        static const char* minors[] = { "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#" };
        out << "Key " << (sc.key.minor ? minors : majors)[juce::jlimit (0, 14, sc.key.fifths + 7)] << (sc.key.minor ? " minor" : " major")
            << ", " << (sc.numStaves == 2 ? "grand staff" : sc.clefs[0] == Clef::bass ? "bass clef" : "treble clef") << "\n";
        for (auto& m : sc.measures)
        {
            if (m.index >= maxBars) { out << "...\n"; break; }
            out << "Bar " << (m.index + 1) << (m.showMeter ? " (" + juce::String (m.num) + "/" + juce::String (m.den) + ")" : juce::String()) << ":";
            for (size_t s = 0; s < m.staves.size(); ++s)
            {
                if (m.staves.size() > 1) out << (s == 0 ? " RH" : " | LH");
                for (auto& e : m.staves[s].events)
                {
                    out << " ";
                    if (e.rest) { out << (e.value == 1 && e.length >= 3 ? juce::String ("whole-bar rest") : "rest " + valueName (e.value, e.dots, e.triplet)); continue; }
                    if (e.heads.size() > 1) { out << "["; for (size_t i = 0; i < e.heads.size(); ++i) out << (i ? " " : "") << headName (e.heads[i]); out << "]"; }
                    else out << headName (e.heads[0]);
                    out << " " << valueName (e.value, e.dots, e.triplet);
                    if (! e.heads.empty() && e.heads[0].tieToNext) out << "~";
                }
            }
            out << "\n";
        }
        return out;
    }

    /** Parses "C#5", "Bb3", "F##4", "Ebb2" -> MIDI pitch (or -1). */
    inline int parsePitch (const juce::String& s)
    {
        auto t = s.trim();
        if (t.isEmpty()) return -1;
        const int step = juce::String (stepNames).indexOfChar (juce::CharacterFunctions::toUpperCase (t[0]));
        if (step < 0) return -1;
        int i = 1, alter = 0;
        while (i < t.length() && (t[i] == '#' || t[i] == 'b' || t[i] == 'x'))
        { alter += t[i] == '#' ? 1 : t[i] == 'x' ? 2 : -1; ++i; }
        auto rest = t.substring (i);
        if (rest.isEmpty() || ! rest.trimCharactersAtStart ("-").containsOnly ("0123456789")) return -1;
        const int oct = rest.getIntValue();
        const int p = (oct + 1) * 12 + stepToSemis (step) + alter;
        return p >= 0 && p <= 127 ? p : -1;
    }

    /** "quarter", "dotted eighth", "triplet eighth", "half", "16th", "1/8", "3/16" -> length in beats (quarters). */
    inline double parseDuration (const juce::String& s)
    {
        auto t = s.trim().toLowerCase();
        double mult = 1.0;
        if (t.startsWith ("dotted ")) { mult = 1.5; t = t.substring (7); }
        if (t.startsWith ("triplet ")) { mult = 2.0 / 3.0; t = t.substring (8); }
        if (t.contains ("/")) { const double n = t.upToFirstOccurrenceOf ("/", false, false).getDoubleValue(), d = t.fromFirstOccurrenceOf ("/", false, false).getDoubleValue(); return d > 0 ? n * 4.0 / d * mult : 0; }
        static const std::pair<const char*, double> names[] = { { "whole", 4 }, { "half", 2 }, { "quarter", 1 }, { "eighth", 0.5 }, { "8th", 0.5 }, { "sixteenth", 0.25 }, { "16th", 0.25 }, { "32nd", 0.125 } };
        for (auto& n : names) if (t == n.first) return n.second * mult;
        return 0;
    }
}
