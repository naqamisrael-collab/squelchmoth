#pragma once
// Squelchmoth - the clip lanes of the Edit window. All seven Pro Tools edit tools and the four edit
// modes live here: Zoomer, Trim, Selector, Grabber, Scrubber, Pencil and the Smart Tool, in Shuffle,
// Slip, Spot or Grid mode. Also clip gain lines, fades, multi-selection, the Edit (time) selection,
// transient detection for Tab-to-Transient, drag & drop import and the clip / lane context menus.

#include "EditOps.h"
#include "PluginMenu.h"
#include "DrumKit.h"
#include "QuickNew.h"
#include "TrackMixControls.h"

//==============================================================================
/** Finds transients in an audio clip (for Tab to Transient and separating at transients). */
struct Transients
{
    static std::vector<double> find (te::Engine& engine, te::WaveAudioClip& c, float sensitivity = 0.5f)
    {
        std::vector<double> out;
        te::AudioFile af (c.getPlaybackFile());
        auto reader = std::unique_ptr<juce::AudioFormatReader> (engine.getAudioFileFormatManager().readFormatManager.createReaderFor (af.getFile()));
        if (reader == nullptr) return out;

        auto pos = c.getPosition();
        auto sr = reader->sampleRate;
        auto speed = c.getSpeedRatio();
        auto start = (juce::int64) (pos.getOffset().inSeconds() * speed * sr);
        auto len = juce::jmin ((juce::int64) (pos.getLength().inSeconds() * speed * sr), (juce::int64) reader->lengthInSamples - start);
        if (len <= 0) return out;

        const int win = juce::jmax (64, (int) (sr * 0.005));
        juce::AudioBuffer<float> buf ((int) reader->numChannels, win);
        std::vector<float> energy;
        for (juce::int64 p = 0; p < len; p += win)
        {
            auto n = (int) juce::jmin ((juce::int64) win, len - p);
            reader->read (&buf, 0, n, start + p, true, true);
            float e = 0;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch) e += buf.getRMSLevel (ch, 0, n);
            energy.push_back (e / (float) juce::jmax (1, buf.getNumChannels()));
        }

        // onset = energy rises sharply compared with the recent average
        float avg = 0;
        const float threshold = juce::jmap (sensitivity, 0.0f, 1.0f, 6.0f, 1.8f);
        int cooldown = 0;
        for (size_t i = 1; i < energy.size(); ++i)
        {
            avg = avg * 0.9f + energy[i - 1] * 0.1f;
            if (cooldown > 0) { --cooldown; continue; }
            if (energy[i] > 0.01f && energy[i] > avg * threshold && energy[i] > energy[i - 1] * 1.5f)
            {
                out.push_back (pos.getStart().inSeconds() + (double) i * win / sr / speed);
                cooldown = (int) (0.05 * sr / win);   // 50 ms minimum spacing
            }
        }
        return out;
    }
};

//==============================================================================
class LaneCanvas : public juce::Component,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
{
public:
    LaneCanvas (Session& s, TimelineState& st) : session (s), state (st)   { startTimerHz (20); }

    std::function<void (te::MidiClip&)> onOpenMidiClip;
    static inline std::function<void (te::Clip&)> openInCutLab;   // the audio editor (set by MainComponent)
    std::function<void (te::StepClip&)> onOpenStepClip;
    std::function<void()> onSelectionChanged;
    std::function<void (double start, double end)> onZoomToRange;
    std::function<void (double factor, double anchorSecs)> onZoom;
    std::function<int (te::Track*)> getTrackY;
    std::function<te::Track* (int y)> getTrackAtY;
    std::function<juce::Array<te::Track*>()> getRows;

    void clearThumbnails()  { thumbnails.clear(); transientCache.clear(); }
    bool isDragging() const { return drag.mode != DragMode::none; }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        if (session.edit == nullptr) return;
        auto& edit = *session.edit;
        auto clip = g.getClipBounds();

        g.fillAll (Theme::bg());
        drawGrid (g, clip);

        for (auto row : getRows())
        {
            int y = getTrackY (row), h = state.heightFor (row);
            if (y > clip.getBottom() || y + h < clip.getY()) continue;
            bool selected = state.isTrackSelected (row);

            g.setColour (selected ? Theme::panel().withAlpha (0.55f) : Theme::panel().withAlpha (0.25f));
            g.fillRect (0, y, getWidth(), h - 1);
            g.setColour (Theme::line());
            g.drawHorizontalLine (y + h - 1, 0.0f, (float) getWidth());

            if (auto t = dynamic_cast<te::AudioTrack*> (row))
            {
                for (auto c : t->getClips())
                    drawClip (g, *c, *t, y, h);

                drawVolumeAutomation (g, *t, y, h);

                if (edit.getTransport().isRecording() && session.isArmed (*t))
                {
                    auto x1 = state.timeToX (edit.getTransport().getTimeWhenStarted().inSeconds());
                    auto x2 = state.timeToX (edit.getTransport().getPosition().inSeconds());
                    g.setColour (Theme::red().withAlpha (0.35f));
                    g.fillRect (juce::Rectangle<float> (x1, (float) y + 2, juce::jmax (2.0f, x2 - x1), (float) h - 5));
                }
            }
            else if (auto f = dynamic_cast<te::FolderTrack*> (row))
                drawFolderSummary (g, *f, y, h);
            else if (row->isMasterTrack())
            {
                g.setColour (Theme::dim().withAlpha (0.5f));
                g.setFont (Theme::font (11.0f, true));
                if (h > 30) g.drawText ("MASTER FADER  -  draw volume automation with the Pencil (F10)", 10, y + 4, 600, 14, juce::Justification::centredLeft);
                drawVolumeAutomation (g, *row, y, h);
            }

            // Edit (time) selection on selected tracks
            if (state.timeSelection && selected)
            {
                auto x1 = state.timeToX (state.timeSelection->getStart().inSeconds()), x2 = state.timeToX (state.timeSelection->getEnd().inSeconds());
                g.setColour (Theme::acid().withAlpha (0.16f));
                g.fillRect (juce::Rectangle<float> (x1, (float) y, juce::jmax (1.0f, x2 - x1), (float) h - 1));
                g.setColour (Theme::acid().withAlpha (0.7f));
                g.drawVerticalLine (juce::roundToInt (x1), (float) y, (float) (y + h - 1));
                g.drawVerticalLine (juce::roundToInt (x2), (float) y, (float) (y + h - 1));
            }
        }

        // markers
        for (auto m : session.getMarkers())
        {
            auto x = juce::roundToInt (state.timeToX (m->getPosition().getStart().inSeconds()));
            if (x < 0 || x > getWidth()) continue;
            g.setColour (Theme::amber().withAlpha (0.45f));
            for (int yy = 0; yy < getHeight(); yy += 8) g.fillRect (x, yy, 1, 4);
        }

        // loop range
        auto& tr = edit.getTransport();
        if (tr.looping.get())
        {
            auto lr = tr.getLoopRange();
            g.setColour (Theme::mint().withAlpha (0.06f));
            g.fillRect (juce::Rectangle<float> (state.timeToX (lr.getStart().inSeconds()), 0.0f,
                                                state.timeToX (lr.getEnd().inSeconds()) - state.timeToX (lr.getStart().inSeconds()), (float) getHeight()));
        }

