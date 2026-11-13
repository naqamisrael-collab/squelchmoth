// Included inside namespace Skins (PluginSkins.h): the Apex Multiband 2 face (1.14 Studio Line, phase E).
//
// Squelchmoth's own "ridgelines at night" design on the Higgsfield plate pa_apexmb2 (six ridges, each lit along its crest):
//  - top bar: the number of bands (1-6), minimum / linear phase, the detector, lookahead, latency
//  - the spectrum: input as a dim veil, output as a lit line, each band a coloured region whose ceiling moves with its
//    gain change in real time; crossovers are lit seams you drag; click a region to edit that band
//  - the band panel: the selected band's transfer curve with the live level dot, its mode, place, key, solo, bypass
//  - the deck: segmented-ring knobs drawn here - seven for the selected band (in its colour), four global ones

    struct ApexMB2Viz : public Viz, private juce::Timer
    {
        using P = SqApexMB2Plugin;
        ApexMB2Viz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mb (dynamic_cast<SqApexMB2Plugin*> (&p)), fft (12), window ((size_t) fftN, juce::dsp::WindowingFunction<float>::hann)
        {
            setInterceptsMouseClicks (true, false);
            specIn.fill (-120.0f); specOut.fill (-120.0f);
            startTimerHz (30);
        }
        SqApexMB2Plugin* mb;
        std::function<void()> onChange;
        int sel = 0;

        static juce::Colour night()  { return juce::Colour (0xff04090a); }
        static juce::Colour mint()   { return juce::Colour (0xff1affc8); }
        static juce::Colour ivory()  { return juce::Colour (0xffeefcf6); }
        static juce::Colour magenta(){ return juce::Colour (0xffff4fa3); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xffe8fff7).withAlpha (a); }
        static juce::Colour bandColour (int k)
        {
            static const juce::uint32 c[6] = { 0xff1affc8, 0xff37d6ff, 0xff5c8dff, 0xff9b7bff, 0xffd46bff, 0xffff6fb4 };
            return juce::Colour (c[juce::jlimit (0, 5, k)]);
        }

        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (214.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (222.0f); return r; }
        juce::Rectangle<float> specPane() const { auto r = middle(); return r.removeFromLeft (r.getWidth() - 342.0f); }
        juce::Rectangle<float> bandPane() const { auto r = middle(); return r.removeFromRight (330.0f); }
        juce::Rectangle<float> plot() const    { return specPane().reduced (14.0f, 0).withTrimmedTop (44.0f).withTrimmedBottom (26.0f).withTrimmedRight (24.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.70f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (mint().withAlpha (0.06f), 0, r.getY(), mint().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (mint().withAlpha (0.14f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        // ---- spectrum analysis (in / out), smoothed
        static constexpr int fftN = 4096, specPts = 256;
        juce::dsp::FFT fft; juce::dsp::WindowingFunction<float> window;
        std::array<float, fftN * 2> work {}; std::array<float, specPts> specIn {}, specOut {};
        void analyse (const std::array<float, P::scopeLen>& src, std::array<float, specPts>& dst)
        {
            const int w = mb->scopeW.load();
            for (int i = 0; i < fftN; ++i) work[(size_t) i] = src[(size_t) ((w - fftN + i) & (P::scopeLen - 1))];
            std::fill (work.begin() + fftN, work.end(), 0.0f);
            window.multiplyWithWindowingTable (work.data(), (size_t) fftN);
            fft.performFrequencyOnlyForwardTransform (work.data(), true);
            const double fs = juce::jmax (8000.0, mb->sampleRate()); const double binHz = fs / fftN;
            for (int i = 0; i < specPts; ++i)
            {
                const double f0 = 20.0 * std::pow (1000.0, i / (double) specPts), f1 = 20.0 * std::pow (1000.0, (i + 1) / (double) specPts);
                int b0 = (int) (f0 / binHz), b1 = std::max (b0 + 1, (int) (f1 / binHz));
                float m = 0; for (int b = b0; b < std::min (b1, fftN / 2); ++b) m = std::max (m, work[(size_t) b]);
                // +4.5 dB/oct tilt so music reads level across the range (as the ear hears it)
                const float dbv = juce::Decibels::gainToDecibels (m * 4.0f / fftN, -120.0f) + 4.5f * (float) std::log2 (std::max (20.0, f0) / 1000.0);
                dst[(size_t) i] = dbv > dst[(size_t) i] ? dbv : dst[(size_t) i] * 0.82f + dbv * 0.18f;
            }
        }
        void timerCallback() override { if (mb != nullptr) { analyse (mb->scopeIn, specIn); analyse (mb->scopeOut, specOut); } repaint(); }

        static float xOf (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0)) * p.getWidth(); }
        static double hzOf (juce::Rectangle<float> p, float x) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
        static float yOfDb (juce::Rectangle<float> p, double dbv) { return p.getY() + (float) juce::jlimit (0.0, 1.0, (12.0 - dbv) / 84.0) * p.getHeight(); }   // +12..-72
        static float yOfGain (juce::Rectangle<float> p, double dbv) { return p.getY() + p.getHeight() * 0.40f - (float) juce::jlimit (-24.0, 24.0, dbv) / 24.0f * p.getHeight() * 0.36f; }

        void paint (juce::Graphics& g) override
        {
            if (mb == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("apexmb2");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.85f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.22f), a.getCentreX(), a.getY(), night().withAlpha (0.85f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (mint().withAlpha (0.22f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            sel = juce::jlimit (0, nb() - 1, sel);
            paintTopBar (g);
            paintSpectrum (g);
            paintBandPane (g);
            paintDeck (g);
        }
        int nb() const { return juce::jlimit (1, P::maxBands, mb->numBands.get()); }

        // ---- the spectrum with band regions
        juce::Rectangle<float> tabR (int k) const
        {
            const auto p = plot(); const auto prm = mb->params(); const auto x = SqMB2::sanitise (prm, 48000.0);
            const float l = k == 0 ? p.getX() : xOf (p, x[(size_t) k - 1]), r = k == nb() - 1 ? p.getRight() : xOf (p, x[(size_t) k]);
            return { l + 2.0f, specPane().getY() + 22.0f, std::max (8.0f, r - l - 4.0f), 18.0f };
        }
        void paintSpectrum (juce::Graphics& g)
        {
            const auto r = specPane(); pane (g, r, "");
            const auto p = plot(); const auto prm = mb->params(); const int n = nb();
            g.setFont (Theme::font (8.5f, true));
            for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
            {
                const float x = xOf (p, hz);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.32f)); g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), (int) x - 16, (int) p.getBottom() + 4, 32, 12, juce::Justification::centred);
            }
            for (int d = -24; d <= 24; d += 12)
            {
                const float y = yOfGain (p, d);
                g.setColour (text (d == 0 ? 0.14f : 0.05f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
                g.setColour (text (0.32f)); g.drawText ((d > 0 ? "+" : "") + juce::String (d), (int) p.getRight() + 2, (int) y - 6, 24, 12, juce::Justification::left);
            }
            // spectra
            auto specPath = [&] (const std::array<float, specPts>& s, bool closed)
            {
                juce::Path pa;
                for (int i = 0; i < specPts; ++i)
                {
                    const float x = p.getX() + p.getWidth() * (i + 0.5f) / specPts, y = yOfDb (p, s[(size_t) i]);
                    if (i == 0) { if (closed) { pa.startNewSubPath (p.getX(), p.getBottom()); pa.lineTo (x, y); } else pa.startNewSubPath (x, y); } else pa.lineTo (x, y);
                }
                if (closed) { pa.lineTo (p.getRight(), p.getBottom()); pa.closeSubPath(); }
                return pa;
            };
            g.setGradientFill (juce::ColourGradient (ivory().withAlpha (0.10f), 0, p.getY(), ivory().withAlpha (0.01f), 0, p.getBottom(), false)); g.fillPath (specPath (specIn, true));
            g.setColour (ivory().withAlpha (0.55f)); g.strokePath (specPath (specOut, false), juce::PathStrokeType (1.0f));
            // each band's territory, faintly tinted
            {
                const auto xs = SqMB2::sanitise (prm, 48000.0);
                for (int k = 0; k < n; ++k)
                {
                    const float l = k == 0 ? p.getX() : xOf (p, xs[(size_t) k - 1]), rr = k == n - 1 ? p.getRight() : xOf (p, xs[(size_t) k]);
                    g.setGradientFill (juce::ColourGradient (bandColour (k).withAlpha (k == sel ? 0.10f : 0.04f), 0, p.getY(), bandColour (k).withAlpha (0.0f), 0, p.getBottom(), false));
                    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (l, p.getY(), rr, p.getBottom()));
                }
            }
            // band regions: each band's response, raised or lowered by its live gain change and make-up
            const double fsN = 48000.0;
            juce::Path total;
            for (int k = 0; k < n; ++k)
            {
                const auto col = bandColour (k);
                const bool on = ! mb->bypass[(size_t) k].get();
                const double gr = on ? mb->grDb[(size_t) k].load() + mb->bp (k, P::gain) : 0.0;
                juce::Path region; bool started = false;
                for (int i = 0; i <= 200; ++i)
                {
                    const float x = p.getX() + p.getWidth() * i / 200.0f; const double hz = hzOf (p, x);
                    const double m = SqMB2::bandMag (prm, k, hz);
                    if (m < 0.02) { if (started) { region.lineTo (x, yOfGain (p, 0)); } continue; }
                    const float y = yOfGain (p, gr * m);
                    if (! started) { region.startNewSubPath (x, yOfGain (p, 0)); started = true; }
                    region.lineTo (x, y);
                }
                region.closeSubPath();
                g.setColour (col.withAlpha (k == sel ? 0.30f : 0.14f)); g.fillPath (region);
                g.setColour (col.withAlpha (k == sel ? 0.95f : 0.55f)); g.strokePath (region, juce::PathStrokeType (k == sel ? 1.6f : 1.0f));
                juce::ignoreUnused (fsN);
            }
            // the combined curve (what the whole processor does right now)
            for (int i = 0; i <= 240; ++i)
            {
                const float x = p.getX() + p.getWidth() * i / 240.0f; const double hz = hzOf (p, x);
                double lin = 0;
                for (int k = 0; k < n; ++k) { const double gk = mb->bypass[(size_t) k].get() ? 0.0 : mb->grDb[(size_t) k].load() + mb->bp (k, P::gain); lin += SqMB2::bandMag (prm, k, hz) * SqMB2::dbToGain (gk); }
                const float y = yOfGain (p, SqMB2::gainToDb (lin));
                if (i == 0) total.startNewSubPath (x, y); else total.lineTo (x, y);
            }
            g.setColour (ivory().withAlpha (0.25f)); g.strokePath (total, juce::PathStrokeType (4.0f));
            g.setColour (ivory()); g.strokePath (total, juce::PathStrokeType (1.4f));
            // crossover seams
            const auto x = SqMB2::sanitise (prm, 48000.0);
            for (int i = 0; i < n - 1; ++i)
            {
                const float sx = xOf (p, x[(size_t) i]); const bool hot = i == hoverSeam || i == dragSeam;
                g.setGradientFill (juce::ColourGradient (bandColour (i).withAlpha (0.8f), sx, p.getY(), bandColour (i + 1).withAlpha (0.8f), sx, p.getBottom(), false));
                g.fillRect (sx - (hot ? 1.5f : 0.75f), p.getY(), hot ? 3.0f : 1.5f, p.getHeight());
                auto knob = juce::Rectangle<float> (sx - 26, p.getBottom() - 20, 52, 16);
                g.setColour (night().withAlpha (0.9f)); g.fillRoundedRectangle (knob, 8.0f); g.setColour ((hot ? ivory() : mint()).withAlpha (0.7f)); g.drawRoundedRectangle (knob, 8.0f, 1.0f);
                g.setColour (text (0.95f)); g.setFont (Theme::font (8.5f, true));
                const double hz = x[(size_t) i]; g.drawText (hz >= 1000 ? juce::String (hz / 1000.0, hz < 10000 ? 2 : 1) + "k" : juce::String ((int) hz), knob, juce::Justification::centred);
            }
            // band tabs with live gain-change bars
            for (int k = 0; k < n; ++k)
            {
                const auto t = tabR (k); const auto col = bandColour (k); const bool s = k == sel;
                g.setColour (col.withAlpha (s ? 0.35f : 0.12f)); g.fillRoundedRectangle (t, 9.0f);
                g.setColour (col.withAlpha (s ? 1.0f : 0.5f)); g.drawRoundedRectangle (t, 9.0f, 1.0f);
                const float gr = mb->grDb[(size_t) k].load();
                auto bar = t.reduced (5.0f, 0).removeFromBottom (3.0f).translated (0, -2.0f);
                const float f = juce::jlimit (0.0f, 1.0f, std::abs (gr) / 24.0f);
                g.setColour (gr < 0 ? magenta() : mint()); g.fillRect (bar.withWidth (bar.getWidth() * f));
                g.setColour (text (s ? 1.0f : 0.75f)); g.setFont (Theme::font (9.0f, true));
                juce::String lab = juce::String (k + 1); if (t.getWidth() > 70) lab = "BAND " + lab + "  " + (std::abs (gr) > 0.05f ? juce::String (gr, 1) : juce::String ("0.0"));
                if (mb->solo[(size_t) k].get()) lab << "  S"; if (mb->bypass[(size_t) k].get()) lab << "  OFF";
                g.drawText (lab, t.withTrimmedBottom (3.0f), juce::Justification::centred);
            }
        }

        // ---- the selected band: transfer curve and switches
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 10.0f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c)
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (9.5f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void paintBandPane (juce::Graphics& g)
        {
            const auto r = bandPane(); const auto col = bandColour (sel);
            pane (g, r, "BAND " + juce::String (sel + 1) + "  -  " + juce::String (SqMB2::modeName (mb->mode[(size_t) sel].get())).toUpperCase());
            auto in = r.reduced (14.0f, 28.0f);
            // transfer curve: input level (x) against output level (y), -60..0 dB
            const float side = std::min (in.getWidth(), in.getHeight() - 104.0f);
            auto cv = in.removeFromTop (side).withSizeKeepingCentre (side, side);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (cv, 6.0f);
            auto px = [&] (double dbv) { return cv.getX() + (float) juce::jlimit (0.0, 1.0, (dbv + 60.0) / 60.0) * cv.getWidth(); };
            auto py = [&] (double dbv) { return cv.getBottom() - (float) juce::jlimit (0.0, 1.0, (dbv + 60.0) / 60.0) * cv.getHeight(); };
            for (int d = -60; d <= 0; d += 12) { g.setColour (text (0.05f)); g.drawVerticalLine ((int) px (d), cv.getY(), cv.getBottom()); g.drawHorizontalLine ((int) py (d), cv.getX(), cv.getRight()); }
            g.setColour (text (0.15f)); g.drawLine (px (-60), py (-60), px (0), py (0), 1.0f);
            const auto prm = mb->params(); const auto& b = prm.band[(size_t) sel];
            juce::Path c;
            for (int i = 0; i <= 120; ++i)
            {
                const double xin = -60.0 + 60.0 * i / 120.0, yout = xin + SqMB2::curveDb (b, xin) + b.gainDb;
                if (i == 0) c.startNewSubPath (px (xin), py (yout)); else c.lineTo (px (xin), py (yout));
            }
            g.setColour (col.withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (6.0f));
            g.setColour (col); g.strokePath (c, juce::PathStrokeType (2.0f));
            // threshold marker + live dot
            g.setColour (col.withAlpha (0.4f)); g.drawVerticalLine ((int) px (b.thrDb), cv.getY(), cv.getBottom());
            const double lv = mb->levelDb[(size_t) sel].load();
            if (lv > -60)
            {
                const double yo = lv + mb->grDb[(size_t) sel].load() + b.gainDb;
                g.setColour (ivory().withAlpha (0.3f)); g.fillEllipse (px (lv) - 7, py (yo) - 7, 14, 14);
                g.setColour (ivory()); g.fillEllipse (px (lv) - 3.5f, py (yo) - 3.5f, 7, 7);
            }
            g.setColour (text (0.35f)); g.setFont (Theme::font (8.0f, true));
            g.drawText ("IN", cv.reduced (4.0f).removeFromBottom (10.0f), juce::Justification::right);
            g.drawText ("OUT", cv.reduced (4.0f).removeFromTop (10.0f), juce::Justification::left);
            in.removeFromTop (10.0f);
            // mode chips
            auto row = [&] (float h) { auto rr = in.removeFromTop (h); in.removeFromTop (6.0f); return rr; };
            {
                auto rr = row (22.0f); const float w = rr.getWidth() / 4.0f;
                for (int m = 0; m < 4; ++m) chip (g, juce::Rectangle<float> (rr.getX() + w * m, rr.getY(), w - 4.0f, rr.getHeight()), juce::String (SqMB2::modeName (m)).toUpperCase(), mb->mode[(size_t) sel].get() == m, 100 + m, col);
            }
            {
                auto rr = row (22.0f); const float w = rr.getWidth() / 5.0f;
                for (int pl = 0; pl < 5; ++pl) chip (g, juce::Rectangle<float> (rr.getX() + w * pl, rr.getY(), w - 4.0f, rr.getHeight()), juce::String (SqMB2::placeName (pl)).toUpperCase(), mb->place[(size_t) sel].get() == pl, 110 + pl, col);
            }
            {
                auto rr = row (22.0f); const float w = rr.getWidth() / 3.0f;
                chip (g, juce::Rectangle<float> (rr.getX(), rr.getY(), w - 4.0f, rr.getHeight()), mb->keyOf[(size_t) sel].get() ? "KEY: SIDE-CHAIN" : "KEY: ITSELF", mb->keyOf[(size_t) sel].get() == 1, 120, col);
                chip (g, juce::Rectangle<float> (rr.getX() + w, rr.getY(), w - 4.0f, rr.getHeight()), "SOLO", mb->solo[(size_t) sel].get(), 121, juce::Colour (0xffffd84f));
                chip (g, juce::Rectangle<float> (rr.getX() + w * 2, rr.getY(), w - 4.0f, rr.getHeight()), "BYPASS", mb->bypass[(size_t) sel].get(), 122, magenta());
            }
        }

        // ---- the deck: segmented-ring knobs
        struct KnobDef { int band; int k; const char* label; };   // band -1 = global (k = the global param)
        std::vector<KnobDef> defs() const
        {
            return { { sel, P::thr, "THRESHOLD" }, { sel, P::ratio, "RATIO" }, { sel, P::range, "RANGE" }, { sel, P::knee, "KNEE" }, { sel, P::attack, "ATTACK" },
                     { sel, P::release, "RELEASE" }, { sel, P::gain, "GAIN" }, { -1, P::input, "INPUT" }, { -1, P::lookahead, "LOOKAHEAD" }, { -1, P::mix, "MIX" }, { -1, P::output, "OUTPUT" } };
        }
        int paramIndex (const KnobDef& d) const { return d.band < 0 ? d.k : mb->paramOf (d.band, d.k); }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *mb->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        juce::UndoManager* um() const { return &mb->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }

        void layoutKnobs()
        {
            knobR.clear();
            auto d = deck().reduced (16.0f, 12.0f);
            auto bandPart = d.removeFromLeft (d.getWidth() * 0.66f); d.removeFromLeft (16.0f);
            bandPart.removeFromTop (14.0f); d.removeFromTop (14.0f);
            for (int i = 0; i < 7; ++i) knobR.push_back ({ bandPart.getX() + bandPart.getWidth() / 7 * i, bandPart.getY(), bandPart.getWidth() / 7, bandPart.getHeight() });
            for (int i = 0; i < 4; ++i) knobR.push_back ({ d.getX() + d.getWidth() / 4 * i, d.getY(), d.getWidth() / 4, d.getHeight() });
        }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, juce::Colour col, bool hot, bool active, bool bipolar)
        {
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (24.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 12.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text (hot ? 0.95f : 0.55f)); g.setFont (Theme::font (9.0f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            g.setGradientFill (juce::ColourGradient (col.withAlpha (hot ? 0.18f : 0.09f), c.x, c.y, col.withAlpha (0.0f), c.x, c.y + rO * 1.4f, true));
            g.fillEllipse (c.x - rO * 1.35f, c.y - rO * 1.35f, rO * 2.7f, rO * 2.7f);
            // segmented ring: 31 capsules; lit ones glow in the band colour
            const int segs = 31; const float r1 = rO * 0.80f, r2 = rO;
            for (int s = 0; s < segs; ++s)
            {
                const float t = s / (float) (segs - 1), a = a0 + (a1 - a0) * t;
                const bool lit = bipolar ? (t >= std::min (0.5f, v) - 0.001f && t <= std::max (0.5f, v) + 0.001f) : t <= v + 0.001f;
                const juce::Point<float> pA (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a)), pB (c.x + r2 * std::sin (a), c.y - r2 * std::cos (a));
                if (lit) { g.setColour (col.withAlpha (0.35f)); g.drawLine ({ pA, pB }, 5.5f); }
                g.setColour (lit ? col.brighter (0.2f) : text (0.10f)); g.drawLine ({ pA, pB }, 2.6f);
            }
            // machined body: dark matte with concentric turning marks
            const float rb = rO * 0.66f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27322f), c.x - rb * 0.6f, c.y - rb * 0.8f, juce::Colour (0xff070b0b), c.x + rb * 0.6f, c.y + rb, false));
            g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            for (float rr = rb * 0.2f; rr < rb; rr += 2.2f) { g.setColour (juce::Colours::white.withAlpha (0.025f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), c.x - rb, c.y - rb, juce::Colours::transparentWhite, c.x, c.y, false));
            g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.2f);
            g.setColour ((hot ? col : text (0.12f))); g.drawEllipse (c.x - rb - 1.5f, c.y - rb - 1.5f, rb * 2 + 3, rb * 2 + 3, 0.8f);
            // inset pointer slot
            const juce::Point<float> p1 (c.x + rb * 0.30f * std::sin (av), c.y - rb * 0.30f * std::cos (av)), p2 (c.x + rb * 0.86f * std::sin (av), c.y - rb * 0.86f * std::cos (av));
            g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawLine ({ p1, p2 }, 4.0f);
            g.setColour (col); g.drawLine ({ p1, p2 }, 2.0f);
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 6.0f, 90.0f), 19.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 9.5f);
            g.setColour (active ? col : text (0.1f)); g.drawRoundedRectangle (pill, 9.5f, 1.0f);
            g.setColour (active ? col : text (0.95f)); g.setFont (Theme::font (10.5f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (const KnobDef& d) const
        {
            const float v = param (paramIndex (d)).getCurrentValue();
            if (d.band < 0)
                switch (d.k) { case P::mix: return juce::String ((int) std::lround (v)) + " %"; case P::lookahead: return v < 0.05f ? juce::String ("OFF") : juce::String (v, 1) + " ms";
                               default: return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
            switch (d.k)
            {
                case P::ratio:   return v >= 19.95f ? juce::String ("20:1") : juce::String (v, 1) + ":1";
                case P::attack:  return juce::String (v, v < 10 ? 1 : 0) + " ms";
                case P::release: return v >= 1000 ? juce::String (v / 1000.0f, 2) + " s" : juce::String ((int) std::lround (v)) + " ms";
                case P::gain:    return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
                default:         return juce::String (v, 1) + " dB";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1514).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (mint().withAlpha (0.16f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            const auto col = bandColour (sel);
            auto groupLabel = [&] (int from, int to, const juce::String& t, juce::Colour c)
            {
                auto rr = knobR[(size_t) from].getUnion (knobR[(size_t) to]).withTrimmedTop (-14.0f).expanded (2.0f, 2.0f);
                g.setColour (c.withAlpha (0.05f)); g.fillRoundedRectangle (rr, 10.0f); g.setColour (c.withAlpha (0.22f)); g.drawRoundedRectangle (rr, 10.0f, 1.0f);
                g.setColour (c.withAlpha (0.8f)); g.setFont (Theme::font (8.5f, true)); g.drawText (t, rr.reduced (10.0f, 3.0f).removeFromTop (11.0f), juce::Justification::left);
            };
            groupLabel (0, 6, "BAND " + juce::String (sel + 1) + "  -  " + juce::String (SqMB2::modeName (mb->mode[(size_t) sel].get())).toUpperCase() + " / " + juce::String (SqMB2::placeName (mb->place[(size_t) sel].get())).toUpperCase(), col);
            groupLabel (7, 10, "GLOBAL", mint());
            const auto ds = defs();
            for (size_t i = 0; i < ds.size(); ++i)
            {
                const auto& k = ds[i]; const int pi_ = paramIndex (k);
                const bool bip = (k.band >= 0 && k.k == P::gain) || (k.band < 0 && (k.k == P::input || k.k == P::output));
                drawKnob (g, knobR[i], k.label, knobText (k), norm (pi_), k.band >= 0 ? col : mint(), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, bip);
            }
        }

        // ---- top bar
        void paintTopBar (juce::Graphics& g)
        {
            hits.clear();
            auto bar = topBar();
            g.setColour (text (0.5f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("BANDS", bar.removeFromLeft (44.0f), juce::Justification::centredLeft);
            for (int k = 1; k <= P::maxBands; ++k) { chip (g, bar.removeFromLeft (28.0f).withSizeKeepingCentre (26.0f, 26.0f), juce::String (k), nb() == k, 10 + k, bandColour (k - 1)); bar.removeFromLeft (4.0f); }
            bar.removeFromLeft (14.0f);
            const bool linear = mb->phase.get() == 1;
            chip (g, bar.removeFromLeft (118.0f), "MINIMUM PHASE", ! linear, 1, mint()); bar.removeFromLeft (6.0f);
            chip (g, bar.removeFromLeft (108.0f), "LINEAR PHASE", linear, 2, mint()); bar.removeFromLeft (14.0f);
            chip (g, bar.removeFromLeft (widthOf ("DETECT: " + P::detectorNames()[juce::jlimit (0, 4, mb->detector.get())].toUpperCase()) + 24.0f),
                  "DETECT: " + P::detectorNames()[juce::jlimit (0, 4, mb->detector.get())].toUpperCase(), true, 3, juce::Colour (0xff37d6ff));
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("LATENCY " + juce::String (mb->getLatencySeconds() * 1000.0, 1) + " MS", bar.removeFromRight (120.0f), juce::Justification::right);
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Apex Multiband 2 setting");
            auto& m = *mb;
            if (id >= 11 && id <= 16) { m.numBands.setValue (id - 10, um()); sel = juce::jmin (sel, id - 11); changed(); return; }
            if (id >= 100 && id < 104) { m.mode[(size_t) sel].setValue (id - 100, um()); changed(); return; }
            if (id >= 110 && id < 115) { m.place[(size_t) sel].setValue (id - 110, um()); changed(); return; }
            switch (id)
            {
                case 1: m.phase.setValue (0, um()); changed(); break;
                case 2: m.phase.setValue (1, um()); changed(); break;
                case 3:
                {
                    juce::PopupMenu menu; const auto names = P::detectorNames();
                    for (int i = 0; i < names.size(); ++i) menu.addItem (i + 1, names[i], true, i == m.detector.get());
                    menu.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<ApexMB2Viz> (this)] (int r) { if (r > 0 && sp != nullptr) { mb->detector.setValue (r - 1, um()); changed(); } });
                    break;
                }
                case 120: m.keyOf[(size_t) sel].setValue (m.keyOf[(size_t) sel].get() ? 0 : 1, um()); changed(); break;
                case 121: m.solo[(size_t) sel].setValue (! m.solo[(size_t) sel].get(), um()); changed(); break;
                case 122: m.bypass[(size_t) sel].setValue (! m.bypass[(size_t) sel].get(), um()); changed(); break;
                default: break;
            }
        }

        // ---- mouse
        int dragSeam = -1, hoverSeam = -1;
        int seamAt (juce::Point<float> pt) const
        {
            const auto p = plot(); if (! p.expanded (0, 4).contains (pt)) return -1;
            const auto x = SqMB2::sanitise (mb->params(), 48000.0);
            for (int i = 0; i < nb() - 1; ++i) if (std::abs (pt.x - xOf (p, x[(size_t) i])) < 6.0f) return i;
            return -1;
        }
        int bandAtX (float px) const
        {
            const auto p = plot(); const double hz = hzOf (p, px); const auto x = SqMB2::sanitise (mb->params(), 48000.0);
            int k = 0; while (k < nb() - 1 && hz > x[(size_t) k]) ++k; return k;
        }
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (4.0f, 0).contains (p)) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), s = seamAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : s >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || s != hoverSeam) { hoverKnob = k; hoverSeam = s; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverSeam = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (mb == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            for (int k = 0; k < nb(); ++k) if (tabR (k).contains (e.position)) { sel = k; changed(); return; }
            if (const int s = seamAt (e.position); s >= 0)
            {
                dragSeam = s; um()->beginNewTransaction ("Apex Multiband 2 crossover"); param (P::x1 + s).parameterChangeGestureBegin(); return;
            }
            if (plot().contains (e.position)) { sel = bandAtX (e.position.x); changed(); return; }
            const int k = knobAt (e.position);
            if (k >= 0)
            {
                const auto d = defs()[(size_t) k]; dragKnob = k; dragStart = e.position; dragStartNorm = norm (paramIndex (d));
                um()->beginNewTransaction (juce::String ("Apex Multiband 2 ") + d.label); param (paramIndex (d)).parameterChangeGestureBegin(); repaint();
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragSeam >= 0)
            {
                auto& p = param (P::x1 + dragSeam); p.setParameter ((float) juce::jlimit (20.0, 20000.0, hzOf (plot(), e.position.x)), juce::sendNotification); changed(); return;
            }
            if (dragKnob < 0) return;
            setNorm (paramIndex (defs()[(size_t) dragKnob]), dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragSeam >= 0) param (P::x1 + dragSeam).parameterChangeGestureEnd();
            if (dragKnob >= 0) param (paramIndex (defs()[(size_t) dragKnob])).parameterChangeGestureEnd();
            dragKnob = -1; dragSeam = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = paramIndex (defs()[(size_t) k]);
            um()->beginNewTransaction ("Apex Multiband 2 reset"); param (pi_).setParameter (mb->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Apex Multiband 2 wheel");
            const int pi_ = paramIndex (defs()[(size_t) k]); setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
