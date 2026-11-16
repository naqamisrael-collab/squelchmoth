#pragma once
// Squelchmoth MIDI tools - the note-editing brains shared by the piano roll and Claude (midi_tools).
// Scales and snap-to-scale, chord stamps with inversions, chord progressions by mood (with an 808 / bass line that
// follows them), strum, arpeggiate, flam, quantise with strength and swing, humanise, legato, 808 slides,
// chop, randomise in key, velocity scaling, flip. Every edit goes through the Edit's undo manager.

#include "Behaviours.h"
#include "Scales.h"

namespace MidiTools
{
    using Notes = juce::Array<te::MidiNote*>;

    //==========================================================================
    // Scales
    using Scale = SqScales::Scale;
    inline const std::vector<Scale>& scales()        { return SqScales::scales(); }
    inline const juce::StringArray& noteNames()      { return SqScales::noteNames(); }

    inline int scaleIndex (const juce::String& name)
    {
        auto& s = scales();
        for (int i = 0; i < (int) s.size(); ++i) if (name.trim().equalsIgnoreCase (s[(size_t) i].name)) return i;
        for (int i = 0; i < (int) s.size(); ++i) if (juce::String (s[(size_t) i].name).containsIgnoreCase (name.trim()) && name.trim().isNotEmpty()) return i;
        if (name.containsIgnoreCase ("minor")) return 2;
        if (name.containsIgnoreCase ("major")) return 1;
        return 0;
    }
    inline int rootIndex (const juce::String& name)
    {
        auto t = name.trim().toUpperCase().replace ("B", "b").replace ("b", "B");
        static const char* flats[] = { "C", "DB", "D", "EB", "E", "F", "GB", "G", "AB", "A", "BB", "B" };
        auto u = name.trim().toUpperCase();
        for (int i = 0; i < 12; ++i) if (u == noteNames()[i] || u == flats[i]) return i;
        juce::ignoreUnused (t);
        return 0;
    }

