#pragma once
// SqCapture: measure a piece of gear with one sweep and build a model of it. Pure C++ (no JUCE).
//
// The sweep is a sine whose pitch rises by the same ratio every second. Played through a unit that distorts, each
// harmonic the unit adds is itself a copy of the sweep running ahead of it by a fixed time (L ln m for the m-th), so
// dividing the recording by the sweep separates them: the unit's ordinary response lands at time zero, its 2nd
// harmonic's response L ln 2 earlier, its 3rd's L ln 3 earlier, and so on. (L is chosen so the sweep's phase lines up
// with each of those copies.)
//
// The model is the sum   y = g1 * u + g2 * u^2 + g3 * u^3 + ...   (u: the input as a fraction of the sweep's level,
// *: a filter). A sine put through u^k makes known amounts of each harmonic, so the filters g follow from the measured
// harmonic responses by solving a small triangular set of equations at every frequency.
//
// What such a model can hold: a unit's tone and the distortion it adds at and below the level it was swept at. What it
// cannot: anything that depends on what came before over more than a few milliseconds (a compressor's gain riding,
// tape's memory), noise, or levels above the sweep's.
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include "SqTube.h"

namespace SqCapture
{
    using cd = std::complex<double>;
    constexpr double pi = 3.14159265358979323846;

