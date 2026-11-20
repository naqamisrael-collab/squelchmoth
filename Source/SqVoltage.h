#pragma once
// Squelchmoth 1.37 - Voltage: a triode gain stage solved as a circuit (see SqTube.h), as a plug-in.
// Drive sets how hard the stage is hit, Bias its cathode resistor (warm to cold), Cathode Cap the bypass capacitor
// (more gain and an earlier, rounder overload with it; cleaner and tighter without), Tube the triode type.
// The level is matched: the stage's own small-signal gain is divided out, so Drive changes colour, not loudness,
// until the stage starts to compress. Every instance is its own unit: its parts sit a little off their nominal
// values (kept with the session), and the left and right channels are two different tubes.

#include <JuceHeader.h>
#include "SqTube.h"
#include <complex>

namespace te = tracktion;

class SqVoltagePlugin : public te::Plugin
{
public:
    SqVoltagePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        driveV.referTo (state, "drive", um, 0.0f); biasV.referTo (state, "bias", um, 45.0f); capV.referTo (state, "cathodeCap", um, 1.0f);
        tubeV.referTo (state, "tube", um, 0.0f); outputV.referTo (state, "output", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f);
        quality.referTo (state, "quality", um, 0); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);      // this unit's own parts, kept with the session

        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        drive  = addParam ("drive",  TRANS("Drive"),  { -12.0f, 36.0f }, db, num);
        bias   = addParam ("bias",   TRANS("Bias"),   { 0.0f, 100.0f }, pct, num);
        cap    = addParam ("cathodeCap", TRANS("Cathode Cap"), { 0.0f, 1.0f, 1.0f }, [] (float v) { return juce::String (v >= 0.5f ? "In" : "Out"); }, [] (const juce::String& s) { return s.containsIgnoreCase ("in") || s.getFloatValue() >= 0.5f ? 1.0f : 0.0f; });
        tube   = addParam ("tube",   TRANS("Tube"),   { 0.0f, 1.0f, 1.0f }, [] (float v) { return juce::String (v >= 0.5f ? "Medium gain" : "High gain"); }, [] (const juce::String& s) { return s.containsIgnoreCase ("med") || s.getFloatValue() >= 0.5f ? 1.0f : 0.0f; });
        output = addParam ("output", TRANS("Output"), { -24.0f, 24.0f }, db, num);
        mix    = addParam ("mix",    TRANS("Mix"),    { 0.0f, 100.0f }, pct, num);
        drive->attachToCurrentValue (driveV); bias->attachToCurrentValue (biasV); cap->attachToCurrentValue (capV);
        tube->attachToCurrentValue (tubeV); output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV);
    }

    ~SqVoltagePlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { drive.get(), bias.get(), cap.get(), tube.get(), output.get(), mix.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage"); }
    static constexpr const char* xmlTypeName = "sqmVoltage";
    static constexpr int delaySamples = 96;             // the oversampling filters, the same at every quality
    static constexpr double voltsAtFullScale = 0.5;     // a full-scale sample is half a volt at the grid before Drive

    juce::String getName() const override               { return "Voltage"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Volt"; }
    juce::String getSelectableDescription() override    { return TRANS("Triode stage (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray qualityNames() { return { "4x", "8x", "16x" }; }
    static int factorFor (int q) { return q <= 0 ? 4 : q == 1 ? 8 : 16; }

    /** This unit's parts: nominal values moved by its own small tolerances (the same every time for the same unit and channel). */
    static void partsFor (int unitSeed, int channel, bool mediumGain, float biasPercent, bool cathodeCap, SqTube::Tube& t, SqTube::Circuit& c)
    {
        t = mediumGain ? SqTube::mediumGain() : SqTube::highGain(); c = {};
        c.rk = 680.0 * std::pow (3900.0 / 680.0, juce::jlimit (0.0, 1.0, (double) biasPercent / 100.0));
        if (! cathodeCap) c.ck = 0.0;
        auto off = [unitSeed, channel] (int which, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (channel + 1) * 40503u ^ (juce::uint32) (which + 1) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        c.rp *= off (0, 0.02); c.rk *= off (1, 0.02); if (c.ck > 0) c.ck *= off (2, 0.10);
        t.mu *= off (3, 0.03); t.kg1 *= off (4, 0.04);
    }

    /** What the stage does to a test sine with these settings, worked out from the same circuit the audio runs through
        (the display shows this; nothing on the face is a drawn-in curve). */
    struct Reading
    {
        std::vector<float> wave;                 // two cycles of the output, level-matched and the right way up
        double harmonics[4] = { -200, -200, -200, -200 };   // 2nd to 5th, dB below the fundamental (negative numbers)
        double plateVolts = 0, cathodeVolts = 0, gainDb = 0, levelDb = 0;   // resting point; stage gain; output level against the input
        bool gridConducts = false;               // the grid is driven positive on peaks
    };
    static Reading measure (int unitSeed, bool mediumGain, float biasPercent, bool cathodeCap, float driveDb, float testAmp = 0.9f)
    {
        Reading r; SqTube::Tube t; SqTube::Circuit c; partsFor (unitSeed, 0, mediumGain, biasPercent, cathodeCap, t, c);
        const double fs = 192000.0, hz = 1000.0, volts = voltsAtFullScale * std::pow (10.0, (double) driveDb / 20.0) * testAmp;
        SqTube::Stage s; s.prepare (fs, t, c); r.plateVolts = s.restingPlate(); r.cathodeVolts = s.restingCathode();
        const double gain = SqTube::Stage::smallSignalGain (t, c); r.gainDb = 20.0 * std::log10 (std::abs (gain) + 1.0e-12);
        r.gridConducts = volts > r.cathodeVolts;
        const int settle = (int) (0.08 * fs), n = 1920;          // ten cycles of 1 kHz
        std::vector<double> y ((size_t) n);
        for (int i = 0; i < settle + n; ++i) { const double v = s.process (volts * std::sin (juce::MathConstants<double>::twoPi * hz * i / fs)); if (i >= settle) y[(size_t) (i - settle)] = v / (gain * voltsAtFullScale * std::pow (10.0, (double) driveDb / 20.0)); }
        double mag[6] = {};
        for (int k = 1; k <= 5; ++k) { std::complex<double> a = 0; for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -juce::MathConstants<double>::twoPi * (k * 10.0) * i / n); mag[k] = std::abs (a) / (n / 2.0); }
        for (int k = 2; k <= 5; ++k) r.harmonics[k - 2] = 20.0 * std::log10 (mag[k] / juce::jmax (1.0e-12, mag[1]) + 1.0e-12);
        r.levelDb = 20.0 * std::log10 (mag[1] / (double) testAmp + 1.0e-12);
        r.wave.resize (384); for (int i = 0; i < 384; ++i) r.wave[(size_t) i] = (float) y[(size_t) i];       // 384 samples at 192 kHz = two cycles
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int q = 0; q < 3; ++q) chan[ch].os[q].prepare (factorFor (q));
            chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0;
        }
        usedQuality = -1; usedBias = -1.0f; usedCap = usedTube = -1; refresh (true);
    }
    void deinitialise() override {}
    void reset() override { usedQuality = -1; refresh (true); for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); } }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        refresh (false);
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = usedQuality;
        const double inVolts = voltsAtFullScale * std::pow (10.0, (double) drive->getCurrentValue() / 20.0);
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        for (int ch = 0; ch < nch; ++ch)
        {
            auto& c = chan[ch]; auto* d = buf.getWritePointer (ch, fc.bufferStartSample);
            const double back = 1.0 / (c.gain * inVolts);            // the stage's own gain and the drive divided out again (the gain is negative: this also turns the signal the right way up)
            for (int i = 0; i < fc.bufferNumSamples; ++i)
            {
                const float in = d[i];
                const double v = c.os[q].process ((double) in * inVolts, [&c] (double x) { return c.stage.process (x); });
                const float delayed = c.dry[(size_t) c.dryPos]; c.dry[(size_t) c.dryPos] = in; if (++c.dryPos >= delaySamples) c.dryPos = 0;
                d[i] = (delayed + wet * ((float) (v * back) - delayed)) * outGain;
            }
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, driveV, biasV, capV, tubeV, outputV, mixV);
        te::copyPropertiesToCachedValues (v, quality, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> driveV, biasV, capV, tubeV, outputV, mixV;
    juce::CachedValue<int> quality, unit;
    te::AutomatableParameter::Ptr drive, bias, cap, tube, output, mix;

private:
    void refresh (bool force)
    {
        const int q = juce::jlimit (0, 2, quality.get()); const float b = bias->getCurrentValue(); const int cp = cap->getCurrentValue() >= 0.5f ? 1 : 0, tb = tube->getCurrentValue() >= 0.5f ? 1 : 0;
        const bool rate = force || q != usedQuality, parts = rate || std::abs (b - usedBias) > 0.01f || cp != usedCap || tb != usedTube;
        if (! parts) return;
        for (int ch = 0; ch < 2; ++ch)
        {
            SqTube::Tube t; SqTube::Circuit c; partsFor (unit.get(), ch, tb == 1, b, cp == 1, t, c);
            if (rate) chan[ch].stage.prepare (sampleRate * factorFor (q), t, c); else chan[ch].stage.setParts (t, c);
            chan[ch].gain = SqTube::Stage::smallSignalGain (t, c);
        }
        usedQuality = q; usedBias = b; usedCap = cp; usedTube = tb;
    }

    struct Channel { SqTube::Stage stage; SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; double gain = -59.0; };
    Channel chan[2]; double sampleRate = 44100.0; int usedQuality = -1, usedCap = -1, usedTube = -1; float usedBias = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltagePlugin)
};
