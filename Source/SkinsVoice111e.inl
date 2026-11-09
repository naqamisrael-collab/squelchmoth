// Included inside namespace Skins (PluginSkins.h): faces for Siren Verb, Mic Closet, Larynx and Exhale (1.11 Voice).

    //==============================================================================
    /** Siren Verb: the early field as reflections, the tail as three band envelopes (lows, mids, highs decaying at the
        rates Decay and Damping give), the echo repeats, and the Auto-EQ cutting the singer's notes out of the tail. */
    struct SirenVerbViz : public Viz, private juce::Timer
    {
        SirenVerbViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sv (dynamic_cast<SqSirenVerbPlugin*> (&p)) { startTimerHz (24); }
        SqSirenVerbPlugin* sv; float lvl = 0, hz = 0, gr = 0;
        void timerCallback() override { if (sv == nullptr) return; lvl = juce::jmax (sv->levelOut.load(), lvl * 0.9f); hz += 0.3f * (sv->proc.liveHz() - hz); gr = sv->proc.compGrDb(); repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (sv == nullptr) return;
            const auto acc = neon (accent); const auto kp = sv->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (240.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), juce::String (SirenVerb::algoName (kp.algo)).toUpperCase() + "   EARLY FIELD + TAIL (lows, mids, highs)" + (kp.delayMode ? "   + ECHO" : ""));
            const float span = juce::jmax (0.6f, kp.preDelayMs * 0.001f + kp.decayS * 1.15f);
            auto xAt = [&] (float s) { return plot.getX() + plot.getWidth() * s / span; };
            auto yAt = [&] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 66.0f); };
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            for (int d = 0; d <= 60; d += 12) g.drawHorizontalLine ((int) yAt ((float) -d), plot.getX(), plot.getRight());
            for (float t = 0.5f; t < span; t += 0.5f) g.drawVerticalLine ((int) xAt (t), plot.getY(), plot.getBottom());
            const float pd = kp.preDelayMs * 0.001f;
            // the tail: three bands
            const float hiT = kp.decayS * juce::jlimit (0.08f, 1.0f, 1.05f - 0.85f * kp.damping * SirenVerb::shape (kp.algo).hiDampScale);
            const float loT = kp.decayS * SirenVerb::shape (kp.algo).loRatio;
            const float T[3] = { loT, kp.decayS, hiT };
            const float alpha[3] = { 0.35f, 0.9f, 0.55f };
            for (int b = 2; b >= 0; --b)
            {
                juce::Path p; p.startNewSubPath (xAt (pd), yAt (-6.0f));
                for (int i = 0; i <= 80; ++i) { const float t = pd + span * i / 80.0f; const float att = t < pd + 0.04f ? (t - pd) / 0.04f : 1.0f; p.lineTo (xAt (t), yAt (-6.0f - 60.0f * (t - pd) / T[b] + (att < 1 ? 20.0f * (att - 1) : 0.0f))); }
                juce::Path f (p); f.lineTo (xAt (span), plot.getBottom()); f.lineTo (xAt (pd), plot.getBottom()); f.closeSubPath();
                g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.25f * alpha[b]), plot.getX(), plot.getY(), acc.withAlpha (0.0f), plot.getX(), plot.getBottom(), false)); g.fillPath (f);
                g.setColour (b == 1 ? acc : acc.withAlpha (alpha[b])); g.strokePath (p, juce::PathStrokeType (b == 1 ? 2.0f : 1.2f));
            }
            // early reflections
            const auto& sh = SirenVerb::shape (kp.algo);
            static const float eTap[10] = { 0.07f, 0.13f, 0.21f, 0.29f, 0.38f, 0.49f, 0.61f, 0.74f, 0.86f, 1.0f };
            if (sh.earlyLevel * kp.early > 0.01f)
                for (int k = 0; k < 10; ++k) { const float t = pd + eTap[k] * sh.earlySpreadMs * (0.5f + kp.size) * 0.001f; const float h = (1.0f - 0.6f * eTap[k]) * sh.earlyLevel * kp.early;
                    g.setColour (juce::Colours::white.withAlpha (0.75f)); g.fillRect (xAt (t), yAt (-6.0f - 30.0f * (1.0f - h)), 2.0f, plot.getBottom() - yAt (-6.0f - 30.0f * (1.0f - h))); }
            // echo repeats
            if (kp.delayMode != SirenVerb::delayOff)
                for (int k = 1; k <= 8; ++k) { const float t = kp.delayMs * 0.001f * k; if (t > span) break; const float db = gainToDbSafe (kp.delayLevel * std::pow (juce::jmax (0.01f, kp.delayFb), (float) (k - 1)));
                    const bool right = kp.delayMode == SirenVerb::delayPingPong && k % 2 == 0;
                    g.setColour ((right ? juce::Colour (0xffffd27a) : juce::Colour (0xff9fe8ff)).withAlpha (0.85f)); g.fillEllipse (xAt (t) - 4, yAt (db) - 4, 8, 8); }
            g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (Theme::font (8.5f));
            g.drawText (juce::String (span, 1) + " s", plot.toNearestInt(), juce::Justification::bottomRight);
            // readout
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "SIREN VERB");
            ColourBits::big (g, q.removeFromTop (28.0f), juce::String (kp.decayS, kp.decayS < 10 ? 2 : 1) + " s", juce::Colours::white, 19.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "Pre-delay " + juce::String (juce::roundToInt (kp.preDelayMs)) + " ms   Size " + juce::String (juce::roundToInt (kp.size * 100)) + " %");
            ColourBits::line (g, q.removeFromTop (16.0f), kp.autoEq ? (hz > 50 ? "Auto-EQ cutting " + ColourBits::hz (hz) + " + " + ColourBits::hz (hz * 2) : juce::String ("Auto-EQ waiting for a voice")) : juce::String ("Auto-EQ off"));
            juce::StringArray pre; if (std::abs (kp.prePitchSt) > 0.05f) pre.add ("Pitch"); if (kp.preThroat != 0) pre.add ("Throat"); if (kp.preTube > 0.01f) pre.add ("Tube"); if (kp.preDeEss > 0.01f) pre.add ("De-Ess"); if (kp.reverse) pre.add ("Reverse");
            ColourBits::line (g, q.removeFromTop (16.0f), "Pre: " + (pre.isEmpty() ? juce::String ("none") : pre.joinIntoString (", ")));
            ColourBits::line (g, q.removeFromTop (16.0f), (kp.comp > 0.01f ? "Comp " + juce::String (gr, 1) + " dB   " : juce::String()) + (sv->mixLockOn.get() ? "Mix locked" : "Mix " + juce::String (juce::roundToInt (kp.mix * 100)) + " %"));
            q.removeFromTop (4.0f);
            auto m = q.removeFromTop (10.0f); g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (m, 3.0f);
            g.setColour (acc); g.fillRoundedRectangle (m.withWidth (m.getWidth() * juce::jlimit (0.0f, 1.0f, (gainToDbSafe (lvl) + 60.0f) / 60.0f)), 3.0f);
        }
        static float gainToDbSafe (float gn) { return juce::Decibels::gainToDecibels (gn, -66.0f); }
    };

    //==============================================================================
    /** Mic Closet: the conversion's response (source taken out, target put in), the target's polar pattern, and the
        proximity the distance adds. */
    struct MicClosetViz : public Viz, private juce::Timer
    {
        MicClosetViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mc (dynamic_cast<SqMicClosetPlugin*> (&p)) { startTimerHz (15); }
        SqMicClosetPlugin* mc;
        void timerCallback() override { repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (mc == nullptr) return;
            const auto acc = neon (accent); const auto kp = mc->knobParams();
            MicCloset::Processor tmp; tmp.p = kp; tmp.prepare (48000.0, 256);
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (250.0f); r.removeFromRight (8.0f);
            auto polar = r.removeFromRight (r.getHeight()); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (12.0f, 8.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "CONVERSION RESPONSE   (" + juce::String (MicCloset::mics()[(size_t) kp.source].name) + "  >  " + MicCloset::mics()[(size_t) kp.target].name + ")");
            ColourBits::freqGrid (g, plot, 18.0f);
            auto c = ColourBits::curve (plot, 18.0f, [&] (float f) { return (float) tmp.responseDb (f); });
            ColourBits::fillUnder (g, c, plot, acc);
            g.setColour (acc); g.strokePath (c, juce::PathStrokeType (2.0f));
            auto t = ColourBits::curve (plot, 18.0f, [&] (float f) { return (float) tmp.targetOnlyDb (f); });
            g.setColour (juce::Colours::white.withAlpha (0.35f)); g.strokePath (t, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // polar pattern
            ColourBits::panel (g, polar, acc);
            auto pc = polar.reduced (16.0f); ColourBits::caption (g, pc.removeFromTop (14.0f), "PATTERN");
            const int pat = tmp.effectivePattern(); const float rad = juce::jmin (pc.getWidth(), pc.getHeight()) * 0.45f; const auto ctr = pc.getCentre();
            g.setColour (juce::Colours::white.withAlpha (0.12f)); for (float k : { 0.33f, 0.66f, 1.0f }) g.drawEllipse (ctr.x - rad * k, ctr.y - rad * k, rad * 2 * k, rad * 2 * k, 1.0f);
            juce::Path pp; const float A = pat == 1 ? 1.0f : pat == 2 ? 0.0f : pat == 3 ? 0.37f : 0.5f;
            for (int i = 0; i <= 120; ++i)
            {
                const float th = juce::MathConstants<float>::twoPi * i / 120.0f; const float v = std::abs (A + (1.0f - A) * std::cos (th));
                const juce::Point<float> pt (ctr.x + rad * v * std::sin (th), ctr.y - rad * v * std::cos (th));
                if (i == 0) pp.startNewSubPath (pt); else pp.lineTo (pt);
            }
            g.setColour (acc.withAlpha (0.25f)); g.fillPath (pp); g.setColour (acc); g.strokePath (pp, juce::PathStrokeType (1.8f));
            g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (Theme::font (8.5f, true));
            g.drawText (MicCloset::patternName (pat), pc.removeFromBottom (12.0f).toNearestInt(), juce::Justification::centred);
            // readout
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            const auto& tm = MicCloset::mics()[(size_t) kp.target];
            ColourBits::caption (g, q.removeFromTop (14.0f), juce::String (tm.family).toUpperCase());
            g.setColour (juce::Colours::white); g.setFont (Theme::font (15.0f, true)); g.drawFittedText (tm.name, q.removeFromTop (36.0f).toNearestInt(), juce::Justification::topLeft, 2);
            const float prox = MicCloset::proximityDb (tm.proximityDb, pat, kp.distanceCm) - MicCloset::proximityDb (tm.proximityDb, pat, MicCloset::refCm);
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (juce::roundToInt (kp.distanceCm)) + " cm   proximity " + (prox >= 0 ? "+" : "") + juce::String (prox, 1) + " dB");
            ColourBits::line (g, q.removeFromTop (16.0f), "Presence " + ColourBits::hz (tm.presHz) + "  " + (tm.presDb >= 0 ? "+" : "") + juce::String (tm.presDb, 1) + " dB");
            ColourBits::line (g, q.removeFromTop (16.0f), "From " + juce::String (MicCloset::mics()[(size_t) kp.source].name));
            ColourBits::line (g, q.removeFromTop (16.0f), "Colour " + juce::String (juce::roundToInt (kp.colour * 100)) + " %" + (kp.padDb > 0 ? "   Pad -" + juce::String (juce::roundToInt (kp.padDb)) + " dB" : juce::String()));
        }
    };

    //==============================================================================
    /** Larynx: the tract as a tube from glottis to lips, its eight sections drawn at their widths (drag them up and down),
        the response the shape gives over a neutral tract, and the source settings. */
    struct LarynxViz : public Viz, private juce::Timer
    {
        LarynxViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), lx (dynamic_cast<SqLarynxPlugin*> (&p)) { setInterceptsMouseClicks (true, false); startTimerHz (20); }
        SqLarynxPlugin* lx; juce::Rectangle<float> tube; int dragging = -1;
        void timerCallback() override { repaint(); }
        int sectionAt (juce::Point<float> p) const { if (! tube.expanded (0, 20).contains (p)) return -1; return juce::jlimit (0, 7, (int) ((p.x - tube.getX()) / tube.getWidth() * 8.0f)); }
        void setFrom (juce::Point<float> p)
        {
            if (lx == nullptr || dragging < 0) return;
            const float v = juce::jlimit (-100.0f, 100.0f, (std::abs (p.y - tube.getCentreY()) / (tube.getHeight() * 0.5f) - 0.5f) * 200.0f);
            lx->bank.params[(size_t) (SqLarynxPlugin::p1 + dragging)]->setParameter (v, juce::sendNotification);
        }
        void mouseDown (const juce::MouseEvent& e) override { dragging = sectionAt (e.position); if (dragging >= 0) { lx->edit.getUndoManager().beginNewTransaction ("Larynx: shape"); setFrom (e.position); } }
        void mouseDrag (const juce::MouseEvent& e) override { setFrom (e.position); }
        void mouseUp (const juce::MouseEvent&) override { dragging = -1; }
        void paint (juce::Graphics& g) override
        {
            if (lx == nullptr) return;
            const auto acc = neon (accent); const auto kp = lx->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 8.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "TRACT   glottis  >  lips   (drag a section: wider / narrower)");
            auto resp = plot.removeFromBottom (plot.getHeight() * 0.36f); plot.removeFromBottom (6.0f);
            tube = plot.withSizeKeepingCentre (plot.getWidth() * juce::jlimit (0.55f, 1.0f, kp.length * 0.62f), plot.getHeight());
            const float sw = tube.getWidth() / 8.0f;
            // the tract as one organic tube: smooth walls through the eight section widths, glowing inside
            float hh[8]; for (int s = 0; s < 8; ++s) hh[s] = tube.getHeight() * 0.5f * juce::jlimit (0.10f, 1.0f, 0.40f * std::sqrt (std::exp (1.3f * kp.shape[s])));
            juce::Path upper, lower;
            const float cy = tube.getCentreY();
            upper.startNewSubPath (tube.getX(), cy - hh[0] * 0.55f); lower.startNewSubPath (tube.getX(), cy + hh[0] * 0.55f);
            for (int s = 0; s < 8; ++s)
            {
                const float xc = tube.getX() + sw * (s + 0.5f);
                const float xPrev = s == 0 ? tube.getX() : tube.getX() + sw * (s - 0.5f);
                const float hPrev = s == 0 ? hh[0] * 0.55f : hh[s - 1];
                upper.cubicTo ((xPrev + xc) * 0.5f, cy - hPrev, (xPrev + xc) * 0.5f, cy - hh[s], xc, cy - hh[s]);
                lower.cubicTo ((xPrev + xc) * 0.5f, cy + hPrev, (xPrev + xc) * 0.5f, cy + hh[s], xc, cy + hh[s]);
            }
            upper.lineTo (tube.getRight(), cy - hh[7] * 1.15f); lower.lineTo (tube.getRight(), cy + hh[7] * 1.15f);
            juce::Path body (upper);
            {   // the lower wall back to the start
              for (int i = 8; i >= 0; --i) { const float x = i == 8 ? tube.getRight() : tube.getX() + sw * (i + 0.5f); const float h = i == 8 ? hh[7] * 1.15f : hh[i]; body.lineTo (x, cy + h); }
              body.lineTo (tube.getX(), cy + hh[0] * 0.55f); body.closeSubPath(); }
            g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.55f), tube.getX(), cy, juce::Colour (0xffffd0e8).withAlpha (0.35f), tube.getRight(), cy, false));
            g.fillPath (body);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), tube.getCentreX(), cy, juce::Colours::transparentWhite, tube.getCentreX(), cy - tube.getHeight() * 0.5f, true));
            g.fillPath (body);
            g.setColour (acc); g.strokePath (upper, juce::PathStrokeType (2.2f)); g.strokePath (lower, juce::PathStrokeType (2.2f));
            // rings between sections, and a handle on each section's wall
            for (int s = 0; s < 8; ++s)
            {
                const float xc = tube.getX() + sw * (s + 0.5f);
                g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawLine (xc, cy - hh[s], xc, cy + hh[s], 1.0f);
                const bool on = s == dragging;
                g.setColour (on ? juce::Colours::white : acc.brighter (0.4f)); g.fillEllipse (xc - (on ? 7.0f : 5.0f), cy - hh[s] - (on ? 7.0f : 5.0f), on ? 14.0f : 10.0f, on ? 14.0f : 10.0f);
                g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (xc - 5.0f, cy - hh[s] - 5.0f, 10.0f, 10.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.65f)); g.setFont (Theme::font (8.0f, true));
                static const char* nm[8] = { "GLOTTIS", "LOW THR", "UP THR", "PHARYNX", "BACK", "MID", "FRONT", "LIPS" };
                g.drawText (nm[s], juce::Rectangle<float> (tube.getX() + sw * s, tube.getBottom() - 12, sw, 12).toNearestInt(), juce::Justification::centred);
            }
            // response of the shape over a neutral tract
            ColourBits::freqGrid (g, resp, 18.0f);
            double area[8], neutral[8]; for (int s = 0; s < 8; ++s) { area[s] = std::exp (1.3 * kp.shape[s]); neutral[s] = 1.0; }
            auto c = ColourBits::curve (resp, 18.0f, [&] (float f) { const double fw = f * kp.length; return (float) juce::jlimit (-18.0, 18.0, Larynx::tubeDb (area, 17.5, fw) - Larynx::tubeDb (neutral, 17.5, fw)); });
            g.setColour (acc); g.strokePath (c, juce::PathStrokeType (1.6f));
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "LARYNX");
            ColourBits::big (g, q.removeFromTop (28.0f), juce::String (juce::roundToInt (kp.length * 100)) + " % tract", juce::Colours::white, 17.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "Formants x" + juce::String (1.0f / kp.length, 2));
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (kp.glottal >= 0 ? "Hard " : "Soft ") + juce::String (juce::roundToInt (std::abs (kp.glottal) * 100)) + " %   Pulse " + juce::String (juce::roundToInt (kp.pulseWidth * 100)) + " %");
            ColourBits::line (g, q.removeFromTop (16.0f), "Breath " + juce::String (juce::roundToInt (kp.breath * 100)) + " % at " + ColourBits::hz (kp.breathHz));
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Latency ") + juce::String (lx->getLatencySeconds() * 1000.0, 1) + " ms" + (kp.levelMatch ? "   level matched" : ""));
        }
    };

    //==============================================================================
    /** Exhale: the voice's harmonic tone and its breath as two live meters, the harmonic-to-noise history, and the
        breath gain being applied now. */
    struct ExhaleViz : public Viz, private juce::Timer
    {
        ExhaleViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), ex (dynamic_cast<SqExhalePlugin*> (&p)) { hist.fill (0.0f); startTimerHz (30); }
        SqExhalePlugin* ex; std::array<float, 240> hist {}; int hw = 0; std::array<bool, 240> vh {};
        void timerCallback() override { if (ex == nullptr) return; hist[(size_t) hw] = ex->proc.hnrDb(); vh[(size_t) hw] = ex->proc.voiced(); hw = (hw + 1) % 240; repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (ex == nullptr) return;
            const auto acc = neon (accent); const auto kp = ex->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 8.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "TONE vs BREATH   (harmonic-to-noise over time; bright = voiced)");
            g.setColour (juce::Colours::white.withAlpha (0.08f)); for (int d = -10; d <= 30; d += 10) g.drawHorizontalLine ((int) (plot.getBottom() - (d + 10) / 40.0f * plot.getHeight()), plot.getX(), plot.getRight());
            juce::Path p; const float w = plot.getWidth() / 239.0f;
            for (int i = 0; i < 240; ++i)
            {
                const float v = juce::jlimit (-10.0f, 30.0f, hist[(size_t) ((hw + i) % 240)]);
                const juce::Point<float> pt (plot.getX() + i * w, plot.getBottom() - (v + 10.0f) / 40.0f * plot.getHeight());
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
                if (vh[(size_t) ((hw + i) % 240)]) { g.setColour (acc.withAlpha (0.12f)); g.fillRect (pt.x, plot.getY(), w + 0.5f, plot.getHeight()); }
            }
            g.setColour (acc); g.strokePath (p, juce::PathStrokeType (1.8f));
            g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (Theme::font (8.5f)); g.drawText ("dB", plot.toNearestInt(), juce::Justification::topLeft);
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "EXHALE");
            ColourBits::big (g, q.removeFromTop (28.0f), kp.breathDb <= -59.5f ? juce::String ("Breath off") : (kp.breathDb >= 0 ? "+" : "") + juce::String (kp.breathDb, 1) + " dB", juce::Colours::white, 18.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "Applied now " + juce::String (juce::jmax (-60.0f, ex->proc.breathGainDb()), 1) + " dB");
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (ex->proc.voiced() ? "Voiced" : "Unvoiced / consonant") + "   HNR " + juce::String (ex->proc.hnrDb(), 1) + " dB");
            ColourBits::line (g, q.removeFromTop (16.0f), "Consonants kept " + juce::String (juce::roundToInt (kp.consonants * 100)) + " %");
            ColourBits::line (g, q.removeFromTop (16.0f), "Latency " + juce::String (ex->getLatencySeconds() * 1000.0, 1) + " ms");
        }
    };
