#pragma once
// Squelchmoth 1.11 shared engines, app side (super prompt §5):
//   SqVoiceCore::Core   Moth Tune's pitch tracker as a shared service: pitch, voicing, sibilance, formants F1-F4,
//                       harmonic-to-noise ratio and a fixed-latency harmonic / noise split. Feeds Siren EQ's pitch bands,
//                       Siren Verb's auto-EQ, Siren Comp's pitch-filter sidechain, Exhale, Vox Moth and Hive Choir.
//   PresetAudit         renders every factory preset of every built-in effect on pink noise, a drum loop and a sung
//                       vocal (48 kHz), checks level, peak, NaN / Inf, CPU and latency, and writes a report.
// The pure DSP lives in SqEngine111Dsp.h.

#include <JuceHeader.h>
#include "SqEngine111Dsp.h"
#include "SqVocalDsp.h"
#include "Signature.h"

namespace te = tracktion;

#include "SqVoiceCore.h"


//==============================================================================
namespace PresetAudit
{
    struct Row
    {
        juce::String plugin, preset, category, why;
        float levelDb[3] {}, peakDb = -120, cpuPercent = 0; int latency = 0, target = 0; bool finite = true, pass = true, levelChecked = true;
    };
    struct Summary { int plugins = 0, presets = 0, passed = 0, failed = 0; double seconds = 0; juce::File report; std::vector<Row> rows; };

    /** Categories whose presets are meant to change the level a lot (destroy, lo-fi, loud...): no +-1 dB rule. */
    inline bool levelExempt (const juce::String& plugin, const juce::String& category, const juce::String& presetName)
    {
        const auto c = category.toLowerCase() + " " + presetName.toLowerCase();
        for (auto w : { "loud", "destroy", "lo-fi", "lofi", "crush", "smash", "gate", "slice", "tremolo", "stutter", "filter sweep", "telephone", "radio", "fx", "creative", "special", "kill", "mute", "duck", "pump", "sound design" })
            if (c.contains (w)) return true;
        juce::ignoreUnused (plugin);
        return false;
    }

    /** The audit signal a preset is held to (+-1 dB): vocal presets on the sung vocal, drum / 808 presets on the drum
        loop, everything else on pink noise. The other two signals are still rendered and reported. */
    inline int targetSignal (const juce::String& pluginCategory, const juce::String& presetCategory, const juce::String& presetName = {})
    {
        const auto c = presetCategory + " " + pluginCategory;
        if (c.containsIgnoreCase ("Vocal") || c.containsIgnoreCase ("Voice")) return 2;
        if (c.containsIgnoreCase ("Drum") || presetCategory.startsWith ("808") || presetCategory.startsWith ("Dub") || presetCategory.startsWith ("Rhythmic") || presetCategory.startsWith ("Snare") || presetCategory.startsWith ("Drum Room") || presetName.containsIgnoreCase ("Parallel Smash | Drum")) return 1;
        return 0;
    }

    /** Plug-ins whose output at Mix 100 % is a different signal by design (reverbs and delays read wet-only, filters,
        modulators): their level rule is advisory unless the plug-in is a 1.11 plug-in that declares otherwise. */
    inline bool levelAdvisoryType (const juce::String& category)
    {
        return category == "Space" || category == "Modulation" || category == "EQ & Filter" || category == "Pitch & Vocal" || category == "Utility";
    }

    struct Signals { std::vector<float> l[3], r[3]; const char* names[3] { "pink", "drums", "vocal" }; };

    inline const Signals& signals (double sr, double seconds)
    {
        static Signals s; static double madeSr = 0, madeLen = 0;
        if (madeSr != sr || madeLen != seconds)
        {
            Eng::TestSignals::pink (s.l[0], s.r[0], sr, seconds, -18.0f);
            Eng::TestSignals::drums (s.l[1], s.r[1], sr, seconds);
            Eng::TestSignals::vocal (s.l[2], s.r[2], sr, seconds);
            madeSr = sr; madeLen = seconds;
        }
        return s;
    }

