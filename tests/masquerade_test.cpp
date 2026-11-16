// Measures Masquerade (Source/SqMasqueradeDsp.h). Needs JUCE core / audio basics (Moth Tune's engine):
//   see the build line in scripts/build_masquerade_test.sh
#include "JuceHeader.h"
#include "SqMasqueradeDsp.h"
#include <cstdio>
static int fails = 0, passes = 0;
static void check (bool ok, const char* what, double v = 0) { std::printf ("%s  %-78s %10.3f\n", ok ? "PASS" : "FAIL", what, v); ok ? ++passes : ++fails; }
static const double SR = 48000;
static std::vector<float> voice (std::function<double (double)> hzAt, double secs, float amp = 0.2f)
{
    // a voiced, vowel-like tone: 12 harmonics with a formant hump near 700 Hz and 1.2 kHz
    std::vector<float> v ((size_t) (SR * secs)); double ph = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double f = hzAt (i / SR); ph += f / SR; ph -= std::floor (ph); double y = 0;
        for (int h = 1; h <= 12; ++h) { const double hf = f * h; const double g = 1.0 / h * (1.0 + 2.5 * std::exp (-std::pow ((hf - 700) / 250, 2)) + 1.5 * std::exp (-std::pow ((hf - 1200) / 300, 2))); y += g * std::sin (2 * juce::MathConstants<double>::pi * h * ph); }
        v[i] = amp * (float) y * 0.5f;
    }
    return v;
}
static double pitchOf (const std::vector<float>& x, size_t a, size_t b)
{
    // normalised autocorrelation, 45..1100 Hz, parabolic peak
    const int lo = (int) (SR / 1100), hi = (int) (SR / 45); double best = -1; int bl = lo; std::vector<double> nc ((size_t) hi + 2, 0.0);
    for (int lag = lo; lag <= hi; ++lag)
    {
        double c = 0, e1 = 0, e2 = 0; for (size_t i = a; i + (size_t) lag < b; ++i) { c += x[i] * x[i + (size_t) lag]; e1 += x[i] * x[i]; e2 += x[i + (size_t) lag] * x[i + (size_t) lag]; }
        nc[(size_t) lag] = c / std::sqrt (e1 * e2 + 1e-20);
    }
    for (int lag = lo; lag <= hi; ++lag) if (nc[(size_t) lag] > best * 1.0 && nc[(size_t) lag] > 0.9 * 1.0 * 0 + best) { best = nc[(size_t) lag]; bl = lag; }
    // prefer the shortest lag within 3 % of the best (avoid octave-down picks)
    for (int lag = lo; lag <= hi; ++lag) if (nc[(size_t) lag] > best * 0.97) { bl = lag; break; }
    const double y0 = nc[(size_t) bl - 1], y1 = nc[(size_t) bl], y2 = nc[(size_t) bl + 1], d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2 + 1e-12);
    return SR / (bl + d);
}
static double cents (double a, double b) { return 1200 * std::log2 (a / b); }
static void run (Masquerade::Processor& m, std::vector<float>& l, std::vector<float>& r)
{
    for (size_t i = 0; i < l.size(); i += 256) { const int n = (int) std::min<size_t> (256, l.size() - i); float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, n); }
}

