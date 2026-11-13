#pragma once
// Apex Multiband 2 - DSP core (1.14 "Studio Line", phase E). No UI, no te:: types: tests/mb2_test.cpp measures it.
//
// Up to six bands, two ways to split:
//   Minimum phase: a Linkwitz-Riley 24 dB/oct tree; every band is passed through the all-passes of the splits above it,
//                  so the bands sum to an all-pass (flat magnitude, no latency of its own).
//   Linear phase:  complementary linear-phase FIRs (Kaiser-windowed sinc low-passes LP1..LPk; band 1 = LP1,
//                  band i = LP(i) - LP(i-1), top band = delta - LPk) run by one partitioned FFT convolver: the bands sum
//                  to the input delayed, exactly. New crossovers are built off the audio thread and cross-faded in.
// Per band: four dynamics modes (compress down, compress up, expand down, expand up), threshold, ratio, range,
// knee, attack, release, make-up gain, a stereo place (stereo / left / right / mid / side), a key (the band itself or
// the same band of the external side-chain), solo and bypass. Detection always runs on a minimum-phase split of the
// key, timed so it leads the audio by exactly the lookahead (0-20 ms) in either phase mode.

#include <JuceHeader.h>
#include "SqApexDsp.h"
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace SqMB2
{
    constexpr int maxBands = 6;
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    enum Mode  { compressDown = 0, compressUp, expandDown, expandUp };
    enum Place { stereo = 0, left, right, mid, side };
    inline const char* modeName (int m)  { static const char* n[4] = { "Compress", "Compress Up", "Expand", "Expand Up" }; return n[juce::jlimit (0, 3, m)]; }
    inline const char* placeName (int p) { static const char* n[5] = { "Stereo", "Left", "Right", "Mid", "Side" }; return n[juce::jlimit (0, 4, p)]; }

    struct Band
    {
        int mode = compressDown, place = stereo, key = 0;   // key 0 = itself, 1 = the external side-chain
        double thrDb = -20, ratio = 2, rangeDb = 12, kneeDb = 6, attackMs = 10, releaseMs = 150, gainDb = 0;
        bool solo = false, bypass = false;
    };
    struct Params
    {
        int numBands = 4, phase = 0, detector = 0;           // phase 0 minimum, 1 linear; detector = Apex::Detector::Type
        std::array<double, maxBands - 1> xover { 120, 500, 2000, 6000, 12000 };
        double lookaheadMs = 0, inputDb = 0, outputDb = 0, mix = 1;
        std::array<Band, maxBands> band {};
    };

    /** The static gain change (dB) of a band for a detector level x (dB): the transfer curve the face draws. */
    inline double curveDb (const Band& b, double x)
    {
        const double T = b.thrDb, W = std::max (0.0, b.kneeDb), R = std::max (1.0, b.ratio), rng = std::max (0.0, b.rangeDb);
        // soft-knee "amount over / under" (quadratic inside the knee)
        auto over  = [&] (double v) { const double d = v - T; if (W <= 0) return std::max (0.0, d); if (d <= -W / 2) return 0.0; if (d >= W / 2) return d; return (d + W / 2) * (d + W / 2) / (2 * W); };
        auto under = [&] (double v) { const double d = T - v; if (W <= 0) return std::max (0.0, d); if (d <= -W / 2) return 0.0; if (d >= W / 2) return d; return (d + W / 2) * (d + W / 2) / (2 * W); };
        switch (b.mode)
        {
            case compressUp:   if (x < -90) return 0.0; return std::min (rng, under (x) * (1.0 - 1.0 / R));
            case expandDown:   return std::max (-rng, -under (x) * (R - 1.0));
            case expandUp:     return std::min (rng, over (x) * (R - 1.0) * 0.25);   // transient lift, a quarter slope so R stays musical
            default:           return std::max (-rng, -over (x) * (1.0 - 1.0 / R));
        }
    }

    /** Magnitude (linear) of band k of n at frequency f, for the face (minimum-phase LR4 and linear-phase have the same shape). */
    inline double bandMag (const Params& p, int k, double f)
    {
        const int n = juce::jlimit (1, maxBands, p.numBands);
        double m = 1;
        if (k > 0)     { const double r = std::pow (f / p.xover[(size_t) k - 1], 4.0); m *= r / (1 + r); }
        if (k < n - 1) { const double r = std::pow (f / p.xover[(size_t) k], 4.0); m *= 1 / (1 + r); }
        return m;
    }

    /** Sorted, separated crossovers (a third of an octave apart, below 0.45 fs). */
    inline std::array<double, maxBands - 1> sanitise (const Params& p, double fs)
    {
        std::array<double, maxBands - 1> x = p.xover;
        const int n = juce::jlimit (1, maxBands, p.numBands);
        for (int i = 0; i < n - 1; ++i)
        {
            x[(size_t) i] = juce::jlimit (20.0, fs * 0.45, x[(size_t) i]);
            if (i > 0) x[(size_t) i] = std::max (x[(size_t) i], x[(size_t) i - 1] * 1.26);
            x[(size_t) i] = std::min (x[(size_t) i], fs * 0.45);
        }
        return x;
    }

    /** Complementary linear-phase band filters for n bands (sum = delta at the centre tap). */
    inline std::vector<std::vector<float>> designLinear (const std::array<double, maxBands - 1>& xo, int n, double fs, int M)
    {
        const int c = (M - 1) / 2; const double beta = 8.6;
        auto bessel0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        std::vector<std::vector<double>> lp ((size_t) std::max (0, n - 1), std::vector<double> ((size_t) M));
        for (int i = 0; i < n - 1; ++i)
        {
            const double fc = xo[(size_t) i] / fs; double sum = 0;
            for (int t = 0; t < M; ++t)
            {
                const double k = t - c, w = bessel0 (beta * std::sqrt (std::max (0.0, 1 - (k / c) * (k / c)))) / bessel0 (beta);
                const double h = k == 0 ? 2 * fc : std::sin (2 * pi * fc * k) / (pi * k);
                lp[(size_t) i][(size_t) t] = h * w; sum += h * w;
            }
            for (auto& v : lp[(size_t) i]) v /= sum;   // exact unity at DC
        }
        std::vector<std::vector<float>> b ((size_t) n, std::vector<float> ((size_t) M, 0.0f));
        for (int t = 0; t < M; ++t)
        {
            double prev = 0;
            for (int k = 0; k < n; ++k)
            {
                const double cur = k < n - 1 ? lp[(size_t) k][(size_t) t] : (t == c ? 1.0 : 0.0);
                b[(size_t) k][(size_t) t] = (float) (cur - prev); prev = cur;
            }
        }
        return b;
    }

    /** A ring delay with a variable length (whole samples). */
    struct Delay
    {
        std::vector<float> b; int mask = 0, w = 0;
        void prepare (int maxN) { int s = 1; while (s < maxN + 2) s <<= 1; b.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        inline float process (float x, int d) { b[(size_t) (w & mask)] = x; const float y = b[(size_t) ((w - d) & mask)]; ++w; return y; }
    };

    /** The minimum-phase split of one channel into n bands (LR4 tree + compensating all-passes). */
    struct MinSplit
    {
        std::array<Apex::LR4, maxBands - 1> xo; std::array<std::array<Apex::LR4, maxBands - 1>, maxBands> ap;
        void set (double fs, const std::array<double, maxBands - 1>& x, int n)
        { for (int i = 0; i < n - 1; ++i) { xo[(size_t) i].set (fs, x[(size_t) i]); for (int k = 0; k < maxBands; ++k) ap[(size_t) k][(size_t) i].set (fs, x[(size_t) i]); } }
        void reset() { for (auto& f : xo) f.reset(); for (auto& r : ap) for (auto& f : r) f.reset(); }
        inline void split (double x, double* band, int n)
        {
            double rest = x;
            for (int k = 0; k < n - 1; ++k)
            {
                double lo, hi; xo[(size_t) k].split (rest, lo, hi);
                for (int j = k + 1; j < n - 1; ++j) lo = ap[(size_t) k][(size_t) j].allpass (lo);
                band[k] = lo; rest = hi;
            }
            band[n - 1] = rest;
        }
    };

    /** Linear-phase split of both channels: one convolver per channel holding the n band filters. */
    struct LinSplit
    {
        std::array<Apex::MultiFir, 2> fir; int n = 0, firLatency = 0;
        std::array<double, maxBands - 1> x {};
        void build (const std::array<double, maxBands - 1>& xo, int bands, double fs, int block)
        {
            const int M = fs > 60000 ? 8191 : 4095;
            const auto irs = designLinear (xo, bands, fs, M);
            for (auto& f : fir) f.prepare (block, irs);
            n = bands; x = xo; firLatency = block + (M - 1) / 2;
        }
    };

    class Engine
    {
    public:
        Params p;
        // meters (audio thread writes)
        std::array<std::atomic<float>, maxBands> meterGrDb {}, meterLevelDb {};

        static constexpr int firBlock = 256;
        static int linearLatency (double fs) { return firBlock + ((fs > 60000 ? 8191 : 4095) - 1) / 2; }

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            const int maxLook = (int) std::ceil (0.020 * fs) + 2, maxLat = linearLatency (fs) + maxLook + 8;
            for (auto& d : detDelay) d.prepare (maxLat);
            for (auto& d : keyDelay) d.prepare (maxLat);
            for (auto& d : dryDelay) d.prepare (maxLat);
            for (auto& ch : bandDelay) for (auto& d : ch) d.prepare (maxLat);
            for (auto& d : det) d.prepare (fs);
            for (auto& d : keyDet) d.prepare (fs);
            lastX.fill (-1); lastN = -1; lastPhase = -1;
            lin.reset(); linNext.reset(); pending.store (nullptr);
            update (true);
            reqN = -1;
            if (p.phase == 1) { auto l = std::make_unique<LinSplit>(); l->build (curX, curN, fs, firBlock); lin = std::move (l); reqN = curN; reqX = curX; }
            reset();
        }

        void reset()
        {
            for (auto& s : audioSplit) s.reset(); for (auto& s : detSplit) s.reset(); for (auto& s : keySplit) s.reset();
            for (auto& d : detDelay) d.clear(); for (auto& d : keyDelay) d.clear(); for (auto& d : dryDelay) d.clear();
            for (auto& ch : bandDelay) for (auto& d : ch) d.clear();
            for (auto& b : bal) b.state = 0;
            for (auto& d : det) d.reset(); for (auto& d : keyDet) d.reset();
        }

        /** Built off the audio thread; the audio thread cross-fades to it. Returns true when a rebuild was needed. */
        bool buildLinearIfNeeded() { return buildLinearIfNeeded (p); }
        bool buildLinearIfNeeded (const Params& q)   // q: a copy of the settings, so the message thread never touches p
        {
            if (q.phase != 1) return false;
            const auto x = sanitise (q, fs); const int n = juce::jlimit (1, maxBands, q.numBands);
            // compare with the last request (message-thread state only: never look at what the audio thread owns)
            if (reqN == n && sameX (reqX, x, n)) return false;
            reqN = n; reqX = x;
            auto l = new LinSplit(); l->build (x, n, fs, firBlock);
            if (auto* old = pending.exchange (l)) delete old;
            return true;
        }

        int latency() const
        {
            const int L = lookN();
            return p.phase == 1 ? std::max (linearLatency (fs), L) : L;
        }

        void process (float* const* ch, int nch, const float* const* key, int keyCh, int n)
        {
            update (false);
            // adopt a newly built linear split (cross-fade)
            if (p.phase == 1)
            {
                if (auto* q = pending.exchange (nullptr))
                {
                    if (lin == nullptr) lin.reset (q);
                    else { linNext.reset (q); xfade = -2 * q->firLatency; }   // let the new convolver fill before fading to it
                }
                if (lin == nullptr) { auto* l = new LinSplit(); l->build (curX, curN, fs, firBlock); lin.reset (l); }   // first use: build now
            }
            const int nb = curN, L = lookN(), A = latency();
            const bool linear = p.phase == 1 && lin != nullptr;
            const int audioExtra = linear ? A - lin->firLatency : A;   // min phase: the lookahead; linear: whatever is beyond the FIR delay
            const int detD = A - L;
            const float inG = (float) dbToGain (p.inputDb), outG = (float) dbToGain (p.outputDb);
            const float mix = (float) juce::jlimit (0.0, 1.0, p.mix);
            bool anySolo = false; for (int k = 0; k < nb; ++k) anySolo = anySolo || p.band[(size_t) k].solo;
            double bandGain[maxBands]; for (int k = 0; k < nb; ++k) bandGain[k] = dbToGain (p.band[(size_t) k].gainDb);
            float pkGr[maxBands] {}, pkLv[maxBands]; for (auto& v : pkLv) v = -100.0f;
            const bool haveKey = key != nullptr && keyCh > 0;

            for (int s = 0; s < n; ++s)
            {
                float x[2] = { ch[0][s] * inG, (nch > 1 ? ch[1][s] : ch[0][s]) * inG };
                // the dry path, timed to the output
                float dry[2] = { dryDelay[0].process (x[0], A), dryDelay[1].process (x[1], A) };
                // detection: minimum-phase split of the (delayed) input and key
                double dB[2][maxBands], kB[2][maxBands];
                for (int c = 0; c < 2; ++c) detSplit[(size_t) c].split (detDelay[(size_t) c].process (x[c], detD), dB[c], nb);
                if (haveKey)
                    for (int c = 0; c < 2; ++c)
                    {
                        const float kx = key[std::min (c, keyCh - 1)][s];
                        keySplit[(size_t) c].split (keyDelay[(size_t) c].process (kx, detD), kB[c], nb);
                    }
                // audio bands
                double aB[2][maxBands];
                if (linear)
                {
                    float y[maxBands], y2[maxBands];
                    for (int c = 0; c < 2; ++c)
                    {
                        lin->fir[(size_t) c].process (x[c], y);
                        if (linNext != nullptr)
                        {
                            linNext->fir[(size_t) c].process (x[c], y2);
                            const float t = (float) std::max (0, xfade) / xfadeLen;
                            for (int k = 0; k < nb; ++k) y[k] = y[k] * (1 - t) + y2[k] * t;
                        }
                        for (int k = 0; k < nb; ++k) aB[c][k] = audioExtra > 0 ? bandDelay[(size_t) c][(size_t) k].process (y[k], audioExtra) : y[k];
                    }
                    if (linNext != nullptr && ++xfade >= xfadeLen) { lin = std::move (linNext); }
                }
                else
                    for (int c = 0; c < 2; ++c)
                    {
                        double tmp[maxBands]; audioSplit[(size_t) c].split (x[c], tmp, nb);
                        for (int k = 0; k < nb; ++k) aB[c][k] = L > 0 ? bandDelay[(size_t) c][(size_t) k].process ((float) tmp[k], L) : tmp[k];
                    }
                // dynamics per band
                double out[2] = { 0, 0 };
                for (int k = 0; k < nb; ++k)
                {
                    const auto& b = p.band[(size_t) k];
                    const double (*src)[maxBands] = (b.key == 1 && haveKey) ? kB : dB;
                    float level;
                    switch (b.place)
                    {
                        case left:  level = det[(size_t) k].process ((float) src[0][k], p.detector); break;
                        case right: level = det[(size_t) k].process ((float) src[1][k], p.detector); break;
                        case mid:   level = det[(size_t) k].process ((float) (0.5 * (src[0][k] + src[1][k])), p.detector); break;
                        case side:  level = det[(size_t) k].process ((float) (0.5 * (src[0][k] - src[1][k])), p.detector); break;
                        default:
                        {   // stereo: linked on the louder side (one detector each, max)
                            const float l0 = det[(size_t) k].process ((float) src[0][k], p.detector), l1 = keyDet[(size_t) k].process ((float) src[1][k], p.detector);
                            level = std::max (l0, l1);
                        }
                    }
                    const double target = b.bypass ? 0.0 : curveDb (b, level);
                    const double grDb = bal[(size_t) k].process ((float) target);
                    const double g = dbToGain (grDb) * (b.bypass ? 1.0 : bandGain[k]);
                    pkGr[k] = std::abs (grDb) > std::abs (pkGr[k]) ? (float) grDb : pkGr[k];
                    pkLv[k] = std::max (pkLv[k], level);
                    double l = aB[0][k], r = aB[1][k];
                    switch (b.place)
                    {
                        case left:  l *= g; break;
                        case right: r *= g; break;
                        case mid:   { const double m = 0.5 * (l + r) * g, sd = 0.5 * (l - r); l = m + sd; r = m - sd; break; }
                        case side:  { const double m = 0.5 * (l + r), sd = 0.5 * (l - r) * g; l = m + sd; r = m - sd; break; }
                        default:    l *= g; r *= g;
                    }
                    if (anySolo && ! b.solo) continue;
                    out[0] += l; out[1] += r;
                }
                const float y0 = (float) out[0] * mix + dry[0] * (1 - mix), y1 = (float) out[1] * mix + dry[1] * (1 - mix);
                ch[0][s] = y0 * outG;
                if (nch > 1) ch[1][s] = y1 * outG;
            }
            for (int k = 0; k < maxBands; ++k) { meterGrDb[(size_t) k] = k < nb ? pkGr[k] : 0.0f; meterLevelDb[(size_t) k] = k < nb ? pkLv[k] : -100.0f; }
        }

    private:
        double fs = 48000;
        std::array<MinSplit, 2> audioSplit, detSplit, keySplit;
        std::array<Delay, 2> detDelay, keyDelay, dryDelay;
        std::array<std::array<Delay, maxBands>, 2> bandDelay;
        std::array<Apex::Detector, maxBands> det, keyDet;   // keyDet: the second (right-hand) detector of a stereo-linked band
        std::array<Apex::Ballistics, maxBands> bal;
        std::unique_ptr<LinSplit> lin, linNext; std::atomic<LinSplit*> pending { nullptr };
        int xfade = 0; static constexpr int xfadeLen = 1024;
        int reqN = -1; std::array<double, maxBands - 1> reqX {};
        std::array<double, maxBands - 1> lastX {}, curX {}; int lastN = -1, curN = 4, lastPhase = -1;
        std::array<double, maxBands> lastAtt {}, lastRel {};

        static bool sameX (const std::array<double, maxBands - 1>& a, const std::array<double, maxBands - 1>& b, int n)
        { for (int i = 0; i < n - 1; ++i) if (std::abs (a[(size_t) i] - b[(size_t) i]) > 0.01) return false; return true; }
        int lookN() const { return (int) std::lround (juce::jlimit (0.0, 20.0, p.lookaheadMs) * 0.001 * fs); }

        void update (bool force)
        {
            curN = juce::jlimit (1, maxBands, p.numBands);
            curX = sanitise (p, fs);
            if (force || curN != lastN || ! sameX (curX, lastX, curN))
            {
                for (auto* s : { &audioSplit, &detSplit, &keySplit }) for (auto& m : *s) m.set (fs, curX, curN);
                if (force || curN != lastN) for (auto* s : { &audioSplit, &detSplit, &keySplit }) for (auto& m : *s) m.reset();
                lastX = curX; lastN = curN;
            }
            for (int k = 0; k < maxBands; ++k)
            {
                const auto& b = p.band[(size_t) k];
                if (force || b.attackMs != lastAtt[(size_t) k] || b.releaseMs != lastRel[(size_t) k])
                { bal[(size_t) k].set (fs, (float) b.attackMs, (float) b.releaseMs, 0.0f); lastAtt[(size_t) k] = b.attackMs; lastRel[(size_t) k] = b.releaseMs; }
            }
            lastPhase = p.phase;
        }
    };
}
