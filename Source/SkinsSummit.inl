// Included inside namespace Skins (PluginSkins.h): the Summit Limiter face (1.14 Studio Line, phase C).
//
// Squelchmoth's own "glacier at night" design on the Higgsfield plate pa_summitlim (a ridge lit along its edge):
//  - top bar: the algorithm (arrows, menu, what it does), true peak, the band-limit guard, DC, oversampling, dither
//    and its shaping, unity audition, listen to what's removed, latency
//  - the level display: input as frost, output in ice blue, gain reduction carved down from the top in coral, the
//    ceiling as a bright line
//  - the loudness panel: integrated loudness large against the chosen target with the difference as a bar,
//    momentary, short-term, loudness range, true-peak max (red when over the target's limit), reset
//  - the knob deck: frosted-glass knobs drawn here; the two link knobs wake up when linking is set by hand

    struct SummitViz : public Viz, private juce::Timer
    {
        SummitViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sm (dynamic_cast<SqSummitLimiterPlugin*> (&p))
        {
            setInterceptsMouseClicks (true, false);
            startTimerHz (30);
        }
        SqSummitLimiterPlugin* sm;
        std::function<void()> onChange;

        static juce::Colour night() { return juce::Colour (0xff050a12); }
        static juce::Colour ice()   { return juce::Colour (0xff8fe3ff); }
        static juce::Colour teal()  { return juce::Colour (0xff3fd6c6); }
        static juce::Colour frost() { return juce::Colour (0xffdff4ff); }
        static juce::Colour coral() { return juce::Colour (0xffff6a5a); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xffeaf6ff).withAlpha (a); }

        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (236.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (244.0f); r.removeFromRight (meterW() + 10.0f); return r; }
        // the numbers: IN / OUT / GR meters on the right (StudioUi, shared by the Studio Line)
        StudioUi::StudioMeter meter;
        static float meterW() { return StudioUi::StudioMeter::width (true); }
        juce::Rectangle<float> meterR() const { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (244.0f); return r.removeFromRight (meterW()); }
        void paintMeter (juce::Graphics& g) { meter.update (sm->levelIn.load(), sm->levelOut.load(), (float) sm->grNow.load()); meter.draw (g, meterR(), ice(), true); }
        juce::Rectangle<float> levels() const  { auto r = middle(); return r.removeFromLeft (r.getWidth() - 330.0f); }
        juce::Rectangle<float> loudPane() const { auto r = middle(); return r.removeFromRight (318.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.68f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (ice().withAlpha (0.06f), 0, r.getY(), ice().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (ice().withAlpha (0.12f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        void timerCallback() override { repaint(); }

        void paint (juce::Graphics& g) override
        {
            if (sm == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("summitlim");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.8f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.25f), a.getCentreX(), a.getY(), night().withAlpha (0.82f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (ice().withAlpha (0.22f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            paintTopBar (g);
            paintLevels (g);
            paintLoudness (g);
            paintDeck (g);
            paintMeter (g);
        }

        // ---- levels
        void paintLevels (juce::Graphics& g)
        {
            const auto r = levels(); pane (g, r, "LEVEL  -  input, output, and what the limiter takes");
            const auto p = r.reduced (14.0f, 28.0f).withTrimmedBottom (-12.0f).withTrimmedRight (30.0f);
            auto yLev = [&] (float db) { return p.getY() + juce::jlimit (0.0f, 1.0f, -db / 36.0f) * p.getHeight(); };
            g.setFont (Theme::font (8.5f, true));
            for (int d = 0; d <= 36; d += 3)
            {
                const float y = yLev ((float) -d);
                g.setColour (text (d % 6 == 0 ? 0.06f : 0.03f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
                if (d % 6 == 0) { g.setColour (text (0.35f)); g.drawText (juce::String (-d), (int) p.getRight() + 4, (int) y - 6, 26, 12, juce::Justification::left); }
            }
            const int N = SqSummitLimiterPlugin::histLen, w = sm->histW.load(), cols = std::min (N - 1, (int) p.getWidth());
            juce::Path inP, outP, grP;
            for (int i = 0; i <= cols; ++i)
            {
                const int idx = (w - 1 - (cols - i)) & (N - 1);
                const float x = p.getX() + p.getWidth() * i / (float) cols;
                const float yi = yLev (sm->histIn[(size_t) idx]), yo = yLev (sm->histOut[(size_t) idx]), yg = p.getY() + juce::jlimit (0.0f, 1.0f, -sm->histGr[(size_t) idx] / 18.0f) * p.getHeight() * 0.45f;
                if (i == 0) { inP.startNewSubPath (x, p.getBottom()); inP.lineTo (x, yi); outP.startNewSubPath (x, p.getBottom()); outP.lineTo (x, yo); grP.startNewSubPath (x, p.getY()); grP.lineTo (x, yg); }
                else { inP.lineTo (x, yi); outP.lineTo (x, yo); grP.lineTo (x, yg); }
            }
            inP.lineTo (p.getRight(), p.getBottom()); inP.closeSubPath();
            juce::Path outFill (outP); outFill.lineTo (p.getRight(), p.getBottom()); outFill.closeSubPath();
            grP.lineTo (p.getRight(), p.getY()); grP.closeSubPath();
            g.setGradientFill (juce::ColourGradient (frost().withAlpha (0.16f), 0, p.getY(), frost().withAlpha (0.02f), 0, p.getBottom(), false)); g.fillPath (inP);
            g.setGradientFill (juce::ColourGradient (ice().withAlpha (0.28f), 0, p.getY(), teal().withAlpha (0.03f), 0, p.getBottom(), false)); g.fillPath (outFill);
            g.setColour (ice()); g.strokePath (outP, juce::PathStrokeType (1.2f));
            g.setGradientFill (juce::ColourGradient (coral().withAlpha (0.75f), 0, p.getY(), coral().withAlpha (0.10f), 0, p.getY() + p.getHeight() * 0.45f, false)); g.fillPath (grP);
            // the ceiling
            const float yc = yLev (sm->bank[SqSummitLimiterPlugin::ceiling]);
            g.setColour (frost().withAlpha (0.25f)); g.fillRect (p.getX(), yc - 2.0f, p.getWidth(), 4.0f);
            g.setColour (frost()); g.drawHorizontalLine ((int) yc, p.getX(), p.getRight());
            g.setFont (Theme::font (9.0f, true)); g.drawText ("CEILING " + juce::String (sm->bank[SqSummitLimiterPlugin::ceiling], 2) + " dBTP", (int) p.getX() + 6, (int) yc - 14, 180, 12, juce::Justification::left);
            // gain reduction now
            const float gr = sm->grNow.load();
            g.setColour (coral()); g.setFont (Theme::font (20.0f, true));
            g.drawText (juce::String (gr, 1) + " dB", p.withTrimmedTop (p.getHeight() * 0.48f).removeFromTop (24.0f).withTrimmedRight (12.0f), juce::Justification::right);
            g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true));
            g.drawText ("GAIN REDUCTION", p.withTrimmedTop (p.getHeight() * 0.48f + 24.0f).removeFromTop (12.0f).withTrimmedRight (12.0f), juce::Justification::right);
        }

        // ---- loudness
        juce::Rectangle<float> resetR, targetR;
        void paintLoudness (juce::Graphics& g)
        {
            auto r = loudPane(); pane (g, r, "LOUDNESS  -  BS.1770-4");
            auto in = r.reduced (16.0f, 30.0f);
            const auto& t = SqSummitLimiterPlugin::targets()[(size_t) juce::jlimit (0, (int) SqSummitLimiterPlugin::targets().size() - 1, sm->target.get())];
            targetR = in.removeFromTop (24.0f);
            g.setColour (ice().withAlpha (0.12f)); g.fillRoundedRectangle (targetR, 12.0f); g.setColour (ice().withAlpha (0.5f)); g.drawRoundedRectangle (targetR, 12.0f, 1.0f);
            g.setColour (text (0.95f)); g.setFont (Theme::font (10.0f, true)); g.drawText ("TARGET  " + juce::String (t.name).toUpperCase(), targetR, juce::Justification::centred);
            in.removeFromTop (14.0f);
            const float I = sm->integrated.load();
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("INTEGRATED", in.removeFromTop (12.0f), juce::Justification::centred);
            g.setColour (frost()); g.setFont (Theme::font (40.0f, true)); g.drawText (I > -70 ? juce::String (I, 1) : juce::String ("--.-"), in.removeFromTop (46.0f), juce::Justification::centred);
            g.setColour (text (0.5f)); g.setFont (Theme::font (10.0f, true)); g.drawText ("LUFS", in.removeFromTop (14.0f), juce::Justification::centred);
            // the difference to the target
            in.removeFromTop (8.0f);
            auto bar = in.removeFromTop (12.0f).reduced (8.0f, 0);
            g.setColour (text (0.07f)); g.fillRoundedRectangle (bar, 6.0f);
            const float d = I > -70 ? juce::jlimit (-6.0f, 6.0f, I - t.lufs) : 0.0f;
            const float cx = bar.getCentreX(), x = cx + d / 6.0f * bar.getWidth() * 0.5f;
            g.setColour (std::abs (d) < 1.0f ? teal() : coral()); g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, x), bar.getY(), std::max (cx, x), bar.getBottom()), 6.0f);
            g.setColour (frost()); g.fillRect (cx - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
            g.setColour (text (0.6f)); g.setFont (Theme::font (9.5f, true));
            g.drawText (I > -70 ? (d >= 0 ? "+" : "") + juce::String (I - t.lufs, 1) + " LU from the target" : juce::String ("play the song through"), in.removeFromTop (18.0f), juce::Justification::centred);
            in.removeFromTop (6.0f);
            // the small readings
            auto row = [&] (const juce::String& k, const juce::String& v, juce::Colour c)
            {
                auto rr = in.removeFromTop (22.0f);
                g.setColour (text (0.5f)); g.setFont (Theme::font (9.5f, true)); g.drawText (k, rr, juce::Justification::left);
                g.setColour (c); g.setFont (Theme::font (13.0f, true)); g.drawText (v, rr, juce::Justification::right);
                g.setColour (text (0.05f)); g.drawHorizontalLine ((int) rr.getBottom(), rr.getX(), rr.getRight());
            };
            const float M = sm->momentary.load(), S = sm->shortTerm.load(), L = sm->lra.load(), TP = sm->tpMax.load();
            row ("MOMENTARY", M > -70 ? juce::String (M, 1) + " LUFS" : juce::String ("--"), frost());
            row ("SHORT-TERM", S > -70 ? juce::String (S, 1) + " LUFS" : juce::String ("--"), frost());
            row ("LOUDNESS RANGE", juce::String (L, 1) + " LU", frost());
            row ("TRUE PEAK MAX", TP > -99 ? juce::String (TP, 2) + " dBTP" : juce::String ("--"), TP > t.tp + 0.01f ? coral() : teal());
            in.removeFromTop (8.0f);
            resetR = in.removeFromTop (24.0f).withSizeKeepingCentre (130.0f, 24.0f);
            g.setColour (text (0.06f)); g.fillRoundedRectangle (resetR, 12.0f); g.setColour (text (0.2f)); g.drawRoundedRectangle (resetR, 12.0f, 1.0f);
            g.setColour (text (0.85f)); g.setFont (Theme::font (10.0f, true)); g.drawText ("RESET METERS", resetR, juce::Justification::centred);
        }

        // ---- knobs: frosted glass
        struct KnobDef { int param; const char* label; bool big; bool bipolar; };
        static const std::vector<KnobDef>& knobDefs()
        {
            using P = SqSummitLimiterPlugin;
            static const std::vector<KnobDef> k { { P::input, "INPUT", true, false }, { P::ceiling, "CEILING", true, false }, { P::release, "RELEASE", true, false },
                                                  { P::lookahead, "LOOKAHEAD", false, false }, { P::transientLink, "TRANSIENT LINK", false, false }, { P::releaseLink, "RELEASE LINK", false, false } };
            return k;
        }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *sm->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float n) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n)), juce::sendNotification); changed(); }
        juce::UndoManager* um() const { return &sm->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        bool knobEnabled (int i) const { const int p = knobDefs()[(size_t) i].param; return ! ((p == SqSummitLimiterPlugin::transientLink || p == SqSummitLimiterPlugin::releaseLink) && sm->linkOwn.get()); }
        juce::Rectangle<float> linkChipR;

        void layoutKnobs()
        {
            knobR.clear();
            auto d = deck().reduced (20.0f, 12.0f);
            // three big knobs on the left two-thirds, three small on the right third (with the linking chip)
            auto left = d.removeFromLeft (d.getWidth() * 0.62f); d.removeFromLeft (14.0f);
            linkChipR = d.removeFromTop (26.0f).withSizeKeepingCentre (std::min (d.getWidth(), 260.0f), 24.0f);
            const float w1 = left.getWidth() / 3.0f, w2 = d.getWidth() / 3.0f;
            for (int i = 0; i < 3; ++i) knobR.push_back ({ left.getX() + w1 * i, left.getY(), w1, left.getHeight() });
            for (int i = 0; i < 3; ++i) knobR.push_back ({ d.getX() + w2 * i, d.getY() + 6.0f, w2, d.getHeight() - 6.0f });
        }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, bool big, bool hot, bool active, bool enabled)
        {
            const float alpha = enabled ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (16.0f); auto read = cell.removeFromBottom (26.0f).withTrimmedTop (4.0f);
            const float rO = std::min (cell.getWidth(), cell.getHeight()) * 0.5f - 4.0f;
            const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text ((hot ? 0.95f : 0.55f) * alpha)); g.setFont (Theme::font (big ? 10.0f : 9.0f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            // a soft ice halo
            g.setGradientFill (juce::ColourGradient (ice().withAlpha (0.10f * alpha), c.x, c.y, ice().withAlpha (0.0f), c.x, c.y + rO * 1.3f, true));
            g.fillEllipse (c.x - rO * 1.25f, c.y - rO * 1.25f, rO * 2.5f, rO * 2.5f);
            // fine tick ring (every 2.5 %), longer every 25 %
            for (int t = 0; t <= 40; ++t)
            {
                const float a = a0 + (a1 - a0) * t / 40.0f; const bool major = t % 10 == 0;
                const float r1 = rO * (major ? 0.88f : 0.93f), r2 = rO;
                g.setColour ((t / 40.0f <= v ? ice() : text (0.25f)).withMultipliedAlpha ((major ? 0.95f : 0.45f) * alpha));
                g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), major ? 1.3f : 0.8f);
            }
            // value arc: a frosted track and an ice arc with a white core
            const float ar = rO * 0.80f;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.55f * alpha)); g.strokePath (track, juce::PathStrokeType (big ? 6.0f : 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, a0, av, true);
            g.setColour (teal().withAlpha (0.25f * alpha)); g.strokePath (arc, juce::PathStrokeType (big ? 12.0f : 10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (teal().withAlpha (alpha), c.x - ar, c.y + ar, ice().withAlpha (alpha), c.x + ar, c.y - ar, false));
            g.strokePath (arc, juce::PathStrokeType (big ? 4.5f : 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::white.withAlpha (0.8f * alpha)); g.strokePath (arc, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // frosted-glass body: dark core, a cool rim light, a specular sweep
            const float rb = rO * 0.6f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27384a).withAlpha (alpha), c.x - rb * 0.5f, c.y - rb * 0.7f, juce::Colour (0xff070d16).withAlpha (alpha), c.x + rb * 0.7f, c.y + rb, true));
            g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f * alpha), c.x - rb * 0.3f, c.y - rb, juce::Colours::transparentWhite, c.x + rb * 0.2f, c.y - rb * 0.1f, false));
            juce::Path spec; spec.addCentredArc (c.x, c.y, rb * 0.82f, rb * 0.82f, 0, -2.2f, 0.6f, true); g.strokePath (spec, juce::PathStrokeType (rb * 0.22f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour ((hot ? ice() : ice().withAlpha (0.25f)).withMultipliedAlpha (alpha)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, hot ? 1.5f : 1.0f);
            // pointer: a slim notch
            const juce::Point<float> p1 (c.x + rb * 0.45f * std::sin (av), c.y - rb * 0.45f * std::cos (av)), p2 (c.x + rb * 0.92f * std::sin (av), c.y - rb * 0.92f * std::cos (av));
            g.setColour (ice().withAlpha (0.4f * alpha)); g.drawLine ({ p1, p2 }, 5.0f);
            g.setColour (frost().withAlpha (alpha)); g.drawLine ({ p1, p2 }, 1.8f);
            // readout
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 8.0f, big ? 110.0f : 92.0f), 20.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 10.0f);
            g.setColour (active ? ice() : text (0.1f)); g.drawRoundedRectangle (pill, 10.0f, 1.0f);
            g.setColour ((active ? ice() : text (0.95f)).withMultipliedAlpha (alpha)); g.setFont (Theme::font (big ? 12.0f : 10.5f, true));
            g.drawText (enabled ? value : juce::String ("ALGORITHM"), pill, juce::Justification::centred);
        }
        juce::String knobText (int i) const
        {
            using P = SqSummitLimiterPlugin; const float v = param (i).getCurrentValue();
            switch (i)
            {
                case P::input:   return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
                case P::ceiling: return juce::String (v, 2) + " dBTP";
                case P::release: return juce::String ((int) std::lround (v)) + " %";
                case P::lookahead: return juce::String (v, 2) + " ms";
                default: return juce::String ((int) std::lround (v)) + " %";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1622).withAlpha (0.94f), 0, d.getY(), night().withAlpha (0.96f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (ice().withAlpha (0.16f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            // a frosted divider between the main three and the linking three
            g.setColour (ice().withAlpha (0.08f)); g.drawVerticalLine ((int) (knobR[3].getX() - 7), d.getY() + 20, d.getBottom() - 20);
            const bool own = sm->linkOwn.get();
            g.setColour (own ? ice().withAlpha (0.12f) : teal().withAlpha (0.22f)); g.fillRoundedRectangle (linkChipR, 12.0f);
            g.setColour (own ? ice().withAlpha (0.5f) : teal()); g.drawRoundedRectangle (linkChipR, 12.0f, 1.0f);
            g.setColour (text (0.95f)); g.setFont (Theme::font (10.0f, true)); g.drawText (own ? "LINKING: SET BY THE ALGORITHM" : "LINKING: SET BY HAND", linkChipR, juce::Justification::centred);
            for (size_t i = 0; i < knobDefs().size(); ++i)
            {
                const auto& k = knobDefs()[i];
                drawKnob (g, knobR[i], k.label, knobText (k.param), norm (k.param), k.big, (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, knobEnabled ((int) i));
                StudioUi::knobScale (g, knobR[i], sm->bank.specs[(size_t) k.param], text (1.0f), 16.0f, 26.0f, knobEnabled ((int) i) ? 1.0f : 0.4f);
            }
        }

        // ---- top bar
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 10.0f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id)
        {
            g.setColour (on ? teal().withAlpha (0.22f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? ice().withAlpha (0.85f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (10.0f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void paintTopBar (juce::Graphics& g)
        {
            hits.clear();
            auto bar = topBar();
            const auto& al = SqSummit::algorithms()[(size_t) juce::jlimit (0, 9, sm->algorithm.get())];
            auto prev = bar.removeFromLeft (26.0f); bar.removeFromLeft (4.0f);
            const juce::String nm = juce::String (al.name).toUpperCase();
            auto name = bar.removeFromLeft (std::max (150.0f, widthOf (nm, 13.0f) + 34.0f)); bar.removeFromLeft (4.0f);
            auto next = bar.removeFromLeft (26.0f); bar.removeFromLeft (12.0f);
            for (auto [rr, glyph, id] : { std::tuple<juce::Rectangle<float>, const char*, int> { prev, "\xe2\x80\xb9", 1 }, { next, "\xe2\x80\xba", 2 } })
            {
                g.setColour (text (0.06f)); g.fillEllipse (rr); g.setColour (ice().withAlpha (0.25f)); g.drawEllipse (rr, 1.0f);
                g.setColour (text (0.9f)); g.setFont (Theme::font (16.0f, true)); g.drawText (juce::String (juce::CharPointer_UTF8 (glyph)), rr.translated (0, -1), juce::Justification::centred);
                hits.push_back ({ rr, id });
            }
            g.setGradientFill (juce::ColourGradient (teal().withAlpha (0.5f), name.getX(), name.getY(), ice().withAlpha (0.25f), name.getRight(), name.getBottom(), false));
            g.fillRoundedRectangle (name, name.getHeight() * 0.5f);
            g.setColour (ice()); g.drawRoundedRectangle (name, name.getHeight() * 0.5f, 1.0f);
            g.setColour (juce::Colours::white); g.setFont (Theme::font (13.0f, true)); g.drawText (nm, name, juce::Justification::centred);
            hits.push_back ({ name, 3 });
            const double lat = sm->getLatencySeconds() * 1000.0;
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("LATENCY " + juce::String (lat, 1) + " MS", bar.removeFromRight (104.0f), juce::Justification::right);
            bar.removeFromRight (8.0f);
            auto rightChip = [&] (const juce::String& t, bool on, int id) { chip (g, bar.removeFromRight (widthOf (t) + 22.0f), t, on, id); bar.removeFromRight (6.0f); };
            rightChip ("LISTEN REMOVED", sm->listenRemoved.get(), 12);
            rightChip ("UNITY", sm->unity.get(), 11);
            rightChip (sm->ditherBits.get() > 0 ? "DITHER " + SqSummitLimiterPlugin::ditherNames()[sm->ditherBits.get()].toUpperCase() + " / " + SqSummitLimiterPlugin::shapeNames()[juce::jlimit (0, 2, sm->noiseShape.get())].toUpperCase() : juce::String ("DITHER OFF"), sm->ditherBits.get() > 0, 10);
            rightChip ("OVERSAMPLING " + SqSummitLimiterPlugin::oversampleNames()[juce::jlimit (0, 5, sm->oversample.get())].toUpperCase(), sm->oversample.get() > 0, 9);
            rightChip ("DC", sm->dcFilter.get(), 8);
            rightChip ("GUARD", sm->truePeak.get() && sm->bandLimit.get(), 7);
            rightChip ("TRUE PEAK", sm->truePeak.get(), 6);
            g.setColour (text (0.6f)); g.setFont (Theme::font (10.0f)); g.drawFittedText (al.about, bar.toNearestInt(), juce::Justification::centredLeft, 2);
        }
        void menuFor (const juce::StringArray& names, int cur, std::function<void (int)> set)
        {
            juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<SummitViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Summit Limiter setting");
            auto& s = *sm;
            switch (id)
            {
                case 1: s.algorithm.setValue ((s.algorithm.get() + 9) % 10, um()); changed(); break;
                case 2: s.algorithm.setValue ((s.algorithm.get() + 1) % 10, um()); changed(); break;
                case 3:
                {
                    juce::PopupMenu m;
                    for (int i = 0; i < 10; ++i) m.addItem (i + 1, juce::String (SqSummit::algorithms()[(size_t) i].name) + "   -   " + SqSummit::algorithms()[(size_t) i].about, true, i == s.algorithm.get());
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<SummitViz> (this)] (int r) { if (r > 0 && sp != nullptr) { sm->algorithm.setValue (r - 1, um()); changed(); } });
                    break;
                }
                case 6: s.truePeak.setValue (! s.truePeak.get(), um()); changed(); break;
                case 7: s.bandLimit.setValue (! s.bandLimit.get(), um()); changed(); break;
                case 8: s.dcFilter.setValue (! s.dcFilter.get(), um()); changed(); break;
                case 9: menuFor (SqSummitLimiterPlugin::oversampleNames(), s.oversample.get(), [this] (int i) { sm->oversample.setValue (i, um()); }); break;
                case 10:
                {
                    juce::PopupMenu m; for (int i = 0; i < 4; ++i) m.addItem (1 + i, SqSummitLimiterPlugin::ditherNames()[i], true, s.ditherBits.get() == i);
                    m.addSeparator(); for (int i = 0; i < 3; ++i) m.addItem (10 + i, SqSummitLimiterPlugin::shapeNames()[i], true, s.noiseShape.get() == i);
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<SummitViz> (this)] (int r) { if (r <= 0 || sp == nullptr) return; if (r >= 10) sm->noiseShape.setValue (r - 10, um()); else sm->ditherBits.setValue (r - 1, um()); changed(); });
                    break;
                }
                case 11: s.unity.setValue (! s.unity.get(), um()); changed(); break;
                case 12: s.listenRemoved.setValue (! s.listenRemoved.get(), um()); changed(); break;
                default: break;
            }
        }

        // ---- mouse
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (6.0f, 0).contains (p) && knobEnabled ((int) i)) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob) { hoverKnob = k; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (sm == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (resetR.contains (e.position)) { sm->resetMeters(); repaint(); return; }
            if (targetR.contains (e.position))
            {
                juce::StringArray names; for (auto& t : SqSummitLimiterPlugin::targets()) names.add (t.name);
                um()->beginNewTransaction ("Loudness target"); menuFor (names, sm->target.get(), [this] (int i) { sm->target.setValue (i, um()); }); return;
            }
            if (linkChipR.contains (e.position)) { um()->beginNewTransaction ("Linking"); sm->linkOwn.setValue (! sm->linkOwn.get(), um()); changed(); return; }
            const int k = knobAt (e.position);
            if (k >= 0)
            {
                dragKnob = k; dragStart = e.position; dragStartNorm = norm (knobDefs()[(size_t) k].param);
                um()->beginNewTransaction (juce::String ("Summit ") + knobDefs()[(size_t) k].label);
                param (knobDefs()[(size_t) k].param).parameterChangeGestureBegin(); repaint();
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragKnob < 0) return;
            setNorm (knobDefs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragKnob >= 0) param (knobDefs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = knobDefs()[(size_t) k].param;
            um()->beginNewTransaction ("Summit reset"); param (pi_).setParameter (sm->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Summit wheel");
            const int pi_ = knobDefs()[(size_t) k].param;
            setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
