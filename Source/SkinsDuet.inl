// Included inside namespace Skins (PluginSkins.h): the Duet Filter and Speck Filter faces (1.14 Studio Line, phase K).
//
// One face class, two personalities:
//  - Duet Filter, on the Higgsfield plate pa_duet (two ribbons of light behind smoked glass): lacquer-and-brass knobs whose
//    pointer is a two-colour ribbon; filter 1 is teal, filter 2 coral. The top bar holds routing, link (with the interval),
//    the LFO (shape, sync) and the envelope source; the response draws both filters' real curves - the left and right
//    channels separately when the LFO's stereo phase pulls them apart - and the combined curve; each filter is a node you
//    drag (sideways = cutoff, up / down = resonance, or gain for a peak). Swept ranges are lit bands; a line shows each
//    cutoff right now.
//  - Speck Filter, on pa_speck (a seed of green light over ripples): matte ceramic knobs with a glowing seed at the pointer;
//    one node, the swept range and the live cutoff, with the envelope level in dB and the LFO traced on the side.
// Both: an output spectrum behind the curves, Hz and dB rulers, IN / OUT meters with numbers, every knob's range printed.

    struct DuetViz : public Viz, private juce::Timer
    {
        DuetViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), dp (dynamic_cast<SqDuetFilterPlugin*> (&p)), spk (dynamic_cast<SqSpeckFilterPlugin*> (&p)),
              fft (12), window ((size_t) fftN, juce::dsp::WindowingFunction<float>::hann)
        { setInterceptsMouseClicks (true, false); spec.fill (-120.0f); lfoHist.fill (0); envHist.fill (0); startTimerHz (30); }
        SqDuetFilterPlugin* dp; SqSpeckFilterPlugin* spk;
        std::function<void()> onChange;
        int sel = 0;
        bool duet() const { return dp != nullptr; }
        SqColourBase* base() const { return dp != nullptr ? (SqColourBase*) dp : (SqColourBase*) spk; }

        // ---- palette
        static juce::Colour ink()   { return juce::Colour (0xff06080a); }
        juce::Colour fCol (int k) const { return duet() ? juce::Colour (k == 0 ? 0xff35e6c8 : 0xffff7a63) : juce::Colour (0xff9dff4a); }
        juce::Colour trim() const  { return duet() ? juce::Colour (0xffd9a95b) : juce::Colour (0xff6fe08a); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfff1f4ee).withAlpha (a); }

        // ---- geometry
        static constexpr float deckH = 196.0f;
        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (deckH).withTrimmedBottom (14.0f); }
        static float meterW() { return StudioUi::StudioMeter::width (false); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (deckH + 8.0f); r.removeFromRight (meterW() + 10.0f); return r; }
        juce::Rectangle<float> meterR() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (deckH + 8.0f); return r.removeFromRight (meterW()); }
        float sideW() const { return duet() ? 300.0f : 280.0f; }
        juce::Rectangle<float> respPane() const { auto r = middle(); return r.removeFromLeft (r.getWidth() - sideW() - 12.0f); }
        juce::Rectangle<float> side() const    { auto r = middle(); return r.removeFromRight (sideW()); }
        juce::Rectangle<float> plot() const    { return respPane().reduced (14.0f, 0).withTrimmedTop (duet() ? 44.0f : 30.0f).withTrimmedBottom (24.0f).withTrimmedRight (26.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (ink().withAlpha (0.8f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (trim().withAlpha (0.07f), 0, r.getY(), trim().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (trim().withAlpha (0.22f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.5f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        // ---- parameter access (both plug-ins)
        te::AutomatableParameter& param (int i) const { return *base()->bank.params[(size_t) i]; }
        const SqDsp::ParamSpec& specOf (int i) const { return base()->bank.specs[(size_t) i]; }
        float val (int i) const { return param (i).getCurrentValue(); }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); }
        int numFilters() const { return duet() ? 2 : 1; }
        bool filterOn (int k) const { return duet() ? dp->on[(size_t) k].get() : true; }
        int typeOf (int k) const { return duet() ? dp->type[(size_t) k].get() : spk->type.get(); }
        int slopeOf (int k) const { return duet() ? dp->slope[(size_t) k].get() : spk->slope.get(); }
        int pCut (int k) const  { return duet() ? dp->paramOf (k, SqDuetFilterPlugin::cutoff) : SqSpeckFilterPlugin::cutoff; }
        int pRes (int k) const  { return duet() ? dp->paramOf (k, SqDuetFilterPlugin::res) : SqSpeckFilterPlugin::res; }
        int pGain (int k) const { return duet() ? dp->paramOf (k, SqDuetFilterPlugin::gain) : SqSpeckFilterPlugin::gain; }
        double lfoOct (int k) const { return duet() ? dp->fp (k, SqDuetFilterPlugin::lfoDepth) : spk->bank[SqSpeckFilterPlugin::lfoDepth]; }
        double envOct (int k) const { return duet() ? dp->fp (k, SqDuetFilterPlugin::envDepth) : spk->bank[SqSpeckFilterPlugin::envDepth]; }
        float cutNow (int k, int c) const { return duet() ? dp->cutNow[(size_t) (k * 2 + c)].load() : spk->cutNow[(size_t) c].load(); }
        double phaseDeg() const { return duet() ? dp->bank[SqDuetFilterPlugin::lfoPhase] : spk->bank[SqSpeckFilterPlugin::lfoPhase]; }
        SqDuet::Engine snap() const { return duet() ? dp->snapshot() : spk->snapshot(); }

        // ---- live data
        static constexpr int fftN = 4096, pts = 256;
        juce::dsp::FFT fft; juce::dsp::WindowingFunction<float> window; std::array<float, fftN * 2> work {}; std::array<float, pts> spec {};
        std::array<float, 180> lfoHist {}, envHist {}; int histPos = 0; float envDbNow = -120;
        StudioUi::StudioMeter meter;
        void timerCallback() override
        {
            if (base() == nullptr) return;
            const auto& scope = duet() ? dp->scope : spk->scope; const int w = duet() ? dp->scopeW.load() : spk->scopeW.load();
            for (int i = 0; i < fftN; ++i) work[(size_t) i] = scope[(size_t) ((w - fftN + i) & (SqSmallFilterCommon::scopeLen - 1))];
            std::fill (work.begin() + fftN, work.end(), 0.0f);
            window.multiplyWithWindowingTable (work.data(), (size_t) fftN); fft.performFrequencyOnlyForwardTransform (work.data(), true);
            const double binHz = juce::jmax (8000.0, duet() ? dp->sampleRate() : spk->sampleRate()) / fftN;
            for (int i = 0; i < pts; ++i)
            {
                const double f0 = 20.0 * std::pow (1000.0, i / (double) pts), f1 = 20.0 * std::pow (1000.0, (i + 1) / (double) pts);
                const int b0 = (int) (f0 / binHz), b1 = std::max (b0 + 1, (int) (f1 / binHz));
                float m = 0; for (int b = b0; b < std::min (b1, fftN / 2); ++b) m = std::max (m, work[(size_t) b]);
                const float v = juce::Decibels::gainToDecibels (m * 4.0f / fftN, -120.0f) + 4.5f * (float) std::log2 (std::max (20.0, f0) / 1000.0);
                spec[(size_t) i] = v > spec[(size_t) i] ? v : spec[(size_t) i] * 0.8f + v * 0.2f;
            }
            lfoHist[(size_t) histPos] = duet() ? dp->lfoNow.load() : spk->lfoNow.load();
            envHist[(size_t) histPos] = duet() ? dp->envNow.load() : spk->envNow.load();
            envDbNow = duet() ? dp->envDb.load() : spk->envDb.load();
            histPos = (histPos + 1) % (int) lfoHist.size();
            meter.update (base()->levelIn.load(), base()->levelOut.load(), 0.0f);
            repaint();
        }
        static float xOf (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0)) * p.getWidth(); }
        static double hzOf (juce::Rectangle<float> p, float x) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
        static float yOfDb (juce::Rectangle<float> p, double dbv) { return p.getY() + (float) juce::jlimit (0.0, 1.0, (30.0 - dbv) / 78.0) * p.getHeight(); }   // +30 .. -48
        static juce::String hzText (double hz) { return hz >= 1000 ? juce::String (hz / 1000.0, hz >= 10000 ? 1 : 2) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }

        void paint (juce::Graphics& g) override
        {
            if (base() == nullptr) return;
            const auto a = all();
            g.setColour (ink()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plateD = artFor ("duet"), plateS = artFor ("speck");
                const auto& plate = duet() ? plateD : plateS;
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.9f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (ink().withAlpha (0.22f), a.getCentreX(), a.getY(), ink().withAlpha (0.88f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (trim().withAlpha (0.3f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            hits.clear();
            paintTopBar (g); paintResponse (g); paintSide (g); paintDeck (g);
            meter.draw (g, meterR(), fCol (0), false);
        }

        // ---- response
        juce::Point<float> nodeOf (int k, const SqDuet::Engine& e) const
        {
            const auto p = plot(); const auto s = e.drawnSlot (k);
            return { xOf (p, s.cutoffHz), s.type == SqFissure::peak ? yOfDb (p, s.gainDb) : yOfDb (p, SqFissure::Engine::responseDb (s, 48000.0, s.cutoffHz)) };
        }
        void paintResponse (juce::Graphics& g)
        {
            pane (g, respPane(), duet() ? "" : "RESPONSE");
            const auto p = plot(); const auto e = snap();
            g.setFont (Theme::font (8.5f, true));
            for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
            {
                const float x = xOf (p, hz);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.34f)); g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), (int) x - 16, (int) p.getBottom() + 4, 32, 12, juce::Justification::centred);
            }
            g.drawText ("Hz", (int) p.getRight() + 4, (int) p.getBottom() + 4, 22, 12, juce::Justification::left);
            for (int d = 24; d >= -48; d -= 12)
            {
                const float y = yOfDb (p, d);
                g.setColour (text (d == 0 ? 0.15f : 0.05f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
                g.setColour (text (0.34f)); g.drawText ((d > 0 ? "+" : "") + juce::String (d), (int) p.getRight() + 2, (int) y - 6, 26, 12, juce::Justification::left);
            }
            g.setColour (text (0.34f)); g.drawText ("dB", (int) p.getRight() + 2, (int) p.getY() - 16, 26, 12, juce::Justification::left);
            // output spectrum
            juce::Path sp; sp.startNewSubPath (p.getX(), p.getBottom());
            for (int i = 0; i < pts; ++i) sp.lineTo (p.getX() + p.getWidth() * (i + 0.5f) / pts, p.getY() + juce::jlimit (0.0f, 1.0f, (6.0f - spec[(size_t) i]) / 84.0f) * p.getHeight());
            sp.lineTo (p.getRight(), p.getBottom()); sp.closeSubPath();
            g.setGradientFill (juce::ColourGradient (text (0.13f), 0, p.getY(), text (0.02f), 0, p.getBottom(), false)); g.fillPath (sp);
            // swept ranges and the cutoffs right now
            for (int k = 0; k < numFilters(); ++k)
            {
                if (! filterOn (k)) continue;
                const double lo = lfoOct (k), en = envOct (k), fc = e.baseCut (k);
                const double up = std::abs (lo) + std::max (0.0, en), dn = -std::abs (lo) + std::min (0.0, en);
                if (up != 0 || dn != 0)
                {
                    const float x0 = xOf (p, fc * std::pow (2.0, dn)), x1 = xOf (p, fc * std::pow (2.0, up));
                    g.setGradientFill (juce::ColourGradient (fCol (k).withAlpha (0.17f), 0, p.getY(), fCol (k).withAlpha (0.0f), 0, p.getBottom(), false));
                    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, p.getY(), x1, p.getBottom()));
                    g.setColour (fCol (k).withAlpha (0.35f)); g.drawVerticalLine ((int) x0, p.getY(), p.getBottom()); g.drawVerticalLine ((int) x1, p.getY(), p.getBottom());
                    for (int c = 0; c < (phaseDeg() > 0.5 ? 2 : 1); ++c)
                    {
                        const float xn = xOf (p, cutNow (k, c));
                        g.setColour (fCol (k).withAlpha (0.85f)); g.drawVerticalLine ((int) xn, p.getY(), p.getBottom());
                        g.setFont (Theme::font (7.5f, true)); g.drawText (c == 0 ? "L" : "R", (int) xn - 6, (int) p.getY() + 2 + c * 10, 12, 10, juce::Justification::centred);
                    }
                }
            }
            // each filter's real curve
            for (int k = 0; k < numFilters(); ++k)
            {
                if (! filterOn (k)) continue;
                const auto s = e.drawnSlot (k); juce::Path c;
                for (int i = 0; i <= 220; ++i) { const float x = p.getX() + p.getWidth() * i / 220.0f; const float y = yOfDb (p, SqFissure::Engine::responseDb (s, 48000.0, hzOf (p, x))); if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y); }
                if (duet())
                {
                    g.setColour (fCol (k).withAlpha (k == sel ? 0.95f : 0.5f)); g.strokePath (c, juce::PathStrokeType (k == sel ? 2.0f : 1.2f));
                }
                else
                {
                    juce::Path fill (c); fill.lineTo (p.getRight(), p.getBottom()); fill.lineTo (p.getX(), p.getBottom()); fill.closeSubPath();
                    g.setGradientFill (juce::ColourGradient (fCol (0).withAlpha (0.18f), 0, p.getY(), fCol (0).withAlpha (0.0f), 0, p.getBottom(), false)); g.fillPath (fill);
                    g.setColour (fCol (0).withAlpha (0.3f)); g.strokePath (c, juce::PathStrokeType (6.0f)); g.setColour (fCol (0)); g.strokePath (c, juce::PathStrokeType (1.8f));
                }
            }
            // the combined curve (Duet): as the routing combines them
            if (duet())
            {
                juce::Path c;
                for (int i = 0; i <= 240; ++i) { const float x = p.getX() + p.getWidth() * i / 240.0f; const float y = yOfDb (p, e.responseDb (hzOf (p, x)) - dp->bank[SqDuetFilterPlugin::output]); if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y); }
                g.setColour (text (0.2f)); g.strokePath (c, juce::PathStrokeType (5.0f)); g.setColour (text (0.95f)); g.strokePath (c, juce::PathStrokeType (1.4f));
                g.setFont (Theme::font (8.0f, true)); g.setColour (text (0.6f));
                const int r = dp->routing.get();
                g.drawText (r == 0 ? "WHITE: 1 THEN 2" : r == 1 ? "WHITE: 1 + 2" : r == 2 ? "WHITE: AVERAGE OF LEFT (1) AND RIGHT (2)" : "WHITE: AVERAGE OF MID (1) AND SIDE (2)", p.reduced (6.0f).removeFromBottom (12.0f), juce::Justification::right);
            }
            // link: a bracket joining the two nodes with the interval
            if (duet() && dp->link.get() && filterOn (0) && filterOn (1))
            {
                const auto n0 = nodeOf (0, e), n1 = nodeOf (1, e); const float yb = std::min (n0.y, n1.y) - 26.0f;
                juce::Path br; br.startNewSubPath (n0.x, n0.y - 14); br.lineTo (n0.x, yb); br.lineTo (n1.x, yb); br.lineTo (n1.x, n1.y - 14);
                g.setColour (trim().withAlpha (0.75f)); g.strokePath (br, juce::PathStrokeType (1.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                const auto t = "LINK " + juce::String (dp->bank[SqDuetFilterPlugin::linkOct] >= 0 ? "+" : "") + juce::String (dp->bank[SqDuetFilterPlugin::linkOct], 2) + " oct";
                g.setFont (Theme::font (8.0f, true)); g.drawText (t, juce::Rectangle<float> ((n0.x + n1.x) * 0.5f - 50, yb - 13, 100, 12), juce::Justification::centred);
            }
            // nodes
            for (int k = 0; k < numFilters(); ++k)
            {
                if (! filterOn (k)) continue;
                const auto n = nodeOf (k, e); const bool hot = k == hoverNode || k == dragNode;
                g.setColour (fCol (k).withAlpha (hot ? 0.42f : 0.2f)); g.fillEllipse (n.x - 14, n.y - 14, 28, 28);
                g.setColour (ink()); g.fillEllipse (n.x - 9, n.y - 9, 18, 18);
                g.setColour (fCol (k)); g.drawEllipse (n.x - 9, n.y - 9, 18, 18, k == sel ? 2.2f : 1.4f);
                g.setColour (text (1.0f)); g.setFont (Theme::font (9.0f, true)); g.drawText (duet() ? juce::String (k + 1) : juce::String ("S"), juce::Rectangle<float> (n.x - 9, n.y - 9, 18, 18), juce::Justification::centred);
                if (hot || k == sel)
                {
                    const auto s = e.drawnSlot (k);
                    auto tag = juce::Rectangle<float> (n.x - 66, n.y - 36, 132, 18).constrainedWithin (p.expanded (20.0f));
                    g.setColour (ink().withAlpha (0.9f)); g.fillRoundedRectangle (tag, 9.0f); g.setColour (fCol (k).withAlpha (0.65f)); g.drawRoundedRectangle (tag, 9.0f, 1.0f);
                    g.setColour (text (0.95f)); g.setFont (Theme::font (9.0f, true));
                    g.drawText (hzText (s.cutoffHz) + (s.type == SqFissure::peak ? "  " + juce::String (s.gainDb, 1) + " dB" : "  Q " + juce::String ((int) std::lround (s.resonance * 100)) + "%"), tag, juce::Justification::centred);
                }
            }
            // Duet: filter tabs along the top
            if (duet())
                for (int k = 0; k < 2; ++k)
                {
                    const auto t = tabR (k); const bool on = filterOn (k), s = k == sel;
                    g.setColour (s ? fCol (k).withAlpha (0.28f) : ink().withAlpha (0.75f)); g.fillRoundedRectangle (t, 9.0f);
                    g.setColour (fCol (k).withAlpha (on ? (s ? 1.0f : 0.6f) : 0.2f)); g.drawRoundedRectangle (t, 9.0f, 1.0f);
                    g.setColour (text (on ? 1.0f : 0.4f)); g.setFont (Theme::font (9.0f, true));
                    g.drawText ("FILTER " + juce::String (k + 1) + "  " + juce::String (SqFissure::typeName (typeOf (k))).toUpperCase() + "  " + juce::String (slopeOf (k)) + " dB" + (on ? juce::String() : "  OFF"), t, juce::Justification::centred);
                }
        }
        juce::Rectangle<float> tabR (int k) const { const auto r = respPane().reduced (14.0f, 0); const float w = (r.getWidth() - 26.0f) / 2.0f; return { r.getX() + w * k + 2.0f, respPane().getY() + 12.0f, w - 4.0f, 22.0f }; }

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

        // ---- top bar
        int lfoShapeV() const { return duet() ? dp->lfoShape.get() : spk->lfoShape.get(); }
        int lfoSyncV() const  { return duet() ? dp->lfoSync.get() : spk->lfoSync.get(); }
        bool envKeyV() const  { return duet() ? dp->envKey.get() : spk->envKey.get(); }
        double lfoHz() const  { return duet() ? dp->lfoHz() : spk->lfoHz(); }
        void paintTopBar (juce::Graphics& g)
        {
            auto bar = topBar();
            g.setFont (Theme::font (9.0f, true));
            if (duet())
            {
                g.setColour (text (0.5f)); g.drawText ("ROUTING", bar.removeFromLeft (56.0f), juce::Justification::centredLeft);
                for (int i = 0; i < 4; ++i) { const auto t = SqDuetFilterPlugin::routingNames()[i].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 22.0f), t, dp->routing.get() == i, 20 + i, trim()); bar.removeFromLeft (3.0f); }
                bar.removeFromLeft (12.0f);
                const auto lt = dp->link.get() ? "LINK  " + juce::String (dp->bank[SqDuetFilterPlugin::linkOct] >= 0 ? "+" : "") + juce::String (dp->bank[SqDuetFilterPlugin::linkOct], 2) + " OCT" : juce::String ("LINK OFF");
                chip (g, bar.removeFromLeft (widthOf (lt) + 22.0f), lt, dp->link.get(), 5, trim()); bar.removeFromLeft (12.0f);
            }
            else
            {
                g.setColour (text (0.5f)); g.drawText ("TYPE", bar.removeFromLeft (36.0f), juce::Justification::centredLeft);
                for (int t = 0; t < SqFissure::numTypes; ++t) { const auto s = juce::String (SqFissure::typeName (t)).toUpperCase(); chip (g, bar.removeFromLeft (widthOf (s, 8.5f) + 18.0f), s, typeOf (0) == t, 10 + t, fCol (0), 8.5f); bar.removeFromLeft (3.0f); }
                bar.removeFromLeft (10.0f);
            }
            const auto ls = "LFO: " + SqSmallFilterCommon::lfoNames()[juce::jlimit (0, 4, lfoShapeV())].toUpperCase();
            chip (g, bar.removeFromLeft (widthOf (ls) + 22.0f), ls, true, 30, fCol (0)); bar.removeFromLeft (3.0f);
            const auto sy = lfoSyncV() > 0 ? SqSmallFilterCommon::syncNames()[lfoSyncV()].toUpperCase() : juce::String (lfoHz(), 2) + " HZ";
            chip (g, bar.removeFromLeft (widthOf (sy) + 22.0f), sy, lfoSyncV() > 0, 40, fCol (0)); bar.removeFromLeft (10.0f);
            const auto es = envKeyV() ? juce::String ("ENV: SIDECHAIN") : juce::String ("ENV: INPUT");
            chip (g, bar.removeFromLeft (widthOf (es) + 22.0f), es, envKeyV(), 50, fCol (duet() ? 1 : 0)); bar.removeFromLeft (10.0f);
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true)); g.drawText ("ZERO LATENCY", bar, juce::Justification::right);
        }

        // ---- side column
        void filterCard (juce::Graphics& g, juce::Rectangle<float>& in, int k)
        {
            const auto col = fCol (k);
            if (duet())
            {
                auto head = in.removeFromTop (22.0f);
                chip (g, head.removeFromLeft (52.0f), filterOn (k) ? "ON" : "OFF", filterOn (k), 1000 + k, col); head.removeFromLeft (8.0f);
                g.setColour (col); g.setFont (Theme::font (10.0f, true)); g.drawText ("FILTER " + juce::String (k + 1), head, juce::Justification::centredLeft);
                hits.push_back ({ head, 1100 + k });   // clicking the title selects it
                in.removeFromTop (6.0f);
                for (int rowI = 0; rowI < 2; ++rowI)
                {
                    auto row = in.removeFromTop (20.0f); in.removeFromTop (4.0f); const int from = rowI * 4, to = std::min ((int) SqFissure::numTypes, from + 4); const float w = row.getWidth() / 4.0f;
                    for (int t = from; t < to; ++t) chip (g, juce::Rectangle<float> (row.getX() + w * (t - from), row.getY(), w - 4.0f, row.getHeight()), juce::String (SqFissure::typeName (t)).toUpperCase(), typeOf (k) == t, 2000 + k * 100 + t, col, 8.0f);
                }
            }
            auto row = in.removeFromTop (20.0f);
            g.setColour (text (0.5f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("SLOPE", row.removeFromLeft (44.0f), juce::Justification::centredLeft);
            for (int sl : { 12, 24, 48 }) { chip (g, row.removeFromLeft (58.0f), juce::String (sl) + " dB/OCT", slopeOf (k) == sl, 3000 + k * 100 + sl, col, 8.0f); row.removeFromLeft (4.0f); }
            in.removeFromTop (6.0f);
            // the numbers for this filter
            const auto e = snap(); const auto s = e.drawnSlot (k);
            auto nums = in.removeFromTop (34.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (nums, 6.0f);
            const float cw = nums.getWidth() / 3.0f;
            const juce::String vals[3] = { hzText (s.cutoffHz), s.type == SqFissure::peak ? (s.gainDb >= 0 ? "+" : "") + juce::String (s.gainDb, 1) + " dB" : juce::String ((int) std::lround (s.resonance * 100)) + " %",
                                           juce::String (SqFissure::Engine::responseDb (s, 48000.0, s.cutoffHz), 1) + " dB" };
            const char* labs[3] = { "CUTOFF", s.type == SqFissure::peak ? "GAIN" : "RESONANCE", "AT CUTOFF" };
            for (int i = 0; i < 3; ++i)
            {
                auto c = juce::Rectangle<float> (nums.getX() + cw * i, nums.getY(), cw, nums.getHeight());
                g.setColour (text (0.4f)); g.setFont (Theme::font (7.0f, true)); g.drawText (labs[i], c.removeFromTop (14.0f).withTrimmedTop (3.0f), juce::Justification::centred);
                g.setColour (col); g.setFont (Theme::font (10.5f, true)); g.drawText (vals[i], c, juce::Justification::centred);
            }
        }
        void paintSide (juce::Graphics& g)
        {
            const auto r = side(); pane (g, r, duet() ? "" : "SPECK");
            auto in = r.reduced (12.0f, 10.0f);
            if (! duet()) in.removeFromTop (16.0f);
            for (int k = 0; k < numFilters(); ++k) { filterCard (g, in, k); in.removeFromTop (k == 0 && duet() ? 10.0f : 8.0f); if (duet() && k == 0) { g.setColour (trim().withAlpha (0.18f)); g.drawHorizontalLine ((int) in.getY() - 5, in.getX(), in.getRight()); } }
            // the modulators: LFO trace (with the stereo twin) and the envelope with its level in dB
            g.setColour (text (0.5f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("MOVEMENT", in.removeFromTop (12.0f), juce::Justification::left);
            in.removeFromTop (4.0f);
            auto mr = in.removeFromTop (std::max (48.0f, std::min (duet() ? 96.0f : 300.0f, in.getHeight() - 34.0f)));
            g.setColour (juce::Colours::black.withAlpha (0.42f)); g.fillRoundedRectangle (mr, 6.0f);
            g.setColour (text (0.08f)); g.drawHorizontalLine ((int) mr.getCentreY(), mr.getX() + 4, mr.getRight() - 4);
            juce::Path a1, e1; const int N = (int) lfoHist.size();
            for (int i = 0; i < N; ++i)
            {
                const int idx = (histPos + i) % N; const float x = mr.getX() + 4 + (mr.getWidth() - 8) * i / (N - 1.0f);
                const float y1 = mr.getCentreY() - lfoHist[(size_t) idx] * mr.getHeight() * 0.38f, y3 = mr.getBottom() - 4 - envHist[(size_t) idx] * (mr.getHeight() - 8);
                if (i == 0) { a1.startNewSubPath (x, y1); e1.startNewSubPath (x, y3); } else { a1.lineTo (x, y1); e1.lineTo (x, y3); }
            }
            g.setColour (fCol (0)); g.strokePath (a1, juce::PathStrokeType (1.3f));
            const auto ec = duet() ? fCol (1) : juce::Colour (0xfff7e36a);
            g.setColour (ec); g.strokePath (e1, juce::PathStrokeType (1.3f));
            g.setFont (Theme::font (7.5f, true)); g.setColour (fCol (0)); g.drawText ("LFO", mr.reduced (6.0f, 3.0f), juce::Justification::topLeft);
            g.setColour (ec); g.drawText ("ENVELOPE", mr.reduced (6.0f, 3.0f), juce::Justification::bottomRight);
            in.removeFromTop (6.0f);
            auto rd = in.removeFromTop (20.0f);
            g.setFont (Theme::font (9.0f, true));
            g.setColour (ec); g.drawText ("ENV " + (envDbNow <= -99 ? juce::String ("-inf") : juce::String (envDbNow, 1)) + " dB", rd.removeFromLeft (rd.getWidth() * 0.5f), juce::Justification::centredLeft);
            g.setColour (fCol (0)); g.drawText ("LFO " + juce::String (lfoHz(), lfoHz() < 1 ? 2 : 1) + " Hz", rd, juce::Justification::centredRight);
        }

        juce::UndoManager* um() const { return &base()->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        void menu (const juce::StringArray& names, int cur, std::function<void (int)> set)
        {
            juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<DuetViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction (duet() ? "Duet setting" : "Speck setting");
            if (id >= 20 && id < 24 && duet()) { dp->routing.setValue (id - 20, um()); changed(); return; }
            if (id == 5 && duet()) { dp->link.setValue (! dp->link.get(), um()); changed(); return; }
            if (id >= 10 && id < 10 + SqFissure::numTypes && ! duet()) { spk->type.setValue (id - 10, um()); changed(); return; }
            if (id == 30) { auto& cv = duet() ? dp->lfoShape : spk->lfoShape; cv.setValue ((cv.get() + 1) % 5, um()); changed(); return; }
            if (id == 40) { auto* cv = duet() ? &dp->lfoSync : &spk->lfoSync; juce::StringArray names = SqSmallFilterCommon::syncNames(); names.set (0, "Free (the Rate knob)"); menu (names, cv->get(), [this, cv] (int i) { cv->setValue (i, um()); }); return; }
            if (id == 50) { auto& cv = duet() ? dp->envKey : spk->envKey; cv.setValue (! cv.get(), um()); changed(); return; }
            if (id >= 1000 && id < 1002 && duet()) { dp->on[(size_t) (id - 1000)].setValue (! dp->on[(size_t) (id - 1000)].get(), um()); changed(); return; }
            if (id >= 1100 && id < 1102) { sel = id - 1100; changed(); return; }
            if (id >= 2000 && id < 2200 && duet()) { const int k = (id - 2000) / 100, t = (id - 2000) % 100; dp->type[(size_t) k].setValue (t, um()); sel = k; changed(); return; }
            if (id >= 3000 && id < 3200)
            {
                const int k = (id - 3000) / 100, sl = (id - 3000) % 100;
                if (duet()) { dp->slope[(size_t) k].setValue (sl, um()); sel = k; } else spk->slope.setValue (sl, um());
                changed();
            }
        }

        // ---- the deck
        struct KnobDef { int param; const char* label; int group; bool bipolar; };
        std::vector<KnobDef> defs() const
        {
            if (duet())
            {
                using D = SqDuetFilterPlugin;
                return { { dp->paramOf (sel, D::cutoff), "CUTOFF", 0, false }, { dp->paramOf (sel, D::res), "RESONANCE", 0, false }, { dp->paramOf (sel, D::drive), "DRIVE", 0, false },
                         { dp->paramOf (sel, D::gain), "GAIN", 0, true }, { dp->paramOf (sel, D::fMix), "FILTER MIX", 0, false }, { dp->paramOf (sel, D::lfoDepth), "LFO DEPTH", 0, true },
                         { dp->paramOf (sel, D::envDepth), "ENV DEPTH", 0, true },
                         { D::linkOct, "LINK", 1, true }, { D::lfoRate, "LFO RATE", 1, false }, { D::lfoPhase, "STEREO PHASE", 1, false },
                         { D::envAttack, "ATTACK", 2, false }, { D::envRelease, "RELEASE", 2, false }, { D::envGain, "SENSITIVITY", 2, true },
                         { D::mix, "MIX", 3, false }, { D::output, "OUTPUT", 3, true } };
            }
            using S = SqSpeckFilterPlugin;
            return { { S::cutoff, "CUTOFF", 0, false }, { S::res, "RESONANCE", 0, false }, { S::drive, "DRIVE", 0, false }, { S::gain, "GAIN", 0, true },
                     { S::envDepth, "DEPTH", 1, true }, { S::envAttack, "ATTACK", 1, false }, { S::envRelease, "RELEASE", 1, false }, { S::envGain, "SENSITIVITY", 1, true },
                     { S::lfoDepth, "DEPTH", 2, true }, { S::lfoRate, "RATE", 2, false }, { S::lfoPhase, "STEREO PHASE", 2, false },
                     { S::mix, "MIX", 3, false }, { S::output, "OUTPUT", 3, true } };
        }
        bool enabled (const KnobDef& d) const
        {
            if (duet())
            {
                if (d.param == dp->paramOf (sel, SqDuetFilterPlugin::gain)) return dp->type[(size_t) sel].get() == SqFissure::peak;
                if (d.param == SqDuetFilterPlugin::linkOct) return dp->link.get();
                if (d.param == SqDuetFilterPlugin::lfoRate) return dp->lfoSync.get() == 0;
                if (d.param == dp->paramOf (1, SqDuetFilterPlugin::cutoff)) return ! dp->link.get();   // linked: filter 1 drives it
                return true;
            }
            if (d.param == SqSpeckFilterPlugin::gain) return spk->type.get() == SqFissure::peak;
            if (d.param == SqSpeckFilterPlugin::lfoRate) return spk->lfoSync.get() == 0;
            return true;
        }
        std::vector<juce::Rectangle<float>> knobR;
        void layoutKnobs()
        {
            knobR.clear(); auto d = deck().reduced (12.0f, 12.0f); d.removeFromTop (14.0f);
            const auto ds = defs(); const int n = (int) ds.size(); const float gaps = 3 * 12.0f; const float w = (d.getWidth() - gaps) / n; float x = d.getX();
            for (int i = 0; i < n; ++i) { if (i > 0 && ds[(size_t) i].group != ds[(size_t) i - 1].group) x += 12.0f; knobR.push_back ({ x, d.getY(), w, d.getHeight() }); x += w; }
        }
        juce::Colour groupCol (int grp) const { return grp == 0 ? fCol (sel) : grp == 1 ? (duet() ? trim() : juce::Colour (0xfff7e36a)) : grp == 2 ? (duet() ? fCol (1 - sel) : fCol (0)) : text (0.9f); }
        juce::String knobText (const KnobDef& d) const
        {
            const auto& s = specOf (d.param); const float v = val (d.param); const juce::String u (s.unit);
            if (u.contains ("Hz") && juce::String (d.label) == "CUTOFF") return hzText (v);
            if ((d.param == SqDuetFilterPlugin::lfoRate && duet()) || (d.param == SqSpeckFilterPlugin::lfoRate && ! duet()))
            { const int sy = lfoSyncV(); if (sy > 0) return SqSmallFilterCommon::syncNames()[sy]; const double r = lfoHz(); return juce::String (r, r < 1 ? 2 : 1) + " Hz"; }
            if (u.contains ("oct")) return (v > 0 ? "+" : "") + juce::String (v, 2) + " oct";
            if (u.contains ("deg")) return juce::String ((int) std::lround (v)) + juce::String::charToString (0x00b0);
            if (u.contains ("ms")) return v >= 1000 ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms";
            if (u.contains ("dB")) return (v > 0 && s.lo < 0 ? "+" : "") + juce::String (v, 1) + " dB";
            return juce::String ((int) std::lround (v)) + " %";
        }
        void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, juce::Colour col, bool hotK, bool active, bool bipolar, bool en) const
        {
            const float al = en ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (24.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 8.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text ((hotK ? 0.95f : 0.58f) * al)); g.setFont (Theme::font (8.0f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            const float ar = rO * 0.96f, as = bipolar ? (a0 + a1) * 0.5f : a0;
            // tick ring: 21 ticks, majors every 5
            for (int t = 0; t <= 20; ++t)
            {
                const float a = a0 + (a1 - a0) * t / 20.0f, r1 = rO * (t % 5 == 0 ? 1.08f : 1.03f), r2 = rO * 1.13f;
                g.setColour (text ((t % 5 == 0 ? 0.32f : 0.14f) * al)); g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), t % 5 == 0 ? 1.1f : 0.8f);
            }
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f * al)); g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (as, av), std::max (as, av), true);
            g.setColour (col.withAlpha (0.25f * al)); g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withAlpha (al)); g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float rb = rO * 0.74f;
            if (duet())
            {
                // brass collar, knurled, then a black lacquer cap with a two-colour ribbon pointer
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2cf8a).withAlpha (al), c.x - rb, c.y - rb, juce::Colour (0xff6e4a1c).withAlpha (al), c.x + rb, c.y + rb, false)); g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
                for (int k = 0; k < 48; ++k)
                {
                    const float a = juce::MathConstants<float>::twoPi * k / 48.0f;
                    g.setColour (juce::Colours::black.withAlpha (0.22f * al)); g.drawLine (c.x + rb * 0.86f * std::sin (a), c.y - rb * 0.86f * std::cos (a), c.x + rb * std::sin (a), c.y - rb * std::cos (a), 0.8f);
                }
                const float rc = rb * 0.8f;
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2d30).withAlpha (al), c.x - rc * 0.5f, c.y - rc, juce::Colour (0xff030304).withAlpha (al), c.x + rc * 0.4f, c.y + rc, false)); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
                g.setColour (juce::Colours::white.withAlpha (0.12f * al)); g.drawEllipse (c.x - rc * 0.92f, c.y - rc * 0.92f, rc * 1.84f, rc * 0.9f, 0.8f);   // lacquer gloss
                for (int s = 0; s < 2; ++s)
                {
                    const float off = (s == 0 ? -0.07f : 0.07f);
                    juce::Path rib; rib.startNewSubPath (c.x + rc * 0.15f * std::sin (av + off * 3), c.y - rc * 0.15f * std::cos (av + off * 3));
                    rib.quadraticTo (c.x + rc * 0.55f * std::sin (av - off * 4), c.y - rc * 0.55f * std::cos (av - off * 4), c.x + rc * 0.92f * std::sin (av + off), c.y - rc * 0.92f * std::cos (av + off));
                    const auto rc_ = juce::Colour (s == 0 ? 0xff35e6c8 : 0xffff7a63);
                    g.setColour (rc_.withAlpha (0.35f * al)); g.strokePath (rib, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (rc_.withAlpha (al)); g.strokePath (rib, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }
            else
            {
                // matte ceramic with ripple rings, a glowing seed at the pointer
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f3b).withAlpha (al), c.x - rb * 0.6f, c.y - rb, juce::Colour (0xff0b0d0c).withAlpha (al), c.x + rb * 0.6f, c.y + rb, false)); g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
                for (int k = 1; k <= 4; ++k) { const float rr = rb * (0.22f + 0.18f * k); g.setColour (col.withAlpha ((0.05f + 0.02f * (k % 2)) * al)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.9f); }
                g.setColour (juce::Colours::black.withAlpha (0.5f * al)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.2f);
                const juce::Point<float> seed (c.x + rb * 0.68f * std::sin (av), c.y - rb * 0.68f * std::cos (av));
                juce::Path ln; ln.startNewSubPath (c.x + rb * 0.15f * std::sin (av), c.y - rb * 0.15f * std::cos (av)); ln.lineTo (seed);
                g.setColour (col.withAlpha (0.45f * al)); g.strokePath (ln, juce::PathStrokeType (1.0f));
                g.setColour (col.withAlpha (0.28f * al)); g.fillEllipse (seed.x - 6, seed.y - 6, 12, 12);
                g.setColour (col.withAlpha (al)); g.fillEllipse (seed.x - 3, seed.y - 3, 6, 6);
                g.setColour (juce::Colours::white.withAlpha (0.8f * al)); g.fillEllipse (seed.x - 1.2f, seed.y - 1.6f, 2.0f, 2.0f);
            }
            if (hotK) { g.setColour (col.withAlpha (0.8f)); g.drawEllipse (c.x - rb - 2, c.y - rb - 2, rb * 2 + 4, rb * 2 + 4, 1.0f); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 4.0f, 84.0f), 19.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 9.5f);
            g.setColour (active ? col : text (0.1f)); g.drawRoundedRectangle (pill, 9.5f, 1.0f);
            g.setColour ((active ? col : text (0.95f)).withMultipliedAlpha (al)); g.setFont (Theme::font (10.0f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (duet() ? 0xff111214 : 0xff0d120e).withAlpha (0.95f), 0, d.getY(), ink().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (trim().withAlpha (0.24f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            const auto ds = defs();
            auto group = [&] (int grp, const juce::String& t)
            {
                int from = -1, to = -1; for (int i = 0; i < (int) ds.size(); ++i) if (ds[(size_t) i].group == grp) { if (from < 0) from = i; to = i; }
                if (from < 0) return;
                auto rr = knobR[(size_t) from].getUnion (knobR[(size_t) to]).withTrimmedTop (-14.0f).expanded (2.0f, 2.0f); const auto c = groupCol (grp);
                g.setColour (c.withAlpha (0.05f)); g.fillRoundedRectangle (rr, 10.0f); g.setColour (c.withAlpha (0.22f)); g.drawRoundedRectangle (rr, 10.0f, 1.0f);
                g.setColour (c.withAlpha (0.88f)); g.setFont (Theme::font (8.5f, true)); g.drawText (t, rr.reduced (10.0f, 3.0f).removeFromTop (11.0f), juce::Justification::left);
            };
            if (duet())
            {
                group (0, "FILTER " + juce::String (sel + 1) + "  -  " + juce::String (SqFissure::typeName (typeOf (sel))).toUpperCase() + "  (CLICK A NODE OR TAB TO SWITCH)");
                group (1, "LINK + LFO"); group (2, "ENVELOPE FOLLOWER"); group (3, "GLOBAL");
            }
            else { group (0, "FILTER  -  " + juce::String (SqFissure::typeName (typeOf (0))).toUpperCase()); group (1, "ENVELOPE FOLLOWER"); group (2, "LFO"); group (3, "GLOBAL"); }
            for (size_t i = 0; i < ds.size(); ++i)
            {
                const auto& k = ds[i]; const bool en = enabled (k); const auto col = groupCol (k.group);
                drawKnob (g, knobR[i], k.label, knobText (k), norm (k.param), col, (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, k.bipolar, en);
                StudioUi::knobScale (g, knobR[i], specOf (k.param), text (1.0f), 15.0f, 24.0f, en ? 1.0f : 0.4f);
            }
        }

        // ---- mouse
        int dragKnob = -1, hoverKnob = -1, dragNode = -1, hoverNode = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        int nodeAt (juce::Point<float> pt) const { const auto e = snap(); for (int k = numFilters() - 1; k >= 0; --k) if (filterOn (k) && pt.getDistanceFrom (nodeOf (k, e)) < 13.0f) return k; return -1; }
        int knobAt (juce::Point<float> p) const { const auto ds = defs(); for (size_t i = 0; i < knobR.size() && i < ds.size(); ++i) if (knobR[i].reduced (4.0f, 0).contains (p) && enabled (ds[i])) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), n = nodeAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : n >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || n != hoverNode) { hoverKnob = k; hoverNode = n; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverNode = -1; repaint(); }
        int nodeYParam (int k) const { return typeOf (k) == SqFissure::peak ? pGain (k) : pRes (k); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (base() == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (duet()) for (int k = 0; k < 2; ++k) if (tabR (k).contains (e.position)) { sel = k; changed(); return; }
            if (const int n = nodeAt (e.position); n >= 0)
            {
                sel = n; dragNode = n; um()->beginNewTransaction ("Filter " + juce::String (n + 1));
                // a linked filter 2 moves filter 1's cutoff (the pair travels together)
                param (pCut (duet() && n == 1 && dp->link.get() ? 0 : n)).parameterChangeGestureBegin(); param (nodeYParam (n)).parameterChangeGestureBegin();
                changed(); return;
            }
            const int k = knobAt (e.position);
            if (k >= 0) { const auto d = defs()[(size_t) k]; dragKnob = k; dragStart = e.position; dragStartNorm = norm (d.param); um()->beginNewTransaction (juce::String ("Knob ") + d.label); param (d.param).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragNode >= 0)
            {
                const auto p = plot(); double hz = hzOf (p, e.position.x);
                if (duet() && dragNode == 1 && dp->link.get()) setValue (pCut (0), (float) (hz / std::pow (2.0, dp->bank[SqDuetFilterPlugin::linkOct])));
                else setValue (pCut (dragNode), (float) hz);
                const double dbAt = 30.0 - 78.0 * juce::jlimit (0.0, 1.0, (double) ((e.position.y - p.getY()) / p.getHeight()));
                if (typeOf (dragNode) == SqFissure::peak) setValue (pGain (dragNode), (float) dbAt);
                else setValue (pRes (dragNode), (float) juce::jlimit (0.0, 100.0, (dbAt + 3.0) / 33.0 * 100.0));
                changed(); return;
            }
            if (dragKnob < 0) return;
            setNorm (defs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragNode >= 0) { param (pCut (duet() && dragNode == 1 && dp->link.get() ? 0 : dragNode)).parameterChangeGestureEnd(); param (nodeYParam (dragNode)).parameterChangeGestureEnd(); }
            if (dragKnob >= 0) param (defs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; dragNode = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = defs()[(size_t) k].param; um()->beginNewTransaction ("Knob reset"); param (pi_).setParameter (specOf (pi_).def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Knob wheel"); const int pi_ = defs()[(size_t) k].param; setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
