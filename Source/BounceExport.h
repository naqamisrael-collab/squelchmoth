#pragma once
// Squelchmoth - Bounce Mix and Export Stems. Offline (faster than real time, sample-accurate), at the session sample rate
// by default, 16 / 24-bit with TPDF dither or 32-bit float, WAV / AIFF / FLAC, optional loudness normalising with a
// true-peak ceiling. Stems all start at the same point and have the same length so they line up in any DAW, and are
// named from a template ({n} {track} {session} {bpm} {date} {sr}).

#include "Session.h"
#include "TimeFormat.h"
#include "Theme.h"
#include "MonitorChain.h"
#include "SqMp3Encoder.h"

namespace BounceExport
{
    struct Options
    {
        juce::String format = "wav";      // wav | aiff | flac | mp3
        int bitDepth = 24;                 // 16, 24, 32 (float)
        double sampleRate = 0;             // 0 = session rate
        bool dither = true;                // TPDF dither for 16 / 24-bit
        bool masterInserts = true;         // print through the Master Fader's inserts
        std::optional<float> lufs;         // normalise to integrated loudness, true peak held under -1 dBTP
        std::optional<te::TimeRange> range;// default: whole session
        double tailSeconds = 0;            // extra time after the range for reverb / delay tails
        int mp3Bitrate = 320;              // 1.12 MP3: CBR kb/s ...
        int mp3VbrQuality = -1;            // ... or VBR V0-V4 when >= 0
        SqMp3::Tags tags;                  // ID3 tags for MP3 files
    };

    //==========================================================================
    // 1.12: MP3 export with Squelchmoth's own encoder. The mix renders to a 32-bit float WAV beside the target, which
    // is then encoded (resampled first when the rate isn't an MP3 rate: 88.2 / 176.4 kHz -> 44.1, 96 / 192 -> 48).
    using SqMp3::mp3RateFor;
    using SqMp3::encodeMp3;

    /** Renders with these parameters to 'dest', going through a temporary WAV when the format is MP3. */
    inline juce::File render (const juce::String& desc, te::Renderer::Parameters p, const Options& o, juce::String& err)
    {
        if (o.format != "mp3") return te::Renderer::renderToFile (desc, p);
        const auto finalDest = p.destFile;
        auto tmp = finalDest.getSiblingFile (finalDest.getFileNameWithoutExtension() + " (mp3 source).wav");
        p.destFile = tmp; p.bitDepth = 32; p.ditheringEnabled = false;
        auto f = te::Renderer::renderToFile (desc, p);
        if (! f.existsAsFile()) return {};
        const bool ok = encodeMp3 (f, finalDest, o.mp3Bitrate, o.mp3VbrQuality, o.tags, err);
        f.deleteFile();
        return ok ? finalDest : juce::File();
    }

    inline juce::AudioFormat* formatFor (Session& s, const juce::String& f)
    {
        auto& afm = s.getEngine().getAudioFileFormatManager();
        return f == "aiff" ? afm.getAiffFormat() : f == "flac" ? afm.getFlacFormat() : afm.getWavFormat();   // mp3: rendered as WAV, then encoded
    }

    inline te::TimeRange rangeFor (Session& s, const Options& o)
    {
        auto r = o.range.value_or (te::TimeRange (0s, te::toPosition (s.edit->getLength())));
        return { r.getStart(), r.getEnd() + te::TimeDuration::fromSeconds (juce::jmax (0.0, o.tailSeconds)) };
    }

    inline te::Renderer::Parameters params (Session& s, const Options& o, const juce::File& dest)
    {
        te::Renderer::Parameters p (*s.edit);
        p.destFile = dest;
        p.audioFormat = formatFor (s, o.format);
        p.bitDepth = o.format == "flac" ? juce::jmin (24, o.bitDepth) : o.bitDepth;
        p.sampleRateForAudio = o.sampleRate > 0 ? o.sampleRate : TimeFmt::sampleRate (*s.edit);
        p.time = rangeFor (s, o);
        p.useMasterPlugins = o.masterInserts;
        p.usePlugins = true;
        p.canRenderInMono = false;
        p.ditheringEnabled = o.dither && p.bitDepth < 32;
        p.blockSizeForAudio = 512;
        if (o.lufs)
        {
            p.shouldNormaliseByLUFS = true;
            p.normaliseToLevelDb = *o.lufs;
            p.limitTruePeak = true;
            p.truePeakCeilingDb = -1.0f;
        }
        return p;
    }

