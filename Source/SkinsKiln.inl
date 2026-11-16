// Included inside namespace Skins (PluginSkins.h): the Kiln face (1.14 Studio Line, phase H).
//
// Squelchmoth's own "inside the kiln" design on the Higgsfield plate pa_kiln:
//  - top bar: bands 1-6, oversampling, linear-phase oversampling, auto gain, latency
//  - the furnace: the output spectrum over the band territories; each band glows with the heat it is putting in right now
//    (its saturation activity), crossovers are seams you drag, the tabs name each band's model
//  - the right column: the selected band's model (arrows + menu + what it is), heat columns for every band, and the
//    modulation: LFO and envelope traces and the four matrix slots (source, target, amount)
//  - the deck: cast-iron knobs whose core glows hotter as they turn up, drawn here

    struct KilnViz : public Viz, private juce::Timer
    {
        using P = SqKilnPlugin;
        KilnViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), kn (dynamic_cast<SqKilnPlugin*> (&p)), fft (12), window ((size_t) fftN, juce::dsp::WindowingFunction<float>::hann)
        { setInterceptsMouseClicks (true, false); spec.fill (-120.0f); lfoHist.fill (0); envHist.fill (0); startTimerHz (30); }
        SqKilnPlugin* kn;
        std::function<void()> onChange;
        int sel = 0;

        static juce::Colour night()  { return juce::Colour (0xff0b0806); }
        static juce::Colour ember()  { return juce::Colour (0xffff7a2e); }
        static juce::Colour gold()   { return juce::Colour (0xffffd27a); }
        static juce::Colour crimson(){ return juce::Colour (0xffd23a2a); }
        static juce::Colour ash()    { return juce::Colour (0xff8c8279); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfffff3e6).withAlpha (a); }
        static juce::Colour heat (float t) { t = juce::jlimit (0.0f, 1.0f, t); return t < 0.5f ? crimson().interpolatedWith (ember(), t * 2) : ember().interpolatedWith (gold(), (t - 0.5f) * 2); }

        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (196.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (204.0f); r.removeFromRight (meterW() + 10.0f); return r; }
        // the numbers: IN / OUT meters on the right (StudioUi, shared by the Studio Line)
        StudioUi::StudioMeter meter;
        static float meterW() { return StudioUi::StudioMeter::width (false); }
        juce::Rectangle<float> meterR() const { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (204.0f); return r.removeFromRight (meterW()); }
        void paintMeter (juce::Graphics& g) { meter.update (kn->levelIn.load(), kn->levelOut.load(), (float) 0.0f); meter.draw (g, meterR(), gold(), false); }
        juce::Rectangle<float> furnace() const { auto r = middle(); return r.removeFromLeft (r.getWidth() - 372.0f); }
        juce::Rectangle<float> side() const    { auto r = middle(); return r.removeFromRight (360.0f); }
        juce::Rectangle<float> plot() const    { return furnace().reduced (14.0f, 0).withTrimmedTop (46.0f).withTrimmedBottom (24.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.70f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (ember().withAlpha (0.06f), 0, r.getY(), ember().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (ember().withAlpha (0.18f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        static constexpr int fftN = 4096, pts = 256;
        juce::dsp::FFT fft; juce::dsp::WindowingFunction<float> window; std::array<float, fftN * 2> work {}; std::array<float, pts> spec {};
        std::array<float, 160> lfoHist {}, envHist {}; int histPos = 0;
        void timerCallback() override
        {
            if (kn != nullptr)
            {
                const int w = kn->scopeW.load();
                for (int i = 0; i < fftN; ++i) work[(size_t) i] = kn->scope[(size_t) ((w - fftN + i) & (P::scopeLen - 1))];
                std::fill (work.begin() + fftN, work.end(), 0.0f);
                window.multiplyWithWindowingTable (work.data(), (size_t) fftN); fft.performFrequencyOnlyForwardTransform (work.data(), true);
                const double binHz = juce::jmax (8000.0, kn->sampleRate()) / fftN;
                for (int i = 0; i < pts; ++i)
                {
                    const double f0 = 20.0 * std::pow (1000.0, i / (double) pts), f1 = 20.0 * std::pow (1000.0, (i + 1) / (double) pts);
                    const int b0 = (int) (f0 / binHz), b1 = std::max (b0 + 1, (int) (f1 / binHz));
                    float m = 0; for (int b = b0; b < std::min (b1, fftN / 2); ++b) m = std::max (m, work[(size_t) b]);
                    const float v = juce::Decibels::gainToDecibels (m * 4.0f / fftN, -120.0f) + 4.5f * (float) std::log2 (std::max (20.0, f0) / 1000.0);
                    spec[(size_t) i] = v > spec[(size_t) i] ? v : spec[(size_t) i] * 0.8f + v * 0.2f;
                }
                lfoHist[(size_t) histPos] = kn->lfoNow.load(); envHist[(size_t) histPos] = kn->envNow.load(); histPos = (histPos + 1) % (int) lfoHist.size();
            }
            repaint();
        }
        static float xOf (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0)) * p.getWidth(); }
        static double hzOf (juce::Rectangle<float> p, float x) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
        int nb() const { return juce::jlimit (1, P::maxBands, kn->numBands.get()); }
        std::array<double, 5> xs() const { SqMB2::Params sp; sp.numBands = nb(); for (int i = 0; i < 5; ++i) sp.xover[(size_t) i] = kn->bank[P::x1 + i]; return SqMB2::sanitise (sp, 48000.0); }
        float bandL (int k) const { const auto p = plot(); return k == 0 ? p.getX() : xOf (p, xs()[(size_t) k - 1]); }
        float bandR (int k) const { const auto p = plot(); return k == nb() - 1 ? p.getRight() : xOf (p, xs()[(size_t) k]); }

        void paint (juce::Graphics& g) override
        {
            if (kn == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("kiln");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.85f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.25f), a.getCentreX(), a.getY(), night().withAlpha (0.86f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (ember().withAlpha (0.24f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            sel = juce::jlimit (0, nb() - 1, sel);
            hits.clear();
            paintTopBar (g); paintFurnace (g); paintSide (g); paintDeck (g); paintMeter (g);
        }

        // ---- the furnace
        void paintFurnace (juce::Graphics& g)
        {
            pane (g, furnace(), "");
            const auto p = plot();
            g.setFont (Theme::font (8.5f, true));
            for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
            {
                const float x = xOf (p, hz);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.32f)); g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), (int) x - 16, (int) p.getBottom() + 4, 32, 12, juce::Justification::centred);
            }
            // each band's heat: a glow rising from the floor, as tall and bright as its saturation activity
            for (int k = 0; k < nb(); ++k)
            {
                const float l = bandL (k), r = bandR (k), act = juce::jlimit (0.0f, 1.0f, kn->activity[(size_t) k].load());
                const bool on = ! kn->bypass[(size_t) k].get();
                auto area = juce::Rectangle<float>::leftTopRightBottom (l, p.getY(), r, p.getBottom());
                g.setGradientFill (juce::ColourGradient (heat (act).withAlpha (on ? 0.06f + 0.30f * act : 0.03f), 0, p.getBottom(), heat (act).withAlpha (0.0f), 0, p.getBottom() - p.getHeight() * (0.20f + 0.60f * act), false));
                g.fillRect (area);
                if (k == sel) { g.setColour (gold().withAlpha (0.04f)); g.fillRect (area); g.setColour (gold().withAlpha (0.35f)); g.drawRect (area, 1.0f); }
            }
            // spectrum
            juce::Path sp; sp.startNewSubPath (p.getX(), p.getBottom());
            for (int i = 0; i < pts; ++i) sp.lineTo (p.getX() + p.getWidth() * (i + 0.5f) / pts, p.getY() + juce::jlimit (0.0f, 1.0f, (6.0f - spec[(size_t) i]) / 84.0f) * p.getHeight());
            juce::Path line (sp); sp.lineTo (p.getRight(), p.getBottom()); sp.closeSubPath();
            g.setGradientFill (juce::ColourGradient (text (0.16f), 0, p.getY(), text (0.02f), 0, p.getBottom(), false)); g.fillPath (sp);
            g.setColour (gold().withAlpha (0.8f)); g.strokePath (line, juce::PathStrokeType (1.1f));
            // seams
            for (int i = 0; i < nb() - 1; ++i)
            {
                const float x = xOf (p, xs()[(size_t) i]); const bool hot = i == hoverSeam || i == dragSeam;
                g.setColour (gold().withAlpha (hot ? 1.0f : 0.6f)); g.fillRect (x - (hot ? 1.5f : 0.75f), p.getY(), hot ? 3.0f : 1.5f, p.getHeight());
                auto tag = juce::Rectangle<float> (x - 26, p.getBottom() - 20, 52, 16);
                g.setColour (night().withAlpha (0.9f)); g.fillRoundedRectangle (tag, 8.0f); g.setColour (gold().withAlpha (0.7f)); g.drawRoundedRectangle (tag, 8.0f, 1.0f);
                const double hz = xs()[(size_t) i]; g.setColour (text (0.95f)); g.drawText (hz >= 1000 ? juce::String (hz / 1000.0, hz < 10000 ? 2 : 1) + "k" : juce::String ((int) hz), tag, juce::Justification::centred);
            }
            // tabs: band number, model, the drive actually in use
            for (int k = 0; k < nb(); ++k)
            {
                const auto t = tabR (k); const bool s = k == sel; const float act = kn->activity[(size_t) k].load();
                g.setColour ((s ? ember().withAlpha (0.35f) : night().withAlpha (0.75f))); g.fillRoundedRectangle (t, 9.0f);
                g.setColour (s ? gold() : ember().withAlpha (0.45f)); g.drawRoundedRectangle (t, 9.0f, 1.0f);
                g.setColour (heat (act)); g.fillEllipse (t.getX() + 7, t.getCentreY() - 3.5f, 7, 7);
                g.setColour (text (s ? 1.0f : 0.8f)); g.setFont (Theme::font (9.0f, true));
                juce::String lab = juce::String (k + 1);
                if (t.getWidth() > 90) lab << "  " << juce::String (Cinder::modelName (kn->model[(size_t) k].get())).toUpperCase();
                if (kn->solo[(size_t) k].get()) lab << "  S"; if (kn->bypass[(size_t) k].get()) lab << "  OFF";
                g.drawText (lab, t.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
            }
        }
        juce::Rectangle<float> tabR (int k) const { return { bandL (k) + 2.0f, furnace().getY() + 14.0f, std::max (20.0f, bandR (k) - bandL (k) - 4.0f), 22.0f }; }

        // ---- right column
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 9.5f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c = ember())
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (9.5f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        juce::Rectangle<float> slotAmountR (int s) const { return slotRows[(size_t) s].withTrimmedLeft (slotRows[(size_t) s].getWidth() * 0.62f); }
        std::array<juce::Rectangle<float>, P::numSlots> slotRows {};
        void paintSide (juce::Graphics& g)
        {
            const auto r = side(); pane (g, r, "BAND " + juce::String (sel + 1));
            auto in = r.reduced (14.0f, 10.0f); in.removeFromTop (16.0f);
            // model selector
            {
                auto row = in.removeFromTop (28.0f);
                auto prev = row.removeFromLeft (26.0f); auto next = row.removeFromRight (26.0f); row.reduce (6.0f, 0);
                for (auto [rr, glyph, id] : { std::tuple<juce::Rectangle<float>, const char*, int> { prev, "\xe2\x80\xb9", 1 }, { next, "\xe2\x80\xba", 2 } })
                { g.setColour (text (0.06f)); g.fillEllipse (rr.withSizeKeepingCentre (26, 26)); g.setColour (ember().withAlpha (0.4f)); g.drawEllipse (rr.withSizeKeepingCentre (26, 26), 1.0f);
                  g.setColour (text (0.9f)); g.setFont (Theme::font (16.0f, true)); g.drawText (juce::String (juce::CharPointer_UTF8 (glyph)), rr.translated (0, -1), juce::Justification::centred); hits.push_back ({ rr, id }); }
                g.setGradientFill (juce::ColourGradient (crimson().withAlpha (0.55f), row.getX(), row.getY(), ember().withAlpha (0.45f), row.getRight(), row.getBottom(), false)); g.fillRoundedRectangle (row, 14.0f);
                g.setColour (gold()); g.drawRoundedRectangle (row, 14.0f, 1.0f);
                g.setColour (juce::Colours::white); g.setFont (Theme::font (12.0f, true)); g.drawText (juce::String (Cinder::modelName (kn->model[(size_t) sel].get())).toUpperCase(), row, juce::Justification::centred);
                hits.push_back ({ row, 3 });
            }
            g.setColour (text (0.6f)); g.setFont (Theme::font (9.5f)); g.drawFittedText (Cinder::model (kn->model[(size_t) sel].get()).archetype, in.removeFromTop (30.0f).toNearestInt(), juce::Justification::centred, 2);
            {
                auto row = in.removeFromTop (22.0f); const float w = row.getWidth() / 2.0f;
                chip (g, row.removeFromLeft (w - 3.0f), "SOLO", kn->solo[(size_t) sel].get(), 4, gold()); row.removeFromLeft (6.0f);
                chip (g, row, "BYPASS", kn->bypass[(size_t) sel].get(), 5, crimson());
            }
            in.removeFromTop (8.0f);
            // heat columns for every band
            {
                auto hc = in.removeFromTop (54.0f); const float w = hc.getWidth() / P::maxBands;
                for (int k = 0; k < P::maxBands; ++k)
                {
                    auto col = juce::Rectangle<float> (hc.getX() + w * k + 3, hc.getY(), w - 6, hc.getHeight() - 12);
                    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (col, 3.0f);
                    if (k < nb())
                    {
                        const float act = juce::jlimit (0.0f, 1.0f, kn->activity[(size_t) k].load());
                        auto fillR = col.withTrimmedTop (col.getHeight() * (1.0f - act));
                        g.setGradientFill (juce::ColourGradient (gold(), 0, col.getY(), crimson(), 0, col.getBottom(), false)); g.fillRoundedRectangle (fillR, 3.0f);
                    }
                    g.setColour (text (k == sel ? 1.0f : 0.4f)); g.setFont (Theme::font (8.5f, true)); g.drawText (juce::String (k + 1), (int) col.getX(), (int) col.getBottom() + 1, (int) col.getWidth(), 11, juce::Justification::centred);
                }
            }
            in.removeFromTop (6.0f);
            // modulators
            {
                auto mr = in.removeFromTop (44.0f);
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (mr, 5.0f);
                juce::Path lp, ep; const int N = (int) lfoHist.size();
                for (int i = 0; i < N; ++i)
                {
                    const int idx = (histPos + i) % N; const float x = mr.getX() + mr.getWidth() * i / (N - 1.0f);
                    const float yl = mr.getCentreY() - lfoHist[(size_t) idx] * mr.getHeight() * 0.4f, ye = mr.getBottom() - 3 - envHist[(size_t) idx] * (mr.getHeight() - 6);
                    if (i == 0) { lp.startNewSubPath (x, yl); ep.startNewSubPath (x, ye); } else { lp.lineTo (x, yl); ep.lineTo (x, ye); }
                }
                g.setColour (gold()); g.strokePath (lp, juce::PathStrokeType (1.3f)); g.setColour (crimson().brighter (0.3f)); g.strokePath (ep, juce::PathStrokeType (1.3f));
                g.setFont (Theme::font (8.0f, true)); g.setColour (gold()); g.drawText ("LFO " + P::lfoNames()[juce::jlimit (0, 3, kn->lfoShape.get())].toUpperCase(), mr.reduced (5.0f, 3.0f), juce::Justification::topLeft);
                g.setColour (crimson().brighter (0.3f)); g.drawText ("ENVELOPE", mr.reduced (5.0f, 3.0f), juce::Justification::topRight);
                hits.push_back ({ mr.removeFromLeft (80.0f).removeFromTop (14.0f), 6 });
            }
            in.removeFromTop (6.0f);
            // the matrix
            g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("MODULATION  -  source, target, amount (drag)", in.removeFromTop (12.0f), juce::Justification::left);
            for (int s = 0; s < P::numSlots; ++s)
            {
                auto row = in.removeFromTop (24.0f); in.removeFromTop (3.0f); slotRows[(size_t) s] = row;
                const int src = kn->slotSource[(size_t) s].get(), dst = kn->slotTarget[(size_t) s].get();
                auto srcR = row.removeFromLeft (70.0f); row.removeFromLeft (4.0f);
                auto dstR = row.removeFromLeft (row.getWidth() * 0.58f); row.removeFromLeft (4.0f);
                chip (g, srcR, P::sourceNames()[juce::jlimit (0, 2, src)].toUpperCase(), src != 0, 20 + s, src == 2 ? crimson() : gold());
                chip (g, dstR, SqKiln::targetName (dst).toUpperCase(), dst != 0, 30 + s, ember());
                // amount: a bipolar bar
                const float amt = kn->bank[P::slot1 + s];
                g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (row, 6.0f);
                const float cx = row.getCentreX(), x2 = cx + amt * row.getWidth() * 0.5f;
                g.setColour ((amt >= 0 ? gold() : crimson()).withAlpha (src != 0 && dst != 0 ? 0.9f : 0.3f)); g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, x2), row.getY() + 6, std::max (cx, x2), row.getBottom() - 6), 3.0f);
                g.setColour (text (0.25f)); g.drawVerticalLine ((int) cx, row.getY() + 3, row.getBottom() - 3);
                g.setColour (text (0.85f)); g.setFont (Theme::font (8.5f, true));
                const float scale = P::slotScale (dst);
                g.drawText (scale > 1 ? (amt >= 0 ? "+" : "") + juce::String (amt * scale, 1) + " dB" : (amt >= 0 ? "+" : "") + juce::String ((int) std::lround (amt * 100)) + " %", row, juce::Justification::centred);
            }
        }

        // ---- top bar
        void paintTopBar (juce::Graphics& g)
        {
            auto bar = topBar();
            g.setColour (text (0.5f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("BANDS", bar.removeFromLeft (44.0f), juce::Justification::centredLeft);
            for (int k = 1; k <= P::maxBands; ++k) { chip (g, bar.removeFromLeft (28.0f).withSizeKeepingCentre (26.0f, 26.0f), juce::String (k), nb() == k, 10 + k); bar.removeFromLeft (4.0f); }
            bar.removeFromLeft (14.0f);
            const auto os = "OVERSAMPLING " + P::osNames()[juce::jlimit (0, 4, kn->oversample.get())];
            chip (g, bar.removeFromLeft (widthOf (os) + 22.0f), os, kn->oversample.get() > 0, 17, gold()); bar.removeFromLeft (6.0f);
            chip (g, bar.removeFromLeft (110.0f), "LINEAR PHASE OS", kn->linearOs.get(), 18, gold()); bar.removeFromLeft (6.0f);
            chip (g, bar.removeFromLeft (90.0f), "AUTO GAIN", kn->autoGain.get(), 19, gold());
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true)); g.drawText ("LATENCY " + juce::String (kn->getLatencySeconds() * 1000.0, 1) + " MS", bar, juce::Justification::right);
        }
        juce::UndoManager* um() const { return &kn->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        void menu (const juce::StringArray& names, int cur, std::function<void (int)> set)
        {
            juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<KilnViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Kiln setting"); auto& k = *kn; const int nm = Cinder::numModels;
            if (id >= 11 && id <= 16) { k.numBands.setValue (id - 10, um()); sel = juce::jmin (sel, id - 11); changed(); return; }
            if (id >= 20 && id < 24) { const int s = id - 20; k.slotSource[(size_t) s].setValue ((k.slotSource[(size_t) s].get() + 1) % 3, um()); changed(); return; }
            if (id >= 30 && id < 34)
            {
                const int s = id - 30; juce::StringArray names; for (int t = 0; t <= 13; ++t) names.add (SqKiln::targetName (t));
                menu (names, k.slotTarget[(size_t) s].get(), [this, s] (int t) { kn->slotTarget[(size_t) s].setValue (t, um()); }); return;
            }
            switch (id)
            {
                case 1: k.model[(size_t) sel].setValue ((k.model[(size_t) sel].get() + nm - 1) % nm, um()); changed(); break;
                case 2: k.model[(size_t) sel].setValue ((k.model[(size_t) sel].get() + 1) % nm, um()); changed(); break;
                case 3:
                {
                    juce::PopupMenu m; for (int i = 0; i < nm; ++i) m.addItem (i + 1, juce::String (Cinder::modelName (i)) + "   -   " + Cinder::model (i).archetype, true, i == k.model[(size_t) sel].get());
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, sp = SafePointer<KilnViz> (this)] (int r) { if (r > 0 && sp != nullptr) { kn->model[(size_t) sel].setValue (r - 1, um()); changed(); } });
                    break;
                }
                case 4: k.solo[(size_t) sel].setValue (! k.solo[(size_t) sel].get(), um()); changed(); break;
                case 5: k.bypass[(size_t) sel].setValue (! k.bypass[(size_t) sel].get(), um()); changed(); break;
                case 6: k.lfoShape.setValue ((k.lfoShape.get() + 1) % 4, um()); changed(); break;
                case 17: menu (P::osNames(), k.oversample.get(), [this] (int i) { kn->oversample.setValue (i, um()); }); break;
                case 18: k.linearOs.setValue (! k.linearOs.get(), um()); changed(); break;
                case 19: k.autoGain.setValue (! k.autoGain.get(), um()); changed(); break;
                default: break;
            }
        }

        // ---- the deck: cast iron with an ember core
        struct KnobDef { int band; int k; const char* label; };
        std::vector<KnobDef> defs() const
        {
            return { { sel, P::drive, "DRIVE" }, { sel, P::bias, "BIAS" }, { sel, P::dynamics, "DYNAMICS" }, { sel, P::bandMix, "BAND MIX" }, { sel, P::bandOut, "BAND OUT" },
                     { -1, P::lfoRate, "LFO RATE" }, { -1, P::envAttack, "ENV ATTACK" }, { -1, P::envRelease, "ENV RELEASE" },
                     { -1, P::input, "INPUT" }, { -1, P::mix, "MIX" }, { -1, P::output, "OUTPUT" } };
        }
        int paramIndex (const KnobDef& d) const { return d.band < 0 ? d.k : kn->paramOf (d.band, d.k); }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1, dragSlot = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *kn->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        void layoutKnobs()
        {
            knobR.clear(); auto d = deck().reduced (14.0f, 12.0f); d.removeFromTop (14.0f);
            const int n = (int) defs().size(); const float w = (d.getWidth() - 24.0f) / n;
            float x = d.getX();
            for (int i = 0; i < n; ++i) { if (i == 5 || i == 8) x += 12.0f; knobR.push_back ({ x, d.getY(), w, d.getHeight() }); x += w; }
        }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, bool hotK, bool active, bool bipolar)
        {
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (24.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 8.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text (hotK ? 0.95f : 0.55f)); g.setFont (Theme::font (8.5f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            // ember arc: crimson to gold along its length
            const float ar = rO * 0.95f; const float as = bipolar ? (a0 + a1) * 0.5f : a0;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (as, av), std::max (as, av), true);
            g.setColour (ember().withAlpha (0.28f)); g.strokePath (arc, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (crimson(), c.x - ar, c.y + ar, gold(), c.x + ar, c.y - ar, false));
            g.strokePath (arc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // cast-iron body, slightly faceted
            const float rb = rO * 0.74f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3530), c.x - rb, c.y - rb, juce::Colour (0xff0e0b09), c.x + rb, c.y + rb, false)); g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            for (int t = 0; t < 24; ++t) { const float a = juce::MathConstants<float>::twoPi * t / 24.0f + av; g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawLine (c.x + rb * 0.88f * std::sin (a), c.y - rb * 0.88f * std::cos (a), c.x + rb * std::sin (a), c.y - rb * std::cos (a), 1.4f); }
            g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.2f);
            // the ember core: glows hotter as the knob turns up
            const float rc = rb * 0.55f, t = bipolar ? std::abs (v - 0.5f) * 2.0f : v;
            g.setGradientFill (juce::ColourGradient (heat (t).withAlpha (0.25f + 0.70f * t), c.x, c.y, juce::Colour (0xff120a06), c.x, c.y + rc, true)); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.0f);
            // pointer
            const juce::Point<float> p1 (c.x + rc * std::sin (av), c.y - rc * std::cos (av)), p2 (c.x + rb * 0.92f * std::sin (av), c.y - rb * 0.92f * std::cos (av));
            g.setColour (gold().withAlpha (0.35f)); g.drawLine ({ p1, p2 }, 5.0f); g.setColour (gold()); g.drawLine ({ p1, p2 }, 2.0f);
            if (hotK) { g.setColour (gold().withAlpha (0.7f)); g.drawEllipse (c.x - rb - 2, c.y - rb - 2, rb * 2 + 4, rb * 2 + 4, 1.0f); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 4.0f, 86.0f), 19.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 9.5f);
            g.setColour (active ? gold() : text (0.1f)); g.drawRoundedRectangle (pill, 9.5f, 1.0f);
            g.setColour (active ? gold() : text (0.95f)); g.setFont (Theme::font (10.5f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (const KnobDef& d) const
        {
            const float v = param (paramIndex (d)).getCurrentValue();
            if (d.band >= 0)
                switch (d.k) { case P::drive: case P::bandOut: return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; case P::bias: return (v > 0 ? "+" : "") + juce::String (v, 2); default: return juce::String ((int) std::lround (v)) + " %"; }
            switch (d.k)
            {
                case P::lfoRate: return juce::String (v, v < 1 ? 2 : 1) + " Hz";
                case P::envAttack: case P::envRelease: return juce::String (v, v < 10 ? 1 : 0) + " ms";
                case P::mix: return juce::String ((int) std::lround (v)) + " %";
                default: return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a120d).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (ember().withAlpha (0.20f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            auto group = [&] (int from, int to, const juce::String& t)
            {
                auto rr = knobR[(size_t) from].getUnion (knobR[(size_t) to]).withTrimmedTop (-14.0f).expanded (2.0f, 2.0f);
                g.setColour (ember().withAlpha (0.05f)); g.fillRoundedRectangle (rr, 10.0f); g.setColour (ember().withAlpha (0.18f)); g.drawRoundedRectangle (rr, 10.0f, 1.0f);
                g.setColour (text (0.5f)); g.setFont (Theme::font (8.5f, true)); g.drawText (t, rr.reduced (10.0f, 3.0f).removeFromTop (11.0f), juce::Justification::left);
            };
            group (0, 4, "BAND " + juce::String (sel + 1) + "  -  " + juce::String (Cinder::modelName (kn->model[(size_t) sel].get())).toUpperCase());
            group (5, 7, "MODULATORS"); group (8, 10, "GLOBAL");
            const auto ds = defs();
            for (size_t i = 0; i < ds.size(); ++i)
            {
                const auto& k = ds[i]; const int pi_ = paramIndex (k);
                const bool bip = (k.band >= 0 && (k.k == P::bias || k.k == P::bandOut)) || (k.band < 0 && (k.k == P::input || k.k == P::output));
                drawKnob (g, knobR[i], k.label, knobText (k), norm (pi_), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, bip);
                StudioUi::knobScale (g, knobR[i], kn->bank.specs[(size_t) pi_], text (1.0f));
            }
        }

        // ---- mouse
        int dragSeam = -1, hoverSeam = -1;
        int seamAt (juce::Point<float> pt) const { const auto p = plot(); if (! p.contains (pt)) return -1; for (int i = 0; i < nb() - 1; ++i) if (std::abs (pt.x - xOf (p, xs()[(size_t) i])) < 6.0f) return i; return -1; }
        int bandAtX (float x) const { for (int k = 0; k < nb(); ++k) if (x >= bandL (k) && x <= bandR (k)) return k; return 0; }
        int knobAt (juce::Point<float> p) const { for (size_t i = 0; i < knobR.size(); ++i) if (knobR[i].reduced (4.0f, 0).contains (p)) return (int) i; return -1; }
        int slotAt (juce::Point<float> p) const { for (int s = 0; s < P::numSlots; ++s) if (slotAmountR (s).contains (p)) return s; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), s = seamAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : (s >= 0 || slotAt (e.position) >= 0) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || s != hoverSeam) { hoverKnob = k; hoverSeam = s; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverSeam = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (kn == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            for (int k = 0; k < nb(); ++k) if (tabR (k).contains (e.position)) { sel = k; changed(); return; }
            if (const int s = slotAt (e.position); s >= 0) { dragSlot = s; um()->beginNewTransaction ("Kiln modulation amount"); param (P::slot1 + s).parameterChangeGestureBegin(); mouseDrag (e); return; }
            if (const int s = seamAt (e.position); s >= 0) { dragSeam = s; um()->beginNewTransaction ("Kiln crossover"); param (P::x1 + s).parameterChangeGestureBegin(); return; }
            if (plot().contains (e.position)) { sel = bandAtX (e.position.x); changed(); return; }
            const int k = knobAt (e.position);
            if (k >= 0) { const auto d = defs()[(size_t) k]; dragKnob = k; dragStart = e.position; dragStartNorm = norm (paramIndex (d)); um()->beginNewTransaction (juce::String ("Kiln ") + d.label); param (paramIndex (d)).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragSlot >= 0) { const auto r = slotAmountR (dragSlot); auto& p = param (P::slot1 + dragSlot); p.setParameter (juce::jlimit (-1.0f, 1.0f, (e.position.x - r.getCentreX()) / (r.getWidth() * 0.5f)), juce::sendNotification); changed(); return; }
            if (dragSeam >= 0) { auto& p = param (P::x1 + dragSeam); p.setParameter ((float) juce::jlimit (20.0, 20000.0, hzOf (plot(), e.position.x)), juce::sendNotification); changed(); return; }
            if (dragKnob < 0) return;
            setNorm (paramIndex (defs()[(size_t) dragKnob]), dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragSlot >= 0) param (P::slot1 + dragSlot).parameterChangeGestureEnd();
            if (dragSeam >= 0) param (P::x1 + dragSeam).parameterChangeGestureEnd();
            if (dragKnob >= 0) param (paramIndex (defs()[(size_t) dragKnob])).parameterChangeGestureEnd();
            dragKnob = -1; dragSeam = -1; dragSlot = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = paramIndex (defs()[(size_t) k]); um()->beginNewTransaction ("Kiln reset"); param (pi_).setParameter (kn->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Kiln wheel"); const int pi_ = paramIndex (defs()[(size_t) k]); setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
