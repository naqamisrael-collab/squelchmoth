#pragma once
// Squelchmoth 1.34 - the picture's shape (vertical / square / wide), and chapters from markers. Pure: no engine, no UI.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace SqShape
{
    /** A shape as width : height. 0 : 0 means "as the video is". */
    struct Shape { int w = 0, h = 0; bool given() const { return w > 0 && h > 0; } double ratio() const { return given() ? (double) w / (double) h : 0.0; } };

    /** Reads "9:16", "9x16", "1:1", or a name: vertical / portrait (9:16), square (1:1), wide / landscape (16:9), tall (4:5),
        source / original / off (as the video is). Anything else is "not given" with ok false. */
    inline Shape parse (std::string s, bool* ok = nullptr)
    {
        for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');
        s.erase (std::remove (s.begin(), s.end(), ' '), s.end());
        if (ok) *ok = true;
        if (s.empty() || s == "source" || s == "original" || s == "off" || s == "none") return {};
        if (s == "vertical" || s == "portrait") return { 9, 16 };
        if (s == "square") return { 1, 1 };
        if (s == "wide" || s == "landscape") return { 16, 9 };
        if (s == "tall") return { 4, 5 };
        const auto sep = s.find_first_of (":x/");
        if (sep != std::string::npos && sep > 0 && sep + 1 < s.size())
        {
            const auto a = s.substr (0, sep), b = s.substr (sep + 1);
            if (a.find_first_not_of ("0123456789") == std::string::npos && b.find_first_not_of ("0123456789") == std::string::npos && a.size() <= 5 && b.size() <= 5)
            { const int w = std::stoi (a), h = std::stoi (b); if (w > 0 && h > 0 && (double) w / h <= 8.0 && (double) h / w <= 8.0) return { w, h }; }
        }
        if (ok) *ok = false;
        return {};
    }

    inline std::string name (Shape s) { return s.given() ? std::to_string (s.w) + ":" + std::to_string (s.h) : std::string ("as the video is"); }

    /** The export size for a shape: the short side is shortSide pixels; even numbers. */
    inline void size (Shape s, int& w, int& h, int shortSide = 1080)
    {
        if (! s.given()) { w = h = 0; return; }
        if (s.w >= s.h) { h = shortSide; w = (int) std::lround ((double) shortSide * s.w / s.h); }
        else            { w = shortSide; h = (int) std::lround ((double) shortSide * s.h / s.w); }
        w &= ~1; h &= ~1;
    }

    struct Rect { int x = 0, y = 0, w = 0, h = 0; };

    /** The largest area of a shape inside a picture, placed by a focus point: 0 keeps the left / top, 1 the right /
        bottom, 0.5 the middle. Even numbers, always inside the picture. */
    inline Rect crop (int srcW, int srcH, Shape s, double focusX = 0.5, double focusY = 0.5)
    {
        Rect r; if (srcW < 2 || srcH < 2) return r;
        if (! s.given()) { r.w = srcW & ~1; r.h = srcH & ~1; return r; }
        const double want = s.ratio(), have = (double) srcW / (double) srcH;
        if (want < have) { r.h = srcH & ~1; r.w = std::min (srcW & ~1, std::max (2, (int) std::lround (r.h * want) & ~1)); }
        else             { r.w = srcW & ~1; r.h = std::min (srcH & ~1, std::max (2, (int) std::lround (r.w / want) & ~1)); }
        r.x = (int) std::lround (std::clamp (focusX, 0.0, 1.0) * (double) (srcW - r.w)) & ~1;
        r.y = (int) std::lround (std::clamp (focusY, 0.0, 1.0) * (double) (srcH - r.h)) & ~1;
        r.x = std::clamp (r.x, 0, srcW - r.w); r.y = std::clamp (r.y, 0, srcH - r.h);
        return r;
    }

    /** Where the whole picture sits inside a frame when it is fitted with bars. */
    inline Rect fitInside (int srcW, int srcH, int frameW, int frameH)
    {
        Rect r; if (srcW < 1 || srcH < 1 || frameW < 1 || frameH < 1) return r;
        const double k = std::min ((double) frameW / srcW, (double) frameH / srcH);
        r.w = std::max (1, (int) std::lround (srcW * k)); r.h = std::max (1, (int) std::lround (srcH * k));
        r.x = (frameW - r.w) / 2; r.y = (frameH - r.h) / 2;
        return r;
    }

    //============================================================================== chapters
    struct Marker { double at = 0; std::string name; };
    struct Chapter { double start = 0, end = 0; std::string title; };

    /** Chapters for an export from..to out of the session's markers: each marker inside the range starts one, which
        runs to the next (or the end). A stretch before the first marker is a chapter called "Start". Times are counted
        from the start of the export. Markers closer than minLength to the one before are left out. No markers inside: none. */
    inline std::vector<Chapter> chapters (std::vector<Marker> markers, double from, double to, double minLength = 1.0)
    {
        std::vector<Chapter> out; if (to <= from) return out;
        std::stable_sort (markers.begin(), markers.end(), [] (const Marker& a, const Marker& b) { return a.at < b.at; });
        std::vector<Marker> in;
        for (auto& m : markers) if (m.at >= from - 1.0e-9 && m.at < to - minLength * 0.5) { if (! in.empty() && m.at - in.back().at < minLength) continue; in.push_back (m); }
        if (in.empty()) return out;
        if (in.front().at - from >= minLength) out.push_back ({ 0.0, in.front().at - from, "Start" });
        else in.front().at = from;
        for (size_t i = 0; i < in.size(); ++i)
            out.push_back ({ in[i].at - from, (i + 1 < in.size() ? in[i + 1].at : to) - from, in[i].name.empty() ? "Chapter " + std::to_string (out.size() + 1) : in[i].name });
        return out;
    }

    inline std::string escapeMeta (const std::string& s)
    {
        std::string o; for (char c : s) { if (c == '=' || c == ';' || c == '#' || c == '\\') { o += '\\'; o += c; } else if (c == '\n') o += "\\\n"; else if (c != '\r') o += c; }
        return o;
    }

    /** The chapters as the text FFmpeg reads as a metadata input. */
    inline std::string metadataText (const std::vector<Chapter>& ch)
    {
        std::string o = ";FFMETADATA1\n";
        for (auto& c : ch)
            o += "[CHAPTER]\nTIMEBASE=1/1000\nSTART=" + std::to_string ((long long) std::llround (c.start * 1000.0)) + "\nEND=" + std::to_string ((long long) std::llround (c.end * 1000.0)) + "\ntitle=" + escapeMeta (c.title) + "\n";
        return o;
    }
}
