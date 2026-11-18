// Squelchmoth 1.31 - video proxies: known answers worked by hand.
#include "SqProxy.h"
#include <cstdio>
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static bool near (double a, double b, double tol = 1e-9) { return std::abs (a - b) <= tol; }
int main()
{
    using namespace SqProxy;
    std::printf ("== Proxy ==\n");
    check (partFrames (25.0) == 7500 && partFrames (30000.0 / 1001.0) == 8991 && partFrames (25.0, 1.0) == 25 && partFrames (0.001, 1.0) == 1, "a part is 300 s in whole frames: 7500 at 25 fps, 8991 at 29.97; never less than one frame");
    check (partCount (100, 25) == 4 && partCount (101, 25) == 5 && partCount (1, 25) == 1 && partCount (900000, 7500) == 120, "parts needed: 100 frames in 25s is 4, 101 is 5; ten hours at 25 fps is 120 parts");
    check (partOf (0, 25) == 0 && partOf (24, 25) == 0 && partOf (25, 25) == 1 && partOf (99, 25) == 3 && frameInPart (24, 25) == 24 && frameInPart (25, 25) == 0 && frameInPart (63, 25) == 13, "frame 24 is the last of part 0, frame 25 the first of part 1, frame 63 is frame 13 of part 2");
    check (framesInPart (0, 101, 25) == 25 && framesInPart (4, 101, 25) == 1 && framesInPart (5, 101, 25) == 0, "the last part may be short: 1 frame of 101");
    check (near (seekFor (0, 25.0), 0.0) && near (seekFor (25, 25.0), 0.99) && near (seekFor (15000, 30.0), 499.9916666667, 1e-9), "seek a quarter frame early: frame 25 at 25 fps is 0.99 s");
    { int w = 0, h = 0; size (3840, 2160, 640, w, h); const bool a = w == 640 && h == 360; size (64, 64, 640, w, h); const bool b = w == 64 && h == 64; size (1920, 817, 640, w, h); const bool c = w == 640 && h == 272;
      check (a && b && c, "proxy size: 4K becomes 640 x 360, a 64 x 64 video stays 64 x 64, 1920 x 817 becomes 640 x 272 (even)"); }
    check (worthIt (3840, 10.0) && worthIt (1280, 600.0) && ! worthIt (1280, 599.0) && ! worthIt (64, 4.0), "made unasked only for a picture wider than 1280 or a video of ten minutes or more");
    check (nextPart ({ 1, 0, 0, 1 }, -1) == 1 && nextPart ({ 1, 0, 0, 1 }, 2) == 2 && nextPart ({ 1, 0, 0, 1 }, 3) == 1 && nextPart ({ 1, 1 }, 0) == -1 && nextPart ({}, 0) == -1 && nextPart ({ 0 }, 9) == 0,
           "next part: the one being looked at first, else the first missing, none when all are made");
    check (made ({ 1, 0, 0, 1 }) == 2 && made ({}) == 0, "parts made are counted");
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
