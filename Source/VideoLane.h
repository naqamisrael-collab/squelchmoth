#pragma once
// Squelchmoth 1.30 - the video lane: the session's picture on the timeline, under the rulers.
// It shows the video as a block of stills where it sits in time. Drag the block to move the video (its sound moves with
// it); click the lane and use the left / right arrow keys to nudge it one frame (Shift: one second); double-click opens
// the Video window. It appears only when the session has a video, and View > Video Lane hides it.

#include <JuceHeader.h>
#include "Session.h"
#include "EditState.h"
#include "Theme.h"
#include "Prefs.h"
#include "Frame.h"
#include "VideoProxy.h"

namespace te = tracktion;

namespace VideoMove
{
    /** The audio clip that holds the video's own sound, when it was brought in with the video. */
    inline te::Clip* soundClip (te::Edit& e)
    {
        const auto id = e.state["frameVideoClip"].toString(); if (id.isEmpty()) return nullptr;
        for (auto t : te::getAudioTracks (e)) for (auto c : t->getClips()) if (c->itemID.toString() == id) return c;
        return nullptr;
    }

    /** Moves the session's video to start at newStart seconds; its sound clip moves by the same amount when withSound.
        One undo step. Returns what happened. */
    inline juce::String moveTo (te::Edit& e, double newStart, bool withSound)
    {
        if (e.state["frameVideo"].toString().isEmpty()) return "This session has no video";
        const double from = (double) e.state.getProperty ("frameVideoAt", 0.0), to = SqStrip::movedStart (newStart), delta = to - from;
        juce::String s = "The video now starts at " + juce::String (to, 3) + " s (was " + juce::String (from, 3) + " s).";
        if (std::abs (delta) < 1.0e-9) return "The video already starts at " + juce::String (to, 3) + " s.";
        e.getUndoManager().beginNewTransaction ("Move video");
        e.state.setProperty ("frameVideoAt", to, &e.getUndoManager());
        if (withSound)
        {
            if (auto* c = soundClip (e))
            {
                const double cs = juce::jmax (0.0, c->getPosition().getStart().inSeconds() + delta);
                c->setStart (te::TimePosition::fromSeconds (cs), false, true);
                s << " Its sound moved with it to " << juce::String (cs, 3) << " s.";
            }
            else s << " No sound clip is tied to this video, so only the picture moved.";
        }
        else s << " Its sound was left where it is.";
        return s;
    }
}

class VideoLane : public juce::Component
{
public:
    static constexpr int laneH = 46;
    VideoLane (Session& s, TimelineState& st) : session (s), state (st) { setWantsKeyboardFocus (true); }

    std::function<void()> onOpenWindow;      // double-click
    std::function<void()> onMoved;           // after the video was moved

    static bool enabled() { return Prefs::getInt ("ed_videoLane", 1) != 0; }

    /** Whether the lane should show: the session has a video file and the lane is not hidden. */
    bool wanted() const
    {
        if (session.edit == nullptr || ! enabled()) return false;
        const auto path = session.edit->state["frameVideo"].toString();
        return path.isNotEmpty() && (path == file.getFullPathName() ? fileThere : juce::File (path).existsAsFile());
    }

    /** Called from the timeline's timer: follows the session's video, the view and the stills as they arrive. */
    void refresh()
    {
        if (session.edit == nullptr) return;
        auto& e = *session.edit;
        const juce::File f (e.state["frameVideo"].toString());
        if (f != file) { file = f; fileThere = f.existsAsFile(); info = fileThere && Frame::ffmpeg().existsAsFile() ? Frame::probe (f) : Frame::Info(); probedWithTools = Frame::ffmpeg().existsAsFile(); if (info.ok) VideoProxy::follow (file, info); }
        else if (! probedWithTools && fileThere && Frame::ffmpeg().existsAsFile()) { info = Frame::probe (f); probedWithTools = true; if (info.ok) VideoProxy::follow (file, info); }
        if (! isVisible()) return;
        strip.setPaused (e.getTransport().isPlaying()); Frame::Proxies::get().setPaused (e.getTransport().isPlaying());
        if (info.ok && info.hasVideo) strip.setSource (file, info, (int) juce::jlimit ((long long) 1, (long long) 120, SqStrip::frameCount (info.seconds, info.fps)), 96);
        const double at = startsAt(), ph = e.getTransport().getPosition().inSeconds();
        if (strip.done() != seenDone || at != seenAt || state.viewStart != seenView || state.secondsPerPixel != seenSpp || ph != seenPlayhead)
        { seenDone = strip.done(); seenAt = at; seenView = state.viewStart; seenSpp = state.secondsPerPixel; seenPlayhead = ph; repaint(); }
    }

    double startsAt() const { return session.edit != nullptr ? (double) session.edit->state.getProperty ("frameVideoAt", 0.0) : 0.0; }
    const Frame::Info& facts() const { return info; }
    Frame::Strip& getStrip() { return strip; }

    /** The block's left and right edge in this component's pixels (may be off screen). */
    juce::Range<float> blockX() const
    {
        const float x0 = (float) headerWidthNow();
        const double at = startsAt() + dragDelta;
        return { x0 + state.timeToX (at), x0 + state.timeToX (at + juce::jmax (0.0, info.seconds)) };
    }

