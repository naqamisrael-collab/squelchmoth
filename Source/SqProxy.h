#pragma once
// Squelchmoth 1.31 - the maths of video proxies. Pure: no engine, no UI.
// A proxy is a small copy of a video made in parts, each a whole number of frames, with a keyframe every few frames, so
// any frame can be fetched in constant time however long the video is and however sparse the original's keyframes are.
#include <algorithm>
#include <cmath>
#include <vector>

namespace SqProxy
{
    /** Frames in one part: about partSeconds, a whole number of frames. */
    inline long long partFrames (double fps, double partSeconds = 300.0) { return std::max (1LL, (long long) std::llround (fps * partSeconds)); }

    /** How many parts a video of that many frames needs. */
    inline int partCount (long long frames, long long perPart) { return (int) ((std::max (1LL, frames) + perPart - 1) / std::max (1LL, perPart)); }

    /** Which part holds a frame, and which frame of that part it is. */
    inline int partOf (long long frame, long long perPart)          { return (int) (std::max (0LL, frame) / std::max (1LL, perPart)); }
    inline long long frameInPart (long long frame, long long perPart) { return std::max (0LL, frame) % std::max (1LL, perPart); }

    /** Frames a part holds (the last one may be short). */
    inline long long framesInPart (int part, long long frames, long long perPart) { return std::clamp (frames - (long long) part * perPart, 0LL, perPart); }

    /** Where to seek to land on a frame: a quarter of a frame before it, so rounding never picks a neighbour. */
    inline double seekFor (long long frame, double fps) { return fps > 0 ? std::max (0.0, ((double) frame - 0.25) / fps) : 0.0; }

    /** The proxy's picture size: no wider than maxWidth, never bigger than the original, same shape, even numbers. */
    inline void size (int srcW, int srcH, int maxWidth, int& w, int& h)
    {
        w = std::max (16, std::min (maxWidth, srcW)) & ~1;
        h = std::max (16, (int) std::lround ((double) w * (double) std::max (1, srcH) / (double) std::max (1, srcW))) & ~1;
    }

    /** Whether a proxy is worth making without being asked: a large picture or a long video. */
    inline bool worthIt (int srcW, double seconds) { return srcW > 1280 || seconds >= 600.0; }

    /** The next part to make: the one being looked at first (when it is missing), otherwise the first missing one; -1 when all are made. */
    inline int nextPart (const std::vector<char>& have, int urgent)
    {
        if (urgent >= 0 && urgent < (int) have.size() && ! have[(size_t) urgent]) return urgent;
        for (size_t i = 0; i < have.size(); ++i) if (! have[i]) return (int) i;
        return -1;
    }

    inline int made (const std::vector<char>& have) { return (int) std::count_if (have.begin(), have.end(), [] (char c) { return c != 0; }); }
}
