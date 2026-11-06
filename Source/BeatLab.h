#pragma once
// Squelchmoth Beat Lab - the model behind the Beat Maker (FL Studio's Channel Rack speed, Studio Pro's pattern
// depth, Logic's step-sequencer tricks), living on an ordinary Pro Tools-style instrument track with a step clip.
//
//  * Patterns A-H per clip and a pattern chain ("A A B A") that fills the clip; or loop just the pattern you edit.
//  * Per-step lanes: velocity, gate, chance (probability), shift (micro-timing +/- half a step), repeat (rolls).
//  * Per-row: mute, solo, lock (keeps the row when the dice rolls), length (polyrhythm), swing amount, level, pan.
//  * Step tools: fill every N, Euclidean, rotate, dice, humanise, copy / paste, double, clear.
//  * Beat Starter: one click writes a genre-correct groove (17 genres) - rows can be locked and re-rolled.
//  * Send to piano roll: the pattern becomes an editable MIDI clip.
// Everything is stored on the clip's ValueTree, so it saves with the session, undoes, and plays in the engine
// (see the Squelchmoth patch in tracktion_StepClip.cpp: swing, shift, repeats, mute/solo and row length).

#include "DrumKit.h"

namespace BeatLab
{
    static constexpr int maxPatterns = 8;
    inline juce::String patternName (int i)   { return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, 25, i))); }

    inline juce::UndoManager* um (te::StepClip& c)   { return &c.edit.getUndoManager(); }

    //==========================================================================
    // Patterns and the chain
    inline void ensurePatterns (te::StepClip& clip, int count)
    {
        clip.createDefaultPatternIfEmpty();
        while (clip.getNumPatterns() < juce::jlimit (1, maxPatterns, count))
        {
            // copy pattern A's structure (its rows), then empty it
            clip.insertPattern (clip.getPattern (0), -1);
            auto p = clip.getPattern (clip.getNumPatterns() - 1);
            for (int ch = 0; ch < p.state.getNumChildren(); ++ch)
            {
                p.clearChannel (ch);
                for (auto* prop : { "sqmShifts", "sqmRepeats", "sqmLen" }) p.state.getChild (ch).removeProperty (prop, nullptr);
            }
        }
    }

    inline int editPattern (te::StepClip& clip)
    {
        return juce::jlimit (0, juce::jmax (0, clip.getNumPatterns() - 1), (int) clip.state.getProperty ("sqmEditPattern", 0));
    }
    inline void setEditPattern (te::StepClip& clip, int i)
    {
        ensurePatterns (clip, i + 1);
        clip.state.setProperty ("sqmEditPattern", juce::jlimit (0, maxPatterns - 1, i), nullptr);
    }
    inline te::StepClip::Pattern pattern (te::StepClip& clip)   { return clip.getPattern (editPattern (clip)); }

    inline bool patternHasNotes (te::StepClip& clip, int index)
    {
        if (index >= clip.getNumPatterns()) return false;
        auto p = clip.getPattern (index);
        for (int ch = 0; ch < clip.getChannels().size(); ++ch)
            if (! p.getChannel (ch).isZero()) return true;
        return false;
    }

    /** The chain as pattern indices, e.g. "A A B A" -> 0 0 1 0. Empty = just pattern A. */
    inline juce::Array<int> chain (te::StepClip& clip)
    {
        juce::Array<int> c;
        for (auto& t : juce::StringArray::fromTokens (clip.state.getProperty ("sqmChain").toString(), " ,", ""))
        {
            auto ch = t.trim().toUpperCase();
            if (ch.isNotEmpty() && ch[0] >= 'A' && ch[0] < 'A' + maxPatterns) c.add (ch[0] - 'A');
        }
        if (c.isEmpty()) c.add (0);
        return c;
    }
    inline juce::String chainText (te::StepClip& clip)
    {
        juce::StringArray s;
        for (auto i : chain (clip)) s.add (patternName (i));
        return s.joinIntoString (" ");
    }
    inline bool loopsEditPattern (te::StepClip& clip)   { return (bool) clip.state.getProperty ("sqmLoopPattern", false); }

    /** Lays the chain (or the pattern being edited) end to end until it covers the whole clip. */
    inline void rebuildSequence (te::StepClip& clip)
    {
        auto seq = loopsEditPattern (clip) ? juce::Array<int> { editPattern (clip) } : chain (clip);
        int maxIdx = 0;
        for (auto i : seq) maxIdx = juce::jmax (maxIdx, i);
        ensurePatterns (clip, maxIdx + 1);

        // the sequence repeats by itself to fill the clip (and keeps doing so when the clip is resized)
        clip.repeatSequence = true;
        clip.removeAllVariations();
        for (auto i : seq)
            clip.insertVariation (i, -1);
    }
    inline void setChain (te::StepClip& clip, const juce::String& text)
    {
        clip.state.setProperty ("sqmChain", text.toUpperCase().trim(), um (clip));
        rebuildSequence (clip);
    }
    inline void setLoopPattern (te::StepClip& clip, bool loopEditPattern)
    {
        clip.state.setProperty ("sqmLoopPattern", loopEditPattern, nullptr);
        rebuildSequence (clip);
    }

    //==========================================================================
    // Per-step lanes stored next to Tracktion's own velocities / gates / probabilities
    enum class Lane { velocity, gate, chance, shift, repeat };
    inline const char* laneName (Lane l)
    {
        switch (l) { case Lane::velocity: return "Velocity"; case Lane::gate: return "Gate"; case Lane::chance: return "Chance"; case Lane::shift: return "Shift"; default: return "Repeat"; }
    }

    inline juce::ValueTree rowState (te::StepClip::Pattern& p, int ch)   { return p.state.getChild (ch); }

    template <typename T>
    inline juce::Array<T> readList (const juce::ValueTree& row, const char* prop)
    {
        juce::Array<T> a;
        for (auto& t : juce::StringArray::fromTokens (row.getProperty (prop).toString(), false))
            a.add (std::is_integral_v<T> ? (T) t.getIntValue() : (T) t.getDoubleValue());
        return a;
    }
    template <typename T>
    inline void writeList (te::StepClip& clip, juce::ValueTree row, const char* prop, const juce::Array<T>& a)
    {
        juce::StringArray s;
        for (auto v : a) s.add (juce::String (v));
        row.setProperty (prop, s.joinIntoString (" "), um (clip));
    }

    inline float getShift (te::StepClip::Pattern& p, int ch, int i)
    {
        auto a = readList<float> (rowState (p, ch), "sqmShifts");
        return juce::isPositiveAndBelow (i, a.size()) ? a[i] : 0.0f;
    }
    inline int getRepeat (te::StepClip::Pattern& p, int ch, int i)
    {
        auto a = readList<int> (rowState (p, ch), "sqmRepeats");
        return juce::isPositiveAndBelow (i, a.size()) ? juce::jmax (1, a[i]) : 1;
    }
    inline void setShift (te::StepClip& clip, te::StepClip::Pattern& p, int ch, int i, float v)
    {
        auto a = readList<float> (rowState (p, ch), "sqmShifts");
        while (a.size() <= i) a.add (0.0f);
        a.set (i, juce::jlimit (-0.5f, 0.5f, v));
        writeList (clip, rowState (p, ch), "sqmShifts", a);
    }
    inline void setRepeat (te::StepClip& clip, te::StepClip::Pattern& p, int ch, int i, int v)
    {
        auto a = readList<int> (rowState (p, ch), "sqmRepeats");
        while (a.size() <= i) a.add (1);
        a.set (i, juce::jlimit (1, 16, v));
        writeList (clip, rowState (p, ch), "sqmRepeats", a);
    }

    /** Lane value normalised 0..1 for drawing / dragging. */
    inline float laneValue (te::StepClip& clip, te::StepClip::Pattern& p, Lane lane, int ch, int i)
    {
        juce::ignoreUnused (clip);
        switch (lane)
        {
            case Lane::velocity: return p.getVelocity (ch, i) / 127.0f;
            case Lane::gate:     return (float) juce::jlimit (0.0, 1.0, p.getGate (ch, i) / 2.0);
            case Lane::chance:   return p.getProbability (ch, i);
            case Lane::shift:    return getShift (p, ch, i) + 0.5f;
            case Lane::repeat:   { static const int r[] = { 1, 2, 3, 4, 6, 8 }; auto v = getRepeat (p, ch, i); for (int k = 0; k < 6; ++k) if (r[k] >= v) return k / 5.0f; return 1.0f; }
        }
        return 0.0f;
    }
    inline void setLaneValue (te::StepClip& clip, te::StepClip::Pattern& p, Lane lane, int ch, int i, float v)
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        switch (lane)
        {
            case Lane::velocity: p.setVelocity (ch, i, juce::jlimit (1, 127, juce::roundToInt (v * 127.0f))); break;
            case Lane::gate:     p.setGate (ch, i, juce::jlimit (0.05, 2.0, (double) v * 2.0)); break;
            case Lane::chance:   p.setProbability (ch, i, juce::jlimit (0.0f, 1.0f, std::round (v * 20.0f) / 20.0f)); break;
            case Lane::shift:    setShift (clip, p, ch, i, std::round ((v - 0.5f) * 20.0f) / 20.0f); break;
            case Lane::repeat:   { static const int r[] = { 1, 2, 3, 4, 6, 8 }; setRepeat (clip, p, ch, i, r[juce::jlimit (0, 5, juce::roundToInt (v * 5.0f))]); break; }
        }
    }
    inline juce::String laneText (te::StepClip::Pattern& p, Lane lane, int ch, int i)
    {
        switch (lane)
        {
            case Lane::velocity: return juce::String (p.getVelocity (ch, i));
            case Lane::gate:     return juce::String (juce::roundToInt (p.getGate (ch, i) * 100.0)) + "%";
            case Lane::chance:   return juce::String (juce::roundToInt (p.getProbability (ch, i) * 100.0f)) + "%";
            case Lane::shift:    { auto s = juce::roundToInt (getShift (p, ch, i) * 100.0f); return (s > 0 ? "+" : "") + juce::String (s) + "%"; }
            case Lane::repeat:   return "x" + juce::String (getRepeat (p, ch, i));
        }
        return {};
    }

    //==========================================================================
    // Rows
    inline te::StepClip::Channel* channel (te::StepClip& clip, int ch)   { return juce::isPositiveAndBelow (ch, clip.getChannels().size()) ? clip.getChannels()[ch] : nullptr; }
    inline bool rowFlag (te::StepClip& clip, int ch, const char* prop)     { auto c = channel (clip, ch); return c != nullptr && (bool) c->state.getProperty (prop, false); }
    inline void setRowFlag (te::StepClip& clip, int ch, const char* prop, bool v)
    {
        if (auto c = channel (clip, ch)) c->state.setProperty (prop, v, um (clip));
    }
    inline bool isMuted  (te::StepClip& c, int ch)   { return rowFlag (c, ch, "sqmMute"); }
    inline bool isSoloed (te::StepClip& c, int ch)   { return rowFlag (c, ch, "sqmSolo"); }
    inline bool isLocked (te::StepClip& c, int ch)   { return rowFlag (c, ch, "sqmLock"); }

    inline int rowLength (te::StepClip::Pattern& p, int ch)
    {
        auto l = (int) rowState (p, ch).getProperty ("sqmLen", 0);
        return l <= 0 ? p.getNumNotes() : juce::jmin (l, p.getNumNotes());
    }
    inline void setRowLength (te::StepClip& clip, te::StepClip::Pattern& p, int ch, int len)
    {
        rowState (p, ch).setProperty ("sqmLen", len >= p.getNumNotes() ? 0 : juce::jmax (1, len), um (clip));
    }

    inline float swing (te::StepClip& clip)                { return (float) (double) clip.state.getProperty ("sqmSwing", 0.0); }
    inline void setSwing (te::StepClip& clip, float s)     { clip.state.setProperty ("sqmSwing", juce::jlimit (0.0f, 1.0f, s), um (clip)); }
    inline float swingMix (te::StepClip& clip, int ch)     { auto c = channel (clip, ch); return c ? (float) (double) c->state.getProperty ("sqmSwingMix", 1.0) : 1.0f; }
    inline void setSwingMix (te::StepClip& clip, int ch, float v)
    {
        if (auto c = channel (clip, ch)) c->state.setProperty ("sqmSwingMix", juce::jlimit (0.0f, 1.0f, v), um (clip));
    }

    /** Turns a step on with the row's default feel, or off (clearing its lane values). */
    inline void setStep (te::StepClip& clip, te::StepClip::Pattern& p, int ch, int i, bool on, int velocity = 100)
    {
        p.setNote (ch, i, on);
        if (on)
        {
            p.setVelocity (ch, i, juce::jlimit (1, 127, velocity));
            p.setGate (ch, i, 1.0);
            p.setProbability (ch, i, 1.0f);
        }
        if (getShift (p, ch, i) != 0.0f) setShift (clip, p, ch, i, 0.0f);
        if (getRepeat (p, ch, i) != 1)    setRepeat (clip, p, ch, i, 1);
    }

    inline void clearRow (te::StepClip& clip, te::StepClip::Pattern& p, int ch)
    {
        p.clearChannel (ch);
        auto row = rowState (p, ch);
        row.removeProperty ("sqmShifts", um (clip));
        row.removeProperty ("sqmRepeats", um (clip));
    }

    inline void fillEvery (te::StepClip& clip, te::StepClip::Pattern& p, int ch, int every, int offset = 0, int velocity = 100)
    {
        clearRow (clip, p, ch);
        for (int i = 0; i < p.getNumNotes(); ++i)
            if (((i - offset) % juce::jmax (1, every) + every) % every == 0) setStep (clip, p, ch, i, true, velocity);
    }

    /** Bjorklund-style Euclidean rhythm: 'hits' spread as evenly as possible over 'steps', then rotated. */
    inline juce::Array<bool> euclid (int hits, int steps, int rotate = 0)
    {
        juce::Array<bool> out;
        steps = juce::jmax (1, steps);
        hits = juce::jlimit (0, steps, hits);
        for (int i = 0; i < steps; ++i) out.add (((i * hits) % steps) < hits);
        juce::Array<bool> rot;
        for (int i = 0; i < steps; ++i) rot.add (out[((i - rotate) % steps + steps) % steps]);
        return rot;
    }
    inline void fillEuclid (te::StepClip& clip, te::StepClip::Pattern& p, int ch, int hits, int steps, int rotate, int velocity = 100)
    {
        clearRow (clip, p, ch);
        auto e = euclid (hits, steps, rotate);
        for (int i = 0; i < p.getNumNotes(); ++i)
            if (e[i % e.size()]) setStep (clip, p, ch, i, true, velocity);
    }

    struct StepData { bool on; int vel; double gate; float prob, shift; int rep; };
    inline std::vector<StepData> readRow (te::StepClip::Pattern& p, int ch)
    {
        std::vector<StepData> d;
        for (int i = 0; i < p.getNumNotes(); ++i)
            d.push_back ({ p.getNote (ch, i), p.getVelocity (ch, i), p.getGate (ch, i), p.getProbability (ch, i), getShift (p, ch, i), getRepeat (p, ch, i) });
        return d;
    }
    inline void writeRow (te::StepClip& clip, te::StepClip::Pattern& p, int ch, const std::vector<StepData>& d)
    {
        clearRow (clip, p, ch);
        juce::Array<int> vel; juce::Array<double> gates; juce::Array<float> probs, shifts; juce::Array<int> reps;
        juce::BigInteger bits;
        for (int i = 0; i < p.getNumNotes(); ++i)
        {
            auto s = i < (int) d.size() ? d[(size_t) i] : StepData { false, 100, 1.0, 1.0f, 0.0f, 1 };
            bits.setBit (i, s.on);
            vel.add (s.on ? s.vel : 100); gates.add (s.on ? s.gate : 1.0); probs.add (s.on ? s.prob : 1.0f);
            shifts.add (s.on ? s.shift : 0.0f); reps.add (s.on ? s.rep : 1);
        }
        p.setChannel (ch, bits);
        p.setVelocities (ch, vel); p.setGates (ch, gates); p.setProbabilities (ch, probs);
        writeList (clip, rowState (p, ch), "sqmShifts", shifts);
        writeList (clip, rowState (p, ch), "sqmRepeats", reps);
    }

    inline void rotateRow (te::StepClip& clip, te::StepClip::Pattern& p, int ch, int by)
    {
        auto d = readRow (p, ch);
        const int n = (int) d.size();
        if (n == 0) return;
        std::vector<StepData> r ((size_t) n);
        for (int i = 0; i < n; ++i) r[(size_t) (((i + by) % n + n) % n)] = d[(size_t) i];
        writeRow (clip, p, ch, r);
    }

    /** Small timing and velocity variations so programmed drums breathe (amount 0..1). */
    inline void humanise (te::StepClip& clip, te::StepClip::Pattern& p, int ch, float amount, juce::Random& rnd)
    {
        auto d = readRow (p, ch);
        for (auto& s : d)
            if (s.on)
            {
                s.vel = juce::jlimit (1, 127, s.vel + juce::roundToInt ((rnd.nextFloat() * 2.0f - 1.0f) * 18.0f * amount));
                s.shift = juce::jlimit (-0.5f, 0.5f, s.shift + (rnd.nextFloat() * 2.0f - 1.0f) * 0.08f * amount);
            }
        writeRow (clip, p, ch, d);
    }

    inline void diceRow (te::StepClip& clip, te::StepClip::Pattern& p, int ch, float density, juce::Random& rnd)
    {
        clearRow (clip, p, ch);
        for (int i = 0; i < p.getNumNotes(); ++i)
        {
            float bias = (i % 4 == 0) ? 1.6f : (i % 2 == 0 ? 1.0f : 0.6f);
            if (rnd.nextFloat() < density * bias) setStep (clip, p, ch, i, true, 80 + rnd.nextInt (40));
        }
    }

    /** Pattern length in steps; every row keeps its data. */
    inline void setNumSteps (te::StepClip& clip, int steps)
    {
        auto p = pattern (clip);
        p.setNumNotes (juce::jlimit (1, 128, steps));
        rebuildSequence (clip);
    }
    /** Doubles the pattern by copying it after itself. */
    inline void doublePattern (te::StepClip& clip)
    {
        auto p = pattern (clip);
        const int n = p.getNumNotes();
        if (n * 2 > 128) return;
        std::vector<std::vector<StepData>> rows;
        for (int ch = 0; ch < clip.getChannels().size(); ++ch) rows.push_back (readRow (p, ch));
        p.setNumNotes (n * 2);
        for (int ch = 0; ch < (int) rows.size(); ++ch)
        {
            auto d = rows[(size_t) ch];
            auto copy = d;
            d.insert (d.end(), copy.begin(), copy.end());
            writeRow (clip, p, ch, d);
        }
        rebuildSequence (clip);
    }

    /** Step resolution in beats: 1/8 = 0.5, 1/16 = 0.25, 1/32 = 0.125, 1/8T = 1/3, 1/16T = 1/6. */
    inline void setResolution (te::StepClip& clip, double beatsPerStep)
    {
        auto p = pattern (clip);
        p.setNoteLength (te::BeatDuration::fromBeats (beatsPerStep));
        rebuildSequence (clip);
    }

    //==========================================================================
    // Clipboard (rows and whole patterns)
    struct Clipboard { std::vector<std::vector<StepData>> rows; int steps = 0; double noteLen = 0.25; bool wholePattern = false; };
    inline Clipboard& clipboard()   { static Clipboard c; return c; }

    inline void copyRow (te::StepClip::Pattern& p, int ch)
    {
        auto& c = clipboard(); c.rows = { readRow (p, ch) }; c.steps = p.getNumNotes(); c.wholePattern = false;
    }
    inline void copyPattern (te::StepClip& clip)
    {
        auto p = pattern (clip);
        auto& c = clipboard(); c.rows.clear(); c.steps = p.getNumNotes(); c.noteLen = p.getNoteLength().inBeats(); c.wholePattern = true;
        for (int ch = 0; ch < clip.getChannels().size(); ++ch) c.rows.push_back (readRow (p, ch));
    }
    inline bool paste (te::StepClip& clip, int toRow)
    {
        auto& c = clipboard();
        if (c.rows.empty()) return false;
        auto p = pattern (clip);
        if (c.wholePattern)
        {
            p.setNumNotes (c.steps);
            p.setNoteLength (te::BeatDuration::fromBeats (c.noteLen));
            for (int ch = 0; ch < (int) c.rows.size() && ch < clip.getChannels().size(); ++ch) writeRow (clip, p, ch, c.rows[(size_t) ch]);
            rebuildSequence (clip);
        }
        else if (juce::isPositiveAndBelow (toRow, clip.getChannels().size()))
            writeRow (clip, p, toRow, c.rows.front());
        return true;
    }

    //==========================================================================
    // Beat Starter: genre grooves. Row strings use 16th steps:
    //   . rest   x hit   X accent   g ghost (soft)   ? 50% chance   2-8 a roll of that many hits
    struct RowIdeas { int channel; std::vector<const char*> options; };
    struct Genre
    {
        const char* name; int bpmLo, bpmHi; float swing; int steps; int kit;
        std::vector<RowIdeas> rows;
        const char* feel;
    };

    // channel indices (same order in every kit)
    enum Ch { kick = 0, snare, rim, clap, hat, openHat, lowTom, highTom, ride, crash, perc, sub };

    inline const std::vector<Genre>& genres()
    {
        static const std::vector<Genre> g {
            { "Trap", 130, 150, 0.0f, 16, 2, {
                { kick, { "x......x..x.....", "x.....x...x..x..", "x..x......x.....", "x......x.x....x.", "x.........xx...." } },
                { snare, { "........x......." } }, { clap, { "........x.......", "........x......g" } },
                { hat, { "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.3.x.", "x.xxx.x.x.x.4.x.", "x.x.x.3.x.x.x.6.", "xxxxx.x.x.xxx.3.", "x.x.x.x.3...x.4." } },
                { openHat, { "................", "..............x.", "......x........." } },
                { perc, { "................", "...x......x.....", ".......?.......?" } },
                { sub, { "x......x..x.....", "x.....x...x..x..", "x..x......x....." } } }, "Half-time snare on 3, rolling hats, 808 on the kick" },
            { "Drill", 138, 146, 0.0f, 16, 2, {
                { kick, { "x.........x.....", "x......x..x.....", "x.....x.......x." } },
                { snare, { "........x....x..", "........x.....x.", "....x.......x..x" } },
                { hat, { "x..x..x.x..x..x.", "x..x..x.x.3x..x.", "x.x..x.x..x.x..x", "x..x..3.x..x.4.." } },
                { perc, { "................", "..x.......x....." } },
                { sub, { "x.........x.....", "x......x..x....." } } }, "Sliding 808s, syncopated snares, 3-3-2 hats" },
            { "Boom Bap", 84, 96, 0.35f, 16, 3, {
                { kick, { "x.......x.x.....", "x......xx.......", "x.x.......x..x..", "x......x..x....." } },
                { snare, { "....x.......x...", "....x..g....x..." } },
                { hat, { "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.xx", "xgxgxgxgxgxgxgxg" } },
                { openHat, { "................", "..............x." } },
                { rim, { "................", "...........g...." } } }, "Swung 16ths, hard snare on 2 and 4" },
            { "Lo-Fi", 70, 90, 0.45f, 16, 3, {
                { kick, { "x......x..x.....", "x.......x.x.....", "x..x......x....." } },
                { snare, { "....x.......x...", "....x..g....x..g" } },
                { rim, { "....x.......x...", "................" } },
                { hat, { "x.x.x.x.x.x.x.x.", "x?x?x?x?x?x?x?x?", "x.xgx.xgx.xgx.xg" } },
                { perc, { "................", "......g.......g." } } }, "Lazy swing, ghost notes, soft hats" },
            { "R&B", 60, 78, 0.25f, 16, 0, {
                { kick, { "x......x..x.....", "x.....x...x..x..", "x......x.x......" } },
                { snare, { "....x.......x...", "....x.......x..g" } },
                { clap, { "....x.......x...", "................" } },
                { rim, { "......x.......x.", "..g.....g.....g." } },
                { hat, { "xgxgxgxgxgxgxgxg", "x.x.x.x.x.x.3.x.", "xxxxxxxxxxxxxxxx" } },
                { sub, { "x......x..x.....", "x.....x...x..x.." } } }, "Smooth half-time, ghosted 16th hats, rim clicks" },
            { "Afrobeats", 98, 112, 0.1f, 16, 1, {
                { kick, { "x...x...x...x...", "x..x..x...x.....", "x.....x...x....." } },
                { snare, { "....x.......x...", "................" } }, { clap, { "....x.......x...", "....x.......x..." } },
                { rim, { "..x..x....x..x..", "...x..x....x..x.", "x..x..x...x..x.." } },
                { hat, { "xgxgxgxgxgxgxgxg", "xxxxxxxxxxxxxxxx" } },
                { perc, { "..x...x...x.x...", "...x..x...x..x..", ".x..x..x.x..x..." } },
                { lowTom, { "................", "......x.......x." } } }, "Tresillo rims, shaker 16ths, bouncing percussion" },
            { "Amapiano", 108, 116, 0.15f, 16, 1, {
                { kick, { "x...x...x...x...", "x.......x......." } },
                { clap, { "....x.......x...", "........x......." } },
                { rim, { "...x..x....x..x.", "..x...x...x..x.." } },
                { hat, { "xgxgxgxgxgxgxgxg" } }, { openHat, { "..x...x...x...x." } },
                { perc, { ".x...x...x.x..x.", "...x.x...x...x.x" } },
                { sub, { "x..x..x...x..x..", "x...x.x..x..x...", "..x..x..x...x..." } } }, "Four on the floor, log-drum bass, shaker 16ths" },
            { "Reggaeton", 88, 100, 0.0f, 16, 1, {
                { kick, { "x...x...x...x..." } },
                { snare, { "...x..x....x..x.", "...x..x.x..x..x." } },
                { hat, { "x.x.x.x.x.x.x.x.", "x.xxx.xxx.xxx.xx" } },
                { perc, { "................", "..x.......x....." } } }, "The dembow: 3-3-2 snare over a straight kick" },
            { "Jersey Club", 135, 145, 0.0f, 16, 1, {
                { kick, { "x...x...x.x...x.", "x...x...x.x.x...", "x..x..x.x.x....." } },
                { clap, { "....x.......x...", "....x.......x.x." } },
                { hat, { "x.x.x.x.x.x.x.x." } },
                { perc, { "......x.......x.", "..x...x...x...x." } } }, "Bouncing triplet-feel kicks, claps on 2 and 4" },
            { "House", 120, 128, 0.1f, 16, 1, {
                { kick, { "x...x...x...x..." } }, { clap, { "....x.......x..." } },
                { hat, { "..x...x...x...x.", "xgxgxgxgxgxgxgxg" } }, { openHat, { "..x...x...x...x." } },
                { perc, { "................", "...x......x....." } } }, "Four on the floor, offbeat open hats" },
            { "Techno", 126, 136, 0.0f, 16, 1, {
                { kick, { "x...x...x...x..." } }, { rim, { "...x..x...x..x..", "..x...x...x..x.." } },
                { hat, { "..x...x...x...x.", "x.x.x.x.x.x.x.x." } }, { openHat, { "..x...x...x...x." } },
                { ride, { "x.x.x.x.x.x.x.x.", "................" } }, { clap, { "....x.......x...", "................" } } }, "Driving kick, offbeat hats, rim syncopation" },
            { "Phonk", 130, 160, 0.0f, 16, 0, {
                { kick, { "x.....x.x.......", "x......x..x....." } }, { clap, { "........x......." } },
                { hat, { "x.x.x.x.x.x.3.x.", "xxx.x.x.xxx.x.x." } },
                { perc, { "x..x..x...x..x..", "x.x..x..x.x..x..", "x..x..x.x..x..x." } },
                { sub, { "x.....x.x.......", "x......x..x....." } } }, "Memphis cowbell melody over half-time drums" },
            { "Pop", 100, 124, 0.0f, 16, 1, {
                { kick, { "x.......x.x.....", "x...x...x...x...", "x......x..x....." } },
                { snare, { "....x.......x..." } }, { clap, { "....x.......x...", "................" } },
                { hat, { "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx" } }, { openHat, { "................", "..............x." } },
                { crash, { "x..............." } } }, "Clean backbeat built for a hook" },
            { "Drum & Bass", 170, 176, 0.0f, 16, 1, {
                { kick, { "x.........x.....", "x.........x..x..", "x.x.......x....." } },
                { snare, { "....x.......x...", "....x.......x..g" } },
                { hat, { "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx" } }, { ride, { "................", "x.x.x.x.x.x.x.x." } } }, "Two-step break at 174" },
            { "Dancehall", 90, 104, 0.05f, 16, 0, {
                { kick, { "x.....x.x.......", "x..x....x..x...." } },
                { rim, { "...x..x....x..x.", "...x..x....x...." } }, { snare, { "........x......." } },
                { hat, { "x.x.x.x.x.x.x.x." } }, { perc, { "..x.......x.....", "................" } } }, "One-drop and riddim syncopation" },
            { "Rock", 100, 140, 0.0f, 16, 4, {
                { kick, { "x.....x.x.......", "x.......x.x.....", "x.x.....x......." } },
                { snare, { "....x.......x...", "....x.......x.gx" } },
                { hat, { "x.x.x.x.x.x.x.x." } }, { crash, { "x..............." } },
                { lowTom, { "................", "............xx.." } }, { highTom, { "................", "............xx.." } } }, "Straight eighths and a big backbeat" },
            { "Funk / Neo-Soul", 88, 104, 0.3f, 16, 4, {
                { kick, { "x..x...x..x..x..", "x.x....x..x....." } },
                { snare, { "....x..g.g..x..g", "....x.g.....x.g." } },
                { hat, { "xgxgxgxgxgxgxgxg", "x.x.x.x.x.x.x.x." } }, { openHat, { "......x.......x." } },
                { rim, { "................", "..g.......g....." } } }, "Ghost-note snares and a pocket that swings" } };
        return g;
    }

    inline int genreIndex (const juce::String& name)
    {
        auto& g = genres();
        for (int i = 0; i < (int) g.size(); ++i) if (name.trim().equalsIgnoreCase (g[(size_t) i].name)) return i;
        for (int i = 0; i < (int) g.size(); ++i) if (juce::String (g[(size_t) i].name).containsIgnoreCase (name.trim()) && name.trim().isNotEmpty()) return i;
        if (name.containsIgnoreCase ("dembow")) return genreIndex ("Reggaeton");
        if (name.containsIgnoreCase ("dnb") || name.containsIgnoreCase ("jungle")) return genreIndex ("Drum & Bass");
        if (name.containsIgnoreCase ("hip")) return genreIndex ("Boom Bap");
        if (name.containsIgnoreCase ("soul") || name.containsIgnoreCase ("funk")) return genreIndex ("Funk / Neo-Soul");
        if (name.containsIgnoreCase ("afro")) return genreIndex ("Afrobeats");
        return -1;
    }

    /** Writes one row from a groove string. */
    inline void writeGrooveString (te::StepClip& clip, te::StepClip::Pattern& p, int ch, const juce::String& s, juce::Random& rnd, int baseVel = 104)
    {
        clearRow (clip, p, ch);
        const int n = p.getNumNotes();
        for (int i = 0; i < n; ++i)
        {
            auto c = s.isEmpty() ? '.' : s[i % s.length()];
            if (c == '.' || c == '-') continue;
            int vel = baseVel;
            float prob = 1.0f;
            int rep = 1;
            if (c == 'X') vel = 127;
            else if (c == 'g') vel = 58;
            else if (c == '?') prob = 0.5f;
            else if (c >= '2' && c <= '8') { rep = c - '0'; vel = 92; }
            setStep (clip, p, ch, i, true, juce::jlimit (1, 127, vel + rnd.nextInt (9) - 4));
            if (prob < 1.0f) p.setProbability (ch, i, prob);
            if (rep > 1) setRepeat (clip, p, ch, i, rep);
        }
    }

    /** Rolls a genre groove into the edited pattern. Locked rows are kept; rowOnly >= 0 re-rolls just that row. */
    inline juce::String starter (te::StepClip& clip, int genre, juce::Random& rnd, int rowOnly = -1, bool setTempo = false)
    {
        auto& gs = genres();
        if (! juce::isPositiveAndBelow (genre, (int) gs.size())) return {};
        auto& g = gs[(size_t) genre];
        auto p = pattern (clip);
        if (rowOnly < 0)
        {
            p.setNoteLength (te::BeatDuration::fromBeats (0.25));
            if (p.getNumNotes() % 16 != 0) p.setNumNotes (16);
        }
        juce::StringArray written;
        for (int ch = 0; ch < clip.getChannels().size(); ++ch)
        {
            if (rowOnly >= 0 && ch != rowOnly) continue;
            if (rowOnly < 0 && isLocked (clip, ch)) continue;
            const RowIdeas* idea = nullptr;
            for (auto& r : g.rows) if (r.channel == ch) idea = &r;
            if (idea == nullptr) { if (rowOnly < 0) clearRow (clip, p, ch); continue; }
            auto opt = juce::String (idea->options[(size_t) rnd.nextInt ((int) idea->options.size())]);
            // repeat the bar, occasionally varying the last bar with another option for movement
            juce::String full;
            for (int bar = 0; bar < p.getNumNotes() / 16; ++bar)
                full << ((bar > 0 && bar == p.getNumNotes() / 16 - 1 && idea->options.size() > 1 && rnd.nextFloat() < 0.5f)
                            ? juce::String (idea->options[(size_t) rnd.nextInt ((int) idea->options.size())]) : opt);
            writeGrooveString (clip, p, ch, full.isEmpty() ? opt : full, rnd, ch == hat ? 92 : 104);
            written.add (clip.getChannels()[ch]->name.get());
        }
        if (rowOnly < 0)
        {
            setSwing (clip, g.swing);
            clip.state.setProperty ("sqmGenre", g.name, nullptr);
            if (setTempo)
                if (auto ts = clip.edit.tempoSequence.getTempo (0))
                    ts->setBpm (g.bpmLo + rnd.nextInt (juce::jmax (1, g.bpmHi - g.bpmLo + 1)));
        }
        rebuildSequence (clip);
        return g.name + juce::String (": ") + written.joinIntoString (", ");
    }

    //==========================================================================
    /** The pattern chain as an editable MIDI clip on the same track (the step clip is removed). */
    inline te::MidiClip* sendToPianoRoll (te::StepClip& clip)
    {
        auto* track = dynamic_cast<te::AudioTrack*> (clip.getTrack());
        if (track == nullptr) return nullptr;
        auto seq = clip.generateMidiSequence (te::MidiList::TimeBase::beats);   // beats from the edit start
        auto range = clip.getEditTimeRange();
        auto startBeat = clip.edit.tempoSequence.toBeats (range.getStart()).inBeats();
        auto name = clip.getName();
        auto mc = dynamic_cast<te::MidiClip*> (track->insertNewClip (te::TrackItem::Type::midi, name + " (MIDI)", range, nullptr));
        if (mc == nullptr) return nullptr;
        seq.updateMatchedPairs();
        auto& list = mc->getSequence();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* e = seq.getEventPointer (i);
            if (! e->message.isNoteOn() || e->noteOffObject == nullptr) continue;
            auto s = e->message.getTimeStamp() - startBeat;
            auto l = juce::jmax (0.01, e->noteOffObject->message.getTimeStamp() - e->message.getTimeStamp());
            list.addNote (e->message.getNoteNumber(), te::BeatPosition::fromBeats (s), te::BeatDuration::fromBeats (l),
                          e->message.getVelocity(), 0, &clip.edit.getUndoManager());
        }
        clip.removeFromParent();
        return mc;
    }
}
