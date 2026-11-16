#pragma once
// The Apex Series - Squelchmoth's own versions of the Pro Tools Pro Series, built further:
//
//  Apex Dynamics  - compressor, expander, gate, ducker and upward compressor in ONE plug-in: Smart / RMS / Average /
//                   Peak / Fast detection, ratios to LMTR and negative ratios, soft knee, depth, hold, hysteresis,
//                   0-15 ms lookahead, dry mix, auto make-up, attenuation listen,
//                   external key with HP / LP / BP / notch filter and key listen, stereo link.
//  Apex Limiter   - 4x true-peak brickwall with lookahead, Character saturation, auto release, link, and a full
//                   BS.1770-4 loudness meter (momentary, short-term, integrated, LRA, true-peak max, 60 s history)
//                   with loudness targets (Streaming, Apple, Podcast, EBU R128, ATSC A/85, Club) and one-click Match.
//  Apex Multiband - up to 6 bands, minimum-phase Linkwitz-Riley or linear-phase crossovers, per-band threshold,
//                   depth, gain, slope, attack, release, flip (below threshold), solo, bypass and auto gain.
//  Apex Sub       - pitch-tracked sub-octave and fifth bands from a chosen source range or MIDI, six drive types,
//                   high/low-pass filters and mix.

#include "SqApexDsp.h"
#include "SqInstruments.h"

//==============================================================================
/** Shared bits for the Apex plug-ins. */
class SqApexBase : public te::Plugin, public SqDsp::BankOwner
{
public:
    explicit SqApexBase (te::PluginCreationInfo info) : te::Plugin (info) {}
    SqDsp::ParamBank bank;
    SqDsp::ParamBank& getBank() override   { return bank; }
    bool needsConstantBufferSize() override             { return false; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        for (auto ap : getAutomatableParameters()) ap->updateFromAttachedValue();
    }
    // meters for the editors
    std::atomic<float> inDb { -100 }, outDb { -100 }, grDb { 0 };
    static constexpr int historySize = 512;
    std::array<float, historySize> grHistory {}, inHistory {};
    std::atomic<int> historyWrite { 0 };
protected:
    void pushHistory (float gr, float in)
    {
        const int w = historyWrite.load();
        grHistory[(size_t) (w & (historySize - 1))] = gr; inHistory[(size_t) (w & (historySize - 1))] = in;
        historyWrite = w + 1;
    }
    double sr = 48000;
};

