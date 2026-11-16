#pragma once
// 1.11 Voice, second wave part 3: Vox Moth (V10), Talk Wing (V15), Clean Room (V9). DSP: SqVoice111gDsp.h (tests/voice111g_test.cpp).

#include "SqVoice111f.h"
#include "SqVoice111gDsp.h"

//==============================================================================
class SqVoxMothPlugin : public SqColor111bBase, private juce::Timer
{
public:
    enum P { bands, loHz, hiHz, resonance, stretch, slide, bandShift, attack, release, smear, tilt, unvoiced, noise, pw, osc2Level, interval2, detune, glide,
             tune, retune, ring, ringHz, drive, chorus, width, xyX, xyY, mix, output, voices, lv1, numParams = lv1 + VoxMoth::curvePoints };
    SQ_COLOUR_IDENTITY (SqVoxMothPlugin, "Vox Moth", "sqmVoxMoth", "Vox Moth")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "bands",     "Bands",            4.0f,    64.0f,   16.0f, 1.0f, "", 0 },
            { "loHz",      "Low Band",        40.0f,  1000.0f,  110.0f, 0.4f, " Hz", 0 },
            { "hiHz",      "High Band",     1500.0f, 16000.0f, 7500.0f, 0.4f, " Hz", 0 },
            { "resonance", "Resonance",        0.0f,   100.0f,   40.0f, 1.0f, " %", 0 },
            { "stretch",   "Stretch",         0.25f,    2.5f,     1.0f, 1.0f, " x", 2 },
            { "slide",     "Slide",          -24.0f,   24.0f,     0.0f, 1.0f, " st", 1 },
            { "bandShift", "Band Shift",      -8.0f,    8.0f,     0.0f, 1.0f, "", 0 },
            { "attack",    "Attack",           0.2f,  100.0f,     3.0f, 0.4f, " ms", 1 },
            { "release",   "Release",          2.0f, 1000.0f,    50.0f, 0.4f, " ms", 0 },
            { "smear",     "Smear",            0.0f,  100.0f,    10.0f, 1.0f, " %", 0 },
            { "tilt",      "Tilt",           -24.0f,   24.0f,     0.0f, 1.0f, " dB", 1 },
            { "unvoiced",  "Sibilance",        0.0f,  100.0f,    60.0f, 1.0f, " %", 0 },
            { "noise",     "Noise Carrier",    0.0f,  100.0f,    35.0f, 1.0f, " %", 0 },
            { "pw",        "Pulse Width",      5.0f,   95.0f,    50.0f, 1.0f, " %", 0 },
            { "osc2",      "Osc 2 Level",    -60.0f,    0.0f,    -6.0f, 1.0f, " dB", 1 },
            { "interval2", "Osc 2 Interval", -12.0f,   12.0f,     0.0f, 1.0f, " st", 0 },
            { "detune",    "Detune",           0.0f,   50.0f,     7.0f, 1.0f, " ct", 0 },
            { "glide",     "Glide",            0.0f, 1000.0f,    30.0f, 0.4f, " ms", 0 },
            { "tune",      "Tune Amount",      0.0f,  100.0f,   100.0f, 1.0f, " %", 0 },
            { "retune",    "Retune Speed",     0.0f,  400.0f,    15.0f, 0.5f, " ms", 0 },
            { "ring",      "Ring Mod",         0.0f,  100.0f,     0.0f, 1.0f, " %", 0 },
            { "ringHz",    "Ring Freq",       20.0f, 2000.0f,   440.0f, 0.3f, " Hz", 0 },
            { "drive",     "Drive",            0.0f,  100.0f,     0.0f, 1.0f, " %", 0 },
            { "chorus",    "Chorus",           0.0f,  100.0f,     0.0f, 1.0f, " %", 0 },
            { "width",     "Width",            0.0f,  200.0f,   100.0f, 1.0f, " %", 0 },
            { "xyX",       "X",                0.0f,  100.0f,    50.0f, 1.0f, " %", 0 },
            { "xyY",       "Y",                0.0f,  100.0f,    50.0f, 1.0f, " %", 0 },
            { "mix",       "Mix",              0.0f,  100.0f,   100.0f, 1.0f, " %", 0 },
            { "output",    "Output",         -24.0f,   24.0f,     0.0f, 1.0f, " dB", 1 },
            { "voices",    "Voices",           1.0f,    8.0f,     8.0f, 1.0f, "", 0 },
            { "lv1",  "Level 1",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv2",  "Level 2",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "lv3",  "Level 3",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv4",  "Level 4",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "lv5",  "Level 5",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv6",  "Level 6",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "lv7",  "Level 7",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv8",  "Level 8",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "lv9",  "Level 9",  -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv10", "Level 10", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "lv11", "Level 11", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv12", "Level 12", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "lv13", "Level 13", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv14", "Level 14", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "lv15", "Level 15", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 }, { "lv16", "Level 16", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray carrierNames() { juce::StringArray a; for (int i = 0; i < VoxMoth::numCarriers; ++i) a.add (VoxMoth::carrierName (i)); return a; }
    static juce::StringArray shapeNames()   { juce::StringArray a; for (int i = 0; i < VoxMoth::numShapes; ++i) a.add (VoxMoth::shapeName (i)); return a; }
    static juce::StringArray waveNames()    { return { "Saw", "Pulse" }; }
    static juce::StringArray octaveNames()  { return { "-2 oct", "-1 oct", "0", "+1 oct", "+2 oct" }; }
    static juce::StringArray voicingNames() { juce::StringArray a; for (auto& v : VoxMoth::voicings()) a.add (v.name); return a; }
    static juce::StringArray driveNames()   { return SqMutantPlugin::driveNames(); }
    SqVoxMothPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        carrier.referTo (state, "carrier", um, 0); shape.referTo (state, "shape", um, 0); wave1.referTo (state, "wave1", um, 0); wave2.referTo (state, "wave2", um, 1);
        octave2.referTo (state, "octave2", um, 2); driveVoicing.referTo (state, "driveVoicing", um, 0); voicing.referTo (state, "voicing", um, 1);
        autoKey.referTo (state, "autoKeyOn", um, true); keyRoot.referTo (state, "keyRoot", um, 0); scale.referTo (state, "scale", um, 1); voiceType.referTo (state, "voiceType", um, 1);
        startTimerHz (4);
    }
    ~SqVoxMothPlugin() override { stopTimer(); notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> carrier, shape, wave1, wave2, octave2, driveVoicing, voicing, keyRoot, scale, voiceType; juce::CachedValue<bool> autoKey;
    void resetProps (juce::UndoManager* um) override
    {
        carrier.setValue (0, um); shape.setValue (0, um); wave1.setValue (0, um); wave2.setValue (1, um); octave2.setValue (2, um); driveVoicing.setValue (0, um); voicing.setValue (1, um);
        autoKey.setValue (true, um); keyRoot.setValue (0, um); scale.setValue (1, um); voiceType.setValue (1, um);
    }
    bool takesMidiInput() override { return true; }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; sessionKey.poll (edit); read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); held.fill (false); }
    void deinitialise() override {}
    void reset() override { proc.reset(); held.fill (false); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) held.fill (false);
            for (auto& m : *fc.bufferForMidiMessages)
            { if (m.isNoteOn()) held[(size_t) m.getNoteNumber()] = true; else if (m.isNoteOff()) held[(size_t) m.getNoteNumber()] = false; else if (m.isAllNotesOff() || m.isAllSoundOff()) held.fill (false); }
        }
        proc.setHeld (held);
        const bool keyed = getSidechainSourceID().isValid();
        render (fc, keyed, [this] (float** ch, int nch, const float* const* key, int n) { proc.process (ch, nch, key, n); liveHz = proc.inputHz(); });
    }
    /** Loads one of the 20 voicings: bands, edges, filter shape, speed, unvoiced and the carrier oscillators. */
    void loadVoicing (int idx, juce::UndoManager* um)
    {
        idx = juce::jlimit (0, 19, idx);
        VoxMoth::Params q; VoxMoth::Processor::applyVoicing (q, idx);
        auto setP = [this] (int i, float v) { if (auto ap = bank.params[(size_t) i]) ap->setParameter (ap->getValueRange().clipValue (v), juce::sendNotification); };
        setP (bands, (float) q.bands); setP (loHz, q.loHz); setP (hiHz, q.hiHz); setP (resonance, q.resonance); setP (attack, q.attackMs); setP (release, q.releaseMs);
        setP (smear, q.smear * 100.0f); setP (unvoiced, q.unvoiced * 100.0f); setP (noise, q.noiseCarrier * 100.0f); setP (pw, q.pw * 100.0f); setP (osc2Level, q.osc2Db); setP (detune, q.detuneCents);
        shape.setValue (q.shape, um); wave1.setValue (q.wave1, um); wave2.setValue (q.wave2, um); voicing.setValue (idx, um);
    }
    /** Dice: a new sound around the current one (band count, edges, resonance, slide, speed, oscillators). */
    void dice (juce::UndoManager* um)
    {
        auto& r = juce::Random::getSystemRandom();
        auto setP = [this] (int i, float v) { if (auto ap = bank.params[(size_t) i]) ap->setParameter (ap->getValueRange().clipValue (v), juce::sendNotification); };
        static const int bandSet[] = { 8, 10, 12, 16, 20, 24, 32, 40 };
        setP (bands, (float) bandSet[r.nextInt (8)]); setP (resonance, 15.0f + 70.0f * r.nextFloat()); setP (slide, (float) (r.nextInt (13) - 6));
        setP (release, 25.0f + 150.0f * r.nextFloat()); setP (smear, 30.0f * r.nextFloat()); setP (pw, 15.0f + 70.0f * r.nextFloat()); setP (detune, 3.0f + 15.0f * r.nextFloat());
        setP (interval2, (float) (std::array<int, 5> { 0, 0, 7, 12, -12 })[(size_t) r.nextInt (5)]);
        shape.setValue (r.nextInt (VoxMoth::numShapes), um); wave1.setValue (r.nextInt (2), um); wave2.setValue (r.nextInt (2), um);
    }
    VoxMoth::Params knobParams() const { VoxMoth::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    VoxMoth::Processor proc; SqSessionKey sessionKey; std::atomic<float> liveHz { 0 };
private:
    std::array<bool, 128> held {};
    void timerCallback() override { sessionKey.poll (edit); }
    void fill (const float* v, VoxMoth::Params& p) const
    {
        p.bands = juce::jlimit (4, 64, juce::roundToInt (v[bands])); p.loHz = v[loHz]; p.hiHz = v[hiHz]; p.resonance = v[resonance]; p.stretch = v[stretch]; p.slideSt = v[slide];
        p.bandShift = juce::roundToInt (v[bandShift]); p.attackMs = v[attack]; p.releaseMs = v[release]; p.smear = v[smear] / 100.0f; p.tiltDb = v[tilt];
        for (int i = 0; i < VoxMoth::curvePoints; ++i) p.curve[(size_t) i] = v[lv1 + i];
        p.unvoiced = v[unvoiced] / 100.0f; p.noiseCarrier = v[noise] / 100.0f;
        p.carrier = juce::jlimit (0, VoxMoth::numCarriers - 1, carrier.get()); p.voices = juce::jlimit (1, 8, juce::roundToInt (v[voices]));
        p.shape = juce::jlimit (0, VoxMoth::numShapes - 1, shape.get()); p.wave1 = juce::jlimit (0, 1, wave1.get()); p.wave2 = juce::jlimit (0, 1, wave2.get()); p.octave2 = juce::jlimit (0, 4, octave2.get()) - 2;
        p.pw = v[pw] / 100.0f; p.osc2Db = v[osc2Level]; p.interval2 = v[interval2]; p.detuneCents = v[detune]; p.glideMs = v[glide];
        p.tune = v[tune] / 100.0f; p.retuneMs = v[retune];
        const bool sess = autoKey.get() && sessionKey.known.load();
        p.keyRoot = sess ? sessionKey.root.load() : juce::jlimit (0, 11, keyRoot.get());
        p.scaleMask = Masquerade::Processor::rotate (SqSessionKey::relMask (sess ? sessionKey.scale.load() : scale.get()), p.keyRoot);
        p.voiceType = juce::jlimit (0, 5, voiceType.get());
        p.ring = v[ring] / 100.0f; p.ringHz = v[ringHz]; p.drive = v[drive] / 100.0f; p.driveVoicing = juce::jlimit (0, (int) Eng::AnalogStyle::numVoicings, driveVoicing.get()); p.chorus = v[chorus] / 100.0f;
        p.width = v[width] / 100.0f; p.xyX = v[xyX] / 100.0f; p.xyY = v[xyY] / 100.0f; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        static const int sm_[] = { slide, ring, drive, chorus, width, xyX, xyY, mix, output };
        float t[9]; for (int i = 0; i < 9; ++i) t[i] = v[sm_[i]]; smooth (t, 9, snap); for (int i = 0; i < 9; ++i) v[sm_[i]] = t[i];
        fill (v, proc.p);
    }
};

