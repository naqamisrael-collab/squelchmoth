#pragma once
// Squelchmoth 1.13 - engraving: lays notation parts out in systems and pages (spacing by duration, line breaks,
// stretching) and paints them with a SMuFL music font: staves, clefs, key and time signatures, note heads, stems,
// flags, beams, dots, accidentals, ledger lines, rests, ties, triplet numbers, barlines, braces, title and bar numbers.
// The same layout serves the Score window (scroll or page view, hit testing) and the PDF / PNG export.

#include "Notation.h"
#include "BinaryData.h"

namespace ScoreRender
{
    namespace Glyph
    {
        // SMuFL code points
        constexpr juce::juce_wchar gClef = 0xE050, fClef = 0xE062, headBlack = 0xE0A4, headHalf = 0xE0A3, headWhole = 0xE0A2,
            sharp = 0xE262, flat = 0xE260, natural = 0xE261, dblSharp = 0xE263, dblFlat = 0xE264,
            restWhole = 0xE4E3, restHalf = 0xE4E4, restQuarter = 0xE4E5, rest8 = 0xE4E6, rest16 = 0xE4E7, rest32 = 0xE4E8,
            flag8Up = 0xE240, flag8Down = 0xE241, flag16Up = 0xE242, flag16Down = 0xE243, flag32Up = 0xE244, flag32Down = 0xE245,
            dot = 0xE1E7, tuplet3 = 0xE883, brace = 0xE000, time0 = 0xE080;
    }

    inline juce::Typeface::Ptr typeface()
    {
        static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (BinaryData::ScoreFont_otf, BinaryData::ScoreFont_otfSize);
        return tf;
    }
    inline juce::Font musicFont (float staffSpace)   { return juce::Font (juce::FontOptions (typeface()).withPointHeight (staffSpace * 4.0f)); }
    inline juce::String g1 (juce::juce_wchar c)       { return juce::String::charToString (c); }

    //==========================================================================
    struct PartView { juce::String name; const Notation::Score* score = nullptr; int partIndex = 0; };

    struct HeadHit { juce::Rectangle<float> box; int part = 0, staff = 0, source = -1; double beat = 0; };

    struct MeasureLayout
    {
        int index = 0; float x = 0, width = 0;
        std::vector<std::pair<double, float>> anchors;   // beat-in-bar -> x offset in the measure (after the start padding)
        bool firstInSystem = false;
        float headerWidth = 0;                            // clef / key / time at the start
    };
    struct System { float y = 0; int page = 0; std::vector<MeasureLayout> measures; std::vector<float> staffTops; std::vector<int> staffPart, staffIndex; float height = 0; };

    struct Layout
    {
        float sp = 8, width = 800, pageHeight = 0, margin = 40;
        bool pages = false;
        std::vector<System> systems;
        int numPages = 1;
        float totalHeight = 0;
        juce::String title, subtitle;
        std::vector<HeadHit> hits;     // filled while painting
    };

    //==========================================================================
    namespace detail
    {
        inline float keyWidth (int fifths, float sp)   { return std::abs (fifths) * sp * 1.0f + (fifths != 0 ? sp * 0.6f : 0); }
        inline float clefWidth (float sp)              { return sp * 3.4f; }
        inline float timeWidth (float sp)              { return sp * 2.6f; }

        /** Horizontal space an event takes before the next one (by duration, Gould's spacing curve). */
        inline float spaceFor (double beats, float sp)   { return sp * (1.6f + 2.4f * (float) std::sqrt (juce::jmax (0.0625, beats))); }
    }

