// Included inside namespace Skins (PluginSkins.h): the Fissure Filter face (1.14 Studio Line, phase J).
//
// Squelchmoth's own "light through a crack in obsidian" design on the Higgsfield plate pa_fissure:
//  - top bar: routing, the two LFOs (shape and sync), latency (none)
//  - the response: the output spectrum, each filter's real curve in its colour and the combined curve in white; every filter
//    is a node you drag (sideways = cutoff, up / down = resonance, or gain for a peak); when a modulator moves a cutoff,
//    the swept range is drawn as a lit band and the node shows where the cutoff is right now
//  - the right column: the selected filter (on, type, slope) and the eight-slot modulation matrix, with the LFO and
//    envelope traces
//  - IN / OUT meters with numbers; the deck's knobs carry their ranges

    struct FissureViz : public Viz, private juce::Timer
    {
        using P = SqFissureFilterPlugin;
        FissureViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ff (dynamic_cast<SqFissureFilterPlugin*> (&p)), fft (12), window ((size_t) fftN, juce::dsp::WindowingFunction<float>::hann)
        { setInterceptsMouseClicks (true, false); spec.fill (-120.0f); lfoHist.fill (0); lfo2Hist.fill (0); envHist.fill (0); startTimerHz (30); }
        SqFissureFilterPlugin* ff;
        std::function<void()> onChange;
        int sel = 0;

        static juce::Colour night()  { return juce::Colour (0xff07060c); }
        static juce::Colour violet() { return juce::Colour (0xffa86bff); }
        static juce::Colour cyan()   { return juce::Colour (0xff3ee8ff); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfff2eeff).withAlpha (a); }
        static juce::Colour slotColour (int k) { static const juce::uint32 c[4] = { 0xff3ee8ff, 0xffa86bff, 0xffff5fb0, 0xffffc24a }; return juce::Colour (c[juce::jlimit (0, 3, k)]); }

        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (190.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (198.0f); r.removeFromRight (meterW() + 10.0f); return r; }
        StudioUi::StudioMeter meter;
        static float meterW() { return StudioUi::StudioMeter::width (false); }
        juce::Rectangle<float> meterR() const { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (198.0f); return r.removeFromRight (meterW()); }
        juce::Rectangle<float> respPane() const { auto r = middle(); return r.removeFromLeft (r.getWidth() - 352.0f); }
        juce::Rectangle<float> side() const    { auto r = middle(); return r.removeFromRight (340.0f); }
        juce::Rectangle<float> plot() const    { return respPane().reduced (14.0f, 0).withTrimmedTop (44.0f).withTrimmedBottom (24.0f).withTrimmedRight (26.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.70f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (violet().withAlpha (0.07f), 0, r.getY(), violet().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (violet().withAlpha (0.18f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        static constexpr int fftN = 4096, pts = 256;
        juce::dsp::FFT fft; juce::dsp::WindowingFunction<float> window; std::array<float, fftN * 2> work {}; std::array<float, pts> spec {};
        std::array<float, 160> lfoHist {}, lfo2Hist {}, envHist {}; int histPos = 0;
        void timerCallback() override
        {
            if (ff != nullptr)
            {
                const int w = ff->scopeW.load();
                for (int i = 0; i < fftN; ++i) work[(size_t) i] = ff->scope[(size_t) ((w - fftN + i) & (P::scopeLen - 1))];
                std::fill (work.begin() + fftN, work.end(), 0.0f);
                window.multiplyWithWindowingTable (work.data(), (size_t) fftN); fft.performFrequencyOnlyForwardTransform (work.data(), true);
                const double binHz = juce::jmax (8000.0, ff->sampleRate()) / fftN;
                for (int i = 0; i < pts; ++i)
                {
                    const double f0 = 20.0 * std::pow (1000.0, i / (double) pts), f1 = 20.0 * std::pow (1000.0, (i + 1) / (double) pts);
                    const int b0 = (int) (f0 / binHz), b1 = std::max (b0 + 1, (int) (f1 / binHz));
                    float m = 0; for (int b = b0; b < std::min (b1, fftN / 2); ++b) m = std::max (m, work[(size_t) b]);
                    const float v = juce::Decibels::gainToDecibels (m * 4.0f / fftN, -120.0f) + 4.5f * (float) std::log2 (std::max (20.0, f0) / 1000.0);
                    spec[(size_t) i] = v > spec[(size_t) i] ? v : spec[(size_t) i] * 0.8f + v * 0.2f;
                }
                lfoHist[(size_t) histPos] = ff->lfo1Now.load(); lfo2Hist[(size_t) histPos] = ff->lfo2Now.load(); envHist[(size_t) histPos] = ff->envNow.load();
                histPos = (histPos + 1) % (int) lfoHist.size();
                meter.update (ff->levelIn.load(), ff->levelOut.load(), 0.0f);
            }
            repaint();
        }
        static float xOf (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0)) * p.getWidth(); }
        static double hzOf (juce::Rectangle<float> p, float x) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
        static float yOfDb (juce::Rectangle<float> p, double dbv) { return p.getY() + (float) juce::jlimit (0.0, 1.0, (30.0 - dbv) / 78.0) * p.getHeight(); }   // +30 .. -48

        void paint (juce::Graphics& g) override
        {
            if (ff == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("fissure");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.85f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.25f), a.getCentreX(), a.getY(), night().withAlpha (0.86f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (violet().withAlpha (0.24f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            hits.clear();
            paintTopBar (g); paintResponse (g); paintSide (g); paintDeck (g);
            meter.draw (g, meterR(), cyan(), false);
        }

        // ---- the response
        juce::Point<float> nodeOf (int k) const
        {
            const auto p = plot(); const auto s = ff->slotParams (k);
            const float x = xOf (p, s.cutoffHz);
            const float y = s.type == SqFissure::peak ? yOfDb (p, s.gainDb) : yOfDb (p, SqFissure::Engine::responseDb (s, 48000.0, s.cutoffHz));
            return { x, y };
        }
        void paintResponse (juce::Graphics& g)
        {
            pane (g, respPane(), "");
            const auto p = plot();
            g.setFont (Theme::font (8.5f, true));
            for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
            {
                const float x = xOf (p, hz);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (text (0.32f)); g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), (int) x - 16, (int) p.getBottom() + 4, 32, 12, juce::Justification::centred);
            }
            for (int d = 24; d >= -48; d -= 12)
            {
                const float y = yOfDb (p, d);
                g.setColour (text (d == 0 ? 0.14f : 0.05f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
                g.setColour (text (0.32f)); g.drawText ((d > 0 ? "+" : "") + juce::String (d), (int) p.getRight() + 2, (int) y - 6, 26, 12, juce::Justification::left);
            }
            // spectrum
            juce::Path sp; sp.startNewSubPath (p.getX(), p.getBottom());
            for (int i = 0; i < pts; ++i) sp.lineTo (p.getX() + p.getWidth() * (i + 0.5f) / pts, p.getY() + juce::jlimit (0.0f, 1.0f, (6.0f - spec[(size_t) i]) / 84.0f) * p.getHeight());
            sp.lineTo (p.getRight(), p.getBottom()); sp.closeSubPath();
            g.setGradientFill (juce::ColourGradient (text (0.14f), 0, p.getY(), text (0.02f), 0, p.getBottom(), false)); g.fillPath (sp);
            // modulation ranges: lit bands across the swept cutoff
            for (int k = 0; k < P::numSlots; ++k)
            {
                if (! ff->slotOn[(size_t) k].get()) continue;
                double up = 0, dn = 0;
                for (int m = 0; m < P::numMods; ++m)
                    if (ff->modDst[(size_t) m].get() == k + 1 && ff->modSrc[(size_t) m].get() != 0)
                    { const double oct = ff->bank[P::mod1 + m] * 6.0; if (ff->modSrc[(size_t) m].get() == 3) { up += std::max (0.0, oct); dn += std::min (0.0, oct); } else { up += std::abs (oct); dn -= std::abs (oct); } }
                if (up == 0 && dn == 0) continue;
                const double fc = ff->sp (k, P::cutoff);
                const float x0 = xOf (p, fc * std::pow (2.0, dn)), x1 = xOf (p, fc * std::pow (2.0, up));
                g.setGradientFill (juce::ColourGradient (slotColour (k).withAlpha (0.16f), 0, p.getY(), slotColour (k).withAlpha (0.0f), 0, p.getBottom(), false));
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, p.getY(), x1, p.getBottom()));
                const float xn = xOf (p, ff->cutNow[(size_t) k].load());
                g.setColour (slotColour (k).withAlpha (0.8f)); g.drawVerticalLine ((int) xn, p.getY(), p.getBottom());
            }
            // each filter's curve
            const double fs = 48000.0;
            for (int k = 0; k < P::numSlots; ++k)
            {
                if (! ff->slotOn[(size_t) k].get()) continue;
                const auto s = ff->slotParams (k); juce::Path c;
                for (int i = 0; i <= 220; ++i) { const float x = p.getX() + p.getWidth() * i / 220.0f; const float y = yOfDb (p, SqFissure::Engine::responseDb (s, fs, hzOf (p, x))); if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y); }
                g.setColour (slotColour (k).withAlpha (k == sel ? 0.9f : 0.45f)); g.strokePath (c, juce::PathStrokeType (k == sel ? 1.8f : 1.1f));
            }
            // the combined curve, as the routing combines them
            {
                juce::Path c; const int routing = ff->routing.get();
                for (int i = 0; i <= 240; ++i)
                {
                    const float x = p.getX() + p.getWidth() * i / 240.0f; const double hz = hzOf (p, x);
                    double lin[4]; bool on[4];
                    for (int k = 0; k < 4; ++k) { on[k] = ff->slotOn[(size_t) k].get(); lin[k] = on[k] ? SqFissure::dbToGain (SqFissure::Engine::responseDb (ff->slotParams (k), fs, hz)) : 1.0; }
                    double m;
                    if (routing == 1) { double s = 0; int n = 0; for (int k = 0; k < 4; ++k) if (on[k]) { s += lin[k]; ++n; } m = n ? s / n : 1.0; }   // magnitudes summed: a guide (phase ignored)
                    else if (routing == 2) m = 0.5 * (lin[0] * lin[1] + lin[2] * lin[3]);
                    else if (routing == 3) m = lin[0] * lin[1];   // the mid's path
                    else m = lin[0] * lin[1] * lin[2] * lin[3];
                    const float y = yOfDb (p, SqFissure::gainToDb (m)); if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
                }
                g.setColour (text (0.22f)); g.strokePath (c, juce::PathStrokeType (5.0f)); g.setColour (text (0.95f)); g.strokePath (c, juce::PathStrokeType (1.4f));
            }
            // nodes
            for (int k = 0; k < P::numSlots; ++k)
            {
                if (! ff->slotOn[(size_t) k].get()) continue;
                const auto n = nodeOf (k); const bool hot = k == hoverNode || k == dragNode;
                g.setColour (slotColour (k).withAlpha (hot ? 0.4f : 0.2f)); g.fillEllipse (n.x - 13, n.y - 13, 26, 26);
                g.setColour (night()); g.fillEllipse (n.x - 8, n.y - 8, 16, 16);
                g.setColour (slotColour (k)); g.drawEllipse (n.x - 8, n.y - 8, 16, 16, k == sel ? 2.2f : 1.4f);
                g.setColour (text (1.0f)); g.setFont (Theme::font (9.0f, true)); g.drawText (juce::String (k + 1), juce::Rectangle<float> (n.x - 8, n.y - 8, 16, 16), juce::Justification::centred);
                if (hot || k == sel)
                {
                    const auto s = ff->slotParams (k);
                    auto tag = juce::Rectangle<float> (n.x - 62, n.y - 34, 124, 18).constrainedWithin (p.expanded (20.0f));
                    g.setColour (night().withAlpha (0.88f)); g.fillRoundedRectangle (tag, 9.0f); g.setColour (slotColour (k).withAlpha (0.6f)); g.drawRoundedRectangle (tag, 9.0f, 1.0f);
                    g.setColour (text (0.95f)); g.setFont (Theme::font (9.0f, true));
                    const auto hz = s.cutoffHz >= 1000 ? juce::String (s.cutoffHz / 1000.0, 2) + " kHz" : juce::String ((int) s.cutoffHz) + " Hz";
                    g.drawText (hz + (s.type == SqFissure::peak ? "  " + juce::String (s.gainDb, 1) + " dB" : "  Q " + juce::String ((int) (s.resonance * 100)) + "%"), tag, juce::Justification::centred);
                }
            }
            // slot tabs along the top
            for (int k = 0; k < P::numSlots; ++k)
            {
                const auto t = tabR (k); const bool on = ff->slotOn[(size_t) k].get(), s = k == sel;
                g.setColour (s ? slotColour (k).withAlpha (0.28f) : night().withAlpha (0.75f)); g.fillRoundedRectangle (t, 9.0f);
                g.setColour (slotColour (k).withAlpha (on ? (s ? 1.0f : 0.6f) : 0.2f)); g.drawRoundedRectangle (t, 9.0f, 1.0f);
                g.setColour (text (on ? 1.0f : 0.4f)); g.setFont (Theme::font (9.0f, true));
                g.drawText (juce::String (k + 1) + "  " + juce::String (SqFissure::typeName (ff->slotType[(size_t) k].get())).toUpperCase() + (on ? juce::String() : "  OFF"), t, juce::Justification::centred);
            }
        }
        juce::Rectangle<float> tabR (int k) const { const auto r = respPane().reduced (14.0f, 0); const float w = (r.getWidth() - 26.0f) / 4.0f; return { r.getX() + w * k + 2.0f, respPane().getY() + 12.0f, w - 4.0f, 22.0f }; }

        // ---- right column
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 9.0f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c, float fsz = 9.0f)
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (fsz, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        std::array<juce::Rectangle<float>, P::numMods> modAmtR {};
        void paintSide (juce::Graphics& g)
        {
            const auto r = side(); const auto col = slotColour (sel); pane (g, r, "FILTER " + juce::String (sel + 1));
            auto in = r.reduced (12.0f, 10.0f); in.removeFromTop (16.0f);
            {
                auto row = in.removeFromTop (22.0f); chip (g, row.removeFromLeft (52.0f), ff->slotOn[(size_t) sel].get() ? "ON" : "OFF", ff->slotOn[(size_t) sel].get(), 1, col); row.removeFromLeft (6.0f);
                for (int sl : { 12, 24, 48 }) { chip (g, row.removeFromLeft (54.0f), juce::String (sl) + " dB", ff->slotSlope[(size_t) sel].get() == sl, 100 + sl, col); row.removeFromLeft (4.0f); }
            }
            in.removeFromTop (6.0f);
            for (int rowI = 0; rowI < 2; ++rowI)
            {
                auto row = in.removeFromTop (22.0f); in.removeFromTop (4.0f); const int from = rowI * 4, to = std::min ((int) SqFissure::numTypes, from + 4); const float w = row.getWidth() / 4.0f;
                for (int t = from; t < to; ++t) chip (g, juce::Rectangle<float> (row.getX() + w * (t - from), row.getY(), w - 4.0f, row.getHeight()), juce::String (SqFissure::typeName (t)).toUpperCase(), ff->slotType[(size_t) sel].get() == t, 10 + t, col, 8.5f);
            }
            in.removeFromTop (6.0f);
            // modulators
            {
                auto mr = in.removeFromTop (40.0f);
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (mr, 5.0f);
                juce::Path a1, a2, e1; const int N = (int) lfoHist.size();
                for (int i = 0; i < N; ++i)
                {
                    const int idx = (histPos + i) % N; const float x = mr.getX() + mr.getWidth() * i / (N - 1.0f);
                    const float y1 = mr.getCentreY() - lfoHist[(size_t) idx] * mr.getHeight() * 0.4f, y2 = mr.getCentreY() - lfo2Hist[(size_t) idx] * mr.getHeight() * 0.4f, y3 = mr.getBottom() - 3 - envHist[(size_t) idx] * (mr.getHeight() - 6);
                    if (i == 0) { a1.startNewSubPath (x, y1); a2.startNewSubPath (x, y2); e1.startNewSubPath (x, y3); } else { a1.lineTo (x, y1); a2.lineTo (x, y2); e1.lineTo (x, y3); }
                }
                g.setColour (cyan()); g.strokePath (a1, juce::PathStrokeType (1.2f)); g.setColour (violet()); g.strokePath (a2, juce::PathStrokeType (1.2f)); g.setColour (juce::Colour (0xffff5fb0)); g.strokePath (e1, juce::PathStrokeType (1.2f));
                g.setFont (Theme::font (7.5f, true)); g.setColour (cyan()); g.drawText ("LFO 1", mr.reduced (4.0f, 2.0f), juce::Justification::topLeft);
                g.setColour (violet()); g.drawText ("LFO 2", mr.reduced (4.0f, 2.0f), juce::Justification::topRight); g.setColour (juce::Colour (0xffff5fb0)); g.drawText ("ENV", mr.reduced (4.0f, 2.0f), juce::Justification::bottomRight);
            }
            in.removeFromTop (6.0f);
            g.setColour (text (0.45f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("MODULATION  -  source, target, amount (drag)", in.removeFromTop (12.0f), juce::Justification::left);
            const float rowH = std::min (22.0f, (in.getHeight() - 2.0f) / P::numMods);
            for (int m = 0; m < P::numMods; ++m)
            {
                auto row = in.removeFromTop (rowH - 2.0f); in.removeFromTop (2.0f);
                const int src = ff->modSrc[(size_t) m].get(), dst = ff->modDst[(size_t) m].get();
                auto sR = row.removeFromLeft (62.0f); row.removeFromLeft (3.0f); auto dR = row.removeFromLeft (row.getWidth() * 0.6f); row.removeFromLeft (3.0f); modAmtR[(size_t) m] = row;
                chip (g, sR, P::sourceNames()[juce::jlimit (0, 3, src)].toUpperCase(), src != 0, 200 + m, src == 3 ? juce::Colour (0xffff5fb0) : src == 2 ? violet() : cyan(), 8.0f);
                chip (g, dR, SqFissure::targetName (dst).toUpperCase(), dst != 0, 300 + m, dst >= 1 && dst <= 12 ? slotColour ((dst - 1) % 4) : cyan(), 8.0f);
                const float amt = ff->bank[P::mod1 + m];
                g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (row, 5.0f);
                const float cx = row.getCentreX(), x2 = cx + amt * row.getWidth() * 0.5f;
                g.setColour (cyan().withAlpha (src && dst ? 0.85f : 0.3f)); g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, x2), row.getY() + 4, std::max (cx, x2), row.getBottom() - 4), 2.0f);
                g.setColour (text (0.25f)); g.drawVerticalLine ((int) cx, row.getY() + 2, row.getBottom() - 2);
                g.setColour (text (0.85f)); g.setFont (Theme::font (8.0f, true));
                const float sc = P::modScale (dst);
                g.drawText (sc > 1 ? (amt >= 0 ? "+" : "") + juce::String (amt * sc, 1) + " oct" : (amt >= 0 ? "+" : "") + juce::String ((int) std::lround (amt * 100)) + " %", row, juce::Justification::centred);
            }
        }

        // ---- top bar
        void paintTopBar (juce::Graphics& g)
        {
            auto bar = topBar();
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText ("ROUTING", bar.removeFromLeft (54.0f), juce::Justification::centredLeft);
            for (int i = 0; i < 4; ++i) { const auto t = P::routingNames()[i].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 22.0f), t, ff->routing.get() == i, 20 + i, violet()); bar.removeFromLeft (3.0f); }
            bar.removeFromLeft (14.0f);
            for (int l = 0; l < 2; ++l)
            {
                const int shp = l == 0 ? ff->lfo1Shape.get() : ff->lfo2Shape.get(), syn = l == 0 ? ff->lfo1Sync.get() : ff->lfo2Sync.get();
                const auto t = "LFO " + juce::String (l + 1) + ": " + P::lfoNames()[juce::jlimit (0, 4, shp)].toUpperCase();
                chip (g, bar.removeFromLeft (widthOf (t) + 22.0f), t, true, 30 + l, l == 0 ? cyan() : violet()); bar.removeFromLeft (3.0f);
                const auto s = syn > 0 ? P::syncNames()[syn].toUpperCase() : juce::String (ff->lfoRate (l), 2) + " HZ";
                chip (g, bar.removeFromLeft (widthOf (s) + 22.0f), s, syn > 0, 40 + l, l == 0 ? cyan() : violet()); bar.removeFromLeft (10.0f);
            }
            g.setColour (text (0.45f)); g.setFont (Theme::font (9.5f, true)); g.drawText ("ZERO LATENCY", bar, juce::Justification::right);
        }
        juce::UndoManager* um() const { return &ff->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        void menu (const juce::StringArray& names, int cur, std::function<void (int)> set)
        {
            juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<FissureViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Fissure setting"); auto& f = *ff;
            if (id == 1) { f.slotOn[(size_t) sel].setValue (! f.slotOn[(size_t) sel].get(), um()); changed(); return; }
            if (id >= 10 && id < 10 + SqFissure::numTypes) { f.slotType[(size_t) sel].setValue (id - 10, um()); changed(); return; }
            if (id == 112 || id == 124 || id == 148) { f.slotSlope[(size_t) sel].setValue (id - 100, um()); changed(); return; }
            if (id >= 20 && id < 24) { f.routing.setValue (id - 20, um()); changed(); return; }
            if (id == 30 || id == 31) { auto& cv = id == 30 ? f.lfo1Shape : f.lfo2Shape; cv.setValue ((cv.get() + 1) % 5, um()); changed(); return; }
            if (id == 40 || id == 41) { auto* cv = id == 40 ? &f.lfo1Sync : &f.lfo2Sync; juce::StringArray names = P::syncNames(); names.set (0, "Free (the Rate knob)"); menu (names, cv->get(), [this, cv] (int i) { cv->setValue (i, um()); }); return; }
            if (id >= 200 && id < 200 + P::numMods) { auto& cv = f.modSrc[(size_t) (id - 200)]; cv.setValue ((cv.get() + 1) % 4, um()); changed(); return; }
            if (id >= 300 && id < 300 + P::numMods)
            { const int m = id - 300; juce::StringArray names; for (int t = 0; t <= 13; ++t) names.add (SqFissure::targetName (t)); menu (names, f.modDst[(size_t) m].get(), [this, m] (int t) { ff->modDst[(size_t) m].setValue (t, um()); }); }
        }

        // ---- the deck: obsidian knobs with a glowing seam
        struct KnobDef { int slot; int k; const char* label; };
        std::vector<KnobDef> defs() const
        {
            return { { sel, P::cutoff, "CUTOFF" }, { sel, P::res, "RESONANCE" }, { sel, P::drive, "DRIVE" }, { sel, P::gain, "GAIN" }, { sel, P::pan, "PAN" }, { sel, P::slotMix, "FILTER MIX" },
                     { -1, P::lfo1Rate, "LFO 1 RATE" }, { -1, P::lfo2Rate, "LFO 2 RATE" }, { -1, P::envAttack, "ENV ATTACK" }, { -1, P::envRelease, "ENV RELEASE" }, { -1, P::envGain, "ENV GAIN" },
                     { -1, P::mix, "MIX" }, { -1, P::output, "OUTPUT" } };
        }
        int paramIndex (const KnobDef& d) const { return d.slot < 0 ? d.k : ff->paramOf (d.slot, d.k); }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1, dragNode = -1, hoverNode = -1, dragMod = -1; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *ff->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); }
        bool enabled (const KnobDef& d) const
        {
            if (d.slot >= 0 && d.k == P::gain) return ff->slotType[(size_t) sel].get() == SqFissure::peak;
            if (d.k == P::lfo1Rate && d.slot < 0) return ff->lfo1Sync.get() == 0;
            if (d.k == P::lfo2Rate && d.slot < 0) return ff->lfo2Sync.get() == 0;
            return true;
        }
        void layoutKnobs()
        {
            knobR.clear(); auto d = deck().reduced (12.0f, 12.0f); d.removeFromTop (14.0f);
            const int n = (int) defs().size(); const float w = (d.getWidth() - 24.0f) / n; float x = d.getX();
            for (int i = 0; i < n; ++i) { if (i == 6 || i == 11) x += 12.0f; knobR.push_back ({ x, d.getY(), w, d.getHeight() }); x += w; }
        }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, juce::Colour col, bool hotK, bool active, bool bipolar, bool en)
        {
            const float al = en ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (24.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 8.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text ((hotK ? 0.95f : 0.55f) * al)); g.setFont (Theme::font (8.0f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            const float ar = rO * 0.96f, as = bipolar ? (a0 + a1) * 0.5f : a0;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f * al)); g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (as, av), std::max (as, av), true);
            g.setColour (col.withAlpha (0.25f * al)); g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withAlpha (al)); g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // obsidian body: a faceted black glass disc
            const float rb = rO * 0.74f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2438).withAlpha (al), c.x - rb * 0.6f, c.y - rb, juce::Colour (0xff050409).withAlpha (al), c.x + rb * 0.5f, c.y + rb, false)); g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
            for (int f = 0; f < 6; ++f)
            {
                const float a = juce::MathConstants<float>::twoPi * f / 6.0f + 0.4f;
                g.setColour (juce::Colours::white.withAlpha ((0.03f + 0.03f * (f % 2)) * al));
                juce::Path facet; facet.startNewSubPath (c); facet.lineTo (c.x + rb * std::sin (a), c.y - rb * std::cos (a)); facet.lineTo (c.x + rb * std::sin (a + 1.05f), c.y - rb * std::cos (a + 1.05f)); facet.closeSubPath(); g.fillPath (facet);
            }
            g.setColour (col.withAlpha (0.35f * al)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
            // the seam: a glowing crack from the centre to the rim along the value angle, with a fork
            const juce::Point<float> p0 (c.x + rb * 0.1f * std::sin (av), c.y - rb * 0.1f * std::cos (av)), p1 (c.x + rb * 0.55f * std::sin (av + 0.12f), c.y - rb * 0.55f * std::cos (av + 0.12f)), p2 (c.x + rb * 0.95f * std::sin (av), c.y - rb * 0.95f * std::cos (av));
            const juce::Point<float> pf (c.x + rb * 0.78f * std::sin (av + 0.38f), c.y - rb * 0.78f * std::cos (av + 0.38f));
            juce::Path seam; seam.startNewSubPath (p0); seam.lineTo (p1); seam.lineTo (p2); seam.startNewSubPath (p1); seam.lineTo (pf);
            g.setColour (col.withAlpha (0.35f * al)); g.strokePath (seam, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::white.withAlpha (0.9f * al)); g.strokePath (seam, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (hotK) { g.setColour (col.withAlpha (0.8f)); g.drawEllipse (c.x - rb - 2, c.y - rb - 2, rb * 2 + 4, rb * 2 + 4, 1.0f); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 4.0f, 84.0f), 19.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 9.5f);
            g.setColour (active ? col : text (0.1f)); g.drawRoundedRectangle (pill, 9.5f, 1.0f);
            g.setColour ((active ? col : text (0.95f)).withMultipliedAlpha (al)); g.setFont (Theme::font (10.0f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (const KnobDef& d) const
        {
            const float v = param (paramIndex (d)).getCurrentValue();
            if (d.slot >= 0)
                switch (d.k)
                {
                    case P::cutoff: return v >= 1000 ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String ((int) std::lround (v)) + " Hz";
                    case P::drive: case P::gain: return (v > 0 && d.k == P::gain ? "+" : "") + juce::String (v, 1) + " dB";
                    case P::pan: return std::abs (v) < 0.5f ? juce::String ("C") : (v < 0 ? "L " : "R ") + juce::String ((int) std::lround (std::abs (v)));
                    default: return juce::String ((int) std::lround (v)) + " %";
                }
            switch (d.k)
            {
                case P::lfo1Rate: case P::lfo2Rate: { const double r = ff->lfoRate (d.k == P::lfo1Rate ? 0 : 1); return juce::String (r, r < 1 ? 2 : 1) + " Hz"; }
                case P::envAttack: case P::envRelease: return juce::String (v, v < 10 ? 1 : 0) + " ms";
                case P::mix: return juce::String ((int) std::lround (v)) + " %";
                default: return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff100c1a).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (violet().withAlpha (0.20f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            auto group = [&] (int from, int to, const juce::String& t, juce::Colour c)
            {
                auto rr = knobR[(size_t) from].getUnion (knobR[(size_t) to]).withTrimmedTop (-14.0f).expanded (2.0f, 2.0f);
                g.setColour (c.withAlpha (0.05f)); g.fillRoundedRectangle (rr, 10.0f); g.setColour (c.withAlpha (0.2f)); g.drawRoundedRectangle (rr, 10.0f, 1.0f);
                g.setColour (c.withAlpha (0.85f)); g.setFont (Theme::font (8.5f, true)); g.drawText (t, rr.reduced (10.0f, 3.0f).removeFromTop (11.0f), juce::Justification::left);
            };
            group (0, 5, "FILTER " + juce::String (sel + 1) + "  -  " + juce::String (SqFissure::typeName (ff->slotType[(size_t) sel].get())).toUpperCase(), slotColour (sel));
            group (6, 10, "MODULATORS", cyan()); group (11, 12, "GLOBAL", violet());
            const auto ds = defs();
            for (size_t i = 0; i < ds.size(); ++i)
            {
                const auto& k = ds[i]; const int pi_ = paramIndex (k); const bool en = enabled (k);
                const bool bip = (k.slot >= 0 && (k.k == P::gain || k.k == P::pan)) || (k.slot < 0 && (k.k == P::envGain || k.k == P::output));
                drawKnob (g, knobR[i], k.label, knobText (k), norm (pi_), k.slot >= 0 ? slotColour (sel) : (i <= 10 ? cyan() : violet()), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, bip, en);
                StudioUi::knobScale (g, knobR[i], ff->bank.specs[(size_t) pi_], text (1.0f), 15.0f, 24.0f, en ? 1.0f : 0.4f);
            }
        }

        // ---- mouse
        int nodeAt (juce::Point<float> pt) const { for (int k = P::numSlots - 1; k >= 0; --k) if (ff->slotOn[(size_t) k].get() && pt.getDistanceFrom (nodeOf (k)) < 12.0f) return k; return -1; }
        int knobAt (juce::Point<float> p) const { const auto ds = defs(); for (size_t i = 0; i < knobR.size() && i < ds.size(); ++i) if (knobR[i].reduced (4.0f, 0).contains (p) && enabled (ds[i])) return (int) i; return -1; }
        int modAt (juce::Point<float> p) const { for (int m = 0; m < P::numMods; ++m) if (modAmtR[(size_t) m].contains (p)) return m; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), n = nodeAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : n >= 0 ? juce::MouseCursor::DraggingHandCursor : modAt (e.position) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || n != hoverNode) { hoverKnob = k; hoverNode = n; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverNode = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (ff == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            for (int k = 0; k < P::numSlots; ++k) if (tabR (k).contains (e.position)) { sel = k; changed(); return; }
            if (const int m = modAt (e.position); m >= 0) { dragMod = m; um()->beginNewTransaction ("Fissure modulation amount"); param (P::mod1 + m).parameterChangeGestureBegin(); mouseDrag (e); return; }
            if (const int n = nodeAt (e.position); n >= 0)
            {
                sel = n; dragNode = n; um()->beginNewTransaction ("Fissure filter " + juce::String (n + 1));
                param (ff->paramOf (n, P::cutoff)).parameterChangeGestureBegin(); param (ff->paramOf (n, ff->slotType[(size_t) n].get() == SqFissure::peak ? P::gain : P::res)).parameterChangeGestureBegin();
                changed(); return;
            }
            const int k = knobAt (e.position);
            if (k >= 0) { const auto d = defs()[(size_t) k]; dragKnob = k; dragStart = e.position; dragStartNorm = norm (paramIndex (d)); um()->beginNewTransaction (juce::String ("Fissure ") + d.label); param (paramIndex (d)).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragMod >= 0) { const auto r = modAmtR[(size_t) dragMod]; auto& p = param (P::mod1 + dragMod); p.setParameter (juce::jlimit (-1.0f, 1.0f, (e.position.x - r.getCentreX()) / (r.getWidth() * 0.5f)), juce::sendNotification); changed(); return; }
            if (dragNode >= 0)
            {
                const auto p = plot(); setValue (ff->paramOf (dragNode, P::cutoff), (float) hzOf (p, e.position.x));
                const double dbAt = 30.0 - 78.0 * juce::jlimit (0.0, 1.0, (double) ((e.position.y - p.getY()) / p.getHeight()));
                if (ff->slotType[(size_t) dragNode].get() == SqFissure::peak) setValue (ff->paramOf (dragNode, P::gain), (float) dbAt);
                else setValue (ff->paramOf (dragNode, P::res), (float) juce::jlimit (0.0, 100.0, (dbAt + 3.0) / 33.0 * 100.0));   // dragging up raises the resonance peak
                changed(); return;
            }
            if (dragKnob < 0) return;
            setNorm (paramIndex (defs()[(size_t) dragKnob]), dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragMod >= 0) param (P::mod1 + dragMod).parameterChangeGestureEnd();
            if (dragNode >= 0) { param (ff->paramOf (dragNode, P::cutoff)).parameterChangeGestureEnd(); param (ff->paramOf (dragNode, ff->slotType[(size_t) dragNode].get() == SqFissure::peak ? P::gain : P::res)).parameterChangeGestureEnd(); }
            if (dragKnob >= 0) param (paramIndex (defs()[(size_t) dragKnob])).parameterChangeGestureEnd();
            dragKnob = -1; dragNode = -1; dragMod = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = paramIndex (defs()[(size_t) k]); um()->beginNewTransaction ("Fissure reset"); param (pi_).setParameter (ff->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Fissure wheel"); const int pi_ = paramIndex (defs()[(size_t) k]); setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
