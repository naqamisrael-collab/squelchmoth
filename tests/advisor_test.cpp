// Squelchmoth 1.23 - the advisor's measurements and the advice written from them, on signals with known answers.
#include <JuceHeader.h>
#include "SqAdvisor.h"
#include <cstdio>
#include <random>
static int passed = 0, failed = 0;
static void check (bool ok, const juce::String& what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); ok ? ++passed : ++failed; }
static const double fs = 48000.0; static const int n = 48000 * 4;
using V = std::vector<float>;
static V sine (double f, double amp) { V v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / fs)); return v; }
static V noise (double amp, unsigned seed) { std::mt19937 g (seed); std::uniform_real_distribution<float> d (-1.0f, 1.0f); V v ((size_t) n); for (auto& x : v) x = (float) amp * d (g); return v; }
static V add (const V& a, const V& b) { V v (a.size()); for (size_t i = 0; i < a.size(); ++i) v[i] = a[i] + b[i]; return v; }
static V neg (const V& a) { V v (a.size()); for (size_t i = 0; i < a.size(); ++i) v[i] = -a[i]; return v; }
static SqAdvise::Stats m (const V& l, const V& r) { return SqAdvise::measure (l.data(), r.data(), n, fs); }
static bool has (const juce::StringArray& a, const char* text) { for (auto& s : a) if (s.containsIgnoreCase (text)) return true; return false; }
int main()
{
    std::printf ("== Advisor ==\n");
    {
        const auto s = sine (80.0, 0.5); const auto st = m (s, s);
        check (std::abs (st.peakDb + 6.02) < 0.1 && std::abs (st.rmsDb + 9.03) < 0.1 && std::abs (st.crestDb - 3.01) < 0.1, "an 80 Hz sine at 0.5: " + SqAdvise::statsLine (st));
        int top = 0; for (int b = 1; b < SqAdvise::numBands; ++b) if (st.bandPower[b] > st.bandPower[top]) top = b;
        check (top == 1 && std::abs (st.bandDb (1) + 9.03) < 0.5, "its energy is in the bass band at the right level: " + SqAdvise::bandsLine (st));
        check (st.correlation > 0.999 && st.lowSideDb < -60.0 && st.clippedRuns == 0, "mono, centred low end, no clipping");
    }
    {
        const auto s = sine (1000.0, 0.25); const auto st = m (s, neg (s));
        check (st.correlation < -0.999, "left = -right reads as out of phase (" + juce::String (st.correlation, 3) + ")");
        check (has (SqAdvise::recordingAdvice (st), "polarity"), "and the recording advice says to flip the polarity");
    }
    {
        const auto l = sine (70.0, 0.4), r = neg (l); const auto mid = sine (1000.0, 0.2);
        const auto st = m (add (l, mid), add (r, mid));
        const auto adv = SqAdvise::mixAdvice (st, {});
        check (st.lowSideDb > 20.0 && has (adv, "low end is wide"), "a bass that is all side: the advice is to keep the low end in the middle");
    }
    {
        V s = sine (220.0, 1.6); for (auto& x : s) x = juce::jlimit (-1.0f, 1.0f, x);
        const auto st = m (s, s);
        check (st.clippedRuns > 500, "a hard-clipped take: " + juce::String (st.clippedRuns) + " flat-topped spots counted");
        check (has (SqAdvise::recordingAdvice (st), "record it again") && has (SqAdvise::mixAdvice (st, {}), "clips"), "both advisors call it out");
    }
    {
        // a voice-like take: bursts of tone over steady room noise at about -46 dBFS RMS
        V s = noise (0.0087, 3); for (int i = 0; i < n; ++i) if ((i / 12000) % 2 == 0) s[(size_t) i] += (float) (0.3 * std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / fs));
        const auto st = m (s, s);
        check (std::abs (st.noiseDb + 46.0) < 1.5, "noise floor between phrases measured at " + juce::String (st.noiseDb, 1) + " dBFS (made at -46)");
        check (has (SqAdvise::recordingAdvice (st), "Noisy room"), "the recording advice names the noise and what to do");
        V q = noise (0.00005, 4); for (int i = 0; i < n; ++i) if ((i / 12000) % 2 == 0) q[(size_t) i] += (float) (0.3 * std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / fs));
        check (SqAdvise::recordingAdvice (m (q, q)).isEmpty(), "a clean take at a good level gets no advice");
    }
    {
        // kick and 808 both on 50 Hz; a hat with a little rumble; a lead alone in the mids
        const auto kick = sine (50.0, 0.3), sub = sine (52.0, 0.35), lead = sine (800.0, 0.2);
        const auto hat = add (noise (0.05, 9), sine (45.0, 0.01));
        std::vector<SqAdvise::Named> tr { { "Kick", m (kick, kick) }, { "808", m (sub, sub) }, { "Lead", m (lead, lead) }, { "Hat", m (hat, hat) } };
        const auto cl = SqAdvise::clashes (tr);
        bool subClash = false, midClash = false; for (auto& c : cl) { if (c.band == 0 && ((c.a == "Kick" && c.b == "808") || (c.a == "808" && c.b == "Kick"))) subClash = true; if (c.band == 3) midClash = true; }
        check (subClash && ! midClash, "kick and 808 are found sharing the sub band; the lead, alone in the mids, is not flagged");
        const auto mix = add (add (kick, sub), add (lead, hat));
        const auto adv = SqAdvise::mixAdvice (m (mix, mix), tr);
        check (has (adv, "Kick") && has (adv, "sidechain"), "the mix advice names both and says to duck or filter one");
        check (has (adv, "Hat has low-end energy"), "the hat's rumble gets a high-pass suggestion");
        bool leadNamed = false; for (auto& s : adv) if (s.startsWith ("Lead")) leadNamed = true;
        check (! leadNamed, "nothing is said about the lead");
        for (auto& s : adv) std::printf ("     - %s\n", s.toRawUTF8());
    }
    {
        const auto s = sine (440.0, 0.02); check (has (SqAdvise::mixAdvice (m (s, s), {}), "mix is low"), "a mix peaking at -34 dBFS is told to come up");
        const auto z = V ((size_t) n, 0.0f); check (has (SqAdvise::mixAdvice (m (z, z), {}), "silent"), "silence is reported as silence");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
