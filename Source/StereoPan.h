#pragma once
// Squelchmoth - Pro Tools-style dual panner for stereo tracks.
//
// Pro Tools gives a stereo track two pan knobs: the left input channel and the right input channel are each
// panned independently across the stereo output (the usual starting point is L100 / R100). Tracktion's own
// Volume & Pan plugin is a balance control, so Squelchmoth adds this hidden plugin in front of the fader when a
// track is switched to "stereo pan". Both knobs follow the session pan depth (-2.5 / -3 / -4.5 / -6 dB).

#include <JuceHeader.h>

namespace te = tracktion;

class SqStereoPanPlugin : public te::Plugin
{
public:
    SqStereoPanPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        panLValue.referTo (state, "panL", um, -1.0f);
        panRValue.referTo (state, "panR", um, 1.0f);
        linked.referTo (state, "linked", um, false);
        inverse.referTo (state, "inverse", um, false);
        panLawValue.referTo (state, "panLaw", um, (int) te::PanLaw3dBCenter);

        auto toText = [] (float v)
        {
            if (std::abs (v) < 0.005f) return juce::String ("C");
            return v < 0 ? "L" + juce::String (juce::roundToInt (-v * 100.0f)) : "R" + juce::String (juce::roundToInt (v * 100.0f));
        };
        auto fromText = [] (const juce::String& s)
        {
            auto t = s.trim().toUpperCase();
            if (t.startsWith ("L")) return -t.substring (1).getFloatValue() / 100.0f;
            if (t.startsWith ("R")) return t.substring (1).getFloatValue() / 100.0f;
            if (t == "C") return 0.0f;
            return juce::jlimit (-1.0f, 1.0f, t.getFloatValue());
        };
        panL = addParam ("panL", TRANS("Pan L"), { -1.0f, 1.0f }, toText, fromText);
        panR = addParam ("panR", TRANS("Pan R"), { -1.0f, 1.0f }, toText, fromText);
        panL->attachToCurrentValue (panLValue);
        panR->attachToCurrentValue (panRValue);
    }

    ~SqStereoPanPlugin() override
    {
        notifyListenersOfDeletion();
        panL->detachFromCurrentValue();
        panR->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Stereo Pan"); }
    static constexpr const char* xmlTypeName = "sqmStereoPan";

    juce::String getName() const override               { return "Stereo Pan"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "StPan"; }
    juce::String getSelectableDescription() override    { return TRANS("Stereo panner (L/R)"); }
    bool canBeAddedToRack() override                    { return false; }
    bool needsConstantBufferSize() override             { return false; }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        const double sr = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        for (auto* s : { &sLL, &sLR, &sRL, &sRR }) s->reset (sr, 0.01);
        updateGains (true);
    }
    void deinitialise() override {}

    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer;
        if (buf.getNumChannels() < 2) return;
        updateGains (false);
        auto* l = buf.getWritePointer (0, fc.bufferStartSample);
        auto* r = buf.getWritePointer (1, fc.bufferStartSample);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float gLL = sLL.getNextValue(), gLR = sLR.getNextValue(), gRL = sRL.getNextValue(), gRR = sRR.getNextValue();
            const float inL = l[i], inR = r[i];
            l[i] = inL * gLL + inR * gRL;
            r[i] = inL * gLR + inR * gRR;
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, panLValue, panRValue, linked, inverse, panLawValue);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    /** Gains for one input channel panned to 'pan' under the current pan law (unity fader). */
    void gainsFor (float pan, float& gl, float& gr) const
    {
        te::getGainsFromVolumeFaderPositionAndPan (te::decibelsToVolumeFaderPosition (0.0f), juce::jlimit (-1.0f, 1.0f, pan),
                                                   (te::PanLaw) panLawValue.get(), gl, gr);
    }

    juce::CachedValue<float> panLValue, panRValue;
    juce::CachedValue<bool> linked, inverse;
    juce::CachedValue<int> panLawValue;
    te::AutomatableParameter::Ptr panL, panR;

private:
    juce::SmoothedValue<float> sLL, sLR, sRL, sRR;

    void updateGains (bool immediate)
    {
        float gl, gr;
        gainsFor (panL->getCurrentValue(), gl, gr);
        if (immediate) { sLL.setCurrentAndTargetValue (gl); sLR.setCurrentAndTargetValue (gr); } else { sLL.setTargetValue (gl); sLR.setTargetValue (gr); }
        gainsFor (panR->getCurrentValue(), gl, gr);
        if (immediate) { sRL.setCurrentAndTargetValue (gl); sRR.setCurrentAndTargetValue (gr); } else { sRL.setTargetValue (gl); sRR.setTargetValue (gr); }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqStereoPanPlugin)
};

//==============================================================================
/** Squelchmoth - makes a track truly mono, as a Pro Tools mono track is: everything entering the track's insert
    chain (a mono input, a stereo file dropped on it, a stereo instrument or a stereo bus feeding a mono Aux Input)
    is summed to one channel before the inserts, so mono plug-ins, the mono pan knob and the meters all see a single
    signal. Hidden from the insert slots; added by TrackOps::setTrackFormat. */
