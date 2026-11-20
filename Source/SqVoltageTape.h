#pragma once
// Squelchmoth 1.40 - Voltage Tape: a tape machine from its parts (see SqTape.h), as a plug-in.
// Drive sets how hard the tape is hit (level-matched: it changes colour, not loudness, until the tape saturates).
// Bias runs from none (the tape ignores quiet sound and distorts) through the working range to overbias (cleaner).
// Speed and EQ pick the standard's recorded-flux curve and set the wavelengths, and with them the top end and where
// the head bump sits. Wow and Flutter are the transport's speed wandering - one transport for both channels.
// Every instance is its own machine: its transport wanders its own way, and its two tracks differ a little.

#include <JuceHeader.h>
#include "SqTape.h"
#include "SqTube.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageTapePlugin : public te::Plugin
{
public:
    SqVoltageTapePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        driveV.referTo (state, "drive", um, 0.0f); biasV.referTo (state, "bias", um, 50.0f); speedV.referTo (state, "speed", um, 1.0f); eqV.referTo (state, "eq", um, 0.0f);
        wowV.referTo (state, "wow", um, 0.05f); flutterV.referTo (state, "flutter", um, 0.03f); bumpV.referTo (state, "bump", um, 50.0f);
        outputV.referTo (state, "output", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f); qualityV.referTo (state, "quality", um, 0.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);

        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto fine = [] (float v) { return juce::String (v, 2) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        drive   = addParam ("drive",   TRANS("Drive"),   { -12.0f, 24.0f }, db, num);
        bias    = addParam ("bias",    TRANS("Bias"),    { 0.0f, 100.0f }, pct, num);
        speed   = addParam ("speed",   TRANS("Speed"),   { 0.0f, 2.0f, 1.0f }, [] (float v) { return speedNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; },
                            [] (const juce::String& s) { const float n = s.retainCharacters ("0123456789.").getFloatValue(); return n >= 29.0f ? 2.0f : n >= 14.0f ? 1.0f : n >= 7.0f ? 0.0f : (float) juce::jlimit (0, 2, juce::roundToInt (n)); });
        eq      = addParam ("eq",      TRANS("EQ"),      { 0.0f, 1.0f, 1.0f }, [] (float v) { return juce::String (v >= 0.5f ? "IEC" : "NAB"); }, [] (const juce::String& s) { return s.containsIgnoreCase ("iec") || s.containsIgnoreCase ("ccir") || s.getFloatValue() >= 0.5f ? 1.0f : 0.0f; });
        wow     = addParam ("wow",     TRANS("Wow"),     { 0.0f, 1.0f }, fine, num);
        flutter = addParam ("flutter", TRANS("Flutter"), { 0.0f, 0.5f }, fine, num);
        bump    = addParam ("bump",    TRANS("Head Bump"), { 0.0f, 100.0f }, pct, num);
        output  = addParam ("output",  TRANS("Output"),  { -24.0f, 24.0f }, db, num);
        mix     = addParam ("mix",     TRANS("Mix"),     { 0.0f, 100.0f }, pct, num);
        quality = addParam ("quality", TRANS("Quality"), { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        drive->attachToCurrentValue (driveV); bias->attachToCurrentValue (biasV); speed->attachToCurrentValue (speedV); eq->attachToCurrentValue (eqV); wow->attachToCurrentValue (wowV);
        flutter->attachToCurrentValue (flutterV); bump->attachToCurrentValue (bumpV); output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageTapePlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { drive.get(), bias.get(), speed.get(), eq.get(), wow.get(), flutter.get(), bump.get(), output.get(), mix.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Tape"); }
    static constexpr const char* xmlTypeName = "sqmVoltageTape";

    juce::String getName() const override               { return "Voltage Tape"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VTape"; }
    juce::String getSelectableDescription() override    { return TRANS("Tape machine (from its parts)"); }
    bool needsConstantBufferSize() override             { return false; }
    /** The oversampler (96), the replay filter and the tape path's resting delay; it depends on the sample rate. */
    static int delaySamplesAt (double rate)             { return 96 + SqTape::Channel::latency (rate); }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamplesAt (sampleRate) / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray speedNames() { return { "7.5 in/s", "15 in/s", "30 in/s" }; }
    /** Bias control to bias field: none at 0 %, 0.2 at 50 % (well inside the working range), 1.0 at 100 % (overbias). */
    static double biasField (float pct) { return std::pow (juce::jlimit (0.0, 1.0, (double) pct / 100.0), 2.3219); }

    static SqTape::Settings settingsFor (int unitSeed, int channel, float driveDb, float biasPercent, int speedIx, int eqIx, float bumpPercent)
    {
        SqTape::Settings s;
        auto off = [unitSeed, channel] (int which, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (channel + 1) * 40503u ^ (juce::uint32) (which + 31) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        s.fieldAtFullScale = SqTape::Channel::referenceField * std::pow (10.0, (double) driveDb / 20.0) * off (0, 0.015);      // the two tracks are not recorded at quite the same level
        s.bias = biasField (biasPercent) * off (1, 0.03);
        s.speed = speedIx <= 0 ? SqTape::Speed::ips7_5 : speedIx == 1 ? SqTape::Speed::ips15 : SqTape::Speed::ips30;
        s.eq = eqIx >= 1 ? SqTape::Eq::iec : SqTape::Eq::nab; s.bump = juce::jlimit (0.0, 1.0, (double) bumpPercent / 100.0);
        s.head.gap *= off (2, 0.05); s.head.spacing *= off (3, 0.10);
        return s;
    }

    /** What the machine does with these settings, from the same parts the audio runs through. */
    struct Reading
    {
        std::vector<float> wave;                 // two cycles of a test sine as the tape keeps it, level-matched
        double third = -200, fifth = -200, levelDb = 0;      // harmonics (dB below the tone) and how much of the tone's level is left
        double at50 = 0, at10k = 0, at20k = 0;   // response in dB against 1 kHz
        double biasField = 0; int biasState = 0; // 0 under, 1 working range, 2 over
        double highUs = 0, lowUs = 0;
    };
    static Reading measure (int unitSeed, float driveDb, float biasPercent, int speedIx, int eqIx, float bumpPercent, float testAmp = 0.9f)
    {
        Reading r; const auto s = settingsFor (unitSeed, 0, driveDb, biasPercent, speedIx, eqIx, bumpPercent);
        SqTape::Channel c; c.prepare (48000.0, 4); c.set (s);
        // a 1 kHz test tone: the flux curve sets how hard it reaches the tape, and lifts its harmonics again on the way back
        double hi, lo; SqTape::timeConstants (s.speed, s.eq, hi, lo); r.highUs = hi * 1.0e6; r.lowUs = lo * 1.0e6;
        auto flux = [hi, lo] (double hz) { return SqTape::fluxLevel (hz, hi, lo); };
        const double field = s.fieldAtFullScale * testAmp * flux (1000.0), gain = c.tape().sineGain (field) * c.matchGain();
        r.third = c.tape().harmonicDb (field, 3) + 20.0 * std::log10 (flux (1000.0) / flux (3000.0) * c.response (3000.0) / c.response (1000.0));
        r.fifth = c.tape().harmonicDb (field, 5) + 20.0 * std::log10 (flux (1000.0) / flux (5000.0) * c.response (5000.0) / c.response (1000.0));
        r.levelDb = 20.0 * std::log10 (gain + 1.0e-12);
        r.wave.resize (384); for (int i = 0; i < 384; ++i) r.wave[(size_t) i] = (float) (c.tape() (field * std::sin (juce::MathConstants<double>::twoPi * i / 192.0)) * c.matchGain() / s.fieldAtFullScale);
        const double ref = c.response (1000.0); auto dbAt = [&] (double hz) { return 20.0 * std::log10 (c.response (hz) / ref); };
        r.at50 = dbAt (50.0); r.at10k = dbAt (10000.0); r.at20k = dbAt (20000.0);
        r.biasField = s.bias; r.biasState = s.bias < 0.03 ? 0 : s.bias <= 0.35 ? 1 : 2;
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0; delaySamples = delaySamplesAt (sampleRate);
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int q = 0; q < 3; ++q) { chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].tape[q].prepare (sampleRate, SqVoltagePlugin::factorFor (q)); }
            chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0;
        }
        for (int q = 0; q < 3; ++q) transport[q].prepare (sampleRate * SqVoltagePlugin::factorFor (q), unit.get());
        late.assign (16, 0.0);
        refresh();
    }
    void deinitialise() override {}
    void reset() override { for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); for (auto& t : c.tape) t.reset(); } }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        refresh();
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue())), factor = SqVoltagePlugin::factorFor (q);
        transport[q].set ((double) wow->getCurrentValue() / 100.0, (double) flutter->getCurrentValue() / 100.0);
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            for (int k = 0; k < factor; ++k) late[(size_t) k] = transport[q].next();      // one transport: both tracks get the same wandering
            for (int ch = 0; ch < nch; ++ch)
            {
                auto& c = chan[ch]; auto* d = buf.getWritePointer (ch, fc.bufferStartSample); const float in = d[i]; int k = 0;
                const double v = c.tape[q].processOuter (c.os[q].process ((double) in, [&] (double x) { return c.tape[q].processInner (x, late[(size_t) k++]); }));
                const float delayed = c.dry[(size_t) c.dryPos]; c.dry[(size_t) c.dryPos] = in; if (++c.dryPos >= delaySamples) c.dryPos = 0;
                d[i] = (delayed + wet * ((float) v - delayed)) * outGain;
            }
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, driveV, biasV, speedV, eqV, wowV, flutterV, bumpV, outputV, mixV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> driveV, biasV, speedV, eqV, wowV, flutterV, bumpV, outputV, mixV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr drive, bias, speed, eq, wow, flutter, bump, output, mix, quality;

private:
    void refresh()
    {
        const int sp = juce::jlimit (0, 2, juce::roundToInt (speed->getCurrentValue())), e = eq->getCurrentValue() >= 0.5f ? 1 : 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto s = settingsFor (unit.get(), ch, drive->getCurrentValue(), bias->getCurrentValue(), sp, e, bump->getCurrentValue());
            for (auto& t : chan[ch].tape) t.set (s);      // set() does nothing when nothing changed
        }
    }

    struct Channel { SqTape::Channel tape[3]; SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; };
    Channel chan[2]; SqTape::Transport transport[3]; std::vector<double> late; double sampleRate = 44100.0; int delaySamples = 96;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageTapePlugin)
};
