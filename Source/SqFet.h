#pragma once
// Squelchmoth 1.43 - Voltage Comp: a FET compressor from its circuit. Pure: no engine, no UI.
// The gain cell: the signal through a resistor to a node, a junction FET from that node to ground used as a resistor that
// the control voltage sets, and half of the node's own voltage fed back to the gate (which cancels most of the FET's
// distortion). The FET is the standard square-law model with its published, measured numbers and its source resistance.
// The sidechain listens to the node (so it compresses what it has already compressed: a feedback compressor), charges a
// capacitor through the attack time when the signal is over the threshold and lets it go through the release time.
// Reference: the same circuit in a circuit simulator (tests/reference/fet/make_reference.py).
#include <algorithm>
#include <cmath>

namespace SqFet
{
    /** A junction FET by its model numbers: pinch-off voltage, transconductance parameter, channel-length modulation, source resistance. */
    struct Fet { double vto = -3.15, beta = 2.25e-3, lambda = 4.3e-3, rs = 23.0; };

    /** Channel current for gate and drain voltages measured at the FET's own source (no series resistance), drain positive
        or negative, and its slopes against drain and gate. */
    inline void channel (const Fet& f, double vgs, double vds, double& id, double& dVds, double& dVgs)
    {
        if (vds < 0.0) { double i, a, b; channel (f, vgs - vds, -vds, i, a, b); id = -i; dVds = a + b; dVgs = -b; return; }      // the other end acts as the source
        const double vgst = vgs - f.vto; id = dVds = dVgs = 0.0; if (vgst <= 0.0) return;
        const double lam = 1.0 + f.lambda * vds;
        if (vds < vgst) { const double k = vds * (2.0 * vgst - vds); id = f.beta * k * lam; dVds = f.beta * ((2.0 * vgst - 2.0 * vds) * lam + k * f.lambda); dVgs = f.beta * 2.0 * vds * lam; }
        else { id = f.beta * vgst * vgst * lam; dVds = f.beta * vgst * vgst * f.lambda; dVgs = 2.0 * f.beta * vgst * lam; }
    }

    /** Drain current with the source resistance in the way (source end grounded): gate and drain voltages against ground.
        Returns the current and its slopes against the drain and gate voltages. */
    inline void fet (const Fet& f, double vg, double vd, double& id, double& dVd, double& dVg, double* hint = nullptr)
    {
        double i = hint != nullptr ? *hint : 0.0, a = 0, b = 0;
        for (int it = 0; it < 40; ++it)
        {
            double c; channel (f, vg - i * f.rs, vd - i * f.rs, c, a, b);
            const double g = i - c, dg = 1.0 + f.rs * (a + b), step = g / dg; i -= step; if (std::abs (step) < 1.0e-15 + 1.0e-12 * std::abs (i)) break;
        }
        { double c; channel (f, vg - i * f.rs, vd - i * f.rs, c, a, b); }
        const double den = 1.0 + f.rs * (a + b); id = i; dVd = a / den; dVg = b / den; if (hint != nullptr) *hint = i;
    }

    struct Cell { double r = 27000.0; };       // the resistor the signal comes through

    /** The gain cell for one control voltage: volts in, volts at the node. The gate sits at half the control plus half the node. */
    class GainCell
    {
    public:
        void prepare (const Fet& fetIn = {}, const Cell& cellIn = {}) { f = fetIn; c = cellIn; v = 0; cur = 0; }
        double process (double vin, double control)
        {
            double x = v; iterations = 0;
            for (int it = 0; it < 40; ++it)
            {
                ++iterations; double id, a, b; fet (f, 0.5 * (control + x), x, id, a, b, &cur);
                const double g = (x - vin) / c.r + id, dg = 1.0 / c.r + a + 0.5 * b, step = g / dg; x -= step; if (std::abs (step) < 1.0e-12) break;
            }
            v = x; return x;
        }
        /** Small-signal level through the cell at this control voltage (1 = nothing taken off). */
        double gainAt (double control) const
        {
            double id, a, b; fet (f, 0.5 * control, 0.0, id, a, b); const double gds = a + 0.5 * b; return 1.0 / (1.0 + c.r * gds);
        }
        int lastIterations() const { return iterations; }
    private:
        Fet f; Cell c; double v = 0, cur = 0; int iterations = 0;
    };

