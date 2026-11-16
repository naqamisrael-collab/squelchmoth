#pragma once
// Squelchmoth 1.12 - External Effect (Pro Tools' hardware insert / Studio One's Pipeline).
//
// An insert that sends the track out of a pair of audio-interface outputs to an outboard unit (a hardware compressor,
// a guitar amp, a tape machine) and brings it back on a pair of inputs. Send and return gain, dry / wet mix, and Ping:
// a click goes out and the plug-in times its return to the sample, then reports that round trip as its latency so the
// session's delay compensation lines everything up, and delays its own dry path by the same amount.
//
// How the audio gets there: ExternalFxBridge is a second callback on the audio device. The engine renders first in
// each device callback (the plug-in writes its send into a FIFO and reads its return from another), then the bridge
// copies each send FIFO to the chosen outputs and each chosen input pair into the return FIFO. The bridge's
// processDeviceBlock() is also what the self-test drives with a software loop, so the measurement is the same code.
//
// Offline bounces can't play through outboard gear (the device isn't running in step), as in Pro Tools: a bounce with
// an External Effect passes the dry signal and says so; use Record (or the track's own bounce in real time) to print it.

#include <JuceHeader.h>

namespace te = tracktion;

class ExternalFxBridge;

/** One send / return pair, shared by the plug-in (engine thread) and the bridge (device thread). */
struct ExternalFxPort
{
    static constexpr int fifoSize = 1 << 16;
    ExternalFxPort() : sendFifo (fifoSize), returnFifo (fifoSize) { sendBuf.setSize (2, fifoSize); returnBuf.setSize (2, fifoSize); }

    std::atomic<int> outChannel { 0 }, inChannel { 0 };    // first channel of each stereo pair (0-based)
    juce::AbstractFifo sendFifo, returnFifo;
    juce::AudioBuffer<float> sendBuf, returnBuf;
    std::atomic<int> underruns { 0 };

    void writeFifo (juce::AbstractFifo& f, juce::AudioBuffer<float>& b, const float* const* src, int nch, int n)
    {
        int s1, n1, s2, n2; f.prepareToWrite (n, s1, n1, s2, n2);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* x = src[juce::jmin (ch, nch - 1)];
            if (x == nullptr) { if (n1 > 0) b.clear (ch, s1, n1); if (n2 > 0) b.clear (ch, s2, n2); continue; }
            if (n1 > 0) b.copyFrom (ch, s1, x, n1);
            if (n2 > 0) b.copyFrom (ch, s2, x + n1, n2);
        }
        f.finishedWrite (n1 + n2);
    }
    /** Reads n samples; returns how many were really there (the rest is silence). */
    int readFifo (juce::AbstractFifo& f, juce::AudioBuffer<float>& b, float* const* dst, int nch, int n)
    {
        const int avail = juce::jmin (n, f.getNumReady());
        int s1, n1, s2, n2; f.prepareToRead (avail, s1, n1, s2, n2);
        for (int ch = 0; ch < nch; ++ch)
        {
            if (dst[ch] == nullptr) continue;
            const int sc = juce::jmin (ch, 1);
            if (n1 > 0) juce::FloatVectorOperations::copy (dst[ch], b.getReadPointer (sc, s1), n1);
            if (n2 > 0) juce::FloatVectorOperations::copy (dst[ch] + n1, b.getReadPointer (sc, s2), n2);
            if (avail < n) juce::FloatVectorOperations::clear (dst[ch] + avail, n - avail);
        }
        f.finishedRead (n1 + n2);
        return avail;
    }
};

/** The device-side half: one per engine, added to the audio device as an extra callback while any port exists. */
class ExternalFxBridge : public juce::AudioIODeviceCallback
{
public:
    static ExternalFxBridge& get()   { static ExternalFxBridge b; return b; }

    void addPort (std::shared_ptr<ExternalFxPort> p, juce::AudioDeviceManager* dm)
    {
        { const juce::SpinLock::ScopedLockType sl (lock); ports.push_back (p); }
        if (dm != nullptr && attachedTo == nullptr) { attachedTo = dm; dm->addAudioCallback (this); }
    }
    void removePort (const std::shared_ptr<ExternalFxPort>& p)
    {
        bool empty;
        { const juce::SpinLock::ScopedLockType sl (lock); ports.erase (std::remove (ports.begin(), ports.end(), p), ports.end()); empty = ports.empty(); }
        if (empty && attachedTo != nullptr) { attachedTo->removeAudioCallback (this); attachedTo = nullptr; }
    }

