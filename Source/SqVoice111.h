#pragma once
// Squelchmoth 1.11 Voice family, first wave (super prompt §3, phase C): Moth Key, Siren Comp, Siren EQ, Hush De-Ess 2.
// The DSP lives in SqVoice111Dsp.h (standard C++, measured by tests/voice111_test.cpp); this file wraps it as
// Tracktion plug-ins. Moth Tune 2 is the upgraded sqmTune (SqVocal.h / SqVocalDsp.h).

#include "SqVoice111Dsp.h"
#include "SqVoiceCore.h"
#include "SqColour.h"
#include "SqRadiantMaw.h"   // SqSmoother
#include "Scales.h"
#include <deque>

/** A smoother for any number of parameters (SqSmoother holds 16). */
template <int N> struct SqSmootherN
{
    float sm[N] {}; bool init = false;
    void apply (float* v, int n, bool snap)
    {
        n = juce::jmin (n, N);
        if (snap || ! init) { std::copy (v, v + n, sm); init = true; }
        else for (int i = 0; i < n; ++i) sm[i] += 0.35f * (v[i] - sm[i]);
        std::copy (sm, sm + n, v);
    }
};

/** Set a bank parameter by index (message thread), as if the knob moved. */
inline void sqSetParam (SqDsp::ParamBank& b, int i, float v)
{
    if (! juce::isPositiveAndBelow (i, (int) b.params.size())) return;
    auto& s = b.specs[(size_t) i];
    b.params[(size_t) i]->setParameter (juce::jlimit (s.lo, s.hi, v), juce::sendNotification);
}

