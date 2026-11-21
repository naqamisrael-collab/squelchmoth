#pragma once
// Squelchmoth 1.42 - Voltage Clip: a diode clipper solved as a circuit. Pure: no engine, no UI.
// The circuit: the signal through a resistor to the output, a pair of diodes from there to ground (one each way; or two
// in series one way for a lopsided clip), and a capacitor beside them. Each diode is the junction law with its series
// resistance. Nothing is drawn as a curve: the knee, how hard it limits and how the top end rounds off as it clips all
// come from the diode's numbers and the capacitor. Reference: the same circuit in a circuit simulator
// (tests/reference/clip/make_reference.py); tests/clip_test.cpp compares harmonics.
#include <algorithm>
#include <cmath>

namespace SqClip
{
    constexpr double thermalVolts = 0.0258649;      // kT/q at 27 C, the simulator's own temperature

    /** A diode by its published model numbers: saturation current, emission coefficient, series resistance. */
    struct Diode { double is = 4.352e-9, n = 1.906, rs = 0.6458; };
    inline Diode silicon()  { return {}; }                               // a small-signal silicon diode (its maker's own model)
    inline Diode schottky() { return { 315.0e-9, 2.03, 2.8 }; }          // a low-threshold diode with a soft knee
    inline Diode sharp()    { return { 680.0e-12, 1.003, 12.0 }; }       // a low-threshold diode with a sharp knee

    /** Current through one diode for a forward voltage v (negative v: the small leakage), and its slope. */
    inline void diode (const Diode& d, double v, double& i, double& di, double* hint = nullptr)
    {
        const double nvt = d.n * thermalVolts;
        if (v <= 0.0) { if (v < -30.0 * nvt) { i = -d.is; di = 1.0e-15; return; } const double e = std::exp (v / nvt); i = d.is * (e - 1.0); di = d.is * e / nvt + 1.0e-15; return; }      // far below zero only the leakage is left
        // junction and its series resistance together: i rs + nvt ln (1 + i / is) = v. Start above the answer, where Newton cannot run away
        // (the function is concave there) - or from the last answer for this diode, raised to the safe side, when the caller keeps one.
        double x = std::min (v / d.rs, v < 40.0 * nvt ? d.is * std::expm1 (v / nvt) : v / d.rs);
        if (hint != nullptr && *hint > 0.0)
        {
            const double g = *hint * d.rs + nvt * std::log1p (*hint / d.is) - v;                 // how far the old answer is from this voltage
            const double start = g >= 0.0 ? *hint : *hint - g / (d.rs + nvt / (*hint + d.is));   // below the answer: one tangent step lands above it
            if (start > 0.0 && start < x) x = start;
        }
        for (int k = 0; k < 30; ++k)
        {
            const double g = x * d.rs + nvt * std::log1p (x / d.is) - v, dg = d.rs + nvt / (x + d.is), step = g / dg;
            x = std::max (x - step, 0.0); if (std::abs (step) < 1.0e-16 + 1.0e-11 * std::abs (x)) break;
        }
        i = x; di = 1.0 / (d.rs + nvt / (x + d.is)); if (hint != nullptr) *hint = x;
    }

    struct Circuit { double r = 2200.0, c = 10.0e-9; bool lopsided = false; };      // lopsided: two diodes in series for the positive side

    /** Current from the output node to ground through the diodes at voltage v, and its slope. */
    inline void pair (const Diode& d, bool lopsided, double v, double& i, double& di, double* hintUp = nullptr, double* hintDown = nullptr)
    {
        double a, da, b, db;
        if (lopsided) { diode (d, 0.5 * v, a, da, hintUp); da *= 0.5; } else diode (d, v, a, da, hintUp);       // two alike in series share the voltage
        diode (d, -v, b, db, hintDown); i = a - b; di = da + db;
    }

    /** The voltage at which the pair passes this current (the knee, for scaling levels): default 100 microamps. */
    inline double kneeVolts (const Diode& d, double current = 1.0e-4) { return current * d.rs + d.n * thermalVolts * std::log1p (current / d.is); }

    /** The clipper: prepare() with the rate it runs at (oversample outside), then process(): volts in, volts out. */
    class Clipper
    {
    public:
        void prepare (double sampleRate, const Diode& diodeIn = {}, const Circuit& circuit = {}) { d = diodeIn; c = circuit; T = 1.0 / sampleRate; reset(); }
        void reset() { v = vBefore = 0.0; ic = 0.0; up = down = 0.0; iterations = 0; }
        void setParts (const Diode& diodeIn, const Circuit& circuit) { d = diodeIn; c = circuit; }
        int lastIterations() const { return iterations; }
        /** Where the capacitor and resistor put the top-end corner for quiet signals, in Hz. */
        static double corner (const Circuit& c) { return 1.0 / (2.0 * 3.14159265358979323846 * c.r * c.c); }

        double process (double vin)
        {
            // one node: (vin - v) / r = diodes (v) + c dv/dt, capacitor by the trapezoidal rule
            const double g = 2.0 * c.c / T, ieq = -(g * v + ic); iterations = 0;
            double x = std::abs (v - vBefore) < 0.05 ? 2.0 * v - vBefore : v;                 // start from where the last two answers point, unless it is moving fast
            for (int it = 0; it < 60; ++it)
            {
                ++iterations; double id, did; pair (d, c.lopsided, x, id, did, &up, &down);
                const double f = (x - vin) / c.r + id + g * x + ieq, df = 1.0 / c.r + did + g;
                double step = f / df; const double limit = 0.25; if (step > limit) step = limit; else if (step < -limit) step = -limit;      // the diode law is steep: small strides
                x -= step; if (std::abs (step) < 1.0e-9) break;
            }
            ic = g * x + ieq; vBefore = v; v = x; return x;
        }
    private:
        Diode d; Circuit c; double T = 1.0 / 192000.0, v = 0, vBefore = 0, ic = 0, up = 0, down = 0; int iterations = 0;
    };
}
