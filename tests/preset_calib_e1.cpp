// Renders every generated Siren Verb / Mic Closet / Larynx / Exhale preset through the real DSP (SqVoice111eDsp.h) on the
// three audit signals and prints the Output trim that holds the level on the sung vocal (all four are vocal plug-ins).
// scripts/gen_presets_111g.py reads it: TRIM <tab> plug-in <tab> name <tab> trim dB <tab> peak dBFS. Mapping as in
// Source/SqVoice111e.h (fill()); the transport is 120 BPM like the in-app audit.
#include "JuceHeader.h"
#include "SqVoice111eDsp.h"
#include "presets111g_table.h"
#include <cstdio>
#include <string>
using namespace Eng;
static double energy (const std::vector<float>& l, const std::vector<float>& r, size_t from)
{ double e = 0; for (size_t i = from; i < l.size(); ++i) e += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]); return e / (double) (l.size() - from); }
static const double sr = 48000, secs = 3.0; static const size_t from = (size_t) (sr * 1.0);
static std::vector<float> L[3], R[3]; static double ein[3];

template <typename Proc, typename Fill>
static void render (const char* plug, const E1Row& row, Fill fill)
{
    double d[3]; float pk = 0; bool finite = true;
    for (int k = 0; k < 3; ++k)
    {
        Proc p; fill (p.p, row);
        p.prepare (sr, 256);
        auto l = L[k], r = R[k];
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; p.process (ch, 2, 256); }
        for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
        d[k] = 10 * std::log10 (std::max (1e-20, energy (l, r, from)) / ein[k]);
    }
    const double trim = -d[2];
    std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", plug, row.name, trim, gainToDb (pk));
    std::fprintf (stderr, "%-12s %-46s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", plug, row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
}

int main()
{
    TestSignals::pink (L[0], R[0], sr, secs, -18); TestSignals::drums (L[1], R[1], sr, secs); TestSignals::vocal (L[2], R[2], sr, secs);
    for (int k = 0; k < 3; ++k) { const size_t n = (L[k].size() / 256) * 256; L[k].resize (n); R[k].resize (n); ein[k] = energy (L[k], R[k], from); }
    static const float shimSt[5] = { 7, 12, 19, 24, -12 };
    for (auto& row : sverbRows)
        render<SirenVerb::Processor> ("Siren Verb", row, [] (SirenVerb::Params& p, const E1Row& r)
        {
            const float* v = r.v; const int* pr = r.pr;
            p.algo = clamp (pr[0], 0, (int) SirenVerb::numAlgos - 1);
            p.size = v[0] / 100.0f; p.decayS = v[1]; p.preDelayMs = v[2]; p.damping = v[3] / 100.0f; p.early = v[4] / 100.0f; p.modRate = v[5]; p.modDepth = v[6] / 100.0f;
            p.width = v[7] / 100.0f; p.lowCutHz = v[8]; p.highCutHz = v[9]; p.mix = v[10] / 100.0f; p.outputDb = 0;
            p.prePitchSt = v[12]; p.preThroat = v[13] / 100.0f; p.preTube = v[14] / 100.0f; p.preDeEss = v[15] / 100.0f; p.reverseMs = v[16]; p.reverse = pr[1] != 0;
            p.autoEq = pr[2] != 0; p.autoEqDb = v[17]; p.tone = v[18] / 100.0f; p.comp = v[19] / 100.0f; p.gate = v[20] / 100.0f; p.duck = v[21] / 100.0f;
            p.shimmerAmt = v[22] / 100.0f; p.shimmerSt = shimSt[clamp (pr[5], 0, 4)];
            p.delayMode = clamp (pr[3], 0, 2); p.delayMs = v[23]; p.delayFb = v[24] / 100.0f; p.delayLevel = v[25] / 100.0f;
            if (pr[4] > 0) p.delayMs = (float) std::min (2000.0, WingMod::syncBeats (pr[4] - 1) * 60000.0 / 120.0);
        });
    for (auto& row : micRows)
        render<MicCloset::Processor> ("Mic Closet", row, [] (MicCloset::Params& p, const E1Row& r)
        {
            const float* v = r.v; const int* pr = r.pr;
            p.source = clamp (pr[0], 0, MicCloset::numMics() - 1); p.target = clamp (pr[1], 0, MicCloset::numMics() - 1); p.pattern = pr[2] <= 0 ? -1 : clamp (pr[2] - 1, 0, 3); p.sourceProx = pr[3] != 0;
            p.distanceCm = v[0]; p.lowCutHz = v[1]; p.padDb = v[2]; p.colour = v[3] / 100.0f; p.noise = v[4] / 100.0f; p.mix = v[5] / 100.0f; p.outputDb = 0;
        });
    for (auto& row : larynxRows)
        render<Larynx::Processor> ("Larynx", row, [] (Larynx::Params& p, const E1Row& r)
        {
            const float* v = r.v;
            p.length = v[0] / 100.0f; for (int i = 0; i < 8; ++i) p.shape[i] = v[1 + i] / 100.0f;
            p.glottal = v[9] / 100.0f; p.pulseWidth = v[10] / 100.0f; p.breath = v[11] / 100.0f; p.breathHz = v[12]; p.precision = v[13] / 100.0f; p.mix = v[14] / 100.0f; p.outputDb = 0; p.levelMatch = r.pr[0] != 0;
        });
    for (auto& row : exhaleRows)
        render<Exhale::Processor> ("Exhale", row, [] (Exhale::Params& p, const E1Row& r)
        {
            const float* v = r.v;
            p.breathDb = v[0]; p.tone = v[1] / 100.0f; p.consonants = v[2] / 100.0f; p.bodyDb = v[3]; p.mix = v[4] / 100.0f; p.outputDb = 0;
        });
}
