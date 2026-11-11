#pragma once
// Squelchmoth 1.13 - the Score's model: gathers MIDI tracks as notation parts (with a link from every drawn note head
// back to its MIDI note, so edits in the score are edits to the MIDI), the session's meters and key, the display
// options kept with the session, MusicXML export / import, and the edits the Score window and Claude share.

#include "Session.h"
#include "MusicXml.h"

namespace ScoreModel
{
    struct NoteRef { te::MidiClip* clip = nullptr; te::MidiNote* note = nullptr; };

    struct Part
    {
        te::AudioTrack* track = nullptr;
        juce::String name;
        std::vector<Notation::InNote> notes;
        std::vector<NoteRef> refs;          // refs[i] is notes[i]'s MIDI note
        Notation::Score score;
    };

    //==========================================================================
    // options (kept in the session)
    inline juce::ValueTree optionsTree (Session& s)
    {
        auto v = s.edit->state.getChildWithName ("SQM_SCORE");
        if (! v.isValid()) { v = juce::ValueTree ("SQM_SCORE"); s.edit->state.appendChild (v, nullptr); }
        return v;
    }
    inline Notation::Options options (Session& s)
    {
        Notation::Options o;
        if (s.edit == nullptr) return o;
        auto v = optionsTree (s);
        o.grid = juce::jlimit (4, 32, (int) v.getProperty ("grid", 16));
        o.triplets = (bool) v.getProperty ("triplets", true);
        o.staves = (Notation::StaffMode) juce::jlimit (0, 3, (int) v.getProperty ("staves", 0));
        o.spellPreference = juce::jlimit (-1, 1, (int) v.getProperty ("spelling", 0));
        return o;
    }
    inline void setOption (Session& s, const juce::Identifier& id, const juce::var& value)   { optionsTree (s).setProperty (id, value, nullptr); }

    /** The session's meters as notation bars (from the tempo track's time signatures). */
    inline std::vector<Notation::Meter> meters (Session& s)
    {
        std::vector<Notation::Meter> out;
        auto& ts = s.edit->tempoSequence;
        for (int i = 0; i < ts.getNumTimeSigs(); ++i)
        {
            auto* sig = ts.getTimeSig (i);
            const int bar = ts.toBarsAndBeats (ts.toTime (sig->getStartBeat())).bars;
            out.push_back ({ juce::jmax (0, bar), juce::jmax (1, sig->numerator.get()), juce::jmax (1, sig->denominator.get()) });
        }
        if (out.empty()) out.push_back ({ 0, 4, 4 });
        return out;
    }

