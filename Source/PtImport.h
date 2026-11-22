#pragma once
// Squelchmoth 1.54 - reading a Pro Tools session: what is on which track, and where. The session file is listed by a
// separate small program, sqptread (built on the LGPL library libptformat - see ThirdParty/ptformat), which this file
// finds and runs; the answer is turned into a plan here. What comes across: audio tracks, their clips (which file,
// where on the timeline, from where in the file, how long) and MIDI notes. What does not: fades, volume, pan,
// automation, plug-ins, sends, tempo changes - the library does not read them.
//
// Sessions whose tracks follow the tempo give clip positions in ticks (960,000 to a quarter note), not in time, and
// the library does not read the tempo. So the tempo is worked out as the fastest at which no two clips on a track
// would overlap (clips that butt up against each other pin it exactly); it can be given instead.

#include <JuceHeader.h>
#include "Frame.h"

namespace PtImport
{
    inline juce::File helper()
    {
        const auto over = juce::SystemStats::getEnvironmentVariable ("SQUELCH_PTREAD", {}); if (over.isNotEmpty()) return juce::File (over);
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::currentApplicationFile).getChildFile ("Contents/Resources/sqptread");
       #elif JUCE_WINDOWS
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("sqptread.exe");
       #else
        { const auto beside = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("sqptread"); if (beside.existsAsFile()) return beside; return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getParentDirectory().getChildFile ("helpers/sqptread"); }
       #endif
    }
    /** Runs the reader on a session file; its JSON answer, or an empty string and why. */
    inline juce::String list (const juce::File& session, juce::String& err)
    {
        const auto h = helper(); if (! h.existsAsFile()) { err = "The Pro Tools reader (sqptread) is not installed beside the app."; return {}; }
        juce::MemoryBlock out; if (! Frame::runBinary ({ h.getFullPathName(), session.getFullPathName() }, out, 64 * 1024 * 1024)) { err = "Couldn't run the Pro Tools reader."; return {}; }
        return out.toString();
    }

    struct Clip { juce::String file, name; double start = 0, offset = 0, length = 0; juce::File found; };
    struct Note { double beat = 0, beats = 0; int pitch = 60, velocity = 100; };
    struct MidiClip { juce::String name; double startBeat = 0; std::vector<Note> notes; };
    struct Track { juce::String name; std::vector<Clip> clips; std::vector<MidiClip> midi; };
    struct Plan
    {
        bool ok = false; juce::String error; int version = 0; double rate = 48000.0, bpm = 0.0; bool inTicks = false, bpmWorkedOut = false, bpmPinned = false, bpmUnknown = false;
        std::vector<Track> audio, midi; juce::StringArray files, missing; std::vector<Clip> unplaced;       // unplaced: files the session lists that the reader found on no track
        int clips = 0, notes = 0, halvesJoined = 0;
    };
    constexpr double ticksPerQuarter = 960000.0, tickZero = 3567587328.0;       // the sessions' own zero point for ticks (10^12), as it comes out of 32 bits

    /** Turns the reader's answer into a plan. folder: where to look for the audio (the session's own folder). bpm 0: work it out. */
    inline Plan plan (const juce::String& json, const juce::File& folder, double bpm = 0.0)
    {
        Plan p; const auto v = juce::JSON::parse (json); if (! v.isObject() || ! (bool) v["ok"]) { p.error = "That file could not be read as a Pro Tools session (versions 5 to 12 are understood)."; return p; }
        p.version = (int) v["version"]; p.rate = juce::jmax (8000.0, (double) v["rate"]);
        if (auto* fs = v["files"].getArray()) for (auto& f : *fs) p.files.add (f["name"].toString());
        struct Raw { juce::String track, file, region; int index; double at, offset, length; }; std::vector<Raw> raw; double least = 1.0e30;
        if (auto* a = v["audio"].getArray()) for (auto& c : *a) { raw.push_back ({ c["track"].toString(), c["file"].toString(), c["region"].toString(), (int) c["index"], (double) c["at"], (double) c["offset"], (double) c["length"] }); least = std::min (least, raw.back().at); }
        p.inTicks = ! raw.empty() && least >= tickZero - 1.0;
        std::map<int, std::vector<Raw>> byTrack; for (auto& r : raw) { if (p.inTicks) r.at -= tickZero; byTrack[r.index].push_back (r); }
        for (auto& t : byTrack) std::sort (t.second.begin(), t.second.end(), [] (const Raw& a, const Raw& b) { return a.at < b.at; });
        if (p.inTicks)
        {
            double most = 1.0e9; bool any = false;
            for (auto& t : byTrack) for (size_t i = 0; i + 1 < t.second.size(); ++i) { const double dq = (t.second[i + 1].at - t.second[i].at) / ticksPerQuarter, secs = t.second[i].length / p.rate; if (dq > 0.0 && secs > 0.0) { most = std::min (most, dq * 60.0 / secs); any = true; } }
            if (bpm > 0.0) p.bpm = bpm; else if (any) { p.bpm = most; const double r = std::round (most * 2.0) / 2.0; if (std::abs (r - most) < 0.02) { p.bpm = r; p.bpmPinned = true; } p.bpmWorkedOut = true; } else { p.bpm = 120.0; p.bpmWorkedOut = true; p.bpmUnknown = true; }
            p.bpm = juce::jlimit (20.0, 400.0, p.bpm);
        }
        auto seconds = [&] (double at) { return p.inTicks ? at / ticksPerQuarter * 60.0 / p.bpm : at / p.rate; };
        auto locate = [&] (const juce::String& name) -> juce::File
        {
            if (name.isEmpty() || folder == juce::File()) return {};
            for (auto& sub : { juce::String ("Audio Files"), juce::String() }) { const auto f = sub.isEmpty() ? folder.getChildFile (name) : folder.getChildFile (sub).getChildFile (name); if (f.existsAsFile()) return f; }
            for (auto& f : folder.findChildFiles (juce::File::findFiles, true, name)) return f;
            return {};
        };
        std::vector<int> order; for (auto& t : byTrack) order.push_back (t.first);
        juce::String lastName; std::vector<std::pair<double, juce::String>> lastSet;
        for (int ix : order)
        {
            auto& rs = byTrack[ix]; std::vector<std::pair<double, juce::String>> set; for (auto& r : rs) set.push_back ({ r.at, r.file });
            if (! p.audio.empty() && rs.front().track == lastName && set == lastSet) { ++p.halvesJoined; continue; }       // the two halves of a stereo track list the same clips twice
            Track t; t.name = rs.front().track.trim(); lastName = rs.front().track; lastSet = set;
            for (auto& r : rs) { Clip c; c.file = r.file; c.name = r.region; c.start = seconds (r.at); c.offset = r.offset / p.rate; c.length = r.length / p.rate; c.found = locate (r.file); if (! c.found.existsAsFile()) p.missing.addIfNotAlreadyThere (r.file); t.clips.push_back (c); ++p.clips; }
            p.audio.push_back (t);
        }
        for (auto& name : p.files)       // a file the session lists but the reader put on no track: its place on the timeline is not known
        {
            bool used = false; for (auto& r : raw) if (r.file == name) used = true; if (used) continue;
            Clip c; c.file = name; c.name = juce::File::createLegalFileName (name).upToLastOccurrenceOf (".", false, false); c.found = locate (name); if (! c.found.existsAsFile()) p.missing.addIfNotAlreadyThere (name); p.unplaced.push_back (c);
        }
        if (auto* m = v["midi"].getArray())
        {
            std::map<int, Track> tracks; std::vector<int> mo;
            for (auto& c : *m)
            {
                const int ix = (int) c["index"]; if (tracks.find (ix) == tracks.end()) { tracks[ix].name = c["track"].toString().trim(); mo.push_back (ix); }
                MidiClip mc; mc.name = c["region"].toString(); mc.startBeat = (double) c["at"] / ticksPerQuarter;
                if (auto* ns = c["notes"].getArray()) for (auto& n : *ns) if (auto* q = n.getArray()) if (q->size() >= 4) { Note nt; nt.beat = (double) (*q)[0] / ticksPerQuarter; nt.beats = juce::jmax (0.01, (double) (*q)[1] / ticksPerQuarter); nt.pitch = (int) (*q)[2]; nt.velocity = juce::jlimit (1, 127, (int) (*q)[3]); mc.notes.push_back (nt); ++p.notes; }
                if (! mc.notes.empty()) tracks[ix].midi.push_back (mc);
            }
            for (int ix : mo) if (! tracks[ix].midi.empty()) p.midi.push_back (tracks[ix]);
            if (! p.midi.empty() && p.bpm <= 0.0) { p.bpm = bpm > 0.0 ? bpm : 120.0; p.bpmWorkedOut = bpm <= 0.0; }
        }
        p.ok = true; return p;
    }
    inline juce::String describe (const Plan& p)
    {
        juce::String s = "Pro Tools " + juce::String (p.version) + " session at " + juce::String (p.rate / 1000.0, 1) + " kHz: " + juce::String ((int) p.audio.size()) + " audio tracks with " + juce::String (p.clips) + " clips from " + juce::String (p.files.size()) + " audio files";
        if (p.halvesJoined > 0) s << " (" << p.halvesJoined << " stereo tracks listed as two halves, joined)";
        if (! p.midi.empty()) s << "; " << (int) p.midi.size() << " MIDI tracks with " << p.notes << " notes";
        s << ".";
        if (p.inTicks) s << " Its clips are placed by bars and beats, and the tempo is not something the reader can see: " << (p.bpmUnknown ? juce::String ("there are not enough clips to work it out, so 120 BPM is used - give bpm if clips land in the wrong place (one starting at the very beginning is right at any tempo)") : p.bpmWorkedOut ? (p.bpmPinned ? "worked out as " + juce::String (p.bpm, 1) + " BPM (clips that butt up against each other fix it)" : "taken as " + juce::String (p.bpm, 2) + " BPM, the fastest at which no clips overlap - give bpm if you know better") : "using the " + juce::String (p.bpm, 2) + " BPM given") << ".";
        else if (! p.midi.empty() && p.bpmWorkedOut) s << " MIDI is placed at 120 BPM unless bpm is given.";
        if (! p.unplaced.empty()) { s << " " << (int) p.unplaced.size() << (p.unplaced.size() == 1 ? " file is" : " files are") << " in the session's clip list but the reader found " << (p.unplaced.size() == 1 ? "it" : "them") << " on no track, so where " << (p.unplaced.size() == 1 ? "it sits" : "they sit") << " on the timeline is NOT known:"; for (size_t i = 0; i < p.unplaced.size() && i < 12; ++i) s << (i ? ", " : " ") << p.unplaced[i].file; s << "."; }
        if (! p.missing.isEmpty()) s << " " << p.missing.size() << " of its audio files were not found beside the session (looked in 'Audio Files' and below): " << p.missing.joinIntoString (", ").substring (0, 600) << ".";
        return s;
    }
}