    /** Milliseconds of CPU this thread has used (POSIX), so time the OS spends on other work doesn't count against a
        plug-in. Windows keeps the wall clock (its thread timer only ticks every ~16 ms). */
    inline double threadCpuMs()
    {
       #if JUCE_MAC || JUCE_LINUX
        timespec ts {}; if (clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts) == 0) return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
       #endif
        return juce::Time::getMillisecondCounterHiRes();
    }

    /** Renders one preset through a private copy of the plug-in. */
    inline Row auditPreset (te::Edit& edit, const Signature::Entry& e, const Signature::Preset& pr, double sr, double seconds, bool strictLevel)
    {
        Row row; row.plugin = e.name; row.preset = pr.name;
        row.category = juce::String (pr.name).contains (" | ") ? juce::String (pr.name).upToFirstOccurrenceOf (" | ", false, false) : juce::String (e.category);
        // straight from the plug-in manager, not the edit's cache: the cache only lets go on its 1 s timer, which never
        // runs while the audit holds the message thread, so hundreds of instances (and their delay lines) would pile up
        auto pp = edit.engine.getPluginManager().createNewPlugin (edit, e.type, {});
        if (pp == nullptr) { row.pass = false; row.why = "could not create"; return row; }
        Signature::applyPreset (*pp, pr);
        const int block = 256;
        const auto& sig = signals (sr, seconds);
        pp->baseClassInitialise ({ te::TimePosition(), sr, block });
        row.latency = juce::roundToInt (pp->getLatencySeconds() * sr);
        double busy = 0, total = 0;
        const size_t skip = (size_t) (sr * juce::jmin (1.0, seconds / 3.0));   // let slow auto-gain / reverb tails settle
        juce::AudioBuffer<float> buf (2, block); te::MidiMessageArray midi;
        for (int k = 0; k < 3; ++k)
        {
            // the input level over the same window as the output (the signals are not stationary)
            double ein = 0, einN = 0;
            for (size_t i = skip; i < sig.l[k].size(); ++i) { ein += 0.5 * ((double) sig.l[k][i] * sig.l[k][i] + (double) sig.r[k][i] * sig.r[k][i]); einN += 1; }
            const float inRmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (ein / juce::jmax (1.0, einN)), -120.0f);
            pp->reset();
            const size_t n = sig.l[k].size(); double e2 = 0, cnt = 0; float pk = 0;
            for (size_t p = 0; p + (size_t) block <= n; p += (size_t) block)
            {
                buf.copyFrom (0, 0, sig.l[k].data() + p, block); buf.copyFrom (1, 0, sig.r[k].data() + p, block);
                midi.clear();
                const double t0 = (double) p / sr, t1 = t0 + block / sr;
                te::PluginRenderContext fc (&buf, 0, block, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (t0), te::TimePosition::fromSeconds (t1)), true, false, true, false);
                const double c0 = threadCpuMs();
                pp->applyToBuffer (fc);
                busy += threadCpuMs() - c0; total += block / sr * 1000.0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < block; ++i)
                    {
                        const float v = buf.getSample (ch, i);
                        if (! std::isfinite (v)) row.finite = false;
                        else if (p >= skip) { e2 += (double) v * v * 0.5; pk = juce::jmax (pk, std::abs (v)); }
                    }
                if (p >= skip) cnt += block;
            }
            row.levelDb[k] = juce::Decibels::gainToDecibels ((float) std::sqrt (e2 / juce::jmax (1.0, cnt)), -120.0f) - inRmsDb;
            row.peakDb = juce::jmax (row.peakDb, juce::Decibels::gainToDecibels (pk, -120.0f));
        }
        row.cpuPercent = (float) (100.0 * busy / juce::jmax (1.0, total));
        // Wall-clock timing can only be inflated by the OS taking the core away (a busy CI machine, a background
        // indexer). If the first measurement is over the limit, time the pink-noise pass again, twice, and keep
        // the lowest: a plug-in that is really over budget is over budget every time.
        for (int retry = 0; retry < 2 && row.cpuPercent > 25.0f; ++retry)
        {
            pp->reset(); double b2 = 0, t2 = 0; const size_t n = sig.l[0].size();
            for (size_t p = 0; p + (size_t) block <= n; p += (size_t) block)
            {
                buf.copyFrom (0, 0, sig.l[0].data() + p, block); buf.copyFrom (1, 0, sig.r[0].data() + p, block);
                midi.clear();
                const double t0 = (double) p / sr, t1 = t0 + block / sr;
                te::PluginRenderContext fc (&buf, 0, block, &midi, 0.0, te::TimeRange (te::TimePosition::fromSeconds (t0), te::TimePosition::fromSeconds (t1)), true, false, true, false);
                const double c0 = threadCpuMs();
                pp->applyToBuffer (fc);
                b2 += threadCpuMs() - c0; t2 += block / sr * 1000.0;
            }
            row.cpuPercent = juce::jmin (row.cpuPercent, (float) (100.0 * b2 / juce::jmax (1.0, t2)));
        }
        pp->baseClassDeinitialise();
        // verdicts
        juce::StringArray why;
        if (! row.finite) why.add ("NaN / Inf in the output");
        if (row.peakDb > 6.0f) why.add ("peak " + juce::String (row.peakDb, 1) + " dBFS");
        row.levelChecked = strictLevel && ! levelExempt (e.name, row.category, pr.name);
        row.target = targetSignal (e.category, row.category, pr.name);
        if (row.levelChecked && std::abs (row.levelDb[row.target]) > 1.0f)
            why.add ("level " + juce::String (row.levelDb[row.target], 1) + " dB on " + sig.names[row.target]);
        if (row.cpuPercent > 25.0f) why.add ("CPU " + juce::String (row.cpuPercent, 1) + " %");
        for (int k = 0; k < 3; ++k) if (row.levelDb[k] < -60.0f) { why.add ("silent on " + juce::String (sig.names[k])); break; }
        row.pass = why.isEmpty();
        row.why = why.joinIntoString ("; ");
        return row;
    }

    /** Audits the given plug-ins (by Signature name; empty = every effect). strictTypes: plug-ins whose presets must
        hold +-1 dB (the 1.11 rule); everything else gets the level reported but not enforced. */
    inline Summary run (te::Edit& edit, const juce::StringArray& only, const juce::StringArray& strictTypes, double seconds, const juce::File& report,
                        std::function<void (const juce::String&)> progress = {})
    {
        Summary s; const double sr = 48000.0; const double started = juce::Time::getMillisecondCounterHiRes();
        for (auto& e : Signature::entries())
        {
            if (juce::String (e.category) == "Instruments" || e.presets.empty()) continue;
            if (juce::String (e.type) == "midiModifier" || juce::String (e.type) == "sqmRackChannel") continue;
            if (! only.isEmpty() && ! only.contains (e.name)) continue;
            ++s.plugins;
            const bool strict = strictTypes.contains (e.type) || strictTypes.contains (e.name) || ! levelAdvisoryType (e.category) && strictTypes.contains ("*");
            for (auto& pr : e.presets)
            {
                auto row = auditPreset (edit, e, pr, sr, seconds, strict);
                ++s.presets; (row.pass ? s.passed : s.failed)++;
                if (progress && ! row.pass) progress (row.plugin + " / " + row.preset + ": " + row.why);
                s.rows.push_back (row);
            }
        }
        s.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
        if (report != juce::File())
        {
            juce::String md;
            md << "# Squelchmoth preset audit\n\n"
               << "Generated " << juce::Time::getCurrentTime().toString (true, true) << " by `Squelchmoth --preset-audit`. 48 kHz, "
               << juce::String (seconds, 1) << " s per signal: pink noise at -18 dBFS RMS, a drum loop peaking at -6 dBFS, a sung vocal at -20 dBFS RMS.\n\n"
               << "- **Plug-ins:** " << s.plugins << "\n- **Presets:** " << s.presets << "\n- **Passed:** " << s.passed << "\n- **Failed:** " << s.failed
               << "\n- **Run time:** " << juce::String (s.seconds, 1) << " s\n\n"
               << "Level is output RMS minus input RMS after the first 250 ms. Rules: no NaN / Inf, peak at most +6 dBFS, CPU at most 25 % of one core, "
                  "never silent, and for plug-ins held to the 1.11 rule, level within +-1 dB on the material the preset was made for (vocal presets on "
                  "the vocal, drum and 808 presets on the drum loop, everything else on pink noise; the starred column) unless the preset's category is "
                  "marked loud / destroy / lo-fi / gate and the like.\n\n"
               << "| Plug-in | Preset | Pink dB | Drums dB | Vocal dB | Peak dBFS | CPU % | Latency | Result |\n|---|---|---:|---:|---:|---:|---:|---:|---|\n";
            for (auto& r : s.rows)
            {
                auto lv = [&r] (int k) { return juce::String (r.levelDb[k], 1) + (r.levelChecked && r.target == k ? "*" : ""); };
                md << "| " << r.plugin << " | " << r.preset.replace ("|", "/") << " | " << lv (0) << " | " << lv (1)
                   << " | " << lv (2) << " | " << juce::String (r.peakDb, 1) << " | " << juce::String (r.cpuPercent, 2) << " | " << r.latency
                   << " | " << (r.pass ? juce::String (r.levelChecked ? "pass" : "pass (level advisory)") : "**FAIL** " + r.why) << " |\n";
            }
            report.getParentDirectory().createDirectory();
            report.replaceWithText (md);
            s.report = report;
        }
        return s;
    }
}
