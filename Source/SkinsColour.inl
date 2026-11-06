// Included inside namespace Skins (PluginSkins.h): faces for the colour plug-ins (SqColour.h).

    /** Shared bits: dark glass panel with an accent wash, small caption, value readout. */
    struct ColourBits
    {
        static void panel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour acc, bool flip = false)
        {
            g.setGradientFill (flip ? juce::ColourGradient (acc.darker (1.7f), r.getX(), r.getY(), juce::Colour (0xff0b0716), r.getRight(), r.getBottom(), false)
                                    : juce::ColourGradient (juce::Colour (0xff0b0716), r.getX(), r.getY(), acc.darker (2.2f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (acc.withAlpha (0.35f)); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
        static void caption (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c = juce::Colours::white.withAlpha (0.65f))
        {
            g.setColour (c); g.setFont (Theme::font (9.5f, true)); g.drawText (t, r, juce::Justification::left);
        }
        static void big (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c = juce::Colours::white, float size = 30.0f)
        {
            g.setColour (c); g.setFont (Theme::font (size, true)); g.drawText (t, r, juce::Justification::left);
        }
        static void line (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t)
        {
            g.setColour (juce::Colours::white.withAlpha (0.82f)); g.setFont (Theme::font (10.0f)); g.drawText (t, r, juce::Justification::left);
        }
        static juce::String hz (float f)  { return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : (f < 10.0f ? juce::String (f, 2) : juce::String (juce::roundToInt (f))) + " Hz"; }
        /** dB grid on a 20 Hz - 20 kHz log axis. */
        static void freqGrid (juce::Graphics& g, juce::Rectangle<float> r, float dbRange)
        {
            g.setFont (Theme::font (8.5f));
            for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
            {
                const float x = Viz::xForHz (f, r);
                g.setColour (juce::Colours::white.withAlpha (f == 100.0f || f == 1000.0f || f == 10000.0f ? 0.16f : 0.07f));
                g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                g.setColour (juce::Colours::white.withAlpha (0.55f));
                if (f > 20.0f && f < 20000.0f) g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x + 2, (int) r.getBottom() - 12, 28, 11, juce::Justification::left);
            }
            const float step = dbRange >= 18.0f ? 6.0f : 3.0f;
            for (float d = -dbRange; d <= dbRange + 0.01f; d += step)
            {
                const float y = r.getCentreY() - d / dbRange * r.getHeight() * 0.5f;
                g.setColour (juce::Colours::white.withAlpha (d == 0.0f ? 0.22f : 0.07f));
                g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                if (std::abs (d) < dbRange) g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), (int) r.getX() + 3, (int) y - 11, 30, 10, juce::Justification::left);
            }
        }
        static juce::Path curve (juce::Rectangle<float> r, float dbRange, const std::function<float (float)>& dbAt)
        {
            juce::Path p;
            for (int i = 0; i <= (int) r.getWidth(); i += 2)
            {
                const float x = r.getX() + (float) i;
                const float y = r.getCentreY() - juce::jlimit (-dbRange, dbRange, dbAt (Viz::hzForX (x, r))) / dbRange * r.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        }
        static void fillUnder (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> r, juce::Colour c)
        {
            juce::Path f (p);
            f.lineTo (r.getRight(), r.getCentreY()); f.lineTo (r.getX(), r.getCentreY()); f.closeSubPath();
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.38f), r.getX(), r.getY(), c.withAlpha (0.05f), r.getX(), r.getCentreY(), false));
            g.fillPath (f);
        }
    };

    //==============================================================================
    /** Aurora Verb: three-band decay envelopes with the early-reflection pattern, and a drifting particle field. */
    struct AuroraViz : public Viz
    {
        AuroraViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ar (dynamic_cast<SqAuroraPlugin*> (&p)) {}
        SqAuroraPlugin* ar;
        void paint (juce::Graphics& g) override
        {
            if (ar == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (210.0f);
            r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            // particle field (the "aurora"), brighter with the reverb's output
            const float act = juce::jlimit (0.0f, 1.0f, ar->activity.load() * 3.0f);
            for (int i = 0; i < 70; ++i)
            {
                const float sx = std::fmod ((float) i * 0.6180339f + phase * 0.01f * (1.0f + (float) (i % 5) * 0.2f), 1.0f);
                const float sy = 0.5f + 0.42f * std::sin ((float) i * 1.7f + phase * 0.12f + sx * 4.0f);
                const auto col = juce::Colour::fromHSV (std::fmod (0.52f + sx * 0.35f, 1.0f), 0.85f, 1.0f, 0.10f + 0.35f * act * (0.4f + 0.6f * sx));
                const float rad = 1.2f + 2.5f * sx;
                g.setColour (col); g.fillEllipse (r.getX() + sx * r.getWidth() - rad, r.getY() + sy * r.getHeight() - rad, rad * 2, rad * 2);
            }
            auto plot = r.reduced (12.0f, 14.0f);
            const float rt = ar->bank[SqAuroraPlugin::decay], lowT = rt * ar->bank[SqAuroraPlugin::lowDecay], highT = rt * ar->bank[SqAuroraPlugin::highDecay];
            const float pre = ar->bank[SqAuroraPlugin::preDelay] * 0.001f;
            const float span = juce::jmax (0.5f, pre + juce::jmax (rt, lowT) * 1.15f);
            auto xT = [&] (float t) { return plot.getX() + t / span * plot.getWidth(); };
            auto yDb = [&] (float db) { return plot.getY() + juce::jlimit (0.0f, 1.0f, -db / 66.0f) * plot.getHeight(); };
            // time grid
            g.setFont (Theme::font (8.5f));
            const float tick = span > 8 ? 2.0f : span > 3 ? 1.0f : span > 1.2f ? 0.25f : 0.1f;
            for (float t = 0; t <= span; t += tick)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawVerticalLine ((int) xT (t), plot.getY(), plot.getBottom());
                g.setColour (juce::Colours::white.withAlpha (0.5f)); g.drawText ((tick < 1 ? juce::String (t, 2) : juce::String (juce::roundToInt (t))) + " s", (int) xT (t) + 2, (int) plot.getBottom() - 11, 40, 10, juce::Justification::left);
            }
            for (float db : { -60.0f, -40.0f, -20.0f }) { g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawHorizontalLine ((int) yDb (db), plot.getX(), plot.getRight()); }
            // early reflections
            const auto& room = SqAuroraPlugin::rooms()[(size_t) juce::jlimit (0, 7, ar->roomType.get())];
            const float erDb = ar->bank[SqAuroraPlugin::early];
            static const int erTapsMs10[12] = { 43, 79, 113, 157, 197, 241, 283, 331, 373, 419, 467, 521 };
            const float sz = ar->bank[SqAuroraPlugin::size] / 100.0f * room.sizeMul;
            if (erDb > -59.0f)
                for (int k = 0; k < 12; ++k)
                {
                    const float t = pre + erTapsMs10[k] * 0.0001f * room.erSpread * juce::jlimit (0.4f, 2.0f, sz);
                    const float db = erDb + Apex::gainToDb (room.er[k]) - 4.0f;
                    g.setColour (juce::Colour (0xffffe37a).withAlpha (0.85f));
                    g.drawLine (xT (t), yDb (db), xT (t), plot.getBottom(), 2.0f);
                }
            // three decay bands
            struct Band { float t; juce::Colour c; const char* name; };
            const Band bands[] = { { lowT, juce::Colour (0xffff5ad1), "LOW" }, { rt, acc, "MID" }, { highT, juce::Colour (0xff3affd8), "HIGH" } };
            const float tailDb = ar->bank[SqAuroraPlugin::tail];
            const bool frz = ar->freeze.get();
            for (auto& b : bands)
            {
                juce::Path p;
                p.startNewSubPath (xT (0), plot.getBottom());
                p.lineTo (xT (pre), plot.getBottom());
                p.lineTo (xT (pre + 0.02f), yDb (tailDb - 6.0f));
                for (float t = 0.02f; pre + t <= span; t += span / 200.0f)
                    p.lineTo (xT (pre + t), yDb (frz ? tailDb - 6.0f : tailDb - 6.0f - 60.0f * t / juce::jmax (0.05f, b.t)));
                juce::Path f (p); f.lineTo (plot.getRight(), plot.getBottom()); f.closeSubPath();
                g.setGradientFill (juce::ColourGradient (b.c.withAlpha (0.28f), plot.getX(), plot.getY(), b.c.withAlpha (0.02f), plot.getX(), plot.getBottom(), false));
                g.fillPath (f);
                glowStroke (g, p, b.c, 1.8f);
            }
            g.setFont (Theme::font (9.0f, true));
            for (int i = 0; i < 3; ++i) { g.setColour (bands[i].c); g.drawText (bands[i].name, (int) plot.getRight() - 150 + i * 50, (int) plot.getY(), 48, 12, juce::Justification::right); }
            // readout
            ColourBits::panel (g, read, acc, true);
            auto p = read.reduced (12.0f);
            ColourBits::caption (g, p.removeFromTop (14.0f), frz ? "FROZEN" : "ROOM");
            ColourBits::big (g, p.removeFromTop (38.0f), room.name, frz ? juce::Colour (0xff9fe8ff) : juce::Colours::white, 28.0f);
            ColourBits::line (g, p.removeFromTop (16.0f), "RT60 " + juce::String (rt, 2) + " s");
            ColourBits::line (g, p.removeFromTop (16.0f), "low " + juce::String (lowT, 2) + " s  /  high " + juce::String (highT, 2) + " s");
            ColourBits::line (g, p.removeFromTop (16.0f), "pre-delay " + juce::String (juce::roundToInt (ar->bank[SqAuroraPlugin::preDelay])) + " ms");
            ColourBits::line (g, p.removeFromTop (16.0f), "band " + ColourBits::hz (ar->bank[SqAuroraPlugin::lowCut]) + " - " + ColourBits::hz (ar->bank[SqAuroraPlugin::highCut]));
            if (ar->bank[SqAuroraPlugin::duck] > 0.05f) ColourBits::line (g, p.removeFromTop (16.0f), "ducking " + juce::String (ar->bank[SqAuroraPlugin::duck], 1) + " dB");
        }
    };

    //==============================================================================
    /** Opto Leveler: a real-looking VU meter (gain reduction or output) with the glowing optical cell. */
    struct OptoViz : public Viz
    {
        OptoViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), op (dynamic_cast<SqOptoPlugin*> (&p)) {}
        SqOptoPlugin* op;
        float needle = 0;
        void paint (juce::Graphics& g) override
        {
            if (op == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto cell = r.removeFromRight (200.0f);
            r.removeFromRight (8.0f);
            // the meter face: warm backlit paper
            auto face = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight() * 2.1f), r.getHeight());
            g.setColour (juce::Colour (0xff17110a)); g.fillRoundedRectangle (face, 8.0f);
            auto paper = face.reduced (10.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff1c4), paper.getCentreX(), paper.getBottom(), juce::Colour (0xffe8b85a), paper.getCentreX(), paper.getY(), true));
            g.fillRoundedRectangle (paper, 5.0f);
            const bool grMode = op->meterMode.get() == 0;
            // VU scale: -20 .. +3 VU, mapped to an arc
            const juce::Point<float> pivot (paper.getCentreX(), paper.getBottom() + paper.getHeight() * 0.35f);
            const float rad = paper.getHeight() * 1.05f;
            auto angFor = [] (float vu) { const float t = juce::jlimit (0.0f, 1.0f, (std::pow (10.0f, vu / 20.0f) - 0.1f) / (1.413f - 0.1f)); return juce::jmap (t, -0.82f, 0.82f); };
            g.setFont (Theme::font (10.0f, true));
            for (float vu : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
            {
                const float a = angFor (vu);
                const auto p1 = pivot.getPointOnCircumference (rad * 0.86f, a), p2 = pivot.getPointOnCircumference (rad * (vu == 0 || vu == -10 || vu == -20 || vu == 3 ? 0.95f : 0.92f), a);
                g.setColour (vu > 0 ? juce::Colour (0xffd0221a) : juce::Colour (0xff2a1a0a));
                g.drawLine ({ p1, p2 }, 1.6f);
                const auto pt = pivot.getPointOnCircumference (rad * 1.0f, a);
                const int shown = grMode ? (int) vu : (int) vu;
                g.drawText (juce::String (shown > 0 ? "+" : "") + juce::String (shown), (int) pt.x - 14, (int) pt.y - 8, 28, 12, juce::Justification::centred);
            }
            juce::Path redArc; redArc.addCentredArc (pivot.x, pivot.y, rad * 0.89f, rad * 0.89f, 0, angFor (0), angFor (3), true);
            g.setColour (juce::Colour (0xffd0221a)); g.strokePath (redArc, juce::PathStrokeType (5.0f));
            g.setColour (juce::Colour (0xff2a1a0a)); g.setFont (Theme::font (16.0f, true));
            g.drawText ("VU", paper.withTrimmedTop (paper.getHeight() * 0.52f).removeFromTop (20.0f), juce::Justification::centred);
            g.setFont (Theme::font (9.0f, true));
            g.drawText (grMode ? "GAIN REDUCTION" : "OUTPUT", paper.withTrimmedTop (paper.getHeight() * 0.68f).removeFromTop (14.0f), juce::Justification::centred);
            // needle with ballistics: GR mode rests at 0 and swings left
            const float target = grMode ? op->grDb.load() : juce::jlimit (-22.0f, 4.0f, op->outVu.load());
            needle += 0.35f * (target - needle);
            const float a = angFor (juce::jlimit (-22.0f, 4.0f, needle));
            const auto tip = pivot.getPointOnCircumference (rad * 0.97f, a);
            const auto base = pivot.getPointOnCircumference (rad * 0.42f, a);
            g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawLine (base.x + 2, base.y + 2, tip.x + 2, tip.y + 2, 2.0f);
            g.setColour (juce::Colour (0xff120a04)); g.drawLine ({ base, tip }, 1.8f);
            g.setColour (juce::Colour (0xff17110a)); g.fillRect (paper.withTop (paper.getBottom() - paper.getHeight() * 0.16f));
            // the optical cell: an electroluminescent panel shining onto a photo-resistor
            ColourBits::panel (g, cell, acc, true);
            auto p = cell.reduced (12.0f);
            ColourBits::caption (g, p.removeFromTop (14.0f), op->limitMode.get() ? "LIMIT" : "COMPRESS");
            const float gr = -op->grDb.load();
            ColourBits::big (g, p.removeFromTop (36.0f), juce::String (gr, 1) + " dB", juce::Colours::white, 26.0f);
            ColourBits::line (g, p.removeFromTop (16.0f), "gain reduction");
            p.removeFromTop (8.0f);
            auto lamp = p.removeFromTop (juce::jmin (p.getHeight(), 70.0f)).withSizeKeepingCentre (70.0f, juce::jmin (p.getHeight(), 70.0f));
            const float glow = juce::jlimit (0.0f, 1.0f, gr / 12.0f);
            const auto lc = juce::Colour (0xff6affef).interpolatedWith (acc, 0.3f);
            for (int i = 6; i >= 1; --i)
            {
                g.setColour (lc.withAlpha (0.05f * glow * (float) (7 - i)));
                g.fillEllipse (lamp.expanded ((float) i * 4.0f));
            }
            g.setGradientFill (juce::ColourGradient (lc.withAlpha (0.25f + 0.75f * glow), lamp.getCentreX(), lamp.getCentreY(), lc.darker (1.5f).withAlpha (0.6f), lamp.getRight(), lamp.getBottom(), true));
            g.fillEllipse (lamp);
            g.setColour (juce::Colours::white.withAlpha (0.3f)); g.drawEllipse (lamp, 1.0f);
        }
    };

    //==============================================================================
    /** Pulse EQ: the exact response of the passive network and mid section, with each section's corner marked. */
    struct PulseViz : public Viz
    {
        PulseViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), eq (dynamic_cast<SqPulseEqPlugin*> (&p)) {}
        SqPulseEqPlugin* eq;
        void paint (juce::Graphics& g) override
        {
            if (eq == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (10.0f, 8.0f);
            const float range = 18.0f;
            ColourBits::freqGrid (g, plot, range);
            const double rate = 48000.0;
            Apex::Biquad f[SqPulseEqPlugin::numFilters];
            eq->design (rate, f);
            const juce::Colour sc[] = { juce::Colour (0xffff4fd8), juce::Colour (0xffff4fd8), juce::Colour (0xffff4fd8), juce::Colour (0xffffd23a), juce::Colour (0xffff7a3a), juce::Colour (0xff7aff5a), juce::Colour (0xff3ad1ff), juce::Colour (0xffb06aff) };
            // each section faintly
            for (int k = 0; k < SqPulseEqPlugin::numFilters; ++k)
            {
                auto p = ColourBits::curve (plot, range, [&] (float hz) { return (float) f[k].dbAt (rate, hz); });
                g.setColour (sc[k].withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (1.0f));
            }
            const float outDb = eq->bank[SqPulseEqPlugin::output];
            auto total = ColourBits::curve (plot, range, [&] (float hz) { float d = outDb; for (auto& b : f) d += (float) b.dbAt (rate, hz); return d; });
            ColourBits::fillUnder (g, total, plot, acc);
            glowStroke (g, total, acc, 2.2f);
            // corner markers
            auto pick = [] (const std::vector<float>& v, int i) { return v[(size_t) juce::jlimit (0, (int) v.size() - 1, i)]; };
            struct M { float hz; const char* t; juce::Colour c; };
            const M marks[] = { { pick (SqPulseEqPlugin::lowFreqs(), eq->lowFreq.get()), "LF", sc[0] }, { pick (SqPulseEqPlugin::highFreqs(), eq->highFreq.get()), "HF", sc[3] },
                                { pick (SqPulseEqPlugin::attenFreqs(), eq->attenFreq.get()), "ATT", sc[4] }, { pick (SqPulseEqPlugin::midLowFreqs(), eq->midLowFreq.get()), "PK", sc[5] },
                                { pick (SqPulseEqPlugin::dipFreqs(), eq->dipFreq.get()), "DIP", sc[6] }, { pick (SqPulseEqPlugin::midHighFreqs(), eq->midHighFreq.get()), "PK", sc[7] } };
            g.setFont (Theme::font (9.0f, true));
            int row = 0;
            for (auto& m : marks)
            {
                const float x = xForHz (juce::jmin (m.hz, 19500.0f), plot);
                g.setColour (m.c.withAlpha (0.55f)); g.drawVerticalLine ((int) x, plot.getY() + 14.0f, plot.getBottom() - 14.0f);
                g.setColour (m.c); g.fillEllipse (x - 3.5f, plot.getY() + 4.0f + (float) (row % 2) * 12.0f, 7.0f, 7.0f);
                g.drawText (m.t, (int) x + 5, (int) plot.getY() + 1 + (row % 2) * 12, 30, 12, juce::Justification::left);
                ++row;
            }
            // the low-end trick readout
            const float lfHz = pick (SqPulseEqPlugin::lowFreqs(), eq->lowFreq.get()) * 0.7f;
            const float lo = (float) (f[0].dbAt (rate, lfHz) + f[1].dbAt (rate, lfHz) + f[2].dbAt (rate, lfHz));
            g.setColour (juce::Colours::white.withAlpha (0.75f)); g.setFont (Theme::font (10.0f, true));
            g.drawText ("LOW END " + juce::String (lo >= 0 ? "+" : "") + juce::String (lo, 1) + " dB", plot.removeFromTop (14.0f).removeFromRight (180.0f), juce::Justification::right);
        }
    };

    //==============================================================================
    /** Oil Can Delay: the spinning electrostatic drum with its echo trail. */
    struct OilCanViz : public Viz
    {
        OilCanViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), oc (dynamic_cast<SqOilCanPlugin*> (&p)) {}
        SqOilCanPlugin* oc;
        void paint (juce::Graphics& g) override
        {
            if (oc == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            ColourBits::panel (g, r, acc);
            auto drum = r.removeFromLeft (r.getHeight()).reduced (12.0f);
            const auto c = drum.getCentre();
            const float rad = drum.getWidth() * 0.5f;
            // the can
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a3420), c.x - rad, c.y - rad, juce::Colour (0xff120c06), c.x + rad, c.y + rad, false));
            g.fillEllipse (drum);
            const float spin = oc->canPhase.load();
            const float var = oc->bank[SqOilCanPlugin::variation] / 100.0f;
            // oil sheen bands (wobbling with the variation)
            for (int i = 0; i < 18; ++i)
            {
                const float a = (float) i / 18.0f * juce::MathConstants<float>::twoPi + spin * juce::MathConstants<float>::twoPi;
                const float wob = 1.0f + 0.05f * var * std::sin (a * 3.0f + phase);
                juce::Path arc; arc.addCentredArc (c.x, c.y, rad * 0.82f * wob, rad * 0.82f * wob, 0, a, a + 0.22f, true);
                g.setColour (juce::Colour::fromHSV (std::fmod (0.08f + (float) i * 0.04f, 1.0f), 0.9f, 1.0f, 0.55f));
                g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (acc.withAlpha (0.8f)); g.drawEllipse (drum, 2.0f);
            g.setColour (juce::Colour (0xff0b0716)); g.fillEllipse (drum.reduced (rad * 0.55f));
            g.setColour (acc); g.setFont (Theme::font (9.0f, true));
            g.drawText (oc->mode.get() == 1 ? "DOUBLER" : "ECHO", drum.reduced (rad * 0.55f), juce::Justification::centred);
            // echo trail
            r.removeFromLeft (10.0f);
            auto trail = r.reduced (8.0f);
            auto label = trail.removeFromRight (150.0f);
            const float fb = oc->mode.get() == 1 ? 0.0f : oc->bank[SqOilCanPlugin::sustain] / 100.0f * 0.92f;
            const float act = juce::jlimit (0.0f, 1.0f, oc->activity.load() * 3.0f);
            float amp = 1.0f;
            for (int k = 0; k < 10 && amp > 0.03f; ++k)
            {
                const float x = trail.getX() + trail.getWidth() * ((float) k + 0.5f) / 10.0f;
                const float h = trail.getHeight() * 0.8f * amp;
                const auto col = acc.interpolatedWith (juce::Colour (0xffff3a5a), (float) k / 10.0f).withAlpha (0.35f + 0.6f * amp * (0.5f + 0.5f * act));
                juce::Path blob;
                for (int i = 0; i <= 24; ++i)
                {
                    const float t = (float) i / 24.0f, y = trail.getCentreY() + (t - 0.5f) * h;
                    const float xx = x + 6.0f * var * std::sin (t * 9.0f + phase * 2.0f + (float) k);
                    if (i == 0) blob.startNewSubPath (xx, y); else blob.lineTo (xx, y);
                }
                glowStroke (g, blob, col, 3.0f);
                amp *= (k == 0 ? 0.8f : juce::jmax (0.0f, fb));
                if (fb <= 0.001f && k >= 0) break;
            }
            label = label.reduced (6.0f, 4.0f);
            ColourBits::caption (g, label.removeFromTop (14.0f), oc->sync.get() > 0 ? "SYNCED" : "FREE", acc);
            ColourBits::line (g, label.removeFromBottom (16.0f), "sustain " + juce::String (juce::roundToInt (oc->bank[SqOilCanPlugin::sustain])) + " %  wobble " + juce::String (oc->bank[SqOilCanPlugin::wobbleRate], 1) + " Hz");
            static const char* syncNm[] = { "FREE", "1/16", "1/8", "1/8 D", "1/4", "1/2" };
            if (oc->sync.get() > 0 && oc->mode.get() == 0) ColourBits::big (g, label.removeFromTop (30.0f), syncNm[juce::jlimit (0, 5, oc->sync.get())], juce::Colours::white, 22.0f);
            else ColourBits::big (g, label.removeFromTop (30.0f), juce::String (juce::roundToInt (oc->mode.get() == 1 ? 25.0f + oc->bank[SqOilCanPlugin::delayMs] * 0.05f : oc->bank[SqOilCanPlugin::delayMs])) + " ms", juce::Colours::white, 22.0f);
        }
    };

    //==============================================================================
    /** Silk Ladder: the filter's response at the live (envelope-moved) cutoff. */
    struct LadderViz : public Viz
    {
        LadderViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ld (dynamic_cast<SqLadderPlugin*> (&p)) {}
        SqLadderPlugin* ld;
        static float respDb (float hz, float fc, float res, int poles)
        {
            // one-pole stages H = 1 / (1 + jw), loop gain -res * H^poles
            const std::complex<float> s (0.0f, hz / fc);
            const std::complex<float> h1 = 1.0f / (1.0f + s);
            std::complex<float> hp = 1.0f; for (int i = 0; i < poles; ++i) hp *= h1;
            const auto H = hp / (1.0f + res * hp) * (1.0f + res * 0.7f);
            return 20.0f * std::log10 (std::abs (H) + 1e-9f);
        }
        void paint (juce::Graphics& g) override
        {
            if (ld == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (180.0f);
            r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (10.0f, 8.0f);
            ColourBits::freqGrid (g, plot, 24.0f);
            const int poles = ld->poles.get() == 1 ? 4 : 2;
            const float res = ld->bank[SqLadderPlugin::resonance] / 100.0f * 4.0f * (poles == 4 ? 1.0f : 0.5f);
            const float base = ld->bank[SqLadderPlugin::cutoff], live = ld->liveCutoff.load();
            auto ghost = ColourBits::curve (plot, 24.0f, [&] (float hz) { return respDb (hz, base, res, poles); });
            g.setColour (juce::Colours::white.withAlpha (0.25f)); g.strokePath (ghost, juce::PathStrokeType (1.2f));
            auto p = ColourBits::curve (plot, 24.0f, [&] (float hz) { return respDb (hz, live, res, poles); });
            ColourBits::fillUnder (g, p, plot, acc);
            glowStroke (g, p, acc, 2.2f);
            const float x = xForHz (live, plot);
            g.setColour (juce::Colour (0xffffe14a).withAlpha (0.7f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), juce::String (poles) + "-POLE LADDER");
            ColourBits::big (g, q.removeFromTop (34.0f), ColourBits::hz (live), juce::Colours::white, 22.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "knob " + ColourBits::hz (base));
            ColourBits::line (g, q.removeFromTop (16.0f), "envelope " + juce::String (juce::roundToInt (ld->bank[SqLadderPlugin::envAmount])) + " %" + (ld->envFast.get() ? " (fast)" : ""));
            ColourBits::line (g, q.removeFromTop (16.0f), "slope " + juce::String (poles * 6) + " dB/oct");
        }
    };

    //==============================================================================
    /** Halo Ring: the carrier, an example input and the sidebands they make. */
    struct RingViz : public Viz
    {
        RingViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), rm (dynamic_cast<SqRingPlugin*> (&p)) {}
        SqRingPlugin* rm;
        void paint (juce::Graphics& g) override
        {
            if (rm == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (180.0f);
            r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto w = r.reduced (12.0f, 10.0f);
            const float f = rm->liveFreq.load();
            const float cyc = juce::jlimit (1.0f, 40.0f, f / 40.0f);
            auto top = w.removeFromTop (w.getHeight() * 0.3f), mid = w.removeFromTop (w.getHeight() * 0.42f);
            drawWave (g, top, [this, cyc] (float t) { return 0.9f * std::sin (juce::MathConstants<float>::twoPi * (t * cyc + phase * 0.2f)); }, juce::Colour (0xffffe14a).withAlpha (0.9f), 1.4f);
            drawWave (g, w, [this] (float t) { return 0.9f * std::sin (juce::MathConstants<float>::twoPi * (t * 3.0f + phase * 0.05f)); }, juce::Colour (0xff3ad1ff).withAlpha (0.8f), 1.4f);
            drawWave (g, mid, [this, cyc] (float t) { return 0.95f * std::sin (juce::MathConstants<float>::twoPi * (t * cyc + phase * 0.2f)) * std::sin (juce::MathConstants<float>::twoPi * (t * 3.0f + phase * 0.05f)); }, acc, 2.4f);
            ColourBits::caption (g, top.removeFromTop (12.0f), "CARRIER", juce::Colour (0xffffe14a));
            ColourBits::caption (g, mid.removeFromTop (12.0f), "OUTPUT", acc);
            ColourBits::caption (g, w.removeFromTop (12.0f), "INPUT", juce::Colour (0xff3ad1ff));
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "FREQUENCY");
            ColourBits::big (g, q.removeFromTop (34.0f), ColourBits::hz (f), juce::Colours::white, 22.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "A4 in gives " + ColourBits::hz (std::abs (440.0f - f)));
            ColourBits::line (g, q.removeFromTop (16.0f), "and " + ColourBits::hz (440.0f + f));
            if (rm->bank[SqRingPlugin::lfoAmount] > 0.5f) ColourBits::line (g, q.removeFromTop (16.0f), "LFO " + juce::String (rm->bank[SqRingPlugin::lfoRate], 2) + " Hz " + (rm->lfoShape.get() == 1 ? "square" : "sine"));
        }
    };

    //==============================================================================
    /** Twelve Phase: the live notch pattern as the sweep moves, plus the stage chain. */
    struct Phase12Viz : public Viz
    {
        Phase12Viz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ph (dynamic_cast<SqPhase12Plugin*> (&p)) {}
        SqPhase12Plugin* ph;
        void paint (juce::Graphics& g) override
        {
            if (ph == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            ColourBits::panel (g, r, acc);
            auto chain = r.removeFromBottom (30.0f).reduced (12.0f, 4.0f);
            auto plot = r.reduced (10.0f, 8.0f);
            ColourBits::freqGrid (g, plot, 24.0f);
            const int ns = SqPhase12Plugin::stageCount (ph->stages.get());
            const float sw = ph->bank[SqPhase12Plugin::sweep] / 100.0f, cen = ph->bank[SqPhase12Plugin::centre], fb = ph->bank[SqPhase12Plugin::resonance] / 100.0f;
            const float mix = ph->bank[SqPhase12Plugin::mix] / 100.0f;
            const float l = ph->liveSweep.load();
            const float fc = cen * std::pow (2.0f, (l - 0.5f) * sw * 6.0f);
            auto resp = [&] (float hz)
            {
                const float phiOne = -2.0f * std::atan (hz / fc);
                const std::complex<float> ap = std::polar (1.0f, phiOne * (float) ns);
                const std::complex<float> wet = ap / (1.0f - fb * ap);
                const auto y = (1.0f - mix * 0.5f) + wet * (mix * 0.5f);
                return 20.0f * std::log10 (std::abs (y) + 1e-6f);
            };
            auto p = ColourBits::curve (plot, 24.0f, resp);
            ColourBits::fillUnder (g, p, plot, acc);
            glowStroke (g, p, acc, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.8f)); g.setFont (Theme::font (10.0f, true));
            g.drawText (juce::String (ns) + " STAGES  -  " + juce::String (ns / 2) + " NOTCHES", plot.removeFromTop (14.0f).removeFromRight (240.0f), juce::Justification::right);
            // stage chain lights, rippling with the sweep
            const float cw = chain.getWidth() / 12.0f;
            for (int k = 0; k < 12; ++k)
            {
                auto b = juce::Rectangle<float> (chain.getX() + cw * (float) k, chain.getY(), cw - 4.0f, chain.getHeight());
                const bool on = k < ns;
                const float lit = on ? 0.45f + 0.55f * (0.5f + 0.5f * std::sin (phase * 0.6f + (float) k * 0.5f + l * 6.0f)) : 0.0f;
                g.setColour (on ? acc.withAlpha (lit) : juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (b, 3.0f);
                g.setColour (juce::Colours::white.withAlpha (on ? 0.8f : 0.25f)); g.setFont (Theme::font (8.5f, true));
                g.drawText (juce::String (k + 1), b, juce::Justification::centred);
            }
        }
    };

    //==============================================================================
    /** Bucket Brigade: the echo train, getting darker with each repeat, and the bucket chain. */
    struct BucketViz : public Viz
    {
        BucketViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), bb (dynamic_cast<SqBucketPlugin*> (&p)) {}
        SqBucketPlugin* bb;
        void paint (juce::Graphics& g) override
        {
            if (bb == nullptr) return;
            const auto acc = neon (accent);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (180.0f);
            r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto chain = r.removeFromBottom (26.0f).reduced (12.0f, 4.0f);
            auto plot = r.reduced (12.0f, 10.0f);
            const float fb = bb->bank[SqBucketPlugin::feedback] / 100.0f * 0.95f;
            const float act = juce::jlimit (0.0f, 1.0f, bb->activity.load() * 3.0f);
            float amp = 1.0f;
            const int n = 12;
            for (int k = 0; k < n && amp > 0.02f; ++k)
            {
                const float x = plot.getX() + plot.getWidth() * ((float) k + 0.5f) / (float) n;
                const float h = plot.getHeight() * amp;
                const float dark = (float) k / (float) n;
                const auto col = acc.interpolatedWith (juce::Colour (0xff6a2aff), dark).withMultipliedBrightness (1.0f - 0.45f * dark);
                g.setGradientFill (juce::ColourGradient (col.withAlpha (0.95f), x, plot.getBottom() - h, col.withAlpha (0.15f), x, plot.getBottom(), false));
                g.fillRoundedRectangle (x - 7.0f + 2.0f * dark, plot.getBottom() - h, 14.0f - 4.0f * dark, h, 4.0f);
                if (k == 0) { g.setColour (juce::Colours::white.withAlpha (0.3f + 0.6f * act)); g.drawRoundedRectangle (x - 7.0f, plot.getBottom() - h, 14.0f, h, 4.0f, 1.2f); }
                amp *= juce::jmax (0.0f, fb) * (1.0f - 0.04f * (float) k);
            }
            // the bucket chain: charges handed down the line
            const int buckets = 32;
            const float bw = chain.getWidth() / (float) buckets;
            for (int k = 0; k < buckets; ++k)
            {
                const float t = std::fmod ((float) k / (float) buckets - phase * 0.05f + 10.0f, 1.0f);
                g.setColour (acc.withAlpha (0.15f + 0.6f * t * (0.4f + 0.6f * act)));
                g.fillRoundedRectangle (chain.getX() + bw * (float) k + 1.0f, chain.getY(), bw - 2.0f, chain.getHeight(), 2.0f);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            static const char* syncNames[] = { "FREE", "1/16", "1/8", "1/8 DOTTED", "1/4", "1/2" };
            ColourBits::caption (g, q.removeFromTop (14.0f), syncNames[juce::jlimit (0, 5, bb->sync.get())]);
            ColourBits::big (g, q.removeFromTop (34.0f), bb->sync.get() > 0 ? juce::String (syncNames[juce::jlimit (0, 5, bb->sync.get())]) : juce::String (juce::roundToInt (bb->bank[SqBucketPlugin::time])) + " ms", juce::Colours::white, 22.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "feedback " + juce::String (juce::roundToInt (bb->bank[SqBucketPlugin::feedback])) + " %");
            ColourBits::line (g, q.removeFromTop (16.0f), "repeats fade to dark");
        }
    };
