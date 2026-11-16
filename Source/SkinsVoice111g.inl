// Included inside namespace Skins (PluginSkins.h): faces for Vox Moth, Talk Wing and Clean Room (1.11 Voice).

    //==============================================================================
    /** Vox Moth: the band bank as glowing columns (each band's level from the voice, on a log frequency axis), the
        16-point level curve drawn over it and draggable, the carrier's notes, and the X/Y pad (drag the dot). */
    struct VoxMothViz : public Viz, private juce::Timer
    {
        VoxMothViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), vm (dynamic_cast<SqVoxMothPlugin*> (&p)) { setInterceptsMouseClicks (true, false); startTimerHz (30); }
        SqVoxMothPlugin* vm; juce::Rectangle<float> bankR, padR; int dragPt = -1; bool dragPad = false; float peak[VoxMoth::maxBands] {};
        void timerCallback() override
        {
            if (vm == nullptr) return;
            for (int k = 0; k < vm->proc.bandCount(); ++k) { const float l = vm->proc.bandLevel (k); peak[k] = std::max (l, peak[k] * 0.9f); }
            repaint();
        }
        void setCurve (juce::Point<float> p)
        {
            if (vm == nullptr || dragPt < 0) return;
            const float v = juce::jlimit (-24.0f, 12.0f, 12.0f - (p.y - bankR.getY()) / bankR.getHeight() * 36.0f);
            vm->bank.params[(size_t) (SqVoxMothPlugin::lv1 + dragPt)]->setParameter (v, juce::sendNotification);
        }
        void setPad (juce::Point<float> p)
        {
            if (vm == nullptr) return;
            vm->bank.params[(size_t) SqVoxMothPlugin::xyX]->setParameter (juce::jlimit (0.0f, 100.0f, (p.x - padR.getX()) / padR.getWidth() * 100.0f), juce::sendNotification);
            vm->bank.params[(size_t) SqVoxMothPlugin::xyY]->setParameter (juce::jlimit (0.0f, 100.0f, (padR.getBottom() - p.y) / padR.getHeight() * 100.0f), juce::sendNotification);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (vm == nullptr) return;
            dragPad = padR.contains (e.position); dragPt = -1;
            if (dragPad) { vm->edit.getUndoManager().beginNewTransaction ("Vox Moth: X/Y"); setPad (e.position); return; }
            if (bankR.contains (e.position)) { dragPt = juce::jlimit (0, VoxMoth::curvePoints - 1, juce::roundToInt ((e.position.x - bankR.getX()) / bankR.getWidth() * (VoxMoth::curvePoints - 1))); vm->edit.getUndoManager().beginNewTransaction ("Vox Moth: level curve"); setCurve (e.position); }
        }
        void mouseDrag (const juce::MouseEvent& e) override { if (dragPad) setPad (e.position); else setCurve (e.position); }
        void mouseUp (const juce::MouseEvent&) override { dragPt = -1; dragPad = false; }
        void paint (juce::Graphics& g) override
        {
            if (vm == nullptr) return;
            const auto acc = neon (accent); const auto kp = vm->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto pad = r.removeFromRight (std::min (r.getHeight(), r.getWidth() * 0.26f)); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            const auto& vc = VoxMoth::voicings()[(size_t) juce::jlimit (0, 19, vm->voicing.get())];
            ColourBits::caption (g, plot.removeFromTop (14.0f), juce::String (kp.bands) + " BANDS   " + juce::String (VoxMoth::shapeName (kp.shape)).toUpperCase() + "   carrier: " + VoxMoth::carrierName (kp.carrier)
                                 + (vm->proc.usingKey() ? " (key input)" : "") + "   voicing: " + vc.name + "   (drag the line to set the band levels)");
            auto notes = plot.removeFromBottom (18.0f); bankR = plot;
            const int nb = vm->proc.bandCount(); const float lo = std::log2 (40.0f), hi = std::log2 (16000.0f);
            auto xAt = [&] (float hz) { return plot.getX() + (std::log2 (std::max (40.0f, hz)) - lo) / (hi - lo) * plot.getWidth(); };
            for (float hz : { 100.0f, 300.0f, 1000.0f, 3000.0f, 10000.0f })
            { g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawVerticalLine ((int) xAt (hz), plot.getY(), plot.getBottom()); g.setFont (Theme::font (8.0f)); g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawText (hz >= 1000 ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz), (int) xAt (hz) + 2, (int) plot.getBottom() - 10, 30, 10, juce::Justification::left); }
            const float bw = std::max (2.0f, plot.getWidth() / (float) std::max (8, nb) * 0.55f);
            for (int k = 0; k < nb; ++k)
            {
                const float hz = vm->proc.bandHz (k), x = xAt (hz);
                const float lv = juce::jlimit (0.0f, 1.0f, (Eng::gainToDb (peak[k] + 1.0e-6f) + 66.0f) / 60.0f);
                auto col = acc.interpolatedWith (juce::Colour (0xff7ad7ff), (float) k / std::max (1, nb - 1));
                auto bar = juce::Rectangle<float> (x - bw * 0.5f, plot.getBottom() - lv * plot.getHeight(), bw, lv * plot.getHeight());
                g.setGradientFill (juce::ColourGradient (col.withAlpha (0.95f), bar.getX(), bar.getY(), col.withAlpha (0.12f), bar.getX(), plot.getBottom(), false));
                g.fillRoundedRectangle (bar, bw * 0.35f);
                g.setColour (col.withAlpha (0.25f)); g.fillRoundedRectangle (bar.expanded (bw * 0.6f, 2.0f), bw);
            }
            // the level curve
            juce::Path c; for (int i = 0; i < VoxMoth::curvePoints; ++i)
            {
                const float x = plot.getX() + (float) i / (VoxMoth::curvePoints - 1) * plot.getWidth();
                const float y = plot.getY() + (12.0f - kp.curve[(size_t) i] - kp.tiltDb * ((float) i / (VoxMoth::curvePoints - 1) - 0.5f)) / 36.0f * plot.getHeight();
                if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
                g.setColour (juce::Colours::white.withAlpha (0.9f)); g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
            }
            g.setColour (juce::Colours::white.withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved));
            g.setColour (juce::Colours::white.withAlpha (0.85f)); g.strokePath (c, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
            // the carrier's notes
            juce::String ns; for (int v = 0; v < VoxMoth::maxVoices; ++v) { const float n = vm->proc.carrierNote (v); if (n > 0) ns << SqSessionKey::rootNames()[juce::roundToInt (n) % 12] << juce::String (juce::roundToInt (n) / 12 - 1) << "  "; }
            ColourBits::caption (g, notes, "CARRIER  " + (ns.isEmpty() ? juce::String ("-") : ns) + (vm->liveHz.load() > 0 ? "   voice " + juce::String (vm->liveHz.load(), 1) + " Hz" : juce::String()));
            // the X/Y pad
            ColourBits::panel (g, pad, acc, true);
            auto q = pad.reduced (12.0f); ColourBits::caption (g, q.removeFromTop (14.0f), "X  SLIDE    Y  RESONANCE");
            padR = q;
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (int i = 1; i < 4; ++i) { g.drawVerticalLine ((int) (q.getX() + q.getWidth() * i / 4.0f), q.getY(), q.getBottom()); g.drawHorizontalLine ((int) (q.getY() + q.getHeight() * i / 4.0f), q.getX(), q.getRight()); }
            const juce::Point<float> dot (q.getX() + kp.xyX * q.getWidth(), q.getBottom() - kp.xyY * q.getHeight());
            g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.6f), dot, acc.withAlpha (0.0f), dot.translated (40.0f, 0.0f), true)); g.fillEllipse (dot.x - 40, dot.y - 40, 80, 80);
            g.setColour (juce::Colours::white); g.fillEllipse (dot.x - 6, dot.y - 6, 12, 12);
        }
    };

    //==============================================================================
    /** Talk Wing: the mouth (the voice's envelope) as a glowing ribbon over the instrument's own envelope, the open
        mouth drawn from it, and which way the sound is routed. */
    struct TalkWingViz : public Viz, private juce::Timer
    {
        TalkWingViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), tw (dynamic_cast<SqTalkWingPlugin*> (&p)) { startTimerHz (30); }
        SqTalkWingPlugin* tw;
        void timerCallback() override { repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (tw == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto side = r.removeFromRight (r.getWidth() * 0.3f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            static const char* modes[] = { "the voice on the key input shapes this track", "this voice shapes the instrument on the key input", "the voice drives the built-in buzz (nothing on the key input)" };
            ColourBits::caption (g, plot.removeFromTop (14.0f), juce::String ("THE MOUTH   ") + modes[juce::jlimit (0, 2, tw->proc.mode())]);
            auto yAt = [&] (float lnMag) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, (lnMag + 9.0f) / 9.0f) * plot.getHeight(); };
            const int n = TalkWing::envPoints;
            auto pathOf = [&] (bool mouth) { juce::Path p; for (int i = 0; i < n; ++i) { const float x = plot.getX() + (float) i / (n - 1) * plot.getWidth(); const float y = yAt (mouth ? tw->proc.mouthEnv (i) : tw->proc.carrierEnv (i)); if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); } return p; };
            auto cp = pathOf (false); g.setColour (juce::Colour (0xff7ad7ff).withAlpha (0.5f)); g.strokePath (cp, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
            auto mp = pathOf (true); juce::Path fill (mp); fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.45f), plot.getX(), plot.getY(), acc.withAlpha (0.02f), plot.getX(), plot.getBottom(), false)); g.fillPath (fill);
            g.setColour (acc.withAlpha (0.3f)); g.strokePath (mp, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved));
            g.setColour (acc); g.strokePath (mp, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));
            for (float hz : { 100.0f, 300.0f, 1000.0f, 3000.0f })
            { const float x = plot.getX() + std::log2 (hz / 80.0f) / 5.5f * plot.getWidth(); g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom()); g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (Theme::font (8.0f)); g.drawText (hz >= 1000 ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz), (int) x + 2, (int) plot.getBottom() - 10, 30, 10, juce::Justification::left); }
            // the mouth itself: opens with the level of the low formant region, widens with the high one
            ColourBits::panel (g, side, acc, true);
            auto q = side.reduced (12.0f); ColourBits::caption (g, q.removeFromTop (14.0f), tw->proc.talking() ? "TALKING" : "QUIET");
            float lowE = 0, highE = 0; for (int i = 10; i < 40; ++i) lowE += tw->proc.mouthEnv (i); for (int i = 50; i < 80; ++i) highE += tw->proc.mouthEnv (i);
            const float open = juce::jlimit (0.05f, 1.0f, (lowE / 30.0f + 7.0f) / 6.0f) * (tw->proc.talking() ? 1.0f : 0.1f), wide = juce::jlimit (0.4f, 1.0f, (highE / 30.0f + 9.0f) / 7.0f);
            const auto c = q.getCentre(); const float w = q.getWidth() * 0.38f * wide, h = q.getHeight() * 0.32f * open;
            juce::Path lips; lips.startNewSubPath (c.x - w, c.y); lips.cubicTo (c.x - w * 0.5f, c.y - h - 8, c.x + w * 0.5f, c.y - h - 8, c.x + w, c.y); lips.cubicTo (c.x + w * 0.5f, c.y + h + 10, c.x - w * 0.5f, c.y + h + 10, c.x - w, c.y); lips.closeSubPath();
            g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.85f), c.x, c.y, juce::Colour (0xffff4fa8).withAlpha (0.4f), c.x + w, c.y + h, true)); g.fillPath (lips);
            g.setColour (juce::Colours::white.withAlpha (0.8f)); g.strokePath (lips, juce::PathStrokeType (1.6f));
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.0f));
            g.drawText ("sibilance " + juce::String (juce::roundToInt (tw->proc.sibilanceNow() * 100.0f)) + " %", q.removeFromBottom (14.0f), juce::Justification::centred);
        }
    };

    //==============================================================================
    /** Clean Room: the input spectrum, the noise floor it found (or learned) and the cut, in 96 bands; the mains and
        how much of each of its harmonics is really there; clicks repaired. */
    struct CleanRoomViz : public Viz, private juce::Timer
    {
        CleanRoomViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), cr (dynamic_cast<SqCleanRoomPlugin*> (&p)) { startTimerHz (25); }
        SqCleanRoomPlugin* cr; float inS[CleanRoom::profileBands] {}, redS[CleanRoom::profileBands] {};
        void timerCallback() override
        {
            if (cr == nullptr) return;
            for (int k = 0; k < CleanRoom::profileBands; ++k) { inS[k] += 0.35f * (cr->proc.inputDb (k) - inS[k]); redS[k] += 0.35f * (cr->proc.reductionDb (k) - redS[k]); }
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (cr == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto side = r.removeFromRight (r.getWidth() * 0.26f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), juce::String ("SPECTRUM   white = input, red = noise floor ") + (cr->proc.hasProfile() ? "(learned)" : "(tracked)") + ", glow = what is taken out"
                                 + (cr->residual.get() ? "   LISTENING TO THE RESIDUAL" : ""));
            const int n = CleanRoom::profileBands;
            auto xAt = [&] (int k) { return plot.getX() + (float) k / (n - 1) * plot.getWidth(); };
            auto yAt = [&] (float dB) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, (dB + 110.0f) / 90.0f) * plot.getHeight(); };
            juce::Path in, nz, cut; in.startNewSubPath (xAt (0), yAt (inS[0])); nz.startNewSubPath (xAt (0), yAt (cr->proc.noiseDb (0)));
            cut.startNewSubPath (xAt (0), yAt (inS[0]));
            for (int k = 1; k < n; ++k) { in.lineTo (xAt (k), yAt (inS[k])); nz.lineTo (xAt (k), yAt (cr->proc.noiseDb (k))); cut.lineTo (xAt (k), yAt (inS[k])); }
            for (int k = n - 1; k >= 0; --k) cut.lineTo (xAt (k), yAt (inS[k] + redS[k]));
            cut.closeSubPath();
            g.setColour (acc.withAlpha (0.35f)); g.fillPath (cut);
            g.setColour (juce::Colour (0xffff5a6e).withAlpha (0.85f)); g.strokePath (nz, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
            g.setColour (juce::Colours::white.withAlpha (0.9f)); g.strokePath (in, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
            for (float hz : { 100.0f, 1000.0f, 10000.0f })
            { const float x = plot.getX() + std::log2 (hz / 40.0f) / 9.0f * plot.getWidth(); g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom()); g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (Theme::font (8.0f)); g.drawText (hz >= 1000 ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz), (int) x + 2, (int) plot.getBottom() - 10, 30, 10, juce::Justification::left); }
            // the mains: each harmonic's bar is how much hum is really there (that much is notched)
            ColourBits::panel (g, side, acc, true);
            auto q = side.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "HUM " + juce::String (cr->proc.mainsHz()) + " Hz" + (cr->humMode.get() == 0 ? " (found)" : "") + "   CLICKS " + juce::String (cr->proc.clicksRepaired()));
            auto bars = q.removeFromTop (q.getHeight() * 0.7f);
            const float bw = bars.getWidth() / 24.0f;
            for (int h = 0; h < 24; ++h)
            {
                const float pr = cr->proc.humPresence (h);
                auto b = juce::Rectangle<float> (bars.getX() + h * bw + 1, bars.getBottom() - pr * bars.getHeight(), bw - 2, pr * bars.getHeight());
                g.setColour ((h < 4 ? juce::Colour (0xffff9a3c) : acc).withAlpha (0.3f + 0.6f * pr)); g.fillRoundedRectangle (b, 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.1f)); g.drawRect (juce::Rectangle<float> (bars.getX() + h * bw + 1, bars.getY(), bw - 2, bars.getHeight()), 0.5f);
            }
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.0f));
            g.drawText (cr->isLearning() ? "LEARNING THE NOISE..." : (cr->proc.hasProfile() ? "noise profile learned" : "noise tracked automatically"), q, juce::Justification::centred);
        }
    };