        // Smart Tool crossfade being drawn: centred on the junction
        if (drag.mode == DragMode::crossfade)
        {
            const float half = std::abs (drag.bandEndX - drag.bandStartX);
            auto r = juce::Rectangle<float> (drag.bandStartX - half, drag.xfTop, half * 2.0f, drag.xfBottom - drag.xfTop);
            g.setColour (Theme::acid().withAlpha (0.18f));
            g.fillRect (r);
            g.setColour (Theme::acid());
            g.drawLine (r.getX(), r.getY(), r.getRight(), r.getBottom(), 1.5f);
            g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.5f);
        }

        // zoomer rubber band
        if (drag.mode == DragMode::zoomBand)
        {
            auto x1 = juce::jmin (drag.bandStartX, drag.bandEndX), x2 = juce::jmax (drag.bandStartX, drag.bandEndX);
            g.setColour (Theme::acid().withAlpha (0.12f));
            g.fillRect (x1, 0.0f, x2 - x1, (float) getHeight());
            g.setColour (Theme::acid());
            g.drawRect (juce::Rectangle<float> (x1, 0.0f, x2 - x1, (float) getHeight()), 1.0f);
        }
    }

    //==========================================================================
    // Hit testing
    enum class Zone { none, body, upper, lower, edgeStart, edgeEnd, fadeIn, fadeOut, gainLine, crossfade };

    struct Hit
    {
        te::Track* row = nullptr;
        te::AudioTrack* track = nullptr;
        te::Clip::Ptr clip;
        Zone zone = Zone::none;
        juce::Rectangle<float> clipRect;
    };

    Hit hitTest (juce::Point<float> p)
    {
        Hit h;
        h.row = getTrackAtY ((int) p.y);
        h.track = dynamic_cast<te::AudioTrack*> (h.row);
        if (h.track == nullptr) return h;
        h.clip = clipAt (*h.track, p);
        if (h.clip == nullptr) return h;

        int y = getTrackY (h.row), th = state.heightFor (h.row);
        auto pos = h.clip->getPosition();
        auto x1 = state.timeToX (pos.getStart().inSeconds()), x2 = state.timeToX (pos.getEnd().inSeconds());
        h.clipRect = { x1, (float) y + 2, juce::jmax (3.0f, x2 - x1), (float) th - 5 };
        auto r = h.clipRect;
        const float edge = juce::jmin (9.0f, r.getWidth() * 0.25f);
        bool nearStart = p.x - r.getX() < edge, nearEnd = r.getRight() - p.x < edge;
        bool topQuarter = p.y < r.getY() + juce::jmax (16.0f, r.getHeight() * 0.3f);
        bool isAudio = dynamic_cast<te::AudioClipBase*> (h.clip.get()) != nullptr;

        // Smart Tool: the bottom corner where two clips butt = crossfade
        const bool bottomQuarter = p.y > r.getBottom() - juce::jmax (16.0f, r.getHeight() * 0.3f);
        if (isAudio && bottomQuarter && r.getHeight() > 36 && (nearStart || nearEnd) && buttingNeighbour (*h.track, *h.clip, nearStart) != nullptr)
            h.zone = Zone::crossfade;
        else if (isAudio && topQuarter && nearStart && r.getHeight() > 36) h.zone = Zone::fadeIn;
        else if (isAudio && topQuarter && nearEnd && r.getHeight() > 36) h.zone = Zone::fadeOut;
        else if (nearStart) h.zone = Zone::edgeStart;
        else if (nearEnd) h.zone = Zone::edgeEnd;
        else if (isAudio && Prefs::clipGainLineVisible() && r.getHeight() > 40 && std::abs (p.y - gainLineY (*dynamic_cast<te::AudioClipBase*> (h.clip.get()), r)) < 4.0f) h.zone = Zone::gainLine;
        else h.zone = p.y < r.getCentreY() ? Zone::upper : Zone::lower;
        return h;
    }

    /** The tool that actually applies for this click (Smart tool picks one from the hit zone). */
    EditTool effectiveTool (const Hit& h, const juce::ModifierKeys& mods)
    {
        auto t = state.tool;
        if (t != EditTool::smart) return t;
        if (h.clip == nullptr) return EditTool::selector;
        switch (h.zone)
        {
            case Zone::edgeStart: case Zone::edgeEnd: return EditTool::trim;
            case Zone::fadeIn: case Zone::fadeOut: case Zone::gainLine: case Zone::crossfade: return EditTool::grabber;
            case Zone::upper: return EditTool::selector;
            default: return mods.isCommandDown() ? EditTool::selector : EditTool::grabber;
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (session.edit == nullptr) return;
        auto& edit = *session.edit;
        auto hit = hitTest (e.position);
        drag = {};
        drag.hit = hit;
        drag.mouseStartTime = state.xToTime (e.position.x);
        drag.startY = e.position.y;
        drag.mods = e.mods;

        if (e.mods.isPopupMenu())
        {
            if (hit.clip != nullptr) { if (! state.isSelected (hit.clip.get())) state.selectClip (hit.clip); showClipMenu (*hit.clip, *hit.track, e); }
            else if (hit.track != nullptr) showLaneMenu (*hit.track, e);
            else
                QuickNew::build (session).showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this] (int r)
                {
                    if (r > 0 && session.quickNewHook) session.quickNewHook (r, 0);
                });
            notify();
            return;
        }

        auto tool = effectiveTool (hit, e.mods);
        drag.tool = tool;

        if (hit.row != nullptr && ! (e.mods.isShiftDown() && tool == EditTool::selector))
            state.selectTrack (hit.row, e.mods.isCommandDown() && tool == EditTool::grabber);

        switch (tool)
        {
            case EditTool::zoomer:
                drag.mode = DragMode::zoomBand;
                drag.bandStartX = drag.bandEndX = e.position.x;
                break;

            case EditTool::scrubber:
                drag.mode = DragMode::scrub;
                edit.getTransport().setUserDragging (true);
                edit.getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, drag.mouseStartTime)));
                break;

            case EditTool::pencil:
                drag.mode = DragMode::pencil;
                pencilAt (e.position);
                break;

            case EditTool::trim:
                if (hit.clip != nullptr)
                {
                    state.selectClip (hit.clip, e.mods.isShiftDown());
                    if (isLocked (*hit.clip)) { session.log.add ("You", "Clip is locked (Clip > Lock / Unlock position)"); repaint(); break; }
                    drag.originalPos = hit.clip->getPosition();
                    bool start = hit.zone == Zone::edgeStart || hit.zone == Zone::fadeIn
                                 || (state.tool == EditTool::trim && hit.zone != Zone::edgeEnd && hit.zone != Zone::fadeOut && e.position.x < hit.clipRect.getCentreX());
                    drag.mode = start ? DragMode::trimStart : DragMode::trimEnd;
                    if (state.editMode == EditMode::spot) { drag.mode = DragMode::none; spotDialog (hit.clip, start ? 1 : 2); break; }
                    // Shuffle: the clips after this one follow the trim (either edge), like Pro Tools
                    if (state.editMode == EditMode::shuffle)
                        for (auto c : hit.track->getClips()) if (c->getPosition().getStart() >= drag.originalPos.getEnd() - 0.0005s && c != hit.clip.get()) drag.followers.push_back ({ c, c->getPosition().getStart() });
                }
                break;

            case EditTool::grabber:
                if (hit.clip != nullptr)
                {
                    if (hit.zone == Zone::crossfade)
                    {
                        const bool atStart = e.position.x - hit.clipRect.getX() < hit.clipRect.getRight() - e.position.x;
                        drag.xfLeft = atStart ? buttingNeighbour (*hit.track, *hit.clip, true) : hit.clip.get();
                        drag.xfRight = atStart ? hit.clip.get() : buttingNeighbour (*hit.track, *hit.clip, false);
                        drag.xfJunction = (atStart ? hit.clip->getPosition().getStart() : hit.clip->getPosition().getEnd()).inSeconds();
                        drag.bandStartX = drag.bandEndX = state.timeToX (drag.xfJunction);
                        drag.xfTop = hit.clipRect.getY(); drag.xfBottom = hit.clipRect.getBottom();
                        drag.mode = DragMode::crossfade;
                        break;
                    }
                    if (hit.zone == Zone::fadeIn || hit.zone == Zone::fadeOut)
                    {
                        state.selectClip (hit.clip, false);
                        drag.mode = hit.zone == Zone::fadeIn ? DragMode::fadeIn : DragMode::fadeOut;
                        if (auto ac = dynamic_cast<te::AudioClipBase*> (hit.clip.get())) drag.originalValue = (hit.zone == Zone::fadeIn ? ac->getFadeIn() : ac->getFadeOut()).inSeconds();
                        break;
                    }
                    if (hit.zone == Zone::gainLine)
                    {
                        state.selectClip (hit.clip, false);
                        drag.mode = DragMode::clipGain;
                        if (auto ac = dynamic_cast<te::AudioClipBase*> (hit.clip.get())) drag.originalValue = ac->getGainDB();
                        break;
                    }

                    bool add = e.mods.isShiftDown() || e.mods.isCommandDown();
                    if (add && state.isSelected (hit.clip.get()) && e.mods.isCommandDown())
                    {
                        state.selectedClips.erase (std::remove_if (state.selectedClips.begin(), state.selectedClips.end(), [&] (auto& c) { return c == hit.clip; }), state.selectedClips.end());
                        if (state.selectedClip == hit.clip) state.selectedClip = state.selectedClips.empty() ? nullptr : state.selectedClips.back();
                        break;
                    }
                    if (! state.isSelected (hit.clip.get()) || ! add) state.selectClip (hit.clip, add && state.isSelected (hit.clip.get()) ? true : add);
                    else state.selectedClip = hit.clip;
                    state.timeSelection = hit.clip->getPosition().time;
                    if (Prefs::linkTimelineAndEdit()) syncLoopToSelection();

                    if (state.editMode == EditMode::spot) { spotDialog (hit.clip); break; }

                    if (! e.mods.isAltDown() && isLocked (*hit.clip)) { session.log.add ("You", "Clip is locked (Clip > Lock / Unlock position)"); repaint(); break; }
                    drag.mode = e.mods.isAltDown() ? DragMode::copyMove : DragMode::move;
                    drag.originalPos = hit.clip->getPosition();
                    for (auto& c : state.selectedClips) if (drag.mode == DragMode::copyMove || ! isLocked (*c)) drag.others.push_back ({ c, c->getPosition().getStart() });
                    captureRows();
                    if (state.editMode == EditMode::shuffle && drag.mode == DragMode::move)
                    {
                        // Shuffle: every other clip on the track is a tile that re-packs around the dragged clip
                        for (auto c : hit.track->getClips())
                            if (c != hit.clip.get()) drag.tiles.push_back ({ c, c->getPosition().getStart().inSeconds() });
                        std::sort (drag.tiles.begin(), drag.tiles.end(), [] (auto& a, auto& b) { return a.second < b.second; });
                    }
                }
                else
                {
                    state.deselectClips();
                    state.timeSelection.reset();
                    edit.getTransport().setPosition (te::TimePosition::fromSeconds (snapIf (edit, drag.mouseStartTime, e.mods)));
                }
                break;

            case EditTool::selector:
            default:
            {
                drag.mode = DragMode::timeSelect;
                auto t = snapIf (edit, drag.mouseStartTime, e.mods);
                if (e.mods.isShiftDown() && state.timeSelection)
                {
                    // extend the existing selection
                    auto s = state.timeSelection->getStart().inSeconds(), en = state.timeSelection->getEnd().inSeconds();
                    drag.anchorTime = std::abs (t - s) > std::abs (t - en) ? s : en;
                    state.timeSelection = te::TimeRange (te::TimePosition::fromSeconds (juce::jmin (drag.anchorTime, t)), te::TimePosition::fromSeconds (juce::jmax (drag.anchorTime, t)));
                    if (hit.row != nullptr && ! state.isTrackSelected (hit.row)) state.selectedTrackIDs.push_back (hit.row->itemID);
                }
                else
                {
                    drag.anchorTime = t;
                    drag.anchorRow = hit.row;
                    state.deselectClips();
                    state.timeSelection = te::TimeRange (te::TimePosition::fromSeconds (t), te::TimePosition::fromSeconds (t));
                    if (! e.mods.isCommandDown()) edit.getTransport().setPosition (te::TimePosition::fromSeconds (t));
                }
                break;
            }
        }
        notify();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (session.edit == nullptr || drag.mode == DragMode::none) return;
        auto& edit = *session.edit;
        auto now = state.xToTime (e.position.x);
        auto delta = now - drag.mouseStartTime;
        const bool grid = gridActive (e.mods);   // Grid mode / Grid+Shuffle / Grid+Slip; Cmd swaps Grid <-> Slip while dragging

        switch (drag.mode)
        {
            case DragMode::zoomBand:
                drag.bandEndX = e.position.x;
                repaint();
                return;

            case DragMode::crossfade:
                drag.bandEndX = e.position.x;
                repaint();
                return;

            case DragMode::scrub:
                edit.getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, now)));
                return;

            case DragMode::pencil:
                pencilAt (e.position);
                return;

            case DragMode::timeSelect:
            {
                auto t = grid ? state.snapToGrid (edit, now) : juce::jmax (0.0, now);
                state.timeSelection = te::TimeRange (te::TimePosition::fromSeconds (juce::jmin (drag.anchorTime, t)), te::TimePosition::fromSeconds (juce::jmax (drag.anchorTime, t)));
                // tracks between the anchor row and the current row
                if (auto row = getTrackAtY ((int) e.position.y); row != nullptr && drag.anchorRow != nullptr)
                {
                    auto rows = getRows();
                    int a = rows.indexOf (drag.anchorRow), b = rows.indexOf (row);
                    if (a >= 0 && b >= 0)
                    {
                        state.selectedTrackIDs.clear();
                        for (int i = juce::jmin (a, b); i <= juce::jmax (a, b); ++i) state.selectedTrackIDs.push_back (rows[i]->itemID);
                        state.selectedRow = drag.anchorRow;
                        state.selectedTrack = dynamic_cast<te::AudioTrack*> (drag.anchorRow);
                    }
                }
                if (Prefs::linkTimelineAndEdit()) syncLoopToSelection();
                break;
            }

            case DragMode::move:
            case DragMode::copyMove:
            {
                auto& c = *drag.hit.clip;
                if (drag.mode == DragMode::copyMove && ! drag.copied && std::abs (e.getDistanceFromDragStartX()) > 4)
                {
                    // Opt-drag: duplicate the selection first, then move the duplicates
                    std::vector<std::pair<te::Clip::Ptr, te::TimePosition>> copies;
                    for (auto& [cp, start] : drag.others)
                        if (auto t = dynamic_cast<te::ClipTrack*> (cp->getClipTrack()))
                        {
                            auto st = cp->state.createCopy();
                            te::EditItemID::remapIDs (st, nullptr, edit);
                            if (auto nc = t->insertClipWithState (st)) copies.push_back ({ nc, start });
                        }
                    drag.others = copies;
                    drag.copied = true;
                    state.deselectClips();
                    for (auto& [cp, start] : copies) state.selectClip (cp, true);
                    if (! copies.empty()) drag.hit.clip = copies.front().first;
                }

                auto& primary = *drag.hit.clip;
                const double origStart = drag.originalPos.getStart().inSeconds();

                if (state.editMode == EditMode::shuffle && drag.mode == DragMode::move)
                {
                    shuffleDrag (primary, grid ? state.snapToGrid (edit, origStart + delta) : origStart + delta);
                    state.timeSelection = c.getPosition().time;
                    break;
                }

                double newStart = origStart + delta;
                if (grid)
                {
                    if (state.gridRelative)
                    {
                        // Relative Grid: move by whole grid steps, keeping the clip's own offset from the grid
                        auto g = juce::jmax (1e-6, state.gridSeconds (edit, origStart));
                        newStart = origStart + std::round (delta / g) * g;
                    }
                    else newStart = state.snapToGrid (edit, newStart);   // Absolute Grid: the clip start lands on a grid line
                }
                auto moveBy = newStart - origStart;
                double earliest = origStart;
                for (auto& [cp, start] : drag.others) earliest = juce::jmin (earliest, start.inSeconds());
                if (earliest + moveBy < 0) moveBy = -earliest;
                for (auto& [cp, start] : drag.others)
                    cp->setStart (te::TimePosition::fromSeconds (start.inSeconds() + moveBy), false, true);

                // vertical: every dragged clip moves by the same number of tracks (Pro Tools keeps their layout)
                moveRows (e.position.y);
                state.timeSelection = c.getPosition().time;
                break;
            }

            case DragMode::trimEnd:
            {
                auto& c = *drag.hit.clip;
                auto newEnd = grid ? state.snapToGrid (edit, drag.originalPos.getEnd().inSeconds() + delta) : drag.originalPos.getEnd().inSeconds() + delta;
                newEnd = juce::jmax (drag.originalPos.getStart().inSeconds() + 0.01, newEnd);
                c.setEnd (te::TimePosition::fromSeconds (newEnd), true);
                auto shift = te::TimeDuration::fromSeconds (newEnd - drag.originalPos.getEnd().inSeconds());
                for (auto& [cp, start] : drag.followers) cp->setStart (juce::jmax (te::TimePosition(), start + shift), false, true);
                break;
            }

            case DragMode::trimStart:
            {
                auto& c = *drag.hit.clip;
                auto newStart = grid ? state.snapToGrid (edit, drag.originalPos.getStart().inSeconds() + delta) : drag.originalPos.getStart().inSeconds() + delta;
                newStart = juce::jlimit (0.0, drag.originalPos.getEnd().inSeconds() - 0.01, newStart);
                auto shift = newStart - drag.originalPos.getStart().inSeconds();
                auto newOffset = drag.originalPos.offset.inSeconds() + shift;
                if (newOffset < 0 && dynamic_cast<te::WaveAudioClip*> (&c) != nullptr && ! c.isLooping()) { shift -= newOffset; newStart -= newOffset; newOffset = 0; }
                if (state.editMode == EditMode::shuffle)
                {
                    // Shuffle: the clip keeps its start; its end and every later clip move by the trimmed amount
                    const auto s0 = drag.originalPos.getStart();
                    const auto newLen = te::TimeDuration::fromSeconds (juce::jmax (0.01, drag.originalPos.getLength().inSeconds() - shift));
                    c.setPosition ({ { s0, s0 + newLen }, te::TimeDuration::fromSeconds (juce::jmax (0.0, newOffset)) });
                    const auto ripple = te::TimeDuration::fromSeconds (newLen.inSeconds() - drag.originalPos.getLength().inSeconds());
                    for (auto& [cp, start] : drag.followers) cp->setStart (juce::jmax (te::TimePosition(), start + ripple), false, true);
                    break;
                }
                c.setPosition ({ { te::TimePosition::fromSeconds (newStart), drag.originalPos.getEnd() }, te::TimeDuration::fromSeconds (juce::jmax (0.0, newOffset)) });
                break;
            }

            case DragMode::fadeIn:
            case DragMode::fadeOut:
                if (auto ac = dynamic_cast<te::AudioClipBase*> (drag.hit.clip.get()))
                {
                    auto len = ac->getPosition().getLength().inSeconds();
                    if (drag.mode == DragMode::fadeIn) ac->setFadeIn (te::TimeDuration::fromSeconds (juce::jlimit (0.0, len, drag.originalValue + delta)));
                    else ac->setFadeOut (te::TimeDuration::fromSeconds (juce::jlimit (0.0, len, drag.originalValue - delta)));
                }
                break;

            case DragMode::clipGain:
                if (auto ac = dynamic_cast<te::AudioClipBase*> (drag.hit.clip.get()))
                {
                    auto dy = drag.startY - e.position.y;
                    ac->setGainDB ((float) juce::jlimit (-60.0, 24.0, drag.originalValue + dy * (e.mods.isShiftDown() ? 0.05 : 0.25)));
                    gainTip = juce::String (ac->getGainDB(), 1) + " dB";
                }
                break;

            default: break;
        }
        notify();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (session.edit == nullptr) { drag = {}; return; }
        auto& edit = *session.edit;

        if (drag.mode == DragMode::zoomBand)
        {
            auto x1 = juce::jmin (drag.bandStartX, drag.bandEndX), x2 = juce::jmax (drag.bandStartX, drag.bandEndX);
            if (x2 - x1 > 6) { if (onZoomToRange) onZoomToRange (state.xToTime (x1), state.xToTime (x2)); }
            else if (onZoom) onZoom (e.mods.isAltDown() ? 1.6 : 0.6, state.xToTime (e.position.x));
        }
        else if (drag.mode == DragMode::scrub)
            edit.getTransport().setUserDragging (false);
        else if (drag.mode == DragMode::crossfade)
        {
            auto l = dynamic_cast<te::AudioClipBase*> (drag.xfLeft.get()), r = dynamic_cast<te::AudioClipBase*> (drag.xfRight.get());
            const double half = juce::jmax (0.005, std::abs (state.xToTime (drag.bandEndX) - drag.xfJunction));
            if (l != nullptr && r != nullptr)
            {
                edit.getUndoManager().beginNewTransaction ("Crossfade");
                auto len = EditOps::crossfade (*l, *r, { te::TimePosition::fromSeconds (juce::jmax (0.0, drag.xfJunction - half)), te::TimePosition::fromSeconds (drag.xfJunction + half) });
                session.log.add ("You", "Crossfade " + juce::String (len * 1000.0, 0) + " ms (equal power)");
            }
            repaint();
        }
        else if (drag.mode == DragMode::timeSelect)
        {
            if (state.timeSelection && state.timeSelection->getLength() < 0.0005s)
            {
                // a plain click: no range, just an insertion point
                auto t = state.timeSelection->getStart();
                state.timeSelection = te::TimeRange (t, t);
            }
        }
        else if (drag.mode == DragMode::trimStart || drag.mode == DragMode::trimEnd || drag.mode == DragMode::move || drag.mode == DragMode::copyMove)
        {
            for (auto& c : state.selectedClips) EditOps::tinyFades (c.get());
            // Pro Tools: one clip sounds at a time on a track - whatever the moved / trimmed clip now covers is overwritten
            if (state.editMode != EditMode::shuffle || drag.mode == DragMode::copyMove)
            {
                if (drag.mode == DragMode::trimStart || drag.mode == DragMode::trimEnd) { if (drag.hit.clip != nullptr) EditOps::resolveOverlaps (*drag.hit.clip); }
                else EditOps::resolveOverlaps (state.selectedClips);
            }
        }

        drag = {};
        gainTip.clear();
        notify();
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto hit = hitTest (e.position);
        auto tool = effectiveTool (hit, e.mods);
        auto cursor = juce::MouseCursor::NormalCursor;
        switch (tool)
        {
            case EditTool::zoomer:   cursor = juce::MouseCursor::CrosshairCursor; break;
            case EditTool::selector: cursor = juce::MouseCursor::IBeamCursor; break;
            case EditTool::scrubber: cursor = juce::MouseCursor::LeftRightResizeCursor; break;
            case EditTool::pencil:   cursor = juce::MouseCursor::CrosshairCursor; break;
            case EditTool::trim:     cursor = hit.clip ? (hit.zone == Zone::edgeEnd || (state.tool == EditTool::trim && e.position.x >= hit.clipRect.getCentreX()) ? juce::MouseCursor::RightEdgeResizeCursor : juce::MouseCursor::LeftEdgeResizeCursor) : juce::MouseCursor::NormalCursor; break;
            case EditTool::grabber:
                cursor = hit.clip == nullptr ? juce::MouseCursor::PointingHandCursor
                       : (hit.zone == Zone::fadeIn || hit.zone == Zone::fadeOut) ? juce::MouseCursor::TopEdgeResizeCursor
                       : hit.zone == Zone::crossfade ? juce::MouseCursor::LeftRightResizeCursor
                       : hit.zone == Zone::gainLine ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::DraggingHandCursor;
                break;
            default: break;
        }
        setMouseCursor (cursor);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (session.edit == nullptr) return;
        auto hit = hitTest (e.position);
        if (hit.track == nullptr) return;

        if (hit.clip != nullptr)
        {
            auto tool = effectiveTool (hit, e.mods);
            if (auto mc = dynamic_cast<te::MidiClip*> (hit.clip.get())) { if (onOpenMidiClip) onOpenMidiClip (*mc); return; }
            if (auto sc = dynamic_cast<te::StepClip*> (hit.clip.get())) { if (onOpenStepClip) onOpenStepClip (*sc); return; }
            if (tool == EditTool::selector)
            {
                // double-click with the Selector selects the whole clip
                state.selectClip (hit.clip);
                state.timeSelection = hit.clip->getPosition().time;
                if (Prefs::linkTimelineAndEdit()) syncLoopToSelection();
                notify();
            }
            else renameClip (hit.clip);
            return;
        }

        if (Session::isInstrumentTrack (*hit.track))   // double-click empty instrument lane: create a 1-bar MIDI clip
            createMidiClipAt (*hit.track, state.xToTime (e.position.x));
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isAltDown())
        {
            if (onZoom) onZoom (w.deltaY > 0 ? 0.85 : 1.18, state.xToTime (e.position.x));
            return;
        }
        if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        {
            auto dx = std::abs (w.deltaX) > 0 ? w.deltaX : w.deltaY;
            state.viewStart = juce::jmax (0.0, state.viewStart - dx * 400.0 * state.secondsPerPixel);
            notify();
            return;
        }
        Component::mouseWheelMove (e, w);   // vertical scroll goes to the viewport
    }

    //==========================================================================
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (session.getEngine().getAudioFileFormatManager().readFormatManager.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr
                || f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        auto time = te::TimePosition::fromSeconds (snapIf (*session.edit, state.xToTime ((float) x), juce::ModifierKeys::currentModifiers));
        auto track = dynamic_cast<te::AudioTrack*> (getTrackAtY (y));

        for (int i = 0; i < files.size(); ++i)
        {
            juce::File f (files[i]);
            if (track == nullptr)
            {
                // Pro Tools makes a mono or stereo track to match the file
                session.nextAudioTrackStereo = ! f.hasFileExtension ("mid;midi") && te::AudioFile (session.getEngine(), f).getInfo().numChannels >= 2;
                track = session.addTrack ("audio", f.getFileNameWithoutExtension());
            }

            if (f.hasFileExtension ("mid;midi"))
            {
                if (auto mc = te::createClipFromFile (f, *track, false))
                    mc->setStart (time, false, true);
                session.log.add ("You", "Imported MIDI " + f.getFileName());
            }
            else
            {
                te::AudioFile af (session.getEngine(), f);
                if (af.isValid())
                {
                    session.prepareTrackForFile (*track, af.getInfo().numChannels);
                    if (auto nc = track->insertWaveClip (f.getFileNameWithoutExtension(), f, { { time, te::TimeDuration::fromSeconds (af.getLength()) }, {} }, false))
                    {
                        if (Session::getElasticMode (*track) != 0) { nc->setTimeStretchMode (Session::elasticToStretchMode (Session::getElasticMode (*track))); nc->setAutoTempo (true); }
                        EditOps::resolveOverlaps (*nc);
                        if (state.editMode == EditMode::spot) spotDialog (nc);
                    }
                }
                session.log.add ("You", "Imported " + f.getFileName() + " onto " + track->getName());
            }
            track = nullptr;
        }
        repaint();
    }

    //==========================================================================
    /** Tab to Transient / Tab to clip boundary. */
    void tabToNext (bool backwards)
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        auto cur = tr.getPosition().inSeconds();
        std::vector<double> points;
        auto tracks = EditOps::targetTracks (session, state);
        if (tracks.isEmpty()) tracks = session.getTracks();
        for (auto t : tracks)
            for (auto c : t->getClips())
            {
                points.push_back (c->getPosition().getStart().inSeconds());
                points.push_back (c->getPosition().getEnd().inSeconds());
                if (state.tabToTransients)
                    if (auto wc = dynamic_cast<te::WaveAudioClip*> (c))
                        for (auto p : transientsFor (*wc)) points.push_back (p);
            }
        std::sort (points.begin(), points.end());
        double target = cur;
        bool found = false;
        if (! backwards) { for (auto p : points) if (p > cur + 0.002) { target = p; found = true; break; } }
        else { for (int i = (int) points.size(); --i >= 0;) if (points[(size_t) i] < cur - 0.002) { target = points[(size_t) i]; found = true; break; } }
        if (found)
        {
            tr.setPosition (te::TimePosition::fromSeconds (target));
            if (juce::ModifierKeys::currentModifiers.isShiftDown() && state.timeSelection)
            {
                auto s = state.timeSelection->getStart().inSeconds(), en = state.timeSelection->getEnd().inSeconds();
                state.timeSelection = te::TimeRange (te::TimePosition::fromSeconds (juce::jmin (s, target)), te::TimePosition::fromSeconds (juce::jmax (en, target)));
            }
            notify();
        }
    }

    const std::vector<double>& transientsFor (te::WaveAudioClip& wc)
    {
        auto& entry = transientCache[wc.itemID];
        auto key = wc.getPosition().getStart().inSeconds() * 1000 + wc.getPosition().getLength().inSeconds() + wc.getPosition().getOffset().inSeconds() * 7;
        if (entry.second != key) { entry.first = Transients::find (session.getEngine(), wc); entry.second = key; }
        return entry.first;
    }

    /** Separate the selected clips at their transients. */
    int separateAtTransients (float sensitivity)
    {
        int n = 0;
        auto clips = state.selectedClips;
        for (auto& c : clips)
            if (auto wc = dynamic_cast<te::WaveAudioClip*> (c.get()))
                if (auto t = dynamic_cast<te::ClipTrack*> (wc->getClipTrack()))
                {
                    auto pts = Transients::find (session.getEngine(), *wc, sensitivity);
                    te::Clip::Ptr cur (wc);
                    for (auto p : pts)
                    {
                        if (p <= cur->getPosition().getStart().inSeconds() + 0.02 || p >= cur->getPosition().getEnd().inSeconds() - 0.02) continue;
                        auto nc = t->splitClip (*cur, te::TimePosition::fromSeconds (p));
                        if (nc) { EditOps::tinyFades (cur.get()); EditOps::tinyFades (nc); cur = nc; ++n; }
                    }
                }
        return n;
    }

    void syncLoopToSelection()
    {
        if (session.edit == nullptr || ! state.timeSelection) return;
        if (state.timeSelection->getLength() > 0.01s)
            session.edit->getTransport().setLoopRange (*state.timeSelection);
    }

