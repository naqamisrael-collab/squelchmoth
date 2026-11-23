#pragma once
// SqLoudness: loudness and true peak as ITU-R BS.1770-5 defines them, with the meter behaviour EBU Tech 3341 (v4)
// and the loudness range EBU Tech 3342 (v4) require. Pure C++ (no JUCE).
//
//  * K-weighting: the two filters of BS.1770 Annex 1, worked out for any sample rate from the design the 48 kHz table
//    comes from (at 48 kHz they match the table; tests/loudness_test.cpp checks it).
//  * Loudness = -0.691 + 10 log10 (sum over channels of G x mean square), G = 1 for L, R, C and 1.41 for Ls, Rs.
//  * Momentary: 0.4 s window; short-term: 3 s window - both sliding a sample at a time, so the highest value a
//    file reaches is found wherever it falls (Tech 3341's file-based cases need that), and neither is smoothed.
//  * Integrated: 400 ms blocks every 100 ms, absolute gate -70 LUFS, relative gate 10 LU under the absolute-gated
//    energy mean; an unfinished block at the end is left out.
//  * Loudness range: short-term values every 100 ms, absolute gate -70, relative gate 20 LU under their energy mean,
//    then the 95th minus the 10th percentile (Tech 3342's rounding of the index).
//  * True peak: four times oversampled with the 48-tap interpolator printed in BS.1770 Annex 2 (exact values).
#include <algorithm>
#include <cmath>
#include <vector>

