// Cinder measurements. g++ -O2 -std=c++17 -I../Source cinder_test.cpp -o /tmp/cin && /tmp/cin
#include "SqCinderDsp.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <string>
using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const std::string& w) { (ok ? passes : fails)++; std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", w.c_str()); }
static std::string f2 (double v, int d = 2) { char b[64]; std::snprintf (b, sizeof b, "%.*f", d, v); return b; }
static std::vector<double> spectrum (const std::vector<float>& x, size_t n)
{
    std::vector<std::complex<double>> a (n); double wsum = 0;
    for (size_t i = 0; i < n; ++i) { const double w = 0.35875 - 0.48829 * std::cos (2 * pi * i / (n - 1)) + 0.14128 * std::cos (4 * pi * i / (n - 1)) - 0.01168 * std::cos (6 * pi * i / (n - 1)); a[i] = x[i] * w; wsum += w; }
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { const double ang = -2 * pi / len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } } }
    std::vector<double> p (n / 2); for (size_t i = 0; i < n / 2; ++i) p[i] = std::norm (a[i]) * 4.0 / (wsum * wsum); return p;   // sine amplitude^2 at its bin
}
static double rms (const float* x, size_t n) { double e = 0; for (size_t i = 0; i < n; ++i) e += (double) x[i] * x[i]; return std::sqrt (e / std::max<size_t> (1, n)); }
static void run (Cinder::Processor& c, std::vector<float>& l, std::vector<float>& r, int block = 256)
{ for (size_t p = 0; p + block <= l.size(); p += block) { float* ch[2] = { l.data() + p, r.data() + p }; c.process (ch, 2, nullptr, block); } }

