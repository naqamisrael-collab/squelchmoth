#pragma once
// Squelchmoth 1.44 - Voltage Opto: an optical levelling compressor (see SqOpto.h), as a plug-in.
// A lamp panel shines on a light-dependent resistor that turns the signal down. There are no attack or release
// controls, because the cell sets them itself: it answers in about 10 ms, lets go of half its reduction in about
// 60 ms and of the rest over seconds - longer the longer and harder it was lit. Reduction is how hard the sidechain
// drives the panel; Gain makes up the level. Compress is a soft squeeze; Limit holds the panel's driver off until the
// signal passes a threshold and then brings the light up steeply. The two channels sit under one panel, so the
// stereo picture holds still. Every instance has its own pair of cells.

#include <JuceHeader.h>
#include "SqOpto.h"
#include "SqTube.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageOptoPlugin : public te::Plugin
{
public:
    SqVoltageOptoPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        reduceV.referTo (state, "reduce", um, 40.0f); gainV.referTo (state, "gain", um, 0.0f); modeV.referTo (state, "mode", um, 0.0f);
        mixV.referTo (state, "mix", um, 100.0f); qualityV.referTo (state, "quality", um, 0.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);
        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        reduce  = addParam ("reduce",  TRANS("Reduction"), { 0.0f, 100.0f }, [] (float v) { return juce::String (juce::roundToInt (v)); }, num);
        gain    = addParam ("gain",    TRANS("Gain"),      { -12.0f, 24.0f }, db, num);
        mode    = addParam ("mode",    TRANS("Mode"),      { 0.0f, 1.0f, 1.0f }, [] (float v) { return modeNames()[v >= 0.5f ? 1 : 0]; },
                            [] (const juce::String& s) { return s.trim().startsWithIgnoreCase ("l") || s.getIntValue() == 1 ? 1.0f : 0.0f; });
        mix     = addParam ("mix",     TRANS("Mix"),       { 0.0f, 100.0f }, pct, num);
        quality = addParam ("quality", TRANS("Quality"),   { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        reduce->attachToCurrentValue (reduceV); gain->attachToCurrentValue (gainV); mode->attachToCurrentValue (modeV); mix->attachToCurrentValue (mixV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageOptoPlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { reduce.get(), gain.get(), mode.get(), mix.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Opto"); }
    static constexpr const char* xmlTypeName = "sqmVoltageOpto";
    static constexpr int delaySamples = 96;

    juce::String getName() const override               { return "Voltage Opto"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VOpto"; }
    juce::String getSelectableDescription() override    { return TRANS("Optical compressor (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray modeNames() { return { "Compress", "Limit" }; }
    /** Reduction 0 to 100: the sidechain's gain, from off through -12 dB to +30 dB. */
    static double driveFor (float pct)
    {
        const double p = juce::jlimit (0.0, 100.0, (double) pct); if (p <= 0.0) return 0.0;
        return std::pow (10.0, (-12.0 + 0.42 * p) / 20.0) * juce::jmin (1.0, p / 5.0);
    }
    static SqOpto::Cell cellFor (int unitSeed, int channel)
    {
        auto off = [unitSeed] (int which, int ch, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (ch + 1) * 40503u ^ (juce::uint32) (which + 131) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        SqOpto::Cell c; c.sense = off (0, 0, 0.15) * off (1, channel, 0.03); return c;       // cells vary a good deal from one to the next (data sheets: nearly two to one); a pair is matched
    }

    /** What the compressor does at these settings, from the same equations the audio runs through (at a lower rate, for speed). */
    struct Reading
    {
        float in[9] = {}, out[9] = {};       // steady 1 kHz tones: level in and level out before Gain, dBFS
        double ratio = 0, reductionAtFull = 0;   // over the top 12 dB; for a full-scale tone
        double attackMs = 0, halfReleaseMs = 0, backSeconds = 0;      // for a full-scale tone held two seconds
    };
    static float inAt (int i) { return -48.0f + 6.0f * (float) i; }
    static Reading measure (int unitSeed, bool limit, float reducePct)
    {
        Reading r; const double fs = 24000.0, drive = driveFor (reducePct), bias = limit ? SqOpto::limitBias() : 0.0; const auto cell = cellFor (unitSeed, 0);
        auto steady = [&] (double levelDb)
        {
            SqOpto::Compressor c; c.prepare (fs, cell, cell); c.set (drive, bias); const double amp = std::pow (10.0, levelDb / 20.0); const int n = (int) (2.5 * fs); double pk = 0;
            for (int i = 0; i < n; ++i) { const double v = c.process (amp * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / fs)); if (i > n - 240) pk = std::max (pk, std::abs (v)); }
            return 20.0 * std::log10 (pk + 1.0e-15);
        };
        for (int i = 0; i < 9; ++i) { r.in[i] = inAt (i); r.out[i] = (float) steady ((double) inAt (i)); }
        r.ratio = 12.0 / juce::jmax (0.05, (double) (r.out[8] - r.out[6])); r.reductionAtFull = (double) r.out[8];
        {
            SqOpto::Compressor c; c.prepare (fs, cell, cell); c.set (drive, bias); const int on = (int) (2.0 * fs), total = (int) (22.0 * fs); double at100 = 0, atEnd = 0; std::vector<float> gr; gr.reserve ((size_t) (total / 24 + 1));
            for (int i = 0; i < total; ++i)
            {
                c.process (i < on ? std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / fs) : 0.0);
                if (i == (int) (0.1 * fs)) at100 = c.reductionDb(); if (i == on - 1) atEnd = c.reductionDb();
                if (i % 24 == 0) { gr.push_back ((float) c.reductionDb()); if (i > on && gr.back() > -0.5f) break; }
            }
            const int onMs = 2000; int a = 0; while (a < (int) gr.size() - 1 && gr[(size_t) a] > 0.632 * at100) ++a; r.attackMs = a;
            int h = onMs; while (h < (int) gr.size() - 1 && gr[(size_t) h] < 0.5 * atEnd) ++h; r.halfReleaseMs = h - onMs; r.backSeconds = juce::jmax (0, (int) gr.size() - 1 - onMs) / 1000.0;
            if (atEnd > -0.5) { r.attackMs = 0; r.halfReleaseMs = 0; r.backSeconds = 0; }
        }
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        for (int ch = 0; ch < 2; ++ch) { for (int q = 0; q < 3; ++q) chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0; }
        for (int q = 0; q < 3; ++q) comp[q].prepare (sampleRate * SqVoltagePlugin::factorFor (q), cellFor (unit.get(), 0), cellFor (unit.get(), 1));
        reduction.store (0.0f);
    }
    void deinitialise() override {}
    void reset() override { for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); } for (auto& c : comp) c.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue()));
        auto& c = comp[q]; c.set (driveFor (reduce->getCurrentValue()), mode->getCurrentValue() >= 0.5f ? SqOpto::limitBias() : 0.0); if (nch < 2) c.clearRight();
        const float outGain = std::pow (10.0f, gain->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        auto* l = buf.getWritePointer (0, fc.bufferStartSample); auto* r = nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample) : nullptr;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float inL = l[i]; int k = 0;
            const double a = chan[0].os[q].process ((double) inL, [&] (double x) { return c.left (x, k++); });
            { auto& d = chan[0]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inL; if (++d.dryPos >= delaySamples) d.dryPos = 0; l[i] = (delayed + wet * ((float) a - delayed)) * outGain; }
            if (r != nullptr)
            {
                const float inR = r[i]; k = 0; const double b = chan[1].os[q].process ((double) inR, [&] (double x) { return c.right (x, k++); });
                auto& d = chan[1]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inR; if (++d.dryPos >= delaySamples) d.dryPos = 0; r[i] = (delayed + wet * ((float) b - delayed)) * outGain;
            }
        }
        reduction.store ((float) c.reductionDb()); memory.store ((float) c.slowState());
    }

    /** Gain reduction right now in dB (0 or below), and how full the cell's slow memory is, for the face. */
    float currentReductionDb() const { return reduction.load(); }
    float currentMemory() const { return memory.load(); }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, reduceV, gainV, modeV, mixV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> reduceV, gainV, modeV, mixV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr reduce, gain, mode, mix, quality;

private:
    struct Channel { SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; };
    Channel chan[2]; SqOpto::Compressor comp[3]; double sampleRate = 44100.0; std::atomic<float> reduction { 0.0f }, memory { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageOptoPlugin)
};
