// Included inside namespace Skins (PluginSkins.h): faces for Chrysalis 2 / Chrysalis SF, Mandible / Mandible Lite and Pollen.

    /** Handles backed by lambdas (for ValueTree-stored zone / slice properties that are not plug-in parameters). */
    struct LocalHandles
    {
        std::vector<std::unique_ptr<Handle>> store; HandleMap map;
        void clear()   { map.clear(); store.clear(); }
        Handle& add (const juce::String& name, std::function<float()> get, std::function<void (float)> set, std::function<juce::String()> text, float defNorm = 0.0f)
        {
            auto h = std::make_unique<Handle>(); h->name = name; h->label = name; h->get = std::move (get); h->set = std::move (set); h->text = std::move (text); h->defaultNorm = defNorm;
            map[name] = h.get(); store.push_back (std::move (h)); return *store.back();
        }
        /** A numeric property on a ValueTree with a natural range. */
        Handle& prop (const juce::String& name, juce::ValueTree tree, const juce::Identifier& id, float lo, float hi, float def, juce::UndoManager* um, const juce::String& unit = {}, int decimals = 0, bool isInt = false)
        {
            auto get = [tree, id, lo, hi, def] { return juce::jlimit (0.0f, 1.0f, ((float) (double) tree.getProperty (id, (double) def) - lo) / (hi - lo)); };
            auto set = [tree, id, lo, hi, um, isInt] (float v) mutable { const float nat = lo + (hi - lo) * v; tree.setProperty (id, isInt ? juce::var ((int) std::lround (nat)) : juce::var ((double) nat), um); };
            auto text = [tree, id, def, unit, decimals] { return juce::String ((double) tree.getProperty (id, (double) def), decimals) + unit; };
            return add (name, get, set, text, (def - lo) / (hi - lo));
        }
    };

    /** Waveform of a sample with optional markers; click reports a sample position. */
    struct WaveView : public juce::Component
    {
        std::shared_ptr<SqSmp::Sample> sample; juce::Colour accent = juce::Colours::white;
        std::vector<int> markers; int selected = -1; int loopStart = -1, loopEnd = -1; float playhead = -1.0f;
        std::function<void (int sampleIndex, bool dbl)> onClick;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r, 3.0f);
            if (sample == nullptr) { g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f)); g.drawText ("drop a file here, or Load...", getLocalBounds(), juce::Justification::centred); return; }
            const int len = sample->buffer.getNumSamples(); if (len < 2) return;
            // selected region shading
            if (selected >= 0 && selected < (int) markers.size())
            {
                const float x0 = r.getX() + r.getWidth() * (float) markers[(size_t) selected] / (float) len, x1 = selected + 1 < (int) markers.size() ? r.getX() + r.getWidth() * (float) markers[(size_t) selected + 1] / (float) len : r.getRight();
                g.setColour (accent.withAlpha (0.12f)); g.fillRect (x0, r.getY(), x1 - x0, r.getHeight());
            }
            if (loopEnd > loopStart && loopStart >= 0) { g.setColour (Theme::amber().withAlpha (0.1f)); g.fillRect (r.getX() + r.getWidth() * (float) loopStart / (float) len, r.getY(), r.getWidth() * (float) (loopEnd - loopStart) / (float) len, r.getHeight()); }
            const int w = juce::jmax (1, (int) r.getWidth()); const int nch = sample->buffer.getNumChannels();
            juce::Path p; const float mid = r.getCentreY(), half = r.getHeight() * 0.48f;
            for (int x = 0; x < w; ++x)
            {
                const int a = (int) ((juce::int64) len * x / w), b = juce::jmax (a + 1, (int) ((juce::int64) len * (x + 1) / w));
                float mn = 0, mx = 0;
                for (int c = 0; c < nch; ++c) { auto rg = sample->buffer.findMinMax (c, a, b - a); mn = juce::jmin (mn, rg.getStart()); mx = juce::jmax (mx, rg.getEnd()); }
                p.addRectangle (r.getX() + (float) x, mid - mx * half, 1.0f, juce::jmax (1.0f, (mx - mn) * half));
            }
            g.setColour (accent.withAlpha (0.85f)); g.fillPath (p);
            for (size_t i = 0; i < markers.size(); ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) markers[i] / (float) len;
                g.setColour ((int) i == selected ? Theme::text() : accent.withAlpha (0.7f)); g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                g.setFont (Theme::font (7.5f)); g.drawText (juce::String (i + 1), (int) x + 2, (int) r.getY() + 1, 20, 10, juce::Justification::centredLeft);
            }
            if (loopEnd > loopStart && loopStart >= 0) { g.setColour (Theme::amber()); g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) loopStart / (float) len), r.getY(), r.getBottom()); g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) loopEnd / (float) len), r.getY(), r.getBottom()); }
            if (playhead >= 0.0f) { g.setColour (Theme::text()); g.drawVerticalLine ((int) (r.getX() + r.getWidth() * playhead), r.getY(), r.getBottom()); }
        }
        int sampleAt (float x) const   { return sample == nullptr ? 0 : (int) ((double) sample->buffer.getNumSamples() * juce::jlimit (0.0f, 1.0f, x / (float) getWidth())); }
        void mouseDown (const juce::MouseEvent& e) override   { if (onClick) onClick (sampleAt (e.position.x), false); }
        void mouseDoubleClick (const juce::MouseEvent& e) override   { if (onClick) onClick (sampleAt (e.position.x), true); }
    };

    //==========================================================================
    struct Chrysalis2Viz : public Viz, public juce::FileDragAndDropTarget, public juce::ListBoxModel
    {
        Chrysalis2Viz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c)
        {
            sp = dynamic_cast<SqSynth::SqChrysalis2Plugin*> (&plugin);
            setInterceptsMouseClicks (false, true);
            list.setModel (this); list.setRowHeight (18); list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack); addAndMakeVisible (list);
            addAndMakeVisible (wave); wave.accent = c;
            strip = std::make_unique<HandleStrip> (zoneHandles.map, c); strip->rowHeight = 19; strip->onChange = [this] { refreshWave(); repaint(); }; addAndMakeVisible (*strip);
            auto mk = [this] (juce::TextButton& b, const char* t, std::function<void()> f) { b.setButtonText (t); b.onClick = std::move (f); addAndMakeVisible (b); };
            mk (loadBtn, "LOAD FOLDER...", [this] { chooseFolder(); });
            mk (importBtn, "IMPORT SFZ / SF2...", [this] { chooseBank(); });
            mk (presetBtn, "BANK PRESET...", [this] { choosePreset(); });
            mk (clearBtn, "CLEAR", [this] { if (sp != nullptr) { sp->clearZones(); select (-1); } });
            mk (removeBtn, "REMOVE ZONE", [this] { if (sp != nullptr && selected >= 0) { sp->removeZone (selected); select (juce::jmin (selected, sp->numZones() - 1)); } });
            select (sp != nullptr && sp->numZones() > 0 ? 0 : -1);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            auto left = r.removeFromLeft (r.getWidth() * 0.34f).reduced (8, 14).withTrimmedTop (4);
            auto btns = left.removeFromBottom (48); const int bw = btns.getWidth() / 3;
            auto row1 = btns.removeFromTop (22), row2 = btns.removeFromBottom (22);
            loadBtn.setBounds (row1.removeFromLeft (bw).reduced (2, 0)); importBtn.setBounds (row1.removeFromLeft (bw).reduced (2, 0)); presetBtn.setBounds (row1.reduced (2, 0));
            removeBtn.setBounds (row2.removeFromLeft (bw).reduced (2, 0)); clearBtn.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
            list.setBounds (left.withTrimmedBottom (4));
            r.removeFromLeft (8);
            auto mid = r.removeFromLeft (r.getWidth() * 0.55f);
            auto top = mid.removeFromTop (mid.getHeight() * 0.42f);
            wave.setBounds (top.reduced (8, 14).withTrimmedTop (4));
            keyMapArea = mid.reduced (8, 14).withTrimmedTop (4);
            r.removeFromLeft (8);
            strip->setBounds (r.reduced (8, 14).withTrimmedTop (4));
        }
        void select (int i)
        {
            selected = i; zoneHandles.clear();
            if (sp != nullptr && juce::isPositiveAndBelow (i, sp->numZones()))
            {
                auto z = sp->zone (i); auto* um = &plugin.edit.getUndoManager();
                zoneHandles.prop ("Root", z, "root", 0, 127, 60, um, "", 0, true).text = [z] { return juce::MidiMessage::getMidiNoteName ((int) z.getProperty ("root", 60), true, true, 3); };
                zoneHandles.prop ("Low Key", z, "loKey", 0, 127, 0, um, "", 0, true).text = [z] { return juce::MidiMessage::getMidiNoteName ((int) z.getProperty ("loKey", 0), true, true, 3); };
                zoneHandles.prop ("High Key", z, "hiKey", 0, 127, 127, um, "", 0, true).text = [z] { return juce::MidiMessage::getMidiNoteName ((int) z.getProperty ("hiKey", 127), true, true, 3); };
                zoneHandles.prop ("Low Vel", z, "loVel", 1, 127, 1, um, "", 0, true); zoneHandles.prop ("High Vel", z, "hiVel", 1, 127, 127, um, "", 0, true);
                zoneHandles.prop ("Tune", z, "tune", -1200, 1200, 0, um, " ct", 0, true).bipolar = true;
                zoneHandles.prop ("Gain", z, "gain", -24, 24, 0, um, " dB", 1).bipolar = true;
                zoneHandles.prop ("Pan", z, "pan", -1, 1, 0, um, "", 2).bipolar = true;
                auto& lp = zoneHandles.prop ("Loop", z, "loopMode", 0, 1, 0, um, "", 0, true); lp.toggle = true;
                const int len = sp->sampleAt (i) != nullptr ? sp->sampleAt (i)->buffer.getNumSamples() : 1;
                zoneHandles.prop ("Loop Start", z, "loopStart", 0, (float) len, 0, um, "", 0, true); zoneHandles.prop ("Loop End", z, "loopEnd", 0, (float) len, (float) len, um, "", 0, true);
                zoneHandles.prop ("Crossfade", z, "xfade", 0, 4096, 0, um, "", 0, true);
                zoneHandles.prop ("Round Robin", z, "rr", 0, 8, 0, um, "", 0, true).text = [z] { const int v = (int) z.getProperty ("rr", 0); return v == 0 ? juce::String ("all") : "group " + juce::String (v); };
                strip->setRows ({ "Root", "Low Key", "High Key", "Low Vel", "High Vel", "Tune", "Gain", "Pan", "Loop", "Loop Start", "Loop End", "Crossfade", "Round Robin" });
            }
            else strip->setRows ({});
            list.updateContent(); list.selectRow (juce::jmax (0, i), juce::dontSendNotification); refreshWave(); repaint();
        }
        void refreshWave()
        {
            if (sp == nullptr || ! juce::isPositiveAndBelow (selected, sp->numZones())) { wave.sample = nullptr; wave.repaint(); return; }
            auto z = sp->zone (selected); wave.sample = sp->sampleAt (selected); wave.markers.clear();
            wave.loopStart = (int) z.getProperty ("loopMode", 0) > 0 ? (int) z.getProperty ("loopStart", -1) : -1; wave.loopEnd = (int) z.getProperty ("loopEnd", -1); wave.repaint();
        }
        // ListBoxModel
        int getNumRows() override   { return sp != nullptr ? sp->numZones() : 0; }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int hgt, bool sel) override
        {
            if (sp == nullptr || row >= sp->numZones()) return;
            auto z = sp->zone (row);
            if (sel) { g.setColour (accent.withAlpha (0.2f)); g.fillRect (0, 0, w, hgt); }
            g.setColour (Theme::text()); g.setFont (Theme::font (8.5f, sel));
            g.drawFittedText (juce::String (row + 1) + "  " + z.getProperty ("name").toString(), 4, 0, w * 0.6f, hgt, juce::Justification::centredLeft, 1);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f));
            g.drawText (juce::MidiMessage::getMidiNoteName ((int) z.getProperty ("loKey", 0), true, true, 3) + "-" + juce::MidiMessage::getMidiNoteName ((int) z.getProperty ("hiKey", 127), true, true, 3) + "  v" + juce::String ((int) z.getProperty ("loVel", 1)) + "-" + juce::String ((int) z.getProperty ("hiVel", 127)), 0, 0, w - 4, hgt, juce::Justification::centredRight);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override   { select (row); }
        // drops
        bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) { juce::File ff (f); if (ff.isDirectory() || ff.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;sfz;sf2")) return true; } return false; }
        void filesDropped (const juce::StringArray& files, int, int) override
        {
            if (sp == nullptr) return;
            juce::Array<juce::File> audio;
            for (auto& f : files)
            {
                juce::File ff (f);
                if (ff.hasFileExtension ("sfz")) { sp->importSfz (ff); select (0); return; }
                if (ff.hasFileExtension ("sf2")) { sp->importSf2 (ff, 0); select (0); return; }
                if (ff.isDirectory()) ff.findChildFiles (audio, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"); else audio.add (ff);
            }
            if (! audio.isEmpty()) { sp->addFilesAutoMapped (audio); select (sp->numZones() - 1); }
        }
        void chooseFolder()
        {
            auto fc = std::make_shared<juce::FileChooser> ("Choose a folder of samples (or files)", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [this, fc] (const juce::FileChooser& c)
            {
                juce::Array<juce::File> audio; for (auto& f : c.getResults()) { if (f.isDirectory()) f.findChildFiles (audio, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"); else audio.add (f); }
                if (sp != nullptr && ! audio.isEmpty()) { sp->addFilesAutoMapped (audio); select (sp->numZones() - 1); }
            });
        }
        void chooseBank()
        {
            auto fc = std::make_shared<juce::FileChooser> ("Choose an SFZ or SF2 bank", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.sfz;*.sf2");
            fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc] (const juce::FileChooser& c)
            {
                auto f = c.getResult(); if (sp == nullptr || ! f.existsAsFile()) return;
                if (f.hasFileExtension ("sfz")) sp->importSfz (f); else sp->importSf2 (f, 0);
                select (0);
            });
        }
        void choosePreset()
        {
            if (sp == nullptr) return;
            juce::File f (sp->state.getProperty ("bankFile").toString());
            if (! f.hasFileExtension ("sf2")) { juce::PopupMenu m; m.addItem (1, "Import an .sf2 bank first", false); m.showMenuAsync ({}); return; }
            auto ps = SqSynth::SqChrysalis2Plugin::sf2Presets (f); juce::PopupMenu m;
            for (auto& p : ps) m.addItem (p.presetIndex + 1, juce::String (p.bank).paddedLeft ('0', 3) + ":" + juce::String (p.program).paddedLeft ('0', 3) + "  " + p.name, true, (int) sp->state.getProperty ("bankPreset", 0) == p.presetIndex);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this, f] (int r) { if (r > 0 && sp != nullptr) { sp->importSf2 (f, r - 1); select (0); } });
        }
        void paint (juce::Graphics& g) override
        {
            if (sp == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto box = [&] (juce::Rectangle<float> a, const juce::String& t) { g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (a, 4.0f); g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true)); g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft); };
            auto left = r.removeFromLeft (r.getWidth() * 0.34f); r.removeFromLeft (8.0f);
            box (left, "ZONES  " + juce::String (sp->numZones()) + (sp->bankName.get().isNotEmpty() ? "   " + sp->bankName.get() : juce::String()) + "   (drop files, a folder, an .sfz or .sf2)");
            auto mid = r.removeFromLeft (r.getWidth() * 0.55f); r.removeFromLeft (8.0f);
            auto top = mid.removeFromTop (mid.getHeight() * 0.42f);
            box (top, selected >= 0 && selected < sp->numZones() ? "ZONE " + juce::String (selected + 1) + "  " + sp->zone (selected).getProperty ("name").toString() : "WAVEFORM");
            box (mid, "KEY MAP  C-1 - G9   zones by key range, stacked by velocity");
            {
                auto in = keyMapArea.toFloat(); const float kw = in.getWidth() / 128.0f;
                // keyboard strip at the bottom
                auto kb = in.removeFromBottom (14.0f);
                for (int n = 0; n < 128; ++n) { const bool black = juce::MidiMessage::isMidiNoteBlack (n); g.setColour (black ? juce::Colours::black.withAlpha (0.8f) : Theme::text().withAlpha (0.35f)); g.fillRect (kb.getX() + kw * (float) n, kb.getY(), juce::jmax (1.0f, kw - 0.5f), kb.getHeight()); if (n % 12 == 0) { g.setColour (Theme::dim()); g.setFont (Theme::font (7.0f)); g.drawText ("C" + juce::String (n / 12 - 1), (int) (kb.getX() + kw * (float) n), (int) kb.getY() - 10, 24, 10, juce::Justification::centredLeft); } }
                in.removeFromBottom (12.0f);
                for (int i = 0; i < sp->numZones(); ++i)
                {
                    auto z = sp->zone (i);
                    const int lo = (int) z.getProperty ("loKey", 0), hi = (int) z.getProperty ("hiKey", 127), lv = (int) z.getProperty ("loVel", 1), hv = (int) z.getProperty ("hiVel", 127), root = (int) z.getProperty ("root", 60);
                    auto cell = juce::Rectangle<float> (in.getX() + kw * (float) lo, in.getBottom() - in.getHeight() * (float) hv / 127.0f, kw * (float) (hi - lo + 1), in.getHeight() * (float) (hv - lv + 1) / 127.0f).reduced (0.5f);
                    const bool sel = i == selected;
                    g.setColour (accent.withAlpha (sel ? 0.75f : 0.35f)); g.fillRoundedRectangle (cell, 2.0f);
                    g.setColour (sel ? Theme::text() : accent); g.drawRoundedRectangle (cell, 2.0f, sel ? 1.5f : 0.8f);
                    g.setColour (Theme::text()); g.fillRect (in.getX() + kw * (float) root, cell.getY(), juce::jmax (1.5f, kw), cell.getHeight());
                }
                if (synth() != nullptr && synth()->lastNote() >= 0 && synth()->displayLevel() > 0.02f) { g.setColour (Theme::text().withAlpha (0.8f)); g.fillRect (kb.getX() + kw * (float) synth()->lastNote(), kb.getY(), juce::jmax (1.5f, kw), kb.getHeight()); }
            }
            box (r, "ZONE");
            if (sp->numZones() != lastCount) { lastCount = sp->numZones(); select (juce::jlimit (-1, lastCount - 1, selected)); }
        }
        SqSynth::SynthCommon* synth()   { return dynamic_cast<SqSynth::SynthCommon*> (&plugin); }
        SqSynth::SqChrysalis2Plugin* sp = nullptr; juce::ListBox list; WaveView wave; LocalHandles zoneHandles; std::unique_ptr<HandleStrip> strip;
        juce::TextButton loadBtn, importBtn, presetBtn, clearBtn, removeBtn; juce::Rectangle<int> keyMapArea; int selected = -1, lastCount = -1;
    };

    //==========================================================================
    struct MandibleViz : public Viz, public juce::FileDragAndDropTarget
    {
        MandibleViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c)
        {
            mp = dynamic_cast<SqSynth::SqMandiblePlugin*> (&plugin);
            setInterceptsMouseClicks (false, true);
            addAndMakeVisible (wave); wave.accent = c;
            wave.onClick = [this] (int smp, bool dbl) { if (mp == nullptr) return; int hit = -1; for (int i = 0; i < mp->numSlices (deck); ++i) if (smp >= (int) mp->slice (deck, i).getProperty ("start")) hit = i; selectSlice (hit); if (dbl && hit >= 0) mp->triggerSlice (deck, hit); else if (hit >= 0 && ! dbl) mp->triggerSlice (deck, hit); };
            strip = std::make_unique<HandleStrip> (sliceHandles.map, c); strip->rowHeight = 22; strip->onChange = [this] { repaint(); }; addAndMakeVisible (*strip);
            auto mk = [this] (juce::TextButton& b, const char* t, std::function<void()> f) { b.setButtonText (t); b.onClick = std::move (f); addAndMakeVisible (b); };
            mk (loadBtn, "LOAD...", [this] { chooseFile(); });
            mk (modeBtn, "SLICE: TRANSIENTS", [this] { chooseMode(); });
            mk (dumpBtn, "DUMP TO PIANO ROLL", [this] { if (Hooks::slicerDump && mp != nullptr) Hooks::slicerDump (plugin, deck); });
            mk (chanBtn, "TO RACK CHANNELS", [this] { if (Hooks::slicerToChannels && mp != nullptr) Hooks::slicerToChannels (plugin, deck); });
            mk (deckABtn, "DECK A", [this] { setDeck (0); }); mk (deckBBtn, "DECK B", [this] { setDeck (1); });
            sens.setRange (0.0, 1.0, 0.01); sens.setValue (0.5, juce::dontSendNotification); sens.setSliderStyle (juce::Slider::LinearHorizontal); sens.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            sens.onDragEnd = [this] { if (mp != nullptr && (int) mp->decks[(size_t) deck].getProperty ("sliceMode", 0) == 0) { mp->reslice (deck, 0, (float) sens.getValue()); refresh(); } };
            addAndMakeVisible (sens);
            if (mp != nullptr && mp->isLite) { deckABtn.setVisible (false); deckBBtn.setVisible (false); chanBtn.setVisible (false); }
            setDeck (0);
        }
        void setDeck (int d)   { deck = d; deckABtn.setToggleState (d == 0, juce::dontSendNotification); deckBBtn.setToggleState (d == 1, juce::dontSendNotification); selectSlice (-1); refresh(); }
        void refresh()
        {
            if (mp == nullptr) return;
            wave.sample = mp->deckSample (deck); wave.markers.clear();
            for (int i = 0; i < mp->numSlices (deck); ++i) wave.markers.push_back ((int) mp->slice (deck, i).getProperty ("start"));
            wave.selected = selected; wave.repaint();
            modeBtn.setButtonText ("SLICE: " + SqSynth::SqMandiblePlugin::sliceModeNames()[juce::jlimit (0, 6, (int) mp->decks[(size_t) deck].getProperty ("sliceMode", 0))]);
            sens.setValue ((double) mp->decks[(size_t) deck].getProperty ("sensitivity", 0.5), juce::dontSendNotification);
            repaint();
        }
        void selectSlice (int i)
        {
            selected = i; sliceHandles.clear();
            if (mp != nullptr && juce::isPositiveAndBelow (i, mp->numSlices (deck)))
            {
                auto s = mp->slice (deck, i); auto* um = &plugin.edit.getUndoManager();
                sliceHandles.prop ("Pitch", s, "pitch", -24, 24, 0, um, " st", 1).bipolar = true;
                sliceHandles.prop ("Reverse", s, "reverse", 0, 1, 0, um, "", 0, true).toggle = true;
                sliceHandles.prop ("Gain", s, "gain", -24, 12, 0, um, " dB", 1).bipolar = true;
                auto& c = sliceHandles.prop ("Cutoff", s, "cutoff", 40, 20000, 20000, um, " Hz", 0); c.get = [s] { return juce::jlimit (0.0f, 1.0f, (float) (std::log ((double) s.getProperty ("cutoff", 20000.0) / 40.0) / std::log (500.0))); }; c.set = [s, um] (float v) mutable { s.setProperty ("cutoff", 40.0 * std::pow (500.0, (double) v), um); }; c.defaultNorm = 1.0f;
                sliceHandles.prop ("Decay", s, "decay", 0.01f, 10, 10, um, " s", 2).defaultNorm = 1.0f;
                sliceHandles.prop ("Mute", s, "mute", 0, 1, 0, um, "", 0, true).toggle = true;
                strip->setRows ({ "Pitch", "Reverse", "Gain", "Cutoff", "Decay", "Mute" });
            }
            else strip->setRows ({});
            wave.selected = i; wave.repaint();
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            auto top = r.removeFromTop (28).reduced (8, 0);
            deckABtn.setBounds (top.removeFromLeft (70)); deckBBtn.setBounds (top.removeFromLeft (70)); top.removeFromLeft (8);
            loadBtn.setBounds (top.removeFromLeft (80)); top.removeFromLeft (4); modeBtn.setBounds (top.removeFromLeft (150)); top.removeFromLeft (4); sens.setBounds (top.removeFromLeft (120)); top.removeFromLeft (8);
            chanBtn.setBounds (top.removeFromRight (140)); top.removeFromRight (4); dumpBtn.setBounds (top.removeFromRight (150));
            r.removeFromTop (4);
            auto right = r.removeFromRight (juce::jmin (260, r.getWidth() / 3));
            strip->setBounds (right.reduced (8, 14).withTrimmedTop (4));
            wave.setBounds (r.reduced (8, 14).withTrimmedTop (4));
        }
        void paint (juce::Graphics& g) override
        {
            if (mp == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f); r.removeFromTop (32.0f);
            auto box = [&] (juce::Rectangle<float> a, const juce::String& t) { g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (a, 4.0f); g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true)); g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft); };
            auto right = r.removeFromRight (juce::jmin (260.0f, r.getWidth() / 3.0f));
            const juce::String deckName = mp->isLite ? juce::String() : (deck == 0 ? "DECK A  " : "DECK B  ");
            auto f = mp->deckFile (deck);
            box (r, deckName + (f.existsAsFile() ? f.getFileName() + "   " + juce::String (mp->numSlices (deck)) + " slices   " + juce::String (mp->deckBpm (deck), 1) + " BPM   " + juce::String ((int) mp->decks[(size_t) deck].getProperty ("bars", 0)) + " bars   keys from " + juce::MidiMessage::getMidiNoteName (SqSynth::SqMandiblePlugin::deckBaseNote[deck], true, true, 3) : "drop a loop on the waveform, or Load..."));
            box (right, selected >= 0 ? "SLICE " + juce::String (selected + 1) + "   " + juce::MidiMessage::getMidiNoteName (SqSynth::SqMandiblePlugin::deckBaseNote[deck] + selected, true, true, 3) : "SLICE  (click one in the waveform)");
            const int ls = mp->lastSlice.load(); if (ls != lastShown && mp->lastDeck.load() == deck) { lastShown = ls; if (ls >= 0 && ls != selected) selectSlice (ls); }
            if (sliceCount != mp->numSlices (deck)) { sliceCount = mp->numSlices (deck); refresh(); }
        }
        bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) return true; return false; }
        void filesDropped (const juce::StringArray& files, int, int) override   { if (mp != nullptr) for (auto& f : files) if (juce::File (f).existsAsFile()) { mp->loadFile (deck, juce::File (f), (int) mp->decks[(size_t) deck].getProperty ("sliceMode", 0), (float) sens.getValue()); refresh(); return; } }
        void chooseFile()
        {
            auto fc = std::make_shared<juce::FileChooser> ("Choose a loop", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc] (const juce::FileChooser& c) { auto f = c.getResult(); if (mp != nullptr && f.existsAsFile()) { mp->loadFile (deck, f, (int) mp->decks[(size_t) deck].getProperty ("sliceMode", 0), (float) sens.getValue()); refresh(); } });
        }
        void chooseMode()
        {
            juce::PopupMenu m; auto names = SqSynth::SqMandiblePlugin::sliceModeNames();
            for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i]);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modeBtn), [this] (int r) { if (r > 0 && mp != nullptr) { mp->reslice (deck, r - 1, (float) sens.getValue()); selectSlice (-1); refresh(); } });
        }
        SqSynth::SqMandiblePlugin* mp = nullptr; int deck = 0, selected = -1, lastShown = -1, sliceCount = -1; WaveView wave; LocalHandles sliceHandles; std::unique_ptr<HandleStrip> strip;
        juce::TextButton loadBtn, modeBtn, dumpBtn, chanBtn, deckABtn, deckBBtn; juce::Slider sens;
    };

    //==========================================================================
    /** Pollen: pack cards with procedural covers, the pack's presets, four named macros. */
    struct PollenViz : public Viz, public juce::ListBoxModel
    {
        PollenViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c)
        {
            po = dynamic_cast<SqSynth::SqPollenPlugin*> (&plugin); sig = Signature::findForPlugin (plugin);
            setInterceptsMouseClicks (true, true);   // hitTest keeps the clicks to the pack cards
            list.setModel (this); list.setRowHeight (20); list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack); addAndMakeVisible (list);
            rebuildPresets();
        }
        std::function<void()> onChange;
        void rebuildPresets()
        {
            presets.clear();
            if (sig != nullptr && po != nullptr) for (int i = 0; i < (int) sig->presets.size(); ++i) { juce::String n (sig->presets[(size_t) i].name); if (n.upToFirstOccurrenceOf (" | ", false, false).equalsIgnoreCase (po->pack.get())) presets.add (i); }
            list.updateContent(); list.repaint();
        }
        int getNumRows() override   { return presets.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int hgt, bool sel) override
        {
            if (sig == nullptr || row >= presets.size()) return;
            juce::String n (sig->presets[(size_t) presets[row]].name); n = n.fromFirstOccurrenceOf (" | ", false, false);
            const bool current = po != nullptr && po->presetName.get() == n;
            if (sel || current) { g.setColour (accent.withAlpha (current ? 0.3f : 0.15f)); g.fillRect (0, 0, w, hgt); }
            g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, current)); g.drawFittedText (n, 6, 0, w - 8, hgt, juce::Justification::centredLeft, 1);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (sig == nullptr || po == nullptr || row >= presets.size()) return;
            Signature::applyPreset (plugin, sig->presets[(size_t) presets[row]]);
            po->presetName = juce::String (sig->presets[(size_t) presets[row]].name).fromFirstOccurrenceOf (" | ", false, false);
            if (onChange) onChange(); repaint();
        }
        juce::Rectangle<float> cardsArea() const   { auto r = getLocalBounds().toFloat().reduced (6.0f); return r.removeFromLeft (r.getWidth() * 0.5f).reduced (8.0f, 16.0f).withTrimmedTop (2.0f); }
        juce::Rectangle<float> cardRect (int i) const
        {
            auto a = cardsArea(); const int cols = 4, rows = 3; const float cw = a.getWidth() / cols, ch = a.getHeight() / rows;
            return juce::Rectangle<float> (a.getX() + cw * (float) (i % cols), a.getY() + ch * (float) (i / cols), cw, ch).reduced (3.0f);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            r.removeFromLeft (r.getWidth() / 2); r.removeFromLeft (8);
            auto mid = r.removeFromLeft (r.getWidth() * 0.55f);
            list.setBounds (mid.reduced (8, 14).withTrimmedTop (4));
        }
        static void cover (juce::Graphics& g, juce::Rectangle<float> r, const SqSynth::SqPollenPlugin::PackInfo& pk, bool sel, float phase)
        {
            juce::Colour c (pk.colour);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 6.0f);
            juce::ColourGradient grad (c.withAlpha (0.55f), r.getX(), r.getY(), c.darker (0.8f).withAlpha (0.6f), r.getRight(), r.getBottom(), false);
            g.setGradientFill (grad); g.fillRoundedRectangle (r, 6.0f);
            auto in = r.reduced (8.0f, 6.0f).withTrimmedBottom (16.0f);
            g.setColour (Theme::text().withAlpha (0.85f));
            switch (pk.glyph)
            {
                case 0: for (int k = 0; k < 7; ++k) { const bool black = k == 1 || k == 2 || k == 4; auto key = juce::Rectangle<float> (in.getX() + in.getWidth() * (float) k / 7.0f, in.getY(), in.getWidth() / 7.0f - 1.0f, in.getHeight() * (black ? 0.55f : 0.9f)); g.setColour (black ? juce::Colours::black.withAlpha (0.8f) : Theme::text().withAlpha (0.8f)); g.fillRect (key); } break;
                case 1: { juce::Path p; for (int i = 0; i <= 40; ++i) { const float t = (float) i / 40.0f; const float y = in.getCentreY() + std::sin (t * 9.0f + phase) * in.getHeight() * 0.3f * std::sin (t * 3.14f); if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + in.getWidth() * t, y); } g.strokePath (p, juce::PathStrokeType (2.0f)); break; }
                case 2: for (int k = 0; k < 5; ++k) { const float x = in.getX() + in.getWidth() * (0.1f + 0.2f * (float) k); g.drawLine (x, in.getBottom(), x, in.getY() + in.getHeight() * (0.2f + 0.15f * (float) (k % 3)), 2.0f); g.fillEllipse (x - 3.0f, in.getY() + in.getHeight() * (0.2f + 0.15f * (float) (k % 3)) - 3.0f, 6.0f, 6.0f); } break;
                case 3: { juce::Path p; for (int i = 0; i <= 40; ++i) { const float t = (float) i / 40.0f; const float y = in.getCentreY() - std::sin (t * 6.28f * 1.5f) * in.getHeight() * 0.4f; if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + in.getWidth() * t, y); } g.strokePath (p, juce::PathStrokeType (3.0f)); break; }
                case 4: { juce::Path p; p.startNewSubPath (in.getX(), in.getCentreY()); p.lineTo (in.getRight() - in.getWidth() * 0.3f, in.getCentreY()); p.lineTo (in.getRight(), in.getY()); p.lineTo (in.getRight(), in.getBottom()); p.closeSubPath(); g.fillPath (p); break; }
                case 5: for (int k = 0; k < 4; ++k) { const float y = in.getY() + in.getHeight() * (0.2f + 0.2f * (float) k); g.drawLine (in.getX(), y, in.getRight(), y, 1.5f); } { juce::Path p; p.startNewSubPath (in.getX() + in.getWidth() * 0.3f, in.getBottom()); p.quadraticTo (in.getCentreX(), in.getY() - in.getHeight() * 0.4f, in.getRight() - in.getWidth() * 0.3f, in.getBottom()); g.strokePath (p, juce::PathStrokeType (2.0f)); } break;
                case 6: for (int k = 0; k < 3; ++k) { g.drawEllipse (in.getX() + in.getWidth() * (0.15f + 0.3f * (float) k), in.getCentreY() - 7.0f, 14.0f, 18.0f, 2.0f); } break;
                case 7: { juce::Path p; p.addTriangle (in.getCentreX(), in.getY(), in.getX() + in.getWidth() * 0.25f, in.getBottom() - 6.0f, in.getRight() - in.getWidth() * 0.25f, in.getBottom() - 6.0f); g.fillPath (p); g.fillEllipse (in.getCentreX() - 4.0f, in.getBottom() - 8.0f, 8.0f, 8.0f); break; }
                case 8: { juce::Random rnd (4); for (int k = 0; k < 40; ++k) g.fillEllipse (in.getX() + rnd.nextFloat() * in.getWidth(), in.getY() + rnd.nextFloat() * in.getHeight(), 2.0f + rnd.nextFloat() * 3.0f, 2.0f + rnd.nextFloat() * 3.0f); break; }
                case 9: for (int k = 0; k < 8; ++k) { const float x = in.getX() + in.getWidth() * (float) k / 8.0f; const float hgt = in.getHeight() * (0.3f + 0.6f * std::abs (std::sin ((float) k * 1.3f))); g.fillRoundedRectangle (x + 1.0f, in.getBottom() - hgt, in.getWidth() / 8.0f - 3.0f, hgt, 2.0f); } break;
                default: for (int k = 0; k < 6; ++k) { const float x = in.getX() + in.getWidth() * (float) k / 6.0f; g.drawLine (x, in.getBottom(), x + in.getWidth() / 6.0f, in.getY(), 2.0f); } break;
            }
            g.setColour (sel ? Theme::text() : juce::Colour (pk.colour).withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, sel ? 2.0f : 1.0f);
            g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, true)); g.drawFittedText (pk.name, r.toNearestInt().reduced (6, 3), juce::Justification::bottomLeft, 1);
        }
        void paint (juce::Graphics& g) override
        {
            if (po == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto box = [&] (juce::Rectangle<float> a, const juce::String& t) { g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (a, 4.0f); g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true)); g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft); };
            auto left = r.removeFromLeft (r.getWidth() * 0.5f); r.removeFromLeft (8.0f);
            box (left, "PACKS  11 packs, " + juce::String (sig != nullptr ? (int) sig->presets.size() : 0) + " sounds   click a pack, then a sound");
            auto& packs = SqSynth::SqPollenPlugin::packs();
            for (int i = 0; i < (int) packs.size(); ++i) cover (g, cardRect (i), packs[(size_t) i], po->pack.get() == packs[(size_t) i].name, phase);
            auto mid = r.removeFromLeft (r.getWidth() * 0.55f); r.removeFromLeft (8.0f);
            box (mid, po->pack.get().toUpperCase() + "  " + juce::String (presets.size()) + " sounds" + (po->presetName.get().isNotEmpty() ? "   now: " + po->presetName.get() : juce::String()));
            const auto& pk = packs[(size_t) SqSynth::SqPollenPlugin::packIndex (po->pack.get())];
            box (r, "MACROS");
            {
                auto in = r.reduced (8.0f, 16.0f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f)); g.drawFittedText (pk.blurb, in.removeFromTop (30.0f).toNearestInt(), juce::Justification::topLeft, 2);
                const float rh = in.getHeight() / 4.0f;
                for (int m = 0; m < 4; ++m)
                {
                    auto row = juce::Rectangle<float> (in.getX(), in.getY() + rh * (float) m, in.getWidth(), rh).reduced (0.0f, 4.0f);
                    const float v = po->macroValue (m);
                    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (row, 4.0f);
                    g.setColour (accent.withAlpha (0.7f)); g.fillRoundedRectangle (row.reduced (2.0f).withWidth (juce::jmax (4.0f, (row.getWidth() - 4.0f) * v)), 3.0f);
                    g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, true)); g.drawText (juce::String (m + 1) + "  " + po->macroLabel (m), row.toNearestInt().reduced (8, 0), juce::Justification::centredLeft);
                    g.setFont (Theme::font (8.5f)); g.drawText (juce::String (juce::roundToInt (v * 100)) + " %", row.toNearestInt().reduced (8, 0), juce::Justification::centredRight);
                }
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f)); g.drawText ("the Macro knobs below move these", in.toNearestInt(), juce::Justification::bottomLeft);
            }
            if (lastPack != po->pack.get()) { lastPack = po->pack.get(); rebuildPresets(); }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (po == nullptr) return;
            auto& packs = SqSynth::SqPollenPlugin::packs();
            for (int i = 0; i < (int) packs.size(); ++i) if (cardRect (i).contains (e.position)) { po->pack = packs[(size_t) i].name; rebuildPresets(); repaint(); return; }
        }
        bool hitTest (int x, int y) override   { return cardsArea().contains ((float) x, (float) y); }
        SqSynth::SqPollenPlugin* po = nullptr; const Signature::Entry* sig = nullptr; juce::ListBox list; juce::Array<int> presets; juce::String lastPack;
    };
