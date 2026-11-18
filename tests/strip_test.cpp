// Squelchmoth 1.29 - thumbnail strip, stepping and contact sheet: known answers worked by hand.
#include "SqStrip.h"
#include <cstdio>
#include <set>
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static bool near (double a, double b, double tol = 1e-9) { return std::abs (a - b) <= tol; }
int main()
{
    using namespace SqStrip;
    std::printf ("== Strip ==\n");
    check (slotsFor (852, 96) == 8 && slotsFor (50, 96) == 1 && slotsFor (100000, 96) == 400 && slotsFor (500, 0) == 1, "slots: 852 px holds 8 stills of 96; never fewer than 1, never more than 400");
    check (near (slotTime (0, 8, 4.0), 0.25) && near (slotTime (7, 8, 4.0), 3.75) && near (slotTime (3, 10, 36000.0), 12600.0), "a slot's still is its middle: 0.25 s and 3.75 s of 4 s in 8; slot 3 of 10 in ten hours is 3 h 30 min");
    check (near (slotTime (-5, 8, 4.0), 0.25) && near (slotTime (99, 8, 4.0), 3.75) && near (slotTime (0, 0, 4.0), 0.0), "slot numbers outside the strip are held to its ends");
    check (near (timeAt (426, 852, 4.0), 2.0) && near (timeAt (-10, 852, 4.0), 0.0) && near (timeAt (9999, 852, 4.0), 4.0), "the time under a pixel: the middle is 2 s; outside is held to the ends");
    check (near (xAt (timeAt (313, 852, 4.0), 852, 4.0), 313.0, 1e-9) && near (xAt (1.0, 800, 4.0), 200.0), "pixel to time and back lands on the same pixel");
    check (slotAt (0, 852, 8) == 0 && slotAt (851, 852, 8) == 7 && slotAt (426, 852, 8) == 4 && slotAt (2000, 852, 8) == 7, "the slot under a pixel");
    check (frameCount (4.0, 25.0) == 100 && frameCount (4.01, 25.0) == 101 && frameCount (0.0, 25.0) == 1, "frame count: 4 s at 25 is 100 frames; a part frame counts");
    check (near (stepFrames (1.013, 25.0, 1, 4.0), 1.04) && near (stepFrames (1.013, 25.0, -1, 4.0), 0.96) && near (stepFrames (1.013, 25.0, 0, 4.0), 1.0), "stepping from 1.013 s at 25 fps: frame 25 is on screen; +1 is 1.04 s, -1 is 0.96 s, 0 snaps to 1.00 s");
    check (near (stepFrames (0.0, 25.0, -3, 4.0), 0.0) && near (stepFrames (3.99, 25.0, 5, 4.0), 3.96) && near (stepFrames (2.0, 25.0, 25, 4.0), 3.0), "stepping stays inside the video: not before frame 0, not past frame 99; 25 frames is one second");
    check (near (stepFrames (10.0, 30000.0 / 1001.0, 1, 60.0), 300.0 * 1001.0 / 30000.0, 1e-9), "at 29.97 fps the frame after 10.0 s (frame 299) starts at 10.01 s");
    {
        const auto o = fillOrder (20); std::set<int> all (o.begin(), o.end());
        check (o.size() == 20 && all.size() == 20 && *all.begin() == 0 && *all.rbegin() == 19 && o[0] == 4 && o[1] == 12 && o[2] == 2, "fill order covers every slot once, coarse first (4, 12, then 2 ...)");
        check (fillOrder (0).empty() && fillOrder (1).size() == 1 && fillOrder (3).size() == 3, "fill order for tiny strips");
    }
    {
        const auto s = sheet (12, 4, 240, 1920, 1080);
        check (s.cols == 4 && s.rows == 3 && s.cellW == 240 && s.imageH == 134 && s.width == 6 + 4 * 246 && s.height == 6 + 3 * (134 + 18 + 6), "sheet of 12 in 4 columns from 1920 x 1080: 3 rows, stills 240 x 134");
        check (s.cellX (0) == 6 && s.cellY (0) == 6 && s.cellX (5) == 6 + 246 && s.cellY (5) == 6 + 158 && s.cellX (11) == 6 + 3 * 246 && s.cellY (11) == 6 + 2 * 158, "sheet cells sit where the grid says");
        const auto a = sheet (9, 0, 160, 64, 64);
        check (a.cols == 4 && a.rows == 3 && a.imageH == 160, "with no column count given, 9 stills make a 4-wide grid; square video keeps square stills");
        const auto one = sheet (1, 6, 100, 16, 9);
        check (one.cols == 1 && one.rows == 1, "one still is one cell");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