    /** Lays the parts out: systems across the width, pages when pageHeight > 0. */
    inline Layout layout (const std::vector<PartView>& parts, float width, float sp, float pageHeight, const juce::String& title, const juce::String& subtitle)
    {
        Layout L; L.sp = sp; L.width = width; L.pageHeight = pageHeight; L.pages = pageHeight > 0; L.title = title; L.subtitle = subtitle;
        L.margin = juce::jmax (24.0f, sp * 5.0f);
        if (parts.empty() || parts[0].score == nullptr || parts[0].score->measures.empty()) return L;
        const int numBars = (int) parts[0].score->measures.size();
        const int fifths = parts[0].score->key.fifths;

        // natural width of each bar: the union of every staff's onsets, spaced by the shortest note sounding there
        struct Natural { std::vector<std::pair<double, float>> anchors; float width; };
        std::vector<Natural> nat ((size_t) numBars);
        for (int b = 0; b < numBars; ++b)
        {
            std::map<double, double> onsets;   // beat -> shortest duration starting there
            double len = 4;
            for (auto& pv : parts)
            {
                if (b >= (int) pv.score->measures.size()) continue;
                auto& m = pv.score->measures[(size_t) b]; len = m.lengthBeats;
                for (auto& st : m.staves)
                    for (auto& e : st.events)
                    {
                        const double k = std::round (e.start * 96.0) / 96.0;
                        auto it = onsets.find (k);
                        if (it == onsets.end()) onsets[k] = e.length; else it->second = std::min (it->second, e.length);
                    }
            }
            if (onsets.empty()) onsets[0] = len;
            float x = sp * 1.2f;
            bool anyAccidentals = false;
            for (auto& pv : parts) if (b < (int) pv.score->measures.size()) for (auto& st : pv.score->measures[(size_t) b].staves) for (auto& e : st.events) for (auto& h : e.heads) anyAccidentals |= h.showAccidental;
            if (anyAccidentals) x += sp * 0.6f;
            for (auto it = onsets.begin(); it != onsets.end(); ++it)
            {
                nat[(size_t) b].anchors.push_back ({ it->first, x });
                auto nx = std::next (it);
                const double dur = nx != onsets.end() ? nx->first - it->first : len - it->first;
                x += detail::spaceFor (juce::jmin (dur, it->second > 0 ? juce::jmax (it->second, dur) : dur), sp);
            }
            nat[(size_t) b].anchors.push_back ({ len, x });
            nat[(size_t) b].width = x + sp * 0.4f;
        }

        // staff stack of a system
        std::vector<int> staffPart, staffIdx;
        for (auto& pv : parts) for (int s = 0; s < pv.score->numStaves; ++s) { staffPart.push_back (pv.partIndex); staffIdx.push_back (s); }
        auto stackHeight = [&] (std::vector<float>& tops)
        {
            tops.clear(); float y = sp * 4.0f;   // room above for stems / bar numbers
            for (size_t i = 0; i < staffPart.size(); ++i)
            {
                if (i > 0) y += (staffPart[i] == staffPart[i - 1] ? sp * 6.5f : sp * 8.0f);
                tops.push_back (y); y += sp * 4.0f;
            }
            return y + sp * 4.0f;
        };

        const float usable = width - 2 * L.margin - sp * 6.0f;   // part names sit in the left margin
        float y = L.margin + (title.isNotEmpty() ? sp * 9.0f : 0.0f);
        int page = 0;
        auto meterAt = [&] (int b) { auto& m = parts[0].score->measures[(size_t) b]; return std::make_pair (m.num, m.den); };
        int b = 0;
        while (b < numBars)
        {
            System sys;
            float header = detail::clefWidth (sp) + detail::keyWidth (fifths, sp);
            float used = header; int first = b;
            while (b < numBars)
            {
                const bool timeHere = b == first ? (b == 0 || meterAt (b) != meterAt (b - 1)) : parts[0].score->measures[(size_t) b].showMeter;
                const float w = nat[(size_t) b].width + (timeHere ? detail::timeWidth (sp) : 0.0f);
                if (b > first && used + w > usable) break;
                used += w; ++b;
            }
            const bool last = b >= numBars;
            const float stretch = (last && used < usable * 0.65f) ? 1.0f : usable / juce::jmax (1.0f, used);
            float x = L.margin + sp * 6.0f;
            for (int k = first; k < b; ++k)
            {
                MeasureLayout ml; ml.index = k; ml.firstInSystem = k == first;
                const bool timeHere = k == first ? (k == 0 || meterAt (k) != meterAt (k - 1)) : parts[0].score->measures[(size_t) k].showMeter;
                ml.headerWidth = (k == first ? header : 0.0f) + (timeHere ? detail::timeWidth (sp) : 0.0f);
                const float bodyScale = stretch;
                ml.x = x;
                for (auto& a : nat[(size_t) k].anchors) ml.anchors.push_back ({ a.first, ml.headerWidth + a.second * bodyScale });
                ml.width = ml.headerWidth + nat[(size_t) k].width * bodyScale;
                x += ml.width;
                sys.measures.push_back (ml);
            }
            std::vector<float> tops; sys.height = stackHeight (tops);
            if (L.pages && y + sys.height > pageHeight - L.margin && ! (y <= L.margin + 1 + (page == 0 && title.isNotEmpty() ? sp * 9.0f : 0.0f)))
            { ++page; y = L.margin; }
            sys.y = y; sys.page = page; sys.staffPart = staffPart; sys.staffIndex = staffIdx;
            for (auto t : tops) sys.staffTops.push_back (y + t);
            y += sys.height;
            L.systems.push_back (sys);
        }
        L.numPages = page + 1;
        L.totalHeight = L.pages ? (float) L.numPages * pageHeight : y + L.margin;
        return L;
    }

