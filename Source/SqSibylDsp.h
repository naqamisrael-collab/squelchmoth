#pragma once
// Sibyl De-Ess - DSP core (1.14 "Studio Line", phase F). No UI, no te:: types: tests/sibyl_test.cpp measures it.
//
// Detection: the key (the input, or the side-chain) through a 4th-order band-pass between Low and High (or, in Track
// mode, a resonant band that follows the loudest of eight probes across that range). Its envelope is compared with the
// threshold either ABSOLUTELY (dBFS) or RELATIVE to the whole signal's envelope, so a whisper and a shout are treated
// alike. The gain change has a soft knee, a range limit, a fixed 0.5 ms attack and a programme-dependent release.
// Processing: SPLIT - a bell spanning Low-High (or at the tracked frequency) cut by exactly the gain change: minimum
//                     phase, so the cut is clean (no residue), and exactly the input when idle;
//             WIDE  - the whole signal is turned down.
// Lookahead delays the audio (not the detector) by up to 15 ms. Stereo: linked, mid only, or side only.
// Listen: the detection band alone, or exactly what is being removed (removed + output = input).

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace SqSibyl
{
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    enum Shape  { split = 0, wide };
    enum Detect { absolute = 0, relative };
    enum Stereo { linked = 0, midOnly, sideOnly };
    enum Listen { normal = 0, detection, removed };

    struct Params
    {
        int shape = split, detect = relative, stereo = linked, listen = normal;
        bool track = false, sidechain = false;
        double thresholdDb = -20, rangeDb = 10, kneeDb = 6, releaseMs = 60, lookaheadMs = 2;
        double lowHz = 5000, highHz = 10000, outputDb = 0;
    };

    /** TPT state-variable filter (exact frequency response, safe to modulate). */
    struct Svf
    {
        double ic1 = 0, ic2 = 0, g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0;
        void set (double fs, double hz, double q)
        {
            g = std::tan (pi * juce::jlimit (10.0, fs * 0.49, hz) / fs);
            k = 1.0 / std::max (0.05, q); a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        inline void tick (double v0, double& lp, double& bp, double& hp)
        {
            const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
        inline double lowpass (double x)  { double l, b, h; tick (x, l, b, h); return l; }
        inline double highpass (double x) { double l, b, h; tick (x, l, b, h); return h; }
        inline double bandpass (double x) { double l, b, h; tick (x, l, b, h); return b * k; }   // unity at the centre
        void reset() { ic1 = ic2 = 0; }
    };

    /** The band: 4th-order (two Butterworth high-passes at Low, two low-passes at High). */
    struct Band
    {
        Svf h1, h2, l1, l2;
        void set (double fs, double lo, double hi) { h1.set (fs, lo, 0.5412); h2.set (fs, lo, 1.3066); l1.set (fs, hi, 0.5412); l2.set (fs, hi, 1.3066); }
        inline double process (double x) { return l2.lowpass (l1.lowpass (h2.highpass (h1.highpass (x)))); }
        void reset() { h1.reset(); h2.reset(); l1.reset(); l2.reset(); }
        /** |H| at f (analytic: Butterworth 4th-order high-pass x low-pass). */
        static double mag (double f, double lo, double hi)
        { const double a = std::pow (f / lo, 4.0), b = std::pow (f / hi, 4.0); return std::sqrt (a / (1 + a)) * std::sqrt (1 / (1 + b)); }
    };

    /** A modulatable bell (TPT SVF): exactly the given gain at fc, exactly the input at 0 dB. Minimum phase, so cutting
        never leaves a phase-shifted residue the way subtracting a band-pass does. */
    struct Bell
    {
        double ic1 = 0, ic2 = 0, g = 0, q = 1.4;
        void set (double fs, double fc, double Q) { g = std::tan (pi * juce::jlimit (10.0, fs * 0.49, fc) / fs); q = std::max (0.2, Q); }
        void reset() { ic1 = ic2 = 0; }
        inline double process (double x, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), k = 1.0 / (q * A);
            const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            return gainDb == 0.0 ? x : x + k * (A * A - 1.0) * v1;
        }
        /** |H| in dB at f for a bell at fc, quality Q, gain gDb. */
        static double magDb (double f, double fc, double Q, double gDb)
        {
            const double A = std::pow (10.0, gDb / 40.0), w = f / fc;
            const std::complex<double> j (0, 1), num = -w * w + j * w * (A / Q) + 1.0, den = -w * w + j * w / (A * Q) + 1.0;
            return 20.0 * std::log10 (std::abs (num / den));
        }
    };

    struct Delay
    {
        std::vector<float> b; int mask = 0, w = 0;
        void prepare (int n) { int s = 1; while (s < n + 2) s <<= 1; b.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        inline float process (float x, int d) { b[(size_t) (w & mask)] = x; const float y = b[(size_t) ((w - d) & mask)]; ++w; return y; }
    };

    /** The static gain change (dB, <= 0) for a detector reading x against threshold T (both dB). */
    inline double curveDb (double x, double T, double knee, double range)
    {
        const double W = std::max (0.0, knee), d = x - T;
        double over;
        if (W <= 0) over = std::max (0.0, d);
        else if (d <= -W / 2) over = 0;
        else if (d >= W / 2) over = d;
        else over = (d + W / 2) * (d + W / 2) / (2 * W);
        return -std::min (std::max (0.0, range), over * 0.85);   // 0.85: ~6.7:1, firm but never a gate
    }

    class Engine
    {
    public:
        Params p;
        float meterGrDb = 0, meterDetDb = -100, meterTrackHz = 7000, meterInDb = -100;

        static constexpr int numProbes = 8;

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            const int maxL = (int) std::ceil (0.015 * fs) + 4;
            for (auto& d : dly) d.prepare (maxL);
            aAtt = 1.0 - std::exp (-1.0 / (0.0005 * fs));
            aFull = 1.0 - std::exp (-1.0 / (0.005 * fs));
            aTrack = 1.0 - std::exp (-1.0 / (0.004 * fs));
            last = Params(); last.lowHz = -1;
            update (true); reset();
        }
        void reset()
        {
            for (auto& b : bell) b.reset();
            for (auto& b : bandA) b.reset(); for (auto& b : bandD) b.reset(); for (auto& s : trackA) s.reset(); for (auto& s : trackD) s.reset();
            for (auto& s : probes) s.reset();
            for (auto& d : dly) d.clear();
            envBand = envFull = 0; grState = 0; probeEnv.fill (0); trackLog = std::log (std::sqrt (p.lowHz * p.highHz));
        }
        int latency() const { return (int) std::lround (juce::jlimit (0.0, 15.0, p.lookaheadMs) * 0.001 * fs); }

        /** Live gain change and the cut's shape (for the face): |1 - (1-g) H(f)| in dB at f. */
        double cutShapeDb (double f, double grDb) const
        {
            if (p.shape == wide) return grDb;
            return Bell::magDb (f, p.track ? (double) meterTrackHz : bellFc(), p.track ? 2.0 : bellQ(), grDb);
        }

        void process (float* const* ch, int nch, const float* const* key, int keyCh, int n)
        {
            update (false);
            const bool stereo = nch > 1;
            const int L = latency();
            const double thr = p.thresholdDb, aRel = 1.0 - std::exp (-1.0 / (std::max (5.0, p.releaseMs) * 0.001 * fs));
            const double aRelFast = 1.0 - std::exp (-1.0 / (std::max (5.0, p.releaseMs * 0.35) * 0.001 * fs));
            const float outG = (float) dbToGain (p.outputDb);
            const bool useKey = p.sidechain && key != nullptr && keyCh > 0;
            float pkGr = 0, pkDet = -100, pkIn = 0;
            for (int s = 0; s < n; ++s)
            {
                const double xl = ch[0][s], xr = stereo ? ch[1][s] : xl;
                pkIn = std::max (pkIn, (float) std::max (std::abs (xl), std::abs (xr)));
                // the detector's signal: the key (input or side-chain), in the stereo mode's channel
                double kl = xl, kr = xr;
                if (useKey) { kl = key[0][s]; kr = key[std::min (1, keyCh - 1)][s]; }
                const double kd = p.stereo == midOnly ? 0.5 * (kl + kr) : p.stereo == sideOnly ? 0.5 * (kl - kr) : (std::abs (kl) > std::abs (kr) ? kl : kr);
                // track: eight probes; the band follows the loudest (in log-frequency, smoothed)
                if (p.track)
                {
                    int best = 0;
                    for (int i = 0; i < numProbes; ++i)
                    {
                        const double y = std::abs (probes[(size_t) i].bandpass (kd));
                        probeEnv[(size_t) i] += (y - probeEnv[(size_t) i]) * aTrack;
                        if (probeEnv[(size_t) i] > probeEnv[(size_t) best]) best = i;
                    }
                    if (probeEnv[(size_t) best] > 1e-6)
                    {
                        // parabolic interpolation between neighbouring probes for a continuous estimate
                        double pos = best;
                        if (best > 0 && best < numProbes - 1)
                        {
                            const double a = probeEnv[(size_t) best - 1], b = probeEnv[(size_t) best], c = probeEnv[(size_t) best + 1], den = a - 2 * b + c;
                            if (std::abs (den) > 1e-12) pos += juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / den);
                        }
                        const double target = std::log (probeHz (pos));
                        trackLog += (target - trackLog) * 0.0015;
                    }
                    if ((s & 15) == 0)
                    {
                        const double fc = std::exp (trackLog);
                        for (auto& f : trackD) f.set (fs, fc, 2.0);
                        for (auto& f : trackA) f.set (fs, fc, 2.0);
                        for (auto& b : bell) b.set (fs, fc, 2.0);
                    }
                }
                const double db = p.track ? trackD[0].bandpass (kd) : bandD[0].process (kd);
                // envelopes: the band (0.5 ms attack, programme-dependent release) and the whole key (5 ms)
                const double ab = std::abs (db), af = std::abs (kd);
                envBand += (ab - envBand) * (ab > envBand ? aAtt : (envBand > 2.0 * ab ? aRelFast : aRel));
                envFull += (af - envFull) * aFull;
                const double bandDb = gainToDb (envBand * 1.4142);   // sine-peak referenced
                const double level = p.detect == relative ? bandDb - gainToDb (envFull * 1.4142) : bandDb;
                const double target = (p.detect == relative && bandDb < -80) ? 0.0 : curveDb (level, thr, p.kneeDb, p.rangeDb);
                grState += (target - grState) * (target < grState ? aAtt : aRel);
                if (std::abs (grState) < 1e-7 && target == 0) grState = 0;   // idle: exactly unity (bit-exact pass-through)
                const double g = grState == 0 ? 1.0 : dbToGain (grState);
                pkGr = std::min (pkGr, (float) grState); pkDet = std::max (pkDet, (float) level);
                // the audio, delayed by the lookahead
                const double al = L > 0 ? dly[0].process ((float) xl, L) : xl, ar = L > 0 ? dly[1].process ((float) xr, L) : xr;
                // the band to act on (split shape) per channel - in the stereo mode's domain
                double ol, orr, bl = 0, br = 0;
                auto bandOf = [&] (int c, double v) { return p.track ? trackA[(size_t) c].bandpass (v) : bandA[(size_t) c].process (v); };   // for listening only
                const double gdb = g == 1.0 ? 0.0 : grState;
                if (p.stereo == linked)
                {
                    bl = bandOf (0, al); br = bandOf (1, ar);
                    if (p.shape == wide) { ol = g == 1.0 ? al : al * g; orr = g == 1.0 ? ar : ar * g; }
                    else { ol = bell[0].process (al, gdb); orr = bell[1].process (ar, gdb); }
                }
                else
                {
                    const double m = 0.5 * (al + ar), sd = 0.5 * (al - ar);
                    double tgt = p.stereo == midOnly ? m : sd;
                    const double bb = bandOf (0, tgt); bl = br = 0;
                    double processed = p.shape == wide ? tgt * g : bell[0].process (tgt, gdb);
                    if (g == 1.0) processed = tgt;
                    const double mm = p.stereo == midOnly ? processed : m, ss = p.stereo == sideOnly ? processed : sd;
                    ol = g == 1.0 ? al : mm + ss; orr = g == 1.0 ? ar : mm - ss;
                    if (p.listen == detection) { bl = p.stereo == midOnly ? bb : bb; br = p.stereo == midOnly ? bb : -bb; }
                }
                double yl = ol, yr = orr;
                if (p.listen == detection) { yl = bl; yr = br; }
                else if (p.listen == removed) { yl = al - ol; yr = ar - orr; }
                ch[0][s] = (float) yl * outG;
                if (stereo) ch[1][s] = (float) yr * outG;
            }
            meterGrDb = pkGr; meterDetDb = pkDet; meterInDb = (float) gainToDb (pkIn); meterTrackHz = (float) std::exp (trackLog);
        }

        double probeHz (double i) const { const double lo = p.lowHz, hi = std::max (p.lowHz * 1.05, p.highHz); return lo * std::pow (hi / lo, i / (numProbes - 1)); }

    private:
        double fs = 48000, aAtt = 0, aFull = 0, aTrack = 0, envBand = 0, envFull = 0, grState = 0, trackLog = std::log (7000.0);
        std::array<Band, 2> bandA; std::array<Band, 1> bandD; std::array<Svf, 2> trackA; std::array<Svf, 1> trackD;
        std::array<Svf, numProbes> probes; std::array<double, numProbes> probeEnv {};
        std::array<Delay, 2> dly; std::array<Bell, 2> bell;
        Params last;
        double bellFc() const { return std::sqrt (p.lowHz * std::max (p.lowHz * 1.05, p.highHz)); }
        double bellQ() const { return juce::jlimit (0.5, 8.0, bellFc() / std::max (1.0, std::max (p.lowHz * 1.05, p.highHz) - p.lowHz)); }

        void update (bool force)
        {
            const double lo = juce::jlimit (1000.0, 16000.0, p.lowHz), hi = juce::jlimit (lo * 1.05, std::min (20000.0, fs * 0.45), p.highHz);
            if (force || lo != last.lowHz || hi != last.highHz || p.track != last.track)
            {
                for (auto& b : bandA) b.set (fs, lo, hi); for (auto& b : bandD) b.set (fs, lo, hi);
                for (int i = 0; i < numProbes; ++i) probes[(size_t) i].set (fs, lo * std::pow (hi / lo, i / (double) (numProbes - 1)), 2.5);
                if (! p.track || force) { const double fc = std::sqrt (lo * hi); trackLog = std::log (fc); for (auto& f : trackA) f.set (fs, fc, 2.0); for (auto& f : trackD) f.set (fs, fc, 2.0); }
                if (! p.track) { const double fc = std::sqrt (lo * hi); for (auto& b : bell) b.set (fs, fc, juce::jlimit (0.5, 8.0, fc / (hi - lo))); }
                last.lowHz = lo; last.highHz = hi; last.track = p.track;
            }
        }
    };
}
