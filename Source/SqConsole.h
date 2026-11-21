#pragma once
// Squelchmoth 1.41 - Voltage Console: what a console adds besides its equalisers. Pure: no engine, no UI.
//  - the transformer on the way in or out: SqTube::LineTransformer (iron with saturation and hysteresis, checked against a
//    circuit simulator in tests/console_test.cpp)
//  - crosstalk between the two rails of a stereo bus: a little of each side leaks into the other through stray
//    capacitance, so it rises with frequency (6 dB an octave) until the wiring's own resistance stops it
#include "SqTube.h"
#include <complex>

namespace SqConsole
{
    constexpr double pi = 3.14159265358979323846;

    /** Left into right and right into left: level `db` at 1 kHz (for example -80), rising 6 dB an octave, levelling off at 40 kHz. */
    class Crosstalk
    {
    public:
        void prepare (double sampleRate) { fs = sampleRate; set (usedDb); reset(); }
        void reset() { xl = xr = yl = yr = 0.0; }
        void set (double db)
        {
            usedDb = db; on = db > -119.0; if (! on) { b0 = 0; a1 = 0; return; }
            const double k = 2.0 * fs, w2 = 2.0 * pi * 40000.0; a1 = (w2 - k) / (w2 + k); b0 = 1.0;
            b0 = std::pow (10.0, db / 20.0) / level (1000.0);        // exactly the asked level at 1 kHz at this rate
        }
        /** Level of the leak against the side it comes from, at hz. */
        double level (double hz) const
        {
            if (! on && b0 == 0.0) return 0.0; const std::complex<double> z = std::polar (1.0, -2.0 * pi * hz / fs); return std::abs (b0 * (1.0 - z) / (1.0 + a1 * z));
        }
        void process (double& l, double& r)
        {
            if (! on) return;
            const double ll = b0 * (l - xl) - a1 * yl, lr = b0 * (r - xr) - a1 * yr; xl = l; xr = r; yl = ll; yr = lr;
            l += lr; r += ll;
        }
    private:
        double fs = 48000.0, b0 = 0, a1 = 0, xl = 0, xr = 0, yl = 0, yr = 0, usedDb = -120.0; bool on = false;
    };

    /** The iron for a size setting: 0 % is a small transformer (a quarter of the reference core), 100 % a large one (four times). */
    inline SqTube::Core ironFor (double percent) { return SqTube::lineCore (0.25 * std::pow (16.0, std::clamp (percent, 0.0, 100.0) / 100.0)); }
}
