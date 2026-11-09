// Included inside namespace Skins (PluginSkins.h): faces for Shard, Murmuration and Masquerade (1.11 Color & Space).

    //==============================================================================
    /** Shard: the delay and the splice train as crystal shards (pointing back for reversed splices, lifted by the pitch),
        the live modulation meters (LFO 1, LFO 2, envelope) and what the engine is doing. */
    struct ShardViz : public Viz, private juce::Timer
    {
        ShardViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sd (dynamic_cast<SqShardPlugin*> (&p)) { startTimerHz (30); }
        SqShardPlugin* sd; float m[3] {}; long long lastSpliced = 0; float flash = 0;
        void timerCallback() override
        {
            if (sd == nullptr) return;
            for (int k = 0; k < 3; ++k) m[k] += 0.5f * (sd->proc.modValue (k) - m[k]);
            const auto n = sd->proc.splicesStarted(); if (n != lastSpliced) { lastSpliced = n; flash = 1; } else flash *= 0.85f;
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (sd == nullptr) return;
            const auto acc = neon (accent); const auto kp = sd->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (220.0f); r.removeFromRight (8.0f);
            auto mods = r.removeFromRight (120.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "DELAY  +  SPLICES   (up = pitched up, arrow = played backwards)");
            const double S = juce::jmax (10.0, sd->proc.spliceMsNow() > 0 ? sd->proc.spliceMsNow() : (double) kp.spliceMs), D = sd->proc.delayMsNow();
            const double span = D + S * 4.2;
            const float pitch = Shard::quantisedPitch (kp.pitchSt, kp.quantize, kp.keyRoot, kp.scaleMask);
            const float base = plot.getBottom() - 16.0f, lift = juce::jlimit (-1.0f, 1.0f, pitch / 48.0f) * plot.getHeight() * 0.32f;
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) base, plot.getX(), plot.getRight());
            const float x0 = plot.getX(), wpx = plot.getWidth();
            auto xAt = [&] (double ms) { return x0 + (float) (ms / span) * wpx; };
            // the input hit and the delay bar
            g.setColour (juce::Colours::white.withAlpha (0.85f)); g.fillRect (x0, base - 30, 3.0f, 30.0f);
            if (D > 1) { g.setColour (juce::Colours::white.withAlpha (0.15f)); g.fillRoundedRectangle (x0 + 4, base - 6, xAt (D) - x0 - 4, 6, 3); }
            ModShapes::Rand rr; rr.s = 777u;
            for (int k = 0; k < 4; ++k)
            {
                const float a = xAt (D + k * S), b = xAt (D + (k + 1) * S) - 3;
                const bool rev = (rr.next() * 0.5f + 0.5f) < kp.reverseProb;
                const float lvl = std::pow (juce::jmax (0.05f, kp.recycle), (float) k);
                const float top = base - 20.0f - lift * (1.0f + 0.4f * k) - 40.0f * lvl;
                juce::Path shard;
                if (rev) { shard.startNewSubPath (a, base); shard.lineTo (b, base); shard.lineTo (b, top); shard.lineTo (a, base - 12); }
                else     { shard.startNewSubPath (a, base); shard.lineTo (b, base); shard.lineTo (b, base - 12); shard.lineTo (a, top); }
                shard.closeSubPath();
                const auto col = acc.withAlpha (0.25f + 0.6f * lvl + (k == 0 ? 0.15f * flash : 0.0f));
                g.setGradientFill (juce::ColourGradient (col, a, top, col.withAlpha (0.05f), a, base, false)); g.fillPath (shard);
                g.setColour (acc.withAlpha (0.9f)); g.strokePath (shard, juce::PathStrokeType (1.4f));
                g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (Theme::font (12.0f, true));
                g.drawText (rev ? juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")), (int) a, (int) base - 16, (int) (b - a), 14, juce::Justification::centred);
            }
            g.setFont (Theme::font (8.5f)); g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText (juce::String (span / 1000.0, 2) + " s", plot.toNearestInt(), juce::Justification::bottomRight);
            // modulation meters
            ColourBits::panel (g, mods, acc);
            auto mm = mods.reduced (10.0f, 10.0f); ColourBits::caption (g, mm.removeFromTop (14.0f), "MOD");
            const char* nm[3] = { "LFO 1", "LFO 2", "ENV" };
            const int tg[3] = { kp.lfo[0].target, kp.lfo[1].target, kp.envTarget };
            const float bw = mm.getWidth() / 3.0f;
            for (int k = 0; k < 3; ++k)
            {
                auto col = mm.withX (mm.getX() + k * bw).withWidth (bw - 6.0f);
                auto lab = col.removeFromBottom (24.0f);
                g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (col, 3.0f);
                const float v = juce::jlimit (-1.0f, 1.0f, m[k]);
                auto bar = k == 2 ? col.withTrimmedTop (col.getHeight() * (1.0f - v)) : (v >= 0 ? col.withTrimmedTop (col.getHeight() * 0.5f * (1.0f - v)).withHeight (col.getHeight() * 0.5f * v)
                                                                                           : col.withTrimmedTop (col.getHeight() * 0.5f).withHeight (col.getHeight() * 0.5f * -v));
                g.setColour (tg[k] > 0 ? acc : acc.withAlpha (0.25f)); g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (juce::Colours::white.withAlpha (0.65f)); g.setFont (Theme::font (8.0f, true));
                g.drawText (nm[k], lab.removeFromTop (11.0f).toNearestInt(), juce::Justification::centred);
                g.setFont (Theme::font (7.5f)); g.drawText (Shard::targetName (tg[k]), lab.toNearestInt(), juce::Justification::centred);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "SHARD");
            ColourBits::big (g, q.removeFromTop (28.0f), (pitch >= 0 ? "+" : "") + juce::String (sd->proc.pitchNow() != 0 ? sd->proc.pitchNow() : pitch, 2) + " st", juce::Colours::white, 19.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (Shard::engineName (kp.engine)) + " engine   " + juce::String (juce::roundToInt (kp.reverseProb * 100)) + " % reversed");
            ColourBits::line (g, q.removeFromTop (16.0f), "Splice " + juce::String (S, 0) + " ms   Delay " + juce::String (D, 0) + " ms");
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Recycle ") + Shard::fbName (kp.fbMode) + "  " + juce::String (juce::roundToInt (kp.recycle * 100)) + " %");
            if (kp.quantize) ColourBits::line (g, q.removeFromTop (16.0f), "Quantized to " + SqSessionKey::rootNames()[kp.keyRoot] + (sd->autoKey.get() && sd->sessionKey.known.load() ? " (Moth Key)" : ""));
            ColourBits::line (g, q.removeFromTop (16.0f), "Grains sounding: " + juce::String (sd->proc.activeGrains()));
        }
    };

    //==============================================================================
    /** Murmuration: the cloud's envelope across Time with every tap as a point (size = its gain), the live time, and the
        state of Shimmer / Freeze / Infinite. */
    struct MurmurationViz : public Viz, private juce::Timer
    {
        MurmurationViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mu (dynamic_cast<SqMurmurationPlugin*> (&p)) { startTimerHz (24); }
        SqMurmurationPlugin* mu; float t = 0;
        void timerCallback() override { t += 0.04f; repaint(); }
        void paint (juce::Graphics& g) override
        {
            if (mu == nullptr) return;
            const auto acc = neon (accent); const auto kp = mu->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (220.0f); r.removeFromRight (8.0f);
            ColourBits::panel (g, r, acc);
            auto plot = r.reduced (14.0f, 10.0f);
            ColourBits::caption (g, plot.removeFromTop (14.0f), "THE CLOUD   (envelope across Time, one point per tap)");
            plot.removeFromTop (4.0f);
            // envelope
            juce::Path env; const int N = (int) plot.getWidth();
            for (int i = 0; i <= N; ++i) { const float f = i / (float) N, e = Murm::envelopeAt (kp, f); const float xx = plot.getX() + i, yy = plot.getBottom() - e * plot.getHeight() * 0.9f; if (i == 0) env.startNewSubPath (xx, yy); else env.lineTo (xx, yy); }
            juce::Path fill (env); fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.3f), plot.getX(), plot.getY(), acc.withAlpha (0.02f), plot.getX(), plot.getBottom(), false)); g.fillPath (fill);
            glowStroke (g, env, acc, 2.0f);
            // taps (the processor's real layout once it has run; otherwise drawn from the knobs)
            const int nt = mu->proc.numTaps();
            float mx = 1e-6f; for (int k = 0; k < nt; ++k) mx = juce::jmax (mx, mu->proc.tapGain (0)[k]);
            ModShapes::Rand rr; rr.s = 99u;
            for (int k = 0; k < nt; ++k)
            {
                const float f = mu->proc.tapPos (0)[k], gk = mu->proc.tapGain (0)[k] / mx;
                const float xx = plot.getX() + f * plot.getWidth(), yy = plot.getBottom() - gk * plot.getHeight() * 0.9f + 3.0f * std::sin (t * 2.0f + k);
                const float sz = 1.5f + 3.0f * gk;
                g.setColour ((kp.shimmer > 0.01f && f > 1.0f - kp.shimmer ? juce::Colour (0xffbfe8ff) : acc).withAlpha (0.25f + 0.6f * gk));
                g.fillEllipse (xx - sz * 0.5f, yy - sz * 0.5f, sz, sz);
            }
            if (kp.freeze || kp.infinite)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (Theme::font (11.0f, true));
                g.drawText (kp.freeze ? (kp.freezeMode ? "SPECTRAL FREEZE" : "FROZEN") : "INFINITE", plot.toNearestInt().removeFromTop (16), juce::Justification::centredRight);
            }
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "MURMURATION");
            const double tm = mu->proc.timeMsNow();
            ColourBits::big (g, q.removeFromTop (28.0f), tm >= 1000 ? juce::String (tm / 1000.0, 2) + " s" : juce::String (juce::roundToInt (tm)) + " ms", juce::Colours::white, 19.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (Murm::tapsFor (kp.texture) * 2) + " taps   Texture " + juce::String (juce::roundToInt (kp.texture * 100)) + " %");
            ColourBits::line (g, q.removeFromTop (16.0f), "Shape X " + juce::String (juce::roundToInt (kp.x * 100)) + "  Y " + juce::String (juce::roundToInt (kp.y * 100)) + (kp.customEnv ? "  (drawn)" : ""));
            if (kp.shimmer > 0.01f) ColourBits::line (g, q.removeFromTop (16.0f), "Shimmer " + juce::String (kp.shimmerSt > 0 ? "+" : "") + juce::String (kp.shimmerSt) + " st  " + juce::String (juce::roundToInt (kp.shimmer * 100)) + " %");
            if (std::abs (kp.color) > 0.01f) ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Color: tail goes ") + (kp.color < 0 ? "darker" : "brighter"));
            ColourBits::line (g, q.removeFromTop (16.0f), "Mix " + juce::String (juce::roundToInt (kp.mix * 100)) + " %");
        }
    };

    //==============================================================================
    /** Masquerade: the voice's note going in and the note coming out on a keyboard strip, the formant shift as a mask that
        widens or narrows, the mode, the key and the engine latency. */
    struct MasqueradeViz : public Viz, private juce::Timer
    {
        MasqueradeViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mq (dynamic_cast<SqMasqueradePlugin*> (&p)) { startTimerHz (30); }
        SqMasqueradePlugin* mq; float inNote = -1, outNote = -1;
        void timerCallback() override
        {
            if (mq == nullptr) return;
            const auto& r = mq->proc.latest();
            const float hz = mq->liveHz.load();
            const float ni = hz > 30 ? 69.0f + 12.0f * std::log2 (hz / 440.0f) : -1.0f;
            inNote = ni < 0 ? -1.0f : (inNote < 0 ? ni : inNote + 0.4f * (ni - inNote));
            const float no = r.voiced ? r.outNote : -1.0f;
            outNote = no < 0 ? -1.0f : (outNote < 0 ? no : outNote + 0.4f * (no - outNote));
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            if (mq == nullptr) return;
            const auto acc = neon (accent); const auto kp = mq->knobParams();
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto read = r.removeFromRight (220.0f); r.removeFromRight (8.0f);
            auto mask = r.removeFromLeft (juce::jmin (220.0f, r.getWidth() * 0.3f)); r.removeFromLeft (8.0f);
            // the mask: width follows the formant, the eyes follow the pitch
            ColourBits::panel (g, mask, acc);
            {
                auto m = mask.reduced (18.0f, 22.0f);
                const float fw = juce::jlimit (0.55f, 1.0f, 0.78f + kp.formantSt / 60.0f), eye = juce::jlimit (-1.0f, 1.0f, kp.pitchSt / 24.0f);
                auto face = m.withSizeKeepingCentre (m.getWidth() * fw, m.getHeight() * 0.8f);
                juce::Path p; p.startNewSubPath (face.getX(), face.getY() + face.getHeight() * 0.3f);
                p.quadraticTo (face.getCentreX(), face.getY() - face.getHeight() * 0.15f, face.getRight(), face.getY() + face.getHeight() * 0.3f);
                p.quadraticTo (face.getRight() - face.getWidth() * 0.1f, face.getBottom(), face.getCentreX(), face.getBottom());
                p.quadraticTo (face.getX() + face.getWidth() * 0.1f, face.getBottom(), face.getX(), face.getY() + face.getHeight() * 0.3f);
                // feathers behind the mask (they fan wider as the formant rises)
                for (int k = -3; k <= 3; ++k)
                {
                    const float ang = (float) k * (0.22f + 0.06f * fw), len = face.getHeight() * (0.75f - 0.06f * std::abs ((float) k));
                    const juce::Point<float> root (face.getCentreX(), face.getY() + face.getHeight() * 0.25f), tip (root.x + std::sin (ang) * len, root.y - std::cos (ang) * len);
                    juce::Path f; f.startNewSubPath (root); f.quadraticTo (root.x + std::sin (ang + 0.25f) * len * 0.6f, root.y - std::cos (ang + 0.25f) * len * 0.6f, tip.x, tip.y);
                    f.quadraticTo (root.x + std::sin (ang - 0.25f) * len * 0.6f, root.y - std::cos (ang - 0.25f) * len * 0.6f, root.x, root.y);
                    g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.9f), tip.x, tip.y, juce::Colour (0xff2a0610), root.x, root.y, false)); g.fillPath (f);
                    g.setColour (juce::Colour (0xffffd27a).withAlpha (0.5f)); g.strokePath (f, juce::PathStrokeType (0.8f));
                }
                g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.85f), face.getCentreX(), face.getY(), juce::Colour (0xff1a0710), face.getCentreX(), face.getBottom(), false)); g.fillPath (p);
                // gold filigree: two scrolls under the eyes and a jewel on the brow
                for (int side = -1; side <= 1; side += 2)
                {
                    juce::Path sc; const float cx = face.getCentreX() + side * face.getWidth() * 0.27f, cy = face.getY() + face.getHeight() * 0.68f;
                    sc.startNewSubPath (cx - side * face.getWidth() * 0.18f, cy - face.getHeight() * 0.05f);
                    sc.cubicTo (cx, cy + face.getHeight() * 0.12f, cx + side * face.getWidth() * 0.14f, cy, cx + side * face.getWidth() * 0.05f, cy - face.getHeight() * 0.06f);
                    g.setColour (juce::Colour (0xffffd27a).withAlpha (0.8f)); g.strokePath (sc, juce::PathStrokeType (1.4f));
                }
                const float jx = face.getCentreX(), jy = face.getY() + face.getHeight() * 0.16f;
                g.setGradientFill (juce::ColourGradient (juce::Colours::white, jx - 2, jy - 2, juce::Colour (0xffc0102a), jx + 5, jy + 5, true)); g.fillEllipse (jx - 6, jy - 6, 12, 12);
                g.setColour (juce::Colour (0xffffd27a)); g.strokePath (p, juce::PathStrokeType (1.6f));
                const float ey = face.getY() + face.getHeight() * (0.42f - 0.12f * eye), ew = face.getWidth() * 0.22f, eh = face.getHeight() * 0.12f;
                g.setColour (juce::Colours::black); g.fillEllipse (face.getCentreX() - face.getWidth() * 0.27f - ew * 0.5f, ey, ew, eh); g.fillEllipse (face.getCentreX() + face.getWidth() * 0.27f - ew * 0.5f, ey, ew, eh);
                g.setColour (juce::Colour (0xffffd27a)); g.drawEllipse (face.getCentreX() - face.getWidth() * 0.27f - ew * 0.5f, ey, ew, eh, 1.2f); g.drawEllipse (face.getCentreX() + face.getWidth() * 0.27f - ew * 0.5f, ey, ew, eh, 1.2f);
            }
            // keyboard strip: in (white) -> out (accent)
            ColourBits::panel (g, r, acc);
            auto kb = r.reduced (14.0f, 12.0f);
            ColourBits::caption (g, kb.removeFromTop (14.0f), "VOICE IN  ->  VOICE OUT");
            kb.removeFromTop (6.0f);
            const int lo = 36, hi = 84;
            const int keyMask = Masquerade::Processor::rotate (kp.scaleMask, kp.keyRoot);
            auto isBlack = [] (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };
            int whites = 0; for (int n = lo; n <= hi; ++n) if (! isBlack (n)) ++whites;
            const float ww = kb.getWidth() / (float) whites;
            // x centre of any note on the keyboard (black keys sit between their white neighbours)
            auto noteX = [&] (float note)
            {
                const int n = (int) std::floor (note); int wi = 0; for (int k = lo; k < n; ++k) if (! isBlack (k)) ++wi;
                const float base = kb.getX() + ww * (float) wi + (isBlack (n) ? 0.0f : ww * 0.5f);
                return base + (note - (float) n) * ww * 0.5f;
            };
            const bool robotMode = kp.mode == Masquerade::robot; const int robotN = kp.robotNote + juce::roundToInt (kp.pitchSt);
            int wi = 0;
            for (int n = lo; n <= hi; ++n)
            {
                if (isBlack (n)) continue;
                auto key = juce::Rectangle<float> (kb.getX() + ww * (float) wi++, kb.getY(), ww - 1.5f, kb.getHeight());
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffece4ea), key.getX(), key.getY(), juce::Colour (0xffb9aeb8), key.getX(), key.getBottom(), false));
                g.fillRoundedRectangle (key, 2.5f);
                if (kp.mode == Masquerade::quantize && ((keyMask >> (n % 12)) & 1)) { g.setColour (acc.withAlpha (0.55f)); g.fillRoundedRectangle (key.removeFromBottom (7.0f).reduced (2.0f, 1.0f), 2.0f); }
                if (robotMode && n == robotN) { g.setColour (acc.withAlpha (0.55f)); g.fillRoundedRectangle (key, 2.5f); }
            }
            for (int n = lo; n <= hi; ++n)
            {
                if (! isBlack (n)) continue;
                const float cx = noteX ((float) n);
                auto key = juce::Rectangle<float> (cx - ww * 0.32f, kb.getY(), ww * 0.64f, kb.getHeight() * 0.6f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2228), key.getX(), key.getY(), juce::Colour (0xff070507), key.getX(), key.getBottom(), false));
                g.fillRoundedRectangle (key, 2.0f);
                if (kp.mode == Masquerade::quantize && ((keyMask >> (n % 12)) & 1)) { g.setColour (acc.withAlpha (0.8f)); g.fillRoundedRectangle (key.removeFromBottom (5.0f).reduced (2.0f, 1.0f), 1.5f); }
                if (robotMode && n == robotN) { g.setColour (acc); g.fillRoundedRectangle (key, 2.0f); }
            }
            for (int oct = lo; oct <= hi; oct += 12) { g.setColour (juce::Colour (0xff4a3a44)); g.setFont (Theme::font (8.0f, true)); g.drawText ("C" + juce::String (oct / 12 - 1), (int) noteX ((float) oct) - 12, (int) kb.getBottom() - 12, 24, 10, juce::Justification::centred); }
            auto marker = [&] (float note, juce::Colour c, const juce::String& t, bool top)
            {
                if (note < lo || note > hi + 1) return;
                const float xx = noteX (note);
                g.setColour (c.withAlpha (0.25f)); g.fillRoundedRectangle (xx - 7.0f, kb.getY(), 14.0f, kb.getHeight(), 6.0f);
                g.setColour (c); g.fillRoundedRectangle (xx - 2.5f, kb.getY(), 5.0f, kb.getHeight(), 2.5f);
                auto tag = juce::Rectangle<float> (xx - 34.0f, top ? kb.getY() + 4.0f : kb.getBottom() - 22.0f, 68.0f, 18.0f);
                g.setColour (juce::Colour (0xe0120810)); g.fillRoundedRectangle (tag, 9.0f); g.setColour (c); g.drawRoundedRectangle (tag, 9.0f, 1.2f);
                g.setFont (Theme::font (10.0f, true)); g.drawText (t, tag.toNearestInt(), juce::Justification::centred);
            };
            auto nn = [] (float n) { return n < 0 ? juce::String ("-") : SqSessionKey::rootNames()[((int) std::lround (n) % 12 + 12) % 12] + juce::String ((int) std::lround (n) / 12 - 1); };
            marker (inNote, juce::Colour (0xff59d0ff), "IN " + nn (inNote), true);
            marker (outNote, acc, "OUT " + nn (outNote), false);
            ColourBits::panel (g, read, acc, true);
            auto q = read.reduced (12.0f);
            ColourBits::caption (g, q.removeFromTop (14.0f), "MASQUERADE");
            ColourBits::big (g, q.removeFromTop (28.0f), Masquerade::modeName (kp.mode), juce::Colours::white, 19.0f);
            ColourBits::line (g, q.removeFromTop (16.0f), "Pitch " + juce::String (kp.pitchSt, 1) + " st   Formant " + juce::String (kp.formantSt, 1) + " st" + (kp.link ? "  (linked)" : ""));
            ColourBits::line (g, q.removeFromTop (16.0f), "Key " + SqSessionKey::rootNames()[kp.keyRoot] + (mq->autoKey.get() && mq->sessionKey.known.load() ? " from Moth Key" : ""));
            ColourBits::line (g, q.removeFromTop (16.0f), juce::String (kp.engine ? "Studio" : "Live") + " engine, " + juce::String (mq->getLatencySeconds() * 1000.0, 1) + " ms");
            if (kp.driveVoicing > 0) ColourBits::line (g, q.removeFromTop (16.0f), juce::String ("Drive: ") + Eng::AnalogStyle::voicingName (kp.driveVoicing - 1) + "  " + juce::String (juce::roundToInt (kp.drive * 100)) + " %");
        }
    };
