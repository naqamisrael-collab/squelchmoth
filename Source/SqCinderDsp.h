#pragma once
// Cinder (super prompt 1.11, C1): the saturation engine. Standard C++ only (measured in tests/engine111_test.cpp).
//
// Ten models, each a measured chain:
//   input stage (transformer high-pass with its resonance)  ->  drive EQ (pre-emphasis)  ->  oversampled nonlinear
//   stage(s)  ->  de-emphasis  ->  output stage colour and bandwidth
// plus the controls: Drive (-12..+36 dB, Punish +20), Bias, Dynamics, Low Cut with Thump, Tone tilt, High Cut
// (6 dB/oct or 30 dB/oct Steep), Output, Auto gain, Mix, Model Blend (two models in parallel), Mid/Side, sidechain-
// ducked drive, Delta (only what the saturation added) and 1x-16x oversampling, minimum or linear phase.
//
// The nonlinear stages are small-signal unity (SqEngine111Dsp.h) and Cinder divides by the drive gain after them, so
// a quiet signal passes at unity and only what saturates changes. Auto gain then matches the energy going into the
// nonlinear stages to the energy coming out (slow, gated), so it compensates the saturation and never the EQ.

#include "SqEngine111Dsp.h"

namespace Cinder
{
    using namespace Eng;

    enum ModelId { tapePreamp, britishChannel, germaniumA, triodeBox, pentodeBox, transformerIron, opAmpRail, diodeFuzz, tapeHysteresis, broadcastAmp, numModels };

    struct ModelDef
    {
        const char* name; const char* archetype;
        int nlA; float biasA, charA;                 // first nonlinear stage
        int nlB; float biasB, charB, gainBDb;        // optional second stage (-1 = none)
        float inHpHz, inHpQ;                         // input transformer high-pass and its resonance
        float emLowHz, emLowDb, emMidHz, emMidDb, emHighHz, emHighDb;   // drive EQ (removed again after the stage)
        float outLowHz, outLowDb, outHighHz, outHighDb, bandwidthHz;    // output stage colour (stays)
        float slewFsPerMs;                           // op-amp slew limit, full scale per ms (0 = none)
        bool limiter;                                // broadcast amp: fast program-dependent limiter before the stage
        float calibDb;                               // stage input gain at Drive 0 for a "0 VU" (-18 dBFS) signal
    };

    inline const ModelDef& model (int m)
    {
        using namespace Nonlinear;
        static const ModelDef d[] = {
            { "Tape Preamp", "1950s tube tape-machine preamp: ultra-smooth, second-harmonic glow, soft top",
              glow, 0.25f, 0.35f,   tape, 0.0f, 0.3f, -6.0f,     22, 0.71f,   0, 0,  0, 0,    5000, 2.0f,    100, 0.5f, 12000, -1.0f, 18000,  0, false, 4.0f },
            { "British Channel", "transformer-coupled silicon console channel: beefy lows, airy top",
              transformer, 0.1f, 0.5f, console, 0.2f, 0.4f, 0.0f, 30, 1.0f,    120, 3.0f,  0, 0,  0, 0.0f,      90, 1.5f, 12000, 1.5f, 22000,  0, false, 3.0f },
            { "Germanium Class-A", "germanium class-A input module: weight and focused mids",
              germanium, 0.3f, 0.0f, -1, 0, 0, 0,                24, 0.8f,    150, 1.0f,  1200, 3.0f,  0, 0.0f,   100, 0.0f, 9000, -1.5f, 16000,   0, false, 2.0f },
            { "Triode Box", "triode tube distortion box: even harmonics that bloom",
              glow, 0.45f, 0.6f,  -1, 0, 0, 0,                   35, 0.71f,   0, 0,  0, 0,    2000, 1.0f,      100, 0.0f, 8000, -2.0f, 14000,   0, false, 6.0f },
            { "Pentode Box", "the same box in pentode: odd harmonics, forward growl",
              pentode, 0.05f, 0.7f, -1, 0, 0, 0,                 35, 0.71f,   0, 0,  0, 0,    2500, 2.0f,      100, 0.0f, 9000, -2.0f, 13000,   0, false, 7.0f },
            { "Transformer Iron", "heavy output transformer: hysteresis density in the low mids",
              transformer, 0.3f, 0.9f, -1, 0, 0, 0,              25, 1.2f,    200, 4.0f,  0, 0,  0, 0.0f,       70, 1.0f, 15000, 0.0f, 20000,   0, false, 4.0f },
            { "Op-Amp Rail", "modern op-amp line stage driven into its rails: hard, slew-limited",
              brick, 0.0f, 0.3f,  -1, 0, 0, 0,                   10, 0.71f,   0, 0,  0, 0,    0, 0.0f,          100, 0.0f, 16000, 0.0f, 22000,  60, false, 2.0f },
            { "Diode Fuzz", "silicon diode fuzz: asymmetric, tight lows, buzzing top",
              diode, 0.7f, 0.7f,  -1, 0, 0, 0,                   60, 0.71f,   300, -3.0f, 0, 0,  1800, 2.0f,   100, 0.0f, 6000, -4.0f, 10000,   0, false, 10.0f },
            { "Tape Hysteresis", "magnetic tape: hysteresis, head bump, highs saturate first",
              tape, 0.15f, 0.75f, -1, 0, 0, 0,                   18, 0.71f,   0, 0,  0, 0,    8000, 3.0f,       60, 2.5f, 14000, -2.0f, 17000,  0, false, 3.0f },
            { "Broadcast Limiter Amp", "tube broadcast limiting amplifier: fast program-dependent limiting into a warm stage",
              glow, 0.2f, 0.3f,   -1, 0, 0, 0,                   28, 0.71f,   0, 0,  0, 0,    0, 0.0f,          100, 0.0f, 15000, -0.5f, 19000,  0, true, 0.0f },
        };
        return d[clamp (m, 0, (int) numModels - 1)];
    }
    inline const char* modelName (int m)   { return model (m).name; }

