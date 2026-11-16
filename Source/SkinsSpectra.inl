// Included inside namespace Skins (PluginSkins.h): the Spectra EQ face (1.14 Studio Line).
//
// Squelchmoth's own "night moth" design - the whole window is one instrument:
//  - a midnight field with a soft vignette; octave / decade lines barely there; a frequency ruler and a dB scale
//  - the analyser as an iridescent wing (blue -> violet -> magenta -> amber across the spectrum), pre as a
//    faint line, the key in amber; 4.5 dB/oct display tilt; freeze
//  - every band's own shape faintly behind; the selected band stronger; the total curve luminous (halo, body,
//    white core) with what it boosts and cuts lightly shaded
//  - glowing nodes; hover or drag shows a floating card (shape, frequency, gain, Q); the selected bell shows its
//    bandwidth with two Q handles; dynamic bands show a ghost node where they are right now and their range as an arc
//  - top bar: the three phase modes as one segmented control, analyser / range / auto gain chips, an OUT chip you
//    drag for output gain, latency; a slim output meter on the right edge
//  - the band strip: one card per setting of the selected band (with a drawing of its shape), arrows to step bands
// Mouse: drag a node = frequency + gain (Shift = fine), wheel = Q, drag a Q handle = bandwidth, double-click empty
// space = new band, double-click a node = remove, right-click = band / display menus, Delete = remove, arrows = step.
// Every edit is one undo step through the same parameters Claude and automation use.

    struct SpectraViz : public Viz, private juce::Timer
    {
        SpectraViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), eq (dynamic_cast<SqSpectraEqPlugin*> (&p)), fft (13)
        {
            setInterceptsMouseClicks (true, false);
            setWantsKeyboardFocus (true);
            preDb.fill (-120.0f); postDb.fill (-120.0f); keyDb.fill (-120.0f);
            // open with the first band selected, so the band strip shows its settings straight away
            if (eq != nullptr) for (int i = 0; i < SqSpectraEqPlugin::numBands; ++i) if (eq->props[(size_t) i].on.get()) { selected = i; break; }
            startTimerHz (30);
        }
        SqSpectraEqPlugin* eq;
        juce::dsp::FFT fft;
        static constexpr int fftN = 8192, cols = 520;
        std::array<float, fftN * 2> work {};
        std::array<float, cols> preDb {}, postDb {}, keyDb {};
        std::function<void()> onChange;
        int selected = -1, hovered = -1, dragging = -1, stripDrag = -1, qDrag = 0; bool frozen = false, outDrag = false;
        juce::Point<float> dragStart; float dragStartVal = 0, dragStartFreq = 0, dragStartGain = 0;
        float meterDb = -100.0f, meterPeak = -100.0f; int peakHold = 0;

        // ---- palette
        static juce::Colour ink()        { return juce::Colour (0xff07050f); }
        static juce::Colour field()      { return juce::Colour (0xff120e24); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xffe9e4ff).withAlpha (a); }
        static juce::Colour bandColour (int i)
        {
            static const uint32_t c[] = { 0xff5ab4ff, 0xffb46bff, 0xffff6bc8, 0xffffa25a, 0xff5affc8, 0xffffe05a, 0xff6b7dff, 0xffff5a7a,
                                          0xff5ae8ff, 0xffd98bff, 0xff9dff5a, 0xffff8b5a, 0xff5a8bff, 0xffff5ae0, 0xff5affa0, 0xffffc25a,
                                          0xff8b5aff, 0xff5ad0ff, 0xffff7aa0, 0xffc8ff5a, 0xff7a5aff, 0xff5affe8, 0xffffb08b, 0xffa07aff };
            return juce::Colour (c[i % 24]);
        }
        /** The wing gradient for the analyser, across the plot. */
        static juce::ColourGradient wing (juce::Rectangle<float> r, float alpha)
        {
            juce::ColourGradient g (juce::Colour (0xff3a6bff).withAlpha (alpha), r.getX(), 0, juce::Colour (0xffffb45a).withAlpha (alpha), r.getRight(), 0, false);
            g.addColour (0.33, juce::Colour (0xff8a5cff).withAlpha (alpha));
            g.addColour (0.62, juce::Colour (0xffe65ce0).withAlpha (alpha));
            g.addColour (0.85, juce::Colour (0xffff7a8a).withAlpha (alpha));
            return g;
        }

        // ---- geometry: 10 Hz ... 30 kHz, +/- range dB
        juce::Rectangle<float> frame() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const    { return frame().reduced (14.0f, 0).removeFromTop (46.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> strip() const     { return frame().reduced (14.0f, 0).removeFromBottom (86.0f).withTrimmedBottom (12.0f); }
        juce::Rectangle<float> plotArea() const  { auto r = frame().reduced (14.0f, 0); r.removeFromTop (48.0f); r.removeFromBottom (92.0f); return r; }
        juce::Rectangle<float> plot() const      { auto r = plotArea(); r.removeFromBottom (22.0f); r.removeFromRight (58.0f); r.removeFromLeft (8.0f); return r.withTrimmedTop (6.0f); }
        juce::Rectangle<float> meterRect() const { auto r = plotArea(); r.removeFromBottom (22.0f); return r.removeFromRight (14.0f).withTrimmedTop (6.0f); }
        static float xFor (float hz, juce::Rectangle<float> r)  { return r.getX() + r.getWidth() * (float) (std::log (hz / 10.0) / std::log (3000.0)); }
        static float hzFor (float x, juce::Rectangle<float> r)  { return (float) (10.0 * std::pow (3000.0, (x - r.getX()) / r.getWidth())); }
        float range() const { return eq != nullptr ? SqSpectraEqPlugin::rangeDb (eq->displayRange.get()) : 12.0f; }
        float yFor (float db, juce::Rectangle<float> r) const { return r.getCentreY() - juce::jlimit (-1.08f, 1.08f, db / range()) * r.getHeight() * 0.5f; }
        float dbFor (float y, juce::Rectangle<float> r) const { return (r.getCentreY() - y) / (r.getHeight() * 0.5f) * range(); }
        static float ySpec (float dbfs, juce::Rectangle<float> r) { return r.getY() + juce::jlimit (0.0f, 1.0f, (-dbfs - 6.0f) / 90.0f) * r.getHeight(); }

        juce::Point<float> nodePos (int i) const
        {
            const auto r = plot(); const auto b = eq->band (i);
            const float x = xFor ((float) juce::jlimit (10.0, 30000.0, b.freq), r);
            const float db = b.hasGain() && b.shape != SqSpectra::Shape::flatTilt ? (float) b.gainDb : 0.0f;
            return { x, yFor (db, r) };
        }
        int hitNode (juce::Point<float> p) const
        {
            if (eq == nullptr) return -1;
            int best = -1; float bd = 14.0f;
            for (int i = 0; i < SqSpectraEqPlugin::numBands; ++i)
                if (eq->props[(size_t) i].on.get()) { const float d = nodePos (i).getDistanceFrom (p); if (d < bd) { bd = d; best = i; } }
            return best;
        }
        /** The selected bell / notch / band pass: its bandwidth edges from Q. */
        bool qHandles (juce::Point<float>& lo, juce::Point<float>& hi) const
        {
            if (selected < 0 || ! eq->props[(size_t) selected].on.get()) return false;
            const auto b = eq->band (selected);
            if (! (b.shape == SqSpectra::Shape::bell || b.shape == SqSpectra::Shape::notch || b.shape == SqSpectra::Shape::bandPass)) return false;
            const double bwOct = 2.0 * std::asinh (1.0 / (2.0 * b.q)) / std::log (2.0);
            const auto r = plot(); const auto n = nodePos (selected);
            lo = { xFor ((float) (b.freq / std::pow (2.0, bwOct / 2)), r), n.y }; hi = { xFor ((float) (b.freq * std::pow (2.0, bwOct / 2)), r), n.y };
            return true;
        }

        // ---- parameter plumbing (undoable, gestures for automation)
        te::AutomatableParameter& param (int band, int which) const { return *eq->bank.params[(size_t) (band * SqSpectraEqPlugin::perBand + which)]; }
        te::AutomatableParameter& outParam() const { return *eq->bank.params[(size_t) SqSpectraEqPlugin::outputParam]; }
        juce::UndoManager* um() const { return &eq->edit.getUndoManager(); }
        void setP (int band, int which, float v) { auto& p = param (band, which); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); changed(); }
        void changed() { repaint(); if (onChange) onChange(); }

        // ---- analyser and meter
        void timerCallback() override
        {
            if (eq == nullptr) return;
            if (! frozen && eq->analyser.get() > 0)
            {
                analyse (eq->post, eq->postW.load(), postDb);
                if (eq->analyser.get() >= 2) analyse (eq->pre, eq->preW.load(), preDb);
                if (eq->analyser.get() >= 3) analyse (eq->keyScope, eq->keyW.load(), keyDb);
            }
            const float lv = eq->levelOut.load(), db = lv > 0 ? 20.0f * std::log10 (lv) : -100.0f;
            meterDb = db > meterDb ? db : meterDb - 1.2f;
            if (db >= meterPeak) { meterPeak = db; peakHold = 45; } else if (--peakHold < 0) meterPeak -= 0.8f;
            repaint();
        }
        void analyse (const std::array<float, SqSpectraEqPlugin::scopeLen>& src, int w, std::array<float, cols>& out)
        {
            for (int i = 0; i < fftN; ++i)
            {
                const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftN - 1));
                work[(size_t) i] = src[(size_t) ((w - fftN + i) & (SqSpectraEqPlugin::scopeLen - 1))] * win;
            }
            std::fill (work.begin() + fftN, work.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (work.data());
            const double sr = eq->rate();
            for (int c = 0; c < cols; ++c)
            {
                // each column covers a log slice: the strongest bin in it (a narrow peak is never missed); below the bin
                // spacing the bins are interpolated so the low end is a smooth line, not steps
                const double f0 = 10.0 * std::pow (3000.0, c / (double) cols), f1 = 10.0 * std::pow (3000.0, (c + 1) / (double) cols);
                const double b0 = f0 * fftN / sr, b1 = f1 * fftN / sr;
                float m = 0;
                if (b1 - b0 < 1.0)
                {
                    const double bc = juce::jlimit (1.0, fftN / 2.0 - 2.0, 0.5 * (b0 + b1)); const int k = (int) bc; const float t = (float) (bc - k);
                    m = work[(size_t) k] * (1 - t) + work[(size_t) k + 1] * t;
                }
                else for (int k = juce::jlimit (1, fftN / 2 - 1, (int) b0); k <= juce::jlimit (1, fftN / 2 - 1, (int) std::ceil (b1)); ++k) m = juce::jmax (m, work[(size_t) k]);
                const double fc = std::sqrt (f0 * f1);
                const float db = 20.0f * std::log10 (m * 4.0f / fftN + 1e-9f) + 4.5f * (float) std::log2 (fc / 1000.0);
                auto& o = out[(size_t) c];
                o += (db > o ? 0.55f : 0.10f) * (db - o);
            }
        }

        // ---- painting
        void paint (juce::Graphics& g) override
        {
            if (eq == nullptr) return;
            const auto acc = neon (accent);
            const auto all = frame();
            g.setGradientFill (juce::ColourGradient (field().brighter (0.12f), all.getCentreX(), all.getCentreY() - all.getHeight() * 0.1f, ink(), all.getX(), all.getBottom(), true));
            g.fillRoundedRectangle (all, 10.0f);
            // the Higgsfield plate (iridescent wing scales) glowing through the corners, darkened where the curve lives
            {
                static const juce::Image plate = artFor ("spectraeq");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (all, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.62f);
                    g.drawImage (plate, all, juce::RectanglePlacement::fillDestination);
                    g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (ink().withAlpha (0.78f), all.getCentreX(), all.getCentreY(), ink().withAlpha (0.25f), all.getX(), all.getY(), true));
                    g.fillRect (all);
                }
            }
            g.setColour (acc.withAlpha (0.16f)); g.drawRoundedRectangle (all.reduced (0.5f), 10.0f, 1.0f);
            const auto r = plot();
            // a dark glass pane under the display, so the curve and the scale read cleanly over the plate
            {
                auto pane = plotArea().withTrimmedBottom (0.0f).expanded (0, 2.0f);
                g.setColour (ink().withAlpha (0.62f)); g.fillRoundedRectangle (pane, 10.0f);
                g.setGradientFill (juce::ColourGradient (text (0.05f), 0, pane.getY(), text (0.0f), 0, pane.getY() + 40.0f, false));
                g.fillRoundedRectangle (pane, 10.0f);
                g.setColour (text (0.08f)); g.drawRoundedRectangle (pane, 10.0f, 1.0f);
            }
            paintGrid (g, r);
            paintAnalyser (g, r);
            {
                juce::Graphics::ScopedSaveState clip (g);
                g.reduceClipRegion (r.toNearestInt().expanded (0, 3));
                paintBands (g, r);
                paintCurve (g, r, acc);
            }
            paintNodes (g, r);
            paintMeter (g, acc);
            paintTopBar (g, acc);
            if (countOn() == 0) paintEmpty (g, r);
            paintStrip (g);
            if (const int card = dragging >= 0 ? dragging : hovered; card >= 0 && eq->props[(size_t) card].on.get()) paintCard (g, card, r);
        }
        int countOn() const { int n = 0; for (auto& p : eq->props) if (p.on.get()) ++n; return n; }

        void paintGrid (juce::Graphics& g, juce::Rectangle<float> r)
        {
            for (float f : { 20.0f, 40.0f, 80.0f, 160.0f, 315.0f, 630.0f, 1250.0f, 2500.0f, 5000.0f, 20000.0f })
            { g.setColour (text (0.035f)); g.drawVerticalLine (juce::roundToInt (xFor (f, r)), r.getY(), r.getBottom()); }
            for (float f : { 100.0f, 1000.0f, 10000.0f })
            { g.setColour (text (0.08f)); g.drawVerticalLine (juce::roundToInt (xFor (f, r)), r.getY(), r.getBottom()); }
            const float rg = range(), step = rg <= 3 ? 1.0f : rg <= 6 ? 2.0f : rg <= 12 ? 3.0f : 6.0f;
            g.setFont (Theme::font (9.0f, true));
            for (float d = -rg; d <= rg + 0.01f; d += step)
            {
                const float y = yFor (d, r);
                const bool zero = std::abs (d) < 0.01f;
                g.setColour (text (zero ? 0.16f : 0.04f)); g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getRight());
                g.setColour (text (zero ? 0.75f : 0.38f));
                g.drawText (zero ? juce::String ("0 dB") : (d > 0 ? "+" : "") + juce::String ((int) d), juce::roundToInt (r.getRight() + 6), juce::roundToInt (y) - 6, 34, 12, juce::Justification::left);
            }
            auto ruler = juce::Rectangle<float> (r.getX(), r.getBottom() + 4.0f, r.getWidth(), 16.0f);
            g.setColour (text (0.05f)); g.fillRoundedRectangle (ruler, 4.0f);
            for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
            {
                const float x = xFor (f, r);
                g.setColour (text (f == 100.0f || f == 1000.0f || f == 10000.0f ? 0.8f : 0.45f));
                g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::roundToInt (x) - 18, juce::roundToInt (ruler.getY()), 36, 16, juce::Justification::centred);
            }
        }

        juce::Path specPath (const std::array<float, cols>& d, juce::Rectangle<float> r) const
        {
            juce::Path p;
            for (int c = 0; c < cols; ++c)
            {
                const float x = r.getX() + r.getWidth() * (c + 0.5f) / cols, y = ySpec (d[(size_t) c], r);
                if (c == 0) p.startNewSubPath (r.getX(), y);
                p.lineTo (x, y);
            }
            return p;
        }
        void paintAnalyser (juce::Graphics& g, juce::Rectangle<float> r)
        {
            if (eq->analyser.get() == 0) return;
            auto post = specPath (postDb, r);
            juce::Path fill (post); fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
            g.setGradientFill (wing (r, 0.30f)); g.fillPath (fill);
            g.setGradientFill (juce::ColourGradient (field().withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.35f, field().withAlpha (0.92f), 0, r.getBottom(), false));
            g.fillPath (fill);
            g.setGradientFill (wing (r, 0.75f)); g.strokePath (post, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));
            if (eq->analyser.get() >= 2) { g.setColour (text (0.20f)); g.strokePath (specPath (preDb, r), juce::PathStrokeType (1.0f, juce::PathStrokeType::curved)); }
            if (eq->analyser.get() >= 3) { g.setColour (juce::Colour (0xffffb43a).withAlpha (0.55f)); g.strokePath (specPath (keyDb, r), juce::PathStrokeType (1.0f)); }
            if (frozen) { g.setColour (juce::Colour (0xffffd23a).withAlpha (0.9f)); g.setFont (Theme::font (9.5f, true)); g.drawText ("ANALYSER FROZEN", r.reduced (10.0f).removeFromTop (14.0f), juce::Justification::right); }
        }

        juce::Path bandPath (int i, juce::Rectangle<float> r, int points) const
        {
            juce::Path p;
            for (int k = 0; k <= points; ++k)
            {
                const float x = r.getX() + r.getWidth() * k / (float) points, y = yFor ((float) eq->bandDb (i, hzFor (x, r)), r);
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        }
        void paintBands (juce::Graphics& g, juce::Rectangle<float> r)
        {
            for (int i = 0; i < SqSpectraEqPlugin::numBands; ++i)
            {
                if (! eq->props[(size_t) i].on.get()) continue;
                const bool sel = i == selected, hov = i == hovered;
                const auto col = bandColour (i);
                auto p = bandPath (i, r, sel ? 300 : 160);
                juce::Path f (p); f.lineTo (r.getRight(), r.getCentreY()); f.lineTo (r.getX(), r.getCentreY()); f.closeSubPath();
                const auto sh = eq->band (i).shape;
                const bool wide = sh == SqSpectra::Shape::lowCut || sh == SqSpectra::Shape::highCut || sh == SqSpectra::Shape::bandPass;   // big areas: lighter
                g.setColour (col.withAlpha ((sel ? 0.20f : hov ? 0.12f : 0.06f) * (wide ? 0.45f : 1.0f))); g.fillPath (f);
                g.setColour (col.withAlpha (sel ? 0.85f : hov ? 0.5f : 0.22f)); g.strokePath (p, juce::PathStrokeType (sel ? 1.4f : 1.0f));
            }
        }
        void paintCurve (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour acc)
        {
            juce::Path p;
            for (int i = 0; i <= 720; ++i)
            {
                const float x = r.getX() + r.getWidth() * i / 720.0f, y = yFor ((float) eq->curveDb (hzFor (x, r)), r);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            juce::Path area (p); area.lineTo (r.getRight(), r.getCentreY()); area.lineTo (r.getX(), r.getCentreY()); area.closeSubPath();
            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (r.withBottom (r.getCentreY()).toNearestInt());
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff8adf).withAlpha (0.16f), 0, r.getY(), juce::Colour (0xffff8adf).withAlpha (0.02f), 0, r.getCentreY(), false));
                g.fillPath (area);
            }
            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (r.withTop (r.getCentreY()).toNearestInt());
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5ab4ff).withAlpha (0.02f), 0, r.getCentreY(), juce::Colour (0xff5ab4ff).withAlpha (0.16f), 0, r.getBottom(), false));
                g.fillPath (area);
            }
            g.setColour (acc.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (acc.withAlpha (0.28f)); g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (acc.brighter (0.6f)); g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::white.withAlpha (0.85f)); g.strokePath (p, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        void paintNodes (juce::Graphics& g, juce::Rectangle<float> r)
        {
            juce::Point<float> lo, hi;
            if (qHandles (lo, hi))
            {
                const auto col = bandColour (selected);
                g.setColour (col.withAlpha (0.55f));
                const float dash[] = { 3.0f, 3.0f }; juce::Path line; line.startNewSubPath (lo); line.lineTo (hi);
                juce::Path dashed; juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dash, 2); g.fillPath (dashed);
                for (auto pt : { lo, hi }) { g.setColour (ink()); g.fillRoundedRectangle (pt.x - 3.0f, pt.y - 7.0f, 6.0f, 14.0f, 3.0f); g.setColour (col); g.drawRoundedRectangle (pt.x - 3.0f, pt.y - 7.0f, 6.0f, 14.0f, 3.0f, 1.2f); }
            }
            for (int i = 0; i < SqSpectraEqPlugin::numBands; ++i)
            {
                if (! eq->props[(size_t) i].on.get()) continue;
                const auto b = eq->band (i); const auto pos = nodePos (i); const auto col = bandColour (i);
                const bool sel = i == selected, hov = i == hovered || i == dragging;
                const float rad = sel ? 11.0f : hov ? 10.0f : 8.5f;
                if (b.dynamic)
                {
                    const float y2 = yFor ((float) b.gainDb + eq->dynDb[(size_t) i].load(), r);
                    if (std::abs (y2 - pos.y) > 1.0f)
                    {
                        g.setColour (col.withAlpha (0.7f)); g.drawLine (pos.x, pos.y, pos.x, y2, 2.0f);
                        g.setColour (col.withAlpha (0.35f)); g.drawEllipse (pos.x - 6, y2 - 6, 12, 12, 1.5f);
                    }
                    const float frac = juce::jlimit (0.05f, 1.0f, std::abs ((float) b.rangeDb) / 30.0f);
                    juce::Path arc; arc.addCentredArc (pos.x, pos.y, rad + 5.0f, rad + 5.0f, 0, -frac * juce::MathConstants<float>::pi, frac * juce::MathConstants<float>::pi, true);
                    g.setColour (col.withAlpha (0.8f)); g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                for (int k = 3; k >= 1; --k)
                {
                    const float e = rad + 2.5f * (4 - k);
                    g.setColour (col.withAlpha ((sel ? 0.10f : 0.06f) * k)); g.fillEllipse (pos.x - e, pos.y - e, e * 2, e * 2);
                }
                g.setGradientFill (juce::ColourGradient (col.brighter (0.5f), pos.x - rad * 0.4f, pos.y - rad * 0.5f, col.darker (0.35f), pos.x + rad, pos.y + rad, true));
                g.fillEllipse (pos.x - rad, pos.y - rad, rad * 2, rad * 2);
                g.setColour (sel ? juce::Colours::white : juce::Colours::white.withAlpha (0.35f)); g.drawEllipse (pos.x - rad, pos.y - rad, rad * 2, rad * 2, sel ? 1.8f : 1.0f);
                g.setColour (ink().withAlpha (0.9f)); g.setFont (Theme::font (sel ? 11.0f : 10.0f, true));
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (pos.x - rad, pos.y - rad, rad * 2, rad * 2), juce::Justification::centred);
                if (b.place != SqSpectra::Place::stereo)
                {
                    static const char* tags[] = { "", "L", "R", "M", "S" };
                    auto tag = juce::Rectangle<float> (pos.x + rad - 2.0f, pos.y - rad - 8.0f, 13.0f, 12.0f);
                    g.setColour (ink()); g.fillRoundedRectangle (tag, 3.0f); g.setColour (col); g.drawRoundedRectangle (tag, 3.0f, 1.0f);
                    g.setFont (Theme::font (8.5f, true)); g.drawText (tags[(int) b.place], tag, juce::Justification::centred);
                }
            }
        }
        void paintCard (juce::Graphics& g, int i, juce::Rectangle<float> r)
        {
            const auto b = eq->band (i); const auto pos = nodePos (i); const auto col = bandColour (i);
            const juce::String head = juce::String (i + 1) + "  " + SqSpectra::shapeNames()[(int) b.shape].toUpperCase() + (b.place != SqSpectra::Place::stereo ? "  /  " + SqSpectra::placeNames()[(int) b.place].toUpperCase() : juce::String());
            juce::String main = ColourBits::hz ((float) b.freq);
            if (b.hasGain()) main << "   " << (b.gainDb > 0 ? "+" : "") << juce::String (b.gainDb, 1) << (b.shape == SqSpectra::Shape::flatTilt ? " dB/oct" : " dB");
            const bool cut = b.shape == SqSpectra::Shape::lowCut || b.shape == SqSpectra::Shape::highCut;
            const juce::String sub = cut ? (b.slope <= 0 ? juce::String ("Brickwall") : juce::String (b.slope) + " dB/oct") + "   Q " + juce::String (b.q, 2)
                                         : "Q " + juce::String (b.q, 2) + (b.dynamic ? "   moving " + juce::String (eq->dynDb[(size_t) i].load(), 1) + " dB" : juce::String());
            const float w = 176.0f, h = 62.0f;
            float x = pos.x + 18.0f, y = pos.y - h - 14.0f;
            if (x + w > r.getRight()) x = pos.x - w - 18.0f;
            if (y < r.getY()) y = pos.y + 18.0f;
            auto card = juce::Rectangle<float> (x, y, w, h);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (card.translated (0, 3), 8.0f);
            g.setColour (ink().withAlpha (0.94f)); g.fillRoundedRectangle (card, 8.0f);
            g.setColour (col.withAlpha (0.55f)); g.drawRoundedRectangle (card, 8.0f, 1.0f);
            g.setColour (col); g.fillRoundedRectangle (card.withWidth (3.0f).reduced (0, 8.0f), 1.5f);
            auto t = card.reduced (12.0f, 8.0f);
            g.setColour (col); g.setFont (Theme::font (9.5f, true)); g.drawText (head, t.removeFromTop (14.0f), juce::Justification::left);
            g.setColour (text (0.96f)); g.setFont (Theme::font (14.0f, true)); g.drawText (main, t.removeFromTop (20.0f), juce::Justification::left);
            g.setColour (text (0.55f)); g.setFont (Theme::font (10.0f)); g.drawText (sub, t, juce::Justification::left);
        }
        void paintMeter (juce::Graphics& g, juce::Colour acc)
        {
            auto m = meterRect();
            g.setColour (text (0.05f)); g.fillRoundedRectangle (m, 4.0f);
            auto yOf = [&] (float db) { return m.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f) * m.getHeight(); };
            if (meterDb > -60.0f)
            {
                juce::ColourGradient cg (juce::Colour (0xff5affc8), 0, m.getBottom(), juce::Colour (0xffff5a7a), 0, m.getY(), false);
                cg.addColour (0.72, juce::Colour (0xffffe05a));
                g.setGradientFill (cg); g.fillRoundedRectangle (m.withTop (yOf (meterDb)).reduced (2.0f, 0), 3.0f);
            }
            if (meterPeak > -60.0f) { g.setColour (text (0.9f)); g.fillRect (m.getX() + 2.0f, yOf (meterPeak) - 1.0f, m.getWidth() - 4.0f, 2.0f); }
            g.setColour (acc.withAlpha (0.6f)); g.fillRect (m.getX() - 3.0f, yOf (0.0f) - 0.5f, m.getWidth() + 6.0f, 1.0f);
            g.setColour (text (0.4f)); g.setFont (Theme::font (8.0f, true)); g.drawText ("OUT", m.withY (m.getBottom() + 4.0f).withHeight (12.0f).expanded (8.0f, 0), juce::Justification::centred);
        }
        void paintEmpty (juce::Graphics& g, juce::Rectangle<float> r)
        {
            auto c = r.withSizeKeepingCentre (430.0f, 74.0f);
            g.setColour (ink().withAlpha (0.6f)); g.fillRoundedRectangle (c, 12.0f);
            g.setColour (text (0.12f)); g.drawRoundedRectangle (c, 12.0f, 1.0f);
            g.setColour (text (0.92f)); g.setFont (Theme::font (15.0f, true)); g.drawText ("Double-click anywhere to add a band", c.removeFromTop (42.0f).withTrimmedTop (12.0f), juce::Justification::centred);
            g.setColour (text (0.5f)); g.setFont (Theme::font (10.5f)); g.drawText ("near the left edge for a low cut, near the right for a high cut", c, juce::Justification::centredTop);
        }

        // ---- top bar
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        void chip (juce::Graphics& g, juce::Rectangle<float> rr, const juce::String& t, bool on, juce::Colour acc, int id)
        {
            g.setColour (on ? acc.withAlpha (0.22f) : text (0.045f)); g.fillRoundedRectangle (rr, rr.getHeight() * 0.5f);
            g.setColour (on ? acc.withAlpha (0.8f) : text (0.14f)); g.drawRoundedRectangle (rr, rr.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 0.98f : 0.72f)); g.setFont (Theme::font (10.0f, true)); g.drawText (t, rr, juce::Justification::centred);
            hits.push_back ({ rr, id });
        }
        static float widthOf (const juce::String& t) { return juce::GlyphArrangement::getStringWidth (Theme::font (10.0f, true), t); }
        void paintTopBar (juce::Graphics& g, juce::Colour acc)
        {
            hits.clear();
            auto bar = topBar();
            {
                const juce::StringArray seg { "ZERO LATENCY", "NATURAL", "LINEAR" };
                float w = 0; for (auto& s : seg) w += widthOf (s) + 26.0f;
                auto sc = bar.removeFromLeft (w); bar.removeFromLeft (12.0f);
                g.setColour (text (0.045f)); g.fillRoundedRectangle (sc, sc.getHeight() * 0.5f);
                g.setColour (text (0.14f)); g.drawRoundedRectangle (sc, sc.getHeight() * 0.5f, 1.0f);
                for (int i = 0; i < 3; ++i)
                {
                    auto sr = sc.removeFromLeft (widthOf (seg[i]) + 26.0f);
                    const bool on = eq->phaseMode.get() == i;
                    if (on) { g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.6f), sr.getX(), sr.getY(), acc.darker (0.6f).withAlpha (0.6f), sr.getX(), sr.getBottom(), false)); g.fillRoundedRectangle (sr.reduced (2.0f), (sr.getHeight() - 4.0f) * 0.5f); }
                    g.setColour (text (on ? 1.0f : 0.55f)); g.setFont (Theme::font (10.0f, true)); g.drawText (seg[i], sr, juce::Justification::centred);
                    hits.push_back ({ sr, 10 + i });
                }
            }
            if (eq->phaseMode.get() == 2) { const auto t = "QUALITY  " + SqSpectraEqPlugin::qualityNames()[juce::jlimit (0, 4, eq->quality.get())].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 24.0f), t, true, acc, 2); bar.removeFromLeft (8.0f); }
            { const auto t = "ANALYSER  " + SqSpectraEqPlugin::analyserNames()[juce::jlimit (0, 3, eq->analyser.get())].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 24.0f), t, eq->analyser.get() > 0, acc, 3); bar.removeFromLeft (8.0f); }
            if (eq->analyser.get() > 0) { chip (g, bar.removeFromLeft (widthOf ("FREEZE") + 24.0f), "FREEZE", frozen, acc, 6); bar.removeFromLeft (8.0f); }
            { const auto t = "RANGE  " + SqSpectraEqPlugin::rangeNames()[juce::jlimit (0, 3, eq->displayRange.get())].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 24.0f), t, false, acc, 4); }
            const float outDb = outParam().getCurrentValue();
            const auto t = "OUT  " + juce::String (outDb > 0 ? "+" : "") + juce::String (outDb, 1) + " dB";
            chip (g, bar.removeFromRight (widthOf (t) + 28.0f), t, outDrag || std::abs (outDb) > 0.05f, acc, 7); bar.removeFromRight (8.0f);
            const auto a = eq->autoGain.get() ? "AUTO GAIN  " + juce::String (eq->appliedAutoGain.load(), 1) + " dB" : juce::String ("AUTO GAIN");
            chip (g, bar.removeFromRight (widthOf (a) + 24.0f), a, eq->autoGain.get(), acc, 5); bar.removeFromRight (12.0f);
            const double lat = eq->getLatencySeconds() * 1000.0;
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("LATENCY  " + (lat < 0.01 ? juce::String ("0 MS") : juce::String (lat, 1) + " MS"), bar.removeFromRight (120.0f), juce::Justification::right);
            if (eq->suggestNatural()) { g.setColour (juce::Colour (0xffffd23a)); g.drawText ("STEEP CUT NEAR THE TOP: NATURAL FOLLOWS IT EXACTLY", bar, juce::Justification::centred); }
        }
        void hitAction (int id)
        {
            auto choose = [this] (const juce::StringArray& names, int cur, std::function<void (int)> set)
            {
                juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
                m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<SpectraViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
            };
            um()->beginNewTransaction ("Spectra EQ option");
            if (id >= 10 && id <= 12) { eq->phaseMode.setValue (id - 10, um()); changed(); }
            else if (id == 2) choose (SqSpectraEqPlugin::qualityNames(), eq->quality.get(), [this] (int i) { eq->quality.setValue (i, um()); });
            else if (id == 3) choose (SqSpectraEqPlugin::analyserNames(), eq->analyser.get(), [this] (int i) { eq->analyser.setValue (i, um()); });
            else if (id == 4) choose (SqSpectraEqPlugin::rangeNames(), eq->displayRange.get(), [this] (int i) { eq->displayRange.setValue (i, um()); });
            else if (id == 5) { eq->autoGain.setValue (! eq->autoGain.get(), um()); changed(); }
            else if (id == 6) { frozen = ! frozen; changed(); }
        }

        // ---- the band strip
        enum Cell { cShape, cFreq, cGain, cQ, cSlope, cPlace, cDyn, cThresh, cRange, cAttack, cRelease, cKey, numCells };
        std::array<juce::Rectangle<float>, numCells> cellR;
        juce::Rectangle<float> prevR, nextR;
        static const char* cellName (int c) { static const char* n[] = { "SHAPE", "FREQUENCY", "GAIN", "Q", "SLOPE", "PLACEMENT", "DYNAMIC", "THRESHOLD", "RANGE", "ATTACK", "RELEASE", "KEY" }; return n[c]; }
        juce::String cellText (int c) const
        {
            const auto b = eq->band (selected);
            switch (c)
            {
                case cShape:  return SqSpectra::shapeNames()[(int) b.shape];
                case cFreq:   return ColourBits::hz ((float) b.freq);
                case cGain:   return (b.gainDb > 0 ? "+" : "") + juce::String (b.gainDb, 2) + (b.shape == SqSpectra::Shape::flatTilt ? " dB/oct" : " dB");
                case cQ:      return juce::String (b.q, b.q < 1 ? 3 : 2);
                case cSlope:  return b.slope <= 0 ? juce::String ("Brickwall") : juce::String (b.slope) + " dB/oct";
                case cPlace:  return SqSpectra::placeNames()[(int) b.place];
                case cDyn:    return b.dynamic ? juce::String ("On") : juce::String ("Off");
                case cThresh: return juce::String (b.threshDb, 1) + " dB";
                case cRange:  return (b.rangeDb > 0 ? "+" : "") + juce::String (b.rangeDb, 1) + " dB";
                case cAttack: return juce::String (b.attackMs, b.attackMs < 10 ? 1 : 0) + " ms";
                case cRelease:return juce::String ((int) b.releaseMs) + " ms";
                case cKey:    return b.externalKey ? juce::String ("External") : juce::String ("Band");
            }
            return {};
        }
        bool cellUsed (int c) const
        {
            const auto b = eq->band (selected);
            const bool cut = b.shape == SqSpectra::Shape::lowCut || b.shape == SqSpectra::Shape::highCut || b.shape == SqSpectra::Shape::bandPass || b.shape == SqSpectra::Shape::notch;
            if (c == cGain) return b.hasGain();
            if (c == cSlope) return cut;
            if (c == cDyn) return b.canBeDynamic();
            if (c >= cThresh) return b.dynamic;
            return true;
        }
        static void shapeIcon (juce::Graphics& g, juce::Rectangle<float> r, SqSpectra::Shape s, juce::Colour col)
        {
            SqSpectra::Band b; b.enabled = true; b.shape = s; b.freq = 1000; b.q = s == SqSpectra::Shape::notch ? 2.5 : 1.0; b.slope = 24;
            b.gainDb = s == SqSpectra::Shape::flatTilt ? 3.0 : 9.0;
            const auto proto = SqSpectra::prototype (b, b.gainDb);
            juce::Path p;
            for (int k = 0; k <= 40; ++k)
            {
                const double hz = 1000.0 * std::pow (2.0, (k - 20) / 4.0);
                double db = 20 * std::log10 (std::max (1e-6, std::abs (SqSpectra::analogResponse (proto, hz))));
                if (s == SqSpectra::Shape::allPass) db = 6.0 * std::sin (std::arg (SqSpectra::analogResponse (proto, hz)));
                const float x = r.getX() + r.getWidth() * k / 40.0f, y = r.getCentreY() - (float) juce::jlimit (-12.0, 12.0, db) / 12.0f * r.getHeight() * 0.5f;
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        void paintStrip (juce::Graphics& g)
        {
            auto s = strip();
            g.setColour (ink().withAlpha (0.55f)); g.fillRoundedRectangle (s, 10.0f);
            g.setColour (text (0.07f)); g.drawRoundedRectangle (s, 10.0f, 1.0f);
            for (auto& c : cellR) c = {};
            prevR = nextR = {};
            if (selected < 0 || ! eq->props[(size_t) selected].on.get())
            {
                g.setColour (text (0.5f)); g.setFont (Theme::font (11.0f));
                g.drawText ("Click a node to edit its band.   Drag: frequency and gain  -  Shift: fine  -  Wheel: Q  -  Double-click: add / remove  -  Right-click: options", s, juce::Justification::centred);
                return;
            }
            const auto col = bandColour (selected);
            s = s.reduced (10.0f, 8.0f);
            auto badge = s.removeFromLeft (92.0f); s.removeFromLeft (10.0f);
            prevR = badge.removeFromLeft (18.0f); nextR = badge.removeFromRight (18.0f);
            g.setColour (text (0.55f)); g.setFont (Theme::font (18.0f, true));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9")), prevR, juce::Justification::centred);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xba")), nextR, juce::Justification::centred);
            auto disc = badge.withSizeKeepingCentre (44.0f, 44.0f);
            g.setColour (col.withAlpha (0.18f)); g.fillEllipse (disc.expanded (5.0f));
            g.setGradientFill (juce::ColourGradient (col.brighter (0.5f), disc.getX(), disc.getY(), col.darker (0.4f), disc.getRight(), disc.getBottom(), false)); g.fillEllipse (disc);
            g.setColour (ink()); g.setFont (Theme::font (18.0f, true)); g.drawText (juce::String (selected + 1), disc, juce::Justification::centred);
            int visible = 0; for (int c = 0; c < numCells; ++c) if (cellUsed (c)) ++visible;
            const float gap = 6.0f, w = (s.getWidth() - gap * (visible - 1)) / juce::jmax (1, visible);
            for (int c = 0; c < numCells; ++c)
            {
                if (! cellUsed (c)) continue;
                auto cr = s.removeFromLeft (w); s.removeFromLeft (gap); cellR[(size_t) c] = cr;
                const bool active = stripDrag == c;
                g.setGradientFill (juce::ColourGradient (text (active ? 0.10f : 0.06f), 0, cr.getY(), text (0.015f), 0, cr.getBottom(), false));
                g.fillRoundedRectangle (cr, 8.0f);
                g.setColour (active ? col.withAlpha (0.8f) : text (0.08f)); g.drawRoundedRectangle (cr, 8.0f, 1.0f);
                auto inner = cr.reduced (8.0f, 6.0f);
                g.setColour (text (0.42f)); g.setFont (Theme::font (8.5f, true)); g.drawText (cellName (c), inner.removeFromTop (12.0f), juce::Justification::centred);
                if (c == cShape) shapeIcon (g, inner.removeFromTop (16.0f).withSizeKeepingCentre (44.0f, 16.0f), eq->band (selected).shape, col);
                g.setColour (active ? col.brighter (0.4f) : text (0.96f)); g.setFont (Theme::font (c == cShape ? 11.0f : 14.0f, true));
                g.drawText (cellText (c), inner, juce::Justification::centred);
            }
        }

        // ---- editing
        int newBand (float hz, float db)
        {
            const int i = eq->firstFreeBand(); if (i < 0) return -1;
            auto& p = eq->props[(size_t) i];
            const auto shape = hz < 30.0f ? SqSpectra::Shape::lowCut : hz > 16000.0f ? SqSpectra::Shape::highCut : SqSpectra::Shape::bell;
            um()->beginNewTransaction ("Add band");
            p.shape.setValue ((int) shape, um()); p.slope.setValue (shape == SqSpectra::Shape::bell ? 12 : 24, um()); p.place.setValue (0, um()); p.dyn.setValue (false, um());
            setP (i, SqSpectraEqPlugin::pFreq, juce::jlimit (10.0f, 30000.0f, shape == SqSpectra::Shape::lowCut ? juce::jmax (20.0f, hz) : hz));
            setP (i, SqSpectraEqPlugin::pGain, shape == SqSpectra::Shape::bell ? juce::jlimit (-30.0f, 30.0f, db) : 0.0f);
            setP (i, SqSpectraEqPlugin::pQ, shape == SqSpectra::Shape::bell ? 1.0f : 0.707f);
            p.on.setValue (true, um());
            return i;
        }
        void removeBand (int i) { um()->beginNewTransaction ("Remove band"); eq->props[(size_t) i].on.setValue (false, um()); if (selected == i) selected = -1; if (hovered == i) hovered = -1; changed(); }
        void step (int dir)
        {
            for (int k = 1; k <= SqSpectraEqPlugin::numBands; ++k)
            {
                const int i = ((selected < 0 ? 0 : selected) + dir * k + SqSpectraEqPlugin::numBands * 4) % SqSpectraEqPlugin::numBands;
                if (eq->props[(size_t) i].on.get()) { selected = i; changed(); return; }
            }
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            if (eq == nullptr) return;
            const int h = hitNode (e.position);
            juce::Point<float> lo, hi;
            const bool onQ = qHandles (lo, hi) && (lo.getDistanceFrom (e.position) < 8.0f || hi.getDistanceFrom (e.position) < 8.0f);
            setMouseCursor (onQ ? juce::MouseCursor::LeftRightResizeCursor : h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            if (h != hovered) { hovered = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { if (hovered >= 0) { hovered = -1; repaint(); } }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (eq == nullptr) return;
            grabKeyboardFocus();
            for (auto& h : hits) if (h.r.contains (e.position))
            {
                if (h.id == 7) { outDrag = true; dragStart = e.position; dragStartVal = outParam().getCurrentValue(); um()->beginNewTransaction ("Output gain"); outParam().parameterChangeGestureBegin(); return; }
                hitAction (h.id); return;
            }
            if (prevR.contains (e.position)) { step (-1); return; }
            if (nextR.contains (e.position)) { step (1); return; }
            if (selected >= 0)
                for (int c = 0; c < numCells; ++c)
                    if (cellR[(size_t) c].contains (e.position)) { stripDown (c, e); return; }
            juce::Point<float> lo, hi;
            if (qHandles (lo, hi) && (lo.getDistanceFrom (e.position) < 8.0f || hi.getDistanceFrom (e.position) < 8.0f))
            {
                qDrag = lo.getDistanceFrom (e.position) < hi.getDistanceFrom (e.position) ? -1 : 1;
                um()->beginNewTransaction ("Band Q"); param (selected, SqSpectraEqPlugin::pQ).parameterChangeGestureBegin(); return;
            }
            const int hit = hitNode (e.position);
            if (e.mods.isPopupMenu()) { if (hit >= 0) { selected = hit; changed(); nodeMenu (hit); } else displayMenu(); return; }
            selected = hit;
            if (hit >= 0)
            {
                dragging = hit; dragStart = e.position;
                const auto b = eq->band (hit); dragStartFreq = (float) b.freq; dragStartGain = (float) b.gainDb;
                um()->beginNewTransaction ("Move band");
                param (hit, SqSpectraEqPlugin::pFreq).parameterChangeGestureBegin(); param (hit, SqSpectraEqPlugin::pGain).parameterChangeGestureBegin();
            }
            changed();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (eq == nullptr) return;
            if (outDrag) { outParam().setParameter (juce::jlimit (-36.0f, 36.0f, dragStartVal + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.02f : 0.1f)), juce::sendNotification); changed(); return; }
            if (stripDrag >= 0) { stripMove (e); return; }
            if (qDrag != 0 && selected >= 0)
            {
                const auto r = plot(); const auto b = eq->band (selected);
                const double half = std::abs (std::log2 (hzFor (e.position.x, r) / b.freq));
                const double bw = juce::jlimit (0.02, 8.0, 2.0 * half);
                setP (selected, SqSpectraEqPlugin::pQ, (float) juce::jlimit (0.025, 40.0, 1.0 / (2.0 * std::sinh (std::log (2.0) * bw / 2.0))));
                return;
            }
            if (dragging < 0) return;
            const auto r = plot(); const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
            const float dx = (e.position.x - dragStart.x) * fine, dy = (e.position.y - dragStart.y) * fine;
            setP (dragging, SqSpectraEqPlugin::pFreq, juce::jlimit (10.0f, 30000.0f, hzFor (xFor (dragStartFreq, r) + dx, r)));
            const auto b = eq->band (dragging);
            if (b.hasGain())
            {
                const float perPx = range() / (r.getHeight() * 0.5f);
                const float g = dragStartGain - dy * perPx * (b.shape == SqSpectra::Shape::flatTilt ? 0.2f : 1.0f);
                setP (dragging, SqSpectraEqPlugin::pGain, b.shape == SqSpectra::Shape::flatTilt ? juce::jlimit (-6.0f, 6.0f, g) : g);
            }
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragging >= 0) { param (dragging, SqSpectraEqPlugin::pFreq).parameterChangeGestureEnd(); param (dragging, SqSpectraEqPlugin::pGain).parameterChangeGestureEnd(); }
            if (stripDrag >= 0 && stripParam (stripDrag) >= 0) param (selected, stripParam (stripDrag)).parameterChangeGestureEnd();
            if (qDrag != 0 && selected >= 0) param (selected, SqSpectraEqPlugin::pQ).parameterChangeGestureEnd();
            if (outDrag) outParam().parameterChangeGestureEnd();
            dragging = -1; stripDrag = -1; qDrag = 0; outDrag = false; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (eq == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { if (h.id == 7) { um()->beginNewTransaction ("Output gain"); outParam().setParameter (0.0f, juce::sendNotification); changed(); } return; }
            if (selected >= 0)
                for (int c = 0; c < numCells; ++c)
                    if (cellR[(size_t) c].contains (e.position)) { resetCell (c); return; }
            if (strip().contains (e.position)) return;
            const int hit = hitNode (e.position);
            if (hit >= 0) { removeBand (hit); return; }
            const auto r = plot(); if (! r.expanded (8.0f).contains (e.position)) return;
            selected = newBand (hzFor (e.position.x, r), dbFor (e.position.y, r));
            changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (eq == nullptr) return;
            int i = hitNode (e.position); if (i < 0) i = selected; if (i < 0) return;
            const auto b = eq->band (i);
            um()->beginNewTransaction ("Band Q");
            setP (i, SqSpectraEqPlugin::pQ, juce::jlimit (0.025f, 40.0f, (float) b.q * std::pow (2.0f, w.deltaY * (e.mods.isShiftDown() ? 0.5f : 2.0f))));
        }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (eq == nullptr || selected < 0) return false;
            if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { removeBand (selected); return true; }
            if (k == juce::KeyPress::leftKey) { step (-1); return true; }
            if (k == juce::KeyPress::rightKey) { step (1); return true; }
            return false;
        }

        int stripParam (int c) const
        {
            switch (c) { case cFreq: return SqSpectraEqPlugin::pFreq; case cGain: return SqSpectraEqPlugin::pGain; case cQ: return SqSpectraEqPlugin::pQ;
                         case cThresh: return SqSpectraEqPlugin::pThresh; case cRange: return SqSpectraEqPlugin::pRange; default: return -1; }
        }
        void stripDown (int c, const juce::MouseEvent& e)
        {
            auto& p = eq->props[(size_t) selected];
            um()->beginNewTransaction ("Band setting");
            if (const int sp = stripParam (c); sp >= 0) { stripDrag = c; dragStart = e.position; dragStartVal = param (selected, sp).getCurrentValue(); param (selected, sp).parameterChangeGestureBegin(); return; }
            if (c == cAttack || c == cRelease) { stripDrag = c; dragStart = e.position; dragStartVal = c == cAttack ? p.attack.get() : p.release.get(); return; }
            juce::PopupMenu m;
            auto choose = [&] (const juce::StringArray& names, int cur, std::function<void (int)> set, const std::vector<int>& values = {})
            {
                for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, (values.empty() ? i : values[(size_t) i]) == cur);
                m.showMenuAsync (juce::PopupMenu::Options(), [set, values, sp = SafePointer<SpectraViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (values.empty() ? r - 1 : values[(size_t) r - 1]); sp->changed(); } });
            };
            if (c == cShape) choose (SqSpectra::shapeNames(), p.shape.get(), [this, &p] (int i) { p.shape.setValue (i, um()); });
            else if (c == cPlace) choose (SqSpectra::placeNames(), p.place.get(), [this, &p] (int i) { p.place.setValue (i, um()); });
            else if (c == cSlope)
            {
                juce::StringArray n; std::vector<int> v; for (auto sl : SqSpectra::slopes()) { n.add (sl <= 0 ? juce::String ("Brickwall") : juce::String (sl) + " dB/oct"); v.push_back (sl); }
                choose (n, p.slope.get(), [this, &p] (int sl) { p.slope.setValue (sl, um()); }, v);
            }
            else if (c == cDyn) { p.dyn.setValue (! p.dyn.get(), um()); changed(); }
            else if (c == cKey) { p.extKey.setValue (! p.extKey.get(), um()); changed(); }
        }
        void stripMove (const juce::MouseEvent& e)
        {
            const float dy = (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.1f : 1.0f);
            auto& p = eq->props[(size_t) selected];
            if (stripDrag == cAttack) { p.attack.setValue (juce::jlimit (0.1f, 500.0f, dragStartVal * std::pow (2.0f, dy / 40.0f)), um()); changed(); return; }
            if (stripDrag == cRelease) { p.release.setValue (juce::jlimit (5.0f, 5000.0f, dragStartVal * std::pow (2.0f, dy / 40.0f)), um()); changed(); return; }
            const int sp = stripParam (stripDrag);
            float v = dragStartVal;
            if (sp == SqSpectraEqPlugin::pFreq) v = dragStartVal * std::pow (2.0f, dy / 30.0f);
            else if (sp == SqSpectraEqPlugin::pQ) v = dragStartVal * std::pow (2.0f, dy / 50.0f);
            else v = dragStartVal + dy * 0.1f;
            setP (selected, sp, v);
        }
        void resetCell (int c)
        {
            um()->beginNewTransaction ("Reset band setting");
            if (c == cGain) setP (selected, SqSpectraEqPlugin::pGain, 0.0f);
            else if (c == cQ) setP (selected, SqSpectraEqPlugin::pQ, eq->band (selected).hasGain() ? 1.0f : 0.707f);
            else if (c == cThresh) setP (selected, SqSpectraEqPlugin::pThresh, -30.0f);
            else if (c == cRange) setP (selected, SqSpectraEqPlugin::pRange, 0.0f);
            else if (c == cAttack) { eq->props[(size_t) selected].attack.setValue (10.0f, um()); changed(); }
            else if (c == cRelease) { eq->props[(size_t) selected].release.setValue (120.0f, um()); changed(); }
        }

        void nodeMenu (int i)
        {
            auto& p = eq->props[(size_t) i]; const auto b = eq->band (i);
            juce::PopupMenu m, shapes, slopes, places;
            for (int s = 0; s < 10; ++s) shapes.addItem (100 + s, SqSpectra::shapeNames()[s], true, (int) b.shape == s);
            for (auto sl : SqSpectra::slopes()) slopes.addItem (200 + juce::jmax (0, sl), sl <= 0 ? juce::String ("Brickwall") : juce::String (sl) + " dB/oct", true, b.slope == sl);
            for (int s = 0; s < 5; ++s) places.addItem (300 + s, SqSpectra::placeNames()[s], true, (int) b.place == s);
            m.addSectionHeader ("Band " + juce::String (i + 1));
            m.addSubMenu ("Shape", shapes); m.addSubMenu ("Slope", slopes); m.addSubMenu ("Placement", places);
            m.addItem (1, "Dynamic", b.canBeDynamic(), b.dynamic);
            m.addItem (2, "Listen to the external key", b.dynamic, b.externalKey);
            m.addSeparator(); m.addItem (3, "Remove band");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, i, &p, sp = SafePointer<SpectraViz> (this)] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                um()->beginNewTransaction ("Band option");
                if (r >= 300) p.place.setValue (r - 300, um());
                else if (r >= 200) p.slope.setValue (r - 200, um());
                else if (r >= 100) p.shape.setValue (r - 100, um());
                else if (r == 1) p.dyn.setValue (! p.dyn.get(), um());
                else if (r == 2) p.extKey.setValue (! p.extKey.get(), um());
                else if (r == 3) { removeBand (i); return; }
                changed();
            });
        }
        void displayMenu()
        {
            juce::PopupMenu m, ph, q, an, rg;
            for (int i = 0; i < 3; ++i) ph.addItem (10 + i, SqSpectra::phaseNames()[i], true, eq->phaseMode.get() == i);
            for (int i = 0; i < 5; ++i) q.addItem (20 + i, SqSpectraEqPlugin::qualityNames()[i], true, eq->quality.get() == i);
            for (int i = 0; i < 4; ++i) an.addItem (30 + i, SqSpectraEqPlugin::analyserNames()[i], true, eq->analyser.get() == i);
            for (int i = 0; i < 4; ++i) rg.addItem (40 + i, SqSpectraEqPlugin::rangeNames()[i], true, eq->displayRange.get() == i);
            m.addSubMenu ("Phase", ph); m.addSubMenu ("Linear quality", q); m.addSubMenu ("Analyser", an); m.addSubMenu ("Display range", rg);
            m.addItem (1, "Auto gain", true, eq->autoGain.get());
            m.addItem (2, "Freeze the analyser", eq->analyser.get() > 0, frozen);
            m.addSeparator(); m.addItem (3, "Remove every band");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<SpectraViz> (this)] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                um()->beginNewTransaction ("Spectra EQ option");
                if (r >= 40) eq->displayRange.setValue (r - 40, um());
                else if (r >= 30) eq->analyser.setValue (r - 30, um());
                else if (r >= 20) eq->quality.setValue (r - 20, um());
                else if (r >= 10) eq->phaseMode.setValue (r - 10, um());
                else if (r == 1) eq->autoGain.setValue (! eq->autoGain.get(), um());
                else if (r == 2) frozen = ! frozen;
                else if (r == 3) { for (auto& p : eq->props) p.on.setValue (false, um()); selected = -1; }
                changed();
            });
        }
    };
