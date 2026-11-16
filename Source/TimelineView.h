#pragma once
// Squelchmoth - the Edit window: Pro Tools style toolbar (edit modes, edit tools, zoom, counters,
// Edit selection, grid & nudge), rulers (Bars|Beats, Min:Secs, Markers), track headers, clip lanes,
// playhead, horizontal scroll and the complete Pro Tools keyboard map for editing.

#include "TrackHeader.h"
#include "LaneCanvas.h"

//==============================================================================
class PlayheadOverlay : public juce::Component
{
public:
    PlayheadOverlay (Session& s, TimelineState& st) : session (s), state (st)  { setInterceptsMouseClicks (false, false); }

    void update()
    {
        if (session.edit == nullptr) return;
        auto x = juce::roundToInt (state.timeToX (session.edit->getTransport().getPosition().inSeconds()));
        if (x != lastX)
        {
            repaint (lastX - 2, 0, 5, getHeight());
            lastX = x;
            repaint (lastX - 2, 0, 5, getHeight());
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (lastX < 0 || lastX > getWidth()) return;
        bool rec = session.edit != nullptr && session.edit->getTransport().isRecording();
        g.setColour (rec ? Theme::red() : Theme::acid());
        g.fillRect (lastX, 0, 2, getHeight());
    }

    int lastX = -10;

private:
    Session& session;
    TimelineState& state;
};

//==============================================================================
/** Pro Tools rulers, each shown or hidden from View > Rulers: Bars|Beats, Min:Secs, Timecode, Samples, Tempo, Meter
    and Markers. Bar numbering follows meter changes; the Tempo and Meter rulers show (and edit) the tempo map.
    The strip along the top shows the timeline selection and loop range; the playhead triangle rides the first ruler. */
class Ruler : public juce::Component
{
public:
    Ruler (Session& s, TimelineState& st) : session (s), state (st) {}

    enum class Row { barsBeats, minSecs, timecode, samples, tempo, meter, markers };
    static juce::StringArray rowNames()  { return { "Bars|Beats", "Min:Secs", "Timecode", "Samples", "Tempo", "Meter", "Markers" }; }
    static constexpr int stripH = 7;

    static std::vector<Row> rows()
    {
        auto txt = Prefs::getString ("ed_rulers", "0,1,4,5,6");
        std::vector<Row> out;
        for (auto& t : juce::StringArray::fromTokens (txt, ",", "")) if (t.trim().isNotEmpty()) out.push_back ((Row) juce::jlimit (0, 6, t.getIntValue()));
        if (out.empty()) out.push_back (Row::barsBeats);
        return out;
    }
    static bool isShown (Row r)    { auto v = rows(); return std::find (v.begin(), v.end(), r) != v.end(); }
    static void setShown (Row r, bool on)
    {
        auto v = rows();
        v.erase (std::remove (v.begin(), v.end(), r), v.end());
        if (on) v.push_back (r);
        std::sort (v.begin(), v.end());
        if (v.empty()) v.push_back (Row::barsBeats);
        juce::StringArray t; for (auto x : v) t.add (juce::String ((int) x));
        Prefs::set ("ed_rulers", t.joinIntoString (","));
    }
    static int rowHeight (Row r)   { return r == Row::barsBeats ? 22 : r == Row::markers ? 18 : 16; }
    static int totalHeight()       { int h = stripH; for (auto r : rows()) h += rowHeight (r); return h; }

    std::function<void()> onChanged;
    std::function<void (te::MarkerClip&)> onEditMarker;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel());
        if (session.edit == nullptr) return;
        auto& edit = *session.edit;
        auto& tr = edit.getTransport();
        const auto lanesX = headerWidthNow();
        auto layout = rows();

        // row labels + separators
        int y = stripH;
        for (auto r : layout)
        {
            const int h = rowHeight (r);
            g.setColour (Theme::dim());
            g.setFont (Theme::font (10.5f));
            juce::String label = rowNames()[(int) r].toUpperCase();
            if (r == Row::timecode) label << "  " << TimeFmt::tcRates()[(size_t) TimeFmt::tcRateIndex (edit)].name;
            g.drawText (label, 10, y, baseHeaderWidth - 20, h, juce::Justification::centredLeft);
            g.setColour (Theme::line());
            g.drawHorizontalLine (y + h - 1, 0.0f, (float) getWidth());
            y += h;
        }
        drawViewsTitles (g);

        g.saveState();
        g.reduceClipRegion (lanesX, 0, getWidth() - lanesX, getHeight());

        // timeline selection / loop strip
        {
            auto lr = tr.getLoopRange();
            auto x1 = lanesX + state.timeToX (lr.getStart().inSeconds()), x2 = lanesX + state.timeToX (lr.getEnd().inSeconds());
            g.setColour ((tr.looping.get() ? Theme::mint() : Theme::dim()).withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (x1, 0.0f, x2 - x1, (float) stripH - 1));
            if (state.timeSelection && state.timeSelection->getLength() > 0.001s)
            {
                auto s1 = lanesX + state.timeToX (state.timeSelection->getStart().inSeconds()), s2 = lanesX + state.timeToX (state.timeSelection->getEnd().inSeconds());
                g.setColour (Theme::acid().withAlpha (0.5f));
                g.fillRect (juce::Rectangle<float> (s1, 0.0f, s2 - s1, (float) stripH - 1));
                juce::Path tri;
                tri.addTriangle (s1, 0.0f, s1 + 6.0f, 0.0f, s1, 6.0f);
                tri.addTriangle (s2, 0.0f, s2 - 6.0f, 0.0f, s2, 6.0f);
                g.setColour (Theme::acid());
                g.fillPath (tri);
            }
        }

        y = stripH;
        for (auto r : layout)
        {
            const int h = rowHeight (r);
            auto area = juce::Rectangle<int> (lanesX, y, getWidth() - lanesX, h);
            switch (r)
            {
                case Row::barsBeats: drawBars (g, area); break;
                case Row::minSecs:   drawTimeRow (g, area, 0); break;
                case Row::timecode:  drawTimeRow (g, area, 1); break;
                case Row::samples:   drawTimeRow (g, area, 2); break;
                case Row::tempo:     drawTempo (g, area); break;
                case Row::meter:     drawMeter (g, area); break;
                case Row::markers:   drawMarkers (g, area); break;
            }
            y += h;
        }

        // playhead: triangle in the first ruler, line through the rest
        auto px = lanesX + state.timeToX (tr.getPosition().inSeconds());
        juce::Path tri;
        const float firstBottom = (float) (stripH + rowHeight (layout.front()));
        tri.addTriangle (px - 6, firstBottom - 9, px + 6, firstBottom - 9, px, firstBottom);
        g.setColour (tr.isRecording() ? Theme::red() : Theme::acid());
        g.fillPath (tri);
        g.fillRect (px - 0.5f, firstBottom, 1.0f, (float) getHeight() - firstBottom);
        g.restoreState();

