// Renders every generated Siren Comp / Siren EQ / Hush De-Ess 2 preset through the real DSP (SqVoice111Dsp.h) and
// prints the Output trim that holds the level on the material it was made for (the sung vocal; Instrument presets on
// pink noise). scripts/gen_presets_111v.py reads it: TRIM <tab> plug-in <tab> name <tab> trim dB <tab> spread dB
#include "SqVoice111Dsp.h"
#include "presets111v_table.h"
#include <cstdio>
#include <string>
using namespace Eng;
static double energy (const std::vector<float>& l, const std::vector<float>& r, size_t from)
{ double e = 0; for (size_t i = from; i < l.size(); ++i) e += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]); return e / (double) (l.size() - from); }
static int target (const char* name) { return std::string (name).rfind ("Instrument", 0) == 0 ? 0 : 2; }
int main()
{
    const double sr = 48000, secs = 3.0; const size_t from = (size_t) (sr * 1.0);
    std::vector<float> L[3], R[3]; TestSignals::pink (L[0], R[0], sr, secs, -18); TestSignals::drums (L[1], R[1], sr, secs); TestSignals::vocal (L[2], R[2], sr, secs);
    double ein[3]; for (int k = 0; k < 3; ++k) ein[k] = energy (L[k], R[k], from);
    auto report = [&] (const char* plug, const char* name, double* d, float pk, bool finite)
    {
        const double lo = std::min (d[0], std::min (d[1], d[2])), hi = std::max (d[0], std::max (d[1], d[2]));
        std::printf ("TRIM\t%s\t%s\t%.2f\t%.2f\n", plug, name, -d[target (name)], hi - lo);
        std::fprintf (stderr, "%-14s %-44s pink %+6.2f drums %+6.2f vocal %+6.2f  peak %+5.1f dBFS%s\n", plug, name, d[0], d[1], d[2], gainToDb (pk), finite ? "" : "  NaN!");
    };
    for (const auto& row : compPresets)
    {
        double d[3]; float pk = 0; bool finite = true;
        for (int k = 0; k < 3; ++k)
        {
            Voice::Comp::Processor c; auto& p = c.p;
            p.s1.model = row.m1; p.s1.fetIdx = row.fet1; p.s1.thresholdDb = row.thr1; p.s1.attackMs = row.atk1; p.s1.releaseMs = row.rel1; p.s1.ratio = row.ratio1; p.s1.kneeDb = row.knee;
            p.s2.model = row.m2; p.s2.fetIdx = row.fet2; p.s2.thresholdDb = row.thr2; p.s2.attackMs = row.atk2; p.s2.releaseMs = row.rel2; p.s2.ratio = row.ratio2; p.s2.kneeDb = row.knee;
            p.dual = row.dual; p.warm = row.warm / 100.0f; p.mix = row.mix / 100.0f; p.makeupDb = 0; p.autoMakeup = false; p.outputDb = 0;
            p.punch = row.punch; p.punchGainDb = row.pgain; p.punchImpact = row.impact / 100.0f; p.punchCeilingDb = row.ceil; p.duckDepthDb = row.duck;
            c.prepare (sr, 256);
            auto l = L[k], r = R[k];
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; c.process (ch, 2, nullptr, 256); }
            for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        report ("Siren Comp", row.name, d, pk, finite);
    }
    for (const auto& row : eqPresets)
    {
        double d[3]; float pk = 0; bool finite = true;
        for (int k = 0; k < 3; ++k)
        {
            Voice::Eq::Processor e; auto& p = e.p;
            for (int b = 0; b < 6; ++b)
            {
                const auto& br = row.b[b]; auto& bd = p.band[(size_t) b];
                bd.on = br.on; bd.type = br.type; bd.mode = br.mode; bd.harmonic = br.harm; bd.freq = br.freq; bd.gainDb = br.gain; bd.q = br.q; bd.thresholdDb = br.thr; bd.rangeDb = br.range; bd.expand = br.expand;
            }
            p.hpHz = row.hp; p.hpSlope = row.hps; p.lpHz = row.lp; p.lpSlope = row.lps; p.airHz = row.airf; p.airDb = row.air; p.tiltDb = row.tilt;
            e.prepare (sr, 256);
            auto l = L[k], r = R[k];
            // the pitch the app's voice core would report: the generated vocal sings A3..E4; pink and drums have none
            static const float notes[] = { 57, 59, 60, 62, 64, 62, 60, 59 };
            for (size_t i = 0; i + 256 <= l.size(); i += 256)
            {
                const double t = i / sr; const int ni = (int) (t / 0.5) % 8;
                e.setPitch (k == 2 ? 440.0f * std::pow (2.0f, (notes[ni] - 69) / 12.0f) : 0.0f);
                float* ch[2] = { l.data() + i, r.data() + i }; e.process (ch, 2, 256);
            }
            for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        report ("Siren EQ", row.name, d, pk, finite);
    }
    for (const auto& row : dePresets)
    {
        double d[3]; float pk = 0; bool finite = true;
        for (int k = 0; k < 3; ++k)
        {
            Voice::DeEss::Processor x; auto& p = x.p;
            p.splitHz = row.split; p.hardThresholdDb = row.hthr; p.softThresholdDb = row.sthr; p.hardRangeDb = row.hr; p.softRangeDb = row.sr; p.sensitivity = row.sens / 100.0f; p.link = row.link;
            if (p.link) { p.softThresholdDb = p.hardThresholdDb - 4; p.softRangeDb = p.hardRangeDb + 2; }
            x.prepare (sr, 256);
            auto l = L[k], r = R[k];
            for (size_t i = 0; i + 256 <= l.size(); i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; x.process (ch, 2, 256); }
            for (size_t i = 0; i < l.size(); ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); if (i > from) pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
            d[k] = 10 * std::log10 (energy (l, r, from) / ein[k]);
        }
        report ("Hush De-Ess 2", row.name, d, pk, finite);
    }
}
