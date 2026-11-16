#pragma once
// Kiln - DSP core (1.14 "Studio Line", phase H). No UI, no te:: types: tests/kiln_test.cpp measures it.
//
// Multiband saturation built on Squelchmoth's own Cinder engine:
//   input -> up to six bands (minimum-phase Linkwitz-Riley tree with compensating all-passes: the bands sum to an
//   all-pass, so with nothing driven the sound is untouched in level) -> each band through its own Cinder processor
//   (any of the ten models, drive, bias, dynamics, mix, output, auto gain, 1x-16x oversampling) -> sum -> global mix
//   (latency-aligned dry) -> output.
// Two modulators and a four-slot matrix move any band's drive or mix (or the global mix) over time:
//   an LFO (sine / triangle / square / random steps, rate in Hz) and an envelope follower (attack / release).
// Every band uses the same oversampling, so their latencies match and the sum stays phase-aligned.

#include "SqCinderDsp.h"
#include "SqApexMB2Dsp.h"
#include <array>
#include <cmath>
#include <vector>

namespace SqKiln
{
    constexpr int maxBands = 6, numSlots = 4;
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    enum Source { srcNone = 0, srcLfo, srcEnv };
    enum LfoShape { sine = 0, triangle, square, randomSteps };
    /** Targets: 0 none, 1..6 band drive, 7..12 band mix, 13 global mix. */
    inline juce::String targetName (int t)
    {
        if (t >= 1 && t <= 6) return "Band " + juce::String (t) + " Drive";
        if (t >= 7 && t <= 12) return "Band " + juce::String (t - 6) + " Mix";
        if (t == 13) return "Global Mix";
        return "None";
    }

    struct Band
    {
        int model = Cinder::triodeBox;
        float driveDb = 6, bias = 0, dynamics = 0.3f, mix = 1, outputDb = 0;
        bool solo = false, bypass = false;
    };
    struct Slot { int source = srcNone, target = 0; float amount = 0; };   // amount: drive dB (+-36) or mix fraction (+-1)
    struct Params
    {
        int numBands = 3, oversampleLog2 = 2; bool linearPhaseOs = false, autoGain = true;
        std::array<double, maxBands - 1> xover { 200, 2500, 6000, 10000, 15000 };
        float inputDb = 0, outputDb = 0, mix = 1;
        std::array<Band, maxBands> band {};
        int lfoShape = sine; float lfoHz = 1.0f;
        float envAttackMs = 10, envReleaseMs = 150;
        std::array<Slot, numSlots> slot {};
    };

    class Engine
    {
    public:
        Params p;
        std::array<float, maxBands> meterActivity {}, meterLevelDb {};
        float meterLfo = 0, meterEnv = 0;
        std::array<float, maxBands> meterDriveDb {};   // the drive actually used (after modulation)

        void prepare (double sampleRate, int maxBlockSize)
        {
            fs = sampleRate; maxBlock = std::max (64, maxBlockSize);
            for (auto& pr : proc) { pr.p = cinderParams (p.band[0]); pr.prepare (fs, maxBlock); }
            for (int c = 0; c < 2; ++c) { for (auto& b : bandBuf[c]) b.assign ((size_t) maxBlock, 0.0f); dryBuf[c].assign ((size_t) maxBlock, 0.0f); }
            dry[0].prepare (65536); dry[1].prepare (65536);
            aEnvA = 1.0 - std::exp (-1.0 / (std::max (0.1f, p.envAttackMs) * 0.001 * fs));
            lastN = -1; lastX.fill (-1);
            reset();
        }
        void reset()
        {
            for (auto& s : split) s.reset();
            for (auto& pr : proc) pr.reset();
            dry[0].clear(); dry[1].clear();
            lfoPhase = 0; env = 0; rndValue = 0; rndSeed = 12345u;
        }
        int latency() const { return (int) std::lround (proc[0].latencySamples()); }
        /** Apply the settings now (no audio): what the latency will be. */
        void refresh() { configure(); }

