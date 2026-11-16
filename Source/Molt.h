#pragma once
// Molt - the AudioSuite time & pitch window (Squelchmoth's X-Form-class tool) and its clip rendering.
//
// AudioSuite > Molt (Time & Pitch)... works on the selected audio clips:
//  * AUDIO     Polyphonic / Monophonic / Poly (Faster), input gain 0 to -6 dB with a clip light
//  * TIME      original start, end and length; processed end and length (editable); original and processed tempo;
//              Shift % (any number of decimals - pull-up / pull-down); units Bars|Beats, Min:Sec, Timecode, Samples;
//              Range 2x / 4x / 8x (which also limits pitch and formant)
//  * TRANSIENT Sensitivity (polyphonic) or Window (monophonic)
//  * PITCH     transpose +/-36 st with cents, Formant In with its own +/-36 st shift
//  * Clip by clip, or Multi-input: every selected clip processed as one phase-coherent multichannel stream
//  * Full-quality preview, presets (pull-up / pull-down and musical), before / after waveform
//  * Render writes new files (the originals are kept) and is undoable.

#include "Theme.h"
#include "MoltDsp.h"

namespace Molt
{
    struct ClipAudio { te::Clip::Ptr clip; juce::AudioBuffer<float> audio; double sr = 44100; double start = 0, length = 0; };

