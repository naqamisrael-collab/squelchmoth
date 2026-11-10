#pragma once
// Squelchmoth - Signature plugin interfaces.
//
// Every Signature plugin gets its own face: Higgsfield-rendered artwork behind a glass panel, a family-specific
// visualiser (EQ curve, compressor transfer curve and activity meter, reverb cloud, delay taps, LFO scope,
// waveshaper, oscillators + envelopes, sampler zones...), knobs in the family's style, and a preset bank with
// Basic / Advanced sections, user presets, A/B compare and bypass. Non-automatable settings (chorus depth,
// phaser rate, delay time, filter mode, pitch algorithm, synth wave shapes...) are exposed as real controls.

#include "Theme.h"
#include "Signature.h"
#include "Prefs.h"
#include "BinaryData.h"
#include "SqInstruments.h"
#include "SqVocalFx.h"
#include "SqApex.h"
#include "SqColour.h"
#include "KitInfo.h"

namespace Skins
{
    inline juce::File& presetsRoot()   { static juce::File f; return f; }   // set by the app: <Sessions>/Plugin Presets

    inline juce::Image artFor (const juce::String& key)
    {
        static std::map<juce::String, juce::Image> cache;
        auto it = cache.find (key);
        if (it != cache.end()) return it->second;
        juce::Image img;
        if (key.isNotEmpty())
        {
            int size = 0;
            if (auto data = BinaryData::getNamedResource (("pa_" + key + "_jpg").toRawUTF8(), size))
                img = juce::ImageFileFormat::loadFrom (data, (size_t) size);
        }
        cache[key] = img;
        return img;
    }

    //==============================================================================
    /** A control handle: one normalised value with text, backed by an automatable parameter or a plain state value. */
    struct Handle
    {
        juce::String name;                          // the name used in specs
        juce::String label;                         // what is printed under the knob
        std::function<float()> get;                 // normalised 0..1
        std::function<void (float)> set;
        std::function<juce::String()> text;
        std::function<void()> gestureBegin, gestureEnd;
        te::AutomatableParameter* param = nullptr;
        juce::StringArray choices;                  // non-empty -> choice control
        bool toggle = false;
        bool bipolar = false;                       // knob fills from the centre (pan, gain, tune...)
        float defaultNorm = 0.0f;

        float natural (float lo, float hi) const    { return lo + (hi - lo) * get(); }
    };

