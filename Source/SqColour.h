#pragma once
// Squelchmoth "colour" plug-ins - original DSP standing in for the rest of Avid's paid bundle:
//
//  Aurora Verb     8-line feedback-delay-network reverb with 8 room types, early reflections, diffusion, three-band
//                  decay (low / mid / high RT60), modulation, ducking, freeze, width, low / high cut
//  Opto Leveler    optical leveling amplifier: program-dependent two-stage release, compress / limit, HF emphasis,
//                  tube output stage, mix and a real VU meter
//  Pulse EQ        passive program EQ with the famous boost-and-cut low end, high boost with bandwidth, high
//                  attenuation, plus the mid-range peak / dip / peak section and a tube stage
//  Oil Can Delay   electrostatic oil-can echo: wobbling delay, sustain, tone, drive, echo or doubler
//  Silk Ladder     4- or 2-pole ladder low-pass with envelope follower and drive
//  Halo Ring       ring modulator with LFO                                          } the analogue-pedal set
//  Twelve Phase    4- to 12-stage phaser with resonance and stereo spread
//  Bucket Brigade  analog bucket-brigade delay with drive, filtered feedback and modulation

#include "SqApexDsp.h"
#include "SqVocalDsp.h"
#include "SqInstruments.h"

/** Shared base: parameter bank, state restore, one level meter for the faces. */
class SqColourBase : public te::Plugin, public SqDsp::BankOwner
{
public:
    explicit SqColourBase (te::PluginCreationInfo info) : te::Plugin (info) {}
    SqDsp::ParamBank bank;
    SqDsp::ParamBank& getBank() override                 { return bank; }
    bool needsConstantBufferSize() override             { return false; }
    BusLayout getBusses() const override                { return BusLayout::singleStereoInOut(); }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        for (auto ap : getAutomatableParameters()) ap->updateFromAttachedValue();
    }
    std::atomic<float> levelIn { 0 }, levelOut { 0 }, activity { 0 };
protected:
    double sr = 48000;
    static float tempoSyncedMs (te::Edit& e, int idx, float freeMs)
    {
        static const double beats[] = { 0, 0.25, 0.5, 0.75, 1.0, 2.0 };
        if (idx <= 0) return freeMs;
        const double bpm = e.tempoSequence.getTempo (0) != nullptr ? e.tempoSequence.getTempo (0)->getBpm() : 120.0;
        return (float) (60000.0 / bpm * beats[juce::jlimit (0, 5, idx)]);
    }
};

#define SQ_COLOUR_IDENTITY(Class, Name, Type, Short) \
    static const char* getPluginName()                  { return NEEDS_TRANS(Name); } \
    static constexpr const char* xmlTypeName = Type; \
    juce::String getName() const override               { return Name; } \
    juce::String getPluginType() override               { return xmlTypeName; } \
    juce::String getShortName (int) override            { return Short; } \
    juce::String getSelectableDescription() override    { return Name; }