        g.setColour (Theme::line());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (session.edit == nullptr) return;
        if (e.x < headerWidthNow())
        {
            if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 1) showRulerMenu();
            return;
        }
        auto& edit = *session.edit;
        auto t = juce::jmax (0.0, state.xToTime ((float) (e.x - headerWidthNow())));
        dragStart = t;
        const auto row = rowAt (e.y);

        if (row == Row::markers)
        {
            if (auto m = markerAt (e.x))
            {
                if (e.mods.isPopupMenu())
                {
                    juce::PopupMenu menu;
                    menu.addItem (1, "Edit memory location...");
                    menu.addItem (2, "Delete");
                    juce::ReferenceCountedObjectPtr<te::MarkerClip> mp (m);
                    menu.showMenuAsync ({}, [this, mp] (int r) { if (r == 1 && onEditMarker) onEditMarker (*mp); else if (r == 2) mp->removeFromParent(); repaint(); });
                }
                else
                {
                    edit.getTransport().setPosition (m->getPosition().getStart());
                    if (m->getPosition().getLength().inSeconds() > 0.05) { state.timeSelection = m->getPosition().time; if (onChanged) onChanged(); }
                    draggingMarker = m;
                }
                return;
            }
            if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2)
            {
                auto m = session.addMarker (te::TimePosition::fromSeconds (state.snapIfNeeded (edit, t)), "Marker " + juce::String (session.getMarkers().size() + 1));
                if (m != nullptr && onEditMarker) onEditMarker (*m);
                repaint();
            }
            return;
        }
        if (row == Row::tempo) { tempoClick (e, t); return; }
        if (row == Row::meter) { meterClick (e, t); return; }

        if (! e.mods.isShiftDown())
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (state.snapIfNeeded (edit, t)));
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.x < headerWidthNow() || session.edit == nullptr) return;
        auto t = juce::jmax (0.0, state.xToTime ((float) (e.x - headerWidthNow())));
        if (draggingMarker != nullptr)
        {
            draggingMarker->setStart (te::TimePosition::fromSeconds (state.snapIfNeeded (*session.edit, t)), false, true);
            repaint();
            return;
        }
        if (draggingTempo >= 0)
        {
            auto& ts = session.edit->tempoSequence;
            if (auto tp = ts.getTempo (draggingTempo))
            {
                // vertical drag changes the tempo value (0.1 BPM per pixel, Shift for 1 BPM per pixel)
                auto bpm = juce::jlimit (20.0, 400.0, tempoAtDragStart - e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 1.0 : 0.1));
                tp->setBpm (std::round (bpm * 100.0) / 100.0);
            }
            repaint();
            if (onChanged) onChanged();
            return;
        }
        if (rowAt (e.getMouseDownY()) == Row::tempo || rowAt (e.getMouseDownY()) == Row::meter) return;
        if (e.mods.isShiftDown() || e.getDistanceFromDragStartX() > 6)
        {
            auto& tr = session.edit->getTransport();
            auto a = state.snapIfNeeded (*session.edit, juce::jmin (dragStart, t)), b = state.snapIfNeeded (*session.edit, juce::jmax (dragStart, t));
            if (b - a > 0.05)
            {
                tr.setLoopRange ({ te::TimePosition::fromSeconds (a), te::TimePosition::fromSeconds (b) });
                if (Prefs::linkTimelineAndEdit()) { state.timeSelection = tr.getLoopRange(); }
                if (onChanged) onChanged();
            }
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override    { draggingMarker = nullptr; draggingTempo = -1; }

    te::MarkerClip* markerAt (int x)
    {
        for (auto m : session.getMarkers())
        {
            auto mx = headerWidthNow() + state.timeToX (m->getPosition().getStart().inSeconds());
            if (x >= mx - 6 && x <= mx + 7 + 60) return m;
        }
        return nullptr;
    }

private:
    Session& session;
    TimelineState& state;
    double dragStart = 0, tempoAtDragStart = 120;
    te::MarkerClip* draggingMarker = nullptr;
    int draggingTempo = -1;

    std::optional<Row> rowAt (int y) const
    {
        int yy = stripH;
        for (auto r : rows()) { if (y >= yy && y < yy + rowHeight (r)) return r; yy += rowHeight (r); }
        return {};
    }

    void showRulerMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Rulers (View > Rulers)");
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, rowNames()[i], true, isShown ((Row) i));
        m.showMenuAsync ({}, [] (int r) { if (r > 0) setShown ((Row) (r - 1), ! isShown ((Row) (r - 1))); });
    }

    void drawViewsTitles (juce::Graphics& g)
    {
        int x = baseHeaderWidth;
        auto title = [&] (bool on, int w, const char* t)
        {
            if (! on) return;
            g.setColour (Theme::line());
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
            g.setColour (Theme::acid().withAlpha (0.8f));
            g.setFont (Theme::font (10.0f, true));
            g.drawText (t, x + 4, getHeight() - 14, w - 8, 13, juce::Justification::centredLeft);
            x += w;
        };
        title (EditViews::io(), EditViews::ioW, "I/O");
        title (EditViews::insAE(), EditViews::slotW, "INSERTS A-E");
        title (EditViews::insFJ(), EditViews::slotW, "INSERTS F-J");
        title (EditViews::sendAE(), EditViews::slotW, "SENDS A-E");
        title (EditViews::sendFJ(), EditViews::slotW, "SENDS F-J");
        title (EditViews::comments(), EditViews::commentsW, "COMMENTS");
    }

    float xOf (double secs) const   { return (float) headerWidthNow() + state.timeToX (secs); }

    void drawBars (juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto& ts = session.edit->tempoSequence;
        const double t0 = juce::jmax (0.0, state.xToTime (0.0f)), t1 = state.xToTime ((float) area.getWidth());
        int bar = juce::jmax (0, ts.toBarsAndBeats (te::TimePosition::fromSeconds (t0)).bars);
        // pick a label spacing from the current bar width
        const auto b0t = ts.toTime (te::tempo::BarsAndBeats { bar, te::BeatDuration(), 0 }), b1t = ts.toTime (te::tempo::BarsAndBeats { bar + 1, te::BeatDuration(), 0 });
        const double pxPerBar = (b1t - b0t).inSeconds() / state.secondsPerPixel;
        const int step = pxPerBar > 60 ? 1 : (pxPerBar > 25 ? 2 : (pxPerBar > 10 ? 4 : (pxPerBar > 5 ? 8 : (pxPerBar > 2 ? 16 : 32))));
        bar -= bar % step;
        g.setFont (Theme::font (11.0f));
        for (int guard = 0; guard < 4000; guard += 1, bar += step)
        {
            const auto bt = ts.toTime (te::tempo::BarsAndBeats { bar, te::BeatDuration(), 0 });
            if (bt.inSeconds() > t1) break;
            const float x = xOf (bt.inSeconds());
            g.setColour (Theme::line().brighter (0.3f));
            g.drawVerticalLine (juce::roundToInt (x), (float) area.getY() + 2, (float) area.getBottom());
            g.setColour (Theme::text());
            g.drawText (juce::String (bar + 1), juce::roundToInt (x) + 4, area.getY(), 50, area.getHeight(), juce::Justification::centredLeft);
            if (step == 1)
            {
                const int num = juce::jmax (1, ts.getTimeSigAt (bt).numerator.get());
                const auto nt = ts.toTime (te::tempo::BarsAndBeats { bar + 1, te::BeatDuration(), 0 });
                if ((nt - bt).inSeconds() / state.secondsPerPixel / num > 14)
                    for (int b = 1; b < num; ++b)
                    {
                        auto bx = xOf (ts.toTime (ts.toBeats (bt) + te::BeatDuration::fromBeats (b)).inSeconds());
                        g.setColour (Theme::line());
                        g.drawVerticalLine (juce::roundToInt (bx), (float) area.getY() + area.getHeight() * 0.55f, (float) area.getBottom());
                        if ((nt - bt).inSeconds() / state.secondsPerPixel / num > 40)
                        {
                            g.setColour (Theme::dim());
                            g.setFont (Theme::font (9.0f));
                            g.drawText (juce::String (bar + 1) + "|" + juce::String (b + 1), juce::roundToInt (bx) + 3, area.getY(), 40, area.getHeight(), juce::Justification::centredLeft);
                            g.setFont (Theme::font (11.0f));
                        }
                    }
            }
        }
    }

    /** kind 0 = Min:Secs, 1 = Timecode, 2 = Samples */
    void drawTimeRow (juce::Graphics& g, juce::Rectangle<int> area, int kind)
    {
        auto& edit = *session.edit;
        const double pxPerSec = 1.0 / state.secondsPerPixel;
        static const double steps[] = { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600 };
        const double minLabelPx = kind == 1 ? 90.0 : kind == 2 ? 80.0 : 60.0;
        double stepS = 600;
        for (auto st : steps) if (st * pxPerSec >= minLabelPx) { stepS = st; break; }
        if (kind == 1)
        {
            // timecode labels on whole frames
            const double frame = 1.0 / TimeFmt::tcRates()[(size_t) TimeFmt::tcRateIndex (edit)].fps;
            if (stepS < frame) stepS = frame * std::ceil (minLabelPx / (frame * pxPerSec));
        }
        g.setFont (Theme::font (9.5f));
        const double t0 = juce::jmax (0.0, state.xToTime (0.0f)), t1 = state.xToTime ((float) area.getWidth());
        for (double s = std::floor (t0 / stepS) * stepS; s <= t1; s += stepS)
        {
            const float x = xOf (s);
            g.setColour (Theme::line().brighter (0.2f));
            g.drawVerticalLine (juce::roundToInt (x), (float) area.getY() + area.getHeight() * 0.45f, (float) area.getBottom());
            g.setColour (Theme::dim());
            juce::String txt = kind == 0 ? (stepS < 1 ? TimeFmt::minSecs (s) : juce::String::formatted ("%d:%02d", (int) s / 60, (int) std::fmod (s, 60.0)))
                             : kind == 1 ? TimeFmt::timecode (edit, s) : TimeFmt::samples (edit, s);
            g.drawText (txt, juce::roundToInt (x) + 3, area.getY(), 110, area.getHeight(), juce::Justification::centredLeft);
        }
    }

    void drawTempo (juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto& ts = session.edit->tempoSequence;
        g.setFont (Theme::font (9.5f, true));
        for (int i = 0; i < ts.getNumTempos(); ++i)
        {
            auto tp = ts.getTempo (i);
            const float x = xOf (tp->getStartTime().inSeconds());
            if (x > getWidth()) break;
            juce::Path p; p.addTriangle (x - 4, (float) area.getY() + 2, x + 4, (float) area.getY() + 2, x, (float) area.getBottom() - 3);
            g.setColour (Theme::mint());
            g.fillPath (p);
            g.setColour (Theme::text());
            g.drawText (juce::String (tp->getBpm(), tp->getBpm() == std::round (tp->getBpm()) ? 0 : 2) + " bpm", juce::roundToInt (x) + 6, area.getY(), 80, area.getHeight(), juce::Justification::centredLeft);
        }
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto& ts = session.edit->tempoSequence;
        g.setFont (Theme::font (9.5f, true));
        for (int i = 0; i < ts.getNumTimeSigs(); ++i)
        {
            auto sig = ts.getTimeSig (i);
            const float x = xOf (ts.toTime (sig->startBeatNumber.get()).inSeconds());
            if (x > getWidth()) break;
            g.setColour (Theme::amber());
            g.fillRect (x - 1.0f, (float) area.getY() + 2, 3.0f, (float) area.getHeight() - 4);
            g.setColour (Theme::text());
            g.drawText (juce::String (sig->numerator.get()) + "/" + juce::String (sig->denominator.get()), juce::roundToInt (x) + 6, area.getY(), 60, area.getHeight(), juce::Justification::centredLeft);
        }
    }

    void drawMarkers (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setFont (Theme::font (10.5f, true));
        for (auto m : session.getMarkers())
        {
            auto x = xOf (m->getPosition().getStart().inSeconds());
            if (x < headerWidthNow() - 200 || x > getWidth()) continue;
            auto w = m->getPosition().getLength().inSeconds() > 0.05 ? juce::jmax (12.0f, (float) (m->getPosition().getLength().inSeconds() / state.secondsPerPixel)) : 0.0f;
            auto y = (float) area.getY();
            juce::Path p;
            p.addTriangle (x - 5, y + 2, x + 5, y + 2, x, y + 9);
            g.setColour (Theme::amber());
            g.fillPath (p);
            if (w > 0) { g.setColour (Theme::amber().withAlpha (0.25f)); g.fillRect (x, y + 2, w, (float) area.getHeight() - 4); }
            g.setColour (Theme::text());
            g.drawText (juce::String (m->getMarkerID()) + "  " + m->getName(), juce::roundToInt (x) + 7, area.getY() + 1, 200, area.getHeight() - 2, juce::Justification::centredLeft);
            g.setColour (Theme::amber().withAlpha (0.35f));
            g.drawVerticalLine (juce::roundToInt (x), (float) stripH, y);
        }
    }

    int tempoIndexAt (int x)
    {
        auto& ts = session.edit->tempoSequence;
        for (int i = 0; i < ts.getNumTempos(); ++i)
            if (std::abs (xOf (ts.getTempo (i)->getStartTime().inSeconds()) - (float) x) < 6) return i;
        return -1;
    }

    int meterIndexAt (int x)
    {
        auto& ts = session.edit->tempoSequence;
        for (int i = 0; i < ts.getNumTimeSigs(); ++i)
            if (std::abs (xOf (ts.toTime (ts.getTimeSig (i)->startBeatNumber.get()).inSeconds()) - (float) x) < 6) return i;
        return -1;
    }

    void tempoClick (const juce::MouseEvent& e, double t)
    {
        auto& edit = *session.edit;
        auto& ts = edit.tempoSequence;
        const int idx = tempoIndexAt (e.x);
        if (idx >= 0 && e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Change tempo...");
            m.addItem (2, "Delete tempo change", idx > 0);
            m.showMenuAsync ({}, [this, idx] (int r) { if (r == 1) tempoDialog (idx, {}); else if (r == 2 && session.edit != nullptr) { session.edit->tempoSequence.removeTempo (idx, false); repaint(); if (onChanged) onChanged(); } });
            return;
        }
        if (idx >= 0)
        {
            if (e.getNumberOfClicks() >= 2) { tempoDialog (idx, {}); return; }
            draggingTempo = idx;
            tempoAtDragStart = ts.getTempo (idx)->getBpm();
            return;
        }
        if (e.getNumberOfClicks() >= 2 || e.mods.isPopupMenu())
            tempoDialog (-1, te::TimePosition::fromSeconds (state.snapToGrid (edit, t)));
    }

    void tempoDialog (int index, std::optional<te::TimePosition> at)
    {
        auto& ts = session.edit->tempoSequence;
        auto current = index >= 0 ? ts.getTempo (index)->getBpm() : ts.getTempoAt (*at).getBpm();
        auto aw = std::make_shared<juce::AlertWindow> (index >= 0 ? "Change Tempo" : "New Tempo Change",
                                                       index >= 0 ? "Tempo (BPM):" : "Tempo (BPM) from " + session.barsBeatsString (*at) + ":", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("bpm", juce::String (current, 2));
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, index, at] (int r)
        {
            aw->setVisible (false);
            if (r != 1 || session.edit == nullptr) return;
            auto bpm = juce::jlimit (20.0, 400.0, aw->getTextEditorContents ("bpm").getDoubleValue());
            auto& ts2 = session.edit->tempoSequence;
            auto* um = &session.edit->getUndoManager();
            if (index >= 0) { if (auto tp = ts2.getTempo (index)) tp->setBpm (bpm); }
            else ts2.insertTempo (ts2.toBeats (*at), bpm, 0.0f);
            repaint();
            if (onChanged) onChanged();
        }));
    }

    void meterClick (const juce::MouseEvent& e, double t)
    {
        auto& edit = *session.edit;
        const int idx = meterIndexAt (e.x);
        if (idx >= 0 && (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2))
        {
            if (e.mods.isPopupMenu() && idx > 0)
            {
                juce::PopupMenu m;
                m.addItem (1, "Change meter...");
                m.addItem (2, "Delete meter change");
                m.showMenuAsync ({}, [this, idx] (int r) { if (r == 1) meterDialog (idx, {}); else if (r == 2 && session.edit != nullptr) { session.edit->tempoSequence.removeTimeSig (idx); repaint(); if (onChanged) onChanged(); } });
            }
            else meterDialog (idx, {});
            return;
        }
        if (e.getNumberOfClicks() >= 2 || e.mods.isPopupMenu())
        {
            // meter changes always start on a bar line
            auto bb = edit.tempoSequence.toBarsAndBeats (te::TimePosition::fromSeconds (t));
            meterDialog (-1, edit.tempoSequence.toTime (te::tempo::BarsAndBeats { bb.bars + (bb.beats.inBeats() > 0.5 ? 1 : 0), te::BeatDuration(), 0 }));
        }
    }

    void meterDialog (int index, std::optional<te::TimePosition> at)
    {
        auto& ts = session.edit->tempoSequence;
        auto* sig = index >= 0 ? ts.getTimeSig (index) : &ts.getTimeSigAt (*at);
        auto aw = std::make_shared<juce::AlertWindow> (index >= 0 ? "Change Meter" : "New Meter Change",
                                                       index >= 0 ? "Meter (e.g. 4/4, 3/4, 6/8, 7/8):" : "Meter from bar " + juce::String (session.timeToBar (*at), 0) + " (e.g. 3/4, 6/8):",
                                                       juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("m", juce::String (sig->numerator.get()) + "/" + juce::String (sig->denominator.get()));
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, index, at] (int r)
        {
            aw->setVisible (false);
            if (r != 1 || session.edit == nullptr) return;
            auto p = juce::StringArray::fromTokens (aw->getTextEditorContents ("m"), "/", "");
            const int n = p[0].getIntValue(), d = p[1].getIntValue();
            if (n < 1 || n > 32 || ! (d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32)) return;
            auto& ts2 = session.edit->tempoSequence;
            te::TimeSigSetting* target = nullptr;
            if (index >= 0) target = ts2.getTimeSig (index);
            else if (auto ns = ts2.insertTimeSig (*at)) target = ns.get();
            if (target != nullptr) { target->numerator = n; target->denominator = d; }
            repaint();
            if (onChanged) onChanged();
        }));
    }
};

