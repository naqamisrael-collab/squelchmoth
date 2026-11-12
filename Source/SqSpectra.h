#pragma once
// Spectra EQ (1.14 "Studio Line", phase A): the flagship 24-band EQ. DSP: SqSpectraDsp.h (tests/spectra_test.cpp).
// Face: SkinsSpectra.inl. Claude: `spectra_eq` (McpToolsSpectra.h).
//
// Automatable per band: Freq, Gain, Q, Threshold, Range (Band N ...). Settings per band (session state, undoable):
// on, shape, placement, slope, dynamic, attack, release, external key. Global: phase mode, linear quality,
// auto gain, analyser mode, display range, Output.

#include "SqColour.h"
#include "SqSpectraDsp.h"

class SqSpectraEqPlugin : public SqColourBase, private juce::AsyncUpdater
{
public:
    static constexpr int numBands = SqSpectra::maxBands;
    enum PerBand { pFreq, pGain, pQ, pThresh, pRange, perBand };
    static constexpr int outputParam = numBands * perBand;

    SQ_COLOUR_IDENTITY (SqSpectraEqPlugin, "Spectra EQ", "sqmSpectraEq", "Spectra")

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s = [] {
            std::vector<SqDsp::ParamSpec> v;
            static std::vector<std::string> keep;                           // ParamSpec holds char pointers
            keep.reserve (numBands * perBand * 2 + 4);
            auto c = [] (const juce::String& t) { keep.push_back (t.toStdString()); return keep.back().c_str(); };
            for (int b = 0; b < numBands; ++b)
            {
                const auto n = juce::String (b + 1);
                v.push_back ({ c ("b" + n + "Freq"),   c ("Band " + n + " Freq"),       10.0f, 30000.0f, defaultFreq (b), 0.18f, " Hz", 0 });
                v.push_back ({ c ("b" + n + "Gain"),   c ("Band " + n + " Gain"),      -30.0f, 30.0f,     0.0f, 1.0f, " dB", 2 });
                v.push_back ({ c ("b" + n + "Q"),      c ("Band " + n + " Q"),          0.025f, 40.0f,    1.0f, 0.25f, "", 3 });
                v.push_back ({ c ("b" + n + "Thresh"), c ("Band " + n + " Threshold"), -80.0f, 0.0f,    -30.0f, 1.0f, " dB", 1 });
                v.push_back ({ c ("b" + n + "Range"),  c ("Band " + n + " Range"),     -30.0f, 30.0f,     0.0f, 1.0f, " dB", 1 });
            }
            v.push_back ({ "output", "Output", -36.0f, 36.0f, 0.0f, 1.0f, " dB", 2 });
            return v;
        }();
        return s;
    }
    static float defaultFreq (int b) { return (float) (40.0 * std::pow (2.0, b * 0.42)); }

    struct BandProps
    {
        juce::CachedValue<bool> on, dyn, extKey;
        juce::CachedValue<int> shape, place, slope;
        juce::CachedValue<float> attack, release;
    };
    std::array<BandProps, numBands> props;
    juce::CachedValue<int> phaseMode, quality, analyser, displayRange;
    juce::CachedValue<bool> autoGain;

    SqSpectraEqPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        for (int b = 0; b < numBands; ++b)
        {
            const auto n = juce::String (b + 1); auto& p = props[(size_t) b];
            p.on.referTo (state, "b" + n + "On", um, false); p.dyn.referTo (state, "b" + n + "Dyn", um, false); p.extKey.referTo (state, "b" + n + "Key", um, false);
            p.shape.referTo (state, "b" + n + "Shape", um, 0); p.place.referTo (state, "b" + n + "Place", um, 0); p.slope.referTo (state, "b" + n + "Slope", um, 12);
            p.attack.referTo (state, "b" + n + "Att", um, 10.0f); p.release.referTo (state, "b" + n + "Rel", um, 120.0f);
        }
        phaseMode.referTo (state, "phaseMode", um, 0); quality.referTo (state, "linQuality", um, 2);
        analyser.referTo (state, "analyser", um, 2); displayRange.referTo (state, "displayRange", um, 2); autoGain.referTo (state, "autoGain", um, false);
        engine.setAsyncFir (true);
        worker = std::make_unique<FirWorker> (*this);
    }
    ~SqSpectraEqPlugin() override { cancelPendingUpdate(); worker.reset(); notifyListenersOfDeletion(); bank.detach(); }

    void resetProps (juce::UndoManager* um) override
    {
        for (auto& p : props) { p.on.setValue (false, um); p.dyn.setValue (false, um); p.extKey.setValue (false, um); p.shape.setValue (0, um); p.place.setValue (0, um); p.slope.setValue (12, um); p.attack.setValue (10.0f, um); p.release.setValue (120.0f, um); }
        phaseMode.setValue (0, um); quality.setValue (2, um); autoGain.setValue (false, um);
    }

    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); } }
    double getLatencySeconds() override { return (double) latencyFor (phaseMode.get(), quality.get()) / juce::jmax (1.0, sr); }
    int latencyFor (int mode, int q) const
    {
        SqSpectra::Engine probe; probe.setPhase ((SqSpectra::Phase) juce::jlimit (0, 2, mode), juce::jlimit (0, 4, q));
        probe.prepareRateOnly (sr); return probe.latency();
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        read();
        engine.prepare (sr, juce::jmax (64, info.blockSizeSamples));
        if (engine.firNeeded()) engine.applyFirs (SqSpectra::Engine::computeFirs (engine.allBands(), sr, engine.firLength()));   // first build before audio
    }
    void deinitialise() override {}
    void reset() override { engine.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        read();
        // Linear filters built off the audio thread
        if (engine.firNeeded())
        {
            const juce::SpinLock::ScopedTryLockType l (firLock);
            if (l.isLocked() && readyFir != nullptr && readyGen == engine.bandsGeneration()) { engine.applyFirs (*readyFir); readyFir.reset(); }
            else if (l.isLocked() && requestedGen != engine.bandsGeneration()) { requestedGen = engine.bandsGeneration(); request = std::make_unique<Request> (Request { engine.allBands(), sr, engine.firLength(), requestedGen }); worker->notify(); }
        }
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = b.getNumChannels() > 2;
        if (haveKey) { key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0); }
        feed (pre, preW, ch, nch, n);
        if (haveKey) feed (keyScope, keyW, key, b.getNumChannels() > 3 ? 2 : 1, n);
        float pk = 0; for (int c = 0; c < nch; ++c) pk = juce::jmax (pk, b.getMagnitude (c, s0, n)); levelIn = pk;
        engine.process (ch, nch, n, haveKey ? key : nullptr, haveKey ? 2 : 0);
        feed (post, postW, ch, nch, n);
        float po = 0; for (int c = 0; c < nch; ++c) po = juce::jmax (po, b.getMagnitude (c, s0, n)); levelOut = po;
        for (int i = 0; i < numBands; ++i) dynDb[(size_t) i] = (float) engine.dynamicDb (i);
        appliedAutoGain = (float) autoGainDb;
    }

    // ---- what the face and Claude read
    SqSpectra::Band band (int i) const
    {
        SqSpectra::Band b; const auto& p = props[(size_t) i]; const int base = i * perBand;
        b.enabled = p.on.get(); b.shape = (SqSpectra::Shape) juce::jlimit (0, 9, p.shape.get()); b.place = (SqSpectra::Place) juce::jlimit (0, 4, p.place.get());
        b.freq = bank[(size_t) (base + pFreq)]; b.gainDb = bank[(size_t) (base + pGain)]; b.q = bank[(size_t) (base + pQ)];
        b.slope = p.slope.get(); b.dynamic = p.dyn.get() && b.canBeDynamic();
        b.threshDb = bank[(size_t) (base + pThresh)]; b.rangeDb = bank[(size_t) (base + pRange)];
        b.attackMs = p.attack.get(); b.releaseMs = p.release.get(); b.externalKey = p.extKey.get();
        return b;
    }
    /** The static curve (dB) at hz, every band plus output and auto gain - the face's curve and Claude's readout. */
    double curveDb (double hz, bool withDynamics = true) const
    {
        std::complex<double> h (1, 0);
        for (int i = 0; i < numBands; ++i)
        {
            const auto b = band (i); if (! b.enabled) continue;
            h *= SqSpectra::analogResponse (SqSpectra::prototype (b, b.gainDb + (withDynamics && b.dynamic ? dynDb[(size_t) i].load() : 0.0f)), hz);
        }
        return 20 * std::log10 (std::max (1e-12, std::abs (h))) + bank[(size_t) outputParam] + (autoGain.get() ? appliedAutoGain.load() : 0.0f);
    }
    double bandDb (int i, double hz) const
    {
        const auto b = band (i); if (! b.enabled) return 0;
        return 20 * std::log10 (std::max (1e-12, std::abs (SqSpectra::analogResponse (SqSpectra::prototype (b, b.gainDb + (b.dynamic ? dynDb[(size_t) i].load() : 0.0f)), hz))));
    }
    int firstFreeBand() const { for (int i = 0; i < numBands; ++i) if (! props[(size_t) i].on.get()) return i; return -1; }
    double rate() const { return sr; }
    /** A band near Nyquist the 1x mode can't follow exactly (the face suggests Natural). */
    bool suggestNatural() const
    {
        if (phaseMode.get() != 0) return false;
        for (int i = 0; i < numBands; ++i) { const auto b = band (i); if (b.enabled && (b.shape == SqSpectra::Shape::highCut && b.slope >= 36 && b.freq > sr * 0.3)) return true; }
        return false;
    }

    static constexpr int scopeLen = 16384;
    std::array<float, scopeLen> pre {}, post {}, keyScope {};
    std::atomic<int> preW { 0 }, postW { 0 }, keyW { 0 };
    std::array<std::atomic<float>, numBands> dynDb {};
    std::atomic<float> appliedAutoGain { 0 };
    SqSpectra::Engine engine;

    static juce::StringArray qualityNames() { return { "Low (2k)", "Medium (4k)", "High (8k)", "Very High (16k)", "Maximum (32k)" }; }
    static juce::StringArray analyserNames() { return { "Off", "Post", "Pre + Post", "Pre + Post + Key" }; }
    static juce::StringArray rangeNames() { return { "3 dB", "6 dB", "12 dB", "30 dB" }; }
    static float rangeDb (int i) { static const float r[4] = { 3, 6, 12, 30 }; return r[juce::jlimit (0, 3, i)]; }

