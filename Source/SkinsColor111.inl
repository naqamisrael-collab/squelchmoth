// Included inside namespace Skins (PluginSkins.h): faces for the 1.11 Color & Space plug-ins.

    //==============================================================================
    /** Cinder: what the current settings do to a 1 kHz tone at -18 dBFS - the harmonic bars H2-H10 with THD, the
        transfer curve, the model (or the two blended models) and the live drive / auto-gain readout. The profile is
        re-measured off the audio thread whenever a setting changes (a private copy of the DSP renders 0.6 s). */
    struct CinderViz : public Viz, private juce::Timer
    {
        CinderViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), cd (dynamic_cast<SqCinderPlugin*> (&p)) { startTimerHz (20); }
        SqCinderPlugin* cd;
        float harm[10] {}; double thd = 0; juce::String lastKey; int settle = 0;

        juce::String settingsKey() const
        {
            juce::String k;
            for (int i = 0; i < SqCinderPlugin::numParams; ++i) k << juce::String (cd->bank[(size_t) i], 2) << ",";
            k << cd->modelA.get() << cd->modelB.get() << (int) cd->punish.get() << (int) cd->steep.get() << cd->msMode.get() << cd->oversample.get();
            return k;
        }
        void timerCallback() override
        {
            if (cd == nullptr) return;
            const auto k = settingsKey();
            if (k != lastKey) { lastKey = k; settle = 3; }          // wait for the knob to stop for ~150 ms
            else if (settle > 0 && --settle == 0) { thd = cd->harmonicProfile (-18.0f, harm); }
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (cd == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (210.0f);
            r.removeFromRight (8.0f);
            auto curveR = r.removeFromLeft (juce::jmin (230.0f, r.getHeight() + 30.0f));
            r.removeFromLeft (8.0f);

            // transfer curve: output vs input for one slow sine cycle through the current model (shape only)
            ColourBits::panel (g, curveR, acc);
            auto pc = curveR.reduced (12.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawHorizontalLine ((int) pc.getCentreY(), pc.getX(), pc.getRight()); g.drawVerticalLine ((int) pc.getCentreX(), pc.getY(), pc.getBottom());
            g.drawLine (pc.getX(), pc.getBottom(), pc.getRight(), pc.getY(), 1.0f);
            {
                // the model's static curve at the current drive (input -18 dBFS peak .. +6 dBFS), scaled to fit
                const auto& d = Cinder::model (cd->modelA.get());
                const auto& cv = Eng::Nonlinear::curveFor (d.nlA, d.charA);
                const float drv = Eng::dbToGain (cd->bank[SqCinderPlugin::drive] + (cd->punish.get() ? 20.0f : 0.0f) + d.calibDb) * 2.0f;
                const double top = juce::jmax (std::abs (cv.eval (drv)), std::abs (cv.eval (-drv)), 1.0e-6);
                juce::Path p; const int N = 160;
                for (int i = 0; i <= N; ++i)
                {
                    const float x = -1.0f + 2.0f * (float) i / (float) N;
                    const float y = (float) (cv.eval (x * drv) / top);
                    const float px = pc.getX() + pc.getWidth() * (float) i / (float) N, py = pc.getCentreY() - juce::jlimit (-1.0f, 1.0f, y) * pc.getHeight() * 0.5f;
                    if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
                }
                glowStroke (g, p, acc, 2.0f);
            }
            ColourBits::caption (g, curveR.reduced (10.0f, 4.0f).removeFromTop (12.0f), "TRANSFER");

            // harmonic bars
            ColourBits::panel (g, r, acc);
            auto hb = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, hb.removeFromTop (14.0f), "HARMONICS AT -18 dBFS, 1 kHz   (H2-H10, dB below the fundamental)");
            hb.removeFromTop (4.0f);
            auto labels = hb.removeFromBottom (14.0f);
            const float bw = hb.getWidth() / 9.0f;
            for (int k = 1; k < 10; ++k)
            {
                const float db = juce::jlimit (-100.0f, 0.0f, harm[k]);
                const float hgt = (db + 100.0f) / 100.0f * hb.getHeight();
                auto bar = juce::Rectangle<float> (hb.getX() + (k - 1) * bw + bw * 0.18f, hb.getBottom() - hgt, bw * 0.64f, hgt);
                const auto col = (k % 2 == 1) ? acc : juce::Colour (0xffffb23a);       // even harmonics amber, odd in the accent
                g.setGradientFill (juce::ColourGradient (col.withAlpha (0.95f), bar.getX(), bar.getY(), col.withAlpha (0.25f), bar.getX(), bar.getBottom(), false));
                g.fillRoundedRectangle (bar, 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (8.5f));
                g.drawText ("H" + juce::String (k + 1), (int) (hb.getX() + (k - 1) * bw), (int) labels.getY(), (int) bw, 12, juce::Justification::centred);
                if (hgt > 14.0f) g.drawText (juce::String (juce::roundToInt (db)), bar.toNearestInt().withHeight (12).translated (0, -13), juce::Justification::centred);
            }

            // readout
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            const int ma = cd->modelA.get(), mb = cd->modelB.get(); const float bl = cd->bank[SqCinderPlugin::blend];
            ColourBits::caption (g, q.removeFromTop (14.0f), bl > 0.5f ? "MODEL BLEND" : "MODEL");
            ColourBits::big (g, q.removeFromTop (26.0f), Cinder::modelName (ma), juce::Colours::white, 17.0f);
            if (bl > 0.5f) ColourBits::line (g, q.removeFromTop (16.0f), juce::String (juce::roundToInt (bl)) + " % " + Cinder::modelName (mb));
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.0f));
            g.drawFittedText (Cinder::model (ma).archetype, q.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 3);
            ColourBits::line (g, q.removeFromTop (16.0f), "THD " + juce::String (thd, thd < 1.0 ? 2 : 1) + " %");
            ColourBits::line (g, q.removeFromTop (16.0f), "drive now " + juce::String (cd->activity.load(), 1) + " dB" + (cd->punish.get() ? "  (PUNISH)" : ""));
            if (cd->autoGain.get()) ColourBits::line (g, q.removeFromTop (16.0f), "auto gain " + juce::String (cd->autoGainDb.load(), 1) + " dB");
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (SqCinderPlugin::osNames()[juce::jlimit (0, 4, cd->oversample.get())]) + (cd->linearPhase.get() ? " linear phase" : " oversampling")
                              + "  " + juce::String (cd->getLatencySeconds() * 1000.0, 2) + " ms");
            if (cd->delta.get()) ColourBits::line (g, q.removeFromTop (16.0f), "DELTA: hearing only what it adds");
        }
    };