int main()
{
    for (int eng = 0; eng < 2; ++eng)
    {
        for (float st : { 12.0f, -12.0f, 7.0f, 24.0f })
        {
            Masquerade::Processor m; m.p.engine = eng; m.p.mode = Masquerade::transpose; m.p.pitchSt = st; m.prepare (SR, 256);
            auto l = voice ([] (double) { return 200.0; }, 2.0), r = l; run (m, l, r);
            const double f = pitchOf (l, 48000, 60000), want = 200.0 * std::pow (2.0, st / 12.0);
            char buf[120]; std::snprintf (buf, sizeof buf, "Masquerade %s: Transpose %+.0f st on a 200 Hz voice (cents off)", eng ? "Studio" : "Live", st);
            check (std::abs (cents (f, want)) < 15, buf, cents (f, want));
        }
    }
    {
        Masquerade::Processor m; m.p.engine = 0; m.p.mode = Masquerade::transpose; m.p.pitchSt = 0; m.p.formantSt = 12; m.prepare (SR, 256);
        auto l = voice ([] (double) { return 180.0; }, 2.0), r = l; run (m, l, r);
        const double f = pitchOf (l, 48000, 60000);
        check (std::abs (cents (f, 180)) < 15, "Masquerade: Formant +12 st alone keeps the pitch (cents off)", cents (f, 180));
    }
    {
        Masquerade::Processor m; m.p.engine = 0; m.p.mode = Masquerade::quantize; m.p.keyRoot = 0; m.p.scaleMask = 0xAB5; m.p.pitchSt = 0; m.prepare (SR, 256);
        auto l = voice ([] (double) { return 213.0; }, 2.0), r = l; run (m, l, r);       // 213 Hz is G#3 + 9 ct: not in C major
        const double f = pitchOf (l, 48000, 60000), note = 69 + 12 * std::log2 (f / 440.0);
        const int pc = ((int) std::lround (note) % 12 + 12) % 12;
        check (std::abs (note - std::round (note)) < 0.15 && ((0xAB5 >> pc) & 1), "Masquerade: Quantize puts a G# voice on a C-major note (MIDI note)", note);
    }
    {
        Masquerade::Processor m; m.p.engine = 0; m.p.mode = Masquerade::robot; m.p.robotNote = 48; m.p.pitchSt = 0; m.prepare (SR, 256);
        auto l = voice ([] (double t) { return 170.0 + 60.0 * t; }, 2.0), r = l; run (m, l, r);   // a gliding voice
        const double f1 = pitchOf (l, 30000, 40000), f2 = pitchOf (l, 80000, 90000), want = 440.0 * std::pow (2.0, (48 - 69) / 12.0);
        check (std::abs (cents (f1, want)) < 20 && std::abs (cents (f2, want)) < 20, "Masquerade: Robot holds one note (C3) under a gliding voice (cents off, late)", cents (f2, want));
    }
    {
        Masquerade::Processor m; m.p.engine = 0; m.p.mode = Masquerade::midi; m.p.chordLevel = 0.8f; m.prepare (SR, 256);
        std::array<bool, 128> h {}; h[57] = true; h[64] = true; h[69] = true; m.setHeld (h);
        auto l = voice ([] (double) { return 200.0; }, 2.0), r = l; run (m, l, r);
        auto power = [&] (double hz) { const double w = 2 * juce::MathConstants<double>::pi * hz / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; for (size_t i = 48000; i < 96000; ++i) { const double s0 = 0.5 * (l[i] + r[i]) + c * s1 - s2; s2 = s1; s1 = s0; } return s1 * s1 + s2 * s2 - c * s1 * s2; };
        const double pA = power (220), pE = power (329.63), pA4 = power (440), pOff = power (200);
        check (pA > 20 * pOff && pE > 20 * pOff && pA4 > 5 * pOff, "Masquerade: MIDI mode sings a held A-minor chord (A3 vs the 200 Hz input, power ratio)", pA / std::max (1e-9, pOff));
    }
    {
        Masquerade::Processor live, studio; live.p.engine = 0; studio.p.engine = 1; studio.p.voiceType = 0; live.prepare (SR, 256); studio.prepare (SR, 256);
        check (live.latencySamples() / SR * 1000 < 1.0, "Masquerade: Live latency (ms)", live.latencySamples() / SR * 1000);
        check (studio.latencySamples() / SR * 1000 < 25.0, "Masquerade: Studio latency, Soprano range (ms)", studio.latencySamples() / SR * 1000);
    }
    {
        Masquerade::Processor m; m.p.engine = 0; m.p.pitchSt = 5; m.prepare (SR, 256);
        auto l = voice ([] (double) { return 190.0; }, 2.0), r = l; std::vector<float> rr (r.size(), 0.0f); for (size_t i = 48; i < r.size(); ++i) rr[i] = 0.7f * r[i - 48]; r = rr;   // a stereo image: R 1 ms later and quieter
        run (m, l, r);
        const double fl = pitchOf (l, 48000, 60000), fr = pitchOf (r, 48000, 60000), want = 190 * std::pow (2.0, 5 / 12.0);
        int best = 0; double bc = -1e9; for (int lag = -100; lag <= 100; ++lag) { double c = 0; for (size_t i = 50000; i < 70000; ++i) c += (double) l[i] * r[(size_t) ((long) i + lag)]; if (c > bc) { bc = c; best = lag; } }
        check (std::abs (cents (fl, want)) < 15 && std::abs (cents (fr, want)) < 15 && std::abs (best - 48.0 / std::pow (2.0, 5 / 12.0)) <= 3, "Masquerade: stereo input: both sides shifted, the image kept (lag = 48 / ratio samples)", best);
    }
    {
        Masquerade::Processor m; m.p.engine = 0; m.p.pitchSt = -5; m.p.driveVoicing = 4; m.p.drive = 0.8f; m.prepare (SR, 256);
        auto l = voice ([] (double) { return 200.0; }, 3.0), r = l, in = l; run (m, l, r);
        double ei = 0, eo = 0; for (size_t i = 96000; i < in.size(); ++i) { ei += in[i] * in[i]; eo += l[i] * l[i]; }
        bool fin = true; for (auto x : l) fin = fin && std::isfinite (x);
        check (fin && std::abs (10 * std::log10 (eo / ei)) < 2.5, "Masquerade: Crunch drive keeps the level (dB)", 10 * std::log10 (eo / ei));
    }
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
