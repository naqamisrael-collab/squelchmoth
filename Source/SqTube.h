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
        const double ex = std::exp (-std::abs (x)), inv = 1.0 / (1.0 + ex);   // one exponential serves both (1.56)
        const double soft = std::max (x, 0.0) + std::log1p (ex);                 // ln(1 + e^x)
        const double sig = x >= 0.0 ? inv : ex * inv;                            // its slope
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

    //============================================================================== power stage
    /** A beam tetrode / pentode by its published model parameters. */
    struct Pentode { double mu = 8.7, ex = 1.35, kg1 = 1460, kg2 = 4500, kp = 48, kvb = 12, rgi = 1000, ccg = 14.0e-12, cpg = 0.85e-12, ccp = 12.0e-12; };
    inline Pentode classicBeam() { return {}; }
    inline Pentode heavyBeam()   { Pentode t; t.mu = 7.9; t.kg1 = 890; t.kg2 = 4200; t.kp = 60; t.kvb = 24; return t; }
    inline Pentode boldBeam()    { Pentode t; t.mu = 8.8; t.kg1 = 730; t.kg2 = 4200; t.kp = 32; t.kvb = 16; return t; }

    /** Single-ended output stage: input capacitor, grid leak, grid stopper, cathode resistor with its capacitor, screen
        resistor from the supply, and a linear output transformer seen from the primary: winding resistance in series
        with the primary inductance and the reflected load side by side. */
    struct PowerCircuit { double bplus = 350.0, ra = 4000.0, rw = 150.0, lp = 20.0, rk = 270.0, ck = 220.0e-6, rs = 1000.0, rgs = 4700.0, rgl = 220.0e3, cin = 100.0e-9; };

    /** Plate and screen currents and their slopes against plate, control grid and screen (all measured from the cathode).
        E1 = Vs/kp ln(1 + exp(kp (1/mu + Vg/Vs))), Ip = 2 E1^ex / kg1 atan(Vp/kvb); Is = (Vs/mu + Vg)^ex / kg2, nothing where that bracket is negative. */
    inline void pentode (const Pentode& t, double vp, double vg, double vs, double& ip, double dIp[3], double& is, double dIs[3])
    {
        ip = is = 0.0; dIp[0] = dIp[1] = dIp[2] = dIs[0] = dIs[1] = dIs[2] = 0.0;
        const double s = std::max (vs, 1.0e-3);                                  // a screen at or below the cathode passes nothing
        const double x = t.kp * (1.0 / t.mu + vg / s);
        const double ex = std::exp (-std::abs (x)), inv = 1.0 / (1.0 + ex), soft = std::max (x, 0.0) + std::log1p (ex), sig = x >= 0.0 ? inv : ex * inv;
        const double e1 = s / t.kp * soft;
        if (e1 > 0.0)
        {
            const double pw = std::pow (e1, t.ex), k = 2.0 * pw / t.kg1, at = std::atan (vp / t.kvb), dk = 2.0 * t.ex * pw / (e1 * t.kg1);
            ip = k * at;
            dIp[0] = k / (t.kvb * (1.0 + (vp / t.kvb) * (vp / t.kvb)));
            dIp[1] = dk * sig * at;
            dIp[2] = vs > 1.0e-3 ? dk * (soft / t.kp - sig * vg / s) * at : 0.0;
        }
        const double b = vs / t.mu + vg;
        if (b > 0.0) { const double pw = std::pow (b, t.ex); is = pw / t.kg2; const double d = t.ex * pw / (b * t.kg2); dIs[1] = d; dIs[2] = d / t.mu; }
    }

    /** The transformer's iron: a gapped core with saturation and hysteresis (the Jiles-Atherton equations, in electrical
        units). h is the part of the magnetising current that drives the iron and m the iron's magnetisation, both in
        amperes; the flux linkage is lair (h + m) volt-seconds and the gap takes a further (flux / lg) of current.
        The direction switch of the published equations is smoothed over v0 volts so that a solver with a variable
        step (the reference simulator) can follow it; both sides use the same smoothing. */
    struct Core
    {
        bool on = false; double lair = 0.02, ms = 150.0, a = 0.0025, alpha = 1.0e-5, k = 0.004, c = 0.2, lg = 22.0, v0 = 0.01;
        static double langevin (double x)  { return std::abs (x) > 1.0e-4 ? 1.0 / std::tanh (x) - 1.0 / x : x / 3.0; }
        static double langevinD (double x) { if (std::abs (x) <= 1.0e-4) return 1.0 / 3.0; const double t = std::tanh (x); return 1.0 / (x * x) - 1.0 / (t * t) + 1.0; }
        /** dm/dh for the present state and voltage across the winding. */
        double slope (double h, double m, double u) const
        {
            const double q = (h + alpha * m) / a; double lang, langD;
            if (std::abs (q) > 1.0e-4) { const double th = std::tanh (q); lang = 1.0 / th - 1.0 / q; langD = 1.0 / (q * q) - 1.0 / (th * th) + 1.0; } else { lang = q / 3.0; langD = 1.0 / 3.0; }
            const double d0 = ms * lang - m, z = (d0 >= 0 ? u : -u) / (2.0 * v0);
            double d = std::abs (d0) * (z > 20.0 ? 1.0 : z < -20.0 ? 0.0 : 0.5 * (1.0 + std::tanh (z)));
            d = std::min (d, 0.9 * (1.0 - c) * k / alpha);                                // the published form has no guard here; this keeps its denominator positive
            const double firr = (1.0 - c) * d / ((1.0 - c) * k - alpha * d), beta = c * ms / a * langD;
            return std::max (0.0, (firr + beta) / (1.0 - alpha * beta));
        }
        /** The same iron, larger or smaller: flux at saturation and inductance both go with size. */
        static Core sized (double scale, bool isOn = true) { Core k; k.on = isOn; k.lair *= scale; k.lg *= scale; return k; }
        /** The state the iron sits in with a steady current and no history: on its anhysteretic curve. */
        void atRest (double current, double& h, double& m) const
        {
            const double sign = current < 0 ? -1.0 : 1.0, target = std::abs (current);
            auto magnetisation = [this] (double hh) { double mm = 0.0; for (int i = 0; i < 200; ++i) { const double n = ms * langevin ((hh + alpha * mm) / a); if (std::abs (n - mm) < 1.0e-14 * (1.0 + std::abs (n))) return n; mm = n; } return mm; };
            double lo = 0.0, hi = target;                                               // h + lair (h + m(h)) / lg rises with h: halve the interval
            for (int it = 0; it < 100; ++it) { const double mid = 0.5 * (lo + hi); if (mid + lair * (mid + magnetisation (mid)) / lg > target) hi = mid; else lo = mid; }
            const double hh = 0.5 * (lo + hi), mm = magnetisation (hh);
            // the last digits: make the current exact for this m
            h = sign * (target - lair * mm / lg) / (1.0 + lair / lg); m = sign * mm;
        }
    };

    /** The power stage. prepare() with the rate it will be run at, then process(): grid volts in, volts across the transformer primary out
        (no steady part: a transformer passes none). With circuit.core.on the primary inductance is the iron above; otherwise a plain inductance. */
    class PowerStage
    {
    public:
        void prepare (double sampleRate, const Pentode& tube = {}, const PowerCircuit& circuit = {}, const Core& iron = {}) { t = tube; c = circuit; core = iron; T = 1.0 / sampleRate; reset(); }

        /** Settles to the resting point: no signal, capacitors charged, the iron on its anhysteretic curve. */
        void reset()
        {
            ramp = 1.0; rampStep = 0.0;
            x = { 0.0, 0.0, 0.07 * c.rk, c.bplus - 10.0, c.bplus - 5.0, c.bplus - 10.0 };
            for (int pass = 0; pass < 40; ++pass) solve (0.0, true, 60);
            restK = x[2]; restP = x[3]; restS = x[4];
            { double ip, a[3], is, b[3]; pentode (t, x[3] - x[2], x[1] - x[2], x[4] - x[2], ip, a, is, b); restIp = ip; restIs = is; }
            cgkJunction = 10.0e-12 / std::sqrt (1.0 + std::max (0.0, x[2] - x[1]));
            caps[0] = { c.cin, x[0], 0.0 }; caps[1] = { t.ccg + cgkJunction, x[1] - x[2], 0.0 }; caps[2] = { t.cpg, x[1] - x[3], 0.0 };
            caps[3] = { t.ccp, x[3] - x[2], 0.0 }; caps[4] = { c.ck, x[2], 0.0 };
            const double idc = (c.bplus - x[3]) / c.rw; x[5] = x[3]; xPrev = x;
            iL = idc; uL = 0.0; core.atRest (idc, hC, mC); rateC = 0.0;
        }

        /** Switched on from nothing: everything discharged, the iron unmagnetised, the supply rising over rampSeconds. */
        void startCold (double rampSeconds = 0.010)
        {
            x = { 0, 0, 0, 0, 0, 0 }; xPrev = x; for (auto& cp : caps) { cp.v = 0; cp.i = 0; }
            cgkJunction = 10.0e-12; caps[0].c = c.cin; caps[1].c = t.ccg + cgkJunction; caps[2].c = t.cpg; caps[3].c = t.ccp; caps[4].c = c.ck;
            iL = uL = hC = mC = rateC = 0.0; ramp = 0.0; rampStep = T / std::max (T, rampSeconds);
        }

        void setParts (const Pentode& tube, const PowerCircuit& circuit, const Core& iron)
        {
            t = tube; c = circuit; const bool was = core.on; const double flux = core.lair * (hC + mC);
            core = iron; caps[0].c = c.cin; caps[1].c = t.ccg + cgkJunction; caps[2].c = t.cpg; caps[3].c = t.ccp; caps[4].c = c.ck;
            if (core.on && was) { mC = flux / core.lair - hC; }                          // the flux stays; the iron's share of it follows the new core
            else if (core.on) { core.atRest (iL, hC, mC); rateC = 0.0; }
        }
        void setParts (const Pentode& tube, const PowerCircuit& circuit) { setParts (tube, circuit, core); }

        /** Mid-band gain for small signals, grid to primary volts (positive: the output is taken so that it does not invert). */
        static double smallSignalGain (const Pentode& t, const PowerCircuit& c)
        {
            PowerStage r; r.t = t; r.c = c; r.T = 1.0 / 192000.0; r.reset();
            double ip, a[3], is, b[3]; pentode (t, r.x[3] - r.x[2], r.x[1] - r.x[2], r.x[4] - r.x[2], ip, a, is, b);
            const double gk = c.ck > 0 ? 1.0e9 : 1.0 / c.rk, gl = 1.0 / (c.ra + c.rw), gs = 1.0 / c.rs;
            double m[3][4] = {
                { gk + a[0] + a[1] + a[2] + b[1] + b[2], -a[0], -a[2] - b[2], a[1] + b[1] },
                { -a[0] - a[1] - a[2], gl + a[0], a[2], -a[1] },
                { -b[1] - b[2], 0.0, gs + b[2], -b[1] } };
            for (int col = 0; col < 3; ++col)
            {
                int piv = col; for (int rr = col + 1; rr < 3; ++rr) if (std::abs (m[rr][col]) > std::abs (m[piv][col])) piv = rr;
                if (piv != col) for (int j = 0; j < 4; ++j) std::swap (m[piv][j], m[col][j]);
                for (int rr = 0; rr < 3; ++rr) if (rr != col) { const double f = m[rr][col] / m[col][col]; for (int j = col; j < 4; ++j) m[rr][j] -= f * m[col][j]; }
            }
            const double vp = m[1][3] / m[1][1];
            return -vp * c.ra / (c.ra + c.rw);
        }

        double restingPlate() const { return restP; }   double restingCathode() const { return restK; }   double restingScreen() const { return restS; }
        double restingPlateCurrent() const { return restIp; }   double restingScreenCurrent() const { return restIs; }
        int lastIterations() const { return iterations; }
        /** The iron's present flux linkage in volt-seconds (the plain inductance: lp times its current). */
        double flux() const { return core.on ? core.lair * (hC + mC) : c.lp * iL; }
        double fluxAtSaturation() const { return core.lair * core.ms; }
        double coreField() const { return hC; }   double coreMagnetisation() const { return mC; }

        double process (double vin) { if (ramp < 1.0) ramp = std::min (1.0, ramp + rampStep); return solve (vin, false, 50); }

    private:
        // the iron for a trial voltage u across the winding: field at the end of the step (trapezoidal rule), its current and di/du
        void ironAt (double u, double& h, double& rate, double& current, double& slopeOut) const
        {
            const double sum = hC + mC + T / (2.0 * core.lair) * (u + uL);                // h + m at the end of the step: the flux, exactly
            const double base = hC + 0.5 * T * rateC, gain = 0.5 * T * u / core.lair;     // h = base + gain / (1 + f(h))
            double f = 0.0; h = hC; bool done = false;
            for (int it = 0; it < 8 && ! done; ++it)                                     // away from the knee of the curve this settles at once
            {
                f = core.slope (h, sum - h, u); const double hn = base + gain / (1.0 + f);
                done = std::abs (hn - h) < 1.0e-15 + 1.0e-10 * std::abs (hn); h = hn;
            }
            if (! done)
            {
                // at the knee: the answer lies between no iron (f = 0) and iron that takes all the flux; close in on it (false position, Illinois)
                double lo = std::min (base, base + gain), hi = std::max (base, base + gain);
                auto miss = [&] (double hh) { f = core.slope (hh, sum - hh, u); return hh - base - gain / (1.0 + f); };
                double flo = miss (lo), fhi = miss (hi);
                if (flo > 0.0 || fhi < 0.0) h = flo > 0.0 ? lo : hi;                       // cannot happen while f >= 0; keep the nearer end
                else for (int it = 0; it < 60; ++it)
                {
                    const double mid = (fhi - flo) != 0.0 ? lo - flo * (hi - lo) / (fhi - flo) : 0.5 * (lo + hi), fm = miss (mid); h = mid;
                    if (std::abs (fm) < 1.0e-15 + 1.0e-10 * std::abs (mid) || (hi - lo) < 1.0e-15 + 1.0e-11 * std::abs (mid)) break;
                    if (fm > 0.0) { hi = mid; fhi = fm; flo *= 0.5; } else { lo = mid; flo = fm; fhi *= 0.5; }
                }
                f = core.slope (h, sum - h, u);
            }
            rate = u / (core.lair * (1.0 + f));
            current = h + core.lair * sum / core.lg;
            slopeOut = 0.5 * T * (1.0 / (core.lair * (1.0 + f)) + 1.0 / core.lg);
        }

        // dc: capacitors open, the inductance a plain wire - used to find the resting point
        double solve (double vin, bool dc, int maxIt)
        {
            double g[5], ieq[5];
            for (int i = 0; i < 5; ++i) { g[i] = dc ? 0.0 : 2.0 * caps[i].c / T; ieq[i] = dc ? 0.0 : -(g[i] * caps[i].v + caps[i].i); }
            const double gL = T / (2.0 * c.lp), hL = iL + gL * uL, bp = c.bplus * ramp;
            auto v = x; double capV[5] = {}, capI[5] = {}; iterations = 0;
            if (! dc && ramp >= 1.0)                                                      // start from where the last two answers point, unless they are far apart (clipping: the last answer is the safer start)
            {
                double apart = 0.0; for (size_t i = 0; i < 6; ++i) apart = std::max (apart, std::abs (x[i] - xPrev[i]));
                if (apart < 3.0) for (size_t i = 0; i < 6; ++i) v[i] = 2.0 * x[i] - xPrev[i];
            }
            double hNew = hC, rateNew = rateC, iwLast = iL;
            for (int it = 0; it < maxIt; ++it)
            {
                ++iterations;
                const double vA = v[0], vG = v[1], vK = v[2], vP = v[3], vS = v[4], vM = v[5], u = vM - vP;
                double ip, a[3], is, b[3]; pentode (t, vP - vK, vG - vK, vS - vK, ip, a, is, b);
                double ig, dIg; grid (tg, vG - vK, ig, dIg);
                // the winding between M and P: its current and slope against u
                double iw, gw;
                if (dc) { gw = 1.0e7; iw = gw * u; }
                else if (core.on) { ironAt (u, hNew, rateNew, iw, gw); iwLast = iw; }
                else { gw = gL; iw = gL * u + hL; }
                capV[0] = vA - vin; capV[1] = vG - vK; capV[2] = vG - vP; capV[3] = vP - vK; capV[4] = vK;
                for (int i = 0; i < 5; ++i) capI[i] = g[i] * capV[i] + ieq[i];
                double F[6], J[6][6] = {};
                F[0] = capI[0] + vA / c.rgl + (vA - vG) / c.rgs;
                J[0][0] = g[0] + 1.0 / c.rgl + 1.0 / c.rgs; J[0][1] = -1.0 / c.rgs;
                F[1] = (vG - vA) / c.rgs + capI[1] + capI[2] + ig;
                J[1][0] = -1.0 / c.rgs; J[1][1] = 1.0 / c.rgs + g[1] + g[2] + dIg; J[1][2] = -g[1] - dIg; J[1][3] = -g[2];
                F[2] = vK / c.rk + capI[4] - capI[1] - ig - ip - is - capI[3];
                J[2][1] = -g[1] - dIg - a[1] - b[1]; J[2][2] = 1.0 / c.rk + g[4] + g[1] + dIg + a[0] + a[1] + a[2] + b[1] + b[2] + g[3]; J[2][3] = -a[0] - g[3]; J[2][4] = -a[2] - b[2];
                F[3] = -(iw + u / c.ra) + ip + capI[3] - capI[2];
                J[3][1] = a[1] - g[2]; J[3][2] = -a[0] - a[1] - a[2] - g[3]; J[3][3] = gw + 1.0 / c.ra + a[0] + g[3] + g[2]; J[3][4] = a[2]; J[3][5] = -gw - 1.0 / c.ra;
                F[4] = (vS - bp) / c.rs + is;
                J[4][1] = b[1]; J[4][2] = -b[1] - b[2]; J[4][4] = 1.0 / c.rs + b[2];
                F[5] = (vM - bp) / c.rw + iw + u / c.ra;
                J[5][3] = -gw - 1.0 / c.ra; J[5][5] = 1.0 / c.rw + gw + 1.0 / c.ra;
                double d[6]; if (! solveN (J, F, d)) break;
                const double big = std::max ({ std::abs (d[1] - d[2]) / 2.0, std::abs (d[3]) / 60.0, std::abs (d[4]) / 60.0, std::abs (d[5]) / 60.0, 1.0 }), scale = 1.0 / big;
                double moved = 0.0; for (int i = 0; i < 6; ++i) { v[(size_t) i] -= scale * d[i]; moved = std::max (moved, std::abs (scale * d[i])); }
                if (moved < (dc ? 1.0e-10 : 1.0e-8)) break;
            }
            xPrev = dc ? v : x; x = v; if (dc) return 0.0;
            capV[0] = v[0] - vin; capV[1] = v[1] - v[2]; capV[2] = v[1] - v[3]; capV[3] = v[3] - v[2]; capV[4] = v[2];
            for (int i = 0; i < 5; ++i) { caps[i].i = g[i] * capV[i] + ieq[i]; caps[i].v = capV[i]; }
            const double u = v[5] - v[3];
            if (core.on) { const double sum = hC + mC + T / (2.0 * core.lair) * (u + uL); hC = hNew; mC = sum - hNew; rateC = rateNew; iL = iwLast; }      // the iron as found in the last pass (the voltage moved less than the tolerance since); the flux is kept exact
            else iL = gL * u + hL;
            uL = u;
            return u;
        }

        static bool solveN (double a[6][6], const double* b, double* out)
        {
            constexpr int n = 6; double m[n][n + 1];
            for (int i = 0; i < n; ++i) { for (int j = 0; j < n; ++j) m[i][j] = a[i][j]; m[i][n] = b[i]; }
            for (int col = 0; col < n; ++col)
            {
                int piv = col; for (int r = col + 1; r < n; ++r) if (std::abs (m[r][col]) > std::abs (m[piv][col])) piv = r;
                if (std::abs (m[piv][col]) < 1.0e-300) return false;
                if (piv != col) for (int j = 0; j <= n; ++j) std::swap (m[piv][j], m[col][j]);
                for (int r = col + 1; r < n; ++r) { const double f = m[r][col] / m[col][col]; if (f != 0.0) for (int j = col; j <= n; ++j) m[r][j] -= f * m[col][j]; }
            }
            for (int i = n - 1; i >= 0; --i) { double s = m[i][n]; for (int j = i + 1; j < n; ++j) s -= m[i][j] * out[j]; out[i] = s / m[i][i]; }
            return true;
        }

        struct Cap { double c = 0, v = 0, i = 0; };
        Pentode t; PowerCircuit c; Core core; Tube tg = [] { Tube g; g.rgi = 1000; return g; }();
        double T = 1.0 / 192000.0, cgkJunction = 0, restK = 0, restP = 0, restS = 0, restIp = 0, restIs = 0, iL = 0, uL = 0, hC = 0, mC = 0, rateC = 0, ramp = 1.0, rampStep = 0.0; int iterations = 0;
        std::array<double, 6> x { 0, 0, 0, 0, 0, 0 }, xPrev { 0, 0, 0, 0, 0, 0 }; Cap caps[5];
    };

    //============================================================================== line transformer
    /** A console's input or output transformer on its own: a source of resistance r1 (source and winding together) feeding
        a one-to-one transformer whose secondary is loaded with r2. The iron is the same Core as the output stage's, without
        a gap (no steady current flows here), so it distorts evenly on both sides: odd harmonics, most in the bass, where the
        flux is largest. volts in, volts across the secondary out. */
    struct LineCircuit { double r1 = 150.0, r2 = 10000.0; };
    inline Core lineCore (double scale = 1.0) { Core k; k.on = true; k.lair = 0.002 * scale; k.ms = 150.0; k.a = 0.002; k.alpha = 1.0e-5; k.k = 0.003; k.c = 0.2; k.lg = 1.0e12; k.v0 = 0.01; return k; }

    class LineTransformer
    {
    public:
        void prepare (double sampleRate, const Core& iron = lineCore(), const LineCircuit& circuit = {}) { core = iron; c = circuit; T = 1.0 / sampleRate; reset(); }
        void reset() { hC = mC = rateC = uL = uL2 = 0.0; iterations = 0; }
        void setParts (const Core& iron, const LineCircuit& circuit) { const double flux = core.lair * (hC + mC); core = iron; c = circuit; mC = flux / core.lair - hC; }
        /** Level through the transformer in its pass band (the two resistances divide it). */
        static double passGain (const LineCircuit& c) { return c.r2 / (c.r1 + c.r2); }
        /** Inductance seen by small signals with the iron at rest, in henries. */
        static double smallInductance (const Core& k) { const double beta = k.c * k.ms / k.a / 3.0; return k.lair * (1.0 + beta / (1.0 - k.alpha * beta)); }
        /** Where the bass is 3 dB down for small signals, in Hz. */
        static double bassCorner (const Core& k, const LineCircuit& c) { return (c.r1 * c.r2 / (c.r1 + c.r2)) / (2.0 * 3.14159265358979323846 * smallInductance (k)); }
        double flux() const { return core.lair * (hC + mC); }
        double fluxAtSaturation() const { return core.lair * core.ms; }
        int lastIterations() const { return iterations; }

        double process (double vin)
        {
            // one unknown, the voltage across the winding: (vin - u) / r1 = iron (u) + u / r2
            if (vin == 0.0 && std::abs (uL) < 1.0e-7 && std::abs (hC) < 1.0e-9)                 // nothing coming in and the iron come to rest: an idle track costs nothing
            {
                mC += hC; hC = 0.0; rateC = 0.0; uL = uL2 = 0.0; iterations = 0; return 0.0;          // (the flux it holds is kept)
            }
            double u = std::abs (uL - uL2) < 3.0 ? 2.0 * uL - uL2 : uL, h = hC, rate = rateC, cur = 0.0, g = 0.0; iterations = 0;      // start from where the last two answers point
            for (int it = 0; it < 40; ++it)
            {
                ++iterations; ironAt (u, h, rate, cur, g);
                const double f = (u - vin) / c.r1 + cur + u / c.r2, d = 1.0 / c.r1 + g + 1.0 / c.r2, step = f / d;
                u -= step; if (std::abs (step) < 1.0e-8) break;                      // the iron as found in this pass is kept: the voltage moved less than the tolerance since
            }
            const double sum = hC + mC + T / (2.0 * core.lair) * (u + uL); hC = h; mC = sum - h; rateC = rate; uL2 = uL; uL = u;
            return u;
        }

    private:
        void ironAt (double u, double& h, double& rate, double& current, double& slopeOut) const
        {
            const double sum = hC + mC + T / (2.0 * core.lair) * (u + uL), base = hC + 0.5 * T * rateC, gain = 0.5 * T * u / core.lair;
            double f = 0.0; h = hC; bool done = false;
            for (int it = 0; it < 8 && ! done; ++it) { f = core.slope (h, sum - h, u); const double hn = base + gain / (1.0 + f); done = std::abs (hn - h) < 1.0e-15 + 1.0e-10 * std::abs (hn); h = hn; }
            if (! done)
            {
                double lo = std::min (base, base + gain), hi = std::max (base, base + gain);
                auto miss = [&] (double hh) { f = core.slope (hh, sum - hh, u); return hh - base - gain / (1.0 + f); };
                double flo = miss (lo), fhi = miss (hi);
                if (flo > 0.0 || fhi < 0.0) h = flo > 0.0 ? lo : hi;
                else for (int it = 0; it < 60; ++it)
                {
                    const double mid = (fhi - flo) != 0.0 ? lo - flo * (hi - lo) / (fhi - flo) : 0.5 * (lo + hi), fm = miss (mid); h = mid;
                    if (std::abs (fm) < 1.0e-15 + 1.0e-10 * std::abs (mid) || (hi - lo) < 1.0e-15 + 1.0e-11 * std::abs (mid)) break;
                    if (fm > 0.0) { hi = mid; fhi = fm; flo *= 0.5; } else { lo = mid; flo = fm; fhi *= 0.5; }
                }
                f = core.slope (h, sum - h, u);
            }
            rate = u / (core.lair * (1.0 + f));
            current = h + core.lair * sum / core.lg;
            slopeOut = 0.5 * T * (1.0 / (core.lair * (1.0 + f)) + 1.0 / core.lg);
        }
        Core core; LineCircuit c; double T = 1.0 / 192000.0, hC = 0, mC = 0, rateC = 0, uL = 0, uL2 = 0; int iterations = 0;
    };

    //============================================================================== oversampling
    /** Runs a process at `factor` times the rate (1, 2, 4, 8 or 16). Since 1.56: a cascade of half-band filters, each
        stage doubling the rate (Kaiser-windowed, 120 dB down outside the band, flat to 20 kHz at 44.1 kHz), up and then
        the same back down; about a fifth of the arithmetic of the single long filter it replaced. Every half-band filter
        has its odd taps zero but the middle one, so half the products are skipped. The delay is padded to the same
        whole number as before - latency() samples at the outer rate (96 when oversampling) - so nothing downstream moves. */
    class Oversampler
    {
    public:
        void prepare (int factorIn, int /*tapsPerPhase*/ = 96, double /*stopDb*/ = 130.0)
        {
            factor = 1; stages = 0; while (factor < std::max (1, std::min (16, factorIn))) { factor *= 2; ++stages; }
            static const int lengths[4] = { 167, 33, 25, 17 };      // round trips of 83, 16, 12 and 8 samples at each stage's input rate: 83 + 8 + 3 + 1 outer samples
            int total = 0;
            for (int k = 0; k < stages; ++k)
            {
                auto& st = stage[k]; const int n = lengths[k], L = (n - 1) / 2; st.L = L; total += L >> k;
                const double beta = 0.1102 * (120.0 - 8.7); auto bessel = [] (double v) { double s = 1.0, term = 1.0; for (int i = 1; i < 60; ++i) { term *= (v / (2.0 * i)) * (v / (2.0 * i)); s += term; if (term < 1.0e-18 * s) break; } return s; };
                std::vector<double> h ((size_t) n); double sum = 0.0;
                for (int j = 0; j < n; ++j) { const int m = j - L; const double r = (double) m / L; const double sinc = m == 0 ? 1.0 : (m % 2 == 0 ? 0.0 : std::sin (3.14159265358979323846 * m / 2.0) / (3.14159265358979323846 * m / 2.0)); h[(size_t) j] = sinc * bessel (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel (beta); sum += h[(size_t) j]; }
                for (auto& v : h) v *= 2.0 / sum;                  // gain 2 for the zeros put in going up; halved coming down
                for (int ph = 0; ph < 2; ++ph) { st.up[ph].clear(); for (int j = ph; j < n; j += 2) if (h[(size_t) j] != 0.0) st.up[ph].push_back ({ (j - ph) / 2, h[(size_t) j] }); }
                st.down.clear(); for (int j = 0; j < n; ++j) if (h[(size_t) j] != 0.0) st.down.push_back ({ j, 0.5 * h[(size_t) j] });
                st.upLen = (n + 1) / 2 + 1; st.downLen = n; st.upHist.assign ((size_t) st.upLen * 2, 0.0); st.downHist.assign ((size_t) st.downLen * 2, 0.0); st.upPos = st.downPos = 0; st.even = true;
            }
            pad = factor == 1 ? 0 : 96 - total; padLine.assign ((size_t) std::max (1, pad), 0.0); padPos = 0; latencySamples = factor == 1 ? 0 : 96;
        }
        void reset()
        {
            for (int k = 0; k < stages; ++k) { auto& st = stage[k]; std::fill (st.upHist.begin(), st.upHist.end(), 0.0); std::fill (st.downHist.begin(), st.downHist.end(), 0.0); st.upPos = st.downPos = 0; st.even = true; }
            std::fill (padLine.begin(), padLine.end(), 0.0); padPos = 0;
        }
        /** Delay in samples at the outer rate, both directions together. */
        int latency() const { return latencySamples; }

        template <typename Fn> double process (double in, Fn&& inner)
        {
            if (factor == 1) return inner (in);
            // up: each stage turns every sample into two
            double a[16], b[16]; a[0] = in; int count = 1;
            for (int k = 0; k < stages; ++k) { auto& st = stage[k]; for (int i = 0; i < count; ++i) up (st, a[i], b[2 * i], b[2 * i + 1]); count *= 2; std::copy (b, b + count, a); }
            for (int i = 0; i < count; ++i) a[i] = inner (a[i]);
            // down: each stage keeps one sample in two
            for (int k = stages - 1; k >= 0; --k) { auto& st = stage[k]; const int half = count / 2; for (int i = 0; i < half; ++i) b[i] = down (st, a[2 * i], a[2 * i + 1]); count = half; std::copy (b, b + count, a); }
            if (pad <= 0) return a[0];
            const double out = padLine[(size_t) padPos]; padLine[(size_t) padPos] = a[0]; if (++padPos >= pad) padPos = 0; return out;
        }

    private:
        struct Tap { int at; double c; };
        struct Stage
        {
            std::vector<Tap> up[2], down; std::vector<double> upHist, downHist; int L = 0, upLen = 1, downLen = 1, upPos = 0, downPos = 0; bool even = true;
        };
        static void up (Stage& st, double x, double& y0, double& y1)
        {
            st.upHist[(size_t) st.upPos] = st.upHist[(size_t) (st.upPos + st.upLen)] = x; const double* h = st.upHist.data() + st.upPos + st.upLen;      // h[0] newest, h[-q] q samples ago
            double s0 = 0.0, s1 = 0.0; for (const auto& t : st.up[0]) s0 += t.c * h[-t.at]; for (const auto& t : st.up[1]) s1 += t.c * h[-t.at];
            y0 = s0; y1 = s1; if (++st.upPos >= st.upLen) st.upPos = 0;
        }
        static double down (Stage& st, double v0, double v1)
        {
            st.downHist[(size_t) st.downPos] = st.downHist[(size_t) (st.downPos + st.downLen)] = v0; const double* h = st.downHist.data() + st.downPos + st.downLen;
            double s = 0.0; for (const auto& t : st.down) s += t.c * h[-t.at];
            if (++st.downPos >= st.downLen) st.downPos = 0; st.downHist[(size_t) st.downPos] = st.downHist[(size_t) (st.downPos + st.downLen)] = v1; if (++st.downPos >= st.downLen) st.downPos = 0;
            return s;
        }
        Stage stage[4]; int factor = 1, stages = 0, pad = 0, padPos = 0, latencySamples = 0; std::vector<double> padLine;
    };
}
