#pragma once
// Squelchmoth 1.18 - the Vocal Remover window: drop a song, get two lanes - Music and Vocal - each with its waveform and
// a volume slider, play them together in sync, save either one or send both to the session. Built on Prism Split.

#include "Session.h"
#include "Console.h"
#include "PrismSplit.h"
#include "Frame.h"
#include "ReferenceListener.h"

class VocalRemoverContent : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    std::function<void (const juce::Array<juce::File>& files)> onSendFiles;

    struct Lane
    {
        Lane (juce::AudioFormatManager& fm, juce::AudioThumbnailCache& c) : thumb (512, fm, c) {}
        juce::String name, note; juce::Colour colour; juce::File file; juce::Rectangle<int> area;
        juce::AudioThumbnail thumb; std::unique_ptr<juce::AudioFormatReaderSource> reader; juce::AudioTransportSource src; juce::Slider vol;
    };

    explicit VocalRemoverContent (Session& s) : session (s), thumbCache (8)
    {
        formats.registerBasicFormats();
        namespace L = Console::Look;
        auto style = [] (juce::TextButton& b, bool strong = false)
        {
            b.setColour (juce::TextButton::buttonColourId, strong ? L::cyan().withAlpha (0.22f) : L::well());
            b.setColour (juce::TextButton::textColourOffId, L::text());
        };
        for (auto* b : { &chooseBtn, &playBtn, &saveBtn, &sendBtn, &installBtn, &newBtn, &modeBtn }) { addAndMakeVisible (*b); style (*b, b == &chooseBtn || b == &playBtn); }
        chooseBtn.onClick = [this] { chooseFile(); };
        playBtn.onClick = [this] { togglePlay(); };
        saveBtn.onClick = [this] { saveMenu(); };
        sendBtn.onClick = [this] { if (job != nullptr && loaded && onSendFiles) { juce::Array<juce::File> fs; for (auto& l : lanes) fs.add (l->file); onSendFiles (fs); } };
        installBtn.onClick = [this]
        {
            juce::PopupMenu m;
            m.addSectionHeader ("Studio quality is for personal and research use");
            m.addItem (1, "Install the 4-stem separator (165 MB)", ! PrismSplit::StudioPack::installed());
            m.addItem (2, "Install the 6-stem separator with Guitar and Piano (130 MB)", ! PrismSplit::StudioPack::installed6());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&installBtn), [this] (int r) { if (r == 1 || r == 2) { PrismSplit::StudioPack::startInstall (false, r == 2); startTimerHz (10); } });
        };
        newBtn.onClick = [this] { reset(); };
        modeBtn.onClick = [this] { stemsMode = ! stemsMode; if (job != nullptr && job->finished && job->error.isEmpty()) loadResult(); refreshButtons(); repaint(); };
        modeBtn.setTooltip ("Two tracks (Music and Vocal) or four stems (Vocals, Drums, Bass, Other)");
        player.setSource (&mixer);
        setSize (880, 520);
        refreshButtons();
        startTimerHz (10);
    }

    ~VocalRemoverContent() override
    {
        stopAudio();
        player.setSource (nullptr); clearLanes();
    }

    void setStemsMode (bool four) { if (stemsMode != four) modeBtn.onClick(); }
    int numLanes() const { return (int) lanes.size(); }
    juce::String keyTempoText() const { return keyTempo; }

    //==========================================================================
    bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) if ((juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg") || Frame::isVideo (juce::File (f)))) return true; return false; }
    void fileDragEnter (const juce::StringArray&, int, int) override        { hover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override                   { hover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        hover = false;
        for (auto& f : files) if ((juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg") || Frame::isVideo (juce::File (f)))) { start (juce::File (f)); break; }
    }

    /** Starts separating a song (also what the assistant's tool calls). */
    void start (const juce::File& dropped)
    {
        reset();
        const auto label = juce::File::createLegalFileName (dropped.getFileNameWithoutExtension()).substring (0, 60);
        juce::File base = session.edit != nullptr && session.getFile().existsAsFile() ? session.getFile().getParentDirectory() : juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Squelchmoth");
        juce::String videoErr;
        const auto f = Frame::audioOf (dropped, base.getChildFile ("Separated").getChildFile (label), 44100.0, videoErr);   // a video: its sound
        if (! f.existsAsFile())
        {
            if (! Frame::ffmpeg().existsAsFile())
            {
                // this video needs the video tools: fetch them now and carry on with the same file when they arrive
                pendingVideo = dropped; Frame::startInstall(); startTimerHz (10); repaint();
            }
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Vocal Remover", videoErr);
            return;
        }
        song = f;
        studio = PrismSplit::StudioPack::installed();
        const bool six = PrismSplit::StudioPack::installed6();
        job = PrismSplit::Store::get().start (f, 0, 0, base.getChildFile ("Separated").getChildFile (label), label, true, 0.0, studio || six, six ? 6 : 4);
        listen = RefListen::Store::get().start (f); keyTempo = {};
        refreshButtons(); repaint();
    }

    bool isDone() const        { return loaded; }
    bool isWorking() const     { return job != nullptr && ! job->finished; }
    juce::String errorText() const { return job != nullptr ? job->error : juce::String(); }
    double lengthSeconds() const   { return lanes.empty() ? 0.0 : lanes.front()->thumb.getTotalLength(); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        namespace L = Console::Look;
        g.setGradientFill (juce::ColourGradient (L::bodyTop(), 0.0f, 0.0f, L::bodyBot(), 0.0f, (float) getHeight(), false)); g.fillAll();
        g.setColour (L::text()); g.setFont (L::caps (15.0f));
        g.drawText ("VOCAL REMOVER", header.withTrimmedLeft (18), juce::Justification::centredLeft);
        if (keyTempo.isNotEmpty()) { g.setColour (L::cyan()); g.setFont (L::caps (12.5f)); g.drawText (keyTempo, header.withTrimmedLeft (176).withWidth (230), juce::Justification::centredLeft); }
        g.setColour (L::dim()); g.setFont (Theme::font (12.0f));
        g.drawText (statusLine(), header.withTrimmedLeft (410).withTrimmedRight (installBtn.isVisible() ? 250 : 16), juce::Justification::centredRight);

        if (job == nullptr)
        {
            auto dz = dropZone.toFloat();
            L::drawWell (g, dz, 14.0f);
            g.setColour ((hover ? L::cyan() : L::faint()).withAlpha (hover ? 0.9f : 0.8f));
            juce::Path p; p.addRoundedRectangle (dz.reduced (10.0f), 12.0f);
            const float dashes[] = { 8.0f, 6.0f }; juce::Path dashed; juce::PathStrokeType (1.6f).createDashedStroke (dashed, p, dashes, 2); g.fillPath (dashed);
            g.setColour (L::text()); g.setFont (L::caps (18.0f));
            g.drawText ("DROP A SONG OR A VIDEO HERE", dropZone.withTrimmedBottom (dropZone.getHeight() / 2 + 6), juce::Justification::centredBottom);
            g.setColour (L::dim()); g.setFont (Theme::font (13.0f));
            g.drawText ("You get two tracks back: the music without the voice, and the voice alone.", dropZone.withTrimmedTop (dropZone.getHeight() / 2 + 46), juce::Justification::centredTop);
            g.drawText ("WAV, AIFF, FLAC, MP3, OGG, or a video (MP4, MOV). Nothing leaves your computer.", dropZone.withTrimmedTop (dropZone.getHeight() / 2 + 66), juce::Justification::centredTop);
            return;
        }
        if (! loaded)
        {
            auto bar = progressBar.toFloat();
            L::drawWell (g, bar, 6.0f);
            const float pr = job->progress.load();
            g.setColour (L::cyan()); g.fillRoundedRectangle (bar.reduced (2.0f).withWidth (juce::jmax (4.0f, (bar.getWidth() - 4.0f) * pr)), 4.0f);
            g.setColour (job->error.isNotEmpty() ? L::red() : L::text()); g.setFont (Theme::font (14.0f));
            g.drawFittedText (job->error.isNotEmpty() ? job->error : "Separating " + song.getFileName() + "  -  " + juce::String (juce::roundToInt (pr * 100)) + " %   (" + (studio ? "Studio quality" : "Draft quality") + ")",
                              progressBar.translated (0, -34).withHeight (26), juce::Justification::centred, 2);
            return;
        }
        for (auto& l : lanes) drawLane (g, *l);
        if (lanes.empty()) return;
        // playhead
        const double len = juce::jmax (0.001, lengthSeconds()), pos = lanes.front()->src.getCurrentPosition();
        const int x = waveX (lanes.front()->area) + juce::roundToInt ((float) (pos / len) * (float) waveW (lanes.front()->area));
        g.setColour (L::text()); g.fillRect (x, lanes.front()->area.getY() + 4, 2, lanes.back()->area.getBottom() - lanes.front()->area.getY() - 8);
        g.setColour (L::dim()); g.setFont (Theme::font (12.0f));
        g.drawText (timeText (pos) + " / " + timeText (len), timeArea, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        header = r.removeFromTop (48);
        auto foot = r.removeFromBottom (58).reduced (16, 12);
        r.reduce (16, 6);
        dropZone = r.reduced (0, 6);
        chooseBtn.setBounds (dropZone.getCentreX() - 90, dropZone.getCentreY() + 4, 180, 34);
        progressBar = r.withSizeKeepingCentre (r.getWidth() - 80, 16);
        lanesArea = r;
        layoutLanes();
        playBtn.setBounds (foot.removeFromLeft (110)); foot.removeFromLeft (10);
        timeArea = foot.removeFromLeft (110);
        newBtn.setBounds (foot.removeFromRight (100)); foot.removeFromRight (8);
        sendBtn.setBounds (foot.removeFromRight (140)); foot.removeFromRight (8);
        saveBtn.setBounds (foot.removeFromRight (90)); foot.removeFromRight (8);
        modeBtn.setBounds (foot.removeFromRight (110));
        installBtn.setBounds (header.getRight() - 236, header.getY() + 10, 220, 28);
    }

    void mouseDown (const juce::MouseEvent& e) override { scrub (e); }
    void mouseDrag (const juce::MouseEvent& e) override { scrub (e); }

private:
    static juce::String timeText (double s) { const int t = (int) s; return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2); }
    static int waveX (juce::Rectangle<int> lane) { return lane.getX() + 214; }
    static int waveW (juce::Rectangle<int> lane) { return lane.getWidth() - 226; }

    juce::String statusLine() const
    {
        auto& vi = Frame::installState();
        if (vi.running) return "Getting the video tools (once): " + juce::String (vi.megabytes.load()) + " of " + juce::String (Frame::packItem().megabytes) + " MB  -  " + juce::String (juce::roundToInt (vi.progress.load() * 100)) + " %"
                               + (pendingVideo.existsAsFile() ? "  -  " + pendingVideo.getFileName() + " starts by itself after" : juce::String());
        auto& st = PrismSplit::StudioPack::state();
        if (st.running) return "Installing Studio quality: " + st.stage + "  " + juce::String (juce::roundToInt (st.progress.load() * 100)) + " %";
        if (st.finished && st.error.isNotEmpty()) return "Install failed: " + st.error;
        return PrismSplit::StudioPack::installed() ? "Studio quality (neural separator)" : "Draft quality - install Studio quality for clean results";
    }

    void layoutLanes()
    {
        if (lanes.empty()) return;
        auto r = lanesArea; const int gap = 8, n = (int) lanes.size(), h = (r.getHeight() - gap * (n - 1)) / n;
        for (auto& l : lanes) { l->area = r.removeFromTop (h); r.removeFromTop (gap); l->vol.setBounds (l->area.getX() + 12, l->area.getBottom() - 30, 190, 22); }
    }

    void drawLane (juce::Graphics& g, Lane& l)
    {
        namespace L = Console::Look;
        const float gain = (float) l.vol.getValue() / 100.0f; const auto lane = l.area;
        L::drawChromePanel (g, lane.toFloat(), 10.0f, 0.10f, l.colour);
        g.setColour (l.colour); g.setFont (L::caps (14.0f));
        g.drawText (l.name, lane.getX() + 14, lane.getY() + 8, 180, 20, juce::Justification::centredLeft);
        if (lane.getHeight() > 84) { g.setColour (L::dim()); g.setFont (Theme::font (11.5f)); g.drawText (l.note, lane.getX() + 14, lane.getY() + 28, 190, 18, juce::Justification::centredLeft); }
        juce::Rectangle<int> w (waveX (lane), lane.getY() + 8, waveW (lane), lane.getHeight() - 16);
        L::drawWell (g, w.toFloat(), 6.0f);
        g.setColour (l.colour.withAlpha (0.25f + 0.75f * gain));
        if (l.thumb.getTotalLength() > 0) l.thumb.drawChannels (g, w.reduced (2), 0.0, l.thumb.getTotalLength(), juce::jmax (0.05f, gain));
    }

    void scrub (const juce::MouseEvent& e)
    {
        if (! loaded || lanes.empty()) return;
        const auto area = lanes.front()->area.getUnion (lanes.back()->area);
        if (! area.contains (e.getPosition()) || e.x < waveX (area)) return;
        const double t = juce::jlimit (0.0, 1.0, (double) (e.x - waveX (area)) / juce::jmax (1, waveW (area))) * lengthSeconds();
        for (auto& l : lanes) l->src.setPosition (t);
        repaint();
    }

    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a song", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.mp4;*.mov;*.m4v;*.mkv;*.webm;*.avi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) start (fc.getResult()); });
    }

    void saveMenu()
    {
        if (! loaded) return;
        juce::PopupMenu m; int id = 1;
        for (auto& l : lanes) m.addItem (id++, "Save " + l->name.toLowerCase() + "...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&saveBtn), [this] (int r)
        {
            if (r <= 0 || r > (int) lanes.size()) return;
            const auto src = lanes[(size_t) (r - 1)]->file;
            chooser = std::make_unique<juce::FileChooser> ("Save", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (src.getFileName()), "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [src] (const juce::FileChooser& fc) { auto d = fc.getResult(); if (d != juce::File()) src.copyFileTo (d.withFileExtension ("wav")); });
        });
    }

    void clearLanes()
    {
        mixer.removeAllInputs();
        for (auto& l : lanes) { l->src.setSource (nullptr); l->thumb.removeChangeListener (this); removeChildComponent (&l->vol); }
        lanes.clear();
    }

    void reset()
    {
        stopAudio(); clearLanes();
        job.reset(); listen.reset(); keyTempo = {}; loaded = false; song = juce::File();
        refreshButtons(); repaint();
    }

    void loadResult()
    {
        namespace L = Console::Look;
        const double keepPos = lanes.empty() ? 0.0 : lanes.front()->src.getCurrentPosition();
        stopAudio(); clearLanes();
        struct Def { const char* name; const char* note; juce::Colour c; juce::File f; };
        std::vector<Def> defs;
        if (stemsMode)
        {
            static const char* const names[6] = { "VOCALS", "DRUMS", "BASS", "OTHER", "GUITAR", "PIANO" };
            static const char* const notes[6] = { "the lead voice", "kick, snare, hats, percussion", "bass and 808", "synths, strings, horns, the rest", "guitars", "piano and keys" };
            const juce::Colour cols[6] = { L::pink(), L::orange(), L::green(), L::cyan(), L::yellow(), juce::Colour (0xffb08cff) };
            for (int i = 0; i < (int) job->stems.size() && i < 6; ++i) defs.push_back ({ names[i], notes[i], cols[i], job->stems[(size_t) i] });
        }
        else defs = { { "MUSIC", "everything but the lead voice", L::cyan(), job->instrumental }, { "VOCAL", "the lead voice alone", L::pink(), job->acapella } };
        for (auto& d : defs)
        {
            auto l = std::make_unique<Lane> (formats, thumbCache);
            l->name = d.name; l->note = d.note; l->colour = d.c; l->file = d.f;
            if (auto* rd = formats.createReaderFor (d.f)) { const double sr = rd->sampleRate; l->reader = std::make_unique<juce::AudioFormatReaderSource> (rd, true); l->src.setSource (l->reader.get(), 0, nullptr, sr, 2); }
            l->thumb.setSource (new juce::FileInputSource (d.f)); l->thumb.addChangeListener (this);
            l->vol.setSliderStyle (juce::Slider::LinearHorizontal); l->vol.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
            l->vol.setRange (0.0, 100.0, 1.0); l->vol.setValue (100.0, juce::dontSendNotification); l->vol.setTextValueSuffix (" %");
            l->vol.setColour (juce::Slider::trackColourId, d.c); l->vol.setColour (juce::Slider::thumbColourId, L::text());
            l->vol.setColour (juce::Slider::textBoxTextColourId, L::text()); l->vol.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            l->vol.setTooltip (juce::String (d.name).toLowerCase() + " volume");
            l->vol.onValueChange = [this] { applyGains(); };
            addAndMakeVisible (l->vol); l->src.setPosition (keepPos);
            mixer.addInputSource (&l->src, false);
            lanes.push_back (std::move (l));
        }
        loaded = true; layoutLanes(); applyGains(); refreshButtons(); repaint();
    }

    void applyGains() { for (auto& l : lanes) l->src.setGain ((float) l->vol.getValue() / 100.0f); repaint(); }

    void togglePlay()
    {
        if (! loaded || lanes.empty()) return;
        if (playing) stopAudio();
        else
        {
            auto& first = lanes.front()->src;
            if (first.getCurrentPosition() >= first.getLengthInSeconds() - 0.05) for (auto& l : lanes) l->src.setPosition (0);
            for (auto& l : lanes) l->src.setPosition (first.getCurrentPosition());
            session.getEngine().getDeviceManager().deviceManager.addAudioCallback (&player);
            for (auto& l : lanes) l->src.start();
            playing = true;
        }
        refreshButtons();
    }

    void stopAudio()
    {
        if (! playing) return;
        for (auto& l : lanes) l->src.stop();
        session.getEngine().getDeviceManager().deviceManager.removeAudioCallback (&player);
        playing = false;
    }

    void refreshButtons()
    {
        const bool idle = job == nullptr;
        chooseBtn.setVisible (idle);
        for (auto* c : std::initializer_list<juce::Component*> { &playBtn, &saveBtn, &sendBtn }) c->setVisible (loaded);
        modeBtn.setVisible (job != nullptr); modeBtn.setButtonText (stemsMode ? "2 tracks" : (job != nullptr && job->numStems == 6 ? "6 stems" : "4 stems")); sendBtn.setButtonText (stemsMode ? "Send to session" : "Send to session");
        newBtn.setVisible (! idle);
        installBtn.setVisible (! (PrismSplit::StudioPack::installed() && PrismSplit::StudioPack::installed6()) && ! PrismSplit::StudioPack::state().running);
        installBtn.setButtonText (PrismSplit::StudioPack::installed() ? "Add 6 stems (130 MB)" : "Install Studio quality");
        playBtn.setButtonText (playing ? "Stop" : "Play");
    }

    void timerCallback() override
    {
        if (pendingVideo != juce::File() && ! Frame::installState().running && Frame::installState().finished)
        {
            const auto v = pendingVideo; pendingVideo = juce::File();
            if (Frame::installState().error.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Vocal Remover", Frame::installState().error);
            else start (v);
        }
        if (job != nullptr && job->finished && ! loaded && job->error.isEmpty()) loadResult();
        if (playing && ! lanes.empty() && ! lanes.front()->src.isPlaying()) { stopAudio(); refreshButtons(); }
        if (listen != nullptr && listen->finished && keyTempo.isEmpty()) { keyTempo = listen->result.ok ? juce::String (juce::roundToInt (listen->result.bpm)) + " BPM  -  " + listen->result.keyName : juce::String (" "); repaint(); }
        refreshButtons();
        if (isWorking() || playing || PrismSplit::StudioPack::state().running || Frame::installState().running) repaint();
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    Session& session;
    juce::AudioFormatManager formats; juce::AudioThumbnailCache thumbCache;
    std::vector<std::unique_ptr<Lane>> lanes;
    juce::MixerAudioSource mixer; juce::AudioSourcePlayer player;
    juce::TextButton chooseBtn { "Choose a song" }, playBtn { "Play" }, saveBtn { "Save..." }, sendBtn { "Send to session" },
                     installBtn { "Install Studio quality (165 MB)" }, newBtn { "New song" }, modeBtn { "4 stems" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File pendingVideo;
    std::shared_ptr<PrismSplit::Job> job; std::shared_ptr<RefListen::Job> listen; juce::String keyTempo; juce::File song;
    bool loaded = false, playing = false, hover = false, studio = false, stemsMode = false;
    juce::Rectangle<int> header, dropZone, progressBar, lanesArea, timeArea;
};
