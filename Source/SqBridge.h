#pragma once
// SqBridge: a diode-bridge compressor solved as a circuit. Pure C++ (no JUCE).
//
// The gain element is four diodes in a ring. The signal goes in balanced, +half and -half, each through a resistor
// to opposite corners of the ring (A and B); a control current is pushed into the third corner (P) and drawn out of
// the fourth (N). Diodes run P to A, P to B, A to N and B to N. With no control current the diodes are off and the
// signal passes; the more current, the lower the diodes' resistance and the more of the signal they shunt away.
// The four corner voltages are found every sample by Newton's method on the diodes' own law, so the bridge's
// gentle bending of the signal - a little 3rd harmonic that grows as it works - comes out of the diodes.
#include "SqClip.h"

namespace SqBridge
{
    struct Circuit { double r = 4700.0, leak = 1.0e9; };       // each series resistor; a leak from P and from N to ground (it only pins them when no current flows)

    struct Bridge
    {
        void prepare (const SqClip::Diode& d1, const SqClip::Diode& d2, const SqClip::Diode& d3, const SqClip::Diode& d4, const Circuit& circuit) { d[0] = d1; d[1] = d2; d[2] = d3; d[3] = d4; c = circuit; for (int k = 0; k < 4; ++k) tab[k].prepare (d[k]); reset(); }
        void reset() { v[0] = v[1] = 0.0; v[2] = 0.3; v[3] = -0.3; }
        /** Volts in (between the two inputs), control current in amps. Returns the volts between corners A and B. */
        double process (double vin, double control)
        {
            for (int it = 0; it < 60; ++it)
            {
                double i[4], g[4]; SqClip::diode (d[0], v[2] - v[0], i[0], g[0], nullptr, &tab[0]); SqClip::diode (d[1], v[0] - v[3], i[1], g[1], nullptr, &tab[1]); SqClip::diode (d[2], v[2] - v[1], i[2], g[2], nullptr, &tab[2]); SqClip::diode (d[3], v[1] - v[3], i[3], g[3], nullptr, &tab[3]);
                const double gr = 1.0 / c.r, gl = 1.0 / c.leak;
                double f[4] = { (0.5 * vin - v[0]) * gr + i[0] - i[1], (-0.5 * vin - v[1]) * gr + i[2] - i[3], control - i[0] - i[2] - v[2] * gl, i[1] + i[3] - control - v[3] * gl };
                double J[4][5] = { { -gr - g[0] - g[1], 0.0, g[0], g[1], -f[0] }, { 0.0, -gr - g[2] - g[3], g[2], g[3], -f[1] }, { g[0], g[2], -g[0] - g[2] - gl, 0.0, -f[2] }, { g[1], g[3], 0.0, -g[1] - g[3] - gl, -f[3] } };
                for (int a = 0; a < 4; ++a) { int p = a; for (int b = a + 1; b < 4; ++b) if (std::abs (J[b][a]) > std::abs (J[p][a])) p = b; if (p != a) for (int k = 0; k < 5; ++k) std::swap (J[a][k], J[p][k]);
                    for (int b = a + 1; b < 4; ++b) { const double m = J[b][a] / J[a][a]; for (int k = a; k < 5; ++k) J[b][k] -= m * J[a][k]; } }
                double dv[4]; for (int a = 3; a >= 0; --a) { double s = J[a][4]; for (int k = a + 1; k < 4; ++k) s -= J[a][k] * dv[k]; dv[a] = s / J[a][a]; }
                double biggest = 0.0; for (int a = 0; a < 4; ++a) { const double step = std::max (-0.1, std::min (0.1, dv[a])); v[a] += step; biggest = std::max (biggest, std::abs (step)); }       // a diode's law is steep: no corner moves more than a tenth of a volt at a time
                if (biggest < 1.0e-12) break;
            }
            return v[0] - v[1];
        }
        /** Gain for small signals at a control current (matched diodes: 1 / (1 + R I / (n Vt)) to a good approximation). */
        double gainAt (double control) { const double a = process (1.0e-6, control), b = process (-1.0e-6, control); return (a - b) / 2.0e-6; }

        SqClip::Diode d[4]; SqClip::Table tab[4]; Circuit c; double v[4] = { 0.0, 0.0, 0.3, -0.3 };
    };