//==============================================================================
// Moth Key: key, scale and tempo from audio, with key changes over time, written to the session's key track.
namespace MothKeyAnalysis
{
    /** Mono, ~11 kHz copy of a track's audio clips placed on the edit timeline from `start` to `end` seconds. */
    inline std::vector<float> readTrack (te::AudioTrack& track, double start, double end, double& fsOut, std::function<bool()> cancelled = {})
    {
        const double fs = 11025.0; fsOut = fs;
        end = juce::jmax (start + 1.0, end);
        std::vector<float> out ((size_t) ((end - start) * fs) + 1, 0.0f);
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        for (auto clip : track.getClips())
        {
            auto ac = dynamic_cast<te::AudioClipBase*> (clip);
            if (ac == nullptr || ac->isMuted()) continue;
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (ac->getAudioFile().getFile()));
            if (reader == nullptr) continue;
            const double fsr = reader->sampleRate;
            const auto pos = ac->getPosition();
            const double speed = juce::jmax (0.01, ac->getSpeedRatio());
            const double cs = pos.getStart().inSeconds(), ce = pos.getEnd().inSeconds();
            const double a = juce::jmax (cs, start), b = juce::jmin (ce, end);
            if (b <= a) continue;
            const double srcStart = (pos.getOffset().inSeconds() + (a - cs)) * speed;
            const juce::int64 s0 = juce::jmax ((juce::int64) 0, (juce::int64) (srcStart * fsr));
            const juce::int64 n = juce::jmin ((juce::int64) ((b - a) * speed * fsr), reader->lengthInSamples - s0);
            if (n <= 0) continue;
            const double step = fsr * speed / fs;                 // source samples per output sample
            Eng::Biquad l1, l2; l1.set (Eng::Biquad::lowpass, fsr, juce::jmin (4500.0, fs * 0.42 * speed), 0.707); l2 = l1;
            const int chunk = 65536; juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), chunk);
            double nextOut = 0; size_t o = (size_t) ((a - start) * fs);
            for (juce::int64 done = 0; done < n && o < out.size(); done += chunk)
            {
                if (cancelled && cancelled()) return {};
                const int m = (int) juce::jmin ((juce::int64) chunk, n - done);
                reader->read (&buf, 0, m, s0 + done, true, true);
                for (int i = 0; i < m; ++i)
                {
                    float x = 0; for (int ch = 0; ch < buf.getNumChannels(); ++ch) x += buf.getSample (ch, i);
                    x = l2.process (l1.process (x / (float) buf.getNumChannels()));
                    if ((double) (done + i) >= nextOut) { if (o < out.size()) out[o++] += x; nextOut += step; }
                }
            }
        }
        return out;
    }
    /** Mono ~11 kHz copy of an audio file (WAV / AIFF / FLAC / MP3 / OGG). */
    inline std::vector<float> readFile (const juce::File& f, double& fsOut, double maxSeconds = 900)
    {
        const double fs = 11025.0; fsOut = fs;
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr) return {};
        const double fsr = reader->sampleRate; const juce::int64 n = juce::jmin (reader->lengthInSamples, (juce::int64) (maxSeconds * fsr));
        std::vector<float> out; out.reserve ((size_t) (n * fs / fsr) + 8);
        Eng::Biquad l1, l2; l1.set (Eng::Biquad::lowpass, fsr, juce::jmin (4500.0, fs * 0.42), 0.707); l2 = l1;
        const int chunk = 65536; juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), chunk);
        const double step = fsr / fs; double nextOut = 0;
        for (juce::int64 done = 0; done < n; done += chunk)
        {
            const int m = (int) juce::jmin ((juce::int64) chunk, n - done);
            reader->read (&buf, 0, m, done, true, true);
            for (int i = 0; i < m; ++i)
            {
                float x = 0; for (int ch = 0; ch < buf.getNumChannels(); ++ch) x += buf.getSample (ch, i);
                x = l2.process (l1.process (x / (float) buf.getNumChannels()));
                if ((double) (done + i) >= nextOut) { out.push_back (x); nextOut += step; }
            }
        }
        return out;
    }

    inline juce::String keyText (int root, bool minor)   { return juce::String (Voice::noteName (root)) + (minor ? " minor" : " major"); }

    /** The result as JSON (Claude and the face read the same thing). `offset` = edit time of the analysed audio's start. */
    inline juce::var toVar (const Voice::KeyResult& r, double offset)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("key", keyText (r.root, r.minor));
        o->setProperty ("root", Voice::noteName (r.root)); o->setProperty ("scale", r.minor ? "minor" : "major");
        o->setProperty ("confidence", juce::roundToInt (r.confidence * 100) / 100.0);
        if (r.bpmConfidence >= 0.15f) o->setProperty ("bpm", juce::roundToInt (r.bpm * 10) / 10.0);
        else o->setProperty ("bpm", "unclear (no steady beat)");
        o->setProperty ("bpm_confidence", juce::roundToInt (r.bpmConfidence * 100) / 100.0);
        juce::Array<juce::var> alts; for (auto& a : r.alternatives) { auto* x = new juce::DynamicObject(); x->setProperty ("key", keyText (a.root, a.minor)); x->setProperty ("score", juce::roundToInt (a.score * 1000) / 1000.0); alts.add (juce::var (x)); }
        o->setProperty ("alternatives", alts);
        juce::Array<juce::var> segs; for (auto& s : r.segments) { auto* x = new juce::DynamicObject(); x->setProperty ("start_s", juce::roundToInt ((s.start + offset) * 10) / 10.0); x->setProperty ("end_s", juce::roundToInt ((s.end + offset) * 10) / 10.0); x->setProperty ("key", keyText (s.root, s.minor)); segs.add (juce::var (x)); }
        o->setProperty ("key_changes", segs);
        juce::Array<juce::var> ch; for (float c : r.chroma) ch.add (juce::roundToInt (c * 100) / 100.0);
        o->setProperty ("chroma", ch);
        return juce::var (o);
    }

    /** Write the key (and its changes over time) to the session: Moth Tune, Hive Choir and every key-aware plug-in
        follow edit.state sqmKeyRoot / sqmKeyScale, and the key map (sqmKeyMap "time:root:minor;...") over time. */
    inline void writeToSession (te::Edit& edit, const Voice::KeyResult& r, double offset, bool setTempo)
    {
        auto* um = &edit.getUndoManager();
        edit.state.setProperty ("sqmKeyRoot", r.root, um);
        edit.state.setProperty ("sqmKeyScale", r.minor ? 2 : 1, um);
        juce::String map;
        for (auto& s : r.segments) map << juce::String (s.start + offset, 2) << ":" << s.root << ":" << (s.minor ? 1 : 0) << ";";
        edit.state.setProperty ("sqmKeyMap", r.segments.size() > 1 ? map : juce::String(), um);
        if (setTempo && r.bpm > 40 && r.bpmConfidence > 0.15f)
            if (auto t = edit.tempoSequence.getTempo (0)) t->setBpm (juce::roundToInt (r.bpm * 10) / 10.0);
    }

    /** The key in force at edit time t from a sqmKeyMap string (falls back to root / scale). */
    inline void keyAt (const juce::String& map, double t, int& root, int& scale)
    {
        if (map.isEmpty()) return;
        for (auto& e : juce::StringArray::fromTokens (map, ";", ""))
        {
            auto p = juce::StringArray::fromTokens (e, ":", ""); if (p.size() < 3) continue;
            if (p[0].getDoubleValue() <= t + 1e-6) { root = p[1].getIntValue(); scale = p[2].getIntValue() ? 2 : 1; }
        }
    }
}

