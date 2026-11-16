#pragma once
// 1.11 Voice, second wave part 1: Siren Verb (V8), Mic Closet (V13), Larynx (V11), Exhale (V12).
// DSP: SqVoice111eDsp.h (tests/voice111e_test.cpp).

#include "SqColor111d.h"
#include "SqVoice111eDsp.h"

/** A plug-in whose Mix can be locked: factory presets then keep the Mix you set (Signature::applyPreset asks). */
struct SqMixLockable
{
    virtual ~SqMixLockable() = default;
    virtual bool mixLocked() const = 0;
    virtual const char* mixParamId() const { return "mix"; }
};

//==============================================================================
class SqSirenVerbPlugin : public SqColor111bBase, public SqMixLockable
{
public:
    enum P { size, decay, preDelay, damping, early, modRate, modDepth, width, lowCut, highCut, mix, output,
             prePitch, throat, tube, deEss, reverseTime, autoEqDepth, tone, comp, gate, duck, shimmer, delayTime, delayFeedback, delayLevel, numParams };
    SQ_COLOUR_IDENTITY (SqSirenVerbPlugin, "Siren Verb", "sqmSirenVerb", "Siren V")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "size",        "Size",          0.0f, 100.0f,   50.0f, 1.0f, " %", 0 },
            { "decay",       "Decay",         0.1f, 20.0f,     2.0f, 0.4f, " s", 2 },
            { "preDelay",    "Pre-Delay",     0.0f, 500.0f,   20.0f, 0.5f, " ms", 0 },
            { "damping",     "Damping",       0.0f, 100.0f,   50.0f, 1.0f, " %", 0 },
            { "early",       "Early",         0.0f, 100.0f,   50.0f, 1.0f, " %", 0 },
            { "modRate",     "Mod Rate",      0.05f, 5.0f,     0.6f, 0.4f, " Hz", 2 },
            { "modDepth",    "Mod Depth",     0.0f, 100.0f,   30.0f, 1.0f, " %", 0 },
            { "width",       "Width",         0.0f, 200.0f,  100.0f, 1.0f, " %", 0 },
            { "lowCut",      "Low Cut",      20.0f, 1000.0f, 120.0f, 0.4f, " Hz", 0 },
            { "highCut",     "High Cut",   1000.0f, 20000.0f, 12000.0f, 0.4f, " Hz", 0 },
            { "mix",         "Mix",           0.0f, 100.0f,   25.0f, 1.0f, " %", 0 },
            { "output",      "Output",      -24.0f, 12.0f,     0.0f, 1.0f, " dB", 1 },
            { "prePitch",    "Pre Pitch",   -24.0f, 24.0f,     0.0f, 1.0f, " st", 1 },
            { "throat",      "Throat",     -100.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "tube",        "Tube",          0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "deEss",       "De-Ess",        0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "reverseTime", "Reverse Time", 60.0f, 1000.0f, 400.0f, 0.6f, " ms", 0 },
            { "autoEqDepth", "Auto-EQ Depth", 0.0f, 24.0f,     9.0f, 1.0f, " dB", 1 },
            { "tone",        "Tone",       -100.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "comp",        "Compressor",    0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "gate",        "Gate",          0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "duck",        "Ducking",       0.0f, 100.0f,    0.0f, 1.0f, " %", 0 },
            { "shimmer",     "Shimmer",       0.0f, 90.0f,    35.0f, 1.0f, " %", 0 },
            { "delayTime",   "Echo Time",     1.0f, 2000.0f, 375.0f, 0.5f, " ms", 0 },
            { "delayFeedback","Echo Feedback",0.0f, 95.0f,    30.0f, 1.0f, " %", 0 },
            { "delayLevel",  "Echo Level",    0.0f, 100.0f,   30.0f, 1.0f, " %", 0 } };
        return s;
    }
    static juce::StringArray algoNames() { juce::StringArray a; for (int i = 0; i < SirenVerb::numAlgos; ++i) a.add (SirenVerb::algoName (i)); return a; }
    static juce::StringArray delayNames() { return { "Off", "Mono", "Ping-Pong" }; }
    static juce::StringArray shimmerNames() { return { "+7 st", "+12 st", "+19 st", "+24 st", "-12 st" }; }
    static float shimmerSt (int i) { static const float s[] = { 7, 12, 19, 24, -12 }; return s[juce::jlimit (0, 4, i)]; }
    static juce::StringArray vibeNames() { return { "Intimate", "Pop Sheen", "Ballad", "Epic", "Dark", "Retro" }; }
    SqSirenVerbPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        algo.referTo (state, "algo", um, 0); reverseOn.referTo (state, "reverseOn", um, false); autoEqOn.referTo (state, "autoEqOn", um, false);
        delayMode.referTo (state, "delayMode", um, 0); delaySync.referTo (state, "delaySync", um, 0); shimmerPitch.referTo (state, "shimmerPitch", um, 1);
        mixLockOn.referTo (state, "mixLockOn", um, false);
    }
    ~SqSirenVerbPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> algo, delayMode, delaySync, shimmerPitch; juce::CachedValue<bool> reverseOn, autoEqOn, mixLockOn;
    bool mixLocked() const override { return mixLockOn.get(); }
    void resetProps (juce::UndoManager* um) override
    {
        algo.setValue (0, um); reverseOn.setValue (false, um); autoEqOn.setValue (false, um); delayMode.setValue (0, um); delaySync.setValue (0, um); shimmerPitch.setValue (1, um);
        // mixLockOn is a user setting, not part of a preset
    }
    double getTailLength() const override { return juce::jlimit (0.5, 30.0, (double) bank[decay] * 1.5 + 0.6); }
    bool noTail() override { return false; }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        read (false);
        render (fc, false, [this] (float** ch, int nch, const float* const*, int n)
        {
            if (delaySync.get() > 0) proc.p.delayMs = (float) juce::jlimit (1.0, 2000.0, Eng::WingMod::syncBeats (juce::jlimit (0, Eng::WingMod::numSyncDivisions() - 1, delaySync.get() - 1)) * 60000.0 / bpm);
            proc.process (ch, nch, n);
        });
    }
    SirenVerb::Params knobParams() const { SirenVerb::Params p = proc.p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }

    /** Assist: a starting point from the session tempo and the voice it has heard (applied as an undoable change -
        A/B in the window to audition it against what you had). */
    void assist (int vibe, juce::UndoManager* um)
    {
        const double beat = 60.0 / juce::jmax (40.0, bpm);
        const float hz = proc.liveHz();
        auto set = [this] (int k, float v) { bank.params[(size_t) k]->setParameter (v, juce::sendNotification); };
        const float barS = (float) (beat * 4.0);
        switch (juce::jlimit (0, 5, vibe))
        {
            case 0: algo.setValue (SirenVerb::room, um);     set (decay, juce::jlimit (0.3f, 1.2f, (float) beat * 1.0f)); set (preDelay, (float) (beat * 1000.0 / 16.0)); set (mix, 14); set (size, 30); set (damping, 60); break;
            case 1: algo.setValue (SirenVerb::plate, um);    set (decay, juce::jlimit (0.8f, 2.5f, (float) beat * 2.0f)); set (preDelay, (float) (beat * 1000.0 / 8.0)); set (mix, 20); set (damping, 40); set (tone, 20); break;
            case 2: algo.setValue (SirenVerb::hall, um);     set (decay, juce::jlimit (1.8f, 4.5f, barS)); set (preDelay, (float) (beat * 1000.0 / 8.0)); set (mix, 24); set (size, 65); set (damping, 50); break;
            case 3: algo.setValue (SirenVerb::shimmer, um);  set (decay, juce::jlimit (3.0f, 8.0f, barS * 2.0f)); set (preDelay, (float) (beat * 1000.0 / 4.0)); set (mix, 28); set (size, 85); set (shimmer, 30); break;
            case 4: algo.setValue (SirenVerb::chamber, um);  set (decay, juce::jlimit (1.2f, 3.0f, (float) beat * 3.0f)); set (preDelay, 30); set (mix, 22); set (damping, 80); set (tone, -40); break;
            default: algo.setValue (SirenVerb::spring, um);  set (decay, 1.6f); set (preDelay, 10); set (mix, 22); set (damping, 55); set (tube, 35); break;
        }
        // the singer's register decides how much of the low end the tail keeps, and the Auto-EQ goes on for voices it heard
        set (lowCut, hz > 0 ? juce::jlimit (80.0f, 400.0f, hz * 0.9f) : 150.0f);
        if (hz > 0) { autoEqOn.setValue (true, um); set (autoEqDepth, 9); }
    }
    /** Dice: a random but musical setting (algorithm, size, decay, pre-delay, damping, modulation). */
    void dice (juce::UndoManager* um)
    {
        auto& r = juce::Random::getSystemRandom();
        auto set = [this] (int k, float v) { bank.params[(size_t) k]->setParameter (v, juce::sendNotification); };
        algo.setValue (r.nextInt (SirenVerb::numAlgos), um);
        set (size, 20.0f + 70.0f * r.nextFloat()); set (decay, 0.6f + 4.0f * r.nextFloat() * r.nextFloat()); set (preDelay, 80.0f * r.nextFloat());
        set (damping, 20.0f + 70.0f * r.nextFloat()); set (modDepth, 50.0f * r.nextFloat()); set (modRate, 0.2f + 1.5f * r.nextFloat()); set (early, 80.0f * r.nextFloat());
    }
    SirenVerb::Processor proc;