//==============================================================================
class SqApexDynamicsPlugin : public SqApexBase
{
public:
    enum P { threshold, ratio, knee, attack, release, hold, hysteresis, depth, lookahead, dryMix, makeup, scFreq, scQ, numParams };

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "threshold",  "Threshold",  -60.0f, 0.0f, -18.0f, 1.0f, " dB", 1 },
            { "ratio",      "Ratio",      1.0f, 41.0f, 4.0f, 0.45f, "", 1 },
            { "knee",       "Knee",       0.0f, 30.0f, 6.0f, 1.0f, " dB", 1 },
            { "attack",     "Attack",     0.01f, 300.0f, 10.0f, 0.3f, " ms", 2 },
            { "release",    "Release",    5.0f, 4000.0f, 120.0f, 0.3f, " ms", 0 },
            { "hold",       "Hold",       0.0f, 500.0f, 0.0f, 0.5f, " ms", 0 },
            { "hysteresis", "Hysteresis", 0.0f, 20.0f, 3.0f, 1.0f, " dB", 1 },
            { "depth",      "Depth",      0.0f, 80.0f, 40.0f, 1.0f, " dB", 1 },
            { "lookahead",  "Lookahead",  0.0f, 15.0f, 0.0f, 1.0f, " ms", 2 },
            { "dryMix",     "Dry Mix",    0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "makeup",     "Makeup",     -12.0f, 36.0f, 0.0f, 1.0f, " dB", 1 },
            { "scFreq",     "Key Filter Freq", 20.0f, 20000.0f, 100.0f, 0.25f, " Hz", 0 },
            { "scQ",        "Key Filter Q", 0.2f, 10.0f, 0.7f, 0.5f, "", 2 } };
        return s;
    }
    SqApexDynamicsPlugin (te::PluginCreationInfo info) : SqApexBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        mode.referTo (state, "mode", um, 0);
        detection.referTo (state, "detection", um, 0);
        scFilter.referTo (state, "scFilter", um, 0);
        autoMakeup.referTo (state, "autoMakeup", um, false);
        attenListen.referTo (state, "attenListen", um, false);
        keyListen.referTo (state, "keyListen", um, false);
        externalKey.referTo (state, "externalKey", um, false);
        link.referTo (state, "link", um, true);
    }
    ~SqApexDynamicsPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    static const char* getPluginName()                  { return NEEDS_TRANS("Apex Dynamics"); }
    static constexpr const char* xmlTypeName = "sqmApexDyn";
    juce::String getName() const override               { return "Apex Dynamics"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "ApexDyn"; }
    juce::String getSelectableDescription() override    { return "Apex Dynamics"; }
    double getLatencySeconds() override                 { return bank[lookahead] * 0.001; }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override
    {
        te::Plugin::getChannelNames (ins, outs);
        if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); }
    }
    static juce::StringArray modeNames()      { return { "COMPRESS", "EXPAND", "GATE", "DUCK", "UPWARD" }; }
    static juce::StringArray detectionNames() { return { "SMART", "RMS", "AVERAGE", "PEAK", "FAST" }; }
    static juce::StringArray filterNames()    { return { "OFF", "HIGH-PASS", "LOW-PASS", "BAND-PASS", "NOTCH" }; }

    juce::CachedValue<int> mode, detection, scFilter;
    juce::CachedValue<bool> autoMakeup, attenListen, keyListen, externalKey, link;
    std::atomic<float> detDb { -100 };

    void resetProps (juce::UndoManager* um) override
    {
        mode.setValue (0, um); detection.setValue (0, um); scFilter.setValue (0, um); autoMakeup.setValue (false, um);
        attenListen.setValue (false, um); keyListen.setValue (false, um); externalKey.setValue (false, um); link.setValue (true, um);
    }
    /** Auto make-up: half the reduction a full-scale signal would get (dB). */
    float autoMakeupDb() const
    {
        const float g = Apex::Curve::gainDb (mode.get(), 0.0f, bank[threshold], bank[ratio], bank[knee], bank[depth]);
        return mode.get() == 0 ? -g * 0.5f : 0.0f;
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto& d : det) d.prepare (sr);
        for (auto& b : bal) b = {};
        for (auto& f : keyF) f.reset();
        maxLook = (int) std::ceil (0.015 * sr) + 4;
        for (auto& d : delay) { d.assign ((size_t) maxLook * 2 + 8, 0.0f); }
        dw = 0; gateOpen[0] = gateOpen[1] = false;
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
        const bool ext = externalKey.get() && b.getNumChannels() > 2;
        const int md = mode.get(), dt = detection.get(), ft = scFilter.get();
        const float thr = bank[threshold], rat = bank[ratio], kn = bank[knee], dep = bank[depth], hyst = bank[hysteresis];
        const float mk = dbToGainSafe (bank[makeup] + (autoMakeup.get() ? autoMakeupDb() : 0.0f));
        const float dry = bank[dryMix] / 100.0f;
        const int look = juce::jlimit (0, maxLook, (int) std::llround (bank[lookahead] * 0.001 * sr));
        for (auto& bb : bal) bb.set (sr, bank[attack], bank[release], bank[hold]);
        for (auto& f : keyF) f.set (sr, bank[scFreq], bank[scQ]);
        const bool linked = link.get();
        float peakIn = 0, peakOut = 0, minGr = 0, lastDet = -100;
        for (int i = 0; i < n; ++i)
        {
            float x[2], k[2];
            for (int c = 0; c < 2; ++c) x[c] = b.getSample (juce::jmin (c, nch - 1), s0 + i);
            for (int c = 0; c < 2; ++c) k[c] = ext ? b.getSample (juce::jmin (2 + c, b.getNumChannels() - 1), s0 + i) : x[c];
            // key filter
            for (int c = 0; c < 2; ++c)
            {
                switch (ft)
                {
                    case 1: k[c] = (float) keyF[c].highpass (k[c]); break;
                    case 2: k[c] = (float) keyF[c].lowpass (k[c]); break;
                    case 3: k[c] = (float) keyF[c].bandpass (k[c]); break;
                    case 4: k[c] = (float) keyF[c].notch (k[c]); break;
                    default: break;
                }
            }
            float gr[2];
            if (linked)
            {
                const float lv = det[0].process (std::abs (k[0]) > std::abs (k[1]) ? k[0] : k[1], dt);
                lastDet = lv;
                gr[0] = gr[1] = bal[0].process (target (md, lv, thr, rat, kn, dep, hyst, 0));
            }
            else
                for (int c = 0; c < 2; ++c) { const float lv = det[c].process (k[c], dt); lastDet = juce::jmax (lastDet, lv); gr[c] = bal[c].process (target (md, lv, thr, rat, kn, dep, hyst, c)); }
            // delay the audio by the lookahead
            for (int c = 0; c < 2; ++c) delay[c][(size_t) dw] = x[c];
            const int rd = (dw - look + (int) delay[0].size()) % (int) delay[0].size();
            dw = (dw + 1) % (int) delay[0].size();
            for (int c = 0; c < nch; ++c)
            {
                const float in = delay[c][(size_t) rd];
                const float g = Apex::dbToGain (gr[c]);
                float y;
                if (keyListen.get()) y = k[c];
                else if (attenListen.get()) y = in * (1.0f - g);            // what is being taken away
                else y = in * (g * mk * (1.0f - dry) + dry);
                b.setSample (c, s0 + i, y);
                peakIn = juce::jmax (peakIn, std::abs (in)); peakOut = juce::jmax (peakOut, std::abs (y));
            }
            minGr = std::abs (gr[0]) > std::abs (minGr) ? gr[0] : minGr;
        }
        inDb = Apex::gainToDb (peakIn); outDb = Apex::gainToDb (peakOut); grDb = minGr; detDb = lastDet;
        if ((++histDecim & 1) == 0) pushHistory (minGr, Apex::gainToDb (peakIn));
    }

private:
    Apex::Detector det[2]; Apex::Ballistics bal[2]; Apex::Svf keyF[2];
    std::vector<float> delay[2]; int dw = 0, maxLook = 1000;
    bool gateOpen[2] { false, false };
    int histDecim = 0;
    static float dbToGainSafe (float db) { return Apex::dbToGain (juce::jlimit (-60.0f, 48.0f, db)); }
    float target (int md, float lv, float thr, float rat, float kn, float dep, float hyst, int c)
    {
        if (md == 2)   // gate with hysteresis: opens above threshold, closes below threshold - hysteresis
        {
            if (lv > thr) gateOpen[c] = true; else if (lv < thr - hyst) gateOpen[c] = false;
            return gateOpen[c] ? 0.0f : -dep;
        }
        return Apex::Curve::gainDb (md, lv, thr, rat, kn, dep);
    }
};