//==============================================================================
class SqTalkWingPlugin : public SqColor111bBase, private juce::AsyncUpdater
{
public:
    enum P { mouth, formant, precision, articulation, follow, sibilance, gate, tube, brightness, buzzTone, glide, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqTalkWingPlugin, "Talk Wing", "sqmTalkWing", "Talk Wing")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "mouth",        "Mouth",          0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "formant",      "Formant",      -12.0f,  12.0f,   0.0f, 1.0f, " st", 1 },
            { "precision",    "Precision",      0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "articulation", "Articulation",   0.0f, 100.0f,  30.0f, 1.0f, " %", 0 },
            { "follow",       "Follow",         0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "sibilance",    "Sibilance",      0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "gate",         "Gate",         -90.0f, -20.0f, -60.0f, 1.0f, " dB", 0 },
            { "tube",         "Tube",           0.0f, 100.0f,  40.0f, 1.0f, " %", 0 },
            { "brightness",   "Brightness",    -6.0f,   6.0f,   0.0f, 1.0f, " dB/oct", 1 },
            { "buzzTone",     "Buzz Tone",      0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "glide",        "Buzz Glide",     0.0f, 300.0f,  25.0f, 0.5f, " ms", 0 },
            { "mix",          "Mix",            0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",       "Output",       -24.0f,  24.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray sourceNames() { juce::StringArray a; for (int i = 0; i < TalkWing::numSources; ++i) a.add (TalkWing::sourceName (i)); return a; }
    static juce::StringArray engineNames() { return { "Live", "Studio" }; }
    static juce::StringArray octaveNames() { return { "-2 oct", "-1 oct", "0", "+1 oct" }; }
    SqTalkWingPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        source.referTo (state, "source", um, 0); engine.referTo (state, "engine", um, 1); voiceType.referTo (state, "voiceType", um, 1); buzzOctave.referTo (state, "buzzOctave", um, 2);
    }
    ~SqTalkWingPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> source, engine, voiceType, buzzOctave;
    void resetProps (juce::UndoManager* um) override { source.setValue (0, um); engine.setValue (1, um); voiceType.setValue (1, um); buzzOctave.setValue (2, um); }
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override { te::Plugin::getChannelNames (ins, outs); addKeyChannels (ins); }
    double getLatencySeconds() override { return (double) TalkWing::Processor::latencyFor (juce::jlimit (0, 1, engine.get())) / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        const bool keyed = getSidechainSourceID().isValid();
        render (fc, keyed, [this] (float** ch, int nch, const float* const* key, int n) { proc.process (ch, nch, key, n); });
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && id == juce::Identifier ("engine")) triggerAsyncUpdate();
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    TalkWing::Params knobParams() const { TalkWing::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    TalkWing::Processor proc;
private:
    void handleAsyncUpdate() override { edit.restartPlayback(); }
    void fill (const float* v, TalkWing::Params& p) const
    {
        p.source = juce::jlimit (0, TalkWing::numSources - 1, source.get()); p.engine = juce::jlimit (0, 1, engine.get()); p.voiceType = juce::jlimit (0, 5, voiceType.get());
        p.buzzOctave = juce::jlimit (0, 3, buzzOctave.get()) - 2;
        p.mouth = v[mouth] / 100.0f; p.formantSt = v[formant]; p.precision = v[precision] / 100.0f; p.articulation = v[articulation] / 100.0f; p.follow = v[follow] / 100.0f;
        p.sibilance = v[sibilance] / 100.0f; p.gateDb = v[gate]; p.tube = v[tube] / 100.0f; p.brightness = v[brightness]; p.buzzTone = v[buzzTone] / 100.0f; p.glideMs = v[glide];
        p.mix = v[mix] / 100.0f; p.outputDb = v[output];
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};

//==============================================================================
class SqCleanRoomPlugin : public SqColor111bBase, private juce::Timer, private juce::AsyncUpdater
{
public:
    enum P { clean, hum, buzz, hiss, noise, room, clicks, roomDecay, guard, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqCleanRoomPlugin, "Clean Room", "sqmCleanRoom", "Clean Room")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "clean",     "Clean",        0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "hum",       "Hum",          0.0f, 100.0f,  80.0f, 1.0f, " %", 0 },
            { "buzz",      "Buzz",         0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "hiss",      "Hiss",         0.0f, 100.0f,  60.0f, 1.0f, " %", 0 },
            { "noise",     "Noise",        0.0f, 100.0f,  70.0f, 1.0f, " %", 0 },
            { "room",      "Room",         0.0f, 100.0f,  40.0f, 1.0f, " %", 0 },
            { "clicks",    "Clicks",       0.0f, 100.0f,  50.0f, 1.0f, " %", 0 },
            { "roomDecay", "Room Decay",   0.15f,  4.0f,   0.8f, 0.5f, " s", 2 },
            { "guard",     "Artefact Guard", 0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "mix",       "Mix",          0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",    "Output",     -24.0f,  24.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray engineNames() { return { "Live", "Studio" }; }
    static juce::StringArray humNames()    { return { "Auto", "50 Hz", "60 Hz" }; }
    SqCleanRoomPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        engine.referTo (state, "engine", um, 1); humMode.referTo (state, "humMode", um, 0); residual.referTo (state, "residualOn", nullptr, false); profile.referTo (state, "profile", um, juce::String());
        startTimerHz (4);
    }
    ~SqCleanRoomPlugin() override { stopTimer(); cancelPendingUpdate(); notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> engine, humMode; juce::CachedValue<bool> residual; juce::CachedValue<juce::String> profile;
    void resetProps (juce::UndoManager* um) override { engine.setValue (1, um); humMode.setValue (0, um); residual.setValue (false, nullptr); }
    double getLatencySeconds() override { return (double) CleanRoom::Processor::latencyFor (juce::jlimit (0, 1, engine.get())) / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples));
        if (profile.get().isNotEmpty()) proc.setProfile (profile.get());
    }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); }); }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (v == state && id == juce::Identifier ("engine")) triggerAsyncUpdate();
        if (v == state && id == juce::Identifier ("profile")) { if (profile.get().isEmpty()) proc.clearProfile(); else if (profile.get() != proc.profileText()) proc.setProfile (profile.get()); }
        SqColor111bBase::valueTreePropertyChanged (v, id);
    }
    void startLearning() { proc.startLearning(); wasLearning = true; }
    void stopLearning()  { proc.stopLearning(); }
    void clearProfile (juce::UndoManager* um) { profile.setValue (juce::String(), um); proc.clearProfile(); }
    bool isLearning() const { return proc.learning(); }
    CleanRoom::Params knobParams() const { CleanRoom::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    CleanRoom::Processor proc;
private:
    bool wasLearning = false;
    void handleAsyncUpdate() override { edit.restartPlayback(); }
    void timerCallback() override
    {
        // a profile learned on the audio thread is kept with the song once learning stops
        if (wasLearning && ! proc.learning() && proc.hasProfile()) { wasLearning = false; const auto t = proc.profileText(); if (t != profile.get()) profile.setValue (t, getUndoManager()); }
    }
    void fill (const float* v, CleanRoom::Params& p) const
    {
        p.engine = juce::jlimit (0, 1, engine.get()); p.humMode = juce::jlimit (0, 2, humMode.get()); p.residual = residual.get();
        p.clean = v[clean] / 100.0f; p.hum = v[hum] / 100.0f; p.buzz = v[buzz] / 100.0f; p.hiss = v[hiss] / 100.0f; p.noise = v[noise] / 100.0f; p.room = v[room] / 100.0f;
        p.clicks = v[clicks] / 100.0f; p.roomDecay = v[roomDecay]; p.guard = v[guard] / 100.0f; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};