class SqMothKeyPlugin : public SqColourBase, private juce::Timer
{
public:
    enum P { length, numParams };
    SQ_COLOUR_IDENTITY (SqMothKeyPlugin, "Moth Key", "sqmMothKey", "Key")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s { { "length", "Listen Length", 10.0f, 480.0f, 240.0f, 0.5f, " s", 0 } };
        return s;
    }
    SqMothKeyPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        listen.referTo (state, "listenOn", um, true); autoApply.referTo (state, "autoApplyOn", um, true); applyTempo.referTo (state, "tempoOn", um, false);
        resultJson.referTo (state, "result", nullptr, {});
        cap.assign ((size_t) (11025.0 * 490), 0.0f); marks.assign (cap.size() / 512 + 4, -1.0);
        startTimerHz (2);
    }
    ~SqMothKeyPlugin() override { stopTimer(); cancel = true; if (worker.joinable()) worker.join(); notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<bool> listen, autoApply, applyTempo; juce::CachedValue<juce::String> resultJson;
    void resetProps (juce::UndoManager* um) override { listen.setValue (true, um); autoApply.setValue (true, um); applyTempo.setValue (false, um); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        D = juce::jmax (1, juce::roundToInt (sr / 11025.0)); fs = sr / D;
        aa1.set (Eng::Biquad::lowpass, sr, juce::jmin (4500.0, fs * 0.42), 0.707); aa2 = aa1; dcount = 0;
    }
    void deinitialise() override {}
    void reset() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        // audio passes untouched; while listening, a mono ~11 kHz copy is kept (the last 8 minutes)
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0 || ! listen.get()) return;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        levelIn = b.getMagnitude (0, s0, n); levelOut = levelIn.load();
        if (! fc.isPlaying && levelIn.load() < 1.0e-5f) return;
        const float* l = b.getReadPointer (0, s0); const float* r = b.getReadPointer (nch > 1 ? 1 : 0, s0);
        juce::int64 w = writePos.load (std::memory_order_relaxed);
        const size_t N = cap.size();
        marks[(size_t) ((w / 512) % (juce::int64) marks.size())] = fc.isPlaying ? fc.editTime.getStart().inSeconds() : -1.0;
        for (int i = 0; i < n; ++i)
        {
            const float y = aa2.process (aa1.process (0.5f * (l[i] + r[i])));
            if (++dcount >= D) { dcount = 0; cap[(size_t) (w % (juce::int64) N)] = y; ++w; if ((w % 512) == 0) marks[(size_t) ((w / 512) % (juce::int64) marks.size())] = fc.isPlaying ? fc.editTime.getStart().inSeconds() + (double) i / sr : -1.0; }
        }
        writePos.store (w, std::memory_order_release);
    }

    /** Seconds of audio heard so far (up to the Listen Length). */
    double heardSeconds() const   { return juce::jmin ((double) writePos.load() / juce::jmax (1.0, fs), (double) bank[length]); }
    bool isAnalysing() const      { return busy.load(); }
    void clearListening()         { writePos = 0; std::fill (marks.begin(), marks.end(), -1.0); }

    /** Analyse what has been heard (background thread); the result lands in "result" and, with Auto Apply, the session. */
    bool analyseHeard()
    {
        if (busy.load()) return false;
        const juce::int64 w = writePos.load(); const juce::int64 want = juce::jmin (w, (juce::int64) (bank[length] * fs));
        if (want < (juce::int64) (fs * 4)) { setStatus ("Play at least 4 seconds of music through Moth Key first"); return false; }
        auto data = std::make_shared<std::vector<float>> ((size_t) want);
        const size_t N = cap.size();
        for (juce::int64 i = 0; i < want; ++i) (*data)[(size_t) i] = cap[(size_t) ((w - want + i) % (juce::int64) N)];
        // edit time of the first analysed sample, when it was heard during playback
        const juce::int64 first = w - want; double off = marks[(size_t) ((first / 512) % (juce::int64) marks.size())];
        if (off >= 0) off -= (double) (first % 512) / fs;
        runAnalysis (data, fs, off);
        return true;
    }
    /** Analyse a track's clips (Claude's detect_key and the face's "Analyse track"). */
    bool analyseTrack (te::AudioTrack& t, double start, double end)
    {
        if (busy.load()) return false;
        double f = 11025; auto v = std::make_shared<std::vector<float>> (MothKeyAnalysis::readTrack (t, start, end, f));
        if (v->size() < (size_t) (f * 4)) { setStatus ("No readable audio on " + t.getName()); return false; }
        runAnalysis (v, f, start);
        return true;
    }
    juce::String status() const   { const juce::ScopedLock sl (lock); return statusText; }
    Voice::KeyResult lastResult() const { const juce::ScopedLock sl (lock); return result; }
    double lastOffset() const { const juce::ScopedLock sl (lock); return resultOffset; }
    bool hasResult() const { const juce::ScopedLock sl (lock); return haveResult; }
    void applyToSession() { if (hasResult()) MothKeyAnalysis::writeToSession (edit, lastResult(), juce::jmax (0.0, lastOffset()), applyTempo.get()); }

private:
    std::vector<float> cap; std::vector<double> marks; std::atomic<juce::int64> writePos { 0 };
    int D = 4, dcount = 0; double fs = 12000; Eng::Biquad aa1, aa2;
    std::thread worker; std::atomic<bool> busy { false }, cancel { false }, fresh { false };
    juce::CriticalSection lock; juce::String statusText { "Listening" }; Voice::KeyResult result; double resultOffset = 0; bool haveResult = false;
    void setStatus (const juce::String& s) { const juce::ScopedLock sl (lock); statusText = s; }
    void runAnalysis (std::shared_ptr<std::vector<float>> data, double rate, double offset)
    {
        if (worker.joinable()) worker.join();
        busy = true; setStatus ("Analysing " + juce::String (data->size() / rate, 0) + " s...");
        worker = std::thread ([this, data, rate, offset]
        {
            Voice::KeyFinder kf; const auto r = kf.analyse (data->data(), data->size(), rate);
            { const juce::ScopedLock sl (lock); result = r; resultOffset = offset; haveResult = true;
              statusText = MothKeyAnalysis::keyText (r.root, r.minor) + ", " + juce::String (r.bpm, 1) + " BPM"; }
            fresh = true; busy = false;
        });
    }
    void timerCallback() override
    {
        if (! fresh.exchange (false)) return;
        if (worker.joinable()) worker.join();
        resultJson.setValue (juce::JSON::toString (MothKeyAnalysis::toVar (lastResult(), juce::jmax (0.0, lastOffset())), true), nullptr);
        if (autoApply.get()) applyToSession();
    }
};