    /** What one device callback does for every port: send FIFO -> outputs, inputs -> return FIFO. */
    void processDeviceBlock (const float* const* in, int numIn, float* const* out, int numOut, int n)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked()) return;
        float* tmp[2] = { scratch.getWritePointer (0), scratch.getWritePointer (1) };
        for (auto& p : ports)
        {
            const int oc = p->outChannel.load(), ic = p->inChannel.load();
            if (n > scratch.getNumSamples()) break;
            p->readFifo (p->sendFifo, p->sendBuf, tmp, 2, n);
            for (int ch = 0; ch < 2; ++ch)
                if (oc + ch < numOut && out[oc + ch] != nullptr) juce::FloatVectorOperations::add (out[oc + ch], tmp[ch], n);
            const float* src[2] = { ic < numIn ? in[ic] : nullptr, ic + 1 < numIn ? in[ic + 1] : (ic < numIn ? in[ic] : nullptr) };
            p->writeFifo (p->returnFifo, p->returnBuf, src, 2, n);
        }
    }

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n, const juce::AudioIODeviceCallbackContext&) override
    {
        for (int ch = 0; ch < numOut; ++ch) if (out[ch] != nullptr) juce::FloatVectorOperations::clear (out[ch], n);
        if (! manualOnly.load()) processDeviceBlock (in, numIn, out, numOut, n);
    }
    /** Self-test: the real device stays out of it and the test drives processDeviceBlock() with a software loop. */
    std::atomic<bool> manualOnly { false };
    void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    int numPorts()   { const juce::SpinLock::ScopedLockType sl (lock); return (int) ports.size(); }

private:
    ExternalFxBridge() { scratch.setSize (2, 16384); }
    juce::SpinLock lock;
    std::vector<std::shared_ptr<ExternalFxPort>> ports;
    juce::AudioDeviceManager* attachedTo = nullptr;
    juce::AudioBuffer<float> scratch;
};

