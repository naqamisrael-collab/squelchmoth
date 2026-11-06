#pragma once
// Squelchmoth - the Edit window's live state: edit mode (Shuffle / Slip / Spot / Grid), edit tool
// (Zoomer / Trim / Selector / Grabber / Scrubber / Pencil / Smart), grid & nudge values, the Edit
// selection (time range across tracks), clip & track selection, zoom and per-track heights.

#include "Theme.h"
#include "Prefs.h"

enum class EditMode  { shuffle, slip, spot, grid };
enum class EditTool  { zoomer, trim, selector, grabber, scrubber, pencil, smart };

struct GridValue
{
    const char* name;
    double beats;        // 0 = follow zoom (auto), <0 = special (seconds)
};

static const GridValue gridValues[] = {
    { "Auto (follows zoom)", 0.0 },
    { "1 bar",              -1.0 },   // resolved from the time signature
    { "1/2 note",            2.0 },
    { "1/4 note",            1.0 },
    { "1/8 note",            0.5 },
    { "1/16 note",           0.25 },
    { "1/32 note",           0.125 },
    { "1/64 note",           0.0625 },
    { "1/8 triplet",         1.0 / 3.0 },
    { "1/16 triplet",        1.0 / 6.0 },
    { "1/4 dotted",          1.5 },
    { "1/8 dotted",          0.75 },
    { "1/16 dotted",         0.375 },
    { "1 second",           -2.0 },
    { "100 ms",             -3.0 },
    { "10 ms",              -4.0 },
    { "1 ms",               -5.0 },
    { "1 sample",           -6.0 } };

struct TimelineState
{
    // view
    double viewStart = 0.0;          // seconds at left edge of lanes
    double secondsPerPixel = 0.02;
    std::map<te::EditItemID, int> trackHeights;
    std::set<te::EditItemID> collapsedFolders;

    // Pro Tools edit modes & tools
    EditMode editMode = EditMode::slip;
    EditTool tool = EditTool::smart;
    bool snap = false;               // Grid combined with Shuffle or Slip (Pro Tools: Shift-click the Grid button)
    bool shuffleLock = false;        // Pro Tools Shuffle Lock: Cmd-click Shuffle so F1 / clicks can't enter Shuffle
    bool gridRelative = false;       // Relative Grid: keep the clip's offset from the grid when moving
    int gridIndex = 0;               // index into gridValues
    int nudgeIndex = 5;              // index into gridValues (default 1/16)
    bool tabToTransients = false;
    bool linkTrackAndEditSelection = true;
    bool insertionFollowsPlayback = false;
    bool loopPlaybackLinked = true;  // "Link Timeline and Edit Selection"

    // selection
    std::optional<te::TimeRange> timeSelection;          // the Edit selection (time)
    std::vector<te::EditItemID> selectedTrackIDs;        // tracks covered by the Edit selection / clicked headers
    te::Clip::Ptr selectedClip;                          // primary selected clip
    std::vector<te::Clip::Ptr> selectedClips;            // all selected clips (includes selectedClip)
    te::AudioTrack* selectedTrack = nullptr;             // primary selected track (header click)
    te::Track* selectedRow = nullptr;                    // any row kind (folder tracks too)

    // transient scrub & misc
    double lastClickTime = 0.0;

    //==========================================================================
    /** Pro Tools track heights (Micro .. Extreme); heights are saved with the session on each track. */
    struct HeightPreset { const char* name; int px; };
    static const std::array<HeightPreset, 7>& heightPresets()
    {
        static const std::array<HeightPreset, 7> p {{ { "Micro", 22 }, { "Mini", 34 }, { "Small", 52 }, { "Medium", 84 }, { "Large", 130 }, { "Jumbo", 200 }, { "Extreme", 320 } }};
        return p;
    }
    static int presetIndexFor (int px)
    {
        int best = 0;
        for (int i = 0; i < (int) heightPresets().size(); ++i) if (std::abs (heightPresets()[(size_t) i].px - px) < std::abs (heightPresets()[(size_t) best].px - px)) best = i;
        return best;
    }

    int heightFor (te::Track* t) const
    {
        auto it = trackHeights.find (t->itemID);
        if (it != trackHeights.end()) return it->second;
        const int saved = (int) t->state.getProperty ("sqmHeight", 0);
        return saved > 0 ? saved : Prefs::defaultTrackHeight();
    }

    void setHeight (te::Track* t, int px)
    {
        px = juce::jlimit (heightPresets().front().px, 1200, px);
        trackHeights[t->itemID] = px;
        if ((int) t->state.getProperty ("sqmHeight", 0) != px) t->state.setProperty ("sqmHeight", px, nullptr);
    }