private:
    Session& session;
    TimelineState& state;
    std::map<te::EditItemID, std::unique_ptr<te::SmartThumbnail>> thumbnails;
    std::map<te::EditItemID, std::pair<std::vector<double>, double>> transientCache;
    juce::String gainTip;

    static bool isLocked (te::Clip& c)    { return (bool) c.state.getProperty ("sqmLocked", false); }

    enum class DragMode { none, move, copyMove, trimStart, trimEnd, timeSelect, zoomBand, scrub, pencil, fadeIn, fadeOut, clipGain, crossfade };
    struct Drag
    {
        Hit hit;
        EditTool tool = EditTool::smart;
        te::ClipPosition originalPos;
        double mouseStartTime = 0, anchorTime = 0, originalValue = 0;
        float startY = 0, bandStartX = 0, bandEndX = 0;
        te::Track* anchorRow = nullptr;
        DragMode mode = DragMode::none;
        bool copied = false;
        juce::ModifierKeys mods;
        std::vector<std::pair<te::Clip::Ptr, te::TimePosition>> others, followers;
        std::vector<std::pair<te::Clip::Ptr, double>> tiles;      // Shuffle: the other clips on the track, by original start
        std::vector<std::pair<te::Clip::Ptr, int>> rows;          // each dragged clip's original row
        int primaryRow = -1;
        te::Clip::Ptr xfLeft, xfRight;                              // Smart Tool crossfade
        double xfJunction = 0;
        float xfTop = 0, xfBottom = 0;
    } drag;

    /** The clip butting against this one's start (atStart) or end on the same track, or nullptr. */
    static te::Clip* buttingNeighbour (te::AudioTrack& t, te::Clip& c, bool atStart)
    {
        const auto edge = atStart ? c.getPosition().getStart() : c.getPosition().getEnd();
        for (auto o : t.getClips())
        {
            if (o == &c || dynamic_cast<te::AudioClipBase*> (o) == nullptr) continue;
            const auto oe = atStart ? o->getPosition().getEnd() : o->getPosition().getStart();
            if (std::abs ((oe - edge).inSeconds()) < 0.002) return o;
        }
        return nullptr;
    }

    /** Grid snapping for this gesture: Grid mode, or Grid combined with Shuffle / Slip; Cmd swaps Grid and Slip. */
    bool gridActive (const juce::ModifierKeys& m) const
    {
        const bool base = state.editMode == EditMode::grid || state.snap;
        return m.isCommandDown() ? ! base : base;
    }

    double snapIf (te::Edit& edit, double secs, const juce::ModifierKeys& m) const
    {
        return gridActive (m) ? state.snapToGrid (edit, secs) : juce::jmax (0.0, secs);
    }

    void captureRows()
    {
        drag.rows.clear();
        auto rows = getRows();
        for (auto& [cp, start] : drag.others) drag.rows.push_back ({ cp, rows.indexOf (cp->getClipTrack()) });
        drag.primaryRow = drag.hit.clip != nullptr ? rows.indexOf (drag.hit.clip->getClipTrack()) : -1;
    }

    void moveRows (float y)
    {
        auto rows = getRows();
        auto target = getTrackAtY ((int) y);
        if (target == nullptr || drag.primaryRow < 0) return;
        const int deltaRows = rows.indexOf (target) - drag.primaryRow;
        // only when every clip has an audio / instrument track to land on
        for (auto& [cp, row] : drag.rows)
        {
            const int r = row + deltaRows;
            if (r < 0 || r >= rows.size() || dynamic_cast<te::AudioTrack*> (rows[r]) == nullptr) return;
        }
        for (auto& [cp, row] : drag.rows)
            if (auto t = dynamic_cast<te::AudioTrack*> (rows[row + deltaRows]); t != nullptr && t != cp->getClipTrack())
                cp->moveTo (*t);
    }

    /** Shuffle: the clips on the track are tiles. Taking the dragged clip out closes its gap; dropping it on a
        clip boundary pushes everything after that point right by its length. Gaps between other clips are kept. */
    void shuffleDrag (te::Clip& primary, double wanted)
    {
        const double s0 = drag.originalPos.getStart().inSeconds(), L = drag.originalPos.getLength().inSeconds();
        constexpr double eps = 0.0005;
        std::vector<double> closed;
        closed.reserve (drag.tiles.size());
        for (auto& [c, st] : drag.tiles) closed.push_back (st >= s0 + L - eps ? st - L : st);
        std::vector<double> candidates { s0 };
        for (size_t i = 0; i < drag.tiles.size(); ++i)
        {
            candidates.push_back (closed[i]);
            candidates.push_back (closed[i] + drag.tiles[i].first->getPosition().getLength().inSeconds());
        }
        double p = candidates.front();
        for (auto cnd : candidates) if (cnd >= 0 && std::abs (cnd - wanted) < std::abs (p - wanted)) p = cnd;
        primary.setStart (te::TimePosition::fromSeconds (p), false, true);
        for (size_t i = 0; i < drag.tiles.size(); ++i)
            drag.tiles[i].first->setStart (te::TimePosition::fromSeconds (closed[i] + (closed[i] >= p - eps ? L : 0.0)), false, true);
    }

    void timerCallback() override
    {
        if (session.edit != nullptr && session.edit->getTransport().isRecording()) repaint();
    }

    void notify()   { if (onSelectionChanged) onSelectionChanged(); repaint(); }

    te::Clip::Ptr clipAt (te::AudioTrack& t, juce::Point<float> p)
    {
        auto clips = t.getClips();
        for (int i = clips.size(); --i >= 0;)
        {
            auto pos = clips[i]->getPosition();
            if (p.x >= state.timeToX (pos.getStart().inSeconds()) && p.x <= state.timeToX (pos.getEnd().inSeconds()))
                return clips[i];
        }
        return {};
    }

    /** Shuffle mode: snap a dragged clip's start to the nearest clip boundary on its track. */
    double shuffleTarget (te::Clip& c, double wanted)
    {
        auto t = dynamic_cast<te::AudioTrack*> (c.getClipTrack());
        if (t == nullptr) return wanted;
        double best = 0.0, bestDist = std::abs (wanted);
        for (auto o : t->getClips())
        {
            if (o == &c || state.isSelected (o)) continue;
            for (double cand : { o->getPosition().getEnd().inSeconds(), o->getPosition().getStart().inSeconds() - c.getPosition().getLength().inSeconds() })
                if (cand >= 0 && std::abs (wanted - cand) < bestDist) { best = cand; bestDist = std::abs (wanted - cand); }
        }
        return best;
    }

    float gainLineY (te::AudioClipBase& ac, juce::Rectangle<float> r)
    {
        auto body = r.withTrimmedTop (16.0f);
        auto norm = juce::jmap ((float) juce::jlimit (-24.0f, 12.0f, ac.getGainDB()), -24.0f, 12.0f, 1.0f, 0.0f);
        return body.getY() + norm * body.getHeight();
    }

    /** The fader a row's volume automation belongs to: a track's, a folder's, or the Master Fader's. */
    te::VolumeAndPanPlugin* volumeFor (te::Track* row)
    {
        if (row == nullptr || session.edit == nullptr) return nullptr;
        if (auto t = dynamic_cast<te::AudioTrack*> (row)) return t->getVolumePlugin();
        if (row->isMasterTrack()) return session.edit->getMasterVolumePlugin().get();
        if (auto f = dynamic_cast<te::FolderTrack*> (row)) return f->getVolumePlugin();
        return nullptr;
    }

    void pencilAt (juce::Point<float> p)
    {
        auto row = getTrackAtY ((int) p.y);
        if (row == nullptr || session.edit == nullptr) return;
        auto param = MixCtl::viewParam (session, *row);   // the Track View's parameter (volume in waveform view)
        if (param == nullptr) return;
        int y = getTrackY (row), h = state.heightFor (row);
        auto norm = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - (float) y - 18.0f) / (float) juce::jmax (1, h - 24));
        auto rng = param->getValueRange();
        auto value = rng.getStart() + norm * rng.getLength();
        auto t = te::TimePosition::fromSeconds (juce::jmax (0.0, state.xToTime (p.x)));
        auto& curve = param->getCurve();
        // replace any point within 2 px of this time
        auto tol = te::TimeDuration::fromSeconds (state.secondsPerPixel * 2.0);
        auto um = &session.edit->getUndoManager();
        for (int i = curve.getNumPoints(); --i >= 0;)
            if (std::abs ((curve.getPointTime (i) - t).inSeconds()) < tol.inSeconds()) curve.removePoint (i, um);
        curve.addPoint (t, value, 0.0f, um);
        param->updateStream();
    }

    /** Grid lines that follow the real tempo and meter map: bars (strong), beats (medium) and the grid value's
        subdivisions (faint). Time-based grid values (seconds, ms, samples) draw their own lines over faint bars. */
    void drawGrid (juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto& edit = *session.edit;
        auto& ts = edit.tempoSequence;
        const bool lines = Prefs::getBool ("ed_gridLines", true);
        const double t0 = juce::jmax (0.0, state.xToTime ((float) area.getX())), t1 = state.xToTime ((float) area.getRight());
        const auto top = (float) area.getY(), bottom = (float) area.getBottom();
        auto vline = [&] (double secs, juce::Colour c)
        {
            g.setColour (c);
            g.drawVerticalLine (juce::roundToInt (state.timeToX (secs)), top, bottom);
        };
        const auto barC = Theme::gridColour().brighter (0.25f), beatC = Theme::gridColour().withAlpha (0.55f), subC = Theme::gridColour().withAlpha (0.28f);
        auto& gv = gridValues[juce::jlimit (0, (int) std::size (gridValues) - 1, state.gridIndex)];

        // bars and beats (always drawn; faint when a time-based grid is chosen)
        const bool timeGrid = gv.beats < -1.0;
        auto bb = ts.toBarsAndBeats (te::TimePosition::fromSeconds (t0));
        int bar = juce::jmax (0, bb.bars);
        const double gBeats = timeGrid ? 0.0 : state.gridBeats (edit);
        for (int guard = 0; guard < 20000; ++guard)
        {
            const auto barStart = ts.toTime (te::tempo::BarsAndBeats { bar, te::BeatDuration(), 0 });
            if (barStart.inSeconds() > t1) break;
            const auto barEnd = ts.toTime (te::tempo::BarsAndBeats { bar + 1, te::BeatDuration(), 0 });
            const double pxBar = (barEnd - barStart).inSeconds() / state.secondsPerPixel;
            const int every = pxBar >= 5 ? 1 : pxBar >= 1.25 ? 4 : 16;
            if (bar % every == 0) vline (barStart.inSeconds(), timeGrid ? barC.withMultipliedAlpha (0.5f) : barC);
            if (lines && ! timeGrid)
            {
                const int num = juce::jmax (1, ts.getTimeSigAt (barStart).numerator.get());
                const double b0 = ts.toBeats (barStart).inBeats();
                if (pxBar / num >= 8)
                    for (int beat = 1; beat < num; ++beat) vline (ts.toTime (te::BeatPosition::fromBeats (b0 + beat)).inSeconds(), beatC);
                if (gBeats > 0 && gBeats < 1.0 && pxBar / num * gBeats >= 6)
                    for (double sb = gBeats; sb < num - 1e-9; sb += gBeats)
                        if (std::abs (sb - std::round (sb)) > 1e-6) vline (ts.toTime (te::BeatPosition::fromBeats (b0 + sb)).inSeconds(), subC);
            }
            ++bar;
        }

        // time-based grid values: 1 s / 100 ms / 10 ms / 1 ms / 1 sample lines at a readable density
        if (lines && timeGrid)
        {
            double step = state.gridSeconds (edit, t0);
            if (step <= 0) return;
            while (step / state.secondsPerPixel < 6) step *= 10.0;
            for (double t = std::floor (t0 / step) * step; t <= t1; t += step)
            {
                const bool major = std::abs (std::fmod (t / step, 10.0)) < 1e-6;
                vline (t, major ? beatC : subC);
            }
        }
    }

    juce::Colour colourFor (te::Clip& c, te::Track& t)
    {
        if (c.state.hasProperty (te::IDs::colour) && ! c.getColour().isTransparent() && c.getColour() != juce::Colour (0xff000000))
            return c.getColour();
        return t.getColour();
    }

    void drawClip (juce::Graphics& g, te::Clip& c, te::AudioTrack& t, int y, int h)
    {
        auto pos = c.getPosition();
        auto x1 = state.timeToX (pos.getStart().inSeconds()), x2 = state.timeToX (pos.getEnd().inSeconds());
        if (x2 < 0 || x1 > getWidth()) return;

        juce::Rectangle<float> r (x1, (float) y + 2, juce::jmax (3.0f, x2 - x1), (float) h - 5);
        auto colour = colourFor (c, t);
        bool selected = state.isSelected (&c);
        bool muted = c.isMuted() || t.isMuted (true);

        g.setColour (colour.withAlpha (muted ? 0.1f : 0.22f));
        g.fillRoundedRectangle (r, 4.0f);
        if (h > 30)
        {
            g.setColour (colour.withAlpha (muted ? 0.4f : 0.85f));
            g.fillRoundedRectangle (r.withHeight (15.0f), 4.0f);
            if (Prefs::clipNamesVisible())
            {
                g.setColour (Theme::bg());
                g.setFont (Theme::font (11.0f, true));
                juce::String label = (isLocked (c) ? "LOCKED  " : "") + c.getName();
                if (auto ac = dynamic_cast<te::AudioClipBase*> (&c))
                {
                    if (std::abs (ac->getGainDB()) > 0.05f) label << "  " << (ac->getGainDB() > 0 ? "+" : "") << juce::String (ac->getGainDB(), 1) << " dB";
                    if (ac->getAutoTempo()) label << "  ~";
                    if (ac->hasAnyTakes()) label << "  [" << (ac->getCurrentTake() + 1) << "/" << ac->getTakes().size() << "]";
                    if (ac->getClipEffects() != nullptr && ac->state.getChildWithName (te::IDs::EFFECTS).getNumChildren() > 0) label << "  fx";
                }
                g.drawText (label, r.withHeight (15.0f).reduced (5, 0).toNearestInt(), juce::Justification::centredLeft, true);
            }
        }

        auto body = r.withTrimmedTop (h > 30 ? 16.0f : 1.0f).reduced (1.0f, 2.0f);
        g.saveState();
        g.reduceClipRegion (body.toNearestInt());

        if (auto wc = dynamic_cast<te::WaveAudioClip*> (&c))
            drawWave (g, *wc, body, Theme::waveformFor (colour).withAlpha (muted ? 0.3f : 1.0f));
        else if (auto mc = dynamic_cast<te::MidiClip*> (&c))
            drawMidi (g, *mc, body, colour);
        else if (auto sc = dynamic_cast<te::StepClip*> (&c))
            drawSteps (g, *sc, body, colour);

        g.restoreState();

        if (auto ac = dynamic_cast<te::AudioClipBase*> (&c))
        {
            // fades
            g.setColour (Theme::text().withAlpha (0.55f));
            auto fi = (float) (ac->getFadeIn().inSeconds() / state.secondsPerPixel);
            auto fo = (float) (ac->getFadeOut().inSeconds() / state.secondsPerPixel);
            auto topY = h > 30 ? r.getY() + 16 : r.getY();
            if (fi > 2)
            {
                juce::Path p; p.startNewSubPath (r.getX(), r.getBottom()); p.quadraticTo (r.getX() + fi * 0.5f, topY + (r.getBottom() - topY) * 0.5f, r.getX() + fi, topY);
                g.strokePath (p, juce::PathStrokeType (1.2f));
                g.setColour (Theme::bg().withAlpha (0.35f)); p.lineTo (r.getX(), topY); p.closeSubPath(); g.fillPath (p);
                g.setColour (Theme::text().withAlpha (0.55f));
            }
            if (fo > 2)
            {
                juce::Path p; p.startNewSubPath (r.getRight() - fo, topY); p.quadraticTo (r.getRight() - fo * 0.5f, topY + (r.getBottom() - topY) * 0.5f, r.getRight(), r.getBottom());
                g.strokePath (p, juce::PathStrokeType (1.2f));
                g.setColour (Theme::bg().withAlpha (0.35f)); p.lineTo (r.getRight(), topY); p.closeSubPath(); g.fillPath (p);
            }
            // fade handles (Smart tool corners)
            if (h > 36 && r.getWidth() > 30)
            {
                g.setColour (Theme::text().withAlpha (selected ? 0.9f : 0.35f));
                g.drawRect (juce::Rectangle<float> (r.getX() + 2, topY + 1, 6, 6), 1.0f);
                g.drawRect (juce::Rectangle<float> (r.getRight() - 8, topY + 1, 6, 6), 1.0f);
            }
            // clip gain line
            if (Prefs::clipGainLineVisible() && h > 40)
            {
                auto gy = gainLineY (*ac, r);
                g.setColour (Theme::amber().withAlpha (selected ? 0.85f : 0.45f));
                g.drawLine (r.getX() + 1, gy, r.getRight() - 1, gy, 1.0f);
            }
        }

        g.setColour (selected ? Theme::acid() : colour.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 4.0f, selected ? 2.0f : 1.0f);
    }

    void drawWave (juce::Graphics& g, te::WaveAudioClip& c, juce::Rectangle<float> area, juce::Colour colour)
    {
        auto& thumb = thumbnails[c.itemID];
        te::AudioFile file (c.getPlaybackFile());
        if (thumb == nullptr || thumb->file != file)
            thumb = std::make_unique<te::SmartThumbnail> (c.edit.engine, file, *this, &c.edit);

        if (thumb->isGeneratingProxy() || thumb->isOutOfDate())
        {
            g.setColour (Theme::dim());
            g.drawText ("drawing...", area.toNearestInt(), juce::Justification::centred);
            return;
        }

        auto pos = c.getPosition();
        auto visible = area.getIntersection (getLocalBounds().toFloat());
        auto t1 = state.xToTime (visible.getX()) - pos.getStart().inSeconds() + pos.getOffset().inSeconds();
        auto t2 = state.xToTime (visible.getRight()) - pos.getStart().inSeconds() + pos.getOffset().inSeconds();
        auto speed = c.getSpeedRatio();
        auto gain = juce::Decibels::decibelsToGain (c.getGainDB());

        g.setColour (colour.withAlpha (0.9f));
        auto ir = visible.toNearestInt();

        // View > Waveform View: Rectified and Power (log) are drawn column by column from the thumbnail's peaks
        if (const int view = Prefs::getInt ("ed_waveView", 0); view == 1 || view == 2)
        {
            const int chans = juce::jmin (2, thumb->getNumChannels());
            const bool split = chans > 1 && ir.getHeight() > 40;
            auto drawOne = [&] (juce::Rectangle<int> lane, int ch)
            {
                const double secsPerPx = state.secondsPerPixel * speed;
                const float h = (float) lane.getHeight(), bottom = (float) lane.getBottom(), mid = (float) lane.getCentreY();
                double t = t1 * speed;
                for (int x = lane.getX(); x < lane.getRight(); ++x, t += secsPerPx)
                {
                    float mn = 0, mx = 0;
                    thumb->getApproximateMinMax (juce::jmax (0.0, t), juce::jmax (0.0, t + secsPerPx), ch, mn, mx);
                    float a = juce::jmax (std::abs (mn), std::abs (mx)) * (float) gain;
                    if (view == 2) a = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (a, -60.0f) + 60.0f) / 60.0f);   // 60 dB log scale
                    a = juce::jmin (1.0f, a);
                    if (view == 1) g.drawVerticalLine (x, bottom - a * (h - 1.0f), bottom);
                    else           g.drawVerticalLine (x, mid - a * h * 0.5f, mid + a * h * 0.5f);
                }
            };
            if (split) { auto top = ir.removeFromTop (ir.getHeight() / 2); drawOne (top, 0); drawOne (ir, 1); }
            else drawOne (ir, 0);
        }
        else if (thumb->getNumChannels() > 1 && ir.getHeight() > 40)
        {
            auto top = ir.removeFromTop (ir.getHeight() / 2);
            thumb->drawChannel (g, top, { te::TimePosition::fromSeconds (t1 * speed), te::TimePosition::fromSeconds (t2 * speed) }, 0, gain);
            thumb->drawChannel (g, ir, { te::TimePosition::fromSeconds (t1 * speed), te::TimePosition::fromSeconds (t2 * speed) }, 1, gain);
        }
        else
            thumb->drawChannel (g, ir, { te::TimePosition::fromSeconds (t1 * speed), te::TimePosition::fromSeconds (t2 * speed) }, 0, gain);

        if (state.tabToTransients && state.isSelected (&c))
        {
            g.setColour (Theme::magenta().withAlpha (0.6f));
            for (auto p : transientsFor (c))
            {
                auto x = state.timeToX (p);
                if (x >= area.getX() && x <= area.getRight()) g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
            }
        }
    }

    void drawMidi (juce::Graphics& g, te::MidiClip& mc, juce::Rectangle<float> area, juce::Colour colour)
    {
        auto& seq = mc.getSequence();
        if (seq.getNumNotes() == 0)
        {
            g.setColour (Theme::dim());
            g.setFont (Theme::font (11.0f));
            g.drawText ("double-click to edit", area.toNearestInt(), juce::Justification::centred);
            return;
        }

        int lo = 127, hi = 0;
        for (auto n : seq.getNotes()) { lo = juce::jmin (lo, n->getNoteNumber()); hi = juce::jmax (hi, n->getNoteNumber()); }
        lo -= 2; hi += 2;
        auto noteH = juce::jmax (2.0f, area.getHeight() / (float) (hi - lo + 1));

        for (auto n : seq.getNotes())
        {
            auto s = mc.getTimeOfContentBeat (n->getStartBeat()).inSeconds();
            auto e = mc.getTimeOfContentBeat (n->getEndBeat()).inSeconds();
            auto y = area.getBottom() - (float) (n->getNoteNumber() - lo + 1) * noteH;
            g.setColour (colour.brighter (0.4f).withAlpha (0.4f + 0.6f * n->getVelocity() / 127.0f));
            g.fillRect (juce::Rectangle<float> (state.timeToX (s), y, juce::jmax (2.0f, state.timeToX (e) - state.timeToX (s) - 1), juce::jmax (1.5f, noteH - 1)));
        }
    }

    void drawSteps (juce::Graphics& g, te::StepClip& sc, juce::Rectangle<float> area, juce::Colour colour)
    {
        auto p = sc.getPattern (0);
        auto chans = sc.getChannels();
        int n = juce::jmax (1, p.getNumNotes());
        auto stepSecs = sc.edit.tempoSequence.toTime (te::BeatPosition::fromBeats (p.getNoteLength().inBeats())).inSeconds();
        auto loopSecs = stepSecs * n;
        if (loopSecs <= 0) return;
        auto rowH = area.getHeight() / (float) juce::jmax (1, chans.size());
        auto pos = sc.getPosition();
        g.setColour (colour.brighter (0.3f));
        for (double t0 = pos.getStart().inSeconds(); t0 < pos.getEnd().inSeconds(); t0 += loopSecs)
            for (int ch = 0; ch < chans.size(); ++ch)
                for (int i = 0; i < n; ++i)
                    if (p.getNote (ch, i))
                    {
                        auto x1 = state.timeToX (t0 + i * stepSecs), x2 = state.timeToX (t0 + (i + 1) * stepSecs);
                        if (x2 > area.getRight() || x1 < area.getX() - 50) continue;
                        g.fillRect (juce::Rectangle<float> (x1 + 0.5f, area.getBottom() - (ch + 1) * rowH + 1, juce::jmax (1.0f, x2 - x1 - 1), juce::jmax (1.0f, rowH - 2)));
                    }
    }

    void drawFolderSummary (juce::Graphics& g, te::FolderTrack& f, int y, int h)
    {
        g.setColour (f.getColour().withAlpha (0.08f));
        g.fillRect (0, y, getWidth(), h - 1);
        g.setColour (f.getColour().withAlpha (0.5f));
        for (auto t : f.getAllSubTracks (true))
            if (auto at = dynamic_cast<te::AudioTrack*> (t))
                for (auto c : at->getClips())
                {
                    auto x1 = state.timeToX (c->getPosition().getStart().inSeconds()), x2 = state.timeToX (c->getPosition().getEnd().inSeconds());
                    if (x2 < 0 || x1 > getWidth()) continue;
                    g.fillRoundedRectangle (x1, (float) y + 16, juce::jmax (2.0f, x2 - x1), (float) h - 22, 2.0f);
                }
        g.setColour (Theme::bg().withAlpha (0.55f));
        g.fillRect (0, y, 130, 14);
        g.setColour (Theme::text().withAlpha (0.8f));
        g.setFont (Theme::font (10.0f, true));
        g.drawText (f.isSubmixFolder() ? "FOLDER  (routing)" : "VCA  (control)", 6, y + 1, 130, 12, juce::Justification::left);
    }

    void drawVolumeAutomation (juce::Graphics& g, te::Track& t, int y, int h)
    {
        bool automationView = false;
        auto param = MixCtl::viewParam (session, t, &automationView);
        if (param == nullptr || (! automationView && ! param->hasAutomationPoints())) return;
        if (automationView)
        {
            // Pro Tools automation view: the clips step back, the graph and its value come forward
            g.setColour (Theme::bg().withAlpha (0.55f));
            g.fillRect (0, y, getWidth(), h - 1);
            g.setColour (Theme::acid());
            g.setFont (Theme::font (11.0f, true));
            if (h > 26) g.drawText (MixCtl::viewLabel (session, t) + "   " + param->getCurrentValueAsString() + "   -  draw with the Pencil (F10)",
                                    8, y + 2, juce::jmin (getWidth() - 16, 620), 14, juce::Justification::centredLeft, true);
        }
        auto& curve = param->getCurve();
        juce::Path p;
        auto rng = param->getValueRange();
        auto valToY = [&] (float v) { return (float) y + 18 + (1.0f - v) * (float) (h - 24); };
        for (int x = 0; x < getWidth(); x += 3)
        {
            auto v = curve.getValueAt (te::TimePosition::fromSeconds (state.xToTime ((float) x)), param->getCurrentValue());
            auto yy = valToY ((v - rng.getStart()) / juce::jmax (0.0001f, rng.getLength()));
            if (x == 0) p.startNewSubPath ((float) x, yy); else p.lineTo ((float) x, yy);
        }
        g.setColour (Theme::acid().withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (1.5f));
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            auto px = state.timeToX (curve.getPointTime (i).inSeconds());
            auto py = valToY ((curve.getPointValue (i) - rng.getStart()) / juce::jmax (0.0001f, rng.getLength()));
            g.fillEllipse (px - 3, py - 3, 6, 6);
        }
    }

    //==========================================================================
    void createMidiClipAt (te::AudioTrack& t, double secs)
    {
        auto& ts = session.edit->tempoSequence;
        auto num = (double) ts.getTimeSig (0)->numerator.get();
        auto barBeat = std::floor (ts.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, secs))).inBeats() / num) * num;
        auto start = ts.toTime (te::BeatPosition::fromBeats (barBeat));
        auto end = ts.toTime (te::BeatPosition::fromBeats (barBeat + num));
        if (auto mc = t.insertMIDIClip ("MIDI", { start, end }, nullptr))
            if (onOpenMidiClip) onOpenMidiClip (*mc);
    }

