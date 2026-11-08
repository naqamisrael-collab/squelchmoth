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

    //==============================================================================
    /** Wingbeat Echo / Jr.: the tap map - every repeat's time, level and side as the settings stand, with the live
        delay time, tempo, style and ducking. */
    struct WingbeatViz : public Viz, private juce::Timer
    {
        WingbeatViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), wb (dynamic_cast<SqWingbeatBase*> (&p)) { startTimerHz (24); }
        SqWingbeatBase* wb;
        void timerCallback() override { repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (wb == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (210.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "TAP MAP   (time across, level up, left / right by colour)");
            plot.removeFromTop (4.0f);
            const auto& p = wb->proc.p; const double bpm = wb->lastBpm;
            // predicted repeats
            struct Rep { double ms; float level, pan; };
            std::vector<Rep> reps;
            auto msFor = [&] (int sync, double free) { return sync >= 0 ? 60000.0 / bpm * Eng::WingMod::syncBeats (sync) : free; };
            const double tl = msFor (p.syncL, p.timeMs), tr = p.mode == Wingbeat::dual ? msFor (p.syncR, p.timeRMs) : tl;
            const float fb = juce::jlimit (0.0f, 1.0f, p.freeze ? 1.0f : p.feedback);
            if (p.mode == Wingbeat::rhythm)
            {
                const auto& pat = Wingbeat::patterns()[(size_t) juce::jlimit (0, (int) Wingbeat::patterns().size() - 1, p.pattern)];
                const auto& taps = p.customTaps.empty() ? pat.taps : p.customTaps;
                const double six = 60000.0 / bpm / 4.0, len = (p.customTaps.empty() ? pat.lengthSixteenths : p.customLength) * six;
                for (int rep = 0; rep < 3; ++rep) for (auto& t : taps) reps.push_back ({ t.sixteenths * six + rep * len, t.level * std::pow (fb, (float) rep), t.pan });
            }
            else
            {
                float lv = 1.0f;
                for (int k = 1; k <= 12; ++k)
                {
                    const bool right = p.mode == Wingbeat::pingPong ? (k % 2 == 0) : false;
                    const double t = p.mode == Wingbeat::pingPong ? (k % 2 ? tl : tr) * 0 + (double) k * tl : (double) k * tl;
                    reps.push_back ({ t + (right ? p.offsetMs : 0.0), lv, p.mode == Wingbeat::single ? 0.0f : (right ? 1.0f : (p.mode == Wingbeat::pingPong ? -1.0f : -0.6f)) });
                    if (p.mode == Wingbeat::dual) reps.push_back ({ (double) k * tr + p.offsetMs, lv, 0.6f });
                    lv *= fb; if (lv < 0.01f) break;
                }
            }
            double maxMs = 1; for (auto& x : reps) maxMs = juce::jmax (maxMs, x.ms);
            maxMs *= 1.08;
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) plot.getBottom() - 1, plot.getX(), plot.getRight());
            const float duckLv = juce::Decibels::decibelsToGain (wb->liveDuckDb.load());
            for (auto& x : reps)
            {
                const float px = plot.getX() + (float) (x.ms / maxMs) * plot.getWidth();
                const float h = juce::jlimit (0.0f, 1.0f, x.level) * (plot.getHeight() - 14.0f) * duckLv;
                const auto col = x.pan < -0.2f ? acc : (x.pan > 0.2f ? juce::Colour (0xffffb23a) : juce::Colours::white);
                g.setColour (col.withAlpha (0.85f)); g.fillRoundedRectangle (px - 2.0f, plot.getBottom() - h, 4.0f, h, 1.5f);
                g.setColour (col.withAlpha (0.25f)); g.fillEllipse (px - 6.0f, plot.getBottom() - h - 6.0f, 12.0f, 12.0f);
            }
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (8.5f));
            for (int k = 0; k <= 4; ++k)
            {
                const double ms = maxMs * k / 4.0; const float x = plot.getX() + plot.getWidth() * (float) k / 4.0f;
                g.drawText (ms >= 1000 ? juce::String (ms / 1000.0, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms", (int) x - 2, (int) plot.getBottom() + 1, 60, 10, juce::Justification::left);
            }
            // readout
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            const auto& st = Wingbeat::style (p.style);
            ColourBits::caption (g, q.removeFromTop (14.0f), juce::String (Wingbeat::modeName (p.mode)).toUpperCase());
            ColourBits::big (g, q.removeFromTop (26.0f), st.name, juce::Colours::white, 16.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.0f));
            g.drawFittedText (st.archetype, q.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 3);
            ColourBits::line (g, q.removeFromTop (16.0f), "time " + (wb->liveTimeMs.load() >= 1000 ? juce::String (wb->liveTimeMs.load() / 1000.0f, 2) + " s" : juce::String (wb->liveTimeMs.load(), 1) + " ms")
                              + (p.syncL >= 0 ? "  (" + juce::String (Eng::WingMod::syncName (p.syncL)) + ")" : juce::String()));
            ColourBits::line (g, q.removeFromTop (16.0f), "tempo " + juce::String (bpm, 1) + " BPM");
            if (p.duckDb > 0) ColourBits::line (g, q.removeFromTop (16.0f), "ducking " + juce::String (wb->liveDuckDb.load(), 1) + " dB");
            if (p.freeze) ColourBits::line (g, q.removeFromTop (16.0f), "FROZEN");
        }
    };

    //==============================================================================
    /** Foil Plate / Lite: the decay per band (RT60 across the spectrum, with the live auto-decay), the plate, the
        input stage with its measured THD, freeze state. */
    struct FoilPlateViz : public Viz, private juce::Timer
    {
        FoilPlateViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), fp (dynamic_cast<SqFoilPlateBase*> (&p)), full (dynamic_cast<SqFoilPlatePlugin*> (&p)) { startTimerHz (20); }
        SqFoilPlateBase* fp; SqFoilPlatePlugin* full; double thd = 0; juce::String lastKey; int settle = 0;
        void timerCallback() override
        {
            if (full != nullptr)
            {
                const auto k = juce::String (full->input.get()) + "," + juce::String (full->bank[SqFoilPlatePlugin::drive], 1);
                if (k != lastKey) { lastKey = k; settle = 3; } else if (settle > 0 && --settle == 0) thd = full->inputThd();
            }
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (fp == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (210.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "DECAY ACROSS THE SPECTRUM   (RT60, seconds)");
            plot.removeFromTop (4.0f); plot.removeFromBottom (12.0f);
            const auto& p = fp->proc.p; const auto& a = FoilPlate::archetype (p.archetype);
            float lo, mid, hi; fp->proc.bandDecays (lo, mid, hi);
            auto rtAt = [&] (float hz)
            {
                // the absorption shelves: low below 250 Hz, high above the archetype's corner (log interpolation)
                const float xl = juce::jlimit (0.0f, 1.0f, std::log2 (250.0f / hz) + 0.5f), xh = juce::jlimit (0.0f, 1.0f, std::log2 (hz / a.highCornerHz) + 0.5f);
                return mid * std::pow (lo / mid, xl) * std::pow (hi / mid, xh);
            };
            const float top = juce::jmax (1.0f, lo, mid) * 1.2f;
            juce::Path pth;
            for (int i = 0; i <= (int) plot.getWidth(); i += 2)
            {
                const float x = plot.getX() + (float) i, hz = hzForX (x, plot);
                const float y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, rtAt (hz) / top) * plot.getHeight();
                if (i == 0) pth.startNewSubPath (x, y); else pth.lineTo (x, y);
            }
            ColourBits::fillUnder (g, pth, plot, acc);
            glowStroke (g, pth, acc, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (8.5f));
            for (float f : { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f })
                g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xForHz (f, plot) - 10, (int) plot.getBottom() + 1, 30, 10, juce::Justification::centred);
            g.drawText (juce::String (top, 1) + " s", (int) plot.getX() + 2, (int) plot.getY(), 60, 10, juce::Justification::left);
            // readout
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "PLATE");
            ColourBits::big (g, q.removeFromTop (26.0f), a.name, juce::Colours::white, 16.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.0f));
            g.drawFittedText (a.description, q.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 3);
            ColourBits::line (g, q.removeFromTop (16.0f), p.freeze ? juce::String ("FROZEN") + (p.darkening ? " (darkening)" : "") : "500 Hz " + juce::String (mid, 2) + " s   6 kHz " + juce::String (hi, 2) + " s");
            if (p.autoDecay != 0) ColourBits::line (g, q.removeFromTop (16.0f), "auto decay x" + juce::String (fp->liveDecayMul.load(), 2));
            if (full != nullptr) ColourBits::line (g, q.removeFromTop (16.0f), juce::String (FoilPlate::inputName (p.input)) + " input, THD " + juce::String (thd, 2) + " %");
        }
    };

    //==============================================================================
    /** Radiant / Radiant Pre: a heat VU (how hard the first tube is driven), the tone stack curve and measured THD. */
    struct RadiantViz : public Viz, private juce::Timer
    {
        RadiantViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), rd (dynamic_cast<SqRadiantPlugin*> (&p)), pre (dynamic_cast<SqRadiantPrePlugin*> (&p)) { startTimerHz (24); }
        SqRadiantPlugin* rd; SqRadiantPrePlugin* pre; double thd = 0; juce::String lastKey; int settle = 0; float needle = -40;
        void timerCallback() override
        {
            juce::String k; for (auto ap : plugin.getAutomatableParameters()) k << juce::String (ap->getCurrentValue(), 2) << ",";
            for (int i = 0; i < plugin.state.getNumProperties(); ++i) k << plugin.state.getProperty (plugin.state.getPropertyName (i)).toString() << ";";
            if (k != lastKey) { lastKey = k; settle = 3; } else if (settle > 0 && --settle == 0) thd = rd != nullptr ? rd->thd() : (pre != nullptr ? pre->thd() : 0.0);
            const float heat = rd != nullptr ? rd->heatDb.load() : (pre != nullptr ? pre->heatDb.load() : -100.0f);
            needle += 0.3f * (juce::jlimit (-40.0f, 6.0f, heat) - needle);
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (210.0f); r.removeFromRight (8.0f);
            auto vu = r.removeFromLeft (juce::jmin (300.0f, r.getWidth() * 0.45f)); r.removeFromLeft (8.0f);
            ColourBits::panel (g, vu, acc);
            auto face = vu.reduced (16.0f, 14.0f);
            ColourBits::caption (g, face.removeFromTop (14.0f), "HEAT");
            const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.25f);
            const float radius = face.getHeight() * 1.05f;
            for (int db = -20; db <= 3; db += 1)
            {
                const float a = juce::jmap ((float) db, -20.0f, 3.0f, -0.85f, 0.85f);
                const float r0 = radius * (db % 5 == 0 ? 0.80f : 0.86f);
                g.setColour (db > 0 ? juce::Colour (0xffff5a3a) : juce::Colours::white.withAlpha (0.6f));
                g.drawLine (pivot.x + std::sin (a) * r0, pivot.y - std::cos (a) * r0, pivot.x + std::sin (a) * radius * 0.92f, pivot.y - std::cos (a) * radius * 0.92f, db % 5 == 0 ? 1.6f : 0.8f);
            }
            const float an = juce::jmap (juce::jlimit (-20.0f, 3.0f, needle + 12.0f), -20.0f, 3.0f, -0.85f, 0.85f);
            g.setColour (juce::Colour (0xffffe14a)); g.drawLine (pivot.x, pivot.y, pivot.x + std::sin (an) * radius * 0.95f, pivot.y - std::cos (an) * radius * 0.95f, 2.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (12.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), rd != nullptr ? "BASS / TREBLE (Baxandall)" : "PREAMP");
            ColourBits::freqGrid (g, plot, 12.0f);
            if (rd != nullptr)
            {
                Eng::Biquad bq, tq; bq.set (Eng::Biquad::lowShelf, 48000, 100, 0.45, rd->bank[SqRadiantPlugin::bass]); tq.set (Eng::Biquad::highShelf, 48000, 8000, 0.45, rd->bank[SqRadiantPlugin::treble]);
                auto pth = ColourBits::curve (plot, 12.0f, [&] (float hz) { return (float) (bq.magnitudeDb (hz, 48000) + tq.magnitudeDb (hz, 48000)); });
                ColourBits::fillUnder (g, pth, plot, acc); glowStroke (g, pth, acc, 2.0f);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            const int a = rd != nullptr ? juce::jlimit (0, 2, rd->arch.get()) : 3;
            ColourBits::caption (g, q.removeFromTop (14.0f), "CIRCUIT");
            ColourBits::big (g, q.removeFromTop (26.0f), Radiant::archName (a), juce::Colours::white, 16.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.0f));
            g.drawFittedText (Radiant::archDescription (a), q.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 3);
            ColourBits::line (g, q.removeFromTop (16.0f), "THD " + juce::String (thd, thd < 1 ? 2 : 1) + " % at -18 dBFS");
            if (pre != nullptr && pre->bias.get()) ColourBits::line (g, q.removeFromTop (16.0f), "BIAS: starved, choppy");
            if (rd != nullptr && rd->mic.get()) ColourBits::line (g, q.removeFromTop (16.0f), "MIC input (+10 dB)");
        }
    };

    //==============================================================================
    /** Maw / Maw Lite: the gain reduction now and its recent history, with the release that is in force. */
    struct MawViz : public Viz, private juce::Timer
    {
        MawViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mw (dynamic_cast<SqMawBase*> (&p)) { startTimerHz (30); }
        SqMawBase* mw; float hist[Maw::Processor::historyLen] {};
        void timerCallback() override { if (mw != nullptr) mw->proc.historyCopy (hist); repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (mw == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (210.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "GAIN REDUCTION   (last 5 s, 0 to -40 dB)");
            juce::Path pth; const int N = Maw::Processor::historyLen;
            for (int i = 0; i < N; ++i)
            {
                const float x = plot.getX() + plot.getWidth() * (float) i / (float) (N - 1), y = plot.getY() + juce::jlimit (0.0f, 1.0f, -hist[i] / 40.0f) * plot.getHeight();
                if (i == 0) pth.startNewSubPath (x, y); else pth.lineTo (x, y);
            }
            juce::Path fill (pth); fill.lineTo (plot.getRight(), plot.getY()); fill.lineTo (plot.getX(), plot.getY()); fill.closeSubPath();
            g.setColour (acc.withAlpha (0.18f)); g.fillPath (fill); glowStroke (g, pth, acc, 2.0f);
            for (int db : { 10, 20, 30 })
            {
                const float y = plot.getY() + db / 40.0f * plot.getHeight();
                g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
                g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (Theme::font (8.5f)); g.drawText ("-" + juce::String (db), (int) plot.getX() + 2, (int) y - 11, 30, 10, juce::Justification::left);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "CRUSHING");
            ColourBits::big (g, q.removeFromTop (34.0f), juce::String (-mw->grDb.load(), 1) + " dB", juce::Colours::white, 24.0f);
            const auto& p = mw->proc.p;
            ColourBits::line (g, q.removeFromTop (16.0f), "attack 1.3 ms");
            ColourBits::line (g, q.removeFromTop (16.0f), p.fastRelease ? juce::String ("release FAST (80 ms)") : "release " + juce::String (p.releaseS, 2) + " s, stretch " + juce::String (juce::roundToInt (p.stretch * 100)) + " %");
            if (p.darknessHz < 19900) ColourBits::line (g, q.removeFromTop (16.0f), "darkness " + ColourBits::hz (p.darknessHz));
            if (p.keyOn) ColourBits::line (g, q.removeFromTop (16.0f), "KEY input, HPF " + ColourBits::hz (p.keyHpfHz));
        }
    };
