#pragma once
// Squelchmoth 1.40 - Voltage Tape: a tape machine from its parts. Pure: no engine, no UI.
//  - What the tape keeps: a table of the magnetisation left on a tape element after it has passed the record gap, for a
//    grid of signal and bias fields, worked out offline by simulating that process on the hysteresis equations
//    (tests/reference/tape/make_table.py, cross-checked against a circuit simulator). With no bias the tape ignores quiet
//    signals and distorts; with bias it is linear until it saturates; with too much it loses sensitivity.
//  - Around it, the standard's recorded-flux curve for the chosen speed and equalisation (time constants as published),
//    undone again on replay.
//  - Replay losses that depend on wavelength (speed / frequency): head gap, head-to-tape spacing, coating thickness, with a
//    replay trim that makes 10 kHz level again; and the head bump in the bass.
//  - Wow and flutter: the tape's speed wandering, as a delay that moves.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace SqTape
{
    #include "SqTapeTable.inc"
    constexpr double pi = 3.14159265358979323846;

    //============================================================================== what the tape keeps
    /** The remanence curve for one bias setting: field in (units of the tape's 'a'), magnetisation out (fraction of saturation). Odd. */
    class Curve
    {
    public:
        static constexpr int fine = 2048;
        void setBias (double bias)
        {
            bias = std::clamp (bias, remanenceBias[0], remanenceBias[remanenceRows - 1]);
            int r = 0; while (r < remanenceRows - 2 && remanenceBias[r + 1] < bias) ++r;
            const double f = (bias - remanenceBias[r]) / (remanenceBias[r + 1] - remanenceBias[r]);
            double row[remanenceCols]; for (int i = 0; i < remanenceCols; ++i) row[i] = remanenceTable[r][i] + f * (remanenceTable[r + 1][i] - remanenceTable[r][i]);
            row[0] = 0.0;                                                        // no signal, nothing kept (the simulation leaves a few millionths from where the bias happens to stop)
            // through the simulated points with a smooth (Catmull-Rom) curve, onto a fine grid
            auto at = [&row] (int i) { return i < 0 ? -row[-i] : i >= remanenceCols ? row[remanenceCols - 1] + (i - remanenceCols + 1) * (row[remanenceCols - 1] - row[remanenceCols - 2]) : row[i]; };
            for (int k = 0; k <= fine; ++k)
            {
                const double pos = (double) k * (remanenceCols - 1) / fine; const int i = std::min ((int) pos, remanenceCols - 2); const double t = pos - i;
                const double p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
                y[(size_t) k] = 0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t);
            }
            endValue = y[(size_t) fine]; endSlope = std::max (0.0, (y[(size_t) fine] - y[(size_t) fine - 1]) * fine / remanenceHsMax); usedBias = bias;
        }
        double bias() const { return usedBias; }
        double operator() (double h) const
        {
            const double a = std::abs (h); double v;
            if (a >= remanenceHsMax) { const double room = 0.03; v = endValue + room * (1.0 - std::exp (-endSlope * (a - remanenceHsMax) / room)); }      // past the simulated range: on towards saturation, never beyond
            else { const double pos = a * (fine / remanenceHsMax); const int i = (int) pos; const double t = pos - i; v = y[(size_t) i] + t * (y[(size_t) i + 1] - y[(size_t) i]); }
            return h < 0 ? -v : v;
        }
        /** How much of a sine of this peak field comes back at its own frequency (output over input). */
        double sineGain (double amp) const
        {
            if (amp <= 0) return 0.0; double acc = 0.0; const int n = 256;
            for (int i = 0; i < n; ++i) { const double s = std::sin (2.0 * pi * (i + 0.5) / n); acc += (*this) (amp * s) * s; }
            return 2.0 * acc / n / amp;
        }
        /** Level of the k-th harmonic of a sine of this peak field, in dB below its fundamental (negative). */
        double harmonicDb (double amp, int k) const
        {
            double a1 = 0, ak = 0; const int n = 1024;
            for (int i = 0; i < n; ++i) { const double ph = 2.0 * pi * (i + 0.5) / n, v = (*this) (amp * std::sin (ph)); a1 += v * std::sin (ph); ak += v * std::sin (k * ph); }
            return 20.0 * std::log10 (std::abs (ak) / std::max (1.0e-300, std::abs (a1)) + 1.0e-15);
        }
    private:
        std::array<double, fine + 2> y {}; double endValue = 0, endSlope = 0, usedBias = -1;
    };

    //============================================================================== the standards
    enum class Speed { ips7_5, ips15, ips30 };
    enum class Eq { nab, iec };
    inline double metresPerSecond (Speed s) { return s == Speed::ips7_5 ? 0.1905 : s == Speed::ips15 ? 0.381 : 0.762; }
    /** Time constants in seconds: high-frequency, and low-frequency (0: none). 30 in/s has one curve (17.5 us). */
    inline void timeConstants (Speed s, Eq e, double& high, double& low)
    {
        low = 0.0;
        if (s == Speed::ips30) { high = 17.5e-6; return; }
        if (e == Eq::nab) { high = 50.0e-6; low = 3180.0e-6; return; }
        high = s == Speed::ips7_5 ? 70.0e-6 : 35.0e-6;
    }
    /** The recorded-flux curve the standard defines, as a level against frequency (1 at mid frequencies, apart from the
        corner's own small loss): falling 6 dB an octave above the high corner; with a low time constant, rising below it.
        bassLimit caps that rise (the standard's own curve rises without end towards 0 Hz). */
    inline double fluxLevel (double hz, double high, double low, double bassLimit = 10.0)
    {
        const double w = 2.0 * pi * hz; double v = 1.0 / std::sqrt (1.0 + (w * high) * (w * high));
        if (low > 0) { const double wl = 1.0 / low; v *= std::sqrt ((w * w + wl * wl) / (w * w + (wl / bassLimit) * (wl / bassLimit))); }
        return v;
    }

    //============================================================================== replay losses
    struct Head { double gap = 3.0e-6, spacing = 0.25e-6, coating = 12.0e-6; };       // metres; plausible values, not taken from any machine
    inline double gapLoss (double wavelength, double gap)          { const double x = pi * gap / wavelength; return x < 1.0e-9 ? 1.0 : std::abs (std::sin (x) / x); }
    inline double spacingLoss (double wavelength, double spacing)  { return std::pow (10.0, -55.0 * spacing / wavelength / 20.0); }      // 55 dB for a spacing of one wavelength
    inline double thicknessLoss (double wavelength, double coating) { const double x = 2.0 * pi * coating / wavelength; return x < 1.0e-9 ? 1.0 : (1.0 - std::exp (-x)) / x; }
    inline double wavelengthLoss (double hz, double speed, const Head& h)
    {
        if (hz <= 0) return 1.0; const double l = speed / hz; return gapLoss (l, h.gap) * spacingLoss (l, h.spacing) * thicknessLoss (l, h.coating);
    }
    /** The replay amplifier, lined up the way a machine is: level set at 1 kHz, and one rising treble corner set so that
        10 kHz comes back at the same level as 1 kHz. Returns that corner in Hz. */
    inline double trimCorner (double speed, const Head& h)
    {
        const double need = wavelengthLoss (1000.0, speed, h) / wavelengthLoss (10000.0, speed, h);      // how much more 10 kHz has lost than 1 kHz
        if (need <= 1.0) return 1.0e9;
        auto lift = [] (double fc) { return std::sqrt ((1.0 + (10000.0 / fc) * (10000.0 / fc)) / (1.0 + (1000.0 / fc) * (1000.0 / fc))); };      // falls as the corner rises
        double lo = 50.0, hi = 1.0e6; if (lift (lo) < need) return lo;
        for (int i = 0; i < 80; ++i) { const double mid = std::sqrt (lo * hi); if (lift (mid) > need) lo = mid; else hi = mid; }
        return std::sqrt (lo * hi);
    }
    inline double replayLevel (double hz, double speed, const Head& h)
    {
        const double fc = trimCorner (speed, h); auto raw = [&] (double f) { return wavelengthLoss (f, speed, h) * std::sqrt (1.0 + (f / fc) * (f / fc)); };
        return raw (hz) / raw (1000.0);
    }
    /** The head bump: a rise where the wavelength is about the length of the head's face, a smaller dip an octave up, and the
        bass falling away below. The shape is ours (a typical one); amount 0 to 1. Level against frequency. */
    struct Bump { double hz = 50.0, riseDb = 2.0, dipDb = -1.0; };
    inline Bump bumpFor (Speed s, double amount) { Bump b; b.hz = s == Speed::ips7_5 ? 30.0 : s == Speed::ips15 ? 55.0 : 105.0; b.riseDb = 2.0 * amount; b.dipDb = -1.0 * amount; return b; }

    //============================================================================== filters
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void peak (double hz, double q, double db, double fs)
        {
            const double a = std::pow (10.0, db / 40.0), w = 2.0 * pi * hz / fs, al = std::sin (w) / (2.0 * q), n = 1.0 + al / a;
            b0 = (1.0 + al * a) / n; b1 = -2.0 * std::cos (w) / n; b2 = (1.0 - al * a) / n; a1 = b1; a2 = (1.0 - al / a) / n;
        }
        void highpass1 (double hz, double fs) { const double k = std::tan (pi * hz / fs), n = 1.0 + k; b0 = 1.0 / n; b1 = -1.0 / n; b2 = 0; a1 = (k - 1.0) / n; a2 = 0; }
        void flat() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
        double level (double hz, double fs) const
        {
            const std::complex<double> z = std::polar (1.0, -2.0 * pi * hz / fs); return std::abs ((b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z));
        }
    };
    /** First-order section and its exact inverse (it is minimum phase), for the flux curve and its undoing. */
    struct First
    {
        double b0 = 1, b1 = 0, a1 = 0, x1 = 0, y1 = 0, ix1 = 0, iy1 = 0;
        void reset() { x1 = y1 = ix1 = iy1 = 0; }
        double forward (double x) { const double y = b0 * x + b1 * x1 - a1 * y1; x1 = x; y1 = y; return y; }
        double inverse (double y) { const double x = (y + a1 * ix1 - b1 * iy1) / b0; ix1 = y; iy1 = x; return x; }
        /** A plain one-pole low-pass with this time constant (no zero at the top, so its inverse is a short, safe filter). */
        void lowpass (double tau, double fs) { const double p = std::exp (-1.0 / (tau * fs)); b0 = 1.0 - p; b1 = 0; a1 = -p; }
        /** (s + w) / (s + w / limit): a bass rise of `limit` times below 1 / (2 pi tau). */
        void bassRise (double tau, double limit, double fs)
        {
            const double k = 2.0 * fs, wz = 1.0 / tau, wp = wz / limit, n = k + wp; b0 = (k + wz) / n; b1 = (wz - k) / n; a1 = (wp - k) / n;
        }
        void flat() { b0 = 1; b1 = a1 = 0; }
        double level (double hz, double fs) const { const std::complex<double> z = std::polar (1.0, -2.0 * pi * hz / fs); return std::abs ((b0 + b1 * z) / (1.0 + a1 * z)); }
    };

    /** A linear-phase filter drawn from a level-against-frequency curve (frequency sampling, Kaiser window). Its length goes
        with the rate so that the bass end of the curve is followed as closely at 96 kHz as at 48. */
    class LossFir
    {
    public:
        static int delayFor (double fs) { return 127 * std::max (1, (int) std::lround (fs / 48000.0)); }
        template <typename Fn> void design (double fs, Fn&& level)
        {
            delay = delayFor (fs); taps = 2 * delay + 1; h.assign ((size_t) taps, 0.0); if ((int) hist.size() != taps) { hist.assign ((size_t) taps, 0.0); pos = 0; }
            const int grid = 4 * taps; mag.resize ((size_t) grid + 1); for (int k = 0; k <= grid; ++k) mag[(size_t) k] = level (0.5 * fs * k / grid);
            auto bessel = [] (double v) { double s = 1.0, term = 1.0; for (int k = 1; k < 60; ++k) { term *= (v / (2.0 * k)) * (v / (2.0 * k)); s += term; if (term < 1.0e-18 * s) break; } return s; };
            for (int n = 0; n < taps; ++n)
            {
                const double m = n - delay; double acc = 0.0;
                for (int k = 0; k <= grid; ++k) acc += (k == 0 || k == grid ? 0.5 : 1.0) * mag[(size_t) k] * std::cos (pi * k * m / grid);
                const double r = m / (double) delay; h[(size_t) n] = acc / grid * bessel (5.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel (5.0);
            }
        }
        void reset() { std::fill (hist.begin(), hist.end(), 0.0); pos = 0; }
        double process (double x)
        {
            if (taps == 0) return x;
            hist[(size_t) pos] = x; double acc = 0.0; int idx = pos;
            for (int n = 0; n < taps; ++n) { acc += h[(size_t) n] * hist[(size_t) idx]; if (--idx < 0) idx = taps - 1; }
            if (++pos >= taps) pos = 0; return acc;
        }
        double level (double hz, double fs) const { std::complex<double> a = 0; for (int n = 0; n < taps; ++n) a += h[(size_t) n] * std::polar (1.0, -2.0 * pi * hz * n / fs); return std::abs (a); }
    private:
        std::vector<double> h, hist, mag; int pos = 0, taps = 0, delay = 0;
    };

    //============================================================================== wow and flutter
    /** The tape's speed error as a sum of slow (wow) and faster (flutter) wanderings; amounts are peak fractions (0.001 = 0.1 %).
        The same transport carries every channel, so one of these serves both. */
    class Transport
    {
    public:
        void prepare (double fs, int seed)
        {
            rate = fs; t = 0;
            auto rnd = [&seed] () { seed = (int) ((unsigned) seed * 1664525u + 1013904223u); return (double) ((unsigned) seed >> 8) / 16777216.0; };
            const double hz[5] = { 0.55, 1.3, 7.3, 13.1, 29.0 }, share[5] = { 0.8, 0.2, 0.6, 0.3, 0.1 };
            for (int i = 0; i < 5; ++i) { w[i] = 2.0 * pi * hz[i] * (0.9 + 0.2 * rnd()); ph[i] = 2.0 * pi * rnd(); part[i] = share[i]; }
        }
        void set (double wowPeak, double flutterPeak) { wow = wowPeak; flutter = flutterPeak; }
        /** Seconds the sound is late by (around zero) for the next sample, and the speed error at that moment. */
        double next (double* speedError = nullptr)
        {
            double late = 0.0, sp = 0.0; const double time = (double) t / rate; ++t;
            for (int i = 0; i < 5; ++i) { const double a = (i < 2 ? wow : flutter) * part[i]; sp += a * std::sin (w[i] * time + ph[i]); late += a / w[i] * std::cos (w[i] * time + ph[i]); }
            if (speedError != nullptr) *speedError = sp; return late;
        }
        /** The furthest the sound can be early or late, in seconds, at these amounts. */
        double reach() const { double r = 0; for (int i = 0; i < 5; ++i) r += (i < 2 ? wow : flutter) * part[i] / w[i]; return r; }
    private:
        double rate = 48000.0, w[5] = {}, ph[5] = {}, part[5] = {}, wow = 0, flutter = 0; long long t = 0;
    };

    /** A delay that can be read between samples (cubic through four points). */
    class MovingDelay
    {
    public:
        void prepare (int length) { size = 1; while (size < length + 8) size <<= 1; buf.assign ((size_t) size, 0.0); pos = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0); pos = 0; }
        double process (double x, double delaySamples)
        {
            buf[(size_t) pos] = x;
            const double d = std::clamp (delaySamples, 2.0, (double) size - 4.0); const int whole = (int) d; const double f = d - whole;
            const int m = size - 1, i1 = (pos - whole) & m, i0 = (i1 + 1) & m, i2 = (i1 - 1) & m, i3 = (i1 - 2) & m;
            const double p0 = buf[(size_t) i0], p1 = buf[(size_t) i1], p2 = buf[(size_t) i2], p3 = buf[(size_t) i3];
            pos = (pos + 1) & m;
            return 0.5 * ((2.0 * p1) + (-p0 + p2) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * f * f + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * f * f * f);
        }
    private:
        std::vector<double> buf; int size = 1, pos = 0;
    };

    //============================================================================== one channel of the machine
    struct Settings
    {
        double fieldAtFullScale = 0.1;      // peak field (units of the tape's 'a') a full-scale sample reaches
        double bias = 0.2; Speed speed = Speed::ips15; Eq eq = Eq::nab; double bump = 0.5; Head head;
    };

    /** What sits inside the oversampler: flux curve, the tape, the curve undone, the moving delay. */
    class Channel
    {
    public:
        static constexpr double centreSeconds = 0.006;      // the moving delay rests here, so that it can go early as well as late

        /** outerRate: the session's rate; factor: the oversampling. */
        void prepare (double outerRate, int factor)
        {
            outer = outerRate; os = factor; inner = outerRate * factor; centre = (double) centreSamples (outerRate) * factor;
            delay.prepare ((int) (centre * 2.0) + 16); fir.reset(); reset(); dirty = true;
        }
        static int centreSamples (double outerRate) { return (int) std::lround (centreSeconds * outerRate); }
        /** Whole delay of a channel in samples at the outer rate, without the oversampler's own. */
        static int latency (double outerRate) { return centreSamples (outerRate) + LossFir::delayFor (outerRate); }

        void reset() { hi.reset(); lo.reset(); delay.reset(); fir.reset(); for (auto& b : bump) b.reset(); }

        void set (const Settings& s)
        {
            if (! dirty && s.bias == used.bias && s.speed == used.speed && s.eq == used.eq && s.bump == used.bump && s.fieldAtFullScale == used.fieldAtFullScale && s.head.gap == used.head.gap && s.head.spacing == used.head.spacing && s.head.coating == used.head.coating) return;
            if (dirty || s.bias != used.bias) curve.setBias (s.bias);
            double high, low; timeConstants (s.speed, s.eq, high, low);
            hi.lowpass (high, inner); if (low > 0) lo.bassRise (low, 10.0, inner); else lo.flat();
            if (dirty || s.speed != used.speed || s.head.gap != used.head.gap || s.head.spacing != used.head.spacing || s.head.coating != used.head.coating)      // only when the speed or the head changes: this is the costly part
            {
                const double v = metresPerSecond (s.speed); const Head head = s.head;
                fir.design (outer, [v, head] (double hz) { return replayLevel (std::min (hz, 24000.0), v, head); });
            }
            const Bump b = bumpFor (s.speed, s.bump);
            if (s.bump > 0) { bump[0].peak (b.hz, 1.4, b.riseDb, outer); bump[1].peak (2.0 * b.hz, 1.4, b.dipDb, outer); bump[2].highpass1 (b.hz / 3.0 * s.bump + 1.0e-3, outer); }
            else for (auto& q : bump) q.flat();
            // level-matched: a sine at the reference field (what a full-scale sample reaches at Drive 0) comes back as loud as it went in
            match = 1.0 / std::max (1.0e-6, curve.sineGain (referenceField));
            used = s; dirty = false;
        }
        static constexpr double referenceField = 0.1;

        /** Inside the oversampler: one sample at the inner rate; lateSeconds from the Transport. */
        double processInner (double x, double lateSeconds)
        {
            const double field = lo.forward (hi.forward (x * used.fieldAtFullScale));
            const double kept = curve (field) * match;
            const double back = hi.inverse (lo.inverse (kept)) / used.fieldAtFullScale;
            return delay.process (back, centre + lateSeconds * inner);
        }
        /** After the oversampler: replay losses and the head bump, at the outer rate. */
        double processOuter (double x) { double y = fir.process (x); for (auto& b : bump) y = b.process (y); return y; }

        const Curve& tape() const { return curve; }
        /** Level against frequency of the whole machine for quiet signals (the tape itself is level there). */
        double response (double hz) const { double v = fir.level (hz, outer); for (auto& b : bump) v *= b.level (hz, outer); return v; }
        double matchGain() const { return match; }

    private:
        Curve curve; First hi, lo; MovingDelay delay; LossFir fir; Biquad bump[3]; Settings used; bool dirty = true;
        double outer = 48000.0, inner = 192000.0, centre = 0, match = 1.0; int os = 4;
    };
}
