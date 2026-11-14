#pragma once
// Squelchmoth 1.15 "Beat Intelligence" - L1: the Reference Listener in the app.
//
// Loads a reference instrumental (a file, or the audio on a track), runs SqRef::analyse on a background thread,
// caches the result per file + range, and turns it into what a producer needs to rebuild the feel from scratch:
// a plain-language read, the measured blueprint (JSON) and a step-by-step rebuild plan in Squelchmoth's own tools.
// Nothing of the recording is copied or stored: only measurements (super prompt 1.15 section 2).

#include "SqReferenceDsp.h"
#include <memory>
#include <mutex>

namespace RefListen
{
    //==========================================================================
    /** Stereo audio read from a file (start / length in seconds; 0 length = to the end, capped at 8 minutes). */
    inline bool readStereo (const juce::File& f, double startSec, double lengthSec, std::vector<float>& L, std::vector<float>& R, double& fs, juce::String& err)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        if (rd == nullptr || rd->lengthInSamples <= 0) { err = "Couldn't read " + f.getFileName() + " (WAV, AIFF, FLAC, MP3 or OGG)"; return false; }
        fs = rd->sampleRate;
        const juce::int64 s0 = juce::jlimit ((juce::int64) 0, rd->lengthInSamples, (juce::int64) (juce::jmax (0.0, startSec) * fs));
        juce::int64 n = rd->lengthInSamples - s0;
        if (lengthSec > 0) n = juce::jmin (n, (juce::int64) (lengthSec * fs));
        n = juce::jmin (n, (juce::int64) (480.0 * fs));
        if (n < (juce::int64) (4.0 * fs)) { err = "Too short to analyse (needs at least 4 seconds of audio)"; return false; }
        L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
        const int chunk = 1 << 16; juce::AudioBuffer<float> buf ((int) juce::jmax (1u, rd->numChannels), chunk);
        for (juce::int64 done = 0; done < n; done += chunk)
        {
            const int m = (int) juce::jmin ((juce::int64) chunk, n - done);
            rd->read (&buf, 0, m, s0 + done, true, true);
            const float* l = buf.getReadPointer (0); const float* r = buf.getReadPointer (juce::jmin (1, buf.getNumChannels() - 1));
            std::copy (l, l + m, L.begin() + (std::ptrdiff_t) done); std::copy (r, r + m, R.begin() + (std::ptrdiff_t) done);
        }
        return true;
    }

    //==========================================================================
    struct Job
    {
        juce::File file; double startSec = 0, lengthSec = 0;
        juce::String key, label;
        SqRef::Result result;
        juce::String error;
        juce::WaitableEvent done { true };
        std::atomic<bool> finished { false };
    };

    /** The analyses made this run, newest last. Thread-safe; the work happens on a pool thread. */
    class Store
    {
    public:
        static Store& get() { static Store s; return s; }

        static juce::String keyFor (const juce::File& f, double start, double len)
        {
            return f.getFullPathName() + "|" + juce::String (f.getLastModificationTime().toMilliseconds()) + "|" + juce::String (start, 2) + "|" + juce::String (len, 2);
        }

        /** Starts (or finds) the analysis of a file range. */
        std::shared_ptr<Job> start (const juce::File& f, double startSec = 0, double lengthSec = 0, const juce::String& label = {})
        {
            const auto k = keyFor (f, startSec, lengthSec);
            std::lock_guard<std::mutex> lk (lock);
            for (auto& j : jobs) if (j->key == k) { touch (j); return j; }
            auto j = std::make_shared<Job>();
            j->file = f; j->startSec = startSec; j->lengthSec = lengthSec; j->key = k; j->label = label.isNotEmpty() ? label : f.getFileNameWithoutExtension();
            jobs.push_back (j);
            while (jobs.size() > 12) jobs.erase (jobs.begin());
            pool.addJob ([j]
            {
                std::vector<float> L, R; double fs = 44100;
                if (readStereo (j->file, j->startSec, j->lengthSec, L, R, fs, j->error))
                {
                    j->result = SqRef::analyse (L.data(), R.data(), (int) L.size(), fs);
                    if (! j->result.ok && j->error.isEmpty()) j->error = j->result.error;
                }
                j->finished = true; j->done.signal();
            });
            return j;
        }

        std::shared_ptr<Job> latest()
        {
            std::lock_guard<std::mutex> lk (lock);
            return jobs.empty() ? nullptr : jobs.back();
        }

    private:
        void touch (const std::shared_ptr<Job>& j) { auto it = std::find (jobs.begin(), jobs.end(), j); if (it != jobs.end()) { jobs.erase (it); jobs.push_back (j); } }
        std::mutex lock;
        std::vector<std::shared_ptr<Job>> jobs;
        juce::ThreadPool pool { 1 };
    };

    //==========================================================================
    inline juce::String noteName (float midi)
    {
        const int m = (int) std::lround (midi);
        return juce::String (SqRef::noteNames[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    }

    inline int barOf (const SqRef::Result& r, double t)
    {
        int b = 0;
        for (size_t i = 0; i < r.bars.size(); ++i) if (r.bars[i] <= t + 0.06) b = (int) i;
        return b;
    }

    /** The chord loop of a section: chords whose start falls inside it, with their length in bars. */
    inline juce::StringArray chordLoop (const SqRef::Result& r, int fromBar, int bars, bool roman)
    {
        juce::StringArray out;
        for (size_t i = 0; i < r.chords.size(); ++i)
        {
            const auto& c = r.chords[i];
            const int b = barOf (r, c.startSec);
            if (b < fromBar || b >= fromBar + bars) continue;
            out.add (roman && c.roman.isNotEmpty() ? c.roman : c.name);
        }
        // a chord held from before the section starts it
        if (out.isEmpty())
            for (size_t i = r.chords.size(); i-- > 0;)
                if (barOf (r, r.chords[i].startSec) < fromBar) { out.add (roman ? r.chords[i].roman : r.chords[i].name); break; }
        return out;
    }

    /** The shortest repeating unit of a sequence (A F C G A F C G -> A F C G). */
    inline juce::StringArray loopUnit (const juce::StringArray& s)
    {
        for (int p = 1; p <= s.size() / 2; ++p)
        {
            bool ok = true;
            for (int i = p; i < s.size() && ok; ++i) ok = s[i] == s[i - p];
            if (ok) { juce::StringArray u; for (int i = 0; i < p; ++i) u.add (s[i]); return u; }
        }
        return s;
    }

    inline bool halfTime (const SqRef::Result& r)
    {
        // snare only on beat 3 (step 9) in most bars = half-time feel at this tempo
        int s3 = 0, s24 = 0, n = 0;
        for (auto& d : r.drums) { ++n; if (d.snare[8] > 0) ++s3; if (d.snare[4] > 0 || d.snare[12] > 0) ++s24; }
        return n > 4 && s3 > n / 2 && s24 < n / 4;
    }

    /** A cautious style read from the measurements (no names: techniques only). */
    inline juce::String styleRead (const SqRef::Result& r)
    {
        int rolls = 0; for (auto& d : r.drums) rolls += d.hatRolls;
        const bool ht = halfTime (r);
        const double bpm = r.bpm, felt = ht ? bpm / 2 : bpm;
        const bool heavySub = r.subPresence > 0.3f || r.bandDb[0] > r.bandDb[2];
        const bool swung = r.swing > 0.56;
        juce::StringArray cues;
        if (rolls > 2) cues.add ("hi-hat rolls");
        if (r.subGlides > 1) cues.add ("gliding 808");
        if (ht) cues.add ("half-time snare");
        if (swung) cues.add ("swung 16ths (" + juce::String (juce::roundToInt (r.swing * 100)) + " %)");
        if (r.pumpDb > 2.5f) cues.add ("sidechain pump");
        juce::String g;
        if ((bpm >= 125 && bpm <= 165 && (rolls > 2 || r.subGlides > 1)) || (felt >= 62 && felt <= 82 && rolls > 2)) g = r.subGlides > 3 && bpm >= 138 && bpm <= 148 && ! (rolls > 6) ? "drill-leaning trap" : "modern trap";
        else if (bpm >= 84 && bpm <= 100 && swung && ! heavySub) g = "boom bap / soul-sample hip-hop";
        else if (bpm >= 60 && bpm <= 80 && ! ht && ! heavySub) g = "slow R&B / neo-soul";
        else if ((bpm >= 60 && bpm <= 75) || (ht && felt >= 60 && felt <= 75)) g = "R&B / slow hip-hop";
        else if (bpm >= 100 && bpm <= 115 && r.swing > 0.53) g = "afrobeats / afro-swing";
        else if (bpm >= 110 && bpm <= 118 && r.pumpDb > 2) g = "amapiano / deep house";
        else if (bpm >= 118 && bpm <= 132 && r.pumpDb > 2) g = "house / dance";
        else if (bpm >= 85 && bpm <= 100) g = "hip-hop (mid-tempo)";
        else g = "an uncommon tempo for the main rap / R&B styles";
        return "reads like " + g + (cues.isEmpty() ? juce::String() : " (" + cues.joinIntoString (", ") + ")");
    }

    //==========================================================================
    /** The measured blueprint as JSON (full = per-bar drums, every chord and every 808 note). */
    inline juce::var toJson (const SqRef::Result& r, bool full)
    {
        auto o = new juce::DynamicObject(); juce::var v (o);
        o->setProperty ("seconds", r.seconds);
        o->setProperty ("bpm", std::round (r.bpm * 100) / 100); o->setProperty ("bpm_confidence", r.bpmConfidence);
        o->setProperty ("half_time_feel", halfTime (r));
        o->setProperty ("bars", (int) r.bars.size()); o->setProperty ("first_downbeat_sec", r.bars.empty() ? 0.0 : r.bars.front());
        o->setProperty ("swing_percent", juce::roundToInt (r.swing * 100));
        o->setProperty ("key", r.keyName); o->setProperty ("key_root", r.keyRoot); o->setProperty ("minor", r.minor); o->setProperty ("key_confidence", r.keyConfidence);
        o->setProperty ("style_read", styleRead (r));
        juce::Array<juce::var> secs;
        for (auto& s : r.sections)
        {
            auto so = new juce::DynamicObject();
            so->setProperty ("name", s.name); so->setProperty ("letter", juce::String::charToString ((juce::juce_wchar) s.letter));
            so->setProperty ("start_bar", s.startBar + 1); so->setProperty ("bars", s.bars); so->setProperty ("energy_db", std::round (s.energyDb * 10) / 10);
            so->setProperty ("elements", s.elementList.joinIntoString (", ")); so->setProperty ("element_count", s.elements);
            so->setProperty ("chords", loopUnit (chordLoop (r, s.startBar, s.bars, false)).joinIntoString (" "));
            so->setProperty ("roman", loopUnit (chordLoop (r, s.startBar, s.bars, true)).joinIntoString (" "));
            so->setProperty ("kick", SqRef::typicalRow (r.drums, s.startBar, s.startBar + s.bars, 0));
            so->setProperty ("snare", SqRef::typicalRow (r.drums, s.startBar, s.startBar + s.bars, 1));
            so->setProperty ("hat", SqRef::typicalRow (r.drums, s.startBar, s.startBar + s.bars, 2));
            int rolls = 0; juce::StringArray rollBars;
            for (int b = s.startBar; b < s.startBar + s.bars && b < (int) r.drums.size(); ++b) if (r.drums[(size_t) b].hatRolls > 0) { rolls += r.drums[(size_t) b].hatRolls; rollBars.add (juce::String (b + 1)); }
            so->setProperty ("hat_roll_beats", rolls); so->setProperty ("hat_roll_bars", rollBars.joinIntoString (" "));
            secs.add (juce::var (so));
        }
        o->setProperty ("sections", secs);
        auto sub = new juce::DynamicObject();
        sub->setProperty ("glides", r.subGlides); sub->setProperty ("octave_jumps", r.subOctaveJumps); sub->setProperty ("sub_share", std::round (r.subPresence * 100) / 100);
        juce::Array<juce::var> notes; int limit = full ? 400 : 24;
        for (auto& n : r.sub)
        {
            if (notes.size() >= limit) break;
            auto no = new juce::DynamicObject();
            const int b = barOf (r, n.startSec);
            const double barLen = 240.0 / juce::jmax (1.0, r.bpm);
            const double beat = r.bars.empty() ? 0 : (n.startSec - r.bars[(size_t) b]) / barLen * 4.0;
            no->setProperty ("bar", b + 1); no->setProperty ("beat", std::round (beat * 4) / 4 + 1); no->setProperty ("note", noteName (n.midi));
            no->setProperty ("beats", std::round (n.lengthSec / barLen * 16) / 4); if (n.glide) no->setProperty ("glide_in", true);
            notes.add (juce::var (no));
        }
        sub->setProperty ("notes", notes);
        o->setProperty ("808", juce::var (sub));
        auto mix = new juce::DynamicObject();
        const char* bn[] = { "sub_20_60", "low_60_250", "lowmid_250_2k", "presence_2k_6k", "air_6k_11k" };
        for (int i = 0; i < 5; ++i) mix->setProperty (bn[i], std::round (r.bandDb[(size_t) i] * 10) / 10);
        mix->setProperty ("width_low", std::round (r.widthBands[0] * 100) / 100); mix->setProperty ("width_mid", std::round (r.widthBands[1] * 100) / 100); mix->setProperty ("width_high", std::round (r.widthBands[2] * 100) / 100);
        mix->setProperty ("pump_db", std::round (r.pumpDb * 10) / 10); mix->setProperty ("hat_hits_per_beat", std::round (r.hatDensity * 10) / 10);
        o->setProperty ("mix", juce::var (mix));
        if (full)
        {
            juce::Array<juce::var> ch;
            for (auto& c : r.chords) { auto co = new juce::DynamicObject(); co->setProperty ("bar", barOf (r, c.startSec) + 1); co->setProperty ("chord", c.name); co->setProperty ("roman", c.roman); ch.add (juce::var (co)); }
            o->setProperty ("chords", ch);
            juce::Array<juce::var> dr;
            for (size_t b = 0; b < r.drums.size(); ++b)
            {
                auto d = new juce::DynamicObject();
                d->setProperty ("bar", (int) b + 1); d->setProperty ("kick", SqRef::stepString (r.drums[b].kick)); d->setProperty ("snare", SqRef::stepString (r.drums[b].snare));
                d->setProperty ("hat", SqRef::stepString (r.drums[b].hat)); if (r.drums[b].hatRolls) d->setProperty ("hat_roll_beats", r.drums[b].hatRolls);
                dr.add (juce::var (d));
            }
            o->setProperty ("drums_per_bar", dr);
        }
        return v;
    }

    //==========================================================================
    /** What a producer would say after one listen, then the rebuild plan in Squelchmoth's tools. */
    inline juce::String summary (const SqRef::Result& r, const juce::String& label)
    {
        juce::String s;
        const bool ht = halfTime (r);
        s << "REFERENCE: " << label << " (" << juce::String (r.seconds, 1) << " s)\n";
        s << "- Tempo " << juce::String (r.bpm, 1) << " BPM" << (ht ? " (half-time feel: the snare lands on 3, so it feels like " + juce::String (r.bpm / 2, 1) + ")" : juce::String())
          << ", " << (int) r.bars.size() << " bars of 4/4, first downbeat at " << juce::String (r.bars.empty() ? 0.0 : r.bars.front(), 2) << " s.\n";
        s << "- Key " << r.keyName << " (confidence " << juce::String (r.keyConfidence, 2) << "). Swing " << juce::roundToInt (r.swing * 100) << " %" << (r.swing < 0.53 ? " (straight)" : "") << ".\n";
        s << "- " << styleRead (r) << ".\n";
        s << "- Arrangement:";
        for (auto& sec : r.sections) s << " " << sec.name << " " << sec.bars << " (bar " << (sec.startBar + 1) << ")" << ",";
        s = s.trimCharactersAtEnd (",") + ".\n";
        for (auto& sec : r.sections)
        {
            s << "  " << sec.name << " [" << juce::String::charToString ((juce::juce_wchar) sec.letter) << "] bars " << (sec.startBar + 1) << "-" << (sec.startBar + sec.bars)
              << ", " << juce::String (sec.energyDb, 1) << " dB, " << sec.elements << " elements: " << sec.elementList.joinIntoString (", ") << "\n";
            const auto chords = loopUnit (chordLoop (r, sec.startBar, sec.bars, false));
            const auto roman = loopUnit (chordLoop (r, sec.startBar, sec.bars, true));
            if (! chords.isEmpty()) s << "    chords " << chords.joinIntoString (" - ") << "  (" << roman.joinIntoString (" - ") << ")\n";
            const auto k = SqRef::typicalRow (r.drums, sec.startBar, sec.startBar + sec.bars, 0), sn = SqRef::typicalRow (r.drums, sec.startBar, sec.startBar + sec.bars, 1), h = SqRef::typicalRow (r.drums, sec.startBar, sec.startBar + sec.bars, 2);
            if (k.containsChar ('x') || sn.containsChar ('x') || h.containsChar ('x')) s << "    kick  " << k << "\n    snare " << sn << "\n    hat   " << h << "\n";
            int rolls = 0; for (int b = sec.startBar; b < sec.startBar + sec.bars && b < (int) r.drums.size(); ++b) rolls += r.drums[(size_t) b].hatRolls;
            if (rolls > 0) s << "    hat rolls (1/32 or faster) in " << rolls << " beat" << (rolls > 1 ? "s" : "") << "\n";
        }
        s << "- 808 / sub: " << (int) r.sub.size() << " notes, " << r.subGlides << " glides, " << r.subOctaveJumps << " octave jumps, sub share " << juce::roundToInt (r.subPresence * 100) << " % of the energy.";
        if (! r.sub.empty())
        {
            s << " First notes:";
            for (size_t i = 0; i < r.sub.size() && i < 12; ++i) s << " " << (r.sub[i].glide ? "~" : "") << noteName (r.sub[i].midi);
        }
        s << "\n";
        s << "- Mix: sub " << juce::String (r.bandDb[0], 1) << ", low " << juce::String (r.bandDb[1], 1) << ", low-mid " << juce::String (r.bandDb[2], 1)
          << ", presence " << juce::String (r.bandDb[3], 1) << ", air " << juce::String (r.bandDb[4], 1) << " dB (relative to the total). Width low/mid/high "
          << juce::String (r.widthBands[0], 2) << " / " << juce::String (r.widthBands[1], 2) << " / " << juce::String (r.widthBands[2], 2)
          << (r.widthBands[0] < 0.1f ? " (mono low end)" : " (the low end is NOT mono: keep yours mono anyway)")
          << ". Pump " << juce::String (r.pumpDb, 1) << " dB" << (r.pumpDb > 2.5f ? " (audible sidechain)" : "") << ". Hats " << juce::String (r.hatDensity, 1) << " hits per beat.\n";
        return s;
    }

    inline juce::String rebuildPlan (const SqRef::Result& r)
    {
        juce::String p;
        const bool ht = halfTime (r);
        const int root = r.keyRoot; const juce::String keyN = SqRef::noteNames[root];
        p << "REBUILD PLAN (original parts in the same feel - never copy the recording):\n";
        p << "1. set_tempo bpm " << juce::String (r.bpm, 0) << "; set the key " << r.keyName << " (detect_key apply or midi_tools key).\n";
        p << "2. Arrange first: add_marker for each section (";
        for (auto& sec : r.sections) p << sec.name << " bar " << (sec.startBar + 1) << ", ";
        p = p.trimCharactersAtEnd (", ") + "). Match the element counts per section; the biggest section gets every element.\n";
        p << "3. Drums: add_drum_track with a kit that fits (" << styleRead (r).fromFirstOccurrenceOf ("like ", false, false).upToFirstOccurrenceOf (" (", false, false)
          << "), then beat_lab / set_beat per section with the kick, snare and hat rows above as the skeleton. Then make them yours: ghost notes, an open hat, "
          << "fills at the end of every 4 or 8 bars, velocity variation (accents on the backbeat, hats 70-110).";
        int rolls = 0; for (auto& d : r.drums) rolls += d.hatRolls;
        if (rolls > 0) p << " Hat rolls: 1/32 (and 1/24 triplet) bursts on the last beat of a phrase, beat_lab row with roll digits.";
        if (r.swing > 0.53) p << " Swing " << juce::roundToInt ((r.swing - 0.5) / 0.25 * 100) << " % on the 16ths (beat_lab swing).";
        p << "\n";
        p << "4. Harmony: chords generate (or midi_tools progression) in " << r.keyName << " with the same harmonic function as the section's loop (the Roman numerals above are common property; "
          << "make the part yours: new voicings and inversions, a different chord rhythm, an added extension or a passing chord) on a keys / pad instrument "
          << "(instrument list; pick a patch that matches the reference's register and brightness). Voice-lead, 3-4 notes, keep it below the vocal range.\n";
        p << "5. 808: a Moth 808 track; follow the chord roots in octave 1 (" << keyN << "1 area), start each bar on the kick, ";
        if (r.subGlides > 0) p << r.subGlides << " glides measured: use midi_tools slides into the next root (overlapping notes, glide 1/32-1/16), ";
        if (r.subOctaveJumps > 0) p << "octave jumps on pickups, ";
        p << "cut each 808 note where the next kick lands (no overlap mud). Sidechain or tuned so kick + 808 don't fight.\n";
        p << "6. Melody / top line: a lead or counter-melody in the hook only (leave space in verses), " << (r.minor ? "minor pentatonic / natural minor" : "major pentatonic / major") << " in " << r.keyName << ".\n";
        p << "7. Mix to the measured balance: " << (r.bandDb[0] > r.bandDb[2] ? "sub-heavy (808 louder than the mids)" : "mids forward (808 under the mids)")
          << ", mono below 150 Hz, " << (r.widthBands[2] > 0.2f ? "wide top (pads, hats, FX)" : "fairly narrow top") << (r.pumpDb > 2.5f ? ", sidechain pump to the kick" : "") << ".\n";
        p << "8. Bounce a section (analyze_mix), then analyze_reference action compare to check your beat against this reference and fix the biggest gaps first.\n";
        juce::ignoreUnused (ht);
        return p;
    }

    //==========================================================================
    /** Side-by-side numbers of two analyses (the reference and the user's / the AI's beat). */
    inline juce::String compare (const SqRef::Result& ref, const SqRef::Result& mine)
    {
        juce::String s; s << "COMPARE (reference vs yours):\n";
        auto line = [&] (const juce::String& what, const juce::String& a, const juce::String& b, const juce::String& tip) { s << "- " << what << ": " << a << " vs " << b << (tip.isNotEmpty() ? "  -> " + tip : juce::String()) << "\n"; };
        line ("tempo", juce::String (ref.bpm, 1), juce::String (mine.bpm, 1), std::abs (ref.bpm - mine.bpm) > 1 ? "set_tempo " + juce::String (ref.bpm, 0) : "");
        line ("key", ref.keyName, mine.keyName, ref.keyName != mine.keyName ? "transpose to " + ref.keyName : "");
        line ("swing", juce::String (juce::roundToInt (ref.swing * 100)) + " %", juce::String (juce::roundToInt (mine.swing * 100)) + " %", std::abs (ref.swing - mine.swing) > 0.04 ? "adjust beat_lab swing" : "");
        int rr = 0, mr = 0; for (auto& d : ref.drums) rr += d.hatRolls; for (auto& d : mine.drums) mr += d.hatRolls;
        line ("hat rolls (beats)", juce::String (rr), juce::String (mr), rr > 0 && mr == 0 ? "add 1/32 rolls on phrase ends" : "");
        line ("hats per beat", juce::String (ref.hatDensity, 1), juce::String (mine.hatDensity, 1), std::abs (ref.hatDensity - mine.hatDensity) > 1 ? "change the hat rate" : "");
        line ("808 glides", juce::String (ref.subGlides), juce::String (mine.subGlides), ref.subGlides > mine.subGlides + 1 ? "add slides" : "");
        line ("sub share", juce::String (juce::roundToInt (ref.subPresence * 100)) + " %", juce::String (juce::roundToInt (mine.subPresence * 100)) + " %",
              ref.subPresence > mine.subPresence + 0.08f ? "808 louder / longer" : mine.subPresence > ref.subPresence + 0.08f ? "808 quieter / shorter" : "");
        const char* bn[] = { "sub", "low", "low-mid", "presence", "air" };
        for (int i = 0; i < 5; ++i)
        {
            const float d = mine.bandDb[(size_t) i] - ref.bandDb[(size_t) i];
            line (juce::String (bn[i]) + " band", juce::String (ref.bandDb[(size_t) i], 1) + " dB", juce::String (mine.bandDb[(size_t) i], 1) + " dB",
                  std::abs (d) > 2.0f ? (d > 0 ? "cut " : "boost ") + juce::String (std::abs (d), 1) + " dB there (EQ / balance)" : "");
        }
        line ("width high", juce::String (ref.widthBands[2], 2), juce::String (mine.widthBands[2], 2), ref.widthBands[2] > mine.widthBands[2] + 0.1f ? "widen pads / hats" : "");
        line ("pump", juce::String (ref.pumpDb, 1) + " dB", juce::String (mine.pumpDb, 1) + " dB", ref.pumpDb > mine.pumpDb + 1.5f ? "sidechain the music to the kick" : "");
        line ("sections", juce::String ((int) ref.sections.size()), juce::String ((int) mine.sections.size()), mine.sections.size() + 1 < ref.sections.size() ? "arrange more sections (drops, switch-ups)" : "");
        int re = 0, me = 0; for (auto& x : ref.sections) re = juce::jmax (re, x.elements); for (auto& x : mine.sections) me = juce::jmax (me, x.elements);
        line ("most elements in a section", juce::String (re), juce::String (me), re > me ? "add layers in the hook (counter-melody, percussion, pad, FX)" : "");
        return s;
    }
}