    //==========================================================================
    struct Style
    {
        juce::Colour paper { 0xfffbf8f2 }, ink { 0xff15121c }, accent { 0xff6b3fd6 }, selection { 0xffd4ff3a }, playhead { 0xffff3a8a };
    };

    /** x of a beat inside a measure, interpolating between the measure's anchors. */
    inline float xForBeat (const MeasureLayout& ml, double beat)
    {
        auto& a = ml.anchors;
        if (a.empty()) return ml.x;
        if (beat <= a.front().first) return ml.x + a.front().second;
        for (size_t i = 1; i < a.size(); ++i)
            if (beat <= a[i].first + 1.0e-9)
            {
                const double t = (beat - a[i - 1].first) / juce::jmax (1.0e-9, a[i].first - a[i - 1].first);
                return ml.x + a[i - 1].second + (float) t * (a[i].second - a[i - 1].second);
            }
        return ml.x + a.back().second;
    }
    inline double beatForX (const MeasureLayout& ml, float x)
    {
        auto& a = ml.anchors;
        if (a.empty()) return 0;
        const float rx = x - ml.x;
        if (rx <= a.front().second) return a.front().first;
        for (size_t i = 1; i < a.size(); ++i)
            if (rx <= a[i].second) return a[i - 1].first + (a[i].first - a[i - 1].first) * (rx - a[i - 1].second) / juce::jmax (1.0f, a[i].second - a[i - 1].second);
        return a.back().first;
    }

