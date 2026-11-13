// Included inside namespace Skins (PluginSkins.h): the Portcullis Gate face (1.14 Studio Line, phase G).
//
// Squelchmoth's own "iron gate by moonlight" design on the Higgsfield plate pa_portcullis:
//  - top bar: gate / expand / duck, the key (input / side-chain / MIDI), the curve, velocity, listen to the key, flip
//  - the gate history: the input as a dim wash, the key as a steel line, the OPEN threshold (solid) and the CLOSE
//    threshold (dashed: the hysteresis) - both draggable - and the gate itself drawn as iron bars that drop as it closes
//  - the right column: the transfer curve with the live key dot, a portcullis that rises and falls with the gate, and the
//    key filter with its two corners
//  - the deck: forged octagonal knobs with rivets and an amber ring, drawn here

    struct PortcullisViz : public Viz, private juce::Timer
    {
        using P = SqPortcullisGatePlugin;
        PortcullisViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), pg (dynamic_cast<SqPortcullisGatePlugin*> (&p)) { setInterceptsMouseClicks (true, false); startTimerHz (30); }
        SqPortcullisGatePlugin* pg;
        std::function<void()> onChange;

        static juce::Colour night() { return juce::Colour (0xff07090c); }
        static juce::Colour steel() { return juce::Colour (0xff8fb4d9); }
        static juce::Colour iron()  { return juce::Colour (0xff3a434d); }
        static juce::Colour amber() { return juce::Colour (0xffffb04a); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xffeef4fb).withAlpha (a); }

        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (190.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (198.0f); return r; }
        juce::Rectangle<float> histPane() const { auto r = middle(); return r.removeFromLeft (r.getWidth() - 352.0f); }
        juce::Rectangle<float> sidePane() const { auto r = middle(); return r.removeFromRight (340.0f); }
        juce::Rectangle<float> hist() const { return histPane().reduced (16.0f, 0).withTrimmedTop (30.0f).withTrimmedBottom (16.0f).withTrimmedRight (28.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.70f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (steel().withAlpha (0.06f), 0, r.getY(), steel().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (steel().withAlpha (0.16f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }
        void timerCallback() override { repaint(); }

        void paint (juce::Graphics& g) override
        {
            if (pg == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("portcullis");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.85f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.22f), a.getCentreX(), a.getY(), night().withAlpha (0.86f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (steel().withAlpha (0.22f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            paintTopBar (g); paintHistory (g); paintSide (g); paintDeck (g);
        }

        // ---- history
        float yLev (double dbv) const { const auto h = hist(); return h.getY() + (float) juce::jlimit (0.0, 1.0, -dbv / 84.0) * h.getHeight(); }
        void paintHistory (juce::Graphics& g)
        {
            pane (g, histPane(), "THE GATE  -  drag the solid line to open, the dashed line to close (hysteresis)");
            const auto h = hist();
            g.setFont (Theme::font (8.5f, true));
            for (int d = 0; d <= 84; d += 12) { const float y = yLev (-d); g.setColour (text (0.05f)); g.drawHorizontalLine ((int) y, h.getX(), h.getRight()); g.setColour (text (0.3f)); g.drawText (juce::String (-d), (int) h.getRight() + 3, (int) y - 6, 26, 12, juce::Justification::left); }
            const int Nn = P::histLen, w = pg->histW.load(), cols = 360; const double R = std::max (1.0f, pg->bank[P::range]);
            juce::Path inP, keyP;
            // the gate as iron bars: each column's bars drop from the top by how far the gate was closed
            const float colW = h.getWidth() / cols;
            for (int i = 0; i <= cols; ++i)
            {
                const int idx = (w - 1 - (cols - i)) & (Nn - 1);
                const float x = h.getX() + colW * i;
                const float yi = yLev (pg->histIn[(size_t) idx]), yk = yLev (pg->histKey[(size_t) idx]);
                if (i == 0) { inP.startNewSubPath (x, h.getBottom()); inP.lineTo (x, yi); keyP.startNewSubPath (x, yk); } else { inP.lineTo (x, yi); keyP.lineTo (x, yk); }
                const float closed = (float) juce::jlimit (0.0, 1.0, -pg->histGain[(size_t) idx] / R);
                if (closed > 0.01f)
                {
                    const float drop = closed * h.getHeight();
                    if (i % 6 == 0) { g.setGradientFill (juce::ColourGradient (iron().brighter (0.4f), x, h.getY(), iron().darker (0.4f), x + 3, h.getY(), false)); g.fillRect (x, h.getY(), 3.0f, drop); }
                    g.setColour (steel().withAlpha (0.05f + 0.10f * closed)); g.fillRect (x, h.getY(), colW + 0.5f, drop);
                    if (i % 6 == 0) { g.setColour (amber().withAlpha (0.8f)); g.fillRect (x - 1.0f, h.getY() + drop - 3.0f, 5.0f, 3.0f); }   // the spike tips
                }
            }
            for (int yy = 0; yy < 8; ++yy) { const float y = h.getY() + h.getHeight() * yy / 8.0f; g.setColour (iron().withAlpha (0.12f)); g.drawHorizontalLine ((int) y, h.getX(), h.getRight()); }   // cross members, faint
            inP.lineTo (h.getRight(), h.getBottom()); inP.closeSubPath();
            g.setGradientFill (juce::ColourGradient (text (0.18f), 0, h.getY(), text (0.02f), 0, h.getBottom(), false)); g.fillPath (inP);
            g.setColour (steel()); g.strokePath (keyP, juce::PathStrokeType (1.3f));
            // thresholds
            const float yo = yLev (pg->bank[P::threshold]), yc = yLev (pg->bank[P::threshold] - pg->bank[P::hysteresis]);
            g.setColour (amber().withAlpha (0.25f)); g.fillRect (h.getX(), yo - 2.0f, h.getWidth(), 4.0f);
            g.setColour (amber()); g.fillRect (h.getX(), yo - (hot == 1 ? 1.0f : 0.5f), h.getWidth(), hot == 1 ? 2.0f : 1.0f);
            const float dash[] = { 5.0f, 4.0f }; g.setColour (amber().withAlpha (hot == 2 ? 1.0f : 0.7f)); g.drawDashedLine (juce::Line<float> (h.getX(), yc, h.getRight(), yc), dash, 2, hot == 2 ? 2.0f : 1.0f);
            g.setColour (amber()); g.setFont (Theme::font (9.0f, true));
            g.drawText ("OPEN " + juce::String (pg->bank[P::threshold], 1) + " dB", (int) h.getX() + 6, (int) yo - 15, 160, 12, juce::Justification::left);
            g.setColour (amber().withAlpha (0.8f)); g.drawText ("CLOSE " + juce::String (pg->bank[P::threshold] - pg->bank[P::hysteresis], 1) + " dB", (int) h.getX() + 6, (int) yc + 3, 160, 12, juce::Justification::left);
        }

        // ---- right column: transfer curve, the portcullis, the key filter
        juce::Rectangle<float> keyFilterR() const { auto r = sidePane().reduced (14.0f, 10.0f); return r.removeFromBottom (92.0f); }
        void paintSide (juce::Graphics& g)
        {
            const auto r = sidePane(); pane (g, r, "");
            auto in = r.reduced (14.0f, 12.0f);
            auto top = in.removeFromTop (in.getHeight() - 104.0f);
            const float side = std::min (top.getHeight() - 24.0f, top.getWidth() * 0.60f);
            auto cv = top.removeFromLeft (side).withSizeKeepingCentre (side, side).reduced (2.0f); top.removeFromLeft (12.0f); auto gateIcon = top.withTrimmedBottom (26.0f);
            // transfer curve: key level (x) against output level (y)
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (cv, 6.0f);
            auto px = [&] (double dbv) { return cv.getX() + (float) juce::jlimit (0.0, 1.0, (dbv + 84.0) / 84.0) * cv.getWidth(); };
            auto py = [&] (double dbv) { return cv.getBottom() - (float) juce::jlimit (0.0, 1.0, (dbv + 84.0) / 84.0) * cv.getHeight(); };
            for (int d = -84; d <= 0; d += 12) { g.setColour (text (0.05f)); g.drawVerticalLine ((int) px (d), cv.getY(), cv.getBottom()); g.drawHorizontalLine ((int) py (d), cv.getX(), cv.getRight()); }
            juce::Path c; for (int i = 0; i <= 168; ++i) { const double x = -84.0 + i * 0.5, y = x + pg->curveDb (x); if (i == 0) c.startNewSubPath (px (x), py (y)); else c.lineTo (px (x), py (y)); }
            g.setColour (amber().withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (6.0f)); g.setColour (amber()); g.strokePath (c, juce::PathStrokeType (1.8f));
            const double k = pg->keyNow.load();
            if (k > -84) { const double yo = k + pg->gainNow.load(); g.setColour (steel().withAlpha (0.35f)); g.fillEllipse (px (k) - 7, py (yo) - 7, 14, 14); g.setColour (text (1.0f)); g.fillEllipse (px (k) - 3.5f, py (yo) - 3.5f, 7, 7); }
            g.setColour (text (0.35f)); g.setFont (Theme::font (8.0f, true)); g.drawText ("KEY", cv.reduced (4.0f).removeFromBottom (10.0f), juce::Justification::right); g.drawText ("OUT", cv.reduced (4.0f).removeFromTop (10.0f), juce::Justification::left);
            // the portcullis: an arch, the gate raised by the openness
            {
                auto arch = gateIcon.withSizeKeepingCentre (gateIcon.getWidth() - 8.0f, std::min (gateIcon.getHeight() - 8.0f, (gateIcon.getWidth() - 8.0f) * 1.6f));
                juce::Path frame; frame.startNewSubPath (arch.getX(), arch.getBottom()); frame.lineTo (arch.getX(), arch.getY() + arch.getWidth() * 0.5f);
                frame.addCentredArc (arch.getCentreX(), arch.getY() + arch.getWidth() * 0.5f, arch.getWidth() * 0.5f, arch.getWidth() * 0.5f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
                frame.lineTo (arch.getRight(), arch.getBottom());
                juce::Path inner (frame); inner.closeSubPath();
                g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillPath (inner);
                const float openness = pg->openNow.load(), drop = (1.0f - (pg->mode.get() == 2 ? 1.0f - openness : openness)) * arch.getHeight();
                {
                    juce::Graphics::ScopedSaveState ss (g); g.reduceClipRegion (inner);
                    g.setColour (steel().withAlpha (0.10f)); g.fillRect (arch.withHeight (arch.getHeight() - drop).translated (0, drop));   // light through the opening
                    const float top0 = arch.getY() - arch.getHeight() + drop;
                    for (int b = 0; b <= 6; ++b) { const float x = arch.getX() + arch.getWidth() * b / 6.0f; g.setGradientFill (juce::ColourGradient (iron().brighter (0.5f), x - 2, 0, iron().darker (0.5f), x + 2, 0, false)); g.fillRect (x - 2.0f, top0, 4.0f, arch.getHeight()); g.setColour (amber()); juce::Path tip; tip.addTriangle (x - 3.0f, top0 + arch.getHeight(), x + 3.0f, top0 + arch.getHeight(), x, top0 + arch.getHeight() + 7.0f); g.fillPath (tip); }
                    for (int b = 1; b < 7; ++b) { const float y = top0 + arch.getHeight() * b / 7.0f; g.setColour (iron().brighter (0.2f)); g.fillRect (arch.getX(), y - 1.5f, arch.getWidth(), 3.0f); }
                }
                g.setColour (steel().withAlpha (0.6f)); g.strokePath (frame, juce::PathStrokeType (2.0f));
                g.setColour (text (0.95f)); g.setFont (Theme::font (16.0f, true));
                const float gdb = pg->gainNow.load();
                g.drawText (gdb > -0.05f ? juce::String ("OPEN") : juce::String (gdb, 1) + " dB", arch.withY (arch.getBottom() + 2).withHeight (20.0f).expanded (30.0f, 0), juce::Justification::centred);
            }
            // key filter
            const auto kr = keyFilterR();
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (kr, 6.0f);
            g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("KEY FILTER  -  drag the corners", kr.reduced (8.0f, 4.0f).removeFromTop (11.0f), juce::Justification::left);
            const auto kp = kr.reduced (10.0f, 18.0f).withTrimmedTop (2.0f);
            auto kx = [&] (double hz) { return kp.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * kp.getWidth(); };
            const double hpF = pg->bank[P::keyHp], lpF = pg->bank[P::keyLp];
            juce::Path kc; for (int i = 0; i <= 100; ++i) { const double hz = 20.0 * std::pow (1000.0, i / 100.0); const double a2 = std::pow (hz / hpF, 4.0), b2 = std::pow (hz / lpF, 4.0);
                double m = 1; if (hpF > 20.5) m *= std::sqrt (a2 / (1 + a2)); if (lpF < 19999) m *= std::sqrt (1 / (1 + b2));
                const float y = kp.getY() + (float) juce::jlimit (0.0, 1.0, -20 * std::log10 (std::max (1e-6, m)) / 36.0) * kp.getHeight(); if (i == 0) kc.startNewSubPath (kx (hz), y); else kc.lineTo (kx (hz), y); }
            g.setColour (steel()); g.strokePath (kc, juce::PathStrokeType (1.6f));
            for (auto [hz, id] : { std::pair<double, int> { hpF, 3 }, { lpF, 4 } })
            {
                const float x = kx (hz), y = kp.getY() + (float) (3.0 / 36.0) * kp.getHeight();
                g.setColour (hot == id ? amber() : steel()); g.fillEllipse (x - 5, y - 5, 10, 10); g.setColour (night()); g.fillEllipse (x - 2, y - 2, 4, 4);
            }
            g.setColour (text (0.6f)); g.drawText ((hpF > 20.5 ? "HP " + juce::String ((int) hpF) + " Hz" : juce::String ("HP OFF")) + "    " + (lpF < 19999 ? "LP " + juce::String (lpF / 1000.0, 1) + " kHz" : juce::String ("LP OFF")), kr.reduced (8.0f, 3.0f).removeFromBottom (11.0f), juce::Justification::centred);
        }

        // ---- top bar
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t) { return juce::GlyphArrangement::getStringWidth (Theme::font (9.5f, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c)
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (9.5f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void group (juce::Graphics& g, juce::Rectangle<float>& bar, const juce::StringArray& names, int cur, int base, juce::Colour c)
        { for (int i = 0; i < names.size(); ++i) { const auto t = names[i].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 20.0f), t, cur == i, base + i, c); bar.removeFromLeft (3.0f); } bar.removeFromLeft (12.0f); }
        void paintTopBar (juce::Graphics& g)
        {
            hits.clear(); auto bar = topBar();
            group (g, bar, P::modeNames(), pg->mode.get(), 10, amber());
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("KEY", bar.removeFromLeft (28.0f), juce::Justification::centredLeft);
            group (g, bar, P::keyNames(), pg->keySource.get(), 20, steel());
            if (pg->keySource.get() == 2) { chip (g, bar.removeFromLeft (80.0f), "VELOCITY", pg->velocity.get(), 40, steel()); bar.removeFromLeft (12.0f); }
            group (g, bar, P::curveNames(), pg->curve.get(), 30, steel());
            chip (g, bar.removeFromLeft (90.0f), "LISTEN KEY", pg->listenKey.get(), 41, amber()); bar.removeFromLeft (4.0f);
            chip (g, bar.removeFromLeft (56.0f), "FLIP", pg->flip.get(), 42, amber());
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true)); g.drawText ("LATENCY " + juce::String (pg->getLatencySeconds() * 1000.0, 1) + " MS", bar, juce::Justification::right);
        }
        juce::UndoManager* um() const { return &pg->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Portcullis setting"); auto& s = *pg;
            if (id >= 10 && id < 13) s.mode.setValue (id - 10, um());
            else if (id >= 20 && id < 23) s.keySource.setValue (id - 20, um());
            else if (id >= 30 && id < 33) s.curve.setValue (id - 30, um());
            else if (id == 40) s.velocity.setValue (! s.velocity.get(), um());
            else if (id == 41) s.listenKey.setValue (! s.listenKey.get(), um());
            else if (id == 42) s.flip.setValue (! s.flip.get(), um());
            changed();
        }

        // ---- the deck: forged octagonal knobs
        struct KnobDef { int param; const char* label; };
        static const std::vector<KnobDef>& defs()
        {
            static const std::vector<KnobDef> d { { P::threshold, "THRESHOLD" }, { P::hysteresis, "HYSTERESIS" }, { P::range, "RANGE" }, { P::ratio, "RATIO" },
                { P::attack, "ATTACK" }, { P::hold, "HOLD" }, { P::release, "RELEASE" }, { P::lookahead, "LOOKAHEAD" }, { P::keyHp, "KEY HP" }, { P::keyLp, "KEY LP" }, { P::output, "OUTPUT" } };
            return d;
        }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1, hot = 0, dragHot = 0; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *pg->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); changed(); }
        bool enabled (int pi_) const { return ! (pi_ == P::ratio && pg->mode.get() != 1) && ! ((pi_ == P::keyHp || pi_ == P::keyLp) && pg->keySource.get() == 2); }
        void layoutKnobs() { knobR.clear(); auto d = deck().reduced (16.0f, 12.0f); const int n = (int) defs().size(); for (int i = 0; i < n; ++i) knobR.push_back ({ d.getX() + d.getWidth() / n * i, d.getY(), d.getWidth() / n, d.getHeight() }); }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, bool hotK, bool active, bool en)
        {
            const float al = en ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (24.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 10.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text ((hotK ? 0.95f : 0.55f) * al)); g.setFont (Theme::font (8.5f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            // amber value ring with tick notches
            const float ar = rO * 0.97f;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f * al)); g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, a0, av, true);
            g.setColour (amber().withAlpha (0.25f * al)); g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (amber().withAlpha (al)); g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // forged octagon
            const float ro = rO * 0.76f; juce::Path oct;
            for (int k = 0; k < 8; ++k) { const float a = av + juce::MathConstants<float>::pi / 8.0f + juce::MathConstants<float>::twoPi * k / 8.0f; const juce::Point<float> pt (c.x + ro * std::sin (a), c.y - ro * std::cos (a)); if (k == 0) oct.startNewSubPath (pt); else oct.lineTo (pt); }
            oct.closeSubPath();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5b6672).withAlpha (al), c.x - ro, c.y - ro, juce::Colour (0xff12161b).withAlpha (al), c.x + ro, c.y + ro, false)); g.fillPath (oct);
            g.setColour (juce::Colours::black.withAlpha (0.7f * al)); g.strokePath (oct, juce::PathStrokeType (1.2f));
            g.setColour (juce::Colours::white.withAlpha (0.10f * al)); g.strokePath (oct, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (-0.6f, -0.6f));
            // rivets at four corners (turn with the knob)
            for (int k = 0; k < 4; ++k)
            {
                const float a = av + juce::MathConstants<float>::pi / 4.0f + juce::MathConstants<float>::halfPi * k, rr = ro * 0.72f;
                const juce::Point<float> pt (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa9b4bf).withAlpha (al), pt.x - 2, pt.y - 2, juce::Colour (0xff2b3138).withAlpha (al), pt.x + 2, pt.y + 2, false)); g.fillEllipse (pt.x - 2.2f, pt.y - 2.2f, 4.4f, 4.4f);
            }
            // centre boss + pointer
            const float rc = ro * 0.38f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d454e).withAlpha (al), c.x - rc, c.y - rc, juce::Colour (0xff0d1013).withAlpha (al), c.x + rc, c.y + rc, false)); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            const juce::Point<float> p1 (c.x + rc * 0.6f * std::sin (av), c.y - rc * 0.6f * std::cos (av)), p2 (c.x + ro * 0.95f * std::sin (av), c.y - ro * 0.95f * std::cos (av));
            g.setColour (amber().withAlpha (0.35f * al)); g.drawLine ({ p1, p2 }, 5.0f); g.setColour (amber().withAlpha (al)); g.drawLine ({ p1, p2 }, 2.0f);
            if (hotK) { g.setColour (steel()); g.strokePath (oct, juce::PathStrokeType (1.4f)); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 4.0f, 86.0f), 19.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 9.5f);
            g.setColour (active ? amber() : text (0.1f)); g.drawRoundedRectangle (pill, 9.5f, 1.0f);
            g.setColour ((active ? amber() : text (0.95f)).withMultipliedAlpha (al)); g.setFont (Theme::font (10.5f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (int i) const
        {
            const float v = param (i).getCurrentValue();
            switch (i)
            {
                case P::ratio:     return "1:" + juce::String (v, 1);
                case P::attack:    return juce::String (v, v < 1 ? 2 : 1) + " ms";
                case P::hold: case P::release: return v >= 1000 ? juce::String (v / 1000.0f, 2) + " s" : juce::String ((int) std::lround (v)) + " ms";
                case P::lookahead: return v < 0.05f ? juce::String ("OFF") : juce::String (v, 1) + " ms";
                case P::keyHp:     return v <= 20.5f ? juce::String ("OFF") : juce::String ((int) std::lround (v)) + " Hz";
                case P::keyLp:     return v >= 19999.0f ? juce::String ("OFF") : juce::String (v / 1000.0f, 1) + " kHz";
                case P::output:    return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
                case P::range:     return juce::String ((int) std::lround (v)) + " dB";
                default:           return juce::String (v, 1) + " dB";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10151b).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (steel().withAlpha (0.18f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            for (size_t i = 0; i < defs().size(); ++i) drawKnob (g, knobR[i], defs()[i].label, knobText (defs()[i].param), norm (defs()[i].param), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, enabled (defs()[i].param));
        }

        // ---- mouse
        int hotAt (juce::Point<float> pt) const
        {
            const auto h = hist();
            if (h.contains (pt))
            {
                if (std::abs (pt.y - yLev (pg->bank[P::threshold])) < 6) return 1;
                if (std::abs (pt.y - yLev (pg->bank[P::threshold] - pg->bank[P::hysteresis])) < 6) return 2;
            }
            const auto kp = keyFilterR().reduced (10.0f, 18.0f).withTrimmedTop (2.0f);
            if (keyFilterR().contains (pt) && pg->keySource.get() != 2)
            {
                auto kx = [&] (double hz) { return kp.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * kp.getWidth(); };
                if (std::abs (pt.x - kx (pg->bank[P::keyHp])) < 8) return 3;
                if (std::abs (pt.x - kx (pg->bank[P::keyLp])) < 8) return 4;
            }
            return 0;
        }
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (4.0f, 0).contains (p) && enabled (defs()[i].param)) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), h = hotAt (e.position);
            setMouseCursor (k >= 0 || h == 1 || h == 2 ? juce::MouseCursor::UpDownResizeCursor : h ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || h != hot) { hoverKnob = k; hot = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hot = 0; repaint(); }
        static int paramForHot (int h) { return h == 1 ? P::threshold : h == 2 ? P::hysteresis : h == 3 ? P::keyHp : P::keyLp; }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (pg == nullptr) return;
            for (auto& hh : hits) if (hh.r.contains (e.position)) { hitAction (hh.id); return; }
            if (const int h = hotAt (e.position)) { dragHot = h; um()->beginNewTransaction ("Portcullis drag"); param (paramForHot (h)).parameterChangeGestureBegin(); return; }
            const int k = knobAt (e.position);
            if (k >= 0) { dragKnob = k; dragStart = e.position; dragStartNorm = norm (defs()[(size_t) k].param); um()->beginNewTransaction (juce::String ("Portcullis ") + defs()[(size_t) k].label); param (defs()[(size_t) k].param).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragHot)
            {
                const auto h = hist(); const double dbAt = -84.0 * juce::jlimit (0.0, 1.0, (double) ((e.position.y - h.getY()) / h.getHeight()));
                if (dragHot == 1) setValue (P::threshold, (float) dbAt);
                else if (dragHot == 2) setValue (P::hysteresis, (float) (pg->bank[P::threshold] - dbAt));
                else { const auto kp = keyFilterR().reduced (10.0f, 18.0f); const double hz = 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((e.position.x - kp.getX()) / kp.getWidth()))); setValue (dragHot == 3 ? P::keyHp : P::keyLp, (float) hz); }
                return;
            }
            if (dragKnob < 0) return;
            setNorm (defs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragHot) param (paramForHot (dragHot)).parameterChangeGestureEnd();
            if (dragKnob >= 0) param (defs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; dragHot = 0; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = defs()[(size_t) k].param; um()->beginNewTransaction ("Portcullis reset"); param (pi_).setParameter (pg->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Portcullis wheel"); const int pi_ = defs()[(size_t) k].param; setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