class SqMonoSumPlugin : public te::Plugin
{
public:
    SqMonoSumPlugin (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~SqMonoSumPlugin() override                         { notifyListenersOfDeletion(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Mono"); }
    static constexpr const char* xmlTypeName = "sqmMonoSum";

    juce::String getName() const override               { return "Mono"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Mono"; }
    juce::String getSelectableDescription() override    { return TRANS("Mono track format"); }
    bool canBeAddedToRack() override                    { return false; }
    bool needsConstantBufferSize() override             { return false; }
    void initialise (const te::PluginInitialisationInfo&) override {}
    void deinitialise() override {}
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2) return;
        SCOPED_REALTIME_CHECK
        auto* l = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);
        auto* r = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float m = (l[i] + r[i]) * 0.5f;
            l[i] = m; r[i] = m;
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqMonoSumPlugin)
};

//==============================================================================
/** 1.12: Pro Tools' Mute automation lane. A hidden gain stage just before the fader (so pre-fader sends are untouched
    and post-fader sends and the output go quiet, as in Pro Tools) with one automatable parameter, Mute (0 = playing,
    1 = muted). Every change ramps over 5 ms, so a mute edge never clicks. Added the first time a track's Mute lane
    is shown or written; hidden from the insert slots. */
class SqMuteAutoPlugin : public te::Plugin
{
public:
    SqMuteAutoPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        muteValue.referTo (state, "mute", getUndoManager(), 0.0f);
        mute = addParam ("mute", TRANS("Mute"), { 0.0f, 1.0f, 1.0f },
                         [] (float v) { return v >= 0.5f ? juce::String ("muted") : juce::String ("playing"); },
                         [] (const juce::String& s) { auto t = s.trim().toLowerCase(); return (t == "muted" || t == "on" || t == "1" || t == "true") ? 1.0f : 0.0f; });
        mute->attachToCurrentValue (muteValue);
    }
    ~SqMuteAutoPlugin() override                        { notifyListenersOfDeletion(); mute->detachFromCurrentValue(); }

    static const char* getPluginName()                  { return NEEDS_TRANS("Mute Lane"); }
    static constexpr const char* xmlTypeName = "sqmMuteAuto";

    juce::String getName() const override               { return "Mute Lane"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Mute"; }
    juce::String getSelectableDescription() override    { return TRANS("Mute automation"); }
    bool canBeAddedToRack() override                    { return false; }
    bool needsConstantBufferSize() override             { return false; }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        gain.reset (info.sampleRate > 0 ? info.sampleRate : 44100.0, 0.005);
        gain.setCurrentAndTargetValue (isMuted() ? 0.0f : 1.0f);
    }
    void deinitialise() override {}
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    bool isMuted() const                                { return mute->getCurrentValue() >= 0.5f; }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        gain.setTargetValue (isMuted() ? 0.0f : 1.0f);
        if (! gain.isSmoothing())
        {
            if (gain.getTargetValue() < 0.5f) fc.destBuffer->clear (fc.bufferStartSample, fc.bufferNumSamples);
            return;
        }
        const int nch = fc.destBuffer->getNumChannels();
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float g = gain.getNextValue();
            for (int ch = 0; ch < nch; ++ch) fc.destBuffer->getWritePointer (ch, fc.bufferStartSample)[i] *= g;
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, muteValue);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> muteValue;
    te::AutomatableParameter::Ptr mute;

private:
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqMuteAutoPlugin)
};

/** 1.12: stepped (on / off) editing of a Mute lane curve. A mute lane holds pairs of points 1 ms apart, so playback
    and the drawn lane both switch at the exact time, never ramp across. */
namespace MuteLane
{
    inline bool valueAt (te::AutomationCurve& c, te::TimePosition t, float fallback)
    {
        return (c.getNumPoints() > 0 ? c.getValueAt (t, fallback) : fallback) >= 0.5f;
    }

    /** Sets [from, to) to muted / playing and keeps the state either side as it was. */
    inline void paint (te::AutomationCurve& c, te::TimePosition from, te::TimePosition to, bool muted, juce::UndoManager* um)
    {
        if (to <= from) return;
        const auto eps = te::TimeDuration::fromSeconds (0.001);
        const float base = 0.0f;
        const bool before = valueAt (c, from - eps, base), after = valueAt (c, to, base);
        c.removePointsInRegion ({ te::TimePosition::fromSeconds (juce::jmax (0.0, (from - eps).inSeconds())), to + eps }, um);
        const float v = muted ? 1.0f : 0.0f;
        if (before != muted && from.inSeconds() >= 0.001) c.addPoint (from - eps, before ? 1.0f : 0.0f, 0.0f, um);
        c.addPoint (from, v, 0.0f, um);
        c.addPoint (to, v, 0.0f, um);
        if (after != muted) c.addPoint (to + eps, after ? 1.0f : 0.0f, 0.0f, um);
        // tidy: drop points that sit between two points of the same value (keeps the lane to its edges)
        for (int i = c.getNumPoints() - 2; i >= 1; --i)
            if ((c.getPointValue (i - 1) >= 0.5f) == (c.getPointValue (i) >= 0.5f) && (c.getPointValue (i) >= 0.5f) == (c.getPointValue (i + 1) >= 0.5f))
                c.removePoint (i, um);
    }
}