    /** Moves the video by a number of seconds (the mouse and the keys both end here). */
    juce::String moveBy (double seconds)
    {
        if (session.edit == nullptr) return {};
        const auto r = VideoMove::moveTo (*session.edit, startsAt() + seconds, true);
        if (onMoved) onMoved();
        repaint(); return r;
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        const int hx = headerWidthNow();
        g.setColour (Theme::dim()); g.setFont (Theme::font (10.5f));
        g.drawText ("VIDEO", 10, 0, 60, getHeight(), juce::Justification::centredLeft);
        g.setColour (Theme::text().withAlpha (0.8f)); g.setFont (Theme::font (11.0f));
        g.drawText (file.getFileName(), 62, 0, juce::jmax (0, hx - 70), getHeight(), juce::Justification::centredLeft, true);
        g.setColour (Theme::line()); g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth()); g.drawVerticalLine (hx - 1, 0.0f, (float) getHeight());

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (hx, 0, getWidth() - hx, getHeight());
        g.setColour (Theme::bg()); g.fillRect (hx, 0, getWidth() - hx, getHeight() - 1);
        if (! info.ok || ! info.hasVideo)
        {
            g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f));
            g.drawText (Frame::ffmpeg().existsAsFile() ? "This video's picture can't be read" : "The video tools are needed to show the picture (Window > Video)", hx + 10, 0, getWidth() - hx - 20, getHeight(), juce::Justification::centredLeft);
            return;
        }
        const auto bx = blockX(); const float top = 3.0f, h = (float) getHeight() - 7.0f;
        const juce::Rectangle<float> block (bx.getStart(), top, juce::jmax (2.0f, bx.getLength()), h);
        g.setColour (juce::Colours::black); g.fillRect (block);
        const double tileW = juce::jmax (12.0, (double) h * info.width / juce::jmax (1, info.height));
        const int n = strip.size();
        for (auto& t : SqStrip::tiles ((double) bx.getStart(), (double) bx.getEnd(), (double) hx, (double) getWidth(), tileW, n))
        {
            const juce::Rectangle<float> cell ((float) t.x, top, (float) t.w, h);
            const auto th = strip.thumb (t.thumb);
            if (th.isValid())
            {
                juce::Graphics::ScopedSaveState s2 (g); g.reduceClipRegion (cell.getSmallestIntegerContainer());
                g.drawImage (th, juce::Rectangle<float> ((float) t.x, top, (float) tileW, h), juce::RectanglePlacement::stretchToFit);
            }
            else { g.setColour (Theme::panel2().withAlpha (0.6f)); g.fillRect (cell.reduced (0.5f, 0.0f)); }
        }
        g.setColour (dragging ? Theme::acid() : (hasKeyboardFocus (false) ? Theme::acid().withAlpha (0.8f) : Theme::line().brighter (0.3f)));
        g.drawRect (block, dragging || hasKeyboardFocus (false) ? 1.6f : 1.0f);
        if (dragging)
        {
            const juce::String tip = juce::String (startsAt() + dragDelta, 2) + " s";
            g.setFont (Theme::font (11.0f)); const int w = 70;
            const juce::Rectangle<int> chip (juce::jmax (hx + 2, (int) bx.getStart() + 4), 5, w, 16);
            g.setColour (juce::Colours::black.withAlpha (0.75f)); g.fillRoundedRectangle (chip.toFloat(), 3.0f);
            g.setColour (Theme::acid()); g.drawText (tip, chip, juce::Justification::centred);
        }
        if (session.edit != nullptr)
        {
            const float px = (float) hx + state.timeToX (session.edit->getTransport().getPosition().inSeconds());
            if (px >= (float) hx) { g.setColour (Theme::acid()); g.fillRect (px, 0.0f, 2.0f, (float) getHeight()); }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus(); repaint();
        const auto bx = blockX();
        if (e.x >= headerWidthNow() && info.ok && (float) e.x >= bx.getStart() && (float) e.x <= bx.getEnd()) { dragging = true; dragDelta = 0; }
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) dragBy (e.getDistanceFromDragStartX(), e.mods.isCommandDown()); }
    void mouseUp (const juce::MouseEvent&) override     { if (dragging) dropDrag(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onOpenWindow) onOpenWindow(); }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

    /** The drag itself, as the mouse does it (the self-test calls these too): pixels moved so far, then the drop. */
    void dragBy (int pixels, bool noSnap = false)
    {
        if (session.edit == nullptr) return;
        dragging = true;
        const double want = state.snapIfNeeded (*session.edit, juce::jmax (0.0, startsAt() + (double) pixels * state.secondsPerPixel), noSnap);
        dragDelta = want - startsAt(); repaint();
    }
    juce::String dropDrag()
    {
        const double d = dragDelta; dragging = false; dragDelta = 0;
        return std::abs (d) > 1.0e-9 ? moveBy (d) : (repaint(), juce::String());
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (! info.ok || info.fps <= 0) return false;
        const bool shift = k.getModifiers().isShiftDown(); const double step = shift ? 1.0 : 1.0 / info.fps;
        if (k.getKeyCode() == juce::KeyPress::rightKey) { moveBy (step); return true; }
        if (k.getKeyCode() == juce::KeyPress::leftKey)  { moveBy (-step); return true; }
        if (k.getKeyCode() == juce::KeyPress::returnKey) { if (onOpenWindow) onOpenWindow(); return true; }
        return false;
    }

private:
    Session& session; TimelineState& state;
    juce::File file; bool fileThere = false, probedWithTools = false; Frame::Info info; Frame::Strip strip;
    bool dragging = false; double dragDelta = 0;
    int seenDone = -1; double seenAt = -1, seenView = -1, seenSpp = -1, seenPlayhead = -1;
};
