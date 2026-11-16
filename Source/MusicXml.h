#pragma once
// Squelchmoth 1.13 - MusicXML 4.0 (partwise) export from the notation engine, and import back to MIDI notes, so a
// part goes to and from any notation program or notation app.

#include "Notation.h"

namespace MusicXml
{
    struct PartOut { juce::String name; Notation::Score score; };
    struct PartIn  { juce::String name; std::vector<Notation::InNote> notes; std::vector<Notation::Meter> meters; Notation::Key key; double tempo = 0; };

    static constexpr int divisions = 48;   // per quarter: straight 1/32 = 6, triplet 16th = 8

    inline juce::String esc (const juce::String& s)   { return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;").replace ("\"", "&quot;"); }

    inline juce::String exportScore (const std::vector<PartOut>& parts, const juce::String& title, const juce::String& composer, double tempoBpm)
    {
        juce::String x;
        x << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
          << "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">\n"
          << "<score-partwise version=\"4.0\">\n"
          << "  <work><work-title>" << esc (title) << "</work-title></work>\n"
          << "  <identification>\n";
        if (composer.isNotEmpty()) x << "    <creator type=\"composer\">" << esc (composer) << "</creator>\n";
        x << "    <encoding><software>Squelchmoth</software><encoding-date>" << juce::Time::getCurrentTime().formatted ("%Y-%m-%d") << "</encoding-date></encoding>\n"
          << "  </identification>\n  <part-list>\n";
        for (size_t i = 0; i < parts.size(); ++i)
            x << "    <score-part id=\"P" << (int) (i + 1) << "\"><part-name>" << esc (parts[i].name) << "</part-name></score-part>\n";
        x << "  </part-list>\n";
        for (size_t pi = 0; pi < parts.size(); ++pi)
        {
            auto& sc = parts[pi].score;
            x << "  <part id=\"P" << (int) (pi + 1) << "\">\n";
            for (auto& m : sc.measures)
            {
                x << "    <measure number=\"" << (m.index + 1) << "\">\n";
                if (m.index == 0 || m.showMeter || m.showKey)
                {
                    x << "      <attributes>\n";
                    if (m.index == 0) x << "        <divisions>" << divisions << "</divisions>\n";
                    if (m.showKey) x << "        <key><fifths>" << sc.key.fifths << "</fifths><mode>" << (sc.key.minor ? "minor" : "major") << "</mode></key>\n";
                    if (m.showMeter) x << "        <time><beats>" << m.num << "</beats><beat-type>" << m.den << "</beat-type></time>\n";
                    if (m.index == 0)
                    {
                        if (sc.numStaves > 1) x << "        <staves>" << sc.numStaves << "</staves>\n";
                        for (int s = 0; s < sc.numStaves; ++s)
                        {
                            const bool bass = sc.clefs[(size_t) s] == Notation::Clef::bass;
                            x << "        <clef" << (sc.numStaves > 1 ? " number=\"" + juce::String (s + 1) + "\"" : juce::String()) << "><sign>" << (bass ? "F" : "G") << "</sign><line>" << (bass ? 4 : 2) << "</line></clef>\n";
                        }
                    }
                    x << "      </attributes>\n";
                }
                if (m.index == 0 && tempoBpm > 0)
                    x << "      <direction placement=\"above\"><direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>" << juce::roundToInt (tempoBpm)
                      << "</per-minute></metronome></direction-type><sound tempo=\"" << tempoBpm << "\"/></direction>\n";
                const int barDiv = juce::roundToInt (m.lengthBeats * divisions);
                for (size_t s = 0; s < m.staves.size(); ++s)
                {
                    if (s > 0) x << "      <backup><duration>" << barDiv << "</duration></backup>\n";
                    auto& evs = m.staves[s].events;
                    for (size_t ei = 0; ei < evs.size(); ++ei)
                    {
                        auto& e = evs[ei];
                        const int dur = juce::roundToInt (e.length * divisions);
                        static const char* types[] = { "", "whole", "half", "", "quarter", "", "", "", "eighth" };
                        const juce::String type = e.value <= 8 ? juce::String (types[e.value]) : e.value == 16 ? juce::String ("16th") : juce::String ("32nd");
                        auto common = [&]
                        {
                            juce::String c;
                            c << "<voice>" << (int) (s * 4 + 1) << "</voice>";
                            if (! (e.rest && e.value == 1 && e.length >= m.lengthBeats - 1.0e-6 && evs.size() == 1)) c << "<type>" << type << "</type>";
                            for (int d = 0; d < e.dots; ++d) c << "<dot/>";
                            return c;
                        };
                        if (e.rest)
                        {
                            const bool whole = e.value == 1 && evs.size() == 1;
                            x << "      <note><rest" << (whole ? " measure=\"yes\"" : "") << "/><duration>" << dur << "</duration>" << common();
                            if (e.triplet) x << "<time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>";
                            if (sc.numStaves > 1) x << "<staff>" << (int) (s + 1) << "</staff>";
                            x << "</note>\n";
                            continue;
                        }
                        for (size_t hi = 0; hi < e.heads.size(); ++hi)
                        {
                            auto& h = e.heads[hi];
                            x << "      <note>" << (hi > 0 ? "<chord/>" : "") << "<pitch><step>" << juce::String::charToString ((juce::juce_wchar) Notation::stepNames[h.step]) << "</step>";
                            if (h.alter != 0) x << "<alter>" << h.alter << "</alter>";
                            x << "<octave>" << h.octave << "</octave></pitch><duration>" << dur << "</duration>";
                            if (h.tieFromPrev) x << "<tie type=\"stop\"/>";
                            if (h.tieToNext) x << "<tie type=\"start\"/>";
                            x << common();
                            if (h.showAccidental)
                            {
                                static const char* acc[] = { "flat-flat", "flat", "natural", "sharp", "double-sharp" };
                                x << "<accidental>" << acc[juce::jlimit (0, 4, h.shownAccidental + 2)] << "</accidental>";
                            }
                            if (e.triplet) x << "<time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification>";
                            x << "<stem>" << (e.stemUp ? "up" : "down") << "</stem>";
                            if (sc.numStaves > 1) x << "<staff>" << (int) (s + 1) << "</staff>";
                            if (hi == 0 && e.beamGroup >= 0)
                            {
                                const bool first = ei == 0 || evs[ei - 1].beamGroup != e.beamGroup;
                                const bool last = ei + 1 >= evs.size() || evs[ei + 1].beamGroup != e.beamGroup;
                                x << "<beam number=\"1\">" << (first ? "begin" : last ? "end" : "continue") << "</beam>";
                            }
                            if (h.tieFromPrev || h.tieToNext || (e.triplet && hi == 0))
                            {
                                x << "<notations>";
                                if (h.tieFromPrev) x << "<tied type=\"stop\"/>";
                                if (h.tieToNext) x << "<tied type=\"start\"/>";
                                if (e.triplet && hi == 0)
                                {
                                    // bracket the triplet group: starts on a beat, ends where the beat ends
                                    const double beatPos = e.start - std::floor (e.start + 1.0e-6);
                                    if (beatPos < 1.0e-4) x << "<tuplet type=\"start\" bracket=\"no\"/>";
                                    if (std::abs (e.start + e.length - std::round (e.start + e.length)) < 1.0e-4) x << "<tuplet type=\"stop\"/>";
                                }
                                x << "</notations>";
                            }
                            x << "</note>\n";
                        }
                    }
                }
                x << "    </measure>\n";
            }
            x << "  </part>\n";
        }
        x << "</score-partwise>\n";
        return x;
    }

    /** Reads partwise MusicXML: every part's notes (tied notes joined), meters, key and tempo. */
    inline std::vector<PartIn> importScore (const juce::String& text, juce::String& err)
    {
        std::vector<PartIn> out;
        auto xml = juce::XmlDocument::parse (text);
        if (xml == nullptr) { err = "Not an XML file"; return out; }
        if (xml->hasTagName ("score-timewise")) { err = "Timewise MusicXML isn't supported - export partwise (the usual kind)"; return out; }
        if (! xml->hasTagName ("score-partwise")) { err = "Not a MusicXML score"; return out; }
        std::map<juce::String, juce::String> names;
        if (auto pl = xml->getChildByName ("part-list"))
            for (auto* sp : pl->getChildWithTagNameIterator ("score-part"))
                names[sp->getStringAttribute ("id")] = sp->getChildElementAllSubText ("part-name", {});
        for (auto* part : xml->getChildWithTagNameIterator ("part"))
        {
            PartIn p; p.name = names[part->getStringAttribute ("id")];
            double div = 1, cursor = 0, barStart = 0, lastStart = 0;
            int barIndex = 0, beats = 4, beatType = 4;
            std::map<int, size_t> openTies;   // pitch -> index of the note a tie continues
            for (auto* meas : part->getChildWithTagNameIterator ("measure"))
            {
                cursor = barStart; double barMax = 0;
                for (auto* el : meas->getChildIterator())
                {
                    if (el->hasTagName ("attributes"))
                    {
                        if (auto d = el->getChildByName ("divisions")) div = std::max (1.0, d->getAllSubText().getDoubleValue());
                        if (auto k = el->getChildByName ("key")) { p.key.fifths = k->getChildElementAllSubText ("fifths", "0").getIntValue(); p.key.minor = k->getChildElementAllSubText ("mode", "major") == "minor"; }
                        if (auto t = el->getChildByName ("time"))
                        {
                            beats = std::max (1, t->getChildElementAllSubText ("beats", "4").getIntValue()); beatType = std::max (1, t->getChildElementAllSubText ("beat-type", "4").getIntValue());
                            p.meters.push_back ({ barIndex, beats, beatType });
                        }
                    }
                    else if (el->hasTagName ("direction"))
                    {
                        if (auto snd = el->getChildByName ("sound")) if (snd->hasAttribute ("tempo") && p.tempo <= 0) p.tempo = snd->getDoubleAttribute ("tempo");
                    }
                    else if (el->hasTagName ("backup"))  cursor -= el->getChildElementAllSubText ("duration", "0").getDoubleValue() / div;
                    else if (el->hasTagName ("forward")) cursor += el->getChildElementAllSubText ("duration", "0").getDoubleValue() / div;
                    else if (el->hasTagName ("note"))
                    {
                        const double dur = el->getChildElementAllSubText ("duration", "0").getDoubleValue() / div;
                        const bool chord = el->getChildByName ("chord") != nullptr;
                        const double start = chord ? lastStart : cursor;
                        if (auto pitch = el->getChildByName ("pitch"))
                        {
                            const int step = juce::String (Notation::stepNames).indexOf (pitch->getChildElementAllSubText ("step", "C").trim());
                            const int alter = juce::roundToInt (pitch->getChildElementAllSubText ("alter", "0").getDoubleValue());
                            const int oct = pitch->getChildElementAllSubText ("octave", "4").getIntValue();
                            const int midi = (oct + 1) * 12 + Notation::stepToSemis (juce::jmax (0, step)) + alter;
                            bool tieStop = false, tieStart = false;
                            for (auto* t : el->getChildWithTagNameIterator ("tie")) { tieStop |= t->getStringAttribute ("type") == "stop"; tieStart |= t->getStringAttribute ("type") == "start"; }
                            if (tieStop && openTies.count (midi)) p.notes[openTies[midi]].length += dur;
                            else { p.notes.push_back ({ start, dur, midi, 90 }); openTies[midi] = p.notes.size() - 1; }
                            if (! tieStart) openTies.erase (midi);
                        }
                        if (! chord) { lastStart = cursor; cursor += dur; }
                        barMax = std::max (barMax, cursor - barStart);
                    }
                }
                const double barLen = std::max (barMax, beats * 4.0 / beatType);
                barStart += barLen; ++barIndex;
            }
            out.push_back (std::move (p));
        }
        if (out.empty()) err = "The score has no parts";
        return out;
    }
}