    enum MsMode { stereo, midSide, midOnly, sideOnly, numMsModes };
    inline const char* msName (int m)   { static const char* n[] = { "Stereo", "Mid/Side", "Mid only", "Side only" }; return n[clamp (m, 0, 3)]; }

    struct Params
    {
        int modelA = triodeBox, modelB = tapePreamp; float blend = 0;      // 0..1
        float driveDb = 6, biasAmt = 0, dynamics = 0.3f;                   // bias -1..1, dynamics 0..1
        bool punish = false;
        float lowCutHz = 20, thumpDb = 0, toneDb = 0, highCutHz = 22000; bool steep = false;
        float outputDb = 0, mix = 1; bool autoGain = true;
        int ms = stereo; float duckDb = 0; bool keyOn = false;
        int oversampleLog2 = 2; bool linearPhase = false; bool delta = false;
    };

    /** One model's per-channel state. */
    struct ModelChain
    {
        Biquad inHp, emLow, emMid, emHigh, deLow, deMid, deHigh, outLow, outHigh; OnePole bw;
        Nonlinear::Stage a, b; float slewPrev = 0, slewStep = 1;
        int id = -1;
        void prepare (int m, double sr, int osFactor)
        {
            id = m; const auto& d = model (m);
            inHp.set (Biquad::highpass, sr, d.inHpHz, d.inHpQ);
            if (d.emLowDb != 0)  { emLow.set (Biquad::lowShelf, sr, d.emLowHz, 0.7, d.emLowDb);  deLow.set (Biquad::lowShelf, sr, d.emLowHz, 0.7, -d.emLowDb); } else { emLow.setFlat(); deLow.setFlat(); }
            if (d.emMidDb != 0)  { emMid.set (Biquad::peak, sr, d.emMidHz, 0.9, d.emMidDb);       deMid.set (Biquad::peak, sr, d.emMidHz, 0.9, -d.emMidDb); }      else { emMid.setFlat(); deMid.setFlat(); }
            if (d.emHighDb != 0) { emHigh.set (Biquad::highShelf, sr, d.emHighHz, 0.7, d.emHighDb); deHigh.set (Biquad::highShelf, sr, d.emHighHz, 0.7, -d.emHighDb); } else { emHigh.setFlat(); deHigh.setFlat(); }
            if (d.outLowDb != 0)  outLow.set (Biquad::lowShelf, sr, d.outLowHz, 0.8, d.outLowDb);    else outLow.setFlat();
            if (d.outHighDb != 0) outHigh.set (Biquad::highShelf, sr, d.outHighHz, 0.7, d.outHighDb); else outHigh.setFlat();
            bw.set (sr, std::min ((double) d.bandwidthHz, sr * 0.45));
            const double osr = sr * osFactor;
            a.model = d.nlA; a.bias = d.biasA; a.character = d.charA; a.prepare (osr);
            if (d.nlB >= 0) { b.model = d.nlB; b.bias = d.biasB; b.character = d.charB; b.prepare (osr); }
            slewStep = d.slewFsPerMs > 0 ? (float) (d.slewFsPerMs * 1000.0 / osr) : 1.0e9f;
            reset();
        }
        void reset() { inHp.reset(); emLow.reset(); emMid.reset(); emHigh.reset(); deLow.reset(); deMid.reset(); deHigh.reset(); outLow.reset(); outHigh.reset(); bw.reset(); a.reset(); b.reset(); slewPrev = 0; }
        inline float pre (float x)  { return emHigh.process (emMid.process (emLow.process (inHp.process (x)))); }
        inline float post (float y) { return bw.lp (outHigh.process (outLow.process (deHigh.process (deMid.process (deLow.process (y)))))); }
        /** Oversampled: x already emphasised; g = drive gain. Returns the stage output divided back by g. */
        inline float nl (float x, float g, float biasAdd)
        {
            const auto& d = model (id);
            float u = x * g;
            if (slewStep < 1.0e8f)
            {
                // soft slew limit: the step saturates smoothly instead of clamping (no added fold-back)
                const float lim = slewStep * std::max (1.0f, g * 0.25f);
                u = slewPrev + lim * (float) Nonlinear::softAlgD ((u - slewPrev) / lim);
                slewPrev = u;
            }
            a.bias = clamp (d.biasA + biasAdd, -1.0f, 1.0f);
            float y = a.process (u);
            if (d.nlB >= 0) { const float gb = dbToGain (d.gainBDb); b.bias = clamp (d.biasB + biasAdd * 0.5f, -1.0f, 1.0f); y = b.process (y * gb) / gb; }
            return y / g;
        }
    };

