#pragma once
// Squelchmoth 1.35 - laying a video out on the beat grid. Pure: no engine, no UI.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace SqBeatCut
{
    enum Order { keep = 0, reverse = 1, shuffle = 2, spread = 3 };

    /** One slot of the grid: where it sits in the session, and the moment of the video it starts on. */
    struct Slot { double at = 0, length = 0, src = 0; };

    /** The slots between consecutive grid times (grid: session seconds, ascending). Slots shorter than minLength are joined to the one before. */
    inline std::vector<Slot> slots (const std::vector<double>& grid, double minLength = 0.02)
    {
        std::vector<Slot> out;
        for (size_t i = 0; i + 1 < grid.size(); ++i)
        {
            const double len = grid[i + 1] - grid[i]; if (len <= 0) continue;
            if (! out.empty() && len < minLength) { out.back().length += len; continue; }
            out.push_back ({ grid[i], len, 0.0 });
        }
        return out;
    }

    /** A seeded order of 0..n-1 in which no number stays in its own place (n of 3 or more; 2 swaps; 1 stays). */
    inline std::vector<int> shuffled (int n, std::uint32_t seed)
    {
        std::vector<int> p ((size_t) std::max (0, n)); for (int i = 0; i < n; ++i) p[(size_t) i] = i;
        if (n < 2) return p;
        std::uint32_t s = seed * 2654435761u + 0x9E3779B9u;
        auto next = [&s] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
        for (int i = n - 1; i > 0; --i) std::swap (p[(size_t) i], p[(size_t) (next() % (std::uint32_t) i)]);   // Sattolo: one cycle, so nothing stays put
        return p;
    }

    /** Gives every slot its moment of the video.
        videoStart: where the video's first frame falls on the session timeline as things stand (for keep / reverse / shuffle, a slot's
        own moment is slot.at - videoStart). videoSeconds: the video's length. A source start is pulled back so the whole slot has picture,
        and never before the video's start. */
    inline void assign (std::vector<Slot>& s, int order, double videoStart, double videoSeconds, std::uint32_t seed = 1)
    {
        const int n = (int) s.size(); if (n == 0) return;
        std::vector<double> own ((size_t) n); for (int i = 0; i < n; ++i) own[(size_t) i] = s[(size_t) i].at - videoStart;
        std::vector<int> from ((size_t) n); for (int i = 0; i < n; ++i) from[(size_t) i] = i;
        if (order == reverse) std::reverse (from.begin(), from.end());
        else if (order == shuffle) from = shuffled (n, seed);
        for (int i = 0; i < n; ++i)
        {
            auto& slot = s[(size_t) i];
            double src = order == spread ? (n > 1 ? (double) i / (double) (n - 1) * std::max (0.0, videoSeconds - slot.length) : 0.0) : own[(size_t) from[(size_t) i]];
            src = std::min (src, videoSeconds - slot.length);
            slot.src = std::max (0.0, src);
        }
    }
}