    /** Reads what an audio clip plays from its source file (its offset and length). */
    inline bool readClip (te::AudioClipBase& ac, ClipAudio& out, juce::String& err)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        auto file = ac.getAudioFile().getFile();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        if (r == nullptr) { err = "Can't read " + file.getFileName(); return false; }
        const auto pos = ac.getPosition();
        const double speed = juce::jmax (0.01, ac.getSpeedRatio());
        const auto s0 = (juce::int64) std::llround (pos.getOffset().inSeconds() * speed * r->sampleRate);
        const auto n = (juce::int64) std::llround (pos.getLength().inSeconds() * speed * r->sampleRate);
        if (n <= 0 || s0 >= r->lengthInSamples) { err = "Clip '" + ac.getName() + "' has no audio"; return false; }
        out.clip = &ac; out.sr = r->sampleRate; out.start = pos.getStart().inSeconds(); out.length = pos.getLength().inSeconds();
        out.audio.setSize ((int) juce::jmax (1u, r->numChannels), (int) juce::jmin (n, r->lengthInSamples - s0));
        r->read (&out.audio, 0, out.audio.getNumSamples(), s0, true, true);
        return true;
    }

    inline juce::File outputFolder (te::Edit& edit)
    {
        auto f = edit.editFileRetriever ? edit.editFileRetriever() : juce::File();
        auto dir = f.existsAsFile() || f.getParentDirectory().isDirectory() ? f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension() + " Molt")
                                                                            : juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Squelchmoth Molt");
        dir.createDirectory();
        return dir;
    }

    inline juce::File writeWav (const juce::File& dir, const juce::String& base, const juce::AudioBuffer<float>& b, double sr)
    {
        auto f = dir.getNonexistentChildFile (juce::File::createLegalFileName (base), ".wav", false);
        juce::WavAudioFormat wav;
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned int) b.getNumChannels(), 32, {}, 0)))
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        return f;
    }

    /** Process and replace clips. multi = one phase-coherent stream for all of them. Returns a summary. */
    inline juce::String render (te::Edit& edit, const juce::Array<te::Clip::Ptr>& clips, const MoltDsp::Params& p, bool multi, float& peak, juce::String& err)
    {
        std::vector<ClipAudio> in;
        for (auto& c : clips)
            if (auto ac = dynamic_cast<te::AudioClipBase*> (c.get()))
            {
                ClipAudio ca;
                if (! readClip (*ac, ca, err)) return {};
                in.push_back (std::move (ca));
            }
        if (in.empty()) { err = "Select one or more audio clips first"; return {}; }
        auto dir = outputFolder (edit);
        auto& um = edit.getUndoManager();
        um.beginNewTransaction ("Molt (Time & Pitch)");
        peak = 0;
        auto replace = [&] (ClipAudio& ca, const juce::AudioBuffer<float>& outBuf, double newStart)
        {
            auto ac = dynamic_cast<te::AudioClipBase*> (ca.clip.get());
            auto f = writeWav (dir, ca.clip->getName() + " molt", outBuf, ca.sr);
            ac->setAutoTempo (false);
            ac->setSpeedRatio (1.0);
            ac->disableLooping();
            ac->getSourceFileReference().setToFile (f, te::SourceFileReference::PathStyle::chooseBest, false);
            const double newLen = outBuf.getNumSamples() / ca.sr;
            ac->setPosition ({ te::TimeRange (te::TimePosition::fromSeconds (newStart), te::TimeDuration::fromSeconds (newLen)), te::TimeDuration() });
        };
        if (multi && in.size() > 1)
        {
            bool sameRate = true; for (auto& ca : in) sameRate = sameRate && std::abs (ca.sr - in.front().sr) < 1;
            if (! sameRate) { err = "Multi-input needs every clip at the same sample rate"; return {}; }
            const double sr = in.front().sr;
            double r0 = 1e18, r1 = -1e18;
            for (auto& ca : in) { r0 = juce::jmin (r0, ca.start); r1 = juce::jmax (r1, ca.start + ca.length); }
            int total = 0; for (auto& ca : in) total += ca.audio.getNumChannels();
            juce::AudioBuffer<float> big (total, (int) std::ceil ((r1 - r0) * sr) + 1); big.clear();
            int ch = 0;
            for (auto& ca : in)
            {
                const int off = (int) std::llround ((ca.start - r0) * sr);
                for (int c = 0; c < ca.audio.getNumChannels(); ++c) big.copyFrom (ch + c, off, ca.audio, c, 0, juce::jmin (ca.audio.getNumSamples(), big.getNumSamples() - off));
                ch += ca.audio.getNumChannels();
            }
            float pk = 0;
            auto out = MoltDsp::process (big, sr, p, &pk);
            peak = pk;
            ch = 0;
            for (auto& ca : in)
            {
                const int off = (int) std::llround ((ca.start - r0) * sr * p.timeRatio);
                const int len = juce::jmin ((int) std::llround (ca.audio.getNumSamples() * p.timeRatio), out.getNumSamples() - off);
                juce::AudioBuffer<float> part (ca.audio.getNumChannels(), juce::jmax (1, len));
                for (int c = 0; c < ca.audio.getNumChannels(); ++c) part.copyFrom (c, 0, out, ch + c, off, part.getNumSamples());
                replace (ca, part, r0 + (ca.start - r0) * p.timeRatio);
                ch += ca.audio.getNumChannels();
            }
            return "Molt rendered " + juce::String ((int) in.size()) + " clips as one phase-coherent " + juce::String (total) + "-channel stream";
        }
        for (auto& ca : in)
        {
            float pk = 0;
            auto out = MoltDsp::process (ca.audio, ca.sr, p, &pk);
            peak = juce::jmax (peak, pk);
            replace (ca, out, ca.start);
        }
        return "Molt rendered " + juce::String ((int) in.size()) + " clip" + (in.size() == 1 ? "" : "s");
    }

    //==========================================================================
    /** Plays a buffer through the audio device (alongside the session) for Preview. */
    class Previewer : private juce::AudioIODeviceCallback
    {
    public:
        explicit Previewer (juce::AudioDeviceManager& d) : dm (d) {}
        ~Previewer() override { stop(); }
        void play (const juce::AudioBuffer<float>& b, double sr)
        {
            stop();
            const juce::ScopedLock sl (lock);
            buffer = b; srcRate = sr; pos = 0; playing = true;
            dm.addAudioCallback (this);
        }
        void stop()   { dm.removeAudioCallback (this); const juce::ScopedLock sl (lock); playing = false; }
        bool isPlaying() const { return playing; }
        double progress() const { return buffer.getNumSamples() > 0 ? pos / buffer.getNumSamples() : 0.0; }
    private:
        juce::AudioDeviceManager& dm;
        juce::CriticalSection lock;
        juce::AudioBuffer<float> buffer; double srcRate = 44100, devRate = 44100, pos = 0; std::atomic<bool> playing { false };
        void audioDeviceAboutToStart (juce::AudioIODevice* d) override { devRate = d->getCurrentSampleRate(); }
        void audioDeviceStopped() override {}
        void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outs, int numOut, int n, const juce::AudioIODeviceCallbackContext&) override
        {
            for (int c = 0; c < numOut; ++c) if (outs[c] != nullptr) juce::FloatVectorOperations::clear (outs[c], n);
            const juce::ScopedTryLock sl (lock);
            if (! sl.isLocked() || ! playing) return;
            const double step = srcRate / juce::jmax (1.0, devRate);
            for (int i = 0; i < n; ++i)
            {
                const int i0 = (int) pos;
                if (i0 + 1 >= buffer.getNumSamples()) { playing = false; break; }
                const float f = (float) (pos - i0);
                for (int c = 0; c < numOut; ++c)
                {
                    if (outs[c] == nullptr) continue;
                    const int sc = juce::jmin (c, buffer.getNumChannels() - 1);
                    outs[c][i] = buffer.getSample (sc, i0) + (buffer.getSample (sc, i0 + 1) - buffer.getSample (sc, i0)) * f;
                }
                pos += step;
            }
        }
    };

    //==========================================================================
    class Panel : public juce::Component, private juce::Timer
    {
    public:
        Panel (te::Edit& e, juce::AudioDeviceManager& dm, std::function<juce::Array<te::Clip::Ptr>()> sel)
            : edit (e), getSelection (std::move (sel)), previewer (dm)
        {
            auto combo = [this] (juce::ComboBox& c, juce::StringArray items, int def)
            {
                c.addItemList (items, 1); c.setSelectedItemIndex (def, juce::dontSendNotification); addAndMakeVisible (c);
                c.onChange = [this] { refreshFromControls(); };
            };
            combo (typeBox, MoltDsp::typeNames(), 0);
            combo (unitsBox, { "Bars|Beats", "Min:Sec", "Timecode", "Samples" }, 1);
            combo (modeBox, { "Clip by clip", "Multi-input (phase-coherent)" }, 0);
            presetBox.setTextWhenNothingSelected ("Presets...");
            for (int i = 0; i < (int) MoltDsp::presets().size(); ++i) presetBox.addItem (MoltDsp::presets()[(size_t) i].name, i + 1);
            presetBox.onChange = [this] { applyPreset (presetBox.getSelectedItemIndex()); presetBox.setSelectedId (0, juce::dontSendNotification); };
            addAndMakeVisible (presetBox);
            for (auto* b : { &range2, &range4, &range8 }) { b->setClickingTogglesState (true); b->setRadioGroupId (77); addAndMakeVisible (*b); b->onClick = [this] { applyRange(); }; }
            range2.setToggleState (true, juce::dontSendNotification);
            auto slider = [this] (juce::Slider& s, double lo, double hi, double step, double def, const juce::String& suffix)
            {
                s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 86, 22);
                s.setRange (lo, hi, step); s.setValue (def, juce::dontSendNotification); s.setTextValueSuffix (suffix);
                s.setColour (juce::Slider::trackColourId, accent); s.setColour (juce::Slider::thumbColourId, accent.brighter (0.4f));
                s.onValueChange = [this] { refreshFromControls(); };
                addAndMakeVisible (s);
            };
            slider (gain, -6.0, 0.0, 0.1, 0.0, " dB");
            slider (sens, 0.0, 100.0, 1.0, 50.0, " %");
            slider (window, 10.0, 100.0, 1.0, 25.0, " ms");
            slider (transpose, -12.0, 12.0, 0.01, 0.0, " st");
            slider (formant, -12.0, 12.0, 0.01, 0.0, " st");
            formantIn.onClick = [this] { refreshFromControls(); }; addAndMakeVisible (formantIn);
            auto editor = [this] (juce::TextEditor& t, bool editable)
            {
                t.setReadOnly (! editable); t.setJustification (juce::Justification::centredRight);
                t.setColour (juce::TextEditor::backgroundColourId, editable ? juce::Colour (0xff1a1030) : Theme::panel());
                t.setColour (juce::TextEditor::textColourId, editable ? juce::Colours::white : Theme::dim());
                t.setColour (juce::TextEditor::outlineColourId, editable ? accent.withAlpha (0.6f) : Theme::line());
                t.setFont (Theme::font (13.0f, editable));
                addAndMakeVisible (t);
            };
            for (auto* t : { &oStart, &oEnd, &oLen }) editor (*t, false);
            for (auto* t : { &pEnd, &pLen, &oBpm, &pBpm, &shift }) editor (*t, true);
            pLen.onReturnKey = [this] { setRatio (parseDur (pLen.getText()) / juce::jmax (1e-9, origLen)); };
            pLen.onFocusLost = pLen.onReturnKey;
            pEnd.onReturnKey = [this] { setRatio ((parseTime (pEnd.getText()) - origStart) / juce::jmax (1e-9, origLen)); };
            pEnd.onFocusLost = pEnd.onReturnKey;
            shift.onReturnKey = [this] { setRatio (shift.getText().retainCharacters ("0123456789.").getDoubleValue() / 100.0); };
            shift.onFocusLost = shift.onReturnKey;
            pBpm.onReturnKey = [this] { const double o = oBpm.getText().getDoubleValue(), n = pBpm.getText().getDoubleValue(); if (o > 0 && n > 0) setRatio (o / n); };
            pBpm.onFocusLost = pBpm.onReturnKey;
            oBpm.onReturnKey = [this] { refreshFields(); };
            for (auto* b : { &previewBtn, &renderBtn, &reselectBtn }) { addAndMakeVisible (*b); b->setColour (juce::TextButton::buttonColourId, Theme::panel2()); }
            renderBtn.setColour (juce::TextButton::buttonColourId, accent.darker (0.5f));
            previewBtn.onClick = [this] { preview(); };
            renderBtn.onClick = [this] { doRender(); };
            reselectBtn.onClick = [this] { loadSelection(); };
            loadSelection();
            setSize (980, 690);
            startTimerHz (20);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::bg());
            auto r = getLocalBounds();
            auto head = r.removeFromTop (54);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.55f), 0, 0, juce::Colour (0xff3ad1ff).withAlpha (0.35f), (float) getWidth(), 54, false));
            g.fillRect (head);
            g.setColour (juce::Colours::white); g.setFont (Theme::font (22.0f, true));
            g.drawText ("MOLT", head.reduced (14, 0).withHeight (30).withY (4), juce::Justification::centredLeft);
            g.setFont (Theme::font (11.0f));
            g.drawText ("Time & Pitch  -  AudioSuite  -  " + selectionText, head.reduced (14, 0).withHeight (16).withY (34), juce::Justification::centredLeft);
            for (auto& [title, box] : sections)
            {
                g.setColour (Theme::panel().withAlpha (0.85f)); g.fillRoundedRectangle (box.toFloat(), 6.0f);
                g.setColour (accent.withAlpha (0.45f)); g.drawRoundedRectangle (box.toFloat(), 6.0f, 1.0f);
                g.setColour (accent.brighter (0.3f)); g.setFont (Theme::font (10.0f, true));
                g.drawText (title, box.reduced (10, 6), juce::Justification::topLeft);
            }
            g.setFont (Theme::font (10.0f)); g.setColour (Theme::dim());
            for (auto& [text, rect] : labels) g.drawText (text, rect, juce::Justification::centredLeft);
            // clip light
            g.setColour (lastPeak > 0.999f ? juce::Colour (0xffff3a5a) : juce::Colour (0xff1c3a24));
            g.fillEllipse (clipLed.toFloat());
            g.setColour (Theme::dim()); g.drawText ("CLIP", clipLed.translated (16, 0).withWidth (40), juce::Justification::centredLeft);
            // waveforms
            auto w = waveArea.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (w, 6.0f);
            drawWave (g, w.withTrimmedBottom (w.getHeight() * 0.5f).reduced (8, 4), before, juce::Colours::white.withAlpha (0.5f), "ORIGINAL");
            drawWave (g, w.withTrimmedTop (w.getHeight() * 0.5f).reduced (8, 4), after, accent, after.getNumSamples() > 0 ? "PROCESSED (preview)" : "PROCESSED - press PREVIEW");
            if (previewer.isPlaying())
            {
                const float x = w.getX() + 8 + (w.getWidth() - 16) * (float) previewer.progress();
                g.setColour (Theme::amber()); g.drawLine (x, w.getCentreY(), x, w.getBottom() - 4, 1.5f);
            }
            g.setColour (Theme::dim()); g.setFont (Theme::font (10.0f));
            g.drawText (status, getLocalBounds().removeFromBottom (20).reduced (12, 0), juce::Justification::centredLeft);
        }

        void resized() override
        {
            sections.clear(); labels.clear();
            auto r = getLocalBounds().reduced (10);
            r.removeFromTop (52);
            auto top = r.removeFromTop (34);
            modeBox.setBounds (top.removeFromLeft (240).reduced (0, 4));
            top.removeFromLeft (8);
            presetBox.setBounds (top.removeFromLeft (300).reduced (0, 4));
            reselectBtn.setBounds (top.removeFromRight (170).reduced (0, 4));
            r.removeFromTop (6);
            auto row = r.removeFromTop (236);
            auto audioBox = row.removeFromLeft (230); row.removeFromLeft (8);
            auto timeBox = row.removeFromLeft (440); row.removeFromLeft (8);
            auto transBox = row;
            sections.push_back ({ "AUDIO", audioBox }); sections.push_back ({ "TIME", timeBox }); sections.push_back ({ "TRANSIENT", transBox });
            auto a = audioBox.reduced (10).withTrimmedTop (18);
            labels.push_back ({ "Type", a.removeFromTop (16) }); typeBox.setBounds (a.removeFromTop (26)); a.removeFromTop (10);
            labels.push_back ({ "Gain", a.removeFromTop (16) }); gain.setBounds (a.removeFromTop (26)); a.removeFromTop (10);
            clipLed = a.removeFromTop (12).withWidth (12);
            auto t = timeBox.reduced (10).withTrimmedTop (18);
            auto unitsRow = t.removeFromTop (26);
            labels.push_back ({ "Units", unitsRow.removeFromLeft (44) }); unitsBox.setBounds (unitsRow.removeFromLeft (130));
            unitsRow.removeFromLeft (16); labels.push_back ({ "Range", unitsRow.removeFromLeft (44) });
            for (auto* b : { &range2, &range4, &range8 }) { b->setBounds (unitsRow.removeFromLeft (44)); unitsRow.removeFromLeft (4); }
            t.removeFromTop (8);
            auto grid = t;
            const int cw = grid.getWidth() / 4;
            auto cell = [&] (int col, int rowI) { return juce::Rectangle<int> (grid.getX() + col * cw, grid.getY() + rowI * 30, cw - 6, 26); };
            labels.push_back ({ "", {} });
            labels.push_back ({ "Start", cell (1, 0) }); labels.push_back ({ "End", cell (2, 0) }); labels.push_back ({ "Length", cell (3, 0) });
            labels.push_back ({ "Original", cell (0, 1) }); oStart.setBounds (cell (1, 1)); oEnd.setBounds (cell (2, 1)); oLen.setBounds (cell (3, 1));
            labels.push_back ({ "Processed", cell (0, 2) }); pEnd.setBounds (cell (2, 2)); pLen.setBounds (cell (3, 2));
            labels.push_back ({ "Tempo  orig / new", cell (0, 3) }); oBpm.setBounds (cell (1, 3)); pBpm.setBounds (cell (2, 3));
            labels.push_back ({ "Shift %", cell (0, 4) }); shift.setBounds (cell (1, 4));
            auto tr = transBox.reduced (10).withTrimmedTop (18);
            labels.push_back ({ "Sensitivity (polyphonic)", tr.removeFromTop (16) }); sens.setBounds (tr.removeFromTop (26)); tr.removeFromTop (10);
            labels.push_back ({ "Window (monophonic)", tr.removeFromTop (16) }); window.setBounds (tr.removeFromTop (26));
            r.removeFromTop (8);
            auto pitchBox = r.removeFromTop (110);
            sections.push_back ({ "PITCH", pitchBox });
            auto p = pitchBox.reduced (10).withTrimmedTop (18);
            auto prow = p.removeFromTop (30);
            labels.push_back ({ "Transpose", prow.removeFromLeft (80) }); transpose.setBounds (prow.removeFromLeft (360));
            prow.removeFromLeft (20); formantIn.setBounds (prow.removeFromLeft (110));
            labels.push_back ({ "Formant", prow.removeFromLeft (64) }); formant.setBounds (prow);
            p.removeFromTop (4);
            labels.push_back ({ "Transpose shows semitones with cents as decimals (2.50 st = 2 st 50 ct). Formant In keeps the voice's character; its slider moves the formants on their own.", p.removeFromTop (20) });
            r.removeFromTop (8);
            auto buttons = r.removeFromBottom (36);
            renderBtn.setBounds (buttons.removeFromRight (160)); buttons.removeFromRight (8);
            previewBtn.setBounds (buttons.removeFromRight (120));
            r.removeFromBottom (8);
            waveArea = r.withTrimmedBottom (14);
        }

    private:
        te::Edit& edit;
        std::function<juce::Array<te::Clip::Ptr>()> getSelection;
        Previewer previewer;
        const juce::Colour accent { 0xffff8a1f };
        juce::ComboBox typeBox, unitsBox, modeBox, presetBox;
        juce::TextButton range2 { "2x" }, range4 { "4x" }, range8 { "8x" }, previewBtn { "PREVIEW" }, renderBtn { "RENDER" }, reselectBtn { "USE SELECTION" };
        juce::Slider gain, sens, window, transpose, formant;
        juce::ToggleButton formantIn { "Formant In" };
        juce::TextEditor oStart, oEnd, oLen, pEnd, pLen, oBpm, pBpm, shift;
        std::vector<std::pair<juce::String, juce::Rectangle<int>>> sections, labels;
        juce::Rectangle<int> clipLed, waveArea;
        juce::Array<te::Clip::Ptr> clips;
        juce::String selectionText, status;
        double ratio = 1.0, origStart = 0, origLen = 1;
        juce::AudioBuffer<float> before, after;
        float lastPeak = 0;

        MoltDsp::Params params() const
        {
            MoltDsp::Params p;
            p.timeRatio = ratio; p.semitones = transpose.getValue(); p.formantIn = formantIn.getToggleState(); p.formantSt = formant.getValue();
            p.type = typeBox.getSelectedItemIndex(); p.sensitivity = sens.getValue(); p.windowMs = window.getValue(); p.gainDb = gain.getValue();
            return p;
        }
        double rangeFactor() const   { return range8.getToggleState() ? 8.0 : range4.getToggleState() ? 4.0 : 2.0; }
        void applyRange()
        {
            const double rf = rangeFactor(), st = 12.0 * std::log2 (rf);
            transpose.setRange (-st, st, 0.01); formant.setRange (-st, st, 0.01);
            setRatio (ratio);
        }
        void setRatio (double r)
        {
            const double rf = rangeFactor();
            ratio = juce::jlimit (1.0 / rf, rf, std::isfinite (r) && r > 0 ? r : 1.0);
            after.setSize (0, 0);
            refreshFields();
        }
        void loadSelection()
        {
            clips.clear();
            for (auto& c : getSelection()) if (dynamic_cast<te::AudioClipBase*> (c.get()) != nullptr) clips.add (c);
            if (clips.isEmpty()) { selectionText = "no audio clips selected - select clips in the Edit window, then USE SELECTION"; origStart = 0; origLen = 1; }
            else
            {
                double s0 = 1e18, s1 = -1e18;
                for (auto& c : clips) { s0 = juce::jmin (s0, c->getPosition().getStart().inSeconds()); s1 = juce::jmax (s1, c->getPosition().getEnd().inSeconds()); }
                origStart = s0; origLen = s1 - s0;
                selectionText = juce::String (clips.size()) + " clip" + (clips.size() == 1 ? "" : "s") + ": " + clips.getFirst()->getName() + (clips.size() > 1 ? " ..." : "");
                ClipAudio ca; juce::String err;
                if (auto ac = dynamic_cast<te::AudioClipBase*> (clips.getFirst().get())) if (readClip (*ac, ca, err)) before = ca.audio;
            }
            if (oBpm.getText().isEmpty()) oBpm.setText (juce::String (edit.tempoSequence.getTempo (0)->getBpm(), 3), false);
            after.setSize (0, 0);
            refreshFields(); repaint();
        }
        void refreshFromControls() { after.setSize (0, 0); repaint(); }
        void refreshFields()
        {
            oStart.setText (fmtTime (origStart), false); oEnd.setText (fmtTime (origStart + origLen), false); oLen.setText (fmtDur (origLen), false);
            pEnd.setText (fmtTime (origStart + origLen * ratio), false); pLen.setText (fmtDur (origLen * ratio), false);
            const double ob = oBpm.getText().getDoubleValue();
            pBpm.setText (ob > 0 ? juce::String (ob / ratio, 3) : juce::String(), false);
            shift.setText (juce::String (ratio * 100.0, 2), false);
            repaint();
        }
        void applyPreset (int i)
        {
            if (! juce::isPositiveAndBelow (i, (int) MoltDsp::presets().size())) return;
            const auto& p = MoltDsp::presets()[(size_t) i];
            if (std::abs (p.semitones) > 12 || p.timePct > 200 || p.timePct < 50) { range4.setToggleState (true, juce::sendNotification); applyRange(); }
            transpose.setValue (p.semitones, juce::dontSendNotification); formantIn.setToggleState (p.formantIn, juce::dontSendNotification);
            formant.setValue (p.formantSt, juce::dontSendNotification);
            setRatio (p.timePct / 100.0);
            status = "Preset: " + juce::String (p.name);
        }
        // ---- time text in the chosen units
        juce::String fmtTime (double sec) const
        {
            switch (unitsBox.getSelectedItemIndex())
            {
                case 0: { auto bb = edit.tempoSequence.toBarsAndBeats (te::TimePosition::fromSeconds (sec));
                          return juce::String (bb.bars + 1) + "|" + juce::String ((int) bb.getWholeBeats() + 1) + "|" + juce::String ((int) (bb.getFractionalBeats().inBeats() * 960)).paddedLeft ('0', 3); }
                case 2: { const int fps = 30; const int total = (int) std::floor (sec * fps); return juce::String::formatted ("%02d:%02d:%02d:%02d", total / (3600 * fps), (total / (60 * fps)) % 60, (total / fps) % 60, total % fps); }
                case 3: return juce::String ((juce::int64) std::llround (sec * edit.engine.getDeviceManager().getSampleRate()));
                default: return juce::String::formatted ("%d:%06.3f", (int) (sec / 60), std::fmod (sec, 60.0));
            }
        }
        juce::String fmtDur (double sec) const
        {
            if (unitsBox.getSelectedItemIndex() == 0)
            {
                const double beats = edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (origStart + sec)).inBeats() - edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (origStart)).inBeats();
                const int bpb = juce::jmax (1, edit.tempoSequence.getTimeSig (0)->numerator.get());
                const int bars = (int) (beats / bpb); const double rem = beats - bars * bpb;
                return juce::String (bars) + "|" + juce::String ((int) rem) + "|" + juce::String ((int) ((rem - (int) rem) * 960)).paddedLeft ('0', 3);
            }
            if (unitsBox.getSelectedItemIndex() == 2) return fmtTime (sec);
            return fmtTime (sec);
        }
        double parseTime (const juce::String& t) const   { return parseAbs (t); }
        double parseAbs (const juce::String& t) const
        {
            switch (unitsBox.getSelectedItemIndex())
            {
                case 0: { auto p = juce::StringArray::fromTokens (t, "|", ""); const int bpb = juce::jmax (1, edit.tempoSequence.getTimeSig (0)->numerator.get());
                          const double beats = (p[0].getIntValue() - 1) * bpb + (p[1].getIntValue() - 1) + p[2].getIntValue() / 960.0;
                          return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (beats)).inSeconds(); }
                default: return parseDur (t);
            }
        }
        double parseDur (const juce::String& t) const
        {
            switch (unitsBox.getSelectedItemIndex())
            {
                case 0: { auto p = juce::StringArray::fromTokens (t, "|", ""); const int bpb = juce::jmax (1, edit.tempoSequence.getTimeSig (0)->numerator.get());
                          const double beats = p[0].getIntValue() * bpb + p[1].getIntValue() + p[2].getIntValue() / 960.0;
                          const double b0 = edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (origStart)).inBeats();
                          return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b0 + beats)).inSeconds() - origStart; }
                case 2: { auto p = juce::StringArray::fromTokens (t, ":", ""); return p[0].getIntValue() * 3600.0 + p[1].getIntValue() * 60.0 + p[2].getIntValue() + p[3].getIntValue() / 30.0; }
                case 3: return t.getDoubleValue() / juce::jmax (1.0, edit.engine.getDeviceManager().getSampleRate());
                default: { if (t.contains (":")) return t.upToFirstOccurrenceOf (":", false, false).getIntValue() * 60.0 + t.fromFirstOccurrenceOf (":", false, false).getDoubleValue(); return t.getDoubleValue(); }
            }
        }

        void drawWave (juce::Graphics& g, juce::Rectangle<float> r, const juce::AudioBuffer<float>& b, juce::Colour c, const juce::String& label)
        {
            g.setColour (c.withAlpha (0.8f)); g.setFont (Theme::font (9.0f, true)); g.drawText (label, r.removeFromTop (12.0f), juce::Justification::left);
            if (b.getNumSamples() <= 0) return;
            const double maxLen = juce::jmax ((double) before.getNumSamples() * ratio, (double) b.getNumSamples());
            const float wscale = (float) (b.getNumSamples() / maxLen);
            auto rr = r.withWidth (r.getWidth() * wscale);
            const int cols = (int) rr.getWidth();
            const float norm = 0.95f / juce::jmax (0.02f, b.getMagnitude (0, b.getNumSamples()));
            juce::Path p;
            for (int x = 0; x < cols; ++x)
            {
                const int s0 = (int) ((double) x / cols * b.getNumSamples()), s1 = juce::jmax (s0 + 1, (int) ((double) (x + 1) / cols * b.getNumSamples()));
                auto mm = b.findMinMax (0, s0, juce::jmin (s1, b.getNumSamples()) - s0);
                const float y0 = rr.getCentreY() - mm.getEnd() * norm * rr.getHeight() * 0.48f, y1 = rr.getCentreY() - mm.getStart() * norm * rr.getHeight() * 0.48f;
                p.addRectangle (rr.getX() + (float) x, y0, 1.0f, juce::jmax (1.0f, y1 - y0));
            }
            g.setGradientFill (juce::ColourGradient (c.brighter (0.4f), rr.getX(), rr.getY(), c.darker (0.3f), rr.getX(), rr.getBottom(), false));
            g.fillPath (p);
        }

        void preview()
        {
            if (previewer.isPlaying()) { previewer.stop(); previewBtn.setButtonText ("PREVIEW"); return; }
            if (before.getNumSamples() == 0) { status = "Nothing to preview - select an audio clip"; repaint(); return; }
            ClipAudio ca; juce::String err;
            auto ac = dynamic_cast<te::AudioClipBase*> (clips.getFirst().get());
            if (ac == nullptr || ! readClip (*ac, ca, err)) { status = err; repaint(); return; }
            const int maxIn = (int) (ca.sr * 20.0);   // first 20 s at full quality
            if (ca.audio.getNumSamples() > maxIn) ca.audio.setSize (ca.audio.getNumChannels(), maxIn, true);
            float pk = 0;
            after = MoltDsp::process (ca.audio, ca.sr, params(), &pk);
            lastPeak = pk;
            previewer.play (after, ca.sr);
            previewBtn.setButtonText ("STOP");
            status = "Previewing at full quality (" + MoltDsp::typeNames()[params().type] + ")" + (pk > 0.999f ? " - CLIPPING: lower Gain" : "");
            repaint();
        }
        void doRender()
        {
            previewer.stop(); previewBtn.setButtonText ("PREVIEW");
            if (clips.isEmpty()) { status = "Select audio clips, then USE SELECTION"; repaint(); return; }
            juce::String err; float pk = 0;
            auto msg = render (edit, clips, params(), modeBox.getSelectedItemIndex() == 1, pk, err);
            lastPeak = pk;
            status = msg.isNotEmpty() ? msg + (pk > 0.999f ? "  (clipped - Undo and lower Gain)" : "") + ". Undo restores the originals." : err;
            loadSelection();
        }
        void timerCallback() override
        {
            if (previewer.isPlaying()) repaint();
            else if (previewBtn.getButtonText() == "STOP") { previewBtn.setButtonText ("PREVIEW"); repaint(); }
            const bool mono = typeBox.getSelectedItemIndex() == 1;
            sens.setEnabled (! mono); window.setEnabled (mono); formant.setEnabled (formantIn.getToggleState());
        }
    };

    class Window : public juce::DocumentWindow
    {
    public:
        Window (te::Edit& e, juce::AudioDeviceManager& dm, std::function<juce::Array<te::Clip::Ptr>()> sel)
            : DocumentWindow ("Molt - Time & Pitch (AudioSuite)", Theme::bg(), DocumentWindow::closeButton | DocumentWindow::minimiseButton), panel (e, dm, std::move (sel))
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (&panel, true);
            setResizable (true, false);
            centreWithSize (panel.getWidth(), panel.getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { setVisible (false); if (onClose) onClose(); }
        std::function<void()> onClose;
    private:
        Panel panel;
    };
}