    /** The whole processor for up to 2 channels (+ optional 2 key channels). */
    class Processor
    {
    public:
        void prepare (double sampleRate, int maxBlockSize)
        {
            sr = sampleRate; maxBlock = clamp (maxBlockSize, 16, 4096);   // larger host blocks are split
            for (int c = 0; c < 2; ++c) { work[c].assign ((size_t) maxBlock, 0.0f); dryW[c].assign ((size_t) maxBlock, 0.0f); gBuf[c].assign ((size_t) maxBlock + 1, 1.0f); }
            os.prepare (maxBlock, p.oversampleLog2, sr, p.linearPhase ? Oversampler::linearPhase : Oversampler::minimumPhase);
            osDry.prepare (maxBlock, p.oversampleLog2, sr, p.linearPhase ? Oversampler::linearPhase : Oversampler::minimumPhase);
            osB.prepare (maxBlock, p.oversampleLog2, sr, p.linearPhase ? Oversampler::linearPhase : Oversampler::minimumPhase);
            limA = (float) (1.0 - std::exp (-1.0 / (0.0002 * sr)));
            envA = (float) (1.0 - std::exp (-1.0 / (0.010 * sr))); envR = (float) (1.0 - std::exp (-1.0 / (0.150 * sr)));
            keyA = (float) (1.0 - std::exp (-1.0 / (0.005 * sr))); keyR = (float) (1.0 - std::exp (-1.0 / (0.120 * sr)));
            for (int m = 0; m < numModels; ++m) { const auto& d = model (m); Nonlinear::warm (d.nlA, d.charA); if (d.nlB >= 0) Nonlinear::warm (d.nlB, d.charB); }
            applied = {}; applied.modelA = -1; configure (true);
            reset();
        }
        void reset()
        {
            os.reset(); osDry.reset(); osB.reset();
            for (int c = 0; c < 2; ++c) { chA[c].reset(); chB[c].reset(); lowCut[c].reset(); tiltLo[c].reset(); tiltHi[c].reset(); hc1[c].reset(); hc2[c].reset(); hc3[c].reset(); limGain[c] = 1; limEnv[c] = 0; }
            env = 0; keyEnv = 0; eIn = eOut = 1.0e-6f; agGain = 1; gPrev = 1;
        }
        Params p;

