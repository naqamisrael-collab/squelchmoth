#pragma once
// Squelchmoth 1.43 - Voltage Comp: a FET compressor from its circuit (see SqFet.h), as a plug-in.
// There is no threshold control: Input pushes the signal up into a fixed threshold (12 dB under full scale at Input 0),
// as on the classic fast FET compressors, and Output makes up the level. Ratio is the sidechain's depth, in four steps
// named for the ratio each was measured to give over the first 12 dB above the threshold. Attack and Release are the two
// time constants of the control capacitor. The two channels share one sidechain, so the stereo picture holds still.
// It compresses what it has already compressed (a feedback compressor), so pushed very hard its ratio eases off: that is
// the circuit, not a setting. Every instance has its own FETs, and left and right are two of them.

#include <JuceHeader.h>
#include "SqFet.h"
#include "SqTube.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageCompPlugin : public te::Plugin
{
public:
    SqVoltageCompPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        inputV.referTo (state, "input", um, 0.0f); ratioV.referTo (state, "ratio", um, 0.0f); attackV.referTo (state, "attack", um, 50.0f); releaseV.referTo (state, "release", um, 50.0f);
        outputV.referTo (state, "output", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f); qualityV.referTo (state, "quality", um, 1.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);

        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        input   = addParam ("input",   TRANS("Input"),   { -12.0f, 30.0f }, db, num);
        ratio   = addParam ("ratio",   TRANS("Ratio"),   { 0.0f, 3.0f, 1.0f }, [] (float v) { return ratioNames()[juce::jlimit (0, 3, juce::roundToInt (v))]; },
                            [] (const juce::String& s) { const int n = s.retainCharacters ("0123456789").getIntValue(); return n >= 20 ? 3.0f : n >= 12 ? 2.0f : n >= 8 ? 1.0f : n == 4 ? 0.0f : (float) juce::jlimit (0, 3, n); });
        attack  = addParam ("attack",  TRANS("Attack"),  { 0.0f, 100.0f }, [] (float v) { return juce::String (juce::roundToInt (attackSeconds (v) * 1.0e6)) + " us"; }, num);
        release = addParam ("release", TRANS("Release"), { 0.0f, 100.0f }, [] (float v) { return juce::String (juce::roundToInt (releaseSeconds (v) * 1000.0)) + " ms"; }, num);
        output  = addParam ("output",  TRANS("Output"),  { -24.0f, 24.0f }, db, num);
        mix     = addParam ("mix",     TRANS("Mix"),     { 0.0f, 100.0f }, pct, num);
        quality = addParam ("quality", TRANS("Quality"), { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        input->attachToCurrentValue (inputV); ratio->attachToCurrentValue (ratioV); attack->attachToCurrentValue (attackV); release->attachToCurrentValue (releaseV);
        output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageCompPlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { input.get(), ratio.get(), attack.get(), release.get(), output.get(), mix.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Comp"); }
    static constexpr const char* xmlTypeName = "sqmVoltageComp";
    static constexpr int delaySamples = 96;
    static constexpr double referenceVolts = 0.1;       // a full-scale sample is a tenth of a volt at the gain cell before Input
    static constexpr double thresholdDb = -12.0;        // against full scale at Input 0

    juce::String getName() const override               { return "Voltage Comp"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VComp"; }
    juce::String getSelectableDescription() override    { return TRANS("FET compressor (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray ratioNames() { return { "4 : 1", "8 : 1", "12 : 1", "20 : 1" }; }
    /** Attack: 800 microseconds at 0 % down to 20 at 100 % (further round is faster). Release: 1.1 seconds at 0 % down to 50 ms at 100 %. */
    static double attackSeconds (float pct)  { return 800.0e-6 * std::pow (20.0 / 800.0, juce::jlimit (0.0, 1.0, (double) pct / 100.0)); }
    static double releaseSeconds (float pct) { return 1.1 * std::pow (0.05 / 1.1, juce::jlimit (0.0, 1.0, (double) pct / 100.0)); }

    static SqFet::Fet fetFor (int unitSeed, int channel)
    {
        auto off = [unitSeed] (int which, int ch, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (ch + 1) * 40503u ^ (juce::uint32) (which + 91) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        SqFet::Fet f; f.vto *= off (0, 0, 0.03);                                  // the pair is matched for pinch-off (both channels the same), as a builder would
        f.beta *= off (1, 0, 0.05) * off (2, channel, 0.01); return f;           // and differs a little in how hard each conducts
    }
    static SqFet::Sidechain sidechainFor (int ratioIx, float attackPct, float releasePct)
    {
        SqFet::Sidechain s; s.reference = referenceVolts; s.gain = 4.0; s.threshold = 1.0; s.depth = SqFet::depthForRatio (ratioIx); s.attack = attackSeconds (attackPct); s.release = releaseSeconds (releasePct); return s;
    }

    /** What the compressor does with these settings, from the same circuit the audio runs through. */
    struct Reading
    {
        float in[9] = {}, out[9] = {};           // steady 1 kHz tones: level in and level out, dBFS (in runs -36 to +12 against the threshold... see inAt)
        double ratio = 0, reductionAtFull = 0;   // measured over the first 12 dB above the threshold; reduction for a full-scale tone at this Input
        double second = -200, third = -200;      // harmonics of that tone, dB below it
    };
    static float inAt (int i) { return -36.0f + 6.0f * (float) i; }      // level of the test tone against full scale, before Input
    static Reading measure (int unitSeed, int ratioIx, float inputDb)
    {
        Reading r; const double fs = 96000.0; const auto f = fetFor (unitSeed, 0); auto sc = sidechainFor (ratioIx, 50.0f, 50.0f);        // middle times: at this rate a 20 microsecond attack is under two samples and would misstate the top ratio
        auto steady = [&] (double levelDb, double* h2 = nullptr, double* h3 = nullptr)
        {
            SqFet::Compressor c; c.prepare (fs, f, {}, sc); const double amp = referenceVolts * std::pow (10.0, (levelDb + (double) inputDb) / 20.0); const int settle = (int) (0.35 * fs), n = 960;
            std::complex<double> a1 = 0, a2 = 0, a3 = 0;
            for (int i = 0; i < settle + n; ++i) { const double v = c.process (amp * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / fs)); if (i >= settle) { const double ph = juce::MathConstants<double>::twoPi * 10.0 * (i - settle) / n; a1 += v * std::polar (1.0, -ph); a2 += v * std::polar (1.0, -2.0 * ph); a3 += v * std::polar (1.0, -3.0 * ph); } }
            if (h2 != nullptr) *h2 = 20.0 * std::log10 (std::abs (a2) / juce::jmax (1.0e-30, std::abs (a1)) + 1.0e-15); if (h3 != nullptr) *h3 = 20.0 * std::log10 (std::abs (a3) / juce::jmax (1.0e-30, std::abs (a1)) + 1.0e-15);
            return 20.0 * std::log10 (std::abs (a1) / (n / 2.0) / referenceVolts + 1.0e-15);
        };
        for (int i = 0; i < 9; ++i) { r.in[i] = inAt (i); r.out[i] = (float) steady ((double) inAt (i)); }
        const double thr = thresholdDb - (double) inputDb; r.ratio = 12.0 / juce::jmax (0.05, steady (thr + 12.0) - steady (thr));
        r.reductionAtFull = steady (0.0, &r.second, &r.third) - (double) inputDb;      // against what came in
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        for (int ch = 0; ch < 2; ++ch) { for (int q = 0; q < 3; ++q) chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0; }
        for (int q = 0; q < 3; ++q) comp[q].prepare (sampleRate * SqVoltagePlugin::factorFor (q), fetFor (unit.get(), 0), fetFor (unit.get(), 1), {}, sidechainFor (0, 50.0f, 50.0f));
        reduction.store (0.0f);
    }
    void deinitialise() override {}
    void reset() override { for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); } for (int q = 0; q < 3; ++q) comp[q].prepare (sampleRate * SqVoltagePlugin::factorFor (q), fetFor (unit.get(), 0), fetFor (unit.get(), 1), {}, sidechainFor (0, 50.0f, 50.0f)); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue()));
        auto& c = comp[q]; c.set (sidechainFor (juce::roundToInt (ratio->getCurrentValue()), attack->getCurrentValue(), release->getCurrentValue())); if (nch < 2) c.clearRight();
        const double inVolts = referenceVolts * std::pow (10.0, (double) input->getCurrentValue() / 20.0), back = 1.0 / referenceVolts;
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        auto* l = buf.getWritePointer (0, fc.bufferStartSample); auto* r = nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample) : nullptr;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float inL = l[i]; int k = 0;
            const double a = chan[0].os[q].process ((double) inL * inVolts, [&] (double x) { return c.left (x, k++); }) * back;
            { auto& d = chan[0]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inL; if (++d.dryPos >= delaySamples) d.dryPos = 0; l[i] = (delayed + wet * ((float) a - delayed)) * outGain; }
            if (r != nullptr)
            {
                const float inR = r[i]; k = 0; const double b = chan[1].os[q].process ((double) inR * inVolts, [&] (double x) { return c.right (x, k++); }) * back;
                auto& d = chan[1]; const float delayed = d.dry[(size_t) d.dryPos]; d.dry[(size_t) d.dryPos] = inR; if (++d.dryPos >= delaySamples) d.dryPos = 0; r[i] = (delayed + wet * ((float) b - delayed)) * outGain;
            }
        }
        reduction.store ((float) c.reductionDb());
    }

    /** Gain reduction right now in dB (0 or below), for the meter. */
    float currentReductionDb() const { return reduction.load(); }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, inputV, ratioV, attackV, releaseV, outputV, mixV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> inputV, ratioV, attackV, releaseV, outputV, mixV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr input, ratio, attack, release, output, mix, quality;

private:
    struct Channel { SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; };
    Channel chan[2]; SqFet::StereoCompressor comp[3]; double sampleRate = 44100.0; std::atomic<float> reduction { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageCompPlugin)
};
