// Included inside namespace Skins (PluginSkins.h): the Hive Pads face - 4x4 pad grid with banks, the pad editor, layers.

    /** Small dark knobs with an amber arc for the pad editor (and other in-face sliders). */
    struct PadKnobLookAndFeel : public juce::LookAndFeel_V4
    {
        juce::Colour accent;
        explicit PadKnobLookAndFeel (juce::Colour a) : accent (a) { setColour (juce::Slider::textBoxTextColourId, Theme::text()); setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack); }
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
            const float side = juce::jmin (r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre (side, side);
            auto c = r.getCentre(); const float rad = side * 0.5f;
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (r.expanded (1.5f).translated (0, 1.5f));
            g.setGradientFill (juce::ColourGradient (Theme::panel2().brighter (0.35f), c.x - rad * 0.4f, c.y - rad * 0.5f, Theme::panel2().darker (0.5f), c.x + rad, c.y + rad, true));
            g.fillEllipse (r);
            juce::Path track; track.addCentredArc (c.x, c.y, rad * 0.8f, rad * 0.8f, 0.0f, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.strokePath (track, juce::PathStrokeType (3.0f));
            const float av = a0 + (a1 - a0) * pos; const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
            juce::Path lit; const float mid = (a0 + a1) * 0.5f;
            if (bipolar) lit.addCentredArc (c.x, c.y, rad * 0.8f, rad * 0.8f, 0.0f, juce::jmin (mid, av), juce::jmax (mid, av), true); else lit.addCentredArc (c.x, c.y, rad * 0.8f, rad * 0.8f, 0.0f, a0, av, true);
            g.setColour (accent); g.strokePath (lit, juce::PathStrokeType (3.0f));
            const auto tip = c.getPointOnCircumference (rad * 0.62f, av), base = c.getPointOnCircumference (rad * 0.2f, av);
            g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawLine (base.x, base.y, tip.x, tip.y, 2.0f);
        }
        juce::Label* createSliderTextBox (juce::Slider& s) override { auto* l = juce::LookAndFeel_V4::createSliderTextBox (s); l->setFont (Theme::font (9.5f, true)); l->setJustificationType (juce::Justification::centred); return l; }
    };

    struct HivePadsViz : public Viz,
                         public juce::FileDragAndDropTarget
    {
        HivePadsViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), pads (dynamic_cast<SqHivePadsPlugin*> (&p)), knobLnf (Theme::amber())
        {
            setInterceptsMouseClicks (true, true);
            auto mk = [this] (juce::Slider& s, const char* id, double lo, double hi, double step, const juce::String& suffix, double skewMid = 0.0)
            {
                s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
                s.setRange (lo, hi, step); if (skewMid > 0) s.setSkewFactorFromMidPoint (skewMid); s.setTextValueSuffix (suffix); s.setLookAndFeel (&knobLnf);
                s.onValueChange = [this, &s, id] { if (pads) pads->setPadProperty (sel(), id, s.getValue()); repaint(); };
                addAndMakeVisible (s);
            };
            mk (tune, "tune", -24, 24, 1, " st"); mk (fine, "fine", -100, 100, 1, " ct"); mk (gain, "gain", -24, 12, 0.1, " dB"); mk (pan, "pan", -1, 1, 0.01, "");
            mk (attack, "attack", 0, 1, 0.001, " s", 0.1); mk (hold, "hold", 0, 2, 0.001, " s", 0.3); mk (decay, "decay", 0.05, 10, 0.01, " s", 1.0); mk (cut, "cutoff", 40, 20000, 1, " Hz", 1500); mk (res, "res", 0, 1, 0.01, "");
            for (auto* b : { &loadKit, &loadFile, &slice, &clear, &oneShot, &reverse }) { addAndMakeVisible (*b); }
            loadKit.setButtonText ("LOAD KIT"); loadFile.setButtonText ("LOAD SAMPLE"); slice.setButtonText ("SLICE LOOP TO PADS"); clear.setButtonText ("CLEAR PAD");
            oneShot.setButtonText ("ONE SHOT"); reverse.setButtonText ("REVERSE");
            oneShot.setClickingTogglesState (true); reverse.setClickingTogglesState (true);
            oneShot.onClick = [this] { if (pads) pads->setPadProperty (sel(), "oneShot", oneShot.getToggleState()); };
            reverse.onClick = [this] { if (pads) pads->setPadProperty (sel(), "reverse", reverse.getToggleState()); };
            loadKit.onClick = [this] { showKitMenu(); };
            loadFile.onClick = [this] { chooseFile (false); };
            slice.onClick = [this] { chooseFile (true); };
            clear.onClick = [this] { if (pads) { pads->clearPad (sel()); refreshEditor(); } };
            layerMode.addItemList (SqHivePadsPlugin::layerModeNames(), 1); layerMode.onChange = [this] { if (pads) pads->setPadProperty (sel(), "layerMode", layerMode.getSelectedId() - 1); };
            addAndMakeVisible (layerMode);
            choke.addItem ("NO CHOKE", 1); for (int i = 1; i <= 16; ++i) choke.addItem ("CHOKE " + juce::String (i), i + 1);
            choke.onChange = [this] { if (pads) pads->setPadProperty (sel(), "choke", choke.getSelectedId() - 1); };
            addAndMakeVisible (choke);
            noteBox.setTooltip ("MIDI note that plays this pad"); noteBox.onReturnKey = noteBox.onFocusLost = [this] { if (pads) { auto t = noteBox.getText().trim(); const int n = t.containsOnly ("0123456789") ? t.getIntValue() : noteFromName (t); pads->setPadProperty (sel(), "note", juce::jlimit (0, 127, n)); refreshEditor(); } };
            addAndMakeVisible (noteBox);
            nameBox.setTooltip ("Pad name"); nameBox.onReturnKey = nameBox.onFocusLost = [this] { if (pads) pads->setPadProperty (sel(), "name", nameBox.getText()); };
            addAndMakeVisible (nameBox);
            for (int b = 0; b < 4; ++b) { auto& bb = bankBtns[(size_t) b]; bb.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + b))); bb.setClickingTogglesState (false); bb.onClick = [this, b] { if (pads) { pads->currentBank = b; repaint(); } }; addAndMakeVisible (bb); }
            refreshEditor();
        }

        int sel() const   { return pads ? juce::jlimit (0, SqHivePadsPlugin::numPads - 1, pads->selectedPad.get()) : 0; }
        static int noteFromName (const juce::String& s)
        {
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            auto u = s.toUpperCase().replace ("DB", "C#").replace ("EB", "D#").replace ("GB", "F#").replace ("AB", "G#").replace ("BB", "A#");
            int pc = 0, len = 1; for (int i = 11; i >= 0; --i) if (u.startsWith (names[i])) { pc = i; len = (int) std::strlen (names[i]); break; }
            return (u.substring (len).getIntValue() + 1) * 12 + pc;
        }

        void refreshEditor()
        {
            if (! pads) return;
            const int i = sel();
            auto g = [&] (const char* id, double d) { return (double) pads->getPadProperty (i, id, d); };
            tune.setValue (g ("tune", 0), juce::dontSendNotification); fine.setValue (g ("fine", 0), juce::dontSendNotification); gain.setValue (g ("gain", 0), juce::dontSendNotification); pan.setValue (g ("pan", 0), juce::dontSendNotification);
            attack.setValue (g ("attack", 0), juce::dontSendNotification); hold.setValue (g ("hold", 0), juce::dontSendNotification); decay.setValue (g ("decay", 10), juce::dontSendNotification); cut.setValue (g ("cutoff", 20000), juce::dontSendNotification); res.setValue (g ("res", 0), juce::dontSendNotification);
            oneShot.setToggleState ((bool) pads->getPadProperty (i, "oneShot", true), juce::dontSendNotification); reverse.setToggleState ((bool) pads->getPadProperty (i, "reverse", false), juce::dontSendNotification);
            layerMode.setSelectedId ((int) pads->getPadProperty (i, "layerMode", 0) + 1, juce::dontSendNotification);
            choke.setSelectedId ((int) pads->getPadProperty (i, "choke", 0) + 1, juce::dontSendNotification);
            noteBox.setText (juce::MidiMessage::getMidiNoteName (pads->padNote (i), true, true, 4), juce::dontSendNotification);
            nameBox.setText (pads->padName (i), juce::dontSendNotification);
            repaint();
        }

        juce::Rectangle<int> gridArea() const   { auto r = getLocalBounds().reduced (6); return r.removeFromLeft (juce::jmin (r.getHeight(), (int) (r.getWidth() * 0.46f))); }
        juce::Rectangle<int> padRect (int k) const   { auto g = gridArea().withTrimmedTop (24); const int w = g.getWidth() / 4, h = g.getHeight() / 4; const int row = 3 - k / 4, col = k % 4; return juce::Rectangle<int> (g.getX() + col * w, g.getY() + row * h, w, h).reduced (3); }
        int padAt (juce::Point<int> p) const   { for (int k = 0; k < 16; ++k) if (padRect (k).contains (p)) return k; return -1; }

        void resized() override
        {
            auto g = gridArea();
            auto bankRow = g.removeFromTop (22);
            for (int b = 0; b < 4; ++b) bankBtns[(size_t) b].setBounds (bankRow.removeFromLeft (34).reduced (2, 1));
            auto r = getLocalBounds().reduced (6); r.removeFromLeft (gridArea().getWidth() + 10);
            auto top = r.removeFromTop (24);
            nameBox.setBounds (top.removeFromLeft (juce::jmin (180, top.getWidth() / 3))); top.removeFromLeft (6); noteBox.setBounds (top.removeFromLeft (64)); top.removeFromLeft (6);
            layerMode.setBounds (top.removeFromLeft (120)); top.removeFromLeft (6); choke.setBounds (top.removeFromLeft (100));
            r.removeFromTop (6);
            auto btns = r.removeFromTop (24);
            loadKit.setBounds (btns.removeFromLeft (84)); btns.removeFromLeft (4); loadFile.setBounds (btns.removeFromLeft (100)); btns.removeFromLeft (4); slice.setBounds (btns.removeFromLeft (140)); btns.removeFromLeft (4); clear.setBounds (btns.removeFromLeft (84)); btns.removeFromLeft (10);
            oneShot.setBounds (btns.removeFromLeft (80)); btns.removeFromLeft (4); reverse.setBounds (btns.removeFromLeft (76));
            r.removeFromTop (6);
            layersArea = r.removeFromTop (juce::jmax (40, (int) (r.getHeight() * 0.34f)));
            r.removeFromTop (6);
            auto knobs = r;
            const int kw = knobs.getWidth() / 9;
            juce::Slider* ks[] = { &tune, &fine, &gain, &pan, &attack, &hold, &decay, &cut, &res };
            for (int i = 0; i < 9; ++i) ks[i]->setBounds (knobs.removeFromLeft (kw).reduced (2));
        }

        void paint (juce::Graphics& g) override
        {
            if (pads == nullptr) return;
            pads->processPendingTrigger();
            auto grid = gridArea();
            g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (grid.toFloat(), 6.0f);
            const int bank = juce::jlimit (0, 3, pads->currentBank.get());
            for (int b = 0; b < 4; ++b) bankBtns[(size_t) b].setToggleState (b == bank, juce::dontSendNotification);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
            g.drawText ("BANK " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)) + "   -   click: play & select, drop files on a pad, right-click: pad menu", grid.withHeight (22).withTrimmedLeft (150), juce::Justification::centredLeft);
            for (int k = 0; k < 16; ++k)
            {
                const int pad = bank * 16 + k;
                auto r = padRect (k).toFloat();
                const bool has = pads->padHasSound (pad), selected = pad == sel();
                const float flash = pads->padFlash[(size_t) pad];
                auto base = has ? accent.interpolatedWith (Theme::amber(), 0.3f + 0.05f * (k % 4)) : Theme::panel2();
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0, 2), 6.0f);
                g.setGradientFill (juce::ColourGradient (base.brighter (0.25f + flash * 0.6f), r.getX(), r.getY(), base.darker (0.3f), r.getX(), r.getBottom(), false));
                g.fillRoundedRectangle (r, 6.0f);
                if (flash > 0.01f) { g.setColour (juce::Colours::white.withAlpha (flash * 0.5f)); g.fillRoundedRectangle (r, 6.0f); }
                g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (r.reduced (2).withHeight (r.getHeight() * 0.4f), 5.0f);
                g.setColour (selected ? Theme::acid() : juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (r, 6.0f, selected ? 2.0f : 1.0f);
                g.setColour (has ? juce::Colours::white : Theme::dim()); g.setFont (Theme::font (10.0f, true));
                g.drawFittedText (pads->padName (pad), r.toNearestInt().reduced (6, 4), juce::Justification::topLeft, 2);
                g.setFont (Theme::font (8.5f)); g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawText (juce::MidiMessage::getMidiNoteName (pads->padNote (pad), true, true, 4) + (pads->numLayers (pad) > 1 ? "  " + juce::String (pads->numLayers (pad)) + " layers" : ""), r.toNearestInt().reduced (6, 4), juce::Justification::bottomLeft);
                const int choke = (int) pads->getPadProperty (pad, "choke", 0);
                if (choke > 0) { g.setColour (Theme::amber()); g.drawText ("C" + juce::String (choke), r.toNearestInt().reduced (6, 4), juce::Justification::bottomRight); }
            }
            // layers
            g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (layersArea.toFloat(), 4.0f);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
            g.drawText ("LAYERS  -  " + pads->padName (sel()) + "   (velocity range per layer; drop more files on the pad to add layers)", layersArea.reduced (6, 3), juce::Justification::topLeft);
            auto p = pads->padState (sel(), false);
            const int nl = p.isValid() ? p.getNumChildren() : 0;
            auto in = layersArea.reduced (6, 16);
            if (nl == 0) { g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f)); g.drawText ("Empty pad - LOAD SAMPLE, LOAD KIT, or drop a file here", in, juce::Justification::centred); }
            const int rowH = juce::jmin (22, juce::jmax (12, in.getHeight() / juce::jmax (1, nl)));
            for (int l = 0; l < nl; ++l)
            {
                auto L = p.getChild (l);
                auto row = juce::Rectangle<int> (in.getX(), in.getY() + l * rowH, in.getWidth(), rowH - 2);
                g.setColour (Theme::panel2().withAlpha (0.6f)); g.fillRoundedRectangle (row.toFloat(), 3.0f);
                const int lo = (int) L.getProperty ("velLo", 1), hi = (int) L.getProperty ("velHi", 127);
                auto velBar = row.removeFromRight (160).reduced (4, 3);
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (velBar.toFloat(), 2.0f);
                g.setColour (accent.withAlpha (0.8f)); g.fillRoundedRectangle (juce::Rectangle<float> ((float) velBar.getX() + velBar.getWidth() * (lo - 1) / 126.0f, (float) velBar.getY(), velBar.getWidth() * (hi - lo) / 126.0f, (float) velBar.getHeight()), 2.0f);
                g.setColour (Theme::text()); g.setFont (Theme::font (9.0f));
                g.drawFittedText (juce::String (l + 1) + ".  " + juce::File (L.getProperty ("file").toString()).getFileName() + "   root " + juce::MidiMessage::getMidiNoteName ((int) L.getProperty ("root", 60), true, true, 4) + "   " + juce::String ((double) L.getProperty ("gain", 0.0), 1) + " dB", row.reduced (6, 0), juce::Justification::centredLeft, 1);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f)); g.drawText (juce::String (lo) + "-" + juce::String (hi), velBar, juce::Justification::centred);
            }
            // knob captions
            const char* caps[] = { "TUNE", "FINE", "GAIN", "PAN", "ATTACK", "HOLD", "DECAY", "CUTOFF", "RES" };
            juce::Slider* ks[] = { &tune, &fine, &gain, &pan, &attack, &hold, &decay, &cut, &res };
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
            for (int i = 0; i < 9; ++i) g.drawText (caps[i], ks[i]->getBounds().withHeight (12).translated (0, -1), juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! pads) return;
            const int k = padAt (e.getPosition()); if (k < 0) return;
            const int pad = pads->currentBank.get() * 16 + k;
            pads->selectedPad = pad; refreshEditor();
            if (e.mods.isPopupMenu()) { showPadMenu (pad); return; }
            pads->triggerPad (pad, e.mods.isAltDown() ? 0.5f : 1.0f);
            if (onPad) onPad();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            // drag a pad onto another pad: copy / swap (Alt)
            juce::ignoreUnused (e);
        }
        bool isInterestedInFileDrag (const juce::StringArray& files) override   { for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true; return false; }
        void filesDropped (const juce::StringArray& files, int x, int y) override
        {
            if (! pads) return;
            int k = padAt ({ x, y }); int pad = k >= 0 ? pads->currentBank.get() * 16 + k : sel();
            if (layersArea.contains (x, y)) pad = sel();
            // several files onto the grid: spread across consecutive pads; several onto one pad / the layer list: layers
            const bool spread = k >= 0 && files.size() > 1 && ! layersArea.contains (x, y);
            for (int i = 0; i < files.size(); ++i)
            {
                juce::File f (files[i]); if (! f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) continue;
                const int target = spread ? juce::jmin (SqHivePadsPlugin::numPads - 1, pad + i) : pad;
                if (spread || (i == 0 && ! e_shiftHeld)) pads->clearPad (target);
                pads->addLayer (target, f);
            }
            pads->selectedPad = pad; refreshEditor();
        }
        bool e_shiftHeld = false;

        void showKitMenu()
        {
            juce::PopupMenu m;
            for (int k = 0; k < DrumKit::numKits; ++k) m.addItem (k + 1, juce::String (DrumKit::kits[k].name) + "   -   " + DrumKit::kits[k].description);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadKit), [this] (int r)
            {
                if (r <= 0 || ! pads || ! onLoadKit) return;
                onLoadKit (r - 1);
                refreshEditor();
            });
        }
        void chooseFile (bool toSlice)
        {
            auto fc = std::make_shared<juce::FileChooser> (toSlice ? "Slice a loop onto the pads" : "Load a sample on the pad", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | (toSlice ? 0 : juce::FileBrowserComponent::canSelectMultipleItems), [this, fc, toSlice] (const juce::FileChooser& c)
            {
                if (! pads || c.getResults().isEmpty()) return;
                if (toSlice) { const int n = pads->sliceToPads (c.getResult(), pads->currentBank.get() * 16, 16); juce::ignoreUnused (n); }
                else { pads->clearPad (sel()); for (auto& f : c.getResults()) pads->addLayer (sel(), f); }
                refreshEditor();
            });
        }
        void showPadMenu (int pad)
        {
            juce::PopupMenu m;
            m.addItem (1, "Load sample..."); m.addItem (2, "Add layer..."); m.addItem (3, "Clear pad"); m.addSeparator();
            m.addItem (4, "Copy pad"); m.addItem (5, "Paste pad", copied.isValid()); m.addSeparator();
            m.addItem (6, "Rename..."); m.addItem (7, "Set note to the GM default (" + juce::MidiMessage::getMidiNoteName (36 + pad, true, true, 4) + ")");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, pad] (int r)
            {
                if (! pads || r <= 0) return;
                if (r == 1) chooseFile (false);
                else if (r == 2)
                {
                    auto fc = std::make_shared<juce::FileChooser> ("Add a layer", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
                    fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc, pad] (const juce::FileChooser& c) { if (c.getResult() != juce::File()) { pads->addLayer (pad, c.getResult()); refreshEditor(); } });
                }
                else if (r == 3) pads->clearPad (pad);
                else if (r == 4) copied = pads->padState (pad).createCopy();
                else if (r == 5 && copied.isValid()) { auto dst = pads->padState (pad); auto* um = &plugin.edit.getUndoManager(); while (dst.getNumChildren() > 0) dst.removeChild (0, um); for (int i = 0; i < copied.getNumChildren(); ++i) dst.addChild (copied.getChild (i).createCopy(), -1, um); for (auto id : { "tune", "fine", "gain", "pan", "attack", "hold", "decay", "cutoff", "res", "reverse", "choke", "oneShot", "layerMode", "name" }) dst.setProperty (id, copied.getProperty (id), um); }
                else if (r == 6) { auto aw = std::make_shared<juce::AlertWindow> ("Rename Pad", "", juce::MessageBoxIconType::NoIcon); aw->addTextEditor ("n", pads->padName (pad)); aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0); aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, pad] (int rr) { if (rr == 1 && pads) pads->setPadProperty (pad, "name", aw->getTextEditorContents ("n")); refreshEditor(); }), false); }
                else if (r == 7) pads->setPadProperty (pad, "note", 36 + pad);
                refreshEditor();
            });
        }

        ~HivePadsViz() override   { for (auto* k : { &tune, &fine, &gain, &pan, &attack, &hold, &decay, &cut, &res }) k->setLookAndFeel (nullptr); }
        std::function<void()> onPad;
        std::function<void (int kit)> onLoadKit;
        SqHivePadsPlugin* pads;
        PadKnobLookAndFeel knobLnf;
        juce::Slider tune, fine, gain, pan, attack, hold, decay, cut, res;
        juce::TextButton loadKit, loadFile, slice, clear, oneShot, reverse;
        std::array<juce::TextButton, 4> bankBtns;
        juce::ComboBox layerMode, choke;
        juce::TextEditor noteBox, nameBox;
        juce::Rectangle<int> layersArea;
        juce::ValueTree copied;
    };
