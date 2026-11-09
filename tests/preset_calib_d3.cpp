// Renders every generated Larva Filter 2 / Twelve Phase 2 preset through the real DSP (SqColor111dDsp.h) on the three audit
// signals and prints the Output trim that holds the level on the material the preset was made for.
// scripts/gen_presets_111f.py reads it: TRIM <tab> plug-in <tab> name <tab> trim dB <tab> peak dBFS. The mapping is the
// wrappers' (Source/SqColor111d.h, fill()); like the in-app audit the transport plays at 120 BPM from bar 1.
#include "SqColor111dDsp.h"
#include "SqColor111bDsp.h"
#include "presets111f_table.h"
#include <cstdio>
#include <string>
using namespace Eng;
static double energy (const std::vector<float>& l, const std::vector<float>& r, size_t from)
{ double e = 0; for (size_t i = from; i < l.size(); ++i) e += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]); return e / (double) (l.size() - from); }
static int target (const std::string& n)
{
    const auto cat = n.substr (0, n.find (" | "));
    if (cat.find ("Vocal") != std::string::npos || cat.find ("Voice") != std::string::npos) return 2;
    if (cat.find ("Drum") != std::string::npos || cat.rfind ("808", 0) == 0 || cat.rfind ("Dub", 0) == 0 || cat.rfind ("Rhythmic", 0) == 0 || cat.rfind ("Snare", 0) == 0) return 1;
    return 0;
}
static const double sr = 48000, secs = 3.0; static const size_t from = (size_t) (sr * 1.0);
static std::vector<float> L[3], R[3]; static double ein[3];

// Wing Mod props (same order in both tables, after each plug-in's own props): mode shape sync smooth trigger numSteps stepDivision pattern
static void fillMod (WingMod::Settings& s, const int* pr, float rate, float groove, float feel, float eA, float eR, float eS, float a, float d, float sus, float r)
{
    s.mode = clamp (pr[0], 0, (int) WingMod::numModes - 1); s.shape = clamp (pr[1], 0, (int) WingMod::numShapes - 1);
    s.sync = pr[2] > 0; s.syncIndex = std::max (0, pr[2] - 1); s.rateHz = rate; s.smoothing = clamp (pr[3], 0, 4);
    s.trigger = clamp (pr[4], 0, 2); s.numSteps = clamp (pr[5] > 0 ? pr[5] : 16, 1, 16); s.stepDivision = clamp (pr[6], 0, WingMod::numSyncDivisions() - 1);
    s.swing = groove; s.feel = feel; s.attackMs = eA; s.releaseMs = eR; s.sensitivityDb = eS; s.aMs = a; s.dMs = d; s.sLevel = sus; s.rMs = r;
    s.bipolar = true; s.retrigger = s.trigger != 0;
    const auto& pats = Tremor::patterns(); const auto& pat = pats[(size_t) clamp (pr[7], 0, (int) pats.size() - 1)];
    for (int i = 0; i < 16; ++i) s.steps[i] = pat.v[i];
}

template <typename Proc, typename Fill>
static void render (const char* plug, const D3Row& row, Fill fill)
{
    double d[3]; float pk = 0; bool finite = true;
    for (int k = 0; k < 3; ++k)
    {
        Proc p; fill (p.p, row);
        p.prepare (sr, 256);
        auto l = L[k], r = R[k];
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.setTransport (120, (double) i / sr * 2.0, true); p.process (ch, 2, nullptr, 256); }
        for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
        d[k] = 10 * std::log10 (std::max (1e-20, energy (l, r, from)) / ein[k]);
    }
    const double trim = -d[target (row.name)];
    std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", plug, row.name, trim, gainToDb (pk));
    std::fprintf (stderr, "%-16s %-40s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", plug, row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
}

int main()
{
    TestSignals::pink (L[0], R[0], sr, secs, -18); TestSignals::drums (L[1], R[1], sr, secs); TestSignals::vocal (L[2], R[2], sr, secs);
    for (int k = 0; k < 3; ++k) { const size_t n = (L[k].size() / 256) * 256; L[k].resize (n); R[k].resize (n); ein[k] = energy (L[k], R[k], from); }
    // Larva 2 values: cutoff resonance freqMod resMod lrOffset drive mix output input s1..s8 modRate groove feel envAttack envRelease envSens adsrA adsrD adsrS adsrR
    //   props: stages routing resp1..8 model1..8 analog analogPostOn keyOn | Wing Mod
    for (auto& row : larvaRows)
        render<Larva2::Processor> ("Larva Filter 2", row, [] (Larva2::Params& p, const D3Row& r)
        {
            const float* v = r.v; const int* pr = r.pr;
            p.stages = clamp (pr[0], 1, 8); p.routing = clamp (pr[1], 0, 2);
            for (int i = 0; i < 8; ++i) { p.response[i] = clamp (pr[2 + i], 0, 3); p.model[i] = clamp (pr[10 + i], 0, 3); p.stageOct[i] = v[9 + i]; }
            p.cutoffHz = v[0]; p.resonance = v[1] / 100.0f; p.freqModOct = v[2]; p.resMod = v[3] / 100.0f; p.lrOffsetOct = v[4];
            p.analog = clamp (pr[18], 0, (int) AnalogStyle::numVoicings); p.analogAmount = v[5] / 100.0f; p.analogPost = pr[19] != 0;
            p.mix = v[6] / 100.0f; p.outputDb = 0; p.inputDb = v[8]; p.keyOn = false;
            fillMod (p.mod, pr + 21, v[17], v[18] / 100.0f, v[19] / 100.0f, v[20], v[21], v[22], v[23], v[24], v[25] / 100.0f, v[26]);
        });
    // Phase 2 values: centre sweep resonance depth spread mix output drive modRate groove feel envAttack envRelease envSens adsrA adsrD adsrS adsrR
    //   props: style stages classicOn resMode polarity tzOn analog analogPostOn keyOn | Wing Mod
    for (auto& row : phaseRows)
        render<Phase2::Processor> ("Twelve Phase 2", row, [] (Phase2::Params& p, const D3Row& r)
        {
            const float* v = r.v; const int* pr = r.pr;
            p.stages = clamp (pr[1], 2, 24); p.classic = pr[2] != 0; p.resMode = clamp (pr[3], 0, 1); p.polarity = clamp (pr[4], 0, 1); p.throughZero = pr[5] != 0;
            p.centreHz = v[0]; p.sweepOct = v[1]; p.resonance = v[2] / 100.0f; p.depth = v[3] / 100.0f; p.spreadDeg = v[4]; p.mix = v[5] / 100.0f; p.outputDb = 0;
            p.analog = clamp (pr[6], 0, (int) AnalogStyle::numVoicings); p.analogAmount = v[7] / 100.0f; p.analogPost = pr[7] != 0; p.keyOn = false;
            fillMod (p.mod, pr + 9, v[8], v[9] / 100.0f, v[10] / 100.0f, v[11], v[12], v[13], v[14], v[15], v[16] / 100.0f, v[17]);
        });
}
