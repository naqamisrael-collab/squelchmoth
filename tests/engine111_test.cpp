// Measures the 1.11 shared engines in isolation. Build and run:
//   g++ -O2 -std=c++17 -I../Source engine111_test.cpp -o /tmp/e111 && /tmp/e111
#include "SqEngine111Dsp.h"
#include <complex>
#include <cstdio>
#include <string>

using namespace Eng;
static int fails = 0, passes = 0;
static void check (bool ok, const std::string& what) { (ok ? passes : fails)++; std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.c_str()); }
static std::string f2 (double v, int d = 2) { char b[64]; std::snprintf (b, sizeof b, "%.*f", d, v); return b; }

// power spectrum (Blackman-Harris window), radix-2
static std::vector<double> spectrum (const std::vector<float>& x, size_t n)
{
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.35875 - 0.48829 * std::cos (2 * pi * i / (n - 1)) + 0.14128 * std::cos (4 * pi * i / (n - 1)) - 0.01168 * std::cos (6 * pi * i / (n - 1));
        a[i] = x[i] * w;
    }
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * pi / len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
    }
    std::vector<double> p (n / 2); for (size_t i = 0; i < n / 2; ++i) p[i] = std::norm (a[i]); return p;
}
static double rms (const float* x, size_t n) { double e = 0; for (size_t i = 0; i < n; ++i) e += (double) x[i] * x[i]; return std::sqrt (e / std::max<size_t> (1, n)); }

