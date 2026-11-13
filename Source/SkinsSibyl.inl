// Included inside namespace Skins (PluginSkins.h): the Sibyl De-Ess face (1.14 Studio Line, phase F).
//
// Squelchmoth's own "silver light on still water" design on the Higgsfield plate pa_sibyldeess:
//  - top bar: split / wide, absolute / relative detection, track, stereo mode, side-chain, listen (output / detection /
//    removed), latency
//  - the band: the spectrum, the detection band as a lit pool with draggable edges, the real cut drawn live (its depth is
//    the gain change right now), and in Track mode the frequency it has found
//  - the sibilance trace: detector level scrolling against the threshold (drag the line), the cut in rose beneath
//  - the deck: pearl-inlay knobs with rose-gold bezels, drawn here

    struct SibylViz : public Viz, private juce::Timer
    {
        using P = SqSibylDeEssPlugin;
        SibylViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sb (dynamic_cast<SqSibylDeEssPlugin*> (&p)), fft (12), window ((size_t) fftN, juce::dsp::WindowingFunction<float>::hann)
        {
            setInterceptsMouseClicks (true, false); spec.fill (-120.0f); startTimerHz (30);
        }
        SqSibylDeEssPlugin* sb;
        std::function<void()> onChange;

        static juce::Colour night() { return juce::Colour (0xff0a0610); }
        static juce::Colour pearl() { return juce::Colour (0xfff3eef8); }
        static juce::Colour rose()  { return juce::Colour (0xffe8a48c); }   // rose gold
        static juce::Colour lilac() { return juce::Colour (0xffb9a6ff); }
        static juce::Colour cut()   { return juce::Colour (0xffff6f8f); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfff7f2ff).withAlpha (a); }

        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (196.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (204.0f); return r; }
        juce::Rectangle<float> bandPane() const { auto r = middle(); return r.removeFromLeft (r.getWidth() * 0.60f); }
        juce::Rectangle<float> tracePane() const { auto r = middle(); r.removeFromLeft (r.getWidth() * 0.60f + 12.0f); return r; }
        juce::Rectangle<float> plot() const { return bandPane().reduced (16.0f, 0).withTrimmedTop (30.0f).withTrimmedBottom (26.0f).withTrimmedRight (26.0f); }
        juce::Rectangle<float> trace() const { return tracePane().reduced (14.0f, 0).withTrimmedTop (30.0f).withTrimmedBottom (40.0f).withTrimmedRight (28.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.68f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (pearl().withAlpha (0.06f), 0, r.getY(), pearl().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (rose().withAlpha (0.18f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        // spectrum
        static constexpr int fftN = 4096, pts = 240;
        juce::dsp::FFT fft; juce::dsp::WindowingFunction<float> window; std::array<float, fftN * 2> work {}; std::array<float, pts> spec {};
        void timerCallback() override
        {
            if (sb != nullptr)
            {
                const int w = sb->scopeW.load();
                for (int i = 0; i < fftN; ++i) work[(size_t) i] = sb->scope[(size_t) ((w - fftN + i) & (P::scopeLen - 1))];
                std::fill (work.begin() + fftN, work.end(), 0.0f);
                window.multiplyWithWindowingTable (work.data(), (size_t) fftN); fft.performFrequencyOnlyForwardTransform (work.data(), true);
                const double binHz = juce::jmax (8000.0, sb->sampleRate()) / fftN;
                for (int i = 0; i < pts; ++i)
                {
                    const double f0 = 500.0 * std::pow (40.0, i / (double) pts), f1 = 500.0 * std::pow (40.0, (i + 1) / (double) pts);
                    const int b0 = (int) (f0 / binHz), b1 = std::max (b0 + 1, (int) (f1 / binHz));
                    float m = 0; for (int b = b0; b < std::min (b1, fftN / 2); ++b) m = std::max (m, work[(size_t) b]);
                    const float v = juce::Decibels::gainToDecibels (m * 4.0f / fftN, -120.0f) + 3.0f * (float) std::log2 (f0 / 1000.0);
                    spec[(size_t) i] = v > spec[(size_t) i] ? v : spec[(size_t) i] * 0.8f + v * 0.2f;
                }
            }
            repaint();
        }
        // 500 Hz .. 20 kHz: where sibilance lives, with room to see the voice below it
        static float xOf (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (juce::jlimit (500.0, 20000.0, hz) / 500.0) / std::log (40.0)) * p.getWidth(); }
        static double hzOf (juce::Rectangle<float> p, float x) { return 500.0 * std::pow (40.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }

        void paint (juce::Graphics& g) override
        {
            if (sb == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("sibyldeess");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.85f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.20f), a.getCentreX(), a.getY(), night().withAlpha (0.84f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (rose().withAlpha (0.26f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            paintTopBar (g); paintBand (g); paintTrace (g); paintDeck (g);
        }

        // ---- the band
        void paintBand (juce::Graphics& g)
        {
            pane (g, bandPane(), "THE BAND  -  drag its edges; the rose curve is the cut happening now");
            const auto p = plot();
            g.setFont (Theme::font (8.5f, true));
            for (double hz : { 500.0, 1000.0, 2000.0, 3000.0, 5000.0, 7000.0, 10000.0, 15000.0, 20000.0 })
            {
                const float x = xOf (p, hz);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.32f)); g.drawText (hz >= 1000 ? juce::String (hz / 1000.0, 0) + "k" : juce::String ((int) hz), (int) x - 16, (int) p.getBottom() + 4, 32, 12, juce::Justification::centred);
            }
            auto ySpec = [&] (double dbv) { return p.getY() + (float) juce::jlimit (0.0, 1.0, (0.0 - dbv) / 90.0) * p.getHeight(); };
            auto yCut = [&] (double dbv) { return p.getY() + 18.0f + (float) juce::jlimit (0.0, 1.0, -dbv / 24.0) * (p.getHeight() * 0.55f); };
            for (int d = 0; d <= 24; d += 6) { g.setColour (text (d == 0 ? 0.12f : 0.04f)); g.drawHorizontalLine ((int) yCut (-d), p.getX(), p.getRight()); g.setColour (text (0.3f)); g.drawText (d == 0 ? "0" : "-" + juce::String (d), (int) p.getRight() + 2, (int) yCut (-d) - 6, 24, 12, juce::Justification::left); }
            // spectrum veil
            juce::Path sp; sp.startNewSubPath (p.getX(), p.getBottom());
            for (int i = 0; i < pts; ++i) sp.lineTo (p.getX() + p.getWidth() * (i + 0.5f) / pts, ySpec (spec[(size_t) i]));
            sp.lineTo (p.getRight(), p.getBottom()); sp.closeSubPath();
            g.setGradientFill (juce::ColourGradient (pearl().withAlpha (0.20f), 0, p.getY(), lilac().withAlpha (0.02f), 0, p.getBottom(), false)); g.fillPath (sp);
            // the band: a lit pool
            const float xl = xOf (p, sb->bank[P::low]), xh = xOf (p, std::max (sb->bank[P::low] * 1.05f, sb->bank[P::high]));
            auto pool = juce::Rectangle<float>::leftTopRightBottom (xl, p.getY(), xh, p.getBottom());
            g.setGradientFill (juce::ColourGradient (lilac().withAlpha (0.16f), 0, p.getY(), lilac().withAlpha (0.03f), 0, p.getBottom(), false)); g.fillRect (pool);
            for (auto [x, id] : { std::pair<float, int> { xl, 1 }, { xh, 2 } })
            {
                const bool hot = hoverEdge == id || dragEdge == id;
                g.setColour (lilac().withAlpha (hot ? 1.0f : 0.7f)); g.fillRect (x - (hot ? 1.5f : 0.75f), p.getY(), hot ? 3.0f : 1.5f, p.getHeight());
                auto tag = juce::Rectangle<float> (x - 30, p.getBottom() - 20, 60, 16);
                g.setColour (night().withAlpha (0.9f)); g.fillRoundedRectangle (tag, 8.0f); g.setColour (lilac().withAlpha (0.8f)); g.drawRoundedRectangle (tag, 8.0f, 1.0f);
                const float hz = id == 1 ? sb->bank[P::low] : sb->bank[P::high];
                g.setColour (text (0.95f)); g.drawText (juce::String (hz / 1000.0f, hz < 10000 ? 2 : 1) + "k", tag, juce::Justification::centred);
            }
            // the cut, live, and its potential (the full range) faintly
            const double gr = sb->grNow.load(), rng = sb->bank[P::range];
            juce::Path pot, now;
            for (int i = 0; i <= 200; ++i)
            {
                const float x = p.getX() + p.getWidth() * i / 200.0f; const double hz = hzOf (p, x);
                const float yp = yCut (sb->cutShapeDb (hz, -rng)), yn = yCut (sb->cutShapeDb (hz, gr));
                if (i == 0) { pot.startNewSubPath (x, yp); now.startNewSubPath (x, yn); } else { pot.lineTo (x, yp); now.lineTo (x, yn); }
            }
            const float dash[] = { 3.0f, 4.0f }; juce::Path potD; juce::PathStrokeType (1.0f).createDashedStroke (potD, pot, dash, 2);
            g.setColour (rose().withAlpha (0.45f)); g.fillPath (potD);
            juce::Path nowFill (now); nowFill.lineTo (p.getRight(), yCut (0)); nowFill.lineTo (p.getX(), yCut (0)); nowFill.closeSubPath();
            g.setColour (cut().withAlpha (0.20f)); g.fillPath (nowFill);
            g.setColour (cut().withAlpha (0.30f)); g.strokePath (now, juce::PathStrokeType (6.0f));
            g.setColour (cut()); g.strokePath (now, juce::PathStrokeType (2.0f));
            // tracked frequency
            if (sb->track.get())
            {
                const float xt = xOf (p, sb->trackHz.load());
                g.setColour (pearl()); g.fillEllipse (xt - 5, yCut (gr) - 5, 10, 10);
                g.setColour (pearl().withAlpha (0.35f)); g.drawVerticalLine ((int) xt, p.getY(), p.getBottom());
                g.setColour (text (0.95f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("TRACKING " + juce::String (sb->trackHz.load() / 1000.0f, 2) + " kHz", (int) xt + 8, (int) p.getY() + 2, 140, 12, juce::Justification::left);
            }
            // the cut, in numbers
            g.setColour (cut()); g.setFont (Theme::font (24.0f, true));
            g.drawText (juce::String (gr, 1) + " dB", p.withTrimmedTop (p.getHeight() - 70.0f).withHeight (28.0f).withTrimmedLeft (8.0f), juce::Justification::left);
            g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true));
            g.drawText ("TAKEN FROM THE S NOW", p.withTrimmedTop (p.getHeight() - 42.0f).withHeight (12.0f).withTrimmedLeft (8.0f), juce::Justification::left);
        }

        // ---- the sibilance trace
        float yTrace (double dbv) const { const auto t = trace(); const bool rel = sb->detect.get() == 1; const double top = rel ? 6.0 : 0.0, bot = rel ? -48.0 : -66.0; return t.getY() + (float) juce::jlimit (0.0, 1.0, (top - dbv) / (top - bot)) * t.getHeight(); }
        void paintTrace (juce::Graphics& g)
        {
            const bool rel = sb->detect.get() == 1;
            pane (g, tracePane(), rel ? "SIBILANCE  -  the band against the whole voice (dB)" : "SIBILANCE  -  the band level (dBFS)");
            const auto t = trace();
            g.setFont (Theme::font (8.5f, true));
            for (int d = rel ? 6 : 0; d >= (rel ? -48 : -66); d -= 6)
            {
                const float y = yTrace (d); g.setColour (text (d % 12 == 0 ? 0.06f : 0.03f)); g.drawHorizontalLine ((int) y, t.getX(), t.getRight());
                if (d % 12 == 0) { g.setColour (text (0.32f)); g.drawText (juce::String (d), (int) t.getRight() + 3, (int) y - 6, 26, 12, juce::Justification::left); }
            }
            const int Nn = P::histLen, w = sb->histW.load(), cols = 300;
            juce::Path det, gr; bool started = false;
            for (int i = 0; i <= cols; ++i)
            {
                const int idx = (w - 1 - (cols - i)) & (Nn - 1);
                const float x = t.getX() + t.getWidth() * i / (float) cols;
                const float yd = yTrace (sb->histDet[(size_t) idx]), yg = t.getY() + juce::jlimit (0.0f, 1.0f, -sb->histGr[(size_t) idx] / 24.0f) * t.getHeight() * 0.5f;
                if (! started) { det.startNewSubPath (x, t.getBottom()); det.lineTo (x, yd); gr.startNewSubPath (x, t.getY()); gr.lineTo (x, yg); started = true; } else { det.lineTo (x, yd); gr.lineTo (x, yg); }
            }
            det.lineTo (t.getRight(), t.getBottom()); det.closeSubPath(); gr.lineTo (t.getRight(), t.getY()); gr.closeSubPath();
            g.setGradientFill (juce::ColourGradient (lilac().withAlpha (0.45f), 0, t.getY(), lilac().withAlpha (0.03f), 0, t.getBottom(), false)); g.fillPath (det);
            g.setGradientFill (juce::ColourGradient (cut().withAlpha (0.75f), 0, t.getY(), cut().withAlpha (0.08f), 0, t.getY() + t.getHeight() * 0.5f, false)); g.fillPath (gr);
            // threshold: a pearl line you drag
            const float yt = yTrace (sb->bank[P::threshold]); const bool hot = hoverThr || dragThr;
            g.setColour (pearl().withAlpha (0.25f)); g.fillRect (t.getX(), yt - 2.0f, t.getWidth(), 4.0f);
            g.setColour (pearl()); g.fillRect (t.getX(), yt - (hot ? 1.0f : 0.5f), t.getWidth(), hot ? 2.0f : 1.0f);
            g.setFont (Theme::font (9.0f, true)); g.drawText ("THRESHOLD " + juce::String (sb->bank[P::threshold], 1) + " dB", (int) t.getX() + 6, (int) yt - 15, 200, 12, juce::Justification::left);
            // readouts
            auto foot = tracePane().reduced (14.0f, 8.0f).removeFromBottom (24.0f);
            g.setColour (text (0.5f)); g.setFont (Theme::font (9.0f, true));
            g.drawText ("DETECTED " + (sb->detNow.load() > -99 ? juce::String (sb->detNow.load(), 1) + " dB" : juce::String ("--")), foot, juce::Justification::left);
            g.drawText ("RANGE " + juce::String (sb->bank[P::range], 1) + " dB", foot, juce::Justification::right);
        }

        // ---- top bar
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 9.5f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c)
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (9.5f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void group (juce::Graphics& g, juce::Rectangle<float>& bar, const juce::StringArray& names, int cur, int base, juce::Colour c)
        {
            for (int i = 0; i < names.size(); ++i) { const auto t = names[i].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 20.0f), t, cur == i, base + i, c); bar.removeFromLeft (3.0f); }
            bar.removeFromLeft (12.0f);
        }
        void paintTopBar (juce::Graphics& g)
        {
            hits.clear(); auto bar = topBar();
            group (g, bar, P::shapeNames(), sb->shape.get(), 10, rose());
            group (g, bar, P::detectNames(), sb->detect.get(), 20, lilac());
            chip (g, bar.removeFromLeft (64.0f), "TRACK", sb->track.get(), 30, pearl()); bar.removeFromLeft (15.0f);
            group (g, bar, P::stereoNames(), sb->stereoMode.get(), 40, lilac());
            chip (g, bar.removeFromLeft (92.0f), "SIDE-CHAIN", sb->sidechain.get(), 50, rose()); bar.removeFromLeft (15.0f);
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("LISTEN", bar.removeFromLeft (44.0f), juce::Justification::centredLeft);
            group (g, bar, P::listenNames(), sb->listen.get(), 60, cut());
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("LATENCY " + juce::String (sb->getLatencySeconds() * 1000.0, 1) + " MS", bar, juce::Justification::right);
        }
        juce::UndoManager* um() const { return &sb->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Sibyl De-Ess setting"); auto& s = *sb;
            if (id >= 10 && id < 12) s.shape.setValue (id - 10, um());
            else if (id >= 20 && id < 22) s.detect.setValue (id - 20, um());
            else if (id == 30) s.track.setValue (! s.track.get(), um());
            else if (id >= 40 && id < 43) s.stereoMode.setValue (id - 40, um());
            else if (id == 50) s.sidechain.setValue (! s.sidechain.get(), um());
            else if (id >= 60 && id < 63) s.listen.setValue (s.listen.get() == id - 60 && id != 60 ? 0 : id - 60, um());
            changed();
        }

        // ---- the deck: pearl-inlay knobs with rose-gold bezels
        struct KnobDef { int param; const char* label; };
        static const std::vector<KnobDef>& defs()
        {
            static const std::vector<KnobDef> d { { P::threshold, "THRESHOLD" }, { P::range, "RANGE" }, { P::knee, "KNEE" }, { P::release, "RELEASE" },
                                                  { P::lookahead, "LOOKAHEAD" }, { P::low, "LOW" }, { P::high, "HIGH" }, { P::output, "OUTPUT" } };
            return d;
        }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *sb->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); changed(); }
        void layoutKnobs()
        {
            knobR.clear(); auto d = deck().reduced (18.0f, 12.0f);
            for (int i = 0; i < (int) defs().size(); ++i) knobR.push_back ({ d.getX() + d.getWidth() / defs().size() * i, d.getY(), d.getWidth() / defs().size(), d.getHeight() });
        }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, bool hot, bool active, bool bipolar)
        {
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (24.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 20.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text (hot ? 0.95f : 0.55f)); g.setFont (Theme::font (9.5f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            g.setGradientFill (juce::ColourGradient (lilac().withAlpha (hot ? 0.16f : 0.08f), c.x, c.y, lilac().withAlpha (0.0f), c.x, c.y + rO * 1.4f, true));
            g.fillEllipse (c.x - rO * 1.35f, c.y - rO * 1.35f, rO * 2.7f, rO * 2.7f);
            // value arc: rose gold over a hairline track
            const float ar = rO * 0.95f;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (text (0.12f)); g.strokePath (track, juce::PathStrokeType (1.0f));
            const float as = bipolar ? (a0 + a1) * 0.5f : a0;
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (as, av), std::max (as, av), true);
            g.setColour (rose().withAlpha (0.25f)); g.strokePath (arc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (rose(), c.x - ar, c.y + ar, pearl(), c.x + ar, c.y - ar, false));
            g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // rose-gold bezel with engraved ticks
            const float rb = rO * 0.80f, rbIn = rb * 0.86f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2c6b4), c.x - rb, c.y - rb, juce::Colour (0xff6e3b30), c.x + rb, c.y + rb, false));
            g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            for (int t = 0; t < 48; ++t)
            {
                const float a = juce::MathConstants<float>::twoPi * t / 48.0f;
                g.setColour (juce::Colour (0xff3a1d18).withAlpha (t % 4 == 0 ? 0.55f : 0.25f));
                g.drawLine (c.x + rbIn * std::sin (a), c.y - rbIn * std::cos (a), c.x + rb * 0.98f * std::sin (a), c.y - rb * 0.98f * std::cos (a), 0.8f);
            }
            // lacquered black face with a pearl inlay ring
            const float rf = rbIn - 1.0f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2032), c.x - rf * 0.5f, c.y - rf * 0.8f, juce::Colour (0xff07040a), c.x + rf * 0.6f, c.y + rf, true));
            g.fillEllipse (c.x - rf, c.y - rf, rf * 2, rf * 2);
            const float rp = rf * 0.62f;
            g.setGradientFill (juce::ColourGradient (pearl().withAlpha (0.85f), c.x - rp, c.y - rp, lilac().withAlpha (0.55f), c.x + rp, c.y + rp, false));
            g.drawEllipse (c.x - rp, c.y - rp, rp * 2, rp * 2, 1.6f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x, c.y - rf, juce::Colours::transparentWhite, c.x, c.y, false));
            g.fillEllipse (c.x - rf * 0.8f, c.y - rf * 0.95f, rf * 1.6f, rf * 0.95f);
            // pointer: a pearl blade from the inlay to the rim
            const juce::Point<float> p1 (c.x + rp * std::sin (av), c.y - rp * std::cos (av)), p2 (c.x + rf * 0.92f * std::sin (av), c.y - rf * 0.92f * std::cos (av));
            g.setColour (pearl().withAlpha (0.35f)); g.drawLine ({ p1, p2 }, 4.5f);
            g.setColour (pearl()); g.drawLine ({ p1, p2 }, 1.8f);
            if (hot) { g.setColour (rose()); g.drawEllipse (c.x - rb - 1.5f, c.y - rb - 1.5f, rb * 2 + 3, rb * 2 + 3, 1.0f); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 6.0f, 96.0f), 19.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 9.5f);
            g.setColour (active ? rose() : text (0.1f)); g.drawRoundedRectangle (pill, 9.5f, 1.0f);
            g.setColour (active ? rose() : text (0.95f)); g.setFont (Theme::font (11.0f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (int i) const
        {
            const float v = param (i).getCurrentValue();
            switch (i)
            {
                case P::release:   return juce::String ((int) std::lround (v)) + " ms";
                case P::lookahead: return v < 0.05f ? juce::String ("OFF") : juce::String (v, 1) + " ms";
                case P::low: case P::high: return juce::String (v / 1000.0f, v < 10000 ? 2 : 1) + " kHz";
                case P::output:    return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
                default:           return juce::String (v, 1) + " dB";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff170f1d).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (rose().withAlpha (0.20f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            for (size_t i = 0; i < defs().size(); ++i)
                drawKnob (g, knobR[i], defs()[i].label, knobText (defs()[i].param), norm (defs()[i].param), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, defs()[i].param == P::output);
        }

        // ---- mouse
        int dragEdge = 0, hoverEdge = 0; bool dragThr = false, hoverThr = false;
        int edgeAt (juce::Point<float> pt) const
        {
            const auto p = plot(); if (! p.contains (pt)) return 0;
            if (std::abs (pt.x - xOf (p, sb->bank[P::low])) < 6) return 1;
            if (std::abs (pt.x - xOf (p, sb->bank[P::high])) < 6) return 2;
            return 0;
        }
        bool thrAt (juce::Point<float> pt) const { return trace().contains (pt) && std::abs (pt.y - yTrace (sb->bank[P::threshold])) < 6; }
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (6.0f, 0).contains (p)) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), ed = edgeAt (e.position); const bool th = thrAt (e.position);
            setMouseCursor (k >= 0 || th ? juce::MouseCursor::UpDownResizeCursor : ed ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || ed != hoverEdge || th != hoverThr) { hoverKnob = k; hoverEdge = ed; hoverThr = th; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverEdge = 0; hoverThr = false; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (sb == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (const int ed = edgeAt (e.position)) { dragEdge = ed; um()->beginNewTransaction ("Sibyl band"); param (ed == 1 ? P::low : P::high).parameterChangeGestureBegin(); return; }
            if (thrAt (e.position)) { dragThr = true; um()->beginNewTransaction ("Sibyl threshold"); param (P::threshold).parameterChangeGestureBegin(); return; }
            const int k = knobAt (e.position);
            if (k >= 0) { dragKnob = k; dragStart = e.position; dragStartNorm = norm (defs()[(size_t) k].param); um()->beginNewTransaction (juce::String ("Sibyl ") + defs()[(size_t) k].label); param (defs()[(size_t) k].param).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragEdge) { setValue (dragEdge == 1 ? P::low : P::high, (float) hzOf (plot(), e.position.x)); return; }
            if (dragThr)
            {
                const auto t = trace(); const bool rel = sb->detect.get() == 1; const double top = rel ? 6.0 : 0.0, bot = rel ? -48.0 : -66.0;
                setValue (P::threshold, (float) (top - (top - bot) * juce::jlimit (0.0, 1.0, (double) ((e.position.y - t.getY()) / t.getHeight())))); return;
            }
            if (dragKnob < 0) return;
            setNorm (defs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragEdge) param (dragEdge == 1 ? P::low : P::high).parameterChangeGestureEnd();
            if (dragThr) param (P::threshold).parameterChangeGestureEnd();
            if (dragKnob >= 0) param (defs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; dragEdge = 0; dragThr = false; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = defs()[(size_t) k].param; um()->beginNewTransaction ("Sibyl reset"); param (pi_).setParameter (sb->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Sibyl wheel"); const int pi_ = defs()[(size_t) k].param; setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