private:
    void fill (const float* v, SirenVerb::Params& p) const
    {
        p.algo = juce::jlimit (0, (int) SirenVerb::numAlgos - 1, algo.get());
        p.size = v[size] / 100.0f; p.decayS = v[decay]; p.preDelayMs = v[preDelay]; p.damping = v[damping] / 100.0f; p.early = v[early] / 100.0f;
        p.modRate = v[modRate]; p.modDepth = v[modDepth] / 100.0f; p.width = v[width] / 100.0f; p.lowCutHz = v[lowCut]; p.highCutHz = v[highCut];
        p.mix = v[mix] / 100.0f; p.outputDb = v[output];
        p.prePitchSt = v[prePitch]; p.preThroat = v[throat] / 100.0f; p.preTube = v[tube] / 100.0f; p.preDeEss = v[deEss] / 100.0f; p.reverseMs = v[reverseTime]; p.reverse = reverseOn.get();
        p.autoEq = autoEqOn.get(); p.autoEqDb = v[autoEqDepth]; p.tone = v[tone] / 100.0f; p.comp = v[comp] / 100.0f; p.gate = v[gate] / 100.0f; p.duck = v[duck] / 100.0f;
        p.shimmerAmt = v[shimmer] / 100.0f; p.shimmerSt = shimmerSt (shimmerPitch.get());
        p.delayMode = juce::jlimit (0, 2, delayMode.get()); p.delayMs = v[delayTime]; p.delayFb = v[delayFeedback] / 100.0f; p.delayLevel = v[delayLevel] / 100.0f;
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap);
        const float keepDelay = proc.p.delayMs; fill (v, proc.p); if (delaySync.get() > 0) proc.p.delayMs = keepDelay;
    }
};

