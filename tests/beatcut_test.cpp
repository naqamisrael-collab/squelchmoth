// Squelchmoth 1.35 - cut to the beat: known answers worked by hand.
#include "SqBeatCut.h"
#include <cstdio>
#include <set>
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static bool near (double a, double b, double tol = 1e-9) { return std::abs (a - b) <= tol; }
int main()
{
    using namespace SqBeatCut;
    std::printf ("== Beat cut ==\n");
    // 120 BPM: a beat is half a second. A 4-second video at 0, cut every beat: 8 slots.
    std::vector<double> grid; for (int i = 0; i <= 8; ++i) grid.push_back (i * 0.5);
    {
        auto s = slots (grid); assign (s, keep, 0.0, 4.0);
        bool ok = s.size() == 8; for (size_t i = 0; i < s.size(); ++i) ok = ok && near (s[i].at, i * 0.5) && near (s[i].length, 0.5) && near (s[i].src, i * 0.5);
        check (ok, "keep: eight half-second slots, each showing its own moment (plain cuts on the grid)");
    }
    {
        auto s = slots (grid); assign (s, reverse, 0.0, 4.0);
        check (near (s[0].src, 3.5) && near (s[1].src, 3.0) && near (s[7].src, 0.0), "reverse: the first slot shows the last half second (3.5 s), the last slot the first");
    }
    {
        auto s = slots (grid); assign (s, keep, 2.0, 4.0);       // the video starts at 2 s of the session: slots before it have no moment of their own
        check (near (s[0].src, 0.0) && near (s[3].src, 0.0) && near (s[4].src, 0.0) && near (s[5].src, 0.5) && near (s[7].src, 1.5), "keep with the video starting at 2 s: slots before it are held at the video's start");
    }
    {
        // spread: a 60-second video condensed into the same 8 slots: starts at 0, 8.5, 17, ... 59.5
        auto s = slots (grid); assign (s, spread, 0.0, 60.0);
        check (near (s[0].src, 0.0) && near (s[1].src, 59.5 / 7.0) && near (s[7].src, 59.5) && near (s[7].src + s[7].length, 60.0), "spread: stretches taken evenly through a 60 s video; the last ends exactly at its end");
        auto one = slots ({ 0.0, 2.0 }); assign (one, spread, 0.0, 60.0); check (one.size() == 1 && near (one[0].src, 0.0), "spread with one slot starts at the beginning");
    }
    {
        auto s = slots (grid); assign (s, reverse, 0.0, 3.2);    // the video is shorter than the range: no stretch may run past its end
        bool inside = true; for (auto& x : s) inside = inside && x.src >= 0 && x.src + x.length <= 3.2 + 1e-9;
        check (inside && near (s[0].src, 2.7), "a stretch never runs past the video's end: starts are pulled back (3.5 s becomes 2.7 s in a 3.2 s video)");
        auto tiny = slots (grid); assign (tiny, keep, 0.0, 0.3); bool z = true; for (auto& x : tiny) z = z && near (x.src, 0.0);
        check (z, "a video shorter than one slot always starts at its beginning");
    }
    {
        const auto p = shuffled (8, 7), q = shuffled (8, 7), r = shuffled (8, 8);
        std::set<int> all (p.begin(), p.end()); bool moved = true; for (int i = 0; i < 8; ++i) moved = moved && p[(size_t) i] != i;
        check (all.size() == 8 && *all.begin() == 0 && *all.rbegin() == 7 && moved && p == q && p != r, "shuffle: every stretch used once, none left in its own slot, the same seed gives the same order, another seed another");
        bool always = true; for (std::uint32_t seed = 0; seed < 500; ++seed) for (int n : { 3, 4, 5, 16 }) { const auto x = shuffled (n, seed); for (int i = 0; i < n; ++i) always = always && x[(size_t) i] != i; }
        check (always && shuffled (2, 1) == std::vector<int> { 1, 0 } && shuffled (1, 1) == std::vector<int> { 0 } && shuffled (0, 1).empty(), "over 500 seeds and several sizes nothing ever stays put; two swap; one stays");
        auto s = slots (grid); assign (s, shuffle, 0.0, 4.0, 7);
        bool ok = true; for (int i = 0; i < 8; ++i) ok = ok && near (s[(size_t) i].src, p[(size_t) i] * 0.5);
        check (ok, "shuffle assigns each slot the moment of the slot its order names");
    }
    {
        // an uneven grid (a tempo change) and a sliver at the end
        const auto s = slots ({ 0.0, 0.5, 1.0, 1.75, 2.5, 2.505 });
        check (s.size() == 4 && near (s[2].length, 0.75) && near (s[3].length, 0.755) && slots ({}).empty() && slots ({ 1.0 }).empty() && slots ({ 2.0, 1.0 }).empty(), "slots follow an uneven grid; a sliver joins the slot before; no grid, no slots");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