//==============================================================================
// Siren Comp
class SqSirenCompPlugin : public SqColourBase
{
public:
    enum P { thr1, atk1, rel1, ratio1, knee1, thr2, atk2, rel2, ratio2, warm, mix, makeup, output, punchGain, punchImpact, punchCeiling, duck, numParams };
    SQ_COLOUR_IDENTITY (SqSirenCompPlugin, "Siren Comp", "sqmSirenComp", "Siren C")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "thr1",    "Threshold 1", -60.0f, 0.0f, -18.0f, 1.0f, " dB", 1 },
            { "atk1",    "Attack 1",     0.02f, 100.0f, 5.0f, 0.3f, " ms", 2 },
            { "rel1",    "Release 1",    5.0f, 2000.0f, 150.0f, 0.35f, " ms", 0 },
            { "ratio1",  "Ratio 1",      1.0f, 20.0f, 4.0f, 0.5f, ":1", 1 },
            { "knee1",   "Knee",         0.0f, 18.0f, 6.0f, 1.0f, " dB", 1 },
            { "thr2",    "Threshold 2", -60.0f, 0.0f, -24.0f, 1.0f, " dB", 1 },
            { "atk2",    "Attack 2",     0.02f, 100.0f, 10.0f, 0.3f, " ms", 2 },
            { "rel2",    "Release 2",    5.0f, 2000.0f, 300.0f, 0.35f, " ms", 0 },
            { "ratio2",  "Ratio 2",      1.0f, 20.0f, 3.0f, 0.5f, ":1", 1 },
            { "warm",    "Warm",         0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "mix",     "Mix",          0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "makeup",  "Make-Up",    -12.0f, 24.0f, 0.0f, 1.0f, " dB", 1 },
            { "output",  "Output",     -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "pGain",   "Punch Gain",   0.0f, 18.0f, 0.0f, 1.0f, " dB", 1 },
            { "pImpact", "Impact",       0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "pCeil",   "Ceiling",    -12.0f, 0.0f, -1.0f, 1.0f, " dB", 1 },
            { "duck",    "Duck Depth",   0.0f, 18.0f, 6.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray modelNames()  { return { "FET", "OPTO A", "OPTO B", "MODERN" }; }
    static juce::StringArray fetNames()    { return { "4:1", "8:1", "12:1", "20:1", "ALL" }; }
    static juce::StringArray amountNames() { return { "MINIMAL", "CONTROLLED", "AGGRESSIVE" }; }
    SqSirenCompPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        model1.referTo (state, "model1", um, 1); model2.referTo (state, "model2", um, 1); fet1.referTo (state, "fet1", um, 0); fet2.referTo (state, "fet2", um, 0);
        dual.referTo (state, "dualOn", um, false); autoMakeup.referTo (state, "autoMakeupOn", um, true); punch.referTo (state, "punchOn", um, false);
        keyOn.referTo (state, "keyOn", um, false); amount.referTo (state, "assistAmount", um, 1); preview.referTo (state, "previewOn", nullptr, false);
    }
    ~SqSirenCompPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> model1, model2, fet1, fet2, amount; juce::CachedValue<bool> dual, autoMakeup, punch, keyOn, preview;
    void resetProps (juce::UndoManager* um) override
    {
        model1.setValue (1, um); model2.setValue (1, um); fet1.setValue (0, um); fet2.setValue (0, um); dual.setValue (false, um);
        autoMakeup.setValue (true, um); punch.setValue (false, um); keyOn.setValue (false, um); preview.setValue (false, nullptr);
    }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); if (ins != nullptr) { ins->add ("Key L"); ins->add ("Key R"); } }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true, proc.p); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples));
        keyCore.prepare (sr, juce::jmax (64, info.blockSizeSamples)); keyMono.assign ((size_t) juce::jmax (64, info.blockSizeSamples) * 2, 0.0f);
    }
    void deinitialise() override {}
    void reset() override { proc.prepare (sr, 512); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        bool used = false;
        if (preview.get()) { const juce::SpinLock::ScopedTryLockType l (sugLock); if (l.isLocked() && haveSuggestion) { proc.p = suggestion; used = true; } }
        if (! used) read (false, proc.p);
        proc.learning = learning.load();
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        const float* key[2] = { nullptr, nullptr };
        const bool haveKey = keyOn.get() && b.getNumChannels() > 2;
        if (haveKey)
        {
            key[0] = b.getReadPointer (2, s0); key[1] = b.getReadPointer (b.getNumChannels() > 3 ? 3 : 2, s0);
            if ((int) keyMono.size() >= n) { for (int i = 0; i < n; ++i) keyMono[(size_t) i] = 0.5f * (key[0][i] + key[1][i]); keyCore.analyse (keyMono.data(), n); }
            const auto& sn = keyCore.latest(); proc.setKeyPitch (sn.voiced ? sn.hz : 0.0f); keyHz = sn.voiced ? sn.hz : 0.0f;
        }
        levelIn = b.getMagnitude (0, s0, n); proc.process (ch, nch, haveKey ? key : nullptr, n); levelOut = b.getMagnitude (0, s0, n);
        gr1 = proc.gr1(); gr2 = proc.gr2(); punchGr = proc.punchGrDb(); outDb = proc.outLevelDb();
    }
    std::atomic<float> gr1 { 0 }, gr2 { 0 }, punchGr { 0 }, outDb { -100 }, keyHz { 0 };
    std::atomic<bool> learning { false };
    Voice::Comp::Processor proc;

    // ---- Assist (message thread): learn -> suggest (preview, A/B) -> apply or discard
    void startLearning()  { learning = true; proc.learnClear(); }
    void stopLearning()   { learning = false; }
    bool makeSuggestion (int amt)
    {
        auto s = proc.suggest (juce::jlimit (0, 2, amt));
        { const juce::SpinLock::ScopedLockType l (sugLock); suggestion = s; haveSuggestion = true; }
        amount.setValue (amt, nullptr);
        return true;
    }
    bool hasSuggestion() const { return haveSuggestion; }
    Voice::Comp::Params suggestionCopy() { const juce::SpinLock::ScopedLockType l (sugLock); return suggestion; }
    /** Commit the suggestion to the knobs (undoable). */
    void applySuggestion()
    {
        if (! haveSuggestion) return;
        const auto s = suggestionCopy(); auto um = getUndoManager();
        model1.setValue (s.s1.model, um); fet1.setValue (s.s1.fetIdx, um); model2.setValue (s.s2.model, um); fet2.setValue (s.s2.fetIdx, um); dual.setValue (s.dual, um);
        sqSetParam (bank, thr1, s.s1.thresholdDb); sqSetParam (bank, atk1, s.s1.attackMs); sqSetParam (bank, rel1, s.s1.releaseMs); sqSetParam (bank, ratio1, s.s1.ratio);
        sqSetParam (bank, thr2, s.s2.thresholdDb); sqSetParam (bank, atk2, s.s2.attackMs); sqSetParam (bank, rel2, s.s2.releaseMs); sqSetParam (bank, ratio2, s.s2.ratio);
        autoMakeup.setValue (s.autoMakeup, um); sqSetParam (bank, makeup, s.makeupDb);
        preview.setValue (false, nullptr);
    }
