#pragma once
// Spindle Delay - DSP core (1.14 "Studio Line", phase I). No UI, no te:: types: tests/spindle_test.cpp measures it.
//
// A stereo delay with four characters and everything in the loop:
//   input -> (ducking key) -> write heads -> two delay lines (left / right time, free in ms or synced to the tempo)
//   read with cubic interpolation, the read position gliding (TAPE: pitch bends like a real machine) or
//   cross-fading (DIGITAL: a clean jump) when the time changes
//   -> in the feedback loop: high-pass and low-pass (12 dB/oct, so every repeat gets thinner / darker), drive (a soft
//   saturator that keeps runaway feedback musical), wow and flutter (two slow / fast modulators of the read position),
//   diffusion (two all-passes that smear each repeat into a wash)
//   -> routing: Normal (L->L, R->R), Ping-Pong (repeats alternate sides), Cross (each side feeds the other by an amount)
//   -> width, ducking (the echoes step back while you play), freeze (the loop holds, input muted), mix, output.
// Characters set defaults inside the loop: Digital (clean), Tape (wow, flutter, gentle drive and top roll-off),
// Analog (darker, more drive, a little chorus), Diffuse (diffusion high: a reverberant echo).

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

namespace SqSpindle
{
    constexpr double pi = 3.14159265358979323846;
    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (1e-12, g)); }

    enum Character { digital = 0, tape, analog, diffuse };
    enum Routing { normal = 0, pingPong, cross };
    inline const char* characterName (int c) { static const char* n[4] = { "Digital", "Tape", "Analog", "Diffuse" }; return n[juce::jlimit (0, 3, c)]; }
    inline const char* routingName (int r)   { static const char* n[3] = { "Normal", "Ping-Pong", "Cross" }; return n[juce::jlimit (0, 2, r)]; }

    struct Params
    {
        int character = tape, routing = normal;
        double timeLMs = 375, timeRMs = 375;       // already resolved from sync by the plug-in
        double feedback = 0.45;                    // 0..1.1 (above 1 the drive keeps it bounded)
        double crossFeed = 0.3;                    // Cross routing amount 0..1
        double lowCutHz = 80, highCutHz = 9000;    // in the loop
        double drive = 0.2;                        // 0..1
        double wow = -1, flutter = -1, diffusion = -1;   // 0..1; -1 = the character's own
        double width = 1.0, duckDb = 0, duckReleaseMs = 300, mix = 0.3, outputDb = 0;
        bool freeze = false;
    };

    struct CharDefaults { double wow, flutter, diffusion, driveAdd, topHz; bool glide; };
    inline CharDefaults defaultsFor (int c)
    {
        switch (c)
        {
            case tape:    return { 0.35, 0.25, 0.0, 0.10, 12000, true };
            case analog:  return { 0.15, 0.05, 0.1, 0.25, 6000, true };
            case diffuse: return { 0.10, 0.05, 0.8, 0.00, 14000, false };
            default:      return { 0.0, 0.0, 0.0, 0.0, 0, false };
        }
    }

    struct Line
    {
        std::vector<float> b; int mask = 0, w = 0;
        void prepare (int n) { int s = 1; while (s < n + 8) s <<= 1; b.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        inline float read (double d) const
        {
            const double pos = (double) w - d; const double fl = std::floor (pos); const int i = (int) fl; const float f = (float) (pos - fl);
            const float y0 = b[(size_t) ((i - 1) & mask)], y1 = b[(size_t) (i & mask)], y2 = b[(size_t) ((i + 1) & mask)], y3 = b[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
        inline void write (float x) { b[(size_t) (w & mask)] = x; ++w; }
    };
    struct Svf
    {
        double ic1 = 0, ic2 = 0, g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0;
        void set (double fs, double hz, double q = 0.7071) { g = std::tan (pi * juce::jlimit (10.0, fs * 0.49, hz) / fs); k = 1.0 / q; a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
        inline void tick (double v0, double& lp, double& hp) { const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3; ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; hp = v0 - k * v1 - v2; }
        void reset() { ic1 = ic2 = 0; }
    };
    struct AllPass
    {
        std::vector<float> b; int len = 1, idx = 0;
        void prepare (int n) { len = std::max (1, n); b.assign ((size_t) len, 0.0f); idx = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        inline float process (float x, float g) { const float z = b[(size_t) idx]; const float w = x - g * z; b[(size_t) idx] = w; if (++idx >= len) idx = 0; return z + g * w; }
    };

    /** The loop's saturator: unity for small signals, bounded at 1.5 - drive. */
    inline double sat (double x, double drive)
    {
        if (drive <= 0.0005) return x;
        const double c = 1.5 - drive;   // the ceiling falls from 1.5 to 0.5 as drive rises; small signals pass at unity
        return c * std::tanh (x / c);
    }

    class Engine
    {
    public:
        Params p;
        float meterInDb = -100, meterWetDb = -100, meterDuckDb = 0, meterTimeLMs = 0, meterTimeRMs = 0;
        static constexpr double maxSeconds = 4.0;

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            for (auto& l : line) l.prepare ((int) std::ceil ((maxSeconds + 0.05) * fs));
            const double apMs[2][2] = { { 7.3, 11.9 }, { 8.1, 13.7 } };
            for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) ap[(size_t) c][(size_t) k].prepare ((int) std::lround (apMs[c][k] * 0.001 * fs));
            lastLow = lastHigh = -1; reset();
        }
        void reset()
        {
            for (auto& l : line) l.clear();
            for (auto& a : ap) for (auto& x : a) x.clear();
            for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); for (auto& f : top) f.reset();
            for (int c = 0; c < 2; ++c) { cur[c] = target (c); old[c] = cur[c]; fade[c] = 1; fb[c] = 0; }
            wowPh = 0; flutPh = 0.37; duckEnv = 0; frz = p.freeze ? 1 : 0;
        }

        void process (float* const* ch, int nch, int n)
        {
            const auto cd = defaultsFor (p.character);
            const double wow = p.wow >= 0 ? p.wow : cd.wow, flutter = p.flutter >= 0 ? p.flutter : cd.flutter, diff = p.diffusion >= 0 ? p.diffusion : cd.diffusion;
            const double drive = juce::jlimit (0.0, 1.0, p.drive + cd.driveAdd);
            if (p.lowCutHz != lastLow) { for (auto& f : hp) f.set (fs, std::max (10.0, p.lowCutHz)); lastLow = p.lowCutHz; }
            if (p.highCutHz != lastHigh || cd.topHz != lastTop)
            { for (auto& f : lp) f.set (fs, std::min (20000.0, p.highCutHz)); for (auto& f : top) f.set (fs, cd.topHz > 0 ? cd.topHz : 20000.0, 0.6); lastHigh = p.highCutHz; lastTop = cd.topHz; }
            const bool hpOn = p.lowCutHz > 10.5, lpOn = p.highCutHz < 19999, topOn = cd.topHz > 0;
            const double fbG = juce::jlimit (0.0, 1.1, p.feedback), xf = juce::jlimit (0.0, 1.0, p.crossFeed);
            const float mix = (float) juce::jlimit (0.0, 1.0, p.mix), outG = (float) dbToGain (p.outputDb);
            const double aGlide = 1.0 / (0.12 * fs), fadeStep = 1.0 / (0.03 * fs);
            const double wowDepth = wow * 0.004 * fs, flutDepth = flutter * 0.00035 * fs;   // samples: up to 4 ms and 0.35 ms
            const double wowInc = 0.55 / fs, flutInc = 6.3 / fs;
            const double duckRel = 1.0 - std::exp (-1.0 / (std::max (10.0, p.duckReleaseMs) * 0.001 * fs)), duckAtt = 1.0 - std::exp (-1.0 / (0.005 * fs));
            const double frzA = 1.0 / (0.02 * fs);
            float pkIn = 0, pkWet = 0; double minDuck = 0;
            const bool stereo = nch > 1;
            for (int s = 0; s < n; ++s)
            {
                const float dl = ch[0][s], dr = stereo ? ch[1][s] : dl;
                pkIn = std::max (pkIn, std::max (std::abs (dl), std::abs (dr)));
                frz += ((p.freeze ? 1.0 : 0.0) - frz) * frzA;
                // delay times: glide (tape / analog) or cross-fade (digital / diffuse)
                for (int c = 0; c < 2; ++c)
                {
                    const double t = target (c);
                    if (cd.glide) cur[c] += juce::jlimit (-0.5, 0.5, (t - cur[c]) * aGlide * 8.0);   // pitch excursion capped at +-50 %
                    else if (std::abs (t - cur[c]) > 0.5 && fade[c] >= 1.0) { old[c] = cur[c]; cur[c] = t; fade[c] = 0; }
                    if (fade[c] < 1.0) fade[c] = std::min (1.0, fade[c] + fadeStep);
                }
                wowPh += wowInc; if (wowPh >= 1) wowPh -= 1; flutPh += flutInc; if (flutPh >= 1) flutPh -= 1;
                const double mod = wowDepth * (1.0 + std::sin (2 * pi * wowPh)) + flutDepth * (1.0 + std::sin (2 * pi * flutPh));
                double echo[2];
                for (int c = 0; c < 2; ++c)
                {
                    const double m = c == 0 ? mod : mod * 0.93;   // the right head wobbles slightly differently
                    double y = line[(size_t) c].read (cur[c] + m);
                    if (fade[c] < 1.0) { const double yo = line[(size_t) c].read (old[c] + m); y = yo + (y - yo) * (0.5 - 0.5 * std::cos (pi * fade[c])); }
                    echo[c] = y;
                }
                // the loop: filters, character top, drive, diffusion
                double loop[2];
                for (int c = 0; c < 2; ++c)
                {
                    double v = echo[c], lo, hi;
                    if (hpOn) { hp[(size_t) c].tick (v, lo, hi); v = hi; }
                    if (lpOn) { lp[(size_t) c].tick (v, lo, hi); v = lo; }
                    if (topOn) { top[(size_t) c].tick (v, lo, hi); v = lo; }
                    v = sat (v, drive);
                    if (diff > 0.001) { const float g = (float) (0.65 * diff); v = ap[(size_t) c][1].process (ap[(size_t) c][0].process ((float) v, g), g); }
                    loop[c] = v;
                }
                const double inG = 1.0 - frz, g = frz > 0.999 ? 1.0 : fbG + (1.0 - fbG) * frz;   // frozen: lossless loop, input muted
                double wl, wr;
                switch (p.routing)
                {
                    case pingPong: wl = dl * inG * 0.5 + dr * inG * 0.5 + loop[1] * g; wr = loop[0] * g; break;   // mono in on the left; repeats alternate
                    case cross:    wl = dl * inG + (loop[0] * (1 - xf) + loop[1] * xf) * g; wr = dr * inG + (loop[1] * (1 - xf) + loop[0] * xf) * g; break;
                    default:       wl = dl * inG + loop[0] * g; wr = dr * inG + loop[1] * g;
                }
                line[0].write ((float) wl); line[1].write ((float) wr);
                // wet out: width, ducking
                double el = echo[0], er = echo[1];
                const double mm = 0.5 * (el + er), sd = 0.5 * (el - er) * p.width; el = mm + sd; er = mm - sd;
                if (p.duckDb > 0.01)
                {
                    const double e = std::max (std::abs (dl), std::abs (dr));
                    duckEnv += (e - duckEnv) * (e > duckEnv ? duckAtt : duckRel);
                    const double over = juce::jlimit (0.0, 1.0, (gainToDb (duckEnv) + 40.0) / 20.0);   // -40 dB .. -20 dB of input: none .. full
                    const double gd = -p.duckDb * over; minDuck = std::min (minDuck, gd);
                    const double gg = dbToGain (gd); el *= gg; er *= gg;
                }
                pkWet = std::max (pkWet, (float) std::max (std::abs (el), std::abs (er)));
                ch[0][s] = (float) (dl * (1 - mix) + el * mix) * outG;
                if (stereo) ch[1][s] = (float) (dr * (1 - mix) + er * mix) * outG;
            }
            meterInDb = (float) gainToDb (pkIn); meterWetDb = (float) gainToDb (pkWet); meterDuckDb = (float) minDuck;
            meterTimeLMs = (float) (cur[0] / fs * 1000.0); meterTimeRMs = (float) (cur[1] / fs * 1000.0);
        }

    private:
        double fs = 48000;
        std::array<Line, 2> line; std::array<std::array<AllPass, 2>, 2> ap;
        std::array<Svf, 2> hp, lp, top;
        double cur[2] {}, old[2] {}, fade[2] { 1, 1 }, fb[2] {};
        double wowPh = 0, flutPh = 0, duckEnv = 0, frz = 0, lastLow = -1, lastHigh = -1, lastTop = -1;
        double target (int c) const { return juce::jlimit (4.0, maxSeconds * fs, (c == 0 ? p.timeLMs : p.timeRMs) * 0.001 * fs); }
    };
}
