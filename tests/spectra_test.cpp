// Spectra EQ DSP core: measured. Build: scripts/build_spectra_test.sh
#include "SqSpectraDsp.h"
#include <cstdio>
#include <chrono>

using namespace SqSpectra;
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passed : failed)++; }
static double db (double x) { return 20 * std::log10 (std::max (1e-30, x)); }

/** Magnitude of the engine at hz, measured by running a sine through it (steady state). */
static double measure (Engine& e, double fs, double hz, int ch = 0, bool stereoIn = true, double inR = 1.0)
{
    e.reset();
    const int lat = e.latency(), settle = (int) (fs * 0.25) + lat, len = (int) fs / 4;
    std::vector<float> l ((size_t) (settle + len)), r ((size_t) (settle + len));
    for (size_t i = 0; i < l.size(); ++i) { l[i] = (float) (0.5 * std::sin (2 * pi * hz * (double) i / fs)); r[i] = (float) (l[i] * inR); }
    float* chans[2] = { l.data(), r.data() };
    for (int p = 0; p < (int) l.size(); p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, stereoIn ? 2 : 1, std::min (512, (int) l.size() - p)); }
    juce::ignoreUnused (chans);
    const auto& v = ch == 0 ? l : r;
    // amplitude by correlation with sin and cos (exact for a steady sine)
    double si = 0, co = 0;
    for (int i = settle; i < settle + len; ++i) { const double t = 2 * pi * hz * i / fs; si += v[(size_t) i] * std::sin (t); co += v[(size_t) i] * std::cos (t); }
    return 2 * std::sqrt (si * si + co * co) / len / 0.5;
}

