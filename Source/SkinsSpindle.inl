// Included inside namespace Skins (PluginSkins.h): the Spindle Delay face (1.14 Studio Line, phase I).
//
// Squelchmoth's own "tape at midnight" design on the Higgsfield plate pa_spindle:
//  - top bar: character (digital / tape / analog / diffuse), routing (normal / ping-pong / cross), the sync of each side,
//    link L/R, freeze
//  - the echo trail: every repeat drawn where and how loud it will land (left above the line, right below), fading in
//    colour as the loop filters darken it; the live wet level runs along the bottom
//  - the loop: the feedback filters as a curve with draggable corners, and a reel that turns at the delay's speed and
//    wobbles with the wow
//  - the deck: knobs drawn as small spoked reels with a teal arc, drawn here

    struct SpindleViz : public Viz, private juce::Timer
    {
        using P = SqSpindleDelayPlugin;
        SpindleViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), sp (dynamic_cast<SqSpindleDelayPlugin*> (&p)) { setInterceptsMouseClicks (true, false); startTimerHz (30); }
        SqSpindleDelayPlugin* sp;
        std::function<void()> onChange;
        double reelAngle = 0;

        static juce::Colour night() { return juce::Colour (0xff060a12); }
        static juce::Colour teal()  { return juce::Colour (0xff3fe0d0); }
        static juce::Colour cream() { return juce::Colour (0xfff5ead6); }
        static juce::Colour blue()  { return juce::Colour (0xff4a7dff); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xffeef6ff).withAlpha (a); }

        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> topBar() const  { return all().reduced (16.0f, 0).removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> deck() const    { return all().reduced (16.0f, 0).removeFromBottom (188.0f).withTrimmedBottom (14.0f); }
        juce::Rectangle<float> middle() const  { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (196.0f); r.removeFromRight (meterW() + 10.0f); return r; }
        // the numbers: IN / OUT meters on the right (StudioUi, shared by the Studio Line)
        StudioUi::StudioMeter meter;
        static float meterW() { return StudioUi::StudioMeter::width (false); }
        juce::Rectangle<float> meterR() const { auto r = all().reduced (16.0f, 0); r.removeFromTop (50.0f); r.removeFromBottom (196.0f); return r.removeFromRight (meterW()); }
        void paintMeter (juce::Graphics& g) { meter.update (sp->levelIn.load(), sp->levelOut.load(), (float) 0.0f); meter.draw (g, meterR(), teal(), false); }
        juce::Rectangle<float> trailPane() const { auto r = middle(); return r.removeFromLeft (r.getWidth() - 352.0f); }
        juce::Rectangle<float> loopPane() const  { auto r = middle(); return r.removeFromRight (340.0f); }
        juce::Rectangle<float> trail() const { return trailPane().reduced (16.0f, 0).withTrimmedTop (30.0f).withTrimmedBottom (44.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (night().withAlpha (0.70f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (teal().withAlpha (0.06f), 0, r.getY(), teal().withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (teal().withAlpha (0.16f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (text (0.45f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }
        void timerCallback() override
        {
            if (sp != nullptr) { const double t = std::max (1.0, (double) sp->liveL.load()); reelAngle += 0.033 * juce::MathConstants<double>::twoPi * std::min (3.0, 250.0 / t); }
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (sp == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("spindle");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.85f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.22f), a.getCentreX(), a.getY(), night().withAlpha (0.86f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (teal().withAlpha (0.22f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            hits.clear();
            paintTopBar (g); paintTrail (g); paintLoop (g); paintDeck (g); paintMeter (g);
        }

        // ---- the echo trail (computed from the settings: where each repeat lands, how loud, how dark)
        void paintTrail (juce::Graphics& g)
        {
            pane (g, trailPane(), "THE ECHOES  -  where every repeat lands (left above, right below), darker as the loop filters it");
            const auto t = trail();
            const double tL = sp->timeMs (0), tR = sp->timeMs (1), fb = sp->bank[P::feedback] / 100.0, xf = sp->bank[P::crossFeed] / 100.0;
            const int routing = sp->routing.get(); const bool frozen = sp->freeze.get();
            const double span = std::max (tL, tR) * 8.5;
            auto xAt = [&] (double ms) { return t.getX() + (float) juce::jlimit (0.0, 1.0, ms / span) * t.getWidth(); };
            const float yMid = t.getCentreY(), half = t.getHeight() * 0.5f - 4.0f;
            auto hOf = [&] (double gainDb) { return (float) juce::jlimit (0.0, 1.0, (gainDb + 60.0) / 60.0) * half; };
            g.setFont (Theme::font (8.5f, true));
            for (int k = 0; k <= 8; ++k)
            {
                const double ms = span * k / 8.0; const float x = xAt (ms);
                g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, t.getY(), t.getBottom());
                g.setColour (text (0.32f)); g.drawText (ms >= 1000 ? juce::String (ms / 1000.0, 2) + " s" : juce::String ((int) ms) + " ms", (int) x - 30, (int) t.getBottom() + 4, 60, 12, juce::Justification::centred);
            }
            g.setColour (text (0.12f)); g.drawHorizontalLine ((int) yMid, t.getX(), t.getRight());
            // the dry hit
            g.setColour (cream()); g.fillRect (t.getX() - 1.0f, yMid - half, 2.5f, half * 2);
            // repeats: simulate the routing's energy flow (levels only), darken by the loop's high cut
            const double hcut = sp->bank[P::highCut], darkPerPass = juce::jlimit (0.0, 1.0, 1.0 - hcut / 20000.0);
            double l = 1.0, r = 1.0; double tl = 0, tr = 0;
            if (routing == 1) r = 0;   // ping-pong: the input goes in on the left
            for (int k = 1; k <= 24; ++k)
            {
                double nl, nr, ntl, ntr;
                if (routing == 1) { nl = r * fb; nr = l * fb; ntl = tr + tL; ntr = tl + tR; if (k == 1) { nl = 1.0; nr = 0; ntl = tL; ntr = tR; } }
                else if (routing == 2) { nl = (l * (1 - xf) + r * xf) * (k == 1 ? 1.0 : fb); nr = (r * (1 - xf) + l * xf) * (k == 1 ? 1.0 : fb); ntl = tl + tL; ntr = tr + tR; if (k == 1) { nl = 1; nr = 1; } }
                else { nl = k == 1 ? 1.0 : l * fb; nr = k == 1 ? 1.0 : r * fb; ntl = tl + tL; ntr = tr + tR; }
                if (frozen && k > 1) { nl = l; nr = r; }
                l = nl; r = nr; tl = ntl; tr = ntr;
                const float dark = (float) juce::jlimit (0.0, 1.0, darkPerPass * (k - 1) * 0.6);
                const juce::Colour c = teal().interpolatedWith (blue().darker (0.6f), dark);
                for (int side = 0; side < 2; ++side)
                {
                    const double amp = side == 0 ? l : r, ms = side == 0 ? tl : tr;
                    if (amp < 0.001 || ms > span) continue;
                    const float x = xAt (ms), h = hOf (20.0 * std::log10 (amp));
                    const float y0 = side == 0 ? yMid - h : yMid;
                    g.setColour (c.withAlpha (0.18f)); g.fillRoundedRectangle (x - 4.0f, y0, 8.0f, h, 3.0f);
                    g.setColour (c); g.fillRoundedRectangle (x - 1.5f, y0, 3.0f, h, 1.5f);
                    g.setColour (cream().withAlpha (0.9f - dark * 0.6f)); g.fillEllipse (x - 2.5f, (side == 0 ? y0 : y0 + h) - 2.5f, 5, 5);
                }
            }
            g.setColour (text (0.35f)); g.setFont (Theme::font (8.5f, true));
            g.drawText ("L", (int) t.getX() + 4, (int) t.getY(), 12, 12, juce::Justification::left);
            g.drawText ("R", (int) t.getX() + 4, (int) t.getBottom() - 12, 12, 12, juce::Justification::left);
            // readout
            g.setColour (cream()); g.setFont (Theme::font (20.0f, true));
            const juce::String tt = sp->linkLR.get() ? timeLabel (0) : timeLabel (0) + "  /  " + timeLabel (1);
            g.drawText (frozen ? juce::String ("FROZEN") : tt, t.withTrimmedLeft (t.getWidth() - 360.0f).withHeight (24.0f).translated (0, 2), juce::Justification::right);
            // live wet strip
            auto strip = trailPane().reduced (16.0f, 6.0f).removeFromBottom (18.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (strip, 4.0f);
            juce::Path wet; const int Nn = P::histLen, w = sp->histW.load(), cols = 240;
            for (int i = 0; i <= cols; ++i) { const int idx = (w - 1 - (cols - i)) & (Nn - 1); const float x = strip.getX() + strip.getWidth() * i / cols, y = strip.getBottom() - juce::jlimit (0.0f, 1.0f, (sp->histWet[(size_t) idx] + 60.0f) / 60.0f) * strip.getHeight();
                if (i == 0) wet.startNewSubPath (x, y); else wet.lineTo (x, y); }
            g.setColour (teal()); g.strokePath (wet, juce::PathStrokeType (1.0f));
            if (sp->bank[P::duck] > 0.05f) { g.setColour (text (0.6f)); g.setFont (Theme::font (8.0f, true)); g.drawText ("DUCK " + juce::String (sp->duckNow.load(), 1) + " dB", strip.reduced (6.0f, 0), juce::Justification::right); }
        }
        juce::String timeLabel (int side) const
        {
            const int s = side == 0 || sp->linkLR.get() ? sp->syncL.get() : sp->syncR.get();
            const double ms = sp->timeMs (side);
            return (s > 0 ? P::syncNames()[s] + "  " : juce::String()) + (ms >= 1000 ? juce::String (ms / 1000.0, 2) + " s" : juce::String (ms, ms < 100 ? 1 : 0) + " ms");
        }

        // ---- the loop
        juce::Rectangle<float> filterR() const { auto r = loopPane().reduced (14.0f, 10.0f); r.removeFromTop (16.0f); return r.removeFromTop (r.getHeight() * 0.48f); }
        float fx (double hz) const { const auto r = filterR().reduced (8.0f, 14.0f); return r.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * r.getWidth(); }
        void paintLoop (juce::Graphics& g)
        {
            pane (g, loopPane(), "THE LOOP  -  what every repeat passes through");
            const auto fr = filterR(); const auto pr = fr.reduced (8.0f, 14.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (fr, 6.0f);
            const double lo = sp->bank[P::lowCut], hi = sp->bank[P::highCut];
            juce::Path c;
            for (int i = 0; i <= 120; ++i)
            {
                const double hz = 20.0 * std::pow (1000.0, i / 120.0); double m = 1;
                if (lo > 10.5) { const double a = std::pow (hz / lo, 4.0); m *= std::sqrt (a / (1 + a)); }
                if (hi < 19999) { const double b = std::pow (hz / hi, 4.0); m *= std::sqrt (1 / (1 + b)); }
                const float y = pr.getY() + (float) juce::jlimit (0.0, 1.0, -20 * std::log10 (std::max (1e-6, m)) / 30.0) * pr.getHeight();
                if (i == 0) c.startNewSubPath (fx (hz), y); else c.lineTo (fx (hz), y);
            }
            juce::Path fill (c); fill.lineTo (pr.getRight(), pr.getBottom()); fill.lineTo (pr.getX(), pr.getBottom()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (teal().withAlpha (0.25f), 0, pr.getY(), teal().withAlpha (0.02f), 0, pr.getBottom(), false)); g.fillPath (fill);
            g.setColour (teal()); g.strokePath (c, juce::PathStrokeType (1.8f));
            for (auto [hz, id] : { std::pair<double, int> { std::max (20.0, lo), 1 }, { std::min (20000.0, hi), 2 } })
            {
                const float x = fx (hz), y = pr.getY() + pr.getHeight() * 0.1f;
                g.setColour (hot == id ? cream() : teal()); g.fillEllipse (x - 6, y - 6, 12, 12); g.setColour (night()); g.fillEllipse (x - 2.5f, y - 2.5f, 5, 5);
            }
            g.setColour (text (0.6f)); g.setFont (Theme::font (9.0f, true));
            g.drawText ((lo > 10.5 ? "LOW CUT " + juce::String ((int) lo) + " Hz" : juce::String ("LOW CUT OFF")) + "     " + (hi < 19999 ? "HIGH CUT " + juce::String (hi / 1000.0, 1) + " kHz" : juce::String ("HIGH CUT OFF")),
                        fr.reduced (6.0f, 2.0f).removeFromBottom (12.0f), juce::Justification::centred);
            // the reel: turns with the delay speed, wobbles with the wow
            auto rr = loopPane().reduced (14.0f, 12.0f); rr.removeFromTop (16.0f + filterR().getHeight() + 10.0f);
            const float R = std::min (rr.getWidth(), rr.getHeight()) * 0.42f; const auto ctr = rr.getCentre();
            const double wobble = std::sin (reelAngle * 0.37) * sp->bank[P::wow] / 100.0 * 0.15;
            const float ang = (float) (reelAngle + wobble);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b2a1c), ctr.x, ctr.y, juce::Colour (0xff120c08), ctr.x, ctr.y + R, true));
            const float tapeR = R * (0.55f + 0.4f * (float) (sp->bank[P::feedback] / 110.0));   // more feedback, a fuller reel
            g.fillEllipse (ctr.x - tapeR, ctr.y - tapeR, tapeR * 2, tapeR * 2);
            g.setColour (juce::Colour (0xff5a4230).withAlpha (0.6f)); for (float q = R * 0.45f; q < tapeR; q += 3.0f) g.drawEllipse (ctr.x - q, ctr.y - q, q * 2, q * 2, 0.5f);
            juce::Path flange; flange.addEllipse (ctr.x - R, ctr.y - R, R * 2, R * 2);
            for (int k = 0; k < 3; ++k)
            {
                const float a0 = ang + juce::MathConstants<float>::twoPi * k / 3.0f;
                juce::Path win; win.addCentredArc (ctr.x, ctr.y, R * 0.78f, R * 0.78f, 0, a0 + 0.35f, a0 + 1.65f, true);
                win.lineTo (ctr.x + R * 0.32f * std::sin (a0 + 1.65f), ctr.y - R * 0.32f * std::cos (a0 + 1.65f));
                win.addCentredArc (ctr.x, ctr.y, R * 0.32f, R * 0.32f, 0, a0 + 1.65f, a0 + 0.35f, false); win.closeSubPath();
                flange.addPath (win);
            }
            flange.setUsingNonZeroWinding (false);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9d3dc).withAlpha (0.55f), ctr.x - R, ctr.y - R, juce::Colour (0xff3a4652).withAlpha (0.55f), ctr.x + R, ctr.y + R, false)); g.fillPath (flange);
            g.setColour (teal().withAlpha (0.6f)); g.drawEllipse (ctr.x - R, ctr.y - R, R * 2, R * 2, 1.2f);
            g.setColour (cream()); g.fillEllipse (ctr.x - R * 0.12f, ctr.y - R * 0.12f, R * 0.24f, R * 0.24f);
            g.setColour (text (0.55f)); g.setFont (Theme::font (9.0f, true));
            g.drawText (juce::String (SqSpindle::characterName (sp->character.get())).toUpperCase() + "  -  " + juce::String (SqSpindle::routingName (sp->routing.get())).toUpperCase(), rr.removeFromBottom (12.0f), juce::Justification::centred);
        }

        // ---- top bar
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t) { return juce::GlyphArrangement::getStringWidth (Theme::font (9.5f, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c)
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.15f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (9.5f, true)); g.drawText (t, r, juce::Justification::centred);
            hits.push_back ({ r, id });
        }
        void group (juce::Graphics& g, juce::Rectangle<float>& bar, const juce::StringArray& names, int cur, int base, juce::Colour c)
        { for (int i = 0; i < names.size(); ++i) { const auto t = names[i].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (t) + 20.0f), t, cur == i, base + i, c); bar.removeFromLeft (3.0f); } bar.removeFromLeft (12.0f); }
        void paintTopBar (juce::Graphics& g)
        {
            auto bar = topBar();
            group (g, bar, P::characterNames(), sp->character.get(), 10, teal());
            group (g, bar, P::routingNames(), sp->routing.get(), 20, blue());
            const auto sl = "L: " + P::syncNames()[juce::jlimit (0, 12, sp->syncL.get())].toUpperCase();
            chip (g, bar.removeFromLeft (widthOf (sl) + 22.0f), sl, sp->syncL.get() > 0, 30, cream()); bar.removeFromLeft (3.0f);
            if (! sp->linkLR.get()) { const auto sr = "R: " + P::syncNames()[juce::jlimit (0, 12, sp->syncR.get())].toUpperCase(); chip (g, bar.removeFromLeft (widthOf (sr) + 22.0f), sr, sp->syncR.get() > 0, 31, cream()); bar.removeFromLeft (3.0f); }
            chip (g, bar.removeFromLeft (70.0f), "LINK L/R", sp->linkLR.get(), 32, cream()); bar.removeFromLeft (12.0f);
            chip (g, bar.removeFromLeft (72.0f), sp->freeze.get() ? "FROZEN" : "FREEZE", sp->freeze.get(), 33, juce::Colour (0xff8fe3ff));
        }
        juce::UndoManager* um() const { return &sp->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Spindle setting"); auto& s = *sp;
            if (id >= 10 && id < 14)
            {   // a character brings its own wow, flutter and diffusion (the knobs move to them; you can change them after)
                s.character.setValue (id - 10, um()); const auto d = SqSpindle::defaultsFor (id - 10);
                setValue (P::wow, (float) d.wow * 100.0f); setValue (P::flutter, (float) d.flutter * 100.0f); setValue (P::diffusion, (float) d.diffusion * 100.0f);
                changed(); return;
            }
            if (id >= 20 && id < 23) { s.routing.setValue (id - 20, um()); changed(); return; }
            switch (id)
            {
                case 30: case 31:
                {
                    juce::PopupMenu m; const auto names = P::syncNames(); const int cur = id == 30 ? s.syncL.get() : s.syncR.get();
                    for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, i == 0 ? juce::String ("Free (the Time knob)") : names[i], true, i == cur);
                    m.showMenuAsync (juce::PopupMenu::Options(), [this, id, ptr = SafePointer<SpindleViz> (this)] (int r) { if (r > 0 && ptr != nullptr) { (id == 30 ? sp->syncL : sp->syncR).setValue (r - 1, um()); changed(); } });
                    break;
                }
                case 32: s.linkLR.setValue (! s.linkLR.get(), um()); changed(); break;
                case 33: s.freeze.setValue (! s.freeze.get(), um()); changed(); break;
                default: break;
            }
        }

        // ---- the deck: reel knobs
        struct KnobDef { int param; const char* label; };
        std::vector<KnobDef> defs() const
        {
            std::vector<KnobDef> d { { P::timeL, sp->linkLR.get() ? "TIME" : "TIME L" } };
            if (! sp->linkLR.get()) d.push_back ({ P::timeR, "TIME R" });
            for (auto k : std::initializer_list<KnobDef> { { P::feedback, "FEEDBACK" }, { P::crossFeed, "CROSS" }, { P::drive, "DRIVE" }, { P::wow, "WOW" }, { P::flutter, "FLUTTER" },
                                                         { P::diffusion, "DIFFUSION" }, { P::width, "WIDTH" }, { P::duck, "DUCK" }, { P::duckRelease, "DUCK REL" }, { P::mix, "MIX" }, { P::output, "OUTPUT" } })
                d.push_back (k);
            return d;
        }
        std::vector<juce::Rectangle<float>> knobR;
        int dragKnob = -1, hoverKnob = -1, hot = 0, dragHot = 0; float dragStartNorm = 0; juce::Point<float> dragStart;
        te::AutomatableParameter& param (int i) const { return *sp->bank.params[(size_t) i]; }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        bool enabled (int pi_) const
        {
            if ((pi_ == P::timeL && sp->syncL.get() > 0) || (pi_ == P::timeR && sp->syncR.get() > 0)) return false;
            if (pi_ == P::crossFeed && sp->routing.get() != 2) return false;
            if (pi_ == P::duckRelease && sp->bank[P::duck] < 0.05f) return false;
            return true;
        }
        void layoutKnobs() { knobR.clear(); auto d = deck().reduced (12.0f, 12.0f); const auto ds = defs(); const int n = (int) ds.size(); for (int i = 0; i < n; ++i) knobR.push_back ({ d.getX() + d.getWidth() / n * i, d.getY(), d.getWidth() / n, d.getHeight() }); }
        static void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, bool hotK, bool active, bool en)
        {
            const float al = en ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (15.0f); auto read = cell.removeFromBottom (24.0f).withTrimmedTop (3.0f);
            const float rO = std::min (cell.getWidth() - 8.0f, cell.getHeight()) * 0.5f - 2.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text ((hotK ? 0.95f : 0.55f) * al)); g.setFont (Theme::font (8.5f, true));
            juce::String spaced; for (auto ch : label) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x2009); }
            g.drawText (spaced, lab, juce::Justification::centred);
            const float ar = rO * 0.96f;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f * al)); g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, a0, av, true);
            g.setColour (teal().withAlpha (0.25f * al)); g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (blue().withAlpha (al), c.x - ar, c.y + ar, teal().withAlpha (al), c.x + ar, c.y - ar, false));
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // a small reel: a steel flange with three windows, turned to the value
            const float R = rO * 0.72f;
            juce::Path flange; flange.addEllipse (c.x - R, c.y - R, R * 2, R * 2);
            for (int k = 0; k < 3; ++k)
            {
                const float b0 = av + juce::MathConstants<float>::twoPi * k / 3.0f;
                juce::Path win; win.addCentredArc (c.x, c.y, R * 0.80f, R * 0.80f, 0, b0 + 0.40f, b0 + 1.70f, true);
                win.lineTo (c.x + R * 0.36f * std::sin (b0 + 1.70f), c.y - R * 0.36f * std::cos (b0 + 1.70f));
                win.addCentredArc (c.x, c.y, R * 0.36f, R * 0.36f, 0, b0 + 1.70f, b0 + 0.40f, false); win.closeSubPath();
                flange.addPath (win);
            }
            flange.setUsingNonZeroWinding (false);
            g.setColour (juce::Colour (0xff0a0f16).withAlpha (al)); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);   // dark behind the windows
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8e0e8).withAlpha (al), c.x - R, c.y - R, juce::Colour (0xff4a5662).withAlpha (al), c.x + R, c.y + R, false)); g.fillPath (flange);
            g.setColour (juce::Colours::black.withAlpha (0.5f * al)); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
            // hub + the pointer notch on the rim
            g.setColour (cream().withAlpha (al)); g.fillEllipse (c.x - R * 0.16f, c.y - R * 0.16f, R * 0.32f, R * 0.32f);
            const juce::Point<float> p1 (c.x + R * 0.82f * std::sin (av), c.y - R * 0.82f * std::cos (av)), p2 (c.x + R * 1.0f * std::sin (av), c.y - R * 1.0f * std::cos (av));
            g.setColour (teal().withAlpha (al)); g.drawLine ({ p1, p2 }, 3.0f);
            if (hotK) { g.setColour (teal().withAlpha (0.7f)); g.drawEllipse (c.x - R - 2, c.y - R - 2, R * 2 + 4, R * 2 + 4, 1.0f); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 4.0f, 84.0f), 19.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 9.5f);
            g.setColour (active ? teal() : text (0.1f)); g.drawRoundedRectangle (pill, 9.5f, 1.0f);
            g.setColour ((active ? teal() : text (0.95f)).withMultipliedAlpha (al)); g.setFont (Theme::font (10.0f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        juce::String knobText (int i) const
        {
            const float v = param (i).getCurrentValue();
            switch (i)
            {
                case P::timeL: case P::timeR: { const double ms = sp->timeMs (i == P::timeL ? 0 : 1); return ms >= 1000 ? juce::String (ms / 1000.0, 2) + " s" : juce::String (ms, ms < 100 ? 1 : 0) + " ms"; }
                case P::lowCut:  return v <= 10.5f ? juce::String ("OFF") : juce::String ((int) std::lround (v)) + " Hz";
                case P::highCut: return v >= 19999.0f ? juce::String ("OFF") : juce::String (v / 1000.0f, 1) + " kHz";
                case P::duck:    return v < 0.05f ? juce::String ("OFF") : juce::String (v, 1) + " dB";
                case P::duckRelease: return juce::String ((int) std::lround (v)) + " ms";
                case P::output:  return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
                default:         return juce::String ((int) std::lround (v)) + " %";
            }
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1220).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (teal().withAlpha (0.18f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs(); const auto ds = defs();
            for (size_t i = 0; i < ds.size(); ++i) drawKnob (g, knobR[i], ds[i].label, knobText (ds[i].param), norm (ds[i].param), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, enabled (ds[i].param));
            for (size_t i = 0; i < ds.size(); ++i) StudioUi::knobScale (g, knobR[i], sp->bank.specs[(size_t) ds[i].param], text (1.0f), 15.0f, 24.0f, enabled (ds[i].param) ? 1.0f : 0.4f);
        }

        // ---- mouse
        int hotAt (juce::Point<float> pt) const
        {
            if (! filterR().contains (pt)) return 0;
            if (std::abs (pt.x - fx (std::max (20.0f, sp->bank[P::lowCut]))) < 8) return 1;
            if (std::abs (pt.x - fx (std::min (20000.0f, sp->bank[P::highCut]))) < 8) return 2;
            return 0;
        }
        int knobAt (juce::Point<float> p) const { const auto ds = defs(); for (size_t i = 0; i < knobR.size() && i < ds.size(); ++i) if (knobR[i].reduced (4.0f, 0).contains (p) && enabled (ds[i].param)) return (int) i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), h = hotAt (e.position);
            setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : h ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || h != hot) { hoverKnob = k; hot = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hot = 0; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (sp == nullptr) return;
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (const int h = hotAt (e.position)) { dragHot = h; um()->beginNewTransaction ("Spindle loop filter"); param (h == 1 ? P::lowCut : P::highCut).parameterChangeGestureBegin(); return; }
            const int k = knobAt (e.position);
            if (k >= 0) { const auto d = defs()[(size_t) k]; dragKnob = k; dragStart = e.position; dragStartNorm = norm (d.param); um()->beginNewTransaction (juce::String ("Spindle ") + d.label); param (d.param).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragHot)
            {
                const auto pr = filterR().reduced (8.0f, 14.0f); const double hz = 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((e.position.x - pr.getX()) / pr.getWidth())));
                setValue (dragHot == 1 ? P::lowCut : P::highCut, (float) hz); changed(); return;
            }
            if (dragKnob < 0) return;
            setNorm (defs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragHot) param (dragHot == 1 ? P::lowCut : P::highCut).parameterChangeGestureEnd();
            if (dragKnob >= 0) param (defs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; dragHot = 0; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = defs()[(size_t) k].param; um()->beginNewTransaction ("Spindle reset"); param (pi_).setParameter (sp->bank.specs[(size_t) pi_].def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Spindle wheel"); const int pi_ = defs()[(size_t) k].param; setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
