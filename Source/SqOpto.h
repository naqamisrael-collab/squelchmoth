#pragma once
// SqOpto: an optical gain cell - a lamp panel shining on a light-dependent resistor - and the levelling
// compressor built round it. Pure C++ (no JUCE), so the standalone test and the plug-in run the same code.
//
// The resistor is an ordinary (ohmic) resistor whose value depends on light: resistance falls as light to the
// power -gamma (gamma 0.9 is the typical figure on the cadmium-sulphide cell data sheets read for this work).
// It hangs from the signal path to ground after a series resistor, so more light means less signal.
// What makes the sound is the cell's sluggishness: it answers light quickly, lets go quickly at first, and
// then keeps a memory of how long and how hard it was lit, which lets go over seconds.
#include <algorithm>
#include <cmath>

namespace SqOpto
{
    struct Cell
    {
        double gamma    = 0.9;      // resistance against light (data sheets: 0.9 typical)
        double dark     = 0.005;    // series resistance over dark resistance: the small loss when nothing is lit
        double law      = 40.0 / 9.0; // how steeply the panel's light rises with the voltage on it (ours: the ratio heads for 5 : 1)
        double driver   = 0.0001;   // seconds: smoothing in the panel's driver (ours; it keeps the light's flicker from folding back as aliasing)
        double rise     = 0.450;    // seconds: the cell answering light (ours: set so the attack measures about 10 ms at 8 dB of reduction)
        double fall     = 0.035;    // seconds: the first, fast part of letting go (ours: set so half the release takes about 60 ms)
        double memory   = 0.25;     // how much the slow memory counts against the fast response
        double memoryUp = 0.7;      // seconds: how slowly the memory builds while lit
        double memoryDown = 5.0;    // seconds: how slowly it fades in the dark
        double sense    = 1.0;      // this cell's sensitivity against the nominal one (part tolerance)
    };

    /** The divider's gain for a given amount of effective light (1 = the light that halves the signal, less the dark loss). */
    inline double gainFor (const Cell& c, double light)
    {
        return 1.0 / (1.0 + c.dark + c.sense * std::pow (std::max (light, 0.0), c.gamma));
    }

    /** Two channels under one panel. The sidechain listens to the louder side of what comes out (feedback).
        In Limit the panel's driver is biased off, so nothing lights until the signal passes a threshold and then
        the light comes up steeply: a firm ceiling instead of a gentle squeeze. */
    struct Compressor
    {
        void prepare (double sampleRate, const Cell& left, const Cell& right)
        {
            fs = sampleRate; c[0] = left; c[1] = right; reset();
            aDriver = 1.0 - std::exp (-1.0 / (left.driver * fs)); aRise = 1.0 - std::exp (-1.0 / (left.rise * fs)); aFall = 1.0 - std::exp (-1.0 / (left.fall * fs));
            aUp = 1.0 - std::exp (-1.0 / (left.memoryUp * fs)); aDown = 1.0 - std::exp (-1.0 / (left.memoryDown * fs));
        }
        void reset() { lit = 0.0; fast = 0.0; slow = 0.0; for (int i = 0; i < 16; ++i) { seen[i] = 0.0; other[i] = 0.0; } }
        /** drive: sidechain gain (0 = no compression). bias: how far the panel's driver is held off (0 = Compress).
            Either way the panel reaches the light that halves the signal when the output is 1 / drive. */
        void set (double driveGain, double driverBias) { drive = driveGain; bias = driverBias; }

        double light() const { return fast + c[0].memory * slow; }
        double reductionDb() const { return 20.0 * std::log10 (gainFor (c[0], light()) * (1.0 + c[0].dark)); }
        double fastState() const { return fast; }
        double slowState() const { return slow; }

        /** One step of the panel and the cell for a given level heard at the output. */
        void advance (double heard)
        {
            const double volts = drive * heard * (1.0 + bias) - bias;
            const double lamp = volts > 0.0 ? std::pow (volts, c[0].law) : 0.0;
            lit += (lamp - lit) * aDriver;
            fast += (lit - fast) * (lit > fast ? aRise : aFall);
            slow += (fast - slow) * (fast > slow ? aUp : aDown);
        }
        void process (double& l, double& r)
        {
            const double e = light(), yl = l * gainFor (c[0], e), yr = r * gainFor (c[1], e);
            advance (std::max (std::abs (yl), std::abs (yr))); l = yl; r = yr;
        }
        /** The same, a channel at a time, for a host that hands over the left's inner samples and then the right's
            (k counts the inner samples of one outer sample, up to 16). The left moves the panel, hearing the right as it was
            one outer sample ago (a few microseconds); the right then takes the light the left saw at the same instant. */
        double left (double v, int k)
        {
            const double e = light(); seen[k & 15] = e; const double y = v * gainFor (c[0], e);
            advance (std::max (std::abs (y), other[k & 15])); return y;
        }
        double right (double v, int k) { const double y = v * gainFor (c[1], seen[k & 15]); other[k & 15] = std::abs (y); return y; }
        void clearRight() { for (auto& o : other) o = 0.0; }
        double process (double v) { double other = 0.0; process (v, other); return v; }

        double fs = 48000.0, drive = 0.0, bias = 0.0, lit = 0.0, fast = 0.0, slow = 0.0, aDriver = 0, aRise = 0, aFall = 0, aUp = 0, aDown = 0;
        double seen[16] = {}, other[16] = {};
        Cell c[2];
    };

    /** Limit: how far the panel's driver is held off. */
    inline double limitBias() { return 8.0; }
}