        /** Applies parameter changes (call at the start of each block). Filter coefficient updates are cheap;
            a model change re-prepares that model's stages (no allocation). */
        void configure (bool force = false)
        {
            const int f = clamp (p.oversampleLog2, 0, Oversampler::maxStages);
            const int ph = p.linearPhase ? Oversampler::linearPhase : Oversampler::minimumPhase;
            const bool osChanged = f != os.factorLog2() || ph != os.phase();
            if (osChanged) { os.setFactorLog2 (f, ph); osDry.setFactorLog2 (f, ph); osB.setFactorLog2 (f, ph); }
            if (force || osChanged || p.modelA != applied.modelA) for (auto& c : chA) c.prepare (p.modelA, sr, os.factor());
            if (force || osChanged || p.modelB != applied.modelB) for (auto& c : chB) c.prepare (p.modelB, sr, os.factor());
            if (force || p.lowCutHz != applied.lowCutHz || p.thumpDb != applied.thumpDb)
                for (auto& c : lowCut) c.set (Biquad::highpass, sr, p.lowCutHz, 0.7071 * std::pow (10.0, p.thumpDb / 20.0));
            if (force || p.toneDb != applied.toneDb)
                for (int c = 0; c < 2; ++c) { tiltLo[c].set (Biquad::lowShelf, sr, 1000.0, 0.5, -0.5 * p.toneDb); tiltHi[c].set (Biquad::highShelf, sr, 1000.0, 0.5, 0.5 * p.toneDb); }
            if (force || p.highCutHz != applied.highCutHz || p.steep != applied.steep)
                for (int c = 0; c < 2; ++c)
                {
                    const double hz = std::min ((double) p.highCutHz, sr * 0.49);
                    if (p.steep) { hc1[c].set (Biquad::lowpass, sr, hz, 0.6180); hc2[c].set (Biquad::lowpass, sr, hz, 1.6180); hc3[c].set (sr, hz); }   // 5th-order Butterworth
                    else hc3[c].set (sr, hz);
                    hcOff = p.highCutHz >= 21900.0f;
                }
            applied = p;
        }
        double latencySamples() const   { return os.latencySamples(); }
        float gainReductionDb() const   { return juce_like_gainToDb (std::min (limGain[0], limGain[1])); }
        float driveActivity() const     { return activity; }
        float autoGainDb() const        { return gainToDb (agGain); }