//==============================================================================
class SqApexLimiterPlugin : public SqApexBase
{
public:
    enum P { input, ceiling, release, character, numParams };
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "input",     "Input",     0.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
            { "ceiling",   "Ceiling",   -30.0f, 0.0f, -1.0f, 2.0f, " dBTP", 1 },
            { "release",   "Release",   1.0f, 1000.0f, 80.0f, 0.35f, " ms", 0 },
            { "character", "Character", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 } };
        return s;
    }
    struct Target { const char* name; float lufs, tp; };
    static const std::vector<Target>& targets()
    {
        static const std::vector<Target> t { { "Streaming (-14 LUFS)", -14.0f, -1.0f }, { "Apple Music (-16 LUFS)", -16.0f, -1.0f }, { "Podcast (-16 LUFS)", -16.0f, -1.0f },
                                             { "EBU R128 broadcast (-23 LUFS)", -23.0f, -1.0f }, { "ATSC A/85 (-24 LKFS)", -24.0f, -2.0f }, { "Club / DJ (-8 LUFS)", -8.0f, -0.3f },
                                             { "CD loud (-9 LUFS)", -9.0f, -0.1f } };
        return t;
    }
    SqApexLimiterPlugin (te::PluginCreationInfo info) : SqApexBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        autoRelease.referTo (state, "autoRelease", um, true);
        link.referTo (state, "link", um, true);
        target.referTo (state, "target", um, 0);
    }
    ~SqApexLimiterPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    static const char* getPluginName()                  { return NEEDS_TRANS("Apex Limiter"); }
    static constexpr const char* xmlTypeName = "sqmApexLim";
    juce::String getName() const override               { return "Apex Limiter"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "ApexLim"; }
    juce::String getSelectableDescription() override    { return "Apex Limiter"; }
    double getLatencySeconds() override                 { return (double) latencyFor (sampleRate) / juce::jmax (1.0, sampleRate); }
    static int latencyFor (double s)                    { return (int) std::ceil (0.0015 * s) + Apex::TruePeak::delay(); }

    juce::CachedValue<bool> autoRelease, link;
    juce::CachedValue<int> target;
    std::atomic<float> momentary { -100 }, shortTerm { -100 }, integrated { -100 }, lra { 0 }, tpMax { -100 }, tpNow { -100 };
    std::atomic<bool> resetRequest { false };
    static constexpr int loudHistory = 600;                 // 60 s at 10 Hz
    std::array<float, loudHistory> stHistory {}, tpHistory {};
    std::atomic<int> loudWrite { 0 };

    void resetProps (juce::UndoManager* um) override { autoRelease.setValue (true, um); link.setValue (true, um); target.setValue (0, um); }
    /** Sets Input so the measured integrated loudness lands on the chosen target, and the ceiling to its true-peak limit. */
    juce::String matchTarget()
    {
        const auto& t = targets()[(size_t) juce::jlimit (0, (int) targets().size() - 1, target.get())];
        const float integ = integrated.load();
        bank.params[(size_t) ceiling]->setParameter (t.tp, juce::sendNotification);
        if (integ < -69.0f) return "Play the song (at least 3 s) so Apex can measure it, then Match again";
        const float newIn = juce::jlimit (0.0f, 24.0f, bank[input] + (t.lufs - integ));
        bank.params[(size_t) input]->setParameter (newIn, juce::sendNotification);
        resetRequest = true;
        return "Input " + juce::String (newIn, 1) + " dB, ceiling " + juce::String (t.tp, 1) + " dBTP for " + t.name + " (was " + juce::String (integ, 1) + " LUFS)";
    }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        L = (int) std::ceil (0.0015 * sr);
        const int tpd = Apex::TruePeak::delay();
        totalDelay = L + tpd;
        for (int c = 0; c < 2; ++c) { tp[c].reset(); audio[c].assign ((size_t) (totalDelay + 4) * 2 + 64, 0.0f); }
        ring = (int) audio[0].size();
        reqRing.assign ((size_t) L + 2, 1.0f); minq.clear(); minq.reserve ((size_t) L + 2200); qHead = 0; tIndex = 0;
        maRing.assign ((size_t) L + 2, 1.0f); maSum = (double) (L) ; maW = 0; reqW = 0;
        aw = 0; gain = 1.0f; envSlow = 0;
        loud.prepare (sr); loudOut.prepare (sr); loudDecim = 0; tpMaxLin = 0;
        rq.assign ((size_t) L + 1, 1.0f); rqHead = 0; rqCount = 0;
    }
    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        if (resetRequest.exchange (false)) { loudOut.reset(); tpMaxLin = 0; }
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        const float inG = Apex::dbToGain (bank[input]), ceil = Apex::dbToGain (bank[ceiling]) * 0.999f;
        const float chr = bank[character] / 100.0f;
        const bool autoRel = autoRelease.get(), linked = link.get();
        const float relCoef = (float) std::exp (-1.0 / (juce::jmax (1.0f, bank[release]) * 0.001 * sr));
        float peakIn = 0, peakOut = 0, minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float x[2];
            for (int c = 0; c < 2; ++c)
            {
                float v = b.getSample (juce::jmin (c, nch - 1), s0 + i) * inG;
                if (chr > 0.0f)   // Character: gentle tube-style saturation ahead of the limiter
                {
                    const float d = 1.0f + chr * 2.0f;
                    v = v * (1.0f - chr) + chr * std::tanh (v * d) / std::tanh (d);
                }
                x[c] = v;
            }
            // true-peak detection on the (not yet delayed) signal
            float p[2] = { tp[0].process (x[0]), tp[1].process (x[1]) };
            peakIn = juce::jmax (peakIn, p[0], p[1]);
            const float pk = linked ? juce::jmax (p[0], p[1]) : juce::jmax (p[0], p[1]);
            const float req = pk > ceil ? ceil / pk : 1.0f;
            // sliding minimum over the lookahead window, then a moving average across it (reaches req by the peak)
            const float mn = slidingMin (req);
            maSum += mn - maRing[(size_t) maW]; maRing[(size_t) maW] = mn; maW = (maW + 1) % (L + 1);
            const float smooth = (float) (maSum / (L + 1));
            const float att = smooth;   // the average of window minima is always at or below what the peak needs
            // release: auto = program dependent (fast after short peaks, slow while the limiter works continuously)
            float rel = relCoef;
            if (autoRel)
            {
                envSlow = envSlow * 0.9995f + (1.0f - att) * 0.0005f;
                const float relMs = juce::jlimit (15.0f, 800.0f, 30.0f + envSlow * 6000.0f);
                rel = (float) std::exp (-1.0 / (relMs * 0.001 * sr));
            }
            gain = att < gain ? att : juce::jmin (att, 1.0f - (1.0f - gain) * rel);
            minG = juce::jmin (minG, gain);
            // delayed audio out
            for (int c = 0; c < 2; ++c) audio[c][(size_t) aw] = x[c];
            const int rd = (aw - totalDelay + ring) % ring;
            aw = (aw + 1) % ring;
            float y[2];
            for (int c = 0; c < 2; ++c) y[c] = juce::jlimit (-ceil, ceil, audio[c][(size_t) rd] * gain);   // safety clip never engages in normal use
            for (int c = 0; c < nch; ++c) b.setSample (c, s0 + i, y[c]);
            peakOut = juce::jmax (peakOut, std::abs (y[0]), std::abs (y[1]));
            loudOut.process (y[0], y[1]);
            tpMaxLin = juce::jmax (tpMaxLin, tpOut[0].process (y[0]), tpOut[1].process (y[1]));
        }
        inDb = Apex::gainToDb (peakIn); outDb = Apex::gainToDb (peakOut); grDb = Apex::gainToDb (minG);
        momentary = loudOut.getMomentary(); shortTerm = loudOut.getShortTerm(); integrated = loudOut.getIntegrated(); lra = loudOut.getRange();
        tpMax = Apex::gainToDb (tpMaxLin); tpNow = Apex::gainToDb (peakOut);
        if ((++histDecim & 1) == 0) pushHistory (Apex::gainToDb (minG), Apex::gainToDb (peakIn));
        loudDecim += n;
        if (loudDecim >= (int) (sr * 0.1))
        {
            loudDecim = 0;
            const int w = loudWrite.load();
            stHistory[(size_t) (w % loudHistory)] = shortTerm.load(); tpHistory[(size_t) (w % loudHistory)] = tpNow.load();
            loudWrite = w + 1;
        }
    }