//==============================================================================
class SqAuroraPlugin : public SqColourBase
{
public:
    enum P { preDelay, size, decay, lowDecay, highDecay, diffusion, early, tail, modDepth, modRate, width, lowCut, highCut, duck, mix, numParams };
    SQ_COLOUR_IDENTITY (SqAuroraPlugin, "Aurora Verb", "sqmAurora", "Aurora")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "preDelay",  "Pre-Delay",  0.0f, 300.0f, 20.0f, 0.5f, " ms", 0 },
            { "size",      "Size",       30.0f, 300.0f, 100.0f, 0.6f, " %", 0 },
            { "decay",     "Decay",      0.1f, 20.0f, 2.2f, 0.35f, " s", 2 },
            { "lowDecay",  "Low Decay",  0.25f, 4.0f, 1.2f, 0.5f, " x", 2 },
            { "highDecay", "High Decay", 0.1f, 1.5f, 0.5f, 0.6f, " x", 2 },
            { "diffusion", "Diffusion",  0.0f, 100.0f, 80.0f, 1.0f, " %", 0 },
            { "early",     "Early Level", -60.0f, 6.0f, -6.0f, 2.0f, " dB", 1 },
            { "tail",      "Tail Level", -60.0f, 6.0f, 0.0f, 2.0f, " dB", 1 },
            { "modDepth",  "Modulation", 0.0f, 100.0f, 20.0f, 1.0f, " %", 0 },
            { "modRate",   "Mod Rate",   0.1f, 5.0f, 0.7f, 0.5f, " Hz", 2 },
            { "width",     "Width",      0.0f, 150.0f, 100.0f, 1.0f, " %", 0 },
            { "lowCut",    "Low Cut",    20.0f, 1000.0f, 80.0f, 0.35f, " Hz", 0 },
            { "highCut",   "High Cut",   1000.0f, 20000.0f, 12000.0f, 0.35f, " Hz", 0 },
            { "duck",      "Ducking",    0.0f, 30.0f, 0.0f, 1.0f, " dB", 1 },
            { "mix",       "Mix",        0.0f, 100.0f, 30.0f, 1.0f, " %", 0 } };
        return s;
    }
    struct Room { const char* name; float sizeMul, diff, erSpread; float er[12]; };
    static const std::array<Room, 8>& rooms()
    {
        static const std::array<Room, 8> r {{
            { "Hall",      1.6f, 0.75f, 1.0f, { 0.85f, 0.7f, 0.62f, 0.55f, 0.5f, 0.42f, 0.38f, 0.33f, 0.28f, 0.24f, 0.2f, 0.16f } },
            { "Chamber",   1.0f, 0.85f, 0.7f, { 0.9f, 0.8f, 0.7f, 0.6f, 0.52f, 0.45f, 0.4f, 0.34f, 0.3f, 0.25f, 0.2f, 0.18f } },
            { "Room",      0.6f, 0.7f, 0.45f, { 1.0f, 0.85f, 0.75f, 0.6f, 0.5f, 0.4f, 0.32f, 0.25f, 0.2f, 0.15f, 0.12f, 0.1f } },
            { "Plate",     0.8f, 0.95f, 0.25f, { 0.5f, 0.5f, 0.48f, 0.46f, 0.44f, 0.42f, 0.4f, 0.38f, 0.36f, 0.34f, 0.32f, 0.3f } },
            { "Church",    2.2f, 0.7f, 1.4f, { 0.7f, 0.6f, 0.55f, 0.5f, 0.46f, 0.42f, 0.38f, 0.35f, 0.32f, 0.29f, 0.26f, 0.23f } },
            { "Cathedral", 3.0f, 0.65f, 1.8f, { 0.6f, 0.55f, 0.5f, 0.47f, 0.44f, 0.41f, 0.38f, 0.35f, 0.33f, 0.31f, 0.29f, 0.27f } },
            { "Studio",    0.45f, 0.6f, 0.35f, { 1.0f, 0.75f, 0.6f, 0.45f, 0.35f, 0.25f, 0.18f, 0.12f, 0.08f, 0.05f, 0.03f, 0.02f } },
            { "Ambience",  0.3f, 0.5f, 0.25f, { 1.0f, 0.8f, 0.6f, 0.4f, 0.3f, 0.2f, 0.12f, 0.08f, 0.05f, 0.03f, 0.02f, 0.01f } } }};
        return r;
    }
    static juce::StringArray roomNames()   { juce::StringArray s; for (auto& r : rooms()) s.add (r.name); return s; }

    SqAuroraPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        roomType.referTo (state, "roomType", um, 0);
        freeze.referTo (state, "freeze", um, false);
    }
    ~SqAuroraPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    double getTailLength() const override               { return 25.0; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    juce::CachedValue<int> roomType;
    juce::CachedValue<bool> freeze;
    void resetProps (juce::UndoManager* um) override    { roomType.setValue (0, um); freeze.setValue (false, um); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        const double sc = sr / 48000.0;
        for (int i = 0; i < N; ++i) { lines[(size_t) i].prepare ((int) (baseLen[i] * sc * 3.2 * 3.0) + 64); lines[(size_t) i].clear(); }
        for (auto& d : pre) { d.prepare ((int) (sr * 0.31) + 8); d.clear(); }
        for (auto& d : erLine) { d.prepare ((int) (sr * 0.25) + 8); d.clear(); }
        for (auto& ch : diff) for (auto& a : ch) { a.prepare ((int) (sr * 0.06) + 8); a.clear(); }
        for (auto& f : damp) f = {};
        w = 0; lfo = 0; duckEnv = 0;
        for (auto& f : outF) f.reset();
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
        const auto& room = rooms()[(size_t) juce::jlimit (0, 7, roomType.get())];
        const double sc = sr / 48000.0;
        const float sz = bank[size] / 100.0f * room.sizeMul;
        const float rt = juce::jmax (0.1f, bank[decay]);
        const bool frz = freeze.get();
        float lens[N], gMid[N], gLow[N], gHigh[N];
        for (int i = 0; i < N; ++i)
        {
            lens[i] = (float) std::round (baseLen[i] * sc * juce::jlimit (0.3f, 3.2f, sz));   // whole samples: no interpolation loss in the loop
            const float t = lens[i] / (float) sr;
            gMid[i] = frz ? 1.0f : std::pow (10.0f, -3.0f * t / rt);
            gLow[i] = frz ? 1.0f : std::pow (10.0f, -3.0f * t / (rt * bank[lowDecay]));
            gHigh[i] = frz ? 1.0f : std::pow (10.0f, -3.0f * t / (rt * bank[highDecay]));
        }
        const float lpLo = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 300.0 / sr));
        const float lpHi = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 4000.0 / sr));
        const float dg = 0.45f + 0.25f * bank[diffusion] / 100.0f * room.diff;
        const float preS = bank[preDelay] * 0.001f * (float) sr;
        const float erG = gainOrZero (bank[early]), tailG = gainOrZero (bank[tail]);
        const float modD = bank[modDepth] / 100.0f * 0.0012f * (float) sr, modInc = bank[modRate] / (float) sr;
        const float wd = bank[width] / 100.0f, mx = bank[mix] / 100.0f;
        outF[0].set (sr, bank[lowCut], 0.7); outF[1].set (sr, bank[lowCut], 0.7);
        outF[2].set (sr, bank[highCut], 0.7); outF[3].set (sr, bank[highCut], 0.7);
        const float duckDb = bank[duck];
        const float dAtt = (float) (1.0 - std::exp (-1.0 / (0.005 * sr))), dRel = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));
        static const int erTapsMs10[12] = { 43, 79, 113, 157, 197, 241, 283, 331, 373, 419, 467, 521 };   // 0.1 ms units
        float act = 0;
        for (int i = 0; i < n; ++i)
        {
            const float inL = b.getSample (0, s0 + i), inR = b.getSample (nch > 1 ? 1 : 0, s0 + i);
            const float inM = frz ? 0.0f : 0.5f * (inL + inR);
            // pre-delay
            pre[0].at (w) = frz ? 0.0f : inL; pre[1].at (w) = frz ? 0.0f : inR;
            const float pl = pre[0].cubic ((double) w - juce::jmax (2.0f, preS)), pr = pre[1].cubic ((double) w - juce::jmax (2.0f, preS));
            // early reflections (alternating sides, spread by the room)
            erLine[0].at (w) = pl; erLine[1].at (w) = pr;
            float eL = 0, eR = 0;
            for (int k = 0; k < 12; ++k)
            {
                const double d = erTapsMs10[k] * 0.0001 * sr * room.erSpread * juce::jlimit (0.4f, 2.0f, sz);
                const float v = (k & 1 ? erLine[1] : erLine[0]).cubic ((double) w - juce::jmax (2.0, d)) * room.er[k];
                if (k & 1) eR += v; else eL += v;
            }
            // input diffusion
            float dIn[2] = { pl, pr };
            static const float apMs[4] = { 4.71f, 3.59f, 12.73f, 9.31f };
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 4; ++k)
                {
                    auto& ap = diff[(size_t) c][(size_t) k];
                    const double d = apMs[k] * (c ? 1.07 : 1.0) * 0.001 * sr * juce::jlimit (0.5f, 1.6f, sz);
                    const float del = ap.cubic ((double) w - d);
                    const float v = dIn[c] + del * dg;
                    ap.at (w) = v;
                    dIn[c] = del - v * dg;
                }
            // FDN: read, damp (three bands), Householder mix, write
            lfo += modInc; if (lfo >= 1.0f) lfo -= 1.0f;
            float y[N];
            for (int k = 0; k < N; ++k)
            {
                const float m = (modD > 0.0f && (k == 1 || k == 6)) ? modD * std::sin (juce::MathConstants<float>::twoPi * (lfo + 0.3f * (float) k)) : 0.0f;
                y[k] = lines[(size_t) k].cubic ((double) w - juce::jmax (4.0f, lens[k] + m));
                auto& f = damp[(size_t) k];
                f.lo += lpLo * (y[k] - f.lo); f.hiLp += lpHi * (y[k] - f.hiLp);
                const float low = f.lo, high = y[k] - f.hiLp, mid = y[k] - low - high;
                y[k] = low * gLow[k] + mid * gMid[k] + high * gHigh[k];
            }
            float sum = 0; for (int k = 0; k < N; ++k) sum += y[k];
            const float hh = sum * (2.0f / N);
            for (int k = 0; k < N; ++k)
            {
                const float inj = (k & 1 ? dIn[1] : dIn[0]) * 0.35f;
                lines[(size_t) k].at (w) = y[k] - hh + inj;
            }
            float tL = (y[0] - y[2] + y[4] - y[6]) * 0.5f, tR = (y[1] - y[3] + y[5] - y[7]) * 0.5f;
            // width (mid / side)
            const float mid = 0.5f * (tL + tR), side = 0.5f * (tL - tR) * wd;
            tL = mid + side; tR = mid - side;
            float wetL = eL * erG * 0.35f + tL * tailG, wetR = eR * erG * 0.35f + tR * tailG;
            wetL = (float) outF[2].lowpass (outF[0].highpass (wetL)); wetR = (float) outF[3].lowpass (outF[1].highpass (wetR));
            // ducking: loud input pushes the wet signal down
            if (duckDb > 0.01f)
            {
                const float a = std::abs (inM);
                duckEnv += (a > duckEnv ? dAtt : dRel) * (a - duckEnv);
                const float over = juce::jlimit (0.0f, 1.0f, (Apex::gainToDb (duckEnv) + 36.0f) / 24.0f);
                const float g = Apex::dbToGain (-duckDb * over);
                wetL *= g; wetR *= g;
            }
            ++w;
            act = juce::jmax (act, std::abs (wetL));
            b.setSample (0, s0 + i, inL * (1.0f - mx) + wetL * mx);
            if (nch > 1) b.setSample (1, s0 + i, inR * (1.0f - mx) + wetR * mx);
        }
        activity = act;
    }