    /** Bounces the whole mix. Returns the file, or an empty File with err set. */
    inline juce::File bounceMix (Session& s, const Options& o, const juce::File& dest, juce::String& err)
    {
        if (! s.savingAllowed()) { err = "The 30-day trial has ended, so bouncing is off. Buy a licence (Help > Buy Squelchmoth) or enter a key."; return {}; }
        s.edit->getTransport().stop (false, false);
        auto r = rangeFor (s, o);
        if (r.getLength() <= 0.001s) { err = "Nothing to bounce - the session is empty or the range is invalid."; return {}; }
        auto p = params (s, o, dest);
        p.tracksToDo = te::toBitSet (te::getAllTracks (*s.edit));
        MonitorChain::PrintGuard guard (s);   // the Control Room chain and (optionally) the Master fader never print
        auto f = render ("Bounce", p, o, err);
        if (! f.existsAsFile()) { if (err.isEmpty()) err = "The bounce produced no audio - is there anything between those points?"; return {}; }
        return f;
    }

    //==========================================================================
    enum class StemSource { selectedTracks, allTracks, folders };

    struct StemOptions : Options
    {
        StemSource source = StemSource::allTracks;
        juce::Array<te::Track*> explicitTracks;     // overrides source when not empty
        juce::String nameTemplate = "{n} {track}";
        bool withSendEffects = false;               // also play the aux returns the track sends to (stems "with FX")
        bool skipSilent = false;                    // 1.10: delete stems that rendered silent (peak below -90 dBFS)
        juce::File folder;                          // default: Bounces/<session> Stems <date>
    };