//==============================================================================
class TimelineView : public juce::Component,
                     private juce::Timer,
                     private juce::ValueTree::Listener,
                     private juce::AsyncUpdater,
                     private juce::ChangeListener,
                     private juce::ScrollBar::Listener
{
public:
    explicit TimelineView (Session& s) : session (s)
    {
        addAndMakeVisible (ruler);
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&body, false);
        viewport.setScrollBarsShown (true, false);
        body.addAndMakeVisible (canvas);
        body.addAndMakeVisible (playhead);
        addAndMakeVisible (hScroll);
        hScroll.addListener (this);
        hScroll.setAutoHide (false);

        canvas.getTrackY = [this] (te::Track* t) { return trackY (t); };
        canvas.getTrackAtY = [this] (int y) { return trackAtY (y); };
        canvas.getRows = [this] { return visibleRows(); };
        canvas.onSelectionChanged = [this] { selectionChanged(); };
        canvas.onOpenMidiClip = [this] (te::MidiClip& mc) { if (onOpenMidiClip) onOpenMidiClip (mc); };
        canvas.onOpenStepClip = [this] (te::StepClip& sc) { if (onOpenStepClip) onOpenStepClip (sc); };
        canvas.onZoomToRange = [this] (double a, double b) { zoomToRange (a, b); };
        canvas.onZoom = [this] (double f, double anchor) { zoomAround (f, anchor); };
        ruler.onChanged = [this] { selectionChanged(); };
        ruler.onEditMarker = [this] (te::MarkerClip& m) { editMarker (m); };

        buildToolbar();

        session.addChangeListener (this);
        Theme::broadcaster().addChangeListener (this);
        Prefs::broadcaster().addChangeListener (this);
        attachToEdit();
        startTimerHz (30);
    }

    ~TimelineView() override
    {
        session.removeChangeListener (this);
        Theme::broadcaster().removeChangeListener (this);
        Prefs::broadcaster().removeChangeListener (this);
        if (watched.isValid()) watched.removeListener (this);
    }

    std::function<void (te::MidiClip&)> onOpenMidiClip;
    std::function<void (te::StepClip&)> onOpenStepClip;
    std::function<void()> onNewMemoryLocation;
    std::function<void()> onNewTracks;
    TimelineState state;
    LaneCanvas& getCanvas()     { return canvas; }

    //==========================================================================
    /** Pro Tools edit-mode switching: plain = that mode alone; Shift-click Grid while in Shuffle or Slip (or Shift-click
        Shuffle / Slip while in Grid) combines them; Cmd-click Shuffle toggles Shuffle Lock. */
    void setEditMode (EditMode m, bool combineWithGrid = false)
    {
        if (m == EditMode::shuffle && state.shuffleLock && state.editMode != EditMode::shuffle)
        {
            session.log.add ("You", "Shuffle is locked - Cmd-click the Shuffle button to unlock it");
            return;
        }
        if (combineWithGrid && m == EditMode::grid && (state.editMode == EditMode::shuffle || state.editMode == EditMode::slip))
            state.snap = ! state.snap;
        else if (combineWithGrid && (m == EditMode::shuffle || m == EditMode::slip) && (state.editMode == EditMode::grid || state.snap))
        {
            state.editMode = m;
            state.snap = true;
        }
        else
        {
            state.editMode = m;
            state.snap = false;
        }
        refreshToolbar();
        session.log.add ("You", juce::String ("Edit mode: ") + editModeName (state.editMode) + (state.snap && state.editMode != EditMode::grid ? " + Grid" : ""));
    }
    void toggleShuffleLock()
    {
        state.shuffleLock = ! state.shuffleLock;
        if (state.shuffleLock && state.editMode == EditMode::shuffle) state.editMode = EditMode::slip;
        session.log.add ("You", state.shuffleLock ? "Shuffle Lock on" : "Shuffle Lock off");
        refreshToolbar();
    }
    void setTool (EditTool t)       { state.tool = t; refreshToolbar(); }
    EditMode getEditMode() const    { return state.editMode; }
    EditTool getTool() const        { return state.tool; }

    void zoomTo (double barsVisible, double firstBar)
    {
        if (session.edit == nullptr) return;
        auto start = session.barToTime (firstBar).inSeconds(), end = session.barToTime (firstBar + barsVisible).inSeconds();
        zoomToRange (start, end);
    }

    void zoomToRange (double start, double end)
    {
        state.viewStart = juce::jmax (0.0, start);
        state.secondsPerPixel = juce::jlimit (0.00002, 2.0, (end - start) / juce::jmax (100, canvas.getWidth()));
        repaintAll();
    }

    void zoomToFit()
    {
        if (session.edit == nullptr) return;
        auto len = juce::jmax (20.0, session.edit->getLength().inSeconds() * 1.1);
        zoomToRange (0, len);
    }

    void zoomToSelection()
    {
        if (state.timeSelection && state.timeSelection->getLength() > 0.01s)
        {
            auto pad = state.timeSelection->getLength().inSeconds() * 0.05;
            zoomToRange (state.timeSelection->getStart().inSeconds() - pad, state.timeSelection->getEnd().inSeconds() + pad);
        }
        else if (state.selectedClip != nullptr)
        {
            auto p = state.selectedClip->getPosition();
            zoomToRange (p.getStart().inSeconds() - p.getLength().inSeconds() * 0.05, p.getEnd().inSeconds() + p.getLength().inSeconds() * 0.05);
        }
    }

    void zoomAround (double factor, double anchorSecs)
    {
        auto x = state.timeToX (anchorSecs);
        state.secondsPerPixel = juce::jlimit (0.00002, 2.0, state.secondsPerPixel * factor);
        state.viewStart = juce::jmax (0.0, anchorSecs - x * state.secondsPerPixel);
        repaintAll();
    }

    /** Pro Tools "Fit to Window": the given tracks share the visible height of the Edit window equally. */
    void fitHeights (juce::Array<te::Track*> ts)
    {
        if (ts.isEmpty()) return;
        const int each = juce::jlimit (TimelineState::heightPresets().front().px, 1200, viewport.getViewHeight() / ts.size());
        for (auto t : ts) state.setHeight (t, each);
        layoutBody(); repaintAll();
        scrollToTrack (ts.getFirst());
    }

    void setTrackHeightsAll (int h)
    {
        for (auto t : session.getRows()) state.setHeight (t, h);
        layoutBody(); repaintAll();
    }

    void scrollToTrack (te::Track* t)
    {
        if (t == nullptr) return;
        auto y = trackY (t);
        auto vp = viewport.getViewPositionY();
        if (y < vp || y + state.heightFor (t) > vp + viewport.getViewHeight())
            viewport.setViewPosition (0, juce::jmax (0, y - 20));
    }

    void addMemoryLocationAtPlayhead()
    {
        if (session.edit == nullptr) return;
        auto at = session.edit->getTransport().getPosition();
        te::TimeDuration len;
        if (state.timeSelection && state.timeSelection->getLength() > 0.05s) { at = state.timeSelection->getStart(); len = state.timeSelection->getLength(); }
        if (auto m = session.addMarker (at, "Marker " + juce::String (session.getMarkers().size() + 1), len))
            editMarker (*m);
    }

    void editMarker (te::MarkerClip& m)
    {
        auto aw = std::make_shared<juce::AlertWindow> ("Memory Location " + juce::String (m.getMarkerID()), "Name and position of this memory location:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", m.getName(), "Name");
        aw->addTextEditor ("pos", TimeFmt::format (*session.edit, m.getPosition().getStart(), (TimeFmt::Kind) mainFmt()), "Position (" + TimeFmt::kindNames()[mainFmt()] + ", or any format)");
        aw->addTextEditor ("num", juce::String (m.getMarkerID()), "Number");
        aw->addComboBox ("kind", { "Marker", "Selection (uses the current Edit selection length)" }, "Type");
        aw->getComboBoxComponent ("kind")->setSelectedItemIndex (m.getPosition().getLength().inSeconds() > 0.05 ? 1 : 0);
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Delete", 2);
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::ReferenceCountedObjectPtr<te::MarkerClip> mp (&m);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, mp] (int r)
        {
            aw->setVisible (false);
            if (r == 2) { mp->removeFromParent(); repaintAll(); return; }
            if (r != 1 || session.edit == nullptr) return;
            mp->setName (aw->getTextEditorContents ("name"));
            if (auto t = TimeFmt::parse (*session.edit, aw->getTextEditorContents ("pos"), (TimeFmt::Kind) mainFmt()))
                mp->setStart (*t, false, true);
            if (aw->getTextEditorContents ("num").getIntValue() > 0) mp->setMarkerID (aw->getTextEditorContents ("num").getIntValue());
            if (aw->getComboBoxComponent ("kind")->getSelectedItemIndex() == 1 && state.timeSelection && state.timeSelection->getLength() > 0.05s)
                mp->setPosition ({ { mp->getPosition().getStart(), mp->getPosition().getStart() + state.timeSelection->getLength() }, {} });
            else if (aw->getComboBoxComponent ("kind")->getSelectedItemIndex() == 0)
                mp->setPosition ({ { mp->getPosition().getStart(), mp->getPosition().getStart() + 0.001s }, {} });
            repaintAll();
        }));
    }

    void goToMarker (int number)
    {
        for (auto m : session.getMarkers())
            if (m->getMarkerID() == number)
            {
                session.edit->getTransport().setPosition (m->getPosition().getStart());
                if (m->getPosition().getLength().inSeconds() > 0.05) state.timeSelection = m->getPosition().time;
                selectionChanged();
                return;
            }
    }

    //==========================================================================
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (session.edit == nullptr) return false;
        auto& tr = session.edit->getTransport();
        auto mods = k.getModifiers();
        auto code = k.getKeyCode();
        auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        bool cmd = mods.isCommandDown(), shift = mods.isShiftDown(), alt = mods.isAltDown(), plain = ! mods.isAnyModifierKeyDown();

        // Edit modes / tools
        if (code == juce::KeyPress::F1Key) { setEditMode (EditMode::shuffle); return true; }
        if (code == juce::KeyPress::F2Key) { setEditMode (EditMode::slip); return true; }
        if (code == juce::KeyPress::F3Key) { setEditMode (EditMode::spot); return true; }
        if (code == juce::KeyPress::F4Key) { setEditMode (EditMode::grid); return true; }
        if (code == juce::KeyPress::F5Key) { setTool (EditTool::zoomer); return true; }
        if (code == juce::KeyPress::F6Key) { setTool (state.tool == EditTool::trim ? EditTool::smart : EditTool::trim); return true; }
        if (code == juce::KeyPress::F7Key) { setTool (state.tool == EditTool::selector ? EditTool::smart : EditTool::selector); return true; }
        if (code == juce::KeyPress::F8Key) { setTool (state.tool == EditTool::grabber ? EditTool::smart : EditTool::grabber); return true; }
        if (code == juce::KeyPress::F9Key) { setTool (EditTool::scrubber); return true; }
        if (code == juce::KeyPress::F10Key) { setTool (EditTool::pencil); return true; }
        if (k == juce::KeyPress::escapeKey) { state.deselectClips(); state.timeSelection.reset(); repaintAll(); return true; }

        // Clipboard & editing
        if (cmd && code == 'E') { EditOps::separate (session, state); repaintAll(); return true; }
        if (cmd && ! shift && code == 'D') { EditOps::duplicate (session, state); repaintAll(); return true; }
        if ((cmd && shift && code == 'D') || (alt && code == 'R')) { canvas.repeatDialog(); return true; }
        if (cmd && code == 'F') { EditOps::fade (session, state); repaintAll(); return true; }
        if (cmd && code == 'U') { canvas.stripSilenceDialog(); return true; }
        if (cmd && code == 'H') { EditOps::heal (session, state); repaintAll(); return true; }
        if (cmd && code == 'T') { EditOps::trimToSelection (session, state); repaintAll(); return true; }
        if (cmd && code == 'M' && ! shift) { for (auto& c : state.selectedClips) c->setMuted (! c->isMuted()); repaintAll(); return true; }
        if (cmd && code == 'A') { EditOps::selectAll (session, state); repaintAll(); return true; }
        if (cmd && shift && (code == ',' || k.getTextCharacter() == '<'))   // 1.12: Identify Sync Point at the playhead
        {
            if (session.edit != nullptr)
            {
                const auto at = session.edit->getTransport().getPosition();
                for (auto& c : state.selectedClips) EditOps::setSyncPoint (*c, at);
            }
            repaintAll(); return true;
        }
        if (cmd && code == 'X') { EditOps::cut (session, state); repaintAll(); return true; }
        if (cmd && code == 'C') { EditOps::copy (session, state); return true; }
        if (cmd && code == 'V') { EditOps::paste (session, state); repaintAll(); return true; }
        if ((alt || cmd) && shift && k.getTextCharacter() == '3') { juce::String err; EditOps::consolidate (session, state, err); if (err.isNotEmpty()) Ui::message (juce::MessageBoxIconType::WarningIcon, "Consolidate", err); repaintAll(); return true; }
        if (k == juce::KeyPress::backspaceKey || k == juce::KeyPress::deleteKey)
        {
            if (! state.selectedClips.empty() || (state.timeSelection && state.timeSelection->getLength() > 0.001s)) { EditOps::deleteSelection (session, state, shift); repaintAll(); return true; }
            return false;
        }

        // Legacy single-key split + trims (keyboard focus)
        if (plain && ch == 'b') { EditOps::separate (session, state); repaintAll(); return true; }
        if (plain && ch == 'a') { EditOps::trimToPlayhead (session, state, true); repaintAll(); return true; }
        if (plain && ch == 's') { EditOps::trimToPlayhead (session, state, false); repaintAll(); return true; }

        // Zoom
        if ((cmd && code == ']') || (plain && (ch == 't' || k.getTextCharacter() == '='))) { zoom (0.7); return true; }
        if ((cmd && code == '[') || (plain && (ch == 'r' || k.getTextCharacter() == '-'))) { zoom (1.4); return true; }
        if (alt && ! cmd && code == 'F') { zoomToSelection(); return true; }
        if (alt && cmd && code == 'F') { zoomToFit(); return true; }
        // Track heights (Pro Tools): Ctrl+Up / Ctrl+Down steps the selected tracks through Micro..Extreme; Cmd+Opt does every track
        if ((code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) && ((cmd && alt) || (mods.isCtrlDown() && ! alt && ! shift)))
        {
            const int dir = code == juce::KeyPress::upKey ? 1 : -1;
            juce::Array<te::Track*> targets;
            for (auto t : session.getRows()) if ((cmd && alt) || state.isTrackSelected (t)) targets.add (t);
            if (targets.isEmpty() && state.selectedRow != nullptr) targets.add (state.selectedRow);
            if (targets.isEmpty()) targets = session.getRows();
            for (auto t : targets) state.stepHeight (t, dir);
            layoutBody(); repaintAll(); return true;
        }

        // Nudge (+ / - and , . for grid)
        if (k.getTextCharacter() == '+' || (code == juce::KeyPress::numberPadAdd)) { EditOps::nudge (session, state, 1); repaintAll(); return true; }
        if ((k.getTextCharacter() == '_' ) || code == juce::KeyPress::numberPadSubtract) { EditOps::nudge (session, state, -1); repaintAll(); return true; }
        if (plain && k.getTextCharacter() == '.') { EditOps::nudge (session, state, 1, true); repaintAll(); return true; }
        if (plain && k.getTextCharacter() == ',') { EditOps::nudge (session, state, -1, true); repaintAll(); return true; }

        // Tab to transient / clip boundary
        if (code == juce::KeyPress::tabKey) { canvas.tabToNext (alt); repaintAll(); return true; }

        // Track selection / solo / mute / arm on selected tracks
        if (shift && ! cmd && (ch == 's' || ch == 'm' || ch == 'r'))
        {
            for (auto t : session.getRows())
                if (state.isTrackSelected (t))
                {
                    if (ch == 's') session.setTrackSolo (*t, ! t->isSolo (false));
                    else if (ch == 'm') session.setTrackMute (*t, ! t->isMuted (false));
                    else if (auto at = dynamic_cast<te::AudioTrack*> (t)) session.setArmed (*at, ! session.isArmed (*at));
                }
            repaintAll();
            return true;
        }
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey || (plain && (ch == 'p' || ch == ';')))
        {
            auto rows = session.getRows (&state.collapsedFolders);
            int idx = rows.indexOf (state.selectedRow);
            int next = juce::jlimit (0, rows.size() - 1, idx + ((code == juce::KeyPress::upKey || ch == 'p') ? -1 : 1));
            if (rows.size() > 0)
            {
                if (shift && idx >= 0) { if (! state.isTrackSelected (rows[next])) state.selectedTrackIDs.push_back (rows[next]->itemID); state.selectedRow = rows[next]; state.selectedTrack = dynamic_cast<te::AudioTrack*> (rows[next]); }
                else state.selectTrack (rows[next]);
                scrollToTrack (rows[next]);
            }
            repaintAll();
            return true;
        }
        if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
        {
            if (state.timeSelection) tr.setPosition (code == juce::KeyPress::leftKey ? state.timeSelection->getStart() : state.timeSelection->getEnd());
            else EditOps::nudge (session, state, code == juce::KeyPress::leftKey ? -1 : 1, true);
            repaintAll();
            return true;
        }
        if (k == juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys::shiftModifier, 0))
        {
            addMemoryLocationAtPlayhead();
            return true;
        }
        if (plain && k.getTextCharacter() >= '1' && k.getTextCharacter() <= '9' && ! state.selectedClips.empty() && false) return false;
        return false;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        layoutToolbar (r.removeFromTop (toolbarHeight));
        ruler.setBounds (r.removeFromTop (Ruler::totalHeight()));
        lastRulerHeight = Ruler::totalHeight();
        hScroll.setBounds (r.removeFromBottom (14).withTrimmedLeft (headerWidthNow()));
        viewport.setBounds (r);
        layoutBody();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg());
        auto tb = getLocalBounds().removeFromTop (toolbarHeight);
        g.setGradientFill (juce::ColourGradient (Theme::panel2(), 0, 0, Theme::panel(), 0, (float) toolbarHeight, false));
        g.fillRect (tb);
        g.setColour (Theme::line());
        g.drawHorizontalLine (toolbarHeight - 1, 0.0f, (float) getWidth());
        // recessed wells behind the button groups (1.10 console finish)
        auto groupWell = [&] (juce::Rectangle<int> u)
        {
            if (u.isEmpty()) return;
            auto w = u.expanded (4, 3).toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (w, 7.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) w.getBottom() - 1, w.getX() + 7.0f, w.getRight() - 7.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (w, 7.0f, 1.0f);
        };
        juce::Rectangle<int> modes, tools;
        for (auto& b : modeBtns) modes = modes.isEmpty() ? b.getBounds() : modes.getUnion (b.getBounds());
        for (auto& b : toolBtns) tools = tools.isEmpty() ? b.getBounds() : tools.getUnion (b.getBounds());
        groupWell (modes); groupWell (tools);
        g.setFont (Theme::font (9.0f, true).withExtraKerningFactor (0.12f));
        g.setColour (Theme::dim());
        g.drawText ("EDIT MODE", modeLabelX, 3, 120, 10, juce::Justification::left);
        g.drawText ("TOOLS", toolsLabelX, 3, 120, 10, juce::Justification::left);
        g.drawText ("ZOOM", zoomLabelX, 3, 60, 10, juce::Justification::left);
        g.drawText ("MAIN COUNTER", counterX, 3, 120, 10, juce::Justification::left);
        g.drawText ("EDIT SELECTION  START / END / LENGTH", selX, 3, 260, 10, juce::Justification::left);
        g.drawText ("GRID", gridX, 49, 60, 10, juce::Justification::left);
        g.drawText ("NUDGE", nudgeX, 49, 60, 10, juce::Justification::left);
        g.drawText ("TAB TO TRANSIENT  /  LINK TIMELINE & EDIT", nudgeX + 166, 49, 300, 10, juce::Justification::left);
    }