private:
    static constexpr int N = 8;
    static constexpr int baseLen[N] = { 1559, 1777, 1949, 2131, 2309, 2503, 2687, 2903 };
    std::array<SqVocal::Ring, N> lines;
    SqVocal::Ring pre[2], erLine[2];
    std::array<std::array<SqVocal::Ring, 4>, 2> diff;
    struct Damp { float lo = 0, hiLp = 0; };
    std::array<Damp, N> damp;
    Apex::Svf outF[4];
    juce::int64 w = 0; float lfo = 0, duckEnv = 0;
    static float gainOrZero (float db) { return db <= -59.5f ? 0.0f : Apex::dbToGain (db); }
};

//==============================================================================
class SqOptoPlugin : public SqColourBase
{
public:
    enum P { peakReduction, gain, emphasis, tube, mix, numParams };
    SQ_COLOUR_IDENTITY (SqOptoPlugin, "Opto Leveler", "sqmOpto", "Opto")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "peakReduction", "Peak Reduction", 0.0f, 100.0f, 35.0f, 1.0f, "", 0 },
            { "gain",          "Gain",           0.0f, 40.0f, 8.0f, 1.0f, " dB", 1 },
            { "emphasis",      "Emphasis",       0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "tube",          "Tube",           0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "mix",           "Mix",            0.0f, 100.0f, 100.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqOptoPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        limitMode.referTo (state, "limitMode", um, false);
        meterMode.referTo (state, "meterMode", um, 0);
    }
    ~SqOptoPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<bool> limitMode;
    juce::CachedValue<int> meterMode;
    std::atomic<float> grDb { 0 }, outVu { -40 };
    void resetProps (juce::UndoManager* um) override { limitMode.setValue (false, um); meterMode.setValue (0, um); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        cellFast = cellSlow = 0; memory = 0; vu = 0;
        for (auto& f : emph) f.reset();
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
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
        const float pr = bank[peakReduction] / 100.0f;
        const float scGain = Apex::dbToGain (-6.0f + pr * 42.0f);              // peak reduction = sidechain level
        const float outG = Apex::dbToGain (bank[gain] - 12.0f);
        const float em = bank[emphasis] / 100.0f, tb = bank[tube] / 100.0f, mx = bank[mix] / 100.0f;
        for (auto& f : emph) f.highShelf (sr, 3000.0, 0.7, em * 12.0);
        const bool lim = limitMode.get();
        const float ratioSlope = lim ? 0.1f : 0.33f;
        const float aAtt = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
        const float aFast = (float) (1.0 - std::exp (-1.0 / (0.060 * sr)));
        const float vuA = (float) (1.0 - std::exp (-1.0 / (0.065 * sr)));     // ~300 ms VU ballistics overall
        float minGain = 1;
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { b.getSample (0, s0 + i), b.getSample (nch > 1 ? 1 : 0, s0 + i) };
            const float sc = (float) emph[0].process (0.5 * (x[0] + x[1])) * scGain;
            const float lvl = Apex::gainToDb (std::abs (sc));
            // static curve: above 0 dB on the (gain-scaled) detector, soft knee 10 dB
            const float over = lvl + 5.0f;
            float grWant = 0;
            if (over > 0) grWant = (over < 10 ? over * over / 20.0f : over - 5.0f) * (1.0f - ratioSlope);
            // the optical cell: quick to light up, releases in two stages; the longer it works, the slower it lets go
            if (grWant > cellFast) cellFast += aAtt * (grWant - cellFast);
            else cellFast += aFast * (grWant - cellFast);
            memory = juce::jlimit (0.0f, 1.0f, memory + (grWant > 1.0f ? 0.4f : -0.25f) / (float) sr);
            const float slowSec = 0.5f + memory * 4.5f;
            const float aSlow = (float) (1.0 - std::exp (-1.0 / (slowSec * sr)));
            if (grWant > cellSlow) cellSlow += aAtt * (grWant - cellSlow);
            else cellSlow += aSlow * (grWant - cellSlow);
            const float grNow = 0.5f * cellFast + 0.5f * cellSlow;
            const float g = Apex::dbToGain (-grNow);
            minGain = juce::jmin (minGain, g);
            for (int c = 0; c < nch; ++c)
            {
                float y = x[c] * g * outG;
                if (tb > 0) { const float d = 1.0f + tb * 3.0f; y = (std::tanh (d * y + 0.08f * tb) - std::tanh (0.08f * tb)) / d; }   // asymmetric tube stage
                // DC blocker after the asymmetric tube
                const float o = y - dcX[c] + 0.9995f * dcY[c]; dcX[c] = y; dcY[c] = o;
                b.setSample (c, s0 + i, x[c] * (1.0f - mx) + o * mx);
                vu += vuA * (o * o - vu);
            }
        }
        grDb = Apex::gainToDb (minGain);
        outVu = 10.0f * std::log10 (vu + 1e-12f) + 3.0f;
        levelOut = outVu.load();
    }
