#pragma once
// Duet Filter and Speck Filter - DSP core (1.14 "Studio Line", phase K small tools). No UI, no te:: types:
// tests/duet_test.cpp measures it.
//
// Two filter slots built on the Fissure filter (SqFissureDsp.h: Butterworth cascades of zero-delay SVFs, exactly -3 dB at
// the cutoff with no resonance; low-pass, high-pass, band-pass, notch, peak, comb, vowel; 12 / 24 / 48 dB/oct).
// Routing: Serial (1 -> 2), Parallel (1 + 2), Stereo (1 on the left, 2 on the right) and Mid / Side (1 on the mid, 2 on the side).
// Link: filter 2's cutoff follows filter 1 at a fixed interval (octaves), so one knob moves the pair.
// Movement: one LFO (sine / triangle / saw / square / random, free or tempo-synced by the plug-in) with its own depth for
// each filter and a stereo phase offset (0..180 degrees: the left and right cutoffs move apart), and an envelope follower
// (attack, release, sensitivity, from the input or a sidechain key) with its own depth for each filter.
// Speck Filter is this engine with filter 2 switched off.

#include "SqFissureDsp.h"

namespace SqDuet
{
    using SqFissure::pi; using SqFissure::dbToGain; using SqFissure::gainToDb;
    enum Routing { serial = 0, parallel, stereoSplit, midSide };
    inline const char* routingName (int r) { static const char* n[] = { "Serial", "Parallel", "Stereo", "Mid / Side" }; return n[juce::jlimit (0, 3, r)]; }

    struct Params
    {
        int routing = serial;
        std::array<SqFissure::Slot, 2> slot {};
        bool link = false; double linkOct = 1.0;                 // filter 2 = filter 1 x 2^linkOct when linked
        int lfoShape = SqFissure::sine; double lfoHz = 1.0, lfoPhaseDeg = 0.0;
        std::array<double, 2> lfoOct { 0.0, 0.0 };               // LFO depth per filter, octaves (+-4)
        double envAttackMs = 5, envReleaseMs = 150, envGainDb = 0; bool envFromKey = false;
        std::array<double, 2> envOct { 0.0, 0.0 };               // envelope depth per filter, octaves (+-6)
        double mix = 1, outputDb = 0;
    };

    class Engine
    {
    public:
        Params p;
        std::array<std::array<float, 2>, 2> meterCutoff {};   // [filter][channel] - the cutoffs as they are right now
        float meterLfo = 0, meterEnv = 0, meterEnvDb = -120;

        void prepare (double sampleRate, int)
        {
            fs = sampleRate;
            for (auto& c : st) for (auto& s : c) s.prepare (fs);
            reset();
        }
        void reset()
        {
            for (auto& c : st) for (auto& s : c) s.reset();
            lfoPh = 0; env = 0; rnd = 0; seed = 4321u; ctr = 0;
            for (int k = 0; k < 2; ++k) for (int c = 0; c < 2; ++c) smCut[k][c] = std::log (baseCut (k));
            for (int k = 0; k < 2; ++k) smRes[k] = p.slot[(size_t) k].resonance;
            for (int k = 0; k < 2; ++k) for (int c = 0; c < 2; ++c) st[(size_t) c][(size_t) k].set (p.slot[(size_t) k], fs, clampHz (baseCut (k)), p.slot[(size_t) k].resonance);
        }

        /** Filter k's cutoff before modulation (with the link applied). */
        double baseCut (int k) const
        {
            if (k == 1 && p.link) return clampHz (p.slot[0].cutoffHz * std::pow (2.0, p.linkOct));
            return clampHz (p.slot[(size_t) k].cutoffHz);
        }
        /** Slot k as the face should draw it (link applied). */
        SqFissure::Slot drawnSlot (int k) const { auto s = p.slot[(size_t) k]; s.cutoffHz = baseCut (k); return s; }
        /** The combined magnitude (dB) at f for the current routing, unmodulated (Stereo / Mid-Side show the average). */
        double responseDb (double f) const
        {
            const double a = SqFissure::Engine::responseDb (drawnSlot (0), fs, f), b = p.slot[1].on ? SqFissure::Engine::responseDb (drawnSlot (1), fs, f) : 0.0;
            double out;
            if (p.routing == serial) out = a + b;
            else if (p.routing == parallel) out = p.slot[1].on && p.slot[0].on ? gainToDb (0.5 * (dbToGain (a) + dbToGain (b))) : (p.slot[0].on ? a : b);
            else out = gainToDb (0.5 * (dbToGain (a) + dbToGain (b)));
            return gainToDb (dbToGain (out) * p.mix + (1.0 - p.mix)) + p.outputDb;
        }
        double sampleRate() const { return fs; }