    /** The sidechain's settings. Levels are against `reference` volts at the node (a full-scale signal at Input 0 dB). */
    struct Sidechain { double reference = 0.1, gain = 4.0, threshold = 1.0, depth = 0.05, attack = 0.0002, release = 0.3, rest = 0.02; };      // depth: volts at the gate per volt on the capacitor (the ratio control); rest: how far below pinch-off the gate idles

    /** The compressor: gain cell and sidechain together. prepare() with the rate it runs at; process(): volts in, volts at the node. */
    class Compressor
    {
    public:
        void prepare (double sampleRate, const Fet& fetIn = {}, const Cell& cellIn = {}, const Sidechain& sc = {}) { f = fetIn; s = sc; T = 1.0 / sampleRate; cell.prepare (fetIn, cellIn); x = 0; last = 0; }
        void reset() { cell.prepare (f, {}); x = 0; last = 0; }
        void set (const Sidechain& sc) { s = sc; }
        /** The control voltage at the gate's feed for a capacitor voltage x: idling just below pinch-off, towards zero as it compresses. */
        double controlFor (double cap) const { return std::min (2.0 * (f.vto - s.rest) + 2.0 * s.depth * cap, -0.1); }       // the gate sees half of this (plus half the node)
        double capacitor() const { return x; }
        /** Level through the cell right now for small signals, in dB (0: not compressing). */
        double reductionDb() const { return 20.0 * std::log10 (cell.gainAt (controlFor (x))); }

        double process (double vin)
        {
            // the capacitor first, from the node's last value (the loop has one sample of delay, as any feedback loop computed in steps must; the oversampling keeps it short)
            const double over = std::max (s.gain * std::abs (last) / s.reference - s.threshold - x, 0.0);
            x += T * (over / s.attack - x / s.release); if (x < 0.0) x = 0.0;
            last = cell.process (vin, controlFor (x)); return last;
        }
    private:
        Fet f; Sidechain s; GainCell cell; double T = 1.0 / 192000.0, x = 0, last = 0;
    };
    /** The measured ratios (over the first 12 dB above the threshold) and the sidechain depth that gives each. */
    inline double depthForRatio (int index) { static const double d[4] = { 0.0848, 0.2083, 0.3326, 0.5762 }; return d[std::clamp (index, 0, 3)]; }

    /** Two gain cells under one sidechain, for stereo: the capacitor listens to whichever side is louder, so both are turned
        down alike and the picture between the speakers does not move. Call left() for the left sample, then right() for the
        right sample, with the same step index k inside an oversampling period (0 .. steps - 1). With one channel, call left() alone. */
    class StereoCompressor
    {
    public:
        static constexpr int maxSteps = 16;
        void prepare (double sampleRate, const Fet& left, const Fet& right, const Cell& cellIn = {}, const Sidechain& sc = {})
        {
            fl = left; s = sc; T = 1.0 / sampleRate; cell[0].prepare (left, cellIn); cell[1].prepare (right, cellIn); x = lastL = 0; for (auto& v : other) v = 0; for (auto& v : control) v = controlFor (0);
        }
        void set (const Sidechain& sc) { s = sc; }
        double controlFor (double cap) const { return std::min (2.0 * (fl.vto - s.rest) + 2.0 * s.depth * cap, -0.1); }
        double capacitor() const { return x; }
        double reductionDb() const { return 20.0 * std::log10 (cell[0].gainAt (controlFor (x))); }
        double left (double vin, int k)
        {
            const double heard = std::max (std::abs (lastL), std::abs (other[k]));       // the right side as it was one period ago (a few microseconds)
            const double over = std::max (s.gain * heard / s.reference - s.threshold - x, 0.0);
            x += T * (over / s.attack - x / s.release); if (x < 0.0) x = 0.0;
            control[k] = controlFor (x); lastL = cell[0].process (vin, control[k]); return lastL;
        }
        double right (double vin, int k) { other[k] = cell[1].process (vin, control[k]); return other[k]; }
        /** With one channel only: forget what the other side last did. */
        void clearRight() { for (auto& v : other) v = 0; }
    private:
        Fet fl; Sidechain s; GainCell cell[2]; double T = 1.0 / 192000.0, x = 0, lastL = 0, other[maxSteps] = {}, control[maxSteps] = {};
    };
}