private:
    float cellFast = 0, cellSlow = 0, memory = 0, vu = 0, dcX[2] {}, dcY[2] {};
    Apex::Biquad emph[2];
};

//==============================================================================
class SqPulseEqPlugin : public SqColourBase
{
public:
    enum P { lowBoost, lowAtten, highBoost, bandwidth, highAtten, midLowPeak, midDip, midHighPeak, output, tube, numParams };
    SQ_COLOUR_IDENTITY (SqPulseEqPlugin, "Pulse EQ", "sqmPulse", "Pulse")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "lowBoost",    "Low Boost",      0.0f, 10.0f, 0.0f, 1.0f, "", 1 },
            { "lowAtten",    "Low Atten",      0.0f, 10.0f, 0.0f, 1.0f, "", 1 },
            { "highBoost",   "High Boost",     0.0f, 10.0f, 0.0f, 1.0f, "", 1 },
            { "bandwidth",   "Bandwidth",      0.0f, 10.0f, 5.0f, 1.0f, "", 1 },
            { "highAtten",   "High Atten",     0.0f, 10.0f, 0.0f, 1.0f, "", 1 },
            { "midLowPeak",  "Mid Low Peak",   0.0f, 10.0f, 0.0f, 1.0f, "", 1 },
            { "midDip",      "Mid Dip",        0.0f, 10.0f, 0.0f, 1.0f, "", 1 },
            { "midHighPeak", "Mid High Peak",  0.0f, 10.0f, 0.0f, 1.0f, "", 1 },
            { "output",      "Output",         -12.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "tube",        "Tube",           0.0f, 100.0f, 25.0f, 1.0f, " %", 0 } };
        return s;
    }
    static const std::vector<float>& lowFreqs()     { static const std::vector<float> v { 20, 30, 60, 100 }; return v; }
    static const std::vector<float>& highFreqs()    { static const std::vector<float> v { 3000, 4000, 5000, 8000, 10000, 12000, 16000 }; return v; }
    static const std::vector<float>& attenFreqs()   { static const std::vector<float> v { 5000, 10000, 20000 }; return v; }
    static const std::vector<float>& midLowFreqs()  { static const std::vector<float> v { 200, 300, 500, 700, 1000 }; return v; }
    static const std::vector<float>& dipFreqs()     { static const std::vector<float> v { 200, 300, 500, 700, 1000, 1500, 2000, 3000, 4000, 5000, 7000 }; return v; }
    static const std::vector<float>& midHighFreqs() { static const std::vector<float> v { 1500, 2000, 3000, 4000, 5000 }; return v; }
    static juce::StringArray names (const std::vector<float>& v)
    {
        juce::StringArray s; for (auto f : v) s.add (f >= 1000 ? juce::String (f / 1000.0f, f >= 10000 || std::fmod (f, 1000.0f) == 0 ? 0 : 1) + "k" : juce::String ((int) f));
        return s;
    }
    SqPulseEqPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        lowFreq.referTo (state, "lowFreq", um, 2); highFreq.referTo (state, "highFreq", um, 4); attenFreq.referTo (state, "attenFreq", um, 1);
        midLowFreq.referTo (state, "midLowFreq", um, 1); dipFreq.referTo (state, "dipFreq", um, 4); midHighFreq.referTo (state, "midHighFreq", um, 2);
    }
    ~SqPulseEqPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> lowFreq, highFreq, attenFreq, midLowFreq, dipFreq, midHighFreq;
    void resetProps (juce::UndoManager* um) override
    {
        lowFreq.setValue (2, um); highFreq.setValue (4, um); attenFreq.setValue (1, um); midLowFreq.setValue (1, um); dipFreq.setValue (4, um); midHighFreq.setValue (2, um);
    }
    /** The EQ's filters for the current settings (also used by the editor to draw the curve). */
    void design (double rate, Apex::Biquad* f) const
    {
        auto pick = [] (const std::vector<float>& v, int i) { return v[(size_t) juce::jlimit (0, (int) v.size() - 1, i)]; };
        const float lf = pick (lowFreqs(), lowFreq.get());
        // The passive network. The boost is a resonant (LC) shelf, the attenuation a gentle first-order shelf whose
        // corner sits about 1.9x higher - so using both gives the famous low lift with a dip just above it. Fitted to
        // the published response of the original hardware (boost 0-13.5 dB, attenuation 0-17.5 dB).
        const double bdb = bank[lowBoost] * 1.35;
        f[0].lowShelf (rate, lf, 0.744, bdb);
        f[1].peak (rate, lf, 0.658, bdb * 0.496);
        f[2].lowShelf1 (rate, lf * 1.87, -17.5 * std::pow (bank[lowAtten] / 10.0, 1.087));
        const double q = juce::jmap ((double) bank[bandwidth], 0.0, 10.0, 2.6, 0.45);   // sharp .. broad
        f[3].peak (rate, pick (highFreqs(), highFreq.get()), q, bank[highBoost] * 1.8);
        f[4].highShelf (rate, pick (attenFreqs(), attenFreq.get()), 0.7, -bank[highAtten] * 1.6);
        f[5].peak (rate, pick (midLowFreqs(), midLowFreq.get()), 1.0, bank[midLowPeak]);
        f[6].peak (rate, pick (dipFreqs(), dipFreq.get()), 1.2, -bank[midDip]);
        f[7].peak (rate, pick (midHighFreqs(), midHighFreq.get()), 1.1, bank[midHighPeak]);
    }
    static constexpr int numFilters = 8;

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto& ch : filt) for (auto& f : ch) f.reset();
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
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
        Apex::Biquad proto[numFilters];
        design (sr, proto);
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < numFilters; ++k)
            {
                auto& f = filt[(size_t) c][(size_t) k];
                f.b0 = proto[k].b0; f.b1 = proto[k].b1; f.b2 = proto[k].b2; f.a1 = proto[k].a1; f.a2 = proto[k].a2;
            }
        const float outG = Apex::dbToGain (bank[output]), tb = bank[tube] / 100.0f;
        float pk = 0;
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
            {
                double v = b.getSample (c, s0 + i);
                for (int k = 0; k < numFilters; ++k) v = filt[(size_t) c][(size_t) k].process (v);
                float y = (float) v * outG;
                if (tb > 0) { const float d = 1.0f + tb * 2.0f; y = (std::tanh (d * y + 0.05f * tb) - std::tanh (0.05f * tb)) / d; }
                const float o = y - dcX[c] + 0.9995f * dcY[c]; dcX[c] = y; dcY[c] = o;
                b.setSample (c, s0 + i, o);
                pk = juce::jmax (pk, std::abs (o));
            }
        levelOut = pk;
    }
