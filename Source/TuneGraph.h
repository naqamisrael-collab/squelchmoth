#pragma once
// Moth Tune Graph mode - the note-by-note pitch editor.
//
// Unlike a tracking-pass editor, Graph mode reads the track's audio files straight from disk (Squelchmoth owns the
// timeline, so there is nothing to "record in" and no ARA needed). It shows the sung pitch curve and the notes found
// in it; you drag notes to new pitches, split and join them, and set per-note Retune, Vibrato, Formant and Drift.
// Graph notes override Moth Tune's automatic mode only where they exist. Every change is undoable and is saved in
// the session with the plug-in.

#include "Theme.h"
#include "SqVocal.h"
#include "Scales.h"

namespace TuneGraph
{
    struct Curve { std::vector<double> t; std::vector<float> note; double hop = 0.0025; };

    /** Pitch curve (edit time, MIDI note) of every audio clip on a track, read from the clips' files. */
    inline Curve analyseTrack (te::AudioTrack& track, int voiceType, float ref = 440.0f, std::function<bool()> cancelled = {})
    {
        Curve c;
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::vector<std::pair<double, float>> pts;
        for (auto clip : track.getClips())
        {
            auto ac = dynamic_cast<te::AudioClipBase*> (clip);
            if (ac == nullptr) continue;
            auto file = ac->getAudioFile().getFile();
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
            if (reader == nullptr) continue;
            const double fsr = reader->sampleRate;
            const auto pos = ac->getPosition();
            const double speed = juce::jmax (0.01, ac->getSpeedRatio());
            const double srcStart = pos.getOffset().inSeconds() * speed;
            const double srcLen = pos.getLength().inSeconds() * speed;
            const juce::int64 s0 = juce::jmax ((juce::int64) 0, (juce::int64) (srcStart * fsr)), n = juce::jmin ((juce::int64) (srcLen * fsr), reader->lengthInSamples - s0);
            if (n <= 0) continue;
            SqVocal::PitchTracker tr; tr.prepare (fsr);
            const auto& vt = SqVocal::voiceTypes()[(size_t) juce::jlimit (0, 5, voiceType)];
            tr.setRange (vt.lo, vt.hi); tr.setTracking (50.0f);
            const int chunk = 32768;
            juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), chunk);
            juce::int64 base = -1;
            for (juce::int64 done = 0; done < n; done += chunk)
            {
                if (cancelled && cancelled()) return c;
                const int m = (int) juce::jmin ((juce::int64) chunk, n - done);
                reader->read (&buf, 0, m, s0 + done, true, true);
                for (int i = 0; i < m; ++i)
                {
                    float x = 0;
                    for (int ch = 0; ch < buf.getNumChannels(); ++ch) x += buf.getSample (ch, i);
                    x /= (float) buf.getNumChannels();
                    if (tr.push (x))
                    {
                        const auto& f = tr.latest();
                        if (base < 0) base = tr.position() - (done + i + 1);
                        const double srcT = (double) (f.pos - base) / fsr;            // seconds into the read region
                        const double et = pos.getStart().inSeconds() + srcT / speed;
                        if (et < pos.getStart().inSeconds() || et > pos.getEnd().inSeconds()) continue;
                        pts.push_back ({ et, f.voiced ? SqVocal::hzToNote (f.hz, ref) : -1.0f });
                    }
                }
            }
        }
        std::sort (pts.begin(), pts.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        for (auto& p : pts) { c.t.push_back (p.first); c.note.push_back (p.second); }
        if (c.t.size() > 1) c.hop = (c.t.back() - c.t.front()) / (double) (c.t.size() - 1);
        return c;
    }

    /** Notes found in a curve: stable voiced runs, split where the pitch jumps. */
    inline std::vector<SqVocal::GraphNote> findNotes (const Curve& c, float retuneMs = 20.0f)
    {
        std::vector<SqVocal::GraphNote> out;
        const size_t N = c.t.size();
        size_t i = 0;
        while (i < N)
        {
            if (c.note[i] <= 0) { ++i; continue; }
            size_t j = i;
            std::vector<float> vals { c.note[i] };
            while (j + 1 < N && c.note[j + 1] > 0 && c.t[j + 1] - c.t[j] < 0.03)
            {
                std::vector<float> tail (vals.end() - (long) std::min<size_t> (vals.size(), 20), vals.end());
                std::nth_element (tail.begin(), tail.begin() + (long) tail.size() / 2, tail.end());
                const float med = tail[tail.size() / 2];
                // a jump of more than ~0.7 semitone held for 40 ms starts a new note
                if (std::abs (c.note[j + 1] - med) > 0.7f)
                {
                    size_t k = j + 1; int held = 0;
                    while (k < N && c.note[k] > 0 && std::abs (c.note[k] - med) > 0.7f && held < 16) { ++k; ++held; }
                    if (held >= 16) break;
                }
                ++j; vals.push_back (c.note[j]);
            }
            if (c.t[j] - c.t[i] >= 0.06)
            {
                std::vector<float> v (vals);
                std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
                SqVocal::GraphNote gn;
                gn.start = c.t[i] - c.hop * 0.5; gn.end = c.t[j] + c.hop * 0.5;
                gn.orig = v[v.size() / 2]; gn.pitch = gn.orig; gn.retuneMs = retuneMs; gn.vibScale = 1; gn.formantSt = 0; gn.drift = 1;
                out.push_back (gn);
            }
            i = j + 1;
        }
        return out;
    }

    /** Moves notes toward the nearest allowed pitch. strength 0..1. */
    inline void correctToScale (std::vector<SqVocal::GraphNote>& notes, int mask, float strength, const std::vector<int>* only = nullptr)
    {
        for (int i = 0; i < (int) notes.size(); ++i)
        {
            if (only != nullptr && std::find (only->begin(), only->end(), i) == only->end()) continue;
            auto& n = notes[(size_t) i];
            const float target = (float) SqVocal::nearestInMask (n.orig, mask);
            n.pitch = n.orig + (target - n.orig) * juce::jlimit (0.0f, 1.0f, strength);
        }
    }

    // The last analysis of each Moth Tune instance (kept in memory; re-analysing is quick).
    inline std::map<juce::String, Curve>& cache()   { static std::map<juce::String, Curve> c; return c; }
    inline juce::String cacheKey (te::Plugin& p)     { return p.itemID.toString(); }

    /** Analyse the plug-in's track, cache the curve, and (if asked) replace the graph with the notes found. */
    inline int analyse (SqTunePlugin& tune, bool replaceNotes, juce::String& err)
    {
        auto at = dynamic_cast<te::AudioTrack*> (tune.getOwnerTrack());
        if (at == nullptr) { err = "Moth Tune is not on an audio track"; return -1; }
        const float ref = 440.0f * std::pow (2.0f, tune.bank[SqTunePlugin::detune] / 1200.0f);
        auto c = analyseTrack (*at, tune.voiceType.get(), ref);
        if (c.t.empty()) { err = "No audio clips with readable files on " + at->getName(); return -1; }
        cache()[cacheKey (tune)] = c;
        if (replaceNotes)
        {
            auto notes = findNotes (c, juce::jmax (5.0f, tune.bank[SqTunePlugin::retune]));
            tune.edit.getUndoManager().beginNewTransaction ("Graph: analyse");
            tune.setGraphNotes (notes, &tune.edit.getUndoManager());
            return (int) notes.size();
        }
        return (int) tune.getGraphNotes().size();
    }

    /** Key detection from the session's MIDI and this track's singing (Krumhansl-Schmuckler). */
    inline SqScales::KeyGuess detectKey (te::Edit& edit, SqTunePlugin* tune)
    {
        std::array<double, 12> h {};
        for (auto t : te::getAudioTracks (edit))
            for (auto clip : t->getClips())
                if (auto mc = dynamic_cast<te::MidiClip*> (clip))
                    for (auto* n : mc->getSequence().getNotes())
                        h[(size_t) (n->getNoteNumber() % 12)] += n->getLengthBeats().inBeats();
        if (tune != nullptr)
        {
            auto it = cache().find (cacheKey (*tune));
            if (it == cache().end()) { juce::String e; analyse (*tune, false, e); it = cache().find (cacheKey (*tune)); }
            if (it != cache().end())
                for (auto v : it->second.note) if (v > 0) h[(size_t) (((juce::roundToInt (v) % 12) + 12) % 12)] += 0.01;
        }
        return SqScales::guessKey (h);
    }
    inline void applyKey (te::Edit& edit, SqTunePlugin* tune, const SqScales::KeyGuess& k)
    {
        auto* um = &edit.getUndoManager();
        edit.state.setProperty ("sqmKeyRoot", k.root, um);
        edit.state.setProperty ("sqmKeyScale", k.minor ? 2 : 1, um);
        if (tune != nullptr) { tune->keyRoot.setValue (k.root, um); tune->scaleIndex.setValue (k.minor ? 2 : 1, um); }
    }

    //==========================================================================
    class Editor : public juce::Component, private juce::Timer
    {
    public:
        explicit Editor (SqTunePlugin& p) : tune (&p)
        {
            for (auto* b : { &analyseBtn, &correctAllBtn, &correctSelBtn, &splitBtn, &joinBtn, &deleteBtn, &clearBtn, &fitBtn })
            { addAndMakeVisible (*b); b->setColour (juce::TextButton::buttonColourId, Theme::panel2()); }
            analyseBtn.setTooltip ("Read the track's audio and find its notes (replaces the graph)");
            analyseBtn.onClick = [this] { juce::String e; const int n = analyse (*tune, true, e); status = n < 0 ? e : "Found " + juce::String (n) + " notes"; reload(); fit(); };
            correctAllBtn.onClick = [this] { correct (false); };
            correctSelBtn.onClick = [this] { correct (true); };
            splitBtn.setTooltip ("Split the selected note at the playhead (or double-click a note where you want to cut it)");
            splitBtn.onClick = [this] { if (selection.size() == 1) splitAt (*selection.begin(), tune->edit.getTransport().getPosition().inSeconds()); };
            joinBtn.onClick = [this] { join(); };
            deleteBtn.onClick = [this] { removeSelected(); };
            clearBtn.onClick = [this] { notes.clear(); selection.clear(); commit ("Graph: clear"); };
            fitBtn.onClick = [this] { fit(); };
            auto setupSlider = [this] (juce::Slider& s, double lo, double hi, double def, const juce::String& suffix, std::function<void (SqVocal::GraphNote&, float)> apply)
            {
                s.setSliderStyle (juce::Slider::LinearBar); s.setRange (lo, hi, 0.01); s.setValue (def, juce::dontSendNotification);
                s.setTextValueSuffix (suffix); s.setColour (juce::Slider::trackColourId, tune != nullptr ? juce::Colour (0xff19e3ff).withAlpha (0.55f) : Theme::violet());
                s.onDragStart = [this] { tune->edit.getUndoManager().beginNewTransaction ("Graph: note settings"); };
                s.onValueChange = [this, &s, apply]
                {
                    for (int i : selection) if (i < (int) notes.size()) apply (notes[(size_t) i], (float) s.getValue());
                    tune->setGraphNotes (notes, &tune->edit.getUndoManager());
                    repaint();
                };
                addAndMakeVisible (s);
            };
            setupSlider (strength, 0, 100, 100, " % correct", [] (SqVocal::GraphNote&, float) {});
            strength.onValueChange = nullptr; strength.onDragStart = nullptr;
            setupSlider (retune, 0, 400, 20, " ms retune", [] (SqVocal::GraphNote& n, float v) { n.retuneMs = v; });
            setupSlider (vib, 0, 200, 100, " % vibrato", [] (SqVocal::GraphNote& n, float v) { n.vibScale = v / 100.0f; });
            setupSlider (formant, -12, 12, 0, " st formant", [] (SqVocal::GraphNote& n, float v) { n.formantSt = v; });
            setupSlider (drift, 0, 100, 100, " % drift", [] (SqVocal::GraphNote& n, float v) { n.drift = v / 100.0f; });
            snapBtn.setToggleState (true, juce::dontSendNotification); addAndMakeVisible (snapBtn);
            setWantsKeyboardFocus (true);
            reload();
            if (! notes.empty() || hasCurve()) fit();
            startTimerHz (20);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto bar = r.removeFromTop (28);
            for (auto* b : { &analyseBtn, &correctAllBtn, &correctSelBtn }) { b->setBounds (bar.removeFromLeft (b == &analyseBtn ? 96 : 120)); bar.removeFromLeft (4); }
            strength.setBounds (bar.removeFromLeft (130)); bar.removeFromLeft (10);
            for (auto* b : { &splitBtn, &joinBtn, &deleteBtn, &clearBtn, &fitBtn }) { b->setBounds (bar.removeFromLeft (64)); bar.removeFromLeft (4); }
            snapBtn.setBounds (bar.removeFromLeft (90));
            auto bar2 = r.removeFromTop (30).withTrimmedTop (4);
            for (auto* s : { &retune, &vib, &formant, &drift }) { s->setBounds (bar2.removeFromLeft (180)); bar2.removeFromLeft (6); }
            r.removeFromTop (6);
            canvas = r;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::bg());
            if (tune == nullptr) return;
            const auto acc = juce::Colour (0xff19e3ff);
            auto c = canvas.toFloat();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0716), c.getX(), c.getY(), juce::Colour (0xff10202a), c.getRight(), c.getBottom(), false));
            g.fillRect (c);
            const int mask = tune->allowedMask(), root = tune->keyRoot.get();
            g.saveState(); g.reduceClipRegion (canvas);
            // semitone lanes
            for (int n = (int) std::floor (lo); n <= (int) std::ceil (hi); ++n)
            {
                const int pc = ((n % 12) + 12) % 12;
                const float y = yFor ((float) n), hgt = rowH();
                if ((mask >> pc) & 1) { g.setColour (acc.withAlpha (pc == root ? 0.16f : 0.07f)); g.fillRect (c.getX(), y - hgt * 0.5f, c.getWidth(), hgt); }
                g.setColour (juce::Colours::white.withAlpha (pc == 0 ? 0.15f : 0.04f)); g.drawHorizontalLine ((int) (y + hgt * 0.5f), c.getX(), c.getRight());
            }
            // bar lines
            auto& ts = tune->edit.tempoSequence;
            const int bpb = juce::jmax (1, ts.getTimeSig (0)->numerator.get());
            const double b0 = ts.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, t0))).inBeats(), b1 = ts.toBeats (te::TimePosition::fromSeconds (t1)).inBeats();
            for (int bar = (int) std::floor (b0 / bpb); bar <= (int) std::ceil (b1 / bpb); ++bar)
            {
                const double tt = ts.toTime (te::BeatPosition::fromBeats (bar * bpb)).inSeconds();
                const float x = xFor (tt);
                g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawVerticalLine ((int) x, c.getY(), c.getBottom());
                g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (Theme::font (9.0f));
                g.drawText (juce::String (bar + 1), (int) x + 3, (int) c.getY() + 2, 30, 12, juce::Justification::left);
            }
            // notes
            for (int i = 0; i < (int) notes.size(); ++i)
            {
                const auto& n = notes[(size_t) i];
                const bool sel = selection.count (i) > 0;
                auto r = juce::Rectangle<float> (xFor (n.start), yFor (n.pitch) - rowH() * 0.45f, juce::jmax (3.0f, xFor (n.end) - xFor (n.start)), rowH() * 0.9f);
                const bool moved = std::abs (n.pitch - n.orig) > 0.02f;
                g.setColour ((moved ? acc : juce::Colour (0xffb08cff)).withAlpha (sel ? 0.75f : 0.45f));
                g.fillRoundedRectangle (r, 4.0f);
                g.setColour (sel ? juce::Colours::white : acc.withAlpha (0.8f));
                g.drawRoundedRectangle (r, 4.0f, sel ? 2.0f : 1.0f);
                if (moved) { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawLine (r.getCentreX(), yFor (n.orig), r.getCentreX(), r.getCentreY(), 1.0f); }
                if (r.getWidth() > 34)
                {
                    g.setColour (juce::Colours::white); g.setFont (Theme::font (10.5f, true));
                    const float cents = (n.pitch - std::round (n.pitch)) * 100.0f;
                    g.drawText (Skins_noteLabel (n.pitch) + (std::abs (cents) >= 1 ? juce::String (cents >= 0 ? " +" : " ") + juce::String (juce::roundToInt (cents)) : juce::String()), r.toNearestInt().reduced (4, 0), juce::Justification::centredLeft);
                }
            }
            // curves: detected (white) and the corrected result (cyan)
            if (auto cv = curve())
            {
                juce::Path pin, pout; bool pi = false, po = false;
                for (size_t k = 0; k < cv->t.size(); ++k)
                {
                    const double t = cv->t[k]; if (t < t0 - 0.1 || t > t1 + 0.1) { pi = po = false; continue; }
                    const float v = cv->note[k];
                    if (v <= 0) { pi = po = false; continue; }
                    const float x = xFor (t);
                    if (! pi) { pin.startNewSubPath (x, yFor (v)); pi = true; } else pin.lineTo (x, yFor (v));
                    const float o = corrected (*cv, k);
                    if (! po) { pout.startNewSubPath (x, yFor (o)); po = true; } else pout.lineTo (x, yFor (o));
                }
                g.setColour (juce::Colours::white.withAlpha (0.45f)); g.strokePath (pin, juce::PathStrokeType (1.3f));
                g.setColour (acc.withAlpha (0.18f)); g.strokePath (pout, juce::PathStrokeType (5.0f));
                g.setColour (acc); g.strokePath (pout, juce::PathStrokeType (1.8f));
            }
            // rubber band
            if (! band.isEmpty()) { g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRect (band); g.setColour (juce::Colours::white.withAlpha (0.6f)); g.drawRect (band); }
            // playhead
            const float px = xFor (tune->edit.getTransport().getPosition().inSeconds());
            g.setColour (Theme::amber()); g.drawLine (px, c.getY(), px, c.getBottom(), 1.5f);
            g.restoreState();
            // gutter labels
            g.saveState(); g.reduceClipRegion (canvas);
            g.setFont (Theme::font (9.0f));
            for (int n = (int) std::floor (lo); n <= (int) std::ceil (hi); ++n)
            {
                const int pc = ((n % 12) + 12) % 12;
                g.setColour (((mask >> pc) & 1) ? juce::Colours::white.withAlpha (0.8f) : juce::Colours::white.withAlpha (0.3f));
                if (rowH() >= 9 || pc == 0) g.drawText (Skins_noteLabel ((float) n), canvas.getX() + 3, (int) yFor ((float) n) - 6, 36, 12, juce::Justification::left);
            }
            g.restoreState();
            g.setColour (Theme::dim()); g.setFont (Theme::font (10.0f));
            g.drawText (status.isNotEmpty() ? status : juce::String ("Analyse reads the track's audio. Drag notes up/down (Alt = fine), double-click to split, J joins, Delete removes, arrows move a semitone. Cmd-wheel zooms."),
                        getLocalBounds().removeFromBottom (16).reduced (10, 0), juce::Justification::centredLeft);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            if (! canvas.contains (e.getPosition())) return;
            const int hit = noteAt (e.position);
            dragMode = none;
            if (hit >= 0)
            {
                if (e.mods.isShiftDown()) { if (selection.count (hit)) selection.erase (hit); else selection.insert (hit); }
                else if (! selection.count (hit)) { selection.clear(); selection.insert (hit); }
                dragStartPitch.clear();
                for (int i : selection) dragStartPitch[i] = notes[(size_t) i].pitch;
                dragMode = movePitch;
                tune->edit.getUndoManager().beginNewTransaction ("Graph: move notes");
                syncSliders();
            }
            else
            {
                if (! e.mods.isShiftDown()) selection.clear();
                dragMode = rubber; band = {};
            }
            repaint();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragMode == movePitch)
            {
                float d = -(float) e.getDistanceFromDragStartY() / rowH();
                if (snapBtn.getToggleState() && ! e.mods.isAltDown()) d = std::round (d);
                for (auto& [i, p0] : dragStartPitch)
                {
                    float np = p0 + d;
                    if (snapBtn.getToggleState() && ! e.mods.isAltDown() && std::abs (p0 - std::round (p0)) > 0.01f && d != 0) np = std::round (p0) + d;
                    notes[(size_t) i].pitch = np;
                }
                tune->setGraphNotes (notes, &tune->edit.getUndoManager());
                repaint();
            }
            else if (dragMode == rubber)
            {
                band = juce::Rectangle<float> (e.mouseDownPosition, e.position).toNearestInt().toFloat();
                selection.clear();
                for (int i = 0; i < (int) notes.size(); ++i)
                    if (noteRect (i).intersects (band)) selection.insert (i);
                syncSliders(); repaint();
            }
        }
        void mouseUp (const juce::MouseEvent&) override   { dragMode = none; band = {}; repaint(); }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int hit = noteAt (e.position);
            if (hit >= 0) splitAt (hit, tFor (e.position.x));
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            {
                const double at = tFor (e.position.x), f = w.deltaY > 0 ? 0.8 : 1.25;
                t0 = at - (at - t0) * f; t1 = at + (t1 - at) * f;
            }
            else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
            {
                const double d = (t1 - t0) * (w.deltaX != 0 ? -w.deltaX : -w.deltaY) * 0.5;
                t0 += d; t1 += d;
            }
            else { const float d = w.deltaY * 6.0f; lo += d; hi += d; }
            repaint();
        }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { removeSelected(); return true; }
            if (k.getTextCharacter() == 'j' || k.getTextCharacter() == 'J') { join(); return true; }
            if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)) { selection.clear(); for (int i = 0; i < (int) notes.size(); ++i) selection.insert (i); repaint(); return true; }
            if (k.isKeyCode (juce::KeyPress::upKey) || k.isKeyCode (juce::KeyPress::downKey))
            {
                const float d = (k.isKeyCode (juce::KeyPress::upKey) ? 1.0f : -1.0f) * (k.getModifiers().isAltDown() ? 0.1f : 1.0f);
                tune->edit.getUndoManager().beginNewTransaction ("Graph: nudge");
                for (int i : selection) notes[(size_t) i].pitch += d;
                tune->setGraphNotes (notes, &tune->edit.getUndoManager()); repaint(); return true;
            }
            return false;
        }

        SqTunePlugin* tune;

    private:
        juce::TextButton analyseBtn { "ANALYSE" }, correctAllBtn { "CORRECT ALL" }, correctSelBtn { "CORRECT SELECTED" }, splitBtn { "SPLIT" }, joinBtn { "JOIN" }, deleteBtn { "DELETE" }, clearBtn { "CLEAR" }, fitBtn { "FIT" };
        juce::ToggleButton snapBtn { "Snap" };
        juce::Slider strength, retune, vib, formant, drift;
        juce::Rectangle<int> canvas;
        std::vector<SqVocal::GraphNote> notes;
        std::set<int> selection;
        std::map<int, float> dragStartPitch;
        enum DragMode { none, movePitch, rubber } dragMode = none;
        juce::Rectangle<float> band;
        double t0 = 0, t1 = 16; float lo = 48, hi = 76;
        juce::String status;
        size_t lastNoteCount = 0;

        static juce::String Skins_noteLabel (float note)
        {
            const int n = juce::roundToInt (note);
            return SqScales::noteNames()[((n % 12) + 12) % 12] + juce::String (n / 12 - 1);
        }
        float rowH() const              { return (float) canvas.getHeight() / juce::jmax (1.0f, hi - lo); }
        float yFor (float n) const      { return (float) canvas.getBottom() - (n - lo) * rowH(); }
        float xFor (double t) const     { return (float) canvas.getX() + 40.0f + (float) ((t - t0) / juce::jmax (0.01, t1 - t0)) * ((float) canvas.getWidth() - 40.0f); }
        double tFor (float x) const     { return t0 + (double) (x - (float) canvas.getX() - 40.0f) / juce::jmax (1.0f, (float) canvas.getWidth() - 40.0f) * (t1 - t0); }
        juce::Rectangle<float> noteRect (int i) const
        {
            const auto& n = notes[(size_t) i];
            return { xFor (n.start), yFor (n.pitch) - rowH() * 0.45f, juce::jmax (3.0f, xFor (n.end) - xFor (n.start)), rowH() * 0.9f };
        }
        int noteAt (juce::Point<float> p) const
        {
            for (int i = (int) notes.size() - 1; i >= 0; --i) if (noteRect (i).expanded (1.0f, 2.0f).contains (p)) return i;
            return -1;
        }
        const Curve* curve() const      { auto it = cache().find (cacheKey (*tune)); return it != cache().end() ? &it->second : nullptr; }
        bool hasCurve() const           { return curve() != nullptr; }

        float corrected (const Curve& cv, size_t k) const
        {
            const double t = cv.t[k]; const float v = cv.note[k];
            for (auto& n : notes)
                if (t >= n.start && t < n.end)
                {
                    // local centre: average over ~150 ms of voiced frames
                    double s = 0; int c = 0;
                    const int span = (int) (0.075 / juce::jmax (0.001, cv.hop));
                    for (int j = juce::jmax (0, (int) k - span); j <= juce::jmin ((int) cv.t.size() - 1, (int) k + span); ++j) if (cv.note[(size_t) j] > 0) { s += cv.note[(size_t) j]; ++c; }
                    const float centre = c > 0 ? (float) (s / c) : v;
                    return n.pitch + (v - centre) * n.vibScale + (centre - n.orig) * n.drift;
                }
            return v;
        }

        void reload()   { notes = tune->getGraphNotes(); lastNoteCount = notes.size(); for (auto it = selection.begin(); it != selection.end();) it = *it >= (int) notes.size() ? selection.erase (it) : std::next (it); repaint(); }
        void commit (const juce::String& what)
        {
            tune->edit.getUndoManager().beginNewTransaction (what);
            tune->setGraphNotes (notes, &tune->edit.getUndoManager());
            repaint();
        }
        void correct (bool selectedOnly)
        {
            std::vector<int> sel (selection.begin(), selection.end());
            correctToScale (notes, tune->allowedMask(), (float) strength.getValue() / 100.0f, selectedOnly ? &sel : nullptr);
            commit ("Graph: correct to scale");
            status = (selectedOnly ? "Corrected " + juce::String ((int) sel.size()) : "Corrected all " + juce::String ((int) notes.size())) + " notes to the scale";
        }
        void splitAt (int i, double t)
        {
            if (i < 0 || i >= (int) notes.size()) return;
            auto n = notes[(size_t) i];
            if (t <= n.start + 0.03 || t >= n.end - 0.03) return;
            auto second = n; second.start = t; notes[(size_t) i].end = t;
            notes.insert (notes.begin() + i + 1, second);
            selection.clear(); selection.insert (i + 1);
            commit ("Graph: split note");
        }
        void join()
        {
            if (selection.size() < 2) return;
            const int a = *selection.begin(), b = *selection.rbegin();
            auto merged = notes[(size_t) a];
            merged.end = notes[(size_t) b].end;
            double w = 0, sumO = 0, sumP = 0;
            for (int i = a; i <= b; ++i) { const double d = notes[(size_t) i].end - notes[(size_t) i].start; w += d; sumO += d * notes[(size_t) i].orig; sumP += d * notes[(size_t) i].pitch; }
            merged.orig = (float) (sumO / w); merged.pitch = (float) (sumP / w);
            notes.erase (notes.begin() + a, notes.begin() + b + 1);
            notes.insert (notes.begin() + a, merged);
            selection.clear(); selection.insert (a);
            commit ("Graph: join notes");
        }
        void removeSelected()
        {
            for (auto it = selection.rbegin(); it != selection.rend(); ++it) if (*it < (int) notes.size()) notes.erase (notes.begin() + *it);
            selection.clear(); commit ("Graph: delete notes");
        }
        void fit()
        {
            double a = 1e9, b = -1e9; float l = 1e9f, h = -1e9f;
            for (auto& n : notes) { a = juce::jmin (a, n.start); b = juce::jmax (b, n.end); l = juce::jmin (l, n.pitch, n.orig); h = juce::jmax (h, n.pitch, n.orig); }
            if (auto cv = curve()) for (size_t k = 0; k < cv->t.size(); ++k) if (cv->note[k] > 0) { a = juce::jmin (a, cv->t[k]); b = juce::jmax (b, cv->t[k]); l = juce::jmin (l, cv->note[k]); h = juce::jmax (h, cv->note[k]); }
            if (b > a) { t0 = a - 0.3; t1 = b + 0.3; lo = l - 3; hi = h + 3; }
            if (hi - lo < 14) { const float m = (hi + lo) * 0.5f; lo = m - 7; hi = m + 7; }
            repaint();
        }
        void syncSliders()
        {
            if (selection.empty()) return;
            const auto& n = notes[(size_t) *selection.begin()];
            retune.setValue (n.retuneMs, juce::dontSendNotification); vib.setValue (n.vibScale * 100.0, juce::dontSendNotification);
            formant.setValue (n.formantSt, juce::dontSendNotification); drift.setValue (n.drift * 100.0, juce::dontSendNotification);
        }
        void timerCallback() override
        {
            // follow undo / Claude edits
            if (dragMode == none)
            {
                auto fresh = tune->getGraphNotes();
                bool same = fresh.size() == notes.size();
                for (size_t i = 0; same && i < fresh.size(); ++i) same = std::abs (fresh[i].pitch - notes[i].pitch) < 1e-4f && std::abs (fresh[i].start - notes[i].start) < 1e-6;
                if (! same) reload();
            }
            repaint();
        }
    };

    class Window : public juce::DocumentWindow
    {
    public:
        explicit Window (SqTunePlugin& p)
            : DocumentWindow ("Moth Tune Graph - " + (p.getOwnerTrack() != nullptr ? p.getOwnerTrack()->getName() : juce::String()), Theme::bg(), DocumentWindow::allButtons),
              pluginRef (&p), editor (p)
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (&editor, false);
            setResizable (true, true);
            centreWithSize (1280, 720);
            setVisible (true);
            toFront (true);
        }
        void closeButtonPressed() override   { if (onClose) onClose(); }
        std::function<void()> onClose;
        te::Plugin::Ptr pluginRef;
    private:
        Editor editor;
    };
}