namespace SqLoudness
{
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    /** The two K-weighting stages for a sample rate (BS.1770's own design parameters). */
    inline void kWeighting (double fs, Biquad& shelf, Biquad& hp)
    {
        const double pi = 3.14159265358979323846;
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = std::tan (pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
            shelf = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (pi * f0 / fs), a0 = 1.0 + K / Q + K * K;
            hp = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
    }
    /** BS.1770-5 Annex 2: the interpolator, phase by phase, 12 taps each. */
    static const double truePeakTaps[4][12] = {
        { 0.0017089843750, 0.0109863281250, -0.0196533203125, 0.0332031250000, -0.0594482421875, 0.1373291015625, 0.9721679687500, -0.1022949218750, 0.0476074218750, -0.0266113281250, 0.0148925781250, -0.0083007812500 },
        { -0.0291748046875, 0.0292968750000, -0.0517578125000, 0.0891113281250, -0.1665039062500, 0.4650878906250, 0.7797851562500, -0.2003173828125, 0.1015625000000, -0.0582275390625, 0.0330810546875, -0.0189208984375 },
        { -0.0189208984375, 0.0330810546875, -0.0582275390625, 0.1015625000000, -0.2003173828125, 0.7797851562500, 0.4650878906250, -0.1665039062500, 0.0891113281250, -0.0517578125000, 0.0292968750000, -0.0291748046875 },
        { -0.0083007812500, 0.0148925781250, -0.0266113281250, 0.0476074218750, -0.1022949218750, 0.9721679687500, 0.1373291015625, -0.0594482421875, 0.0332031250000, -0.0196533203125, 0.0109863281250, 0.0017089843750 } };

    inline double toLufs (double energy) { return energy > 1.0e-30 ? -0.691 + 10.0 * std::log10 (energy) : -200.0; }

    class Meter
    {
    public:
        /** channels: 1 to 5, in the order L, R, C, Ls, Rs (a mono file is one channel). */
        void prepare (double sampleRate, int channelCount)
        {
            fs = sampleRate; nch = std::max (1, std::min (5, channelCount)); for (int c = 0; c < 5; ++c) weight[c] = c >= 3 ? 1.41 : 1.0;
            Biquad s, h; kWeighting (fs, s, h); for (int c = 0; c < 5; ++c) { shelf[c] = s; hp[c] = h; }
            mLen = (int) std::lround (0.4 * fs); sLen = (int) std::lround (3.0 * fs); sub = (int) std::lround (0.1 * fs); reset();
        }
        void reset()
        {
            for (auto& z : st) { z[0] = State(); z[1] = State(); } ring.assign ((size_t) sLen, 0.0); pos = 0; filled = 0; mSum = sSum = 0.0; mMax = sMax = 0.0; mNow = sNow = 0.0; sinceExact = 0;
            subAcc = 0.0; subCount = 0; subs.clear(); blocks.clear(); shorts.clear(); for (auto& h : tpHist) for (auto& v : h) v = 0.0; tpPos = 0; peak = 0.0; samples = 0;
        }
        /** One frame: x[c] for each channel. */
        void push (const float* x)
        {
            double e = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                const double v = x[c]; double y = run (shelf[c], st[c][0], v); y = run (hp[c], st[c][1], y); e += weight[c] * y * y;
                auto& hst = tpHist[c]; hst[(size_t) tpPos] = hst[(size_t) tpPos + 12] = v; const double* h = hst + tpPos + 12;       // h[0] newest
                for (int p = 0; p < 4; ++p) { double acc = 0.0; for (int k = 0; k < 12; ++k) acc += truePeakTaps[p][k] * h[-k]; peak = std::max (peak, std::abs (acc)); }
            }
            if (++tpPos >= 12) tpPos = 0;
            // the sliding windows: the newest energy in, the one that falls out of each window out
            const double outS = filled >= sLen ? ring[(size_t) pos] : 0.0; const int mOld = pos - mLen + (pos - mLen < 0 ? sLen : 0); const double outM = filled >= mLen ? ring[(size_t) mOld] : 0.0;
            ring[(size_t) pos] = e; sSum += e - outS; mSum += e - outM; if (++pos >= sLen) pos = 0; ++filled; if (filled > sLen) filled = sLen; ++samples;
            if (++sinceExact >= sLen) exact();
            if (samples >= mLen) { mNow = std::max (0.0, mSum) / mLen; mMax = std::max (mMax, mNow); }
            if (samples >= sLen) { sNow = std::max (0.0, sSum) / sLen; sMax = std::max (sMax, sNow); }
            subAcc += e; if (++subCount >= sub) closeSubBlock();
        }
        /** Interleaved or planar helpers. */
        void pushPlanar (const float* const* ch, int n) { float f[5]; for (int i = 0; i < n; ++i) { for (int c = 0; c < nch; ++c) f[c] = ch[c][i]; push (f); } }

        double momentary() const { return samples >= mLen ? toLufs (mNow) : -200.0; }
        double shortTerm() const { return samples >= sLen ? toLufs (sNow) : -200.0; }
        double maxMomentary() const { return toLufs (mMax); }
        double maxShortTerm() const { return toLufs (sMax); }
        void clearMaxima() { mMax = sMax = 0.0; }
        double truePeakDb() const { return peak > 0.0 ? 20.0 * std::log10 (peak) : -200.0; }
        double integrated() const
        {
            double s = 0.0; int n = 0; for (double b : blocks) if (toLufs (b) > -70.0) { s += b; ++n; } if (n == 0) return -200.0;
            const double gate = toLufs (s / n) - 10.0; double s2 = 0.0; int n2 = 0; for (double b : blocks) if (toLufs (b) > -70.0 && toLufs (b) > gate) { s2 += b; ++n2; }
            return n2 > 0 ? toLufs (s2 / n2) : -200.0;
        }
        double range() const
        {
            std::vector<double> v; double s = 0.0; for (double e : shorts) { const double l = toLufs (e); if (l >= -70.0) { v.push_back (l); s += e; } } if (v.size() < 2) return 0.0;
            const double gate = toLufs (s / (double) v.size()) - 20.0; std::vector<double> w; for (double l : v) if (l >= gate) w.push_back (l); if (w.size() < 2) return 0.0;
            std::sort (w.begin(), w.end()); auto at = [&] (double pct) { return w[(size_t) std::lround ((double) (w.size() - 1) * pct / 100.0)]; };
            return at (95.0) - at (10.0);
        }
        double seconds() const { return samples / fs; }

    private:
        struct State { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
        static double run (const Biquad& c, State& s, double x) { const double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2; s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y; return y; }
        /** The running sums drift a little over hours: worked out again from the ring every 3 s. */
        void exact()
        {
            sinceExact = 0; double s = 0.0, m = 0.0; const int have = std::min (filled, sLen);
            for (int i = 1; i <= have; ++i) { const int k = (pos - i + sLen) % sLen; s += ring[(size_t) k]; if (i <= mLen) m += ring[(size_t) k]; }
            sSum = s; mSum = m;
        }
        void closeSubBlock()
        {
            subs.push_back (subAcc / sub); subAcc = 0.0; subCount = 0; const size_t n = subs.size();
            if (n >= 4) blocks.push_back ((subs[n - 1] + subs[n - 2] + subs[n - 3] + subs[n - 4]) / 4.0);
            if (n >= 30) { double s = 0.0; for (size_t i = n - 30; i < n; ++i) s += subs[i]; shorts.push_back (s / 30.0); }
            if (subs.size() > 64) subs.erase (subs.begin(), subs.begin() + 32);
        }

        double fs = 48000.0, weight[5] = { 1, 1, 1, 1.41, 1.41 }; int nch = 2, mLen = 19200, sLen = 144000, sub = 4800;
        Biquad shelf[5], hp[5]; State st[5][2]; std::vector<double> ring; int pos = 0, filled = 0, sinceExact = 0; long long samples = 0;
        double mSum = 0, sSum = 0, mNow = 0, sNow = 0, mMax = 0, sMax = 0, subAcc = 0; int subCount = 0; std::vector<double> subs, blocks, shorts;
        double tpHist[5][24] = {}; int tpPos = 0; double peak = 0.0;
    };
}
