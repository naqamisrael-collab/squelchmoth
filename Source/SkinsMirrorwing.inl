// Included inside namespace Skins (PluginSkins.h): the Mirrorwing face (1.14 Studio Line, phase K - the flagship synth).
//
// Squelchmoth's own "mirrored glass wings" design on the Higgsfield plate pa_mirrorwing:
//  - top bar: voice mode, glide mode, quality, bend range, max voices, filter routing (series / parallel), voices sounding
//  - the SOURCE STRIP: every modulation source as a chip - drag one onto any knob to route it there (a ring appears on the
//    knob showing how far it moves it; the routing lands in the matrix)
//  - four OSCILLATOR cards: one true cycle each (shape, width, fold), on / route / unison / sync / free-phase chips, numbers
//  - FILTERS: both filters' real curves and the combined curve, a draggable node each, type and slope chips
//  - ENVELOPES (1-4, tabs): delay / attack / hold / decay / sustain / release drawn to time, the voice's level riding it
//  - LFOs (1-4, tabs): one cycle of the shape with the start phase and the live value; shape, sync and retrigger chips
//  - MATRIX: 16 rows - source, target, amount (drag) in the target's units
//  - the deck: three rows of chrome-and-prism knobs with their ranges and modulation rings; L / R meters with numbers

    struct MirrorwingViz : public Viz, private juce::Timer
    {
        using P = SqMirrorwingPlugin;
        MirrorwingViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c), mp (dynamic_cast<SqMirrorwingPlugin*> (&p))
        { setInterceptsMouseClicks (true, false); meter.labIn = "L"; meter.labOut = "R"; startTimerHz (30); }
        SqMirrorwingPlugin* mp;
        std::function<void()> onChange;
        int selOsc = 0, selF = 0, selEnv = 0, selLfo = 0;

        static juce::Colour night()  { return juce::Colour (0xff07060d); }
        static juce::Colour violet() { return juce::Colour (0xffb48cff); }
        static juce::Colour cyan()   { return juce::Colour (0xff5ee7ff); }
        static juce::Colour gold()   { return juce::Colour (0xfff2c46b); }
        static juce::Colour rose()   { return juce::Colour (0xffff7ab8); }
        static juce::Colour text (float a = 0.85f) { return juce::Colour (0xfff3f0ff).withAlpha (a); }
        static juce::Colour oscCol (int k) { static const juce::uint32 c[4] = { 0xff5ee7ff, 0xffb48cff, 0xfff2c46b, 0xffff7ab8 }; return juce::Colour (c[juce::jlimit (0, 3, k)]); }
        static juce::Colour srcCol (int s)
        {
            using namespace SqMirror;
            if (s >= sEnv1 && s <= sEnv4) return violet(); if (s >= sLfo1 && s <= sLfo4) return cyan(); if (s >= sMacro1) return rose(); return gold();
        }

        // ---- geometry
        static constexpr float deckH = 352.0f;
        juce::Rectangle<float> all() const     { return getLocalBounds().toFloat(); }
        juce::Rectangle<float> inner() const   { return all().reduced (16.0f, 0); }
        juce::Rectangle<float> topBar() const  { return inner().removeFromTop (48.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f); }
        juce::Rectangle<float> srcStrip() const { auto r = inner(); r.removeFromTop (48.0f); return r.removeFromTop (30.0f); }
        juce::Rectangle<float> deck() const    { return inner().removeFromBottom (deckH).withTrimmedBottom (14.0f); }
        static float meterW() { return StudioUi::StudioMeter::width (false); }
        juce::Rectangle<float> body() const    { auto r = inner(); r.removeFromTop (84.0f); r.removeFromBottom (deckH + 8.0f); return r; }
        juce::Rectangle<float> meterR() const  { return body().removeFromRight (meterW()); }
        juce::Rectangle<float> main() const    { auto r = body(); r.removeFromRight (meterW() + 10.0f); return r; }
        juce::Rectangle<float> oscRow() const  { return main().removeFromTop (172.0f); }
        juce::Rectangle<float> oscCard (int k) const { auto r = oscRow(); const float w = (r.getWidth() - 3 * 10.0f) / 4.0f; return { r.getX() + k * (w + 10.0f), r.getY(), w, r.getHeight() }; }
        juce::Rectangle<float> rowB() const    { auto r = main(); r.removeFromTop (180.0f); return r; }
        juce::Rectangle<float> fltPane() const { auto r = rowB(); return r.removeFromLeft (r.getWidth() * 0.34f).withTrimmedRight (5.0f); }
        juce::Rectangle<float> envPane() const { auto r = rowB(); r.removeFromLeft (r.getWidth() * 0.34f); return r.removeFromLeft (rowB().getWidth() * 0.22f).withTrimmedLeft (5.0f).withTrimmedRight (5.0f); }
        juce::Rectangle<float> lfoPane() const { auto r = rowB(); r.removeFromLeft (r.getWidth() * 0.56f); return r.removeFromLeft (rowB().getWidth() * 0.18f).withTrimmedLeft (5.0f).withTrimmedRight (5.0f); }
        juce::Rectangle<float> matPane() const { auto r = rowB(); r.removeFromLeft (r.getWidth() * 0.74f); return r.withTrimmedLeft (5.0f); }
        juce::Rectangle<float> fPlot() const   { return fltPane().reduced (12.0f, 0).withTrimmedTop (56.0f).withTrimmedBottom (22.0f).withTrimmedRight (22.0f); }

        void pane (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour c)
        {
            g.setColour (night().withAlpha (0.78f)); g.fillRoundedRectangle (r, 10.0f);
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.08f), 0, r.getY(), c.withAlpha (0.0f), 0, r.getY() + 40.0f, false)); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (c.withAlpha (0.22f)); g.drawRoundedRectangle (r, 10.0f, 1.0f);
            if (title.isNotEmpty()) { g.setColour (c.withAlpha (0.9f)); g.setFont (Theme::font (9.0f, true)); g.drawText (title, r.reduced (12.0f, 8.0f).removeFromTop (12.0f), juce::Justification::left); }
        }

        // ---- params
        SqDsp::ParamBank& bank() const { return mp->theBank(); }
        te::AutomatableParameter& param (int i) const { return *bank().params[(size_t) i]; }
        const SqDsp::ParamSpec& spec (int i) const { return bank().specs[(size_t) i]; }
        float val (int i) const { return param (i).getCurrentValue(); }
        float norm (int i) const { auto& p = param (i); return p.valueRange.convertTo0to1 (p.getCurrentValue()); }
        void setNorm (int i, float v) { auto& p = param (i); p.setParameter (p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); changed(); }
        void setValue (int i, float v) { auto& p = param (i); p.setParameter (juce::jlimit (p.valueRange.start, p.valueRange.end, v), juce::sendNotification); }
        juce::UndoManager* um() const { return &mp->edit.getUndoManager(); }
        void changed() { repaint(); if (onChange) onChange(); }

        // ---- live
        StudioUi::StudioMeter meter; std::array<float, 4> envNow {}, lfoNow {}; std::array<float, 2> cutNow {}; int vNote = -1;
        void timerCallback() override
        {
            if (mp == nullptr) return;
            float l, r; mp->takePeaks (l, r); meter.update (l, r, 0.0f);
            if (! mp->newest (envNow, lfoNow, cutNow, vNote)) { envNow.fill (0); lfoNow.fill (0); }
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (mp == nullptr) return;
            const auto a = all();
            g.setColour (night()); g.fillRoundedRectangle (a, 10.0f);
            {
                static const juce::Image plate = artFor ("mirrorwing");
                if (plate.isValid())
                {
                    juce::Graphics::ScopedSaveState s (g);
                    juce::Path clip; clip.addRoundedRectangle (a, 10.0f); g.reduceClipRegion (clip);
                    g.setOpacity (0.9f); g.drawImage (plate, a, juce::RectanglePlacement::fillDestination); g.setOpacity (1.0f);
                    g.setGradientFill (juce::ColourGradient (night().withAlpha (0.55f), a.getCentreX(), a.getY(), night().withAlpha (0.9f), a.getCentreX(), a.getBottom(), false));
                    g.fillRect (a);
                }
            }
            g.setColour (violet().withAlpha (0.26f)); g.drawRoundedRectangle (a.reduced (0.5f), 10.0f, 1.0f);
            hits.clear();
            paintTopBar (g); paintSources (g);
            for (int k = 0; k < P::numOsc; ++k) paintOsc (g, k);
            paintFilters (g); paintEnv (g); paintLfo (g); paintMatrix (g); paintDeck (g);
            meter.draw (g, meterR(), cyan(), false);
            if (dragSrc > 0)
            {
                const auto t = SqMirror::sourceNames()[dragSrc].toUpperCase(); auto r = juce::Rectangle<float> (dragPos.x - 34, dragPos.y - 11, 68, 22);
                g.setColour (srcCol (dragSrc).withAlpha (0.35f)); g.fillRoundedRectangle (r, 11.0f); g.setColour (srcCol (dragSrc)); g.drawRoundedRectangle (r, 11.0f, 1.4f);
                g.setColour (text (1.0f)); g.setFont (Theme::font (8.5f, true)); g.drawText (t, r, juce::Justification::centred);
                if (const int k = knobAt (dragPos); k >= 0 && P::targetOfParam (defs()[(size_t) k].param) > 0) { g.setColour (srcCol (dragSrc)); g.drawRoundedRectangle (knobR[(size_t) k].reduced (2.0f), 8.0f, 1.6f); }
            }
        }

        // ---- chips
        struct Hit { juce::Rectangle<float> r; int id; };
        std::vector<Hit> hits;
        static float widthOf (const juce::String& t, float sz = 9.0f) { return juce::GlyphArrangement::getStringWidth (Theme::font (sz, true), t); }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, int id, juce::Colour c, float fsz = 9.0f)
        {
            g.setColour (on ? c.withAlpha (0.26f) : text (0.05f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (on ? c.withAlpha (0.95f) : text (0.16f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (text (on ? 1.0f : 0.72f)); g.setFont (Theme::font (fsz, true)); g.drawText (t, r, juce::Justification::centred);
            if (id >= 0) hits.push_back ({ r, id });
        }
        void paintTopBar (juce::Graphics& g)
        {
            auto bar = topBar(); g.setFont (Theme::font (9.0f, true));
            g.setColour (text (0.5f)); g.drawText ("VOICE", bar.removeFromLeft (42.0f), juce::Justification::centredLeft);
            const auto pm = SqSynth::polyModeNames();
            for (int i = 0; i < 3; ++i) { chip (g, bar.removeFromLeft (widthOf (pm[i]) + 20.0f), pm[i], mp->polyModeValue().get() == i, 10 + i, violet()); bar.removeFromLeft (3.0f); }
            bar.removeFromLeft (8.0f); g.setColour (text (0.5f)); g.drawText ("GLIDE", bar.removeFromLeft (40.0f), juce::Justification::centredLeft);
            const auto gm = SqSynth::glideModeNames();
            for (int i = 0; i < 3; ++i) { chip (g, bar.removeFromLeft (widthOf (gm[i]) + 20.0f), gm[i], mp->glideModeValue().get() == i, 20 + i, violet()); bar.removeFromLeft (3.0f); }
            bar.removeFromLeft (8.0f);
            const auto q = mp->qualityValue().get() == 1 ? juce::String ("HD  2X") : juce::String ("ECO");
            chip (g, bar.removeFromLeft (widthOf (q) + 22.0f), q, mp->qualityValue().get() == 1, 31, cyan()); bar.removeFromLeft (3.0f);
            const auto bd = "BEND " + juce::String::charToString (0x00b1) + juce::String (mp->bendRangeValue().get());
            chip (g, bar.removeFromLeft (widthOf (bd) + 20.0f), bd, false, 32, cyan()); bar.removeFromLeft (3.0f);
            const auto mx = "MAX " + juce::String (mp->maxPolyValue().get());
            chip (g, bar.removeFromLeft (widthOf (mx) + 20.0f), mx, false, 33, cyan()); bar.removeFromLeft (10.0f);
            g.setColour (text (0.5f)); g.drawText ("FILTERS", bar.removeFromLeft (54.0f), juce::Justification::centredLeft);
            chip (g, bar.removeFromLeft (64.0f), "SERIES", ! mp->parallel.get(), 34, gold()); bar.removeFromLeft (3.0f);
            chip (g, bar.removeFromLeft (72.0f), "PARALLEL", mp->parallel.get(), 35, gold()); bar.removeFromLeft (10.0f);
            const auto dt = "DELAY " + P::delayNames()[juce::jlimit (0, 5, mp->delayTime.get())].toUpperCase();
            chip (g, bar.removeFromLeft (widthOf (dt) + 20.0f), dt, false, 36, rose());
            g.setColour (text (0.55f)); g.setFont (Theme::font (9.5f, true));
            g.drawText ("VOICES " + juce::String (mp->activeVoiceCount()) + " / " + juce::String (mp->maxVoiceCount()) + (vNote >= 0 ? "   NOTE " + juce::MidiMessage::getMidiNoteName (vNote, true, true, 3) : juce::String()), bar, juce::Justification::right);
        }
        std::array<juce::Rectangle<float>, SqMirror::numSources> srcR {};
        void paintSources (juce::Graphics& g)
        {
            auto r = srcStrip().withTrimmedTop (3.0f).withTrimmedBottom (5.0f);
            g.setColour (text (0.5f)); g.setFont (Theme::font (8.5f, true)); g.drawText ("DRAG A SOURCE ONTO ANY KNOB", r.removeFromLeft (180.0f), juce::Justification::centredLeft);
            const auto names = SqMirror::sourceNames(); const float w = (r.getWidth() - 3.0f * (names.size() - 2)) / (names.size() - 1);
            for (int s = 1; s < names.size(); ++s)
            {
                srcR[(size_t) s] = r.removeFromLeft (w); r.removeFromLeft (3.0f);
                // a source in use glows
                bool used = false; for (int m = 0; m < P::numSlots; ++m) used = used || (mp->modSrc[(size_t) m].get() == s && mp->modDst[(size_t) m].get() > 0);
                chip (g, srcR[(size_t) s], names[s].toUpperCase(), used, -1, srcCol (s), 7.5f);
                // the live value of envelopes and LFOs as a small bar under the chip
                float v = -2; if (s >= SqMirror::sEnv1 && s <= SqMirror::sEnv4) v = envNow[(size_t) (s - SqMirror::sEnv1)]; else if (s >= SqMirror::sLfo1 && s <= SqMirror::sLfo4) v = 0.5f + 0.5f * lfoNow[(size_t) (s - SqMirror::sLfo1)];
                if (v > -1) { auto b = srcR[(size_t) s].withTop (srcR[(size_t) s].getBottom() + 1.0f).withHeight (2.0f).reduced (8.0f, 0); g.setColour (srcCol (s).withAlpha (0.25f)); g.fillRect (b); g.setColour (srcCol (s)); g.fillRect (b.withWidth (b.getWidth() * juce::jlimit (0.0f, 1.0f, v))); }
            }
        }

        // ---- oscillators
        void drawCycle (juce::Graphics& g, juce::Rectangle<float> wv, double shape, double width, double fold, juce::Colour c, float alpha)
        {
            juce::Path cyc; const int N = 160;
            for (int i = 0; i <= N; ++i)
            {
                const double t = i / (double) N; SqMote::Osc o; o.phase = t; o.inc = 1.0 / 2048.0;
                const double v = SqMirror::Voice::fold (o.value (shape, width), fold);
                const float x = wv.getX() + 4 + (wv.getWidth() - 8) * (float) t, y = wv.getCentreY() - (float) juce::jlimit (-1.2, 1.2, v) * wv.getHeight() * 0.38f;
                if (i == 0) cyc.startNewSubPath (x, y); else cyc.lineTo (x, y);
            }
            g.setColour (c.withAlpha (0.3f * alpha)); g.strokePath (cyc, juce::PathStrokeType (5.0f)); g.setColour (c.withAlpha (alpha)); g.strokePath (cyc, juce::PathStrokeType (1.6f));
        }
        void paintOsc (juce::Graphics& g, int k)
        {
            const auto r = oscCard (k); const auto c = oscCol (k); const bool on = mp->oscOn[(size_t) k].get(), sel = k == selOsc;
            pane (g, r, "", c);
            if (sel) { g.setColour (c.withAlpha (0.7f)); g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.6f); }
            hits.push_back ({ r.withHeight (26.0f).withTrimmedLeft (60.0f), 1200 + k });   // the title strip selects
            auto in = r.reduced (10.0f, 8.0f); auto head = in.removeFromTop (20.0f);
            chip (g, head.removeFromLeft (48.0f), on ? "ON" : "OFF", on, 1000 + k, c, 8.5f); head.removeFromLeft (8.0f);
            g.setColour (c.withAlpha (on ? 1.0f : 0.5f)); g.setFont (Theme::font (10.0f, true)); g.drawText ("OSC " + juce::String (k + 1), head.removeFromLeft (52.0f), juce::Justification::centredLeft);
            g.setColour (text (0.7f)); g.setFont (Theme::font (8.0f, true));
            const int oc = (int) val (mp->po (k, P::oOct)), se = (int) val (mp->po (k, P::oSemi)), fi = (int) val (mp->po (k, P::oFine));
            g.drawText ((oc >= 0 ? "+" : "") + juce::String (oc) + " oct  " + (se >= 0 ? "+" : "") + juce::String (se) + " st  " + (fi >= 0 ? "+" : "") + juce::String (fi) + " ct", head, juce::Justification::centredRight);
            in.removeFromTop (4.0f);
            auto chipsRow = in.removeFromBottom (20.0f); in.removeFromBottom (4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (in, 6.0f);
            g.setColour (text (0.07f)); g.drawHorizontalLine ((int) in.getCentreY(), in.getX() + 4, in.getRight() - 4);
            const double shp = val (mp->po (k, P::oShape)), wd = val (mp->po (k, P::oWidth)) / 100.0, fo = val (mp->po (k, P::oFold)) / 100.0;
            drawCycle (g, in, shp, wd, fo, c, on ? 1.0f : 0.35f);
            g.setColour (text (0.75f)); g.setFont (Theme::font (8.0f, true));
            g.drawText (juce::String (SqMote::shapeName ((float) shp)).toUpperCase() + " " + juce::String (shp, 2) + (fo > 0.005 ? "  FOLD " + juce::String ((int) std::lround (fo * 100)) + "%" : juce::String()), in.reduced (6.0f, 4.0f), juce::Justification::topLeft);
            g.drawText ("LEVEL " + juce::String ((int) val (mp->po (k, P::oLevel))) + "%", in.reduced (6.0f, 4.0f), juce::Justification::bottomRight);
            if (k > 0 && val (mp->po (k, P::oFm)) > 0.5f) { g.setColour (oscCol (k - 1)); g.drawText ("FM " + juce::String ((int) val (mp->po (k, P::oFm))) + "% FROM " + juce::String (k), in.reduced (6.0f, 4.0f), juce::Justification::topRight); }
            const auto rt = P::routeNames()[juce::jlimit (0, 2, mp->oscRoute[(size_t) k].get())].toUpperCase().replace ("FILTER ", "F");
            const float cw = (chipsRow.getWidth() - 9.0f) / 4.0f;
            chip (g, chipsRow.removeFromLeft (cw), "TO " + rt, true, 1010 + k, gold(), 7.5f); chipsRow.removeFromLeft (3.0f);
            chip (g, chipsRow.removeFromLeft (cw), "UNI " + juce::String (mp->oscUnison[(size_t) k].get()), mp->oscUnison[(size_t) k].get() > 1, 1020 + k, c, 7.5f); chipsRow.removeFromLeft (3.0f);
            chip (g, chipsRow.removeFromLeft (cw), k == 0 ? juce::String ("MASTER") : juce::String ("SYNC"), k > 0 && mp->oscSync[(size_t) k].get(), k > 0 ? 1030 + k : -1, c, 7.5f); chipsRow.removeFromLeft (3.0f);
            chip (g, chipsRow, mp->oscFree[(size_t) k].get() ? "FREE PH" : "RESET PH", mp->oscFree[(size_t) k].get(), 1040 + k, c, 7.5f);
        }

        // ---- filters
        static float xOf (juce::Rectangle<float> p, double hz) { return p.getX() + (float) (std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0)) * p.getWidth(); }
        static double hzOf (juce::Rectangle<float> p, float x) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
        static float yOfDb (juce::Rectangle<float> p, double dbv) { return p.getY() + (float) juce::jlimit (0.0, 1.0, (30.0 - dbv) / 78.0) * p.getHeight(); }
        SqFissure::Slot fslot (int f) const
        {
            SqFissure::Slot s; s.on = mp->fOn[(size_t) f].get(); s.type = mp->fType[(size_t) f].get(); s.slope = mp->fSlope[(size_t) f].get();
            s.cutoffHz = val (mp->pf (f, P::fCut)); s.resonance = val (mp->pf (f, P::fRes)) / 100.0; s.gainDb = val (mp->pf (f, P::fGain)); return s;
        }
        juce::Point<float> nodeOf (int f) const
        {
            const auto p = fPlot(); const auto s = fslot (f);
            return { xOf (p, s.cutoffHz), s.type == SqFissure::peak ? yOfDb (p, s.gainDb) : yOfDb (p, SqFissure::Engine::responseDb (s, 48000.0, s.cutoffHz)) };
        }
        static juce::Colour fCol (int f) { return f == 0 ? juce::Colour (0xff5ee7ff) : juce::Colour (0xfff2c46b); }
        void paintFilters (juce::Graphics& g)
        {
            const auto r = fltPane(); pane (g, r, "FILTERS", gold());
            auto row = r.reduced (12.0f, 0).withTrimmedTop (26.0f).removeFromTop (20.0f);
            for (int f = 0; f < 2; ++f) { chip (g, row.removeFromLeft (38.0f), "F" + juce::String (f + 1), f == selF, 1300 + f, fCol (f), 8.5f); row.removeFromLeft (3.0f); }
            chip (g, row.removeFromLeft (40.0f), mp->fOn[(size_t) selF].get() ? "ON" : "OFF", mp->fOn[(size_t) selF].get(), 1310, fCol (selF), 8.0f); row.removeFromLeft (6.0f);
            const auto tn = juce::String (SqFissure::typeName (mp->fType[(size_t) selF].get())).toUpperCase();
            chip (g, row.removeFromLeft (widthOf (tn, 8.0f) + 18.0f), tn + "  " + juce::String::charToString (0x25be), true, 1311, fCol (selF), 8.0f); row.removeFromLeft (6.0f);
            for (int sl : { 12, 24, 48 }) { chip (g, row.removeFromLeft (40.0f), juce::String (sl) + " dB", mp->fSlope[(size_t) selF].get() == sl, 1320 + sl, fCol (selF), 7.5f); row.removeFromLeft (3.0f); }
            const auto p = fPlot();
            g.setFont (Theme::font (8.0f, true));
            for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            { const float x = xOf (p, hz); g.setColour (text (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom()); g.setColour (text (0.34f)); g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), (int) x - 14, (int) p.getBottom() + 4, 28, 12, juce::Justification::centred); }
            g.drawText ("Hz", (int) p.getRight() + 2, (int) p.getBottom() + 4, 22, 12, juce::Justification::left);
            for (int d = 24; d >= -48; d -= 12)
            { const float y = yOfDb (p, d); g.setColour (text (d == 0 ? 0.15f : 0.05f)); g.drawHorizontalLine ((int) y, p.getX(), p.getRight()); g.setColour (text (0.34f)); g.drawText ((d > 0 ? "+" : "") + juce::String (d), (int) p.getRight() + 2, (int) y - 6, 22, 12, juce::Justification::left); }
            for (int f = 0; f < 2; ++f)
            {
                if (! mp->fOn[(size_t) f].get()) continue;
                const auto s = fslot (f); juce::Path c;
                for (int i = 0; i <= 180; ++i) { const float x = p.getX() + p.getWidth() * i / 180.0f, y = yOfDb (p, SqFissure::Engine::responseDb (s, 48000.0, hzOf (p, x))); if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y); }
                g.setColour (fCol (f).withAlpha (f == selF ? 0.9f : 0.45f)); g.strokePath (c, juce::PathStrokeType (f == selF ? 1.8f : 1.1f));
                if (vNote >= 0) { const float xn = xOf (p, cutNow[(size_t) f]); g.setColour (fCol (f).withAlpha (0.8f)); g.drawVerticalLine ((int) xn, p.getY(), p.getBottom()); }
            }
            {   // combined (for a signal into filter 1)
                juce::Path c;
                for (int i = 0; i <= 200; ++i)
                {
                    const float x = p.getX() + p.getWidth() * i / 200.0f; const double hz = hzOf (p, x);
                    const double a = mp->fOn[0].get() ? SqFissure::Engine::responseDb (fslot (0), 48000.0, hz) : 0.0, b = mp->fOn[1].get() ? SqFissure::Engine::responseDb (fslot (1), 48000.0, hz) : 0.0;
                    const double m = mp->parallel.get() ? a : a + b;
                    if (i == 0) c.startNewSubPath (x, yOfDb (p, m)); else c.lineTo (x, yOfDb (p, m));
                }
                g.setColour (text (0.18f)); g.strokePath (c, juce::PathStrokeType (5.0f)); g.setColour (text (0.92f)); g.strokePath (c, juce::PathStrokeType (1.3f));
                g.setFont (Theme::font (7.5f, true)); g.setColour (text (0.55f));
                g.drawText (mp->parallel.get() ? "WHITE: OSCS ROUTED TO F1 (PARALLEL)" : "WHITE: F1 THEN F2 (SERIES)", p.reduced (4.0f).removeFromBottom (10.0f), juce::Justification::right);
            }
            for (int f = 0; f < 2; ++f)
            {
                if (! mp->fOn[(size_t) f].get()) continue;
                const auto n = nodeOf (f); const bool hot = f == hoverNode || f == dragNode;
                g.setColour (fCol (f).withAlpha (hot ? 0.42f : 0.2f)); g.fillEllipse (n.x - 13, n.y - 13, 26, 26);
                g.setColour (night()); g.fillEllipse (n.x - 8, n.y - 8, 16, 16); g.setColour (fCol (f)); g.drawEllipse (n.x - 8, n.y - 8, 16, 16, f == selF ? 2.2f : 1.4f);
                g.setColour (text (1.0f)); g.setFont (Theme::font (8.5f, true)); g.drawText (juce::String (f + 1), juce::Rectangle<float> (n.x - 8, n.y - 8, 16, 16), juce::Justification::centred);
                if (hot || f == selF)
                {
                    const auto s = fslot (f); auto tag = juce::Rectangle<float> (n.x - 58, n.y - 32, 116, 17).constrainedWithin (p.expanded (16.0f));
                    g.setColour (night().withAlpha (0.9f)); g.fillRoundedRectangle (tag, 8.5f); g.setColour (fCol (f).withAlpha (0.6f)); g.drawRoundedRectangle (tag, 8.5f, 1.0f);
                    g.setColour (text (0.95f)); g.setFont (Theme::font (8.5f, true));
                    g.drawText ((s.cutoffHz >= 1000 ? juce::String (s.cutoffHz / 1000.0, 2) + " kHz" : juce::String ((int) std::lround (s.cutoffHz)) + " Hz") + (s.type == SqFissure::peak ? "  " + juce::String (s.gainDb, 1) + " dB" : "  Q " + juce::String ((int) std::lround (s.resonance * 100)) + "%"), tag, juce::Justification::centred);
                }
            }
        }

        // ---- envelopes
        void paintEnv (juce::Graphics& g)
        {
            const auto r = envPane(); pane (g, r, "ENVELOPES", violet());
            auto tabs = r.reduced (12.0f, 0).withTrimmedTop (26.0f).removeFromTop (20.0f); const float tw = (tabs.getWidth() - 9.0f) / 4.0f;
            for (int e = 0; e < 4; ++e) { chip (g, tabs.removeFromLeft (tw), e == 0 ? juce::String ("1 AMP") : juce::String (e + 1), e == selEnv, 1400 + e, violet(), 8.0f); tabs.removeFromLeft (3.0f); }
            auto in = r.reduced (12.0f, 10.0f).withTrimmedTop (44.0f);
            auto foot = in.removeFromBottom (30.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (in, 6.0f);
            auto d = in.reduced (8.0f, 10.0f);
            const float De = val (mp->pe (selEnv, P::eDelay)), A = val (mp->pe (selEnv, P::eAttack)), H = val (mp->pe (selEnv, P::eHold)), Dc = val (mp->pe (selEnv, P::eDecay)), S = val (mp->pe (selEnv, P::eSustain)) / 100.0f, R = val (mp->pe (selEnv, P::eRelease));
            const float wDe = std::sqrt (De), wA = std::sqrt (A), wH = std::sqrt (H), wD = std::sqrt (Dc), wR = std::sqrt (R), wS = (wA + wD + wR) * 0.3f + 1.0f, tot = wDe + wA + wH + wD + wS + wR;
            float x = d.getX(); auto adv = [&] (float w) { x += d.getWidth() * w / tot; return x; };
            auto yL = [&] (float lv) { return d.getBottom() - lv * d.getHeight(); };
            juce::Path pth; pth.startNewSubPath (d.getX(), yL (0));
            const float xDe = adv (wDe); pth.lineTo (xDe, yL (0)); const float xA = adv (wA); pth.lineTo (xA, yL (1)); const float xH = adv (wH); pth.lineTo (xH, yL (1));
            const float x0 = xH, xD = adv (wD); for (int i = 1; i <= 20; ++i) { const float t = i / 20.0f; pth.lineTo (x0 + (xD - x0) * t, yL (S + (1 - S) * std::pow (0.01f, t))); }
            const float xS = adv (wS); pth.lineTo (xS, yL (S)); for (int i = 1; i <= 20; ++i) { const float t = i / 20.0f; pth.lineTo (xS + (d.getRight() - xS) * t, yL (S * std::pow (0.01f, t))); }
            juce::Path fill (pth); fill.lineTo (d.getRight(), yL (0)); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (violet().withAlpha (0.25f), 0, d.getY(), violet().withAlpha (0.0f), 0, d.getBottom(), false)); g.fillPath (fill);
            g.setColour (violet()); g.strokePath (pth, juce::PathStrokeType (1.6f));
            for (float xx : { xDe, xA, xH, xD, xS }) { g.setColour (text (0.1f)); g.drawVerticalLine ((int) xx, d.getY(), d.getBottom()); }
            if (vNote >= 0) { const float y = yL (envNow[(size_t) selEnv]); g.setColour (violet().withAlpha (0.4f)); g.fillEllipse (d.getRight() - 12, y - 6, 12, 12); g.setColour (juce::Colours::white); g.fillEllipse (d.getRight() - 9, y - 3, 6, 6); }
            auto ms = [] (float v) { return v >= 1000 ? juce::String (v / 1000.0f, 2) + "s" : juce::String (v, v < 10 ? 1 : 0) + "ms"; };
            g.setColour (text (0.75f)); g.setFont (Theme::font (7.5f, true));
            auto l1 = foot.removeFromTop (14.0f), l2 = foot;
            g.drawText ("DLY " + ms (De) + "   ATK " + ms (A) + "   HLD " + ms (H), l1, juce::Justification::left);
            g.drawText ("DEC " + ms (Dc) + "   SUS " + juce::String ((int) std::lround (S * 100)) + "%   REL " + ms (R), l2, juce::Justification::left);
        }

        // ---- LFOs
        void paintLfo (juce::Graphics& g)
        {
            const auto r = lfoPane(); pane (g, r, "LFOs", cyan());
            auto tabs = r.reduced (12.0f, 0).withTrimmedTop (26.0f).removeFromTop (20.0f); const float tw = (tabs.getWidth() - 9.0f) / 4.0f;
            for (int l = 0; l < 4; ++l) { chip (g, tabs.removeFromLeft (tw), juce::String (l + 1), l == selLfo, 1500 + l, cyan(), 8.0f); tabs.removeFromLeft (3.0f); }
            auto in = r.reduced (12.0f, 10.0f).withTrimmedTop (44.0f);
            auto chipsR = in.removeFromBottom (44.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (in, 6.0f);
            auto d = in.reduced (8.0f, 10.0f); const int shp = mp->lfoShape[(size_t) selLfo].get();
            g.setColour (text (0.08f)); g.drawHorizontalLine ((int) d.getCentreY(), d.getX(), d.getRight());
            juce::Path c; juce::Random rnd (11); float held = rnd.nextFloat() * 2 - 1, nextR = rnd.nextFloat() * 2 - 1;
            for (int i = 0; i <= 120; ++i)
            {
                const double ph = i / 120.0; double v;
                switch (shp)
                {
                    case SqMirror::lTriangle: v = 1.0 - 4.0 * std::abs (ph - 0.5); break;
                    case SqMirror::lSaw: v = 2.0 * ph - 1.0; break;
                    case SqMirror::lSquare: v = ph < 0.5 ? 1.0 : -1.0; break;
                    case SqMirror::lSampleHold: if (i % 30 == 0) held = rnd.nextFloat() * 2 - 1; v = held; break;
                    case SqMirror::lSmooth: { if (i % 40 == 0) { held = nextR; nextR = rnd.nextFloat() * 2 - 1; } const double t = (i % 40) / 40.0; v = held + (nextR - held) * (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * t)); break; }
                    default: v = std::sin (juce::MathConstants<double>::twoPi * ph);
                }
                const float x = d.getX() + d.getWidth() * (float) ph, y = d.getCentreY() - (float) v * d.getHeight() * 0.45f;
                if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
            }
            g.setColour (cyan().withAlpha (0.3f)); g.strokePath (c, juce::PathStrokeType (5.0f)); g.setColour (cyan()); g.strokePath (c, juce::PathStrokeType (1.5f));
            const float sx = d.getX() + d.getWidth() * val (mp->pl (selLfo, P::lPhase)) / 360.0f; g.setColour (text (0.4f)); g.drawVerticalLine ((int) sx, d.getY(), d.getBottom());
            if (vNote >= 0) { const float y = d.getCentreY() - lfoNow[(size_t) selLfo] * d.getHeight() * 0.45f; g.setColour (juce::Colours::white); g.fillEllipse (d.getRight() - 9, y - 3, 6, 6); }
            g.setColour (text (0.75f)); g.setFont (Theme::font (7.5f, true));
            g.drawText (juce::String (mp->lfoHz (selLfo), mp->lfoHz (selLfo) < 1 ? 2 : 1) + " Hz", in.reduced (6.0f, 3.0f), juce::Justification::topRight);
            auto r1 = chipsR.removeFromTop (20.0f); chipsR.removeFromTop (4.0f);
            const auto sn = SqMirror::lfoShapeNames()[shp].toUpperCase();
            chip (g, r1.removeFromLeft (r1.getWidth() * 0.5f - 2.0f), sn, true, 1510, cyan(), 7.5f); r1.removeFromLeft (4.0f);
            const int sy = mp->lfoSync[(size_t) selLfo].get();
            chip (g, r1, sy > 0 ? P::syncNames()[sy].toUpperCase() : juce::String ("FREE HZ"), sy > 0, 1511, cyan(), 7.5f);
            chip (g, chipsR, mp->lfoRetrig[(size_t) selLfo].get() ? "RESTARTS EACH NOTE" : "FREE RUNNING", mp->lfoRetrig[(size_t) selLfo].get(), 1512, cyan(), 7.5f);
        }

        // ---- matrix
        std::array<juce::Rectangle<float>, P::numSlots> amtR {};
        void paintMatrix (juce::Graphics& g)
        {
            const auto r = matPane(); pane (g, r, "MATRIX  (16)", rose());
            auto in = r.reduced (10.0f, 8.0f).withTrimmedTop (20.0f);
            const float rowH = in.getHeight() / P::numSlots; const auto sn = SqMirror::sourceNames();
            for (int m = 0; m < P::numSlots; ++m)
            {
                auto row = juce::Rectangle<float> (in.getX(), in.getY() + m * rowH, in.getWidth(), rowH - 2.0f);
                const int s = mp->modSrc[(size_t) m].get(), t = mp->modDst[(size_t) m].get();
                g.setColour (text (0.35f)); g.setFont (Theme::font (7.0f, true)); g.drawText (juce::String (m + 1), row.removeFromLeft (14.0f), juce::Justification::centredLeft);
                auto sR = row.removeFromLeft (row.getWidth() * 0.26f); row.removeFromLeft (2.0f); auto dR = row.removeFromLeft (row.getWidth() * 0.5f); row.removeFromLeft (2.0f); amtR[(size_t) m] = row;
                chip (g, sR, sn[juce::jlimit (0, sn.size() - 1, s)].toUpperCase(), s != 0, 1600 + m, srcCol (s), 7.0f);
                chip (g, dR, SqMirror::targetName (t).toUpperCase(), t != 0, 1700 + m, rose(), 7.0f);
                const float amt = val (P::modBase + m);
                g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (row, 4.0f);
                const float cx = row.getCentreX(), x2 = cx + amt * row.getWidth() * 0.5f;
                g.setColour (srcCol (s).withAlpha (s && t ? 0.85f : 0.25f)); g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, x2), row.getY() + 3, std::max (cx, x2), row.getBottom() - 3), 2.0f);
                g.setColour (text (0.25f)); g.drawVerticalLine ((int) cx, row.getY() + 2, row.getBottom() - 2);
                const double sc = SqMirror::targetScale (t); const juce::String u (SqMirror::targetUnit (t));
                g.setColour (text (0.9f)); g.setFont (Theme::font (7.0f, true));
                g.drawText (u.isEmpty() ? (amt >= 0 ? "+" : "") + juce::String ((int) std::lround (amt * 100)) + "%" : (amt >= 0 ? "+" : "") + juce::String (amt * sc, 1) + " " + u, row, juce::Justification::centred);
            }
        }

        void menu (const juce::StringArray& names, int cur, std::function<void (int)> set)
        {
            juce::PopupMenu m; for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur);
            m.showMenuAsync (juce::PopupMenu::Options(), [set, sp = SafePointer<MirrorwingViz> (this)] (int r) { if (r > 0 && sp != nullptr) { set (r - 1); sp->changed(); } });
        }
        void hitAction (int id)
        {
            um()->beginNewTransaction ("Mirrorwing setting"); auto& m = *mp;
            if (id >= 10 && id < 13) { m.polyModeValue().setValue (id - 10, um()); changed(); return; }
            if (id >= 20 && id < 23) { m.glideModeValue().setValue (id - 20, um()); changed(); return; }
            if (id == 31) { m.qualityValue().setValue (1 - m.qualityValue().get(), um()); changed(); return; }
            if (id == 32) { auto& b = m.bendRangeValue(); const int v[] = { 2, 7, 12, 24 }; int i = 0; while (i < 3 && v[i] <= b.get()) ++i; b.setValue (b.get() >= 24 ? 2 : v[i], um()); changed(); return; }
            if (id == 33) { auto& x = m.maxPolyValue(); x.setValue (x.get() >= 16 ? 1 : x.get() * 2, um()); changed(); return; }
            if (id == 34 || id == 35) { m.parallel.setValue (id == 35, um()); changed(); return; }
            if (id == 36) { menu (P::delayNames(), m.delayTime.get(), [this] (int i) { mp->delayTime.setValue (i, um()); }); return; }
            if (id >= 1000 && id < 1004) { const int k = id - 1000; m.oscOn[(size_t) k].setValue (! m.oscOn[(size_t) k].get(), um()); selOsc = k; changed(); return; }
            if (id >= 1010 && id < 1014) { const int k = id - 1010; m.oscRoute[(size_t) k].setValue ((m.oscRoute[(size_t) k].get() + 1) % 3, um()); changed(); return; }
            if (id >= 1020 && id < 1024) { const int k = id - 1020; auto& u = m.oscUnison[(size_t) k]; u.setValue (u.get() % 8 + 1, um()); changed(); return; }
            if (id >= 1031 && id < 1034) { const int k = id - 1030; m.oscSync[(size_t) k].setValue (! m.oscSync[(size_t) k].get(), um()); changed(); return; }
            if (id >= 1040 && id < 1044) { const int k = id - 1040; m.oscFree[(size_t) k].setValue (! m.oscFree[(size_t) k].get(), um()); changed(); return; }
            if (id >= 1200 && id < 1204) { selOsc = id - 1200; changed(); return; }
            if (id == 1300 || id == 1301) { selF = id - 1300; changed(); return; }
            if (id == 1310) { m.fOn[(size_t) selF].setValue (! m.fOn[(size_t) selF].get(), um()); changed(); return; }
            if (id == 1311) { juce::StringArray n; for (int t = 0; t < SqFissure::numTypes; ++t) n.add (SqFissure::typeName (t)); menu (n, m.fType[(size_t) selF].get(), [this] (int i) { mp->fType[(size_t) selF].setValue (i, um()); }); return; }
            if (id == 1332 || id == 1344 || id == 1368) { m.fSlope[(size_t) selF].setValue (id - 1320, um()); changed(); return; }
            if (id >= 1400 && id < 1404) { selEnv = id - 1400; changed(); return; }
            if (id >= 1500 && id < 1504) { selLfo = id - 1500; changed(); return; }
            if (id == 1510) { auto& s = m.lfoShape[(size_t) selLfo]; s.setValue ((s.get() + 1) % SqMirror::numLfoShapes, um()); changed(); return; }
            if (id == 1511) { juce::StringArray n = P::syncNames(); n.set (0, "Free (the Rate knob)"); menu (n, m.lfoSync[(size_t) selLfo].get(), [this] (int i) { mp->lfoSync[(size_t) selLfo].setValue (i, um()); }); return; }
            if (id == 1512) { m.lfoRetrig[(size_t) selLfo].setValue (! m.lfoRetrig[(size_t) selLfo].get(), um()); changed(); return; }
            if (id >= 1600 && id < 1616) { const int s = id - 1600; menu (SqMirror::sourceNames(), m.modSrc[(size_t) s].get(), [this, s] (int i) { mp->modSrc[(size_t) s].setValue (i, um()); }); return; }
            if (id >= 1700 && id < 1716)
            {
                const int s = id - 1700; juce::StringArray n; for (int t = 0; t < SqMirror::numTargets; ++t) n.add (SqMirror::targetName (t));
                menu (n, m.modDst[(size_t) s].get(), [this, s] (int i) { mp->modDst[(size_t) s].setValue (i, um()); });
            }
        }

        // ---- the deck: chrome-and-prism knobs
        struct KnobDef { int param; juce::String label; int group; bool bipolar; };
        std::vector<KnobDef> defs() const
        {
            const int k = selOsc, f = selF, e = selEnv, l = selLfo;
            std::vector<KnobDef> d {
                { mp->po (k, P::oShape), "SHAPE", 0, false }, { mp->po (k, P::oWidth), "WIDTH", 0, false }, { mp->po (k, P::oFold), "FOLD", 0, false },
                { mp->po (k, P::oOct), "OCTAVE", 0, true }, { mp->po (k, P::oSemi), "SEMI", 0, true }, { mp->po (k, P::oFine), "FINE", 0, true },
                { mp->po (k, P::oLevel), "LEVEL", 0, false }, { mp->po (k, P::oPan), "PAN", 0, true }, { mp->po (k, P::oUniDetune), "DETUNE", 0, false },
                { mp->po (k, P::oUniSpread), "SPREAD", 0, false }, { mp->po (k, P::oFm), k > 0 ? "FM FROM " + juce::String (k) : juce::String ("FM"), 0, false }, { mp->po (k, P::oPhase), "PHASE", 0, false },
                { P::noise, "NOISE", 1, false }, { P::noiseColour, "COLOUR", 1, false },
                { mp->pf (f, P::fCut), "CUTOFF", 2, false }, { mp->pf (f, P::fRes), "RESONANCE", 2, false }, { mp->pf (f, P::fDrive), "DRIVE", 2, false }, { mp->pf (f, P::fGain), "GAIN", 2, true }, { mp->pf (f, P::fKey), "KEY", 2, false },
                { mp->pe (e, P::eDelay), "DELAY", 3, false }, { mp->pe (e, P::eAttack), "ATTACK", 3, false }, { mp->pe (e, P::eHold), "HOLD", 3, false },
                { mp->pe (e, P::eDecay), "DECAY", 3, false }, { mp->pe (e, P::eSustain), "SUSTAIN", 3, false }, { mp->pe (e, P::eRelease), "RELEASE", 3, false },
                { mp->pl (l, P::lRate), "RATE", 4, false }, { mp->pl (l, P::lPhase), "PHASE", 4, false }, { mp->pl (l, P::lFade), "FADE IN", 4, false },
                { P::macroBase, "MACRO 1", 5, false }, { P::macroBase + 1, "MACRO 2", 5, false }, { P::macroBase + 2, "MACRO 3", 5, false }, { P::macroBase + 3, "MACRO 4", 5, false },
                { P::fxChorus, "CHORUS", 6, false }, { P::fxDelay, "DELAY", 6, false }, { P::fxDelayFb, "FEEDBACK", 6, false }, { P::fxReverb, "REVERB", 6, false }, { P::fxSize, "SIZE", 6, false }, { P::width, "WIDTH", 6, false },
                { P::glide, "GLIDE", 7, false }, { P::ampVel, "VELOCITY", 7, false }, { P::level, "LEVEL", 7, true } };
            return d;
        }
        static int rowOf (int grp) { return grp <= 1 ? 0 : grp <= 4 ? 1 : 2; }
        juce::String groupTitle (int grp) const
        {
            switch (grp)
            {
                case 0: return "OSC " + juce::String (selOsc + 1) + (selOsc > 0 ? "" : "  (CLICK A CARD TO SWITCH)");
                case 1: return "NOISE";
                case 2: return "FILTER " + juce::String (selF + 1) + "  -  " + juce::String (SqFissure::typeName (mp->fType[(size_t) selF].get())).toUpperCase();
                case 3: return "ENVELOPE " + juce::String (selEnv + 1) + (selEnv == 0 ? "  (AMP)" : "");
                case 4: return "LFO " + juce::String (selLfo + 1);
                case 5: return "MACROS (SOURCES IN THE MATRIX)";
                case 6: return "EFFECTS";
                default: return "VOICE + OUTPUT";
            }
        }
        juce::Colour groupCol (int grp) const { switch (grp) { case 0: return oscCol (selOsc); case 1: return text (0.9f); case 2: return fCol (selF); case 3: return violet(); case 4: return cyan(); case 5: return rose(); case 6: return gold(); default: return text (0.9f); } }
        bool enabled (const KnobDef& d) const
        {
            if (d.param == mp->po (selOsc, P::oWidth)) return val (mp->po (selOsc, P::oShape)) > 2.0f;
            if (d.param == mp->po (selOsc, P::oUniDetune) || d.param == mp->po (selOsc, P::oUniSpread)) return mp->oscUnison[(size_t) selOsc].get() > 1;
            if (d.param == mp->po (selOsc, P::oFm)) return selOsc > 0;
            if (d.param == mp->po (selOsc, P::oPhase)) return ! mp->oscFree[(size_t) selOsc].get();
            if (d.param == mp->pf (selF, P::fGain)) return mp->fType[(size_t) selF].get() == SqFissure::peak;
            if (d.param == mp->pl (selLfo, P::lRate)) return mp->lfoSync[(size_t) selLfo].get() == 0;
            if (d.param == P::glide) return mp->glideModeValue().get() != 0;
            if (d.param == P::noiseColour) return val (P::noise) > 0.5f;
            if (d.param == P::fxDelayFb) return val (P::fxDelay) > 0.5f;
            if (d.param == P::fxSize) return val (P::fxReverb) > 0.5f;
            return true;
        }
        std::vector<juce::Rectangle<float>> knobR;
        void layoutKnobs()
        {
            const auto ds = defs(); knobR.assign (ds.size(), {});
            auto d = deck().reduced (12.0f, 8.0f); const float rowH = (d.getHeight() - 16.0f) / 3.0f;
            for (int row = 0; row < 3; ++row)
            {
                auto rr = juce::Rectangle<float> (d.getX(), d.getY() + row * (rowH + 8.0f), d.getWidth(), rowH).withTrimmedTop (14.0f);
                std::vector<int> idx; int groups = 0, lastG = -1;
                for (int i = 0; i < (int) ds.size(); ++i) if (rowOf (ds[(size_t) i].group) == row) { idx.push_back (i); if (ds[(size_t) i].group != lastG) { ++groups; lastG = ds[(size_t) i].group; } }
                const float w = std::min (130.0f, (rr.getWidth() - (groups - 1) * 12.0f) / (float) idx.size()); float x = rr.getX(); lastG = -1;
                for (int i : idx) { const int g = ds[(size_t) i].group; if (lastG >= 0 && g != lastG) x += 12.0f; lastG = g; knobR[(size_t) i] = { x, rr.getY(), w, rr.getHeight() }; x += w; }
            }
        }
        juce::String knobText (const KnobDef& d) const
        {
            const auto& s = spec (d.param); const float v = val (d.param); const juce::String u (s.unit);
            if (d.param == mp->po (selOsc, P::oShape)) return juce::String (SqMote::shapeName (v)).substring (0, 3).toUpperCase() + " " + juce::String (v, 2);
            if (d.param == mp->pl (selLfo, P::lRate) && mp->lfoSync[(size_t) selLfo].get() > 0) return P::syncNames()[mp->lfoSync[(size_t) selLfo].get()];
            if (d.param == mp->po (selOsc, P::oOct)) return (v > 0 ? "+" : "") + juce::String ((int) std::lround (v));
            if (u.contains ("Hz")) return v >= 1000 ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 10 ? 2 : 0) + " Hz";
            if (u.contains ("ms")) return v >= 1000 ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms";
            if (u.contains ("deg")) return juce::String ((int) std::lround (v)) + juce::String::charToString (0x00b0);
            if (u.contains ("st")) return (v > 0 ? "+" : "") + juce::String ((int) std::lround (v)) + " st";
            if (u.contains ("ct")) return (v > 0 && s.lo < 0 ? "+" : "") + juce::String (v, s.lo < 0 ? 0 : 1) + " ct";
            if (u.contains ("dB")) return (v > 0 && s.lo < 0 ? "+" : "") + juce::String (v, 1) + " dB";
            if (u.contains ("%")) { if (d.param == mp->po (selOsc, P::oPan)) return std::abs (v) < 0.5f ? juce::String ("C") : (v < 0 ? "L " : "R ") + juce::String ((int) std::lround (std::abs (v))); return juce::String ((int) std::lround (v)) + " %"; }
            return juce::String (v, 2);
        }
        /** The routings landing on a knob: (normalised end point, source colour). */
        std::vector<std::pair<float, juce::Colour>> ringsOf (int param) const
        {
            std::vector<std::pair<float, juce::Colour>> out; const int t = P::targetOfParam (param); if (t <= 0) return out;
            auto& p = param_ (param);
            for (int m = 0; m < P::numSlots; ++m)
            {
                if (mp->modDst[(size_t) m].get() != t || mp->modSrc[(size_t) m].get() == 0) continue;
                const double amt = val (P::modBase + m) * SqMirror::targetScale (t); const double v = p.getCurrentValue();
                double to;
                if (juce::String (SqMirror::targetUnit (t)) == "oct") to = v * std::pow (2.0, amt);
                else if (t >= SqMirror::tOscBase && t < SqMirror::tFilBase && (t - SqMirror::tOscBase) % SqMirror::perOscT == SqMirror::tPitch) to = v + amt;
                else if (t == SqMirror::tAmp) to = v + 20.0 * std::log10 (std::max (0.01, 1.0 + amt));   // the amp target multiplies the level
                else if (juce::String (spec (param).unit).contains ("%")) to = v + amt * 100.0;                 // fractions shown in percent
                else to = v + amt;                                                                          // shape, dB
                out.push_back ({ p.valueRange.convertTo0to1 ((float) juce::jlimit ((double) p.valueRange.start, (double) p.valueRange.end, to)), srcCol (mp->modSrc[(size_t) m].get()) });
            }
            return out;
        }
        te::AutomatableParameter& param_ (int i) const { return *bank().params[(size_t) i]; }
        void drawKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& label, const juce::String& value, float v, juce::Colour col, bool hotK, bool active, bool bipolar, bool en, const std::vector<std::pair<float, juce::Colour>>& rings) const
        {
            const float al = en ? 1.0f : 0.35f;
            auto lab = cell.removeFromTop (14.0f); auto read = cell.removeFromBottom (20.0f).withTrimmedTop (2.0f);
            const float rO = std::min (cell.getWidth() - 12.0f, cell.getHeight()) * 0.5f - 3.0f; const auto c = cell.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, av = a0 + (a1 - a0) * v;
            g.setColour (text ((hotK ? 0.95f : 0.58f) * al)); g.setFont (Theme::font (7.5f, true)); g.drawText (label, lab, juce::Justification::centred);
            const float ar = rO * 0.92f, as = bipolar ? (a0 + a1) * 0.5f : a0;
            juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.6f * al)); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (as, av), std::max (as, av), true);
            g.setColour (col.withAlpha (0.25f * al)); g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withAlpha (al)); g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // modulation rings: one thin outer arc per routing, from the value to where the source can take it
            float rr = rO * 1.1f;
            for (auto& [to, sc] : rings)
            {
                const float at = a0 + (a1 - a0) * to; juce::Path m; m.addCentredArc (c.x, c.y, rr, rr, 0, std::min (av, at), std::max (av, at), true);
                g.setColour (sc.withAlpha (0.9f * al)); g.strokePath (m, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.fillEllipse (c.x + rr * std::sin (at) - 2.0f, c.y - rr * std::cos (at) - 2.0f, 4.0f, 4.0f);
                rr += 3.2f;
            }
            // chrome body with a prism rim: a conic sweep violet -> cyan -> gold
            const float rb = rO * 0.7f;
            for (int s = 0; s < 36; ++s)
            {
                const float a = juce::MathConstants<float>::twoPi * s / 36.0f; const float t = (float) s / 36.0f;
                const auto pc = t < 0.33f ? violet().interpolatedWith (cyan(), t * 3) : t < 0.66f ? cyan().interpolatedWith (gold(), (t - 0.33f) * 3) : gold().interpolatedWith (violet(), (t - 0.66f) * 3);
                juce::Path wedge; wedge.addPieSegment (c.x - rb - 2, c.y - rb - 2, (rb + 2) * 2, (rb + 2) * 2, a, a + juce::MathConstants<float>::twoPi / 36.0f + 0.01f, 0.86f);
                g.setColour (pc.withAlpha (0.55f * al)); g.fillPath (wedge);
            }
            const float rc = rb * 0.86f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e6f5).withAlpha (al), c.x - rc * 0.7f, c.y - rc, juce::Colour (0xff1a1828).withAlpha (al), c.x + rc * 0.6f, c.y + rc, false)); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2640).withAlpha (al), c.x, c.y - rc * 0.6f, juce::Colour (0xff09080f).withAlpha (al), c.x, c.y + rc * 0.6f, false)); g.fillEllipse (c.x - rc * 0.74f, c.y - rc * 0.74f, rc * 1.48f, rc * 1.48f);
            // mirrored pointer: the line and its faint reflection across the vertical axis
            const juce::Point<float> p1 (c.x + rc * 0.15f * std::sin (av), c.y - rc * 0.15f * std::cos (av)), p2 (c.x + rc * 0.92f * std::sin (av), c.y - rc * 0.92f * std::cos (av));
            g.setColour (juce::Colours::white.withAlpha (0.12f * al)); g.drawLine (2 * c.x - p1.x, p1.y, 2 * c.x - p2.x, p2.y, 1.2f);
            g.setColour (col.withAlpha (0.45f * al)); g.drawLine (p1.x, p1.y, p2.x, p2.y, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (al)); g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.5f);
            if (hotK) { g.setColour (col.withAlpha (0.8f)); g.drawEllipse (c.x - rb - 3, c.y - rb - 3, rb * 2 + 6, rb * 2 + 6, 1.0f); }
            auto pill = read.withSizeKeepingCentre (std::min (read.getWidth() - 2.0f, 76.0f), 17.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pill, 8.5f);
            g.setColour (active ? col : text (0.1f)); g.drawRoundedRectangle (pill, 8.5f, 1.0f);
            g.setColour ((active ? col : text (0.95f)).withMultipliedAlpha (al)); g.setFont (Theme::font (8.5f, true)); g.drawText (value, pill, juce::Justification::centred);
        }
        void paintDeck (juce::Graphics& g)
        {
            auto d = deck();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120f1c).withAlpha (0.95f), 0, d.getY(), night().withAlpha (0.97f), 0, d.getBottom(), false)); g.fillRoundedRectangle (d, 14.0f);
            g.setColour (violet().withAlpha (0.22f)); g.drawRoundedRectangle (d, 14.0f, 1.0f);
            layoutKnobs();
            const auto ds = defs();
            for (int grp = 0; grp < 8; ++grp)
            {
                juce::Rectangle<float> u; for (size_t i = 0; i < ds.size(); ++i) if (ds[i].group == grp) u = u.isEmpty() ? knobR[i] : u.getUnion (knobR[i]);
                auto rr = u.withTrimmedTop (-14.0f).expanded (2.0f, 2.0f); const auto c = groupCol (grp);
                g.setColour (c.withAlpha (0.05f)); g.fillRoundedRectangle (rr, 10.0f); g.setColour (c.withAlpha (0.2f)); g.drawRoundedRectangle (rr, 10.0f, 1.0f);
                g.setColour (c.withAlpha (0.88f)); g.setFont (Theme::font (8.5f, true)); g.drawText (groupTitle (grp), rr.reduced (10.0f, 3.0f).removeFromTop (11.0f), juce::Justification::left);
            }
            for (size_t i = 0; i < ds.size(); ++i)
            {
                const auto& k = ds[i]; const bool en = enabled (k);
                drawKnob (g, knobR[i], k.label, knobText (k), norm (k.param), groupCol (k.group), (int) i == hoverKnob || (int) i == dragKnob, (int) i == dragKnob, k.bipolar, en, ringsOf (k.param));
                StudioUi::knobScale (g, knobR[i], spec (k.param), text (1.0f), 14.0f, 20.0f, en ? 1.0f : 0.4f);
            }
        }

        // ---- mouse
        int dragKnob = -1, hoverKnob = -1, dragNode = -1, hoverNode = -1, dragAmt = -1, dragSrc = 0; float dragStartNorm = 0; juce::Point<float> dragStart, dragPos;
        int knobAt (juce::Point<float> p) const { const auto ds = defs(); for (size_t i = 0; i < knobR.size() && i < ds.size(); ++i) if (knobR[i].reduced (3.0f, 0).contains (p) && enabled (ds[i])) return (int) i; return -1; }
        int nodeAt (juce::Point<float> p) const { for (int f = 1; f >= 0; --f) if (mp->fOn[(size_t) f].get() && p.getDistanceFrom (nodeOf (f)) < 12.0f) return f; return -1; }
        int amtAt (juce::Point<float> p) const { for (int m = 0; m < P::numSlots; ++m) if (amtR[(size_t) m].contains (p)) return m; return -1; }
        int srcAt (juce::Point<float> p) const { for (int s = 1; s < SqMirror::numSources; ++s) if (srcR[(size_t) s].contains (p)) return s; return 0; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position), n = nodeAt (e.position);
            setMouseCursor (srcAt (e.position) ? juce::MouseCursor::DraggingHandCursor : k >= 0 ? juce::MouseCursor::UpDownResizeCursor : n >= 0 ? juce::MouseCursor::DraggingHandCursor : amtAt (e.position) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            if (k != hoverKnob || n != hoverNode) { hoverKnob = k; hoverNode = n; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverKnob = -1; hoverNode = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (mp == nullptr) return;
            if (const int s = srcAt (e.position)) { dragSrc = s; dragPos = e.position; repaint(); return; }
            for (auto& h : hits) if (h.r.contains (e.position)) { hitAction (h.id); return; }
            if (const int m = amtAt (e.position); m >= 0) { dragAmt = m; um()->beginNewTransaction ("Mirrorwing matrix amount"); param (P::modBase + m).parameterChangeGestureBegin(); mouseDrag (e); return; }
            if (const int n = nodeAt (e.position); n >= 0)
            {
                selF = n; dragNode = n; um()->beginNewTransaction ("Mirrorwing filter " + juce::String (n + 1));
                param (mp->pf (n, P::fCut)).parameterChangeGestureBegin(); param (mp->pf (n, mp->fType[(size_t) n].get() == SqFissure::peak ? P::fGain : P::fRes)).parameterChangeGestureBegin(); changed(); return;
            }
            for (int k = 0; k < 4; ++k) if (oscCard (k).contains (e.position)) { selOsc = k; changed(); return; }
            const int k = knobAt (e.position);
            if (k >= 0) { const auto d = defs()[(size_t) k]; dragKnob = k; dragStart = e.position; dragStartNorm = norm (d.param); um()->beginNewTransaction ("Mirrorwing " + d.label); param (d.param).parameterChangeGestureBegin(); repaint(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragSrc > 0) { dragPos = e.position; repaint(); return; }
            if (dragAmt >= 0) { const auto r = amtR[(size_t) dragAmt]; param (P::modBase + dragAmt).setParameter (juce::jlimit (-1.0f, 1.0f, (e.position.x - r.getCentreX()) / (r.getWidth() * 0.5f)), juce::sendNotification); changed(); return; }
            if (dragNode >= 0)
            {
                const auto p = fPlot(); setValue (mp->pf (dragNode, P::fCut), (float) hzOf (p, e.position.x));
                const double dbAt = 30.0 - 78.0 * juce::jlimit (0.0, 1.0, (double) ((e.position.y - p.getY()) / p.getHeight()));
                if (mp->fType[(size_t) dragNode].get() == SqFissure::peak) setValue (mp->pf (dragNode, P::fGain), (float) dbAt);
                else setValue (mp->pf (dragNode, P::fRes), (float) juce::jlimit (0.0, 100.0, (dbAt + 3.0) / 33.0 * 100.0));
                changed(); return;
            }
            if (dragKnob < 0) return;
            setNorm (defs()[(size_t) dragKnob].param, dragStartNorm + (dragStart.y - e.position.y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f));
        }
        /** Drag-and-drop: routes source s to the knob's target (reusing the slot if that pair already exists). */
        void route (int s, int param)
        {
            const int t = P::targetOfParam (param); if (t <= 0) return;
            um()->beginNewTransaction ("Mirrorwing route " + SqMirror::sourceNames()[s] + " to " + SqMirror::targetName (t));
            int slot = -1;
            for (int m = 0; m < P::numSlots && slot < 0; ++m) if (mp->modSrc[(size_t) m].get() == s && mp->modDst[(size_t) m].get() == t) slot = m;
            if (slot < 0) for (int m = 0; m < P::numSlots && slot < 0; ++m) if (mp->modSrc[(size_t) m].get() == 0 || mp->modDst[(size_t) m].get() == 0) slot = m;
            if (slot < 0) return;
            const bool fresh = ! (mp->modSrc[(size_t) slot].get() == s && mp->modDst[(size_t) slot].get() == t);
            mp->modSrc[(size_t) slot].setValue (s, um()); mp->modDst[(size_t) slot].setValue (t, um());
            if (fresh) param_ (P::modBase + slot).setParameter (0.25f, juce::sendNotification);
            changed();
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragSrc > 0) { if (const int k = knobAt (e.position); k >= 0) route (dragSrc, defs()[(size_t) k].param); dragSrc = 0; repaint(); return; }
            if (dragAmt >= 0) param (P::modBase + dragAmt).parameterChangeGestureEnd();
            if (dragNode >= 0) { param (mp->pf (dragNode, P::fCut)).parameterChangeGestureEnd(); param (mp->pf (dragNode, mp->fType[(size_t) dragNode].get() == SqFissure::peak ? P::fGain : P::fRes)).parameterChangeGestureEnd(); }
            if (dragKnob >= 0) param (defs()[(size_t) dragKnob].param).parameterChangeGestureEnd();
            dragKnob = -1; dragNode = -1; dragAmt = -1; repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            const int pi_ = defs()[(size_t) k].param; um()->beginNewTransaction ("Mirrorwing reset"); param (pi_).setParameter (spec (pi_).def, juce::sendNotification); changed();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = knobAt (e.position); if (k < 0) return;
            um()->beginNewTransaction ("Mirrorwing wheel"); const int pi_ = defs()[(size_t) k].param; setNorm (pi_, norm (pi_) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f));
        }
    };
