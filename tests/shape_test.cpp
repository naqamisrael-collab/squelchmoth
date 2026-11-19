// Squelchmoth 1.34 - picture shapes and chapters: known answers worked by hand.
#include "SqShape.h"
#include <cstdio>
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static bool near (double a, double b, double tol = 1e-9) { return std::abs (a - b) <= tol; }
static bool is (SqShape::Rect r, int x, int y, int w, int h) { return r.x == x && r.y == y && r.w == w && r.h == h; }
int main()
{
    using namespace SqShape;
    std::printf ("== Shape ==\n");
    { bool ok = false; const auto a = parse ("9:16", &ok); const bool A = ok && a.w == 9 && a.h == 16; const auto b = parse (" Vertical ", &ok); const bool B = ok && b.w == 9 && b.h == 16;
      const auto c = parse ("Square", &ok); const bool C = ok && c.w == 1 && c.h == 1; const auto d = parse ("4x5", &ok); const bool D = ok && d.w == 4 && d.h == 5; const auto e = parse ("wide", &ok); const bool E = ok && e.w == 16;
      const auto f = parse ("source", &ok); const bool F = ok && ! f.given(); const auto g = parse ("banana", &ok); const bool G = ! ok && ! g.given(); parse ("0:5", &ok); const bool H = ! ok; parse ("100:1", &ok); const bool I = ! ok;
      check (A && B && C && D && E && F && G && H && I, "shapes are read from numbers (9:16, 4x5) and names (vertical, square, wide, source); nonsense and extreme shapes are refused"); }
    { int w = 0, h = 0; size ({ 9, 16 }, w, h); const bool a = w == 1080 && h == 1920; size ({ 1, 1 }, w, h); const bool b = w == 1080 && h == 1080; size ({ 4, 5 }, w, h); const bool c = w == 1080 && h == 1350;
      size ({ 16, 9 }, w, h); const bool d = w == 1920 && h == 1080; size ({ 9, 16 }, w, h, 720); const bool e = w == 720 && h == 1280; size ({}, w, h); const bool f = w == 0 && h == 0;
      check (a && b && c && d && e && f, "sizes with a short side of 1080: 9:16 is 1080 x 1920, 1:1 1080 x 1080, 4:5 1080 x 1350, 16:9 1920 x 1080; at 720 the vertical is 720 x 1280"); }
    check (is (crop (1920, 1080, { 9, 16 }), 656, 0, 608, 1080), "vertical from the middle of 1920 x 1080: a 608 x 1080 area starting 656 from the left");
    check (is (crop (1920, 1080, { 9, 16 }, 0.0), 0, 0, 608, 1080) && is (crop (1920, 1080, { 9, 16 }, 1.0), 1312, 0, 608, 1080) && is (crop (1920, 1080, { 9, 16 }, 7.0), 1312, 0, 608, 1080), "focus 0 keeps the left edge, 1 the right edge (1312), and a focus outside 0 to 1 is held");
    check (is (crop (1920, 1080, { 1, 1 }), 420, 0, 1080, 1080) && is (crop (1080, 1920, { 16, 9 }, 0.5, 0.25), 0, 328, 1080, 608), "square from wide is the middle 1080; wide from a vertical video, a quarter of the way down, starts at 328");
    check (is (crop (1920, 1080, { 16, 9 }), 0, 0, 1920, 1080) && is (crop (1920, 1080, {}), 0, 0, 1920, 1080) && is (crop (1921, 1081, {}), 0, 0, 1920, 1080) && is (crop (128, 64, { 1, 1 }, 1.0), 64, 0, 64, 64),
           "the same shape keeps everything; odd sizes are made even; a 128 x 64 picture squared at the right is its right half");
    { const auto r = crop (1, 1, { 1, 1 }); const auto t = crop (37, 23, { 5, 4 }, 0.3, 0.9); check (r.w == 0 && t.w % 2 == 0 && t.h % 2 == 0 && t.x >= 0 && t.y >= 0 && t.x + t.w <= 37 && t.y + t.h <= 23 && t.h == 22, "a crop never leaves the picture, even for awkward sizes"); }
    check (is (fitInside (128, 64, 64, 64), 0, 16, 64, 32) && is (fitInside (1920, 1080, 1080, 1920), 0, 656, 1080, 608) && is (fitInside (64, 64, 640, 360), 140, 0, 360, 360), "fitted with bars: 128 x 64 in a square sits 64 x 32 with 16 above; a square in 640 x 360 is 360 wide, 140 from the left");
    std::printf ("== Chapters ==\n");
    {
        const std::vector<Marker> m { { 30.0, "Hook" }, { 10.0, "Verse" }, { 95.0, "Outro" }, { 30.4, "Too close" }, { 200.0, "After the end" } };
        const auto c = chapters (m, 0.0, 100.0);
        check (c.size() == 4 && c[0].title == "Start" && near (c[0].start, 0) && near (c[0].end, 10) && c[1].title == "Verse" && near (c[1].end, 30) && c[2].title == "Hook" && near (c[2].end, 95) && c[3].title == "Outro" && near (c[3].end, 100),
               "markers at 10, 30 and 95 s of a 100 s export: Start 0-10, Verse 10-30, Hook 30-95, Outro 95-100; one 0.4 s after another and one past the end are left out");
        const auto part = chapters (m, 20.0, 60.0);
        check (part.size() == 2 && part[0].title == "Start" && near (part[0].end, 10.0) && part[1].title == "Hook" && near (part[1].start, 10.0) && near (part[1].end, 40.0), "exporting 20 to 60 s counts chapter times from the export's start: Start 0-10, Hook 10-40");
        const auto atZero = chapters ({ { 0.3, "Intro" }, { 50.0, "" } }, 0.0, 100.0);
        check (atZero.size() == 2 && atZero[0].title == "Intro" && near (atZero[0].start, 0.0) && atZero[1].title == "Chapter 2", "a marker within a second of the start begins the first chapter at zero; an unnamed marker is numbered");
        check (chapters ({}, 0, 100).empty() && chapters (m, 300, 400).empty() && chapters (m, 50, 50).empty() && chapters ({ { 99.8, "Late" } }, 0, 100).empty(), "no markers in the range, an empty range, or a marker in the last half second: no chapters");
        check (metadataText (chapters ({ { 0.0, "A=B; #1" }, { 5.0, "Two" } }, 0.0, 9.5)) == ";FFMETADATA1\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=0\nEND=5000\ntitle=A\\=B\\; \\#1\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=5000\nEND=9500\ntitle=Two\n",
               "the metadata text is exactly as expected, with = ; and # in a title escaped");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
