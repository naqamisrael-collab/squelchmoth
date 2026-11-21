#pragma once
// Squelchmoth 1.47 - Voltage Spring: a spring reverb from the spring's own vibration (see SqSpring.h), as a plug-in.
// Each spring is a real tank's spring by its measured dimensions - turns, coil and wire - and what comes out is the sum
// of that spring's own modes, driven at one end and heard at the other. Because short waves travel more slowly along
// a coil than long ones, every echo arrives as a chirp that smears as it goes round: the splash and "drip" of a
// spring tank come out of the equations and are not added. Length winds the springs longer or shorter, Decay and
// Damping set how the modes die, Low Cut leaves the lowest modes out, Width spreads the springs between the speakers.
// It adds no delay and, being a sum of resonators, nothing folds back. Every instance has its own springs.

#include <JuceHeader.h>
#include "SqSpring.h"

namespace te = tracktion;

class SqVoltageSpringPlugin : public te::Plugin
{
public:
    SqVoltageSpringPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        tankV.referTo (state, "tank", um, 2.0f); lengthV.referTo (state, "length", um, 100.0f); decayV.referTo (state, "decay", um, 2.5f); dampingV.referTo (state, "damping", um, 4.0f);
        lowCutV.referTo (state, "lowcut", um, 60.0f); widthV.referTo (state, "width", um, 70.0f); mixV.referTo (state, "mix", um, 25.0f); outputV.referTo (state, "output", um, 0.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);
        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        juce::NormalisableRange<float> decayRange (0.5f, 6.0f); decayRange.setSkewForCentre (2.0f);
        juce::NormalisableRange<float> cutRange (20.0f, 500.0f); cutRange.setSkewForCentre (100.0f);
        tank    = addParam ("tank",    TRANS("Tank"),    { 0.0f, 2.0f, 1.0f }, [] (float v) { return tankNames()[juce::jlimit (0, 2, juce::roundToInt (v))]; },
                            [] (const juce::String& s) { const auto t = s.trim().toLowerCase(); return t.startsWith ("si") || t == "0" || t == "1 spring" ? 0.0f : t.startsWith ("sh") || t == "1" ? 1.0f : 2.0f; });
        length  = addParam ("length",  TRANS("Length"),  { 60.0f, 120.0f }, pct, num);
        decay   = addParam ("decay",   TRANS("Decay"),   decayRange, [] (float v) { return juce::String (v, 1) + " s"; }, num);
        damping = addParam ("damping", TRANS("Damping"), { 1.0f, 12.0f }, [] (float v) { return juce::String (v, 1) + " kHz"; }, num);
        lowCut  = addParam ("lowcut",  TRANS("Low Cut"), cutRange, [] (float v) { return juce::String (juce::roundToInt (v)) + " Hz"; }, num);
        width   = addParam ("width",   TRANS("Width"),   { 0.0f, 100.0f }, pct, num);
        mix     = addParam ("mix",     TRANS("Mix"),     { 0.0f, 100.0f }, pct, num);
        output  = addParam ("output",  TRANS("Output"),  { -24.0f, 12.0f }, db, num);
        tank->attachToCurrentValue (tankV); length->attachToCurrentValue (lengthV); decay->attachToCurrentValue (decayV); damping->attachToCurrentValue (dampingV);
        lowCut->attachToCurrentValue (lowCutV); width->attachToCurrentValue (widthV); mix->attachToCurrentValue (mixV); output->attachToCurrentValue (outputV);
    }

    ~SqVoltageSpringPlugin() override
    {
        notifyListenersOfDeletion();
        for (auto* p : { tank.get(), length.get(), decay.get(), damping.get(), lowCut.get(), width.get(), mix.get(), output.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Spring"); }
    static constexpr const char* xmlTypeName = "sqmVoltageSpring";

    juce::String getName() const override               { return "Voltage Spring"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VSprg"; }
    juce::String getSelectableDescription() override    { return TRANS("Spring reverb (physical model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return 0.0; }
    double getTailLength() const override               { return 6.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray tankNames() { return { "Single", "Short pair", "Long trio" }; }
    static int springsIn (int tankIx) { return tankIx <= 0 ? 1 : tankIx == 1 ? 2 : 3; }
    /** One spring of this unit: the tank's published dimensions, wound a little differently in every unit, at this Length. */
    static SqSpring::Spring springFor (int unitSeed, int tankIx, int which, float lengthPct)
    {
        auto off = [unitSeed] (int what, int n, double spread)
        {
            juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (n + 1) * 40503u ^ (juce::uint32) (what + 251) * 2246822519u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return 1.0 + spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
        };
        auto s = tankIx == 1 ? SqSpring::shortSpring (which) : SqSpring::longSpring (which); const double scale = juce::jlimit (0.6, 1.2, (double) lengthPct / 100.0);
        s.turns *= scale * off (0, which + 10 * tankIx, 0.015); s.helixLength *= scale; s.coilDiameter *= off (1, which + 10 * tankIx, 0.01); return s;
    }
    static SqSpring::Loss lossFor (float decaySeconds, float dampingKHz) { SqSpring::Loss l; l.seconds = juce::jlimit (0.3, 8.0, (double) decaySeconds); l.halfAtHz = 1000.0 * juce::jlimit (0.5, 20.0, (double) dampingKHz); return l; }

    /** What these settings come to, from the springs' own arithmetic. */
    struct Reading
    {
        int springs = 0, modes = 0; double echoMs[3] = {}, transitionHz[3] = {}, decayLow = 0, decayHigh = 0;      // decay: seconds to fall 60 dB at 500 Hz and at 5 kHz
        float curveHz[3][24] = {}, curveMs[3][24] = {};                                                               // one trip down each spring: arrival time against pitch
    };
    static Reading measure (int unitSeed, int tankIx, float lengthPct, float decaySeconds, float dampingKHz, float lowCutHz)
    {
        Reading r; r.springs = springsIn (tankIx); const auto loss = lossFor (decaySeconds, dampingKHz); const double pi = juce::MathConstants<double>::pi;
        r.decayLow = 6.907755278982137 / loss.sigma (500.0); r.decayHigh = 6.907755278982137 / loss.sigma (5000.0);
        for (int i = 0; i < r.springs; ++i)
        {
            const auto s = springFor (unitSeed, tankIx, i, lengthPct); const double L = s.wireLength(), k = s.k(); r.echoMs[i] = 1000.0 * s.echoSeconds(); r.transitionHz[i] = s.transitionHz();
            int beyond = 0; for (int n = 1; n < 100000; ++n) { const double b = n * pi / L, f = s.omega (b) / (2.0 * pi); if (b > k && f > 20000.0) { if (++beyond > 3) break; continue; } if (f >= (double) lowCutHz && f <= 20000.0) ++r.modes; }
            double top = 0.0; { double a = 0.0, b = k; for (int j = 0; j < 60; ++j) { const double m1 = a + (b - a) / 3.0, m2 = b - (b - a) / 3.0; if (s.omega (m1) < s.omega (m2)) a = m1; else b = m2; } top = 0.5 * (a + b); }
            for (int j = 0; j < 24; ++j) { const double b = top * (0.02 + 0.955 * j / 23.0), db = 1.0e-4 * k, vg = (s.omega (b + db) - s.omega (b - db)) / (2.0 * db); r.curveHz[i][j] = (float) (s.omega (b) / (2.0 * pi)); r.curveMs[i][j] = (float) (1000.0 * L / juce::jmax (1.0, vg)); }
        }
        return r;
    }

    void initialise (const te::PluginInitialisationInfo& info) override { sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0; for (auto& b : bank) b.reset(); designedFor[0] = -1.0f; }
    void deinitialise() override {}
    void reset() override { for (auto& b : bank) b.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels());
        const float key[5] = { (float) juce::roundToInt (tank->getCurrentValue()), length->getCurrentValue(), decay->getCurrentValue(), damping->getCurrentValue(), lowCut->getCurrentValue() };
        bool changed = false; for (int i = 0; i < 5; ++i) changed = changed || key[i] != designedFor[i];
        if (changed)       // the springs' modes are worked out again (no allocation); what is ringing keeps ringing
        {
            const int tankIx = (int) key[0]; active = springsIn (tankIx); double power = 0.0;
            for (int i = 0; i < 3; ++i) { if (i < active) { bank[i].design (springFor (unit.get(), tankIx, i, key[1]), lossFor (key[2], key[3]), sampleRate, (double) key[4], 20000.0); const double l = bank[i].level(); power += l * l; } else { bank[i].used = 0; bank[i].reset(); } }
            scale = power > 0.0 ? (float) (1.0 / std::sqrt (power)) : 0.0f; for (int i = 0; i < 5; ++i) designedFor[i] = key[i];
        }
        const float w = juce::jlimit (0.0f, 1.0f, width->getCurrentValue() / 100.0f), outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        // where each spring sits between the speakers (equal power): a pair left and right, a third in the middle
        float gl[3], gr[3]; for (int i = 0; i < 3; ++i) { const float pos = active == 1 ? 0.0f : (i == 0 ? -w : i == 1 ? w : 0.0f), ang = (pos + 1.0f) * juce::MathConstants<float>::pi * 0.25f; gl[i] = std::cos (ang) * 1.41421356f; gr[i] = std::sin (ang) * 1.41421356f; }
        auto* l = buf.getWritePointer (0, fc.bufferStartSample); auto* r = nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample) : nullptr;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float inL = l[i], inR = r != nullptr ? r[i] : inL, drive = 0.5f * (inL + inR) * scale; float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < active; ++k) { const float y = bank[k].process (drive); wl += gl[k] * y; wr += gr[k] * y; }
            if (r == nullptr) { l[i] = (inL + wet * (0.5f * (wl + wr) - inL)) * outGain; }
            else { l[i] = (inL + wet * (wl - inL)) * outGain; r[i] = (inR + wet * (wr - inR)) * outGain; }
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, tankV, lengthV, decayV, dampingV, lowCutV, widthV, mixV, outputV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> tankV, lengthV, decayV, dampingV, lowCutV, widthV, mixV, outputV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr tank, length, decay, damping, lowCut, width, mix, output;

private:
    SqSpring::Bank bank[3]; double sampleRate = 44100.0; float designedFor[5] = { -1, -1, -1, -1, -1 }, scale = 0.0f; int active = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltageSpringPlugin)
};
