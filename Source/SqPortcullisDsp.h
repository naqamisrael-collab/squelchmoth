#pragma once
// Portcullis Gate - DSP core (1.14 "Studio Line", phase G). No UI, no te:: types: tests/portcullis_test.cpp measures it.
//
// A gate / expander / ducker with:
//   - two thresholds (open and close: hysteresis, so a decaying note never chatters at the threshold)
//   - range (how far it closes), attack, hold, release; the attack and release follow a chosen curve
//   - lookahead up to 10 ms (the gate is already open when the transient arrives)
//   - a key: the input, the side-chain, or MIDI notes (a note-on opens it, its velocity can scale the opening)
//   - a key filter (high-pass and low-pass, 12 dB/oct) and "listen to the key"
//   - modes: Gate (hard, to the range), Expand (a ratio below the threshold), Duck (the key pulls the signal DOWN)
//   - flip: hear only what the gate removes
// The gate gain is computed on a 0..1 "openness" that the face draws, and is applied as a level in dB so a 60 dB
// range closes to exactly -60 dB.

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

namespace SqPortcullis
{
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    enum Mode  { gate = 0, expand, duck };
    enum Key   { self = 0, sidechain, midi };
    enum Curve { linear = 0, smooth, punchy };

    struct Params
    {
        int mode = gate, key = self, curve = smooth;
        double thresholdDb = -40, hysteresisDb = 6, rangeDb = 60, ratio = 3;
        double attackMs = 0.5, holdMs = 30, releaseMs = 120, lookaheadMs = 0;
        double keyHpHz = 20, keyLpHz = 20000;   // 20 / 20000 = off
        bool listenKey = false, flip = false, velocity = false;
        double outputDb = 0;
    };

    struct Svf
    {
        double ic1 = 0, ic2 = 0, g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0;
        void set (double fs, double hz) { g = std::tan (pi * juce::jlimit (10.0, fs * 0.49, hz) / fs); k = 1.4142; a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
        inline void tick (double v0, double& lp, double& hp) { const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3; ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; hp = v0 - k * v1 - v2; }
        void reset() { ic1 = ic2 = 0; }
    };

    struct Delay
    {
        std::vector<float> b; int mask = 0, w = 0;
        void prepare (int n) { int s = 1; while (s < n + 2) s <<= 1; b.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        inline float process (float x, int d) { b[(size_t) (w & mask)] = x; const float y = b[(size_t) ((w - d) & mask)]; ++w; return y; }
    };

    /** Shape an openness 0..1 by the curve (linear, smooth = raised cosine, punchy = fast start). */
    inline double shape (double t, int curve)
    {
        t = juce::jlimit (0.0, 1.0, t);
        if (curve == smooth) return 0.5 - 0.5 * std::cos (pi * t);
        if (curve == punchy) return 1.0 - (1.0 - t) * (1.0 - t);
        return t;
    }

    class Engine
    {
    public:
        Params p;
        float meterKeyDb = -100, meterGainDb = 0, meterOpen = 0;   // openness 0..1

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            for (auto& d : dly) d.prepare ((int) std::ceil (0.010 * fs) + 4);
            aEnv = 1.0 - std::exp (-1.0 / (0.0003 * fs));
            relEnv = 1.0 - std::exp (-1.0 / (0.010 * fs));
            pendingOn.reserve (256); pendingOff.reserve (256);
            reset();
        }
        void reset()
        {
            for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); for (auto& d : dly) d.clear();
            env = 0; open = 0; isOpen = false; holdLeft = 0; midiNotes = 0; midiVel = 1; pendingOn.clear(); pendingOff.clear();
            lastHp = lastLp = -1;
        }
        int latency() const { return (int) std::lround (juce::jlimit (0.0, 10.0, p.lookaheadMs) * 0.001 * fs); }

        /** MIDI for the MIDI key: positions are sample offsets within the next process() block. */
        void noteOn (int at, float velocity) { pendingOn.push_back ({ at, velocity }); }
        void noteOff (int at) { pendingOff.push_back ({ at, 0 }); }

        /** The static curve for the face: gain change (dB) for a key level x (dB), fully settled. */
        double curveDb (double x) const
        {
            const double R = std::max (0.0, p.rangeDb);
            switch (p.mode)
            {
                case expand: return x >= p.thresholdDb ? 0.0 : std::max (-R, (x - p.thresholdDb) * (std::max (1.0, p.ratio) - 1.0));
                case duck:   return x >= p.thresholdDb ? -R : 0.0;
                default:     return x >= p.thresholdDb ? 0.0 : -R;
            }
        }

