#pragma once
// Squelchmoth 1.50 - Voltage Overdrive: an overdrive's clipping stage solved as a circuit (see SqDrive.h), as a plug-in.
// An amplifier with a pair of diodes in its feedback: quiet signals are simply made louder (about 12 times with Drive at
// its least, up to 118 at its most, above 720 Hz - bass passes with less gain, so it stays clean), and once the feedback
// reaches the diodes' knee they take over and the wave rounds off. The output is the input itself plus what the diodes
// allow, which is why this clips softly where Voltage Clip clips hard. Diodes picks the pair, Symmetry makes one side two
// diodes in series (even harmonics), Tone is a low-pass after the stage. A full-scale sample is half a volt at the stage.
// Every instance has its own parts, and left and right are two circuits.

#include <JuceHeader.h>
#include "SqDrive.h"
#include "SqTube.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageOverdrivePlugin : public te::Plugin
{
public:
    SqVoltageOverdrivePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        driveV.referTo (state, "drive", um, 20.0f); toneV.referTo (state, "tone", um, 60.0f); diodesV.referTo (state, "diodes", um, 0.0f); symmetryV.referTo (state, "symmetry", um, 0.0f);
        outputV.referTo (state, "output", um, -10.0f); mixV.referTo (state, "mix", um, 100.0f); qualityV.referTo (state, "quality", um, 1.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);

        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        drive    = addParam ("drive",    TRANS("Drive"),    { 0.0f, 100.0f }, pct, num);
        tone     = addParam ("tone",     TRANS("Tone"),     { 0.0f, 100.0f }, [] (float v) { const double hz = toneHz (v); return hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }, num);
        diodes   = addParam ("diodes",   TRANS("Diodes"),   { 0.0f, 2.0f, 1.0f }, [] (float v) { return diodeNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; },
                             [] (const juce::String& s) { return s.containsIgnoreCase ("soft") ? 1.0f : s.containsIgnoreCase ("sharp") ? 2.0f : s.containsIgnoreCase ("sil") ? 0.0f : (float) juce::jlimit (0, 2, juce::roundToInt (s.getFloatValue())); });
        symmetry = addParam ("symmetry", TRANS("Symmetry"), { 0.0f, 1.0f, 1.0f }, [] (float v) { return juce::String (v >= 0.5f ? "Lopsided" : "Even"); }, [] (const juce::String& s) { return s.containsIgnoreCase ("lop") || s.containsIgnoreCase ("asym") || s.getFloatValue() >= 0.5f ? 1.0f : 0.0f; });
        output   = addParam ("output",   TRANS("Output"),   { -36.0f, 12.0f }, db, num);
        mix      = addParam ("mix",      TRANS("Mix"),      { 0.0f, 100.0f }, pct, num);
        quality  = addParam ("quality",  TRANS("Quality"),  { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        drive->attachToCurrentValue (driveV); tone->attachToCurrentValue (toneV); diodes->attachToCurrentValue (diodesV); symmetry->attachToCurrentValue (symmetryV);
        output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageOverdrivePlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { drive.get(), tone.get(), diodes.get(), symmetry.get(), output.get(), mix.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Overdrive"); }
    static constexpr const char* xmlTypeName = "sqmVoltageOverdrive";
    static constexpr int delaySamples = 96;
    static constexpr double stageVolts = 0.5;           // a full-scale sample at the stage's input

    juce::String getName() const override               { return "Voltage Overdrive"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VOvd"; }
    juce::String getSelectableDescription() override    { return TRANS("Overdrive: diodes in an amplifier's feedback (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray diodeNames() { return { "Silicon", "Soft", "Sharp" }; }
    /** Tone to the low-pass corner after the stage: 723 Hz at 0 % (the fixed filter such pedals have), 20 kHz at 100 %. */
    static double toneHz (float pct) { return 723.0 * std::pow (20000.0 / 723.0, juce::jlimit (0.0, 1.0, (double) pct / 100.0)); }
    /** Drive to the feedback resistance: 51 k fixed plus a 500 k control with a logarithmic sweep. */
    static double feedbackOhms (float pct) { const double x = juce::jlimit (0.0, 1.0, (double) pct / 100.0); return SqDrive::fixedOhms + SqDrive::driveOhms * (std::pow (10.0, 2.0 * x) - 1.0) / 99.0; }

    static void partsFor (int unitSeed, int channel, int diodeIx, bool lopsided, float drivePercent, SqClip::Diode& d, SqDrive::Circuit& c)
    {
        auto off = [unitSeed, channel] (int which, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (channel + 1) * 40503u ^ (juce::uint32) (which + 431) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        d = diodeIx == 1 ? SqClip::schottky() : diodeIx == 2 ? SqClip::sharp() : SqClip::silicon(); c = {}; c.lopsided = lopsided;
        c.rg *= off (0, 0.02); c.cg *= off (1, 0.05); c.cf *= off (2, 0.05); c.rf = feedbackOhms (drivePercent) * off (3, 0.02); d.is *= off (4, 0.20);
    }
    /** The plug-in's full scale in volts at the stage's output: a full-scale input plus what the diodes hold at the current it pushes. */
    static double ceilingVolts (const SqClip::Diode& d, const SqDrive::Circuit& c)
    {
        const double held = SqClip::kneeVolts (d, stageVolts / c.rg); return stageVolts + (c.lopsided ? 2.0 * held : held);
    }

    /** What the stage does to a 1 kHz test tone at these settings, from the same circuit the audio runs through. */
    struct Reading
    {
        std::vector<float> wave;                     // two cycles of the output, where 1 is full scale
        double harmonics[4] = { -200, -200, -200, -200 };      // 2nd to 5th, dB below the tone
        double kneeVolts = 0, gainDb = 0, gainTimes = 0, bassHz = 0, topHz = 0, peakDb = 0, levelDb = 0;      // gainDb: a quiet 1 kHz tone, in to out before Output and Tone
    };
    static Reading measure (int unitSeed, int diodeIx, bool lopsided, float drivePercent, float testAmp = 0.25f)
    {
        Reading r; SqClip::Diode d; SqDrive::Circuit c; partsFor (unitSeed, 0, diodeIx, lopsided, drivePercent, d, c); const double ceiling = ceilingVolts (d, c);
        r.kneeVolts = SqClip::kneeVolts (d); r.gainTimes = SqDrive::Stage::smallGain (c, 1000.0); r.gainDb = 20.0 * std::log10 (r.gainTimes * stageVolts / ceiling);
        r.bassHz = 1.0 / (juce::MathConstants<double>::twoPi * c.rg * c.cg); r.topHz = 1.0 / (juce::MathConstants<double>::twoPi * c.rf * c.cf);
        const double fs = 384000.0, volts = stageVolts * testAmp; const int settle = 7680, n = 3840;
        SqDrive::Stage k; k.prepare (fs, d, c); std::vector<double> y ((size_t) n); double pk = 0;
        for (int i = 0; i < settle + n; ++i) { const double v = k.process (volts * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / fs)) / ceiling; if (i >= settle) { y[(size_t) (i - settle)] = v; pk = std::max (pk, std::abs (v)); } }
        double mag[6] = {};
        for (int h = 1; h <= 5; ++h) { std::complex<double> a = 0; for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -juce::MathConstants<double>::twoPi * (h * 10.0) * i / n); mag[h] = std::abs (a) / (n / 2.0); }
        for (int h = 2; h <= 5; ++h) r.harmonics[h - 2] = 20.0 * std::log10 (mag[h] / juce::jmax (1.0e-12, mag[1]) + 1.0e-12);
        r.peakDb = 20.0 * std::log10 (pk + 1.0e-12); r.levelDb = 20.0 * std::log10 (mag[1] + 1.0e-12);
        r.wave.resize (384); for (int i = 0; i < 384; ++i) r.wave[(size_t) i] = (float) y[(size_t) (i * 2)];
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        for (int ch = 0; ch < 2; ++ch) { for (int q = 0; q < 3; ++q) chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0; chan[ch].lp = 0.0; }
        usedQuality = -1; refresh (true);
    }
    void deinitialise() override {}
    void reset() override { usedQuality = -1; refresh (true); for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); c.lp = 0.0; } }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        refresh (false);
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = usedQuality;
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        const double lpA = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * toneHz (tone->getCurrentValue()) / sampleRate);
        for (int ch = 0; ch < nch; ++ch)
        {
            auto& c = chan[ch]; auto* d = buf.getWritePointer (ch, fc.bufferStartSample); const double back = 1.0 / c.ceiling;
            for (int i = 0; i < fc.bufferNumSamples; ++i)
            {
                const float in = d[i];
                const double v = c.os[q].process ((double) in * stageVolts, [&c] (double x) { return c.stage.process (x); }) * back;
                c.lp += lpA * (v - c.lp);
                const float delayed = c.dry[(size_t) c.dryPos]; c.dry[(size_t) c.dryPos] = in; if (++c.dryPos >= delaySamples) c.dryPos = 0;
                d[i] = (delayed + wet * ((float) c.lp - delayed)) * outGain;
            }
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, driveV, toneV, diodesV, symmetryV, outputV, mixV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> driveV, toneV, diodesV, symmetryV, outputV, mixV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr drive, tone, diodes, symmetry, output, mix, quality;

private:
    void refresh (bool force)
    {
        const int q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue())), dk = juce::jlimit (0, 2, juce::roundToInt (diodes->getCurrentValue())), sy = symmetry->getCurrentValue() >= 0.5f ? 1 : 0; const float dr = drive->getCurrentValue();
        const bool rate = force || q != usedQuality, parts = rate || dk != usedDiodes || sy != usedSymmetry || std::abs (dr - usedDrive) > 0.01f;
        if (! parts) return;
        for (int ch = 0; ch < 2; ++ch)
        {
            SqClip::Diode d; SqDrive::Circuit c; partsFor (unit.get(), ch, dk, sy == 1, dr, d, c);
            if (rate) chan[ch].stage.prepare (sampleRate * SqVoltagePlugin::factorFor (q), d, c); else chan[ch].stage.setParts (d, c);
            chan[ch].ceiling = ceilingVolts (d, c);
        }
        usedQuality = q; usedDiodes = dk; usedSymmetry = sy; usedDrive = dr;
    }

    struct Channel { SqDrive::Stage stage; SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; double ceiling = 1.0, lp = 0.0; };
    Channel chan[2]; double sampleRate = 44100.0; int usedQuality = -1, usedDiodes = -1, usedSymmetry = -1; float usedDrive = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageOverdrivePlugin)
};