private:
    std::array<std::array<Apex::Biquad, numFilters>, 2> filt;
    float dcX[2] {}, dcY[2] {};
};

//==============================================================================
class SqOilCanPlugin : public SqColourBase
{
public:
    enum P { delayMs, variation, sustain, tone, drive, wobbleRate, mix, numParams };
    SQ_COLOUR_IDENTITY (SqOilCanPlugin, "Oil Can Delay", "sqmOilCan", "OilCan")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "delay",      "Delay",       60.0f, 300.0f, 180.0f, 1.0f, " ms", 0 },
            { "variation",  "Variation",   0.0f, 100.0f, 40.0f, 1.0f, " %", 0 },
            { "sustain",    "Sustain",     0.0f, 100.0f, 35.0f, 1.0f, " %", 0 },
            { "tone",       "Tone",        0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "drive",      "Drive",       0.0f, 100.0f, 20.0f, 1.0f, " %", 0 },
            { "wobbleRate", "Wobble Rate", 0.5f, 8.0f, 3.5f, 0.6f, " Hz", 2 },
            { "mix",        "Mix",         0.0f, 100.0f, 35.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqOilCanPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        mode.referTo (state, "mode", um, 0);
        sync.referTo (state, "sync", um, 0);
    }
    ~SqOilCanPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    double getTailLength() const override               { return 6.0; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    juce::CachedValue<int> mode, sync;
    std::atomic<float> canPhase { 0 };
    void resetProps (juce::UndoManager* um) override    { mode.setValue (0, um); sync.setValue (0, um); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto& d : line) { d.prepare ((int) (sr * 1.2) + 16); d.clear(); }
        for (auto& f : band) f.reset();
        w = 0; ph = 0; rnd = 0; rndT = 0;
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
        const bool dbl = mode.get() == 1;
        const float dMs = dbl ? 25.0f + bank[delayMs] * 0.05f : juce::jlimit (20.0f, 1000.0f, tempoSyncedMs (edit, sync.get(), bank[delayMs]));
        const float var = bank[variation] / 100.0f, fb = dbl ? 0.0f : bank[sustain] / 100.0f * 0.92f;
        const float centre = 450.0f * std::pow (8.0f, bank[tone] / 100.0f);
        for (auto& f : band) f.set (sr, centre, 0.6);
        const float drv = 1.0f + bank[drive] / 100.0f * 6.0f, mx = bank[mix] / 100.0f;
        const float inc = bank[wobbleRate] / (float) sr;
        float act = 0;
        for (int i = 0; i < n; ++i)
        {
            const float inL = b.getSample (0, s0 + i), inR = b.getSample (nch > 1 ? 1 : 0, s0 + i);
            ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            if (++rndT > (int) (sr * 0.05)) { rndT = 0; rndTarget = random.nextFloat() * 2.0f - 1.0f; }
            rnd += (rndTarget - rnd) * 0.0005f;
            // the oil-can rotation: a periodic wobble plus a slow random drift
            const float wob = (std::sin (juce::MathConstants<float>::twoPi * ph) * 0.7f + rnd * 0.3f) * var;
            const float dS = dMs * 0.001f * (float) sr * (1.0f + wob * (dbl ? 0.08f : 0.035f));
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float d = line[c].cubic ((double) w - juce::jmax (4.0f, dS * (c ? 1.013f : 1.0f)));
                out[c] = d;
                const float fbs = (float) band[c].bandpass (d) * 1.4f;
                const float in = (c ? inR : inL) + fbs * fb;
                line[c].at (w) = std::tanh (in * drv) / drv;
            }
            ++w;
            act = juce::jmax (act, std::abs (out[0]));
            b.setSample (0, s0 + i, inL * (1.0f - mx) + out[0] * mx);
            if (nch > 1) b.setSample (1, s0 + i, inR * (1.0f - mx) + out[1] * mx);
        }
        activity = act; canPhase = ph;
    }