    /** Paints one page (or, in scroll view, everything: page = -1). selected = sources to highlight per part. */
    inline void paint (juce::Graphics& g, Layout& L, const std::vector<PartView>& parts, int page, const Style& st,
                       const std::function<bool (int part, int source)>& isSelected, double playheadBeat = -1)
    {
        L.hits.clear();
        if (parts.empty()) return;
        const float sp = L.sp;
        auto font = musicFont (sp);
        g.setFont (font);
        auto glyph = [&] (juce::juce_wchar c, float x, float y) { g.drawSingleLineText (g1 (c), juce::roundToInt (x), juce::roundToInt (y)); };
        auto glyphW = [&] (juce::juce_wchar c) { return juce::GlyphArrangement::getStringWidth (font, g1 (c)); };
        const float headW = glyphW (Glyph::headBlack);
        const float pageTop = page >= 0 && L.pages ? (float) page * L.pageHeight : 0.0f;

        // title block on the first page
        if (L.title.isNotEmpty() && (page <= 0))
        {
            g.setColour (st.ink);
            g.setFont (juce::FontOptions (sp * 2.6f, juce::Font::bold));
            g.drawText (L.title, juce::Rectangle<float> (L.margin, L.margin - pageTop, L.width - 2 * L.margin, sp * 4.0f), juce::Justification::centred);
            if (L.subtitle.isNotEmpty())
            {
                g.setFont (juce::FontOptions (sp * 1.4f));
                g.drawText (L.subtitle, juce::Rectangle<float> (L.margin, L.margin + sp * 4.5f - pageTop, L.width - 2 * L.margin, sp * 2.5f), juce::Justification::centredRight);
            }
            g.setFont (font);
        }

        for (auto& sys : L.systems)
        {
            if (page >= 0 && L.pages && sys.page != page) continue;
            const float oy = -pageTop;
            const float left = sys.measures.empty() ? L.margin : sys.measures.front().x, right = sys.measures.empty() ? L.width - L.margin : sys.measures.back().x + sys.measures.back().width;
            g.setColour (st.ink);
            // staves
            for (size_t si = 0; si < sys.staffTops.size(); ++si)
            {
                const float top = sys.staffTops[si] + oy;
                for (int l = 0; l < 5; ++l) g.fillRect (left, top + l * sp - sp * 0.06f, right - left, sp * 0.12f);
            }
            // part names, braces, system barline
            g.setFont (juce::FontOptions (sp * 1.3f));
            for (size_t si = 0; si < sys.staffTops.size(); ++si)
            {
                if (sys.staffIndex[si] != 0) continue;
                const auto& pv = parts[(size_t) juce::jlimit (0, (int) parts.size() - 1, sys.staffPart[si])];
                const float top = sys.staffTops[si] + oy;
                const float bottom = pv.score->numStaves > 1 ? sys.staffTops[si + 1] + oy + 4 * sp : top + 4 * sp;
                g.drawText (pv.name, juce::Rectangle<float> (L.margin - sp, top, sp * 6.0f, bottom - top), juce::Justification::centredLeft, true);
                if (pv.score->numStaves > 1)
                {
                    // brace: the font's brace scaled to the grand staff
                    juce::GlyphArrangement ga; ga.addLineOfText (font, g1 (Glyph::brace), 0, 0);
                    juce::Path bp; ga.createPath (bp);
                    auto bb = bp.getBounds();
                    if (! bb.isEmpty())
                        g.fillPath (bp, juce::AffineTransform::translation (-bb.getX(), -bb.getY()).scaled (sp * 1.1f / bb.getWidth(), (bottom - top) / bb.getHeight()).translated (left - sp * 1.4f, top));
                }
            }
            g.setFont (font);
            const float sysTop = sys.staffTops.front() + oy, sysBottom = sys.staffTops.back() + oy + 4 * sp;
            g.fillRect (left - sp * 0.08f, sysTop, sp * 0.16f, sysBottom - sysTop);
            // bar number
            if (! sys.measures.empty())
            {
                g.setFont (juce::FontOptions (sp * 1.1f, juce::Font::italic));
                g.drawText (juce::String (sys.measures.front().index + 1), juce::Rectangle<float> (left, sysTop - sp * 3.2f, sp * 4, sp * 1.5f), juce::Justification::centredLeft);
                g.setFont (font);
            }

            for (auto& ml : sys.measures)
            {
                for (size_t si = 0; si < sys.staffTops.size(); ++si)
                {
                    const auto& pv = parts[(size_t) juce::jlimit (0, (int) parts.size() - 1, sys.staffPart[si])];
                    const auto& sc = *pv.score;
                    if (ml.index >= (int) sc.measures.size()) continue;
                    const auto& m = sc.measures[(size_t) ml.index];
                    const int sIdx = sys.staffIndex[si];
                    if (sIdx >= (int) m.staves.size()) continue;
                    const auto& staff = m.staves[(size_t) sIdx];
                    const float top = sys.staffTops[si] + oy, bottom = top + 4 * sp;
                    auto yLine = [&] (int line) { return bottom - (float) line * sp * 0.5f; };
                    const bool bass = staff.clef == Notation::Clef::bass;
                    float hx = ml.x + sp * 0.5f;
                    if (ml.firstInSystem)
                    {
                        glyph (bass ? Glyph::fClef : Glyph::gClef, hx, yLine (bass ? 6 : 2));
                        hx += detail::clefWidth (sp);
                        // key signature
                        static const int sharpLinesT[7] = { 8, 5, 9, 6, 3, 7, 4 }, flatLinesT[7] = { 4, 7, 3, 6, 2, 5, 1 };
                        const int k = sc.key.fifths;
                        for (int i = 0; i < std::abs (k) && i < 7; ++i)
                        {
                            int line = k > 0 ? sharpLinesT[i] : flatLinesT[i];
                            if (bass) line -= 2;
                            glyph (k > 0 ? Glyph::sharp : Glyph::flat, hx + i * sp, yLine (line));
                        }
                        hx += detail::keyWidth (k, sp);
                    }
                    if (m.showMeter || (ml.firstInSystem && ml.index == 0))
                    {
                        auto digits = [&] (int v, int line)
                        {
                            juce::String s; for (auto c : juce::String (v)) s << g1 ((juce::juce_wchar) (Glyph::time0 + (c - '0')));
                            const float w = juce::GlyphArrangement::getStringWidth (font, s);
                            g.drawSingleLineText (s, juce::roundToInt (hx + sp * 1.2f - w / 2), juce::roundToInt (yLine (line)));
                        };
                        digits (m.num, 6); digits (m.den, 2);
                    }
                    // barline at the end of the measure
                    g.fillRect (ml.x + ml.width - sp * 0.06f, top, sp * 0.12f, 4 * sp);
                    if (ml.index == (int) sc.measures.size() - 1)
                    {
                        g.fillRect (ml.x + ml.width - sp * 0.6f, top, sp * 0.12f, 4 * sp);
                        g.fillRect (ml.x + ml.width - sp * 0.3f, top, sp * 0.4f, 4 * sp);
                    }

                    // events
                    struct StemInfo { float x, yEnd, yHeadFar; bool up; int value; int group; bool triplet; float xHeadLeft; };
                    std::vector<StemInfo> stems; stems.reserve (staff.events.size());
                    for (size_t ei = 0; ei < staff.events.size(); ++ei)
                    {
                        const auto& e = staff.events[ei];
                        const float ex = xForBeat (ml, e.start);
                        if (e.rest)
                        {
                            const bool wholeBar = e.value == 1;
                            const auto c = e.value == 1 ? Glyph::restWhole : e.value == 2 ? Glyph::restHalf : e.value == 4 ? Glyph::restQuarter : e.value == 8 ? Glyph::rest8 : e.value == 16 ? Glyph::rest16 : Glyph::rest32;
                            const float rx = wholeBar && staff.events.size() == 1 ? ml.x + ml.headerWidth + (ml.width - ml.headerWidth) / 2 - glyphW (c) / 2 : ex;
                            glyph (c, rx, yLine (e.value == 1 ? 6 : 4));
                            for (int d = 0; d < e.dots; ++d) glyph (Glyph::dot, rx + glyphW (c) + sp * (0.3f + d * 0.5f), yLine (5));
                            stems.push_back ({ 0, 0, 0, true, 0, -1, e.triplet, ex });
                            continue;
                        }
                        // heads: seconds in a chord sit on the other side of the stem
                        float lowY = -1e9f, highY = 1e9f;
                        int prevLine = -1000; bool flip = false;
                        for (size_t hi = 0; hi < e.heads.size(); ++hi)
                        {
                            const auto& h = e.heads[hi];
                            const float hy = yLine (h.staffLine);
                            flip = (h.staffLine - prevLine == 1) ? ! flip : false;
                            prevLine = h.staffLine;
                            const float xOff = flip ? (e.stemUp ? headW - sp * 0.1f : -(headW - sp * 0.1f)) : 0.0f;
                            // ledger lines
                            g.setColour (st.ink);
                            for (int l = -2; l >= h.staffLine && l >= -12; l -= 2) g.fillRect (ex + xOff - sp * 0.4f, yLine (l) - sp * 0.08f, headW + sp * 0.8f, sp * 0.16f);
                            for (int l = 10; l <= h.staffLine && l <= 22; l += 2) g.fillRect (ex + xOff - sp * 0.4f, yLine (l) - sp * 0.08f, headW + sp * 0.8f, sp * 0.16f);
                            const bool sel = isSelected && isSelected (pv.partIndex, h.source);
                            g.setColour (sel ? st.accent : st.ink);
                            glyph (e.value == 1 ? Glyph::headWhole : e.value == 2 ? Glyph::headHalf : Glyph::headBlack, ex + xOff, hy);
                            if (sel)
                            {
                                g.setColour (st.selection.withAlpha (0.45f));
                                g.fillEllipse (ex + xOff - sp * 0.35f, hy - sp * 0.85f, headW + sp * 0.7f, sp * 1.7f);
                                g.setColour (st.accent);
                                glyph (e.value == 1 ? Glyph::headWhole : e.value == 2 ? Glyph::headHalf : Glyph::headBlack, ex + xOff, hy);
                            }
                            if (h.showAccidental)
                            {
                                const auto ac = h.shownAccidental == 1 ? Glyph::sharp : h.shownAccidental == -1 ? Glyph::flat : h.shownAccidental == 2 ? Glyph::dblSharp : h.shownAccidental == -2 ? Glyph::dblFlat : Glyph::natural;
                                glyph (ac, ex - glyphW (ac) - sp * 0.25f, hy);
                            }
                            for (int d = 0; d < e.dots; ++d)
                                glyph (Glyph::dot, ex + headW + sp * (0.35f + d * 0.5f), yLine (h.staffLine % 2 == 0 ? h.staffLine + 1 : h.staffLine));
                            // ties: to the next event's head of the same pitch (or toward the bar end)
                            if (h.tieToNext)
                            {
                                float tx2 = ml.x + ml.width - sp * 0.3f;
                                if (ei + 1 < staff.events.size()) tx2 = xForBeat (ml, staff.events[ei + 1].start) - sp * 0.15f;
                                const float tx1 = ex + headW + sp * 0.15f;
                                const float dir = e.stemUp ? 1.0f : -1.0f, ty = hy + dir * sp * 0.6f;
                                juce::Path tie; tie.startNewSubPath (tx1, ty);
                                tie.quadraticTo ((tx1 + tx2) / 2, ty + dir * sp * 1.1f, tx2, ty);
                                tie.quadraticTo ((tx1 + tx2) / 2, ty + dir * sp * 0.75f, tx1, ty);
                                tie.closeSubPath();
                                g.fillPath (tie);
                            }
                            L.hits.push_back ({ juce::Rectangle<float> (ex + xOff - sp * 0.2f, hy - sp * 0.7f, headW + sp * 0.4f, sp * 1.4f), pv.partIndex, sIdx, h.source, m.startBeat + e.start });
                            lowY = juce::jmax (lowY, hy); highY = juce::jmin (highY, hy);
                        }
                        g.setColour (st.ink);
                        if (e.value >= 2)
                        {
                            const float sx = e.stemUp ? ex + headW - sp * 0.06f : ex + sp * 0.06f;
                            float yEnd = e.stemUp ? highY - sp * 3.5f : lowY + sp * 3.5f;
                            // stems reach the middle line when the notes are far outside the staff
                            if (e.stemUp && yEnd > yLine (4)) yEnd = yLine (4);
                            if (! e.stemUp && yEnd < yLine (4)) yEnd = yLine (4);
                            stems.push_back ({ sx, yEnd, e.stemUp ? lowY : highY, e.stemUp, e.value, e.beamGroup, e.triplet, ex });
                        }
                        else stems.push_back ({ 0, 0, 0, true, 1, -1, e.triplet, ex });
                    }
                    // beams: straight between the group's first and last stem ends (gentle slope), stems extended to it
                    std::map<int, std::vector<size_t>> groups;
                    for (size_t i = 0; i < stems.size(); ++i) if (stems[i].group >= 0) groups[stems[i].group].push_back (i);
                    for (auto& [gid, idx] : groups)
                    {
                        auto& a = stems[idx.front()]; auto& b2 = stems[idx.back()];
                        const bool up = a.up;
                        float y0 = a.yEnd, y1 = b2.yEnd;
                        // keep every stem at least 2.5 spaces long
                        for (auto i : idx) { auto& s2 = stems[i]; if (up) { y0 = juce::jmin (y0, s2.yHeadFar - sp * 2.5f - sp * 0.0f); } else { y0 = juce::jmax (y0, s2.yHeadFar + sp * 2.5f); } }
                        const float slope = juce::jlimit (-sp, sp, (y1 - a.yEnd)) / juce::jmax (1.0f, b2.x - a.x);
                        auto beamY = [&] (float x) { return y0 + slope * (x - a.x); };
                        for (auto i : idx)
                        {
                            auto& s2 = stems[i];
                            s2.yEnd = beamY (s2.x);
                            if (up) s2.yEnd = juce::jmin (s2.yEnd, s2.yHeadFar - sp * 2.5f); else s2.yEnd = juce::jmax (s2.yEnd, s2.yHeadFar + sp * 2.5f);
                        }
                        const float th = sp * 0.5f;
                        auto drawBeam = [&] (float xa, float xb, float off)
                        {
                            juce::Path bp; const float dy = up ? off : -off;
                            bp.startNewSubPath (xa, beamY (xa) + dy); bp.lineTo (xb, beamY (xb) + dy);
                            bp.lineTo (xb, beamY (xb) + dy + (up ? th : -th)); bp.lineTo (xa, beamY (xa) + dy + (up ? th : -th)); bp.closeSubPath();
                            g.fillPath (bp);
                        };
                        drawBeam (a.x, b2.x + sp * 0.06f, 0);
                        // secondary beams for 16ths / 32nds (partial beams point toward their neighbour)
                        for (int level = 1; level <= 2; ++level)
                        {
                            const int need = level == 1 ? 16 : 32;
                            for (size_t k = 0; k < idx.size(); ++k)
                            {
                                auto& s2 = stems[idx[k]]; if (s2.value < need) continue;
                                const bool nextToo = k + 1 < idx.size() && stems[idx[k + 1]].value >= need;
                                if (nextToo) drawBeam (s2.x, stems[idx[k + 1]].x + sp * 0.06f, level * sp * 0.75f);
                                else if (! (k > 0 && stems[idx[k - 1]].value >= need)) drawBeam (k + 1 < idx.size() ? s2.x : s2.x - sp * 1.2f, k + 1 < idx.size() ? s2.x + sp * 1.2f : s2.x, level * sp * 0.75f);
                            }
                        }
                        if (stems[idx.front()].triplet)
                        {
                            const float mx = (a.x + b2.x) / 2;
                            glyph (Glyph::tuplet3, mx - glyphW (Glyph::tuplet3) / 2, up ? beamY (mx) - sp * 0.7f : beamY (mx) + sp * 2.0f);
                        }
                    }
                    // stems and flags
                    for (auto& s2 : stems)
                    {
                        if (s2.value < 2) continue;
                        g.fillRect (s2.x - sp * 0.06f, juce::jmin (s2.yEnd, s2.yHeadFar), sp * 0.12f, std::abs (s2.yHeadFar - s2.yEnd));
                        if (s2.group < 0 && s2.value >= 8)
                        {
                            const auto f = s2.up ? (s2.value == 8 ? Glyph::flag8Up : s2.value == 16 ? Glyph::flag16Up : Glyph::flag32Up)
                                                 : (s2.value == 8 ? Glyph::flag8Down : s2.value == 16 ? Glyph::flag16Down : Glyph::flag32Down);
                            glyph (f, s2.x - sp * 0.06f, s2.yEnd);
                        }
                        if (s2.group < 0 && s2.triplet) glyph (Glyph::tuplet3, s2.xHeadLeft, s2.up ? s2.yEnd - sp * 0.6f : s2.yEnd + sp * 1.8f);
                    }
                }
            }
            // playhead
            if (playheadBeat >= 0)
                for (auto& ml : sys.measures)
                {
                    const auto& m = parts[0].score->measures[(size_t) ml.index];
                    if (playheadBeat >= m.startBeat && playheadBeat < m.startBeat + m.lengthBeats)
                    {
                        const float px = xForBeat (ml, playheadBeat - m.startBeat);
                        g.setColour (st.playhead.withAlpha (0.8f));
                        g.fillRect (px - 1.0f, sysTop - sp * 2, 2.0f, sysBottom - sysTop + sp * 4);
                    }
                }
        }
        // page number
        if (L.pages && page >= 0)
        {
            g.setColour (st.ink.withAlpha (0.6f));
            g.setFont (juce::FontOptions (sp * 1.1f));
            g.drawText (juce::String (page + 1), juce::Rectangle<float> (0, L.pageHeight - L.margin * 0.8f, L.width, sp * 2), juce::Justification::centred);
        }
    }