    inline bool inScale (int note, int root, int scale)
    {
        auto& st = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, scale)].steps;
        const int pc = ((note - root) % 12 + 12) % 12;
        return std::find (st.begin(), st.end(), pc) != st.end();
    }
    /** Nearest note in the scale (ties go down). */
    inline int snapToScale (int note, int root, int scale)
    {
        for (int d = 0; d < 12; ++d)
        {
            if (inScale (note - d, root, scale)) return juce::jlimit (0, 127, note - d);
            if (inScale (note + d, root, scale)) return juce::jlimit (0, 127, note + d);
        }
        return note;
    }
    /** Moves a note by whole scale steps (diatonic transpose). */
    inline int scaleStep (int note, int steps, int root, int scale)
    {
        int n = snapToScale (note, root, scale);
        const int dir = steps > 0 ? 1 : -1;
        for (int k = 0; k < std::abs (steps); ++k)
        {
            n += dir;
            while (! inScale (n, root, scale) && n > 0 && n < 127) n += dir;
        }
        return juce::jlimit (0, 127, n);
    }

    //==========================================================================
    // Chords
    struct ChordType { const char* name; const char* symbol; std::vector<int> intervals; };
    inline const std::vector<ChordType>& chordTypes()
    {
        static const std::vector<ChordType> c {
            { "Major", "", { 0, 4, 7 } }, { "Minor", "m", { 0, 3, 7 } }, { "Diminished", "dim", { 0, 3, 6 } }, { "Augmented", "aug", { 0, 4, 8 } },
            { "Sus2", "sus2", { 0, 2, 7 } }, { "Sus4", "sus4", { 0, 5, 7 } }, { "Power (5)", "5", { 0, 7, 12 } },
            { "Major 6", "6", { 0, 4, 7, 9 } }, { "Minor 6", "m6", { 0, 3, 7, 9 } }, { "Dominant 7", "7", { 0, 4, 7, 10 } },
            { "Major 7", "maj7", { 0, 4, 7, 11 } }, { "Minor 7", "m7", { 0, 3, 7, 10 } }, { "Half-diminished", "m7b5", { 0, 3, 6, 10 } },
            { "Diminished 7", "dim7", { 0, 3, 6, 9 } }, { "Add 9", "add9", { 0, 4, 7, 14 } }, { "Minor add 9", "madd9", { 0, 3, 7, 14 } },
            { "Dominant 9", "9", { 0, 4, 7, 10, 14 } }, { "Major 9", "maj9", { 0, 4, 7, 11, 14 } }, { "Minor 9", "m9", { 0, 3, 7, 10, 14 } },
            { "Minor 11", "m11", { 0, 3, 7, 10, 14, 17 } }, { "Dominant 13", "13", { 0, 4, 7, 10, 14, 21 } } };
        return c;
    }
    inline int chordIndex (const juce::String& name)
    {
        auto& c = chordTypes();
        for (int i = 0; i < (int) c.size(); ++i) if (name.trim().equalsIgnoreCase (c[(size_t) i].name) || name.trim() == c[(size_t) i].symbol) return i;
        for (int i = 0; i < (int) c.size(); ++i) if (juce::String (c[(size_t) i].name).containsIgnoreCase (name.trim()) && name.trim().isNotEmpty()) return i;
        return 0;
    }
    /** Chord notes from a root, inverted (each inversion lifts the lowest note an octave). */
    inline juce::Array<int> chordNotes (int root, int type, int inversion = 0)
    {
        juce::Array<int> notes;
        for (auto iv : chordTypes()[(size_t) juce::jlimit (0, (int) chordTypes().size() - 1, type)].intervals) notes.add (root + iv);
        for (int k = 0; k < inversion && ! notes.isEmpty(); ++k) { auto lo = notes.removeAndReturn (0); notes.add (lo + 12); }
        juce::Array<int> out;
        for (auto n : notes) if (n >= 0 && n <= 127) out.add (n);
        return out;
    }
    /** Keys that are written with flats (F, Bb, Eb, Ab, Db, Gb major and their relative minors). */
    inline bool keyUsesFlats (int root, bool minorKey)
    {
        const int pc = ((root % 12) + 12) % 12;
        static const int maj[] = { 5, 10, 3, 8, 1, 6 }, mnr[] = { 2, 7, 0, 5, 10, 3 };
        for (auto k : (minorKey ? mnr : maj)) if (k == pc) return true;
        return false;
    }
    inline juce::String pitchClassName (int pc, bool flats)
    {
        static const char* flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        pc = ((pc % 12) + 12) % 12;
        return flats ? juce::String (flatNames[pc]) : noteNames()[pc];
    }
    inline juce::String noteName (int note, bool flats)   { return pitchClassName (note, flats) + juce::String (note / 12 - 1); }
    inline juce::String chordName (int root, int type, bool flats = false)   { return pitchClassName (root, flats) + chordTypes()[(size_t) type].symbol; }

    //==========================================================================
    // Progressions by mood. Degrees are 1-based scale degrees; 'q' picks the chord quality from the key,
    // or force one with the type index (-1 = from the key, add 7ths when sevenths = true).
    struct ProgChord { int degree; int forceType; };
    struct Progression { const char* name; const char* mood; bool minorKey; std::vector<ProgChord> chords; bool sevenths; };
    inline const std::vector<Progression>& progressions()
    {
        static const std::vector<Progression> p {
            { "Dark Trap (i - VI - III - VII)",        "Trap / Drill",  true,  { { 1, -1 }, { 6, -1 }, { 3, -1 }, { 7, -1 } }, false },
            { "Drill Tension (i - iv - VI - V)",       "Trap / Drill",  true,  { { 1, -1 }, { 4, -1 }, { 6, -1 }, { 5, 0 } }, false },
            { "Sad Piano (i - VI - iv - v)",           "Emotional",     true,  { { 1, -1 }, { 6, -1 }, { 4, -1 }, { 5, -1 } }, false },
            { "Epic Minor (i - VII - VI - VII)",       "Cinematic",     true,  { { 1, -1 }, { 7, -1 }, { 6, -1 }, { 7, -1 } }, false },
            { "Pop Anthem (I - V - vi - IV)",          "Pop",           false, { { 1, -1 }, { 5, -1 }, { 6, -1 }, { 4, -1 } }, false },
            { "Feel Good (I - IV - vi - V)",           "Pop",           false, { { 1, -1 }, { 4, -1 }, { 6, -1 }, { 5, -1 } }, false },
            { "50s / Doo-Wop (I - vi - IV - V)",       "Classic",       false, { { 1, -1 }, { 6, -1 }, { 4, -1 }, { 5, -1 } }, false },
            { "R&B Smooth (ii7 - V7 - Imaj7 - vi7)",   "R&B / Neo-Soul", false, { { 2, -1 }, { 5, -1 }, { 1, -1 }, { 6, -1 } }, true },
            { "Neo-Soul (Imaj7 - iii7 - vi7 - IVmaj7)", "R&B / Neo-Soul", false, { { 1, -1 }, { 3, -1 }, { 6, -1 }, { 4, -1 } }, true },
            { "Lo-Fi Jazz (ii9 - V9 - Imaj9 - Imaj9)", "Lo-Fi",         false, { { 2, 18 }, { 5, 16 }, { 1, 17 }, { 1, 17 } }, false },
            { "Afro / Amapiano (i - iv - VII - III)",  "Afro",          true,  { { 1, 11 }, { 4, 11 }, { 7, 10 }, { 3, 10 } }, false },
            { "Gospel Lift (IV - V - iii - vi)",       "Gospel",        false, { { 4, -1 }, { 5, -1 }, { 3, -1 }, { 6, -1 } }, true },
            { "Reggaeton (i - VI - VII - VI)",         "Latin",         true,  { { 1, -1 }, { 6, -1 }, { 7, -1 }, { 6, -1 } }, false },
            { "House Stab (vi - IV - I - V)",          "Dance",         false, { { 6, -1 }, { 4, -1 }, { 1, -1 }, { 5, -1 } }, true },
            { "Phonk (i - i - VI - V)",                "Phonk",         true,  { { 1, -1 }, { 1, -1 }, { 6, -1 }, { 5, 0 } }, false },
            // 1.8 additions - the Chord Progression Tool library by genre
            { "Trap Melancholy (i - VII - VI - v)",    "Trap / Drill",  true,  { { 1, -1 }, { 7, -1 }, { 6, -1 }, { 5, -1 } }, false },
            { "Trap Minor Loop (i - III - VII - VI)",  "Trap / Drill",  true,  { { 1, -1 }, { 3, -1 }, { 7, -1 }, { 6, -1 } }, false },
            { "Drill Slide (i - VI - VII - i)",        "Trap / Drill",  true,  { { 1, -1 }, { 6, -1 }, { 7, -1 }, { 1, -1 } }, false },
            { "Dark Drill (i - v - VI - VII)",         "Trap / Drill",  true,  { { 1, -1 }, { 5, -1 }, { 6, -1 }, { 7, -1 } }, false },
            { "Harmonic Trap (i - VI - V7 - i)",       "Trap / Drill",  true,  { { 1, -1 }, { 6, -1 }, { 5, 9 }, { 1, -1 } }, false },
            { "R&B Slow Jam (Imaj7 - vi7 - ii7 - V7)",  "R&B / Neo-Soul", false, { { 1, -1 }, { 6, -1 }, { 2, -1 }, { 5, -1 } }, true },
            { "R&B 90s (vi7 - IVmaj7 - Imaj7 - V7)",    "R&B / Neo-Soul", false, { { 6, -1 }, { 4, -1 }, { 1, -1 }, { 5, -1 } }, true },
            { "Neo-Soul Lift (IVmaj7 - iii7 - vi7 - ii7)", "R&B / Neo-Soul", false, { { 4, -1 }, { 3, -1 }, { 6, -1 }, { 2, -1 } }, true },
            { "Gospel Walk (I - I/3 - IV - iv)",       "Gospel",        false, { { 1, -1 }, { 3, -1 }, { 4, -1 }, { 4, 1 } }, true },
            { "Gospel Turnaround (I - vi - ii - V)",   "Gospel",        false, { { 1, -1 }, { 6, -1 }, { 2, -1 }, { 5, -1 } }, true },
            { "Gospel Praise (I - IV - I - V)",        "Gospel",        false, { { 1, -1 }, { 4, -1 }, { 1, -1 }, { 5, -1 } }, true },
            { "Lo-Fi Study (Imaj7 - vi7 - IVmaj7 - V7)", "Lo-Fi",       false, { { 1, -1 }, { 6, -1 }, { 4, -1 }, { 5, -1 } }, true },
            { "Lo-Fi Rain (ii7 - V7 - iii7 - vi7)",    "Lo-Fi",         false, { { 2, -1 }, { 5, -1 }, { 3, -1 }, { 6, -1 } }, true },
            { "Lo-Fi Minor (i7 - iv7 - VII7 - IIImaj7)", "Lo-Fi",       true,  { { 1, -1 }, { 4, -1 }, { 7, -1 }, { 3, -1 } }, true },
            { "Afrobeats Sun (I - V - vi - IV)",       "Afro",          false, { { 1, -1 }, { 5, -1 }, { 6, -1 }, { 4, -1 } }, false },
            { "Afro Minor (i - VI - III - VII)",       "Afro",          true,  { { 1, -1 }, { 6, -1 }, { 3, -1 }, { 7, -1 } }, false },
            { "Amapiano Log (i - iv - v - i)",         "Afro",          true,  { { 1, 11 }, { 4, 11 }, { 5, 11 }, { 1, 11 } }, false },
            { "Highlife (I - IV - V - IV)",            "Afro",          false, { { 1, -1 }, { 4, -1 }, { 5, -1 }, { 4, -1 } }, false },
            { "Deep House (i7 - VI maj7 - III maj7 - VII7)", "Dance",   true,  { { 1, -1 }, { 6, -1 }, { 3, -1 }, { 7, -1 } }, true },
            { "Piano House (vi - IV - V - I)",         "Dance",         false, { { 6, -1 }, { 4, -1 }, { 5, -1 }, { 1, -1 } }, false },
            { "Techno Drone (i - i - i - VI)",         "Dance",         true,  { { 1, -1 }, { 1, -1 }, { 1, -1 }, { 6, -1 } }, false },
            { "Trance Lift (vi - IV - I - V)",         "Dance",         false, { { 6, -1 }, { 4, -1 }, { 1, -1 }, { 5, -1 } }, false },
            { "UK Garage (ii7 - V7 - iii7 - vi7)",     "Dance",         false, { { 2, -1 }, { 5, -1 }, { 3, -1 }, { 6, -1 } }, true },
            { "Cinematic Rise (i - VI - III - VII sus)", "Cinematic",   true,  { { 1, -1 }, { 6, -1 }, { 3, -1 }, { 7, 4 } }, false },
            { "Cinematic Hope (I - V - vi - iii - IV)", "Cinematic",    false, { { 1, -1 }, { 5, -1 }, { 6, -1 }, { 3, -1 }, { 4, -1 } }, false },
            { "Cinematic Doom (i - iv - i - V)",       "Cinematic",     true,  { { 1, -1 }, { 4, -1 }, { 1, -1 }, { 5, 0 } }, false },
            { "Score Pulse (i - VII - VI - V)",        "Cinematic",     true,  { { 1, -1 }, { 7, -1 }, { 6, -1 }, { 5, 0 } }, false },
            { "Pop Ballad (vi - IV - I - V)",          "Pop",           false, { { 6, -1 }, { 4, -1 }, { 1, -1 }, { 5, -1 } }, false },
            { "Pop Punk (I - V - vi - IV - I - V - IV - IV)", "Pop",    false, { { 1, -1 }, { 5, -1 }, { 6, -1 }, { 4, -1 } }, false },
            { "Pop Bright (I - iii - IV - V)",         "Pop",           false, { { 1, -1 }, { 3, -1 }, { 4, -1 }, { 5, -1 } }, false },
            { "Indie (I - V - IV - IV)",               "Pop",           false, { { 1, -1 }, { 5, -1 }, { 4, -1 }, { 4, -1 } }, false },
            { "Emotional Minor (i - VI - III - iv)",   "Emotional",     true,  { { 1, -1 }, { 6, -1 }, { 3, -1 }, { 4, -1 } }, false },
            { "Bittersweet (I - iii - vi - IV)",       "Emotional",     false, { { 1, -1 }, { 3, -1 }, { 6, -1 }, { 4, -1 } }, false },
            { "Jazz ii-V-I (ii7 - V7 - Imaj7)",        "Jazz",          false, { { 2, -1 }, { 5, -1 }, { 1, -1 } }, true },
            { "Jazz Turnaround (Imaj7 - vi7 - ii7 - V7)", "Jazz",       false, { { 1, -1 }, { 6, -1 }, { 2, -1 }, { 5, -1 } }, true },
            { "Minor Blues (i7 - iv7 - i7 - V7)",      "Jazz",          true,  { { 1, -1 }, { 4, -1 }, { 1, -1 }, { 5, 9 } }, true },
            { "Bossa (Imaj7 - ii7 - V7 - Imaj7)",      "Latin",         false, { { 1, -1 }, { 2, -1 }, { 5, -1 }, { 1, -1 } }, true },
            { "Latin Trap (i - VI - VII - i)",         "Latin",         true,  { { 1, -1 }, { 6, -1 }, { 7, -1 }, { 1, -1 } }, false } };
        return p;
    }

    inline juce::StringArray progressionGenres()
    {
        juce::StringArray g; for (auto& p : progressions()) g.addIfNotAlreadyThere (p.mood); return g;
    }

    /** Chord Stamp, top-down: the clicked note is the top voice; the chord is built downwards (melody-led). */
    inline juce::Array<int> chordNotesTopDown (int topNote, int type)
    {
        auto& iv = chordTypes()[(size_t) juce::jlimit (0, (int) chordTypes().size() - 1, type)].intervals;
        juce::Array<int> out; const int span = iv.empty() ? 0 : iv.back();
        for (auto i : iv) out.add (topNote - span + i);
        return out;
    }

    /** Name of the chord the notes spell (pitch-class match against the chord table), or "" when unknown. */
    inline juce::String detectChord (const juce::Array<int>& notes, bool flats = false)
    {
        if (notes.size() < 2) return {};
        std::set<int> pcs; int lowest = 127; for (auto n : notes) { pcs.insert (((n % 12) + 12) % 12); lowest = juce::jmin (lowest, n); }
        juce::String first;
        // prefer the reading whose root is the bass note (Am7 over C6/A), else the first match in root order
        for (int pass = 0; pass < 2; ++pass)
            for (int root = 0; root < 12; ++root)
            {
                if (pass == 0 && root != lowest % 12) continue;
                for (size_t t = 0; t < chordTypes().size(); ++t)
                {
                    std::set<int> want; for (auto i : chordTypes()[t].intervals) want.insert ((root + i) % 12);
                    if (want == pcs) { auto name = chordName (root, (int) t, flats); if (lowest % 12 != root) name << "/" << pitchClassName (lowest % 12, flats); return name; }
                }
            }
        return first;
    }

    /** Root note and chord type of a scale degree in a key (triads or sevenths stacked in thirds from the scale). */
    inline std::pair<int, int> degreeChord (int degree, int keyRoot, bool minorKey, bool sevenths, int forceType)
    {
        auto& st = scales()[(size_t) (minorKey ? 2 : 1)].steps;
        const int d = juce::jlimit (1, 7, degree) - 1;
        auto at = [&] (int k) { return st[(size_t) (k % 7)] + 12 * (k / 7); };
        const int root = keyRoot + at (d);
        if (forceType >= 0) return { root, forceType };
        const int third = at (d + 2) - at (d), fifth = at (d + 4) - at (d), seventh = at (d + 6) - at (d);
        int type = 0;
        if (third == 4 && fifth == 7) type = sevenths ? (seventh == 11 ? 10 : 9) : 0;
        else if (third == 3 && fifth == 7) type = sevenths ? 11 : 1;
        else if (third == 3 && fifth == 6) type = sevenths ? 12 : 2;
        else if (third == 4 && fifth == 8) type = 3;
        return { root, type };
    }

    inline juce::UndoManager* um (te::MidiClip& c)   { return &c.edit.getUndoManager(); }

    //==========================================================================
    // Writing
    /** Stamps a chord; returns the new notes. */
    inline Notes stampChord (te::MidiClip& clip, int root, int type, int inversion, double startBeat, double lengthBeats, int velocity = 96)
    {
        Notes made;
        for (auto n : chordNotes (root, type, inversion))
            made.add (clip.getSequence().addNote (n, te::BeatPosition::fromBeats (startBeat), te::BeatDuration::fromBeats (lengthBeats), velocity, 0, um (clip)));
        return made;
    }

    /** Everything the Chord Progression Tool can set when it writes chords. */
    struct ChordWriteOptions
    {
        int keyRoot = 0; bool minorKey = false; bool sevenths = false;
        double startBeat = 0.0, beatsPerChord = 4.0, totalBeats = 16.0;
        int octave = 4, velocity = 92;
        bool withBass = true, voiceLead = true, humanise = false;
        double strumBeats = 0.0; bool strumUp = true;     // 0 = no strum
        int seed = 1;
    };

    inline void strum (te::MidiClip& clip, Notes ns, double stepBeats, bool up);
    inline void humanise (te::MidiClip& clip, Notes ns, double timingBeats, int velRange, int seed);

    /** The chord a progression step resolves to in a key: { root pitch class, chord type }. */
    inline std::pair<int, int> resolveChord (const ProgChord& pc, int keyRoot, bool minorKey, bool sevenths)
    {
        auto [root, type] = degreeChord (pc.degree, keyRoot % 12, minorKey, sevenths, pc.forceType);
        return { ((root % 12) + 12) % 12, type };
    }

    /** Chord names for a step list in a key ("Am", "F", "C", "G"). */
    inline juce::StringArray chordNamesFor (const std::vector<ProgChord>& chords, int keyRoot, bool minorKey, bool sevenths)
    {
        juce::StringArray names;
        for (auto& pc : chords) { auto [r, t] = resolveChord (pc, keyRoot, minorKey, sevenths); names.add (chordName (r, t, keyUsesFlats (keyRoot, minorKey))); }
        return names;
    }

    /** Writes a list of chords (one per 'beatsPerChord'), repeated to fill 'totalBeats', voice-led, with options.
        Returns the chord names (one per step), and the notes it made through 'madeOut' when given. */
    inline juce::StringArray writeChords (te::MidiClip& clip, const std::vector<ProgChord>& chords, const ChordWriteOptions& o, Notes* madeOut = nullptr)
    {
        juce::StringArray names;
        if (chords.empty()) return names;
        int prevCentre = (o.octave + 1) * 12 + 4;
        const int count = juce::jmax (1, (int) std::round (o.totalBeats / o.beatsPerChord));
        Notes everything;
        for (int k = 0; k < count; ++k)
        {
            auto pc = chords[(size_t) (k % (int) chords.size())];
            auto [root, type] = resolveChord (pc, o.keyRoot, o.minorKey, o.sevenths);
            const int r = (o.octave + 1) * 12 + root;
            // voice leading: pick the inversion whose average pitch is closest to the previous chord
            int bestInv = 0; double bestDist = 1e9;
            const int maxInv = o.voiceLead ? (int) chordTypes()[(size_t) type].intervals.size() : 1;
            for (int inv = 0; inv < maxInv; ++inv)
                for (int shift : { -12, 0 })
                {
                    if (! o.voiceLead && shift != 0) continue;
                    auto ns = chordNotes (r + shift, type, inv);
                    double avg = 0; for (auto n : ns) avg += n; avg /= juce::jmax (1, ns.size());
                    auto dist = std::abs (avg - prevCentre);
                    if (dist < bestDist) { bestDist = dist; bestInv = inv + (shift < 0 ? 100 : 0); }
                }
            const int shift = bestInv >= 100 ? -12 : 0, inv = bestInv % 100;
            const double at = o.startBeat + k * o.beatsPerChord;
            auto made = stampChord (clip, r + shift, type, inv, at, o.beatsPerChord * 0.98, o.velocity);
            double avg = 0; for (auto* n : made) avg += n->getNoteNumber(); prevCentre = (int) (avg / juce::jmax (1, made.size()));
            if (o.strumBeats > 0) strum (clip, made, o.strumBeats, o.strumUp);
            everything.addArray (made);
            if (o.withBass)
                everything.add (clip.getSequence().addNote (24 + root + (root > 6 ? 0 : 12), te::BeatPosition::fromBeats (at),
                                                            te::BeatDuration::fromBeats (o.beatsPerChord * 0.95), juce::jmin (127, o.velocity + 12), 0, um (clip)));
            if (k < (int) chords.size()) names.add (chordName (root, type, keyUsesFlats (o.keyRoot, o.minorKey)));
        }
        if (o.humanise) humanise (clip, everything, 0.015, 10, o.seed);
        if (madeOut) *madeOut = everything;
        return names;
    }

    /** Writes a library progression (one chord per 'beatsPerChord'), repeated to fill 'totalBeats'. Optional bass / 808 line. */
    inline juce::StringArray writeProgression (te::MidiClip& clip, int progIndex, int keyRoot, double startBeat, double beatsPerChord,
                                               double totalBeats, int octave = 4, bool withBass = true, bool voiceLead = true)
    {
        auto& pr = progressions()[(size_t) juce::jlimit (0, (int) progressions().size() - 1, progIndex)];
        ChordWriteOptions o;
        o.keyRoot = keyRoot; o.minorKey = pr.minorKey; o.sevenths = pr.sevenths; o.startBeat = startBeat; o.beatsPerChord = beatsPerChord;
        o.totalBeats = totalBeats; o.octave = octave; o.withBass = withBass; o.voiceLead = voiceLead;
        return writeChords (clip, pr.chords, o);
    }

    /** Pure version for tests and MCP: the note numbers each chord of a step list would get (no clip needed), voice-led. */
    inline std::vector<juce::Array<int>> progressionNotes (const std::vector<ProgChord>& chords, int keyRoot, bool minorKey, bool sevenths, int octave = 4, bool voiceLead = true)
    {
        std::vector<juce::Array<int>> out;
        int prevCentre = (octave + 1) * 12 + 4;
        for (auto& pc : chords)
        {
            auto [root, type] = resolveChord (pc, keyRoot, minorKey, sevenths);
            const int r = (octave + 1) * 12 + root;
            int bestInv = 0; double bestDist = 1e9;
            const int maxInv = voiceLead ? (int) chordTypes()[(size_t) type].intervals.size() : 1;
            for (int inv = 0; inv < maxInv; ++inv)
                for (int shift : { -12, 0 })
                {
                    if (! voiceLead && shift != 0) continue;
                    auto ns = chordNotes (r + shift, type, inv);
                    double avg = 0; for (auto n : ns) avg += n; avg /= juce::jmax (1, ns.size());
                    auto dist = std::abs (avg - prevCentre);
                    if (dist < bestDist) { bestDist = dist; bestInv = inv + (shift < 0 ? 100 : 0); }
                }
            auto ns = chordNotes (r + (bestInv >= 100 ? -12 : 0), type, bestInv % 100);
            double avg = 0; for (auto n : ns) avg += n; prevCentre = (int) (avg / juce::jmax (1, ns.size()));
            out.push_back (ns);
        }
        return out;
    }

    /** Parses "I - V - vi - IV", "1 5 6 4", "i VI III VII" into steps (case picks major / minor quality; lowercase forces minor, uppercase major,
        a trailing 7 adds a seventh, "dim" / "aug" / "sus2" / "sus4" force those). */
    inline std::vector<ProgChord> parseDegrees (const juce::String& text)
    {
        std::vector<ProgChord> out;
        juce::StringArray toks; toks.addTokens (text.replace ("-", " ").replace (",", " ").replace ("|", " "), " ", "");
        toks.removeEmptyStrings();
        static const std::pair<const char*, int> romans[] = { { "vii", 7 }, { "iii", 3 }, { "vi", 6 }, { "iv", 4 }, { "ii", 2 }, { "v", 5 }, { "i", 1 } };
        for (auto tok : toks)
        {
            auto t = tok.trim(); int degree = 0; juce::String rest;
            if (t.getIntValue() > 0) { degree = t.getIntValue(); rest = t.trimCharactersAtStart ("0123456789"); }
            else for (auto& [rn, d] : romans) if (t.toLowerCase().startsWith (rn)) { degree = d; rest = t.substring ((int) strlen (rn)); break; }
            if (degree < 1 || degree > 7) continue;
            int force = -1;
            const bool lower = t.getIntValue() == 0 && t.substring (0, 1).toLowerCase() == t.substring (0, 1);
            if (rest.startsWithIgnoreCase ("dim7")) force = 13; else if (rest.startsWithIgnoreCase ("dim")) force = 2; else if (rest.startsWithIgnoreCase ("aug")) force = 3;
            else if (rest.startsWithIgnoreCase ("sus2")) force = 4; else if (rest.startsWithIgnoreCase ("sus4")) force = 5;
            else if (rest.startsWithIgnoreCase ("maj7")) force = 10; else if (rest.startsWithIgnoreCase ("m7")) force = 11; else if (rest.startsWith ("7")) force = lower ? 11 : 9;
            else if (rest.startsWithIgnoreCase ("m9")) force = 18; else if (rest.startsWith ("9")) force = 16; else if (rest.startsWithIgnoreCase ("add9")) force = 14;
            else if (t.getIntValue() == 0 && rest.isEmpty()) force = lower ? 1 : 0;   // roman numeral case decides the triad quality
            out.push_back ({ degree, force });
        }
        return out;
    }

    inline Notes stampChordTopDown (te::MidiClip& clip, int topNote, int type, double startBeat, double lengthBeats, int velocity = 96)
    {
        Notes made;
        for (auto n : chordNotesTopDown (topNote, type))
            if (n >= 0) made.add (clip.getSequence().addNote (n, te::BeatPosition::fromBeats (startBeat), te::BeatDuration::fromBeats (lengthBeats), velocity, 0, um (clip)));
        return made;
    }

    //==========================================================================
    // Transforms (act on the given notes; empty = all notes in the clip)
    inline Notes notesOrAll (te::MidiClip& clip, const Notes& sel)
    {
        if (! sel.isEmpty()) return sel;
        Notes all;
        for (auto* n : clip.getSequence().getNotes()) all.add (n);
        return all;
    }
    inline void sortByTime (Notes& ns)
    {
        std::sort (ns.begin(), ns.end(), [] (te::MidiNote* a, te::MidiNote* b)
        {
            if (a->getStartBeat() != b->getStartBeat()) return a->getStartBeat() < b->getStartBeat();
            return a->getNoteNumber() < b->getNoteNumber();
        });
    }
    /** Groups of notes that start together (chords). */
    inline std::vector<Notes> chordsIn (Notes ns, double tolerance = 0.02)
    {
        sortByTime (ns);
        std::vector<Notes> groups;
        for (auto* n : ns)
        {
            if (groups.empty() || std::abs (groups.back().getFirst()->getStartBeat().inBeats() - n->getStartBeat().inBeats()) > tolerance) groups.push_back ({});
            groups.back().add (n);
        }
        return groups;
    }

    inline void setStart (te::MidiClip& clip, te::MidiNote* n, double beat)
    {
        n->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (0.0, beat)), n->getLengthBeats(), um (clip));
    }
    inline void setLength (te::MidiClip& clip, te::MidiNote* n, double len)
    {
        n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (juce::jmax (1.0 / 64.0, len)), um (clip));
    }

    /** Quantise starts (and optionally ends) to a grid with strength 0..1 and swing 0..1 on every second grid line. */
    inline void quantise (te::MidiClip& clip, Notes ns, double grid, float strength = 1.0f, float swing = 0.0f, bool ends = false)
    {
        for (auto* n : notesOrAll (clip, ns))
        {
            const double s = n->getStartBeat().inBeats();
            const int idx = (int) std::round (s / grid);
            double target = idx * grid + ((idx % 2) == 1 ? swing * grid * 0.5 : 0.0);
            auto len = n->getLengthBeats().inBeats();
            setStart (clip, n, s + (target - s) * strength);
            if (ends) setLength (clip, n, juce::jmax (grid, std::round (len / grid) * grid));
        }
    }

    /** Strums every chord: each note a little later than the one below (up) or above (down). */
    inline void strum (te::MidiClip& clip, Notes ns, double stepBeats, bool up = true)
    {
        for (auto& chord : chordsIn (notesOrAll (clip, ns)))
        {
            auto c = chord;
            std::sort (c.begin(), c.end(), [up] (te::MidiNote* a, te::MidiNote* b) { return up ? a->getNoteNumber() < b->getNoteNumber() : a->getNoteNumber() > b->getNoteNumber(); });
            const double start = c.getFirst()->getStartBeat().inBeats();
            for (int i = 0; i < c.size(); ++i)
            {
                auto end = c[i]->getEndBeat().inBeats();
                setStart (clip, c[i], start + i * stepBeats);
                setLength (clip, c[i], end - (start + i * stepBeats));
            }
        }
    }

    enum class ArpMode { up, down, upDown, random, chordOrder };
    /** Replaces each held chord with an arpeggio at 'rate' beats per note (gate 0..1). */
    inline void arpeggiate (te::MidiClip& clip, Notes ns, double rate, ArpMode mode, float gate = 0.8f, int octaves = 1)
    {
        juce::Random rnd (42);
        for (auto& chord : chordsIn (notesOrAll (clip, ns)))
        {
            juce::Array<int> pitches; int vel = 96;
            double start = chord.getFirst()->getStartBeat().inBeats(), end = start;
            for (auto* n : chord) { pitches.add (n->getNoteNumber()); end = juce::jmax (end, n->getEndBeat().inBeats()); vel = n->getVelocity(); }
            pitches.sort();
            juce::Array<int> base = pitches;
            for (int o = 1; o < octaves; ++o) for (auto p : base) pitches.add (p + 12 * o);
            juce::Array<int> order;
            if (mode == ArpMode::down) for (int i = pitches.size(); --i >= 0;) order.add (pitches[i]);
            else if (mode == ArpMode::upDown) { order = pitches; for (int i = pitches.size() - 2; i > 0; --i) order.add (pitches[i]); }
            else order = pitches;
            for (auto* n : chord) clip.getSequence().removeNote (*n, um (clip));
            int k = 0;
            for (double b = start; b < end - 1e-6; b += rate, ++k)
            {
                int p = mode == ArpMode::random ? order[rnd.nextInt (order.size())] : order[k % order.size()];
                clip.getSequence().addNote (p, te::BeatPosition::fromBeats (b), te::BeatDuration::fromBeats (juce::jmin (rate * gate, end - b)), vel, 0, um (clip));
            }
        }
    }

    /** A grace hit just before each note (drum flams / piano grace notes). */
    inline void flam (te::MidiClip& clip, Notes ns, double offsetBeats = 1.0 / 32.0, float velScale = 0.6f)
    {
        for (auto* n : notesOrAll (clip, ns))
        {
            auto s = n->getStartBeat().inBeats();
            if (s - offsetBeats < 0) continue;
            clip.getSequence().addNote (n->getNoteNumber(), te::BeatPosition::fromBeats (s - offsetBeats), te::BeatDuration::fromBeats (offsetBeats * 0.9),
                                        juce::jlimit (1, 127, (int) (n->getVelocity() * velScale)), 0, um (clip));
        }
    }

    inline void humanise (te::MidiClip& clip, Notes ns, double timingBeats = 0.02, int velRange = 12, int seed = 1)
    {
        juce::Random r (seed);
        for (auto* n : notesOrAll (clip, ns))
        {
            setStart (clip, n, n->getStartBeat().inBeats() + (r.nextDouble() * 2.0 - 1.0) * timingBeats);
            n->setVelocity (juce::jlimit (1, 127, n->getVelocity() + r.nextInt (velRange * 2 + 1) - velRange), um (clip));
        }
    }

    /** Each note lasts until the next one starts. */
    inline void legato (te::MidiClip& clip, Notes ns)
    {
        auto groups = chordsIn (notesOrAll (clip, ns));
        for (size_t g = 0; g + 1 < groups.size(); ++g)
        {
            auto next = groups[g + 1].getFirst()->getStartBeat().inBeats();
            for (auto* n : groups[g]) setLength (clip, n, next - n->getStartBeat().inBeats());
        }
    }

    /** 808 slides: each note overlaps the next by 'glideBeats', so a mono / legato instrument (Moth 808) glides into it. */
    inline void slides (te::MidiClip& clip, Notes ns, double glideBeats = 0.125)
    {
        auto groups = chordsIn (notesOrAll (clip, ns));
        for (size_t g = 0; g + 1 < groups.size(); ++g)
        {
            auto next = groups[g + 1].getFirst()->getStartBeat().inBeats();
            for (auto* n : groups[g]) setLength (clip, n, next - n->getStartBeat().inBeats() + glideBeats);
        }
    }

    /** Cuts every note into pieces 'rate' beats long (trap hats, stutters). */
    inline void chop (te::MidiClip& clip, Notes ns, double rate)
    {
        for (auto* n : notesOrAll (clip, ns))
        {
            const double s = n->getStartBeat().inBeats(), e = n->getEndBeat().inBeats();
            const int p = n->getNoteNumber(), v = n->getVelocity();
            if (e - s <= rate * 1.01) continue;
            clip.getSequence().removeNote (*n, um (clip));
            for (double b = s; b < e - 1e-6; b += rate)
                clip.getSequence().addNote (p, te::BeatPosition::fromBeats (b), te::BeatDuration::fromBeats (juce::jmin (rate * 0.95, e - b)), v, 0, um (clip));
        }
    }

    inline void scaleVelocities (te::MidiClip& clip, Notes ns, float factor, int add = 0)
    {
        for (auto* n : notesOrAll (clip, ns)) n->setVelocity (juce::jlimit (1, 127, (int) std::round (n->getVelocity() * factor) + add), um (clip));
    }

    inline void snapNotesToScale (te::MidiClip& clip, Notes ns, int root, int scale)
    {
        for (auto* n : notesOrAll (clip, ns)) n->setNoteNumber (snapToScale (n->getNoteNumber(), root, scale), um (clip));
    }
    inline void transposeInScale (te::MidiClip& clip, Notes ns, int steps, int root, int scale)
    {
        for (auto* n : notesOrAll (clip, ns)) n->setNoteNumber (scaleStep (n->getNoteNumber(), steps, root, scale), um (clip));
    }
    /** Mirrors pitches around the middle of the selection. */
    inline void flip (te::MidiClip& clip, Notes ns)
    {
        auto all = notesOrAll (clip, ns);
        int lo = 127, hi = 0;
        for (auto* n : all) { lo = juce::jmin (lo, n->getNoteNumber()); hi = juce::jmax (hi, n->getNoteNumber()); }
        for (auto* n : all) n->setNoteNumber (juce::jlimit (0, 127, lo + hi - n->getNoteNumber()), um (clip));
    }

    /** A random melody in the key over a range of beats (FL's Randomize / a riff starter). */
    inline Notes randomMelody (te::MidiClip& clip, int root, int scale, double startBeat, double lengthBeats, double grid, int lowNote, int highNote,
                               float density = 0.6f, int seed = 3)
    {
        juce::Random r (seed);
        Notes made;
        int last = snapToScale ((lowNote + highNote) / 2, root, scale);
        for (double b = startBeat; b < startBeat + lengthBeats - 1e-6; b += grid)
        {
            if (r.nextFloat() > density) continue;
            last = juce::jlimit (lowNote, highNote, scaleStep (last, r.nextInt (5) - 2, root, scale));
            const double len = grid * (r.nextFloat() < 0.25f ? 2.0 : 1.0);
            made.add (clip.getSequence().addNote (last, te::BeatPosition::fromBeats (b), te::BeatDuration::fromBeats (len * 0.95), 80 + r.nextInt (35), 0, um (clip)));
        }
        return made;
    }
}
