#pragma once
// SqSpring: a helical spring as a vibrating body, for spring reverb. Pure C++ (no JUCE).
//
// The equations are the published simplified ones for a thin wire wound into a helix (a curved rod): u is the
// motion along the wire, v the motion towards the helix's axis, s the distance along the wire, c^2 = E / rho,
// r the wire's radius and k = 1 / (the coil's radius):
//     u_tt = c^2 (u_ss - k v_s)
//     v_tt = -(c^2 r^2 / 4) (v_ssss + 2 k^2 v_ss + k^4 v) + c^2 k (u_s - k v)
// A wave exp(i (b s - w t)) then obeys  (w^2 - c^2 b^2) (w^2 - c^2 k^2 - (c^2 r^2 / 4) (b^2 - k^2)^2) = c^4 k^2 b^2.
// Its slow branch is the sound of a spring: long waves all travel at c r k / 2 (so the echoes come 4 L R / (r c)
// apart), shorter ones travel slower and slower until, at the transition frequency, they hardly travel at all -
// which is why every echo is a chirp that sweeps up and smears - and still shorter ones behave like a bent bar.
//
// With the ends held so that the wire may slide but not move sideways (u_s = 0, v = 0, v_ss = 0: our choice), the
// spring's own modes are whole half-waves, b = n pi / L, and the motion is exactly a sum of them. That is what is
// computed here: one damped resonator per mode, driven at one end and heard at the other.
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace SqSpring
{
    /** A spring by its dimensions (metres) and its steel. */
    struct Spring
    {
        double coilDiameter = 0.0044, wireDiameter = 0.00035, helixLength = 0.163, turns = 303.0;
        double youngs = 2.11e11, density = 7800.0;
        double c() const { return std::sqrt (youngs / density); }
        double r() const { return 0.5 * wireDiameter; }
        double k() const { return 2.0 / coilDiameter; }
        double wireLength() const { const double round = 3.14159265358979323846 * coilDiameter * turns; return std::sqrt (round * round + helixLength * helixLength); }
        /** The slow branch: angular frequency for a wavenumber, from the relation above. */
        double omega (double b) const
        {
            const double c2 = c() * c(), kk = k(), d = b * b - kk * kk, stiff = 0.25 * c2 * r() * r() * d * d;
            const double B = c2 * b * b + c2 * kk * kk + stiff, C = c2 * b * b * stiff;        // w^4 - B w^2 + C = 0
            return std::sqrt (std::max (0.0, 2.0 * C / (B + std::sqrt (std::max (0.0, B * B - 4.0 * C)))));
        }
        /** How much of a mode's motion is along the wire (where it is driven and heard): 1 / (1 + (V / U)^2). */
        double along (double b, double w) const { const double c2 = c() * c(), ratio = (w * w - c2 * b * b) / (c2 * k() * b); return 1.0 / (1.0 + ratio * ratio); }
        /** Echo spacing for long waves, there and back: 4 L R / (r c). */
        double echoSeconds() const { return 4.0 * wireLength() * (0.5 * coilDiameter) / (r() * c()); }
        /** The transition frequency in hertz: the top of the slow branch between b = 0 and b = k (found by search). */
        double transitionHz() const
        {
            double lo = 0.0, hi = k();
            for (int i = 0; i < 80; ++i) { const double a = lo + (hi - lo) / 3.0, b = hi - (hi - lo) / 3.0; if (omega (a) < omega (b)) lo = a; else hi = b; }
            return omega (0.5 * (lo + hi)) / (2.0 * 3.14159265358979323846);
        }
    };

    /** How quickly the modes die: 60 dB in `seconds` at low frequencies, faster higher up (the loss grows with the square of
        frequency until, at `halfAtHz`, the decay time has halved). */
    struct Loss { double seconds = 2.0, halfAtHz = 4000.0; double sigma (double hz) const { const double x = hz / halfAtHz; return 6.907755278982137 / seconds * (1.0 + x * x); } };

    /** One spring as a bank of resonators. The output is the far end's velocity, which is what a pickup coil senses. */
    struct Bank
    {
        static constexpr int capacity = 1500;
        Bank() { re.assign (capacity, 0.0f); im.assign (capacity, 0.0f); cr.assign (capacity, 0.0f); ci.assign (capacity, 0.0f); gain.assign (capacity, 0.0f); lag.assign (capacity, 0.0f); hz.assign (capacity, 0.0f); }

        /** Works the modes out again (no allocation). lowHz .. highHz: the modes kept. Returns how many. */
        int design (const Spring& sp, const Loss& loss, double sampleRate, double lowHz = 20.0, double highHz = 20000.0)
        {
            const double pi = 3.14159265358979323846, L = sp.wireLength(), top = std::min (highHz, 0.49 * sampleRate); int count = 0; double beyond = 0.0;
            for (int n = 1; n < 200000 && count < capacity; ++n)
            {
                const double b = n * pi / L, w = sp.omega (b), f = w / (2.0 * pi);
                if (b > sp.k() && f > top) { if (++beyond > 3) break; continue; }       // past the bar-like branch's audible part
                if (f < lowHz || f > top) continue;
                const double s = loss.sigma (f), wd = std::sqrt (std::max (0.0, w * w - s * s)), rad = std::exp (-s / sampleRate);
                cr[(size_t) count] = (float) (rad * std::cos (wd / sampleRate)); ci[(size_t) count] = (float) (rad * std::sin (wd / sampleRate));
                gain[(size_t) count] = (float) ((n % 2 == 0 ? 1.0 : -1.0) * sp.along (b, w)); lag[(size_t) count] = (float) (gain[(size_t) count] * (wd > 0.0 ? s / wd : 0.0)); hz[(size_t) count] = (float) f; ++count;
            }
            for (int i = count; i < used; ++i) { re[(size_t) i] = 0.0f; im[(size_t) i] = 0.0f; }
            used = count; return count;
        }
        void reset() { std::fill (re.begin(), re.end(), 0.0f); std::fill (im.begin(), im.end(), 0.0f); }

        /** One sample: the force at the near end in, the far end's velocity out (unscaled: see level()). */
        float process (float x)
        {
            float y = 0.0f; float* a = re.data(); float* b = im.data(); const float* p = cr.data(); const float* q = ci.data(); const float* g = gain.data(); const float* l = lag.data();
            // each mode's velocity after a push: exp(-sigma t) (cos(w t) - (sigma / w) sin(w t))
            for (int i = 0; i < used; ++i) { const float nr = p[i] * a[i] - q[i] * b[i] + x, ni = p[i] * b[i] + q[i] * a[i]; a[i] = nr; b[i] = ni; y += g[i] * nr - l[i] * ni; }
            return y;
        }
        /** The rms the output would have for white noise of rms 1 going in, with these modes (for setting the level). */
        double level() const { double s = 0.0; for (int i = 0; i < used; ++i) { const double r2 = (double) cr[(size_t) i] * cr[(size_t) i] + (double) ci[(size_t) i] * ci[(size_t) i]; s += 0.5 * (double) gain[(size_t) i] * gain[(size_t) i] / std::max (1.0e-9, 1.0 - r2); } return std::sqrt (s); }

        int used = 0; std::vector<float> re, im, cr, ci, gain, lag, hz;
    };

    /** Two real tanks' springs, by the dimensions measured and published for them. */
    inline Spring shortSpring (int which) { Spring s; s.helixLength = 0.065; s.wireDiameter = 0.00035; if (which == 0) { s.coilDiameter = 0.0054; s.turns = 148.0; } else { s.coilDiameter = 0.0061; s.turns = 133.0; } return s; }
    inline Spring longSpring (int which) { Spring s; s.helixLength = 0.163; s.wireDiameter = 0.00035; const double d[3] = { 0.0044, 0.0045, 0.0046 }, n[3] = { 303.0, 280.0, 351.0 }; s.coilDiameter = d[which % 3]; s.turns = n[which % 3]; return s; }
}