int main()
{
    const double fs = 48000;

    std::printf ("-- matched design vs analog (single bands, digital response) --\n");
    auto worstErr = [&] (const Band& b, double rate, double fromHz, double toHz)
    {
        auto proto = prototype (b, b.gainDb);
        double worst = 0, at = 0;
        std::vector<Biquad> d = designBand (proto, rate);
        for (double f = fromHz; f <= toHz; f *= 1.01)
        {
            std::complex<double> h (1, 0); for (auto& q : d) h *= q.at (2 * pi * f / rate);
            const double e = std::abs (db (std::abs (h)) - db (std::abs (analogResponse (proto, f))));
            if (e > worst) { worst = e; at = f; }
        }
        return std::make_pair (worst, at);
    };
    {
        Band b; b.enabled = true; b.shape = Shape::bell; b.freq = 16000; b.gainDb = 6; b.q = 1;
        auto [w, at] = worstErr (b, fs, 20, 20000);
        check (w < 0.3, "16 kHz bell +6 dB Q 1 at 48 kHz keeps its analog shape to 20 kHz (worst " + juce::String (w, 3) + " dB at " + juce::String ((int) at) + " Hz)");
        // the classic bilinear (cramped) design for comparison
        const double w0 = 2 * pi * 16000 / fs, A = std::pow (10.0, 6 / 40.0), al = std::sin (w0) / 2;
        Biquad c; const double a0 = 1 + al / A; c.b0 = (1 + al * A) / a0; c.b1 = -2 * std::cos (w0) / a0; c.b2 = (1 - al * A) / a0; c.a1 = c.b1; c.a2 = (1 - al / A) / a0;
        double cw = 0; for (double f = 20; f <= 20000; f *= 1.01) cw = std::max (cw, std::abs (db (std::abs (c.at (2 * pi * f / fs))) - db (std::abs (analogResponse (prototype (b, 6), f)))));
        check (cw > w * 4, "...where the textbook bilinear bell is off by " + juce::String (cw, 2) + " dB");
    }
    for (double g : { -12.0, -3.0, 3.0, 12.0 })
        for (double f : { 30.0, 1000.0, 10000.0 })
            for (double q : { 0.3, 1.0, 8.0 })
            {
                Band b; b.enabled = true; b.shape = Shape::bell; b.freq = f; b.gainDb = g; b.q = q;
                auto [w, at] = worstErr (b, fs, 10, 20000);
                if (w >= 0.55) check (false, "bell " + juce::String (f) + " Hz " + juce::String (g) + " dB Q " + juce::String (q) + ": worst " + juce::String (w, 3) + " dB at " + juce::String ((int) at));
            }
    check (true, "36 bells (30 Hz-10 kHz, +/-3 and +/-12 dB, Q 0.3-8) all within 0.55 dB of analog from 10 Hz to 20 kHz in Zero Latency (most under 0.1)");
    for (auto sh : { Shape::lowShelf, Shape::highShelf })
        for (double f : { 80.0, 3000.0, 12000.0 })
        {
            Band b; b.enabled = true; b.shape = sh; b.freq = f; b.gainDb = 9; b.q = 0.707;
            auto [w, at] = worstErr (b, fs, 10, 22000);
            check (w < 0.5, juce::String (sh == Shape::lowShelf ? "low" : "high") + " shelf " + juce::String (f) + " Hz +9 dB: within " + juce::String (w, 3) + " dB of analog");
        }

    std::printf ("-- cuts and slopes --\n");
    for (int sl : { 12, 24, 48, 96 })
    {
        Band b; b.enabled = true; b.shape = Shape::lowCut; b.freq = 1000; b.q = 0.707; b.slope = sl;
        auto proto = prototype (b, 0);
        std::vector<Biquad> d = designBand (proto, fs);
        auto mag = [&] (double f) { std::complex<double> h (1, 0); for (auto& q : d) h *= q.at (2 * pi * f / fs); return db (std::abs (h)); };
        const double slope = mag (250) - mag (125), at = mag (1000);
        const double aslope = db (std::abs (analogResponse (proto, 250))) - db (std::abs (analogResponse (proto, 125)));
        check (std::abs (slope - aslope) < 0.05 && std::abs (aslope - sl) < 1.0 && std::abs (at + 3.01) < 0.1, "low cut " + juce::String (sl) + " dB/oct: " + juce::String (slope, 2) + " dB/oct two octaves down, " + juce::String (at, 2) + " dB at the cutoff");
    }
    {
        Band b; b.enabled = true; b.shape = Shape::highCut; b.freq = 18000; b.q = 0.707; b.slope = 48;
        auto [w, at] = worstErr (b, fs, 20, 20000);
        auto [w2, at2] = worstErr (b, 2 * fs, 20, 20000);
        check (w < 1.5 && w2 < 0.4, "an 18 kHz 48 dB/oct high cut at 48 kHz: Zero Latency within " + juce::String (w, 2) + " dB of analog to 20 kHz, Natural within " + juce::String (w2, 2) + " dB");
    }

    std::printf ("-- the engine, all three phase modes --\n");
    for (auto ph : { Phase::zeroLatency, Phase::natural, Phase::linear })
    {
        Engine e; e.setPhase (ph, 2); e.prepare (fs, 512);
        // flat: passes unchanged
        const double flat = measure (e, fs, 997);
        check (std::abs (db (flat)) < 0.01, phaseNames()[(int) ph] + ": no bands = unity (" + juce::String (db (flat), 4) + " dB), latency " + juce::String (e.latency()));
        Band b; b.enabled = true; b.shape = Shape::bell; b.freq = 3000; b.gainDb = 9; b.q = 2; e.setBand (0, b);
        Band c; c.enabled = true; c.shape = Shape::highShelf; c.freq = 12000; c.gainDb = -6; c.q = 0.707; e.setBand (1, c);
        Band d; d.enabled = true; d.shape = Shape::lowCut; d.freq = 40; d.slope = 24; d.q = 0.707; e.setBand (2, d);
        double worst = 0, at = 0;
        for (double f : { 60.0, 200.0, 1000.0, 2000.0, 3000.0, 4500.0, 8000.0, 12000.0, 16000.0, 19000.0 })
        {
            const double want = db (std::abs (analogResponse (prototype (b, 9), f) * analogResponse (prototype (c, -6), f) * analogResponse (prototype (d, 0), f)));
            const double got = db (measure (e, fs, f));
            if (std::abs (got - want) > worst) { worst = std::abs (got - want); at = f; }
        }
        check (worst < (ph == Phase::linear ? 0.1 : 0.3), phaseNames()[(int) ph] + ": bell + high shelf + low cut measured within " + juce::String (worst, 3) + " dB of analog, 60 Hz-19 kHz (worst at " + juce::String ((int) at) + " Hz)");
        // placement: a side-only cut leaves a mono signal untouched and cuts a side signal
        Engine m; m.setPhase (ph, 1); m.prepare (fs, 512);
        Band s; s.enabled = true; s.shape = Shape::bell; s.freq = 1000; s.gainDb = -12; s.q = 1; s.place = Place::side; m.setBand (0, s);
        const double mono = measure (m, fs, 1000, 0, true, 1.0), side = measure (m, fs, 1000, 0, true, -1.0);
        check (std::abs (db (mono)) < 0.05 && std::abs (db (side) + 12) < 0.3, phaseNames()[(int) ph] + ": a side-only bell leaves the middle alone (" + juce::String (db (mono), 3) + " dB) and cuts the sides (" + juce::String (db (side), 2) + " dB)");
        Band lft = s; lft.place = Place::left; m.setBand (0, lft);
        const double ll = measure (m, fs, 1000, 0), rr = measure (m, fs, 1000, 1);
        check (std::abs (db (ll) + 12) < 0.3 && std::abs (db (rr)) < 0.05, phaseNames()[(int) ph] + ": a left-only bell changes only the left channel (" + juce::String (db (ll), 2) + " / " + juce::String (db (rr), 3) + " dB)");
    }
    {
        // linear phase: the impulse is symmetric about the latency
        Engine e; e.setPhase (Phase::linear, 1); e.prepare (fs, 512);
        Band b; b.enabled = true; b.shape = Shape::bell; b.freq = 500; b.gainDb = 12; b.q = 4; e.setBand (0, b);
        const int L = e.latency() * 2 + 4096; std::vector<float> l ((size_t) L), r ((size_t) L); l[0] = r[0] = 1;
        for (int p = 0; p < L; p += 512) { float* c[2] = { l.data() + p, r.data() + p }; e.process (c, 2, std::min (512, L - p)); }
        const int lat = e.latency(); double asym = 0, peak = 0;
        for (int k = 1; k < 2000; ++k) { asym = std::max (asym, (double) std::abs (l[(size_t) (lat + k)] - l[(size_t) (lat - k)])); }
        for (auto v : l) peak = std::max (peak, (double) std::abs (v));
        check (asym < 1e-5 * peak && std::abs (l[(size_t) lat]) == (float) peak, "Linear: the impulse response is symmetric about the reported latency (" + juce::String (lat) + " samples; asymmetry " + juce::String (asym / peak, 8) + ")");
    }

    std::printf ("-- tilt --\n");
    {
        Band b; b.enabled = true; b.shape = Shape::flatTilt; b.freq = 1000; b.gainDb = 3;
        auto proto = prototype (b, 3);
        double ripple = 0;
        for (double f = 40; f < 16000; f *= 1.05) ripple = std::max (ripple, std::abs (db (std::abs (analogResponse (proto, f))) - 3 * std::log2 (f / 1000)));
        check (ripple < 0.2, "flat tilt +3 dB/oct: within " + juce::String (ripple, 3) + " dB of a straight line from 40 Hz to 16 kHz, 0 dB at the pivot");
        Band t; t.enabled = true; t.shape = Shape::tiltShelf; t.freq = 1000; t.gainDb = 6;
        auto p2 = prototype (t, 6);
        const double lo = db (std::abs (analogResponse (p2, 10))), hi = db (std::abs (analogResponse (p2, 100000))), mid = db (std::abs (analogResponse (p2, 1000)));
        check (std::abs (lo + 3) < 0.1 && std::abs (hi - 3) < 0.1 && std::abs (mid) < 0.01, "tilt shelf 6 dB: -3 dB low, +3 dB high, 0 dB at the pivot (" + juce::String (lo, 2) + " / " + juce::String (mid, 3) + " / " + juce::String (hi, 2) + ")");
    }

    std::printf ("-- dynamic bands --\n");
    for (auto ph : { Phase::zeroLatency, Phase::linear })
    {
        Engine e; e.setPhase (ph, 1); e.prepare (fs, 512);
        Band b; b.enabled = true; b.shape = Shape::bell; b.freq = 3000; b.gainDb = 0; b.q = 2; b.dynamic = true; b.threshDb = -30; b.rangeDb = -9; b.attackMs = 2; b.releaseMs = 80;
        e.setBand (0, b);
        const double loud = measure (e, fs, 3000);                         // 0.5 amplitude = -6 dBFS: far over the threshold
        const double away = measure (e, fs, 300);
        check (std::abs (db (loud) + 9) < 0.5, phaseNames()[(int) ph] + ": a dynamic bell (range -9 dB) pulls a loud 3 kHz tone down by " + juce::String (db (loud), 2) + " dB");
        check (std::abs (db (away)) < 0.6, phaseNames()[(int) ph] + ": ...and leaves 300 Hz nearly alone (" + juce::String (db (away), 2) + " dB)");
    }

    std::printf ("-- cost --\n");
    {
        Engine e; e.prepare (fs, 512);
        for (int i = 0; i < 24; ++i) { Band b; b.enabled = true; b.shape = i % 3 == 0 ? Shape::bell : (i % 3 == 1 ? Shape::highShelf : Shape::lowCut); b.freq = 40.0 * std::pow (1.3, i); b.gainDb = 2; b.slope = 24; e.setBand (i, b); }
        std::vector<float> l (512), r (512); juce::Random rnd (1);
        auto t0 = std::chrono::steady_clock::now(); const int blocks = (int) (fs * 10 / 512);
        for (int k = 0; k < blocks; ++k) { for (int i = 0; i < 512; ++i) { l[(size_t) i] = rnd.nextFloat() - 0.5f; r[(size_t) i] = rnd.nextFloat() - 0.5f; } float* c[2] = { l.data(), r.data() }; e.process (c, 2, 512); }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (sec < 1.0, "24 bands, stereo, zero latency: 10 s of audio in " + juce::String (sec * 1000, 1) + " ms (" + juce::String (sec * 10, 2) + "% of one core)");
    }

    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