int main()
{
    const double sr = 48000;
    setvbuf (stdout, nullptr, _IONBF, 0);
    std::printf ("== 1.11 engines ==\n");

    // ---------------------------------------------------------------- half-band design
    {
        auto c0 = HalfBandDesign::design (110.0, 0.0425);
        check (c0.size() >= 6 && c0.size() <= 16, "half-band stage 0 has " + std::to_string (c0.size()) + " coefficients");
        bool inRange = true; for (double c : c0) inRange = inRange && c > 0 && c < 1;
        check (inRange, "every allpass coefficient lies in (0, 1) - stable");
        // impulse response of one 2x upsampler -> frequency response at the doubled rate
        HalfBand hb; hb.setCoefs (c0);
        const size_t n = 1 << 15; std::vector<float> h (n, 0.0f);
        for (size_t i = 0; i < n / 2; ++i) { float a, b; hb.up (i == 0 ? 1.0f : 0.0f, a, b); h[2 * i] = a; h[2 * i + 1] = b; }
        std::vector<std::complex<double>> H; // direct DFT at chosen frequencies
        auto mag = [&] (double hz) { std::complex<double> s = 0; for (size_t i = 0; i < n; ++i) s += (double) h[i] * std::polar (1.0, -2 * pi * hz * (double) i / (2 * sr)); return 20 * std::log10 (std::abs (s) / 2.0 + 1e-30); };
        const double m1k = mag (1000), m20k = mag (20000), mImage = mag (2 * sr / 2 - 20000), mImage2 = mag (sr - 1000);
        check (std::abs (m1k) < 0.01 && std::abs (m20k) < 0.1, "pass band flat: 1 kHz " + f2 (m1k, 4) + " dB, 20 kHz " + f2 (m20k, 4) + " dB");
        check (mImage < -100 && mImage2 < -100, "image rejection: 28 kHz image " + f2 (mImage, 1) + " dB, 47 kHz image " + f2 (mImage2, 1) + " dB");
    }

    // ---------------------------------------------------------------- oversampler round trip
    for (int st = 1; st <= 4; ++st)
    {
        Oversampler os; os.prepare (256, st, sr);
        std::vector<float> in (48128), out (48128);
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * (float) std::sin (2 * pi * 1000.0 * i / sr);
        for (size_t p = 0; p < in.size(); p += 256) { float* o = os.up (0, in.data() + p, 256); os.down (0, o, 256, out.data() + p); }
        const double g = gainToDb ((float) (rms (out.data() + 24000, 24000) / rms (in.data() + 24000, 24000)));
        // latency by cross-correlation at sub-sample resolution
        double best = -1e9, bestLag = 0;
        for (double lag = 0; lag < 40; lag += 0.01)
        {
            double s = 0; for (size_t i = 30000; i < 34000; ++i) s += out[i] * 0.5 * std::sin (2 * pi * 1000.0 * (i - lag) / sr);
            if (s > best) { best = s; bestLag = lag; }
        }
        check (std::abs (g) < 0.01, std::to_string (1 << st) + "x round trip at 1 kHz: " + f2 (g, 4) + " dB");
        check (std::abs (bestLag - os.latencySamples()) < 0.25, std::to_string (1 << st) + "x latency reported " + f2 (os.latencySamples(), 2) + " samples, measured " + f2 (bestLag, 2));
    }

    // linear-phase option: flat, unity, latency known, image rejection
    for (int st = 1; st <= 4; ++st)
    {
        Oversampler os; os.prepare (256, st, sr, Oversampler::linearPhase);
        std::vector<float> in (48128), out (48128);
        auto run = [&] (double hz) { os.reset(); for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * (float) std::sin (2 * pi * hz * i / sr);
                                     for (size_t p = 0; p + 256 <= in.size(); p += 256) { float* o = os.up (0, in.data() + p, 256); os.down (0, o, 256, out.data() + p); }
                                     return gainToDb ((float) (rms (out.data() + 24000, 24000) / rms (in.data() + 24000, 24000))); };
        const double g1 = run (1000), g20 = run (20000);
        // linear phase: same delay at 100 Hz and 15 kHz
        auto delayAt = [&] (double hz) { os.reset(); const double w = 2 * pi * hz / sr; double S = 0, C = 0;
                                         for (size_t i = 0; i < in.size(); ++i) in[i] = (float) std::sin (w * i);
                                         for (size_t p = 0; p + 256 <= in.size(); p += 256) { float* o = os.up (0, in.data() + p, 256); os.down (0, o, 256, out.data() + p); }
                                         for (size_t i = 24000; i < 48000; ++i) { S += out[i] * std::sin (w * i); C += out[i] * std::cos (w * i); }
                                         double d = std::atan2 (-C, S) / w; const double per = 2 * pi / w; while (d < os.latencySamples() - per / 2) d += per; while (d > os.latencySamples() + per / 2) d -= per; return d; };
        const double d100 = delayAt (100), d15k = delayAt (15000);
        check (std::abs (g1) < 0.01 && std::abs (g20) < 0.1 && std::abs (d100 - d15k) < 0.02 && std::abs (d100 - os.latencySamples()) < 0.05,
               std::to_string (1 << st) + "x linear phase: 1 kHz " + f2 (g1, 4) + " dB, 20 kHz " + f2 (g20, 3) + " dB, delay " + f2 (d100, 2) + " at 100 Hz and " + f2 (d15k, 2) + " at 15 kHz (" + std::to_string (os.firTaps (0)) + " taps, reported " + f2 (os.latencySamples(), 3) + ")");
    }
    // factor switching on the fly keeps the latency table
    {
        Oversampler os; os.prepare (256, 2, sr); const double l4 = os.latencySamples(); os.setFactorLog2 (4, 0); const double l16 = os.latencySamples(); os.setFactorLog2 (2, 0);
        check (os.latencySamples() == l4 && l16 > l4 && os.factor() == 4, "factor switch without allocation: 4x " + f2 (l4) + ", 16x " + f2 (l16) + " samples");
    }

    // ---------------------------------------------------------------- the ten models: unity, safety, harmonics, aliasing
    for (int m = 0; m < Nonlinear::numModels; ++m)
    {
        Nonlinear::Stage s; s.model = m; s.prepare (sr * 8); s.drive = 1; s.bias = 0.2f; s.character = m == Nonlinear::germanium ? 0.0f : 0.5f;   // Germ's gate is measured below
        // small-signal gain (-50 dBFS 1 kHz at the 8x rate)
        std::vector<float> x (sr * 8 / 2), y (x.size());
        for (size_t i = 0; i < x.size(); ++i) x[i] = dbToGain (-50) * (float) std::sin (2 * pi * 1000.0 * i / (sr * 8));
        for (size_t i = 0; i < x.size(); ++i) y[i] = s.process (x[i]);
        const double g = gainToDb ((float) (rms (y.data() + y.size() / 2, y.size() / 2) / rms (x.data() + x.size() / 2, x.size() / 2)));
        // hot: +24 dB drive into a 0 dBFS sine, check finite and bounded, DC removed
        s.reset(); s.drive = dbToGain (24); bool finite = true; double dc = 0, pk = 0;
        for (size_t i = 0; i < x.size(); ++i) { const float v = s.process (std::sin (2 * pi * 100.0 * i / (sr * 8))); finite = finite && std::isfinite (v); if (i > x.size() / 2) { dc += v; pk = std::max (pk, (double) std::abs (v)); } }
        dc /= (x.size() / 2);
        check (std::abs (g) < (m == Nonlinear::transformer || m == Nonlinear::fold || m == Nonlinear::rectify ? 1.0 : 0.25) && finite && pk < 4.0 && std::abs (dc) < 0.01,
               std::string ("model ") + Nonlinear::modelName (m) + ": small-signal " + f2 (g, 2) + " dB, +24 dB drive peak " + f2 (pk, 2) + ", DC " + f2 (dc, 4));
    }

    // Germ's gate: quiet tails sputter by design (character 0.5 gates a -50 dBFS signal)
    {
        Nonlinear::Stage s; s.model = Nonlinear::germanium; s.prepare (sr * 8); s.character = 0.5f; double e = 0;
        for (int i = 0; i < (int) sr * 4; ++i) { const float v = s.process (dbToGain (-50) * (float) std::sin (2 * pi * 1000.0 * i / (sr * 8))); if (i > sr * 2) e += v * v; }
        check (gainToDb ((float) std::sqrt (e / (sr * 2))) < -60.0f, "Germ: character gates a -50 dBFS tail (starved bias)");
    }
    // aliasing: a 7 kHz sine driven 24 dB, energy outside the harmonic bins below 20 kHz
    {
        auto aliasDb = [&] (int stages, int model)
        {
            Oversampler os; os.prepare (512, stages, sr); Nonlinear::Stage s; s.model = model; s.prepare (sr * (1 << stages)); s.drive = dbToGain (24); s.character = 0.0f; s.bias = 0.0f;
            const size_t n = 1 << 16; std::vector<float> in (n + 8192), out (n + 8192);
            for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * (float) std::sin (2 * pi * 7031.25 * i / sr);   // bin-centred
            for (size_t p = 0; p + 512 <= in.size(); p += 512) { float* o = os.up (0, in.data() + p, 512); for (int i = 0; i < 512 << stages; ++i) o[i] = s.process (o[i]); os.down (0, o, 512, out.data() + p); }
            std::vector<float> seg (out.begin() + 8192, out.begin() + 8192 + n);
            auto P = spectrum (seg, n);
            const double binHz = sr / n; double harm = 0, alias = 0;
            for (size_t k = 2; k < P.size(); ++k)
            {
                const double f = k * binHz; const double r = f / 7031.25; const bool isHarm = std::abs (r - std::round (r)) * 7031.25 < 6 * binHz;
                if (f > 20000) continue;
                (isHarm ? harm : alias) += P[k];
            }
            return 10 * std::log10 (alias / harm);
        };
        const double b1 = aliasDb (0, Nonlinear::brick), b16 = aliasDb (4, Nonlinear::brick), g1 = aliasDb (0, Nonlinear::glow), g8 = aliasDb (3, Nonlinear::glow), g16 = aliasDb (4, Nonlinear::glow);
        check (b16 < -75 && b16 < b1 - 50, "aliasing, hard clip (Brick, ADAA), 7 kHz +24 dB: 1x " + f2 (b1, 1) + " dB, 16x " + f2 (b16, 1) + " dB below the harmonics");
        check (g8 < -75 && g16 < -100, "aliasing, Glow, 7 kHz +24 dB: 1x " + f2 (g1, 1) + " dB, 8x " + f2 (g8, 1) + " dB, 16x " + f2 (g16, 1) + " dB");
    }

    // ---------------------------------------------------------------- Analog Style: every voicing stays within +-1.5 dB on pink noise
    {
        std::vector<float> l, r; TestSignals::pink (l, r, sr, 4.0, -18.0f);
        for (int v = 0; v < AnalogStyle::numVoicings; ++v)
        {
            AnalogStyle as; as.prepare (sr, 256, 2); as.setVoicing (v); as.setAmount (1.0f);
            std::vector<float> L = l, R = r;
            for (size_t p = 0; p + 256 <= L.size(); p += 256) { float* ch[2] = { L.data() + p, R.data() + p }; as.process (ch, 2, 256); }
            const double d = gainToDb ((float) (rms (L.data() + 96000, 96000) / rms (l.data() + 96000, 96000)));
            bool finite = true; for (float x : L) finite = finite && std::isfinite (x);
            check (finite && std::abs (d) < 1.5, std::string ("Analog Style ") + AnalogStyle::voicingName (v) + ": level change " + f2 (d, 2) + " dB on pink noise at -18 dBFS");
        }
    }

    // ---------------------------------------------------------------- Wing Mod
    {
        WingMod m; m.prepare (sr); m.s.mode = WingMod::lfo; m.s.shape = WingMod::sine; m.s.rateHz = 2.0;
        int crossings = 0; float prev = 0;
        for (int i = 0; i < (int) sr * 10; ++i) { float l, r; m.next (0, l, r); if (prev < 0 && l >= 0) ++crossings; prev = l; }
        check (crossings == 20 || crossings == 19, "LFO free 2 Hz: " + std::to_string (crossings) + " cycles in 10 s");

        // synced, playing at 120 BPM: 1/4 = 0.5 s period, value at ppq 1.0 is sin(0) = 0 and at ppq 1.25 is +1
        m.reset(); m.s.sync = true; m.s.syncIndex = 12; m.setTransport (120, 1.0, true);
        float l0, r0; m.next (0, l0, r0);
        for (int i = 1; i < (int) (sr * 0.125); ++i) { float a, b; m.next (0, a, b); }
        float lq, rq; m.next (0, lq, rq);
        check (std::abs (l0) < 1e-3 && std::abs (lq - 1.0f) < 1e-3, "LFO synced to the transport: " + f2 (l0, 4) + " on the beat, " + f2 (lq, 4) + " a sixteenth later");

        // stereo phase 90: R leads by a quarter cycle
        m.reset(); m.s.sync = false; m.s.rateHz = 1; m.s.stereoPhaseDeg = 90; float L, R; m.next (0, L, R);
        check (std::abs (L) < 1e-3 && std::abs (R - 1.0f) < 1e-3, "stereo phase 90 degrees: L " + f2 (L, 3) + ", R " + f2 (R, 3));

        // envelope follower: 10 ms attack reaches 90 % of the dB range within ~3 time constants; release follows
        WingMod e; e.prepare (sr); e.s.mode = WingMod::envelope; e.s.attackMs = 10; e.s.releaseMs = 150; e.s.bipolar = false;
        int t90 = -1; for (int i = 0; i < (int) sr; ++i) { float a, b; e.next (i < sr / 2 ? 1.0f : 0.0f, a, b); if (t90 < 0 && a > 0.95f) t90 = i; }
        check (t90 > 0 && t90 < (int) (0.04 * sr), "envelope follower: 10 ms attack is at 95 % after " + f2 (t90 / sr * 1000.0, 1) + " ms");

        // step mode advances on audio hits above the threshold, not on the quieter ones
        WingMod st; st.prepare (sr); st.s.mode = WingMod::step; st.s.bipolar = false; st.s.thresholdDb = -20;
        for (int k = 0; k < 16; ++k) st.s.steps[k] = (float) k / 15.0f;
        for (int hit = 0; hit < 5; ++hit) for (int i = 0; i < (int) (sr * 0.2); ++i) { float a, b; const bool loud = hit != 2; st.next (i < 200 ? (loud ? 0.5f : 0.02f) : 0.0f, a, b); }
        check (st.currentStep() == 3, "step mode: 4 loud hits advance 4 steps and the quiet one is ignored (step " + std::to_string (st.currentStep()) + ")");

        // ADSR from a manual trigger
        WingMod ad; ad.prepare (sr); ad.s.mode = WingMod::adsr; ad.s.bipolar = false; ad.s.aMs = 10; ad.s.dMs = 50; ad.s.sLevel = 0.5f; ad.s.rMs = 100;
        ad.trigger(); int tPeak = -1; float a = 0, b;
        for (int i = 0; i < (int) (sr * 0.5); ++i) { ad.next (0, a, b); if (tPeak < 0 && a >= 0.999f) tPeak = i; }
        const float sus = a; ad.release(); for (int i = 0; i < (int) (sr * 0.5); ++i) ad.next (0, a, b);
        check (tPeak > 0 && tPeak < (int) (0.02 * sr) && std::abs (sus - 0.5f) < 0.01f && a < 0.01f, "ADSR: peak at " + f2 (tPeak / sr * 1000, 1) + " ms, sustain " + f2 (sus, 3) + ", released to " + f2 (a, 4));

        // rhythm: 16 steps of 1/16 at 120 BPM over one bar, step levels follow the pattern; swing moves step 2
        WingMod rh; rh.prepare (sr); rh.s.mode = WingMod::rhythm; rh.s.bipolar = false; rh.s.stepGate = 1.0f;
        rh.setTransport (120, 0.0, true); std::vector<float> lv (sr * 2); for (size_t i = 0; i < lv.size(); ++i) { float x, y; rh.next (0, x, y); lv[i] = x; }
        const size_t s16 = (size_t) (sr * 0.125);
        check (std::abs (lv[s16 / 2] - 1.0f) < 0.01f && lv[s16 + s16 / 2] < 0.01f && std::abs (lv[2 * s16 + s16 / 2] - 0.6f) < 0.01f, "rhythm pattern lands on the sixteenths (" + f2 (lv[s16 / 2]) + ", " + f2 (lv[s16 + s16 / 2]) + ", " + f2 (lv[2 * s16 + s16 / 2]) + ")");
        WingMod sw = rh; sw.reset(); sw.s.swing = 0.5f; for (int k = 0; k < 16; ++k) sw.s.steps[k] = (k % 2) ? 1.0f : 0.0f; sw.setTransport (120, 0.0, true);
        int firstOn = -1; for (int i = 0; i < (int) (sr * 0.5); ++i) { float x, y; sw.next (0, x, y); if (firstOn < 0 && x > 0.5f) firstOn = i; }
        check (std::abs (firstOn / sr - 0.125 * 1.25) < 0.003, "swing 50 %: the off-beat sixteenth starts at " + f2 (firstOn / sr * 1000, 1) + " ms (straight: 125 ms)");
    }

    // ---------------------------------------------------------------- test signals
    {
        std::vector<float> l, r;
        TestSignals::pink (l, r, sr, 2.0); check (std::abs (gainToDb ((float) rms (l.data(), l.size())) + 18.0f) < 0.1f, "pink noise at -18 dBFS RMS");
        TestSignals::drums (l, r, sr, 4.0); float pk = 0; for (float x : l) pk = std::max (pk, std::abs (x)); check (std::abs (gainToDb (pk) + 6.0f) < 0.1f, "drum loop peaks at -6 dBFS");
        TestSignals::vocal (l, r, sr, 4.0); check (std::abs (gainToDb ((float) rms (l.data(), l.size())) + 20.0f) < 0.1f, "sung vocal at -20 dBFS RMS");
    }

    std::printf ("\n== %d passed, %d failed ==\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