        /** Current modulator outputs, -1..1 (LFO) and 0..1 (envelope). */
        float lfoValue() const { return meterLfo; }

        void process (float* const* ch, int nch, int n)
        {
            if (n > maxBlock) { for (int o = 0; o < n; o += maxBlock) { float* sub[2] = { ch[0] + o, ch[nch > 1 ? 1 : 0] + o }; process (sub, nch, std::min (maxBlock, n - o)); } return; }
            configure();
            const int nb = curN;
            const bool stereo = nch > 1;
            const float inG = (float) dbToGain (p.inputDb), outG = (float) dbToGain (p.outputDb), mix = juce::jlimit (0.0f, 1.0f, p.mix);
            // modulators: evaluated once per block (smooth enough for drive / mix; blocks are <= 512 samples)
            double blockPk = 0;
            for (int s = 0; s < n; ++s) blockPk = std::max (blockPk, (double) std::max (std::abs (ch[0][s]), std::abs (stereo ? ch[1][s] : ch[0][s])) * inG);
            const double aRel = 1.0 - std::exp (-(double) n / (std::max (1.0f, p.envReleaseMs) * 0.001 * fs));
            const double aAtt = 1.0 - std::exp (-(double) n / (std::max (0.1f, p.envAttackMs) * 0.001 * fs));
            const double envTarget = juce::jlimit (0.0, 1.0, (gainToDb (blockPk) + 60.0) / 60.0);
            env += (envTarget - env) * (envTarget > env ? aAtt : aRel);
            lfoPhase += p.lfoHz * n / fs;
            if (lfoPhase >= 1.0) { lfoPhase -= std::floor (lfoPhase); rndSeed = rndSeed * 1664525u + 1013904223u; rndValue = (float) ((rndSeed >> 8) / 8388608.0 - 1.0); }
            float lfo;
            switch (p.lfoShape)
            {
                case triangle:    lfo = (float) (1.0 - 4.0 * std::abs (lfoPhase - 0.5)); break;
                case square:      lfo = lfoPhase < 0.5 ? 1.0f : -1.0f; break;
                case randomSteps: lfo = rndValue; break;
                default:          lfo = (float) std::sin (2 * pi * lfoPhase);
            }
            meterLfo = lfo; meterEnv = (float) env;
            float driveMod[maxBands] {}, mixMod[maxBands] {}; float globalMixMod = 0;
            for (auto& sl : p.slot)
            {
                if (sl.source == srcNone || sl.target <= 0) continue;
                const float v = sl.source == srcLfo ? lfo : (float) env;
                if (sl.target <= 6) driveMod[sl.target - 1] += v * sl.amount;
                else if (sl.target <= 12) mixMod[sl.target - 7] += v * sl.amount;
                else if (sl.target == 13) globalMixMod += v * sl.amount;
            }
            // split into bands
            for (int s = 0; s < n; ++s)
            {
                const float x[2] = { ch[0][s] * inG, (stereo ? ch[1][s] : ch[0][s]) * inG };
                for (int c = 0; c < 2; ++c)
                {
                    double b[maxBands]; split[(size_t) c].split (x[c], b, nb);
                    for (int k = 0; k < nb; ++k) bandBuf[c][(size_t) k][(size_t) s] = (float) b[k];
                    dryBuf[c][(size_t) s] = dry[c].process (x[c], latency());
                }
            }
            // each band through its own Cinder
            bool anySolo = false; for (int k = 0; k < nb; ++k) anySolo = anySolo || p.band[(size_t) k].solo;
            for (int k = 0; k < nb; ++k)
            {
                auto& b = p.band[(size_t) k]; auto& pr = proc[(size_t) k];
                auto cp = cinderParams (b);
                cp.driveDb = juce::jlimit (-12.0f, 36.0f, b.driveDb + driveMod[k]);
                cp.mix = b.bypass ? 0.0f : juce::jlimit (0.0f, 1.0f, b.mix + mixMod[k]);
                meterDriveDb[(size_t) k] = cp.driveDb;
                pr.p = cp;
                float* bc[2] = { bandBuf[0][(size_t) k].data(), bandBuf[1][(size_t) k].data() };
                float pk = 0; for (int s = 0; s < n; ++s) pk = std::max (pk, std::abs (bc[0][s]));
                pr.process (bc, 2, nullptr, n);
                meterActivity[(size_t) k] = b.bypass ? 0.0f : pr.driveActivity();
                meterLevelDb[(size_t) k] = (float) gainToDb (pk);
            }
            for (int k = nb; k < maxBands; ++k) { meterActivity[(size_t) k] = 0; meterLevelDb[(size_t) k] = -100; }
            const float gm = juce::jlimit (0.0f, 1.0f, mix + globalMixMod);
            for (int s = 0; s < n; ++s)
            {
                double sum[2] = { 0, 0 };
                for (int k = 0; k < nb; ++k) if (! anySolo || p.band[(size_t) k].solo) { sum[0] += bandBuf[0][(size_t) k][(size_t) s]; sum[1] += bandBuf[1][(size_t) k][(size_t) s]; }
                const float y0 = (float) sum[0] * gm + dryBuf[0][(size_t) s] * (1 - gm), y1 = (float) sum[1] * gm + dryBuf[1][(size_t) s] * (1 - gm);
                ch[0][s] = y0 * outG; if (stereo) ch[1][s] = y1 * outG;
            }
        }