    /** Ctrl+Up / Ctrl+Down: step to the next larger / smaller preset. */
    void stepHeight (te::Track* t, int dir)
    {
        const int cur = heightFor (t);
        const auto& p = heightPresets();
        if (dir > 0) { for (auto& h : p) if (h.px > cur) { setHeight (t, h.px); return; } }
        else for (int i = (int) p.size() - 1; i >= 0; --i) if (p[(size_t) i].px < cur) { setHeight (t, p[(size_t) i].px); return; }
    }

    float timeToX (double secs) const    { return (float) ((secs - viewStart) / secondsPerPixel); }
    double xToTime (float x) const       { return viewStart + x * secondsPerPixel; }

    bool isSelected (te::Clip* c) const
    {
        for (auto& p : selectedClips) if (p.get() == c) return true;
        return false;
    }

    bool isTrackSelected (te::Track* t) const
    {
        if (t == nullptr) return false;
        if (selectedRow == t) return true;
        return std::find (selectedTrackIDs.begin(), selectedTrackIDs.end(), t->itemID) != selectedTrackIDs.end();
    }

    void selectClip (te::Clip::Ptr c, bool add = false)
    {
        if (! add) selectedClips.clear();
        if (c != nullptr)
        {
            if (! isSelected (c.get())) selectedClips.push_back (c);
            selectedClip = c;
        }
        else if (! add) selectedClip = nullptr;
    }

    void deselectClips()     { selectedClips.clear(); selectedClip = nullptr; }

    void selectTrack (te::Track* t, bool add = false)
    {
        if (! add) selectedTrackIDs.clear();
        selectedRow = t;
        selectedTrack = dynamic_cast<te::AudioTrack*> (t);
        if (t != nullptr && ! isTrackSelected (t)) selectedTrackIDs.push_back (t->itemID);
    }

    void dropReferencesTo (te::Track* t)
    {
        if (selectedTrack == t) selectedTrack = nullptr;
        if (selectedRow == t) selectedRow = nullptr;
        if (selectedClip != nullptr && selectedClip->getClipTrack() == t) selectedClip = nullptr;
        selectedClips.erase (std::remove_if (selectedClips.begin(), selectedClips.end(),
                                             [t] (auto& c) { return c == nullptr || c->getClipTrack() == t || c->getClipTrack() == nullptr; }),
                             selectedClips.end());
        if (t != nullptr)
            selectedTrackIDs.erase (std::remove (selectedTrackIDs.begin(), selectedTrackIDs.end(), t->itemID), selectedTrackIDs.end());
    }

    void clearAll()
    {
        timeSelection.reset();
        selectedTrackIDs.clear();
        selectedClips.clear();
        selectedClip = nullptr;
        selectedTrack = nullptr;
        selectedRow = nullptr;
        trackHeights.clear();
        collapsedFolders.clear();
    }

    //==========================================================================
    /** Grid size in seconds at a time position. */
    double gridSeconds (te::Edit& edit, double atSeconds) const
    {
        return gridBeatsOrSeconds (edit, gridIndex, atSeconds, true);
    }

    double nudgeSeconds (te::Edit& edit, double atSeconds) const
    {
        return gridBeatsOrSeconds (edit, nudgeIndex, atSeconds, false);
    }

    double gridBeats (te::Edit& edit) const
    {
        auto& gv = gridValues[juce::jlimit (0, (int) std::size (gridValues) - 1, gridIndex)];
        if (gv.beats > 0) return gv.beats;
        if (gv.beats == -1.0) return (double) edit.tempoSequence.getTimeSigAt (te::TimePosition::fromSeconds (viewStart)).numerator.get();
        if (gv.beats == 0.0) return autoGridBeats (edit);
        return 0.0;   // time-based grid
    }

    double autoGridBeats (te::Edit& edit) const
    {
        const auto at = te::TimePosition::fromSeconds (juce::jmax (0.0, viewStart));
        auto pxPerBeat = 60.0 / edit.tempoSequence.getTempoAt (at).getBpm() / secondsPerPixel;
        if (pxPerBeat > 320) return 0.0625;
        if (pxPerBeat > 160) return 0.125;
        if (pxPerBeat > 80)  return 0.25;
        if (pxPerBeat > 40)  return 0.5;
        if (pxPerBeat > 14)  return 1.0;
        return (double) edit.tempoSequence.getTimeSigAt (at).numerator.get();
    }