private:
    int L = 72, totalDelay = 80, ring = 256, aw = 0, maW = 0, reqW = 0, loudDecim = 0, histDecim = 0;
    Apex::TruePeak tp[2], tpOut[2];
    std::vector<float> audio[2], reqRing, maRing;
    double maSum = 0;
    float gain = 1, envSlow = 0, tpMaxLin = 0;
    Apex::Loudness loud, loudOut;
    // monotonic deque for the sliding minimum (fixed storage)
    std::vector<std::pair<int64_t, float>> minq;
    std::vector<float> rq; int rqHead = 0, rqCount = 0; int64_t tIndex = 0; size_t qHead = 0;
    float slidingMin (float v)
    {
        // classic ascending deque in a vector with a moving head (compacted when it grows)
        while (minq.size() > qHead && minq.back().second >= v) minq.pop_back();
        minq.push_back ({ tIndex, v });
        while (minq[qHead].first <= tIndex - (L + 1)) ++qHead;
        if (qHead > 1024) { minq.erase (minq.begin(), minq.begin() + (long) qHead); qHead = 0; }
        ++tIndex;
        return minq[qHead].second;
    }
};

//==============================================================================
class SqApexMultibandPlugin : public SqApexBase, private juce::AsyncUpdater
{
public:
    static constexpr int maxBands = 6;
    enum P { inputGain, outputGain, mixP, x1, x2, x3, x4, x5, bandBase };   // then per band: thr, depth, gain, slope, attack, release
    static constexpr int perBand = 6;

    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> s = []
        {
            std::vector<SqDsp::ParamSpec> v {
                { "inputGain",  "Input",  -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "outputGain", "Output", -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
                { "mix",        "Mix",    0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
                { "x1", "Crossover 1", 20.0f, 20000.0f, 120.0f, 0.25f, " Hz", 0 },
                { "x2", "Crossover 2", 20.0f, 20000.0f, 500.0f, 0.25f, " Hz", 0 },
                { "x3", "Crossover 3", 20.0f, 20000.0f, 2000.0f, 0.25f, " Hz", 0 },
                { "x4", "Crossover 4", 20.0f, 20000.0f, 6000.0f, 0.25f, " Hz", 0 },
                { "x5", "Crossover 5", 20.0f, 20000.0f, 12000.0f, 0.25f, " Hz", 0 } };
            auto addS = [] (const std::string& t) { names.push_back (t); return names.back().c_str(); };
            for (int b = 1; b <= maxBands; ++b)
            {
                const auto n = std::to_string (b);
                v.push_back ({ addS ("b" + n + "thr"),   addS ("Band " + n + " Threshold"), -60.0f, 0.0f, -20.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("b" + n + "depth"), addS ("Band " + n + " Depth"),     -24.0f, 24.0f, -6.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("b" + n + "gain"),  addS ("Band " + n + " Gain"),      -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 });
                v.push_back ({ addS ("b" + n + "slope"), addS ("Band " + n + " Slope"),     0.0f, 100.0f, 40.0f, 1.0f, " %", 0 });
                v.push_back ({ addS ("b" + n + "att"),   addS ("Band " + n + " Attack"),    0.1f, 1000.0f, 10.0f, 0.3f, " ms", 1 });
                v.push_back ({ addS ("b" + n + "rel"),   addS ("Band " + n + " Release"),   1.0f, 10000.0f, 150.0f, 0.3f, " ms", 0 });
            }
            return v;
        }();
        return s;
    }
    SqApexMultibandPlugin (te::PluginCreationInfo info) : SqApexBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        numBands.referTo (state, "bands", um, 4);
        linearPhase.referTo (state, "linearPhase", um, false);
        detection.referTo (state, "detection", um, 0);
        for (int b = 0; b < maxBands; ++b)
        {
            const auto n = juce::String (b + 1);
            solo[(size_t) b].referTo (state, juce::Identifier ("solo" + n), um, false);
            bypass[(size_t) b].referTo (state, juce::Identifier ("bypass" + n), um, false);
            flip[(size_t) b].referTo (state, juce::Identifier ("flip" + n), um, false);
            autoGain[(size_t) b].referTo (state, juce::Identifier ("auto" + n), um, false);
        }
    }
    ~SqApexMultibandPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }
    static const char* getPluginName()                  { return NEEDS_TRANS("Apex Multiband"); }
    static constexpr const char* xmlTypeName = "sqmApexMB";
    juce::String getName() const override               { return "Apex Multiband"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "ApexMB"; }
    juce::String getSelectableDescription() override    { return "Apex Multiband"; }
    double getLatencySeconds() override                 { return linearPhase.get() ? (double) linearLatency (sampleRate) / juce::jmax (1.0, sampleRate) : 0.0; }
    static int firLength (double s)   { return s > 60000 ? 8191 : 4095; }
    static int linearLatency (double s) { return firBlock + (firLength (s) - 1) / 2; }
    static constexpr int firBlock = 256;

    juce::CachedValue<int> numBands, detection;
    juce::CachedValue<bool> linearPhase;
    std::array<juce::CachedValue<bool>, maxBands> solo, bypass, flip, autoGain;
    std::array<std::atomic<float>, maxBands> bandGr {}, bandLevel {};
    // spectrum feed for the editor
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;
    std::array<float, fftSize> scope {}; std::atomic<int> scopeWrite { 0 };
    int firLen = 4095;

    void resetProps (juce::UndoManager* um) override
    {
        numBands.setValue (4, um); linearPhase.setValue (false, um); detection.setValue (0, um);
        for (int b = 0; b < maxBands; ++b) { solo[(size_t) b].setValue (false, um); bypass[(size_t) b].setValue (false, um); flip[(size_t) b].setValue (false, um); autoGain[(size_t) b].setValue (false, um); }
    }
    float crossover (int i) const   { return bank[(size_t) (x1 + i)]; }
    float bp (int band, int k) const { return bank[(size_t) (bandBase + band * perBand + k)]; }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        firLen = firLength (sr);
        for (auto& ch : xo) for (auto& f : ch) f.reset();
        for (auto& ch : aps) for (auto& row : ch) for (auto& f : row) f.reset();
        for (auto& d : det) d.prepare (sr);
        lastX.fill (-1.0f);
        linActive = linearPhase.get();
        if (linActive) buildFir();
        const int D = linearLatency (sr);
        dryDelay[0].assign ((size_t) D * 2 + 8, 0.0f); dryDelay[1].assign ((size_t) D * 2 + 8, 0.0f); dd = 0;
        inDelay[0].assign ((size_t) D * 2 + 8, 0.0f); inDelay[1].assign ((size_t) D * 2 + 8, 0.0f);
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
        const int nb = juce::jlimit (1, maxBands, numBands.get());
        // crossovers (sorted, kept apart by a third of an octave)
        float xf[maxBands - 1];
        for (int i = 0; i < nb - 1; ++i) { xf[i] = crossover (i); if (i > 0) xf[i] = juce::jmax (xf[i], xf[i - 1] * 1.26f); xf[i] = juce::jmin (xf[i], (float) (sr * 0.45)); }
        if (! linActive)
            for (int i = 0; i < nb - 1; ++i)
                if (std::abs (xf[i] - lastX[(size_t) i]) > 0.01f)
                {
                    lastX[(size_t) i] = xf[i];
                    for (int c = 0; c < 2; ++c) { xo[c][(size_t) i].set (sr, xf[i]); for (int k = 0; k < maxBands; ++k) aps[c][(size_t) k][(size_t) i].set (sr, xf[i]); }
                }
        bool anySolo = false; for (int i = 0; i < nb; ++i) anySolo = anySolo || solo[(size_t) i].get();
        float bandGain[maxBands], thr[maxBands], dep[maxBands], slope[maxBands];
        for (int k = 0; k < nb; ++k)
        {
            bal[(size_t) k].set (sr, bp (k, 4), bp (k, 5), 0);
            thr[k] = bp (k, 0); dep[k] = bp (k, 1); slope[k] = bp (k, 3) / 100.0f;
            float g = bp (k, 2);
            if (autoGain[(size_t) k].get() && ! flip[(size_t) k].get() && dep[k] < 0) g += -dep[k] * 0.4f;
            bandGain[k] = Apex::dbToGain (g);
        }
        const float inG = Apex::dbToGain (bank[inputGain]), outG = Apex::dbToGain (bank[outputGain]), mixW = bank[mixP] / 100.0f;
        const int dtype = detection.get();
        float peakGr[maxBands] {}, peakLv[maxBands] {};
        for (int k = 0; k < maxBands; ++k) peakLv[k] = -100.0f;
        int sw = scopeWrite.load();
        for (int i = 0; i < n; ++i)
        {
            double band[2][maxBands];
            float dry[2];
            for (int c = 0; c < 2; ++c)
            {
                const float x = b.getSample (juce::jmin (c, nch - 1), s0 + i) * inG;
                dry[c] = x;
                if (linActive) firSplit (c, x, band[c], nb);
                else
                {
                    double rest = x;
                    for (int k = 0; k < nb - 1; ++k)
                    {
                        double lo, hi; xo[c][(size_t) k].split (rest, lo, hi);
                        // keep this band in phase with the splits above it
                        for (int j = k + 1; j < nb - 1; ++j) lo = aps[c][(size_t) k][(size_t) j].allpass (lo);
                        band[c][k] = lo; rest = hi;
                    }
                    band[c][nb - 1] = rest;
                }
            }
            // per band dynamics (linked stereo)
            float out[2] = { 0, 0 };
            for (int k = 0; k < nb; ++k)
            {
                const double m = std::abs (band[0][k]) > std::abs (band[1][k]) ? band[0][k] : band[1][k];
                const float lv = det[(size_t) k].process ((float) m, dtype);
                peakLv[k] = juce::jmax (peakLv[k], lv);
                float g = 0;
                if (! bypass[(size_t) k].get())
                {
                    const float ratio = 1.5f + slope[k] * slope[k] * 18.5f, knee = 12.0f * (1.0f - slope[k]);
                    const float over = flip[(size_t) k].get() ? thr[k] - lv : lv - thr[k];
                    float amount;
                    if (over <= -knee * 0.5f) amount = 0;
                    else if (over < knee * 0.5f) { const float d = over + knee * 0.5f; amount = d * d / (2.0f * juce::jmax (0.01f, knee)); }
                    else amount = over;
                    amount *= (1.0f - 1.0f / ratio);
                    g = dep[k] < 0 ? -juce::jmin (-dep[k], amount) : juce::jmin (dep[k], amount);
                }
                const float gs = bal[(size_t) k].process (g);
                peakGr[k] = std::abs (gs) > std::abs (peakGr[k]) ? gs : peakGr[k];
                const float gl = Apex::dbToGain (gs) * (bypass[(size_t) k].get() ? 1.0f : bandGain[k]);
                const bool audible = ! anySolo || solo[(size_t) k].get();
                if (audible) { out[0] += (float) band[0][k] * gl; out[1] += (float) band[1][k] * gl; }
            }
            // dry path (latency matched in linear-phase mode)
            float dl[2] = { dry[0], dry[1] };
            if (linActive)
            {
                const int D = linearLatency (sr), sz = (int) dryDelay[0].size();
                for (int c = 0; c < 2; ++c) dl[c] = dryDelay[c][(size_t) ((dd - D + sz) % sz)];
                dd = (dd + 1) % sz;
            }
            for (int c = 0; c < nch; ++c) b.setSample (c, s0 + i, (dl[c] * (1.0f - mixW) + out[c] * mixW) * outG);
            scope[(size_t) (sw & (fftSize - 1))] = 0.5f * (out[0] + out[1]) * outG; ++sw;
        }
        scopeWrite = sw;
        for (int k = 0; k < maxBands; ++k) { bandGr[(size_t) k] = k < nb ? peakGr[k] : 0.0f; bandLevel[(size_t) k] = k < nb ? peakLv[k] : -100.0f; }
    }

    /** Linear-phase crossovers: windowed-sinc low-passes, bands = differences (sum is a pure delay). */
    void buildFir()
    {
        const int nb = juce::jlimit (1, maxBands, numBands.get());
        fir.assign ((size_t) maxBands - 1, std::vector<float> ((size_t) firLen, 0.0f));
        for (int k = 0; k < nb - 1; ++k)
        {
            const double fc = juce::jlimit (20.0, sr * 0.45, (double) crossover (k)) / sr;
            for (int i = 0; i < firLen; ++i)
            {
                const double t = i - (firLen - 1) * 0.5;
                const double s = std::abs (t) < 1e-9 ? 2 * fc : std::sin (juce::MathConstants<double>::twoPi * fc * t) / (juce::MathConstants<double>::pi * t);
                const double w = 0.42 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (firLen - 1)) + 0.08 * std::cos (2 * juce::MathConstants<double>::twoPi * i / (firLen - 1));
                fir[(size_t) k][(size_t) i] = (float) (s * w);
            }
            double sum = 0; for (auto v : fir[(size_t) k]) sum += v;
            for (auto& v : fir[(size_t) k]) v = (float) (v / sum);
        }
        builtBands = nb;
        std::vector<std::vector<float>> irs ((size_t) juce::jmax (0, nb - 1));
        for (int k = 0; k < nb - 1; ++k) irs[(size_t) k] = fir[(size_t) k];
        for (auto& m : multiFir) m.prepare (firBlock, irs);
    }
    bool rebuildIfNeeded() { if (! linActive) return false; buildFir(); return true; }

