// Included inside namespace Skins (PluginSkins.h): the Mote face (1.14 Studio Line, phase K).
//
// Squelchmoth's own "a single mote of copper light" design on the Higgsfield plate pa_mote:
//  - top bar: voice mode (poly / mono / legato), glide mode, unison count, quality (Eco / HD), the LFO's shape and sync,
//    and how many voices are sounding
//  - OSCILLATOR: one cycle of the wave exactly as the oscillator makes it (shape, width, sub and noise added), a shape
//    rail you drag (SINE - TRI - SAW - SQUARE), the sub's octave and wave, and the output scope
//  - FILTER: the filter's real response at the cutoff, the range the envelope / key / LFO sweep lit, the newest voice's
//    cutoff as a moving line, and a node you drag (cutoff sideways, resonance up / down); type and slope chips
//  - ENVELOPES: the amp and filter envelopes drawn to their times (labelled in ms and %) with the newest voice's level
//    riding them, and the eight-slot modulation matrix
//  - L / R output meters with numbers; every knob shows its range

    struct MoteViz : public Viz, private juce::Timer
    {
        using P = SqMotePlugin;
        MoteViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mp (dynamic_cast<SqMotePlugin*> (&p))
        { setInterceptsMouseClicks (true, false); meter.labIn = "L"; meter.labOut = "R"; startTimerHz (30); }
        SqMotePlugin* mp;
        std::function<void()> onChange;

        static juce::Colour night()  { return juce::Colour (0xff0a0710); }
        static juce::Colour copper() { return juce::Colour (0xffffa35a); }
        static juce::Colour plum()   { return juce::Colour (0xffb98cff); }
        static juce::Colour ember()  { return juce::Colour (0xffff6b4a); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfff7efe8).withAlpha (a); }

        // ---- geometry
        static constexpr float deckH = 292.0f;
        juce::Rectangle<float> all() const    { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const   { return all().reduced (16.0f, 0).removeFromBottom (deckH).withTrimmedBottom (14.0f); }
        static float meterW() { return StudioUi::StudioMeter::width (false); }
        juce::Rectangle<float> middle() const { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (deckH + 8.0f); r.removeFromRight (meterW() + 10.0f); return r; }
        juce::Rectangle<float> meterR() const { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (deckH + 8.0f); return r.removeFromRight (meterW()); }
        juce::Rectangle<float> oscPane() const { auto r = middle(); return r.removeFromLeft (r.getWidth() * 0.33f).withTrimmedRight (6.0f); }
        juce::Rectangle<float> fltPane() const { auto r = middle(); r.removeFromLeft (r.getWidth() * 0.33f); return r.removeFromLeft (r.getWidth() * 0.52f).withTrimmedLeft (6.0f).withTrimmedRight (6.0f); }
        juce::Rectangle<float> envPane() const { auto r = middle(); r.removeFromLeft (r.getWidth() * 0.33f); r.removeFromLeft (r.getWidth() * 0.52f); return r.withTrimmedLeft (6.0f); }
        juce::Rectangle<float> fPlot() const   { return fltPane().reduced (14.0f, 0).withTrimmedTop (62.0f).withTrimmedBottom (24.0f).withTrimmedRight (24.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.72f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (copper().withAlpha (0.08f), 0, r.getY(), copper().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (copper().withAlpha (0.2f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (copper().withAlpha (0.8f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        // ---- params
        SqDsp::ParamBank& bank() const { return mp->synthBank(); }
        te::AutomatableParameter& param (int i) const { return *bank().params[(size_t) i]; }
        float val (int i) const { return param (i).getCurrentValue(); }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); }
        juce::UndoManager* um() const { return &mp->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }

        // ---- live
        StudioUi::StudioMeter meter; float vAmp = 0, vFlt = 0, vCut = 1000; int vNote = -1;
        void timerCallback() override
        {
            if (mp == nullptr) return;
            float l, r; mp->takePeaks (l, r); meter.update (l, r, 0.0f);
            mp->newestVoice (vAmp, vFlt, vCut, vNote);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (mp == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("mote");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.9f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.25f), a.getCentreX(), a.getY(), night().withAlpha (0.88f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (copper().withAlpha (0.26f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            hits.clear();
            paintTopBar (g); paintOsc (g); paintFilter (g); paintEnv (g); paintDeck (g);
            meter.draw (g, meterR(), copper(), false);
        }

        // ---- chips
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 9.0f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c, float fsz = 9.0f)
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.16f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (fsz, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void paintTopBar (juce::Graphics& g)
        {
            auto bar = topBar(); g.setFont (Theme::font (9.0f, true));
            g.setColour (text (0.5f)); g.drawText ("VOICE", bar.removeFromLeft (42.0f), juce::Justification::centredLeft);
            const auto pm = SqSynth::polyModeNames();
            for (int i = 0; i < 3; ++i) { chip (g, bar.removeFromLeft (widthOf (pm[i]) + 20.0f), pm[i], mp->polyModeValue().get() == i, 10 + i, copper()); bar.removeFromLeft (3.0f); }
            bar.removeFromLeft (10.0f); g.setColour (text (0.5f)); g.drawText ("GLIDE", bar.removeFromLeft (40.0f), juce::Justification::centredLeft);
            const auto gm = SqSynth::glideModeNames();
            for (int i = 0; i < 3; ++i) { chip (g, bar.removeFromLeft (widthOf (gm[i]) + 20.0f), gm[i], mp->glideModeValue().get() == i, 20 + i, copper()); bar.removeFromLeft (3.0f); }
            bar.removeFromLeft (10.0f);
            const auto un = "UNISON " + juce::String (mp->unisonValue().get());
            chip (g, bar.removeFromLeft (widthOf (un) + 22.0f), un, mp->unisonValue().get() > 1, 30, plum()); bar.removeFromLeft (3.0f);
            const auto q = mp->qualityValue().get() == 1 ? juce::String ("HD  2X") : juce::String ("ECO");
            chip (g, bar.removeFromLeft (widthOf (q) + 22.0f), q, mp->qualityValue().get() == 1, 31, plum()); bar.removeFromLeft (3.0f);
            const auto bd = "BEND " + juce::String::charToString (0x00b1) + juce::String (mp->bendRangeValue().get());
            chip (g, bar.removeFromLeft (widthOf (bd) + 20.0f), bd, false, 32, plum()); bar.removeFromLeft (3.0f);
            const auto mx = "MAX " + juce::String (mp->maxPolyValue().get());
            chip (g, bar.removeFromLeft (widthOf (mx) + 20.0f), mx, false, 33, plum()); bar.removeFromLeft (10.0f);
            const auto ls = "LFO: " + P::lfoNames()[juce::jlimit (0, 5, mp->lfoWave.get())].toUpperCase();
            chip (g, bar.removeFromLeft (widthOf (ls) + 22.0f), ls, true, 40, ember()); bar.removeFromLeft (3.0f);
            const auto sy = mp->lfoSync.get() > 0 ? P::syncNames()[mp->lfoSync.get()].toUpperCase() : juce::String (val (P::lfoRate), 2) + " HZ";
            chip (g, bar.removeFromLeft (widthOf (sy) + 22.0f), sy, mp->lfoSync.get() > 0, 41, ember());
            g.setColour (text (0.55f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("VOICES " + juce::String (mp->activeVoiceCount()) + " / " + juce::String (mp->maxVoiceCount()) + (vNote >= 0 ? "   NOTE " + juce::MidiMessage::getMidiNoteName (vNote, true, true, 3) : juce::String()), bar, juce::Justification::right);
        }

        // ---- oscillator
        juce::Rectangle<float> shapeRail() const { auto r = oscPane().reduced (14.0f, 0); return { r.getX() + 10.0f, oscPane().getY() + 34.0f, r.getWidth() - 20.0f, 18.0f }; }
        void paintOsc (juce::Graphics& g)
        {
            const auto r = oscPane(); pane (g, r, "OSCILLATOR");
            // shape rail
            const auto rail = shapeRail(); const float sh = val (P::shape);
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (rail.withSizeKeepingCentre (rail.getWidth(), 6.0f), 3.0f);
            const char* names[4] = { "SINE", "TRI", "SAW", "SQUARE" };
            g.setFont (Theme::font (7.5f, true));
            for (int i = 0; i < 4; ++i)
            {
                const float x = rail.getX() + rail.getWidth() * i / 3.0f;
                g.setColour (text (std::abs (sh - i) < 0.5f ? 0.95f : 0.4f)); g.drawText (names[i], (int) x - 24, (int) rail.getBottom() + 1, 48, 10, juce::Justification::centred);
                g.setColour (text (0.25f)); g.fillRect (x - 0.5f, rail.getCentreY() - 5, 1.0f, 10.0f);
            }
            const float hx = rail.getX() + rail.getWidth() * sh / 3.0f;
            g.setColour (copper().withAlpha (0.3f)); g.fillEllipse (hx - 9, rail.getCentreY() - 9, 18, 18);
            g.setColour (copper()); g.fillEllipse (hx - 5, rail.getCentreY() - 5, 10, 10);
            // one cycle as the oscillator draws it
            auto wv = r.reduced (14.0f, 0).withTrimmedTop (66.0f); auto bottom = wv.removeFromBottom (wv.getHeight() * 0.42f); wv = wv.withTrimmedBottom (6.0f);
            g.setColour (juce::Colours::black.withAlpha (0.38f)); g.fillRoundedRectangle (wv, 6.0f);
            g.setColour (text (0.07f)); g.drawHorizontalLine ((int) wv.getCentreY(), wv.getX() + 4, wv.getRight() - 4);
            for (int q = 1; q < 4; ++q) g.drawVerticalLine ((int) (wv.getX() + wv.getWidth() * q / 4.0f), wv.getY() + 4, wv.getBottom() - 4);
            const auto& s = mp->currentSettings();
            juce::Path cyc, subP; const int N = 256; const double dt = 1.0 / 2048.0;
            for (int i = 0; i <= N; ++i)
            {
                const double t = i / (double) N; SqMote::Osc o; o.phase = t; o.inc = dt;
                const double main = s.oscLevel * o.value (s.shape, s.width);
                const double sb = s.subLevel * (s.subWave == 0 ? SqMote::Osc::sine (std::fmod (t / (s.subOct >= 2 ? 4.0 : 2.0), 1.0)) : SqMote::Osc::pulse (std::fmod (t / (s.subOct >= 2 ? 4.0 : 2.0), 1.0), dt, 0.5));
                const float x = wv.getX() + 6 + (wv.getWidth() - 12) * (float) t;
                const float y = wv.getCentreY() - (float) juce::jlimit (-1.4, 1.4, main) * wv.getHeight() * 0.32f, ys = wv.getCentreY() - (float) juce::jlimit (-1.4, 1.4, sb) * wv.getHeight() * 0.32f;
                if (i == 0) { cyc.startNewSubPath (x, y); subP.startNewSubPath (x, ys); } else { cyc.lineTo (x, y); subP.lineTo (x, ys); }
            }
            if (s.subLevel > 0.001) { g.setColour (plum().withAlpha (0.65f)); g.strokePath (subP, juce::PathStrokeType (1.1f)); }
            g.setColour (copper().withAlpha (0.3f)); g.strokePath (cyc, juce::PathStrokeType (6.0f)); g.setColour (copper()); g.strokePath (cyc, juce::PathStrokeType (1.8f));
            if (s.noiseLevel > 0.001)
            {
                juce::Random rnd (7); juce::Path nz;
                for (int i = 0; i <= 120; ++i) { const float x = wv.getX() + 6 + (wv.getWidth() - 12) * i / 120.0f, y = wv.getCentreY() + (rnd.nextFloat() * 2 - 1) * (float) s.noiseLevel * wv.getHeight() * 0.3f; if (i == 0) nz.startNewSubPath (x, y); else nz.lineTo (x, y); }
                g.setColour (text (0.22f)); g.strokePath (nz, juce::PathStrokeType (0.7f));
            }
            g.setFont (Theme::font (8.5f, true)); g.setColour (text (0.75f));
            const juce::String pitch = juce::String ((int) val (P::octave) >= 0 ? "+" : "") + juce::String ((int) val (P::octave)) + " oct  " + juce::String ((int) val (P::semi) >= 0 ? "+" : "") + juce::String ((int) val (P::semi)) + " st  " + juce::String ((int) val (P::fine) >= 0 ? "+" : "") + juce::String ((int) val (P::fine)) + " ct";
            g.drawText (juce::String (SqMote::shapeName (sh)).toUpperCase() + "  " + juce::String (sh, 2) + (sh > 2.5f ? "   WIDTH " + juce::String ((int) val (P::width)) + " %" : juce::String()), wv.reduced (8.0f, 5.0f), juce::Justification::topLeft);
            g.drawText (pitch, wv.reduced (8.0f, 5.0f), juce::Justification::bottomRight);
            if (s.subLevel > 0.001) { g.setColour (plum()); g.drawText ("SUB " + juce::String ((int) std::lround (s.subLevel * 100)) + " %", wv.reduced (8.0f, 5.0f), juce::Justification::topRight); }
            // sub chips + scope
            auto chips = bottom.removeFromTop (22.0f);
            g.setColour (text (0.5f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("SUB", chips.removeFromLeft (30.0f), juce::Justification::centredLeft);
            chip (g, chips.removeFromLeft (52.0f), "-1 OCT", mp->subOct.get() == 1, 50, plum(), 8.0f); chips.removeFromLeft (3.0f);
            chip (g, chips.removeFromLeft (52.0f), "-2 OCT", mp->subOct.get() == 2, 51, plum(), 8.0f); chips.removeFromLeft (8.0f);
            chip (g, chips.removeFromLeft (50.0f), "SINE", mp->subWave.get() == 0, 52, plum(), 8.0f); chips.removeFromLeft (3.0f);
            chip (g, chips.removeFromLeft (60.0f), "SQUARE", mp->subWave.get() == 1, 53, plum(), 8.0f);
            bottom.removeFromTop (6.0f); auto sc = bottom.withTrimmedBottom (12.0f);
            g.setColour (juce::Colours::black.withAlpha (0.38f)); g.fillRoundedRectangle (sc, 6.0f);
            const auto& scope = mp->scopeData(); const int w = mp->scopeWritePos(); juce::Path sp;
            for (int i = 0; i < 512; ++i) { const float x = sc.getX() + 4 + (sc.getWidth() - 8) * i / 511.0f, y = sc.getCentreY() - juce::jlimit (-1.0f, 1.0f, scope[(size_t) ((w + i) & 511)]) * sc.getHeight() * 0.45f; if (i == 0) sp.startNewSubPath (x, y); else sp.lineTo (x, y); }
            g.setColour (ember().withAlpha (0.85f)); g.strokePath (sp, juce::PathStrokeType (1.0f));
            g.setColour (text (0.4f)); g.setFont (Theme::font (7.5f, true)); g.drawText ("OUTPUT SCOPE", sc.reduced (6.0f, 3.0f), juce::Justification::topLeft);
        }

        // ---- filter
        static float xOf (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0)) * p.getWidth(); }
        static double hzOf (juce::Rectangle<float> p, float x) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
        static float yOfDb (juce::Rectangle<float> p, double dbv) { return p.getY() + (float) juce::jlimit (0.0, 1.0, (30.0 - dbv) / 78.0) * p.getHeight(); }
        SqFissure::Slot slot() const { SqFissure::Slot s; s.type = SqMote::fissureType (mp->filterType.get()); s.slope = mp->slope.get(); s.cutoffHz = val (P::cutoff); s.resonance = val (P::res) / 100.0; return s; }
        juce::Point<float> nodePos() const { const auto p = fPlot(); const auto s = slot(); return { xOf (p, s.cutoffHz), yOfDb (p, SqFissure::Engine::responseDb (s, 48000.0, s.cutoffHz)) }; }
        void paintFilter (juce::Graphics& g)
        {
            const auto r = fltPane(); pane (g, r, "FILTER");
            auto row = r.reduced (14.0f, 0).withTrimmedTop (28.0f).removeFromTop (20.0f);
            const auto tn = SqMote::filterTypeNames();
            for (int t = 0; t < tn.size(); ++t) { const auto s = tn[t].toUpperCase(); chip (g, row.removeFromLeft (widthOf (s, 8.0f) + 14.0f), s, mp->filterType.get() == t, 60 + t, copper(), 8.0f); row.removeFromLeft (3.0f); }
            row.removeFromLeft (6.0f);
            for (int sl : { 12, 24, 48 }) { chip (g, row.removeFromLeft (44.0f), juce::String (sl) + " dB", mp->slope.get() == sl, 70 + sl, ember(), 8.0f); row.removeFromLeft (3.0f); }
            const auto p = fPlot();
            g.setFont (Theme::font (8.0f, true));
            for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            {
                const float x = xOf (p, hz); g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.34f)); g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), (int) x - 14, (int) p.getBottom() + 4, 28, 12, juce::Justification::centred);
            }
            g.drawText ("Hz", (int) p.getRight() - 24, (int) p.getBottom() + 4, 24, 12, juce::Justification::right);
            for (int d = 24; d >= -48; d -= 12)
            {
                const float y = yOfDb (p, d); g.setColour (text (d == 0 ? 0.15f : 0.05f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
                g.setColour (text (0.34f)); g.drawText ((d > 0 ? "+" : "") + juce::String (d), (int) p.getRight() + 2, (int) y - 6, 24, 12, juce::Justification::left);
            }
            // the sweep: envelope depth (and the LFO), drawn from the knob position
            const double fc = val (P::cutoff), envO = val (P::fEnvDepth), lfoO = val (P::lfoCut);
            const double up = std::max (0.0, envO) + std::abs (lfoO), dn = std::min (0.0, envO) - std::abs (lfoO);
            if (up != 0 || dn != 0)
            {
                const float x0 = xOf (p, fc * std::pow (2.0, dn)), x1 = xOf (p, fc * std::pow (2.0, up));
                g.setGradientFill (juce::ColourGradient (copper().withAlpha (0.16f), 0, p.getY(), copper().withAlpha (0.0f), 0, p.getBottom(), false));
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, p.getY(), x1, p.getBottom()));
                g.setColour (copper().withAlpha (0.35f)); g.drawVerticalLine ((int) x0, p.getY(), p.getBottom()); g.drawVerticalLine ((int) x1, p.getY(), p.getBottom());
            }
            if (vNote >= 0) { const float xn = xOf (p, vCut); g.setColour (ember().withAlpha (0.9f)); g.drawVerticalLine ((int) xn, p.getY(), p.getBottom()); g.setFont (Theme::font (7.5f, true)); g.drawText ("NOW", (int) xn - 14, (int) p.getY() + 2, 28, 10, juce::Justification::centred); }
            const auto s = slot(); juce::Path c, fill;
            for (int i = 0; i <= 200; ++i) { const float x = p.getX() + p.getWidth() * i / 200.0f; const float y = yOfDb (p, SqFissure::Engine::responseDb (s, 48000.0, hzOf (p, x))); if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y); }
            fill = c; fill.lineTo (p.getRight(), p.getBottom()); fill.lineTo (p.getX(), p.getBottom()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (copper().withAlpha (0.18f), 0, p.getY(), copper().withAlpha (0.0f), 0, p.getBottom(), false)); g.fillPath (fill);
            g.setColour (copper().withAlpha (0.3f)); g.strokePath (c, juce::PathStrokeType (6.0f)); g.setColour (copper()); g.strokePath (c, juce::PathStrokeType (1.8f));
            const auto n = nodePos(); const bool hot = hoverNode || dragNode;
            g.setColour (copper().withAlpha (hot ? 0.42f : 0.2f)); g.fillEllipse (n.x - 13, n.y - 13, 26, 26);
            g.setColour (night()); g.fillEllipse (n.x - 8, n.y - 8, 16, 16); g.setColour (copper()); g.drawEllipse (n.x - 8, n.y - 8, 16, 16, 2.0f);
            auto tag = juce::Rectangle<float> (n.x - 62, n.y - 34, 124, 18).constrainedWithin (p.expanded (18.0f));
            g.setColour (night().withAlpha (0.9f)); g.fillRoundedRectangle (tag, 9.0f); g.setColour (copper().withAlpha (0.6f)); g.drawRoundedRectangle (tag, 9.0f, 1.0f);
            g.setColour (text (0.95f)); g.setFont (Theme::font (9.0f, true));
            g.drawText ((fc >= 1000 ? juce::String (fc / 1000.0, 2) + " kHz" : juce::String ((int) std::lround (fc)) + " Hz") + "  Q " + juce::String ((int) std::lround (val (P::res))) + "%", tag, juce::Justification::centred);
            g.setFont (Theme::font (8.0f, true)); g.setColour (text (0.6f));
            g.drawText ("ENV " + juce::String (envO >= 0 ? "+" : "") + juce::String (envO, 1) + " oct   KEY " + juce::String ((int) val (P::keyTrack)) + " %   VEL " + juce::String (val (P::velDepth) >= 0 ? "+" : "") + juce::String (val (P::velDepth), 1) + " oct",
                        p.reduced (6.0f).removeFromBottom (12.0f), juce::Justification::right);
        }

        // ---- envelopes + matrix
        void drawEnv (juce::Graphics& g, juce::Rectangle<float> r, int a, int d, int s, int rl, float live, juce::Colour col, const juce::String& title)
        {
            g.setColour (juce::Colours::black.withAlpha (0.38f)); g.fillRoundedRectangle (r, 6.0f);
            auto in = r.reduced (8.0f, 6.0f); auto head = in.removeFromTop (12.0f); auto foot = in.removeFromBottom (12.0f);
            g.setColour (col); g.setFont (Theme::font (8.5f, true)); g.drawText (title, head, juce::Justification::left);
            const float A = val (a), D = val (d), S = val (s) / 100.0f, R = val (rl);
            // widths by the square root of each time, so short and long stages both read; a fixed sustain plateau
            const float wa = std::sqrt (A), wd = std::sqrt (D), wr = std::sqrt (R), ws = (wa + wd + wr) * 0.35f + 1.0f, tot = wa + wd + ws + wr;
            const float x0 = in.getX(), xa = x0 + in.getWidth() * wa / tot, xd = xa + in.getWidth() * wd / tot, xs = xd + in.getWidth() * ws / tot, xr = in.getRight();
            auto yL = [&] (float lv) { return in.getBottom() - lv * in.getHeight(); };
            juce::Path pth; pth.startNewSubPath (x0, yL (0)); pth.lineTo (xa, yL (1));
            for (int i = 1; i <= 24; ++i) { const float t = i / 24.0f; pth.lineTo (xa + (xd - xa) * t, yL (S + (1 - S) * std::pow (0.01f, t))); }
            pth.lineTo (xs, yL (S));
            for (int i = 1; i <= 24; ++i) { const float t = i / 24.0f; pth.lineTo (xs + (xr - xs) * t, yL (S * std::pow (0.01f, t))); }
            juce::Path fill (pth); fill.lineTo (xr, yL (0)); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (col.withAlpha (0.22f), 0, in.getY(), col.withAlpha (0.0f), 0, in.getBottom(), false)); g.fillPath (fill);
            g.setColour (col); g.strokePath (pth, juce::PathStrokeType (1.6f));
            for (float x : { xa, xd, xs }) { g.setColour (text (0.12f)); g.drawVerticalLine ((int) x, in.getY(), in.getBottom()); }
            g.setColour (text (0.12f)); g.drawHorizontalLine ((int) yL (S), in.getX(), in.getRight());
            if (vNote >= 0) { const float y = yL (live); g.setColour (col.withAlpha (0.35f)); g.fillEllipse (in.getRight() - 14, y - 6, 12, 12); g.setColour (juce::Colours::white); g.fillEllipse (in.getRight() - 11, y - 3, 6, 6); }
            auto ms = [] (float v) { return v >= 1000 ? juce::String (v / 1000.0f, 2) + "s" : juce::String (v, v < 10 ? 1 : 0) + "ms"; };
            g.setColour (text (0.7f)); g.setFont (Theme::font (7.5f, true));
            g.drawText ("A " + ms (A), juce::Rectangle<float> (x0, foot.getY(), std::max (60.0f, xa - x0), 12.0f), juce::Justification::left);
            g.drawText ("D " + ms (D), juce::Rectangle<float> (xa, foot.getY(), std::max (54.0f, xd - xa), 12.0f), juce::Justification::centred);
            g.drawText ("S " + juce::String ((int) std::lround (S * 100)) + "%", juce::Rectangle<float> (xd, foot.getY(), std::max (40.0f, xs - xd), 12.0f), juce::Justification::centred);
            g.drawText ("R " + ms (R), juce::Rectangle<float> (xs, foot.getY(), xr - xs, 12.0f), juce::Justification::right);
        }
        std::array<juce::Rectangle<float>, SqSynth::numModSlots> modAmtR {};
        void paintEnv (juce::Graphics& g)
        {
            const auto r = envPane(); pane (g, r, "ENVELOPES + MODULATION");
            auto in = r.reduced (12.0f, 10.0f); in.removeFromTop (18.0f);
            const float eh = std::min (110.0f, (in.getHeight() - 150.0f) * 0.5f);
            drawEnv (g, in.removeFromTop (eh), P::aA, P::aD, P::aS, P::aR, vAmp, copper(), "AMP"); in.removeFromTop (6.0f);
            drawEnv (g, in.removeFromTop (eh), P::fA, P::fD, P::fS, P::fR, vFlt, ember(), "FILTER  (" + juce::String (val (P::fEnvDepth) >= 0 ? "+" : "") + juce::String (val (P::fEnvDepth), 1) + " oct)");
            in.removeFromTop (8.0f);
            g.setColour (text (0.5f)); g.setFont (Theme::font (8.0f, true)); g.drawText ("MATRIX  -  source, target, amount (drag)", in.removeFromTop (12.0f), juce::Justification::left);
            in.removeFromTop (2.0f);
            const auto srcN = SqSynth::modSourceNames(); const auto dests = mp->modDestinations();
            const float rowH = std::min (18.0f, in.getHeight() / SqSynth::numModSlots);
            for (int m = 0; m < SqSynth::numModSlots; ++m)
            {
                auto row = in.removeFromTop (rowH - 2.0f); in.removeFromTop (2.0f);
                const int src = mp->modSourceValue (m).get(); const auto dst = mp->modDestValue (m).get();
                auto sR = row.removeFromLeft (row.getWidth() * 0.34f); row.removeFromLeft (3.0f); auto dR = row.removeFromLeft (row.getWidth() * 0.45f); row.removeFromLeft (3.0f); modAmtR[(size_t) m] = row;
                chip (g, sR, srcN[juce::jlimit (0, srcN.size() - 1, src)].toUpperCase(), src != 0, 200 + m, plum(), 7.5f);
                chip (g, dR, dst.isEmpty() ? juce::String ("NONE") : dst.toUpperCase(), dst.isNotEmpty(), 300 + m, copper(), 7.5f);
                const float amt = mp->modDepthValue (m).get();
                g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (row, 5.0f);
                const float cx = row.getCentreX(), x2 = cx + amt * row.getWidth() * 0.5f;
                g.setColour (plum().withAlpha (src && dst.isNotEmpty() ? 0.85f : 0.3f)); g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, x2), row.getY() + 3, std::max (cx, x2), row.getBottom() - 3), 2.0f);
                g.setColour (text (0.25f)); g.drawVerticalLine ((int) cx, row.getY() + 2, row.getBottom() - 2);
                g.setColour (text (0.85f)); g.setFont (Theme::font (7.5f, true)); g.drawText ((amt >= 0 ? "+" : "") + juce::String ((int) std::lround (amt * 100)) + " %", row, juce::Justification::centred);
            }
        }

        void menu (const juce::StringArray& names, int cur, std::function<void (int)> set)
        {
            juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<MoteViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Mote setting");
            if (id >= 10 && id < 13) { mp->polyModeValue().setValue (id - 10, um()); changed(); return; }
            if (id >= 20 && id < 23) { mp->glideModeValue().setValue (id - 20, um()); changed(); return; }
            if (id == 30) { auto& u = mp->unisonValue(); u.setValue (u.get() % 8 + 1, um()); changed(); return; }
            if (id == 31) { auto& q = mp->qualityValue(); q.setValue (1 - q.get(), um()); changed(); return; }
            if (id == 32) { auto& b = mp->bendRangeValue(); const int v[] = { 2, 7, 12, 24 }; int i = 0; while (i < 3 && v[i] <= b.get()) ++i; b.setValue (b.get() >= 24 ? 2 : v[i], um()); changed(); return; }
            if (id == 33) { auto& m = mp->maxPolyValue(); m.setValue (m.get() >= 16 ? 1 : m.get() * 2, um()); changed(); return; }
            if (id == 40) { mp->lfoWave.setValue ((mp->lfoWave.get() + 1) % 6, um()); changed(); return; }
            if (id == 41) { juce::StringArray names = P::syncNames(); names.set (0, "Free (the Rate knob)"); menu (names, mp->lfoSync.get(), [this] (int i) { mp->lfoSync.setValue (i, um()); }); return; }
            if (id == 50 || id == 51) { mp->subOct.setValue (id - 49, um()); changed(); return; }
            if (id == 52 || id == 53) { mp->subWave.setValue (id - 52, um()); changed(); return; }
            if (id >= 60 && id < 66) { mp->filterType.setValue (id - 60, um()); changed(); return; }
            if (id == 82 || id == 94 || id == 118) { mp->slope.setValue (id - 70, um()); changed(); return; }
            if (id >= 200 && id < 200 + SqSynth::numModSlots) { const int m = id - 200; menu (SqSynth::modSourceNames(), mp->modSourceValue (m).get(), [this, m] (int i) { mp->modSourceValue (m).setValue (i, um()); }); return; }
            if (id >= 300 && id < 300 + SqSynth::numModSlots)
            {
                const int m = id - 300; juce::StringArray names { "None" }; const auto d = mp->modDestinations(); for (auto& x : d) names.add (x);
                menu (names, juce::jmax (0, names.indexOf (mp->modDestValue (m).get())), [this, m, d] (int i) { mp->modDestValue (m).setValue (i == 0 ? juce::String() : d[i - 1], um()); });
            }
        }

        // ---- the deck: copper dials with a mote of light at the pointer
        struct KnobDef { int param; const char* label; int group; bool bipolar; };
        static const std::vector<KnobDef>& defs()
        {
            static const std::vector<KnobDef> d {
                { P::shape, "SHAPE", 0, false }, { P::width, "WIDTH", 0, false }, { P::octave, "OCTAVE", 0, true }, { P::semi, "SEMI", 0, true }, { P::fine, "FINE", 0, true },
                { P::oscLevel, "OSC", 0, false }, { P::subLevel, "SUB", 0, false }, { P::noise, "NOISE", 0, false }, { P::noiseColour, "COLOUR", 0, false }, { P::drive, "DRIVE", 0, false },
                { P::cutoff, "CUTOFF", 1, false }, { P::res, "RESONANCE", 1, false }, { P::fEnvDepth, "ENV", 1, true }, { P::keyTrack, "KEY", 1, false }, { P::velDepth, "VELOCITY", 1, true },
                { P::fA, "ATTACK", 2, false }, { P::fD, "DECAY", 2, false }, { P::fS, "SUSTAIN", 2, false }, { P::fR, "RELEASE", 2, false },
                { P::aA, "ATTACK", 3, false }, { P::aD, "DECAY", 3, false }, { P::aS, "SUSTAIN", 3, false }, { P::aR, "RELEASE", 3, false }, { P::ampVel, "VELOCITY", 3, false },
                { P::lfoRate, "RATE", 4, false }, { P::lfoPitch, "PITCH", 4, false }, { P::lfoCut, "CUTOFF", 4, true }, { P::lfoShapeAmt, "SHAPE", 4, true }, { P::lfoWidth, "WIDTH", 4, true },
                { P::lfoAmp, "TREMOLO", 4, false }, { P::lfoDelay, "DELAY", 4, false },
                { P::glide, "GLIDE", 5, false }, { P::uniDetune, "DETUNE", 5, false }, { P::uniSpread, "SPREAD", 5, false }, { P::level, "LEVEL", 5, true } };
            return d;
        }
        static int rowOf (int group) { return group <= 1 ? 0 : 1; }
        bool enabled (const KnobDef& d) const
        {
            if (d.param == P::width) return val (P::shape) > 2.0f;
            if (d.param == P::lfoRate) return mp->lfoSync.get() == 0;
            if (d.param == P::glide) return mp->glideModeValue().get() != 0;
            if (d.param == P::uniDetune || d.param == P::uniSpread) return mp->unisonValue().get() > 1;
            if (d.param == P::noiseColour) return val (P::noise) > 0.5f;
            return true;
        }
        juce::Colour groupCol (int g) const { static const juce::uint32 c[6] = { 0xffffa35a, 0xffff8a4a, 0xffff6b4a, 0xffffc36b, 0xffb98cff, 0xfff0e2d0 }; return juce::Colour (c[juce::jlimit (0, 5, g)]); }
        std::vector<juce::Rectangle<float>> knobR;
        void layoutKnobs()
        {
            knobR.assign (defs().size(), {});
            auto d = deck().reduced (12.0f, 10.0f); const float rowH = (d.getHeight() - 10.0f) / 2.0f;
            for (int row = 0; row < 2; ++row)
            {
                auto rr = juce::Rectangle<float> (d.getX(), d.getY() + row * (rowH + 10.0f), d.getWidth(), rowH).withTrimmedTop (14.0f);
                std::vector<int> idx; int groups = 0, lastG = -1;
                for (int i = 0; i < (int) defs().size(); ++i) if (rowOf (defs()[(size_t) i].group) == row) { idx.push_back (i); if (defs()[(size_t) i].group != lastG) { ++groups; lastG = defs()[(size_t) i].group; } }
                const float w = (rr.getWidth() - (groups - 1) * 12.0f) / (float) idx.size(); float x = rr.getX(); lastG = -1;
                for (int i : idx) { const int g = defs()[(size_t) i].group; if (lastG >= 0 && g != lastG) x += 12.0f; lastG = g; knobR[(size_t) i] = { x, rr.getY(), w, rr.getHeight() }; x += w; }
            }
        }
        juce::String knobText (const KnobDef& d) const
        {
            const auto& s = bank().specs[(size_t) d.param]; const float v = val (d.param); const juce::String u (s.unit);
            if (d.param == P::shape) return juce::String (SqMote::shapeName (v)).substring (0, 3).toUpperCase() + " " + juce::String (v, 2);
            if (d.param == P::lfoRate && mp->lfoSync.get() > 0) return P::syncNames()[mp->lfoSync.get()];
            if (d.param == P::octave) return (v > 0 ? "+" : "") + juce::String ((int) std::lround (v));
            if (u.contains ("Hz")) return v >= 1000 ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 10 ? 2 : 0) + " Hz";
            if (u.contains ("ms")) return v >= 1000 ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms";
            if (u.contains ("oct")) return (v > 0 ? "+" : "") + juce::String (v, 2) + " oct";
            if (u.contains ("st")) return (v > 0 && s.lo < 0 ? "+" : "") + juce::String (v, s.lo < 0 ? 0 : 2) + " st";
            if (u.contains ("ct")) return (v > 0 && s.lo < 0 ? "+" : "") + juce::String (v, v < 10 && s.lo >= 0 ? 1 : 0) + " ct";
            if (u.contains ("dB")) return (v > 0 && s.lo < 0 ? "+" : "") + juce::String (v, 1) + " dB";
            if (u.contains ("%")) return juce::String ((int) std::lround (v)) + " %";
            return (v > 0 && s.lo < 0 ? "+" : "") + juce::String (v, 2);
        }
        void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, juce::Colour col, bool hotK, bool active, bool bipolar, bool en) const
        {
            const float al = en ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (22.0f).withTrimmedTop (2.0f);
            const float rO = std::min (cell.getWidth() - 10.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text ((hotK ? 0.95f : 0.58f) * al)); g.setFont (Theme::font (7.5f, true)); g.drawText (label, lab, juce::Justification::centred);
            for (int t = 0; t <= 10; ++t)
            {
                const float a = a0 + (a1 - a0) * t / 10.0f, r1 = rO * 1.04f, r2 = rO * (t % 5 == 0 ? 1.16f : 1.11f);
                g.setColour (text ((t % 5 == 0 ? 0.32f : 0.15f) * al)); g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), 0.9f);
            }
            const float ar = rO * 0.95f, as = bipolar ? (a0 + a1) * 0.5f : a0;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f * al)); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (as, av), std::max (as, av), true);
            g.setColour (col.withAlpha (0.25f * al)); g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withAlpha (al)); g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // brushed copper body: radial brushing, a darker centre well, a bright mote at the pointer
            const float rb = rO * 0.72f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9a26a).withAlpha (al), c.x - rb * 0.7f, c.y - rb, juce::Colour (0xff3a1d12).withAlpha (al), c.x + rb * 0.6f, c.y + rb, false)); g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            for (int k = 0; k < 36; ++k) { const float a = juce::MathConstants<float>::twoPi * k / 36.0f; g.setColour (juce::Colours::white.withAlpha ((k % 3 == 0 ? 0.06f : 0.025f) * al)); g.drawLine (c.x + rb * 0.35f * std::sin (a), c.y - rb * 0.35f * std::cos (a), c.x + rb * 0.97f * std::sin (a), c.y - rb * 0.97f * std::cos (a), 0.6f); }
            const float rw = rb * 0.42f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120a10).withAlpha (al), c.x, c.y - rw, juce::Colour (0xff2c1a1e).withAlpha (al), c.x, c.y + rw, false)); g.fillEllipse (c.x - rw, c.y - rw, rw * 2, rw * 2);
            g.setColour (juce::Colours::black.withAlpha (0.5f * al)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
            const juce::Point<float> m (c.x + rb * 0.72f * std::sin (av), c.y - rb * 0.72f * std::cos (av));
            g.setColour (juce::Colour (0xffffe0b8).withAlpha (0.3f * al)); g.fillEllipse (m.x - 5.5f, m.y - 5.5f, 11, 11);
            g.setColour (juce::Colour (0xfffff3e0).withAlpha (al)); g.fillEllipse (m.x - 2.4f, m.y - 2.4f, 4.8f, 4.8f);
            if (hotK) { g.setColour (col.withAlpha (0.8f)); g.drawEllipse (c.x - rb - 2, c.y - rb - 2, rb * 2 + 4, rb * 2 + 4, 1.0f); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 2.0f, 76.0f), 17.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 8.5f);
            g.setColour (active ? col : text (0.1f)); g.drawRoundedRectangle (pill, 8.5f, 1.0f);
            g.setColour ((active ? col : text (0.95f)).withMultipliedAlpha (al)); g.setFont (Theme::font (9.0f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff140d12).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (copper().withAlpha (0.22f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            static const char* titles[6] = { "OSCILLATOR", "FILTER", "FILTER ENVELOPE", "AMP ENVELOPE", "LFO  (DELAY FADES IT IN PER NOTE)", "VOICE + OUTPUT" };
            for (int grp = 0; grp < 6; ++grp)
            {
                juce::Rectangle<float> u; for (size_t i = 0; i < defs().size(); ++i) if (defs()[i].group == grp) u = u.isEmpty() ? knobR[i] : u.getUnion (knobR[i]);
                auto rr = u.withTrimmedTop (-14.0f).expanded (2.0f, 2.0f); const auto c = groupCol (grp);
                g.setColour (c.withAlpha (0.05f)); g.fillRoundedRectangle (rr, 10.0f); g.setColour (c.withAlpha (0.2f)); g.drawRoundedRectangle (rr, 10.0f, 1.0f);
                g.setColour (c.withAlpha (0.88f)); g.setFont (Theme::font (8.5f, true)); g.drawText (titles[grp], rr.reduced (10.0f, 3.0f).removeFromTop (11.0f), juce::Justification::left);
            }
            for (size_t i = 0; i < defs().size(); ++i)
            {
                const auto& k = defs()[i]; const bool en = enabled (k);
                drawKnob (g, knobR[i], k.label, knobText (k), norm (k.param), groupCol (k.group), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, k.bipolar, en);
                StudioUi::knobScale (g, knobR[i], bank().specs[(size_t) k.param], text (1.0f), 15.0f, 22.0f, en ? 1.0f : 0.4f);
            }
        }

        // ---- mouse
        int dragKnob = -1, hoverKnob = -1, dragMod = -1; bool dragNode = false, hoverNode = false, dragShape = false; float dragStartNorm = 0; juce::Point<float> dragStart;
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (3.0f, 0).contains (p) && enabled (defs()[i])) return (int) i; return -1; }
        bool onNode (juce::Point<float> p) const { return p.getDistanceFrom (nodePos()) < 13.0f; }
        int modAt (juce::Point<float> p) const { for (int m = 0; m < SqSynth::numModSlots; ++m) if (modAmtR[(size_t) m].contains (p)) return m; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); const bool n = onNode (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : n ? juce::MouseCursor::DraggingHandCursor : (modAt (e.position) >= 0 || shapeRail().expanded (0, 6).contains (e.position)) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || n != hoverNode) { hoverKnob = k; hoverNode = n; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverNode = false; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (mp == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (shapeRail().expanded (0, 6).contains (e.position)) { dragShape = true; um()->beginNewTransaction ("Mote shape"); param (P::shape).parameterChangeGestureBegin(); mouseDrag (e); return; }
            if (const int m = modAt (e.position); m >= 0) { dragMod = m; um()->beginNewTransaction ("Mote modulation amount"); mouseDrag (e); return; }
            if (onNode (e.position)) { dragNode = true; um()->beginNewTransaction ("Mote filter"); param (P::cutoff).parameterChangeGestureBegin(); param (P::res).parameterChangeGestureBegin(); return; }
            const int k = knobAt (e.position);
            if (k >= 0) { const auto d = defs()[(size_t) k]; dragKnob = k; dragStart = e.position; dragStartNorm = norm (d.param); um()->beginNewTransaction (juce::String ("Mote ") + d.label); param (d.param).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragShape) { const auto r = shapeRail(); setValue (P::shape, juce::jlimit (0.0f, 3.0f, (e.position.x - r.getX()) / r.getWidth() * 3.0f)); changed(); return; }
            if (dragMod >= 0) { const auto r = modAmtR[(size_t) dragMod]; mp->modDepthValue (dragMod).setValue (juce::jlimit (-1.0f, 1.0f, (e.position.x - r.getCentreX()) / (r.getWidth() * 0.5f)), um()); changed(); return; }
            if (dragNode)
            {
                const auto p = fPlot(); setValue (P::cutoff, (float) hzOf (p, e.position.x));
                const double dbAt = 30.0 - 78.0 * juce::jlimit (0.0, 1.0, (double) ((e.position.y - p.getY()) / p.getHeight()));
                setValue (P::res, (float) juce::jlimit (0.0, 100.0, (dbAt + 3.0) / 33.0 * 100.0)); changed(); return;
            }
            if (dragKnob < 0) return;
            setNorm (defs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragShape) param (P::shape).parameterChangeGestureEnd();
            if (dragNode) { param (P::cutoff).parameterChangeGestureEnd(); param (P::res).parameterChangeGestureEnd(); }
            if (dragKnob >= 0) param (defs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; dragNode = false; dragShape = false; dragMod = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = defs()[(size_t) k].param; um()->beginNewTransaction ("Mote reset"); param (pi_).setParameter (bank().specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Mote wheel"); const int pi_ = defs()[(size_t) k].param; setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
