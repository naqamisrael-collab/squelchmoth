#pragma once
// Squelchmoth 1.29 / 1.30 - the maths of the video thumbnail strip, frame stepping and the contact sheet. Pure: no engine, no UI.
#include <algorithm>
#include <cmath>
#include <vector>

namespace SqStrip
{
    /** How many stills fit a strip: as many slotW-wide slots as the width holds, at least one, at most maxSlots. */
    inline int slotsFor (int widthPx, int slotW, int maxSlots = 400) { return std::clamp (slotW > 0 ? widthPx / slotW : 1, 1, std::max (1, maxSlots)); }

    /** The moment a slot's still is taken from: the middle of the slot. */
    inline double slotTime (int k, int count, double seconds) { return count > 0 ? ((double) std::clamp (k, 0, count - 1) + 0.5) * seconds / (double) count : 0.0; }

    /** The time under a pixel of the strip, kept inside the video. */
    inline double timeAt (double x, double width, double seconds) { return width > 0 ? std::clamp (x / width, 0.0, 1.0) * seconds : 0.0; }

    /** The pixel a time falls on. */
    inline double xAt (double t, double width, double seconds) { return seconds > 0 ? std::clamp (t / seconds, 0.0, 1.0) * width : 0.0; }

    /** The slot under a pixel. */
    inline int slotAt (double x, double width, int count) { return width > 0 && count > 0 ? std::clamp ((int) std::floor (x / width * (double) count), 0, count - 1) : 0; }

    /** How many frames a video has (the last one may be cut short). */
    inline long long frameCount (double seconds, double fps) { return std::max (1LL, (long long) std::ceil (seconds * fps - 1.0e-6)); }

    /** Moves by whole frames from a time: snaps to the frame on screen, moves, stays inside the video. Returns the start of that frame. */
    inline double stepFrames (double t, double fps, long long frames, double seconds)
    {
        if (fps <= 0) return t;
        const long long last = frameCount (seconds, fps) - 1;
        const long long idx = std::clamp ((long long) std::floor (std::max (0.0, t) * fps + 1.0e-6) + frames, 0LL, last);
        return (double) idx / fps;
    }

    /** The order stills are fetched in: every 8th first, then every 4th, 2nd and the rest - the whole length shows early. */
    inline std::vector<int> fillOrder (int count)
    {
        std::vector<int> order; std::vector<char> used ((size_t) std::max (0, count), 0);
        for (int stride : { 8, 4, 2, 1 })
            for (int k = stride == 1 ? 0 : stride / 2; k < count; k += stride)
                if (! used[(size_t) k]) { used[(size_t) k] = 1; order.push_back (k); }
        return order;
    }

    /** A contact sheet: count stills in a grid. Each cell is the still with a label band under it. */
    struct Sheet { int count = 0, cols = 0, rows = 0, cellW = 0, imageH = 0, labelH = 0, gap = 0, width = 0, height = 0;
                   int cellX (int k) const { return gap + (k % cols) * (cellW + gap); }
                   int cellY (int k) const { return gap + (k / cols) * (imageH + labelH + gap); } };

    inline Sheet sheet (int count, int cols, int cellW, int srcW, int srcH, int labelH = 18, int gap = 6)
    {
        Sheet s; s.count = std::max (1, count);
        s.cols = cols > 0 ? std::min (cols, s.count) : std::clamp ((int) std::ceil (std::sqrt ((double) s.count * 1.6)), 1, s.count);
        s.rows = (s.count + s.cols - 1) / s.cols;
        s.cellW = std::max (16, cellW) & ~1;
        s.imageH = std::max (16, (int) std::lround ((double) s.cellW * (double) std::max (1, srcH) / (double) std::max (1, srcW))) & ~1;
        s.labelH = labelH; s.gap = gap;
        s.width = s.gap + s.cols * (s.cellW + s.gap); s.height = s.gap + s.rows * (s.imageH + s.labelH + s.gap);
        return s;
    }
    /** The stills drawn along a video's block on the timeline. The block runs from x1 to x2 (pixels, may be off screen);
        tiles are tileW wide, anchored to the block's start so they stay put while the view scrolls; only the ones that
        show between visL and visR are returned. Each names the still (of count) nearest its middle. */
    struct Tile { double x = 0, w = 0; int thumb = 0; };
    inline std::vector<Tile> tiles (double x1, double x2, double visL, double visR, double tileW, int count)
    {
        std::vector<Tile> out;
        if (! (x2 > x1) || tileW < 1.0 || count < 1 || visR <= visL) return out;
        const long long first = std::max (0LL, (long long) std::floor ((std::max (x1, visL) - x1) / tileW));
        const long long last = (long long) std::ceil ((std::min (x2, visR) - x1) / tileW) - 1;
        for (long long i = first; i <= last && out.size() < 4096; ++i)
        {
            Tile t; t.x = x1 + (double) i * tileW; t.w = std::min (tileW, x2 - t.x);
            if (t.w <= 0) break;
            t.thumb = slotAt (t.x + t.w * 0.5 - x1, x2 - x1, count);
            out.push_back (t);
        }
        return out;
    }

    /** Where a video ends up when its start is moved: never before the start of the session. */
    inline double movedStart (double newStart) { return std::max (0.0, newStart); }
}
