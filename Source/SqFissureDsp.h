#pragma once
// Fissure Filter - DSP core (1.14 "Studio Line", phase J). No UI, no te:: types: tests/fissure_test.cpp measures it.
//
// Four filter slots, each: type (low-pass, high-pass, band-pass, notch, peak, comb, vowel), slope (12 / 24 / 48 dB/oct as
// Butterworth cascades of zero-delay state-variable filters - exactly -3 dB at the cutoff with resonance at zero; the
// resonance lifts only the highest-Q stage), cutoff, resonance, drive (a soft stage before the filter), gain (peak),
// pan and mix. Routing: Serial (1 -> 2 -> 3 -> 4), Parallel (all four summed), Two Pairs (1->2 and 3->4 in parallel),
// Mid / Side (1->2 on the mid, 3->4 on the side).
// Modulation: two LFOs (sine / triangle / saw / square / random; free Hz or a tempo division resolved by the plug-in) and
// an envelope follower, routed by an eight-slot matrix to any slot's cutoff (octaves), resonance or mix, or the global mix.
// Coefficients follow the modulation every 8 samples, smoothly (the SVF tolerates fast changes without zipper or blow-up).

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace SqFissure
{
    constexpr int numSlots = 4, numMods = 8;
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    enum Type { lowPass = 0, highPass, bandPass, notch, peak, comb, vowel, numTypes };
    enum Routing { serial = 0, parallel, twoPairs, midSide };
    enum Source { srcNone = 0, srcLfo1, srcLfo2, srcEnv };
    enum LfoShape { sine = 0, triangle, saw, square, randomSteps };
    inline const char* typeName (int t) { static const char* n[] = { "Low-Pass", "High-Pass", "Band-Pass", "Notch", "Peak", "Comb", "Vowel" }; return n[juce::jlimit (0, numTypes - 1, t)]; }
    inline const char* routingName (int r) { static const char* n[] = { "Serial", "Parallel", "Two Pairs", "Mid / Side" }; return n[juce::jlimit (0, 3, r)]; }
    /** Targets: 0 none; 1..4 slot cutoff; 5..8 slot resonance; 9..12 slot mix; 13 global mix. */
    inline juce::String targetName (int t)
    {
        if (t >= 1 && t <= 4) return "Filter " + juce::String (t) + " Cutoff";
        if (t >= 5 && t <= 8) return "Filter " + juce::String (t - 4) + " Resonance";
        if (t >= 9 && t <= 12) return "Filter " + juce::String (t - 8) + " Mix";
        if (t == 13) return "Global Mix";
        return "None";
    }

    struct Slot
    {
        bool on = true; int type = lowPass, slope = 24;   // 12 / 24 / 48
        double cutoffHz = 1000, resonance = 0.2, driveDb = 0, gainDb = 0, pan = 0, mix = 1;   // resonance 0..1, pan -1..1
    };
    struct Mod { int source = srcNone, target = 0; double amount = 0; };   // amount: octaves for cutoff (+-6), fraction otherwise (+-1)
    struct Params
    {
        int routing = serial;
        std::array<Slot, numSlots> slot {};
        std::array<int, 2> lfoShape { sine, triangle }; std::array<double, 2> lfoHz { 1.0, 0.25 };
        double envAttackMs = 5, envReleaseMs = 120, envGainDb = 0;
        std::array<Mod, numMods> mod {};
        double mix = 1, outputDb = 0;
    };

    /** Butterworth Q sets for each slope; the last (highest) Q stage carries the resonance. */
    inline const std::vector<double>& qSet (int slope)
    {
        static const std::vector<double> q12 { 0.70710678 }, q24 { 0.54119610, 1.30656296 }, q48 { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        return slope >= 48 ? q48 : slope >= 24 ? q24 : q12;
    }
    /** Resonance 0..1 -> a multiplier on the top stage's Q (1 = Butterworth, up to 40x). */
    inline double resQ (double r) { return std::exp (juce::jlimit (0.0, 1.0, r) * std::log (40.0)); }

    struct Svf
    {
        double ic1 = 0, ic2 = 0, g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0;
        void set (double fs, double hz, double q)
        {
            g = std::tan (pi * juce::jlimit (10.0, fs * 0.49, hz) / fs); k = 1.0 / std::max (0.05, q);
            a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        inline void tick (double v0, double& lp, double& bp, double& hp)
        {
            const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    /** Vowel formants (Hz) for A E I O U - the vowel type morphs across them with the cutoff (as a position 0..1). */
    inline void vowelFormants (double pos, double& f1, double& f2, double& f3)
    {
        static const double F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        const double x = juce::jlimit (0.0, 1.0, pos) * 4.0; const int i = std::min (3, (int) x); const double t = x - i;
        f1 = F[i][0] + (F[i + 1][0] - F[i][0]) * t; f2 = F[i][1] + (F[i + 1][1] - F[i][1]) * t; f3 = F[i][2] + (F[i + 1][2] - F[i][2]) * t;
    }

    /** One slot for one channel. */
    struct SlotState
    {
        std::array<Svf, 4> st; std::array<Svf, 3> vw;
        std::vector<float> combBuf; int combW = 0;
        void prepare (double fs) { int s = 1; while (s < (int) (fs / 20.0) + 8) s <<= 1; combBuf.assign ((size_t) s, 0.0f); combW = 0; }
        void reset() { for (auto& f : st) f.reset(); for (auto& f : vw) f.reset(); std::fill (combBuf.begin(), combBuf.end(), 0.0f); }
        int nStages = 1; double combDelay = 48, combFb = 0, peakK = 0;   // current coefficients (set by the engine)
        void set (const Slot& s, double fs, double fc, double res)
        {
            const auto& qs = qSet (s.slope); nStages = (int) qs.size();
            switch (s.type)
            {
                case comb: combDelay = juce::jlimit (2.0, (double) combBuf.size() - 4, fs / juce::jlimit (20.0, fs * 0.45, fc)); combFb = 0.98 * juce::jlimit (0.0, 1.0, res); break;
                case vowel:
                {
                    double f1, f2, f3; vowelFormants (std::log (juce::jlimit (20.0, 20000.0, fc) / 20.0) / std::log (1000.0), f1, f2, f3);
                    const double q = 5.0 + 15.0 * res; vw[0].set (fs, f1, q); vw[1].set (fs, f2, q); vw[2].set (fs, f3, q);
                    break;
                }
                case peak: { const double A = std::pow (10.0, s.gainDb / 40.0); st[0].set (fs, fc, (0.7 + 6.0 * res) * A); peakK = st[0].k * (A * A - 1.0); nStages = 1; break; }   // RBJ-equivalent bell
                case bandPass: case notch: st[0].set (fs, fc, 0.5 + 15.0 * res); nStages = 1; break;
                default:
                    for (int i = 0; i < nStages; ++i) st[(size_t) i].set (fs, fc, qs[(size_t) i] * (i == nStages - 1 ? resQ (res) : 1.0));
            }
        }
        inline double process (const Slot& s, double x)
        {
            double lp, bp, hp;
            switch (s.type)
            {
                case highPass: for (int i = 0; i < nStages; ++i) { st[(size_t) i].tick (x, lp, bp, hp); x = hp; } return x;
                case bandPass: st[0].tick (x, lp, bp, hp); return bp * st[0].k;   // unity at the centre
                case notch:    st[0].tick (x, lp, bp, hp); return lp + hp;
                case peak:     st[0].tick (x, lp, bp, hp); return x + peakK * bp;
                case comb:
                {
                    const int mask = (int) combBuf.size() - 1; const double pos = combW - combDelay; const int i0 = (int) std::floor (pos); const double f = pos - i0;
                    const double d = combBuf[(size_t) (i0 & mask)] * (1 - f) + combBuf[(size_t) ((i0 + 1) & mask)] * f;
                    const double y = x + combFb * d;
                    combBuf[(size_t) (combW & mask)] = (float) juce::jlimit (-4.0, 4.0, y); ++combW;
                    return y * (1.0 - combFb * 0.5);
                }
                case vowel:
                {
                    double y = 0; for (int i = 0; i < 3; ++i) { vw[(size_t) i].tick (x, lp, bp, hp); y += bp * vw[(size_t) i].k * (i == 0 ? 1.0 : i == 1 ? 0.7 : 0.4); }
                    return y;
                }
                default:       for (int i = 0; i < nStages; ++i) { st[(size_t) i].tick (x, lp, bp, hp); x = lp; } return x;
            }
        }
    };

    class Engine
    {
    public:
        Params p;
        std::array<float, numSlots> meterCutoff {}, meterRes {}, meterMix {};
        std::array<float, 2> meterLfo {}; float meterEnv = 0;

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            for (auto& c : st) for (auto& s : c) s.prepare (fs);
            reset();
        }
        void reset()
        {
            for (auto& c : st) for (auto& s : c) s.reset();
            lfoPh = { 0.0, 0.5 }; env = 0; rnd = { 0, 0 }; seed = 777u; ctr = 0;
            for (int k = 0; k < numSlots; ++k) { smCut[k] = std::log (p.slot[(size_t) k].cutoffHz); smRes[k] = p.slot[(size_t) k].resonance; }
            configure();
        }

        /** The magnitude (dB) of slot k's filter at f with its current settings (unmodulated) - for the face's curves. */
        static double responseDb (const Slot& s, double fs, double f)
        {
            if (! s.on) return 0;
            SlotState tmp; tmp.combBuf.assign (8, 0.0f);   // comb is drawn analytically below
            if (s.type == comb)
            {
                const double D = fs / juce::jlimit (20.0, fs * 0.45, s.cutoffHz), fb = 0.98 * s.resonance, w = 2 * pi * f / fs;
                const std::complex<double> z = std::polar (1.0, -w * D);
                return gainToDb (std::abs (1.0 / (1.0 - fb * z)) * (1.0 - fb * 0.5));
            }
            tmp.set (s, fs, s.cutoffHz, s.resonance);
            // evaluate the SVF chain's transfer function at f (bilinear: z = e^{jw})
            const std::complex<double> z1 = std::polar (1.0, -2 * pi * f / fs);
            auto svf = [&] (const Svf& v, int out) -> std::complex<double>
            {
                // TPT SVF transfer: with s = (1 - z^-1) / (g (1 + z^-1))
                const std::complex<double> s_ = (1.0 - z1) / (v.g * (1.0 + z1)), den = s_ * s_ + v.k * s_ + 1.0;
                if (out == 0) return 1.0 / den;            // LP
                if (out == 1) return s_ / den;             // BP
                return s_ * s_ / den;                      // HP
            };
            std::complex<double> h = 1.0;
            switch (s.type)
            {
                case highPass: for (int i = 0; i < tmp.nStages; ++i) h *= svf (tmp.st[(size_t) i], 2); break;
                case bandPass: h = svf (tmp.st[0], 1) * tmp.st[0].k; break;
                case notch:    h = svf (tmp.st[0], 0) + svf (tmp.st[0], 2); break;
                case peak:     h = 1.0 + tmp.peakK * svf (tmp.st[0], 1); break;
                case vowel:    h = svf (tmp.vw[0], 1) * tmp.vw[0].k + 0.7 * svf (tmp.vw[1], 1) * tmp.vw[1].k + 0.4 * svf (tmp.vw[2], 1) * tmp.vw[2].k; break;
                default:       for (int i = 0; i < tmp.nStages; ++i) h *= svf (tmp.st[(size_t) i], 0);
            }
            const double m = std::abs (h);
            return gainToDb (m * s.mix + (1.0 - s.mix) * 1.0);
        }

        void process (float* const* ch, int nch, int n)
        {
            const bool stereo = nch > 1;
            const float outG = (float) dbToGain (p.outputDb);
            const double aAtt = 1.0 - std::exp (-1.0 / (std::max (0.1, p.envAttackMs) * 0.001 * fs)), aRel = 1.0 - std::exp (-1.0 / (std::max (1.0, p.envReleaseMs) * 0.001 * fs));
            const double envG = dbToGain (p.envGainDb);
            for (int s = 0; s < n; ++s)
            {
                const double xl = ch[0][s], xr = stereo ? ch[1][s] : xl;
                // modulators (every sample: they are cheap)
                const double a = std::max (std::abs (xl), std::abs (xr)) * envG;
                env += (a - env) * (a > env ? aAtt : aRel);
                double lfo[2];
                for (int i = 0; i < 2; ++i)
                {
                    lfoPh[(size_t) i] += p.lfoHz[(size_t) i] / fs;
                    if (lfoPh[(size_t) i] >= 1.0) { lfoPh[(size_t) i] -= 1.0; seed = seed * 1664525u + 1013904223u; rnd[(size_t) i] = (double) (seed >> 8) / 8388608.0 - 1.0; }
                    const double ph = lfoPh[(size_t) i];
                    switch (p.lfoShape[(size_t) i])
                    {
                        case triangle:    lfo[i] = 1.0 - 4.0 * std::abs (ph - 0.5); break;
                        case saw:         lfo[i] = 2.0 * ph - 1.0; break;
                        case square:      lfo[i] = ph < 0.5 ? 1.0 : -1.0; break;
                        case randomSteps: lfo[i] = rnd[(size_t) i]; break;
                        default:          lfo[i] = std::sin (2 * pi * ph);
                    }
                }
                const double envN = juce::jlimit (0.0, 1.0, (gainToDb (env) + 60.0) / 60.0);
                if ((ctr++ & 7) == 0)
                {   // the matrix, then the coefficients (every 8 samples, smoothed)
                    double cutOct[numSlots] {}, resAdd[numSlots] {}, mixAdd[numSlots] {}; gMixAdd = 0;
                    for (auto& m : p.mod)
                    {
                        if (m.source == srcNone || m.target <= 0) continue;
                        const double v = m.source == srcLfo1 ? lfo[0] : m.source == srcLfo2 ? lfo[1] : envN;
                        if (m.target <= 4) cutOct[m.target - 1] += v * m.amount;
                        else if (m.target <= 8) resAdd[m.target - 5] += v * m.amount;
                        else if (m.target <= 12) mixAdd[m.target - 9] += v * m.amount;
                        else if (m.target == 13) gMixAdd += v * m.amount;
                    }
                    for (int k = 0; k < numSlots; ++k)
                    {
                        const auto& sl = p.slot[(size_t) k];
                        const double tCut = std::log (juce::jlimit (20.0, 20000.0, sl.cutoffHz)) + cutOct[k] * std::log (2.0), tRes = juce::jlimit (0.0, 1.0, sl.resonance + resAdd[k]);
                        smCut[k] += (tCut - smCut[k]) * 0.35; smRes[k] += (tRes - smRes[k]) * 0.35;
                        curMix[k] = juce::jlimit (0.0, 1.0, sl.mix + mixAdd[k]);
                        const double fc = juce::jlimit (20.0, std::min (20000.0, fs * 0.45), std::exp (smCut[k]));
                        for (int c = 0; c < 2; ++c) st[(size_t) c][(size_t) k].set (sl, fs, fc, smRes[k]);
                        meterCutoff[(size_t) k] = (float) fc; meterRes[(size_t) k] = (float) smRes[k]; meterMix[(size_t) k] = (float) curMix[k];
                    }
                }
                double in[2] = { xl, xr };
                if (p.routing == midSide) { const double m = 0.5 * (xl + xr), sd = 0.5 * (xl - xr); in[0] = m; in[1] = sd; }
                double out[2];
                for (int c = 0; c < 2; ++c)
                {
                    auto run = [&] (int k, double x)
                    {
                        const auto& sl = p.slot[(size_t) k];
                        if (! sl.on) return x;
                        const double d = sl.driveDb > 0.01 ? std::tanh (x * dbToGain (sl.driveDb)) / std::tanh (dbToGain (sl.driveDb)) : x;
                        const double y = st[(size_t) c][(size_t) k].process (sl, d);
                        // pan inside the slot (constant power) applies to the slot's wet part only
                        double pg = 1.0;
                        if (std::abs (sl.pan) > 0.001 && p.routing != midSide) { const double a_ = (sl.pan + 1.0) * 0.25 * pi; pg = (c == 0 ? std::cos (a_) : std::sin (a_)) * std::sqrt (2.0); }
                        return x + (y * pg - x) * curMix[k];
                    };
                    switch (p.routing)
                    {
                        case parallel: { double sum = 0; int nOn = 0; for (int k = 0; k < numSlots; ++k) if (p.slot[(size_t) k].on) { sum += run (k, in[c]); ++nOn; } out[c] = nOn ? sum / nOn : in[c]; break; }
                        case twoPairs: { const double a_ = run (1, run (0, in[c])), b_ = run (3, run (2, in[c])); out[c] = 0.5 * (a_ + b_); break; }
                        case midSide:  out[c] = c == 0 ? run (1, run (0, in[c])) : run (3, run (2, in[c])); break;
                        default:       out[c] = run (3, run (2, run (1, run (0, in[c]))));
                    }
                }
                if (p.routing == midSide) { const double m = out[0], sd = out[1]; out[0] = m + sd; out[1] = m - sd; }
                const double gm = juce::jlimit (0.0, 1.0, p.mix + gMixAdd);
                ch[0][s] = (float) ((xl + (out[0] - xl) * gm) * outG);
                if (stereo) ch[1][s] = (float) ((xr + (out[1] - xr) * gm) * outG);
                meterLfo = { (float) lfo[0], (float) lfo[1] }; meterEnv = (float) envN;
            }
        }

    private:
        double fs = 48000;
        std::array<std::array<SlotState, numSlots>, 2> st;
        std::array<double, 2> lfoPh {}, rnd {}; double env = 0, gMixAdd = 0; unsigned seed = 777u; unsigned ctr = 0;
        double smCut[numSlots] {}, smRes[numSlots] {}, curMix[numSlots] { 1, 1, 1, 1 };
        void configure()
        {
            for (int k = 0; k < numSlots; ++k)
            {
                const auto& sl = p.slot[(size_t) k];
                curMix[k] = sl.mix;
                for (int c = 0; c < 2; ++c) st[(size_t) c][(size_t) k].set (sl, fs, juce::jlimit (20.0, std::min (20000.0, fs * 0.45), sl.cutoffHz), sl.resonance);
            }
        }
    };
}
