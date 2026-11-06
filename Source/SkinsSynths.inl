// Included inside namespace Skins (PluginSkins.h): faces for the 1.8 instrument shelf (SqSynths.h).
// Every instrument gets its own visual identity; they share the mod-matrix panel and the scope.

    /** Value of a bank parameter by ID (0 if the plug-in has no such parameter). */
    inline float bankValue (SqSynth::SynthCommon& s, const char* id, float fallback = 0.0f)
    {
        auto& b = s.synthBank();
        for (size_t i = 0; i < b.specs.size(); ++i) if (std::strcmp (b.specs[i].id, id) == 0) return b[i];
        return fallback;
    }
    inline juce::String bankLabel (SqSynth::SynthCommon& s, const juce::String& id)
    {
        auto& b = s.synthBank();
        for (auto& sp : b.specs) if (id == sp.id) return sp.name;
        return id;
    }

    //==========================================================================
    /** The 8-slot modulation matrix: click a source or destination to pick one, drag the bar for depth, double-click to clear. */
    class ModMatrixPanel : public juce::Component
    {
    public:
        ModMatrixPanel (SqSynth::SynthCommon& s, te::Plugin& p, juce::Colour acc) : synth (s), plugin (p), accent (acc) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
            g.drawText ("MOD MATRIX", r.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
            g.drawText ("source  >  destination  =  depth", r.toNearestInt().reduced (6, 3), juce::Justification::topRight);
            auto rows = r.reduced (6.0f, 4.0f).withTrimmedTop (14.0f);
            const float rh = rows.getHeight() / (float) SqSynth::numModSlots;
            auto names = SqSynth::modSourceNames();
            for (int i = 0; i < SqSynth::numModSlots; ++i)
            {
                auto row = juce::Rectangle<float> (rows.getX(), rows.getY() + rh * (float) i, rows.getWidth(), rh).reduced (0.0f, 1.0f);
                const int src = synth.modSourceValue (i).get();
                const auto dest = synth.modDestValue (i).get();
                const float depth = synth.modDepthValue (i).get();
                const bool live = src != 0 && dest.isNotEmpty();
                auto srcCell = row.removeFromLeft (row.getWidth() * 0.3f), dstCell = row.removeFromLeft (row.getWidth() * 0.45f), bar = row.reduced (4.0f, 3.0f);
                g.setColour (live ? accent.withAlpha (0.22f) : juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (srcCell.reduced (1.0f), 3.0f);
                g.fillRoundedRectangle (dstCell.reduced (1.0f), 3.0f);
                g.setFont (Theme::font (9.0f, live)); g.setColour (live ? Theme::text() : Theme::dim());
                g.drawFittedText (names[juce::jlimit (0, names.size() - 1, src)], srcCell.toNearestInt().reduced (4, 0), juce::Justification::centredLeft, 1);
                g.drawFittedText (dest.isEmpty() ? "-" : bankLabelFor (dest), dstCell.toNearestInt().reduced (4, 0), juce::Justification::centredLeft, 1);
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (bar, 2.0f);
                const float mid = bar.getCentreX(), half = bar.getWidth() * 0.5f;
                g.setColour (live ? accent : Theme::dim().withAlpha (0.5f));
                if (depth >= 0) g.fillRoundedRectangle (juce::Rectangle<float> (mid, bar.getY(), half * depth, bar.getHeight()), 2.0f);
                else            g.fillRoundedRectangle (juce::Rectangle<float> (mid + half * depth, bar.getY(), -half * depth, bar.getHeight()), 2.0f);
                g.setColour (Theme::text().withAlpha (0.5f)); g.drawVerticalLine ((int) mid, bar.getY(), bar.getBottom());
            }
        }

        juce::String bankLabelFor (const juce::String& id) const
        {
            static const std::map<juce::String, juce::String> friendly = { { "pitch", "Pitch" }, { "cutoff", "Cutoff" }, { "resonance", "Resonance" }, { "pw", "Pulse Width" }, { "amp", "Amp" }, { "pan", "Pan" }, { "fm", "FM" }, { "index", "FM Index" }, { "ratio", "Ratio" }, { "feedback", "Feedback" } };
            auto it = friendly.find (id); return it != friendly.end() ? it->second : bankLabel (synth, id);
        }

        int rowAt (juce::Point<float> p) const
        {
            auto rows = getLocalBounds().toFloat().reduced (6.0f, 4.0f).withTrimmedTop (14.0f);
            return juce::jlimit (0, SqSynth::numModSlots - 1, (int) ((p.y - rows.getY()) / (rows.getHeight() / (float) SqSynth::numModSlots)));
        }
        int colAt (juce::Point<float> p) const
        {
            auto rows = getLocalBounds().toFloat().reduced (6.0f, 4.0f);
            const float x = (p.x - rows.getX()) / rows.getWidth();
            return x < 0.3f ? 0 : x < 0.3f + 0.7f * 0.45f ? 1 : 2;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int row = rowAt (e.position), col = colAt (e.position);
            auto* um = &plugin.edit.getUndoManager();
            if (col == 0)
            {
                juce::PopupMenu m; auto names = SqSynth::modSourceNames();
                for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, synth.modSourceValue (row).get() == i);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, row, um] (int r) { if (r > 0) { synth.modSourceValue (row).setValue (r - 1, um); repaint(); } });
            }
            else if (col == 1)
            {
                juce::PopupMenu m; auto dests = synth.modDestinations();
                m.addItem (1, "-", true, synth.modDestValue (row).get().isEmpty());
                for (int i = 0; i < dests.size(); ++i) m.addItem (i + 2, bankLabelFor (dests[i]), true, synth.modDestValue (row).get() == dests[i]);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, row, um, dests] (int r) { if (r == 1) synth.modDestValue (row).setValue ({}, um); else if (r > 1) synth.modDestValue (row).setValue (dests[r - 2], um); repaint(); });
            }
            else { dragRow = row; dragStart = synth.modDepthValue (row).get(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragRow < 0) return;
            const float barW = getWidth() * 0.7f * 0.55f - 8.0f;
            synth.modDepthValue (dragRow).setValue (juce::jlimit (-1.0f, 1.0f, dragStart + (float) e.getDistanceFromDragStartX() / (barW * 0.5f)), &plugin.edit.getUndoManager());
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override   { dragRow = -1; }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int row = rowAt (e.position); auto* um = &plugin.edit.getUndoManager();
            if (colAt (e.position) == 2) synth.modDepthValue (row).setValue (0.0f, um);
            else { synth.modSourceValue (row).setValue (0, um); synth.modDestValue (row).setValue ({}, um); synth.modDepthValue (row).setValue (0.0f, um); }
            repaint();
        }
    private:
        SqSynth::SynthCommon& synth; te::Plugin& plugin; juce::Colour accent;
        int dragRow = -1; float dragStart = 0.0f;
    };

    //==========================================================================
    /** Shared drawing for the shelf: scope, envelopes, filter curves and oscillator waves. */
    struct ShelfViz : public Viz
    {
        ShelfViz (HandleMap& h, te::Plugin& p, juce::Colour c, bool withMatrix = true) : Viz (h, p, c)
        {
            synth = dynamic_cast<SqSynth::SynthCommon*> (&plugin);
            if (synth != nullptr && withMatrix) { matrix = std::make_unique<ModMatrixPanel> (*synth, plugin, accent); addAndMakeVisible (*matrix); }
            setInterceptsMouseClicks (false, true);
        }
        void resized() override
        {
            if (matrix) matrix->setBounds (getLocalBounds().reduced (6).removeFromRight (juce::jmin (330, getWidth() / 3)));
        }
        juce::Rectangle<float> bodyArea() const
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            if (matrix) r.removeFromRight ((float) matrix->getWidth() + 8.0f);
            return r;
        }
        float bv (const char* id, float fb = 0.0f) const   { return synth != nullptr ? bankValue (*synth, id, fb) : fb; }

        void box (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& t, float alpha = 0.32f)
        {
            g.setColour (juce::Colours::black.withAlpha (alpha)); g.fillRoundedRectangle (a, 4.0f);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
            g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
        }
        void scope (juce::Graphics& g, juce::Rectangle<float> in, juce::Colour c)
        {
            if (synth == nullptr) return;
            const int w = synth->scopeWritePos(); const auto& sc = synth->scopeData();
            juce::Path p;
            for (int i = 0; i < 512; ++i)
            {
                const float v = sc[(size_t) ((w + i) & 511)];
                const float x = in.getX() + in.getWidth() * (float) i / 511.0f, y = in.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * in.getHeight() * 0.48f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.5f));
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f));
            const int n = synth->lastNote();
            g.drawText (juce::String (synth->activeVoiceCount()) + " / " + juce::String (synth->maxPolyValue().get()) + " voices" + (n >= 0 && synth->displayLevel() > 0.01f ? "   " + juce::MidiMessage::getMidiNoteName (n, true, true, 4) : juce::String()),
                        in.toNearestInt(), juce::Justification::bottomRight);
        }
        void envelope (juce::Graphics& g, juce::Rectangle<float> in, float a, float d, float s, float rl, juce::Colour c)
        {
            auto norm = [] (float sec) { return juce::jlimit (0.03f, 1.0f, std::log (1.0f + sec * 4.0f) / std::log (9.0f)); };
            const float wa = norm (a), wd = norm (d), wr = norm (rl), total = wa + wd + 0.5f + wr;
            juce::Path p; p.startNewSubPath (in.getX(), in.getBottom());
            float x = in.getX();
            x += wa / total * in.getWidth(); p.lineTo (x, in.getY());
            x += wd / total * in.getWidth(); p.lineTo (x, in.getBottom() - s * in.getHeight());
            x += 0.5f / total * in.getWidth(); p.lineTo (x, in.getBottom() - s * in.getHeight());
            x += wr / total * in.getWidth(); p.lineTo (x, in.getBottom());
            g.setColour (c.withAlpha (0.18f)); juce::Path f (p); f.closeSubPath(); g.fillPath (f);
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.8f));
        }
        /** Approximate magnitude response of the voice filter for the display (dB). */
        static float filterDb (int type, float cut, float res, float hz)
        {
            const float w = hz / juce::jmax (20.0f, cut);
            const float q = 0.5f + res * 9.5f;
            const float lp2 = -10.0f * std::log10 (std::pow (1.0f - w * w, 2.0f) + std::pow (w / q, 2.0f));
            const float hp2 = lp2 + 40.0f * std::log10 (juce::jmax (1.0e-3f, w));
            switch (type)
            {
                case 0: case 6: return 2.0f * lp2 * 0.5f + (w < 1.0f ? 0.0f : -40.0f * std::log10 (w)) + (type == 6 ? res * 6.0f * std::exp (-std::pow (std::log (w), 2.0f) * 8.0f) : 0.0f);
                case 1: return lp2;
                case 2: return 2.0f * hp2 * 0.5f + (w > 1.0f ? 0.0f : 40.0f * std::log10 (juce::jmax (1.0e-3f, w)));
                case 3: return hp2;
                case 4: return -10.0f * std::log10 (1.0f + q * q * std::pow (w - 1.0f / w, 2.0f));
                default: return 10.0f * std::log10 (std::pow (w - 1.0f / w, 2.0f) / (std::pow (w - 1.0f / w, 2.0f) + 1.0f / (q * q)) + 1.0e-4f);
            }
        }
        void filterCurve (juce::Graphics& g, juce::Rectangle<float> in, int type, float cut, float res, juce::Colour c, float envDb = 0.0f)
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (in.expanded (1.0f, 6.0f).toNearestInt());
            juce::Path p;
            for (int i = 0; i <= (int) in.getWidth(); ++i)
            {
                const float hz = hzForX (in.getX() + (float) i, in);
                const float db = juce::jlimit (-36.0f, 18.0f, filterDb (type, cut * std::pow (2.0f, envDb), res, hz));
                const float y = in.getCentreY() - db / 36.0f * in.getHeight() * 0.9f;
                if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
            }
            g.setColour (c.withAlpha (0.15f)); juce::Path f (p); f.lineTo (in.getRight(), in.getBottom()); f.lineTo (in.getX(), in.getBottom()); f.closeSubPath(); g.fillPath (f);
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (2.0f));
        }
        static float waveSample (int w, float t, float pw)
        {
            const float ph = t - std::floor (t);
            switch (w) { case 0: return std::sin (ph * 6.2831853f); case 1: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; case 2: return 2.0f * ph - 1.0f; case 3: return ph < 0.5f ? 1.0f : -1.0f; case 4: return ph < pw ? 1.0f : -1.0f; default: return std::sin (ph * 977.0f) * std::sin (ph * 311.0f + 1.0f); }
        }
        void oscWave (juce::Graphics& g, juce::Rectangle<float> row, int w, float pw, float level, juce::Colour c, const juce::String& caption)
        {
            g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (row, 3.0f);
            g.setFont (Theme::font (8.5f, true)); g.setColour (c.withAlpha (0.9f));
            g.drawText (caption, row.toNearestInt().reduced (5, 0), juce::Justification::centredLeft);
            if (level < 0.001f) { g.setColour (Theme::dim()); g.drawText ("off", row.toNearestInt(), juce::Justification::centred); return; }
            auto in = row.withTrimmedLeft (46.0f).reduced (4.0f, 3.0f);
            juce::Path p;
            for (int i = 0; i <= (int) in.getWidth(); ++i)
            {
                const float t = (float) i / in.getWidth() * 2.0f + phase * 0.15f;
                const float y = in.getCentreY() - waveSample (w, t, pw) * level * in.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
            }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f));
        }
        SqSynth::SynthCommon* synth = nullptr;
        std::unique_ptr<ModMatrixPanel> matrix;
    };

    //==========================================================================
    // Mothsynth 2: three oscillator lanes, dual filter response, three envelopes, scope (deep violet)
    struct Mothsynth2Viz : public ShelfViz
    {
        using ShelfViz::ShelfViz;
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto oscs = r.removeFromLeft (r.getWidth() * 0.3f); r.removeFromLeft (8.0f);
            auto filt = r.removeFromLeft (r.getWidth() * 0.45f); r.removeFromLeft (8.0f);
            const juce::Colour cols[] = { Theme::violet(), Theme::magenta(), Theme::acid(), Theme::mint() };
            const float each = oscs.getHeight() / 4.0f;
            const char* ids[4][3] = { { "o1Wave", "o1Level", "o1Pw" }, { "o2Wave", "o2Level", "o2Pw" }, { "o3Wave", "o3Level", "o3Pw" }, { "subWave", "subLevel", nullptr } };
            for (int i = 0; i < 4; ++i)
            {
                auto row = juce::Rectangle<float> (oscs.getX(), oscs.getY() + each * (float) i, oscs.getWidth(), each).reduced (0.0f, 1.0f);
                oscWave (g, row, (int) std::lround (bv (ids[i][0])), ids[i][2] ? bv (ids[i][2], 0.5f) : 0.5f, bv (ids[i][1]), cols[i], i < 3 ? "OSC " + juce::String (i + 1) : "SUB");
            }
            box (g, filt, "FILTERS  " + SqSynth::filterTypeNames()[juce::jlimit (0, 6, (int) std::lround (bv ("f1Type")))] + (bv ("routing") > 0.5f ? (bv ("routing") > 1.5f ? "  ||  " : "  >  ") + SqSynth::filterTypeNames()[juce::jlimit (0, 6, (int) std::lround (bv ("f2Type")))] : juce::String()));
            {
                auto in = filt.reduced (8.0f, 16.0f);
                drawFreqGrid (g, in, 36.0f);
                filterCurve (g, in, (int) std::lround (bv ("f1Type")), bv ("f1Cut", 1000), bv ("f1Res"), accent);
                if (bv ("routing") > 0.5f) filterCurve (g, in, (int) std::lround (bv ("f2Type")), bv ("f2Cut", 100), bv ("f2Res"), Theme::mint());
            }
            auto envs = r.removeFromTop (r.getHeight() * 0.62f); r.removeFromTop (6.0f);
            const float ew = envs.getWidth() / 3.0f;
            const char* pre[3] = { "a", "f", "m" }; const char* titles[3] = { "AMP", "FILTER", "MOD" };
            for (int i = 0; i < 3; ++i)
            {
                auto a = juce::Rectangle<float> (envs.getX() + ew * (float) i, envs.getY(), ew, envs.getHeight()).reduced (2.0f, 0.0f);
                box (g, a, titles[i]);
                const juce::String p (pre[i]);
                envelope (g, a.reduced (6.0f, 14.0f), bv ((p + "A").toRawUTF8()), bv ((p + "D").toRawUTF8()), bv ((p + "S").toRawUTF8()), bv ((p + "R").toRawUTF8()), cols[i]);
            }
            box (g, r, "OUTPUT");
            scope (g, r.reduced (8.0f, 14.0f), Theme::acid());
        }
    };

    //==========================================================================
    // Venom: fang filter curve, trance-gate pattern strip, two LFO shapes, scope (acid green)
    struct VenomViz : public ShelfViz
    {
        using ShelfViz::ShelfViz;
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto left = r.removeFromLeft (r.getWidth() * 0.5f); r.removeFromLeft (8.0f);
            auto filt = left.removeFromTop (left.getHeight() * 0.62f); left.removeFromTop (6.0f);
            box (g, filt, "FILTER  " + SqSynth::filterTypeNames()[juce::jlimit (0, 6, (int) std::lround (bv ("f1Type")))] + "   env " + juce::String (juce::roundToInt (bv ("f1Env") * 100)) + "%");
            {
                auto in = filt.reduced (8.0f, 16.0f);
                drawFreqGrid (g, in, 36.0f);
                // ghost: the envelope-opened position
                filterCurve (g, in, (int) std::lround (bv ("f1Type")), bv ("f1Cut", 1000), bv ("f1Res"), accent.withAlpha (0.35f), bv ("f1Env") * 5.0f);
                filterCurve (g, in, (int) std::lround (bv ("f1Type")), bv ("f1Cut", 1000), bv ("f1Res"), accent);
                if (bv ("routing") > 0.5f) filterCurve (g, in, (int) std::lround (bv ("f2Type")), bv ("f2Cut", 100), bv ("f2Res"), Theme::amber());
            }
            // trance gate strip
            box (g, left, bv ("gateOn") > 0.5f ? "TRANCE GATE  on" : "TRANCE GATE  off");
            {
                static const juce::uint16 patterns[8] = { 0b1010101010101010, 0b1101101101101101, 0b1110111011101110, 0b1001001010010010, 0b1111000011110000, 0b1011011010110110, 0b1000100010001000, 0b1101011011010110 };
                auto in = left.reduced (8.0f, 16.0f);
                const juce::uint16 pat = patterns[juce::jlimit (0, 7, (int) std::lround (bv ("gatePattern")))];
                const float sw = in.getWidth() / 16.0f;
                const bool on = bv ("gateOn") > 0.5f;
                for (int i = 0; i < 16; ++i)
                {
                    const bool hit = (pat >> (15 - i)) & 1;
                    auto cell = juce::Rectangle<float> (in.getX() + sw * (float) i, in.getY(), sw, in.getHeight()).reduced (1.5f, 0.0f);
                    g.setColour (hit ? (on ? accent : Theme::dim()).withAlpha (hit ? 0.85f : 0.3f) : juce::Colours::black.withAlpha (0.35f));
                    g.fillRoundedRectangle (hit ? cell : cell.withTrimmedTop (cell.getHeight() * (1.0f - (1.0f - bv ("gateDepth")))), 2.0f);
                }
            }
            auto lfos = r.removeFromTop (r.getHeight() * 0.42f); r.removeFromTop (6.0f);
            const float lw = lfos.getWidth() / 2.0f;
            for (int i = 0; i < 2; ++i)
            {
                auto a = juce::Rectangle<float> (lfos.getX() + lw * (float) i, lfos.getY(), lw, lfos.getHeight()).reduced (2.0f, 0.0f);
                const juce::String id = i == 0 ? "lfo1" : "lfo2";
                const int shape = (int) std::lround (bv ((id + "Shape").toRawUTF8()));
                box (g, a, "LFO " + juce::String (i + 1) + "  " + SqSynth::lfoShapeNames()[juce::jlimit (0, 5, shape)] + "  " + juce::String (bv ((id + "Rate").toRawUTF8(), 1), 2) + " Hz");
                auto in = a.reduced (8.0f, 15.0f);
                juce::Path p; juce::Random rnd (7);
                float sh = 0;
                for (int k = 0; k <= (int) in.getWidth(); ++k)
                {
                    const float t = (float) k / in.getWidth() * 3.0f + phase * 0.3f;
                    float v = 0;
                    switch (shape) { case 0: v = std::sin (t * 6.2831853f); break; case 1: v = waveSample (1, t, 0.5f); break; case 2: v = waveSample (2, t, 0.5f); break; case 3: v = waveSample (3, t, 0.5f); break;
                                     default: { const int seg = (int) std::floor (t); juce::Random r2 (seg * 31 + 7); sh = shape == 4 ? r2.nextFloat() * 2.0f - 1.0f : sh + (r2.nextFloat() * 2.0f - 1.0f - sh) * 0.02f; v = sh; } }
                    const float y = in.getCentreY() - v * in.getHeight() * 0.45f;
                    if (k == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) k, y);
                }
                g.setColour (i == 0 ? Theme::acid() : Theme::mint()); g.strokePath (p, juce::PathStrokeType (1.6f));
            }
            box (g, r, "OUTPUT");
            scope (g, r.reduced (8.0f, 14.0f), accent);
        }
    };

    //==========================================================================
    // Sim Moth: three big oscillator dials drawn as rings, a single filter, two envelopes (warm amber)
    struct SimMothViz : public ShelfViz
    {
        SimMothViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz (h, p, c, false) {}
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto oscs = r.removeFromLeft (r.getWidth() * 0.5f); r.removeFromLeft (8.0f);
            box (g, oscs, "OSCILLATORS");
            {
                auto in = oscs.reduced (8.0f, 16.0f);
                const float cw = in.getWidth() / 3.0f;
                const char* ids[3][3] = { { "o1Wave", "o1Level", "o1Semi" }, { "o2Wave", "o2Level", "o2Semi" }, { "o3Wave", "o3Level", "o3Semi" } };
                const juce::Colour cols[] = { Theme::amber(), accent, Theme::acid() };
                for (int i = 0; i < 3; ++i)
                {
                    auto cell = juce::Rectangle<float> (in.getX() + cw * (float) i, in.getY(), cw, in.getHeight()).reduced (4.0f);
                    const float rad = juce::jmin (cell.getWidth(), cell.getHeight()) * 0.42f;
                    auto c = cell.getCentre();
                    const float lvl = bv (ids[i][1]);
                    for (int k = 0; k < 4; ++k)
                    {
                        g.setColour (cols[i].withAlpha (k == 0 ? 0.9f : 0.25f));
                        g.drawEllipse (c.x - rad * (0.55f + 0.15f * k), c.y - rad * (0.55f + 0.15f * k), rad * 2.0f * (0.55f + 0.15f * k), rad * 2.0f * (0.55f + 0.15f * k), k == 0 ? 2.5f : 1.0f);
                    }
                    // the waveform rides around the ring
                    juce::Path p; const int w = (int) std::lround (bv (ids[i][0]));
                    for (int k = 0; k <= 180; ++k)
                    {
                        const float a = (float) k / 180.0f * 6.2831853f;
                        const float v = waveSample (w, (float) k / 180.0f * 3.0f + phase * 0.1f, 0.5f) * lvl * rad * 0.22f;
                        const float rr = rad * 0.55f + v;
                        const float x = c.x + rr * std::cos (a), y = c.y + rr * std::sin (a);
                        if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                    }
                    p.closeSubPath();
                    g.setColour (cols[i]); g.strokePath (p, juce::PathStrokeType (1.6f));
                    g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, true));
                    const int semi = juce::roundToInt (bv (ids[i][2]));
                    g.drawText ("OSC " + juce::String (i + 1) + "  " + SqSynth::waveNames()[juce::jlimit (0, 5, w)] + "  " + (semi >= 0 ? "+" : "") + juce::String (semi), cell.toNearestInt().removeFromBottom (12), juce::Justification::centred);
                }
            }
            auto filt = r.removeFromTop (r.getHeight() * 0.5f); r.removeFromTop (6.0f);
            box (g, filt, "FILTER  " + SqSynth::filterTypeNames()[juce::jlimit (0, 6, (int) std::lround (bv ("fType")))]);
            { auto in = filt.reduced (8.0f, 16.0f); drawFreqGrid (g, in, 36.0f); filterCurve (g, in, (int) std::lround (bv ("fType")), bv ("cutoff", 1000), bv ("res"), accent); }
            auto a1 = r.removeFromLeft (r.getWidth() * 0.5f).reduced (2.0f, 0.0f), a2 = r.reduced (2.0f, 0.0f);
            box (g, a1, "AMP"); envelope (g, a1.reduced (6.0f, 14.0f), bv ("aA"), bv ("aD"), bv ("aS"), bv ("aR"), Theme::amber());
            box (g, a2, "FILTER ENV"); envelope (g, a2.reduced (6.0f, 14.0f), bv ("fA"), bv ("fD"), bv ("fS"), bv ("fR"), accent);
        }
    };

    //==========================================================================
    // Mini Moth: one big wave, one filter, one envelope - clean cyan
    struct MiniMothViz : public ShelfViz
    {
        MiniMothViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz (h, p, c, false) {}
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto wave = r.removeFromLeft (r.getWidth() * 0.45f); r.removeFromLeft (8.0f);
            const int w = (int) std::lround (bv ("wave"));
            box (g, wave, "WAVE  " + SqSynth::waveNames()[juce::jlimit (0, 5, w)] + "   sub " + juce::String (juce::roundToInt (bv ("subLevel") * 100)) + "%");
            {
                auto in = wave.reduced (10.0f, 16.0f);
                juce::Path p, s;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth() * 2.0f + phase * 0.1f;
                    const float y = in.getCentreY() - waveSample (w, t, 0.5f) * in.getHeight() * 0.3f;
                    const float ys = in.getCentreY() - waveSample (3, t * 0.5f, 0.5f) * in.getHeight() * 0.3f * bv ("subLevel");
                    if (i == 0) { p.startNewSubPath (in.getX(), y); s.startNewSubPath (in.getX(), ys); } else { p.lineTo (in.getX() + (float) i, y); s.lineTo (in.getX() + (float) i, ys); }
                }
                g.setColour (accent.withAlpha (0.35f)); g.strokePath (s, juce::PathStrokeType (1.2f));
                g.setColour (accent); g.strokePath (p, juce::PathStrokeType (3.0f));
            }
            auto filt = r.removeFromLeft (r.getWidth() * 0.55f); r.removeFromLeft (8.0f);
            box (g, filt, "FILTER  LP 24");
            { auto in = filt.reduced (8.0f, 16.0f); drawFreqGrid (g, in, 36.0f); filterCurve (g, in, 0, bv ("cutoff", 1000), bv ("res"), accent, bv ("fEnv") * 2.5f); }
            box (g, r, "ENVELOPE");
            envelope (g, r.reduced (6.0f, 14.0f), bv ("attack"), bv ("decay"), bv ("sustain"), bv ("release"), accent);
        }
    };

    //==========================================================================
    // Thump: pitch sweep + amplitude decay on one axis, click burst, drive curve, scope (red)
    struct ThumpViz : public ShelfViz
    {
        ThumpViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz (h, p, c, false) {}
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto sweep = r.removeFromLeft (r.getWidth() * 0.45f); r.removeFromLeft (8.0f);
            box (g, sweep, "PITCH SWEEP  +" + juce::String (juce::roundToInt (bv ("startPitch"))) + " st  >  " + juce::String (juce::roundToInt (bv ("endHz"))) + " Hz");
            {
                auto in = sweep.reduced (8.0f, 16.0f);
                const float st = bv ("startPitch"), pt = juce::jmax (0.002f, bv ("pitchTime", 0.06f)), curve = bv ("pitchCurve", 2.0f), decay = juce::jmax (0.05f, bv ("decay", 0.6f));
                const float span = juce::jmin (1.5f, decay * 1.1f);
                juce::Path pitch, amp;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth() * span;
                    const float semis = st * std::exp (-t / pt * curve);
                    const float y = in.getBottom() - juce::jlimit (0.0f, 1.0f, semis / 60.0f) * in.getHeight() * 0.9f - in.getHeight() * 0.05f;
                    const float a = std::exp (-4.6f * t / decay);
                    const float ya = in.getBottom() - a * in.getHeight();
                    if (i == 0) { pitch.startNewSubPath (in.getX(), y); amp.startNewSubPath (in.getX(), ya); } else { pitch.lineTo (in.getX() + (float) i, y); amp.lineTo (in.getX() + (float) i, ya); }
                }
                g.setColour (Theme::amber().withAlpha (0.3f)); juce::Path f (amp); f.lineTo (in.getRight(), in.getBottom()); f.lineTo (in.getX(), in.getBottom()); f.closeSubPath(); g.fillPath (f);
                g.setColour (accent); g.strokePath (pitch, juce::PathStrokeType (2.4f));
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f));
                g.drawText (juce::String (span, 2) + " s", in.toNearestInt(), juce::Justification::bottomRight);
            }
            auto parts = r.removeFromTop (r.getHeight() * 0.5f); r.removeFromTop (6.0f);
            auto click = parts.removeFromLeft (parts.getWidth() * 0.5f).reduced (2.0f, 0.0f), drive = parts.reduced (2.0f, 0.0f);
            box (g, click, "CLICK + BODY");
            {
                auto in = click.reduced (8.0f, 14.0f);
                juce::Random rnd (3); juce::Path p;
                const float ck = bv ("click"), bd = bv ("body"), ratio = bv ("bodyRatio", 1.5f);
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth();
                    const float v = (rnd.nextFloat() * 2.0f - 1.0f) * ck * std::exp (-t * 40.0f) + std::sin (t * 60.0f * ratio) * bd * std::exp (-t * 6.0f);
                    const float y = in.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * in.getHeight() * 0.45f;
                    if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
                }
                g.setColour (Theme::amber()); g.strokePath (p, juce::PathStrokeType (1.4f));
            }
            box (g, drive, "DRIVE  " + SqSynth::driveTypeNames()[juce::jlimit (0, 3, (int) std::lround (bv ("driveType")))]);
            {
                auto in = drive.reduced (10.0f, 14.0f); juce::Path c;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float x = (float) i / in.getWidth() * 2.0f - 1.0f;
                    const float y = SqSynth::driveShape (x, bv ("drive"), (int) std::lround (bv ("driveType")));
                    const float px = in.getX() + (float) i, py = in.getCentreY() - y * in.getHeight() * 0.48f;
                    if (i == 0) c.startNewSubPath (px, py); else c.lineTo (px, py);
                }
                g.setColour (Theme::text().withAlpha (0.12f)); g.drawLine (in.getX(), in.getCentreY(), in.getRight(), in.getCentreY());
                g.setColour (accent); g.strokePath (c, juce::PathStrokeType (2.0f));
            }
            box (g, r, "OUTPUT");
            scope (g, r.reduced (8.0f, 14.0f), Theme::amber());
        }
    };

    //==========================================================================
    // Dial 10: the operator pair as two dials, the sideband spectrum, two envelopes (orange)
    struct Dial10Viz : public ShelfViz
    {
        using ShelfViz::ShelfViz;
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto ops = r.removeFromLeft (r.getWidth() * 0.3f); r.removeFromLeft (8.0f);
            box (g, ops, "OPERATORS");
            {
                auto in = ops.reduced (8.0f, 16.0f);
                auto top = in.removeFromTop (in.getHeight() * 0.5f), bot = in;
                auto dial = [&] (juce::Rectangle<float> a, const juce::String& t, float amount, juce::Colour c)
                {
                    const float rad = juce::jmin (a.getWidth(), a.getHeight()) * 0.4f; auto ctr = a.getCentre();
                    juce::Path arc; arc.addCentredArc (ctr.x, ctr.y, rad, rad, 0.0f, -2.4f, 2.4f, true);
                    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.strokePath (arc, juce::PathStrokeType (6.0f));
                    juce::Path lit; lit.addCentredArc (ctr.x, ctr.y, rad, rad, 0.0f, -2.4f, -2.4f + 4.8f * juce::jlimit (0.0f, 1.0f, amount), true);
                    g.setColour (c); g.strokePath (lit, juce::PathStrokeType (6.0f));
                    g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, true)); g.drawText (t, a.toNearestInt(), juce::Justification::centred);
                };
                dial (top, "MOD  x" + juce::String (bv ("ratio", 1), 2), bv ("modLevel"), accent);
                dial (bot, "CARRIER", 0.6f + 0.4f * synth->displayLevel(), Theme::amber());
                g.setColour (accent.withAlpha (0.6f)); g.drawArrow (juce::Line<float> (top.getCentreX(), top.getBottom() - 6.0f, bot.getCentreX(), bot.getY() + 6.0f), 2.0f, 8.0f, 8.0f);
            }
            auto spec = r.removeFromTop (r.getHeight() * 0.58f); r.removeFromTop (6.0f);
            box (g, spec, "SIDEBANDS  index " + juce::String (bv ("modLevel") * 8.0f, 1) + "   feedback " + juce::String (juce::roundToInt (bv ("feedback") * 100)) + "%");
            {
                auto in = spec.reduced (8.0f, 16.0f);
                const float idx = bv ("modLevel") * 8.0f * (0.3f + 0.7f * bv ("mS")), ratio = bv ("ratio", 1), fb = bv ("feedback");
                const int bands = 24; const float bw = in.getWidth() / (float) bands;
                for (int k = 0; k < bands; ++k)
                {
                    // crude Bessel-like envelope: harmonics at 1 + n*ratio
                    float amp = 0.0f;
                    for (int n = -12; n <= 12; ++n)
                    {
                        const float f = 1.0f + (float) n * ratio;
                        if (std::abs (f - (float) (k + 1)) < 0.5f) amp += std::exp (-(float) (n * n) / juce::jmax (0.2f, idx * (1.0f + fb))) * (1.0f / (1.0f + 0.15f * std::abs ((float) n)));
                    }
                    amp = juce::jlimit (0.0f, 1.0f, amp);
                    auto bar = juce::Rectangle<float> (in.getX() + bw * (float) k, in.getBottom() - amp * in.getHeight(), bw - 2.0f, amp * in.getHeight());
                    g.setColour (accent.withAlpha (0.35f + 0.6f * amp)); g.fillRoundedRectangle (bar, 2.0f);
                }
            }
            auto a1 = r.removeFromLeft (r.getWidth() * 0.5f).reduced (2.0f, 0.0f), a2 = r.reduced (2.0f, 0.0f);
            box (g, a1, "MOD ENV"); envelope (g, a1.reduced (6.0f, 14.0f), bv ("mA"), bv ("mD"), bv ("mS"), bv ("mR"), accent);
            box (g, a2, "AMP ENV"); envelope (g, a2.reduced (6.0f, 14.0f), bv ("aA"), bv ("aD"), bv ("aS"), bv ("aR"), Theme::amber());
        }
    };