private:
    SqVocal::Ring line[2]; Apex::Svf band[2];
    juce::int64 w = 0; float ph = 0, rnd = 0, rndTarget = 0; int rndT = 0; juce::Random random;
};

//==============================================================================
class SqLadderPlugin : public SqColourBase
{
public:
    enum P { cutoff, resonance, envAmount, drive, output, mix, numParams };
    SQ_COLOUR_IDENTITY (SqLadderPlugin, "Silk Ladder", "sqmLadder", "Ladder")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "cutoff",    "Cutoff",     20.0f, 20000.0f, 900.0f, 0.25f, " Hz", 0 },
            { "resonance", "Resonance",  0.0f, 100.0f, 30.0f, 1.0f, " %", 0 },
            { "envAmount", "Env Amount", -100.0f, 100.0f, 40.0f, 1.0f, " %", 0 },
            { "drive",     "Drive",      0.0f, 100.0f, 20.0f, 1.0f, " %", 0 },
            { "output",    "Output",     -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "mix",       "Mix",        0.0f, 100.0f, 100.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqLadderPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        auto um = getUndoManager();
        poles.referTo (state, "poles", um, 1); envFast.referTo (state, "envFast", um, false);
    }
    ~SqLadderPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> poles; juce::CachedValue<bool> envFast;
    std::atomic<float> liveCutoff { 900 };
    void resetProps (juce::UndoManager* um) override { poles.setValue (1, um); envFast.setValue (false, um); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto& c : st) c = {};
        env = 0;
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
        const float baseHz = bank[cutoff], res = bank[resonance] / 100.0f * 4.0f, envAmt = bank[envAmount] / 100.0f;
        const float drv = 1.0f + bank[drive] / 100.0f * 8.0f, outG = Apex::dbToGain (bank[output]), mx = bank[mix] / 100.0f;
        const bool four = poles.get() == 1;
        const float aA = (float) (1.0 - std::exp (-1.0 / ((envFast.get() ? 0.002 : 0.012) * sr))), aR = (float) (1.0 - std::exp (-1.0 / ((envFast.get() ? 0.06 : 0.3) * sr)));
        float lastHz = baseHz;
        for (int i = 0; i < n; ++i)
        {
            const float inL = b.getSample (0, s0 + i), inR = b.getSample (nch > 1 ? 1 : 0, s0 + i);
            const float a = std::abs (0.5f * (inL + inR));
            env += (a > env ? aA : aR) * (a - env);
            const float hz = juce::jlimit (20.0f, (float) (sr * 0.45), baseHz * std::pow (2.0f, envAmt * juce::jlimit (0.0f, 1.0f, env * 4.0f) * 5.0f));
            lastHz = hz;
            const float g = (float) std::tan (juce::MathConstants<double>::pi * hz / sr), G = g / (1.0f + g);
            for (int c = 0; c < 2; ++c)
            {
                auto& s = st[(size_t) c];
                const float x = (c ? inR : inL) * drv;
                // Huovilainen-style zero-delay ladder (one-pole stages with tanh), feedback from the last stage
                const float fbk = res * (four ? s.y[3] : s.y[1]);
                float u = std::tanh (x - fbk);
                for (int k = 0; k < (four ? 4 : 2); ++k)
                {
                    const float v = (u - s.z[k]) * G;
                    s.y[k] = v + s.z[k]; s.z[k] = s.y[k] + v;
                    u = std::tanh (s.y[k]);
                }
                const float y = (four ? s.y[3] : s.y[1]) * (1.0f + res * 0.7f) / drv * outG;
                const float dry = c ? inR : inL;
                if (c < nch) b.setSample (c, s0 + i, dry * (1.0f - mx) + y * mx);
            }
        }
        liveCutoff = lastHz;
    }