    /** In-place radix-2 transform; the size must be a power of two. */
    inline void fft (std::vector<cd>& a, bool inverse)
    {
        const size_t n = a.size(); for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * pi / (double) len * (inverse ? 1.0 : -1.0); const cd wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len) { cd w (1.0, 0.0); for (size_t k = 0; k < len / 2; ++k) { const cd u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; } }
        }
        if (inverse) for (auto& v : a) v /= (double) n;
    }

    struct Sweep
    {
        double rate = 48000.0, lowHz = 15.0, highHz = 22000.0, seconds = 8.0, level = 0.5, tailSeconds = 1.5, quietDb = -12.0;
        /** The sweep's time constant, rounded so that lowHz x L is a whole number (the harmonics' copies then line up in phase). */
        double L() const { const double raw = seconds / std::log (highHz / lowHz); return std::max (1.0, std::round (raw * lowHz)) / lowHz; }
        int sweepSamples() const { return (int) std::round (L() * std::log (highHz / lowHz) * rate); }
        /** One pass: the sweep and the silence after it. The whole signal is two passes: at `level`, then quietDb lower. */
        int passSamples() const { return sweepSamples() + (int) std::round (tailSeconds * rate); }
        int totalSamples() const { return 2 * passSamples(); }
        double quietGain() const { return std::pow (10.0, quietDb / 20.0); }
        /** The fade at each end (0.1 s in, 4 ms out); 0 outside the sweep. i counts from the start of a pass. */
        double envelope (int i) const { const int n = sweepSamples(), in = (int) (0.1 * rate), out = (int) (0.004 * rate); if (i < 0 || i >= n) return 0.0; if (i < in) return 0.5 - 0.5 * std::cos (pi * i / in); if (i > n - out) return 0.5 - 0.5 * std::cos (pi * (n - i) / out); return 1.0; }
        /** One pass at unit level. */
        std::vector<double> pass() const { const int n = sweepSamples(); const double l = L(); std::vector<double> x ((size_t) passSamples(), 0.0); for (int i = 0; i < n; ++i) x[(size_t) i] = envelope (i) * std::sin (2.0 * pi * lowHz * l * std::expm1 (i / rate / l)); return x; }
        std::vector<float> make() const
        {
            const auto one = pass(); const int n = (int) one.size(); std::vector<float> x ((size_t) (2 * n)); const double q = quietGain();
            for (int i = 0; i < n; ++i) { x[(size_t) i] = (float) (level * one[(size_t) i]); x[(size_t) (n + i)] = (float) (level * q * one[(size_t) i]); }
            return x;
        }
    };

    struct Model
    {
        double rate = 48000.0, level = 0.5; int order = 0, taps = 0, pre = 32;
        std::vector<std::vector<float>> g;                // one filter per power, 1 to order
        std::vector<float> front;                         // a filter before the powers (empty: none) - for units whose distortion depends on pitch
        double frontPeak = 1.0;                           // the most the front filter can give for an input at the sweep's level
        // read off the measurement, for the user: delay found, gain at 1 kHz, each harmonic of a 1 kHz tone at the sweep's level (dB under the tone),
        // and how far the model's answer to the quiet pass is from the unit's (average dB, 40 Hz to 16 kHz) with and without the front filter
        int latency = 0; double gainDbAt1k = 0.0, quietMissDb = 0.0, quietMissPlainDb = 0.0; std::vector<double> harmonicDb;
        bool valid() const { return order > 0 && taps > 0 && (int) g.size() == order; }
    };

    /** How much of harmonic m (as sin(m theta)) the power u^k of a unit sine holds: 2j C(k, r) (-1)^r / (2j)^k with r = (k - m) / 2. */
    inline cd share (int m, int k)
    {
        if (k < m || ((k - m) & 1)) return 0.0; const int r = (k - m) / 2; double c = 1.0; for (int i = 0; i < r; ++i) c = c * (k - i) / (i + 1);
        cd p (1.0, 0.0); for (int i = 0; i < k; ++i) p *= cd (0.0, 2.0); return cd (0.0, 2.0) * c * ((r & 1) ? -1.0 : 1.0) / p;
    }

    /** One pass taken apart: the unit's ordinary response and each harmonic's, as spectra on a grid of W points (bin b is b x rate / W),
        each with its moment moved to sample `pre`. `x` is the pass as played at unit level. */
    inline std::vector<std::vector<cd>> takeApart (const Sweep& sweep, const std::vector<double>& x, const float* recorded, int count, int order, int W, int pre, int& latency, bool findLatency)
    {
        size_t N = 1; while (N < x.size() + 8192) N <<= 1; std::vector<cd> X (N), Y (N); for (size_t i = 0; i < x.size(); ++i) X[i] = x[i];
        // even powers of the sweep stand on a steady pedestal while it plays (sin^2 is one half on average); its size is found against the
        // sweep's own outline and it is taken out first
        double pedestal = 0.0; { double num = 0.0, den = 0.0; for (int i = 0; i < count && i < (int) x.size(); ++i) { const double e = sweep.envelope (i), e2 = e * e; num += recorded[i] * e2; den += e2 * e2; } if (den > 0.0) pedestal = num / den; }
        for (int i = 0; i < count && (size_t) i < N; ++i) { const double e = sweep.envelope (i); Y[(size_t) i] = recorded[i] - pedestal * e * e; }
        fft (X, false); fft (Y, false); double top = 0.0; for (auto& v : X) top = std::max (top, std::norm (v));
        const double fs = sweep.rate, lo = sweep.lowHz, hi = sweep.highHz;
        auto band = [&] (double f, double from, double to) { f = std::abs (f); if (f <= from || f >= to) return 0.0; const double a = from * 1.25, b = to * 0.985; if (f < a) return 0.5 - 0.5 * std::cos (pi * (f - from) / (a - from)); if (f > b) return 0.5 + 0.5 * std::cos (pi * (f - b) / (to - b)); return 1.0; };
        for (size_t i = 0; i < N; ++i) { const double f = (i <= N / 2 ? (double) i : (double) i - (double) N) * fs / (double) N; Y[i] = Y[i] * std::conj (X[i]) / (std::norm (X[i]) + 1.0e-9 * top) * band (f, lo, hi); }
        fft (Y, true);       // Y is now the whole answer in time: the ordinary response from 0 on, the harmonics' before it (wrapped to the end)
        if (findLatency) { double best = 0.0; latency = 0; const int span = (int) std::min ((double) N / 4, fs * 0.5); for (int i = 0; i < span; ++i) { const double v = std::abs (Y[(size_t) i].real()); if (v > best) { best = v; latency = i; } } }
        const double l = sweep.L(); const int half = W / 2; std::vector<std::vector<cd>> H ((size_t) order, std::vector<cd> ((size_t) W));
        for (int m = 1; m <= order; ++m)
        {
            // an even harmonic comes back as a cosine where the sweep is a sine, a quarter turn apart at every frequency: in time that is a
            // response with a long tail BEFORE its moment as well as after. So each one is cut out with its moment in the middle of the
            // window, and only put back at the front once the shares have been divided out (which undoes the quarter turn).
            const double tm = latency - l * fs * std::log ((double) m), fl = std::floor (tm), frac = tm - fl; const long n0 = (long) fl - half;
            auto& h = H[(size_t) (m - 1)]; const long fade = W / 8;
            for (long i = 0; i < W; ++i)
            {
                long at = (n0 + i) % (long) N; if (at < 0) at += (long) N; double g = 1.0;
                if (i < fade) g = 0.5 - 0.5 * std::cos (pi * i / fade); if (i > W - fade) g = 0.5 - 0.5 * std::cos (pi * (W - i) / fade);
                h[(size_t) i] = Y[(size_t) at].real() * g;
            }
            fft (h, false);
            for (int b = 0; b <= half; ++b) { const double f = b * fs / W; h[(size_t) b] *= std::polar (1.0, 2.0 * pi * f * (half - pre + frac) / fs) * (f > hi * 0.9 ? band (f, m * lo, hi) : 1.0); }
            // below where the sweep reached for this harmonic nothing was measured: the value at the edge is held down to 0 Hz (cutting it off
            // sharply instead would ring for longer than the filters are)
            const int edge = std::min (half, (int) std::ceil (m * lo * 1.3 * W / fs)); for (int b = 0; b < edge; ++b) h[(size_t) b] = h[(size_t) edge];
        }
        return H;
    }

    /** Fit a model from the sweep (as made by `sweep`: a pass at its level, then a quiet one) and the recording of it through the unit.
        The loud pass gives the filters. Whether the unit needs a filter in FRONT of its distortion (bass driving it less, say) cannot be
        told from one level - any front filter can be made to agree with the loud pass - so candidates for it are worked out from how the
        2nd or 3rd harmonic's strength changes with pitch, and the one whose model best predicts the quiet pass is kept (or none). */
    inline Model fit (const Sweep& sweep, const float* recorded, int count, int order = 5, int taps = 4096)
    {
        Model mo; mo.rate = sweep.rate; mo.level = sweep.level; order = std::max (1, std::min (7, order)); mo.taps = taps; mo.order = order;
        const auto x = sweep.pass(); const int passN = (int) x.size(), W = taps * 2, half = W / 2; const double fs = sweep.rate, hi = sweep.highHz, lo = sweep.lowHz, q = sweep.quietGain();
        std::vector<float> loud ((size_t) passN, 0.0f), quiet ((size_t) passN, 0.0f);
        for (int i = 0; i < passN; ++i) { if (i < count) loud[(size_t) i] = recorded[i]; if (passN + i < count) quiet[(size_t) i] = recorded[passN + i] / (float) q; }
        const auto H = takeApart (sweep, x, loud.data(), passN, order, W, mo.pre, mo.latency, true); int same = mo.latency;
        const auto Hq = takeApart (sweep, x, quiet.data(), passN, 1, W, mo.pre, same, false);
        auto at = [&] (const std::vector<cd>& v, double hz) { const double pos = hz * W / fs; const int b = (int) pos; const double t = pos - b; if (b + 1 > half) return v[(size_t) half]; return v[(size_t) b] * (1.0 - t) + v[(size_t) b + 1] * t; };
        const double tone = std::abs (at (H[0], 1000.0)); mo.gainDbAt1k = 20.0 * std::log10 (tone / sweep.level + 1.0e-30);
        for (int m = 2; m <= order; ++m) mo.harmonicDb.push_back (m * 1000.0 < hi ? 20.0 * std::log10 (std::abs (at (H[(size_t) (m - 1)], m * 1000.0)) / (tone + 1.0e-30) + 1.0e-30) : -300.0);

        // the filters for a given front filter P: at each frequency, measured harmonic m = sum over powers k of share (m, k) x filter k x
        // (P at the pitch that made it: its phase m times over, its size k times over) - triangular, solved from the highest power down
        auto solve = [&] (const std::vector<cd>& P)
        {
            std::vector<std::vector<cd>> G ((size_t) order, std::vector<cd> ((size_t) W));
            for (int b = 0; b <= half; ++b)
            {
                const double f = b * fs / W; cd turn[8]; double size[8];
                for (int m = 1; m <= order; ++m) { const cd pm = at (P, f / m); size[m] = std::max (1.0e-6, std::abs (pm)); turn[m] = std::pow (pm / size[m], m); }
                for (int k = order; k >= 1; --k) { cd v = H[(size_t) (k - 1)][(size_t) b]; for (int j = k + 2; j <= order; j += 2) v -= share (k, j) * turn[k] * std::pow (size[k], j) * G[(size_t) (j - 1)][(size_t) b]; G[(size_t) (k - 1)][(size_t) b] = v / (share (k, k) * turn[k] * std::pow (size[k], k)); }
            }
            return G;
        };
        // how far the model's answer to a quiet sine is from the quiet pass, average dB over 40 Hz to 16 kHz
        auto miss = [&] (const std::vector<std::vector<cd>>& G, const std::vector<cd>& P)
        {
            double sum = 0.0; int n = 0;
            for (double f = 40.0; f < std::min (16000.0, hi * 0.8); f *= 1.0595) { const cd p = at (P, f); const double sz = std::abs (p); cd v = 0.0; for (int k = 1; k <= order; k += 2) v += share (1, k) * at (G[(size_t) (k - 1)], f) * p * std::pow (sz * q, k - 1); sum += std::abs (20.0 * std::log10 ((std::abs (v) + 1.0e-12) / (std::abs (at (Hq[0], f)) + 1.0e-12))); ++n; }
            return sum / std::max (1, n);
        };
        // a candidate front filter from harmonic m (2 or 3): its strength against the tone's at the same pitch goes as P (f / m)^m / P (f).
        // Taking P as flat at the top, that gives P one step of m lower in pitch at a time; its phase is the least a filter of that shape can have.
        auto candidate = [&] (int m, double strength)
        {
            const int per = 24, n = (int) (per * std::log2 (hi / 10.0)) + 1; std::vector<double> d ((size_t) n, 0.0), p ((size_t) n, 0.0); auto hzOf = [&] (int i) { return 10.0 * std::pow (2.0, (double) i / per); };
            for (int i = 0; i < n; ++i) { const double f = hzOf (i); d[(size_t) i] = f > m * lo * 1.3 && f < hi * 0.9 ? std::log (std::abs (at (H[(size_t) (m - 1)], f)) + 1.0e-12) - std::log (std::abs (at (H[0], f)) + 1.0e-12) : 0.0; }
            { std::vector<double> sm (d); for (int i = 0; i < n; ++i) { double a = 0.0; int c = 0; for (int j = -4; j <= 4; ++j) if (i + j >= 0 && i + j < n) { a += d[(size_t) (i + j)]; ++c; } sm[(size_t) i] = a / c; } d = sm; }
            const double topHz = hi / (2.0 * m); std::vector<double> ref; for (int i = 0; i < n; ++i) { const double f = hzOf (i); if (f > topHz * m * 1.05 && f < hi * 0.8) ref.push_back (d[(size_t) i]); }
            if (ref.empty()) return std::vector<cd>(); std::sort (ref.begin(), ref.end()); const double c = ref[ref.size() / 2]; const int stepBins = (int) std::round (per * std::log2 ((double) m));
            for (int i = n - 1; i >= 0; --i) { const double f = hzOf (i); const int up = i + stepBins; if (f >= topHz || up >= n) { p[(size_t) i] = 0.0; continue; } const double fu = hzOf (up); p[(size_t) i] = fu > m * lo * 1.3 ? std::max (-3.5, std::min (3.5, (p[(size_t) up] + d[(size_t) up] - c) / m)) : p[(size_t) (i + 1)]; }
            // onto the grid, then the least-phase filter of that size (through the cepstrum)
            std::vector<cd> c2 ((size_t) W); for (int b = 0; b <= half; ++b) { const double f = std::max (10.0, b * fs / W), pos = per * std::log2 (f / 10.0); const int i = std::min (n - 2, (int) pos); const double t = std::min (1.0, pos - i); c2[(size_t) b] = strength * (p[(size_t) i] * (1.0 - t) + p[(size_t) i + 1] * t); if (b > 0 && b < half) c2[(size_t) (W - b)] = c2[(size_t) b]; }
            fft (c2, true); for (int i = 1; i < half; ++i) { c2[(size_t) i] *= 2.0; c2[(size_t) (W - i)] = 0.0; } fft (c2, false); for (auto& v : c2) v = std::exp (v);
            return c2;
        };
        std::vector<cd> flat ((size_t) W, cd (1.0, 0.0)); auto best = solve (flat); auto bestP = flat; double bestMiss = miss (best, flat); mo.quietMissPlainDb = bestMiss; bool front = false;
        for (int m : { 3, 2 })
        {
            if (m > order || (m == 2 ? mo.harmonicDb[0] : mo.harmonicDb[1]) < -70.0) continue;       // too weak to learn anything from
            for (double strength : { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 2.5, 3.0 })       // the rule of thumb behind the candidate is rough: its depth is tried at several sizes
            {
                const auto P = candidate (m, strength); if (P.empty()) break; const auto G = solve (P); const double e = miss (G, P);
                if (e < bestMiss - 0.03) { bestMiss = e; best = G; bestP = P; front = true; }
            }
        }
        mo.quietMissDb = bestMiss;
        auto toTaps = [&] (std::vector<cd> s, int n, bool fadeTail)
        {
            s[0] = cd (s[0].real(), 0.0); s[(size_t) half] = cd (s[(size_t) half].real(), 0.0); for (int b = 1; b < half; ++b) s[(size_t) (W - b)] = std::conj (s[(size_t) b]);
            fft (s, true); std::vector<float> out ((size_t) n); for (int i = 0; i < n; ++i) { double g = 1.0; if (fadeTail && i > n - n / 8) g = 0.5 - 0.5 * std::cos (pi * (n - i) / (n / 8.0)); out[(size_t) i] = (float) (s[(size_t) i].real() * g); }
            return out;
        };
        mo.g.clear(); for (int k = 0; k < order; ++k) mo.g.push_back (toTaps (best[(size_t) k], taps, true));
        if (front) { mo.front = toTaps (bestP, taps / 2, true); double pk = 0.0; for (int b = 0; b <= half; ++b) pk = std::max (pk, std::abs (bestP[(size_t) b])); mo.frontPeak = pk; }
        return mo;
    }

    /** Plays a model one sample at a time (plain convolution - the plug-in uses a faster engine for the same sum).
        The powers are made at 8 times the rate so they do not fold. Delay: latency() samples. */
    class Player
    {
    public:
        void prepare (const Model& model)
        {
            m = model; os.assign ((size_t) std::max (0, m.order - 1), {}); for (auto& o : os) o.prepare (8); hist.assign ((size_t) m.order, std::vector<float> ((size_t) m.taps * 2, 0.0f)); pos = 0;
            line.assign ((size_t) (os.empty() ? 1 : os[0].latency() + 1), 0.0); linePos = 0; fh.assign (m.front.size() * 2, 0.0f); fpos = 0;
        }
        int latency() const { return (os.empty() ? 0 : os[0].latency()) + m.pre; }
        /** The input held inside the level the unit was swept at (beyond it the model knows nothing). */
        static double held (double u) { return std::max (-1.0, std::min (1.0, u)); }
        double process (double x)
        {
            double u = held (x / m.level), y = 0.0;
            if (const int n = (int) m.front.size())       // the front filter, then held inside the most it gave during the sweep
            {
                fh[(size_t) fpos] = fh[(size_t) (fpos + n)] = (float) u; const float* a = fh.data() + fpos + n; const float* g = m.front.data(); float acc = 0.0f; for (int i = 0; i < n; ++i) acc += g[i] * a[-i];
                if (++fpos >= n) fpos = 0; u = std::max (-m.frontPeak, std::min (m.frontPeak, (double) acc));
            }
            for (int k = 1; k <= m.order; ++k)
            {
                double v;
                if (k == 1) { if (line.size() > 1) { v = line[(size_t) linePos]; line[(size_t) linePos] = u; if (++linePos >= (int) line.size() - 1) linePos = 0; } else v = u; }
                else v = os[(size_t) (k - 2)].process (u, [k] (double p) { double r = p; for (int i = 1; i < k; ++i) r *= p; return r; });
                auto& h = hist[(size_t) (k - 1)]; h[(size_t) pos] = h[(size_t) (pos + m.taps)] = (float) v;
                const float* a = h.data() + pos + m.taps; const float* g = m.g[(size_t) (k - 1)].data(); float acc = 0.0f; for (int i = 0; i < m.taps; ++i) acc += g[i] * a[-i];
                y += acc;
            }
            if (++pos >= m.taps) pos = 0; return y;
        }
    private:
        Model m; std::vector<SqTube::Oversampler> os; std::vector<std::vector<float>> hist; std::vector<double> line; std::vector<float> fh; int pos = 0, linePos = 0, fpos = 0;
    };
}
