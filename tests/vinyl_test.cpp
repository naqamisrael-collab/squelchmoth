// Known-answer test for SqVinyl (vinyl pre-master checks and line-up tones). No dependencies.
#include <cstdio>
#include <complex>
#include "SqVinyl.h"
using namespace SqVinyl;
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static char line[900]; static const double fs = 48000.0;
struct Mix { std::vector<float> l, r; };
/** 20 s: bass at 60 Hz (side: how much of it differs between the channels, -1 = opposite), mids, and optional "s" bursts. */
static Mix make (double bassL, double bassR, bool ess, double sub = 0.0, double seconds = 20.0, double tilt = 0.0)
{
    const int n = (int) (seconds * fs); Mix m; m.l.resize ((size_t) n); m.r.resize ((size_t) n); unsigned st = 99;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs, bass = 0.3 * std::sin (2 * pi * 60.0 * t), mid = 0.1 * std::sin (2 * pi * 440.0 * t) + 0.05 * std::sin (2 * pi * 1500.0 * t), wide = 0.05 * std::sin (2 * pi * 3000.0 * t);
        st = st * 1664525u + 1013904223u; const double nz = (double) (st >> 8) / 8388608.0 - 1.0; static double h1 = 0, h2 = 0; h1 += 0.5 * (nz - h1); const double hi = nz - h1; h2 += 0.6 * (hi - h2);
        const double burst = ess && std::fmod (t, 1.0) < 0.12 ? 3.0 * h2 : 0.0, gain = 1.0 + tilt * (t / seconds - 0.5), s = sub * std::sin (2 * pi * 18.0 * t);
        m.l[(size_t) i] = (float) (gain * (bassL * bass + mid + wide + burst + s)); m.r[(size_t) i] = (float) (gain * (bassR * bass + mid - wide + burst + s));
    }
    return m;
}
static bool flagged (const std::vector<Finding>& f, const char* word) { for (auto& x : f) if (x.problem && x.text.find (word) != std::string::npos) return true; return false; }
int main()
{
    std::printf ("== Bass and phase ==\n");
    {
        auto a = make (1, 1, false); const auto ra = analyze (a.l.data(), a.r.data(), (int) a.l.size(), fs); auto b = make (1, 0, false); const auto rb = analyze (b.l.data(), b.r.data(), (int) b.l.size(), fs); auto c = make (1, -1, false); const auto rc = analyze (c.l.data(), c.r.data(), (int) c.l.size(), fs);
        std::snprintf (line, sizeof (line), "bass the same in both channels: its difference is %.0f dB under what they share; bass in the left only: %.2f dB (equal, as it must be); bass opposite: %.0f dB over", -ra.bassSideDb, rb.bassSideDb, rc.bassSideDb);
        check (ra.bassSideDb < -40 && std::abs (rb.bassSideDb) < 0.1 && rc.bassSideDb > 40, line);
        check (! flagged (advise (ra, Format::lp33), "Bass") && flagged (advise (rb, Format::lp33), "Bass") && flagged (advise (rc, Format::lp33), "Bass"), "centred bass passes; bass on one side or opposite is called out");
        std::snprintf (line, sizeof (line), "left-right agreement: %.2f centred, %.2f with opposite bass (opposite %.0f %% of the time) - called out", ra.worstMatch, rc.worstMatch, rc.oppositeShare * 100); check (ra.worstMatch > 0.8 && rc.worstMatch < -0.5 && rc.oppositeShare > 0.9 && flagged (advise (rc, Format::lp33), "Out-of-phase") && ! flagged (advise (ra, Format::lp33), "Out-of-phase"), line);
    }
    std::printf ("== Highs, sub, order ==\n");
    {
        auto a = make (1, 1, false); const auto ra = analyze (a.l.data(), a.r.data(), (int) a.l.size(), fs); auto e = make (1, 1, true); const auto re = analyze (e.l.data(), e.r.data(), (int) e.l.size(), fs);
        std::snprintf (line, sizeof (line), "with bursts of hiss like a hard \"s\" every second: the 5 to 10 kHz band peaks at %.1f dBFS, %.1f dB over its average (without: %.1f dBFS) - called out", re.essDb, re.essOverDb, ra.essDb);
        check (flagged (advise (re, Format::lp33), "Hot highs") && ! flagged (advise (ra, Format::lp33), "Hot highs") && re.essDb > ra.essDb + 20, line);
        auto s = make (1, 1, false, 0.3); const auto rs = analyze (s.l.data(), s.r.data(), (int) s.l.size(), fs);
        std::snprintf (line, sizeof (line), "an 18 Hz rumble as strong as the bass: %.1f dB against the whole mix (without: %.0f) - called out", rs.subDb, ra.subDb); check (flagged (advise (rs, Format::lp33), "below 30 Hz") && ! flagged (advise (ra, Format::lp33), "below 30 Hz"), line);
        auto t = make (1, 1, false, 0.0, 20.0, 1.2); const auto rt = analyze (t.l.data(), t.r.data(), (int) t.l.size(), fs); bool said = false; for (auto& f : advise (rt, Format::lp33)) if (f.text.find ("loudest, brightest songs first") != std::string::npos) said = true;
        std::snprintf (line, sizeof (line), "a side that gets louder as it goes (second half %.1f dB up) is told to put its loudest songs first", -rt.frontMinusBackDb); check (said && rt.frontMinusBackDb < -2.0, line);
    }
    std::printf ("== Side length (a pressing plant's published figures) ==\n");
    {
        auto verdict = [] (double minutes, Format f) { Report r; r.seconds = minutes * 60.0; r.worstMatch = 1; const auto a = advise (r, f)[0]; return std::string (a.problem ? "problem: " : "ok: ") + a.text.substr (0, a.text.find (':')); };
        Report r; auto prob = [&] (double minutes, Format f) { r = {}; r.seconds = minutes * 60.0; return advise (r, f)[0].problem; };
        check (! prob (17, Format::lp33) && ! prob (19.5, Format::lp33) && prob (21, Format::lp33) && prob (23, Format::lp33) && ! prob (11, Format::lp45) && ! prob (13.5, Format::lp45) && prob (14.5, Format::lp45) && prob (16, Format::lp45) && ! prob (4.4, Format::seven45) && prob (4.7, Format::seven45) && ! prob (5.9, Format::seven33) && prob (6.2, Format::seven33),
               "12-inch at 33: 18 ideal, 20 still good, 22 trouble; at 45: 12, 14, 15; 7-inch: 4.5 at 45, 6 at 33 - each side of each line gets the right verdict");
        (void) verdict;
    }
    std::printf ("== Line-up tones ==\n");
    {
        const auto x = tone (1000.0, 2.0, -18.0, fs); std::complex<double> a = 0; double pk = 0; for (int i = 24000; i < 72000; ++i) { a += (double) x[(size_t) i] * std::polar (1.0, -2 * pi * 1000.0 * i / fs); pk = std::max (pk, (double) std::abs (x[(size_t) i])); }
        std::snprintf (line, sizeof (line), "a 1 kHz tone asked for at -18 dBFS measures %.3f dBFS (peak %.3f); it starts and ends at zero", 20 * std::log10 (2 * std::abs (a) / 48000.0), 20 * std::log10 (pk)); check (std::abs (20 * std::log10 (2 * std::abs (a) / 48000.0) + 18.0) < 0.01 && x.front() == 0.0f && std::abs (x.back()) < 1e-3, line);
        const auto p = pink (8.0, -20.0, fs); double e = 0; for (float v : p) e += (double) v * v; const double rms = 10 * std::log10 (e / p.size());
        Biquad lo1, hi1, lo2, hi2; hi1.set (100, fs, true); lo1.set (200, fs, false); hi2.set (3200, fs, true); lo2.set (6400, fs, false); double e1 = 0, e2 = 0; for (float v : p) { const double a1 = lo1.run (hi1.run (v)), a2 = lo2.run (hi2.run (v)); e1 += a1 * a1; e2 += a2 * a2; }
        std::snprintf (line, sizeof (line), "pink noise asked for at -20 dBFS rms measures %.2f; the octave from 100 Hz and the octave from 3.2 kHz carry the same level (%.2f dB apart); the same seed gives the same noise", rms, 10 * std::log10 (e1 / e2));
        check (std::abs (rms + 20.0) < 0.05 && std::abs (10 * std::log10 (e1 / e2)) < 0.7 && pink (1.0, -20.0, fs)[1000] == pink (1.0, -20.0, fs)[1000], line);
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
