// Squelchmoth 1.16 - Prism Split Draft separator: known-answer test on a synthetic mix with known stems.
#include <JuceHeader.h>
#include "SqSplitDsp.h"
#include <cstdio>
#include <random>
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); ok ? ++passed : ++failed; }
static double sdr (const std::vector<float>& ref, const std::vector<float>& est, int from)
{
    double s = 0, e = 0; for (size_t i = (size_t) from; i < ref.size(); ++i) { s += (double) ref[i] * ref[i]; const double d = (double) est[i] - ref[i]; e += d * d; }
    return 10.0 * std::log10 ((s + 1e-12) / (e + 1e-12));
}
int main()
{
    std::printf ("== Prism Split (Draft) ==\n");
    const double sr = 44100, bpm = 100, beat = 60.0 / bpm; const int n = (int) (sr * 16 * beat * 1.0);
    std::vector<float> vL (n, 0), dL (n, 0), bL (n, 0), oL (n, 0), oR (n, 0);
    std::mt19937 rng (3); std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
    const double tp = 2 * M_PI;
    // "vocal": a centred harmonic voice with vibrato, a new note every 2 beats, formant-ish harmonic weights
    { const double notes[] = { 220, 262, 294, 262, 330, 294, 262, 220 }; double ph = 0;
      for (int i = 0; i < n; ++i) { const double t = i / sr; const int k = (int) (t / (2 * beat)) % 8; const double f = notes[k] * (1.0 + 0.006 * std::sin (tp * 5.5 * t)); ph += tp * f / sr;
        const double tn = std::fmod (t, 2 * beat), env = std::min (1.0, tn / 0.04) * std::min (1.0, (2 * beat - tn) / 0.08); double v = 0; const double w[] = { 1, 0.7, 0.9, 0.5, 0.35, 0.2, 0.12 };
        for (int h = 1; h <= 7; ++h) v += w[h - 1] * std::sin (h * ph); vL[(size_t) i] = (float) (0.09 * env * v); } }
    // drums: kick on beats, noise snare on 2 and 4, noise hats on 8ths (centre)
    for (int b = 0; b < 16; ++b)
    {
        const int a = (int) (b * beat * sr); double ph = 0;
        for (int i = 0; i < (int) (0.25 * sr) && a + i < n; ++i) { const double t = i / sr; ph += tp * (48 + 130 * std::exp (-t * 38)) / sr; dL[(size_t) (a + i)] += (float) (0.6 * std::sin (ph) * std::exp (-t * 14)); }
        if (b % 2 == 1) for (int i = 0; i < (int) (0.15 * sr) && a + i < n; ++i) dL[(size_t) (a + i)] += uni (rng) * 0.3f * (float) std::exp (-i / sr * 26);
        for (int h = 0; h < 2; ++h) { const int ah = a + (int) (h * 0.5 * beat * sr); for (int i = 0; i < (int) (0.03 * sr) && ah + i < n; ++i) dL[(size_t) (ah + i)] += uni (rng) * 0.12f * (float) std::exp (-i / sr * 140); }
    }
    // bass: a held sine with a second harmonic, a new note every 4 beats (centre)
    { const double notes[] = { 55, 43.65, 49, 55 }; double ph = 0; for (int i = 0; i < n; ++i) { const double t = i / sr; const double f = notes[(int) (t / (4 * beat)) % 4]; ph += tp * f / sr; const double tn = std::fmod (t, 4 * beat); bL[(size_t) i] = (float) (0.35 * std::min (1.0, tn / 0.02) * (std::sin (ph) + 0.2 * std::sin (2 * ph))); } }
    // other: two different sustained chords hard left and hard right (pads)
    for (int i = 0; i < n; ++i) { const double t = i / sr; double l = 0, r = 0; for (double f : { 392.0, 493.9, 587.3 }) l += std::sin (tp * f * t); for (double f : { 349.2, 440.0, 523.3, 659.3 }) r += std::sin (tp * f * t * 1.003); oL[(size_t) i] = (float) (0.05 * l); oR[(size_t) i] = (float) (0.05 * r); }
    std::vector<float> L (n), R (n);
    for (int i = 0; i < n; ++i) { L[(size_t) i] = vL[(size_t) i] + dL[(size_t) i] + bL[(size_t) i] + oL[(size_t) i]; R[(size_t) i] = vL[(size_t) i] + dL[(size_t) i] + bL[(size_t) i] + oR[(size_t) i]; }

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    auto st = SqSplit::splitBuffers (L.data(), R.data(), n, sr);
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    // null: the four stems sum back to the mix
    double pe = 0, ps = 0; for (int i = 0; i < n; ++i) { double sum = 0; for (int s = 0; s < 4; ++s) sum += st[(size_t) (2 * s)][(size_t) i]; pe += (sum - L[(size_t) i]) * (sum - L[(size_t) i]); ps += (double) L[(size_t) i] * L[(size_t) i]; }
    const double nullDb = 10 * std::log10 ((pe + 1e-20) / ps);
    check (nullDb < -80.0, "the four stems sum back to the mix (null " + juce::String (nullDb, 1) + " dB)");
    const int from = (int) sr;   // skip the first second (the medians' start-up)
    const double sv = sdr (vL, st[0], from), sd = sdr (dL, st[2], from), sb = sdr (bL, st[4], from), so = sdr (oL, st[6], from);
    // what the mix itself would score as an "estimate" of each stem: the separation has to beat it clearly
    const double mv = sdr (vL, L, from), md = sdr (dL, L, from), mb = sdr (bL, L, from), mo = sdr (oL, L, from);
    std::printf ("     SDR (stem vs mix as estimate): vocals %.1f (%.1f)  drums %.1f (%.1f)  bass %.1f (%.1f)  other %.1f (%.1f) dB\n", sv, mv, sd, md, sb, mb, so, mo);
    check (sv > 5.0 && sv > mv + 8.0, "vocals: " + juce::String (sv, 1) + " dB SDR, " + juce::String (sv - mv, 1) + " dB better than the mix");
    check (sd > 4.0 && sd > md + 5.0, "drums: " + juce::String (sd, 1) + " dB SDR, " + juce::String (sd - md, 1) + " dB better than the mix");
    check (sb > 8.0 && sb > mb + 5.0, "bass: " + juce::String (sb, 1) + " dB SDR, " + juce::String (sb - mb, 1) + " dB better than the mix");
    check (so > 4.0 && so > mo + 8.0, "other (side pads): " + juce::String (so, 1) + " dB SDR, " + juce::String (so - mo, 1) + " dB better than the mix");
    // instrumental = mix - vocals: how much of the vocal is left
    { double e = 0, s = 0; for (int i = from; i < n; ++i) { const double inst = L[(size_t) i] - st[0][(size_t) i]; const double truth = L[(size_t) i] - vL[(size_t) i]; e += (inst - truth) * (inst - truth); s += (double) vL[(size_t) i] * vL[(size_t) i]; }
      const double red = 10 * std::log10 (s / (e + 1e-20)); check (red > 5.0, "vocal remover: the vocal is " + juce::String (red, 1) + " dB down in the instrumental"); }
    check (ms < n / sr * 1000.0 * 0.5, "faster than 2x real time on one core (" + juce::String (n / sr, 1) + " s of audio in " + juce::String (ms / 1000.0, 2) + " s)");
    // streaming in odd block sizes gives the same answer
    { SqSplit::Splitter sp (sr); std::vector<float> acc; auto sink = [&] (const float* const* s, int m) { acc.insert (acc.end(), s[0], s[0] + m); };
      for (int p = 0; p < n;) { const int m = std::min (n - p, 777 + (p % 1000)); sp.push (L.data() + p, R.data() + p, m, sink); p += m; } sp.finish (sink); acc.resize ((size_t) n);
      double d = 0; for (int i = 0; i < n; ++i) d = std::max (d, (double) std::abs (acc[(size_t) i] - st[0][(size_t) i])); check (d < 1e-6, "streaming in uneven blocks matches the one-shot result (max diff " + juce::String (d, 9) + ")"); }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