    inline juce::String expandName (Session& s, const juce::String& tmpl, int n, te::Track& t, double sr)
    {
        auto bpm = s.edit->tempoSequence.getTempo (0)->getBpm();
        auto txt = tmpl.replace ("{n}", juce::String (n).paddedLeft ('0', 2))
                       .replace ("{track}", t.getName())
                       .replace ("{session}", s.getFile().getFileNameWithoutExtension())
                       .replace ("{bpm}", juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.01 ? 0 : 2))
                       .replace ("{date}", juce::Time::getCurrentTime().formatted ("%Y-%m-%d"))
                       .replace ("{sr}", juce::String (sr / 1000.0, sr == 44100.0 || sr == 88200.0 || sr == 176400.0 ? 1 : 0) + "k");
        txt = juce::File::createLegalFileName (txt.trim());
        return txt.isEmpty() ? t.getName() : txt;
    }

    /** The tracks a stem export covers. */
    inline juce::Array<te::Track*> stemTracks (Session& s, const StemOptions& o, const std::function<bool (te::Track*)>& isSelected)
    {
        if (! o.explicitTracks.isEmpty()) return o.explicitTracks;
        juce::Array<te::Track*> out;
        if (o.source == StemSource::folders)
        {
            // each top-level routing folder is one stem; tracks outside any folder are their own stems
            for (auto t : te::getTopLevelTracks (*s.edit))
            {
                if (auto f = dynamic_cast<te::FolderTrack*> (t)) { if (f->isSubmixFolder() || ! f->getAllSubTracks (true).isEmpty()) out.add (t); }
                else if (dynamic_cast<te::AudioTrack*> (t) != nullptr) out.add (t);
            }
            return out;
        }
        for (auto t : s.getRows())
        {
            auto at = dynamic_cast<te::AudioTrack*> (t);
            if (at == nullptr) continue;
            if (o.source == StemSource::selectedTracks && ! (isSelected && isSelected (t))) continue;
            // "all": skip empty tracks (no clips and not an aux return) - a folder of silent files helps nobody
            if (o.source == StemSource::allTracks && at->getClips().isEmpty() && s.getBusNumberOfTrack (*at) < 0) continue;
            out.add (t);
        }
        return out;
    }

    /** Everything a stem needs to sound right: the track (or folder and its members), the routing folders it passes
        through and, optionally, the aux returns it feeds. */
    inline juce::Array<te::Track*> renderSetFor (Session& s, te::Track& stem, bool withSendFx)
    {
        juce::Array<te::Track*> set;
        set.add (&stem);
        if (auto f = dynamic_cast<te::FolderTrack*> (&stem)) for (auto sub : f->getAllSubTracks (true)) set.addIfNotAlreadyThere (sub);
        for (auto p = stem.getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack()) set.addIfNotAlreadyThere (p);
        if (withSendFx)
        {
            juce::Array<te::Track*> sources (set);
            for (auto t : sources)
                if (auto at = dynamic_cast<te::AudioTrack*> (t))
                    for (auto send : Session::getSends (*at))
                        if (auto ret = s.findBusTrack (send->busNumber.get()))
                        {
                            set.addIfNotAlreadyThere (ret);
                            for (auto p = ret->getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack()) set.addIfNotAlreadyThere (p);
                        }
        }
        return set;
    }

    struct StemResult { int written = 0, silent = 0, failed = 0; juce::File folder; juce::StringArray lines; };

    /** Sample peak of a rendered file in dBFS (-100 = silent or unreadable). */
    inline float peakDbOfFile (Session& s, const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> r (s.getEngine().getAudioFileFormatManager().readFormatManager.createReaderFor (f));
        if (r == nullptr || r->lengthInSamples <= 0) return -100.0f;
        float peak = 0.0f;
        juce::AudioBuffer<float> buf ((int) r->numChannels, 65536);
        for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += 65536)
        {
            const int n = (int) juce::jmin ((juce::int64) 65536, r->lengthInSamples - pos);
            r->read (&buf, 0, n, pos, true, true);
            for (int c = 0; c < buf.getNumChannels(); ++c) peak = juce::jmax (peak, buf.getMagnitude (c, 0, n));
        }
        return peak > 0.0f ? juce::Decibels::gainToDecibels (peak) : -100.0f;
    }

    inline StemResult exportStems (Session& s, const StemOptions& o, const std::function<bool (te::Track*)>& isSelected = {})
    {
        StemResult res;
        if (! Edition::allows (Edition::Feature::stems)) { res.lines.add (Edition::featureLockMessage (Edition::Feature::stems)); res.failed = 1; return res; }
        if (! s.savingAllowed()) { res.lines.add ("The 30-day trial has ended, so exporting is off."); ++res.failed; return res; }
        s.edit->getTransport().stop (false, false);
        auto stems = stemTracks (s, o, isSelected);
        if (stems.isEmpty()) { res.lines.add ("No tracks to export."); return res; }

        const double sr = o.sampleRate > 0 ? o.sampleRate : TimeFmt::sampleRate (*s.edit);
        res.folder = o.folder != juce::File() ? o.folder
                       : s.bouncesDir.getNonexistentChildFile (juce::File::createLegalFileName (s.getFile().getFileNameWithoutExtension() + " Stems " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M")), {}, false);
        res.folder.createDirectory();
        const auto ext = "." + (o.format == "aiff" ? juce::String ("aif") : o.format);

        MonitorChain::PrintGuard guard (s);
        int n = 0;
        for (auto t : stems)
        {
            ++n;
            auto dest = res.folder.getNonexistentChildFile (expandName (s, o.nameTemplate, n, *t, sr), ext, false);
            auto p = params (s, o, dest);
            p.tracksToDo = te::toBitSet (renderSetFor (s, *t, o.withSendEffects));
            p.checkNodesForAudio = false;   // a silent stem is still written so every file lines up
            juce::String stemErr;
            auto f = render ("Stem " + t->getName(), p, o, stemErr);
            if (! f.existsAsFile()) { ++res.failed; res.lines.add ("FAILED  " + t->getName()); continue; }
            if (o.skipSilent && peakDbOfFile (s, f) < -90.0f)
            {
                f.deleteFile();
                ++res.silent;
                res.lines.add ("SKIPPED (silent)  " + t->getName());
                continue;
            }
            ++res.written;
            res.lines.add (f.getFileName());
        }
        s.log.add ("Squelchmoth", "Exported " + juce::String (res.written) + " stems to " + res.folder.getFullPathName());
        return res;
    }

    //==========================================================================
    /** File > Bounce Mix... / Export Stems... dialog (Pro Tools Bounce dialog layout). */
    class Dialog : public juce::Component
    {
    public:
        Dialog (Session& s, bool stemsMode, std::function<bool (te::Track*)> selectedFn, bool hasSelection, std::optional<te::TimeRange> editSel)
            : session (s), stems (stemsMode), isSelected (std::move (selectedFn)), selection (editSel)
        {
            auto addCombo = [this] (juce::ComboBox& c, const juce::StringArray& items, int sel)
            {
                addAndMakeVisible (c); c.addItemList (items, 1); c.setSelectedId (sel, juce::dontSendNotification);
            };
            addAndMakeVisible (nameEd);
            nameEd.setText (stems ? Prefs::getString ("bx_stemTemplate", "{n} {track}") : s.getFile().getFileNameWithoutExtension() + " mix", false);
            nameEd.setTooltip (stems ? "File names: {n} number, {track} track name, {session}, {bpm}, {date}, {sr}" : "File name");
            addCombo (fmt, { "WAV (BWF)", "AIFF", "FLAC", "MP3 320 kb/s", "MP3 256 kb/s", "MP3 192 kb/s", "MP3 128 kb/s", "MP3 VBR V0 (best)", "MP3 VBR V2", "MP3 VBR V4" }, juce::jlimit (1, 10, Prefs::getInt ("bx_format", 1)));
            addCombo (depth, { "16-bit", "24-bit", "32-bit float" }, juce::jlimit (1, 3, Prefs::getInt ("bx_depth", 2)));
            juce::StringArray rateNames;
            const double sr = TimeFmt::sampleRate (*s.edit);
            rateNames.add ("Session rate (" + juce::String (sr / 1000.0, 1) + " kHz)");
            for (auto r : rates) rateNames.add (juce::String (r / 1000.0, 1) + " kHz");
            addCombo (rate, rateNames, 1);
            addCombo (lufs, { "Off (no normalising)", "-14 LUFS (Spotify / YouTube)", "-16 LUFS (Apple Music)", "-9 LUFS (club / loud)", "-23 LUFS (broadcast)" }, 1);
            addCombo (range, { "Whole session", "Edit selection" }, hasSelection && editSel ? 2 : 1);
            range.setItemEnabled (2, editSel.has_value());
            addCombo (tail, { "No tail", "+1 second tail", "+2 second tail", "+4 second tail" }, juce::jlimit (1, 4, Prefs::getInt ("bx_tail", 3)));
            if (stems) addCombo (source, { "All audio / instrument / aux tracks", "Selected tracks", "Routing folders (one stem each)" }, hasSelection ? 2 : 1);
            for (auto* b : { &dither, &master, &sendFx }) addAndMakeVisible (b);
            dither.setToggleState (Prefs::getBool ("bx_dither", true), juce::dontSendNotification);
            master.setToggleState (Prefs::getBool (stems ? "bx_stemMaster" : "bx_master", ! stems), juce::dontSendNotification);
            sendFx.setToggleState (Prefs::getBool ("bx_sendFx", false), juce::dontSendNotification);
            sendFx.setVisible (stems);
            for (auto* b : { &goBtn, &cancelBtn }) addAndMakeVisible (b);
            goBtn.setButtonText (stems ? "Export Stems" : "Bounce");
            goBtn.onClick = [this] { run(); };
            cancelBtn.onClick = [this] { close(); };
            lufs.setVisible (! stems);
            setSize (520, stems ? 372 : 372);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel());
            g.setColour (Theme::dim()); g.setFont (Theme::font (10.0f, true));
            for (auto& l : labels) g.drawText (l.second, l.first.withWidth (150), juce::Justification::centredLeft);
        }

        void resized() override
        {
            labels.clear();
            auto r = getLocalBounds().reduced (16);
            auto row = [&] (const juce::String& label, juce::Component& c)
            {
                auto line = r.removeFromTop (26); r.removeFromTop (6);
                labels.push_back ({ line.removeFromLeft (150), label });
                c.setBounds (line);
            };
            row (stems ? "NAME TEMPLATE" : "FILE NAME", nameEd);
            if (stems) row ("STEMS", source);
            row ("FILE TYPE", fmt);
            row ("BIT DEPTH", depth);
            row ("SAMPLE RATE", rate);
            row ("RANGE", range);
            row ("TAIL", tail);
            if (! stems) row ("NORMALISE", lufs);   // normalising stems one by one would break the mix balance
            auto t = r.removeFromTop (24); r.removeFromTop (4);
            dither.setBounds (t.removeFromLeft (170)); master.setBounds (t.removeFromLeft (190)); sendFx.setBounds (t);
            auto b = r.removeFromBottom (30);
            cancelBtn.setBounds (b.removeFromRight (100)); b.removeFromRight (8); goBtn.setBounds (b.removeFromRight (150));
        }

        std::function<void (const juce::String& summary, const juce::File& reveal)> onDone;

    private:
        static constexpr double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
        Session& session;
        bool stems;
        std::function<bool (te::Track*)> isSelected;
        std::optional<te::TimeRange> selection;
        juce::TextEditor nameEd;
        juce::ComboBox fmt, depth, rate, lufs, range, tail, source;
        juce::ToggleButton dither { "Dither (16 / 24-bit)" }, master { "Through Master inserts" }, sendFx { "Include send effects" };
        juce::TextButton goBtn, cancelBtn { "Cancel" };
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> labels;

        void close()    { if (auto dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0); }

        void fill (Options& o)
        {
            static const char* fmts[] = { "wav", "aiff", "flac", "mp3", "mp3", "mp3", "mp3", "mp3", "mp3", "mp3" };
            static const int mp3Rates[] = { 0, 0, 0, 320, 256, 192, 128, 0, 0, 0 };
            static const int mp3Vbr[] = { -1, -1, -1, -1, -1, -1, -1, 0, 2, 4 };
            static const int depths[] = { 16, 24, 32 };
            static const float lufsVals[] = { 0, -14, -16, -9, -23 };
            static const double tails[] = { 0, 1, 2, 4 };
            o.format = fmts[fmt.getSelectedId() - 1];
            if (o.format == "mp3")
            {
                o.mp3Bitrate = mp3Rates[fmt.getSelectedId() - 1] > 0 ? mp3Rates[fmt.getSelectedId() - 1] : 320;
                o.mp3VbrQuality = mp3Vbr[fmt.getSelectedId() - 1];
                o.tags.title = nameEd.getText().trim().isNotEmpty() ? nameEd.getText().trim() : session.getFile().getFileNameWithoutExtension();
                o.tags.year = juce::String (juce::Time::getCurrentTime().getYear());
            }
            o.bitDepth = depths[depth.getSelectedId() - 1];
            o.sampleRate = rate.getSelectedId() <= 1 ? 0.0 : rates[rate.getSelectedId() - 2];
            o.dither = dither.getToggleState();
            o.masterInserts = master.getToggleState();
            if (! stems && lufs.getSelectedId() > 1) o.lufs = lufsVals[lufs.getSelectedId() - 1];
            if (range.getSelectedId() == 2 && selection) o.range = *selection;
            o.tailSeconds = tails[tail.getSelectedId() - 1];
            Prefs::set ("bx_format", fmt.getSelectedId()); Prefs::set ("bx_depth", depth.getSelectedId()); Prefs::set ("bx_tail", tail.getSelectedId());
            Prefs::set ("bx_dither", o.dither); Prefs::set (stems ? "bx_stemMaster" : "bx_master", o.masterInserts);
        }

        void run()
        {
            if (session.edit == nullptr) return;
            juce::String summary; juce::File reveal;
            if (stems)
            {
                StemOptions o; fill (o);
                o.nameTemplate = nameEd.getText().trim().isEmpty() ? juce::String ("{n} {track}") : nameEd.getText().trim();
                o.source = source.getSelectedId() == 2 ? StemSource::selectedTracks : source.getSelectedId() == 3 ? StemSource::folders : StemSource::allTracks;
                o.withSendEffects = sendFx.getToggleState();
                Prefs::set ("bx_stemTemplate", o.nameTemplate); Prefs::set ("bx_sendFx", o.withSendEffects);
                auto res = exportStems (session, o, isSelected);
                summary = juce::String (res.written) + " stem(s) exported" + (res.failed > 0 ? ", " + juce::String (res.failed) + " failed" : juce::String()) + ":\n" + res.lines.joinIntoString ("\n");
                reveal = res.folder;
            }
            else
            {
                Options o; fill (o);
                const auto ext = "." + (o.format == "aiff" ? juce::String ("aif") : o.format);
                auto dest = session.bouncesDir.getNonexistentChildFile (juce::File::createLegalFileName (nameEd.getText().trim().isEmpty() ? juce::String ("Bounce") : nameEd.getText().trim()), ext, false);
                juce::String err;
                auto f = bounceMix (session, o, dest, err);
                summary = f.existsAsFile() ? "Bounced to " + f.getFullPathName() : err;
                reveal = f;
            }
            auto done = onDone;
            close();
            if (done) done (summary, reveal);
        }
    };
}