    inline std::unique_ptr<Handle> fromParam (te::AutomatableParameter& p, juce::String label = {})
    {
        auto h = std::make_unique<Handle>();
        h->name = p.getParameterName();
        h->label = label.isNotEmpty() ? label : p.getParameterName();
        h->param = &p;
        auto range = p.getValueRange();
        // frequency parameters with a linear engine range get a logarithmic knob so 100 Hz isn't crammed at the start
        const bool logFreq = (p.getParameterName().containsIgnoreCase ("freq") || p.getParameterName().containsIgnoreCase ("cut")) && range.getStart() > 0.0f && range.getEnd() / range.getStart() > 50.0f;
        if (logFreq)
        {
            const float lo = range.getStart(), hi = range.getEnd();
            h->get = [&p, lo, hi] { return juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (lo, p.getCurrentValue()) / lo) / std::log (hi / lo)); };
            h->set = [&p, lo, hi] (float v) { p.setParameter (lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, v)), juce::sendNotification); };
        }
        else
        {
            h->get = [&p] { return p.getCurrentNormalisedValue(); };
            h->set = [&p] (float v) { p.setNormalisedParameter (juce::jlimit (0.0f, 1.0f, v), juce::sendNotification); };
        }
        // AirWindows prints bare 0..1 numbers for most knobs: show those as percentages, keep real units as they are
        const bool unitRange = range.getStart() == 0.0f && range.getEnd() == 1.0f;   // only bare 0..1 knobs, never a 0..10 dial sitting at 0
        h->text = [&p, unitRange]
        {
            auto t = p.getCurrentValueAsString().trim();
            if (unitRange && t.containsOnly ("0123456789.") && std::abs (t.getFloatValue() - p.getCurrentNormalisedValue()) < 0.0015f)
                return juce::String (juce::roundToInt (t.getFloatValue() * 100.0f)) + " %";
            return t;
        };
        h->gestureBegin = [&p] { p.parameterChangeGestureBegin(); };
        h->gestureEnd = [&p] { p.parameterChangeGestureEnd(); };
        h->defaultNorm = h->get();
        const auto& nm = h->name;
        h->bipolar = nm.containsIgnoreCase ("pan") || nm.containsIgnoreCase ("shelf gain") || nm.containsIgnoreCase ("Mid gain") || nm.containsIgnoreCase ("tune")
                     || nm.containsIgnoreCase ("semitones") || nm.containsIgnoreCase ("spread") || nm.containsIgnoreCase ("Voicing") || nm.containsIgnoreCase ("filterAmount") || nm == "Point" || nm == "Center";
        return h;
    }

    inline std::unique_ptr<Handle> fromCached (const juce::String& name, juce::CachedValue<float>& cv, float lo, float hi, float skew, const juce::String& unit, int decimals = 1)
    {
        auto h = std::make_unique<Handle>();
        h->name = name; h->label = name;
        juce::NormalisableRange<float> range (lo, hi, 0.0f, skew);
        h->get = [&cv, range] { return range.convertTo0to1 (juce::jlimit (range.start, range.end, cv.get())); };
        h->set = [&cv, range] (float v) { cv = range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)); };
        h->text = [&cv, unit, decimals] { return SqDsp::numText (cv.get(), decimals) + unit; };
        h->defaultNorm = range.convertTo0to1 (juce::jlimit (lo, hi, cv.get()));
        return h;
    }

    inline std::unique_ptr<Handle> fromCachedInt (const juce::String& name, juce::CachedValue<int>& cv, int lo, int hi, float skew, const juce::String& unit)
    {
        auto h = std::make_unique<Handle>();
        h->name = name; h->label = name;
        juce::NormalisableRange<float> range ((float) lo, (float) hi, 1.0f, skew);
        h->get = [&cv, range] { return range.convertTo0to1 (juce::jlimit (range.start, range.end, (float) cv.get())); };
        h->set = [&cv, range] (float v) { cv = juce::roundToInt (range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); };
        h->text = [&cv, unit] { return juce::String (cv.get()) + unit; };
        h->defaultNorm = range.convertTo0to1 (juce::jlimit ((float) lo, (float) hi, (float) cv.get()));
        return h;
    }

    inline std::unique_ptr<Handle> choice (const juce::String& name, juce::StringArray choices, std::function<int()> get, std::function<void (int)> set)
    {
        auto h = std::make_unique<Handle>();
        h->name = name; h->label = name; h->choices = choices;
        auto n = juce::jmax (1, choices.size() - 1);
        h->get = [get, n] { return (float) get() / (float) n; };
        h->set = [set, n] (float v) { set (juce::roundToInt (v * (float) n)); };
        h->text = [get, choices] { return choices[juce::jlimit (0, choices.size() - 1, get())]; };
        return h;
    }

    inline std::unique_ptr<Handle> toggle (const juce::String& name, std::function<bool()> get, std::function<void (bool)> set)
    {
        auto h = std::make_unique<Handle>();
        h->name = name; h->label = name; h->toggle = true;
        h->get = [get] { return get() ? 1.0f : 0.0f; };
        h->set = [set] (float v) { set (v > 0.5f); };
        h->text = [get] { return juce::String (get() ? "on" : "off"); };
        return h;
    }

    //==============================================================================
    enum class KnobStyle { ring, vintage, flat, chicken, slim };

    /** The Squelchmoth knob: vertical drag, Cmd/Ctrl = fine, double-click = default, shows label + value. */
    class Knob : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Knob (Handle& h, juce::Colour accent, KnobStyle s, bool big = false)
            : handle (h), colour (accent.withSaturation (juce::jmin (1.0f, accent.getSaturation() + 0.2f)).withBrightness (juce::jmin (1.0f, accent.getBrightness() + 0.1f))), style (s), large (big)
        {
            setTooltip (h.label + ": drag up/down, Cmd-drag for fine control, double-click to reset");
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto labelArea = r.removeFromBottom (13.0f);
            auto valueArea = r.removeFromBottom (13.0f);
            float side = juce::jmin (r.getWidth(), r.getHeight());
            auto k = r.withSizeKeepingCentre (side, side).reduced (large ? 4.0f : 3.0f);
            auto c = k.getCentre();
            auto radius = k.getWidth() * 0.5f;
            const float a0 = juce::MathConstants<float>::pi * 1.2f, a1 = juce::MathConstants<float>::pi * 2.8f;
            auto v = juce::jlimit (0.0f, 1.0f, handle.get());
            auto angle = a0 + v * (a1 - a0);
            const bool bipolar = handle.bipolar;
            // vivid value glow shared by the arc styles
            auto glowArc = [&] (const juce::Path& p, float w)
            {
                g.setColour (colour.withAlpha (0.14f)); g.strokePath (p, juce::PathStrokeType (w * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (colour.withAlpha (0.28f)); g.strokePath (p, juce::PathStrokeType (w * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            };

            switch (style)
            {
                case KnobStyle::ring:
                {
                    g.setColour (juce::Colours::black.withAlpha (0.45f));
                    g.fillEllipse (k);
                    juce::Path track; track.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, a0, a1, true);
                    g.setColour (Theme::line().withAlpha (0.8f));
                    g.strokePath (track, juce::PathStrokeType (large ? 4.0f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    juce::Path fill;
                    auto from = bipolar ? (a0 + a1) * 0.5f : a0;
                    fill.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
                    glowArc (fill, large ? 4.0f : 3.0f);
                    g.setColour (colour);
                    g.strokePath (fill, juce::PathStrokeType (large ? 4.0f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    auto inner = k.reduced (radius * 0.3f);
                    g.setGradientFill (juce::ColourGradient (Theme::panel2().brighter (0.25f), inner.getX(), inner.getY(), Theme::panel2().darker (0.6f), inner.getRight(), inner.getBottom(), false));
                    g.fillEllipse (inner);
                    auto tip = c.getPointOnCircumference (radius * 0.62f, angle);
                    g.setColour (colour.brighter (0.4f));
                    g.drawLine (c.getPointOnCircumference (radius * 0.25f, angle).x, c.getPointOnCircumference (radius * 0.25f, angle).y, tip.x, tip.y, 2.5f);
                    break;
                }
                case KnobStyle::vintage:
                {
                    g.setColour (juce::Colours::black.withAlpha (0.5f));
                    g.fillEllipse (k.expanded (2.0f));
                    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3630), k.getX(), k.getY(), juce::Colour (0xff15130f), k.getRight(), k.getBottom(), false));
                    g.fillEllipse (k);
                    g.setColour (juce::Colour (0xff5a554c));
                    g.drawEllipse (k.reduced (1.0f), 1.5f);
                    for (int i = 0; i <= 10; ++i)
                    {
                        auto a = a0 + (a1 - a0) * (float) i / 10.0f;
                        auto p1 = c.getPointOnCircumference (radius + 1.0f, a), p2 = c.getPointOnCircumference (radius + 4.0f, a);
                        const bool lit = bipolar ? (a - (a0 + a1) * 0.5f) * (angle - (a0 + a1) * 0.5f) >= 0.0f && std::abs (a - (a0 + a1) * 0.5f) <= std::abs (angle - (a0 + a1) * 0.5f) + 0.01f
                                                 : a <= angle + 0.01f;
                        g.setColour (lit ? colour : Theme::dim());
                        g.drawLine (p1.x, p1.y, p2.x, p2.y, lit ? 1.6f : 1.0f);
                    }
                    auto tip = c.getPointOnCircumference (radius * 0.85f, angle);
                    g.setColour (juce::Colour (0xfff0e8d8));
                    g.drawLine (c.getPointOnCircumference (radius * 0.2f, angle).x, c.getPointOnCircumference (radius * 0.2f, angle).y, tip.x, tip.y, 3.0f);
                    g.setColour (colour.withAlpha (0.25f));
                    g.fillEllipse (tip.x - 6.0f, tip.y - 6.0f, 12.0f, 12.0f);
                    g.setColour (colour);
                    g.fillEllipse (tip.x - 2.5f, tip.y - 2.5f, 5.0f, 5.0f);
                    break;
                }
                case KnobStyle::flat:
                {
                    juce::Path track; track.addCentredArc (c.x, c.y, radius - 1.0f, radius - 1.0f, 0.0f, a0, a1, true);
                    g.setColour (Theme::line());
                    g.strokePath (track, juce::PathStrokeType (large ? 7.0f : 5.0f));
                    juce::Path fill;
                    auto from = bipolar ? (a0 + a1) * 0.5f : a0;
                    fill.addCentredArc (c.x, c.y, radius - 1.0f, radius - 1.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
                    glowArc (fill, large ? 5.0f : 3.5f);
                    g.setColour (colour);
                    g.strokePath (fill, juce::PathStrokeType (large ? 7.0f : 5.0f));
                    g.setColour (Theme::text());
                    auto tip = c.getPointOnCircumference (radius - 1.0f, angle);
                    g.fillEllipse (tip.x - 3.0f, tip.y - 3.0f, 6.0f, 6.0f);
                    break;
                }
                case KnobStyle::chicken:
                {
                    g.setColour (juce::Colours::black.withAlpha (0.4f));
                    g.fillEllipse (k);
                    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b33), k.getX(), k.getY(), juce::Colour (0xff0e0e12), k.getRight(), k.getBottom(), false));
                    g.fillEllipse (k.reduced (3.0f));
                    juce::Path pointer;
                    pointer.addTriangle (c.getPointOnCircumference (radius * 0.9f, angle), c.getPointOnCircumference (radius * 0.35f, angle + 0.55f), c.getPointOnCircumference (radius * 0.35f, angle - 0.55f));
                    {
                        juce::Path arc; arc.addCentredArc (c.x, c.y, radius - 1.0f, radius - 1.0f, 0.0f, bipolar ? juce::jmin ((a0 + a1) * 0.5f, angle) : a0, bipolar ? juce::jmax ((a0 + a1) * 0.5f, angle) : angle, true);
                        glowArc (arc, 1.6f);
                        g.setColour (colour.withAlpha (0.85f)); g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    }
                    g.setColour (colour.withAlpha (0.3f));
                    g.strokePath (pointer, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (colour);
                    g.fillPath (pointer);
                    g.setColour (juce::Colours::white.withAlpha (0.25f));
                    g.drawEllipse (k.reduced (3.0f), 1.0f);
                    break;
                }
                case KnobStyle::slim:
                {
                    juce::Path track; track.addCentredArc (c.x, c.y, radius - 1.0f, radius - 1.0f, 0.0f, a0, a1, true);
                    g.setColour (Theme::line());
                    g.strokePath (track, juce::PathStrokeType (1.5f));
                    juce::Path fill; fill.addCentredArc (c.x, c.y, radius - 1.0f, radius - 1.0f, 0.0f, a0, angle, true);
                    glowArc (fill, 2.0f);
                    g.setColour (colour);
                    g.strokePath (fill, juce::PathStrokeType (2.5f));
                    g.setColour (Theme::text());
                    auto tip = c.getPointOnCircumference (radius - 6.0f, angle);
                    g.drawLine (c.x, c.y, tip.x, tip.y, 1.5f);
                    break;
                }
            }

            g.setFont (Theme::font (large ? 11.0f : 9.5f, true));
            g.setColour (Theme::text());
            g.drawText (handle.text(), valueArea, juce::Justification::centred);
            g.setFont (Theme::font (large ? 10.0f : 9.0f));
            g.setColour (Theme::dim());
            g.drawText (handle.label.toUpperCase(), labelArea, juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { showMenu(); return; }
            dragStart = handle.get();
            if (handle.gestureBegin) handle.gestureBegin();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            auto sens = (e.mods.isCommandDown() || e.mods.isCtrlDown()) ? 1200.0f : 200.0f;
            handle.set (dragStart - (float) e.getDistanceFromDragStartY() / sens);
            repaint();
            if (onChange) onChange();
        }
        void mouseUp (const juce::MouseEvent&) override            { if (handle.gestureEnd) handle.gestureEnd(); }
        void mouseDoubleClick (const juce::MouseEvent&) override   { handle.set (handle.defaultNorm); repaint(); if (onChange) onChange(); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { handle.set (handle.get() + w.deltaY * 0.08f); repaint(); if (onChange) onChange(); }

        void showMenu()
        {
            juce::PopupMenu m;
            m.addItem (1, "Reset to default");
            m.addItem (2, "Type a value...");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
            {
                if (r == 1) { handle.set (handle.defaultNorm); repaint(); if (onChange) onChange(); }
                else if (r == 2 && handle.param != nullptr)
                {
                    auto w = std::make_shared<juce::AlertWindow> (handle.label, "Enter a value (" + handle.text() + ")", juce::MessageBoxIconType::NoIcon);
                    w->addTextEditor ("v", handle.text());
                    w->addButton ("OK", 1); w->addButton ("Cancel", 0);
                    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int res)
                    {
                        if (res == 1 && handle.param != nullptr) { handle.param->setParameter (handle.param->getValueRange().clipValue (handle.param->stringToValue (w->getTextEditorContents ("v"))), juce::sendNotification); repaint(); if (onChange) onChange(); }
                    }), false);
                }
            });
        }

        std::function<void()> onChange;
        Handle& handle;
        juce::Colour colour;
        KnobStyle style;
        bool large;
    private:
        float dragStart = 0.0f;
    };

    /** Choice control: a row of segments (filter type, wave shape, mode...). */
    class ChoiceBar : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ChoiceBar (Handle& h, juce::Colour accent) : handle (h), colour (accent) { setTooltip (h.label); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto labelArea = r.removeFromBottom (13.0f);
            g.setFont (Theme::font (9.0f));
            g.setColour (Theme::dim());
            g.drawText (handle.label.toUpperCase(), labelArea, juce::Justification::centred);
            auto n = handle.choices.size();
            auto cur = juce::roundToInt (handle.get() * (float) juce::jmax (1, n - 1));
            if (usePopup())
            {
                auto cell = r.reduced (1.0f, 2.0f);
                g.setColour (colour.withAlpha (0.85f)); g.fillRoundedRectangle (cell, 3.0f);
                g.setColour (juce::Colours::black); g.setFont (Theme::font (9.5f, true));
                g.drawFittedText (handle.choices[juce::jlimit (0, n - 1, cur)] + "  v", cell.toNearestInt().reduced (4, 0), juce::Justification::centred, 1);
                return;
            }
            auto w = r.getWidth() / (float) juce::jmax (1, n);
            for (int i = 0; i < n; ++i)
            {
                auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (1.0f, 2.0f);
                g.setColour (i == cur ? colour.withAlpha (0.85f) : juce::Colours::black.withAlpha (0.4f));
                g.fillRoundedRectangle (cell, 3.0f);
                g.setColour (i == cur ? juce::Colours::black : Theme::text());
                g.setFont (Theme::font (9.5f, i == cur));
                g.drawFittedText (handle.choices[i], cell.toNearestInt(), juce::Justification::centred, 1);
            }
        }
        bool usePopup() const   { int total = 0; for (auto& c : handle.choices) total += c.length(); return total > 36 || handle.choices.size() > 7 || (getWidth() > 0 && getWidth() < total * 6 + handle.choices.size() * 6); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            auto n = handle.choices.size();
            if (usePopup())
            {
                juce::PopupMenu m;
                auto cur = juce::roundToInt (handle.get() * (float) juce::jmax (1, n - 1));
                for (int i = 0; i < n; ++i) m.addItem (i + 1, handle.choices[i], true, i == cur);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, n] (int r) { if (r > 0) { handle.set ((float) (r - 1) / (float) juce::jmax (1, n - 1)); repaint(); if (onChange) onChange(); } });
                return;
            }
            auto w = (float) getWidth() / (float) juce::jmax (1, n);
            auto idx = juce::jlimit (0, n - 1, (int) ((float) e.x / w));
            handle.set ((float) idx / (float) juce::jmax (1, n - 1));
            repaint();
            if (onChange) onChange();
        }
        std::function<void()> onChange;
        Handle& handle;
        juce::Colour colour;
    };

    /** Toggle: an illuminated button. */
    class ToggleLamp : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ToggleLamp (Handle& h, juce::Colour accent) : handle (h), colour (accent) { setTooltip (h.label); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto labelArea = r.removeFromBottom (13.0f);
            bool on = handle.get() > 0.5f;
            auto btn = r.reduced (4.0f, 6.0f);
            g.setColour (on ? colour.withAlpha (0.9f) : juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (btn, 4.0f);
            g.setColour (on ? colour.brighter (0.5f) : Theme::line());
            g.drawRoundedRectangle (btn, 4.0f, 1.0f);
            g.setColour (on ? juce::Colours::black : Theme::dim());
            g.setFont (Theme::font (10.0f, true));
            g.drawText (on ? "ON" : "OFF", btn, juce::Justification::centred);
            g.setFont (Theme::font (9.0f));
            g.setColour (Theme::dim());
            g.drawText (handle.label.toUpperCase(), labelArea, juce::Justification::centred);
        }
        void mouseDown (const juce::MouseEvent&) override   { handle.set (handle.get() > 0.5f ? 0.0f : 1.0f); repaint(); if (onChange) onChange(); }
        std::function<void()> onChange;
        Handle& handle;
        juce::Colour colour;
    };

    //==============================================================================
    // Visualisers. Each one reads the handles by name so it works on any plugin of its family.
    using HandleMap = std::map<juce::String, Handle*>;

    struct Viz : public juce::Component
    {
        Viz (HandleMap& h, te::Plugin& p, juce::Colour c) : handles (h), plugin (p), accent (c)
        {
            setInterceptsMouseClicks (false, false);
            if (auto t = plugin.getOwnerTrack())
                if (auto at = dynamic_cast<te::AudioTrack*> (t))
                    if (auto lm = at->getLevelMeterPlugin())
                    {
                        measurer = &lm->measurer;
                        measurer->addClient (client);
                    }
        }
        ~Viz() override   { if (measurer != nullptr) measurer->removeClient (client); }

        float v (const juce::String& name, float fallback = 0.5f) const   { auto it = handles.find (name); return it != handles.end() ? it->second->get() : fallback; }
        Handle* h (const juce::String& name) const                          { auto it = handles.find (name); return it != handles.end() ? it->second : nullptr; }

        /** Live level (dB) from the owning track's meter: lets visualisers breathe with the music. */
        float activityDb()
        {
            if (measurer == nullptr) return -100.0f;
            auto l = juce::jmax (client.getAndClearAudioLevel (0).dB, client.getAndClearAudioLevel (1).dB);
            lastLevel = l > -99.0f ? l : juce::jmax (-100.0f, lastLevel - 2.0f);
            return lastLevel;
        }
        te::LevelMeasurer* measurer = nullptr;
        te::LevelMeasurer::Client client;
        float lastLevel = -100.0f;
        static float xForHz (float hz, juce::Rectangle<float> r)   { return r.getX() + r.getWidth() * (std::log (hz / 20.0f) / std::log (1000.0f)); }
        static float hzForX (float x, juce::Rectangle<float> r)    { return 20.0f * std::pow (1000.0f, (x - r.getX()) / r.getWidth()); }

        void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> r, float dbRange = 18.0f)
        {
            g.setColour (Theme::text().withAlpha (0.08f));
            for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            {
                auto x = xForHz (hz, r);
                g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            }
            for (float db = -dbRange; db <= dbRange; db += dbRange / 3.0f)
                g.drawHorizontalLine ((int) (r.getCentreY() - db / dbRange * r.getHeight() * 0.5f), r.getX(), r.getRight());
            g.setFont (Theme::font (8.5f));
            g.setColour (Theme::dim().withAlpha (0.8f));
            for (float hz : { 100.0f, 1000.0f, 10000.0f })
                g.drawText (hz >= 1000 ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String ((int) hz), (int) xForHz (hz, r) + 2, (int) r.getBottom() - 12, 30, 11, juce::Justification::left);
        }

        void drawCurve (juce::Graphics& g, juce::Rectangle<float> r, std::function<float (float hz)> dbAt, float dbRange, juce::Colour c, bool fill = true)
        {
            juce::Path p;
            for (int i = 0; i <= (int) r.getWidth(); i += 2)
            {
                auto x = r.getX() + (float) i;
                auto db = juce::jlimit (-dbRange, dbRange, dbAt (hzForX (x, r)));
                auto y = r.getCentreY() - db / dbRange * r.getHeight() * 0.5f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            if (fill)
            {
                juce::Path f (p);
                f.lineTo (r.getRight(), r.getCentreY()); f.lineTo (r.getX(), r.getCentreY()); f.closeSubPath();
                g.setColour (c.withAlpha (0.18f));
                g.fillPath (f);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (2.0f));
        }

        void drawWave (juce::Graphics& g, juce::Rectangle<float> r, std::function<float (float t)> yAt, juce::Colour c, float thickness = 2.0f)
        {
            juce::Path p;
            for (int i = 0; i <= (int) r.getWidth(); ++i)
            {
                auto t = (float) i / r.getWidth();
                auto y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, yAt (t)) * r.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + (float) i, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved));
        }

        HandleMap& handles;
        te::Plugin& plugin;
        juce::Colour accent;
        float phase = 0.0f;     // animation phase advanced by the editor timer
    };

    // --- EQ family ------------------------------------------------------------
    struct EQViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            drawFreqGrid (g, r);
            if (auto eq = dynamic_cast<te::EqualiserPlugin*> (&plugin))
            {
                drawCurve (g, r, [eq] (float hz) { return eq->getDBGainAtFrequency (hz); }, 18.0f, accent);
                // band handles
                struct Band { const char* f; const char* gn; juce::Colour c; };
                const Band bands[] = { { "Low-shelf freq", "Low-shelf gain", Theme::amber() }, { "Mid freq 1", "Mid gain 1", Theme::acid() }, { "Mid freq 2", "Mid gain 2", Theme::magenta() }, { "High-shelf freq", "High-shelf gain", Theme::mint() } };
                int i = 1;
                for (auto& b : bands)
                {
                    auto fh = h (b.f); auto gh = h (b.gn);
                    if (! fh || ! gh) continue;
                    auto hz = fh->param ? fh->param->getCurrentValue() : 1000.0f;
                    auto db = gh->param ? gh->param->getCurrentValue() : 0.0f;
                    auto x = xForHz (juce::jlimit (20.0f, 20000.0f, hz), r), y = r.getCentreY() - juce::jlimit (-18.0f, 18.0f, db) / 18.0f * r.getHeight() * 0.5f;
                    g.setColour (b.c);
                    g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
                    g.setColour (juce::Colours::black);
                    g.setFont (Theme::font (8.0f, true));
                    g.drawText (juce::String (i++), (int) x - 6, (int) y - 6, 12, 12, juce::Justification::centred);
                }
            }
        }
    };

    /** Draggable band nodes for Prism EQ (drag = freq/gain, wheel = Q). */
    struct EQNodes : public juce::Component
    {
        EQNodes (HandleMap& hm, te::Plugin& p) : handles (hm), plugin (p) {}
        struct Band { const char* f; const char* gn; const char* q; };
        const Band bands[4] = { { "Low-shelf freq", "Low-shelf gain", "Low-shelf Q" }, { "Mid freq 1", "Mid gain 1", "Mid Q 1" }, { "Mid freq 2", "Mid gain 2", "Mid Q 2" }, { "High-shelf freq", "High-shelf gain", "High-shelf Q" } };
        juce::Rectangle<float> area() const   { return getLocalBounds().toFloat().reduced (6.0f); }
        int hit (juce::Point<float> p)
        {
            auto r = area();
            for (int i = 0; i < 4; ++i)
            {
                auto fh = handles.find (bands[i].f), gh = handles.find (bands[i].gn);
                if (fh == handles.end() || gh == handles.end() || ! fh->second->param || ! gh->second->param) continue;
                auto x = Viz::xForHz (juce::jlimit (20.0f, 20000.0f, fh->second->param->getCurrentValue()), r);
                auto y = r.getCentreY() - juce::jlimit (-18.0f, 18.0f, gh->second->param->getCurrentValue()) / 18.0f * r.getHeight() * 0.5f;
                if (p.getDistanceFrom ({ x, y }) < 10.0f) return i;
            }
            return -1;
        }
        void mouseDown (const juce::MouseEvent& e) override   { dragging = hit (e.position); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging < 0) return;
            auto r = area();
            auto fh = handles.find (bands[dragging].f), gh = handles.find (bands[dragging].gn);
            if (fh != handles.end() && fh->second->param)
                fh->second->param->setParameter (fh->second->param->getValueRange().clipValue (Viz::hzForX (juce::jlimit (r.getX(), r.getRight(), e.position.x), r)), juce::sendNotification);
            if (gh != handles.end() && gh->second->param)
                gh->second->param->setParameter (gh->second->param->getValueRange().clipValue ((r.getCentreY() - e.position.y) / (r.getHeight() * 0.5f) * 18.0f), juce::sendNotification);
            if (onChange) onChange();
        }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            auto i = hit (e.position);
            if (i < 0) return;
            auto qh = handles.find (bands[i].q);
            if (qh != handles.end()) { qh->second->set (qh->second->get() + w.deltaY * 0.1f); if (onChange) onChange(); }
        }
        std::function<void()> onChange;
        HandleMap& handles;
        te::Plugin& plugin;
        int dragging = -1;
    };

    struct FilterViz : public Viz   // Veil Filter / Capacitor / Scalpel: stylised response
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            drawFreqGrid (g, r, 24.0f);
            float lp = 20000.0f, hp = 20.0f, q = 0.7f; bool notch = false, band = false;
            if (auto lpf = dynamic_cast<te::LowPassPlugin*> (&plugin))
            {
                auto f = lpf->frequency->getCurrentValue();
                if (lpf->isLowPass()) lp = f; else hp = f;
            }
            else if (h ("Lowpass") != nullptr)  { lp = 20.0f * std::pow (1000.0f, v ("Lowpass", 1.0f)); hp = 20.0f * std::pow (1000.0f, v ("Highpass", 0.0f)); }
            else if (h ("Type") != nullptr)
            {
                auto t = v ("Type"); auto f = 20.0f * std::pow (1000.0f, v ("Freq")); q = 0.3f + v ("Q") * 6.0f;
                if (t < 0.17f) lp = f; else if (t < 0.5f) hp = f; else if (t < 0.84f) { band = true; lp = f * 1.6f; hp = f / 1.6f; } else { notch = true; lp = f; }
            }
            auto dbAt = [=] (float hz)
            {
                float db = 0.0f;
                if (notch) { auto d = std::abs (std::log2 (hz / lp)); db = -24.0f * std::exp (-d * d * (q * 3.0f)); return db; }
                if (hz > lp) db -= 12.0f * std::log2 (hz / lp) * (band ? 1.0f : 2.0f);
                if (hz < hp) db -= 12.0f * std::log2 (hp / hz) * (band ? 1.0f : 2.0f);
                if (! band && q > 1.0f) { auto d = std::abs (std::log2 (hz / (lp < 20000.0f ? lp : hp))); db += (q - 1.0f) * 2.5f * std::exp (-d * d * 12.0f); }
                return db;
            };
            drawCurve (g, r, dbAt, 24.0f, accent);
        }
    };

    struct TiltViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            drawFreqGrid (g, r, 12.0f);
            auto tilt = (v ("Voicing") - 0.5f) * 12.0f, highs = (v ("Highs") - 0.5f) * 8.0f;
            drawCurve (g, r, [=] (float hz) { return tilt * (std::log2 (hz / 630.0f) / 5.0f) + highs * juce::jlimit (0.0f, 1.0f, std::log2 (hz / 4000.0f) / 2.3f); }, 12.0f, accent);
        }
    };

    struct AirViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            drawFreqGrid (g, r, 12.0f);
            auto t22 = (v ("22K tap") - 0.5f) * 12.0f, t15 = (v ("15K tap") - 0.5f) * 12.0f, t11 = (v ("11K tap") - 0.5f) * 12.0f;
            drawCurve (g, r, [=] (float hz) { auto bump = [hz] (float f, float g2) { auto d = std::log2 (hz / f); return g2 * std::exp (-d * d * 2.5f); }; return bump (11000.0f, t11) + bump (15000.0f, t15) + bump (20000.0f, t22); }, 12.0f, accent);
            for (float f : { 11000.0f, 15000.0f, 20000.0f }) { g.setColour (accent.withAlpha (0.35f)); g.drawVerticalLine ((int) xForHz (f, r), r.getY(), r.getBottom()); }
        }
    };

    struct SubViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            drawFreqGrid (g, r, 18.0f);
            auto xo = 20.0f * std::pow (40.0f, v ("XOver", 0.74f));
            auto bass = (v ("BassOut") - 0.5f) * 18.0f + v ("BsDrive") * 4.0f, sub = v ("SubOut") * 14.0f, grind = v ("TGrind") * 5.0f;
            drawCurve (g, r, [=] (float hz) { float db = 0.0f; if (hz < xo) db += bass * (1.0f - hz / xo); if (hz < xo * 0.5f) db += sub * (1.0f - hz / (xo * 0.5f)); db += grind * juce::jlimit (0.0f, 1.0f, std::log2 (hz / 300.0f) / 5.0f); return db; }, 18.0f, accent);
            g.setColour (Theme::amber().withAlpha (0.6f)); g.drawVerticalLine ((int) xForHz (xo, r), r.getY(), r.getBottom());
        }
    };

    // --- Dynamics ---------------------------------------------------------------
    struct DynViz : public Viz
    {
        using Viz::Viz;
        float grSmooth = 0.0f, inSmooth = -100.0f;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto meter = r.removeFromRight (54.0f);
            r.removeFromRight (8.0f);
            // transfer curve
            g.setColour (Theme::text().withAlpha (0.08f));
            for (int i = 1; i < 6; ++i) { g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 6.0f), r.getY(), r.getBottom()); g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 6.0f), r.getX(), r.getRight()); }
            float threshDb = -20.0f, ratio = 3.0f, knee = 6.0f;
            if (auto c = dynamic_cast<te::CompressorPlugin*> (&plugin))
            {
                threshDb = juce::Decibels::gainToDecibels (c->getThreshold());
                ratio = c->getRatio() > 0.001f ? 1.0f / c->getRatio() : 100.0f;
            }
            else
            {
                auto main = v ("Pressure", v ("Compress", v ("Intensty", v ("Intenst", v ("Boost", 0.3f)))));
                threshDb = -6.0f - main * 34.0f; ratio = 1.5f + main * 8.0f; knee = 12.0f;
                if (h ("Thresh")) { threshDb = -60.0f + v ("Thresh") * 50.0f; ratio = 0.2f; }   // gate: expansion below
            }
            juce::Path p;
            for (int i = 0; i <= (int) r.getWidth(); i += 2)
            {
                auto in = -60.0f + 60.0f * (float) i / r.getWidth();
                float out;
                if (ratio < 1.0f) out = in < threshDb ? threshDb - (threshDb - in) / ratio : in;   // expander/gate
                else
                {
                    auto over = in - threshDb;
                    if (over <= -knee * 0.5f) out = in;
                    else if (over >= knee * 0.5f) out = threshDb + over / ratio;
                    else { auto t = (over + knee * 0.5f) / knee; out = in - (1.0f - 1.0f / ratio) * (t * t) * knee * 0.5f; }
                }
                out = juce::jlimit (-60.0f, 0.0f, out);
                auto x = r.getX() + (float) i, y = r.getBottom() - (out + 60.0f) / 60.0f * r.getHeight();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (Theme::dim().withAlpha (0.35f));
            g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.0f);
            g.setColour (accent);
            g.strokePath (p, juce::PathStrokeType (2.2f));
            // threshold marker
            auto tx = r.getX() + (threshDb + 60.0f) / 60.0f * r.getWidth();
            g.setColour (Theme::amber().withAlpha (0.7f));
            g.drawVerticalLine ((int) tx, r.getY(), r.getBottom());
            g.setFont (Theme::font (8.5f));
            g.setColour (Theme::dim());
            g.drawText ("IN", r.withTrimmedTop (r.getHeight() - 12.0f).toNearestInt(), juce::Justification::centredRight);
            g.drawText ("OUT", r.withWidth (30.0f).toNearestInt(), juce::Justification::topLeft);
            // activity + estimated gain reduction meter
            auto in = activityDb();
            inSmooth = in > inSmooth ? in : inSmooth - 1.5f;
            float gr = 0.0f;
            if (inSmooth > threshDb && ratio >= 1.0f) gr = (inSmooth - threshDb) * (1.0f - 1.0f / ratio);
            grSmooth = gr > grSmooth ? grSmooth + (gr - grSmooth) * 0.5f : grSmooth - 0.6f;
            grSmooth = juce::jmax (0.0f, grSmooth);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (meter, 3.0f);
            auto half = meter.getWidth() / 2.0f;
            auto inBar = meter.withWidth (half).reduced (3.0f, 3.0f), grBar = meter.withTrimmedLeft (half).reduced (3.0f, 3.0f);
            auto inProp = juce::jlimit (0.0f, 1.0f, (inSmooth + 60.0f) / 60.0f);
            g.setColour (Theme::mint().withAlpha (0.8f));
            g.fillRect (inBar.withTop (inBar.getBottom() - inBar.getHeight() * inProp));
            auto grProp = juce::jlimit (0.0f, 1.0f, grSmooth / 24.0f);
            g.setColour (Theme::amber());
            g.fillRect (grBar.withHeight (grBar.getHeight() * grProp));
            g.setColour (Theme::dim());
            g.setFont (Theme::font (8.0f));
            g.drawText ("IN", inBar.toNearestInt().withHeight (10).translated (0, (int) inBar.getHeight() - 10), juce::Justification::centred);
            g.drawText ("GR", grBar.toNearestInt().withHeight (10).translated (0, (int) grBar.getHeight() - 10), juce::Justification::centred);
            g.setColour (Theme::text());
            g.setFont (Theme::font (9.0f, true));
            g.drawText ("-" + juce::String (grSmooth, 1), grBar.toNearestInt().withHeight (12), juce::Justification::centred);
        }
    };

    struct GateViz : public Viz
    {
        using Viz::Viz;
        float env = -100.0f;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto thresh = -60.0f + v ("Thresh") * 50.0f;
            auto in = activityDb(); env = in > env ? in : env - 1.0f;
            bool open = env > thresh;
            auto lamp = r.removeFromRight (70.0f).reduced (10.0f);
            g.setColour (open ? Theme::acid() : juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (lamp.withSizeKeepingCentre (40.0f, 40.0f));
            g.setColour (Theme::dim()); g.setFont (Theme::font (9.0f, true));
            g.drawText (open ? "OPEN" : "CLOSED", lamp.withTrimmedTop (lamp.getHeight() - 14.0f).toNearestInt(), juce::Justification::centred);
            // level history bar
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r, 3.0f);
            auto prop = juce::jlimit (0.0f, 1.0f, (env + 60.0f) / 60.0f);
            g.setColour (open ? Theme::mint() : Theme::dim());
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * prop).reduced (3.0f), 2.0f);
            auto tx = r.getX() + (thresh + 60.0f) / 60.0f * r.getWidth();
            g.setColour (Theme::amber()); g.drawVerticalLine ((int) tx, r.getY(), r.getBottom());
            g.setFont (Theme::font (8.5f)); g.setColour (Theme::dim());
            g.drawText ("threshold " + juce::String (juce::roundToInt (thresh)) + " dB   release tone: treble " + juce::String ((int) (v ("TrebSus") * 100)) + "%  bass " + juce::String ((int) (v ("BassSus") * 100)) + "%", r.toNearestInt().reduced (6, 0), juce::Justification::centredLeft);
        }
    };

    struct DeEssViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            drawFreqGrid (g, r, 18.0f);
            auto f = 2000.0f * std::pow (6.0f, v ("Freq"));
            auto depth = v ("Intense") * 18.0f;
            drawCurve (g, r, [=] (float hz) { return hz > f ? -depth * juce::jlimit (0.0f, 1.0f, std::log2 (hz / f)) : 0.0f; }, 18.0f, accent);
            g.setColour (Theme::amber().withAlpha (0.8f)); g.drawVerticalLine ((int) xForHz (f, r), r.getY(), r.getBottom());
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::text());
            g.drawText ("sibilance above " + juce::String ((int) f) + " Hz", r.toNearestInt().reduced (6, 4), juce::Justification::topLeft);
        }
    };

    struct TransientViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto point = (v ("Point", v ("Intenst", 0.5f)) - 0.5f) * 2.0f;   // -1 soften .. +1 sharpen
            if (h ("Intenst")) point = v ("Intenst");
            auto speed = v ("Reaction Speed", 0.5f);
            drawWave (g, r, [=] (float t)
            {
                auto base = t < 0.05f ? t / 0.05f : std::exp (-(t - 0.05f) * (4.0f + speed * 6.0f));
                auto attack = t < 0.05f ? (t / 0.05f) : std::exp (-(t - 0.05f) * 40.0f);
                return juce::jlimit (-1.0f, 1.0f, base * 0.7f + point * attack * 0.6f) * std::sin (t * 90.0f);
            }, accent, 1.5f);
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::dim());
            g.drawText (point > 0.05f ? "attack sharpened" : point < -0.05f ? "attack softened" : "neutral", r.toNearestInt().reduced (6, 4), juce::Justification::topLeft);
        }
    };

    // --- Saturation ---------------------------------------------------------------
    struct SatViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto curve = r.removeFromLeft (r.getHeight()).reduced (4.0f);
            r.removeFromLeft (10.0f);
            auto drive = v ("Drive", v ("Density", v ("Triode", v ("Overdrive", v ("Louder", v ("Input", 0.5f)))))) ;
            if (h ("Density")) drive = juce::jmax (0.0f, (v ("Density") - 0.2f) / 0.8f);
            if (h ("Input")) drive = juce::jlimit (0.0f, 1.0f, (v ("Input") - 0.5f) * 2.0f);
            auto k = 1.0f + drive * 12.0f;
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (curve, 4.0f);
            g.setColour (Theme::text().withAlpha (0.1f));
            g.drawLine (curve.getX(), curve.getBottom(), curve.getRight(), curve.getY());
            juce::Path p;
            for (int i = 0; i <= (int) curve.getWidth(); ++i)
            {
                auto x = -1.0f + 2.0f * (float) i / curve.getWidth();
                auto y = std::tanh (x * k) / std::tanh (k);
                auto px = curve.getX() + (float) i, py = curve.getCentreY() - y * curve.getHeight() * 0.48f;
                if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            g.setColour (accent); g.strokePath (p, juce::PathStrokeType (2.2f));
            // the wave before/after
            drawWave (g, r, [] (float t) { return std::sin (t * 12.56f) * 0.9f; }, Theme::dim().withAlpha (0.5f), 1.2f);
            drawWave (g, r, [=] (float t) { return std::tanh (std::sin (t * 12.56f) * 0.9f * k) / std::tanh (k); }, accent, 2.0f);
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::dim());
            g.drawText ("harmonic drive " + juce::String ((int) (drive * 100)) + "%", r.toNearestInt().reduced (6, 4), juce::Justification::topLeft);
        }
    };

    struct TapeViz : public Viz   // spinning reels; speed follows activity, wobble follows Flutter
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto reelR = juce::jmin (r.getHeight() * 0.42f, 42.0f);
            auto c1 = juce::Point<float> (r.getX() + r.getWidth() * 0.28f, r.getCentreY()), c2 = juce::Point<float> (r.getX() + r.getWidth() * 0.72f, r.getCentreY());
            auto flutter = v ("Flutter", v ("Depth", 0.1f));
            auto wobble = std::sin (phase * 7.0f) * flutter * 0.15f;
            for (auto c : { c1, c2 })
            {
                g.setColour (juce::Colour (0xff2a1f14));
                g.fillEllipse (c.x - reelR, c.y - reelR, reelR * 2, reelR * 2);
                g.setColour (accent.withAlpha (0.9f));
                g.drawEllipse (c.x - reelR, c.y - reelR, reelR * 2, reelR * 2, 2.0f);
                for (int i = 0; i < 3; ++i)
                {
                    auto a = phase * 2.0f + wobble + (float) i * 2.094f;
                    auto p1 = c.getPointOnCircumference (reelR * 0.25f, a), p2 = c.getPointOnCircumference (reelR * 0.85f, a);
                    g.setColour (Theme::text().withAlpha (0.8f));
                    g.drawLine (p1.x, p1.y, p2.x, p2.y, 3.0f);
                }
                g.setColour (Theme::panel2()); g.fillEllipse (c.x - reelR * 0.2f, c.y - reelR * 0.2f, reelR * 0.4f, reelR * 0.4f);
            }
            g.setColour (accent.withAlpha (0.7f));
            g.drawLine (c1.x, c1.y - reelR, c2.x, c2.y - reelR, 1.5f);
            g.drawLine (c1.x, c1.y + reelR, c1.x + (c2.x - c1.x) * 0.5f, r.getBottom() - 4.0f, 1.5f);
            g.drawLine (c1.x + (c2.x - c1.x) * 0.5f, r.getBottom() - 4.0f, c2.x, c2.y + reelR, 1.5f);
            // VU
            auto vu = juce::Rectangle<float> (r.getRight() - 26.0f, r.getY() + 4.0f, 20.0f, r.getHeight() - 8.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (vu, 3.0f);
            auto lvl = juce::jlimit (0.0f, 1.0f, (activityDb() + 60.0f) / 60.0f);
            g.setColour (Theme::amber()); g.fillRect (vu.reduced (3.0f).withTop (vu.getBottom() - 3.0f - (vu.getHeight() - 6.0f) * lvl));
        }
    };

    struct ConsoleViz : public Viz   // row of lamps that light with level
    {
        using Viz::Viz;
        float env = 0.0f;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto lvl = juce::jlimit (0.0f, 1.0f, (activityDb() + 54.0f) / 54.0f) * (0.5f + v ("Input", 1.0f) * 0.5f);
            env = lvl > env ? lvl : env - 0.02f;
            const int n = 16;
            auto w = r.getWidth() / (float) n;
            for (int i = 0; i < n; ++i)
            {
                auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (3.0f, 10.0f);
                bool on = (float) i / (float) n < env;
                auto c = i >= 13 ? Theme::red() : i >= 10 ? Theme::amber() : accent;
                g.setColour (on ? c : juce::Colours::black.withAlpha (0.5f));
                g.fillRoundedRectangle (cell, 3.0f);
                g.setColour (on ? c.brighter (0.4f) : c.withAlpha (0.35f));
                g.drawRoundedRectangle (cell, 3.0f, 1.0f);
                if (on) { g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (cell.expanded (3.0f), 5.0f); }
            }
        }
    };

    struct VinylViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto rad = juce::jmin (r.getHeight() * 0.46f, 60.0f);
            auto c = juce::Point<float> (r.getX() + rad + 10.0f, r.getCentreY());
            g.setColour (juce::Colour (0xff0a0a0a)); g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
            for (float rr = rad * 0.35f; rr < rad; rr += 4.0f) { g.setColour (juce::Colours::white.withAlpha (0.05f + 0.04f * std::sin (rr + phase * 3.0f))); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.0f); }
            g.setColour (accent); g.fillEllipse (c.x - rad * 0.33f, c.y - rad * 0.33f, rad * 0.66f, rad * 0.66f);
            auto a = phase * 1.5f;
            g.setColour (Theme::panel()); g.drawLine (c.x, c.y, c.getPointOnCircumference (rad * 0.3f, a).x, c.getPointOnCircumference (rad * 0.3f, a).y, 2.0f);
            auto wear = v ("Gv Wear", 0.06f);
            auto curve = r.withTrimmedLeft (rad * 2.0f + 24.0f);
            drawFreqGrid (g, curve, 18.0f);
            auto midHp = 20.0f * std::pow (20.0f, v ("Mid HiP", 0.2f)), sideHp = 20.0f * std::pow (20.0f, v ("SideHiP", 0.34f)), hLimit = 20000.0f / std::pow (8.0f, v ("H Limit", 0.32f));
            drawCurve (g, curve, [=] (float hz) { float db = 0; if (hz < midHp) db -= 12.0f * std::log2 (midHp / hz); if (hz > hLimit) db -= 10.0f * std::log2 (hz / hLimit); db -= wear * 4.0f * juce::jlimit (0.0f, 1.0f, std::log2 (hz / 2000.0f) / 3.3f); return db; }, 18.0f, accent);
            g.setColour (Theme::magenta().withAlpha (0.6f)); g.drawVerticalLine ((int) xForHz (sideHp, curve), curve.getY(), curve.getBottom());
        }
    };

    struct BitsViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto rate = v ("Rate", 1.0f), rez = v ("Rez", 1.0f);
            int steps = 2 + (int) (rate * rate * 60.0f);
            int levels = 2 + (int) (rez * rez * 60.0f);
            drawWave (g, r, [] (float t) { return std::sin (t * 12.56f) * 0.9f; }, Theme::dim().withAlpha (0.5f), 1.0f);
            juce::Path p;
            for (int i = 0; i < steps; ++i)
            {
                auto t0 = (float) i / (float) steps, t1 = (float) (i + 1) / (float) steps;
                auto y = std::round (std::sin (t0 * 12.56f) * 0.9f * (float) levels) / (float) levels;
                auto py = r.getCentreY() - y * r.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (r.getX() + t0 * r.getWidth(), py); else p.lineTo (r.getX() + t0 * r.getWidth(), py);
                p.lineTo (r.getX() + t1 * r.getWidth(), py);
            }
            g.setColour (accent); g.strokePath (p, juce::PathStrokeType (2.0f));
        }
    };

    // --- Space --------------------------------------------------------------------
    struct ReverbViz : public Viz   // decay envelope + drifting particles
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto size = v ("Room Size", v ("Size", v ("Sustain", 0.5f))), damp = v ("Damping", 1.0f - v ("Grain", 0.5f));
            auto decay = 0.3f + size * 4.0f;
            juce::Random rnd (42);
            for (int i = 0; i < 60; ++i)
            {
                auto t = std::fmod (rnd.nextFloat() + phase * 0.03f * (0.5f + rnd.nextFloat()), 1.0f);
                auto amp = std::exp (-t * 3.0f / decay) * (1.0f - damp * 0.5f);
                auto x = r.getX() + t * r.getWidth(), y = r.getCentreY() + (rnd.nextFloat() - 0.5f) * r.getHeight() * 0.9f * (0.3f + size);
                g.setColour (accent.withAlpha (juce::jlimit (0.0f, 1.0f, amp)));
                auto d = 2.0f + amp * 4.0f;
                g.fillEllipse (x - d * 0.5f, y - d * 0.5f, d, d);
            }
            juce::Path env;
            for (int i = 0; i <= (int) r.getWidth(); i += 3) { auto t = (float) i / r.getWidth(); auto a = std::exp (-t * 3.0f / decay); auto y = r.getBottom() - a * r.getHeight() * 0.9f; if (i == 0) env.startNewSubPath (r.getX(), y); else env.lineTo (r.getX() + (float) i, y); }
            g.setColour (accent.withAlpha (0.7f)); g.strokePath (env, juce::PathStrokeType (1.5f));
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::dim());
            g.drawText ("decay ~" + juce::String (decay, 1) + " s", r.toNearestInt().reduced (6, 4), juce::Justification::topLeft);
        }
    };

    struct DelayViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            float fbNorm = v ("Feedback", v ("Feedbak", 0.3f)), mix = v ("Mix proportion", v ("Wet", 0.3f));
            float ms = 300.0f;
            if (auto d = dynamic_cast<te::DelayPlugin*> (&plugin)) { ms = (float) d->lengthMs.get(); fbNorm = juce::Decibels::decibelsToGain (d->feedbackDb->getCurrentValue()); }
            else ms = 40.0f + v ("Delay", 0.5f) * 900.0f;
            auto span = juce::jmax (1200.0f, ms * 6.0f);
            g.setColour (Theme::text().withAlpha (0.1f)); g.drawHorizontalLine ((int) r.getBottom(), r.getX(), r.getRight());
            float amp = 1.0f;
            for (int i = 0; i < 12 && amp > 0.02f; ++i)
            {
                auto x = r.getX() + (ms * (float) i) / span * r.getWidth();
                if (x > r.getRight()) break;
                auto hgt = r.getHeight() * (i == 0 ? 0.9f : juce::jlimit (0.0f, 0.9f, amp * mix * 1.6f));
                g.setColour (i == 0 ? Theme::text().withAlpha (0.7f) : accent.withAlpha (0.3f + 0.7f * amp));
                g.fillRoundedRectangle (x - 3.0f, r.getBottom() - hgt, 6.0f, hgt, 2.0f);
                amp *= juce::jlimit (0.0f, 0.98f, fbNorm);
            }
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::dim());
            g.drawText (juce::String ((int) ms) + " ms", r.toNearestInt().reduced (6, 4), juce::Justification::topRight);
        }
    };

    struct IRViz : public Viz   // thumbnail of the loaded impulse response
    {
        using Viz::Viz;
        juce::AudioBuffer<float> ir; juce::String loaded;
        void load (const juce::File& f)
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f)))
            {
                auto n = (int) juce::jmin ((juce::int64) 44100 * 4, reader->lengthInSamples);
                ir.setSize (1, n); reader->read (&ir, 0, n, 0, true, false);
                loaded = f.getFileName();
            }
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto file = juce::File (plugin.state.getProperty ("sqmIRPath").toString());
            if (file.existsAsFile() && loaded != file.getFileName()) load (file);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r, 4.0f);
            if (ir.getNumSamples() == 0)
            {
                g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f));
                g.drawText ("No impulse response loaded - click LOAD IR and pick a WAV (a hall, a plate, a guitar cab...)", r.toNearestInt(), juce::Justification::centred);
                return;
            }
            auto n = ir.getNumSamples();
            juce::Path p; p.startNewSubPath (r.getX(), r.getCentreY());
            for (int i = 0; i < (int) r.getWidth(); ++i)
            {
                auto s0 = (int) ((float) i / r.getWidth() * (float) n), s1 = juce::jmin (n, (int) ((float) (i + 1) / r.getWidth() * (float) n) + 1);
                float mx = 0.0f; for (int s = s0; s < s1; ++s) mx = juce::jmax (mx, std::abs (ir.getSample (0, s)));
                p.lineTo (r.getX() + (float) i, r.getCentreY() - mx * r.getHeight() * 0.48f);
            }
            for (int i = (int) r.getWidth(); i >= 0; --i)
            {
                auto s0 = (int) ((float) i / r.getWidth() * (float) n), s1 = juce::jmin (n, (int) ((float) (i + 1) / r.getWidth() * (float) n) + 1);
                float mx = 0.0f; for (int s = s0; s < s1; ++s) mx = juce::jmax (mx, std::abs (ir.getSample (0, s)));
                p.lineTo (r.getX() + (float) i, r.getCentreY() + mx * r.getHeight() * 0.48f);
            }
            p.closeSubPath();
            g.setColour (accent.withAlpha (0.8f)); g.fillPath (p);
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::text());
            g.drawText (loaded + "  (" + juce::String ((float) n / 44100.0f, 2) + " s)", r.toNearestInt().reduced (6, 4), juce::Justification::topLeft);
        }
    };

    // --- Modulation ---------------------------------------------------------------
    struct LfoViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            float rate = 1.0f, depth = 0.5f; juce::String shape = "sine";
            if (auto c = dynamic_cast<te::ChorusPlugin*> (&plugin)) { rate = c->speedHz.get(); depth = juce::jlimit (0.0f, 1.0f, c->depthMs.get() / 10.0f); }
            else if (auto ph = dynamic_cast<te::PhaserPlugin*> (&plugin)) { rate = ph->rate.get(); depth = juce::jlimit (0.0f, 1.0f, ph->depth.get() / 10.0f); shape = "phase"; }
            else if (h ("Speed")) { rate = 0.2f + v ("Speed") * 8.0f; depth = v ("Depth", 1.0f); shape = "tremolo"; }
            else if (h ("Ensemble")) { rate = 0.5f + v ("Ensemble") * 2.0f; depth = 0.3f + v ("Fullness") * 0.7f; shape = "ensemble"; }
            g.setColour (Theme::text().withAlpha (0.08f)); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
            auto cycles = juce::jlimit (1.0f, 12.0f, rate * 2.0f);
            if (shape == "ensemble")
                for (int vcs = 0; vcs < 4; ++vcs)
                    drawWave (g, r, [=] (float t) { return std::sin ((t * cycles + phase * 0.3f + (float) vcs * 0.25f) * 6.283f) * depth * (0.5f + (float) vcs * 0.15f); }, accent.withAlpha (0.4f + (float) vcs * 0.15f), 1.5f);
            else if (shape == "tremolo")
                drawWave (g, r, [=] (float t) { return std::sin (t * 60.0f) * (1.0f - depth * 0.5f * (1.0f + std::sin ((t * cycles + phase * 0.5f) * 6.283f))); }, accent, 1.5f);
            else
                drawWave (g, r, [=] (float t) { return std::sin ((t * cycles + phase * 0.5f) * 6.283f) * depth; }, accent, 2.2f);
            // playhead dot
            auto t = std::fmod (phase * 0.5f, 1.0f);
            g.setColour (Theme::text()); g.fillEllipse (r.getX() + t * r.getWidth() - 3.0f, r.getY() + 4.0f, 6.0f, 6.0f);
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::dim());
            g.drawText (juce::String (rate, 2) + " Hz", r.toNearestInt().reduced (6, 4), juce::Justification::topRight);
        }
    };

    struct PitchViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            float semis = 0.0f;
            if (auto p = h ("Semitones")) if (p->param) semis = p->param->getCurrentValue();
            // keyboard: two octaves, highlight the shift from C
            const int keys = 25; auto kw = r.getWidth() / (float) keys;
            for (int i = 0; i < keys; ++i)
            {
                int n = i % 12; bool black = n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
                auto k = juce::Rectangle<float> (r.getX() + kw * (float) i, r.getY(), kw - 1.0f, r.getHeight() - 14.0f);
                int rel = i - 12;
                bool hl = rel == 0 || rel == juce::roundToInt (semis);
                g.setColour (hl ? (rel == 0 ? Theme::text() : accent) : (black ? juce::Colours::black.withAlpha (0.7f) : Theme::panel2().withAlpha (0.8f)));
                g.fillRoundedRectangle (k, 2.0f);
            }
            g.setFont (Theme::font (9.5f, true)); g.setColour (Theme::text());
            g.drawText ((semis >= 0 ? "+" : "") + juce::String (semis, 2) + " semitones", r.withTrimmedTop (r.getHeight() - 13.0f).toNearestInt(), juce::Justification::centred);
        }
    };

    struct WidthViz : public Viz   // stereo vector field
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto sq = r.withSizeKeepingCentre (r.getHeight(), r.getHeight());
            auto width = h ("Width") ? v ("Width") * 2.0f : (h ("Monotor") ? 1.0f - v ("Monotor") * 0.9f : 1.0f);
            auto centre = v ("Center", 0.5f) * 2.0f;
            g.setColour (Theme::text().withAlpha (0.1f));
            g.drawLine (sq.getX(), sq.getBottom(), sq.getRight(), sq.getY()); g.drawLine (sq.getX(), sq.getY(), sq.getRight(), sq.getBottom());
            g.drawLine (sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom());
            juce::Random rnd (7);
            auto lvl = juce::jlimit (0.15f, 1.0f, (activityDb() + 50.0f) / 50.0f);
            for (int i = 0; i < 160; ++i)
            {
                auto m = (rnd.nextFloat() - 0.5f) * 2.0f * centre * lvl, s = (rnd.nextFloat() - 0.5f) * 2.0f * width * 0.6f * lvl;
                auto x = sq.getCentreX() + s * sq.getWidth() * 0.45f, y = sq.getCentreY() - std::abs (m) * sq.getHeight() * 0.45f + (rnd.nextFloat() - 0.5f) * 4.0f;
                g.setColour (accent.withAlpha (0.35f + rnd.nextFloat() * 0.5f));
                g.fillEllipse (x - 1.5f, y - 1.5f, 3.0f, 3.0f);
            }
            g.setFont (Theme::font (9.0f)); g.setColour (Theme::dim());
            g.drawText ("L", sq.toNearestInt().reduced (4, 2), juce::Justification::bottomLeft);
            g.drawText ("R", sq.toNearestInt().reduced (4, 2), juce::Justification::bottomRight);
            if (h ("Monotor")) { auto f = 20.0f * std::pow (50.0f, v ("Monotor")); g.setColour (Theme::text()); g.drawText (v ("Monotor") <= 0.0f ? "mono maker off" : "mono below ~" + juce::String ((int) f) + " Hz", r.toNearestInt().reduced (6, 4), juce::Justification::topRight); }
        }
    };

    struct DitherViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            juce::Random rnd ((juce::int64) (phase * 10.0f));
            for (int i = 0; i < 500; ++i)
            {
                auto x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                g.setColour (accent.withAlpha (0.15f + rnd.nextFloat() * 0.4f));
                g.fillRect (x, y, 1.5f, 1.5f);
            }
            g.setFont (Theme::font (11.0f)); g.setColour (Theme::text());
            g.drawText ("Noise-shaped dither - put this LAST on the master before a 16/24-bit bounce", r.toNearestInt(), juce::Justification::centred);
        }
    };

    struct TrimViz : public Viz   // analogue VU needle
    {
        using Viz::Viz;
        float needle = -20.0f;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto face = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight() * 1.9f), r.getHeight());
            g.setColour (juce::Colour (0xfff2e3c0)); g.fillRoundedRectangle (face, 6.0f);
            g.setColour (juce::Colour (0xff8a7a55)); g.drawRoundedRectangle (face, 6.0f, 1.5f);
            auto gainDb = (v ("Gain", 0.5f) - 0.5f) * 80.0f;
            auto target = juce::jlimit (-20.0f, 3.0f, activityDb() + gainDb);
            needle += (target - needle) * 0.25f;
            // pivot below the face; the scale is an arc near the top of the face
            auto c = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
            auto rad = face.getHeight() * 1.3f;
            auto angleFor = [] (float db) { return ((db + 20.0f) / 23.0f - 0.5f) * 1.1f; };   // 0 = straight up
            juce::Path arc; arc.addCentredArc (c.x, c.y, rad * 0.92f, rad * 0.92f, 0.0f, angleFor (-20.0f), angleFor (3.0f), true);
            g.setColour (juce::Colours::black); g.strokePath (arc, juce::PathStrokeType (1.2f));
            juce::Path red; red.addCentredArc (c.x, c.y, rad * 0.92f, rad * 0.92f, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
            g.setColour (Theme::red()); g.strokePath (red, juce::PathStrokeType (3.0f));
            for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
            {
                auto a = angleFor (db);
                g.setColour (db > 0 ? Theme::red() : juce::Colours::black);
                auto p1 = c.getPointOnCircumference (rad * 0.88f, a), p2 = c.getPointOnCircumference (rad * 0.95f, a);
                g.drawLine (p1.x, p1.y, p2.x, p2.y, db == 0.0f ? 2.0f : 1.0f);
                g.setFont (Theme::font (8.0f, true));
                auto tp = c.getPointOnCircumference (rad * 0.8f, a);
                g.drawText (juce::String ((int) db), (int) tp.x - 10, (int) tp.y - 6, 20, 12, juce::Justification::centred);
            }
            auto a = angleFor (needle);
            auto tip = c.getPointOnCircumference (rad * 0.96f, a), base = c.getPointOnCircumference (rad * 0.42f, a);
            g.setColour (juce::Colours::black); g.drawLine (base.x, base.y, tip.x, tip.y, 1.8f);
            g.setColour (juce::Colours::black.withAlpha (0.7f)); g.setFont (Theme::font (9.0f, true));
            g.drawText ("VU", face.toNearestInt().reduced (8, 4), juce::Justification::bottomLeft);
            g.drawText ((gainDb >= 0 ? "+" : "") + juce::String (gainDb, 1) + " dB", face.toNearestInt().reduced (8, 4), juce::Justification::bottomRight);
        }
    };

    struct MidiViz : public Viz
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            float semis = 0.0f; if (auto p = h ("Semitones")) if (p->param) semis = p->param->getCurrentValue();
            const int keys = 36; auto kw = r.getWidth() / (float) keys;
            for (int i = 0; i < keys; ++i)
            {
                int n = i % 12; bool black = n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
                auto k = juce::Rectangle<float> (r.getX() + kw * (float) i, r.getY(), kw - 1.0f, r.getHeight() - 14.0f);
                int rel = i - 12;
                bool src = rel == 0, dst = rel == juce::roundToInt (semis);
                g.setColour (dst ? accent : src ? Theme::text() : (black ? juce::Colours::black.withAlpha (0.7f) : Theme::panel2().withAlpha (0.8f)));
                g.fillRoundedRectangle (k, 2.0f);
            }
            g.setFont (Theme::font (9.5f, true)); g.setColour (Theme::text());
            g.drawText ("notes in: C  ->  out: " + juce::MidiMessage::getMidiNoteName (60 + juce::roundToInt (semis), true, true, 4) + "  (" + juce::String (semis, 0) + " st)", r.withTrimmedTop (r.getHeight() - 13.0f).toNearestInt(), juce::Justification::centred);
        }
    };

    struct SynthViz : public Viz   // the four oscillators + amp / filter envelopes
    {
        using Viz::Viz;
        static float wave (int shape, float t, float pw)
        {
            auto ph = std::fmod (t, 1.0f);
            switch (shape) { case 1: return std::sin (ph * 6.283f); case 2: return ph < 0.5f ? 4.0f * ph - 1.0f : -4.0f * ph + 3.0f; case 3: return ph * 2.0f - 1.0f; case 4: return 1.0f - ph * 2.0f; case 5: return ph < pw ? 1.0f : -1.0f; case 6: return std::sin (ph * 977.0f) * std::sin (ph * 311.0f); default: return 0.0f; }
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto fo = dynamic_cast<te::FourOscPlugin*> (&plugin);
            if (fo == nullptr) return;
            auto oscArea = r.removeFromLeft (r.getWidth() * 0.5f);
            r.removeFromLeft (8.0f);
            auto each = oscArea.getHeight() / 4.0f;
            const juce::Colour cols[] = { Theme::violet(), Theme::magenta(), Theme::acid(), Theme::mint() };
            for (int i = 0; i < 4; ++i)
            {
                auto row = juce::Rectangle<float> (oscArea.getX(), oscArea.getY() + each * (float) i, oscArea.getWidth(), each).reduced (0.0f, 1.0f);
                int shape = (int) plugin.state.getProperty ("waveShape" + juce::String (i + 1), 0);
                float pw = (float) plugin.state.getProperty ("pulseWidth" + juce::String (i + 1), 0.5f);
                float lvl = juce::Decibels::decibelsToGain ((float) plugin.state.getProperty ("level" + juce::String (i + 1), 0.0f));
                g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (row, 3.0f);
                g.setFont (Theme::font (8.5f, true)); g.setColour (cols[i].withAlpha (0.9f));
                g.drawText (juce::String (i + 1), row.toNearestInt().reduced (4, 0), juce::Justification::centredLeft);
                if (shape == 0) { g.setColour (Theme::dim()); g.drawText ("off", row.toNearestInt(), juce::Justification::centred); continue; }
                drawWave (g, row.withTrimmedLeft (14.0f), [=] (float t) { return wave (shape, t * 2.0f + phase * 0.2f, pw) * lvl * 0.9f; }, cols[i], 1.6f);
            }
            // envelopes
            auto drawEnv = [&] (juce::Rectangle<float> area, const char* pre, juce::Colour c, const juce::String& title)
            {
                g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (area, 3.0f);
                float a = (float) plugin.state.getProperty (juce::String (pre) + "Attack", 0.1f), d = (float) plugin.state.getProperty (juce::String (pre) + "Decay", 0.1f);
                float s = (float) plugin.state.getProperty (juce::String (pre) + "Sustain", 80.0f) / 100.0f, rl = (float) plugin.state.getProperty (juce::String (pre) + "Release", 0.1f);
                auto norm = [] (float sec) { return juce::jlimit (0.03f, 1.0f, std::log (1.0f + sec * 4.0f) / std::log (9.0f)); };
                float wa = norm (a), wd = norm (d), wr = norm (rl); float total = wa + wd + 0.6f + wr;
                auto in = area.reduced (6.0f, 8.0f);
                juce::Path p; p.startNewSubPath (in.getX(), in.getBottom());
                float x = in.getX();
                x += wa / total * in.getWidth(); p.lineTo (x, in.getY());
                x += wd / total * in.getWidth(); p.lineTo (x, in.getBottom() - s * in.getHeight());
                x += 0.6f / total * in.getWidth(); p.lineTo (x, in.getBottom() - s * in.getHeight());
                x += wr / total * in.getWidth(); p.lineTo (x, in.getBottom());
                g.setColour (c.withAlpha (0.2f)); juce::Path f (p); f.closeSubPath(); g.fillPath (f);
                g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.8f));
                g.setFont (Theme::font (8.5f, true)); g.setColour (Theme::dim());
                g.drawText (title, area.toNearestInt().reduced (5, 2), juce::Justification::topLeft);
            };
            auto top = r.removeFromTop (r.getHeight() * 0.5f).reduced (0.0f, 1.0f);
            drawEnv (top, "amp", Theme::acid(), "AMP ENVELOPE");
            drawEnv (r.reduced (0.0f, 1.0f), "filter", Theme::mint(), "FILTER ENVELOPE");
        }
    };

    struct SamplerViz : public Viz   // the zone map on a keyboard
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto sp = dynamic_cast<te::SamplerPlugin*> (&plugin);
            if (sp == nullptr) return;
            const int lo = 24, hi = 108; auto kw = r.getWidth() / (float) (hi - lo);
            auto kb = r.removeFromBottom (26.0f);
            for (int n = lo; n < hi; ++n)
            {
                int k = n % 12; bool black = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
                g.setColour (black ? juce::Colours::black.withAlpha (0.8f) : Theme::text().withAlpha (0.8f));
                g.fillRect (kb.getX() + kw * (float) (n - lo), kb.getY(), kw - 0.5f, black ? kb.getHeight() * 0.6f : kb.getHeight());
            }
            auto n = sp->getNumSounds();
            if (n == 0) { g.setColour (Theme::dim()); g.setFont (Theme::font (11.0f)); g.drawText ("No samples yet - click ADD SAMPLE (or drop WAVs on the window) and set the root / range", r.toNearestInt(), juce::Justification::centred); return; }
            auto rowH = juce::jmin (18.0f, r.getHeight() / (float) n);
            for (int i = 0; i < n; ++i)
            {
                auto mn = sp->getMinKey (i), mx = sp->getMaxKey (i), root = sp->getKeyNote (i);
                auto x0 = kb.getX() + kw * (float) juce::jlimit (lo, hi, mn) , x1 = kb.getX() + kw * (float) (juce::jlimit (lo, hi, mx) + 1);
                auto zone = juce::Rectangle<float> (x0, r.getY() + rowH * (float) i, juce::jmax (4.0f, x1 - x0), rowH - 2.0f);
                g.setColour (accent.withAlpha (0.35f + 0.1f * (float) (i % 3))); g.fillRoundedRectangle (zone, 3.0f);
                g.setColour (Theme::text()); g.setFont (Theme::font (9.0f, true));
                g.drawFittedText (sp->getSoundName (i), zone.toNearestInt().reduced (3, 0), juce::Justification::centredLeft, 1);
                auto rx = kb.getX() + kw * (float) juce::jlimit (lo, hi, root) + kw * 0.5f;
                g.setColour (Theme::amber()); g.fillEllipse (rx - 3.0f, zone.getCentreY() - 3.0f, 6.0f, 6.0f);
            }
        }
    };

    //==============================================================================
    // --- Squelchmoth native instruments -------------------------------------------
    struct EightOhEightViz : public Viz   // the punch / glide pitch curve, the drive curve and a live scope
    {
        using Viz::Viz;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto* p = dynamic_cast<Sq808Plugin*> (&plugin);
            if (p == nullptr) return;
            auto left = r.removeFromLeft (r.getWidth() * 0.38f);
            r.removeFromLeft (8.0f);
            auto mid = r.removeFromLeft (r.getWidth() * 0.32f);
            r.removeFromLeft (8.0f);
            auto box = [&] (juce::Rectangle<float> a, const char* t)
            {
                g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (a, 4.0f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
                g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
            };
            // pitch envelope: punch (semitones over time) on a 0-400 ms axis
            box (left, "PITCH  (punch + decay)");
            {
                auto in = left.reduced (8.0f, 16.0f);
                const float punch = p->bank[Sq808Plugin::punch], pt = juce::jmax (0.001f, p->bank[Sq808Plugin::punchTime]);
                const float decay = juce::jmax (0.05f, p->bank[Sq808Plugin::decay]);
                juce::Path pitch, amp;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float t = (float) i / in.getWidth() * juce::jmin (2.5f, decay);
                    const float st = punch * std::exp (-t / pt);
                    const float y = in.getBottom() - juce::jlimit (0.0f, 1.0f, st / 36.0f) * in.getHeight() * 0.9f - in.getHeight() * 0.06f;
                    const float a = std::exp (-6.9f * t / decay);
                    const float ya = in.getBottom() - a * in.getHeight();
                    if (i == 0) { pitch.startNewSubPath (in.getX(), y); amp.startNewSubPath (in.getX(), ya); }
                    else        { pitch.lineTo (in.getX() + (float) i, y); amp.lineTo (in.getX() + (float) i, ya); }
                }
                g.setColour (accent.withAlpha (0.25f)); g.strokePath (amp, juce::PathStrokeType (1.2f));
                g.setColour (accent); g.strokePath (pitch, juce::PathStrokeType (2.2f));
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f));
                g.drawText ("+" + juce::String (juce::roundToInt (punch)) + " st", in.toNearestInt().removeFromTop (12), juce::Justification::topRight);
                g.drawText (juce::String (juce::jmin (2.5f, decay), 1) + " s", in.toNearestInt().removeFromBottom (12), juce::Justification::bottomRight);
            }
            // drive transfer curve
            box (mid, juce::String ("DRIVE  " + Sq808Plugin::driveTypeNames()[juce::jlimit (0, 3, p->driveType.get())]).toRawUTF8());
            {
                auto in = mid.reduced (10.0f, 16.0f);
                const float drv = p->bank[Sq808Plugin::drive];
                const float gg = 1.0f + drv * drv * 29.0f;
                const int type = p->driveType.get();
                juce::Path curve;
                for (int i = 0; i <= (int) in.getWidth(); ++i)
                {
                    const float x = (float) i / in.getWidth() * 2.0f - 1.0f;
                    float y = 0;
                    switch (type)
                    {
                        case 1:  { const float b = 0.18f; y = (std::tanh (gg * x + b) - std::tanh (b)) / std::tanh (gg + b); break; }
                        case 2:  y = juce::jlimit (-1.0f, 1.0f, gg * x) * 0.9f; break;
                        case 3:  y = std::sin (juce::jlimit (-8.0f, 8.0f, gg * x) * 1.5707963f) * 0.9f; break;
                        default: y = std::tanh (gg * x) / std::tanh (gg); break;
                    }
                    const float px = in.getX() + (float) i, py = in.getCentreY() - y * in.getHeight() * 0.48f;
                    if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
                }
                g.setColour (Theme::text().withAlpha (0.12f));
                g.drawLine (in.getX(), in.getCentreY(), in.getRight(), in.getCentreY());
                g.drawLine (in.getCentreX(), in.getY(), in.getCentreX(), in.getBottom());
                g.setColour (Theme::magenta()); g.strokePath (curve, juce::PathStrokeType (2.0f));
            }
            // live scope + the note that is playing
            box (r, "OUTPUT");
            {
                auto in = r.reduced (8.0f, 16.0f);
                const int w = p->scopeWrite.load();
                juce::Path sc;
                for (int i = 0; i < 512; ++i)
                {
                    const float v = p->scope[(size_t) ((w + i) & 511)];
                    const float x = in.getX() + in.getWidth() * (float) i / 511.0f, y = in.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * in.getHeight() * 0.48f;
                    if (i == 0) sc.startNewSubPath (x, y); else sc.lineTo (x, y);
                }
                g.setColour (Theme::acid()); g.strokePath (sc, juce::PathStrokeType (1.6f));
                const float note = p->displayNote.load();
                if (note >= 0 && p->displayEnv.load() > 0.01f)
                {
                    g.setColour (Theme::text()); g.setFont (Theme::font (16.0f, true));
                    g.drawText (juce::MidiMessage::getMidiNoteName (juce::roundToInt (note), true, true, 4) + "  " + juce::String (440.0 * std::pow (2.0, (note - 69.0) / 12.0), 1) + " Hz",
                                in.toNearestInt(), juce::Justification::topRight);
                }
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f));
                g.drawText ("GLIDE: " + Sq808Plugin::glideModeNames()[juce::jlimit (0, 2, p->glideMode.get())] + (p->glideMode.get() == 0 ? "  (overlap notes to slide)" : ""),
                            in.toNearestInt(), juce::Justification::bottomLeft);
            }
        }
    };

    struct BlossomViz : public Viz   // vibrating strings with exciter and pickup, body resonances, and the material pad
    {
        BlossomViz (HandleMap& h, te::Plugin& p, juce::Colour c) : Viz (h, p, c) { setInterceptsMouseClicks (true, false); }

        juce::Rectangle<float> padArea() const
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            return r.removeFromRight (r.getHeight() * 1.25f).reduced (2.0f);
        }
        void mouseDown (const juce::MouseEvent& e) override   { setPad (e.position); }
        void mouseDrag (const juce::MouseEvent& e) override   { setPad (e.position); }
        void setPad (juce::Point<float> pt)
        {
            auto pad = padArea().reduced (10.0f, 14.0f);
            if (! padArea().contains (pt)) return;
            if (auto* hs = h ("Stiffness")) hs->set (juce::jlimit (0.0f, 1.0f, (pt.x - pad.getX()) / pad.getWidth()));
            if (auto* hd = h ("Damping"))   hd->set (juce::jlimit (0.0f, 1.0f, 1.0f - (pt.y - pad.getY()) / pad.getHeight()));
            repaint();
            if (onPad) onPad();
        }
        std::function<void()> onPad;

        void paint (juce::Graphics& g) override
        {
            auto* p = dynamic_cast<SqBlossomPlugin*> (&plugin);
            if (p == nullptr) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            auto pad = padArea();
            r.removeFromRight (pad.getWidth() + 10.0f);
            auto strings = r.removeFromTop (r.getHeight() * 0.62f);
            r.removeFromTop (6.0f);
            auto box = [&] (juce::Rectangle<float> a, const juce::String& t)
            {
                g.setColour (juce::Colours::black.withAlpha (0.32f)); g.fillRoundedRectangle (a, 4.0f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
                g.drawText (t, a.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
            };
            // strings
            const int exc = juce::jlimit (0, 2, p->exciterType.get());
            box (strings, "STRINGS  |  " + SqBlossomPlugin::exciterNames()[exc] + "  |  " + juce::String (p->activeVoices.load()) + " voices");
            {
                auto in = strings.reduced (14.0f, 18.0f);
                const float energy = juce::jlimit (0.0f, 1.0f, std::sqrt (p->displayEnergy.load()) * 2.6f);
                const float pos = p->bank[SqBlossomPlugin::position], pick = p->bank[SqBlossomPlugin::pickup];
                const float ratio = p->bank[SqBlossomPlugin::s2Ratio], mix2 = p->bank[SqBlossomPlugin::s2Mix];
                const float stiff = p->bank[SqBlossomPlugin::stiffness];
                for (int sIdx = 0; sIdx < 2; ++sIdx)
                {
                    const float y0 = in.getY() + in.getHeight() * (sIdx == 0 ? 0.32f : 0.72f);
                    const float amp = in.getHeight() * 0.22f * energy * (sIdx == 0 ? 1.0f : juce::jmax (0.25f, mix2));
                    const float harm = sIdx == 0 ? 1.0f : ratio;
                    juce::Path sp;
                    for (int i = 0; i <= (int) in.getWidth(); i += 2)
                    {
                        const float x = (float) i / in.getWidth();
                        float d = std::sin (x * juce::MathConstants<float>::pi) * std::sin (phase * 9.0f * harm)
                                + 0.35f * std::sin (x * 2.0f * juce::MathConstants<float>::pi) * std::sin (phase * 18.0f * harm * (1.0f + stiff * 0.08f))
                                + 0.2f * std::sin (x * 3.0f * juce::MathConstants<float>::pi) * std::sin (phase * 27.0f * harm * (1.0f + stiff * 0.2f));
                        const float yy = y0 + d * amp;
                        if (i == 0) sp.startNewSubPath (in.getX(), yy); else sp.lineTo (in.getX() + (float) i, yy);
                    }
                    g.setColour ((sIdx == 0 ? accent : Theme::violet().brighter (0.3f)).withAlpha (0.25f + 0.6f * energy));
                    g.strokePath (sp, juce::PathStrokeType (sIdx == 0 ? 2.2f : 1.6f));
                    g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f));
                    g.drawText (sIdx == 0 ? "S1" : "S2 x" + juce::String (ratio, 2), juce::Rectangle<float> (in.getX() - 12.0f, y0 - 14.0f, 40.0f, 10.0f), juce::Justification::left);
                }
                // bridge at the left, nut at the right; exciter and pickup markers
                g.setColour (Theme::text().withAlpha (0.4f));
                g.fillRect (in.getX() - 3.0f, in.getY(), 3.0f, in.getHeight());
                g.fillRect (in.getRight(), in.getY(), 3.0f, in.getHeight());
                const float ex = in.getX() + in.getWidth() * pos;
                g.setColour (Theme::magenta()); g.drawLine (ex, in.getY(), ex, in.getBottom(), 2.0f);
                g.setFont (Theme::font (8.0f, true)); g.drawText (exc == 2 ? "BOW" : exc == 1 ? "HAMMER" : "PLUCK", juce::Rectangle<float> (ex - 30.0f, in.getBottom() - 1.0f, 60.0f, 10.0f), juce::Justification::centred);
                const float px = in.getX() + in.getWidth() * pos * pick;
                g.setColour (Theme::mint()); g.fillEllipse (px - 4.0f, in.getCentreY() - 4.0f, 8.0f, 8.0f);
                g.drawText ("PICKUP", juce::Rectangle<float> (px - 30.0f, in.getCentreY() + 5.0f, 60.0f, 10.0f), juce::Justification::centred);
            }
            // body resonances on a log frequency axis
            const int body = juce::jlimit (0, 7, p->bodyType.get());
            box (r, "BODY  |  " + SqBlossomPlugin::bodyNames()[body]);
            {
                auto in = r.reduced (10.0f, 16.0f);
                const float size = p->bank[SqBlossomPlugin::bodySize];
                for (auto& m : SqBlossomPlugin::bodyTable()[(size_t) body])
                {
                    if (m.g <= 0) continue;
                    const float f = m.f / size;
                    const float x = in.getX() + in.getWidth() * (std::log (f / 50.0f) / std::log (300.0f));
                    const float hgt = in.getHeight() * m.g * (0.3f + 0.7f * p->bank[SqBlossomPlugin::bodyMix]);
                    g.setColour (accent.withAlpha (0.8f));
                    g.fillRoundedRectangle (x - 2.0f, in.getBottom() - hgt, 4.0f, hgt, 2.0f);
                }
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f));
                for (float hz : { 100.0f, 1000.0f, 10000.0f })
                    g.drawText (hz >= 1000 ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String ((int) hz), (int) (in.getX() + in.getWidth() * (std::log (hz / 50.0f) / std::log (300.0f))) - 10, (int) in.getBottom() - 9, 30, 10, juce::Justification::left);
            }
            // material pad
            box (pad, "MATERIAL  (drag)");
            {
                auto in = pad.reduced (10.0f, 14.0f);
                g.setColour (Theme::text().withAlpha (0.08f));
                g.drawRect (in, 1.0f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.5f, true));
                g.drawText ("NYLON", in.toNearestInt().reduced (3), juce::Justification::topLeft);
                g.drawText ("STEEL / GLASS", in.toNearestInt().reduced (3), juce::Justification::topRight);
                g.drawText ("GUT / FELT", in.toNearestInt().reduced (3), juce::Justification::bottomLeft);
                g.drawText ("WOOD / BAR", in.toNearestInt().reduced (3), juce::Justification::bottomRight);
                const float sx = in.getX() + in.getWidth() * v ("Stiffness", 0.1f);
                const float sy = in.getBottom() - in.getHeight() * v ("Damping", 0.35f);
                g.setColour (accent.withAlpha (0.25f)); g.fillEllipse (sx - 14.0f, sy - 14.0f, 28.0f, 28.0f);
                g.setColour (accent); g.fillEllipse (sx - 6.0f, sy - 6.0f, 12.0f, 12.0f);
                g.setColour (Theme::dim()); g.setFont (Theme::font (8.0f));
                g.drawText ("stiffness >", in.toNearestInt().withTrimmedTop ((int) in.getHeight() + 2).withHeight (11), juce::Justification::centred);
            }
        }
    };

    /** App hooks for buttons that need the rest of Squelchmoth (set by MainComponent). */
    struct Hooks { static inline std::function<void (te::Plugin&)> openTuneGraph, detectKey; static inline std::function<void (te::Plugin&, int deck)> slicerDump, slicerToChannels; };
    #include "SkinsVocal.inl"
    #include "SkinsApex.inl"
    #include "SkinsColour.inl"
    #include "SkinsColor111.inl"
    #include "SkinsVoice111.inl"
    #include "SkinsColor111b.inl"
    #include "SkinsColor111c.inl"
    #include "SkinsColor111d.inl"
    #include "SkinsVoice111e.inl"
    #include "SkinsVoice111f.inl"
    #include "SkinsSynths.inl"
    #include "SkinsSynths2.inl"
    #include "SkinsFx181.inl"
    #include "SkinsSamplers2.inl"
    #include "SkinsPads.inl"

    enum class Family { eq, filter, tilt, air, sub, dyn, gate, deess, transient, sat, tape, console, vinyl, bits, reverb, delay, ir, lfo, pitch, width, dither, trim, midi, synth, sampler, eight08, blossom, tune, tunefx, apexdyn, apexlim, apexmb, apexsub, aurora, opto, pulse, oilcan, ladder, ring, phase12, bbd, mothsynth2, venom, simmoth, minimoth, thump, dial10, hivepads, hexwing, toxin, lantern, lanternexo, larvabass, thornbass, carapace, livedrum, drumpad, thumplive, timeweaver, larvafilter, triad, chrysalis2, mandible, pollen, cinder, wingbeat, foilplate, radiant, maw, mothkey, sirencomp, sireneq, hushdeess2, vellum, tremor, orbitpan, mirrorshift, shutter, shard, murmuration, masquerade, larva2, phase2, hiverack, sirenverb, miccloset, larynx, exhale, hivechoir, doublewing2, mutant, generic };

    /** App hooks for buttons that need the rest of Squelchmoth (set by MainComponent). */
    static inline std::function<juce::Array<juce::File> (int kit)> loadKitHook;   // renders / finds a kit's files (needs the Session)

    struct Group { juce::String title; juce::StringArray params; bool big = false; int cols = 0; };
    struct Spec
    {
        Family family = Family::generic;
        KnobStyle knob = KnobStyle::ring;
        std::vector<Group> groups;
        int width = 640, height = 470;
        int vizH = 0;          // 0 = automatic
        int rowSplit = -1;     // >= 0: groups from this index go on a second row
        float row1Share = 0.4f;
        bool resizable = false;   // the window can be dragged larger or smaller (same proportions)
        juce::StringArray hiddenPrefixes;   // parameters drawn by the visualiser instead of knobs
        bool hideWetDry = true;     // AirWindows add Dry/Wet to everything - keep them in a 'Mix' group instead
    };

    inline Spec specFor (const juce::String& name)
    {
        Spec s;
        auto G = [] (const char* t, juce::StringArray p, bool big = false, int cols = 0) { return Group { t, p, big, cols }; };
        if (name == "Mothsynth")          { s.family = Family::synth; s.knob = KnobStyle::ring; s.width = 940; s.height = 640; }
        else if (name == "Chrysalis Sampler" || name == "Hive Drums") { s.family = Family::sampler; s.width = 720; s.height = 480; }
        else if (name == "Sample Channel") { s = specFor ("Hive Pads"); }
        else if (name == "Prism EQ")      { s.family = Family::eq; s.knob = KnobStyle::flat; s.width = 760; s.height = 500;
                                            s.groups = { G ("LOW SHELF", { "Low-shelf freq", "Low-shelf gain", "Low-shelf Q" }), G ("MID 1", { "Mid freq 1", "Mid gain 1", "Mid Q 1" }), G ("MID 2", { "Mid freq 2", "Mid gain 2", "Mid Q 2" }), G ("HIGH SHELF", { "High-shelf freq", "High-shelf gain", "High-shelf Q" }) }; }
        else if (name == "Scalpel")       { s.family = Family::filter; s.knob = KnobStyle::slim; s.groups = { G ("FILTER", { "Type", "Freq", "Q" }, true), G ("OUTPUT", { "Output", "Inv/Wet" }) }; }
        else if (name == "Lantern Tilt")  { s.family = Family::tilt; s.knob = KnobStyle::vintage; s.groups = { G ("TILT", { "Voicing", "Highs" }, true) }; }
        else if (name == "Antenna Air")   { s.family = Family::air; s.knob = KnobStyle::slim; s.groups = { G ("AIR BANDS", { "22K tap", "15K tap", "11K tap" }, true), G ("SHAPE", { "filters Q", "Output Level" }) }; }
        else if (name == "Deep Sub")      { s.family = Family::sub; s.knob = KnobStyle::chicken; s.groups = { G ("GRIND", { "TGrind", "Grd/Out" }), G ("BASS", { "XOver", "BsDrive", "BsVoice", "BassOut" }), G ("SUB", { "SbDrive", "SbVoice", "SubOut" }) }; }
        else if (name == "Veil Filter")   { s.family = Family::filter; s.knob = KnobStyle::ring; s.groups = { G ("FILTER", { "Mode", "Frequency" }, true) }; }
        else if (name == "Capacitor")     { s.family = Family::filter; s.knob = KnobStyle::vintage; s.groups = { G ("FILTERS", { "Lowpass", "Highpass" }, true) }; }
        else if (name == "Cocoon Compressor") { s.family = Family::dyn; s.knob = KnobStyle::ring; s.width = 700; s.groups = { G ("COMPRESSOR", { "Threshold", "Ratio", "Attack", "Release" }, true), G ("OUTPUT", { "Output gain" }), G ("SIDECHAIN", { "Sidechain gain", "External sidechain" }) }; }
        else if (name == "Moth Press")    { s.family = Family::dyn; s.knob = KnobStyle::vintage; s.groups = { G ("PRESSURE", { "Pressure" }, true), G ("CHARACTER", { "Speed", "Mewiness", "Output Gain" }) }; }
        else if (name == "Pupa Buss Comp") { s.family = Family::dyn; s.knob = KnobStyle::flat; s.groups = { G ("GLUE", { "Compress", "Output" }, true) }; }
        else if (name == "Vari-Mu Tube")  { s.family = Family::dyn; s.knob = KnobStyle::vintage; s.groups = { G ("TUBE COMPRESSOR", { "Intensty", "Speed", "Output" }, true) }; }
        else if (name == "Clamp Limiter") { s.family = Family::dyn; s.knob = KnobStyle::flat; s.groups = { G ("LIMITER", { "Boost", "Soften", "Enhance" }, true), G ("MODE", { "Mode" }) }; }
        else if (name == "Veil Gate")     { s.family = Family::gate; s.knob = KnobStyle::ring; s.groups = { G ("GATE", { "Thresh", "AttackS" }, true), G ("RELEASE TONE", { "TrebSus", "BassSus" }) }; }
        else if (name == "Hush De-Ess") { s.family = Family::deess; s.knob = KnobStyle::slim; s.groups = { G ("DE-ESSER", { "Intense", "Max DS", "Freq" }, true) }; }
        else if (name == "Snap Transient") { s.family = Family::transient; s.knob = KnobStyle::flat; s.groups = { G ("TRANSIENT", { "Point", "Reaction Speed" }, true), G ("INPUT", { "Input Trim" }) }; }
        else if (name == "Pop Knock")     { s.family = Family::transient; s.knob = KnobStyle::chicken; s.groups = { G ("KNOCK", { "Intenst", "Output" }, true) }; }
        else if (name == "Ember Tape")    { s.family = Family::tape; s.knob = KnobStyle::vintage; s.width = 700; s.groups = { G ("TAPE", { "Louder", "Softer", "Fatter", "Flutter" }, true), G ("OUTPUT", { "Output" }) }; }
        else if (name == "Scorch Drive")  { s.family = Family::sat; s.knob = KnobStyle::chicken; s.groups = { G ("DRIVE", { "Drive", "Highpass", "Out Level" }, true) }; }
        else if (name == "Lumen Channel" || name == "Lumen Buss") { s.family = Family::console; s.knob = KnobStyle::vintage; s.groups = { G ("CONSOLE", { "Input" }, true) }; }
        else if (name == "Ashen Desk")    { s.family = Family::sat; s.knob = KnobStyle::vintage; s.groups = { G ("DESK", { "Overdrive", "Hi Choke", "Power Sag", "Frequency" }, true), G ("OUTPUT", { "Output Trim" }) }; }
        else if (name == "Furnace Density") { s.family = Family::sat; s.knob = KnobStyle::ring; s.groups = { G ("DENSITY", { "Density", "Highpass", "Out Level" }, true) }; }
        else if (name == "Triode Glow")   { s.family = Family::sat; s.knob = KnobStyle::vintage; s.groups = { G ("TUBE STAGES", { "Triode", "Clas AB", "Clas B" }, true) }; }
        else if (name == "Vinyl Press")   { s.family = Family::vinyl; s.knob = KnobStyle::vintage; s.groups = { G ("CUTTING LATHE", { "Mid HiP", "SideHiP", "H Limit", "Gv Wear" }, true) }; }
        else if (name == "Grit Bits")     { s.family = Family::bits; s.knob = KnobStyle::flat; s.groups = { G ("DEGRADE", { "Rate", "Rez", "Hard" }, true) }; }
        else if (name == "Nebula Verb")   { s.family = Family::reverb; s.knob = KnobStyle::ring; s.width = 700; s.groups = { G ("SPACE", { "Room Size", "Damping", "Width" }, true), G ("MIX", { "Dry Level", "Wet Level", "Freeze" }) }; }
        else if (name == "Cavern IR")     { s.family = Family::ir; s.knob = KnobStyle::ring; s.width = 720; s.groups = { G ("IMPULSE", { "Load IR" }), G ("TONE", { "Low Cut", "High Cut", "Filter Q" }), G ("MIX", { "Mix", "Gain" }, true) }; }
        else if (name == "Pocket Spaces") { s.family = Family::reverb; s.knob = KnobStyle::chicken; s.groups = { G ("SPACE", { "Type", "Size", "Gating" }, true) }; }
        else if (name == "Starfield")     { s.family = Family::reverb; s.knob = KnobStyle::slim; s.groups = { G ("SHIMMER", { "Sustain", "Grain" }, true) }; }
        else if (name == "Dust Delay")    { s.family = Family::delay; s.knob = KnobStyle::ring; s.groups = { G ("DELAY", { "Time", "Sync", "Feedback", "Mix proportion" }, true) }; }
        else if (name == "Echo Wing")     { s.family = Family::delay; s.knob = KnobStyle::vintage; s.width = 720; s.groups = { G ("TAPE ECHO", { "Delay", "Feedbak", "Lean/Ft", "Depth" }, true), G ("MIX", { "Dry", "Wet" }) }; }
        else if (name == "Flutter Chorus") { s.family = Family::lfo; s.knob = KnobStyle::ring; s.groups = { G ("CHORUS", { "Depth", "Speed", "Width", "Mix" }, true) }; }
        else if (name == "Swarm Ensemble") { s.family = Family::lfo; s.knob = KnobStyle::slim; s.groups = { G ("ENSEMBLE", { "Ensemble", "Fullness", "Brighten" }, true) }; }
        else if (name == "Phase Moth")    { s.family = Family::lfo; s.knob = KnobStyle::chicken; s.groups = { G ("PHASER", { "Depth", "Rate", "Feedback" }, true) }; }
        else if (name == "Quiver Tremolo") { s.family = Family::lfo; s.knob = KnobStyle::vintage; s.groups = { G ("TREMOLO", { "Speed", "Depth" }, true) }; }
        else if (name == "Helix Pitch")   { s.family = Family::pitch; s.knob = KnobStyle::ring; s.groups = { G ("PITCH", { "Semitones" }, true), G ("ALGORITHM", { "Algorithm" }) }; }
        else if (name == "Double Wing ADT") { s.family = Family::delay; s.knob = KnobStyle::slim; s.width = 700; s.groups = { G ("VOICE A", { "A Delay", "A Level" }, true), G ("VOICE B", { "B Delay", "B Level" }, true), G ("LEVELS", { "Headrm", "Output" }) }; }
        else if (name == "Wingspan")      { s.family = Family::width; s.knob = KnobStyle::flat; s.groups = { G ("STEREO", { "Width", "Center" }, true) }; }
        else if (name == "Mono Maker")    { s.family = Family::width; s.knob = KnobStyle::flat; s.groups = { G ("MONO BELOW", { "Monotor" }, true) }; }
        else if (name == "Dither Dust")   { s.family = Family::dither; s.knob = KnobStyle::flat; s.height = 330; }
        else if (name == "Trim Gain")     { s.family = Family::trim; s.knob = KnobStyle::vintage; s.groups = { G ("GAIN", { "Gain", "Slow Fade" }, true) }; }
        else if (name == "Moth 808")      { s.family = Family::eight08; s.knob = KnobStyle::ring; s.width = 980; s.height = 600;
                                            s.groups = { G ("PITCH", { "Tune", "Fine", "Glide", "Glide Mode" }), G ("PUNCH", { "Punch", "Punch Time", "Click" }, true),
                                                         G ("TONE", { "Harmonics", "Sub", "Tone Cutoff" }), G ("DRIVE", { "Drive", "Drive Type" }, true),
                                                         G ("AMP", { "Decay", "Release", "Velocity", "Level" }), G ("PLAY", { "Voice Mode", "Bend Range" }) }; }
        else if (name == "Blossom 2")     { s.family = Family::blossom; s.knob = KnobStyle::slim; s.width = 1200; s.height = 700;
                                            s.groups = { G ("EXCITER", { "Exciter", "Position", "Hardness", "Noise", "Bow Pressure", "Bow Speed" }),
                                                         G ("MATERIAL", { "Stiffness", "Damping", "Decay" }, true),
                                                         G ("STRING 2", { "String 2 Ratio", "String 2 Detune", "String 2 Mix", "Coupling" }),
                                                         G ("BODY", { "Body Type", "Body Size", "Body", "Sympathetic", "Pickup" }),
                                                         G ("PLAY", { "Attack", "Release", "Vibrato", "Vibrato Rate", "Velocity", "Tune" }),
                                                         G ("SPACE", { "Stereo Spread", "Chorus", "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Level" }) }; }
        else if (name == "Mothsynth 2")   { s.family = Family::mothsynth2; s.knob = KnobStyle::ring; s.width = 1360; s.height = 920; s.vizH = 250; s.rowSplit = 4; s.row1Share = 0.42f;
                                            s.groups = { G ("OSC 1", { "Osc 1 Wave", "Osc 1 Level", "Osc 1 Semi", "Osc 1 Fine", "Osc 1 Width" }),
                                                         G ("OSC 2", { "Osc 2 Wave", "Osc 2 Level", "Osc 2 Semi", "Osc 2 Fine", "Osc 2 Width", "Osc 2 Sync" }),
                                                         G ("OSC 3 + SUB", { "Osc 3 Wave", "Osc 3 Level", "Osc 3 Semi", "Osc 3 Fine", "Sub Wave", "Sub Level", "Sub Octave", "Noise", "Ring Mod", "FM Amount" }, false, 5),
                                                         G ("FILTERS", { "F1 Type", "Cutoff", "Resonance", "Filter Env", "Key Track", "Routing", "F2 Type", "F2 Cutoff", "F2 Res", "F2 Env" }, false, 5),
                                                         G ("ENVELOPES", { "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release", "Mod Attack", "Mod Decay", "Mod Sustain", "Mod Release" }, false, 4),
                                                         G ("LFO 1 / LFO 2", { "LFO 1 Shape", "LFO 1 Rate", "LFO 1 Sync", "LFO1 > Pitch", "LFO1 > Cut", "LFO 2 Shape", "LFO 2 Rate", "LFO 2 Sync", "LFO2 > Width", "LFO2 > Amp" }, false, 5),
                                                         G ("VOICE", { "Voice Mode", "Glide Mode", "Glide", "Unison", "Uni Detune", "Uni Spread", "Max Voices", "Bend Range", "Velocity", "Quality" }, false, 5),
                                                         G ("DRIVE + FX", { "Drive", "Drive Type", "Pan", "Width", "Chorus", "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Level" }, false, 5) }; }
        else if (name == "Venom")         { s.family = Family::venom; s.knob = KnobStyle::flat; s.width = 1360; s.height = 900; s.vizH = 250; s.rowSplit = 4; s.row1Share = 0.42f;
                                            s.groups = { G ("OSC 1", { "Osc 1 Wave", "Osc 1 Level", "Osc 1 Semi", "Osc 1 Fine", "Osc 1 Width" }),
                                                         G ("OSC 2", { "Osc 2 Wave", "Osc 2 Level", "Osc 2 Semi", "Osc 2 Fine", "Osc 2 Width", "Osc 2 Sync" }),
                                                         G ("SUB / NOISE / MIX", { "Sub Level", "Sub Octave", "Noise", "Ring Mod", "FM Amount" }),
                                                         G ("FILTERS", { "F1 Type", "Cutoff", "Resonance", "Filter Env", "Key Track", "Routing", "F2 Type", "F2 Cutoff", "F2 Res" }, false, 5),
                                                         G ("ENVELOPES", { "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release", "Mod Attack", "Mod Decay", "Mod Sustain", "Mod Release" }, false, 4),
                                                         G ("LFO 1 / LFO 2", { "LFO 1 Shape", "LFO 1 Rate", "LFO 1 Sync", "LFO1 > Pitch", "LFO1 > Cut", "LFO 2 Shape", "LFO 2 Rate", "LFO 2 Sync", "LFO2 > Width", "LFO2 > Amp" }, false, 5),
                                                         G ("VOICE + GATE", { "Voice Mode", "Glide Mode", "Glide", "Unison", "Uni Detune", "Uni Spread", "Max Voices", "Bend Range", "Velocity", "Quality", "Gate On", "Gate Rate", "Gate Depth", "Gate Pattern" }, false, 5),
                                                         G ("DRIVE + FX", { "Drive", "Drive Type", "Pan", "Width", "Chorus", "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Level" }, false, 5) }; }
        else if (name == "Sim Moth")      { s.family = Family::simmoth; s.knob = KnobStyle::vintage; s.width = 1240; s.height = 760; s.vizH = 230; s.rowSplit = 4; s.row1Share = 0.5f;
                                            s.groups = { G ("OSC 1", { "Osc 1 Wave", "Osc 1 Level", "Osc 1 Semi", "Osc 1 Width" }, false, 2),
                                                         G ("OSC 2", { "Osc 2 Wave", "Osc 2 Level", "Osc 2 Semi", "Osc 2 Fine" }, false, 2),
                                                         G ("OSC 3", { "Osc 3 Wave", "Osc 3 Level", "Osc 3 Semi", "Osc 3 Fine" }, false, 2),
                                                         G ("FILTER", { "Filter Type", "Cutoff", "Resonance", "Filter Env" }, false, 2),
                                                         G ("ENVELOPES", { "Attack", "Decay", "Sustain", "Release", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release" }, false, 4),
                                                         G ("LFO + PLAY", { "LFO Rate", "LFO > Pitch", "LFO > Cutoff", "LFO > Width", "Glide", "Chorus", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality", "Level" }, false, 5) }; }
        else if (name == "Mini Moth")     { s.family = Family::minimoth; s.knob = KnobStyle::slim; s.width = 1000; s.height = 600; s.vizH = 200;
                                            s.groups = { G ("SOUND", { "Wave", "Sub", "Cutoff", "Resonance", "Filter Env" }, true),
                                                         G ("ENVELOPE", { "Attack", "Decay", "Sustain", "Release" }),
                                                         G ("PLAY", { "LFO Rate", "Vibrato", "Glide", "Chorus", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality", "Level" }, false, 4) }; }
        else if (name == "Thump")         { s.family = Family::thump; s.knob = KnobStyle::chicken; s.width = 1000; s.height = 640; s.vizH = 230;
                                            s.groups = { G ("PITCH", { "Pitch Sweep", "Pitch (C3)", "Sweep Time", "Sweep Curve" }, true, 2),
                                                         G ("BODY", { "Wave", "Decay", "Body Harm", "Body Ratio", "Body Decay", "Sub Layer" }, false, 3),
                                                         G ("CLICK + DRIVE", { "Click", "Click Tone", "Drive", "Drive Type", "Tone" }, false, 3),
                                                         G ("OUT", { "Velocity", "Level", "Voice Mode", "Glide Mode", "Max Voices", "Quality", "Bend Range", "Unison" }, false, 2) }; }
        else if (name == "Dial 10")       { s.family = Family::dial10; s.knob = KnobStyle::ring; s.width = 1100; s.height = 660; s.vizH = 250;
                                            s.groups = { G ("MODULATOR", { "Mod Ratio", "Mod Level", "Feedback", "Vel > Mod", "Key > Bright" }, true, 3),
                                                         G ("MOD ENV", { "Mod Attack", "Mod Decay", "Mod Sustain", "Mod Release" }, false, 2),
                                                         G ("AMP ENV", { "Attack", "Decay", "Sustain", "Release" }, false, 2),
                                                         G ("PLAY", { "Carrier", "Octave", "Fine", "Vibrato Rate", "Vibrato", "Voice Mode", "Glide Mode", "Bend Range", "Unison", "Max Voices", "Quality", "Level" }, false, 4) }; }
        else if (name == "Hexwing")       { s.family = Family::hexwing; s.knob = KnobStyle::ring; s.width = 1400; s.height = 960; s.vizH = 270; s.rowSplit = 3; s.row1Share = 0.5f;
                                            auto ops = [] (int i) { const juce::String n = juce::String (i); return juce::StringArray { "Op " + n + " Wave", "Op " + n + " Ratio", "Op " + n + " Fine", "Op " + n + " Level", "Op " + n + " Feedback", "Op " + n + " Attack", "Op " + n + " Decay", "Op " + n + " Sustain", "Op " + n + " Release", "Op " + n + " Velocity", "Op " + n + " Key Scale" }; };
                                            juce::StringArray opsA, opsB; for (int i = 1; i <= 3; ++i) opsA.addArray (ops (i)); for (int i = 4; i <= 6; ++i) opsB.addArray (ops (i));
                                            s.groups = { G ("OPERATORS 1 - 3", opsA, false, 11), G ("OPERATORS 4 - 6", opsB, false, 11),
                                                         G ("ALGORITHM + FILTER", { "Algorithm", "Mod Depth", "Filter Type", "Cutoff", "Resonance", "Filter Env", "Key Track", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release" }, false, 6),
                                                         G ("LFO 1 / LFO 2", { "LFO 1 Shape", "LFO 1 Rate", "LFO 1 Sync", "LFO1 > Pitch", "LFO1 > Index", "LFO 2 Shape", "LFO 2 Rate", "LFO 2 Sync", "LFO2 > Amp", "LFO2 > Cut" }, false, 5),
                                                         G ("VOICE", { "Voice Mode", "Glide Mode", "Glide", "Unison", "Uni Detune", "Uni Spread", "Max Voices", "Bend Range", "Quality" }, false, 5),
                                                         G ("DRIVE + FX", { "Drive", "Drive Type", "Pan", "Width", "Chorus", "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Level" }, false, 5) }; }
        else if (name == "Toxin FM")      { s.family = Family::toxin; s.knob = KnobStyle::flat; s.width = 1400; s.height = 960; s.vizH = 400; s.rowSplit = 3; s.row1Share = 0.5f;
                                            auto ops = [] (int i) { const juce::String n = juce::String (i); return juce::StringArray { "Op " + n + " Wave", "Op " + n + " Ratio", "Op " + n + " Fine", "Op " + n + " Level", "Op " + n + " Feedback", "Op " + n + " Attack", "Op " + n + " Decay", "Op " + n + " Sustain", "Op " + n + " Release", "Op " + n + " Velocity", "Op " + n + " Key Scale" }; };
                                            juce::StringArray opsA, opsB; for (int i = 1; i <= 3; ++i) opsA.addArray (ops (i)); for (int i = 4; i <= 6; ++i) opsB.addArray (ops (i));
                                            s.groups = { G ("OPERATORS 1 - 3", opsA, false, 11), G ("OPERATORS 4 - 6", opsB, false, 11),
                                                         G ("ALGORITHM + LADDER", { "Algorithm", "Mod Depth", "Filter Type", "Cutoff", "Resonance", "Filter Env", "Key Track", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release" }, false, 6),
                                                         G ("SEQUENCER", { "Sequencer", "Seq Rate", "Seq Swing", "Seq Gate", "Accent", "Seq Octave" }, false, 3),
                                                         G ("LFO + VOICE", { "LFO Rate", "LFO > Pitch", "LFO > Index", "Glide", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 5),
                                                         G ("DRIVE + FX", { "Drive", "Drive Type", "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Level" }, false, 4) };
                                            s.hiddenPrefixes = { "Step " }; }
        else if (name == "Lantern")       { s.family = Family::lantern; s.knob = KnobStyle::vintage; s.width = 1300; s.height = 820; s.vizH = 260; s.rowSplit = 4; s.row1Share = 0.48f;
                                            s.groups = { G ("OSC 1", { "Osc 1 Wave", "Osc 1 Level", "Osc 1 Octave", "Osc 1 Width" }, false, 2),
                                                         G ("OSC 2", { "Osc 2 Wave", "Osc 2 Level", "Osc 2 Semi", "Osc 2 Fine", "Osc 2 Width", "Osc 2 Sync" }, false, 3),
                                                         G ("CHARACTER", { "Sub", "Noise", "Drift", "Age", "Era", "Vintage Mode" }, false, 3),
                                                         G ("FILTER", { "Filter", "Cutoff", "Resonance", "Filter Env", "Key Track" }, true, 5),
                                                         G ("ENVELOPES", { "Attack", "Decay", "Sustain", "Release", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release" }, false, 4),
                                                         G ("LFO", { "LFO Shape", "LFO Rate", "LFO > Pitch", "LFO > Cutoff", "LFO > Width", "LFO Delay" }, false, 3),
                                                         G ("ENSEMBLE + PLAY", { "Ensemble", "Ens Rate", "Glide", "Uni Detune", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 5),
                                                         G ("FX", { "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Level" }, false, 5) }; }
        else if (name == "Lantern Exo")   { s.family = Family::lanternexo; s.knob = KnobStyle::ring; s.width = 1380; s.height = 920; s.vizH = 270; s.rowSplit = 4; s.row1Share = 0.42f;
                                            s.groups = { G ("OSC 1", { "Osc 1 Wave", "Osc 1 Morph", "Osc 1 Level", "Osc 1 Semi", "Osc 1 Fine", "Osc 1 Width" }, false, 3),
                                                         G ("OSC 2", { "Osc 2 Wave", "Osc 2 Morph", "Osc 2 Level", "Osc 2 Semi", "Osc 2 Fine", "Osc 2 Width", "Osc 2 Sync" }, false, 4),
                                                         G ("SUB / NOISE / MIX", { "Sub Level", "Sub Wave", "Noise", "Ring Mod", "FM Amount", "Drift" }, false, 3),
                                                         G ("FILTERS", { "F1 Type", "Cutoff", "Resonance", "Filter Env", "Key Track", "Routing", "F2 Type", "F2 Cutoff", "F2 Res", "F2 Env" }, false, 5),
                                                         G ("ENVELOPES", { "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release", "Mod Attack", "Mod Decay", "Mod Sustain", "Mod Release" }, false, 4),
                                                         G ("LFO 1 / LFO 2", { "LFO 1 Shape", "LFO 1 Rate", "LFO 1 Sync", "LFO1 > Pitch", "LFO1 > Cut", "LFO 2 Shape", "LFO 2 Rate", "LFO 2 Sync", "LFO2 > Width", "LFO2 > Amp" }, false, 5),
                                                         G ("VOICE", { "Voice Mode", "Glide Mode", "Glide", "Unison", "Uni Detune", "Uni Spread", "Max Voices", "Bend Range", "Velocity", "Quality" }, false, 5),
                                                         G ("DRIVE + FX", { "Drive", "Drive Type", "Pan", "Width", "Chorus", "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Level" }, false, 5) }; }
        else if (name == "Larva Bass")    { s.family = Family::larvabass; s.knob = KnobStyle::chicken; s.width = 1240; s.height = 760; s.vizH = 300; s.rowSplit = 2; s.row1Share = 0.5f;
                                            s.groups = { G ("ACID", { "Wave", "Cutoff", "Resonance", "Env Mod", "Decay", "Accent" }, true, 6),
                                                         G ("PLAY", { "Slide Time", "Sub", "Tone", "Velocity", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 5),
                                                         G ("SEQUENCER", { "Sequencer", "Seq Rate", "Seq Swing", "Seq Gate", "Seq Octave" }, false, 3),
                                                         G ("DRIVE + DELAY", { "Drive", "Drive Type", "Delay", "Delay Time", "Delay FB", "Level" }, false, 3) };
                                            s.hiddenPrefixes = { "Step " }; }
        else if (name == "Thorn Bass")    { s.family = Family::thornbass; s.knob = KnobStyle::vintage; s.width = 1100; s.height = 640; s.vizH = 250;
                                            s.groups = { G ("STRING", { "Pick Position", "Pickup Position", "Neck / Bridge", "Tone", "Decay", "Body" }, true, 6),
                                                         G ("TECHNIQUE", { "Slap", "Palm Mute", "Fret Noise", "Round Robin", "Velocity", "Slide" }, false, 3),
                                                         G ("AMP", { "Amp Drive", "Amp Tone", "Chorus", "Level", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 4) }; }
        else if (name == "Carapace")      { s.family = Family::carapace; s.knob = KnobStyle::ring; s.width = 1240; s.height = 660; s.vizH = 440;
                                            s.groups = { G ("MASTER", { "Level", "Master Tone", "Master Drive", "Velocity" }, true, 4), G ("PLAY", { "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 3) };
                                            for (auto& n : SqSynth::SqCarapacePlugin::soundNames()) s.hiddenPrefixes.add (n + " "); }
        else if (name == "Live Drum Synth") { s.family = Family::livedrum; s.knob = KnobStyle::chicken; s.width = 1240; s.height = 650; s.vizH = 430;
                                            s.groups = { G ("MASTER", { "Level", "Drive" }, true, 2), G ("PLAY", { "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 3) };
                                            for (auto& n : SqSynth::SqLiveDrumSynthPlugin::soundNames()) s.hiddenPrefixes.add (n + " "); }
        else if (name == "Drum Pad Model") { s.family = Family::drumpad; s.knob = KnobStyle::slim; s.width = 1000; s.height = 540; s.vizH = 230;
                                            s.groups = { G ("MODEL", { "Model", "Pitch", "Pitch Env", "Pitch Time", "Decay" }, true, 5),
                                                         G ("CHARACTER", { "Tone", "Noise", "Snap", "Drive" }, false, 4),
                                                         G ("PLAY", { "Key Track", "Velocity", "Level", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 5) }; }
        else if (name == "Thump Live")    { s.family = Family::thumplive; s.knob = KnobStyle::chicken; s.width = 1240; s.height = 720; s.vizH = 330;
                                            s.groups = { G ("KICK", { "Pitch (C3)", "Click", "Click Tone", "Click Decay", "Sub", "Tone", "Drive Type", "Velocity" }, true, 4),
                                                         G ("PATTERN", { "Pattern", "Pattern Rate" }, false, 2),
                                                         G ("OUT", { "Level", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Quality", "Bend Range" }, false, 4) };
                                            s.hiddenPrefixes = { "Pattern Step ", "Pitch T", "Pitch V", "Amp T", "Amp V", "Dist T", "Dist V" }; }
        else if (name == "Time Weaver")   { s.family = Family::timeweaver; s.knob = KnobStyle::flat; s.width = 1200; s.height = 640; s.vizH = 380;
                                            s.groups = { G ("SLOTS", { "Time Slot", "Volume Slot" }, true, 2), G ("MIX", { "Time Mix", "Volume Mix", "Smoothing", "Tape Tone", "Time Ramp", "Tempo Sync", "Level" }, false, 4) };
                                            s.hiddenPrefixes = { "Time Step ", "Volume Step " }; }
        else if (name == "Larva Filter")  { s.family = Family::larvafilter; s.knob = KnobStyle::slim; s.width = 1400; s.height = 900; s.vizH = 290; s.rowSplit = 2; s.row1Share = 0.66f;
                                            auto st = [] (int i) { const juce::String n = juce::String (i); return juce::StringArray { "Stage " + n + " On", "Stage " + n + " Type", "Stage " + n + " Cutoff", "Stage " + n + " Resonance", "Stage " + n + " Mix", "Stage " + n + " Seq Amount", "Stage " + n + " LFO Rate", "Stage " + n + " LFO Amount", "Stage " + n + " Env Amount", "Stage " + n + " Drive" }; };
                                            juce::StringArray a, b; for (int i = 1; i <= 4; ++i) a.addArray (st (i)); for (int i = 5; i <= 8; ++i) b.addArray (st (i));
                                            s.groups = { G ("STAGES 1 - 4", a, false, 10), G ("STAGES 5 - 8", b, false, 10),
                                                         G ("SEQUENCER", { "Sequencer", "Seq Rate", "Seq Swing", "Seq Smooth" }, false, 4), G ("ROUTING + MIX", { "Routing", "Input", "Output", "Mix" }, false, 4) };
                                            s.hiddenPrefixes = { "Step " }; }
        else if (name == "Apex Triad")  { s.family = Family::triad; s.knob = KnobStyle::flat; s.width = 1240; s.height = 760; s.vizH = 330; s.rowSplit = 3; s.row1Share = 0.55f;
                                            s.groups = { G ("LOW", { "Low Threshold", "Low Gain", "Low Attack", "Low Release", "Low Knee", "Low Mix" }, false, 3),
                                                         G ("MID", { "Mid Threshold", "Mid Gain", "Mid Attack", "Mid Release", "Mid Knee", "Mid Mix" }, false, 3),
                                                         G ("HIGH", { "High Threshold", "High Gain", "High Attack", "High Release", "High Knee", "High Mix" }, false, 3),
                                                         G ("CROSSOVERS", { "Low Crossover", "High Crossover" }, true, 2),
                                                         G ("MASTER", { "Input", "Output", "Ceiling", "Lookahead", "Mix" }, true, 5) }; }
        else if (name == "Chrysalis 2" || name == "Chrysalis SF") { s.family = Family::chrysalis2; s.knob = KnobStyle::ring; s.width = 1300; s.height = 760; s.vizH = 420;
                                            s.groups = { G ("AMP", { "Attack", "Decay", "Sustain", "Release" }, false, 4), G ("FILTER", { "Cutoff", "Resonance", "Filter Env", "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release" }, false, 4),
                                                         G ("PLAY", { "Level", "Tune", "Velocity", "Key Track", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 5) };
                                            s.hiddenPrefixes = { "Filter Type" }; }
        else if (name == "Mandible")      { s.family = Family::mandible; s.knob = KnobStyle::chicken; s.width = 1300; s.height = 700; s.vizH = 400;
                                            s.groups = { G ("DECK A", { "Deck A Level", "Deck A Pitch", "Deck A Decay", "Deck A Cutoff", "Deck A Stretch", "Deck A Reverse" }, false, 6),
                                                         G ("DECK B", { "Deck B Level", "Deck B Pitch", "Deck B Decay", "Deck B Cutoff", "Deck B Stretch", "Deck B Reverse" }, false, 6),
                                                         G ("MIX", { "Crossfade A/B", "Level", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 4) }; }
        else if (name == "Mandible Lite") { s.family = Family::mandible; s.knob = KnobStyle::chicken; s.width = 1100; s.height = 620; s.vizH = 380;
                                            s.groups = { G ("DECK", { "Deck A Level", "Deck A Pitch", "Deck A Decay", "Deck A Cutoff", "Deck A Stretch", "Deck A Reverse" }, false, 6), G ("OUT", { "Level", "Voice Mode", "Glide Mode", "Unison", "Max Voices", "Bend Range", "Quality" }, false, 4) };
                                            s.hiddenPrefixes = { "Deck B", "Crossfade" }; }
        else if (name == "Pollen")        { s.family = Family::pollen; s.knob = KnobStyle::ring; s.width = 1300; s.height = 820; s.vizH = 380; s.rowSplit = 3; s.row1Share = 0.45f;
                                            s.groups = { G ("MACROS", { "Macro 1", "Macro 2", "Macro 3", "Macro 4" }, true, 4),
                                                         G ("TONE", { "Cutoff", "Resonance", "Filter Env", "Osc 1 Morph", "Osc 2 Morph", "Drive" }, false, 3),
                                                         G ("ENVELOPE", { "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release" }, false, 2),
                                                         G ("SPACE", { "Chorus", "Delay", "Delay Time", "Delay FB", "Reverb", "Reverb Size", "Width", "Pan" }, false, 4),
                                                         G ("PLAY", { "Voice Mode", "Glide Mode", "Glide", "Unison", "Uni Detune", "Max Voices", "Bend Range", "Velocity", "Quality", "Level" }, false, 5) };
                                            s.hiddenPrefixes = { "Osc 1 Wave", "Osc 1 Level", "Osc 1 Semi", "Osc 1 Fine", "Osc 1 Width", "Osc 2 Wave", "Osc 2 Level", "Osc 2 Semi", "Osc 2 Fine", "Osc 2 Width", "Osc 2 Sync", "Sub", "Noise", "Ring", "FM", "F1", "F2", "Key Track", "Routing", "Flt ", "Mod ", "LFO", "Drift", "Uni Spread", "Drive Type" }; }
        else if (name == "Rack Channel")  { s.family = Family::generic; s.knob = KnobStyle::flat; s.width = 1180; s.height = 620; s.vizH = 40; s.rowSplit = 4; s.row1Share = 0.45f;
                                            s.groups = { G ("LEVELS", { "Mod X", "Mod Y", "Pitch", "Pitch On", "Velocity" }),
                                                         G ("POLYPHONY", { "Max Poly", "Mono", "Legato", "Porta", "Slide Time" }),
                                                         G ("TIME", { "Gate", "Shift" }),
                                                         G ("CUT", { "Cut Group", "Cut By", "Cut Self" }),
                                                         G ("TRACKING", { "Vel > Pan", "Vel > Cut", "Vel > Mod X", "Vel > Mod Y", "Key > Pan", "Key > Cut", "Key > Mod X", "Key > Mod Y", "Key Mid" }, false, 5),
                                                         G ("ARPEGGIATOR", { "Arp Mode", "Arp Time", "Arp Gate", "Arp Range", "Arp Repeat", "Arp Chord" }, false, 3),
                                                         G ("ECHO", { "Echoes", "Echo Time", "Echo Feed", "Echo Pan", "Ping Pong", "Echo Pitch", "Echo Cut" }, false, 4),
                                                         G ("LAYER", { "Layer Mode" }) }; }
        else if (name == "Hive Pads")     { s.family = Family::hivepads; s.knob = KnobStyle::chicken; s.width = 1240; s.height = 820; s.vizH = 560;
                                            s.groups = { G ("MASTER", { "Level", "Tune", "Velocity" }, true), G ("ALL PADS", { "Attack", "Decay", "Cutoff", "Resonance" }) }; }
        else if (name == "Moth Tune")     { s.family = Family::tune; s.knob = KnobStyle::ring; s.width = 1400; s.height = 960; s.vizH = 340; s.rowSplit = 5;
                                            s.groups = { G ("CORRECTION", { "Retune Speed", "Humanize", "Flex Tune", "Correction" }, true),
                                                         G ("VIBRATO", { "Vibrato Keep", "Natural Vibrato" }, true),
                                                         G ("ENGINE", { "Engine", "Character", "Voice Type", "Auto Range", "Tracking" }),
                                                         G ("KEY & SCALE", { "Key", "Scale", "Auto-Key", "Learn Scale", "Detect Key" }),
                                                         G ("PITCH & FORMANT", { "Transpose", "Conform", "Detune", "Formant", "Formant Link", "Throat", "Formant Preserve" }),
                                                         G ("DYNAMIC VIBRATO", { "Dynamic Vibrato", "Vibrato Threshold", "Vibrato Detect Delay", "Vibrato Attack", "Vibrato Irregularity", "Pitch Depth", "Amplitude Depth", "Timbre Depth" }, false, 4),
                                                         G ("CREATE VIBRATO", { "Vibrato Shape", "Vibrato Rate", "Vibrato Depth", "Vibrato Delay", "Vibrato Onset", "Vibrato Variation", "Straight Notes Only" }, false, 4),
                                                         G ("HARMONY VOICES", { "Voice 1 Mode", "Voice 1 Interval", "Voice 1 Level", "Voice 1 Pan", "Voice 1 Formant",
                                                                                "Voice 2 Mode", "Voice 2 Interval", "Voice 2 Level", "Voice 2 Pan", "Voice 2 Formant",
                                                                                "Voice 3 Mode", "Voice 3 Interval", "Voice 3 Level", "Voice 3 Pan", "Voice 3 Formant",
                                                                                "Voice 4 Mode", "Voice 4 Interval", "Voice 4 Level", "Voice 4 Pan", "Voice 4 Formant" }, false, 5),
                                                         G ("HARMONY FEEL", { "Harmony Detune", "Harmony Timing", "Harmony Trigger", "Harmony CC", "Harmony Latch" }, false, 2),
                                                         G ("MIDI & GRAPH", { "MIDI Targets", "MIDI Track", "Lead Follows MIDI", "All Octaves", "Graph Notes", "Graph Mode" }, false, 2),
                                                         G ("OUTPUT", { "Mix", "Output" }, true, 1) }; }
        else if (name == "Moth Tune FX")  { s.family = Family::tunefx; s.knob = KnobStyle::ring; s.width = 1240; s.height = 870; s.vizH = 480;
                                            s.groups = { G ("TUNE", { "Retune Speed", "Humanize", "Transpose", "Formant", "Throat", "Formant Preserve" }, false, 3),
                                                         G ("KEY & ENGINE", { "Key", "Scale", "Auto-Key", "Engine", "Voice Type" }, false, 2),
                                                         G ("MOTION", { "Motion Mode", "Pattern", "Sync", "Steps", "Motion Rate", "Motion Glide", "Scale Steps", "Latch", "Hold" }, false, 3),
                                                         G ("OUTPUT", { "Mix", "Output" }, true, 1) };
                                            s.hiddenPrefixes = { "Slot " }; }
        else if (name == "Apex Dynamics") { s.family = Family::apexdyn; s.knob = KnobStyle::flat; s.width = 1100; s.height = 660; s.vizH = 240;
                                            s.groups = { G ("MODE", { "Mode", "Detection", "Stereo Link" }, false, 1),
                                                         G ("DYNAMICS", { "Threshold", "Ratio", "Knee", "Depth" }, true, 2),
                                                         G ("TIMING", { "Attack", "Release", "Hold", "Hysteresis", "Lookahead" }, false, 3),
                                                         G ("KEY (SIDECHAIN)", { "Key Input", "External Key", "Key Filter", "Key Filter Freq", "Key Filter Q", "Key Listen" }, false, 3),
                                                         G ("OUTPUT", { "Makeup", "Auto Makeup", "Dry Mix", "Attenuation Listen" }, false, 2) }; }
        else if (name == "Apex Limiter")  { s.family = Family::apexlim; s.knob = KnobStyle::flat; s.width = 1040; s.height = 640; s.vizH = 320;
                                            s.groups = { G ("LIMITER", { "Input", "Ceiling", "Character" }, true, 3),
                                                         G ("RELEASE", { "Release", "Auto Release", "Link" }, false, 2),
                                                         G ("LOUDNESS TARGET", { "Target", "Match Target", "Reset Meter" }, false, 1) }; }
        else if (name == "Apex Multiband") { s.family = Family::apexmb; s.knob = KnobStyle::flat; s.width = 1340; s.height = 960; s.vizH = 280; s.rowSplit = 4;
                                            s.groups.push_back (G ("GLOBAL", { "Bands", "Phase", "Detection", "Input", "Output", "Mix" }, false, 2));
                                            for (int b = 1; b <= 6; ++b)
                                            {
                                                const auto n = juce::String (b);
                                                s.groups.push_back (Group { "BAND " + n, { "Band " + n + " Threshold", "Band " + n + " Depth", "Band " + n + " Gain", "Band " + n + " Slope", "Band " + n + " Attack",
                                                                                          "Band " + n + " Release", "Band " + n + " Solo", "Band " + n + " Bypass", "Band " + n + " Flip", "Band " + n + " Auto Gain" }, false, 5 });
                                            }
                                            s.hiddenPrefixes = { "Crossover " }; s.row1Share = 0.5f; }
        else if (name == "Apex Sub")      { s.family = Family::apexsub; s.knob = KnobStyle::chicken; s.width = 940; s.height = 580; s.vizH = 230;
                                            s.groups = { G ("SOURCE", { "Range", "MIDI Tune" }, false, 1), G ("BANDS", { "Lower", "Upper", "Direct" }, true, 3),
                                                         G ("FILTERS", { "High-pass", "High-pass Q", "Low-pass", "Low-pass Q" }, false, 2), G ("DRIVE", { "Drive Type", "Drive" }, false, 1), G ("MIX", { "Mix" }, true, 1) }; }
        else if (name == "Aurora Verb")   { s.family = Family::aurora; s.knob = KnobStyle::ring; s.width = 1160; s.height = 660; s.vizH = 250;
                                            s.groups = { G ("ROOM", { "Room", "Size", "Pre-Delay", "Diffusion" }, false, 2), G ("DECAY", { "Decay", "Low Decay", "High Decay", "Freeze" }, true, 2),
                                                         G ("LEVELS", { "Early Level", "Tail Level", "Mix" }, false, 1), G ("MOTION", { "Modulation", "Mod Rate", "Width" }, false, 1),
                                                         G ("TONE", { "Low Cut", "High Cut", "Ducking" }, false, 1) }; }
        else if (name == "Opto Leveler")  { s.family = Family::opto; s.knob = KnobStyle::vintage; s.width = 940; s.height = 620; s.vizH = 270;
                                            s.groups = { G ("LEVELER", { "Peak Reduction", "Gain" }, true, 2), G ("MODE", { "Mode", "Meter" }, false, 1), G ("COLOUR", { "Emphasis", "Tube", "Mix" }, false, 3) }; }
        else if (name == "Pulse EQ")      { s.family = Family::pulse; s.knob = KnobStyle::vintage; s.width = 1180; s.height = 760; s.vizH = 230; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("LOW FREQUENCY", { "Low Freq", "Low Boost", "Low Atten" }, true, 3), G ("HIGH BOOST", { "High Freq", "High Boost", "Bandwidth" }, false, 3),
                                                         G ("HIGH ATTEN", { "Atten Sel", "High Atten" }, false, 2),
                                                         G ("MID SECTION", { "Mid Low Freq", "Mid Low Peak", "Dip Freq", "Mid Dip", "Mid High Freq", "Mid High Peak" }, false, 6), G ("OUTPUT", { "Output", "Tube" }, false, 2) }; }
        else if (name == "Oil Can Delay") { s.family = Family::oilcan; s.knob = KnobStyle::vintage; s.width = 980; s.height = 600; s.vizH = 230;
                                            s.groups = { G ("ECHO", { "Mode", "Sync", "Delay" }, false, 1), G ("CAN", { "Variation", "Wobble Rate", "Sustain" }, true, 3), G ("TONE", { "Tone", "Drive" }, false, 1), G ("MIX", { "Mix" }, true, 1) }; }
        else if (name == "Cinder")        { s.family = Family::cinder; s.knob = KnobStyle::chicken; s.width = 1180; s.height = 760; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("SATURATION", { "Model", "Drive", "Punish" }, true, 3), G ("CHARACTER", { "Bias", "Dynamics", "Model B", "Model Blend" }, false, 2),
                                                         G ("OUTPUT", { "Output", "Auto Gain", "Mix" }, true, 3),
                                                         G ("TONE", { "Low Cut", "Thump", "Tone", "High Cut", "Steep" }, false, 5), G ("STEREO / KEY", { "Mid/Side", "Duck Drive", "Key Input" }, false, 3),
                                                         G ("QUALITY", { "Oversampling", "Linear Phase", "Delta" }, false, 3) }; }
        else if (name == "Wingbeat Echo") { s.family = Family::wingbeat; s.knob = KnobStyle::chicken; s.width = 1280; s.height = 800; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("ECHO", { "Mode", "Style", "Sync L", "Time", "Feedback", "Mix" }, true, 3), G ("CHARACTER", { "Saturation", "Wow", "Flutter", "Diffusion" }, false, 2),
                                                         G ("OUTPUT", { "Duck", "Duck Release", "Output" }, false, 3),
                                                         G ("TWEAK", { "Sync R", "Time R", "Groove", "Feel", "Width", "L/R Offset", "Accent", "Warp" }, false, 4), G ("TONE", { "Low Cut", "High Cut", "Input" }, false, 3),
                                                         G ("MACHINE", { "Time Change", "Heads", "Limiter", "Pattern", "Freeze", "Prime", "Key Input" }, false, 4) }; }
        else if (name == "Wingbeat Jr.")  { s.family = Family::wingbeat; s.knob = KnobStyle::chicken; s.width = 960; s.height = 600; s.vizH = 210;
                                            s.groups = { G ("ECHO", { "Style", "Mode", "Sync", "Time" }, true, 2), G ("SOUND", { "Feedback", "Saturation", "Tone" }, false, 3), G ("MIX", { "Change", "Mix", "Output" }, true, 3) }; }
        else if (name == "Foil Plate")    { s.family = Family::foilplate; s.knob = KnobStyle::vintage; s.width = 1240; s.height = 780; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("PLATE", { "Plate", "Decay", "Pre-Delay" }, true, 3), G ("INPUT", { "Input", "Input Drive", "Low Cut", "High Cut" }, false, 2),
                                                         G ("MIX", { "Mix", "Output" }, true, 2),
                                                         G ("TWEAK", { "Mod", "Mod Rate", "Width", "Balance", "Low EQ", "High EQ" }, false, 3), G ("AUTO DECAY", { "Auto Decay", "Auto Threshold", "Key Input" }, false, 3),
                                                         G ("MODES", { "Low Slope", "High Slope", "Freeze", "Darkening", "Mono In" }, false, 3) }; }
        else if (name == "Foil Plate Lite") { s.family = Family::foilplate; s.knob = KnobStyle::vintage; s.width = 860; s.height = 560; s.vizH = 220;
                                            s.groups = { G ("PLATE", { "Decay", "Low Cut", "Mod" }, true, 3), G ("MIX", { "Mix", "Output" }, true, 2) }; }
        else if (name == "Radiant")       { s.family = Family::radiant; s.knob = KnobStyle::vintage; s.width = 1100; s.height = 680; s.vizH = 240;
                                            s.groups = { G ("TUBES", { "Circuit", "Input", "Output" }, true, 3), G ("TONE", { "Bass", "Treble" }, true, 2), G ("MIX", { "Mix", "Trim" }, false, 1),
                                                         G ("SWITCHES", { "Mic", "Noisy", "Oversampling", "Mid/Side" }, false, 2) }; }
        else if (name == "Radiant Pre")   { s.family = Family::radiant; s.knob = KnobStyle::vintage; s.width = 860; s.height = 560; s.vizH = 220;
                                            s.groups = { G ("PREAMP", { "Gain", "Bias", "Noisy" }, true, 3), G ("OUT", { "Output", "Mix" }, true, 2) }; }
        else if (name == "Maw")           { s.family = Family::maw; s.knob = KnobStyle::chicken; s.width = 1060; s.height = 660; s.vizH = 240;
                                            s.groups = { G ("CRUSH", { "Crush", "Crunch", "Darkness" }, true, 3), G ("TIME", { "Release", "Stretch", "Fast" }, false, 3),
                                                         G ("OUT", { "Mix", "Output", "Key Input", "Key HPF" }, false, 2) }; }
        else if (name == "Maw Lite")      { s.family = Family::maw; s.knob = KnobStyle::chicken; s.width = 760; s.height = 520; s.vizH = 200;
                                            s.groups = { G ("CRUSH", { "Crush", "Crunch", "Output" }, true, 3) }; }
        else if (name == "Moth Key")      { s.family = Family::mothkey; s.knob = KnobStyle::ring; s.width = 1080; s.height = 560; s.vizH = 300;
                                            s.groups = { G ("LISTEN", { "Listen", "Listen Length", "Clear" }, true, 3), G ("ANALYSE", { "Analyse", "Auto Apply", "Set Tempo", "Apply to Session" }, false, 4) }; }
        else if (name == "Siren Comp")    { s.family = Family::sirencomp; s.knob = KnobStyle::vintage; s.width = 1320; s.height = 820; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("STAGE 1", { "Model 1", "FET 1", "Threshold 1", "Attack 1", "Release 1", "Ratio 1", "Knee" }, true, 4),
                                                         G ("STAGE 2", { "Dual", "Model 2", "FET 2", "Threshold 2", "Attack 2", "Release 2", "Ratio 2" }, false, 4),
                                                         G ("OUTPUT", { "Warm", "Mix", "Make-Up", "Auto Make-Up", "Output" }, false, 3),
                                                         G ("PUNCH", { "Punch", "Punch Gain", "Impact", "Ceiling" }, false, 4),
                                                         G ("PITCH DUCK", { "Key Input", "Duck Depth" }, false, 2),
                                                         G ("ASSIST", { "Learn", "Amount", "Preview", "Apply" }, false, 4) }; }
        else if (name == "Siren EQ")      { s.family = Family::sireneq; s.knob = KnobStyle::ring; s.width = 1560; s.height = 900; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.45f;
                                            s.groups = { G ("BAND 1", { "B1 On", "B1 Type", "B1 Mode", "B1 Harmonic", "Band 1 Freq", "Band 1 Gain", "Band 1 Q", "Band 1 Threshold", "Band 1 Range", "B1 Expand", "B1 Solo", "B1 Mute" }, false, 6),
                                                         G ("BAND 2", { "B2 On", "B2 Type", "B2 Mode", "B2 Harmonic", "Band 2 Freq", "Band 2 Gain", "Band 2 Q", "Band 2 Threshold", "Band 2 Range", "B2 Expand", "B2 Solo", "B2 Mute" }, false, 6),
                                                         G ("BAND 3", { "B3 On", "B3 Type", "B3 Mode", "B3 Harmonic", "Band 3 Freq", "Band 3 Gain", "Band 3 Q", "Band 3 Threshold", "Band 3 Range", "B3 Expand", "B3 Solo", "B3 Mute" }, false, 6),
                                                         G ("BAND 4", { "B4 On", "B4 Type", "B4 Mode", "B4 Harmonic", "Band 4 Freq", "Band 4 Gain", "Band 4 Q", "Band 4 Threshold", "Band 4 Range", "B4 Expand", "B4 Solo", "B4 Mute" }, false, 6),
                                                         G ("BAND 5", { "B5 On", "B5 Type", "B5 Mode", "B5 Harmonic", "Band 5 Freq", "Band 5 Gain", "Band 5 Q", "Band 5 Threshold", "Band 5 Range", "B5 Expand", "B5 Solo", "B5 Mute" }, false, 6),
                                                         G ("BAND 6", { "B6 On", "B6 Type", "B6 Mode", "B6 Harmonic", "Band 6 Freq", "Band 6 Gain", "Band 6 Q", "Band 6 Threshold", "Band 6 Range", "B6 Expand", "B6 Solo", "B6 Mute" }, false, 6),
                                                         G ("FILTERS", { "Low Cut", "Low Slope", "High Cut", "High Slope" }, false, 2), G ("AIR & TILT", { "Air Freq", "Air", "Tilt", "Output" }, false, 2),
                                                         G ("VOCAL LEARNING", { "Learn", "Preview", "Apply" }, false, 3) }; }
        else if (name == "Hush De-Ess 2") { s.family = Family::hushdeess2; s.knob = KnobStyle::slim; s.width = 1080; s.height = 640; s.vizH = 250;
                                            s.groups = { G ("DETECT", { "Split", "Sensitivity" }, true, 2), G ("HARD  T K CH", { "Hard Threshold", "Hard Range" }, true, 2),
                                                         G ("SOFT  S SH Z", { "Soft Threshold", "Soft Range" }, true, 2),
                                                         G ("LISTEN", { "Link", "Solo", "Output" }, false, 3), G ("ASSIST", { "Learn", "Preview", "Apply" }, false, 3) }; }
        else if (name == "Vellum EQ")     { s.family = Family::vellum; s.knob = KnobStyle::vintage; s.width = 1240; s.height = 760; s.vizH = 260; s.rowSplit = 3; s.row1Share = 0.55f;
                                            s.groups = { G ("LOW", { "Low Freq", "Low" }, true, 2), G ("MID", { "Mid Freq", "Mid" }, true, 2), G ("HIGH", { "High Freq", "High" }, true, 2),
                                                         G ("MID 2", { "Mid 2 On", "Mid 2 Freq", "Mid 2" }, false, 3), G ("AMP", { "Drive", "Oversampling", "Output" }, false, 3),
                                                         G ("MODE", { "Stepped", "Channels" }, false, 2) }; }
        else if (name == "Tremor")        { s.family = Family::tremor; s.knob = KnobStyle::chicken; s.width = 1320; s.height = 820; s.vizH = 240; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("TREMOLO", { "Mode", "Shape", "Sync", "Division", "Rate", "Depth" }, true, 3), G ("FEEL", { "Feel", "Groove", "Accent", "Smoothing", "Pulse Width" }, false, 3),
                                                         G ("OUTPUT", { "Stereo Phase", "Mix", "Output" }, false, 3),
                                                         G ("RHYTHM", { "Pattern", "Step Length", "Steps", "Retrigger" }, false, 2), G ("DYNAMICS", { "Dynamics", "Threshold", "Attack", "Release", "Key Input" }, false, 3),
                                                         G ("COLOUR", { "Rate Mod", "Rate Mod Hz", "Harmonic", "Crossover", "Analog", "Drive" }, false, 3) }; }
        else if (name == "Orbit Pan")     { s.family = Family::orbitpan; s.knob = KnobStyle::ring; s.width = 1320; s.height = 820; s.vizH = 260; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("MOTION", { "Mode", "Shape", "Sync", "Division", "Rate" }, true, 3), G ("IMAGE", { "Offset", "Width", "Smoothing", "Source Width" }, true, 2),
                                                         G ("OUTPUT", { "Pan Law", "Mix", "Output" }, false, 3),
                                                         G ("RHYTHM", { "Pattern", "Step Length", "Steps" }, false, 3), G ("TRIGGER", { "Audio Trigger", "Divider", "Threshold", "Detector HP", "Detector LP", "Key Input" }, false, 3),
                                                         G ("SPACE", { "Depth", "Haas", "Binaural" }, false, 3) }; }
        else if (name == "Mirror Shift")  { s.family = Family::mirrorshift; s.knob = KnobStyle::ring; s.width = 1080; s.height = 680; s.vizH = 250;
                                            s.groups = { G ("SHIFT", { "Style", "Detune", "Delay" }, true, 3), G ("FOCUS", { "Focus", "Mix", "Output" }, true, 3),
                                                         G ("DUCK", { "Duck", "Duck Threshold", "Key Input" }, false, 3) }; }
        else if (name == "Mirror Shift Lite") { s.family = Family::mirrorshift; s.knob = KnobStyle::ring; s.width = 820; s.height = 540; s.vizH = 220;
                                            s.groups = { G ("WIDTH", { "Style", "Amount" }, true, 2), G ("OUT", { "Mix", "Output" }, true, 2) }; }
        else if (name == "Shutter Delay") { s.family = Family::shutter; s.knob = KnobStyle::flat; s.width = 1280; s.height = 800; s.vizH = 240; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("LINE A", { "Sync A", "Time A", "Feedback A" }, true, 3), G ("LINE B", { "Sync B", "Time B", "Feedback B" }, true, 3),
                                                         G ("MIX", { "Mix", "Output" }, true, 2),
                                                         G ("CONVERTER", { "Multiply", "Converter", "Input" }, false, 3), G ("ROUTING", { "Algorithm", "Freeze", "Time Change" }, false, 3),
                                                         G ("LFO", { "LFO Rate", "LFO Depth" }, false, 2) }; }
        else if (name == "Shutter Lite")  { s.family = Family::shutter; s.knob = KnobStyle::flat; s.width = 900; s.height = 580; s.vizH = 220;
                                            s.groups = { G ("DELAY", { "Time", "Multiply", "Feedback" }, true, 3), G ("MIX", { "Mix", "Freeze", "Output" }, true, 3) }; }
        else if (name == "Shard")         { s.family = Family::shard; s.knob = KnobStyle::ring; s.width = 1440; s.height = 900; s.vizH = 240; s.rowSplit = 3; s.row1Share = 0.45f;
                                            s.groups = { G ("SHARD", { "Engine", "Splice Sync", "Splice", "Delay Sync", "Delay", "Pitch" }, true, 3), G ("ECHO", { "Recycle", "Recycle Mode", "Reverse", "Mix", "Output" }, true, 3),
                                                         G ("KEY", { "Quantize", "Session Key", "Key", "Scale" }, false, 2),
                                                         G ("GRAINS", { "Smoothing", "Spray", "Density", "Size Jitter" }, false, 2), G ("TWEAK", { "Pitch L", "Pitch R", "Splice R", "Delay R", "Low Cut", "High Cut" }, false, 3),
                                                         G ("GATE / DUCK", { "Gate/Duck", "Gate/Duck Threshold", "Gate/Duck Release", "Key Input" }, false, 2),
                                                         G ("MOD MATRIX", { "LFO 1 Target", "LFO 1 Shape", "LFO 1 Sync", "LFO 1 Rate", "LFO 1 Depth", "LFO 2 Target", "LFO 2 Shape", "LFO 2 Sync", "LFO 2 Rate", "LFO 2 Depth",
                                                                            "Env Target", "Env Attack", "Env Release", "Env Depth" }, false, 5) }; }
        else if (name == "Murmuration")   { s.family = Family::murmuration; s.knob = KnobStyle::vintage; s.width = 1400; s.height = 860; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("CLOUD", { "Sync", "Time", "Beats", "Shape X", "Shape Y", "Texture" }, true, 3), G ("CHARACTER", { "Warp", "Color", "Blur", "Grain" }, true, 2),
                                                         G ("MIX", { "Mix", "Output" }, true, 1),
                                                         G ("MOD", { "Mod Shape", "Mod Rate", "Mod Depth" }, false, 3), G ("SHIMMER", { "Shimmer Pitch", "Shimmer", "Shimmer Fade" }, false, 3),
                                                         G ("HOLD", { "Freeze", "Freeze Mode", "Infinite", "Drawn Envelope" }, false, 2),
                                                         G ("TWEAK", { "Tail Tilt", "Low Cut", "High Cut", "Width", "Ducking", "Pre-Delay" }, false, 3) }; }
        else if (name == "Masquerade")    { s.family = Family::masquerade; s.knob = KnobStyle::chicken; s.width = 1240; s.height = 760; s.vizH = 240; s.rowSplit = 3; s.row1Share = 0.55f;
                                            s.groups = { G ("VOICE", { "Mode", "Pitch", "Formant", "Link" }, true, 2), G ("DRIVE", { "Drive Type", "Drive" }, true, 2), G ("OUT", { "Mix", "Output" }, true, 1),
                                                         G ("KEY", { "Session Key", "Key", "Scale", "Retune" }, false, 2), G ("ROBOT / MIDI", { "Robot Note", "Chord Level", "Chord Spread" }, false, 3),
                                                         G ("ENGINE", { "Engine", "Voice Range" }, false, 2) }; }
        else if (name == "Larva Filter 2") { s.family = Family::larva2; s.knob = KnobStyle::chicken; s.width = 1440; s.height = 900; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("FILTER", { "Cutoff", "Resonance", "Stages", "Routing", "L/R Offset", "Input" }, true, 3),
                                                         G ("WING MOD", { "Mod Mode", "Mod Shape", "Mod Sync", "Mod Rate", "Freq Mod", "Res Mod" }, true, 3),
                                                         G ("OUT", { "Analog", "Drive", "Mix", "Output" }, true, 2),
                                                         G ("STAGES", { "Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Stage 7", "Stage 8",
                                                                        "Stage 1 Offset", "Stage 2 Offset", "Stage 3 Offset", "Stage 4 Offset", "Stage 5 Offset", "Stage 6 Offset", "Stage 7 Offset", "Stage 8 Offset" }, false, 8),
                                                         G ("RHYTHM", { "Pattern", "Step Length", "Steps", "Groove", "Feel", "Smoothing" }, false, 3),
                                                         G ("ENVELOPE / ADSR", { "Trigger", "Fire", "Env Attack", "Env Release", "Env Sensitivity", "Key Input", "Attack", "Decay", "Sustain", "Release", "Analog Post" }, false, 4) }; }
        else if (name == "Twelve Phase 2") { s.family = Family::phase2; s.knob = KnobStyle::vintage; s.width = 1400; s.height = 860; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f;
                                            s.groups = { G ("PHASER", { "Style", "Stages", "Centre", "Sweep", "Resonance", "Intensity" }, true, 3),
                                                         G ("WING MOD", { "Mod Mode", "Mod Shape", "Mod Sync", "Mod Rate", "Stereo Phase" }, true, 3),
                                                         G ("OUT", { "Mix", "Output" }, true, 1),
                                                         G ("COLOUR", { "Classic Spread", "Resonance Path", "Polarity", "Through-Zero", "Analog", "Drive", "Analog Post" }, false, 4),
                                                         G ("RHYTHM", { "Pattern", "Step Length", "Steps", "Groove", "Feel", "Smoothing" }, false, 3),
                                                         G ("ENVELOPE / ADSR", { "Trigger", "Env Attack", "Env Release", "Env Sensitivity", "Key Input", "Attack", "Decay", "Sustain", "Release" }, false, 5) }; }
        else if (name == "Hive Rack")     { s.family = Family::hiverack; s.knob = KnobStyle::ring; s.width = 1400; s.height = 880; s.vizH = 300; s.rowSplit = 3; s.row1Share = 0.55f; s.resizable = true;
                                            s.groups = { G ("SLOTS", { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6", "Slot 7", "Slot 8" }, true, 4),
                                                         G ("RACK", { "Chain", "Routing", "Open Slot", "Lane Balance", "Input", "Output", "Mix" }, true, 3),
                                                         G ("RECYCLE", { "Recycle", "Recycle Time", "Recycle Sync", "Recycle Division" }, true, 2),
                                                         G ("MACROS", { "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Macro 7", "Macro 8" }, false, 8) }; }
        else if (name == "Siren Verb")    { s.family = Family::sirenverb; s.knob = KnobStyle::vintage; s.width = 1440; s.height = 900; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.5f; s.resizable = true;
                                            s.groups = { G ("REVERB", { "Algorithm", "Assist", "Dice", "Size", "Decay", "Pre-Delay" }, true, 3),
                                                         G ("SPACE", { "Damping", "Early", "Width", "Low Cut", "High Cut", "Mod Depth" }, true, 3),
                                                         G ("MIX", { "Mix", "Mix Lock", "Output" }, true, 1),
                                                         G ("PRE-VERB", { "Pre Pitch", "Throat", "Tube", "De-Ess", "Reverse", "Reverse Time" }, false, 3),
                                                         G ("POST-VERB", { "Auto-EQ", "Auto-EQ Depth", "Tone", "Compressor", "Gate", "Ducking", "Shimmer", "Shimmer Pitch", "Mod Rate" }, false, 3),
                                                         G ("ECHO", { "Echo", "Echo Sync", "Echo Time", "Echo Feedback", "Echo Level" }, false, 3) }; }
        else if (name == "Mic Closet")    { s.family = Family::miccloset; s.knob = KnobStyle::chicken; s.width = 1320; s.height = 820; s.vizH = 250; s.rowSplit = 3; s.row1Share = 0.55f; s.resizable = true;
                                            s.groups = { G ("MICROPHONES", { "Source Mic", "Target Mic", "Pattern", "Source Proximity" }, true, 2),
                                                         G ("PLACEMENT", { "Distance", "Low Cut", "Pad" }, true, 3),
                                                         G ("OUT", { "Mix", "Output" }, true, 1),
                                                         G ("CHARACTER", { "Colour", "Self-Noise" }, false, 2) }; }
        else if (name == "Larynx")        { s.family = Family::larynx; s.knob = KnobStyle::ring; s.width = 1400; s.height = 880; s.vizH = 280; s.rowSplit = 3; s.row1Share = 0.5f; s.resizable = true;
                                            s.groups = { G ("TRACT", { "Tract Length", "Precision", "Level Match" }, true, 3),
                                                         G ("GLOTTIS", { "Glottal Hardness", "Pulse Width", "Breathiness", "Breath Freq" }, true, 2),
                                                         G ("OUT", { "Mix", "Output" }, true, 1),
                                                         G ("SHAPE", { "Glottis", "Lower Throat", "Upper Throat", "Pharynx", "Back Tongue", "Mid Tongue", "Front Tongue", "Lips" }, false, 8) }; }
        else if (name == "Exhale")        { s.family = Family::exhale; s.knob = KnobStyle::vintage; s.width = 1100; s.height = 680; s.vizH = 240; s.resizable = true;
                                            s.groups = { G ("BREATH", { "Breath", "Breath Tone", "Consonants" }, true, 3), G ("TONE", { "Body" }, true, 1), G ("OUT", { "Mix", "Output" }, true, 2) }; }
        else if (name == "Hive Choir")    { s.family = Family::hivechoir; s.knob = KnobStyle::vintage; s.width = 1560; s.height = 1060; s.vizH = 240; s.rowSplit = 3; s.row1Share = 0.33f; s.resizable = true;
                                            s.groups = { G ("KEY + ENGINE", { "Engine", "Voice Range", "Session Key", "Key", "Scale", "Lead Level" }, true, 3),
                                                         G ("HUMANIZE", { "Humanize Pitch", "Humanize Time", "Consonants" }, true, 3),
                                                         G ("OUT", { "Width", "Mix", "Output" }, true, 3),
                                                         G ("VOICE 1", { "Voice 1 Mode", "Voice 1 Interval", "Voice 1 Level", "Voice 1 Pan", "Voice 1 Throat", "Voice 1 Singers", "Voice 1 Pitch Variation", "Voice 1 Time Variation", "Voice 1 Timbre Variation", "Voice 1 Spread", "Voice 1 Vibrato Rate", "Voice 1 Vibrato Depth" }, false, 3), G ("VOICE 2", { "Voice 2 Mode", "Voice 2 Interval", "Voice 2 Level", "Voice 2 Pan", "Voice 2 Throat", "Voice 2 Singers", "Voice 2 Pitch Variation", "Voice 2 Time Variation", "Voice 2 Timbre Variation", "Voice 2 Spread", "Voice 2 Vibrato Rate", "Voice 2 Vibrato Depth" }, false, 3), G ("VOICE 3", { "Voice 3 Mode", "Voice 3 Interval", "Voice 3 Level", "Voice 3 Pan", "Voice 3 Throat", "Voice 3 Singers", "Voice 3 Pitch Variation", "Voice 3 Time Variation", "Voice 3 Timbre Variation", "Voice 3 Spread", "Voice 3 Vibrato Rate", "Voice 3 Vibrato Depth" }, false, 3), G ("VOICE 4", { "Voice 4 Mode", "Voice 4 Interval", "Voice 4 Level", "Voice 4 Pan", "Voice 4 Throat", "Voice 4 Singers", "Voice 4 Pitch Variation", "Voice 4 Time Variation", "Voice 4 Timbre Variation", "Voice 4 Spread", "Voice 4 Vibrato Rate", "Voice 4 Vibrato Depth" }, false, 3) }; }
        else if (name == "Double Wing 2") { s.family = Family::doublewing2; s.knob = KnobStyle::ring; s.width = 1280; s.height = 800; s.vizH = 260; s.rowSplit = 3; s.row1Share = 0.5f; s.resizable = true;
                                            s.groups = { G ("DOUBLES", { "Doubles", "Voice Range", "Doubles Level", "Lead Level" }, true, 2),
                                                         G ("VARIATION", { "Pitch Variation", "Timing Variation", "Timbre Variation", "Spread" }, true, 2),
                                                         G ("OUT", { "Width", "Output" }, true, 1),
                                                         G ("VIBRATO", { "Vibrato Rate", "Vibrato Depth", "Straight Notes Only", "New Voices" }, false, 4) }; }
        else if (name == "Mutant")        { s.family = Family::mutant; s.knob = KnobStyle::chicken; s.width = 1360; s.height = 860; s.vizH = 270; s.rowSplit = 3; s.row1Share = 0.5f; s.resizable = true;
                                            s.groups = { G ("MUTATE", { "Pitch", "Throat", "Engine", "Voice Range" }, true, 2),
                                                         G ("RING", { "Ring Mod", "Ring Ratio", "Ring Unvoiced" }, true, 3),
                                                         G ("OUT", { "Drive Type", "Drive", "Mix", "Output" }, true, 2),
                                                         G ("ALIENIZATION", { "Alienization", "Alien Rate", "Alien Glide", "New Mutation" }, false, 4),
                                                         G ("STUTTER", { "Stutter", "Stutter Length", "Repeats" }, false, 3) }; }
        else if (name == "Silk Ladder")   { s.family = Family::ladder; s.knob = KnobStyle::chicken; s.width = 940; s.height = 580; s.vizH = 220;
                                            s.groups = { G ("FILTER", { "Cutoff", "Resonance" }, true, 2), G ("ENVELOPE", { "Env Amount", "Fast Envelope" }, false, 1), G ("DRIVE", { "Poles", "Drive" }, false, 1), G ("OUTPUT", { "Output", "Mix" }, false, 1) }; }
        else if (name == "Halo Ring")     { s.family = Family::ring; s.knob = KnobStyle::chicken; s.width = 900; s.height = 560; s.vizH = 220;
                                            s.groups = { G ("CARRIER", { "Frequency" }, true, 1), G ("LFO", { "LFO Shape", "LFO Rate", "LFO Amount" }, false, 3), G ("OUTPUT", { "Drive", "Output", "Mix" }, false, 3) }; }
        else if (name == "Twelve Phase")  { s.family = Family::phase12; s.knob = KnobStyle::chicken; s.width = 920; s.height = 580; s.vizH = 230;
                                            s.groups = { G ("SWEEP", { "Rate", "Sweep", "Centre" }, true, 3), G ("COLOUR", { "Stages", "Resonance", "Stereo Spread" }, false, 3), G ("MIX", { "Mix" }, true, 1) }; }
        else if (name == "Bucket Brigade") { s.family = Family::bbd; s.knob = KnobStyle::chicken; s.width = 980; s.height = 600; s.vizH = 230;
                                            s.groups = { G ("DELAY", { "Sync", "Time", "Feedback" }, true, 3), G ("TONE", { "Filter", "Drive" }, false, 1), G ("MODULATION", { "Mod Rate", "Mod Depth" }, false, 1), G ("MIX", { "Mix" }, true, 1) }; }
        else if (name == "MIDI Shaper")   { s.family = Family::midi; s.knob = KnobStyle::ring; s.groups = { G ("TRANSPOSE", { "Semitones" }, true) }; }
        return s;
    }

    //==============================================================================
    /** The editor. */
    class SkinEditor : public te::Plugin::EditorComponent,
                       private juce::Timer,
                       public juce::FileDragAndDropTarget
    {
    public:
        explicit SkinEditor (te::Plugin& p)
            : plugin (p), sig (Signature::findForPlugin (p)), name (Signature::displayName (p)), spec (specFor (name)),
              accent (sig != nullptr ? juce::Colour (sig->colour) : Theme::violet()), art (artFor (Signature::artKey (p)))
        {
            buildHandles();
            buildViz();
            buildControls();
            buildHeader();
            setSize (spec.width, spec.height);
            startTimerHz (24);
        }

        ~SkinEditor() override   { plugin.edit.flushPluginStateIfNeeded (plugin); }

        bool allowWindowResizing() override                                 { return spec.resizable; }
        juce::ComponentBoundsConstrainer* getBoundsConstrainer() override
        {
            if (! spec.resizable) return nullptr;
            constrainer.setFixedAspectRatio ((double) spec.width / (double) spec.height);
            constrainer.setSizeLimits (spec.width * 3 / 4, spec.height * 3 / 4, spec.width * 2, spec.height * 2);
            return &constrainer;
        }
        juce::ComponentBoundsConstrainer constrainer;

        //==========================================================================
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();
            g.fillAll (Theme::bg());
            if (art.isValid())
            {
                auto scale = juce::jmax ((float) getWidth() / (float) art.getWidth(), (float) getHeight() / (float) art.getHeight());
                auto w = art.getWidth() * scale, h = art.getHeight() * scale;
                g.drawImage (art, juce::Rectangle<float> ((getWidth() - w) * 0.5f, (getHeight() - h) * 0.5f, w, h));
            }
            // darkening so the controls read clearly, strongest at the bottom
            // keep the artwork vivid: a light veil at the top, deeper only where the controls sit, plus an accent wash
            g.setGradientFill (juce::ColourGradient (Theme::bg().withAlpha (0.10f), 0, 0, Theme::bg().withAlpha (0.66f), 0, (float) getHeight(), false));
            g.fillRect (r);
            g.setGradientFill (juce::ColourGradient (neon (accent).withAlpha (0.16f), 0, 0, neon (accent).withAlpha (0.0f), (float) getWidth() * 0.6f, (float) getHeight() * 0.7f, true));
            g.fillRect (r);
            // header band
            auto header = r.removeFromTop (headerH);
            g.setColour (Theme::bg().withAlpha (0.55f));
            g.fillRect (header);
            g.setGradientFill (juce::ColourGradient (neon (accent).withAlpha (0.30f), 0, 0, neon (accent).withAlpha (0.0f), (float) getWidth() * 0.55f, 0, false));
            g.fillRect (header);
            g.setColour (neon (accent));
            g.fillRect (header.removeFromLeft (5));
            g.setColour (Theme::text());
            g.setFont (Theme::font (20.0f, true));
            g.drawText (name.toUpperCase(), 16, 7, getWidth() - 360, 26, juce::Justification::centredLeft);
            g.setColour (Theme::dim());
            g.setFont (Theme::font (10.5f));
            juce::String sub = sig != nullptr ? juce::String (sig->tagline) : plugin.getName();
            g.drawFittedText (sub, 16, 32, getWidth() - 360, 14, juce::Justification::centredLeft, 1);
            g.setColour (neon (accent).withAlpha (0.95f));
            g.setFont (Theme::font (9.0f, true));
            g.drawText ("SQUELCHMOTH SIGNATURE" + (sig != nullptr ? juce::String ("  |  ") + sig->category : juce::String()), 16, 46, getWidth() - 360, 11, juce::Justification::centredLeft);
            // panels
            g.setColour (Theme::panel().withAlpha (0.55f));
            g.fillRoundedRectangle (vizArea.toFloat(), 6.0f);
            g.setColour (Theme::line().withAlpha (0.5f));
            g.drawRoundedRectangle (vizArea.toFloat(), 6.0f, 1.0f);
            for (auto& gb : groupBoxes)
            {
                const auto gbf = gb.bounds.toFloat();
                g.setColour (Theme::panel().withAlpha (0.52f));
                g.fillRoundedRectangle (gbf, 6.0f);
                g.setGradientFill (juce::ColourGradient (neon (accent).withAlpha (0.13f), gbf.getX(), gbf.getY(), neon (accent).withAlpha (0.0f), gbf.getX(), gbf.getY() + juce::jmin (120.0f, gbf.getHeight()), false));
                g.fillRoundedRectangle (gbf, 6.0f);
                g.setColour (neon (accent).withAlpha (0.32f));
                g.drawRoundedRectangle (gbf, 6.0f, 1.0f);
                g.setColour (neon (accent));
                g.setFont (Theme::font (9.0f, true));
                g.drawText (gb.title, gb.bounds.reduced (8, 4), juce::Justification::topLeft);
            }
            g.setColour (Theme::dim().withAlpha (0.7f));
            g.setFont (Theme::font (8.5f));
            g.drawText ("Squelchmoth - created by Jamar Stewart", getLocalBounds().reduced (10, 4), juce::Justification::bottomRight);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto header = r.removeFromTop (headerH).reduced (10, 8);
            bypassBtn.setBounds (header.removeFromRight (64).withHeight (22));
            header.removeFromRight (6);
            abBtn.setBounds (header.removeFromRight (40).withHeight (22));
            header.removeFromRight (6);
            presetBox.setBounds (header.removeFromRight (230).withHeight (22));
            r.reduce (10, 8);

            if (spec.family == Family::synth)   { layoutSynth (r); return; }
            if (spec.family == Family::sampler) { layoutSampler (r); return; }

            auto vizH = spec.vizH > 0 ? spec.vizH : spec.family == Family::dither ? r.getHeight() - 20 : juce::jmin (170, r.getHeight() / 2);
            vizArea = r.removeFromTop (vizH);
            if (viz) viz->setBounds (vizArea);
            if (eqNodes) eqNodes->setBounds (vizArea);
            r.removeFromTop (8);
            if (spec.rowSplit > 0 && spec.rowSplit < (int) groupBoxes.size())
            {
                auto row1 = r.removeFromTop ((int) ((float) (r.getHeight() - 8) * spec.row1Share));
                r.removeFromTop (8);
                layoutGroups (row1, 0, spec.rowSplit);
                layoutGroups (r, spec.rowSplit, (int) groupBoxes.size());
            }
            else layoutGroups (r, 0, (int) groupBoxes.size());
        }

        //==========================================================================
        bool isInterestedInFileDrag (const juce::StringArray& files) override
        {
            for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
            return false;
        }
        void filesDropped (const juce::StringArray& files, int, int) override
        {
            if (auto sp = dynamic_cast<te::SamplerPlugin*> (&plugin))
                for (auto& f : files) addSampleFile (*sp, juce::File (f));
            else if (dynamic_cast<te::ImpulseResponsePlugin*> (&plugin) != nullptr && files.size() > 0)
                loadIR (juce::File (files[0]));
        }

    private:
        //==========================================================================
        te::Plugin& plugin;
        const Signature::Entry* sig;
        juce::String name;
        Spec spec;
        juce::Colour accent;
        juce::Image art;
        static constexpr int headerH = 62;

        std::vector<std::unique_ptr<Handle>> handleStore;
        HandleMap handles;
        std::unique_ptr<Viz> viz;
        std::unique_ptr<EQNodes> eqNodes;
        std::vector<std::unique_ptr<juce::Component>> controls;    // knobs, choice bars, toggles, buttons
        struct GroupBox { juce::String title; juce::Rectangle<int> bounds; std::vector<juce::Component*> items; bool big; int cols = 0; };
        std::vector<GroupBox> groupBoxes;
        juce::Rectangle<int> vizArea;
        juce::ComboBox presetBox;
        juce::TextButton bypassBtn { "BYPASS" }, abBtn { "A/B" };
        juce::ValueTree abState; bool abIsB = false;
        std::vector<juce::File> userPresets;

        // sampler extras
        juce::TextButton addSampleBtn { "ADD SAMPLE..." }, removeSampleBtn { "REMOVE" };
        juce::ListBox zoneList;
        struct ZoneModel : public juce::ListBoxModel
        {
            ZoneModel (SkinEditor& o) : owner (o) {}
            int getNumRows() override { auto sp = dynamic_cast<te::SamplerPlugin*> (&owner.plugin); return sp ? sp->getNumSounds() : 0; }
            void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
            {
                auto sp = dynamic_cast<te::SamplerPlugin*> (&owner.plugin); if (! sp || row >= sp->getNumSounds()) return;
                g.setColour (sel ? owner.accent.withAlpha (0.4f) : (row % 2 ? Theme::panel2().withAlpha (0.5f) : juce::Colours::transparentBlack));
                g.fillRect (0, 0, w, h);
                g.setColour (Theme::text()); g.setFont (Theme::font (11.0f));
                g.drawText (sp->getSoundName (row), 6, 0, w / 2, h, juce::Justification::centredLeft);
                g.setColour (Theme::dim()); g.setFont (Theme::font (10.0f));
                g.drawText ("root " + juce::MidiMessage::getMidiNoteName (sp->getKeyNote (row), true, true, 3) + "   range " + juce::MidiMessage::getMidiNoteName (sp->getMinKey (row), true, true, 3) + " - " + juce::MidiMessage::getMidiNoteName (sp->getMaxKey (row), true, true, 3) + "   " + juce::String (sp->getSoundGainDb (row), 1) + " dB", w / 2, 0, w / 2 - 6, h, juce::Justification::centredRight);
            }
            void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { owner.editZone (row); }
            SkinEditor& owner;
        };
        std::unique_ptr<ZoneModel> zoneModel;

        //==========================================================================
        Handle& add (std::unique_ptr<Handle> h)
        {
            auto& ref = *h;
            handles[h->name] = h.get();
            handleStore.push_back (std::move (h));
            return ref;
        }

        void buildHandles()
        {
            // friendlier labels for AirWindows' abbreviated parameter names
            static const std::map<juce::String, juce::String> labels {
                { "Intensty", "Intensity" }, { "Intenst", "Intensity" }, { "Feedbak", "Feedback" }, { "Monotor", "Mono Below" }, { "TrebSus", "Treble Sustain" }, { "BassSus", "Bass Sustain" },
                { "AttackS", "Attack" }, { "Thresh", "Threshold" }, { "Mewiness", "Mewl" }, { "Grd/Out", "Grind Out" }, { "TGrind", "Grind" }, { "XOver", "Crossover" }, { "BsDrive", "Bass Drive" },
                { "BsVoice", "Bass Voice" }, { "BassOut", "Bass Out" }, { "SbDrive", "Sub Drive" }, { "SbVoice", "Sub Voice" }, { "SubOut", "Sub Out" }, { "Headrm", "Headroom" }, { "Gv Wear", "Groove Wear" },
                { "H Limit", "Highs Limit" }, { "Mid HiP", "Mid Low-Cut" }, { "SideHiP", "Side Low-Cut" }, { "Clas AB", "Class AB" }, { "Clas B", "Class B" }, { "Lean/Ft", "Lean / Fat" }, { "Inv/Wet", "Inverse / Wet" },
                { "Max DS", "Max De-ess" }, { "Rez", "Bit Depth" }, { "Hard", "Hardness" }, { "filters Q", "Q" }, { "22K tap", "22 kHz" }, { "15K tap", "15 kHz" }, { "11K tap", "11 kHz" }, { "Mix proportion", "Mix" },
                { "Low-shelf freq", "Freq" }, { "Low-shelf gain", "Gain" }, { "Low-shelf Q", "Q" }, { "Mid freq 1", "Freq" }, { "Mid gain 1", "Gain" }, { "Mid Q 1", "Q" }, { "Mid freq 2", "Freq" }, { "Mid gain 2", "Gain" }, { "Mid Q 2", "Q" },
                { "High-shelf freq", "Freq" }, { "High-shelf gain", "Gain" }, { "High-shelf Q", "Q" }, { "Output gain", "Output" }, { "Sidechain gain", "Sidechain" }, { "Out Level", "Output" }, { "Output Level", "Output" }, { "Output Trim", "Output" } };
            const bool byID = dynamic_cast<te::FourOscPlugin*> (&plugin) != nullptr;   // 4OSC repeats names (three 'Mix' knobs) - key by ID
            for (auto p : plugin.getAutomatableParameters())
            {
                auto it = labels.find (p->getParameterName());
                auto h = fromParam (*p, it != labels.end() ? it->second : p->getParameterName());
                if (byID) h->name = p->paramID;
                if (name == "Wingspan" || name == "Snap Transient" || name == "Antenna Air") h->bipolar = h->name != "Reaction Speed" && h->name != "Input Trim" && h->name != "filters Q" && h->name != "Output Level";
                add (std::move (h));
            }
            if (auto rc = dynamic_cast<SqRackChannelPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto tb = [&] (const char* nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                tb ("Pitch On", rc->pitchOn); tb ("Mono", rc->mono); tb ("Legato", rc->legato); tb ("Porta", rc->porta); tb ("Cut Self", rc->cutSelf); tb ("Ping Pong", rc->echoPingPong);
                add (choice ("Layer Mode", SqRackChannelPlugin::layerModeNames(), [rc] { return rc->layerMode.get(); }, [rc, um] (int i) { rc->layerMode.setValue (i, um); }));
                for (auto& hs : handleStore) { if (hs->name == "Arp Mode") hs->choices = SqRackChannelPlugin::arpModeNames(); if (hs->name == "Arp Chord") hs->choices = SqRackChannelPlugin::arpChordNames(); if (hs->name == "Pitch" || hs->name.startsWith ("Vel >") || hs->name.startsWith ("Key >") || hs->name == "Echo Pitch") hs->bipolar = true; }
            }
            else if (auto sc = dynamic_cast<SqSynth::SynthCommon*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                // integer parameters become choice bars; on/off ones become lamps
                auto asChoice = [&] (const juce::String& pname, juce::StringArray names) { for (auto& hs : handleStore) if (hs->name == pname) { hs->choices = names; } };
                auto asToggle = [&] (const juce::String& pname) { for (auto& hs : handleStore) if (hs->name == pname) hs->toggle = true; };
                for (int i = 1; i <= 3; ++i) asChoice ("Osc " + juce::String (i) + " Wave", SqSynth::waveNames());
                asChoice ("Wave", dynamic_cast<SqSynth::SqThumpPlugin*> (&plugin) != nullptr ? juce::StringArray { "SINE", "TRI", "WARM" } : SqSynth::waveNames());
                asChoice ("Sub Wave", { "SINE", "TRI", "SAW", "SQUARE" }); asChoice ("Sub Octave", { "-1", "-2" });
                asChoice ("F1 Type", SqSynth::filterTypeNames()); asChoice ("F2 Type", SqSynth::filterTypeNames()); asChoice ("Filter Type", SqSynth::filterTypeNames());
                asChoice ("Routing", { "F1", "SERIES", "PARALLEL" }); asChoice ("Drive Type", SqSynth::driveTypeNames());
                asChoice ("LFO 1 Shape", SqSynth::lfoShapeNames()); asChoice ("LFO 2 Shape", SqSynth::lfoShapeNames());
                asChoice ("Gate Rate", { "1/4", "1/8", "1/16", "1/32" }); asChoice ("Gate Pattern", { "1", "2", "3", "4", "5", "6", "7", "8" });
                asChoice ("Carrier", { "SINE", "BRIGHT" }); asChoice ("Octave", { "-2", "-1", "0", "+1", "+2" });
                asToggle ("Osc 2 Sync"); asToggle ("LFO 1 Sync"); asToggle ("LFO 2 Sync"); asToggle ("Gate On");
                // short knob labels where a row holds eleven knobs (the group title says which operator / sound)
                for (auto& hs : handleStore)
                {
                    if (hs->name.startsWith ("Op ") && hs->name.length() > 5 && juce::CharacterFunctions::isDigit (hs->name[3])) hs->label = hs->name.substring (5);
                }
                // 1.8.1 instruments
                for (int i = 1; i <= 6; ++i) asChoice ("Op " + juce::String (i) + " Wave", SqSynth::opWaveNames());
                asChoice ("Algorithm", SqSynth::algorithmNames()); asChoice ("LFO Shape", SqSynth::lfoShapeNames()); asChoice ("Era", SqSynth::SqLanternPlugin::eraNames()); asChoice ("Filter", { "LADDER", "LP 24" });
                asChoice ("Osc 1 Octave", { "-2", "-1", "0", "+1", "+2" }); asChoice ("Seq Rate", { "1/8", "1/16", "1/32", "1/16T" }); asChoice ("Pattern Rate", { "1/8", "1/16", "1/32" });
                asChoice ("Seq Octave", { "-2", "-1", "0", "+1", "+2" }); asChoice ("Model", SqSynth::drumModelNames());
                if (dynamic_cast<SqSynth::SqLarvaBassPlugin*> (&plugin) != nullptr) asChoice ("Wave", { "SAW", "SQUARE" });
                asToggle ("Sequencer"); asToggle ("Vintage Mode"); asToggle ("Pattern");
                asToggle ("Deck A Stretch"); asToggle ("Deck B Stretch"); asToggle ("Deck A Reverse"); asToggle ("Deck B Reverse");
                for (auto& hs : handleStore) if (hs->name.startsWith ("Deck ") && hs->name.length() > 7) hs->label = hs->name.substring (7);
                for (auto& hs : handleStore)
                {
                    if (hs->name.startsWith ("Step ") && ! hs->name.endsWith ("Note")) hs->toggle = true;
                    if (hs->name.startsWith ("Pattern Step ")) hs->toggle = true;
                    if (hs->name.endsWith (" Model")) hs->choices = SqSynth::drumModelNames();
                    if (hs->name.endsWith (" Osc 1") || hs->name.endsWith (" Osc 2")) hs->choices = SqSynth::drumOscNames();
                    if (hs->name.endsWith (" Pan") || hs->name == "Seq Octave" || hs->name == "Osc 1 Octave") hs->bipolar = hs->choices.isEmpty();
                    if (hs->name.endsWith (" Choke")) hs->choices = { "OFF", "1", "2", "3", "4" };
                }
                for (auto& hs : handleStore) if (hs->name == "Pan" || hs->name == "Filter Env" || hs->name == "F2 Env" || hs->name == "LFO1 > Cut" || hs->name.endsWith ("Semi") || hs->name.endsWith ("Fine") || hs->name == "Key > Bright") hs->bipolar = true;
                add (choice ("Voice Mode", SqSynth::polyModeNames(), [sc] { return sc->polyModeValue().get(); }, [sc, um] (int i) { sc->polyModeValue().setValue (i, um); }));
                add (choice ("Glide Mode", SqSynth::glideModeNames(), [sc] { return sc->glideModeValue().get(); }, [sc, um] (int i) { sc->glideModeValue().setValue (i, um); }));
                add (choice ("Unison", { "1", "2", "3", "4", "6", "8" }, [sc] { const int u = sc->unisonValue().get(); return u <= 1 ? 0 : u == 2 ? 1 : u == 3 ? 2 : u == 4 ? 3 : u <= 6 ? 4 : 5; },
                             [sc, um] (int i) { const int v[] = { 1, 2, 3, 4, 6, 8 }; sc->unisonValue().setValue (v[juce::jlimit (0, 5, i)], um); }));
                add (choice ("Quality", SqSynth::qualityNames(), [sc] { return sc->qualityValue().get(); }, [sc, um] (int i) { sc->qualityValue().setValue (i, um); }));
                add (choice ("Bend Range", { "2", "7", "12", "24" }, [sc] { auto b = sc->bendRangeValue().get(); return b <= 2 ? 0 : b <= 7 ? 1 : b <= 12 ? 2 : 3; },
                             [sc, um] (int i) { const int v[] = { 2, 7, 12, 24 }; sc->bendRangeValue().setValue (v[juce::jlimit (0, 3, i)], um); }));
                {
                    const int mx = sc->maxVoiceCount();
                    juce::StringArray opts; juce::Array<int> vals;
                    for (int v : { 1, 2, 4, 8, 16, 32 }) if (v <= mx) { opts.add (juce::String (v)); vals.add (v); }
                    add (choice ("Max Voices", opts, [sc, vals] { int best = 0; for (int i = 0; i < vals.size(); ++i) if (vals[i] <= sc->maxPolyValue().get()) best = i; return best; },
                                 [sc, um, vals] (int i) { sc->maxPolyValue().setValue (vals[juce::jlimit (0, vals.size() - 1, i)], um); }));
                }
            }
            else if (auto e8 = dynamic_cast<Sq808Plugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Drive Type", Sq808Plugin::driveTypeNames(), [e8] { return e8->driveType.get(); }, [e8, um] (int i) { e8->driveType.setValue (i, um); }));
                add (choice ("Glide Mode", Sq808Plugin::glideModeNames(), [e8] { return e8->glideMode.get(); }, [e8, um] (int i) { e8->glideMode.setValue (i, um); }));
                add (choice ("Voice Mode", { "MONO", "POLY" }, [e8] { return e8->polyMode.get(); }, [e8, um] (int i) { e8->polyMode.setValue (i, um); }));
                add (choice ("Bend Range", { "2", "7", "12", "24" }, [e8] { auto b = e8->bendRange.get(); return b <= 2 ? 0 : b <= 7 ? 1 : b <= 12 ? 2 : 3; },
                             [e8, um] (int i) { const int v[] = { 2, 7, 12, 24 }; e8->bendRange.setValue (v[juce::jlimit (0, 3, i)], um); }));
            }
            else if (auto tu = dynamic_cast<SqTunePlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const char* nm, juce::StringArray names, juce::CachedValue<int>& cv) { add (choice (nm, names, [&cv] { return cv.get(); }, [&cv, um] (int i) { cv.setValue (i, um); })); };
                auto tb = [&] (const char* nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                ci ("Engine", SqTunePlugin::engineNames(), tu->engine);
                ci ("Character", SqTunePlugin::characterNames(), tu->character);
                ci ("Voice Type", SqTunePlugin::voiceNames(), tu->voiceType);
                ci ("Key", SqScales::noteNames(), tu->keyRoot);
                ci ("Scale", SqTunePlugin::scaleNames(), tu->scaleIndex);
                ci ("Vibrato Shape", SqTunePlugin::vibShapeNames(), tu->vibShape);
                ci ("MIDI Targets", SqTunePlugin::midiModeNames(), tu->midiMode);
                for (int i = 0; i < 4; ++i) ci (("Voice " + juce::String (i + 1) + " Mode").toRawUTF8(), SqTunePlugin::harmModeNames(), tu->harmMode[(size_t) i]);
                tb ("Formant Preserve", tu->preserve); tb ("Conform", tu->conform); tb ("Auto-Key", tu->autoKey); tb ("Learn Scale", tu->learnScale);
                tb ("All Octaves", tu->allOctaves); tb ("Graph Notes", tu->graphOn); tb ("Lead Follows MIDI", tu->midiLead);
                // Moth Tune 2
                ci ("Dynamic Vibrato", SqTunePlugin::dvModeNames(), tu->dvMode);
                ci ("Harmony Trigger", SqTunePlugin::triggerNames(), tu->harmTrigger);
                { juce::StringArray ccs; for (int c = 0; c < 128; ++c) ccs.add ("CC " + juce::String (c)); ci ("Harmony CC", ccs, tu->harmCC); }
                tb ("Harmony Latch", tu->harmLatch); tb ("Formant Link", tu->formantLink); tb ("Auto Range", tu->autoRange); tb ("Straight Notes Only", tu->vibFxStraight);
                {
                    const std::pair<const char*, const char*> shortNames[] { { "Vibrato Threshold", "Threshold" }, { "Vibrato Detect Delay", "Detect" }, { "Vibrato Attack", "Attack" },
                        { "Vibrato Irregularity", "Irregular" }, { "Pitch Depth", "Pitch" }, { "Amplitude Depth", "Amplitude" }, { "Timbre Depth", "Timbre" }, { "Dynamic Vibrato", "Mode" },
                        { "Harmony Trigger", "Trigger" }, { "Harmony CC", "CC" }, { "Harmony Latch", "Latch" }, { "Straight Notes Only", "Straight Only" },
                        { "Vibrato Shape", "Shape" }, { "Vibrato Rate", "Rate" }, { "Vibrato Depth", "Depth" }, { "Vibrato Delay", "Delay" }, { "Vibrato Onset", "Onset" }, { "Vibrato Variation", "Variation" },
                        { "Harmony Detune", "Detune" }, { "Harmony Timing", "Timing" } };
                    for (auto& hs : handleStore) for (auto& sn : shortNames) if (hs->name == sn.first) hs->label = sn.second;
                }
                // MIDI track: every track that holds MIDI clips
                juce::StringArray tracks { "(none)" };
                for (auto t : te::getAudioTracks (plugin.edit))
                    for (auto c : t->getClips()) if (dynamic_cast<te::MidiClip*> (c) != nullptr) { tracks.addIfNotAlreadyThere (t->getName()); break; }
                add (choice ("MIDI Track", tracks, [tu, tracks] { return juce::jmax (0, tracks.indexOf (tu->midiTrack.get())); },
                             [tu, tracks, um] (int i) { tu->midiTrack.setValue (i <= 0 ? juce::String() : tracks[i], um); if (i > 0) tu->midiMode.setValue (2, um); }));
                for (auto& hs : handleStore)
                    if (hs->name.startsWith ("Voice ")) hs->label = hs->name.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);
                for (auto& hs : handleStore)
                    if (hs->name == "Voice 1 Pan" || hs->name == "Voice 2 Pan" || hs->name == "Voice 3 Pan" || hs->name == "Voice 4 Pan" || hs->name == "Natural Vibrato" || hs->name == "Transpose" || hs->name == "Formant" || hs->name == "Detune" || hs->name.endsWith ("Interval") || hs->name.endsWith ("Formant"))
                        hs->bipolar = true;
            }
            else if (auto fx = dynamic_cast<SqTuneFxPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const char* nm, juce::StringArray names, juce::CachedValue<int>& cv) { add (choice (nm, names, [&cv] { return cv.get(); }, [&cv, um] (int i) { cv.setValue (i, um); })); };
                auto tb = [&] (const char* nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                ci ("Engine", SqTunePlugin::engineNames(), fx->engine);
                ci ("Voice Type", SqTunePlugin::voiceNames(), fx->voiceType);
                ci ("Key", SqScales::noteNames(), fx->keyRoot);
                ci ("Scale", SqTunePlugin::scaleNames(), fx->scaleIndex);
                ci ("Motion Mode", SqTuneFxPlugin::motionModeNames(), fx->motionMode);
                ci ("Sync", SqTuneFxPlugin::motionSyncNames(), fx->motionSync);
                juce::StringArray lens; for (int i = 1; i <= SqTuneFxPlugin::maxSteps; ++i) lens.add (juce::String (i));
                add (choice ("Steps", lens, [fx] { return juce::jlimit (1, SqTuneFxPlugin::maxSteps, fx->motionLen.get()) - 1; }, [fx, um] (int i) { fx->motionLen.setValue (i + 1, um); }));
                juce::StringArray pats { "(custom)" }; for (auto& pt : SqTuneFxPlugin::factoryPatterns()) pats.add (pt.name);
                add (choice ("Pattern", pats,
                             [fx]
                             {
                                 auto& fp = SqTuneFxPlugin::factoryPatterns();
                                 for (size_t i = 0; i < fp.size(); ++i)
                                 {
                                     auto a = juce::StringArray::fromTokens (fp[i].steps, " ", ""), b = juce::StringArray::fromTokens (fx->motionSteps.get(), " ", "");
                                     bool same = fx->motionLen.get() == fp[i].len;
                                     for (int k = 0; same && k < a.size(); ++k) same = a[k] == b[k];
                                     if (same) return (int) i + 1;
                                 }
                                 return 0;
                             },
                             [fx, um] (int i)
                             {
                                 auto& fp = SqTuneFxPlugin::factoryPatterns();
                                 if (i <= 0 || i > (int) fp.size()) return;
                                 auto& pt = fp[(size_t) i - 1];
                                 auto toks = juce::StringArray::fromTokens (pt.steps, " ", "");
                                 while (toks.size() < SqTuneFxPlugin::maxSteps) toks.add ("0");
                                 fx->motionSteps.setValue (toks.joinIntoString (" "), um); fx->motionLen.setValue (pt.len, um);
                                 fx->motionSync.setValue (pt.sync, um); fx->motionDiatonic.setValue (pt.diatonic, um);
                                 if (fx->motionMode.get() == 0) fx->motionMode.setValue (1, um);
                             }));
                tb ("Formant Preserve", fx->preserve); tb ("Auto-Key", fx->autoKey); tb ("Scale Steps", fx->motionDiatonic); tb ("Latch", fx->motionLatch);
                add (toggle ("Hold", [fx] { return fx->momentaryHeld.load(); }, [fx] (bool b) { fx->momentaryHeld = b; }));
                for (auto& hs : handleStore) if (hs->name == "Transpose" || hs->name == "Formant") hs->bipolar = true;
            }
            else if (auto dy = dynamic_cast<SqApexDynamicsPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const char* nm, juce::StringArray names, juce::CachedValue<int>& cv) { add (choice (nm, names, [&cv] { return cv.get(); }, [&cv, um] (int i) { cv.setValue (i, um); })); };
                auto tb = [&] (const char* nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                ci ("Mode", SqApexDynamicsPlugin::modeNames(), dy->mode); ci ("Detection", SqApexDynamicsPlugin::detectionNames(), dy->detection);
                ci ("Key Filter", SqApexDynamicsPlugin::filterNames(), dy->scFilter);
                tb ("Auto Makeup", dy->autoMakeup); tb ("Attenuation Listen", dy->attenListen); tb ("Key Listen", dy->keyListen); tb ("External Key", dy->externalKey); tb ("Stereo Link", dy->link);
                auto srcs = plugin.getSidechainSourceNames (true);
                add (choice ("Key Input", srcs, [this, srcs] { return juce::jmax (0, srcs.indexOf (plugin.getSidechainSourceName())); },
                             [this, srcs, dy, um] (int i) { plugin.setSidechainSourceByName (srcs[i]); dy->externalKey.setValue (i > 0, um); }));
                if (auto h = handles.find ("Ratio"); h != handles.end())
                    h->second->text = [dy] { return Apex::Curve::ratioText (dy->bank[SqApexDynamicsPlugin::ratio], dy->mode.get()); };
            }
            else if (auto li = dynamic_cast<SqApexLimiterPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (toggle ("Auto Release", [li] { return li->autoRelease.get(); }, [li, um] (bool b) { li->autoRelease.setValue (b, um); }));
                add (toggle ("Link", [li] { return li->link.get(); }, [li, um] (bool b) { li->link.setValue (b, um); }));
                juce::StringArray tn; for (auto& t : SqApexLimiterPlugin::targets()) tn.add (t.name);
                add (choice ("Target", tn, [li] { return li->target.get(); }, [li, um] (int i) { li->target.setValue (i, um); }));
            }
            else if (auto mbp = dynamic_cast<SqApexMultibandPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Bands", { "1", "2", "3", "4", "5", "6" }, [mbp] { return juce::jlimit (1, 6, mbp->numBands.get()) - 1; }, [mbp, um] (int i) { mbp->numBands.setValue (i + 1, um); }));
                add (choice ("Phase", { "MINIMUM", "LINEAR" }, [mbp] { return mbp->linearPhase.get() ? 1 : 0; }, [mbp, um] (int i) { mbp->linearPhase.setValue (i == 1, um); }));
                add (choice ("Detection", { "SMART", "RMS", "AVERAGE", "PEAK" }, [mbp] { return juce::jlimit (0, 3, mbp->detection.get()); }, [mbp, um] (int i) { mbp->detection.setValue (i, um); }));
                for (int b = 0; b < SqApexMultibandPlugin::maxBands; ++b)
                {
                    const auto n = juce::String (b + 1);
                    add (toggle ("Band " + n + " Solo", [mbp, b] { return mbp->solo[(size_t) b].get(); }, [mbp, b, um] (bool v) { mbp->solo[(size_t) b].setValue (v, um); }));
                    add (toggle ("Band " + n + " Bypass", [mbp, b] { return mbp->bypass[(size_t) b].get(); }, [mbp, b, um] (bool v) { mbp->bypass[(size_t) b].setValue (v, um); }));
                    add (toggle ("Band " + n + " Flip", [mbp, b] { return mbp->flip[(size_t) b].get(); }, [mbp, b, um] (bool v) { mbp->flip[(size_t) b].setValue (v, um); }));
                    add (toggle ("Band " + n + " Auto Gain", [mbp, b] { return mbp->autoGain[(size_t) b].get(); }, [mbp, b, um] (bool v) { mbp->autoGain[(size_t) b].setValue (v, um); }));
                }
                for (auto& hs : handleStore)
                    if (hs->name.startsWith ("Band ")) hs->label = hs->name.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);
                for (auto& hs : handleStore) if (hs->name.endsWith ("Depth") || hs->name.endsWith ("Gain") || hs->name == "Input" || hs->name == "Output") hs->bipolar = true;
            }
            else if (dynamic_cast<SqApexTriadPlugin*> (&plugin) != nullptr)
            {
                for (auto& hs : handleStore) if (hs->name.startsWith ("Low ") || hs->name.startsWith ("Mid ") || hs->name.startsWith ("High ")) if (! hs->name.endsWith ("Crossover")) hs->label = hs->name.fromFirstOccurrenceOf (" ", false, false);
            }
            else if (dynamic_cast<SqTimeWeaverPlugin*> (&plugin) != nullptr || dynamic_cast<SqLarvaFilterPlugin*> (&plugin) != nullptr)
            {
                for (auto& hs : handleStore)
                {
                    if (hs->name == "Time Slot") hs->choices = SqTimeWeaverPlugin::timeSlotNames();
                    if (hs->name == "Volume Slot") hs->choices = SqTimeWeaverPlugin::volumeSlotNames();
                    if (hs->name == "Tempo Sync" || hs->name == "Time Ramp" || hs->name == "Sequencer" || hs->name.endsWith (" On")) hs->toggle = true;
                    if (hs->name == "Routing") hs->choices = SqLarvaFilterPlugin::routingNames();
                    if (hs->name == "Seq Rate") hs->choices = SqLarvaFilterPlugin::rateNames();
                    if (hs->name.startsWith ("Stage ") && hs->name.endsWith (" Type")) hs->choices = SqLarvaFilterPlugin::typeNames();
                    if (hs->name.startsWith ("Stage ") && (hs->name.endsWith ("Amount"))) hs->bipolar = true;
                    if (hs->name.startsWith ("Stage ")) hs->label = hs->name.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);
                }
            }
            else if (auto sb = dynamic_cast<SqApexSubPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Range", SqApexSubPlugin::rangeNames(), [sb] { return sb->range.get(); }, [sb, um] (int i) { sb->range.setValue (i, um); }));
                add (choice ("Drive Type", SqApexSubPlugin::driveNames(), [sb] { return sb->driveType.get(); }, [sb, um] (int i) { sb->driveType.setValue (i, um); }));
                add (toggle ("MIDI Tune", [sb] { return sb->midiTune.get(); }, [sb, um] (bool b) { sb->midiTune.setValue (b, um); }));
            }
            else if (auto ar = dynamic_cast<SqAuroraPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Room", SqAuroraPlugin::roomNames(), [ar] { return ar->roomType.get(); }, [ar, um] (int i) { ar->roomType.setValue (i, um); }));
                add (toggle ("Freeze", [ar] { return ar->freeze.get(); }, [ar, um] (bool b) { ar->freeze.setValue (b, um); }));
            }
            else if (auto op = dynamic_cast<SqOptoPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Mode", { "COMPRESS", "LIMIT" }, [op] { return op->limitMode.get() ? 1 : 0; }, [op, um] (int i) { op->limitMode.setValue (i == 1, um); }));
                add (choice ("Meter", { "GAIN REDUCTION", "OUTPUT" }, [op] { return op->meterMode.get(); }, [op, um] (int i) { op->meterMode.setValue (i, um); }));
            }
            else if (auto pq = dynamic_cast<SqPulseEqPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const char* nm, const std::vector<float>& tbl, juce::CachedValue<int>& cv)
                { add (choice (nm, SqPulseEqPlugin::names (tbl), [&cv] { return cv.get(); }, [&cv, um] (int i) { cv.setValue (i, um); })); };
                ci ("Low Freq", SqPulseEqPlugin::lowFreqs(), pq->lowFreq); ci ("High Freq", SqPulseEqPlugin::highFreqs(), pq->highFreq);
                ci ("Atten Sel", SqPulseEqPlugin::attenFreqs(), pq->attenFreq); ci ("Mid Low Freq", SqPulseEqPlugin::midLowFreqs(), pq->midLowFreq);
                ci ("Dip Freq", SqPulseEqPlugin::dipFreqs(), pq->dipFreq); ci ("Mid High Freq", SqPulseEqPlugin::midHighFreqs(), pq->midHighFreq);
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto oc = dynamic_cast<SqOilCanPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Mode", { "ECHO", "DOUBLER" }, [oc] { return oc->mode.get(); }, [oc, um] (int i) { oc->mode.setValue (i, um); }));
                add (choice ("Sync", { "FREE", "1/16", "1/8", "1/8 D", "1/4", "1/2" }, [oc] { return oc->sync.get(); }, [oc, um] (int i) { oc->sync.setValue (i, um); }));
            }
            else if (auto rdp = dynamic_cast<SqRadiantPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Circuit", SqRadiantPlugin::archNames(), [rdp] { return rdp->arch.get(); }, [rdp, um] (int i) { rdp->arch.setValue (i, um); }));
                add (toggle ("Mic", [rdp] { return rdp->mic.get(); }, [rdp, um] (bool b) { rdp->mic.setValue (b, um); }));
                add (toggle ("Noisy", [rdp] { return rdp->noisy.get(); }, [rdp, um] (bool b) { rdp->noisy.setValue (b, um); }));
                add (choice ("Oversampling", SqRadiantPlugin::osNames(), [rdp] { return rdp->oversample.get(); }, [rdp, um] (int i) { rdp->oversample.setValue (i, um); }));
                add (choice ("Mid/Side", SqRadiantPlugin::msNames(), [rdp] { return rdp->ms.get(); }, [rdp, um] (int i) { rdp->ms.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Bass" || hs->name == "Treble" || hs->name == "Output" || hs->name == "Trim" || hs->name == "Input") hs->bipolar = true;
            }
            else if (auto rpr = dynamic_cast<SqRadiantPrePlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (toggle ("Bias", [rpr] { return rpr->bias.get(); }, [rpr, um] (bool b) { rpr->bias.setValue (b, um); }));
                add (toggle ("Noisy", [rpr] { return rpr->noisy.get(); }, [rpr, um] (bool b) { rpr->noisy.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto mk = dynamic_cast<SqMothKeyPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (toggle ("Listen", [mk] { return mk->listen.get(); }, [mk, um] (bool b) { mk->listen.setValue (b, um); }));
                add (toggle ("Clear", [] { return false; }, [mk] (bool) { mk->clearListening(); }));
                add (toggle ("Analyse", [mk] { return mk->isAnalysing(); }, [mk] (bool) { mk->analyseHeard(); }));
                add (toggle ("Auto Apply", [mk] { return mk->autoApply.get(); }, [mk, um] (bool b) { mk->autoApply.setValue (b, um); }));
                add (toggle ("Set Tempo", [mk] { return mk->applyTempo.get(); }, [mk, um] (bool b) { mk->applyTempo.setValue (b, um); }));
                add (toggle ("Apply to Session", [] { return false; }, [mk, um] (bool) { um->beginNewTransaction ("Moth Key: apply"); mk->applyToSession(); }));
            }
            else if (auto scp = dynamic_cast<SqSirenCompPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Model 1", SqSirenCompPlugin::modelNames(), [scp] { return scp->model1.get(); }, [scp, um] (int i) { scp->model1.setValue (i, um); }));
                add (choice ("Model 2", SqSirenCompPlugin::modelNames(), [scp] { return scp->model2.get(); }, [scp, um] (int i) { scp->model2.setValue (i, um); }));
                add (choice ("FET 1", SqSirenCompPlugin::fetNames(), [scp] { return scp->fet1.get(); }, [scp, um] (int i) { scp->fet1.setValue (i, um); }));
                add (choice ("FET 2", SqSirenCompPlugin::fetNames(), [scp] { return scp->fet2.get(); }, [scp, um] (int i) { scp->fet2.setValue (i, um); }));
                add (toggle ("Dual", [scp] { return scp->dual.get(); }, [scp, um] (bool b) { scp->dual.setValue (b, um); }));
                add (toggle ("Auto Make-Up", [scp] { return scp->autoMakeup.get(); }, [scp, um] (bool b) { scp->autoMakeup.setValue (b, um); }));
                add (toggle ("Punch", [scp] { return scp->punch.get(); }, [scp, um] (bool b) { scp->punch.setValue (b, um); }));
                add (toggle ("Key Input", [scp] { return scp->keyOn.get(); }, [scp, um] (bool b) { scp->keyOn.setValue (b, um); }));
                add (toggle ("Learn", [scp] { return scp->learning.load(); }, [scp] (bool b) { if (b) scp->startLearning(); else { scp->stopLearning(); scp->makeSuggestion (scp->amount.get()); scp->preview.setValue (true, nullptr); } }));
                add (choice ("Amount", SqSirenCompPlugin::amountNames(), [scp] { return scp->amount.get(); }, [scp] (int i) { scp->makeSuggestion (i); }));
                add (toggle ("Preview", [scp] { return scp->preview.get(); }, [scp] (bool b) { scp->preview.setValue (b && scp->hasSuggestion(), nullptr); }));
                add (toggle ("Apply", [] { return false; }, [scp, um] (bool) { um->beginNewTransaction ("Siren Comp: apply Assist"); scp->applySuggestion(); }));
                for (auto& hs : handleStore) if (hs->name == "Make-Up" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto seq = dynamic_cast<SqSirenEqPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                juce::StringArray harmN; for (int hN = 1; hN <= 16; ++hN) harmN.add ("H" + juce::String (hN));
                for (int b = 0; b < 6; ++b)
                {
                    const auto pre = "B" + juce::String (b + 1) + " ";
                    add (toggle (pre + "On", [seq, b] { return seq->on[(size_t) b].get(); }, [seq, um, b] (bool v) { seq->on[(size_t) b].setValue (v, um); }));
                    add (choice (pre + "Type", SqSirenEqPlugin::typeNames(), [seq, b] { return seq->type[(size_t) b].get(); }, [seq, um, b] (int i) { seq->type[(size_t) b].setValue (i, um); }));
                    add (choice (pre + "Mode", SqSirenEqPlugin::modeNames(), [seq, b] { return seq->mode[(size_t) b].get(); }, [seq, um, b] (int i) { seq->mode[(size_t) b].setValue (i, um); }));
                    add (choice (pre + "Harmonic", harmN, [seq, b] { return seq->harm[(size_t) b].get() - 1; }, [seq, um, b] (int i) { seq->harm[(size_t) b].setValue (i + 1, um); }));
                    add (toggle (pre + "Expand", [seq, b] { return seq->expand[(size_t) b].get(); }, [seq, um, b] (bool v) { seq->expand[(size_t) b].setValue (v, um); }));
                    add (toggle (pre + "Solo", [seq, b] { return seq->solo[(size_t) b].get(); }, [seq, b] (bool v) { seq->solo[(size_t) b].setValue (v, nullptr); }));
                    add (toggle (pre + "Mute", [seq, b] { return seq->mute[(size_t) b].get(); }, [seq, um, b] (bool v) { seq->mute[(size_t) b].setValue (v, um); }));
                }
                add (choice ("Low Slope", SqSirenEqPlugin::slopeNames(), [seq] { return seq->hpSlope.get(); }, [seq, um] (int i) { seq->hpSlope.setValue (i, um); }));
                add (choice ("High Slope", SqSirenEqPlugin::slopeNames(), [seq] { return seq->lpSlope.get(); }, [seq, um] (int i) { seq->lpSlope.setValue (i, um); }));
                add (toggle ("Learn", [seq] { return seq->learning.load(); }, [seq] (bool b) { if (b) seq->startLearning(); else { seq->finishLearning(); if (seq->hasSuggestion()) seq->preview.setValue (true, nullptr); } }));
                add (toggle ("Preview", [seq] { return seq->preview.get(); }, [seq] (bool b) { seq->preview.setValue (b && seq->hasSuggestion(), nullptr); }));
                add (toggle ("Apply", [] { return false; }, [seq, um] (bool) { um->beginNewTransaction ("Siren EQ: apply Vocal Learning"); seq->applySuggestion(); }));
                for (auto& hs : handleStore)
                {
                    if (hs->name.startsWith ("Band ")) hs->label = hs->name.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);
                    else if (hs->name.length() > 3 && hs->name[0] == 'B' && hs->name[1] >= '1' && hs->name[1] <= '6' && hs->name[2] == ' ') hs->label = hs->name.substring (3);
                    if (hs->name.endsWith ("Gain") || hs->name == "Air" || hs->name == "Tilt" || hs->name == "Output") hs->bipolar = true;
                }
            }
            else if (auto hd = dynamic_cast<SqHushDeEss2Plugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (toggle ("Link", [hd] { return hd->link.get(); }, [hd, um] (bool b) { hd->link.setValue (b, um); }));
                add (toggle ("Solo", [hd] { return hd->solo.get(); }, [hd] (bool b) { hd->solo.setValue (b, nullptr); }));
                add (toggle ("Learn", [hd] { return hd->learning.load(); }, [hd] (bool b) { if (b) hd->startLearning(); else { hd->finishLearning(); hd->preview.setValue (true, nullptr); } }));
                add (toggle ("Preview", [hd] { return hd->preview.get(); }, [hd] (bool b) { hd->preview.setValue (b && hd->hasSuggestion(), nullptr); }));
                add (toggle ("Apply", [] { return false; }, [hd, um] (bool) { um->beginNewTransaction ("Hush: apply Assist"); hd->applySuggestion(); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto vep = dynamic_cast<SqVellumPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Low Freq", SqVellumPlugin::freqNames (Vellum::lowFreqs()), [vep] { return vep->lowFreq.get(); }, [vep, um] (int i) { vep->lowFreq.setValue (i, um); }));
                add (choice ("Mid Freq", SqVellumPlugin::freqNames (Vellum::midFreqs()), [vep] { return vep->midFreq.get(); }, [vep, um] (int i) { vep->midFreq.setValue (i, um); }));
                add (choice ("High Freq", SqVellumPlugin::freqNames (Vellum::highFreqs()), [vep] { return vep->highFreq.get(); }, [vep, um] (int i) { vep->highFreq.setValue (i, um); }));
                add (toggle ("Mid 2 On", [vep] { return vep->mid2On.get(); }, [vep, um] (bool b) { vep->mid2On.setValue (b, um); }));
                add (toggle ("Stepped", [vep] { return vep->stepped.get(); }, [vep, um] (bool b) { vep->stepped.setValue (b, um); }));
                add (choice ("Channels", SqVellumPlugin::targetNames(), [vep] { return vep->target.get(); }, [vep, um] (int i) { vep->target.setValue (i, um); }));
                add (choice ("Oversampling", SqVellumPlugin::osNames(), [vep] { return vep->oversample.get(); }, [vep, um] (int i) { vep->oversample.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Low" || hs->name == "Mid" || hs->name == "Mid 2" || hs->name == "High" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto trp = dynamic_cast<SqTremorPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Mode", SqTremorPlugin::modeNames(), [trp] { return trp->mode.get(); }, [trp, um] (int i) { trp->mode.setValue (i, um); }));
                add (choice ("Shape", SqColor111bBase::shapeNames(), [trp] { return trp->shape.get(); }, [trp, um] (int i) { trp->shape.setValue (i, um); }));
                add (toggle ("Sync", [trp] { return trp->syncOn.get(); }, [trp, um] (bool b) { trp->syncOn.setValue (b, um); }));
                add (choice ("Division", SqColor111bBase::syncNames (false), [trp] { return trp->division.get(); }, [trp, um] (int i) { trp->division.setValue (i, um); }));
                add (choice ("Step Length", SqColor111bBase::syncNames (false), [trp] { return trp->stepDivision.get(); }, [trp, um] (int i) { trp->stepDivision.setValue (i, um); }));
                add (choice ("Steps", { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16" }, [trp] { return trp->numSteps.get() - 1; }, [trp, um] (int i) { trp->numSteps.setValue (i + 1, um); }));
                add (choice ("Pattern", SqTremorPlugin::patternNames(), [trp] { return trp->pattern.get(); }, [trp, um] (int i) { trp->steps.setValue ({}, um); trp->pattern.setValue (i, um); }));
                add (toggle ("Retrigger", [trp] { return trp->retrigOn.get(); }, [trp, um] (bool b) { trp->retrigOn.setValue (b, um); }));
                add (choice ("Dynamics", SqTremorPlugin::dynNames(), [trp] { return trp->dynMode.get(); }, [trp, um] (int i) { trp->dynMode.setValue (i, um); }));
                add (toggle ("Key Input", [trp] { return trp->keyOn.get(); }, [trp, um] (bool b) { trp->keyOn.setValue (b, um); }));
                add (toggle ("Harmonic", [trp] { return trp->harmonicOn.get(); }, [trp, um] (bool b) { trp->harmonicOn.setValue (b, um); }));
                add (choice ("Analog", SqTremorPlugin::analogNames(), [trp] { return trp->analog.get(); }, [trp, um] (int i) { trp->analog.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Feel") hs->bipolar = true;
            }
            else if (auto orp = dynamic_cast<SqOrbitPanPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Mode", SqOrbitPanPlugin::modeNames(), [orp] { return orp->mode.get(); }, [orp, um] (int i) { orp->mode.setValue (i, um); }));
                add (choice ("Shape", SqColor111bBase::shapeNames(), [orp] { return orp->shape.get(); }, [orp, um] (int i) { orp->shape.setValue (i, um); }));
                add (toggle ("Sync", [orp] { return orp->syncOn.get(); }, [orp, um] (bool b) { orp->syncOn.setValue (b, um); }));
                add (choice ("Division", SqColor111bBase::syncNames (false), [orp] { return orp->division.get(); }, [orp, um] (int i) { orp->division.setValue (i, um); }));
                add (choice ("Step Length", SqColor111bBase::syncNames (false), [orp] { return orp->stepDivision.get(); }, [orp, um] (int i) { orp->stepDivision.setValue (i, um); }));
                add (choice ("Steps", { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16" }, [orp] { return orp->numSteps.get() - 1; }, [orp, um] (int i) { orp->numSteps.setValue (i + 1, um); }));
                add (choice ("Pattern", SqOrbitPanPlugin::patternNames(), [orp] { return orp->pattern.get(); }, [orp, um] (int i) { orp->steps.setValue ({}, um); orp->pattern.setValue (i, um); }));
                add (toggle ("Audio Trigger", [orp] { return orp->triggerOn.get(); }, [orp, um] (bool b) { orp->triggerOn.setValue (b, um); }));
                add (choice ("Divider", { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16" }, [orp] { return orp->divider.get() - 1; }, [orp, um] (int i) { orp->divider.setValue (i + 1, um); }));
                add (toggle ("Key Input", [orp] { return orp->keyOn.get(); }, [orp, um] (bool b) { orp->keyOn.setValue (b, um); }));
                add (toggle ("Binaural", [orp] { return orp->binauralOn.get(); }, [orp, um] (bool b) { orp->binauralOn.setValue (b, um); }));
                add (choice ("Pan Law", SqOrbitPanPlugin::lawNames(), [orp] { return orp->panLaw.get(); }, [orp, um] (int i) { orp->panLaw.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Offset" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto msp = dynamic_cast<SqMirrorShiftPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Style", SqMirrorShiftPlugin::styleNames(), [msp] { return msp->style.get(); }, [msp, um] (int i) { msp->style.setValue (i, um); }));
                add (toggle ("Key Input", [msp] { return msp->keyOn.get(); }, [msp, um] (bool b) { msp->keyOn.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto mlp = dynamic_cast<SqMirrorShiftLitePlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Style", SqMirrorShiftPlugin::styleNames (3), [mlp] { return mlp->style.get(); }, [mlp, um] (int i) { mlp->style.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto shp = dynamic_cast<SqShutterPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Multiply", SqShutterBase::multiplyNames(), [shp] { return shp->multiply.get(); }, [shp, um] (int i) { shp->multiply.setValue (i, um); }));
                add (choice ("Converter", SqShutterPlugin::bitsNames(), [shp] { return shp->bits.get(); }, [shp, um] (int i) { shp->bits.setValue (i, um); }));
                add (choice ("Algorithm", SqShutterPlugin::algoNames(), [shp] { return shp->algo.get(); }, [shp, um] (int i) { shp->algo.setValue (i, um); }));
                add (choice ("Sync A", SqColor111bBase::syncNames (true), [shp] { return shp->syncA.get(); }, [shp, um] (int i) { shp->syncA.setValue (i, um); }));
                add (choice ("Sync B", SqColor111bBase::syncNames (true), [shp] { return shp->syncB.get(); }, [shp, um] (int i) { shp->syncB.setValue (i, um); }));
                add (toggle ("Freeze", [shp] { return shp->freeze.get(); }, [shp, um] (bool b) { shp->freeze.setValue (b, um); }));
                add (choice ("Time Change", { "Glitch", "Smooth" }, [shp] { return shp->smoothOn.get() ? 1 : 0; }, [shp, um] (int i) { shp->smoothOn.setValue (i == 1, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Input") hs->bipolar = true;
            }
            else if (auto slp = dynamic_cast<SqShutterLitePlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Multiply", SqShutterBase::multiplyNames(), [slp] { return slp->multiply.get(); }, [slp, um] (int i) { slp->multiply.setValue (i, um); }));
                add (toggle ("Freeze", [slp] { return slp->freeze.get(); }, [slp, um] (bool b) { slp->freeze.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto sdp = dynamic_cast<SqShardPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const juce::String& nm, juce::StringArray names, juce::CachedValue<int>& cv) { add (choice (nm, names, [&cv] { return cv.get(); }, [&cv, um] (int i) { cv.setValue (i, um); })); };
                auto tb = [&] (const juce::String& nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                ci ("Engine", SqShardPlugin::engineNames(), sdp->engine); ci ("Recycle Mode", SqShardPlugin::fbNames(), sdp->fbMode); ci ("Gate/Duck", SqShardPlugin::dynNames(), sdp->dynMode);
                ci ("Splice Sync", SqColor111bBase::syncNames (true), sdp->spliceSync); ci ("Delay Sync", SqColor111bBase::syncNames (true), sdp->delaySync);
                ci ("Key", SqSessionKey::rootNames(), sdp->keyRoot); ci ("Scale", SqSessionKey::scaleNames(), sdp->scale);
                ci ("LFO 1 Target", SqShardPlugin::targetNames(), sdp->lfo1Target); ci ("LFO 1 Shape", SqColor111bBase::shapeNames(), sdp->lfo1Shape); ci ("LFO 1 Sync", SqColor111bBase::syncNames (true), sdp->lfo1Sync);
                ci ("LFO 2 Target", SqShardPlugin::targetNames(), sdp->lfo2Target); ci ("LFO 2 Shape", SqColor111bBase::shapeNames(), sdp->lfo2Shape); ci ("LFO 2 Sync", SqColor111bBase::syncNames (true), sdp->lfo2Sync);
                ci ("Env Target", SqShardPlugin::targetNames(), sdp->envTarget);
                tb ("Quantize", sdp->quantize); tb ("Session Key", sdp->autoKey); tb ("Key Input", sdp->keyOn);
                for (auto& hs : handleStore) if (hs->name == "Pitch" || hs->name == "Output" || hs->name.endsWith ("Depth") || hs->name.startsWith ("Pitch ") || hs->name.endsWith (" R")) hs->bipolar = true;
            }
            else if (auto mup = dynamic_cast<SqMurmurationPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Sync", SqColor111bBase::syncNames (true), [mup] { return mup->sync.get(); }, [mup, um] (int i) { mup->sync.setValue (i, um); }));
                add (choice ("Mod Shape", SqColor111bBase::shapeNames(), [mup] { return mup->modShape.get(); }, [mup, um] (int i) { mup->modShape.setValue (i, um); }));
                add (choice ("Shimmer Pitch", SqMurmurationPlugin::shimmerNames(), [mup] { return mup->shimmerSt.get(); }, [mup, um] (int i) { mup->shimmerSt.setValue (i, um); }));
                add (toggle ("Freeze", [mup] { return mup->freeze.get(); }, [mup, um] (bool b) { mup->freeze.setValue (b, um); }));
                add (choice ("Freeze Mode", SqMurmurationPlugin::freezeNames(), [mup] { return mup->freezeMode.get(); }, [mup, um] (int i) { mup->freezeMode.setValue (i, um); }));
                add (toggle ("Infinite", [mup] { return mup->infinite.get(); }, [mup, um] (bool b) { mup->infinite.setValue (b, um); }));
                add (toggle ("Drawn Envelope", [mup] { return mup->customEnv.get(); }, [mup, um] (bool b) { mup->customEnv.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Color" || hs->name == "Tail Tilt" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto mqp = dynamic_cast<SqMasqueradePlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Mode", SqMasqueradePlugin::modeNames(), [mqp] { return mqp->mode.get(); }, [mqp, um] (int i) { mqp->mode.setValue (i, um); }));
                add (toggle ("Link", [mqp] { return mqp->link.get(); }, [mqp, um] (bool b) { mqp->link.setValue (b, um); }));
                add (choice ("Drive Type", SqMasqueradePlugin::driveNames(), [mqp] { return mqp->driveVoicing.get(); }, [mqp, um] (int i) { mqp->driveVoicing.setValue (i, um); }));
                add (toggle ("Session Key", [mqp] { return mqp->autoKey.get(); }, [mqp, um] (bool b) { mqp->autoKey.setValue (b, um); }));
                add (choice ("Key", SqSessionKey::rootNames(), [mqp] { return mqp->keyRoot.get(); }, [mqp, um] (int i) { mqp->keyRoot.setValue (i, um); }));
                add (choice ("Scale", SqSessionKey::scaleNames(), [mqp] { return mqp->scale.get(); }, [mqp, um] (int i) { mqp->scale.setValue (i, um); }));
                add (choice ("Robot Note", SqMasqueradePlugin::noteNames(), [mqp] { return mqp->robotNote.get() - 24; }, [mqp, um] (int i) { mqp->robotNote.setValue (i + 24, um); }));
                add (choice ("Engine", SqMasqueradePlugin::engineNames(), [mqp] { return mqp->engine.get(); }, [mqp, um] (int i) { mqp->engine.setValue (i, um); }));
                add (choice ("Voice Range", SqMasqueradePlugin::voiceNames(), [mqp] { return mqp->voiceType.get(); }, [mqp, um] (int i) { mqp->voiceType.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Pitch" || hs->name == "Formant" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto l2p = dynamic_cast<SqLarvaFilter2Plugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const juce::String& nm, juce::StringArray names, juce::CachedValue<int>& cv, int offset = 0) { add (choice (nm, names, [&cv, offset] { return cv.get() - offset; }, [&cv, um, offset] (int i) { cv.setValue (i + offset, um); })); };
                auto tb = [&] (const juce::String& nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                ci ("Stages", { "1","2","3","4","5","6","7","8" }, l2p->stages, 1); ci ("Routing", SqLarvaFilter2Plugin::routingNames(), l2p->routing);
                juce::StringArray types; for (auto& m : SqLarvaFilter2Plugin::modelNames()) for (auto& rsp : SqLarvaFilter2Plugin::responseNames()) types.add (rsp + " " + m);
                for (int i = 0; i < 8; ++i)
                    add (choice ("Stage " + juce::String (i + 1), types, [l2p, i] { return l2p->model[(size_t) i].get() * 4 + l2p->resp[(size_t) i].get(); },
                                 [l2p, um, i] (int k) { l2p->model[(size_t) i].setValue (k / 4, um); l2p->resp[(size_t) i].setValue (k % 4, um); }));
                ci ("Mod Mode", SqWingModProps::modeNames(), l2p->wm.mode); ci ("Mod Shape", SqColor111bBase::shapeNames(), l2p->wm.shape);
                ci ("Mod Sync", SqColor111bBase::syncNames (true), l2p->wm.sync); ci ("Smoothing", SqWingModProps::smoothNames(), l2p->wm.smoothing);
                ci ("Trigger", SqWingModProps::triggerNames(), l2p->wm.trigger); ci ("Step Length", SqColor111bBase::syncNames (false), l2p->wm.stepDivision);
                ci ("Steps", { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16" }, l2p->wm.numSteps, 1);
                add (choice ("Pattern", SqTremorPlugin::patternNames(), [l2p] { return l2p->wm.pattern.get(); }, [l2p, um] (int i) { l2p->wm.steps.setValue ({}, um); l2p->wm.pattern.setValue (i, um); }));
                ci ("Analog", SqTremorPlugin::analogNames(), l2p->analog); tb ("Analog Post", l2p->analogPost); tb ("Key Input", l2p->keyOn);
                add (toggle ("Fire", [] { return false; }, [l2p] (bool) { l2p->manualTrigger = true; }));
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Input" || hs->name == "Freq Mod" || hs->name == "Res Mod" || hs->name.endsWith ("Offset") || hs->name == "Env Sensitivity") hs->bipolar = true;
            }
            else if (auto p2p = dynamic_cast<SqTwelvePhase2Plugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const juce::String& nm, juce::StringArray names, juce::CachedValue<int>& cv, int offset = 0) { add (choice (nm, names, [&cv, offset] { return cv.get() - offset; }, [&cv, um, offset] (int i) { cv.setValue (i + offset, um); })); };
                auto tb = [&] (const juce::String& nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                add (choice ("Style", SqTwelvePhase2Plugin::styleNames(), [p2p] { return p2p->style.get(); }, [p2p, um] (int i) { p2p->applyStyle (i, um); }));
                ci ("Stages", SqTwelvePhase2Plugin::stageNames(), p2p->stages, 2);
                tb ("Classic Spread", p2p->classic); ci ("Resonance Path", { "Around the chain", "Inside the chain" }, p2p->resMode); ci ("Polarity", { "Positive", "Negative" }, p2p->polarity);
                tb ("Through-Zero", p2p->tz);
                ci ("Mod Mode", SqWingModProps::modeNames(), p2p->wm.mode); ci ("Mod Shape", SqColor111bBase::shapeNames(), p2p->wm.shape);
                ci ("Mod Sync", SqColor111bBase::syncNames (true), p2p->wm.sync); ci ("Smoothing", SqWingModProps::smoothNames(), p2p->wm.smoothing);
                ci ("Trigger", SqWingModProps::triggerNames(), p2p->wm.trigger); ci ("Step Length", SqColor111bBase::syncNames (false), p2p->wm.stepDivision);
                ci ("Steps", { "1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16" }, p2p->wm.numSteps, 1);
                add (choice ("Pattern", SqTremorPlugin::patternNames(), [p2p] { return p2p->wm.pattern.get(); }, [p2p, um] (int i) { p2p->wm.steps.setValue ({}, um); p2p->wm.pattern.setValue (i, um); }));
                ci ("Analog", SqTremorPlugin::analogNames(), p2p->analog); tb ("Analog Post", p2p->analogPost); tb ("Key Input", p2p->keyOn);
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Env Sensitivity") hs->bipolar = true;
            }
            else if (auto hrp = dynamic_cast<SqHiveRackPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                const auto slotNames = SqHiveRackPlugin::slotChoices();
                for (int i = 0; i < SqHiveRackPlugin::numSlots; ++i)
                    add (choice ("Slot " + juce::String (i + 1), slotNames, [hrp, i, slotNames] { const auto nm = hrp->slotInfo (i).name; return nm.isEmpty() ? 0 : juce::jmax (0, slotNames.indexOf (nm)); },
                                 [hrp, i, slotNames] (int k) { hrp->setSlot (i, slotNames[k]); }));
                juce::StringArray chainNames { "(custom)" }; for (auto& c : HiveChains::chains()) chainNames.add (c.name);
                add (choice ("Chain", chainNames, [hrp] { return hrp->loadedChain + 1; }, [hrp, um] (int k) { if (k > 0) { hrp->loadChain (k - 1); hrp->chain.setValue (k - 1, um); } }));
                add (choice ("Routing", SqHiveRackPlugin::routingNames(), [hrp] { return hrp->routing.get(); }, [hrp, um] (int i) { hrp->routing.setValue (i, um); }));
                add (choice ("Open Slot", { "-", "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6", "Slot 7", "Slot 8" }, [] { return 0; },
                             [hrp] (int k) { if (k > 0) if (auto* sp = hrp->slotPlugin (k - 1)) sp->showWindowExplicitly(); }));
                add (toggle ("Recycle Sync", [hrp] { return hrp->syncOn.get(); }, [hrp, um] (bool b) { hrp->syncOn.setValue (b, um); }));
                add (choice ("Recycle Division", SqColor111bBase::syncNames (false), [hrp] { return hrp->syncDiv.get(); }, [hrp, um] (int i) { hrp->syncDiv.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Input" || hs->name == "Lane Balance") hs->bipolar = true;
            }
            else if (auto svp = dynamic_cast<SqSirenVerbPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                auto ci = [&] (const juce::String& nm, juce::StringArray names, juce::CachedValue<int>& cv) { add (choice (nm, names, [&cv] { return cv.get(); }, [&cv, um] (int i) { cv.setValue (i, um); })); };
                auto tb = [&] (const juce::String& nm, juce::CachedValue<bool>& cv) { add (toggle (nm, [&cv] { return cv.get(); }, [&cv, um] (bool b) { cv.setValue (b, um); })); };
                ci ("Algorithm", SqSirenVerbPlugin::algoNames(), svp->algo); ci ("Echo", SqSirenVerbPlugin::delayNames(), svp->delayMode);
                ci ("Echo Sync", SqColor111bBase::syncNames (true), svp->delaySync); ci ("Shimmer Pitch", SqSirenVerbPlugin::shimmerNames(), svp->shimmerPitch);
                tb ("Reverse", svp->reverseOn); tb ("Auto-EQ", svp->autoEqOn); tb ("Mix Lock", svp->mixLockOn);
                juce::StringArray vibes { "-" }; vibes.addArray (SqSirenVerbPlugin::vibeNames());
                add (choice ("Assist", vibes, [] { return 0; }, [svp, um] (int i) { if (i > 0) { um->beginNewTransaction ("Siren Verb: Assist"); svp->assist (i - 1, um); } }));
                add (toggle ("Dice", [] { return false; }, [svp, um] (bool) { um->beginNewTransaction ("Siren Verb: Dice"); svp->dice (um); }));
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Pre Pitch" || hs->name == "Throat" || hs->name == "Tone") hs->bipolar = true;
            }
            else if (auto mcp = dynamic_cast<SqMicClosetPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Source Mic", SqMicClosetPlugin::micNames(), [mcp] { return mcp->source.get(); }, [mcp, um] (int i) { mcp->source.setValue (i, um); }));
                add (choice ("Target Mic", SqMicClosetPlugin::micNames(), [mcp] { return mcp->target.get(); }, [mcp, um] (int i) { mcp->target.setValue (i, um); }));
                add (choice ("Pattern", SqMicClosetPlugin::patternNames(), [mcp] { return mcp->pattern.get(); }, [mcp, um] (int i) { mcp->pattern.setValue (i, um); }));
                add (toggle ("Source Proximity", [mcp] { return mcp->sourceProxOn.get(); }, [mcp, um] (bool b) { mcp->sourceProxOn.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto lxp = dynamic_cast<SqLarynxPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (toggle ("Level Match", [lxp] { return lxp->levelMatchOn.get(); }, [lxp, um] (bool b) { lxp->levelMatchOn.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Glottal Hardness" || juce::StringArray { "Glottis", "Lower Throat", "Upper Throat", "Pharynx", "Back Tongue", "Mid Tongue", "Front Tongue", "Lips" }.contains (hs->name)) hs->bipolar = true;
            }
            else if (dynamic_cast<SqExhalePlugin*> (&plugin) != nullptr)
            {
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Breath Tone" || hs->name == "Body") hs->bipolar = true;
            }
            else if (auto hcp = dynamic_cast<SqHiveChoirPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                for (int k = 0; k < 4; ++k)
                    add (choice ("Voice " + juce::String (k + 1) + " Mode", SqHiveChoirPlugin::modeNames(), [hcp, k] { return hcp->mode[(size_t) k].get(); }, [hcp, um, k] (int i) { hcp->mode[(size_t) k].setValue (i, um); }));
                add (choice ("Engine", SqHiveChoirPlugin::engineNames(), [hcp] { return hcp->engine.get(); }, [hcp, um] (int i) { hcp->engine.setValue (i, um); }));
                add (choice ("Voice Range", SqHiveChoirPlugin::voiceNames(), [hcp] { return hcp->voiceType.get(); }, [hcp, um] (int i) { hcp->voiceType.setValue (i, um); }));
                add (toggle ("Session Key", [hcp] { return hcp->autoKey.get(); }, [hcp, um] (bool b) { hcp->autoKey.setValue (b, um); }));
                add (choice ("Key", SqSessionKey::rootNames(), [hcp] { return hcp->keyRoot.get(); }, [hcp, um] (int i) { hcp->keyRoot.setValue (i, um); }));
                add (choice ("Scale", SqSessionKey::scaleNames(), [hcp] { return hcp->scale.get(); }, [hcp, um] (int i) { hcp->scale.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name.endsWith ("Interval") || hs->name.endsWith ("Pan") || hs->name.endsWith ("Throat") || hs->name == "Output") hs->bipolar = true;
                for (auto& hs : handleStore) if (hs->name.startsWith ("Voice ") && hs->name != "Voice Range") hs->label = hs->name.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);   // the group says which voice
            }
            else if (auto dwp = dynamic_cast<SqDoubleWing2Plugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Doubles", SqDoubleWing2Plugin::countNames(), [dwp] { return dwp->doubles.get() - 1; }, [dwp, um] (int i) { dwp->doubles.setValue (i + 1, um); }));
                add (choice ("Voice Range", SqHiveChoirPlugin::voiceNames(), [dwp] { return dwp->voiceRange.get(); }, [dwp, um] (int i) { dwp->voiceRange.setValue (i, um); }));
                add (toggle ("Straight Notes Only", [dwp] { return dwp->vibStraight.get(); }, [dwp, um] (bool b) { dwp->vibStraight.setValue (b, um); }));
                handleStore.back()->label = "Straight Only";
                add (toggle ("New Voices", [] { return false; }, [dwp, um] (bool) { dwp->seed.setValue (juce::Random::getSystemRandom().nextInt (100000) + 1, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto mup = dynamic_cast<SqMutantPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Engine", SqMutantPlugin::engineNames(), [mup] { return mup->engine.get(); }, [mup, um] (int i) { mup->engine.setValue (i, um); }));
                add (choice ("Voice Range", SqMutantPlugin::voiceNames(), [mup] { return mup->voiceType.get(); }, [mup, um] (int i) { mup->voiceType.setValue (i, um); }));
                add (choice ("Drive Type", SqMutantPlugin::driveNames(), [mup] { return mup->driveVoicing.get(); }, [mup, um] (int i) { mup->driveVoicing.setValue (i, um); }));
                add (choice ("Alien Rate", SqColor111bBase::syncNames (false), [mup] { return mup->alienDiv.get(); }, [mup, um] (int i) { mup->alienDiv.setValue (i, um); }));
                add (choice ("Stutter Length", SqColor111bBase::syncNames (false), [mup] { return mup->stutterDiv.get(); }, [mup, um] (int i) { mup->stutterDiv.setValue (i, um); }));
                add (choice ("Repeats", SqMutantPlugin::repeatNames(), [mup] { return mup->repeats.get(); }, [mup, um] (int i) { mup->repeats.setValue (i, um); }));
                add (toggle ("New Mutation", [] { return false; }, [mup, um] (bool) { mup->seed.setValue (juce::Random::getSystemRandom().nextInt (100000) + 1, um); }));
                for (auto& hs : handleStore) if (hs->name == "Pitch" || hs->name == "Throat" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto mwp = dynamic_cast<SqMawPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (toggle ("Fast", [mwp] { return mwp->fast.get(); }, [mwp, um] (bool b) { mwp->fast.setValue (b, um); }));
                add (toggle ("Key Input", [mwp] { return mwp->keyOn.get(); }, [mwp, um] (bool b) { mwp->keyOn.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto fpl = dynamic_cast<SqFoilPlatePlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Plate", SqFoilPlatePlugin::archetypeNames(), [fpl] { return fpl->plate.get(); }, [fpl, um] (int i) { fpl->plate.setValue (i, um); }));
                add (choice ("Input", SqFoilPlatePlugin::inputNames(), [fpl] { return fpl->input.get(); }, [fpl, um] (int i) { fpl->input.setValue (i, um); }));
                add (choice ("Low Slope", SqFoilPlatePlugin::slopeNames(), [fpl] { return fpl->lowSlope.get(); }, [fpl, um] (int i) { fpl->lowSlope.setValue (i, um); }));
                add (choice ("High Slope", SqFoilPlatePlugin::slopeNames(), [fpl] { return fpl->highSlope.get(); }, [fpl, um] (int i) { fpl->highSlope.setValue (i, um); }));
                add (toggle ("Freeze", [fpl] { return fpl->freeze.get(); }, [fpl, um] (bool b) { fpl->freeze.setValue (b, um); }));
                add (toggle ("Darkening", [fpl] { return fpl->darkening.get(); }, [fpl, um] (bool b) { fpl->darkening.setValue (b, um); }));
                add (toggle ("Mono In", [fpl] { return fpl->monoIn.get(); }, [fpl, um] (bool b) { fpl->monoIn.setValue (b, um); }));
                add (toggle ("Key Input", [fpl] { return fpl->keyOn.get(); }, [fpl, um] (bool b) { fpl->keyOn.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Balance" || hs->name == "Low EQ" || hs->name == "High EQ" || hs->name == "Auto Decay" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto we = dynamic_cast<SqWingbeatPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Mode", SqWingbeatPlugin::modeNames(), [we] { return we->mode.get(); }, [we, um] (int i) { we->mode.setValue (i, um); }));
                add (choice ("Style", SqWingbeatPlugin::styleNames(), [we] { return we->style.get(); }, [we, um] (int i) { we->style.setValue (i, um); }));
                add (choice ("Sync L", SqWingbeatPlugin::syncNames(), [we] { return we->syncL.get(); }, [we, um] (int i) { we->syncL.setValue (i, um); }));
                add (choice ("Sync R", SqWingbeatPlugin::syncNames(), [we] { return we->syncR.get(); }, [we, um] (int i) { we->syncR.setValue (i, um); }));
                add (choice ("Time Change", SqWingbeatPlugin::changeNames(), [we] { return we->timeChange.get(); }, [we, um] (int i) { we->timeChange.setValue (i, um); }));
                add (choice ("Heads", SqWingbeatPlugin::headNames(), [we] { return we->heads.get() - 1; }, [we, um] (int i) { we->heads.setValue (i + 1, um); }));
                add (choice ("Limiter", SqWingbeatPlugin::limitNames(), [we] { return we->limiter.get(); }, [we, um] (int i) { we->limiter.setValue (i, um); }));
                add (choice ("Pattern", SqWingbeatPlugin::patternNames(), [we] { return we->pattern.get(); }, [we, um] (int i) { we->pattern.setValue (i, um); }));
                add (toggle ("Freeze", [we] { return we->freeze.get(); }, [we, um] (bool b) { we->freeze.setValue (b, um); }));
                add (toggle ("Prime", [we] { return we->prime.get(); }, [we, um] (bool b) { we->prime.setValue (b, um); }));
                add (toggle ("Key Input", [we] { return we->keyOn.get(); }, [we, um] (bool b) { we->keyOn.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Groove" || hs->name == "Feel" || hs->name == "Output" || hs->name == "Input") hs->bipolar = true;
            }
            else if (auto wj = dynamic_cast<SqWingbeatJrPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Style", SqWingbeatJrPlugin::styleNames(), [wj] { return wj->style.get(); }, [wj, um] (int i) { wj->style.setValue (i, um); }));
                add (choice ("Mode", SqWingbeatJrPlugin::modeNames(), [wj] { return wj->mode.get(); }, [wj, um] (int i) { wj->mode.setValue (i, um); }));
                add (choice ("Sync", SqWingbeatPlugin::syncNames(), [wj] { return wj->sync.get(); }, [wj, um] (int i) { wj->sync.setValue (i, um); }));
                add (choice ("Change", SqWingbeatJrPlugin::changeNames(), [wj] { return wj->change.get(); }, [wj, um] (int i) { wj->change.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto cd = dynamic_cast<SqCinderPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Model", SqCinderPlugin::modelNames(), [cd] { return cd->modelA.get(); }, [cd, um] (int i) { cd->modelA.setValue (i, um); }));
                add (choice ("Model B", SqCinderPlugin::modelNames(), [cd] { return cd->modelB.get(); }, [cd, um] (int i) { cd->modelB.setValue (i, um); }));
                add (toggle ("Punish", [cd] { return cd->punish.get(); }, [cd, um] (bool b) { cd->punish.setValue (b, um); }));
                add (toggle ("Steep", [cd] { return cd->steep.get(); }, [cd, um] (bool b) { cd->steep.setValue (b, um); }));
                add (toggle ("Auto Gain", [cd] { return cd->autoGain.get(); }, [cd, um] (bool b) { cd->autoGain.setValue (b, um); }));
                add (choice ("Mid/Side", SqCinderPlugin::msNames(), [cd] { return cd->msMode.get(); }, [cd, um] (int i) { cd->msMode.setValue (i, um); }));
                add (choice ("Oversampling", SqCinderPlugin::osNames(), [cd] { return cd->oversample.get(); }, [cd, um] (int i) { cd->oversample.setValue (i, um); }));
                add (toggle ("Linear Phase", [cd] { return cd->linearPhase.get(); }, [cd, um] (bool b) { cd->linearPhase.setValue (b, um); }));
                add (toggle ("Delta", [cd] { return cd->delta.get(); }, [cd, um] (bool b) { cd->delta.setValue (b, um); }));
                add (toggle ("Key Input", [cd] { return cd->keyOn.get(); }, [cd, um] (bool b) { cd->keyOn.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output" || hs->name == "Tone" || hs->name == "Bias") hs->bipolar = true;
            }
            else if (auto ld = dynamic_cast<SqLadderPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Poles", { "2-POLE", "4-POLE" }, [ld] { return ld->poles.get(); }, [ld, um] (int i) { ld->poles.setValue (i, um); }));
                add (toggle ("Fast Envelope", [ld] { return ld->envFast.get(); }, [ld, um] (bool b) { ld->envFast.setValue (b, um); }));
                for (auto& hs : handleStore) if (hs->name == "Env Amount" || hs->name == "Output") hs->bipolar = true;
            }
            else if (auto rg = dynamic_cast<SqRingPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("LFO Shape", { "SINE", "SQUARE" }, [rg] { return rg->lfoShape.get(); }, [rg, um] (int i) { rg->lfoShape.setValue (i, um); }));
                for (auto& hs : handleStore) if (hs->name == "Output") hs->bipolar = true;
            }
            else if (auto p12 = dynamic_cast<SqPhase12Plugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Stages", { "4", "6", "8", "12" }, [p12] { return p12->stages.get(); }, [p12, um] (int i) { p12->stages.setValue (i, um); }));
            }
            else if (auto bbd = dynamic_cast<SqBucketPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Sync", { "FREE", "1/16", "1/8", "1/8 D", "1/4", "1/2" }, [bbd] { return bbd->sync.get(); }, [bbd, um] (int i) { bbd->sync.setValue (i, um); }));
            }
            else if (auto bl = dynamic_cast<SqBlossomPlugin*> (&plugin))
            {
                auto um = &plugin.edit.getUndoManager();
                add (choice ("Exciter", SqBlossomPlugin::exciterNames(), [bl] { return bl->exciterType.get(); }, [bl, um] (int i) { bl->exciterType.setValue (i, um); }));
                add (choice ("Body Type", SqBlossomPlugin::bodyNames(), [bl] { return bl->bodyType.get(); }, [bl, um] (int i) { bl->bodyType.setValue (i, um); }));
            }
            else if (auto c = dynamic_cast<te::ChorusPlugin*> (&plugin))
            {
                add (fromCached ("Depth", c->depthMs, 0.1f, 20.0f, 0.6f, " ms"));
                add (fromCached ("Speed", c->speedHz, 0.05f, 10.0f, 0.4f, " Hz", 2));
                add (fromCached ("Width", c->width, 0.0f, 1.0f, 1.0f, "", 2));
                add (fromCached ("Mix", c->mixProportion, 0.0f, 1.0f, 1.0f, "", 2));
            }
            else if (auto ph = dynamic_cast<te::PhaserPlugin*> (&plugin))
            {
                add (fromCached ("Depth", ph->depth, 0.0f, 10.0f, 1.0f, "", 1));
                add (fromCached ("Rate", ph->rate, 0.02f, 10.0f, 0.35f, " Hz", 2));
                add (fromCached ("Feedback", ph->feedbackGain, 0.0f, 0.99f, 1.0f, "", 2));
            }
            else if (auto d = dynamic_cast<te::DelayPlugin*> (&plugin))
            {
                add (fromCachedInt ("Time", d->lengthMs, 1, 2000, 0.5f, " ms"));
                add (choice ("Sync", { "free", "1/16", "1/8", "1/8.", "1/4", "1/2" },
                             [this] { return (int) plugin.state.getProperty ("sqmSync", 0); },
                             [this, d] (int i)
                             {
                                 plugin.state.setProperty ("sqmSync", i, nullptr);
                                 if (i > 0)
                                 {
                                     const double beats[] = { 0, 0.25, 0.5, 0.75, 1.0, 2.0 };
                                     auto bpm = plugin.edit.tempoSequence.getTempo (0)->getBpm();
                                     d->lengthMs = juce::jlimit (1, 2000, juce::roundToInt (60000.0 / bpm * beats[i]));
                                 }
                             }));
            }
            else if (auto lp = dynamic_cast<te::LowPassPlugin*> (&plugin))
            {
                add (choice ("Mode", { "LOW-PASS", "HIGH-PASS" }, [lp] { return lp->isLowPass() ? 0 : 1; }, [lp] (int i) { lp->mode = i == 0 ? "lowpass" : "highpass"; }));
            }
            else if (auto ps = dynamic_cast<te::PitchShiftPlugin*> (&plugin))
            {
                juce::StringArray names;
                std::vector<int> modes;
                for (auto& m : te::TimeStretcher::getPossibleModes (plugin.edit.engine, true)) { names.add (m); modes.push_back ((int) te::TimeStretcher::getModeFromName (plugin.edit.engine, m)); }
                if (names.isEmpty()) { names.add ("default"); modes.push_back ((int) te::TimeStretcher::defaultMode); }
                add (choice ("Algorithm", names, [ps, modes] { for (size_t i = 0; i < modes.size(); ++i) if (modes[i] == ps->mode.get()) return (int) i; return 0; },
                             [ps, modes] (int i) { if (i >= 0 && i < (int) modes.size()) ps->mode = modes[(size_t) i]; }));
            }
            else if (auto c = dynamic_cast<te::CompressorPlugin*> (&plugin))
            {
                add (toggle ("External sidechain", [c] { return c->useSidechainTrigger.get(); }, [c] (bool b) { c->useSidechainTrigger = b; }));
            }
            else if (auto fo = dynamic_cast<te::FourOscPlugin*> (&plugin))
            {
                juce::StringArray shapes { "off", "sine", "tri", "saw", "saw-", "square", "noise" };
                for (int i = 1; i <= 4; ++i)
                {
                    auto id = "waveShape" + juce::String (i);
                    add (choice ("Wave " + juce::String (i), shapes, [this, id] { return (int) plugin.state.getProperty (id, 0); }, [this, id] (int v) { plugin.state.setProperty (id, v, &plugin.edit.getUndoManager()); }));
                    auto vid = "voices" + juce::String (i);
                    auto h = std::make_unique<Handle>();
                    h->name = "Unison " + juce::String (i); h->label = "Unison";
                    h->get = [this, vid] { return ((float) (int) plugin.state.getProperty (vid, 1) - 1.0f) / 7.0f; };
                    h->set = [this, vid] (float v) { plugin.state.setProperty (vid, 1 + juce::roundToInt (v * 7.0f), &plugin.edit.getUndoManager()); };
                    h->text = [this, vid] { return juce::String ((int) plugin.state.getProperty (vid, 1)) + " voice" + ((int) plugin.state.getProperty (vid, 1) == 1 ? "" : "s"); };
                    add (std::move (h));
                }
                for (int i = 1; i <= 2; ++i)
                {
                    auto id = "lfoWaveShape" + juce::String (i);
                    add (choice ("LFO Wave " + juce::String (i), { "off", "sine", "tri", "saw", "saw-", "square", "random" }, [this, id] { return (int) plugin.state.getProperty (id, 1); }, [this, id] (int v) { plugin.state.setProperty (id, v, &plugin.edit.getUndoManager()); }));
                }
                add (choice ("Filter Type", { "LP", "HP", "BP" }, [this] { return (int) plugin.state.getProperty ("filterType", 0); }, [this] (int v) { plugin.state.setProperty ("filterType", v, &plugin.edit.getUndoManager()); }));
                add (choice ("Slope", { "12 dB", "24 dB" }, [this] { return (int) plugin.state.getProperty ("filterSlope", 12) == 24 ? 1 : 0; }, [this] (int v) { plugin.state.setProperty ("filterSlope", v == 1 ? 24 : 12, &plugin.edit.getUndoManager()); }));
                add (choice ("Voice Mode", { "MONO", "LEGATO", "POLY" }, [this] { return (int) plugin.state.getProperty ("voiceMode", 2); }, [this] (int v) { plugin.state.setProperty ("voiceMode", v, &plugin.edit.getUndoManager()); }));
                for (auto* fx : { "distortionOn", "reverbOn", "delayOn", "chorusOn" })
                {
                    juce::String id (fx);
                    add (toggle (id.upToFirstOccurrenceOf ("On", false, false).toUpperCase(), [this, id] { return (bool) plugin.state.getProperty (id, false); }, [this, id] (bool b) { plugin.state.setProperty (id, b, &plugin.edit.getUndoManager()); }));
                }
                juce::ignoreUnused (fo);
            }
        }

        void buildViz()
        {
            switch (spec.family)
            {
                case Family::eq:        viz = std::make_unique<EQViz> (handles, plugin, accent); eqNodes = std::make_unique<EQNodes> (handles, plugin); eqNodes->onChange = [this] { repaintControls(); }; break;
                case Family::filter:    viz = std::make_unique<FilterViz> (handles, plugin, accent); break;
                case Family::tilt:      viz = std::make_unique<TiltViz> (handles, plugin, accent); break;
                case Family::air:       viz = std::make_unique<AirViz> (handles, plugin, accent); break;
                case Family::sub:       viz = std::make_unique<SubViz> (handles, plugin, accent); break;
                case Family::dyn:       viz = std::make_unique<DynViz> (handles, plugin, accent); break;
                case Family::gate:      viz = std::make_unique<GateViz> (handles, plugin, accent); break;
                case Family::deess:     viz = std::make_unique<DeEssViz> (handles, plugin, accent); break;
                case Family::transient: viz = std::make_unique<TransientViz> (handles, plugin, accent); break;
                case Family::sat:       viz = std::make_unique<SatViz> (handles, plugin, accent); break;
                case Family::tape:      viz = std::make_unique<TapeViz> (handles, plugin, accent); break;
                case Family::console:   viz = std::make_unique<ConsoleViz> (handles, plugin, accent); break;
                case Family::vinyl:     viz = std::make_unique<VinylViz> (handles, plugin, accent); break;
                case Family::bits:      viz = std::make_unique<BitsViz> (handles, plugin, accent); break;
                case Family::reverb:    viz = std::make_unique<ReverbViz> (handles, plugin, accent); break;
                case Family::delay:     viz = std::make_unique<DelayViz> (handles, plugin, accent); break;
                case Family::ir:        viz = std::make_unique<IRViz> (handles, plugin, accent); break;
                case Family::lfo:       viz = std::make_unique<LfoViz> (handles, plugin, accent); break;
                case Family::pitch:     viz = std::make_unique<PitchViz> (handles, plugin, accent); break;
                case Family::width:     viz = std::make_unique<WidthViz> (handles, plugin, accent); break;
                case Family::dither:    viz = std::make_unique<DitherViz> (handles, plugin, accent); break;
                case Family::trim:      viz = std::make_unique<TrimViz> (handles, plugin, accent); break;
                case Family::midi:      viz = std::make_unique<MidiViz> (handles, plugin, accent); break;
                case Family::synth:     viz = std::make_unique<SynthViz> (handles, plugin, accent); break;
                case Family::sampler:   viz = std::make_unique<SamplerViz> (handles, plugin, accent); break;
                case Family::eight08:   viz = std::make_unique<EightOhEightViz> (handles, plugin, accent); break;
                case Family::tune:      { auto tv = std::make_unique<TuneViz> (handles, plugin, accent); tv->onChange = [this] { repaintControls(); }; viz = std::move (tv); break; }
                case Family::tunefx:    { auto fv = std::make_unique<TuneFxViz> (handles, plugin, accent); fv->onChange = [this] { repaintControls(); }; viz = std::move (fv); break; }
                case Family::apexdyn:   viz = std::make_unique<ApexDynViz> (handles, plugin, accent); break;
                case Family::apexlim:   viz = std::make_unique<ApexLimViz> (handles, plugin, accent); break;
                case Family::apexmb:    { auto mv = std::make_unique<ApexMBViz> (handles, plugin, accent); mv->onChange = [this] { repaintControls(); }; viz = std::move (mv); break; }
                case Family::apexsub:   viz = std::make_unique<ApexSubViz> (handles, plugin, accent); break;
                case Family::aurora:    viz = std::make_unique<AuroraViz> (handles, plugin, accent); break;
                case Family::opto:      viz = std::make_unique<OptoViz> (handles, plugin, accent); break;
                case Family::pulse:     viz = std::make_unique<PulseViz> (handles, plugin, accent); break;
                case Family::oilcan:    viz = std::make_unique<OilCanViz> (handles, plugin, accent); break;
                case Family::ladder:    viz = std::make_unique<LadderViz> (handles, plugin, accent); break;
                case Family::cinder:    viz = std::make_unique<CinderViz> (handles, plugin, accent); break;
                case Family::wingbeat:  viz = std::make_unique<WingbeatViz> (handles, plugin, accent); break;
                case Family::foilplate: viz = std::make_unique<FoilPlateViz> (handles, plugin, accent); break;
                case Family::radiant:   viz = std::make_unique<RadiantViz> (handles, plugin, accent); break;
                case Family::maw:       viz = std::make_unique<MawViz> (handles, plugin, accent); break;
                case Family::mothkey:   viz = std::make_unique<MothKeyViz> (handles, plugin, accent); break;
                case Family::sirencomp: viz = std::make_unique<SirenCompViz> (handles, plugin, accent); break;
                case Family::sireneq:   viz = std::make_unique<SirenEqViz> (handles, plugin, accent); break;
                case Family::hushdeess2: viz = std::make_unique<HushViz> (handles, plugin, accent); break;
                case Family::vellum:    viz = std::make_unique<VellumViz> (handles, plugin, accent); break;
                case Family::tremor:    viz = std::make_unique<TremorViz> (handles, plugin, accent); break;
                case Family::orbitpan:  viz = std::make_unique<OrbitViz> (handles, plugin, accent); break;
                case Family::mirrorshift: viz = std::make_unique<MirrorViz> (handles, plugin, accent); break;
                case Family::shutter:   viz = std::make_unique<ShutterViz> (handles, plugin, accent); break;
                case Family::shard:     viz = std::make_unique<ShardViz> (handles, plugin, accent); break;
                case Family::murmuration: viz = std::make_unique<MurmurationViz> (handles, plugin, accent); break;
                case Family::masquerade: viz = std::make_unique<MasqueradeViz> (handles, plugin, accent); break;
                case Family::larva2: viz = std::make_unique<Larva2Viz> (handles, plugin, accent); break;
                case Family::phase2: viz = std::make_unique<Phase2Viz> (handles, plugin, accent); break;
                case Family::hiverack: viz = std::make_unique<HiveRackViz> (handles, plugin, accent); break;
                case Family::sirenverb: viz = std::make_unique<SirenVerbViz> (handles, plugin, accent); break;
                case Family::miccloset: viz = std::make_unique<MicClosetViz> (handles, plugin, accent); break;
                case Family::larynx: viz = std::make_unique<LarynxViz> (handles, plugin, accent); break;
                case Family::exhale: viz = std::make_unique<ExhaleViz> (handles, plugin, accent); break;
                case Family::hivechoir: viz = std::make_unique<HiveChoirViz> (handles, plugin, accent); break;
                case Family::doublewing2: viz = std::make_unique<DoubleWing2Viz> (handles, plugin, accent); break;
                case Family::mutant: viz = std::make_unique<MutantViz> (handles, plugin, accent); break;
                case Family::ring:      viz = std::make_unique<RingViz> (handles, plugin, accent); break;
                case Family::phase12:   viz = std::make_unique<Phase12Viz> (handles, plugin, accent); break;
                case Family::bbd:       viz = std::make_unique<BucketViz> (handles, plugin, accent); break;
                case Family::blossom:   { auto bv = std::make_unique<BlossomViz> (handles, plugin, accent); bv->onPad = [this] { repaintControls(); }; viz = std::move (bv); break; }
                case Family::mothsynth2: viz = std::make_unique<Mothsynth2Viz> (handles, plugin, accent); break;
                case Family::venom:     viz = std::make_unique<VenomViz> (handles, plugin, accent); break;
                case Family::simmoth:   viz = std::make_unique<SimMothViz> (handles, plugin, accent); break;
                case Family::minimoth:  viz = std::make_unique<MiniMothViz> (handles, plugin, accent); break;
                case Family::thump:     viz = std::make_unique<ThumpViz> (handles, plugin, accent); break;
                case Family::dial10:    viz = std::make_unique<Dial10Viz> (handles, plugin, accent); break;
                case Family::hivepads:  { auto hv = std::make_unique<HivePadsViz> (handles, plugin, accent); hv->onPad = [this] { repaintControls(); };
                                          hv->onLoadKit = [this] (int kit)
                                          {
                                              if (auto hp = dynamic_cast<SqHivePadsPlugin*> (&plugin)) if (loadKitHook)
                                              {
                                                  auto files = loadKitHook (kit); juce::StringArray names; juce::Array<int> notes; juce::Array<float> gains;
                                                  for (int i = 0; i < DrumKit::numSounds; ++i) { names.add (DrumKit::sounds[i].name); notes.add (DrumKit::sounds[i].note); gains.add (DrumKit::gainsDb[i]); }
                                                  hp->loadKitFiles (files, names, notes, gains, hp->currentBank.get() * 16);
                                                  plugin.state.setProperty ("sqmPadsKit", DrumKit::kits[kit].name, nullptr);
                                              }
                                          };
                                          viz = std::move (hv); break; }
                case Family::hexwing:   viz = std::make_unique<HexwingViz> (handles, plugin, accent); break;
                case Family::toxin:     viz = std::make_unique<ToxinViz> (handles, plugin, accent); break;
                case Family::lantern:   viz = std::make_unique<LanternViz> (handles, plugin, accent); break;
                case Family::lanternexo: viz = std::make_unique<LanternExoViz> (handles, plugin, accent); break;
                case Family::larvabass: viz = std::make_unique<LarvaViz> (handles, plugin, accent); break;
                case Family::thornbass: viz = std::make_unique<ThornViz> (handles, plugin, accent); break;
                case Family::carapace:  viz = std::make_unique<CarapaceViz> (handles, plugin, accent); break;
                case Family::livedrum:  viz = std::make_unique<LiveDrumViz> (handles, plugin, accent); break;
                case Family::drumpad:   viz = std::make_unique<DrumPadViz> (handles, plugin, accent); break;
                case Family::thumplive: viz = std::make_unique<ThumpLiveViz> (handles, plugin, accent); break;
                case Family::timeweaver: { auto tv = std::make_unique<TimeWeaverViz> (handles, plugin, accent); tv->onChange = [this] { repaintControls(); }; viz = std::move (tv); break; }
                case Family::larvafilter: viz = std::make_unique<LarvaFilterViz> (handles, plugin, accent); break;
                case Family::triad:   viz = std::make_unique<TriadViz> (handles, plugin, accent); break;
                case Family::chrysalis2: viz = std::make_unique<Chrysalis2Viz> (handles, plugin, accent); break;
                case Family::mandible:  viz = std::make_unique<MandibleViz> (handles, plugin, accent); break;
                case Family::pollen:    { auto pv = std::make_unique<PollenViz> (handles, plugin, accent); pv->onChange = [this] { repaintControls(); }; viz = std::move (pv); break; }
                case Family::generic:   break;
            }
            if (viz) addAndMakeVisible (*viz);
            if (eqNodes) addAndMakeVisible (*eqNodes);
        }

        juce::Component* makeControl (Handle& h, bool big)
        {
            std::unique_ptr<juce::Component> c;
            if (! h.choices.isEmpty()) { auto cb = std::make_unique<ChoiceBar> (h, accent); cb->onChange = [this] { repaintControls(); }; c = std::move (cb); }
            else if (h.toggle)         { auto t = std::make_unique<ToggleLamp> (h, accent); t->onChange = [this] { repaintControls(); }; c = std::move (t); }
            else                       { auto k = std::make_unique<Knob> (h, accent, spec.knob, big); k->onChange = [this] { if (viz) viz->repaint(); }; c = std::move (k); }
            addAndMakeVisible (*c);
            controls.push_back (std::move (c));
            return controls.back().get();
        }

        void buildControls()
        {
            if (spec.family == Family::synth)   { buildSynthControls(); return; }
            if (spec.family == Family::sampler) { buildSamplerControls(); return; }

            std::set<juce::String> used;
            for (auto& grp : spec.groups)
            {
                GroupBox gb { grp.title, {}, {}, grp.big, grp.cols };
                for (auto& pn : grp.params)
                {
                    if (pn == "Match Target" || pn == "Reset Meter")
                    {
                        auto b = std::make_unique<juce::TextButton> (pn == "Match Target" ? "MATCH TARGET" : "RESET METER");
                        b->setTooltip (pn == "Match Target" ? "Play the song, then press: sets Input so the integrated loudness lands on the target and the ceiling to its true-peak limit"
                                                            : "Start the loudness measurement again");
                        const bool match = pn == "Match Target";
                        b->onClick = [this, match]
                        {
                            if (auto li = dynamic_cast<SqApexLimiterPlugin*> (&plugin))
                            {
                                if (match) { auto msg = li->matchTarget(); if (auto lv = dynamic_cast<ApexLimViz*> (viz.get())) lv->status = msg; }
                                else li->resetRequest = true;
                                repaintControls();
                            }
                        };
                        addAndMakeVisible (*b); controls.push_back (std::move (b)); gb.items.push_back (controls.back().get());
                        continue;
                    }
                    if (pn == "Graph Mode" || pn == "Detect Key")
                    {
                        auto b = std::make_unique<juce::TextButton> (pn == "Graph Mode" ? "GRAPH MODE..." : "DETECT KEY");
                        b->setTooltip (pn == "Graph Mode" ? "Open the pitch graph: see every note, drag notes to new pitches, per-note retune, vibrato, formant and drift"
                                                          : "Find the session key from its MIDI and this track's singing, then set it here (and as the session key)");
                        const bool graph = pn == "Graph Mode";
                        b->onClick = [this, graph] { if (graph) { if (Hooks::openTuneGraph) Hooks::openTuneGraph (plugin); } else if (Hooks::detectKey) { Hooks::detectKey (plugin); repaintControls(); } };
                        addAndMakeVisible (*b); controls.push_back (std::move (b)); gb.items.push_back (controls.back().get());
                        continue;
                    }
                    if (pn == "Load IR")
                    {
                        auto b = std::make_unique<juce::TextButton> ("LOAD IR...");
                        b->onClick = [this] { chooseIR(); };
                        addAndMakeVisible (*b); controls.push_back (std::move (b)); gb.items.push_back (controls.back().get());
                        continue;
                    }
                    auto it = handles.find (pn);
                    if (it == handles.end()) continue;
                    used.insert (pn);
                    gb.items.push_back (makeControl (*it->second, grp.big));
                }
                if (! gb.items.empty()) groupBoxes.push_back (gb);
            }
            // anything the spec didn't mention: AirWindows Dry/Wet -> a MIX group, others -> MORE
            GroupBox mix { "MIX", {}, {}, false, 0 }, more { "MORE", {}, {}, false, 0 };
            for (auto& h : handleStore)
            {
                if (used.count (h->name)) continue;
                bool hidden = false;
                for (auto& hp : spec.hiddenPrefixes) hidden = hidden || h->name.startsWith (hp);
                if (hidden) continue;
                if (h->name == "Dry Level" || h->name == "Wet Level") mix.items.push_back (makeControl (*h, false));
                else more.items.push_back (makeControl (*h, false));
            }
            if (! more.items.empty()) groupBoxes.push_back (more);
            if (! mix.items.empty()) groupBoxes.push_back (mix);
        }

        void layoutGroups (juce::Rectangle<int> r, int from, int to)
        {
            // weight each group by its item count (or column count) and size; big groups get bigger knobs
            auto unitsOf = [] (const GroupBox& gb) { return (gb.cols > 0 ? gb.cols : (int) gb.items.size()) * (gb.big ? 3 : 2) + 1; };
            int totalUnits = 0;
            for (int i = from; i < to; ++i) totalUnits += unitsOf (groupBoxes[(size_t) i]);
            int x = r.getX();
            const int gap = 8;
            int remaining = r.getWidth() - gap * (to - from - 1);
            for (int i = from; i < to; ++i)
            {
                auto& gb = groupBoxes[(size_t) i];
                int units = unitsOf (gb);
                int w = i + 1 == to ? remaining : juce::jmax (90, remaining * units / juce::jmax (1, totalUnits));
                remaining -= w; totalUnits -= units;
                gb.bounds = juce::Rectangle<int> (x, r.getY(), w, r.getHeight());
                auto inner = gb.bounds.reduced (6, 4).withTrimmedTop (14);
                int n = (int) gb.items.size();
                int cols = gb.cols > 0 ? gb.cols : juce::jmax (1, juce::jmin (n, inner.getWidth() / (gb.big ? 96 : 74)));
                int rows = (n + cols - 1) / cols;
                int cw = inner.getWidth() / cols, ch = inner.getHeight() / rows;
                for (int k = 0; k < n; ++k)
                {
                    auto cell = juce::Rectangle<int> (inner.getX() + (k % cols) * cw, inner.getY() + (k / cols) * ch, cw, ch);
                    auto* c = gb.items[(size_t) k];
                    if (dynamic_cast<ChoiceBar*> (c) != nullptr)        c->setBounds (cell.withSizeKeepingCentre (juce::jmin (cw - 4, 220), 44));
                    else if (dynamic_cast<ToggleLamp*> (c) != nullptr)   c->setBounds (cell.withSizeKeepingCentre (juce::jmin (cw - 4, 80), 44));
                    else if (dynamic_cast<juce::TextButton*> (c) != nullptr) c->setBounds (cell.withSizeKeepingCentre (juce::jmin (cw - 8, 120), 26));
                    else
                    {
                        int side = juce::jmin (cw - 6, ch - 4, gb.big ? 118 : 86);
                        c->setBounds (cell.withSizeKeepingCentre (side, juce::jmin (ch - 2, side + 26)));
                    }
                }
                x += w + gap;
            }
        }

        //==========================================================================
        // 4OSC layout: OSC 1-4 | FILTER + ENV | AMP + LFO | FX + MASTER
        void buildSynthControls()
        {
            auto grp = [&] (const juce::String& title, juce::StringArray names, bool big = false)
            {
                GroupBox gb { title, {}, {}, big };
                for (auto& n : names) { auto it = handles.find (n); if (it != handles.end()) gb.items.push_back (makeControl (*it->second, big)); }
                groupBoxes.push_back (gb);
            };
            for (int i = 1; i <= 4; ++i)
            {
                auto s = juce::String (i);
                grp ("OSC " + s, { "Wave " + s, "tune" + s, "fineTune" + s, "level" + s, "pulseWidth" + s, "Unison " + s, "detune" + s, "spread" + s, "pan" + s });
            }
            grp ("FILTER", { "Filter Type", "Slope", "filterFreq", "filterResonance", "filterAmount", "filterKey", "filterVelocity" });
            grp ("FILTER ENVELOPE", { "filterAttack", "filterDecay", "filterSustain", "filterRelease" });
            grp ("AMP ENVELOPE", { "ampAttack", "ampDecay", "ampSustain", "ampRelease", "ampVelocity" });
            grp ("LFO 1 / LFO 2", { "LFO Wave 1", "lfoRate1", "lfoDepth1", "LFO Wave 2", "lfoRate2", "lfoDepth2" });
            grp ("MOD ENVELOPES", { "modAttack1", "modDecay1", "modSustain1", "modRelease1", "modAttack2", "modDecay2", "modSustain2", "modRelease2" });
            grp ("FX", { "DISTORTION", "distortion", "REVERB", "reverbSize", "reverbDamping", "reverbWidth", "reverbMix", "DELAY", "delayFeedback", "delayCrossfeed", "delayMix", "CHORUS", "chorusSpeed", "chorusDepth", "chorusWidth", "chorusMix" });
            grp ("VOICE & MASTER", { "Voice Mode", "legato", "masterLevel" });
        }

        void layoutSynth (juce::Rectangle<int> r)
        {
            vizArea = r.removeFromTop (130);
            if (viz) viz->setBounds (vizArea);
            r.removeFromTop (6);
            // two rows: row 1 = the four oscillators; row 2 = everything else
            auto row1 = r.removeFromTop (r.getHeight() * 46 / 100);
            r.removeFromTop (6);
            auto place = [&] (GroupBox& gb, juce::Rectangle<int> b)
            {
                gb.bounds = b;
                auto inner = b.reduced (5, 3).withTrimmedTop (13);
                int n = (int) gb.items.size();
                int cols = juce::jmax (1, juce::jmin (n, inner.getWidth() / 54));
                int rows = juce::jmax (1, (n + cols - 1) / cols);
                int cw = inner.getWidth() / cols, ch = inner.getHeight() / rows;
                for (int k = 0; k < n; ++k)
                {
                    auto cell = juce::Rectangle<int> (inner.getX() + (k % cols) * cw, inner.getY() + (k / cols) * ch, cw, ch);
                    auto* c = gb.items[(size_t) k];
                    if (dynamic_cast<ChoiceBar*> (c) || dynamic_cast<ToggleLamp*> (c)) c->setBounds (cell.withSizeKeepingCentre (cw - 2, juce::jmin (ch, 40)));
                    else c->setBounds (cell.withSizeKeepingCentre (juce::jmin (cw - 2, 56), juce::jmin (ch, 72)));
                }
            };
            int x = row1.getX(); int w = (row1.getWidth() - 18) / 4;
            for (int i = 0; i < 4 && i < (int) groupBoxes.size(); ++i) { place (groupBoxes[(size_t) i], juce::Rectangle<int> (x, row1.getY(), w, row1.getHeight())); x += w + 6; }
            const int weights[] = { 7, 4, 5, 6, 8, 12, 3 };
            int total = 0; for (int wgt : weights) total += wgt;
            x = r.getX(); int avail = r.getWidth() - 6 * 6;
            for (size_t i = 4; i < groupBoxes.size() && i - 4 < 7; ++i)
            {
                int gw = avail * weights[i - 4] / total;
                place (groupBoxes[i], juce::Rectangle<int> (x, r.getY(), gw, r.getHeight()));
                x += gw + 6;
            }
        }

        //==========================================================================
        void buildSamplerControls()
        {
            zoneModel = std::make_unique<ZoneModel> (*this);
            zoneList.setModel (zoneModel.get());
            zoneList.setRowHeight (22);
            zoneList.setColour (juce::ListBox::backgroundColourId, Theme::panel().withAlpha (0.6f));
            addAndMakeVisible (zoneList);
            addSampleBtn.onClick = [this] { chooseSample(); };
            removeSampleBtn.onClick = [this] { if (auto sp = dynamic_cast<te::SamplerPlugin*> (&plugin)) { auto row = zoneList.getSelectedRow(); if (row >= 0) { sp->removeSound (row); zoneList.updateContent(); } } };
            addAndMakeVisible (addSampleBtn); addAndMakeVisible (removeSampleBtn);
        }

        void layoutSampler (juce::Rectangle<int> r)
        {
            vizArea = r.removeFromTop (150);
            if (viz) viz->setBounds (vizArea);
            r.removeFromTop (8);
            auto btns = r.removeFromBottom (26);
            addSampleBtn.setBounds (btns.removeFromLeft (140)); btns.removeFromLeft (6); removeSampleBtn.setBounds (btns.removeFromLeft (90));
            r.removeFromBottom (6);
            zoneList.setBounds (r);
        }

        void chooseSample()
        {
            auto fc = std::make_shared<juce::FileChooser> ("Add a sample", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [this, fc] (const juce::FileChooser& c)
            {
                if (auto sp = dynamic_cast<te::SamplerPlugin*> (&plugin))
                    for (auto& f : c.getResults()) addSampleFile (*sp, f);
            });
        }

        void addSampleFile (te::SamplerPlugin& sp, const juce::File& f)
        {
            if (! f.existsAsFile()) return;
            te::AudioFile af (plugin.edit.engine, f);
            auto err = sp.addSound (f.getFullPathName(), f.getFileNameWithoutExtension(), 0.0, af.getLength() > 0 ? af.getLength() : 0.0, 0.0f);
            auto idx = sp.getNumSounds() - 1;
            if (err.isEmpty() && idx >= 0) sp.setSoundParams (idx, 60, 0, 127);
            zoneList.updateContent();
        }

        void editZone (int row)
        {
            auto sp = dynamic_cast<te::SamplerPlugin*> (&plugin);
            if (sp == nullptr || row < 0 || row >= sp->getNumSounds()) return;
            auto w = std::make_shared<juce::AlertWindow> ("Zone: " + sp->getSoundName (row), "Root note, lowest and highest note (MIDI numbers, 60 = C3)", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("root", juce::String (sp->getKeyNote (row)), "Root");
            w->addTextEditor ("min", juce::String (sp->getMinKey (row)), "Low");
            w->addTextEditor ("max", juce::String (sp->getMaxKey (row)), "High");
            w->addButton ("OK", 1); w->addButton ("Cancel", 0);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, sp, row] (int r)
            {
                if (r == 1 && row < sp->getNumSounds())
                    sp->setSoundParams (row, juce::jlimit (0, 127, w->getTextEditorContents ("root").getIntValue()), juce::jlimit (0, 127, w->getTextEditorContents ("min").getIntValue()), juce::jlimit (0, 127, w->getTextEditorContents ("max").getIntValue()));
                zoneList.updateContent(); zoneList.repaint();
            }), false);
        }

        //==========================================================================
        void chooseIR()
        {
            auto fc = std::make_shared<juce::FileChooser> ("Load an impulse response", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac");
            fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, fc] (const juce::FileChooser& c)
            {
                if (c.getResult().existsAsFile()) loadIR (c.getResult());
            });
        }

        void loadIR (const juce::File& f)
        {
            if (auto ir = dynamic_cast<te::ImpulseResponsePlugin*> (&plugin))
                if (ir->loadImpulseResponse (f))
                {
                    plugin.state.setProperty ("sqmIRPath", f.getFullPathName(), &plugin.edit.getUndoManager());
                    ir->name = f.getFileNameWithoutExtension();
                    if (viz) viz->repaint();
                }
        }

        //==========================================================================
        void buildHeader()
        {
            rebuildPresetBox();
            presetBox.setTextWhenNothingSelected ("Presets...");
            presetBox.onChange = [this] { presetChosen (presetBox.getSelectedId()); };
            addAndMakeVisible (presetBox);

            bypassBtn.setClickingTogglesState (true);
            bypassBtn.setToggleState (! plugin.isEnabled(), juce::dontSendNotification);
            if (auto mbp = dynamic_cast<SqApexMultibandPlugin*> (&plugin))   // dim the bands that aren't in use
                for (size_t gi = 1; gi < groupBoxes.size(); ++gi)
                {
                    const bool on = (int) gi <= mbp->numBands.get();
                    for (auto* c : groupBoxes[gi].items) if (c->isEnabled() != on) { c->setEnabled (on); c->setAlpha (on ? 1.0f : 0.3f); }
                }
            bypassBtn.setColour (juce::TextButton::buttonOnColourId, Theme::amber().darker (0.3f));
            bypassBtn.setTooltip ("Bypass this plugin");
            bypassBtn.onClick = [this] { plugin.setEnabled (! bypassBtn.getToggleState()); };
            addAndMakeVisible (bypassBtn);

            abBtn.setTooltip ("A/B compare: the first click stores the current settings as A and switches to a copy (B) you can tweak; click again to flip between them");
            abBtn.onClick = [this] { toggleAB(); };
            addAndMakeVisible (abBtn);
        }

        void rebuildPresetBox()
        {
            presetBox.clear (juce::dontSendNotification);
            int id = 1;
            if (sig != nullptr && ! sig->presets.empty())
            {
                bool categorised = false;
                for (auto& p : sig->presets) if (juce::String (p.name).contains (" | ")) { categorised = true; break; }
                if (categorised)
                {
                    // "Category | Name": one section per category, in order of first appearance
                    auto catOf = [] (const char* nm) { const juce::String n (nm); return n.contains (" | ") ? n.upToFirstOccurrenceOf (" | ", false, false).trim() : juce::String ("Classic"); };
                    auto nameOf = [] (const char* nm) { const juce::String n (nm); return n.contains (" | ") ? n.fromFirstOccurrenceOf (" | ", false, false) : n; };
                    juce::StringArray cats;
                    for (auto& p : sig->presets) if (! cats.contains (catOf (p.name))) cats.add (catOf (p.name));
                    for (auto& c : cats)
                    {
                        presetBox.addSectionHeading (c.toUpperCase());
                        id = 1;
                        for (auto& p : sig->presets)
                        {
                            if (catOf (p.name) == c) presetBox.addItem (nameOf (p.name) + (p.advanced ? "  *" : ""), id);
                            ++id;
                        }
                    }
                }
                else
                {
                    presetBox.addSectionHeading ("BASIC");
                    for (auto& p : sig->presets) if (! p.advanced) presetBox.addItem (p.name, id++); else ++id;
                    id = 1;
                    presetBox.addSectionHeading ("ADVANCED");
                    for (auto& p : sig->presets) if (p.advanced) presetBox.addItem (p.name, id++); else ++id;
                }
            }
            userPresets.clear();
            auto dir = presetsRoot().getChildFile (name);
            if (dir.isDirectory())
            {
                presetBox.addSectionHeading ("YOUR PRESETS");
                for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.sqmpreset"))
                {
                    userPresets.push_back (f);
                    presetBox.addItem (f.getFileNameWithoutExtension(), 1000 + (int) userPresets.size());
                }
            }
            presetBox.addSeparator();
            presetBox.addItem ("Save current settings as preset...", 9000);
            presetBox.addItem ("Reset to default", 9001);
        }

        void presetChosen (int id)
        {
            if (id == 0) return;
            if (id == 9000) { savePreset(); presetBox.setSelectedId (0, juce::dontSendNotification); return; }
            if (id == 9001)
            {
                for (auto& h : handleStore) h->set (h->defaultNorm);
                presetBox.setSelectedId (0, juce::dontSendNotification);
                repaintControls();
                return;
            }
            if (id >= 1000)
            {
                auto idx = id - 1001;
                if (idx >= 0 && idx < (int) userPresets.size())
                    if (auto xml = juce::XmlDocument::parse (userPresets[(size_t) idx]))
                    {
                        auto v = juce::ValueTree::fromXml (*xml);
                        if (v.isValid()) plugin.restorePluginStateFromValueTree (v);
                    }
                repaintControls();
                return;
            }
            if (sig != nullptr && id >= 1 && id <= (int) sig->presets.size())
            {
                Signature::applyPreset (plugin, sig->presets[(size_t) (id - 1)]);
                repaintControls();
            }
        }

        void savePreset()
        {
            auto dir = presetsRoot().getChildFile (name);
            dir.createDirectory();
            auto w = std::make_shared<juce::AlertWindow> ("Save preset", "Preset name", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("n", "My " + name + " preset");
            w->addButton ("Save", 1); w->addButton ("Cancel", 0);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, dir] (int r)
            {
                if (r != 1) return;
                plugin.edit.flushPluginStateIfNeeded (plugin);
                auto f = dir.getChildFile (juce::File::createLegalFileName (w->getTextEditorContents ("n")) + ".sqmpreset");
                if (auto xml = plugin.state.createXml()) xml->writeTo (f);
                rebuildPresetBox();
            }), false);
        }

        void toggleAB()
        {
            plugin.edit.flushPluginStateIfNeeded (plugin);
            if (! abState.isValid()) { abState = plugin.state.createCopy(); abIsB = true; abBtn.setButtonText ("B"); return; }
            auto current = plugin.state.createCopy();
            plugin.restorePluginStateFromValueTree (abState);
            abState = current;
            abIsB = ! abIsB;
            abBtn.setButtonText (abIsB ? "B" : "A");
            repaintControls();
        }

        void repaintControls()
        {
            for (auto& c : controls) c->repaint();
            if (viz) viz->repaint();
        }

        void timerCallback() override
        {
            if (viz) { viz->phase += 0.05f; viz->repaint(); }
            for (auto& c : controls) c->repaint();
            bypassBtn.setToggleState (! plugin.isEnabled(), juce::dontSendNotification);
            if (spec.family == Family::sampler && zoneList.getModel() != nullptr && zoneList.getNumRowsOnScreen() >= 0) zoneList.repaint();
        }
    };
}
