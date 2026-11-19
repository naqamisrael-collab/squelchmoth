#pragma once
// Squelchmoth 1.28 - the Video window: the session's picture, following the transport.
// It shows the frame that belongs to the playhead - exact when stopped or scrubbing, decoded ahead while playing - with
// timecode and frame number. Drop a video on it to bring one in. Editing the picture comes later; this is the viewer.
// 1.32: the picture follows the clips of the video's sound, so cutting those clips cuts the picture.
// 1.31: long or large videos get a proxy in the background (the Proxy button shows it), so scrubbing keeps up anywhere.
// 1.29: a strip of stills along the bottom shows the whole video; click or drag it to go there, arrow keys step by frame.

#include <JuceHeader.h>
#include "Console.h"
#include "Frame.h"
#include "VideoProxy.h"
#include "VideoCuts.h"

class VideoContent : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    /** The app tells the window where the session is: video file, where it sits on the timeline, playhead, playing. */
    struct State { juce::File video; double videoStartsAt = 0, playhead = 0; bool playing = false;
                   bool cut = false; std::vector<SqCut::Piece> pieces; double slip = 0; };   // cut: pieces say where the picture is (otherwise one whole piece at videoStartsAt)

    std::function<State()> readState;
    std::function<juce::String (const juce::File&)> importVideo;       // brings a video into the session; returns what happened
    std::function<void (double)> seek;                                 // moves the session's playhead (seconds on the timeline)

    VideoContent()
    {
        namespace L = Console::Look;
        for (auto* b : { &importBtn, &toolsBtn, &proxyBtn })
        {
            addAndMakeVisible (*b); b->setColour (juce::TextButton::buttonColourId, L::well()); b->setColour (juce::TextButton::textColourOffId, L::text());
        }
        importBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a video", juce::File(), "*.mp4;*.mov;*.m4v;*.mkv;*.webm;*.avi");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) bringIn (fc.getResult()); });
        };
        toolsBtn.onClick = [this] { Frame::startInstall(); };
        proxyBtn.setTooltip ("A proxy is a small copy of the video that makes stills and scrubbing fast anywhere in a long or large video.");
        proxyBtn.onClick = [this]
        {
            if (! player.ready()) return;
            const auto st = Frame::Proxies::get().status (player.source());
            juce::PopupMenu m;
            m.addSectionHeader (VideoProxy::sentence (player.source(), st).upToFirstOccurrenceOf (".", true, false));
            m.addItem (1, "Make a proxy of this video  (P)", ! st.complete() && ! (st.wanted && st.error.isEmpty()));
            m.addItem (2, "Remove this video's proxy", st.known && (st.made > 0 || st.wanted));
            m.addSeparator();
            m.addItem (3, "Make proxies of long or large videos without asking", true, VideoProxy::autoOn());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&proxyBtn), [safe = juce::Component::SafePointer<VideoContent> (this)] (int r)
            {
                if (safe == nullptr) return;
                if (r == 1) safe->makeProxy();
                else if (r == 2) safe->removeProxy();
                else if (r == 3) Prefs::set ("frame_autoProxy", VideoProxy::autoOn() ? 0 : 1);
            });
        };
        setWantsKeyboardFocus (true);
        setSize (880, 630);
        startTimerHz (30);
    }
    ~VideoContent() override { stopTimer(); }

    //==========================================================================
    bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) if (Frame::isVideo (juce::File (f))) return true; return false; }
    void fileDragEnter (const juce::StringArray&, int, int) override        { hover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override                   { hover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override   { hover = false; for (auto& f : files) if (Frame::isVideo (juce::File (f))) { bringIn (juce::File (f)); break; } repaint(); }

    void bringIn (const juce::File& f) { note = importVideo ? importVideo (f).upToFirstOccurrenceOf ("\n", false, false) : juce::String ("Open a session first."); update(); repaint(); }

    /** One step of the window's clock (the timer calls this; the self-test calls it directly). */
    void update()
    {
        const bool haveTools = Frame::ffmpeg().existsAsFile();
        toolsBtn.setVisible (! haveTools && ! Frame::installState().running);
        if (! readState) return;
        const auto st = readState();
        if (st.video != player.source()) { player.setSource (st.video); shown = {}; shownIndex = -1; if (player.ready() && haveTools) VideoProxy::follow (player.source(), player.facts()); proxyTick = 0; }
        Frame::Proxies::get().setPaused (st.playing);
        proxyBtn.setVisible (player.ready() && haveTools);
        if (player.ready() && (proxyTick++ % 15) == 0) { const auto lab = VideoProxy::label (Frame::Proxies::get().status (player.source())); if (lab != proxyBtn.getButtonText()) proxyBtn.setButtonText (lab); }
        state = st;
        if (! player.ready()) { strip.setSource ({}, {}, 0); if (shown.isValid()) { shown = {}; repaint(); } return; }
        strip.setPaused (st.playing);
        if (haveTools)
        {
            // keep the stills while the window is being resized: fetch again only when the count changes by more than a quarter
            const int want = stripSlots(), have = strip.size();
            strip.setSource (player.source(), player.facts(), have > 0 && std::abs (want - have) * 4 <= have ? have : want);
        }
        if (strip.done() != stripSeen) { stripSeen = strip.done(); repaint (stripArea); }
        juce::int64 idx = -1;
        const auto hit = hitAt (st); const double inVideo = hit.src;
        const bool inside = hit.shows();
        auto img = inside ? player.frameFor (inVideo, st.playing, &idx) : juce::Image();
        if (! inside) { img = {}; idx = -1; }
        if (idx != shownIndex || img.isValid() != shown.isValid() || std::abs (st.playhead - lastPlayhead) > 1.0e-4) { shown = img; shownIndex = idx; lastPlayhead = st.playhead; repaint(); }
    }

    juce::int64 frameShown() const   { return shownIndex; }
    juce::Image imageShown() const   { return shown; }
    Frame::Player& getPlayer()       { return player; }
    /** The picture's pieces for a state, and what shows at its playhead. */
    std::vector<SqCut::Piece> piecesOf (const State& s) const { if (s.cut) return s.pieces; return { { s.videoStartsAt, juce::jmax (0.0, player.facts().seconds), 0.0, 1.0 } }; }
    SqCut::Hit hitAt (const State& s) const { return SqCut::lookup (piecesOf (s), s.playhead, s.slip, player.facts().seconds); }
    Frame::Strip& getStrip()         { return strip; }
    /** Asks for this video's proxy to be made (the button, the P key and the tool all end in the same call). */
    void makeProxy()   { if (player.ready()) { Frame::Proxies::get().open (player.source(), player.facts(), true); proxyTick = 0; } }
    void removeProxy() { if (player.ready()) { Frame::Proxies::get().remove (player.source()); proxyTick = 0; } }
    juce::Rectangle<int> stripBounds() const { return stripArea; }

    /** How many stills the strip holds at this width: each is as wide as the video's shape makes it at the strip's height. */
    int stripSlots() const
    {
        const auto& in = player.facts(); if (! player.ready() || in.height <= 0) return 0;
        const int slotW = juce::jmax (24, juce::roundToInt ((double) (stripArea.getHeight() - 4) * in.width / in.height));
        return SqStrip::slotsFor (stripArea.getWidth(), slotW);
    }

    /** Goes to the moment under a point of the strip (x in this component). Returns the session time, or -1 when there is no video. */
    double seekToStripX (int x)
    {
        if (! player.ready() || stripArea.getWidth() <= 0) return -1.0;
        // the last frame's start is the furthest a click can go, so the end of the strip still shows a picture
        const auto& in = player.facts();
        const double inVideo = juce::jmin (SqStrip::timeAt ((double) (x - stripArea.getX()), (double) stripArea.getWidth(), in.seconds), (double) (SqStrip::frameCount (in.seconds, in.fps) - 1) / in.fps);
        const double t = SqCut::whereIs (piecesOf (state), inVideo, state.slip, in.seconds);
        if (t < 0) { note = "That moment of the video is not in the session (it was cut out)."; repaint(); return -1.0; }
        if (note.startsWith ("That moment")) note.clear();
        if (seek) seek (t); return t;
    }

    /** Moves by whole frames of the video from the playhead. Returns the new session time, or -1 when there is no video. */
    double stepFrames (juce::int64 frames)
    {
        if (! player.ready()) return -1.0;
        const auto& in = player.facts(); const auto st = readState ? readState() : state;
        // step inside the video when the playhead is on a piece and the next frame is still in the session; otherwise by the same time on the timeline
        const auto pieces = piecesOf (st); const auto hit = SqCut::lookup (pieces, st.playhead, st.slip, in.seconds);
        double t = -1.0;
        if (hit.shows()) t = SqCut::whereIs (pieces, SqStrip::stepFrames (hit.src, in.fps, (long long) frames, in.seconds), st.slip, in.seconds);
        if (t < 0) t = juce::jmax (0.0, st.playhead + (double) frames / in.fps);
        if (seek) seek (t); return t;
    }

    void mouseDown (const juce::MouseEvent& e) override { grabKeyboardFocus(); if (stripArea.contains (e.getPosition())) { dragging = true; seekToStripX (e.x); } }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) seekToStripX (e.x); }
    void mouseUp (const juce::MouseEvent&) override     { dragging = false; }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (! player.ready()) return false;
        const auto& in = player.facts(); const juce::int64 second = juce::jmax ((juce::int64) 1, (juce::int64) juce::roundToInt (in.fps));
        const bool shift = k.getModifiers().isShiftDown();
        if (k.getKeyCode() == juce::KeyPress::rightKey) { stepFrames (shift ? second : 1); return true; }
        if (k.getKeyCode() == juce::KeyPress::leftKey)  { stepFrames (shift ? -second : -1); return true; }
        if (k.getKeyCode() == juce::KeyPress::homeKey)  { if (seek) seek (SqCut::start (piecesOf (state))); return true; }
        if (k.getKeyCode() == juce::KeyPress::endKey)   { seekToStripX (stripArea.getRight()); return true; }
        if (k.getTextCharacter() == 'p' || k.getTextCharacter() == 'P') { makeProxy(); return true; }
        return false;
    }

    static juce::String timecode (double seconds, double fps)
    {
        if (fps <= 0) fps = 30.0;
        const auto total = (juce::int64) std::floor (juce::jmax (0.0, seconds) * fps + 1.0e-6); const int f = juce::roundToInt (fps);
        const auto frames = total % f; const auto s = total / f;
        return juce::String (s / 3600).paddedLeft ('0', 2) + ":" + juce::String ((s / 60) % 60).paddedLeft ('0', 2) + ":" + juce::String (s % 60).paddedLeft ('0', 2) + ":" + juce::String (frames).paddedLeft ('0', 2);
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        namespace L = Console::Look;
        g.setGradientFill (juce::ColourGradient (L::bodyTop(), 0.0f, 0.0f, L::bodyBot(), 0.0f, (float) getHeight(), false)); g.fillAll();
        g.setColour (L::text()); g.setFont (L::caps (15.0f)); g.drawText ("VIDEO", header.withTrimmedLeft (18), juce::Justification::centredLeft);
        const auto& in = player.facts();
        g.setColour (L::dim()); g.setFont (Theme::font (12.0f));
        g.drawText (player.ready() ? player.source().getFileName() + "   " + juce::String (in.width) + " x " + juce::String (in.height) + "   " + juce::String (in.fps, in.fps == std::floor (in.fps) ? 0 : 2) + " fps"
                                   : juce::String ("The session's picture, following the playhead."),
                    header.withTrimmedLeft (92).withTrimmedRight (270), juce::Justification::centredLeft);

        g.setColour (juce::Colours::black); g.fillRoundedRectangle (screen.toFloat(), 6.0f);
        if (shown.isValid())
            g.drawImage (shown, screen.reduced (2).toFloat(), juce::RectanglePlacement::centred);
        else
        {
            g.setColour (L::dim()); g.setFont (L::caps (17.0f));
            const auto& vi = Frame::installState();
            const juce::String big = vi.running ? "GETTING THE VIDEO TOOLS  " + juce::String (juce::roundToInt (vi.progress.load() * 100)) + " %"
                                   : ! Frame::ffmpeg().existsAsFile() ? juce::String ("VIDEO NEEDS ITS TOOLS (ONE DOWNLOAD)")
                                   : ! player.ready() ? juce::String ("DROP A VIDEO HERE")
                                   : state.playhead < SqCut::start (piecesOf (state)) ? juce::String ("THE PICTURE STARTS LATER") : state.playhead >= SqCut::end (piecesOf (state)) ? juce::String ("THE PICTURE HAS ENDED") : juce::String ("NO PICTURE HERE");
            g.drawText (big, screen.withTrimmedBottom (screen.getHeight() / 2 + 4), juce::Justification::centredBottom);
            g.setFont (Theme::font (12.5f));
            g.drawText (! player.ready() ? "MP4, MOV, MKV, WEBM or AVI. Its sound goes on a track; the picture plays here in step with the session."
                                         : state.cut && state.pieces.size() != 1 ? "The picture is in " + juce::String ((int) state.pieces.size()) + " pieces, from " + juce::String (SqCut::start (state.pieces), 2) + " to " + juce::String (SqCut::end (state.pieces), 2) + " s. It follows the clips of the video's sound."
                                         : "The picture runs from " + juce::String (SqCut::start (piecesOf (state)), 2) + " to " + juce::String (SqCut::end (piecesOf (state)), 2) + " s on the timeline.",
                        screen.withTrimmedTop (screen.getHeight() / 2 + 6), juce::Justification::centredTop);
        }
        if (hover) { g.setColour (L::cyan().withAlpha (0.18f)); g.fillRoundedRectangle (screen.toFloat(), 6.0f); g.setColour (L::cyan()); g.drawRoundedRectangle (screen.toFloat().reduced (1.0f), 6.0f, 2.0f); }

        // the strip: the whole video in stills, the playhead on it
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (stripArea.toFloat(), 4.0f);
        if (player.ready() && strip.size() > 0)
        {
            const int n = strip.size(); const auto inner = stripArea.reduced (2);
            for (int k = 0; k < n; ++k)
            {
                const int x0 = inner.getX() + k * inner.getWidth() / n, x1 = inner.getX() + (k + 1) * inner.getWidth() / n;
                const juce::Rectangle<int> cell (x0, inner.getY(), x1 - x0, inner.getHeight());
                const auto th = strip.thumb (k);
                if (th.isValid()) g.drawImage (th, cell.toFloat(), juce::RectanglePlacement::fillDestination);
                else { g.setColour (L::well().withAlpha (k % 2 ? 0.55f : 0.4f)); g.fillRect (cell); }
            }
            const auto sh = hitAt (state); const double inV = sh.src;
            if (sh.shows())
            {
                const float px = (float) stripArea.getX() + (float) SqStrip::xAt (inV, (double) stripArea.getWidth(), in.seconds);
                g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRect (px - 2.0f, (float) stripArea.getY(), 4.0f, (float) stripArea.getHeight());
                g.setColour (L::cyan()); g.fillRect (px - 1.0f, (float) stripArea.getY() - 2.0f, 2.0f, (float) stripArea.getHeight() + 4.0f);
            }
        }
        else { g.setColour (L::dim()); g.setFont (Theme::font (11.5f)); g.drawText ("The whole video appears here in stills. Click to go to a moment; the arrow keys step by frame.", stripArea, juce::Justification::centred); }

        // the numbers: session time, timecode inside the video, frame number
        g.setColour (L::well().withAlpha (0.85f)); g.fillRoundedRectangle (footer.toFloat(), 6.0f);
        const auto fh = hitAt (state); const double inVideo = fh.shows() ? fh.src : -1.0;
        auto cell = [&] (int i, const juce::String& label, const juce::String& value, juce::Colour c)
        {
            auto r = footer.withX (footer.getX() + i * footer.getWidth() / 4).withWidth (footer.getWidth() / 4);
            g.setColour (L::dim()); g.setFont (L::caps (10.5f)); g.drawText (label, r.withTrimmedLeft (14).withHeight (18).translated (0, 6), juce::Justification::centredLeft);
            g.setColour (c); g.setFont (L::caps (19.0f)); g.drawText (value, r.withTrimmedLeft (14).withTrimmedTop (22), juce::Justification::centredLeft);
        };
        cell (0, "SESSION", timecode (state.playhead, player.ready() ? in.fps : 30.0), L::text());
        cell (1, "IN THE VIDEO", player.ready() && inVideo >= 0 ? timecode (inVideo, in.fps) : juce::String ("--:--:--:--"), L::cyan());
        cell (2, "FRAME", shownIndex >= 0 ? juce::String (shownIndex) + " of " + juce::String ((juce::int64) std::floor (in.seconds * in.fps)) : juce::String ("-"), L::text());
        cell (3, "STATE", state.playing ? "PLAYING" : "STOPPED", state.playing ? L::green() : L::dim());
        if (note.isNotEmpty()) { g.setColour (L::dim()); g.setFont (Theme::font (11.5f)); g.drawText (note, noteArea, juce::Justification::centredLeft); }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        header = r.removeFromTop (46);
        importBtn.setBounds (header.getRight() - 138, header.getY() + 9, 122, 28);
        toolsBtn.setBounds (importBtn.getX() - 148, header.getY() + 9, 140, 28);
        proxyBtn.setBounds (importBtn.getX() - 118, header.getY() + 9, 110, 28);
        r.reduce (14, 4);
        noteArea = r.removeFromBottom (18);
        footer = r.removeFromBottom (58); r.removeFromBottom (8);
        stripArea = r.removeFromBottom (58); r.removeFromBottom (8);
        screen = r;
    }