//==============================================================================
class SqMicClosetPlugin : public SqColor111bBase
{
public:
    enum P { distance, lowCut, pad, colour, noise, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqMicClosetPlugin, "Mic Closet", "sqmMicCloset", "Mic Closet")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "distance", "Distance",  1.0f, 100.0f, 15.0f, 0.4f, " cm", 0 },
            { "lowCut",   "Low Cut",  20.0f, 300.0f, 20.0f, 0.5f, " Hz", 0 },
            { "pad",      "Pad",       0.0f, 20.0f,   0.0f, 1.0f, " dB", 0 },
            { "colour",   "Colour",    0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "noise",    "Self-Noise",0.0f, 100.0f,  0.0f, 1.0f, " %", 0 },
            { "mix",      "Mix",       0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",   "Output",  -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    static juce::StringArray micNames() { juce::StringArray a; for (auto& m : MicCloset::mics()) a.add (m.name); return a; }
    static juce::StringArray patternNames() { return { "Mic's Own", "Cardioid", "Omni", "Figure-8", "Supercardioid" }; }
    SqMicClosetPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        source.referTo (state, "source", um, 0); target.referTo (state, "target", um, 1); pattern.referTo (state, "pattern", um, 0); sourceProxOn.referTo (state, "sourceProxOn", um, true);
    }
    ~SqMicClosetPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> source, target, pattern; juce::CachedValue<bool> sourceProxOn;
    void resetProps (juce::UndoManager* um) override { source.setValue (0, um); target.setValue (1, um); pattern.setValue (0, um); sourceProxOn.setValue (true, um); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); }); }
    MicCloset::Params knobParams() const { MicCloset::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    MicCloset::Processor proc;
private:
    void fill (const float* v, MicCloset::Params& p) const
    {
        p.source = juce::jlimit (0, MicCloset::numMics() - 1, source.get()); p.target = juce::jlimit (0, MicCloset::numMics() - 1, target.get());
        p.pattern = pattern.get() <= 0 ? -1 : juce::jlimit (0, 3, pattern.get() - 1); p.sourceProx = sourceProxOn.get();
        p.distanceCm = v[distance]; p.lowCutHz = v[lowCut]; p.padDb = v[pad]; p.colour = v[colour] / 100.0f; p.noise = v[noise] / 100.0f; p.mix = v[mix] / 100.0f; p.outputDb = v[output];
    }
    void read (bool snap)
    {
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i];
        // distance and low cut move filters: no per-block smoothing (configure() runs only when they change)
        juce::ignoreUnused (snap); fill (v, proc.p);
    }
};

