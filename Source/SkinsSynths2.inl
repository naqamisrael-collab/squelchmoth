// Included inside namespace Skins (PluginSkins.h): faces for the 1.8.1 instruments (SqSynths2.h, SqDrumSynths.h).
// Hexwing, Toxin FM, Lantern, Lantern Exo, Larva Bass, Thorn Bass, Carapace, Live Drum Synth, Drum Pad Model, Thump Live.
// Shared interactive pieces: HandleStrip (a column of faders bound to parameters by name), StepGrid (16-step lanes),
// EnvEditor4 (four drag-able breakpoints), PadGrid (click to play / select a sound).

    //==========================================================================
    /** A compact column of horizontal faders bound to parameters by display name. Choice parameters show their
        name and open a menu; on/off ones are lamps; drag horizontally (or vertically for fine control), double-click resets. */
    class HandleStrip : public juce::Component
    {
    public:
        HandleStrip (HandleMap& h, juce::Colour acc) : handles (h), accent (acc) {}
        void setRows (const juce::StringArray& names, const juce::StringArray& labels = {})
        {
            rows = names; rowLabels = labels; repaint();
        }
        std::function<void()> onChange;
        int rowHeight = 20;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float rh = juce::jmin ((float) rowHeight, r.getHeight() / juce::jmax (1, rows.size()));
            for (int i = 0; i < rows.size(); ++i)
            {
                auto row = juce::Rectangle<float> (r.getX(), r.getY() + rh * (float) i, r.getWidth(), rh).reduced (0.0f, 1.5f);
                auto* hd = handle (i);
                auto label = row.removeFromLeft (row.getWidth() * 0.36f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
                g.drawFittedText ((i < rowLabels.size() ? rowLabels[i] : rows[i]).toUpperCase(), label.toNearestInt(), juce::Justification::centredLeft, 1);
                if (hd == nullptr) { g.drawText ("-", row.toNearestInt(), juce::Justification::centredLeft); continue; }
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (row, 3.0f);
                if (! hd->choices.isEmpty())
                {
                    const int n = hd->choices.size(), idx = juce::jlimit (0, n - 1, juce::roundToInt (hd->get() * (float) (n - 1)));
                    g.setColour (accent.withAlpha (0.18f)); g.fillRoundedRectangle (row.reduced (1.0f), 3.0f);
                    g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, true));
                    g.drawFittedText (hd->choices[idx], row.toNearestInt().reduced (6, 0), juce::Justification::centredLeft, 1);
                    g.setColour (Theme::dim()); g.drawText ("v", row.toNearestInt().reduced (6, 0), juce::Justification::centredRight);
                }
                else if (hd->toggle)
                {
                    const bool on = hd->get() > 0.5f;
                    auto lamp = row.removeFromLeft (row.getHeight()).reduced (4.0f);
                    g.setColour (on ? accent : Theme::dim().withAlpha (0.4f)); g.fillEllipse (lamp);
                    g.setColour (on ? Theme::text() : Theme::dim()); g.setFont (Theme::font (9.0f, on)); g.drawText (on ? "ON" : "OFF", row.toNearestInt().reduced (4, 0), juce::Justification::centredLeft);
                }
                else
                {
                    const float v = juce::jlimit (0.0f, 1.0f, hd->get());
                    auto bar = row.reduced (1.0f);
                    if (hd->bipolar) { const float mid = bar.getCentreX(), half = bar.getWidth() * 0.5f * (v - 0.5f) * 2.0f;
                                       g.setColour (accent.withAlpha (0.75f)); g.fillRoundedRectangle (half >= 0 ? juce::Rectangle<float> (mid, bar.getY(), half, bar.getHeight()) : juce::Rectangle<float> (mid + half, bar.getY(), -half, bar.getHeight()), 2.0f);
                                       g.setColour (Theme::text().withAlpha (0.4f)); g.drawVerticalLine ((int) mid, bar.getY(), bar.getBottom()); }
                    else { g.setColour (accent.withAlpha (0.75f)); g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * v)), 2.0f); }
                    g.setColour (Theme::text()); g.setFont (Theme::font (9.0f));
                    g.drawFittedText (hd->text ? hd->text() : juce::String (v, 2), row.toNearestInt().reduced (6, 0), juce::Justification::centredRight, 1);
                }
            }
        }
        int rowAt (juce::Point<float> p) const
        {
            const float rh = juce::jmin ((float) rowHeight, (float) getHeight() / juce::jmax (1, rows.size()));
            const int i = (int) (p.y / rh); return juce::isPositiveAndBelow (i, rows.size()) ? i : -1;
        }
        Handle* handle (int i) const   { if (! juce::isPositiveAndBelow (i, rows.size())) return nullptr; auto it = handles.find (rows[i]); return it != handles.end() ? it->second : nullptr; }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragRow = rowAt (e.position); auto* hd = handle (dragRow); if (hd == nullptr) { dragRow = -1; return; }
            if (hd->gestureBegin) hd->gestureBegin();
            if (! hd->choices.isEmpty())
            {
                juce::PopupMenu m; const int n = hd->choices.size(), cur = juce::roundToInt (hd->get() * (float) (n - 1));
                for (int i = 0; i < n; ++i) m.addItem (i + 1, hd->choices[i], true, i == cur);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, hd, n] (int r) { if (r > 0) { hd->set ((float) (r - 1) / (float) juce::jmax (1, n - 1)); if (hd->gestureEnd) hd->gestureEnd(); repaint(); if (onChange) onChange(); } });
                dragRow = -1; return;
            }
            if (hd->toggle) { hd->set (hd->get() > 0.5f ? 0.0f : 1.0f); if (hd->gestureEnd) hd->gestureEnd(); dragRow = -1; repaint(); if (onChange) onChange(); return; }
            dragStart = hd->get();
            // click straight onto the bar sets the value there
            const float barX = (float) getWidth() * 0.36f, barW = (float) getWidth() * 0.64f;
            if (e.position.x > barX && e.mods.isLeftButtonDown() && e.getNumberOfClicks() == 1 && ! e.mods.isShiftDown())
            {
                const float v = juce::jlimit (0.0f, 1.0f, (e.position.x - barX) / barW);
                if (std::abs (v - dragStart) > 0.04f) { hd->set (v); dragStart = v; }
            }
            dragOrigin = e.position;
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            auto* hd = handle (dragRow); if (hd == nullptr) return;
            const float dx = (e.position.x - dragOrigin.x) / ((float) getWidth() * 0.64f), dy = -(e.position.y - dragOrigin.y) / 300.0f;
            hd->set (juce::jlimit (0.0f, 1.0f, dragStart + (e.mods.isShiftDown() ? dx * 0.1f : dx) + dy)); repaint();
        }
        void mouseUp (const juce::MouseEvent&) override   { if (auto* hd = handle (dragRow)) { if (hd->gestureEnd) hd->gestureEnd(); } dragRow = -1; if (onChange) onChange(); }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (auto* hd = handle (rowAt (e.position))) if (hd->choices.isEmpty() && ! hd->toggle) { hd->set (hd->defaultNorm); repaint(); if (onChange) onChange(); }
        }
    private:
        HandleMap& handles; juce::Colour accent; juce::StringArray rows, rowLabels;
        int dragRow = -1; float dragStart = 0.0f; juce::Point<float> dragOrigin;
    };

    //==========================================================================
    /** Sixteen steps: an optional note lane (drag up / down) and any number of on/off lanes (click). The playhead
        comes from the instrument. Names are built from a pattern: "Step %d Note", "Pattern Step %d", ... */
    class StepGrid : public juce::Component
    {
    public:
        struct Lane { juce::String title, pattern; bool note = false; };
        StepGrid (HandleMap& h, juce::Colour acc, std::vector<Lane> l, std::function<int()> step) : handles (h), accent (acc), lanes (std::move (l)), currentStep (std::move (step)) {}
        std::function<void()> onChange;

        Handle* handleFor (int lane, int step) const
        {
            auto it = handles.find (lanes[(size_t) lane].pattern.replace ("%d", juce::String (step + 1)));
            return it != handles.end() ? it->second : nullptr;
        }
        juce::Rectangle<float> laneArea (int lane) const
        {
            auto r = getLocalBounds().toFloat().withTrimmedLeft (54.0f);
            int toggles = 0; bool hasNote = false;
            for (auto& l : lanes) { if (l.note) hasNote = true; else ++toggles; }
            const float toggleH = hasNote ? 18.0f : juce::jmax (12.0f, r.getHeight() / (float) juce::jmax (1, toggles)), totalToggles = toggleH * (float) toggles;
            const float noteH = hasNote ? juce::jmax (30.0f, r.getHeight() - totalToggles - 4.0f) : 0.0f;
            float y = r.getY();
            for (int i = 0; i < (int) lanes.size(); ++i)
            {
                const float hgt = lanes[(size_t) i].note ? noteH : toggleH;
                if (i == lane) return { r.getX(), y, r.getWidth(), hgt };
                y += hgt + (lanes[(size_t) i].note ? 4.0f : 0.0f);
            }
            return {};
        }
        void paint (juce::Graphics& g) override
        {
            const int play = currentStep ? currentStep() : -1;
            for (int li = 0; li < (int) lanes.size(); ++li)
            {
                auto a = laneArea (li); const auto& lane = lanes[(size_t) li];
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f, true));
                g.drawFittedText (lane.title.toUpperCase(), juce::Rectangle<int> (0, (int) a.getY(), 52, (int) a.getHeight()), juce::Justification::centredLeft, 1);
                const float sw = a.getWidth() / 16.0f;
                for (int s = 0; s < 16; ++s)
                {
                    auto cell = juce::Rectangle<float> (a.getX() + sw * (float) s, a.getY(), sw, a.getHeight()).reduced (1.5f, 1.0f);
                    const bool beat = s % 4 == 0;
                    g.setColour (juce::Colours::black.withAlpha (beat ? 0.5f : 0.35f)); g.fillRoundedRectangle (cell, 2.0f);
                    if (s == play) { g.setColour (Theme::text().withAlpha (0.12f)); g.fillRoundedRectangle (cell, 2.0f); }
                    auto* hd = handleFor (li, s); if (hd == nullptr) continue;
                    const float v = juce::jlimit (0.0f, 1.0f, hd->get());
                    if (lane.note)
                    {
                        auto bar = cell.withTop (cell.getBottom() - juce::jmax (3.0f, cell.getHeight() * v));
                        const bool on = onAt (s);
                        g.setColour ((on ? accent : Theme::dim()).withAlpha (s == play ? 1.0f : 0.8f)); g.fillRoundedRectangle (bar, 2.0f);
                        if (cell.getWidth() > 26.0f) { g.setColour (Theme::text()); g.setFont (Theme::font (8.0f)); g.drawFittedText (hd->text ? hd->text().trim() : juce::String(), cell.toNearestInt(), juce::Justification::centredTop, 1); }
                    }
                    else
                    {
                        const bool on = v > 0.5f;
                        g.setColour (on ? accent.withAlpha (s == play ? 1.0f : 0.85f) : Theme::dim().withAlpha (0.25f)); g.fillRoundedRectangle (cell.reduced (2.0f), 2.0f);
                    }
                }
            }
        }
        bool onAt (int s) const   { for (int li = 0; li < (int) lanes.size(); ++li) if (lanes[(size_t) li].title.equalsIgnoreCase ("on")) if (auto* hd = handleFor (li, s)) return hd->get() > 0.5f; return true; }
        int laneAt (juce::Point<float> p) const   { for (int i = 0; i < (int) lanes.size(); ++i) if (laneArea (i).contains (p)) return i; return -1; }
        int stepAt (juce::Point<float> p) const   { auto a = laneArea (0); const int s = (int) ((p.x - a.getX()) / (a.getWidth() / 16.0f)); return juce::isPositiveAndBelow (s, 16) ? s : -1; }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragLane = laneAt (e.position); dragStep = stepAt (e.position);
            if (dragLane < 0 || dragStep < 0) return;
            auto* hd = handleFor (dragLane, dragStep); if (hd == nullptr) return;
            if (lanes[(size_t) dragLane].note) { dragStart = hd->get(); paintValue = -1.0f; applyNote (e.position); }
            else { paintValue = hd->get() > 0.5f ? 0.0f : 1.0f; hd->set (paintValue); }
            repaint(); if (onChange) onChange();
        }
        void applyNote (juce::Point<float> p)
        {
            auto a = laneArea (dragLane); auto* hd = handleFor (dragLane, dragStep); if (hd == nullptr) return;
            hd->set (juce::jlimit (0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight()));
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragLane < 0) return;
            if (lanes[(size_t) dragLane].note) { const int s = stepAt (e.position); if (s >= 0) dragStep = s; applyNote (e.position); }
            else { const int s = stepAt (e.position); if (s >= 0) if (auto* hd = handleFor (dragLane, s)) hd->set (paintValue); }
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override   { dragLane = -1; if (onChange) onChange(); }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int li = laneAt (e.position), s = stepAt (e.position);
            if (li >= 0 && s >= 0 && lanes[(size_t) li].note) if (auto* hd = handleFor (li, s)) { hd->set (hd->defaultNorm); repaint(); if (onChange) onChange(); }
        }
    private:
        HandleMap& handles; juce::Colour accent; std::vector<Lane> lanes; std::function<int()> currentStep;
        int dragLane = -1, dragStep = -1; float dragStart = 0.0f, paintValue = 0.0f;
    };

    //==========================================================================
    /** Four breakpoints (t0 = 0, V0) (T1, V1) (T2, V2) (T3, V3): drag a point to move it in time and value. */
    class EnvEditor4 : public juce::Component
    {
    public:
        EnvEditor4 (HandleMap& h, juce::Colour acc, juce::String title, juce::String timePrefix, juce::String valuePrefix, juce::String unit)
            : handles (h), accent (acc), name (std::move (title)), tPre (std::move (timePrefix)), vPre (std::move (valuePrefix)), units (std::move (unit)) {}
        std::function<void()> onChange;

        Handle* th (int i) const   { auto it = handles.find (tPre + juce::String (i)); return it != handles.end() ? it->second : nullptr; }
        Handle* vh (int i) const   { auto it = handles.find (vPre + juce::String (i)); return it != handles.end() ? it->second : nullptr; }
        float timeOf (int i) const   { if (i == 0) return 0.0f; auto* hd = th (i); return hd != nullptr && hd->param != nullptr ? hd->param->getCurrentValue() : 0.1f * (float) i; }
        float valueOf (int i) const  { auto* hd = vh (i); return hd != nullptr ? juce::jlimit (0.0f, 1.0f, hd->get()) : 0.0f; }
        float span() const           { return juce::jmax (0.05f, timeOf (3) * 1.15f); }
        juce::Rectangle<float> area() const   { return getLocalBounds().toFloat().reduced (8.0f, 4.0f).withTrimmedTop (14.0f).withTrimmedBottom (4.0f); }
        juce::Point<float> pointOf (int i) const   { auto a = area(); return { a.getX() + a.getWidth() * std::sqrt (timeOf (i) / span()), a.getBottom() - a.getHeight() * valueOf (i) }; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
            g.drawText (name, r.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
            g.drawText (juce::String (span(), 2) + " s", r.toNearestInt().reduced (6, 3), juce::Justification::topRight);
            auto a = area();
            g.setColour (Theme::text().withAlpha (0.06f)); for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());
            juce::Path p; p.startNewSubPath (pointOf (0));
            for (int i = 1; i < 4; ++i) p.lineTo (pointOf (i));
            p.lineTo (a.getRight(), pointOf (3).y);
            juce::Path f (p); f.lineTo (a.getRight(), a.getBottom()); f.lineTo (a.getX(), a.getBottom()); f.closeSubPath();
            g.setColour (accent.withAlpha (0.18f)); g.fillPath (f);
            g.setColour (accent); g.strokePath (p, juce::PathStrokeType (2.0f));
            for (int i = 0; i < 4; ++i)
            {
                auto pt = pointOf (i);
                g.setColour (i == hover || i == drag ? Theme::text() : accent); g.fillEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
                g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f, 1.0f);
            }
            if (drag >= 0 || hover >= 0)
            {
                const int i = drag >= 0 ? drag : hover; auto* vhd = vh (i);
                g.setColour (Theme::text()); g.setFont (Theme::font (8.5f));
                g.drawText ((vhd != nullptr && vhd->text ? vhd->text().trim() : juce::String (valueOf (i), 2)) + units + (i > 0 ? "  @ " + juce::String (timeOf (i), 3) + " s" : juce::String()), a.toNearestInt(), juce::Justification::bottomRight);
            }
        }
        int pointAt (juce::Point<float> p) const   { for (int i = 3; i >= 0; --i) if (pointOf (i).getDistanceFrom (p) < 9.0f) return i; return -1; }
        void mouseMove (const juce::MouseEvent& e) override   { const int h = pointAt (e.position); if (h != hover) { hover = h; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override    { hover = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override  { drag = pointAt (e.position); if (drag >= 0) { if (auto* hd = vh (drag)) if (hd->gestureBegin) hd->gestureBegin(); if (auto* hd = th (drag)) if (hd->gestureBegin) hd->gestureBegin(); } }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (drag < 0) return;
            auto a = area();
            if (auto* hd = vh (drag)) hd->set (juce::jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight()));
            if (drag > 0) if (auto* hd = th (drag)) if (hd->param != nullptr)
            {
                const float frac = juce::jlimit (0.0f, 1.0f, (e.position.x - a.getX()) / a.getWidth());
                float t = frac * frac * span();
                const auto range = hd->param->getValueRange();
                t = juce::jlimit (range.getStart(), range.getEnd(), t);
                // keep the breakpoints in order
                if (drag > 1) t = juce::jmax (t, timeOf (drag - 1) + 0.001f);
                if (drag < 3) t = juce::jmin (t, timeOf (drag + 1) - 0.001f);
                hd->param->setParameter (t, juce::sendNotification);
            }
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override   { if (drag >= 0) { if (auto* hd = vh (drag)) if (hd->gestureEnd) hd->gestureEnd(); if (auto* hd = th (drag)) if (hd->gestureEnd) hd->gestureEnd(); } drag = -1; if (onChange) onChange(); repaint(); }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int i = pointAt (e.position); if (i < 0) return;
            if (auto* hd = vh (i)) hd->set (hd->defaultNorm);
            if (i > 0) if (auto* hd = th (i)) hd->set (hd->defaultNorm);
            repaint(); if (onChange) onChange();
        }
    private:
        HandleMap& handles; juce::Colour accent; juce::String name, tPre, vPre, units; int hover = -1, drag = -1;
    };

    //==========================================================================
    /** A grid of pads: click plays the sound (and selects it for editing). */
    class PadGrid : public juce::Component
    {
    public:
        PadGrid (juce::StringArray n, int cols, juce::Colour acc) : names (std::move (n)), columns (cols), accent (acc) {}
        std::function<void (int)> onHit, onSelect;
        std::function<int()> lastHit;
        std::function<int (int)> modelOf;   // optional: an index into modelNames for the pad's caption
        juce::StringArray modelNames;
        int selected = 0; bool round = false;
        juce::Rectangle<float> padRect (int i) const
        {
            const int rows = (names.size() + columns - 1) / columns;
            auto r = getLocalBounds().toFloat();
            const float pw = r.getWidth() / (float) columns, ph = r.getHeight() / (float) rows;
            const int row = rows - 1 - i / columns, col = i % columns;   // pad 1 bottom-left like a drum machine
            return juce::Rectangle<float> (r.getX() + pw * (float) col, r.getY() + ph * (float) row, pw, ph).reduced (3.0f);
        }
        void paint (juce::Graphics& g) override
        {
            const int hit = lastHit ? lastHit() : -1;
            for (int i = 0; i < names.size(); ++i)
            {
                auto p = padRect (i); const bool sel = i == selected, lit = i == hit && glow > 0.01f;
                juce::Colour base = juce::Colours::black.withAlpha (0.45f);
                if (round) { g.setColour (base); g.fillEllipse (p.reduced (2.0f)); if (lit) { g.setColour (accent.withAlpha (glow * 0.6f)); g.fillEllipse (p.reduced (2.0f)); } g.setColour (sel ? accent : Theme::dim().withAlpha (0.4f)); g.drawEllipse (p.reduced (2.0f), sel ? 2.0f : 1.0f); }
                else { g.setColour (base); g.fillRoundedRectangle (p, 5.0f); if (lit) { g.setColour (accent.withAlpha (glow * 0.6f)); g.fillRoundedRectangle (p, 5.0f); } g.setColour (sel ? accent : Theme::dim().withAlpha (0.4f)); g.drawRoundedRectangle (p.reduced (1.0f), 5.0f, sel ? 2.0f : 1.0f); }
                g.setColour (Theme::text()); g.setFont (Theme::font (p.getHeight() > 48.0f ? 10.0f : 9.0f, true));
                g.drawFittedText (names[i], p.toNearestInt().reduced (4, 4), juce::Justification::centred, 2);
                g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f));
                g.drawText (juce::MidiMessage::getMidiNoteName (36 + i, true, true, 3), p.toNearestInt().reduced (5, 3), juce::Justification::topLeft);
                if (modelOf && ! modelNames.isEmpty()) g.drawText (modelNames[juce::jlimit (0, modelNames.size() - 1, modelOf (i))], p.toNearestInt().reduced (5, 3), juce::Justification::bottomRight);
            }
        }
        void tick()   { const int hit = lastHit ? lastHit() : -1; if (hit != lastSeen) { lastSeen = hit; glow = 1.0f; } else glow *= 0.82f; }
        int padAt (juce::Point<float> p) const   { for (int i = 0; i < names.size(); ++i) if (padRect (i).contains (p)) return i; return -1; }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = padAt (e.position); if (i < 0) return;
            selected = i; glow = 1.0f;
            if (onHit) onHit (i);
            if (onSelect) onSelect (i);
            repaint();
        }
    private:
        juce::StringArray names; int columns; juce::Colour accent; float glow = 0.0f; int lastSeen = -1;
    };

    //==========================================================================
    /** Shared bits for the 1.8.1 faces. */
    struct ShelfViz2 : public ShelfViz
    {
        using ShelfViz::ShelfViz;
        /** Natural (un-normalised) value of a parameter by display name. */
        float nv (const juce::String& name, float fb = 0.0f) const   { auto* hd = h (name); return hd != nullptr && hd->param != nullptr ? hd->param->getCurrentValue() : fb; }
        int choiceIndex (const juce::String& name) const   { auto* hd = h (name); if (hd == nullptr) return 0; const int n = juce::jmax (2, hd->choices.size()); return juce::jlimit (0, n - 1, juce::roundToInt (hd->get() * (float) (n - 1))); }

        /** The six FM operators as a routing diagram for the current algorithm. */
        void fmAlgorithm (juce::Graphics& g, juce::Rectangle<float> in, int alg)
        {
            const auto& a = SqSynth::algorithm (alg);
            // depth of each op: carriers 0, their modulators 1, ...
            int depth[6] = { 0, 0, 0, 0, 0, 0 };
            for (int pass = 0; pass < 6; ++pass)
                for (int o = 0; o < 6; ++o)
                    for (int k = 0; k < a.numMods[o]; ++k) depth[a.mods[o][k]] = juce::jmax (depth[a.mods[o][k]], depth[o] + 1);
            int maxDepth = 0; for (int d : depth) maxDepth = juce::jmax (maxDepth, d);
            // columns: carriers in order, modulators above the op they feed
            float xs[6], ys[6]; int col[6] = { 0 };
            int c = 0; for (int o = 0; o < 6; ++o) if (a.carrier[o]) col[o] = c++;
            const int carriers = juce::jmax (1, c);
            for (int pass = 0; pass < 6; ++pass) for (int o = 0; o < 6; ++o) for (int k = 0; k < a.numMods[o]; ++k) col[a.mods[o][k]] = col[o];
            const float bw = juce::jmin (46.0f, in.getWidth() / (float) carriers - 6.0f), bh = juce::jmin (26.0f, in.getHeight() / (float) (maxDepth + 1) - 6.0f);
            for (int o = 0; o < 6; ++o)
            {
                // several modulators at the same depth in one column spread sideways
                int same = 0, idx = 0;
                for (int p = 0; p < 6; ++p) if (col[p] == col[o] && depth[p] == depth[o]) { if (p == o) idx = same; ++same; }
                const float cx = in.getX() + in.getWidth() * ((float) col[o] + 0.5f) / (float) carriers + ((float) idx - (float) (same - 1) * 0.5f) * (bw + 4.0f);
                xs[o] = cx; ys[o] = in.getBottom() - bh * 0.5f - (float) depth[o] * (in.getHeight() - bh) / (float) juce::jmax (1, maxDepth) * (maxDepth > 0 ? 1.0f : 0.0f);
            }
            // links
            for (int o = 0; o < 6; ++o) for (int k = 0; k < a.numMods[o]; ++k)
            {
                const int m = a.mods[o][k];
                g.setColour (accent.withAlpha (0.55f)); g.drawArrow (juce::Line<float> (xs[m], ys[m] + bh * 0.5f, xs[o], ys[o] - bh * 0.5f), 1.5f, 6.0f, 6.0f);
            }
            auto waves = SqSynth::opWaveNames();
            for (int o = 0; o < 6; ++o)
            {
                const juce::String n = juce::String (o + 1);
                const float lvl = nv ("Op " + n + " Level"), fb = nv ("Op " + n + " Feedback");
                auto box = juce::Rectangle<float> (xs[o] - bw * 0.5f, ys[o] - bh * 0.5f, bw, bh);
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (box, 4.0f);
                g.setColour ((a.carrier[o] ? Theme::amber() : accent).withAlpha (0.2f + 0.75f * lvl)); g.fillRoundedRectangle (box, 4.0f);
                g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, true)); g.drawText (n, box.toNearestInt(), juce::Justification::centred);
                g.setFont (Theme::font (6.5f)); g.setColour (Theme::text().withAlpha (0.8f));
                g.drawText (waves[juce::jlimit (0, 5, (int) std::lround (nv ("Op " + n + " Wave")))], box.toNearestInt().reduced (3, 1), juce::Justification::bottomRight);
                g.drawText ("x" + juce::String (nv ("Op " + n + " Ratio", 1.0f), 2).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."), box.toNearestInt().reduced (3, 1), juce::Justification::topLeft);
                if (fb > 0.01f) { juce::Path loop; loop.addCentredArc (box.getRight() - 2.0f, box.getY() + 2.0f, 5.0f, 5.0f, 0.0f, 0.5f, 5.6f, true); g.setColour (accent.withAlpha (0.4f + 0.6f * fb)); g.strokePath (loop, juce::PathStrokeType (1.5f)); }
            }
        }
        /** Six small operator envelopes in two rows. */
        void opEnvelopes (juce::Graphics& g, juce::Rectangle<float> in, int highlightCarriersAlg = -1)
        {
            const float cw = in.getWidth() / 3.0f, ch = in.getHeight() / 2.0f;
            for (int o = 0; o < 6; ++o)
            {
                const juce::String n = juce::String (o + 1);
                auto cell = juce::Rectangle<float> (in.getX() + cw * (float) (o % 3), in.getY() + ch * (float) (o / 3), cw, ch).reduced (3.0f);
                const bool carrier = highlightCarriersAlg >= 0 && SqSynth::algorithm (highlightCarriersAlg).carrier[o];
                g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (cell, 3.0f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f, true)); g.drawText ("OP " + n, cell.toNearestInt().reduced (4, 1), juce::Justification::topLeft);
                envelope (g, cell.reduced (5.0f, 11.0f), nv ("Op " + n + " Attack"), nv ("Op " + n + " Decay"), nv ("Op " + n + " Sustain"), nv ("Op " + n + " Release"), (carrier ? Theme::amber() : accent).withAlpha (0.4f + 0.6f * nv ("Op " + n + " Level")));
            }
        }
        /** Approximate partial spectrum of the FM patch: bars at the carrier's harmonics, weighted by index. */
        void fmSpectrum (juce::Graphics& g, juce::Rectangle<float> in, int alg)
        {
            const auto& a = SqSynth::algorithm (alg);
            const int bands = 32; float amp[32] = { 0 };
            const float depth = nv ("Mod Depth", 1.0f);
            for (int o = 0; o < 6; ++o) if (a.carrier[o])
            {
                const float lvl = nv ("Op " + juce::String (o + 1) + " Level"); if (lvl < 0.01f) continue;
                float idx = 0.0f, ratio = 1.0f;
                for (int k = 0; k < a.numMods[o]; ++k) { const int m = a.mods[o][k]; idx += nv ("Op " + juce::String (m + 1) + " Level") * depth * 2.2f; ratio = nv ("Op " + juce::String (m + 1) + " Ratio", 1.0f); }
                const float cr = nv ("Op " + juce::String (o + 1) + " Ratio", 1.0f);
                for (int n = -16; n <= 16; ++n)
                {
                    const float f = cr + (float) n * ratio; if (f < 0.5f) continue;
                    const int k = (int) std::lround (f) - 1; if (! juce::isPositiveAndBelow (k, bands)) continue;
                    amp[k] += lvl * (idx < 0.05f ? (n == 0 ? 1.0f : 0.0f) : std::exp (-(float) (n * n) / juce::jmax (0.3f, idx * 1.5f)) / (1.0f + 0.1f * std::abs ((float) n)));
                }
            }
            const float bw = in.getWidth() / (float) bands;
            for (int k = 0; k < bands; ++k)
            {
                const float v = juce::jlimit (0.0f, 1.0f, amp[k]);
                auto bar = juce::Rectangle<float> (in.getX() + bw * (float) k, in.getBottom() - v * in.getHeight(), bw - 1.5f, v * in.getHeight());
                g.setColour (accent.withAlpha (0.3f + 0.65f * v)); g.fillRoundedRectangle (bar, 1.5f);
            }
        }
    };

    //==========================================================================
    // Hexwing: the algorithm as a routing diagram, six operator envelopes, the sideband spectrum, filter, scope (cyan)
    struct HexwingViz : public ShelfViz2
    {
        using ShelfViz2::ShelfViz2;
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            const int alg = juce::jlimit (0, 7, (int) std::lround (nv ("Algorithm")));
            auto left = r.removeFromLeft (r.getWidth() * 0.3f); r.removeFromLeft (8.0f);
            box (g, left, "ALGORITHM  " + SqSynth::algorithmNames()[alg]);
            fmAlgorithm (g, left.reduced (10.0f, 18.0f), alg);
            auto mid = r.removeFromLeft (r.getWidth() * 0.55f); r.removeFromLeft (8.0f);
            auto envs = mid.removeFromTop (mid.getHeight() * 0.6f); mid.removeFromTop (6.0f);
            box (g, envs, "OPERATOR ENVELOPES"); opEnvelopes (g, envs.reduced (6.0f, 14.0f).withTrimmedTop (2.0f), alg);
            box (g, mid, "SIDEBANDS  depth " + juce::String (nv ("Mod Depth", 1.0f), 2)); fmSpectrum (g, mid.reduced (8.0f, 14.0f), alg);
            auto flt = r.removeFromTop (r.getHeight() * 0.5f); r.removeFromTop (6.0f);
            box (g, flt, "FILTER  " + SqSynth::filterTypeNames()[juce::jlimit (0, 6, (int) std::lround (nv ("Filter Type")))]);
            { auto in = flt.reduced (8.0f, 14.0f); drawFreqGrid (g, in); filterCurve (g, in, (int) std::lround (nv ("Filter Type")), nv ("Cutoff", 20000.0f), nv ("Resonance"), accent); }
            box (g, r, "OUTPUT"); scope (g, r.reduced (8.0f, 14.0f), accent);
        }
    };

    //==========================================================================
    // Toxin FM: algorithm + filter on top, the 16-step note / on / accent / slide grid underneath (acid green)
    struct ToxinViz : public ShelfViz2
    {
        ToxinViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz2 (h, p, c)
        {
            auto* tox = dynamic_cast<SqSynth::SqToxinPlugin*> (&plugin);
            grid = std::make_unique<StepGrid> (h, c, std::vector<StepGrid::Lane> { { "Note", "Step %d Note", true }, { "On", "Step %d On" }, { "Accent", "Step %d Accent" }, { "Slide", "Step %d Slide" } },
                                               [tox] { return tox != nullptr && tox->sequencerOn() ? tox->currentStep() : -1; });
            addAndMakeVisible (*grid);
        }
        void resized() override
        {
            ShelfViz2::resized();
            auto r = getLocalBounds().reduced (6);
            grid->setBounds (r.removeFromBottom (r.getHeight() * 0.48f).reduced (8, 12).withTrimmedTop (6));
            if (matrix) matrix->setBounds (getLocalBounds().reduced (6).withTrimmedBottom (getHeight() * 0.48f + 6).removeFromRight (juce::jmin (300, getWidth() / 3)));
        }
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto seqArea = r.removeFromBottom (r.getHeight() * 0.48f); r.removeFromBottom (6.0f);
            auto* tox = dynamic_cast<SqSynth::SqToxinPlugin*> (&plugin);
            const bool seqOn = tox != nullptr && tox->sequencerOn();
            box (g, seqArea, juce::String ("SEQUENCER  ") + (seqOn ? "ON" : "off - hold a note and switch Sequencer on") + "   " + juce::StringArray { "1/8", "1/16", "1/32", "1/16T" }[juce::jlimit (0, 3, (int) std::lround (nv ("Seq Rate", 1)))]
                              + "   swing " + juce::String (juce::roundToInt (nv ("Seq Swing"))) + "%   gate " + juce::String (juce::roundToInt (nv ("Seq Gate", 0.5f) * 100)) + "%");
            if (matrix) r.removeFromRight ((float) matrix->getWidth() + 8.0f);
            const int alg = juce::jlimit (0, 7, (int) std::lround (nv ("Algorithm")));
            auto a = r.removeFromLeft (r.getWidth() * 0.34f); r.removeFromLeft (8.0f);
            box (g, a, "ALGORITHM  " + SqSynth::algorithmNames()[alg]); fmAlgorithm (g, a.reduced (10.0f, 18.0f), alg);
            auto e = r.removeFromLeft (r.getWidth() * 0.5f); r.removeFromLeft (8.0f);
            box (g, e, "OPERATORS"); opEnvelopes (g, e.reduced (6.0f, 14.0f).withTrimmedTop (2.0f), alg);
            box (g, r, "LADDER  " + juce::String (juce::roundToInt (nv ("Cutoff", 4000.0f))) + " Hz   drive " + juce::String (juce::roundToInt (nv ("Drive") * 100)) + "%");
            { auto in = r.reduced (8.0f, 14.0f); drawFreqGrid (g, in); filterCurve (g, in, (int) std::lround (nv ("Filter Type", 6)), nv ("Cutoff", 4000.0f), nv ("Resonance"), accent, nv ("Filter Env") * 2.0f * synth->displayLevel()); }
        }
        std::unique_ptr<StepGrid> grid;
    };

    //==========================================================================
    // Lantern: era plate, two drifting oscillators, the ladder, ensemble swirl, voice lamps (amber / warm)
    struct LanternViz : public ShelfViz2
    {
        using ShelfViz2::ShelfViz2;
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            const int era = juce::jlimit (0, 2, (int) std::lround (nv ("Era")));
            const float drift = nv ("Drift"), age = nv ("Age");
            auto eras = r.removeFromLeft (118.0f); r.removeFromLeft (8.0f);
            box (g, eras, "ERA");
            {
                auto in = eras.reduced (8.0f, 16.0f); auto names = SqSynth::SqLanternPlugin::eraNames();
                const float rh = in.getHeight() / 3.0f;
                for (int i = 0; i < 3; ++i)
                {
                    auto row = juce::Rectangle<float> (in.getX(), in.getY() + rh * (float) i, in.getWidth(), rh).reduced (0.0f, 3.0f);
                    g.setColour (i == era ? accent.withAlpha (0.25f) : juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (row, 4.0f);
                    auto lamp = row.removeFromLeft (row.getHeight()).reduced (6.0f);
                    g.setColour (i == era ? accent : Theme::dim().withAlpha (0.3f)); g.fillEllipse (lamp);
                    if (i == era) { g.setColour (accent.withAlpha (0.35f)); g.fillEllipse (lamp.expanded (4.0f)); }
                    g.setColour (i == era ? Theme::text() : Theme::dim()); g.setFont (Theme::font (9.0f, i == era));
                    g.drawFittedText (names[i], row.toNearestInt().reduced (2, 0), juce::Justification::centredLeft, 1);
                }
            }
            auto oscs = r.removeFromLeft (r.getWidth() * 0.42f); r.removeFromLeft (8.0f);
            box (g, oscs, "OSCILLATORS  drift " + juce::String (juce::roundToInt (drift * 100)) + "%   age " + juce::String (juce::roundToInt (age * 100)) + "%");
            {
                auto in = oscs.reduced (8.0f, 14.0f).withTrimmedTop (4.0f);
                auto row1 = in.removeFromTop (in.getHeight() * 0.33f).reduced (0.0f, 2.0f), row2 = in.removeFromTop (in.getHeight() * 0.5f).reduced (0.0f, 2.0f), row3 = in.reduced (0.0f, 2.0f);
                // drift wobbles the drawn waves slowly, the way the tuning wanders
                const float wob1 = std::sin (phase * 0.7f) * drift * 0.08f, wob2 = std::sin (phase * 0.9f + 1.3f) * drift * 0.08f;
                auto waveRow = [&] (juce::Rectangle<float> row, int w, float pw, float level, float wob, juce::Colour c, const juce::String& cap)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (row, 3.0f);
                    g.setFont (Theme::font (8.5f, true)); g.setColour (c.withAlpha (0.9f)); g.drawText (cap, row.toNearestInt().reduced (5, 0), juce::Justification::centredLeft);
                    if (level < 0.001f) { g.setColour (Theme::dim()); g.drawText ("off", row.toNearestInt(), juce::Justification::centred); return; }
                    auto wa = row.withTrimmedLeft (54.0f).reduced (4.0f, 3.0f); juce::Path p;
                    for (int i = 0; i <= (int) wa.getWidth(); ++i)
                    {
                        const float t = (float) i / wa.getWidth() * (2.0f + wob) + phase * 0.15f;
                        const float y = wa.getCentreY() - waveSample (w, t, pw) * level * wa.getHeight() * 0.45f * (1.0f - age * 0.15f);
                        if (i == 0) p.startNewSubPath (wa.getX(), y); else p.lineTo (wa.getX() + (float) i, y);
                    }
                    g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f));
                };
                waveRow (row1, (int) std::lround (nv ("Osc 1 Wave", 2)), nv ("Osc 1 Width", 0.5f), nv ("Osc 1 Level", 0.8f), wob1, accent, "OSC 1  " + juce::String ((int) std::lround (nv ("Osc 1 Octave"))) + " oct");
                waveRow (row2, (int) std::lround (nv ("Osc 2 Wave", 2)), nv ("Osc 2 Width", 0.5f), nv ("Osc 2 Level", 0.6f), wob2, Theme::amber(), "OSC 2  " + juce::String ((int) std::lround (nv ("Osc 2 Semi"))) + " st");
                waveRow (row3, 0, 0.5f, nv ("Sub", 0.2f), 0.0f, accent.withAlpha (0.7f), "SUB");
            }
            auto flt = r.removeFromTop (r.getHeight() * 0.5f); r.removeFromTop (6.0f);
            const bool ladder = nv ("Filter") < 0.5f;
            box (g, flt, juce::String (ladder ? "LADDER" : "LP 24") + "  " + juce::String (juce::roundToInt (nv ("Cutoff", 1600.0f))) + " Hz" + (nv ("Resonance") > 0.92f ? "   self-oscillating" : ""));
            { auto in = flt.reduced (8.0f, 14.0f); drawFreqGrid (g, in); filterCurve (g, in, ladder ? 6 : 0, nv ("Cutoff", 1600.0f), nv ("Resonance"), accent, nv ("Filter Env") * 2.0f * synth->displayLevel()); }
            auto ens = r.removeFromLeft (r.getWidth() * 0.6f); r.removeFromLeft (6.0f);
            box (g, ens, "ENSEMBLE  " + juce::String (juce::roundToInt (nv ("Ensemble") * 100)) + "%");
            {
                auto in = ens.reduced (8.0f, 14.0f); const float amt = nv ("Ensemble"), rate = nv ("Ens Rate", 0.6f);
                for (int line = 0; line < 3; ++line)
                {
                    juce::Path p;
                    for (int i = 0; i <= (int) in.getWidth(); ++i)
                    {
                        const float t = (float) i / in.getWidth();
                        const float y = in.getCentreY() + std::sin (t * 6.28f * 1.5f + phase * rate * 2.0f + (float) line * 2.09f) * amt * in.getHeight() * 0.35f * (0.5f + 0.5f * std::sin (t * 3.14f));
                        if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
                    }
                    g.setColour ((line == 0 ? accent : Theme::amber()).withAlpha (0.35f + 0.25f * (float) line)); g.strokePath (p, juce::PathStrokeType (1.4f));
                }
            }
            const bool vintage = nv ("Vintage Mode", 1) > 0.5f;
            box (g, r, vintage ? "VINTAGE  8 voices" : "VOICES  16");
            {
                auto in = r.reduced (8.0f, 16.0f); const int n = vintage ? 8 : 16, active = synth->activeVoiceCount();
                const int cols = vintage ? 4 : 8; const float cw = in.getWidth() / (float) cols, ch = in.getHeight() / (float) (n / cols);
                for (int i = 0; i < n; ++i)
                {
                    auto lamp = juce::Rectangle<float> (in.getX() + cw * (float) (i % cols), in.getY() + ch * (float) (i / cols), cw, ch).reduced (3.0f);
                    const float d = juce::jmin (lamp.getWidth(), lamp.getHeight()); lamp = lamp.withSizeKeepingCentre (d, d);
                    g.setColour (i < active ? accent : Theme::dim().withAlpha (0.25f)); g.fillEllipse (lamp);
                }
            }
        }
    };

    //==========================================================================
    // Lantern Exo: the wavetable stack with the morph position, both oscillators, dual filters, three envelopes (ice blue)
    struct LanternExoViz : public ShelfViz2
    {
        using ShelfViz2::ShelfViz2;
        void tableWave (juce::Graphics& g, juce::Rectangle<float> in, float morph, juce::Colour c, float thickness, float alpha)
        {
            const auto& bank = SqSynth::WaveTableBank::get(); juce::Path p;
            for (int i = 0; i <= (int) in.getWidth(); ++i)
            {
                const double ph = (double) i / (double) in.getWidth();
                const float y = in.getCentreY() - bank.readMorph (morph, ph, 1.0 / 2048.0) * in.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
            }
            g.setColour (c.withAlpha (alpha)); g.strokePath (p, juce::PathStrokeType (thickness));
        }
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto stack = r.removeFromLeft (r.getWidth() * 0.34f); r.removeFromLeft (8.0f);
            const int w1 = (int) std::lround (nv ("Osc 1 Wave", 6)), w2 = (int) std::lround (nv ("Osc 2 Wave", 6));
            const float m1 = nv ("Osc 1 Morph", 0.3f), m2 = nv ("Osc 2 Morph", 0.3f);
            auto names = SqSynth::WaveTableBank::names();
            box (g, stack, "WAVETABLE STACK  " + (w1 == 6 ? names[juce::jlimit (0, 7, (int) std::lround (m1 * 7.0f))] : SqSynth::waveNames()[juce::jlimit (0, 6, w1)]));
            {
                auto in = stack.reduced (8.0f, 16.0f);
                const float rowH = in.getHeight() / 8.0f;
                for (int t = 0; t < 8; ++t)
                {
                    auto row = juce::Rectangle<float> (in.getX(), in.getY() + rowH * (float) (7 - t), in.getWidth(), rowH).reduced (0.0f, 1.0f);
                    const float near1 = w1 == 6 ? juce::jmax (0.0f, 1.0f - std::abs (m1 * 7.0f - (float) t)) : 0.0f, near2 = w2 == 6 ? juce::jmax (0.0f, 1.0f - std::abs (m2 * 7.0f - (float) t)) : 0.0f;
                    g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (row, 2.0f);
                    g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f)); g.drawText (names[t], row.toNearestInt().reduced (4, 0), juce::Justification::centredLeft);
                    tableWave (g, row.withTrimmedLeft (44.0f).reduced (2.0f), (float) t / 7.0f, accent.interpolatedWith (Theme::amber(), near2 > near1 ? 1.0f : 0.0f), 1.0f + 1.2f * juce::jmax (near1, near2), 0.3f + 0.7f * juce::jmax (near1, near2));
                }
                // morph cursors
                auto cursor = [&] (float m, juce::Colour c) { const float y = in.getBottom() - in.getHeight() * (m * 7.0f + 0.5f) / 8.0f; g.setColour (c.withAlpha (0.9f)); g.fillEllipse (in.getX() + 36.0f, y - 3.0f, 6.0f, 6.0f); };
                if (w1 == 6) cursor (m1, accent); if (w2 == 6) cursor (m2, Theme::amber());
            }
            auto mid = r.removeFromLeft (r.getWidth() * 0.5f); r.removeFromLeft (8.0f);
            auto o = mid.removeFromTop (mid.getHeight() * 0.5f); mid.removeFromTop (6.0f);
            box (g, o, "OSCILLATORS");
            {
                auto in = o.reduced (8.0f, 14.0f).withTrimmedTop (4.0f);
                auto r1 = in.removeFromTop (in.getHeight() * 0.5f).reduced (0.0f, 2.0f), r2 = in.reduced (0.0f, 2.0f);
                auto osc = [&] (juce::Rectangle<float> row, int w, float morph, float lvl, float pw, juce::Colour c, const juce::String& cap)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (row, 3.0f);
                    g.setFont (Theme::font (8.5f, true)); g.setColour (c.withAlpha (0.9f)); g.drawText (cap, row.toNearestInt().reduced (5, 0), juce::Justification::centredLeft);
                    auto wa = row.withTrimmedLeft (64.0f).reduced (4.0f, 3.0f);
                    if (lvl < 0.001f) { g.setColour (Theme::dim()); g.drawText ("off", wa.toNearestInt(), juce::Justification::centred); return; }
                    if (w == 6) tableWave (g, wa, morph, c, 1.6f, 1.0f);
                    else { juce::Path p; for (int i = 0; i <= (int) wa.getWidth(); ++i) { const float t = (float) i / wa.getWidth() * 2.0f + phase * 0.15f; const float y = wa.getCentreY() - waveSample (w, t, pw) * lvl * wa.getHeight() * 0.45f; if (i == 0) p.startNewSubPath (wa.getX(), y); else p.lineTo (wa.getX() + (float) i, y); } g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f)); }
                };
                osc (r1, w1, m1, nv ("Osc 1 Level", 0.8f), nv ("Osc 1 Width", 0.5f), accent, "OSC 1  " + juce::String ((int) std::lround (nv ("Osc 1 Semi"))) + " st");
                osc (r2, w2, m2, nv ("Osc 2 Level", 0.6f), nv ("Osc 2 Width", 0.5f), Theme::amber(), "OSC 2  " + juce::String ((int) std::lround (nv ("Osc 2 Semi"))) + " st");
            }
            box (g, mid, "FILTERS  " + juce::StringArray { "F1", "SERIES", "PARALLEL" }[juce::jlimit (0, 2, (int) std::lround (nv ("Routing")))]);
            {
                auto in = mid.reduced (8.0f, 14.0f); drawFreqGrid (g, in);
                filterCurve (g, in, (int) std::lround (nv ("F1 Type")), nv ("Cutoff", 2000.0f), nv ("Resonance"), accent, nv ("Filter Env") * 2.0f * synth->displayLevel());
                if (nv ("Routing") > 0.5f) filterCurve (g, in, (int) std::lround (nv ("F2 Type")), nv ("F2 Cutoff", 2000.0f), nv ("F2 Res"), Theme::amber().withAlpha (0.8f), nv ("F2 Env") * 2.0f * synth->displayLevel());
            }
            auto e1 = r.removeFromTop (r.getHeight() / 3.0f).reduced (0.0f, 2.0f), e2 = r.removeFromTop (r.getHeight() * 0.5f).reduced (0.0f, 2.0f), e3 = r.reduced (0.0f, 2.0f);
            box (g, e1, "AMP"); envelope (g, e1.reduced (6.0f, 13.0f), nv ("Amp Attack"), nv ("Amp Decay"), nv ("Amp Sustain"), nv ("Amp Release"), Theme::amber());
            box (g, e2, "FILTER"); envelope (g, e2.reduced (6.0f, 13.0f), nv ("Flt Attack"), nv ("Flt Decay"), nv ("Flt Sustain"), nv ("Flt Release"), accent);
            box (g, e3, "MOD"); envelope (g, e3.reduced (6.0f, 13.0f), nv ("Mod Attack"), nv ("Mod Decay"), nv ("Mod Sustain"), nv ("Mod Release"), accent.withAlpha (0.7f));
        }
    };

    //==========================================================================
    // Larva Bass: the diode ladder with the env-mod sweep, accent / slide, drive, and the 16-step grid (acid yellow)
    struct LarvaViz : public ShelfViz2
    {
        LarvaViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz2 (h, p, c, false)
        {
            auto* lb = dynamic_cast<SqSynth::SqLarvaBassPlugin*> (&plugin);
            grid = std::make_unique<StepGrid> (h, c, std::vector<StepGrid::Lane> { { "Note", "Step %d Note", true }, { "On", "Step %d On" }, { "Accent", "Step %d Accent" }, { "Slide", "Step %d Slide" } },
                                               [lb] { return lb != nullptr && lb->sequencerOn() ? lb->currentStep() : -1; });
            addAndMakeVisible (*grid);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            grid->setBounds (r.removeFromRight (r.getWidth() * 0.56f).reduced (8, 12).withTrimmedTop (6));
        }
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto seqArea = r.removeFromRight (r.getWidth() * 0.56f); r.removeFromRight (8.0f);
            auto* lb = dynamic_cast<SqSynth::SqLarvaBassPlugin*> (&plugin);
            const bool seqOn = lb != nullptr && lb->sequencerOn();
            box (g, seqArea, juce::String ("SEQUENCER  ") + (seqOn ? "ON" : "off") + "   " + juce::StringArray { "1/8", "1/16", "1/32", "1/16T" }[juce::jlimit (0, 3, (int) std::lround (nv ("Seq Rate", 1)))] + "   octave " + juce::String ((int) std::lround (nv ("Seq Octave"))) + "   swing " + juce::String (juce::roundToInt (nv ("Seq Swing"))) + "%");
            auto flt = r.removeFromTop (r.getHeight() * 0.55f); r.removeFromTop (6.0f);
            const float cut = nv ("Cutoff", 520.0f), env = nv ("Env Mod", 0.55f), acc = nv ("Accent", 0.6f);
            box (g, flt, juce::String (nv ("Wave") < 0.5f ? "SAW" : "SQUARE") + "  >  DIODE LADDER  " + juce::String (juce::roundToInt (cut)) + " Hz   env " + juce::String (juce::roundToInt (env * 100)) + "%");
            {
                auto in = flt.reduced (8.0f, 14.0f); drawFreqGrid (g, in);
                // the sweep: resting curve, the env-mod peak, and the accent peak
                filterCurve (g, in, 6, cut, nv ("Resonance", 0.65f), accent.withAlpha (0.35f));
                filterCurve (g, in, 6, cut * std::pow (2.0f, env * 3.0f), nv ("Resonance", 0.65f), accent.withAlpha (0.7f));
                filterCurve (g, in, 6, cut * std::pow (2.0f, (env + acc * 0.6f) * 3.0f), juce::jmin (1.0f, nv ("Resonance", 0.65f) + acc * 0.2f), Theme::amber().withAlpha (0.8f));
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f)); g.drawText ("rest  >  env  >  accent", in.toNearestInt(), juce::Justification::bottomLeft);
            }
            auto drv = r.removeFromLeft (r.getWidth() * 0.5f).reduced (2.0f, 0.0f), out = r.reduced (2.0f, 0.0f);
            box (g, drv, "DRIVE  " + SqSynth::driveTypeNames()[juce::jlimit (0, 3, (int) std::lround (nv ("Drive Type", 1)))]);
            {
                auto in = drv.reduced (10.0f, 14.0f); juce::Path c;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float x = (float) i / in.getWidth() * 2.0f - 1.0f, y = SqSynth::driveShape (x, nv ("Drive"), (int) std::lround (nv ("Drive Type", 1)));
                    const float px = in.getX() + (float) i, py = in.getCentreY() - y * in.getHeight() * 0.48f;
                    if (i == 0) c.startNewSubPath (px, py); else c.lineTo (px, py);
                }
                g.setColour (Theme::text().withAlpha (0.12f)); g.drawLine (in.getX(), in.getCentreY(), in.getRight(), in.getCentreY());
                g.setColour (accent); g.strokePath (c, juce::PathStrokeType (2.0f));
            }
            box (g, out, "OUTPUT"); scope (g, out.reduced (8.0f, 14.0f), accent);
        }
        std::unique_ptr<StepGrid> grid;
    };

    //==========================================================================
    // Thorn Bass: the string between nut and bridge with pick and pickups, the amp, round-robin picks (wood / brass)
    struct ThornViz : public ShelfViz2
    {
        ThornViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz2 (h, p, c, false) {}
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = bodyArea();
            auto str = r.removeFromTop (r.getHeight() * 0.56f); r.removeFromTop (6.0f);
            const float pick = nv ("Pick Position", 0.2f), pickup = nv ("Pickup Position", 0.3f), blend = nv ("Neck / Bridge", 0.5f), mute = nv ("Palm Mute"), slap = nv ("Slap"), decay = nv ("Decay", 0.7f);
            box (g, str, "STRING  pick " + juce::String (juce::roundToInt (pick * 100)) + "%   pickups neck / bridge " + juce::String (juce::roundToInt ((1.0f - blend) * 100)) + " / " + juce::String (juce::roundToInt (blend * 100)) + (mute > 0.05f ? "   palm mute" : "") + (slap > 0.05f ? "   slap" : ""));
            {
                auto in = str.reduced (14.0f, 18.0f);
                const float sy = in.getY() + in.getHeight() * 0.45f;
                // body (right) and fretboard (left) behind the string; the 12th fret sits at half the scale length
                { auto body = juce::Rectangle<float> (in.getX() + in.getWidth() * 0.5f, sy - in.getHeight() * 0.42f, in.getWidth() * 0.5f + 6.0f, in.getHeight() * 0.84f);
                  g.setColour (accent.withAlpha (0.10f)); g.fillRoundedRectangle (body, in.getHeight() * 0.3f);
                  auto neck = juce::Rectangle<float> (in.getX(), sy - 9.0f, in.getWidth() * 0.52f, 18.0f);
                  g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (neck, 3.0f);
                  for (int f = 1; f <= 12; ++f) { const float x = in.getX() + in.getWidth() * (1.0f - std::pow (0.5f, (float) f / 12.0f)); g.setColour (Theme::dim().withAlpha (0.55f)); g.drawVerticalLine ((int) x, neck.getY(), neck.getBottom()); if (f == 3 || f == 5 || f == 7 || f == 9) { g.setColour (Theme::dim().withAlpha (0.5f)); g.fillEllipse (x - 7.0f, sy + 12.0f, 4.0f, 4.0f); } } }
                // the string: standing wave shaped by the pick position, decaying with the level
                const float lvl = synth->displayLevel(), amp = juce::jmin (1.0f, lvl * 2.0f) * (1.0f - mute * 0.7f);
                juce::Path s;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float x = (float) i / in.getWidth();   // 0 = nut, 1 = bridge
                    float y = 0.0f;
                    for (int n = 1; n <= 6; ++n) y += std::sin ((float) n * 3.14159f * pick) / (float) (n * n) * std::sin ((float) n * 3.14159f * x) * std::cos (phase * (float) n * 3.0f + (float) n);
                    const float py = sy - y * amp * in.getHeight() * 0.4f;
                    if (i == 0) s.startNewSubPath (in.getX(), py); else s.lineTo (in.getX() + (float) i, py);
                }
                g.setColour (Theme::text().withAlpha (0.25f)); g.drawLine (in.getX(), sy, in.getRight(), sy, 1.0f);
                g.setColour (accent); g.strokePath (s, juce::PathStrokeType (2.0f + (1.0f - decay) * 1.0f));
                // nut and bridge
                g.setColour (Theme::text().withAlpha (0.7f)); g.fillRect (in.getX() - 3.0f, sy - 14.0f, 4.0f, 28.0f); g.fillRect (in.getRight() - 1.0f, sy - 12.0f, 5.0f, 24.0f);
                // pick
                { const float px = in.getX() + in.getWidth() * (1.0f - pick); juce::Path pk; pk.addTriangle (px - 6.0f, sy - 26.0f, px + 6.0f, sy - 26.0f, px, sy - 8.0f); g.setColour (Theme::amber()); g.fillPath (pk); g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f)); g.drawText ("PICK", (int) px - 20, (int) (sy - 40.0f), 40, 12, juce::Justification::centred); }
                // pickups: neck at 1 - pickupPos*2, bridge near the bridge; alpha = blend
                auto pu = [&] (float x01, float a, const juce::String& cap) { const float px = in.getX() + in.getWidth() * x01; auto pr = juce::Rectangle<float> (px - 10.0f, sy + 10.0f, 20.0f, 16.0f); g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (pr, 3.0f); g.setColour (accent.withAlpha (0.25f + 0.7f * a)); g.fillRoundedRectangle (pr.reduced (2.0f), 2.0f); g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f)); g.drawText (cap, (int) px - 20, (int) pr.getBottom() + 1, 40, 12, juce::Justification::centred); };
                pu (1.0f - pickup * 2.0f, 1.0f - blend, "NECK"); pu (1.0f - pickup * 0.6f, blend, "BRIDGE");
            }
            auto amp = r.removeFromLeft (r.getWidth() * 0.4f); r.removeFromLeft (6.0f);
            box (g, amp, "AMP  drive " + juce::String (juce::roundToInt (nv ("Amp Drive") * 100)) + "%   tone " + juce::String (juce::roundToInt (nv ("Amp Tone", 3500.0f))) + " Hz");
            {
                auto in = amp.reduced (10.0f, 14.0f); juce::Path c;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float x = (float) i / in.getWidth() * 2.0f - 1.0f, y = SqSynth::driveShape (x, nv ("Amp Drive"), 1);
                    const float px = in.getX() + (float) i, py = in.getCentreY() - y * in.getHeight() * 0.48f;
                    if (i == 0) c.startNewSubPath (px, py); else c.lineTo (px, py);
                }
                g.setColour (Theme::text().withAlpha (0.12f)); g.drawLine (in.getX(), in.getCentreY(), in.getRight(), in.getCentreY());
                g.setColour (Theme::amber()); g.strokePath (c, juce::PathStrokeType (2.0f));
            }
            auto rr = r.removeFromLeft (r.getWidth() * 0.4f); r.removeFromLeft (6.0f);
            box (g, rr, "ROUND ROBIN  " + juce::String (juce::roundToInt (nv ("Round Robin", 0.5f) * 100)) + "%");
            {
                auto in = rr.reduced (10.0f, 16.0f); const float v = nv ("Round Robin", 0.5f);
                for (int i = 0; i < 5; ++i)
                {
                    const float d = in.getHeight() * 0.5f;
                    auto dot = juce::Rectangle<float> (in.getX() + (in.getWidth() - d) * (float) i / 4.0f, in.getCentreY() - d * 0.5f, d, d).reduced (3.0f);
                    const float var = v * std::sin ((float) i * 1.7f + phase * 0.3f);
                    g.setColour (accent.withAlpha (0.4f + 0.5f * (0.5f + 0.5f * var))); g.fillEllipse (dot.reduced (3.0f * (1.0f - var)));
                }
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f)); g.drawText ("each pick differs a little", in.toNearestInt(), juce::Justification::bottomLeft);
            }
            box (g, r, "OUTPUT"); scope (g, r.reduced (8.0f, 14.0f), accent);
        }
    };

    //==========================================================================
    // Carapace: 16 pads (click to play / select), the selected sound's twelve controls, its model sketch (orange-red)
    struct CarapaceViz : public ShelfViz2
    {
        CarapaceViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz2 (h, p, c, false)
        {
            car = dynamic_cast<SqSynth::SqCarapacePlugin*> (&plugin);
            pads = std::make_unique<PadGrid> (SqSynth::SqCarapacePlugin::soundNames(), 4, c);
            pads->modelNames = SqSynth::drumModelNames();
            pads->modelOf = [this] (int i) { return (int) std::lround (nv (SqSynth::SqCarapacePlugin::soundNames()[i] + " Model")); };
            pads->lastHit = [this] { return car != nullptr ? car->lastHitSound() : -1; };
            pads->onHit = [this] (int i) { if (car != nullptr) car->triggerSound (i, 1.0f); };
            pads->onSelect = [this] (int i) { select (i); };
            strip = std::make_unique<HandleStrip> (h, c); strip->rowHeight = 30; strip->onChange = [this] { repaint(); };
            addAndMakeVisible (*pads); addAndMakeVisible (*strip);
            select (0);
        }
        void select (int i)
        {
            selected = i; const auto nm = SqSynth::SqCarapacePlugin::soundNames()[i];
            juce::StringArray names, labels;
            for (auto* f : { "Model", "Pitch", "Pitch Env", "Pitch Time", "Decay", "Tone", "Noise", "Snap", "Drive", "Level", "Pan", "Choke" }) { names.add (nm + " " + f); labels.add (f); }
            strip->setRows (names, labels); repaint();
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            pads->setBounds (r.removeFromLeft (r.getWidth() * 0.42f).reduced (8, 12).withTrimmedTop (6));
            r.removeFromLeft (8);
            auto ed = r.removeFromLeft (r.getWidth() * 0.5f);
            strip->setBounds (ed.reduced (8, 12).withTrimmedTop (6));
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            pads->tick();
            auto padArea = r.removeFromLeft (r.getWidth() * 0.42f); r.removeFromLeft (8.0f);
            box (g, padArea, "PADS  C1 - D#2   click to play, select to edit");
            auto ed = r.removeFromLeft (r.getWidth() * 0.5f); r.removeFromLeft (8.0f);
            const auto nm = SqSynth::SqCarapacePlugin::soundNames()[selected];
            const int model = juce::jlimit (0, 4, (int) std::lround (nv (nm + " Model")));
            box (g, ed, nm.toUpperCase() + "  -  " + SqSynth::drumModelNames()[model]);
            auto sketch = r.removeFromTop (r.getHeight() * 0.55f); r.removeFromTop (6.0f);
            box (g, sketch, "MODEL");
            {
                auto in = sketch.reduced (8.0f, 16.0f);
                const float decay = juce::jmax (0.02f, nv (nm + " Decay", 0.3f)), pEnv = nv (nm + " Pitch Env"), pTime = juce::jmax (0.002f, nv (nm + " Pitch Time", 0.04f)), noise = nv (nm + " Noise"), snap = nv (nm + " Snap"), tone = nv (nm + " Tone", 0.5f);
                juce::Random rnd (7); juce::Path p; const float span = juce::jmin (1.2f, decay * 1.3f);
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth() * span, env = std::exp (-4.6f * t / decay);
                    float v = 0.0f;
                    switch (model)
                    {
                        case 1: v = ((rnd.nextFloat() * 2.0f - 1.0f) * 0.5f + std::sin (t * 900.0f * (1.0f + tone)) * 0.5f) * env; break;
                        case 2: v = (rnd.nextFloat() * 2.0f - 1.0f) * (0.4f + 0.6f * noise) * env + std::sin (t * 300.0f) * (1.0f - noise) * env * 0.6f; break;
                        case 3: v = std::sin (t * 1400.0f * (0.6f + tone)) * std::exp (-t * 30.0f / juce::jmax (0.05f, decay * 3.0f)); break;
                        default: { const float f = 60.0f * std::pow (2.0f, pEnv / 12.0f * std::exp (-t / pTime)); v = std::sin (t * f * 3.0f) * env; break; }
                    }
                    v += (rnd.nextFloat() * 2.0f - 1.0f) * snap * std::exp (-t * 160.0f);
                    const float y = in.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * in.getHeight() * 0.45f;
                    if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
                }
                g.setColour (accent); g.strokePath (p, juce::PathStrokeType (1.4f));
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f)); g.drawText (juce::String (span, 2) + " s", in.toNearestInt(), juce::Justification::bottomRight);
            }
            box (g, r, "OUTPUT"); scope (g, r.reduced (8.0f, 14.0f), accent);
        }
        SqSynth::SqCarapacePlugin* car = nullptr; std::unique_ptr<PadGrid> pads; std::unique_ptr<HandleStrip> strip; int selected = 0;
    };

    //==========================================================================
    // Live Drum Synth: twelve round kit pieces, the selected piece's two oscillators + noise (yellow)
    struct LiveDrumViz : public ShelfViz2
    {
        LiveDrumViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz2 (h, p, c, false)
        {
            ld = dynamic_cast<SqSynth::SqLiveDrumSynthPlugin*> (&plugin);
            pads = std::make_unique<PadGrid> (SqSynth::SqLiveDrumSynthPlugin::soundNames(), 4, c); pads->round = true;
            pads->lastHit = [this] { return ld != nullptr ? ld->lastHitSound() : -1; };
            pads->onHit = [this] (int i) { if (ld != nullptr) ld->triggerSound (i, 1.0f); };
            pads->onSelect = [this] (int i) { select (i); };
            strip = std::make_unique<HandleStrip> (h, c); strip->rowHeight = 26; strip->onChange = [this] { repaint(); };
            addAndMakeVisible (*pads); addAndMakeVisible (*strip);
            select (0);
        }
        void select (int i)
        {
            selected = i; const auto nm = SqSynth::SqLiveDrumSynthPlugin::soundNames()[i];
            juce::StringArray names, labels;
            for (auto* f : { "Osc 1", "Osc 1 Pitch", "Pitch Env", "Pitch Time", "Osc 1 Decay", "Osc 2", "Osc 2 Pitch", "Osc 2 Level", "Osc 2 Decay", "Noise", "Noise Colour", "Noise Decay", "Level", "Pan" }) { names.add (nm + " " + f); labels.add (f); }
            strip->setRows (names, labels); repaint();
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            pads->setBounds (r.removeFromLeft (r.getWidth() * 0.4f).reduced (8, 12).withTrimmedTop (6));
            r.removeFromLeft (8);
            strip->setBounds (r.removeFromLeft (r.getWidth() * 0.5f).reduced (8, 12).withTrimmedTop (6));
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            pads->tick();
            auto padArea = r.removeFromLeft (r.getWidth() * 0.4f); r.removeFromLeft (8.0f);
            box (g, padArea, "KIT  C1 - B1   click to play, select to edit");
            auto ed = r.removeFromLeft (r.getWidth() * 0.5f); r.removeFromLeft (8.0f);
            const auto nm = SqSynth::SqLiveDrumSynthPlugin::soundNames()[selected];
            box (g, ed, nm.toUpperCase());
            auto sk = r.removeFromTop (r.getHeight() * 0.55f); r.removeFromTop (6.0f);
            box (g, sk, "OSC 1 + OSC 2 + NOISE");
            {
                auto in = sk.reduced (8.0f, 16.0f);
                const float p1 = nv (nm + " Osc 1 Pitch", 60.0f), pe = nv (nm + " Pitch Env"), pt = juce::jmax (0.002f, nv (nm + " Pitch Time", 0.03f)), d1 = juce::jmax (0.02f, nv (nm + " Osc 1 Decay", 0.3f));
                const float p2 = nv (nm + " Osc 2 Pitch", 180.0f), l2 = nv (nm + " Osc 2 Level", 0.3f), d2 = juce::jmax (0.01f, nv (nm + " Osc 2 Decay", 0.1f));
                const float nz = nv (nm + " Noise", 0.3f), dn = juce::jmax (0.01f, nv (nm + " Noise Decay", 0.15f));
                const int w1 = (int) std::lround (nv (nm + " Osc 1")), w2 = (int) std::lround (nv (nm + " Osc 2"));
                const float span = juce::jmin (1.0f, juce::jmax (d1, d2, dn) * 1.2f);
                juce::Random rnd (3); juce::Path a, b, c;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth() * span;
                    const float f1 = p1 * std::pow (2.0f, pe / 12.0f * std::exp (-t / pt));
                    const float v1 = waveSample (w1 == 4 ? 5 : w1, t * f1 * 0.08f, 0.5f) * std::exp (-4.6f * t / d1);
                    const float v2 = waveSample (w2 == 4 ? 5 : w2, t * p2 * 0.08f, 0.5f) * (0.3f + 0.7f * l2) * std::exp (-4.6f * t / d2);
                    const float v3 = (rnd.nextFloat() * 2.0f - 1.0f) * (0.3f + 0.7f * nz) * std::exp (-4.6f * t / dn);
                    const float x = in.getX() + (float) i;
                    const float y1 = in.getY() + in.getHeight() * 0.18f - v1 * in.getHeight() * 0.16f, y2 = in.getY() + in.getHeight() * 0.5f - v2 * in.getHeight() * 0.16f, y3 = in.getY() + in.getHeight() * 0.82f - v3 * in.getHeight() * 0.16f;
                    if (i == 0) { a.startNewSubPath (x, y1); b.startNewSubPath (x, y2); c.startNewSubPath (x, y3); } else { a.lineTo (x, y1); b.lineTo (x, y2); c.lineTo (x, y3); }
                }
                g.setColour (accent); g.strokePath (a, juce::PathStrokeType (1.4f));
                g.setColour (Theme::amber()); g.strokePath (b, juce::PathStrokeType (1.2f));
                g.setColour (Theme::text().withAlpha (0.6f)); g.strokePath (c, juce::PathStrokeType (1.0f));
                g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f));
                g.drawText (SqSynth::drumOscNames()[juce::jlimit (0, 4, w1)] + " " + juce::String (juce::roundToInt (p1)) + " Hz", in.toNearestInt(), juce::Justification::topLeft);
                g.drawText (SqSynth::drumOscNames()[juce::jlimit (0, 4, w2)] + " " + juce::String (juce::roundToInt (p2)) + " Hz  " + juce::String (juce::roundToInt (l2 * 100)) + "%", in.toNearestInt().withTrimmedTop ((int) (in.getHeight() * 0.33f)), juce::Justification::topLeft);
                g.drawText ("noise  " + juce::String (juce::roundToInt (nz * 100)) + "%", in.toNearestInt().withTrimmedTop ((int) (in.getHeight() * 0.66f)), juce::Justification::topLeft);
            }
            box (g, r, "OUTPUT"); scope (g, r.reduced (8.0f, 14.0f), accent);
        }
        SqSynth::SqLiveDrumSynthPlugin* ld = nullptr; std::unique_ptr<PadGrid> pads; std::unique_ptr<HandleStrip> strip; int selected = 0;
    };

    //==========================================================================
    // Drum Pad Model: one big pad, the model's response and the key-tracked pitch span (mint)
    struct DrumPadViz : public ShelfViz2
    {
        DrumPadViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz2 (h, p, c, false)
        {
            dp = dynamic_cast<SqSynth::SqDrumPadModelPlugin*> (&plugin);
            pads = std::make_unique<PadGrid> (juce::StringArray { "HIT" }, 1, c);
            pads->lastHit = [this] { return dp != nullptr && dp->activeVoiceCount() > 0 ? 0 : -1; };
            pads->onHit = [this] (int) { if (dp != nullptr) dp->triggerSound (0, 1.0f); };
            addAndMakeVisible (*pads);
        }
        void resized() override   { auto r = getLocalBounds().reduced (6); pads->setBounds (r.removeFromLeft (juce::jmin (r.getHeight(), r.getWidth() / 3)).reduced (10, 14).withTrimmedTop (6)); }
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            pads->tick();
            auto padArea = r.removeFromLeft (juce::jmin (r.getHeight(), r.getWidth() / 3.0f)); r.removeFromLeft (8.0f);
            const int model = juce::jlimit (0, 4, (int) std::lround (nv ("Model")));
            box (g, padArea, SqSynth::drumModelNames()[model]);
            auto resp = r.removeFromTop (r.getHeight() * 0.6f); r.removeFromTop (6.0f);
            box (g, resp, "RESPONSE  pitch " + juce::MidiMessage::getMidiNoteName ((int) std::lround (nv ("Pitch", 48)), true, true, 3) + "   decay " + juce::String (nv ("Decay", 0.4f), 2) + " s");
            {
                auto in = resp.reduced (8.0f, 16.0f);
                const float decay = juce::jmax (0.02f, nv ("Decay", 0.4f)), pEnv = nv ("Pitch Env"), pTime = juce::jmax (0.002f, nv ("Pitch Time", 0.04f)), tone = nv ("Tone", 0.5f), noise = nv ("Noise"), snap = nv ("Snap");
                juce::Random rnd (5); juce::Path p, env; const float span = juce::jmin (2.0f, decay * 1.3f);
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth() * span, e = std::exp (-4.6f * t / decay);
                    float v;
                    switch (model) { case 1: v = ((rnd.nextFloat() * 2.0f - 1.0f) * 0.5f + std::sin (t * 900.0f * (1.0f + tone)) * 0.5f) * e; break; case 2: v = (rnd.nextFloat() * 2.0f - 1.0f) * e; break; case 3: v = std::sin (t * 1400.0f * (0.6f + tone)) * std::exp (-t * 30.0f / juce::jmax (0.05f, decay * 3.0f)); break; default: v = std::sin (t * 60.0f * std::pow (2.0f, pEnv / 12.0f * std::exp (-t / pTime)) * 3.0f) * e; }
                    v += (rnd.nextFloat() * 2.0f - 1.0f) * (snap * std::exp (-t * 160.0f) + noise * 0.3f * e);
                    const float x = in.getX() + (float) i, y = in.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * in.getHeight() * 0.45f;
                    if (i == 0) { p.startNewSubPath (x, y); env.startNewSubPath (x, in.getBottom() - e * in.getHeight()); } else { p.lineTo (x, y); env.lineTo (x, in.getBottom() - e * in.getHeight()); }
                }
                g.setColour (accent.withAlpha (0.15f)); juce::Path f (env); f.lineTo (in.getRight(), in.getBottom()); f.lineTo (in.getX(), in.getBottom()); f.closeSubPath(); g.fillPath (f);
                g.setColour (accent); g.strokePath (p, juce::PathStrokeType (1.4f));
                drawTimeAxis (g, in.withHeight (in.getHeight() + 12.0f), span);   // phase Z: real time under the hit
            }
            box (g, r, "KEY TRACK  " + juce::String (juce::roundToInt (nv ("Key Track", 1.0f) * 100)) + "%   C3 plays the set pitch");
            {
                auto in = r.reduced (8.0f, 16.0f); const float kt = nv ("Key Track", 1.0f);
                const int lo = 36, hi = 84; int whites = 0; for (int n = lo; n <= hi; ++n) if (! juce::MidiMessage::isMidiNoteBlack (n)) ++whites;
                const float kw = in.getWidth() / (float) whites;
                auto keyX = [&] (int n) { int w = 0; for (int k = lo; k < n; ++k) if (! juce::MidiMessage::isMidiNoteBlack (k)) ++w; return in.getX() + kw * (float) w; };
                const int played = synth->lastNote();
                for (int pass = 0; pass < 2; ++pass)
                    for (int n = lo; n <= hi; ++n)
                    {
                        const bool black = juce::MidiMessage::isMidiNoteBlack (n); if ((pass == 1) != black) continue;
                        auto key = black ? juce::Rectangle<float> (keyX (n) - kw * 0.3f, in.getY(), kw * 0.6f, in.getHeight() * 0.6f) : juce::Rectangle<float> (keyX (n), in.getY(), kw, in.getHeight()).reduced (0.5f, 0.0f);
                        const float semis = (float) (n - 60) * kt;   // how far this key shifts the sound
                        const float shade = 0.25f + 0.55f * juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (semis) / 30.0f);
                        g.setColour (n == 60 ? accent : black ? juce::Colours::black.withAlpha (0.85f) : Theme::text().withAlpha (shade)); g.fillRect (key);
                        if (played == n && synth->displayLevel() > 0.02f) { g.setColour (accent.withAlpha (0.9f)); g.fillRect (key); }
                    }
                g.setColour (Theme::dim()); g.setFont (Theme::font (7.5f)); g.drawText ("C1", in.toNearestInt().translated (0, 2), juce::Justification::bottomLeft); g.drawText ("C5", in.toNearestInt().translated (0, 2), juce::Justification::bottomRight);
            }
        }
        SqSynth::SqDrumPadModelPlugin* dp = nullptr; std::unique_ptr<PadGrid> pads;
    };

    //==========================================================================
    // Thump Live: three drawable envelopes (pitch, amp, distortion), the 16-step kick pattern, scope (red-pink)
    struct ThumpLiveViz : public ShelfViz2
    {
        ThumpLiveViz (HandleMap& h, te::Plugin& p, juce::Colour c) : ShelfViz2 (h, p, c, false)
        {
            tl = dynamic_cast<SqSynth::SqThumpLivePlugin*> (&plugin);
            envs[0] = std::make_unique<EnvEditor4> (h, c, "PITCH  semitones above the base", "Pitch T", "Pitch V", " st");
            envs[1] = std::make_unique<EnvEditor4> (h, Theme::amber(), "AMP", "Amp T", "Amp V", "");
            envs[2] = std::make_unique<EnvEditor4> (h, c.withRotatedHue (0.08f), "DISTORTION", "Dist T", "Dist V", "");
            for (auto& e : envs) { e->onChange = [this] { repaint(); }; addAndMakeVisible (*e); }
            grid = std::make_unique<StepGrid> (h, c, std::vector<StepGrid::Lane> { { "Kick", "Pattern Step %d" } }, [this] { return tl != nullptr && tl->patternOn() ? tl->currentStep() : -1; });
            addAndMakeVisible (*grid);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            auto bottom = r.removeFromBottom (54); r.removeFromBottom (6);
            grid->setBounds (bottom.reduced (8, 12).withTrimmedTop (4).withTrimmedRight (r.getWidth() * 0.3f + 8));
            auto top = r.withTrimmedRight (r.getWidth() * 0.3f + 8);
            const int w = top.getWidth() / 3;
            for (int i = 0; i < 3; ++i) envs[(size_t) i]->setBounds (top.removeFromLeft (w).reduced (2, 0));
        }
        void paint (juce::Graphics& g) override
        {
            if (synth == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto bottom = r.removeFromBottom (54.0f); r.removeFromBottom (6.0f);
            auto right = r.removeFromRight (r.getWidth() * 0.3f); r.removeFromRight (8.0f);
            auto patArea = bottom.withTrimmedRight (right.getWidth() + 8.0f);
            const bool pat = tl != nullptr && tl->patternOn();
            box (g, patArea, juce::String ("PATTERN  ") + (pat ? "ON - hold a key" : "off") + "   " + juce::StringArray { "1/8", "1/16", "1/32" }[juce::jlimit (0, 2, (int) std::lround (nv ("Pattern Rate", 1)))]);
            auto out = right.removeFromTop (right.getHeight() * 0.55f); right.removeFromTop (6.0f);
            box (g, out, "OUTPUT  " + juce::String (juce::roundToInt (nv ("Pitch (C3)", 50.0f))) + " Hz at C3"); scope (g, out.reduced (8.0f, 14.0f), accent);
            auto clickArea = right.withHeight (right.getHeight() + bottom.getHeight() + 6.0f);
            box (g, clickArea, "CLICK + SUB  " + juce::String (juce::roundToInt (nv ("Click") * 100)) + "% / " + juce::String (juce::roundToInt (nv ("Sub") * 100)) + "%");
            {
                auto in = clickArea.reduced (8.0f, 14.0f); juce::Random rnd (3); juce::Path p;
                const float ck = nv ("Click"), cd = juce::jmax (0.001f, nv ("Click Decay", 0.005f)), sub = nv ("Sub");
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth() * 0.06f;
                    const float v = (rnd.nextFloat() * 2.0f - 1.0f) * ck * std::exp (-t / cd) + std::sin (t * 50.0f * 6.28f) * 0.5f + std::sin (t * 25.0f * 6.28f) * sub * 0.5f;
                    const float y = in.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * in.getHeight() * 0.45f;
                    if (i == 0) p.startNewSubPath (in.getX(), y); else p.lineTo (in.getX() + (float) i, y);
                }
                g.setColour (Theme::amber()); g.strokePath (p, juce::PathStrokeType (1.4f));
            }
        }
        SqSynth::SqThumpLivePlugin* tl = nullptr; std::array<std::unique_ptr<EnvEditor4>, 3> envs; std::unique_ptr<StepGrid> grid;
    };
