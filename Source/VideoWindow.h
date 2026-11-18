#pragma once
// Squelchmoth 1.28 - the Video window: the session's picture, following the transport.
// It shows the frame that belongs to the playhead - exact when stopped or scrubbing, decoded ahead while playing - with
// timecode and frame number. Drop a video on it to bring one in. Editing the picture comes later; this is the viewer.

#include <JuceHeader.h>
#include "Console.h"
#include "Frame.h"

class VideoContent : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    /** The app tells the window where the session is: video file, where it sits on the timeline, playhead, playing. */
    struct State { juce::File video; double videoStartsAt = 0, playhead = 0; bool playing = false; };
    std::function<State()> readState;
    std::function<juce::String (const juce::File&)> importVideo;       // brings a video into the session; returns what happened

    VideoContent()
    {
        namespace L = Console::Look;
        for (auto* b : { &importBtn, &toolsBtn })
        {
            addAndMakeVisible (*b); b->setColour (juce::TextButton::buttonColourId, L::well()); b->setColour (juce::TextButton::textColourOffId, L::text());
        }
        importBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a video", juce::File(), "*.mp4;*.mov;*.m4v;*.mkv;*.webm;*.avi");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) bringIn (fc.getResult()); });
        };
        toolsBtn.onClick = [this] { Frame::startInstall(); };
        setSize (880, 560);
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
        if (st.video != player.source()) { player.setSource (st.video); shown = {}; shownIndex = -1; }
        state = st;
        if (! player.ready()) { if (shown.isValid()) { shown = {}; repaint(); } return; }
        juce::int64 idx = -1;
        auto img = player.frameFor (st.playhead - st.videoStartsAt, st.playing, &idx);
        const double inVideo = st.playhead - st.videoStartsAt;
        const bool inside = inVideo >= 0 && inVideo < player.facts().seconds;
        if (! inside) { img = {}; idx = -1; }
        if (idx != shownIndex || img.isValid() != shown.isValid() || std::abs (st.playhead - lastPlayhead) > 1.0e-4) { shown = img; shownIndex = idx; lastPlayhead = st.playhead; repaint(); }
    }

    juce::int64 frameShown() const   { return shownIndex; }
    juce::Image imageShown() const   { return shown; }
    Frame::Player& getPlayer()       { return player; }

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
                                   : state.playhead < state.videoStartsAt ? juce::String ("THE PICTURE STARTS LATER") : juce::String ("THE PICTURE HAS ENDED");
            g.drawText (big, screen.withTrimmedBottom (screen.getHeight() / 2 + 4), juce::Justification::centredBottom);
            g.setFont (Theme::font (12.5f));
            g.drawText (! player.ready() ? "MP4, MOV, MKV, WEBM or AVI. Its sound goes on a track; the picture plays here in step with the session."
                                         : "The video sits at " + juce::String (state.videoStartsAt, 2) + " s on the timeline and is " + juce::String (in.seconds, 2) + " s long.",
                        screen.withTrimmedTop (screen.getHeight() / 2 + 6), juce::Justification::centredTop);
        }
        if (hover) { g.setColour (L::cyan().withAlpha (0.18f)); g.fillRoundedRectangle (screen.toFloat(), 6.0f); g.setColour (L::cyan()); g.drawRoundedRectangle (screen.toFloat().reduced (1.0f), 6.0f, 2.0f); }

        // the numbers: session time, timecode inside the video, frame number
        g.setColour (L::well().withAlpha (0.85f)); g.fillRoundedRectangle (footer.toFloat(), 6.0f);
        const double inVideo = state.playhead - state.videoStartsAt;
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
        r.reduce (14, 4);
        noteArea = r.removeFromBottom (18);
        footer = r.removeFromBottom (58); r.removeFromBottom (8);
        screen = r;
    }

private:
    void timerCallback() override { update(); if (Frame::installState().running) repaint(); }

    Frame::Player player; State state; juce::Image shown; juce::int64 shownIndex = -1; double lastPlayhead = -1.0;
    juce::TextButton importBtn { "Import video..." }, toolsBtn { "Get video tools" };
    std::unique_ptr<juce::FileChooser> chooser; juce::String note; bool hover = false;
    juce::Rectangle<int> header, screen, footer, noteArea;
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
        return s;
    };
    c.importVideo = [&tools] (const juce::File& f)
    {
        juce::var args (new juce::DynamicObject()); args.getDynamicObject()->setProperty ("path", f.getFullPathName());
        return tools.call ("video_import", args).text;
    };
}
