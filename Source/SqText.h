#pragma once
// Squelchmoth 1.33 - words on the picture: the model, timing and caption files. Pure: no engine, no UI.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace SqText
{
    enum Style { caption = 0, title = 1, lowerThird = 2 };
    enum Place { automatic = 0, top = 1, middle = 2, bottom = 3 };

    struct Item
    {
        double start = 0, end = 0;          // session seconds; shows from start up to (not including) end
        std::string text;                    // UTF-8; "\n" separates lines
        int style = caption, place = automatic;
        double size = 1.0;                   // a factor on the style's own size
        unsigned int colour = 0xffffffffu;   // ARGB
    };

    inline const char* styleName (int s) { return s == title ? "title" : s == lowerThird ? "lower third" : "caption"; }
    inline std::string lower (std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a'); return s; }
    inline int styleFrom (const std::string& given) { const auto s = lower (given); return s.find ("title") != std::string::npos ? title : (s.find ("lower") != std::string::npos || s.find ("third") != std::string::npos) ? lowerThird : caption; }
    inline const char* placeName (int p) { return p == top ? "top" : p == middle ? "middle" : p == bottom ? "bottom" : "auto"; }
    inline int placeFrom (const std::string& given) { const auto s = lower (given); return s == "top" ? top : (s == "middle" || s == "centre" || s == "center") ? middle : s == "bottom" ? bottom : automatic; }

    /** The style's size as a fraction of the frame height, and where it sits when no place is given. */
    inline double heightFraction (const Item& i) { return (i.style == title ? 0.11 : i.style == lowerThird ? 0.06 : 0.055) * std::clamp (i.size, 0.25, 4.0); }
    inline int placeOf (const Item& i) { return i.place != automatic ? i.place : (i.style == title ? middle : bottom); }

    /** Makes a text's times sane: not before 0, at least minLength long. */
    inline void tidy (Item& i, double minLength = 0.04) { i.start = std::max (0.0, i.start); i.end = std::max (i.end, i.start + minLength); }

    /** The texts on screen at a time, in list order (later ones are drawn over earlier ones). */
    inline std::vector<int> showing (const std::vector<Item>& items, double t)
    {
        std::vector<int> out; for (size_t i = 0; i < items.size(); ++i) if (t >= items[i].start && t < items[i].end) out.push_back ((int) i);
        return out;
    }

    //============================================================================== caption files (.srt)
    inline std::string srtTime (double seconds)
    {
        const long long ms = (long long) std::llround (std::max (0.0, seconds) * 1000.0);
        char b[40]; std::snprintf (b, sizeof (b), "%02lld:%02lld:%02lld,%03lld", ms / 3600000, (ms / 60000) % 60, (ms / 1000) % 60, ms % 1000);
        return b;
    }

    /** Reads "hh:mm:ss,mmm" (also with a dot, with fewer digits, or without hours). Returns -1 when it is not a time. */
    inline double parseSrtTime (std::string s)
    {
        while (! s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        size_t a = 0; while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a; s = s.substr (a);
        if (s.empty()) return -1.0;
        double parts[3] = { 0, 0, 0 }; int n = 0; double frac = 0; size_t i = 0; bool any = false;
        while (i < s.size())
        {
            if (s[i] >= '0' && s[i] <= '9') { if (n >= 3) return -1.0; parts[n] = parts[n] * 10 + (s[i] - '0'); any = true; ++i; }
            else if (s[i] == ':') { if (! any) return -1.0; ++n; any = false; ++i; }
            else if (s[i] == ',' || s[i] == '.')
            {
                double scale = 0.1; ++i; bool digits = false;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') { frac += (s[i] - '0') * scale; scale *= 0.1; ++i; digits = true; }
                if (! digits || i != s.size()) return -1.0;
            }
            else return -1.0;
        }
        if (! any && frac == 0) return -1.0;
        if (n == 0) return -1.0;                                   // a bare number is a counter, not a time
        return n == 2 ? parts[0] * 3600 + parts[1] * 60 + parts[2] + frac : parts[0] * 60 + parts[1] + frac;
    }

    inline std::string stripTags (const std::string& s)
    {
        std::string out; bool in = false, brace = false;
        for (char c : s) { if (c == '<') in = true; else if (c == '>' && in) in = false; else if (c == '{') brace = true; else if (c == '}' && brace) brace = false; else if (! in && ! brace) out += c; }
        return out;
    }

    /** Reads a caption file. Lenient: a byte-order mark, CR LF, missing counters, a dot before the milliseconds and
        simple tags are all accepted. offset is added to every time. */
    inline std::vector<Item> parseSrt (std::string text, double offset = 0.0)
    {
        if (text.size() >= 3 && (unsigned char) text[0] == 0xEF && (unsigned char) text[1] == 0xBB && (unsigned char) text[2] == 0xBF) text = text.substr (3);
        std::vector<std::string> lines; std::string cur;
        for (char c : text) { if (c == '\r') continue; if (c == '\n') { lines.push_back (cur); cur.clear(); } else cur += c; }
        lines.push_back (cur);
        std::vector<Item> out; size_t i = 0;
        while (i < lines.size())
        {
            const auto arrow = lines[i].find ("-->");
            if (arrow == std::string::npos) { ++i; continue; }
            std::string right = lines[i].substr (arrow + 3);
            // anything after the end time (position hints) is dropped
            { size_t a = 0; while (a < right.size() && right[a] == ' ') ++a; const auto sp = right.find (' ', a); if (sp != std::string::npos) right = right.substr (0, sp); }
            const double a = parseSrtTime (lines[i].substr (0, arrow)), b = parseSrtTime (right);
            ++i;
            std::string words;
            while (i < lines.size() && ! lines[i].empty() && lines[i].find_first_not_of (" \t") != std::string::npos)
            {
                // a counter directly followed by a time line belongs to the next caption (a missing blank line)
                if (i + 1 < lines.size() && lines[i + 1].find ("-->") != std::string::npos && lines[i].find_first_not_of ("0123456789 ") == std::string::npos) break;
                if (lines[i].find ("-->") != std::string::npos) break;
                const auto clean = stripTags (lines[i]);
                if (! clean.empty()) { if (! words.empty()) words += "\n"; words += clean; }
                ++i;
            }
            if (a < 0 || b < 0 || words.empty()) continue;
            Item it; it.start = a + offset; it.end = b + offset; it.text = words; it.style = caption; tidy (it);
            out.push_back (it);
        }
        return out;
    }

    /** Writes texts as a caption file, in time order, times shifted by -offset; texts wholly before the offset are left out. */
    inline std::string writeSrt (std::vector<Item> items, double offset = 0.0)
    {
        std::stable_sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.start < b.start; });
        std::string out; int n = 0;
        for (auto& it : items)
        {
            if (it.end - offset <= 0 || it.text.empty()) continue;
            out += std::to_string (++n) + "\n" + srtTime (it.start - offset) + " --> " + srtTime (it.end - offset) + "\n" + it.text + "\n\n";
        }
        return out;
    }
}