//==============================================================================
class SqLarynxPlugin : public SqColor111bBase
{
public:
    enum P { length, p1, p2, p3, p4, p5, p6, p7, p8, glottal, pulseWidth, breath, breathFreq, precision, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqLarynxPlugin, "Larynx", "sqmLarynx", "Larynx")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "length",     "Tract Length",  60.0f, 160.0f, 100.0f, 1.0f, " %", 0 },
            { "p1", "Glottis",  -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 }, { "p2", "Lower Throat", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "p3", "Upper Throat", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 }, { "p4", "Pharynx", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "p5", "Back Tongue", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 }, { "p6", "Mid Tongue", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "p7", "Front Tongue", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 }, { "p8", "Lips", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "glottal",    "Glottal Hardness", -100.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "pulseWidth", "Pulse Width",  10.0f, 90.0f, 50.0f, 1.0f, " %", 0 },
            { "breath",     "Breathiness",   0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "breathFreq", "Breath Freq", 500.0f, 9000.0f, 3000.0f, 0.4f, " Hz", 0 },
            { "precision",  "Precision",     0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "mix",        "Mix",           0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",     "Output",      -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqLarynxPlugin (te::PluginCreationInfo info) : SqColor111bBase (info)
    {
        bank.create (*this, paramSpecs());
        levelMatchOn.referTo (state, "levelMatchOn", getUndoManager(), true);
    }
    ~SqLarynxPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<bool> levelMatchOn;
    void resetProps (juce::UndoManager* um) override { levelMatchOn.setValue (true, um); }
    double getLatencySeconds() override { return Larynx::Channel::latency() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); }); }
    Larynx::Params knobParams() const { Larynx::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Larynx::Processor proc;
private:
    void fill (const float* v, Larynx::Params& p) const
    {
        p.length = v[length] / 100.0f; for (int i = 0; i < 8; ++i) p.shape[i] = v[p1 + i] / 100.0f;
        p.glottal = v[glottal] / 100.0f; p.pulseWidth = v[pulseWidth] / 100.0f; p.breath = v[breath] / 100.0f; p.breathHz = v[breathFreq];
        p.precision = v[precision] / 100.0f; p.mix = v[mix] / 100.0f; p.outputDb = v[output]; p.levelMatch = levelMatchOn.get();
    }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};

//==============================================================================
class SqExhalePlugin : public SqColor111bBase
{
public:
    enum P { breath, tone, consonants, body, mix, output, numParams };
    SQ_COLOUR_IDENTITY (SqExhalePlugin, "Exhale", "sqmExhale", "Exhale")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "breath",     "Breath",     -60.0f, 12.0f,   0.0f, 1.0f, " dB", 1 },
            { "tone",       "Breath Tone",-100.0f, 100.0f,  0.0f, 1.0f, " %", 0 },
            { "consonants", "Consonants",   0.0f, 100.0f,  70.0f, 1.0f, " %", 0 },
            { "body",       "Body",       -12.0f, 12.0f,   0.0f, 1.0f, " dB", 1 },
            { "mix",        "Mix",          0.0f, 100.0f, 100.0f, 1.0f, " %", 0 },
            { "output",     "Output",     -24.0f, 12.0f,   0.0f, 1.0f, " dB", 1 } };
        return s;
    }
    SqExhalePlugin (te::PluginCreationInfo info) : SqColor111bBase (info) { bank.create (*this, paramSpecs()); }
    ~SqExhalePlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    void resetProps (juce::UndoManager*) override {}
    double getLatencySeconds() override { return proc.latencySamples() / juce::jmax (1.0, sr); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; read (true); proc.prepare (sr, juce::jmax (64, info.blockSizeSamples)); }
    void deinitialise() override {}
    void reset() override { proc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override { read (false); render (fc, false, [this] (float** ch, int nch, const float* const*, int n) { proc.process (ch, nch, n); }); }
    Exhale::Params knobParams() const { Exhale::Params p; float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; fill (v, p); return p; }
    Exhale::Processor proc;
private:
    void fill (const float* v, Exhale::Params& p) const
    { p.breathDb = v[breath]; p.tone = v[tone] / 100.0f; p.consonants = v[consonants] / 100.0f; p.bodyDb = v[body]; p.mix = v[mix] / 100.0f; p.outputDb = v[output]; }
    void read (bool snap) { float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = bank[(size_t) i]; smooth (v, numParams, snap); fill (v, proc.p); }
};