    /** Snaps a time to the current grid (always, regardless of mode). */
    double snapToGrid (te::Edit& edit, double secs) const
    {
        auto& gv = gridValues[juce::jlimit (0, (int) std::size (gridValues) - 1, gridIndex)];
        secs = juce::jmax (0.0, secs);
        if (gv.beats < -1.0)
        {
            auto step = gridBeatsOrSeconds (edit, gridIndex, secs, true);
            return step > 0 ? std::round (secs / step) * step : secs;
        }
        auto& ts = edit.tempoSequence;
        const auto t = te::TimePosition::fromSeconds (secs);
        // grid lines restart at every bar line, so meter changes are honoured (3/4 after 4/4 etc.)
        auto bb = ts.toBarsAndBeats (t);
        const auto barStart = ts.toTime (te::tempo::BarsAndBeats { bb.bars, te::BeatDuration(), 0 });
        const auto nextBar = ts.toTime (te::tempo::BarsAndBeats { bb.bars + 1, te::BeatDuration(), 0 });
        const bool barGrid = gv.beats == -1.0 || (gv.beats == 0.0 && autoGridBeats (edit) >= ts.getTimeSigAt (t).numerator.get());
        if (barGrid) return (secs - barStart.inSeconds() < nextBar.inSeconds() - secs) ? barStart.inSeconds() : nextBar.inSeconds();
        auto g = gridBeats (edit);
        if (g <= 0) return secs;
        const double b0 = ts.toBeats (barStart).inBeats(), b1 = ts.toBeats (nextBar).inBeats();
        const double local = ts.toBeats (t).inBeats() - b0;
        const double snapped = juce::jmin (b1 - b0, std::round (local / g) * g);
        return ts.toTime (te::BeatPosition::fromBeats (b0 + snapped)).inSeconds();
    }

    /** Snaps only when the edit mode / snap toggle says so. */
    double snapIfNeeded (te::Edit& edit, double secs, bool forceOff = false) const
    {
        if (forceOff) return juce::jmax (0.0, secs);
        if (editMode == EditMode::grid || snap) return snapToGrid (edit, secs);
        return juce::jmax (0.0, secs);
    }

private:
    double gridBeatsOrSeconds (te::Edit& edit, int index, double atSeconds, bool allowAuto) const
    {
        auto& gv = gridValues[juce::jlimit (0, (int) std::size (gridValues) - 1, index)];
        auto& ts = edit.tempoSequence;
        auto beatsToSecs = [&] (double beats)
        {
            auto b0 = ts.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, atSeconds)));
            return (ts.toTime (b0 + te::BeatDuration::fromBeats (beats)) - ts.toTime (b0)).inSeconds();
        };
        if (gv.beats > 0)  return beatsToSecs (gv.beats);
        if (gv.beats == 0) return beatsToSecs (allowAuto ? autoGridBeats (edit) : 0.25);
        if (gv.beats == -1.0) return beatsToSecs ((double) ts.getTimeSigAt (te::TimePosition::fromSeconds (juce::jmax (0.0, atSeconds))).numerator.get());
        if (gv.beats == -2.0) return 1.0;
        if (gv.beats == -3.0) return 0.1;
        if (gv.beats == -4.0) return 0.01;
        if (gv.beats == -5.0) return 0.001;
        return 1.0 / juce::jmax (1.0, edit.engine.getDeviceManager().getSampleRate());
    }
};

/** Pro Tools "View > Edit Window Views": optional columns beside the track names. */
namespace EditViews
{
    inline bool io()        { return Prefs::getBool ("ev_io", true); }
    inline bool insAE()     { return Prefs::getBool ("ev_insAE", false); }
    inline bool insFJ()     { return Prefs::getBool ("ev_insFJ", false); }
    inline bool sendAE()    { return Prefs::getBool ("ev_sendAE", false); }
    inline bool sendFJ()    { return Prefs::getBool ("ev_sendFJ", false); }
    inline bool comments()  { return Prefs::getBool ("ev_comments", false); }
    constexpr int ioW = 112, slotW = 104, commentsW = 150;
    inline int width()      { return (io() ? ioW : 0) + (insAE() ? slotW : 0) + (insFJ() ? slotW : 0) + (sendAE() ? slotW : 0) + (sendFJ() ? slotW : 0) + (comments() ? commentsW : 0); }
}

static constexpr int baseHeaderWidth = 250;

/** Track > Clear All Clip Indicators bumps this; every meter compares it on its next tick. */
inline int& clipIndicatorResetCounter()   { static int c = 0; return c; }
inline int headerWidthNow()     { return baseHeaderWidth + EditViews::width(); }

inline const char* editModeName (EditMode m)
{
    switch (m) { case EditMode::shuffle: return "Shuffle"; case EditMode::slip: return "Slip"; case EditMode::spot: return "Spot"; default: return "Grid"; }
}

inline const char* editToolName (EditTool t)
{
    switch (t)
    {
        case EditTool::zoomer: return "Zoomer"; case EditTool::trim: return "Trim"; case EditTool::selector: return "Selector";
        case EditTool::grabber: return "Grabber"; case EditTool::scrubber: return "Scrubber"; case EditTool::pencil: return "Pencil";
        default: return "Smart Tool";
    }
}
