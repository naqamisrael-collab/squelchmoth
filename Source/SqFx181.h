#pragma once
// Squelchmoth 1.8.1 effects:
//  Time Weaver  - time + volume envelope slicer: 32 steps over two bars, 36 factory slots per envelope, MIDI-selectable
//  Larva Filter - eight filter stages with a shared 16-step sequencer, LFO and envelope follower per stage
//  Apex Triad - three-band maximiser with per-band history for the envelope view
#include "SqApex.h"
#include "SqSynthCore.h"

//==============================================================================
class SqTimeWeaverPlugin : public SqApexBase
{
public:
    static constexpr int steps = 32, numSlots = 36;
    enum P { timeSlot, volSlot, timeMix, volMix, smooth, tempoSyncP, halfTimeFilter, timeRamp, tStepFirst, vStepFirst = tStepFirst + steps, level = vStepFirst + steps, numParams };

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "timeSlot", "Time Slot", 0, (float) numSlots, 0, 1, "", 0 },
                { "volSlot",  "Volume Slot", 0, (float) numSlots, 0, 1, "", 0 },
                { "timeMix",  "Time Mix", 0, 100, 100, 1, " %", 0 },
                { "volMix",   "Volume Mix", 0, 100, 100, 1, " %", 0 },
                { "smooth",   "Smoothing", 0, 100, 25, 1, " ms", 0 },
                { "tempoSync", "Tempo Sync", 0, 1, 1, 1, "", 0 },
                { "halfTimeFilter", "Tape Tone", 0, 1, 0.3f, 1, "", 2 },
                { "timeRamp", "Time Ramp", 0, 1, 0, 1, "", 0 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            for (int i = 1; i <= steps; ++i) v.push_back ({ addS ("t" + std::to_string (i)), addS ("Time Step " + std::to_string (i)), 0, 8, 0, 1, " beats", 2 });
            for (int i = 1; i <= steps; ++i) v.push_back ({ addS ("v" + std::to_string (i)), addS ("Volume Step " + std::to_string (i)), 0, 1, 1, 1, "", 2 });
            v.push_back ({ "level", "Level", -24, 12, 0, 1, " dB", 1 });
            return v;
        }();
        return s;
    }

    /** Factory slot shapes. Time slots give a delay in beats per 1/16 step (0 = live); volume slots give a gain 0..1. */
    struct Slot { const char* name; std::array<float, steps> values; bool ramp = false; };   // ramp: interpolate between steps (speed changes); flat: jump (stutters)
    static const std::array<Slot, numSlots>& timeSlots()
    {
        static const std::array<Slot, numSlots> s = []
        {
            std::array<Slot, numSlots> out {};
            auto fill = [] (const char* name, std::function<float (int)> f, bool ramp = false) { Slot sl; sl.name = name; sl.ramp = ramp; for (int i = 0; i < steps; ++i) sl.values[(size_t) i] = f (i); return sl; };
            auto beat = [] (int i) { return (float) i * 0.25f; };   // beats since the start of the 2-bar cycle
            int k = 0;
            out[(size_t) k++] = fill ("Live", [] (int) { return 0.0f; });
            out[(size_t) k++] = fill ("Half Time", [&] (int i) { return beat (i) * 0.5f; }, true);                          // plays at half speed: delay grows half a beat per beat
            out[(size_t) k++] = fill ("Half Time Bar", [&] (int i) { return std::fmod (beat (i), 4.0f) * 0.5f; }, true);
            out[(size_t) k++] = fill ("Half Time 1/2 Bar", [&] (int i) { return std::fmod (beat (i), 2.0f) * 0.5f; }, true);
            out[(size_t) k++] = fill ("Quarter Time", [&] (int i) { return std::fmod (beat (i), 4.0f) * 0.75f; }, true);
            out[(size_t) k++] = fill ("Stutter 1/16", [&] (int) { return 0.25f; });                                   // every step repeats the previous 1/16
            out[(size_t) k++] = fill ("Stutter 1/8", [&] (int i) { return (float) (i % 2) * 0.25f; });
            out[(size_t) k++] = fill ("Stutter 1/4", [&] (int i) { return (float) (i % 4) * 0.25f; });
            out[(size_t) k++] = fill ("Stutter 1/2", [&] (int i) { return (float) (i % 8) * 0.25f; });
            out[(size_t) k++] = fill ("Repeat Bar", [&] (int i) { return i >= 16 ? 4.0f : 0.0f; });
            out[(size_t) k++] = fill ("Repeat Beat 2", [&] (int i) { return (i % 16) >= 12 ? 1.0f : 0.0f; });
            out[(size_t) k++] = fill ("Repeat Last 1/8", [&] (int i) { return (i % 8) >= 6 ? 0.5f : 0.0f; });
            out[(size_t) k++] = fill ("Reverse Beat", [&] (int i) { return 2.0f * (float) (i % 4) * 0.25f; }, true);        // reading backwards: delay grows twice as fast as time
            out[(size_t) k++] = fill ("Reverse Bar", [&] (int i) { return 2.0f * (float) (i % 16) * 0.25f; }, true);
            out[(size_t) k++] = fill ("Reverse 1/2 Bar", [&] (int i) { return 2.0f * (float) (i % 8) * 0.25f; }, true);
            out[(size_t) k++] = fill ("Scratch", [&] (int i) { const float t = (float) (i % 8) / 8.0f; return 1.0f - std::abs (t * 2.0f - 1.0f); }, true);
            out[(size_t) k++] = fill ("Tape Stop Beat 4", [&] (int i) { const int b = i % 16; return b >= 12 ? (float) (b - 12) * (float) (b - 12) * 0.06f : 0.0f; }, true);
            out[(size_t) k++] = fill ("Tape Stop Bar", [&] (int i) { const float t = (float) i / 32.0f; return t * t * 6.0f; }, true);
            out[(size_t) k++] = fill ("Build 1/16 > 1/32", [&] (int i) { return i < 16 ? (float) (i % 4) * 0.25f : (float) (i % 2) * 0.125f; });
            out[(size_t) k++] = fill ("Triplet Stutter", [&] (int i) { return (float) (i % 3) * 0.25f; });
            out[(size_t) k++] = fill ("Off-beat Echo", [&] (int i) { return (i % 2) ? 0.5f : 0.0f; });
            out[(size_t) k++] = fill ("Dotted Echo", [&] (int i) { return (i % 4) == 3 ? 0.75f : 0.0f; });
            out[(size_t) k++] = fill ("Half Time + Stutter", [&] (int i) { return i < 24 ? std::fmod (beat (i), 4.0f) * 0.5f : 0.25f; }, true);
            out[(size_t) k++] = fill ("Double Time", [&] (int i) { return 4.0f - std::fmod (beat (i), 4.0f); }, true);   // reads the previous bar at twice the speed
            out[(size_t) k++] = fill ("Skip Beat", [&] (int i) { return ((i / 4) % 4) == 1 ? 1.0f : 0.0f; });
            out[(size_t) k++] = fill ("Drunk", [&] (int i) { return std::abs (std::sin ((float) i * 1.7f)) * 0.5f; }, true);
            out[(size_t) k++] = fill ("Ramp Down Bar", [&] (int i) { return (float) (i % 16) / 16.0f * 2.0f; }, true);
            out[(size_t) k++] = fill ("Freeze Beat 1", [&] (int i) { const int b = i % 16; return b < 4 ? 0.0f : (float) (b - (b % 4)) * 0.25f; });
            out[(size_t) k++] = fill ("Gate Repeat 1/32", [&] (int i) { return (i % 2) ? 0.125f : 0.0f; });
            out[(size_t) k++] = fill ("Trap Roll", [&] (int i) { const int b = i % 16; return b >= 14 ? 0.0625f : (b >= 12 ? 0.125f : 0.0f); });
            out[(size_t) k++] = fill ("Half Time Bar 2", [&] (int i) { return i >= 16 ? std::fmod (beat (i), 4.0f) * 0.5f : 0.0f; }, true);
            out[(size_t) k++] = fill ("Reverse Bar 2", [&] (int i) { return i >= 16 ? 2.0f * (float) (i % 16) * 0.25f : 0.0f; }, true);
            out[(size_t) k++] = fill ("Shuffle", [&] (int i) { return (i % 2) ? 0.08f : 0.0f; });
            out[(size_t) k++] = fill ("Ping Pong 1/8", [&] (int i) { return (float) ((i / 2) % 2) * 0.5f; });
            out[(size_t) k++] = fill ("Slow Down End", [&] (int i) { return i >= 24 ? (float) (i - 24) * 0.1f : 0.0f; }, true);
            out[(size_t) k++] = fill ("Stop Start", [&] (int i) { return (i % 8) >= 4 ? (float) ((i % 8) - 3) * 0.25f : 0.0f; }, true);
            jassert (k == numSlots);
            return out;
        }();
        return s;
    }
    static const std::array<Slot, numSlots>& volumeSlots()
    {
        static const std::array<Slot, numSlots> s = []
        {
            std::array<Slot, numSlots> out {};
            auto fill = [] (const char* name, std::function<float (int)> f) { Slot sl; sl.name = name; for (int i = 0; i < steps; ++i) sl.values[(size_t) i] = juce::jlimit (0.0f, 1.0f, f (i)); return sl; };
            int k = 0;
            out[(size_t) k++] = fill ("Full", [] (int) { return 1.0f; });
            out[(size_t) k++] = fill ("Gate 1/16", [] (int i) { return (i % 2) ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Gate 1/8", [] (int i) { return ((i / 2) % 2) ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Gate 1/4", [] (int i) { return ((i / 4) % 2) ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Gate Off-beat", [] (int i) { return (i % 2) ? 1.0f : 0.0f; });
            out[(size_t) k++] = fill ("Side-chain Pump", [] (int i) { const float t = (float) (i % 4) / 4.0f; return t < 0.1f ? 0.1f : 0.1f + 0.9f * std::pow ((t - 0.1f) / 0.9f, 0.6f); });
            out[(size_t) k++] = fill ("Pump Soft", [] (int i) { const float t = (float) (i % 4) / 4.0f; return 0.4f + 0.6f * t; });
            out[(size_t) k++] = fill ("Pump Hard", [] (int i) { const float t = (float) (i % 4) / 4.0f; return t < 0.25f ? 0.0f : (t - 0.25f) / 0.75f; });
            out[(size_t) k++] = fill ("Pump 1/2", [] (int i) { const float t = (float) (i % 8) / 8.0f; return 0.15f + 0.85f * t; });
            out[(size_t) k++] = fill ("Pump 1/8", [] (int i) { const float t = (float) (i % 2) / 2.0f; return 0.3f + 0.7f * t; });
            out[(size_t) k++] = fill ("Trance Gate", [] (int i) { static const int p[16] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1 }; return (float) p[i % 16]; });
            out[(size_t) k++] = fill ("Trance Gate 2", [] (int i) { static const int p[16] = { 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0 }; return (float) p[i % 16]; });
            out[(size_t) k++] = fill ("Triplet Gate", [] (int i) { return (i % 3) ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Fade In Bar", [] (int i) { return (float) (i % 16) / 15.0f; });
            out[(size_t) k++] = fill ("Fade Out Bar", [] (int i) { return 1.0f - (float) (i % 16) / 15.0f; });
            out[(size_t) k++] = fill ("Fade In 2 Bars", [] (int i) { return (float) i / 31.0f; });
            out[(size_t) k++] = fill ("Fade Out 2 Bars", [] (int i) { return 1.0f - (float) i / 31.0f; });
            out[(size_t) k++] = fill ("Tremolo 1/8", [] (int i) { return 0.5f + 0.5f * std::cos ((float) i * 3.14159f * 0.5f); });
            out[(size_t) k++] = fill ("Tremolo 1/4", [] (int i) { return 0.5f + 0.5f * std::cos ((float) i * 3.14159f * 0.25f); });
            out[(size_t) k++] = fill ("Swell", [] (int i) { return std::sin ((float) (i % 8) / 8.0f * 3.14159f); });
            out[(size_t) k++] = fill ("Mute Beat 4", [] (int i) { return (i % 16) >= 12 ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Mute Bar 2", [] (int i) { return i >= 16 ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Drop Last 1/8", [] (int i) { return (i % 16) >= 14 ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Half Volume Off-beats", [] (int i) { return (i % 2) ? 0.5f : 1.0f; });
            out[(size_t) k++] = fill ("Accent Downbeats", [] (int i) { return (i % 4) == 0 ? 1.0f : 0.6f; });
            out[(size_t) k++] = fill ("Stutter Fade", [] (int i) { return (i % 2) ? 0.0f : 1.0f - (float) (i % 8) / 8.0f; });
            out[(size_t) k++] = fill ("Ratchet Build", [] (int i) { return i < 16 ? (((i / 2) % 2) ? 0.0f : 1.0f) : ((i % 2) ? 0.0f : 1.0f); });
            out[(size_t) k++] = fill ("Reverse Pump", [] (int i) { const float t = (float) (i % 4) / 4.0f; return 1.0f - 0.9f * t; });
            out[(size_t) k++] = fill ("Duck 1/8", [] (int i) { return (i % 2) ? 1.0f : 0.3f; });
            out[(size_t) k++] = fill ("Random Gate", [] (int i) { static const int p[32] = { 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1 }; return (float) p[i]; });
            out[(size_t) k++] = fill ("Dotted Gate", [] (int i) { static const int p[16] = { 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1 }; return (float) p[i % 16]; });
            out[(size_t) k++] = fill ("Saw Up 1/4", [] (int i) { return (float) (i % 4) / 4.0f + 0.25f; });
            out[(size_t) k++] = fill ("Saw Down 1/4", [] (int i) { return 1.0f - (float) (i % 4) / 4.0f; });
            out[(size_t) k++] = fill ("Long Swell 2 Bars", [] (int i) { return std::sin ((float) i / 32.0f * 3.14159f); });
            out[(size_t) k++] = fill ("Kill Switch 1/32", [] (int i) { return (i % 4) == 3 ? 0.0f : 1.0f; });
            out[(size_t) k++] = fill ("Quiet", [] (int) { return 0.25f; });
            jassert (k == numSlots);
            return out;
        }();
        return s;
    }
    static juce::StringArray timeSlotNames()   { juce::StringArray s { "USER" }; for (auto& t : timeSlots()) s.add (t.name); return s; }
    static juce::StringArray volumeSlotNames() { juce::StringArray s { "USER" }; for (auto& t : volumeSlots()) s.add (t.name); return s; }

    SqTimeWeaverPlugin (te::PluginCreationInfo info) : SqApexBase (info)   { bank.create (*this, paramSpecs()); }
    ~SqTimeWeaverPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    static const char* getPluginName()                  { return NEEDS_TRANS("Time Weaver"); }
    static constexpr const char* xmlTypeName = "sqmTimeWeaver";
    juce::String getName() const override               { return "Time Weaver"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "TimeWv"; }
    juce::String getSelectableDescription() override    { return "Time Weaver time / volume slicer"; }
    bool takesMidiInput() override                      { return true; }
    double getLatencySeconds() override                 { return 0.0; }

    /** Display: the live step and the envelopes in use. */
    std::atomic<int> currentStep { -1 }, liveTimeSlot { 0 }, liveVolSlot { 0 };
    std::atomic<float> liveDelayBeats { 0 }, liveGain { 1 };
    float timeValue (int step) const   { const int sl = midiTimeSlot >= 0 ? midiTimeSlot : (int) std::lround (bank[timeSlot]); return sl <= 0 ? bank[tStepFirst + step] : timeSlots()[(size_t) juce::jlimit (0, numSlots - 1, sl - 1)].values[(size_t) step]; }
    bool timeRamps() const             { const int sl = midiTimeSlot >= 0 ? midiTimeSlot : (int) std::lround (bank[timeSlot]); return sl <= 0 ? bank[timeRamp] > 0.5f : timeSlots()[(size_t) juce::jlimit (0, numSlots - 1, sl - 1)].ramp; }
    /** Delay in beats at a position in the 2-bar cycle: flat per step, or a ramp towards the next step's value. */
    float delayAt (double cyclePos, int step) const
    {
        const float v0 = timeValue (step);
        if (! timeRamps()) return v0;
        const float frac = (float) (cyclePos * 4.0 - step);
        float v1 = step < steps - 1 ? timeValue (step + 1) : v0;
        float slope = step > 0 ? v0 - timeValue (step - 1) : 0.0f;
        if (std::abs (slope) > 1.0f) slope = 0.0f;
        if (step == steps - 1 || std::abs (v1 - v0) > 1.0f) v1 = v0 + slope;   // a jump ahead (cycle or bar wrap): keep the slope, the jump itself happens at the step edge with a crossfade
        return v0 + (v1 - v0) * frac;
    }
    float volumeValue (int step) const { const int sl = midiVolSlot >= 0 ? midiVolSlot : (int) std::lround (bank[volSlot]); return sl <= 0 ? bank[vStepFirst + step] : volumeSlots()[(size_t) juce::jlimit (0, numSlots - 1, sl - 1)].values[(size_t) step]; }
    /** Copy a factory slot into the user steps (message thread). */
    void copySlotToUser (bool time, int slot)
    {
        if (slot < 1 || slot > numSlots) return;
        auto& src = time ? timeSlots()[(size_t) (slot - 1)] : volumeSlots()[(size_t) (slot - 1)];
        auto params = getAutomatableParameters();
        for (int i = 0; i < steps; ++i)
            if (auto* p = params[(time ? tStepFirst : vStepFirst) + i]) p->setParameter (src.values[(size_t) i], juce::sendNotification);
        if (auto* p = params[time ? timeSlot : volSlot]) p->setParameter (0.0f, juce::sendNotification);
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        bufLen = juce::nextPowerOfTwo ((int) (sr * 8.0 * 60.0 / 40.0) + 1);   // 8 beats at 40 BPM
        for (auto& b : buf) b.assign ((size_t) bufLen, 0.0f);
        writePos = 0; freeBeat = 0; delaySm = 0; gainSm = 1; xfade = 1; xfadeFrom = 0; curDelay = 0;
        for (auto& f : tone) f.reset();
    }
    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        if (fc.bufferForMidiMessages != nullptr)
            for (auto& m : *fc.bufferForMidiMessages)
            {
                if (m.isNoteOn())
                {
                    const int n = m.getNoteNumber();
                    if (n >= 36 && n < 36 + numSlots) midiTimeSlot = n - 36 + 1;
                    else if (n >= 72 && n < 72 + numSlots) midiVolSlot = n - 72 + 1;
                }
                else if (m.isNoteOff())
                {
                    const int n = m.getNoteNumber();
                    if (n >= 36 && n < 36 + numSlots && midiTimeSlot == n - 36 + 1) midiTimeSlot = -1;
                    else if (n >= 72 && n < 72 + numSlots && midiVolSlot == n - 72 + 1) midiVolSlot = -1;
                }
            }
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0 || bufLen == 0) return;
        const double bpm = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
        const double samplesPerBeat = sr * 60.0 / juce::jmax (20.0, bpm);
        double beat = fc.isPlaying && bank[tempoSyncP] > 0.5f ? edit.tempoSequence.toBeats (fc.editTime.getStart()).inBeats() : freeBeat;
        const float tMix = bank[timeMix] / 100.0f, vMix = bank[volMix] / 100.0f, outG = Apex::dbToGain (bank[level]);
        const float smoothCoef = (float) std::exp (-1.0 / (juce::jmax (0.5f, bank[smooth]) * 0.001 * sr));
        const int xfadeLen = juce::jmax (32, (int) (0.004 * sr));
        for (auto& f : tone) f.set (sr, 2500.0 + 16000.0 * (1.0 - bank[halfTimeFilter]), 0.6);
        liveTimeSlot = midiTimeSlot >= 0 ? midiTimeSlot : (int) std::lround (bank[timeSlot]);
        liveVolSlot = midiVolSlot >= 0 ? midiVolSlot : (int) std::lround (bank[volSlot]);
        float* ch[2] = { b.getWritePointer (0, s0), nch > 1 ? b.getWritePointer (1, s0) : nullptr };
        for (int i = 0; i < n; ++i)
        {
            const double cyc = std::fmod (beat, 8.0); const double cycPos = cyc < 0 ? cyc + 8.0 : cyc;
            const int step = juce::jlimit (0, steps - 1, (int) (cycPos * 4.0));
            const float targetDelay = juce::jmax (0.0f, delayAt (cycPos, step)) * tMix;
            const float targetGain = 1.0f + (volumeValue (step) - 1.0f) * vMix;
            if (step != lastStep) { lastStep = step; currentStep = step; }
            // a jump in delay starts a short crossfade from the old read head
            const float maxRampPerSample = (float) (8.0 / samplesPerBeat) * 4.0f;   // a ramp moves the read head at most ~4x real time
            if (std::abs (targetDelay - curDelay) > maxRampPerSample) { xfadeFrom = curDelay; xfade = 0; }
            curDelay = targetDelay;
            if (xfade < 1.0f) xfade = juce::jmin (1.0f, xfade + 1.0f / (float) xfadeLen);
            gainSm += (1.0f - smoothCoef) * (targetGain - gainSm);
            for (int c = 0; c < nch; ++c)
            {
                const float in = ch[c][i];
                buf[(size_t) c][(size_t) writePos] = in;
                auto readAt = [&] (float delayBeats)
                {
                    if (delayBeats <= 1.0e-5f) return in;
                    const double d = delayBeats * samplesPerBeat;
                    double rp = (double) writePos - d; while (rp < 0) rp += bufLen;
                    const int i0 = (int) rp, i1 = (i0 + 1) % bufLen; const float fr = (float) (rp - i0);
                    float y = buf[(size_t) c][(size_t) i0] + (buf[(size_t) c][(size_t) i1] - buf[(size_t) c][(size_t) i0]) * fr;
                    return y;
                };
                float y = xfade < 1.0f ? readAt (xfadeFrom) * (1.0f - xfade) + readAt (curDelay) * xfade : readAt (curDelay);
                if (curDelay > 1.0e-4f && bank[halfTimeFilter] > 0.01f) y = (float) tone[(size_t) c].lowpass (y);
                ch[c][i] = y * gainSm * outG;
            }
            writePos = (writePos + 1) % bufLen;
            beat += 1.0 / samplesPerBeat;
        }
        if (! (fc.isPlaying && bank[tempoSyncP] > 0.5f)) freeBeat = beat; else freeBeat = beat;
        liveDelayBeats = curDelay; liveGain = gainSm;
    }

private:
    std::array<std::vector<float>, 2> buf; int bufLen = 0, writePos = 0, lastStep = -1;
    double freeBeat = 0; float delaySm = 0, gainSm = 1, xfade = 1, xfadeFrom = 0, curDelay = 0;
    int midiTimeSlot = -1, midiVolSlot = -1;
    std::array<Apex::Svf, 2> tone;
};

//==============================================================================
class SqLarvaFilterPlugin : public SqApexBase
{
public:
    static constexpr int stages = 8, steps = 16, perStage = 10;
    enum Stage { sOn = 0, sType, sCut, sRes, sMix, sSeqAmt, sLfoRate, sLfoAmt, sEnvAmt, sDrive };
    enum P { routing, seqRate, seqSwing, seqSmooth, seqOn, inputGain, outputGain, mixP, stageFirst, stepFirst = stageFirst + stages * perStage, numParams = stepFirst + steps };
    static juce::StringArray typeNames()      { return { "LP 12", "LP 24", "HP 12", "HP 24", "BAND", "NOTCH", "PEAK", "ALLPASS" }; }
    static juce::StringArray routingNames()   { return { "SERIES", "PARALLEL", "SPLIT 4+4" }; }
    static juce::StringArray rateNames()      { return { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }; }

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "routing",   "Routing", 0, 2, 0, 1, "", 0 },
                { "seqRate",   "Seq Rate", 0, 5, 2, 1, "", 0 },
                { "seqSwing",  "Seq Swing", 0, 100, 0, 1, " %", 0 },
                { "seqSmooth", "Seq Smooth", 0, 100, 30, 1, " %", 0 },
                { "seqOn",     "Sequencer", 0, 1, 1, 1, "", 0 },
                { "inputGain", "Input", -24, 24, 0, 1, " dB", 1 },
                { "outputGain", "Output", -24, 24, 0, 1, " dB", 1 },
                { "mix",       "Mix", 0, 100, 100, 1, " %", 0 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            for (int i = 1; i <= stages; ++i)
            {
                const auto n = std::to_string (i), p = "f" + n;
                v.push_back ({ addS (p + "On"), addS ("Stage " + n + " On"), 0, 1, i == 1 ? 1.0f : 0.0f, 1, "", 0 });
                v.push_back ({ addS (p + "Type"), addS ("Stage " + n + " Type"), 0, 7, (float) ((i - 1) % 4 == 0 ? 1 : (i - 1) % 4 == 1 ? 4 : (i - 1) % 4 == 2 ? 3 : 5), 1, "", 0 });
                v.push_back ({ addS (p + "Cut"), addS ("Stage " + n + " Cutoff"), 20, 20000, 800.0f * (float) i, 0.3f, " Hz", 0 });
                v.push_back ({ addS (p + "Res"), addS ("Stage " + n + " Resonance"), 0, 1, 0.3f, 1, "", 2 });
                v.push_back ({ addS (p + "Mix"), addS ("Stage " + n + " Mix"), 0, 1, 1, 1, "", 2 });
                v.push_back ({ addS (p + "Seq"), addS ("Stage " + n + " Seq Amount"), -1, 1, i == 1 ? 0.6f : 0.0f, 1, "", 2 });
                v.push_back ({ addS (p + "LfoRate"), addS ("Stage " + n + " LFO Rate"), 0.02f, 20, 0.5f, 0.3f, " Hz", 2 });
                v.push_back ({ addS (p + "LfoAmt"), addS ("Stage " + n + " LFO Amount"), -1, 1, 0, 1, "", 2 });
                v.push_back ({ addS (p + "Env"), addS ("Stage " + n + " Env Amount"), -1, 1, 0, 1, "", 2 });
                v.push_back ({ addS (p + "Drive"), addS ("Stage " + n + " Drive"), 0, 1, 0, 1, "", 2 });
            }
            for (int i = 1; i <= steps; ++i) v.push_back ({ addS ("step" + std::to_string (i)), addS ("Step " + std::to_string (i)), 0, 1, (i % 4 == 1) ? 1.0f : (i % 2 == 1 ? 0.6f : 0.3f), 1, "", 2 });
            return v;
        }();
        return s;
    }

    SqLarvaFilterPlugin (te::PluginCreationInfo info) : SqApexBase (info)   { bank.create (*this, paramSpecs()); }
    ~SqLarvaFilterPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    static const char* getPluginName()                  { return NEEDS_TRANS("Larva Filter"); }
    static constexpr const char* xmlTypeName = "sqmLarvaFilter";
    juce::String getName() const override               { return "Larva Filter"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "LarvaF"; }
    juce::String getSelectableDescription() override    { return "Larva Filter eight-stage filter sequencer"; }
    bool takesMidiInput() override                      { return true; }

    std::atomic<int> currentStep { -1 };
    std::array<std::atomic<float>, stages> liveCutoff {};   // the modulated cutoff of each stage for the display
    float stageParam (int stage, Stage which) const   { return bank[stageFirst + stage * perStage + (int) which]; }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto& st : filt) for (auto& f : st) { f.a.reset(); f.b.reset(); }
        for (auto& p : lfoPhase) p = 0; env = 0; seqSm = 0; freeBeat = 0;
    }
    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        if (fc.bufferForMidiMessages != nullptr)
            for (auto& m : *fc.bufferForMidiMessages) { if (m.isNoteOn()) { midiNote = m.getNoteNumber(); envTrig = 1.0f; } else if (m.isNoteOff() && m.getNoteNumber() == midiNote) midiNote = -1; }
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        const double bpm = edit.tempoSequence.getBpmAt (fc.editTime.getStart());
        static const double rates[6] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        const double stepBeats = rates[juce::jlimit (0, 5, (int) std::lround (bank[seqRate]))];
        double beat = fc.isPlaying ? edit.tempoSequence.toBeats (fc.editTime.getStart()).inBeats() : freeBeat;
        const double swing = bank[seqSwing] / 100.0 * 0.33;
        const float inG = Apex::dbToGain (bank[inputGain]), outG = Apex::dbToGain (bank[outputGain]), mx = bank[mixP] / 100.0f;
        const float smoothCoef = (float) std::exp (-1.0 / (juce::jmax (0.001f, bank[seqSmooth] / 100.0f * 0.12f) * sr + 1.0));
        const bool seqEnabled = bank[seqOn] > 0.5f;
        const int route = juce::jlimit (0, 2, (int) std::lround (bank[routing]));
        const float envA = (float) (1.0 - std::exp (-1.0 / (0.003 * sr))), envR = (float) (1.0 - std::exp (-1.0 / (0.15 * sr)));
        // per-block stage setup
        struct StageRun { bool on; int type; float cut, res, mix, seqAmt, lfoInc, lfoAmt, envAmt, drive; float bandGain = 1.0f; } run[stages];
        for (int s = 0; s < stages; ++s)
        {
            run[s].on = stageParam (s, sOn) > 0.5f; run[s].type = juce::jlimit (0, 7, (int) std::lround (stageParam (s, sType)));
            run[s].cut = stageParam (s, sCut); run[s].res = stageParam (s, sRes); run[s].mix = stageParam (s, sMix); run[s].seqAmt = stageParam (s, sSeqAmt);
            run[s].lfoInc = (float) (stageParam (s, sLfoRate) / sr); run[s].lfoAmt = stageParam (s, sLfoAmt); run[s].envAmt = stageParam (s, sEnvAmt); run[s].drive = stageParam (s, sDrive);
        }
        float* ch[2] = { b.getWritePointer (0, s0), nch > 1 ? b.getWritePointer (1, s0) : nullptr };
        const int ctrlInterval = 16;
        for (int i = 0; i < n; ++i)
        {
            if ((i % ctrlInterval) == 0)
            {
                // sequencer value with swing (every other step delayed)
                double pos = beat / stepBeats; const int stepIdx = (int) std::floor (pos); double frac = pos - stepIdx;
                int stepNow = ((stepIdx % steps) + steps) % steps;
                if ((stepNow % 2) == 1 && frac < swing) stepNow = (stepNow + steps - 1) % steps;   // swung step starts late
                if (stepNow != lastStep) { lastStep = stepNow; currentStep = stepNow; }
                const float seqTarget = seqEnabled ? bank[stepFirst + stepNow] : 0.5f;
                seqSm += (1.0f - std::pow (smoothCoef, (float) ctrlInterval)) * (seqTarget - seqSm);
                for (int s = 0; s < stages; ++s)
                {
                    if (! run[s].on) continue;
                    lfoPhase[(size_t) s] += run[s].lfoInc * (float) ctrlInterval; if (lfoPhase[(size_t) s] >= 1.0f) lfoPhase[(size_t) s] -= 1.0f;
                    const float lfo = std::sin (lfoPhase[(size_t) s] * juce::MathConstants<float>::twoPi);
                    const float octs = (seqSm - 0.5f) * 2.0f * run[s].seqAmt * 4.0f + lfo * run[s].lfoAmt * 3.0f + env * run[s].envAmt * 5.0f;
                    const float hz = juce::jlimit (20.0f, 20000.0f, run[s].cut * std::pow (2.0f, octs));
                    liveCutoff[(size_t) s] = hz;
                    const double q = 0.5 + run[s].res * run[s].res * 19.5;
                    run[s].bandGain = (float) std::sqrt (q);   // a narrow band keeps its level: the peak rises with the resonance, like a wah
                    for (int c = 0; c < nch; ++c) { filt[(size_t) s][(size_t) c].a.set (sr, hz, q); filt[(size_t) s][(size_t) c].b.set (sr, hz, 0.7071); }
                }
            }
            float dryL = ch[0][i] * inG, dryR = nch > 1 ? ch[1][i] * inG : dryL;
            const float lvl = juce::jmax (std::abs (dryL), std::abs (dryR));
            env += (lvl > env ? envA : envR) * (lvl - env);
            auto process = [&] (int s, float x, int c) -> float
            {
                auto& f = filt[(size_t) s][(size_t) c];
                if (run[s].drive > 0.001f) x = SqSynth::driveShape (x, run[s].drive, 1) * 1.5f;
                double y;
                switch (run[s].type)
                {
                    case 0: y = f.a.lowpass (x); break;
                    case 1: y = f.b.lowpass (f.a.lowpass (x)); break;
                    case 2: y = f.a.highpass (x); break;
                    case 3: y = f.b.highpass (f.a.highpass (x)); break;
                    case 4: y = f.a.bandpass (x) * run[s].bandGain; break;
                    case 5: y = f.a.notch (x); break;
                    case 6: y = x + f.a.bandpass (x) * (0.5 + run[s].res * 3.0); break;
                    default: y = f.a.allpass (x); break;
                }
                return x + ((float) y - x) * run[s].mix;
            };
            for (int c = 0; c < nch; ++c)
            {
                const float dry = c == 0 ? dryL : dryR; float wet = 0.0f;
                if (route == 0) { wet = dry; for (int s = 0; s < stages; ++s) if (run[s].on) wet = process (s, wet, c); }
                else if (route == 1) { int cnt = 0; for (int s = 0; s < stages; ++s) if (run[s].on) { wet += process (s, dry, c); ++cnt; } wet = cnt > 0 ? wet / std::sqrt ((float) cnt) : dry; }
                else { float a = dry, bb = dry; int ca = 0, cb = 0; for (int s = 0; s < 4; ++s) if (run[s].on) { a = process (s, a, c); ++ca; } for (int s = 4; s < 8; ++s) if (run[s].on) { bb = process (s, bb, c); ++cb; } wet = (ca > 0 && cb > 0) ? (a + bb) * 0.7071f : (ca > 0 ? a : bb); }
                ch[c][i] = (dry + (wet - dry) * mx) * outG;
            }
            beat += 1.0 / (sr * 60.0 / juce::jmax (20.0, bpm));
        }
        if (! fc.isPlaying) freeBeat = beat;
        inDb = Apex::gainToDb (env);
    }

