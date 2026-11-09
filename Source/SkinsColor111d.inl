// Included inside namespace Skins (PluginSkins.h): faces for Larva Filter 2, Twelve Phase 2 and Hive Rack (1.11 Color & Space).

    namespace Face111d
    {
        /** Display magnitude (dB) of one Larva 2 stage: a 2-pole response for the Sallen-Key and state-variable models,
            4-pole low / high-pass for the ladders; Q follows the resonance the way the stages are voiced. */
        inline float stageDb (int response, int model, float f, float fc, float res)
        {
            const double w = f / juce::jmax (10.0f, fc), q = 0.55 + res * (model <= 1 ? 6.0 : 9.0);
            const std::complex<double> s (0.0, w), den = s * s + s / q + 1.0;
            std::complex<double> h;
            switch (response)
            {
                case 1:  h = (s * s) / den; break;
                case 2:  h = (s / q) / den; break;
                case 3:  h = (s * s + 1.0) / den; break;
                default: h = 1.0 / den; break;
            }
            double m = std::abs (h);
            if (model <= 1 && response <= 1) { const std::complex<double> d2 = s * s + s * 1.4142 + 1.0; m *= std::abs (response == 0 ? 1.0 / d2 : (s * s) / d2); }
            return (float) juce::Decibels::gainToDecibels (m, -80.0);
        }
        inline float larvaDb (const Larva2::Params& p, float f, float cutoff)
        {
            auto chain = [&] (int a, int b)
            {
                double db = 0; for (int i = a; i < b; ++i) db += stageDb (p.response[i], p.model[i], f, cutoff * std::exp2 (p.stageOct[i]), p.resonance); return db;
            };
            const int n = juce::jlimit (1, 8, p.stages);
            double db;
            if (p.routing == Larva2::parallel)
            {
                double sum = 0; for (int i = 0; i < n; ++i) sum += juce::Decibels::decibelsToGain (stageDb (p.response[i], p.model[i], f, cutoff * std::exp2 (p.stageOct[i]), p.resonance));
                db = juce::Decibels::gainToDecibels (sum / n, -80.0);
            }
            else if (p.routing == Larva2::split44 && n > 4)
                db = juce::Decibels::gainToDecibels (0.5 * (juce::Decibels::decibelsToGain (chain (0, 4)) + juce::Decibels::decibelsToGain (chain (4, n))), -80.0);
            else db = chain (0, n);
            return (float) (db + p.outputDb + p.inputDb);
        }
        inline void stepStrip (juce::Graphics& g, juce::Rectangle<float> r, const Eng::WingMod::Settings& m, int cur, juce::Colour acc)
        {
            const int ns = juce::jlimit (1, 16, m.numSteps); const float w = r.getWidth() / 16.0f;
            for (int i = 0; i < 16; ++i)
            {
                auto c = juce::Rectangle<float> (r.getX() + i * w + 1, r.getY(), w - 2, r.getHeight());
                g.setColour (juce::Colours::white.withAlpha (i < ns ? 0.07f : 0.025f)); g.fillRoundedRectangle (c, 2.0f);
                if (i >= ns) continue;
                const float v = juce::jlimit (0.0f, 1.0f, m.steps[i]);
                g.setColour (i == cur ? juce::Colours::white : acc.withAlpha (0.75f)); g.fillRoundedRectangle (c.withTrimmedTop (c.getHeight() * (1.0f - v)), 2.0f);
            }
        }
    }

    //==============================================================================
    /** Larva Filter 2: the combined response at the live (modulated) cutoff over the static one, a lane per stage showing its
        type and offset, the Wing Mod meter and rhythm steps. */
    struct Larva2Viz : public Viz, private juce::Timer
    {
        Larva2Viz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), lf (dynamic_cast<SqLarvaFilter2Plugin*> (&p)) { startTimerHz (30); }
        SqLarvaFilter2Plugin* lf; float cut = 800, mod = 0, env = 0; int step = -1;
        void timerCallback() override
        {
            if (lf == nullptr) return;
            const float c = lf->proc.modulatedCutoff(); if (c > 1.0f) cut += 0.5f * (c - cut);
            mod += 0.5f * (lf->proc.modValue() - mod); env = lf->proc.envelopeLevel(); step = lf->proc.currentStep();
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (lf == nullptr) return;
            const auto acc = neon (accent); const auto kp = lf->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (12.0f, 8.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "RESPONSE   (bright = now, faint = at the Cutoff knob)");
            auto lanes = plot.removeFromBottom (34.0f); plot.removeFromBottom (4.0f);
            ColourBits::freqGrid (g, plot, 30.0f);
            const float live = cut > 1.0f && lf->isEnabled() ? cut : kp.cutoffHz;
            auto still = ColourBits::curve (plot, 30.0f, [&] (float f) { return Face111d::larvaDb (kp, f, kp.cutoffHz); });
            g.setColour (acc.withAlpha (0.3f)); g.strokePath (still, juce::PathStrokeType (1.2f));
            auto now = ColourBits::curve (plot, 30.0f, [&] (float f) { return Face111d::larvaDb (kp, f, live); });
            ColourBits::fillUnder (g, now, plot, acc);
            g.setColour (acc); g.strokePath (now, juce::PathStrokeType (2.0f));
            g.setColour (juce::Colours::white.withAlpha (0.8f)); g.drawVerticalLine ((int) Viz::xForHz (live, plot), plot.getY(), plot.getBottom());
            // one lane per stage
            const int n = juce::jlimit (1, 8, kp.stages); const float lw = lanes.getWidth() / 8.0f;
            for (int i = 0; i < 8; ++i)
            {
                auto c = juce::Rectangle<float> (lanes.getX() + i * lw + 2, lanes.getY(), lw - 4, lanes.getHeight());
                const bool on = i < n;
                g.setColour (on ? acc.withAlpha (kp.routing == Larva2::split44 && i >= 4 ? 0.16f : 0.24f) : juce::Colours::white.withAlpha (0.04f)); g.fillRoundedRectangle (c, 4.0f);
                if (! on) continue;
                g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (Theme::font (9.0f, true));
                g.drawText (juce::String (i + 1) + "  " + SqLarvaFilter2Plugin::responseNames()[kp.response[i]], c.removeFromTop (16.0f).toNearestInt(), juce::Justification::centred);
                g.setFont (Theme::font (8.0f)); g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawText (SqLarvaFilter2Plugin::modelNames()[kp.model[i]] + (std::abs (kp.stageOct[i]) > 0.005f ? juce::String (kp.stageOct[i] > 0 ? "  +" : "  ") + juce::String (kp.stageOct[i], 1) : juce::String()),
                            c.toNearestInt(), juce::Justification::centred);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "LARVA 2   " + juce::String (Larva2::routingName (kp.routing)).toUpperCase());
            ColourBits::big (g, q.removeFromTop (28.0f), ColourBits::hz (live), juce::Colours::white, 19.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (Eng::WingMod::modeName (kp.mod.mode)) + (kp.mod.sync ? juce::String ("  ") + Eng::WingMod::syncName (kp.mod.syncIndex) : "  " + juce::String (kp.mod.rateHz, 2) + " Hz")
                                                          + "   " + (kp.freqModOct >= 0 ? "+" : "") + juce::String (kp.freqModOct, 1) + " oct");
            ColourBits::line (g, q.removeFromTop (16.0f), "Res " + juce::String (juce::roundToInt (kp.resonance * 100)) + " %   Mix " + juce::String (juce::roundToInt (kp.mix * 100)) + " %"
                                                          + (kp.analog > 0 ? "   Analog " + juce::String (juce::roundToInt (kp.analogAmount * 100)) + " %" : juce::String()));
            q.removeFromTop (6.0f);
            auto meter = q.removeFromTop (10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (meter, 3.0f);
            const float mv = juce::jlimit (-1.0f, 1.0f, mod);
            g.setColour (acc); g.fillRoundedRectangle (mv >= 0 ? meter.withTrimmedLeft (meter.getWidth() * 0.5f).withWidth (meter.getWidth() * 0.5f * mv)
                                                              : meter.withTrimmedLeft (meter.getWidth() * 0.5f * (1.0f + mv)).withWidth (meter.getWidth() * 0.5f * -mv), 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.4f)); g.drawVerticalLine ((int) meter.getCentreX(), meter.getY(), meter.getBottom());
            q.removeFromTop (6.0f);
            if (kp.mod.mode == Eng::WingMod::rhythm || kp.mod.mode == Eng::WingMod::step) Face111d::stepStrip (g, q.removeFromTop (34.0f), kp.mod, step, acc);
            else if (kp.mod.mode == Eng::WingMod::envelope || kp.mod.mode == Eng::WingMod::adsr)
            { auto e = q.removeFromTop (12.0f); g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (e, 3.0f); g.setColour (acc); g.fillRoundedRectangle (e.withWidth (e.getWidth() * juce::jlimit (0.0f, 1.0f, env)), 3.0f); }
        }
    };

    //==============================================================================
    /** Twelve Phase 2: the notch comb (where the cancellations sit right now), one pebble per allpass stage with the sweep running
        through them, and the style readout. */
    struct Phase2Viz : public Viz, private juce::Timer
    {
        Phase2Viz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ph (dynamic_cast<SqTwelvePhase2Plugin*> (&p)) { startTimerHz (30); }
        SqTwelvePhase2Plugin* ph; float sw = 0.5f, lim = 0;
        void timerCallback() override { if (ph == nullptr) return; sw += 0.5f * (ph->proc.sweepNow() - sw); lim = ph->proc.limiterDb(); repaint(); }
        static float combDb (const Phase2::Params& p, float f, float fc)
        {
            const int n = juce::jlimit (2, 24, p.stages);
            double ph = 0;
            for (int i = 0; i < n; ++i)
            {
                const double k = p.classic ? std::pow (1.0 + 0.35 * (i - n * 0.5) / n, 2.0) : 1.0;
                ph += -2.0 * std::atan (f / (fc * k));
            }
            // as the processor: (dry + depth x chain) / (1 + depth), then Mix against the dry; resonance as feedback around the chain
            const double sgn = p.polarity ? -1.0 : 1.0, depth = juce::jlimit (0.0f, 1.0f, p.depth), mix = juce::jlimit (0.0f, 1.0f, p.mix);
            const double fbAmt = juce::jlimit (0.0f, 1.0f, p.resonance) * (0.95 - 0.012 * n) * sgn;
            const std::complex<double> ap = std::polar (1.0, ph);
            const std::complex<double> wet = ap / (1.0 - fbAmt * ap);
            const std::complex<double> h = (1.0 + sgn * depth * wet) / (1.0 + depth);
            return (float) juce::Decibels::gainToDecibels (std::abs ((1.0 - mix) + mix * h), -60.0) + p.outputDb;
        }
        void paint (juce::Graphics& g) override
        {
            if (ph == nullptr) return;
            const auto acc = neon (accent); const auto kp = ph->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (12.0f, 8.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "NOTCHES NOW   +   " + juce::String (juce::jlimit (2, 24, kp.stages)) + " ALLPASS STAGES");
            auto stones = plot.removeFromBottom (30.0f); plot.removeFromBottom (4.0f);
            ColourBits::freqGrid (g, plot, 24.0f);
            const float fc = kp.centreHz * std::exp2 ((juce::jlimit (0.0f, 1.0f, sw) - 0.5f) * kp.sweepOct);
            auto c = ColourBits::curve (plot, 24.0f, [&] (float f) { return combDb (kp, f, fc); });
            ColourBits::fillUnder (g, c, plot, acc);
            g.setColour (acc); g.strokePath (c, juce::PathStrokeType (2.0f));
            // sweep range
            const float lo = Viz::xForHz (kp.centreHz * std::exp2 (-0.5f * kp.sweepOct), plot), hi = Viz::xForHz (kp.centreHz * std::exp2 (0.5f * kp.sweepOct), plot);
            g.setColour (acc.withAlpha (0.12f)); g.fillRect (juce::Rectangle<float> (lo, plot.getBottom() - 6, hi - lo, 6));
            g.setColour (juce::Colours::white.withAlpha (0.85f)); g.fillEllipse (Viz::xForHz (fc, plot) - 4, plot.getBottom() - 7, 8, 8);
            // the stages as pebbles; the light runs through them with the sweep
            const int n = juce::jlimit (2, 24, kp.stages); const float pw = stones.getWidth() / 24.0f;
            for (int i = 0; i < n; ++i)
            {
                const float ph01 = (float) i / (float) juce::jmax (1, n - 1);
                const float glow = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (ph01 - sw) * 3.0f);
                auto e = juce::Rectangle<float> (stones.getX() + (24 - n) * pw * 0.5f + i * pw, stones.getY() + 4, pw - 3, stones.getHeight() - 8);
                g.setColour (acc.withAlpha (0.22f + 0.6f * glow)); g.fillEllipse (e);
                g.setColour (juce::Colours::white.withAlpha (0.25f + 0.5f * glow)); g.drawEllipse (e, 1.0f);
            }
            if (kp.throughZero) { g.setColour (juce::Colours::white.withAlpha (0.8f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("THROUGH-ZERO", stones.toNearestInt(), juce::Justification::centredRight); }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "STYLE");
            const auto names = SqTwelvePhase2Plugin::styleNames();
            ColourBits::big (g, q.removeFromTop (26.0f), names[juce::jlimit (0, names.size() - 1, ph->style.get())], juce::Colours::white, 15.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "Centre " + ColourBits::hz (kp.centreHz) + "   Sweep " + juce::String (kp.sweepOct, 1) + " oct");
            ColourBits::line (g, q.removeFromTop (16.0f), "Res " + juce::String (juce::roundToInt (kp.resonance * 100)) + " % " + (kp.resMode == 0 ? "around" : "inside")
                                                          + "   Stereo " + juce::String (juce::roundToInt (kp.spreadDeg)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")));
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (Eng::WingMod::modeName (kp.mod.mode)) + (kp.mod.sync ? juce::String ("  ") + Eng::WingMod::syncName (kp.mod.syncIndex) : "  " + juce::String (kp.mod.rateHz, 2) + " Hz")
                                                          + (lim < -0.5f ? "   loop limiter " + juce::String (lim, 1) + " dB" : juce::String()));
            q.removeFromTop (6.0f);
            if (kp.mod.mode == Eng::WingMod::rhythm || kp.mod.mode == Eng::WingMod::step) Face111d::stepStrip (g, q.removeFromTop (30.0f), kp.mod, ph->proc.currentStep(), acc);
        }
    };

    //==============================================================================
    /** Hive Rack: the eight slots as a honeycomb (two lanes when parallel), each cell with its plug-in, preset and lock;
        click a cell to open that plug-in. The macros' targets and the rack meters run along the side. */
    struct HiveRackViz : public Viz, private juce::Timer
    {
        HiveRackViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), hr (dynamic_cast<SqHiveRackPlugin*> (&p)) { setInterceptsMouseClicks (true, false); startTimerHz (20); }
        SqHiveRackPlugin* hr; float in = 0, out = 0; int hover = -1;
        std::array<juce::Rectangle<float>, 8> cells;
        void timerCallback() override
        {
            if (hr == nullptr) return;
            in = juce::jmax (hr->levelIn.load(), in * 0.85f); out = juce::jmax (hr->levelOut.load(), out * 0.85f);
            repaint();
        }
        static juce::Path hexagon (juce::Rectangle<float> r)
        {
            juce::Path p; const auto c = r.getCentre(); const float rad = juce::jmin (r.getWidth() * 0.5f, r.getHeight() * 0.58f);
            for (int k = 0; k < 6; ++k) { const float a = juce::MathConstants<float>::pi / 3.0f * k; const juce::Point<float> pt (c.x + rad * std::cos (a), c.y + rad * 0.86f * std::sin (a)); if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt); }
            p.closeSubPath(); return p;
        }
        void mouseMove (const juce::MouseEvent& e) override { int h = -1; for (int i = 0; i < 8; ++i) if (cells[(size_t) i].contains (e.position)) h = i; if (h != hover) { hover = h; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (hr == nullptr) return;
            for (int i = 0; i < 8; ++i) if (cells[(size_t) i].contains (e.position)) if (auto* sp = hr->slotPlugin (i)) { sp->showWindowExplicitly(); return; }
        }
        void paint (juce::Graphics& g) override
        {
            if (hr == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (240.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 8.0f);
            const bool lanes = hr->routing.get() == 1;
            ColourBits::caption (g, plot.removeFromTop (14.0f), lanes ? "TWO LANES   (1-4 and 5-8 in parallel - click a cell to open it)" : "SERIAL   (1 > 8 - click a cell to open it)");
            // honeycomb: two rows of four, offset like a comb
            const float cw = plot.getWidth() / 4.6f, ch = plot.getHeight() / 2.0f;
            for (int i = 0; i < 8; ++i)
            {
                const int row = i / 4, col = (row == 1 && ! lanes) ? 3 - i % 4 : i % 4;   // serial snakes back along the second row
                cells[(size_t) i] = juce::Rectangle<float> (plot.getX() + col * cw * 1.08f + (row == 1 ? cw * 0.54f : 0.0f), plot.getY() + row * ch, cw, ch).reduced (4.0f);
                juce::ignoreUnused (col);
            }
            // signal path
            g.setColour (acc.withAlpha (0.45f));
            if (lanes) for (int ln = 0; ln < 2; ++ln) for (int i = 0; i < 3; ++i) g.drawLine (juce::Line<float> (cells[(size_t) (ln * 4 + i)].getCentre(), cells[(size_t) (ln * 4 + i + 1)].getCentre()), 1.6f);
            else for (int i = 0; i < 7; ++i) g.drawLine (juce::Line<float> (cells[(size_t) i].getCentre(), cells[(size_t) i + 1].getCentre()), 1.6f);
            for (int i = 0; i < 8; ++i)
            {
                const auto si = hr->slotInfo (i); const auto& c = cells[(size_t) i];
                auto hex = hexagon (c);
                const bool full = si.name.isNotEmpty();
                const auto fill = ! full ? juce::Colours::white.withAlpha (0.04f) : (! si.allowed ? juce::Colour (0xff3a3a3a) : acc.withAlpha (i == hover ? 0.5f : 0.3f));
                g.setGradientFill (juce::ColourGradient (fill.brighter (0.2f), c.getX(), c.getY(), fill.darker (0.4f), c.getX(), c.getBottom(), false)); g.fillPath (hex);
                g.setColour (full ? acc : juce::Colours::white.withAlpha (0.18f)); g.strokePath (hex, juce::PathStrokeType (i == hover ? 2.4f : 1.4f));
                auto t = c.reduced (c.getWidth() * 0.16f, c.getHeight() * 0.2f);
                g.setColour (juce::Colours::white.withAlpha (0.45f)); g.setFont (Theme::font (8.0f, true));
                g.drawText (juce::String (i + 1), t.removeFromTop (11.0f).toNearestInt(), juce::Justification::centred);
                g.setColour (juce::Colours::white.withAlpha (full ? 0.95f : 0.35f)); g.setFont (Theme::font (10.5f, true));
                g.drawFittedText (full ? si.name : juce::String ("empty"), t.removeFromTop (28.0f).toNearestInt(), juce::Justification::centred, 2);
                g.setFont (Theme::font (8.0f)); g.setColour (juce::Colours::white.withAlpha (0.6f));
                if (! si.allowed) g.drawFittedText ("Locked - " + si.lockedBy, t.toNearestInt(), juce::Justification::centredTop, 2);
                else if (si.preset.isNotEmpty()) g.drawFittedText (si.preset.fromLastOccurrenceOf ("| ", false, false), t.toNearestInt(), juce::Justification::centredTop, 2);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "HIVE RACK");
            const juce::String chainName = hr->loadedChain >= 0 && hr->loadedChain < (int) HiveChains::chains().size() ? juce::String (HiveChains::chains()[(size_t) hr->loadedChain].name) : juce::String ("Custom rack");
            ColourBits::big (g, q.removeFromTop (24.0f), chainName.fromLastOccurrenceOf ("| ", false, false), juce::Colours::white, 14.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), chainName.contains ("|") ? chainName.upToFirstOccurrenceOf (" |", false, false) : juce::String ("Your own chain"));
            const float rc = hr->bank[SqHiveRackPlugin::recycle];
            ColourBits::line (g, q.removeFromTop (16.0f), rc > 0.5f ? "Recycle " + juce::String (juce::roundToInt (rc)) + " %  " + (hr->syncOn.get() ? juce::String ("synced") : juce::String (juce::roundToInt (hr->bank[SqHiveRackPlugin::recycleTime])) + " ms")
                                                                     : juce::String ("Recycle off"));
            const auto ts = hr->macroTargets();
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String ((int) ts.size()) + " macro target" + (ts.size() == 1 ? "" : "s") + "   latency " + juce::String (hr->getLatencySeconds() * 1000.0, 1) + " ms");
            q.removeFromTop (6.0f);
            auto meter = [&] (const char* lab, float v)
            {
                auto row = q.removeFromTop (12.0f); q.removeFromTop (4.0f);
                g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (Theme::font (8.0f, true)); g.drawText (lab, row.removeFromLeft (28.0f).toNearestInt(), juce::Justification::centredLeft);
                g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (row, 3.0f);
                const float db = juce::Decibels::gainToDecibels (v, -60.0f);
                g.setColour (db > -1.0f ? juce::Colour (0xffff5a4a) : acc); g.fillRoundedRectangle (row.withWidth (row.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f)), 3.0f);
            };
            meter ("IN", in); meter ("OUT", out);
        }
    };
