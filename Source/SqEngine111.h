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

namespace SqVoiceCore
{
    struct Snapshot
    {
        float hz = 0, confidence = 0, rmsDb = -120, hnrDb = 0;
        bool voiced = false, sibilant = false;
        float formants[4] {};        // Hz, 0 when not found
        juce::int64 position = 0;    // input sample the frame describes
    };

    class Core
    {
    public:
        /** Latency of split(): the comb looks one longest period (60 Hz) either side. */
        int splitLatency() const   { return maxPeriod; }

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate > 0 ? sampleRate : 48000.0;
            tracker.prepare (sr);
            tracker.setRange (60.0f, 1100.0f);
            D = juce::jmax (1, juce::roundToInt (sr / 11025.0));
            fsd = sr / D;
            frameLen = 256; hopLen = 64;                                     // ~23 ms frame, ~6 ms hop at 11 kHz
            dec.assign ((size_t) frameLen * 2, 0.0f);
            frame.assign ((size_t) frameLen, 0.0f);
            tmp.reserve ((size_t) frameLen + 8);
            aa1.set (sr, juce::jmin (4800.0, fsd * 0.44)); aa2.set (sr, juce::jmin (4800.0, fsd * 0.44));
            maxPeriod = (int) std::ceil (sr / 60.0) + 4;
            hist.assign ((size_t) juce::nextPowerOfTwo (maxPeriod * 3 + juce::jmax (64, maxBlock) + 8), 0.0f);
            histMask = (int) hist.size() - 1;
            reset();
        }
        void reset()
        {
            tracker.reset(); aa1.reset(); aa2.reset();
            std::fill (dec.begin(), dec.end(), 0.0f); decW = 0; decCount = 0; hopCount = 0;
            std::fill (hist.begin(), hist.end(), 0.0f); hw = 0;
            snap = {}; voicedMix = 0; periodSmooth = 0; eH = eN = 1.0e-9f;
        }

