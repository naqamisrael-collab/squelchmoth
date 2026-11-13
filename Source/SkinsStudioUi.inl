// Included inside namespace Skins (PluginSkins.h), before the 1.14 Studio Line faces: shared measurement drawing.
//  - knobScale: the range printed at both ends of every knob's arc (in the parameter's own units: dB, Hz, ms, %, ratio)
//  - StudioMeter: IN / OUT level meters (peak with hold, -60..+6 dB scale with numbers, a numeric peak readout) and an
//    optional GR meter (0..-24 dB, drawn down from the top), so every face can be read by numbers, not only by eye.

    namespace StudioUi
    {
        /** A compact number for a scale end, in the parameter's units. */
        inline juce::String compact (const SqDsp::ParamSpec& s, float v)
        {
            const juce::String u (s.unit);
            auto trimNum = [] (double x, int dec) { juce::String t (x, dec); if (t.contains (".")) { while (t.endsWithChar ('0')) t = t.dropLastCharacters (1); if (t.endsWithChar ('.')) t = t.dropLastCharacters (1); } return t; };
            if (u.contains ("Hz")) return v >= 1000.0f ? trimNum (v / 1000.0, 1) + "k" : trimNum (v, 0);
            if (u.contains ("dB")) return (v > 0 && s.lo < 0 ? "+" : "") + trimNum (v, std::abs (v) < 10 && std::abs (v - std::round (v)) > 0.01f ? 1 : 0);
            if (u.contains ("ms")) return v >= 1000.0f ? trimNum (v / 1000.0, 1) + "s" : trimNum (v, v < 10 ? 1 : 0);
            if (u.contains (":1")) return trimNum (v, 0) + ":1";
            if (u.contains ("%")) return trimNum (v, 0) + "%";
            if (u.trim() == "s") return trimNum (v, 1) + "s";
            if (u.trim() == "x") return "x" + trimNum (v, 2);
            return trimNum (v, 2);
        }

        /** Prints the minimum and maximum at the two ends of a knob's arc. The geometry matches the Studio Line knobs:
            the label strip on top, the readout pill at the bottom, the dial in between. */
        inline void knobScale (juce::Graphics& g, juce::Rectangle<float> cell, const SqDsp::ParamSpec& s, juce::Colour c, float labelH = 15.0f, float readH = 24.0f, float alpha = 1.0f)
        {
            auto inner = cell.withTrimmedTop (labelH).withTrimmedBottom (readH);
            const float rO = std::min (inner.getWidth() - 8.0f, inner.getHeight()) * 0.5f - 2.0f; const auto ctr = inner.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
            g.setFont (Theme::font (7.5f, true));
            for (auto [a, v, just] : { std::tuple<float, float, juce::Justification> { a0, s.lo, juce::Justification::centredRight }, { a1, s.hi, juce::Justification::centredLeft } })
            {
                const float r = rO + 3.0f;
                const juce::Point<float> p (ctr.x + r * std::sin (a), ctr.y - r * std::cos (a) + 7.0f);
                const auto t = compact (s, v); const float w = juce::GlyphArrangement::getStringWidth (Theme::font (7.5f, true), t) + 4.0f;
                auto box = just == juce::Justification::centredRight ? juce::Rectangle<float> (p.x - w + 4.0f, p.y - 5.0f, w, 10.0f) : juce::Rectangle<float> (p.x - 4.0f, p.y - 5.0f, w, 10.0f);
                g.setColour (c.withAlpha (0.62f * alpha)); g.drawText (t, box, just);
            }
        }

        /** IN / OUT (and optionally GR) meters with numbers. Call update() once per repaint, then draw(). */
        struct StudioMeter
        {
            float inDb = -100, outDb = -100, grDb = 0, inHold = -100, outHold = -100, grHold = 0; double inHoldT = 0, outHoldT = 0, grHoldT = 0;
            static float toDb (float lin) { return juce::Decibels::gainToDecibels (lin, -100.0f); }
            void update (float inLin, float outLin, float gr)
            {
                const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
                auto ballistic = [] (float& cur, float target) { cur = target > cur ? target : std::max (target, cur - 0.9f); };   // ~27 dB/s fall at 30 fps
                ballistic (inDb, toDb (inLin)); ballistic (outDb, toDb (outLin));
                grDb = gr < grDb ? gr : std::min (gr, grDb + 0.6f);
                if (inDb >= inHold || now - inHoldT > 1.5) { inHold = inDb; inHoldT = now; }
                if (outDb >= outHold || now - outHoldT > 1.5) { outHold = outDb; outHoldT = now; }
                if (grDb <= grHold || now - grHoldT > 1.5) { grHold = grDb; grHoldT = now; }
            }
            static float width (bool withGr) { return withGr ? 112.0f : 80.0f; }
            void draw (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, bool withGr, juce::Colour grColour = juce::Colour (0xffff5a7a)) const
            {
                g.setColour (juce::Colour (0xff05070a).withAlpha (0.72f)); g.fillRoundedRectangle (r, 10.0f);
                g.setColour (accent.withAlpha (0.18f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
                auto in = r.reduced (8.0f, 8.0f);
                auto head = in.removeFromTop (30.0f); in.removeFromBottom (4.0f);
                const float barW = 14.0f, scaleW = 24.0f;
                auto colIn = in.removeFromLeft (barW); auto scale = in.removeFromLeft (scaleW); auto colOut = in.removeFromLeft (barW);
                juce::Rectangle<float> colGr; if (withGr) { in.removeFromLeft (10.0f); colGr = in.removeFromLeft (barW); }
                auto yOf = [&] (const juce::Rectangle<float>& c, float dbv) { return c.getY() + juce::jlimit (0.0f, 1.0f, (6.0f - dbv) / 66.0f) * c.getHeight(); };
                // scale numbers between the bars
                g.setFont (Theme::font (7.5f, true));
                for (int d : { 6, 0, -6, -12, -18, -24, -36, -48, -60 })
                {
                    const float y = yOf (colIn, (float) d);
                    g.setColour (juce::Colours::white.withAlpha (d == 0 ? 0.75f : 0.42f)); g.drawText (d > 0 ? "+" + juce::String (d) : juce::String (d), scale.withY (y - 5.0f).withHeight (10.0f), juce::Justification::centred);
                    g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawHorizontalLine ((int) y, colIn.getX(), colIn.getRight()); g.drawHorizontalLine ((int) y, colOut.getX(), colOut.getRight());
                }
                auto bar = [&] (juce::Rectangle<float> c, float dbv, float hold)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (c, 3.0f);
                    const float top = yOf (c, dbv);
                    if (dbv > -60.0f)
                    {
                        juce::ColourGradient grad (juce::Colour (0xffff4040), 0, yOf (c, 3.0f), accent.withAlpha (0.9f), 0, c.getBottom(), false);
                        grad.addColour (juce::jlimit (0.0, 1.0, (double) ((yOf (c, -6.0f) - yOf (c, 3.0f)) / (c.getBottom() - yOf (c, 3.0f)))), juce::Colour (0xffffc04a));
                        g.setGradientFill (grad); g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (c.getX() + 1, top, c.getRight() - 1, c.getBottom() - 1), 2.0f);
                    }
                    if (hold > -60.0f) { g.setColour (hold > -0.1f ? juce::Colour (0xffff4040) : juce::Colours::white.withAlpha (0.85f)); g.fillRect (c.getX() + 1, yOf (c, hold) - 1.0f, c.getWidth() - 2, 2.0f); }
                };
                bar (colIn, inDb, inHold); bar (colOut, outDb, outHold);
                // GR: from the top down, 0..-24 dB with its own numbers
                if (withGr)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (colGr, 3.0f);
                    auto yG = [&] (float dbv) { return colGr.getY() + juce::jlimit (0.0f, 1.0f, -dbv / 24.0f) * colGr.getHeight(); };
                    if (grDb < -0.05f) { g.setGradientFill (juce::ColourGradient (grColour, 0, colGr.getY(), grColour.withAlpha (0.5f), 0, yG (grDb), false)); g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (colGr.getX() + 1, colGr.getY() + 1, colGr.getRight() - 1, yG (grDb)), 2.0f); }
                    if (grHold < -0.05f) { g.setColour (juce::Colours::white.withAlpha (0.85f)); g.fillRect (colGr.getX() + 1, yG (grHold) - 1.0f, colGr.getWidth() - 2, 2.0f); }
                    g.setFont (Theme::font (7.0f, true));
                    for (int d : { 0, -3, -6, -12, -18, -24 }) { g.setColour (juce::Colours::white.withAlpha (0.42f)); g.drawText (juce::String (d), (int) colGr.getRight() + 1, (int) yG ((float) d) - 5, 18, 10, juce::Justification::left); }
                }
                // numeric peaks on top
                g.setFont (Theme::font (8.0f, true));
                auto num = [&] (juce::Rectangle<float> col, const juce::String& lab, float hold, juce::Colour c)
                {
                    auto box = juce::Rectangle<float> (col.getCentreX() - 18.0f, head.getY(), 36.0f, head.getHeight());
                    g.setColour (juce::Colours::white.withAlpha (0.5f)); g.drawText (lab, box.removeFromTop (12.0f), juce::Justification::centred);
                    g.setColour (c); g.drawText (hold > -99.0f ? juce::String (hold, 1) : juce::String ("-inf"), box, juce::Justification::centred);
                };
                num (colIn, "IN", inHold, juce::Colours::white); num (colOut, "OUT", outHold, outHold > -0.1f ? juce::Colour (0xffff4040) : juce::Colours::white);
                if (withGr) num (colGr, "GR", grHold, grColour);
            }
        };
    }