//==============================================================================
class SqExternalFxPlugin : public te::Plugin
{
public:
    SqExternalFxPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        sendDb.referTo (state, "sendDb", um, 0.0f);
        returnDb.referTo (state, "returnDb", um, 0.0f);
        mixPct.referTo (state, "mix", um, 100.0f);
        outPair.referTo (state, "outPair", um, 2);       // outputs 3-4 by default (1-2 are usually the speakers)
        inPair.referTo (state, "inPair", um, 2);         // inputs 3-4
        latencySamples.referTo (state, "latency", um, 0);
        port = std::make_shared<ExternalFxPort>();
        port->outChannel = outPair.get(); port->inChannel = inPair.get();
        ExternalFxBridge::get().addPort (port, edit.isRendering() ? nullptr : &edit.engine.getDeviceManager().deviceManager);
    }
    ~SqExternalFxPlugin() override   { notifyListenersOfDeletion(); ExternalFxBridge::get().removePort (port); }

    static const char* getPluginName()                  { return NEEDS_TRANS("External Effect"); }
    static constexpr const char* xmlTypeName = "sqmExternalFx";

    juce::String getName() const override               { return "External Effect"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "ExtFx"; }
    juce::String getSelectableDescription() override    { return TRANS("Hardware insert"); }
    bool needsConstantBufferSize() override             { return false; }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    double getLatencySeconds() override                 { return sampleRate > 0 ? latencySamples.get() / sampleRate : 0.0; }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 44100.0;
        dry.setSize (2, 1 << 18); dry.clear(); dryPos = 0;
        tmp.setSize (4, 16384); tmp.clear();
        port->sendFifo.reset(); port->returnFifo.reset();
    }
    void deinitialise() override {}

    void setPairs (int outFirst, int inFirst)
    {
        outPair = juce::jmax (0, outFirst); inPair = juce::jmax (0, inFirst);
        port->outChannel = outPair.get(); port->inChannel = inPair.get();
    }

    /** Starts a ping: the next blocks send one click and listen for up to a second. */
    void startPing()   { pingState = 1; }
    bool isPinging() const   { return pingState.load() != 0; }
    /** Last ping result in samples, -1 = nothing came back, -2 = not measured yet. */
    int lastPing() const     { return pingResult.load(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        auto& buf = *fc.destBuffer;
        const int n = fc.bufferNumSamples, st = fc.bufferStartSample, nch = juce::jmin (2, buf.getNumChannels());
        if (fc.isRendering || n > tmp.getNumSamples()) return;   // offline bounce: dry (see the note at the top)
        float* d[2] = { buf.getWritePointer (0, st), buf.getWritePointer (juce::jmin (1, nch - 1), st) };

        // keep the dry signal for the mix, delayed by the round trip
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i) dry.setSample (ch, (dryPos + i) & (dry.getNumSamples() - 1), d[ch][i]);

        // send (or the ping click)
        float* s0 = tmp.getWritePointer (0); float* s1 = tmp.getWritePointer (1);
        const float sg = juce::Decibels::decibelsToGain (sendDb.get());
        for (int i = 0; i < n; ++i) { s0[i] = d[0][i] * sg; s1[i] = d[1][i] * sg; }
        if (pingState.load() == 1)
        {
            for (int i = 0; i < n; ++i) { s0[i] = s1[i] = 0.0f; }
            s0[0] = s1[0] = 0.9f; pingSent = pingClock; pingState = 2; pingBestAmp = 0.0f; pingBestAt = -1;
        }
        else if (pingState.load() == 2) for (int i = 0; i < n; ++i) { s0[i] = s1[i] = 0.0f; }
        const float* sp[2] = { s0, s1 };
        port->writeFifo (port->sendFifo, port->sendBuf, sp, 2, n);

        // return
        float* r0 = tmp.getWritePointer (2); float* r1 = tmp.getWritePointer (3);
        float* rp[2] = { r0, r1 };
        if (port->readFifo (port->returnFifo, port->returnBuf, rp, 2, n) < n) ++port->underruns;
        if (pingState.load() == 2)
        {
            for (int i = 0; i < n; ++i)
                if (std::abs (r0[i]) > pingBestAmp) { pingBestAmp = std::abs (r0[i]); pingBestAt = pingClock + i; }
            if (pingClock + n - pingSent > (juce::int64) sampleRate)
            {
                const int found = pingBestAmp > 0.01f ? (int) (pingBestAt - pingSent) : -1;
                pingResult = found;
                pingState = 0;
                if (found >= 0) juce::MessageManager::callAsync ([w = te::SafeSelectable<te::Plugin> (this), found]
                {
                    if (auto* p = dynamic_cast<SqExternalFxPlugin*> (w.get())) p->applyPing (found);
                });
            }
        }

        const float rg = juce::Decibels::decibelsToGain (returnDb.get()), wet = juce::jlimit (0.0f, 1.0f, mixPct.get() / 100.0f);
        const int lat = juce::jlimit (0, dry.getNumSamples() - 1, (int) latencySamples.get());
        for (int i = 0; i < n; ++i)
        {
            const int di = (dryPos + i - lat) & (dry.getNumSamples() - 1);
            d[0][i] = (pingState.load() != 0 ? 0.0f : 1.0f) * (dry.getSample (0, di) * (1.0f - wet) + r0[i] * rg * wet);
            if (nch > 1) d[1][i] = (pingState.load() != 0 ? 0.0f : 1.0f) * (dry.getSample (1, di) * (1.0f - wet) + r1[i] * rg * wet);
        }
        dryPos = (dryPos + n) & (dry.getNumSamples() - 1);
        pingClock += n;
    }

    /** Message thread: takes a measured round trip as the latency (also what the self-test calls). */
    void applyPing (int samples)   { latencySamples = juce::jmax (0, samples); changed(); }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, sendDb, returnDb, mixPct, outPair, inPair, latencySamples);
        port->outChannel = outPair.get(); port->inChannel = inPair.get();
    }

    juce::CachedValue<float> sendDb, returnDb, mixPct;
    juce::CachedValue<int> outPair, inPair, latencySamples;
    std::shared_ptr<ExternalFxPort> port;

private:
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> dry, tmp; int dryPos = 0;
    std::atomic<int> pingState { 0 }, pingResult { -2 };
    juce::int64 pingClock = 0, pingSent = 0, pingBestAt = -1; float pingBestAmp = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqExternalFxPlugin)
};