        /** Analyse a block (mono). Cheap enough to run on the audio thread of every plug-in that needs it. */
        void analyse (const float* x, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                if (tracker.push (x[i]))
                {
                    const auto& f = tracker.latest();
                    snap.hz = f.voiced ? f.hz : 0.0f; snap.confidence = f.conf; snap.voiced = f.voiced; snap.sibilant = f.sibilant;
                    snap.rmsDb = juce::Decibels::gainToDecibels (f.rms, -120.0f); snap.position = f.pos;
                }
                const float y = aa2.lp (aa1.lp (x[i]));
                if (++decCount >= D)
                {
                    decCount = 0;
                    dec[(size_t) (decW++ % (int) dec.size())] = y;
                    if (++hopCount >= hopLen) { hopCount = 0; formantFrame(); }
                }
            }
        }

        /** Harmonic / noise split (mono, in -> harm + noise, both delayed by splitLatency()). Call after analyse(). */
        void split (const float* in, float* harm, float* noise, int n)
        {
            const float targetPeriod = snap.voiced && snap.hz > 0 ? (float) (sr / snap.hz) : 0.0f;
            const float targetMix = snap.voiced ? juce::jlimit (0.0f, 1.0f, (snap.confidence - 0.3f) / 0.5f) : 0.0f;
            const float k = (float) (1.0 - std::exp (-1.0 / (0.008 * sr)));
            for (int i = 0; i < n; ++i)
            {
                hist[(size_t) (hw & histMask)] = in[i];
                if (targetPeriod > 0) periodSmooth = periodSmooth <= 0 ? targetPeriod : periodSmooth + k * (targetPeriod - periodSmooth);
                voicedMix += k * (targetMix - voicedMix);
                const double centre = (double) hw - maxPeriod;
                const float c = at (centre);
                float h = 0;
                if (periodSmooth > 2.0f)
                {
                    const float T = juce::jmin (periodSmooth, (float) maxPeriod - 2.0f);
                    h = 0.25f * at (centre - T) + 0.5f * c + 0.25f * at (centre + T);   // zero at odd multiples of f0/2
                }
                const float hh = h * voicedMix;
                harm[i] = hh; noise[i] = c - hh;
                eH = 0.9995f * eH + 0.0005f * hh * hh; eN = 0.9995f * eN + 0.0005f * (c - hh) * (c - hh);
                ++hw;
            }
            snap.hnrDb = 10.0f * std::log10 ((eH + 1.0e-12f) / (eN + 1.0e-12f));
        }

        const Snapshot& latest() const      { return snap; }
        const SqVocal::PitchTracker& pitchTracker() const { return tracker; }
        int analysisLag() const             { return tracker.lagSamples(); }

    private:
        double sr = 48000, fsd = 11025; int D = 4, frameLen = 256, hopLen = 64, decW = 0, decCount = 0, hopCount = 0;
        SqVocal::PitchTracker tracker;
        SqVocal::Svf aa1, aa2;
        std::vector<float> dec, frame, tmp, hist; int histMask = 0; juce::int64 hw = 0; int maxPeriod = 800;
        Snapshot snap; float voicedMix = 0, periodSmooth = 0, eH = 1.0e-9f, eN = 1.0e-9f;

        float at (double pos) const
        {
            const juce::int64 i = (juce::int64) std::floor (pos); const float f = (float) (pos - (double) i);
            const float a = hist[(size_t) (i & histMask)], b = hist[(size_t) ((i + 1) & histMask)];
            return a + (b - a) * f;
        }

        /** LPC (order 12 at 11 kHz) -> spectral envelope -> the four lowest peaks between 150 Hz and 4.8 kHz. */
        void formantFrame()
        {
            for (int i = 0; i < frameLen; ++i) frame[(size_t) i] = dec[(size_t) ((decW - frameLen + i + (int) dec.size() * 4) % (int) dec.size())];
            constexpr int order = 12; float k[SqVocal::Lpc::maxOrder] {}; float err = 0;
            if (! SqVocal::Lpc::analyse (frame.data(), frameLen, order, fsd, k, err, tmp, 0.0) || ! snap.voiced)
            { std::fill (std::begin (snap.formants), std::end (snap.formants), 0.0f); return; }
            // reflection -> direct form (step-up)
            double a[order + 1] {}, t[order + 1] {}; a[0] = 1;
            for (int m = 1; m <= order; ++m)
            {
                for (int i = 0; i <= m; ++i) t[i] = a[i];
                for (int i = 1; i < m; ++i) a[i] = t[i] + k[m - 1] * t[m - i];
                a[m] = k[m - 1];
            }
            constexpr int bins = 192; double env[bins];
            for (int b = 0; b < bins; ++b)
            {
                const double w = juce::MathConstants<double>::pi * (double) b / (bins - 1) * juce::jmin (1.0, 4800.0 / (fsd * 0.5));
                double re = 0, im = 0; for (int i = 0; i <= order; ++i) { re += a[i] * std::cos (w * i); im -= a[i] * std::sin (w * i); }
                env[b] = -10.0 * std::log10 (re * re + im * im + 1e-30);
            }
            int found = 0; float fm[4] {};
            const double hzPerBin = juce::jmin (4800.0, fsd * 0.5) / (bins - 1);
            for (int b = 1; b < bins - 1 && found < 4; ++b)
            {
                if (env[b] > env[b - 1] && env[b] >= env[b + 1])
                {
                    const double d = 0.5 * (env[b - 1] - env[b + 1]) / (env[b - 1] - 2 * env[b] + env[b + 1] - 1e-12);   // parabolic
                    const double hz = (b + juce::jlimit (-0.5, 0.5, d)) * hzPerBin;
                    if (hz > 150.0) fm[found++] = (float) hz;
                }
            }
            for (int i = 0; i < 4; ++i) snap.formants[i] = fm[i];
        }
    };
}

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

    /** Renders one preset through a private copy of the plug-in. */
    inline Row auditPreset (te::Edit& edit, const Signature::Entry& e, const Signature::Preset& pr, double sr, double seconds, bool strictLevel)
    {
        Row row; row.plugin = e.name; row.preset = pr.name;
        row.category = juce::String (pr.name).contains (" | ") ? juce::String (pr.name).upToFirstOccurrenceOf (" | ", false, false) : juce::String (e.category);
        auto pp = edit.getPluginCache().createNewPlugin (e.type, {});
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
                const double c0 = juce::Time::getMillisecondCounterHiRes();
                pp->applyToBuffer (fc);
                busy += juce::Time::getMillisecondCounterHiRes() - c0; total += block / sr * 1000.0;
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
        pp->baseClassDeinitialise();
        row.cpuPercent = (float) (100.0 * busy / juce::jmax (1.0, total));
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