private:
    static constexpr int toolbarHeight = 92;
    Session& session;
    Ruler ruler { session, state };
    juce::Viewport viewport;
    juce::Component body;
    LaneCanvas canvas { session, state };
    PlayheadOverlay playhead { session, state };
    juce::ScrollBar hScroll { false };
    std::vector<std::unique_ptr<TrackHeader>> headers;

    // toolbar
    juce::TextButton modeBtns[4] { juce::TextButton ("SHUFFLE"), juce::TextButton ("SLIP"), juce::TextButton ("SPOT"), juce::TextButton ("GRID") };
    juce::TextButton toolBtns[7] { juce::TextButton ("ZOOM"), juce::TextButton ("TRIM"), juce::TextButton ("SELECT"), juce::TextButton ("GRAB"), juce::TextButton ("SCRUB"), juce::TextButton ("PENCIL"), juce::TextButton ("SMART") };
    juce::TextButton zoomInBtn { "+" }, zoomOutBtn { "-" }, fitBtn { "FIT" }, fitSelBtn { "SEL" }, addTrackBtn { "+ TRACK" }, tabTransBtn { "TAB>|" }, linkBtn { "LINK" }, relGridBtn { "REL" }, snapBtn { "+GRID" };
    juce::TextButton insFollowBtn { "INS FOLLOW" }, scrollBtn { "SCROLL" }, ctrFmtBtn { "BB" }, subFmtBtn { "MS" };
    static int mainFmt()   { return juce::jlimit (0, 3, Prefs::getInt ("ed_mainCounterFmt", 0)); }
    static int subFmt()    { return juce::jlimit (0, 3, Prefs::getInt ("ed_subCounterFmt", 1)); }
    void counterFormatMenu (bool main)
    {
        juce::PopupMenu m;
        for (int i = 0; i < 4; ++i) m.addItem (i + 1, TimeFmt::kindNames()[i], true, (main ? mainFmt() : subFmt()) == i);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (main ? ctrFmtBtn : subFmtBtn), [this, main] (int r)
        {
            if (r <= 0) return;
            Prefs::set (main ? "ed_mainCounterFmt" : "ed_subCounterFmt", r - 1);
            refreshToolbar();
        });
    }
    juce::Label counter, subCounter, selStart, selEnd, selLength, hint;
    juce::ComboBox gridCombo, nudgeCombo;
    int modeLabelX = 0, toolsLabelX = 0, zoomLabelX = 0, counterX = 0, selX = 0, gridX = 0, nudgeX = 0;

    juce::ValueTree watched;
    bool structureDirty = true;
    int repaintCountdown = 0;

    //==========================================================================
    void buildToolbar()
    {
        for (int i = 0; i < 4; ++i)
        {
            addAndMakeVisible (modeBtns[i]);
            modeBtns[i].setClickingTogglesState (false);
            modeBtns[i].setColour (juce::TextButton::buttonOnColourId, Theme::violet());
            modeBtns[i].onClick = [this, i]
            {
                auto mods = juce::ModifierKeys::currentModifiers;
                if (i == 0 && mods.isCommandDown()) { toggleShuffleLock(); return; }
                setEditMode ((EditMode) i, mods.isShiftDown());
            };
            modeBtns[i].setTooltip (juce::String ("F") + juce::String (i + 1) + " - " + editModeName ((EditMode) i) + " mode");
        }
        modeBtns[0].setTooltip ("Shuffle (F1): clips are tiles - drag to re-order, trims and deletes ripple the later clips. "
                                "Cmd-click: Shuffle Lock. Shift-click while in Grid: Shuffle + Grid");
        modeBtns[1].setTooltip ("Slip (F2): free movement, no snapping (hold Cmd to snap). A clip dropped over another covers it. "
                                "Shift-click while in Grid: Slip + Grid");
        modeBtns[2].setTooltip ("Spot (F3): click a clip (or drop a file) to type its exact Start / End / Duration in any time format");
        modeBtns[3].setTooltip ("Grid (F4): everything snaps to the grid (hold Cmd to move freely). Right-click for Absolute / Relative. "
                                "Shift-click while in Shuffle or Slip combines them");
        modeBtns[3].onClick = [this]
        {
            if (juce::ModifierKeys::currentModifiers.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem (1, "Absolute Grid", true, ! state.gridRelative);
                m.addItem (2, "Relative Grid", true, state.gridRelative);
                m.addSeparator();
                m.addItem (3, "Show Grid Lines", true, Prefs::getBool ("ed_gridLines", true));
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (modeBtns[3]), [this] (int r)
                {
                    if (r == 3) Prefs::set ("ed_gridLines", ! Prefs::getBool ("ed_gridLines", true));
                    else if (r) state.gridRelative = r == 2;
                    refreshToolbar();
                });
            }
            else setEditMode (EditMode::grid, juce::ModifierKeys::currentModifiers.isShiftDown());
        };

        const char* toolTips[7] = { "Zoomer (F5): drag a range to zoom into it, click to zoom in, Opt-click to zoom out",
                                    "Trim (F6): drag clip edges (left half trims the start, right half the end)",
                                    "Selector (F7): drag to make an Edit selection across tracks, click to place the insertion point",
                                    "Grabber (F8): click/drag clips, Shift-click adds, Opt-drag copies, Cmd-click toggles",
                                    "Scrubber (F9): drag to scrub audio",
                                    "Pencil (F10): draw volume automation",
                                    "Smart Tool: Selector in the top half of clips, Grabber in the bottom half, Trim at edges, Fades in the top corners" };
        for (int i = 0; i < 7; ++i)
        {
            addAndMakeVisible (toolBtns[i]);
            toolBtns[i].setClickingTogglesState (false);
            toolBtns[i].setColour (juce::TextButton::buttonOnColourId, Theme::mint().darker (0.45f));
            toolBtns[i].onClick = [this, i] { setTool ((EditTool) i); };
            toolBtns[i].setTooltip (toolTips[i]);
        }

        for (auto* b : { &addTrackBtn, &zoomInBtn, &zoomOutBtn, &fitBtn, &fitSelBtn, &snapBtn, &tabTransBtn, &linkBtn, &relGridBtn, &insFollowBtn, &scrollBtn, &ctrFmtBtn, &subFmtBtn })
            addAndMakeVisible (b);
        insFollowBtn.setClickingTogglesState (false);
        insFollowBtn.setTooltip ("Insertion Follows Playback: when on, Stop leaves the insertion point where playback stopped; "
                                 "off (Pro Tools default) returns to where play started");
        insFollowBtn.onClick = [this] { Prefs::set ("op_returnToStart", ! Prefs::returnToStartOnStop()); refreshToolbar(); };
        insFollowBtn.setColour (juce::TextButton::buttonOnColourId, Theme::violet().darker (0.3f));
        scrollBtn.setTooltip ("Edit window scrolling during playback: No Auto-Scrolling, Page, or Continuous (playhead stays centred)");
        scrollBtn.onClick = [this]
        {
            juce::PopupMenu m;
            m.addItem (1, "No Auto-Scrolling", true, Prefs::scrollMode() == 0);
            m.addItem (2, "Page Scrolling", true, Prefs::scrollMode() == 1);
            m.addItem (3, "Continuous Scrolling (playhead centred)", true, Prefs::scrollMode() == 2);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (scrollBtn), [this] (int r) { if (r) { Prefs::set ("op_scrollMode", r - 1); refreshToolbar(); } });
        };
        for (auto* b : { &ctrFmtBtn, &subFmtBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, Theme::bg());
            b->setTooltip ("Time format: Bars|Beats, Min:Secs, Timecode or Samples");
        }
        ctrFmtBtn.onClick = [this] { counterFormatMenu (true); };
        subFmtBtn.onClick = [this] { counterFormatMenu (false); };
        addTrackBtn.onClick = [this]
        {
            if (juce::ModifierKeys::currentModifiers.isPopupMenu() || onNewTracks == nullptr) showAddTrackMenu();
            else onNewTracks();
        };
        addTrackBtn.setTooltip ("New Tracks (" SQ_CMD "+Shift+N) - right-click for a quick track menu");
        zoomInBtn.onClick = [this] { zoom (0.7); };          zoomInBtn.setTooltip ("Zoom in (" SQ_CMD "+] or T)");
        zoomOutBtn.onClick = [this] { zoom (1.4); };         zoomOutBtn.setTooltip ("Zoom out (" SQ_CMD "+[ or R)");
        fitBtn.onClick = [this] { zoomToFit(); };            fitBtn.setTooltip ("Fit whole session (" SQ_CMD "+" SQ_OPT "+F)");
        fitSelBtn.onClick = [this] { zoomToSelection(); };   fitSelBtn.setTooltip ("Zoom to the selection (" SQ_OPT "+F)");
        snapBtn.setClickingTogglesState (false);
        snapBtn.setTooltip ("Grid combined with Shuffle or Slip (same as Shift-clicking the Grid button): positions snap to the grid "
                            "while the mode's own behaviour is kept. Hold Cmd while dragging to swap Grid and Slip.");
        snapBtn.onClick = [this] { if (state.editMode == EditMode::shuffle || state.editMode == EditMode::slip) { state.snap = ! state.snap; refreshToolbar(); } };
        snapBtn.setColour (juce::TextButton::buttonOnColourId, Theme::violet().darker (0.3f));
        tabTransBtn.setClickingTogglesState (true);
        tabTransBtn.setTooltip ("Tab to Transient: Tab / " SQ_OPT "+Tab jump between detected transients instead of clip edges");
        tabTransBtn.onClick = [this] { state.tabToTransients = tabTransBtn.getToggleState(); canvas.repaint(); };
        tabTransBtn.setColour (juce::TextButton::buttonOnColourId, Theme::magenta().darker (0.4f));
        linkBtn.setClickingTogglesState (true);
        linkBtn.setToggleState (Prefs::linkTimelineAndEdit(), juce::dontSendNotification);
        linkBtn.setTooltip ("Link Timeline and Edit Selection: the Edit selection sets the loop / playback range");
        linkBtn.onClick = [this] { Prefs::set ("op_linkTimelineEdit", linkBtn.getToggleState()); };
        linkBtn.setColour (juce::TextButton::buttonOnColourId, Theme::violet().darker (0.3f));
        relGridBtn.setClickingTogglesState (true);
        relGridBtn.setTooltip ("Relative Grid: clips keep their offset from the grid when moved");
        relGridBtn.onClick = [this] { state.gridRelative = relGridBtn.getToggleState(); };
        relGridBtn.setColour (juce::TextButton::buttonOnColourId, Theme::violet().darker (0.3f));

        for (auto* l : { &counter, &subCounter, &selStart, &selEnd, &selLength })
        {
            addAndMakeVisible (l);
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::backgroundColourId, Theme::bg());
            l->setColour (juce::Label::outlineColourId, Theme::line());
            l->setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold)));
        }
        counter.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::bold)));
        counter.setColour (juce::Label::textColourId, Theme::acid());
        counter.setTooltip ("Main counter. Click to type a location in any format (5|3|240, 1:02.5, 01:00:02:10, 12.5s, 48000smp); the button beside it picks the time format.");
        counter.setEditable (true);
        counter.onTextChange = [this] { locateFromText (counter.getText()); };
        subCounter.setColour (juce::Label::textColourId, Theme::dim());
        subCounter.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
        for (auto* l : { &selStart, &selEnd, &selLength })
        {
            l->setColour (juce::Label::textColourId, Theme::text());
            l->setEditable (true);
        }
        selStart.onTextChange = [this] { editSelectionFromText (0, selStart.getText()); };
        selEnd.onTextChange = [this] { editSelectionFromText (1, selEnd.getText()); };
        selLength.onTextChange = [this] { editSelectionFromText (2, selLength.getText()); };
        selStart.setTooltip ("Edit selection start - type a time (in the main counter's format, or any format) to move it");
        selEnd.setTooltip ("Edit selection end");
        selLength.setTooltip ("Edit selection length");

        for (int i = 0; i < (int) std::size (gridValues); ++i) { gridCombo.addItem (gridValues[i].name, i + 1); nudgeCombo.addItem (gridValues[i].name, i + 1); }
        gridCombo.setSelectedId (state.gridIndex + 1, juce::dontSendNotification);
        nudgeCombo.setSelectedId (state.nudgeIndex + 1, juce::dontSendNotification);
        gridCombo.onChange = [this] { state.gridIndex = gridCombo.getSelectedId() - 1; repaintAll(); };
        nudgeCombo.onChange = [this] { state.nudgeIndex = nudgeCombo.getSelectedId() - 1; };
        gridCombo.setTooltip ("Grid value");
        nudgeCombo.setTooltip ("Nudge value (+ / - keys)");
        addAndMakeVisible (gridCombo);
        addAndMakeVisible (nudgeCombo);

        hint.setColour (juce::Label::textColourId, Theme::dim());
        hint.setFont (Theme::font (10.0f));
        hint.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (hint);
        refreshToolbar();
    }

    void layoutToolbar (juce::Rectangle<int> tb)
    {
        auto all = tb.reduced (6, 0);
        auto r = all.removeFromTop (46).withTrimmedTop (14);
        modeLabelX = r.getX();
        for (auto& b : modeBtns) { b.setBounds (r.removeFromLeft (64)); r.removeFromLeft (2); }
        r.removeFromLeft (12);
        toolsLabelX = r.getX();
        for (auto& b : toolBtns) { b.setBounds (r.removeFromLeft (60)); r.removeFromLeft (2); }
        r.removeFromLeft (12);
        zoomLabelX = r.getX();
        zoomOutBtn.setBounds (r.removeFromLeft (28)); r.removeFromLeft (2);
        zoomInBtn.setBounds (r.removeFromLeft (28)); r.removeFromLeft (2);
        fitBtn.setBounds (r.removeFromLeft (38)); r.removeFromLeft (2);
        fitSelBtn.setBounds (r.removeFromLeft (38)); r.removeFromLeft (12);
        counterX = r.getX();
        auto cnt = r.removeFromLeft (172);
        auto top = cnt.removeFromTop (cnt.getHeight() * 2 / 3 + 2);
        ctrFmtBtn.setBounds (top.removeFromRight (22));
        counter.setBounds (top);
        subFmtBtn.setBounds (cnt.removeFromRight (22));
        subCounter.setBounds (cnt);
        r.removeFromLeft (12);
        selX = r.getX();
        auto sel = r.removeFromLeft (juce::jmin (330, juce::jmax (210, r.getWidth() - 100)));
        auto sw = sel.getWidth() / 3;
        selStart.setBounds (sel.removeFromLeft (sw).reduced (1, 0));
        selEnd.setBounds (sel.removeFromLeft (sw).reduced (1, 0));
        selLength.setBounds (sel.reduced (1, 0));
        r.removeFromLeft (12);
        addTrackBtn.setBounds (r.removeFromLeft (juce::jmin (90, juce::jmax (40, r.getWidth()))).reduced (0, 3));

        auto r2 = all.removeFromTop (46).withTrimmedTop (14).withTrimmedBottom (6);
        gridX = r2.getX();
        gridCombo.setBounds (r2.removeFromLeft (170)); r2.removeFromLeft (4);
        relGridBtn.setBounds (r2.removeFromLeft (44)); r2.removeFromLeft (4);
        snapBtn.setBounds (r2.removeFromLeft (54)); r2.removeFromLeft (16);
        nudgeX = r2.getX();
        nudgeCombo.setBounds (r2.removeFromLeft (150)); r2.removeFromLeft (16);
        tabTransBtn.setBounds (r2.removeFromLeft (64)); r2.removeFromLeft (4);
        linkBtn.setBounds (r2.removeFromLeft (54)); r2.removeFromLeft (4);
        insFollowBtn.setBounds (r2.removeFromLeft (78)); r2.removeFromLeft (4);
        scrollBtn.setBounds (r2.removeFromLeft (92)); r2.removeFromLeft (12);
        hint.setBounds (r2);
    }

    void refreshToolbar()
    {
        for (int i = 0; i < 4; ++i) modeBtns[i].setToggleState ((int) state.editMode == i, juce::dontSendNotification);
        for (int i = 0; i < 7; ++i) toolBtns[i].setToggleState ((int) state.tool == i, juce::dontSendNotification);
        modeBtns[0].setButtonText (state.shuffleLock ? "SHUFFLE*" : "SHUFFLE");
        modeBtns[3].setButtonText (state.gridRelative ? "GRID~" : "GRID");
        // Grid combined with Shuffle / Slip lights the Grid button as well (Pro Tools shows both)
        modeBtns[3].setColour (juce::TextButton::buttonColourId, state.snap && state.editMode != EditMode::grid ? Theme::violet().darker (0.5f) : getLookAndFeel().findColour (juce::TextButton::buttonColourId));
        relGridBtn.setToggleState (state.gridRelative, juce::dontSendNotification);
        snapBtn.setToggleState (state.snap && state.editMode != EditMode::grid, juce::dontSendNotification);
        snapBtn.setEnabled (state.editMode == EditMode::shuffle || state.editMode == EditMode::slip);
        insFollowBtn.setToggleState (Prefs::insertionFollowsPlayback(), juce::dontSendNotification);
        scrollBtn.setButtonText (juce::StringArray { "SCROLL: OFF", "SCROLL: PAGE", "SCROLL: CONT" }[Prefs::scrollMode()]);
        ctrFmtBtn.setButtonText (juce::StringArray { "BB", "MS", "TC", "SM" }[mainFmt()]);
        subFmtBtn.setButtonText (juce::StringArray { "BB", "MS", "TC", "SM" }[subFmt()]);
        gridCombo.setSelectedId (state.gridIndex + 1, juce::dontSendNotification);
        nudgeCombo.setSelectedId (state.nudgeIndex + 1, juce::dontSendNotification);
        tabTransBtn.setToggleState (state.tabToTransients, juce::dontSendNotification);
        linkBtn.setToggleState (Prefs::linkTimelineAndEdit(), juce::dontSendNotification);
        hint.setText (juce::String (editModeName (state.editMode)) + " / " + editToolName (state.tool), juce::dontSendNotification);
        canvas.repaint();
    }

    void locateFromText (const juce::String& txt)
    {
        if (session.edit == nullptr) return;
        if (auto t = TimeFmt::parse (*session.edit, txt, (TimeFmt::Kind) mainFmt())) session.edit->getTransport().setPosition (*t);
    }

    void editSelectionFromText (int which, const juce::String& txt)
    {
        if (session.edit == nullptr || txt.trim().isEmpty()) return;
        auto& edit = *session.edit;
        const auto k = (TimeFmt::Kind) mainFmt();
        auto cur = state.timeSelection.value_or (te::TimeRange (edit.getTransport().getPosition(), edit.getTransport().getPosition()));
        if (which == 0) { if (auto s = TimeFmt::parse (edit, txt, k)) state.timeSelection = te::TimeRange (*s, juce::jmax (*s, cur.getEnd())); }
        else if (which == 1) { if (auto e = TimeFmt::parse (edit, txt, k)) state.timeSelection = te::TimeRange (juce::jmin (cur.getStart(), *e), *e); }
        else if (k == TimeFmt::Kind::barsBeats)
        {
            // length typed as bars|beats|ticks from the start (bars use the meter at the start)
            auto parts = juce::StringArray::fromTokens (txt, "|:", "");
            auto& ts = edit.tempoSequence;
            const double num = ts.getTimeSigAt (cur.getStart()).numerator.get();
            auto beats = parts[0].getDoubleValue() * num + (parts.size() > 1 ? parts[1].getDoubleValue() : 0) + (parts.size() > 2 ? parts[2].getDoubleValue() / 960.0 : 0);
            state.timeSelection = te::TimeRange (cur.getStart(), ts.toTime (ts.toBeats (cur.getStart()) + te::BeatDuration::fromBeats (beats)));
        }
        else if (auto len = TimeFmt::parse (edit, txt, k == TimeFmt::Kind::timecode ? k : TimeFmt::Kind::minSecs))
        {
            auto secs = len->inSeconds() + (k == TimeFmt::Kind::timecode ? TimeFmt::tcStartSeconds (edit) : 0.0);
            if (k == TimeFmt::Kind::samples) secs = txt.retainCharacters ("0123456789").getLargeIntValue() / TimeFmt::sampleRate (edit);
            state.timeSelection = te::TimeRange (cur.getStart(), cur.getStart() + te::TimeDuration::fromSeconds (secs));
        }
        canvas.syncLoopToSelection();
        selectionChanged();
    }

    //==========================================================================
    void selectionChanged()
    {
        for (auto& h : headers) h->repaint();
        ruler.repaint();
        canvas.repaint();
        updateScroll();
        if (Prefs::scrollToTrackOnSelect() && state.selectedRow != nullptr) scrollToTrack (state.selectedRow);
    }

    void attachToEdit()
    {
        if (watched.isValid()) watched.removeListener (this);
        state.clearAll();
        canvas.clearThumbnails();
        if (session.edit != nullptr)
        {
            watched = session.edit->state;
            watched.addListener (this);
        }
        rebuild();
    }

    void changeListenerCallback (juce::ChangeBroadcaster* b) override
    {
        if (b == &Theme::broadcaster()) { buildToolbarColours(); repaintAll(); for (auto& h : headers) h->repaint(); return; }
        if (b == &Prefs::broadcaster())
        {
            // View > Edit Window Views may have changed the header width
            if (Ruler::totalHeight() != lastRulerHeight) { resized(); repaintAll(); }
            if (EditViews::width() != lastViewsWidth)
            {
                lastViewsWidth = EditViews::width();
                for (auto& h : headers) h->viewsChanged();
                resized();
                layoutBody();
                repaintAll();
            }
            return;
        }
        attachToEdit();
    }

    void buildToolbarColours()
    {
        for (auto* l : { &counter, &subCounter, &selStart, &selEnd, &selLength })
        {
            l->setColour (juce::Label::backgroundColourId, Theme::bg());
            l->setColour (juce::Label::outlineColourId, Theme::line());
        }
        counter.setColour (juce::Label::textColourId, Theme::acid());
        subCounter.setColour (juce::Label::textColourId, Theme::dim());
        for (auto* l : { &selStart, &selEnd, &selLength }) l->setColour (juce::Label::textColourId, Theme::text());
        hint.setColour (juce::Label::textColourId, Theme::dim());
        for (auto& b : modeBtns) b.setColour (juce::TextButton::buttonOnColourId, Theme::violet());
        for (auto& b : toolBtns) b.setColour (juce::TextButton::buttonOnColourId, Theme::mint().darker (0.45f));
    }

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override           { structureDirty = true; triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int) override
    {
        if (child.hasType (te::IDs::TRACK) || child.hasType (te::IDs::FOLDERTRACK))
        {
            // Drop the header for a deleted track right now, before any repaint or timer can touch it.
            headers.erase (std::remove_if (headers.begin(), headers.end(),
                                           [&child, this] (auto& h)
                                           {
                                               if (h->getTrack().state != child) return false;
                                               state.dropReferencesTo (&h->getTrack());
                                               return true;
                                           }),
                           headers.end());
        }
        structureDirty = true;
        triggerAsyncUpdate();
    }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override            { structureDirty = true; triggerAsyncUpdate(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override
    {
        repaintCountdown = 2;
        if (id == te::IDs::hidden || id == te::IDs::process || id == juce::Identifier ("sqmMasterFader") || id == juce::Identifier ("sqmMasterHidden"))
        { structureDirty = true; triggerAsyncUpdate(); }
    }

    void handleAsyncUpdate() override
    {
        if (structureDirty) rebuild();
    }

public:
    /** Rows shown in the Edit window: tracks that are not hidden, then the Master Fader. */
    juce::Array<te::Track*> visibleRows()
    {
        juce::Array<te::Track*> out;
        if (session.edit == nullptr) return out;
        for (auto t : session.getRows (&state.collapsedFolders)) if (! t->isHidden()) out.add (t);
        if (TrackOps::hasMasterFader (session) && ! (bool) session.edit->state.getProperty ("sqmMasterHidden", false))
            if (auto m = session.edit->getMasterTrack()) out.add (m);
        return out;
    }

    void showAllTracks()
    {
        if (session.edit == nullptr) return;
        for (auto t : te::getAllTracks (*session.edit)) if (t->isHidden()) t->setHidden (false);
        session.edit->state.removeProperty ("sqmMasterHidden", nullptr);
        structureDirty = true;
        triggerAsyncUpdate();
    }

private:
    int lastViewsWidth = EditViews::width();
    int lastRulerHeight = 0;

    void rebuild()
    {
        structureDirty = false;
        auto rows = visibleRows();

        std::vector<std::unique_ptr<TrackHeader>> newHeaders;
        for (auto t : rows)
        {
            std::unique_ptr<TrackHeader> h;
            for (auto& old : headers)
                if (old != nullptr && &old->getTrack() == t) { h = std::move (old); break; }
            if (h == nullptr)
            {
                h = std::make_unique<TrackHeader> (session, state, *t);
                h->onSelect = [this] { selectionChanged(); };
                h->onStructureChanged = [this] { structureDirty = true; triggerAsyncUpdate(); };
                h->onDragReorder = [this] (te::Track& tr, const juce::MouseEvent& e) { reorderByDrag (tr, e); };
                h->onFitHeights = [this] (juce::Array<te::Track*> ts) { fitHeights (ts); };
                body.addAndMakeVisible (*h);
            }
            newHeaders.push_back (std::move (h));
        }
        headers = std::move (newHeaders);
        if (state.selectedRow != nullptr && ! te::getAllTracks (*session.edit).contains (state.selectedRow)) { state.selectedRow = nullptr; state.selectedTrack = nullptr; }
        layoutBody();
        repaintAll();
    }

    void reorderByDrag (te::Track& t, const juce::MouseEvent& e)
    {
        auto yInBody = e.getEventRelativeTo (&body).y;
        auto target = trackAtY (yInBody);
        if (target == nullptr || target == &t || session.edit == nullptr) return;
        auto rows = visibleRows();
        if (t.isMasterTrack() || target->isMasterTrack()) return;
        int from = rows.indexOf (&t), to = rows.indexOf (target);
        if (from < 0 || to < 0 || std::abs (from - to) < 1) return;
        if (juce::Time::getMillisecondCounter() - lastReorderMs < 250) return;
        lastReorderMs = juce::Time::getMillisecondCounter();
        if (to > from) session.edit->moveTrack (&t, te::TrackInsertPoint (target->getParentFolderTrack(), target));
        else
        {
            te::Track* preceding = nullptr;
            auto siblings = target->getParentFolderTrack() != nullptr ? target->getParentFolderTrack()->getAllSubTracks (false) : te::getTopLevelTracks (*session.edit);
            int ti = siblings.indexOf (target);
            if (ti > 0) preceding = siblings[ti - 1];
            session.edit->moveTrack (&t, te::TrackInsertPoint (target->getParentFolderTrack(), preceding));
        }
    }
    juce::uint32 lastReorderMs = 0;

    void layoutBody()
    {
        int y = 0;
        for (auto& h : headers)
        {
            auto hh = state.heightFor (&h->getTrack());
            h->setBounds (0, y, headerWidthNow(), hh);
            y += hh;
        }
        auto w = viewport.getMaximumVisibleWidth();
        body.setSize (w, juce::jmax (y + 120, viewport.getHeight()));
        canvas.setBounds (headerWidthNow(), 0, w - headerWidthNow(), body.getHeight());
        playhead.setBounds (canvas.getBounds());
        updateScroll();
    }

    int trackY (te::Track* t)
    {
        int y = 0;
        for (auto& h : headers)
        {
            if (&h->getTrack() == t) return y;
            y += state.heightFor (&h->getTrack());
        }
        return -1000;
    }

    te::Track* trackAtY (int yy)
    {
        int y = 0;
        for (auto& h : headers)
        {
            auto hh = state.heightFor (&h->getTrack());
            if (yy >= y && yy < y + hh) return &h->getTrack();
            y += hh;
        }
        return nullptr;
    }

    void zoom (double factor)
    {
        double centre = state.viewStart + canvas.getWidth() * 0.5 * state.secondsPerPixel;
        if (session.edit != nullptr && session.edit->getTransport().isPlaying())
            centre = session.edit->getTransport().getPosition().inSeconds();
        else if (Prefs::zoomTogglesFollowSelection() && state.timeSelection) centre = state.timeSelection->getStart().inSeconds();
        state.secondsPerPixel = juce::jlimit (0.00002, 2.0, state.secondsPerPixel * factor);
        state.viewStart = juce::jmax (0.0, centre - canvas.getWidth() * 0.5 * state.secondsPerPixel);
        repaintAll();
    }

    void repaintAll()
    {
        layoutBodyIfNeeded();
        ruler.repaint();
        canvas.repaint();
        playhead.lastX = -10;
        playhead.repaint();
        for (auto& h : headers) h->repaint();
        updateScroll();
        repaint();
    }

    void layoutBodyIfNeeded()
    {
        int total = 0;
        for (auto& h : headers) total += state.heightFor (&h->getTrack());
        if (body.getHeight() < total + 120 || (headers.size() && headers.back()->getBottom() != total)) layoutBody();
    }

    void updateScroll()
    {
        if (session.edit == nullptr) return;
        auto total = juce::jmax (state.viewStart + canvas.getWidth() * state.secondsPerPixel, session.edit->getLength().inSeconds() + 60.0);
        hScroll.setRangeLimits (0.0, total, juce::dontSendNotification);
        hScroll.setCurrentRange (state.viewStart, canvas.getWidth() * state.secondsPerPixel, juce::dontSendNotification);
    }

    void scrollBarMoved (juce::ScrollBar*, double newStart) override
    {
        state.viewStart = newStart;
        ruler.repaint();
        canvas.repaint();
    }

    /** Runs at the display's refresh rate: the playhead and the scrolling are drawn every frame, like Pro Tools. */
    void onVBlank()
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        const double pos = tr.getPosition().inSeconds();
        const bool moving = tr.isPlaying() || tr.isRecording();
        if (moving && ! canvas.isDragging())
        {
            const int mode = Prefs::scrollMode();
            const double widthSecs = canvas.getWidth() * state.secondsPerPixel;
            if (mode == 2)
            {
                // Continuous: the session scrolls under a centred playhead
                const double want = juce::jmax (0.0, pos - widthSecs * 0.5);
                if (std::abs (want - state.viewStart) > state.secondsPerPixel * 0.5) { state.viewStart = want; canvas.repaint(); ruler.repaint(); updateScroll(); }
            }
            else if (mode == 1)
            {
                auto px = state.timeToX (pos);
                if (px > canvas.getWidth() - 40 || px < 0) { state.viewStart = juce::jmax (0.0, pos - 40 * state.secondsPerPixel); repaintAll(); }
            }
        }
        playhead.update();
        const int rx = headerWidthNow() + juce::roundToInt (state.timeToX (pos));
        if (rx != lastRulerPlayheadX)
        {
            ruler.repaint (lastRulerPlayheadX - 8, 0, 17, ruler.getHeight());
            ruler.repaint (rx - 8, 0, 17, ruler.getHeight());
            lastRulerPlayheadX = rx;
        }
    }
    int lastRulerPlayheadX = -100;
    juce::VBlankAttachment vblank { this, [this] { onVBlank(); } };

    void timerCallback() override
    {
        if (session.edit == nullptr) return;
        auto& tr = session.edit->getTransport();
        auto pos = tr.getPosition();
        auto& edit = *session.edit;
        const auto mk = (TimeFmt::Kind) mainFmt();
        if (! counter.isBeingEdited()) counter.setText (TimeFmt::format (edit, pos, mk), juce::dontSendNotification);
        subCounter.setText (TimeFmt::format (edit, pos, (TimeFmt::Kind) subFmt()), juce::dontSendNotification);
        if (state.timeSelection)
        {
            if (! selStart.isBeingEdited()) selStart.setText (TimeFmt::format (edit, state.timeSelection->getStart(), mk), juce::dontSendNotification);
            if (! selEnd.isBeingEdited()) selEnd.setText (TimeFmt::format (edit, state.timeSelection->getEnd(), mk), juce::dontSendNotification);
            if (! selLength.isBeingEdited()) selLength.setText (TimeFmt::formatLength (edit, state.timeSelection->getStart(), state.timeSelection->getEnd(), mk), juce::dontSendNotification);
        }
        else
        {
            if (! selStart.isBeingEdited()) selStart.setText (TimeFmt::format (edit, pos, mk), juce::dontSendNotification);
            if (! selEnd.isBeingEdited()) selEnd.setText (TimeFmt::format (edit, pos, mk), juce::dontSendNotification);
            if (! selLength.isBeingEdited()) selLength.setText (TimeFmt::formatLength (edit, pos, pos, mk), juce::dontSendNotification);
        }

        if (repaintCountdown > 0 && --repaintCountdown == 0)
        {
            canvas.repaint();
            for (auto& h : headers) h->repaint();
        }
        if (++toolbarSync % 15 == 0) refreshToolbar();
    }
    int toolbarSync = 0;

    void showAddTrackMenu()
    {
        QuickNew::build (session).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addTrackBtn), [this] (int r)
        {
            if (r > 0 && session.quickNewHook) session.quickNewHook (r, 0);
        });
    }
};