private:
    SqSmoother smo; juce::SpinLock sugLock; Voice::Comp::Params suggestion; bool haveSuggestion = false;
    SqVoiceCore::Core keyCore; std::vector<float> keyMono;
    void read (bool snap, Voice::Comp::Params& p)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smo.apply (v, juce::jmin (16, (int) numParams), snap);   // the duck depth (last) needs no smoothing
        fill (v, p);
    }
public:
    /** What the knobs say right now (for the face; the audio thread smooths its own copy). */
    Voice::Comp::Params knobParams() const { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; Voice::Comp::Params p; fill (v, p); return p; }
private:
    void fill (const float* v, Voice::Comp::Params& p) const
    {
        p.s1.model = juce::jlimit (0, 3, model1.get()); p.s1.fetIdx = juce::jlimit (0, 4, fet1.get()); p.s1.thresholdDb = v[thr1]; p.s1.attackMs = v[atk1]; p.s1.releaseMs = v[rel1]; p.s1.ratio = v[ratio1]; p.s1.kneeDb = v[knee1];
        p.s2.model = juce::jlimit (0, 3, model2.get()); p.s2.fetIdx = juce::jlimit (0, 4, fet2.get()); p.s2.thresholdDb = v[thr2]; p.s2.attackMs = v[atk2]; p.s2.releaseMs = v[rel2]; p.s2.ratio = v[ratio2]; p.s2.kneeDb = v[knee1];
        p.dual = dual.get(); p.warm = v[warm] / 100.0f; p.mix = v[mix] / 100.0f; p.makeupDb = v[makeup]; p.outputDb = v[output]; p.autoMakeup = autoMakeup.get();
        p.punch = punch.get(); p.punchGainDb = v[punchGain]; p.punchImpact = v[punchImpact] / 100.0f; p.punchCeilingDb = v[punchCeiling];
        p.duckDepthDb = v[duck]; p.keyOn = keyOn.get();
    }
};

