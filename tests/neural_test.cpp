// Squelchmoth 1.16 - Prism Split Studio (neural) separator on a synthetic mix with known drums and bass.
// The runtime library and the model are not in the repository: pass their paths in SQ_ORT_LIB and SQ_SPLIT_MODEL.
#include <JuceHeader.h>
#include "SqNeuralSplit.h"
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
    std::printf ("== Prism Split (Studio, neural) ==\n");
    const juce::String libPath (std::getenv ("SQ_ORT_LIB") ? std::getenv ("SQ_ORT_LIB") : ""), modelPath (std::getenv ("SQ_SPLIT_MODEL") ? std::getenv ("SQ_SPLIT_MODEL") : "");
    if (libPath.isEmpty() || modelPath.isEmpty()) { std::printf ("  not run: SQ_ORT_LIB and SQ_SPLIT_MODEL are not set\n"); return 2; }
    #include "synth_mix.inc"

    const juce::String model6 (std::getenv ("SQ_SPLIT_MODEL6") ? std::getenv ("SQ_SPLIT_MODEL6") : "");
    for (int N : { 4, 6 })
    {
    const juce::String mp = N == 6 ? model6 : modelPath;
    if (mp.isEmpty()) { std::printf ("  %d-stem model not run: no path given\n", N); continue; }
    SqNeural::Engine eng; juce::String err;
    const bool loaded = eng.load (juce::File (libPath), juce::File (mp), 2, err, N);
    check (loaded, juce::String (N) + "-stem model: runtime and model load" + (err.isNotEmpty() ? " - " + err : juce::String()));
    if (! loaded) continue;
    std::vector<std::vector<float>> st ((size_t) (2 * N));
    auto sink = [&] (const float* const* s, int m) { for (int c = 0; c < 2 * N; ++c) st[(size_t) c].insert (st[(size_t) c].end(), s[c], s[c] + m); };
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    SqNeural::Splitter sp (eng);
    for (int p = 0; p < n;) { const int m = std::min (n - p, 50000); sp.push (L.data() + p, R.data() + p, m, sink); p += m; }
    sp.finish (sink);
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    check (sp.error.isEmpty() && (int) st[0].size() == n, "every sample comes back (" + juce::String ((int) st[0].size()) + " of " + juce::String (n) + ")" + (sp.error.isNotEmpty() ? " - " + sp.error : juce::String()));
    if ((int) st[0].size() != n) continue;
    double pe = 0, ps = 0; for (int i = 0; i < n; ++i) { double sum = 0; for (int s = 0; s < N; ++s) sum += st[(size_t) (2 * s)][(size_t) i]; pe += (sum - L[(size_t) i]) * (sum - L[(size_t) i]); ps += (double) L[(size_t) i] * L[(size_t) i]; }
    const double nullDb = 10 * std::log10 ((pe + 1e-20) / ps);
    check (nullDb < -100.0, juce::String (N) + " stems sum back to the mix (null " + juce::String (nullDb, 1) + " dB)");
    const int from = (int) sr;
    const double sd = sdr (dL, st[2], from), sb = sdr (bL, st[4], from);
    check (sd > 15.0, "drums: " + juce::String (sd, 1) + " dB SDR (Draft scores about 6 on this mix)");
    check (sb > 15.0, "bass: " + juce::String (sb, 1) + " dB SDR (Draft scores about 14)");
    {
        // no click at a segment seam: the drum stem's error around the first seam is no worse than mid-segment
        auto errAt = [&] (int a, int len) { double e = 0; for (int i = a; i < a + len; ++i) { const double d = st[2][(size_t) i] - dL[(size_t) i]; e += d * d; } return 10 * std::log10 (e / len + 1e-20); };
        const double seam = errAt (SqNeural::stride - 2000, 4000 + SqNeural::overlap), mid = errAt (SqNeural::stride / 2, 4000 + SqNeural::overlap);
        check (seam < mid + 6.0, "segment seams are clean (error " + juce::String (seam, 1) + " dB at the seam, " + juce::String (mid, 1) + " dB mid-segment)");
    }
    std::printf ("     %.1f s of audio in %.1f s on 2 threads\n", n / sr, ms / 1000.0);
    if (N == 6) { double g = 0, pi = 0, tot = 0; for (int i = 0; i < n; ++i) { g += (double) st[8][(size_t) i] * st[8][(size_t) i]; pi += (double) st[10][(size_t) i] * st[10][(size_t) i]; tot += (double) L[(size_t) i] * L[(size_t) i]; }
                  std::printf ("     guitar stem %.1f dB, piano stem %.1f dB relative to the mix (this test mix has neither)\n", 10 * std::log10 (g / tot + 1e-12), 10 * std::log10 (pi / tot + 1e-12)); }
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