    //==========================================================================
    /** Renders every page to images (for PNG / PDF). A4 at the given pixels per inch. */
    inline std::vector<juce::Image> renderPages (const std::vector<PartView>& parts, const juce::String& title, const juce::String& subtitle, bool letter, float ppi)
    {
        const float wIn = letter ? 8.5f : 8.2677f, hIn = letter ? 11.0f : 11.6929f;
        const int w = juce::roundToInt (wIn * ppi), h = juce::roundToInt (hIn * ppi);
        const float sp = ppi * 0.0703f;   // a 7 mm staff (1.75 mm spaces): the usual size for parts
        auto L = layout (parts, (float) w, sp, (float) h, title, subtitle);
        std::vector<juce::Image> pages;
        Style st; st.paper = juce::Colours::white; st.ink = juce::Colours::black;
        for (int p = 0; p < L.numPages; ++p)
        {
            juce::Image img (juce::Image::RGB, w, h, true);
            juce::Graphics g (img);
            g.fillAll (st.paper);
            paint (g, L, parts, p, st, {});
            pages.push_back (img);
        }
        return pages;
    }

    /** Writes the page images as a PDF (one JPEG-compressed page each, at the paper size). */
    inline bool writePdf (const juce::File& f, const std::vector<juce::Image>& pages, bool letter, juce::String& err)
    {
        if (pages.empty()) { err = "Nothing to print"; return false; }
        const float wPt = letter ? 612.0f : 595.276f, hPt = letter ? 792.0f : 841.89f;
        juce::MemoryOutputStream out;
        std::vector<juce::int64> offsets;
        auto obj = [&] (const juce::String& body) { offsets.push_back ((juce::int64) out.getDataSize()); out << (int) offsets.size() << " 0 obj\n" << body << "\nendobj\n"; };
        out << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
        const int n = (int) pages.size();
        // 1 catalog, 2 pages, then per page: page, content, image
        juce::String kids; for (int i = 0; i < n; ++i) kids << (3 + i * 3) << " 0 R ";
        obj ("<< /Type /Catalog /Pages 2 0 R >>");
        obj ("<< /Type /Pages /Kids [" + kids + "] /Count " + juce::String (n) + " >>");
        for (int i = 0; i < n; ++i)
        {
            const int pageObj = 3 + i * 3, contentObj = pageObj + 1, imgObj = pageObj + 2;
            obj ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + juce::String (wPt, 2) + " " + juce::String (hPt, 2) + "] /Resources << /XObject << /Im" + juce::String (i)
                 + " " + juce::String (imgObj) + " 0 R >> >> /Contents " + juce::String (contentObj) + " 0 R >>");
            const juce::String content = "q " + juce::String (wPt, 2) + " 0 0 " + juce::String (hPt, 2) + " 0 0 cm /Im" + juce::String (i) + " Do Q";
            obj ("<< /Length " + juce::String (content.length()) + " >>\nstream\n" + content + "\nendstream");
            juce::MemoryOutputStream jpg; juce::JPEGImageFormat jf; jf.setQuality (0.92f); jf.writeImageToStream (pages[(size_t) i], jpg);
            offsets.push_back ((juce::int64) out.getDataSize());
            out << imgObj << " 0 obj\n<< /Type /XObject /Subtype /Image /Width " << pages[(size_t) i].getWidth() << " /Height " << pages[(size_t) i].getHeight()
                << " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length " << (int) jpg.getDataSize() << " >>\nstream\n";
            out.write (jpg.getData(), jpg.getDataSize());
            out << "\nendstream\nendobj\n";
        }
        const auto xref = (juce::int64) out.getDataSize();
        out << "xref\n0 " << (int) (offsets.size() + 1) << "\n0000000000 65535 f \n";
        for (auto o : offsets) out << juce::String (o).paddedLeft ('0', 10) << " 00000 n \n";
        out << "trailer\n<< /Size " << (int) (offsets.size() + 1) << " /Root 1 0 R >>\nstartxref\n" << xref << "\n%%EOF\n";
        if (! f.replaceWithData (out.getData(), out.getDataSize())) { err = "Couldn't write " + f.getFullPathName(); return false; }
        return true;
    }
}
