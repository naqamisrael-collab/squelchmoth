// Included inside namespace Skins (PluginSkins.h): the Moth Tune face.

    /** Vivid colour helpers shared by the new Signature faces. */
    inline juce::Colour neon (juce::Colour c, float boost = 0.25f)   { return c.withSaturation (juce::jmin (1.0f, c.getSaturation() + boost)).withBrightness (juce::jmin (1.0f, c.getBrightness() + 0.12f)); }
    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
    {
        g.setColour (c.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (w * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.22f)); g.strokePath (p, juce::PathStrokeType (w * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);                   g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    inline juce::String noteLabel (float note)
    {
        if (note < 0) return "--";
        const int n = juce::roundToInt (note);
        return SqScales::noteNames()[((n % 12) + 12) % 12] + juce::String (n / 12 - 1);
    }

    /** Moth Tune: scrolling pitch graph (input, corrected, target, harmonies), big note readout with a cents meter,
        a 78-note range strip and a clickable one-octave scale editor (click = remove / restore, right-click = bypass). */
    struct TuneViz : public Viz
    {
        TuneViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), tune (dynamic_cast<SqTunePlugin*> (&p))
        {
            setInterceptsMouseClicks (true, false);
        }
        SqTunePlugin* tune;
        float viewCentre = 60.0f;
        std::function<void()> onChange;

        juce::Rectangle<float> graphArea() const   { auto r = getLocalBounds().toFloat().reduced (8.0f); r.removeFromBottom (78.0f); return r.removeFromLeft (r.getWidth() - 230.0f); }
        juce::Rectangle<float> readoutArea() const { auto r = getLocalBounds().toFloat().reduced (8.0f); r.removeFromBottom (78.0f); return r.removeFromRight (222.0f); }
        juce::Rectangle<float> rangeArea() const   { auto r = getLocalBounds().toFloat().reduced (8.0f); return r.removeFromBottom (78.0f).removeFromTop (18.0f); }
        juce::Rectangle<float> keysArea() const    { auto r = getLocalBounds().toFloat().reduced (8.0f); auto b = r.removeFromBottom (78.0f); b.removeFromTop (22.0f); return b; }

        void paint (juce::Graphics& g) override
        {
            if (tune == nullptr) return;
            const auto acc = neon (accent);
            const juce::Colour hc[4] { juce::Colour (0xff3ad1ff), juce::Colour (0xffff3ad1), juce::Colour (0xffb6ff3a), juce::Colour (0xffffa13a) };
            auto ga = graphArea();
            // backdrop
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0716).withAlpha (0.92f), ga.getX(), ga.getY(), acc.darker (2.2f).withAlpha (0.92f), ga.getRight(), ga.getBottom(), false));
            g.fillRoundedRectangle (ga, 6.0f);

            // gather points
            const int w = tune->dispWrite.load();
            const int count = juce::jmin (SqTunePlugin::dispSize - 1, juce::jmin (w, 700));
            float lo = 1e9f, hi = -1e9f;
            for (int k = 0; k < count; ++k)
            {
                const auto& d = tune->disp[(size_t) ((w - 1 - k) & (SqTunePlugin::dispSize - 1))];
                if (d.in > 0) { lo = juce::jmin (lo, d.in, d.out); hi = juce::jmax (hi, d.in, d.out); }
            }
            if (hi > lo) viewCentre += ((lo + hi) * 0.5f - viewCentre) * 0.08f;
            const float span = 18.0f;
            auto yFor = [&] (float n) { return ga.getCentreY() - (n - viewCentre) / span * ga.getHeight(); };

            // semitone lanes: scale notes glow, root brighter, removed red, bypass amber
            const int mask = tune->allowedMask(), byp = tune->bypassMask.get(), rem = tune->removeMask.get(), root = tune->keyRoot.get();
            g.saveState();
            g.reduceClipRegion (ga.toNearestInt());
            for (int n = (int) std::floor (viewCentre - span * 0.6f); n <= (int) std::ceil (viewCentre + span * 0.6f); ++n)
            {
                const float y = yFor ((float) n), hgt = ga.getHeight() / span;
                const int pc = ((n % 12) + 12) % 12;
                juce::Colour band = juce::Colours::transparentBlack;
                if ((rem >> pc) & 1) band = juce::Colour (0xffff3a5a).withAlpha (0.10f);
                else if ((byp >> pc) & 1) band = juce::Colour (0xffffb23a).withAlpha (0.12f);
                else if ((mask >> pc) & 1) band = acc.withAlpha (pc == root ? 0.20f : 0.09f);
                g.setColour (band); g.fillRect (ga.getX(), y - hgt * 0.5f, ga.getWidth(), hgt);
                g.setColour (juce::Colours::white.withAlpha (pc == 0 ? 0.16f : 0.05f));
                g.drawHorizontalLine ((int) (y + hgt * 0.5f), ga.getX(), ga.getRight());
                g.setColour (((mask >> pc) & 1) ? juce::Colours::white.withAlpha (0.75f) : juce::Colours::white.withAlpha (0.28f));
                g.setFont (Theme::font (9.0f, pc == root));
                g.drawText (noteLabel ((float) n), (int) ga.getX() + 4, (int) (y - 6), 34, 12, juce::Justification::left);
            }
            // traces, newest at the right
            const float dx = (ga.getWidth() - 40.0f) / 700.0f;
            auto trace = [&] (auto get, juce::Colour c, float width, bool dashed)
            {
                juce::Path p; bool pen = false;
                for (int k = count - 1; k >= 0; --k)
                {
                    const auto& d = tune->disp[(size_t) ((w - 1 - k) & (SqTunePlugin::dispSize - 1))];
                    const float v = get (d);
                    const float x = ga.getRight() - 4.0f - (float) k * dx;
                    if (v <= 0) { pen = false; continue; }
                    const float y = yFor (v);
                    if (! pen) { p.startNewSubPath (x, y); pen = true; } else p.lineTo (x, y);
                }
                if (dashed) { g.setColour (c.withMultipliedAlpha (0.7f)); g.strokePath (p, juce::PathStrokeType (1.0f)); }   // (JUCE dashes join sub-paths, so a plain hairline)
                else glowStroke (g, p, c, width);
            };
            for (int i = 0; i < 4; ++i) trace ([i] (const SqTunePlugin::Disp& d) { return d.h[i]; }, hc[i].withAlpha (0.85f), 1.6f, false);
            trace ([] (const SqTunePlugin::Disp& d) { return d.target; }, juce::Colours::white.withAlpha (0.55f), 1.0f, true);
            trace ([] (const SqTunePlugin::Disp& d) { return d.in; }, juce::Colours::white.withAlpha (0.45f), 1.4f, false);
            trace ([] (const SqTunePlugin::Disp& d) { return d.out; }, acc, 2.4f, false);
            g.restoreState();
            g.setColour (acc.withAlpha (0.35f)); g.drawRoundedRectangle (ga, 6.0f, 1.0f);
            // legend
            g.setFont (Theme::font (9.0f, true));
            auto leg = ga.reduced (8.0f).removeFromTop (12.0f).removeFromRight (300.0f);
            auto item = [&] (const juce::String& t, juce::Colour c) { auto b = leg.removeFromLeft (75.0f); g.setColour (c); g.fillRoundedRectangle (b.removeFromLeft (10.0f).reduced (0, 3), 2.0f); g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawText (t, b.withTrimmedLeft (4), juce::Justification::centredLeft); };
            item ("INPUT", juce::Colours::white.withAlpha (0.5f)); item ("TUNED", acc); item ("TARGET", juce::Colours::white.withAlpha (0.7f)); item ("HARMONY", hc[0]);

            // Moth Tune 2 vibrato visualiser: the singer's vibrato depth (filled) and rate (line) over the same time,
            // with the part dynamic vibrato is acting on brightened
            {
                auto vs = ga.reduced (6.0f).removeFromBottom (42.0f);
                g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (vs, 4.0f);
                juce::Path depth, rate, act; bool pr = false;
                depth.startNewSubPath (vs.getRight() - 4.0f, vs.getBottom());
                for (int k = 0; k < count; ++k)
                {
                    const auto& d = tune->disp[(size_t) ((w - 1 - k) & (SqTunePlugin::dispSize - 1))];
                    const float x = vs.getRight() - 4.0f - (float) k * dx;
                    if (x < vs.getX()) break;
                    const float yd = vs.getBottom() - juce::jlimit (0.0f, 1.0f, d.vibDepth / 120.0f) * (vs.getHeight() - 4.0f);
                    depth.lineTo (x, yd);
                    if (d.vibAmt > 0.05f) act.addRectangle (x - dx * 0.5f, yd, dx + 0.5f, vs.getBottom() - yd);
                    if (d.vibRate > 0 && d.in > 0) { const float yr = vs.getBottom() - juce::jlimit (0.0f, 1.0f, (d.vibRate - 2.0f) / 8.0f) * (vs.getHeight() - 4.0f); if (! pr) { rate.startNewSubPath (x, yr); pr = true; } else rate.lineTo (x, yr); }
                    else pr = false;
                }
                depth.lineTo (juce::jmax (vs.getX(), vs.getRight() - 4.0f - (float) count * dx), vs.getBottom()); depth.closeSubPath();
                g.setColour (acc.withAlpha (0.22f)); g.fillPath (depth);
                g.setColour (acc.withAlpha (0.55f)); g.fillPath (act);
                g.setColour (juce::Colour (0xff3ad1ff).withAlpha (0.8f)); g.strokePath (rate, juce::PathStrokeType (1.2f));
                g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (Theme::font (8.5f, true));
                const auto mode = SqTunePlugin::dvModeNames()[juce::jlimit (0, 3, tune->dvMode.get())];
                g.drawText ("VIBRATO  " + juce::String (tune->liveVibRate.load(), 1) + " Hz  " + juce::String (juce::roundToInt (tune->liveVibDepth.load())) + " ct"
                            + (tune->dvMode.get() > 0 ? "   DYNAMIC " + mode : juce::String()), vs.reduced (6.0f, 2.0f), juce::Justification::topLeft);
            }

            // readout
            auto ra = readoutArea();
            g.setGradientFill (juce::ColourGradient (acc.darker (1.6f).withAlpha (0.9f), ra.getX(), ra.getY(), juce::Colour (0xff120a1e).withAlpha (0.92f), ra.getRight(), ra.getBottom(), false));
            g.fillRoundedRectangle (ra, 6.0f);
            g.setColour (acc.withAlpha (0.45f)); g.drawRoundedRectangle (ra, 6.0f, 1.0f);
            const float tgt = tune->liveTarget.load(), in = tune->liveIn.load(), cents = tune->liveCents.load();
            auto top = ra.reduced (10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("TARGET", top.removeFromTop (14.0f), juce::Justification::centredLeft);
            g.setColour (tgt > 0 ? juce::Colours::white : juce::Colours::white.withAlpha (0.3f));
            g.setFont (Theme::font (46.0f, true));
            g.drawText (noteLabel (tgt), top.removeFromTop (52.0f), juce::Justification::centredLeft);
            // cents meter
            auto meter = top.removeFromTop (22.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (meter, 4.0f);
            const float cx = meter.getCentreX();
            const float v = juce::jlimit (-100.0f, 100.0f, cents) / 100.0f * meter.getWidth() * 0.5f;
            juce::ColourGradient cg (juce::Colour (0xff3affb0), cx, 0, juce::Colour (0xffff3a5a), meter.getRight(), 0, false);
            cg.addColour (0.5, juce::Colour (0xffffe03a));
            g.setGradientFill (cg);
            if (tgt > 0) g.fillRoundedRectangle (v >= 0 ? meter.withLeft (cx).withWidth (juce::jmax (2.0f, v)) : meter.withLeft (cx + v).withWidth (-v), 4.0f);
            g.setColour (juce::Colours::white); g.fillRect (cx - 1.0f, meter.getY() - 2.0f, 2.0f, meter.getHeight() + 4.0f);
            g.setFont (Theme::font (10.0f, true));
            g.drawText ((cents >= 0 ? "+" : "") + juce::String (tgt > 0 ? cents : 0.0f, 1) + " ct correction", top.removeFromTop (18.0f), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.65f)); g.setFont (Theme::font (10.0f));
            if (in > 0)
            {
                const float ic = (in - std::round (in)) * 100.0f;
                g.drawText ("Singing " + noteLabel (in) + (ic >= 0 ? "  +" : "  ") + juce::String (juce::roundToInt (ic)) + " ct", top.removeFromTop (16.0f), juce::Justification::centredLeft);
            }
            else g.drawText ("Waiting for voice...", top.removeFromTop (16.0f), juce::Justification::centredLeft);
            top.removeFromTop (4.0f);
            const int eng = tune->engine.get();
            const double srr = tune->currentRate();
            const double lat = SqTunePlugin::latencyFor (eng, tune->voiceType.get(), srr);
            auto badge = top.removeFromTop (20.0f);
            g.setColour (eng == 0 ? juce::Colour (0xffff3ad1) : juce::Colour (0xff3ad1ff)); g.fillRoundedRectangle (badge.removeFromLeft (64.0f), 4.0f);
            g.setColour (juce::Colours::black); g.setFont (Theme::font (9.5f, true));
            g.drawText (eng == 0 ? "LIVE" : "STUDIO", badge.withWidth (64.0f).withX (ra.getX() + 10.0f), juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.8f)); g.setFont (Theme::font (9.5f));
            g.drawText ("  " + juce::String (lat * 1000.0 / srr, 2) + " ms  " + (tune->character.get() == 1 ? "CLASSIC" : "MODERN"), badge, juce::Justification::centredLeft);
            top.removeFromTop (4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.0f));
            const int sc = tune->scaleIndex.get();
            g.drawFittedText ("Key: " + SqScales::noteNames()[root] + " " + SqTunePlugin::scaleNames()[sc] + (tune->autoKey.get() ? "  (Auto-Key)" : "")
                              + (tune->midiMode.get() != 0 ? "\nTargets: MIDI notes" : ""), top.removeFromTop (28.0f).toNearestInt(), juce::Justification::topLeft, 2);

            // 78-note range strip (C1..F7): where the voice and target sit
            auto rg = rangeArea();
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (rg, 3.0f);
            const int first = 24, last = 101;
            const float kw = rg.getWidth() / (float) (last - first + 1);
            for (int n = first; n <= last; ++n)
            {
                const int pc = n % 12; const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
                auto kr = juce::Rectangle<float> (rg.getX() + (float) (n - first) * kw, rg.getY(), kw, rg.getHeight()).reduced (0.5f, black ? 4.0f : 1.0f);
                juce::Colour c = black ? juce::Colour (0xff1a1a22) : juce::Colour (0xff2c2c36);
                if ((mask >> pc) & 1) c = c.interpolatedWith (acc, 0.25f);
                if (in > 0 && juce::roundToInt (in) == n) c = juce::Colours::white;
                if (tgt > 0 && juce::roundToInt (tgt) == n) c = acc;
                g.setColour (c); g.fillRect (kr);
            }
            // one-octave scale editor
            auto ka = keysArea();
            const float bw = ka.getWidth() / 12.0f;
            const int heldMask = tune->heldMaskUi.load();
            for (int pc = 0; pc < 12; ++pc)
            {
                auto kr = juce::Rectangle<float> (ka.getX() + (float) pc * bw, ka.getY(), bw, ka.getHeight()).reduced (2.0f, 0.0f);
                const bool inScale = (mask >> pc) & 1, removed = (rem >> pc) & 1, bypass = (byp >> pc) & 1;
                const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
                juce::Colour fill = black ? juce::Colour (0xff15151c) : juce::Colour (0xff24242e);
                if (inScale) fill = acc.withMultipliedBrightness (pc == root ? 1.0f : 0.72f);
                if (removed) fill = juce::Colour (0xff4a1420);
                if (bypass) fill = juce::Colour (0xffb8781c);
                g.setColour (fill); g.fillRoundedRectangle (kr, 4.0f);
                if (in > 0 && (((juce::roundToInt (in) % 12) + 12) % 12) == pc) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (kr.reduced (1.0f), 4.0f, 2.0f); }
                if ((heldMask >> pc) & 1) { g.setColour (juce::Colour (0xff3ad1ff)); g.fillEllipse (kr.getCentreX() - 3.0f, kr.getBottom() - 9.0f, 6.0f, 6.0f); }
                g.setColour (inScale && ! removed ? juce::Colours::black : juce::Colours::white.withAlpha (0.75f));
                g.setFont (Theme::font (11.0f, true));
                g.drawText (SqScales::noteNames()[pc], kr.removeFromTop (kr.getHeight() * 0.55f), juce::Justification::centred);
                g.setFont (Theme::font (8.5f, true));
                g.drawText (removed ? "REMOVED" : bypass ? "BYPASS" : inScale ? (pc == root ? "ROOT" : "ON") : "off", kr, juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (tune == nullptr) return;
            auto ka = keysArea();
            if (! ka.contains (e.position)) return;
            const int pc = juce::jlimit (0, 11, (int) ((e.position.x - ka.getX()) / (ka.getWidth() / 12.0f)));
            auto* um = &tune->edit.getUndoManager();
            if (e.mods.isPopupMenu() || e.mods.isShiftDown())
                tune->bypassMask.setValue (tune->bypassMask.get() ^ (1 << pc), um);
            else if (tune->scaleIndex.get() >= (int) SqScales::scales().size())
                tune->customMask.setValue (tune->customMask.get() ^ (1 << pc), um);
            else
                tune->removeMask.setValue (tune->removeMask.get() ^ (1 << pc), um);
            repaint();
            if (onChange) onChange();
        }
    };

    /** Moth Tune FX: the editable Motion pattern (drag bars, right-click = rest) and the six-slot rack (click the
        name to choose a module, drag the X/Y pad, drag the mix bar, arrows re-order, lamp = on/off). */
    struct TuneFxViz : public Viz
    {
        TuneFxViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), fx (dynamic_cast<SqTuneFxPlugin*> (&p)) { setInterceptsMouseClicks (true, false); }
        SqTuneFxPlugin* fx;
        std::function<void()> onChange;
        static constexpr int N = SqTuneFxPlugin::numSlots;
        const juce::Colour slotCol[N] { juce::Colour (0xffff3ad1), juce::Colour (0xff3ad1ff), juce::Colour (0xffb6ff3a), juce::Colour (0xffffa13a), juce::Colour (0xff9d6bff), juce::Colour (0xff3affb0) };

        juce::Rectangle<float> motionArea() const { return getLocalBounds().toFloat().reduced (8.0f).removeFromTop (150.0f); }
        juce::Rectangle<float> rackArea() const   { auto r = getLocalBounds().toFloat().reduced (8.0f); r.removeFromTop (158.0f); return r; }
        juce::Rectangle<float> card (int i) const { auto r = rackArea(); const float w = (r.getWidth() - 6.0f * (N - 1)) / N; return { r.getX() + (float) i * (w + 6.0f), r.getY(), w, r.getHeight() }; }
        juce::Rectangle<float> titleR (int i) const { return card (i).reduced (6.0f).removeFromTop (22.0f); }
        juce::Rectangle<float> padR (int i) const   { auto c = card (i).reduced (6.0f); c.removeFromTop (26.0f); c.removeFromBottom (34.0f); return c; }
        juce::Rectangle<float> mixR (int i) const   { auto c = card (i).reduced (6.0f); return c.removeFromBottom (28.0f).withTrimmedTop (8.0f); }
        juce::Rectangle<float> stepR (int k) const
        {
            auto m = motionArea().reduced (10.0f, 22.0f).withTrimmedLeft (34.0f).withTrimmedRight (160.0f);
            const float w = m.getWidth() / SqTuneFxPlugin::maxSteps;
            return { m.getX() + (float) k * w, m.getY(), w, m.getHeight() };
        }

        void paint (juce::Graphics& g) override
        {
            if (fx == nullptr) return;
            const auto acc = neon (accent);
            // ---- motion editor
            auto m = motionArea();
            g.setGradientFill (juce::ColourGradient (acc.darker (2.0f).withAlpha (0.92f), m.getX(), m.getY(), juce::Colour (0xff0b0716).withAlpha (0.92f), m.getRight(), m.getBottom(), false));
            g.fillRoundedRectangle (m, 6.0f);
            g.setColour (acc.withAlpha (0.4f)); g.drawRoundedRectangle (m, 6.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.75f)); g.setFont (Theme::font (10.0f, true));
            g.drawText ("MOTION PATTERN  -  drag steps up/down, right-click = rest", m.reduced (10.0f, 4.0f).removeFromTop (14.0f), juce::Justification::left);
            const auto st = fx->parsedSteps();
            const int len = juce::jlimit (1, SqTuneFxPlugin::maxSteps, fx->motionLen.get()), cur = fx->currentStep.load();
            const float range = 12.0f;
            auto first = stepR (0);
            const float midY = first.getCentreY();
            g.setColour (juce::Colours::white.withAlpha (0.15f)); g.drawHorizontalLine ((int) midY, first.getX(), stepR (SqTuneFxPlugin::maxSteps - 1).getRight());
            g.setFont (Theme::font (8.5f)); g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawText ("+12", (int) m.getX() + 6, (int) first.getY() - 2, 28, 10, juce::Justification::left);
            g.drawText ("0", (int) m.getX() + 6, (int) midY - 5, 28, 10, juce::Justification::left);
            g.drawText ("-12", (int) m.getX() + 6, (int) first.getBottom() - 8, 28, 10, juce::Justification::left);
            for (int k = 0; k < SqTuneFxPlugin::maxSteps; ++k)
            {
                auto r = stepR (k).reduced (2.0f, 0.0f);
                const bool active = k < len, rest = std::isnan (st[(size_t) k]);
                g.setColour (juce::Colours::black.withAlpha (active ? 0.35f : 0.15f)); g.fillRoundedRectangle (r, 3.0f);
                if (k == cur) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRoundedRectangle (r, 3.0f); }
                if (! active) continue;
                if (rest) { g.setColour (juce::Colours::white.withAlpha (0.4f)); g.setFont (Theme::font (12.0f, true)); g.drawText ("x", r, juce::Justification::centred); continue; }
                const float v = juce::jlimit (-range, range, st[(size_t) k]);
                const float y = midY - v / range * r.getHeight() * 0.5f;
                auto bar = juce::Rectangle<float> (r.getX() + 3.0f, juce::jmin (y, midY), r.getWidth() - 6.0f, juce::jmax (3.0f, std::abs (y - midY)));
                g.setGradientFill (juce::ColourGradient (acc.brighter (0.3f), bar.getX(), bar.getY(), acc.darker (0.4f), bar.getX(), bar.getBottom(), false));
                g.fillRoundedRectangle (bar, 2.0f);
                if (k == cur) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (bar.expanded (1.0f), 2.0f, 1.5f); }
                g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (Theme::font (9.0f, true));
                g.drawText (juce::String (juce::roundToInt (v)), r.withY (r.getBottom() + 2.0f).withHeight (12.0f), juce::Justification::centred);
            }
            // live note readout
            auto ro = m.reduced (10.0f).removeFromRight (150.0f).withTrimmedTop (16.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("TUNED NOTE", ro.removeFromTop (14.0f), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white); g.setFont (Theme::font (40.0f, true));
            g.drawText (noteLabel (fx->liveNote.load()), ro.removeFromTop (46.0f), juce::Justification::centredLeft);
            g.setFont (Theme::font (10.0f)); g.setColour (acc);
            g.drawText (cur >= 0 ? "MOTION  step " + juce::String (cur + 1) + "/" + juce::String (len) : "MOTION  idle", ro.removeFromTop (16.0f), juce::Justification::centredLeft);

            // ---- rack
            for (int i = 0; i < N; ++i)
            {
                auto c = card (i);
                const int type = fx->slotType[(size_t) i].get();
                const bool on = fx->slotOn[(size_t) i].get();
                const auto col = neon (slotCol[i]);
                g.setGradientFill (juce::ColourGradient (col.darker (type != 0 && on ? 1.6f : 2.6f).withAlpha (0.95f), c.getX(), c.getY(), juce::Colour (0xff0b0716).withAlpha (0.95f), c.getRight(), c.getBottom(), false));
                g.fillRoundedRectangle (c, 8.0f);
                g.setColour (col.withAlpha (type != 0 && on ? 0.85f : 0.3f)); g.drawRoundedRectangle (c, 8.0f, 1.5f);
                auto t = titleR (i);
                // arrows + lamp
                g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (11.0f, true));
                g.drawText ("<", t.removeFromLeft (14.0f), juce::Justification::centred);
                g.drawText (">", t.removeFromRight (14.0f), juce::Justification::centred);
                auto lamp = t.removeFromRight (16.0f).withSizeKeepingCentre (10.0f, 10.0f);
                g.setColour (on ? col : juce::Colours::black); g.fillEllipse (lamp);
                g.setColour (col); g.drawEllipse (lamp, 1.0f);
                g.setColour (type != 0 ? juce::Colours::white : juce::Colours::white.withAlpha (0.4f)); g.setFont (Theme::font (11.0f, true));
                g.drawFittedText (juce::String (i + 1) + "  " + SqVocalFx::moduleNames()[type] + "  v", t.toNearestInt(), juce::Justification::centred, 1);
                // pad
                auto pd = padR (i);
                g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (pd, 5.0f);
                g.setColour (col.withAlpha (0.12f));
                for (int k = 1; k < 4; ++k) { g.drawVerticalLine ((int) (pd.getX() + pd.getWidth() * (float) k / 4.0f), pd.getY(), pd.getBottom()); g.drawHorizontalLine ((int) (pd.getY() + pd.getHeight() * (float) k / 4.0f), pd.getX(), pd.getRight()); }
                const float xv = fx->bank[(size_t) (SqTuneFxPlugin::s1x + i * 3)], yv = fx->bank[(size_t) (SqTuneFxPlugin::s1y + i * 3)];
                const auto dot = juce::Point<float> (pd.getX() + xv * pd.getWidth(), pd.getBottom() - yv * pd.getHeight());
                if (type != 0)
                {
                    const float act = juce::jlimit (0.0f, 1.0f, fx->slotActivity[(size_t) i].load() * 3.0f);
                    g.setColour (col.withAlpha (0.18f + 0.25f * act)); g.fillEllipse (juce::Rectangle<float> (34.0f + act * 20.0f, 34.0f + act * 20.0f).withCentre (dot));
                    g.setColour (col.withAlpha (0.5f)); g.drawLine (dot.x, pd.getBottom(), dot.x, dot.y, 1.0f); g.drawLine (pd.getX(), dot.y, dot.x, dot.y, 1.0f);
                    g.setColour (col); g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (dot));
                    g.setColour (juce::Colours::white); g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (dot), 1.5f);
                    auto ax = SqVocalFx::axisNames (type);
                    g.setColour (juce::Colours::white.withAlpha (0.65f)); g.setFont (Theme::font (8.5f, true));
                    g.drawText (juce::String (ax.first) + " " + juce::String (juce::roundToInt (xv * 100)) + "%", pd.reduced (4.0f).removeFromBottom (11.0f), juce::Justification::right);
                    g.drawText (juce::String (ax.second) + " " + juce::String (juce::roundToInt (yv * 100)) + "%", pd.reduced (4.0f).removeFromTop (11.0f), juce::Justification::left);
                }
                else
                {
                    g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (Theme::font (10.0f));
                    g.drawFittedText ("Click the name to\nload a module", pd.toNearestInt(), juce::Justification::centred, 2);
                }
                // mix bar
                auto mb = mixR (i);
                const float mv = fx->bank[(size_t) (SqTuneFxPlugin::s1mix + i * 3)] / 100.0f;
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (mb, 4.0f);
                g.setColour (col.withAlpha (type != 0 ? 0.85f : 0.3f)); g.fillRoundedRectangle (mb.withWidth (mb.getWidth() * mv), 4.0f);
                g.setColour (juce::Colours::white); g.setFont (Theme::font (9.0f, true));
                g.drawText ("MIX " + juce::String (juce::roundToInt (mv * 100)) + "%", mb, juce::Justification::centred);
            }
        }

        // ---- interaction
        enum Drag { none, step, pad, mixBar } drag = none;
        int dragIndex = -1;
        te::AutomatableParameter* gx = nullptr; te::AutomatableParameter* gy = nullptr;

        void setStepFromY (int k, float y)
        {
            auto r = stepR (k);
            const float v = juce::jlimit (-12.0f, 12.0f, std::round ((r.getCentreY() - y) / (r.getHeight() * 0.5f) * 12.0f));
            fx->setStep (k, v, false, &fx->edit.getUndoManager());
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (fx == nullptr) return;
            auto* um = &fx->edit.getUndoManager();
            um->beginNewTransaction ("Moth Tune FX");
            for (int k = 0; k < SqTuneFxPlugin::maxSteps; ++k)
                if (stepR (k).contains (e.position))
                {
                    if (k >= fx->motionLen.get()) fx->motionLen.setValue (k + 1, um);
                    if (e.mods.isPopupMenu()) { const auto st = fx->parsedSteps(); fx->setStep (k, 0, ! std::isnan (st[(size_t) k]), um); }
                    else { drag = step; setStepFromY (k, e.position.y); }
                    changed(); return;
                }
            for (int i = 0; i < N; ++i)
            {
                auto t = titleR (i);
                if (t.contains (e.position))
                {
                    if (e.position.x < t.getX() + 14.0f) { fx->swapSlots (i, i - 1, um); changed(); return; }
                    if (e.position.x > t.getRight() - 14.0f) { fx->swapSlots (i, i + 1, um); changed(); return; }
                    if (e.position.x > t.getRight() - 30.0f) { fx->slotOn[(size_t) i].setValue (! fx->slotOn[(size_t) i].get(), um); changed(); return; }
                    juce::PopupMenu menu;
                    for (int m = 0; m < SqVocalFx::moduleNames().size(); ++m) menu.addItem (m + 1, SqVocalFx::moduleNames()[m], true, m == fx->slotType[(size_t) i].get());
                    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (t.toNearestInt())), [this, i] (int r)
                    {
                        if (r > 0 && fx != nullptr) { fx->edit.getUndoManager().beginNewTransaction ("Moth Tune FX module"); fx->slotType[(size_t) i].setValue (r - 1, &fx->edit.getUndoManager()); fx->slotOn[(size_t) i].setValue (true, &fx->edit.getUndoManager()); changed(); }
                    });
                    return;
                }
                if (padR (i).contains (e.position))
                {
                    drag = pad; dragIndex = i;
                    gx = fx->bank.params[(size_t) (SqTuneFxPlugin::s1x + i * 3)].get(); gy = fx->bank.params[(size_t) (SqTuneFxPlugin::s1y + i * 3)].get();
                    gx->parameterChangeGestureBegin(); gy->parameterChangeGestureBegin();
                    mouseDrag (e); return;
                }
                if (mixR (i).contains (e.position))
                {
                    drag = mixBar; dragIndex = i;
                    gx = fx->bank.params[(size_t) (SqTuneFxPlugin::s1mix + i * 3)].get(); gx->parameterChangeGestureBegin();
                    mouseDrag (e); return;
                }
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (fx == nullptr) return;
            if (drag == step)
            {
                for (int k = 0; k < SqTuneFxPlugin::maxSteps; ++k)
                    if (e.position.x >= stepR (k).getX() && e.position.x < stepR (k).getRight() && k < fx->motionLen.get()) setStepFromY (k, e.position.y);
            }
            else if (drag == pad && gx != nullptr)
            {
                auto pd = padR (dragIndex);
                gx->setParameter (juce::jlimit (0.0f, 1.0f, (e.position.x - pd.getX()) / pd.getWidth()), juce::sendNotification);
                gy->setParameter (juce::jlimit (0.0f, 1.0f, (pd.getBottom() - e.position.y) / pd.getHeight()), juce::sendNotification);
            }
            else if (drag == mixBar && gx != nullptr)
            {
                auto mb = mixR (dragIndex);
                gx->setParameter (juce::jlimit (0.0f, 100.0f, (e.position.x - mb.getX()) / mb.getWidth() * 100.0f), juce::sendNotification);
            }
            changed();
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (drag == pad && gx != nullptr) { gx->parameterChangeGestureEnd(); gy->parameterChangeGestureEnd(); }
            if (drag == mixBar && gx != nullptr) gx->parameterChangeGestureEnd();
            drag = none; gx = gy = nullptr;
        }
        void changed() { repaint(); if (onChange) onChange(); }
    };