private:
    std::array<std::array<Apex::LR4, maxBands - 1>, 2> xo;
    std::array<std::array<std::array<Apex::LR4, maxBands - 1>, maxBands>, 2> aps;
    std::array<Apex::Detector, maxBands> det; std::array<Apex::Ballistics, maxBands> bal;
    std::array<float, maxBands - 1> lastX {};
    bool linActive = false; int builtBands = 0;
    std::vector<std::vector<float>> fir;
    std::vector<float> dryDelay[2], inDelay[2]; int dd = 0;
    Apex::MultiFir multiFir[2];
    void firSplit (int c, float x, double* band, int nb)
    {
        // FFT-partitioned linear-phase low-passes; bands are their differences, the top band is the delayed input
        // minus the highest low-pass, so all bands sum back to a pure delay
        const int D = linearLatency (sr), sz = (int) inDelay[c].size();
        inDelay[c][(size_t) dd] = x; dryDelay[c][(size_t) dd] = x;
        const float delayed = inDelay[c][(size_t) ((dd - D + sz) % sz)];
        float lows[maxBands] {};
        if (nb > 1) multiFir[c].process (x, lows);
        double prevLow = 0;
        for (int k = 0; k < nb - 1; ++k) { band[k] = lows[k] - prevLow; prevLow = lows[k]; }
        band[nb - 1] = delayed - prevLow;
    }
    void handleAsyncUpdate() override { edit.restartPlayback(); }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state && (id == juce::Identifier ("linearPhase") || (linActive && (id == juce::Identifier ("bands") || id.toString().startsWith ("x"))))) triggerAsyncUpdate();
    }
};