int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    const double sr = 48000;
    std::printf ("== Cinder ==\n");
    // unity: quiet 1 kHz at minimum drive
    for (int m = 0; m < Cinder::numModels; ++m)
    {
        Cinder::Processor c; c.p.modelA = m; c.p.driveDb = -12; c.p.autoGain = false; c.p.dynamics = 0; c.prepare (sr, 256);
        std::vector<float> l (48000), r; for (size_t i = 0; i < l.size(); ++i) l[i] = dbToGain (-40) * (float) std::sin (2 * pi * 1000 * i / sr); r = l; auto in = l;
        run (c, l, r);
        const double d = gainToDb ((float) (rms (l.data() + 24000, 23000) / rms (in.data() + 24000, 23000)));
        float h[10]; const double thd6 = c.harmonics (-18, h);
        Cinder::Processor c2; c2.p = c.p; c2.p.driveDb = 18; c2.prepare (sr, 256); float h2[10]; const double thd18 = c2.harmonics (-18, h2);
        check (std::abs (d) < 0.5 && thd18 > thd6, std::string (Cinder::modelName (m)) + ": quiet unity " + f2 (d) + " dB; THD at -18 dBFS: drive -12 " + f2 (thd6, 3) + " %, drive +18 " + f2 (thd18, 1)
               + " % (H2 " + f2 (h2[1], 1) + ", H3 " + f2 (h2[2], 1) + ", H4 " + f2 (h2[3], 1) + ", H5 " + f2 (h2[4], 1) + " dB)");
    }
    // character: triode is even-dominant, pentode odd-dominant at +6 drive
    {
        Cinder::Processor t; t.p.modelA = Cinder::triodeBox; t.p.driveDb = 6; t.prepare (sr, 256); float ht[10]; t.harmonics (-18, ht);
        Cinder::Processor q; q.p.modelA = Cinder::pentodeBox; q.p.driveDb = 6; q.prepare (sr, 256); float hq[10]; q.harmonics (-18, hq);
        check (ht[1] > ht[2] && hq[2] > hq[1], "Triode Box: H2 " + f2 (ht[1], 1) + " > H3 " + f2 (ht[2], 1) + " dB; Pentode Box: H3 " + f2 (hq[2], 1) + " > H2 " + f2 (hq[1], 1) + " dB (drive +6, -18 dBFS)");
    }
    // measured advantage: 10 kHz at +24 drive, 44.1 kHz, 8x: aliasing products below -100 dBFS (every model)
    {
        const double rate = 44100; double worst = -999; std::string worstName;
        for (int m = 0; m < Cinder::numModels; ++m)
        {
            Cinder::Processor c; c.p.modelA = m; c.p.driveDb = 24; c.p.oversampleLog2 = 3; c.p.dynamics = 0; c.prepare (rate, 512);
            const size_t n = 1 << 16; const double f0 = rate * 14861.0 / n;   // ~10 kHz on a bin
            std::vector<float> l (n + 16384), r;
            for (size_t i = 0; i < l.size(); ++i) l[i] = dbToGain (-18) * 1.414f * (float) std::sin (2 * pi * f0 * i / rate);
            r = l; run (c, l, r, 512);
            std::vector<float> seg (l.begin() + 16384, l.begin() + 16384 + n);
            auto P = spectrum (seg, n); double worstBin = 0;
            for (size_t k = 3; k < P.size(); ++k)
            {
                const double f = k * rate / n; const double rr = f / f0;
                if (std::abs (rr - std::round (rr)) * f0 < 8 * rate / n) continue;     // a harmonic
                worstBin = std::max (worstBin, P[k]);
            }
            const double db = 10 * std::log10 (worstBin + 1e-30);
            std::printf ("      %s: worst alias %.1f dBFS\n", Cinder::modelName (m), db);
            if (db > worst) { worst = db; worstName = Cinder::modelName (m); }
        }
        check (worst < -100, "aliasing, 10 kHz at +24 dB drive, 44.1 kHz, 8x: worst product " + f2 (worst, 1) + " dBFS (" + worstName + ")");
    }
    // auto gain holds pink noise within 1 dB at +24 drive
    {
        std::vector<float> l, r; TestSignals::pink (l, r, sr, 6.0); auto in = l;
        for (int m : { (int) Cinder::triodeBox, (int) Cinder::diodeFuzz, (int) Cinder::tapeHysteresis })
        {
            Cinder::Processor c; c.p.modelA = m; c.p.driveDb = 24; c.prepare (sr, 256); auto L = l, R = r; run (c, L, R);
            const double d = gainToDb ((float) (rms (L.data() + 4 * 48000, 2 * 48000) / rms (in.data() + 4 * 48000, 2 * 48000)));
            check (std::abs (d) < 1.0, std::string ("auto gain, ") + Cinder::modelName (m) + " at +24 dB on pink noise: " + f2 (d) + " dB");
        }
    }
    // delta at minimum drive on a quiet signal is (nearly) silent; at high drive it carries the distortion
    {
        std::vector<float> l (48000), r; for (size_t i = 0; i < l.size(); ++i) l[i] = dbToGain (-30) * (float) std::sin (2 * pi * 440 * i / sr); r = l; auto in = l;
        Cinder::Processor c; c.p.driveDb = -12; c.p.delta = true; c.p.dynamics = 0; c.prepare (sr, 256); auto L = l, R = r; run (c, L, R);
        const double q = gainToDb ((float) (rms (L.data() + 24000, 23000) / rms (in.data() + 24000, 23000)));
        Cinder::Processor d; d.p.driveDb = 30; d.p.delta = true; d.prepare (sr, 256); L = l; R = r; run (d, L, R);
        const double hot = gainToDb ((float) (rms (L.data() + 24000, 23000) / rms (in.data() + 24000, 23000)));
        check (q < -40 && hot > -20, "delta: " + f2 (q, 1) + " dB at minimum drive, " + f2 (hot, 1) + " dB at +30 dB drive (relative to the input)");
    }
    // mix 0 = the dry signal, delayed exactly by the reported latency (no comb with the wet path)
    {
        std::vector<float> l (48000), r; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.25f * (float) std::sin (2 * pi * 1000 * i / sr); r = l;
        Cinder::Processor c; c.p.mix = 0; c.prepare (sr, 256); run (c, l, r);
        const double w = 2 * pi * 1000 / sr; double S = 0, C = 0; for (size_t i = 24000; i < 48000; ++i) { S += l[i] * std::sin (w * i); C += l[i] * std::cos (w * i); }
        double dl = std::atan2 (-C, S) / w; if (dl < 0) dl += 48;
        const double g = gainToDb ((float) (2 * std::sqrt (S * S + C * C) / 24000 / 0.25));
        check (std::abs (g) < 0.01 && std::abs (dl - c.latencySamples()) < 0.05, "mix 0: unity (" + f2 (g, 4) + " dB) and delayed " + f2 (dl, 3) + " samples = the reported " + f2 (c.latencySamples(), 3));
    }
    // mid only: a pure side signal passes untouched (apart from the shared delay)
    {
        std::vector<float> l (48000), r (48000); for (size_t i = 0; i < l.size(); ++i) { l[i] = 0.3f * (float) std::sin (2 * pi * 200 * i / sr); r[i] = -l[i]; }
        auto in = l; Cinder::Processor c; c.p.ms = Cinder::midOnly; c.p.driveDb = 30; c.p.autoGain = false; c.prepare (sr, 256); run (c, l, r);
        float h[10]; (void) h;
        const double g = gainToDb ((float) (rms (l.data() + 24000, 23000) / rms (in.data() + 24000, 23000)));
        check (std::abs (g) < 0.1, "Mid only at +30 dB drive: a pure side signal passes at " + f2 (g, 3) + " dB");
    }
    // CPU per instance at 48 kHz, 128-sample blocks
    for (int f : { 0, 2, 4 })
    {
        Cinder::Processor c; c.p.oversampleLog2 = f; c.p.driveDb = 12; c.prepare (sr, 128);
        std::vector<float> l, r; TestSignals::drums (l, r, sr, 10.0);
        auto t0 = std::chrono::steady_clock::now(); run (c, l, r, 128); auto t1 = std::chrono::steady_clock::now();
        const double pct = std::chrono::duration<double> (t1 - t0).count() / 10.0 * 100.0;
        check (f > 0 || pct < 1.5, "CPU at " + std::to_string (1 << f) + "x: " + f2 (pct, 2) + " % of one core");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passes, fails);
    return fails ? 1 : 0;
}
