// Included inside namespace Skins (PluginSkins.h): the Torque Comp face (1.14 Studio Line, phase B).
//
// Squelchmoth's own "heat under pressure" design on the Higgsfield plate pa_torquecomp (coiled copper filaments):
//  - top bar: the style (arrows, menu, what it does), key source, routing, character, oversampling, the auto
//    switches, audition, latency
//  - the history: input as a smoky fill, output as a line, gain reduction glowing down from the top, the threshold
//  - the transfer curve: the real static curve of the style and the settings, unity faint, the knee shaded, a live
//    dot where the signal is right now
//  - meters: IN, GR, OUT with peak holds and the momentary loudness
//  - the key panel: the key EQ's curve with up to six draggable nodes
//  - the knob deck: precision knobs drawn here (machined body, tick ring, glowing value arc, pointer, readout);
//    drag (Shift = fine), wheel, double-click = default. Every move is automation-ready and one undo step.

    struct TorqueViz : public Viz, private juce::Timer
    {
        TorqueViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), tq (dynamic_cast<SqTorqueCompPlugin*> (&p))
        {
            setInterceptsMouseClicks (true, false);
            startTimerHz (30);
        }
        SqTorqueCompPlugin* tq;
        std::function<void()> onChange;

        // ---- palette: charcoal, copper, amber, ember
        static juce::Colour ink()    { return juce::Colour (0xff09070a); }
        static juce::Colour coal()   { return juce::Colour (0xff17120f); }
        static juce::Colour copper() { return juce::Colour (0xffe0763a); }
        static juce::Colour amber()  { return juce::Colour (0xffffb347); }
        static juce::Colour ember()  { return juce::Colour (0xffff4a2e); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfff6ebe2).withAlpha (a); }

        // ---- layout
        juce::Rectangle<float> all() const      { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const   { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const     { return all().reduced (16.0f, 0).removeFromBottom (306.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const   { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (314.0f); return r; }
        juce::Rectangle<float> history() const  { auto r = middle(); return r.removeFromLeft (r.getWidth() * 0.50f); }
        juce::Rectangle<float> column() const   { auto r = middle(); r.removeFromLeft (r.getWidth() * 0.50f + 12.0f); r.removeFromRight (136.0f); return r; }
        juce::Rectangle<float> transfer() const { auto r = column(); return r.removeFromTop (r.getHeight() * 0.64f); }
        juce::Rectangle<float> keyPanel() const { auto r = column(); r.removeFromTop (r.getHeight() * 0.64f + 10.0f); return r; }
        juce::Rectangle<float> meters() const   { auto r = middle(); return r.removeFromRight (124.0f); }

        // ---- the knob deck
        struct KnobDef { int param; const char* label; bool big; bool bipolar; };
        static const std::vector<KnobDef>& knobDefs()
        {
            using P = SqTorqueCompPlugin;
            static const std::vector<KnobDef> k {
                { P::threshold, "THRESHOLD", true, false }, { P::ratio, "RATIO", true, false }, { P::knee, "KNEE", true, false },
                { P::attack, "ATTACK", true, false }, { P::release, "RELEASE", true, false }, { P::mix, "MIX", true, false }, { P::makeup, "MAKEUP", true, true },
                { P::range, "RANGE", false, false }, { P::hold, "HOLD", false, false }, { P::lookahead, "LOOKAHEAD", false, false },
                { P::link, "STEREO LINK", false, false }, { P::drive, "DRIVE", false, false }, { P::output, "OUTPUT", false, true } };
            return k;
        }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *tq->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float n) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n)), juce::sendNotification); changed(); }
        juce::UndoManager* um() const { return &tq->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }

        void layoutKnobs()
        {
            knobR.clear();
            auto d = deck().reduced (18.0f, 12.0f);
            auto row1 = d.removeFromTop (d.getHeight() * 0.53f); d.removeFromTop (10.0f); auto row2 = d;
            int nBig = 0, nSmall = 0; for (auto& k : knobDefs()) (k.big ? nBig : nSmall)++;
            const float w1 = row1.getWidth() / nBig, w2 = row2.getWidth() / nSmall;
            int ib = 0, is = 0;
            for (auto& k : knobDefs())
                knobR.push_back (k.big ? juce::Rectangle<float> (row1.getX() + w1 * ib++, row1.getY(), w1, row1.getHeight())
                                       : juce::Rectangle<float> (row2.getX() + w2 * is++, row2.getY(), w2, row2.getHeight()));
        }

        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, bool bipolar, bool big, bool hot, bool active)
        {
            const float labelH = 14.0f, readH = 20.0f;
            auto lab = cell.removeFromTop (labelH); auto read = cell.removeFromBottom (readH + 4.0f).withTrimmedTop (4.0f);
            const float rOuter = std::min (cell.getWidth(), cell.getHeight()) * 0.5f - 4.0f;
            const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            // label, letter-spaced
            g.setColour (text (hot ? 0.95f : 0.55f)); g.setFont (Theme::font (big ? 10.0f : 9.0f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            // shadow well
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), c.x, c.y, juce::Colours::transparentBlack, c.x, c.y + rOuter * 1.25f, true));
            g.fillEllipse (c.x - rOuter * 1.15f, c.y - rOuter * 1.05f, rOuter * 2.3f, rOuter * 2.3f);
            // tick ring
            const int ticks = big ? 40 : 30;
            for (int t = 0; t <= ticks; ++t)
            {
                const float a = a0 + (a1 - a0) * t / ticks, major = t % 5 == 0 ? 1.0f : 0.0f;
                const float r1 = rOuter * (major > 0 ? 0.86f : 0.90f), r2 = rOuter * 0.97f;
                const bool lit = bipolar ? ((t / (float) ticks >= std::min (0.5f, v)) && (t / (float) ticks <= std::max (0.5f, v))) : (t / (float) ticks <= v);
                g.setColour (lit ? amber().withAlpha (major > 0 ? 0.95f : 0.65f) : text (major > 0 ? 0.30f : 0.14f));
                g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), major > 0 ? 1.4f : 1.0f);
            }
            // value arc: track, then the lit part with a glow
            const float ar = rOuter * 0.78f;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (track, juce::PathStrokeType (big ? 5.0f : 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float from = bipolar ? (a0 + a1) * 0.5f : a0;
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (from, av), std::max (from, av), true);
            juce::ColourGradient cg (copper(), c.x - ar, c.y + ar, amber(), c.x + ar, c.y - ar, false);
            g.setGradientFill (juce::ColourGradient (copper().withAlpha (0.25f), c.x - ar, c.y + ar, amber().withAlpha (0.25f), c.x + ar, c.y - ar, false));
            g.strokePath (arc, juce::PathStrokeType (big ? 11.0f : 9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (cg); g.strokePath (arc, juce::PathStrokeType (big ? 4.0f : 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const juce::Point<float> cap (c.x + ar * std::sin (av), c.y - ar * std::cos (av));
            g.setColour (amber().brighter (0.5f).withAlpha (0.35f)); g.fillEllipse (cap.x - 6, cap.y - 6, 12, 12);
            g.setColour (juce::Colours::white); g.fillEllipse (cap.x - 2.2f, cap.y - 2.2f, 4.4f, 4.4f);
            // machined body
            const float rb = rOuter * 0.62f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a302a), c.x - rb * 0.5f, c.y - rb * 0.7f, juce::Colour (0xff0f0b09), c.x + rb * 0.6f, c.y + rb, true));
            g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            for (int k = 1; k <= 6; ++k) { const float rr = rb * (0.2f + 0.13f * k); g.setColour (text (0.025f + 0.008f * (k % 2))); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), c.x - rb * 0.4f, c.y - rb * 0.8f, juce::Colours::transparentWhite, c.x, c.y, true));
            g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            g.setColour (hot ? amber().withAlpha (0.55f) : text (0.12f)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
            // pointer
            const juce::Point<float> p1 (c.x + rb * 0.35f * std::sin (av), c.y - rb * 0.35f * std::cos (av)), p2 (c.x + rb * 0.9f * std::sin (av), c.y - rb * 0.9f * std::cos (av));
            g.setColour (amber().withAlpha (0.35f)); g.drawLine ({ p1, p2 }, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.95f)); g.drawLine ({ p1, p2 }, 2.0f);
            // readout pill
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 8.0f, big ? 96.0f : 84.0f), readH);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, readH * 0.5f);
            g.setColour (active ? amber().withAlpha (0.9f) : text (0.10f)); g.drawRoundedRectangle (pill, readH * 0.5f, 1.0f);
            g.setColour (active ? amber() : text (0.95f)); g.setFont (Theme::font (big ? 11.5f : 10.5f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (int i) const
        {
            using P = SqTorqueCompPlugin;
            const float v = param (i).getCurrentValue();
            switch (i)
            {
                case P::ratio:     return v >= 99.5f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e : 1")) : juce::String (v, v < 10 ? 2 : 1) + " : 1";
                case P::attack:    return v < 1.0f ? juce::String (v * 1000.0f, 0) + " us" : juce::String (v, v < 10 ? 2 : 1) + " ms";
                case P::release:   return v >= 1000 ? juce::String (v / 1000.0f, 2) + " s" : juce::String ((int) v) + " ms";
                case P::hold: case P::lookahead: return juce::String (v, v < 10 ? 1 : 0) + " ms";
                case P::link: case P::mix: return juce::String ((int) std::lround (v)) + " %";
                default:           return (v > 0 && (i == P::makeup || i == P::output) ? "+" : "") + juce::String (v, 1) + " dB";
            }
        }

        // ---- timer
        float pkIn = -100, pkOut = -100, pkGr = 0;
        void timerCallback() override
        {
            if (tq == nullptr) return;
            pkIn = std::max (tq->inNow.load(), pkIn - 0.6f); pkOut = std::max (tq->outNow.load(), pkOut - 0.6f); pkGr = std::min (tq->grNow.load(), pkGr + 0.3f);
            repaint();
        }

        // ---- painting
        void paint (juce::Graphics& g) override
        {
            if (tq == nullptr) return;
            const auto a = all();
            g.setColour (ink()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("torquecomp");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.75f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (ink().withAlpha (0.72f), a.getCentreX(), a.getCentreY(), ink().withAlpha (0.18f), a.getX(), a.getY(), true));
                    g.fillRect (a);
                }
            }
            g.setColour (copper().withAlpha (0.22f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            paintTopBar (g);
            paintHistory (g);
            paintTransfer (g);
            paintMeters (g);
            paintKey (g);
            paintDeck (g);
        }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (ink().withAlpha (0.66f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (text (0.05f), 0, r.getY(), text (0.0f), 0, r.getY() + 36.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (text (0.08f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        void paintHistory (juce::Graphics& g)
        {
            const auto r = history(); pane (g, r, "GAIN REDUCTION HISTORY");
            const auto p = r.reduced (12.0f, 26.0f).withTrimmedBottom (-14.0f);
            auto yLev = [&] (float db) { return p.getY() + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * p.getHeight(); };
            // grid: every 6 dB
            g.setFont (Theme::font (8.5f, true));
            for (int d = 0; d <= 60; d += 6) { const float y = yLev ((float) -d); g.setColour (text (d == 0 ? 0.12f : 0.04f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight()); g.setColour (text (0.3f)); g.drawText (juce::String (-d), (int) p.getRight() - 24, (int) y - 10, 22, 10, juce::Justification::right); }
            const int N = SqTorqueCompPlugin::histLen, w = tq->histW.load(), cols = std::min (N - 1, (int) p.getWidth());
            juce::Path inP, outP, grP;
            for (int i = 0; i <= cols; ++i)
            {
                const int idx = (w - 1 - (cols - i)) & (N - 1);
                const float x = p.getX() + p.getWidth() * i / (float) cols;
                const float yi = yLev (tq->histIn[(size_t) idx]), yo = yLev (tq->histOut[(size_t) idx]), yg = p.getY() + juce::jlimit (0.0f, 1.0f, -tq->histGr[(size_t) idx] / 30.0f) * p.getHeight() * 0.5f;
                if (i == 0) { inP.startNewSubPath (x, p.getBottom()); inP.lineTo (x, yi); outP.startNewSubPath (x, yo); grP.startNewSubPath (x, p.getY()); grP.lineTo (x, yg); }
                else { inP.lineTo (x, yi); outP.lineTo (x, yo); grP.lineTo (x, yg); }
            }
            inP.lineTo (p.getRight(), p.getBottom()); inP.closeSubPath();
            grP.lineTo (p.getRight(), p.getY()); grP.closeSubPath();
            g.setGradientFill (juce::ColourGradient (text (0.20f), 0, p.getY(), text (0.03f), 0, p.getBottom(), false)); g.fillPath (inP);
            g.setGradientFill (juce::ColourGradient (ember().withAlpha (0.65f), 0, p.getY(), amber().withAlpha (0.08f), 0, p.getY() + p.getHeight() * 0.5f, false)); g.fillPath (grP);
            g.setColour (amber().withAlpha (0.25f)); g.strokePath (outP, juce::PathStrokeType (4.0f));
            g.setColour (amber()); g.strokePath (outP, juce::PathStrokeType (1.4f));
            // threshold
            const float T = tq->autoThreshold.get() ? tq->autoThreshNow.load() : tq->bank[SqTorqueCompPlugin::threshold];
            const float yt = yLev (T);
            const float dash[] = { 5.0f, 4.0f }; juce::Path tl; tl.startNewSubPath (p.getX(), yt); tl.lineTo (p.getRight(), yt);
            juce::Path td; juce::PathStrokeType (1.0f).createDashedStroke (td, tl, dash, 2); g.setColour (copper().withAlpha (0.8f)); g.fillPath (td);
            g.setColour (copper()); g.setFont (Theme::font (9.0f, true)); g.drawText ("THRESHOLD " + juce::String (T, 1), (int) p.getX() + 4, (int) yt - 13, 140, 11, juce::Justification::left);
        }

        void paintTransfer (juce::Graphics& g)
        {
            auto r = transfer(); pane (g, r, "TRANSFER");
            auto inner = r.reduced (26.0f, 26.0f).translated (6.0f, 6.0f);
            const float side = std::min (inner.getWidth(), inner.getHeight());
            const auto p = inner.withSizeKeepingCentre (side, side);
            auto xOf = [&] (double db) { return p.getX() + (float) juce::jlimit (0.0, 1.0, (db + 60.0) / 60.0) * p.getWidth(); };
            auto yOf = [&] (double db) { return p.getBottom() - (float) juce::jlimit (0.0, 1.0, (db + 60.0) / 60.0) * p.getHeight(); };
            g.setFont (Theme::font (8.0f, true));
            for (int d = 0; d <= 60; d += 12)
            {
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) xOf (-d), p.getY(), p.getBottom()); g.drawHorizontalLine ((int) yOf (-d), p.getX(), p.getRight());
                g.setColour (text (0.3f)); g.drawText (juce::String (-d), (int) xOf (-d) - 12, (int) p.getBottom() + 2, 24, 10, juce::Justification::centred);
            }
            g.setColour (text (0.12f)); g.drawLine (xOf (-60), yOf (-60), xOf (0), yOf (0), 1.0f);
            // knee shading
            const auto& st = SqTorque::styles()[(size_t) juce::jlimit (0, 13, tq->style.get())];
            const float T = tq->autoThreshold.get() ? tq->autoThreshNow.load() : tq->bank[SqTorqueCompPlugin::threshold];
            const float W = tq->bank[SqTorqueCompPlugin::knee] + (float) st.kneeExtra;
            if (W > 0.1f) { g.setColour (copper().withAlpha (0.07f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xOf (T - W / 2), p.getY(), xOf (T + W / 2), p.getBottom())); }
            juce::Path c;
            for (int i = 0; i <= 240; ++i) { const double in = -60.0 + 60.0 * i / 240.0; const float x = xOf (in), y = yOf (tq->curveOutDb (in)); if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y); }
            {
                juce::Graphics::ScopedSaveState s (g); g.reduceClipRegion (p.toNearestInt().expanded (2));
                g.setColour (amber().withAlpha (0.12f)); g.strokePath (c, juce::PathStrokeType (10.0f));
                g.setColour (amber().withAlpha (0.35f)); g.strokePath (c, juce::PathStrokeType (4.0f));
                g.setColour (juce::Colours::white.withAlpha (0.9f)); g.strokePath (c, juce::PathStrokeType (1.6f));
            }
            // live dot
            const float in = tq->inNow.load();
            if (in > -60)
            {
                const float x = xOf (in), y = yOf (tq->curveOutDb (in) + tq->grNow.load() - SqTorque::staticGainDb (in, T, tq->bank[SqTorqueCompPlugin::ratio], W, tq->bank[SqTorqueCompPlugin::range], st.upward, st.muLaw) * std::max (1.0, tq->bank[SqTorqueCompPlugin::mix] / 100.0));
                g.setColour (ember().withAlpha (0.3f)); g.fillEllipse (x - 9, y - 9, 18, 18);
                g.setColour (amber()); g.fillEllipse (x - 4.5f, y - 4.5f, 9, 9);
            }
            g.setColour (text (0.4f)); g.setFont (Theme::font (8.5f, true));
            g.drawText ("IN", p.withTop (p.getBottom() + 12).withHeight (10), juce::Justification::centredRight);
        }

        void paintMeters (juce::Graphics& g)
        {
            auto r = meters(); pane (g, r, "");
            auto m = r.reduced (12.0f, 14.0f);
            auto lufs = m.removeFromBottom (34.0f);
            const float cw = (m.getWidth() - 16.0f) / 3.0f;
            auto bar = [&] (juce::Rectangle<float> b, float v01, juce::ColourGradient cg, const juce::String& label, const juce::String& val, bool fromTop)
            {
                auto lab = b.removeFromBottom (24.0f);
                g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (b, 4.0f);
                auto fill = fromTop ? b.withHeight (b.getHeight() * v01) : b.withTop (b.getBottom() - b.getHeight() * v01);
                if (v01 > 0.001f) { g.setGradientFill (cg); g.fillRoundedRectangle (fill.reduced (2.0f, 0), 3.0f); }
                g.setColour (text (0.5f)); g.setFont (Theme::font (8.5f, true)); g.drawText (label, lab.removeFromTop (11.0f), juce::Justification::centred);
                g.setColour (text (0.9f)); g.setFont (Theme::font (9.5f, true)); g.drawText (val, lab, juce::Justification::centred);
            };
            auto lev = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
            juce::ColourGradient lg (juce::Colour (0xff6bd38a), 0, m.getBottom(), ember(), 0, m.getY(), false); lg.addColour (0.75, amber());
            bar (m.removeFromLeft (cw), lev (pkIn), lg, "IN", juce::String (pkIn > -99 ? pkIn : -99.0f, 1), false); m.removeFromLeft (8.0f);
            juce::ColourGradient gg (amber(), 0, m.getY(), ember(), 0, m.getBottom(), false);
            bar (m.removeFromLeft (cw), juce::jlimit (0.0f, 1.0f, -pkGr / 30.0f), gg, "GR", juce::String (pkGr, 1), true); m.removeFromLeft (8.0f);
            bar (m.removeFromLeft (cw), lev (pkOut), lg, "OUT", juce::String (pkOut > -99 ? pkOut : -99.0f, 1), false);
            g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("MOMENTARY", lufs.removeFromTop (12.0f), juce::Justification::centred);
            const float L = tq->lufsNow.load();
            g.setColour (amber()); g.setFont (Theme::font (14.0f, true)); g.drawText (L > -70 ? juce::String (L, 1) + " LUFS" : juce::String ("-- LUFS"), lufs, juce::Justification::centred);
        }

        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (coal().withAlpha (0.92f), 0, d.getY(), ink().withAlpha (0.95f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (copper().withAlpha (0.18f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            // a hairline between the rows
            { const auto in = d.reduced (18.0f, 12.0f); g.setColour (text (0.06f)); g.drawHorizontalLine ((int) (in.getY() + in.getHeight() * 0.53f + 5), d.getX() + 30, d.getRight() - 30); }
            layoutKnobs();
            for (size_t i = 0; i < knobDefs().size(); ++i)
            {
                const auto& k = knobDefs()[i];
                drawKnob (g, knobR[i], k.label, knobText (k.param), norm (k.param), k.bipolar, k.big, (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob);
                StudioUi::knobScale (g, knobR[i], tq->bank.specs[(size_t) k.param], text (1.0f), 14.0f, 24.0f);
            }
        }


        // ---- the key panel: the key EQ curve, up to six nodes
        juce::Rectangle<float> keyPlot() const { return keyPanel().reduced (14.0f, 10.0f).withTrimmedTop (16.0f).withTrimmedBottom (10.0f); }
        static float kx (double hz, juce::Rectangle<float> r) { return r.getX() + r.getWidth() * (float) (std::log (hz / 20.0) / std::log (1000.0)); }
        static double khz (float x, juce::Rectangle<float> r) { return 20.0 * std::pow (1000.0, (x - r.getX()) / r.getWidth()); }
        static float ky (double db, juce::Rectangle<float> r) { return r.getCentreY() - (float) juce::jlimit (-1.1, 1.1, db / 18.0) * r.getHeight() * 0.5f; }
        SqSpectra::Band keyBand (int b) const
        {
            SqSpectra::Band k; const auto& kp = tq->keyProps[(size_t) b];
            k.enabled = kp.on.get(); k.shape = (SqSpectra::Shape) juce::jlimit (0, 9, kp.shape.get()); k.slope = kp.slope.get();
            k.freq = tq->bank[(size_t) (SqTorqueCompPlugin::k1Freq + b * 3)]; k.gainDb = tq->bank[(size_t) (SqTorqueCompPlugin::k1Gain + b * 3)]; k.q = tq->bank[(size_t) (SqTorqueCompPlugin::k1Q + b * 3)];
            return k;
        }
        juce::Point<float> keyNode (int b) const
        {
            const auto k = keyBand (b); const auto r = keyPlot();
            return { kx (juce::jlimit (20.0, 20000.0, k.freq), r), ky (k.hasGain() ? k.gainDb : 0.0, r) };
        }
        int keyDrag = -1; float keyStartFreq = 0, keyStartGain = 0;
        void paintKey (juce::Graphics& g)
        {
            auto r = keyPanel(); pane (g, r, "KEY EQ  -  what the detector hears");
            const auto p = keyPlot();
            for (float f : { 100.0f, 1000.0f, 10000.0f }) { g.setColour (text (0.06f)); g.drawVerticalLine ((int) kx (f, p), p.getY(), p.getBottom()); }
            g.setColour (text (0.12f)); g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());
            juce::Path c; bool any = false;
            for (int i = 0; i <= 200; ++i)
            {
                const double hz = khz (p.getX() + p.getWidth() * i / 200.0f, p);
                std::complex<double> h (1, 0);
                for (int b = 0; b < 6; ++b) { const auto k = keyBand (b); if (k.enabled) { h *= SqSpectra::analogResponse (SqSpectra::prototype (k, k.gainDb), hz); any = true; } }
                const float y = ky (20 * std::log10 (std::max (1e-6, std::abs (h))), p);
                if (i == 0) c.startNewSubPath (p.getX(), y); else c.lineTo (p.getX() + p.getWidth() * i / 200.0f, y);
            }
            {
                juce::Graphics::ScopedSaveState s (g); g.reduceClipRegion (p.toNearestInt());
                juce::Path f (c); f.lineTo (p.getRight(), p.getBottom()); f.lineTo (p.getX(), p.getBottom()); f.closeSubPath();
                g.setGradientFill (juce::ColourGradient (copper().withAlpha (0.22f), 0, p.getY(), copper().withAlpha (0.02f), 0, p.getBottom(), false)); g.fillPath (f);
                g.setColour (amber()); g.strokePath (c, juce::PathStrokeType (1.6f));
            }
            for (int b = 0; b < 6; ++b)
            {
                if (! tq->keyProps[(size_t) b].on.get()) continue;
                const auto n = keyNode (b);
                g.setColour (ember().withAlpha (0.3f)); g.fillEllipse (n.x - 9, n.y - 9, 18, 18);
                g.setGradientFill (juce::ColourGradient (amber().brighter (0.4f), n.x - 3, n.y - 4, copper().darker (0.3f), n.x + 6, n.y + 6, true)); g.fillEllipse (n.x - 7, n.y - 7, 14, 14);
                g.setColour (ink()); g.setFont (Theme::font (8.5f, true)); g.drawText (juce::String (b + 1), juce::Rectangle<float> (n.x - 7, n.y - 7, 14, 14), juce::Justification::centred);
            }
            if (! any)
            {
                g.setColour (text (0.5f)); g.setFont (Theme::font (10.0f));
                g.drawText ("Double-click to shape what the detector hears", p.withTrimmedBottom (p.getHeight() * 0.55f), juce::Justification::centredBottom);
                g.setColour (text (0.32f)); g.setFont (Theme::font (9.0f));
                g.drawText ("a low cut here keeps the bass from pumping the whole mix", p.withTrimmedTop (p.getHeight() * 0.58f), juce::Justification::centredTop);
            }
        }
        int keyHit (juce::Point<float> pos) const { for (int b = 0; b < 6; ++b) if (tq->keyProps[(size_t) b].on.get() && keyNode (b).getDistanceFrom (pos) < 10.0f) return b; return -1; }
        te::AutomatableParameter& keyParam (int b, int which) const { return *tq->bank.params[(size_t) (SqTorqueCompPlugin::k1Freq + b * 3 + which)]; }
        bool keyMouseDown (const juce::MouseEvent& e)
        {
            if (! keyPlot().expanded (6.0f).contains (e.position)) return false;
            const int b = keyHit (e.position);
            if (b >= 0 && e.mods.isPopupMenu())
            {
                juce::PopupMenu m; const int sh = tq->keyProps[(size_t) b].shape.get();
                const int shapes[] = { 0, 4, 5, 2, 3, 6, 1 };
                for (int s : shapes) m.addItem (100 + s, SqSpectra::shapeNames()[s], true, sh == s);
                juce::PopupMenu sl; for (int v : { 6, 12, 18, 24, 36, 48 }) sl.addItem (200 + v, juce::String (v) + " dB/oct", true, tq->keyProps[(size_t) b].slope.get() == v);
                m.addSubMenu ("Slope", sl); m.addSeparator(); m.addItem (1, "Remove");
                m.showMenuAsync (juce::PopupMenu::Options(), [this, b, sp = SafePointer<TorqueViz> (this)] (int r)
                {
                    if (sp == nullptr || r <= 0) return; um()->beginNewTransaction ("Key EQ");
                    if (r == 1) tq->keyProps[(size_t) b].on.setValue (false, um()); else if (r >= 200) tq->keyProps[(size_t) b].slope.setValue (r - 200, um()); else tq->keyProps[(size_t) b].shape.setValue (r - 100, um());
                    changed();
                });
                return true;
            }
            if (b >= 0)
            {
                keyDrag = b; dragStart = e.position; keyStartFreq = keyParam (b, 0).getCurrentValue(); keyStartGain = keyParam (b, 1).getCurrentValue();
                um()->beginNewTransaction ("Key EQ"); keyParam (b, 0).parameterChangeGestureBegin(); keyParam (b, 1).parameterChangeGestureBegin();
            }
            return true;
        }
        bool keyMouseDrag (const juce::MouseEvent& e)
        {
            if (keyDrag < 0) return false;
            const auto p = keyPlot(); const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
            const double hz = khz (kx (keyStartFreq, p) + (e.position.x - dragStart.x) * fine, p);
            keyParam (keyDrag, 0).setParameter ((float) juce::jlimit (20.0, 20000.0, hz), juce::sendNotification);
            if (keyBand (keyDrag).hasGain()) keyParam (keyDrag, 1).setParameter (juce::jlimit (-30.0f, 30.0f, keyStartGain - (e.position.y - dragStart.y) * fine * 36.0f / p.getHeight()), juce::sendNotification);
            changed(); return true;
        }
        bool keyMouseUp()
        {
            if (keyDrag < 0) return false;
            keyParam (keyDrag, 0).parameterChangeGestureEnd(); keyParam (keyDrag, 1).parameterChangeGestureEnd(); keyDrag = -1; return true;
        }
        bool keyDoubleClick (const juce::MouseEvent& e)
        {
            const auto p = keyPlot(); if (! p.expanded (6.0f).contains (e.position)) return false;
            const int hit = keyHit (e.position);
            um()->beginNewTransaction ("Key EQ");
            if (hit >= 0) { tq->keyProps[(size_t) hit].on.setValue (false, um()); changed(); return true; }
            for (int b = 0; b < 6; ++b)
                if (! tq->keyProps[(size_t) b].on.get())
                {
                    const double hz = khz (e.position.x, p);
                    tq->keyProps[(size_t) b].shape.setValue (hz < 150 ? 4 : hz > 8000 ? 5 : 0, um());
                    keyParam (b, 0).setParameter ((float) hz, juce::sendNotification);
                    keyParam (b, 1).setParameter (hz < 150 || hz > 8000 ? 0.0f : juce::jlimit (-30.0f, 30.0f, (float) ((p.getCentreY() - e.position.y) / (p.getHeight() * 0.5f) * 18.0f)), juce::sendNotification);
                    keyParam (b, 2).setParameter (0.707f, juce::sendNotification);
                    tq->keyProps[(size_t) b].on.setValue (true, um());
                    changed(); return true;
                }
            return true;
        }

        // ---- top bar
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 10.0f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id)
        {
            g.setColour (on ? copper().withAlpha (0.25f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? amber().withAlpha (0.85f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (10.0f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void paintTopBar (juce::Graphics& g)
        {
            hits.clear();
            auto bar = topBar();
            // style: arrows + name (menu) + what it does
            const auto& st = SqTorque::styles()[(size_t) juce::jlimit (0, 13, tq->style.get())];
            auto prev = bar.removeFromLeft (26.0f); bar.removeFromLeft (4.0f);
            const juce::String nm = juce::String (st.name).toUpperCase();
            auto name = bar.removeFromLeft (std::max (150.0f, widthOf (nm, 13.0f) + 34.0f)); bar.removeFromLeft (4.0f);
            auto next = bar.removeFromLeft (26.0f); bar.removeFromLeft (12.0f);
            for (auto [rr, glyph, id] : { std::tuple<juce::Rectangle<float>, const char*, int> { prev, "\xe2\x80\xb9", 1 }, { next, "\xe2\x80\xba", 2 } })
            {
                g.setColour (text (0.06f)); g.fillEllipse (rr); g.setColour (text (0.18f)); g.drawEllipse (rr, 1.0f);
                g.setColour (text (0.9f)); g.setFont (Theme::font (16.0f, true)); g.drawText (juce::String (juce::CharPointer_UTF8 (glyph)), rr.translated (0, -1), juce::Justification::centred);
                hits.push_back ({ rr, id });
            }
            g.setGradientFill (juce::ColourGradient (copper().withAlpha (0.55f), name.getX(), name.getY(), ember().withAlpha (0.35f), name.getRight(), name.getBottom(), false));
            g.fillRoundedRectangle (name, name.getHeight() * 0.5f);
            g.setColour (amber()); g.drawRoundedRectangle (name, name.getHeight() * 0.5f, 1.0f);
            g.setColour (juce::Colours::white); g.setFont (Theme::font (13.0f, true)); g.drawText (nm, name, juce::Justification::centred);
            hits.push_back ({ name, 3 });
            // right side first (fixed), then the description fills what's left
            const double lat = tq->getLatencySeconds() * 1000.0;
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("LATENCY " + (lat < 0.01 ? juce::String ("0 MS") : juce::String (lat, 1) + " MS"), bar.removeFromRight (104.0f), juce::Justification::right);
            bar.removeFromRight (8.0f);
            auto rightChip = [&] (const juce::String& t, bool on, int id) { chip (g, bar.removeFromRight (widthOf (t) + 22.0f), t, on, id); bar.removeFromRight (6.0f); };
            rightChip ("LISTEN KEY", tq->audition.get(), 13);
            rightChip ("AUTO GAIN", tq->autoGain.get(), 12);
            rightChip ("AUTO THRESH", tq->autoThreshold.get(), 11);
            rightChip ("AUTO RELEASE", tq->autoRelease.get(), 10);
            rightChip (SqTorqueCompPlugin::oversampleNames()[juce::jlimit (0, 5, tq->oversample.get())], tq->characterMode.get() > 0, 9);
            rightChip ("COLOUR " + SqTorqueCompPlugin::characterNames()[juce::jlimit (0, 4, tq->characterMode.get())].toUpperCase() + (tq->characterMode.get() > 0 ? (tq->characterPost.get() ? " POST" : " PRE") : ""), tq->characterMode.get() > 0, 8);
            rightChip (SqTorque::routingNames()[juce::jlimit (0, 4, tq->routing.get())].toUpperCase(), tq->routing.get() > 0, 7);
            rightChip ("KEY " + SqTorque::keyNames()[juce::jlimit (0, 3, tq->keySource.get())].toUpperCase() + (tq->keySource.get() == 3 ? " " + SqTorqueCompPlugin::tempoNames()[juce::jlimit (0, 4, tq->tempoDiv.get())] : ""), tq->keySource.get() > 0, 6);
            g.setColour (text (0.6f)); g.setFont (Theme::font (10.0f)); g.drawFittedText (st.about, bar.toNearestInt(), juce::Justification::centredLeft, 2);
        }
        void menuFor (const juce::StringArray& names, int cur, std::function<void (int)> set)
        {
            juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<TorqueViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Torque Comp setting");
            auto& t = *tq;
            switch (id)
            {
                case 1: t.style.setValue ((t.style.get() + 13) % 14, um()); changed(); break;
                case 2: t.style.setValue ((t.style.get() + 1) % 14, um()); changed(); break;
                case 3:
                {
                    juce::PopupMenu m;
                    for (int i = 0; i < 14; ++i) m.addItem (i + 1, juce::String (SqTorque::styles()[(size_t) i].name) + "   -   " + SqTorque::styles()[(size_t) i].about, true, i == t.style.get());
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<TorqueViz> (this)] (int r) { if (r > 0 && sp != nullptr) { tq->style.setValue (r - 1, um()); changed(); } });
                    break;
                }
                case 6:
                {
                    juce::PopupMenu m; for (int i = 0; i < 4; ++i) m.addItem (1 + i, SqTorque::keyNames()[i], true, t.keySource.get() == i);
                    juce::PopupMenu td; for (int i = 0; i < 5; ++i) td.addItem (10 + i, SqTorqueCompPlugin::tempoNames()[i], true, t.tempoDiv.get() == i);
                    m.addSubMenu ("Tempo pulse length", td);
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<TorqueViz> (this)] (int r) { if (r <= 0 || sp == nullptr) return; if (r >= 10) tq->tempoDiv.setValue (r - 10, um()); else tq->keySource.setValue (r - 1, um()); changed(); });
                    break;
                }
                case 7: menuFor (SqTorque::routingNames(), t.routing.get(), [this] (int i) { tq->routing.setValue (i, um()); }); break;
                case 8:
                {
                    juce::PopupMenu m; for (int i = 0; i < 5; ++i) m.addItem (1 + i, SqTorqueCompPlugin::characterNames()[i], true, t.characterMode.get() == i);
                    m.addSeparator(); m.addItem (20, "Before the gain stage", true, ! t.characterPost.get()); m.addItem (21, "After the gain stage", true, t.characterPost.get());
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<TorqueViz> (this)] (int r) { if (r <= 0 || sp == nullptr) return; if (r >= 20) tq->characterPost.setValue (r == 21, um()); else tq->characterMode.setValue (r - 1, um()); changed(); });
                    break;
                }
                case 9: menuFor (SqTorqueCompPlugin::oversampleNames(), t.oversample.get(), [this] (int i) { tq->oversample.setValue (i, um()); }); break;
                case 10: t.autoRelease.setValue (! t.autoRelease.get(), um()); changed(); break;
                case 11: t.autoThreshold.setValue (! t.autoThreshold.get(), um()); changed(); break;
                case 12: t.autoGain.setValue (! t.autoGain.get(), um()); changed(); break;
                case 13: t.audition.setValue (! t.audition.get(), um()); changed(); break;
                default: break;
            }
        }

        // ---- mouse
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (6.0f, 0).contains (p)) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob) { hoverKnob = k; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (tq == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (keyMouseDown (e)) return;
            const int k = knobAt (e.position);
            if (k >= 0)
            {
                dragKnob = k; dragStart = e.position; dragStartNorm = norm (knobDefs()[(size_t) k].param);
                um()->beginNewTransaction (juce::String ("Torque ") + knobDefs()[(size_t) k].label);
                param (knobDefs()[(size_t) k].param).parameterChangeGestureBegin();
                repaint();
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (keyMouseDrag (e)) return;
            if (dragKnob < 0) return;
            const float sens = e.mods.isShiftDown() ? 0.0008f : 0.005f;
            setNorm (knobDefs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * sens);
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            keyMouseUp();
            if (dragKnob >= 0) param (knobDefs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (keyDoubleClick (e)) return;
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = knobDefs()[(size_t) k].param;
            um()->beginNewTransaction ("Torque reset");
            param (pi_).setParameter (tq->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Torque wheel");
            const int pi_ = knobDefs()[(size_t) k].param;
            setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