public:
    void renameClip (te::Clip::Ptr c)
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Name", "Clip name:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", c->getName());
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, c] (int r) { aw->setVisible (false); if (r == 1) c->setName (aw->getTextEditorContents ("name")); }));
    }

    /** Pro Tools Spot dialog: Start / End / Duration in any time format, plus the file's original (BWF) time stamp.
        which: 0 = spot the clip (move), 1 = trim its start, 2 = trim its end. */
    void spotDialog (te::Clip::Ptr c, int which = 0)
    {
        if (session.edit == nullptr || c == nullptr) return;
        auto& edit = *session.edit;
        const auto fmt = (TimeFmt::Kind) Prefs::getInt ("ed_mainCounterFmt", 0);
        auto pos = c->getPosition();
        juce::String title = which == 0 ? "Spot Dialog" : which == 1 ? "Spot Dialog - Trim Start" : "Spot Dialog - Trim End";
        auto aw = std::make_shared<juce::AlertWindow> (title, "\"" + c->getName() + "\"  -  type a time in " + TimeFmt::kindNames()[(int) fmt]
                                                        + " (any format works: 5|3|240, 1:02.5, 01:00:02:10, 12.5s, 48000smp)", juce::MessageBoxIconType::NoIcon);
        aw->addComboBox ("fmt", TimeFmt::kindNames(), "Time Scale");
        aw->getComboBoxComponent ("fmt")->setSelectedItemIndex ((int) fmt, juce::dontSendNotification);
        aw->addTextEditor ("start", TimeFmt::format (edit, pos.getStart(), fmt), "Start");
        aw->addTextEditor ("end", TimeFmt::format (edit, pos.getEnd(), fmt), "End");
        aw->addTextEditor ("dur", TimeFmt::formatLength (edit, pos.getStart(), pos.getEnd(), fmt), "Duration");
        double originalStamp = -1.0;
        if (auto wc = dynamic_cast<te::WaveAudioClip*> (c.get()))
        {
            auto info = te::AudioFile (wc->getPlaybackFile()).getInfo();
            auto ref = info.metadata.getValue (juce::WavAudioFormat::bwavTimeReference, {});
            if (ref.isNotEmpty() && info.sampleRate > 0)
                originalStamp = ref.getLargeIntValue() / info.sampleRate - TimeFmt::tcStartSeconds (edit) - pos.getOffset().inSeconds();
        }
        aw->addTextBlock (originalStamp >= 0 ? "Original Time Stamp: " + TimeFmt::format (edit, te::TimePosition::fromSeconds (juce::jmax (0.0, originalStamp)), fmt)
                                             : "Original Time Stamp: none in this file");
        const double userStamp = c->state.hasProperty ("sqmUserStamp") ? (double) c->state["sqmUserStamp"] + pos.getOffset().inSeconds() : -1.0;
        if (userStamp >= 0) aw->addTextBlock ("User Time Stamp: " + TimeFmt::format (edit, te::TimePosition::fromSeconds (userStamp), fmt));
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        if (originalStamp >= 0 && which == 0) aw->addButton ("Use Original Time Stamp", 3);
        if (userStamp >= 0 && which == 0) aw->addButton ("Use User Time Stamp", 4);
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        // re-display the fields when the time scale changes
        auto* combo = aw->getComboBoxComponent ("fmt");
        juce::Component::SafePointer<juce::AlertWindow> safe (aw.get());
        combo->onChange = [this, safe, c, combo]
        {
            if (safe == nullptr || session.edit == nullptr) return;
            auto k = (TimeFmt::Kind) combo->getSelectedItemIndex();
            Prefs::set ("ed_mainCounterFmt", (int) k);
            auto p = c->getPosition();
            safe->getTextEditor ("start")->setText (TimeFmt::format (*session.edit, p.getStart(), k));
            safe->getTextEditor ("end")->setText (TimeFmt::format (*session.edit, p.getEnd(), k));
            safe->getTextEditor ("dur")->setText (TimeFmt::formatLength (*session.edit, p.getStart(), p.getEnd(), k));
        };
        const juce::String s0 = aw->getTextEditorContents ("start"), e0 = aw->getTextEditorContents ("end"), d0 = aw->getTextEditorContents ("dur");
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, c, which, s0, e0, d0, originalStamp, userStamp] (int r)
        {
            aw->setVisible (false);
            if (r == 0 || session.edit == nullptr || c->getClipTrack() == nullptr) return;
            auto& ed = *session.edit;
            auto k = (TimeFmt::Kind) aw->getComboBoxComponent ("fmt")->getSelectedItemIndex();
            auto p = c->getPosition();
            if (r == 3) { c->setStart (te::TimePosition::fromSeconds (juce::jmax (0.0, originalStamp)), false, true); EditOps::resolveOverlaps (*c); notify(); return; }
            if (r == 4) { c->setStart (te::TimePosition::fromSeconds (juce::jmax (0.0, userStamp)), false, true); EditOps::resolveOverlaps (*c); notify(); return; }
            auto st = aw->getTextEditorContents ("start"), en = aw->getTextEditorContents ("end"), du = aw->getTextEditorContents ("dur");
            if (which == 1)
            {
                if (auto t = TimeFmt::parse (ed, st, k); t && *t < p.getEnd())
                {
                    auto shift = *t - p.getStart();
                    c->setPosition ({ { *t, p.getEnd() }, juce::jmax (te::TimeDuration(), p.getOffset() + shift) });
                }
            }
            else if (which == 2)
            {
                if (auto t = TimeFmt::parse (ed, en, k); t && *t > p.getStart()) c->setEnd (*t, true);
            }
            else if (st != s0) { if (auto t = TimeFmt::parse (ed, st, k)) EditOps::spot (*c, *t, 0); }
            else if (en != e0) { if (auto t = TimeFmt::parse (ed, en, k)) EditOps::spot (*c, *t, 1); }
            else if (du != d0)
            {
                // duration typed: Bars|Beats as bars|beats|ticks length, others as a time
                if (k == TimeFmt::Kind::barsBeats)
                {
                    auto parts = juce::StringArray::fromTokens (du, "|", "");
                    auto& ts = ed.tempoSequence;
                    const int num = juce::jmax (1, ts.getTimeSigAt (p.getStart()).numerator.get());
                    const double beats = parts[0].getDoubleValue() * num + parts[1].getDoubleValue() + parts[2].getDoubleValue() / 960.0;
                    if (beats > 0) c->setEnd (ts.toTime (ts.toBeats (p.getStart()) + te::BeatDuration::fromBeats (beats)), true);
                }
                else if (auto t = TimeFmt::parse (ed, du, k == TimeFmt::Kind::timecode ? k : TimeFmt::Kind::minSecs)) c->setEnd (p.getStart() + te::TimeDuration::fromSeconds (t->inSeconds() + (k == TimeFmt::Kind::timecode ? TimeFmt::tcStartSeconds (ed) : 0.0)), true);
            }
            EditOps::resolveOverlaps (*c);
            notify();
        }));
    }

    void showLaneMenu (te::AudioTrack& t, const juce::MouseEvent& e)
    {
        juce::PopupMenu m;
        if (Session::isInstrumentTrack (t)) m.addItem (1, "New MIDI clip here");
        m.addItem (2, "Paste here", te::Clipboard::getInstance()->hasContentWithType<te::Clipboard::Clips>());
        m.addItem (3, "Insert silence (1 bar) here");
        m.addItem (4, "Add memory location here");
        auto secs = state.xToTime (e.position.x);
        m.showMenuAsync ({}, [this, &t, secs] (int r)
        {
            if (r == 1) createMidiClipAt (t, secs);
            else if (r == 2) { session.edit->getTransport().setPosition (te::TimePosition::fromSeconds (state.snapIfNeeded (*session.edit, secs))); state.selectTrack (&t); state.timeSelection.reset(); EditOps::paste (session, state); }
            else if (r == 3)
            {
                auto s = te::TimePosition::fromSeconds (state.snapIfNeeded (*session.edit, secs));
                auto bar = session.barToTime (std::floor (session.timeToBar (s)) + 1) - session.barToTime (std::floor (session.timeToBar (s)));
                te::insertSpaceIntoEdit (*session.edit, { s, s + bar });
            }
            else if (r == 4) session.addMarker (te::TimePosition::fromSeconds (juce::jmax (0.0, secs)), "Marker " + juce::String (session.getMarkers().size() + 1));
            notify();
        });
    }

    void showClipMenu (te::Clip& c, te::AudioTrack& t, const juce::MouseEvent& e)
    {
        juce::PopupMenu m, gain, colours, audioSuite, takes, elastic;
        auto at = te::TimePosition::fromSeconds (state.snapIfNeeded (*session.edit, state.xToTime (e.position.x)));
        bool many = state.selectedClips.size() > 1;
        auto ac = dynamic_cast<te::AudioClipBase*> (&c);

        m.addItem (1, "Separate here  (" SQ_CMD "+E at playhead)");
        m.addItem (2, "Duplicate  (" SQ_CMD "+D)");
        m.addItem (12, "Repeat...  (" SQ_OPT "+R)");
        m.addItem (13, "Cut  (" SQ_CMD "+X)"); m.addItem (14, "Copy  (" SQ_CMD "+C)");
        m.addItem (3, state.editMode == EditMode::shuffle ? "Delete (closes gap)" : "Delete");
        m.addItem (15, "Rename...");
        m.addItem (16, c.isMuted() ? "Unmute clip" : "Mute clip  (" SQ_CMD "+M)");
        m.addItem (17, "Lock position", true, c.state.getProperty ("sqmLocked", false));
        m.addItem (18, "Spot...");
        m.addItem (19, "Time Stamp Selected (set User Time Stamp)");
        m.addSeparator();

        if (dynamic_cast<te::MidiClip*> (&c) != nullptr) m.addItem (10, "Open MIDI editor");
        if (dynamic_cast<te::StepClip*> (&c) != nullptr) m.addItem (11, "Open Beat Maker");

        if (ac != nullptr)
        {
            gain.addItem (20, "+1 dB"); gain.addItem (21, "-1 dB"); gain.addItem (4, "+3 dB"); gain.addItem (5, "-3 dB");
            gain.addItem (22, "Reset to 0 dB"); gain.addItem (23, "Set clip gain...");
            gain.addItem (24, "Normalise to -1 dBFS (clip gain)");
            m.addSubMenu ("Clip gain  (" + juce::String (ac->getGainDB(), 1) + " dB)", gain);
            m.addItem (6, "Reverse", true, ac->getIsReversed());
            m.addItem (7, "Fades: in " + juce::String (Prefs::fadeCommandLength(), 2) + " s / out " + juce::String (Prefs::fadeCommandLength(), 2) + " s  (" SQ_CMD "+F)");
            m.addItem (25, "Remove fades");
            m.addItem (26, "Strip Silence...  (" SQ_CMD "+U)");
            m.addItem (27, "Separate at transients");
            m.addItem (28, "Consolidate  (" SQ_OPT "+Shift+3)");
            m.addItem (24, "Open in Cut Lab (audio editor)");
            m.addItem (29, "Heal separation  (" SQ_CMD "+H)", many);

            elastic.addItem (40, "Conform to session tempo (detect & stretch)");
            elastic.addItem (41, ac->getAutoTempo() ? "Stop following tempo" : "Follow session tempo (auto-tempo)");
            elastic.addItem (42, "Half speed"); elastic.addItem (43, "Double speed"); elastic.addItem (44, "Speed / pitch...");
            m.addSubMenu ("Elastic Audio", elastic);

            audioSuite.addItem (50, "Normalise (render)"); audioSuite.addItem (51, "Reverse (render)"); audioSuite.addItem (52, "Invert phase (render)");
            audioSuite.addItem (53, "Make mono (render)"); audioSuite.addItem (54, "Pitch shift... (render)");
            audioSuite.addSeparator();
            audioSuite.addSubMenu ("Plugin (render through)...", PluginMenu::build (session.getEngine(), PluginMenu::effects));
            audioSuite.addSeparator();
            audioSuite.addItem (55, "Remove all rendered effects", ac->getClipEffects() != nullptr && ac->state.getChildWithName (te::IDs::EFFECTS).getNumChildren() > 0);
            m.addSubMenu ("AudioSuite (rendered effects)", audioSuite);

            if (ac->hasAnyTakes())
            {
                auto tk = ac->getTakes();
                for (int i = 0; i < tk.size(); ++i) takes.addItem (60 + i, "Take " + juce::String (i + 1), true, ac->getCurrentTake() == i);
                takes.addSeparator();
                takes.addItem (90, "Expand takes to new tracks");
                takes.addItem (91, "Expand takes to lanes (comp)");
                m.addSubMenu ("Playlists / takes", takes);
            }
        }

        auto& pal = Theme::trackPalette();
        for (int i = 0; i < pal.size(); ++i) colours.addColouredItem (100 + i, "Colour " + juce::String (i + 1), pal[i]);
        colours.addItem (199, "Use track colour");
        m.addSubMenu ("Clip colour", colours);

        te::Clip::Ptr cp (&c);
        m.showMenuAsync ({}, [this, cp, &t, at] (int r)
        {
            if (r == 0) return;
            auto ac = dynamic_cast<te::AudioClipBase*> (cp.get());
            if (r == 1) { auto nc = t.splitClip (*cp, at); EditOps::tinyFades (cp.get()); EditOps::tinyFades (nc); }
            else if (r == 2) EditOps::duplicate (session, state);
            else if (r == 12) repeatDialog();
            else if (r == 13) EditOps::cut (session, state);
            else if (r == 14) EditOps::copy (session, state);
            else if (r == 3) EditOps::deleteSelection (session, state);
            else if (r == 15) renameClip (cp);
            else if (r == 16) for (auto& c2 : state.selectedClips) c2->setMuted (! c2->isMuted());
            else if (r == 17) cp->state.setProperty ("sqmLocked", ! (bool) cp->state.getProperty ("sqmLocked", false), nullptr);
            else if (r == 18) spotDialog (cp);
            else if (r == 19)
            {
                // Pro Tools: the User Time Stamp remembers where the clip's audio sits, so Spot can put it back there
                auto targets = state.selectedClips;
                if (targets.empty()) targets.push_back (cp);
                for (auto& sc : targets)
                    sc->state.setProperty ("sqmUserStamp", (sc->getPosition().getStart() - sc->getPosition().getOffset()).inSeconds(), &sc->edit.getUndoManager());
                session.log.add ("You", "User Time Stamp set on " + juce::String ((int) targets.size()) + " clip(s)");
            }
            else if (r == 10) { if (auto mc = dynamic_cast<te::MidiClip*> (cp.get())) if (onOpenMidiClip) onOpenMidiClip (*mc); }
            else if (r == 11) { if (auto sc = dynamic_cast<te::StepClip*> (cp.get())) if (onOpenStepClip) onOpenStepClip (*sc); }
            else if (r >= 100 && r < 100 + Theme::trackPalette().size()) for (auto& c2 : state.selectedClips) c2->setColour (Theme::trackPalette()[r - 100]);
            else if (r == 199) for (auto& c2 : state.selectedClips) c2->state.removeProperty (te::IDs::colour, &session.edit->getUndoManager());
            else if (ac != nullptr)
            {
                auto forAllAudio = [this] (std::function<void (te::AudioClipBase&)> fn) { for (auto& c2 : state.selectedClips) if (auto a2 = dynamic_cast<te::AudioClipBase*> (c2.get())) fn (*a2); };
                if (r == 20) forAllAudio ([] (auto& a) { a.setGainDB (a.getGainDB() + 1.0f); });
                else if (r == 21) forAllAudio ([] (auto& a) { a.setGainDB (a.getGainDB() - 1.0f); });
                else if (r == 4) forAllAudio ([] (auto& a) { a.setGainDB (a.getGainDB() + 3.0f); });
                else if (r == 5) forAllAudio ([] (auto& a) { a.setGainDB (a.getGainDB() - 3.0f); });
                else if (r == 22) forAllAudio ([] (auto& a) { a.setGainDB (0.0f); });
                else if (r == 23) gainDialog();
                else if (r == 24) forAllAudio ([this] (auto& a) { normaliseClipGain (a); });
                else if (r == 6) forAllAudio ([] (auto& a) { a.setIsReversed (! a.getIsReversed()); });
                else if (r == 7) EditOps::fade (session, state);
                else if (r == 25) forAllAudio ([] (auto& a) { a.setFadeIn ({}); a.setFadeOut ({}); });
                else if (r == 26) stripSilenceDialog();
                else if (r == 27) separateAtTransients (0.5f);
                else if (r == 28) { juce::String err; EditOps::consolidate (session, state, err); if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Consolidate", err); }
                else if (r == 29) EditOps::heal (session, state);
                else if (r == 40) forAllAudio ([] (auto& a) { if (a.getTimeStretchMode() == te::TimeStretcher::disabled) a.setTimeStretchMode (Session::elasticToStretchMode (1)); a.performTempoDetect(); a.setAutoTempo (true); });
                else if (r == 41) forAllAudio ([] (auto& a) { if (! a.getAutoTempo() && a.getTimeStretchMode() == te::TimeStretcher::disabled) a.setTimeStretchMode (Session::elasticToStretchMode (1)); a.setAutoTempo (! a.getAutoTempo()); });
                else if (r == 42 || r == 43) forAllAudio ([r] (auto& a) { a.setSpeedRatio (juce::jlimit (0.1, 10.0, a.getSpeedRatio() * (r == 42 ? 0.5 : 2.0))); });
                else if (r == 44) speedDialog();
                else if (r == 24) { if (openInCutLab) openInCutLab (*cp); }
                else if (r >= 50 && r <= 55) applyAudioSuite (r);
                else if (r >= 60 && r < 90) ac->setCurrentTake (r - 60);
                else if (r == 90) ac->unpackTakes (true);
                else if (r == 91) ac->unpackTakes (false);
                else if (PluginMenu::isSearch (r))
                {
                    auto sp = juce::Component::SafePointer<juce::Component> (this);
                    PluginMenu::showSearch (session.getEngine(), PluginMenu::effects, true, [this, sp] (int id)
                    {
                        if (sp != nullptr) if (auto p = PluginMenu::createFromResult (session, id)) audioSuitePlugin (p);
                    });
                }
                else if (auto p = PluginMenu::createFromResult (session, r)) audioSuitePlugin (p);
            }
            notify();
        });
    }

public:
    //==========================================================================
    void repeatDialog()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Repeat", "How many times?", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", "4");
        aw->addButton ("Repeat", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r) { aw->setVisible (false); if (r == 1) { EditOps::duplicate (session, state, juce::jlimit (1, 200, aw->getTextEditorContents ("n").getIntValue())); notify(); } }));
    }

    void gainDialog()
    {
        auto ac = dynamic_cast<te::AudioClipBase*> (state.selectedClip.get());
        if (ac == nullptr) return;
        auto aw = std::make_shared<juce::AlertWindow> ("Clip Gain", "Gain in dB (-60 to +24):", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("db", juce::String (ac->getGainDB(), 1));
        aw->addButton ("Apply", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            auto db = (float) juce::jlimit (-60.0, 24.0, aw->getTextEditorContents ("db").getDoubleValue());
            for (auto& c : state.selectedClips) if (auto a = dynamic_cast<te::AudioClipBase*> (c.get())) a->setGainDB (db);
            notify();
        }));
    }

    void speedDialog()
    {
        auto ac = dynamic_cast<te::AudioClipBase*> (state.selectedClip.get());
        if (ac == nullptr) return;
        auto aw = std::make_shared<juce::AlertWindow> ("Speed / Pitch", "Speed ratio (1 = original, 2 = twice as fast) and pitch change in semitones:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("speed", juce::String (ac->getSpeedRatio(), 3), "Speed");
        aw->addTextEditor ("pitch", juce::String (ac->getPitchChange(), 2), "Pitch (semitones)");
        aw->addButton ("Apply", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            for (auto& c : state.selectedClips)
                if (auto a = dynamic_cast<te::AudioClipBase*> (c.get()))
                {
                    if (a->getTimeStretchMode() == te::TimeStretcher::disabled) a->setTimeStretchMode (Session::elasticToStretchMode (1));
                    a->setSpeedRatio (juce::jlimit (0.1, 10.0, aw->getTextEditorContents ("speed").getDoubleValue()));
                    a->setPitchChange ((float) juce::jlimit (-24.0, 24.0, aw->getTextEditorContents ("pitch").getDoubleValue()));
                }
            notify();
        }));
    }

    void stripSilenceDialog()
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Strip Silence", "Removes the silent parts of the selected clips, splitting them into separate clips.", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("thr", juce::String (juce::roundToInt (Prefs::stripSilenceThreshold())), "Threshold (dBFS)");
        aw->addTextEditor ("min", juce::String (juce::roundToInt (Prefs::stripSilenceMinLength() * 1000.0)), "Minimum strip length (ms)");
        aw->addTextEditor ("pad", juce::String (juce::roundToInt (Prefs::stripSilencePad() * 1000.0)), "Clip start / end pad (ms)");
        aw->addButton ("Strip", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            aw->setVisible (false);
            if (r != 1) return;
            EditOps::stripSilence (session, state, aw->getTextEditorContents ("thr").getDoubleValue(), aw->getTextEditorContents ("min").getDoubleValue() / 1000.0, aw->getTextEditorContents ("pad").getDoubleValue() / 1000.0);
            notify();
        }));
    }

    void normaliseClipGain (te::AudioClipBase& a)
    {
        auto wc = dynamic_cast<te::WaveAudioClip*> (&a);
        if (wc == nullptr) return;
        te::AudioFile af (wc->getPlaybackFile());
        auto reader = std::unique_ptr<juce::AudioFormatReader> (session.getEngine().getAudioFileFormatManager().readFormatManager.createReaderFor (af.getFile()));
        if (reader == nullptr) return;
        auto pos = a.getPosition();
        auto s = (juce::int64) (pos.getOffset().inSeconds() * reader->sampleRate);
        auto n = juce::jmin ((juce::int64) (pos.getLength().inSeconds() * a.getSpeedRatio() * reader->sampleRate), (juce::int64) reader->lengthInSamples - s);
        if (n <= 0) return;
        float peak = 0;
        juce::int64 done = 0;
        juce::AudioBuffer<float> buf ((int) reader->numChannels, 65536);
        while (done < n)
        {
            auto chunk = (int) juce::jmin ((juce::int64) 65536, n - done);
            reader->read (&buf, 0, chunk, s + done, true, true);
            for (int ch = 0; ch < buf.getNumChannels(); ++ch) peak = juce::jmax (peak, buf.getMagnitude (ch, 0, chunk));
            done += chunk;
        }
        if (peak > 0.00001f) a.setGainDB (juce::Decibels::gainToDecibels (juce::Decibels::decibelsToGain (-1.0f) / peak));
    }

    /** AudioSuite-style rendered effects, implemented with Tracktion clip effects (non-destructive chain). */
    void applyAudioSuite (int which)
    {
        for (auto& c : state.selectedClips)
        {
            auto ac = dynamic_cast<te::AudioClipBase*> (c.get());
            if (ac == nullptr) continue;
            if (which == 55) { ac->enableEffects (false, false); continue; }
            ac->enableEffects (true, false);
            auto fxTree = ac->state.getChildWithName (te::IDs::EFFECTS);
            if (! fxTree.isValid()) continue;
            using T = te::ClipEffect::EffectType;
            auto um = &session.edit->getUndoManager();
            if (which == 50) fxTree.addChild (te::ClipEffect::create (T::normalise), -1, um);
            else if (which == 51) fxTree.addChild (te::ClipEffect::create (T::reverse), -1, um);
            else if (which == 52) fxTree.addChild (te::ClipEffect::create (T::invert), -1, um);
            else if (which == 53) fxTree.addChild (te::ClipEffect::create (T::makeMono), -1, um);
            else if (which == 54)
            {
                auto aw = std::make_shared<juce::AlertWindow> ("Pitch Shift (render)", "Semitones (-24 to +24):", juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("st", "0");
                aw->addButton ("Render", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                te::Clip::Ptr cp (c);
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, cp] (int r)
                {
                    aw->setVisible (false);
                    if (r != 1) return;
                    if (auto a2 = dynamic_cast<te::AudioClipBase*> (cp.get()))
                        if (auto fx2 = a2->state.getChildWithName (te::IDs::EFFECTS); fx2.isValid())
                        {
                            auto v = te::ClipEffect::create (te::ClipEffect::EffectType::pitchShift);
                            auto ps = te::PitchShiftPlugin::create();
                            ps.setProperty (te::IDs::semitonesUp, (float) juce::jlimit (-24.0, 24.0, aw->getTextEditorContents ("st").getDoubleValue()), nullptr);
                            v.addChild (ps, -1, nullptr);
                            fx2.addChild (v, -1, &cp->edit.getUndoManager());
                        }
                }));
            }
        }
    }

    void audioSuitePlugin (te::Plugin::Ptr p)
    {
        for (auto& c : state.selectedClips)
            if (auto ac = dynamic_cast<te::AudioClipBase*> (c.get()))
            {
                ac->enableEffects (true, false);
                if (auto fx = ac->state.getChildWithName (te::IDs::EFFECTS); fx.isValid())
                {
                    auto v = te::ClipEffect::create (te::ClipEffect::EffectType::filter);
                    p->flushPluginStateToValueTree();
                    v.addChild (p->state.createCopy(), -1, nullptr);
                    fx.addChild (v, -1, &session.edit->getUndoManager());
                }
            }
        session.log.add ("You", "Rendered " + p->getName() + " onto " + juce::String ((int) state.selectedClips.size()) + " clip(s) (AudioSuite)");
    }
};
