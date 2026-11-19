// Squelchmoth 1.32 - cutting the picture: known answers worked by hand.
#include "SqCut.h"
#include <cstdio>
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static bool near (double a, double b, double tol = 1e-9) { return std::abs (a - b) <= tol; }
int main()
{
    using namespace SqCut;
    std::printf ("== Cut ==\n");
    const double V = 10.0;                                   // a ten-second video
    const std::vector<Piece> whole { { 2.0, 10.0, 0.0 } };   // placed at 2 s
    { const auto h = lookup (whole, 5.0, 0, V); check (h.piece == 0 && near (h.src, 3.0), "one whole piece at 2 s: session 5 s shows the video's 3 s"); }
    check (! lookup (whole, 1.99, 0, V).shows() && ! lookup (whole, 12.0, 0, V).shows() && lookup (whole, 2.0, 0, V).shows() && near (lookup (whole, 11.999, 0, V).src, 9.999), "nothing before the piece or at its end; the start is included");
    check (isWhole (whole, 0, V) && ! isWhole (whole, 0.1, V) && ! isWhole ({ { 2, 10, 1 } }, 0, V) && ! isWhole ({ { 2, 4, 0 }, { 6, 6, 4 } }, 0, V) && ! isWhole ({ { 2, 5, 0 } }, 0, V), "untouched means one piece, from the video's start, whole, no slip");

    // split at 6 s, then the second half moved 3 s later: a gap from 6 to 9
    const std::vector<Piece> gap { { 2.0, 4.0, 0.0 }, { 9.0, 6.0, 4.0 } };
    check (near (lookup (gap, 5.5, 0, V).src, 3.5) && ! lookup (gap, 7.0, 0, V).shows() && lookup (gap, 9.0, 0, V).piece == 1 && near (lookup (gap, 9.0, 0, V).src, 4.0) && near (lookup (gap, 14.5, 0, V).src, 9.5), "after a split and a move: 5.5 s shows 3.5 s, 7 s is a gap, 9 s shows 4 s, 14.5 s shows 9.5 s");
    check (near (start (gap), 2.0) && near (end (gap), 15.0) && near (end ({}), 0.0), "the picture runs from 2 s to 15 s");
    check (near (whereIs (gap, 3.5, 0, V), 5.5) && near (whereIs (gap, 4.0, 0, V), 9.0) && near (whereIs (gap, 9.5, 0, V), 14.5), "where a moment of the video plays: 3.5 s at 5.5 s, 4 s at 9 s");

    // the middle cut out and closed up (ripple): video 3..7 s removed
    const std::vector<Piece> ripple { { 0.0, 3.0, 0.0 }, { 3.0, 3.0, 7.0 } };
    check (near (lookup (ripple, 2.9, 0, V).src, 2.9) && near (lookup (ripple, 3.0, 0, V).src, 7.0) && near (lookup (ripple, 5.9, 0, V).src, 9.9) && whereIs (ripple, 5.0, 0, V) < 0, "a section cut out and closed up: 3 s jumps to the video's 7 s, and the cut-out moment plays nowhere");

    // overlap: the later piece in the list wins
    const std::vector<Piece> over { { 0.0, 6.0, 0.0 }, { 4.0, 3.0, 8.0 } };
    check (lookup (over, 3.9, 0, V).piece == 0 && lookup (over, 4.0, 0, V).piece == 1 && near (lookup (over, 5.0, 0, V).src, 9.0) && ! lookup (over, 6.5, 0, V).shows(), "where two pieces overlap the later one shows; past the video's end (10 s) there is no picture");
    check (near (whereIs (over, 5.0, 0, V), -1.0) && near (whereIs (over, 8.5, 0, V), 4.5), "a moment hidden under an overlap plays nowhere");

    // slip: the picture half a second later than its sound
    check (near (lookup (whole, 5.0, 0.5, V).src, 2.5) && ! lookup (whole, 2.2, 0.5, V).shows() && near (whereIs (whole, 2.5, 0.5, V), 5.0), "slip 0.5 s: 5 s shows the video's 2.5 s, and the first half second of the piece has no picture yet");
    // speed
    { const std::vector<Piece> fast { { 0.0, 4.0, 1.0, 2.0 } }; check (near (lookup (fast, 1.5, 0, V).src, 4.0) && hasSpeedChange (fast) && ! hasSpeedChange (gap), "a piece at double speed: 1.5 s in shows 3 s further into the video"); }

    // export plans
    {
        const auto p = plan (gap, 0, V, 0.0, 16.0);
        check (p.size() == 5 && ! p[0].picture && near (p[0].length, 2.0) && p[1].picture && near (p[1].src, 0.0) && near (p[1].length, 4.0) && ! p[2].picture && near (p[2].length, 3.0)
               && p[3].picture && near (p[3].src, 4.0) && near (p[3].length, 6.0) && ! p[4].picture && near (p[4].length, 1.0),
               "export plan 0 to 16 s: black 2 s, video 0-4 s, black 3 s, video 4-10 s, black 1 s");
        double total = 0; for (auto& s : p) total += s.length; check (near (total, 16.0), "the plan has no holes: its stretches add up to the range");
        const auto part = plan (gap, 0, V, 5.0, 10.0);
        check (part.size() == 3 && part[0].picture && near (part[0].src, 3.0) && near (part[0].length, 1.0) && ! part[1].picture && near (part[1].length, 3.0) && part[2].picture && near (part[2].src, 4.0) && near (part[2].length, 1.0),
               "a plan for part of the session (5 to 10 s) starts inside a piece: video from 3 s for 1 s, black 3 s, video from 4 s for 1 s");
        const auto w = plan (whole, 0, V, 2.0, 12.0);
        check (w.size() == 1 && w[0].picture && near (w[0].src, 0.0) && near (w[0].length, 10.0), "an untouched video is one stretch");
        const auto r = plan (ripple, 0, V, 0.0, 6.0);
        check (r.size() == 2 && near (r[0].src, 0.0) && near (r[0].length, 3.0) && near (r[1].src, 7.0) && near (r[1].length, 3.0), "a closed-up cut is two stretches of video, no black");
        const auto o = plan (over, 0, V, 0.0, 7.0);
        check (o.size() == 3 && near (o[0].length, 4.0) && near (o[1].src, 8.0) && near (o[1].length, 2.0) && ! o[2].picture && near (o[2].length, 1.0), "an overlap running past the video's end: 4 s of the first piece, 2 s of the second (to the video's end), then black");
        const auto sl = plan (whole, 0.5, V, 2.0, 12.0);
        check (sl.size() == 2 && ! sl[0].picture && near (sl[0].length, 0.5) && sl[1].picture && near (sl[1].src, 0.0) && near (sl[1].length, 9.5), "with slip the plan starts with black for the slip, then the video from its start");
        // two pieces that simply carry on (a split with nothing moved) are one stretch
        const auto j = plan ({ { 2.0, 4.0, 0.0 }, { 6.0, 6.0, 4.0 } }, 0, V, 2.0, 12.0);
        check (j.size() == 1 && near (j[0].length, 10.0), "a split with nothing moved exports as one stretch");
        check (plan ({}, 0, V, 0.0, 3.0).size() == 1 && ! plan ({}, 0, V, 0.0, 3.0)[0].picture && plan (gap, 0, V, 5.0, 5.0).empty(), "no pieces is all black; an empty range is no plan");
    }
    {
        // 0.5 s + 0.26 s + 0.25 s at 25 fps: edges at frames 12.5, 19, 25.25 -> 13 (rounded), 19, 25: counts 13, 6, 6 = 25 (1.01 s is 25 frames)
        const std::vector<Stretch> p { { 4.0, 0.5, 0, true }, { 4.5, 0.26, 0, false }, { 4.76, 0.25, 1, true } };
        const auto c = frameCounts (p, 25.0);
        check (c.size() == 3 && c[0] == 13 && c[1] == 6 && c[2] == 6, "frames per stretch at 25 fps meet on whole frames: 13, 6, 6 for stretches of 0.5, 0.26 and 0.25 s");
        std::vector<Stretch> many; for (int i = 0; i < 1000; ++i) many.push_back ({ i * 0.033, 0.033, 0, true });
        long long total = 0; for (auto n : frameCounts (many, 30.0)) total += n;
        check (total == 990 && frameCounts ({}, 25.0).empty(), "a thousand cuts of 33 ms at 30 fps still total 990 frames (33 s): no drift");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
