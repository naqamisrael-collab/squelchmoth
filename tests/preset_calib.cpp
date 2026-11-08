// Renders every generated preset through the real DSP on the three audit signals and prints the Output trim that
// centres the level changes (scripts/gen_presets_111.py reads it). TRIM <tab> name <tab> trim dB <tab> spread dB
#include "SqCinderDsp.h"
#include "SqWingbeatDsp.h"
#include "SqFoilPlateDsp.h"
#include "SqRadiantMawDsp.h"
#include "presets111_table.h"
#include <cstdio>
#include <string>
using namespace Eng;
static double energy (const std::vector<float>& l, const std::vector<float>& r, size_t from)
{ double e = 0; for (size_t i = from; i < l.size(); ++i) e += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]); return e / (double) (l.size() - from); }
int main()
{
    const double sr = 48000, secs = 3.0; const size_t from = (size_t) (sr * 1.0);
    std::vector<float> L[3], R[3]; TestSignals::pink (L[0], R[0], sr, secs, -18); TestSignals::drums (L[1], R[1], sr, secs); TestSignals::vocal (L[2], R[2], sr, secs);
    double ein[3]; for (int k = 0; k < 3; ++k) ein[k] = energy (L[k], R[k], from);
    for (const auto& row : cinderPresets)
    {
        double d[3]; float pk = 0; bool finite = true;
        for (int k = 0; k < 3; ++k)
        {
            Cinder::Processor c; auto& p = c.p;
            p.modelA = row.model; p.modelB = row.modelB; p.blend = row.blend / 100.0f; p.driveDb = row.drive; p.biasAmt = row.bias / 100.0f; p.dynamics = row.dyn / 100.0f;
            p.lowCutHz = row.lc; p.thumpDb = row.thump; p.toneDb = row.tone; p.highCutHz = row.hc; p.steep = row.steep; p.mix = row.mix / 100.0f;
            p.punish = row.punish; p.ms = row.ms; p.oversampleLog2 = row.os; p.linearPhase = row.linear; p.autoGain = row.autoGain;
            c.prepare (sr, 256);
            auto l = L[k], r = R[k];
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; c.process (ch, 2, nullptr, 256); }
            for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        // the preset holds level on the material it was made for (same rule as the in-app audit, SqEngine111.h)
        const std::string nm (row.name);
        const int t = (nm.find ("Vocal") != std::string::npos) ? 2 : (nm.find ("Drum") != std::string::npos || nm.rfind ("808", 0) == 0) ? 1 : 0;
        const double lo = std::min (d[0], std::min (d[1], d[2])), hi = std::max (d[0], std::max (d[1], d[2]));
        std::printf ("TRIM\t%s\t%.2f\t%.2f\n", row.name, -d[t], hi - lo);
        std::fprintf (stderr, "%-44s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
    }
    for (const auto& row : wingbeatPresets)
    {
        double d[3]; float pk = 0; bool finite = true;
        for (int k = 0; k < 3; ++k)
        {
            Wingbeat::Processor w; w.prepare (sr, 256); auto& p = w.p;
            p.style = row.style; p.mode = row.mode; p.syncL = row.syncL; p.syncR = row.syncR; p.timeMs = row.time; p.timeRMs = row.timeR;
            p.feedback = row.feedback / 100.0f; p.mix = row.mix / 100.0f; p.saturation = row.saturation / 100.0f; p.groove = row.groove / 100.0f; p.feelMs = row.feel;
            p.width = row.width / 100.0f; p.offsetMs = row.offset; p.accent = row.accent / 100.0f; p.warp = row.warp / 100.0f; p.lowCut = row.lowCut; p.highCut = row.highCut;
            p.wow = row.wow / 100.0f; p.flutter = row.flutter / 100.0f; p.diffusion = row.diffusion / 100.0f; p.duckDb = row.duck; p.duckReleaseMs = row.duckRelease;
            p.heads = row.heads; p.limiter = row.limiter; p.pattern = row.pattern; p.timeChange = row.timeChange; p.prime = row.prime;
            w.configure (120, true);
            auto l = L[k], r = R[k];
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; w.process (ch, 2, nullptr, 256, 120); }
            for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        const std::string nm (row.name);
        const int t = (nm.find ("Vocal") != std::string::npos) ? 2 : (nm.find ("Drum") != std::string::npos || nm.find ("Dub") == 0 || nm.find ("Rhythmic") == 0) ? 1 : 0;
        const double lo = std::min (d[0], std::min (d[1], d[2])), hi = std::max (d[0], std::max (d[1], d[2]));
        std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", row.jr ? "Wingbeat Jr." : "Wingbeat Echo", row.name, -d[t], hi - lo);
        std::fprintf (stderr, "%-48s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
    }
    for (const auto& row : foilPresets)
    {
        double d[3]; float pk = 0; bool finite = true;
        for (int k = 0; k < 3; ++k)
        {
            FoilPlate::Processor w; auto& p = w.p;
            p.archetype = row.plate; p.decayS = row.decay; p.mix = row.mix / 100.0f; p.preDelayMs = row.preDelay; p.lowCut = row.lowCut; p.highCut = row.highCut;
            p.mod = row.mod / 100.0f; p.modRateHz = row.modRate; p.width = row.width / 100.0f; p.balance = row.balance / 100.0f; p.eqLowDb = row.eqLow; p.eqHighDb = row.eqHigh;
            p.autoDecay = row.autoDecay / 100.0f; p.autoThresholdDb = row.autoThreshold; p.inputDrive = row.drive / 100.0f; p.input = row.input;
            p.lowSlope = row.lowSlope; p.highSlope = row.highSlope; p.darkening = row.darkening; p.monoIn = row.monoIn;
            w.prepare (sr, 256);
            auto l = L[k], r = R[k];
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; w.process (ch, 2, nullptr, 256); }
            for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        const std::string nm (row.name);
        const int t = (nm.find ("Vocal") != std::string::npos) ? 2 : (nm.find ("Drum") != std::string::npos || nm.find ("Snare") == 0) ? 1 : 0;
        const double lo = std::min (d[0], std::min (d[1], d[2])), hi = std::max (d[0], std::max (d[1], d[2]));
        std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", row.lite ? "Foil Plate Lite" : "Foil Plate", row.name, -d[t], hi - lo);
        std::fprintf (stderr, "%-52s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", row.name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
    }
    auto target = [] (const std::string& nm) { return (nm.find ("Vocal") != std::string::npos) ? 2 : (nm.find ("Drum") != std::string::npos || nm.rfind ("808", 0) == 0 || nm.find ("Parallel Smash | Drum") == 0) ? 1 : 0; };
    for (const auto& row : radiantPresets)
    {
        double d[3]; float pk = 0;
        for (int k = 0; k < 3; ++k)
        {
            Radiant::Processor w; auto& p = w.p;
            p.arch = row.arch; p.inputDb = row.input; p.bassDb = row.bass; p.trebleDb = row.treble; p.outputDb = row.output; p.mix = row.mix / 100.0f; p.mic = row.mic; p.noisy = row.noisy;
            p.oversampleLog2 = row.os; p.ms = row.ms; p.singleStage = row.pre; p.bias = row.bias;
            w.prepare (sr, 256);
            auto l = L[k], r = R[k];
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; w.process (ch, 2, 256); }
            for (size_t i = from; i < l.size(); ++i) pk = std::max (pk, std::abs (l[i]));
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        const double lo = std::min (d[0], std::min (d[1], d[2])), hi = std::max (d[0], std::max (d[1], d[2]));
        std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", row.pre ? "Radiant Pre" : "Radiant", row.name, -d[target (row.name)], hi - lo);
        std::fprintf (stderr, "%-52s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS\n", row.name, d[0], d[1], d[2], gainToDb (pk));
    }
    for (const auto& row : mawPresets)
    {
        double d[3]; float pk = 0;
        for (int k = 0; k < 3; ++k)
        {
            Maw::Processor w; auto& p = w.p;
            p.crush = row.crush / 100.0f; p.crunch = row.crunch / 100.0f; p.darknessHz = row.darkness; p.mix = row.mix / 100.0f; p.releaseS = row.release; p.stretch = row.stretch / 100.0f;
            p.fastRelease = row.fast; p.oversampleLog2 = row.os;
            w.prepare (sr, 256);
            auto l = L[k], r = R[k];
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; w.process (ch, 2, nullptr, 256); }
            for (size_t i = from; i < l.size(); ++i) pk = std::max (pk, std::abs (l[i]));
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        const double lo = std::min (d[0], std::min (d[1], d[2])), hi = std::max (d[0], std::max (d[1], d[2]));
        std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", row.lite ? "Maw Lite" : "Maw", row.name, -d[target (row.name)], hi - lo);
        std::fprintf (stderr, "%-52s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS\n", row.name, d[0], d[1], d[2], gainToDb (pk));
    }
}