private:
    struct St { float z[4] {}, y[4] {}; };
    std::array<St, 2> st; float env = 0;
};

//==============================================================================
class SqRingPlugin : public SqColourBase
{
public:
    enum P { freq, lfoRate, lfoAmount, drive, output, mix, numParams };
    SQ_COLOUR_IDENTITY (SqRingPlugin, "Halo Ring", "sqmRing", "Ring")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "freq",      "Frequency",  0.5f, 4000.0f, 220.0f, 0.2f, " Hz", 1 },
            { "lfoRate",   "LFO Rate",   0.1f, 25.0f, 1.0f, 0.4f, " Hz", 2 },
            { "lfoAmount", "LFO Amount", 0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "drive",     "Drive",      0.0f, 100.0f, 0.0f, 1.0f, " %", 0 },
            { "output",    "Output",     -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1 },
            { "mix",       "Mix",        0.0f, 100.0f, 100.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqRingPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        lfoShape.referTo (state, "lfoShape", getUndoManager(), 0);
    }
    ~SqRingPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> lfoShape;
    std::atomic<float> liveFreq { 220 };
    void resetProps (juce::UndoManager* um) override { lfoShape.setValue (0, um); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; ph = lph = 0; }
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        const float f0 = bank[freq], lr = bank[lfoRate], la = bank[lfoAmount] / 100.0f, drv = 1.0f + bank[drive] / 100.0f * 5.0f;
        const float outG = Apex::dbToGain (bank[output]), mx = bank[mix] / 100.0f;
        float f = f0;
        for (int i = 0; i < n; ++i)
        {
            lph += lr / (float) sr; if (lph >= 1.0f) lph -= 1.0f;
            const float l = lfoShape.get() == 1 ? (lph < 0.5f ? 1.0f : -1.0f) : std::sin (juce::MathConstants<float>::twoPi * lph);
            f = juce::jlimit (0.1f, (float) (sr * 0.45), f0 * std::pow (2.0f, l * la * 2.0f));
            ph += (double) f / sr; ph -= std::floor (ph);
            const float car = (float) std::sin (juce::MathConstants<double>::twoPi * ph);
            for (int c = 0; c < nch; ++c)
            {
                const float x = b.getSample (c, s0 + i);
                const float y = std::tanh (x * drv) / std::sqrt (drv) * car * outG;
                b.setSample (c, s0 + i, x * (1.0f - mx) + y * mx);
            }
        }
        liveFreq = f;
    }
private:
    double ph = 0; float lph = 0;
};

//==============================================================================
class SqPhase12Plugin : public SqColourBase
{
public:
    enum P { rate, sweep, centre, resonance, spread, mix, numParams };
    SQ_COLOUR_IDENTITY (SqPhase12Plugin, "Twelve Phase", "sqmPhase12", "Phase12")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "rate",      "Rate",      0.02f, 20.0f, 0.4f, 0.35f, " Hz", 2 },
            { "sweep",     "Sweep",     0.0f, 100.0f, 70.0f, 1.0f, " %", 0 },
            { "centre",    "Centre",    100.0f, 8000.0f, 800.0f, 0.3f, " Hz", 0 },
            { "resonance", "Resonance", 0.0f, 95.0f, 50.0f, 1.0f, " %", 0 },
            { "spread",    "Stereo Spread", 0.0f, 180.0f, 90.0f, 1.0f, " deg", 0 },
            { "mix",       "Mix",       0.0f, 100.0f, 50.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqPhase12Plugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        stages.referTo (state, "stages", getUndoManager(), 3);
    }
    ~SqPhase12Plugin() override { notifyListenersOfDeletion(); bank.detach(); }
    juce::CachedValue<int> stages;   // 0: 4, 1: 6, 2: 8, 3: 12
    std::atomic<float> liveSweep { 0.5f };
    static int stageCount (int i) { const int s[] = { 4, 6, 8, 12 }; return s[juce::jlimit (0, 3, i)]; }
    void resetProps (juce::UndoManager* um) override { stages.setValue (3, um); }
    void initialise (const te::PluginInitialisationInfo& info) override { sr = info.sampleRate > 0 ? info.sampleRate : 48000.0; for (auto& c : z) for (auto& v : c) v = 0; fbs[0] = fbs[1] = 0; lph = 0; }
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        SCOPED_REALTIME_CHECK
        juce::ScopedNoDenormals noDenormals;
        auto& b = *fc.destBuffer;
        const int nch = juce::jmin (2, b.getNumChannels()), n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (nch <= 0) return;
        const int ns = stageCount (stages.get());
        const float sw = bank[sweep] / 100.0f, cen = bank[centre], fb = bank[resonance] / 100.0f, spr = bank[spread] / 360.0f, mx = bank[mix] / 100.0f;
        const float inc = bank[rate] / (float) sr;
        float last = 0;
        for (int i = 0; i < n; ++i)
        {
            lph += inc; if (lph >= 1.0f) lph -= 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float l = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * (lph + (c ? spr : 0.0f)));
                if (c == 0) last = l;
                const float hz = juce::jlimit (30.0f, (float) (sr * 0.45), cen * std::pow (2.0f, (l - 0.5f) * sw * 6.0f));
                const float t = (float) std::tan (juce::MathConstants<double>::pi * hz / sr), a = (t - 1.0f) / (t + 1.0f);
                const float x = b.getSample (juce::jmin (c, nch - 1), s0 + i);
                float v = x + fbs[c] * fb;
                for (int k = 0; k < ns; ++k)
                {
                    const float y = a * v + z[c][(size_t) k];
                    z[c][(size_t) k] = v - a * y;
                    v = y;
                }
                fbs[c] = std::tanh (v);
                if (c < nch) b.setSample (c, s0 + i, x * (1.0f - mx * 0.5f) + v * mx * 0.5f);
            }
        }
        liveSweep = last;
    }