        /** In place. ch[0], ch[1] main; key may be nullptr (sidechain L/R). */
        void process (float* const* ch, int nch, const float* const* key, int n)
        {
            if (n > maxBlock) { for (int o = 0; o < n; o += maxBlock) { float* sub[2] = { ch[0] + o, ch[nch > 1 ? 1 : 0] + o }; const float* ks[2] = { key ? key[0] + o : nullptr, key ? key[1] + o : nullptr }; process (sub, nch, key ? ks : nullptr, std::min (maxBlock, n - o)); } return; }
            configure();
            const bool stereoIn = nch > 1;
            float* L = ch[0]; float* R = stereoIn ? ch[1] : ch[0];
            const auto& dA = model (p.modelA); const auto& dB = model (p.modelB);
            const bool useB = p.blend > 0.001f, useA = p.blend < 0.999f;
            // 1) drive gain per sample: drive + punish + calibration, dynamics, sidechain ducking
            const float baseDb = p.driveDb + (p.punish ? 20.0f : 0.0f);
            for (int i = 0; i < n; ++i)
            {
                const float a = 0.5f * (std::abs (L[i]) + std::abs (R[i]));
                env += (a > env ? envA : envR) * (a - env);
                float db = baseDb + p.dynamics * clamp (gainToDb (env + 1.0e-9f) + 18.0f, -18.0f, 18.0f) * 0.5f;
                if (p.keyOn && key != nullptr && p.duckDb > 0)
                {
                    const float k = 0.5f * (std::abs (key[0][i]) + std::abs (key[1][i]));
                    keyEnv += (k > keyEnv ? keyA : keyR) * (k - keyEnv);
                    db -= p.duckDb * clamp ((gainToDb (keyEnv + 1.0e-9f) + 40.0f) / 40.0f, 0.0f, 1.0f);
                }
                gBuf[0][(size_t) i] = dbToGain (db);
            }
            activity = gainToDb (gBuf[0][(size_t) (n - 1)]);
            // 2) M/S encode, keep the dry copy
            for (int i = 0; i < n; ++i)
            {
                dryW[0][(size_t) i] = L[i]; dryW[1][(size_t) i] = R[i];
                float a = L[i], b = R[i];
                if (p.ms != stereo) { const float m = 0.5f * (a + b), s = 0.5f * (a - b); a = m; b = s; }
                work[0][(size_t) i] = a; work[1][(size_t) i] = b;
            }
            const int chans = stereoIn ? 2 : 1;
            bool nlOnCh[2] { true, true };
            const float biasAdd = p.biasAmt * 0.5f;
            float eInBlk = 0, eOutBlk = 0;
            for (int c = 0; c < chans; ++c)
            {
                const bool nlOn = ! (p.ms == midOnly && c == 1) && ! (p.ms == sideOnly && c == 0);
                nlOnCh[c] = nlOn;
                float* w = work[c].data();
                // pre: low cut (+thump), tilt, then model A's input stage / emphasis (B gets its own copy below)
                const bool lcOn = p.lowCutHz > 20.5f || p.thumpDb > 0.05f;     // Low Cut at its minimum (20 Hz) with no Thump is off
                const bool tiltOn = std::abs (p.toneDb) > 0.01f;
                for (int i = 0; i < n; ++i)
                {
                    float v = w[i];
                    if (lcOn) v = lowCut[c].process (v);
                    if (tiltOn) v = tiltHi[c].process (tiltLo[c].process (v));
                    w[i] = v; eInBlk += v * v;
                }
                // broadcast limiter (model A or B): fast attack, program-dependent release, ceiling -10 dBFS pre-drive
                float* wb = nullptr;
                if (useB) { wb = scratchB[c]; std::memcpy (wb, w, sizeof (float) * (size_t) n); }
                if (useA) for (int i = 0; i < n; ++i) w[i] = chA[c].pre (limit (c, dA, w[i]));
                if (useB) for (int i = 0; i < n; ++i) wb[i] = chB[c].pre (limit (c, dB, wb[i]));
                // oversampled nonlinear section: keep only the residual when Delta is on
                const int F = os.factor(), m = n * F;
                auto runNl = [&] (Oversampler& o, ModelChain& mc, float* buf, float calibDb)
                {
                    float* u = o.up (c, buf, n);
                    const float cal = dbToGain (calibDb);
                    float gp = gPrev * cal;
                    for (int i = 0; i < n; ++i)
                    {
                        const float g1 = gBuf[0][(size_t) i] * cal, step = (g1 - gp) / (float) F;
                        for (int k = 0; k < F; ++k)
                        {
                            gp += step; float& v = u[i * F + k];
                            const float y = nlOn ? mc.nl (v, gp, biasAdd) : v;
                            v = p.delta ? y - v : y;
                        }
                    }
                    o.down (c, u, n, buf);
                    (void) m;
                };
                if (useA) runNl (os, chA[c], w, dA.calibDb);
                if (useB) runNl (osB, chB[c], wb, dB.calibDb);
                // post: model output stage, then blend
                for (int i = 0; i < n; ++i)
                {
                    const float ya = useA ? chA[c].post (w[i]) : 0.0f, yb = useB ? chB[c].post (wb[i]) : 0.0f;
                    w[i] = ya * (1.0f - p.blend) + yb * p.blend;
                    eOutBlk += w[i] * w[i];
                }
                // high cut
                if (! hcOff)
                    for (int i = 0; i < n; ++i) w[i] = p.steep ? hc3[c].lp (hc2[c].process (hc1[c].process (w[i]))) : hc3[c].lp (w[i]);
            }
            gPrev = gBuf[0][(size_t) (n - 1)];
            // 3) auto gain: match the energy after the tone controls (into the model) to the energy out of the model's
            //    output stage (before the High Cut), so it compensates drive and model colour but never your EQ moves
            if (p.autoGain && ! p.delta)
            {
                const float k = (float) (1.0 - std::exp (-(double) n / (0.6 * sr)));
                const float inPow = eInBlk / (float) (n * chans);
                if (inPow > 1.0e-6f)                                               // gate: hold during silence (-60 dBFS)
                {
                    eIn += k * (eInBlk - eIn); eOut += k * (eOutBlk - eOut);
                }
                agTarget = clamp (std::sqrt (std::max (1.0e-12f, eIn) / std::max (1.0e-12f, eOut)), 0.01f, 1000.0f);
            }
            else agTarget = p.delta ? 1.0f : std::sqrt (dbToGain (std::max (0.0f, baseDb)));   // Auto off: louder with drive, unity at Drive 0 and below
            // 4) decode, output, mix with the latency-aligned dry signal
            const float outG = dbToGain (p.outputDb);
            const float agK = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));
            for (int c = 0; c < chans; ++c) { float* u = osDry.up (c, dryW[c].data(), n); osDry.down (c, u, n, dryW[c].data()); }
            for (int i = 0; i < n; ++i)
            {
                agGain += agK * (agTarget - agGain);
                float a = work[0][(size_t) i], b = chans > 1 ? work[1][(size_t) i] : a;
                if (nlOnCh[0]) a *= agGain;
                if (nlOnCh[1]) b *= agGain;
                if (p.ms != stereo) { const float l = a + b, r = a - b; a = l; b = r; }
                const float g = outG;
                if (p.delta) { L[i] = a * outG; if (stereoIn) R[i] = b * outG; continue; }
                L[i] = (dryW[0][(size_t) i] * (1.0f - p.mix) + a * p.mix) * g;           // Output is after the Mix (trims the whole plug-in)
                if (stereoIn) R[i] = (dryW[1][(size_t) i] * (1.0f - p.mix) + b * p.mix) * g;
            }
        }

        /** Harmonic profile of the current settings: a 1 kHz sine at levelDb through a private copy (not real-time).
            h[0] = fundamental level (dBFS), h[1..9] = H2..H10 relative to the fundamental (dB); returns THD in %. */
        double harmonics (float levelDb, float* h, int count = 10) const
        {
            Processor q; q.p = p; q.p.mix = 1; q.p.delta = false; q.p.autoGain = false; q.p.keyOn = false; q.p.ms = stereo; q.p.dynamics = p.dynamics;
            const double rate = 48000.0; q.prepare (rate, 480);
            const int n = 480, blocks = 60;   // 0.6 s; analyse the last 0.2 s (200 cycles)
            std::vector<float> l ((size_t) n), r ((size_t) n), acc;
            const double w = 2.0 * pi * 1000.0 / rate; const float amp = dbToGain (levelDb) * 1.41421356f;
            for (int bl = 0; bl < blocks; ++bl)
            {
                for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (w * (bl * n + i));
                float* ch[2] = { l.data(), r.data() }; q.process (ch, 2, nullptr, n);
                if (bl >= blocks - 20) acc.insert (acc.end(), l.begin(), l.end());
            }
            double fund = 0, harm = 0;
            for (int k = 1; k <= count; ++k)
            {
                double S = 0, C = 0; const double wk = w * k;
                for (size_t i = 0; i < acc.size(); ++i) { const double t = (double) ((blocks - 20) * n + (int) i); S += acc[i] * std::sin (wk * t); C += acc[i] * std::cos (wk * t); }
                const double a = 2.0 * std::sqrt (S * S + C * C) / (double) acc.size();
                if (k == 1) { fund = a; h[0] = gainToDb ((float) (a / 1.41421356)); }
                else { harm += a * a; h[k - 1] = gainToDb ((float) (a / std::max (1.0e-12, fund))); }
            }
            return 100.0 * std::sqrt (harm) / std::max (1.0e-12, fund);
        }

    private:
        double sr = 48000; int maxBlock = 512;
        Params applied;
        Oversampler os, osDry, osB;
        ModelChain chA[2], chB[2];
        Biquad lowCut[2], tiltLo[2], tiltHi[2], hc1[2], hc2[2]; OnePole hc3[2]; bool hcOff = true;
        std::vector<float> work[2], dryW[2], gBuf[2];
        float scratchB[2][4096] {};
        float env = 0, envA = 0, envR = 0, keyEnv = 0, keyA = 0, keyR = 0, gPrev = 1, activity = 0;
        float limGain[2] { 1, 1 }, limEnv[2] {}, limA = 0;
        float eIn = 1.0e-6f, eOut = 1.0e-6f, agGain = 1, agTarget = 1;

        static float juce_like_gainToDb (float g)   { return gainToDb (g); }
        /** Broadcast amp limiter: ceiling at -10 dBFS (pre-drive), instant attack, release 60 ms .. 1.2 s depending on
            how long it has been limiting (program-dependent, like the optical / tube originals). */
        inline float limit (int c, const ModelDef& d, float x)
        {
            if (! d.limiter) return x;
            const float ceil = dbToGain (-10.0f), a = std::abs (x) * limGain[c];
            limEnv[c] += 0.00002f * ((a > ceil ? 1.0f : 0.0f) - limEnv[c]);          // long-term "how much limiting"
            if (a > ceil) limGain[c] = ceil / std::max (1.0e-9f, std::abs (x));
            else
            {
                const double relS = 0.06 + 1.14 * limEnv[c];
                limGain[c] += (float) (1.0 - std::exp (-1.0 / (relS * sr))) * (1.0f - limGain[c]);
            }
            return x * limGain[c];
        }
    };
}
