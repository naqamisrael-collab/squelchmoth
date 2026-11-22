#pragma once
// SqDrive: an overdrive's clipping stage - an amplifier with a pair of diodes in its feedback - solved as a circuit.
// Pure C++ (no JUCE).
//
// The signal goes to the amplifier's + input. From the - input a resistor and a capacitor in series run to ground;
// from the output back to the - input run the Drive resistance, a small capacitor and the two diodes, side by side.
// The amplifier holds its two inputs together, so the current through the ground leg is set by the input alone, and
// all of it must come round through the feedback parts. Small signals see a plain amplifier (gain 1 + Rf / Rg above
// the ground leg's corner, 1 below it - bass passes clean). Once the voltage across the feedback reaches the diodes'
// knee they take the current instead and that voltage stops growing: the output is the input itself plus what the
// diodes allow. That is why this kind of clipper is soft - the clean signal is always in there - where a pair of
// diodes straight across the signal (SqClip.h) is hard.
#include "SqClip.h"

namespace SqDrive
{
    /** The parts. rg, cg: the ground leg (their corner is where the gain starts, 720 Hz with these values). rf: the fixed
        feedback resistor plus the Drive control. cf: the capacitor beside the diodes. */
    struct Circuit { double rg = 4700.0, cg = 47.0e-9, rf = 51000.0, cf = 51.0e-12; bool lopsided = false; };
    constexpr double fixedOhms = 51000.0, driveOhms = 500000.0;

    class Stage
    {
    public:
        void prepare (double sampleRate, const SqClip::Diode& diodeIn = {}, const Circuit& circuit = {}) { d = diodeIn; c = circuit; T = 1.0 / sampleRate; reset(); }
        void setParts (const SqClip::Diode& diodeIn, const Circuit& circuit) { d = diodeIn; c = circuit; }
        void reset() { vc = iLeg = vf = vfBefore = ic = 0.0; up = down = 0.0; iterations = 0; }
        int lastIterations() const { return iterations; }
        /** The voltage across the feedback parts after the last sample (what the diodes allowed). */
        double feedbackVolts() const { return vf; }

        /** Volts in, volts out. */
        double process (double vin)
        {
            // the ground leg: vin = i rg + vc, cg dvc/dt = i (trapezoidal rule) - the current follows from the input alone
            const double a = 0.5 * T / c.cg, i = (vin - vc - a * iLeg) / (c.rg + a); vc += a * (i + iLeg); iLeg = i;
            // the feedback node: cf dvf/dt = i - vf / rf - diodes (vf)
            const double g = 2.0 * c.cf / T, gf = 1.0 / c.rf; iterations = 0;
            double x = std::abs (vf - vfBefore) < 0.05 ? 2.0 * vf - vfBefore : vf;
            for (int it = 0; it < 60; ++it)
            {
                ++iterations; double id, did; SqClip::pair (d, c.lopsided, x, id, did, &up, &down);
                const double f = g * (x - vf) - ic - (i - x * gf - id), df = g + gf + did;
                double step = f / df; const double limit = 0.25; if (step > limit) step = limit; else if (step < -limit) step = -limit;
                x -= step; if (std::abs (step) < 1.0e-10) break;
            }
            ic = g * (x - vf) - ic; vfBefore = vf; vf = x; return vin + x;
        }
        /** Gain for small signals at a frequency (the plain amplifier, before the diodes do anything): |1 + Zf / Zg|. */
        static double smallGain (const Circuit& c, double hz)
        {
            const double w = 2.0 * 3.14159265358979323846 * hz, fr = c.rf / (1.0 + w * w * c.rf * c.rf * c.cf * c.cf), fi = -w * c.rf * c.rf * c.cf / (1.0 + w * w * c.rf * c.rf * c.cf * c.cf);
            const double gr = c.rg, gi = -1.0 / (w * c.cg), den = gr * gr + gi * gi, re = 1.0 + (fr * gr + fi * gi) / den, im = (fi * gr - fr * gi) / den; return std::sqrt (re * re + im * im);
        }
    private:
        SqClip::Diode d; Circuit c; double T = 1.0 / 192000.0, vc = 0, iLeg = 0, vf = 0, vfBefore = 0, ic = 0, up = 0, down = 0; int iterations = 0;
    };
}
