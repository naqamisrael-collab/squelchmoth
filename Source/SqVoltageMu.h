#pragma once
// Squelchmoth 1.45 - Voltage Mu: a variable-mu tube compressor from its circuit (see SqMu.h), as a plug-in.
// A push-pull pair of remote-cutoff triodes carries the signal; the sidechain pulls their grids negative and their
// gain falls. Slow and smooth: attack 25 to 70 ms, recovery in five steps from 0.2 to 8 seconds (the figures
// published for this kind of unit). Compress is a soft squeeze measured at 1.5 : 1, Limit a firmer 4 : 1; both climb
// when pushed, and both lean on the recovery time, because the control capacitor empties between peaks.
// Input sets how hard the tubes are driven, Threshold where compression starts, Output makes up the level.
// The two channels share one sidechain. Every instance has its own four tubes.

#include <JuceHeader.h>
#include "SqMu.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageMuPlugin : public te::Plugin
{
public:
    SqVoltageMuPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        inputV.referTo (state, "input", um, 0.0f); thresholdV.referTo (state, "threshold", um, -14.0f); attackV.referTo (state, "attack", um, 50.0f); recoveryV.referTo (state, "recovery", um, 2.0f);
        modeV.referTo (state, "mode", um, 0.0f); outputV.referTo (state, "output", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f); qualityV.referTo (state, "quality", um, 0.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);
        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        input     = addParam ("input",     TRANS("Input"),     { -12.0f, 12.0f }, db, num);
        threshold = addParam ("threshold", TRANS("Threshold"), { -30.0f, 0.0f }, db, num);
        attack    = addParam ("attack",    TRANS("Attack"),    { 25.0f, 70.0f }, [] (float v) { return juce::String (juce::roundToInt (v)) + " ms"; }, num);
        recovery  = addParam ("recovery",  TRANS("Recovery"),  { 0.0f, 4.0f, 1.0f }, [] (float v) { return recoveryNames()[juce::jlimit (0, 4, juce::roundToInt (v))]; },
                              [] (const juce::String& s) { const float t = s.retainCharacters ("0123456789.").getFloatValue(); int best = 0; for (int i = 1; i < 5; ++i) if (std::abs ((float) recoverySeconds (i) - t) < std::abs ((float) recoverySeconds (best) - t)) best = i; return (float) best; });
        mode      = addParam ("mode",      TRANS("Mode"),      { 0.0f, 1.0f, 1.0f }, [] (float v) { return modeNames()[v >= 0.5f ? 1 : 0]; },
                              [] (const juce::String& s) { return s.trim().startsWithIgnoreCase ("l") || s.getIntValue() == 1 ? 1.0f : 0.0f; });
        output    = addParam ("output",    TRANS("Output"),    { -12.0f, 24.0f }, db, num);
        mix       = addParam ("mix",       TRANS("Mix"),       { 0.0f, 100.0f }, pct, num);
        quality   = addParam ("quality",   TRANS("Quality"),   { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        input->attachToCurrentValue (inputV); threshold->attachToCurrentValue (thresholdV); attack->attachToCurrentValue (attackV); recovery->attachToCurrentValue (recoveryV);
        mode->attachToCurrentValue (modeV); output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageMuPlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { input.get(), threshold.get(), attack.get(), recovery.get(), mode.get(), output.get(), mix.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Mu"); }
    static constexpr const char* xmlTypeName = "sqmVoltageMu";
    static constexpr int delaySamples = 96;
    static constexpr double referenceVolts = 1.0;       // a full-scale sample is one volt between the two grids at Input 0
    static constexpr double depthCompress = 1.9103, depthLimit = 16.0162;      // set so the circuit measures 1.5 : 1 and 4 : 1 (attack 50 ms, recovery 0.6 s)

    juce::String getName() const override               { return "Voltage Mu"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VMu"; }
    juce::String getSelectableDescription() override    { return TRANS("Variable-mu tube compressor (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray modeNames() { return { "Compress", "Limit" }; }
    static juce::StringArray recoveryNames() { return { "0.2 s", "0.4 s", "0.6 s", "4 s", "8 s" }; }
    static double recoverySeconds (int ix) { static const double t[5] = { 0.2, 0.4, 0.6, 4.0, 8.0 }; return t[juce::jlimit (0, 4, ix)]; }

    /** One tube of this unit: which (0, 1: the left pair; 2, 3: the right pair). */
    static SqMu::Remote tubeFor (int unitSeed, int which)
    {
        auto off = [unitSeed] (int what, int n, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (n + 1) * 40503u ^ (juce::uint32) (what + 171) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        auto t = SqMu::remote(); const double mu = off (0, 0, 0.03), k = off (1, which / 2, 0.03) * off (2, which, 0.01);      // the unit's tubes share a type; the pairs are matched to about one per cent
        t.a.mu *= mu; t.b.mu *= mu; t.a.kg1 *= k; t.b.kg1 *= k; return t;
    }
    /** inputDb: the compressor's own levels are measured before Input, so the threshold moves down by as much as Input pushes up. */
    static SqMu::Sidechain sidechainFor (bool limit, float thresholdDb, float attackMs, int recoveryIx, float inputDb = 0.0f)
    {
        SqMu::Sidechain s; s.threshold = std::pow (10.0, ((double) thresholdDb - (double) inputDb) / 20.0); s.depth = limit ? depthLimit : depthCompress; s.attack = juce::jlimit (0.025, 0.07, (double) attackMs / 1000.0); s.release = recoverySeconds (recoveryIx); return s;
    }

    /** What the compressor settles to for steady tones at these settings. The capacitor's resting voltage is found from the
        circuit's own balance (what flows in over a cycle through the attack time equals what leaks through the recovery time),
        so the face does not have to run seconds of audio to know. */
    struct Reading
    {
        float in[9] = {}, out[9] = {};       // steady tones: level in (before Input) and level out (before Output), dBFS
        double ratio = 0, reductionAtFull = 0, third = -200, gridsAtFull = 0;     // over the first 12 dB above the threshold; for a full-scale tone: reduction, its 3rd harmonic, the grids' voltage
    };
    static float inAt (int i) { return -48.0f + 6.0f * (float) i; }
    static Reading measure (int unitSeed, bool limit, float thresholdDb, float inputDb, float attackMs, int recoveryIx)
    {
        Reading r; SqMu::Stage st; st.prepare (tubeFor (unitSeed, 0), tubeFor (unitSeed, 1), {}); const auto sc = sidechainFor (limit, thresholdDb, attackMs, recoveryIx);
        const double g0 = st.gainAt (0.0), deepest = st.c.deepest;
        auto settle = [&] (double levelDb, double& control)       // level in dBFS after Input; returns the output level in dBFS
        {
            const double amp = std::pow (10.0, levelDb / 20.0);
            auto net = [&] (double x)
            {
                const double ctl = juce::jmax (-sc.depth * x, deepest), peak = amp * st.gainAt (ctl) / g0 / sc.threshold; double in = 0.0;
                for (int i = 0; i < 64; ++i) in += juce::jmax (peak * std::sin (juce::MathConstants<double>::pi * (i + 0.5) / 64.0) - 1.0 - x, 0.0);
                return in / 64.0 / sc.attack - x / sc.release;
            };
            double lo = 0.0, hi = 1.0; while (net (hi) > 0.0 && hi < 1.0e4) hi *= 2.0;
            for (int k = 0; k < 50; ++k) { const double mid = 0.5 * (lo + hi); (net (mid) > 0.0 ? lo : hi) = mid; }
            control = juce::jmax (-sc.depth * lo, deepest); return levelDb + 20.0 * std::log10 (st.gainAt (control) / g0);
        };
        double ctl = 0.0;
        for (int i = 0; i < 9; ++i) { r.in[i] = inAt (i); r.out[i] = (float) settle ((double) inAt (i) + (double) inputDb, ctl); }
        r.ratio = 12.0 / juce::jmax (0.05, settle ((double) thresholdDb + 12.0, ctl) - settle ((double) thresholdDb, ctl));
        r.reductionAtFull = settle ((double) inputDb, ctl) - (double) inputDb; r.gridsAtFull = st.c.rest + ctl;
        {
            const double volts = referenceVolts * std::pow (10.0, (double) inputDb / 20.0); std::complex<double> a1 = 0, a3 = 0; const int N = 96;
            for (int i = 0; i < N; ++i) { const double y = st.process (volts * std::sin (juce::MathConstants<double>::twoPi * i / N), ctl); a1 += y * std::polar (1.0, -juce::MathConstants<double>::twoPi * i / N); a3 += y * std::polar (1.0, -3.0 * juce::MathConstants<double>::twoPi * i / N); }
            r.third = 20.0 * std::log10 (std::abs (a3) / juce::jmax (1.0e-30, std::abs (a1)) + 1.0e-15);
        }
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0; shapedFor = -1000.0f;
        for (int ch = 0; ch < 2; ++ch) { for (int q = 0; q < 3; ++q) chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0; }
        reduction.store (0.0f); grids.store (-1.5f);
    }
    void deinitialise() override {}
    void reset() override { for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); } for (auto& c : comp) c.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue()));
        const float inDb = input->getCurrentValue();
        if (inDb != shapedFor)       // Input is the level at the grids: the pairs are set up again for it (their unity gain depends on it only through the scaling)
        {
            const double volts = referenceVolts * std::pow (10.0, (double) inDb / 20.0);
            for (int k = 0; k < 3; ++k) { const double keep = comp[k].x; comp[k].prepare (sampleRate * SqVoltagePlugin::factorFor (k), tubeFor (unit.get(), 0), tubeFor (unit.get(), 1), tubeFor (unit.get(), 2), tubeFor (unit.get(), 3), {}, volts); if (shapedFor > -999.0f) comp[k].x = keep; }
            shapedFor = inDb;
        }
        auto& c = comp[q]; c.set (sidechainFor (mode->getCurrentValue() >= 0.5f, threshold->getCurrentValue(), attack->getCurrentValue(), juce::roundToInt (recovery->getCurrentValue()), inDb)); if (nch < 2) c.clearRight();
        const double inGain = std::pow (10.0, (double) inDb / 20.0);       // the pair's output is scaled to unity for its own level, so Input's gain is put back here
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        auto* l = buf.getWritePointer (0, fc.bufferStartSample); auto* r = nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample) : nullptr;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float inL = l[i]; int k = 0;
            const double a = chan[0].os[q].process ((double) inL, [&] (double x) { return c.left (x, k++); }) * inGain;
            { auto& d = chan[0]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inL; if (++d.dryPos >= delaySamples) d.dryPos = 0; l[i] = (delayed + wet * ((float) a - delayed)) * outGain; }
            if (r != nullptr)
            {
                const float inR = r[i]; k = 0; const double b = chan[1].os[q].process ((double) inR, [&] (double x) { return c.right (x, k++); }) * inGain;
                auto& d = chan[1]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inR; if (++d.dryPos >= delaySamples) d.dryPos = 0; r[i] = (delayed + wet * ((float) b - delayed)) * outGain;
            }
        }
        reduction.store ((float) c.reductionDb()); grids.store ((float) (c.st[0].c.rest + c.control()));
    }

    /** Gain reduction right now in dB (0 or below) and the voltage on the grids, for the face. */
    float currentReductionDb() const { return reduction.load(); }
    float currentGridVolts() const { return grids.load(); }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, inputV, thresholdV, attackV, recoveryV, modeV, outputV, mixV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> inputV, thresholdV, attackV, recoveryV, modeV, outputV, mixV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr input, threshold, attack, recovery, mode, output, mix, quality;

private:
    struct Channel { SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; };
    Channel chan[2]; SqMu::Compressor comp[3]; double sampleRate = 44100.0; float shapedFor = -1000.0f; std::atomic<float> reduction { 0.0f }, grids { -1.5f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageMuPlugin)
};
