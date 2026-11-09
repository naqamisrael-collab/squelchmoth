// Renders every generated Vellum EQ / Tremor / Orbit Pan / Mirror Shift (+ Lite) / Shutter Delay (+ Lite) preset through the
// real DSP (SqColor111bDsp.h) on the three audit signals and prints the Output trim that holds the level on the material
// the preset was made for. scripts/gen_presets_111d.py reads it: TRIM <tab> plug-in <tab> name <tab> trim dB <tab> peak dBFS
// The parameter mapping is the same as the plug-in wrappers' (Source/SqColor111b.h); like the in-app audit, the transport
// plays at 120 BPM from bar 1.
#include "SqColor111bDsp.h"
#include "presets111d_table.h"
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

template <typename Proc, typename Fill, typename Run>
static void render (const char* plug, const D1Row& row, Fill fill, Run run)
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
    const double trim = -d[target (row.name)];
    std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", plug, row.name, trim, gainToDb (pk));
    std::fprintf (stderr, "%-18s %-40s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", plug, row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
}

int main()
{
    TestSignals::pink (L[0], R[0], sr, secs, -18); TestSignals::drums (L[1], R[1], sr, secs); TestSignals::vocal (L[2], R[2], sr, secs);
    for (int k = 0; k < 3; ++k) ein[k] = energy (L[k], R[k], from);
    // Vellum: lowGain midGain mid2Freq mid2Gain highGain drive output | lowFreq midFreq highFreq mid2On steppedOn target oversample
    for (auto& row : vellumRows)
        render<Vellum::Processor> ("Vellum EQ", row, [] (Vellum::Params& p, const D1Row& r)
        {
            p.lowDb = r.v[0]; p.midDb = r.v[1]; p.mid2Hz = r.v[2]; p.mid2Db = r.v[3]; p.highDb = r.v[4]; p.drive = r.v[5] / 100.0f; p.outputDb = 0;
            p.lowIdx = r.pr[0]; p.midIdx = r.pr[1]; p.highIdx = r.pr[2]; p.mid2On = r.pr[3]; p.stepped = r.pr[4]; p.target = r.pr[5]; p.oversampleLog2 = r.pr[6];
        }, [] (Vellum::Processor& p, float** ch, double) { p.process (ch, 2, 256); });
    // Tremor: depth rate feel groove accent smoothing width threshold attack release rateMod rateModHz drive stereo xover mix output
    //   | mode shape syncOn division stepDivision numSteps dynMode analog harmonicOn keyOn retrigOn pattern
    for (auto& row : tremorRows)
        render<Tremor::Processor> ("Tremor", row, [] (Tremor::Params& p, const D1Row& r)
        {
            p.depth = r.v[0] / 100.0f; p.rateHz = r.v[1]; p.feel = r.v[2] / 100.0f; p.groove = r.v[3] / 100.0f; p.accent = r.v[4] / 100.0f; p.smoothMs = r.v[5]; p.pulseWidth = r.v[6] / 100.0f;
            p.thresholdDb = r.v[7]; p.attackMs = r.v[8]; p.releaseMs = r.v[9]; p.rateModAmt = r.v[10] / 100.0f; p.rateModHz = r.v[11]; p.analogAmount = r.v[12] / 100.0f;
            p.stereoDeg = r.v[13]; p.xoverHz = r.v[14]; p.mix = r.v[15] / 100.0f; p.outputDb = 0;
            p.mode = r.pr[0]; p.shape = r.pr[1]; p.sync = r.pr[2]; p.syncIndex = r.pr[3]; p.stepDivision = r.pr[4]; p.numSteps = r.pr[5]; p.dyn = r.pr[6]; p.analog = r.pr[7];
            p.harmonic = r.pr[8]; p.keyOn = r.pr[9]; p.retrigger = r.pr[10];
            const auto& pat = Tremor::patterns()[(size_t) clamp (r.pr[11], 0, (int) Tremor::patterns().size() - 1)];
            for (int i = 0; i < 16; ++i) p.steps[i] = pat.v[i];
        }, [] (Tremor::Processor& p, float** ch, double ppq) { p.setTransport (120, ppq, true); p.process (ch, 2, nullptr, 256); });
    // Orbit: rate offset width smoothing depth haas threshold detHp detLp sourceWidth mix output
    //   | mode shape syncOn division stepDivision numSteps triggerOn divider binauralOn panLaw pattern
    for (auto& row : orbitRows)
        render<Orbit::Processor> ("Orbit Pan", row, [] (Orbit::Params& p, const D1Row& r)
        {
            p.rateHz = r.v[0]; p.offsetDeg = r.v[1]; p.widthDeg = r.v[2]; p.smoothMs = r.v[3]; p.depth = r.v[4] / 100.0f; p.haasMs = r.v[5]; p.trigThreshDb = r.v[6];
            p.detHpHz = r.v[7]; p.detLpHz = r.v[8]; p.sourceWidth = r.v[9] / 100.0f; p.mix = r.v[10] / 100.0f; p.outputDb = 0;
            p.mode = r.pr[0]; p.shape = r.pr[1]; p.sync = r.pr[2]; p.syncIndex = r.pr[3]; p.stepDivision = r.pr[4]; p.numSteps = r.pr[5]; p.audioTrigger = r.pr[6];
            p.divider = std::max (1, r.pr[7]); p.binaural = r.pr[8]; p.law = r.pr[9] > 0 ? r.pr[9] - 1 : Orbit::law3dB;
            const auto& pat = Orbit::patterns()[(size_t) clamp (r.pr[10], 0, (int) Orbit::patterns().size() - 1)];
            for (int i = 0; i < 16; ++i) p.steps[i] = pat.v[i];
        }, [] (Orbit::Processor& p, float** ch, double ppq) { p.setTransport (120, ppq, true); p.process (ch, 2, nullptr, 256); });
    // Mirror: detune delay focus mix duck duckThresh output | style keyOn
    for (auto& row : mirrorRows)
        render<Mirror::Processor> ("Mirror Shift", row, [] (Mirror::Params& p, const D1Row& r)
        {
            p.detune = r.v[0] / 100.0f; p.delay = r.v[1] / 100.0f; p.focusHz = r.v[2]; p.mix = r.v[3] / 100.0f; p.duckDb = r.v[4]; p.duckThreshDb = r.v[5]; p.outputDb = 0; p.style = r.pr[0];
        }, [] (Mirror::Processor& p, float** ch, double) { p.process (ch, 2, nullptr, 256); });
    // Mirror Lite: amount mix output | style
    for (auto& row : mirrorLiteRows)
        render<Mirror::Processor> ("Mirror Shift Lite", row, [] (Mirror::Params& p, const D1Row& r)
        {
            p.style = r.pr[0]; p.detune = p.delay = r.v[0] / 100.0f; p.focusHz = 20; p.mix = r.v[1] / 100.0f; p.outputDb = 0;
        }, [] (Mirror::Processor& p, float** ch, double) { p.process (ch, 2, nullptr, 256); });
    // Shutter: timeA timeB feedbackA feedbackB mix lfoRate lfoDepth input output | multiply algo bits syncA syncB freezeOn smoothOn
    for (auto& row : shutterRows)
        render<Shutter::Processor> ("Shutter Delay", row, [] (Shutter::Params& p, const D1Row& r)
        {
            p.timeAMs = std::max (1.0f, r.v[0]); p.timeBMs = std::max (1.0f, r.v[1]); p.feedbackA = r.v[2] / 100.0f; p.feedbackB = r.v[3] / 100.0f; p.mix = r.v[4] / 100.0f;
            p.lfoHz = r.v[5]; p.lfoDepth = r.v[6] / 100.0f; p.inputDb = r.v[7]; p.outputDb = 0;
            p.multiply = r.pr[0]; p.algo = r.pr[1]; p.bits = r.pr[2]; p.syncA = r.pr[3] - 1; p.syncB = r.pr[4] - 1; p.freeze = r.pr[5]; p.smooth = r.pr[6];
        }, [] (Shutter::Processor& p, float** ch, double) { p.setTempo (120); p.process (ch, 2, 256); });
    // Shutter Lite: time feedback mix output | multiply
    for (auto& row : shutterLiteRows)
        render<Shutter::Processor> ("Shutter Lite", row, [] (Shutter::Params& p, const D1Row& r)
        {
            p.algo = Shutter::classic; p.timeAMs = p.timeBMs = r.v[0]; p.feedbackA = r.v[1] / 100.0f; p.mix = r.v[2] / 100.0f; p.outputDb = 0; p.multiply = r.pr[0]; p.bits = 1;
        }, [] (Shutter::Processor& p, float** ch, double) { p.setTempo (120); p.process (ch, 2, 256); });
}