//==============================================================================
// Siren EQ
class SqSirenEqPlugin : public SqColourBase
{
public:
    // per band b (0..5): freq, gain, q, threshold, range  ->  index b * 5 + k ; then the global ones
    enum G { hpFreq = 30, lpFreq, airFreq, airGain, tilt, output, numParams };
    static constexpr int bandParam (int b, int k) { return b * 5 + k; }
    SQ_COLOUR_IDENTITY (SqSirenEqPlugin, "Siren EQ", "sqmSirenEq", "Siren EQ")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static std::deque<std::string> names;
        static const std::vector<SqDsp::ParamSpec> specs = []
        {
            std::vector<SqDsp::ParamSpec> s;
            auto keep = [] (const std::string& x) { names.push_back (x); return names.back().c_str(); };
            const float defF[6] = { 120, 300, 1000, 3000, 5500, 10000 };
            for (int b = 0; b < 6; ++b)
            {
                const auto n = std::to_string (b + 1);
                s.push_back ({ keep ("b" + n + "Freq"),  keep ("Band " + n + " Freq"),  20.0f, 20000.0f, defF[b], 0.25f, " Hz", 0 });
                s.push_back ({ keep ("b" + n + "Gain"),  keep ("Band " + n + " Gain"), -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1 });
                s.push_back ({ keep ("b" + n + "Q"),     keep ("Band " + n + " Q"),      0.1f, 18.0f, 1.0f, 0.4f, "", 2 });
                s.push_back ({ keep ("b" + n + "Thr"),   keep ("Band " + n + " Threshold"), -60.0f, 0.0f, -30.0f, 1.0f, " dB", 1 });
                s.push_back ({ keep ("b" + n + "Range"), keep ("Band " + n + " Range"),  0.0f, 24.0f, 6.0f, 1.0f, " dB", 1 });
            }
            s.push_back ({ "hpFreq",  "Low Cut",   20.0f, 1000.0f, 20.0f, 0.3f, " Hz", 0 });
            s.push_back ({ "lpFreq",  "High Cut",  1000.0f, 20000.0f, 20000.0f, 0.3f, " Hz", 0 });
            s.push_back ({ "airFreq", "Air Freq",  2500.0f, 40000.0f, 12000.0f, 0.3f, " Hz", 0 });
            s.push_back ({ "airGain", "Air",       -12.0f, 18.0f, 0.0f, 1.0f, " dB", 1 });
            s.push_back ({ "tilt",    "Tilt",      -6.0f, 6.0f, 0.0f, 1.0f, " dB", 1 });
            s.push_back ({ "output",  "Output",    -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 });
            return s;
        }();
        return specs;
    }
    static juce::StringArray typeNames()  { return { "BELL", "LOW SHELF", "HIGH SHELF", "NOTCH" }; }
    static juce::StringArray modeNames()  { return { "FIXED", "PITCH", "DYNAMIC" }; }
    static juce::StringArray slopeNames() { return { "6 dB", "12 dB", "18 dB", "24 dB" }; }
    SqSirenEqPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        for (int b = 0; b < 6; ++b)
        {
            const juce::String n (b + 1);
            on[(size_t) b].referTo (state, "b" + n + "On", um, false);
            type[(size_t) b].referTo (state, "b" + n + "Type", um, b == 0 ? 1 : b == 5 ? 2 : 0);
            mode[(size_t) b].referTo (state, "b" + n + "Mode", um, 0);
            harm[(size_t) b].referTo (state, "b" + n + "Harm", um, 1);
            expand[(size_t) b].referTo (state, "b" + n + "Expand", um, false);
            solo[(size_t) b].referTo (state, "b" + n + "Solo", nullptr, false);
            mute[(size_t) b].referTo (state, "b" + n + "Mute", um, false);
        }
        hpSlope.referTo (state, "hpSlope", um, 1); lpSlope.referTo (state, "lpSlope", um, 1);
        preview.referTo (state, "previewOn", nullptr, false); suggestionText.referTo (state, "suggestion", nullptr, {});
    }
    ~SqSirenEqPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    std::array<juce::CachedValue<bool>, 6> on, expand, solo, mute; std::array<juce::CachedValue<int>, 6> type, mode, harm;
    juce::CachedValue<int> hpSlope, lpSlope; juce::CachedValue<bool> preview; juce::CachedValue<juce::String> suggestionText;
    void resetProps (juce::UndoManager* um) override
    {
        for (int b = 0; b < 6; ++b) { on[(size_t) b].setValue (false, um); type[(size_t) b].setValue (b == 0 ? 1 : b == 5 ? 2 : 0, um); mode[(size_t) b].setValue (0, um); harm[(size_t) b].setValue (1, um); expand[(size_t) b].setValue (false, um); solo[(size_t) b].setValue (false, nullptr); mute[(size_t) b].setValue (false, um); }
        hpSlope.setValue (1, um); lpSlope.setValue (1, um); preview.setValue (false, nullptr);
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, info.blockSizeSamples);
        core.prepare (sr, juce::jmax (64, info.blockSizeSamples)); mono.assign ((size_t) juce::jmax (64, info.blockSizeSamples) * 2, 0.0f);
    }
    void deinitialise() override {}
    void reset() override { proc.prepare (sr, 512); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        read (false);
        { const juce::SpinLock::ScopedTryLockType l (sugLock); if (l.isLocked()) { proc.p.suggested = sugBands; proc.p.suggestedHp = sugHp; } }
        proc.p.preview = preview.get() && haveSuggestion;
        proc.learning = learning.load();
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        if ((int) mono.size() >= n)
        {
            for (int i = 0; i < n; ++i) mono[(size_t) i] = nch > 1 ? 0.5f * (ch[0][i] + ch[1][i]) : ch[0][i];
            core.analyse (mono.data(), n);
            const auto& sn = core.latest(); proc.setPitch (sn.voiced ? sn.hz : 0.0f); pitchHz = sn.voiced ? sn.hz : 0.0f;
            if (sn.voiced) proc.learnPitch (sn.hz);
        }
        levelIn = b.getMagnitude (0, s0, n); proc.process (ch, nch, n); levelOut = b.getMagnitude (0, s0, n);
        // spectrum feed for the face (output, mono)
        juce::int64 w = specW.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) spec[(size_t) (w++ & (specLen - 1))] = nch > 1 ? 0.5f * (ch[0][i] + ch[1][i]) : ch[0][i];
        specW.store (w, std::memory_order_release);
        for (int k = 0; k < 6; ++k) { bandHz[(size_t) k] = proc.bandFreq (k); bandDyn[(size_t) k] = proc.bandDynGain (k); }
    }
    static constexpr int specLen = 4096;
    std::array<float, specLen> spec {}; std::atomic<juce::int64> specW { 0 };
    std::atomic<float> pitchHz { 0 }; std::array<std::atomic<float>, 6> bandHz {}, bandDyn {};
    std::atomic<bool> learning { false };
    Voice::Eq::Processor proc;
    double rate() const { return sr; }

    // ---- Vocal Learning (message thread): learn -> suggestion (audition with Preview) -> apply or discard
    void startLearning() { proc.learnReset(); learning = true; }
    /** Stop learning and build the suggestion; returns a one-line description. */
    juce::String finishLearning()
    {
        learning = false;
        const auto lr = proc.learnResult();
        if (! lr.ok) return "Not enough audio heard yet - play the vocal for a few seconds while learning";
        std::array<Voice::Eq::Band, 6> bands {};
        for (size_t i = 0; i < lr.bands.size() && i < 6; ++i) bands[i] = lr.bands[i];
        { const juce::SpinLock::ScopedLockType l (sugLock); sugBands = bands; sugHp = lr.hpHz; haveSuggestion = true; }
        juce::String t; t << "range " << (lr.lowHz > 0 ? ColourName::hz (lr.lowHz) + " - " + ColourName::hz (lr.highHz) : juce::String ("not found")) << ", low cut " << ColourName::hz (lr.hpHz);
        for (auto& b : lr.bands) t << ", cut " << juce::String (b.gainDb, 1) << " dB at " << ColourName::hz (b.freq) << " (Q " << juce::String (b.q, 1) << ")";
        suggestionText.setValue (t, nullptr);
        return t;
    }
    bool hasSuggestion() const { return haveSuggestion; }
    /** Write the suggestion into free bands (or over the first bands when none are free) and the low cut. */
    void applySuggestion()
    {
        if (! haveSuggestion) return;
        std::array<Voice::Eq::Band, 6> s; float hp;
        { const juce::SpinLock::ScopedLockType l (sugLock); s = sugBands; hp = sugHp; }
        auto um = getUndoManager(); int next = 0;
        for (auto& sb : s)
        {
            if (! sb.on) continue;
            int slot = -1; for (int b = next; b < 6; ++b) if (! on[(size_t) b].get()) { slot = b; break; }
            if (slot < 0) slot = juce::jmin (5, next);
            next = slot + 1;
            on[(size_t) slot].setValue (true, um); type[(size_t) slot].setValue (0, um); mode[(size_t) slot].setValue (0, um);
            sqSetParam (bank, bandParam (slot, 0), sb.freq); sqSetParam (bank, bandParam (slot, 1), sb.gainDb); sqSetParam (bank, bandParam (slot, 2), sb.q);
        }
        if (hp > bank[hpFreq]) sqSetParam (bank, hpFreq, hp);
        preview.setValue (false, nullptr);
    }
    struct ColourName { static juce::String hz (float f) { return f >= 1000 ? juce::String (f / 1000.0f, f >= 10000 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; } };
private:
    SqSmootherN<36> smo; SqVoiceCore::Core core; std::vector<float> mono;
    juce::SpinLock sugLock; std::array<Voice::Eq::Band, 6> sugBands {}; float sugHp = 20; bool haveSuggestion = false;
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smo.apply (v, numParams, snap);
        fill (v, proc.p);
    }
