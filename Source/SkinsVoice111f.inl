// Included inside namespace Skins (PluginSkins.h): faces for Hive Choir, Double Wing 2 and Mutant (1.11 Voice).

    //==============================================================================
    /** Hive Choir: the lead and every voice's note scrolling on a pitch lane (a living score), and each voice's choir as a
        swarm of singers placed across the stereo field (left to right) and in time (nearer = on the beat). */
    struct HiveChoirViz : public Viz, private juce::Timer
    {
        HiveChoirViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), hc (dynamic_cast<SqHiveChoirPlugin*> (&p)) { for (auto& r : hist) r.fill (-1.0f); startTimerHz (30); }
        SqHiveChoirPlugin* hc; std::array<std::array<float, 5>, 180> hist {}; int hw = 0;
        static juce::Colour voiceCol (int k) { static const juce::uint32 c[5] = { 0xffffffff, 0xffffc857, 0xffff7a59, 0xff7ad7ff, 0xffb18cff }; return juce::Colour (c[k]); }
        void timerCallback() override
        {
            if (hc == nullptr) return;
            const auto& r = hc->proc.latest(); auto& e = hist[(size_t) hw];
            e[0] = r.voiced ? r.inNote : -1.0f;
            for (int k = 0; k < 4; ++k) e[(size_t) k + 1] = (r.voiced && r.hOn[k] && hc->mode[(size_t) k].get() != 0) ? r.hNote[k] : -1.0f;
            hw = (hw + 1) % 180; repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (hc == nullptr) return;
            const auto acc = neon (accent); const auto kp = hc->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto swarm = r.removeFromRight (r.getWidth() * 0.38f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "THE SCORE   (white = lead, colours = voices 1-4, key " + SqSessionKey::rootNames()[kp.keyRoot] + ")");
            float lo = 200, hi = -200;
            for (auto& e : hist) for (float n : e) if (n > 0) { lo = std::min (lo, n); hi = std::max (hi, n); }
            if (lo > hi) { lo = 48; hi = 72; }
            lo = std::floor (lo) - 3; hi = std::ceil (hi) + 3; if (hi - lo < 14) { const float c = 0.5f * (lo + hi); lo = c - 7; hi = c + 7; }
            auto yAt = [&] (float n) { return plot.getBottom() - (n - lo) / (hi - lo) * plot.getHeight(); };
            for (int n = (int) lo; n <= (int) hi; ++n)
            {
                const bool inKey = SqVocal::pcIn (Masquerade::Processor::rotate (kp.scaleMask, kp.keyRoot), n);
                g.setColour (juce::Colours::white.withAlpha (inKey ? (n % 12 == kp.keyRoot ? 0.16f : 0.07f) : 0.02f)); g.fillRect (plot.getX(), yAt ((float) n + 0.5f), plot.getWidth(), std::max (1.0f, plot.getHeight() / (hi - lo) - 1.0f));
                if (n % 12 == 0) { g.setColour (juce::Colours::white.withAlpha (0.45f)); g.setFont (Theme::font (8.0f)); g.drawText ("C" + juce::String (n / 12 - 1), (int) plot.getX() + 2, (int) yAt ((float) n) - 6, 30, 10, juce::Justification::left); }
            }
            const float w = plot.getWidth() / 179.0f;
            for (int v = 4; v >= 0; --v)
            {
                juce::Path p; bool open = false;
                for (int i = 0; i < 180; ++i)
                {
                    const float n = hist[(size_t) ((hw + i) % 180)][(size_t) v];
                    if (n <= 0) { open = false; continue; }
                    const juce::Point<float> pt (plot.getX() + i * w, yAt (n));
                    if (! open) { p.startNewSubPath (pt); open = true; } else p.lineTo (pt);
                }
                g.setColour (voiceCol (v).withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (v == 0 ? 7.0f : 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (voiceCol (v)); g.strokePath (p, juce::PathStrokeType (v == 0 ? 2.4f : 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            // the swarm: every singer of every voice, across the field (x) and how late it sings (y)
            ColourBits::panel (g, swarm, acc, true);
            auto q = swarm.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "THE CHOIR   L  <  stereo  >  R      (lower = later)");
            g.setColour (juce::Colours::white.withAlpha (0.1f)); g.drawVerticalLine ((int) q.getCentreX(), q.getY(), q.getBottom());
            int total = 0;
            for (int k = 0; k < 4; ++k)
            {
                if (kp.v[k].mode == HiveChoir::off) continue;
                const auto& en = hc->proc.ensemble (k); total += en.count();
                const float vp = juce::jlimit (-1.0f, 1.0f, kp.v[k].pan * kp.width);
                for (int i = 0; i < en.count(); ++i)
                {
                    const float x = q.getCentreX() + juce::jlimit (-1.0f, 1.0f, en.panOf (i) * 0.75f + vp * 0.6f) * q.getWidth() * 0.46f;
                    const float y = q.getY() + 10 + juce::jlimit (0.0f, 1.0f, en.timeOf (i) / 80.0f) * (q.getHeight() - 30);
                    const float rr = 4.0f + 3.0f * dbToGainClamp (kp.v[k].levelDb);
                    g.setColour (voiceCol (k + 1).withAlpha (0.25f)); g.fillEllipse (x - rr * 2, y - rr * 2, rr * 4, rr * 4);
                    g.setColour (voiceCol (k + 1)); g.fillEllipse (x - rr * 0.5f, y - rr * 0.5f, rr, rr);
                }
            }
            g.setColour (juce::Colours::white.withAlpha (0.75f)); g.setFont (Theme::font (9.0f, true));
            g.drawText (juce::String (total) + " singers   " + (kp.engine ? "Studio" : "Live") + " engine   " + juce::String (hc->getLatencySeconds() * 1000.0, 1) + " ms",
                        q.removeFromBottom (14.0f).toNearestInt(), juce::Justification::centredLeft);
        }
        static float dbToGainClamp (float db) { return juce::jlimit (0.0f, 1.5f, juce::Decibels::decibelsToGain (db)); }
    };

    //==============================================================================
    /** Double Wing 2: the lead in the middle and the doubles placed where they sit (left / right, and late by how much),
        the straight-note detector and the vibrato it is adding. */
    struct DoubleWing2Viz : public Viz, private juce::Timer
    {
        DoubleWing2Viz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), dw (dynamic_cast<SqDoubleWing2Plugin*> (&p)) { startTimerHz (30); }
        SqDoubleWing2Plugin* dw; float ph = 0, lvl = 0;
        void timerCallback() override { if (dw == nullptr) return; ph += 0.18f; lvl = juce::jmax (dw->levelOut.load(), lvl * 0.9f); repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (dw == nullptr) return;
            const auto acc = neon (accent); const auto kp = dw->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "THE DOUBLES   L  <  >  R    (further down = later)");
            const auto& en = dw->proc.ensemble();
            const float cx = plot.getCentreX();
            auto yAt = [&] (float ms) { return plot.getY() + 20 + juce::jlimit (0.0f, 1.0f, ms / 80.0f) * (plot.getHeight() - 40); };
            g.setColour (juce::Colours::white.withAlpha (0.08f)); for (int ms = 0; ms <= 80; ms += 20) g.drawHorizontalLine ((int) yAt ((float) ms), plot.getX(), plot.getRight());
            g.setColour (juce::Colours::white.withAlpha (0.45f)); g.setFont (Theme::font (8.0f)); for (int ms = 20; ms <= 80; ms += 20) g.drawText (juce::String (ms) + " ms", (int) plot.getX(), (int) yAt ((float) ms) - 11, 40, 10, juce::Justification::left);
            // the lead
            g.setColour (juce::Colours::white); g.fillEllipse (cx - 9, yAt (0) - 9, 18, 18);
            g.setColour (juce::Colours::white.withAlpha (0.8f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("LEAD", (int) cx - 20, (int) yAt (0) + 10, 40, 10, juce::Justification::centred);
            const float vib = dw->proc.vibratoAmount() * kp.vibCents / 40.0f;
            for (int i = 0; i < en.count(); ++i)
            {
                const float x = cx + en.panOf (i) * plot.getWidth() * 0.45f * juce::jlimit (0.0f, 1.0f, kp.width), y = yAt (en.timeOf (i));
                const float wob = vib * 6.0f * std::sin (ph + i * 1.7f);
                g.setColour (acc.withAlpha (0.3f)); g.drawLine (cx, yAt (0), x, y, 1.0f);
                g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.9f), x, y + wob, acc.withAlpha (0.0f), x + 22, y + wob, true)); g.fillEllipse (x - 22, y - 22 + wob, 44, 44);
                g.setColour (juce::Colours::white); g.fillEllipse (x - 6, y - 6 + wob, 12, 12);
                g.setColour (juce::Colours::white.withAlpha (0.75f)); g.drawText (juce::String (i + 1), (int) x - 10, (int) (y + 10 + wob), 20, 10, juce::Justification::centred);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "DOUBLE WING 2");
            ColourBits::big (g, q.removeFromTop (28.0f), juce::String (en.count()) + (en.count() == 1 ? " double" : " doubles"), juce::Colours::white, 18.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "Pitch +-" + juce::String (juce::roundToInt (kp.pitchVar)) + " ct   Timing " + juce::String (juce::roundToInt (kp.timeVar)) + " ms");
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Vibrato ") + (kp.vibCents < 0.5f ? juce::String ("off") : juce::String (juce::roundToInt (kp.vibCents)) + " ct" + (kp.vibStraightOnly ? " on straight notes" : "")));
            q.removeFromTop (6.0f);
            auto bar = [&] (const juce::String& lab, float v)
            {
                auto row = q.removeFromTop (12.0f); q.removeFromTop (5.0f);
                g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (Theme::font (8.0f, true)); g.drawText (lab, row.removeFromLeft (70.0f).toNearestInt(), juce::Justification::centredLeft);
                g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (row, 3.0f);
                g.setColour (acc); g.fillRoundedRectangle (row.withWidth (row.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 3.0f);
            };
            bar ("STRAIGHT", dw->proc.straightness()); bar ("VIBRATO", dw->proc.vibratoAmount()); bar ("OUT", juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (lvl, -60.0f) + 60.0f) / 60.0f));
        }
    };

    //==============================================================================
    /** Mutant: the mutation field - pitch (x) against Throat (y) with the Alienization jumps as a trail, the ring
        carrier as an orbit around the singer's pitch, and the stutter as a flashing slice. */
    struct MutantViz : public Viz, private juce::Timer
    {
        MutantViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mu (dynamic_cast<SqMutantPlugin*> (&p)) { trail.fill ({ 0, 0 }); startTimerHz (30); }
        SqMutantPlugin* mu; std::array<juce::Point<float>, 48> trail {}; int tw = 0; float orbit = 0, flash = 0;
        void timerCallback() override
        {
            if (mu == nullptr) return;
            const auto kp = mu->knobParams();
            trail[(size_t) tw] = { kp.pitchSt + mu->proc.alienPitchNow(), kp.throatSt }; tw = (tw + 1) % 48;
            orbit += 0.12f + 0.4f * kp.ringMix; flash = mu->proc.stuttering() ? 1.0f : flash * 0.85f;
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (mu == nullptr) return;
            const auto acc = neon (accent); const auto kp = mu->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (230.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "MUTATION FIELD   pitch  >   |   throat  ^");
            auto xAt = [&] (float st) { return plot.getCentreX() + juce::jlimit (-24.0f, 24.0f, st) / 24.0f * plot.getWidth() * 0.47f; };
            auto yAt = [&] (float st) { return plot.getCentreY() - juce::jlimit (-24.0f, 24.0f, st) / 24.0f * plot.getHeight() * 0.45f; };
            for (int k = -24; k <= 24; k += 12) { g.setColour (juce::Colours::white.withAlpha (k == 0 ? 0.18f : 0.07f)); g.drawVerticalLine ((int) xAt ((float) k), plot.getY(), plot.getBottom()); g.drawHorizontalLine ((int) yAt ((float) k), plot.getX(), plot.getRight()); }
            if (flash > 0.02f) { g.setColour (acc.withAlpha (0.12f * flash)); g.fillRect (plot); }
            for (int i = 0; i < 48; ++i)
            {
                const auto& t = trail[(size_t) ((tw + i) % 48)]; const float a = (float) i / 48.0f;
                g.setColour (acc.withAlpha (0.08f + 0.5f * a)); g.fillEllipse (xAt (t.x) - 3, yAt (t.y) - 3, 6, 6);
            }
            const auto& now = trail[(size_t) ((tw + 47) % 48)];
            const float cx = xAt (now.x), cy = yAt (now.y);
            if (kp.ringMix > 0.01f)
            {
                const float rr = 18.0f + 30.0f * std::log2 (1.0f + kp.ringRatio);
                g.setColour (juce::Colour (0xffff7af0).withAlpha (0.35f + 0.5f * kp.ringMix)); g.drawEllipse (cx - rr, cy - rr, rr * 2, rr * 2, 1.4f);
                g.setColour (juce::Colour (0xffff7af0)); g.fillEllipse (cx + rr * std::cos (orbit) - 4, cy + rr * std::sin (orbit) - 4, 8, 8);
            }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx, cy, acc.withAlpha (0.0f), cx + 26, cy, true)); g.fillEllipse (cx - 26, cy - 26, 52, 52);
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "MUTANT");
            ColourBits::big (g, q.removeFromTop (28.0f), (now.x >= 0 ? "+" : "") + juce::String (now.x, 1) + " st", juce::Colours::white, 18.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "Throat " + juce::String (kp.throatSt >= 0 ? "+" : "") + juce::String (kp.throatSt, 1) + " st");
            ColourBits::line (g, q.removeFromTop (16.0f), kp.ringMix > 0.01f ? "Ring " + juce::String (juce::roundToInt (kp.ringMix * 100)) + " % at x" + juce::String (kp.ringRatio, 2) + " pitch" : juce::String ("Ring off"));
            ColourBits::line (g, q.removeFromTop (16.0f), kp.alien > 0.01f ? "Alien " + juce::String (juce::roundToInt (kp.alien * 100)) + " % every " + Eng::WingMod::syncName (kp.alienDiv) : juce::String ("Alienization off"));
            ColourBits::line (g, q.removeFromTop (16.0f), kp.stutter > 0.01f ? "Stutter " + juce::String (juce::roundToInt (kp.stutter * 100)) + " %  " + Eng::WingMod::syncName (kp.stutterDiv) + " x" + juce::String (kp.stutterRepeats) : juce::String ("Stutter off"));
            ColourBits::line (g, q.removeFromTop (16.0f), "Singer " + (mu->liveHz.load() > 0 ? ColourBits::hz (mu->liveHz.load()) : juce::String ("-")) + "   " + juce::String (mu->getLatencySeconds() * 1000.0, 1) + " ms");
        }
    };