    /** The sidechain: the output over the threshold charges a capacitor through the attack time and lets go through the
        recovery time; in the two automatic settings a second, slow capacitor follows the first and lets go far more slowly,
        so a long loud passage recovers slowly and a single peak quickly. The control current is the capacitor's voltage
        raised to (ratio - 1): with that law the ratio holds however far above the threshold the signal goes. */
    struct Sidechain { double threshold = 0.25, ratio = 3.0, attack = 0.003, release = 0.4, slowRelease = 0.0 /* 0: off */, slowCharge = 0.5, highPassHz = 0.0 /* 0: off */; };

    struct Compressor
    {
        static constexpr double driverLag = 2.0e-4;
        void prepare (double sampleRate, const SqClip::Diode ring[8], const Circuit& circuit, double voltsAtFullScale)
        {
            fs = sampleRate; volts = voltsAtFullScale; st[0].prepare (ring[0], ring[1], ring[2], ring[3], circuit); st[1].prepare (ring[4], ring[5], ring[6], ring[7], circuit);
            unit = ring[0].n * SqClip::thermalVolts / circuit.r; for (int i = 0; i < 2; ++i) open[i] = 1.0 / (st[i].gainAt (0.0) * volts);
            reset();
        }
        void reset() { fast = slow = drive = 0.0; hp[0] = hp[1] = hpIn[0] = hpIn[1] = 0.0; for (int i = 0; i < 16; ++i) { seen[i] = 0.0; other[i] = 0.0; } }
        void set (const Sidechain& sc) { s = sc; kA = 1.0 / (s.attack * fs); kR = 1.0 / (s.release * fs); kD = 1.0 - std::exp (-1.0 / (driverLag * fs)); kS = s.slowRelease > 0.0 ? 1.0 / (s.slowRelease * fs) : 0.0; kC = 1.0 / (s.slowCharge * fs); hpA = s.highPassHz > 0.0 ? std::exp (-2.0 * 3.14159265358979323846 * s.highPassHz / fs) : 0.0; }
        double held() const { return s.slowRelease > 0.0 ? std::max (fast, slow) : fast; }
        /** The control current now, in amps: the unit current (the one that halves the signal) times held ^ (ratio - 1). */
        double wanted() const { const double x = held(); return x > 0.0 ? unit * std::pow (x, s.ratio - 1.0) : 0.0; }
        /** The current the driver is giving now: it follows the wanted one through its own short lag (0.2 ms). */
        double control() const { return drive; }
        double reductionDb() { return 20.0 * std::log10 (st[0].gainAt (control()) * open[0] * volts); }

        /** A channel's output as the sidechain hears it: on Slow it goes through a 100 Hz high-pass first, so bass moves the gain less. */
        double hear (int ch, double y) { if (hpA > 0.0) { const double h = hpA * (hp[ch] + y - hpIn[ch]); hpIn[ch] = y; hp[ch] = h; y = h; } return std::abs (y); }
        void advance (double heard)
        {
            const double over = heard / s.threshold - 1.0;
            fast += std::max (over - fast, 0.0) * kA - fast * kR; if (fast < 0.0) fast = 0.0;
            if (kS > 0.0) { slow += std::max (fast - slow, 0.0) * kC - slow * kS; if (slow < 0.0) slow = 0.0; }
            drive += (wanted() - drive) * kD;
        }
        double through (int ch, double v, double ctl) { return st[ch].process (v * volts, ctl) * open[ch]; }
        void process (double& l, double& r) { const double ctl = control(), yl = through (0, l, ctl), yr = through (1, r, ctl); advance (std::max (hear (0, yl), hear (1, yr))); l = yl; r = yr; }
        double process (double v) { const double y = through (0, v, control()); advance (hear (0, y)); return y; }
        /** A channel at a time, for an oversampler that hands over the left's inner samples and then the right's: the left
            moves the sidechain, hearing the right from one outer sample ago. */
        double left (double v, int k) { const double ctl = control(); seen[k & 15] = ctl; const double y = through (0, v, ctl); advance (std::max (hear (0, y), other[k & 15])); return y; }
        double right (double v, int k) { const double y = through (1, v, seen[k & 15]); other[k & 15] = hear (1, y); return y; }
        void clearRight() { for (auto& o : other) o = 0.0; }

        Bridge st[2]; Sidechain s; double fs = 48000.0, volts = 0.03, unit = 1.0e-5, open[2] = { 1.0, 1.0 }, fast = 0.0, slow = 0.0, drive = 0.0, kD = 1.0, hp[2] = {}, hpIn[2] = {}, kA = 0, kR = 0, kS = 0, kC = 0, hpA = 0, seen[16] = {}, other[16] = {};
    };
}