private:
    struct Pair { Apex::Svf a, b; };
    std::array<std::array<Pair, 2>, stages> filt;
    std::array<float, stages> lfoPhase {};
    float env = 0, envTrig = 0, seqSm = 0; int lastStep = -1, midiNote = -1; double freeBeat = 0;
};

//==============================================================================
class SqApexTriadPlugin : public SqApexBase
{
public:
    static constexpr int bands = 3, perBand = 6;
    enum Band { bThr = 0, bGain, bAtt, bRel, bKnee, bMix };
    enum P { inputGain, outputGain, mixP, xLow, xHigh, ceiling, lookahead, bandFirst, numParams = bandFirst + bands * perBand };
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "inputGain",  "Input",  -24, 24, 0, 1, " dB", 1 },
                { "outputGain", "Output", -24, 24, 0, 1, " dB", 1 },
                { "mix",        "Mix", 0, 100, 100, 1, " %", 0 },
                { "xLow",       "Low Crossover", 40, 1000, 160, 0.3f, " Hz", 0 },
                { "xHigh",      "High Crossover", 1000, 12000, 3200, 0.3f, " Hz", 0 },
                { "ceiling",    "Ceiling", -12, 0, -0.3f, 1, " dB", 1 },
                { "lookahead",  "Lookahead", 0, 10, 2, 1, " ms", 1 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            static const char* bn[bands] = { "Low", "Mid", "High" };
            for (int b = 0; b < bands; ++b)
            {
                const std::string n = bn[b], p = std::string (1, (char) ('a' + b));
                v.push_back ({ addS (p + "Thr"), addS (n + " Threshold"), -40, 0, -6, 1, " dB", 1 });
                v.push_back ({ addS (p + "Gain"), addS (n + " Gain"), -12, 12, 0, 1, " dB", 1 });
                v.push_back ({ addS (p + "Att"), addS (n + " Attack"), 0.1f, 200, b == 0 ? 20.0f : (b == 1 ? 8.0f : 2.0f), 0.3f, " ms", 1 });
                v.push_back ({ addS (p + "Rel"), addS (n + " Release"), 5, 2000, b == 0 ? 250.0f : (b == 1 ? 120.0f : 60.0f), 0.3f, " ms", 0 });
                v.push_back ({ addS (p + "Knee"), addS (n + " Knee"), 0, 24, 6, 1, " dB", 1 });
                v.push_back ({ addS (p + "Mix"), addS (n + " Mix"), 0, 100, 100, 1, " %", 0 });
            }
            return v;
        }();
        return s;
    }

    SqApexTriadPlugin (te::PluginCreationInfo info) : SqApexBase (info)   { bank.create (*this, paramSpecs()); }
    ~SqApexTriadPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    static const char* getPluginName()                  { return NEEDS_TRANS("Apex Triad"); }
    static constexpr const char* xmlTypeName = "sqmApexTriad";
    juce::String getName() const override               { return "Apex Triad"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Triad"; }
    juce::String getSelectableDescription() override    { return "Apex Triad three-band maximiser"; }
    double getLatencySeconds() override                 { return bank.specs.empty() ? 0.0 : bank[lookahead] * 0.001; }

    // per-band history for the envelope view (input level and gain reduction, dB)
    std::array<std::array<float, historySize>, bands> bandIn {}, bandGr {};
    std::array<std::atomic<float>, bands> bandInDb {}, bandGrDb {};
    std::atomic<float> limiterGrDb { 0 };
    float bandParam (int b, Band which) const   { return bank[bandFirst + b * perBand + (int) which]; }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto& c : xo1) c.reset(); for (auto& c : xo2) c.reset();
        for (auto& d : det) d.prepare (sr);
        for (auto& bl : bal) bl.state = 0;
        for (auto& h : bandIn) h.fill (-100.0f); for (auto& h : bandGr) h.fill (0.0f); grHistory.fill (0.0f);
        laLen = juce::jmax (1, (int) (0.012 * sr)); for (auto& l : la) l.assign ((size_t) laLen, 0.0f); for (auto& l : laDry) l.assign ((size_t) laLen, 0.0f); laPos = 0; limGain = 1; histCount = 0;
    }
    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        for (auto& c : xo1) c.set (sr, bank[xLow]); for (auto& c : xo2) c.set (sr, bank[xHigh]);
        const float inG = Apex::dbToGain (bank[inputGain]), outG = Apex::dbToGain (bank[outputGain]), mx = bank[mixP] / 100.0f, ceil = Apex::dbToGain (bank[ceiling]);
        struct BR { float thr, gain, knee, mix; } br[bands];
        for (int k = 0; k < bands; ++k) { br[k] = { bandParam (k, bThr), Apex::dbToGain (bandParam (k, bGain)), juce::jmax (0.1f, bandParam (k, bKnee)), bandParam (k, bMix) / 100.0f }; bal[(size_t) k].set (sr, bandParam (k, bAtt), bandParam (k, bRel), 0.0f); }
        const int laSamples = juce::jlimit (1, laLen - 1, (int) (bank[lookahead] * 0.001 * sr));
        const float limRel = (float) std::exp (-1.0 / (0.08 * sr)), limAtt = (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0, laSamples * 0.5) )));
        float* ch[2] = { b.getWritePointer (0, s0), nch > 1 ? b.getWritePointer (1, s0) : nullptr };
        float grAcc[bands] = { 0, 0, 0 }, inAcc[bands] = { -100, -100, -100 }; float limAcc = 0;
        for (int i = 0; i < n; ++i)
        {
            float dry[2], bandS[bands][2];
            for (int c = 0; c < nch; ++c)
            {
                dry[c] = ch[c][i]; const double x = dry[c] * inG;
                double lo, rest, mid, hi; xo1[(size_t) c].split (x, lo, rest); xo2[(size_t) c].split (rest, mid, hi); lo = xo2[(size_t) c].allpass (lo);
                bandS[0][c] = (float) lo; bandS[1][c] = (float) mid; bandS[2][c] = (float) hi;
            }
            float out[2] = { 0, 0 };
            for (int k = 0; k < bands; ++k)
            {
                const float lvl = juce::jmax (std::abs (bandS[k][0]), nch > 1 ? std::abs (bandS[k][1]) : 0.0f);
                const float lvlDb = det[(size_t) k].process (lvl, Apex::Detector::smart);
                // soft-knee upward-friendly maximiser curve: above threshold compress 4:1 towards the threshold, knee width per band
                const float over = lvlDb - br[k].thr; float gr = 0.0f;
                if (over > -br[k].knee * 0.5f)
                {
                    const float kx = over + br[k].knee * 0.5f;
                    gr = over > br[k].knee * 0.5f ? -(over) * 0.75f : -(kx * kx) / (2.0f * br[k].knee) * 0.75f;
                }
                const float grSm = bal[(size_t) k].process (gr);
                // maximiser make-up: a full-scale peak comes back to where it went in, everything under it is lifted, so
                // lowering a band's threshold makes that band louder (and denser), never quieter
                const float g = Apex::dbToGain (grSm - br[k].thr * 0.75f) * br[k].gain;
                grAcc[k] = juce::jmin (grAcc[k], grSm); inAcc[k] = juce::jmax (inAcc[k], lvlDb);
                for (int c = 0; c < nch; ++c) out[c] += bandS[k][c] + (bandS[k][c] * g - bandS[k][c]) * br[k].mix;
            }
            // lookahead brick-wall at the ceiling
            float pk = 0.0f; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, std::abs (out[c] * outG));
            const float need = pk > ceil ? ceil / pk : 1.0f;
            if (need < limGain) limGain += limAtt * (need - limGain); else limGain = need + (limGain - need) * limRel;
            for (int c = 0; c < nch; ++c)
            {
                const float delayed = la[(size_t) c][(size_t) laPos];
                la[(size_t) c][(size_t) laPos] = out[c] * outG;
                const float dryDelayed = laDry[(size_t) c][(size_t) laPos]; laDry[(size_t) c][(size_t) laPos] = dry[c];
                const float wet = juce::jlimit (-ceil, ceil, delayed * limGain);
                ch[c][i] = dryDelayed + (wet - dryDelayed) * mx;
            }
            laPos = (laPos + 1) % laSamples;
            limAcc = juce::jmin (limAcc, Apex::gainToDb (limGain));
            if (++histCount >= 64)
            {
                histCount = 0; const int w = historyWrite.load();
                for (int k = 0; k < bands; ++k) { bandIn[(size_t) k][(size_t) (w & (historySize - 1))] = inAcc[k]; bandGr[(size_t) k][(size_t) (w & (historySize - 1))] = grAcc[k]; bandInDb[(size_t) k] = inAcc[k]; bandGrDb[(size_t) k] = grAcc[k]; grAcc[k] = 0; inAcc[k] = -100; }
                grHistory[(size_t) (w & (historySize - 1))] = limAcc; historyWrite = w + 1; limiterGrDb = limAcc; limAcc = 0;
            }
        }
    }

private:
    std::array<Apex::LR4, 2> xo1, xo2;
    std::array<Apex::Detector, bands> det; std::array<Apex::Ballistics, bands> bal;
    std::array<std::vector<float>, 2> la, laDry; int laLen = 1, laPos = 0, histCount = 0; float limGain = 1;
};
