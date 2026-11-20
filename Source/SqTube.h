#pragma once
// Squelchmoth 1.37 - Voltage: a triode gain stage solved as a circuit. Pure: no engine, no UI.
// The circuit is the common-cathode stage: input capacitor, grid leak, grid stopper, the triode with its three
// capacitances and grid current, cathode resistor with or without a bypass capacitor, plate resistor to the supply,
// output capacitor into a load. Every sample the five node voltages are found by Newton's method on the full
// nonlinear network (capacitors by the trapezoidal rule), so bias shift, grid-current blocking and the Miller
// roll-off come out of the circuit, not out of a curve. The reference is the same circuit in a circuit simulator
// (tests/reference/triode); tests/tube_test.cpp compares harmonics and response against it.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace SqTube
{
    /** A triode by its published model parameters (plate-current law with exponent ex, amplification mu). */
    struct Tube { double mu = 100, ex = 1.4, kg1 = 1060, kp = 600, kvb = 300, rgi = 2000, ccg = 2.3e-12, cgp = 2.4e-12, ccp = 0.9e-12; };
    inline Tube highGain() { return {}; }                                                              // high-mu small-signal triode
    inline Tube mediumGain() { Tube t; t.mu = 21.5; t.ex = 1.3; t.kg1 = 1180; t.kp = 84; t.kvb = 300; t.ccg = 2.3e-12; t.cgp = 2.2e-12; t.ccp = 1.0e-12; return t; }   // medium-mu triode

    struct Circuit { double bplus = 300.0, rp = 100.0e3, rk = 1500.0, ck = 22.0e-6 /* 0: no bypass */, rgs = 10.0e3, rgl = 1.0e6, cin = 22.0e-9, cout = 100.0e-9, rl = 1.0e6; };

    /** Plate current and its slopes: E1 = Vpk/kp * ln(1 + exp(kp (1/mu + Vgk / sqrt(kvb + Vpk^2)))), Ip = 2 E1^ex / kg1 for E1 > 0. */
    inline void plate (const Tube& t, double vpk, double vgk, double& ip, double& dVpk, double& dVgk)
    {
        ip = dVpk = dVgk = 0.0;
        const double root = std::sqrt (t.kvb + vpk * vpk);
        const double x = t.kp * (1.0 / t.mu + vgk / root);
        const double soft = x > 40.0 ? x : std::log1p (std::exp (x));           // ln(1 + e^x)
        const double sig = x > 40.0 ? 1.0 : 1.0 / (1.0 + std::exp (-x));         // its slope
        const double e1 = vpk / t.kp * soft;
        if (! (e1 > 0.0)) return;
        const double p = std::pow (e1, t.ex);
        ip = 2.0 * p / t.kg1;
        const double dIdE = 2.0 * t.ex * p / (e1 * t.kg1);
        const double dxdVpk = -t.kp * vgk * vpk / (root * root * root), dxdVgk = t.kp / root;
        dVpk = dIdE * (soft / t.kp + vpk / t.kp * sig * dxdVpk);
        dVgk = dIdE * (vpk / t.kp * sig * dxdVgk);
    }

    /** Grid current and its slope: a junction (saturation current 1 nA, 25.852 mV) in series with rgi + 1 ohm. */
    inline void grid (const Tube& t, double vgk, double& ig, double& dIg)
    {
        constexpr double is = 1.0e-9, vt = 0.025852; const double r = t.rgi + 1.0;
        if (vgk <= 0.0) { const double e = std::exp (std::max (vgk, -2.0) / vt); ig = is * (e - 1.0); dIg = is * e / vt + 1.0e-12; return; }
        // solve i r + vt ln(1 + i / is) = vgk for i; start from above the answer, where Newton cannot run away
        double i = std::min (vgk / r, vgk < 1.0 ? is * std::expm1 (vgk / vt) : vgk / r);
        for (int k = 0; k < 12; ++k)
        {
            const double g = i * r + vt * std::log1p (i / is) - vgk, dg = r + vt / (i + is);
            const double step = g / dg; i = std::max (i - step, -0.5 * is);
            if (std::abs (step) < 1.0e-15 + 1.0e-12 * std::abs (i)) break;
        }
        ig = i; dIg = 1.0 / (r + vt / (i + is));
    }

    /** The stage. prepare() with the rate it will be run at (oversample outside), then process() one sample at a time: volts in, volts out. */
    class Stage
    {
    public:
        void prepare (double sampleRate, const Tube& tube = {}, const Circuit& circuit = {})
        {
            t = tube; c = circuit; T = 1.0 / sampleRate; reset();
        }

        /** Settles to the resting point: no signal, capacitors charged. */
        void reset()
        {
            // resting point: grid at ground, cathode and plate from the two-node balance
            double vk = 1.5, vp = c.bplus * 0.66, vg = 0.0, va = 0.0;
            for (int it = 0; it < 400; ++it)
            {
                // the grid's own small leakage through the grid leak lifts it about a millivolt
                const double vgWas = vg;
                { double ig, dg; grid (t, vg - vk, ig, dg); va = -ig * c.rgl; vg = -ig * (c.rgl + c.rgs); }
                double ip, a, b; plate (t, vp - vk, vg - vk, ip, a, b);
                const double f1 = vk / c.rk - ip, f2 = (vp - c.bplus) / c.rp + ip;
                // d ip / d vk = -a - b ; d ip / d vp = a
                const double j11 = 1.0 / c.rk + a + b, j12 = -a, j21 = -a - b, j22 = 1.0 / c.rp + a;
                const double det = j11 * j22 - j12 * j21; if (std::abs (det) < 1.0e-30) break;
                const double dk = (f1 * j22 - f2 * j12) / det, dp = (j11 * f2 - j21 * f1) / det;
                vk -= std::clamp (dk, -0.5, 0.5); vp -= std::clamp (dp, -20.0, 20.0);
                if (std::abs (dk) < 1.0e-13 && std::abs (dp) < 1.0e-11 && std::abs (vg - vgWas) < 1.0e-13) break;
            }
            x = { va, vg, vk, vp, 0.0 }; restK = vk; restP = vp;
            // the junction's own capacitance at the resting grid voltage (10 pF at zero volts)
            cgkJunction = 10.0e-12 / std::sqrt (1.0 + std::max (0.0, vk)); cgk = t.ccg + cgkJunction;
            caps[0] = { c.cin, va, 0.0 };               // in - A   (voltage: vA - vin)
            caps[1] = { cgk, vg - vk, 0.0 };            // G - K
            caps[2] = { t.cgp, vg - vp, 0.0 };          // G - P
            caps[3] = { t.ccp, vp - vk, 0.0 };          // P - K
            caps[4] = { c.ck, vk, 0.0 };                // K - ground
            caps[5] = { c.cout, vp, 0.0 };              // P - O
        }

        /** Changes components while running: the capacitors keep their charge and the stage settles as a real one would. */
        void setParts (const Tube& tube, const Circuit& circuit)
        {
            t = tube; const bool hadCk = c.ck > 0; c = circuit;
            caps[0].c = c.cin; caps[2].c = t.cgp; caps[3].c = t.ccp; caps[4].c = c.ck; caps[5].c = c.cout; caps[1].c = t.ccg + cgkJunction;
            if (! hadCk && c.ck > 0) { caps[4].v = x[2]; caps[4].i = 0.0; }
        }

        /** The stage's gain for small mid-band signals with these parts (negative: it inverts), from its own equations. */
        static double smallSignalGain (const Tube& t, const Circuit& c)
        {
            Stage rest; rest.t = t; rest.c = c; rest.T = 1.0 / 192000.0; rest.reset();
            const double vk0 = rest.restK, vp0 = rest.restP, rac = 1.0 / (1.0 / c.rp + 1.0 / c.rl);
            auto plateAt = [&] (double vg)
            {
                double vk = vk0, vp = vp0;
                for (int it = 0; it < 60; ++it)
                {
                    double ip, a, b; plate (t, vp - vk, vg - vk, ip, a, b);
                    if (c.ck > 0)
                    {
                        // cathode held by its capacitor; the plate sees its resistor and the load together
                        const double f = (vp - vp0) / rac + (ip - (c.bplus - vp0) / c.rp), d = 1.0 / rac + a; const double step = f / d; vp -= step; if (std::abs (step) < 1.0e-12) break;
                    }
                    else
                    {
                        const double f1 = vk / c.rk - ip, f2 = (vp - vp0) / rac + (ip - (c.bplus - vp0) / c.rp);
                        const double j11 = 1.0 / c.rk + a + b, j12 = -a, j21 = -a - b, j22 = 1.0 / rac + a, det = j11 * j22 - j12 * j21;
                        const double dk = (f1 * j22 - f2 * j12) / det, dp = (j11 * f2 - j21 * f1) / det; vk -= dk; vp -= dp; if (std::abs (dk) + std::abs (dp) < 1.0e-12) break;
                    }
                }
                return vp;
            };
            const double d = 1.0e-4; return (plateAt (d) - plateAt (-d)) / (2.0 * d);
        }

        double restingPlate() const   { return restP; }
        double restingCathode() const { return restK; }
        int lastIterations() const    { return iterations; }

        double process (double vin)
        {
            double g[6], ieq[6];
            for (int i = 0; i < 6; ++i) { g[i] = 2.0 * caps[i].c / T; ieq[i] = -(g[i] * caps[i].v + caps[i].i); }
            auto v = x;                                                         // start from the last answer
            double capV[6] = {}, capI[6] = {};
            iterations = 0;
            for (int it = 0; it < 50; ++it)
            {
                ++iterations;
                const double vA = v[0], vG = v[1], vK = v[2], vP = v[3], vO = v[4];
                double ip, dIpP, dIpG; plate (t, vP - vK, vG - vK, ip, dIpP, dIpG);
                double ig, dIg; grid (t, vG - vK, ig, dIg);
                capV[0] = vA - vin; capV[1] = vG - vK; capV[2] = vG - vP; capV[3] = vP - vK; capV[4] = vK; capV[5] = vP - vO;
                for (int i = 0; i < 6; ++i) capI[i] = g[i] * capV[i] + ieq[i];
                double F[5], J[5][5] = {};
                F[0] = capI[0] + vA / c.rgl + (vA - vG) / c.rgs;
                J[0][0] = g[0] + 1.0 / c.rgl + 1.0 / c.rgs; J[0][1] = -1.0 / c.rgs;
                F[1] = (vG - vA) / c.rgs + capI[1] + capI[2] + ig;
                J[1][0] = -1.0 / c.rgs; J[1][1] = 1.0 / c.rgs + g[1] + g[2] + dIg; J[1][2] = -g[1] - dIg; J[1][3] = -g[2];
                F[2] = vK / c.rk + capI[4] - capI[1] - ig - ip - capI[3];
                J[2][1] = -g[1] - dIg - dIpG; J[2][2] = 1.0 / c.rk + g[4] + g[1] + dIg + dIpP + dIpG + g[3]; J[2][3] = -dIpP - g[3];
                F[3] = (vP - c.bplus) / c.rp + ip + capI[3] - capI[2] + capI[5];
                J[3][1] = dIpG - g[2]; J[3][2] = -dIpP - dIpG - g[3]; J[3][3] = 1.0 / c.rp + dIpP + g[3] + g[2] + g[5]; J[3][4] = -g[5];
                F[4] = vO / c.rl - capI[5];
                J[4][3] = -g[5]; J[4][4] = 1.0 / c.rl + g[5];
                double d[5]; if (! solve5 (J, F, d)) break;
                // keep a step from throwing the grid or plate far past where the curves mean anything
                double scale = 1.0; const double big = std::max ({ std::abs (d[1] - d[2]) / 0.5, std::abs (d[3]) / 60.0, 1.0 }); scale = 1.0 / big;
                double moved = 0.0; for (int i = 0; i < 5; ++i) { v[(size_t) i] -= scale * d[i]; moved = std::max (moved, std::abs (scale * d[i])); }
                if (moved < 1.0e-10) break;
            }
            // accept: update the capacitors' memories from the answer
            capV[0] = v[0] - vin; capV[1] = v[1] - v[2]; capV[2] = v[1] - v[3]; capV[3] = v[3] - v[2]; capV[4] = v[2]; capV[5] = v[3] - v[4];
            for (int i = 0; i < 6; ++i) { caps[i].i = g[i] * capV[i] + ieq[i]; caps[i].v = capV[i]; }
            x = v;
            return v[4];
        }

    private:
        static bool solve5 (double a[5][5], const double* b, double* out)
        {
            double m[5][6];
            for (int i = 0; i < 5; ++i) { for (int j = 0; j < 5; ++j) m[i][j] = a[i][j]; m[i][5] = b[i]; }
            for (int col = 0; col < 5; ++col)
            {
                int piv = col; for (int r = col + 1; r < 5; ++r) if (std::abs (m[r][col]) > std::abs (m[piv][col])) piv = r;
                if (std::abs (m[piv][col]) < 1.0e-300) return false;
                if (piv != col) for (int j = 0; j < 6; ++j) std::swap (m[piv][j], m[col][j]);
                for (int r = col + 1; r < 5; ++r) { const double f = m[r][col] / m[col][col]; if (f != 0.0) for (int j = col; j < 6; ++j) m[r][j] -= f * m[col][j]; }
            }
            for (int i = 4; i >= 0; --i) { double s = m[i][5]; for (int j = i + 1; j < 5; ++j) s -= m[i][j] * out[j]; out[i] = s / m[i][i]; }
            return true;
        }

        struct Cap { double c = 0, v = 0, i = 0; };
        Tube t; Circuit c; double T = 1.0 / 192000.0, cgk = 0, cgkJunction = 0, restK = 0, restP = 0; int iterations = 0;
        std::array<double, 5> x { 0, 0, 0, 0, 0 }; Cap caps[6];
    };

    //============================================================================== oversampling
    /** Runs a process at `factor` times the rate: a windowed-sinc (Kaiser) filter up and the same down. Constant delay of
        latency() samples at the outer rate. */
    class Oversampler
    {
    public:
        void prepare (int factorIn, int tapsPerPhase = 96, double stopDb = 130.0)
        {
            factor = std::max (1, factorIn); per = tapsPerPhase; const int n = per * factor + 1;      // odd length: the delay is a whole number of outer samples
            h.assign ((size_t) n, 0.0);
            if (factor == 1) { h.assign (1, 1.0); per = 0; }
            else
            {
                const double beta = 0.1102 * (stopDb - 8.7), cutoff = 0.5 / factor;      // centred on the outer rate's fold: with 96 taps a phase the response is flat to 20 kHz at 44.1 kHz,
                                                                       // and what folds back lands only above that
                auto bessel = [] (double v) { double s = 1.0, term = 1.0; for (int k = 1; k < 60; ++k) { term *= (v / (2.0 * k)) * (v / (2.0 * k)); s += term; if (term < 1.0e-18 * s) break; } return s; };
                double sum = 0.0;
                for (int i = 0; i < n; ++i)
                {
                    const double m = i - (n - 1) / 2.0, r = 2.0 * i / (n - 1) - 1.0;
                    const double sinc = std::abs (m) < 1.0e-12 ? 2.0 * cutoff : std::sin (2.0 * 3.14159265358979323846 * cutoff * m) / (3.14159265358979323846 * m);
                    h[(size_t) i] = sinc * bessel (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel (beta); sum += h[(size_t) i];
                }
                for (auto& v : h) v /= sum;
            }
            upLen = per + 1; upHist.assign ((size_t) upLen, 0.0); downHist.assign (h.size(), 0.0); upPos = downPos = 0;
        }
        void reset() { std::fill (upHist.begin(), upHist.end(), 0.0); std::fill (downHist.begin(), downHist.end(), 0.0); upPos = downPos = 0; }
        /** Delay in samples at the outer rate, both filters together. */
        int latency() const { return factor == 1 ? 0 : per; }

        template <typename Fn> double process (double in, Fn&& inner)
        {
            if (factor == 1) return inner (in);
            upHist[(size_t) upPos] = in; double out = 0.0;
            const int n = (int) h.size();
            for (int p = 0; p < factor; ++p)
            {
                // phase p of the up filter: taps p, p + factor, ... against the newest inputs first
                double up = 0.0; int idx = upPos;
                for (int k = p; k < n; k += factor) { up += h[(size_t) k] * upHist[(size_t) idx]; if (--idx < 0) idx = upLen - 1; }
                const double y = inner (up * factor);
                downHist[(size_t) downPos] = y; if (++downPos >= n) downPos = 0;
                // the output is taken at the first inner sample of the period, which makes the whole delay a whole number of outer samples
                if (p == 0) { int idx = downPos; for (int k = 0; k < n; ++k) { if (--idx < 0) idx = n - 1; out += h[(size_t) k] * downHist[(size_t) idx]; } }
            }
            if (++upPos >= upLen) upPos = 0;
            return out;
        }

    private:
        int factor = 1, per = 1, upLen = 1, upPos = 0, downPos = 0; std::vector<double> h, upHist, downHist;
    };
}
