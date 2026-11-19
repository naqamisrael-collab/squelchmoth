#pragma once
// Squelchmoth 1.32 - the maths of cutting a video's picture. Pure: no engine, no UI.
// The picture is a list of pieces. A piece shows a stretch of the video at a place on the timeline; in the app each
// piece is one audio clip playing the video's own sound, so editing the sound edits the picture.
#include <algorithm>
#include <cmath>
#include <vector>

namespace SqCut
{
    /** at: where the piece starts on the timeline; length: how long it lasts there; srcIn: the moment of the video it
        starts on; speed: 1 unless its clip was sped up or slowed down. Later pieces in the list win where two overlap. */
    struct Piece { double at = 0, length = 0, srcIn = 0, speed = 1.0; double end() const { return at + length; } };

    struct Hit { int piece = -1; double src = 0; bool shows() const { return piece >= 0; } };

    /** What the picture shows at a session time: the piece, and the moment of the video. slip shows the picture that
        much later than its sound. Nothing (piece -1) in a gap or where the video has no frame for that moment. */
    inline Hit lookup (const std::vector<Piece>& pieces, double t, double slip, double videoSeconds)
    {
        for (int i = (int) pieces.size() - 1; i >= 0; --i)
        {
            const auto& p = pieces[(size_t) i];
            if (p.length > 0 && t >= p.at && t < p.end())
            {
                const double src = p.srcIn + (t - p.at) * p.speed - slip;
                if (src >= 0 && src < videoSeconds) return { i, src };
                return {};
            }
        }
        return {};
    }

    /** The session time at which a moment of the video is on screen (the first piece that shows it), or -1 when it was cut out. */
    inline double whereIs (const std::vector<Piece>& pieces, double src, double slip, double videoSeconds)
    {
        double best = -1.0;
        for (size_t i = 0; i < pieces.size(); ++i)
        {
            const auto& p = pieces[i]; if (p.length <= 0 || p.speed <= 0) continue;
            const double t = p.at + (src + slip - p.srcIn) / p.speed;
            if (t >= p.at && t < p.end())
            {
                const auto h = lookup (pieces, t, slip, videoSeconds);
                if (h.piece == (int) i && (best < 0 || t < best)) best = t;
            }
        }
        return best;
    }

    inline double start (const std::vector<Piece>& pieces) { double s = 0; bool any = false; for (auto& p : pieces) if (p.length > 0) { s = any ? std::min (s, p.at) : p.at; any = true; } return s; }
    inline double end (const std::vector<Piece>& pieces)   { double e = 0; for (auto& p : pieces) if (p.length > 0) e = std::max (e, p.end()); return e; }

    /** True when the picture is still one untouched piece playing the video from srcIn 0 at normal speed. */
    inline bool isWhole (const std::vector<Piece>& pieces, double slip, double videoSeconds)
    {
        return pieces.size() == 1 && std::abs (pieces[0].srcIn) < 1.0e-6 && std::abs (pieces[0].speed - 1.0) < 1.0e-9 && std::abs (slip) < 1.0e-9 && pieces[0].length >= videoSeconds - 0.05;
    }

    /** One stretch of an export: a stretch of the video (src to src + length), or black. */
    struct Stretch { double at = 0, length = 0, src = 0; bool picture = false; };

    /** The picture between two session times as stretches in order with no holes: video where a piece shows, black
        elsewhere. Stretches shorter than tiny are folded into their neighbour's black. Speed-changed pieces are not
        handled here (hasSpeedChange says so). */
    inline std::vector<Stretch> plan (const std::vector<Piece>& pieces, double slip, double videoSeconds, double from, double to, double tiny = 0.0005)
    {
        std::vector<double> edges { from, to };
        for (auto& p : pieces)
        {
            if (p.length <= 0) continue;
            edges.push_back (p.at); edges.push_back (p.end());
            // the moments the video itself starts and ends inside this piece
            edges.push_back (p.at + (slip - p.srcIn)); edges.push_back (p.at + (videoSeconds + slip - p.srcIn));
        }
        std::sort (edges.begin(), edges.end());
        std::vector<Stretch> out;
        for (size_t i = 0; i + 1 < edges.size(); ++i)
        {
            const double a = std::max (from, edges[i]), b = std::min (to, edges[i + 1]);
            if (b - a <= 1.0e-9) continue;
            const auto h = lookup (pieces, (a + b) * 0.5, slip, videoSeconds);
            Stretch s; s.at = a; s.length = b - a; s.picture = h.shows();
            if (s.picture) { const auto& p = pieces[(size_t) h.piece]; s.src = p.srcIn + (a - p.at) - slip; }
            // join onto the stretch before when it simply carries on (same kind, and for video the same run of frames)
            if (! out.empty())
            {
                auto& last = out.back();
                const bool carriesOn = last.picture == s.picture && (! s.picture || std::abs ((last.src + last.length) - s.src) < 1.0e-6);
                if (carriesOn) { last.length += s.length; continue; }
                if (s.length < tiny) { last.length += s.length; continue; }
            }
            out.push_back (s);
        }
        return out;
    }

    inline bool hasSpeedChange (const std::vector<Piece>& pieces) { for (auto& p : pieces) if (p.length > 0 && std::abs (p.speed - 1.0) > 1.0e-6) return true; return false; }
    /** How many frames each stretch of a plan gets at an output frame rate, so the stretches meet on whole frames and
        the total is the range's own frame count (no drift however many cuts there are). */
    inline std::vector<long long> frameCounts (const std::vector<Stretch>& plan, double fps)
    {
        std::vector<long long> out; if (plan.empty()) return out;
        const double t0 = plan.front().at; long long before = 0;
        for (auto& s : plan) { const long long upTo = (long long) std::llround ((s.at + s.length - t0) * fps); out.push_back (std::max (0LL, upTo - before)); before = std::max (before, upTo); }
        return out;
    }
}
