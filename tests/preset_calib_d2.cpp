// Renders every generated Shard / Murmuration / Masquerade preset through the real DSP on the three audit signals and
// prints the Output trim that holds the level on the material the preset was made for. scripts/gen_presets_111e.py reads
// it: TRIM <tab> plug-in <tab> name <tab> trim dB <tab> peak dBFS. Mapping as in Source/SqColor111c.h; the transport plays
// at 120 BPM like the in-app audit. Built by scripts/build_calib_d2.sh (Masquerade needs JUCE core).
#include "JuceHeader.h"
#include "SqColor111cDsp.h"
#include "SqMasqueradeDsp.h"
#include "presets111e_table.h"
#include <cstdio>
#include <string>
using namespace Eng;
static double energy (const std::vector<float>& l, const std::vector<float>& r, size_t from)
{ double e = 0; for (size_t i = from; i < l.size(); ++i) e += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]); return e / (double) (l.size() - from); }
static int target (const std::string& n, bool vocalPlugin)
{
    if (vocalPlugin) return 2;
    const auto cat = n.substr (0, n.find (" | "));
    if (cat.find ("Vocal") != std::string::npos || cat.find ("Voice") != std::string::npos) return 2;
    if (cat.find ("Drum") != std::string::npos || cat.rfind ("808", 0) == 0 || cat.rfind ("Dub", 0) == 0 || cat.rfind ("Rhythmic", 0) == 0 || cat.rfind ("Snare", 0) == 0) return 1;
    return 0;
}
static const double sr = 48000, secs = 3.0; static const size_t from = (size_t) (sr * 1.0);
static std::vector<float> L[3], R[3]; static double ein[3];

template <typename Proc, typename Fill, typename Run>
static void render (const char* plug, const D2Row& row, bool vocalPlugin, Fill fill, Run run)
{
    double d[3]; float pk = 0; bool finite = true;
    for (int k = 0; k < 3; ++k)
    {
        Proc p; fill (p.p, row);
        p.prepare (sr, 256);
        auto l = L[k], r = R[k];
        for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; run (p, ch, (double) i / sr * 2.0); }
        for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
        d[k] = 10 * std::log10 (std::max (1e-20, energy (l, r, from)) / ein[k]);
    }
    std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", plug, row.name, -d[target (row.name, vocalPlugin)], gainToDb (pk));
    std::fprintf (stderr, "%-12s %-42s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", plug, row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
}

int main()
{
    TestSignals::pink (L[0], R[0], sr, secs, -18); TestSignals::drums (L[1], R[1], sr, secs); TestSignals::vocal (L[2], R[2], sr, secs);
    for (int k = 0; k < 3; ++k) ein[k] = energy (L[k], R[k], from);
    for (auto& row : shardRows)
        render<Shard::Processor> ("Shard", row, false, [] (Shard::Params& p, const D2Row& r)
        {
            const float* v = r.v; const int* q = r.pr;
            p.spliceMs = v[0]; p.delayMs = v[1]; p.pitchSt = v[2]; p.recycle = v[3] / 100.0f; p.mix = v[4] / 100.0f; p.reverseProb = v[5] / 100.0f; p.smoothing = v[6] / 100.0f;
            p.spray = v[7] / 100.0f; p.density = v[8]; p.sizeJitter = v[9] / 100.0f; p.pitchOffsetLCents = v[10]; p.pitchOffsetRCents = v[11]; p.spliceOffsetR = v[12] / 100.0f; p.delayOffsetRMs = v[13];
            p.lowCutHz = v[14]; p.highCutHz = v[15]; p.dynThreshDb = v[16]; p.dynReleaseMs = v[17];
            p.engine = q[0]; p.fbMode = q[1]; p.dyn = q[2]; p.keyOn = false; p.quantize = q[4]; p.keyRoot = q[6]; p.scaleMask = 0xAB5; p.spliceSync = q[8] - 1; p.delaySync = q[9] - 1;
            p.lfo[0] = { q[10], v[18] / 100.0f, v[19], q[11], q[12] > 0, std::max (0, q[12] - 1) };
            p.lfo[1] = { q[13], v[20] / 100.0f, v[21], q[14], q[15] > 0, std::max (0, q[15] - 1) };
            p.envTarget = q[16]; p.envDepth = v[22] / 100.0f; p.envAttackMs = v[23]; p.envReleaseMs = v[24]; p.outputDb = 0;
        }, [] (Shard::Processor& p, float** ch, double ppq) { p.setTransport (120, ppq, true); p.process (ch, 2, nullptr, 256); });
    for (auto& row : murmurationRows)
        render<Murm::Processor> ("Murmuration", row, false, [] (Murm::Params& p, const D2Row& r)
        {
            const float* v = r.v; const int* q = r.pr;
            p.timeMs = v[0]; p.timeBeats = v[1] >= 0.25f ? v[1] : 0.0f; p.x = v[2] / 100.0f; p.y = v[3] / 100.0f; p.texture = v[4] / 100.0f; p.warp = v[5] / 100.0f; p.color = v[6] / 100.0f;
            p.blur = v[7] / 100.0f; p.grain = v[8] / 100.0f; p.modRate = v[9]; p.modDepth = v[10] / 100.0f; p.shimmer = v[11] / 100.0f; p.shimmerFade = v[12] / 100.0f; p.tiltDb = v[13];
            p.lowCutHz = v[14]; p.highCutHz = v[15]; p.width = v[16] / 100.0f; p.duckDb = v[17]; p.preDelayMs = v[18]; p.mix = v[19] / 100.0f; p.outputDb = 0;
            static const int sst[] = { -24, -12, 12, 24 };
            p.sync = q[0] - 1; p.modShape = q[1]; p.shimmerSt = sst[clamp (q[2], 0, 3)]; p.freeze = q[3]; p.freezeMode = q[4]; p.infinite = q[5]; p.customEnv = q[6];
        }, [] (Murm::Processor& p, float** ch, double ppq) { p.setTransport (120, ppq, true); p.process (ch, 2, 256); });
    for (auto& row : masqueradeRows)
        render<Masquerade::Processor> ("Masquerade", row, true, [] (Masquerade::Params& p, const D2Row& r)
        {
            const float* v = r.v; const int* q = r.pr;
            p.pitchSt = v[0]; p.formantSt = v[1]; p.drive = v[2] / 100.0f; p.retuneMs = v[3]; p.chordLevel = v[4] / 100.0f; p.chordSpread = v[5] / 100.0f; p.mix = v[6] / 100.0f; p.outputDb = 0;
            p.mode = q[0]; p.link = q[1]; p.engine = q[2]; p.voiceType = q[3]; p.keyRoot = q[5]; p.scaleMask = 0xAB5; p.robotNote = q[7]; p.driveVoicing = q[8];
        }, [] (Masquerade::Processor& p, float** ch, double) { p.process (ch, 2, 256); });
}
