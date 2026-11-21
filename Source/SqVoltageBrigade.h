#pragma once
// Squelchmoth 1.46 - Voltage Brigade: a bucket-brigade delay and chorus (see SqBbd.h), as a plug-in.
// The delay comes from a chip that passes each sample along a row of buckets, one step per clock edge:
// delay = stages / (2 x clock). Time moves the clock (between 5 and 100 kHz), so a longer delay is a slower clock:
// darker, and above half the clock rate the sound folds over - the dirt these echoes are loved for. Turning Time
// while it plays bends the pitch, and Rate / Depth wobble the clock for chorus and vibrato. Feedback sends the
// output round again through the chip and both filters, so every repeat is darker and a little more bent; past
// 100 % it feeds itself and howls, held by the buckets' own limit.
// Tone is the corner of the two analog filters round the chip. Every instance has its own chips and capacitors.

#include <JuceHeader.h>
#include "SqBbd.h"
#include "SqTube.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageBrigadePlugin : public te::Plugin
{
public:
    SqVoltageBrigadePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        timeV.referTo (state, "time", um, 180.0f); stagesV.referTo (state, "stages", um, 2.0f); feedbackV.referTo (state, "feedback", um, 35.0f); toneV.referTo (state, "tone", um, 4.5f);
        rateV.referTo (state, "rate", um, 0.5f); depthV.referTo (state, "depth", um, 0.0f); widthV.referTo (state, "width", um, 0.0f);
        mixV.referTo (state, "mix", um, 30.0f); outputV.referTo (state, "output", um, 0.0f); qualityV.referTo (state, "quality", um, 0.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);
        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        juce::NormalisableRange<float> timeRange (1.0f, 820.0f); timeRange.setSkewForCentre (60.0f);
        juce::NormalisableRange<float> rateRange (0.05f, 8.0f); rateRange.setSkewForCentre (1.0f);
        time     = addParam ("time",     TRANS("Time"),     timeRange, [] (float v) { return (v < 10.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms"; }, num);
        stages   = addParam ("stages",   TRANS("Stages"),   { 0.0f, 3.0f, 1.0f }, [] (float v) { return juce::String (stagesAt (juce::roundToInt (v))); },
                             [] (const juce::String& s) { const int n = s.retainCharacters ("0123456789").getIntValue(); return n >= 6000 ? 3.0f : n >= 2500 ? 2.0f : n >= 600 ? 1.0f : n >= 4 ? 0.0f : (float) juce::jlimit (0, 3, n); });
        feedback = addParam ("feedback", TRANS("Feedback"), { 0.0f, 110.0f }, pct, num);
        tone     = addParam ("tone",     TRANS("Tone"),     { 1.5f, 12.0f }, [] (float v) { return juce::String (v, 1) + " kHz"; }, num);
        rate     = addParam ("rate",     TRANS("Rate"),     rateRange, [] (float v) { return juce::String (v, 2) + " Hz"; }, num);
        depth    = addParam ("depth",    TRANS("Depth"),    { 0.0f, 100.0f }, pct, num);
        width    = addParam ("width",    TRANS("Width"),    { 0.0f, 100.0f }, pct, num);
        mix      = addParam ("mix",      TRANS("Mix"),      { 0.0f, 100.0f }, pct, num);
        output   = addParam ("output",   TRANS("Output"),   { -24.0f, 12.0f }, db, num);
        quality  = addParam ("quality",  TRANS("Quality"),  { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        time->attachToCurrentValue (timeV); stages->attachToCurrentValue (stagesV); feedback->attachToCurrentValue (feedbackV); tone->attachToCurrentValue (toneV); rate->attachToCurrentValue (rateV);
        depth->attachToCurrentValue (depthV); width->attachToCurrentValue (widthV); mix->attachToCurrentValue (mixV); output->attachToCurrentValue (outputV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageBrigadePlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { time.get(), stages.get(), feedback.get(), tone.get(), rate.get(), depth.get(), width.get(), mix.get(), output.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Brigade"); }
    static constexpr const char* xmlTypeName = "sqmVoltageBrigade";
    static constexpr int delaySamples = 96;
    static constexpr double fullScaleVolts = 0.5091;     // a full-scale tone is 0.36 V rms at the chip: the level its data sheet gives for 2.5 % distortion
    static constexpr double clockLow = 5000.0, clockHigh = 100000.0, wobbleAtFull = 0.25;      // the clock's range; Depth 100 % swings it a quarter either way

    juce::String getName() const override               { return "Voltage Brigade"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VBrig"; }
    juce::String getSelectableDescription() override    { return TRANS("Bucket-brigade delay and chorus (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static int stagesAt (int ix) { static const int n[4] = { 256, 1024, 4096, 8192 }; return n[juce::jlimit (0, 3, ix)]; }
    /** The clock that gives this delay with this many stages, kept inside the range the chip is run at. */
    static double clockFor (int stageCount, double timeMs) { return juce::jlimit (clockLow, clockHigh, stageCount / (2.0 * juce::jmax (0.01, timeMs) / 1000.0)); }
    static double tolerance (int unitSeed, int what, int n, double spread)
    {
        juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (n + 1) * 40503u ^ (juce::uint32) (what + 211) * 2246822519u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
        return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
    }

    /** What these settings come to, worked out from the same arithmetic the line obeys. */
    struct Reading
    {
        double delayMs = 0, clockKHz = 0, foldHz = 0, reachHz = 0, tailSeconds = 0;      // reachHz: where the first repeat is 3 dB down
        float first[48] = {}, fourth[48] = {};                                             // the first and fourth repeats against frequency (hzAt), dB
    };
    static float hzAt (int i) { return 20.0f * std::pow (1000.0f, (float) i / 47.0f); }
    static double repeatDb (double hz, double clock, double cutoffHz, int repeats, double feedbackFraction)
    {
        const double x = hz / clock, hold = std::abs (x) < 1.0e-9 ? 1.0 : std::abs (std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x));
        const double once = hold / std::sqrt (1.0 + std::pow (hz / cutoffHz, 6.0)) / std::sqrt (1.0 + std::pow (hz / cutoffHz, 10.0));
        return 20.0 * std::log10 (juce::jmax (1.0e-12, std::pow (once, (double) repeats) * std::pow (juce::jmax (1.0e-6, feedbackFraction), (double) (repeats - 1))));
    }
    static Reading measure (int stageIx, float timeMs, float toneKHz, float feedbackPct)
    {
        Reading r; const int n = stagesAt (stageIx); const double clock = clockFor (n, timeMs), cutoff = 1000.0 * (double) toneKHz, fb = (double) feedbackPct / 100.0;
        r.clockKHz = clock / 1000.0; r.delayMs = 1000.0 * n / (2.0 * clock); r.foldHz = clock / 2.0;
        for (int i = 0; i < 48; ++i) { r.first[i] = (float) repeatDb (hzAt (i), clock, cutoff, 1, fb); r.fourth[i] = (float) repeatDb (hzAt (i), clock, cutoff, 4, fb); }
        double lo = 20.0, hi = 20000.0; for (int k = 0; k < 40; ++k) { const double mid = std::sqrt (lo * hi); (repeatDb (mid, clock, cutoff, 1, fb) > -3.0 ? lo : hi) = mid; } r.reachHz = lo;
        r.tailSeconds = fb >= 0.999 ? -1.0 : fb > 0.001 ? r.delayMs / 1000.0 * (1.0 + 60.0 / (-20.0 * std::log10 (fb))) : r.delayMs / 1000.0;       // -1: it feeds itself
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int q = 0; q < 3; ++q) { chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].line[q].prepare (sampleRate * SqVoltagePlugin::factorFor (q), stagesAt (juce::roundToInt (stages->getCurrentValue())), 4500.0);
                                          chan[ch].line[q].buckets.a *= tolerance (unit.get(), 2, ch, 0.1); chan[ch].line[q].buckets.b *= tolerance (unit.get(), 3, ch, 0.1); chan[ch].last[q] = 0.0; }
            chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0;
        }
        clockNow = 0.0; phase = 0.0; stagesWas = -1;
    }
    void deinitialise() override {}
    void reset() override { for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (int q = 0; q < 3; ++q) { c.os[q].reset(); c.line[q].reset(); c.last[q] = 0.0; } } }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue()));
        const int stageIx = juce::roundToInt (stages->getCurrentValue()), stageCount = stagesAt (stageIx);
        if (stageIx != stagesWas) { for (auto& c : chan) for (int k = 0; k < 3; ++k) { c.line[k].setStages (stageCount); c.line[k].reset(); c.last[k] = 0.0; } stagesWas = stageIx; }
        const double trim = tolerance (unit.get(), 0, 0, 0.01), target = clockFor (stageCount, (double) time->getCurrentValue()) * trim; if (clockNow <= 0.0) clockNow = target;
        const double glide = 1.0 - std::exp (-1.0 / (0.03 * sampleRate));       // the clock's own smoothing: moving Time bends the pitch for a moment, as on the hardware
        for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < 3; ++k) chan[ch].line[k].setCutoff (1000.0 * (double) tone->getCurrentValue() * tolerance (unit.get(), 1, ch, 0.03));
        const double fb = juce::jlimit (0.0, 1.1, (double) feedback->getCurrentValue() / 100.0), wobble = wobbleAtFull * juce::jlimit (0.0, 1.0, (double) depth->getCurrentValue() / 100.0);
        const double dPhase = juce::MathConstants<double>::twoPi * (double) rate->getCurrentValue() / sampleRate, offset = juce::MathConstants<double>::pi * juce::jlimit (0.0, 1.0, (double) width->getCurrentValue() / 100.0);
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        float* data[2] = { buf.getWritePointer (0, fc.bufferStartSample), nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample) : nullptr };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            clockNow += (target - clockNow) * glide; phase += dPhase; if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
            for (int ch = 0; ch < nch; ++ch)
            {
                auto& c = chan[ch]; const float in = data[ch][i]; const double clock = juce::jmax (2000.0, clockNow * (1.0 + wobble * std::sin (phase + (ch == 1 ? offset : 0.0))));
                const double y = c.os[q].process ((double) in, [&] (double x) { const double v = c.line[q].process ((x + fb * c.last[q]) * fullScaleVolts, clock) / fullScaleVolts; c.last[q] = v; return v; });
                const float delayed = c.dry[(size_t) c.dryPos]; c.dry[(size_t) c.dryPos] = in; if (++c.dryPos >= delaySamples) c.dryPos = 0;
                data[ch][i] = (delayed + wet * ((float) y - delayed)) * outGain;
            }
        }
        clockShown.store ((float) clockNow);
    }

    /** The clock right now in hertz (before the wobble), for the face. */
    float currentClockHz() const { return clockShown.load(); }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, timeV, stagesV, feedbackV, toneV, rateV, depthV, widthV, mixV, outputV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> timeV, stagesV, feedbackV, toneV, rateV, depthV, widthV, mixV, outputV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr time, stages, feedback, tone, rate, depth, width, mix, output, quality;

private:
    struct Channel { SqTube::Oversampler os[3]; SqBbd::Line line[3]; double last[3] = {}; std::vector<float> dry; int dryPos = 0; };
    Channel chan[2]; double sampleRate = 44100.0, clockNow = 0.0, phase = 0.0; int stagesWas = -1; std::atomic<float> clockShown { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageBrigadePlugin)
};
