// Included inside namespace Skins (PluginSkins.h): the Apex Series faces.

    /** Shared meter / history drawing for the Apex faces. */
    struct ApexBits
    {
        static void meter (juce::Graphics& g, juce::Rectangle<float> r, float db, juce::Colour c, const juce::String& label, float lo = -60.0f, bool fromTop = false)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r, 3.0f);
            const float t = juce::jlimit (0.0f, 1.0f, (db - lo) / (0.0f - lo));
            auto f = fromTop ? r.withHeight (r.getHeight() * (1.0f - t)) : r.withTrimmedTop (r.getHeight() * (1.0f - t));
            juce::ColourGradient cg (c.brighter (0.3f), r.getX(), r.getY(), c.darker (0.5f), r.getX(), r.getBottom(), false);
            if (! fromTop) cg.addColour (0.08, juce::Colour (0xffff3a5a));
            g.setGradientFill (cg); g.fillRoundedRectangle (f, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (Theme::font (8.5f, true));
            g.drawText (label, r.withY (r.getBottom() + 2.0f).withHeight (11.0f).expanded (8.0f, 0.0f), juce::Justification::centred);
        }
        static void history (juce::Graphics& g, juce::Rectangle<float> r, SqApexBase& p, juce::Colour c, float range = 24.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r, 4.0f);
            const int w = p.historyWrite.load(), n = SqApexBase::historySize;
            juce::Path in, gr;
            for (int k = 0; k < n; ++k)
            {
                const int idx = (w - n + k) & (n - 1);
                const float x = r.getX() + r.getWidth() * (float) k / (float) (n - 1);
                const float yi = r.getBottom() - juce::jlimit (0.0f, 1.0f, (p.inHistory[(size_t) idx] + 60.0f) / 60.0f) * r.getHeight();
                const float yg = r.getY() + juce::jlimit (0.0f, 1.0f, -p.grHistory[(size_t) idx] / range) * r.getHeight();
                if (k == 0) { in.startNewSubPath (x, r.getBottom()); gr.startNewSubPath (x, r.getY()); }
                in.lineTo (x, yi); gr.lineTo (x, yg);
            }
            in.lineTo (r.getRight(), r.getBottom()); in.closeSubPath();
            gr.lineTo (r.getRight(), r.getY()); gr.closeSubPath();
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.03f), r.getX(), r.getBottom(), false));
            g.fillPath (in);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff3a5a).withAlpha (0.85f), r.getX(), r.getY(), c.withAlpha (0.25f), r.getX(), r.getBottom(), false));
            g.fillPath (gr);
            g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (Theme::font (8.5f));
            for (float d : { 6.0f, 12.0f, 18.0f }) if (d < range) { const float y = r.getY() + d / range * r.getHeight(); g.drawHorizontalLine ((int) y, r.getX(), r.getRight()); g.drawText ("-" + juce::String ((int) d), (int) r.getX() + 3, (int) y - 11, 30, 10, juce::Justification::left); }
            g.setColour (c.withAlpha (0.5f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.65f)); g.setFont (Theme::font (9.0f, true));
            g.drawText ("GAIN REDUCTION", r.reduced (6.0f).removeFromTop (12.0f), juce::Justification::right);
        }
    };

    /** Apex Dynamics: animated transfer curve with the live ball, gain-reduction history and meters. */
    struct ApexDynViz : public Viz
    {
        ApexDynViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), dyn (dynamic_cast<SqApexDynamicsPlugin*> (&p)) {}
        SqApexDynamicsPlugin* dyn;
        void paint (juce::Graphics& g) override
        {
            if (dyn == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto curveR = r.removeFromLeft (r.getHeight()).reduced (2.0f);
            r.removeFromLeft (10.0f);
            auto meters = r.removeFromRight (92.0f).withTrimmedBottom (14.0f);
            r.removeFromRight (8.0f);
            // transfer curve
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0716), curveR.getX(), curveR.getY(), acc.darker (2.4f), curveR.getRight(), curveR.getBottom(), false));
            g.fillRoundedRectangle (curveR, 6.0f);
            const float lo = -60.0f;
            auto px = [&] (float db) { return curveR.getX() + (db - lo) / -lo * curveR.getWidth(); };
            auto py = [&] (float db) { return curveR.getBottom() - (juce::jlimit (lo - 30, 24.0f, db) - lo) / -lo * curveR.getHeight(); };
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            for (float d = -54; d < 0; d += 6) { g.drawVerticalLine ((int) px (d), curveR.getY(), curveR.getBottom()); g.drawHorizontalLine ((int) py (d), curveR.getX(), curveR.getRight()); }
            g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawLine (px (lo), py (lo), px (0), py (0), 1.0f);
            g.setFont (Theme::font (8.0f)); g.setColour (juce::Colours::white.withAlpha (0.5f));   // phase Z: dB on both axes
            for (float d = -48; d < 0; d += 12)
            {
                g.drawText (juce::String ((int) d), (int) px (d) + 2, (int) curveR.getBottom() - 24, 26, 9, juce::Justification::left);
                g.drawText (juce::String ((int) d), (int) curveR.getX() + 3, (int) py (d) - 9, 26, 9, juce::Justification::left);
            }
            g.drawText ("IN dB", curveR.reduced (6.0f, 14.0f).toNearestInt(), juce::Justification::bottomLeft);
            g.drawText ("OUT dB", curveR.reduced (6.0f, 16.0f).toNearestInt(), juce::Justification::topRight);
            const int md = dyn->mode.get();
            const float thr = dyn->bank[SqApexDynamicsPlugin::threshold], rat = dyn->bank[SqApexDynamicsPlugin::ratio], kn = dyn->bank[SqApexDynamicsPlugin::knee], dep = dyn->bank[SqApexDynamicsPlugin::depth];
            juce::Path pth;
            for (int i = 0; i <= 120; ++i)
            {
                const float x = lo + (float) i / 120.0f * -lo;
                float yv = x + (md == 2 ? (x > thr ? 0.0f : -dep) : Apex::Curve::gainDb (md, x, thr, rat, kn, dep));
                if (i == 0) pth.startNewSubPath (px (x), py (yv)); else pth.lineTo (px (x), py (yv));
            }
            glowStroke (g, pth, acc, 2.4f);
            g.setColour (Theme::amber().withAlpha (0.7f)); g.drawVerticalLine ((int) px (thr), curveR.getY(), curveR.getBottom());
            // the ball: detector level in, curve out
            const float d = juce::jlimit (lo, 0.0f, dyn->detDb.load()), gr = dyn->grDb.load();
            const auto ball = juce::Point<float> (px (d), py (d + gr));
            const bool active = std::abs (gr) > 0.1f;
            g.setColour ((active ? juce::Colour (0xffff3a5a) : acc).withAlpha (0.3f)); g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (ball));
            g.setColour (active ? juce::Colour (0xffff3a5a) : acc); g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (ball));
            g.setColour (juce::Colours::white.withAlpha (0.8f)); g.setFont (Theme::font (9.0f, true));
            g.drawText (SqApexDynamicsPlugin::modeNames()[md] + "  " + Apex::Curve::ratioText (rat, md), curveR.reduced (6.0f).removeFromTop (12.0f), juce::Justification::left);
            g.drawText ("THR " + juce::String (thr, 1), curveR.reduced (6.0f).removeFromBottom (12.0f), juce::Justification::right);
            // history
            ApexBits::history (g, r, *dyn, acc, juce::jmax (12.0f, juce::jmin (48.0f, dep)));
            // meters
            const float mw = (meters.getWidth() - 12.0f) / 3.0f;
            ApexBits::meter (g, meters.removeFromLeft (mw), dyn->inDb.load(), acc, "IN"); meters.removeFromLeft (6.0f);
            ApexBits::meter (g, meters.removeFromLeft (mw), -juce::jmin (60.0f, std::abs (gr)) , juce::Colour (0xffff3a5a), "GR", -60.0f, true); meters.removeFromLeft (6.0f);
            ApexBits::meter (g, meters, dyn->outDb.load(), acc, "OUT");
        }
    };

    /** Apex Limiter: gain reduction history, loudness panel with targets and the 60-second loudness history. */
    struct ApexLimViz : public Viz
    {
        ApexLimViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), lim (dynamic_cast<SqApexLimiterPlugin*> (&p)) {}
        SqApexLimiterPlugin* lim;
        juce::String status;
        void paint (juce::Graphics& g) override
        {
            if (lim == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto panel = r.removeFromRight (300.0f);
            r.removeFromRight (8.0f);
            auto top = r.removeFromTop (r.getHeight() * 0.48f);
            r.removeFromTop (6.0f);
            ApexBits::history (g, top, *lim, acc, 18.0f);
            // 60 s loudness history (short-term) and true peak
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r, 4.0f);
            const auto& tg = SqApexLimiterPlugin::targets()[(size_t) juce::jlimit (0, (int) SqApexLimiterPlugin::targets().size() - 1, lim->target.get())];
            auto yFor = [&] (float lufs) { return r.getBottom() - juce::jlimit (0.0f, 1.0f, (lufs + 40.0f) / 40.0f) * r.getHeight(); };
            g.setColour (Theme::amber().withAlpha (0.8f)); g.drawHorizontalLine ((int) yFor (tg.lufs), r.getX(), r.getRight());
            g.setFont (Theme::font (8.5f)); g.drawText ("TARGET " + juce::String (juce::roundToInt (tg.lufs)), (int) r.getRight() - 80, (int) yFor (tg.lufs) - 11, 76, 10, juce::Justification::right);
            const int w = lim->loudWrite.load(), N = SqApexLimiterPlugin::loudHistory;
            juce::Path st; bool pen = false;
            for (int k = 0; k < N; ++k)
            {
                const int idx = w - N + k; if (idx < 0) { pen = false; continue; }
                const float v = lim->stHistory[(size_t) (idx % N)];
                const float x = r.getX() + r.getWidth() * (float) k / (float) (N - 1);
                if (v < -69.0f) { pen = false; continue; }
                if (! pen) { st.startNewSubPath (x, yFor (v)); pen = true; } else st.lineTo (x, yFor (v));
                const float tpv = lim->tpHistory[(size_t) (idx % N)];
                if (tpv > tg.tp) { g.setColour (juce::Colour (0xffff3a5a).withAlpha (0.6f)); g.drawVerticalLine ((int) x, r.getY(), r.getY() + 6.0f); }
            }
            glowStroke (g, st, juce::Colour (0xff3ad1ff), 1.8f);
            g.setColour (juce::Colours::white.withAlpha (0.65f)); g.setFont (Theme::font (9.0f, true));
            g.drawText ("SHORT-TERM LOUDNESS - LAST 60 s", r.reduced (6.0f).removeFromTop (12.0f), juce::Justification::left);
            // loudness panel
            g.setGradientFill (juce::ColourGradient (acc.darker (1.8f).withAlpha (0.95f), panel.getX(), panel.getY(), juce::Colour (0xff0b0716), panel.getRight(), panel.getBottom(), false));
            g.fillRoundedRectangle (panel, 6.0f);
            g.setColour (acc.withAlpha (0.5f)); g.drawRoundedRectangle (panel, 6.0f, 1.0f);
            auto p = panel.reduced (10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("INTEGRATED", p.removeFromTop (14.0f), juce::Justification::left);
            const float integ = lim->integrated.load();
            const bool onTarget = std::abs (integ - tg.lufs) <= 1.0f;
            g.setColour (integ < -69.0f ? juce::Colours::white.withAlpha (0.3f) : onTarget ? juce::Colour (0xff3affb0) : juce::Colours::white);
            g.setFont (Theme::font (40.0f, true));
            g.drawText (integ < -69.0f ? juce::String ("--.-") : juce::String (integ, 1), p.removeFromTop (46.0f), juce::Justification::left);
            g.setFont (Theme::font (11.0f, true)); g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText ("LUFS   target " + juce::String (juce::roundToInt (tg.lufs)) + " / " + juce::String (tg.tp, 1) + " dBTP", p.removeFromTop (16.0f), juce::Justification::left);
            p.removeFromTop (8.0f);
            auto bar = [&] (const juce::String& name, float v, float lo, float hi, juce::Colour c, const juce::String& unit)
            {
                auto row = p.removeFromTop (22.0f);
                g.setColour (juce::Colours::white.withAlpha (0.65f)); g.setFont (Theme::font (9.0f, true));
                g.drawText (name, row.removeFromLeft (78.0f), juce::Justification::left);
                auto val = row.removeFromRight (64.0f);
                auto b = row.reduced (0, 5);
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b, 3.0f);
                const float t = juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
                g.setGradientFill (juce::ColourGradient (c.darker (0.4f), b.getX(), 0, c.brighter (0.3f), b.getRight(), 0, false));
                if (v > lo) g.fillRoundedRectangle (b.withWidth (b.getWidth() * t), 3.0f);
                g.setColour (juce::Colours::white); g.setFont (Theme::font (10.0f, true));
                g.drawText (v <= lo + 0.01f ? juce::String ("--") : juce::String (v, 1) + unit, val, juce::Justification::right);
            };
            bar ("MOMENTARY", lim->momentary.load(), -40.0f, 0.0f, juce::Colour (0xff3ad1ff), "");
            bar ("SHORT-TERM", lim->shortTerm.load(), -40.0f, 0.0f, juce::Colour (0xff9d6bff), "");
            bar ("RANGE (LRA)", lim->lra.load(), 0.0f, 20.0f, juce::Colour (0xffb6ff3a), " LU");
            bar ("TRUE PEAK MAX", lim->tpMax.load(), -30.0f, 3.0f, lim->tpMax.load() > tg.tp + 0.05f ? juce::Colour (0xffff3a5a) : juce::Colour (0xff3affb0), "");
            bar ("GAIN REDUCTION", -lim->grDb.load(), 0.0f, 18.0f, juce::Colour (0xffff3a5a), " dB");
            if (status.isNotEmpty()) { g.setColour (Theme::amber()); g.setFont (Theme::font (9.0f)); g.drawFittedText (status, p.removeFromTop (30.0f).toNearestInt(), juce::Justification::topLeft, 2); }
        }
    };

    /** Apex Multiband: live spectrum with coloured bands, draggable crossovers, per-band gain-change bars. */
    struct ApexMBViz : public Viz, private juce::Timer
    {
        ApexMBViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mb (dynamic_cast<SqApexMultibandPlugin*> (&p)), fft (SqApexMultibandPlugin::fftOrder)
        {
            setInterceptsMouseClicks (true, false);
            spectrum.fill (-100.0f);
            startTimerHz (20);
        }
        SqApexMultibandPlugin* mb;
        juce::dsp::FFT fft;
        std::array<float, SqApexMultibandPlugin::fftSize * 2> work {};
        std::array<float, SqApexMultibandPlugin::fftSize / 2> spectrum {};
        std::function<void()> onChange;
        int dragX = -1;
        const juce::Colour bandCol[6] { juce::Colour (0xffff3a5a), juce::Colour (0xffffa13a), juce::Colour (0xffffe03a), juce::Colour (0xff3affb0), juce::Colour (0xff3ad1ff), juce::Colour (0xff9d6bff) };

        juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (6.0f); }
        void timerCallback() override
        {
            if (mb == nullptr) return;
            const int N = SqApexMultibandPlugin::fftSize, w = mb->scopeWrite.load();
            for (int i = 0; i < N; ++i)
            {
                const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));
                work[(size_t) i] = mb->scope[(size_t) ((w - N + i) & (N - 1))] * win;
            }
            std::fill (work.begin() + N, work.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (work.data());
            for (int b = 0; b < N / 2; ++b)
            {
                const float db = Apex::gainToDb (work[(size_t) b] * 4.0f / (float) N);
                spectrum[(size_t) b] = db > spectrum[(size_t) b] ? db : spectrum[(size_t) b] * 0.85f + db * 0.15f;
            }
        }
        void paint (juce::Graphics& g) override
        {
            if (mb == nullptr) return;
            auto r = area();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0716), r.getX(), r.getY(), neon (accent).darker (2.2f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            drawFreqGrid (g, r, 24.0f);
            const int nb = juce::jlimit (1, SqApexMultibandPlugin::maxBands, mb->numBands.get());
            const double sr = mb->edit.engine.getDeviceManager().getSampleRate() > 0 ? mb->edit.engine.getDeviceManager().getSampleRate() : 48000.0;
            // bands
            float edges[7]; edges[0] = 20.0f; edges[nb] = 20000.0f;
            for (int i = 0; i < nb - 1; ++i) edges[i + 1] = mb->crossover (i);
            for (int k = 0; k < nb; ++k)
            {
                const float x0 = xForHz (edges[k], r), x1 = xForHz (edges[k + 1], r);
                const auto col = bandCol[k];
                const bool byp = mb->bypass[(size_t) k].get(), sol = mb->solo[(size_t) k].get();
                g.setGradientFill (juce::ColourGradient (col.withAlpha (byp ? 0.04f : 0.16f), 0, r.getY(), col.withAlpha (0.02f), 0, r.getBottom(), false));
                g.fillRect (juce::Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight()));
                // gain change bar from the centre line
                const float gr = mb->bandGr[(size_t) k].load();
                const float cy = r.getCentreY(), h = juce::jlimit (-1.0f, 1.0f, gr / 24.0f) * r.getHeight() * 0.5f;
                auto barR = juce::Rectangle<float> ((x0 + x1) * 0.5f - 7.0f, h < 0 ? cy : cy - h, 14.0f, std::abs (h));
                g.setColour (col.withAlpha (0.9f)); g.fillRoundedRectangle (barR, 3.0f);
                // threshold line (relative to a -60..0 scale)
                const float ty = r.getBottom() - (mb->bp (k, 0) + 60.0f) / 60.0f * r.getHeight();
                g.setColour (col.withAlpha (0.8f)); g.drawLine (x0 + 4, ty, x1 - 4, ty, 1.2f);
                g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (Theme::font (9.5f, true));
                g.drawText ("BAND " + juce::String (k + 1) + (sol ? "  S" : "") + (byp ? "  BYP" : "") + (mb->flip[(size_t) k].get() ? "  FLIP" : ""), juce::Rectangle<float> (x0, r.getY() + 4.0f, x1 - x0, 12.0f), juce::Justification::centred);
                g.setFont (Theme::font (9.0f)); g.setColour (col);
                g.drawText (juce::String (gr, 1) + " dB", juce::Rectangle<float> (x0, r.getY() + 16.0f, x1 - x0, 12.0f), juce::Justification::centred);
            }
            // spectrum
            juce::Path sp; bool started = false;
            const int N = SqApexMultibandPlugin::fftSize;
            for (int b = 1; b < N / 2; ++b)
            {
                const float hz = (float) (b * sr / N); if (hz < 20.0f || hz > 20000.0f) continue;
                const float x = xForHz (hz, r), y = r.getBottom() - juce::jlimit (0.0f, 1.0f, (spectrum[(size_t) b] + 90.0f) / 90.0f) * r.getHeight();
                if (! started) { sp.startNewSubPath (x, r.getBottom()); started = true; }
                sp.lineTo (x, y);
            }
            if (started)
            {
                sp.lineTo (r.getRight(), r.getBottom()); sp.closeSubPath();
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), 0, r.getY(), juce::Colours::white.withAlpha (0.03f), 0, r.getBottom(), false));
                g.fillPath (sp);
            }
            // crossover handles
            for (int i = 0; i < nb - 1; ++i)
            {
                const float x = xForHz (edges[i + 1], r);
                g.setColour (juce::Colours::white.withAlpha (dragX == i ? 1.0f : 0.6f)); g.drawLine (x, r.getY(), x, r.getBottom(), dragX == i ? 2.0f : 1.2f);
                g.fillEllipse (x - 5.0f, r.getBottom() - 14.0f, 10.0f, 10.0f);
                g.setFont (Theme::font (8.5f, true));
                const float hz = edges[i + 1];
                g.drawText (hz >= 1000 ? juce::String (hz / 1000.0f, 1) + "k" : juce::String ((int) hz), (int) x - 24, (int) r.getBottom() - 28, 48, 12, juce::Justification::centred);
            }
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (9.0f, true));
            g.drawText (mb->linearPhase.get() ? "LINEAR PHASE" : "MINIMUM PHASE (LR4)", r.reduced (8.0f).removeFromBottom (12.0f), juce::Justification::left);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (mb == nullptr) return;
            const int nb = juce::jlimit (1, SqApexMultibandPlugin::maxBands, mb->numBands.get());
            auto r = area(); dragX = -1; float best = 10.0f;
            for (int i = 0; i < nb - 1; ++i) { const float d = std::abs (xForHz (mb->crossover (i), r) - e.position.x); if (d < best) { best = d; dragX = i; } }
            if (dragX >= 0) mb->bank.params[(size_t) (SqApexMultibandPlugin::x1 + dragX)]->parameterChangeGestureBegin();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (mb == nullptr || dragX < 0) return;
            const float hz = juce::jlimit (20.0f, 20000.0f, hzForX (e.position.x, area()));
            mb->bank.params[(size_t) (SqApexMultibandPlugin::x1 + dragX)]->setParameter (hz, juce::sendNotification);
            repaint(); if (onChange) onChange();
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (mb != nullptr && dragX >= 0) mb->bank.params[(size_t) (SqApexMultibandPlugin::x1 + dragX)]->parameterChangeGestureEnd();
            dragX = -1; repaint();
        }
    };

    /** Apex Sub: the tracked note, the source band, the lower / upper sub waves and the filter shape. */
    struct ApexSubViz : public Viz
    {
        ApexSubViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sub (dynamic_cast<SqApexSubPlugin*> (&p)) {}
        SqApexSubPlugin* sub;
        void paint (juce::Graphics& g) override
        {
            if (sub == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (190.0f);
            r.removeFromRight (8.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0716), r.getX(), r.getY(), acc.darker (2.3f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            // filter + range on a 10 Hz - 1 kHz log axis
            auto xf = [&] (float hz) { return r.getX() + std::log (hz / 10.0f) / std::log (100.0f) * r.getWidth(); };
            const auto& rg = SqApexSubPlugin::ranges()[(size_t) juce::jlimit (0, 5, sub->range.get())];
            g.setColour (Theme::amber().withAlpha (0.18f)); g.fillRect (juce::Rectangle<float> (xf (rg.lo), r.getY(), xf (rg.hi) - xf (rg.lo), r.getHeight()));
            g.setColour (Theme::amber().withAlpha (0.8f)); g.setFont (Theme::font (9.0f, true));
            g.drawText ("SOURCE " + juce::String (rg.name) + " Hz", juce::Rectangle<float> (xf (rg.lo), r.getY() + 4.0f, xf (rg.hi) - xf (rg.lo) + 60.0f, 12.0f), juce::Justification::left);
            const float hp = sub->bank[SqApexSubPlugin::hpFreq], lp = sub->bank[SqApexSubPlugin::lpFreq];
            juce::Path fr;
            for (int i = 0; i <= (int) r.getWidth(); i += 2)
            {
                const float hz = 10.0f * std::pow (100.0f, (float) i / r.getWidth());
                const float h4 = std::pow (hz / hp, 4.0f), l4 = std::pow (lp / hz, 4.0f);
                const float mag = (h4 / (1.0f + h4)) * (l4 / (1.0f + l4));
                const float y = r.getBottom() - 8.0f - juce::jlimit (0.0f, 1.0f, (Apex::gainToDb (mag) + 36.0f) / 42.0f) * (r.getHeight() - 20.0f);
                if (i == 0) fr.startNewSubPath (r.getX() + (float) i, y); else fr.lineTo (r.getX() + (float) i, y);
            }
            glowStroke (g, fr, acc, 2.0f);
            // phase Z: Hz along the bottom, dB up the right (the curve spans -36..+6 dB)
            g.setFont (Theme::font (8.5f));
            for (float f : { 20.0f, 30.0f, 50.0f, 100.0f, 200.0f, 300.0f, 500.0f })
            {
                g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawVerticalLine ((int) xf (f), r.getY(), r.getBottom());
                g.setColour (juce::Colours::white.withAlpha (0.5f)); g.drawText (juce::String ((int) f), (int) xf (f) + 2, (int) r.getBottom() - 12, 30, 10, juce::Justification::left);
            }
            g.drawText ("Hz", (int) r.getRight() - 24, (int) r.getBottom() - 12, 20, 10, juce::Justification::right);
            for (float db : { 0.0f, -12.0f, -24.0f })
            {
                const float y = r.getBottom() - 8.0f - (db + 36.0f) / 42.0f * (r.getHeight() - 20.0f);
                g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
                g.setColour (juce::Colours::white.withAlpha (0.5f)); g.drawText ((db < 0 ? juce::String ((int) db) : juce::String ("0")) + " dB", (int) r.getRight() - 46, (int) y - 10, 42, 10, juce::Justification::right);
            }
            // sub waves (lower and upper), animated with the level
            const float lvl = juce::jlimit (0.0f, 1.0f, sub->subLevel.load() * 2.0f + 0.15f);
            auto wv = r.reduced (20.0f, r.getHeight() * 0.3f);
            drawWave (g, wv, [this, lvl] (float t) { return lvl * std::sin (juce::MathConstants<float>::twoPi * (t * 2.0f + phase * 0.25f)); }, juce::Colour (0xffff3ad1).withAlpha (0.9f), 2.4f);
            drawWave (g, wv, [this, lvl] (float t) { return 0.6f * lvl * std::sin (juce::MathConstants<float>::twoPi * (t * 3.0f + phase * 0.375f)); }, juce::Colour (0xff3ad1ff).withAlpha (0.7f), 1.6f);
            // readout
            g.setGradientFill (juce::ColourGradient (acc.darker (1.7f), read.getX(), read.getY(), juce::Colour (0xff0b0716), read.getRight(), read.getBottom(), false));
            g.fillRoundedRectangle (read, 6.0f);
            auto p = read.reduced (10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (9.5f, true));
            g.drawText (sub->midiTune.get() ? "MIDI NOTE" : "TRACKING", p.removeFromTop (14.0f), juce::Justification::left);
            const float hz = sub->trackedHz.load();
            g.setColour (hz > 0 ? juce::Colours::white : juce::Colours::white.withAlpha (0.3f)); g.setFont (Theme::font (34.0f, true));
            g.drawText (hz > 0 ? noteLabel (SqVocal::hzToNote (hz * 0.5f)) : juce::String ("--"), p.removeFromTop (40.0f), juce::Justification::left);
            g.setFont (Theme::font (10.0f)); g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (hz > 0 ? "source " + juce::String (hz, 1) + " Hz" : juce::String ("no low end found"), p.removeFromTop (16.0f), juce::Justification::left);
            g.drawText (hz > 0 ? "sub " + juce::String (hz * 0.5f, 1) + " Hz, fifth " + juce::String (hz * 0.75f, 1) + " Hz" : juce::String(), p.removeFromTop (16.0f), juce::Justification::left);
        }
    };