private:
    void timerCallback() override { update(); if (Frame::installState().running) repaint(); }

    Frame::Player player; Frame::Strip strip; int stripSeen = -1; bool dragging = false; State state; juce::Image shown; juce::int64 shownIndex = -1; double lastPlayhead = -1.0;
    juce::TextButton importBtn { "Import video..." }, toolsBtn { "Get video tools" }, proxyBtn { "Proxy" }; int proxyTick = 0;
    std::unique_ptr<juce::FileChooser> chooser; juce::String note; bool hover = false;
    juce::Rectangle<int> header, screen, footer, noteArea, stripArea;
};

/** Connects the Video window to the session and its tools (the app and the self-test both use this). */
template <typename Tools, typename Sess>
void attachVideo (VideoContent& c, Tools& tools, Sess& session)
{
    c.readState = [&session]
    {
        VideoContent::State s;
        if (session.edit == nullptr) return s;
        auto& e = *session.edit;
        s.video = juce::File (e.state["frameVideo"].toString()); s.videoStartsAt = (double) e.state.getProperty ("frameVideoAt", 0.0);
        s.playhead = e.getTransport().getPosition().inSeconds(); s.playing = e.getTransport().isPlaying();
        if (VideoCuts::soundFile (e) != juce::File()) { const auto pic = VideoCuts::read (e, 0.0); s.cut = true; s.pieces = pic.pieces; s.slip = pic.slip; }
        else s.slip = (double) e.state.getProperty ("frameVideoSlip", 0.0);
        return s;
    };
    c.seek = [&session] (double t) { if (session.edit != nullptr) session.edit->getTransport().setPosition (tracktion::TimePosition::fromSeconds (juce::jmax (0.0, t))); };
    c.importVideo = [&tools] (const juce::File& f)
    {
        juce::var args (new juce::DynamicObject()); args.getDynamicObject()->setProperty ("path", f.getFullPathName());
        return tools.call ("video_import", args).text;
    };
}
