#pragma once
// Squelchmoth 1.42 - Voltage Clip: a diode clipper solved as a circuit (see SqClip.h), as a plug-in.
// Drive pushes the signal into a pair of diodes; at 0 dB a full-scale peak sits at the clipper's ceiling, and however hard
// it is driven the circuit's output stays under full scale - the ceiling is where the diodes hold a 40 volt input.
// Full scale is set 3 % above that, the room band-limiting was measured to need, so the output stays under full scale after it too
// (measured on tones through every pair and on a square wave driven flat out; it is not a true-peak limiter for every possible signal). Diodes
// picks the pair (silicon; a low-threshold soft one; a low-threshold sharp one), Symmetry makes one side two diodes in
// series (lopsided: even harmonics), Tone is the capacitor beside the diodes (the top rounds off as it clips).
// Every instance has its own parts, and left and right are two pairs.

#include <JuceHeader.h>
#include "SqClip.h"
#include "SqTube.h"
#include "SqVoltage.h"

namespace te = tracktion;

class SqVoltageClipPlugin : public te::Plugin
{
public:
    SqVoltageClipPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        driveV.referTo (state, "drive", um, 0.0f); diodesV.referTo (state, "diodes", um, 0.0f); symmetryV.referTo (state, "symmetry", um, 0.0f); toneV.referTo (state, "tone", um, 90.0f);
        outputV.referTo (state, "output", um, 0.0f); mixV.referTo (state, "mix", um, 100.0f); qualityV.referTo (state, "quality", um, 1.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);

        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        drive    = addParam ("drive",    TRANS("Drive"),    { -12.0f, 30.0f }, db, num);
        diodes   = addParam ("diodes",   TRANS("Diodes"),   { 0.0f, 2.0f, 1.0f }, [] (float v) { return diodeNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; },
                             [] (const juce::String& s) { return s.containsIgnoreCase ("soft") ? 1.0f : s.containsIgnoreCase ("sharp") ? 2.0f : s.containsIgnoreCase ("sil") ? 0.0f : (float) juce::jlimit (0, 2, juce::roundToInt (s.getFloatValue())); });
        symmetry = addParam ("symmetry", TRANS("Symmetry"), { 0.0f, 1.0f, 1.0f }, [] (float v) { return juce::String (v >= 0.5f ? "Lopsided" : "Even"); }, [] (const juce::String& s) { return s.containsIgnoreCase ("lop") || s.containsIgnoreCase ("asym") || s.getFloatValue() >= 0.5f ? 1.0f : 0.0f; });
        tone     = addParam ("tone",     TRANS("Tone"),     { 0.0f, 100.0f }, [] (float v) { const double hz = cornerHz (v); return hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }, num);
        output   = addParam ("output",   TRANS("Output"),   { -24.0f, 24.0f }, db, num);
        mix      = addParam ("mix",      TRANS("Mix"),      { 0.0f, 100.0f }, pct, num);
        quality  = addParam ("quality",  TRANS("Quality"),  { 0.0f, 2.0f, 1.0f }, [] (float v) { return SqVoltagePlugin::qualityNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; }, [] (const juce::String& s) { return SqVoltagePlugin::qualityFromText (s); });
        drive->attachToCurrentValue (driveV); diodes->attachToCurrentValue (diodesV); symmetry->attachToCurrentValue (symmetryV); tone->attachToCurrentValue (toneV);
        output->attachToCurrentValue (outputV); mix->attachToCurrentValue (mixV); quality->attachToCurrentValue (qualityV);
    }

    ~SqVoltageClipPlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { drive.get(), diodes.get(), symmetry.get(), tone.get(), output.get(), mix.get(), quality.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Clip"); }
    static constexpr const char* xmlTypeName = "sqmVoltageClip";
    static constexpr int delaySamples = 96;

    juce::String getName() const override               { return "Voltage Clip"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VClip"; }
    juce::String getSelectableDescription() override    { return TRANS("Diode clipper (circuit model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return sampleRate > 0 ? delaySamples / sampleRate : 0.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray diodeNames() { return { "Silicon", "Soft", "Sharp" }; }
    /** Tone to the corner the capacitor makes with the resistor: 3 kHz at 0 %, 30 kHz at 100 %. */
    static double cornerHz (float pct) { return 3000.0 * std::pow (10.0, juce::jlimit (0.0, 1.0, (double) pct / 100.0)); }

    static void partsFor (int unitSeed, int channel, int diodeIx, bool lopsided, float tonePercent, SqClip::Diode& d, SqClip::Circuit& c)
    {
        auto off = [unitSeed, channel] (int which, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (channel + 1) * 40503u ^ (juce::uint32) (which + 71) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        d = diodeIx == 1 ? SqClip::schottky() : diodeIx == 2 ? SqClip::sharp() : SqClip::silicon(); c = {}; c.lopsided = lopsided;
        c.r *= off (0, 0.02); c.c = 1.0 / (juce::MathConstants<double>::twoPi * 2200.0 * cornerHz (tonePercent)) * off (1, 0.05); d.is *= off (2, 0.20);      // diodes of one type differ most in their leakage
    }

    /** The plug-in's full scale: where the pair holds a 40 volt input (the larger side), plus 3 % - the room that band-limiting
        was measured to need (a sharp-kneed pair driven flat out, and a square wave, ring by up to 0.22 dB when brought back to the session's rate). */
    static double ceilingVolts (const SqClip::Diode& d, const SqClip::Circuit& c)
    {
        auto held = [&] (double vin) { double x = 0.0; for (int it = 0; it < 200; ++it) { double i, di; SqClip::pair (d, c.lopsided, x, i, di); const double f = (x - vin) / c.r + i, df = 1.0 / c.r + di; double step = f / df; step = juce::jlimit (-0.25, 0.25, step); x -= step; if (std::abs (step) < 1.0e-12) break; } return std::abs (x); };
        return 1.03 * juce::jmax (held (40.0), held (-40.0));
    }

    /** What the clipper does to a 1 kHz test tone with these settings, from the same circuit the audio runs through. */
    struct Reading
    {
        std::vector<float> wave;                     // two cycles of the output, where 1 is full scale
        double harmonics[4] = { -200, -200, -200, -200 };      // 2nd to 5th, dB below the tone
        double kneeVolts = 0, ceilingVolts = 0, peakDb = 0, levelDb = 0, cornerHz = 0;      // peakDb: the output's peak against full scale
    };
    static Reading measure (int unitSeed, int diodeIx, bool lopsided, float tonePercent, float driveDb, float testAmp = 0.9f)
    {
        Reading r; SqClip::Diode d; SqClip::Circuit c; partsFor (unitSeed, 0, diodeIx, lopsided, tonePercent, d, c);
        r.kneeVolts = SqClip::kneeVolts (d); r.ceilingVolts = ceilingVolts (d, c); r.cornerHz = SqClip::Clipper::corner (c);
        const double fs = 384000.0, volts = r.ceilingVolts * std::pow (10.0, (double) driveDb / 20.0) * testAmp; const int settle = 3840, n = 3840;      // ten cycles of 1 kHz
        SqClip::Clipper k; k.prepare (fs, d, c); std::vector<double> y ((size_t) n); double pk = 0;
        for (int i = 0; i < settle + n; ++i) { const double v = k.process (volts * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / fs)) / r.ceilingVolts; if (i >= settle) { y[(size_t) (i - settle)] = v; pk = std::max (pk, std::abs (v)); } }
        double mag[6] = {};
        for (int h = 1; h <= 5; ++h) { std::complex<double> a = 0; for (int i = 0; i < n; ++i) a += y[(size_t) i] * std::polar (1.0, -juce::MathConstants<double>::twoPi * (h * 10.0) * i / n); mag[h] = std::abs (a) / (n / 2.0); }
        for (int h = 2; h <= 5; ++h) r.harmonics[h - 2] = 20.0 * std::log10 (mag[h] / juce::jmax (1.0e-12, mag[1]) + 1.0e-12);
        r.peakDb = 20.0 * std::log10 (pk + 1.0e-12); r.levelDb = 20.0 * std::log10 (mag[1] + 1.0e-12);
        r.wave.resize (384); for (int i = 0; i < 384; ++i) r.wave[(size_t) i] = (float) y[(size_t) (i * 2)];      // 768 samples at 384 kHz = two cycles
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        for (int ch = 0; ch < 2; ++ch) { for (int q = 0; q < 3; ++q) chan[ch].os[q].prepare (SqVoltagePlugin::factorFor (q)); chan[ch].dry.assign ((size_t) delaySamples, 0.0f); chan[ch].dryPos = 0; }
        usedQuality = -1; refresh (true);
    }
    void deinitialise() override {}
    void reset() override { usedQuality = -1; refresh (true); for (auto& c : chan) { std::fill (c.dry.begin(), c.dry.end(), 0.0f); for (auto& o : c.os) o.reset(); } }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        refresh (false);
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels()), q = usedQuality;
        const double push = std::pow (10.0, (double) drive->getCurrentValue() / 20.0);
        const float outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        for (int ch = 0; ch < nch; ++ch)
        {
            auto& c = chan[ch]; auto* d = buf.getWritePointer (ch, fc.bufferStartSample); const double inVolts = c.ceiling * push, back = 1.0 / c.ceiling;
            for (int i = 0; i < fc.bufferNumSamples; ++i)
            {
                const float in = d[i];
                const double v = c.os[q].process ((double) in * inVolts, [&c] (double x) { return c.clip.process (x); });
                const float delayed = c.dry[(size_t) c.dryPos]; c.dry[(size_t) c.dryPos] = in; if (++c.dryPos >= delaySamples) c.dryPos = 0;
                d[i] = (delayed + wet * ((float) (v * back) - delayed)) * outGain;
            }
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, driveV, diodesV, symmetryV, toneV, outputV, mixV, qualityV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> driveV, diodesV, symmetryV, toneV, outputV, mixV, qualityV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr drive, diodes, symmetry, tone, output, mix, quality;

private:
    void refresh (bool force)
    {
        const int q = juce::jlimit (0, 2, juce::roundToInt (quality->getCurrentValue())), dk = juce::jlimit (0, 2, juce::roundToInt (diodes->getCurrentValue())), sy = symmetry->getCurrentValue() >= 0.5f ? 1 : 0; const float tn = tone->getCurrentValue();
        const bool rate = force || q != usedQuality, parts = rate || dk != usedDiodes || sy != usedSymmetry || std::abs (tn - usedTone) > 0.01f;
        if (! parts) return;
        for (int ch = 0; ch < 2; ++ch)
        {
            SqClip::Diode d; SqClip::Circuit c; partsFor (unit.get(), ch, dk, sy == 1, tn, d, c);
            if (rate) chan[ch].clip.prepare (sampleRate * SqVoltagePlugin::factorFor (q), d, c); else chan[ch].clip.setParts (d, c);
            chan[ch].ceiling = ceilingVolts (d, c);
        }
        usedQuality = q; usedDiodes = dk; usedSymmetry = sy; usedTone = tn;
    }

    struct Channel { SqClip::Clipper clip; SqTube::Oversampler os[3]; std::vector<float> dry; int dryPos = 0; double ceiling = 0.76; };
    Channel chan[2]; double sampleRate = 44100.0; int usedQuality = -1, usedDiodes = -1, usedSymmetry = -1; float usedTone = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageClipPlugin)
};
