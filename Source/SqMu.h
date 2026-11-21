#pragma once
// SqMu: a variable-mu compressor - a push-pull pair of remote-cutoff triodes whose gain falls as their grids are
// pulled negative - solved as a circuit. Pure C++ (no JUCE beyond SqTube's own maths).
//
// A remote-cutoff tube has a grid wound with uneven pitch: the closely wound part cuts off early, the open part
// keeps conducting far into negative bias, so the gain can be turned down smoothly over a wide range. It is written
// here in the published form for such tubes: two plate-current laws in parallel, a main one and a small share with
// a much lower mu (the open part of the grid).
//
// Two of them work in push-pull: one grid gets +half the signal, the other -half, both ride on the control voltage,
// each plate has its own resistor to the supply and the output is the difference of the plates. Even harmonics and
// the thump of the moving control voltage cancel, as far as the two tubes match.
#include "SqTube.h"

namespace SqMu
{
    struct Remote { SqTube::Tube a, b; double share = 0.05; };       // a: the closely wound part; b: the open part (share of the current law)
    inline Remote remote() { Remote r; r.a = SqTube::mediumGain(); r.a.mu = 40.0; r.b = r.a; r.b.mu = 40.0 / 3.5; return r; }

    inline void plate (const Remote& t, double vpk, double vgk, double& ip, double& dVpk, double& dVgk)
    {
        double i1, p1, g1, i2, p2, g2; SqTube::plate (t.a, vpk, vgk, i1, p1, g1); SqTube::plate (t.b, vpk, vgk, i2, p2, g2);
        ip = (1.0 - t.share) * i1 + t.share * i2; dVpk = (1.0 - t.share) * p1 + t.share * p2; dVgk = (1.0 - t.share) * g1 + t.share * g2;
    }

    struct Circuit { double bplus = 150.0, ra = 22000.0, rest = -1.5, deepest = -12.0; };       // supply, each plate's resistor, the grids' bias with no compression, the furthest the sidechain may pull them

    /** The push-pull pair. No capacitors in the signal path: each plate settles at once for its grid voltage. */
    struct Stage
    {
        void prepare (const Remote& first, const Remote& second, const Circuit& circuit) { t[0] = first; t[1] = second; c = circuit; vp[0] = vp[1] = c.bplus * 0.5; solve (0, c.rest); solve (1, c.rest); }
        /** One plate's voltage for its grid voltage: Vp = B - Ra Ip(Vp, Vg), by Newton's method from where it was. */
        double solve (int i, double vg)
        {
            double v = vp[i];
            for (int k = 0; k < 40; ++k)
            {
                double ip, dp, dg; plate (t[i], v, vg, ip, dp, dg); const double f = v - c.bplus + c.ra * ip, step = f / (1.0 + c.ra * dp);
                v = std::min (c.bplus, std::max (0.0, v - step)); if (std::abs (step) < 1.0e-10) break;
            }
            return vp[i] = v;
        }
        /** control: volts added to the grids' bias (0 or negative). Returns the difference of the plates in volts. */
        double process (double vin, double control)
        {
            const double bias = c.rest + control; const double p0 = solve (0, bias + 0.5 * vin), p1 = solve (1, bias - 0.5 * vin); return p1 - p0;
        }
        /** Small-signal gain (volts out per volt in) at a control voltage. */
        double gainAt (double control)
        {
            double sum = 0.0;
            for (int i = 0; i < 2; ++i) { const double v = solve (i, c.rest + control); double ip, dp, dg; plate (t[i], v, c.rest + control, ip, dp, dg); sum += 0.5 * c.ra * dg / (1.0 + c.ra * dp); }
            return sum;
        }
        Remote t[2]; Circuit c; double vp[2] = { 125.0, 125.0 };
    };

    /** The sidechain: the output, rectified, charges a capacitor through the attack time whenever it is above the
        threshold; the capacitor lets go through the release time. Its voltage pulls the grids negative. */
    struct Sidechain { double threshold = 0.25, depth = 4.0, attack = 0.05, release = 0.6; };

    /** Two channels, one sidechain. Levels in and out are in full-scale units: `volts` is the signal between the
        two grids for a full-scale sample, and the output is scaled so that with no compression the gain is one. */
    struct Compressor
    {
        void prepare (double sampleRate, const Remote& l1, const Remote& l2, const Remote& r1, const Remote& r2, const Circuit& circuit, double voltsAtFullScale)
        {
            fs = sampleRate; volts = voltsAtFullScale; st[0].prepare (l1, l2, circuit); st[1].prepare (r1, r2, circuit);
            for (int i = 0; i < 2; ++i) { unity[i] = 1.0 / (st[i].gainAt (0.0) * volts); offset[i] = st[i].process (0.0, 0.0); }
            hpA = std::exp (-2.0 * 3.14159265358979323846 * 5.0 / fs); reset();
        }
        void reset() { x = 0.0; hpIn[0] = hpIn[1] = hpOut[0] = hpOut[1] = 0.0; for (int i = 0; i < 16; ++i) { seen[i] = 0.0; other[i] = 0.0; } }
        void set (const Sidechain& sc) { s = sc; kA = 1.0 / (s.attack * fs); kR = 1.0 / (s.release * fs); }
        double capacitor() const { return x; }
        double control() const { return std::max (-s.depth * x, st[0].c.deepest); }
        double reductionDb() { return 20.0 * std::log10 (st[0].gainAt (control()) * unity[0] * volts); }

        void advance (double heard)
        {
            const double over = heard / s.threshold - 1.0;
            x += std::max (over - x, 0.0) * kA - x * kR; if (x < 0.0) x = 0.0;
        }
        /** One channel through its pair at the present control voltage, then through the output transformer, which passes no
            standing voltage (here a 5 Hz high-pass): unmatched tubes thump when the control voltage moves, but do not stay off-centre. */
        double through (int ch, double v, double ctl)
        {
            const double in = (st[ch].process (v * volts, ctl) - offset[ch]) * unity[ch]; const double out = hpA * (hpOut[ch] + in - hpIn[ch]); hpIn[ch] = in; hpOut[ch] = out; return out;
        }
        void process (double& l, double& r)
        {
            const double ctl = control(), yl = through (0, l, ctl), yr = through (1, r, ctl); advance (std::max (std::abs (yl), std::abs (yr))); l = yl; r = yr;
        }
        double process (double v) { const double y = through (0, v, control()); advance (std::abs (y)); return y; }
        /** A channel at a time, for an oversampler that hands over the left's inner samples and then the right's (see SqOpto.h). */
        double left (double v, int k) { const double ctl = control(); seen[k & 15] = ctl; const double y = through (0, v, ctl); advance (std::max (std::abs (y), other[k & 15])); return y; }
        double right (double v, int k) { const double y = through (1, v, seen[k & 15]); other[k & 15] = std::abs (y); return y; }
        void clearRight() { for (auto& o : other) o = 0.0; }

        Stage st[2]; Sidechain s; double fs = 48000.0, volts = 0.2, unity[2] = { 1.0, 1.0 }, offset[2] = { 0.0, 0.0 }, x = 0.0, kA = 0.0, kR = 0.0, hpA = 1.0, hpIn[2] = {}, hpOut[2] = {}, seen[16] = {}, other[16] = {};
    };
}
