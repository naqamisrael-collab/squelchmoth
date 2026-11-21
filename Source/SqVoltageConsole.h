#pragma once
// Squelchmoth 1.41 - Voltage Console: a console's transformer and bus crosstalk, as a plug-in (see SqConsole.h).
// On a track it is the channel's input transformer; on a bus or the master it is the summing stage's output transformer
// with the two rails leaking a little into each other. Drive is level-matched. Iron is the transformer's size: a small
// one distorts the bass sooner and lets it fall away earlier. No oversampling and NO added delay: the iron bends gently
// enough that aliasing stays more than 100 dB down at the session's own rate, so it can sit on every track while tracking.
// Options > Console Mode puts one on every track and one on the master in a single step (ConsoleMode.h).

#include <JuceHeader.h>
#include "SqConsole.h"

namespace te = tracktion;

class SqVoltageConsolePlugin : public te::Plugin
{
public:
    SqVoltageConsolePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        driveV.referTo (state, "drive", um, 0.0f); ironV.referTo (state, "iron", um, 50.0f); crosstalkV.referTo (state, "crosstalk", um, -120.0f);
        outputV.referTo (state, "output", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);

        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        drive     = addParam ("drive",     TRANS("Drive"),     { -12.0f, 18.0f }, db, num);
        iron      = addParam ("iron",      TRANS("Iron"),      { 0.0f, 100.0f }, pct, num);
        crosstalk = addParam ("crosstalk", TRANS("Crosstalk"), { -120.0f, -50.0f }, [] (float v) { return v <= -119.0f ? juce::String ("Off") : juce::String (v, 0) + " dB"; },
                              [] (const juce::String& s) { return s.containsIgnoreCase ("off") ? -120.0f : juce::jlimit (-120.0f, -50.0f, -std::abs (s.retainCharacters ("0123456789.").getFloatValue())); });
        output    = addParam ("output",    TRANS("Output"),    { -24.0f, 24.0f }, db, num);
        mix       = addParam ("mix",       TRANS("Mix"),       { 0.0f, 100.0f }, pct, num);
        drive->attachToCurrentValue (driveV); iron->attachToCurrentValue (ironV); crosstalk->attachToCurrentValue (crosstalkV); output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV);
    }

    ~SqVoltageConsolePlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { drive.get(), iron.get(), crosstalk.get(), output.get(), mix.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Console"); }
    static constexpr const char* xmlTypeName = "sqmVoltageConsole";
    static constexpr double voltsAtFullScale = 5.0;     // a full-scale sample is five volts into the transformer before Drive

    juce::String getName() const override               { return "Voltage Console"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VCon"; }
    juce::String getSelectableDescription() override    { return TRANS("Console transformer and bus crosstalk"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    /** This unit's iron: the size asked for, a little off nominal (the same every time for the same unit and channel). */
    static SqTube::Core ironFor (int unitSeed, int channel, float ironPercent)
    {
        auto off = [unitSeed, channel] (int which, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (channel + 1) * 40503u ^ (juce::uint32) (which + 51) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        auto k = SqConsole::ironFor (ironPercent); k.lair *= off (0, 0.05); k.k *= off (1, 0.05); k.ms *= off (2, 0.03); return k;
    }

    /** What the transformer does with these settings, from the same model the audio runs through. */
    struct Reading
    {
        std::vector<float> wave;                     // two cycles of a 50 Hz test tone as it comes out, level-matched
        double third50 = -200, third1k = -200;       // 3rd harmonic, dB below the tone
        double fluxPercent = 0, cornerHz = 0;        // how far the iron is driven by the 50 Hz tone; where quiet bass is 3 dB down
    };
    static Reading measure (int unitSeed, float ironPercent, float driveDb, float testAmp = 0.9f)
    {
        Reading r; const auto k = ironFor (unitSeed, 0, ironPercent); const SqTube::LineCircuit c; const double fs = 48000.0, volts = voltsAtFullScale * std::pow (10.0, (double) driveDb / 20.0) * testAmp;
        r.cornerHz = SqTube::LineTransformer::bassCorner (k, c);
        for (int which = 0; which < 2; ++which)
        {
            const double hz = which == 0 ? 50.0 : 1000.0; const int settle = 24000, n = 1920; SqTube::LineTransformer t; t.prepare (fs, k, c); std::vector<double> y ((size_t) n); double pk = 0;
            for (int i = 0; i < settle + n; ++i) { const double v = t.process (volts * std::sin (juce::MathConstants<double>::twoPi * hz * i / fs)); if (i >= settle) { y[(size_t) (i - settle)] = v / (SqTube::LineTransformer::passGain (c) * volts / testAmp); pk = std::max (pk, std::abs (t.flux())); } }
            const double cyc = hz * n / fs; std::complex<double> a1 = 0, a3 = 0;
            for (int i = 0; i < n; ++i) { a1 += y[(size_t) i] * std::polar (1.0, -juce::MathConstants<double>::twoPi * cyc * i / n); a3 += y[(size_t) i] * std::polar (1.0, -juce::MathConstants<double>::twoPi * 3.0 * cyc * i / n); }
            (which == 0 ? r.third50 : r.third1k) = 20.0 * std::log10 (std::abs (a3) / juce::jmax (1.0e-30, std::abs (a1)) + 1.0e-15);
            if (which == 0) { r.fluxPercent = 100.0 * pk / t.fluxAtSaturation(); r.wave.resize (384); for (int i = 0; i < 384; ++i) r.wave[(size_t) i] = (float) y[(size_t) juce::jmin (n - 1, i * 5)]; }      // 1920 samples = two cycles of 50 Hz
        }
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0; usedIron = -1.0f;
        for (int ch = 0; ch < 2; ++ch) chan[ch].prepare (sampleRate, ironFor (unit.get(), ch, iron->getCurrentValue()), {});
        usedIron = iron->getCurrentValue(); leak.prepare (sampleRate); leak.set (crosstalk->getCurrentValue());
    }
    void deinitialise() override {}
    void reset() override { for (auto& c : chan) c.reset(); leak.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        if (std::abs (iron->getCurrentValue() - usedIron) > 0.01f) { usedIron = iron->getCurrentValue(); for (int ch = 0; ch < 2; ++ch) chan[ch].setParts (ironFor (unit.get(), ch, usedIron), {}); }
        if (crosstalk->getCurrentValue() != usedLeak) { usedLeak = crosstalk->getCurrentValue(); leak.set (usedLeak); }
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels());
        const double inVolts = voltsAtFullScale * std::pow (10.0, (double) drive->getCurrentValue() / 20.0), back = 1.0 / (SqTube::LineTransformer::passGain ({}) * inVolts);
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        auto* l = buf.getWritePointer (0, fc.bufferStartSample); auto* r = nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample) : nullptr;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float inL = l[i], inR = r != nullptr ? r[i] : 0.0f;
            double a = chan[0].process ((double) inL * inVolts) * back, b = r != nullptr ? chan[1].process ((double) inR * inVolts) * back : 0.0;
            if (r != nullptr) leak.process (a, b);
            l[i] = (inL + wet * ((float) a - inL)) * outGain; if (r != nullptr) r[i] = (inR + wet * ((float) b - inR)) * outGain;
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, driveV, ironV, crosstalkV, outputV, mixV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> driveV, ironV, crosstalkV, outputV, mixV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr drive, iron, crosstalk, output, mix;

private:
    SqTube::LineTransformer chan[2]; SqConsole::Crosstalk leak; double sampleRate = 44100.0; float usedIron = -1.0f, usedLeak = -999.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageConsolePlugin)
};
