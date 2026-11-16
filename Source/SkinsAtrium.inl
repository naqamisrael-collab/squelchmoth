// Included inside namespace Skins (PluginSkins.h): the Atrium Verb face (1.14 Studio Line, phase D).
//
// Squelchmoth's own "atrium at blue hour" design on the Higgsfield plate pa_atriumverb (stone columns, a glass vault):
//  - top bar: the space (arrows, menu, what it is), freeze, predelay sync, the wet meter
//  - the reflection timeline: the dry hit, the predelay gap, every early reflection as a filament (left above the
//    axis, right below), and the late tail drawn as its true envelope from the decay time, with the live wet level
//    and the ducking traced over it
//  - the decay landscape: decay time against frequency, computed from the actual per-line filters; the low and high
//    corners are handles - drag sideways for the crossover, up and down for the multiplier
//  - the knob deck: knurled brass rings with smoked-glass caps, drawn here; big knobs for the four you touch most

    struct AtriumViz : public Viz, private juce::Timer
    {
        AtriumViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), av (dynamic_cast<SqAtriumVerbPlugin*> (&p))
        {
            setInterceptsMouseClicks (true, false);
            startTimerHz (30);
        }
        SqAtriumVerbPlugin* av;
        std::function<void()> onChange;
        using P = SqAtriumVerbPlugin;

        static juce::Colour night()  { return juce::Colour (0xff09070f); }
        static juce::Colour amber()  { return juce::Colour (0xffffb35c); }
        static juce::Colour violet() { return juce::Colour (0xffa58bff); }
        static juce::Colour rose()   { return juce::Colour (0xffff7aa8); }
        static juce::Colour cream()  { return juce::Colour (0xfff6ead8); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfff4ecff).withAlpha (a); }

        juce::Rectangle<float> all() const      { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const   { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const     { return all().reduced (16.0f, 0).removeFromBottom (318.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const   { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (326.0f); r.removeFromRight (meterW() + 10.0f); return r; }
        // the numbers: IN / OUT meters on the right (StudioUi, shared by the Studio Line)
        StudioUi::StudioMeter meter;
        static float meterW() { return StudioUi::StudioMeter::width (false); }
        juce::Rectangle<float> meterR() const { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (326.0f); return r.removeFromRight (meterW()); }
        void paintMeter (juce::Graphics& g) { meter.update (av->levelIn.load(), av->levelOut.load(), (float) 0.0f); meter.draw (g, meterR(), amber(), false); }
        juce::Rectangle<float> timeline() const { auto r = middle(); return r.removeFromLeft (r.getWidth() * 0.58f); }
        juce::Rectangle<float> landscape() const{ auto r = middle(); r.removeFromLeft (r.getWidth() * 0.58f + 12.0f); return r; }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.66f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (violet().withAlpha (0.07f), 0, r.getY(), violet().withAlpha (0.0f), 0, r.getY() + 44.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (violet().withAlpha (0.16f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        void timerCallback() override { repaint(); }

        void paint (juce::Graphics& g) override
        {
            if (av == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("atriumverb");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.85f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.20f), a.getCentreX(), a.getY(), night().withAlpha (0.84f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (violet().withAlpha (0.24f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            paintTopBar (g);
            paintTimeline (g);
            paintLandscape (g);
            paintDeck (g);
            paintMeter (g);
        }

        // ---- the reflection timeline
        void paintTimeline (juce::Graphics& g)
        {
            const auto r = timeline(); pane (g, r, "REFLECTIONS  -  the hit, the predelay, early reflections and the tail");
            const auto p = r.reduced (16.0f, 30.0f).withTrimmedBottom (-10.0f).withTrimmedLeft (22.0f);
            const double T = av->bank[P::decay], pd = av->predelayMs() / 1000.0;
            const auto taps = av->taps();
            double erEnd = 0; for (auto& t : taps) erEnd = std::max (erEnd, t.sec);
            // the time axis spans the predelay, the reflections and the tail down to -60 dB (square-root warped so the
            // first 100 ms get room even when the tail is 20 s long)
            const double span = std::max (0.25, pd + std::max (erEnd, 0.05) + (av->freeze.get() ? 4.0 : T) * 1.05);
            auto xAt = [&] (double sec) { return p.getX() + (float) std::sqrt (juce::jlimit (0.0, 1.0, sec / span)) * p.getWidth(); };
            const float yMid = p.getCentreY(), half = p.getHeight() * 0.5f - 4.0f;
            auto yUp = [&] (double dbv) { return yMid - (float) juce::jlimit (0.0, 1.0, (dbv + 60.0) / 60.0) * half; };
            auto yDn = [&] (double dbv) { return yMid + (float) juce::jlimit (0.0, 1.0, (dbv + 60.0) / 60.0) * half; };
            // grid: -12 dB steps, time marks
            g.setFont (Theme::font (8.5f, true));
            for (int d = 0; d <= 60; d += 12)
            {
                g.setColour (text (d == 60 ? 0.10f : 0.04f));
                g.drawHorizontalLine ((int) yUp (-d), p.getX(), p.getRight()); g.drawHorizontalLine ((int) yDn (-d), p.getX(), p.getRight());
                if (d < 60) { g.setColour (text (0.30f)); g.drawText (juce::String (-d), (int) p.getX() - 26, (int) yUp (-d) - 6, 22, 12, juce::Justification::right); }
            }
            for (double tm : { 0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 40.0 })
            {
                if (tm > span) break;
                const float x = xAt (tm);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.32f)); g.drawText (tm < 1 ? juce::String ((int) std::lround (tm * 1000)) + " ms" : juce::String (tm, 0) + " s", (int) x - 30, (int) p.getBottom() + 2, 60, 12, juce::Justification::centred);
            }
            g.setColour (text (0.35f)); g.setFont (Theme::font (8.5f, true));
            g.drawText ("L", (int) p.getX() - 18, (int) p.getY(), 14, 12, juce::Justification::centred);
            g.drawText ("R", (int) p.getX() - 18, (int) p.getBottom() - 12, 14, 12, juce::Justification::centred);
            // the predelay gap
            const float xp = xAt (pd);
            if (pd > 0.0005)
            {
                g.setColour (violet().withAlpha (0.07f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (p.getX(), p.getY(), xp, p.getBottom()));
                g.setColour (violet().withAlpha (0.55f)); g.setFont (Theme::font (9.0f, true));
                g.drawText ("PREDELAY " + juce::String (pd * 1000.0, pd < 0.1 ? 1 : 0) + " MS" + (av->predelaySync.get() > 0 ? "  (" + P::syncNames()[av->predelaySync.get()].toUpperCase() + ")" : juce::String()),
                            juce::Rectangle<float> (p.getX() + 6, p.getY() + 16, 240, 12), juce::Justification::left);
            }
            // the dry hit
            g.setColour (cream()); g.fillRect (p.getX() - 1.0f, yUp (0), 2.0f, yDn (0) - yUp (0));
            g.setColour (cream().withAlpha (0.18f)); g.fillRect (p.getX() - 4.0f, yUp (0), 8.0f, yDn (0) - yUp (0));
            // the late tail envelope: onset after the shortest path, then -60 dB over the decay
            const double lateLvl = av->bank[P::late] / 100.0, earlyLvl = av->bank[P::early] / 100.0;
            const double sizeN = av->bank[P::size] / 100.0;
            const double onset = pd + 0.004 + 0.012 * sizeN;
            const double peakDb = juce::Decibels::gainToDecibels (std::max (1e-4, 0.5 * lateLvl));
            juce::Path tailU, tailD;
            const int steps = 220; bool started = false;
            for (int i = 0; i <= steps; ++i)
            {
                const double sec = onset + (span - onset) * std::pow (i / (double) steps, 2.0);
                const double rise = std::min (1.0, (sec - onset) / (0.02 + 0.06 * sizeN));
                const double dbv = av->freeze.get() ? peakDb : peakDb + 20.0 * std::log10 (std::max (1e-4, rise)) - 60.0 * (sec - onset) / std::max (0.1, T);
                const float x = xAt (sec);
                if (! started) { tailU.startNewSubPath (x, yMid); tailD.startNewSubPath (x, yMid); started = true; }
                tailU.lineTo (x, yUp (dbv)); tailD.lineTo (x, yDn (dbv));
            }
            tailU.lineTo (p.getRight(), yMid); tailD.lineTo (p.getRight(), yMid);
            juce::Path fillU (tailU), fillD (tailD); fillU.closeSubPath(); fillD.closeSubPath();
            g.setGradientFill (juce::ColourGradient (violet().withAlpha (0.42f), xp, yMid - half, violet().withAlpha (0.04f), p.getRight(), yMid, false)); g.fillPath (fillU);
            g.setGradientFill (juce::ColourGradient (violet().withAlpha (0.42f), xp, yMid + half, violet().withAlpha (0.04f), p.getRight(), yMid, false)); g.fillPath (fillD);
            g.setColour (violet().withAlpha (0.95f)); g.strokePath (tailU, juce::PathStrokeType (1.3f)); g.strokePath (tailD, juce::PathStrokeType (1.3f));
            // early reflections: filaments with a glow, amber
            for (auto& t : taps)
            {
                const float x = xAt (pd + t.sec);
                const double dl = juce::Decibels::gainToDecibels (std::max (1e-5, (double) std::abs (t.gL))), dr = juce::Decibels::gainToDecibels (std::max (1e-5, (double) std::abs (t.gR)));
                g.setColour (amber().withAlpha (0.10f)); g.fillRect (x - 1.5f, yUp (dl), 3.0f, yMid - yUp (dl)); g.fillRect (x - 1.5f, yMid, 3.0f, yDn (dr) - yMid);
                g.setColour (amber().withAlpha (0.9f)); g.drawLine (x, yMid, x, yUp (dl), 1.0f); g.drawLine (x, yMid, x, yDn (dr), 1.0f);
                g.setColour (cream()); g.fillEllipse (x - 2.0f, yUp (dl) - 2.0f, 4.0f, 4.0f); g.fillEllipse (x - 2.0f, yDn (dr) - 2.0f, 4.0f, 4.0f);
            }
            if (taps.empty() || earlyLvl <= 0.001)
            {
                g.setColour (text (0.35f)); g.setFont (Theme::font (9.0f, true));
                g.drawText (taps.empty() ? "NO EARLY REFLECTIONS IN THIS SPACE" : "EARLY REFLECTIONS OFF", juce::Rectangle<float> (xp + 8, yMid - 26, 260, 12), juce::Justification::left);
            }
            g.setColour (text (0.12f)); g.drawHorizontalLine ((int) yMid, p.getX(), p.getRight());
            // live: wet level and ducking, the last ~8 seconds, as a slim strip along the top
            {
                auto strip = juce::Rectangle<float> (p.getRight() - 210.0f, p.getY() - 2.0f, 210.0f, 34.0f);
                g.setColour (night().withAlpha (0.75f)); g.fillRoundedRectangle (strip, 6.0f); g.setColour (violet().withAlpha (0.18f)); g.drawRoundedRectangle (strip, 6.0f, 1.0f);
                auto sp = strip.reduced (6.0f, 5.0f).withTrimmedTop (9.0f);
                g.setColour (text (0.42f)); g.setFont (Theme::font (7.5f, true)); g.drawText ("LIVE WET / DUCK", strip.reduced (6.0f, 3.0f).removeFromTop (9.0f), juce::Justification::left);
                const int Nn = P::histLen, w = av->histW.load(), cols = 160;
                juce::Path wet, duck;
                for (int i = 0; i <= cols; ++i)
                {
                    const int idx = (w - 1 - (cols - i)) & (Nn - 1);
                    const float x = sp.getX() + sp.getWidth() * i / (float) cols;
                    const float yw = sp.getBottom() - juce::jlimit (0.0f, 1.0f, (av->histWet[(size_t) idx] + 60.0f) / 60.0f) * sp.getHeight();
                    const float yd = sp.getY() + juce::jlimit (0.0f, 1.0f, -av->histDuck[(size_t) idx] / 24.0f) * sp.getHeight();
                    if (i == 0) { wet.startNewSubPath (x, yw); duck.startNewSubPath (x, yd); } else { wet.lineTo (x, yw); duck.lineTo (x, yd); }
                }
                g.setColour (violet()); g.strokePath (wet, juce::PathStrokeType (1.1f));
                if (av->bank[P::duck] > 0.01f) { g.setColour (rose()); g.strokePath (duck, juce::PathStrokeType (1.1f)); }
            }
            // the decay readout
            g.setColour (cream()); g.setFont (Theme::font (22.0f, true));
            const auto readArea = juce::Rectangle<float> (p.getRight() - 210.0f, p.getBottom() - 46.0f, 206.0f, 26.0f);
            g.drawText (av->freeze.get() ? juce::String ("FROZEN") : juce::String (T, T < 10 ? 2 : 1) + " s", readArea, juce::Justification::right);
            g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true));
            g.drawText (av->freeze.get() ? "THE TANK HOLDS FOREVER" : "DECAY TO -60 DB AT 1 KHZ", readArea.translated (0, 24).withHeight (12.0f), juce::Justification::right);
        }

        // ---- the decay landscape (decay time vs frequency), with draggable corners
        juce::Rectangle<float> landPlot() const { return landscape().reduced (16.0f, 30.0f).withTrimmedLeft (26.0f).withTrimmedBottom (-6.0f); }
        static float xOfHz (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * p.getWidth(); }
        static double hzOfX (juce::Rectangle<float> p, float x) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
        float yOfSec (juce::Rectangle<float> p, double sec) const
        {
            const double T = av->bank[P::decay], lo = T / 6.0, hi = T * 6.0;   // two decades centred on the Decay
            return p.getBottom() - (float) juce::jlimit (0.0, 1.0, std::log (std::max (1e-3, sec) / lo) / std::log (hi / lo)) * p.getHeight();
        }
        juce::Point<float> lowHandle() const  { const auto p = landPlot(); return { xOfHz (p, av->bank[P::lowX]),  yOfSec (p, av->bank[P::decay] * av->bank[P::lowMult]) }; }
        juce::Point<float> highHandle() const { const auto p = landPlot(); return { xOfHz (p, av->bank[P::highX]), yOfSec (p, av->bank[P::decay] * av->bank[P::highMult]) }; }

        void paintLandscape (juce::Graphics& g)
        {
            const auto r = landscape(); pane (g, r, "DECAY BY FREQUENCY  -  drag a corner: sideways = crossover, up / down = multiplier");
            const auto p = landPlot();
            const double T = av->bank[P::decay];
            g.setFont (Theme::font (8.5f, true));
            for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
            {
                const float x = xOfHz (p, hz);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.32f)); g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), (int) x - 16, (int) p.getBottom() + 2, 32, 12, juce::Justification::centred);
            }
            for (double m : { 1.0 / 4, 1.0 / 2, 1.0, 2.0, 4.0 })
            {
                const float y = yOfSec (p, T * m);
                g.setColour (text (m == 1.0 ? 0.12f : 0.05f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
                const double sv = T * m;
                g.setColour (text (0.32f)); g.drawText (sv < 100 ? juce::String (sv, sv < 1 ? 2 : 1) : juce::String ((int) sv), (int) p.getX() - 30, (int) y - 6, 26, 12, juce::Justification::right);
            }
            // the curve, from the real per-line filters
            juce::Path c; juce::Path fillP;
            const int n = 160;
            for (int i = 0; i <= n; ++i)
            {
                const float x = p.getX() + p.getWidth() * i / (float) n;
                const double hz = hzOfX (p, x), dsec = av->freeze.get() ? T * 6.0 : av->decayAt (hz);
                const float y = yOfSec (p, dsec);
                if (i == 0) { c.startNewSubPath (x, y); fillP.startNewSubPath (x, p.getBottom()); fillP.lineTo (x, y); } else { c.lineTo (x, y); fillP.lineTo (x, y); }
            }
            fillP.lineTo (p.getRight(), p.getBottom()); fillP.closeSubPath();
            g.setGradientFill (juce::ColourGradient (amber().withAlpha (0.30f), 0, p.getY(), violet().withAlpha (0.05f), 0, p.getBottom(), false)); g.fillPath (fillP);
            g.setColour (amber().withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (amber()); g.strokePath (c, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (cream().withAlpha (0.8f)); g.strokePath (c, juce::PathStrokeType (0.7f));
            // the cut filters shade the ends (they shape the sound, not the decay)
            if (av->bank[P::lowCut] > 10.5f) { g.setColour (rose().withAlpha (0.08f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (p.getX(), p.getY(), xOfHz (p, av->bank[P::lowCut]), p.getBottom())); }
            if (av->bank[P::highCut] < 19999.0f) { g.setColour (rose().withAlpha (0.08f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xOfHz (p, av->bank[P::highCut]), p.getY(), p.getRight(), p.getBottom())); }
            // the handles
            auto handle = [&] (juce::Point<float> h, const juce::String& lab, const juce::String& val, bool hot)
            {
                g.setColour (amber().withAlpha (hot ? 0.35f : 0.18f)); g.fillEllipse (h.x - 13, h.y - 13, 26, 26);
                g.setColour (night()); g.fillEllipse (h.x - 7, h.y - 7, 14, 14);
                g.setColour (hot ? cream() : amber()); g.drawEllipse (h.x - 7, h.y - 7, 14, 14, 2.0f);
                g.setColour (cream()); g.fillEllipse (h.x - 2.5f, h.y - 2.5f, 5, 5);
                auto tag = juce::Rectangle<float> (h.x - 52, h.y - 34, 104, 18).constrainedWithin (p.expanded (24.0f, 24.0f));
                g.setColour (night().withAlpha (0.85f)); g.fillRoundedRectangle (tag, 9.0f); g.setColour (amber().withAlpha (0.5f)); g.drawRoundedRectangle (tag, 9.0f, 1.0f);
                g.setColour (text (0.95f)); g.setFont (Theme::font (9.0f, true)); g.drawText (lab + "  " + val, tag, juce::Justification::centred);
            };
            // the asymptotes the corners set: dotted guides out to the edges
            {
                const float dash[] = { 3.0f, 4.0f };
                const auto lh = lowHandle(), hh = highHandle();
                g.setColour (amber().withAlpha (0.55f));
                g.drawDashedLine (juce::Line<float> (p.getX(), lh.y, lh.x - 9.0f, lh.y), dash, 2, 1.0f);
                g.drawDashedLine (juce::Line<float> (hh.x + 9.0f, hh.y, p.getRight(), hh.y), dash, 2, 1.0f);
                g.setColour (violet().withAlpha (0.35f));
                g.drawDashedLine (juce::Line<float> (lh.x, p.getY(), lh.x, p.getBottom()), dash, 2, 0.8f);
                g.drawDashedLine (juce::Line<float> (hh.x, p.getY(), hh.x, p.getBottom()), dash, 2, 0.8f);
            }
            handle (lowHandle(), "LOW", "x" + juce::String (av->bank[P::lowMult], 2) + "  " + juce::String ((int) av->bank[P::lowX]) + " Hz", dragHandle == 1 || hoverHandle == 1);
            handle (highHandle(), "HIGH", "x" + juce::String (av->bank[P::highMult], 2) + "  " + (av->bank[P::highX] >= 1000 ? juce::String (av->bank[P::highX] / 1000.0f, 1) + "k" : juce::String ((int) av->bank[P::highX])), dragHandle == 2 || hoverHandle == 2);
        }

        // ---- knobs: knurled brass rings, smoked-glass caps
        struct KnobDef { int param; const char* label; bool big; };
        static const std::vector<KnobDef>& knobDefs()
        {
            static const std::vector<KnobDef> k {
                { P::decay, "DECAY", true }, { P::size, "SIZE", true }, { P::predelay, "PREDELAY", true }, { P::mix, "MIX", true },
                { P::lowMult, "LOW DECAY", false }, { P::highMult, "HIGH DECAY", false }, { P::lowX, "LOW X", false }, { P::highX, "HIGH X", false },
                { P::diffusion, "DIFFUSION", false }, { P::character, "CHARACTER", false }, { P::width, "WIDTH", false }, { P::early, "EARLY", false }, { P::late, "LATE", false },
                { P::lowCut, "LOW CUT", false }, { P::highCut, "HIGH CUT", false }, { P::duck, "DUCKING", false }, { P::duckThresh, "DUCK THRESH", false },
                { P::duckRelease, "DUCK RELEASE", false }, { P::output, "OUTPUT", false } };
            return k;
        }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *av->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float n) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n)), juce::sendNotification); changed(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); changed(); }
        juce::UndoManager* um() const { return &av->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        bool knobEnabled (int i) const { return ! (knobDefs()[(size_t) i].param == P::predelay && av->predelaySync.get() > 0); }

        void layoutKnobs()
        {
            knobR.clear();
            auto d = deck().reduced (18.0f, 12.0f);
            auto top = d.removeFromTop (d.getHeight() * 0.56f); d.removeFromTop (6.0f);
            auto bigs = top.removeFromLeft (top.getWidth() * 0.6f); top.removeFromLeft (12.0f);
            for (int i = 0; i < 4; ++i) knobR.push_back ({ bigs.getX() + bigs.getWidth() / 4 * i, bigs.getY(), bigs.getWidth() / 4, bigs.getHeight() });
            top.removeFromTop (16.0f);   // the "shape of the decay" group label
            for (int i = 0; i < 4; ++i) knobR.push_back ({ top.getX() + top.getWidth() / 4 * i, top.getY() + 4, top.getWidth() / 4, top.getHeight() - 4 });
            const int rest = (int) knobDefs().size() - 8;
            for (int i = 0; i < rest; ++i) knobR.push_back ({ d.getX() + d.getWidth() / rest * i, d.getY(), d.getWidth() / rest, d.getHeight() });
        }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, bool big, bool hot, bool active, bool enabled, bool bipolar)
        {
            const float alpha = enabled ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (big ? 26.0f : 22.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 10.0f, cell.getHeight()) * 0.5f - 2.0f;
            const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av_ = a0 + (a1 - a0) * v;
            g.setColour (text ((hot ? 0.95f : 0.55f) * alpha)); g.setFont (Theme::font (big ? 10.0f : 8.5f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            // violet bloom under the knob
            g.setGradientFill (juce::ColourGradient (violet().withAlpha ((hot ? 0.20f : 0.11f) * alpha), c.x, c.y, violet().withAlpha (0.0f), c.x, c.y + rO * 1.35f, true));
            g.fillEllipse (c.x - rO * 1.3f, c.y - rO * 1.3f, rO * 2.6f, rO * 2.6f);
            // scale: dots, larger at quarters
            for (int t = 0; t <= 20; ++t)
            {
                const float a = a0 + (a1 - a0) * t / 20.0f; const bool major = t % 5 == 0; const float rr = rO * 0.99f;
                const float dot = major ? 2.2f : 1.2f;
                const bool lit = bipolar ? ((t / 20.0f >= std::min (0.5f, v) && t / 20.0f <= std::max (0.5f, v))) : (t / 20.0f <= v);
                g.setColour ((lit ? amber() : text (0.28f)).withMultipliedAlpha (alpha));
                g.fillEllipse (c.x + rr * std::sin (a) - dot, c.y - rr * std::cos (a) - dot, dot * 2, dot * 2);
            }
            // value arc in a recessed groove
            const float ar = rO * 0.84f;
            juce::Path groove; groove.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f * alpha)); g.strokePath (groove, juce::PathStrokeType (big ? 6.0f : 4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float as = bipolar ? (a0 + a1) * 0.5f : a0;
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (as, av_), std::max (as, av_), true);
            g.setColour (amber().withAlpha (0.22f * alpha)); g.strokePath (arc, juce::PathStrokeType (big ? 12.0f : 9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (rose().withAlpha (alpha), c.x - ar, c.y + ar, amber().withAlpha (alpha), c.x + ar, c.y - ar, false));
            g.strokePath (arc, juce::PathStrokeType (big ? 4.0f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (cream().withAlpha (0.75f * alpha)); g.strokePath (arc, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // knurled brass ring
            const float rk = rO * 0.68f, rkIn = rk * 0.86f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe7c48a).withAlpha (alpha), c.x - rk, c.y - rk, juce::Colour (0xff5a3d1c).withAlpha (alpha), c.x + rk, c.y + rk, false));
            g.fillEllipse (c.x - rk, c.y - rk, rk * 2, rk * 2);
            const int teeth = big ? 64 : 44;
            for (int t = 0; t < teeth; ++t)
            {
                const float a = juce::MathConstants<float>::twoPi * t / teeth + av_;   // the knurl turns with the knob
                const float shade = 0.5f + 0.5f * std::sin (a - 0.8f);
                g.setColour (juce::Colour (0xff2a1a08).withAlpha ((0.25f + 0.35f * (1.0f - shade)) * alpha));
                g.drawLine (c.x + rkIn * std::sin (a), c.y - rkIn * std::cos (a), c.x + rk * std::sin (a), c.y - rk * std::cos (a), 1.0f);
            }
            g.setColour (juce::Colours::black.withAlpha (0.55f * alpha)); g.drawEllipse (c.x - rk, c.y - rk, rk * 2, rk * 2, 1.0f);
            // smoked-glass cap
            const float rc = rkIn - 1.5f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a2c52).withAlpha (alpha), c.x - rc * 0.4f, c.y - rc * 0.8f, juce::Colour (0xff0c0814).withAlpha (alpha), c.x + rc * 0.6f, c.y + rc, true));
            g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f * alpha), c.x, c.y - rc, juce::Colours::transparentWhite, c.x, c.y, false));
            g.fillEllipse (c.x - rc * 0.78f, c.y - rc * 0.94f, rc * 1.56f, rc * 0.95f);
            g.setColour ((hot ? amber() : violet().withAlpha (0.35f)).withMultipliedAlpha (alpha)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, hot ? 1.4f : 0.9f);
            // pointer: a glowing amber line with a cream tip
            const juce::Point<float> p1 (c.x + rc * 0.25f * std::sin (av_), c.y - rc * 0.25f * std::cos (av_)), p2 (c.x + rc * 0.88f * std::sin (av_), c.y - rc * 0.88f * std::cos (av_));
            g.setColour (amber().withAlpha (0.35f * alpha)); g.drawLine ({ p1, p2 }, 5.0f);
            g.setColour (amber().withAlpha (alpha)); g.drawLine ({ p1, p2 }, 2.0f);
            g.setColour (cream().withAlpha (alpha)); g.fillEllipse (p2.x - 1.8f, p2.y - 1.8f, 3.6f, 3.6f);
            // readout pill
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 6.0f, big ? 108.0f : 84.0f), big ? 20.0f : 18.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
            g.setColour (active ? amber() : text (0.1f)); g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);
            g.setColour ((active ? amber() : text (0.95f)).withMultipliedAlpha (alpha)); g.setFont (Theme::font (big ? 12.0f : 10.0f, true));
            g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (int i) const
        {
            const float v = param (i).getCurrentValue();
            auto hz = [] (float f) { return f >= 1000 ? juce::String (f / 1000.0f, f < 10000 ? 2 : 1) + " kHz" : juce::String ((int) std::lround (f)) + " Hz"; };
            switch (i)
            {
                case P::decay:    return av->freeze.get() ? juce::String ("FROZEN") : juce::String (v, v < 10 ? 2 : 1) + " s";
                case P::predelay: return av->predelaySync.get() > 0 ? P::syncNames()[av->predelaySync.get()] : juce::String (v, v < 100 ? 1 : 0) + " ms";
                case P::lowMult: case P::highMult: return "x " + juce::String (v, 2);
                case P::lowX: case P::highX: return hz (v);
                case P::lowCut:   return v <= 10.5f ? juce::String ("OFF") : hz (v);
                case P::highCut:  return v >= 19999.0f ? juce::String ("OFF") : hz (v);
                case P::duck:     return v < 0.05f ? juce::String ("OFF") : juce::String (v, 1) + " dB";
                case P::duckThresh: return juce::String (v, 1) + " dB";
                case P::duckRelease: return juce::String ((int) std::lround (v)) + " ms";
                case P::output:   return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
                default:          return juce::String ((int) std::lround (v)) + " %";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15101f).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (violet().withAlpha (0.18f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            // group frames
            auto frame = [&] (int from, int to, const juce::String& t)
            {
                auto r = knobR[(size_t) from].getUnion (knobR[(size_t) to]).expanded (2.0f, 4.0f).withTrimmedTop (t.isNotEmpty() ? -14.0f : 0.0f);
                g.setColour (violet().withAlpha (0.05f)); g.fillRoundedRectangle (r, 10.0f);
                g.setColour (violet().withAlpha (0.12f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
                if (t.isNotEmpty()) { g.setColour (text (0.4f)); g.setFont (Theme::font (8.5f, true)); g.drawText (t, r.reduced (10.0f, 4.0f).removeFromTop (11.0f), juce::Justification::left); }
            };
            frame (4, 7, "THE SHAPE OF THE DECAY");
            frame (8, 12, ""); frame (13, 14, ""); frame (15, 17, ""); frame (18, 18, "");
            for (size_t i = 0; i < knobDefs().size(); ++i)
            {
                const auto& k = knobDefs()[i];
                drawKnob (g, knobR[i], k.label, knobText (k.param), norm (k.param), k.big, (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, knobEnabled ((int) i), k.param == P::output);
                StudioUi::knobScale (g, knobR[i], av->bank.specs[(size_t) k.param], text (1.0f), 15.0f, k.big ? 26.0f : 22.0f, knobEnabled ((int) i) ? 1.0f : 0.4f);
            }
        }

        // ---- top bar
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 10.0f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour onC)
        {
            g.setColour (on ? onC.withAlpha (0.24f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? onC.withAlpha (0.9f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (10.0f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void paintTopBar (juce::Graphics& g)
        {
            hits.clear();
            auto bar = topBar();
            const int nSp = (int) SqAtrium::spaces().size();
            const auto& sp = SqAtrium::spaces()[(size_t) juce::jlimit (0, nSp - 1, av->space.get())];
            auto prev = bar.removeFromLeft (26.0f); bar.removeFromLeft (4.0f);
            const juce::String nm = juce::String (sp.name).toUpperCase();
            auto name = bar.removeFromLeft (std::max (170.0f, widthOf (nm, 13.0f) + 34.0f)); bar.removeFromLeft (4.0f);
            auto next = bar.removeFromLeft (26.0f); bar.removeFromLeft (12.0f);
            for (auto [rr, glyph, id] : { std::tuple<juce::Rectangle<float>, const char*, int> { prev, "\xe2\x80\xb9", 1 }, { next, "\xe2\x80\xba", 2 } })
            {
                g.setColour (text (0.06f)); g.fillEllipse (rr); g.setColour (amber().withAlpha (0.3f)); g.drawEllipse (rr, 1.0f);
                g.setColour (text (0.9f)); g.setFont (Theme::font (16.0f, true)); g.drawText (juce::String (juce::CharPointer_UTF8 (glyph)), rr.translated (0, -1), juce::Justification::centred);
                hits.push_back ({ rr, id });
            }
            g.setGradientFill (juce::ColourGradient (violet().withAlpha (0.55f), name.getX(), name.getY(), amber().withAlpha (0.35f), name.getRight(), name.getBottom(), false));
            g.fillRoundedRectangle (name, name.getHeight() * 0.5f);
            g.setColour (amber()); g.drawRoundedRectangle (name, name.getHeight() * 0.5f, 1.0f);
            g.setColour (juce::Colours::white); g.setFont (Theme::font (13.0f, true)); g.drawText (nm, name, juce::Justification::centred);
            hits.push_back ({ name, 3 });
            // wet meter (right)
            {
                auto m = bar.removeFromRight (150.0f).withSizeKeepingCentre (150.0f, 12.0f);
                g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("WET", m.removeFromLeft (26.0f), juce::Justification::left);
                g.setColour (text (0.07f)); g.fillRoundedRectangle (m, 6.0f);
                const float wv = av->histWet[(size_t) ((av->histW.load() - 1) & (P::histLen - 1))];
                const float f = juce::jlimit (0.0f, 1.0f, (wv + 60.0f) / 60.0f);
                g.setGradientFill (juce::ColourGradient (violet(), m.getX(), 0, amber(), m.getRight(), 0, false)); g.fillRoundedRectangle (m.withWidth (m.getWidth() * f), 6.0f);
            }
            bar.removeFromRight (10.0f);
            auto rightChip = [&] (const juce::String& t, bool on, int id, juce::Colour c) { chip (g, bar.removeFromRight (widthOf (t) + 22.0f), t, on, id, c); bar.removeFromRight (6.0f); };
            const int sy = av->predelaySync.get();
            rightChip (sy > 0 ? "PREDELAY " + P::syncNames()[sy].toUpperCase() : juce::String ("PREDELAY FREE"), sy > 0, 5, amber());
            rightChip (av->freeze.get() ? "FROZEN" : "FREEZE", av->freeze.get(), 4, juce::Colour (0xff8fe3ff));
            g.setColour (text (0.6f)); g.setFont (Theme::font (10.0f)); g.drawFittedText (sp.about, bar.toNearestInt(), juce::Justification::centredLeft, 2);
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Atrium Verb setting");
            auto& s = *av; const int nSp = (int) SqAtrium::spaces().size();
            switch (id)
            {
                case 1: s.space.setValue ((s.space.get() + nSp - 1) % nSp, um()); changed(); break;
                case 2: s.space.setValue ((s.space.get() + 1) % nSp, um()); changed(); break;
                case 3:
                {
                    juce::PopupMenu m;
                    for (int i = 0; i < nSp; ++i) m.addItem (i + 1, juce::String (SqAtrium::spaces()[(size_t) i].name) + "   -   " + SqAtrium::spaces()[(size_t) i].about, true, i == s.space.get());
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<AtriumViz> (this)] (int r) { if (r > 0 && sp != nullptr) { av->space.setValue (r - 1, um()); changed(); } });
                    break;
                }
                case 4: s.freeze.setValue (! s.freeze.get(), um()); changed(); break;
                case 5:
                {
                    juce::PopupMenu m; const auto names = P::syncNames();
                    for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, i == 0 ? juce::String ("Free (the Predelay knob)") : "Sync to " + names[i], true, i == s.predelaySync.get());
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<AtriumViz> (this)] (int r) { if (r > 0 && sp != nullptr) { av->predelaySync.setValue (r - 1, um()); changed(); } });
                    break;
                }
                default: break;
            }
        }

        // ---- mouse
        int dragHandle = 0, hoverHandle = 0; float handleStartX = 0, handleStartMult = 0, handleStartHz = 0;
        int handleAt (juce::Point<float> p) const
        {
            if (av->freeze.get()) return 0;
            if (p.getDistanceFrom (lowHandle()) < 14.0f) return 1;
            if (p.getDistanceFrom (highHandle()) < 14.0f) return 2;
            return 0;
        }
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (4.0f, 0).contains (p) && knobEnabled ((int) i)) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), h = handleAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : h > 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || h != hoverHandle) { hoverKnob = k; hoverHandle = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverHandle = 0; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (av == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (const int h = handleAt (e.position))
            {
                dragHandle = h; um()->beginNewTransaction (h == 1 ? "Atrium low decay" : "Atrium high decay");
                param (h == 1 ? P::lowMult : P::highMult).parameterChangeGestureBegin(); param (h == 1 ? P::lowX : P::highX).parameterChangeGestureBegin();
                return;
            }
            const int k = knobAt (e.position);
            if (k >= 0)
            {
                dragKnob = k; dragStart = e.position; dragStartNorm = norm (knobDefs()[(size_t) k].param);
                um()->beginNewTransaction (juce::String ("Atrium ") + knobDefs()[(size_t) k].label);
                param (knobDefs()[(size_t) k].param).parameterChangeGestureBegin(); repaint();
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragHandle > 0)
            {
                const auto p = landPlot(); const double T = av->bank[P::decay], lo = T / 6.0, hi = T * 6.0;
                const double sec = lo * std::pow (hi / lo, juce::jlimit (0.0, 1.0, (double) ((p.getBottom() - e.position.y) / p.getHeight())));
                setValue (dragHandle == 1 ? P::lowMult : P::highMult, (float) (sec / T));
                setValue (dragHandle == 1 ? P::lowX : P::highX, (float) hzOfX (p, e.position.x));
                return;
            }
            if (dragKnob < 0) return;
            setNorm (knobDefs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragHandle > 0) { param (dragHandle == 1 ? P::lowMult : P::highMult).parameterChangeGestureEnd(); param (dragHandle == 1 ? P::lowX : P::highX).parameterChangeGestureEnd(); }
            if (dragKnob >= 0) param (knobDefs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; dragHandle = 0; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (const int h = handleAt (e.position))
            {
                um()->beginNewTransaction ("Atrium decay shape reset");
                const int pm = h == 1 ? P::lowMult : P::highMult, px = h == 1 ? P::lowX : P::highX;
                param (pm).setParameter (av->bank.specs[(size_t) pm].def, juce::sendNotification); param (px).setParameter (av->bank.specs[(size_t) px].def, juce::sendNotification); changed(); return;
            }
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = knobDefs()[(size_t) k].param;
            um()->beginNewTransaction ("Atrium reset"); param (pi_).setParameter (av->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Atrium wheel");
            const int pi_ = knobDefs()[(size_t) k].param;
            setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