        /** ch: the audio (1 or 2 channels, processed in place); key: optional sidechain (1 or 2 channels). */
        void process (float* const* ch, int nch, int n, const float* const* key = nullptr, int nKey = 0)
        {
            const bool stereo = nch > 1;
            const float outG = (float) dbToGain (p.outputDb);
            const double aAtt = 1.0 - std::exp (-1.0 / (std::max (0.1, p.envAttackMs) * 0.001 * fs)), aRel = 1.0 - std::exp (-1.0 / (std::max (1.0, p.envReleaseMs) * 0.001 * fs));
            const double envG = dbToGain (p.envGainDb), phOff = juce::jlimit (0.0, 180.0, p.lfoPhaseDeg) / 360.0;
            const bool useKey = p.envFromKey && key != nullptr && nKey > 0;
            for (int s = 0; s < n; ++s)
            {
                const double xl = ch[0][s], xr = stereo ? ch[1][s] : xl;
                const double det = useKey ? std::max (std::abs ((double) key[0][s]), std::abs ((double) key[nKey > 1 ? 1 : 0][s])) : std::max (std::abs (xl), std::abs (xr));
                const double a = det * envG;
                env += (a - env) * (a > env ? aAtt : aRel);
                lfoPh += p.lfoHz / fs;
                if (lfoPh >= 1.0) { lfoPh -= 1.0; seed = seed * 1664525u + 1013904223u; rnd = (double) (seed >> 8) / 8388608.0 - 1.0; }
                if ((ctr++ & 7) == 0)
                {
                    const double envN = juce::jlimit (0.0, 1.0, (gainToDb (env) + 60.0) / 60.0);
                    const double lfo[2] = { lfoAt (lfoPh), lfoAt (lfoPh + phOff) };   // left, right
                    for (int k = 0; k < 2; ++k)
                    {
                        const auto& sl = p.slot[(size_t) k];
                        smRes[k] += (sl.resonance - smRes[k]) * 0.35;
                        for (int c = 0; c < 2; ++c)
                        {
                            const double tCut = std::log (baseCut (k)) + (p.lfoOct[(size_t) k] * lfo[c] + p.envOct[(size_t) k] * envN) * std::log (2.0);
                            smCut[k][c] += (tCut - smCut[k][c]) * 0.35;
                            const double fc = clampHz (std::exp (smCut[k][c]));
                            auto slot = sl; if (k == 1 && p.link) slot.cutoffHz = fc;
                            st[(size_t) c][(size_t) k].set (slot, fs, fc, smRes[k]);
                            meterCutoff[(size_t) k][(size_t) c] = (float) fc;
                        }
                    }
                    meterLfo = (float) lfo[0]; meterEnv = (float) envN; meterEnvDb = (float) gainToDb (env);
                }
                double in[2] = { xl, xr };
                if (p.routing == midSide) { in[0] = 0.5 * (xl + xr); in[1] = 0.5 * (xl - xr); }
                double out[2];
                for (int c = 0; c < 2; ++c)
                {
                    auto run = [&] (int k, double x)
                    {
                        const auto& sl = p.slot[(size_t) k];
                        if (! sl.on) return x;
                        const double d = sl.driveDb > 0.01 ? std::tanh (x * dbToGain (sl.driveDb)) / std::tanh (dbToGain (sl.driveDb)) : x;
                        const double y = st[(size_t) c][(size_t) k].process (sl, d);
                        return x + (y - x) * sl.mix;
                    };
                    switch (p.routing)
                    {
                        case parallel:
                        {
                            const bool a0 = p.slot[0].on, a1 = p.slot[1].on;
                            out[c] = a0 && a1 ? 0.5 * (run (0, in[c]) + run (1, in[c])) : a0 ? run (0, in[c]) : a1 ? run (1, in[c]) : in[c];
                            break;
                        }
                        case stereoSplit: case midSide: out[c] = run (c, in[c]); break;
                        default: out[c] = run (1, run (0, in[c]));
                    }
                }
                if (p.routing == midSide) { const double m = out[0], sd = out[1]; out[0] = m + sd; out[1] = m - sd; }
                if (! stereo && p.routing == stereoSplit) out[0] = 0.5 * (out[0] + out[1]);
                const double gm = juce::jlimit (0.0, 1.0, p.mix);
                ch[0][s] = (float) ((xl + (out[0] - xl) * gm) * outG);
                if (stereo) ch[1][s] = (float) ((xr + (out[1] - xr) * gm) * outG);
            }
        }

    private:
        double fs = 48000;
        std::array<std::array<SqFissure::SlotState, 2>, 2> st;   // [channel][filter]
        double lfoPh = 0, env = 0, rnd = 0; unsigned seed = 4321u, ctr = 0;
        double smCut[2][2] {}, smRes[2] {};
        double clampHz (double f) const { return juce::jlimit (20.0, std::min (20000.0, fs * 0.45), f); }
        double lfoAt (double ph) const
        {
            ph -= std::floor (ph);
            switch (p.lfoShape)
            {
                case SqFissure::triangle:    return 1.0 - 4.0 * std::abs (ph - 0.5);
                case SqFissure::saw:         return 2.0 * ph - 1.0;
                case SqFissure::square:      return ph < 0.5 ? 1.0 : -1.0;
                case SqFissure::randomSteps: return rnd;
                default:                     return std::sin (2 * pi * ph);
            }
        }
    };
}
