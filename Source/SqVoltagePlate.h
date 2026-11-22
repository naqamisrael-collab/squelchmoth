#pragma once
// Squelchmoth 1.48 - Voltage Plate: a plate reverb from the plate's own vibration (see SqPlate.h), as a plug-in.
// The plate is a thin sheet of steel, 2 m by 1 m and half a millimetre thick (or a small one, 1 m by 0.5 m), pushed
// at one point and heard at two. Its sound is the sum of its own modes - about 26,000 of them for the big plate,
// spread evenly from the bottom to the top, which is why a plate is smooth and dense where a spring chirps.
// That is too many to ring one by one while the music plays, so the plate is "cast": whenever Plate, Decay, Damping
// or Low Cut change, the plate's answer to a single tap is worked out from all its modes (a second or more, in
// the background, the old plate sounding meanwhile) and the music is convolved with it - the same thing, for a
// body that obeys the plate equation. It adds no delay. Every instance has its own plate.

#include <JuceHeader.h>
#include "SqPlate.h"

namespace te = tracktion;

class SqVoltagePlatePlugin : public te::Plugin, private juce::Timer
{
public:
    SqVoltagePlatePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        plateV.referTo (state, "plate", um, 1.0f); decayV.referTo (state, "decay", um, 2.5f); dampingV.referTo (state, "damping", um, 5.0f);
        lowCutV.referTo (state, "lowcut", um, 80.0f); widthV.referTo (state, "width", um, 100.0f); mixV.referTo (state, "mix", um, 25.0f); outputV.referTo (state, "output", um, 0.0f); unit.referTo (state, "unit", nullptr, 0);
        if (unit.get() == 0) unit = 1 + juce::Random::getSystemRandom().nextInt (1000000);
        auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
        auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
        auto num = [] (const juce::String& s) { return s.retainCharacters ("0123456789.-").getFloatValue(); };
        juce::NormalisableRange<float> decayRange (0.5f, 6.0f); decayRange.setSkewForCentre (2.0f);
        juce::NormalisableRange<float> cutRange (20.0f, 500.0f); cutRange.setSkewForCentre (100.0f);
        plate   = addParam ("plate",   TRANS("Plate"),   { 0.0f, 1.0f, 1.0f }, [] (float v) { return plateNames()[v >= 0.5f ? 1 : 0]; }, [] (const juce::String& s) { const auto t = s.trim().toLowerCase(); return t.startsWith ("c") || t.startsWith ("b") || t == "1" ? 1.0f : 0.0f; });
        decay   = addParam ("decay",   TRANS("Decay"),   decayRange, [] (float v) { return juce::String (v, 1) + " s"; }, num);
        damping = addParam ("damping", TRANS("Damping"), { 1.0f, 12.0f }, [] (float v) { return juce::String (v, 1) + " kHz"; }, num);
        lowCut  = addParam ("lowcut",  TRANS("Low Cut"), cutRange, [] (float v) { return juce::String (juce::roundToInt (v)) + " Hz"; }, num);
        width   = addParam ("width",   TRANS("Width"),   { 0.0f, 100.0f }, pct, num);
        mix     = addParam ("mix",     TRANS("Mix"),     { 0.0f, 100.0f }, pct, num);
        output  = addParam ("output",  TRANS("Output"),  { -24.0f, 12.0f }, db, num);
        plate->attachToCurrentValue (plateV); decay->attachToCurrentValue (decayV); damping->attachToCurrentValue (dampingV); lowCut->attachToCurrentValue (lowCutV);
        width->attachToCurrentValue (widthV); mix->attachToCurrentValue (mixV); output->attachToCurrentValue (outputV);
        shared = std::make_shared<Shared>(); startTimerHz (8);
    }

    ~SqVoltagePlatePlugin() override
    {
        stopTimer(); notifyListenersOfDeletion();
        for (auto* p : { plate.get(), decay.get(), damping.get(), lowCut.get(), width.get(), mix.get(), output.get() }) p->detachFromCurrentValue();
    }

    static const char* getPluginName()                  { return NEEDS_TRANS("Voltage Plate"); }
    static constexpr const char* xmlTypeName = "sqmVoltagePlate";

    juce::String getName() const override               { return "Voltage Plate"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "VPlat"; }
    juce::String getSelectableDescription() override    { return TRANS("Plate reverb (physical model)"); }
    bool needsConstantBufferSize() override             { return false; }
    double getLatencySeconds() override                 { return 0.0; }
    double getTailLength() const override               { return 8.0; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }

    static juce::StringArray plateNames() { return { "Small", "Classic" }; }

    /** Everything a cast depends on. */
    struct Key
    {
        int plateIx = 1, unitSeed = 0; float decay = 2.5f, damping = 5.0f, lowCut = 80.0f; double rate = 44100.0;
        bool operator== (const Key& o) const { return plateIx == o.plateIx && unitSeed == o.unitSeed && decay == o.decay && damping == o.damping && lowCut == o.lowCut && rate == o.rate; }
        bool operator!= (const Key& o) const { return ! (*this == o); }
    };
    static double tolerance (int unitSeed, int what, double spread)
    {
        juce::uint32 h = (juce::uint32) unitSeed * 2654435761u ^ (juce::uint32) (what + 291) * 2246822519u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
        return spread * (((double) (h & 0xffffff) / (double) 0xffffff) * 2.0 - 1.0);
    }
    /** This unit's plate (rolled a little thicker or thinner) and where its driver and pickups were fixed. */
    static SqPlate::Plate plateFor (const Key& k) { auto p = k.plateIx >= 1 ? SqPlate::classic() : SqPlate::small(); p.thickness *= 1.0 + tolerance (k.unitSeed, 0, 0.02); return p; }
    static SqPlate::Points pointsFor (const Key& k) { SqPlate::Points at; at.driveX += tolerance (k.unitSeed, 1, 0.01); at.driveY += tolerance (k.unitSeed, 2, 0.01); at.leftX += tolerance (k.unitSeed, 3, 0.01); at.leftY += tolerance (k.unitSeed, 4, 0.01); at.rightX += tolerance (k.unitSeed, 5, 0.01); at.rightY += tolerance (k.unitSeed, 6, 0.01); return at; }
    static SqPlate::Loss lossFor (const Key& k) { SqPlate::Loss l; l.seconds = juce::jlimit (0.3, 8.0, (double) k.decay); l.halfAtHz = 1000.0 * juce::jlimit (0.5, 20.0, (double) k.damping); return l; }

    /** Casts the plate: its answer to one tap at the two pickups, scaled so steady noise comes out at the level it went in. */
    static juce::AudioBuffer<float> cast (const Key& k, int* modesOut = nullptr)
    {
        SqPlate::Cast c; const int n = c.design (plateFor (k), pointsFor (k), lossFor (k), k.rate, (double) k.lowCut, 20000.0); if (modesOut != nullptr) *modesOut = n;
        std::vector<float> l, r; c.tap (l, r, 80.0, 10.0); const double level = 0.5 * (c.level (false) + c.level (true)); const float scale = level > 0.0 ? (float) (1.0 / level) : 0.0f;
        juce::AudioBuffer<float> b (2, juce::jmax (1, (int) l.size())); b.clear();
        for (size_t i = 0; i < l.size(); ++i) { b.setSample (0, (int) i, l[i] * scale); b.setSample (1, (int) i, r[i] * scale); }
        return b;
    }

    /** What these settings come to, from the plate's own arithmetic (no cast needed). */
    struct Reading { int modes = 0; double modesPerHz = 0, lowestHz = 0, decayLow = 0, decayHigh = 0, tapSeconds = 0, lengthM = 0, widthM = 0; float curveHz[24] = {}, curveSeconds[24] = {}; };
    static Reading measure (const Key& k)
    {
        Reading r; const auto p = plateFor (k); const auto loss = lossFor (k); r.modesPerHz = p.modesPerHz(); r.lowestHz = p.hz (1, 1); r.lengthM = p.lx; r.widthM = p.ly;
        for (int m = 1; p.hz (m, 1) <= 20000.0; ++m) for (int n = 1; ; ++n) { const double f = p.hz (m, n); if (f > 20000.0) break; if (f >= (double) k.lowCut) ++r.modes; }
        r.decayLow = 6.907755278982137 / loss.sigma (500.0); r.decayHigh = 6.907755278982137 / loss.sigma (5000.0); r.tapSeconds = juce::jmin (10.0, 80.0 * 0.11512925464970229 / loss.sigma ((double) k.lowCut));
        for (int i = 0; i < 24; ++i) { const double f = 50.0 * std::pow (400.0, i / 23.0); r.curveHz[i] = (float) f; r.curveSeconds[i] = (float) (6.907755278982137 / loss.sigma (f)); }
        return r;
    }

    Key currentKey() const { Key k; k.plateIx = plate->getCurrentValue() >= 0.5f ? 1 : 0; k.unitSeed = unit.get(); k.decay = decay->getCurrentValue(); k.damping = damping->getCurrentValue(); k.lowCut = lowCut->getCurrentValue(); k.rate = sampleRate; return k; }
    /** True while a new plate is being cast in the background. */
    bool isCasting() const { return shared->busy.load(); }
    int modesCast() const { return shared->modes.load(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0; maxBlock = juce::jmax (64, info.blockSizeSamples); scratch.setSize (2, maxBlock, false, false, true);
        const auto k = currentKey(); juce::AudioBuffer<float> copy; bool found = false;
        { const juce::SpinLock::ScopedLockType lock (shared->lock); if (shared->haveKept && shared->keptKey == k) { copy = shared->kept; found = true; } }
        if (! found)       // a plate that has not been cast yet is cast here and now, so the first note of a bounce already has its reverb
        {
            int n = 0; auto fresh = cast (k, &n); copy = fresh; shared->modes = n;
            const juce::SpinLock::ScopedLockType lock (shared->lock); shared->kept = std::move (fresh); shared->keptKey = k; shared->haveKept = true;
        }
        wantedKey = k; convolution.loadImpulseResponse (std::move (copy), sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        convolution.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 }); convolution.reset(); ready = true;
    }
    void deinitialise() override { ready = false; }
    void reset() override { convolution.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! ready) return;
        SCOPED_REALTIME_CHECK
        {
            // a plate cast in the background is taken up here, on the audio thread, as the convolution engine asks
            const juce::SpinLock::ScopedTryLockType lock (shared->lock);
            if (lock.isLocked() && shared->fresh) { shared->fresh = false; convolution.loadImpulseResponse (std::move (shared->buffer), sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no); }
        }
        auto& buf = *fc.destBuffer; const int nch = juce::jmin (2, buf.getNumChannels());
        const float w = juce::jlimit (0.0f, 1.0f, width->getCurrentValue() / 100.0f), outGain = std::pow (10.0f, output->getCurrentValue() / 20.0f), wet = juce::jlimit (0.0f, 1.0f, mix->getCurrentValue() / 100.0f);
        for (int done = 0; done < fc.bufferNumSamples; done += maxBlock)
        {
            const int n = juce::jmin (maxBlock, fc.bufferNumSamples - done); auto* l = buf.getWritePointer (0, fc.bufferStartSample + done); auto* r = nch > 1 ? buf.getWritePointer (1, fc.bufferStartSample + done) : nullptr;
            auto* sl = scratch.getWritePointer (0); auto* sr = scratch.getWritePointer (1);
            for (int i = 0; i < n; ++i) { const float drive = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i]; sl[i] = drive; sr[i] = drive; }
            juce::dsp::AudioBlock<float> block (scratch.getArrayOfWritePointers(), 2, (size_t) n); convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
            for (int i = 0; i < n; ++i)
            {
                const float mid = 0.5f * (sl[i] + sr[i]), side = 0.5f * (sl[i] - sr[i]) * w, wl = mid + side, wr = mid - side;
                if (r == nullptr) l[i] = (l[i] + wet * (mid - l[i])) * outGain; else { l[i] = (l[i] + wet * (wl - l[i])) * outGain; r[i] = (r[i] + wet * (wr - r[i])) * outGain; }
            }
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, plateV, decayV, dampingV, lowCutV, widthV, mixV, outputV);
        te::copyPropertiesToCachedValues (v, unit);
        for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    }

    juce::CachedValue<float> plateV, decayV, dampingV, lowCutV, widthV, mixV, outputV;
    juce::CachedValue<int> unit;
    te::AutomatableParameter::Ptr plate, decay, damping, lowCut, width, mix, output;

private:
    struct Shared { juce::SpinLock lock; juce::AudioBuffer<float> buffer, kept; Key keptKey; bool fresh = false, haveKept = false; std::atomic<bool> busy { false }; std::atomic<int> modes { 0 }; };
    static juce::ThreadPool& pool() { static juce::ThreadPool p (1); return p; }

    /** On the message thread: when the settings no longer match the plate that was asked for, a new one is cast in the background. */
    void timerCallback() override
    {
        if (! ready || shared->busy.load()) return;
        const auto k = currentKey(); if (k == wantedKey) return;
        wantedKey = k; shared->busy = true; auto sh = shared;
        pool().addJob ([sh, k] { int n = 0; auto b = cast (k, &n); juce::AudioBuffer<float> keep (b); { const juce::SpinLock::ScopedLockType lock (sh->lock); sh->buffer = std::move (b); sh->fresh = true; sh->kept = std::move (keep); sh->keptKey = k; sh->haveKept = true; } sh->modes = n; sh->busy = false; });
    }

    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 1024 } }; juce::AudioBuffer<float> scratch; std::shared_ptr<Shared> shared;
    Key wantedKey; bool ready = false; double sampleRate = 44100.0; int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqVoltagePlatePlugin)
};