        void process (float* const* ch, int nch, const float* const* key, int keyCh, int n)
        {
            if (p.keyHpHz != lastHp) { for (auto& f : hp) f.set (fs, std::max (20.0, p.keyHpHz)); lastHp = p.keyHpHz; }
            if (p.keyLpHz != lastLp) { for (auto& f : lp) f.set (fs, std::min (20000.0, p.keyLpHz)); lastLp = p.keyLpHz; }
            const bool hpOn = p.keyHpHz > 20.5, lpOn = p.keyLpHz < 19999.0;
            const bool stereo = nch > 1;
            const int L = latency();
            const double openThr = p.thresholdDb, closeThr = p.thresholdDb - std::max (0.0, p.hysteresisDb);
            const double stepA = 1.0 / std::max (1.0, p.attackMs * 0.001 * fs), stepR = 1.0 / std::max (1.0, p.releaseMs * 0.001 * fs);
            const int holdN = (int) (p.holdMs * 0.001 * fs);
            const double R = std::max (0.0, p.rangeDb);
            const float outG = (float) dbToGain (p.outputDb);
            float pkKey = -100; double minG = 0;
            for (int s = 0; s < n; ++s)
            {
                // MIDI events for this sample
                for (auto& e : pendingOn) if (e.at == s) { ++midiNotes; midiVel = e.vel; }
                for (auto& e : pendingOff) if (e.at == s) midiNotes = std::max (0, midiNotes - 1);
                const double xl = ch[0][s], xr = stereo ? ch[1][s] : xl;
                double kl = xl, kr = xr;
                if (p.key == sidechain && key != nullptr && keyCh > 0) { kl = key[0][s]; kr = key[std::min (1, keyCh - 1)][s]; }
                double kf[2] = { kl, kr };
                for (int c = 0; c < 2; ++c)
                {
                    double lo, hi;
                    if (hpOn) { hp[(size_t) c].tick (kf[c], lo, hi); kf[c] = hi; }
                    if (lpOn) { lp[(size_t) c].tick (kf[c], lo, hi); kf[c] = lo; }
                }
                const double a = std::max (std::abs (kf[0]), std::abs (kf[1]));
                env = a > env ? env + (a - env) * aEnv : env + (a - env) * relEnv;
                const double keyDb = gainToDb (env * 1.0);
                pkKey = std::max (pkKey, (float) keyDb);
                // open / close decision (with hysteresis and hold)
                bool want;
                if (p.key == midi) want = midiNotes > 0;
                else want = isOpen ? keyDb > closeThr : keyDb > openThr;
                if (want) { isOpen = true; holdLeft = holdN; }
                else if (holdLeft > 0) --holdLeft;
                else isOpen = false;
                // openness ramps by the attack / release times, then the curve shapes it
                open = isOpen ? std::min (1.0, open + stepA) : std::max (0.0, open - stepR);
                const double o = shape (open, p.curve);
                double gDb;
                switch (p.mode)
                {
                    case expand:
                    {
                        const double ex = keyDb >= p.thresholdDb ? 0.0 : std::max (-R, (keyDb - p.thresholdDb) * (std::max (1.0, p.ratio) - 1.0));
                        gDb = (1.0 - o) * ex;   // the envelope still governs how fast it follows
                        break;
                    }
                    case duck: gDb = -R * o; break;
                    default:   gDb = -R * (1.0 - o); break;
                }
                if (p.key == midi && p.velocity && p.mode == gate) gDb += o * gainToDb (std::max (0.05, (double) midiVel));   // softer notes open less
                const double g = gDb <= -120.0 ? 0.0 : (gDb == 0.0 ? 1.0 : dbToGain (gDb));
                minG = std::min (minG, gDb);
                meterOpen = (float) o;
                const double al = L > 0 ? dly[0].process ((float) xl, L) : xl, ar = L > 0 ? dly[1].process ((float) xr, L) : xr;
                double yl = al * g, yr = ar * g;
                if (p.flip) { yl = al - yl; yr = ar - yr; }
                if (p.listenKey) { yl = kf[0]; yr = kf[1]; }
                ch[0][s] = (float) yl * outG;
                if (stereo) ch[1][s] = (float) yr * outG;
            }
            pendingOn.clear(); pendingOff.clear();
            meterKeyDb = pkKey; meterGainDb = (float) minG;
        }

    private:
        double fs = 48000, aEnv = 0, relEnv = 0, env = 0, open = 0, lastHp = -1, lastLp = -1;
        bool isOpen = false; int holdLeft = 0, midiNotes = 0; float midiVel = 1;
        struct Ev { int at; float vel; };
        std::vector<Ev> pendingOn, pendingOff;
        std::array<Svf, 2> hp, lp; std::array<Delay, 2> dly;
    };
}