//==============================================================================
class SqApexSubPlugin : public SqApexBase
{
public:
    enum P { lower, upper, direct, hpFreq, hpQ, lpFreq, lpQ, drive, mixP, numParams };
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "lower",  "Lower",  -60.0f, 12.0f, -3.0f, 2.0f, " dB", 1 },
            { "upper",  "Upper",  -60.0f, 12.0f, -18.0f, 2.0f, " dB", 1 },
            { "direct", "Direct", -60.0f, 12.0f, 0.0f, 2.0f, " dB", 1 },
            { "hpFreq", "High-pass", 5.0f, 1000.0f, 20.0f, 0.35f, " Hz", 0 },
            { "hpQ",    "High-pass Q", 0.1f, 5.0f, 0.7f, 0.5f, "", 2 },
            { "lpFreq", "Low-pass",  5.0f, 1000.0f, 180.0f, 0.35f, " Hz", 0 },
            { "lpQ",    "Low-pass Q", 0.1f, 5.0f, 0.7f, 0.5f, "", 2 },
            { "drive",  "Drive",  0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "mix",    "Mix",    0.0f, 100.0f, 50.0f, 1.0f, " %", 0 } };
        return s;
    }
    struct Range { const char* name; float lo, hi; };
    static const std::array<Range, 6>& ranges()
    {
        static const std::array<Range, 6> r {{ { "20-40", 20, 40 }, { "30-60", 30, 60 }, { "40-80", 40, 80 }, { "55-110", 55, 110 }, { "75-150", 75, 150 }, { "100-200", 100, 200 } }};
        return r;
    }
    static juce::StringArray rangeNames()   { juce::StringArray s; for (auto& r : ranges()) s.add (r.name); return s; }
    static juce::StringArray driveNames()   { return { "CLEAN", "SOFT", "WARM", "TUBE", "FUZZ", "FOLD" }; }

    SqApexSubPlugin (te::PluginCreationInfo info) : SqApexBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        range.referTo (state, "range", um, 2);
        midiTune.referTo (state, "midiTune", um, false);
        driveType.referTo (state, "driveType", um, 0);
    }
    ~SqApexSubPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    static const char* getPluginName()                  { return NEEDS_TRANS("Apex Sub"); }
    static constexpr const char* xmlTypeName = "sqmApexSub";
    juce::String getName() const override               { return "Apex Sub"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "ApexSub"; }
    juce::String getSelectableDescription() override    { return "Apex Sub"; }
    bool takesMidiInput() override                      { return true; }

    juce::CachedValue<int> range, driveType;
    juce::CachedValue<bool> midiTune;
    std::atomic<float> trackedHz { 0 }, subLevel { 0 };
    void resetProps (juce::UndoManager* um) override { range.setValue (2, um); midiTune.setValue (false, um); driveType.setValue (0, um); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto* f : { &bp1, &bp2, &hp1, &hp2, &lp1, &lp2 }) f->reset();
        for (auto& f : outHp) f.reset();
        for (auto& f : outLp) f.reset();
        env = 0; ph = 0; ph2 = 0; lastSign = 0; samplesSinceZc = 0; periodSm = 0; midiNote = -1; freq = 0;
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
                if (m.isNoteOn()) midiNote = m.getNoteNumber();
                else if (m.isNoteOff() && m.getNoteNumber() == midiNote) midiNote = -1;
            }
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        const auto& rg = ranges()[(size_t) juce::jlimit (0, 5, range.get())];
        const double centre = std::sqrt (rg.lo * rg.hi);
        bp1.set (sr, centre, 0.9); bp2.set (sr, centre, 0.9);
        for (auto& f : outHp) f.set (sr, bank[hpFreq], bank[hpQ]);
        for (auto& f : outLp) f.set (sr, bank[lpFreq], bank[lpQ]);
        const float gLo = gainOrZero (bank[lower]), gUp = gainOrZero (bank[upper]), gDir = gainOrZero (bank[direct]);
        const float drv = bank[drive] / 100.0f, mx = bank[mixP] / 100.0f;
        const float wetG = juce::jmin (1.0f, 2.0f * mx), dryG = juce::jmin (1.0f, 2.0f * (1.0f - mx));
        const int dtype = driveType.get();
        const bool useMidi = midiTune.get();
        const float aEnvA = (float) (1.0 - std::exp (-1.0 / (0.003 * sr))), aEnvR = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
        float lvl = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = b.getSample (0, s0 + i), r = b.getSample (nch > 1 ? 1 : 0, s0 + i);
            const float m = 0.5f * (l + r);
            const float band = (float) bp2.bandpass (bp1.bandpass (m));
            const float a = std::abs (band);
            env += (a > env ? aEnvA : aEnvR) * (a - env);
            // period tracking from positive-going zero crossings of the band
            ++samplesSinceZc;
            // Schmitt trigger at 30% of the band's envelope, so noise and leakage don't count as cycles
            const float hyst = juce::jmax (1.0e-4f, env * 0.3f);
            const int sgn = band > hyst ? 1 : band < -hyst ? -1 : lastSign;
            if (sgn > 0 && lastSign < 0)
            {
                const double per = (double) samplesSinceZc;
                const double hz = sr / per;
                if (hz >= rg.lo * 0.7 && hz <= rg.hi * 1.4) periodSm = periodSm <= 0 ? per : periodSm + 0.3 * (per - periodSm);
                samplesSinceZc = 0;
                // gently pull the sub oscillator's phase onto every second cycle of the source
                if (++cycle % 2 == 0) { const double e = ph - std::round (ph); ph -= 0.15 * e; }
            }
            lastSign = sgn;
            if (samplesSinceZc > (int) (sr / (rg.lo * 0.5)) || env < 1.0e-3f) periodSm = 0;   // no low end: stop tracking
            freq = useMidi && midiNote >= 0 ? 440.0 * std::pow (2.0, (midiNote - 69) / 12.0) : (periodSm > 0 ? sr / periodSm : 0.0);
            float wet = 0;
            if (freq > 0)
            {
                ph += 0.5 * freq / sr; ph -= std::floor (ph);
                ph2 += 0.75 * freq / sr; ph2 -= std::floor (ph2);
                const float e = useMidi && midiNote >= 0 ? juce::jmax (env, 0.0f) : env;
                wet = (float) (gLo * std::sin (juce::MathConstants<double>::twoPi * ph) + gUp * std::sin (juce::MathConstants<double>::twoPi * ph2)) * e * 2.0f;
            }
            wet = shape (wet, drv, dtype);
            wet = (float) outLp[1].lowpass (outLp[0].lowpass (outHp[1].highpass (outHp[0].highpass (wet))));
            lvl = juce::jmax (lvl, std::abs (wet));
            const float dl = l * gDir, dr = r * gDir;
            b.setSample (0, s0 + i, dl * dryG + wet * wetG);
            if (nch > 1) b.setSample (1, s0 + i, dr * dryG + wet * wetG);
        }
        trackedHz = (float) freq; subLevel = lvl;
        inDb = Apex::gainToDb (env); outDb = Apex::gainToDb (lvl);
    }