public:
    /** What the controls say right now, with the suggestion when previewing (for the face). */
    Voice::Eq::Params knobParams() const
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        Voice::Eq::Params p; fill (v, p);
        p.preview = preview.get() && haveSuggestion; p.suggested = sugBands; p.suggestedHp = sugHp;
        return p;
    }
private:
    void fill (const float* v, Voice::Eq::Params& p) const
    {
        for (int b = 0; b < 6; ++b)
        {
            auto& bd = p.band[(size_t) b];
            bd.on = on[(size_t) b].get(); bd.type = juce::jlimit (0, 3, type[(size_t) b].get()); bd.mode = juce::jlimit (0, 2, mode[(size_t) b].get()); bd.harmonic = juce::jlimit (1, 16, harm[(size_t) b].get());
            bd.freq = v[bandParam (b, 0)]; bd.gainDb = v[bandParam (b, 1)]; bd.q = v[bandParam (b, 2)]; bd.thresholdDb = v[bandParam (b, 3)]; bd.rangeDb = v[bandParam (b, 4)];
            bd.expand = expand[(size_t) b].get(); bd.solo = solo[(size_t) b].get(); bd.mute = mute[(size_t) b].get();
        }
        p.hpHz = v[hpFreq]; p.lpHz = v[lpFreq]; p.hpSlope = juce::jlimit (0, 3, hpSlope.get()); p.lpSlope = juce::jlimit (0, 3, lpSlope.get());
        p.airHz = v[airFreq]; p.airDb = v[airGain]; p.tiltDb = v[tilt]; p.outputDb = v[output];
    }
};