    /** The session key: the Score's own setting, else the key the tuners / Moth Key set, else C major. */
    inline Notation::Key key (Session& s)
    {
        Notation::Key k;
        auto v = optionsTree (s);
        if (v.hasProperty ("fifths")) { k.fifths = juce::jlimit (-7, 7, (int) v["fifths"]); k.minor = (bool) v.getProperty ("minor", false); return k; }
        auto& es = s.edit->state;
        if (es.hasProperty ("sqmKeyRoot"))
        {
            const int root = ((int) es["sqmKeyRoot"] % 12 + 12) % 12;
            const bool minor = (int) es.getProperty ("sqmKeyScale", 1) == 2;
            const int major = minor ? (root + 3) % 12 : root;
            static const int fifthsOf[12] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };
            k.fifths = fifthsOf[major]; k.minor = minor;
        }
        return k;
    }

    inline juce::String keyName (Notation::Key k)
    {
        static const char* majors[] = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
        static const char* minors[] = { "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#" };
        return juce::String ((k.minor ? minors : majors)[juce::jlimit (0, 14, k.fifths + 7)]) + (k.minor ? " minor" : " major");
    }
    /** "Eb major", "F# minor", "Bb" -> key, false if not a key. */
    inline bool parseKey (const juce::String& text, Notation::Key& out)
    {
        auto t = text.trim(); const bool minor = t.containsIgnoreCase ("minor") || (t.length() >= 2 && t.endsWithChar ('m') && ! t.containsIgnoreCase ("major"));
        auto root = t.upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("m", false, false);
        if (root.isEmpty()) return false;
        for (int f = -7; f <= 7; ++f)
        {
            Notation::Key k { f, minor };
            if (keyName (k).upToFirstOccurrenceOf (" ", false, false).equalsIgnoreCase (root)) { out = k; return true; }
        }
        return false;
    }

    //==========================================================================
    inline bool hasMidi (te::AudioTrack& t)   { for (auto c : t.getClips()) if (dynamic_cast<te::MidiClip*> (c) != nullptr) return true; return false; }

    /** One part per track: every note of every MIDI clip in edit beats (clip offset and end respected). */
    inline Part gather (Session& s, te::AudioTrack& t)
    {
        Part p; p.track = &t; p.name = t.getName();
        auto& ts = s.edit->tempoSequence;
        for (auto c : t.getClips())
            if (auto mc = dynamic_cast<te::MidiClip*> (c))
            {
                const double clipStart = ts.toBeats (mc->getPosition().getStart()).inBeats();
                const double clipEnd = ts.toBeats (mc->getPosition().getEnd()).inBeats();
                const double offset = mc->getOffsetInBeats().inBeats();
                for (auto n : mc->getSequence().getNotes())
                {
                    const double st = clipStart + n->getStartBeat().inBeats() - offset;
                    double en = st + n->getLengthBeats().inBeats();
                    if (st < clipStart - 1.0e-6 || st >= clipEnd - 1.0e-6) continue;
                    en = juce::jmin (en, clipEnd);
                    p.notes.push_back ({ st, en - st, n->getNoteNumber(), n->getVelocity() });
                    p.refs.push_back ({ mc, n });
                }
            }
        return p;
    }

    inline juce::Array<te::AudioTrack*> midiTracks (Session& s)
    {
        juce::Array<te::AudioTrack*> out;
        for (auto t : s.getTracks()) if (hasMidi (*t)) out.add (t);
        return out;
    }

    /** Builds every part's score with the session's options; all parts get the same number of bars. */
    inline std::vector<Part> build (Session& s, const juce::Array<te::AudioTrack*>& tracks)
    {
        std::vector<Part> parts;
        auto opt = options (s); auto mt = meters (s); auto k = key (s);
        double endBeat = 0;
        for (auto t : tracks) { parts.push_back (gather (s, *t)); for (auto& n : parts.back().notes) endBeat = juce::jmax (endBeat, n.start + n.length); }
        // bars to cover the longest part
        int bars = 0; double pos = 0;
        auto meterAt = [&] (int bar) { Notation::Meter m; for (auto& mm : mt) if (mm.bar <= bar) m = mm; return m; };
        while (pos < endBeat - 1.0e-6 || bars == 0) { auto m = meterAt (bars); pos += m.num * 4.0 / m.den; ++bars; }
        for (auto& p : parts)
        {
            auto o = opt;
            auto pv = p.track->state.getProperty ("sqmScoreStaves");
            if (! pv.isVoid()) o.staves = (Notation::StaffMode) juce::jlimit (0, 3, (int) pv);
            p.score = Notation::build (p.notes, mt, k, o, bars);
        }
        return parts;
    }

    //==========================================================================
    inline juce::String musicXml (Session& s, const std::vector<Part>& parts, const juce::String& title, const juce::String& composer)
    {
        std::vector<MusicXml::PartOut> outs;
        for (auto& p : parts) outs.push_back ({ p.name, p.score });
        const double bpm = s.edit->tempoSequence.getTempo (0) != nullptr ? s.edit->tempoSequence.getTempo (0)->getBpm() : 120.0;
        return MusicXml::exportScore (outs, title, composer, bpm);
    }

    /** Imports MusicXML: one new instrument-ready MIDI track and clip per part, at bar 1. Returns the tracks made. */
    inline juce::Array<te::AudioTrack*> importMusicXml (Session& s, const juce::File& f, juce::String& err)
    {
        juce::Array<te::AudioTrack*> made;
        auto parts = MusicXml::importScore (f.loadFileAsString(), err);
        if (parts.empty()) return made;
        auto& ts = s.edit->tempoSequence;
        for (auto& p : parts)
        {
            double end = 4; for (auto& n : p.notes) end = juce::jmax (end, n.start + n.length);
            auto t = s.addTrack ("instrument", p.name.isNotEmpty() ? p.name : juce::String ("Score part"), "4osc");
            if (t == nullptr) { err = "Couldn't add a track"; continue; }
            const auto st = ts.toTime (te::BeatPosition()), en = ts.toTime (te::BeatPosition::fromBeats (std::ceil (end / 4.0) * 4.0));
            auto clip = t->insertMIDIClip (p.name.isNotEmpty() ? p.name : juce::String ("Score"), { st, en }, nullptr);
            if (clip == nullptr) continue;
            for (auto& n : p.notes)
                clip->getSequence().addNote (n.pitch, te::BeatPosition::fromBeats (n.start), te::BeatDuration::fromBeats (n.length), juce::jlimit (1, 127, n.velocity), 0, &s.edit->getUndoManager());
            made.add (t);
        }
        if (! parts.empty() && parts[0].tempo > 0) if (auto tp = ts.getTempo (0)) tp->setBpm (parts[0].tempo);
        return made;
    }

    //==========================================================================
    // edits (in edit beats; the clip that holds the position is used, or a new clip is made)
    inline te::MidiClip* clipAt (Session& s, te::AudioTrack& t, double beat)
    {
        auto& ts = s.edit->tempoSequence;
        for (auto c : t.getClips())
            if (auto mc = dynamic_cast<te::MidiClip*> (c))
                if (beat >= ts.toBeats (mc->getPosition().getStart()).inBeats() - 1.0e-6 && beat < ts.toBeats (mc->getPosition().getEnd()).inBeats() - 1.0e-6) return mc;
        return nullptr;
    }

    /** Adds a note at an edit beat; makes (or extends to) a clip of whole bars if none covers it. */
    inline te::MidiNote* addNote (Session& s, te::AudioTrack& t, double beat, double length, int pitch, int velocity = 96)
    {
        auto& ts = s.edit->tempoSequence;
        auto mc = clipAt (s, t, beat);
        if (mc == nullptr)
        {
            const int bar = ts.toBarsAndBeats (ts.toTime (te::BeatPosition::fromBeats (beat))).bars;
            const auto st = ts.toTime (te::tempo::BarsAndBeats { bar, te::BeatDuration(), 0 });
            const auto en = ts.toTime (te::tempo::BarsAndBeats { bar + 1, te::BeatDuration(), 0 });
            auto clip = t.insertMIDIClip ("Score", { st, juce::jmax (en, ts.toTime (te::BeatPosition::fromBeats (beat + length))) }, nullptr);
            mc = clip.get();
        }
        if (mc == nullptr) return nullptr;
        const double rel = beat - ts.toBeats (mc->getPosition().getStart()).inBeats() + mc->getOffsetInBeats().inBeats();
        // grow the clip if the note runs past it
        const double clipEnd = ts.toBeats (mc->getPosition().getEnd()).inBeats();
        if (beat + length > clipEnd + 1.0e-6) mc->setEnd (ts.toTime (te::BeatPosition::fromBeats (beat + length)), true);
        return mc->getSequence().addNote (juce::jlimit (0, 127, pitch), te::BeatPosition::fromBeats (rel), te::BeatDuration::fromBeats (length), juce::jlimit (1, 127, velocity), 0, &s.edit->getUndoManager());
    }

    inline void transpose (Session& s, const std::vector<NoteRef>& refs, int semis)
    {
        for (auto& r : refs) if (r.note != nullptr) r.note->setNoteNumber (juce::jlimit (0, 127, r.note->getNoteNumber() + semis), &s.edit->getUndoManager());
    }
    inline void moveBy (Session& s, const std::vector<NoteRef>& refs, double beats)
    {
        for (auto& r : refs) if (r.note != nullptr) r.note->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (0.0, r.note->getStartBeat().inBeats() + beats)), r.note->getLengthBeats(), &s.edit->getUndoManager());
    }
    inline void setLength (Session& s, const std::vector<NoteRef>& refs, double beats)
    {
        for (auto& r : refs) if (r.note != nullptr) r.note->setStartAndLength (r.note->getStartBeat(), te::BeatDuration::fromBeats (juce::jmax (0.01, beats)), &s.edit->getUndoManager());
    }
    inline void remove (Session& s, const std::vector<NoteRef>& refs)
    {
        for (auto& r : refs) if (r.clip != nullptr && r.note != nullptr) r.clip->getSequence().removeNote (*r.note, &s.edit->getUndoManager());
    }
}