private:
    std::array<std::array<float, 12>, 2> z {}; float fbs[2] {}, lph = 0;
};

//==============================================================================
class SqBucketPlugin : public SqColourBase
{
public:
    enum P { time, feedback, filter, drive, modRate, modDepth, mix, numParams };
    SQ_COLOUR_IDENTITY (SqBucketPlugin, "Bucket Brigade", "sqmBBD", "BBD")
    static const std::vector<SqDsp::ParamSpec>& paramSpecs()
    {
        static const std::vector<SqDsp::ParamSpec> s {
            { "time",     "Time",      20.0f, 1000.0f, 350.0f, 0.5f, " ms", 0 },
            { "feedback", "Feedback",  0.0f, 100.0f, 40.0f, 1.0f, " %", 0 },
            { "filter",   "Filter",    0.0f, 100.0f, 50.0f, 1.0f, " %", 0 },
            { "drive",    "Drive",     0.0f, 100.0f, 20.0f, 1.0f, " %", 0 },
            { "modRate",  "Mod Rate",  0.1f, 10.0f, 0.6f, 0.5f, " Hz", 2 },
            { "modDepth", "Mod Depth", 0.0f, 100.0f, 15.0f, 1.0f, " %", 0 },
            { "mix",      "Mix",       0.0f, 100.0f, 35.0f, 1.0f, " %", 0 } };
        return s;
    }
    SqBucketPlugin (te::PluginCreationInfo info) : SqColourBase (info)
    {
        bank.create (*this, paramSpecs());
        sync.referTo (state, "sync", getUndoManager(), 0);
    }
    ~SqBucketPlugin() override { notifyListenersOfDeletion(); bank.detach(); }
    double getTailLength() const override               { return 8.0; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    juce::CachedValue<int> sync;
    void resetProps (juce::UndoManager* um) override { sync.setValue (0, um); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto& d : line) { d.prepare ((int) (sr * 2.2) + 16); d.clear(); }
        for (auto& f : lp) f.reset();
        for (auto& f : lp2) f.reset();
        for (auto& f : hp) f.reset();
        w = 0; ph = 0;
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
        const float tMs = juce::jlimit (20.0f, 2000.0f, tempoSyncedMs (edit, sync.get(), bank[time]));
        // a BBD's bandwidth falls as the delay gets longer (fewer clock pulses per second)
        const float cutoff = juce::jlimit (1200.0f, 12000.0f, (2500.0f + bank[filter] * 90.0f) * std::pow (300.0f / tMs, 0.35f));
        for (int c = 0; c < 2; ++c) { lp[c].set (sr, cutoff, 0.7); lp2[c].set (sr, cutoff * 1.1, 0.6); hp[c].set (sr, 80.0, 0.7); }
        const float fb = bank[feedback] / 100.0f * 0.95f, drv = 1.0f + bank[drive] / 100.0f * 4.0f, mx = bank[mix] / 100.0f;
        const float inc = bank[modRate] / (float) sr, dep = bank[modDepth] / 100.0f * 0.004f * (float) sr;
        float act = 0;
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            const float m = dep * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * ph));
            const float inL = b.getSample (0, s0 + i), inR = b.getSample (nch > 1 ? 1 : 0, s0 + i);
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float d = line[c].cubic ((double) w - juce::jmax (4.0f, tMs * 0.001f * (float) sr + m * (c ? 1.1f : 1.0f)));
                const float y = (float) lp2[c].lowpass (lp[c].lowpass (d));
                out[c] = y;
                const float in = (c ? inR : inL) + (float) hp[c].highpass (y) * fb;
                line[c].at (w) = std::tanh (in * drv) / drv;   // the compander's soft saturation
            }
            ++w;
            act = juce::jmax (act, std::abs (out[0]));
            b.setSample (0, s0 + i, inL * (1.0f - mx) + out[0] * mx);
            if (nch > 1) b.setSample (1, s0 + i, inR * (1.0f - mx) + out[1] * mx);
        }
        activity = act;
    }
private:
    SqVocal::Ring line[2]; Apex::Svf lp[2], lp2[2], hp[2];
    juce::int64 w = 0; float ph = 0;
};