//==============================================================================
// Hush De-Ess 2
class SqHushDeEss2Plugin : public SqColourBase
{
public:
    enum P { split, hardThr, softThr, hardRange, softRange, sensitivity, output, numParams };
    SQ_COLOUR_IDENTITY (SqHushDeEss2Plugin, "Hush De-Ess 2", "sqmHushDeEss2", "Hush 2")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "split",     "Split",          2000.0f, 12000.0f, 4500.0f, 0.5f, " Hz", 0 },
            { "hardThr",   "Hard Threshold", -60.0f, 0.0f, -30.0f, 1.0f, " dB", 1 },
            { "softThr",   "Soft Threshold", -60.0f, 0.0f, -34.0f, 1.0f, " dB", 1 },
            { "hardRange", "Hard Range",       0.0f, 24.0f, 8.0f, 1.0f, " dB", 1 },
            { "softRange", "Soft Range",       0.0f, 24.0f, 10.0f, 1.0f, " dB", 1 },
            { "sens",      "Sensitivity",      0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "output",    "Output",         -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqHushDeEss2Plugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        link.referTo (state, "linkOn", um, false); solo.referTo (state, "soloOn", nullptr, false); preview.referTo (state, "previewOn", nullptr, false);
    }
    ~SqHushDeEss2Plugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<bool> link, solo, preview;
    void resetProps (juce::UndoManager* um) override { link.setValue (false, um); solo.setValue (false, nullptr); preview.setValue (false, nullptr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, info.blockSizeSamples); }
    void deinitialise() override {}
    void reset() override { proc.prepare (sr, 512); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK juce::ScopedNoDenormals nd;
        auto& b = *fc.destBuffer; const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        read (false);
        if (preview.get() && haveSuggestion) { proc.p.hardThresholdDb = sugHard; proc.p.softThresholdDb = sugSoft; }
        proc.learning = learning.load();
        float* ch[2] = { b.getWritePointer (0, s0), b.getWritePointer (nch > 1 ? 1 : 0, s0) };
        levelIn = b.getMagnitude (0, s0, n); proc.process (ch, nch, n); levelOut = b.getMagnitude (0, s0, n);
        grDb = proc.gainReductionDb(); kindNow = proc.currentKind();
    }
    std::atomic<float> grDb { 0 }; std::atomic<int> kindNow { 0 }; std::atomic<bool> learning { false };
    Voice::DeEss::Processor proc;

    void startLearning() { proc.learnReset(); learning = true; }
    juce::String finishLearning()
    {
        learning = false;
        const auto s = proc.suggest();
        sugHard = s.hardThresholdDb; sugSoft = s.softThresholdDb; haveSuggestion = true;
        return "Hard threshold " + juce::String (sugHard, 1) + " dB, Soft threshold " + juce::String (sugSoft, 1) + " dB";
    }
    bool hasSuggestion() const { return haveSuggestion; }
    float suggestedHard() const { return sugHard; }
    float suggestedSoft() const { return sugSoft; }
    void applySuggestion() { if (! haveSuggestion) return; sqSetParam (bank, hardThr, sugHard); sqSetParam (bank, softThr, sugSoft); preview.setValue (false, nullptr); }
private:
    SqSmoother smo; std::atomic<float> sugHard { -30 }, sugSoft { -34 }; std::atomic<bool> haveSuggestion { false };
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        smo.apply (v, numParams, snap);
        fill (v, proc.p);
    }
public:
    Voice::DeEss::Params knobParams() const { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; Voice::DeEss::Params p; fill (v, p); return p; }
private:
    void fill (const float* v, Voice::DeEss::Params& p) const
    {
        p.splitHz = v[split]; p.hardThresholdDb = v[hardThr]; p.softThresholdDb = v[softThr]; p.hardRangeDb = v[hardRange]; p.softRangeDb = v[softRange];
        p.sensitivity = v[sensitivity] / 100.0f; p.outputDb = v[output]; p.solo = solo.get(); p.link = link.get();
        if (p.link) { p.softThresholdDb = p.hardThresholdDb - 4.0f; p.softRangeDb = p.hardRangeDb + 2.0f; }   // one control moves both detectors
    }
};
