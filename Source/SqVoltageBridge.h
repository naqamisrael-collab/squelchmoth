#pragma once
// Squelchmoth 1.49 - Voltage Bridge: a diode-bridge compressor from its circuit (see SqBridge.h), as a plug-in.
// Four diodes in a ring shunt the signal away as a control current flows through them; the sidechain listens to the
// output. Fast and firm, the console compressor for drums and the mix bus: ratios 1.5, 2, 3, 4 and 6 to 1, attack
// Fast (3 ms) or Slow (6 ms, and the sidechain hears less below 100 Hz), recovery 100, 400, 800 or 1500 ms, or one
// of two automatic settings that let go quickly after a single peak and slowly after a long loud passage.
// Threshold sets where compression starts, Gain makes up the level (0 to 20 dB).
// The two channels share one sidechain. Every instance has its own eight diodes.

#include <JuceHeader.h>
#include "SqBridge.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageBridgePlugin : public te::Plugin
{
public:
    SqVoltageBridgePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        thresholdV.referTo (state, "threshold", um, -16.0f); ratioV.referTo (state, "ratio", um, 3.0f); attackV.referTo (state, "attack", um, 1.0f); recoveryV.referTo (state, "recovery", um, 0.0f);
        gainV.referTo (state, "gain", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f); qualityV.referTo (state, "quality", um, 0.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);
        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        threshold = addParam ("threshold", TRANS("Threshold"), { -30.0f, 0.0f }, db, num);
        ratio     = addParam ("ratio",     TRANS("Ratio"),     { 0.0f, 4.0f, 1.0f }, [] (float v) { return ratioNames()[juce::jlimit (0, 4, juce::roundToInt (v))]; },
                              [] (const juce::String& s) { const float t = s.upToFirstOccurrenceOf (":", false, false).retainCharacters ("0123456789.").getFloatValue(); int best = 0; for (int i = 1; i < 5; ++i) if (std::abs ((float) ratioOf (i) - t) < std::abs ((float) ratioOf (best) - t)) best = i; return (float) best; });
        attack    = addParam ("attack",    TRANS("Attack"),    { 0.0f, 1.0f, 1.0f }, [] (float v) { return attackNames()[v >= 0.5f ? 1 : 0]; },
                              [] (const juce::String& s) { return s.trim().startsWithIgnoreCase ("s") || s.getIntValue() == 1 ? 1.0f : 0.0f; });
        recovery  = addParam ("recovery",  TRANS("Recovery"),  { 0.0f, 5.0f, 1.0f }, [] (float v) { return recoveryNames()[juce::jlimit (0, 5, juce::roundToInt (v))]; },
                              [] (const juce::String& s)
                              {
                                  const auto t = s.trim(); if (t.startsWithIgnoreCase ("a")) return t.containsChar ('2') ? 5.0f : 4.0f;
                                  float secs = t.retainCharacters ("0123456789.").getFloatValue(); if (t.containsIgnoreCase ("ms") || secs > 10.0f) secs /= 1000.0f;
                                  int best = 0; for (int i = 1; i < 4; ++i) if (std::abs ((float) recoverySeconds (i) - secs) < std::abs ((float) recoverySeconds (best) - secs)) best = i; return (float) best;
                              });
        gain      = addParam ("gain",      TRANS("Gain"),      { 0.0f, 20.0f }, db, num);
        mix       = addParam ("mix",       TRANS("Mix"),       { 0.0f, 100.0f }, pct, num);
        quality   = addParam ("quality",   TRANS("Quality"),   { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        threshold->attachToCurrentValue (thresholdV); ratio->attachToCurrentValue (ratioV); attack->attachToCurrentValue (attackV); recovery->attachToCurrentValue (recoveryV);
        gain->attachToCurrentValue (gainV); mix->attachToCurrentValue (mixV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageBridgePlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { threshold.get(), ratio.get(), attack.get(), recovery.get(), gain.get(), mix.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Bridge"); }
    static constexpr const char* xmlTypeName = "sqmVoltageBridge";
    static constexpr int delaySamples = 96;
    static constexpr double bridgeVolts = 0.03;         // a full-scale sample is 30 mV across the ring's inputs: low enough that the diodes stay nearly straight

    juce::String getName() const override               { return "Voltage Bridge"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VBrg"; }
    juce::String getSelectableDescription() override    { return TRANS("Diode-bridge compressor (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray ratioNames() { return { "1.5:1", "2:1", "3:1", "4:1", "6:1" }; }
    static double ratioOf (int ix) { static const double r[5] = { 1.5, 2.0, 3.0, 4.0, 6.0 }; return r[juce::jlimit (0, 4, ix)]; }
    static juce::StringArray attackNames() { return { "Fast", "Slow" }; }
    static juce::StringArray recoveryNames() { return { "100 ms", "400 ms", "800 ms", "1.5 s", "Auto 1", "Auto 2" }; }
    static double recoverySeconds (int ix) { static const double t[6] = { 0.1, 0.4, 0.8, 1.5, 0.1, 0.05 }; return t[juce::jlimit (0, 5, ix)]; }

    /** One diode of this unit (0 to 3: the left ring; 4 to 7: the right). The eight are matched to a third of a per cent. */
    static SqClip::Diode diodeFor (int unitSeed, int which)
    {
        auto off = [unitSeed] (int what, int n, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (n + 1) * 40503u ^ (juce::uint32) (what + 311) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        auto d = SqClip::silicon(); d.is *= off (0, 0, 0.05) * off (1, which, 0.003); return d;
    }
    static SqBridge::Sidechain sidechainFor (float thresholdDb, int ratioIx, bool slowAttack, int recoveryIx)
    {
        SqBridge::Sidechain s; s.threshold = std::pow (10.0, (double) thresholdDb / 20.0); s.ratio = ratioOf (ratioIx); s.attack = slowAttack ? 0.006 : 0.003; s.highPassHz = slowAttack ? 100.0 : 0.0;
        s.release = recoverySeconds (recoveryIx); s.slowRelease = recoveryIx == 4 ? 2.0 : recoveryIx == 5 ? 5.0 : 0.0; s.slowCharge = 0.5; return s;
    }

    /** What the compressor settles to for steady tones at these settings. The capacitor's resting voltage is found from the
        circuit's own balance (what flows in over a cycle through the attack time equals what leaks through the recovery time),
        so the face does not have to run seconds of audio to know. */
    struct Reading
    {
        float in[9] = {}, out[9] = {};       // steady tones: level in and level out (before Gain), dBFS
        double ratio = 0, reductionAtFull = 0, third = -200, microampsAtFull = 0;     // measured from 20 to 30 dB over the threshold; for a full-scale tone: reduction, its 3rd harmonic, the control current
    };
    static float inAt (int i) { return -48.0f + 6.0f * (float) i; }
    static Reading measure (int unitSeed, float thresholdDb, int ratioIx, bool slowAttack, int recoveryIx)
    {
        Reading r; SqBridge::Bridge br; br.prepare (diodeFor (unitSeed, 0), diodeFor (unitSeed, 1), diodeFor (unitSeed, 2), diodeFor (unitSeed, 3), {});
        const auto sc = sidechainFor (thresholdDb, ratioIx, slowAttack, recoveryIx); const double g0 = br.gainAt (0.0), unitAmps = diodeFor (unitSeed, 0).n * SqClip::thermalVolts / br.c.r;
        auto currentFor = [&] (double x) { return x > 0.0 ? unitAmps * std::pow (x, sc.ratio - 1.0) : 0.0; };
        auto settle = [&] (double levelDb, double& amps)       // returns the output level in dBFS
        {
            const double amp = std::pow (10.0, levelDb / 20.0);
            auto net = [&] (double x)
            {
                const double peak = amp * br.gainAt (currentFor (x)) / g0 / sc.threshold; double in = 0.0;
                for (int i = 0; i < 64; ++i) in += juce::jmax (peak * std::sin (juce::MathConstants<double>::pi * (i + 0.5) / 64.0) - 1.0 - x, 0.0);
                return in / 64.0 / sc.attack - x / sc.release;
            };
            double lo = 0.0, hi = 1.0; while (net (hi) > 0.0 && hi < 1.0e4) hi *= 2.0;
            for (int k = 0; k < 50; ++k) { const double mid = 0.5 * (lo + hi); (net (mid) > 0.0 ? lo : hi) = mid; }
            amps = currentFor (lo); return levelDb + 20.0 * std::log10 (br.gainAt (amps) / g0);
        };
        double amps = 0.0;
        for (int i = 0; i < 9; ++i) { r.in[i] = inAt (i); r.out[i] = (float) settle ((double) inAt (i), amps); }
        r.ratio = 10.0 / juce::jmax (0.05, settle ((double) thresholdDb + 30.0, amps) - settle ((double) thresholdDb + 20.0, amps));
        r.reductionAtFull = settle (0.0, amps); r.microampsAtFull = amps * 1.0e6;
        {
            std::complex<double> a1 = 0, a3 = 0; const int N = 96;
            for (int i = 0; i < N; ++i) { const double y = br.process (bridgeVolts * std::sin (juce::MathConstants<double>::twoPi * i / N), amps); a1 += y * std::polar (1.0, -juce::MathConstants<double>::twoPi * i / N); a3 += y * std::polar (1.0, -3.0 * juce::MathConstants<double>::twoPi * i / N); }
            r.third = 20.0 * std::log10 (std::abs (a3) / juce::jmax (1.0e-30, std::abs (a1)) + 1.0e-15);
        }
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        SqClip::Diode ring[8]; for (int i = 0; i < 8; ++i) ring[i] = diodeFor (unit.get(), i);
        for (int k = 0; k < 3; ++k) comp[k].prepare (sampleRate * SqVoltagePlugin::factorFor (k), ring, {}, bridgeVolts);
        for (int ch = 0; ch < 2; ++ch) { for (int q = 0; q < 3; ++q) chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0; chan[ch].hpIn = chan[ch].hpOut = 0.0; }
        hpA = std::exp (-juce::MathConstants<double>::twoPi * 5.0 / sampleRate); reduction.store (0.0f); current.store (0.0f);
    }
    void deinitialise() override {}
    void reset() override { for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); c.hpIn = c.hpOut = 0.0; } for (auto& c : comp) c.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue()));
        auto& c = comp[q]; c.set (sidechainFor (threshold->getCurrentValue(), juce::roundToInt (ratio->getCurrentValue()), attack->getCurrentValue() >= 0.5f, juce::roundToInt (recovery->getCurrentValue()))); if (nch < 2) c.clearRight();
        const float outGain = std::pow (10.0f, gain->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        auto* l = buf.getWritePointer (0, fc.bufferStartSample); auto* r = nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample) : nullptr;
        // the output transformer passes no standing voltage (here a 5 Hz high-pass): diodes that are not quite alike thump a little when the current moves, but the signal does not stay off-centre
        auto transformer = [this] (Channel& d, double v) { const double out = hpA * (d.hpOut + v - d.hpIn); d.hpIn = v; d.hpOut = out; return out; };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float inL = l[i]; int k = 0;
            const double a = transformer (chan[0], chan[0].os[q].process ((double) inL, [&] (double x) { return c.left (x, k++); }));
            { auto& d = chan[0]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inL; if (++d.dryPos >= delaySamples) d.dryPos = 0; l[i] = (delayed + wet * ((float) a - delayed)) * outGain; }
            if (r != nullptr)
            {
                const float inR = r[i]; k = 0; const double b = transformer (chan[1], chan[1].os[q].process ((double) inR, [&] (double x) { return c.right (x, k++); }));
                auto& d = chan[1]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inR; if (++d.dryPos >= delaySamples) d.dryPos = 0; r[i] = (delayed + wet * ((float) b - delayed)) * outGain;
            }
        }
        reduction.store ((float) c.reductionDb()); current.store ((float) (c.control() * 1.0e6));
    }

    /** Gain reduction right now in dB (0 or below) and the control current through the ring in microamps, for the face. */
    float currentReductionDb() const { return reduction.load(); }
    float currentMicroamps() const { return current.load(); }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, thresholdV, ratioV, attackV, recoveryV, gainV, mixV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> thresholdV, ratioV, attackV, recoveryV, gainV, mixV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr threshold, ratio, attack, recovery, gain, mix, quality;

private:
    struct Channel { SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; double hpIn = 0.0, hpOut = 0.0; };
    Channel chan[2]; SqBridge::Compressor comp[3]; double sampleRate = 44100.0, hpA = 1.0; std::atomic<float> reduction { 0.0f }, current { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageBridgePlugin)
};
