// Renders every generated Hive Choir / Double Wing 2 / Mutant preset through the real DSP (SqVoice111fDsp.h) on the three
// audit signals and prints the Output trim that holds the level on the sung vocal. scripts/gen_presets_111h.py reads it.
// Mapping as in Source/SqVoice111f.h (fill()); the transport plays at 120 BPM from bar 1 like the in-app audit; with no
// session key the preset's key is used (C; scale 1 major, 2 minor).
#include "JuceHeader.h"
#include "SqVoice111fDsp.h"
#include "presets111h_table.h"
#include <cstdio>
using namespace Eng;
static double energy (const std::vector<float>& l, const std::vector<float>& r, size_t from)
{ double e = 0; for (size_t i = from; i < l.size(); ++i) e += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]); return e / (double) (l.size() - from); }
static const double sr = 48000, secs = 3.0; static const size_t from = (size_t) (sr * 1.0);
static std::vector<float> L[3], R[3]; static double ein[3];
template <typename Proc, typename Fill>
static void render (const char* plug, const E2Row& row, Fill fill, bool transport)
{
    double d[3]; float pk = 0; bool finite = true;
    for (int k = 0; k < 3; ++k)
    {
        Proc p; fill (p.p, row); p.prepare (sr, 256);
        auto l = L[k], r = R[k];
        for (size_t i = 0; i + 256 <= l.size(); i += 256)
        {
            float* ch[2] = { l.data() + i, r.data() + i };
            if constexpr (std::is_same_v<Proc, Mutant::Processor>) p.setTransport (120, (double) i / sr * 2.0, true);
            p.process (ch, 2, 256);
        }
        for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
        d[k] = 10 * std::log10 (std::max (1e-20, energy (l, r, from)) / ein[k]);
    }
    (void) transport;
    std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", plug, row.name, -d[2], gainToDb (pk));
    std::fprintf (stderr, "%-14s %-44s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", plug, row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
}
static int maskFor (int scale) { return scale == 2 ? 0x5AD : 0xAB5; }
int main()
{
    TestSignals::pink (L[0], R[0], sr, secs, -18); TestSignals::drums (L[1], R[1], sr, secs); TestSignals::vocal (L[2], R[2], sr, secs);
    for (int k = 0; k < 3; ++k) { const size_t n = (L[k].size() / 256) * 256; L[k].resize (n); R[k].resize (n); ein[k] = energy (L[k], R[k], from); }
    for (auto& row : choirRows)
        render<HiveChoir::Processor> ("Hive Choir", row, [] (HiveChoir::Params& p, const E2Row& r)
        {
            const float* v = r.v; const int* pr = r.pr;   // props: mode1-4 engine voiceType autoKeyOn keyRoot scale seed
            p.engine = clamp (pr[4], 0, 1); p.voiceType = clamp (pr[5], 0, 5); p.keyRoot = clamp (pr[7], 0, 11); p.scaleMask = maskFor (pr[8]); p.seed = (uint32_t) std::max (1, pr[9]);
            for (int k = 0; k < 4; ++k)
            {
                auto& x = p.v[k]; const float* b = v + k * 11;
                x.mode = clamp (pr[k], 0, (int) HiveChoir::numModes - 1); x.interval = b[0]; x.levelDb = b[1]; x.pan = b[2] / 100.0f; x.throatSt = b[3]; x.unison = clamp ((int) std::lround (b[4]), 1, 32);
                x.pitchVar = b[5]; x.timeVar = b[6]; x.timbreVar = b[7] / 100.0f; x.spread = b[8] / 100.0f; x.vibHz = b[9]; x.vibCents = b[10];
            }
            const float* g = v + 44;
            p.leadDb = g[0]; p.humanCents = g[1]; p.humanMs = g[2]; p.consonants = g[3] / 100.0f; p.width = g[4] / 100.0f; p.mix = g[5] / 100.0f; p.outputDb = 0;
        }, false);
    for (auto& row : dblRows)
        render<DoubleWing2::Processor> ("Double Wing 2", row, [] (DoubleWing2::Params& p, const E2Row& r)
        {
            const float* v = r.v; const int* pr = r.pr;
            p.doubles = clamp (pr[0], 1, 4); p.voiceRange = clamp (pr[1], 0, 5); p.vibStraightOnly = pr[2] != 0; p.seed = (uint32_t) std::max (1, pr[3]);
            p.pitchVar = v[0]; p.timeVar = v[1]; p.timbreVar = v[2] / 100.0f; p.spread = v[3] / 100.0f; p.vibHz = v[4]; p.vibCents = v[5]; p.doublesDb = v[6]; p.leadDb = v[7]; p.width = v[8] / 100.0f; p.outputDb = 0;
        }, false);
    static const int reps[7] = { 2, 3, 4, 6, 8, 12, 16 };
    for (auto& row : mutRows)
        render<Mutant::Processor> ("Mutant", row, [] (Mutant::Params& p, const E2Row& r)
        {
            const float* v = r.v; const int* pr = r.pr;
            p.engine = clamp (pr[0], 0, 1); p.voiceType = clamp (pr[1], 0, 5); p.alienDiv = pr[2]; p.stutterDiv = pr[3]; p.stutterRepeats = reps[clamp (pr[4], 0, 6)]; p.driveVoicing = pr[5]; p.seed = (uint32_t) std::max (1, pr[6]);
            p.pitchSt = v[0]; p.throatSt = v[1]; p.ringMix = v[2] / 100.0f; p.ringRatio = v[3]; p.ringFallbackHz = v[4]; p.alien = v[5] / 100.0f; p.alienGlideMs = v[6]; p.stutter = v[7] / 100.0f;
            p.drive = v[8] / 100.0f; p.mix = v[9] / 100.0f; p.outputDb = 0;
        }, true);
}
