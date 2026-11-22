#pragma once
// SqPlate: a thin steel plate as a vibrating body, for plate reverb. Pure C++ (no JUCE).
//
// The equation is the thin-plate one: u_tt = -kappa^2 (u_xxxx + 2 u_xxyy + u_yyyy) - 2 sigma u_t, with
// kappa^2 = E h^2 / (12 rho (1 - nu^2)). With the edges held but free to tilt (simply supported) the plate's own
// modes are sin(m pi x / Lx) sin(n pi y / Ly) at angular frequency kappa pi^2 (m^2 / Lx^2 + n^2 / Ly^2), and the
// motion is exactly a sum of them. A 2 m by 1 m plate half a millimetre thick has about 26,000 of them below
// 20 kHz - 1.3 in every hertz, evenly all the way up - which is why a plate sounds smooth where a spring chirps.
//
// That is too many to run as resonators while the music plays, so the plate is "cast" instead: the sum of all its
// modes is worked out once as the plate's answer to a single tap (pushed at one point, heard at two), and the music
// is then convolved with that answer. The two are the same thing for a body that obeys this equation.
#include <algorithm>
#include <cmath>
#include <vector>

namespace SqPlate
{
    struct Plate
    {
        double lx = 2.0, ly = 1.0, thickness = 0.0005, youngs = 2.0e11, density = 7872.0, poisson = 0.3;
        double kappa() const { return thickness * std::sqrt (youngs / (12.0 * density * (1.0 - poisson * poisson))); }
        double hz (int m, int n) const { return 0.5 * 3.14159265358979323846 * kappa() * ((double) m * m / (lx * lx) + (double) n * n / (ly * ly)); }
        /** Modes in every hertz (the same at every pitch): area / (2 kappa). */
        double modesPerHz() const { return lx * ly / (2.0 * kappa()); }
    };
    /** Where the plate is pushed and where its two pickups sit, as fractions of its sides. */
    struct Points { double driveX = 0.52, driveY = 0.53, leftX = 0.10, leftY = 0.45, rightX = 0.84, rightY = 0.45; };
    /** How the modes die: 60 dB in `seconds` at low frequencies; the decay time has halved by `halfAtHz`. */
    struct Loss { double seconds = 2.0, halfAtHz = 4000.0; double sigma (double f) const { const double x = f / halfAtHz; return 6.907755278982137 / seconds * (1.0 + x * x); } };

    struct Cast
    {
        /** Lists the modes between lowHz and highHz, longest-lived first. Returns how many. */
        int design (const Plate& p, const Points& at, const Loss& loss, double sampleRate, double lowHz = 20.0, double highHz = 20000.0)
        {
            const double pi = 3.14159265358979323846, top = std::min (highHz, 0.49 * sampleRate); fs = sampleRate; struct M { double f, s, gl, gr; }; std::vector<M> all;
            for (int m = 1; p.hz (m, 1) <= top; ++m) for (int n = 1; ; ++n)
            {
                const double f = p.hz (m, n); if (f > top) break; if (f < lowHz) continue;
                const double drive = std::sin (m * pi * at.driveX) * std::sin (n * pi * at.driveY);
                all.push_back ({ f, loss.sigma (f), drive * std::sin (m * pi * at.leftX) * std::sin (n * pi * at.leftY), drive * std::sin (m * pi * at.rightX) * std::sin (n * pi * at.rightY) });
            }
            std::sort (all.begin(), all.end(), [] (const M& a, const M& b) { return a.s < b.s; });
            const size_t n = all.size(); cr.resize (n); ci.resize (n); gl.resize (n); gr.resize (n); ll.resize (n); lr.resize (n); sig.resize (n); hzs.resize (n);
            for (size_t i = 0; i < n; ++i)
            {
                const auto& a = all[i]; const double w = 2.0 * pi * a.f, wd = std::sqrt (std::max (0.0, w * w - a.s * a.s)), rad = std::exp (-a.s / fs), lag = wd > 0.0 ? a.s / wd : 0.0;
                cr[i] = (float) (rad * std::cos (wd / fs)); ci[i] = (float) (rad * std::sin (wd / fs)); gl[i] = (float) a.gl; gr[i] = (float) a.gr; ll[i] = (float) (a.gl * lag); lr[i] = (float) (a.gr * lag); sig[i] = a.s; hzs[i] = (float) a.f;
            }
            return (int) n;
        }
        int modes() const { return (int) cr.size(); }
        /** The rms each pickup would give for white noise of rms 1 going in. */
        double level (bool right) const { double s = 0.0; const auto& g = right ? gr : gl; for (size_t i = 0; i < cr.size(); ++i) { const double r2 = (double) cr[i] * cr[i] + (double) ci[i] * ci[i]; s += 0.5 * (double) g[i] * g[i] / std::max (1.0e-9, 1.0 - r2); } return std::sqrt (s); }

        /** The plate's answer to one tap: each pickup's velocity, a sample at a time, until every mode is `floorDb` down
            (or maxSeconds). Each mode is exp(-sigma t) (cos(w t) - (sigma / w) sin(w t)), dropped once it is that far down. */
        void tap (std::vector<float>& left, std::vector<float>& right, double floorDb = 90.0, double maxSeconds = 12.0) const
        {
            const int n = modes(); left.clear(); right.clear(); if (n == 0) return;
            const double nepers = floorDb * 0.11512925464970229; const int length = (int) std::min (maxSeconds * fs, std::ceil (nepers / sig[0] * fs)) + 1;
            left.assign ((size_t) length, 0.0f); right.assign ((size_t) length, 0.0f);
            std::vector<float> re ((size_t) n, 1.0f), im ((size_t) n, 0.0f); int active = n;
            for (int t = 0; t < length; ++t)
            {
                while (active > 0 && sig[(size_t) active - 1] * (double) t / fs > nepers) --active;
                if (active == 0) { left.resize ((size_t) t); right.resize ((size_t) t); break; }
                float aL[8] = {}, aR[8] = {}; int i = 0; float* a = re.data(); float* b = im.data(); const float* p = cr.data(); const float* q = ci.data(); const float* g1 = gl.data(); const float* g2 = gr.data(); const float* l1 = ll.data(); const float* l2 = lr.data();
                for (; i + 8 <= active; i += 8)
                    for (int j = 0; j < 8; ++j)
                    {
                        const float x = a[i + j], y = b[i + j]; aL[j] += g1[i + j] * x - l1[i + j] * y; aR[j] += g2[i + j] * x - l2[i + j] * y;
                        a[i + j] = p[i + j] * x - q[i + j] * y; b[i + j] = p[i + j] * y + q[i + j] * x;
                    }
                for (; i < active; ++i) { const float x = a[i], y = b[i]; aL[0] += g1[i] * x - l1[i] * y; aR[0] += g2[i] * x - l2[i] * y; a[i] = p[i] * x - q[i] * y; b[i] = p[i] * y + q[i] * x; }
                float sl = 0.0f, sr = 0.0f; for (int j = 0; j < 8; ++j) { sl += aL[j]; sr += aR[j]; } left[(size_t) t] = sl; right[(size_t) t] = sr;
            }
        }

        double fs = 48000.0; std::vector<float> cr, ci, gl, gr, ll, lr, hzs; std::vector<double> sig;
    };

    /** The classic big plate and a small one. */
    inline Plate classic() { return {}; }
    inline Plate small() { Plate p; p.lx = 1.0; p.ly = 0.5; return p; }
}