    private:
        double fs = 48000; int maxBlock = 512;
        std::array<SqMB2::MinSplit, 2> split;
        std::array<Cinder::Processor, maxBands> proc;
        std::array<std::array<std::vector<float>, maxBands>, 2> bandBuf; std::array<std::vector<float>, 2> dryBuf;
        std::array<SqMB2::Delay, 2> dry;
        std::array<double, maxBands - 1> lastX {}; int lastN = -1, curN = 3, lastOs = -1; bool lastLin = false;
        double lfoPhase = 0, env = 0, aEnvA = 0; float rndValue = 0; unsigned rndSeed = 12345u;

        Cinder::Params cinderParams (const Band& b) const
        {
            Cinder::Params c; c.modelA = juce::jlimit (0, (int) Cinder::numModels - 1, b.model); c.blend = 0;
            c.driveDb = b.driveDb; c.biasAmt = b.bias; c.dynamics = b.dynamics; c.mix = b.mix; c.outputDb = b.outputDb;
            c.autoGain = p.autoGain; c.oversampleLog2 = p.oversampleLog2; c.linearPhase = p.linearPhaseOs;
            c.lowCutHz = 20; c.highCutHz = 22000; c.toneDb = 0; c.thumpDb = 0;
            return c;
        }
        void configure()
        {
            curN = juce::jlimit (1, maxBands, p.numBands);
            SqMB2::Params sp; sp.numBands = curN; sp.xover = p.xover;
            const auto x = SqMB2::sanitise (sp, fs);
            bool changed = curN != lastN; for (int i = 0; i < curN - 1 && ! changed; ++i) changed = std::abs (x[(size_t) i] - lastX[(size_t) i]) > 0.01;
            if (changed) { for (auto& s : split) { s.set (fs, x, curN); if (curN != lastN) s.reset(); } lastX = x; lastN = curN; }
            if (p.oversampleLog2 != lastOs || p.linearPhaseOs != lastLin)
            {   // every band changes oversampling together (their latencies must match)
                for (int k = 0; k < maxBands; ++k) { proc[(size_t) k].p = cinderParams (p.band[(size_t) k]); proc[(size_t) k].configure (true); }
                lastOs = p.oversampleLog2; lastLin = p.linearPhaseOs;
            }
        }
    };
}