private:
    struct Request { std::array<SqSpectra::Band, numBands> bands; double fs; int length; uint64_t gen; };
    struct FirWorker : juce::Thread
    {
        explicit FirWorker (SqSpectraEqPlugin& o) : juce::Thread ("Spectra EQ linear phase"), owner (o) { startThread (juce::Thread::Priority::low); }
        ~FirWorker() override { signalThreadShouldExit(); notify(); stopThread (4000); }
        void run() override
        {
            while (! threadShouldExit())
            {
                wait (-1);
                if (threadShouldExit()) return;
                std::unique_ptr<Request> r;
                { const juce::SpinLock::ScopedLockType l (owner.firLock); r = std::move (owner.request); }
                if (r == nullptr) continue;
                auto set = std::make_unique<SqSpectra::Engine::FirSet> (SqSpectra::Engine::computeFirs (r->bands, r->fs, r->length));
                const juce::SpinLock::ScopedLockType l (owner.firLock);
                owner.readyFir = std::move (set); owner.readyGen = r->gen;
            }
        }
        SqSpectraEqPlugin& owner;
    };
    juce::SpinLock firLock;
    std::unique_ptr<Request> request;
    std::unique_ptr<SqSpectra::Engine::FirSet> readyFir;
    uint64_t readyGen = 0, requestedGen = ~0ull;
    std::unique_ptr<FirWorker> worker;
    double autoGainDb = 0; uint64_t autoGainGen = ~0ull;

    void read()
    {
        engine.setPhase ((SqSpectra::Phase) juce::jlimit (0, 2, phaseMode.get()), juce::jlimit (0, 4, quality.get()));
        for (int i = 0; i < numBands; ++i) engine.setBand (i, band (i));
        // auto gain: the static curve's mean over 40 log-spaced points, 30 Hz - 16 kHz (pink weighting), cancelled
        if (autoGain.get() && autoGainGen != engine.bandsGeneration())
        {
            autoGainGen = engine.bandsGeneration();
            double sum = 0; int n = 0;
            for (int k = 0; k < 40; ++k)
            {
                const double hz = 30.0 * std::pow (16000.0 / 30.0, k / 39.0);
                std::complex<double> h (1, 0);
                for (int i = 0; i < numBands; ++i) { const auto& b = engine.band (i); if (b.enabled && b.shape != SqSpectra::Shape::lowCut && b.shape != SqSpectra::Shape::highCut) h *= SqSpectra::analogResponse (SqSpectra::prototype (b, b.gainDb), hz); }
                sum += 20 * std::log10 (std::max (1e-9, std::abs (h))); ++n;
            }
            autoGainDb = -sum / n;
        }
        engine.setOutput (bank[(size_t) outputParam] + (autoGain.get() ? autoGainDb : 0.0));
    }
    static void feed (std::array<float, scopeLen>& dst, std::atomic<int>& w, const float* const* ch, int nch, int n)
    {
        int wi = w.load();
        for (int i = 0; i < n; ++i) { float v = ch[0][i]; if (nch > 1) v = 0.5f * (v + ch[1][i]); dst[(size_t) (wi & (scopeLen - 1))] = v; ++wi; }
        w = wi;
    }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state && (id == juce::Identifier ("phaseMode") || id == juce::Identifier ("linQuality"))) triggerAsyncUpdate();   // the latency changed
    }
};
