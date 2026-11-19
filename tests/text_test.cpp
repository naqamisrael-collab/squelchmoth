// Squelchmoth 1.33 - titles and captions: known answers worked by hand.
#include "SqText.h"
#include <cstdio>
static int passed = 0, failed = 0;
static void check (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "PASS" : "FAIL", what); ok ? ++passed : ++failed; }
static bool near (double a, double b, double tol = 1e-9) { return std::abs (a - b) <= tol; }
int main()
{
    using namespace SqText;
    std::printf ("== Text ==\n");
    check (srtTime (0) == "00:00:00,000" && srtTime (3723.4567) == "01:02:03,457" && srtTime (59.9996) == "00:01:00,000" && srtTime (-4) == "00:00:00,000", "caption times are written hh:mm:ss,mmm, rounded to the millisecond");
    check (near (parseSrtTime ("01:02:03,457"), 3723.457) && near (parseSrtTime ("00:00:01.5"), 1.5) && near (parseSrtTime (" 2:03,25 "), 123.25) && near (parseSrtTime ("10:00:00,000"), 36000.0), "times are read with a comma or a dot, with or without hours; ten hours is 36000 s");
    check (parseSrtTime ("12") < 0 && parseSrtTime ("") < 0 && parseSrtTime ("hello") < 0 && parseSrtTime ("1:2:3:4") < 0 && parseSrtTime ("00:01,") < 0, "a counter, nothing, words and broken times are not times");
    {
        const std::string file = "\xEF\xBB\xBF" "1\r\n00:00:01,000 --> 00:00:02,500\r\nHello there\r\n\r\n2\r\n00:00:03,000 --> 00:00:04,000 X1:10\r\n<i>Two</i> lines\r\n{\\an8}here\r\n\r\n";
        const auto c = parseSrt (file);
        check (c.size() == 2 && near (c[0].start, 1.0) && near (c[0].end, 2.5) && c[0].text == "Hello there" && near (c[1].start, 3.0) && near (c[1].end, 4.0) && c[1].text == "Two lines\nhere",
               "a caption file with a byte-order mark, CR LF, tags and a position hint reads as two captions, the second of two lines");
        const auto again = parseSrt (writeSrt (c));
        check (again.size() == 2 && again[1].text == c[1].text && near (again[0].start, 1.0) && near (again[1].end, 4.0) && writeSrt (c) == "1\n00:00:01,000 --> 00:00:02,500\nHello there\n\n2\n00:00:03,000 --> 00:00:04,000\nTwo lines\nhere\n\n",
               "written and read again it is the same; the written file is exactly as expected");
        const auto shifted = parseSrt (file, 10.0);
        check (near (shifted[0].start, 11.0) && near (shifted[1].end, 14.0), "an offset places a caption file later on the timeline");
        check (writeSrt (shifted, 12.0) == "1\n00:00:00,000 --> 00:00:00,500\nHello there\n\n2\n00:00:01,000 --> 00:00:02,000\nTwo lines\nhere\n\n" && writeSrt (shifted, 13.5) == "1\n00:00:00,000 --> 00:00:00,500\nTwo lines\nhere\n\n",
               "writing from 12 s counts times from there and starts a caption already showing at zero; from 13.5 s the one that ended before is left out");
    }
    {
        // lenient: no counters, no blank line between captions, dots, a caption with no words, an end before its start
        const auto c = parseSrt ("00:01.000 --> 00:02.000\nOne\n2\n00:00:05,0 --> 00:00:04,0\nBackwards\n\n00:00:07,000 --> 00:00:08,000\n\n00:00:09,000 --> 00:00:10,000\nLast");
        check (c.size() == 3 && c[0].text == "One" && c[1].text == "Backwards" && near (c[1].start, 5.0) && near (c[1].end, 5.04) && c[2].text == "Last" && near (c[2].end, 10.0),
               "lenient reading: missing counters and blank lines are coped with, an empty caption is dropped, a backwards one is given a minimum length");
        check (parseSrt ("").empty() && parseSrt ("just words\nno times").empty(), "a file with no captions gives none");
    }
    {
        std::vector<Item> v (3); v[0].start = 1; v[0].end = 3; v[1].start = 2; v[1].end = 4; v[2].start = 4; v[2].end = 5;
        check (showing (v, 0.5).empty() && showing (v, 2.5).size() == 2 && showing (v, 3.0) == std::vector<int> { 1 } && showing (v, 4.0) == std::vector<int> { 2 } && showing (v, 5.0).empty(), "what shows when: start included, end not; two can show at once");
        Item t; t.style = title; Item c; Item big; big.size = 2.0; Item huge; huge.size = 99;
        check (near (heightFraction (t), 0.11) && near (heightFraction (c), 0.055) && near (heightFraction (big), 0.11) && near (heightFraction (huge), 0.22) && placeOf (t) == middle && placeOf (c) == bottom, "sizes are fractions of the frame height (title 11 %, caption 5.5 %); a title sits in the middle, a caption at the bottom");
        Item p; p.place = top; p.start = -2; p.end = -1; tidy (p);
        check (placeOf (p) == top && near (p.start, 0.0) && near (p.end, 0.04) && styleFrom ("Lower Third") == lowerThird && styleFrom ("title") == title && styleFrom ("anything") == caption && placeFrom ("Centre") == middle, "a given place wins; times are kept sane; style and place names are read");
    }
    std::printf ("\n== %d passed, %d failed ==\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
