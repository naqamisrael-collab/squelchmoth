#pragma once
// Spectra EQ - DSP core (1.14 "Studio Line", phase A). No UI, no te:: types: tests/spectra_test.cpp measures it alone.
//
// Every band is described once, as an ANALOG prototype (a product of first- and second-order sections in s).
// That one description drives all three phase modes, so they agree to the measured limit:
//  - Zero latency: each section is turned into a digital biquad with "matched" poles (impulse invariance) and a
//    numerator fitted so the digital magnitude equals the analog magnitude at DC, at Nyquist and at the section's
//    own frequency. Result: no cramping near Nyquist (a 16 kHz bell at 48 kHz keeps its analog shape) and a phase
//    response close to the analog filter's. Where a pole sits too close to Nyquist for impulse invariance, the poles
//    come from a pre-warped bilinear map instead and the same three-point fit keeps the magnitude honest.
//  - Natural: the same matched sections run at 2x the sample rate (a short minimum-phase half-band pair), which
//    pulls magnitude AND phase even closer to analog in the top octave.
//  - Linear: the exact analog magnitude of every band is sampled on an FFT grid and turned into zero-phase FIRs.
//    Per-band stereo placement is exact here too: each band is a 2x2 matrix per frequency (stereo / left / right /
//    mid / side), the matrices are multiplied, and the four results become the LL / LR / RL / RR filters, run by a
//    uniformly partitioned overlap-save convolver. Latency = FIR length / 2 + partition.
// Dynamic bands (bell, shelves, tilt) move their gain with a key level measured through the band's own band-pass,
// with attack / release; coefficients are re-designed every 32 samples and cross-interpolated per sample.

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace SqSpectra
{
    constexpr int maxBands = 24;
    constexpr double pi = 3.14159265358979323846;

    enum class Shape { bell, notch, lowShelf, highShelf, lowCut, highCut, bandPass, tiltShelf, flatTilt, allPass };
    enum class Place { stereo, left, right, mid, side };
    enum class Phase { zeroLatency, natural, linear };

    inline juce::StringArray shapeNames()  { return { "Bell", "Notch", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" }; }
    inline juce::StringArray placeNames()  { return { "Stereo", "Left", "Right", "Mid", "Side" }; }
    inline juce::StringArray phaseNames()  { return { "Zero Latency", "Natural", "Linear" }; }
    /** Cut / band-pass / notch slopes in dB per octave; 0 = brickwall. */
    inline const std::array<int, 10>& slopes() { static const std::array<int, 10> s { 6, 12, 18, 24, 30, 36, 48, 72, 96, 0 }; return s; }

    struct Band
    {
        bool enabled = false;
        Shape shape = Shape::bell;
        Place place = Place::stereo;
        double freq = 1000.0, gainDb = 0.0, q = 1.0;
        int slope = 12;                 // dB/oct for cuts / band pass / notch; 0 = brickwall
        // dynamics
        bool dynamic = false;
        double threshDb = -30.0, rangeDb = 0.0, attackMs = 10.0, releaseMs = 120.0;
        bool externalKey = false;
        bool operator== (const Band& o) const
        {
            return enabled == o.enabled && shape == o.shape && place == o.place && freq == o.freq && gainDb == o.gainDb && q == o.q
                && slope == o.slope && dynamic == o.dynamic && threshDb == o.threshDb && rangeDb == o.rangeDb
                && attackMs == o.attackMs && releaseMs == o.releaseMs && externalKey == o.externalKey;
        }
        bool hasGain() const { return shape == Shape::bell || shape == Shape::lowShelf || shape == Shape::highShelf || shape == Shape::tiltShelf || shape == Shape::flatTilt; }
        bool canBeDynamic() const { return hasGain(); }
    };

    //==========================================================================
    /** One analog section, normalised to w0: (n2 s^2 + n1 s + n0) / (d2 s^2 + d1 s + d0), s = j w / w0.
        A first-order section has n2 = d2 = 0. */
    struct Analog
    {
        double w0 = 1;     // rad/s
        double n2 = 0, n1 = 0, n0 = 1, d2 = 0, d1 = 0, d0 = 1;
        bool allPass = false;
        bool firstOrder() const { return d2 == 0; }
        std::complex<double> at (double w) const          // response at physical rad/s
        {
            std::complex<double> s (0, w / w0);
            return (n2 * s * s + n1 * s + n0) / (d2 * s * s + d1 * s + d0);
        }
        double mag2 (double w) const { return std::norm (at (w)); }
    };

    using Sections = std::vector<Analog>;

    inline double butterQ (int order, int k) { return 1.0 / (2.0 * std::cos ((2.0 * k - 1.0) * pi / (2.0 * order))); }

    /** The analog prototype of a band at gain offset 'gainDb' (static gain plus any dynamic movement). */
    inline Sections prototype (const Band& b, double gainDb)
    {
        Sections out;
        const double w0 = 2 * pi * b.freq, q = juce::jlimit (0.025, 40.0, b.q);
        const double A = std::pow (10.0, gainDb / 40.0);          // sqrt of linear gain
        auto sec = [&] (double n2, double n1, double n0, double d2, double d1, double d0, double w = -1)
        { Analog a; a.w0 = w > 0 ? w : w0; a.n2 = n2; a.n1 = n1; a.n0 = n0; a.d2 = d2; a.d1 = d1; a.d0 = d0; out.push_back (a); };
        const int order = b.slope <= 0 ? 32 : juce::jlimit (1, 16, b.slope / 6);   // brickwall: 192 dB/oct prototype
        // a cut's resonance: Q 0.707 = flat Butterworth; other values scale the sharpest section
        const double res = q / std::sqrt (0.5);
        switch (b.shape)
        {
            case Shape::bell:       sec (1, A / q, 1, 1, 1 / (A * q), 1); break;
            case Shape::lowShelf:   sec (A * A, A * std::sqrt (A) / q, A * A * A, A, std::sqrt (A) / q, 1); break;   // A * (s^2 + vA/Q s + A) / (A s^2 + vA/Q s + 1)
            case Shape::highShelf:  sec (A * A * A, A * std::sqrt (A) / q, A, 1, std::sqrt (A) / q, A); break;        // A * (A s^2 + vA/Q s + 1) / (s^2 + vA/Q s + A)
            case Shape::lowCut:
            case Shape::highCut:
            {
                const bool hp = b.shape == Shape::lowCut;
                for (int k = 1; k <= order / 2; ++k)
                {
                    double qk = butterQ (order, k);
                    if (k == 1) qk *= res;                                   // the sharpest section carries the resonance
                    if (hp) sec (1, 0, 0, 1, 1 / qk, 1); else sec (0, 0, 1, 1, 1 / qk, 1);
                }
                if (order % 2) { if (hp) sec (0, 1, 0, 0, 1, 1); else sec (0, 0, 1, 0, 1, 1); }
                break;
            }
            case Shape::bandPass:
            {
                const int n = juce::jmax (1, (b.slope <= 0 ? 96 : b.slope) / 12);
                const double qn = q * std::sqrt (std::pow (2.0, 1.0 / n) - 1.0) / std::sqrt (std::pow (2.0, 1.0) - 1.0);   // n sections keep the single section's -3 dB width
                for (int i = 0; i < n; ++i) sec (0, 1 / qn, 0, 1, 1 / qn, 1);
                break;
            }
            case Shape::notch:
            {
                const int n = juce::jmax (1, (b.slope <= 0 ? 96 : b.slope) / 12);
                for (int i = 0; i < n; ++i) sec (1, 0, 1, 1, 1 / q, 1);
                break;
            }
            case Shape::allPass:
                sec (1, -1 / q, 1, 1, 1 / q, 1); out.back().allPass = true; break;
            case Shape::tiltShelf:
            {
                // first-order tilt: -g/2 dB far below f0, +g/2 dB far above, exactly 0 dB at f0 (the tilt's pivot)
                const double g = std::pow (10.0, gainDb / 20.0), rg = std::sqrt (g);
                Analog a; a.w0 = w0; a.d1 = 1.0 / rg; a.d0 = 1;
                // H = (1/sqrt g) (1 + s sqrt g) / (1 + s / sqrt g): zero at w0/sqrt g, pole at w0 sqrt g
                a.n1 = 1.0; a.n0 = 1.0 / rg;
                out.push_back (a);
                break;
            }
            case Shape::flatTilt:
            {
                // a constant slope (gainDb per octave, at most +/-6) pivoting at f0 (sections from 1 Hz to 200 kHz, so it stays straight across the audio band): one first-order
                // zero / pole pair per third of an octave, each adding a third of the slope (ripple measured by the test)
                const double slopeDb = juce::jlimit (-6.0, 6.0, gainDb);
                const double step = std::pow (10.0, slopeDb / 3.0 / 20.0);
                for (double f = 1.0; f < 200000.0; f *= std::pow (2.0, 1.0 / 3.0))
                {
                    Analog a; a.w0 = 2 * pi * f; a.n1 = 1; a.n0 = 1; a.d1 = 1.0 / step; a.d0 = 1;   // zero at f, pole at f * step
                    out.push_back (a);
                }
                std::complex<double> h (1, 0); for (auto& sct : out) h *= sct.at (w0);
                const double norm = 1.0 / std::abs (h); out.front().n1 *= norm; out.front().n0 *= norm;
                break;
            }
        }
        return out;
    }

    inline std::complex<double> analogResponse (const Sections& s, double hz)
    {
        std::complex<double> h (1, 0); for (auto& a : s) h *= a.at (2 * pi * hz); return h;
    }

    //==========================================================================
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::complex<double> at (double w) const
        {
            auto z1 = std::polar (1.0, -w), z2 = z1 * z1;
            return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
        }
    };

    constexpr int fitGrid = 176 + 32;
    struct FitGrid { double w[fitGrid]; int n = 0; };
    /** Digital frequencies (rad/sample) 10 Hz ... 0.98 x Nyquist used by a fit: 56 log-spaced points up to 2 kHz,
        120 evenly spaced above (about every 190 Hz at 48 kHz, so no top-octave dip hides between points), plus 32
        points packed around the section's own frequency (+/- 4 bandwidths) so a narrow bell is followed exactly. */
    inline FitGrid makeGrid (double fs, double focusW = 0, double focusQ = 1)
    {
        FitGrid g;
        const double lo = std::log (2 * pi * 10 / fs), mid = std::log (2 * pi * 2000 / fs), top = 0.98 * pi;
        for (int i = 0; i < 56; ++i) g.w[g.n++] = std::exp (lo + (mid - lo) * i / 56.0);
        for (int i = 0; i < 120; ++i) g.w[g.n++] = std::exp (mid) + (top - std::exp (mid)) * i / 119.0;
        if (focusW > 0)
        {
            const double bw = focusW / juce::jmax (0.1, focusQ);
            for (int i = 0; i < 32; ++i)
            {
                const double w = focusW + bw * (-4.0 + 8.0 * i / 31.0);
                if (w > std::exp (lo) && w < top) g.w[g.n++] = w;
            }
        }
        return g;
    }

    /** Given poles (a1, a2), fit the numerator so |H|^2 follows 'target' (|H|^2 on the fit grid) in relative terms,
        with DC pinned to 'dc'. Returns false for unstable poles. errOut = worst |ln| error (ignoring deep stop band). */
    inline bool fitNumerator (double a1, double a2, const FitGrid& grid, const double* target, double dc, double fs, Biquad& q, double& errOut)
    {
        if (std::abs (a2) >= 1 || std::abs (a1) >= 1 + a2) return false;
        const double* gw = grid.w; const int G = grid.n;
        const double A0 = (1 + a1 + a2) * (1 + a1 + a2), A1 = (1 - a1 + a2) * (1 - a1 + a2), A2 = -4 * a2;
        const double B0 = A0 * dc;
        double s11 = 0, s12 = 0, s22 = 0, r1 = 0, r2 = 0;
        double Tw[fitGrid], P0[fitGrid], P1[fitGrid], P2[fitGrid], Dw[fitGrid];
        for (int i = 0; i < G; ++i)
        {
            const double p1 = std::pow (std::sin (gw[i] / 2), 2), p0 = 1 - p1, p2 = 4 * p0 * p1;
            const double D = A0 * p0 + A1 * p1 + A2 * p2, T = target[i] * D;
            // relative error, but the stop band below -40 dB only counts as if it were -40 dB, and anything above
            // 20 kHz counts a fifth: accuracy is spent where it can be heard
            const double wgt = (gw[i] * fs > 2 * pi * 20500 ? 0.5 : 1.0) / std::max (T, 1e-4 * D);
            const double y = (T - B0 * p0) * wgt, x1 = p1 * wgt, x2 = p2 * wgt;
            s11 += x1 * x1; s12 += x1 * x2; s22 += x2 * x2; r1 += x1 * y; r2 += x2 * y;
            Tw[i] = T; P0[i] = p0; P1[i] = p1; P2[i] = p2; Dw[i] = D;
        }
        const double det = s11 * s22 - s12 * s12;
        if (std::abs (det) < 1e-300) return false;
        double B1 = (r1 * s22 - r2 * s12) / det, B2 = (s11 * r2 - s12 * r1) / det;
        if (B1 < 0) B1 = 0;
        const double W = 0.5 * (std::sqrt (B0) + std::sqrt (B1));
        if (W * W + B2 < 0) B2 = -W * W;
        q.a1 = a1; q.a2 = a2;
        q.b0 = 0.5 * (W + std::sqrt (W * W + B2));
        q.b1 = 0.5 * (std::sqrt (B0) - std::sqrt (B1));
        q.b2 = q.b0 != 0 ? -B2 / (4 * q.b0) : 0;
        double worst = 0;
        for (int i = 0; i < G; ++i)
        {
            if (Tw[i] < 1e-7 * Dw[i]) continue;                              // deep stop band (below -70 dB): not judged
            const double N = B0 * P0[i] + B1 * P1[i] + B2 * P2[i];
            worst = std::max (worst, std::abs (std::log (std::max (1e-300, N) / Tw[i])));
        }
        errOut = worst;
        return true;
    }

    /** The matched digital section for one analog section (see the header comment). */
    inline Biquad match (const Analog& a, double fs)
    {
        Biquad d;
        const double nyq = pi * fs;
        if (a.firstOrder())
        {
            // pole of (d1 s/w0 + d0): s = -w0 d0 / d1
            const double wp = a.w0 * a.d0 / a.d1;
            const double p = std::exp (-wp / fs);
            const double h0 = std::sqrt (a.mag2 (0)), hn = std::sqrt (a.mag2 (nyq));
            const double s1 = (1 - p) * h0, s2 = (1 + p) * hn;               // b0 + b1, b0 - b1
            d.b0 = 0.5 * (s1 + s2); d.b1 = 0.5 * (s1 - s2); d.b2 = 0; d.a1 = -p; d.a2 = 0;
            // the sign of the low / high response (a shelf's numerator can only be positive here)
            return d;
        }
        if (a.allPass)
        {
            // phase is the point of an all-pass: bilinear with pre-warp at w0 keeps it exactly all-pass
            const double w = juce::jmin (a.w0 / fs, 0.999 * pi), K = std::tan (w / 2), q = 1.0 / a.d1;
            const double norm = 1.0 / (1 + K / q + K * K);
            d.b0 = (1 - K / q + K * K) * norm; d.b1 = 2 * (K * K - 1) * norm; d.b2 = 1; d.a1 = d.b1; d.a2 = d.b0;
            return d;
        }
        // Candidate pole pairs: impulse-invariant (the analog decay, exact phase character), and two pre-warped
        // bilinear maps (warped at the pole frequency and at the band frequency). For each, the numerator is fitted by
        // least squares on |H|^2 (relative error, log-spaced 10 Hz ... 0.98 x Nyquist, DC pinned exactly), and the
        // candidate with the smallest worst-case dB error wins. |N|^2 is linear in (B0, B1, B2), so the fit is a
        // 2x2 solve - cheap enough to run every 32 samples for dynamic bands.
        const double wp = a.w0 * std::sqrt (a.d0 / a.d2);                    // rad/s
        const double zeta = a.d1 / (2 * std::sqrt (a.d0 * a.d2));
        const double w = wp / fs;
        struct Cand { double a1, a2; };
        Cand cands[10]; int nc = 0;
        if (w < 0.97 * pi)
        {
            const double e = std::exp (-zeta * w);
            cands[nc++] = { zeta <= 1 ? -2 * e * std::cos (std::sqrt (1 - zeta * zeta) * w) : -2 * e * std::cosh (std::sqrt (zeta * zeta - 1) * w), std::exp (-2 * zeta * w) };
        }
        auto bilinear = [&] (double warpAt)                                  // warpAt: digital rad/sample where analog and digital frequency agree
        {
            const double ww = juce::jlimit (1e-6, 0.995 * pi, warpAt);
            const double K = (ww * fs / wp) / std::tan (ww / 2);             // s/wp = K (1 - z^-1) / (1 + z^-1)
            const double a0 = K * K + 2 * zeta * K + 1;
            return Cand { 2 * (1 - K * K) / a0, (K * K - 2 * zeta * K + 1) / a0 };
        };
        cands[nc++] = bilinear (juce::jmin (w, 0.995 * pi));
        cands[nc++] = bilinear (juce::jmin (a.w0 / fs, 0.995 * pi));
        // a sweep of warp points between the band and Nyquist: the fit picks the one whose pole placement lets the
        // numerator follow the analog curve best (this is what keeps top-octave bells honest)
        if (a.w0 / fs > 0.15 * pi)
            for (double f : { 0.55, 0.65, 0.75, 0.85, 0.92, 0.97 })
                cands[nc++] = bilinear (juce::jmax (a.w0 / fs, f * pi));

        const auto grid = makeGrid (fs, a.w0 / fs, a.d1 > 0 ? std::sqrt (a.d0 * a.d2) / a.d1 : 1.0);
        double target[fitGrid];
        for (int i = 0; i < grid.n; ++i) target[i] = a.mag2 (grid.w[i] * fs);
        double bestErr = 1e300; Biquad best;
        for (int c = 0; c < nc; ++c)
        {
            double err = 0; Biquad q;
            if (fitNumerator (cands[c].a1, cands[c].a2, grid, target, a.mag2 (0), fs, q, err) && err < bestErr) { bestErr = err; best = q; }
        }
        d = best;
        return d;
    }


    /** A whole band: every section matched, then (for bands of several sections) three Gauss-Seidel sweeps in which
        each second-order section's numerator is refitted against the analog total divided by what the OTHER digital
        sections really do - so errors near Nyquist don't stack up section by section. */
    inline std::vector<Biquad> designBand (const Sections& sections, double fs)
    {
        std::vector<Biquad> d; d.reserve (sections.size());
        for (auto& a : sections) d.push_back (match (a, fs));
        if (sections.size() < 2) return d;
        const auto grid = makeGrid (fs, sections.front().w0 / fs, 2.0);
        const double* gw = grid.w; const int G = grid.n;
        double total[fitGrid]; double dcTotal = 1;
        for (int i = 0; i < G; ++i) { double t = 1; for (auto& a : sections) t *= a.mag2 (gw[i] * fs); total[i] = t; }
        for (auto& a : sections) dcTotal *= a.mag2 (0);
        auto sweepFit = [&] (std::vector<Biquad>& d)
        {
        for (int sweep = 0; sweep < 3; ++sweep)
            for (size_t k = 0; k < d.size(); ++k)
            {
                if (sections[k].firstOrder() || sections[k].allPass) continue;
                double target[fitGrid]; double dcOthers = 1;
                for (int i = 0; i < G; ++i)
                {
                    double others = 1;
                    for (size_t j = 0; j < d.size(); ++j) if (j != k) others *= std::norm (d[j].at (gw[i]));
                    target[i] = total[i] / std::max (1e-300, others);
                }
                for (size_t j = 0; j < d.size(); ++j) if (j != k) dcOthers *= std::norm (d[j].at (0));
                Biquad q; double err;
                if (dcOthers > 1e-300 && fitNumerator (d[k].a1, d[k].a2, grid, target, dcTotal / dcOthers, fs, q, err)) d[k] = q;
                else if (dcOthers <= 1e-300 && fitNumerator (d[k].a1, d[k].a2, grid, target, 0, fs, q, err)) d[k] = q;
            }
        };
        auto worst = [&] (const std::vector<Biquad>& d)
        {
            double w = 0;
            for (int i = 0; i < G; ++i)
            {
                if (total[i] < 1e-7) continue;
                double t = 1; for (auto& q : d) t *= std::norm (q.at (gw[i]));
                w = std::max (w, std::abs (std::log (std::max (1e-300, t) / total[i])));
            }
            return w;
        };
        sweepFit (d);
        // alternative: every second-order section's poles from a bilinear map warped at the band frequency or above
        for (double warp : { 1.0, 1.08, 1.16 })
        {
            std::vector<Biquad> alt = d;
            bool ok = true;
            for (size_t k = 0; k < alt.size(); ++k)
            {
                const auto& a = sections[k]; if (a.firstOrder() || a.allPass) continue;
                const double wp = a.w0 * std::sqrt (a.d0 / a.d2), zeta = a.d1 / (2 * std::sqrt (a.d0 * a.d2));
                const double ww = juce::jlimit (1e-6, 0.995 * pi, a.w0 / fs * warp);
                const double K = (ww * fs / wp) / std::tan (ww / 2), a0 = K * K + 2 * zeta * K + 1;
                alt[k].a1 = 2 * (1 - K * K) / a0; alt[k].a2 = (K * K - 2 * zeta * K + 1) / a0;
                if (std::abs (alt[k].a2) >= 1) ok = false;
            }
            if (! ok) continue;
            sweepFit (alt);
            if (worst (alt) < worst (d)) d = alt;
        }
        return d;
    }

    //==========================================================================
    /** Transposed direct form II, double precision (a 20 Hz bell at Q 10 stays exact). */
    struct Tdf2
    {
        double z1 = 0, z2 = 0;
        double run (const Biquad& c, double x) { double y = c.b0 * x + z1; z1 = c.b1 * x - c.a1 * y + z2; z2 = c.b2 * x - c.a2 * y; return y; }
        void reset() { z1 = z2 = 0; }
    };

    /** A band realised as digital sections, two channels. Coefficients can glide (dynamic bands). */
    struct DigitalBand
    {
        std::vector<Biquad> cur, target, step;
        std::vector<std::array<Tdf2, 2>> st;
        int glide = 0;
        void design (const Sections& s, double fs, int glideSamples)
        {
            std::vector<Biquad> t = designBand (s, fs);
            if (t.size() != cur.size() || glideSamples <= 0)
            {
                cur = target = t; step.assign (t.size(), {}); for (auto& z : step) { z.b0 = z.b1 = z.b2 = z.a1 = z.a2 = 0; }
                if (st.size() != t.size()) st.assign (t.size(), {});
                glide = 0; return;
            }
            target = t; step.resize (t.size()); glide = glideSamples;
            const double k = 1.0 / glideSamples;
            for (size_t i = 0; i < t.size(); ++i)
            {
                step[i].b0 = (t[i].b0 - cur[i].b0) * k; step[i].b1 = (t[i].b1 - cur[i].b1) * k; step[i].b2 = (t[i].b2 - cur[i].b2) * k;
                step[i].a1 = (t[i].a1 - cur[i].a1) * k; step[i].a2 = (t[i].a2 - cur[i].a2) * k;
            }
        }
        inline void advance()
        {
            if (glide <= 0) return;
            for (size_t i = 0; i < cur.size(); ++i)
            { cur[i].b0 += step[i].b0; cur[i].b1 += step[i].b1; cur[i].b2 += step[i].b2; cur[i].a1 += step[i].a1; cur[i].a2 += step[i].a2; }
            if (--glide == 0) cur = target;
        }
        inline double run (int ch, double x) { for (size_t i = 0; i < cur.size(); ++i) x = st[i][(size_t) ch].run (cur[i], x); return x; }
        void reset() { for (auto& s : st) for (auto& t : s) t.reset(); }
        std::complex<double> at (double w) const { std::complex<double> h (1, 0); for (auto& c : cur) h *= c.at (w); return h; }
    };

    //==========================================================================
    /** Uniformly partitioned overlap-save convolution (one input, one FIR). Latency = partition size. */
    class PartitionedFir
    {
    public:
        void prepare (int partition, int firLength)
        {
            B = partition; order = 0; while ((1 << order) < 2 * B) ++order;
            fft = std::make_unique<juce::dsp::FFT> (order);
            N = 1 << order;
            P = juce::jmax (1, (firLength + B - 1) / B);
            parts.assign ((size_t) P, std::vector<std::complex<float>> ((size_t) N));
            fdl.assign ((size_t) P, std::vector<std::complex<float>> ((size_t) N));
            in.assign ((size_t) N, 0.0f); out.assign ((size_t) B, 0.0f);
            work.assign ((size_t) N, {}); acc.assign ((size_t) N, {});
            pos = 0; head = 0; filled = 0;
        }
        /** Loads new taps. The next block is computed with the old AND the new taps and crossfaded, so moving a band
            in Linear mode never clicks. History is kept (no re-prepare unless the length changes). */
        void load (const float* fir, int len)
        {
            const bool had = active;
            if (had) { partsOld = parts; fadePending = true; }
            for (int p = 0; p < P; ++p)
            {
                std::fill (work.begin(), work.end(), std::complex<float>());
                for (int i = 0; i < B; ++i) { const int k = p * B + i; if (k < len) work[(size_t) i] = fir[k]; }
                fft->perform (work.data(), parts[(size_t) p].data(), false);
            }
            active = len > 0;
        }
        bool ready() const { return fft != nullptr && active; }
        bool preparedFor (int partition, int firLength) const { return fft != nullptr && B == partition && P == juce::jmax (1, (firLength + partition - 1) / partition); }
        /** One sample in, one sample out (latency B). */
        inline float process (float x)
        {
            in[(size_t) (B + pos)] = x;
            const float y = out[(size_t) pos];
            if (++pos == B) { block(); pos = 0; }
            return y;
        }
        void reset() { for (auto& f : fdl) std::fill (f.begin(), f.end(), std::complex<float>()); std::fill (in.begin(), in.end(), 0.0f); std::fill (out.begin(), out.end(), 0.0f); pos = 0; }
        bool active = false;
    private:
        void block()
        {
            for (int i = 0; i < N; ++i) work[(size_t) i] = in[(size_t) i];
            head = (head + P - 1) % P;
            fft->perform (work.data(), fdl[(size_t) head].data(), false);
            auto convolve = [&] (const std::vector<std::vector<std::complex<float>>>& taps, std::vector<float>& dest)
            {
                std::fill (acc.begin(), acc.end(), std::complex<float>());
                for (int p = 0; p < P; ++p)
                {
                    auto& X = fdl[(size_t) ((head + p) % P)]; auto& H = taps[(size_t) p];
                    for (int k = 0; k < N; ++k) acc[(size_t) k] += X[(size_t) k] * H[(size_t) k];
                }
                fft->perform (acc.data(), work.data(), true);
                for (int i = 0; i < B; ++i) dest[(size_t) i] = work[(size_t) (B + i)].real();
            };
            convolve (parts, out);
            if (fadePending)
            {
                fadeOut.resize ((size_t) B); convolve (partsOld, fadeOut);
                for (int i = 0; i < B; ++i) { const float t = (i + 0.5f) / (float) B; out[(size_t) i] = out[(size_t) i] * t + fadeOut[(size_t) i] * (1 - t); }
                fadePending = false;
            }
            std::copy (in.begin() + B, in.begin() + 2 * B, in.begin());
        }
        int B = 256, N = 512, P = 1, order = 9, pos = 0, head = 0, filled = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<std::vector<std::complex<float>>> parts, partsOld, fdl;
        std::vector<float> in, out, fadeOut;
        bool fadePending = false;
        std::vector<std::complex<float>> work, acc;
    };

    //==========================================================================
    /** Polyphase half-band pair for Natural mode (2x): 95 taps, Kaiser beta 9 (about -90 dB stop band, flat to
        0.42 x the 2x rate, i.e. past 20 kHz at 44.1 kHz). Latency: 23.5 + 23.5 samples at the 1x rate. */
    struct HalfBand
    {
        static constexpr int K = 24;                                         // odd-distance taps
        static const std::array<double, K>& taps()
        {
            static const std::array<double, K> t = [] {
                std::array<double, K> c {};
                const int M = 2 * K - 1; const double beta = 9.0;              // half length 47
                auto i0 = [] (double x) { double sum = 1, term = 1; for (int k = 1; k < 50; ++k) { term *= (x / (2 * k)) * (x / (2 * k)); sum += term; } return sum; };
                for (int j = 0; j < K; ++j)
                {
                    const int n = 2 * j + 1;
                    const double h = std::sin (pi * n / 2.0) / (pi * n);
                    const double r = (double) n / (M + 1), w = i0 (beta * std::sqrt (juce::jmax (0.0, 1 - r * r))) / i0 (beta);
                    c[(size_t) j] = h * w;
                }
                return c;
            }();
            return t;
        }
        static constexpr int len = 64, msk = 63, D = K - 1;                  // centre delay in 1x samples (23)
        std::array<double, len> upBuf {}, dnA {}, dnB {};
        int upPos = 0, dnPos = 0;
        void reset() { upBuf.fill (0); dnA.fill (0); dnB.fill (0); upPos = dnPos = 0; }
        /** One input -> two outputs in time order. */
        inline void up (double x, double& first, double& second)
        {
            upBuf[(size_t) upPos] = x;
            const auto& t = taps(); double s = 0;
            for (int j = 0; j < K; ++j) s += t[(size_t) j] * (upBuf[(size_t) ((upPos - (D - j)) & msk)] + upBuf[(size_t) ((upPos - (D + 1 + j)) & msk)]);
            first = 2 * s;                                                   // the in-between sample (time n - 23.5)
            second = upBuf[(size_t) ((upPos - D) & msk)];                    // the original sample (time n - 23)
            upPos = (upPos + 1) & msk;
        }
        /** Two inputs (time order) -> one output. */
        inline double down (double first, double second)
        {
            dnA[(size_t) dnPos] = first; dnB[(size_t) dnPos] = second;
            const auto& t = taps(); double s = 0;
            for (int j = 0; j < K; ++j) s += t[(size_t) j] * (dnB[(size_t) ((dnPos - (D - j)) & msk)] + dnB[(size_t) ((dnPos - (D + 1 + j)) & msk)]);
            const double y = 0.5 * dnA[(size_t) ((dnPos - D) & msk)] + s;
            dnPos = (dnPos + 1) & msk;
            return y;
        }
    };

    //==========================================================================
    class Engine
    {
    public:
        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            for (auto& h : hb) h.reset();
            dirty = true; firDirty = true;
            for (auto& e : env) e = 0;
            rebuild (true);
        }
        void setPhase (Phase p, int linearQuality = 2)
        {
            if (p != phase || linearQuality != quality) { phase = p; quality = juce::jlimit (0, 4, linearQuality); dirty = firDirty = true; ++generation; }
        }
        Phase getPhase() const { return phase; }
        void setBand (int i, const Band& b)
        {
            if (i < 0 || i >= maxBands || bands[(size_t) i] == b) return;
            // a dynamic band's threshold / range / timing don't change the static filters
            Band x = bands[(size_t) i], y = b; x.threshDb = y.threshDb; x.rangeDb = y.rangeDb; x.attackMs = y.attackMs; x.releaseMs = y.releaseMs; x.externalKey = y.externalKey;
            const bool staticChange = ! (x == y);
            bands[(size_t) i] = b;
            if (staticChange) { dirty = firDirty = true; ++generation; }
        }
        /** Counts every change that needs new filters (the plug-in matches background FIR builds against it). */
        uint64_t bandsGeneration() const { return generation; }
        /** Only the sample rate (for latency queries without a full prepare). */
        void prepareRateOnly (double sampleRate) { fs = sampleRate; }
        const Band& band (int i) const { return bands[(size_t) i]; }
        void setOutput (double gainDb, double panLr = 0) { outGain = std::pow (10.0, gainDb / 20.0); pan = juce::jlimit (-1.0, 1.0, panLr); }

        /** FIR length for each Linear quality step (at 48 kHz; scaled with the sample rate). */
        int firLength() const { static const int L[5] = { 2048, 4096, 8192, 16384, 32768 }; int n = L[quality]; while (fs > 50000 && n < 65536 && n * 48000.0 < fs * L[quality]) n *= 2; return n; }
        int latency() const
        {
            if (phase == Phase::linear) return firLength() / 2 + partition;
            if (phase == Phase::natural) return 47;                         // 23.5 + 23.5 samples of the half-band pair
            return 0;
        }
        /** The current gain movement of a dynamic band (dB), for the display and Claude. */
        double dynamicDb (int i) const { return dyn[(size_t) i]; }

        /** What the band does right now, digital (Zero latency / Natural) or exact analog (Linear). */
        std::complex<double> response (double hz, int channelMatrixRow = 0) const
        {
            juce::ignoreUnused (channelMatrixRow);
            std::complex<double> h (1, 0);
            for (int i = 0; i < maxBands; ++i)
            {
                const auto& b = bands[(size_t) i]; if (! b.enabled) continue;
                if (phase == Phase::linear) h *= analogResponse (prototype (b, b.gainDb + dyn[(size_t) i]), hz);
                else h *= dig[(size_t) i].at (2 * pi * hz / (phase == Phase::natural ? 2 * fs : fs));
            }
            return h;
        }

        void process (float* const* ch, int numCh, int n, const float* const* key = nullptr, int keyCh = 0)
        {
            if (dirty) rebuild (false);
            const bool stereo = numCh >= 2;
            if (phase == Phase::linear)
            {
                if (firDirty && ! asyncFir) buildFir();
                if (! fir[0].ready()) { for (int s = 0; s < n; ++s) for (int c = 0; c < numCh; ++c) ch[c][s] = (float) (ch[c][s] * outGain); return; }
                // dynamic bands run as minimum-phase sections around the FIR (their static part is in the FIR)
                for (int s = 0; s < n; ++s)
                {
                    const double l = ch[0][s], r = stereo ? ch[1][s] : l;
                    double yl = fir[0].process ((float) l), yr = yl;
                    if (stereo)
                    {
                        yl = (fir[0].active ? yl : 0) + (fir[1].active ? fir[1].process ((float) r) : 0);
                        yr = (fir[2].active ? fir[2].process ((float) l) : 0) + (fir[3].active ? fir[3].process ((float) r) : 0);
                    }
                    double out[2] = { yl, yr };
                    runDynamicOnly (out, stereo, key, keyCh, s, 0.5 * (l + r));
                    ch[0][s] = (float) (out[0] * outGain * panGain (0));
                    if (stereo) ch[1][s] = (float) (out[1] * outGain * panGain (1));
                }
                return;
            }
            for (int s = 0; s < n; ++s)
            {
                if ((counter++ & 31) == 0) updateDynamics (key, keyCh, ch, s, stereo);
                double x[2] = { ch[0][s], stereo ? ch[1][s] : ch[0][s] };
                const double keyIn = 0.5 * (x[0] + x[1]);
                if (phase == Phase::natural)
                {
                    double a0, a1, b0, b1; hb[0].up (x[0], a0, a1); hb[1].up (x[1], b0, b1);
                    double p[2] = { a0, b0 }, q[2] = { a1, b1 };
                    runBands (p, stereo); runBands (q, stereo);
                    x[0] = hb[2].down (p[0], q[0]); x[1] = hb[3].down (p[1], q[1]);
                }
                else runBands (x, stereo);
                ch[0][s] = (float) (x[0] * outGain * panGain (0));
                if (stereo) ch[1][s] = (float) (x[1] * outGain * panGain (1));
                trackKeyLevel (key, keyCh, keyIn, s);
            }
        }

        void reset() { for (auto& d : dig) d.reset(); for (auto& f : fir) f.reset(); for (auto& h : hb) h.reset(); for (auto& e : env) e = 0; }

    private:
        double fs = 48000;
        Phase phase = Phase::zeroLatency;
        int quality = 2, partition = 256;
        std::array<Band, maxBands> bands {};
        std::array<DigitalBand, maxBands> dig;
        std::array<DigitalBand, maxBands> keyBp;          // the band-pass each dynamic band listens through
        std::array<double, maxBands> env {}, dyn {};
        std::array<PartitionedFir, 4> fir;
        std::array<HalfBand, 4> hb;
        bool dirty = true, firDirty = true;
        uint64_t generation = 1;
        double outGain = 1, pan = 0;
        uint32_t counter = 0;
        std::array<double, maxBands> keyAcc {};

        double panGain (int c) const { return c == 0 ? juce::jmin (1.0, 1.0 - pan) : juce::jmin (1.0, 1.0 + pan); }
        double rate() const { return phase == Phase::natural ? 2 * fs : fs; }

        void rebuild (bool hard)
        {
            for (int i = 0; i < maxBands; ++i)
            {
                const auto& b = bands[(size_t) i];
                if (! b.enabled) continue;
                dig[(size_t) i].design (prototype (b, b.gainDb + (b.dynamic ? dyn[(size_t) i] : 0)), rate(), hard ? 0 : 64);
                if (b.dynamic)
                {
                    Band bp = b; bp.shape = Shape::bandPass; bp.slope = 12;
                    if (b.shape == Shape::lowShelf)  { bp.shape = Shape::highCut; bp.q = std::sqrt (0.5); }
                    if (b.shape == Shape::highShelf) { bp.shape = Shape::lowCut;  bp.q = std::sqrt (0.5); }
                    keyBp[(size_t) i].design (prototype (bp, 0), fs, 0);
                }
            }
            dirty = false;
        }

        inline void runBands (double* x, bool stereo)
        {
            bool ms = false;
            for (int i = 0; i < maxBands; ++i)
            {
                const auto& b = bands[(size_t) i]; if (! b.enabled) continue;
                auto& d = dig[(size_t) i];
                d.advance();
                const bool wantMs = b.place == Place::mid || b.place == Place::side;
                if (stereo && wantMs != ms)
                {
                    if (wantMs) { const double m = 0.5 * (x[0] + x[1]), s = 0.5 * (x[0] - x[1]); x[0] = m; x[1] = s; }
                    else        { const double l = x[0] + x[1], r = x[0] - x[1]; x[0] = l; x[1] = r; }
                    ms = wantMs;
                }
                switch (b.place)
                {
                    case Place::stereo: x[0] = d.run (0, x[0]); if (stereo) x[1] = d.run (1, x[1]); break;
                    case Place::left:   x[0] = d.run (0, x[0]); break;
                    case Place::right:  if (stereo) x[1] = d.run (1, x[1]); else x[0] = d.run (0, x[0]); break;
                    case Place::mid:    x[0] = d.run (0, x[0]); break;
                    case Place::side:   if (stereo) x[1] = d.run (1, x[1]); break;
                }
            }
            if (ms) { const double l = x[0] + x[1], r = x[0] - x[1]; x[0] = l; x[1] = r; }
            if (! stereo) x[1] = x[0];
        }

        // ---- dynamics: the key level through each dynamic band's band-pass, peak-RMS hybrid with attack / release
        inline void trackKeyLevel (const float* const* key, int keyCh, double internalKey, int s)
        {
            for (int i = 0; i < maxBands; ++i)
            {
                const auto& b = bands[(size_t) i]; if (! (b.enabled && b.dynamic && b.canBeDynamic())) continue;
                double k;
                if (b.externalKey && key != nullptr && keyCh > 0) k = keyCh > 1 ? 0.5 * (key[0][s] + key[1][s]) : key[0][s];
                else k = internalKey;
                const double v = keyBp[(size_t) i].run (0, k);
                keyAcc[(size_t) i] = juce::jmax (keyAcc[(size_t) i], std::abs (v));
            }
        }

        void updateDynamics (const float* const* key, int keyCh, float* const* ch, int s, bool stereo)
        {
            juce::ignoreUnused (key, keyCh, ch, s, stereo);
            bool any = false;
            for (int i = 0; i < maxBands; ++i)
            {
                auto& b = bands[(size_t) i]; if (! (b.enabled && b.dynamic && b.canBeDynamic())) { dyn[(size_t) i] = 0; continue; }
                const double peak = keyAcc[(size_t) i]; keyAcc[(size_t) i] = 0;
                const double block = 32.0 / fs;
                const double at = std::exp (-block / juce::jmax (0.0001, b.attackMs * 0.001));
                const double rt = std::exp (-block / juce::jmax (0.001, b.releaseMs * 0.001));
                auto& e = env[(size_t) i];
                e = peak > e ? at * e + (1 - at) * peak : rt * e + (1 - rt) * peak;
                const double lev = 20 * std::log10 (e + 1e-12);
                const double over = juce::jmax (0.0, lev - b.threshDb);
                // the band moves 1 dB per 2 dB over the threshold until it reaches its range (a 2:1 knee, soft for 3 dB)
                double move = over < 3 ? over * over / 12.0 : (over - 1.5) * 0.5;
                move = juce::jmin (std::abs (b.rangeDb), move) * (b.rangeDb < 0 ? -1.0 : 1.0);
                if (std::abs (move - dyn[(size_t) i]) > 0.01)
                {
                    dyn[(size_t) i] = move; any = true;
                    dig[(size_t) i].design (prototype (b, b.gainDb + move), rate(), phase == Phase::natural ? 64 : 32);
                }
            }
            juce::ignoreUnused (any);
        }

        // Linear mode: static FIR plus the dynamic movement as minimum-phase gain-only sections
        std::array<DigitalBand, maxBands> dynOnly;
        inline void runDynamicOnly (double* x, bool stereo, const float* const* key, int keyCh, int s, double keyIn)
        {
            if ((counter++ & 31) == 0)
            {
                for (int i = 0; i < maxBands; ++i)
                {
                    auto& b = bands[(size_t) i]; if (! (b.enabled && b.dynamic && b.canBeDynamic())) continue;
                    const double peak = keyAcc[(size_t) i]; keyAcc[(size_t) i] = 0;
                    const double block = 32.0 / fs;
                    const double at = std::exp (-block / juce::jmax (0.0001, b.attackMs * 0.001));
                    const double rt = std::exp (-block / juce::jmax (0.001, b.releaseMs * 0.001));
                    auto& e = env[(size_t) i];
                    e = peak > e ? at * e + (1 - at) * peak : rt * e + (1 - rt) * peak;
                    const double over = juce::jmax (0.0, 20 * std::log10 (e + 1e-12) - b.threshDb);
                    double move = over < 3 ? over * over / 12.0 : (over - 1.5) * 0.5;
                    move = juce::jmin (std::abs (b.rangeDb), move) * (b.rangeDb < 0 ? -1.0 : 1.0);
                    if (std::abs (move - dyn[(size_t) i]) > 0.01)
                    {
                        dyn[(size_t) i] = move;
                        Band g = b; g.gainDb = move;                         // only the movement
                        dynOnly[(size_t) i].design (prototype (g, move), fs, 32);
                    }
                }
            }
            for (int i = 0; i < maxBands; ++i)
            {
                auto& b = bands[(size_t) i]; if (! (b.enabled && b.dynamic && b.canBeDynamic()) || dynOnly[(size_t) i].cur.empty()) continue;
                auto& d = dynOnly[(size_t) i]; d.advance();
                x[0] = d.run (0, x[0]); if (stereo) x[1] = d.run (1, x[1]);
            }
            trackKeyLevel (key, keyCh, keyIn, s);
        }

    public:
        /** The four Linear-mode filters (LL, LR, RL, RR) for a set of bands: heavy (an FFT of 2L plus every band's
            analog curve on every bin), so the plug-in runs it on a background thread and hands the result in. */
        struct FirSet { std::array<std::vector<float>, 4> h; std::array<bool, 4> active {}; int length = 0; };
        static FirSet computeFirs (const std::array<Band, maxBands>& bands, double fs, int L)
        {
            const int nfft = 2 * L; int order = 0; while ((1 << order) < nfft) ++order;
            const int N = 1 << order;
            // 2x2 matrix per bin: start with identity
            std::vector<std::array<double, 4>> M ((size_t) (N / 2 + 1), { 1, 0, 0, 1 });
            for (int i = 0; i < maxBands; ++i)
            {
                const auto& b = bands[(size_t) i]; if (! b.enabled) continue;
                const auto proto = prototype (b, b.gainDb);
                for (int k = 0; k <= N / 2; ++k)
                {
                    const double hz = juce::jmax (1e-3, (double) k * fs / N);
                    double h = std::abs (analogResponse (proto, hz));
                    if (b.shape == Shape::allPass) h = 1;
                    std::array<double, 4> B { 1, 0, 0, 1 };
                    switch (b.place)
                    {
                        case Place::stereo: B = { h, 0, 0, h }; break;
                        case Place::left:   B = { h, 0, 0, 1 }; break;
                        case Place::right:  B = { 1, 0, 0, h }; break;
                        case Place::mid:    B = { 0.5 * (h + 1), 0.5 * (h - 1), 0.5 * (h - 1), 0.5 * (h + 1) }; break;
                        case Place::side:   B = { 0.5 * (1 + h), 0.5 * (1 - h), 0.5 * (1 - h), 0.5 * (1 + h) }; break;
                    }
                    auto& m = M[(size_t) k];
                    m = { B[0] * m[0] + B[1] * m[2], B[0] * m[1] + B[1] * m[3], B[2] * m[0] + B[3] * m[2], B[2] * m[1] + B[3] * m[3] };
                }
            }
            juce::dsp::FFT fft (order);
            std::vector<std::complex<float>> spec ((size_t) N), time ((size_t) N);
            FirSet set; set.length = L;
            for (int e = 0; e < 4; ++e)
            {
                bool nonzero = false;
                for (int k = 0; k <= N / 2; ++k)
                {
                    const double v = M[(size_t) k][(size_t) e];
                    if (std::abs (v) > 1e-9) nonzero = true;
                    spec[(size_t) k] = (float) v; if (k > 0 && k < N / 2) spec[(size_t) (N - k)] = (float) v;
                }
                if (! nonzero) { set.active[(size_t) e] = false; continue; }
                auto& h = set.h[(size_t) e]; h.assign ((size_t) L, 0.0f); set.active[(size_t) e] = true;
                fft.perform (spec.data(), time.data(), true);
                // zero-phase impulse centred at L/2, Blackman-Harris window over L taps
                for (int i = 0; i < L; ++i)
                {
                    const int t = (i - L / 2 + N) % N;
                    const double a = 2 * pi * (i + 0.5) / L;
                    const double w = 0.35875 - 0.48829 * std::cos (a) + 0.14128 * std::cos (2 * a) - 0.01168 * std::cos (3 * a);
                    h[(size_t) i] = (float) (time[(size_t) t].real() * w);
                }
            }
            return set;
        }
        /** Copies of the bands, for a background FIR build. */
        const std::array<Band, maxBands>& allBands() const { return bands; }
        double sampleRate() const { return fs; }
        /** True when Linear mode needs new filters (the plug-in builds them off the audio thread). */
        bool firNeeded() const { return phase == Phase::linear && firDirty; }
        /** In the plug-in the FIRs come from a background thread; in tests (default) they are built in place. */
        void setAsyncFir (bool a) { asyncFir = a; }
        /** Audio thread: install filters built by computeFirs (crossfaded, history kept). */
        void applyFirs (const FirSet& set)
        {
            for (int e = 0; e < 4; ++e)
            {
                auto& f = fir[(size_t) e];
                if (! set.active[(size_t) e]) { f.active = false; continue; }
                if (! f.preparedFor (partition, set.length)) f.prepare (partition, set.length);
                f.load (set.h[(size_t) e].data(), set.length);
            }
            firDirty = false;
        }
    private:
        bool asyncFir = false;
        void buildFir() { applyFirs (computeFirs (bands, fs, firLength())); }
    };
}