private:
    Apex::Svf bp1, bp2, hp1, hp2, lp1, lp2, outHp[2], outLp[2];
    float env = 0; double ph = 0, ph2 = 0, periodSm = 0, freq = 0; int lastSign = 0, samplesSinceZc = 0, midiNote = -1, cycle = 0;
    static float gainOrZero (float db) { return db <= -59.5f ? 0.0f : Apex::dbToGain (db); }
    static float shape (float x, float d, int t)
    {
        if (d <= 0.001f && t == 0) return x;
        const float g = 1.0f + d * 12.0f;
        switch (t)
        {
            case 1:  return std::tanh (x * g) / std::sqrt (g);                                   // soft
            case 2:  { const float b = 0.2f * d; return (std::tanh (x * g + b) - std::tanh (b)) / std::sqrt (g); }   // warm (even harmonics)
            case 3:  return (x * g) / (1.0f + std::abs (x * g)) * 1.4f / std::sqrt (g);         // tube
            case 4:  return juce::jlimit (-0.5f, 0.5f, x * g * 2.0f) / std::sqrt (g) * 1.5f;    // fuzz
            case 5:  return std::sin (juce::jlimit (-6.0f, 6.0f, x * g * 1.5f)) * 0.6f;          // fold
            default: return x * (1.0f + d * 0.5f);                                               // clean (just more level)
        }
    }
};
